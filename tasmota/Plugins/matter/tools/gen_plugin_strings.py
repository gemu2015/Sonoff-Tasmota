#!/usr/bin/env python3
"""gen_plugin_strings.py — move matter_c's string literals out of host .rodata.

    python3 tasmota/Plugins/matter/tools/gen_plugin_strings.py [--check]

A BinPlugin has no .rodata of its own: every "literal" in the amalgamated
matter sources lands in the plugin-host firmware's .rodata, at an address that
means nothing in the running firmware (blib_audit: PTR-FW). PSTR is no way out
either: the module is mapped on the instruction bus, and matter_c reads its
strings byte-wise (snprintf formats, strlen, TLV copies, mDNS/kv via the host)
— a LoadStoreError on ESP32/S3.

So all literals go into ONE PROGMEM blob (include/mtrc_plugin_strings.h) that
the plugin copies word by word into its heap block at pFUNC_INIT, and each
literal in the source becomes

    MTRC_S(<id>, "the literal")

which is the literal itself in every non-plugin build and a pointer into the
RAM copy in the plugin (mtrc_plugin_statics.h). The literal stays in the source
for readability and for a compile-time length check against <id>; identical
texts share one id. Re-run after editing a literal: ids are renumbered from
scratch, the result is deterministic. --check only reports whether the sources
and the header are up to date (exit 1 if not).

Left alone: #include lines, char-array initializers (`x[] = "..."`, handled as
tables), literals in brace initializer lists (pointer tables must be filled at
run time instead).
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                        # tasmota/Plugins/matter
SRC = sorted(glob.glob(os.path.join(ROOT, "src", "*_c.h")))
HDR = os.path.join(ROOT, "include", "mtrc_plugin_strings.h")

# comments, MTRC_S(id, <literals>), plain literal runs, char literals
TOK = re.compile(r'//[^\n]*|/\*.*?\*/'
                 r'|MTRC_S\(\s*\d+\s*,\s*((?:"(?:\\.|[^"\\\n])*"\s*)+)\)'
                 r'|((?:"(?:\\.|[^"\\\n])*"\s*)+)'
                 r"|'(?:\\.|[^'\\\n])*'", re.S)
LIT = re.compile(r'"((?:\\.|[^"\\\n])*)"')


ESC = {"n": 10, "t": 9, "r": 13, "0": 0, "\\": 92, '"': 34, "'": 39, "a": 7, "b": 8, "f": 12, "v": 11, "?": 63}


def decode(lits):
    """C literal run -> bytes as GCC emits them (source is UTF-8), without the NUL"""
    out = bytearray()
    for body in LIT.findall(lits):
        raw, i = body.encode("utf-8"), 0
        while i < len(raw):
            c = raw[i]
            if c != 0x5C:                                  # not a backslash
                out.append(c); i += 1; continue
            n = chr(raw[i + 1])
            if n == "x":
                m = re.match(rb"[0-9a-fA-F]+", raw[i + 2:])
                out.append(int(m.group(0), 16) & 0xFF); i += 2 + len(m.group(0))
            elif n in "01234567":
                m = re.match(rb"[0-7]{1,3}", raw[i + 1:])
                out.append(int(m.group(0), 8) & 0xFF); i += 1 + len(m.group(0))
            else:
                out.append(ESC[n]); i += 2
    return bytes(out)


def skip(s, start):
    line = s[s.rfind("\n", 0, start) + 1:start]
    if line.lstrip().startswith("#include"):
        return True
    before = s[max(0, start - 60):start]
    if re.search(r'\]\s*=\s*$', before):              # char x[] = "..."
        return True
    if re.search(r'[{,]\s*$', before) and re.search(r'=\s*\{[^;]*$', before):
        return True                                    # { "a", "b" } initializer list
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    texts = {}          # bytes -> id (first occurrence order)
    plan = []           # (path, source, [(start, end, lits)])
    for path in SRC:
        s = open(path, encoding="utf-8").read()
        edits = []
        for m in TOK.finditer(s):
            lits = m.group(1) or m.group(2)
            if not lits:
                continue
            if m.group(2) and skip(s, m.start()):
                continue
            lits = lits.rstrip()
            t = decode(lits)
            if t not in texts:
                texts[t] = len(texts)
            end = m.start() + len(m.group(0).rstrip()) if m.group(2) else m.end()
            edits.append((m.start(), end, lits))
        plan.append((path, s, edits))

    changed = []
    for path, s, edits in plan:
        out, pos = [], 0
        for start, end, lits in edits:
            out.append(s[pos:start])
            out.append(f"MTRC_S({texts[decode(lits)]}, {lits})")
            pos = end
        out.append(s[pos:])
        new = "".join(out)
        if new != s:
            changed.append(path)
            if not a.check:
                open(path, "w", encoding="utf-8").write(new)

    # blob: all texts NUL-terminated, concatenated, padded to whole words
    order = sorted(texts, key=texts.get)
    blob, offs = b"", []
    for t in order:
        offs.append(len(blob))
        blob += t + b"\0"
    blob += b"\0" * (-len(blob) % 4)
    words = [int.from_bytes(blob[i:i + 4], "little") for i in range(0, len(blob), 4)]
    lines = ["// GENERATED by tools/gen_plugin_strings.py — do not edit, re-run the tool.",
             "// All string literals of the amalgamated matter sources, for the plugin build",
             "// (see the tool's docstring and mtrc_plugin_statics.h).",
             "#ifndef MTRC_PLUGIN_STRINGS_H",
             "#define MTRC_PLUGIN_STRINGS_H",
             "",
             f"#define MTRC_STR_COUNT  {len(order)}",
             f"#define MTRC_STR_WORDS  {len(words)}   // {len(blob)} bytes",
             "",
             "// offset / length (incl. NUL) of each text in the blob",
             "enum {"]
    for i, t in enumerate(order):
        shown = t.decode("latin-1").replace("*/", "*_/")[:60]
        lines.append(f"  MTRC_SOFF_{i} = {offs[i]}, MTRC_SLEN_{i} = {len(t) + 1},   /* {shown} */")
    lines += ["};", "",
              "// word-aligned blob in the module (PROGMEM), copied to RAM at pFUNC_INIT",
              "const uint32_t mtrc_str_blob[MTRC_STR_WORDS] PROGMEM = {"]
    for i in range(0, len(words), 6):
        lines.append("  " + ", ".join(f"0x{w:08x}" for w in words[i:i + 6]) + ",")
    lines += ["};", "", "#endif // MTRC_PLUGIN_STRINGS_H", ""]
    hdr = "\n".join(lines)
    old = open(HDR, encoding="utf-8").read() if os.path.exists(HDR) else ""
    if hdr != old:
        changed.append(HDR)
        if not a.check:
            open(HDR, "w", encoding="utf-8").write(hdr)

    print(f"{len(order)} texts, {len(blob)} bytes; "
          + (("out of date: " if a.check else "rewritten: ") + ", ".join(os.path.basename(c) for c in changed)
             if changed else "up to date"))
    return 1 if (a.check and changed) else 0


if __name__ == "__main__":
    sys.exit(main())
