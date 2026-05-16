# tc2plugin — SHT31 live-test findings (2026-05-16, PAUSED)

Status: **paused for documentation.** Nothing committed (per standing
instruction: hold tc2plugin commits until "nearly complete"). The
firmware-side jt[215] edits remain uncommitted too — see end.

Test rig: `.39` = `192.168.188.39`, tasmota32s3-devkit, Tasmota
15.4.0.1, BuildDateTime 2026.05.16 11:35:05 (flashed in a prior
session — NOT reflashed this session; we only `/modu`-uploaded plugin
.bins). SHT31 physically on I2C @ 0x44, bus 1. Serial captured via
`/tmp/tc_serial_logger.py` → `/tmp/tc39_serial.log` (timestamped,
auto-reconnect across the reboot USB-CDC drop — this caught every
crash the HTTP `/cs` log polling missed).

Deploy pattern that works without the loader slot-churn trap:
`unlink 0` (all) → upload only SHT31 via `-F modu=@…` POST `/modu`
→ lands canonical slot 1 `0x42800000` → `iniz 1` (NOTE: bare `iniz`
has payload -99, a no-op; `Module_iniz` needs payload `0` or `1..MAX`).

## Bug 1 — addLog: two PSTR per line + dropped varargs  ✅ FIXED (live)

`tc2plugin_app.py` `call()` (~line 659). Old codegen:
`AddLog(LOG_LEVEL_INFO, PSTR("%s"), {args[0]})` — and the str-node
(`ex()`, `n=='str'`) already wraps every string literal in one
`PSTR()`. So `addLog("…0x%x…%d", a, b)` emitted
`AddLog(LOG_LEVEL_INFO, PSTR("%s"), PSTR("…0x%x…%d"))`: **2 PSTR per
line** (rule violation) + format mis-wrapped as `%s` + varargs
`a,b` silently dropped. Caused the original `.39` `LoadStoreError`
crash in `tc_main`'s addLog right after `tc_i2c_setdev`.

Fix (in place): branch on the first-arg AST node.
- first arg is `n=='str'` → it IS the (already single-PSTR) PROGMEM
  fmt; emit `AddLog(LOG_LEVEL_INFO, <args…>)` verbatim.
- otherwise (runtime `char[]`/expr) → keep our own single
  `PSTR("%s")` and pass the buffer as the `%s` value.

Live-verified: `12:26:12.668  SHT3X found at 0x44 on bus 0` printed
correctly, no Guru. 0 two-PSTR lines in the whole generated file
(also audited `snprintf_P`/`GetTextIndexed` paths — all single-PSTR).

## Bug 2 — FUNC_INIT never sets `initialized`  ✅ FIXED (live)

Firmware `Init_module()` (xdrv_123_plugins.ino:3678-3749)
deliberately does NOT set `modules[].flags.initialized` — the source
comment (3737-3744) states the plugin's own FUNC_INIT handler owns
that flag (`module_defines.h:591 #define initialized
mt->flags.initialized`). Hand-written duals set `initialized = true`
inside their ALLOCMEM'd `*_Detect()`. tc2plugin emitted
`case pFUNC_INIT: return tc_main();` — never set it → `init` stayed
0 → every `iniz` re-ran `tc_main` → repeated `sht_scan` → SWI2C
wire churn → "SHT3X not found" on the 2nd pass → device blocked.

False start: setting it via a 2nd standalone `{ SETREGS
initialized = 1; }` **in `mod_func_execute` after `tc_main()`
returned** → deterministic Exception 7 / EXCVADDR 0: `gettbl()`
there returned NULL → `mt->jt` / `*asettings` null-deref.

Fix (in place, `render_func`): set `initialized = 1` **inside
`tc_main`'s own `ALLOCMEM`-bound prologue scope** (mt freshly
valid — the exact place `Sht3x_Detect` sets it). For an `is_init`
fn lacking a reg-bind, force `ALLOCMEM`. Dispatch reverted to
`case pFUNC_INIT: return tc_main();`. Live-verified: periodic
callbacks (`fn=7/10/12/22 → tc_JsonCall`) now dispatch — they only
run when `flags.initialized` is set, so the fix is proven.

## Bug 3 — systemic EXEC_OFFSET mis-relocation of jt-routed calls  ❌ OPEN

Revealed only after Bug 2 (periodic callbacks finally ran). The
DBG_TRACE (`AddLog("TCDBG>…")` at every fn entry) localized it
exactly.

Crash A — full `sht31.tc`: last trace `TCDBG>tc_sht_calc_dewpoint`;
fault inside it. `tc_EverySecond`'s **inline** soft-float
(`fadd/fdiff/fmul/fdiv/tofloat`, lines 251-253) ran clean first →
soft-float jt bindings are OK. dewpoint differs only by `logf`
(jt-routed transcendental). Core1 PC `0x428663cc` "MMU entry fault
(invalid mmu entry)", backtrace `0x428663c9 ← 0x42800d9c`.

Per user direction, split off `examples/sht31_th.tc` (T+H only, no
`log`/`exp`); canonical `sht31.tc` untouched.

Crash B — `sht31_th.tc`: crash **moved** to after `tc_JsonCall`
(no transcendentals at all). `InstrFetchProhibited`, Core1:
```
PC : 0xbf3cc9ae     (wild)        A0 : 0x82800a61  → ret @0x42800a61 (plugin)
A8 : 0x8280087b  (plugin)         A9 : 0x0265ab00  ← == EXEC_OFFSET (mdir ex-offs)
```
Across all three crashes the bad PCs (`0x428663c9`, `0x7f3cc9ab`,
`0xbf3cc9ae`) keep the **same low bits**, high bits garbage, and
`A9` carries the plugin EXEC_OFFSET. Textbook signature of a call
target computed as `addr ± EXEC_OFFSET` with the relocation applied
where it must NOT be (jt[] entries point into *firmware* → no
EXEC_OFFSET) or omitted where it must (plugin-local pointer).

Not a jt-index skew: `module_defines.h` diff is **append-only**
(just `jexpf`→`jt[215]`, +5/−1, no renumber) and user confirmed
.39's 11:35 firmware already has `jt[215]`. So the defect is in
**how the Emitter relocates jt[]/EXEC_OFFSET call targets**, not
the table. Disabling functions only relocates the wild-jump to the
next jt-routed call (transcendentals → `ResponseAppend_P` /
`snprintf_P` float path). Soft-float survives by luck of layout.

### Recommended next step (not yet done)

Diff how the Emitter emits jt[]/EXEC_OFFSET-routed calls vs. a
known-good hand-written dual (`tasmota/Plugins/xsns_14_sht3x_dual.cpp`):
GSTR/FLTC/PSTR macro expansion, the soft-float `j*` wrappers, and
`ResponseAppend_P`/`snprintf_P` ABI routing. Find the site where
EXEC_OFFSET is added to a firmware-resident jt target (or not added
to a plugin-local one) and fix the codegen. THEN re-test
`sht31_th.tc` (T+H) before re-enabling transcendentals.

Optional de-risking before codegen work: reflash `.39` from the
exact current tree so plugin .bin and firmware are provably the
same `module_defines.h` (rules out any 11:35 divergence beyond the
append).

## Build/deploy provenance

- `build_plugin.py --plugin USE_SHT31_DUAL_CNV_MOD --cpu esp32`
  → `build_output/firmware/Plugins/ESP32/TENSILICA/SHT31_32.bin`.
- Bins this session: 3680 (Bug1 only) → 3724 (Bug2 false start,
  crashed) → 3700 (Bug2 correct, full sht31.tc, Crash A) → 2692
  (`sht31_th.tc`, T+H, Crash B; sha `f8dcf267…`).
- `.39` left with the 2692 B T+H bin resident at slot 1, `init:0`
  (boot does NOT auto-iniz → stable, no boot-loop). Re-test via
  `iniz 1` once codegen fixed.

## Uncommitted working-tree state (HOLD — do not commit yet)

- `tasmota/tinyc/utils/tc2plugin/` (whole dir, untracked) — incl.
  Bug1+Bug2 fixes in `tc2plugin_app.py`.
- `tasmota/tinyc/examples/sht31_th.tc` (new, untracked) — T+H test
  variant.
- `tasmota/Plugins/module_defines.h` (M) — `jexpf`→`jt[215]`.
- `tasmota/tasmota_xdrv_driver/xdrv_123_plugins.ino` (M) —
  `JMPTBL&tmod_expf //215` + `tmod_expf` wrapper (append-only).
- `tasmota/Plugins/xsns_198_sht31_cnv_dual.cpp` — generated slot
  (regenerated each build; transient).

## Session 2026-05-16 — translator PARKED (pivot to CAN-bus)

Bug 3 still ❌OPEN (untouched this session). Translator work paused
"until some other day" — user pivoted to CAN-bus bring-up (authentic
transceivers arrived). All below uncommitted in `tc2plugin_app.py`
(HOLD with the rest):

- **Right pane now editable** (`readonly` removed) + live syntax
  highlight on input; **Compile** already sent the right-pane buffer
  verbatim, so hand-edits are what build.
- **Save ▼ button** — client-side download of the right pane as
  `xsns_198_<mod>_cnv_dual.cpp` (server slot is auto-deleted by
  `do_compile`'s finally, hence client-side).
- **Restore ↩ button** — Compile overwrites the pane with the build
  log; Restore toggles back to the pre-compile (hand-edited) source
  and back to the log. Symmetric capture; cleared on fresh Translate.
- **Init contract implemented** — TinyC `main()` return value is the
  success signal (`>0` ok / `<=0` fail). Every `return` in the init
  fn is rewritten `{ int32_t _tc_ret=(expr); initialized=(_tc_ret>0);
  return _tc_ret; }`; prologue seeds fail-safe `initialized = 0`.
  `sht31_th.tc` `main()` updated to `return 1;`/`return 0;`. Plain
  `return` unaffected in non-init fns (verified). Canonical
  `sht31.tc` still returns 0 both paths — leave as-is.
- **Debug trace gated** — every `TCDBG>` AddLog wrapped in
  `#ifdef TC2PLUGIN_DBG`; macro ships commented out → default build
  silent; uncomment one line (or `-D TC2PLUGIN_DBG`) to re-enable, no
  re-translate. `DBG_TRACE` Python flag still controls scaffolding.
- **App default** `DEFAULT_NAME` `sht31.tc` → `sht31_th.tc` (the
  live-test variant; canonical has the Bug-3 transcendentals).
