# Serial Monitor

A browser-based console for ESP devices with a **large scrollback
history** so events never scroll away. Two capture sources, usable at
the same time into one shared history:

- **Serial** — a local USB/serial port (`pyserial`).
- **Syslog** — a UDP syslog listener for devices you can't reach
  physically; lines are tagged per sending device IP.

Built in the same manner as the SML emulator: one dependency-light
Python server (only `pyserial`; syslog uses stdlib) serving an embedded
single-page UI — no Electron, no build step.

Useful when you need to see a boot log / crash backtrace / sporadic
event that the Arduino IDE / `screen` / `pio device monitor` would have
already scrolled past — or to collect logs from a device that's in a
ceiling/wall with no cable access.

## Controls

- **Port selector** — auto-lists serial ports (`⟳` rescans, e.g. after
  plugging the device in). USB ports are listed first.
- **Baudrate selector** — 300 … 1500000 (default 115200; Tasmota uses
  115200, ESP boot ROM log is 74880).
- **Connect / Disconnect** — open/close the selected serial port.
- **Syslog listener** — for devices you can't reach with a cable. Set
  the UDP port (default 514; `<1024` may need root — use e.g. `5514`)
  and press **Listen**. Then on each device:
  `Backlog LogHost <this-PC-IP>; LogPort <port>; SysLog 2`. Every line
  is tagged with the sending device's IP, so several remote devices
  can be watched in one stream (toggle the `src` checkbox to show/hide
  the IP). Serial and Syslog can run **at the same time**, both
  feeding the same history/Save. Caveat: syslog only flows once WiFi
  is up — it won't capture the boot-ROM log or a pre-network crash
  (use Serial for those).
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
  `http://127.0.0.1:8124/`.
- **Any OS / Linux:** `python3 serial_monitor_server.py`

Requires Python 3 and `pyserial` (`pip3 install pyserial`).

If it is already running, launching again just re-focuses the existing
instance (the port is reused, history is preserved).

### macOS TCC / `~/Desktop` note (why the .app runs a bundled copy)

Modern macOS privacy (TCC) blocks a **Finder-launched unsigned app**
from reading `~/Desktop`, `~/Documents`, `~/Downloads`. When this repo
lives under one of those (e.g. `~/Desktop/...`), running the repo's
`serial_monitor_server.py` directly from the `.app` fails with
`[Errno 1] Operation not permitted` and the app *appears to do
nothing*. A `.command` / Terminal run works because Terminal already
holds the Desktop grant.

Fix: the `.app` ships its own copy of the server at
`Serial Monitor.app/Contents/Resources/serial_monitor_server.py` and
runs **that** — an app reading files inside its *own bundle* is exempt
from the Desktop restriction. After editing the canonical
`serial_monitor_server.py`, double-click **`sync_app.command`** (or
`cp serial_monitor_server.py "Serial Monitor.app/Contents/Resources/"`)
to refresh the bundled copy. Launch failures are no longer silent: any
problem raises a dialog and is logged to
`~/Library/Logs/SerialMonitor-launch.log`.

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
