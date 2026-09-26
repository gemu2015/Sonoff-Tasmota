# TinyC Changelog

All notable changes to TinyC — firmware (`xdrv_124_tinyc*`), bytecode format and
the browser IDE — newest first. Test builds are published as the
[`testing` release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing).

**Updating:** the IDE lives in the device filesystem (`/tinyc_ide.html.gz`) and is
**not** replaced by flashing firmware. After a firmware update run `TinyCIde` in the
console (or press "Update IDE" on `/tc`) and hard-reload the IDE page.

**Markers:** ⚠️ = behaviour change, compatibility note or important fix.
"ABI n" = syscall ABI revision, see the table below.

## Syscall ABI

Every `.tcb` carries the syscall ABI revision (`abi_rev`) it was compiled for. Since
ABI 21 (1.6.47) firmware **refuses** a `.tcb` with a newer `abi_rev` instead of
failing later on an unknown syscall or opcode. Since 1.6.49 the IDE compiles for the
ABI of the connected device: missing fusions are left out, a missing syscall aborts
compilation with its name. Most steps are pure appends — existing `.tcb` files keep
running.

| ABI | Firmware | Change |
|---|---|---|
| 32 | 1.6.68 | FTP client (syscalls 556–566) |
| 31 | 1.6.67 | `serialReadArray` (555) |
| 30 | 1.6.67 | USB host for FTDI adapters, `usb*` (546–554) |
| 29 | 1.6.66 | opcode `LLK_OP2_ST` (0xC2) |
| 28 | 1.6.64 | eleven syscalls accept packed `byte[]` correctly (stamped only when a `byte[]` is passed) |
| 27 | 1.6.62 | packed `int16[]` / `uint16[]`, opcodes 0xB6–0xC1 |
| 26 | 1.6.61 | `httpGet`, `httpPost`, `smlGetStr`, `tasmCmd` accept packed `byte[]` correctly (stamped only when a `byte[]` is passed) |
| 25 | 1.6.60 | `WebChartQ` (545); `WebChart` on a `byte[]` |
| 24 | 1.6.50 | `mqttPublish` with runtime strings and log level (544) |
| 21–23 | 1.6.47 | superinstructions 0xB0–0xB5; firmware refuses newer bytecode from here on |
| 20 | 1.6.46 | BLE "SPP" persistent GATT connection (535–543) |
| 19 | 1.6.46 | `lvglChartUpdateMode` (534) |
| 17–18 | 1.6.46 | Bluetooth Classic SPP (524–533) |
| 16 | 1.6.40 | `webCard` (521) |
| 15 | ≤ 1.6.39 | LVGL polyline, arc background/style, rotate (517–520) |
| 14 | ≤ 1.6.39 | LVGL canvas from PSRAM image slots (514–516) |
| 13 | ≤ 1.6.39 | `audioMicGain` (513) |
| 12 | ≤ 1.6.39 | `rsaEncrypt` (512) |
| 11 | ≤ 1.6.39 | `utcSecs` (511) |
| 10 | ≤ 1.6.39 | raw TLS client `tls*` (503–509), `base64Enc` (510) |
| 9 | ≤ 1.6.39 | `i2sDuplexBegin` (502) |
| 8 | ≤ 1.6.39 | ⚠️ `i2sBegin` gained a leading `mclk` argument (not a pure append) |
| 7 | ≤ 1.6.39 | I2S microphone (498–501) |
| 6 | ≤ 1.6.39 | LVGL lines (495–497) |
| 5 | ≤ 1.6.39 | `lvglImageScale` (494) |
| 4 | ≤ 1.6.39 | `lvglSetFont` (493) |
| 3 | ≤ 1.6.39 | `touchGet` (492) |
| 2 | ≤ 1.6.39 | `fcall` float BLIB call (371) |

The bytecode **format** is separate from the ABI: format v6 (self-describing header
with total size and `abi_rev`) arrived in 1.6.35; v2–v5 files still load.

---

## 1.6.68 — 2026-09-22

- **FTP client** (ABI 32, syscalls 556–566) — one session per device: `ftpOpen(host, user, pw)` / `ftpClose()`; `ftpPut(remote, local, mode)` sends a file, `ftpPutStr(remote, data, mode)` a `char[]`/`byte[]` buffer (`mode` 0 = replace, 1 = append); `ftpGet` / `ftpGetStr` fetch into a file or buffer; `ftpList`, `ftpSize`, `ftpDelete`, `ftpMkdir`, `ftpRename`. A dropped idle link is reopened on the next call; every step is bounded. Tested against a FRITZ!Box 7590 and `utils/ftp_testserver.py`. Examples: `examples/ftp_log.tc`, `examples/ftp_browse.tc`. Use the box's IP rather than `fritz.box` (over IPv6 the box refuses PASV).
- ⚠️ **Deadlock fixed:** `httpGet`, `httpPost`, `mailSend` (and the FTP calls) called from `main()` in a script that also has a `TaskLoop()` blocked the device for good (web and syslog dead until power cycle). All four sites now check the mutex owner.
- Compiler: `usb*` and `serialReadArray` now stamp the correct ABI (was 26), so older firmware refuses such a `.tcb` with the ABI message.
- `ftpList` on an empty directory returns 0 (FRITZ!Box answers "550 No files found").
- New chapter "FTP Client" in both references.

## 1.6.67 — 2026-09-17

Catch-up test build: the GitHub testing release had stayed at 1.6.59 while the firmware reached 1.6.66. This build ships all of it (see below) plus:

- **ESP32 as USB host** for an FTDI serial adapter (ABI 30; S3/S2, `USE_TINYC_USBSERIAL`, custom build): `usbInit`, `usbState`, `usbOpen`, `usbAvailable`, `usbRead`, `usbWrite`, `usbClose`, `usbDeinit`, `usbInfo`. Measured 22.3 kB/s = 97 % of 230400 Bd.
- **`serialReadArray(h, buf, max)`** (ABI 31): a whole block from a serial port in one syscall. `serialWriteBytes` no longer drops everything silently when `len > 256`.
- **Fix:** unloading one slot tore down the TCP server of another slot.
- Two serial locks: closing can no longer hang; a pin cannot be assigned twice.
- Repository selection by drop-down on `/tc` (one base URL serves bytecode, examples, IDE self-update and `/tcrepo`).
- Webcam: motion detection by pixel difference (`camControl 16/17`), person detection (`camControl 21/22`, ESP-DL, S3 custom build), several camera fixes.
- `mdnsRegister()` no longer crashes when Tasmota's mDNS is off.
- ⚠️ ABI 31: a `.tcb` using `usb*` or `serialReadArray` is refused by older firmware — update firmware **and** IDE together.

## 1.6.66 — 2026-09-03

- One more superinstruction, `LLK_OP2_ST` (0xC2), for `x = y OP (z OP k)` — the integer benchmark loop is down from 13 to 8 opcodes. Verified against the unfused form on 5,600 cases.
- Superinstructions now report the error position, and `% 0` says "Modulo by zero".

## 1.6.65 — 2026-08-30

- **The `sprintf` `%s` family no longer truncates silently** at 255 characters — it logs `sprintf: %s cut at 255 of N chars -- use strcat() for long strings`. The limit itself stays.
- **`mdnsRegister()` removes old services first** — a script switching what it emulates (EcoTracker ↔ Shelly) no longer advertises both. Tasmota's own `_http._tcp` advert is restored.

## 1.6.64 — 2026-08-27

- ⚠️ **Eleven more syscalls wrote int32 slots into a packed `byte[]`** (up to four times past its end): `webArg`, `webParse`, `jsonStr`, `fileReadDir`, `fileGetStr`, `smlRead`, `pluginQuery`, `tlsReadLine`, `tlsRead`, `pwlStr`, `sppScan`. The compiler stamps ABI 28 when a `byte[]` reaches one of them.
- Program pickers show a plain name and info link from `// @name:` / `// @info:` in the source.
- A kept-alive HTTP connection can no longer block the web server for hours.

## 1.6.63 — 2026-08-26

- ⚠️ **`webText()` writes back again** for buffers larger than 16 elements — it used to overwrite the script's first global variables instead.
- A string literal may be passed to a `byte[]` parameter.
- Syntax highlighters know `do`, `struct`, `enum`, `typedef`, `const`, `static`, `persist`, `watch`, `global`, `byte`, `int16`, `uint16`.

## 1.6.62 — 2026-08-26

- **Packed 16-bit arrays:** `int16[]` / `uint16[]` (aliases `int16_t`/`short`, `uint16_t`/`ushort`) — half the RAM of `int[]`, usable everywhere `int[]` is (locals, globals, heap, struct fields, parameters, `persist`).
- Fixes: `sortArray` on packed arrays; disassembler entries for the byte opcodes.

## 1.6.61 — 2026-08-25

- ⚠️ **Six `byte[]` write paths fixed** (`httpGet`, `httpPost`, `smlGetStr`, `tasmCmd`, `fileWrite`, `fileRead`) — they wrote past the end of a packed array (ABI 26 when a `byte[]` is passed).
- The SML examples' text buffers moved to `byte[]` (up to −53 % RAM).
- Intraday column charts label the x axis by time span instead of point count.

## 1.6.60 — 2026-08-25

- **Chart memory:** `WebChart` accepts a packed `byte[]`; **`WebChartQ(scale, offset)`** (syscall 545) maps raw 0..255 values for the next series. Chart data on the wire is about 60 % smaller.
- A `WebChart` on a `byte[]` stamps ABI 25 (older firmware would read garbage).

## 1.6.59 — 2026-08-24

- Documentation release; firmware identical to 1.6.58. Packed byte arrays documented; the German reference caught up with the English one; all 453 builtins documented in both.

## 1.6.58 — 2026-08-24

- **Packed `byte[]` arrays complete:** all string functions (`strcpy`, `strcat`, `sprintf`, `strToken`, `strSub`, `strReplace`, `strlen`, `strcmp`, `strFind` …), `%s`, `webSend`, `mqttPublish`, `persist byte[]`, byte fields in structs. `byte[]` is a full drop-in for `char[]` at a quarter of the RAM. 62 on-device self-tests (`examples/byte_array_suite.tc`).
- Charts default to card width; CSV export button.
- **`/tcrepo`:** install an example from the repo onto the device without the IDE on the device.
- More robust uploads (self-retry, clean failure on low heap).
- `mqttPublish` with runtime topic and silent publishing; TLS fails cleanly on handshake errors.
- Two fork-local options: `MQTT_RETRY_CEILING_SECS` and `WIFI_RETRY_RESTART_COUNT`.

## 1.6.57 — 2026-08-24

- Byte-oriented syscalls accept `byte[]` (crypto, hex/base64, binary file I/O, TCP/UDP, serial, I²C, TWAI).

## 1.6.56 — 2026-08-24

- **Packed byte arrays:** `byte buf[1024]` takes 1024 bytes instead of 4096 (eight new opcodes 0xA8–0xAF). `byte` is a type name, not a keyword. Details in `docs/BYTE_ARRAYS.md`.

## 1.6.55 — 2026-08-19

- Chart layout: charts, buttons and tables share one centred width that fits phones (`class='tcc'` for scripts with their own Google Charts divs).

## 1.6.54 — 2026-08-19

- Charts without `WebChartSize` fill the card width instead of a fixed 960 px; `WebChartSize(0, h)` requests card width explicitly.
- Main-page buttons from `webPageLabel()` disappear when the slot stops (mi-hol, discussion #118).
- Console button to **unload** a slot (frees bytecode and VM memory).
- The IDE's `#include` resolver falls back to the repo (`examples/`, then `examples/common/`).
- **`/tcrepo`** page served from flash (`USE_TINYC_REPO_IDE`).

## 1.6.53 — 2026-08-18

- **Fix:** unloading a slot did not free the VM heap (leaked once per upload until reboot).

## 1.6.52 — 2026-08-18

- An upload whose loader runs out of memory retries by itself, loading from the file.

## 1.6.51 — 2026-08-18

- ⚠️ **MQTT syscalls were compiled out of every build** (gated on the long-gone `USE_MQTT`); `mqttPublish` always returned −1. Fixed.
- New error `TC_ERR_OUT_OF_MEMORY` instead of a misleading "Stack overflow".
- An upload unloads the target slot immediately.

## 1.6.50 — 2026-08-18

- **`mqttPublish(topic, value, level)`** with runtime strings and a log level (ABI 24, syscall 544) — level 0 publishes silently.
- `WebChart` state is reset for every response, including `webOn` handlers.

## 1.6.49 — 2026-08-10

- **The IDE compiles for the connected device's ABI:** fusions the device does not know are left out (still correct, just slower); a syscall the device lacks aborts compilation with its name.

## 1.6.48 — 2026-08-09

- **IDE hotfix:** the 1.6.47 IDE crashed while compiling almost any script (a typo in the disassembler case for the new superinstructions). Reported by mi-hol (#115). Firmware and bytecode unchanged; only the IDE needs updating (`TinyCIde`).

## 1.6.47 — 2026-08-09

- **Faster VM on ESP32:** direct-threaded dispatch (2.4–3× per opcode), reused frame locals (a call costs 2.8 µs instead of 11.8 µs) and six superinstructions for the most common patterns (`i++`, `x = y OP z`, loop heads). Overall benchmark 3353 → 2433 ms (Berry on the same chip: 3592 ms).
- **Fix:** raw web responses (Scripter-style `won` handlers with their own headers) were closed before sending anything unless the Tasmota web UI happened to be open — the 8 s send budget is now anchored per request.
- `TC_HEAPLOG` also builds on ESP32 (only with `-DTINYC_HEAP_DEBUG`).
- ⚠️ **ABI 20 → 23:** newly compiled `.tcb` files do not run on older firmware. Since ABI 21 the loader **refuses** a `.tcb` with a newer ABI instead of failing later on an unknown opcode. Flash first, then update the IDE, then recompile.

## 1.6.46 — 2026-08-08

- **Fix:** `WebCall` / `WebPage` / `JsonCall` skipped a slot whose `TaskLoop` was busy — the script's whole block was missing from about half of all page loads. Page rendering now waits for a usable window (at most 400 ms per page); remaining skips are counted (`WebSkip` in the `TinyC` console output).
- **Compiler:** `#if NAME` evaluates the macro's value (`#define SIM 0` / `#if SIM` was true).
- **BLE "SPP"** — a persistent GATT connection (ABI 20): `bleSppTarget`, `bleSppConnect`, `bleSppState`, `bleSppSub`, `bleSppAvailable`, `bleSppRead`, `bleSppWrite` (split at the ATT MTU), `bleSppClose`, `bleGattDump`. 128-bit UUIDs as strings. Needs `USE_TINYC_BLE`.
- **Bluetooth Classic SPP** (ABI 17/18): `sppInit` … `sppScan`, `sppDeinit` — classic ESP32 only, with `USE_TINYC_SPP`.
- `lvglChartUpdateMode` (ABI 19): circular chart updates redraw one column instead of the whole chart.
- ⚠️ ABI 16 → 20. Existing `.tcb` files keep running.

## 1.6.45 — 2026-08-01

- **Three silent failures fixed:**
  - A string literal passed through a `char[]` function parameter produced no output in `webSend`, `responseAppend`, `mailBody` and the other streaming calls.
  - `tcbtn()` lost a button's inner HTML and inline colour after the first click.
  - Compiler: a string ternary as an argument (`f(c ? "a" : "b")`) compiled but did nothing at run time (about 79 syscalls affected). The call is now split into two complete calls — this part works with the new IDE even on older firmware.

## 1.6.44 — 2026-07-26

- **`fileRename(from, to)`** (syscalls 522/523): never overwrites an existing target and refuses to move between file systems.
- Browser simulator fix: it died with a `ReferenceError` on the first syscall without an earlier numeric case (e.g. any program using `fileWrite`).
- ⚠️ The IDE lives in the device filesystem (`/tinyc_ide.html.gz`) and is **not** replaced by a firmware flash — run `TinyCIde` in the console after updating.

## 1.6.43 — 2026-07-13

- **Persist overhaul:** `.pvs` entries are keyed by name (PV3). Adding, reordering or resizing persist variables no longer wipes saved state; old files upgrade automatically. Cap raised 64 → 128 on ESP32 with a loud error when exceeded (was silent truncation). Persisted **arrays** now really restore across reload and reboot.
- `spawnTask` workers run on their own VM context (default on ESP32; opt out with `USE_TINYC_NO_WORKER_VM`) — fixes frame corruption under concurrent network I/O.
- **Fixes:** a cross-slot file-handle race; large `/ufsu` uploads (moved off the loop task); download/upload and camera servers start on Ethernet-only devices; aborted uploads delete the partial file.
- Multi-series `WebChart` legend on top; `TC_LV_MAX` 128 → 256.

## 1.6.42 — 2026-06-27

- ⚠️ **Heap-corruption fix:** the idle shrink of the VM heap could move it while live absolute pointers referenced it (use-after-free, boot loops). Disabled.
- Callback argument buffers are reserved per slot at load time ("Event callback heap alloc failed" after hours of uptime).
- **ESP8266 runs again** (hardware watchdog in output programs fixed).
- Control-panel scripts render inline on the main page; a bare `WebUI()` no longer adds an empty "TinyC UI" button.

## 1.6.41 — 2026-06-25

- Repo index and `.tcb` downloads on `/tc` moved to the browser (a synchronous TLS fetch on the loop task wedged the device for minutes).
- A slow client can no longer wedge the web server permanently (self-recovering ~8 s cap).
- Deferred commands (`tasmDefer`, `audioPlay`, TTS, `sendmail`) now run from event-driven slots.
- LVGL on renderer-based displays (RA8876); `WebChart` puts the newest sample at the right edge; several crash fixes (Scripter `?sv=`, `freeUart` on C3/C6).

## 1.6.40 — 2026-06-23

- **`webCard(n)`** (syscall 521, ABI 16): each script's main-page output gets its own frame; `webCard(0)` opts out.
- Guard against a TLS call before WiFi is up; widget writes routed to the rendering slot.

## 1.6.39 — 2026-06-19

- Roll-up test release: main-loop wedge fix for `httpGet` / `httpPost` / `sendMail`, ~120 KB internal RAM back on S3 + PSRAM (share table), bounded connects to dead IPs, raw TLS client, `rsaEncrypt()`, `utcSecs()`, audio (full-duplex I2S, WAV player, microphone), on-device LVGL GUI, P4 MIPI camera, BLE GATT, IDE/firmware ABI mismatch warning. ESP8266 custom builds run basic programs again.

## 1.6.38 — 2026-06-05

- **Two-phase autoexec boot:** every slot's `main()` runs before any `TaskLoop` starts, so late slots are no longer starved before they reach `addCommand()`.

## 1.6.37 — 2026-06-04

- The registered command prefix is stored in `/tinyc.cfg` and restored even when `main()` did not reach `addCommand()`; autoexec loads slots one after another.

## 1.6.36 — 2026-06-04

- The command prefix survives a worker stop/resume; a per-second self-heal re-registers it ("commands return Unknown until slot restart").

## 1.6.35 — 2026-06-04

- **`.tcb` format v6:** self-describing header with total size and syscall-ABI revision. The loader detects truncated uploads and warns when a script was compiled with a stale IDE. v2–v5 files still load; v6 files are rejected by older firmware.

## 1.6.34 — 2026-06-03

- A TinyC command whose slot is busy answers `{"Command":"Busy"}` instead of `Unknown`.

## 1.6.33 — 2026-06-02

- **Compiler fix:** a float variable whose name matched a built-in `#define` (`r`, `w`, `a`, colours, `MATTER_*` …) was printed as garbage by `sprintf("%f")` / `addLog`.
- New example `sml_custom_line.tc`.

## 1.6.32 — 2026-06-01

- **`fastMux(flag, time, buf, len)`** (syscall 427): hardware-timer GPIO multiplexer for LED matrices and 7-segment displays (port of Scripter's `ESP32_FAST_MUX`; classic ESP32 and S3, `USE_TINYC_FAST_MUX`).

## 1.6.31 — 2026-05-31

- **Custom HTML controls:** `varIdx(var)` (424) returns a global's index for hand-built `<button>`/`<input>` elements; `webButtonV` / `webSliderV` (425/426) take runtime labels.

## 1.6.30 — 2026-05-31

- **JSON on any buffer:** `jsonNum(buf, "a#b#c")` and `jsonStr(buf, "a#b#c", dst)` (syscalls 422/423), e.g. on an `httpGet` response.

## 1.6.29 — 2026-05-31

- **`httpGet` reads the body itself** (with de-chunking) and retries up to 3 times — fixes empty bodies from slow ESP8266 peers on busy devices. POST is not retried.

## 1.6.28 — 2026-05-31

- **IDE self-update:** console command `TinyCIde [url]` and an "Update IDE" button on `/tc` replace `/tinyc_ide.html.gz` from the repo.

## 1.6.27 — 2026-05-30

- **Matter with Alexa:** lights and plugs commission and work as direct nodes. Alexa rejects a node that mixes actuators and sensors — split them into two nodes (the mixed bridge still works with Apple and Google).

## 1.6.26 — 2026-05-28

- **Matter:** Google Nest pairing fixed (IM TimedRequest is answered).

## 1.6.25 — 2026-05-27

- **Matter:** mandatory `SAT` added to the operational mDNS record (Google Nest failed after CASE).

## 1.6.24 — 2026-05-27

- **Matter:** hourly reboot of a large bridge on ESP32-C6 fixed (per-packet log in the UDP callback overflowed the AsyncTCP stack).

## 1.6.23 — 2026-05-26

- **Fix:** a slot writing 60 `shareSetFloat`/s could stall the HTTP server for minutes on a single-core C3. Share readers now use a 50 ms try-lock (default value on timeout), `FUNC_COMMAND` a 200 ms deadline instead of waiting forever.

## 1.6.22 — 2026-05-26

- `WebChart` tooltips and axis labels use the language's decimal separator (`D_DECIMAL_SEPARATOR`); the SML chart editor is locale-aware.

## 1.6.21 — 2026-05-26

- New route **`/cedit`** serves the SML chart editor from the device filesystem, so it can load and save `/sml_chart.bin` in place. Requested by mi-hol (discussion #83).

## 1.6.20 — 2026-05-25

- **Fix:** a polling `spawnTask` worker exited silently at its first `delay()` when UDP globals were injected into the VM it had borrowed ("no values fetched" with `PWL_DIRECT_GLOBALS`).

## 1.6.19 — 2026-05-25

- **Fix:** UDP-global reception blocked the loop task for the whole duration of a TLS request in a `spawnTask` worker, which corrupted the heap (Exception 29 in LwIP). Reception now uses a non-blocking try-lock and drops the value when the VM is busy.

## 1.6.18 — 2026-05-25

- ⚠️ **`fileWriteArray` / `fileReadArray` store floats as readable tab-separated text** (like Scripter's `fwa`/`fra`). New signature `fileWriteArray(arr, handle, count [, append [, decimals]])` / `fileReadArray(arr, handle [, count])` — `count` is now explicit. The integer variant is gone (use `fileWriteBin`/`fileReadBin`); arrays of any size work.

## 1.6.17 — 2026-05-25

- **Fix:** uploading a large file (≥ 16 KB) to internal flash via the file manager hard-hung a dual-core node running a VM. Running VMs are now stopped for the duration of the write and restarted afterwards.

## 1.6.16 — 2026-05-24

- ⚠️ **Matter commissioning fix** on newlib-nano builds (e.g. `tinyc32s3`, `tinyc32c3`): the 64-bit node id in the DNS-SD name came out as garbage, so controllers never connected.
- `matterSet` / `matterSetFloat` only send a report when the value actually changed.

## 1.6.15 — 2026-05-24

- **`matterName(ep, "label")`** (syscall 408): names individual Matter endpoints (the node becomes a bridge). Already paired nodes must be removed and re-added to pick up the names.

## 1.6.14 — 2026-05-19

- ⚠️ **DMX now uses RMT instead of a UART:** `dmxInit(gpio)` takes one argument; all UARTs stay free for SML, Modbus and serial.

## 1.6.13 — 2026-05-18

- `dmxInit(uart, rx, tx)` (syscall 397) — replaced by the RMT version in 1.6.14.

## 1.6.12 — 2026-05-18

- **`dmxWrite(channel, value)`** (syscall 396): TX-only DMX512, e.g. for surplus-power heater dimmers; refreshed at ~20 Hz, all channels to zero after 30 s without writes.

## 1.6.11 — 2026-05-18

- **Fix:** a commanded `Restart` or OTA was mistaken for a boot loop and disabled autoexec. Crash loops (panic, watchdog, brownout) are still detected.

## 1.6.10 — 2026-05-17

- WebOn / WebUI pages no longer return a spurious 503 "TinyC not ready" while a slot is briefly busy (waits up to `TC_WEBON_HALTED_WAIT_MS`, default 1.5 s).
- **Persist safety net:** a `.pvs` file that no longer matches the program layout is renamed to `.pvs.bak` instead of being deleted.

## 1.6.9 — 2026-05-16

- `responseCmnd(buf)` no longer truncates silently at 255 characters: the limit is `TC_RESPONSE_MAX` (ESP32 512, ESP8266 256) and truncation is logged.

## 1.6.8 — 2026-05-15

- **Fix:** WebUI widget writes (checkbox, button, slider) went to slot 0 even when the page belonged to another slot.

## 1.6.7 — 2026-05-15

- `webButton` is now a momentary action button (optional `"Idle|Active"` confirmation text); new **`webToggle(var, "On|Off")`** (syscall 394) for a latching on/off button.

## 1.6.6 — 2026-05-15

- ⚠️ **`udp(10, mcast_ip)` fixed:** leaving a multicast group now really stops the packets (socket-level drop membership, then plain unicast on the same port). Works on ESP32 and ESP8266.
- The share table is placed in PSRAM when available.
- `shareSet*` failures (table full, string too long) are logged with a hint instead of being dropped silently.

## 1.6.5 — 2026-05-14

- ESP8266 build fix for `udp(10, …)` (superseded by 1.6.6).

## 1.6.4 — 2026-05-14

- The main-phase mutex change from 1.6.3 was **rolled back**: it caused Modbus-TCP disconnect storms in production.

## 1.6.3 — 2026-05-13

- `main()` executed under the slot's VM mutex (rolled back in 1.6.4).

## 1.6.2 — 2026-05-13

- **`shareDump()`** (syscall 352): logs every live share-table entry.

## 1.6.1 — 2026-05-13

- **`udp(10, mcast_ip)`**: leave a multicast group (fixed in 1.6.6).

## 1.6.0 — 2026-05-08

- **`tcpTransact(req, req_len, resp, resp_max, timeout_ms)`** (syscall 351): write a request and wait for the reply in one call — Modbus-TCP helpers in `examples/modbus_lib.tc` use it.

## 1.5.3 — 2026-05-08

- **Binary libraries (BLIB):** plugins of type `MODULE_TYPE_BLIB` export native functions that scripts call with `bcall("name", buf, len)` (syscall 370). Reference plugin with `mb_crc16`, `crc32`, `crc8_dallas`; example `examples/blib_crc_demo.tc`.

## 1.5.2 — 2026-05-07

- **`#include "file.tc"`** in the IDE compiler: includes are fetched from the device filesystem, recursively, each file only once. Firmware unchanged. Example library: `examples/modbus_lib.tc`.

## 1.5.1 — 2026-05-07

- **TCP client tuning** (syscalls 348–350): `tcpKeepalive(idle, intvl, count)` (fixes SMA Tripower / SolarEdge idle disconnects after 60 s), `tcpNoDelay(on)`, `tcpDisconnectReason()` (0..5: never, connected, peer closed, timeout, network, user closed).
- `TC_TCP_CLI_SLOTS` raised 4 → 8 (override with `-DTC_TCP_CLI_SLOTS=N`).
- ⚠️ **Fix:** the AES/HMAC/SHA/hex buffers from 1.3.20 were switch-case locals in `tc_vm_step`, which inflated every callback's stack by ~5 KB and overflowed into the heap (crashes in WiFi RX). They are now heap-allocated.
- ⚠️ **Fix:** multi-slot deadlock on startup / first nav button — the halted pre-check before the JsonCall/WebCall fan-out is restored.

## 1.5.0 — 2026-05-06

- **String operations** (syscalls 302–308), in place on `char[]` buffers: `strReplace`, `strStartsWith`, `strEndsWith`, `strContains` (literal needles), `strToUpper`, `strToLower`, `strTrim`.

## 1.4.3 — 2026-05-06

- **Reference parameters:** `void swap(int& a, int& b)` — scalar pass-by-reference for locals and globals, including compound assignment. No new opcodes (reuses the array-reference machinery).

## 1.4.2 — 2026-05-06

- **Function pointers as struct fields:** `struct CmdEntry { char name[12]; cmd_handler handler; }` and `cmds[i].handler(args)` — dispatch tables.

## 1.4.1 — 2026-05-06

- **Function pointers:** `typedef int (*cmp_fn)(int, int);`, assign a named function, call indirectly; works for locals, globals and parameters. New opcode `OP_CALL_INDIRECT` (0x56).

## 1.4.0 — 2026-05-06

- **Structs by value:** `struct Tag { int x; float y; char name[16]; }` — locals, globals, `persist`, positional initializers, arrays of structs, nested structs, whole-struct assignment, structs as parameters and return values, `sizeof(Tag)`. Compiler-only; the VM is unchanged.
- Known limitation: the persist layout hash does not include field names — reordering fields does not invalidate an existing `.pvs`.

## 1.3.38 — 2026-05-05

- **2D arrays:** `char/int/float buf[N][M]`, element access, row passing to `char[]`/`int[]`/`float[]` parameters, `sprintf("%s", buf[i])`, `strcpy`/`strcat`/`strcmp` on rows. Compiler-only.

## 1.3.37 — 2026-05-04

- **Binary array file I/O** (syscalls 286/287): `fileReadBin(handle, arr, count)` / `fileWriteBin(handle, arr, count)` — e.g. chart history that survives a `persist` layout change.

## 1.3.31 – 1.3.36 — 2026-05-03

- **New `BootInit()` callback** (1.3.31): runs once per VM run after `main()` returns and after all Tasmota drivers are initialized — the place for `serialBegin`, `i2cBegin`, `spiBegin` (equivalent of Scripter's `>BS`). 1.3.32–1.3.34 fixed its gating (per slot, works with `TaskLoop`, uptime gate).
- **Autoexec start moved** from `FUNC_INIT` to the first `FUNC_LOOP` (1.3.35) and additionally waits for `TC_AUTOEXEC_MIN_UPTIME` (default 3 s, 1.3.36). `serialBegin` in `main()` no longer receives zero bytes after a cold boot — the old `delay(15000)` workaround is no longer needed.

## 1.3.23 – 1.3.30 — 2026-05-01 … 2026-05-03

- **`pwlRequest` accepts a runtime `char[]`** (1.3.23): Powerwall credentials can be read from a config file instead of being compiled in.
- `pwlRequest("@R")` (1.3.27): resets the TLS session without a reboot.
- 1.3.24–1.3.29 tried several connect/handshake timeout caps; Tesla's switch to ECDSA certificates (4–5 s handshakes) made them abort valid connections. **1.3.30 returns to the Scripter pattern** (no extra caps); recovery is left to the script-side circuit breaker in `examples/powerwall.tc`.

## 1.3.22 — 2026-05-01

- **Fix:** `sprintf` printed `%%` literally when it stood next to a float format (`"85 %% (12.98 kWh)"`).

## 1.3.21 — 2026-05-01

- **UDP `global` send side heals itself:** a failed multicast `endPacket()` now re-binds the socket (throttled to every 5 s) instead of silencing all broadcasts of the slot until restart.

## 1.3.20 — 2026-04-26

- **Symmetric crypto** (syscalls 360–365, ESP32 via mbedtls): `aesEcb`, `aesCbc` (AES-128, in place), `hmacSha256`, `sha256`, `hex2bin`, `bin2hex`. First user: Tuya local protocol v3.3 (`examples/pool_pump.tc`).

## 1.3.19 — 2026-04-25

- **Cross-VM share table** (syscalls 340–347, `share*`): two slots exchange named scalars and strings (32 entries).
- `TC_MAX_PROGRAM` 64 KB → 128 KB; program and constants fall back to PSRAM when DRAM is short.
- **Compiler fixes:** syscall ids ≥ 256 were truncated to 8 bits in four places (`strcmp(arr, "literal")` returned NaN bits); float-returning builtins are now recognised from the symbol table (`sprintf("%.2f", shareGetFloat(…))`).

## 1.3.18 — 2026-04-25

- Constant pool cap raised 512 → 1024 on ESP32 (`#ifndef`-gated).

## 1.3.17 — 2026-04-24

- **Diagnostics:** `TC_ERR_BOUNDS` now logs index, bound and PC; a stack-balance check on every return reports SP leaks at the first leaking frame; the compiler warns on implicit float → int narrowing.

## 1.3.14 – 1.3.16 — 2026-04-24

- **SML mini-scripter:** `spin`/`spinm` opcodes for IR reading heads, native `for … next` loops, and actionable error messages instead of "unknown statement". Coverage of ottelo's SML script collection rose to 103 of 104 meters.

## 1.3.13 — 2026-04-23

- **Fix:** callback dispatchers (Command, OnMqttData, TouchButton, HomeKitWrite) leaked one stack slot per call — stack overflow after ~240 calls. New `vmStackDepth()` (syscall 283).

## 1.1.1 — 2026-04-22

- Upload allocator honours the `?fsz=N` hint instead of always requesting 64 KB (fragmented heaps rejected uploads).
- Variadic `addLog` (compile-time only).

## 1.1.0 — 2026-03-16

- Unified `sprintf` / `sprintfAppend`; the VM pauses automatically during uploads.

## 1.0.0 — 2026-03-13

- First versioned release.

---

Versions 1.2.x and 1.3.0–1.3.12 predate the per-version notes in the firmware
header and are not listed individually.
