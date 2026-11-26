#!/usr/bin/env python3
"""pre-push hook: refuse a push that would publish a secret.

Searches every object the push sends that no remote has yet (the new commits, not the whole history) for strings that must never become
public. Where the strings come from (each source is optional, a missing one is skipped):

  1. WLAN credentials: STA_SSIDn / STA_PASSn from tasmota/user_config_override.h (gitignored; another file with $CREDS_HEADER).
     Passwords from 4 characters, SSIDs from 6 (a 4-letter SSID is an ordinary word and would match third-party sources).
  2. A personal list, one entry per line, in the first of: $SECRETS_LIST, <main repo>/.git/secrets.txt (also found from a linked worktree; inside .git: it can never be committed),
     ~/.tinyc_secrets. Format:      label|value      (the label is what the report shows; without a '|' the line number is the label)
     Lines starting with '#' and blank lines are ignored, entries shorter than 4 characters are ignored. Matching is case-insensitive
     (a MAC address written in capitals or small letters is found either way). Put in meter numbers, gateway IDs, MAC / IP addresses,
     e-mail addresses, tokens: everything that is yours and does not belong in the repository.

The report names the object path and the LABEL of the match, never a value. Bypass only knowingly: git push --no-verify.
Install with scripts/install_pre_push.sh (see scripts/PRE_PUSH_SECRETS.md)."""
import os, re, subprocess, sys

def git(*args, **kw):
    """Run git; a failing call stops the push - a hook that cannot look must not wave the push through (26.11.2026, Andreas)."""
    r = subprocess.run(["git", *args], capture_output=True, **kw)
    if r.returncode != 0:
        err = (r.stderr.decode(errors="replace") if isinstance(r.stderr, bytes) else (r.stderr or "")).strip()
        print(f"pre-push: 'git {' '.join(args[:2])}' failed ({err or 'no message'}) - cannot check, push REFUSED", file=sys.stderr)
        print("pre-push: (a drive without ownership info needs 'git config --global --add safe.directory <folder>'; or push knowingly with --no-verify)", file=sys.stderr)
        sys.exit(1)
    return r

top = git("rev-parse", "--show-toplevel").stdout.decode().strip()
# --git-common-dir, not --absolute-git-dir: in a linked worktree the latter is .git/worktrees/<name>, but the list and the main
# working folder live with the main repository.
commondir = os.path.abspath(os.path.join(top, git("rev-parse", "--git-common-dir").stdout.decode().strip()))
gitdir = commondir
mainroot = os.path.dirname(commondir) if os.path.basename(commondir) == ".git" else top
secrets = []          # (label, lowercase bytes)

# 1. WLAN credentials
hdr_path = os.environ.get("CREDS_HEADER")
if not hdr_path:
    # the header is gitignored: in a linked worktree it only exists in the main working folder
    for root in (top, mainroot):
        cand = os.path.join(root, "tasmota", "user_config_override.h")
        if os.path.isfile(cand):
            hdr_path = cand
            break
try:
    hdr = open(hdr_path, errors="replace").read()
    for m in re.finditer(r'#define\s+(STA_SSID\d|STA_PASS\d)\s+"([^"]+)"', hdr):
        kind, val = m.group(1), m.group(2)
        if len(val) >= (4 if "PASS" in kind else 6):
            secrets.append((kind, val.lower().encode()))
except (OSError, TypeError):
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
objs = git("rev-list", "--objects", *tips, "--not", f"--remotes={remote}", text=True).stdout.splitlines()
paths = {}
for line in objs:
    sha, _, path = line.partition(" ")
    if path:
        paths[sha] = path
if not paths:
    sys.exit(0)
r = subprocess.run(["git", "cat-file", "--batch"], input="\n".join(paths).encode() + b"\n", capture_output=True)
if r.returncode != 0:
    print("pre-push: 'git cat-file' failed - cannot check, push REFUSED", file=sys.stderr)
    sys.exit(1)
batch = r.stdout
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
