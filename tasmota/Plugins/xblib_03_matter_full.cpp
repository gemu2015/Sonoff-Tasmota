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

// ── jumptable seam (the compile keystone) ────────────────────────────────────
// The framework #defines every libc/firmware call matter_c makes (memcpy/memset/
// memcmp/malloc/free/millis/strlen/strncpy/snprintf/strchr/…) to its jX macro,
// each of which expands to `jt[N]` — and `jt` is a LOCAL brought in only by
// SETMEMREGS, which matter_c's functions don't carry. Define `jt` itself as
// `(gettbl()->jt)` for the matter-include region: every jX macro then resolves
// the jumptable per call with no local needed, and matter uses the firmware's
// real (fast) memcpy/snprintf/…. Verified safe: no matter source uses `jt` as an
// identifier, and none uses snprintf's (void jsnprintf_P) return value. We #undef
// it after the matter region so the harness's own ALLOCMEM/GET_JT still works.
// (RUNTIME caveat for later: const string literals — snprintf formats etc. — still
// need the PROGMEM + EXEC_OFFSET discipline; this is compile-correctness only.)
#define jt (gettbl()->jt)

// The framework's OBJECT-like `#define millis jmillis` expands the matter_port_t
// `millis` field declaration AND every `g.port.millis(...)`/`port->millis` access
// (matter uses the HAL 12x and never calls raw millis() — the only `millis(` in
// the sources are in comments). Just remove the macro for the matter region.
#undef millis

// matter_c calls matter_special_malloc() (a firmware shim) for the PSRAM context;
// here it allocates through the framework malloc (now resolved via the jt define).
extern "C" void *matter_special_malloc(size_t n) { return malloc(n); }

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

#undef jt   // restore: the harness's own ALLOCMEM/GET_JT below need the real local jt

// ── plugin descriptor ────────────────────────────────────────────────────────
// MODULE_MEMORY is now defined (by mtrc_plugin_mem.h inside matter_c.c) = the
// 4-byte ctx pointer. ALLOCMEM (at iniz) allocates it; matter_init later fills it.
MODULE_DESCRIPTOR("MTRFULL", MODULE_TYPE_BLIB, 1<<16|5, "", 0, "", 0, "", 0, "", 0)

MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END

int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    ALLOCMEM;                 // allocate MODULE_MEMORY (the ctx pointer holder)
    initialized = 1;
    return 1;
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
