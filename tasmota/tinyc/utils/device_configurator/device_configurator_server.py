#!/usr/bin/env python3
# Device Configurator — pick a `#define device_X` from user_config_override.h,
# edit its inner defines block, edit its companion [env:...] section in
# platformio_override.ini, then compile that one env. Runs as a tiny HTTP
# server on localhost; opens the browser SPA at http://127.0.0.1:8125/.
#
# Same pattern as serial_monitor: single Python file + embedded SPA,
# launched by Device Configurator.command.
#
# Files this tool reads/writes (all GITIGNORED, user-owned):
#   tasmota/user_config_override.h        (the device_X selector list + ifdef blocks)
#   platformio_override.ini               (the [env:...] blocks)
# A backup .bak is written next to each file before any save (rotating, 1 deep).

import http.server
import socketserver
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import json
import os
import re
import sys
import time
import subprocess
import threading
import webbrowser
from collections import deque
from pathlib import Path

HERE  = Path(__file__).resolve().parent
REPO  = HERE.parents[3]
UCO   = REPO / 'tasmota' / 'user_config_override.h'
PIO   = REPO / 'platformio_override.ini'
PORT  = 8125

# ---------------------------------------------------------------------------
# Parsing: user_config_override.h
# ---------------------------------------------------------------------------
#
# Each device has TWO regions:
#   1. SELECTOR line — `#define device_X` or `//#define device_X` near the
#      top of the file. The ACTIVE device is the one whose selector line is
#      uncommented. (Activate = comment out all others, uncomment chosen.)
#   2. BLOCK         — `#ifdef device_X` ... matching `#endif` further down,
#      containing the device-specific defines. May contain NESTED #ifdef.
#
# Mapping device→env is persisted as a single comment line right after the
# `#ifdef device_X`:
#     // device_config_env: tinyc32s3-p16
# That keeps the mapping with the block it belongs to (the file is gitignored
# anyway). If absent the user picks from a dropdown the first time.

SEL_RE = re.compile(r'^\s*(//\s*)?#define\s+(device_\w+)\s*$')
IF_RE  = re.compile(r'^\s*#if(?:def|ndef|\s)')
END_RE = re.compile(r'^\s*#endif\b')
MAP_RE     = re.compile(r'^\s*//\s*device_config_env\s*:\s*(\S+)\s*$')
TARGETS_RE = re.compile(r'^\s*//\s*device_config_targets\s*:\s*(.*)\s*$')

# Common section — a free-form region of `#undef` / `#define` that applies to
# every build regardless of which device_* selector is active. Demarcated by
# two marker comments anywhere in the file:
#     // ===== device_config_common: BEGIN =====
#     ... (editable content)
#     // ===== device_config_common: END =====
# Parser finds the markers anywhere; the UI surfaces their content in a tab.
COMMON_BEGIN_RE = re.compile(r'^\s*//\s*=+\s*device_config_common:\s*BEGIN\s*=*\s*$')
COMMON_END_RE   = re.compile(r'^\s*//\s*=+\s*device_config_common:\s*END\s*=*\s*$')
COMMON_BEGIN = '// ===== device_config_common: BEGIN ====='
COMMON_END   = '// ===== device_config_common: END ====='

def _read_lines(path):
    return path.read_text().splitlines(keepends=False)

def _write_lines(path, lines):
    # rotating one-deep backup
    try:
        bak = path.with_suffix(path.suffix + '.bak')
        bak.write_bytes(path.read_bytes())
    except Exception:
        pass
    path.write_text('\n'.join(lines) + '\n')

def parse_uco():
    """Return {name: {'sel_line':N, 'active':bool, 'block_start':N|None,
                      'block_end':N|None, 'env':str|None}}, plus the raw lines."""
    lines = _read_lines(UCO)
    devices = {}
    # 1. Find selector lines (any line matching SEL_RE)
    for i, line in enumerate(lines):
        m = SEL_RE.match(line)
        if not m:
            continue
        name = m.group(2)
        active = (m.group(1) is None) or (m.group(1).strip() == '')
        # If a device appears more than once in selector list, keep first.
        # Active wins if either occurrence is active.
        if name not in devices:
            devices[name] = {'sel_line': i, 'active': active,
                             'block_start': None, 'block_end': None, 'env': None}
        elif active:
            devices[name]['active'] = True
            devices[name]['sel_line'] = i
    # 2. Find matching #ifdef ... #endif block per device (nested-safe)
    for name, info in devices.items():
        for i, line in enumerate(lines):
            if re.match(rf'^\s*#ifdef\s+{re.escape(name)}\b', line):
                depth = 1
                for j in range(i + 1, len(lines)):
                    if IF_RE.match(lines[j]):
                        depth += 1
                    elif END_RE.match(lines[j]):
                        depth -= 1
                        if depth == 0:
                            info['block_start'] = i
                            info['block_end'] = j
                            break
                if info['block_start'] is not None:
                    # 3. Look for env mapping + targets marker comments in
                    #    the first ~8 lines of the block (right after #ifdef).
                    info['targets'] = []
                    for k in range(i + 1, min(i + 8, info['block_end'])):
                        m = MAP_RE.match(lines[k])
                        if m:
                            info['env'] = m.group(1)
                            continue
                        m = TARGETS_RE.match(lines[k])
                        if m:
                            info['targets'] = [t.strip() for t in m.group(1).split(',') if t.strip()]
                            continue
                break
    return lines, devices

def get_block_text(lines, info):
    """Return the verbatim block text (between #ifdef and #endif inclusive)."""
    if info.get('block_start') is None:
        return ''
    return '\n'.join(lines[info['block_start']:info['block_end'] + 1])

# ---------------------------------------------------------------------------
# Parsing: platformio_override.ini
# ---------------------------------------------------------------------------
#
# Envs are `[env:NAME]` blocks. Block = the [env:NAME] line plus all
# subsequent lines until the next `[` at column 0 or EOF. NOT a strict
# parse — preserve the user's formatting verbatim on round-trip.

ENV_RE = re.compile(r'^\[env:([A-Za-z0-9_.\-]+)\]\s*$')
SEC_RE = re.compile(r'^\[[^\]]+\]\s*$')

def parse_envs():
    """Return {name: (start_line, end_line_exclusive)} for every [env:NAME] block."""
    lines = _read_lines(PIO)
    envs = {}
    i = 0
    while i < len(lines):
        m = ENV_RE.match(lines[i])
        if m:
            name = m.group(1)
            start = i
            j = i + 1
            while j < len(lines) and not SEC_RE.match(lines[j]):
                j += 1
            envs[name] = (start, j)
            i = j
        else:
            i += 1
    return lines, envs

def get_env_text(lines, span):
    s, e = span
    # Trim trailing blank lines from the block for cleaner editing
    while e > s + 1 and lines[e - 1].strip() == '':
        e -= 1
    return '\n'.join(lines[s:e])

# ---------------------------------------------------------------------------
# Writes
# ---------------------------------------------------------------------------

def save_device_block(name, new_block_text):
    """Replace the existing #ifdef device_X ... #endif region with new_block_text.
       The new text MUST start with `#ifdef <name>` and end with `#endif`."""
    lines, devices = parse_uco()
    info = devices.get(name)
    if not info or info.get('block_start') is None:
        raise ValueError(f'no block found for {name}')
    new_lines = new_block_text.split('\n')
    if not new_lines or not new_lines[0].lstrip().startswith(f'#ifdef {name}'):
        raise ValueError('new block must start with `#ifdef ' + name + '`')
    if not new_lines[-1].lstrip().startswith('#endif'):
        raise ValueError('new block must end with `#endif`')
    out = lines[:info['block_start']] + new_lines + lines[info['block_end'] + 1:]
    _write_lines(UCO, out)

def save_device_env_mapping(name, env_name):
    """Insert/update the `// device_config_env: <env>` marker right after `#ifdef <name>`."""
    lines, devices = parse_uco()
    info = devices.get(name)
    if not info or info.get('block_start') is None:
        raise ValueError(f'no block found for {name}')
    bs = info['block_start']
    new_marker = f'// device_config_env: {env_name}'
    # Replace existing marker if present in the first few lines, else insert.
    replaced = False
    for k in range(bs + 1, min(bs + 8, info['block_end'])):
        if MAP_RE.match(lines[k]):
            lines[k] = new_marker
            replaced = True
            break
    if not replaced:
        lines.insert(bs + 1, new_marker)
    _write_lines(UCO, lines)

def save_device_targets(name, targets):
    """Insert/update `// device_config_targets: ip1, ip2, ...` right after the
       `#ifdef <name>` (and after the env-mapping line if it exists). Pass an
       empty list to remove the marker."""
    lines, devices = parse_uco()
    info = devices.get(name)
    if not info or info.get('block_start') is None:
        raise ValueError(f'no block found for {name}')
    bs = info['block_start']
    ips = [t.strip() for t in targets if t and t.strip()]
    new_marker = f'// device_config_targets: {", ".join(ips)}' if ips else None
    # Find existing targets marker
    existing_at = None
    insert_after = bs   # default: right after #ifdef
    for k in range(bs + 1, min(bs + 8, info['block_end'])):
        if TARGETS_RE.match(lines[k]):
            existing_at = k
            break
        if MAP_RE.match(lines[k]):
            insert_after = k   # put targets right after the env-mapping line
    if existing_at is not None:
        if new_marker is None:
            del lines[existing_at]
        else:
            lines[existing_at] = new_marker
    elif new_marker is not None:
        lines.insert(insert_after + 1, new_marker)
    _write_lines(UCO, lines)

def activate_device(name):
    """Make `name` the ACTIVE selector: uncomment its `#define device_<name>`
       and comment out every OTHER `#define device_*` selector line."""
    lines, devices = parse_uco()
    if name not in devices:
        raise ValueError(f'unknown device {name}')
    out = []
    for i, line in enumerate(lines):
        m = SEL_RE.match(line)
        if not m:
            out.append(line)
            continue
        this_name = m.group(2)
        # Preserve indentation; rewrite the leading slashes only.
        stripped = line.lstrip()
        indent = line[:len(line) - len(stripped)]
        if this_name == name:
            out.append(f'{indent}#define {this_name}')
        else:
            # Force commented form; collapse "//#define" or "// #define" -> "//#define"
            out.append(f'{indent}//#define {this_name}')
    _write_lines(UCO, out)

def find_common_span(lines):
    """Return (begin_idx, end_idx) of the BEGIN/END marker lines, or None."""
    b = e = None
    for i, line in enumerate(lines):
        if b is None and COMMON_BEGIN_RE.match(line):
            b = i
        elif b is not None and COMMON_END_RE.match(line):
            e = i
            break
    if b is not None and e is not None:
        return (b, e)
    return None

def read_common(lines):
    """Return (exists: bool, text: str). Text is the inner content (between
       the markers, exclusive of the marker lines themselves)."""
    span = find_common_span(lines)
    if not span:
        return False, ''
    b, e = span
    return True, '\n'.join(lines[b + 1:e])

def save_common(new_text):
    """Replace the inner content of the common region (preserves markers).
       Errors if the region does not exist — call create_common first."""
    lines = _read_lines(UCO)
    span = find_common_span(lines)
    if not span:
        raise ValueError('common section markers not found — create it first')
    b, e = span
    new_inner = new_text.split('\n') if new_text else []
    # Strip leading/trailing blank lines for cleanliness
    while new_inner and new_inner[0].strip() == '':
        new_inner.pop(0)
    while new_inner and new_inner[-1].strip() == '':
        new_inner.pop()
    out = lines[:b] + [COMMON_BEGIN] + new_inner + [COMMON_END] + lines[e + 1:]
    _write_lines(UCO, out)

def create_common(initial_text=''):
    """Insert a fresh common section right before the FIRST `#ifdef device_*`
       block (so it logically sits between the selector list at the top and the
       per-device blocks below). If no device block exists, append at EOF.
       Returns True if created, False if already present."""
    lines = _read_lines(UCO)
    if find_common_span(lines):
        return False
    # Find first `#ifdef device_*` line
    insert_at = None
    for i, line in enumerate(lines):
        m = re.match(r'^\s*#ifdef\s+(device_\w+)\b', line)
        if m:
            insert_at = i
            break
    if insert_at is None:
        insert_at = len(lines)
    inner = initial_text.split('\n') if initial_text else [
        '// Global `#undef` / `#define` that apply to EVERY build, regardless',
        '// of which `device_*` selector is active. These run AFTER the selector',
        '// list at the top of this file but BEFORE the per-device #ifdef blocks',
        '// below, so a device block can re-`#define` something this common',
        '// section `#undef`d.',
        '',
    ]
    block = ['', COMMON_BEGIN] + inner + [COMMON_END, '']
    out = lines[:insert_at] + block + lines[insert_at:]
    _write_lines(UCO, out)
    return True

def save_env_block(env_name, new_text):
    """Replace the `[env:env_name]` block (start..next section) with new_text.
       new_text must start with `[env:env_name]`."""
    lines, envs = parse_envs()
    span = envs.get(env_name)
    if not span:
        raise ValueError(f'no env block named {env_name}')
    new_lines = new_text.split('\n')
    if not ENV_RE.match(new_lines[0]) or ENV_RE.match(new_lines[0]).group(1) != env_name:
        raise ValueError('new text must start with [env:' + env_name + ']')
    s, e = span
    # Strip trailing blanks from the new text so we don't accumulate them
    while new_lines and new_lines[-1].strip() == '':
        new_lines.pop()
    out = lines[:s] + new_lines + [''] + lines[e:]
    _write_lines(PIO, out)

# ---------------------------------------------------------------------------
# Compile runner (background, captures stdout/stderr to a ring buffer)
# ---------------------------------------------------------------------------

_compile_state = {'running': False, 'env': None, 'pid': None, 'rc': None,
                  'started': None, 'finished': None}
_compile_log   = deque(maxlen=4000)   # ~4k lines of output kept
_compile_lock  = threading.Lock()
_compile_proc  = None

def _pio_binary():
    # Prefer the user's platformio venv (matches release.sh)
    cand = Path.home() / '.platformio' / 'penv' / 'bin' / 'pio'
    if cand.exists():
        return str(cand)
    for cand in ('/usr/local/bin/pio', '/opt/homebrew/bin/pio'):
        if Path(cand).exists():
            return cand
    return 'pio'

def _compile_worker(env_name):
    global _compile_proc
    cmd = [_pio_binary(), 'run', '-e', env_name]
    _compile_log.append(f'$ {" ".join(cmd)}')
    _compile_log.append(f'(cwd: {REPO})')
    try:
        _compile_proc = subprocess.Popen(
            cmd, cwd=str(REPO),
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            bufsize=1, text=True)
        with _compile_lock:
            _compile_state['pid'] = _compile_proc.pid
        for line in _compile_proc.stdout:
            _compile_log.append(line.rstrip('\n'))
        rc = _compile_proc.wait()
        _compile_log.append(f'[exit {rc}]')
        with _compile_lock:
            _compile_state['rc'] = rc
    except Exception as e:
        _compile_log.append(f'compile failed: {e!r}')
        with _compile_lock:
            _compile_state['rc'] = -1
    finally:
        with _compile_lock:
            _compile_state['running']  = False
            _compile_state['finished'] = time.time()
            _compile_proc = None

def start_compile(env_name):
    with _compile_lock:
        if _compile_state['running']:
            return False, 'already running'
        _compile_log.clear()
        _compile_state.update({'running': True, 'env': env_name, 'pid': None,
                               'rc': None, 'started': time.time(),
                               'finished': None})
    t = threading.Thread(target=_compile_worker, args=(env_name,), daemon=True)
    t.start()
    return True, 'started'

def stop_compile():
    global _compile_proc
    p = _compile_proc
    if p and p.poll() is None:
        try:
            p.terminate()
            for _ in range(20):
                if p.poll() is not None:
                    break
                time.sleep(0.1)
            if p.poll() is None:
                p.kill()
            _compile_log.append('[killed by user]')
            return True
        except Exception as e:
            _compile_log.append(f'[stop failed: {e!r}]')
    return False

def get_compile_status():
    with _compile_lock:
        st = dict(_compile_state)
    st['tail'] = list(_compile_log)[-200:]   # last 200 lines for quick poll
    return st


# ---------------------------------------------------------------------------
# OTA flasher (sequential per target — same safeboot escalation as our
# /tmp/flash_p16.sh + flash_m5b.sh did manually)
# ---------------------------------------------------------------------------

import urllib.request, urllib.error, ssl   # noqa: E402  (kept near use site)

def find_firmware_bin(env_name):
    """Return Path to the most recent firmware.bin for `env`, or None."""
    cand = REPO / '.pio' / 'build' / env_name / 'firmware.bin'
    return cand if cand.exists() else None

def _http(url, method='GET', data=None, timeout=8, headers=None):
    """Tiny urllib helper — returns (status, body_bytes) or (None, error_str)."""
    req = urllib.request.Request(url, data=data, method=method,
                                  headers=headers or {})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read() if hasattr(e, 'read') else b''
    except Exception as e:
        return None, str(e).encode('utf-8', 'replace')

def _device_status(ip):
    """Return (version, uptime_s) or (None, None)."""
    code, body = _http(f'http://{ip}/cm?cmnd=Status%202', timeout=5)
    if code != 200 or not body: return None, None
    try:
        d = json.loads(body)['StatusFWR']
        ver = d.get('Version', '')
    except Exception:
        return None, None
    code, body = _http(f'http://{ip}/cm?cmnd=Status%2011', timeout=5)
    up = None
    try:
        if code == 200:
            up = int(json.loads(body)['StatusSTS'].get('UptimeSec', -1))
    except Exception:
        pass
    return ver, up

def _post_multipart(url, filename, file_bytes, timeout=180):
    """Minimal multipart/form-data POST (field name 'file')."""
    boundary = '----dcfgr' + hex(int(time.time()*1000))[2:]
    crlf = b'\r\n'
    pre  = (f'--{boundary}{crlf.decode()}'
            f'Content-Disposition: form-data; name="file"; filename="{filename}"{crlf.decode()}'
            f'Content-Type: application/octet-stream{crlf.decode()}{crlf.decode()}').encode()
    post = f'{crlf.decode()}--{boundary}--{crlf.decode()}'.encode()
    body = pre + file_bytes + post
    headers = {'Content-Type': f'multipart/form-data; boundary={boundary}',
               'Content-Length': str(len(body))}
    return _http(url, method='POST', data=body, timeout=timeout, headers=headers)

# Single shared log + status, like _compile_*
_flash_state = {'running': False, 'targets': [], 'current': None,
                'started': None, 'finished': None,
                'results': {}}   # ip -> 'OK' | 'FAILED: ...'
_flash_lock  = threading.Lock()

def _flog(msg, ip=None):
    line = f'[flash:{ip}] {msg}' if ip else f'[flash] {msg}'
    _compile_log.append(line)

def _flash_one(ip, fw_path, fw_bytes):
    """Flash one device. Strategy: try direct /u2; on 'Speicherplatz', trigger
       /u4?u4=fct, wait for safeboot, retry /u2. Verify by reboot + tasmota app."""
    base = f'http://{ip}'
    _flog('probing', ip)
    pre_ver, pre_up = _device_status(ip)
    if pre_ver is None:
        _flog('unreachable', ip); return 'FAILED: unreachable'
    _flog(f'pre: ver={pre_ver} uptime={pre_up}s', ip)

    def try_upload(label):
        _flog(f'POST /u2 ({label})', ip)
        code, body = _post_multipart(f'{base}/u2', fw_path.name, fw_bytes, timeout=180)
        text = body.decode('utf-8', 'replace') if body else ''
        # strip HTML noise — Tasmota responds with a full page
        for marker in ['erfolgreich', 'Successful',
                       'fehlgeschlagen', 'Speicherplatz', 'nicht genug',
                       'miscompare']:
            if marker.lower() in text.lower():
                _flog(f'response: {marker}', ip); return marker.lower()
        _flog(f'response: code={code} unknown ({len(text)} B)', ip)
        return None

    result = try_upload('direct')
    needs_safeboot = result in (None, 'speicherplatz', 'nicht genug',
                                 'fehlgeschlagen', 'miscompare')
    success = result in ('erfolgreich', 'successful')

    if not success and needs_safeboot:
        _flog('escalating: trigger safeboot via /u4?u4=fct', ip)
        _http(f'{base}/u4?u4=fct&api=', timeout=10)
        sb = False
        for i in range(30):   # up to 150s
            time.sleep(5)
            code, body = _http(f'{base}/u4?u4=fct&api=', timeout=5)
            mark = body.decode('utf-8','replace').strip() if body else ''
            ver, _ = _device_status(ip)
            if mark == 'true' or (ver and '(safeboot)' in ver):
                sb = True; _flog('safeboot ready', ip); break
            if i % 3 == 0: _flog(f'  waiting safeboot... ({i*5}s)', ip)
        if not sb:
            return 'FAILED: safeboot not reached'
        time.sleep(2)
        result = try_upload('safeboot')
        success = result in ('erfolgreich', 'successful')

    if not success:
        return f'FAILED: upload ({result})'

    # Verify: wait for reboot (uptime drops) into the (tasmota) app.
    _flog('verifying reboot...', ip)
    for i in range(40):   # up to 200s
        time.sleep(5)
        ver, up = _device_status(ip)
        if ver and up is not None and up < 180 and '(tasmota)' in ver and '(safeboot)' not in ver:
            _flog(f'OK ver={ver} up={up}s', ip); return 'OK'
        if i % 3 == 0: _flog(f'  waiting reboot... ({i*5}s) ver={ver} up={up}', ip)
    return 'FAILED: reboot/verify timeout'

def _flash_worker(env_name, targets):
    fw_path = find_firmware_bin(env_name)
    if not fw_path:
        _flog(f'firmware.bin not found for env {env_name} — compile first.')
        with _flash_lock:
            _flash_state.update({'running': False, 'finished': time.time()})
        return
    fw_bytes = fw_path.read_bytes()
    _flog(f'=== OTA START env={env_name} fw={fw_path.name} ({len(fw_bytes)/1024:.0f} KB) targets={targets} ===')
    for ip in targets:
        with _flash_lock:
            _flash_state['current'] = ip
        _flog(f'-- target {ip} --')
        try:
            r = _flash_one(ip, fw_path, fw_bytes)
        except Exception as e:
            r = f'FAILED: {type(e).__name__}: {e}'
        _flog(f'-- {ip}: {r} --')
        with _flash_lock:
            _flash_state['results'][ip] = r
    _flog('=== OTA DONE ===')
    with _flash_lock:
        _flash_state.update({'running': False, 'current': None,
                             'finished': time.time()})

def start_flash(env_name, targets):
    with _compile_lock:
        if _compile_state.get('running'):
            return False, 'a compile is running'
    with _flash_lock:
        if _flash_state['running']:
            return False, 'flash already running'
        _flash_state.update({'running': True, 'targets': list(targets),
                             'current': None, 'started': time.time(),
                             'finished': None, 'results': {}})
    t = threading.Thread(target=_flash_worker, args=(env_name, list(targets)),
                         daemon=True)
    t.start()
    return True, 'started'

def get_flash_status():
    with _flash_lock:
        return dict(_flash_state)

# ---------------------------------------------------------------------------
# HTTP server
# ---------------------------------------------------------------------------

class Handler(BaseHTTPRequestHandler):
    server_version = 'DeviceConfigurator/1.0'

    def log_message(self, fmt, *args):
        # quiet logger
        sys.stderr.write(f'{self.address_string()} - {fmt % args}\n')

    # ----- low-level helpers -----
    def _send(self, code, ctype, body, headers=None):
        if isinstance(body, str):
            body = body.encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        if headers:
            for k, v in headers.items():
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj, code=200):
        self._send(code, 'application/json', json.dumps(obj))

    def _err(self, msg, code=400):
        self._json({'ok': False, 'error': msg}, code)

    def _read_json(self):
        n = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(n).decode('utf-8') if n else ''
        return json.loads(raw) if raw else {}

    # ----- routes -----
    def do_GET(self):
        try:
            return self._do_get()
        except Exception as e:
            self._err(f'{type(e).__name__}: {e}', 500)

    def do_POST(self):
        try:
            return self._do_post()
        except Exception as e:
            self._err(f'{type(e).__name__}: {e}', 500)

    def _do_get(self):
        p = self.path.split('?', 1)[0]
        if p == '/' or p == '/index.html':
            return self._send(200, 'text/html; charset=utf-8', SPA_HTML)
        if p == '/api/devices':
            _, devices = parse_uco()
            _, envs = parse_envs()
            out = []
            for name in sorted(devices.keys()):
                info = devices[name]
                out.append({
                    'name': name,
                    'active': bool(info['active']),
                    'has_block': info['block_start'] is not None,
                    'env': info.get('env'),
                })
            return self._json({'devices': out, 'envs': sorted(envs.keys())})
        if p.startswith('/api/device/'):
            name = p[len('/api/device/'):]
            lines, devices = parse_uco()
            info = devices.get(name)
            if not info:
                return self._err(f'unknown device {name}', 404)
            block = get_block_text(lines, info) if info['block_start'] is not None else ''
            env_name = info.get('env')
            env_block = ''
            if env_name:
                env_lines, envs = parse_envs()
                span = envs.get(env_name)
                if span:
                    env_block = get_env_text(env_lines, span)
            _, envs = parse_envs()
            return self._json({
                'name': name,
                'active': info['active'],
                'env': env_name,
                'targets': info.get('targets') or [],
                'fw_present': bool(env_name and find_firmware_bin(env_name)),
                'block': block,
                'env_block': env_block,
                'all_envs': sorted(envs.keys()),
            })
        if p == '/api/common':
            lines = _read_lines(UCO)
            exists, text = read_common(lines)
            return self._json({'exists': exists, 'text': text})
        if p == '/api/compile/status':
            return self._json(get_compile_status())
        if p == '/api/flash/status':
            return self._json(get_flash_status())
        if p == '/quit':
            self._send(200, 'text/plain', 'bye')
            threading.Thread(target=lambda: (time.sleep(0.2), os._exit(0)),
                             daemon=True).start()
            return
        return self._err(f'no route {p}', 404)

    def _do_post(self):
        p = self.path.split('?', 1)[0]
        data = self._read_json()
        if p == '/api/device/save_block':
            name = data.get('name')
            block = data.get('block', '')
            save_device_block(name, block)
            return self._json({'ok': True})
        if p == '/api/device/save_env_mapping':
            name = data.get('name')
            env  = data.get('env')
            if not env:
                return self._err('env required')
            save_device_env_mapping(name, env)
            return self._json({'ok': True})
        if p == '/api/device/save_targets':
            name = data.get('name')
            targets = data.get('targets', [])
            save_device_targets(name, targets)
            return self._json({'ok': True})
        if p == '/api/flash/start':
            env = data.get('env')
            targets = data.get('targets', [])
            if not env: return self._err('env required')
            if not targets: return self._err('no targets')
            ok, msg = start_flash(env, targets)
            return self._json({'ok': ok, 'msg': msg})
        if p == '/api/device/activate':
            name = data.get('name')
            activate_device(name)
            return self._json({'ok': True})
        if p == '/api/env/save':
            env  = data.get('env')
            text = data.get('text', '')
            save_env_block(env, text)
            return self._json({'ok': True})
        if p == '/api/common/save':
            save_common(data.get('text', ''))
            return self._json({'ok': True})
        if p == '/api/common/create':
            created = create_common(data.get('text', ''))
            return self._json({'ok': True, 'created': created})
        if p == '/api/compile/start':
            env = data.get('env')
            if not env:
                return self._err('env required')
            ok, msg = start_compile(env)
            return self._json({'ok': ok, 'msg': msg})
        if p == '/api/compile/stop':
            ok = stop_compile()
            return self._json({'ok': ok})
        return self._err(f'no route {p}', 404)


# ---------------------------------------------------------------------------
# Embedded SPA (single-file HTML/CSS/JS)
# ---------------------------------------------------------------------------

SPA_HTML = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Device Configurator</title>
<style>
 :root{--bg:#0e1116;--bar:#161b22;--fg:#d6deeb;--mut:#7a8aa0;--acc:#3b82f6;
       --ok:#87d96b;--warn:#e0a84e;--err:#ff6b6b;--inp:#0e1116;}
 *{box-sizing:border-box}
 html,body{margin:0;padding:0;height:100vh;background:var(--bg);color:var(--fg);
   font:13px/1.45 ui-sans-serif,system-ui,Segoe UI,sans-serif;overflow:hidden}
 #top{display:flex;align-items:center;padding:8px 12px;background:var(--bar);
   border-bottom:1px solid #222;gap:10px}
 #top h1{font-size:15px;margin:0;font-weight:600}
 #top .mut{color:var(--mut);font-size:12px;margin-left:auto}
 button,select,input{font:inherit;background:var(--inp);color:var(--fg);
   border:1px solid #30363d;border-radius:4px;padding:5px 9px;cursor:pointer}
 button:hover{border-color:var(--acc)}
 button[disabled]{opacity:.45;cursor:not-allowed}
 button.primary{background:#0d4884;border-color:#1d6fc0;color:#fff}
 button.primary:hover{background:#0e5aa8}
 button.danger{background:#5a1a1a;border-color:#7a2a2a}
 #main{display:flex;height:calc(100vh - 41px)}
 /* LEFT: device list */
 #left{flex:0 0 280px;background:#11151c;border-right:1px solid #222;
   overflow:auto;display:flex;flex-direction:column}
 #search{padding:8px;border-bottom:1px solid #222}
 #search input{width:100%}
 #devs{flex:1;overflow:auto;padding:4px 0}
 .dev{padding:5px 10px;cursor:pointer;border-left:2px solid transparent;
   display:flex;align-items:center;gap:6px}
 .dev:hover{background:#1b2230}
 .dev.sel{background:#162033;border-left-color:var(--acc)}
 .dev .name{flex:1;color:var(--fg);font-family:ui-monospace,Menlo,monospace;font-size:12px}
 .dev .dot{width:7px;height:7px;border-radius:50%;background:#444;flex:0 0 7px}
 .dev.active .dot{background:var(--ok);box-shadow:0 0 4px var(--ok)}
 .dev .badge{font-size:10px;color:var(--mut);padding:1px 5px;border:1px solid #2a3340;
   border-radius:9px}
 .dev.noblock .name{color:var(--err);text-decoration:line-through}
 /* RIGHT: edit panels */
 #right{flex:1;display:flex;flex-direction:column;min-width:0}
 #title{padding:9px 14px;background:#0c0f15;border-bottom:1px solid #222;
   display:flex;align-items:center;gap:12px;flex-wrap:wrap}
 #title h2{margin:0;font-size:14px;font-weight:600}
 #title .pill{font-size:11px;padding:2px 8px;border-radius:10px;border:1px solid #30363d;color:var(--mut)}
 #title .pill.act{color:var(--ok);border-color:#2a4a30;background:#0f1d12}
 #title .right{margin-left:auto;display:flex;gap:6px;align-items:center}
 .tabs{display:flex;background:#0c0f15;border-bottom:1px solid #222;padding:0 10px;gap:2px}
 .tab{padding:6px 14px;cursor:pointer;color:var(--mut);border:none;background:transparent;
   border-bottom:2px solid transparent;border-radius:0}
 .tab.on{color:var(--fg);border-bottom-color:var(--acc)}
 .tab.common{margin-left:auto;color:var(--warn)}
 .tab.common.on{border-bottom-color:var(--warn)}
 .pane{flex:1;display:none;flex-direction:column;min-height:0}
 .pane.on{display:flex}
 .pane header{padding:6px 14px;color:var(--mut);font-size:12px;display:flex;
   align-items:center;gap:10px;background:#0c0f15;border-bottom:1px solid #1a1f28}
 .pane header .right{margin-left:auto;display:flex;gap:6px}
 textarea{flex:1;width:100%;font:12px/1.45 ui-monospace,Menlo,Consolas,monospace;
   background:#0a0d12;color:#cfe2ff;border:0;padding:10px 14px;resize:none;outline:none;tab-size:2}
 #log{flex:1;overflow:auto;padding:8px 14px;font:11.5px/1.45 ui-monospace,Menlo,monospace;
   color:#cde7ff;background:#06080c;white-space:pre-wrap}
 #log .err{color:var(--err)} #log .ok{color:var(--ok)}
 .status{color:var(--mut);font-size:12px}
 .saved{color:var(--ok);font-size:11px;opacity:0;transition:opacity .25s}
 .saved.on{opacity:1}
 .err-msg{color:var(--err);font-size:11px}
 .no-env{color:var(--warn);font-size:12px;padding:14px}
</style>
</head>
<body>
<div id="top">
  <h1>Device Configurator</h1>
  <span class="mut" id="paths"></span>
  <span class="mut" id="lastSave"></span>
  <button onclick="fetch('/quit').then(()=>window.close())">Quit</button>
</div>

<div id="main">
  <aside id="left">
    <div id="search"><input id="filter" placeholder="filter devices…" oninput="renderList()"/></div>
    <div id="devs"></div>
  </aside>

  <main id="right">
    <div id="title">
      <h2 id="curName">— pick a device —</h2>
      <span class="pill" id="curActive">inactive</span>
      <span class="pill" id="curEnv">no env mapped</span>
      <span class="pill" id="curTargets" style="display:none">→ 0 targets</span>
      <div class="right">
        <button id="btnActivate" onclick="activate()">Activate</button>
        <button class="primary" id="btnCompile" onclick="compile()">Compile</button>
        <button class="primary" id="btnFlash" onclick="flash()" disabled title="OTA-flash the most recent build to the device IP(s) below">Flash ▸</button>
      </div>
    </div>

    <div class="tabs">
      <button class="tab on" data-pane="defs" onclick="tab('defs')">Defines</button>
      <button class="tab"    data-pane="env"  onclick="tab('env')">Env</button>
      <button class="tab"    data-pane="bld"  onclick="tab('bld')">Build</button>
      <button class="tab common" data-pane="common" onclick="tab('common')" title="global #undef / #define applied to every build">Common ⌑</button>
    </div>

    <section class="pane on" id="pane-defs">
      <header>
        <span>user_config_override.h — <code>#ifdef device_…</code> block</span>
        <span class="saved" id="sv-defs">saved ✓</span>
        <span class="err-msg" id="er-defs"></span>
        <div class="right">
          <button onclick="saveDefs()">Save</button>
        </div>
      </header>
      <textarea id="ta-defs" spellcheck="false" placeholder="(no device selected)"></textarea>
    </section>

    <section class="pane" id="pane-env">
      <header>
        <span>platformio_override.ini — <code>[env:…]</code> block</span>
        <select id="envSel" onchange="mapEnv(this.value)" title="map this device to a PlatformIO env"></select>
        <span class="saved" id="sv-env">saved ✓</span>
        <span class="err-msg" id="er-env"></span>
        <div class="right">
          <button onclick="saveEnv()">Save</button>
        </div>
      </header>
      <textarea id="ta-env" spellcheck="false" placeholder="(no env mapped)"></textarea>
    </section>

    <section class="pane" id="pane-bld">
      <header>
        <span>Build: <code id="bld-env">—</code></span>
        <span class="status" id="bld-stat">idle</span>
        <div class="right">
          <button class="danger" id="btnStop" onclick="stopCompile()" disabled>Stop</button>
        </div>
      </header>
      <header style="border-top:1px solid #1a1f28">
        <span>OTA targets (comma-separated IPs):</span>
        <input id="targets" type="text" placeholder="192.168.188.20, 192.168.188.135" style="flex:1;min-width:220px" oninput="onTargetsInput()" onblur="saveTargets()"/>
        <span class="saved" id="sv-targets">saved ✓</span>
        <span class="status" id="fw-stat"></span>
      </header>
      <div id="log">(no build yet — click Compile)</div>
    </section>

    <section class="pane" id="pane-common">
      <header>
        <span><b style="color:var(--warn)">Common — applies to EVERY build</b>
              · <code>user_config_override.h</code> between markers
              <code>device_config_common: BEGIN/END</code></span>
        <span class="saved" id="sv-common">saved ✓</span>
        <span class="err-msg" id="er-common"></span>
        <div class="right">
          <button id="btnCreateCommon" onclick="createCommon()" style="display:none">Create section</button>
          <button onclick="saveCommon()">Save</button>
        </div>
      </header>
      <div id="no-common" class="no-env" style="display:none">
        No common section found yet. Click <b>Create section</b> above to insert
        the BEGIN/END markers near the top of <code>user_config_override.h</code>
        (right before the first <code>#ifdef device_*</code> block, so it runs
        between the selector list and the per-device defines).
      </div>
      <textarea id="ta-common" spellcheck="false" placeholder="(empty — global #undef / #define here)"></textarea>
    </section>
  </main>
</div>

<script>
let DEVS=[], ENVS=[], CUR=null, FILTER='';

async function api(path, body){
  const init = body ? {method:'POST', headers:{'Content-Type':'application/json'},
                       body:JSON.stringify(body)} : undefined;
  const r = await fetch(path, init);
  return r.json();
}
const $=s=>document.querySelector(s);
const _esc=s=>String(s==null?'':s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

async function loadDevices(){
  const j = await api('/api/devices');
  DEVS = j.devices;
  ENVS = j.envs;
  $('#paths').textContent = `${DEVS.length} devices · ${ENVS.length} envs`;
  renderList();
}

function renderList(){
  FILTER = $('#filter').value.trim().toLowerCase();
  const ul = $('#devs');
  ul.innerHTML = '';
  let active = null;
  DEVS.forEach(d=>{
    if(FILTER && !d.name.toLowerCase().includes(FILTER)) return;
    const div = document.createElement('div');
    div.className = 'dev' + (d.active?' active':'') + (d.has_block?'':' noblock')
      + (CUR && CUR.name===d.name ? ' sel' : '');
    div.onclick = ()=>selectDevice(d.name);
    div.innerHTML = '<span class="dot" title="'+(d.active?'ACTIVE':'inactive')+'"></span>'
      + '<span class="name">'+_esc(d.name.replace(/^device_/,''))+'</span>'
      + (d.env ? '<span class="badge" title="env">'+_esc(d.env)+'</span>' : '');
    ul.appendChild(div);
    if(d.active) active = d;
  });
  if(!CUR && active) selectDevice(active.name);
}

async function selectDevice(name){
  const j = await api('/api/device/'+encodeURIComponent(name));
  if(j.error){ alert(j.error); return; }
  CUR = j;
  $('#curName').textContent = name;
  $('#curActive').className = 'pill' + (j.active ? ' act' : '');
  $('#curActive').textContent = j.active ? 'ACTIVE selector' : 'inactive';
  $('#curEnv').textContent = j.env ? ('env: '+j.env) : 'no env mapped';
  $('#ta-defs').value = j.block || '(no block found for '+name+')';
  $('#ta-env').value  = j.env_block || '';
  $('#ta-env').placeholder = j.env ? '' : 'pick an env above ↑';
  // Env dropdown
  const sel = $('#envSel');
  sel.innerHTML = '<option value="">— map to env —</option>'
    + j.all_envs.map(e=>'<option value="'+_esc(e)+'"'
        + (j.env===e?' selected':'')+'>'+_esc(e)+'</option>').join('');
  $('#btnActivate').disabled = j.active;
  $('#btnCompile').disabled  = !j.env;
  $('#bld-env').textContent  = j.env || '—';
  // Targets + flash UI
  $('#targets').value = (j.targets||[]).join(', ');
  updateTargetsUI(j.targets||[], j.fw_present);
  // Highlight current in list
  document.querySelectorAll('.dev').forEach(el=>{
    el.classList.toggle('sel', el.querySelector('.name').textContent === name.replace(/^device_/,''));
  });
  flagSaved('sv-defs', false);
  flagSaved('sv-env',  false);
}

function tab(id){
  document.querySelectorAll('.tab').forEach(t=>t.classList.toggle('on', t.dataset.pane===id));
  document.querySelectorAll('.pane').forEach(p=>p.classList.toggle('on', p.id==='pane-'+id));
  if(id==='bld') pollCompile();
  if(id==='common') loadCommon();
}
async function loadCommon(){
  const j = await api('/api/common');
  $('#ta-common').value = j.text || '';
  $('#no-common').style.display = j.exists ? 'none' : 'block';
  $('#ta-common').style.display = j.exists ? 'block' : 'none';
  $('#btnCreateCommon').style.display = j.exists ? 'none' : 'inline-block';
}
async function saveCommon(){
  $('#er-common').textContent = '';
  const j = await api('/api/common/save', {text: $('#ta-common').value});
  if(j.ok) flagSaved('sv-common', true);
  else $('#er-common').textContent = j.error || 'save failed';
}
async function createCommon(){
  if(!confirm('Insert `device_config_common: BEGIN/END` markers near the top of user_config_override.h?\n\n(Right before the first `#ifdef device_*` block.)')) return;
  const j = await api('/api/common/create', {});
  if(j.ok){ await loadCommon(); }
  else alert(j.error || 'create failed');
}
function flagSaved(id, on){
  const el = document.getElementById(id);
  el.classList.toggle('on', !!on);
  if(on) setTimeout(()=>el.classList.remove('on'), 1500);
}

async function saveDefs(){
  if(!CUR) return;
  $('#er-defs').textContent = '';
  const j = await api('/api/device/save_block', {name: CUR.name, block: $('#ta-defs').value});
  if(j.ok){ flagSaved('sv-defs', true); }
  else { $('#er-defs').textContent = j.error || 'save failed'; }
}
async function saveEnv(){
  if(!CUR || !CUR.env){ alert('Map an env first'); return; }
  $('#er-env').textContent = '';
  const j = await api('/api/env/save', {env: CUR.env, text: $('#ta-env').value});
  if(j.ok){ flagSaved('sv-env', true); }
  else { $('#er-env').textContent = j.error || 'save failed'; }
}
async function mapEnv(env){
  if(!CUR || !env) return;
  const j = await api('/api/device/save_env_mapping', {name: CUR.name, env: env});
  if(j.ok){ await selectDevice(CUR.name); await loadDevices(); }
  else alert(j.error || 'mapping failed');
}
async function activate(){
  if(!CUR) return;
  if(!confirm('Make `'+CUR.name+'` the active selector?\n\nAll other `#define device_*` lines in user_config_override.h will be commented out.')) return;
  const j = await api('/api/device/activate', {name: CUR.name});
  if(j.ok){ await loadDevices(); await selectDevice(CUR.name); }
  else alert(j.error || 'activate failed');
}
async function compile(){
  if(!CUR || !CUR.env) return;
  if(!CUR.active && !confirm('Selector `'+CUR.name+'` is NOT active in user_config_override.h.\n\n'
    + 'The build still uses env `'+CUR.env+'` but the device defines won\'t be picked up unless you Activate first.\n\nCompile anyway?')) return;
  tab('bld');
  $('#log').textContent = '';
  $('#bld-stat').textContent = 'starting…';
  $('#btnStop').disabled = false;
  $('#btnCompile').disabled = true;
  const j = await api('/api/compile/start', {env: CUR.env});
  if(!j.ok){ $('#bld-stat').textContent = 'failed: '+j.msg; $('#btnCompile').disabled = false; return; }
  pollCompile();
}
async function stopCompile(){
  await api('/api/compile/stop', {});
}
let _pollTO=null;
async function pollCompile(){
  clearTimeout(_pollTO);
  const j = await api('/api/compile/status');
  const log = $('#log');
  log.textContent = (j.tail||[]).join('\n');
  log.scrollTop = log.scrollHeight;
  if(j.running){
    const t = j.started ? Math.floor(performance.now()/1000 + (Date.now()/1000 - j.started)*0) : 0;
    const dt = j.started ? Math.floor(Date.now()/1000 - j.started) : 0;
    $('#bld-stat').textContent = `building (env=${j.env}, ${dt}s, pid=${j.pid||''})`;
    _pollTO = setTimeout(pollCompile, 1500);
  } else {
    $('#btnStop').disabled = true;
    if(CUR && CUR.env) $('#btnCompile').disabled = false;
    if(j.rc===null) $('#bld-stat').textContent = 'idle';
    else if(j.rc===0) $('#bld-stat').innerHTML = '<span style="color:var(--ok)">SUCCESS</span>';
    else $('#bld-stat').innerHTML = '<span style="color:var(--err)">FAILED (rc='+j.rc+')</span>';
  }
}

// ---- Targets + OTA flash ----
function parseTargets(){
  return $('#targets').value.split(',').map(s=>s.trim()).filter(Boolean);
}
function updateTargetsUI(targets, fwPresent){
  const n = (targets||[]).length;
  const pill = $('#curTargets');
  pill.style.display = n ? 'inline-block' : 'none';
  pill.textContent   = '→ ' + n + (n===1?' target':' targets');
  $('#btnFlash').disabled = !(n && fwPresent);
  $('#btnFlash').textContent = 'Flash ' + (n||'') + ' ▸';
  $('#fw-stat').textContent = fwPresent
    ? 'firmware.bin ready'
    : (CUR && CUR.env ? '(no firmware.bin yet — compile first)' : '(map an env first)');
}
function onTargetsInput(){
  updateTargetsUI(parseTargets(), CUR && CUR.fw_present);
}
async function saveTargets(){
  if(!CUR) return;
  const ts = parseTargets();
  const j = await api('/api/device/save_targets', {name: CUR.name, targets: ts});
  if(j.ok){ CUR.targets = ts; flagSaved('sv-targets', true); }
}
async function flash(){
  if(!CUR || !CUR.env) return;
  const ts = parseTargets();
  if(!ts.length){ alert('No targets'); return; }
  if(!confirm('OTA-flash ' + ts.length + ' device(s) with the most recent build of env "' + CUR.env + '"?\n\n  ' + ts.join('\n  '))) return;
  await saveTargets();   // persist the current text first
  tab('bld');
  const j = await api('/api/flash/start', {env: CUR.env, targets: ts});
  if(!j.ok){ alert('flash failed to start: ' + j.msg); return; }
  $('#btnFlash').disabled = true;
  $('#btnCompile').disabled = true;
  pollFlash();
}
async function pollFlash(){
  const j = await api('/api/flash/status');
  const c = await api('/api/compile/status');   // shared ring buffer
  const log = $('#log');
  log.textContent = (c.tail||[]).join('\n');
  log.scrollTop = log.scrollHeight;
  if(j.running){
    const dt = j.started ? Math.floor(Date.now()/1000 - j.started) : 0;
    $('#bld-stat').innerHTML = '<span style="color:var(--warn)">flashing ' + (j.current||'') + ' (' + dt + 's)</span>';
    setTimeout(pollFlash, 2000);
  } else {
    if(CUR && CUR.env) $('#btnCompile').disabled = false;
    if(CUR) updateTargetsUI(CUR.targets, CUR.fw_present);
    if(j.finished){
      const r = j.results || {};
      const total = Object.keys(r).length;
      const ok = Object.values(r).filter(v=>v==='OK').length;
      $('#bld-stat').innerHTML = ok===total && total>0
        ? '<span style="color:var(--ok)">flash OK (' + ok + '/' + total + ')</span>'
        : '<span style="color:var(--err)">flash done (' + ok + '/' + total + ' OK)</span>';
    }
  }
}
// After a compile finishes, fw_present flips true — re-check targets state.
const _origPollCompile = pollCompile;
pollCompile = async function(){
  await _origPollCompile.apply(this, arguments);
  if(CUR){
    const j = await api('/api/device/' + encodeURIComponent(CUR.name));
    CUR.fw_present = j.fw_present;
    updateTargetsUI(CUR.targets, j.fw_present);
  }
}

loadDevices();
</script>
</body>
</html>
'''


def main():
    if not UCO.exists():
        print(f'cannot find {UCO}', file=sys.stderr); sys.exit(1)
    if not PIO.exists():
        print(f'cannot find {PIO}', file=sys.stderr); sys.exit(1)

    # Bind to 127.0.0.1, NOT 0.0.0.0: HTTPServer.server_bind() calls
    # socket.getfqdn(host) to set server_name, and getfqdn('0.0.0.0') does a
    # reverse-DNS roundtrip that can take 5+ seconds on macOS with a
    # router-served domain (e.g. .fritz.box) — that delay is the "kann keine
    # Verbindung aufbauen" window after the browser opens. 127.0.0.1 is
    # instant AND restricts the server to localhost (safer for a dev tool).
    httpd = ThreadingHTTPServer(('127.0.0.1', PORT), Handler)
    url = f'http://127.0.0.1:{PORT}/'
    print(f'Device Configurator listening on {url}')
    print(f'  repo: {REPO}')
    threading.Thread(target=lambda: (time.sleep(0.4), webbrowser.open(url)),
                     daemon=True).start()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print('\nshutting down')
        httpd.shutdown()


if __name__ == '__main__':
    main()
