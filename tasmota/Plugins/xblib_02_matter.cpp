/*
  xblib_02_matter.cpp — matter_c as a loadable BinPlugin (Fork B, BLIB model).

  STAGE 1 (this file, current): a TRIVIAL MODULE_TYPE_BLIB stub that exports
  two probe functions and nothing else. Its only job is to validate the build
  + load + tc_blib_lookup round-trip end-to-end BEFORE any matter_c source is
  amalgamated in. Modeled directly on xblib_01_crc.cpp.

      python3 tasmota/Plugins/build_plugin.py --plugin USE_MATTER_MOD --cpu esp32_riscv
      # upload build_output/firmware/Plugins/ESP32/RISC/MATTER_32r.bin via "Mod"
      # iniz N        → "BLIB: registered matter_probe / matter_abi"
      # from TinyC:  bcall("matter_probe", x, 0)  → 0x4D545201  ('MTR' v1)
      #              bcall("matter_abi",   x, 0)  → 14           (Matter 1.4)

  WHY a BLIB and not a DRIVER (design note — see matter/PLUGIN_PLAN.md):
  matter_c is a reactive, flat-API library with its I/O behind a matter_port_t
  HAL handed in at matter_init(). That maps onto a BLIB cleanly:
    - The plugin exports matter_init / add_endpoint / start / loop / set_attr…
      as native functions in BLIB_EXPORTS[].
    - The FIRMWARE keeps the lifecycle (FUNC_NETWORK_UP→init, FUNC_LOOP→loop)
      and fills matter_port_t with its OWN function addresses, passing the
      struct into matter_init — so udp/mdns/kv/log are plain indirect calls
      back into firmware, NOT jumptable shims.
    - The firmware's matter syscall layer resolves the exports via
      tc_blib_lookup("matter_*") when built lean (no built-in lib), else calls
      the built-in lib directly (build-time gate: lib → else plugin → else off).
    - The ONLY remaining host seam is the BearSSL subset matter_c needs
      (AES-CCM / EC-P256 i15 / ECDSA / HKDF / HMAC / SHA) — bundled (Fork A) or
      exported via tmod_ext_call / the jumptable (Fork B). jt[192..198] today
      export only br_gcm_* (AES-GCM, for SML decrypt) — a different subset.

  STAGE 2+ (later in this file): replace the probe exports with the real
  matter_c API, amalgamating tasmota/Plugins/matter/src/*.c via #include after
  the discipline pass (PROGMEM consts, fdiv/fmul, no 64-bit intrinsics).

  Copyright (C) 2026  Gerhard Mutz / claude
  GPL v3 (Tasmota's license)
*/

#include "tasmota_options.h"

#ifdef USE_MATTER_MOD

#define XBLIB_02  1

#include "module.h"
#include "module_defines.h"

PUSH_OPTIONS

// No per-instance state in the probe stage (matter_ctx lands here in Stage 2).
typedef struct {
  uint8_t _unused;
} MODULE_MEMORY;

// Revision must be >= MINREV (0x00010004 in xdrv_123_plugins.ino). Match the
// level of the shipping plugins (xblib_01_crc / xdrv_42_i2s use 1<<16|5).
MODULE_DESCRIPTOR("MATTER", MODULE_TYPE_BLIB, 1<<16|5,
                  "", 0, "", 0, "", 0, "", 0)

MODULE_PART int32_t matter_mod_probe(uint8_t *buf, int len);
MODULE_PART int32_t matter_mod_abi(uint8_t *buf, int len);
MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END

// ─── implementations ─────────────────────────────────────────────

// Liveness/version magic. 0x4D545201 ('M','T','R',v1) is a >12-bit literal,
// which in the PIC plugin build would compile to an l32r literal-pool load that
// miscompiles — so it lives in a PROGMEM uconst[] (the pico_uconst pattern) and
// is read back through EXEC_OFFSET via GUI32p, the same correction the loader
// applies to const data. NON-static per plugin discipline.
const uint32_t matter_uconst[1] PROGMEM = { 0x4D545201 };

// Liveness/version probe. A caller (TinyC bcall or firmware via tc_blib_lookup)
// gets the magic confirming the plugin is mapped + running.
int32_t matter_mod_probe(uint8_t *buf, int len) {
  GET_MTBL;                                   // `mt` → EXEC_OFFSET
  const uint32_t *ucp = GUI32p(matter_uconst);
  return (int32_t)ucp[0];
}

// Matter data-model ABI revision the plugin was built against (1.4 → 14).
// In Stage 2 this returns the matter_c-reported ABI so the firmware can refuse
// a plugin/lib version mismatch before wiring the HAL.
int32_t matter_mod_abi(uint8_t *buf, int len) {
  return 14;
}

// ─── Stage-2 crypto-seam de-risk (task #125) ─────────────────────────────────
// ⚠ TEMPORARY SCAFFOLD — remove (this export + the mem* shims + mtrc_crypto_selftest.h
// + xdrv_124's CmndMatterCryptoTest) once the real matter_c amalgamation + the
// mtrc_crypto_bind path land. Its only job is to prove the by-pointer seam works.
// Validate the Fork-B by-pointer crypto seam on a LOADED plugin: the firmware
// fills mtrc_crypto_ops (its linked br_* + the two base-.rodata vtables) plus the
// input vectors; this export runs every primitive THROUGH io->ops and writes the
// outputs back; the firmware checks them against known-answer vectors.
//
// PLUGIN DISCIPLINE — NO static / file-scope mutable state. The first cut
// amalgamated matter_c's mtrc_crypto.c, which keeps the bound ops in a `static`
// global (g_cr). In a relocatable plugin, file-scope mutable data does NOT
// relocate → mtrc_crypto_bind() wrote to a wild address and the first g_cr->…
// call jumped through garbage → "Software reset CPU" (caught live on .156). So
// the de-risk does NOT amalgamate mtrc_crypto.c; it inlines the same primitive
// orchestration here, driving io->ops directly — every datum is on the stack or
// in the caller's io struct, zero plugin globals. (The eventual full
// amalgamation must move matter_c's such globals into MODULE_MEMORY.)
// Needs -I tasmota/Plugins/matter/include + -I the BearSSL src on the plugin env.
//
// The BearSSL umbrella header (t_bearssl.h, pulled by mtrc_crypto_ops.h) has
// inline functions that call memcpy/memmove. The plugin framework #defines those
// to jumptable calls (jt[91..92]) needing `jt` in scope — absent here, so they
// fail to even PARSE. Neutralize the remap with plain (NON-static, per plugin
// discipline) byte-loop replacements before the include; the bearssl inlines are
// unused so this only has to make them parse, and our own selftest avoids mem*.
#undef  memcpy
#undef  memcpy_P
#undef  memset
#undef  memmove
#include <string.h>
void *mtrc_x_memcpy(void *d, const void *s, size_t n) {
  uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
  for (size_t i = 0; i < n; i++) a[i] = b[i]; return d;
}
void *mtrc_x_memmove(void *d, const void *s, size_t n) {
  uint8_t *a = (uint8_t *)d; const uint8_t *b = (const uint8_t *)s;
  if (a < b) { for (size_t i = 0; i < n; i++) a[i] = b[i]; }
  else       { for (size_t i = n; i > 0; i--) a[i-1] = b[i-1]; }
  return d;
}
void *mtrc_x_memset(void *d, int c, size_t n) {
  uint8_t *a = (uint8_t *)d; for (size_t i = 0; i < n; i++) a[i] = (uint8_t)c; return d;
}
#define memcpy  mtrc_x_memcpy
#define memmove mtrc_x_memmove
#define memset  mtrc_x_memset
#include "matter/include/mtrc_crypto_selftest.h"  // io struct + ops (pulls the br_* types)

extern "C" MODULE_PART int32_t matter_crypto_selftest(uint8_t *io_buf, int len) {
  mtrc_crypto_selftest_io *io = (mtrc_crypto_selftest_io *)io_buf;
  if (!io || !io->ops) return 0;
  const mtrc_crypto_ops *cr = io->ops;   // local, not static

  // SHA-256 — plain br_* function pointers through the seam
  { br_sha256_context c;
    cr->sha256_init(&c);
    cr->sha256_update(&c, io->sha_in, io->sha_in_len);
    cr->sha256_out(&c, io->sha_out); }

  // HMAC-SHA256 — passes the sha256_vtable pointer into the base fns
  { br_hmac_key_context kc; br_hmac_context hc;
    cr->hmac_key_init(&kc, cr->sha256_vtable, io->hmac_key, io->hmac_key_len);
    cr->hmac_init(&hc, &kc, 0);
    cr->hmac_update(&hc, io->hmac_in, io->hmac_in_len);
    cr->hmac_out(&hc, io->hmac_out); }

  // HKDF-SHA256
  { br_hkdf_context hk;
    cr->hkdf_init(&hk, cr->sha256_vtable, io->hkdf_salt, io->hkdf_salt_len);
    cr->hkdf_inject(&hk, io->hkdf_ikm, io->hkdf_ikm_len);
    cr->hkdf_flip(&hk);
    cr->hkdf_produce(&hk, io->hkdf_info, io->hkdf_info_len, io->hkdf_out, io->hkdf_out_len); }

  // P-256 pub = d*G — the mulgen METHOD CALL through the ec_p256_m15 vtable (THE flagged risk)
  cr->ec_p256_m15->mulgen(io->ec_pub, io->ec_priv, 32, BR_EC_secp256r1);

  // ECDSA P-256 sign + verify round-trip over the SHA-256 digest computed above
  { br_ec_private_key sk; sk.curve = BR_EC_secp256r1; sk.x = (unsigned char *)io->ec_priv; sk.xlen = 32;
    size_t sl = cr->ecdsa_sign_raw(cr->ec_p256_m15, cr->sha256_vtable, io->sha_out, &sk, io->ecdsa_sig);
    if (sl == 64) {
      br_ec_public_key pk; pk.curve = BR_EC_secp256r1; pk.q = (unsigned char *)io->ec_pub; pk.qlen = 65;
      io->ecdsa_verify = cr->ecdsa_vrfy_raw(cr->ec_p256_m15, io->sha_out, 32, &pk, io->ecdsa_sig, 64) ? 1 : 0;
    } }

  // AES-128-CCM encrypt then decrypt round-trip (aes_ct_ctrcbc keys + ccm fns).
  // Byte-loop copies (no memcpy) so the harness adds no libc dependency.
  { br_aes_ct_ctrcbc_keys bc; cr->aes_ct_ctrcbc_init(&bc, io->ccm_key, 16);
    for (uint32_t i = 0; i < io->ccm_pt_len; i++) io->ccm_ct[i] = io->ccm_pt[i];
    br_ccm_context cc; cr->ccm_init(&cc, &bc.vtable);
    if (cr->ccm_reset(&cc, io->ccm_nonce, io->ccm_nonce_len, io->ccm_aad_len, io->ccm_pt_len, 16)) {
      if (io->ccm_aad_len) cr->ccm_aad_inject(&cc, io->ccm_aad, io->ccm_aad_len);
      cr->ccm_flip(&cc);
      cr->ccm_run(&cc, 1, io->ccm_ct, io->ccm_pt_len);
      cr->ccm_get_tag(&cc, io->ccm_tag);
      for (uint32_t i = 0; i < io->ccm_pt_len; i++) io->ccm_dec[i] = io->ccm_ct[i];
      br_ccm_context cc2; cr->ccm_init(&cc2, &bc.vtable);
      if (cr->ccm_reset(&cc2, io->ccm_nonce, io->ccm_nonce_len, io->ccm_aad_len, io->ccm_pt_len, 16)) {
        if (io->ccm_aad_len) cr->ccm_aad_inject(&cc2, io->ccm_aad, io->ccm_aad_len);
        cr->ccm_flip(&cc2);
        cr->ccm_run(&cc2, 0, io->ccm_dec, io->ccm_pt_len);
        io->ccm_dec_ok = cr->ccm_check_tag(&cc2, io->ccm_tag) ? 1 : 0;
      } } }

  io->ran = 0x5A;
  return io->ran;
}

// ─── exports table ───────────────────────────────────────────────
// Read by the loader via mod_func_execute(pFUNC_GET_TINYC_EXPORTS). Names MUST
// be named PROGMEM arrays (not inline literals) so they land in the plugin's
// own mod_string section and EXEC_OFFSET-correct to valid runtime addresses
// (see the long comment in xblib_01_crc.cpp — inline literals crash).
const char NAME_MATTER_PROBE[]    PROGMEM = "matter_probe";
const char NAME_MATTER_ABI[]      PROGMEM = "matter_abi";
const char NAME_MATTER_SELFTEST[] PROGMEM = "matter_crypto_selftest";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_MATTER_PROBE, (void *)matter_mod_probe, 2, TC_RET_INT,
                       { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  { NAME_MATTER_ABI,   (void *)matter_mod_abi,   2, TC_RET_INT,
                       { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  // De-risk: firmware fills the io struct (passed as the buf arg) + checks KATs.
  { NAME_MATTER_SELFTEST, (void *)matter_crypto_selftest, 2, TC_RET_INT,
                       { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  // Sentinel — name == NULL marks end of list.
  { NULL, NULL, 0, 0, { 0 } }
};

// ─── dispatch ────────────────────────────────────────────────────
// Two selectors, same contract as every BLIB:
//   pFUNC_INIT              — self-check + set initialized (read by deiniz).
//   pFUNC_GET_TINYC_EXPORTS — hand the loader BLIB_EXPORTS[] to register.
// Everything else returns 0 so the regular dispatch loops walk past us.
int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    GET_MTBL;
    initialized = 1;
    return 1;
  }
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];
  }
  return 0;
}

PULL_OPTIONS

#endif  // USE_MATTER_MOD
