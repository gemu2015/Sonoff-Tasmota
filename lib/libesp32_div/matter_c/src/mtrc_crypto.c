// mtrc_crypto.c — BearSSL-backed crypto primitives for matter_c.
// See mtrc_crypto.h. GPLv3; crypto via BearSSL (BSD).

#include "mtrc_crypto.h"
#include "t_bearssl.h"
#include <string.h>

#define MTRC_CURVE  BR_EC_secp256r1   // 23

// P-256 subgroup order n (big-endian).
static const uint8_t P256_N[32] = {
  0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
  0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51
};

static const br_ec_impl *EC(void) { return &br_ec_p256_m15; }

// ---- hashing / MAC / KDF ----------------------------------------------
void mtrc_sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
  br_sha256_context c;
  br_sha256_init(&c);
  br_sha256_update(&c, data, len);
  br_sha256_out(&c, out);
}

void mtrc_hmac_sha256(const uint8_t *key, size_t key_len,
                      const uint8_t *data, size_t data_len, uint8_t out[32]) {
  br_hmac_key_context kc;
  br_hmac_context hc;
  br_hmac_key_init(&kc, &br_sha256_vtable, key, key_len);
  br_hmac_init(&hc, &kc, 0);                 // 0 = full-length output (32)
  br_hmac_update(&hc, data, data_len);
  br_hmac_out(&hc, out);
}

int mtrc_hkdf_sha256(const uint8_t *salt, size_t salt_len,
                     const uint8_t *ikm, size_t ikm_len,
                     const uint8_t *info, size_t info_len,
                     uint8_t *out, size_t out_len) {
  br_hkdf_context hc;
  br_hkdf_init(&hc, &br_sha256_vtable, salt, salt_len);
  br_hkdf_inject(&hc, ikm, ikm_len);
  br_hkdf_flip(&hc);
  br_hkdf_produce(&hc, info, info_len, out, out_len);
  return 1;
}

// ---- P-256 EC ops ------------------------------------------------------
int mtrc_ec_mulgen(uint8_t out[65], const uint8_t *k, size_t k_len) {
  size_t r = EC()->mulgen(out, k, k_len, MTRC_CURVE);
  return r != 0;
}

int mtrc_ec_mul(uint8_t point[65], const uint8_t *k, size_t k_len) {
  return (int)EC()->mul(point, MTRC_P256_POINT_LEN, k, k_len, MTRC_CURVE);
}

int mtrc_ec_muladd(uint8_t A[65], const uint8_t *B,
                   const uint8_t *a, size_t a_len,
                   const uint8_t *b, size_t b_len) {
  // BearSSL: muladd(A, B, len, x, xlen, y, ylen, curve) => A = x*A + y*B
  return (int)EC()->muladd(A, B, MTRC_P256_POINT_LEN,
                           a, a_len, b, b_len, MTRC_CURVE);
}

void mtrc_ec_scalar_neg(const uint8_t s[32], uint8_t neg[32]) {
  // neg = n - s  (256-bit big-endian subtract; assumes 0 < s < n)
  int borrow = 0;
  for (int i = 31; i >= 0; i--) {
    int d = (int)P256_N[i] - (int)s[i] - borrow;
    if (d < 0) { d += 256; borrow = 1; } else { borrow = 0; }
    neg[i] = (uint8_t)d;
  }
}
