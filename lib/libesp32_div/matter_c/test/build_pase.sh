#!/usr/bin/env bash
# Host-side build+run of the PASE self-test (key schedule + messages).
# Needs BearSSL (EC/SHA/HKDF/HMAC) + tlv + spake2p + pase.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$HERE/.."
REPO="$(cd "$LIB/../../.." && pwd)"
BSSL="$REPO/lib/lib_ssl/bearssl-esp8266/src"
OUT=/tmp/mtrc_pase_test
mkdir -p "$OUT"

BSSL_SRC="$BSSL/ec/ec_p256_m15.c $BSSL/ec/ec_secp256r1.c \
          $BSSL/hash/sha2small.c $BSSL/mac/hmac.c $BSSL/kdf/hkdf.c \
          $(find "$BSSL"/codec -name '*.c')"

cc -std=c11 -O2 -Wall -I"$BSSL" -I"$LIB/include" \
   $BSSL_SRC \
   "$LIB/src/mtrc_crypto.c" "$LIB/src/mtrc_spake2p.c" \
   "$LIB/src/mtrc_tlv.c" "$LIB/src/mtrc_pase.c" \
   "$HERE/test_pase.c" -o "$OUT/pase"
"$OUT/pase"
