#!/usr/bin/env python3
"""
lib2mod.py — native2dual's multi-file *library-module* path.

scaffold.py turns ONE native xsns_/xdrv_ driver into a DUAL-mode file
(compiles as native AND as a BinPlugin). matter_c is different: a 16-file
portable C library that is ONLY ever a plugin (a *library module*), never
compiled into firmware. So we drop all the dual-mode hedging and the
native-driver assumptions (Xsns dispatcher, USE_I2C guards, TasmotaGlobal,
license strip) and apply ONLY the relocation discipline the BinPlugin
loader needs — the same rules scaffold.py proved, lifted here:

  pass S  strings    inline "..." used as a value  -> PSTR("...")  (the
          framework macro places it in .plugin.mod_string and returns the
          EXEC_OFFSET-corrected address). SKIP array/aggregate initializers
          (`char x[]="..."`, `={...}`), #-lines, comments.
  pass A  RO arrays  file-scope `const uint8_t X[]` -> PROGMEM, and every
          USE of X -> MP8(X) = (const uint8_t*)X + EXEC_OFFSET. (ESP32 flash
          is directly readable; only the *address* needs relocating.)
  pass F  floats     float literals in code -> one module-wide FP_CONST[]
          PROGMEM + FLTC(idx). (int literals need nothing — ICONST≡A.)

NOT here (matter uses its own keystone, already applied):
  - strip static + MODULE_PART  -> gen_module_parts.py
  - file-scope mutable state    -> matter's `g`/MODULE_MEMORY keystone

Offset-exact (comments/strings/preprocessor handled by a shared scanner)
and idempotent. Run with --dry-run first; every wrap/skip is reported.

Run:  python3 lib2mod.py --dir <c-src-dir> [--pass S,A,F] [--dry-run]
"""

import argparse
import re
import sys
from pathlib import Path


def scan_mask(b):
    """Return (clean, kind) where kind[i] classifies byte i of b:
       0=code 1=string-literal-body 2=comment 3=preprocessor.
    `clean` blanks non-code to spaces (length-preserving) so code-only
    regexes are safe. String *delimiters* (the quotes) are code (0) so we
    can find a literal's start; its interior is 1."""
    n = len(b)
    clean = list(b)
    kind = bytearray(n)
    st = None
    at_ls = True
    i = 0
    while i < n:
        c = b[i]
        d = b[i + 1] if i + 1 < n else ''
        if st == 'blk':
            kind[i] = 2
            if c != '\n':
                clean[i] = ' '
            if c == '*' and d == '/':
                kind[i + 1] = 2
                clean[i + 1] = ' '
                i += 2
                st = None
                continue
            i += 1
            continue
        if st == 'line':
            if c == '\n':
                st = None
                at_ls = True
            else:
                kind[i] = 2
                clean[i] = ' '
            i += 1
            continue
        if st == 'pp':
            if c == '\n' and (i == 0 or b[i - 1] != '\\'):
                st = None
                at_ls = True
            else:
                if c != '\n':
                    kind[i] = 3
                    clean[i] = ' '
            i += 1
            continue
        if st in ('"', "'"):
            if c == '\\':
                kind[i] = 1
                if i + 1 < n:
                    kind[i + 1] = 1
                    clean[i + 1] = ' '
                clean[i] = ' '
                i += 2
                continue
            if c == st:                       # closing quote = code
                st = None
                i += 1
                continue
            kind[i] = 1
            if c != '\n':
                clean[i] = ' '
            i += 1
            continue
        # code
        if c == '#' and at_ls:
            st = 'pp'
            kind[i] = 3
            clean[i] = ' '
            i += 1
            at_ls = False
            continue
        if c == '/' and d == '*':
            st = 'blk'
            kind[i] = kind[i + 1] = 2
            clean[i] = clean[i + 1] = ' '
            i += 2
            at_ls = False
            continue
        if c == '/' and d == '/':
            st = 'line'
            kind[i] = kind[i + 1] = 2
            clean[i] = clean[i + 1] = ' '
            i += 2
            at_ls = False
            continue
        if c == '"':
            st = '"'
        elif c == "'":
            st = "'"
        if c == '\n':
            at_ls = True
        elif not c.isspace():
            at_ls = False
        i += 1
    return ''.join(clean), kind


def _agg_spans(clean):
    """Byte spans inside `= { … }` aggregate initializers (no PSTR there)."""
    spans = []
    for mm in re.finditer(r'=\s*\{', clean):
        s0 = mm.end() - 1
        depth = 0
        j = s0
        while j < len(clean):
            if clean[j] == '{':
                depth += 1
            elif clean[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        spans.append((mm.start(), j + 1))
    return spans


def pass_strings(b):
    """PSTR-wrap inline string literals used as values. Returns
    (new_b, wrapped, flagged) where flagged = [(line, snippet)] for
    literals in non-call contexts a human should eyeball."""
    clean, kind = scan_mask(b)
    aggs = _agg_spans(clean)

    def in_agg(p):
        return any(a <= p < z for a, z in aggs)

    # find string literals: a run where kind==1 bracketed by code quotes.
    lits = []
    i, n = 0, len(b)
    while i < n:
        if b[i] == '"' and (kind[i] == 0):     # opening quote (code)
            j = i + 1
            while j < n and not (b[j] == '"' and kind[j] == 0):
                j += 1
            lits.append((i, j + 1))            # [open, close+1)
            i = j + 1
            continue
        i += 1

    edits = []
    wrapped = 0
    flagged = []
    for s0, e0 in lits:
        if in_agg(s0):
            continue                            # aggregate initializer
        pre = b[:s0]
        if re.search(r'\b(?:PSTR|MP8)\s*\(\s*$', pre):
            continue                            # already wrapped
        # adjacent string concatenation "a" "b" + PSTR breaks → flag.
        if re.match(r'\s*"', b[e0:]):
            flagged.append((b.count('\n', 0, s0) + 1, b[s0:e0][:40] + ' …(concat)'))
            continue
        pm = re.search(r'([(),?:=])\s*$', pre)
        prev = pm.group(1) if pm else ''
        if prev in ('(', ',', '?', ':', ')'):   # call-arg / ternary / cast
            edits.append((s0, e0, 'PSTR(' + b[s0:e0] + ')'))
            wrapped += 1
        elif prev == '=':                       # initializer → skip (array)
            continue
        elif re.search(r'\breturn\s*$', pre):    # return "literal"
            edits.append((s0, e0, 'PSTR(' + b[s0:e0] + ')'))
            wrapped += 1
        else:
            flagged.append((b.count('\n', 0, s0) + 1,
                            (b[max(0, s0 - 8):e0]).replace('\n', ' ')[:48]))
    for s0, e0, rep in sorted(edits, reverse=True):
        b = b[:s0] + rep + b[e0:]
    return b, wrapped, flagged


# File-scope (col-0) only. Function-local `static const X[]` also need the
# treatment but their short names (info/sr/inv/…) risk colliding with locals
# elsewhere under a cross-file global replace, so they're flagged, not wrapped.
_ARR_DECL = re.compile(
    r'(?m)^()(?:static\s+)?const\s+((?:uint8_t|char))\s+'
    r'([A-Za-z_]\w*)\s*(\[[^\]]*\])\s*=')
_ARR_LOCAL = re.compile(
    r'(?m)^[ \t]+(?:static\s+)?const\s+(?:uint8_t|char)\s+'
    r'([A-Za-z_]\w*)\s*\[[^\]]*\]\s*=')


def collect_arrays(files, verbose=False):
    """name -> elem-type, for every FILE-SCOPE `const uint8_t/char NAME[]=`."""
    arrays = {}
    locals_ = set()
    for f in files:
        txt = f.read_text()
        for m in _ARR_DECL.finditer(txt):
            arrays[m.group(3)] = m.group(2)
        for m in _ARR_LOCAL.finditer(txt):
            locals_.add(f'{f.name}:{m.group(1)}')
    if verbose and locals_:
        print('LOCAL static const arrays (handle by hand — relocation-unsafe '
              'until MP8-wrapped in their own scope):')
        for x in sorted(locals_):
            print(f'    {x}')
    return arrays


def pass_ro_arrays(b, names):
    """PROGMEM the RO-array decls + MP8()-wrap every other use (so the
    base address relocates). Keep `sizeof(NAME)` and the declaration
    itself untouched. Returns (new_b, n_decls, n_uses)."""
    clean, kind = scan_mask(b)
    # 1) record declaration spans (so we never wrap the declared name)
    decl_spans = []
    decl_edits = []
    for m in _ARR_DECL.finditer(b):
        if m.group(3) not in names:
            continue
        decl_spans.append((m.start(), m.end()))
        # strip `static`, insert PROGMEM before `=`
        new = f'{m.group(1)}const {m.group(2)} {m.group(3)}{m.group(4)} PROGMEM ='
        decl_edits.append((m.start(), m.end(), new))

    def in_decl(p):
        return any(a <= p < z for a, z in decl_spans)

    # 2) wrap uses of any array name (code only, not in a decl, not sizeof,
    #    not already MP8-wrapped)
    use_edits = []
    n_uses = 0
    name_re = re.compile(r'\b(' + '|'.join(re.escape(n) for n in names) + r')\b')
    for m in name_re.finditer(clean):           # clean = code-only (no strings/comments)
        s0 = m.start()
        if in_decl(s0):
            continue
        pre = b[:s0]
        if re.search(r'sizeof\s*\(\s*$', pre):
            continue                            # keep sizeof(NAME)
        if re.search(r'\bMP8\s*\(\s*$', pre):
            continue
        use_edits.append((s0, m.end(), f'MP8({m.group(1)})'))
        n_uses += 1

    for s0, e0, rep in sorted(decl_edits + use_edits, key=lambda r: -r[0]):
        b = b[:s0] + rep + b[e0:]
    return b, len(decl_edits), n_uses


def _split_commas(s):
    """Split a declarator list on TOP-LEVEL commas (respecting []/())."""
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '[(':
            depth += 1
        elif c in '])':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur)
    return out


_BUF = re.compile(
    r'(?m)^([ \t]+)static\s+'
    r'((?:unsigned\s+)?(?:u?int(?:8|16|32|64)_t|char|int|size_t))\s+'
    r'([A-Za-z_][^;=]*\[[^;=]*);')


def pass_buffers(text, idx0, fields):
    """Function-local `static T buf[N];` scratch buffers → a per-buffer
    field in the ctx g, aliased back with a C++ reference-to-array so
    every use + sizeof is unchanged. A BinPlugin has no .bss, so these
    static buffers would resolve to garbage addresses on-device. Appends
    (type, field, dim) to `fields`; returns (new_text, next_idx)."""
    clean, _ = scan_mask(text)
    edits = []
    idx = idx0
    for m in _BUF.finditer(clean):
        indent, ty = m.group(1), m.group(2)
        decl = text[m.start(3):m.end(3)]
        parsed = []                            # (name, dim) — validate ALL first
        ok = True
        for part in _split_commas(decl):
            am = re.match(r'\s*([A-Za-z_]\w*)\s*\[(.+)\]\s*$', part.strip())
            if not am:
                ok = False
                break
            parsed.append((am.group(1), am.group(2).strip()))
        if not ok or not parsed:
            continue                           # leave non-array statics alone
        refs = []
        for name, dim in parsed:               # commit only once valid
            fld = f'scr_{idx}'
            idx += 1
            fields.append((ty, fld, dim))
            refs.append(f'{ty} (&{name})[{dim}] = g.{fld};')
        edits.append((m.start(), m.end(), indent + ' '.join(refs)))
    for s0, e0, rep in sorted(edits, reverse=True):
        text = text[:s0] + rep + text[e0:]
    return text, idx


def pass_destatic_decls(text):
    """Strip `static` from FUNCTION forward-declarations (the definitions
    were already de-static'd by gen_module_parts; a lingering `static`
    forward-decl forces the function back to internal linkage). A decl is
    `static <ret> name(<args>);` — has '(' before ';' and no '='/'{'.
    Returns (new_text, n)."""
    clean, _ = scan_mask(text)
    edits = []
    for m in re.finditer(r'(?m)^([ \t]*)static\s+(?=[A-Za-z_][^\n;{]*\([^\n{=]*\)\s*;)',
                         clean):
        edits.append((m.start(1) + len(m.group(1)), m.end(), ''))
    for s0, e0, rep in sorted(edits, reverse=True):
        text = text[:s0] + rep + text[e0:]
    return text, len(edits)


def manual_flags(b):
    """Relocation-unsafe const constructs lib2mod does NOT auto-handle —
    a human finishes these in the editor. Returns [(line, note)]."""
    clean, _ = scan_mask(b)
    out = []

    def ln(p):
        return b.count('\n', 0, p) + 1
    for m in re.finditer(r'(?m)^[ \t]*(?:static\s+)?const\s[\w \t*]+?\w+\s*'
                         r'\[[^\]]*\]\s*\[', clean):
        out.append((ln(m.start()), '2D const array — copy to a stack array '
                    'at fn entry (memcpy from MP8-corrected source)'))
    for m in re.finditer(r'(?m)const\s+char\s*\*\s*\w+\s*\[\s*\]\s*=\s*\{',
                         clean):
        out.append((ln(m.start()), 'const char *[] — both the pointer array '
                    'AND each string relocate; rebuild at runtime'))
    for m in re.finditer(r'(?m)^[ \t]+(?:static\s+)const\s+(?:uint8_t|char)'
                         r'\s+\w+\s*\[[^\]]*\]\s*=', clean):
        out.append((ln(m.start()), 'function-local const array — PROGMEM + '
                    'scoped MP8() (name may collide; edit in place)'))
    for m in re.finditer(r'(?<![\w.])\d+\.\d+(?![\w.])', clean):
        out.append((ln(m.start()), 'float literal — FP_CONST/FLTC'))
    out.sort()
    return out


def translate_text(text, names):
    """Apply the auto passes (S + A) to one file's text. Returns
    (new_text, flags) — flags = PSTR non-call flags + manual_flags."""
    nb, _w, sfl = pass_strings(text)
    nb, _nd, _nu = pass_ro_arrays(nb, names) if names else (nb, 0, 0)
    flags = [(ln, 'string literal — not a call-arg; PSTR by hand if its '
              'address is used: ' + sn) for ln, sn in sfl]
    flags += [(ln, n) for ln, n in manual_flags(text)]
    flags.sort()
    return nb, flags


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', required=True)
    ap.add_argument('--passes', default='S')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    src = Path(a.dir)
    files = sorted(src.glob('*.c'))
    arrays = collect_arrays(files, verbose=True) if 'A' in a.passes else {}
    if arrays:
        print(f'RO arrays: {", ".join(sorted(arrays))}')
    tot_w = 0
    tot_a = 0
    tot_f = []
    bfields = []          # (type, field, dim) for every relocated scratch buffer
    bidx = 0
    tot_ds = 0
    for f in files:
        b = f.read_text()
        nb = b
        if 'S' in a.passes:
            nb, w, fl = pass_strings(nb)
            tot_w += w
            for ln, sn in fl:
                tot_f.append((f.name, ln, sn))
        if 'A' in a.passes and arrays:
            nb, nd, nu = pass_ro_arrays(nb, arrays)
            tot_a += nu
            if nd or nu:
                print(f'  {f.name:18} PROGMEM {nd} decl / MP8 {nu} use(s)')
        if 'B' in a.passes:
            nb, ds = pass_destatic_decls(nb)
            tot_ds += ds
            before = bidx
            nb, bidx = pass_buffers(nb, bidx, bfields)
            if ds or (bidx - before):
                print(f'  {f.name:18} de-static {ds} decl / {bidx-before} '
                      f'scratch buffer(s) → ctx')
        if not a.dry_run and nb != b:
            f.write_text(nb)
    if 'B' in a.passes and bfields:
        hdr = src.parent / 'include' / 'mtrc_scratch_fields.h'
        lines = ['// GENERATED by lib2mod.py pass B — scratch buffers moved off',
                 '// the (non-existent) plugin .bss into the relocatable PSRAM',
                 '// ctx. #included inside matter_ctx_t. DO NOT EDIT.']
        for ty, fld, dim in bfields:
            lines.append(f'{ty} {fld}[{dim}];')
        if not a.dry_run:
            hdr.write_text('\n'.join(lines) + '\n')
            # inject the #include into matter_ctx_t (before its closing brace)
            mc = src / 'matter_c.c'
            t = mc.read_text()
            inc = '#include "mtrc_scratch_fields.h"'
            if inc not in t:
                t = t.replace('} matter_ctx_t;',
                              f'  {inc}   // pass-B scratch buffers\n}} matter_ctx_t;', 1)
                mc.write_text(t)
        print(f'{"would move" if a.dry_run else "moved"} {len(bfields)} scratch '
              f'buffers → ctx (include/mtrc_scratch_fields.h); de-static {tot_ds}')
    if 'S' in a.passes:
        print(f'{"would wrap" if a.dry_run else "wrapped"} {tot_w} string literals in PSTR')
    if 'A' in a.passes:
        print(f'{"would wrap" if a.dry_run else "wrapped"} {tot_a} RO-array uses in MP8')
    if tot_f:
        print(f'\nFLAGGED ({len(tot_f)}) — non-call-arg string literals, review by hand:')
        for fn, ln, sn in tot_f:
            print(f'    {fn}:{ln}  {sn}')


if __name__ == '__main__':
    main()
