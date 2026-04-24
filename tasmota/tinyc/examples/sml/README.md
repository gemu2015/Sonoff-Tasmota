Mini-Scripter-Compatible SML Descriptors
========================================

These four `.tas` descriptors were hand-rewritten from the upstream
`ottelo9/tasmota-sml-script` versions so they compile with the TinyC
mini-scripter subset (see `xdrv_124_tinyc_vm.h`, `tc_mscr_compile()`).
Upstream originals use full-Scripter features the subset does not cover:

  - `for … next` loops             → **decomposed into straight-line `sml()` calls**
  - `=#name` subroutines           → **inlined at the single call site**
  - `print …` diagnostic output    → **dropped** (no functional effect)
  - `mins` / `upsecs` / `tstamp`   → **replaced with an `lnv` tick counter**
                                      (the `>S` block fires at 1 Hz anyway)
  - `sb(tstamp …)` byte slice      → **dropped** (was redundant with adjacent branch)
  - `spin(pin, v)` / `spinm(pin,m)`→ **added to subset in v1.3.14** (native opcode)

Load them via TinyC's `smlScripterLoad("/path/to/descriptor.tas")`. On builds
with `-DTINYC_NO_SCRIPTER`, they keep the M-Bus / IR-read-head wake-up
handshake working without the full Scripter subsystem.

Files
-----

  - `allmess_wasseruhr.tas`      M-Bus water meter  — once-per-hour read
  - `engelmann_sensostar.tas`    M-Bus heat meter   — every ~45 min read
  - `itron_cf_echo_ii.tas`       M-Bus heat meter   — once-per-hour read
  - `easymeter_q3a.tas`          3-phase IR reader  — 60 s GPIO duty cycle

Notes
-----

- `TC_MSCR_HEX_MAX = 128` B → the ~2.2 s `0x55` wake preamble (530 B @ 2400 bd)
  is sent as 5 sequential `sml()` calls instead of `for lnvN 1 53 1 …`.
  Same wire effect, same timing (SML_Write is blocking).

- The `lnv` counter approach means the first fire is N ticks after boot,
  not phase-locked to wall-clock minute. For M-Bus heat/water meters this
  is irrelevant — what matters is the repeat interval.

- Pin placeholders like `%0txpin%` are substituted with numeric pin IDs
  by `smlApplyPins()` before compile. The EasyMeter Q3A example uses a
  literal `15` for self-contained testability; production descriptors
  should keep the placeholder.
