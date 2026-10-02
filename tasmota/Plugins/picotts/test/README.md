# PicoTTS as a BinPlugin — tests and how the plugin was brought up (02.10.2026)

The plugin is `tasmota/Plugins/xblib_04_picotts.cpp` plus the generated engine in `../engine/`
(`../tools/gen_picotts.py` derives it from the untouched `lib/libesp32_div/pico`). The firmware side
(task, text queue, arena, callbacks) lives in `xdrv_123_plugins.ino` ("PicoTTS from the BLIB"), so the
unchanged I2SAUDIO plugin keeps using jump table entries 209..214 whichever way the engine is provided.

## Programs here

| File | What it is |
|---|---|
| `host_render.c`, `build.sh` | the ORIGINAL engine on the host, three math variants (double, float, all-float) |
| `host_generated.cpp` | the GENERATED plugin source on the host, with a fake module table (`gcc -x c` or `g++ -fpermissive`) |
| `shim/` | `endian.h` and `floatmath.h` for the host builds |

Run: `./render_gen <ta.bin> <sg.bin> "text" out.raw` (16 kHz, 16 bit mono). Environment switches of
`host_generated`: `PICO_DEVICEFEED=1` (push all bytes first, then drain, like the firmware task),
`PICO_SPLIT=1`, `PICO_POISON=<byte>` (pre-fill the stack), `PICO_ERR=<f>` (emulate the firmware math precision),
`PICO_DUMP=1` (print the trace words).

## Results

| Question | Answer |
|---|---|
| Float instead of double math audible? | No: identical length and energy envelope. |
| Firmware `sinf/cosf/sqrtf` (jump table) precise enough? | They are approximations (sin(1) off by 2.4e-5); 100x that error changes nothing in the rendered length. |
| Bit-exact comparison possible? | No. Samples differ from run to run (even identical binaries). Compare lengths and levels. |
| Engine reads uninitialised memory? | Yes: with the arena filled with 0xA5 the length changes. The firmware zeroes the arena (`pico_arena_malloc`, `ptb_init`). |
| Size for the S3 | 144 KB module (`-Os`); `-O2` is 25 % bigger and brings blib_audit findings. |
| Speed | faster than real time on an S3 (engine 2.7 s for 4.6 s of speech) |

## The miscompile (found the hard way)

First device runs were 12 % shorter than the host (20 160 instead of 22 720 samples for "Guten Tag.") while the
built-in engine of the same firmware matched the host exactly. Bisecting with the trace hooks of the generator
(`gen_picotts.py --trace items,lex,pam,kdt,vec,acph`, plus `PICOTTS_DEBUG` and `blibtest picotts_pv`):

1. item stream (type, info bytes, payload checksum) identical up to the second word, then `info2` 51 vs 1;
2. decision trees asked: the device never asks the accent tree (13 attributes) — `acphAccentuation`;
3. the whole behaviour flips with ANY compiler barrier in that function, with `-O1` for the file, and with `-O0`
   for that one function; it does not depend on strict aliasing, `-fwrapv`, VRP or the IPA passes;
4. the same source behaves at every `-O` level on x86 (gcc 15, clang), as C and as C++, with poisoned stack.

So: `acphAccentuation` is compiled at `-O0` (generator, `FLOAT_EDITS`). Regression test: speak a set of German
sentences on the device and compare the sample count in the log (`PTT: utterance N samples`) with
`render_gen` — all eight in the table below matched exactly after the fix:

| Text | samples |
|---|---|
| Guten Tag. | 22720 |
| Guten Tag. Die Kamera spricht jetzt deutsch. | 63104 |
| Guten Tag. Die Kamera spricht jetzt deutsch, und das Wetter ist heute schön. | 94336 |
| Es ist 23 Uhr und 45 Minuten. | 60160 |
| Die Temperatur beträgt 21,5 Grad. | 66432 |
| Bewegung im Wohnzimmer erkannt! | 38208 |
| Achtung, die Haustür ist offen. | 43136 |
| Wie spät ist es? | 26816 |

## Traps met on the way

* PlatformIO builds every `.c` under `tasmota/Plugins/` into the firmware: host programs there must be guarded.
* GCC turns a hand assembled `(b0<<24)|(b1<<16)|(b2<<8)|b3` into `__bswapsi2`, and `x*31` into a `__mulsi3`
  call (ROM address on the S3): use other formulations (blib_audit reports them as ROM).
* `build_plugin.py` skips the build ("up to date") when only generated files changed: use `--force`.
* `Module_CheckFree` rounds the sector count up twice, so a plugin never fits a hole of exactly its own size.
