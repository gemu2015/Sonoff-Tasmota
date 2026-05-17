# Serial Monitor

A browser-based **ESP Swiss-army knife** — console *and* firmware
flasher — with a **large scrollback history** so events never scroll
away. Two capture sources, usable at the same time into one shared
history, plus two ways to flash:

- **Serial** — a local USB/serial port (`pyserial`).
- **Syslog** — a UDP syslog listener for devices you can't reach
  physically; lines are tagged per sending device IP.
- **Flash / Serial** — `esptool` (optional; ESP8266 + all ESP32).
- **Flash / OTA** — upload to a Tasmota device's `/u2` over the LAN.

Built in the same manner as the SML emulator: one dependency-light
Python server (only `pyserial`; syslog/OTA use stdlib; `esptool` only
if you serial-flash) serving an embedded single-page UI — no Electron,
no build step.

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
- **Flash ▾** — firmware flasher (toggles a panel). Pick a `.bin`,
  then either:
  - **Serial** — flashes via `esptool` (auto-detects ESP8266 / all
    ESP32 variants) using the **Port** selected in the top bar. Baud
    (default 460800), flash **Offset** (`0x0` for ESP8266 and ESP32
    *factory* images; `0x10000` for an app-only ESP32 image), and an
    optional **erase** first. The monitor on that port is closed
    automatically (esptool needs exclusive access). `esptool` is
    optional and only needed for serial flashing — if missing:
    `pip3 install --user esptool`.
  - **OTA** — uploads the `.bin` to a Tasmota device's web updater
    (`/u2`) over the LAN with a live progress bar. **⟳ Scan** finds
    Tasmota devices on the subnet (of the selected LogHost IP) in a
    couple of seconds and lists them by friendly name in the device
    dropdown (password-protected ones show as `(locked)`); or just
    type an IP/host. Enter the `WebPassword` if one is set. No cable
    needed (OTA is the default mode). **Picking/typing a device
    shows a live confirm card** — name · CPU/chip @MHz · flash size
    (app / free) · Tasmota version · IP · MAC · uptime · RSSI — so
    you can be sure it's the right target before overwriting it; the
    OTA button also re-shows it in the confirm dialog.

  Progress and full tool output stream into the same big-history
  console (and into Save). **Cancel** aborts a running serial job.
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

Cross-platform — one pure-Python server (stdlib + `pyserial`). The
browser opens at `http://127.0.0.1:8124/`.

- **macOS:** double-click `Serial Monitor.app` (no Terminal window) or
  `Serial Monitor.command`.
- **Windows:** double-click **`Serial Monitor.bat`** (uses
  `pythonw`/`pyw` so no console window stays open). Install Python 3
  from python.org (tick *Add python.exe to PATH*), then
  `py -m pip install pyserial`.
- **Linux:** double-click **`Serial Monitor.desktop`** (mark it
  *Allow Launching* / executable the first time) or run
  **`./serial_monitor.sh`**. `pip3 install --user pyserial`.
- **Any OS:** `python3 serial_monitor_server.py`.

Requires Python 3 and `pyserial`.

Platform notes:

- **Linux serial access** usually needs the `dialout` group:
  `sudo usermod -aG dialout "$USER"` then re-login (else the port
  shows but won't open: *permission denied*).
- **Syslog port < 1024** (e.g. the default 514) needs root on
  macOS/Linux — use a high port like **5514** and set Tasmota
  `LogPort 5514` (no sudo). On Windows 514 normally works as-is.
- The **port/baud/syslog dropdowns and `LogHost` IP list adapt per
  OS** (Windows IPs/adapter names come from `ipconfig`, Linux from
  `ifconfig`/`ip`, macOS adds the Wi-Fi/Ethernet port labels).

If an instance is already running, launching again **replaces it**
(it asks the old one to quit, waits for the port, then takes over) so
a relaunch always runs the current code — no stale-process trap. The
browser tabs reconnect automatically; in-memory history is cleared by
the restart.

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
