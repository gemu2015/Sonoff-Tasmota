// ============================================================================
// mtrc_plugin_statics.h — all mutable static data of matter_c in one heap block
// ============================================================================
//
// Plugin build only (MTRC_PLUGIN_BUILD). A BinPlugin has no .data/.bss of its
// own: a static lands in the plugin-host's .bss, at an address that means
// nothing in the running firmware (blib_audit: PTR-BSS). Everything that was a
// file-scope or function-local static lives here instead, in one block that
// xblib_03 allocates with plain calloc at pFUNC_INIT — early after boot, while
// the heap is still in one piece — so it works with and without PSRAM. Same
// RAM as the built-in lib, where these were .bss.
//
// Function-local buffers use MTRC_STATIC(T, name, dims, field): a C++ array
// reference to the field, so sizeof(name) is unchanged and a size mismatch is
// a compile error.
// ============================================================================
#ifndef MTRC_PLUGIN_STATICS_H
#define MTRC_PLUGIN_STATICS_H

#include "mtrc_plugin_strings.h"   // generated: all string literals as one PROGMEM blob

// File-scope byte tables (see mtrc_tables.h): each one keeps its initializer in
// PROGMEM as <name>_PGMGM and gets a RAM copy tab_<name> in the heap block, filled
// at pFUNC_INIT (xblib_03). <name> itself refers to the RAM copy — defined
// below the struct, AFTER all headers with extern declarations were read.
#define MTRC_FILE_TABLES(X) \
  X(uint8_t, P256_N,                      [32])     /* mtrc_crypto   */ \
  X(uint8_t, SPAKE_M,                     [65])     /* mtrc_spake2p  */ \
  X(uint8_t, SPAKE_N,                     [65])                         \
  X(uint8_t, SCALAR_ONE,                  [1])                          \
  X(uint8_t, ALG_ECPK,                    [21])     /* mtrc_csr      */ \
  X(uint8_t, ALG_ECDSA_SHA256,            [12])                         \
  X(uint8_t, MTRC_CASE_NONCE_SIGMA2,      [13])     /* mtrc_case     */ \
  X(uint8_t, MTRC_CASE_NONCE_SIGMA3,      [13])                         \
  X(uint8_t, INFO_SIGMA2,                 [6])                          \
  X(uint8_t, INFO_SIGMA3,                 [6])                          \
  X(uint8_t, INFO_SESSION,                [11])                         \
  X(char,    SPAKE_CTX_PREFIX,            [27])     /* mtrc_pase     */ \
  X(uint8_t, INFO_CONFIRM,                [16])                         \
  X(uint8_t, INFO_SESSION_P,              [11])                         \
  X(int8_t,  ECC_CODEWORDS_PER_BLOCK,     [4][41])  /* qrcodegen     */ \
  X(int8_t,  NUM_ERROR_CORRECTION_BLOCKS, [4][41])
#define MTRC_TAB_FIELD(T, name, dims)  T tab_##name dims;

typedef struct mtrc_statics {
  mtrc_dm_table_t  dm;                            // mtrc_dm.c
  mtrc_fabric      fab[MTRC_MAX_FABRICS];         // mtrc_store.c
  mtrc_tx_route_t  tx;                            // matter_c.c g_tx
  int              qr_ok;                         // matter_c.c g_qr_ok
  uint8_t          qrbuf[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
  uint32_t         strs[MTRC_STR_WORDS];          // RAM copy of mtrc_str_blob (MTRC_S)
  MTRC_FILE_TABLES(MTRC_TAB_FIELD)                // RAM copies of the file-scope byte tables
  // function-local scratch buffers (<function>_<name>)
  uint8_t  mtrc_build_onboarding_tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
  char     mtrc_publish_commissionable_txt_d[16];
  char     mtrc_publish_commissionable_txt_cm[8];
  char     mtrc_publish_commissionable_txt_vp[24];
  uint8_t  case_handle_sigma1_tmp[1100];
  uint8_t  case_handle_sigma1_enc2[1100];
  uint8_t  case_handle_sigma1_s2buf[1280];
  uint8_t  case_handle_sigma3_tbe3[1024];
  uint8_t  case_handle_sigma3_tmp[1100];
  uint8_t  secured_send_out[1280];
  uint8_t  build_csr_response_csr[400];
  uint8_t  build_csr_response_nocsr[480];
  uint8_t  build_csr_response_hin[480 + 16];
  uint8_t  build_attestation_response_ae[768];
  uint8_t  build_attestation_response_hin[768 + 16];
  uint8_t  im_handle_invoke_resp[1024];
  uint8_t  send_report_chunk_chunk[1280];
  uint8_t  send_report_chunk_frag[1024];
  uint8_t  im_handle_write_resp[512];
  uint8_t  secured_dispatch_pt[1280];
  uint8_t  secured_dispatch_sr[80];
  uint8_t  send_subscription_report_buf[1100];
  uint8_t  matter_emit_event_buf[256];
  uint8_t  matter_loop_rep[160];
  uint8_t  mtrc_pase_context_tmp[1024];
  uint8_t  mtrc_spake2p_transcript_tt[8 + 256 + 8 + 64 + 8 + 64 + 8*7 + 65*6 + 32 + 64];
} mtrc_statics_t;

#define MTRC_ST  (MTRC_MEM->st)

// load offset of this module: PROGMEM (link) address + offset = real address
#define MTRC_EXEC_OFF  (((FLASH_MODULE *)gettbl()->mod_addr)->execution_offset)

// Copy n bytes from PROGMEM (link address) to RAM. The module is mapped on the
// instruction bus: only 32-bit reads, so read words and store bytes (dst may
// be unaligned, n need not be a multiple of 4). always_inline: an out-of-line
// copy of a header inline has no MODULE_PART and would sit outside the module.
static inline __attribute__((always_inline)) void mtrc_bcopy(void *dst, const void *src_p, size_t n) {
  const volatile uint32_t *s = (const volatile uint32_t *)((const uint8_t *)src_p + MTRC_EXEC_OFF);
  uint8_t *d = (uint8_t *)dst;
  uint32_t w = 0;
  for (size_t i = 0; i < n; i++) {
    if ((i & 3) == 0) w = s[i >> 2];
    d[i] = (uint8_t)(w >> ((i & 3) * 8));
  }
}

// const tables (see mtrc_tables.h for the non-plugin forms)
#define MTRC_FTABLE(T, name, dims)        const T name##_PGM dims PROGMEM
#define MTRC_FTABLE_X(T, name, dims)      const T name##_PGM dims PROGMEM
#define MTRC_BTABLE(T, name, dims)        static const T name##_PGM dims PROGMEM
#define MTRC_BTABLE_LOAD(T, name, dims)   T name dims; mtrc_bcopy(name, name##_PGM, sizeof(name))
#define MTRC_WTABLE(T, name)              static const T name##_PGM[] PROGMEM
#define MTRC_WTABLE_PTR(T, name)          const T *name = (const T *)((const uint8_t *)name##_PGM + MTRC_EXEC_OFF)
#define MTRC_WTABLE_N(name)               (sizeof(name##_PGM) / sizeof(name##_PGM[0]))

#define P256_N                       (MTRC_ST->tab_P256_N)
#define SPAKE_M                      (MTRC_ST->tab_SPAKE_M)
#define SPAKE_N                      (MTRC_ST->tab_SPAKE_N)
#define SCALAR_ONE                   (MTRC_ST->tab_SCALAR_ONE)
#define ALG_ECPK                     (MTRC_ST->tab_ALG_ECPK)
#define ALG_ECDSA_SHA256             (MTRC_ST->tab_ALG_ECDSA_SHA256)
#define MTRC_CASE_NONCE_SIGMA2       (MTRC_ST->tab_MTRC_CASE_NONCE_SIGMA2)
#define MTRC_CASE_NONCE_SIGMA3       (MTRC_ST->tab_MTRC_CASE_NONCE_SIGMA3)
#define INFO_SIGMA2                  (MTRC_ST->tab_INFO_SIGMA2)
#define INFO_SIGMA3                  (MTRC_ST->tab_INFO_SIGMA3)
#define INFO_SESSION                 (MTRC_ST->tab_INFO_SESSION)
#define SPAKE_CTX_PREFIX             (MTRC_ST->tab_SPAKE_CTX_PREFIX)
#define INFO_CONFIRM                 (MTRC_ST->tab_INFO_CONFIRM)
#define INFO_SESSION_P               (MTRC_ST->tab_INFO_SESSION_P)
#define ECC_CODEWORDS_PER_BLOCK      (MTRC_ST->tab_ECC_CODEWORDS_PER_BLOCK)
#define NUM_ERROR_CORRECTION_BLOCKS  (MTRC_ST->tab_NUM_ERROR_CORRECTION_BLOCKS)

// A literal becomes a pointer into the RAM copy of the text blob. The size
// check turns a stale id (literal edited, tool not re-run) into a compile error.
#undef  MTRC_S
#undef  MTRC_SM
#define MTRC_SM(id, m)  MTRC_S(id, m)   // m: a header macro that expands to a literal
#define MTRC_S(id, s) \
  ((const char *)MTRC_ST->strs + MTRC_SOFF_##id + 0 * sizeof(char[(sizeof(s) == MTRC_SLEN_##id) ? 1 : -1]))
#undef  MTRC_STATIC   // replaces the plain-static fallback defined at the top of matter_c.c
#define MTRC_STATIC(T, name, dims, field)  T (&name) dims = MTRC_ST->field

#endif // MTRC_PLUGIN_STATICS_H
