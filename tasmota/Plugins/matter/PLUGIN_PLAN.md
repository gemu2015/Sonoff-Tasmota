# matter_c as a loadable BinPlugin — design, measured gap & plan

**Approach (gemu, 2026-06-02):** keep the built-in lib (`lib/libesp32_div/matter_c`)
and its firmware integration **100% untouched**; give the plugin its **own duplicated
copy** of the matter_c source (here under `tasmota/Plugins/matter/`). Resolution stays
*built-in lib → else plugin → else gracefully off*, achieved by **build-time selection +
the plugin self-integrating** through the BinPlugin framework — NOT by refactoring the
firmware's built-in path (the earlier in-firmware `g_matter` indirection idea is dropped).

---

## Phase 0 (DONE) — the lib boundary is clean
16 matter_c sources compile on host clang with **no firmware**. External surface =
**BearSSL (23 `br_*`) + libc** only; no host I/O callbacks (the `matter_port_t` HAL is
passed at `matter_init`; the core is reactive). The BearSSL dependency is isolated to a
**single file, `mtrc_crypto.c`** — a clean crypto seam.

## The BinPlugin discipline (the constraint)
From `tasmota/Plugins/readme.md` + the reference BLIB (`xblib_01_crc.cpp`):
- Pure C; **one `.cpp` entry per plugin** (extra sources amalgamated via `#include`),
  gated `USE_..._MOD`, with a `MODULE_DESCRIPTOR` + a single `MODULE_MEMORY` state struct.
- **No initialized mutable file-scope data** (init in code); mutable globals → the struct.
- **All const data in PROGMEM** (esp. >12-bit + float consts; RISC-V keeps consts in ROM).
- **No compiler intrinsics** — float math via `fdiv`/`fmul`; 64-bit `/`,`%` also emit intrinsics.
- Built by `build_plugin.py` → `pio run -e tasmota32c3-plugin` (standalone env in the
  gitignored `platformio_override.ini`); output = a relocatable `.bin` loaded into `custom`.

## Measured porting gap (2026-06-02)
**matter_c (16 files, 412 KB src) — MODERATE:**
- const tables: 40 `const={...}` + ~195 const strings → PROGMEM annotation (mechanical).
- truly-mutable initialized file-scope data: **~0** (the 13-hit heuristic was const/false-positive).
- float ops: **8** → trivial (mainly `matter_set_attr_scaled`); convert to `fdiv`/`fmul`.
- 64-bit: 79 `uint64` uses — mostly load/store/compare (no intrinsic); only the few `/`,`%`.
- ~86 data/bss/rodata symbols → const → PROGMEM, the few mutable → `MODULE_MEMORY`.

**BearSSL subset — THE WALL (bounded but real):**
- 23 `br_*`: AES-CT-CTRCBC + CCM, **EC-P256-m15 + i15 bignum**, ECDSA-i15, HKDF, HMAC, SHA-224/256.
- Closure ≈ 20-30 of BearSSL's 196 files; the **i15 modular-int / P256 math** is const-table +
  arithmetic heavy → the dominant porting effort under the discipline.
- A self-contained plugin must **bundle** this subset: the ESP32 firmware uses **mbedTLS**, not
  BearSSL — BearSSL is compiled only as part of the matter_c lib build, so it is NOT present in a
  lean base to import.

## Strategic fork — decide before the heavy port
- **A — fully self-contained plugin:** bundle + port matter_c **and** the BearSSL subset to the
  plugin discipline. Max flash relief + fully independent OTA, but the BearSSL i15 EC/bignum port
  is multi-day and must stay bit-exact (crypto). High risk/effort.
- **B — firmware provides crypto (RECOMMENDED):** the lean base links the same BearSSL subset
  (compiled **normally**, no discipline port) and exports the 23 `br_*` on the jumptable; the
  plugin bundles **only matter_c** (the moderate port). Far more feasible — the 23 `br_*` are a
  clean, stable seam already isolated to `mtrc_crypto.c`. Flash benefit is partial (matter_c moves
  out, ~the crypto stays in the base). Touches the **base + jumptable**, NOT the matter_c lib.

## Status / next steps
- ✅ Duplicated source: `tasmota/Plugins/matter/{src,include}/` (lib untouched, verified pristine).
- ✅ Gap measured (above). Phase 0 boundary confirmed.
- ⏭ **Decide A vs B** (the gating call — B recommended).
- ⏭ Then: amalgamation `.cpp` entry (`MODULE_DESCRIPTOR` + `matter_port_t` HAL via jumptable
  shims for udp/mdns/kv/log/millis/random), PROGMEM-annotate matter_c consts, `MODULE_MEMORY`
  consolidation; under B, wire the 23 `br_*` jumptable exports; `build_plugin.py` → `.bin`; load
  on a dev device (.156) and commission.
- **Honest effort:** not an afternoon. Under B, ~days (matter_c discipline pass + jumptable seam +
  build/relocate/commission); under A, more (the BearSSL i15 port dominates).

## RAM note (unchanged)
The plugin saves flash + enables independent OTA. It does **NOT** reduce RAM — matter's heap
(`matter_ctx` / CASE sessions / `rpt_paths` / TLV) is identical, so the no-PSRAM C3 ceiling is
unaffected. This remains a flash/modularity play.

---

## Fork B — CHOSEN (gemu, 2026-06-02): concrete implementation spec
Base provides crypto + transport via the jumptable; the plugin bundles **only matter_c**.

### Build environment (GATING — must be the main checkout)
`build_plugin.py` → `pio run -e tasmota32c3-plugin` needs `platformio_override.ini` +
`user_config_override.h`, which are **gitignored / main-checkout-only** (absent in this
worktree). So every Fork-B step below (jumptable edits, base BearSSL, build) runs in the main
checkout (or after copying those two files in). **First action: get a TRIVIAL matter-plugin
stub to build end-to-end — validate the loop before porting.**

### Jumptable exports to ADD (highest slot today ≈ 219 → new ones at jt[220+])
Already available for the `matter_port_t` HAL: `log`=jt[5], `millis`=jt[73],
`malloc`=`special_malloc`, `free`=jt[18], and **kv** via `jfile_open/close/seek/read/write/size`
(jt[142..160]). **Missing → add ~4 host exports + the 23 br_*:**
- `udp_send(ip6[16], port, buf, len)` — lwIP UDP6 (SML routes net via jt[171] op-codes; add a UDP op or a slot)
- `mdns_publish(service, instance, port, txt[], n)` + `mdns_remove(service, instance)` — esp-mDNS
- `random_bytes(buf, len)` — CSPRNG (`esp_fill_random`)
- the **23 `br_*`** (AES-CCM/CT, EC-P256-m15 + i15, ECDSA-i15, HKDF, HMAC, SHA-224/256) — link the
  BearSSL subset (the matter_c lib's own ~26-file subset) into the lean base, then export.

### Plugin entry (`tasmota/Plugins/xdrv_NNN_matter.cpp`)
- `#ifdef USE_MATTER_MOD`; `MODULE_DESCRIPTOR(MODULE_TYPE_DRIVER, …)`; `MODULE_MEMORY` = matter
  ctx ptr + the few mutable matter_c globals.
- Fill `matter_port_t`: kv→`jfile_*`, millis→`jmillis`, log→`jAddLog`, udp_send/mdns_*/random→the new slots.
- `mod_func_execute`: `pFUNC_INIT`→`matter_init(&port,&cfg)`+`add_endpoint`+`matter_start`;
  `pFUNC_LOOP`→`matter_loop()`; `pFUNC_COMMAND`→console cmds; `pFUNC_WEB_*`→QR/commissioning.
- Amalgamate matter_c via `#include "matter/src/<each>.c"` (unity) — AFTER the discipline pass.

### matter_c discipline pass (on the duplicated copy in `tasmota/Plugins/matter/`)
PROGMEM the 40 const tables + ~195 strings; consolidate the few mutable globals into
`MODULE_MEMORY` (most of the 86 symbols are const→PROGMEM); replace the 8 float ops with
`fdiv`/`fmul`; check the few 64-bit `/`,`%`; declare the 23 `br_*` as jumptable shims (not local).

### Build / flash / commission
`build_plugin.py --plugin USE_MATTER_MOD --cpu esp32_riscv` → `matter_32r.bin`. Build a LEAN base
(no `TINYC_MATTER`, but WITH the BearSSL subset + the new jumptable exports); flash to .156; load
`matter_32r.bin` into a `custom` slot; `iniz`; commission (chip-tool / Apple).

### Honest scope & cadence
Multi-day, every step build-gated (main-checkout env). Cadence: (1) trivial matter stub builds →
loop proven; (2) add the HAL + 23 `br_*` exports + base BearSSL link; (3) discipline-port
matter_c; (4) amalgamate → build → commission on .156.
