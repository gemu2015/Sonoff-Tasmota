#!/usr/bin/env python3
"""slcan_client.py — minimal SLCAN client for the ESP32-C3 CAN bridge.

Implements the subset of the Lawicel-CAN232 / SLCAN ASCII protocol the
TinyC bridge (`examples/slcan_bridge.tc`) speaks:

  - S<n>     set bitrate (S0..S8 → 10/20/50/100/125/250/500/800/1000 kbit/s,
             with S0/S1 mapped to 25k/50k by the bridge since TWAI lacks
             10k/20k presets)
  - O        open the channel
  - C        close
  - t<…>     transmit standard 11-bit frame
  - T<…>     transmit extended 29-bit frame
  - V        version query
  - F        status flags query
  Output frames received from the bus arrive as `t<…>\\r` / `T<…>\\r`
  lines on the serial input stream.

Why a hand-rolled client and not python-can?

  python-can has a slcan backend that works fine — `pip install
  python-can; bus = can.Bus('/dev/cu.usbmodem...', interface='slcan',
  bitrate=250000)`. For one-off use that is the right answer. We ship
  this module so the SML emulator can run with zero extra dependencies
  beyond `pyserial` (which it already needs for serial-protocol
  meters).

Usage:

  from slcan_client import SlcanClient
  client = SlcanClient('/dev/cu.usbmodem*', bitrate=250)
  client.open()
  try:
      client.send(0x18FF50E5, ext=True, data=bytes.fromhex('0102030405'))
      while True:
          frame = client.recv(timeout=1.0)
          if frame is None: continue           # timeout, no frame
          print(frame)
  finally:
      client.close()

Each `recv` returns a `CanFrame` namedtuple `(id, ext, data)` or `None`
on timeout. `send` returns `True` on bridge ACK (`z`/`Z` reply) within
a 200 ms window, `False` on NACK/timeout.
"""

from __future__ import annotations
import collections
import socket
import threading
import time
import re
import glob
from typing import Optional, Iterator

try:
    import serial      # pyserial — required for the serial transport.
except ImportError:
    serial = None      # TCP transport works without pyserial; only error
                        # if the user actually picks a serial port path.


CanFrame = collections.namedtuple('CanFrame', ['id', 'ext', 'data'])
"""One received CAN frame.

  id   : 11-bit (0..0x7FF) or 29-bit (0..0x1FFFFFFF) identifier as int.
  ext  : True for 29-bit extended IDs, False for 11-bit standard.
  data : bytes object of length 0..8.
"""


# SLCAN bitrate mapping. Index = command digit (S0..S8).
# 10k/20k aren't TWAI presets; the bridge maps them to 25k/50k (closest
# available). Listed here for protocol correctness only — clients that
# care should pass an exact `bitrate=` int (matching one of the entries
# in `BITRATE_TO_CMD` below).
SLCAN_BITRATES = [10, 20, 50, 100, 125, 250, 500, 800, 1000]

BITRATE_TO_CMD = {kbps: f"S{i}" for i, kbps in enumerate(SLCAN_BITRATES)}


def _resolve_port(port_pattern: str) -> str:
    """Glob-resolve a port pattern.

    Lets callers pass `/dev/cu.usbmodem*` and have it resolve to the
    actual node (`/dev/cu.usbmodem14101`, etc) without knowing the
    suffix in advance. If the pattern matches multiple ports, the
    first hit alphabetically is chosen with a warning.
    """
    if not any(c in port_pattern for c in '*?['):
        return port_pattern
    hits = sorted(glob.glob(port_pattern))
    if not hits:
        raise FileNotFoundError(f"no serial port matched {port_pattern!r}")
    if len(hits) > 1:
        print(f"slcan_client: pattern {port_pattern!r} matched "
              f"{len(hits)} ports; using {hits[0]!r}")
    return hits[0]


# ── Transport abstraction ────────────────────────────────────────
# The SLCAN protocol is identical over USB-serial and TCP — only the
# byte transport differs. _Transport hides the difference behind two
# methods: `read(max)` returns up to max bytes, `write(b)` sends bytes.
class _Transport:
    def read(self, max_bytes: int = 256) -> bytes: raise NotImplementedError
    def write(self, data: bytes) -> None: raise NotImplementedError
    def close(self) -> None: raise NotImplementedError


class _SerialTransport(_Transport):
    def __init__(self, port_pattern: str, baud: int):
        if serial is None:
            raise SystemExit(
                "slcan_client serial transport requires pyserial. "
                "Install with: pip install pyserial"
            )
        self._port_path = _resolve_port(port_pattern)
        self._ser = serial.Serial(self._port_path, baud, timeout=0.1)

    def read(self, max_bytes: int = 256) -> bytes:
        return self._ser.read(max_bytes)

    def write(self, data: bytes) -> None:
        self._ser.write(data)
        self._ser.flush()

    def close(self) -> None:
        try:
            self._ser.close()
        except Exception:
            pass


class _TcpTransport(_Transport):
    def __init__(self, host: str, port: int):
        self._sock = socket.create_connection((host, port), timeout=5.0)
        self._sock.settimeout(0.1)
        # TCP_NODELAY — SLCAN lines are tiny + latency-sensitive.
        self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    def read(self, max_bytes: int = 256) -> bytes:
        try:
            data = self._sock.recv(max_bytes)
        except socket.timeout:
            return b''                       # alive, just no data this tick
        except OSError as e:
            raise ConnectionError(f"bridge socket error: {e}") from e
        if data == b'':
            # recv() returning 0 bytes WITHOUT a timeout == the peer
            # closed the connection (bridge restarted / TinyCRun). This
            # is NOT idle — surface it so the runner reconnects now.
            raise ConnectionError("bridge closed the connection (EOF)")
        return data

    def write(self, data: bytes) -> None:
        self._sock.sendall(data)

    def close(self) -> None:
        try:
            self._sock.shutdown(socket.SHUT_RDWR)
        except Exception:
            pass
        try:
            self._sock.close()
        except Exception:
            pass


def _make_transport(target: str, baud: int) -> _Transport:
    """Pick a transport based on the target string.

    `host:port` (with at least one digit-ish in the port slot, no `/`,
    no glob chars unless they're escaped) → TCP.
    Anything else → serial port (path or glob).

    Examples:
        '/dev/cu.usbmodem*'        → SerialTransport
        '/dev/ttyUSB0'             → SerialTransport
        '192.168.188.143:8888'     → TcpTransport
        'bridge.local:8888'        → TcpTransport
    """
    # Heuristic: if the string has no slash and contains a colon
    # followed by digits at the end, treat as host:port.
    if '/' not in target and ':' in target:
        host, _, port_str = target.rpartition(':')
        if port_str.isdigit() and host:
            return _TcpTransport(host, int(port_str))
    return _SerialTransport(target, baud)


class SlcanClient:
    """Thin SLCAN client. Threaded reader, callable send/recv.

    Transport is chosen automatically by the `target` string:
        '/dev/cu.usbmodem*'  → USB-serial
        '192.168.188.143:8888' → TCP
    """

    def __init__(self, target: str, bitrate: int = 250, baud: int = 115200):
        """
        target  : serial device path / glob ('/dev/cu.usbmodem*'), OR
                  'host:port' for TCP transport ('192.168.188.143:8888')
        bitrate : CAN bus rate in kbit/s — must be one of SLCAN_BITRATES
        baud    : USB-CDC framing baud (ignored for TCP)
        """
        if bitrate not in BITRATE_TO_CMD:
            raise ValueError(
                f"bitrate {bitrate} not in {SLCAN_BITRATES}"
            )
        self._target = target
        self._bitrate = bitrate
        self._baud = baud
        self._tr: Optional[_Transport] = None
        self._rx_thread: Optional[threading.Thread] = None
        self._stop_evt = threading.Event()
        self._rx_queue: collections.deque[CanFrame] = collections.deque(maxlen=4096)
        self._rx_lock = threading.Lock()
        self._rx_cond = threading.Condition(self._rx_lock)
        # Pending send-ACK channel: each `send` call deposits a token
        # the reader thread fills with True (ack), False (nack), or
        # None (timeout). We use a 1-deep queue per outstanding send —
        # caller serialises sends via _send_lock so this stays simple.
        self._send_lock = threading.Lock()
        self._send_ack: Optional[bool] = None
        self._send_ack_evt = threading.Event()
        # Last status flags from F\r reply (single byte hex).
        self._status_flags: int = 0
        # Set True by the reader thread when the transport dies (peer
        # closed / socket error) rather than on an intentional close.
        # recv() surfaces it so the runner reconnects fast instead of
        # looping forever on a dead socket.
        self._link_dead = False

    # ── Lifecycle ────────────────────────────────────────────────

    def open(self) -> None:
        """Open the transport, send `S<n>\\r` then `O\\r`, start the
        reader thread. Raises on bridge NACK or no reply within 1 s."""
        self._tr = _make_transport(self._target, self._baud)
        self._stop_evt.clear()
        self._rx_thread = threading.Thread(
            target=self._reader, name='slcan-rx', daemon=True
        )
        self._rx_thread.start()

        # Set bitrate, then open. Each command waits for a CR ack.
        self._send_ack_evt.clear()
        self._send_ack = None
        self._raw_write(BITRATE_TO_CMD[self._bitrate].encode() + b'\r')
        if not self._wait_simple_ack(1.0):
            raise IOError(
                f"bridge did not ACK S{SLCAN_BITRATES.index(self._bitrate)}"
            )

        self._send_ack_evt.clear()
        self._send_ack = None
        self._raw_write(b'O\r')
        if not self._wait_simple_ack(1.0):
            raise IOError("bridge did not ACK O (channel open)")

    def close(self) -> None:
        """Send `C\\r`, stop the reader thread, close the transport."""
        if self._tr is None:
            return
        try:
            self._raw_write(b'C\r')
            time.sleep(0.05)  # let any in-flight RX drain
        except Exception:
            pass
        self._stop_evt.set()
        if self._rx_thread is not None:
            self._rx_thread.join(timeout=1.0)
        try:
            self._tr.close()
        finally:
            self._tr = None

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    # ── TX ────────────────────────────────────────────────────────

    def send(self, can_id: int, ext: bool, data: bytes,
             timeout: float = 0.2) -> bool:
        """Transmit one frame. Returns True on bridge ACK (`z`/`Z\\r`),
        False on NACK or timeout. Caller must `open()` first."""
        if self._tr is None:
            raise RuntimeError("not open")
        if len(data) > 8:
            raise ValueError("CAN payload max 8 bytes")
        # Build SLCAN line
        if ext:
            id_hex = f"{can_id & 0x1FFFFFFF:08x}"
            cmd = b'T'
        else:
            id_hex = f"{can_id & 0x7FF:03x}"
            cmd = b't'
        line = cmd + id_hex.encode() + f"{len(data)}".encode()
        line += data.hex().encode() + b'\r'

        # Serialise sends so a concurrent caller doesn't see another's ACK.
        with self._send_lock:
            self._send_ack = None
            self._send_ack_evt.clear()
            self._raw_write(line)
            if not self._send_ack_evt.wait(timeout):
                return False
            return bool(self._send_ack)

    # ── RX ────────────────────────────────────────────────────────

    def recv(self, timeout: float = 1.0) -> Optional[CanFrame]:
        """Pop the next received frame, blocking up to `timeout` s.
        Returns None on timeout."""
        with self._rx_cond:
            if self._rx_queue:
                return self._rx_queue.popleft()
            self._rx_cond.wait(timeout)
            if self._rx_queue:
                return self._rx_queue.popleft()
            if self._link_dead:
                # Transport died (bridge restarted / socket dropped).
                # Raise so the runner's reconnect loop fires instead of
                # spinning on a permanently-empty queue.
                raise ConnectionError("bridge link dead — reconnecting")
            return None

    def recv_iter(self) -> Iterator[CanFrame]:
        """Yield received frames forever. Stops cleanly when `close()`
        is called (the reader thread exits and the queue drains)."""
        while not self._stop_evt.is_set() or self._rx_queue:
            frame = self.recv(timeout=0.5)
            if frame is not None:
                yield frame

    # ── Internals ────────────────────────────────────────────────

    def _raw_write(self, b: bytes) -> None:
        assert self._tr is not None
        self._tr.write(b)

    def _wait_simple_ack(self, timeout: float) -> bool:
        """Wait for either CR (ack) or BEL (nack)."""
        if not self._send_ack_evt.wait(timeout):
            return False
        return bool(self._send_ack)

    # SLCAN line format match:
    #   t<3 hex id><1 hex dlc><dlc*2 hex payload>     (11-bit)
    #   T<8 hex id><1 hex dlc><dlc*2 hex payload>     (29-bit)
    _T_LINE_RE = re.compile(
        rb'^t([0-9A-Fa-f]{3})([0-9])([0-9A-Fa-f]*)$'
    )
    _BIG_T_LINE_RE = re.compile(
        rb'^T([0-9A-Fa-f]{8})([0-9])([0-9A-Fa-f]*)$'
    )

    def _reader(self) -> None:
        """Background thread: drain transport, parse SLCAN lines, push
        frames to the RX queue and ACK/NACK signals to the send-ack
        channel."""
        buf = bytearray()
        while not self._stop_evt.is_set():
            try:
                chunk = self._tr.read(256) if self._tr else b''
            except OSError:
                # Real transport death (EOF / socket error), not an
                # intentional close. Flag it for recv() unless we were
                # asked to stop.
                if not self._stop_evt.is_set():
                    self._link_dead = True
                    with self._rx_cond:
                        self._rx_cond.notify_all()
                break
            if not chunk:
                continue
            for byte in chunk:
                if byte == 0x0D:           # CR — end of line
                    self._consume_line(bytes(buf))
                    buf.clear()
                elif byte == 0x07:         # BEL — NACK
                    self._send_ack = False
                    self._send_ack_evt.set()
                    buf.clear()
                elif byte == 0x0A:         # LF — ignore
                    pass
                else:
                    if len(buf) < 256:
                        buf.append(byte)
                    else:
                        # Overrun — drop the line we were building.
                        buf.clear()

    def _consume_line(self, line: bytes) -> None:
        """Dispatch one CR-terminated SLCAN line."""
        if not line:
            # Bare CR = positive ACK for the previous command.
            self._send_ack = True
            self._send_ack_evt.set()
            return

        first = line[0:1]
        if first == b'z' or first == b'Z':
            # TX-ACK reply — also followed by CR which arrives on its
            # own; the simple ACK mechanism would set self._send_ack
            # then again on the bare CR following. Set True now and
            # let the bare-CR path be a no-op on an already-set evt.
            self._send_ack = True
            self._send_ack_evt.set()
            return

        if first in (b'V', b'N'):
            # Version / serial reply, ignore for now.
            return
        if first == b'F':
            # Status flags: one hex byte after F.
            try:
                self._status_flags = int(line[1:3], 16)
            except ValueError:
                pass
            return

        if first == b't':
            m = self._T_LINE_RE.match(line)
            if m:
                can_id = int(m.group(1), 16)
                dlc = int(m.group(2), 16)
                payload_hex = m.group(3)
                if len(payload_hex) >= dlc * 2:
                    data = bytes.fromhex(payload_hex[:dlc * 2].decode())
                    self._enqueue(CanFrame(can_id, False, data))
                return

        if first == b'T':
            m = self._BIG_T_LINE_RE.match(line)
            if m:
                can_id = int(m.group(1), 16)
                dlc = int(m.group(2), 16)
                payload_hex = m.group(3)
                if len(payload_hex) >= dlc * 2:
                    data = bytes.fromhex(payload_hex[:dlc * 2].decode())
                    self._enqueue(CanFrame(can_id, True, data))
                return

        # Unknown line — ignore. Some bridges echo unrecognised
        # commands; we don't.

    def _enqueue(self, frame: CanFrame) -> None:
        with self._rx_cond:
            self._rx_queue.append(frame)
            self._rx_cond.notify()


# ── Standalone CLI demo ─────────────────────────────────────────────

def _cli_main():
    """Command-line demo. Usage:
       python slcan_client.py [target] [--bitrate KBPS] [--send ID/HEX]

    target can be a serial port path/glob ('/dev/cu.usbmodem*') OR a
    TCP host:port ('192.168.188.143:8888').
    """
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('target', nargs='?', default='/dev/cu.usbmodem*',
                    help="serial port glob (default '/dev/cu.usbmodem*') "
                         "or TCP 'host:port'")
    ap.add_argument('--bitrate', type=int, default=250,
                    choices=SLCAN_BITRATES, help='kbit/s')
    ap.add_argument('--send', metavar='ID/HEX',
                    help="transmit one frame, then sniff (e.g. '18FF50E5/01020304')")
    ap.add_argument('--ext', action='store_true', help='use 29-bit ID for --send')
    ap.add_argument('--seconds', type=float, default=10,
                    help='how long to sniff')
    args = ap.parse_args()

    with SlcanClient(args.target, bitrate=args.bitrate) as c:
        if args.send:
            id_hex, _, data_hex = args.send.partition('/')
            sent = c.send(int(id_hex, 16), args.ext,
                          bytes.fromhex(data_hex))
            print(f"send → {'ACK' if sent else 'NACK / timeout'}")
        end = time.time() + args.seconds
        while time.time() < end:
            f = c.recv(timeout=0.5)
            if f is None:
                continue
            tag = 'EXT' if f.ext else 'STD'
            print(f"  {tag} 0x{f.id:08x} [{len(f.data)}] {f.data.hex(' ')}")


if __name__ == '__main__':
    _cli_main()
