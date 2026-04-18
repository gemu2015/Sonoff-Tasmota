#!/usr/bin/env python3
"""
SML Emulator Server — Python hybrid app
Serves sml_emulator.html with injected bridge script that replaces Web Serial
calls with HTTP API calls.  Provides:
  - Serial port access via pyserial
  - Modbus TCP slave on port 1502 (always running for sdm630_tcp profile)

Usage:  python3 sml_emulator_server.py
Deps:   pip install pyserial
"""

import os, sys, json, threading, time, struct, socket, webbrowser, subprocess, random
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import io

def open_browser_reliable(url):
    """Open a URL using the OS-native handler (more reliable than webbrowser.open
    on macOS, where webbrowser.open sometimes raises Safari without navigating)."""
    try:
        if sys.platform == 'darwin':
            subprocess.Popen(['open', url])
            return True
        if sys.platform.startswith('win'):
            os.startfile(url)  # type: ignore[attr-defined]
            return True
        # Linux / BSD
        subprocess.Popen(['xdg-open', url],
                         stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
        return True
    except Exception:
        try:
            return webbrowser.open(url)
        except Exception:
            return False

HTTP_PORT   = 8099
MODBUS_PORT = 1502
HTML_FILE   = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sml_emulator.html')

# ── Shared state ──────────────────────────────────────────────────────────────
state_lock      = threading.Lock()
serial_port     = None          # pyserial Serial object
serial_baud     = 9600
serial_log      = []            # list of dicts: {seq, t, type, msg}
log_seq         = 0             # monotonic counter (never resets, survives ring-buffer rotation)
tcp_clients     = 0             # active Modbus TCP connections
last_http_req   = 0.0           # timestamp of last HTTP request (for browser watchdog)
browser_opened  = False         # whether browser has been opened at least once

# Register bank: maps Modbus register address → float value
# Updated by POST /api/regs (pushed from JS computeLiveValues every second)
reg_bank   = {}
# Write bank: registers written by Modbus master (FC06/FC16)
# Polled by browser via GET /api/writeregs
write_bank = {}

def log_entry(typ, msg):
    global log_seq
    with state_lock:
        log_seq += 1
        serial_log.append({'seq': log_seq, 't': time.strftime('%H:%M:%S'),
                           'type': typ, 'msg': msg})
        if len(serial_log) > 500:
            serial_log.pop(0)

# ── CRC-16 Modbus ─────────────────────────────────────────────────────────────
def crc16modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b & 0xFF
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF

# ── Serial reader thread ───────────────────────────────────────────────────────
rx_buf = bytearray()

def serial_reader():
    global serial_port
    while True:
        try:
            port = None
            with state_lock:
                port = serial_port
            if port is None or not port.is_open:
                time.sleep(0.05)
                continue
            data = port.read(256)
            if not data:
                continue
            with state_lock:
                rx_buf.extend(data)
            # Process Modbus RTU frames (FC04, 8 bytes each)
            _process_modbus_rtu()
        except Exception as e:
            log_entry('err', f'Serial read: {e}')
            time.sleep(0.1)

_FC_NAMES = {0x03: 'FC03 read-holding', 0x04: 'FC04 read-input',
             0x06: 'FC06 write-single',  0x10: 'FC16 write-multi'}

def _describe_rtu_request(req: bytes) -> str:
    if len(req) < 2: return ''
    addr, fc = req[0], req[1]
    name = _FC_NAMES.get(fc, f'FC{fc:02X}')
    if fc in (0x03, 0x04, 0x06) and len(req) >= 6:
        reg = (req[2] << 8) | req[3]
        val = (req[4] << 8) | req[5]
        tag = 'cnt' if fc in (0x03, 0x04) else 'val'
        return f'id={addr} {name} reg=0x{reg:04X} {tag}={val}'
    if fc == 0x10 and len(req) >= 7:
        reg  = (req[2] << 8) | req[3]
        cnt  = (req[4] << 8) | req[5]
        return f'id={addr} {name} reg=0x{reg:04X} cnt={cnt}'
    return f'id={addr} {name}'

def _process_modbus_rtu():
    global rx_buf, serial_port
    while len(rx_buf) >= 8:
        result = _handle_modbus_rtu_frame(bytes(rx_buf))
        if result:
            resp, consumed = result
            req_bytes = bytes(rx_buf[:consumed])
            rx_buf = rx_buf[consumed:]
            log_entry('rx', f'Modbus RTU req  {len(req_bytes)}B  {req_bytes.hex(" ")}  [{_describe_rtu_request(req_bytes)}]')
            try:
                with state_lock:
                    if serial_port and serial_port.is_open:
                        serial_port.write(resp)
                log_entry('tx', f'Modbus RTU resp {len(resp)}B  {resp.hex(" ")}')
            except Exception as e:
                log_entry('err', f'Serial write: {e}')
        else:
            rx_buf = rx_buf[1:]  # discard leading byte, re-scan

def _handle_modbus_rtu_frame(buf: bytes):
    """Returns (response_bytes, bytes_consumed) or None if frame invalid/incomplete."""
    if len(buf) < 8:
        return None
    addr = buf[0]
    fc   = buf[1]
    if addr != 1:
        return None

    # FC03 / FC04 Read
    if fc in (0x03, 0x04):
        if len(buf) < 8: return None
        start_reg = (buf[2] << 8) | buf[3]
        reg_count = (buf[4] << 8) | buf[5]
        crc_rx    = buf[6] | (buf[7] << 8)
        if crc16modbus(buf[:6]) != crc_rx: return None
        if reg_count != 2: return None
        val        = _get_reg_value(start_reg)
        float_bytes = struct.pack('>f', val)
        resp = bytes([addr, fc, 4]) + float_bytes
        crc  = crc16modbus(resp)
        return resp + bytes([crc & 0xFF, crc >> 8]), 8

    # FC06 Write Single Register
    if fc == 0x06:
        if len(buf) < 8: return None
        crc_rx = buf[6] | (buf[7] << 8)
        if crc16modbus(buf[:6]) != crc_rx: return None
        reg    = (buf[2] << 8) | buf[3]
        raw    = (buf[4] << 8) | buf[5]
        _write_reg_word(reg, raw)
        return bytes(buf[:8]), 8  # echo

    # FC16 Write Multiple Registers
    if fc == 0x10:
        if len(buf) < 9: return None
        byte_count = buf[6]
        frame_len  = 7 + byte_count + 2
        if len(buf) < frame_len: return None
        crc_rx = buf[frame_len-2] | (buf[frame_len-1] << 8)
        if crc16modbus(buf[:frame_len-2]) != crc_rx: return None
        start_reg = (buf[2] << 8) | buf[3]
        reg_count = (buf[4] << 8) | buf[5]
        for i in range(0, reg_count, 2):
            off  = 7 + i * 2
            hi   = (buf[off] << 8)   | buf[off+1]
            lo   = (buf[off+2] << 8) | buf[off+3]
            val  = struct.unpack('>f', struct.pack('>HH', hi, lo))[0]
            _set_write_reg(start_reg + i, val)
        resp = bytes([addr, 0x10, buf[2], buf[3], buf[4], buf[5]])
        crc  = crc16modbus(resp)
        return resp + bytes([crc & 0xFF, crc >> 8]), frame_len

    return None

# FC06 word buffer for float32 assembly (two consecutive FC06 writes = one float)
_fc06_word_buf = {}

def _write_reg_word(reg: int, raw: int):
    """Buffer a single FC06 uint16 write; assemble float32 when both words received."""
    even = (reg & 1) == 0
    if even:
        _fc06_word_buf[reg] = raw
    else:
        hi_reg = reg - 1
        hi = _fc06_word_buf.pop(hi_reg, 0)
        val = struct.unpack('>f', struct.pack('>HH', hi, raw))[0]
        _set_write_reg(hi_reg, val)

def _set_write_reg(reg: int, val: float):
    with state_lock:
        write_bank[reg] = val
    log_entry('info', f'WRITE reg=0x{reg:04X} = {val:.3f}')

# Per-read fluctuation: so every Modbus read returns a slightly different value,
# not just every 1s when the browser re-POSTs. Browser can set pct via /api/regs
# (key "_fluct_pct"). Registers in _NO_FLUCT_REGS pass through unchanged
# (e.g. energy counters, tariff codes).
fluct_pct       = 0.0
_NO_FLUCT_REGS  = {0x0048, 0x004A}  # energyIn, energyOut

def _get_reg_value(reg: int) -> float:
    with state_lock:
        if reg in write_bank:
            return float(write_bank[reg])
        base = float(reg_bank.get(reg, 0))
        pct  = fluct_pct
    if pct <= 0 or base == 0 or reg in _NO_FLUCT_REGS:
        return base
    noise = base * (pct / 100.0) * (random.random() * 2 - 1)
    val   = base + noise
    return val if val > 0 else 0.0

# ── Modbus TCP slave ───────────────────────────────────────────────────────────
def modbus_tcp_server():
    global tcp_clients
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind(('', MODBUS_PORT))
    except OSError as e:
        log_entry('err', f'Modbus TCP bind port {MODBUS_PORT}: {e}')
        return
    srv.listen(4)
    log_entry('info', f'Modbus TCP slave listening on port {MODBUS_PORT}')
    while True:
        try:
            conn, addr = srv.accept()
            threading.Thread(target=modbus_tcp_client, args=(conn, addr), daemon=True).start()
        except Exception as e:
            log_entry('err', f'Modbus TCP accept: {e}')

def modbus_tcp_client(conn, addr):
    global tcp_clients
    with state_lock:
        tcp_clients += 1
    log_entry('info', f'Modbus TCP client {addr[0]}:{addr[1]}')
    try:
        buf = b''
        while True:
            data = conn.recv(256)
            if not data:
                break
            buf += data
            while len(buf) >= 8:
                # MBAP header: trans_id(2) + proto(2) + length(2) + unit_id(1)
                pdu_len   = (buf[4] << 8) | buf[5]   # length field = unit_id + PDU bytes
                # Sanity-check MBAP length. A valid Modbus PDU is at most 253
                # bytes, +1 unit_id = 254. Protocol ID (bytes 2-3) must be 0.
                # Anything else means we're parsing garbage — usually a truncated
                # frame from a client that miscomputed its MBAP header.
                proto_id  = (buf[2] << 8) | buf[3]
                if proto_id != 0 or pdu_len < 2 or pdu_len > 254:
                    log_entry('err',
                        f'Modbus TCP desync from {addr[0]}: proto_id=0x{proto_id:04X} '
                        f'pdu_len={pdu_len} — closing connection')
                    return
                total_len = 6 + pdu_len
                if len(buf) < total_len:
                    break
                trans_id  = (buf[0] << 8) | buf[1]
                unit_id   = buf[6]
                fc        = buf[7]
                # Need at least [fc][regHi][regLo][cntHi][cntLo] for the ops we support
                if total_len < 12:
                    log_entry('err',
                        f'Modbus TCP short frame from {addr[0]}: '
                        f'{bytes(buf[:total_len]).hex(" ")} — closing connection')
                    return
                start_reg = (buf[8] << 8) | buf[9]
                reg_count = (buf[10] << 8) | buf[11]
                req_frame = bytes(buf[:total_len])
                buf_pdu   = buf[7:total_len]
                buf = buf[total_len:]

                fc_name = _FC_NAMES.get(fc, f'FC{fc:02X}')
                log_entry('rx', f'Modbus TCP req  {len(req_frame)}B  {req_frame.hex(" ")}  [id={unit_id} {fc_name} reg=0x{start_reg:04X} cnt={reg_count} from {addr[0]}]')

                if fc in (0x03, 0x04):
                    # Read registers — build float response for each pair
                    payload = b''
                    for i in range(0, reg_count, 2):
                        val = _get_reg_value(start_reg + i)
                        payload += struct.pack('>f', val)
                    pdu_resp = bytes([fc, len(payload)]) + payload
                    mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                    frame = mbap + pdu_resp
                    conn.sendall(frame)
                    log_entry('tx', f'Modbus TCP resp {len(frame)}B  {frame.hex(" ")}')

                elif fc == 0x06:
                    # Write Single Register — PDU layout: [fc][regHi][regLo][valHi][valLo]
                    raw = (buf_pdu[3] << 8) | buf_pdu[4]  # value at PDU bytes 3-4
                    _write_reg_word(start_reg, raw)
                    # echo back
                    pdu_resp = buf_pdu[:6]
                    mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                    frame = mbap + pdu_resp
                    conn.sendall(frame)
                    log_entry('tx', f'Modbus TCP resp {len(frame)}B  {frame.hex(" ")}')

                elif fc == 0x10:
                    # Write Multiple Registers — PDU: [fc][regHi][regLo][cntHi][cntLo][byteCount][data...]
                    # Expected PDU length: 6 header + byteCount; MBAP length = 1 (unit) + PDU
                    if len(buf_pdu) < 6 or len(buf_pdu) < 6 + buf_pdu[5] or buf_pdu[5] != reg_count * 2:
                        # Truncated / inconsistent — reject with exception 0x03 (illegal data value)
                        pdu_resp = bytes([fc | 0x80, 0x03])
                        mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                        frame = mbap + pdu_resp
                        conn.sendall(frame)
                        log_entry('err',
                            f'Modbus TCP FC16 rejected (truncated): '
                            f'pdu_len={len(buf_pdu)} byte_count={buf_pdu[5] if len(buf_pdu) > 5 else "?"} '
                            f'expected_bc={reg_count*2}')
                        continue
                    data_bytes = buf_pdu[6:6 + buf_pdu[5]]
                    for i in range(0, reg_count, 2):
                        off = i * 2
                        if off + 4 > len(data_bytes): break
                        hi  = (data_bytes[off] << 8)   | data_bytes[off+1]
                        lo  = (data_bytes[off+2] << 8) | data_bytes[off+3]
                        val = struct.unpack('>f', struct.pack('>HH', hi, lo))[0]
                        _set_write_reg(start_reg + i, val)
                    pdu_resp = bytes([0x10, buf_pdu[1], buf_pdu[2], buf_pdu[3], buf_pdu[4]])
                    mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                    frame = mbap + pdu_resp
                    conn.sendall(frame)
                    log_entry('tx', f'Modbus TCP resp {len(frame)}B  {frame.hex(" ")}')

                else:
                    # Exception: illegal function
                    pdu_resp = bytes([fc | 0x80, 0x01])
                    mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                    frame = mbap + pdu_resp
                    conn.sendall(frame)
                    log_entry('tx', f'Modbus TCP resp {len(frame)}B  {frame.hex(" ")}')
    except Exception as e:
        pass
    finally:
        with state_lock:
            tcp_clients -= 1
        try:
            conn.close()
        except Exception:
            pass

# ── List serial ports ─────────────────────────────────────────────────────────
def list_ports():
    try:
        from serial.tools import list_ports as lp
        return [{'device': p.device, 'desc': p.description} for p in lp.comports()]
    except ImportError:
        return []

# ── HTTP handler ───────────────────────────────────────────────────────────────
BRIDGE_SCRIPT = r"""
<script>
/* ── SML Emulator Server Bridge ── */
(function() {
'use strict';

const BASE = 'http://localhost:""" + str(HTTP_PORT) + r"""';

// ── Port selector UI ─────────────────────────────────────────────────────────
async function buildPortSelector() {
  const resp = await fetch(BASE + '/api/ports').catch(() => null);
  const ports = resp ? await resp.json().catch(() => []) : [];
  const sel = document.createElement('select');
  sel.id = 'bridgePortSel';
  sel.style.cssText = 'width:160px;background:#0a1628;border:1px solid #1a4a7a;color:#e0e0e0;padding:5px 8px;border-radius:4px;font-size:0.81em;';
  const placeholder = document.createElement('option');
  placeholder.value = '';
  placeholder.textContent = '— select port —';
  sel.appendChild(placeholder);
  ports.forEach(p => {
    const o = document.createElement('option');
    o.value = p.device;
    o.textContent = `${p.device}  ${p.desc || ''}`.trim();
    sel.appendChild(o);
  });
  return sel;
}

// Replace Connect button area
async function setupConnectUI() {
  const btnConn = document.getElementById('btnConnect');
  const btnDisc = document.getElementById('btnDisconnect');
  if (!btnConn) return;

  // Hide Web Serial warning — we have our own port picker
  const warn = document.getElementById('warnSerial');
  if (warn) warn.style.display = 'none';

  // Insert port selector before Connect button
  const sel = await buildPortSelector();
  btnConn.parentNode.insertBefore(sel, btnConn);
  btnConn.parentNode.insertBefore(document.createTextNode(' '), btnConn);

  // Re-enable even if Web Serial check disabled it (e.g. Safari)
  btnConn.disabled = false;

  // Override Connect button
  btnConn.onclick = async () => {
    const device = sel.value;
    if (!device) { alert('Select a serial port first'); return; }
    const baud = +document.getElementById('selBaud').value;
    const serCfg = document.getElementById('selSerial').value;
    const r = await fetch(BASE + '/api/open', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({device, baud, config: serCfg})
    });
    const j = await r.json();
    if (j.ok) {
      // Create a fake writer proxy
      window._bridgeWriter = {
        write: async (data) => {
          await fetch(BASE + '/api/send', {
            method: 'POST',
            headers: {'Content-Type':'application/octet-stream'},
            body: data
          });
        }
      };
      writer = window._bridgeWriter;
      setConn(true);
      log('info', `Opened ${device} at ${baud} baud ${serCfg}`);
    } else {
      log('err', `Open failed: ${j.error}`);
    }
  };

  // Override Disconnect button
  btnDisc.onclick = async () => {
    stopSending();
    await fetch(BASE + '/api/close', { method: 'POST' });
    writer = null;
    setConn(false);
    log('info', 'Disconnected');
  };
}

// ── Fix stopSending: port is always null in bridge — use writer for state ─────
const _origStopSending = stopSending;
stopSending = function() {
  _origStopSending();
  // Original sets btnStart.disabled=(port===null); port is always null in bridge.
  // Re-enable Start if still connected (writer set) and not in auto-slave mode.
  if (writer && !isModbusRtu() && !isT510()) {
    document.getElementById('btnStart').disabled = false;
  }
};

// ── Override Modbus slave (no-op — Python handles RTU, always-on TCP) ─────────
window.startModbusSlave = function() {
  modbusRunning = true;
  setModbusStatus('#4caf50', '● Python Modbus RTU slave active  |  TCP slave port """ + str(MODBUS_PORT) + r"""');
  log('info', 'Modbus slave running in Python backend');
};
window.stopModbusSlave = function() {
  modbusRunning = false;
};

// ── Auto-push registers every second from computeLiveValues ──────────────────
// Python applies the ±fluct_pct noise on every Modbus read, so a master polling
// faster than 1 Hz sees fresh jitter. We post the BASE values (raw input fields,
// not already-fluctuated live.*) plus the fluctuation percent.
const _origComputeLiveValues = computeLiveValues;
const _numVal = id => +document.getElementById(id).value || 0;
computeLiveValues = function() {
  _origComputeLiveValues();   // still updates live.* for SML frame builders
  const vL1Base = _numVal('vVL1') || 230;
  const vL2Base = _numVal('vVL2') || 230;
  const vL3Base = _numVal('vVL3') || 230;
  const pL1     = Math.max(0, _numVal('vPL1'));
  const pL2     = Math.max(0, _numVal('vPL2'));
  const pL3     = Math.max(0, _numVal('vPL3'));
  const pIn     = Math.max(0, _numVal('vPowerIn'));
  const pOut    = Math.max(0, _numVal('vPowerOut'));
  const fBase   = _numVal('vFreq') || 50;
  const pct     = +document.getElementById('selFluct').value || 0;
  const regs = {
    0x0000: vL1Base,
    0x0002: vL2Base,
    0x0004: vL3Base,
    0x0006: vL1Base > 0 ? pL1 / vL1Base : 0,
    0x0008: vL2Base > 0 ? pL2 / vL2Base : 0,
    0x000A: vL3Base > 0 ? pL3 / vL3Base : 0,
    0x000C: pL1,
    0x000E: pL2,
    0x0010: pL3,
    0x0012: pIn - pOut,
    0x001E: fBase,
    0x0046: Math.sqrt(3) * vL1Base,
    0x0048: live.energyIn,   // energy accumulates — passed through, no jitter
    0x004A: live.energyOut,
    _fluct_pct: pct
  };
  fetch(BASE + '/api/regs', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(regs)
  }).catch(() => {});
};

// ── For TCP / Modbus-RTU profiles: push registers every second ────────────────
// (neither of these runs the sendFrame loop that would normally call computeLiveValues)
setInterval(() => {
  if (isTcpProfile() || (typeof isModbusRtu === 'function' && isModbusRtu())) {
    computeLiveValues();
  }
}, 1000);

// ── Poll writable registers written by TCP master ────────────────────────────
async function pollWriteRegs() {
  try {
    const r = await fetch(BASE + '/api/writeregs');
    if (!r.ok) return;
    const data = await r.json();
    for (const [hexAddr, val] of Object.entries(data)) {
      const reg = parseInt(hexAddr, 16);
      const idx = wregStore.findIndex(r => r.addr === reg || r.addr === (reg & ~1));
      if (idx < 0) continue;
      if (wregStore[idx].value === val) continue;
      wregStore[idx].value = val;
      const el = document.getElementById(`wregVal${idx}`);
      if (el) {
        el.textContent = isFinite(val) ? val.toFixed(3) : String(val);
        el.classList.add('wreg-written');
        setTimeout(() => el.classList.remove('wreg-written'), 800);
      }
    }
  } catch(_) {}
}
setInterval(pollWriteRegs, 1000);

// ── Poll Python log entries ───────────────────────────────────────────────────
let lastLogIdx = 0;
async function pollLog() {
  try {
    const r = await fetch(BASE + `/api/status?since=${lastLogIdx}`);
    if (!r.ok) return;
    const j = await r.json();
    for (const e of (j.log || [])) {
      const t = (e.type === 'err' || e.type === 'tx' || e.type === 'rx') ? e.type : 'info';
      log(t, `[py] ${e.msg}`);
    }
    lastLogIdx = j.log_idx;
  } catch(_) {}
}
setInterval(pollLog, 1000);

// ── Init ──────────────────────────────────────────────────────────────────────
setupConnectUI();

// ── RX log style (added by bridge; base HTML only has tx/info/err) ───────────
(function () {
  const css = document.createElement('style');
  css.textContent = '.log-rx { color: #b2ff59; }';
  document.head.appendChild(css);
})();

// ── Quit button (bottom-right, away from the Connect controls) ───────────────
(function () {
  const btn = document.createElement('button');
  btn.textContent = 'Quit Server';
  btn.title = 'Stops the Python server and closes this emulator.';
  btn.style.cssText =
    'position:fixed;bottom:10px;right:12px;z-index:9999;' +
    'background:#c0392b;color:#fff;border:0;border-radius:4px;' +
    'padding:6px 12px;font-size:0.8em;cursor:pointer;opacity:0.85;' +
    'box-shadow:0 2px 6px rgba(0,0,0,0.25);';
  btn.onmouseenter = () => { btn.style.background = '#e74c3c'; btn.style.opacity = '1'; };
  btn.onmouseleave = () => { btn.style.background = '#c0392b'; btn.style.opacity = '0.85'; };
  btn.onclick = async () => {
    if (!confirm('Stop the SML Emulator server?')) return;
    try { await fetch(BASE + '/api/shutdown', { method: 'POST' }); } catch(_) {}
    document.body.innerHTML =
      '<div style="font:1.2em sans-serif;padding:40px;text-align:center;">' +
      'SML Emulator stopped.<br><small>You can close this tab.</small></div>';
  };
  document.body.appendChild(btn);
})();

// For sdm630_tcp profile: Modbus TCP slave always runs in Python, no UI needed
const tcpStatusEl = document.createElement('div');
tcpStatusEl.style.cssText = 'font-size:0.73em;color:#4caf50;padding:4px 0;';
tcpStatusEl.textContent = `Modbus TCP slave: port """ + str(MODBUS_PORT) + r"""`;
const tcpPanel = document.getElementById('tcpPanel');
if (tcpPanel) {
  const sep = tcpPanel.querySelector('h2');
  if (sep) sep.after(tcpStatusEl);
}

})();

// Fix Modbus TCP port to match Python server — outside IIFE for reliability
(function() {
  var portEl = document.getElementById('inpBridgePort');
  if (portEl) {
    portEl.value = """ + str(MODBUS_PORT) + r""";
    portEl.dispatchEvent(new Event('input'));
    if (typeof generateScript === 'function') generateScript();
  }
})();
</script>
</body>
"""

def browser_watchdog(url):
    """Reopen browser automatically if no HTTP activity for 8 s after first use."""
    global last_http_req, browser_opened
    while True:
        time.sleep(2)
        if browser_opened and last_http_req > 0 and (time.time() - last_http_req) > 8:
            last_http_req = time.time()   # reset so we don't spam
            open_browser_reliable(url)

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass   # suppress default access log

    def do_OPTIONS(self):
        self._cors()
        self.send_response(204)
        self.end_headers()

    def _cors(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')

    def _touch(self):
        global last_http_req
        last_http_req = time.time()

    def do_GET(self):
        self._touch()
        parsed = urlparse(self.path)
        path   = parsed.path

        if path == '/':
            self._serve_html()
        elif path == '/api/ports':
            self._json(list_ports())
        elif path == '/api/writeregs':
            with state_lock:
                data = {f'0x{k:04X}': v for k, v in write_bank.items()}
            self._json(data)
        elif path == '/api/status':
            qs    = parse_qs(parsed.query)
            since = int(qs.get('since', ['0'])[0])
            with state_lock:
                # If the client is "ahead" of the server (server was restarted
                # → log_seq reset, but browser tab still holds old lastLogIdx)
                # start from 0 and return everything available.
                if since > log_seq:
                    since = 0
                # Entries with seq > since are new to the client.
                # After ring-buffer rotation, serial_log[0].seq > 0, so clients
                # that fell behind still resync from the oldest retained entry
                # (they miss at most the overflow).
                chunk = [e for e in serial_log if e['seq'] > since]
                current_seq = log_seq
                port_open = serial_port is not None and serial_port.is_open
            self._json({'log': chunk, 'log_idx': current_seq,
                        'port_open': port_open, 'tcp_clients': tcp_clients})
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        self._touch()
        path = urlparse(self.path).path

        if path == '/api/open':
            body = self._read_json()
            self._open_serial(body.get('device',''), body.get('baud', 9600),
                              body.get('config', '8N1'))
        elif path == '/api/close':
            self._close_serial()
            self._json({'ok': True})
        elif path == '/api/send':
            data = self.rfile.read(int(self.headers.get('Content-Length', 0)))
            self._serial_write(data)
            self._json({'ok': True, 'bytes': len(data)})
        elif path == '/api/regs':
            body = self._read_json()
            global fluct_pct
            with state_lock:
                for k, v in body.items():
                    if k == '_fluct_pct':
                        fluct_pct = float(v) if v is not None else 0.0
                        continue
                    reg_bank[int(k)] = float(v) if v is not None else 0.0
            self._json({'ok': True})
        elif path == '/api/shutdown':
            self._json({'ok': True})
            log_entry('info', 'Shutdown requested via HTTP')
            # Exit after the response has been flushed
            threading.Timer(0.3, lambda: os._exit(0)).start()
        else:
            self.send_response(404)
            self.end_headers()

    # ── helpers ──────────────────────────────────────────────────────────────
    def _serve_html(self):
        try:
            with open(HTML_FILE, 'r', encoding='utf-8') as f:
                html = f.read()
        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()
            return
        # Inject bridge before </body>
        html = html.replace('</body>', BRIDGE_SCRIPT, 1)
        data = html.encode('utf-8')
        self.send_response(200)
        self._cors()
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _json(self, obj):
        data = json.dumps(obj).encode()
        self.send_response(200)
        self._cors()
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _read_json(self):
        length = int(self.headers.get('Content-Length', 0))
        raw    = self.rfile.read(length)
        try:
            return json.loads(raw)
        except Exception:
            return {}

    def _open_serial(self, device, baud, config):
        global serial_port, rx_buf
        try:
            import serial as pyserial
        except ImportError:
            self._json({'ok': False, 'error': 'pyserial not installed (pip install pyserial)'})
            return
        self._close_serial(quiet=True)
        try:
            # Parse config string e.g. "8N1", "7E1"
            data_bits = int(config[0]) if config else 8
            parity_c  = config[1].upper() if len(config) > 1 else 'N'
            stop_bits = int(config[2]) if len(config) > 2 else 1
            parity_map = {'N': pyserial.PARITY_NONE,
                          'E': pyserial.PARITY_EVEN,
                          'O': pyserial.PARITY_ODD}
            p = pyserial.Serial(
                port=device, baudrate=baud,
                bytesize=data_bits,
                parity=parity_map.get(parity_c, pyserial.PARITY_NONE),
                stopbits=stop_bits,
                timeout=0.05
            )
            with state_lock:
                serial_port = p
                rx_buf.clear()
            log_entry('info', f'Opened {device} {baud} {config}')
            self._json({'ok': True})
        except Exception as e:
            self._json({'ok': False, 'error': str(e)})

    def _close_serial(self, quiet=False):
        global serial_port
        with state_lock:
            p = serial_port
            serial_port = None
        if p:
            try:
                p.close()
            except Exception:
                pass
            if not quiet:
                log_entry('info', 'Serial port closed')

    def _serial_write(self, data: bytes):
        with state_lock:
            p = serial_port
        if p and p.is_open:
            try:
                p.write(data)
                log_entry('tx', f'TX {len(data)}B  {data[:32].hex(" ")}')
            except Exception as e:
                log_entry('err', f'Serial write: {e}')


# ── Main ───────────────────────────────────────────────────────────────────────
def main():
    if not os.path.exists(HTML_FILE):
        print(f'ERROR: {HTML_FILE} not found', file=sys.stderr)
        sys.exit(1)

    url = f'http://localhost:{HTTP_PORT}/'

    # If server already running (browser was closed), just reopen the browser
    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    probe.settimeout(0.5)
    try:
        probe.connect(('127.0.0.1', HTTP_PORT))
        probe.close()
        print(f'Server already running — reopening browser at {url}')
        # Double-open trick for macOS: first call may be ignored if Safari is
        # already foreground but on a different page.
        open_browser_reliable(url)
        time.sleep(0.3)
        open_browser_reliable(url)
        return
    except (ConnectionRefusedError, OSError):
        pass
    finally:
        try: probe.close()
        except: pass

    # Start Modbus TCP slave
    threading.Thread(target=modbus_tcp_server, daemon=True).start()

    # Start serial reader
    threading.Thread(target=serial_reader, daemon=True).start()

    # Start HTTP server (SO_REUSEADDR so restart after crash doesn't fail)
    HTTPServer.allow_reuse_address = True
    server = HTTPServer(('127.0.0.1', HTTP_PORT), Handler)
    print(f'SML Emulator Server')
    print(f'  HTTP:      {url}')
    print(f'  Modbus TCP port: {MODBUS_PORT}')
    print(f'  Press Ctrl+C to stop')

    def _open_browser():
        global browser_opened, last_http_req
        open_browser_reliable(url)
        browser_opened = True
        last_http_req  = time.time()
    threading.Timer(0.8, _open_browser).start()
    threading.Thread(target=browser_watchdog, args=(url,), daemon=True).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nStopped.')

if __name__ == '__main__':
    main()
