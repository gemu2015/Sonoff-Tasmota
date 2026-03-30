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

import os, sys, json, threading, time, struct, socket, webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import io

HTTP_PORT   = 8099
MODBUS_PORT = 1502
HTML_FILE   = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sml_emulator.html')

# ── Shared state ──────────────────────────────────────────────────────────────
state_lock      = threading.Lock()
serial_port     = None          # pyserial Serial object
serial_baud     = 9600
serial_log      = []            # list of dicts: {t, type, msg}
tcp_clients     = 0             # active Modbus TCP connections
last_http_req   = 0.0           # timestamp of last HTTP request (for browser watchdog)
browser_opened  = False         # whether browser has been opened at least once

# Register bank: maps Modbus register address → float value
# Updated by POST /api/regs (pushed from JS computeLiveValues every second)
reg_bank = {}

def log_entry(typ, msg):
    with state_lock:
        serial_log.append({'t': time.strftime('%H:%M:%S'), 'type': typ, 'msg': msg})
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

def _process_modbus_rtu():
    global rx_buf, serial_port
    while len(rx_buf) >= 8:
        frame = bytes(rx_buf[:8])
        resp = _handle_modbus_rtu_frame(frame)
        if resp:
            rx_buf = rx_buf[8:]
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
    if len(buf) < 8:
        return None
    addr      = buf[0]
    fc        = buf[1]
    start_reg = (buf[2] << 8) | buf[3]
    reg_count = (buf[4] << 8) | buf[5]
    crc_rx    = buf[6] | (buf[7] << 8)
    if crc16modbus(buf[:6]) != crc_rx:
        return None
    if addr != 1 or fc not in (0x03, 0x04):
        return None
    if reg_count != 2:
        return None
    val = _get_reg_value(start_reg)
    float_bytes = struct.pack('>f', val)
    resp = bytes([addr, fc, 4]) + float_bytes
    crc  = crc16modbus(resp)
    return resp + bytes([crc & 0xFF, crc >> 8])

def _get_reg_value(reg: int) -> float:
    with state_lock:
        return float(reg_bank.get(reg, 0))

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
            while len(buf) >= 12:
                # MBAP header: trans_id(2) + proto(2) + length(2) + unit_id(1) = 7 bytes
                # PDU: fc(1) + reg_hi(1) + reg_lo(1) + cnt_hi(1) + cnt_lo(1) = 5 bytes → total 12
                trans_id  = (buf[0] << 8) | buf[1]
                pdu_len   = (buf[4] << 8) | buf[5]   # length field = unit_id + PDU bytes
                total_len = 6 + pdu_len               # MBAP header (6) + pdu_len
                if len(buf) < total_len:
                    break
                unit_id   = buf[6]
                fc        = buf[7]
                start_reg = (buf[8] << 8) | buf[9]
                reg_count = (buf[10] << 8) | buf[11]
                buf = buf[total_len:]

                if fc not in (0x03, 0x04):
                    # Exception: illegal function
                    pdu_resp = bytes([fc | 0x80, 0x01])
                    mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                    conn.sendall(mbap + pdu_resp)
                    continue

                # Build float response for each pair of registers
                payload = b''
                for i in range(0, reg_count, 2):
                    val = _get_reg_value(start_reg + i)
                    payload += struct.pack('>f', val)

                pdu_resp = bytes([fc, len(payload)]) + payload
                mbap = struct.pack('>HHHB', trans_id, 0, len(pdu_resp) + 1, unit_id)
                conn.sendall(mbap + pdu_resp)
                log_entry('tx', f'Modbus TCP  reg=0x{start_reg:04x} cnt={reg_count}  {addr[0]}')
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
const _origComputeLiveValues = computeLiveValues;
computeLiveValues = function() {
  _origComputeLiveValues();
  // Push current live values to Python (best-effort, no await)
  const regs = {
    0x0000: live.vL1,
    0x0002: live.vL2,
    0x0004: live.vL3,
    0x0006: live.iL1,
    0x0008: live.iL2,
    0x000A: live.iL3,
    0x000C: live.pL1,
    0x000E: live.pL2,
    0x0010: live.pL3,
    0x0012: live.pNet,
    0x001E: live.freq,
    0x0046: Math.sqrt(3) * live.vL1,
    0x0048: live.energyIn,
    0x004A: live.energyOut
  };
  fetch(BASE + '/api/regs', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(regs)
  }).catch(() => {});
};

// ── For TCP profiles: push registers every second (no sendFrame loop runs) ────
setInterval(() => {
  if (isTcpProfile()) computeLiveValues();
}, 1000);

// ── Poll Python log entries ───────────────────────────────────────────────────
let lastLogIdx = 0;
async function pollLog() {
  try {
    const r = await fetch(BASE + `/api/status?since=${lastLogIdx}`);
    if (!r.ok) return;
    const j = await r.json();
    for (const e of (j.log || [])) {
      log(e.type === 'err' ? 'err' : e.type === 'tx' ? 'tx' : 'info',
          `[py] ${e.msg}`);
    }
    lastLogIdx = j.log_idx;
  } catch(_) {}
}
setInterval(pollLog, 1000);

// ── Init ──────────────────────────────────────────────────────────────────────
setupConnectUI();

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
    """Reopen browser automatically if no HTTP activity for 15 s after first use."""
    global last_http_req, browser_opened
    while True:
        time.sleep(5)
        if browser_opened and last_http_req > 0 and (time.time() - last_http_req) > 15:
            last_http_req = time.time()   # reset so we don't spam
            webbrowser.open(url)

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
        elif path == '/api/status':
            qs    = parse_qs(parsed.query)
            since = int(qs.get('since', ['0'])[0])
            with state_lock:
                total = len(serial_log)
                chunk = serial_log[since:] if since < total else []
                port_open = serial_port is not None and serial_port.is_open
            self._json({'log': chunk, 'log_idx': total,
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
            with state_lock:
                for k, v in body.items():
                    reg_bank[int(k)] = float(v) if v is not None else 0.0
            self._json({'ok': True})
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
        webbrowser.open(url)
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
        webbrowser.open(url)
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
