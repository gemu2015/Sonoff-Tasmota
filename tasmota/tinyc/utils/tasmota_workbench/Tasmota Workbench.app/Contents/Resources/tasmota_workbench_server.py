#!/usr/bin/env python3
# Tasmota Workbench — a browser-based control surface for ESP / Tasmota
# devices. One dependency-light Python server (pyserial for the serial pane;
# stdlib sockets for everything else) serving an embedded SPA with three tabs:
#
#   • Monitor — a large-scrollback console. Two capture sources, usable at
#       the same time, feeding one shared ring:
#         - Serial : a local USB/serial port (pyserial)
#         - Syslog : a UDP syslog listener; set Tasmota
#                    `LogHost <this-ip>` / `LogPort <port>` / `SysLog 2`;
#                    each line is tagged with the sending device's IP.
#   • Devices — LAN scan of Tasmota devices (CPU / build / partitions / heap),
#               inline rename of DeviceName + Hostname, OTA-flash one or many.
#   • Shares  — Tasmota multicast share-protocol monitor (the `g:` global-
#               variable broadcast on 239.255.255.250:1999): live table of
#               which device emits which global, with clash detection (same
#               name from multiple devices) and CSV export.
#
# Was historically called "Serial Monitor"; the name shifted as the tool
# grew the Devices and Shares panes. Settings/launch-log paths migrated:
# old `~/.serial_monitor.json` is auto-read for backward compat on first run.
#
# Run:  python3 tasmota_workbench_server.py   (opens http://127.0.0.1:8124/)
# or double-click "Tasmota Workbench.app" / "Tasmota Workbench.command".

import os, sys, json, threading, time, socket, subprocess, webbrowser
import tempfile, http.client, base64, shutil, re as _re, struct
import urllib.request, concurrent.futures
from collections import deque, OrderedDict
from http.server import (HTTPServer, ThreadingHTTPServer,
                          BaseHTTPRequestHandler)
from urllib.parse import urlparse, parse_qs

HTTP_PORT  = 8124
# Big ring so events do NOT scroll away. ~200k lines ≈ tens of MB RAM;
# override with TASMOTA_WORKBENCH_HISTORY=<lines>.
HISTORY    = int(os.environ.get('TASMOTA_WORKBENCH_HISTORY',
                  os.environ.get('SERIAL_MONITOR_HISTORY', '200000')))
# Remember the last-used port/baud across restarts. New canonical path;
# read the legacy `~/.serial_monitor.json` on first run if present.
SETTINGS        = os.path.expanduser('~/.tasmota_workbench.json')
SETTINGS_LEGACY = os.path.expanduser('~/.serial_monitor.json')

# Tasmota share-protocol multicast (Scripter/TinyC `g:` globals broadcast).
SHARE_MCAST_GRP  = '239.255.255.250'
SHARE_MCAST_PORT = 1999


def _load_settings():
    # Try canonical path first. Fall back to legacy ~/.serial_monitor.json so
    # existing users keep their last-used port/baud after the rename. The
    # legacy file is read-only here; the next _save_settings() writes to the
    # canonical path so subsequent runs use the new file directly.
    for p in (SETTINGS, SETTINGS_LEGACY):
        try:
            with open(p) as f:
                d = json.load(f)
                if isinstance(d, dict):
                    return d
        except Exception:
            continue
    return {}


def _save_settings(**kw):
    try:
        d = _load_settings()
        d.update(kw)
        with open(SETTINGS, 'w') as f:
            json.dump(d, f)
    except Exception:
        pass


_s = _load_settings()
pref_device = str(_s.get('device', ''))
pref_baud   = int(_s.get('baud', 115200))
pref_sport  = int(_s.get('sport', 514))

state_lock  = threading.Lock()
serial_port = None                       # pyserial Serial or None
log_buf     = deque(maxlen=HISTORY)       # dicts: {seq,t,d,x[,h][,src]}
log_seq     = 0                           # monotonic; survives rotation
rx_bytes    = 0
cur_device  = ''
cur_baud    = 0
syslog_sock = None                        # bound UDP socket or None
syslog_port = 0                           # 0 = not listening

fw_lock     = threading.Lock()
fw_path     = None                        # temp file with uploaded .bin
fw_name     = ''
fw_size     = 0
flash_proc  = None                        # running esptool Popen or None
flash_state = {'running': False, 'pct': 0, 'phase': '',
               'ok': None, 'error': ''}


def _run(cmd, timeout=4):
    """subprocess.run wrapper; on Windows suppresses the console flash."""
    kw = {}
    if sys.platform == 'win32':
        kw['creationflags'] = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
    return subprocess.run(cmd, capture_output=True, text=True,
                          timeout=timeout, **kw)


def _open_url(url):
    try:
        if sys.platform == 'darwin':
            subprocess.Popen(['open', url]); return True
        if sys.platform == 'win32':
            os.startfile(url); return True          # noqa: built-in
    except Exception:
        pass
    try:
        return webbrowser.open(url)
    except Exception:
        return False


def _line_hex(raw):
    """Hex view of a raw line, only stored when it contains
    non-printable / non-ASCII bytes (keeps memory ~unchanged for
    plain text logs; pure-ASCII hex is derived client-side)."""
    if any((b < 0x20 and b != 0x09) or b >= 0x7f for b in raw):
        return ' '.join(f'{b:02x}' for b in raw)
    return None


def add_line(direction, text, hexs=None, src=None):
    """direction: 'rx' | 'tx' | 'info' | 'err'. src: optional source
    tag (syslog sender IP) so multi-device logs stay disambiguated."""
    global log_seq
    with state_lock:
        log_seq += 1
        e = {'seq': log_seq, 't': time.strftime('%H:%M:%S'),
             'd': direction, 'x': text}
        if hexs:
            e['h'] = hexs
        if src:
            e['src'] = src
        log_buf.append(e)


def list_ports():
    try:
        from serial.tools import list_ports as lp
        out = []
        for p in lp.comports():
            out.append({'device': p.device,
                        'desc': (p.description or '').strip()})
        # Helpful default ordering: usbserial/usbmodem first.
        out.sort(key=lambda d: (('usb' not in d['device'].lower()),
                                 d['device']))
        return out
    except ImportError:
        return []


def _serial_reader():
    """Accumulate bytes, emit a log line per newline; flush a pending
    partial line after a short idle so promptless output still shows."""
    global rx_bytes
    buf = bytearray()
    last = time.time()
    while True:
        with state_lock:
            sp = serial_port
        if sp is None:
            buf.clear()
            time.sleep(0.1)
            continue
        try:
            chunk = sp.read(4096)
        except Exception as e:
            add_line('err', f'[serial read error: {e}]')
            _close_serial()
            continue
        now = time.time()
        if chunk:
            with state_lock:
                rx_bytes += len(chunk)
            buf.extend(chunk)
            while True:
                nl = buf.find(b'\n')
                if nl < 0:
                    break
                raw = bytes(buf[:nl]).rstrip(b'\r')
                del buf[:nl + 1]
                add_line('rx', raw.decode('utf-8', 'replace'),
                         _line_hex(raw))
            last = now
        else:
            # idle: flush a lingering partial line (e.g. a prompt)
            if buf and (now - last) > 0.25:
                pr = bytes(buf)
                add_line('rx', pr.decode('utf-8', 'replace'),
                         _line_hex(pr))
                buf.clear()
            time.sleep(0.02)


def _close_serial(quiet=False):
    global serial_port, cur_device, cur_baud
    with state_lock:
        sp = serial_port
        serial_port = None
        dev = cur_device
        cur_device = ''
        cur_baud = 0
    if sp is not None:
        try:
            sp.close()
        except Exception:
            pass
        if not quiet:
            add_line('info', f'[closed {dev}]')


def _write_serial(text, eol):
    """Send a typed command to the device. eol: 'crlf'|'lf'|'cr'|''."""
    end = {'crlf': '\r\n', 'lf': '\n', 'cr': '\r'}.get(eol, '')
    with state_lock:
        sp = serial_port
    if sp is None:
        return False, 'not connected'
    try:
        sp.write((text + end).encode('utf-8', 'replace'))
        sp.flush()
    except Exception as e:
        return False, str(e)
    add_line('tx', text)
    return True, None


def _open_serial(device, baud):
    global serial_port, cur_device, cur_baud, pref_device, pref_baud
    try:
        import serial as pyserial
    except ImportError:
        return False, 'pyserial not installed (pip3 install pyserial)'
    _close_serial(quiet=True)
    try:
        p = pyserial.Serial(port=device, baudrate=int(baud),
                             bytesize=8, parity='N', stopbits=1,
                             timeout=0.05)
    except Exception as e:
        return False, str(e)
    with state_lock:
        serial_port = p
        cur_device = device
        cur_baud = int(baud)
    pref_device, pref_baud = device, int(baud)
    _save_settings(device=device, baud=int(baud))
    add_line('info', f'[opened {device} @ {baud} baud]')
    return True, None


def _default_ip():
    """The source IP the OS would use to reach the internet — for a
    normal single-LAN host this is THE one to use for `LogHost`."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))       # no packet sent; picks route
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '127.0.0.1'


def _iface_map():
    """{iface_or_adapter: [ipv4,...]}. Windows: ipconfig (adapter name
    is already friendly). mac/BSD/Linux: ifconfig, then `ip` fallback.
    Best-effort; empty on failure."""
    import re
    m = {}
    if sys.platform == 'win32':
        try:
            out = _run(['ipconfig']).stdout
            cur = None
            for ln in out.splitlines():
                if ln.strip() and not ln[:1].isspace():
                    cur = re.sub(r'.*adapter\s*', '',
                                 ln.strip().rstrip(':')).strip() \
                          or ln.strip().rstrip(':')
                    m.setdefault(cur, [])
                else:
                    g = re.search(r'IPv4 Address.*?:\s*'
                                  r'([\d.]+)', ln)
                    if cur and g:
                        m[cur].append(g.group(1))
        except Exception:
            pass
        return m
    try:
        out = _run(['ifconfig']).stdout
        cur = None
        for ln in out.splitlines():
            h = re.match(r'^(\w[\w.\-]*):?\s', ln)
            if h and not ln[:1].isspace():
                cur = h.group(1)
                m.setdefault(cur, [])
            ip = re.search(r'\binet (\d+\.\d+\.\d+\.\d+)', ln)
            if cur and ip:
                m[cur].append(ip.group(1))
    except Exception:
        pass
    if not m:
        try:
            out = _run(['ip', '-o', '-4', 'addr']).stdout
            for ln in out.splitlines():
                g = re.match(r'^\d+:\s+(\S+)\s+inet\s+'
                             r'(\d+\.\d+\.\d+\.\d+)', ln)
                if g:
                    m.setdefault(g.group(1), []).append(g.group(2))
        except Exception:
            pass
    return m


def _mac_port_names():
    """macOS: {device: 'Wi-Fi'|'Ethernet'|...} for friendly labels."""
    names = {}
    if sys.platform != 'darwin':
        return names
    try:
        out = _run(['networksetup', '-listallhardwareports']).stdout
        port = None
        for ln in out.splitlines():
            if ln.startswith('Hardware Port:'):
                port = ln.split(':', 1)[1].strip()
            elif ln.startswith('Device:') and port:
                names[ln.split(':', 1)[1].strip()] = port
    except Exception:
        pass
    return names


def _local_ips():
    """LAN IPv4 candidates for Tasmota `LogHost`, collapsed to one
    entry per real interface/subnet and labelled. Default first.
    Returns [{'ip','label','default'}]."""
    default = _default_ip()
    ports = _mac_port_names()
    ifaces = _iface_map()
    out, seen_sub = [], set()

    def add(ip, dev):
        if (ip.startswith('127.') or ip.startswith('169.254.')
                or ip in [o['ip'] for o in out]):
            return
        sub = ip.rsplit('.', 1)[0]
        if sub in seen_sub:                  # collapse aliases/subnet dups
            return
        seen_sub.add(sub)
        label = ports.get(dev) or dev or 'net'
        out.append({'ip': ip, 'label': label,
                    'default': ip == default})

    # Default first, with its interface label if we can find it.
    ddev = next((d for d, ips in ifaces.items() if default in ips), '')
    add(default, ddev)
    for dev, ips in ifaces.items():
        for ip in ips:
            add(ip, dev)
    # Generic fallback (any OS): pull extra subnets from DNS so the
    # list is still useful if ifconfig/ip/ipconfig parsing failed.
    try:
        for ip in socket.gethostbyname_ex(socket.gethostname())[2]:
            add(ip, '')
    except Exception:
        pass
    if not out:                              # last-resort fallback
        out = [{'ip': default, 'label': '', 'default': True}]
    return out


def _syslog_reader(sock):
    """One UDP datagram = one log line. Strip the RFC3164 <PRI> prefix;
    tag with the sender IP so several devices show in one stream."""
    import re
    pri = re.compile(rb'^<\d{1,3}>')
    while True:
        try:
            data, addr = sock.recvfrom(65535)
        except OSError:
            return                       # socket closed by _stop_syslog
        if not data:
            continue
        ip = addr[0]
        for raw in data.replace(b'\r', b'').split(b'\n'):
            if not raw:
                continue
            raw = pri.sub(b'', raw, count=1)
            add_line('rx', raw.decode('utf-8', 'replace'),
                     _line_hex(raw), src=ip)


def _start_syslog(port):
    global syslog_sock, syslog_port
    _stop_syslog(quiet=True)
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('0.0.0.0', int(port)))
    except PermissionError:
        return False, (f'Port {port} needs root (<1024). Use a high '
                       f'port, e.g. 5514, and set Tasmota LogPort 5514.')
    except Exception as e:
        return False, str(e)
    with state_lock:
        syslog_sock = s
        syslog_port = int(port)
    threading.Thread(target=_syslog_reader, args=(s,),
                     daemon=True).start()
    _save_settings(sport=int(port))
    ip = _local_ips()[0]['ip']
    add_line('info', f'[syslog listening on UDP {port} — on the device: '
                     f'Backlog LogHost {ip}; LogPort {port}; SysLog 2]')
    return True, None


def _stop_syslog(quiet=False):
    global syslog_sock, syslog_port
    with state_lock:
        s = syslog_sock
        syslog_sock = None
        p = syslog_port
        syslog_port = 0
    if s is not None:
        try:
            s.close()
        except Exception:
            pass
        if not quiet:
            add_line('info', f'[syslog stopped (was UDP {p})]')


# --------------------- share-protocol monitor ----------------------
# Tasmota's Scripter/TinyC `g:` cross-device global broadcasts (multicast
# 239.255.255.250:1999). One line per global per packet, either
#   name=value          (text)        OR
#   name:<2B little-endian count><count*float32 little-endian>    (binary)
# We bind the multicast group, capture for a user-chosen duration, decode
# every packet, build a per-(source-IP, name) table, detect name clashes
# (same global emitted by >1 IP), then resolve each sender's device name
# via HTTP /cm?cmnd=DeviceName so the table reads device-friendly.

share_state = {
    'scanning': False, 'started': None, 'finished': None,
    'duration': 0, 'progress': 0, 'packets': 0,
    'status': 'idle', 'results': [], 'clashes': [], 'stop_flag': False,
}
share_lock = threading.Lock()


def _share_parse_packet(data):
    """Yield (name, value, ptype) tuples from one UDP datagram."""
    text = data.decode('latin-1')
    for line in text.split('\n'):
        line = line.strip()
        if not line:
            continue
        if line.startswith('=>'):
            line = line[2:]
        eq  = line.find('=')
        col = line.find(':')
        if eq >= 0 and (col < 0 or eq < col):
            yield (line[:eq], line[eq + 1:], 'ascii')
            continue
        if col >= 0:
            name = line[:col]
            idx  = data.find(line.encode('latin-1'))
            raw  = data[idx + col + 1:] if idx >= 0 else b''
            if len(raw) > 4:
                alen = raw[0] | (raw[1] << 8)
                if alen > 0 and len(raw) - 2 == alen * 4:
                    floats = []
                    for i in range(alen):
                        floats.append(struct.unpack('<f', raw[2 + i*4:2 + i*4 + 4])[0])
                    val = '[' + ', '.join(f'{f:.2f}' for f in floats[:8])
                    if alen > 8:
                        val += f', … ({alen} total)'
                    val += ']'
                    yield (name, val, f'bin float[{alen}]')
                    continue
            if len(raw) >= 4:
                f = struct.unpack('<f', raw[:4])[0]
                yield (name, f'{f:.2f}', 'bin float')


def _share_resolve_name(ip):
    """Best-effort DeviceName / Hostname lookup for an IP — short timeout."""
    for cmd in ('DeviceName', 'Status%205'):
        try:
            with urllib.request.urlopen(f'http://{ip}/cm?cmnd={cmd}',
                                        timeout=3) as r:
                d = json.loads(r.read() or b'{}')
                if cmd == 'DeviceName':
                    n = d.get('DeviceName') or ''
                else:
                    n = (d.get('StatusNET') or {}).get('Hostname') or ''
                if n:
                    return n
        except Exception:
            continue
    return ''


def _share_worker(duration):
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
                             socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            try:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            except Exception:
                pass
        sock.bind(('', SHARE_MCAST_PORT))
        mreq = struct.pack('4s4s',
                           socket.inet_aton(SHARE_MCAST_GRP),
                           socket.inet_aton('0.0.0.0'))
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        sock.settimeout(1.0)
    except Exception as e:
        with share_lock:
            share_state.update({'scanning': False, 'status': f'bind failed: {e}',
                                'finished': time.time()})
        if sock:
            try: sock.close()
            except Exception: pass
        return

    seen = OrderedDict()           # (src, name) -> info dict
    start = time.time()
    end   = start + duration
    while True:
        with share_lock:
            stop = share_state['stop_flag']
        if stop or time.time() >= end:
            break
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            with share_lock:
                share_state['progress'] = min(100,
                    int((time.time() - start) * 100.0 / max(0.001, duration)))
            continue
        src = addr[0]
        now = time.time()
        for name, val, ptype in _share_parse_packet(data):
            key = (src, name)
            if key in seen:
                seen[key]['value'] = val
                seen[key]['count'] += 1
                seen[key]['last'] = now
            else:
                seen[key] = {'src': src, 'name': name, 'value': val,
                             'type': ptype, 'count': 1,
                             'first': now, 'last': now}
        with share_lock:
            share_state['packets'] += 1
            share_state['progress'] = min(100,
                int((time.time() - start) * 100.0 / max(0.001, duration)))

    try: sock.close()
    except Exception: pass

    # Resolve device names per IP (parallel)
    sender_ips = sorted(set(info['src'] for info in seen.values()),
                        key=lambda ip: tuple(int(p) for p in ip.split('.')))
    names = {}
    if sender_ips:
        with share_lock:
            share_state['status'] = 'resolving device names…'
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            fut = {pool.submit(_share_resolve_name, ip): ip for ip in sender_ips}
            for f in concurrent.futures.as_completed(fut):
                ip = fut[f]
                try: names[ip] = f.result() or ''
                except Exception: names[ip] = ''

    # Detect clashes (same name emitted by >1 IP)
    by_name = {}
    for info in seen.values():
        by_name.setdefault(info['name'], set()).add(info['src'])
    clashes = sorted([{'name': n, 'sources': sorted(s)}
                       for n, s in by_name.items() if len(s) > 1],
                      key=lambda c: c['name'])

    rows = []
    for info in seen.values():
        rows.append({'src': info['src'], 'device': names.get(info['src'], ''),
                     'name': info['name'], 'value': info['value'],
                     'type': info['type'], 'count': info['count'],
                     'last_age': round(time.time() - info['last'], 1)})
    rows.sort(key=lambda r: (r['src'], r['name']))

    with share_lock:
        share_state.update({'scanning': False, 'finished': time.time(),
                            'status': f"done — {len(rows)} entries, "
                                       f"{len(clashes)} clashes",
                            'progress': 100,
                            'results': rows, 'clashes': clashes})


def _share_start(duration):
    duration = max(1, min(int(duration), 600))
    with share_lock:
        if share_state['scanning']:
            return False, 'already scanning'
        share_state.update({'scanning': True, 'started': time.time(),
                            'finished': None, 'duration': duration,
                            'progress': 0, 'packets': 0,
                            'status': f'scanning {duration}s…',
                            'results': [], 'clashes': [],
                            'stop_flag': False})
    threading.Thread(target=_share_worker, args=(duration,),
                     daemon=True).start()
    return True, None


def _share_stop():
    with share_lock:
        if not share_state['scanning']:
            return False
        share_state['stop_flag'] = True
    return True


def _share_csv():
    """Return current results as CSV (utf-8 bytes) — safe to call any time."""
    with share_lock:
        rows = list(share_state['results'])
    out = ['src,device,name,value,type,count,last_age_s']
    for r in rows:
        v = (r['value'] or '').replace('"', '""')
        out.append(f'{r["src"]},{r["device"]},"{r["name"]}","{v}",'
                   f'"{r["type"]}",{r["count"]},{r["last_age"]}')
    return ('\n'.join(out) + '\n').encode('utf-8')


# ----------------------------- flasher ------------------------------
# Serial flashing via esptool (use-if-present); OTA via Tasmota's
# HTTP /u2 web-updater. Output streams into the same big-history log.

# Isolated esptool venv (PEP 668 / "externally-managed" safe — never
# touches system Python; created on user request for non-developers).
ESPVENV = os.path.expanduser('~/.serial_monitor_venv')
_esptool_cache = None                         # None | False | python-exe
install_state = {'running': False, 'ok': None, 'error': ''}


def _venv_py():
    p = os.path.join(
        ESPVENV, 'Scripts' if sys.platform == 'win32' else 'bin',
        'python.exe' if sys.platform == 'win32' else 'python3')
    return p if os.path.isfile(p) else None


def _have_esptool(refresh=False):
    """Resolve the python interpreter that can run esptool. Cached
    (runs on every /api/poll). Prefers the isolated venv, else a
    system/PATH esptool. Returns the python path (truthy) or False."""
    global _esptool_cache
    if refresh:
        _esptool_cache = None
    if _esptool_cache is None:
        vp = _venv_py()
        if vp:
            try:
                if subprocess.run([vp, '-m', 'esptool', 'version'],
                                  capture_output=True,
                                  timeout=15).returncode == 0:
                    _esptool_cache = vp
            except Exception:
                pass
        if _esptool_cache is None:
            if shutil.which('esptool') or shutil.which('esptool.py'):
                _esptool_cache = sys.executable
            else:
                try:
                    import esptool           # noqa: F401
                    _esptool_cache = sys.executable
                except Exception:
                    _esptool_cache = False
    return _esptool_cache


def _install_esptool():
    """Create ~/.serial_monitor_venv and pip-install esptool into it.
    venv sidesteps PEP 668 'externally-managed-environment' and keeps
    the system Python pristine. Output streams to the console log."""
    if install_state['running']:
        return
    install_state.update(running=True, ok=None, error='')
    _flog('installing esptool into an isolated venv '
          '(~/.serial_monitor_venv) — one-time, ~10-30s …')
    try:
        if not _venv_py():
            r = subprocess.run([sys.executable, '-m', 'venv', ESPVENV],
                               capture_output=True, text=True,
                               timeout=120)
            if r.returncode != 0:
                raise RuntimeError('venv create: '
                                   + (r.stderr or r.stdout)[:300])
        vp = _venv_py()
        if not vp:
            raise RuntimeError('venv python missing after create')
        for args, fatal in (
                ([vp, '-m', 'pip', '-q', 'install', '--upgrade',
                  'pip'], False),
                ([vp, '-m', 'pip', 'install', 'esptool'], True)):
            _flog('$ ' + ' '.join(args[1:]))
            p = subprocess.Popen(args, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  bufsize=1)
            for ln in p.stdout:
                ln = ln.rstrip()
                if ln:
                    _flog(ln)
            if p.wait() != 0 and fatal:
                raise RuntimeError('pip install esptool failed')
        if _have_esptool(refresh=True):
            install_state.update(running=False, ok=True)
            _flog('esptool installed ✓ — serial flashing is ready')
        else:
            raise RuntimeError('esptool still not found after install')
    except Exception as e:
        install_state.update(running=False, ok=False, error=str(e))
        _flog('esptool install failed: ' + str(e), err=True)


def _flash_set(**kw):
    with fw_lock:
        flash_state.update(kw)


def _flog(line, err=False):
    add_line('err' if err else 'info', '[flash] ' + line)


def _multipart_first_file(raw, ctype):
    """Minimal multipart/form-data parser (cgi was removed in 3.13):
    return (filename, filebytes) of the first file part, or (None,b'')."""
    m = _re.search(r'boundary=(?:"([^"]+)"|([^;]+))', ctype or '')
    if not m:
        return None, b''
    bnd = ('--' + (m.group(1) or m.group(2)).strip()).encode()
    for part in raw.split(bnd):
        if b'\r\n\r\n' not in part:
            continue
        head, _, data = part.partition(b'\r\n\r\n')
        if b'filename=' not in head:
            continue
        fn = _re.search(rb'filename="([^"]*)"', head)
        return ((fn.group(1).decode('utf-8', 'replace') if fn else 'fw'),
                data[:-2] if data.endswith(b'\r\n') else data)
    return None, b''


def _classify_fw(blob):
    """Inspect an ESP firmware image and pick the serial-flash offset
    automatically (like ESP_Flasher — no manual offset needed).

    - ESP image magic is 0xE9.
    - A *factory*/merged image has the partition-table signature
      0xAA50 at file offset 0x8000  -> flash whole thing at 0x0.
    - ESP8266 images are whole-image too -> 0x0.
    - An ESP32 *app-only* image (extended header carries a known
      chip_id, no table at 0x8000) cannot be serial-flashed at a
      fixed offset (its app-partition offset lives in the device's
      partition table) -> tell the user to use the .factory.bin / OTA.
    Returns {'kind','offset'|None,'note'}."""
    if not blob or blob[0] != 0xE9:
        return {'kind': 'unknown', 'offset': None,
                'note': 'not an ESP image (no 0xE9 magic)'}
    if len(blob) > 0x8002 and blob[0x8000:0x8002] == b'\xaa\x50':
        return {'kind': 'factory', 'offset': '0x0',
                'note': 'factory/merged image (bootloader + partition '
                        'table + app) → 0x0'}
    # ESP32 app-image extended header: chip_id at bytes 12-13 (LE).
    esp32_ids = {0, 2, 4, 5, 6, 9, 12, 13, 16, 18, 20, 23}
    cid = int.from_bytes(blob[12:14], 'little') if len(blob) > 14 \
        else -1
    if cid in esp32_ids:
        names = {0: 'ESP32', 2: 'ESP32-S2', 5: 'ESP32-C3',
                 9: 'ESP32-S3', 12: 'ESP32-C2', 13: 'ESP32-C6',
                 16: 'ESP32-H2', 18: 'ESP32-P4'}
        return {'kind': 'esp32-app', 'offset': None,
                'note': '%s APP-only image — NOT serial-flashable at '
                        'a fixed offset. Use the *.factory.bin for '
                        'serial, or OTA this image.'
                        % names.get(cid, 'ESP32')}
    return {'kind': 'esp8266', 'offset': '0x0',
            'note': 'whole-image (ESP8266) → 0x0'}


def _flash_guard():
    with fw_lock:
        if flash_state['running']:
            return 'a flash job is already running'
    if not fw_path or not os.path.isfile(fw_path):
        return 'no firmware uploaded'
    return None


def _flash_serial(port, baud, offset, erase, nostub=False):
    global flash_proc
    pybin = _have_esptool()
    if not pybin:
        _flash_set(running=False, ok=False,
                   error='esptool not found')
        _flog('esptool not found — click "Install esptool" in the '
              'Flash panel, or use OTA (no esptool needed).', err=True)
        return
    _close_serial(quiet=True)                 # esptool needs the port
    _flash_set(running=True, pct=0, ok=None, error='', phase='serial')
    baud = int(baud)
    if nostub and baud > 115200:
        # The ROM loader can't reliably do the high-baud switch
        # (-> "0105: format of received message invalid" at seq 0).
        # Pin it to 115200 — slower but it actually completes.
        _flog(f'no-stub: capping baud {baud} -> 115200 for ROM '
              f'loader reliability')
        baud = 115200
    base = [pybin, '-u', '-m', 'esptool',
            '--chip', 'auto', '--port', port, '--baud', str(baud)]
    if nostub:                                # ROM loader, skips the
        base += ['--no-stub']                 # RAM-stub upload step
    steps = []
    if erase:
        steps.append(('erase', base + ['erase_flash']))
    steps.append(('write', base + ['--before', 'default_reset',
                  '--after', 'hard_reset', 'write_flash',
                  '--flash_size', 'keep', str(offset), fw_path]))
    try:
        for phase, cmd in steps:
            _flash_set(phase=phase)
            _flog(f'{phase}: ' + ' '.join(cmd[3:]))
            p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  bufsize=1)
            with fw_lock:
                flash_proc = p
            tail = ''
            for ln in p.stdout:
                ln = ln.rstrip('\r\n')
                if ln:
                    _flog(ln)
                    tail += ln + '\n'
                mm = _re.search(r'(\d+)\s*%', ln)
                if mm:
                    _flash_set(pct=int(mm.group(1)))
            rc = p.wait()
            with fw_lock:
                flash_proc = None
            if rc != 0:
                # esptool's RAM stub upload is broken on some
                # chip/port/esptool combos ("Failed to write to
                # target RAM ... Checksum error"). The ROM loader
                # (--no-stub) works — retry automatically once.
                if (not nostub and _re.search(
                        r'write to target RAM|Checksum error|'
                        r'stub', tail, _re.I)):
                    _flog('stub upload failed — retrying with '
                          '--no-stub (ROM loader, slower). If this '
                          'also fails, use the ESP32-S3 "USB '
                          'JTAG/serial debug unit" port instead of a '
                          'UART bridge.')
                    return _flash_serial(port, baud, offset, erase,
                                         nostub=True)
                _flash_set(running=False, ok=False,
                           error=f'{phase} exited {rc}')
                _flog(f'FAILED ({phase} exit {rc})', err=True)
                return
        _flash_set(running=False, ok=True, pct=100, phase='done')
        _flog('serial flash OK — device reset'
              + (' (no-stub)' if nostub else ''))
    except Exception as e:
        with fw_lock:
            flash_proc = None
        _flash_set(running=False, ok=False, error=str(e))
        _flog('ERROR ' + str(e), err=True)


def _pick_local_ip(dev_ip):
    """Our LAN IP on the device's /24 (so the device can fetch the
    firmware back from us), else the default route IP."""
    sub = dev_ip.rsplit('.', 1)[0] + '.'
    for e in _local_ips():
        if e['ip'].startswith(sub):
            return e['ip']
    return _default_ip()


def _cm(host, port, cmnd, user, password, timeout=8):
    """Tasmota /cm command → (status, text). Auth via user/password
    query params (Tasmota's scheme when WebPassword is set)."""
    import urllib.parse
    q = 'cmnd=' + urllib.parse.quote(cmnd)
    if password:
        q = ('user=%s&password=%s&'
             % (urllib.parse.quote(user or 'admin'),
                urllib.parse.quote(password))) + q
    try:
        r = _NOPROXY.open('http://%s:%d/cm?%s' % (host, port, q),
                          timeout=timeout)
        return r.status, r.read(4000).decode('utf-8', 'replace')
    except urllib.error.HTTPError as e:
        return e.code, ''
    except Exception as e:
        return 0, str(e)


def _fw_version(host, port, user, password):
    s, t = _cm(host, port, 'Status 2', user, password, timeout=4)
    try:
        return json.loads(t).get('StatusFWR', {}).get('Version', '')
    except Exception:
        return ''


def _device_mode(host, port, user, password):
    """Classify the post-OTA reachable state. Tasmota's safeboot build is
    HTTP-reachable but does NOT serve `Status 2` (returns
    `{"Command":"Unknown"}`), so the old _fw_version-only poll would
    time out even though the device is up — just stuck in the recovery
    UI waiting for a Restart click. We probe the root page once we know
    /cm is answering; the safeboot main menu identifies itself in the
    page footer (`release-safeboot` build tag) and `<title>`.

    Returns one of:
      ('normal',   version)     — full firmware, OTA succeeded
      ('safeboot', version)     — recovery partition booted (1× Restart fixes it)
      ('unreachable', None)     — no usable response yet
    """
    # /cm?cmnd=Status 2 gets us the version when in normal mode.
    s, t = _cm(host, port, 'Status 2', user, password, timeout=4)
    try:
        v = json.loads(t).get('StatusFWR', {}).get('Version', '')
        if v:
            return ('normal', v)
    except Exception:
        pass
    # /cm answered but no StatusFWR → either safeboot (`{"Command":"Unknown"}`)
    # or transient. Fetch the root page; safeboot tags itself clearly.
    if s == 200:
        try:
            r = _NOPROXY.open('http://%s:%d/' % (host, port), timeout=4)
            body = r.read(4000).decode('utf-8', 'replace').lower()
            if ('release-safeboot' in body
                    or 'safeboot' in body
                    or 'main menu' in body):
                # Try to lift a version string out of the footer
                # ("Tasmota 15.4.0(release-safeboot)") if present.
                import re
                m = re.search(r'tasmota\s+([0-9][\w.()-]*)', body)
                return ('safeboot', m.group(1) if m else '')
        except Exception:
            pass
    return ('unreachable', None)


class _FwHandler(BaseHTTPRequestHandler):
    fw = b''

    def log_message(self, *a):
        pass

    def _serve(self, body):
        self.send_response(200)
        self.send_header('Content-Type', 'application/octet-stream')
        self.send_header('Content-Length', str(len(_FwHandler.fw)))
        self.end_headers()
        if not body:
            return
        blob = _FwHandler.fw
        sent, step = 0, 16384
        while sent < len(blob):
            self.wfile.write(blob[sent:sent + step])
            sent += step
            _flash_set(pct=min(90, int(90 * sent / len(blob))),
                       phase='sending')
        _flog('firmware sent (%d B) — device now flashing/rebooting'
              % len(blob))

    def do_HEAD(self):
        self._serve(False)

    def do_GET(self):
        _flog('device pulling firmware …')
        self._serve(True)


def _flash_ota(host, user, password):
    """Robust Tasmota OTA: serve the .bin on a LAN-reachable port and
    tell the device to pull it via `OtaUrl` + `Upgrade 1`. The device
    does the ESP32 safeboot partition switch itself — works for full
    images and is language-independent (no /u2 page scraping)."""
    _flash_set(running=True, pct=0, ok=None, error='', phase='ota')
    srv = None
    try:
        with open(fw_path, 'rb') as f:
            _FwHandler.fw = f.read()
        host = host.strip()
        if ':' in host:
            h, dport = host.split(':', 1)
            dport = int(dport)
        else:
            h, dport = host, 80
        dev_ip = socket.gethostbyname(h)
        my_ip = _pick_local_ip(dev_ip)
        before = _fw_version(h, dport, user, password)

        # Temp file server on 0.0.0.0 (device must reach our LAN IP;
        # the main UI server is loopback-only). Ephemeral port.
        ThreadingHTTPServer.daemon_threads = True
        srv = ThreadingHTTPServer(('0.0.0.0', 0), _FwHandler)
        fport = srv.server_address[1]
        threading.Thread(target=srv.serve_forever, daemon=True).start()
        url = 'http://%s:%d/fw.bin' % (my_ip, fport)
        _flog('serving %s (%d B); current version: %s'
              % (url, len(_FwHandler.fw), before or '?'))

        st, txt = _cm(h, dport, 'Backlog OtaUrl %s; Upgrade 1' % url,
                      user, password, timeout=10)
        if st == 401:
            raise RuntimeError('WebPassword required/incorrect')
        if st != 200:
            raise RuntimeError('device /cm failed (%s) %s'
                               % (st, txt[:120]))
        _flog('Upgrade triggered: %s' % txt.strip()[:200])
        _flash_set(phase='upgrading')

        # Wait for it to flash & come back (safeboot dance ~ up to 2-3
        # min). Three possible outcomes:
        #   1) device reachable + Status 2 valid → 'normal' → success
        #   2) device reachable but in safeboot recovery UI → auto-trigger
        #      Restart 1 once, give it ~60 s to land in app0
        #   3) deadline → real timeout
        deadline = time.time() + 240
        time.sleep(8)
        back = ''
        restart_kicked = False
        while time.time() < deadline:
            mode, v = _device_mode(h, dport, user, password)
            if mode == 'normal':
                back = v
                break
            if mode == 'safeboot' and not restart_kicked:
                _flog('device booted into Safeboot (recovery partition, %s) '
                      '— new app0 image did not auto-start. This usually '
                      'means the 1.6.11 bootloop-guard tripped once on '
                      'first boot; the .pvs.bak safety net + safeboot '
                      'recovery are working as designed. Sending Restart 1 '
                      'to switch back to app0…' % (v or 'version unknown'))
                _cm(h, dport, 'Restart 1', user, password, timeout=6)
                restart_kicked = True
                # Push back the deadline by 60 s for the second boot.
                deadline = max(deadline, time.time() + 60)
                _flash_set(phase='post-safeboot-restart')
                time.sleep(8)
                continue
            _flash_set(pct=min(99, (flash_state.get('pct') or 90) + 1))
            time.sleep(5)
        if not back:
            # Did we at least see safeboot? Distinguish "stuck in safeboot"
            # (the new image really won't run) from "no response at all".
            final_mode, final_v = _device_mode(h, dport, user, password)
            if final_mode == 'safeboot':
                _flash_set(running=False, ok=False,
                           error='device stuck in Safeboot after Restart 1 '
                                 '— new .bin appears faulty')
                _flog('device still in Safeboot (%s) after Restart 1 — the '
                      'new app0 image is faulty and was rejected twice. '
                      'Re-flash a known-good .bin via the Safeboot Web-UI '
                      'at http://%s/' % (final_v or '?', h), err=True)
            else:
                _flash_set(running=False, ok=False,
                           error='no response after upgrade (timeout)')
                _flog('device did not come back within 240s — check it '
                      'physically', err=True)
            return
        changed = before and back and (before != back)
        _flash_set(running=False, ok=True, pct=100, phase='done')
        recovered = ' (recovered from Safeboot via auto-Restart 1)' \
            if restart_kicked else ''
        _flog('OTA OK — device back online, version %s%s%s'
              % (back, '' if not before else
                 (' (was %s)' % before if changed else
                  ' (unchanged — re-flash of same version)'),
                 recovered))
    except Exception as e:
        _flash_set(running=False, ok=False, error=str(e))
        _flog('OTA ERROR ' + str(e), err=True)
    finally:
        if srv is not None:
            try:
                srv.shutdown()
                srv.server_close()
            except Exception:
                pass


# Bypass any configured HTTP proxy for LAN device calls (a proxy
# would mis-route 192.168.x and silently break scan/devinfo while a
# browser still works).
_NOPROXY = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def _port80_open(ip):
    try:
        socket.create_connection((ip, 80), 0.6).close()
        return ip
    except OSError:
        return None


def _http_status(ip):
    """HTTP Status probe of a host already known to have :80 open.
    One retry — under server load a single attempt is flaky."""
    body = None
    for attempt in (1, 2):
        try:
            r = _NOPROXY.open('http://%s/cm?cmnd=Status' % ip,
                              timeout=4)
            body = r.read(2048).decode('utf-8', 'replace')
            break
        except urllib.error.HTTPError as e:
            # A 401 alone is NOT proof of Tasmota — any HTTP Basic-Auth
            # device (router/NAS/printer) returns one. Tasmota's /cm uses
            # query-param auth and replies 200 + {"WARNING":…}; it does not
            # Basic-Auth-401 here. So only treat a 401 as a *locked Tasmota*
            # if the body carries a Tasmota marker (JSON "WARNING" /
            # "tasmota"); a generic HTML "Authorization required" page → skip.
            if e.code != 401:
                return None
            try:
                b = e.read(1024).decode('utf-8', 'replace').lower()
            except Exception:
                b = ''
            return {'ip': ip, 'name': '(locked)'} \
                if ('warning' in b or 'tasmota' in b) else None
        except Exception:
            if attempt == 2:
                return None
            time.sleep(0.25)
    try:
        st = json.loads(body).get('Status', {})
        nm = st.get('DeviceName') \
            or (st.get('FriendlyName') or [''])[0] \
            or st.get('Topic') or ''
        return {'ip': ip, 'name': nm}
    except Exception:
        if body and ('WARNING' in body or 'tasmota' in body.lower()):
            return {'ip': ip, 'name': '(locked)'}
    return None


def _dev_partitions(host, user='admin', password=''):
    """ESP32 partition table from the Information page (/in). The data
    is embedded as `}1Partition <name>}2<n> KB (genutzt <p>%)`.
    Language-agnostic: we read the numbers, not the words. Best-effort
    — returns [] on any failure (older/ESP8266 builds have none)."""
    try:
        req = urllib.request.Request('http://%s/in' % host)
        if password:
            tok = base64.b64encode(
                ('%s:%s' % (user or 'admin', password)).encode()).decode()
            req.add_header('Authorization', 'Basic ' + tok)
        html = _NOPROXY.open(req, timeout=4).read(
            12000).decode('utf-8', 'replace')
    except Exception:
        return []
    out = []
    for m in _re.finditer(
            r'Partition ([^}]+?)\}2\s*(\d+)\s*KB'
            r'(?:[^}]*?(\d+)\s*%)?', html):
        out.append({'name': m.group(1).strip(),
                    'kb': int(m.group(2)),
                    'used': int(m.group(3)) if m.group(3) else None})
    return out


def _status(host, cmnd, user='admin', password=''):
    """One Tasmota Status query → parsed dict (top-level keys), or {} on
    failure. HTTP 401 surfaces as {'__http__':401}. Reads a generous cap;
    used for the small per-section queries that fit the device's JSON
    response buffer where a full `Status 0` can overflow and truncate."""
    q = 'cmnd=' + cmnd.replace(' ', '%20')
    if password:
        q = ('user=%s&password=%s&' % (user or 'admin', password)) + q
    try:
        r = _NOPROXY.open('http://%s/cm?%s' % (host, q), timeout=4)
        j = json.loads(r.read(16000).decode('utf-8', 'replace'))
        return j if isinstance(j, dict) else {}
    except urllib.error.HTTPError as e:
        return {'__http__': e.code}
    except Exception:
        return {}


def _dev_info(host, user='admin', password=''):
    """Tasmota status → a concise confirm-before-flash card. Tries the
    one-shot `Status 0`; if that doesn't parse (some devices — e.g. an
    Energy_Manager with a long ID list — overflow the response buffer
    and truncate the JSON mid-stream), re-assemble from the small
    per-section queries, which individually fit the buffer."""
    host = host.strip()
    j = _status(host, 'Status 0', user, password)
    if j.get('__http__') == 401 or 'WARNING' in j:
        return {'ok': False, 'locked': True}
    if 'Status' not in j or 'StatusFWR' not in j:
        j = {}
        for c in ('Status', 'Status 2', 'Status 4', 'Status 5',
                  'Status 11'):
            part = _status(host, c, user, password)
            if part.get('__http__') == 401:
                return {'ok': False, 'locked': True}
            j.update({k: v for k, v in part.items() if k != '__http__'})
        if 'StatusFWR' not in j:
            return {'ok': False, 'error': 'no parseable Status '
                    '(device response-buffer overflow?)'}
    S = j.get('Status', {})
    F = j.get('StatusFWR', {})
    N = j.get('StatusNET', {})
    T = j.get('StatusSTS', {})
    M = j.get('StatusMEM', {})
    fn = S.get('FriendlyName')

    def _mb(kb):
        try:
            kb = int(kb)
        except Exception:
            return ''
        return ('%g MB' % round(kb / 1024, 1)) if kb >= 1024 \
            else ('%d KB' % kb)
    return {'ok': True,
            'name': S.get('DeviceName')
                    or (fn[0] if isinstance(fn, list) and fn else ''),
            'topic': S.get('Topic', ''),
            'module': S.get('Module', ''),
            'version': F.get('Version', ''),
            'hardware': F.get('Hardware', ''),     # CPU / chip
            'core': F.get('Core', ''),
            'cpufreq': F.get('CpuFrequency', ''),
            'flash': _mb(M.get('FlashSize')),      # total flash chip
            'appflash': _mb(M.get('ProgramFlashSize')),
            'used': _mb(M.get('ProgramSize')),
            'free': _mb(M.get('Free')),
            'host': N.get('Hostname', ''),
            'ip': N.get('IPAddress', host),
            'mac': N.get('Mac', ''),
            'uptime': T.get('Uptime', ''),
            'rssi': (T.get('Wifi') or {}).get('RSSI', ''),
            'parts': _dev_partitions(host, user, password)}


def _scan_tasmota(net):
    """Two-phase scan of net ('192.168.1') .1-.254.

    Phase A: fast TCP connect-scan of all 254 (cheap, resilient) →
    the handful with :80 open. Phase B: HTTP Status probe ONLY those,
    low concurrency + retry. The old single-phase 80-way HTTP scan
    flaked when run inside the threaded server (GIL + poll threads),
    intermittently dropping live devices like .39."""
    net = net.strip().rstrip('.')
    if not _re.match(r'^\d{1,3}\.\d{1,3}\.\d{1,3}$', net):
        return []
    ips = ['%s.%d' % (net, i) for i in range(1, 255)]
    cf = concurrent.futures
    with cf.ThreadPoolExecutor(max_workers=64) as ex:
        live = [x for x in ex.map(_port80_open, ips) if x]
    found = []
    with cf.ThreadPoolExecutor(max_workers=12) as ex:
        for d in ex.map(_http_status, live):
            if d:
                found.append(d)
    found.sort(key=lambda d: tuple(int(x) for x in d['ip'].split('.')))
    return found


def _scan_full(net, pw=''):
    """Scan + enrich: find every Tasmota on `net`, then fetch each one's
    confirm-card details (CPU/version/flash/free/partition map) in
    parallel so the UI can show a full picker TABLE, not just names.
    Locked devices come back with {'locked':True}. Best-effort per
    device — a slow/odd one still lands in the list with what we got."""
    base = _scan_tasmota(net)
    if not base:
        return []

    def enrich(d):
        ip = d['ip']
        info = _dev_info(ip, 'admin', pw)        # Status 0 + /in partitions
        if info.get('ok'):
            info['ip'] = ip
            if not info.get('name'):
                info['name'] = d.get('name', '')
            return info
        # not ok: keep it in the list so the user still sees the device
        return {'ip': ip, 'name': d.get('name', ''),
                'ok': False, 'locked': bool(info.get('locked')),
                'error': info.get('error', '')}

    cf = concurrent.futures
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        out = list(ex.map(enrich, base))
    out.sort(key=lambda d: tuple(int(x) for x in d['ip'].split('.')))
    return out


def _flash_cancel():
    with fw_lock:
        p = flash_proc
    if p is not None:
        try:
            p.terminate()
        except Exception:
            pass
        _flog('cancelled by user', err=True)
        _flash_set(running=False, ok=False, error='cancelled')


HTML = r"""<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Serial Monitor</title>
<style>
 :root{--bg:#0e1116;--bar:#161b22;--fg:#d6deeb;--mut:#7a8aa0;--acc:#3b82f6;
       --rx:#cde7ff;--info:#9ece6a;--err:#ff6b6b}
 *{box-sizing:border-box}
 body{margin:0;font:14px/1.4 -apple-system,Segoe UI,Roboto,sans-serif;
      background:var(--bg);color:var(--fg);height:100vh;display:flex;
      flex-direction:column}
 #bar,#modebar,#monpanel,#devpanel,#sharepanel{display:flex;flex-wrap:wrap;gap:8px;
      align-items:center;padding:8px 10px;background:var(--bar);
      border-bottom:1px solid #222}
 #modebar{gap:6px}
 .tab{background:#0e1116;color:var(--mut);border:1px solid #30363d;
      border-radius:6px;padding:7px 13px;font-weight:600;cursor:pointer}
 .tab:hover{border-color:var(--acc)}
 .tab.on{background:#1f6feb;border-color:#1f6feb;color:#fff}
 select,button,input{font:13px inherit;background:#0e1116;color:var(--fg);
      border:1px solid #30363d;border-radius:5px;padding:6px 9px}
 button{cursor:pointer}
 button:hover{border-color:var(--acc)}
 button.go{background:#1f6feb;border-color:#1f6feb;color:#fff}
 button.stop{background:#aa2b2b;border-color:#aa2b2b;color:#fff}
 #stat{margin-left:auto;color:var(--mut);font-size:12px;white-space:nowrap}
 #dot{display:inline-block;width:8px;height:8px;border-radius:50%;
      background:#555;margin-right:5px;vertical-align:middle}
 #dot.on{background:#3fb950}
 label{color:var(--mut);font-size:12px}
 #log{flex:1;overflow:auto;padding:6px 10px;white-space:pre-wrap;
      word-break:break-word;font:12px/1.45 ui-monospace,Menlo,Consolas,
      monospace}
 .l{display:flex;gap:8px}
 .l .t{color:#586274;flex:none;-webkit-user-select:none;
       user-select:none}
 .rx{color:var(--rx)} .info{color:var(--info)} .err{color:var(--err)}
 .tx{color:#f0c674} .tx::before{content:"» "}
 .l .s{color:#6b7e9b;flex:none;-webkit-user-select:none;
       user-select:none}
 #log.nosrc .s{display:none}
 #sport{width:64px}
 #cmd{flex:1;min-width:120px}
 #cmdbar{display:flex;gap:8px;align-items:center;flex-wrap:wrap;
      padding:8px 10px;background:var(--bar);border-top:1px solid #222}
 .hide{display:none!important}
 #fwpct{width:160px;height:14px;accent-color:var(--acc)}
 #devpanel .grp{display:flex;gap:8px;align-items:center}
 #host{min-width:150px}
 #devinfo{flex-basis:100%;font:12px ui-monospace,Menlo,monospace;
      color:var(--mut);padding:2px 0 0}
 #devinfo b{color:var(--fg)}
 #devinfo.ok b{color:var(--info)}
 #devinfo.bad{color:var(--err)}
 #devinfo .parts{margin-top:3px;color:var(--mut);font-size:11px}
 #devinfo .parts b{color:var(--fg)}
 #scanwrap{flex-basis:100%;overflow:auto;max-height:240px;margin-top:4px;
      border:1px solid #222;border-radius:6px}
 #scantbl{border-collapse:collapse;width:100%;
      font:11px/1.4 ui-monospace,Menlo,Consolas,monospace}
 #scantbl th,#scantbl td{padding:3px 7px;text-align:left;
      border-bottom:1px solid #1c2230;white-space:nowrap;vertical-align:top}
 #scantbl th{color:var(--mut);position:sticky;top:0;background:var(--bar);
      z-index:1;font-weight:600}
 #scantbl tbody tr{cursor:pointer}
 #scantbl tbody tr:nth-child(even){background:#12161d}
 #scantbl tbody tr:hover{background:#1b2230}
 #scantbl tbody tr.sel{background:#173049;
      box-shadow:inset 2px 0 0 var(--acc)}
 #scantbl b{color:var(--fg)}
 #scantbl td.parts{white-space:normal;color:var(--mut);
      max-width:320px;font-size:10px}
 #scantbl .lk{color:var(--err)}
 #scantbl a.iplink{color:var(--acc);text-decoration:none;font-weight:600}
 #scantbl a.iplink:hover{text-decoration:underline}
 /* CPU-family colour badges */
 .cpu{display:inline-block;padding:0 6px;border-radius:9px;font-size:10px;
      font-weight:700;border:1px solid transparent}
 .cpu-8266{color:#e0a84e;background:#2e2410;border-color:#4a3a18}
 .cpu-32  {color:#7ab0ff;background:#15233b;border-color:#28456e}
 .cpu-s2  {color:#5fd0c8;background:#103230;border-color:#1d5450}
 .cpu-s3  {color:#87d96b;background:#162c12;border-color:#2c4f22}
 .cpu-c3  {color:#b88bf0;background:#261a3a;border-color:#432f63}
 .cpu-c6  {color:#5cc8e6;background:#0f2a33;border-color:#1d4a59}
 /* free-heap health + partition fill */
 .free-ok{color:#87d96b} .free-warn{color:#e0a84e} .free-low{color:var(--err);font-weight:700}
 .pu-warn{color:#e0a84e} .pu-hi{color:var(--err);font-weight:700}
 #scantbl button{padding:2px 9px;font-size:11px}
 #scantbl .rn{cursor:pointer;color:#5a6b86;margin-left:3px;
      -webkit-user-select:none;user-select:none}
 #scantbl .rn:hover{color:var(--acc)}
 /* Share-monitor results table — same style as scan table */
 #sharemain{flex:1;display:flex;flex-direction:column;overflow:auto;
   padding:4px 10px;gap:6px}
 #shclash{padding:6px 10px;border:1px solid #5a2424;background:#2c1414;
   border-radius:4px;color:#ffb6b6;font:11px/1.5 ui-monospace,Menlo,monospace}
 #shclash b{color:#ff8a8a}
 #shwrap{flex:1;overflow:auto;border:1px solid #222;border-radius:6px}
 #shtbl{border-collapse:collapse;width:100%;
   font:11px/1.4 ui-monospace,Menlo,Consolas,monospace}
 #shtbl th,#shtbl td{padding:3px 7px;text-align:left;
   border-bottom:1px solid #1c2230;white-space:nowrap;vertical-align:top}
 #shtbl th{color:var(--mut);position:sticky;top:0;background:var(--bar);
   z-index:1;font-weight:600}
 #shtbl tbody tr{border-left:3px solid transparent}     /* per-source accent */
 #shtbl tbody tr.grp-start{border-top:2px solid #2a3340} /* visible break between source groups */
 #shtbl tbody tr.grp-start:first-child{border-top:0}
 #shtbl tbody tr.clash{background:#2c1414}
 #shtbl tbody tr:not(.clash):hover{background:#1b2230}
 /* Eye-flow per row (left to right):
    [src-color stripe] · device/IP (src-color) · GLOBAL NAME (warm gold,
    the headline) · last value (sky blue) · type/count/age (legible
    mid-tones, not graveyard grey). */
 #shtbl b{color:#facc15;font-weight:600}                 /* Global name — warm gold */
 #shtbl td.dev{font-weight:600;max-width:160px;overflow:hidden;
   text-overflow:ellipsis;white-space:nowrap}
 #shtbl td.src{font-family:ui-monospace,Menlo,Consolas,monospace}
 #shtbl td.val{color:#7ab0ff;max-width:340px;overflow:hidden;
   text-overflow:ellipsis;white-space:nowrap}
 #shtbl td.type{color:#9aa9bf;font-size:10px}            /* was var(--mut) #7a8aa0 — too low contrast */
 #shtbl td.cnt {color:#b8c2d4;text-align:right}          /* mid-tone, readable but subordinate */
 #shtbl td.age {color:#b8c2d4;text-align:right}
 #log.notime .t{display:none}
</style></head><body>
<div id="modebar">
  <button id="modeMon" class="tab on" title="Serial / UDP-syslog console">📟 Monitor</button>
  <button id="modeDev" class="tab" title="Scan the LAN · OTA-flash · rename devices">🔧 Devices · Scan &amp; OTA</button>
  <button id="modeShare" class="tab" title="Tasmota multicast share-protocol monitor (239.255.255.250:1999)">🛰 Shares</button>
  <span style="width:1px;height:22px;background:#30363d;margin:0 4px"></span>
  <label title="This PC's LAN IP — used as the Tasmota LogHost target (Monitor) AND as the subnet to scan (Devices)">IP</label>
  <select id="hostip" title="this PC's LAN IP — LogHost target / scan subnet; click 📋 to copy"></select>
  <button id="copyip" title="copy IP for Tasmota LogHost">📋</button>
  <button id="quit" class="stop" title="Stop the server process">Quit</button>
  <span id="stat"><span id="dot"></span><span id="msg">idle</span></span>
</div>
<div id="monpanel">
  <label>Port</label>
  <select id="port" style="min-width:190px"></select>
  <button id="refresh" title="Rescan serial ports">⟳</button>
  <label>Baud</label>
  <select id="baud"></select>
  <button id="conn" class="go">Connect</button>
  <span style="width:1px;height:22px;background:#30363d;margin:0 2px"></span>
  <label title="UDP syslog listener for remote devices. In Tasmota set LogHost &lt;this PC IP&gt;, LogPort, SysLog 2.">Syslog</label>
  <input id="sport" type="number" min="1" max="65535" value="514"
    title="UDP port. <1024 needs root; use e.g. 5514 and Tasmota LogPort 5514.">
  <button id="syslog" class="go">Listen</button>
  <span style="width:1px;height:22px;background:#30363d;margin:0 2px"></span>
  <button id="clear">Clear</button>
  <button id="save">Save</button>
  <label style="margin-left:6px"><input type="checkbox" id="auto" checked
    style="vertical-align:middle"> autoscroll</label>
  <label><input type="checkbox" id="ts" checked
    style="vertical-align:middle"> time</label>
  <label><input type="checkbox" id="hex"
    style="vertical-align:middle"> hex</label>
  <label title="show the syslog sender IP per line"><input
    type="checkbox" id="srcck" checked
    style="vertical-align:middle"> src</label>
</div>
<div id="devpanel" class="hide">
  <label>Firmware</label>
  <input type="file" id="fwfile" accept=".bin">
  <span id="fwinfo" style="color:var(--mut);font-size:12px">no file</span>
  <label>Mode</label>
  <select id="fmode">
    <option value="ota" selected>OTA (Tasmota /u2)</option>
    <option value="serial">Serial (esptool)</option>
  </select>
  <span id="gSerial" class="grp hide">
    <label>Port</label>
    <select id="fport" style="min-width:170px"></select>
    <button id="frefresh" title="Rescan serial ports">⟳</button>
    <label>Baud</label>
    <select id="fbaud">
      <option>115200</option><option selected>460800</option>
      <option>921600</option><option>230400</option>
    </select>
    <label title="Auto-detected from the image when you pick a file; override only if you know better">Offset</label>
    <input id="foffset" value="0x0" style="width:74px"
      title="Auto-set from the firmware (factory/ESP8266 → 0x0). Manual override possible.">
    <label><input type="checkbox" id="ferase"> erase</label>
    <label title="ESP32-S3/C3: use the ROM loader, skip the RAM stub upload — fixes 'Failed to write to target RAM / Checksum error'"><input type="checkbox" id="fnostub"> no-stub</label>
    <span style="color:var(--mut);font-size:12px">for ESP32-S3 prefer
      the &ldquo;USB JTAG/serial debug unit&rdquo; port</span>
    <span id="noesptool" class="hide" style="color:var(--err);
      font-size:12px">esptool not installed —
      <button id="espinstall" style="padding:3px 8px">Install esptool
      (isolated)</button>
      <a href="https://tasmota.github.io/install/" target="_blank"
        style="color:#64b4ff">or use the Tasmota Web Installer ↗</a>
      <span style="color:var(--mut)">· no esptool needed for
      <b>OTA</b> (device already on WiFi)</span></span>
  </span>
  <span id="gOta" class="grp">
    <label>Device</label>
    <select id="hostsel" title="Tasmota devices found on the LAN"
      style="min-width:170px"><option value="">— scan the LAN —</option></select>
    <button id="hostscan" title="Scan this subnet for Tasmota devices">⟳ Scan</button>
    <input id="host" placeholder="or type 192.168.x.x / name"
      autocomplete="off" style="min-width:150px">
    <input id="fwpass" type="password" placeholder="WebPassword (if set)"
      style="width:150px" autocomplete="off">
  </span>
  <button id="flashgo" class="go">Flash</button>
  <button id="flashcancel" class="stop hide">Cancel</button>
  <progress id="fwpct" max="100" value="0" class="hide"></progress>
  <span id="fwstat" style="color:var(--mut);font-size:12px"></span>
  <div id="devinfo"></div>
  <div id="scanwrap" class="hide"><table id="scantbl">
    <thead><tr><th>IP</th><th>Name</th><th>Hostname</th><th>CPU</th>
      <th>Tasmota</th><th>Flash</th><th>Free</th><th>Partitions</th>
      <th></th></tr></thead>
    <tbody id="scanbody"></tbody></table></div>
</div>
<div id="sharepanel" class="hide">
  <label title="Listen on multicast 239.255.255.250:1999 for this many seconds">Duration</label>
  <input id="shdur" type="number" min="1" max="600" value="30" style="width:78px">
  <button id="shstart" class="go">Start</button>
  <button id="shstop" class="stop hide">Stop</button>
  <span id="shstat" style="color:var(--mut);font-size:12px"></span>
  <progress id="shpct" max="100" value="0" class="hide" style="width:120px"></progress>
  <span style="flex:1"></span>
  <button id="shcsv" title="Download the current results as CSV" disabled>Export CSV</button>
</div>
<div id="log" class="" tabindex="0"></div>
<div id="sharemain" class="hide">
  <div id="shclash" class="hide"></div>
  <div id="shwrap"><table id="shtbl">
    <thead><tr><th>Device</th><th>IP</th><th>Global</th><th>Last value</th>
      <th>Type</th><th>Count</th><th>Age (s)</th></tr></thead>
    <tbody id="shbody"></tbody></table></div>
</div>
<div id="cmdbar">
  <input id="cmd" placeholder="type a command, Enter to send"
    autocomplete="off" disabled>
  <select id="eol" title="line ending appended to the command">
    <option value="crlf" selected>CR LF</option>
    <option value="lf">LF</option>
    <option value="cr">CR</option>
    <option value="">none</option>
  </select>
  <button id="send" disabled>Send</button>
</div>
<script>
const $=s=>document.querySelector(s);
const logEl=$('#log'), portEl=$('#port'), baudEl=$('#baud'),
      connEl=$('#conn'), msgEl=$('#msg'), dotEl=$('#dot'),
      cmdEl=$('#cmd'), sendEl=$('#send'), eolEl=$('#eol'),
      syslogEl=$('#syslog'), sportEl=$('#sport'),
      hostipEl=$('#hostip'), copyipEl=$('#copyip'),
      fwfileEl=$('#fwfile'), fmodeEl=$('#fmode'),
      flashgoEl=$('#flashgo'), flashcancelEl=$('#flashcancel'),
      fwpctEl=$('#fwpct'), fwstatEl=$('#fwstat'), fwinfoEl=$('#fwinfo');
let cmdHist=[], histIdx=0, prefsApplied=false, syslogOn=false,
    fwReady=false, flashing=false, fwName='', fwKind='', fwOffset='';
let connected=false, since=0, total=0, dropped=0, pollTimer=null,
    MAXDOM=50000;   // keep huge but bounded scrollback in the DOM

[300,1200,2400,4800,9600,19200,38400,57600,74880,115200,230400,
 460800,921600,1500000].forEach(b=>{
  const o=document.createElement('option');o.value=b;o.textContent=b;
  if(b===115200)o.selected=true;baudEl.appendChild(o);});

async function loadPorts(){
  try{
    const r=await fetch('/api/ports'); const ps=await r.json();
    const keep=portEl.value;
    portEl.innerHTML='';
    if(!ps.length){
      const o=document.createElement('option');
      o.textContent='(no serial ports — is pyserial installed?)';
      o.value='';portEl.appendChild(o);
    }
    ps.forEach(p=>{const o=document.createElement('option');
      o.value=p.device;
      o.textContent=p.device+(p.desc?'  —  '+p.desc:'');
      portEl.appendChild(o);});
    if(keep)portEl.value=keep;
    const fpEl=$('#fport');               // mirror the same ports into the
    if(fpEl){ const k=fpEl.value;         // serial-flash Port picker (Devices)
      fpEl.innerHTML=portEl.innerHTML;
      if(k&&[...fpEl.options].some(o=>o.value===k)) fpEl.value=k; }
    if(!prefsApplied){
      prefsApplied=true;
      try{
        const pr=await(await fetch('/api/prefs')).json();
        if(pr.device&&[...portEl.options].some(o=>o.value===pr.device))
          portEl.value=pr.device;
        if(pr.baud&&[...baudEl.options].some(o=>+o.value===+pr.baud))
          baudEl.value=pr.baud;
        if(pr.sport) sportEl.value=pr.sport;
      }catch(e){}
    }
  }catch(e){
    portEl.innerHTML='';
    const o=document.createElement('option');
    o.textContent='(port list failed: '+e+')';o.value='';
    portEl.appendChild(o);
  }
}

const _enc=new TextEncoder();
function toHex(s){return Array.from(_enc.encode(s),
  b=>b.toString(16).padStart(2,'0')).join(' ');}
function hexMode(){return $('#hex').checked;}
function render(span){
  const h=hexMode();
  span.textContent = h ? (span.dataset.h || toHex(span.dataset.x||''))
                       : (span.dataset.x||'');
}

function setStat(on,t){dotEl.classList.toggle('on',on);msgEl.textContent=t;}

function append(lines){
  if(!lines.length)return;
  const atBottom = logEl.scrollHeight-logEl.scrollTop-logEl.clientHeight<40;
  const frag=document.createDocumentFragment();
  for(const ln of lines){
    const d=document.createElement('div');d.className='l';
    const t=document.createElement('span');t.className='t';t.textContent=ln.t;
    d.appendChild(t);
    if(ln.src){const s=document.createElement('span');s.className='s';
      s.textContent=ln.src;d.appendChild(s);}
    const x=document.createElement('span');x.className=ln.d;
    x.dataset.x=ln.x; if(ln.h)x.dataset.h=ln.h; render(x);
    d.appendChild(x);frag.appendChild(d);
  }
  logEl.appendChild(frag);
  // trim DOM to MAXDOM lines (server keeps the full history for Save)
  let over=logEl.childElementCount-MAXDOM;
  while(over-->0 && logEl.firstChild) logEl.removeChild(logEl.firstChild);
  if($('#auto').checked && atBottom) logEl.scrollTop=logEl.scrollHeight;
}

async function poll(){
  try{
    const r=await fetch('/api/poll?since='+since);
    const j=await r.json();
    since=j.seq; total=j.total;
    connected=j.open; syslogOn=!!j.syslog;
    const parts=[];
    if(j.open) parts.push('● serial '+(j.dev||'')+' @ '+j.baud);
    if(j.syslog) parts.push('◆ syslog UDP '+j.sport);
    if(!j.open&&!j.syslog) parts.push('○ idle');
    setStat(j.open||j.syslog, parts.join('  ')
                    +'  |  '+total+' lines  '+
                    (j.rx_bytes||0)+' B'+
                    (j.dropped?('  ('+j.dropped+' rolled off)'):''));
    connEl.textContent=j.open?'Disconnect':'Connect';
    connEl.className=j.open?'stop':'go';
    syslogEl.textContent=j.syslog?'Stop':'Listen';
    syslogEl.className=j.syslog?'stop':'go';
    if(j.syslog&&+sportEl.value!==+j.sport) sportEl.value=j.sport;
    cmdEl.disabled=sendEl.disabled=!j.open;
    updateFlash(j);
    if(j.lines&&j.lines.length) append(j.lines);
  }catch(e){ setStat(false,'server unreachable'); }
}

connEl.onclick=async()=>{
  if(connected){ await fetch('/api/close',{method:'POST'}); }
  else{
    const dev=portEl.value;
    if(!dev){alert('Select a serial port');return;}
    const r=await fetch('/api/open',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({device:dev,baud:+baudEl.value})});
    const j=await r.json();
    if(!j.ok) alert('Open failed: '+(j.error||'unknown'));
  }
  poll();
};
async function sendCmd(){
  const t=cmdEl.value;
  if(!t.length){return;}
  cmdEl.value='';
  cmdHist.push(t); histIdx=cmdHist.length;
  try{
    const r=await fetch('/api/send',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({text:t,eol:eolEl.value})});
    const j=await r.json();
    if(!j.ok) setStat(connected,'send failed: '+(j.error||'?'));
  }catch(e){ setStat(false,'server unreachable'); }
  poll();
}
sendEl.onclick=sendCmd;
cmdEl.addEventListener('keydown',e=>{
  if(e.key==='Enter'){e.preventDefault();sendCmd();}
  else if(e.key==='ArrowUp'){
    if(histIdx>0){histIdx--;cmdEl.value=cmdHist[histIdx]||'';}
    e.preventDefault();
  }else if(e.key==='ArrowDown'){
    if(histIdx<cmdHist.length){histIdx++;
      cmdEl.value=cmdHist[histIdx]||'';}
    e.preventDefault();
  }
});
async function loadHost(){
  try{
    const j=await(await fetch('/api/host')).json();
    hostipEl.innerHTML='';
    (j.ips||[]).forEach(e=>{const o=document.createElement('option');
      o.value=e.ip;
      o.textContent=e.ip+(e.label?'  ('+e.label+')':'')
        +(e.default?'  ✓ default':'');
      if(e.default)o.selected=true;
      hostipEl.appendChild(o);});
  }catch(e){}
}
async function copyIp(){
  const ip=hostipEl.value; if(!ip)return;
  try{ await navigator.clipboard.writeText(ip); }
  catch(e){ hostipEl.focus(); }
  const o=copyipEl.textContent; copyipEl.textContent='✓';
  setTimeout(()=>copyipEl.textContent=o,900);
}
copyipEl.onclick=copyIp;
syslogEl.onclick=async()=>{
  if(syslogOn){ await fetch('/api/syslog',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({action:'stop'})}); }
  else{
    const r=await fetch('/api/syslog',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({action:'start',port:+sportEl.value})});
    const j=await r.json();
    if(!j.ok) alert('Syslog: '+(j.error||'failed'));
  }
  poll();
};
$('#srcck').onchange=e=>logEl.classList.toggle('nosrc',!e.target.checked);

// ---- mode switch (Monitor  |  Devices · Scan & OTA  |  Shares) ----
function setMode(m){
  const dev   = (m==='dev');
  const share = (m==='share');
  const mon   = (!dev && !share);
  $('#monpanel').classList.toggle('hide', !mon);
  $('#devpanel').classList.toggle('hide', !dev);
  $('#sharepanel').classList.toggle('hide', !share);
  $('#cmdbar').classList.toggle('hide', !mon);           // command input = Monitor only
  $('#log').classList.toggle('hide', share);             // log area hidden under Shares
  $('#sharemain').classList.toggle('hide', !share);      // share table only under Shares
  $('#modeMon').classList.toggle('on',   mon);
  $('#modeDev').classList.toggle('on',   dev);
  $('#modeShare').classList.toggle('on', share);
  if(dev && $('#host').value.trim()) loadDevInfo();
  if(share) sharePoll();                                  // refresh state on entry
}
$('#modeMon').onclick   = ()=>setMode('mon');
$('#modeDev').onclick   = ()=>setMode('dev');
$('#modeShare').onclick = ()=>setMode('share');

// ---- share-protocol monitor ----
function escHtml(s){return String(s==null?'':s)
  .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
  .replace(/"/g,'&quot;');}
// Stable per-source accent color: hash the IP into a hue, then pick a fixed
// pleasant saturation/lightness so colors are deterministic AND legible on the
// dark bg. Same source → same color forever, including across reloads.
function srcColor(src){
  let h=0; for(let i=0;i<src.length;i++) h=((h*31)+src.charCodeAt(i))>>>0;
  return 'hsl('+(h%360)+',55%,65%)';
}
let _sharePoll = null;
async function sharePoll(){
  try {
    const r = await fetch('/api/share/status');
    const s = await r.json();
    $('#shstat').textContent = s.status || 'idle';
    $('#shpct').classList.toggle('hide', !s.scanning);
    $('#shpct').value = s.progress || 0;
    $('#shstart').classList.toggle('hide',  s.scanning);
    $('#shstop' ).classList.toggle('hide', !s.scanning);
    $('#shcsv'  ).disabled = !(s.results && s.results.length);
    // Clash box
    const clash = $('#shclash');
    if(s.clashes && s.clashes.length){
      clash.classList.remove('hide');
      clash.innerHTML = '<b>'+s.clashes.length+' clash(es)</b> — same global emitted by &gt;1 device: '
        + s.clashes.map(c => escHtml(c.name)+' ('+c.sources.join(', ')+')').join('  ·  ');
    } else { clash.classList.add('hide'); clash.textContent=''; }
    // Rows — grouped by source IP, each source gets a stable hash-derived
    // accent color (left border + IP/Device text) so a scan of column 2 makes
    // the source obvious even before reading the text.
    const tb = $('#shbody');
    const clashNames = new Set((s.clashes||[]).map(c=>c.name));
    const rows = s.results||[];
    let prevSrc = null;
    tb.innerHTML = rows.map(r => {
      const c = srcColor(r.src);
      const newGroup = (r.src !== prevSrc); prevSrc = r.src;
      let cls = clashNames.has(r.name) ? 'clash' : '';
      if (newGroup) cls = cls ? cls + ' grp-start' : 'grp-start';
      return '<tr'+(cls?' class="'+cls+'"':'')
        + ' style="border-left-color:'+c+'">'
        + '<td class="dev" style="color:'+c+'">'+escHtml(r.device||'')+'</td>'
        + '<td class="src" style="color:'+c+'">'+escHtml(r.src)+'</td>'
        + '<td><b>'+escHtml(r.name)+'</b></td>'
        + '<td class="val" title="'+escHtml(r.value)+'">'+escHtml(r.value)+'</td>'
        + '<td class="type">'+escHtml(r.type)+'</td>'
        + '<td class="cnt">'+r.count+'</td>'
        + '<td class="age">'+r.last_age+'</td>'
        + '</tr>';
    }).join('');
    // Auto-repoll while scanning, then once more 1 s after done.
    if(s.scanning) _sharePoll = setTimeout(sharePoll, 1000);
    else { clearTimeout(_sharePoll); _sharePoll = null; }
  } catch(e){ /* server gone — leave UI as-is */ }
}
$('#shstart').onclick = async ()=>{
  const dur = Math.max(1, Math.min(600, +$('#shdur').value || 30));
  await fetch('/api/share/start',{method:'POST',headers:{'Content-Type':'application/json'},
                                   body:JSON.stringify({duration: dur})});
  sharePoll();
};
$('#shstop').onclick  = async ()=>{
  await fetch('/api/share/stop',{method:'POST'});
  sharePoll();
};
$('#shcsv').onclick   = ()=>{ window.location = '/api/share/csv'; };

// ---- flasher ----
fmodeEl.onchange=()=>{
  const ota=fmodeEl.value==='ota';
  $('#gOta').classList.toggle('hide',!ota);
  $('#gSerial').classList.toggle('hide',ota);
};
fwfileEl.onchange=async()=>{
  const f=fwfileEl.files[0]; if(!f)return;
  fwinfoEl.textContent='uploading '+f.name+' …';
  const fd=new FormData(); fd.append('fw',f,f.name);
  try{
    const j=await(await fetch('/api/fw',{method:'POST',body:fd})).json();
    if(j.ok){fwReady=true; fwName=j.name||'';
      fwKind=j.kind||''; fwOffset=j.offset||'';
      if(fwOffset) $('#foffset').value=fwOffset;
      fwinfoEl.textContent=j.name+'  ('+j.size.toLocaleString()+' B)  — '
        +(j.note||'');
      fwinfoEl.style.color=(fwKind==='esp32-app'||fwKind==='unknown')
        ?'var(--err)':'var(--info)';}
    else{fwinfoEl.textContent='upload failed: '+(j.error||'?');}
  }catch(e){fwinfoEl.textContent='upload error: '+e;}
};
flashgoEl.onclick=async()=>{
  if(!fwReady){alert('Choose a firmware .bin first');return;}
  let url,bdy;
  if(fmodeEl.value==='ota'){
    const host=$('#host').value.trim();
    if(!host){alert('Enter the device IP/hostname');return;}
    if(!confirm('OTA-flash this device?\n\n'
      +(devinfoEl.textContent||host)
      +'\n\nThis overwrites its firmware.'))return;
    url='/api/flash/ota';
    bdy={host,user:'admin',password:$('#fwpass').value};
  }else{
    const fpEl=$('#fport');
    const port=fpEl.value;
    if(!port){alert('Select the serial Port in the Serial-flash group '
      +'first');return;}
    // Auto-detected image type decides the offset (no manual entry).
    if(fwKind==='esp32-app'){
      alert('This is an ESP32 APP-only image — it can NOT be '
        +'serial-flashed at a fixed offset (its app-partition offset '
        +'lives in the device partition table).\n\nUse the matching '
        +'*.factory.bin for serial flashing, or switch Mode to OTA '
        +'to push this image.');
      return;
    }
    if(fwKind==='unknown'){
      if(!confirm('Could not recognise this file as an ESP image. '
        +'Flash it anyway at '+$('#foffset').value+'?'))return;
    }
    const offset=fwOffset||$('#foffset').value;
    const warn=[];
    const sel=fpEl.options[fpEl.selectedIndex];
    const txt=(sel&&sel.textContent)||'';
    const hasJtag=[...fpEl.options].some(o=>/JTAG/i.test(o.textContent));
    if(!/JTAG/i.test(txt)&&hasJtag)
      warn.push('• For ESP32-S3 the "USB JTAG/serial debug unit" port '
        +'is far more reliable — a UART bridge often fails esptool\'s '
        +'stub upload (Checksum error). Consider selecting it.');
    if(!confirm('Serial-flash '+port+'?\n\nfile: '+fwName
      +'\noffset: '+offset+' (auto-detected: '+(fwKind||'?')+')'
      +'\n\nThis closes the monitor on that port and overwrites the '
      +'device (settings/filesystem may be lost).'
      +(warn.length?'\n\n⚠ '+warn.join('\n\n⚠ '):'')
      +'\n\nProceed?'))return;
    url='/api/flash/serial';
    bdy={port,baud:+$('#fbaud').value,offset:offset,
         erase:$('#ferase').checked,nostub:$('#fnostub').checked};
  }
  const j=await(await fetch(url,{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(bdy)})).json();
  if(!j.ok) alert('Flash: '+(j.error||'failed'));
  poll();
};
flashcancelEl.onclick=()=>fetch('/api/flash/cancel',{method:'POST'});
document.addEventListener('click',e=>{
  if(e.target&&e.target.id==='espinstall'){
    e.target.disabled=true;
    fetch('/api/esptool/install',{method:'POST'}).catch(()=>{});
  }
});
const hostselEl=$('#hostsel'), hostscanEl=$('#hostscan'),
      devinfoEl=$('#devinfo');
let devTimer=null;
async function loadDevInfo(){
  const host=$('#host').value.trim();
  if(!host){devinfoEl.textContent='';devinfoEl.className='';return;}
  devinfoEl.className='';devinfoEl.textContent='querying '+host+' …';
  try{
    const u='/api/devinfo?host='+encodeURIComponent(host)
      +'&pw='+encodeURIComponent($('#fwpass').value||'');
    const d=await(await fetch(u)).json();
    if(d.ok){
      devinfoEl.className='ok';
      const flash=d.flash?(d.flash+' flash'
        +(d.appflash?' (app '+d.appflash
          +(d.free?', free '+d.free:'')+')':'')):'';
      devinfoEl.innerHTML='✓ <b>'+(d.name||d.host||host)+'</b>'
        +(d.hardware?' · <b>'+d.hardware+'</b>':'')
        +(d.cpufreq?' @'+d.cpufreq+'MHz':'')
        +(flash?' · '+flash:'')
        +(d.version?' · Tasmota '+d.version
            +(d.core?' ('+d.core+')':''):'')
        +' · '+(d.ip||host)+(d.mac?' · '+d.mac:'')
        +(d.uptime?' · up '+d.uptime:'')
        +(d.rssi!==''?' · RSSI '+d.rssi+'%':'')
        +'  — confirm this is the device you want to overwrite';
      if(d.parts&&d.parts.length){
        const fmt=k=>k>=1024?(Math.round(k/102.4)/10+' MB'):(k+' KB');
        devinfoEl.innerHTML+='<div class="parts">'+d.parts.map(p=>{
          const u=p.used!=null?' <span'+(p.used>=90
            ?' style="color:var(--err)"':'')+'>'+p.used+'%</span>':'';
          return '<b>'+p.name+'</b> '+fmt(p.kb)+u;
        }).join('  ·  ')+'</div>';
      }
    }else if(d.locked){
      devinfoEl.className='bad';
      devinfoEl.textContent='🔒 '+host+' needs its WebPassword '
        +'(enter it, then it re-checks)';
    }else{
      devinfoEl.className='bad';
      devinfoEl.textContent='✗ '+host+' — '+(d.error||'no response '
        +'(not a Tasmota device?)');
    }
  }catch(e){devinfoEl.className='bad';
    devinfoEl.textContent='✗ '+host+' — '+e;}
}
function devInfoSoon(ms){clearTimeout(devTimer);
  devTimer=setTimeout(loadDevInfo,ms||450);}
hostselEl.onchange=()=>{ if(hostselEl.value){
  $('#host').value=hostselEl.value; loadDevInfo(); } };
$('#host').addEventListener('input',()=>devInfoSoon());
$('#fwpass').addEventListener('change',loadDevInfo);
fmodeEl.addEventListener('change',()=>{ if(fmodeEl.value==='ota')
  loadDevInfo(); else {devinfoEl.textContent='';devinfoEl.className='';}});
const scanwrapEl=$('#scanwrap'), scanbodyEl=$('#scanbody');
function _fmtKB(k){return k>=1024?(Math.round(k/102.4)/10+' MB'):(k+' KB');}
function _esc(s){return String(s==null?'':s).replace(/[&<>"]/g,
  c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));}
function _nameCell(d){return '<b>'+_esc(d.name)+'</b> <span class="rn" '
  +'data-f="devicename" title="rename device name (instant)">✎</span>';}
function _hostCell(d){return _esc(d.host)+' <span class="rn" '
  +'data-f="hostname" title="rename hostname (reboots the device)">✎</span>';}
function _ipCell(d){const ip=_esc(d.ip);return '<a class="iplink" href="http://'+ip
  +'" target="_blank" rel="noopener" title="open http://'+ip+' in a browser tab" '
  +'onclick="event.stopPropagation()">'+ip+'</a>';}
function _cpuCell(d){
  const hw=d.hardware||'', u=hw.toUpperCase();
  let cls='cpu-32';
  if(u.includes('8266'))      cls='cpu-8266';
  else if(u.includes('C6'))   cls='cpu-c6';
  else if(u.includes('C2')||u.includes('C3')) cls='cpu-c3';
  else if(u.includes('S2'))   cls='cpu-s2';
  else if(u.includes('S3'))   cls='cpu-s3';
  const pill=hw?'<span class="cpu '+cls+'">'+_esc(hw)+'</span>':'';
  return pill+(d.cpufreq?' <span style="color:var(--mut)">@'+_esc(d.cpufreq)+'</span>':'');
}
function _freeCell(d){
  const s=d.free||''; if(!s) return '';
  const m=s.match(/([\d.]+)\s*(KB|MB)/i); let cls='';
  if(m){let kb=parseFloat(m[1]); if(/MB/i.test(m[2])) kb*=1024;
    cls = kb<20?'free-low':(kb<45?'free-warn':'free-ok');}
  return '<span class="'+cls+'">'+_esc(s)+'</span>';
}
function selectDevice(ip,tr){
  $('#host').value=ip;
  [...scanbodyEl.children].forEach(r=>r.classList.remove('sel'));
  if(tr) tr.classList.add('sel');
  loadDevInfo();
}
function renderScanTable(devs){
  scanbodyEl.innerHTML='';
  if(!devs||!devs.length){
    scanbodyEl.innerHTML='<tr><td colspan="9" style="color:var(--mut);'
      +'padding:9px;white-space:normal">No Tasmota devices answered on '
      +'this subnet.<br>• Check the <b>LogHost</b> IP (top bar) is on the '
      +'same LAN as your devices.<br>• If you opened <b>Serial Monitor.app</b> '
      +'from Finder, macOS may be blocking its LAN access — enable it under '
      +'<b>System Settings → Privacy &amp; Security → Local Network</b> '
      +'(Serial Monitor / Python) and relaunch, <i>or</i> just run '
      +'<b>Serial Monitor.command</b> from Terminal (which already has '
      +'Local-Network access).</td></tr>';
    scanwrapEl.classList.remove('hide');
    return;
  }
  devs.forEach(d=>{
    const tr=document.createElement('tr');
    if(d.ok===false){
      const why=d.locked?'🔒 locked — type WebPassword below + rescan'
        :('✗ '+(d.error||'no Status'));
      tr.innerHTML='<td>'+_ipCell(d)+'</td><td><b>'+(d.name||'')+'</b></td>'
        +'<td class="lk" colspan="7">'+why+'</td>';
    }else{
      const parts=(d.parts||[]).map(p=>{
        let u='';
        if(p.used!=null){const c=p.used>=90?'pu-hi':(p.used>=75?'pu-warn':'');
          u=' <span class="'+c+'">'+p.used+'%</span>';}
        return '<b>'+_esc(p.name)+'</b> '+_fmtKB(p.kb)+u;
      }).join(' · ') || '—';
      tr.innerHTML='<td>'+_ipCell(d)+'</td>'
        +'<td>'+_nameCell(d)+'</td>'
        +'<td>'+_hostCell(d)+'</td>'
        +'<td>'+_cpuCell(d)+'</td>'
        +'<td>'+_esc(d.version)+(d.core?' <span style="color:var(--mut)">('+_esc(d.core)+')</span>':'')+'</td>'
        +'<td style="color:var(--mut)">'+_esc(d.flash)+'</td>'
        +'<td>'+_freeCell(d)+'</td>'
        +'<td class="parts">'+parts+'</td>'
        +'<td><button class="go">OTA ▸</button></td>';
    }
    tr.onclick=(e)=>{
      if(e.target&&e.target.classList&&e.target.classList.contains('rn')){
        startRename(d, e.target.dataset.f, tr); return;   // ✎ pencil
      }
      selectDevice(d.ip,tr);
      if(e.target&&e.target.tagName==='BUTTON'){
        if(!fwReady){alert('Choose a firmware .bin first (top of the '
          +'Flash bar), then click OTA on the device.');return;}
        flashgoEl.click();           // confirms, then OTAs $('#host')
      }
    };
    scanbodyEl.appendChild(tr);
  });
  scanwrapEl.classList.remove('hide');
}
function startRename(d, field, tr){
  const isHost = field==='hostname';
  const cur = isHost ? (d.host||'') : (d.name||'');
  const nv = prompt('New '+(isHost
      ? 'hostname for '+d.ip+'  (A–Z a–z 0–9 - only; the device REBOOTS):'
      : 'device name for '+d.ip+' :'), cur);
  if(nv===null) return;
  const v=nv.trim();
  if(!v || v===cur) return;
  if(isHost && !confirm('Set hostname of '+d.ip+' to "'+v+'"?\n\n'
      +'The device will REBOOT to apply it (brief WLAN drop).')) return;
  applyRename(d, field, v, tr);
}
async function applyRename(d, field, value, tr){
  try{
    const j=await(await fetch('/api/rename',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({host:d.ip,field:field,value:value,
        password:$('#fwpass').value||''})})).json();
    if(!j.ok){ alert('Rename failed: '+(j.locked
      ?'WebPassword required — type it in the Flash bar and retry'
      :(j.error||'unknown'))); return; }
    if(field==='hostname'){
      d.host=j.value;
      tr.children[2].innerHTML=_hostCell(d)
        +' <i style="color:var(--mut)">rebooting… rescan in ~20 s</i>';
    }else{
      d.name=j.value;
      tr.children[1].innerHTML=_nameCell(d);
    }
  }catch(e){ alert('Rename error: '+e); }
}
hostscanEl.onclick=async()=>{
  const ip=(hostipEl&&hostipEl.value)||'';
  const net=ip.split('.').slice(0,3).join('.');
  hostscanEl.disabled=true;
  const ot=hostscanEl.textContent; hostscanEl.textContent='scanning…';
  scanbodyEl.innerHTML='<tr><td colspan="9" style="color:var(--mut)">'
    +'scanning '+(net||'LAN')+'.x — reading CPU / version / partitions …'
    +'</td></tr>'; scanwrapEl.classList.remove('hide');
  try{
    const pw=encodeURIComponent($('#fwpass').value||'');
    const j=await(await fetch('/api/scan?full=1'+(net?'&net='+net:'')
      +'&pw='+pw)).json();
    renderScanTable(j.devices);
    // keep the compact dropdown in sync as a fallback picker
    hostselEl.innerHTML='';
    const o0=document.createElement('option');
    o0.value='';o0.textContent=(j.devices||[]).length
      ? '— '+j.devices.length+' on '+j.net+'.x —'
      : '— none found on '+j.net+'.x —';
    hostselEl.appendChild(o0);
    (j.devices||[]).forEach(d=>{const o=document.createElement('option');
      o.value=d.ip;o.textContent=d.ip+(d.name?'  ('+d.name+')':'');
      hostselEl.appendChild(o);});
    if((j.devices||[]).length===1) selectDevice(j.devices[0].ip,
      scanbodyEl.firstChild);
  }catch(e){ scanbodyEl.innerHTML='<tr><td colspan="9" class="lk">'
    +'scan failed: '+e+'</td></tr>'; }
  hostscanEl.textContent=ot; hostscanEl.disabled=false;
};
function updateFlash(j){
  const fs=j.flash||{}; flashing=!!fs.running;
  if(j.fw&&!fwReady){fwReady=true; fwName=j.fw;
    fwinfoEl.textContent=j.fw+'  ('+(j.fwsize||0).toLocaleString()+' B)';}
  const noesp=$('#noesptool'), espbtn=$('#espinstall');
  noesp.classList.toggle('hide', !!j.esptool);
  if(espbtn){
    espbtn.disabled=!!j.esptool_installing;
    espbtn.textContent=j.esptool_installing
      ? 'installing esptool …' : 'Install esptool (isolated)';
  }
  if(j.esptool_installing)
    fwstatEl.textContent='installing esptool (see log) …';
  else if(fs.running)
    fwstatEl.textContent=(fs.phase||'')+' '+(fs.pct||0)+'%';
  else if(fs.ok===true) fwstatEl.textContent='✓ done';
  else if(fs.ok===false) fwstatEl.textContent='✗ '+(fs.error||'failed');
  else fwstatEl.textContent='';
  fwpctEl.classList.toggle('hide',!fs.running);
  fwpctEl.value=fs.pct||0;
  flashgoEl.disabled=fs.running||!!j.esptool_installing;
  flashcancelEl.classList.toggle('hide',!fs.running);
}

$('#refresh').onclick=loadPorts;
$('#frefresh').onclick=loadPorts;
$('#clear').onclick=async()=>{
  await fetch('/api/clear',{method:'POST'});
  logEl.innerHTML=''; since=0;
};
$('#save').onclick=()=>{ window.location='/api/save'; };
$('#ts').onchange=e=>logEl.classList.toggle('notime',!e.target.checked);
$('#hex').onchange=()=>{
  for(const s of logEl.querySelectorAll('.l>span:last-child')) render(s);
};
$('#quit').onclick=async()=>{
  if(!confirm('Stop the Serial Monitor server?'))return;
  if(pollTimer)clearInterval(pollTimer);
  try{await fetch('/api/quit',{method:'POST'});}catch(e){}
  document.title='Serial Monitor (stopped)';
  document.body.innerHTML=
    '<div style="font:1.2em -apple-system,sans-serif;padding:60px;'+
    'text-align:center;color:#7a8aa0;background:#0e1116;'+
    'height:100vh;margin:0;">'+
    '<h2 style="color:#d6deeb">Serial Monitor stopped.</h2>'+
    '<p>You can close this tab.</p></div>';
};

setMode('mon'); loadPorts(); loadHost(); poll(); pollTimer=setInterval(poll,250);
</script></body></html>"""


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):           # silence stock logging
        pass

    def _send(self, code, body, ctype='application/json', extra=None):
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Access-Control-Allow-Origin', '*')
        # Safari caches aggressively; never let it serve a stale UI.
        self.send_header('Cache-Control', 'no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        if extra:
            for k, v in extra.items():
                self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj):
        self._send(200, json.dumps(obj).encode('utf-8'))

    def do_GET(self):
        path = urlparse(self.path).path
        if path == '/' or path == '/index.html':
            self._send(200, HTML.encode('utf-8'),
                       'text/html; charset=utf-8')
        elif path == '/api/ports':
            self._json(list_ports())
        elif path == '/api/prefs':
            self._json({'device': pref_device, 'baud': pref_baud,
                        'sport': pref_sport})
        elif path == '/api/host':
            self._json({'ips': _local_ips()})
        elif path == '/api/scan':
            qs = parse_qs(urlparse(self.path).query)
            net = (qs.get('net') or [''])[0]
            pw = (qs.get('pw') or [''])[0]
            full = (qs.get('full') or ['0'])[0] in ('1', 'true', 'yes')
            if not net:
                net = _default_ip().rsplit('.', 1)[0]
            devs = _scan_full(net, pw) if full else _scan_tasmota(net)
            self._json({'net': net, 'devices': devs})
        elif path == '/api/devinfo':
            qs = parse_qs(urlparse(self.path).query)
            host = (qs.get('host') or [''])[0]
            pw = (qs.get('pw') or [''])[0]
            if not host:
                self._json({'ok': False, 'error': 'no host'})
            else:
                self._json(_dev_info(host, 'admin', pw))
        elif path == '/api/poll':
            qs = parse_qs(urlparse(self.path).query)
            since = int((qs.get('since') or ['0'])[0])
            with state_lock:
                seq = log_seq
                oldest = log_buf[0]['seq'] if log_buf else seq + 1
                lines = [e for e in log_buf if e['seq'] > since]
                dropped = max(0, (oldest - 1) - since) if since else 0
                openf = serial_port is not None
                slog = syslog_port
                dev, baud, rb, tot = cur_device, cur_baud, rx_bytes, \
                    len(log_buf)
            with fw_lock:
                fs = dict(flash_state)
            self._json({'lines': lines, 'seq': seq, 'total': tot,
                        'dropped': dropped, 'open': openf,
                        'dev': dev, 'baud': baud, 'rx_bytes': rb,
                        'syslog': slog > 0, 'sport': slog,
                        'flash': fs, 'fw': fw_name, 'fwsize': fw_size,
                        'esptool': bool(_have_esptool()),
                        'esptool_installing': install_state['running']})
        elif path == '/api/save':
            with state_lock:
                txt = ''.join(
                    f"{e['t']} "
                    f"{('['+e['src']+'] ') if e.get('src') else ''}"
                    f"{e['x']}\n" for e in log_buf)
            fn = time.strftime('serial-%Y%m%d-%H%M%S.log')
            self._send(200, txt.encode('utf-8', 'replace'),
                       'text/plain; charset=utf-8',
                       {'Content-Disposition':
                        f'attachment; filename="{fn}"'})
        elif path == '/api/share/status':
            with share_lock:
                self._json(dict(share_state))
        elif path == '/api/share/csv':
            fn = time.strftime('shares-%Y%m%d-%H%M%S.csv')
            self._send(200, _share_csv(),
                       'text/csv; charset=utf-8',
                       {'Content-Disposition':
                        f'attachment; filename="{fn}"'})
        else:
            self._send(404, b'not found', 'text/plain')

    def do_POST(self):
        global fw_path, fw_name, fw_size
        path = urlparse(self.path).path
        n = int(self.headers.get('Content-Length', 0) or 0)
        raw = self.rfile.read(n) if n else b''
        if path == '/api/fw':
            fn, blob = _multipart_first_file(
                raw, self.headers.get('Content-Type', ''))
            if not blob:
                self._json({'ok': False, 'error': 'no file'})
                return
            try:
                fd, tmp = tempfile.mkstemp(suffix='.bin',
                                           prefix='serialmon-fw-')
                with os.fdopen(fd, 'wb') as f:
                    f.write(blob)
            except Exception as e:
                self._json({'ok': False, 'error': str(e)})
                return
            with fw_lock:
                if fw_path and os.path.isfile(fw_path):
                    try:
                        os.unlink(fw_path)
                    except Exception:
                        pass
                fw_path, fw_name, fw_size = tmp, os.path.basename(fn), \
                    len(blob)
            cls = _classify_fw(blob)
            add_line('info', f'[flash] firmware loaded: {fw_name} '
                             f'({fw_size} bytes) — {cls["note"]}')
            self._json({'ok': True, 'name': fw_name, 'size': fw_size,
                        'kind': cls['kind'], 'offset': cls['offset'],
                        'note': cls['note']})
            return
        try:
            body = json.loads(raw) if raw else {}
        except Exception:
            body = {}
        if path == '/api/flash/serial':
            err = _flash_guard()
            if not err and not body.get('port'):
                err = 'no serial port selected'
            if err:
                self._json({'ok': False, 'error': err})
            else:
                threading.Thread(target=_flash_serial, args=(
                    body['port'], body.get('baud', 460800),
                    body.get('offset', '0x0'),
                    bool(body.get('erase')),
                    bool(body.get('nostub'))), daemon=True).start()
                self._json({'ok': True})
            return
        if path == '/api/flash/ota':
            err = _flash_guard()
            if not err and not body.get('host'):
                err = 'no device host/IP'
            if err:
                self._json({'ok': False, 'error': err})
            else:
                threading.Thread(target=_flash_ota, args=(
                    body['host'], body.get('user', 'admin'),
                    body.get('password', '')), daemon=True).start()
                self._json({'ok': True})
            return
        if path == '/api/rename':
            host = str(body.get('host', '')).strip()
            field = str(body.get('field', ''))
            value = str(body.get('value', '')).strip()
            pw = str(body.get('password', '') or body.get('pw', ''))
            if not host or not value or field not in ('devicename',
                                                      'hostname'):
                self._json({'ok': False,
                            'error': 'host/field/value required'})
                return
            dport = 80
            if ':' in host:
                h, p = host.split(':', 1)
                host, dport = h, int(p)
            if field == 'hostname':
                # Tasmota Hostname: A-Za-z0-9-, <=32, applied on reboot.
                v = _re.sub(r'[^A-Za-z0-9-]', '-', value)[:32].strip('-')
                if not v:
                    self._json({'ok': False, 'error': 'invalid hostname'})
                    return
                cmnd = 'Backlog Hostname %s; Restart 1' % v
            else:
                v = value.replace(';', ' ')[:32].strip()   # DeviceName
                cmnd = 'DeviceName %s' % v
            st, txt = _cm(host, dport, cmnd, 'admin', pw, timeout=8)
            if st == 401:
                self._json({'ok': False, 'locked': True,
                            'error': 'WebPassword required'})
            elif st != 200:
                self._json({'ok': False,
                            'error': 'device /cm returned %s' % st})
            else:
                _flog('%s %s -> "%s"%s' % (host, field, v,
                      ' (rebooting)' if field == 'hostname' else ''))
                self._json({'ok': True, 'value': v,
                            'rebooting': field == 'hostname'})
            return
        if path == '/api/flash/cancel':
            _flash_cancel()
            self._json({'ok': True})
            return
        if path == '/api/esptool/install':
            if _have_esptool():
                self._json({'ok': True, 'note': 'already installed'})
            elif install_state['running']:
                self._json({'ok': True, 'note': 'install in progress'})
            else:
                threading.Thread(target=_install_esptool,
                                 daemon=True).start()
                self._json({'ok': True})
            return
        if path == '/api/open':
            ok, err = _open_serial(body.get('device', ''),
                                   body.get('baud', 115200))
            self._json({'ok': ok, 'error': err})
        elif path == '/api/send':
            ok, err = _write_serial(str(body.get('text', '')),
                                    str(body.get('eol', 'crlf')))
            self._json({'ok': ok, 'error': err})
        elif path == '/api/close':
            _close_serial()
            self._json({'ok': True})
        elif path == '/api/syslog':
            if body.get('action') == 'start':
                ok, err = _start_syslog(body.get('port', 514))
            else:
                _stop_syslog()
                ok, err = True, None
            self._json({'ok': ok, 'error': err})
        elif path == '/api/clear':
            with state_lock:
                log_buf.clear()
            self._json({'ok': True})
        elif path == '/api/share/start':
            ok, err = _share_start(body.get('duration', 30))
            self._json({'ok': ok, 'error': err})
        elif path == '/api/share/stop':
            _share_stop()
            self._json({'ok': True})
        elif path == '/api/quit':
            self._json({'ok': True})
            _close_serial(quiet=True)
            _stop_syslog(quiet=True)
            # let the response flush, then hard-exit the process
            threading.Timer(0.2, lambda: os._exit(0)).start()
        else:
            self._send(404, b'not found', 'text/plain')


def main():
    threading.Thread(target=_serial_reader, daemon=True).start()
    # Safari resolves "localhost" to IPv6 ::1; bind all interfaces and
    # open via 127.0.0.1 so the page AND its fetch()s reach the server.
    url = f'http://127.0.0.1:{HTTP_PORT}/'
    # If an instance is already running, REPLACE it so a relaunch
    # always loads the current code (no stale-process trap). Politely
    # ask it to quit, wait for the port, then take over.
    try:
        socket.create_connection(('127.0.0.1', HTTP_PORT), 0.4).close()
        running = True
    except OSError:
        running = False
    if running:
        print('Serial Monitor already running — replacing it')
        try:
            c = http.client.HTTPConnection('127.0.0.1', HTTP_PORT,
                                            timeout=2)
            c.request('POST', '/api/quit')
            c.getresponse().read()
            c.close()
        except Exception:
            pass
        freed = False
        for _ in range(60):                  # up to ~6s
            try:
                socket.create_connection(
                    ('127.0.0.1', HTTP_PORT), 0.2).close()
                time.sleep(0.1)
            except OSError:
                freed = True
                break
        if not freed:
            print(f'old instance won’t release :{HTTP_PORT} — '
                  f'opening {url}')
            _open_url(url)
            return
    # Threaded: a slow request (LAN scan ~2-3s, devinfo, OTA up to
    # 25s, a multi-minute serial flash) must NOT block the 250ms poll
    # / live log — single-threaded made Safari abort with "Load
    # failed". daemon threads so Quit/os._exit stays instant.
    ThreadingHTTPServer.allow_reuse_address = True
    ThreadingHTTPServer.daemon_threads = True
    # Bind loopback only: the web UI is always local (we open
    # 127.0.0.1). Binding 0.0.0.0 made the macOS app-firewall stall
    # the FIRST inbound connection ~4s (then Safari aborts "Load
    # failed"). Loopback is exempt → instant, and still reachable as
    # 127.0.0.1. (Syslog has its own 0.0.0.0 UDP socket — unrelated.)
    srv = ThreadingHTTPServer(('127.0.0.1', HTTP_PORT), H)
    print(f'Serial Monitor on {url}  (history {HISTORY} lines)')
    threading.Timer(0.6, lambda: _open_url(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        _close_serial(quiet=True)


if __name__ == '__main__':
    main()
