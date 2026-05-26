# TinyC + matter_c memory budget

Measured on `tinyc32-4M-cam` (classic ESP32 with 4 MB PSRAM, app1856k/fs1344k
partition, `-DBOARD_HAS_PSRAM -DHAS_PSRAM_FIX -mfix-esp32-psram-cache-issue`)
built 2026-05-26 against firmware.elf. Use as a rough planning aid — exact
numbers shift with build flags, LTO, and how many script slots are loaded.

## Per-feature footprint

| Feature | Flash (code + rodata) | DRAM (BSS-resident) | DRAM (heap, on demand) | PSRAM (heap) |
|---|---:|---:|---:|---:|
| **TinyC VM + builtins** | ~125 KB ¹ | ~9 KB ² | ~3–5 KB **per loaded slot** ³ | — |
| **matter_c (Matter 1.4 responder)** | ~57 KB ⁴ | ~31 KB ⁵ | — *(with PSRAM patch ⁸)* | ~30 KB ⁶ |
| &nbsp;&nbsp;↳ matter_ctx_t (before PSRAM patch) | — | — | **~30 KB** ⁶ | — |
| **Camera extension** (`-DTINYC_CAMERA`) ⁹ | ~55 KB | ~18 KB | — | **frame buffers ⁰** |
| **Combined, PSRAM-cam board** (TinyC + Matter + cam) | ~237 KB | ~58 KB | ~3.5 KB / slot + buffers | ~30 KB + cam fb |
| **Combined, no-PSRAM board** (TinyC + Matter, no cam) ⁷ | ~182 KB | ~70 KB | ~3.5 KB / slot | — |

### Footnotes

1. **TinyC flash** — sum of `.text` for symbols matching `TinyC`, `Tinyc`, `tc_`,
   `HandleTinyC`, `CmndTinyC`, etc. in firmware.elf. LTO + name-mangling makes
   this approximate (the actual delta vs USE_TINYC=OFF would be tighter).

2. **TinyC DRAM (BSS)** — file-scope static tables that are always resident:

   | Symbol | Size | What |
   |---|---:|---|
   | `tc_share_table` | 2.9 KB | UDP/share globals registry |
   | `tc_ui_widgets`  | 1.3 KB | Web-UI widget descriptors |
   | `tc_blib_reg`    | 1.3 KB | BLib registration table |
   | `tc_pwl_binds`   | 1.2 KB | Powerwall binding state |
   | `tc_mscr`        | 1.1 KB | Misc shared scratch |
   | others (dmx, ssl_client, file_handles, cam, …) | ~1.3 KB | per-subsystem tiny BSS |
   | **subtotal** | **~9 KB** | |

3. **Per-slot heap** — paid only for slots that have a script loaded:

   - Operand stack: 256 × 4 B = 1 KB (`TC_STACK_SIZE`)
   - Frame[0] locals: 256 × 4 B = 1 KB (`TC_MAX_LOCALS`, `tc_frame_alloc`)
   - Globals array: 64 × 4 B = 0.25 KB minimum, grows with script declarations
   - Constants table: ~16 B × N (typical script: ~30 → ~0.5 KB)
   - Constant data (string pool): typical 200–500 B
   - Optional `heap_data`: script-declared arrays (the "568/568" in `TinyCInfo` output)
   - Persisted vars: small

   Up to `TC_MAX_VMS = 6` slots can coexist on ESP32. Each unused slot is NULL
   and costs 0 bytes.

4. **matter_c flash** — symbol-prefix sum reports ~23 KB but that misses
   crypto helpers that share names with other users (AES-CCM, HKDF, SHA-256,
   ECDSA). Verified `USE_MATTER_C` ON-vs-OFF delta is **~57 KB** (recorded in
   project memory).

5. **matter_c DRAM (BSS)** — always resident as long as the feature is built in:

   | Symbol | Size | What |
   |---|---:|---|
   | `dm` (mtrc_dm.c)        | 9.2 KB | endpoint/cluster/attribute tables (sized for ~32 endpoints) |
   | `g_fab` (mtrc_store.c)  | 5.0 KB | fabric table + per-fabric NOC/ICAC/RCAC certificate copies |
   | function-scope statics  | ~17 KB | s2buf, tbe3, pt, out, chunk, frag, csr, nocsr, hin, ae, tmp[N] — handshake/IM scratch buffers |
   | `g_qrbuf`, misc         | ~0.2 KB | onboarding QR module matrix + small statics |
   | **subtotal**            | **~31 KB** | |

   These large statics avoid heap allocs on every CASE handshake / IM
   request — a trade of always-resident DRAM for predictable latency.

   **Lesson from an attempted scratch-buffer move (reverted)**: an earlier
   attempt (commit `ada2ccec8`, reverted by `<this commit>`) tried to lift
   ~11 KB of these statics into matter_ctx_t to follow it into PSRAM. It
   built fine and `.dram0.bss` did drop by 11.3 KB. But on a classic
   ESP32-D0WD with PSRAM, **CASE Sigma3 TBE decrypt then started failing
   deterministically** even after reverting just the in-place AES-CCM
   output buffers. The most likely cause is that the transcript-hash
   input (`g.case_tt`) reads stale bytes when the just-written Sigma2 was
   built via a PSRAM-resident scratch buffer — i.e., PSRAM-to-PSRAM
   sequential write/read coherency is not fully reliable on classic ESP32
   even with `-mfix-esp32-psram-cache-issue`. Future attempts to free
   this DRAM should either (a) keep all crypto-input/output buffers in
   DRAM and only move pure-TLV-build buffers, or (b) add explicit cache
   flush/invalidate around PSRAM-to-PSRAM transcript ops, or (c) host
   `case_tt` itself in DRAM regardless of matter_ctx_t placement.

6. **matter_ctx_t** — single ~30 KB struct allocated *lazily* the first time
   matter is initialized (typically at FUNC_NETWORK_UP from the xdrv binding):

   - `case_sess[16]` × ~112 B  ≈ 1.8 KB — concurrent operational sessions
   - `rx_q[8]` × 1296 B        ≈ 10.4 KB — deferred UDP datagram ring
   - `last_tx_buf[1536]`       =  1.5 KB — Sigma2 retransmit buffer
   - `case_tt[2560]`           =  2.5 KB — full Sigma1+2+3 transcript
   - `rpt_paths[1024]` × 12 B  ≈ 12.0 KB — wildcard-read path enumeration
   - `labels[33]` × ~35 B      ≈ 1.2 KB — bridge endpoint names
   - other fields              ≈ 1.5 KB — PASE state, NOC pending, subs, event queue
   - **total**                 = **30,896 B**

7. **No-PSRAM boards** fall back to plain `malloc()` for matter_ctx_t (still
   ~30 KB DRAM heap). The classic-WROOM (`tinyc32-4M`, no `-DBOARD_HAS_PSRAM`)
   target spends ~290 KB DRAM total, so Matter+TinyC eat ~70 KB ≈ 24 % of it.

8. **PSRAM patch** (commit `853167e84`, 2026-05-26): matter_init() routes the
   matter_ctx_t allocation through `matter_special_malloc` → Tasmota's
   `special_malloc` → `heap_caps_malloc(MALLOC_CAP_SPIRAM)` when PSRAM is
   present. Verified live on .47 with the allocated pointer landing at
   `0x3f8019a8` (classic-ESP32 PSRAM mapping starts at `0x3F800000`).

9. **Camera extension** — measured as the *delta* between `tinyc32-4M` and
   `tinyc32-4M-cam` (both have TinyC + Matter; only the cam env adds
   `-DTINYC_CAMERA` + the esp32-camera library + PSRAM compile flags):

   |  Section          | tinyc32-4M | tinyc32-4M-cam | Δ |
   |---|---:|---:|---:|
   | `.flash.text`     | 1232 KB | 1282 KB | **+50 KB** |
   | `.flash.rodata`   | 184 KB | 189 KB | **+5 KB** |
   | `.iram0.text`     | 88 KB | 89 KB | +1 KB |
   | `.dram0.bss`      | 78 KB | 89 KB | **+11 KB** |
   | `.dram0.data`     | 22 KB | 29 KB | **+7 KB** |

   ≈ **55 KB flash + 18 KB DRAM** of always-resident overhead just by
   compiling the cam in. The DRAM side is dominated by `jpge` Huffman /
   quantization tables (≈9 KB) plus sensor register tables.

0. **Camera framebuffers** are allocated on-demand by `esp_camera_init()` via
   `heap_caps_malloc(MALLOC_CAP_SPIRAM)`. Size depends on resolution +
   format: QVGA-JPEG ≈ 25 KB / frame, VGA-JPEG ≈ 50 KB, UXGA-JPEG ≈ 150 KB.
   Typical `fb_count = 2` doubles that — i.e. operating a 2-frame UXGA-JPEG
   pipeline costs ~300 KB PSRAM (well within the 4 MB pool). These buffers
   are released when the camera deinits; they are NOT counted in the
   tables above because they only exist while a script actively uses the
   camera syscalls.

## Transient overhead (not counted above)

Stuff that briefly lives on the calling task's stack and disappears:

- CASE Sigma3 verify: SHA-256 ctx (~110 B), HKDF temp (~200 B), ECDSA verify
  ECC scratch (~2–3 KB) — peak stack add ~3–4 KB during a handshake.
- IM ReportData chunk build: a chunk plaintext buffer (≤1280 B) is a file
  static (`chunk$20` above, already counted).
- LwIP UDP socket and PCBs: ~1–2 KB out of LwIP's pool (shared with the rest
  of the network stack — not attributable to Matter alone).

## How this was measured

```bash
NM=$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-nm
SIZE=$HOME/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp-elf-size
ELF=.pio/build/tinyc32-4M-cam/firmware.elf

# Section-level sizes (.flash.text, .dram0.bss, etc.)
$SIZE -A $ELF

# Per-symbol DRAM BSS / data, sorted by size
$NM --print-size --size-sort -r $ELF | awk '$3 ~ /^[bBdD]$/' | head -50
```

`matter_ctx_t` size and pointer location were verified live on device .47 via
a one-shot DIAG print (snprintf of `(void*)g_ptr` + `sizeof(matter_ctx_t)`,
since reverted from the source tree).

## Related project memory

- "ESP32 RAM measurement + no-PSRAM Matter heap budget" — single-shot heap
  observations under stress (commissioning + SPAKE2+).
- "matter_c endpoint scaling" — rationale for the fixed-size tables (32 EPs,
  160 clusters, 320 attrs, 1024 rpt_paths) instead of dynamic allocation.
