# UDP Monitor

A desktop tool that scans the local network for **TinyC and Scripter
UDP multicast variables** broadcast by Tasmota devices, decodes them,
and shows a sortable table of every variable + its publishing device.

Background: TinyC's `global` keyword and Scripter's `=>` operator both
broadcast variable updates as UDP multicast packets on
**`239.255.255.250:1999`** so multiple Tasmota devices can share state
on the same LAN without an MQTT broker. This tool listens for those
packets and tells you who's publishing what — invaluable when you're
wiring up a multi-device setup and need to see what names are taken,
which device owns which variable, or whether two devices are
clashing on the same name.

## What you see

For each variable picked up during the scan:

- **Source IP** + device name (resolved via `http://<ip>/`'s hostname
  page, with a small thread pool to keep resolution fast).
- **Variable name**.
- **Type** — `ascii`, `bin float`, or `bin float[N]` (the three formats
  TinyC's UDP encoder produces).
- **Current value**.
- **Send interval** — measured average period between observations.
- **Receive count** — how many packets contributed to this row.
- **Name clashes** — variables with the same name announced by two or
  more devices, called out in a separate section.

Sortable columns + CSV export.

## Quick start

### macOS

1. Double-click `UDP Monitor.app` from the unpacked distribution.
   On first run macOS may prompt about an unsigned application — open
   System Settings → Privacy & Security → "Open Anyway".
2. The browser opens automatically at `http://localhost:8199/`.
3. Pick a scan duration (default 60 s), click **Start Scan**.

### Linux

```bash
cd UDP_Monitor_Linux
chmod +x udp_monitor.sh           # first run only
./udp_monitor.sh
```

The browser opens automatically. Requires only `python3` (standard
library is enough — no extra packages needed).

If the firewall is blocking multicast on port 1999, allow it:

```bash
sudo ufw allow 1999/udp
```

### Windows

Double-click `UDP_Monitor.bat`. Same behaviour as Linux. Python 3 must
be in `PATH`.

### From source (developers)

```bash
cd tasmota/tinyc/utils/udp_monitor
python3 udp_monitor_gui.py             # browser UI on http://localhost:8199/
# or
python3 udp_monitor.py [seconds]       # CLI version, prints to stdout
```

The CLI version (`udp_monitor.py`) is handy for SSH / headless servers —
prints a plain-text table after the scan and exits.

## Two scripts, one parser

| File                   | Role                                                                      |
| ---------------------- | ------------------------------------------------------------------------- |
| `udp_monitor_gui.py`   | Full GUI: HTTP server + browser UI + thread pool name resolver + CSV     |
| `udp_monitor.py`       | CLI-only: same parser, prints summary table to stdout                    |
| `README.md`            | This file                                                                 |

Both share the same `parse_packet()` logic so anything one decodes,
the other does too.

## Packet format (what we're decoding)

Tasmota's UDP multicast packets are line-oriented text where each line
is one variable. A few format variants coexist:

```
=>name=value          ← Scripter ASCII variable
name=value            ← same, without the leading marker
name:<2-byte LE len><N×float32 LE>   ← TinyC bin float array
name:<float32 LE>     ← TinyC bin float scalar
```

`udp_monitor.parse_packet()` walks each line, picks the first `=` or
`:` separator, and decides between ASCII or binary based on which
appears first. For binary lines, a 16-bit LE length prefix that
matches `(payload_length - 2) / 4` indicates an array; otherwise it's
treated as a single float32.

## Network requirements

- Your laptop and the Tasmota devices must be on the same broadcast
  domain (same VLAN / same subnet — multicast doesn't cross routers
  without IGMP forwarding).
- Multicast group: `239.255.255.250`, port `1999`.
- Some macOS / Linux firewalls block this by default. On macOS the
  built-in firewall usually doesn't, but corporate VPN clients
  sometimes do.

## Troubleshooting

| Symptom                                                | Likely cause                                                                |
| ------------------------------------------------------ | --------------------------------------------------------------------------- |
| `0 packets received`                                   | Firewall blocks port 1999, or you're on a different subnet from the devices |
| Variables seen but device name shows IP only           | The device's `/` page is locked behind authentication, or HTTP timed out    |
| One variable name shows up under multiple devices      | Real clash — each device thinks it owns that name; check both scripts      |
| Browser tab opens but page is blank                    | Port `8199` already in use; pass `--port 8299` to `udp_monitor_gui.py`     |

## Distribution layout

```
UDP Monitor.app/                  macOS app bundle
  Contents/
    Info.plist
    MacOS/UDP_Monitor             launcher script
    Resources/
      udp_monitor_gui.py          GUI version (canonical)
      udp_monitor.py              CLI version
      AppIcon.icns

UDP_Monitor_Linux/                Linux folder
  udp_monitor.sh                  launcher
  udp_monitor_gui.py
  README.txt                      (en)

UDP_Monitor_Win/                  Windows folder
  UDP_Monitor.bat                 launcher
  udp_monitor_gui.py
  LIESMICH.txt                    (de)
```

## Implementation notes

- **GUI is browser-based, not Tk/Qt.** Same architecture as the SML
  Emulator: a tiny HTTP server (`http.server`) on `localhost:8199`
  serves an HTML page that polls `/api/state` once per second for live
  scan progress. Zero external dependencies, ships in a ~15 KB Python
  file.
- **Name resolution is parallelized.** After the scan completes, a
  `ThreadPoolExecutor(8)` fetches `http://<ip>/` for each unique
  source IP to extract the device's friendly name. Running serial
  with a 1 s timeout, 30 devices = 30 s; with 8 workers ≈ 4 s.
- **Multicast socket setup**: `IP_ADD_MEMBERSHIP` to join 239.255.255.250
  on `INADDR_ANY` so the OS picks the right interface. SO_REUSEADDR
  is set so multiple instances can run on the same machine for testing.

## Files in this directory

| File                  | Role                                |
| --------------------- | ----------------------------------- |
| `udp_monitor_gui.py`  | Browser-UI version (default)        |
| `udp_monitor.py`      | CLI version                         |
| `README.md`           | This file                           |

## See also

- `../sml_emulator/` — companion tool for emulating real smart meters
  on a USB-serial dongle.
- Tasmota TinyC `global` keyword + Scripter `=>` for the publishing
  side of the same protocol — see `../../TinyC_Reference.md` and
  `../../examples/udp.tc`.
