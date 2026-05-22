#!/usr/bin/env bash
# Host-side build+run of the SPAKE2+ Phase-0 spike against the Tasmota
# BearSSL (compiled with its non-ESP8266 PROGMEM no-op fallback).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$HERE/.."
REPO="$(cd "$LIB/../../.." && pwd)"
BSSL="$REPO/lib/lib_ssl/bearssl-esp8266/src"

OUT=/tmp/mtrc_spake_spike
mkdir -p "$OUT"

# Minimal BearSSL set: P-256 (m15) + curve params + SHA-256 + HMAC + HKDF
# + codec leaf helpers. We call br_ec_p256_m15 directly, so the ECDSA /
# default-selector files (which drag in i31/DRBG) are intentionally omitted.
BSSL_SRC="$BSSL/ec/ec_p256_m15.c \
          $BSSL/ec/ec_secp256r1.c \
          $BSSL/hash/sha2small.c \
          $BSSL/mac/hmac.c \
          $BSSL/kdf/hkdf.c \
          $(find "$BSSL"/codec -name '*.c')"

cc -std=c11 -O2 -Wall \
   -I"$BSSL" -I"$LIB/include" \
   $BSSL_SRC \
   "$LIB/src/mtrc_crypto.c" "$LIB/src/mtrc_spake2p.c" "$HERE/test_spake2p.c" \
   -o "$OUT/spike"

"$OUT/spike"
