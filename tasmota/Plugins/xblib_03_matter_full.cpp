/*
  xblib_03_matter_full.cpp — FULL matter_c amalgamation as a BinPlugin (Fork B, BLIB).

  Stage 3 of the matter_c-as-plugin work (see matter/PLUGIN_PLAN.md). Where
  xblib_02 is the proven probe + crypto-seam de-risk, THIS file amalgamates the
  whole 16-file matter_c into one relocatable BLIB:

      python3 tasmota/Plugins/build_plugin.py --plugin USE_MATTER_FULL_MOD --cpu esp32
      python3 tasmota/Plugins/build_plugin.py --plugin USE_MATTER_FULL_MOD --cpu esp32_riscv

  The plugin discipline (from matter/PLUGIN_PLAN.md + the Stage-2 findings):
    - NO static/file-scope mutable data → the few matter_c globals (g_ptr, g_cr,
      g_qr_ok, g_tx, g_fab[]) move into MODULE_MEMORY / matter_ctx_t.
    - libc is remapped to the framework jumptable by mtrc_plugin_libc.h (the
      gettbl() global accessor → no per-function SETMEMREGS). ⚠ jt[22] snprintf
      returns VOID → any `int n = snprintf(...)` must drop the return.
    - const tables / string literals → PROGMEM; inline literals ≥2048 → pico_uconst;
      float ops → fdiv/fmul; no 64-bit / % intrinsics.
  Build env (gitignored, main checkout): the *-plugin envs already carry
  -I matter/include, -I bearssl src, -fpermissive, -fno-lto, -fno-merge-constants.

  Copyright (C) 2026  Gerhard Mutz / claude  —  GPL v3 (Tasmota's license)
*/

#include "tasmota_options.h"

#ifdef USE_MATTER_FULL_MOD

#define XBLIB_03            1
#define MTRC_PLUGIN_BUILD   1   // gates matter_c.c's MODULE_MEMORY keystone include

#include "module.h"
#include "module_defines.h"

PUSH_OPTIONS

// THE amalgamation enabler: remap libc (memcpy/memset/snprintf/…) to the module
// jumptable via gettbl(), so all 16 matter_c sources amalgamate without adding
// SETMEMREGS to each function. Must come after module_defines.h (whose local-jt
// remaps it #undefs) and before the matter sources.
#include "mtrc_plugin_libc.h"

// The descriptor references mod_func_execute (our dispatch). Forward-declare it.
MODULE_PART int32_t mod_func_execute(uint32_t sel);

// ─── matter_c amalgamation ───────────────────────────────────────────────────
// matter_c.c FIRST: it defines matter_ctx_t + pulls mtrc_plugin_mem.h (→ defines
// MODULE_MEMORY) + the `g`/`g_ptr` macros that the mtrc_*.c bodies below rely on.
// The mtrc_*.c headers are already included by matter_c.c; including the .c files
// here supplies their definitions in the same TU (unity build).
#include "matter/src/matter_c.c"
#include "matter/src/mtrc_tlv.c"
#include "matter/src/mtrc_frame.c"
#include "matter/src/mtrc_crypto.c"
#include "matter/src/mtrc_sec.c"
#include "matter/src/mtrc_spake2p.c"
#include "matter/src/mtrc_pase.c"
#include "matter/src/mtrc_case.c"
#include "matter/src/mtrc_case_msg.c"
#include "matter/src/mtrc_cert.c"
#include "matter/src/mtrc_csr.c"
#include "matter/src/mtrc_store.c"
#include "matter/src/mtrc_dm.c"
#include "matter/src/mtrc_im.c"
#include "matter/src/qrcodegen.c"

// Firmware seam: the built-in lib gets matter_special_malloc (PSRAM-aware) from
// xdrv_124; the standalone plugin routes it to the framework allocator
// (malloc → jt[9] via mtrc_plugin_libc.h). MODULE_PART so it lands in the module
// section alongside the matter code that calls it. (PSRAM placement of the ~22 KB
// ctx is a later refinement — jcalloc is fine to stand the amalgamation up.)
MODULE_PART void *matter_special_malloc(size_t n) { return malloc(n); }

// ─── BLIB descriptor + end marker ────────────────────────────────────────────
// MODULE_MEMORY is now defined (by matter_c.c above), so ALLOCMEM in
// mod_func_execute resolves. Descriptor/end sections are linker-ordered, so
// source position after the amalgamation is fine.
MODULE_DESCRIPTOR("MATTERF", MODULE_TYPE_BLIB, 1<<16|5,
                  "", 0, "", 0, "", 0, "", 0)

MODULE_END

// ─── exports ─────────────────────────────────────────────────────────────────
// First-cut: a single liveness probe so the BLIB is valid + the loop is testable.
// The full matter_* export table (matter_init/add_endpoint/start/loop/set_attr…)
// is wired AFTER the amalgamation compiles clean. Names are named PROGMEM arrays
// (inline literals crash under EXEC_OFFSET — see xblib_01).
const uint32_t matterf_uconst[1] PROGMEM = { 0x4D545203 };   // 'MTR' v3
MODULE_PART int32_t matterf_probe(uint8_t *buf, int len) {
  GET_MTBL;
  const uint32_t *ucp = GUI32p(matterf_uconst);
  return (int32_t)ucp[0];
}

const char NAME_MATTERF_PROBE[] PROGMEM = "matterf_probe";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_MATTERF_PROBE, (void *)matterf_probe, 2, TC_RET_INT,
                        { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  { NULL, NULL, 0, 0, { 0 } }
};

// ─── dispatch ────────────────────────────────────────────────────────────────
int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    ALLOCMEM;                 // jcalloc the MODULE_MEMORY (holds the matter ctx ptr + crypto ops)
    initialized = 1;
    return 1;
  }
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];
  }
  return 0;
}

PULL_OPTIONS

#endif  // USE_MATTER_FULL_MOD
