#!/usr/bin/python3
"""Scan release binaries for WLAN credentials from user_config_override.h.
Prints only file names and which kind of secret matched, never the values.

    creds_scan.py <user_config_override.h> <dir or file> ...   -> exit 1 on a hit

Why: the factory images embed the safeboot partition, and post_esp32.py takes it
from variants/tasmota/<chip>-safeboot.bin -- a LOCALLY built safeboot carries the
home WLAN credentials from user_config_override.h. From 1.6.46 to 1.6.68 the
tinyc32-4M-*/tinyc32s3 factory images shipped them (found 27.09.2026)."""
import gzip, re, sys, pathlib
hdr = pathlib.Path(sys.argv[1]).read_text(errors="replace")
secrets = []
for m in re.finditer(r'#define\s+(STA_SSID\d|STA_PASS\d)\s+"([^"]+)"', hdr):
    if len(m.group(2)) >= 4:
        secrets.append((m.group(1), m.group(2).encode()))
print(f"{len(secrets)} non-empty credential strings to look for")
# arguments after the header: directories (their .bin/.gz) or single files
files = []
for arg in sys.argv[2:]:
    p = pathlib.Path(arg)
    files += sorted(q for q in p.iterdir() if q.is_file() and q.suffix in (".bin", ".gz")) if p.is_dir() else [p]
bad = 0
for f in files:
    if not f.is_file():
        continue
    data = f.read_bytes()
    if f.suffix == ".gz":
        try: data = gzip.decompress(data)
        except Exception: pass
    hits = sorted({k for k, v in secrets if v in data})
    print(f"{'!! ' + ','.join(hits) if hits else 'sauber':>24}  {f.name}")
    bad += bool(hits)
sys.exit(1 if bad else 0)
