# SLCAN bridge ↔ SML emulator

How the desktop SML emulator gets CAN-bus support without a kernel-level
CAN driver: an ESP32-C3 running Tasmota+TinyC acts as a USB-attached
SLCAN bridge. The Mac talks ASCII-over-USB-CDC, the C3 translates to
real CAN frames via its built-in TWAI peripheral, the bus terminates at
the device under test (a separate ESP32 running Tasmota with
`USE_SML_CANBUS`).

## Pieces

| File | Role |
|---|---|
| `tasmota/tinyc/examples/slcan_bridge.tc` | TinyC script that runs on the bridge ESP32-C3. Implements the SLCAN ASCII protocol over UART1, drives TWAI for CAN. |
| `tasmota/include/xdrv_124_tinyc_vm.h` | Firmware-side TWAI syscalls (380..386) that `slcan_bridge.tc` (and any future TinyC CAN script) calls. |
| `tasmota/tinyc/patch_twai_v1.mjs` | Idempotent IDE patcher — adds `twaiBegin/twaiEnd/twaiAvailable/twaiRecv/twaiSend/twaiStatus/twaiFilter` to the TinyC compiler's BUILTINS table + simulator stubs. |
| `slcan_client.py` (this dir) | Minimal Python SLCAN client. No deps beyond `pyserial`. Background reader thread, `send()` / `recv()` API. |

## Hardware

- **ESP32-C3 dev board** (any flavour with USB-CDC + a free UART pair).
- **CAN transceiver** — SN65HVD230, TJA1051, MCP2562, or any 3.3 V-compatible
  part. The C3's TWAI peripheral provides protocol logic only; the
  transceiver is what drives the differential CAN_H / CAN_L signal at the
  proper voltage.
- **120 Ω termination** at *both* ends of the bus (bridge end and DUT end).
  Some transceiver breakouts have a switch / jumper for this.

Default bridge wiring (override in `slcan_bridge.tc` if your board differs):

```
ESP32-C3 GPIO 6  → transceiver TXD          (CAN_TX)
ESP32-C3 GPIO 7  ← transceiver RXD          (CAN_RX)
ESP32-C3 GPIO 21 → host UART RX             (TinyC `serialBegin(20, 21, …)`)
ESP32-C3 GPIO 20 ← host UART TX
                  (USB-CDC option: route through a USB-UART adapter)
transceiver CAN_H / CAN_L → bus, 120 Ω termination at both ends
```

## Bring-up sequence

```bash
# 1. Flash a tinyc32c3 build to the bridge ESP32-C3.
pio run -e tinyc32c3 -t upload

# 2. Upload the bridge script.
node tasmota/tinyc/tc_deploy.mjs \
     tasmota/tinyc/examples/slcan_bridge.tc \
     192.168.1.<bridge_ip>

# 3. Connect via USB. (Mac shows /dev/cu.usbmodem*)
ls /dev/cu.usbmodem*

# 4. Smoke-test the SLCAN protocol.
cd tasmota/tinyc/utils/sml_emulator
python slcan_client.py /dev/cu.usbmodem* --bitrate 250 --seconds 10
# → opens the channel, sniffs CAN frames for 10 s

# 5. Send a frame (29-bit ext, ID 0x18FF50E5, payload 5 bytes):
python slcan_client.py /dev/cu.usbmodem* --bitrate 250 --ext \
       --send '18FF50E5/0102030405'
```

## DUT setup

The device under test runs Tasmota with `USE_SML_CANBUS` enabled and a
descriptor for the meter you're emulating. Examples in ottelo9's repo:

- `Huawei R4850G2 Lipo Charger (CANBus).tas` — 125 kbit/s, 29-bit IDs
- `Sorel XHCC (CANBus).tas` — 250 kbit/s, 29-bit IDs
- `Sorel LTDC (CANBus).tas` — same family

Each descriptor has a `+1,…,C,<bufsize>,<bitrate-code>,<name>,…` header.
The bitrate code is a packed integer: `param % 100` is the bitrate index
(0=25k 1=50k 2=100k 3=125k 4=250k 5=500k 6=800k 7=1M), `param / 100` is
the RX queue length. Match this to the bridge's `S<n>` setting.

## Protocol reference (subset of Lawicel SLCAN)

| Command | Meaning | Bridge response |
|---|---|---|
| `S0`..`S8\r` | Set bitrate (10/20/50/100/125/250/500/800/1000 kbit/s; bridge maps S0/S1 to 25k/50k since TWAI lacks 10k/20k presets) | `\r` (ACK) or `\x07` (NACK) |
| `O\r` | Open channel — calls `twaiBegin()` | `\r` or `\x07` |
| `C\r` | Close channel — calls `twaiEnd()` | `\r` |
| `t<iii><L><DD…>\r` | Transmit standard 11-bit frame | `z\r` (sent) or `\x07` |
| `T<iiiiiiii><L><DD…>\r` | Transmit extended 29-bit frame | `Z\r` or `\x07` |
| `V\r` | Version query | `V1010\r` |
| `N\r` | Serial number | `N0001\r` |
| `F\r` | Status flags | `F<2-hex>\r` |
| `r<…>\r` / `R<…>\r` | RTR frames | accepted, not actually transmitted |

Received frames arrive on the input stream as `t<…>\r` / `T<…>\r` lines.

## Why hand-rolled (not python-can)?

`python-can` has a perfectly fine `slcan` backend
(`pip install python-can; bus = can.Bus(interface='slcan', …)`). For
ad-hoc bench work, that's the answer. The reason `slcan_client.py` exists
is so the SML emulator (which already runs on `pyserial` with no other
deps) can pick up CAN support without pulling in another package on
every contributor's setup. The two are interchangeable for diagnostic
scripting.

## Known limits

- **Bridge default uses UART1 for the host serial**, not USB-CDC.
  Reason: TinyC's `serialBegin` binds to a real UART; USB-CDC on C3 is
  the Tasmota console and isn't directly addressable from a script. If
  your C3 board only exposes USB-CDC, route the host through a USB-UART
  adapter (CP2102 / FT232) wired to GPIO 20/21, or extend
  `slcan_bridge.tc` to write to the console-output channel instead of
  a serial port.
- **No filter at run-time**: ESP-IDF's TWAI filter is part of driver
  install, not run-time mutable. `twaiFilter` is exposed but acts as a
  hint for the next `twaiBegin` (currently a no-op).
- **No bus-error recovery yet**: bridge counts errors but doesn't auto-
  restart on bus-off. If the controller goes bus-off, send `C\rO\r` to
  re-open. Useful as a manual recovery during debugging.
- **No DBC support**. The emulator's CAN profile mode (when added) does
  raw frame matching against descriptor patterns, same model the
  firmware-side SML driver uses. If you want a richer signal-decoding
  layer, plug python-can + cantools in beside this client.

## Status (2026-05-09)

- **Code is in place and builds clean** (firmware compile verified for
  `tinyc32c3` env; bridge script compiles to 2,120 bytes via
  `tc_deploy.mjs`).
- **Untested on hardware** — level converters not yet arrived. Expect
  bring-up tweaks (especially around USB-CDC vs UART1 host serial)
  once a physical bridge is wired.
- **SML emulator GUI integration deferred** — when hardware testing
  confirms the bridge protocol works, add a CAN profile mode to
  `sml_emulator.html` / `sml_emulator_server.py` that uses
  `slcan_client.py` to send frames matching the descriptor patterns.
