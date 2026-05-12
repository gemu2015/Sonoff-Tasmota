/*
 * dual_format_fltc.h — boilerplate-eliminator for the FLTC macro used
 * by dual-format drivers to read a single float out of a PROGMEM
 * float[] table.
 *
 * Caller pattern:
 *
 *     const float FP_CONST_BMP[] PROGMEM = { 0, 0.01, 0.00097656 };
 *     #define DUAL_FLTC_TABLE FP_CONST_BMP
 *     #include "dual_format_fltc.h"
 *
 * After the include, `FLTC(idx)` returns the idx'th float from
 * FP_CONST_BMP — the right way for the current build mode.
 *
 * Why two modes are needed
 * ------------------------
 * On ESP32-S3 plugin mode, direct `lsi` (load single-precision float)
 * from a PROGMEM float[] silently fails — the load instruction returns
 * garbage when the address lives in IROM. The workaround (same pattern
 * Tasmota firmware uses for plain `FP_CONST` in
 * tasmota/Plugins/module_defines.h:498-499): read the slot as
 * `volatile uint32_t` to force the compiler to emit `l32i`
 * (which DOES work on IROM), then memcpy the bit pattern into a
 * float local.
 *
 * Plain `FP_CONST_BMP[idx]` happens to work in contexts where the
 * compiler ends up casting to int (e.g. `(int)(FLTC(1)*1000)`) — in
 * float-arg-passing contexts (e.g. `fscale(x, FLTC(1), FLTC(0))`)
 * the compiler emits the unsafe lsi and corrupts the value to
 * something like 0xDD5F53FB. Reproducer + history: commit 5f8271d02.
 *
 * In native mode there's no plugin loader and no IROM trap, so the
 * macro reduces to a plain array index.
 *
 * EXEC_OFFSET is the plugin loader's runtime relocation offset for
 * PROGMEM symbols; in native mode it's defined as 0 by the compat
 * header.
 */

#ifndef DUAL_FLTC_TABLE
#  error "set #define DUAL_FLTC_TABLE <FP_CONST_array_name> before including dual_format_fltc.h"
#endif

#undef FLTC
#if BUILD_AS_PLUGIN
#  define FLTC(idx) ({                                                        \
      volatile uint32_t _tmp = ((const volatile uint32_t *)((char *)DUAL_FLTC_TABLE + EXEC_OFFSET))[(idx)]; \
      float _f;                                                               \
      __builtin_memcpy(&_f, (void *)&_tmp, 4);                                \
      _f;                                                                     \
    })
#else
#  define FLTC(idx)  (DUAL_FLTC_TABLE[(idx)])
#endif

/* CRITICAL: do NOT `#undef DUAL_FLTC_TABLE` here. The FLTC macro body
   references it; the preprocessor resolves that reference LAZILY at
   each FLTC use site, not at include time. Undef'ing now leaves
   FLTC's body pointing at a now-undefined token → "DUAL_FLTC_TABLE
   was not declared in this scope" at every callsite.

   For the rare multi-driver TU case (firmware co-link via merged
   .ino shims), each driver `#define`s DUAL_FLTC_TABLE before its
   include; the second include gets a redefine warning that's
   harmless because the FLTC bodies are already expanded for the
   earlier driver. To silence the warning, caller can `#undef
   DUAL_FLTC_TABLE` before the new `#define`. */
