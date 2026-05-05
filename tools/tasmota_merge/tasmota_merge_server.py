#!/usr/bin/env python3
"""
Tasmota Merge — fork-vs-upstream side-by-side merge helper.

Downloads a fresh upstream Tasmota ZIP (default: arendst/Tasmota
development branch), unpacks it to a cache dir, diffs every tracked
source file against the local fork, and serves a browser UI for
file-by-file decisions:

  • Keep mine          (fork wins, do nothing)
  • Take theirs        (overwrite fork file with upstream)
  • Custom merge       (write a hand-edited blob)
  • Mark resolved      (record decision without writing — for review later)

Decisions live in `<fork>/.tasmota_merge_state.json` so the session is
resumable. Apply step writes files to the fork tree but does NOT auto-
commit — the user reviews via `git diff` and commits manually.

Stdlib-only (urllib, zipfile, difflib, http.server). No pip required.

Usage:    python3 tasmota_merge_server.py [--port 8500] [--fork PATH]
"""

import os, sys, json, threading, time, hashlib, shutil, tempfile
import subprocess, webbrowser, zipfile, difflib, urllib.request, urllib.parse
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

# ── Configuration ─────────────────────────────────────────────────────────────

HTTP_PORT          = 8500
HTML_FILE          = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  'tasmota_merge.html')

DEFAULT_UPSTREAM   = 'https://github.com/arendst/Tasmota/archive/refs/heads/development.zip'

# Files inside the upstream ZIP top-level dir that we DO compare. Anything
# outside these prefixes is ignored (binaries, build outputs, GitHub workflows
# we don't care about). Tweak in setup if needed.
DEFAULT_INCLUDE_PREFIXES = [
    'tasmota/',
    'lib/',
    'platformio.ini',
    'platformio_override_sample.ini',
    'pio-tools/',
    'boards/',
    'tools/',
]

# Patterns to skip even when inside an included prefix.
DEFAULT_EXCLUDE_GLOBS = [
    '*.tcb',           # TinyC bytecode (not source-of-truth)
    '*.bin',
    '*.elf',
    '*.o',
    '*.a',
    '*.zip',
    '*.gz',
    '*.png',
    '*.jpg',
    '*.icns',
    '*.pdf',
    '__pycache__/*',
    '.git/*',
    '.pioenvs/*',
    '.pio/*',
    'build_output/*',
    # macOS / editor noise (no slash → basename match at any depth)
    '.DS_Store',
    'Thumbs.db',
    '*.swp',
    '*.swo',
    '*~',
    # Tool's own bookkeeping (so the merge tool never lists itself as a diff)
    '.tasmota_merge_state.json',
    '.tasmota_merge_backups/*',
]

# ── Auto-detect fork root ─────────────────────────────────────────────────────

def find_fork_root(start: str) -> str:
    """Walk up from `start` looking for a dir that contains `tasmota/` AND
    `platformio.ini` — that's the fork root."""
    cur = os.path.abspath(start)
    while cur and cur != os.path.dirname(cur):
        if (os.path.isdir(os.path.join(cur, 'tasmota'))
                and os.path.isfile(os.path.join(cur, 'platformio.ini'))):
            return cur
        cur = os.path.dirname(cur)
    return ''

DEFAULT_FORK = find_fork_root(os.path.dirname(os.path.abspath(__file__))) or os.path.expanduser('~')

# Cache for downloaded + extracted upstream
CACHE_DIR = os.path.expanduser('~/.tasmota_merge_cache')

# State file lives in the fork root so it survives across sessions
def state_path(fork_root: str) -> str:
    return os.path.join(fork_root, '.tasmota_merge_state.json')

# Policies file — durable, evolves slowly, persists across upstream merges.
# Separate from state file so per-merge manual decisions don't pollute it.
def policies_path(fork_root: str) -> str:
    return os.path.join(fork_root, '.tasmota_merge_policies.json')

# Default seed policy when no file exists yet. Edit on first run by
# clicking "Edit policies" in the UI; saves back to the JSON.
DEFAULT_POLICIES = {
    'always_keep': [
        # ─── TinyC stack (entirely fork-owned) ─────────────────────────
        'tasmota/tinyc/**',
        'tasmota/include/xdrv_124_tinyc_vm.h',
        'tasmota/tasmota_xdrv_driver/xdrv_124_tinyc.ino',
        # ─── Fork-specific docs / bookkeeping ───────────────────────────
        'tasmota/UNSOLVED.md',
        # ─── The merge tool itself ──────────────────────────────────────
        'tools/tasmota_merge/**',
        # ─── Custom display / plugin work ───────────────────────────────
        'lib/lib_display/UDisplay_legacy/**',
        'tasmota/Plugins/xdrv_14_mp3*',
    ],
    'always_take': [
        # ─── Berry — fork doesn't modify it ─────────────────────────────
        'tasmota/berry/**',
    ],
}

# ── Open browser reliably ────────────────────────────────────────────────────

def open_browser_reliable(url):
    try:
        if sys.platform == 'darwin':
            subprocess.Popen(['open', url]); return True
        if sys.platform.startswith('win'):
            os.startfile(url); return True  # type: ignore[attr-defined]
        subprocess.Popen(['xdg-open', url],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except Exception:
        try: return webbrowser.open(url)
        except Exception: return False

# ── Shared session state ──────────────────────────────────────────────────────

state_lock = threading.Lock()

session = {
    'fork_root':        DEFAULT_FORK,
    'upstream_url':     DEFAULT_UPSTREAM,
    'include_prefixes': list(DEFAULT_INCLUDE_PREFIXES),
    'exclude_globs':    list(DEFAULT_EXCLUDE_GLOBS),
    'phase':            'setup',     # setup | downloading | extracting | diffing | ready | applying | done
    'progress_pct':     0,
    'progress_msg':     'Idle.',
    'error':            '',
    'upstream_dir':     '',          # path to extracted upstream tree
    'upstream_zip':     '',          # path to the cached zip
    'upstream_label':   '',          # e.g. "Tasmota-development"
    'files':            [],          # list of file dicts (see _build_diff_index)
    'decisions':        {},          # path -> {'action': ..., 'merged_content': ...}
    'policies':         {'always_keep': [], 'always_take': []},
    'started_at':       0.0,
}

def _set_phase(phase: str, msg: str = '', pct: int = -1):
    with state_lock:
        session['phase'] = phase
        if msg: session['progress_msg'] = msg
        if pct >= 0: session['progress_pct'] = pct
        if phase != 'error':
            session['error'] = ''

def _set_error(msg: str):
    with state_lock:
        session['phase'] = 'error'
        session['error'] = msg
        session['progress_msg'] = msg

# ── Glob match (for excludes) ────────────────────────────────────────────────

import fnmatch
def _matches_any_glob(path: str, globs) -> bool:
    """gitignore-ish matching:

       • pattern with no slash:    match against basename at any depth
                                   (`.DS_Store` matches `lib/x/.DS_Store`)
       • leading `**/`:            match against basename at any depth
       • pattern `name/*` or
         `name/**` (single segm):  match if any path component equals name
                                   (`__pycache__/*` matches at any depth)
       • pattern `a/b/c/**` (multi-
         segment ending in /**):   match anything under that prefix
                                   (`tasmota/tinyc/**` matches
                                    `tasmota/tinyc/examples/foo.tc`)
       • pattern `a/b/c/`:         shorthand for `a/b/c/**` (trailing slash)
       • pattern with other slash: fnmatch against full relative path
    """
    parts = path.split('/')
    base  = parts[-1]
    for raw in globs:
        g = raw.strip()
        if not g or g.startswith('#'): continue
        if g.endswith('/'): g = g + '**'
        if g.startswith('**/'):
            if fnmatch.fnmatch(base, g[3:]): return True
            continue
        if g.endswith('/**'):
            prefix = g[:-3]
            if '/' in prefix:
                # multi-segment: path must start with this prefix
                if path == prefix or path.startswith(prefix + '/'): return True
            else:
                # single-segment: dir name appearing as any path component
                if prefix in parts[:-1]: return True
                if path == prefix: return True
            continue
        if g.endswith('/*'):
            # NOTE: deviating from gitignore here on purpose. Most users
            # mean "everything under this directory" when they type
            # `lib/basic/*`. Strict gitignore would only match direct
            # children. We treat `/*` as `/**` (recursive) — far less
            # surprising for the merge-tool use case.
            prefix = g[:-2]
            if '/' in prefix:
                if path == prefix or path.startswith(prefix + '/'): return True
            else:
                # single-segment dir anywhere
                if prefix in parts[:-1]: return True
                if path == prefix: return True
            continue
        if '/' in g:
            if fnmatch.fnmatch(path, g): return True
        else:
            if fnmatch.fnmatch(base, g): return True
    return False

def _matches_any_prefix(path: str, prefixes) -> bool:
    return any(path == p or path.startswith(p) for p in prefixes)

# ── Download with progress ───────────────────────────────────────────────────

def _download_with_progress(url: str, dest_path: str):
    _set_phase('downloading', f'Connecting to {url}…', 0)
    req = urllib.request.Request(url, headers={'User-Agent': 'TasmotaMerge/1.0'})
    with urllib.request.urlopen(req, timeout=60) as resp:
        total = int(resp.headers.get('Content-Length', 0)) or 0
        chunk = 64 * 1024
        got   = 0
        with open(dest_path, 'wb') as f:
            while True:
                buf = resp.read(chunk)
                if not buf: break
                f.write(buf)
                got += len(buf)
                if total > 0:
                    pct = int(got * 100 / total)
                    _set_phase('downloading',
                               f'Downloaded {got/1e6:.1f} / {total/1e6:.1f} MB',
                               pct)
                else:
                    _set_phase('downloading',
                               f'Downloaded {got/1e6:.1f} MB',
                               -1)
    _set_phase('downloading', f'Download complete ({got/1e6:.1f} MB).', 100)

# ── Extract ZIP to cache, return root dir of extracted tree ──────────────────

def _extract_zip(zip_path: str) -> str:
    _set_phase('extracting', 'Reading ZIP…', 0)
    target_root = os.path.join(CACHE_DIR, 'extracted')
    if os.path.isdir(target_root):
        shutil.rmtree(target_root, ignore_errors=True)
    os.makedirs(target_root, exist_ok=True)

    with zipfile.ZipFile(zip_path, 'r') as zf:
        members = zf.namelist()
        total   = len(members) or 1
        # Top-level dir is e.g. "Tasmota-development/" — find it
        top_dirs = set(m.split('/', 1)[0] for m in members if '/' in m)
        if len(top_dirs) != 1:
            raise RuntimeError(f'Unexpected ZIP layout — top dirs: {sorted(top_dirs)}')
        top = top_dirs.pop()

        for i, m in enumerate(members):
            zf.extract(m, target_root)
            if i % 200 == 0 or i == total - 1:
                _set_phase('extracting',
                           f'Extracted {i+1}/{total} entries',
                           int((i+1) * 100 / total))

    extracted = os.path.join(target_root, top)
    with state_lock:
        session['upstream_dir']   = extracted
        session['upstream_label'] = top
    _set_phase('extracting', f'Extracted to {extracted}', 100)
    return extracted

# ── Build diff index ─────────────────────────────────────────────────────────

_read_failures = []   # list of (path, errno_str) — populated during diff
_tcc_denied    = False

def _hash_file(path: str) -> str:
    """Hash a file's bytes. On read failure, record it (counted in
    /api/status as 'read_failures'); 'Operation not permitted' on
    user-folder files signals macOS TCC sandboxing — we bubble that up
    so the UI can warn instead of silently producing a bogus diff."""
    global _tcc_denied
    try:
        with open(path, 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()
    except PermissionError as e:
        _read_failures.append((path, f'PermissionError: {e}'))
        # macOS TCC denial → errno 1 (EPERM) "Operation not permitted".
        # Plain UNIX permission denial → errno 13 (EACCES) "Permission denied".
        # We treat EPERM as TCC and flag it so the UI can warn.
        if e.errno == 1 or 'not permitted' in str(e).lower():
            _tcc_denied = True
        return ''
    except OSError as e:
        _read_failures.append((path, f'OSError({e.errno}): {e.strerror}'))
        if e.errno == 1:
            _tcc_denied = True
        return ''
    except Exception as e:
        _read_failures.append((path, f'{type(e).__name__}: {e}'))
        return ''

def _is_text_file(path: str) -> bool:
    """Cheap check: if file is small enough and has no NULs in first 4k, text."""
    try:
        with open(path, 'rb') as f:
            head = f.read(4096)
        return b'\x00' not in head
    except Exception:
        return False

def _build_diff_index(fork_root: str, upstream_dir: str,
                      include_prefixes, exclude_globs):
    """Walk upstream tree; for each included file, compare hash with the same
    relative path inside the fork. Yields entries categorized as:
      • 'modified'     — exists both sides, hashes differ
      • 'upstream_new' — exists upstream only (would add to fork)
      • 'fork_only'    — exists in fork only (would not be touched)
      • 'identical'    — present both sides, hashes match (skipped from list)
    """
    _set_phase('diffing', 'Indexing upstream files…', 0)
    upstream_files = {}   # rel_path -> abs upstream path
    for dirpath, _dirs, files in os.walk(upstream_dir):
        for fn in files:
            ap  = os.path.join(dirpath, fn)
            rel = os.path.relpath(ap, upstream_dir).replace(os.sep, '/')
            if not _matches_any_prefix(rel, include_prefixes): continue
            if _matches_any_glob(rel, exclude_globs):           continue
            upstream_files[rel] = ap

    _set_phase('diffing', f'Indexed {len(upstream_files)} upstream files; comparing…', 25)

    entries = []
    total = len(upstream_files) or 1
    for i, (rel, up_abs) in enumerate(sorted(upstream_files.items())):
        fork_abs = os.path.join(fork_root, rel)
        if os.path.isfile(fork_abs):
            up_hash = _hash_file(up_abs)
            fk_hash = _hash_file(fork_abs)
            if up_hash and fk_hash and up_hash == fk_hash:
                continue  # identical — skip
            entries.append({
                'path':     rel,
                'category': 'modified',
                'fork_size':     os.path.getsize(fork_abs),
                'upstream_size': os.path.getsize(up_abs),
                'is_text':       _is_text_file(fork_abs) and _is_text_file(up_abs),
            })
        else:
            entries.append({
                'path':     rel,
                'category': 'upstream_new',
                'fork_size':     -1,
                'upstream_size': os.path.getsize(up_abs),
                'is_text':       _is_text_file(up_abs),
            })
        if i % 100 == 0:
            _set_phase('diffing',
                       f'Compared {i+1}/{total}…',
                       25 + int((i+1) * 70 / total))

    # Optionally surface fork-only files (added by gerhard, not in upstream)
    # Limited to the same prefixes to keep the list focused.
    _set_phase('diffing', 'Scanning fork for fork-only files…', 95)
    fork_only_count = 0
    for prefix in include_prefixes:
        if prefix.endswith('/'):
            walk_root = os.path.join(fork_root, prefix.rstrip('/'))
            if not os.path.isdir(walk_root): continue
            for dirpath, _dirs, files in os.walk(walk_root):
                for fn in files:
                    ap  = os.path.join(dirpath, fn)
                    rel = os.path.relpath(ap, fork_root).replace(os.sep, '/')
                    if _matches_any_glob(rel, exclude_globs):  continue
                    if rel in upstream_files:                  continue
                    entries.append({
                        'path':     rel,
                        'category': 'fork_only',
                        'fork_size':     os.path.getsize(ap),
                        'upstream_size': -1,
                        'is_text':       _is_text_file(ap),
                    })
                    fork_only_count += 1

    entries.sort(key=lambda e: (e['category'], e['path']))
    _set_phase('diffing',
               f'Done. {len(entries)} files differ ({fork_only_count} fork-only).',
               100)
    return entries

# ── State file (resumable decisions) ─────────────────────────────────────────

def _load_state(fork_root: str):
    p = state_path(fork_root)
    if os.path.isfile(p):
        try:
            with open(p, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return data.get('decisions', {}) or {}
        except Exception:
            return {}
    return {}

def _load_policies(fork_root: str) -> dict:
    """Load .tasmota_merge_policies.json, seed it with DEFAULT_POLICIES on
    first run. Returns {'always_keep': [...], 'always_take': [...]}."""
    p = policies_path(fork_root)
    if os.path.isfile(p):
        try:
            with open(p, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return {
                'always_keep': list(data.get('always_keep') or []),
                'always_take': list(data.get('always_take') or []),
            }
        except Exception as e:
            print(f'WARN: bad policies file: {e}', file=sys.stderr)
            return {'always_keep': [], 'always_take': []}
    # First run — seed with sensible defaults so reviews are short out of the box
    seed = {'always_keep': list(DEFAULT_POLICIES['always_keep']),
            'always_take': list(DEFAULT_POLICIES['always_take'])}
    _save_policies(fork_root, seed)
    return seed

def _save_policies(fork_root: str, pol: dict) -> bool:
    p = policies_path(fork_root)
    try:
        out = {
            'saved_at':    time.strftime('%Y-%m-%d %H:%M:%S'),
            'always_keep': list(pol.get('always_keep') or []),
            'always_take': list(pol.get('always_take') or []),
        }
        with open(p, 'w', encoding='utf-8') as f:
            json.dump(out, f, indent=2)
        return True
    except Exception as e:
        print(f'WARN: could not write policies file: {e}', file=sys.stderr)
        return False

def _policy_decision_for(rel_path: str, pol: dict) -> str:
    """Return 'keep', 'take', or '' depending on which (if any) policy
    list this path matches. always_keep wins on conflict (safer default
    — never overwrite fork files when in doubt)."""
    keep = pol.get('always_keep') or []
    take = pol.get('always_take') or []
    if _matches_any_glob(rel_path, keep): return 'keep'
    if _matches_any_glob(rel_path, take): return 'take'
    return ''

def _count_pattern_matches(patterns, files_paths):
    """For each pattern, count how many file paths it matches. Returns
    a list of {pattern, count} in the same order as `patterns`. Used by
    the policies modal to surface typos / dead patterns ("0 matches"
    next to a line tells you instantly the glob is broken)."""
    out = []
    for raw in patterns:
        p = (raw or '').strip()
        if not p or p.startswith('#'):
            out.append({'pattern': raw, 'count': -1})  # -1 = comment/blank
            continue
        n = 0
        for fp in files_paths:
            if _matches_any_glob(fp, [p]):
                n += 1
        out.append({'pattern': raw, 'count': n})
    return out

def _save_state(fork_root: str, decisions: dict, extra: dict = None):
    p = state_path(fork_root)
    data = {
        'saved_at':     time.strftime('%Y-%m-%d %H:%M:%S'),
        'fork_root':    fork_root,
        'upstream_url': session.get('upstream_url', ''),
        'upstream_label': session.get('upstream_label', ''),
        'decisions':    decisions,
    }
    if extra: data.update(extra)
    try:
        with open(p, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)
        return True
    except Exception as e:
        print(f'WARN: could not write state file: {e}', file=sys.stderr)
        return False

# ── The big "start" thread ────────────────────────────────────────────────────

def _start_session(upstream_url: str, fork_root: str,
                   include_prefixes, exclude_globs,
                   skip_download: bool):
    global _read_failures, _tcc_denied
    try:
        with state_lock:
            session['upstream_url']     = upstream_url
            session['fork_root']        = fork_root
            session['include_prefixes'] = list(include_prefixes)
            session['exclude_globs']    = list(exclude_globs)
            session['decisions']        = _load_state(fork_root)
            session['policies']         = _load_policies(fork_root)
            session['error']            = ''
            session['files']            = []
            session['started_at']       = time.time()
            _read_failures              = []
            _tcc_denied                 = False

        os.makedirs(CACHE_DIR, exist_ok=True)
        zip_path = os.path.join(CACHE_DIR, 'upstream.zip')

        # Download (or reuse)
        if skip_download and os.path.isfile(zip_path):
            _set_phase('downloading',
                       f'Reusing cached ZIP ({os.path.getsize(zip_path)/1e6:.1f} MB).',
                       100)
        else:
            _download_with_progress(upstream_url, zip_path)

        with state_lock:
            session['upstream_zip'] = zip_path

        # Extract
        upstream_dir = _extract_zip(zip_path)

        # Diff
        entries = _build_diff_index(fork_root, upstream_dir,
                                    include_prefixes, exclude_globs)

        with state_lock:
            session['files'] = entries
        _set_phase('ready',
                   f'{len(entries)} files differ — review below.',
                   100)
    except Exception as e:
        import traceback; traceback.print_exc()
        _set_error(f'{type(e).__name__}: {e}')

# ── Apply decisions ──────────────────────────────────────────────────────────

def _apply_decisions():
    """Walk the decisions map and write upstream/merged content to the fork.

    Actions:
      • 'keep'    → no write
      • 'take'    → copy upstream file over fork file
      • 'merge'   → write decision['merged_content'] to fork file
      • 'resolve' → no write (audit-only marker)

    Safety: every overwrite is preceded by a backup at
    <fork>/.tasmota_merge_backups/<timestamp>/<rel_path>.
    """
    _set_phase('applying', 'Preparing backup directory…', 0)
    fork = session['fork_root']
    upstream = session['upstream_dir']
    if not fork or not upstream:
        _set_error('Cannot apply: fork or upstream missing.')
        return

    ts = time.strftime('%Y%m%d_%H%M%S')
    backup_root = os.path.join(fork, '.tasmota_merge_backups', ts)
    written = []
    skipped = []

    decisions = dict(session['decisions'])
    policies  = dict(session['policies'])
    files     = list(session['files'])

    # Compute effective action for every file: manual decision wins, else
    # policy decision, else nothing. Counts only writes for the progress bar.
    def _effective_action(rel):
        manual = decisions.get(rel, {}).get('action')
        if manual: return manual, 'manual'
        pol = _policy_decision_for(rel, policies)
        if pol: return pol, 'policy'
        return '', ''

    total = max(1, sum(1 for f in files
                       if _effective_action(f['path'])[0] in ('take', 'merge')))
    done  = 0

    for f in files:
        rel = f['path']
        act, src = _effective_action(rel)
        if not act: continue
        d = decisions.get(rel) or {}
        if act in ('keep', 'resolve'):
            skipped.append((rel, f'{act} ({src})'))
            continue

        fork_abs = os.path.join(fork, rel)
        os.makedirs(os.path.dirname(fork_abs), exist_ok=True)

        # Backup existing fork file if it exists
        if os.path.isfile(fork_abs):
            bk = os.path.join(backup_root, rel)
            os.makedirs(os.path.dirname(bk), exist_ok=True)
            shutil.copy2(fork_abs, bk)

        if act == 'take':
            up_abs = os.path.join(upstream, rel)
            if not os.path.isfile(up_abs):
                skipped.append((rel, 'take-but-upstream-missing'))
                continue
            shutil.copy2(up_abs, fork_abs)
            written.append((rel, 'take'))
        elif act == 'merge':
            content = d.get('merged_content', '')
            with open(fork_abs, 'w', encoding='utf-8', newline='') as wf:
                wf.write(content)
            written.append((rel, 'merge'))

        done += 1
        _set_phase('applying',
                   f'Applied {done}/{total}: {rel}',
                   int(done * 100 / total))

    # Persist final state
    _save_state(fork, decisions, extra={
        'last_apply_ts':       ts,
        'last_apply_written':  [w[0] for w in written],
        'last_apply_skipped':  [s[0] for s in skipped],
        'last_apply_backup':   backup_root,
    })

    summary = (f'Applied {len(written)} file(s). '
               f'Backup: {backup_root if written else "(none)"}. '
               f'Run `git diff` in the fork to review.')
    _set_phase('done', summary, 100)

# ── HTTP server ──────────────────────────────────────────────────────────────

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args): pass  # quiet

    # --- helpers -------------------------------------------------------------

    def _send_json(self, code, payload):
        body = json.dumps(payload).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def _send_text(self, code, text, ctype='text/plain; charset=utf-8'):
        body = text.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        n = int(self.headers.get('Content-Length', 0) or 0)
        if n <= 0: return {}
        raw = self.rfile.read(n)
        try:    return json.loads(raw.decode('utf-8'))
        except: return {}

    # --- routes --------------------------------------------------------------

    def do_GET(self):
        u = urlparse(self.path)
        p = u.path
        q = parse_qs(u.query)

        if p in ('/', '/index.html'):
            self._serve_html(); return

        if p == '/api/status':
            with state_lock:
                self._send_json(200, {
                    'phase':         session['phase'],
                    'progress_pct':  session['progress_pct'],
                    'progress_msg':  session['progress_msg'],
                    'error':         session['error'],
                    'fork_root':     session['fork_root'],
                    'upstream_url':  session['upstream_url'],
                    'upstream_label': session['upstream_label'],
                    'started_at':    session['started_at'],
                    'file_count':    len(session['files']),
                    'decision_count': len(session['decisions']),
                    'read_failures': len(_read_failures),
                    'tcc_denied':    _tcc_denied,
                    'read_failure_sample': [
                        {'path': p_, 'err': e_} for p_, e_ in _read_failures[:3]
                    ],
                })
            return

        if p == '/api/defaults':
            zp = os.path.join(CACHE_DIR, 'upstream.zip')
            cached = os.path.isfile(zp)
            self._send_json(200, {
                'fork_root':         DEFAULT_FORK,
                'upstream_url':      DEFAULT_UPSTREAM,
                'include_prefixes':  DEFAULT_INCLUDE_PREFIXES,
                'exclude_globs':     DEFAULT_EXCLUDE_GLOBS,
                'cache_dir':         CACHE_DIR,
                'has_cached_zip':    cached,
                'cached_zip_size':   (os.path.getsize(zp) if cached else 0),
                'cached_zip_mtime':  (os.path.getmtime(zp) if cached else 0),
            }); return

        if p == '/api/files':
            with state_lock:
                files     = list(session['files'])
                decisions = dict(session['decisions'])
                policies  = dict(session['policies'])
            for f in files:
                manual = decisions.get(f['path'], {}).get('action', '')
                if manual:
                    f['decision']        = manual
                    f['decision_source'] = 'manual'
                else:
                    pd = _policy_decision_for(f['path'], policies)
                    if pd:
                        f['decision']        = pd
                        f['decision_source'] = 'policy'
                    else:
                        f['decision']        = ''
                        f['decision_source'] = ''
            self._send_json(200, {'files': files}); return

        if p == '/api/policies':
            with state_lock:
                pol = dict(session['policies'])
                paths = [f['path'] for f in session['files']]
            self._send_json(200, {
                'always_keep':       list(pol.get('always_keep') or []),
                'always_take':       list(pol.get('always_take') or []),
                'always_keep_hits':  _count_pattern_matches(
                                       pol.get('always_keep') or [], paths),
                'always_take_hits':  _count_pattern_matches(
                                       pol.get('always_take') or [], paths),
            }); return

        if p == '/api/file':
            rel = (q.get('path') or [''])[0]
            self._serve_file_pair(rel); return

        if p == '/api/diff':
            rel = (q.get('path') or [''])[0]
            self._serve_unified_diff(rel); return

        self.send_error(404, 'Not found')

    def do_POST(self):
        u = urlparse(self.path)
        p = u.path

        if p == '/api/start':
            d = self._read_json()
            upstream = d.get('upstream_url')   or session['upstream_url']
            fork     = d.get('fork_root')      or session['fork_root']
            includes = d.get('include_prefixes') or session['include_prefixes']
            excludes = d.get('exclude_globs')    or session['exclude_globs']
            skip_dl  = bool(d.get('skip_download', False))

            if not os.path.isdir(fork):
                self._send_json(400, {'ok': False,
                                      'error': f'Fork path does not exist: {fork}'})
                return
            if session['phase'] in ('downloading', 'extracting', 'diffing', 'applying'):
                self._send_json(409, {'ok': False,
                                      'error': f'Already {session["phase"]} — wait or restart.'})
                return

            t = threading.Thread(target=_start_session,
                                 args=(upstream, fork, includes, excludes, skip_dl),
                                 daemon=True)
            t.start()
            self._send_json(200, {'ok': True}); return

        if p == '/api/decide':
            d = self._read_json()
            rel    = d.get('path') or ''
            action = d.get('action') or ''
            merged = d.get('merged_content', '')
            if not rel or action not in ('keep', 'take', 'merge', 'resolve', 'reset'):
                self._send_json(400, {'ok': False,
                                      'error': 'path & valid action required'}); return
            with state_lock:
                if action == 'reset':
                    session['decisions'].pop(rel, None)
                else:
                    entry = {'action': action,
                             'ts':     time.strftime('%Y-%m-%d %H:%M:%S')}
                    if action == 'merge':
                        entry['merged_content'] = merged
                    session['decisions'][rel] = entry
                _save_state(session['fork_root'], session['decisions'])
            self._send_json(200, {'ok': True}); return

        if p == '/api/apply':
            with state_lock:
                if session['phase'] != 'ready':
                    self._send_json(409, {'ok': False,
                                          'error': f'Not ready (phase={session["phase"]})'}); return
            t = threading.Thread(target=_apply_decisions, daemon=True)
            t.start()
            self._send_json(200, {'ok': True}); return

        if p == '/api/clear_decisions':
            with state_lock:
                session['decisions'] = {}
                _save_state(session['fork_root'], {})
            self._send_json(200, {'ok': True}); return

        if p == '/api/policies':
            d = self._read_json()
            ak = d.get('always_keep') or []
            at = d.get('always_take') or []
            if not isinstance(ak, list) or not isinstance(at, list):
                self._send_json(400, {'ok': False,
                                      'error': 'always_keep & always_take must be arrays'}); return
            # Normalize: strip empty / commented lines, dedupe
            def norm(lst):
                out = []
                for x in lst:
                    s = (x or '').strip()
                    if s and not s.startswith('#') and s not in out:
                        out.append(s)
                return out
            new_pol = {'always_keep': norm(ak), 'always_take': norm(at)}
            with state_lock:
                session['policies'] = new_pol
                _save_policies(session['fork_root'], new_pol)
                paths = [f['path'] for f in session['files']]
            keep_hits = _count_pattern_matches(new_pol['always_keep'], paths)
            take_hits = _count_pattern_matches(new_pol['always_take'], paths)
            self._send_json(200, {
                'ok':                True,
                'policies':          new_pol,
                'always_keep_hits':  keep_hits,
                'always_take_hits':  take_hits,
                'total_keep_files':  sum(h['count'] for h in keep_hits if h['count'] > 0),
                'total_take_files':  sum(h['count'] for h in take_hits if h['count'] > 0),
            }); return

        if p == '/api/test_policies':
            d = self._read_json()
            ak = d.get('always_keep') or []
            at = d.get('always_take') or []
            with state_lock:
                paths = [f['path'] for f in session['files']]
            self._send_json(200, {
                'always_keep_hits': _count_pattern_matches(ak, paths),
                'always_take_hits': _count_pattern_matches(at, paths),
            }); return

        if p == '/api/reset_policies':
            with state_lock:
                session['policies'] = {
                    'always_keep': list(DEFAULT_POLICIES['always_keep']),
                    'always_take': list(DEFAULT_POLICIES['always_take']),
                }
                _save_policies(session['fork_root'], session['policies'])
            self._send_json(200, {'ok': True, 'policies': session['policies']}); return

        if p == '/api/shutdown':
            self._send_json(200, {'ok': True})
            threading.Thread(target=lambda: (time.sleep(0.3), os._exit(0)),
                             daemon=True).start(); return

        self.send_error(404, 'Not found')

    # --- helpers cont'd ------------------------------------------------------

    def _serve_html(self):
        try:
            with open(HTML_FILE, 'rb') as f:
                body = f.read()
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)
        except FileNotFoundError:
            self.send_error(500, f'Missing HTML file at {HTML_FILE}')

    def _read_text_safe(self, path: str, max_bytes: int = 2_000_000):
        if not path or not os.path.isfile(path):
            return ('', 'missing')
        try:
            sz = os.path.getsize(path)
            if sz > max_bytes:
                return ('', f'too-large ({sz/1e6:.1f} MB)')
            with open(path, 'rb') as f:
                raw = f.read()
            if b'\x00' in raw[:4096]:
                return ('', 'binary')
            try:
                return (raw.decode('utf-8'), 'ok')
            except UnicodeDecodeError:
                return (raw.decode('latin-1'), 'ok-latin1')
        except Exception as e:
            return ('', f'read-error: {e}')

    def _serve_file_pair(self, rel: str):
        if not rel:
            self._send_json(400, {'ok': False, 'error': 'path required'}); return
        with state_lock:
            fork = session['fork_root']
            upstream = session['upstream_dir']
        fork_abs = os.path.join(fork, rel)
        up_abs   = os.path.join(upstream, rel)
        ftext, fstat = self._read_text_safe(fork_abs)
        utext, ustat = self._read_text_safe(up_abs)
        self._send_json(200, {
            'path':           rel,
            'fork_text':      ftext,
            'fork_status':    fstat,
            'upstream_text':  utext,
            'upstream_status': ustat,
            'fork_size':     (os.path.getsize(fork_abs) if os.path.isfile(fork_abs) else -1),
            'upstream_size': (os.path.getsize(up_abs)   if os.path.isfile(up_abs)   else -1),
        })

    def _serve_unified_diff(self, rel: str):
        if not rel:
            self._send_text(400, 'path required'); return
        with state_lock:
            fork = session['fork_root']
            upstream = session['upstream_dir']
        ftext, _ = self._read_text_safe(os.path.join(fork, rel))
        utext, _ = self._read_text_safe(os.path.join(upstream, rel))
        diff = difflib.unified_diff(
            ftext.splitlines(keepends=True),
            utext.splitlines(keepends=True),
            fromfile=f'fork/{rel}',
            tofile=f'upstream/{rel}',
            n=3,
        )
        self._send_text(200, ''.join(diff))


# ── ThreadingHTTPServer (one connection at a time is fine but we also want
#    /api/status responsive while a long /api/start is running — except the
#    long work is in a thread already, so default HTTPServer is enough.) ─────

def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=HTTP_PORT)
    ap.add_argument('--fork', default=DEFAULT_FORK)
    ap.add_argument('--no-browser', action='store_true')
    args = ap.parse_args()

    with state_lock:
        session['fork_root'] = args.fork

    httpd = HTTPServer(('127.0.0.1', args.port), Handler)
    print(f'[Tasmota Merge] Serving on http://127.0.0.1:{args.port}/')
    print(f'[Tasmota Merge] Fork detected: {args.fork or "(none — set on setup screen)"}')
    if not args.no_browser:
        threading.Thread(target=lambda: (time.sleep(0.5),
                                         open_browser_reliable(f'http://127.0.0.1:{args.port}/')),
                         daemon=True).start()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print('\n[Tasmota Merge] Shutting down.')

if __name__ == '__main__':
    main()
