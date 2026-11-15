#!/usr/bin/env python3
"""pre-push hook: refuse a push that would publish a secret.

Searches every object the push sends that no remote has yet (the new commits, not the whole history) for strings that must never become
public. Where the strings come from (each source is optional, a missing one is skipped):

  1. WLAN credentials: STA_SSIDn / STA_PASSn from tasmota/user_config_override.h (gitignored; another file with $CREDS_HEADER).
     Passwords from 4 characters, SSIDs from 6 (a 4-letter SSID is an ordinary word and would match third-party sources).
  2. A personal list, one entry per line, in the first of: $SECRETS_LIST, <repo>/.git/secrets.txt (inside .git: it can never be committed),
     ~/.tinyc_secrets. Format:      label|value      (the label is what the report shows; without a '|' the line number is the label)
     Lines starting with '#' and blank lines are ignored, entries shorter than 4 characters are ignored. Matching is case-insensitive
     (a MAC address written in capitals or small letters is found either way). Put in meter numbers, gateway IDs, MAC / IP addresses,
     e-mail addresses, tokens: everything that is yours and does not belong in the repository.

The report names the object path and the LABEL of the match, never a value. Bypass only knowingly: git push --no-verify.
Install with scripts/install_pre_push.sh (see scripts/PRE_PUSH_SECRETS.md)."""
import os, re, subprocess, sys

top = subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True).stdout.strip()
gitdir = subprocess.run(["git", "rev-parse", "--absolute-git-dir"], capture_output=True, text=True).stdout.strip()
secrets = []          # (label, lowercase bytes)

# 1. WLAN credentials
hdr_path = os.environ.get("CREDS_HEADER") or os.path.join(top, "tasmota", "user_config_override.h")
try:
    hdr = open(hdr_path, errors="replace").read()
    for m in re.finditer(r'#define\s+(STA_SSID\d|STA_PASS\d)\s+"([^"]+)"', hdr):
        kind, val = m.group(1), m.group(2)
        if len(val) >= (4 if "PASS" in kind else 6):
            secrets.append((kind, val.lower().encode()))
except OSError:
    pass

# 2. personal list
list_path = None
for cand in (os.environ.get("SECRETS_LIST"), os.path.join(gitdir, "secrets.txt"), os.path.expanduser("~/.tinyc_secrets")):
    if cand and os.path.isfile(cand):
        list_path = cand
        break
if list_path:
    for n, line in enumerate(open(list_path, errors="replace").read().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        label, sep, val = line.partition("|")
        if not sep:
            label, val = f"line {n}", line
        val = val.strip()
        if len(val) >= 4:
            secrets.append((label.strip() or f"line {n}", val.lower().encode()))

if not secrets:
    print("pre-push: no secrets to look for (no user_config_override.h and no list) - check skipped", file=sys.stderr)
    sys.exit(0)

remote = sys.argv[1] if len(sys.argv) > 1 else "origin"
tips = [l.split()[1] for l in sys.stdin.read().splitlines() if l.split() and not set(l.split()[1]) <= {"0"}]
if not tips:
    sys.exit(0)
objs = subprocess.run(["git", "rev-list", "--objects", *tips, "--not", f"--remotes={remote}"],
                      capture_output=True, text=True).stdout.splitlines()
paths = {}
for line in objs:
    sha, _, path = line.partition(" ")
    if path:
        paths[sha] = path
if not paths:
    sys.exit(0)
batch = subprocess.run(["git", "cat-file", "--batch"], input="\n".join(paths).encode() + b"\n", capture_output=True).stdout
hits, pos = [], 0
while pos < len(batch):
    nl = batch.index(b"\n", pos)
    sha, typ, size = batch[pos:nl].split()[:3]
    size = int(size)
    data = batch[nl + 1:nl + 1 + size]
    pos = nl + 1 + size + 1
    if typ == b"blob":
        low = data.lower()
        found = sorted({k for k, v in secrets if v in low})
        if found:
            hits.append((paths[sha.decode()], found))
if hits:
    print("pre-push: secrets in the objects being pushed - push REFUSED:", file=sys.stderr)
    for p, k in hits:
        print(f"    {p}  ({', '.join(k)})", file=sys.stderr)
    print("pre-push: remove them (and from the commit), or push knowingly with --no-verify", file=sys.stderr)
    sys.exit(1)
print(f"pre-push: {len(paths)} object(s) checked, {len(secrets)} secret string(s), nothing found", file=sys.stderr)
