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

typedef struct mtrc_statics {
  mtrc_dm_table_t  dm;                            // mtrc_dm.c
  mtrc_fabric      fab[MTRC_MAX_FABRICS];         // mtrc_store.c
  mtrc_tx_route_t  tx;                            // matter_c.c g_tx
  int              qr_ok;                         // matter_c.c g_qr_ok
  uint8_t          qrbuf[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
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
#undef  MTRC_STATIC   // replaces the plain-static fallback defined at the top of matter_c.c
#define MTRC_STATIC(T, name, dims, field)  T (&name) dims = MTRC_ST->field

#endif // MTRC_PLUGIN_STATICS_H
