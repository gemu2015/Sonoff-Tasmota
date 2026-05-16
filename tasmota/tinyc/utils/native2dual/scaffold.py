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
        # Strip C++ in-class member initializers (`float t = NAN;`,
        # `uint8_t v = 0;`). The plugin build disallows them and they
        # never run anyway — MODULE_MEMORY is raw jcalloc'd heap (no
        # ctor). Mirrors the hand-dual slimming. Only the `{...}` body
        # is touched; simple-expr inits ( = X before ; or , ).
        def _strip_inits(sd):
            i, j = sd.index('{'), sd.rindex('}')
            inner = re.sub(r'\s*=\s*[^;,{}]+(?=[;,])', '', sd[i+1:j])
            return sd[:i+1] + inner + sd[j:]
        tdef = _strip_inits(tdef)
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
            # PLUGIN RULE 3: a file-scope const array lands in the
            # Xtensa literal pool, which the BinPlugin loader does NOT
            # relocate → must be PROGMEM and read via pgm_read_*.
            # PROGMEM is a no-op section attr in native mode and
            # pgm_read works on plain memory too, so this stays
            # dual-safe (compiles+runs both as native and as plugin).
            base = ty.strip().split()[-1]
            rd = {'uint8_t':'pgm_read_byte','int8_t':'pgm_read_byte',
                  'char':'pgm_read_byte','bool':'pgm_read_byte',
                  'uint16_t':'pgm_read_word','int16_t':'pgm_read_word',
                  'uint32_t':'pgm_read_dword','int32_t':'pgm_read_dword',
                  'int':'pgm_read_dword','size_t':'pgm_read_dword',
                  'float':'pgm_read_float'}.get(base)
            decl = f'const {ty}{nm}{arr} PROGMEM {init or ""};'
            body = body.replace(full, decl, 1)
            if rd:
                # rewrite element reads nm[expr] -> rd(&nm[expr]); the
                # PROGMEM decl uses empty `nm[]` so it is never matched.
                body = re.sub(
                    r'\b' + re.escape(nm) + r'\s*\[\s*([^\]\n]+?)\s*\]',
                    lambda mm: f'{rd}(&{nm}[{mm.group(1)}])', body)
            else:
                body = ('// NEEDS-MANUAL: PROGMEM array %s (elem type %s) '
                        '- wrap element reads with the matching '
                        'pgm_read_*\n' % (nm, base)) + body

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

    # --- per-function prologue pass (brace-matched, string/comment-safe).
    #     ALLOCMEM in the INIT path, SETMEMREGS elsewhere; + STGLOB when
    #     the body uses TasmotaGlobal (declares `tgbl`); + the INIT fn is
    #     made int32_t-returning because ALLOCMEM's OOM guard `return -1`
    #     is invalid in a void fn. Mirrors the hand duals exactly. -----
    init_fn = None
    im = re.search(r'FUNC_INIT[^\n]*\n\s*([A-Z]\w+)\s*\(', s)
    if im: init_fn = im.group(1)

    def mask(t):                       # length-preserving str/comment blank
        o = list(t); i = 0; n = len(t); st = None
        while i < n:
            c = t[i]; d = t[i+1] if i+1 < n else ''
            if st is None:
                if c == '/' and d == '/': st='//'; o[i]=o[i+1]=' '; i+=2; continue
                if c == '/' and d == '*': st='/*'; o[i]=o[i+1]=' '; i+=2; continue
                if c == '"': st='"'; i+=1; continue
                if c == "'": st="'"; i+=1; continue
                i += 1
            elif st == '//':
                if c == '\n': st=None
                else: o[i]=' '
                i += 1
            elif st == '/*':
                if c=='*' and d=='/': o[i]=o[i+1]=' '; st=None; i+=2; continue
                if c != '\n': o[i]=' '
                i += 1
            else:                                   # in " or '
                if c == '\\': i += 2; continue
                if c == st: st=None
                elif c != '\n': o[i]=' '
                i += 1
        return ''.join(o)

    msk = mask(body)
    SIG = re.compile(
        r'^((?:bool|void|int32_t|uint8_t|int|float|const char\s*\*)\s+'
        r'([A-Za-z_]\w*)\s*\([^;{]*\))\s*\{', re.M)
    funcs = []
    for m in SIG.finditer(msk):
        nm = m.group(2)
        if re.match(r'(Xsns|Xdrv)\d+$', nm):        # dispatcher: skip
            continue
        o = msk.index('{', m.end()-1)
        depth = 0
        for j in range(o, len(msk)):
            if msk[j] == '{': depth += 1
            elif msk[j] == '}':
                depth -= 1
                if depth == 0: break
        funcs.append((m.start(), o, j, nm))
    for s0, o, c, nm in sorted(funcs, key=lambda f: -f[0]):
        sig  = body[s0:o].rstrip()
        is_init = (mem_fields and nm == init_fn)
        if is_init and re.match(r'\s*void\b', sig):
            sig = re.sub(r'\bvoid\b', 'int32_t', sig, count=1)
        tok = 'ALLOCMEM' if is_init else ('SETMEMREGS' if mem_fields
                                          else 'SETREGS')
        if 'TasmotaGlobal' in msk[o:c]:
            tok += ' STGLOB'
        inner = body[o+1:c]
        if is_init:
            inner = re.sub(r'\breturn\s*;', 'return 0;', inner)
            inner = inner + '\n  return 0;\n'
        body = body[:s0] + sig + ' {\n  ' + tok + inner + body[c:]

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
// native2dual: route native dual-bus I2C onto the append-only JMPTBL
// slots 216/217/218. FILE-LOCAL only — module_defines.h's global
// `#define I2cWrite8 jI2cWrite8` (jt[45], 3-arg) is undef'd here just
// for this scaffolded TU; no other plugin source is affected, so this
// changes the behaviour of NOTHING that already exists.
#  undef  I2cWrite8
#  define I2cWrite8(a,r,v,b)       jI2cWrite8Bus((a),(r),(v),(b))
#  undef  I2cWrite0
#  define I2cWrite0(a,r,b)         jI2cWrite0((a),(r),(b))
#  undef  I2cReadBuffer0
#  define I2cReadBuffer0(a,bf,l,b) jI2cReadBuffer0((a),(bf),(l),(b))
// module_defines.h has `#define TasmotaGlobal *tgbl`, so native
// `TasmotaGlobal.member` mis-parses as `*(tgbl.member)`. Re-point to
// `(*tgbl)` so `.member` works with the STGLOB-declared `tgbl`.
// File-local; native build & other plugins untouched.
#  undef  TasmotaGlobal
#  define TasmotaGlobal (*tgbl)
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
    # pFUNC_DEINIT must release any I2C address the driver claimed via
    # I2cSetActiveFound (else reload / other drivers can't re-claim the
    # bus — the BinPlugin slot-churn trap). Native register-bang
    # drivers rarely have a deinit; the hand duals add one. Synthesize
    # it from the claim call site: the common `arr[cnt].field` idiom →
    # a release loop; anything else → a single release (best-effort)
    # or an honest NEEDS-MANUAL flag. I2cResetActive(addr,bus) is
    # already in the JMPTBL (jt[30]) — no new slot needed.
    rel = ''
    sm = re.search(r'I2cSetActiveFound\s*\(\s*([^,]+?)\s*,[^,]+,'
                   r'\s*([^)]+?)\s*\)', s)
    if sm:
        ae, be = sm.group(1).strip(), sm.group(2).strip()
        ix = r'^([A-Za-z_]\w*)\s*\[\s*([^\]]+?)\s*\]\s*\.\s*(\w+)$'
        am, bm = re.match(ix, ae), re.match(ix, be)
        if am and bm and am.group(1) == bm.group(1) \
                and am.group(2) == bm.group(2):
            arr, cnt = am.group(1), am.group(2)
            rel = (f'for (uint32_t _di = 0; _di < {cnt}; _di++) '
                   f'I2cResetActive({arr}[_di].{am.group(3)}, '
                   f'{arr}[_di].{bm.group(3)});')
        elif re.search(r'[\[\.]', ae):       # stateful but not the idiom
            rel = ('/* NEEDS-MANUAL: release each I2C device claimed via '
                   'I2cSetActiveFound — I2cResetActive(addr,bus) */')
        else:                                # single fixed address
            rel = f'I2cResetActive({ae}, {be});'
    deinit = ('    case pFUNC_DEINIT: { SETMEMREGS '
              + (rel + ' ' if rel else '')
              + 'RETMEM break; }'
              + ('  // deregister I2C, then free MODULE_MEMORY'
                 if rel else ''))

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
    DISP += [deinit,
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
    # GTBL is a CURATED subset of TasmotaGlobal exposed to plugins; it
    # is offset-sensitive, NOT a safe jt-style append. Member accesses
    # the plugin GTBL doesn't carry must be guarded/dropped by a human
    # (typically optional branches like the #if MAX_I2C>1 dual-bus
    # display). Flag, never auto-edit (would be a semantic change).
    GTBL_ABSENT = ['i2c_enabled', 'spi_enabled2', 'global_state']
    gtbl_flags = sorted({m for m in GTBL_ABSENT
                         if re.search(r'TasmotaGlobal\s*[.>-]+\s*'
                                      + re.escape(m), body)})
    FLAGS = []
    if flagged:
        FLAGS = ['// ' + '='*64,
                 f'// DUAL-BUS I2C ({len(flagged)}) — RESOLVED via append-only '
                 'JMPTBL slots',
                 '// jt[216..218] (tmod_I2cWrite8Bus/I2cWrite0/I2cReadBuffer0).',
                 '// This scaffold injects a FILE-LOCAL remap above; the frozen',
                 '// jI2cWrite8 (jt[45], 3-arg) is left byte-identical, so no',
                 '// existing plugin .bin behaviour changes. Informational:']
        for s_ in flagged:
            FLAGS.append(f'//   routed: {s_:16s} — was: {JMPTBL_GAP[s_]}')
        FLAGS.append('// ' + '='*64)
    if gtbl_flags:
        FLAGS += ['// ' + '='*64,
                  f'// NEEDS-ABI ({len(gtbl_flags)}) — plugin GTBL (curated '
                  'TasmotaGlobal subset)',
                  '// does not carry these members. GTBL is offset-sensitive,',
                  '// NOT a safe jt-style append — a human must guard/drop the',
                  '// (usually optional) branch that reads them, e.g. the',
                  '// `#if MAX_I2C > 1` dual-bus display path.']
        for g_ in gtbl_flags:
            FLAGS.append(f'//   NEEDS-ABI: TasmotaGlobal.{g_} '
                         '— absent from plugin GTBL; guard/drop that branch')
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
