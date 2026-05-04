# SML Emulator

A desktop tool that emulates real smart meters over a USB-serial dongle so
you can develop and test Tasmota's SML driver descriptors without owning
the actual hardware. Pushes (and for poll/response protocols, *answers*)
genuine wire frames at the configured baud rate, with byte-stuffing,
CRC, and protocol-level framing identical to what the real meter sends.

Useful for:

- Iterating on a smart-meter descriptor (`>M 1` block) without the meter
  in front of you.
- Verifying Tasmota's SML decoder against a known-good byte stream.
- Comparing two firmware revisions side-by-side on the same input.
- Prototyping a new meter profile from a published spec or PDF.

## Supported protocols & profiles

| Profile (dropdown)                  | Wire protocol                   | Direction      |
| ----------------------------------- | ------------------------------- | -------------- |
| EHZ363W — Easymeter 1-phase         | SML binary                      | push           |
| EMH eHZ — 1-phase                   | SML binary                      | push           |
| Landis+Gyr E220 — 1-phase           | SML binary                      | push           |
| Landis+Gyr E450 (mode 7)            | DLMS push, AES-128-GCM          | push           |
| Resol Deltasol BS Plus              | VBus (Resol)                    | push           |
| Wolf CSZ 11/300                     | EBus (heater)                   | push           |
| Sensus PolluCom F                   | M-Bus heat meter                | push           |
| Kamstrup OMNIPOWER (Kx7)            | Kamstrup poll/response          | bidirectional  |
| ISKRA MT174 — 3-phase               | IEC 62056-21 OBIS ASCII         | push           |
| Generic OBIS (1-phase / 3-phase)    | IEC 62056-21 OBIS ASCII         | push           |
| Eastron SDM630 — 3-phase            | Modbus RTU                      | bidirectional  |
| Eastron SDM630 (TCP)                | Modbus TCP                      | bidirectional  |
| Apator T510 — 3-phase               | IEC 62056-21 mode-C handshake   | bidirectional  |

For bidirectional protocols (Kamstrup, Modbus RTU, T510) the Python
server runs a state machine that watches for incoming polls and emits
the matching response — see *Architecture* below.

## Quick start

### macOS (recommended)

1. Double-click `SML Emulator.app` from the unpacked distribution.
   On first run, macOS may prompt about an unsigned application — open
   System Settings → Privacy & Security → "Open Anyway".
2. The browser opens automatically at `http://localhost:8198/`.
3. Plug in your USB-serial adapter, pick it from the *Port* dropdown,
   choose a meter profile, click **Connect** and **Start**.

### Linux

```bash
cd SML_Emulator_Linux
chmod +x sml_emulator.sh        # first run only
./sml_emulator.sh
```

The script launches `python3 sml_emulator_server.py` and opens the
browser. Requires `python3` and the `pyserial` package:

```bash
pip3 install pyserial
```

(Most distros' `python3-serial` package works too.)

### Windows

Double-click `SML_Emulator.bat` from the unpacked distribution. The
batch file requires Python 3 with `pyserial` installed:

```cmd
py -m pip install pyserial
```

### From source (developers)

```bash
cd tasmota/tinyc/utils/sml_emulator
python3 sml_emulator_server.py [--port 8198]
```

Open `http://localhost:8198/` in your browser. The server reads
`sml_emulator.html` from the same directory at request time, so any
edit to the HTML is live-reloaded on the next page refresh — no
rebuild step.

## Architecture

```
   ┌──────────────┐  HTTP/JSON     ┌────────────────────┐  pyserial   ┌─────────────────┐
   │  Browser UI  │ ────────────►  │  Python bridge     │ ──────────► │  USB-serial     │
   │  (HTML/JS)   │  /api/*        │  (sml_emulator     │   bytes     │  adapter        │
   │              │ ◄────────────  │   _server.py)      │ ◄────────── │                 │
   └──────────────┘                └────────────────────┘             └────────┬────────┘
                                                                               │
                                                                               │ TTL UART
                                                                               ▼
                                                                      ┌─────────────────┐
                                                                      │  Tasmota device │
                                                                      │  (SML decoder)  │
                                                                      └─────────────────┘
```

The HTML/JS layer:
- Renders the meter-config UI, panel, value sliders, log window.
- For each meter profile, builds the wire-format byte stream entirely
  in JS (e.g. SML TLV encoder, VBus septet packer, M-Bus DIB+VIB
  records, AES-GCM-encrypted DLMS, Kamstrup `kstr` codec + byte
  stuffing).
- Sends bytes to the Python bridge via `POST /api/send`.
- Generates a matching Tasmota descriptor (`>M 1` block) you can
  copy-paste into the device's smart-meter section.

The Python bridge:
- Owns the USB-serial port via `pyserial` (browser Web Serial API
  doesn't have packaging-friendly auto-port discovery on macOS, hence
  the bridge model).
- For push-only protocols, just relays the bytes the browser produces.
- For bidirectional protocols, runs a dedicated state machine in a
  worker thread:
  - `_t510_runner` — IEC 62056-21 mode-C handshake (300 bd `/?!\r\n` →
    9600 bd identification → ACK → OBIS data → back to 300 bd).
  - `_ks_runner` — Kamstrup poll watcher: detects `0x80…0x0d` request
    frames, decodes the count + register list, builds the matching
    `0x40…0x0d` response with current values from `reg_bank`.
  - `_process_modbus_rtu` — Modbus RTU slave: replies to FC03/04/06/16
    addressed at unit ID 1.
  - `modbus_tcp_server` — Modbus TCP slave on port 502.
- Mediates register-value updates from the browser via `POST /api/regs`
  (numeric int keys for Modbus, string keys like `ks_vL1` for Kamstrup).

## Distribution layout

`SML_Emulate.zip` contains three platform copies:

```
SML Emulator.app/                    macOS app bundle
  Contents/
    Info.plist
    MacOS/SML_Emulator               launcher script
    Resources/
      sml_emulator.html              source-of-truth HTML
      sml_emulator_server.py         Python bridge
      AppIcon.icns

SML_Emulator_Linux/                  Linux folder
  sml_emulator.sh                    launcher
  sml_emulator.html
  sml_emulator_server.py
  README.txt                         (en)

SML_Emulator_Win/                    Windows folder
  SML_Emulator.bat                   launcher
  sml_emulator.html
  sml_emulator_server.py
  LIESMICH.txt                       (de)
```

All three platforms share the same HTML and Python files — only the
launcher and surrounding metadata differ.

## Rebuilding the zip after a source edit

When you edit `sml_emulator.html` or `sml_emulator_server.py` in the
source tree, sync the three platform copies inside the zip and re-pack:

```bash
cd /tmp && rm -rf SML_Emulate_build && mkdir SML_Emulate_build && cd SML_Emulate_build
unzip -q /path/to/tasmota/tinyc/utils/sml_emulator/SML_Emulate.zip

SRC=/path/to/tasmota/tinyc/utils/sml_emulator
for dest in "SML Emulator.app/Contents/Resources" "SML_Emulator_Linux" "SML_Emulator_Win"; do
  cp "$SRC/sml_emulator.html"      "$dest/"
  cp "$SRC/sml_emulator_server.py" "$dest/"
done

zip -qry /path/to/tasmota/tinyc/utils/sml_emulator/SML_Emulate.zip \
  "SML Emulator.app" "SML_Emulator_Win" "SML_Emulator_Linux"
```

If the user's macOS already has `SML Emulator.app` open, you can also
sync the live `.app` so the browser-reload picks up changes without
re-installing from the zip:

```bash
cp $SRC/sml_emulator.html      "/Users/<you>/Desktop/SML Emulator.app/Contents/Resources/"
cp $SRC/sml_emulator_server.py "/Users/<you>/Desktop/SML Emulator.app/Contents/Resources/"
```

(For HTML changes, just refresh the browser. For server changes,
quit and relaunch the .app so the Python process restarts.)

## Implementation notes & gotchas

- **Web Serial vs Python bridge.** Earlier versions used the browser's
  Web Serial API directly. That works on Chrome/Edge but not Firefox
  or Safari, and the bridge model gives us bidirectional state
  machines for poll/response protocols, which Web Serial can't host
  reliably while also rendering UI. Today the HTML detects bridge
  presence at load time and falls back to Web Serial only when
  served standalone (no bridge).
- **CRC + byte-stuffing matter.** Real meters compute CRC over
  un-stuffed data, then the stuffer escapes specific bytes. Wire
  frames have to round-trip through both directions exactly — see the
  Kamstrup builder for a complete worked example covering CRC-16
  (poly 0x1021 augmented), byte stuffing for `0x80/40/0d/06/1b`, and
  the `kstr` value codec.
- **Tasmota's `kstr` decoder has a bug**: `for (uint16_t x = 1;
  x <= i; ++x)` silently drops negative exponents (loop iterates
  zero times when `i<0`). The Kamstrup encoder works around this by
  always emitting `exp=0` and letting the descriptor's `@i0:DIVISOR`
  rescale.
- **Default `sbsiz` (48 bytes)** is too small for some long frames.
  M-Bus profiles need `=so3,8` (small for shift-mode pattern alignment),
  while VBus needs `=so3,128` (frame-anchored, full frame must fit).
  Generated descriptors include the right value automatically.
- **Modbus RTU coexists with bidirectional state machines** — the
  serial-reader thread gates `_process_modbus_rtu()` off when T510 or
  Kamstrup runners are active so they don't byte-walk-eat the stream.

## Files in this directory

| File                         | Role                                                    |
| ---------------------------- | ------------------------------------------------------- |
| `sml_emulator.html`          | Browser UI + per-protocol wire-frame builders (in JS)   |
| `sml_emulator_server.py`     | Python bridge (HTTP server + pyserial + state machines) |
| `SML_Emulate.zip`            | Pre-built distribution for macOS / Linux / Windows      |
| `README.md`                  | This file                                               |

## See also

- `../udp_monitor/` — companion tool that shows the UDP multicast
  variables Tasmota devices broadcast on `239.255.255.250:1999`.
- `../../examples/` — TinyC scripts including SML-driven ones
  (`sml_ebus.tc`).
- `../../TinyC_Reference.md` — the smart-meter descriptor syntax used
  in `>M 1` blocks.
