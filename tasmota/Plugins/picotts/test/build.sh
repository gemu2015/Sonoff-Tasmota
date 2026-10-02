#!/bin/sh
# Builds the host test renderer in two variants: ref (double math) and flt (float math).
set -e
cd "$(dirname "$0")"
PICO=../../../../lib/libesp32_div/pico
OUT=${OUT:-/tmp/picotts_test}
mkdir -p $OUT
SRCS=""
for f in $PICO/lib/*.c; do
  case $f in */picorsrc.c|*/picodbg.c) ;; *) SRCS="$SRCS $f";; esac
done
CF="-O2 -w -DPICOTTS_HOST_TEST -Ishim -I$PICO/lib -I$PICO -DPICO_PLATFORM=PICO_Linux"
cc $CF -o $OUT/render_ref host_render.c $PICO/esp_picorsrc.c $SRCS -lm
cc $CF -DPICO_FLOATMATH -include shim/floatmath.h -o $OUT/render_flt host_render.c $PICO/esp_picorsrc.c $SRCS -lm
echo built in $OUT
