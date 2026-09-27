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

// assert() calls __assert_func in the host IRAM and drags file/function/condition
// strings into the host .rodata -- 49 calls + their texts (qrcodegen), all fatal in
// a relocated module (blib_audit, 26.09.2026). Must precede every <assert.h>.
#ifndef NDEBUG
#define NDEBUG 1
#endif

#include "tasmota_options.h"

#ifdef USE_MATTER_FULL_MOD

#define XBLIB_03            1
#define MTRC_PLUGIN_BUILD   1
#define MTRC_ATTEST_TEST_CREDS 1   // dev DAC/PAI/CD as in the tinyc firmware envs (-DMTRC_ATTEST_TEST_CREDS)   // gates matter_c.c's MODULE_MEMORY keystone include

#include "module.h"
#include "module_defines.h"

PUSH_OPTIONS

// THE amalgamation enabler: remap libc (memcpy/memset/snprintf/…) to the module
// jumptable via gettbl(), so all 16 matter_c sources amalgamate without adding
// SETMEMREGS to each function. Must come after module_defines.h (whose local-jt
// remaps it #undefs) and before the matter sources.
#include "mtrc_plugin_libc.h"

// memset/memcpy that the COMPILER emits by itself (struct/array zero-init, struct
// copies) do not go through the macros above -- they call the host's memset
// directly: 92 memset + 1 memcpy in the audit. Redeclaring the builtins with an
// asm label redirects those implicit calls to the module-local copies below.
// The parentheses keep the function-like macros above from expanding here.
extern "C" void *(memset)(void *, int, size_t) __asm__("mtrc_memset");
extern "C" void *(memcpy)(void *, const void *, size_t) __asm__("mtrc_memcpy");
// no-tree-loop-distribute-patterns: otherwise GCC turns these very loops back
// into memset/memcpy calls (endless recursion)
extern "C" MODULE_PART __attribute__((optimize("no-tree-loop-distribute-patterns")))
void *mtrc_memset(void *d, int c, size_t n) {
  uint8_t *p = (uint8_t *)d;
  while (n--) { *p++ = (uint8_t)c; }
  return d;
}
extern "C" MODULE_PART __attribute__((optimize("no-tree-loop-distribute-patterns")))
void *mtrc_memcpy(void *d, const void *s, size_t n) {
  uint8_t *p = (uint8_t *)d; const uint8_t *q = (const uint8_t *)s;
  while (n--) { *p++ = *q++; }
  return d;
}

// The descriptor references mod_func_execute (our dispatch). Forward-declare it.
MODULE_PART int32_t mod_func_execute(uint32_t sel);

// ─── matter_c amalgamation ───────────────────────────────────────────────────
// matter_c.c FIRST: it defines matter_ctx_t + pulls mtrc_plugin_mem.h (→ defines
// MODULE_MEMORY) + the `g`/`g_ptr` macros that the mtrc_*.c bodies below rely on.
// The mtrc_*.c headers are already included by matter_c.c; including the .c files
// here supplies their definitions in the same TU (unity build).
#include "matter/src/matter_c_c.h"
#include "matter/src/mtrc_tlv_c.h"
#include "matter/src/mtrc_frame_c.h"
#include "matter/src/mtrc_crypto_c.h"
#include "matter/src/mtrc_sec_c.h"
#include "matter/src/mtrc_spake2p_c.h"
#include "matter/src/mtrc_pase_c.h"
#include "matter/src/mtrc_case_c.h"
#include "matter/src/mtrc_case_msg_c.h"
#include "matter/src/mtrc_cert_c.h"
#include "matter/src/mtrc_csr_c.h"
#include "matter/src/mtrc_store_c.h"
#include "matter/src/mtrc_dm_c.h"
#include "matter/src/mtrc_im_c.h"
#include "matter/src/qrcodegen_c.h"

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

// The matter_c API for the firmware (xdrv_124_matter_dispatch.h looks these up
// by name). argc = 0 on purpose: TinyC's bcall()/fcall() accept only their own
// fixed signatures, so a script cannot call these with wrong arguments.
const char NAME_MTRC_CRYPTO_BIND[] PROGMEM = "mtrc_crypto_bind";
const char NAME_MTR_INIT[] PROGMEM = "matter_init";
const char NAME_MTR_UDP_RX[] PROGMEM = "matter_udp_rx";
const char NAME_MTR_START[] PROGMEM = "matter_start";
const char NAME_MTR_LOOP[] PROGMEM = "matter_loop";
const char NAME_MTR_QR_URI[] PROGMEM = "matter_qr_uri";
const char NAME_MTR_QR_DARK[] PROGMEM = "matter_qr_dark";
const char NAME_MTR_QR_SIZE[] PROGMEM = "matter_qr_size";
const char NAME_MTR_FACTORY_RESET[] PROGMEM = "matter_factory_reset";
const char NAME_MTR_SET_COMMISSIONABLE[] PROGMEM = "matter_set_commissionable";
const char NAME_MTR_OPEN_COMMISSIONING_WINDOW[] PROGMEM = "matter_open_commissioning_window";
const char NAME_MTR_MANUAL_CODE[] PROGMEM = "matter_manual_code";
const char NAME_MTR_SET_LABEL[] PROGMEM = "matter_set_label";
const char NAME_MTR_SET_ATTR_UINT[] PROGMEM = "matter_set_attr_uint";
const char NAME_MTR_SET_ATTR_SCALED[] PROGMEM = "matter_set_attr_scaled";
const char NAME_MTR_RESET_MODEL[] PROGMEM = "matter_reset_model";
const char NAME_MTR_QUEUE_EVENT[] PROGMEM = "matter_queue_event";
const char NAME_MTR_GET_ATTR_UINT[] PROGMEM = "matter_get_attr_uint";
const char NAME_MTR_ADD_ENDPOINT[] PROGMEM = "matter_add_endpoint";
const char NAME_MTR_ADD_CLUSTER[] PROGMEM = "matter_add_cluster";
const char NAME_MTR_ADD_ATTR[] PROGMEM = "matter_add_attr";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_MATTERF_PROBE, (void *)matterf_probe, 2, TC_RET_INT,
                        { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  { NAME_MTRC_CRYPTO_BIND, (void *)mtrc_crypto_bind, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_INIT, (void *)matter_init, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_UDP_RX, (void *)matter_udp_rx, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_START, (void *)matter_start, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_LOOP, (void *)matter_loop, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_QR_URI, (void *)matter_qr_uri, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_QR_DARK, (void *)matter_qr_dark, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_QR_SIZE, (void *)matter_qr_size, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_FACTORY_RESET, (void *)matter_factory_reset, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_SET_COMMISSIONABLE, (void *)matter_set_commissionable, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_OPEN_COMMISSIONING_WINDOW, (void *)matter_open_commissioning_window, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_MANUAL_CODE, (void *)matter_manual_code, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_SET_LABEL, (void *)matter_set_label, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_SET_ATTR_UINT, (void *)matter_set_attr_uint, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_SET_ATTR_SCALED, (void *)matter_set_attr_scaled, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_RESET_MODEL, (void *)matter_reset_model, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_QUEUE_EVENT, (void *)matter_queue_event, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_GET_ATTR_UINT, (void *)matter_get_attr_uint, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_ADD_ENDPOINT, (void *)matter_add_endpoint, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_ADD_CLUSTER, (void *)matter_add_cluster, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_MTR_ADD_ATTR, (void *)matter_add_attr, 0, TC_RET_INT, { TC_ARG_END } },
  { NULL, NULL, 0, 0, { 0 } }
};

// ─── dispatch ────────────────────────────────────────────────────────────────
int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    ALLOCMEM;                 // jcalloc the MODULE_MEMORY (holds the matter ctx ptr + crypto ops)
    // All former static data (~33 KB, see mtrc_plugin_statics.h) in one block,
    // allocated now — right after boot the heap is still in one piece, which
    // matters on ESP32s without PSRAM. calloc = zero-init like .bss.
    mem->st = (mtrc_statics_t *)calloc(1, sizeof(mtrc_statics_t));
    if (!mem->st) {
      RETMEM
      return -1;
    }
    // texts (MTRC_S): word copy from the module — it is mapped on the
    // instruction bus, where byte reads fault on ESP32/S3
    const volatile uint32_t *sb = (const volatile uint32_t *)((const uint8_t *)mtrc_str_blob + EXEC_OFFSET);
    for (uint32_t i = 0; i < MTRC_STR_WORDS; i++) {
      mem->st->strs[i] = sb[i];
    }
    // file-scope byte tables (mtrc_plugin_statics.h)
#define MTRC_TAB_COPY(T, name, dims)  mtrc_bcopy(mem->st->tab_##name, name##_PGM, sizeof(mem->st->tab_##name));
    MTRC_FILE_TABLES(MTRC_TAB_COPY)
#undef MTRC_TAB_COPY
    initialized = 1;
    return 1;
  }
  if (sel == pFUNC_DEINIT) {
    GET_MTBL; GET_JT;
    MODULE_MEMORY *mem = (MODULE_MEMORY *)mt->mod_memory;
    if (mem) {
      if (mem->mtrc_ctx) { free(mem->mtrc_ctx); }
      if (mem->st)       { free(mem->st); }
    }
    RETMEM                    // the host does not free MODULE_MEMORY itself
    return 0;
  }
  if (sel == pFUNC_GET_RAM) {         // for the module directory: heap beyond MODULE_MEMORY
    GET_MTBL;
    MODULE_MEMORY *mem = (MODULE_MEMORY *)mt->mod_memory;
    if (!mem) return 0;
    return (int32_t)((mem->st ? sizeof(mtrc_statics_t) : 0) + (mem->mtrc_ctx ? sizeof(matter_ctx_t) : 0));
  }
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];
  }
  return 0;
}

PULL_OPTIONS

#endif  // USE_MATTER_FULL_MOD
