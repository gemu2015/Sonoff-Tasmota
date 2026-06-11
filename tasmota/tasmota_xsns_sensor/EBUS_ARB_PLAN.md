# Plan — in-firmware eBUS arbitration in xsns_53 (active master for Andreas's live bus)

Status: DESIGN (no code yet). Verify target: **.150 Wolf bus, read-only active queries**.
Reference: `danielkucera/esp-arduino-ebus` (`src/Arbitration.cpp`, `include/BusState.hpp`,
`src/BusType.cpp`) — the firmware that runs on the adapter.ebusd.eu v5/C3 shield (ESP32 wired
straight to the transceiver, arbitration done in firmware). Read 2026-05-28.

## 0. Why this, and how it differs from what we already shipped
- Shipped + verified (commit `656501760`): `soR` raw test mode = blind-tx a staged telegram on the
  next SYN, over a full-duplex Mac↔ESP UART harness. It validates framing/CRC/escaping/send-path but
  has **no arbitration** — unusable on a real shared multi-master bus.
- Andreas's aroTHERM is a **6-master** bus. To inject an active read query there we MUST win bus
  arbitration against the other masters. That timing (sub-millisecond, collision-based on a wired-AND
  line) cannot be met from Tasmota's ~50 ms `SML_Poll`. It must run in a dedicated UART-event path —
  exactly what esp-arduino-ebus does.

## 1. The algorithm to port (verbatim behaviour from the reference)

### 1a. `BusState` — SYN/address sequence tracker (`BusState::data(symbol)`)
A small state machine fed every received symbol; tracks where we are in the SYN→addr→(SYN→addr)→busy
sequence and timestamps each SYN (`esp_timer_get_time()`):
```
eStartup → (SYN) eStartupFirstSyn → (SYN) eReceivedFirstSYN
eReceivedFirstSYN        --addr--> eReceivedAddressAfterFirstSYN ; --SYN--> stay (new round)
eReceivedAddressAfterFirstSYN --SYN--> eReceivedSecondSYN ; --addr--> eBusy
eReceivedSecondSYN       --addr--> eReceivedAddressAfterSecondSYN ; --SYN--> error→eReceivedFirstSYN
eReceivedAddressAfterSecondSYN --any--> eBusy ; --SYN--> error
eBusy                    --SYN--> eReceivedFirstSYN ; else stay
```
Keeps `_master` (winning addr), `_symbol` (1st payload byte), `_SYNtime`, `_previousSYNtime`,
`microsSinceLastSyn()`.

### 1b. `Arbitration::start(busstate, QQ, startBitTime)` — put our address on the bus
Only on `eReceivedFirstSYN`. eBUS spec (test_1 v1.1.1 §3.2): the address must land **4300–4456 µs**
after the SYN start bit (SYN symbol = 4167 µs). Logic:
- if `timeSinceStartBit > 4456 || Bus.available()` → **late**, skip this round, retry next SYN.
- else (async): `delay = 4300 − timeSinceStartBit − 700`; `esp_rom_delay_us(delay)`; then `Bus.write(QQ)`.
  - **700 µs = the ESP32-C3 UART-TX lead** (time from `write()` to the byte actually hitting the wire).
    This constant is **chip-specific** and must be scope-tuned per target.

### 1c. `Arbitration::data(busstate, symbol, startBitTime)` — resolve win/lose (two rounds)
Driven per received symbol while arbitrating:
- `eReceivedAddressAfterFirstSYN`: read-back `symbol` (the round-1 winner, dominant-0 lowest wins).
  - `== QQ` → **won1** (we own the bus; send the rest of the telegram).
  - same low nibble (`symbol&0x0F == QQ&0x0F`) → same priority class → set `_participateSecond`.
  - else → lost round 1 (wait for `eBusy` to confirm).
- `eReceivedSecondSYN` + `_participateSecond`: re-send QQ for round 2 (same 4300 µs delay).
- `eReceivedAddressAfterSecondSYN`: `== QQ` → **won2**, else lost.
- `eBusy`: finalize **lost1/lost2**.
- Robustness: if a SYN arrives where an address was expected (our byte got clobbered) → **restart1/2**
  up to 3×. ⇒ arbitration-loss/restart is NORMAL; the caller must retry, not error out.

### 1d. Driver (`BusType`) — how it's clocked
- HardwareSerial 2400 8N1, **`setRxFIFOFull(1)`** → RX event per byte.
- RX ISR (`receiveHandler`, `IRAM_ATTR`) does `vTaskNotifyGiveFromISR` → a **dedicated high-priority
  FreeRTOS task** (`configMAX_PRIORITIES−1`) reads the byte + a timestamp and calls
  `Bus.receive(symbol, startBitTime)`, which runs BusState + Arbitration and emits RECEIVED / STARTED
  (won) / FAILED (lost) events.
- Two timestamp sources: SoftwareSerial gives the exact start-bit flange; **HardwareSerial uses
  `esp_timer_get_time()` at read time** (≈ end-of-byte) — coarser, which is why the 700 µs fudge
  exists. We'll use the HardwareSerial path (Tasmota already owns the UART as HardwareSerial).

## 2. Architecture in xsns_53 (new gate `USE_SML_EBUS_ARB`)
Everything new behind `#ifdef USE_SML_EBUS_ARB`. Non-arb meters byte-identical. New opt-in
**`soA[,QQ]`** (sibling of `soE`/`soR`): enable in-firmware arbitration master, QQ default 0xFF.

Reuse the **existing** staging path unchanged: `smlWrite(meter,"QQ ZZ PB SB NN data…")` →
`SML_Send_Seq` intercept → `Ebm_QueueTelegram` (appends eBUS CRC8 poly 0x9B, sets `ebm_req` last).
Arbitration only decides **when** the staged bytes go out.

New pieces:
1. **Port `BusState` + `Arbitration`** into xsns_53 (a new `.h` included by the .ino, or an inline
   block guarded by the flag). Keep the algorithm verbatim; swap `Bus.write/available` for the
   meter's `meter_ss`, `DEBUG_LOG`→`AddLog` (gated), QQ from `ebm_qq`.
2. **Dedicated RX path on the meter UART**: configure the meter's underlying `HardwareSerial`
   (`SML_ESP32_SERIAL::hws`) with `setRxFIFOFull(1)` + `onReceive(cb)`. The Arduino-ESP32 HardwareSerial
   `onReceive` callback runs in its own UART event task — set it high-priority. In the callback:
   `t = esp_timer_get_time(); while(hws->available()){ s = hws->read(); Ebm_OnSymbol(meter,s,t); }`.
3. **`Ebm_OnSymbol(meter, sym, t)`** (the new hot path): runs `BusState.data` + `Arbitration.data`/
   `.start`; on **won1/won2** stream the staged telegram **from index 1** (QQ already on the bus from
   the arbitration write) via `meter_ss->write` with AA/A9 escaping; on **lost/restart** leave
   `ebm_req` set so the next SYN re-arms; AND feed every symbol to the existing `ebus_feed_byte` so the
   passive decode + the slave response (after we win) decode through the normal meter-def matcher.
4. **No work in `SML_Poll`** for arb meters except a watchdog/timeout (e.g. drop a stuck request after
   N SYN rounds) and surfacing stats. The 50 ms loop never touches arbitration timing.

## 3. ESP32 / Tasmota integration specifics & risks
- **Single-core C3**: the `esp_rom_delay_us(≤~700)` busy-wait blocks the core briefly. esp-arduino-ebus
  runs this on a bare C3 fine; but we share the chip with WiFi + SML + (maybe) TinyC. Must confirm the
  arbitration event-task priority is above the SML loop but the busy-wait stays short. **Avoid WiFi TX
  inside the µs window** (the reference explicitly warns). Heed our vm_mutex/loop-starvation lessons —
  the arbitration callback must NOT take the TinyC vm_mutex or do HTTP.
- **Timestamp coarseness**: HardwareSerial onReceive fires after the byte is queued, so `t` ≈ end-of-
  byte, not start-bit. The 700 µs constant absorbs this on the C3 — but it's the #1 thing to verify on
  a scope. Provide it as a tunable `#define EBM_ARB_TX_LEAD_US 700` (per-chip).
- **TasmotaSerial vs HardwareSerial**: the meter UART is `SML_ESP32_SERIAL` wrapping `HardwareSerial`.
  Need clean access to `hws` for `onReceive`/`setRxFIFOFull`, and to make sure SML's own drain loop
  doesn't also `read()` the same UART (double-consume). In arb mode the onReceive callback becomes the
  SOLE reader; `SML_Poll`'s drain for this meter is disabled.
- **Reliability**: expect frequent lost/restart even when correct (ebusd #1622/#1240). The send path
  must retry across SYN rounds and give up gracefully after a cap.
- **Half-duplex / read-back**: arbitration relies on reading our own transmitted byte back from the
  shared wire. On the adapter.ebusd.eu shield TX and RX are on the one bus line via the transceiver,
  so a write is echoed back as an RX byte — that's what `Arbitration::data` compares. Confirm the
  meter wiring reproduces this (it does on .150's real adapter; it did NOT on the full-duplex harness —
  that's the whole reason the harness can't test this).

## 4. License / attribution — DECIDED: clean-room re-implementation
GitHub reports **no detected license** on `danielkucera/esp-arduino-ebus`, so we will **NOT copy its
source**. The eBUS arbitration protocol itself is public/spec-defined (SYN delimiter 0xAA; address must
land 4300–4456 µs after the SYN start bit per spec_test_1 v1.1.1 §3.2; wired-AND dominant-0 line; two
priority rounds keyed on the address low nibble; SYN symbol = 4167 µs at 2400 baud). We implement the
algorithm **from those spec facts**, in original code structured for xsns_53 (our own state names,
our `ebm_*`/`meter_ss` plumbing, our event hook) — reading the reference only confirmed the protocol,
not the implementation. No attribution obligation; clean to upstream to Gemu's GPLv3 fork.

## 5. Verification ladder — **.150 Wolf bus, READ-ONLY** (no writes to solar regs, ever)
.150's adapter is the **plain/direct-wired** type (Gate 0 proved it doesn't do enhanced) = the correct
HW for in-firmware arbitration, and it sits on a REAL eBUS with the Wolf master(s) → real collisions.
1. **Build** `USE_SML_EBUS_ARB` for .150's env (tinyc32c3-ebus-vm + the flag), OTA via the resized
   app0 (OtaUrl+Upgrade path that already works post-resize).
2. **Passive baseline** (arb idle): confirm Collector/clock still decode (regression).
3. **Arbitration dry-run**: arm a request but log only — watch `ARB START/WON1/LOST1/restart` counters
   across many SYN rounds. Success = we WIN sometimes and LOSE/retry cleanly, no bus disruption, no
   reboot, WiFi stays up.
4. **Active READ** (safe, read-only): `smlWrite` a master→slave **identification/read** query (e.g.
   `0704` to a slave addr, or a Wolf read service) — exercises win→send→slave-ACK+response→decode with
   ZERO write risk. Confirm the response lands in an SML slot.
5. **Scope check** (if reachable): logic analyzer on the bus to confirm our address lands in the
   4300–4456 µs window; tune `EBM_ARB_TX_LEAD_US` if off.
6. **Only after .150 is clean** → build for Andreas's C3 (.104); he supplies the exact QQ/ZZ/PB/SB/NN
   read service codes for T1.130 / HE-VL-RL and (ideally) scope traces. **Never** write a control reg.

## 6. Phased implementation order
1. Port `BusState` + `Arbitration` as a guarded header; compile-only on a C3 env. (no behaviour yet)
2. Wire the dedicated `onReceive`+RxFIFO=1 reader on the meter UART; `Ebm_OnSymbol` runs BusState/Arb
   + feeds `ebus_feed_byte`. `soA` opt-in. Disable SML_Poll drain for the arb meter.
3. On won1/won2 stream the staged telegram (from idx 1); lost/restart → retry next SYN; SML_Poll
   watchdog/timeout + stats. Tunable `EBM_ARB_TX_LEAD_US`.
4. Build + OTA .150; run the read-only ladder §5. Tune the lead constant on a scope.
5. License/attribution decision before any upstream PR.

## 6b. Andreas's live bus (.104) — concrete targets, from his validated catalog (2026-05-28)
NOTE: the opt-in shipped as **`soF`** (the planned `soA` collided with the USE_SML_DECRYPT option).
Andreas mailed a Vaillant register catalog **validated against the real aroTHERM + an active ebusd**,
which independently confirms this whole approach (§3.5/§4.0 of his doc: "Tasmota-SML is passive, the
TX-master half is missing... SYN-byte arbitration, lowest address wins, loser waits for next SYN +
retries, no bus crash" = exactly `Eba_OnSymbol`).

- **Device map (Slave = Master+5):** HMU(WP) `03/08` · sensoCOMFORT VRC720 `10/15` · VWZIO `71/76`
  · NETX2 internet-poller `f1/f6` (snoops most reads passively) · ebusd `31`. Six live masters.
- **Our master address (QQ) on .104: `73`** (also 77/7F) — verified **0× in a 1 h dump = free**.
  NOT 70/71 (71 = VWZIO, busy). eBUS master addresses must have the right bit-parity (00,01,03,07,0F,
  10,11,13,17,1F,30,31,33,37,3F,70,71,73,77,7F,F0,F1,F3,F7,FF).
- **Arbitration is inherently SAFE here:** Vaillant masters `03/10/31/f1` are all **lower than `73`**
  → they always win, Tasmota `73` yields + retries next slot → we can never pre-empt the heat-pump
  control. Re-verify `73` is free with a fresh `sensor53 d1` dump before each active session (a service
  tech could dock a tool on it).
- **b514 diagnostic read targets** (passively unreadable; need the active master). Telegram per his
  §4.4: `QQ=73 ZZ=08 B5 14 <ID> 03 FF FF` + CRC → slave replies; our firmware appends the CRC.
  **ZZ=08 (HMU) CONFIRMED** (2026-05-29, `08.hmu.csv:180`) — all 13 b514 reads in his 1 h dump go
  `31 → 08`, never to 76. The earlier "76" was a doc mix-up: slave 76 (VWZIO) carries the *energy*
  counters (`b51a 05ff3246/3249/324a` = op-hours / consumption / starts), a different register set,
  NOT the VL temp. Response-framing proof (sibling read, same ZZ=08):
  `aa 31 08 b514 05 0548 03ffff CRC 00 ‖ 04 4800 0000 CRC 00` — response byte0 echoes the register
  ID, bytes[0:2] are the two IGN-skipped bytes.
  | Diag | ID (after B5 14) | Decoder | Tasmota pattern | Access |
  |------|------------------|---------|-----------------|--------|
  | **T1.130 Heizstab-VL** | `0582` | **SIN**/10 °C | `x2ssSS@10` (IGN:2 then signed16/10) | direct read ✓ |
  | Heizstab-STB T.1.124 | `057c` | UCH 0/1 | `x2uu@1` | direct read ✓ |
  | HE-Vorlauf T.0.40 | `0528` | **SIN**/10 °C | `x2ssSS@10` | ⚠ enable-write first? |
  | HE-Rücklauf T.0.41 | `0529` | **SIN**/10 °C | `x2ssSS@10` | ⚠ enable-write first? |
  | HE-Durchfluss T.0.45 | `052b` | **UIN** l/h | `x2uuUU@1` | ⚠ enable-write first? |
  **Start the read ladder with `0582` (VL) or `057c` (STB)** — both bake `03ffff` into the read ID
  = a single direct telegram, the proven pattern. HE-VL/RL/flow (`0528/0529/052b`) are modelled in
  `08.hmu.csv` as an *enable-write* (`#w b514 0528 HEX:3=03FFFF`) **then** read pair; whether the bare
  `05 28 03 ff ff` read works without that prior write is unverified on the real bus (none appear in
  the dump). The enable-write is a HMU *test-register* write — defer it until the direct-read path is
  proven, and clear any such write with the user first (write-safety rule). Andreas offered a live
  on-wire `0582` capture + real temp (he'd briefly uncomment the `r`-line on his HA-ebusd .123,
  read-only) as a decode oracle — worth taking before we trust our own decode.
- **Cross-check oracle:** his HA-ebusd (read-only `find`/`info`, never `read`/`write`) at
  `192.168.56.123:8888` is ground-truth for comparing whatever our master decodes.
- **Phase-2 verification can run read-only on .104 directly** (in addition to .150) — because Tasmota@73
  yields to Vaillant, a read-only `b514` query cannot disrupt the live system. Andreas runs it.

## 7. Out of scope / deferred
- Writes to a real bus (clock-write on .150 only after read works, and even then optional — the user's
  rule is reads are the goal; the clock harness already proved the write encoder).
- S3/C6 arbitration (different TX-lead constant) — C3 is Andreas's target.
- The `soE` enhanced-client path stays for a hypothetical enhanced adapter; `soR` stays for the harness.

## 8. Diagnostics: `sensor53 d<n>` raw-sniff in soF/arb mode
In arb (`soF`) mode the `onReceive` RX-event task is the **sole** UART reader (draining in
`SML_Poll` too would double-consume bus symbols and wreck arbitration timing). `dump2log()` runs on
the main task and reads via `SML_SAVAILABLE`, so on an arb meter it sees an **empty buffer and prints
nothing** — `d1` looked dead from 30.05 (the `soF` date) onward (Andreas, .104, 11.06). It is **not**
a defect: an ebusd `grab` on the bus is a partial substitute (it only shows complete CRC-valid
telegrams — never the broken frames, discarded-CRC cases, escape raw-bytes or arbitration remnants you
want when debugging the gate/decode), and it needs an ebusd on the same bus.
**Fixed:** `ebus_feed_byte()` is the single choke point every arb byte passes through (incl. everything
the CRC gate later rejects), so `ebus_dump_arb_byte()` mirrors each raw byte into the dump log from
there — gated on `mp->ebm_arb` so a normal passive `'e'` meter still dumps via `dump2log()` unchanged.
Output is the same per-telegram `": aa <hex>"` format, flushed on the SYNC boundary. So `sensor53 d1`
shows the wire again in arb mode. (Also fixed in passing: the `": aa "` prefix reset used
`sizeof(log_data)` — but `log_data` is a `char*`, so `sizeof` is the pointer size (4) and `strlcpy`
truncated it to `": a"`; now bounded by the real `logsize`.)
