# Serial Monitor — ESP/Tasmota Swiss-army knife

A browser-based tool for ESP devices: a **console** (serial + remote
syslog) with a huge scrollback, **plus** a firmware **flasher**
(serial via esptool, or OTA over the LAN). One dependency-light Python
server (stdlib + `pyserial`) serving an embedded single-page UI — no
Electron, no build step, no cloud.

It opens at `http://127.0.0.1:8124/`.

---

## Quick start

Cross-platform (macOS / Windows / Linux). Needs **Python 3** and
**`pyserial`** (`esptool` only if you serial-flash).

| OS | Launch | Get deps |
|----|--------|----------|
| **macOS** | double-click `Serial Monitor.app` (no Terminal) or `Serial Monitor.command` | Python from python.org; `pip3 install pyserial` |
| **Windows** | double-click `Serial Monitor.bat` (no console window) | Python from python.org (tick *Add to PATH*); `py -m pip install pyserial` |
| **Linux** | double-click `Serial Monitor.desktop` (mark *Allow Launching*) or `./serial_monitor.sh` | `pip3 install --user pyserial` |
| **Any** | `python3 serial_monitor_server.py` | — |

Relaunching while it's already running **replaces** the old instance
(loads current code, no stale-process trap); open browser tabs
reconnect automatically.

---

## 1. Console

- **Serial** — pick a **Port** + **Baud** (default 115200; ESP boot-ROM
  log is 74880), **Connect**. The port list auto-populates (`⟳`
  rescans); last port/baud are remembered (`~/.serial_monitor.json`).
- **Syslog** — for devices with no cable. Pick the UDP port (default
  514; `<1024` needs root, so use e.g. **5514**), **Listen**, then on
  each device: `Backlog LogHost <PC-IP>; LogPort <port>; SysLog 2`.
  The **LogHost** dropdown shows this PC's IPs (📋 copies one); the
  *Listen* log line prints a ready-to-paste `Backlog …`. Every line is
  tagged with the sender IP (`src` checkbox shows/hides it) so many
  devices share one stream. *Syslog only flows once WiFi is up — not
  the boot-ROM log / pre-network crash; use Serial for those.*
- Serial and Syslog can run **at the same time** into one history.
- **Command input** (bottom, when serial-connected) — type, **Enter**
  to send; ↑/↓ history; line-ending selector (CR LF default). Sent
  commands show as `» cmd`.
- **hex** — byte view (binary lines keep exact bytes). **time** —
  toggle timestamps. **autoscroll** — follow tail.
- **Save** — downloads the *full* server-side history
  (`serial-YYYYMMDD-HHMMSS.log`). **Clear** — empties view + history.
- **Quit** — stops the server and shows a "stopped" page.

### Large history

Ring buffer of **200 000 lines** by default (tens of MB). Browser
keeps 50 000 in view; **Save** always dumps the full ring. Override:

```
SERIAL_MONITOR_HISTORY=1000000 python3 serial_monitor_server.py
```

---

## 2. Flasher  (**Flash ▾**)

Pick a `.bin`, choose **OTA** (default) or **Serial**.

### Which method?

| Situation | Use |
|-----------|-----|
| Device already on WiFi (update) | **OTA** — no cable, no esptool, keeps filesystem & config |
| Blank chip / recovery / partition change | **Serial** (needs esptool) |
| Non-developer, no esptool | **OTA**, or the **Tasmota Web Installer** link, or one-click isolated install |

### OTA

Serves the `.bin` on a temporary LAN port and tells the device to pull
it via `OtaUrl` + `Upgrade 1`. The device does the ESP32 **safeboot**
partition switch itself, so full-size images work and it's
language-independent. Progress tracks the device's download; then it
reboots and the tool re-reads the version to confirm (up to ~4 min).

- **⟳ Scan** finds Tasmota devices on the subnet in ~seconds, listed
  by friendly name (password-protected = `(locked)`); or type an
  IP/host. Enter **WebPassword** if set.
- Selecting a device shows a **confirm card**: name · chip @MHz ·
  flash size (app / free) · Tasmota version · IP · MAC · uptime ·
  RSSI, **plus the ESP32 partition table** (safeboot / app0 / fs /
  custom, with used %, over-full = red) — so you know it's the right
  device and the image fits *before* overwriting it.

### Serial

Flashes via `esptool` (`--chip auto`: ESP8266 + every ESP32). **The
offset is auto-detected from the image** — you never type it:

- *factory*/merged image, or ESP8266 image → flashed at `0x0`.
- ESP32 **app-only** image → rejected (no fixed serial offset; its
  app partition lives in the device's table) with a hint to use the
  matching `*.factory.bin`, or OTA.

Detection reads the ESP header + the partition-table signature
(`0xAA50` @ `0x8000`). Optional **erase** first; the monitor on that
port is auto-closed. If esptool's RAM stub fails (some UART bridges →
*Checksum error*) it **auto-retries with `--no-stub` at 115200**.

> **For serial flashing, use the `*.factory.bin`** (offset handled for
> you). On ESP32-S3/C3 prefer the **"USB JTAG/serial debug unit"**
> port — UART bridges often fail esptool's stub upload.

**No esptool?** The panel offers **Install esptool (isolated)** — a
private venv at `~/.serial_monitor_venv` (PEP 668-safe, never touches
system Python) — or the official **Tasmota Web Installer**
(<https://tasmota.github.io/install/>, browser-based, nothing to
install). OTA needs no esptool at all.

Progress + full tool output stream into the same console (and Save).
**Cancel** aborts a running serial job.

---

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| Double-click `.app` does **nothing** (macOS) | macOS TCC blocks Finder-launched apps from reading `~/Desktop` etc. The `.app` runs a copy bundled in `Contents/Resources/`; after editing the server run **`sync_app.command`**. Failures log to `~/Library/Logs/SerialMonitor-launch.log`. |
| Safari "server unreachable" / "Load failed" | A stale old instance was serving. Relaunch (auto-replace) and ⌘⇧R. Server binds loopback + sends no-store; the page is always local. |
| Selected device / scan flaky, devices missing | Fixed: two-phase scan (connect-scan, then HTTP probe) + proxy bypass. Relaunch to get current code. |
| Serial: `Failed to write to target RAM … Checksum error` | esptool's stub upload fails on some UART bridges. Auto-retries `--no-stub`@115200; better: use the **USB-JTAG** port. |
| Serial: `0105 … message invalid` at seq 0 | `--no-stub` at high baud — it's pinned to 115200 automatically. |
| Flashed at 0x0 and device won't boot | You used an **app-only** image at 0x0. Use the **`*.factory.bin`** (now auto-detected/blocked). |
| Linux: port shows but won't open | Add yourself to `dialout`: `sudo usermod -aG dialout "$USER"`, re-login. |
| Syslog won't bind 514 | `<1024` needs root on macOS/Linux. Use 5514 and `LogPort 5514` (Windows: 514 is fine). |
| OTA: settings/filesystem after flash | OTA keeps fs + config. **Serial** flashing a factory image resets settings and can wipe `fs` (esp. with **erase**). Back up first; prefer OTA for updates. |

---

## Files in this folder

| File | Purpose |
|------|---------|
| `serial_monitor_server.py` | The whole tool (server + embedded UI) |
| `Serial Monitor.app` | macOS double-click launcher (runs the bundled copy) |
| `sync_app.command` | Refresh the `.app`'s bundled server after edits (macOS) |
| `Serial Monitor.command` | macOS Terminal launcher |
| `Serial Monitor.bat` | Windows launcher (no console window) |
| `serial_monitor.sh` / `Serial Monitor.desktop` | Linux launchers |
| `README.md` | This file |

---

## Notes

- Primarily a monitor: it only writes to the port when you send a
  command or flash. Idle, it never touches the port — safe to leave
  attached during boot (don't send during a flash).
- Bytes are decoded UTF-8 with replacement; a promptless partial line
  is flushed after ~0.25 s so REPL/`>` prompts still show.
- One serial device at a time. For two, run a second copy with a
  different `HTTP_PORT`.
- Per-OS adaptation: serial ports, the `LogHost` IP list, and adapter
  labels come from `ipconfig` (Windows) / `ifconfig`/`ip` (Linux) /
  `networksetup` (macOS).
