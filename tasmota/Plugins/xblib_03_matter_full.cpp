/*
  xblib_03_matter_full.cpp - FULL matter_c amalgamation harness (Fork B, Stage 3).

  Purpose: stand up the plugin amalgamation BUILD so the discipline port of the 16
  matter_c sources can be verified step by step. This first cut just tries to
  COMPILE the CURRENT (still-un-disciplined) sources as a plugin - surfacing the
  compile-level work (libc mem-funcs and jt, include order, framework symbols, the
  MTRC_PLUGIN_BUILD / MODULE_MEMORY wiring). The relocation-level discipline
  (statics to ctx, literals to uconst, ...) is a separate, hardware-verified pass on
  top once this compiles. NOT a shipping plugin yet - mod_func_execute is minimal.

  Build:  python3 tasmota/Plugins/build_plugin.py --plugin USE_MATTER_FULL_MOD --cpu esp32
  Needs (already on the *-plugin envs): -I tasmota/Plugins/matter/include + the BearSSL src.

  Copyright (C) 2026  Gerhard Mutz / claude - GPL v3 (Tasmota's license)
*/

#include "tasmota_options.h"

#ifdef USE_MATTER_FULL_MOD

#define XBLIB_03  1
#define MTRC_PLUGIN_BUILD            // matter_c.c -> mtrc_plugin_mem.h (g via MODULE_MEMORY)

#include "module.h"
#include "module_defines.h"

PUSH_OPTIONS

// ── reg-bind seam ────────────────────────────────────────────────────────────
// Every matter function is MODULE_PART and carries a SETMEMREGS prologue
// (GET_MTBL; GET_JT; MODULE_MEMORY *mem = …) injected by lib2mod pass R — the
// idiomatic plugin pattern. So each binds, ONCE at entry (before any nested
// firmware call could flip the module register): `mt` → EXEC_OFFSET (PSTR/MP8),
// `jt` → the libc redirect (memcpy/snprintf/…), `mem` → the ctx (mtrc_plugin_mem
// .h). No global jt/mt macro — it would collide with SETMEMREGS' own jt/mt decls.

// The framework's OBJECT-like `#define millis jmillis` would expand the
// matter_port_t `millis` field decl AND every g.port.millis(...) HAL call (matter
// never calls raw millis()). Remove it for the matter region.
#undef millis

#define MP8(x) ((const uint8_t *)(x) + EXEC_OFFSET)   // relocate a const-array base ptr
#define MPT(T, x) ((const T *)((const uint8_t *)(x) + EXEC_OFFSET))  // typed variant
#define MP2D(T, x, cols) ((const T (*)[cols])((const uint8_t *)(x) + EXEC_OFFSET))  // 2D

// matter_c calls matter_special_malloc() for the PSRAM context. It is NOT a
// MODULE_PART matter fn (no SETMEMREGS prologue), so bind mt/jt here for the
// framework malloc redirect.
extern "C" void *matter_special_malloc(size_t n) { GET_MTBL; GET_JT; return malloc(n); }

// The BearSSL umbrella header (pulled by mtrc_crypto_ops.h) has inline functions
// (br_eax_*, br_ssl_*) that call memcpy/memset/… at FILE scope — outside any
// SETMEMREGS prologue, so the framework's jX redirect (which needs a local `jt`)
// won't compile there. Replace the mem* redirect with self-contained MODULE_PART
// byte-loop shims: no jt, no baked libc address — relocation-safe, callable from
// BearSSL inlines AND matter. (matter is not a hot path; crypto runs occasionally.)
#undef memcpy
#undef memmove
#undef memset
#undef memcmp
extern "C" MODULE_PART void *mtrc_memcpy(void *d, const void *s, size_t n) {
  uint8_t *D = (uint8_t *)d; const uint8_t *S = (const uint8_t *)s;
  while (n--) *D++ = *S++; return d;
}
extern "C" MODULE_PART void *mtrc_memmove(void *d, const void *s, size_t n) {
  uint8_t *D = (uint8_t *)d; const uint8_t *S = (const uint8_t *)s;
  if (D < S) { while (n--) *D++ = *S++; }
  else { D += n; S += n; while (n--) *--D = *--S; }
  return d;
}
extern "C" MODULE_PART void *mtrc_memset(void *d, int c, size_t n) {
  uint8_t *D = (uint8_t *)d; while (n--) *D++ = (uint8_t)c; return d;
}
extern "C" MODULE_PART int mtrc_memcmp(const void *a, const void *b, size_t n) {
  const uint8_t *A = (const uint8_t *)a, *B = (const uint8_t *)b;
  for (size_t i = 0; i < n; i++) if (A[i] != B[i]) return A[i] < B[i] ? -1 : 1;
  return 0;
}
#define memcpy  mtrc_memcpy
#define memmove mtrc_memmove
#define memset  mtrc_memset
#define memcmp  mtrc_memcmp

// ── amalgamate the 16 matter_c sources ───────────────────────────────────────
// matter_c.c FIRST: it defines matter_ctx_t + (via mtrc_plugin_mem.h) MODULE_MEMORY
// and the `g` macro that the other units reference.
#include "matter/src/matter_c.c"
#include "matter/src/mtrc_tlv.c"
#include "matter/src/mtrc_frame.c"
#include "matter/src/mtrc_crypto.c"
#include "matter/src/mtrc_sec.c"
#include "matter/src/mtrc_mrp.c"
#include "matter/src/mtrc_store.c"
#include "matter/src/mtrc_cert.c"
#include "matter/src/mtrc_csr.c"
#include "matter/src/mtrc_spake2p.c"
#include "matter/src/mtrc_pase.c"
#include "matter/src/mtrc_case_msg.c"
#include "matter/src/mtrc_case.c"
#include "matter/src/mtrc_dm.c"
#include "matter/src/mtrc_im.c"
#include "matter/src/qrcodegen.c"

#undef MP8   // matter-region only; the harness below doesn't use it
#undef MPT
#undef MP2D

// ── plugin descriptor ────────────────────────────────────────────────────────
// MODULE_MEMORY is now defined (by mtrc_plugin_mem.h inside matter_c.c) = the
// 4-byte ctx pointer. ALLOCMEM (at iniz) allocates it; matter_init later fills it.
MODULE_DESCRIPTOR("MTRFULL", MODULE_TYPE_BLIB, 1<<16|5, "", 0, "", 0, "", 0, "", 0)

MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END

// ── exports: retain + register the core matter API ───────────────────────────
// Referencing these in BLIB_EXPORTS keeps LTO from dead-stripping matter_c (their
// call graph covers ~the whole library) and lets the firmware resolve them via
// tc_blib_lookup later. REGISTERING them at iniz does NOT call them — this is the
// SAFE "does the full module load + relocate + register on .156" hardware test;
// executing matter (the relocation-discipline iterate-loop) comes after. Names are
// NON-static PROGMEM arrays (plugin discipline — inline literals crash).
const char NAME_M_INIT[]  PROGMEM = "matter_init";
const char NAME_M_START[] PROGMEM = "matter_start";
const char NAME_M_LOOP[]  PROGMEM = "matter_loop";
const char NAME_M_RESET[] PROGMEM = "matter_factory_reset";
const char NAME_M_UDPRX[] PROGMEM = "matter_udp_rx";
const char NAME_M_ADDEP[] PROGMEM = "matter_add_endpoint";
const char NAME_M_SETU[]  PROGMEM = "matter_set_attr_uint";
const char NAME_M_QRURI[] PROGMEM = "matter_qr_uri";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_M_INIT,  (void *)matter_init,          2, TC_RET_INT, { TC_ARG_BUF, TC_ARG_BUF, TC_ARG_END } },
  { NAME_M_START, (void *)matter_start,         0, TC_RET_INT, { TC_ARG_END } },
  { NAME_M_LOOP,  (void *)matter_loop,          0, TC_RET_INT, { TC_ARG_END } },
  { NAME_M_RESET, (void *)matter_factory_reset, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_M_UDPRX, (void *)matter_udp_rx,        4, TC_RET_INT, { TC_ARG_BUF, TC_ARG_INT, TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  { NAME_M_ADDEP, (void *)matter_add_endpoint,  1, TC_RET_INT, { TC_ARG_INT, TC_ARG_END } },
  { NAME_M_SETU,  (void *)matter_set_attr_uint, 4, TC_RET_INT, { TC_ARG_INT, TC_ARG_INT, TC_ARG_INT, TC_ARG_INT, TC_ARG_END } },
  { NAME_M_QRURI, (void *)matter_qr_uri,        0, TC_RET_INT, { TC_ARG_END } },
  { NULL, NULL, 0, 0, { 0 } }
};

int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    ALLOCMEM;                 // allocate MODULE_MEMORY (the ctx pointer holder)
    initialized = 1;
    return 1;
  }
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];   // loader EXEC_OFFSET-corrects + registers
  }
  // RETENTION ONLY — 0xFFFFFFFF is never a real selector, so this never executes,
  // but the compiler can't prove sel != that, so it keeps these CALLS — which keeps
  // LTO from dead-stripping the matter API + its whole call graph (a const table of
  // pointers, read only for its address, is NOT enough to retain them). Remove once
  // the firmware actually invokes the exports.
  if (sel == 0xFFFFFFFFu) {
    (void)matter_init(NULL, NULL); (void)matter_start(); matter_loop();
    (void)matter_factory_reset(); matter_udp_rx(NULL, 0, NULL, 0);
    (void)matter_add_endpoint(0); (void)matter_set_attr_uint(0, 0, 0, 0);
    (void)matter_qr_uri();
  }
  if (sel == pFUNC_DEINIT) {
    GET_MTBL;                 // RETMEM needs mt (+ jfree needs the local jt)
    GET_JT;
    RETMEM;
    return 1;
  }
  return 0;
}

PULL_OPTIONS

#endif  // USE_MATTER_FULL_MOD
