# Serial Monitor

A browser-based serial console for ESP devices with a **large scrollback
history** so events never scroll away. Built in the same manner as the
SML emulator: one dependency-light Python server (only `pyserial`)
serving an embedded single-page UI — no Electron, no build step.

Useful when you need to see a boot log / crash backtrace / sporadic
event that the Arduino IDE / `screen` / `pio device monitor` would have
already scrolled past.

## Controls

- **Port selector** — auto-lists serial ports (`⟳` rescans, e.g. after
  plugging the device in). USB ports are listed first.
- **Baudrate selector** — 300 … 1500000 (default 115200; Tasmota uses
  115200, ESP boot ROM log is 74880).
- **Connect / Disconnect**.
- **Command input** (bottom bar, enabled while connected) — type a
  command, **Enter** (or **Send**) transmits it; ↑/↓ recall history.
  The line-ending selector appends CR LF (Tasmota console default),
  LF, CR, or nothing. Sent commands appear in the log as `» cmd`
  (`tx`) lines, so they are part of history/Save too.
- **Clear** — empties the view *and* the server history.
- **Save** — downloads the full server-side history as a timestamped
  `serial-YYYYMMDD-HHMMSS.log`.
- **Quit** — stops the server process (use this before relaunching so
  you don't end up talking to a stale instance).
- The last port + baud you connected with are remembered across
  restarts (`~/.serial_monitor.json`) and pre-selected on next launch.
- `autoscroll` — follow the tail (untick to scroll back freely while
  data keeps arriving).
- `time` — show/hide per-line timestamps.
- `hex` — render lines as space-separated byte hex (lines with
  non-printable/non-ASCII bytes carry their exact raw bytes; toggling
  re-renders the whole visible buffer, no reconnect needed).

## Large history

The server keeps a ring buffer of **200 000 lines** by default
(tens of MB RAM) — far beyond what a terminal scrollback holds. The
browser keeps up to 50 000 lines in view for performance, but **Save**
always dumps the *full* server ring. Override the depth with:

```
SERIAL_MONITOR_HISTORY=1000000 python3 serial_monitor_server.py
```

## Run

- **macOS:** double-click `Serial Monitor.app` (no Terminal window) or
  `Serial Monitor.command`. The browser opens at
  `http://localhost:8124/`.
- **Any OS / Linux:** `python3 serial_monitor_server.py`

Requires Python 3 and `pyserial` (`pip3 install pyserial`).

If it is already running, launching again just re-focuses the existing
instance (the port is reused, history is preserved).

## Notes

- Primarily a monitor: it only writes when you explicitly type a
  command and press Send/Enter. Idle (and while just watching) it
  never touches the port, so it is safe to leave attached during
  flashing/boot — but don't send commands during a flash, and close
  it first if the OS enforces exclusive port access.
- Bytes are decoded UTF-8 with replacement; a promptless partial line
  (no newline) is flushed after ~0.25 s so REPL/`>` prompts still show.
- One device at a time. To watch two devices, run a second copy with a
  different port: `SERIAL_MONITOR_HISTORY=… ` and edit `HTTP_PORT`.
