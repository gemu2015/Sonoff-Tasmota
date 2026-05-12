# Shelly / EcoTracker Tester

> **Origin / credit.** This tool is a cross-platform port of the
> original Windows-only PowerShell GUI by **ottelo**
> (<https://ottelo.jimdofree.com/>). The UI layout, German labels,
> color-coded log, JSON pretty-printer, and the persistent UDP
> listener concept are all from the original. This rewrite is just
> the same tool ported to Python so it runs on macOS and Linux too —
> all design credit goes to ottelo.

Browser GUI for poking at Shelly and EcoTracker devices on the local
network. Sends UDP-RPC commands on port 1010, HTTP GET requests on
port 80, and runs ICMP ping loops with statistics — all from the same
dark-themed UI.

The credit banner at the top of the running app links back to
<https://ottelo.jimdofree.com/>; please leave it visible if you
distribute modified copies.

## What it does

| Mode | What it sends | Use it for |
| ---- | ------------- | ---------- |
| **UDP** (default port 1010) | Arbitrary text payload — typically `Shelly.GetStatus` or `EM.GetStatus` for a Shelly RPC, or any string a custom firmware listens for | Talking to Shelly devices in their local UDP-RPC mode, or to any device using a UDP-text protocol |
| **HTTP GET** (default port 80) | Arbitrary URL path | Hitting `/rpc/Shelly.GetStatus`, `/v1/json` (EcoTracker), or any web endpoint |
| **Ping** | ICMP echo via the system `ping` binary | Connectivity testing; loss / RTT statistics |
| **Jackery-Emu** (default port 80) | HTTP/1.1 GETs on ONE persistent TCP socket, like the real Jackery Homepower 2000 polls a physical EcoTracker | Validating that an EcoTracker emulator (e.g. ottelo's TinyC `ecotracker.tc`) correctly keeps the TCP socket open across polls — the property Jackery firmware requires and the four other modes can't observe (they all open fresh sockets per request) |

All three modes share a single host/IP field, color-coded answer log,
and a pretty-printed JSON pane that auto-parses any response.

Extras:

- **UDP listener** — bind a persistent local socket so the device can
  push unsolicited packets back and they're captured in the log
  (lavender entries). When active, outgoing UDP requests reuse the
  same socket so server replies arrive at the listener port too.
- **Interval mode** — re-send the same UDP/HTTP request every N
  seconds (sane way to watch a value evolve without touching the
  Send button).
- **Ping log file** — optional, written to `~/Desktop/ping_<host>_<stamp>.log`
  with summary statistics on stop. "Nur Fehler loggen" mode skips the
  successful pings to keep the log tight when you're hunting for drops.

## Quick start

### macOS

1. Double-click **`Shelly Tester.app`**.
   On first run, macOS may ask about an unsigned application — open
   System Settings → Privacy & Security → "Open Anyway".
2. Browser opens automatically at `http://localhost:8200/`.

### Linux

```bash
cd Shelly_Tester_Linux
chmod +x shelly_tester.sh           # first run only
./shelly_tester.sh
```

Browser opens automatically. Requires only `python3` (standard
library — no external packages).

If your Linux distribution gates ICMP, prefix the launcher with
`sudo` once or grant cap_net_raw to `python3` for ping mode to work.
UDP and HTTP modes don't need privileges.

### Windows

Double-click **`Shelly_Tester.bat`**. Same behavior as Linux. Python 3
must be in `PATH`.

### From source (developers)

```bash
cd tasmota/tinyc/utils/shelly_tester
python3 shelly_tester_gui.py [--port 8200] [--no-browser]
```

`--no-browser` is handy on a remote / headless machine where you'll
hit the GUI from another box.

## Cross-platform notes

The Python ping uses the system `ping` binary via `subprocess` and
parses RTT/TTL from output. Argument flags differ by OS — handled
internally:

| Platform | Command |
| -------- | ------- |
| Windows | `ping -n 1 -w <ms> host` |
| macOS | `ping -c 1 -W <ms> host` |
| Linux | `ping -c 1 -W <s> host` |

All three return a successful exit code when at least one reply
arrives, which is what our parser relies on. RTT regex covers
`time=42ms`, `time=42 ms`, and `time<1ms` formats.

## File / distribution layout

```
Shelly Tester.app/                  macOS app bundle
  Contents/
    Info.plist
    MacOS/run                       launcher (calls python3 on the bundled .py)
    Resources/
      shelly_tester_gui.py          single-file Python server + embedded HTML

Shelly_Tester_Linux/                Linux folder
  shelly_tester.sh                  launcher
  shelly_tester_gui.py
  README.txt                        (en)

Shelly_Tester_Win/                  Windows folder
  Shelly_Tester.bat                 launcher
  shelly_tester_gui.py
  LIESMICH.txt                      (de)
```

The Python file embeds the entire HTML/JS UI as a triple-quoted
string — one file, no resources to ship around. Edit the `HTML_PAGE`
constant directly to tweak the UI.

## Architecture

```
   ┌──────────────┐   localhost   ┌───────────────────┐   socket / urllib   ┌─────────────────┐
   │  Browser UI  │ ◄────────────► │  Python backend  │ ◄─────────────────► │  Shelly /       │
   │  (HTML/JS)   │   /api/*       │  (Handler)       │  UDP / HTTP / ICMP  │  EcoTracker /   │
   └──────────────┘                └───────────────────┘                     │  any device     │
                                                                             └─────────────────┘
```

`/api` endpoints:

| Path                       | Method | Purpose                                                            |
| -------------------------- | ------ | ------------------------------------------------------------------ |
| `/`                        | GET    | The HTML page (with `__VERSION__` substituted)                     |
| `/api/udp/send`            | POST   | Send a UDP datagram, collect responses (uses listener if active)   |
| `/api/udp/listener`        | POST   | `action: 'start' | 'stop'` — manage the persistent listener socket |
| `/api/udp/poll`            | GET    | Drain pending listener-captured packets (browser polls every 250 ms) |
| `/api/http/get`            | POST   | Make an HTTP GET to `host:port/path`                               |
| `/api/ping/start`          | POST   | Start a background ping loop                                       |
| `/api/ping/stop`           | POST   | Stop the ping loop                                                 |
| `/api/ping/status`         | GET    | Live stats + recent results (browser polls every 200 ms while running) |
| `/api/jackery/start`       | POST   | Start a Jackery-Emu session (one persistent socket, N HTTP/1.1 GETs at `interval_s`) |
| `/api/jackery/stop`        | POST   | Stop the running Jackery-Emu session                               |
| `/api/jackery/status`      | GET    | Live state + per-iteration log (browser polls every 500 ms while running) |
| `/api/shutdown`            | POST   | Clean exit (300 ms grace, then `os._exit`)                         |

UDP send/receive is one-shot per call (3-second receive timeout). The
listener runs in its own thread and accumulates packets in a 500-cap
ring buffer for the browser to poll.

## Troubleshooting

| Symptom | Likely cause |
| ------- | ------------ |
| `UDP Timeout: keine Antwort` | Wrong port (default 1010 — check the device), firewall blocking outbound UDP, or the device isn't running its UDP-RPC service |
| `HTTP 401 / 403` | Device requires authentication — Shelly's `/rpc/` endpoints don't (yet), but other paths might |
| Ping `NoPing` / `ping binary not found` | Highly unusual, but can happen on minimal Docker images. Install the `iputils-ping` (Linux) / standard ping (Windows) |
| Linux ping always fails despite `ping` working in shell | Some distros require root or `cap_net_raw` for ICMP. `sudo ./shelly_tester.sh` once to confirm; permanent fix: `sudo setcap cap_net_raw+ep $(which python3)` |
| Listener "kein lokaler Port" / port-in-use | Should auto-pick free port; if you see this, another instance is already running |
| Page is blank | Port 8200 already used; pass `--port 8201` |

## Implementation notes

- **No external Python deps** — pure stdlib (`socket`, `urllib`,
  `subprocess`, `http.server`). Same baseline as the SML Emulator and
  UDP Monitor in the sibling utils.
- **Jackery-Emu mode** — uses raw `socket` directly (not `urllib`),
  opens exactly one TCP connection and pipelines HTTP/1.1 GETs on it
  with `Connection: keep-alive`. Reads responses with Content-Length
  awareness OR chunked-transfer decoding, depending on the server's
  framing choice. Detects the failure modes that matter for the
  Jackery-EcoTracker handshake: server closing socket mid-stream
  (= keep-alive broken on the emulator side), server returning
  `Connection: close` (= per-response opt-out even if the socket
  stays alive), 4xx/5xx status (= wrong path or auth required),
  truncated bodies. Verdict on session end:
  - **PASS** if all iterations returned 2xx AND no per-response
    `Connection: close` AND no socket teardown.
  - **STOPPED** if the run was clean but ended early (target count
    reached, user-stop, or stop-reason set by a Connection: close
    flag).
  - **FAIL** otherwise — typically socket closed mid-response or a
    sequence of non-2xx replies.
- **Persistent UDP listener** — Shelly's RPC sometimes pushes
  unsolicited packets after a control command (e.g. set output state).
  The listener socket binds with `SO_REUSEADDR` so it can co-exist
  with other listeners; OS picks a free port for it. Outgoing UDP
  reuses this socket if it's active so server replies arrive on the
  same port.
- **JSON parsing for glued responses** — devices occasionally send two
  back-to-back JSON objects in a single UDP packet (`...}{...`). The
  parser detects that pattern, surfaces only the first complete object
  in the JSON pane, and tags the log entry with "Paket doppelt
  erhalten" (orange) so it's visible without breaking the JSON view.
- **Ping fast-mode** — `Intervall = 0` means as fast as possible while
  leaving CPU room for the GUI poll. Effective rate ~10/s; if you need
  raw flood pings, use the system `ping -i 0` flag manually.

## Files in this directory

| File                       | Role                                                  |
| -------------------------- | ----------------------------------------------------- |
| `shelly_tester_gui.py`     | Single-file Python server + embedded HTML            |
| `Shelly_Tester.zip`        | Pre-built distribution for macOS / Linux / Windows   |
| `README.md`                | This file                                            |

## Credit

Original Windows PowerShell GUI by **ottelo** —
<https://ottelo.jimdofree.com/>.

## See also

- `../sml_emulator/` — emulate smart meters over USB-serial
- `../udp_monitor/` — sniff TinyC `global` and Scripter `=>` UDP
  multicasts on `239.255.255.250:1999`
