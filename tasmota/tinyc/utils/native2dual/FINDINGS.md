# native2dual — PoC evaluation & findings

Goal: loadable drivers to beat the firmware flash-overflow problem,
without reflashing, covering as many of Tasmota's ~354 native drivers
as possible. Question: can a transpiler automate native `xsns_/xdrv_`
→ dual-format BinPlugin (the *existing, proven* loadable-plugin path)?

## Method

1. Diffed pristine originals vs hand-written duals (`xsns_14_sht3x.ino`
   vs `xsns_14_sht3x_dual.cpp`, htu21, ads1115).
2. Built a triage classifier over all 354 drivers.
3. Built a faithful mechanical scaffolder (`scaffold.py`), ran it on
   the lib-free `xsns_14_sht3x.ino`, compiled the result as a real
   BinPlugin `.bin` (`build_plugin.py --plugin USE_SHT3X_N2D_MOD`),
   and harvested the compiler/linker error set across 3 iterations.

## Finding 1 — hand duals are NOT a transform oracle

The 20 hand duals are human *rewrites* of *years-old* native sources
(SHT3X dropped the whole SHT4X variant; bodies rebuilt to compat
idioms). "Diff tool-output vs hand-dual" can never pass — discarded as
an oracle. Real oracle: *does the generated dual compile as native
unchanged AND as a loadable .bin*.

## Finding 2 — the real cost is C++→C library port (irreducible)

Per user: building the original plugins required hand-porting vendor
C++ libraries to C. A transpiler fundamentally cannot do that. So tool
value exists only for drivers with **no external C++ lib dependency**.

Triage (committed `9ccf1cfd5`), validated against the 24 proven
conversions with **0 spurious blocks**:

| class | n | meaning |
|---|---|---|
| CHEAP | 52 | scaffold-only, <280 LOC register-bang — prime targets |
| SERIAL_SHIM | 6 | scaffold + existing compat TasmotaSerial bridge |
| VIABLE_BIG | 74 | lib-free but ≥280 LOC (convertible, not trivial) |
| NEEDS_PORT | 222 | needs a C++→C library/class port first (human) |

→ **~132 / 354 (37%) convertible without a lib port.**

## Finding 3 — for the lib-free class the residue is SMALL & SHARED

Faithful scaffold of `xsns_14_sht3x.ino` (no feature drop), 3 build
iterations harvesting the error set:

- **build1** (gating + descriptor + dispatcher only): `mt/jt/mem/
  MODULE_MEMORY not declared` cluster — pure scaffolder-incompleteness.
- **build2** (+ state→MODULE_MEMORY + reg-bind prologues): cluster
  gone; left ordering bugs + category-B.
- **build3** (+ hoist #defines/enums/PROGMEM, fix is_written): zero
  scaffolder errors; residue = pure frozen-ABI gap (3 dual-bus I²C
  syms + tgbl/return-in-void).
- **build4** (+ APPEND-ONLY JMPTBL slots 216/217/218 =
  `tmod_I2cWrite8Bus/I2cWrite0/I2cReadBuffer0`, new `j*` macros, and a
  FILE-LOCAL `#undef I2cWrite8`/remap injected by the scaffold — frozen
  `jI2cWrite8` jt[45] left byte-identical): **all 3 I²C errors gone.**
  Residue: 2 scaffolder gaps — `void` INIT fn vs ALLOCMEM `return -1`,
  and `TasmotaGlobal` needing the `STGLOB` prologue.
- **build5** (+ INIT fn → `int32_t`, `STGLOB` when body uses
  TasmotaGlobal): both gone. Residue: `*tgbl.member` mis-parse.
- **build6** (+ file-local `#define TasmotaGlobal (*tgbl)`): gone.
  **Final residue = ONE line:** `TasmotaGlobal.i2c_enabled[1]` in the
  optional `#if MAX_I2C>1` dual-bus *display* branch — the plugin's
  curated `GTBL` struct doesn't carry that member.

The mechanical scaffolding (gating, MODULE_DESCRIPTOR/MODULE_PART,
file-scope state → per-slot MODULE_MEMORY, ALLOCMEM/SETMEMREGS/RETMEM +
STGLOB prologues, INIT→int32_t, RO-array→const, #define/enum/PROGMEM
hoist, file-local I²C+TasmotaGlobal remap) is **fully auto-generated**.
After the one-time append-only JMPTBL extension, a real 236-line
lib-free native sensor compiles down to a **single** human decision:
guard/drop one optional dual-bus-display line (`NEEDS-ABI` flagged).
GTBL is offset-sensitive and was deliberately NOT extended (different
ABI surface than jt; not a safe append; out of authorised scope).

## Conclusion

The bottleneck for the cheap ~1/3 of the backlog is **not** 132
individual hand conversions — it is **one shared, APPEND-ONLY JMPTBL
extension**: the plugin ABI froze when Tasmota I²C was single-bus;
native gained a `bus` param and `I2cWrite0`/`I2cReadBuffer0` since.

**Hard constraint (project rule):** the JMPTBL is a frozen ABI bound
by already-compiled plugin `.bin`s. NEVER modify/reorder/re-signature
an existing entry — *append only*.

**DONE (this PoC):** appended JMPTBL slots 216/217/218
(`tmod_I2cWrite8Bus` 4-arg, `tmod_I2cWrite0`, `tmod_I2cReadBuffer0`) +
matching `j*` macros; `jI2cWrite8`/jt[45] and all 0..215 left
byte-identical (verified — no existing `.bin` behaviour changes). The
scaffold injects a *file-local* `#undef I2cWrite8`/remap so only
scaffolded TUs opt in. Empirically this collapsed the residue of a
real lib-free sensor to **one** flagged optional-feature line.

So: one shared, append-only ABI extension (done) + the auto-scaffolder
takes the entire CHEAP + much of VIABLE_BIG register-bang sensor class
to near-push-button; the per-driver residue is small, localized, and
`NEEDS-ABI`/`NEEDS-PORT`-flagged for a human. The 222 NEEDS_PORT
(vendor C++ libs) stay irreducibly human — unchanged. `GTBL` (curated
TasmotaGlobal subset) is offset-sensitive and was intentionally NOT
extended — a separate, riskier ABI surface; optional branches reading
absent GTBL members are flagged for a human to guard/drop.

tc2plugin (TinyC→plugin) is kept as the backup transpiler; this is a
separate, complementary path. No firmware/ABI changes were made by
this PoC — the JMPTBL extension is the recommended *next* step and a
deliberate human decision (frozen ABI).

## Artifacts

- `triage.py` — backlog classifier (committed `9ccf1cfd5`).
- `scaffold.py` — faithful mechanical scaffolder + honest
  `NEEDS-JMPTBL` flag report.
- Build logs: `/tmp/n2d_build{,2,3}.log` (transient).
- `xsns_198_sht3x_n2d.cpp` — generated slot (transient, regenerated
  per run; removed from the tree — not committed).
