#!/usr/bin/env python3
"""
gen_module_parts.py — tag every matter function MODULE_PART (in place).

The Tasmota BinPlugin loader only relocates functions the linker gathered
into `.plugin.mod_part` — i.e. functions tagged `MODULE_PART`. A plain
amalgamation of the matter sources leaves every function in `.flash.text`,
so the extracted module is ~empty. The framework idiom is to put
`MODULE_PART` on each function; we also strip `static` so each one is an
external, individually-relocatable symbol the linker can't inline away or
merge.

This rewrites the PLUGIN COPY of the matter sources
(tasmota/Plugins/matter/src/*.c — NOT the firmware lib in
lib/libesp32_div/matter_c) so each top-level function definition reads:

    static void foo(...) {        ->   MODULE_PART void foo(...) {
    int bar(...) {                ->   MODULE_PART int  bar(...) {

Idempotent: a definition already starting with MODULE_PART is left alone.
Comments, preprocessor directives, data definitions (`const x[] = {…}`),
and function-pointer variables are not touched. Re-run after editing a
matter source. The transform is offset-exact (comments/preprocessor are
blanked length-preserving, so positions found in the cleaned text apply
verbatim to the original).

Run:  python3 tasmota/Plugins/matter/gen_module_parts.py [--dry-run]
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC = ROOT / 'src'

FILES = [
    'matter_c.c', 'mtrc_tlv.c', 'mtrc_frame.c', 'mtrc_crypto.c',
    'mtrc_sec.c', 'mtrc_mrp.c', 'mtrc_store.c', 'mtrc_cert.c', 'mtrc_csr.c',
    'mtrc_spake2p.c', 'mtrc_pase.c', 'mtrc_case_msg.c', 'mtrc_case.c',
    'mtrc_dm.c', 'mtrc_im.c', 'qrcodegen.c',
]

KEYWORDS = {
    'if', 'for', 'while', 'switch', 'do', 'else', 'return', 'typedef',
    'struct', 'enum', 'union', 'case', 'default', 'goto', 'sizeof',
    'break', 'continue', 'static_assert', '_Static_assert', 'MODULE_PART',
}


def blank_noncode(s):
    """Replace comment + preprocessor bytes with spaces (newlines kept),
    so the result is the same LENGTH as the input — offsets found in it
    map 1:1 onto the original text. String/char literals are preserved."""
    out = list(s)
    i, n = 0, len(s)
    st = None
    at_line_start = True
    while i < n:
        c = s[i]
        d = s[i + 1] if i + 1 < n else ''
        if st == 'blk':
            if c != '\n':
                out[i] = ' '
            if c == '*' and d == '/':
                out[i + 1] = ' '
                i += 2
                st = None
                continue
            i += 1
            continue
        if st == 'line':
            if c == '\n':
                st = None
                at_line_start = True
            else:
                out[i] = ' '
            i += 1
            continue
        if st == 'pp':                       # preprocessor line (blank it)
            if c == '\n':
                cont = ''.join(out).rstrip()  # not used; track via prev char
            if c == '\n' and (i == 0 or s[i - 1] != '\\'):
                st = None
                at_line_start = True
            else:
                if c != '\n':
                    out[i] = ' '
            i += 1
            continue
        if st in ('str', 'chr'):
            if c == '\\':
                i += 2
                continue
            if (st == 'str' and c == '"') or (st == 'chr' and c == "'"):
                st = None
            i += 1
            continue
        # normal code
        if c == '#' and at_line_start:
            st = 'pp'
            out[i] = ' '
            i += 1
            at_line_start = False
            continue
        if c == '/' and d == '*':
            st = 'blk'
            out[i] = ' '
            out[i + 1] = ' '
            i += 2
            at_line_start = False
            continue
        if c == '/' and d == '/':
            st = 'line'
            out[i] = ' '
            out[i + 1] = ' '
            i += 2
            at_line_start = False
            continue
        if c == '"':
            st = 'str'
        elif c == "'":
            st = 'chr'
        if c == '\n':
            at_line_start = True
        elif not c.isspace():
            at_line_start = False
        i += 1
    return ''.join(out)


def is_func_def(seg):
    """seg = code accumulated before a depth-0 '{'. Return the function
    name if seg is a function definition header, else None."""
    seg = seg.strip()
    if not seg:
        return None
    first = seg.find('(')
    if first < 0:
        return None
    pre = seg[:first]
    if '=' in pre or '[' in pre:             # initializer / array, not a func
        return None
    idents = re.findall(r'[A-Za-z_]\w*', pre)
    if not idents:
        return None
    # struct/enum/union may LEAD a return type (`struct Seg foo(...)`) — only
    # the control keywords / typedef rule out a function. (A struct/var/array
    # *definition* is already excluded above via '='/'[' or the no-'(' path.)
    LEADING = KEYWORDS - {'struct', 'enum', 'union'}
    if idents[0] in LEADING or idents[-1] in KEYWORDS:
        return None
    return idents[-1]


def find_def_offsets(clean):
    """Return [offset, …] — the start (first non-space of the leading
    decl-specifier) of each top-level function definition, in `clean`
    (offsets valid for the original)."""
    offs = []
    depth = 0
    i, n = 0, len(clean)
    stmt_start = None
    seg_chars = []
    while i < n:
        c = clean[i]
        if depth == 0:
            if c == '{':
                seg = ''.join(seg_chars)
                if is_func_def(seg) and stmt_start is not None:
                    offs.append(stmt_start)
                seg_chars = []
                stmt_start = None
                depth = 1
            elif c in ';}':
                seg_chars = []
                stmt_start = None
            else:
                if not c.isspace() and stmt_start is None:
                    stmt_start = i
                seg_chars.append(c)
        else:
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
        i += 1
    return offs


_STATIC_RE = re.compile(r'static\s+')


def tag_file(path, dry=False):
    orig = path.read_text()
    clean = blank_noncode(orig)
    assert len(clean) == len(orig), f'{path.name}: length drift'
    offs = find_def_offsets(clean)
    edits = []                               # (start, end, replacement)
    tagged = 0
    for off in offs:
        # already tagged?  (look at the original at this offset)
        if orig.startswith('MODULE_PART', off):
            continue
        m = _STATIC_RE.match(orig, off)
        end = m.end() if m else off          # drop a leading `static `
        edits.append((off, end, 'MODULE_PART '))
        tagged += 1
    # apply high-offset-first so earlier offsets stay valid
    new = orig
    for start, end, rep in sorted(edits, reverse=True):
        new = new[:start] + rep + new[end:]
    if not dry and new != orig:
        path.write_text(new)
    return tagged, len(offs)


def main():
    dry = '--dry-run' in sys.argv
    total = 0
    for f in FILES:
        t, n = tag_file(SRC / f, dry=dry)
        print(f'  {f:18} {t:3} tagged / {n:3} defs'
              + ('  (dry-run)' if dry else ''))
        total += t
    print(f'{"would tag" if dry else "tagged"} {total} functions MODULE_PART'
          f' across {len(FILES)} files')


if __name__ == '__main__':
    main()
