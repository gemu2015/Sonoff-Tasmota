#!/usr/bin/env python3
# =====================================================================
# scaffold.py — PoC mechanical native->dual-format-plugin scaffolder.
#
# Faithful: keeps the driver body verbatim (no feature drop). Applies
# ONLY the mechanical wrapping the dual-format/BinPlugin ABI needs:
#   - gating preamble + dual_format_compat.h
#   - file-scope mutable state -> MODULE_MEMORY + accessor #defines
#   - read-only file-scope arrays -> const (RULE 2)
#   - MODULE_DESCRIPTOR / MODULE_PART block from parsed signatures
#   - Xsns##() dispatcher -> mod_func_execute() (plugin) / kept (native)
#   - reg-bind prologue (ALLOCMEM in INIT path, SETMEMREGS elsewhere)
#
# Everything it cannot resolve mechanically is left for the compiler to
# reject — the point of the PoC experiment is to harvest that error set
# as the empirical NEEDS-* flag taxonomy.
#
# Usage: scaffold.py <native.ino> <NAME> > slot.cpp
# =====================================================================
import re, sys, os

def main():
    src_path, NAME = sys.argv[1], sys.argv[2]
    s = open(src_path, encoding='utf-8', errors='replace').read()
    U = NAME.upper()
    xsns_m = re.search(r'\bbool\s+(Xsns|Xdrv)(\d+)\s*\(\s*uint(?:8|32)_t\s+\w+\s*\)',
                       s)
    disp, xnum = xsns_m.group(1), xsns_m.group(2)

    # --- strip the original outer #ifdef USE_*/license, keep body ----
    body = s
    body = re.sub(r'^\s*/\*.*?\*/\s*', '', body, count=1, flags=re.S)  # license
    body = re.sub(r'#ifdef\s+USE_I2C\s*\n', '', body, count=1)
    body = re.sub(r'#ifdef\s+USE_\w+\s*\n', '', body, count=1)
    body = re.sub(r'#endif\s*//\s*USE_\w+\s*\n?', '', body)
    body = re.sub(r'#endif\s*//\s*USE_I2C\s*\n?', '', body)

    # --- state-wrap: file-scope mutable vars/instances -> MODULE_MEMORY,
    #     read-only arrays -> const (RULE 2). Struct *type* defs stay at
    #     file scope; only the instance moves into MODULE_MEMORY. -------
    NM = NAME.lower()
    mem_fields, accessors, type_defs = [], [], []
    # Hoist object-like #defines, enums and PROGMEM const tables ABOVE
    # the state block — MODULE_MEMORY and the accessors reference these
    # (e.g. sht3x_sensors[SHT3X_ADDRESSES]); they must be defined first.
    hoist = []
    for m in list(re.finditer(r'^[ \t]*#define[ \t]+[A-Za-z_]\w*[ \t]+[^\n]+$',
                              body, re.M)):
        hoist.append(m.group(0));
    for m in list(re.finditer(r'^[ \t]*enum\s+\w+\s*\{[^}]*\}\s*;', body, re.M)):
        hoist.append(m.group(0))
    for m in list(re.finditer(
            r'^[ \t]*const\s+[\w \t*]+\b\w+\s*\[\s*\]\s*PROGMEM\s*=[^;]+;',
            body, re.M)):
        hoist.append(m.group(0))
    for h in hoist:
        body = body.replace(h, '', 1)

    def is_written(nm):
        # True only if assigned/mutated SOMEWHERE OTHER THAN its own
        # declaration line (a decl `T x[] = {...};` is not a "write").
        for mm in re.finditer(r'\b' + re.escape(nm) +
                  r'\s*(\[[^\]]*\])?\s*(=[^=]|\+\+|--|[-+*/|&^]=)', body):
            ln_start = body.rfind('\n', 0, mm.start()) + 1
            line = body[ln_start:body.find('\n', mm.start())]
            if re.match(r'\s*(static\s+)?(const\s+)?'
                        r'(uint\d+_t|int\d*_t|int|float|double|bool|char|'
                        r'size_t|struct\s+\w+)\b[^;]*\b' + re.escape(nm),
                        line):
                continue                     # this match is the declaration
            return True
        return False
    # struct Tag { ... } inst[..];  -> split type vs instance
    for m in list(re.finditer(
            r'^(struct\s+(\w+)\s*\{[^}]*\})\s*([A-Za-z_]\w*)\s*'
            r'(\[[^\]]*\])?\s*;', body, re.S | re.M)):
        tdef, tag, inst, arr = m.groups()
        type_defs.append(tdef + ';')
        mem_fields.append(f'{tag} {inst}{arr or ""};')
        accessors.append(f'#define {inst} mem->{inst}')
        body = body.replace(m.group(0), '', 1)
    # plain scalar/array file-scope vars
    for m in list(re.finditer(
            r'^(?:static\s+)?((?:uint\d+_t|int\d*_t|int|float|double|bool|'
            r'char|uint8_t|size_t)\s+)([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*'
            r'(=\s*[^;]+)?;', body, re.M)):
        full, ty, nm, arr, init = m.group(0), m.group(1), m.group(2), \
                                  m.group(3), m.group(4)
        if is_written(nm):
            mem_fields.append(f'{ty}{nm}{arr or ""};')
            accessors.append(f'#define {nm} mem->{nm}')
            body = body.replace(full, '', 1)
        elif arr and 'static' not in full:                  # RO array
            body = body.replace(full, 'const ' + full, 1)

    STATE = []
    if mem_fields:
        STATE = type_defs + [
            f'#define DUAL_NATIVE_NAME    {NM}',
            f'#define DUAL_NATIVE_STATE_T {NM}_n2d_state_t',
            '#include "dual_format_native_state.h"',
            'typedef struct {'] + ['  ' + f for f in mem_fields] + \
            ['} MODULE_MEMORY;'] + accessors
    else:
        STATE = type_defs

    # --- inject reg-bind prologue into every top-level function body --
    # ALLOCMEM in the INIT path (allocates MODULE_MEMORY), SETMEMREGS
    # elsewhere (binds mem + mt/jt). Mechanical, mirrors the hand duals.
    init_fn = None
    im = re.search(r'FUNC_INIT[^\n]*\n\s*([A-Z]\w+)\s*\(', s)
    if im: init_fn = im.group(1)
    def add_prologue(mm):
        head, name = mm.group(0), mm.group(2)
        tok = 'ALLOCMEM' if (mem_fields and name == init_fn) \
              else ('SETMEMREGS' if mem_fields else 'SETREGS')
        return head + f'\n  {tok}'
    body = re.sub(
        r'^((?:bool|void|int32_t|uint8_t|int|float|const char\s*\*)\s+'
        r'([A-Za-z_]\w*)\s*\([^;{]*\))\s*\{',
        lambda mm: mm.group(1) + ' {\n  ' +
        ('ALLOCMEM' if (mem_fields and mm.group(2) == init_fn)
         else ('SETMEMREGS' if mem_fields else 'SETREGS')),
        body, flags=re.M)

    # --- collect top-level function signatures for MODULE_PART --------
    sigs = re.findall(
        r'^\s*((?:bool|void|int32_t|uint8_t|int|float|const char\s*\*)\s+'
        r'[A-Za-z_]\w*\s*\([^;{]*\))\s*\{', body, re.M)
    sigs = [re.sub(r'\s+', ' ', g).strip() for g in sigs
            if not re.match(r'(bool\s+)?(Xsns|Xdrv)\d+', g)]

    # --- the original dispatcher body (FUNC_ switch) -----------------
    dispm = re.search(r'bool\s+'+disp+xnum+r'\s*\(\s*uint(?:8|32)_t\s+\w+\s*\)\s*\{',
                      body)
    disp_start = dispm.start()
    disp_body = body[disp_start:]
    body = body[:disp_start]                      # functions above dispatcher

    PRE = f'''#include "tasmota_options.h"
#ifndef BUILD_AS_PLUGIN
#  ifdef USE_{U}_N2D_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif
#include "dual_format_compat.h"
#if BUILD_AS_PLUGIN
#  ifdef USE_{U}_N2D_MOD
#    define _{U}_N2D_ENABLED 1
#  endif
#endif
#ifdef _{U}_N2D_ENABLED
// ===================================================================
// native2dual PoC scaffold of {os.path.basename(src_path)} — FAITHFUL
// (no feature drop). Bodies verbatim; only mechanical wrapping added.
// Unresolved constructs are left for the compiler to reject — that
// error set IS the deliverable (empirical NEEDS-* taxonomy).
// ===================================================================
'''
    PART = ['PUSH_OPTIONS',
            f'MODULE_DESCRIPTOR("{U[:6]}", MODULE_TYPE_SENSOR, 1<<16|{xnum},'
            ' "",0,"",0,"",0,"",0)']
    for sg in sigs:
        PART.append(f'MODULE_PART {sg};')
    PART.append('#if BUILD_AS_PLUGIN')
    PART.append('MODULE_PART int32_t mod_func_execute(uint32_t function);')
    PART.append('#endif')
    PART.append('MODULE_END')

    # FUNC_-case -> pFUNC_ map from the original dispatcher
    fmap = {'FUNC_INIT':'pFUNC_INIT','FUNC_EVERY_SECOND':'pFUNC_EVERY_SECOND',
            'FUNC_JSON_APPEND':'pFUNC_JSON_APPEND','FUNC_WEB_SENSOR':'pFUNC_WEB_SENSOR'}
    DISP = ['#if BUILD_AS_PLUGIN',
            'int32_t mod_func_execute(uint32_t function) {',
            '  switch (function) {']
    for fn, pf in fmap.items():
        m = re.search(fn + r'\b[\s\S]*?\b([A-Z][A-Za-z0-9_]*)\s*\(', disp_body)
    # Pull the calls the original dispatcher makes per FUNC_ (heuristic)
    for fn, pf in fmap.items():
        seg = re.search(fn + r'\b(.*?)(?:break;|case |\}\s*\n)', disp_body, re.S)
        call = ''
        if seg:
            cm = re.search(r'\b([A-Z]\w+)\s*\(([^;]*)\)\s*;', seg.group(1))
            if cm: call = cm.group(0)
        if not call and fn == 'FUNC_INIT':
            im = re.search(r'FUNC_INIT[^\n]*\n\s*([A-Z]\w+\s*\([^;]*\);)',
                           disp_body)
            call = im.group(1) if im else ''
        DISP.append(f'    case {pf}: {{ {call} break; }}')
    DISP += ['    case pFUNC_DEINIT: { SETMEMREGS RETMEM break; }',
             '    default: break;',
             '  }', '  return 0;', '}', 'PULL_OPTIONS', '#endif',
             f'#endif  // _{U}_N2D_ENABLED']

    # --- honest NEEDS-JMPTBL report: symbols the frozen plugin ABI
    #     does not expose (or exposes with a drifted signature). These
    #     are the irreducible residue — a human/firmware must extend the
    #     JMPTBL; the scaffolder cannot invent ABI. (Empirically harvested
    #     from the sht3x build; extend as more drivers are run.) --------
    JMPTBL_GAP = {
        'I2cWrite8':     'frozen jI2cWrite8 is 3-arg; native is 4-arg '
                         '(addr,reg,val,bus) — dual-bus param added post-ABI',
        'I2cWrite0':     'not in plugin JMPTBL/compat (dual-bus reg write)',
        'I2cReadBuffer0':'not in plugin JMPTBL/compat (dual-bus buffer read)',
        'I2cRead8':      'verify JMPTBL arity vs native dual-bus form',
        'I2cRead16':     'verify JMPTBL arity vs native dual-bus form',
    }
    flagged = sorted({sym for sym in JMPTBL_GAP
                      if re.search(r'\b'+re.escape(sym)+r'\s*\(', body)})
    FLAGS = []
    if flagged:
        FLAGS = ['// ' + '='*64,
                 f'// NEEDS-JMPTBL ({len(flagged)}) — irreducible: the frozen '
                 'BinPlugin ABI does',
                 '// not expose these (or its signature drifted from native).',
                 '// A human/firmware must add/modernise the JMPTBL entry;',
                 '// the scaffolder cannot invent ABI. Until then this plugin',
                 '// will not link. (This is THE bottleneck for the lib-free',
                 '// register-bang driver class — shared across many drivers.)']
        for s_ in flagged:
            FLAGS.append(f'//   NEEDS-JMPTBL: {s_:16s} — {JMPTBL_GAP[s_]}')
        FLAGS.append('// ' + '='*64)

    print(PRE)
    if FLAGS:
        print('\n'.join(FLAGS)); print()
    print('\n'.join(PART))
    print()
    if hoist:
        print('// --- hoisted #defines / enums / PROGMEM tables '
              '(needed by MODULE_MEMORY + bodies) ---')
        print('\n'.join(hoist))
        print()
    if STATE:
        print('// --- mechanical state-wrap: file-scope mutable state ->')
        print('// MODULE_MEMORY (per-slot heap); read-only arrays -> const')
        print('\n'.join(STATE))
        print()
    print(body.rstrip())
    print()
    print('\n'.join(DISP))

if __name__ == '__main__':
    main()
