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
- **build3** (+ hoist #defines/enums/PROGMEM, fix is_written): **zero
  scaffolder errors remain.** Residue is *exactly* the frozen-ABI gap:

```
NEEDS-JMPTBL (3):
  I2cWrite8       frozen jI2cWrite8 is 3-arg; native 4-arg (…,bus) — drift
  I2cWrite0       not in plugin JMPTBL/compat (dual-bus reg write)
  I2cReadBuffer0  not in plugin JMPTBL/compat (dual-bus buffer read)
  (+ module_defines.h tgbl / return-in-void = compat-header ABI drift)
```

The mechanical scaffolding (gating, MODULE_DESCRIPTOR/MODULE_PART,
file-scope state → per-slot MODULE_MEMORY, ALLOCMEM/SETMEMREGS/RETMEM
prologues, RO-array→const, #define/enum/PROGMEM hoist) is **fully
auto-generated and compiles clean up to the ABI boundary**.

## Conclusion

The bottleneck for the cheap ~1/3 of the backlog is **not** 132
individual hand conversions — it is **one shared, addressable JMPTBL
modernisation**: the plugin ABI froze when Tasmota I²C was single-bus;
native gained a `bus` param and `I2cWrite0`/`I2cReadBuffer0` since.
Add/modernise those ~3–5 dual-bus I²C JMPTBL entries **once** and the
entire CHEAP + much of VIABLE_BIG register-bang sensor class becomes
near-push-button via `scaffold.py`. The 222 NEEDS_PORT (vendor C++
libs) stay irreducibly human — unchanged.

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
