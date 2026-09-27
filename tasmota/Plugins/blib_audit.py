#!/usr/bin/env python3
"""
blib_audit.py — find the runtime-fatal spots in a BinPlugin BEFORE it runs.

    python3 tasmota/Plugins/blib_audit.py [--elf .pio/build/tasmota32-plugin/firmware.elf]
                                          [--name MATTERF] [--all]

Why: a plugin module is copied out of the plugin-host firmware (between the
4A FC AA 55 / 55 AA FC 4A sync words, grepmodule-firmware.py) and run at another
address. Everything that is not position independent compiles and links fine
and only fails on the device, usually as a silent "Software reset CPU":

  CALL-OUT   call/j to an address outside the module — a compiler helper
             (__divdi3, __addsf3 …) or a firmware function called directly
             instead of through the jumptable
  LIT-OUT    l32r literal slot outside the module (the literal pool was not
             placed inside the bracket)
  PTR-FW     literal holding an absolute address into the host firmware —
             typically a string literal or const table in the host .rodata
  PTR-BSS    literal holding the address of a zero-initialised global (.bss)
             in the host — mutable state that must move into MODULE_MEMORY
  ROM        literal holding a ROM address (double math, libgcc helpers): the
             plugin build links against the classic ESP32 ROM — wrong on S2/S3
  PTR-SELF   literal holding an absolute address inside the module — only
             correct if the code adds EXEC_OFFSET before using it (PROGMEM
             tables read via GUI32p), so these are listed for review
  CONST      a plain number in the module's literal pool (l32r) — the house rule
             (readme.md) wants every constant beyond the 12-bit movi range and
             every float constant in a PROGMEM table read via EXEC_OFFSET
  NUM?       literal whose value lies in host code or ROM but is not the start
             of a symbol — almost always a number that only looks like an
             address (a float between 2.0 and ~4.5 is 0x400xxxxx, e.g. 2.22 =
             0x400e147b), listed for review and not counted. A real code
             pointer is a function address, i.e. a symbol start.

Plain numeric literals stored inside the module are fine and not reported.
Literals inside the 64-byte module header (FLASH_MODULE: mtv, jtab, execution
offset …) are written by the loader at load time — e.g. gettbl() in plugins.S
reads `module_header+48` — and are counted as HEADER, not as problems.
Xtensa (ESP32/S2/S3) and, for the ELF, RISC-V (C3/C6: calls and resolved lui/auipc
addresses instead of literal pools).

--bin FILE audits an already extracted module (e.g. a shipped *_32.bin) without
its ELF: the header's mod_start_org gives the link address, and every l32r is
checked against it. Without section information host pointers are reported
together as PTR-ABS, calls into ROM as ROM (ESP32 ROM addresses — wrong on S2/S3).
"""
import argparse
import bisect
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OBJDUMP_CANDIDATES = [
    os.path.expanduser("~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32-elf-objdump"),
    os.path.expanduser("~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-objdump"),
]
RISCV_OBJDUMP = os.path.expanduser("~/.platformio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-objdump")


def is_riscv(elf):
    """ELF e_machine 243 = RISC-V (tasmota32c3-plugin: C3/C6), 94 = Xtensa"""
    with open(elf, "rb") as f:
        return int.from_bytes(f.read(20)[18:20], "little") == 243
START = bytes([0x4A, 0xFC, 0xAA, 0x55])
HEADER_SIZE = 0x40          # sizeof(FLASH_MODULE) without the ms[] tail, module_defines.h
END = bytes([0x55, 0xAA, 0xFC, 0x4A])


def objdump():
    for p in OBJDUMP_CANDIDATES:
        if os.path.isfile(p):
            return p
    sys.exit("xtensa objdump not found (PlatformIO toolchain-xtensa-esp-elf)")


def sections(od, elf, nobits=False):
    """[(name, vma, size, file_offset)] of loaded sections with contents
    (nobits=True: the allocated sections WITHOUT contents instead, e.g. .bss)"""
    out = []
    lines = subprocess.run([od, "-h", elf], capture_output=True, text=True).stdout.splitlines()
    for i, l in enumerate(lines):
        m = re.match(r"\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)", l)
        if m and i + 1 < len(lines) and "ALLOC" in lines[i + 1] and ("CONTENTS" in lines[i + 1]) != nobits:
            out.append((m.group(1), int(m.group(3), 16), int(m.group(2), 16), int(m.group(4), 16)))
    return out


def find_module(data, secs, want):
    """(name, start_vma, end_vma) of the module bracket"""
    for name, vma, size, off in secs:
        blob = data[off:off + size]
        i = 0
        while (i := blob.find(START, i)) >= 0:
            if i % 4 == 0 and i + 32 <= len(blob):
                arch = int.from_bytes(blob[i + 4:i + 8], "little") & 0xF
                typ = int.from_bytes(blob[i + 8:i + 12], "little")
                mname = blob[i + 16:i + 32].split(b"\0")[0].decode("ascii", "replace")
                if arch in (1, 2, 3) and typ <= 4 and (not want or mname == want):   # ESP32, RISC-V, P4
                    j = blob.find(END, i + 32)
                    while j >= 0 and j % 4:
                        j = blob.find(END, j + 1)
                    if j >= 0:
                        return mname, vma + i, vma + j + 4
            i += 1
    return None


def reader(data, secs):
    def word(addr):
        for _, vma, size, off in secs:
            if vma <= addr < vma + size - 3:
                return int.from_bytes(data[off + addr - vma:off + addr - vma + 4], "little")
        return None
    return word


def in_any(addr, secs):
    return any(vma <= addr < vma + size for _, vma, size, _ in secs)


def section_of(addr, secs):
    for name, vma, size, _ in secs:
        if vma <= addr < vma + size:
            return name
    return "?"


def symbols(od, elf):
    """sorted [(addr, name)] from the ELF symbol table"""
    nm = od.replace("objdump", "nm")
    out = []
    for l in subprocess.run([nm, "-n", "-C", elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split(None, 2)
        if len(p) == 3 and re.fullmatch(r"[0-9a-f]{8}", p[0]):
            out.append((int(p[0], 16), p[2]))
    return out


def audit_bin(od, path):
    """Audit a raw extracted module: l32r targets and literal values vs. mod_start_org."""
    b = open(path, "rb").read()
    size = int.from_bytes(b[40:44], "little")
    base = int.from_bytes(b[56:60], "little")
    name = b[16:32].split(b"\0")[0].decode("ascii", "replace")
    s, e = base, base + len(b)
    print(f"module {name}: {path} ({len(b)} bytes, header size {size}), linked at 0x{base:08x}")
    dis = subprocess.run([od, "-D", "-b", "binary", "-m", "xtensa", f"--adjust-vma=0x{base:x}",
                          "--no-show-raw-insn", path], capture_output=True, text=True).stdout.splitlines()
    # A linear sweep falls out of step at padding and data and invents l32r loads.
    # Decode per function instead: windowed-ABI functions start with `entry a1, N`
    # (bytes 36 x1) on a 4-byte boundary — decode from each such start to the next.
    cands = [o for o in range(HEADER_SIZE, len(b) - 2, 4) if b[o] == 0x36 and (b[o + 1] & 0x0F) == 1]
    def decode(c, stop):
        out = subprocess.run([od, "-D", "-b", "binary", "-m", "xtensa", f"--adjust-vma=0x{base:x}",
                              f"--start-address=0x{base + c:x}", f"--stop-address=0x{base + stop:x}",
                              "--no-show-raw-insn", path], capture_output=True, text=True).stdout
        rows = [l for l in out.splitlines() if re.match(r"\s*[0-9a-f]+:\t", l) and len(l.split("\t")) > 1
                and l.split("\t")[1].strip()]
        ins = [l.split("\t")[1].split()[0] for l in rows]
        ends = [k for k, x in enumerate(ins) if x in ("retw", "retw.n", "ret", "ret.n", "j", "jx", "return")]
        clean = bool(ends) and not any(x in ("ill", "ill.n", "(bad)") or x.startswith(".")
                                       for x in ins[:ends[-1] + 1])
        return clean, rows

    loads = []
    i = 0
    while i < len(cands):
        c = cands[i]
        # A constant table (PROGMEM data in the module) can hold the entry pattern too;
        # decoded it gives invalid opcodes and no return -> dropped. A false pattern
        # INSIDE a function cuts it short (no return yet) -> extend to the next start.
        # Junk is allowed only as padding after the last return.
        j = i + 1
        ok, rows = decode(c, cands[j] if j < len(cands) else len(b))
        while not ok and j < len(cands) and j - i < 8:
            j += 1
            ok, rows = decode(c, cands[j] if j < len(cands) else len(b))
        if ok:
            for l in rows:
                m = re.match(r"\s*([0-9a-f]+):\s+l32r\s+\S+,\s*0x([0-9a-f]+)", l) or \
                    re.match(r"\s*([0-9a-f]+):\s+l32r\s+\S+\s+([0-9a-f]{8})", l)
                if m and int(m.group(2), 16) % 4 == 0:
                    loads.append((int(m.group(1), 16), int(m.group(2), 16)))
            i = j
        else:
            i += 1
    # Layout (patch_linker_file.py): header, strings, literal pool, code. l32r only
    # reaches backwards, so no load targets anything past the pool: code starts after
    # the highest in-module target, and "functions" found before it are data.
    inside = [t for _, t in loads if s + HEADER_SIZE <= t < e]
    code = (max(inside) + 4) if inside else s + HEADER_SIZE
    print(f"code starts at +0x{code - s:x}, {sum(1 for c in cands if base + c >= code)} functions")
    found = defaultdict(list)
    for at, lit in loads:
        if at < code:
            continue
        if s <= lit < s + HEADER_SIZE:
            found["HEADER"].append((hex(at), f"header+{lit - s}"))
        elif not (s <= lit < e):
            found["LIT-OUT"].append((hex(at), f"literal at 0x{lit:08x}"))
        else:
            v = int.from_bytes(b[lit - s:lit - s + 4], "little")
            if s <= v < e:
                found["PTR-SELF"].append((hex(at), f"0x{v:08x}"))
            elif 0x40000000 <= v < 0x40070000:
                found["ROM"].append((hex(at), f"0x{v:08x}"))
            elif 0x3F000000 <= v < 0x40400000:
                found["PTR-ABS"].append((hex(at), f"0x{v:08x}"))
            else:
                found["CONST"].append((hex(at), f"0x{v:08x}"))
    for kind in ("LIT-OUT", "PTR-ABS", "ROM", "PTR-SELF", "CONST", "HEADER"):
        hits = found[kind]
        print(f"{kind:9s} {len(hits):5d}")
        for at, t in hits[:40] if kind in ("LIT-OUT", "PTR-ABS", "ROM", "CONST") else []:
            print(f"            at {at}: {t}")
    bad = len(found["LIT-OUT"]) + len(found["PTR-ABS"]) + len(found["ROM"])
    print(f"\n=> {bad} certain problems, {len(found['PTR-SELF'])} self-pointers to review")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", default=os.path.join(REPO, ".pio/build/tasmota32-plugin/firmware.elf"))
    ap.add_argument("--name", default="", help="module name (default: first module found)")
    ap.add_argument("--all", action="store_true", help="list every finding, not only a summary")
    ap.add_argument("--bin", help="audit an extracted module file instead of the ELF")
    a = ap.parse_args()

    od = objdump()
    if a.bin:
        return audit_bin(od, a.bin)
    rv = is_riscv(a.elf)
    if rv:
        od = RISCV_OBJDUMP
    data = open(a.elf, "rb").read()
    secs = sections(od, a.elf)
    mod = find_module(data, secs, a.name)
    if not mod:
        sys.exit("no module bracket found in " + a.elf)
    mname, s, e = mod
    word = reader(data, secs)
    bss = sections(od, a.elf, nobits=True)
    syms = symbols(od, a.elf)
    sym_addr = [x for x, _ in syms]
    sym_set = set(sym_addr)

    def sym(v):
        i = bisect.bisect_right(sym_addr, v) - 1
        if i < 0:
            return hex(v)
        base, name = syms[i]
        return name if v == base else f"{name}+0x{v - base:x}"
    print(f"module {mname}: 0x{s:08x}..0x{e:08x} ({e - s} bytes)")

    dis = subprocess.run([od, "-d", "--no-show-raw-insn", f"--start-address=0x{s:x}", f"--stop-address=0x{e:x}", a.elf],
                         capture_output=True, text=True).stdout.splitlines()
    func = "?"
    found = defaultdict(list)          # kind -> [(func, text)]
    callout = Counter()
    for l in dis:
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", l)
        if m:
            func = m.group(2)
            continue
        m = re.match(r"\s*([0-9a-f]+):\s+(\S+)\s+(.*)$", l)
        if not m:
            continue
        op, args = m.group(2), m.group(3)
        if int(m.group(1), 16) >= e - 4:
            continue                               # the end marker 55 AA FC 4A, not code
        tgt = re.search(r"\b([0-9a-f]{8})\b(?: <([^>]+)>)?", args)
        if rv:
            # RISC-V: no literal pools. Calls are pc-relative jal (target shown)
            # or auipc+jalr (target in the "# addr <sym>" comment); data addresses
            # come from lui+addi/load/store, also resolved in that comment.
            cm = re.search(r"#\s*([0-9a-f]+)(?:\s*<([^>]+)>)?", args)
            if (op in ("jal", "j") or (op.startswith("b") and op not in ("bseti", "bclri"))) and tgt and not cm:
                t, name = int(tgt.group(1), 16), tgt.group(2) or tgt.group(1)
            elif op == "jalr" and cm:
                t, name = int(cm.group(1), 16), cm.group(2) or cm.group(1)
            else:
                t = None
            if t is not None:
                if not (s <= t < e):
                    kind = "ROM" if 0x40000000 <= t < 0x40070000 else "CALL-OUT"
                    found[kind].append((func, f"{op} {name}"))
                    if kind == "CALL-OUT":
                        callout[re.sub(r"\+0x[0-9a-f]+$", "", name)] += 1
                continue
            if not cm:
                continue
            v = int(cm.group(1), 16)
            if s <= v < s + HEADER_SIZE:
                found["HEADER"].append((func, f"header+{v - s}"))
            elif s <= v < e:
                found["PTR-SELF"].append((func, f"0x{v:08x}"))
            elif 0x40000000 <= v < 0x40070000:
                found["ROM"].append((func, sym(v)))
            elif v >= 0x3C000000 and in_any(v, bss):
                found["PTR-BSS"].append((func, f"{section_of(v, bss)}: {sym(v)}"))
            elif v >= 0x3C000000 and in_any(v, secs):
                found["PTR-FW"].append((func, f"{section_of(v, secs)}: {sym(v)}"))
            continue
        if op in ("call0", "call4", "call8", "call12", "j") and tgt:
            t = int(tgt.group(1), 16)
            if not (s <= t < e):
                name = tgt.group(2) or hex(t)
                found["CALL-OUT"].append((func, f"{op} {name}"))
                callout[re.sub(r"\+0x[0-9a-f]+$", "", name)] += 1
        elif op == "l32r" and tgt:
            lit = int(tgt.group(1), 16)
            if s <= lit < s + HEADER_SIZE:
                found["HEADER"].append((func, f"header+{lit - s}"))
                continue
            if not (s <= lit < e):
                v = word(lit)
                found["LIT-OUT"].append((func, f"literal at 0x{lit:08x} = " + (f"0x{v:08x}" if v is not None else "?")))
                continue
            v = word(lit)
            if v is None:
                continue
            code = (0x40000000 <= v < 0x40070000) or \
                   (v >= 0x40070000 and in_any(v, secs) and re.search(r"text|iram|vector", section_of(v, secs)))
            if s <= v < e:
                found["PTR-SELF"].append((func, f"0x{v:08x}"))
            elif code and v not in sym_set:
                found["NUM?"].append((func, f"0x{v:08x} (~{sym(v)})"))
            elif 0x40000000 <= v < 0x40070000:
                found["ROM"].append((func, sym(v)))
            elif v >= 0x3F000000 and in_any(v, bss):
                found["PTR-BSS"].append((func, f"{section_of(v, bss)}: {sym(v)}"))
            elif v >= 0x3F000000 and in_any(v, secs):
                found["PTR-FW"].append((func, f"{section_of(v, secs)}: {sym(v)}"))
            else:
                found["CONST"].append((func, f"0x{v:08x}"))

    print()
    for kind in ("CALL-OUT", "LIT-OUT", "PTR-FW", "PTR-BSS", "ROM", "CONST", "PTR-SELF", "NUM?", "HEADER"):
        hits = found[kind]
        per = Counter(f for f, _ in hits)
        print(f"{kind:9s} {len(hits):5d}  in {len(per)} functions")
        if a.all:
            for f, t in hits:
                print(f"            {f}: {t}")
        elif hits:
            for f, n in per.most_common(6):
                print(f"            {n:4d}  {f}")
    fw = Counter(t for _, t in found["PTR-FW"])
    if fw:
        per_sec = Counter(t.split(":")[0] for _, t in found["PTR-FW"])
        print("\nPTR-FW by section:", dict(per_sec))
        print("PTR-FW most frequent targets:")
        for t, c in fw.most_common(30):
            print(f"  {c:4d}  {t}")
    lo = Counter(section_of(int(t.split()[2], 16), secs) for _, t in found["LIT-OUT"])
    if lo:
        print("\nLIT-OUT literal slots by section:", dict(lo))
    if callout:
        print("\ncalls leaving the module, by target:")
        for n, c in callout.most_common(25):
            print(f"  {c:4d}  {n}")
    bss_t = Counter(t for _, t in found["PTR-BSS"])
    if bss_t:
        print("\nPTR-BSS targets:")
        for t, c in bss_t.most_common(15):
            print(f"  {c:4d}  {t}")
    bad = len(found["CALL-OUT"]) + len(found["LIT-OUT"]) + len(found["PTR-FW"]) + len(found["PTR-BSS"]) + len(found["ROM"])
    print(f"\n=> {bad} certain problems, {len(found['PTR-SELF'])} self-pointers to review")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
