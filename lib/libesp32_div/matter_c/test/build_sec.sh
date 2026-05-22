#!/usr/bin/env bash
# Host-side build+run of the secured-message self-test (AES-CCM + framing).
# Needs BearSSL CCM + AES (ctrcbc) + mtrc_crypto/frame/sec.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$HERE/.."
REPO="$(cd "$LIB/../../.." && pwd)"
BSSL="$REPO/lib/lib_ssl/bearssl-esp8266/src"
OUT=/tmp/mtrc_sec_test
mkdir -p "$OUT"

BSSL_SRC="$BSSL/aead/ccm.c \
          $(find "$BSSL"/symcipher -name 'aes_ct*.c') \
          $BSSL/ec/ec_p256_m15.c $BSSL/ec/ec_secp256r1.c \
          $BSSL/hash/sha2small.c $BSSL/mac/hmac.c $BSSL/kdf/hkdf.c \
          $(find "$BSSL"/codec -name '*.c')"

cc -std=c11 -O2 -Wall -I"$BSSL" -I"$LIB/include" \
   $BSSL_SRC \
   "$LIB/src/mtrc_crypto.c" "$LIB/src/mtrc_frame.c" "$LIB/src/mtrc_sec.c" \
   "$HERE/test_sec.c" -o "$OUT/sec"
"$OUT/sec"
