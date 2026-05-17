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

def _pool_consts(b):
    """Plugin literal-pool rule: the Xtensa literal pool is NOT
    relocated in plugin mode, so in executable code:
      * float literals  -> pooled `const float FP_CONST[] PROGMEM`,
        referenced via FLTC(idx)
      * int  literals |v|>2047 -> ICONST(v)
    FLTC/ICONST are no-ops in native, real reads in plugin (compat
    header / module_defines). Skip contexts that MUST stay compile-
    time constants: strings/comments, #-lines, `case … :` labels,
    `[ … ]` (array dim/subscript) and `= { … }` aggregate initializers.
    Returns (new_body, fp_const_decl_or_None). Edits applied right-to-
    left so positions stay valid."""
    n = len(b)
    m = list(b)                       # strings/comments -> spaces
    skip = bytearray(n)               # 1 = do not touch
    X = 1
    st = None; i = 0
    while i < n:
        c = b[i]; d = b[i+1] if i+1 < n else ''
        if st is None:
            if c == '/' and d == '/': st='//'; m[i]=m[i+1]=' '; skip[i]=skip[i+1]=X; i+=2; continue
            if c == '/' and d == '*': st='/*'; m[i]=m[i+1]=' '; skip[i]=skip[i+1]=X; i+=2; continue
            if c == '"': st='"'; skip[i]=X; i+=1; continue
            if c == "'": st="'"; skip[i]=X; i+=1; continue
            i += 1
        elif st == '//':
            if c == '\n': st=None
            else: m[i]=' '; skip[i]=X
            i += 1
        elif st == '/*':
            if c=='*' and d=='/': m[i]=m[i+1]=' '; skip[i]=skip[i+1]=X; st=None; i+=2; continue
            if c!='\n': m[i]=' '; skip[i]=X
            i += 1
        else:
            if c == '\\':
                skip[i]=X
                if i+1<n: skip[i+1]=X
                i += 2; continue
            if c == st: st=None
            elif c!='\n': m[i]=' '
            skip[i]=X; i += 1
    m = ''.join(m)
    def mark(a, z):
        for k in range(a, min(z, n)): skip[k] = X
    for mm in re.finditer(r'(?m)^[ \t]*#[^\n]*', m): mark(mm.start(), mm.end())
    for mm in re.finditer(r'\bcase\b[^:\n]*:', m):    mark(mm.start(), mm.end())
    i = 0
    while i < n:                                   # all [ … ] spans
        if m[i] == '[':
            d = 0; j = i
            while j < n:
                if m[j] == '[': d += 1
                elif m[j] == ']':
                    d -= 1
                    if d == 0: break
                j += 1
            mark(i, j+1); i = j+1; continue
        i += 1
    for mm in re.finditer(r'=\s*\{', m):           # = { … } initializers
        s0 = mm.end()-1; d = 0; j = s0
        while j < n:
            if m[j] == '{': d += 1
            elif m[j] == '}':
                d -= 1
                if d == 0: break
            j += 1
        mark(mm.start(), j+1)
    fp = []; fmap = {}; reps = []
    FLOAT = re.compile(r'(?<![\w.])(\d+\.\d*(?:[eE][+-]?\d+)?|'
                       r'\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)[fFlL]?'
                       r'(?![\w.])')
    for mo in FLOAT.finditer(m):
        s0, e0 = mo.start(), mo.end()
        if skip[s0]: continue
        key = b[s0:e0].rstrip('fFlL')
        try: float(key)
        except ValueError: continue
        if key not in fmap:
            fmap[key] = len(fp); fp.append(key)
        reps.append((s0, e0, f'FLTC({fmap[key]})'))
    INT = re.compile(r'(?<![\w.])(0[xX][0-9a-fA-F]+|\d+)[uUlL]*'
                     r'(?![\w.\d])')
    for mo in INT.finditer(m):
        s0, e0 = mo.start(), mo.end()
        if skip[s0]: continue
        try: v = int(mo.group(1), 0)
        except ValueError: continue
        if -2048 <= v <= 2047: continue
        reps.append((s0, e0, f'ICONST({b[s0:e0]})'))
    for s0, e0, rep in sorted(reps, key=lambda r: -r[0]):
        b = b[:s0] + rep + b[e0:]
    fpd = None
    if fp:
        vals = ', '.join((x if re.search(r'[.eE]', x) else x + '.0') + 'f'
                         for x in fp)
        fpd = ('// plugin literal-pool rule: float consts pooled in '
               'PROGMEM, read via FLTC(idx)\n'
               'const float FP_CONST[] PROGMEM = { %s };' % vals)
    return b, fpd

def _soft_float(b):
    """Plugin mode: compiler-emitted HW float ops (__divsf3/__floatsisf
    …) mis-relocate under the loader (tc2plugin Bug-3 class). Lower the
    SAFE, recognisable sensor idiom — a float-context binary op where
    one side is a pooled FLTC(idx) and the other a single identifier —
    to the soft-float helpers (fdiv/fmul/fadd/fdiff + tofloat on an
    integer operand), matching the hand dual. Anything more complex
    (chained float exprs, float-typed locals, transcendentals) is NOT
    mechanically lowerable here — that is tc2plugin's type-inference
    job — so it is honestly flagged NEEDS-SOFTFLOAT, never miscompiled.
    Returns (new_body, [flag_lines])."""
    intv = set(re.findall(
        r'\b(?:u?int(?:8|16|32|64)?_t|int|unsigned|short|long|size_t|'
        r'char|bool)\s+([A-Za-z_]\w*)', b))
    FN = {'/': 'fdiv', '*': 'fmul', '+': 'fadd', '-': 'fdiff'}
    def wrap(tok):
        tok = tok.strip()
        return f'tofloat({tok})' if tok in intv else tok
    # ident OP FLTC(i)  — but NOT `obj.member`/`ptr->member` OP FLTC:
    # matching the bare member name there corrupts the access into a
    # bogus `obj.fdiv(member, …)`. A leading `.`/`->` ⇒ leave it (it is
    # a member float-op — honest residue, never silently mangled).
    b = re.sub(r'(\.|->)?\s*\b([A-Za-z_]\w*)\s*([*/+\-])\s*FLTC\((\d+)\)',
               lambda m: m.group(0) if m.group(1) else
                         f'{FN[m.group(3)]}({wrap(m.group(2))}, '
                         f'FLTC({m.group(4)}))', b)
    # FLTC(i) OP ident
    b = re.sub(r'FLTC\((\d+)\)\s*([*/+\-])\s*(\.|->)?\s*\b([A-Za-z_]\w*)',
               lambda m: m.group(0) if m.group(3) else
                         f'{FN[m.group(2)]}(FLTC({m.group(1)}), '
                         f'{wrap(m.group(4))})', b)
    # Lower bare int→float local assignments:
    #   `float v = <integer-valued expr>;`  →  `float v = tofloat(<expr>);`
    # The loader mis-relocates the compiler's int→float conversion just
    # like HW float ops, so the int compensation result (Bosch BMP/BME
    # `float T = (t_fine*5+128)>>8;`, `float h = (v_x1_u32r>>12);`) reads
    # garbage without tofloat. Conservative: only when the RHS is
    # PROVABLY integer — contains ≥1 known int var and every identifier
    # is a known int var or the EXEC_OFFSET/ICONST int macros, no float
    # literal, not already a soft-float/FLTC/tofloat call. Anything else
    # is left for the honest NEEDS-SOFTFLOAT scan (never miscompiled).
    _INTMACRO = {'EXEC_OFFSET', 'ICONST'}
    def _intfloat(m):
        decl, var, rhs = m.group(1), m.group(2), m.group(3).strip()
        if re.search(r'\b(fdiv|fmul|fadd|fdiff|tofloat|FLTC|'
                     r'ConvertTemp|ConvertHumidity|NAN|nanf?)\b', rhs):
            return m.group(0)
        if re.search(r'(?:\d\.\d|\.\d\d*|\d\.|\d[eE][-+]?\d|\d[fF]\b)',
                     rhs):                         # float literal-ish
            return m.group(0)
        ids = set(re.findall(r'\b([A-Za-z_]\w*)\s*(?!\s*\()', rhs))
        intids = ids & intv
        if not intids:                              # no runtime int var
            return m.group(0)                       # (pure const) — skip
        if not (ids <= (intv | _INTMACRO)):         # an unknown/float id
            return m.group(0)                       # honest flag instead
        return f'{decl}{var} = tofloat({rhs});'
    b = re.sub(r'\b((?:static\s+)?(?:const\s+)?float\s+)(\w+)\s*=\s*'
               r'([^;]+);', _intfloat, b)
    flags = []
    # Honest residual scan: a `float` local still produced by bare HW
    # arithmetic (not via fXxx/FLTC) — flag the line, do not touch it.
    for ln in b.splitlines():
        s = ln.strip()
        if re.match(r'(static\s+)?(const\s+)?float\s+\w', s) and \
           re.search(r'[^|&=!<>]=[^=]', s) and \
           not re.search(r'\b(fdiv|fmul|fadd|fdiff|tofloat|FLTC|'
                         r'ConvertTemp|ConvertHumidity)\b', s) and \
           re.search(r'[-+*/]', s.split('=', 1)[1] if '=' in s else ''):
            flags.append('NEEDS-SOFTFLOAT: ' + s[:90])
    return b, sorted(set(flags))

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
    # Strip ONLY the two OUTERMOST wrapper closers (matching the two
    # leading #ifdefs removed above) — the LAST `#endif // USE_I2C`
    # then the LAST remaining `#endif // USE_<driver>`. A global
    # `re.sub` here also deletes INNER feature guards like
    # `#endif // USE_BME68X` that close an `#ifdef USE_BME68X` inside a
    # switch, leaving the #ifdef unbalanced → preprocessor corruption
    # (hit by xsns_09_bmp; SHT3X has no inner USE_ guard so it slipped
    # through). Remove last-match only, outermost first.
    def _strip_last(pat, txt):
        ms = list(re.finditer(pat, txt))
        if not ms:
            return txt
        m = ms[-1]
        return txt[:m.start()] + txt[m.end():]
    body = _strip_last(r'#endif\s*//\s*USE_I2C\s*\n?', body)
    body = _strip_last(r'#endif\s*//\s*USE_\w+\s*\n?', body)

    # --- `static` is NOT allowed in a BinPlugin, AT ALL. -------------
    # A `static` FUNCTION whose address is taken (e.g. bme68x callbacks
    # `bme_dev->delay_us = Bme68x_Delayus`) yields the UN-relocated link
    # address — the lib later calls it → wild PC → Exception 28
    # LoadProhibited (xsns_09_bmp with USE_BME68X compiled in). A
    # `static` file/loc VAR is unrelocated .data/.bss (same class as
    # the pointer-global bug). Strip the leading `static` qualifier
    # everywhere BEFORE the state-wrap / RO-array passes so they then
    # treat the decl correctly (vars → MODULE_MEMORY, const arrays →
    # PROGMEM+EXEC_OFFSET, functions → plain external). Must run before
    # state-wrap. (PSTR's designed `static const char __c[]` lives in
    # the PRE preamble, not in `body`, so it is untouched.)
    body = re.sub(r'(?m)^([ \t]*)static\s+(?=[A-Za-z_])', r'\1', body)

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

    # Hoist `typedef struct [tag] { ... } NAME;` blocks into type_defs
    # so they precede MODULE_MEMORY (output order: STATE before body).
    # Needed once a MODULE_MEMORY field is a pointer to such a typedef
    # (xsns_09_bmp: `bmp_sensors_t *bmp_sensors;`). Strip C++ in-struct
    # member initializers (plugin build disallows them; MODULE_MEMORY is
    # raw jcalloc'd heap, no ctor) — same rule as the struct-instance
    # split below. Structs here have no nested braces.
    for m in list(re.finditer(
            r'^[ \t]*typedef\s+struct\s*\w*\s*\{[^{}]*\}\s*\w+\s*;',
            body, re.M)):
        td = m.group(0)
        i, j = td.index('{'), td.rindex('}')
        td = td[:i+1] + re.sub(r'\s*=\s*[^;,{}]+(?=[;,])', '',
                               td[i+1:j]) + td[j:]
        type_defs.append(td)
        body = body.replace(m.group(0), '', 1)

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
                # rewrite element reads nm[expr] -> rd(&nm[...]); the
                # PROGMEM decl uses empty `nm[]` so it is never matched.
                # PLUGIN RELOCATION: the BinPlugin loader relocates the
                # module by EXEC_OFFSET, so a plain &nm[expr] is the
                # UN-relocated link address and reads garbage/0. The
                # codebase's canonical idiom (dual_format_compat.h:341;
                # e.g. PN532 `pgm_read_byte(&PN532_NACK_TAB[EXEC_OFFSET
                # + i])`) is to bias the INDEX by EXEC_OFFSET. That is
                # only size-correct for byte arrays (index step == byte
                # step). EXEC_OFFSET == 0 in native (dual_format_compat.h)
                # so this is dual-safe. For wider element types add the
                # byte offset to a uint8_t* base then re-type.
                if rd == 'pgm_read_byte':
                    rep = lambda mm: (f'{rd}(&{nm}[EXEC_OFFSET + '
                                      f'({mm.group(1)})])')
                else:
                    rep = lambda mm: (
                        f'{rd}((const {base}*)((const uint8_t*)({nm}) + '
                        f'EXEC_OFFSET) + ({mm.group(1)}))')
                body = re.sub(
                    r'\b' + re.escape(nm) + r'\s*\[\s*([^\]\n]+?)\s*\]',
                    rep, body)
            else:
                body = ('// NEEDS-MANUAL: PROGMEM array %s (elem type %s) '
                        '- wrap element reads with the matching '
                        'pgm_read_*\n' % (nm, base)) + body

    # file-scope mutable POINTER globals: `[static] T *p [= init];`
    # (incl. typedef'd / `struct X` types). In a BinPlugin a raw
    # .data/.bss global is NOT reliably addressable — it must live in
    # MODULE_MEMORY exactly like the scalar state, else a write
    # (`p = calloc(...)`) is not read back (xsns_09_bmp: bmp_sensors /
    # bmp180_cal_data / Bme280CalibrationData stayed raw → calloc result
    # lost → BmpDetect aborted on the `!bmp_sensors` early-return). The
    # scalar regex above only matches builtin types, so pointers /
    # typedef'd types fell through. Init (`= nullptr`) is dropped (the
    # field is jcalloc-zeroed). A MODULE_MEMORY member that is a pointer
    # to an incomplete struct (e.g. bme68x under #ifdef-off) is legal.
    _PTR_TYPE = (r'(?:struct\s+\w+|\w+_t|void|char|float|double|int|'
                 r'bool|uint\d+_t|int\d+_t)')
    for m in list(re.finditer(
            r'^(?:static\s+)?(' + _PTR_TYPE + r')\s*(\*+)\s*'
            r'([A-Za-z_]\w*)\s*(=\s*[^;]+)?;', body, re.M)):
        full, ty, stars, nm = m.group(0), m.group(1), m.group(2), \
                              m.group(3)
        if not is_written(nm):
            continue
        mem_fields.append(f'{ty} {stars}{nm};')
        accessors.append(f'#define {nm} mem->{nm}')
        body = body.replace(full, '', 1)

    # plugin literal-pool rule: float consts -> FP_CONST/FLTC,
    # int >12-bit -> ICONST (in executable code only).
    body, _fpdecl = _pool_consts(body)
    if _fpdecl:
        hoist.append(_fpdecl)          # emitted before fn bodies
    # plugin: lower the safe FLTC-based float idiom to soft-float;
    # flag the rest (tc2plugin's type-inference territory).
    body, _sf_flags = _soft_float(body)

    # `PressureUnit().c_str()` — native String vs plugin const-char*
    # jt[219]. Route through the dual-safe N2D_PRESSURE_UNIT_CSTR macro
    # (defined per-mode in the file-local preamble).
    body = re.sub(r'\bPressureUnit\s*\(\s*\)\s*\.\s*c_str\s*\(\s*\)',
                  'N2D_PRESSURE_UNIT_CSTR', body)

    # WSContentSpaceButton: native 1-arg `WSContentSpaceButton(BTN)`;
    # the proven ABI form is 2-arg `jWSContentSpaceButton(A,bool)`
    # (jt[124]). Add the 2nd arg `true` (what the hand dual passes).
    # Only single-arg calls (no comma) are adapted; dual-safe (native
    # WSContentSpaceButton also accepts the optional 2nd arg).
    body = re.sub(
        r'\bWSContentSpaceButton\s*\(\s*([^(),]+?)\s*\)',
        r'WSContentSpaceButton(\1, true)', body)

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

    # --- post-INIT "presence" gate. The BinPlugin loader requires the
    #     pFUNC_INIT handler to SET `initialized` (= mt->flags.initialized,
    #     module_defines.h) or every later dispatch is gated off and the
    #     plugin produces nothing (xdrv_123_plugins.ino:3736-3754). Native
    #     register-bang drivers never set `initialized` — they gate the
    #     non-INIT branch on a count/found expr, e.g.
    #         if (FUNC_INIT == function) { Detect(); }
    #         else if (sht3x_count) { switch (function) {...} }
    #     Harvest that `else if (<gate>)` expression; the scaffold drives
    #     `initialized` from it (mirrors the hand dual's bool-detect). No
    #     gate found → assume always-present (1) + NEEDS-REVIEW flag.
    init_gate = '1'
    dm0 = re.search(r'bool\s+(?:Xsns|Xdrv)\d+\s*\([^)]*\)\s*\{', s)
    if dm0:
        gm = re.search(r'\belse\s+if\s*\(\s*(.+?)\s*\)\s*\{',
                       s[dm0.start():], re.S)
        if gm:
            init_gate = re.sub(r'\s+', ' ', gm.group(1)).strip()

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
        # `Settings->x` → `jsettings->x` (module_defines.h). SETREGS and
        # ALLOCMEM declare `SETTINGS *jsettings = *asettings;` but
        # SETMEMREGS does NOT — a SETMEMREGS fn using Settings fails to
        # compile (xsns_09_bmp BmpShow). Inject STSET (plugin: declare
        # jsettings; native: empty) — analogous to STGLOB. ONLY for the
        # SETMEMREGS prologue; SETREGS/ALLOCMEM already have jsettings so
        # adding it would redeclare.
        if tok.startswith('SETMEMREGS') and \
           re.search(r'\bSettings\s*(?:->|\))', msk[o:c]):
            tok += ' STSET'
        inner = body[o+1:c]
        if is_init:
            # The loader needs pFUNC_INIT to (a) set `initialized` so
            # later dispatch is enabled and (b) return that result. Drive
            # both off the native presence gate. `initialized` resolves
            # to mt->flags.initialized in plugin mode (module_defines.h);
            # `mt`/`mem` are in scope via this fn's ALLOCMEM prologue.
            # On detect-FAIL (no device) FREE the lazily-ALLOCMEM'd
            # MODULE_MEMORY (RETMEM) instead of leaking it — mirrors the
            # hand dual's `else { <driver>_Deinit(); }`. Capture the gate
            # into a local BEFORE RETMEM (RETMEM frees `mem`, so the gate
            # expr — which expands to mem->… — must not be read after).
            fin = ('{ int32_t _n2d_ok = (' + init_gate + ') ? 1 : 0;'
                   ' if (_n2d_ok) { initialized = 1; }'
                   ' else { RETMEM } return _n2d_ok; }')
            inner = re.sub(r'\breturn\s*;', fin, inner)
            inner = inner + '\n  ' + fin + '\n'
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
// Dual-bus I2C reads onto the jt[219] selector slot. I2cRead8 has a
// frozen 2-arg `#define I2cRead8 jI2cRead8` (jt[?]) for existing
// plugins — undef + file-local 3-arg form (like I2cWrite8). The
// others have no frozen form. FILE-LOCAL; native build untouched.
#  undef  I2cRead8
#  define I2cRead8(a,r,b)          jI2cRead8Bus((a),(r),(b))
#  undef  I2cRead24
#  define I2cRead24(a,r,b)         jI2cRead24((a),(r),(b))
#  undef  I2cRead16LE
#  define I2cRead16LE(a,r,b)       jI2cRead16LE((a),(r),(b))
#  undef  I2cReadS16_LE
#  define I2cReadS16_LE(a,r,b)     jI2cReadS16_LE((a),(r),(b))
// Native drivers iterate `for (bus=0; bus<MAX_I2C; bus++)`. In the
// plugin TU `MAX_I2C` (firmware's tasmota_globals.h SoC-controller
// count) is NOT in scope — tasmota_options.h doesn't pull it — so the
// bound is wrong/absent and the probe loop never runs (pFUNC_INIT
// returns 0, "no device found" even though I2cScan sees it). The
// plugin/dual world's bus-count symbol is `MAX_I2C_Busses` (defined by
// module_defines.h + dual_format_compat.h, always >=1). The hand duals
// use it deliberately (dual_format_compat.h:203). Map native MAX_I2C
// onto it, FILE-LOCAL.
#  ifdef MAX_I2C
#    undef MAX_I2C
#  endif
#  define MAX_I2C MAX_I2C_Busses
// module_defines.h has `#define TasmotaGlobal *tgbl`, so native
// `TasmotaGlobal.member` mis-parses as `*(tgbl.member)`. Re-point to
// `(*tgbl)` so `.member` works with the STGLOB-declared `tgbl`.
// File-local; native build & other plugins untouched.
#  undef  TasmotaGlobal
#  define TasmotaGlobal (*tgbl)
// Same idiom for XdrvMailbox: module_defines.h has
// `#define XdrvMailbox (jXdrvMailbox)` (a POINTER, jt[57]); native
// driver code uses `XdrvMailbox.index` (instance `.`). Re-point to the
// dereferenced form so `.member` works unchanged in the plugin —
// exactly what the hand dual xdrv_28_pcf8574_dual.cpp does per-use.
// File-local; native build untouched.
#  undef  XdrvMailbox
#  define XdrvMailbox (*jXdrvMailbox)
// SETMEMREGS (unlike SETREGS/ALLOCMEM) does NOT declare `jsettings`,
// but `Settings` -> `jsettings` (module_defines.h). Scaffolded
// SETMEMREGS fns that use `Settings->` get a ` STSET` prologue token
// which declares it from the existing `asettings` (jt[135]) — no ABI
// change. Native: `Settings` is the real global, STSET is empty.
#  define STSET SETTINGS *jsettings = *asettings;
// `PressureUnit()` returns String (native) — the jt[219] form returns
// `const char*`. Scaffolder rewrites `PressureUnit().c_str()` to this
// dual-safe macro: plugin → jPressureUnit() (already const char*);
// native → the real String.c_str().
#  define N2D_PRESSURE_UNIT_CSTR  jPressureUnit()
#else
#  define STSET
#  define N2D_PRESSURE_UNIT_CSTR  PressureUnit().c_str()
#endif
#ifdef _{U}_N2D_ENABLED
// ===================================================================
// native2dual PoC scaffold of {os.path.basename(src_path)} — FAITHFUL
// (no feature drop). Bodies verbatim; only mechanical wrapping added.
// Unresolved constructs are left for the compiler to reject — that
// error set IS the deliverable (empirical NEEDS-* taxonomy).
// ===================================================================
'''
    # xnum keeps its zero-padded form for the `Xsns09` name regex, but
    # as a C numeric literal `09` is an INVALID octal constant — emit
    # the decimal value.
    # Xdrv## -> MODULE_TYPE_DRIVER, Xsns## -> MODULE_TYPE_SENSOR
    # (the hand dual xdrv_28_pcf8574_dual.cpp is explicit: a driver
    # MUST be _DRIVER, not _SENSOR — wrong type → loader mis-dispatch).
    mtype = 'MODULE_TYPE_DRIVER' if disp == 'Xdrv' else 'MODULE_TYPE_SENSOR'
    PART = ['PUSH_OPTIONS',
            f'MODULE_DESCRIPTOR("{U[:6]}", {mtype}, '
            f'1<<16|{int(xnum)},'
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
            '  int32_t result = 0;',
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
        if fn == 'FUNC_INIT':
            # Capture the INIT fn's return — the scaffolded INIT body
            # returns `initialized` (set from the native presence gate);
            # the loader logs/uses this and the flag enables dispatch.
            DISP.append(f'    case {pf}: {{ result = {call} break; }}')
        else:
            DISP.append(f'    case {pf}: {{ {call} break; }}')
    DISP += [deinit,
             '    default: break;',
             '  }', '  return result;', '}', 'PULL_OPTIONS', '#endif',
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
    # i2c_enabled was appended to GTBL/TGTAB (idx 13) — now resolvable.
    GTBL_ABSENT = ['spi_enabled2', 'free_gpio']
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
    if _sf_flags:
        FLAGS += ['// ' + '='*64,
                  f'// NEEDS-SOFTFLOAT ({len(_sf_flags)}) — float-typed '
                  'expressions the',
                  '// scaffolder will NOT auto-lower (general C++ float→'
                  'soft-float is',
                  '// tc2plugin type-inference territory). Bare HW float ops '
                  'mis-relocate',
                  '// under the plugin loader — a human must rewrite these '
                  'via fdiv/fmul/',
                  '// fadd/fdiff/tofloat/FLTC (see xsns_14_sht3x_dual.cpp '
                  'for the pattern):']
        for f_ in _sf_flags:
            FLAGS.append('//   ' + f_)
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
