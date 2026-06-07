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
The plugin bundles **only matter_c**; the base provides the BearSSL crypto subset.

### Plugin shape: `MODULE_TYPE_BLIB` (gemu redirect 2026-06-02 — model on `xblib_01_crc.cpp`)
matter_c is a **reactive, flat-API** library with all its I/O behind a `matter_port_t` HAL
handed in at `matter_init()` — i.e. it has **no lifecycle of its own**. That maps onto a BLIB,
not a DRIVER:
- The plugin is a `MODULE_TYPE_BLIB` that exports the matter_c API
  (`matter_init`/`add_endpoint`/`start`/`loop`/`set_attr`/…) in a `BLIB_EXPORTS[]` table. The
  loader EXEC_OFFSET-corrects + registers each at `iniz`; `tc_blib_lookup("matter_*")` (an
  `extern "C"` in `xdrv_123_plugins.ino`) returns the ready-to-call native fn pointer — usable
  by firmware (cast to the real signature), not just by TinyC `bcall`.
- **The HAL needs NO jumptable shims.** The *firmware* fills `matter_port_t` with its own
  function addresses (udp/mdns/kv/log/millis/random) and passes the struct into the plugin's
  `matter_init`; the plugin calls back through those pointers — ordinary indirect calls into
  firmware. So transport/mdns/kv/log come for free, NOT as `tmod_ext_call` selectors.
- **Lifecycle stays in the firmware**, where `matter_port_t` is already wired
  (FUNC_NETWORK_UP→`matter_init`, FUNC_LOOP→`matter_loop`). The firmware's matter syscall layer
  resolves the exports via `tc_blib_lookup` when built **lean** (no built-in lib), else calls the
  built-in lib directly — a **build-time gate** (`#ifdef TINYC_MATTER` → lib, else plugin, else
  bail). The full-lib build stays byte-identical; no runtime `g_matter` refactor.
- **The ONLY host seam left is the BearSSL subset** matter_c needs. jt[192..198] export only
  `br_gcm_*` (AES-GCM, for SML decrypt) — a *different* subset. The matter subset (AES-CCM /
  EC-P256-i15 / ECDSA / HKDF / HMAC / SHA) is either **bundled** in the plugin (Fork A) or
  **exported** from a lean base via `tmod_ext_call`/the jumptable (Fork B).

### Stage 1 (DONE 2026-06-02) — probe BLIB builds + loads
`tasmota/Plugins/xblib_02_matter.cpp` (gate `USE_MATTER_MOD`): a trivial BLIB exporting
`matter_probe`→`0x4D545201` and `matter_abi`→`14`, NO matter_c yet. Validates build → load →
`tc_blib_lookup`/`bcall` round-trip before amalgamating matter_c. Built for esp32 (S3, `_32.bin`)
+ esp32_riscv (C3/C6, `_32r.bin`).
HARDWARE-VERIFIED on .156 (ESP32-S3, 15.4.0.1, has a 128 KB `custom` partition): upload via
`POST /modu` → `mdir` shows MOD #1 MATTER/xblb/292 B → `iniz 1` → log `BLIB: registered
'matter_probe' fn=0x423700d0 / 'matter_abi' fn=0x423700d8` → `blibtest matter_probe 00` →
`{"result":1297371649,"hex":"4d545201"}` = the exact export return. The whole loop
(build → upload → mmap → EXEC_OFFSET → tc_blib_lookup → native call) is proven — this is the
mechanism the firmware will use to resolve the real matter_c exports.

### Stage 2 (in progress 2026-06-02) — the BearSSL seam: subset enumerated, Fork B confirmed
matter_c's BearSSL use is isolated to `mtrc_crypto.c` (includes `t_bearssl.h`). Exact subset:
- functions (~21): `br_aes_ct_ctrcbc_init`; `br_ccm_{init,reset,aad_inject,flip,run,get_tag,check_tag}`;
  `br_ecdsa_i15_{sign,vrfy}_raw`; `br_hkdf_{init,inject,flip,produce}`; `br_hmac_{init,key_init,update,out}`;
  `br_sha256_{init,update,out}`.
- const vtables (2): `br_ec_p256_m15` (`br_ec_impl`), `br_sha256_vtable` (`br_hash_class`).
- NOTE: no GCM — does NOT overlap the `br_gcm_*`/`br_aes` already on jt[192..198] (SML decrypt).
File closure in `lib/lib_ssl/bearssl-esp8266/src/`: `symcipher/aes_ct_ctrcbc.c`, `aead/ccm.c`,
`kdf/hkdf.c`, `hash/sha2small.c`, `ssl/prf_sha256.c`, `ec/ecdsa_i15_{sign,vrfy}_raw.c`,
`ec/ec_all_m15.c` + `ec_p256_m15.c` (2111 lines) + `ec_prime_i15.c` (826) + ~10 `int/i15_*.c`
bignum files. The i15 EC/bignum is the heavy part. This tree already compiles+links on ESP32 (both
the built-in matter lib and the SML-decrypt `br_gcm` exports pull from it).

**DECISION: Fork B.** Fork A would mean PROGMEM/no-intrinsic hand-porting the 2111-line EC impl +
i15 bignum, bit-exact — high risk, multi-day. Fork B links the subset NORMALLY into a lean base and
hands it to the plugin.

**REFINEMENT (supersedes tmod_ext_call for crypto): pass crypto BY POINTER, like the HAL.** Extend
the plugin's `matter_port_t` (duplicated copy) with a crypto-ops sub-struct holding the ~21 `br_*`
fn pointers + the 2 vtable addresses; the firmware fills it with its linked `br_*` and passes it at
`matter_init`. The plugin's `mtrc_crypto.c` (duplicated copy) is refactored to call through the
struct (`crypto->ccm_init(...)`) instead of the direct `br_*` symbols. Signatures match → no
arg-packing and **NO tmod_ext_call / jumptable changes at all** — both HAL and crypto arrive as
init-time fn-pointer structs, the natural extension of the BLIB/HAL-by-pointer model. (tmod_ext_call
selectors stay a fallback if a by-pointer field proves awkward, e.g. a vtable whose inner fn
pointers also need EXEC_OFFSET handling.) Only base change: force-link the subset (the struct-fill
references it) + fill the struct. Open risk to verify first: the 2 const vtables hold fn pointers
into base `.rodata`; confirm the plugin can call through them as-is (base absolute addresses, no
EXEC_OFFSET) before committing to the by-pointer path for the vtables.

### Build environment (GATING — must be the main checkout)
`build_plugin.py` → `pio run -e tasmota32c3-plugin` needs `platformio_override.ini` +
`user_config_override.h`, which are **gitignored / main-checkout-only** (absent in this
worktree). So every Fork-B step below (jumptable edits, base BearSSL, build) runs in the main
checkout (or after copying those two files in). **First action: get a TRIVIAL matter-plugin
stub to build end-to-end — validate the loop before porting.**

### Host interface via `tmod_ext_call` (jt[219] selector dispatch — NOT new jt slots) [gemu]
The jumptable is frozen 0..218 (byte-identical across plugins); new host functions go through
**`tmod_ext_call(uint32_t sel, uint32_t a, uint32_t b, uint32_t c) -> int32_t`** (jt[219],
defined in `xdrv_123_plugins.ino`) as new `case sel:` entries — ABI-stable, no plugin-wide
rebuild, no new slots. So all of Fork B's additions are **new `tmod_ext_call` selectors**:
- Already available directly: `log`=jt[5], `millis`=jt[73], `malloc`/`free`, **kv** via
  `jfile_open/close/seek/read/write/size` (jt[142..160]).
- Add as `tmod_ext_call` selectors: `udp_send(ip6,port,buf,len)`, `mdns_publish(...)`,
  `mdns_remove(...)`, `random_bytes(buf,len)` — pass pointers/lengths via a/b/c, or a small
  args-struct pointer for the >3-arg ones (same pattern SML uses for `client_*` via jt[171]).
- Add as `tmod_ext_call` selectors: the **23 `br_*`** (AES-CCM/CT, EC-P256-m15 + i15, ECDSA-i15,
  HKDF, HMAC, SHA-224/256). The lean **base** links the BearSSL subset (the matter_c lib's own
  ~26-file subset) compiled **normally**, and each selector forwards to the linked `br_*`.
- Plugin side: thin macros wrap `tmod_ext_call(SEL_x, …)` per function (a `matter_port_t` HAL
  shim layer + a `br_*` shim header), so matter_c's `mtrc_crypto.c` calls resolve to the seam.

### Plugin entry (`tasmota/Plugins/xdrv_NNN_matter.cpp`)
- `#ifdef USE_MATTER_MOD`; `MODULE_DESCRIPTOR(MODULE_TYPE_DRIVER, …)`; `MODULE_MEMORY` = matter
  ctx ptr + the few mutable matter_c globals.
- Fill `matter_port_t`: kv→`jfile_*`, millis→`jmillis`, log→`jAddLog`, udp_send/mdns_*/random→the new slots.
- `mod_func_execute`: `pFUNC_INIT`→`matter_init(&port,&cfg)`+`add_endpoint`+`matter_start`;
  `pFUNC_LOOP`→`matter_loop()`; `pFUNC_COMMAND`→console cmds; `pFUNC_WEB_*`→QR/commissioning.
- Amalgamate matter_c via `#include "matter/src/<each>_c.h"` (unity) — AFTER the discipline pass.
  (Sources carry a `_c.h` extension so PlatformIO never standalone-compiles them under src_dir;
  they are only pulled in by the xblib_03 amalgamation. See the `_c.h` convention used by
  cc1101 / VL53L0X / the Audio plugins.)

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

### Stage 2 — by-pointer crypto seam DE-RISKED (PASSED, 2026-06-03, live on .156)
Harness: firmware (`xdrv_124` `CmndMatterCryptoTest`) fills `mtrc_crypto_ops` with its linked `br_*`
+ the two const vtables and passes it (with KAT input vectors) to a plugin export
`matter_crypto_selftest` (`xblib_02_matter.cpp` + `mtrc_crypto_selftest.h`); the plugin runs SHA-256
/ HMAC / HKDF / EC-mulgen / ECDSA / AES-CCM **through `io->ops`** and writes outputs back; firmware
checks each vs known-answer vectors → `mask`. **Result: `{"ran":90,"mask":"0x3f","pass":true}`,
repeatable, no reboot (Uptime climbing).** Bit 3 (`ec`) green = `ec_p256_m15->mulgen` called THROUGH
the base-`.rodata` vtable with **NO EXEC_OFFSET** on the method pointers → the flagged vtable risk is
**cleared**; the by-pointer crypto model is sound. The eventual lean-base just fills the struct and
calls the plugin's `mtrc_crypto_bind`.

**Discipline findings (the real cost of amalgamating matter_c — confirmed live, not theory):**
1. **`static` mutable file-scope data is fatal.** First cut amalgamated `mtrc_crypto.c`, whose bound
   ops sit in `static const mtrc_crypto_ops *g_cr` → in the PIC plugin that doesn't relocate →
   `bind()` wrote a wild address → first `g_cr->…` call = `Software reset CPU` (caught on .156). The
   full amalgamation must move every such global into `MODULE_MEMORY` (via `ALLOCMEM`/`SETMEMREGS`),
   or keep the touching code stateless/parameter-driven (what the de-risk did).
2. **`static const` too** (export-name arrays etc.) → make non-static.
3. **Inline integer literals ≥ 2048 miscompile** (l32r literal-pool, PIC). E.g. the probe's
   `0x4D545201` → moved to a non-static `const uint32_t matter_uconst[] PROGMEM` read via
   `GUI32p()`+`EXEC_OFFSET` (the `pico_uconst` pattern). matter_c has MANY such constants.
4. **libc `mem*` in pulled-in headers** (the BearSSL umbrella `t_bearssl.h` has inline fns using
   `memcpy`/`memmove`) expand to the framework's `jt[91..92]` jumptable calls needing `jt` in scope —
   absent at file scope → won't even parse. Neutralized with plain (non-static) byte-loop `mem*`
   before the include. Functions that genuinely need libc `mem*` must carry `jt` via `SETMEMREGS`.

Build/flash notes: plugin env needs `-I tasmota/Plugins/matter/include -I lib/lib_ssl/bearssl-esp8266/src`
(added to the `*-plugin` envs). .156 is an **S3-mini2 (S3FH4R2, QUAD PSRAM)** → base env is
`tinyc32s3-mini[-home]` (board `esp32s3-qio_qspi`), NOT `tinyc32s3` (octal PSRAM → PSRAM-init
boot-loop → safeboot rollback; wrong-variant OTA cost a creds reset). BLIB swap: `unlink N` → POST
`/modu` (field `modu`) → `iniz N` → `TinyCMtrCrypto`.

### Stage 3 — amalgamation foundation RECREATED + key enabler VALIDATED (2026-06-04, autonomous, UNCOMMITTED — held for review)
The 2026-06-03 Stage-3 foundation (`mtrc_plugin_mem.h` + the gated keystone) was uncommitted
and did NOT survive; recreated from the documented design, plus the libc enabler that makes the
whole amalgamation tractable. Build loop confirmed: `build_plugin.py --plugin USE_MATTER_MOD
--cpu esp32` → `MATTER_32.bin` (Tensilica, for .156 S3).

**Recreated / new artifacts (working tree, uncommitted):**
1. `matter/include/mtrc_plugin_mem.h` — `MODULE_MEMORY { matter_ctx_t *mtrc_ctx; const mtrc_crypto_ops *cr; }`
   + `#define MTRC_MEM ((MODULE_MEMORY*)gettbl()->mod_memory)` + `#define g_ptr (MTRC_MEM->mtrc_ctx)`.
   gettbl() is the framework's global MODULES_TABLE accessor → `g_ptr` (and `#define g (*g_ptr)`)
   resolve in ANY function with NO per-function `SETMEMREGS`. ctx stays in PSRAM; only the ptr is in
   the tiny jcalloc'd MODULE_MEMORY.
2. `matter/src/matter_c.c` keystone GATED: `#ifdef MTRC_PLUGIN_BUILD` → `#include "mtrc_plugin_mem.h"`
   `#else static matter_ctx_t *g_ptr=NULL; #endif` (non-plugin/lib copy unchanged).
3. **`matter/include/mtrc_plugin_libc.h` — THE amalgamation enabler, COMPILE-VALIDATED on Tensilica.**
   Same gettbl() trick applied to libc: `#undef`s the framework's local-jt remaps and redefines
   memcpy/memmove(jt92) memset(91) memcmp(89) strlen(59) strncpy(97) calloc/malloc(9) free(18)
   realloc(189) snprintf/sprintf(jt22, variadic) as `((sig)gettbl()->jt[N])(...)`. This is what lets
   all 61 matter_c functions (60 memcpy, 38 snprintf, 25 memset, …) amalgamate WITHOUT adding
   SETMEMREGS to each. Validated by a probe inside the building de-risk plugin → SUCCESS, 0 errors.
   ⚠ Caveats for the real port: jt[22] (snprintf) returns VOID → any `int n = snprintf(...)` in
   matter_c must drop the return; string literals (format strings, ~195 of them) still need PROGMEM
   /EXEC_OFFSET handling (separate discipline item, NOT solved by this shim).

**Structural standup target (NEXT — do in a NEW file, NOT xblib_02):** the override already carries a
commented `//#define USE_MATTER_FULL_MOD  // xblib_03_matter_full.cpp` gate → the FULL amalgamation
lives in **`xblib_03_matter_full.cpp`** (keep xblib_02 as the proven de-risk). Skeleton order:
`tasmota_options.h` → `#ifdef USE_MATTER_FULL_MOD` → `#define MTRC_PLUGIN_BUILD 1` → `module.h` +
`module_defines.h` → `PUSH_OPTIONS` → `mtrc_plugin_libc.h` → the 16 `#include "matter/src/*_c.h"`
(matter_c.c defines `matter_ctx_t` + pulls `mtrc_plugin_mem.h` → defines `MODULE_MEMORY`) →
`MODULE_DESCRIPTOR`/`MODULE_END` → exports (matter_init/add_endpoint/start/loop/set_attr…) +
`mod_func_execute` (ALLOCMEM in pFUNC_INIT) → `PULL_OPTIONS`.
**Ordering gotcha:** `MODULE_DESCRIPTOR` + ALLOCMEM need `MODULE_MEMORY` defined first, but it's defined
inside matter_c.c. Either (a) #include the matter sources BEFORE the descriptor (verify the SYNC-MARKER
extraction still brackets the object), or (b) give the ctx a struct TAG (`typedef struct matter_ctx_s
{…} matter_ctx_t;`) and forward-declare it so MODULE_MEMORY can hold the ptr above the sources.
**Remaining runtime discipline (audit, build-compiles-but-runtime-fatal — NOT caught by the compiler):**
g_qr_ok(304)+g_tx(1168) → move into `matter_ctx_t` (unconditional, both builds); g_cr(crypto.c:29) →
`MTRC_MEM->cr` (gated); g_fab[](store.c:6) → ctx `g.fab[]` (gated, needs mtrc_fabric in scope); ~73
inline literals ≥2048 → `const uint32_t[] PROGMEM` + GUI32p/EXEC_OFFSET (pico_uconst); ~15 static const
tables → non-static PROGMEM; ~195 string literals → PROGMEM; 16 float ops → fdiv/fmul. Then build →
flash .156 → iniz → commission.
