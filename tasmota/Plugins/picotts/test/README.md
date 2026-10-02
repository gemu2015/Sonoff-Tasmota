# PicoTTS as a BLIB plugin: host feasibility test (02.10.2026)

`build.sh` builds `host_render` in two variants from the untouched engine sources in
`lib/libesp32_div/pico/` (output in `/tmp/picotts_test`):

- `render_ref` — engine as shipped (double math)
- `render_flt` — every double transcendental (`sqrt sin cos exp fabs`) routed through the single
  precision functions the plugin jump table offers (`shim/floatmath.h`)

A third variant (`render_all`, `typedef float picopal_double` plus `(double)` casts replaced) was
built from a scratch copy in `/tmp/picotts_allfloat`; it is not kept in the tree.

```
./render_ref <ta.bin> <sg.bin> "text" out.raw     # 16 kHz, 16 bit mono
```

## Results

| Question | Answer |
|---|---|
| Does the engine run outside FreeRTOS? | Yes, with `esp_picorsrc.c` and the engine files only (`esp_picotts.c` is the only FreeRTOS part). |
| Float instead of double math audible? | Length identical (94336 samples), peak identical, level within the run-to-run spread of the reference. |
| Bit-exact comparison possible? | **No.** Even identical binaries give different samples from run to run (all three variants). Zeroed arena, zeroed locals, `-O0` and no PIE did not remove it; with a 0xA5-filled arena the *length* changes, so the engine reads uninitialised memory. Compare length and level statistics over several runs, not samples. |
| Size for the S3 (`-Os`, 27 objects) | 150.8 KB text+rodata, 4 B bss. |
| Compiler helpers still needed (xtensa) | `__divsf3` plus double helpers (`__muldf3 __fixdfsi __extendsfdf2 __truncdfsf2 __divdf3 __floatsidf`). The all-float copy leaves `__divsf3`, `__muldf3`, `__fixdfsi`, `__extendsfdf2`, `__truncdfsf2`. |
| Where | `td_psola2` (6 double mults), `norm_result`, `pam_step`, `sigStep`, `picopal_sin/cos`; ~21 float divisions in `pam_step` 7, `pamDoCommand` 5, `sigStep` 4, `td_psola2` 4, `norm_result` 1. |
| libc the engine needs | `atoi memcpy memmove memset strcat strchr strcmp strcpy strlen strncmp strstr vsprintf` plus stdio file I/O in `picopal.c` (compile out). |

Open: the nondeterminism may exist on the device too (arena comes from PSRAM, not zeroed).
