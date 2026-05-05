# Unsolved problems — running dossier

Living document for hard problems we've been chipping at across multiple
sessions. Each entry collects everything known so far so a future
investigation (potentially with deeper context, more compute, or
hardware-side help) can pick up without re-deriving the basics.

**Maintenance rule:** when a problem is solved, MOVE its entry to the
top under a "Solved" section with a one-line summary linking to the
fixing commit, and rotate older solved items out into commit history
after a quarter or so. Keep this file under ~500 lines.

**Entry format:** Symptom → History → Hypothesis → Data → Files →
Next-steps → Open questions. Skip sections that don't apply.

---

## 1. Shine MP3 PIC plugin

**Status:** ⚠ partially fixed (linker order, ICONST narrowing identified
and patched 2026-02). Re-verify whether the encoder produces correct
output end-to-end on real hardware after the documented fixes; if not,
treat as still-broken and continue investigation.

### Symptom

Shine MP3 encoder shipped as a Tasmota PIC plugin (~82 KB compiled
Xtensa) crashed at runtime with **"invalid mmu entry"** when attempting
to read PROGMEM strings. Subsequent failure mode (after first fix):
ICONST(A) narrowing incorrectly truncated values >4095 on the Xtensa
target, producing wrong frame sizes / buffer offsets at runtime.

### History (what's known + what's been tried)

#### A. Linker section order — FIXED in patch_linker_file.py
- **Wrong order** (before fix): `… → mod_part → mod_string`. PROGMEM
  strings ended up beyond the flash-MMU mapping → "invalid mmu entry"
  crash on first PSTR access.
- **Correct order**: `mod_desc → mod_string → mod_part.literal →
  mod_part → mod_end`.
- **Why this matters**:
  - PSTR/PROGMEM strings must stay in low flash addresses (within MMU
    mapping).
  - Literal pools must be immediately before the code that references
    them (Xtensa `l32r` is PC-relative).
  - Growing `mod_string` shifts literal+code by the same delta, so the
    `l32r` offsets are preserved. Putting `mod_string` AFTER code would
    invalidate every l32r in the plugin.
- **File**: `tasmota/Plugins/patch_linker_file.py` lines 88–94.

#### B. ICONST(A) narrowing — FIXED via INTC(idx) workaround
- `ICONST(A)` on Xtensa compiles to `fixsfti(A)` which is **broken for
  values > 4095** (silently truncates / produces wrong int32).
- **Workaround**: use `INTC(idx)` against an `INT_CONST` PROGMEM array
  the linker prepares.
- **Shine specifics**: `INT_CONST` entries 14–19 hold sizeof values for
  internal structs.

### Current best hypothesis if still broken

After the two fixes above, Shine should encode correctly. If it still
fails, candidates:
1. A third location where ICONST is hit on a value >4095 that wasn't
   migrated to INTC. Audit Shine source for any constant ≥ 4096 that's
   used in expressions reaching ICONST.
2. Toolchain version drift — Xtensa GCC version after Tasmota updates
   may have changed `fixsfti`/`fixsfdi` behaviour. Check
   `xtensa-esp32-elf-gcc --version` against the version that produced
   the working .lo.
3. Static-init data not being relocated — Shine has a few large arrays
   (Huffman tables) that need to land in PROGMEM. If the patch_linker
   step doesn't catch them, they end up in RAM and inflate plugin size
   beyond what the loader expects.
4. Endianness or alignment trap — uncommon but possible if a struct
   layout assumption differs between host gcc (used to compute
   sizeof at PIC-build time) and target gcc.

### Data points / measurements (from prior runs)

- Compiled PIC code size: ~82 KB (Shine encoder, full)
- INT_CONST table: 6 entries (indices 14–19 specifically, sizeof of
  internal structs). Other indices unknown / unused.
- Runtime behaviour with old (broken) section order: crashed within
  a few hundred ms of first encoder call, addr2line pointed at a PSTR
  read.

### Files involved

| File | Role |
|---|---|
| `tasmota/Plugins/xdrv_14_mp3.cpp` | Tasmota driver hosting the plugin |
| `tasmota/Plugins/xdrv_14_mp3_test.cpp` | Test harness |
| `tasmota/Plugins/patch_linker_file.py` | Section reorder + INT_CONST table inject (lines 88–94) |
| `tasmota/Plugins/intrinsics.h` | INTC / ICONST macros |
| Shine encoder sources | (location: TBD — paste in dossier) |

### Suggested next steps for a deep-dive session

1. Verify current state: build a fresh `xdrv_14_mp3.bin`, flash to a
   test ESP32, run encoder on a known WAV file, compare output bytes
   against a reference encoded by host Shine.
2. If output is still wrong: dump the PIC plugin with
   `xtensa-esp32-elf-objdump -dCx out.lo` and inspect every site that
   loads a 32-bit constant. Confirm none use raw ICONST for values
   ≥ 4096.
3. If runtime crashes: addr2line the crash PC against the loaded
   plugin's relocated address space. Verify which section the PC
   landed in — should be `mod_part`, NOT `mod_string` or `mod_desc`.

### Open questions / requests for the user

- [ ] Where do the Shine encoder source files live exactly? Tasmota's
      tree, vendor-bundled, or external?
- [ ] What's the most recent symptom you're seeing? Crash, wrong
      output, or partial success that fails after N seconds?
- [ ] Do you have a reference reproducer (specific WAV/MP3 sample +
      expected output bytes)?
- [ ] What esp32 toolchain version does the working/broken build use?
      (`pio_default.ini` / `platformio.ini`)

---

## 2. New universal display driver — Waveshare epaper partial refresh broken

**Status:** ⚠ workaround in place (legacy library via PIO env), root
cause not identified. New `UDisplay` library (refactored by another
contributor) breaks partial refresh on Waveshare e-paper panels;
legacy `UDisplay_legacy` library still works.

### Symptom

- Universal Display driver was reworked into a modular structure (one
  file per panel type) by another contributor. Since that rework,
  Waveshare e-paper displays (gerhard's units) **don't refresh
  correctly with the new driver**. Specifically:
  - **Partial refresh does not work** — confirmed.
  - Whether full refresh also fails or "just" looks degraded is open.
- Workaround: build with the **legacy** library (`UDisplay_legacy/`)
  via a dedicated PIO env (`tinyc32c3-epd`, `tinyc32s3-epd`).

### History (what's been tried)

- Multiple attempts to update / port the partial-refresh path in the
  new modular driver — none succeeded.
- Currently shipping the legacy driver as a parallel option in
  `platformio_override.ini` (lines 134–138 + ~738+ for the EPD env
  overrides). Default builds use the new driver; epaper variants
  switch via `lib_ignore` swap.

### Current best hypothesis

The legacy and new drivers BOTH recognise the special epaper opcodes
`EP_LUT_FULL` (0x61), `EP_LUT_PARTIAL` (0x62), `EP_SEND_FRAME` (0x68),
etc. — confirmed by symbol-grep in both. But the IMPLEMENTATIONS
diverge in subtle ways:

```
Legacy (UDisplay_legacy/uDisplay.cpp:970)
    case EP_LUT_PARTIAL:
        SetLut(lut_partial);                           // direct member
        ep_update_mode = DISPLAY_INIT_PARTIAL;

New (UDisplay/src/uDisplay.cpp:1051)
    case EP_LUT_PARTIAL:
        epd->setLut(epd->cfg.lut_partial_data,         // through cfg ptr
                    epd->cfg.lutpsize);
        epd->setUpdateMode(DISPLAY_INIT_PARTIAL);
```

Likely root-cause candidates (highest priority first):

1. **`cfg.lut_partial_data` / `cfg.lutpsize` not populated during
   descriptor parse.** If the new driver's config-loader doesn't copy
   these fields from the descriptor's `:partial` section into the
   `EPDPanel::cfg` struct, the SetLut at runtime gets `(NULL, 0)` and
   silently does nothing. Partial refresh then runs against whatever
   LUT was last loaded (typically the full-refresh LUT) → wrong /
   no update.
2. **`updateFrame()` partial-mode dispatch silent no-op when
   `epc_part_cnt == 0`** (`UDisplay_EPD_panel.cpp:439`). Possible if
   partial-command list isn't parsed from the descriptor in the new
   loader, even though the LUT itself is.
3. **`ep_mode` not reaching the panel object.** New driver's switch
   at line 436 requires `cfg.ep_mode == 1 || == 3`. If the modular
   refactor split panel-init across files and `ep_mode` was forgotten
   in the cfg copy, the entire partial branch is unreachable.
4. **Timing / WAITIDLE differences.** New driver might not honour
   `EP_WAITIDLE` (0x63) at the right point in the sequence — Waveshare
   panels need explicit busy-wait between commands or the partial LUT
   gets clobbered.

### Data points

- New driver location: `lib/lib_display/UDisplay/` (modular, ~14
  files). New EPD-specific code: `src/uDisplay_EPD_panel.cpp` +
  `include/uDisplay_EPD_panel.h`.
- Legacy driver: `lib/lib_display/UDisplay_legacy/uDisplay.cpp` (single
  ~1500 line file, working).
- The new driver has compile-time tracing built in:
  `#define UDSP_EPD_TRACE 1` enables `AddLog`-style diagnostics in
  partial-refresh dispatch. Lines 427–432 + 1031–1037. Useful for the
  next investigation — just enable + reproduce + capture log.

### Files involved

| File | Role |
|---|---|
| `lib/lib_display/UDisplay/src/uDisplay.cpp` | Main descriptor parser + command dispatcher (new) |
| `lib/lib_display/UDisplay/src/uDisplay_EPD_panel.cpp` | EPD panel object — `updateFrame`, `setLut`, `displayFrame` (new) |
| `lib/lib_display/UDisplay/include/uDisplay_EPD_panel.h` | EPD panel cfg struct definition |
| `lib/lib_display/UDisplay/include/uDisplay_config.h` | EP_LUT_FULL / EP_LUT_PARTIAL / EP_SEND_FRAME constants |
| `lib/lib_display/UDisplay_legacy/uDisplay.cpp` | Legacy reference — known working partial refresh (lines 970–1456) |
| `platformio_override.ini` | Build-env switch (default: new; `*-epd`: legacy) |

### Suggested next steps

For a deep-dive session (deserves max-effort + 1M context — see the
docs guidance in §1 of this file):

1. **Enable `UDSP_EPD_TRACE`** in the new driver, build with the
   default (new) library, flash, attempt partial refresh, capture
   the AddLog output. The trace prints `epcoffs_part`, `epc_part_cnt`,
   and per-opcode dispatch — should immediately show which of the
   four hypotheses above is in play.
2. **Side-by-side cfg dump.** Add a one-time `AddLog` call in both
   driver's panel-init that prints `lut_partial_data ptr / lutpsize /
   epcoffs_part / epc_part_cnt / ep_mode` after descriptor parse.
   Compare same descriptor against legacy + new — anything that's
   non-zero in legacy and zero in new is the bug.
3. **Diff the descriptor parser sections.** The legacy parser has
   `lut_partial = (uint8_t*)malloc(lut_siz_partial)` at line 273 +
   per-byte fill at 589. Find the equivalent in the new parser
   (presumably split out of `uDisplay.cpp`). Verify both code paths
   actually run.
4. **Logic-analyzer the SPI traffic.** Capture the full command/data
   stream during a partial refresh on legacy (working) vs new
   (broken). The first byte that differs is the fault site. Fast
   disambiguation if the source-side comparison is ambiguous.

### Open questions

- [ ] Is the contributor who reworked the driver responsive? They'd
      know what got moved where during the refactor; a 5-minute Q&A
      with them is probably worth more than 5 hours of source diving.

### Specific scope (refined 2026-05-05)

- **Three Waveshare descriptors in tree**:
  - `tasmota/displaydesc/WS_epaper29_v1_display.ini` (2.9" v1, ep_mode=1)
  - `tasmota/displaydesc/WS_epaper29_v2_display.ini` (2.9" v2, ep_mode=1)
  - `tasmota/displaydesc/WS_epaper42_display.ini`    (4.2", ep_mode=2)
- **Tested with new driver:** only **WS_epaper29_v2** — partial refresh
  fails. v1 untested. 4.2 untested.
- **4.2 is multi-LUT (ep_mode=2) and only does full refresh anyway** —
  separate code path (`displayFrame_42()` in `uDisplay_EPD_panel.cpp:504`).
  Not relevant to the partial-refresh failure.

### Sharper hypothesis (after source comparison 2026-05-05)

**Most likely root cause:** the new driver expects the descriptor's
`:I` section to include an `EP_LUT_PARTIAL` (0x62) pseudo-opcode —
that's how it knows when to call `setLut(lut_partial_data)`. But
**WS_epaper29_v2's `:I` section does NOT contain EP_LUT_PARTIAL**.
The legacy driver worked because its partial-refresh code path had
a HARDCODED `SetLut(lut_partial)` call (legacy `uDisplay.cpp:1455`)
that ran regardless of what the descriptor said.

```
Legacy partial-init flow (uDisplay.cpp:1450-1456):
    if (p == DISPLAY_INIT_PARTIAL) {
        SetLut(lut_partial);          // ← hardcoded, no descriptor opcode needed
        Updateframe_EPD();
    }

New driver (uDisplay.cpp:1051-1054, EP_LUT_PARTIAL handler):
    case EP_LUT_PARTIAL:
        epd->setLut(epd->cfg.lut_partial_data, epd->cfg.lutpsize);
        epd->setUpdateMode(DISPLAY_INIT_PARTIAL);
        // ← only fires if descriptor's :I contains EP_LUT_PARTIAL (0x62)
```

Result: with the new driver, after `:I` runs, the panel is left
loaded with the FULL LUT (or no LUT, depending on chip state). The
first partial-refresh request then runs against the wrong LUT and
silently produces no visible update.

The new driver source comment at `uDisplay.cpp:1268-1270` actually
admits this assumption:

> "ep_mode 1/3 rely on :I's own trailing 24,0 + 66,0 + MASTER_ACTIVATION
>  + EP_LUT_PARTIAL to leave the chip ready for partial updates."

— but WS_epaper29_v2 (and probably v1) does not include EP_LUT_PARTIAL
in `:I`.

### Two equally valid fixes

**A. Driver-side (preferred — preserves all existing descriptors):**

In `lib/lib_display/UDisplay/src/uDisplay.cpp` around line 1271,
inside the `if (ep_mode == 2)` block (or alongside, for ep_mode 1/3):
mimic the legacy hardcoded behaviour by automatically calling
`epd->setLut(...)` and `epd->setUpdateMode(DISPLAY_INIT_PARTIAL)`
after `send_spi_cmds(0, dsp_ncmds)` for ep_mode 1/3 IF the descriptor
parsed a partial LUT. Roughly:

```cpp
} else if (ep_mode == 1 || ep_mode == 3) {
    if (epd->cfg.lut_partial_data && epd->cfg.lutpsize > 0) {
        epd->setLut(epd->cfg.lut_partial_data, epd->cfg.lutpsize);
        epd->setUpdateMode(DISPLAY_INIT_PARTIAL);
    }
}
```

This restores the legacy "auto-init to partial" semantics. All
existing descriptors that worked under legacy will work under the
new driver without modification.

**B. Descriptor-side (fragile — needs to update every existing file):**

Append an `EP_LUT_PARTIAL` pseudo-opcode to every Waveshare descriptor's
`:I` section:

```ini
:I
…existing init bytes…
62,0          # EP_LUT_PARTIAL — load partial LUT, set DISPLAY_INIT_PARTIAL
```

Risk: every user with a custom descriptor must edit it. Discoverable
only from this entry / source comment. Not portable.

### Recommended next step

Before patching: **enable the diagnostic trace** to confirm the LUT
isn't loading at runtime.

1. Add `#define UDSP_EPD_TRACE 1` near the top of
   `lib/lib_display/UDisplay/src/uDisplay_EPD_panel.cpp` and
   `lib/lib_display/UDisplay/src/uDisplay.cpp`. (Already-existing
   gates around `AddLog` calls fire at LOG_LEVEL_INFO.)
2. Build + flash a 2.9" v2 device with the new driver.
3. Trigger a partial refresh, capture WebLog or serial log.
4. Look for `"setLut SKIP lut=0 len=0"` (= LUT never loaded), or
   absence of any `setLut cmd=…` line during the partial sequence.

If the log confirms the hypothesis, apply fix A (10 lines, low risk),
verify partial refresh works on .v2, then test v1 + 4.2 to make sure
nothing else broke.

### Open questions

- [ ] Did the trace confirm `setLut SKIP` during the partial-refresh
      attempt?
- [ ] Should the new driver behave identically to legacy by default,
      OR is moving the LUT-init-into-descriptor an intentional
      design choice we should respect (and update descriptors)?

---

## 3. Heat-pump device .31 multi-minute hangs

**Status:** ⚠ root cause narrowed to Tasmota loop task (NOT TinyC).
Watchdog auto-recovers within ~5 min via `Restart 1`. Underlying cause
identification deferred — needs Tasmota-side instrumentation, beyond
TinyC scope.

### Symptom

Device .31 (EPD-47, runs `examples/heatpump_map.tc` to sniff the
heat-pump's Modbus-RTU bus) freezes for 4+ minutes, becoming completely
unresponsive on HTTP. Eventually self-recovers via the `heatpump_map.tc`
TaskLoop watchdog forcing `Restart 1` — but the actual reboot happens
only AFTER the loop task unsticks enough to process the queued command
(observed: 8–13 watchdog re-fires every ~5.6 s within a single hang
event before the device finally reboots).

### History

#### Phase 1 — initial discovery
Originally suspected the `/tc` page synchronous HTTPS GET to GitHub
(commit `af67aedd4`, 2026-05-04). Fixed: added 60 s cache + 2 s
connect/read timeout. Worst-case loop block per `/tc` visit dropped
from ~50 s to ~2 s. Did NOT fix the multi-minute freezes.

#### Phase 2 — TinyC instrumentation in heatpump_map.tc
Added (commit `0ff139a96`):
- `cb_active_id` — set by Every50ms / EverySecond / WebCall / JsonCall
  at entry, cleared at exit. Identifies which TinyC callback was
  running when the watchdog fired.
- Per-callback peak counters (`cb_50_max_ms`, `cb_sec_max_ms`,
  `cb_web_max_ms`, `cb_json_max_ms`).
- Persist ring buffer of last 8 trips: silent_ms, uptime_s, active_cb,
  per-callback peaks.
- TaskLoop-driven watchdog: forces `Restart 1` if Every50ms hasn't
  ticked in 20 s.
- Lower threshold: 60 s → 20 s so daytime 30–60 s hangs trip the ring
  buffer instead of self-recovering invisibly.

Result: confirmed `cb_active_id = 0` (no TinyC callback running) on
ALL trips, and per-callback peaks all healthy (`e50=14ms, e1s=584ms,
web=10ms, json=5ms, mb=0`). TinyC is innocent.

#### Phase 3 — TaskLoop liveness probe
Added `last_task_loop_run_ms` global, set at top of TaskLoop body.
Trip handler captures `tlms = millis() - last_task_loop_run_ms` into
the ring buffer.

Result: `tlms = 0` for ALL trips → TaskLoop is alive in `tc_vm_task`
while the freeze is in progress, only the loop task is hung. Conclusively
rules out "TinyC syscall holding vm_mutex".

### Current best hypothesis

The freeze is in **Tasmota's loop task**, in non-TinyC code. With MQTT
disabled on this device, candidates:

1. **WiFi reconnect cascade** at marginal RSSI — observed -47 to -82
   dBm range. A brief disconnect at marginal signal triggers reconnect
   attempts, and at the same moment some Tasmota subsystem doing a
   synchronous lwIP socket op blocks for the OS-level timeout window.
2. **Heavy-serial-traffic-induced** something. The user noted .31 is
   the only device with this issue, and the only device with heavy
   serial traffic (Modbus-RTU 19200 8N2 from heat-pump cloud client).
   The 457 ms `e50` peak observed during the morning-hang event is a
   smoking gun for SOMEthing. After the `parse_frame()` fast-path fix
   (skip embedded-write scan for pure FC03 frames) + per-tick byte cap
   (192 bytes/Every50ms), the freezes continue.
3. Some periodic Tasmota driver task — Driver 10 (Scripter) is loaded
   on .31 even though unused; FUNC_LOOP runs every iteration.
4. Daily NTP / DHCP renewal stalling on the marginal connection.

### Data points

| Metric | Value |
|---|---|
| Hang duration | 200–270 s typical (one event = 4 min 8 s longest) |
| Watchdog re-fires per event | 8–13 (every ~5.6 s = one full trip-handler iteration) |
| `cb_active_id` at trip | 0 in every captured ring slot (37+ samples) |
| `tlms` at trip | 0 in every slot — TaskLoop unaffected |
| Per-callback peaks | All within healthy range, never the cause |
| BootCount delta per event | +1 (the watchdog DOES eventually reboot) |
| Wall-clock pattern | Morning hangs at 06:46 + 07:53 (one .pvs sample); other times unclear — needs longer logging |
| RSSI when hanging | -47 to -82 dBm (varies day to day) |

### Files involved

| File | Role |
|---|---|
| `tasmota/tinyc/examples/heatpump_map.tc` | Sniffer + ring-buffer watchdog (~1900 lines) |
| `tasmota/include/xdrv_124_tinyc_vm.h` | TinyC VM internals (TaskLoop, persist) |
| `tasmota/tasmota_xdrv_driver/xdrv_124_tinyc.ino` | TinyC driver glue |
| _(unknown — prime suspect)_ | The Tasmota driver actually causing the freeze |

### Suggested next steps

The remaining work is **firmware-side**, requires Tasmota build/flash
cycle, and is too risky to tackle while .31 runs the heat-pump in
production. Options:

1. **Log-only minimal-impact instrumentation:** add `EVERY_LOOP`
   timing into Tasmota's main loop dispatcher so each FUNC_*-call
   that exceeds N ms gets logged with the driver/index. Compile a
   special build, flash to .31 once, capture a multi-day log,
   correlate freezes with which driver function ran long.
2. **Disable Scripter (driver 10)** — even though the user doesn't
   use it on .31, disabling it removes one code path. Build with
   `#undef USE_SCRIPT` for .31's firmware and observe.
3. **Pin Wi-Fi power to max** — `WifiPower 19.5` (currently default
   17). May reduce reconnect frequency on marginal links.
4. **Replace .31's antenna** if hardware can be modified — quickest
   "is it the radio environment" test.

### Open questions

- [ ] Are there OTHER devices in the house with comparably heavy
      serial traffic that DON'T hang? If yes, .31 isn't unique-by-
      traffic-volume — must be device-specific (config, hardware,
      placement).
- [ ] Has .31's hang frequency / duration changed across Tasmota
      version bumps? Could narrow the regression window.
- [ ] Would a JTAG / ESP-PROG attached during the hang be useful, or
      is the device too physically inaccessible?

---

<!-- New entries go above this line. Keep entries roughly in the order
     of how often we revisit them. -->
