/*
 * dual_format_native_state.h — boilerplate-eliminator for the native
 * (non-plugin) side of a dual-format driver.
 *
 * Background. Every dual-format driver spells out the same shape:
 *
 *   #if !BUILD_AS_PLUGIN
 *     static <T> *<name>_state = nullptr;
 *   #  undef  SETREGS
 *   #  define SETREGS    MODULE_MEMORY *mem = <name>_state;
 *   #  define ALLOCMEM   if (!<name>_state) <name>_state = ...calloc...
 *                        if (!<name>_state) return -1;
 *                        MODULE_MEMORY *mem = <name>_state;
 *   #  define RETMEM     if (<name>_state) { free(...); <name>_state = nullptr; }
 *   #  define initialized mem->initialized_flag
 *   #endif
 *
 * The only thing that varies is the <name> prefix and the struct type
 * <T>. With ~20 dual drivers in tree, that's ~200 lines of identical
 * code with one canonical place to change if the shape ever needs to
 * evolve (e.g. switching calloc → heap_caps_malloc, or rebinding the
 * on-fail return path).
 *
 * Usage — caller pattern (include BEFORE the MODULE_MEMORY typedef):
 *
 *     #define DUAL_NATIVE_NAME    bme
 *     #define DUAL_NATIVE_STATE_T bme_state_t
 *     #include "dual_format_native_state.h"
 *
 *     typedef struct {...} MODULE_MEMORY;
 *
 *     #define temp mem->temp
 *     // ...accessor macros...
 *
 *     #if !BUILD_AS_PLUGIN
 *       static bme_state_t *bme_state = nullptr;
 *     #endif
 *
 * The header is unconditional; it gates the native-only `#define`s
 * with `#if !BUILD_AS_PLUGIN` internally so it's safe to include in
 * both modes. In native mode it also aliases MODULE_MEMORY to
 * DUAL_NATIVE_STATE_T so the typedef declaration that follows reads
 * `typedef struct {...} bme_state_t;`. In plugin mode MODULE_MEMORY
 * is whatever the plugin loader's headers define (and is gated out
 * via `#if BUILD_AS_PLUGIN` in module_defines.h).
 *
 * The header rebinds SETREGS / ALLOCMEM / RETMEM / `initialized` for
 * native ownership, then self-cleans (undefines DUAL_NATIVE_NAME etc.)
 * so the next driver in the merged firmware TU can include the header
 * again with different arguments. The state-pointer declaration stays
 * with the caller — it's `static` and per-TU, not parameterisable via
 * include.
 *
 * Why not a function-style macro: `#undef` and `#define` are PP
 * directives that cannot live inside a macro expansion. The
 * set-a-name-then-include trick is the canonical workaround for
 * parameterised PP-code generation in C.
 *
 * Why `initialized` is redefined: plugin-mode's module_defines.h
 * already defines `initialized` to `mt->flags.initialized` (loader-
 * gated dispatch). In native mode we own the state and bind it to
 * `mem->initialized_flag` instead. The order matters: native include
 * must come AFTER any plugin-side header that might define
 * `initialized` first.
 *
 * Co-link caveat (unchanged by this refactor): when multiple
 * macro-accessor-pattern duals merge into the firmware's
 * tasmota.ino.cpp, the per-driver `#define field mem->field` accessor
 * blocks still need their end-of-file `#undef` cleanup — that's
 * separate from the SETREGS/ALLOCMEM/RETMEM concerns this header
 * factors out.
 */

#ifndef DUAL_NATIVE_NAME
#  error "set #define DUAL_NATIVE_NAME <prefix> before including dual_format_native_state.h"
#endif
#ifndef DUAL_NATIVE_STATE_T
#  error "set #define DUAL_NATIVE_STATE_T <type> before including dual_format_native_state.h"
#endif

#if !BUILD_AS_PLUGIN

/* MODULE_MEMORY → driver's state-struct typedef. The typedef that
   follows (`typedef struct {...} MODULE_MEMORY;`) is then read as
   `typedef struct {...} bme_state_t;` (or whatever the caller
   passed). In plugin mode this is gated out — plugin's MODULE_MEMORY
   comes from the loader-side module_defines.h. */
#undef MODULE_MEMORY
#define MODULE_MEMORY DUAL_NATIVE_STATE_T

/* Two-step indirection so that DUAL_NATIVE_NAME (itself a macro) is
   fully expanded before being pasted into the identifier. */
#define DUAL_PASTE_(a,b) a##b
#define DUAL_PASTE(a,b)  DUAL_PASTE_(a,b)
#define DUAL_NATIVE_PTR  DUAL_PASTE(DUAL_NATIVE_NAME, _state)

#undef SETREGS
#define SETREGS    MODULE_MEMORY *mem = DUAL_NATIVE_PTR;

/* Some drivers (in-function `MODULE_MEMORY *mem` pattern, e.g. LTR308
   and Bresser) also distinguish a SETMEMREGS variant for callsites
   that need `mem` available in non-SETREGS contexts. Same shape —
   bind to the same global state pointer. Drivers that don't use
   SETMEMREGS at all simply never reference this macro. */
#undef SETMEMREGS
#define SETMEMREGS MODULE_MEMORY *mem = DUAL_NATIVE_PTR;

#undef ALLOCMEM
#define ALLOCMEM                                                              \
    if (!DUAL_NATIVE_PTR)                                                     \
        DUAL_NATIVE_PTR = (DUAL_NATIVE_STATE_T *)calloc(1, sizeof(DUAL_NATIVE_STATE_T)); \
    if (!DUAL_NATIVE_PTR) return -1;                                          \
    MODULE_MEMORY *mem = DUAL_NATIVE_PTR;

#undef RETMEM
#define RETMEM                                                                \
    if (DUAL_NATIVE_PTR) { free(DUAL_NATIVE_PTR); DUAL_NATIVE_PTR = nullptr; }

#undef initialized
#define initialized  mem->initialized_flag

/* Caller invokes DUAL_NATIVE_STATE_PTR_DECL exactly once AFTER the
   MODULE_MEMORY typedef (so the type is complete) — typically inside
   the same `#if !BUILD_AS_PLUGIN` block as the XSNS_NN / XI2C_NN
   defines. Expands to the static state-pointer declaration with
   driver-specific name and type baked in via DUAL_PASTE. */
#undef DUAL_NATIVE_STATE_PTR_DECL
#define DUAL_NATIVE_STATE_PTR_DECL  static DUAL_NATIVE_STATE_T *DUAL_NATIVE_PTR = nullptr;

#endif  /* !BUILD_AS_PLUGIN */

/* In plugin mode the pointer-decl macro is a no-op so caller code
   that uses it unconditionally compiles cleanly in both builds. */
#if BUILD_AS_PLUGIN
#  undef DUAL_NATIVE_STATE_PTR_DECL
#  define DUAL_NATIVE_STATE_PTR_DECL  /* (empty in plugin mode) */
#endif

/* CRITICAL: do NOT `#undef` DUAL_NATIVE_NAME, DUAL_NATIVE_STATE_T,
   DUAL_NATIVE_PTR, DUAL_PASTE, or DUAL_PASTE_ here. The
   SETREGS/ALLOCMEM/RETMEM/initialized macros reference these LAZILY
   at each use site, not at include time. Undef'ing now leaves the
   bodies pointing at undefined tokens → "DUAL_NATIVE_PTR was not
   declared in this scope" at every SETREGS callsite.

   For the rare multi-driver TU case (firmware co-link via merged
   .ino shims), each driver `#define`s these before its include;
   the second include gets redefine warnings that are harmless
   because the earlier driver's macro bodies are already expanded.
   To silence the warnings, caller can `#undef DUAL_NATIVE_NAME`
   and `#undef DUAL_NATIVE_STATE_T` before the new `#define`s. */
