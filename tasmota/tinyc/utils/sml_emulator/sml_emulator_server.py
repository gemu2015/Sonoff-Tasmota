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

# Coil banks for FC01/FC02/FC05/FC15 (Read Coils / Read Discrete Inputs /
# Write Single Coil / Write Multiple Coils). Each maps absolute coil-address
# (uint16) → 0/1.
#
# These were added when we introduced the EPEVER Tracer-AN MPPT profile —
# that device exposes:
#   • FC01 coils (writable on/off flags: manual load control, force-charge,
#     factory-reset trigger)
#   • FC02 discrete inputs (read-only flags: charging state bulk/absorb/
#     float, over-temp alarm, day/night sensor, battery-low alarm)
# Existing energy-meter profiles (SDM630 etc.) populate only reg_bank, so
# both new banks stay empty for them — the FC01/FC02 handlers just respond
# with zero coils, which is correct behaviour for "register doesn't exist".
#
# Browser pushes initial values (and any user-toggled values from the
# emulator UI) via POST /api/coils and POST /api/dinputs. FC05/FC15 writes
# update coil_bank in place; the browser polls (or a future websocket) to
# observe master-driven coil changes.
coil_bank      = {}   # addr → 0/1 (FC01 read, FC05/FC15 write)
discrete_bank  = {}   # addr → 0/1 (FC02 read; read-only per spec — no FC for write)

# Per-register encoding override for FC03/FC04 reads. By default, the FC03/04
# handler responds with IEEE-754 float32 BE (legacy SDM630 behaviour). For
# devices like EPEVER Tracer-AN that use plain scaled uint16/int16 per
# register, set reg_format[addr] = 'u16' or 's16' so the wire bytes match
# what xsns_53_sml.ino's `UUuu` / `SSss` patterns decode. Browser pushes via
# POST /api/reg_format on profile-select.
reg_format = {}       # addr → 'u16' | 's16' (absent → 'f32', the default)

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

# T510 mode swaps the reader's semantics: bytes accumulate in rx_buf but we do
# NOT attempt Modbus-RTU parsing on them. The T510 thread consumes rx_buf via
# _t510_take_bytes() whenever it is waiting for a handshake pattern.
t510_running = False     # True while the T510 state machine is active
t510_serial  = 'T510001'
t510_thread  = None
t510_stop    = False

# Kamstrup mode (similar to T510 but for Kamstrup OMNIPOWER / K382 / Kx7
# poll/response protocol). The browser-side `port.readable.getReader()` is
# unavailable under the bridge, and Kamstrup REQUIRES request/response, so
# we run the entire protocol server-side.
ks_running = False
ks_thread  = None
ks_stop    = False

# CAN-bus profile mode. The emulator plays a CAN device (e.g. Huawei
# R4850G2) reached over a network SLCAN bridge (slcan_bridge_tcp.tc on
# an ESP32, TCP host:port). Like Kamstrup it is request/response: the
# DUT (Tasmota + USE_SML_CANBUS) periodically broadcasts a poll frame;
# we answer with the descriptor's response frames. The browser parses
# the .tas (it already has the UU/uu/@scale codec) and pushes a poll
# matcher + prebuilt response frames via /api/can_cfg; this runner
# stays dumb — match poll, blast the current frame set over SLCAN.
can_running = False
can_thread  = None
can_stop    = False
can_cfg     = None       # {'poll': {'id':int,'ext':bool}} poll matcher
can_frames  = []         # [{'id':int,'ext':bool,'data':<hex str>}] responses

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
            # T510 and Kamstrup each have a dedicated state machine that
            # consumes rx_buf. Don't let _process_modbus_rtu() touch it in
            # those modes — it byte-walks the buffer when frames don't
            # validate (line ~155: `rx_buf = rx_buf[1:]`), silently dropping
            # every byte of a non-Modbus protocol before the state machine
            # ever wakes up to read it.
            if not (t510_running or ks_running):
                _process_modbus_rtu()
        except Exception as e:
            log_entry('err', f'Serial read: {e}')
            time.sleep(0.1)

_FC_NAMES = {0x01: 'FC01 read-coils',   0x02: 'FC02 read-disc-in',
             0x03: 'FC03 read-holding', 0x04: 'FC04 read-input',
             0x05: 'FC05 write-coil',   0x06: 'FC06 write-single',
             0x0F: 'FC15 write-coils',  0x10: 'FC16 write-multi'}

def _describe_rtu_request(req: bytes) -> str:
    if len(req) < 2: return ''
    addr, fc = req[0], req[1]
    name = _FC_NAMES.get(fc, f'FC{fc:02X}')
    if fc in (0x01, 0x02, 0x03, 0x04, 0x06) and len(req) >= 6:
        reg = (req[2] << 8) | req[3]
        val = (req[4] << 8) | req[5]
        tag = 'cnt' if fc in (0x01, 0x02, 0x03, 0x04) else 'val'
        return f'id={addr} {name} addr=0x{reg:04X} {tag}={val}'
    if fc == 0x05 and len(req) >= 6:
        coil = (req[2] << 8) | req[3]
        on   = (req[4] == 0xFF)
        return f'id={addr} {name} coil=0x{coil:04X} {"ON" if on else "OFF"}'
    if fc in (0x0F, 0x10) and len(req) >= 7:
        reg  = (req[2] << 8) | req[3]
        cnt  = (req[4] << 8) | req[5]
        return f'id={addr} {name} addr=0x{reg:04X} cnt={cnt}'
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

def _pack_bits(values: list) -> bytes:
    """Pack a list of 0/1 ints (LSB-first per Modbus FC01/FC02 spec) into bytes."""
    n = len(values)
    out = bytearray((n + 7) // 8)
    for i, v in enumerate(values):
        if v:
            out[i // 8] |= (1 << (i % 8))
    return bytes(out)

def _handle_modbus_rtu_frame(buf: bytes):
    """Returns (response_bytes, bytes_consumed) or None if frame invalid/incomplete."""
    if len(buf) < 8:
        return None
    addr = buf[0]
    fc   = buf[1]
    if addr != 1:
        return None

    # FC01 Read Coils / FC02 Read Discrete Inputs — both have identical wire
    # format: request 8 bytes (addr, fc, reg_hi, reg_lo, cnt_hi, cnt_lo, crc_lo,
    # crc_hi); response is (addr, fc, byte_count, packed_bits..., crc_lo, crc_hi)
    # where byte_count = ceil(cnt/8) and bits are packed LSB-first within each
    # byte. Coils respond from coil_bank, discrete inputs from discrete_bank.
    # Master may request up to 2000 bits per Modbus spec; we cap at 2008 (251
    # bytes) which fits the 256-byte ADU.
    if fc in (0x01, 0x02):
        if len(buf) < 8: return None
        start = (buf[2] << 8) | buf[3]
        cnt   = (buf[4] << 8) | buf[5]
        crc_rx = buf[6] | (buf[7] << 8)
        if crc16modbus(buf[:6]) != crc_rx: return None
        if cnt < 1 or cnt > 2000: return None
        bank = coil_bank if fc == 0x01 else discrete_bank
        with state_lock:
            bits = [(1 if bank.get(start + i, 0) else 0) for i in range(cnt)]
        packed = _pack_bits(bits)
        resp = bytes([addr, fc, len(packed)]) + packed
        crc  = crc16modbus(resp)
        return resp + bytes([crc & 0xFF, crc >> 8]), 8

    # FC03 / FC04 Read
    # Default encoding is IEEE-754 float32 BE (4 bytes per request, ignores
    # reg_count > 2) — that matches the SDM630 / generic-Modbus pattern where
    # each "logical reading" is a 32-bit float spanning 2 consecutive
    # 16-bit register slots. Real devices that use plain uint16/int16 per
    # register (EPEVER, growatt, JK BMS, …) need per-register encoding so
    # the response wire bytes line up with what the driver's `UUuu` / `SSss`
    # patterns decode. reg_format[addr] = 'u16'|'s16' overrides the default
    # for that address; the response then carries reg_count consecutive
    # 16-bit registers (2 B each, BE) read from reg_bank.
    if fc in (0x03, 0x04):
        if len(buf) < 8: return None
        start_reg = (buf[2] << 8) | buf[3]
        reg_count = (buf[4] << 8) | buf[5]
        crc_rx    = buf[6] | (buf[7] << 8)
        if crc16modbus(buf[:6]) != crc_rx: return None
        if reg_count < 1 or reg_count > 125: return None
        with state_lock:
            fmt = reg_format.get(start_reg)
        if fmt in ('u16', 's16'):
            data_bytes = bytearray()
            packer = '>h' if fmt == 's16' else '>H'
            mask   = None if fmt == 's16' else 0xFFFF
            for i in range(reg_count):
                v = int(_get_reg_value(start_reg + i))
                if mask is not None: v &= mask
                # int16 wraparound for s16
                if fmt == 's16':
                    if v > 32767:  v -= 65536
                    if v < -32768: v = -32768
                data_bytes += struct.pack(packer, v)
            resp = bytes([addr, fc, len(data_bytes)]) + bytes(data_bytes)
        else:
            # Default: float32 BE per pair of registers. Single-float
            # reads (reg_count=2) → 4-byte response (legacy SDM630
            # behaviour). Bulk reads (reg_count=2N) → N packed floats.
            # Repo descriptors that read N floats in one shot
            # (e.g. `r010400000012` for 9 floats) need this.
            if reg_count % 2 != 0: return None
            data_bytes = bytearray()
            for i in range(0, reg_count, 2):
                val = _get_reg_value(start_reg + i)
                data_bytes += struct.pack('>f', val)
            resp = bytes([addr, fc, len(data_bytes)]) + bytes(data_bytes)
        crc  = crc16modbus(resp)
        return resp + bytes([crc & 0xFF, crc >> 8]), 8

    # FC05 Write Single Coil — value is 0xFF00 (ON) or 0x0000 (OFF).
    # Response echoes the request verbatim (per Modbus spec).
    if fc == 0x05:
        if len(buf) < 8: return None
        crc_rx = buf[6] | (buf[7] << 8)
        if crc16modbus(buf[:6]) != crc_rx: return None
        coil  = (buf[2] << 8) | buf[3]
        valhi = buf[4]
        # Spec is strict: only 0xFF00 (ON) or 0x0000 (OFF) accepted.
        if (buf[4], buf[5]) not in ((0xFF, 0x00), (0x00, 0x00)):
            return None
        with state_lock:
            coil_bank[coil] = 1 if valhi == 0xFF else 0
        log_entry('info', f'WRITE coil 0x{coil:04X} = {"ON" if valhi == 0xFF else "OFF"}')
        return bytes(buf[:8]), 8  # echo

    # FC06 Write Single Register
    if fc == 0x06:
        if len(buf) < 8: return None
        crc_rx = buf[6] | (buf[7] << 8)
        if crc16modbus(buf[:6]) != crc_rx: return None
        reg    = (buf[2] << 8) | buf[3]
        raw    = (buf[4] << 8) | buf[5]
        _write_reg_word(reg, raw)
        return bytes(buf[:8]), 8  # echo

    # FC15 Write Multiple Coils — request frame:
    #   addr, fc, start_hi, start_lo, qty_hi, qty_lo, byte_count, bits..., crc_lo, crc_hi
    # Bits are packed LSB-first within each byte (same as FC01 response).
    # Response echoes addr/fc/start/qty (8 bytes total + crc).
    if fc == 0x0F:
        if len(buf) < 9: return None
        byte_count = buf[6]
        frame_len  = 7 + byte_count + 2
        if len(buf) < frame_len: return None
        crc_rx = buf[frame_len-2] | (buf[frame_len-1] << 8)
        if crc16modbus(buf[:frame_len-2]) != crc_rx: return None
        start = (buf[2] << 8) | buf[3]
        qty   = (buf[4] << 8) | buf[5]
        if qty < 1 or qty > 1968 or byte_count != (qty + 7) // 8:
            return None
        with state_lock:
            for i in range(qty):
                bit_byte = buf[7 + (i // 8)]
                bit      = (bit_byte >> (i % 8)) & 1
                coil_bank[start + i] = bit
        log_entry('info', f'WRITE coils 0x{start:04X}..0x{start+qty-1:04X} ({qty} bits)')
        resp = bytes([addr, 0x0F, buf[2], buf[3], buf[4], buf[5]])
        crc  = crc16modbus(resp)
        return resp + bytes([crc & 0xFF, crc >> 8]), frame_len

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

# ── T510 IEC 62056-21 state machine ───────────────────────────────────────────
# Runs entirely server-side under the Python bridge. The browser-side T510
# listener in sml_emulator.html needs `port.readable.getReader()` which is
# unavailable under the bridge (the browser doesn't own the port), so when
# profile == 't510' we drive the full mode-C handshake here instead.
#
# On-wire bytes from Tasmota's SML driver are emitted 8N1 with bit 7 = even
# parity of the low 7 bits (see SML_Send_Seq in xsns_53_sml.ino). The adapter
# is typically opened 8N1 too, so we mask bit 7 when matching ASCII patterns.

def _t510_take_bytes():
    """Atomically drain rx_buf and return the bytes."""
    global rx_buf
    with state_lock:
        data = bytes(rx_buf)
        rx_buf.clear()
    return data

def _t510_wait_for(pattern_ascii: bytes, timeout_s: float, label: str) -> bool:
    """Wait up to timeout_s for pattern_ascii (7-bit) in the RX stream.
    Any accumulated RX bytes are masked to 7 bits before comparison. Returns
    True if found, False on timeout or stop."""
    deadline = time.time() + timeout_s
    buf = bytearray()
    while time.time() < deadline and not t510_stop:
        data = _t510_take_bytes()
        if data:
            # Log raw bytes for debug, mask for match
            log_entry('rx', f'T510 RX  {data.hex(" ")}')
            for b in data:
                buf.append(b & 0x7F)
            if len(buf) >= len(pattern_ascii):
                # Search for pattern anywhere in recent tail
                idx = buf.rfind(pattern_ascii)
                if idx >= 0:
                    return True
                # Keep only the last (len(pattern)-1) bytes so we can still
                # match a pattern straddling the next chunk
                if len(buf) > len(pattern_ascii):
                    del buf[:-len(pattern_ascii)]
        else:
            time.sleep(0.02)
    if not t510_stop:
        log_entry('warn', f'T510 timeout waiting for {label}')
    return False

def _t510_set_baud(baud: int):
    """Hot-reconfigure the open serial port to a new baud rate. Avoids a
    close/reopen (which can drop pending bytes on some adapters)."""
    with state_lock:
        p = serial_port
    if p and p.is_open:
        try:
            p.baudrate = baud
            log_entry('info', f'T510 port → {baud} baud')
        except Exception as e:
            log_entry('err', f'T510 baud change to {baud}: {e}')

def _t510_serial_write(data: bytes, label: str = ''):
    with state_lock:
        p = serial_port
    if p and p.is_open:
        try:
            p.write(data)
            extra = f' ({label})' if label else ''
            log_entry('tx', f'T510 TX {len(data)}B{extra}  {data[:48].hex(" ")}')
        except Exception as e:
            log_entry('err', f'T510 write: {e}')

def _t510_obis_now() -> str:
    t = time.localtime()
    return f'{t.tm_year%100:02d}{t.tm_mon:02d}{t.tm_mday:02d}{t.tm_hour:02d}{t.tm_min:02d}{t.tm_sec:02d}'

t510_body_override = None  # set via /api/t510_body when running a repo
                            # descriptor; supersedes the built-in SDM-style
                            # OBIS body.

def _t510_build_ascii(serial_num: str) -> str:
    """Build a minimal T510 OBIS ASCII telegram from reg_bank. The browser
    pushes fresh register values every second via /api/regs, so reg_bank is
    the single source of truth for live power/energy data.

    When running under repo mode, the browser POSTs the fully-formed OBIS
    body to /api/t510_body and we just emit that — same wakeup-handshake +
    9600-baud-data flow, but with an arbitrary descriptor's value lines."""
    if t510_body_override is not None:
        body = t510_body_override
        # Force the identification header to match `serial_num` so the ACK
        # phase still matches what we sent in phase 2a.
        if body.startswith('/'):
            return body
        return f'/T515{serial_num}\r\n\r\n' + body
    with state_lock:
        pL1 = float(reg_bank.get(0x000C, 0))  # W
        pL2 = float(reg_bank.get(0x000E, 0))
        pL3 = float(reg_bank.get(0x0010, 0))
        iL1 = float(reg_bank.get(0x0006, 0))  # A
        iL2 = float(reg_bank.get(0x0008, 0))
        iL3 = float(reg_bank.get(0x000A, 0))
        eIn = float(reg_bank.get(0x0048, 0))  # kWh
    lines = [
        f'/T515{serial_num}\r\n',
        '\r\n',
        f'0.0.0({serial_num})\r\n',
        f'0.9.1({_t510_obis_now()})\r\n',
        f'1.8.0({eIn:.3f})\r\n',
        f'21.7.0({int(round(pL1 * 1000))})\r\n',  # mW
        f'41.7.0({int(round(pL2 * 1000))})\r\n',
        f'61.7.0({int(round(pL3 * 1000))})\r\n',
        f'31.7.0({iL1:.2f})\r\n',
        f'51.7.0({iL2:.2f})\r\n',
        f'71.7.0({iL3:.2f})\r\n',
        '!\r\n',
    ]
    return ''.join(lines)

def _t510_runner(serial_num: str):
    """Full IEC 62056-21 mode-C loop: wait for /?!\\r\\n at 300 baud, send
    identification, wait for ACK, send OBIS data at 9600, go back to 300."""
    log_entry('info', f'T510 state machine started (serial={serial_num})')
    _t510_take_bytes()  # flush
    try:
        while not t510_stop:
            # Phase 1 — wait for /?!\r\n (mode-C wake-up)
            if not _t510_wait_for(b'/?!\r\n', 65.0, '/?!\\r\\n wake-up'):
                continue
            log_entry('info', 'T510 ✓ wake-up received')

            # Phase 2a — send identification at 300 baud
            ident = f'/T515{serial_num}\r\n'.encode('ascii')
            _t510_serial_write(ident, 'identification')

            # Phase 2b — wait for ACK \x06 050 \r\n
            if not _t510_wait_for(b'\x0605\x30\r\n', 10.0, '\\x06050\\r\\n ACK'):
                continue
            log_entry('info', 'T510 ✓ ACK received — switching to 9600 baud')

            # Phase 3 — switch to 9600, send OBIS ASCII telegram
            _t510_set_baud(9600)
            time.sleep(0.2)  # let Tasmota's updateBaudRate settle
            telegram = _t510_build_ascii(serial_num)
            _t510_serial_write(telegram.encode('ascii'), 'OBIS ASCII 9600bd')
            time.sleep(3.0)  # give Tasmota time to parse

            # Phase 4 — back to 300 baud for next cycle
            _t510_set_baud(300)
            _t510_take_bytes()  # flush any stale bytes from 9600 phase
    except Exception as e:
        log_entry('err', f'T510 runner exception: {e}')
    finally:
        log_entry('info', 'T510 state machine stopped')

def t510_start(serial_num: str):
    global t510_running, t510_thread, t510_stop, t510_serial
    if t510_running:
        return
    t510_serial  = serial_num or 'T510001'
    t510_stop    = False
    t510_running = True
    t510_thread  = threading.Thread(target=_t510_runner, args=(t510_serial,),
                                    daemon=True)
    t510_thread.start()

def t510_stop_runner():
    global t510_running, t510_stop
    if not t510_running:
        return
    t510_stop    = True
    t510_running = False
    # Thread exits at its next wait/poll boundary; daemon flag cleans up on exit.

# ── Kamstrup OMNIPOWER (Kx7) poll/response state machine ─────────────────────
#
# Real Kamstrup electric meters speak a proprietary request/response protocol.
# The Tasmota driver TX-es a poll like 3F100100010001... every `tsecs` seconds
# (descriptor's `,10,...` field). The meter answers with a frame containing the
# requested register values. We mirror that dialog here so the emulator looks
# like a real meter on the wire.
#
# Wire framing (both directions):
#   master → meter:   0x80 [stuffed payload] 0x0d
#   meter → master:   0x40 [stuffed payload] 0x0d
# Byte stuffing (any byte in {0x80, 0x40, 0x0d, 0x06, 0x1b}):
#                     b   →   0x1b  (b XOR 0xff)
# CRC: CRC-16, polynomial 0x1021, init 0, augmented mode (computed over
#      payload + 2 zero placeholders, then those placeholders are replaced
#      with the CRC big-endian).
#
# kstr value encoding (per register in payload after [reg_hi reg_lo]):
#   [unit] [mantLen] [exp_signs] [mant_BE × mantLen]
#   exp_signs bits:  7 = value sign,  6 = exp sign,  5..0 = |exp|
# IMPORTANT: We always emit exp=0 because Tasmota's kstr decoder
# (xsns_53_sml.ino:2431) silently drops negative exponents — its loop
# `for (uint16_t x = 1; x <= i; ++x)` runs 0 times when i<0, so ifl stays
# at 1 and the negative scaling is lost. By emitting integer mantissa with
# exp=0 and letting the descriptor's `@i0:DIVISOR` rescale, we sidestep
# the bug entirely.

# Register address → (browser-supplied reg_bank key, scale_int_factor)
# scale_int_factor pre-multiplies the float value to make an integer mantissa
# matching the descriptor's divisor. Voltage 230.4 V × 10 = 2304 (dV);
# descriptor `@i0:10` brings it back to 230.4 V.
KS_REGS = {
    0x0001: ('ks_energy_kWh', 1000),  # kWh × 1000 → mWh integer; descriptor /1000
    0x041E: ('ks_vL1',        10),    # V × 10     → dV integer;   descriptor /10
    0x0434: ('ks_iL1',        100),   # A × 100    → cA integer;   descriptor /100
    0x03FF: ('ks_powerW',     1),     # W           → W integer;    descriptor /1
}

# Dynamic register map populated by /api/ks_regs when running a repo
# descriptor. Keys are register addresses (int), values are pre-scaled
# integer mantissas (descriptor's @ divisor already applied browser-side).
# Supersedes KS_REGS lookups when a register is present here.
ks_dyn_regs = {}

def _ks_crc(data):
    """CRC-16 (poly 0x1021, init 0, no XOR-out, augmented). Matches the C
    KS_calculateCRC at xsns_53_sml.ino:4985 byte-for-byte."""
    crc = 0
    for b in data:
        mask = 0x80
        while mask:
            crc <<= 1
            if b & mask:
                crc |= 1
            mask >>= 1
            if crc & 0x10000:
                crc &= 0xFFFF
                crc ^= 0x1021
        crc &= 0xFFFFFFFF
    return crc & 0xFFFF

def _ks_unstuff(stuffed):
    """Undo byte stuffing: 0x1b XX → (XX XOR 0xff)."""
    out = bytearray()
    i = 0
    while i < len(stuffed):
        if stuffed[i] == 0x1b and i + 1 < len(stuffed):
            out.append(stuffed[i + 1] ^ 0xFF)
            i += 2
        else:
            out.append(stuffed[i])
            i += 1
    return bytes(out)

def _ks_stuff(raw):
    out = bytearray()
    for b in raw:
        if b in (0x80, 0x40, 0x0d, 0x06, 0x1b):
            out.append(0x1b)
            out.append(b ^ 0xFF)
        else:
            out.append(b)
    return bytes(out)

def _ks_kstr_encode_int(int_mant, mant_len=4):
    """Encode integer mantissa with exp=0 (works around C decoder neg-exp bug).
    Returns [unit, len, exp_byte, mant_BE...]."""
    neg = int_mant < 0
    m   = abs(int(round(int_mant)))
    cap = (1 << (mant_len * 8)) - 1
    if m > cap:
        log_entry('warn', f'KS: mantissa {int_mant} exceeds {cap} for {mant_len}B — clamping')
        m = cap
    exp_byte = 0x80 if neg else 0x00
    out = [0x00, mant_len, exp_byte]
    for i in range(mant_len - 1, -1, -1):
        out.append((m >> (i * 8)) & 0xFF)
    return out

def _ks_get_value(reg_addr):
    """Look up a register's integer mantissa. Repo-mode descriptors push
    pre-scaled integer mantissas to ks_dyn_regs (via /api/ks_regs), which
    take precedence — that lets arbitrary descriptors work without a
    server-side hardcoded register map. Falls back to the named-key
    KS_REGS map (built-in profile) if the descriptor is unknown.
    Returns 0 if neither map covers the register."""
    with state_lock:
        if reg_addr in ks_dyn_regs:
            return int(ks_dyn_regs[reg_addr])
    spec = KS_REGS.get(reg_addr)
    if not spec:
        return 0
    key, scale = spec
    with state_lock:
        v = float(reg_bank.get(key, 0.0))
    return int(round(v * scale))

def _ks_build_response(reg_addrs):
    """Build a meter→master response for the given list of register addresses.
    The wire format is [0x40] [stuffed payload] [0x0d] where payload =
    [3F 10] [reg_hi reg_lo + kstr] × N + [crc_hi crc_lo]."""
    payload = bytearray([0x3F, 0x10])
    for addr in reg_addrs:
        payload.append((addr >> 8) & 0xFF)
        payload.append(addr & 0xFF)
        payload.extend(_ks_kstr_encode_int(_ks_get_value(addr), 4))
    crc = _ks_crc(bytes(payload) + b'\x00\x00')
    payload.append((crc >> 8) & 0xFF)
    payload.append(crc & 0xFF)
    return bytes([0x40]) + _ks_stuff(bytes(payload)) + bytes([0x0d])

def _ks_decode_poll(stuffed_body):
    """Decode a master→meter poll body (already stripped of 0x80/0x0d framing).
    Returns list of requested register addresses, or None on CRC failure.

    Poll payload structure:  3F 10 [count] [reg_hi reg_lo] × count [crc_hi crc_lo]
    """
    body = _ks_unstuff(stuffed_body)
    if len(body) < 6 or body[0] != 0x3F or body[1] != 0x10:
        return None
    if _ks_crc(body) != 0:
        return None
    count = body[2]
    if len(body) < 3 + count * 2 + 2:
        return None
    addrs = []
    for i in range(count):
        hi = body[3 + i * 2]
        lo = body[4 + i * 2]
        addrs.append((hi << 8) | lo)
    return addrs

def _ks_runner():
    """Watch rx_buf for 0x80...0x0d poll frames, decode, respond."""
    global rx_buf
    log_entry('info', 'Kamstrup state machine started (push reg values via /api/regs)')
    accum = bytearray()       # bytes between current 0x80 and 0x0d
    in_frame = False
    try:
        while not ks_stop:
            with state_lock:
                chunk = bytes(rx_buf)
                rx_buf.clear()
            if not chunk:
                time.sleep(0.02)
                continue
            for b in chunk:
                if b == 0x80:           # frame start (master → meter)
                    in_frame = True
                    accum.clear()
                elif b == 0x0d and in_frame:
                    body = bytes(accum)
                    accum.clear()
                    in_frame = False
                    log_entry('rx', f'KS poll  {len(body)+2}B  80 {body.hex(" ")} 0d')
                    addrs = _ks_decode_poll(body)
                    if addrs is None:
                        log_entry('warn', 'KS poll: CRC fail or malformed — ignoring')
                        continue
                    log_entry('info', f'KS poll: {len(addrs)} reg(s) requested: '
                              + ', '.join(f'0x{a:04X}' for a in addrs))
                    resp = _ks_build_response(addrs)
                    with state_lock:
                        p = serial_port
                    if p and p.is_open:
                        try:
                            p.write(resp)
                            log_entry('tx', f'KS resp  {len(resp)}B  {resp.hex(" ")}')
                        except Exception as e:
                            log_entry('err', f'KS write: {e}')
                elif in_frame:
                    accum.append(b)
                # else: byte outside any frame (could be device's own echo
                # or noise) — silently dropped
    except Exception as e:
        log_entry('err', f'KS runner exception: {e}')
    finally:
        log_entry('info', 'Kamstrup state machine stopped')

def kamstrup_start():
    global ks_running, ks_thread, ks_stop
    if ks_running:
        return
    ks_stop    = False
    ks_running = True
    ks_thread  = threading.Thread(target=_ks_runner, daemon=True)
    ks_thread.start()

def kamstrup_stop_runner():
    global ks_running, ks_stop
    if not ks_running:
        return
    ks_stop    = True
    ks_running = False

# ── CAN-bus profile (network SLCAN bridge) ──────────────────────────────────────
def _can_runner(bridge: str, bitrate: int):
    """Hold one SlcanClient (TCP) to the bridge; on each matching DUT
    poll frame, transmit the current prebuilt response frame set.

    The browser owns descriptor parsing + value encoding (its existing
    .tas UU/uu/@scale codec) and refreshes can_frames via /api/can_cfg.
    """
    global can_running
    try:
        # slcan_client.py sits next to this server file; make the import
        # path-independent of how the process was launched (repo dir,
        # packaged .app bundle, or an external headless launcher).
        _here = os.path.dirname(os.path.abspath(__file__))
        if _here not in sys.path:
            sys.path.insert(0, _here)
        from slcan_client import SlcanClient
    except Exception as e:
        log_entry('err', f'CAN: slcan_client import failed: {e}')
        can_running = False
        return
    log_entry('info', f'CAN profile started — SLCAN bridge {bridge} @ {bitrate} kbit/s')
    # Resilience: the SLCAN bridge has no bus-off auto-recovery (it goes
    # silent after CAN TX errors and stays that way until the channel is
    # re-opened — see SLCAN_README "send C\rO\r to re-open"), and a fresh
    # bridge (post Restart 1 / TinyCRun) can miss the first S/O before
    # its command loop is servicing the socket. So: keep an outer
    # reconnect loop. Open with retries; if the link goes silent for
    # IDLE_LIMIT s while the DUT should be polling (~1 Hz), assume
    # bus-off/stale and re-open (the bridge's O reinstalls TWAI). The
    # emulator self-heals across bridge restarts — the user presses
    # Connect once and leaves it.
    IDLE_LIMIT  = 8.0          # s of total silence → reconnect
    backoff     = 1.0
    while not can_stop:
        cli = None
        try:
            cli = SlcanClient(bridge, bitrate=bitrate)
            cli.open()                                  # S<n> + O, ACKed
            log_entry('info', 'CAN: SLCAN channel open (S/O ACK)')
            backoff = 1.0                               # reset on success
            last_rx = time.time()
            poll_n  = 0
            while not can_stop:
                f = cli.recv(timeout=0.5)
                if f is None:
                    if time.time() - last_rx > IDLE_LIMIT:
                        log_entry('warn', 'CAN: link silent '
                                  f'{IDLE_LIMIT:.0f}s (bridge bus-off?) '
                                  '— re-opening channel')
                        break                           # → reconnect
                    continue
                last_rx = time.time()
                with state_lock:
                    cfg    = can_cfg
                    frames = list(can_frames)
                poll = cfg.get('poll') if cfg else None
                if poll and f.ext == bool(poll.get('ext')) \
                        and f.id == int(poll.get('id')):
                    poll_n += 1
                    acks = 0
                    for fr in frames:
                        data = bytes.fromhex(fr['data'])
                        if cli.send(int(fr['id']), bool(fr['ext']),
                                    data, timeout=0.3):
                            acks += 1
                    # One compact line per poll cycle (10 frames → 1 log
                    # line, not 10 — keeps the console readable).
                    if not frames:
                        log_entry('warn', 'CAN poll #%d: no response '
                                  'frames yet (set values in the form)'
                                  % poll_n)
                    elif poll_n <= 3 or poll_n % 10 == 0:
                        log_entry('tx', f'CAN poll #{poll_n} → '
                                  f'{acks}/{len(frames)} frames ACKed')
                    elif acks != len(frames):
                        log_entry('warn', f'CAN poll #{poll_n}: only '
                                  f'{acks}/{len(frames)} ACKed')
                # else: non-poll bus frame — ignored silently
        except Exception as e:
            log_entry('warn', f'CAN: channel error ({e}) — retrying')
        finally:
            if cli:
                try:
                    cli.close()
                except Exception:
                    pass
        if can_stop:
            break
        time.sleep(backoff)                             # reconnect backoff
        backoff = min(backoff * 1.5, 5.0)
    can_running = False
    log_entry('info', 'CAN profile stopped')

def can_start(bridge: str, bitrate: int):
    global can_running, can_thread, can_stop
    if can_running:
        return
    can_stop    = False
    can_running = True
    can_thread  = threading.Thread(target=_can_runner,
                                   args=(bridge, bitrate), daemon=True)
    can_thread.start()

def can_stop_runner():
    global can_running, can_stop
    if not can_running:
        return
    can_stop    = True
    can_running = False

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

  // CAN-bus profile: a network SLCAN bridge (slcan_bridge_tcp.tc on an
  // ESP32) replaces the serial port. A labelled host:port field —
  // remembered across sessions — shown only when the parsed descriptor
  // is a CAN ('+…,C,…') one (swaps in place of the serial-port select).
  const CAN_BRIDGE_KEY = 'sml_can_bridge';
  let savedBridge;
  try { savedBridge = localStorage.getItem(CAN_BRIDGE_KEY); } catch (e) {}
  const canWrap = document.createElement('span');
  canWrap.id = 'bridgeCanWrap';
  canWrap.style.cssText = 'display:none;align-items:center;gap:6px';
  const canLbl = document.createElement('label');
  canLbl.textContent = 'CAN bridge:';
  canLbl.htmlFor = 'bridgeCanHost';
  canLbl.style.cssText = 'color:#9ec1ea;font-size:0.82em;white-space:nowrap';
  const canInp = document.createElement('input');
  canInp.id = 'bridgeCanHost';
  canInp.type = 'text';
  canInp.value = savedBridge || '192.168.188.143:9999';
  canInp.placeholder = 'host:port';
  canInp.title = 'SLCAN bridge host:port (slcan_bridge_tcp.tc on the ESP32). Remembered across sessions.';
  canInp.style.cssText = 'width:170px;background:#0a1628;border:1px solid #1a4a7a;color:#e0e0e0;padding:5px 8px;border-radius:4px;font-size:0.81em';
  canInp.addEventListener('input', () => {
    try { localStorage.setItem(CAN_BRIDGE_KEY, canInp.value.trim()); } catch (e) {}
  });
  canWrap.appendChild(canLbl);
  canWrap.appendChild(canInp);
  btnConn.parentNode.insertBefore(canWrap, btnConn);
  btnConn.parentNode.insertBefore(document.createTextNode(' '), btnConn);
  // Surface the bridge field where the user actually looks: as the
  // first row of the parsed-descriptor value panel (above the metric
  // inputs), not buried in the connect toolbar. Relocate the single
  // canWrap node there for CAN descriptors (the HTML rebuilds
  // repoValuesFormInner per parse but not repoValuesForm itself, so a
  // node inserted before repoValuesFormInner survives; the poller
  // re-asserts it anyway). Hide the serial-port select in CAN mode.
  setInterval(() => {
    const isCan = (typeof isRepoMode === 'function' && isRepoMode()
                   && typeof repoState !== 'undefined' && repoState
                   && repoState.proto === 'c');
    sel.style.display = isCan ? 'none' : '';
    const form = document.getElementById('repoValuesForm');
    if (isCan && form) {
      if (canWrap.parentNode !== form) {
        canWrap.style.cssText =
          'display:flex;align-items:center;gap:8px;margin:0 0 10px 0;'
        + 'padding:7px 9px;background:#0a2030;border:1px solid #1a6a9a;'
        + 'border-radius:5px';
        canLbl.style.cssText =
          'color:#7ec8ff;font-size:0.85em;font-weight:600;white-space:nowrap';
        canInp.style.width = '180px';
        form.insertBefore(canWrap, form.firstChild);
      }
      canWrap.style.display = 'flex';
    } else {
      canWrap.style.display = 'none';
    }
  }, 700);

  // Re-enable even if Web Serial check disabled it (e.g. Safari)
  btnConn.disabled = false;

  // Override Connect button
  btnConn.onclick = async () => {
    // ── CAN-bus profile: network SLCAN bridge, no serial port ──
    if (typeof isRepoMode === 'function' && isRepoMode()
        && typeof repoState !== 'undefined' && repoState
        && repoState.proto === 'c') {
      const bridge  = (canInp.value || '').trim();
      if (!bridge) { alert('Enter the SLCAN bridge host:port'); return; }
      const bitrate = repoState.canBitrate || 125;
      const r = await fetch(BASE + '/api/open', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({profile: 'repo_can', bridge, bitrate})
      });
      const j = await r.json().catch(() => ({}));
      if (j && j.ok) {
        window._bridgeWriter = { write: async () => {} };  // CAN has no serial TX
        writer = window._bridgeWriter;
        setConn(true);
        pushCanCfg();                       // send poll matcher + initial frames
        log('info', `CAN bridge ${bridge} @ ${bitrate} kbit/s — emulating `
                    + (repoState.name || 'CAN device'));
      } else {
        log('err', `CAN open failed: ${(j && j.error) || 'unknown'}`);
      }
      return;
    }
    const device = sel.value;
    if (!device) { alert('Select a serial port first'); return; }
    // In repo mode the per-meter baud comes from the parsed descriptor,
    // not the dropdown — match what the non-bridge connect path does so
    // the python serial port opens at the descriptor's baud (T510 must
    // start at 300, M-Bus at 2400, etc.).
    let baud = +document.getElementById('selBaud').value;
    let serCfg = document.getElementById('selSerial').value;
    if (typeof isRepoMode === 'function' && isRepoMode() && window.repoState) {
      if (window.repoState.baud) baud = window.repoState.baud;
      if (window.repoState.serialFmtDefault) serCfg = window.repoState.serialFmtDefault;
    }
    // Tell Python about the profile so it can run the T510 IEC 62056-21
    // or Kamstrup state machine server-side (the browser's port.readable
    // is not available under the bridge, so the HTML state machines never
    // fire). Repo mode picks the pseudo-profile based on the descriptor.
    let profile = document.getElementById('selProfile')?.value || '';
    if (typeof isRepoMode === 'function' && isRepoMode() && window.repoState) {
      if (window.repoState.proto === 'k') profile = 'repo_kamstrup';
      else if (window.repoState.proto === 'o' && window.repoState.baud === 300) profile = 'repo_t510';
      else profile = '';  // SML / OBIS / VBus / EBus / shift / Modbus all run
                          // off the existing serial path; no pseudo-profile.
    }
    const serialNum = document.getElementById('inpSerial')?.value || '';
    const r = await fetch(BASE + '/api/open', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({device, baud, config: serCfg, profile, serial: serialNum})
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
  // Kamstrup also auto-runs server-side — keep Start disabled there.
  if (writer && !isModbusRtu() && !isT510() && !isKamstrup()) {
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
  const iL1 = vL1Base > 0 ? pL1 / vL1Base : 0;
  const regs = {
    0x0000: vL1Base,
    0x0002: vL2Base,
    0x0004: vL3Base,
    0x0006: iL1,
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
    // Kamstrup state machine (server-side _ks_runner) reads these named
    // keys when assembling poll responses. Storing alongside Modbus regs
    // (which use numeric keys) — Python /api/regs handler routes by key
    // type. Values are float; the server converts to integer mantissa using
    // KS_REGS scale factors.
    'ks_energy_kWh': live.energyIn,
    'ks_vL1':        vL1Base,
    'ks_iL1':        iL1,
    'ks_powerW':     pIn - pOut,
    _fluct_pct: pct
  };
  fetch(BASE + '/api/regs', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify(regs)
  }).catch(() => {});
};

// ── For TCP / Modbus-RTU / T510 profiles: push registers every second ─────────
// None of these runs the browser-side sendFrame loop that would normally call
// computeLiveValues — for T510 the Python-side state machine reads reg_bank to
// build OBIS telegrams, so we must keep pushing base values for it to see.
//
// The Auto-push dropdown (selAutoPush) gates this — switch Off when running a
// custom descriptor that shares register addresses with SDM630 (e.g. polling
// 0x001E for something other than frequency). Otherwise SDM630's freq=50 keeps
// landing in your reg_bank every second and shows up as bleed-through in the
// Tasmota JSON.
setInterval(() => {
  const autoPush = document.getElementById('selAutoPush');
  if (autoPush && autoPush.value === '0') return;
  if (isTcpProfile()
      || (typeof isModbusRtu  === 'function' && isModbusRtu())
      || (typeof isT510       === 'function' && isT510())
      || (typeof isKamstrup   === 'function' && isKamstrup())) {
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

// ── CAN profile: encode repo items → SLCAN response frames ───────────────────
// The descriptor data line is [id:4B][dlc:1B][data…] where part of the
// data is literal selector bytes and a run of U/u/S/s/f/F placeholders
// marks the value field. We mirror Tasmota's UU/uu endianness convention
// (uppercase-leading = big-endian, lowercase = byte-swapped/LE). raw =
// round(displayed_value × @scale) — the firmware reports raw / scale.
function _canScanFormat(rest) {
  const m8 = rest.match(/^(ffffffff|FFFFFFFF|UUuuUUuu|SSssSSss|uuUUuuUU|ssSSssSS)/);
  if (m8) { const t = m8[1];
    return { width: 4, letters: 8,
      fmt: /^[fF]+$/.test(t) ? 'f32be'
         : t === 'UUuuUUuu' ? 'u32be' : t === 'SSssSSss' ? 's32be'
         : t === 'uuUUuuUU' ? 'u32le' : 's32le' }; }
  const m4 = rest.match(/^(UUuu|SSss|uuUU|ssSS)/);
  if (m4) { const t = m4[1];
    return { width: 2, letters: 4,
      fmt: t === 'UUuu' ? 'u16be' : t === 'SSss' ? 's16be'
         : t === 'uuUU' ? 'u16le' : 's16le' }; }
  const m1 = rest.match(/^(UU|SS|uu|ss)/);
  if (m1) { const t = m1[1];
    return { width: 1, letters: 2,
      fmt: (t[0] === 'S' || t[0] === 's') ? 's8' : 'u8' }; }
  return null;
}
function _canEncodeValue(fmt, width, raw) {
  const b = new Uint8Array(width);
  if (fmt === 'f32be') { new DataView(b.buffer).setFloat32(0, raw, false); return b; }
  let v = Math.round(raw);
  const span = Math.pow(2, width * 8);
  if (fmt[0] === 's') {                       // signed: clamp then 2's-comp
    const lim = span / 2;
    if (v >  lim - 1) v = lim - 1;
    if (v < -lim)     v = -lim;
    if (v < 0) v += span;
  } else {
    if (v < 0) v = 0;
    if (v > span - 1) v = span - 1;
  }
  const le = fmt.endsWith('le');
  for (let i = 0; i < width; i++) {
    const sh = le ? i : (width - 1 - i);
    b[i] = Math.floor(v / Math.pow(2, 8 * sh)) & 0xff;
  }
  return b;
}
function buildCanCfg() {
  if (typeof repoState === 'undefined' || !repoState
      || repoState.proto !== 'c' || !repoState.canPoll)
    return null;
  const frames = [];
  for (const it of (repoState.items || [])) {
    const pat = it.obis || '';                       // [id8][dlc2][data…]
    if (pat.length < 12) continue;
    const id  = parseInt(pat.substr(0, 8), 16) >>> 0;
    const ext = id > 0x7ff;
    const dlc = parseInt(pat.substr(8, 2), 16) || 0;
    if (dlc < 1 || dlc > 8) continue;
    const data = new Uint8Array(dlc);
    let rest = pat.substr(10), off = 0, placed = false;
    while (rest.length > 0 && off < dlc) {
      const f = _canScanFormat(rest);
      if (f) {
        const raw = (+it.value || 0) * (it.scale || 1);
        const enc = _canEncodeValue(f.fmt, f.width, raw);
        for (let i = 0; i < f.width && off < dlc; i++) data[off++] = enc[i];
        rest = rest.substr(f.letters); placed = true; continue;
      }
      if (/^[0-9a-fA-F]{2}/.test(rest)) {              // literal selector byte
        data[off++] = parseInt(rest.substr(0, 2), 16);
        rest = rest.substr(2); continue;
      }
      break;
    }
    if (!placed) continue;                              // no value field
    let hex = '';
    for (const x of data) hex += x.toString(16).padStart(2, '0');
    frames.push({ id, ext, data: hex });
  }
  return { poll: { id: repoState.canPoll.id, ext: repoState.canPoll.ext },
           frames };
}
async function pushCanCfg() {
  const cfg = buildCanCfg();
  if (!cfg) return;
  try {
    await fetch(BASE + '/api/can_cfg', {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify(cfg)
    });
  } catch (_) {}
}
setInterval(() => {
  if (writer && typeof repoState !== 'undefined' && repoState
      && repoState.proto === 'c') pushCanCfg();
}, 1000);

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
    """Re-open the browser ONLY if it was never successfully loaded in the first
    place (e.g. the initial `open` call failed silently). A previously-loaded
    page that has gone silent is almost certainly a backgrounded/throttled tab
    — modern browsers throttle setInterval to ≥1 s when unfocused and ≥60 s
    after 5 min idle, so the 1 Hz pollLog/pollWriteRegs heartbeats die on their
    own. Forcibly re-opening the URL would yank the live tab away, reload the
    page, and kill any open Web Serial port (T510 IEC 62056-21 listener, etc.).
    So: only retry while we haven't yet seen a single request."""
    global last_http_req, browser_opened
    deadline = time.time() + 30.0
    while time.time() < deadline:
        time.sleep(2)
        if last_http_req > 0:
            return                      # page loaded — stop, never reopen
        if browser_opened:
            open_browser_reliable(url)  # retry until first request arrives

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
        # ── Local meter descriptors (PDF-extracted, supplements upstream
        #    ottelo9/tasmota-sml-script). Two endpoints:
        #      * `/local_meters.json` — index of bundled descriptors
        #        (see _serve_local_meter_index for the three outcomes:
        #        curated JSON, auto-discovered tree, or empty index).
        #      * `/local_meters/<vendor>/<file>.tas` — raw .tas content.
        #    As of the upstream PR merge, all of our previous additions
        #    are now in ottelo9, so the folder ships empty and these
        #    endpoints behave as no-ops. Drop a new `.tas` file into
        #    `local_meters/<vendor>/` to bring local-only descriptors
        #    back without re-plumbing — auto-discovery picks them up.
        # ───────────────────────────────────────────────────────────────
        elif path == '/local_meters.json':
            self._serve_local_meter_index()
        elif path.startswith('/local_meters/'):
            self._serve_local_meter_file(path[len('/local_meters/'):])
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
            prof = body.get('profile') or ''
            # CAN-bus repo profile runs over a network SLCAN bridge, not
            # a local serial device — skip _open_serial entirely and own
            # the HTTP response here (the serial path's _json is bypassed).
            if prof in ('can', 'repo_can'):
                bridge  = (body.get('bridge') or '192.168.188.143:9999').strip()
                try:
                    bitrate = int(body.get('bitrate') or 125)
                except (ValueError, TypeError):
                    bitrate = 125
                can_start(bridge, bitrate)
                self._json({'ok': True, 'mode': 'can',
                            'bridge': bridge, 'bitrate': bitrate})
                return
            self._open_serial(body.get('device',''), body.get('baud', 9600),
                              body.get('config', '8N1'))
            # Launch protocol-specific state machines that need to run
            # server-side because under the bridge the browser never owns
            # the port (see BRIDGE_SCRIPT btnConnect override). Repo mode
            # passes pseudo-profiles `repo_t510` / `repo_kamstrup` to
            # request these machines without a built-in profile match.
            if prof in ('t510', 'repo_t510'):
                t510_start(body.get('serial') or 'T510001')
            elif prof in ('kamstrup_kx7', 'repo_kamstrup'):
                kamstrup_start()
        elif path == '/api/close':
            t510_stop_runner()
            kamstrup_stop_runner()
            can_stop_runner()
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
                    fv = float(v) if v is not None else 0.0
                    # Modbus uses int register addresses; Kamstrup (and any
                    # future named-register bank) uses string keys. Accept
                    # both — int-coerce numeric strings, fall back to string
                    # storage for non-numeric keys.
                    try:
                        reg_bank[int(k)] = fv
                    except (ValueError, TypeError):
                        reg_bank[k] = fv
            self._json({'ok': True})
        elif path == '/api/coils':
            # Browser pushes a {addr_str: 0|1} dict. Replaces existing entries
            # at those addresses; doesn't clear addresses not in the body.
            # FC05/FC15 master writes go to the same coil_bank, so the
            # browser-side UI sees them on its next /api/writeregs-style poll
            # (poll endpoint TODO when we wire UI).
            body = self._read_json()
            with state_lock:
                for k, v in body.items():
                    try:
                        coil_bank[int(k)] = 1 if v else 0
                    except (ValueError, TypeError):
                        pass
            self._json({'ok': True})
        elif path == '/api/dinputs':
            # Discrete inputs are read-only by Modbus spec — only the
            # emulator (= the simulated device) can change them. Browser
            # POSTs the current "physical state" (charging-state flags,
            # day/night, alarm bits) so FC02 reads return realistic values.
            body = self._read_json()
            with state_lock:
                for k, v in body.items():
                    try:
                        discrete_bank[int(k)] = 1 if v else 0
                    except (ValueError, TypeError):
                        pass
            self._json({'ok': True})
        elif path == '/api/reg_format':
            # Register encoding override for FC03/FC04. Body is
            # {addr_int_or_str: 'u16'|'s16'|'f32'|null}. null/missing entries
            # delete the override (back to default float32). Used by EPEVER
            # and other scaled-int Modbus devices.
            body = self._read_json()
            with state_lock:
                for k, v in body.items():
                    try:
                        addr = int(k)
                    except (ValueError, TypeError):
                        continue
                    if v in ('u16', 's16'):
                        reg_format[addr] = v
                    else:
                        reg_format.pop(addr, None)
            self._json({'ok': True})
        elif path == '/api/ks_regs':
            # Repo-mode Kamstrup descriptor pushes its register list with
            # pre-scaled integer mantissas. Body shape:
            #   {"<reg_addr_decimal_or_hex>": <int_mantissa>, ...}
            # null/missing values clear the override (back to KS_REGS).
            body = self._read_json()
            with state_lock:
                for k, v in body.items():
                    try:
                        addr = int(k, 0)  # accept "1", "0x041e", etc.
                    except (ValueError, TypeError):
                        continue
                    if v is None:
                        ks_dyn_regs.pop(addr, None)
                    else:
                        try:
                            ks_dyn_regs[addr] = int(v)
                        except (ValueError, TypeError):
                            pass
            self._json({'ok': True})
        elif path == '/api/can_cfg':
            # Repo-mode CAN descriptor pushes (a) the DUT poll matcher and
            # (b) the current prebuilt response frames. The browser owns
            # .tas parsing + UU/uu/@scale value encoding; we just store
            # the latest set for _can_runner to blast on each poll.
            # Body shape:
            #   {"poll":   {"id": <int>, "ext": <bool>},
            #    "frames": [{"id": <int>, "ext": <bool>, "data": "<hex>"}, ...]}
            # Re-POSTed every ~1 s with refreshed values (like /api/regs).
            global can_cfg, can_frames
            body = self._read_json()
            poll = body.get('poll') or None
            cleaned = []
            for fr in (body.get('frames') or []):
                try:
                    fid = int(fr['id'])
                    ext = bool(fr.get('ext', True))
                    hx  = str(fr['data']).strip()
                    b   = bytes.fromhex(hx)          # validate hex
                except (KeyError, ValueError, TypeError):
                    continue
                if len(b) > 8:                       # CAN max payload
                    continue
                cleaned.append({'id': fid, 'ext': ext, 'data': b.hex()})
            with state_lock:
                if poll is not None:
                    can_cfg = {'poll': {'id': int(poll.get('id', 0)),
                                        'ext': bool(poll.get('ext', True))}}
                can_frames = cleaned
            self._json({'ok': True, 'frames': len(cleaned)})
        elif path == '/api/t510_body':
            # Repo-mode T510 descriptor: browser pre-builds the OBIS-ASCII
            # body via buildOBISAsciiRepo() and POSTs it here as raw text.
            # An empty body restores the built-in SDM-style telegram.
            global t510_body_override
            length = int(self.headers.get('Content-Length', '0') or 0)
            raw = self.rfile.read(length).decode('utf-8', errors='replace') if length > 0 else ''
            with state_lock:
                t510_body_override = raw if raw else None
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

    def _serve_local_meter_index(self):
        """Return the index of locally-bundled meter descriptors. Three
        possible outcomes, in priority order:

        1. `local_meters/smartmeter.json` exists → serve it verbatim
           (curated index with friendly labels, matches the upstream
           ottelo9 format).
        2. `local_meters/` exists and contains *.tas files but no
           index.json → auto-discover the tree and synthesize the
           index on the fly. Useful during development: drop a new
           `.tas` file in a vendor folder and it appears in the
           dropdown immediately, without needing to edit JSON.
        3. Folder absent or empty → return an empty index (with a
           single "(none)" entry) so the browser falls back to
           upstream-only without an error.

        As of the upstream PR merge, all our previously-PDF-only
        descriptors landed in ottelo9/tasmota-sml-script, so the
        bundled folder ships empty by default. The endpoint stays
        wired up so new PDF-extracted descriptors can be reintroduced
        — just drop them in (1) or (2) — without touching the route
        table or the loader."""
        base_dir = os.path.join(os.path.dirname(HTML_FILE), 'local_meters')
        idx_path = os.path.join(base_dir, 'smartmeter.json')

        # Outcome 1: curated index file present.
        if os.path.isfile(idx_path):
            try:
                with open(idx_path, 'rb') as f:
                    data = f.read()
                self.send_response(200)
                self._cors()
                self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            except OSError:
                pass  # fall through to auto-discovery

        # Outcome 2: auto-discover *.tas files in vendor subdirectories.
        # Label = filename without the .tas extension (good enough for
        # the dropdown; user can rename the file for a cleaner label).
        if os.path.isdir(base_dir):
            entries = [{'label': '(none)', 'filename': ''}]
            try:
                for vendor in sorted(os.listdir(base_dir)):
                    vdir = os.path.join(base_dir, vendor)
                    if not os.path.isdir(vdir):
                        continue
                    for fname in sorted(os.listdir(vdir)):
                        if not fname.endswith('.tas'):
                            continue
                        rel   = f'{vendor}/{fname}'
                        label = fname[:-4]
                        entries.append({'label': label, 'filename': rel})
                if len(entries) > 1:
                    self._json({'smartmeter': entries})
                    return
            except OSError:
                pass  # fall through to empty index

        # Outcome 3: no folder, empty folder, or only non-.tas files.
        self._json({'smartmeter': [{'label': '(none)', 'filename': ''}]})

    def _serve_local_meter_file(self, rel_path):
        """Serve a .tas file from local_meters/. rel_path is whatever
        followed /local_meters/ in the request URL (already URL-decoded
        upstream by urlparse)."""
        from urllib.parse import unquote
        rel = unquote(rel_path)
        # Defence-in-depth: forbid path traversal. The static files all live
        # within local_meters/ — anything trying to climb out is malicious.
        if '..' in rel.split('/') or rel.startswith('/'):
            self.send_response(403)
            self.end_headers()
            return
        full = os.path.join(os.path.dirname(HTML_FILE), 'local_meters', rel)
        try:
            with open(full, 'rb') as f:
                data = f.read()
            self.send_response(200)
            self._cors()
            self.send_header('Content-Type', 'text/plain; charset=utf-8')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()

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
