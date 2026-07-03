# SML driver extensions (xsns_53) — interim notes

Small note for the new SML descriptor features added 2026-05. Fold into the main
SML / TinyC-Reference docs later. All changes live in `xsns_53_sml.ino`.

---

## 1. eBUS BCD date/time decoder — `ETIME` / `EDATE`

Decode the BCD time and date out of an eBUS telegram (e.g. the `07 00`
"date/time" broadcast) into plain **integers**, so they fit the SML numeric
model (no string slot needed).

| Token | Reads | Output integer | Example | Max |
|-------|-------|----------------|---------|-----|
| `ETIME` | 3 BCD bytes `HH MM SS` | `HHMMSS` | `10:20:11` → `102011` | 235959 |
| `EDATE` | 4 BCD bytes `DD MM WD YY` (day-first, weekday skipped) | `YYMMDD` | `2026-05-28` → `260528` | 991231 |

**Why split date and time** (and not one combined `YYYYMMDDHHMM`): SML keeps
values as `double` internally, but **TinyC `smlGet()` hands them back as a
`float`** (~2²² ≈ 4.2 M exact). A 12-digit combined timestamp would be mangled
on export. `HHMMSS` and `YYMMDD` both stay well under 2²², so they round-trip
exactly. (The century is dropped from the date for the same reason —
`YYYYMMDD` = 20.2 M already overflows float.)

**Byte order** follows the eBUS standard (cross-checked against ebusd's
datatypes): time `BTI` is `BCD|REV` → hours first (`HH MM SS`); date `BDA` is
`BCD`, day first (`DD MM WD YY`). Each token reads from the current match
position, so position it with wildcards.

### eBUS `07 00` broadcast layout
`QQ ZZ 07 00 NN | temp(2) | time(3) | date(4)` — time starts at byte index 7,
date at index 10.

### Descriptor example (Wolf/Vaillant `0f fe 0700` clock broadcast)
```
1,xxxx0700xxxxxxETIME@1,Uhrzeit,,ClockTime,0
1,xxxx0700xxxxxxxxxxxxEDATE@1,Datum,,ClockDate,0
```
- `xxxx` = QQ+ZZ, `0700` = service literal.
- `xxxxxx` (6 wildcard nibbles = 3 bytes: NN+temp) puts the cursor at the time → `ETIME`.
- `xxxxxxxxxxxx` (12 = 6 bytes: NN+temp+time) puts the cursor at the date → `EDATE`.
- `@1` = identity scale (see §2 — the `@` is mandatory).

Verified live: `ClockDate 260528` (correct date), `ClockTime 102011`.

---

## 2. Robustness: a value line **must** end its pattern with an `@` scale

The decode matcher walks the pattern until the `@` scale marker. A value line
**without** `@` (e.g. `...uu,Name`) previously ran the parser off the end of
the descriptor string → out-of-bounds read → crash on boot → **boot loop**.

Now: the match loop is bounded at end-of-string, and a missing `@` makes the
line a clean non-match (logged once: `SML: a decoder line is missing its '@'
scale — line ignored`). A malformed line is skipped, never fatal.

**Takeaway for descriptor authors:** always terminate a value pattern with a
scale, e.g. `@1` for "no scaling". `bcd2@1`, `ETIME@1`, `ssSS@16`, `uu@b0:1`.

---

## 3. Active eBUS master — `soE` (enhanced adapter) / `soR` (raw test)

All behind `#ifdef USE_SML_EBUS_MASTER`; non-enhanced `'e'` meters are
byte-identical when the flag is off. The write path reuses the existing
`smlWrite(meter, "QQ ZZ PB SB NN data…")` builtin — **no new TinyC syscalls**.
The firmware appends the eBUS CRC8 (poly `0x9B`), AA/A9-escapes the telegram,
and the slave reply (if any) decodes through the normal meter-def matcher.

Two opt-in modes, selected per meter in the descriptor:

| Option | Mode | Arbitration | Use |
|--------|------|-------------|-----|
| `1,=soE[,QQ]` | ebusd **enhanced protocol** | done by the adapter (adapter.ebusd.eu v3.1/C6) | real multi-master bus |
| `1,=soR[,QQ]` | **raw test mode** | none (blind-tx on next bus SYN) | direct UART harness, no real bus |

`QQ` = our master address (default `0xFF`); in raw mode the QQ actually sent
comes from the `smlWrite` hex, not this option.

- **`soE`** drives the adapter with the ebusd enhanced 2-byte symbol protocol
  (`INIT`/`SEND`/`START`/`INFO` ↔ `RESETTED`/`RECEIVED`/`STARTED`/`FAILED`…), so
  the timing-critical 2400-baud arbitration is delegated to the adapter's PIC/C6.
  A non-blocking state machine (`Ebm_Step`, stepped per-tick and event-driven)
  spreads a round over many 50 ms ticks — the main loop never blocks. If the
  adapter never answers `INIT`, it warns and falls back to passive read.
- **`soR`** skips arbitration entirely: on each received bus SYN (`0xAA`) it
  blind-transmits one staged telegram. Meant for a **full-duplex Mac↔ESP UART
  harness** (no real eBUS electrics), driven against the SML-emulator's "EBus
  responder" mode — exercises framing, CRC, escaping and the whole send path
  without a real device.

**Verified 2026-05 on `.39` (S3) ↔ Mac UART + SML-emulator, no real Wolf:**
- READ — emulator repo-replays the Wolf solar descriptor → `.39` decodes
  Collector/Storage/Pump **and** the `0700` clock (`ETIME`/`EDATE`).
- WRITE — `smlWrite` a `0f fe 0700 …` clock broadcast → emulator receives,
  CRC-validates and decodes `CLOCK 28.05.26 12:34:56` (CRC `9a`).

Safe target by design: only the eBUS clock broadcast is written in testing — no
solar/control register is ever touched. See the project memory note
`ebus_active_master_xsns53` for the full status and the live-adapter (`soE`) gate.

## 4. `/sml_meter.def` accepts `;` comments and blank lines

The SML descriptor grammar itself has no comment syntax. Under Scripter the `>M`
section is pre-stripped, but a **file-based** descriptor (`USE_SML_M`, filesystem
`/sml_meter.def`) is parsed raw. `SML_Init` now strips, in-place at load, any line
whose first non-blank char is `;` plus blank lines (and `\r` from CRLF-saved files),
so a hand-written `.def` can be annotated and spaced for readability. It runs before
the parse, so a `;` line placed *between* comma-continuation lines of a long Modbus
register list is removed first and can't break the continuation. Scripter-fed
descriptors are unaffected (they never hit the file branch).
