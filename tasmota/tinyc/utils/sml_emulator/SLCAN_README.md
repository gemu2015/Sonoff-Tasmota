# SLCAN bridge ↔ SML emulator

How the desktop SML emulator gets CAN-bus support without a kernel-level
CAN driver: an ESP32 running Tasmota+TinyC acts as a network SLCAN
bridge. The Mac talks SLCAN-ASCII over **TCP**, the bridge translates to
real CAN frames via its built-in TWAI peripheral, the bus terminates at
the device under test (a separate ESP32 running Tasmota with
`USE_SML_CANBUS`).

## REQUIREMENT: a dedicated CAN bridge device

CAN support is **not** a software-only feature and **not** a USB-serial
dongle. You must have, on the same CAN bus as the device-under-test:

1. **A separate ESP32 bridge** running Tasmota+TinyC with
   `tasmota/tinyc/examples/slcan_bridge_tcp.tc` (network/TCP variant —
   this is the one the emulator uses; the older USB-CDC `slcan_bridge.tc`
   is kept only for the loopback test). The emulator connects to the
   bridge's `host:port` (default `:9999`) over WiFi/LAN.
2. **A real CAN transceiver** on the bridge (SN65HVD230 / TJA1051 /
   MCP2562, 3.3 V). The TWAI peripheral is protocol-logic only — without
   a transceiver there is no differential bus and nothing will ACK.
3. **120 Ω termination at *both* ends** of the bus.
4. **Correct RX/TX pin wiring** on both the bridge and the DUT (see the
   pin map below — this is the #1 time-sink if wrong).

Without all four, the emulator's CAN profiles cannot reach the DUT.

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

## Loopback test (no transceiver needed)

Before the level converters arrive you can verify the protocol stack
end-to-end with just the ESP32-C3 and a single jumper wire:

1. Edit `tasmota/tinyc/examples/slcan_bridge.tc`:
   ```c
   int  twai_mode  = 1;          // ← change 0 to 1 (NO_ACK)
   ```
   Recompile + upload via `tc_deploy.mjs`.

2. Wire **GPIO 6 → GPIO 7** with a single jumper. No transceiver, no
   termination, no bus.

3. Run the loopback exerciser:

   ```bash
   cd tasmota/tinyc/utils/sml_emulator
   python slcan_loopback_test.py /dev/cu.usbmodem* --frames 50
   ```

   Expected: `✅ ALL PASS` — every transmitted frame round-trips back
   identically. The test covers std/ext IDs, DLCs 0..8, payload byte
   fidelity, and a few real-world-shaped frames (Sorel/Huawei IDs).

What this test covers (✓) and doesn't (✗):

```
✓ USB-CDC / UART RX+TX between Mac and bridge
✓ SLCAN ASCII command parser (S/O/C/t/T/V/F)
✓ TinyC twaiBegin / twaiSend / twaiRecv plumbing
✓ TWAI driver in NO_ACK mode (single-node operation)
✓ Frame format round-trip (IDs, DLCs, payloads)
✗ CAN transceiver electricals — no transceiver in the loop
✗ Differential bus signalling — wires are single-ended digital
✗ Bus arbitration with peer nodes
✗ Bus-off / error-recovery behaviour
```

The remaining ✗ items get exercised once the transceivers arrive and
a real DUT is on the bus. Switch `twai_mode` back to `0` (NORMAL) for
real-bus operation.

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
- **Bus-off recovery IS implemented** (TCP bridge): the bridge detects
  a run of consecutive `twaiSend` failures (the only reliable bus-off
  signal — `twaiStatus()` does *not* expose controller state) and
  reinstalls the TWAI driver in place, logging
  `TWAI bus-off -> reinstalled in place`. The emulator runner also
  self-heals across bridge restarts (EOF → reconnect < 1 s). No manual
  `C\rO\r` churn needed anymore.
- **No DBC support**. The emulator's CAN profile mode (when added) does
  raw frame matching against descriptor patterns, same model the
  firmware-side SML driver uses. If you want a richer signal-decoding
  layer, plug python-can + cantools in beside this client.

## Pin map (board-specific — get this right FIRST)

The TCP bridge's CAN pins are set at the top of
`slcan_bridge_tcp.tc` (`can_rx_pin` / `can_tx_pin`) and are
**board-specific**. Bench reference (2026-05-16/17):

| Device | Role | CAN RX | CAN TX |
|---|---|---|---|
| `.143` ESP32-C3 (bridge) | `slcan_bridge_tcp.tc` | GPIO 9 | GPIO 10 |
| `.39` ESP32-S3 (DUT) | `USE_SML_CANBUS` | GPIO 39 | GPIO 38 |

`RX` on one node wires to `TX` on the other through the transceiver
pair. **A swapped RX/TX is the single biggest red herring**: you get
intermittent per-frame ACK loss, the bridge bus-offs every cycle, and
the DUT decodes nothing — which looks *exactly* like a software
burst/timing/queue bug. The transport is robust; if the symptom is
"DUT ACKs some frames but never decodes, bus-off every cycle", **verify
the wiring/pin map before touching code**. A lower bitrate (Huawei
125 k) can mask a marginal mis-wire that a higher one (Sorel 250 k)
exposes.

## Status (2026-05-17) — PROVEN end-to-end

- **Hardware-verified, both modes**, live on `.39` through the `.143`
  bridge:
  - Huawei R4850G2 — **poll/response**, 125 kbit/s: 10/10 frames ACKed
    per poll, sustained.
  - Sorel **LTDC** — **broadcast**, 250 kbit/s: decodes
    `{"CAN":{"S1":22,"S2":22,"S3":22,"S4":22,"R1":0,"R2":0,"R3":0}}`.
- **GUI integrated**: CAN profile mode in `sml_emulator.html` /
  `sml_emulator_server.py` (`proto 'c'`); enter the bridge `host:port`,
  **Connect CAN bridge** — no Start button (server-side runner).
- **Self-healing**: bridge reinstalls a bus-off TWAI in place; runner
  reconnects across bridge restarts; broadcast frames are spread across
  the period (real-device cadence, not a burst) so the DUT keeps up.
- Hardening commit: `35202886c` (bridge `client_connected`/RX-gate
  deadlock, dead bus-off-recovery code, frame spreading, EOF
  fast-reconnect, CAN-Start no-op). No firmware/ABI change.
