#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Shelly / EcoTracker Tester — cross-platform browser GUI.

============================================================================
ORIGIN / CREDIT
============================================================================

This tool is a cross-platform PORT of the original Windows-only
PowerShell GUI by **ottelo**:

    https://ottelo.jimdofree.com/

The UI layout, German labels, color-coded log, JSON pretty-printer, the
persistent UDP listener concept, and the overall workflow are all from
the original PowerShell version. This file is just a Python rewrite of
the same tool so it runs on macOS, Linux, and Windows.

All design credit goes to ottelo. The credit banner rendered at the top
of the running app links back to https://ottelo.jimdofree.com/ — please
leave it visible if you distribute modified copies.

============================================================================

Drives the same protocols the Shelly and EcoTracker devices speak —
UDP-RPC on port 1010, HTTP GET on port 80, plus an ICMP ping mode for
connectivity testing.

Architecture: tiny stdlib HTTP server on localhost serves a single HTML
page; UDP send/receive, HTTP GET, and ping happen in the Python backend
via /api/* endpoints. No external Python deps. Same packaging pattern as
the SML Emulator and UDP Monitor.

Usage:
    python3 shelly_tester_gui.py [--port 8200] [--no-browser]
"""

import argparse
import json
import os
import platform
import re
import socket
import struct
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError

DEFAULT_PORT = 8200
APP_VERSION = "v2026-05-04"

# ─── Background state ────────────────────────────────────────────────────────
state_lock = threading.Lock()

# UDP listener — a persistent socket bound to a free local port that
# captures unsolicited packets (Shelly/EcoTracker push events). When
# active, /api/udp/send reuses this socket so server replies arrive at
# the same port the device might be configured to push to.
udp_listener_sock = None
udp_listener_port = None
udp_listener_packets = []        # list of {ts, addr, port, hex, text, json?}

# Ping background loop
ping_thread = None
ping_stop_event = threading.Event()
ping_state = {
    'running': False,
    'host': '',
    'sent': 0,
    'received': 0,
    'lost': 0,
    'min_ms': None,
    'max_ms': 0.0,
    'total_ms': 0.0,
    'started_at': None,
    'duration_s': 0,
    'log_file': None,
    'errors_only': False,
    'recent': [],          # last N ping results, FIFO
}
PING_RECENT_MAX = 200


# ─── UDP listener ────────────────────────────────────────────────────────────
def udp_listener_start():
    global udp_listener_sock, udp_listener_port
    with state_lock:
        if udp_listener_sock is not None:
            return udp_listener_port
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(('', 0))     # OS picks free port
        except OSError as e:
            s.close()
            raise
        s.settimeout(0.2)
        udp_listener_sock = s
        udp_listener_port = s.getsockname()[1]
    threading.Thread(target=udp_listener_loop, daemon=True).start()
    return udp_listener_port


def udp_listener_stop():
    global udp_listener_sock, udp_listener_port
    with state_lock:
        s = udp_listener_sock
        udp_listener_sock = None
        udp_listener_port = None
    if s:
        try:
            s.close()
        except Exception:
            pass


def udp_listener_loop():
    while True:
        with state_lock:
            s = udp_listener_sock
        if s is None:
            return
        try:
            data, addr = s.recvfrom(65535)
        except socket.timeout:
            continue
        except OSError:
            return    # socket closed
        ts = time.strftime('%Y-%m-%d %H:%M:%S') + f'.{int(time.time()*1000)%1000:03d}'
        text = ''
        try:
            text = data.decode('utf-8', errors='replace')
        except Exception:
            text = ''
        json_obj = _try_parse_json_first_object(text)
        with state_lock:
            udp_listener_packets.append({
                'ts': ts,
                'addr': addr[0],
                'port': addr[1],
                'len': len(data),
                'hex': data.hex(' '),
                'text': text,
                'json': json_obj,
            })
            # Cap buffer
            if len(udp_listener_packets) > 500:
                del udp_listener_packets[:len(udp_listener_packets) - 500]


def _try_parse_json_first_object(text):
    """Best-effort: parse the first JSON object out of a possibly-glued
    response. Real devices sometimes send ...}{... with two payloads
    back-to-back; we want to surface the first complete one."""
    if not text:
        return None
    text = text.strip()
    if not text:
        return None
    try:
        return json.loads(text)
    except Exception:
        pass
    # Find first `}{` which suggests two glued objects
    idx = text.find('}{')
    if idx > 0:
        try:
            return json.loads(text[:idx + 1])
        except Exception:
            pass
    return None


# ─── UDP send (one-shot, optionally via the listener socket) ─────────────────
def udp_send_recv(host, port, payload, timeout_s=3.0, use_listener=False):
    """Send `payload` (str) to host:port and collect responses.
    Returns dict with: ok, text, hex, addr, port, len, json, error.
    If use_listener=True and a listener socket is active, send via that
    socket (server reply arrives at the listener port too); otherwise
    create a one-shot socket."""
    payload_b = payload.encode('utf-8')
    own_socket = None
    use_existing = False
    s = None
    if use_listener:
        with state_lock:
            s = udp_listener_sock
        if s is not None:
            use_existing = True
    if s is None:
        try:
            own_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            own_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65535)
            own_socket.settimeout(timeout_s)
            s = own_socket
        except OSError as e:
            return {'ok': False, 'error': f'socket: {e}'}

    try:
        s.sendto(payload_b, (host, port))
        # Try to drain a few responses (server might send multiple);
        # collect into one chunk. Stop after first 500 ms once we got
        # at least one packet.
        chunks = []
        last_addr = None
        try:
            data, addr = s.recvfrom(65535)
            chunks.append(data)
            last_addr = addr
            s.settimeout(0.5)
            while True:
                try:
                    data, addr = s.recvfrom(65535)
                    chunks.append(data)
                    last_addr = addr
                except socket.timeout:
                    break
        except socket.timeout:
            return {'ok': False, 'error': 'timeout', 'sent_to': f'{host}:{port}'}
        finally:
            if own_socket is not None:
                own_socket.close()

        merged = b''.join(chunks)
        text = merged.decode('utf-8', errors='replace')
        return {
            'ok': True,
            'text': text,
            'hex': merged.hex(' '),
            'len': len(merged),
            'addr': last_addr[0] if last_addr else '',
            'port': last_addr[1] if last_addr else 0,
            'json': _try_parse_json_first_object(text),
            'duplicate': '}{' in text,
            'sent_to': f'{host}:{port}',
        }
    except Exception as e:
        if own_socket is not None:
            try:
                own_socket.close()
            except Exception:
                pass
        return {'ok': False, 'error': str(e)}


# ─── HTTP GET ────────────────────────────────────────────────────────────────
def http_get(host, port, path, timeout_s=5.0):
    if not path.startswith('/'):
        path = '/' + path
    if port and str(port) not in ('', '80'):
        url = f'http://{host}:{port}{path}'
    else:
        url = f'http://{host}{path}'
    try:
        req = Request(url, headers={'User-Agent': 'Shelly-Tester/1.0'})
        with urlopen(req, timeout=timeout_s) as resp:
            data = resp.read()
            text = data.decode('utf-8', errors='replace')
            return {
                'ok': True,
                'url': url,
                'status': resp.status,
                'text': text,
                'len': len(data),
                'json': _try_parse_json_first_object(text),
            }
    except HTTPError as e:
        body = ''
        try:
            body = e.read().decode('utf-8', errors='replace')
        except Exception:
            pass
        return {'ok': False, 'url': url, 'status': e.code, 'error': str(e), 'text': body}
    except URLError as e:
        return {'ok': False, 'url': url, 'error': str(e.reason)}
    except Exception as e:
        return {'ok': False, 'url': url, 'error': str(e)}


# ─── Ping (cross-platform via subprocess on the system `ping` binary) ────────
def _ping_cmd(host, timeout_s):
    """Build the platform-correct ping command for ONE echo request."""
    sysname = platform.system()
    if sysname == 'Windows':
        # /n 1 = 1 echo, /w in ms
        return ['ping', '-n', '1', '-w', str(int(timeout_s * 1000)), host]
    if sysname == 'Darwin':
        # macOS: -W in ms
        return ['ping', '-c', '1', '-W', str(int(timeout_s * 1000)), host]
    # Linux / BSD: -W in seconds (integer)
    return ['ping', '-c', '1', '-W', str(max(1, int(timeout_s))), host]


_TIME_RE  = re.compile(r'time[=<]([0-9.]+)\s*ms', re.IGNORECASE)
_TTL_RE   = re.compile(r'ttl[= ]([0-9]+)', re.IGNORECASE)
_FROM_RE  = re.compile(r'from\s+([0-9.]+)', re.IGNORECASE)


def ping_once(host, timeout_s=2.0):
    cmd = _ping_cmd(host, timeout_s)
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True, text=True,
            timeout=timeout_s + 2,
        )
    except subprocess.TimeoutExpired:
        return {'ok': False, 'status': 'TimedOut', 'error': 'timeout'}
    except FileNotFoundError:
        return {'ok': False, 'status': 'NoPing', 'error': 'ping binary not found in PATH'}
    except Exception as e:
        return {'ok': False, 'status': 'Error', 'error': str(e)}

    out = (proc.stdout or '') + (proc.stderr or '')
    if proc.returncode != 0:
        # Distinguish "host not found" from "no reply" loosely
        err_status = 'TimedOut' if 'time' in out.lower() else 'Failed'
        if 'unknown host' in out.lower() or 'cannot resolve' in out.lower() or 'name or service' in out.lower():
            err_status = 'NoHost'
        return {'ok': False, 'status': err_status, 'error': out.strip().splitlines()[0] if out.strip() else 'unreachable'}

    m_time = _TIME_RE.search(out)
    m_ttl  = _TTL_RE.search(out)
    m_from = _FROM_RE.search(out)
    return {
        'ok': True,
        'status': 'Success',
        'rtt_ms': float(m_time.group(1)) if m_time else 0.0,
        'ttl': int(m_ttl.group(1)) if m_ttl else 0,
        'from': m_from.group(1) if m_from else '',
    }


def ping_loop(host, interval_s, duration_s, errors_only, log_file):
    """Background ping loop. Stops on ping_stop_event or duration cap."""
    started = time.time()
    fast = interval_s <= 0
    while not ping_stop_event.is_set():
        loop_start = time.time()
        result = ping_once(host)
        ts = time.strftime('%Y-%m-%d %H:%M:%S') + f'.{int(time.time()*1000)%1000:03d}'
        with state_lock:
            ping_state['sent'] += 1
            line = ''
            if result['ok']:
                rtt = float(result.get('rtt_ms', 0))
                ping_state['received'] += 1
                ping_state['total_ms'] += rtt
                if ping_state['min_ms'] is None or rtt < ping_state['min_ms']:
                    ping_state['min_ms'] = rtt
                if rtt > ping_state['max_ms']:
                    ping_state['max_ms'] = rtt
                line = f"[{ts}]  PING  {host}  ->  Antwort von {result.get('from','?')}: {rtt:.1f}ms  TTL={result.get('ttl','?')}"
            else:
                ping_state['lost'] += 1
                line = f"[{ts}]  PING  {host}  ->  {result.get('status','?')} ({result.get('error','')})"
            ping_state['recent'].append({
                'ts': ts,
                'ok': result['ok'],
                'rtt_ms': float(result.get('rtt_ms', 0)) if result['ok'] else None,
                'ttl': result.get('ttl', 0) if result['ok'] else None,
                'from_addr': result.get('from', ''),
                'status': result.get('status', ''),
                'error': result.get('error', ''),
                'line': line,
            })
            if len(ping_state['recent']) > PING_RECENT_MAX:
                del ping_state['recent'][:len(ping_state['recent']) - PING_RECENT_MAX]

        # File log
        if log_file and (not errors_only or not result['ok']):
            try:
                with open(log_file, 'a', encoding='utf-8') as f:
                    f.write(line + '\n')
            except Exception:
                pass

        # Duration cap
        if duration_s and (time.time() - started) >= duration_s:
            break

        # Pace next iteration
        if fast:
            # roughly as fast as possible while leaving CPU room — ~10/s
            elapsed = time.time() - loop_start
            sleep_for = max(0.05, 0.1 - elapsed)
            ping_stop_event.wait(sleep_for)
        else:
            elapsed = time.time() - loop_start
            sleep_for = max(0.0, interval_s - elapsed)
            ping_stop_event.wait(sleep_for)

    # Wrap up
    with state_lock:
        ping_state['running'] = False
        if log_file:
            try:
                with open(log_file, 'a', encoding='utf-8') as f:
                    sent = ping_state['sent']
                    received = ping_state['received']
                    lost = ping_state['lost']
                    loss_pct = (lost / sent * 100.0) if sent else 0.0
                    avg = (ping_state['total_ms'] / received) if received else 0.0
                    mn  = ping_state['min_ms'] if ping_state['min_ms'] is not None else 0.0
                    f.write('=' * 78 + '\n')
                    f.write(f'Ping-Statistik fuer {host}:\n')
                    f.write(f'Gesendet: {sent}  |  Empfangen: {received}  |  Verloren: {lost} ({loss_pct:.1f}%)\n')
                    f.write(f'RTT: Min={mn:.1f}ms  Avg={avg:.1f}ms  Max={ping_state["max_ms"]:.1f}ms\n')
                    f.write('=' * 78 + '\n')
            except Exception:
                pass


def ping_start(host, interval_s, duration_s, errors_only, write_log):
    global ping_thread
    with state_lock:
        if ping_state['running']:
            return False, 'already running'
        ping_state.update({
            'running': True,
            'host': host,
            'sent': 0,
            'received': 0,
            'lost': 0,
            'min_ms': None,
            'max_ms': 0.0,
            'total_ms': 0.0,
            'started_at': time.time(),
            'duration_s': duration_s,
            'errors_only': errors_only,
            'recent': [],
        })
        log_file = None
        if write_log:
            home = os.path.expanduser('~')
            desktop = os.path.join(home, 'Desktop')
            base = desktop if os.path.isdir(desktop) else home
            stamp = time.strftime('%Y-%m-%d_%H-%M-%S')
            log_file = os.path.join(base, f'ping_{host}_{stamp}.log')
            try:
                with open(log_file, 'w', encoding='utf-8') as f:
                    f.write(f"Ping-Log gestartet: {time.strftime('%Y-%m-%d %H:%M:%S')}  Ziel: {host}\n")
                    f.write(f"Intervall: {('max. Speed' if interval_s <= 0 else f'{interval_s}s')}  "
                            f"Dauer: {('unendlich' if not duration_s else f'{duration_s}s')}"
                            f"{'  [Nur Fehler]' if errors_only else ''}\n")
                    f.write('-' * 78 + '\n')
                ping_state['log_file'] = log_file
            except Exception:
                ping_state['log_file'] = None
        else:
            ping_state['log_file'] = None
    ping_stop_event.clear()
    ping_thread = threading.Thread(
        target=ping_loop,
        args=(host, interval_s, duration_s, errors_only, log_file),
        daemon=True,
    )
    ping_thread.start()
    return True, log_file


def ping_stop():
    ping_stop_event.set()


# ─── HTML page (embedded) ────────────────────────────────────────────────────
HTML_PAGE = r"""<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<title>Shelly / EcoTracker Tester __VERSION__ &mdash; based on ottelo's PowerShell tool</title>
<style>
  * { box-sizing: border-box; }
  html, body { margin: 0; padding: 0; background: #1e1e1e; color: #fff;
               font-family: 'Segoe UI', system-ui, sans-serif; font-size: 13px; }
  body { padding: 14px; }
  .row { display: flex; gap: 8px; align-items: center; margin-bottom: 10px; flex-wrap: wrap; }
  .row label { color: #b4b4b4; min-width: 110px; }
  input[type=text], input[type=number], textarea, select {
    background: #2d2d2d; color: #fff; border: 1px solid #444; padding: 5px 8px;
    border-radius: 3px; font-family: inherit; font-size: 13px;
  }
  input.short { width: 80px; }
  input.tiny  { width: 60px; text-align: center; }
  textarea { width: 100%; min-height: 26px; }
  button { background: #3c3c3c; color: #fff; border: 0; padding: 6px 12px;
           border-radius: 3px; cursor: pointer; font: inherit; }
  button:hover:not(:disabled) { background: #4a4a4a; }
  button:disabled { opacity: 0.45; cursor: not-allowed; }
  button.send  { background: #009688; }   /* UDP green */
  button.http  { background: #3c78c8; }
  button.ping  { background: #c8a000; }
  button.stop  { background: #c83c3c; }
  .indicator { font-weight: bold; color: #009688; }
  .indicator.http { color: #3c78c8; }
  .indicator.ping { color: #c8a000; }
  fieldset { border: 1px solid #404040; border-radius: 4px; margin: 10px 0; padding: 10px; }
  legend { color: #b4b4b4; padding: 0 6px; }
  .panel { display: none; }
  .panel.active { display: block; }
  hr { border: 0; border-top: 1px solid #464646; margin: 12px 0; }
  .log { background: #2d2d2d; color: #64ffb4; font-family: Consolas, monospace;
         font-size: 12.5px; padding: 8px; border-radius: 3px; height: 240px;
         overflow-y: auto; white-space: pre-wrap; word-wrap: break-word; }
  .log .ok    { color: #64ffb4; }
  .log .err   { color: #ff5050; }
  .log .warn  { color: #ffdc64; }
  .log .dup   { color: #ffa532; }
  .log .listen{ color: #b482ff; }
  .json { background: #2d2d2d; color: #82b4ff; font-family: Consolas, monospace;
          font-size: 12.5px; padding: 8px; border-radius: 3px; height: 270px;
          overflow: auto; white-space: pre; }
  .status { background: #2d2d2d; color: #b4b4b4; padding: 4px 8px; margin-top: 10px;
            border-radius: 3px; font-size: 12px; }
  .right { margin-left: auto; }
  a { color: #64b4ff; }
  .preset { background: #3c3c3c; }
  .listener { color: #b482ff; }
  input[type="checkbox"] { accent-color: #009688; }
  .hint { color: #888; font-size: 11px; }
  .banner { background: linear-gradient(90deg, #1a3a4a, #2d2d2d);
            border-left: 4px solid #009688; padding: 10px 14px;
            border-radius: 4px; margin-bottom: 14px; position: relative; }
  .banner-title { font-size: 15px; font-weight: 600; color: #fff; margin-bottom: 3px; }
  .banner-ver { font-size: 11px; color: #888; font-weight: 400; margin-left: 6px; }
  .banner-credit { font-size: 12px; color: #b4b4b4; }
  .banner-credit a { color: #64b4ff; text-decoration: none; font-weight: 600; }
  .banner-credit a:hover { text-decoration: underline; }
  .banner-exit { position: absolute; top: 10px; right: 12px;
                 background: #c83c3c; color: #fff; border: 0; padding: 6px 12px;
                 border-radius: 3px; cursor: pointer; font: inherit; font-size: 12px;
                 font-weight: 600; }
  .banner-exit:hover { background: #d44848; }
</style>
</head>
<body>

<!-- Header / credit banner -->
<div class="banner">
  <button id="btnExit" class="banner-exit" title="Server beenden und Fenster schliessen">&times; Beenden</button>
  <div class="banner-title">Shelly / EcoTracker Tester <span class="banner-ver">__VERSION__</span></div>
  <div class="banner-credit">
    Cross-platform port of the Windows PowerShell tool by
    <a href="https://ottelo.jimdofree.com/" target="_blank">ottelo</a>
    &mdash; original code &amp; UI design © ottelo.jimdofree.com
  </div>
</div>

<!-- Protokoll -->
<div class="row">
  <label>Protokoll:</label>
  <label><input type="radio" name="proto" id="protoUdp"  checked> UDP</label>
  <label><input type="radio" name="proto" id="protoHttp"> HTTP GET</label>
  <label><input type="radio" name="proto" id="protoPing"> Ping</label>
  <span id="modeIndicator" class="indicator">[ UDP-Modus ]</span>
</div>

<!-- Ziel-Host -->
<div class="row">
  <label>Ziel-Host / IP:</label>
  <input type="text" id="host" value="192.168.178.31" style="width:300px">
  <label id="lblPort" style="min-width:auto">Port:</label>
  <input type="text" id="port" class="short" value="1010">
</div>

<!-- UDP/HTTP Anfragebereich -->
<div id="panelUdpHttp" class="panel active">
  <div class="row">
    <label id="lblCmd">UDP-Anfrage:</label>
    <input type="text" id="cmd" style="flex:1">
    <button id="btnSend" class="send" style="min-width:130px">Senden</button>
  </div>
  <div class="row">
    <label>Vorgaben:</label>
    <span id="udpPresets">
      <button class="preset" data-set="Shelly.GetStatus">Shelly.GetStatus</button>
      <button class="preset" data-set="EM.GetStatus">EM.GetStatus</button>
    </span>
    <span id="httpPresets" style="display:none">
      <button class="preset" data-set="/v1/json">json/v1</button>
      <button class="preset" data-set="/rpc/Shelly.GetStatus">Shelly.GetStatus</button>
      <button class="preset" data-set="/rpc/EM.GetStatus">EM.GetStatus</button>
    </span>
    <label id="lblListener" class="right listener" style="min-width:auto">
      <input type="checkbox" id="chkListener"> UDP Listener
    </label>
  </div>
  <div class="row">
    <label>Intervall (Sek.):</label>
    <input type="number" id="interval" class="tiny" min="1" value="5">
    <button id="btnIntervalStart" class="send">Intervall starten</button>
    <button id="btnIntervalStop" class="stop" disabled>Intervall stoppen</button>
    <span id="intervalStatus" class="hint"></span>
  </div>
</div>

<!-- Ping-Bereich -->
<div id="panelPing" class="panel">
  <div class="row">
    <label>Ping-Intervall (Sek.):</label>
    <input type="number" id="pingInterval" class="tiny" min="0" value="1">
    <span class="hint">(0 = so schnell wie moeglich)</span>
    <label class="right" style="min-width:auto">
      <input type="checkbox" id="chkPingLog"> In Datei loggen
    </label>
  </div>
  <div class="row">
    <label>Dauer (Sek.):</label>
    <input type="number" id="pingDuration" class="tiny" min="0" value="0">
    <span class="hint">(0 = unendlich)</span>
    <label class="right" style="min-width:auto">
      <input type="checkbox" id="chkPingErrorsOnly"> Nur Fehler loggen
    </label>
  </div>
  <div class="row">
    <button id="btnPingStart" class="ping">Ping starten</button>
    <button id="btnPingStop"  class="stop" disabled>Ping stoppen</button>
    <span id="pingStatus" class="hint"></span>
    <span id="pingLogPath" class="hint listener" style="margin-left:auto"></span>
  </div>
</div>

<hr>

<!-- Antwort-Log -->
<div class="row">
  <label>Antwort-Log:</label>
  <button id="btnClearLog" class="stop right">Log leeren</button>
</div>
<div class="log" id="log"></div>

<!-- JSON -->
<div class="row" style="margin-top:10px">
  <label>JSON (formatiert):</label>
  <button id="btnClearJson" class="stop right">JSON leeren</button>
</div>
<div class="json" id="json"></div>

<div class="status" id="status">Bereit.</div>

<div class="row" style="margin-top:10px">
  <a href="https://ottelo.jimdofree.com/" target="_blank">https://ottelo.jimdofree.com/</a>
</div>

<script>
'use strict';

// ── Helpers ────────────────────────────────────────────────────────────────
const $ = id => document.getElementById(id);
const setStatus = (s) => { $('status').textContent = s; };

// ── Settings persistence via localStorage ──────────────────────────────────
// All user-tunable fields restore from localStorage on load and re-save on
// every change. Mode-specific defaults (port=1010 for UDP, port=80 for HTTP)
// only override if the user hasn't explicitly typed a port for that mode —
// see updateMode() below.
const LS_PREFIX = 'shellytester:';
const PERSIST_FIELDS = [
  // [id, type]   type ∈ { 'value', 'checked' }
  // port is intentionally NOT here — it's managed per-mode by
  // portSaveCurrent / portRestoreForMode (one stored value per UDP/HTTP).
  ['host', 'value'],
  ['cmd',  'value'],
  ['interval',     'value'],
  ['pingInterval', 'value'],
  ['pingDuration', 'value'],
  ['chkListener',       'checked'],
  ['chkPingLog',        'checked'],
  ['chkPingErrorsOnly', 'checked'],
];
function lsLoad() {
  for (const [id, kind] of PERSIST_FIELDS) {
    const el = $(id); if (!el) continue;
    const raw = localStorage.getItem(LS_PREFIX + id);
    if (raw === null) continue;
    if (kind === 'checked') el.checked = (raw === '1');
    else el.value = raw;
  }
  // Active proto radio
  const proto = localStorage.getItem(LS_PREFIX + 'proto') || 'udp';
  const radioId = {udp:'protoUdp', http:'protoHttp', ping:'protoPing'}[proto] || 'protoUdp';
  $(radioId).checked = true;
}
// Per-mode port memory: when the user switches UDP↔HTTP, restore the port
// they last used for THAT mode (default 1010 for UDP, 80 for HTTP) instead
// of always clobbering with the canonical default.
function portKey() { return $('protoUdp').checked ? 'port_udp'
                         : $('protoHttp').checked ? 'port_http'
                         : null; }
function portSaveCurrent() {
  const k = portKey(); if (k) {
    try { localStorage.setItem(LS_PREFIX + k, $('port').value); } catch (e) {}
  }
}
function portRestoreForMode() {
  const isUdp = $('protoUdp').checked;
  const isHttp = $('protoHttp').checked;
  if (!isUdp && !isHttp) return;          // ping mode: port hidden, don't touch
  const k = isUdp ? 'port_udp' : 'port_http';
  const def = isUdp ? '1010' : '80';
  $('port').value = localStorage.getItem(LS_PREFIX + k) || def;
}
function lsSave(id, kind) {
  const el = $(id); if (!el) return;
  const v = (kind === 'checked') ? (el.checked ? '1' : '0') : el.value;
  try { localStorage.setItem(LS_PREFIX + id, v); } catch (e) { /* quota / private mode */ }
}
function lsAttachAll() {
  for (const [id, kind] of PERSIST_FIELDS) {
    const el = $(id); if (!el) continue;
    el.addEventListener(kind === 'checked' ? 'change' : 'input', () => lsSave(id, kind));
  }
  for (const id of ['protoUdp','protoHttp','protoPing']) {
    $(id).addEventListener('change', () => {
      const m = $('protoUdp').checked ? 'udp' : ($('protoHttp').checked ? 'http' : 'ping');
      try { localStorage.setItem(LS_PREFIX + 'proto', m); } catch (e) {}
    });
  }
}

function nowStamp() {
  const d = new Date();
  const pad = (n, w) => String(n).padStart(w, '0');
  return `${d.getFullYear()}-${pad(d.getMonth()+1,2)}-${pad(d.getDate(),2)} ${pad(d.getHours(),2)}:${pad(d.getMinutes(),2)}:${pad(d.getSeconds(),2)}.${pad(d.getMilliseconds(),3)}`;
}

function logEntry(text, cls = 'ok') {
  const el = document.createElement('span');
  el.className = cls;
  el.textContent = text;
  // Insert at top (newest first, like the original)
  $('log').insertBefore(el, $('log').firstChild);
  // Trim — keep last 800 entries
  while ($('log').childNodes.length > 800) {
    $('log').removeChild($('log').lastChild);
  }
}

function setJson(header, obj) {
  if (obj == null) {
    $('json').textContent = `# ${nowStamp()}\n# ${header}\n# Antwort ist kein gueltiges JSON.\n`;
    return;
  }
  $('json').textContent = `# ${nowStamp()}\n# ${header}\n\n${JSON.stringify(obj, null, 2)}`;
}

async function api(path, body = null) {
  const opts = { method: body ? 'POST' : 'GET' };
  if (body) {
    opts.headers = { 'Content-Type': 'application/json' };
    opts.body = JSON.stringify(body);
  }
  const r = await fetch(path, opts);
  return await r.json();
}

// ── Mode switch ────────────────────────────────────────────────────────────
// Track what mode is currently active so we know which "port_<mode>"
// localStorage key the live port input belongs to. Whenever the proto
// changes, we save the OLD mode's port first, then restore the NEW
// mode's port — so a user-typed port survives switches and reloads.
let _lastMode = null;     // 'udp' | 'http' | 'ping' | null
function currentMode() {
  return $('protoUdp').checked  ? 'udp'
       : $('protoHttp').checked ? 'http'
       : 'ping';
}
function updateMode() {
  const newMode = currentMode();
  // Save the port we were just on (if it was a port-bearing mode) before
  // we restore the new mode's value into the same input field.
  if (_lastMode === 'udp' || _lastMode === 'http') portSaveCurrent();

  const isUdp  = newMode === 'udp';
  const isHttp = newMode === 'http';
  const isPing = newMode === 'ping';
  $('panelUdpHttp').classList.toggle('active', !isPing);
  $('panelPing').classList.toggle('active', isPing);
  $('lblPort').style.display = isPing ? 'none' : '';
  $('port').style.display     = isPing ? 'none' : '';

  const ind = $('modeIndicator');
  ind.classList.remove('http','ping');
  if (isUdp) {
    ind.textContent = '[ UDP-Modus ]';
    $('lblCmd').textContent = 'UDP-Anfrage:';
    $('btnSend').className = 'send';
    $('btnIntervalStart').className = 'send';
    $('udpPresets').style.display = '';
    $('httpPresets').style.display = 'none';
    $('lblListener').style.display = '';
    portRestoreForMode();
    document.title = `Shelly / EcoTracker Tester — UDP`;
  } else if (isHttp) {
    ind.textContent = '[ HTTP-Modus ]';
    ind.classList.add('http');
    $('lblCmd').textContent = 'URL-Pfad:';
    $('btnSend').className = 'http';
    $('btnIntervalStart').className = 'http';
    $('udpPresets').style.display = 'none';
    $('httpPresets').style.display = '';
    $('lblListener').style.display = 'none';
    if ($('chkListener').checked) {
      $('chkListener').checked = false;
      api('/api/udp/listener', { action: 'stop' });
    }
    portRestoreForMode();
    document.title = `Shelly / EcoTracker Tester — HTTP GET`;
  } else {
    ind.textContent = '[ Ping-Modus ]';
    ind.classList.add('ping');
    if ($('chkListener').checked) {
      $('chkListener').checked = false;
      api('/api/udp/listener', { action: 'stop' });
    }
    document.title = `Shelly / EcoTracker Tester — Ping`;
  }
  _lastMode = newMode;
}
['protoUdp','protoHttp','protoPing'].forEach(id => $(id).addEventListener('change', updateMode));
// Save port on every keystroke too (so even switching tabs without
// mode change preserves the typed value).
$('port').addEventListener('input', portSaveCurrent);

// ── Presets ────────────────────────────────────────────────────────────────
document.querySelectorAll('button.preset').forEach(btn => {
  btn.addEventListener('click', () => {
    $('cmd').value = btn.dataset.set;
  });
});

// ── Listener toggle ────────────────────────────────────────────────────────
$('chkListener').addEventListener('change', async () => {
  if ($('chkListener').checked) {
    const r = await api('/api/udp/listener', { action: 'start' });
    if (r.ok) {
      setStatus(`UDP Listener aktiv (lokaler Port: ${r.port})`);
    } else {
      $('chkListener').checked = false;
      setStatus(`Listener-Fehler: ${r.error}`);
    }
  } else {
    await api('/api/udp/listener', { action: 'stop' });
    setStatus('UDP Listener gestoppt.');
  }
});

// Poll listener for unsolicited packets
async function pollListener() {
  if (!$('chkListener').checked) return;
  try {
    const r = await api('/api/udp/poll');
    if (r.packets && r.packets.length) {
      for (const p of r.packets) {
        const head = `[${p.ts}]  LISTENER << Erzwungenes Paket vom Server erhalten (${p.addr}:${p.port}, ${p.len} Bytes)`;
        const sep = '\n' + '-'.repeat(78) + '\n';
        logEntry(`${head}\n${p.text}${sep}`, 'listen');
        if (p.json) setJson(`LISTENER: Erzwungenes Paket`, p.json);
      }
    }
  } catch (e) { /* ignore transient poll errors */ }
}
setInterval(pollListener, 250);

// ── Send (UDP / HTTP) ──────────────────────────────────────────────────────
async function sendOnce() {
  const isUdp = $('protoUdp').checked;
  const host = $('host').value.trim();
  const port = $('port').value.trim();
  const cmd  = $('cmd').value.trim();
  if (!host) return setStatus('Fehler: Kein Ziel-Host angegeben.');
  if (!cmd)  return setStatus('Fehler: Keine Anfrage angegeben.');

  if (isUdp) {
    const portN = parseInt(port, 10);
    if (!portN || portN < 1 || portN > 65535) return setStatus('Fehler: Ungueltiger Port.');
    setStatus(`UDP: Sende an ${host}:${portN} ...`);
    const r = await api('/api/udp/send', {
      host, port: portN, payload: cmd,
      use_listener: $('chkListener').checked,
    });
    const ts = nowStamp();
    if (r.ok) {
      const dupHint = r.duplicate ? '\n[HINWEIS] Paket doppelt erhalten!' : '';
      const sep = '\n' + '-'.repeat(78) + '\n';
      logEntry(`[${ts}]  UDP <<  ${cmd}${dupHint}\n${r.text}${sep}`, r.duplicate ? 'dup' : 'ok');
      setJson(`UDP <<  ${cmd}${r.duplicate ? '  [doppelt]' : ''}`, r.json);
      setStatus(`UDP: Antwort von ${r.addr}:${r.port} (${r.len} Bytes)`);
    } else {
      const sep = '\n' + '-'.repeat(78) + '\n';
      logEntry(`[${ts}]  UDP <<  ${cmd}\n[FEHLER] ${r.error}${sep}`, 'err');
      setStatus(`UDP Fehler: ${r.error}`);
    }
  } else {
    setStatus(`HTTP GET: ${host}:${port}${cmd.startsWith('/') ? cmd : '/'+cmd} ...`);
    const r = await api('/api/http/get', { host, port, path: cmd });
    const ts = nowStamp();
    const sep = '\n' + '-'.repeat(78) + '\n';
    if (r.ok) {
      logEntry(`[${ts}]  HTTP GET  ${r.url}\n${r.text}${sep}`, 'ok');
      setJson(`HTTP GET ${r.url}`, r.json);
      setStatus(`HTTP: Antwort erhalten von ${r.url} (${r.len} Bytes)`);
    } else {
      logEntry(`[${ts}]  HTTP GET  ${r.url}\n[FEHLER]${r.status ? ' [HTTP '+r.status+']' : ''} ${r.error}${sep}`, 'err');
      setStatus(`HTTP Fehler: ${r.error}`);
    }
  }
}
$('btnSend').addEventListener('click', sendOnce);
$('cmd').addEventListener('keydown', e => {
  if (e.key === 'Enter') { e.preventDefault(); sendOnce(); }
});

// ── Interval timer ─────────────────────────────────────────────────────────
let intervalHandle = null;
$('btnIntervalStart').addEventListener('click', () => {
  const sec = parseInt($('interval').value, 10);
  if (!sec || sec < 1) return setStatus('Fehler: Intervall muss mindestens 1 Sekunde sein.');
  $('btnIntervalStart').disabled = true;
  $('btnIntervalStop').disabled = false;
  $('interval').disabled = true;
  ['protoUdp','protoHttp','protoPing'].forEach(id => $(id).disabled = true);
  $('intervalStatus').textContent = `Aktiv: alle ${sec}s`;
  $('intervalStatus').style.color = '#64ffb4';
  setStatus(`Intervall gestartet (${sec}s).`);
  sendOnce();
  intervalHandle = setInterval(sendOnce, sec * 1000);
});
$('btnIntervalStop').addEventListener('click', () => {
  if (intervalHandle) { clearInterval(intervalHandle); intervalHandle = null; }
  $('btnIntervalStart').disabled = false;
  $('btnIntervalStop').disabled = true;
  $('interval').disabled = false;
  ['protoUdp','protoHttp','protoPing'].forEach(id => $(id).disabled = false);
  $('intervalStatus').textContent = 'Gestoppt';
  $('intervalStatus').style.color = '#888';
  setStatus('Intervall gestoppt.');
});

// ── Ping ───────────────────────────────────────────────────────────────────
let pingPollHandle = null;
let pingShownCount = 0;

$('btnPingStart').addEventListener('click', async () => {
  const host = $('host').value.trim();
  if (!host) return setStatus('Fehler: Kein Ziel-Host angegeben.');
  const interval = parseFloat($('pingInterval').value || '1');
  const duration = parseInt($('pingDuration').value || '0', 10);
  if (interval < 0) return setStatus('Fehler: Ping-Intervall muss >= 0 sein.');
  if (duration < 0) return setStatus('Fehler: Dauer muss >= 0 sein.');

  const r = await api('/api/ping/start', {
    host,
    interval_s: interval,
    duration_s: duration,
    errors_only: $('chkPingErrorsOnly').checked,
    write_log: $('chkPingLog').checked,
  });
  if (!r.ok) return setStatus(`Ping-Fehler: ${r.error}`);

  $('btnPingStart').disabled = true;
  $('btnPingStop').disabled = false;
  ['pingInterval','pingDuration','chkPingLog','chkPingErrorsOnly'].forEach(id => $(id).disabled = true);
  ['protoUdp','protoHttp'].forEach(id => $(id).disabled = true);

  const intervalText = interval <= 0 ? 'max. Speed' : `${interval}s`;
  const durText = duration === 0 ? 'unendlich' : `${duration}s`;
  $('pingStatus').textContent = `Laeuft... (${intervalText} / ${durText})`;
  $('pingStatus').style.color = '#64ffb4';
  $('pingLogPath').textContent = r.log_file ? r.log_file.split(/[\\/]/).pop() : '';
  setStatus(`Ping laeuft: ${host}...`);

  pingShownCount = 0;
  logEntry(`--- Ping gestartet: ${host}  (Intervall: ${intervalText}, Dauer: ${durText})${$('chkPingErrorsOnly').checked ? '  [Nur Fehler]' : ''} ---\n`, 'warn');

  pingPollHandle = setInterval(pollPing, 200);
});

$('btnPingStop').addEventListener('click', async () => {
  await api('/api/ping/stop', { action: 'stop' });
  // pollPing will detect running=false on its next tick and finalise
});

async function pollPing() {
  const r = await api('/api/ping/status');
  if (!r) return;
  // Render any new ping results
  const errOnly = $('chkPingErrorsOnly').checked;
  const newOnes = r.recent.slice(pingShownCount);
  for (const p of newOnes) {
    if (errOnly && p.ok) continue;
    logEntry(p.line + '\n', p.ok ? 'ok' : 'err');
  }
  pingShownCount = r.recent.length;
  // Status
  const sent = r.sent, recv = r.received, lost = r.lost;
  const lossPct = sent ? (lost / sent * 100).toFixed(1) : '0.0';
  const avg  = recv ? (r.total_ms / recv).toFixed(1) : '0.0';
  const minMs = (r.min_ms == null) ? '-' : r.min_ms.toFixed(1);
  setStatus(`Ping #${sent}: ${recv}/${sent} OK | Min=${minMs} Avg=${avg} Max=${r.max_ms.toFixed(1)} | Verlust=${lossPct}%`);

  if (!r.running) {
    if (pingPollHandle) { clearInterval(pingPollHandle); pingPollHandle = null; }
    $('btnPingStart').disabled = false;
    $('btnPingStop').disabled = true;
    ['pingInterval','pingDuration','chkPingLog','chkPingErrorsOnly'].forEach(id => $(id).disabled = false);
    ['protoUdp','protoHttp'].forEach(id => $(id).disabled = false);

    const summary  = '\n' + '='.repeat(78) + '\n'
                   + `  Ping-Statistik fuer ${r.host}:\n`
                   + `  Gesendet: ${sent}  |  Empfangen: ${recv}  |  Verloren: ${lost} (${lossPct}%)\n`
                   + `  RTT: Min=${minMs}ms  Avg=${avg}ms  Max=${r.max_ms.toFixed(1)}ms\n`
                   + '='.repeat(78) + '\n';
    logEntry(summary, 'warn');
    $('pingStatus').textContent = `Beendet (${sent} Pings)`;
    $('pingStatus').style.color = '#888';
    if (r.log_file) {
      setStatus(`Ping beendet. Log: ${r.log_file}`);
    } else {
      setStatus(`Ping beendet: ${recv}/${sent} empfangen, ${lossPct}% Verlust, Avg=${avg}ms`);
    }
  }
}

// ── Clear ──────────────────────────────────────────────────────────────────
$('btnClearLog').addEventListener('click', () => { $('log').textContent = ''; setStatus('Log geleert.'); });
$('btnClearJson').addEventListener('click', () => { $('json').textContent = ''; });

// ── Exit button ────────────────────────────────────────────────────────────
// The Python server runs independently of the browser tab; closing the
// tab leaves the server (and any active ping loop / UDP listener) alive.
// This button POSTs /api/shutdown — the server schedules os._exit after
// a 300 ms grace, and we replace the body with a goodbye page.
$('btnExit').addEventListener('click', async () => {
  if (!confirm('Shelly Tester Server beenden?')) return;
  // Stop any running ping loop / listener cleanly first (cosmetic — the
  // os._exit below would kill them anyway, but this avoids stale state
  // if shutdown ever fails).
  try { await api('/api/ping/stop',     {}); } catch(_) {}
  try { await api('/api/udp/listener',  { action: 'stop' }); } catch(_) {}
  try { await fetch('/api/shutdown', { method: 'POST' }); } catch(_) {}
  document.body.innerHTML =
    '<div style="font:1.2em sans-serif;padding:60px;text-align:center;color:#b4b4b4;background:#1e1e1e;height:100vh;margin:-14px;">' +
    '<h2 style="color:#fff">Shelly / EcoTracker Tester beendet.</h2>' +
    '<p>Du kannst dieses Fenster schliessen.</p>' +
    '<p style="margin-top:30px;font-size:0.85em;opacity:0.6;">' +
    'Ursprung: <a href="https://ottelo.jimdofree.com/" target="_blank" style="color:#64b4ff">ottelo.jimdofree.com</a>' +
    '</p></div>';
});

// ── Init ───────────────────────────────────────────────────────────────────
// Load saved values first so updateMode() sees the right radio-button
// state and the right per-mode port. Then call updateMode() to render
// the correct panel + presets, then attach listeners that save on any
// further change.
lsLoad();
updateMode();
lsAttachAll();
</script>
</body>
</html>
"""


# ─── HTTP server ─────────────────────────────────────────────────────────────
class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return  # silence default access logs

    def _send_json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_body(self):
        n = int(self.headers.get('Content-Length', 0) or 0)
        if not n:
            return {}
        raw = self.rfile.read(n)
        try:
            return json.loads(raw.decode('utf-8'))
        except Exception:
            return {}

    def do_GET(self):
        path = urlparse(self.path).path
        if path == '/' or path == '/index.html':
            page = HTML_PAGE.replace('__VERSION__', APP_VERSION)
            data = page.encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        if path == '/api/udp/poll':
            with state_lock:
                pkts = list(udp_listener_packets)
                udp_listener_packets.clear()
            return self._send_json({'packets': pkts})
        if path == '/api/ping/status':
            with state_lock:
                # serialize a copy
                out = {k: v for k, v in ping_state.items() if k != 'recent'}
                out['recent'] = list(ping_state['recent'])
                # min_ms might be None — serialize as None
            return self._send_json(out)
        self.send_response(404); self.end_headers()

    def do_POST(self):
        path = urlparse(self.path).path
        body = self._read_body()
        if path == '/api/udp/send':
            host = (body.get('host') or '').strip()
            port = int(body.get('port') or 0)
            payload = body.get('payload') or ''
            use_listener = bool(body.get('use_listener'))
            res = udp_send_recv(host, port, payload, use_listener=use_listener)
            return self._send_json(res)
        if path == '/api/udp/listener':
            action = body.get('action')
            if action == 'start':
                try:
                    p = udp_listener_start()
                    return self._send_json({'ok': True, 'port': p})
                except Exception as e:
                    return self._send_json({'ok': False, 'error': str(e)})
            elif action == 'stop':
                udp_listener_stop()
                return self._send_json({'ok': True})
            return self._send_json({'ok': False, 'error': 'unknown action'}, 400)
        if path == '/api/http/get':
            res = http_get(
                (body.get('host') or '').strip(),
                str(body.get('port') or '').strip(),
                body.get('path') or '/',
            )
            return self._send_json(res)
        if path == '/api/ping/start':
            host = (body.get('host') or '').strip()
            if not host:
                return self._send_json({'ok': False, 'error': 'no host'})
            ok, info = ping_start(
                host,
                float(body.get('interval_s') or 1),
                int(body.get('duration_s') or 0),
                bool(body.get('errors_only')),
                bool(body.get('write_log')),
            )
            return self._send_json({'ok': ok, 'log_file': info if ok else None,
                                    'error': info if not ok else None})
        if path == '/api/ping/stop':
            ping_stop()
            return self._send_json({'ok': True})
        if path == '/api/shutdown':
            self._send_json({'ok': True})
            threading.Timer(0.3, lambda: os._exit(0)).start()
            return
        self.send_response(404); self.end_headers()


def main():
    ap = argparse.ArgumentParser(description='Shelly / EcoTracker Tester (cross-platform)')
    ap.add_argument('--port', type=int, default=DEFAULT_PORT)
    ap.add_argument('--no-browser', action='store_true', help="don't auto-open the browser")
    args = ap.parse_args()

    server = HTTPServer(('127.0.0.1', args.port), Handler)
    url = f'http://127.0.0.1:{args.port}/'
    print(f'Shelly Tester {APP_VERSION} — listening on {url}')
    print('Cross-platform port of ottelo.jimdofree.com Shelly/EcoTracker Tester')
    print('Press Ctrl+C to stop.')

    if not args.no_browser:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nStopping.')
        udp_listener_stop()
        ping_stop()


if __name__ == '__main__':
    main()
