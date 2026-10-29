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

## 1.6.76 — 2026-10-28

- ⚠️ **`tcpSelect()` is now per VM, not one pointer for all slots** (Andreas, 29.10.2026). The selected outgoing TCP client used to live once in the driver state, so two slots that both talk TCP in their own tasks overwrote each other's selection: slot A's `tcpSelect(0)`, a wait for the reply, slot B's `tcpSelect(1)` in between, and A read from (or wrote to) B's connection. Now each VM (and each `spawnTask` worker) keeps its own selection. Behaviour change: a slot that never calls `tcpSelect` always uses client 0 (before it inherited whatever another slot had selected last). Test on an ESP32-S3: two slots, each writing 600 lines to its own client of a Mac server over 50 s: before 2 and 580 lines arrived (slot 0 nearly silent), after 598 and 597, no line on the wrong connection.
- **`TinyCLogName 1`: lines written with `addLog()` / `addLogLvl()` start with the program's `// @name:`** (mi-hol, #137). `TCC: [SML Chart Shelly] start …` instead of every script inventing its own identification string, so a log line can be linked to the running script (the same name the slot row and the repository list show). Off by default (a script that adds its own prefix would show both); saved in `/tinyc.cfg` as `_logname,1`, takes effect at once; build default `-DTINYC_LOG_NAME=1`. A program without `@name` stays unchanged. The prefix is read from the `.tcb` header in memory, so it costs nothing at load time.
- ⚠️ **Settings ID (`CFG_HOLDER`) is now 4617, the same as official Tasmota and ottelo's images** (mi-hol, #129). Our release builds carried 5002, ottelo's images 4617, so every switch between the two (and from official Tasmota) threw the saved settings away: default settings, Wi-Fi included, and a device that could not be reached any more. From this build on the settings are kept when you switch between them. **One last reset:** a device that runs a TinyC build up to 1.6.75 has the ID 5002 and starts with default settings when it gets this build for the first time (Wi-Fi included, open access point `sonoff-NNNN`, `http://192.168.4.1`), so make a backup first (`Backup Configuration`). The release text says so. The ID lives in the (untracked) `user_config_override.h` of the build tree, public builds from `my_user_config.h` always had 4617.

## 1.6.75 — 2026-10-27

- ⚠️ **Upstream merge: Tasmota 16.0.0.1 (arendst/development of 25.10.2026), Arduino core 3.3.12 / IDF 5.5.5, platform `espressif32 2026.09.50` (ESP8266: `2026.09.00`).** The fork now shares real Git history with upstream (merge base: the upstream state of 05.07.2026), so the next update is a plain `git merge arendst/development`. **Building needs PlatformIO Core ≥ 6.2.0** (6.1.19 reports "IncompatiblePlatform" with the new platform). 25 conflicts: the documents (`README.md`, `FIRMWARE.md`, `CHANGELOG.md`, `RELEASENOTES.md`, `TEMPLATES.md`, the PR template) keep our version, files we had deleted (`.clangd`, `.doc_for_ai/`, `.github/workflows/`) stay deleted, `sdkconfig.defaults` and `CMakeLists.txt` stay tracked (the Ethernet builds write them), the rest by hand: `tls_mini` (the EEBUS key block stays), `HttpClientLight.h` (`uint32_t` timeout from upstream + our TLS receive buffer), UDisplay (DSI `pushColors`: our rotation/byte-swap path with upstream's retry loop for the DMA2D semaphore), `xdrv_54_lvgl` (`start_lvgl()` returns `bool` like upstream), BLE (`xdrv_79`, `xsns_52`, `xsns_62`: upstream's target list plus `ESP32P4`; our GATT dump now asks `BLELogLevel[LOG_LEVEL_INFO]`, switch it on with `BLEDebug2 <loglevel>`, because upstream removed `BLEDebugMode`), `tasmota_lv_conf.h` (upstream's file with our three values: `LV_USE_SNAPSHOT 1`, `LV_DRAW_SW_SUPPORT_RGB565A8 1`, `LV_USE_FREETYPE 0`), `my_user_config.h`, `.gitignore`. ⚠️ `HandleUploadUFSLoop` (`/ufsu`) keeps our version WITHOUT upstream's new `WebAuthenticate()` check (this fork had dropped the privileged-access test there on purpose). Checked: every line this fork added since the merge base is still in the result (only the lines changed on purpose differ). Built: `tinyc32s3`, `tinyc32-4M`, `tasmota32p4-display`, `tinyc32s3-eth-guard`, `tinyc32s3-eth-steuve`, `dfrobot-ai-cam`; run on hardware: `tinyc32s3` on an ESP32-S3 (settings, WLAN, time kept, TinyC benchmark runs) and `dfrobot-ai-cam` on the DFR1154 (picture, script autostart, SD card, PicoTTS plugin speaks, restart). Not run on hardware: BLE, LVGL, the P4 display, a real Steuerbox. The EEBUS entries dated after 21.10. under 1.6.74 (Steuerbox v188 … v216, SteuVE v017 … v025, `tls_mini` per connection) are part of this release too.

## 1.6.74 — 2026-10-21

- **The repository drop-down now offers both sources by default** (mi-hol, #131). The default `TINYC_REPO_LIST` has our repository and ottelo's SmartMeter/PV/Emulator programs (the same two entries his images carry). Up to 1.6.73 it had one entry, and with one entry the `/tc` page shows no source drop-down: a device that was updated from ottelo's image lost the drop-down and his programs. Our repository stays preselected. Without flashing: a file `/tinyc_repos.cfg` with the two lines replaces the list (see the reference).
- ⚠️ **A program name of 27 or more characters could destroy the program (ESP32; Andreas, 15.10.2026).** The slot, `/tinyc.cfg`, the upload and the persist file each held the name in 32 bytes. The upload cut it to 31 characters and lost the `.tcb`, the persist file got no `.pvs` and ended up with the SAME path as the program, and the first `saveVars()` / slot stop wrote the persist data over the program (118 bytes became 14, then `Load failed: Invalid binary` after the next start); two long names with the same start also landed in one file. Now the four buffers are 64 bytes on the ESP32 (`TC_FILENAME_MAX`; ESP8266 stays at 32), an upload (or `TinyCRun`) with a name that still does not fit is **refused** with a clear message (HTTP 400, the slot keeps its program), and the persist name is always `<stem>.pvs` and can never be the program's own path. Reproduced on an ESP32-S3 (a program with a `persist` variable and names of 4, 26, 27 and 35 characters). Programs with a name of up to 26 characters were never affected.
- **Repository list, IDE list and slot rows show only the program NAME, no `(file)` any more** (agreed with ottelo, 17.10.2026; it undoes the "Name (file)" label of #128). The name says what the program is; the file name is the tooltip of a slot row. Without a name the file name is shown as before. The `/tc` page and the IDE list "Repo Examples" use the same label. If the repository's `index.json` has a `"version"` for the entry, the DATE follows the name in the repository list and in the IDE list, short and without the time (`SML Chart · 14.10.26`; ottelo's wish — the closed select is cut off on a phone); the full version is in the slot row.
- **Program version and an update button** (idea and format agreed with ottelo, 17.10.2026). A program can carry `// @version: YYYYMMDD_HHMM` (e.g. `20261017_1430`, set by hand when the program changes; date AND time, so two changes on one day differ). The compiler writes it as `.tcb` header TLV 3 (older firmware skips it), and `compile_examples.mjs` / ottelo's `build.html` write `"version"` into `index.json`. A slot row shows it behind the name as `17.10.26 14:30`, and when the repository's `index.json` lists the same file with a NEWER version the row gets a `🔄 17.10.26 14:30` button that asks and then runs "Download & Load" into the same slot (settings stay: the `.pvs` is name-keyed). "Newer" is a plain string compare, so a test build that is newer than the repository is never offered a downgrade; if one of the two versions is not in the `YYYYMMDD_HHMM` form (or the slot has none) it is "different" instead. No version in the repository, no button. Slot rows also show the size in whole kB now.
- **"Delete all" removes ALL `.tcb` files, not at most 16 per file system** (Andreas, 17.10.2026). It collected 16 names per pass and stopped; with more programs the rest stayed without a word. It now repeats until a pass finds nothing (measured on an ESP32-S3: 20 files before, 0 after).
- **The index file of a log (`.ind`) is found for paths with a dot in a folder name, and cannot overflow** (Andreas, 17.10.2026). `/sd/v1.2/log` became `/sd/v1.ind`, and a path without any dot got `.ind` appended with `strcat`, which overflowed the 128-byte buffer from 124 characters on. One helper (`tc_ind_path`) looks for the extension only behind the last `/` and appends with `strlcat`; both places (download, time filter) use it.
- **EEBUS (`USE_EEBUS_GUARD` / `USE_EEBUS_STEUVE`): `USE_MQTT_CLIENT_CERT` and a 24 KB stack are set automatically** (Andreas, 19.10.2026; confirmed by a forum user). Without `USE_MQTT_CLIENT_CERT` the TLS library never sends the device's own certificate and the HEMS aborts the handshake (`tls connect err=296`); with the 12 KB stack of the TinyC builds the device crashes while signing with ECDSA. `my_user_config.h` now sets both when one of the two drivers is defined. ⚠️ The EEBUS Ethernet images of the interim release `eebus-eth` built on 18.10. lack both: use the rebuilt ones.
- **SML over Modbus TCP: a late answer no longer lands in the wrong register** (Andreas, 18.10.2026). The transaction number was sent but never compared, so a meter that answered after the next request had been sent had its answer stored under the next register (a voltage as an energy counter). The answer is now decoded only when its transaction number matches the request; stale data is dropped before each request, and the MBAP length is no longer cut to 8 bits. Measured on four Shelly Pro 3EM over WLAN: 20 wrong values in 31 minutes before, 0 in 38 minutes after. Not tested: FC16 writes (`USE_BAT_CTRL`).
- **IDE update (`TinyCIde` / the Update IDE button) falls back to the flash file system when writing to the SD card is cut short, and says what really happened** (Andreas, 19.10.2026). On a fresh ESP32-S3-ETH with an almost empty SD card the update failed with "the partition is probably full" (the same text for every write error) and "The old IDE was kept" (added to every failure, even when there was no old IDE). Now: when the write to the user file system (the SD card) is cut short, the IDE is written to the flash file system instead (the `/ide` page looks there as well); "full" is only said when the free space really is too small, otherwise the message names the bytes on disk and the kB free; "The old IDE was kept" appears only when one is left (`"kept"` in the answer). ⚠️ The fallback itself is untested — it needs an SD card whose write is cut short; message and `kept` were checked on an ESP32-S3.
- **SML on the ESP8266: the serial config of a meter line (`m2`, `mN2`, `mE1` …) now reaches the software serial** (Tasmota #7004, @RaphaelSquair, 20.10.2026). The software-serial start was `meter_ss->begin(baud)`, so the `smode` the driver computed was dropped and software serial always ran 8N1; only the hardware UART got it. Now `begin(baud, smode)` is used: 8N2 for Modbus works on software serial. TasmotaSerial has no real parity: 8E1/8O1 are emulated with an extra stop bit, so a meter on software serial that used to "work" as 8N1 with such a setting may behave differently. Compiled for the ESP8266 (SML forced on, `SML_Decode` in the image); not run on a device.
- **`tasmDefer()` says so when it drops a command** (Andreas, 20.10.2026). There is only ONE waiting place for all slots, and every further call before the next 50 ms tick was dropped without a sign. It now writes an INFO line `TCC: deferred command dropped (one is still waiting): <cmd>`; reference and `CLAUDE.md` (anti-patterns 26 and 27) say so, and that a nested `tasmCmd()` inside `Command()` overwrites the command's own answer. The example `steuve` is v006 (`tasmDefer` + at most one message per second).
- **A new version of a script keeps its saved values (`.pvs`) when it comes under a new file name** (Andreas, 20.10.2026). The IDE names a locally loaded file `slot<N>-<name>`, so every new version (`slot0-steuve_v005.tcb` → `…_v006.tcb`) got a NEW, empty `.pvs` and all settings seemed lost. When a slot loads a file, the new one has no `.pvs` yet, the previous file of that slot has one, and BOTH programs carry the same `// @name:`, the old `.pvs` is copied as the starting point (the old one stays; log line `TCC: persist values taken over from …`). That is safe because a `.pvs` stores its values by variable name (PV3): new variables keep their default, dropped ones are ignored. A different `@name` never inherits. Tested on an ESP32-S3: v1 run twice (counter 2), v2 with the same `@name` starts at 3, a program with another `@name` starts at 1.
- **SML + Domoticz (`USE_DOMOTICZ`): the values are picked by UNIT, not by position** (ottelo, 20.10.2026). The hook at the end of `SML_Show()` took value lines 1..4 as energy, power, voltage and current, which fits almost no descriptor (usual order: 1 = power, 2 = import, 3 = export, then L1..L3): Domoticz got the power as energy, the import as power, the export as voltage. Now the first `W`/`kW` line is the power (kW × 1000), the first `kWh`/`MWh`/`Wh` line the import and the second the export (both in Wh like Tasmota's energy driver), the first `V` / `A` line voltage / current; only values already received from the meter. Sent once per teleperiod: `DZ_POWER_ENERGY` (power;import) and `DZ_P1_SMART_METER` (import;0;export;0;power), voltage and current when found, each only when its `DzSensorIdx` is set. ⚠️ It changes what Domoticz receives for anyone who relied on the old position-based mapping. Without `USE_DOMOTICZ` nothing changes. Tested by ottelo on an ESP32-C3 (MT175: `26714427.2;0;659262.9;0;180;0`, matches the main page); here compiled with Domoticz on an ESP32-S3.
- ⚠️ **An OTA update no longer wipes the heap arrays of persist variables** (ottelo, 20.10.2026: after a firmware update the charts of the SML programs were empty). OTA start stops every slot (`TinyCStopVM` saves the persist file and frees the heap), and then `FUNC_SAVE_BEFORE_RESTART` saved the loaded slots a second time: every persist array above 16 elements (chart day and week bars, curves) went out as zeros over the good file, while scalars and small arrays survived. That loop now skips slots that are already torn down, and `tc_persist_save()` refuses to write while a heap entry has no heap (log line `persist … not saved - heap array gone`). Checked on an ESP32-S3 with a test program (`persist int cnt; persist int big[40]`): three OTA runs in a row over the fixed firmware kept the array. ⚠️ The fix works in the firmware that is being REPLACED: the OTA that installs this version still runs the old code and can wipe the arrays once.
- **EEBUS Steuerbox v187 / SteuVE v016: direct Steuerbox ↔ SteuVE connection, IPv6, robust sending** (Andreas, 20.10.2026). A direct connection between the two broke after ~20 s and took the HEMS connection down with it: both sides sent their 3.6 KB discovery at the same moment and neither read, so the Steuerbox cut its frame (log: `WS-SENDEN UNVOLLSTAENDIG … bei Byte 3456 von 3622`) and the SteuVE lost the frame boundaries. Steuerbox: header + masked payload go out in ONE buffer / ONE `write()` instead of 64-byte records, a frame that goes out only partly marks the connection broken and tears it down cleanly (`EebusSendBroken`, keep-alive reconnects). SteuVE: reads while it waits to write (non-blocking `send`, own receive buffer, abort after 5 s WITHOUT progress). IPv6: `peer_ip` 46 characters (a heat pump called in over IPv6 and the address was cut at 16), the scan keeps the first IPv6 per device, `EEBusPeers` shows `"Ip6"`. Tested by Andreas against a HEMS (all three ways: Steuerbox ↔ HEMS, HEMS ↔ SteuVE, Steuerbox ↔ SteuVE direct); here only compiled and booted on an ESP32-S3.
- **`steuve` v007 (`@version 20261020_1321`) knows the common Shelly kinds, `steuve.html` has a TinyC and an IDE button** (Andreas, 20.10.2026). Gen2+ switches (Plus 1PM, Plus Plug S, Pro 1PM …) and Gen1 relays measure and switch; PM Mini Gen3, Pro EM / EM Gen3, Pro 3EM and Gen1 SHEM / SHEM-3 only measure and are never switched ("switchable" is taken out by the script). A device listed as a switch that answers without `output` is asked again.
- **TinyC says which file system it uses** (20.10.2026): at boot `TCC: user file system = SD card, flash as fallback` or `= flash`, and a `.pvs` that is not on the SD card but read from the flash copy logs `persist … not on the SD card - read from the FLASH file system`. Andreas found a SteuVE that started with the EMPTY flash copy of its `.pvs` although the SD card held the saved settings (two diverging copies), and a `steuerbox.html` on the SD card that was not found; if the SD card is not mounted at boot, the whole boot uses the flash. The line shows that at once.
- ⚠️ **Loading a script from the IDE now takes its saved values over, and a never-started program no longer overwrites the `.pvs`** (Andreas, 21.10.2026). The IDE loads through the upload, not through `TinyCLoadFile`, so the take-over of the entry above never ran there. Worse: an upload loads the new program WITHOUT reading its `.pvs`, the IDE then sends a stop before "run", and the stop saved the program's DEFAULT values over the `.pvs` (also over one just taken over; probably also when the same script is uploaded again under the same name). Now `TcSlot::started` is false after every load and true once `TinyCStartVM` has read the `.pvs`; a stop and the save before a restart write only when it is true (otherwise log line `TCC: persist not saved - <file> was loaded but never started`), and the upload calls `TinyCAdoptPersist` with the slot's previous file name. Measured by Andreas on the SteuVE: IDE load v009 → v010 with the same `@name` keeps the whole list, uploading the same version again keeps it, a `.pvs` copied by hand is no longer overwritten. Checked here on an ESP32-S3 through `/tc_upload` + stop + run like the IDE (`persist int cnt; persist int big[40]`): a new file name with the same `@name` carries the values over (1 → 2, array 7 → 14), uploading it again keeps them (3, 21). A program that ends with a VM error is not covered by `started`, but `tc_persist_save()` already refuses to write while a heap array has no heap. ⚠️ Someone who already had the fault finds an empty `.pvs` under the new name: delete it once by hand, the take-over never overwrites an existing `.pvs`.
- **`steuve` v010 (`@version 20261021_1027`) also reads and switches Tasmota devices with energy metering** (Andreas, 21.10.2026). A slot that does not answer as a Shelly is asked with `/cm?cmnd=Status`; relay count from `Status#Power`, measurement from `Status 10` (`StatusSNS#ENERGY#Power`). Types 7 = relay + measurement, 8 = measurement only (a meter), 9 = Tasmota without `ENERGY` (not usable); 8 and 9 are never switched. Switching is `Power0 On/Off` (all relays of a strip, which has one measuring circuit). An empty answer (HTTP API off) counts as unreachable, `WARNING` as "Tasmota: Passwort?". `steuve.html`: the tab "Shellys" is now "Verbraucher". Andreas measured three former Gosund plugs and four Shellys, and under load 210.1 W (Tasmota), 218.2 W (Shelly), 210 W (SteuVE). `CLAUDE.md` anti-pattern 28.
- **EEBUS Steuerbox v188 / SteuVE v017: an incoming connection is also matched to its SKI by the AccessMethods id** (Andreas, 21.10.2026). A heat pump (Daikin EKRHH, forum thread 263104) calls the box over IPv6 and has TWO IPv6 addresses (ULA and global); the scan keeps only the first, so the connection kept its pseudo-SKI (the address) and the page said "keine stehende Verbindung mit dieser SKI" (`EEBusLpc auto` in the console worked). New `EebusSrvSkiFromId()`: a connection without a real SKI is matched against the mDNS `id` of the found devices (log line `EBG: SRV <ip> ist SKI <ski> (ueber die Kennung <id>)`). Confirmed by the forum tester with his Daikin (the limit is written and the heat pump reacted); Andreas runs both on his own devices. ⚠️ The id comes from the peer's own AccessMethods message and the SKI is not checked against the TLS certificate (BearSSL-light does not give the peer certificate): a device on the LAN that sends the id of a trusted Energy Guard is treated as that device. Before, the mapping needed the same IP address. On the SteuVE the trust check (`EEBusCsTrust`) compares exactly this SKI, so such a claim would pass it. These are test devices; the missing certificate check is already known (discussed with Andreas for `xdrv_129`).
- ⚠️ **EEBUS Steuerbox v189 / SteuVE v020: no crash when a device announces only IPv6; the SteuVE trusts an Energy Guard only by its ADDRESS; `EEBusCsTrust` works without a restart** (Andreas, 21.10.2026). (1) `EebusShipConnect` took the target address from the mDNS IPv4 only; a device that announces only IPv6 gave an empty address, `fromString` failed and `EebusShipFree()` called `stop()` on a TLS client that never tried `connect()`: load access fault at address 0, restart (found by his HEMS branch in a test network; affects `EEBusConnect` by hand and the automatic reconnect, and is older than v188). Now the address is checked BEFORE the client is created; without IPv4 the IPv6 address is used (host header in `[]`); without any address there is no attempt (`EBG: kein Verbindungsversuch zu … - keine Adresse (mDNS)`). (2) SteuVE v018: an SKI that was found through the AccessMethods id (v017) does not count for the trust check (`ski_aus_id` on the slot, `EebusCsTrusted()` asks for an SKI that came from the address) — closes the gap of the v017 entry above for the SteuVE. An Energy Guard with several IPv6 addresses that calls over another one than the scan kept is therefore not recognised as trusted for now. (3) SteuVE v019: `EEBusCsTrust <ski>` looks for the new Energy Guard at once (reconnect plus a new mDNS announcement); `EEBusCsTrust 0` forgets the remembered partner if it was the old one. Tested by Andreas in a test network (Steuerbox v189, SteuVE v020 and his HEMS branch, no restart until the end; the SteuVE announced IPv4 in these runs, so the IPv6 fallback itself was not exercised) and on his own devices (v018/v019 at the energy manager, `EEBusCsTrust 0` → `hm`: controlled again after 8 s). The illustrated manuals follow v189 / v020.
- ⚠️ **EEBUS (Steuerbox, SteuVE): known design flaw — the peer's SKI is not taken from its certificate** (found by Andreas, 22.10.2026). All EEBUS drivers since the first version (31.07.) identify the peer by IP address and the mDNS list: an incoming connection (no client certificate is requested) is matched to a device by its address, and for a connection the device opens itself the SKI comes from the peer's own mDNS announcement. That is the common root of the problems of the last days: an incoming call while the list is still empty, a device that calls over IPv6 or over a second IPv6 address (the forum tester's Daikin heat pump; the id matching of v188/v017 was a workaround, and an id can be chosen freely, hence v018), an old address in the list. Security: the Steuerbox writes limits itself and obeys no caller; the SteuVE (and the HEMS) obey a trusted Energy Guard, and inside your own network whoever gets that device's address is treated as that device. Not reachable from outside; for a control chain under § 14a EnWG it still has to be closed. The fix — the SKI is taken from the certificate in the TLS handshake and the peer has to prove its key (BearSSL server: client certificate requested, `CertificateVerify` checked) — is built and tested by Andreas for the receiving side in a driver of his; the client side needs a small, `#ifdef`-guarded addition to `tls_mini`; the Steuerbox and the SteuVE follow. A connection without a client certificate must not count as trusted (SHIP requires mutual certificates). The workarounds through the id (v188/v017/v018) can go then.
- ⚠️ **EEBUS Steuerbox v193 / SteuVE v022: the peer's SKI now comes from its TLS certificate** (Andreas, 22.10.2026; closes the design flaw of the entry above). **Incoming** (a device calls us): the TLS server asks for a client certificate (BearSSL: one empty trust-anchor name, `BR_OPT_TOLERATE_NO_CLIENT_AUTH`), a small X.509 reader takes the EC key of the first certificate, SKI = SHA-1 over the uncompressed point (65 bytes), and `br_ssl_engine_set_ecdsa` makes BearSSL check the `CertificateVerify` signature, so a copied SKI is worthless. **Outgoing** (we call): `tls_mini` hands the peer's public key of the last TLS connection to the driver (`BearSSL::ShipPeerPubKey()`, the only change in `lib/lib_ssl/tls_mini`: a static buffer and a free function, everything behind `USE_EEBUS_GUARD/STEUVE/HEMS`, no class member, so a build without EEBUS stays unchanged), the driver forms the SKI and compares it with the mDNS list; a mismatch gives no connection. `EEBusStatus` shows `"SkiQuelle":"Zertifikat|Adresse|Fundliste"` per connection. **SteuVE:** trust (limits, failsafe, heartbeat, subscription) counts only when the trusted Energy Guard identified itself with a certificate; a connection without one is shown and matched but every write is refused (`vertraut nicht, kein Zertifikat`, errorNumber 7). Switch `EEBusCsStreng` (default 1; `0` only for tests with devices without a client certificate, not saved). A write to a limitId other than 1 is ignored and logged. Checked by Andreas: the SKI from the certificate of his energy manager equals its mDNS SKI in both directions (method 1 fits; the SubjectKeyIdentifier extension is not read yet); a lab chain HEMS ↔ SteuVE in both start orders and with both calling at once; the Steuerbox at the energy manager (limits under § 14a and § 9 accepted and handed on). The negative case (a caller without certificate) is shown in the source, not attacked live. ⚠️ A device that forms its SKI differently (not RFC 5280 method 1) would be refused as an outgoing peer. Built here for both Ethernet images and started on an ESP32-S3 (the Steuerbox answers `EEBusRole` and shows `SkiQuelle`, the SteuVE `EEBusCsStreng`=1); a TLS client with a self-made EC certificate completed the handshake against the SteuVE's SHIP port with and without a client certificate. Not run here: the SHIP link after the handshake with a certificate (the log line `weist sich mit Zertifikat aus`), the SteuVE as a caller against a device, Ethernet at the W5500, TLS against a real HEMS.
- **EEBUS Steuerbox v217** (Andreas, 28.10.2026). Without an SD card the operating log is no longer silently skipped: once per start (15 s after boot) the log line `EBG: Betriebslogbuch aus - keine SD-Karte` is written, and `EEBusStatus` has a new field `Betriebslogbuch` (`aktiv` / `aus - keine SD-Karte`). Nothing else changed (SteuVE stays v025). Tested by Andreas on his test Steuerbox with and without SD card; taken over here WITHOUT a build or a device run (file byte-equal to his attachment, LF).
- **EEBUS Steuerbox v216 / SteuVE v025, `tls_mini` per connection** (Andreas, 25./26.10.2026). `tls_mini`: the peer's public key is kept per connection (table of 8 slots, released in `_freeSSL`) instead of one static buffer, so a second TLS connection can no longer overwrite the key of the first. Steuerbox: up to 6 devices (HEMS and direct mixed, mode per connection); a command to a device without a standing connection is refused and logged; after a reconnect it sends its last limit or a release within 30 s (LPC-913), a refused limit is repeated every 60 s; the automatic rebuild takes a partner that is in the scan first (`EEBusAutoConn -<SKI>` removes one partner); a directly controlled device is dimmed exactly to its minimum power (BK6-22-300 Anl. 1 Ziff. 4.5.1, 4200 W); failsafe values are written on both paths; the server send path no longer blocks (a SteuVE that called first got no limit, mutual blocking while both sent their discovery); mDNS now announces the IPv4 address too (before: AAAA only, a device without IPv6 could not call). New: an operating log on the SD card only, `/betriebslogbuch/YYYY-MM.csv` (VDE FNN recommendation on Tenorziffer 2a BK6-22-300, 4.1.2.2 and connection monitoring; folder, columns and 30 months retention are our choice), without an SD card nothing is logged. SteuVE: after a restart it starts limited to the failsafe value and counts as controlled only after heartbeat AND a limit (LPC-901/902/916). New `steuerbox.html` / `steuve.html` and manuals (state 26.10.). Tested by Andreas in a test network (two SteuVE) and on his own plant with the Sunny Home Manager 2.0. Taken over here WITHOUT a build or a device run (files byte-equal to his attachments apart from the line ends, which follow the repository). Known open on his side: a first call of a SteuVE once ended with errno 104; a file uploaded to an SD card was back at the old state after power-off.
- **Illustrated manuals for the Steuerbox (v187) and the SteuVE (driver v016, script v010)** by Andreas (21.10.2026), German, as PDF and as HTML sources with the images (addresses, SKIs and identifiers blacked out): `tasmota/tinyc/docs/eebus_anleitungen/`, linked from `docs/EEBUS_ETHERNET.md`.
- **Repository list: `"ordered":true` and `{"sep":1}` in `index.json`** (ottelo, 12.10.2026). A repository with its own order keeps it (`"ordered":true` skips the sort), and `{"sep":1}` is a disabled separator line. Without `"ordered"` nothing changes: the list is sorted by its label and separators are dropped.
- ⚠️ **A `spawnTask` worker can use arrays again (ESP32).** Every array > 16 elements — locals included — lives in the slot's heap, but the worker VM had an empty private heap of its own, so any such array was a `BOUNDS heap[n] idx=0 size=0` error there (once even a reset), and no string longer than 16 characters could be handled. The worker now works on the **primary's heap** (`TcVM.heap_shared` / `heap_peer`; the primary refreshes the worker's view when its heap grows; the worker never allocates). Found and fixed by ottelo (12.10.2026, an FTP upload in a worker). Tried on an ESP32-S3 (`examples/worker_arrays_test.tc`): a local `char[240]`, a 15 s worker with `delay()` next to commands and web renders, 30 runs in a row, stop / eject / reload / reset with a worker running. A worker and a callback that use the same array share the buffer. Before this fix: use `TaskLoop()` for jobs that need buffers.

## 1.6.73 — 2026-10-08

- ⚠️ **Updating from official Tasmota (or any firmware with another settings ID) resets the settings, Wi-Fi included — make a backup first (`Backup Configuration`).** Official Tasmota uses `CFG_HOLDER` 4617, our builds 5002. The device then starts with default settings and, without Wi-Fi, opens an open access point named like `sonoff-1234` (`sonoff` is our builds' default topic, plus four digits; `http://192.168.4.1`): enter the Wi-Fi again. Until 1.6.72 such a device kept loading its old `/.settings.lkg` file on every boot — Wi-Fi survived, but no changed setting did (Rolf, 05.10.2026); the last-known-good copy is now only used when it carries the compiled holder.
- **`/tcrepo` ("Run IDE from repo": the IDE is read from GitHub, nothing is stored on the device) is on by default** in every build. It was only set by some of our own environments, so a build from the repository and the ESP8266 build did not serve it. `-DTINYC_NO_REPO_IDE` leaves it out (ESP8266: +4.2 KB).
- **New: `common/net_watchdog.tc`** — restarts a device whose network stack hangs (a Matter node on an ESP32-C6 hung for hours with script and display running while HTTP and Matter were dead) and keeps a small black box file (`/bb1.txt`, `/bb2.txt`, 10–20 hours, the restart reason once per start). `matter_home_bridge.tc` uses it and now sends the lamp's commands over HTTP (answered, repeated).
- **New examples: `zeitschaltuhr.tc`** (a timer that works as a state controller: after a power cut or reboot every relay is put into the state the timers say it should have now; 8 timers, sunrise/sunset, random window, weekdays, weekly plan) and **`sensor_log87.tc`** (one CSV line every two minutes with every sensor of a sensor box, kept on the device's flash and appended to the NAS). **`energy_dashboard.tc`**: month chart and table show the real date instead of 0..30 / 1..31.
- **Example lists: one label and one order everywhere** (mi-hol, #128). The `/tc` Repository box showed "name (file.tcb)" sorted by file name, the IDE list "Repo Examples" only the bare file name, so an example could not be found by the name the other list showed. Both now show **"Plain name (file)"** and sort by that label; the IDE reads the names from `bytecode/index.json` (bare file name when offline), and the 22 examples that had no `// @name:` got one. The IDE part arrives with `TinyCIde` + a hard reload, the `/tc` part needs this firmware.
- **New: `common/nas_backup.tc` and the example `nas_log_upload.tc` — move files (Tasmota `FileLog` logs, CSV, …) to the NAS over FTP and free the space** (mi-hol, discussion #126). `nb_logs()` uploads every log file Tasmota has finished with (size unchanged since the previous call) under a unique name and deletes it locally; `nb_put()` / `nb_move()` do the same for any file. Same `/ftp.cfg` as `ftp_log.tc`. Tried on an ESP32-S3 against a FRITZ!NAS.
- **`energy_backup.tc`: the source URL is now an optional fifth line of `/ftp.cfg`** (mi-hol). Without it the old default applies, so nothing changes for existing setups.
- **`energy_backup.tc` checks its source address before it starts** (mi-hol, #128): `http://host[:port]/path/` only. A typo such as `http://host/:82/ufs/` (slash before the port, copy and paste) is refused with a message in the "Energie-Backup" row instead of sending a request to port 80 of that host.
- **SML family from ottelo (07.10.2026): new PV switch (`pv_switch_common.tc`), CT003 Bluetooth fix, and for a B2500 the lowest "step" is raised to 80 W (PR #89, Gronkdalonka).** A configured "step" of e.g. 40 W could keep a B2500 battery from ever restarting after the feed-in had switched off, because its minimum output is 80 W. Taken over unchanged (`ct002_common.tc`, `ui_common.tc`, `sml_chart_common.tc`, `sml_descriptor.tc`, `sml_modbus_common.tc`, the six programs; `pv_switch_common.tc` is new).
- **SML family from ottelo (05.10.2026): the CT002 limit is now applied to all phases, not only phase D (PR #88, Gronkdalonka); `sml_ct002` and `sml_chart_ct002` added with `.tc` and `.tcb`.** Taken over unchanged (`ct002_common.tc` and the two programs); all 214 examples compile and the `.tcb` are bit-identical to his.
- **SML family from ottelo (04.10.2026): the meter selection says "meter" instead of "repo" and the list is sorted by manufacturer.** Taken over unchanged (`sml_descriptor.tc` and the six programs); all 214 examples compile and the six `.tcb` are bit-identical to his.

## 1.6.72 — 2026-10-04

- **Repo pulldowns: an entry can be selectable without a file (`"keep": 1`)** (ottelo). Since 1.6.71 an entry with an empty `filename` is a disabled heading, so "no meter from the list — I write `/sml_meter.def` myself" could no longer be chosen. An entry with an empty `filename` and `"keep": 1` is selectable and downloads nothing, only its key is stored (TinyC `webRepoPulldown()` and the Scripter SML pulldown). Without `keep` nothing changes; old firmware ignores the field.
- ⚠️ **Scripter `udp(1)`: one byte was written past the receive buffer** (ottelo, reported by next145). The datagram was read with the full buffer size and then terminated with `packet[len] = 0`: a datagram of `max_ssize` bytes or more wrote one byte onto the stack, and a read error (-1) one byte before the buffer. Now one byte less is read and an error counts as an empty packet.
- ⚠️ **`tasmCmd(char[] …)` / `tasmDefer(char[])` with a command built at run time** (ottelo). The command buffer was 128 bytes, so every command was cut at 127 characters, and the command was read as `int32` slots even from a packed `byte[]` (four characters per "character": garbage). Both now go through the byte-aware string reader: 511 characters on ESP32 (255 on ESP8266, `tasmDefer` 255), a longer command is logged as cut, and the answer is written into a `byte[]` correctly. Checked on a device with a 199-character command from a `char[]` and from a `byte[]`: the log shows it complete.
- ⚠️ **`tasmCmd()` returned 0 bytes for EVERY command since the last Tasmota core update.** The core's `XdrvRulesProcess()` ends with `ResponseClear()`, so `ResponseData()` is empty the moment `ExecuteCommand()` returns; the answer was published (MQTT, console) but never reached the script. `tasmCmd` now reads the answer the way Tasmota's own `/cm` handler does — from the log buffer (temporary log level, the `RESULT` line since the call) — for the literal form and the `char[]`/`byte[]` form alike; several answers are merged into one object as `/cm` does. Checked on an ESP32-S3: `Hostname` → `{"Hostname":"devkit-s3"}` into a `char[]` and a `byte[]`, an unknown command → `{"Command":"Unknown","Input":"…"}`.
- **Scripter (ESP8266): Marstek CT002 registration page `/ctreg`** (ottelo / next145). Build with `-DUSE_SCRIPT_CT002_REGISTRATION` (ESP8266 + web server only; nothing changes for any other build). Log in with the Marstek e-mail and password (hashed at once, never stored), reuse an existing HME-4 of the account or create one, so the CT002 emulator script can be used by the batteries. Header `tasmota/include/xdrv_10_ct002_registration.h`, four hooks in `xdrv_10_scripter.ino`.
- ⚠️ **Matter: live reports are sent as the device's own exchange — Apple Home stops re-subscribing every 44 s.** Subscription and event reports were sent as if they were responses (Initiator flag clear, the closed Subscribe request's exchange id). Apple acknowledged them at the transport layer but never passed them on, so the subscription looked silent and, 44 s later (MaxInterval 30 s + margin), it subscribed again on a new session. The stale sessions piled up (16 on a single ESP32-C6, 1 KB report each every 5 s), until the device froze — it surfaced after an iPadOS/HomePod update. Live and event reports now carry the Initiator flag and a new exchange id, and the controller's StatusResponse is read: a rejection (e.g. InvalidSubscription) drops that subscription. After the fix: one session, no re-subscription in 5½ minutes, the controller answers every report.
- **Matter: dead sessions are evicted by what the controller sent, not by our own reports.** A session's age was taken from our last report, which every report refreshed, so nothing ever aged out. Sessions now count as dead after 90 s (or twice the report interval) of silence from the controller; a new session of the same node replaces stale ones after 60 s; a full table evicts the longest-silent session.
- **Matter: our acknowledgement of the controller's StatusResponse now carries the right Initiator flag.** The ack to a reply on an exchange the device started went out with the flag clear, the controller did not match it and repeated its StatusResponse four times after every report. Now one reply per report. The same fix and the two above are in the `MATTERF` plugins too (they carry their own copy of the engine, so the first 1.6.72 plugin files did not have them).
- ⚠️ **Matter: Home showed the wrong state of a lamp — three engine/firmware bugs.** (1) Every attribute report carried `DataVersion = 1`; a controller may treat a report with a version it already holds as "nothing new". The version now starts at a random value per boot and changes with the data. (2) Boolean attributes (OnOff) went out as a TLV unsigned integer instead of a TLV boolean, which a controller decoding the attribute as bool rejects. (3) The firmware answered OnOff of endpoint 1 with the state of the device's own relay 1. In a script whose endpoint 1 is a light (`matter_home_bridge.tc`) Home therefore showed the state of the bridge's relay, never the state of the lamp. Endpoint 1 now mirrors relay 1 only while it is a plug (`MATTER_PLUG` or no script model); a light declared by a script owns its OnOff.
- **`matter_home_bridge.tc` reports the REAL state of the lamp back to Matter.** The lamp is asked over HTTP every 4 s (one `HSBColor` request returns the whole light state) and Power / Dimmer / HSB are mirrored into the Matter attributes. For 10 s after a command from Home the answer is ignored; a lamp that still disagrees is logged. The status page shows the real lamp state next to the Matter state.
- **Person detection no longer fails every other time (`person detect error -4`).** The motion detector decodes the camera slot in the main loop every 500 ms, the person detector decodes it in the VM task; run at the same time, `jpg2rgb565()` failed in about every second person check. The two decodes now take turns (the person check waits, the motion sample is skipped while the decoder is busy). Measured on a DFR1154 with a script that only ran `camControl(21)` in a loop: motion on 27 of 50 failed before, 0 of 75 after; with motion off it was 0 of 50 already.
- **SML program family updated from ottelo (03.10.2026).** `sml_ct002` / `sml_chart_ct002`: new commands `CTOFFSET [W]` and `CTLIMIT [W]`, a `ct_maxdc` option; `sml_eco_shelly` / `sml_chart_eco_shelly`: the PV emulator's offset can be changed at run time (`PVEmuOffset [W]`, RAM only; Save keeps it) and appears as a Home Assistant number entity (new `common/ha_discovery.tc`), the live value is shown next to the field. `sml_descriptor`: a descriptor with only power and import no longer reports its power as export (the driver answers an index past the last value with 1). `sml_chart_common`: the "Feed-in" row only with feed-in on. The six `.tcb` from our compiler are bit-identical to his.
- **DFRobot AI CAM build: 72 KB smaller.** `user_config_override_dfrobot.h` now switches off the GPIO viewer, the light driver (and everything that forces it back on: PWM dimmer, Tuya MCU, Arilux RF, AC zero-cross dimmer) and the Hue/WeMo emulation — the board has no use for them. 2 771 904 → 2 699 344 B, static RAM −1.2 KB; camera, TinyC, speech and ESP-DL stay.
- **Camera script: `Sleep 0` against gaps in the picture** (`webcam_tinyc.tc`). Tasmota's default `Sleep 50` also puts the WiFi radio into modem sleep: a web request took ~140 ms instead of ~45 ms (AI-Thinker ESP32-CAM and DFR1154 alike), and the main loop, which serves the MJPEG stream, stalled for 1.0-1.7 s now and then. The script now sets `Sleep 0` when the camera starts. Measured on the AI-Thinker at VGA over 60 s: before 7.6 frames/s and nine gaps up to 1.5 s, after 8.8 frames/s and two gaps of 0.4-0.5 s (those are the motion detector's JPEG decode in the main loop; without motion detection 10 frames/s and no gap). Redeploy the script, or run `Sleep 0` by hand.
- **TinyC camera page in Safari** reads the MJPEG stream itself (`fetch`, one connection, full frame rate) instead of one request per picture, with the single images as fallback; `/tc_cam.jpg` waits up to 100 ms for a frame being written instead of answering 503 (a whole second of freeze in Safari on a classic ESP32-CAM). The stream server sends `Access-Control-Allow-Origin: *`.

## 1.6.71 — 2026-10-02

- **Text to speech as a plugin: `PICOTTS_32.bin`.** `I2STTS` no longer needs a firmware built with `-DTINYC_TTS` (125 KB of flash): the SVOX Pico engine runs from the plugin, the task, text queue and voice buffers stay in the firmware, and the unchanged `I2SAUDIO` plugin uses it through the same jump table entries. Tested on an S3: German and English (all eight test sentences render exactly the sample count of the reference), switching languages back and forth without a leak. Guide: `docs/PICOTTS_PLUGIN.md`. ⚠️ The engine reads memory it never wrote — the firmware now zeroes its arena (this also applies to the built-in engine). One function of the engine had to be compiled at `-O0` because the Xtensa toolchain miscompiles it at `-Os` (see `tasmota/Plugins/picotts/test/README.md`).
- **`chkpt aN` accepts up to 8** (512 KB plugin partition, was 4): I2SAUDIO + PICOTTS + Matter do not fit into 256 KB.
- ⚠️ **Replacing a plugin by one of the same size failed with `flash slot memory error`**: the sector count was rounded up twice, so a plugin never fit a hole of exactly its own size. Fixed in `Module_CheckFree`.
- **DFRobot AI CAM (DFR1154): complete build guide** (`tasmota/tinyc/dfrobot/ANLEITUNG.md`, German) with ready-made `platformio_override_dfrobot.ini` and `user_config_override_dfrobot.h` — both files are in `.gitignore`, so a fresh clone had no TinyC switches at all. Checked from a fresh clone with only those two files: firmware (camera, person detection, USB console) and the audio plugin both build. The audio plugin `I2SAUDIO_32.bin` is attached to the release now (the built-in I2S audio is off in these builds; the plugin must be loaded AND "Autostart plugins at boot" ticked, or audio does nothing).
- **Repo pulldowns keep a stable key** (ottelo, via gemu): `webRepoPulldown()` and the Scripter `smlpd()` stored the POSITION of the chosen entry, so inserting a meter anywhere but at the end of `smartmeter.json` made every existing device show the wrong name. An entry may now carry an optional integer `"id"`: the key is the `id` when present, else the position (JSON without `id` behaves exactly as before; old firmware ignores `id`). The stored value, the preselection and the download lookup all use the key. An entry with an empty `filename` is now disabled in the TinyC pulldown too (as the Scripter always did). Checked on a device with an out-of-order list: preselect by key, the download takes the entry with that `id`, a heading downloads nothing.
- **Line charts with one value per day get a weekday tick on every point** (gemu2015/Sonoff-Tasmota#125). The ticks sat at midnight, the START of a day, so the first day (today, index 0) never got a name and, in the evening, the first point looked like it belonged to the next day: weather.tcb's line charts began at "Mi" on a Tuesday while the columns below said "Di". A series stepping about one day (23–25 h) now has its ticks on the points, labelled with each point's own weekday — the rule the column charts always used. Hourly and shorter series keep the midnight ticks.
- ⚠️ **MP3 recordings to SD had a constant tone near 5.5 kHz (`I2SAUDIO_32.bin`).** The plugin's float port of the Shine encoder scaled the polyphase filter coefficients by 2.147483647e9 instead of 2.147483647 (`0x7fffffff * 1e-9`), 1e9 too big, so nearly every coefficient saturated. The raw I2S stream (audio bridge to the Mac) was always clean; only the MP3 path was broken. 4–8 kHz level in a quiet-room recording: about 100 dB before, about 50 dB after (the raw stream's level). Plugin-only fix (the firmware is the same as before): replace `I2SAUDIO_32.bin` on the device (`deiniz`, `unlink`, upload, restart), the firmware is unchanged.

## 1.6.70 — 2026-09-29

- ⚠️ **A click on ▶ / ■ / ↻ / ⏏ / A on the console page no longer repeats itself** (gemu2015/Sonoff-Tasmota#123). The buttons are a GET form, so a click left `/tc?slot=1&cmd=run` in the address bar and the history, and every reload of that address ran the command again: a browser that reloads a background tab when you switch back to it (Chrome's memory saver) restarted the slot on every window change, and so did back/forward. Commands are now answered with a redirect to a plain `/tc` (POST/redirect/GET). Scripts that call `/tc?cmd=…` still work — the command runs, the answer is a 303.
- ⚠️ **The same for the Matter page `/mt`:** a reload of `/mt?unbind=1` removed the device from all Matter controllers again, `/mt?bind=1` reopened the pairing window. Both redirect to a plain `/mt` now.

- ⚠️ **Matter is built in only on the S3 now.** The built-in engine takes about 33 KB of static RAM on every device, whether it ever uses Matter or not. The C3, C6 and P4 test builds are plugin-only (`-DUSE_MATTER_C_PLUGIN_ONLY`) and load Matter as `MATTERF_32r.bin`; the S3 keeps the engine and can use `MATTERF_32.bin` instead. Both plugin files and a step-by-step guide (`docs/MATTER_PLUGIN.md`, `_DE.md`) are attached to the release. ⚠️ Creating the plugin partition formats the file system — scripts and an existing pairing have to be saved first. Tick "Autostart plugins at boot" so the plugin gets its 71 KB at boot.
- ⚠️ **"Autostart plugins at boot" is saved now** (`/plugins.auto` in the file system). The checkbox used to flip the Option A7 flag only in RAM — it was rebuilt from the GPIO configuration at boot, so the tick was lost at the next restart unless a GPIO was set to Option A 7. Without the file Option A7 still works as before; the flag itself is no longer touched (Tasmota also reads it as "Shelly Pro"). The P4 test build had no Matter at all before (its env never set `TINYC_MATTER`).
- ⚠️ **Strings are no longer cut silently** (gemu2015/Sonoff-Tasmota#116): every place that clamps a string now logs it once per script load — `TCC: /sml_chart.tcb: syscall 354 cut a string at 383 of 430 chars: "var g=google.visualizati..."`. Covers `strcpy`/`strcat` into a too-small array and the fixed buffers inside syscalls (URLs, payloads, paths, labels, …). The start of the string names the culprit.
- **`TinyCStrict 1`** turns such a cut into an error: the script halts with "String truncated" (error 14) and a `TCC: HALT (TinyCStrict)` line. Off by default and after every restart — meant for development (suggested by @mi-hol in #116).
- ⚠️ **Small arrays are now bounded too.** A global or local array that is not on the heap (up to 16 elements) was passed to syscalls without its size: `strcpy`, `strcat`, `sprintf` and every other writer stopped only at the end of ALL globals (or of the frame) and overwrote the neighbouring variables — `strcpy` of 49 chars into a `char[10]` changed the next `float` and `int`. The compiler now puts the array's end into the reference; the firmware clamps there (and logs it, see above). Indexing through an array parameter past that end now fails with "Bounds error" instead of writing into a neighbour. Older `.tcb` behave as before; older firmware ignores the new bits, so no ABI step. Recompiling changes the bytecode of every script with small arrays.
- ⚠️ **`WebChartJS()` has no length limit any more.** It copied the snippet into 384 bytes and dropped the rest: ottelo's 4 h chart lost its tail, the browser got a syntax error and the default chart was drawn instead — with nothing in any log. The snippet is now streamed.
- Compiler: `strcpy` followed by `strcat`s of string literals into a fixed `char[]`/`byte[]` is added up and warns when it does not fit (`'g' needs 10 chars but holds 9`). Found two real cuts in the examples: `heatpump_map_full.tc` (the changed-row class lost its closing quote) and `pool_pump.tc` (the fixed nonce lost its last byte).
- Matter plugin on S3 boards with OPI PSRAM (`CONFIG_SPIRAM_XIP_FROM_PSRAM`): the plugin is entered only from the loop task. Flash writes (saving a pairing) stall only the writing task there, and plugin code running in another task at that moment crashed with IllegalInstruction. Calls from other tasks are handed over (`MtrHandover` in the `TinyC` status).
- Examples: ottelo's SML family, state 2026-09-28 (chart snippet, day/week/month/year values of the Bezug/Einspeisung JSON).

## 1.6.69 — 2026-09-27

- ⚠️ **`chkpt r` / `chkpt d` could brick the device:** removing the plugin partition left the old MD5 entry of the partition table standing behind the new one. The bootloader rejects a table with two MD5 entries ("Only one MD5 checksum is allowed") and the device boot-loops. Fixed; hit two ESP32-C3. A device already caught in the loop needs its partition table rewritten over serial (`esptool write-flash 0x8000 partitions.bin`).
- **Matter as a binary plugin** (`MATTERF_32.bin` / `MATTERF_32r.bin`, 57–61 KB): at boot TinyC Matter uses the plugin when it is loaded, otherwise the built-in engine — both can be present. `-DUSE_MATTER_C_PLUGIN_ONLY` builds a firmware without the built-in engine. Runs on ESPs without PSRAM (tested on a C3). Sizes and trade-offs: "Built-in or as a plugin" in the Matter chapter of the reference.
- Plugins: the RAM shown by `mdir` and on the plugin page now includes what a module allocated itself (MATTERF: 12 B → 71 332 B).

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
