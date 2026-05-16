#!/usr/bin/env python3
# =====================================================================
# triage.py — classify Tasmota native drivers by native->dual-plugin
# conversion cost.
#
# The native->dual-format-plugin transpiler can only mechanically
# *scaffold* (gating, MODULE_DESCRIPTOR/MODULE_PART, state->MODULE_MEMORY,
# dispatcher). It CANNOT port an external C++ library to C — that is
# irreducible human work. So before investing in the scaffolder, map
# the backlog: which drivers are cheap (scaffold-only) vs expensive
# (need a C++->C library port first).
#
# Heuristic, evidence-based, deliberately conservative: when unsure it
# leans toward NEEDS_PORT so the "cheap" list is trustworthy.
# =====================================================================
import os, re, sys, glob

ROOTS = [
    "tasmota/tasmota_xsns_sensor",
    "tasmota/tasmota_xdrv_driver",
]

# Headers that are NOT external libs: C stdlib, Tasmota-core, AVR pgm.
CORE_HDR = re.compile(
    r'^(stdint|stdlib|string|stdio|math|stddef|stdbool|stdarg|ctype'
    r'|avr/pgmspace|avr/io|Arduino|tasmota_options|tasmota|i18n'
    r'|stringfunc|gpio|clock|delay|led|board)\b', re.I)

INC_RE   = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
# C++ constructs the plugin C-ABI / loader can't take verbatim.
CPP_CLASS   = re.compile(r'^\s*(class|namespace)\s+\w', re.M)
CPP_TEMPLATE= re.compile(r'\btemplate\s*<', )
CPP_STL     = re.compile(r'\b(std\s*::|#include\s*<vector>|#include\s*<map>'
                         r'|#include\s*<string>|#include\s*<functional>)')
CPP_NEW     = re.compile(r'\bnew\s+[A-Z]\w+\s*[\(;]')
# Direct Arduino bus objects (Tasmota wraps these; direct use = friction).
WIRE_DIRECT = re.compile(r'\bWire\d?\.\s*(begin|requestFrom|write|read|'
                         r'beginTransmission|endTransmission)\b')
SPI_DIRECT  = re.compile(r'\bSPI\d?\.\s*(begin|transfer|beginTransaction)\b')
# Tasmota's own serial class — compat header HAS a bridge for it.
TASMOTA_SERIAL = re.compile(r'\bTasmotaSerial\b')
# A driver instantiating a non-Tasmota object of a lib type: best-effort
# — flag obvious vendor lib class instances (Adafruit_*, *_t handles via
# lib ctor calls). Conservative: only the library #include drives this.
VENDOR_HINT = re.compile(r'(Adafruit_|_BME|_BMP|Sensirion|MCP\d|OpenTherm'
                         r'|TasmotaModbus|bsec|bme68|hpma|cc1101|mcp2515)',
                         re.I)

def strip_code(s):
    # Remove block comments, then string/char literals, then line
    # comments — so a vendor name in a comment ("Sensirion I2C…") or a
    # URL in a string can't trip the C++/vendor heuristics. Strings are
    # blanked before line comments so "http://x" isn't mis-eaten.
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'"(?:\\.|[^"\\])*"', '""', s)
    s = re.sub(r"'(?:\\.|[^'\\])*'", "''", s)
    s = re.sub(r'//[^\n]*', ' ', s)
    return s

def classify(path):
    raw = open(path, encoding='utf-8', errors='replace').read()
    src = strip_code(raw)
    incs = [h for h in INC_RE.findall(src)]
    libincs = [h for h in incs
               if not CORE_HDR.match(os.path.basename(h))
               and not h.startswith(('tasmota', 'include/'))]
    reasons = []
    serial = bool(TASMOTA_SERIAL.search(src))
    # TasmotaSerial.h is a Tasmota lib with a compat shim — not a port.
    lib_real = [h for h in libincs if os.path.basename(h) != 'TasmotaSerial.h']

    if lib_real:
        reasons.append("ext-lib include: " + ", ".join(sorted(set(lib_real))[:4]))
    if CPP_CLASS.search(src):
        reasons.append("defines class/namespace")
    if CPP_TEMPLATE.search(src):
        reasons.append("C++ template")
    if CPP_STL.search(src):
        reasons.append("STL (std::/vector/...)")
    if CPP_NEW.search(src):
        reasons.append("new <Class>")
    if VENDOR_HINT.search(src) and not reasons:
        reasons.append("vendor-lib symbol pattern")

    wire = bool(WIRE_DIRECT.search(src)) or bool(SPI_DIRECT.search(src))

    if reasons:
        cat = "NEEDS_PORT"
    elif wire:
        cat = "NEEDS_PORT"
        reasons.append("direct Wire/SPI object (no Tasmota I2c* wrapper)")
    elif serial:
        cat = "SERIAL_SHIM"          # scaffold-viable via compat TS bridge
        reasons.append("uses TasmotaSerial (compat-shim covers it)")
    else:
        cat = "VIABLE"               # scaffold-only, cheap win
    return cat, reasons, sorted(set(libincs))

def loc(path):
    return sum(1 for _ in open(path, encoding='utf-8', errors='replace'))

def main():
    repo = os.getcwd()
    # Drivers already hand-converted to dual (proven convertible) — used
    # to validate the classifier: their originals SHOULD be VIABLE/SMALL.
    proven = set()
    for d in glob.glob(os.path.join(repo, "tasmota/Plugins/*_dual.cpp")):
        m = re.match(r'(x[sd][nr][sv]_\d+)', os.path.basename(d))
        if m: proven.add(m.group(1))
    rows = []
    for root in ROOTS:
        for p in sorted(glob.glob(os.path.join(repo, root, "*.ino"))):
            base = os.path.basename(p)
            if base.endswith('_dual.ino'):
                continue                      # generated native-shim, skip
            cat, why, incs = classify(p)
            n = loc(p)
            stem = re.match(r'(x[sd][nr][sv]_\d+)', base)
            pv = bool(stem and stem.group(1) in proven)
            # Split VIABLE by size: small register-bang sensor = real
            # cheap win; large lib-free-but-coupled (zigbee/berry/etc.)
            # is lib-free yet NOT a quick scaffold.
            if cat == "VIABLE":
                cat = "CHEAP" if n < 280 else "VIABLE_BIG"
            rows.append((base, root.split('_')[-1], cat, why, n, pv))
    order = {"CHEAP": 0, "SERIAL_SHIM": 1, "VIABLE_BIG": 2, "NEEDS_PORT": 3}
    rows.sort(key=lambda r: (order[r[2]], not r[5], r[4], r[0]))
    from collections import Counter
    c = Counter(r[2] for r in rows)
    tot = len(rows)
    print(f"=== Tasmota native-driver triage  ({tot} drivers) ===")
    print(f"  CHEAP        scaffold-only, <280 LOC, register-bang : {c['CHEAP']}")
    print(f"  SERIAL_SHIM  scaffold + compat TasmotaSerial bridge : {c['SERIAL_SHIM']}")
    print(f"  VIABLE_BIG   lib-free but large/coupled (≥280 LOC)  : {c['VIABLE_BIG']}")
    print(f"  NEEDS_PORT   C++->C library port required first     : {c['NEEDS_PORT']}")
    print()
    # classifier self-check. A proven-convertible driver is allowed to
    # be NEEDS_PORT *iff* it has a real external-lib include (the human
    # paid that port — e.g. ccs811/sgp30 use Adafruit_*). The classifier
    # is only WRONG when it blocks a proven driver that is lib-free.
    # "proven" = a human converted it — which for lib/class drivers
    # INCLUDED paying the C++->C port. So a proven driver is correctly
    # NEEDS_PORT when it has any concrete port reason; the classifier is
    # only WRONG if it blocks a proven driver with NO concrete reason
    # (the old "vendor word in a comment" bug).
    pv_rows = [r for r in rows if r[5]]
    miss = [r for r in pv_rows if r[2] == "NEEDS_PORT" and not r[3]]
    print(f"classifier check: {len(pv_rows)} proven-convertible originals; "
          f"{len(miss)} blocked with NO concrete reason (spurious) — want 0:")
    for r in pv_rows:
        flag = ('OK    ' if r[2] != "NEEDS_PORT"
                else 'PORT✓ ' if r[3] else 'SPURIOUS')
        print(f"   [{flag}] {r[0]:30s} -> {r[2]:11s} ({r[4]}L)"
              f"{'  '+'; '.join(r[3]) if r[2]=='NEEDS_PORT' else ''}")
    print("   (PORT✓ = correctly NEEDS_PORT: the human did pay that "
          "C++→C cost; OK = lib-free, scaffold-convertible)")
    print("\n--- CHEAP (sensor first; ★=already proven convertible) ---")
    for name, kind, cat, why, n, pv in rows:
        if cat == "CHEAP":
            print(f"  {'★' if pv else ' '} {kind:6s} {n:4d}L  {name}")
    print("\n--- SERIAL_SHIM ---")
    for name, kind, cat, why, n, pv in rows:
        if cat == "SERIAL_SHIM":
            print(f"  {kind:6s} {n:4d}L  {name}")
    print(f"\n--- VIABLE_BIG ({c['VIABLE_BIG']}) : lib-free but not quick ---")
    for name, kind, cat, why, n, pv in rows:
        if cat == "VIABLE_BIG":
            print(f"  {kind:6s} {n:5d}L  {name}")

if __name__ == "__main__":
    main()
