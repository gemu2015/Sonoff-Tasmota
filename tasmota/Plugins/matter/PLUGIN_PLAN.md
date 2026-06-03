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

### Stage 3 — discipline port: audit + foundation laid (2026-06-03), bulk pending
Re-audit (the old "~0 mutable file-scope data" was wrong — it missed `g_cr`). Real fatal counts:
**4 mutable file-scope globals** (`g_qr_ok`,`g_tx` matter_c.c; `g_cr` mtrc_crypto.c [done via the
by-pointer seam]; `g_fab[]` mtrc_store.c) + **24 function-local `static` scratch buffers** (~15-18 KB,
mostly matter_c.c, 80-1280 B). Plus ~73 inline literals ≥2048 (59 in matter_c.c), ~15 `static const`
tables (+~195 strings), 16 float ops, ~1 64-bit `/`.

**KEY structural finding:** matter_c does NOT thread a `ctx`; it uses `static matter_ctx_t *g_ptr` +
`#define g (*g_ptr)` (the ~22 KB ctx is in PSRAM, task #75) with thousands of `g.field` sites. So the
globals/buffers mostly **collapse into that one context** — only the *pointer* is the true
plugin-illegal global.

**DECISION (gemu): gettbl()-macro.** Keep all `g.field`; hold the ctx POINTER in MODULE_MEMORY,
reached via gettbl(); ctx stays in PSRAM. **DONE:** `include/mtrc_plugin_mem.h` (MODULE_MEMORY =
`{matter_ctx_t *mtrc_ctx;}`; `#define g_ptr (MTRC_MEM->mtrc_ctx)` LVALUE so all 13 `g_ptr` sites +
all `g.` compile unchanged; `#define g (*g_ptr)`) + the gated keystone in matter_c.c
(`#ifdef MTRC_PLUGIN_BUILD` → include the header; else the plain static — keeps host/lib compile).

**Remaining bulk (mechanical-but-careful; verify at the amalgamation build):**
1. **Field-moves into `matter_ctx_t`** (ride the PSRAM ctx via `g.`): `g_qr_ok`→`g.qr_ok`,
   `g_tx`→`g.tx`, `g_fab[]`→`g.fab[]`. ⚠ NULL-safety: these were readable while `g_ptr==NULL`
   (`matter_qr_size`/`matter_qr_module`, `matter_is_commissionable`, `matter_qr_uri`/`manual_code`) —
   after the move, guard every not-inited accessor with `g_ptr &&` before the `g.` deref. `g_fab` is
   cross-file (mtrc_store.c): needs the `g` macro visible (amalgamation order: matter_c.c before
   mtrc_store.c) and `mtrc_fabric` defined before `matter_ctx_t` (check include order).
2. **24 scratch buffers → a `g.arena[]` field** (PSRAM, in ctx). Carve via stack-discipline
   high-water mark (save/restore at each fn entry/exit incl. early returns) OR fixed non-overlapping
   offsets from a call-graph "which buffers are live together" analysis. Size to worst-case nesting
   depth (NOT the 15-18 KB sum). Sites (pre-edit matter_c.c lines): 348,655,978,1002,1010,1038,1067,
   1218,1307,1311,1329,1396,1405,1605,2381-2,2532,2598,2652,2802,2830,2939 + mtrc_pase.c:30 +
   mtrc_spake2p.c:87.
3. **Other categories** (separate passes): ~73 literals ≥2048 → `const uint32_t[] PROGMEM` + GUI32p
   (pico_uconst); ~15 `static const` tables + ~195 strings → non-static PROGMEM + EXEC_OFFSET (GU8/
   PSTR); 16 float → fdiv/fmul; the BearSSL umbrella mem*/jt shims (as in the de-risk).

**Firmware integration (eventual lean base / real plugin entry .cpp):** ALLOCMEM the MODULE_MEMORY at
iniz; set the module register before every firmware→matter entry call so gettbl() resolves;
matter_init's `g_ptr = matter_special_malloc(sizeof(matter_ctx_t))` becomes an `MTRC_MEM->mtrc_ctx`
assignment — PSRAM ctx, MODULE_MEMORY holds only the 4-byte pointer.

**Verification:** only the full amalgamation build (plugin .cpp `#include`s the 16 sources +
module_defines + bearssl, `-DMTRC_PLUGIN_BUILD`). Host-compile (flag undefined) syntax-checks the
field-moves meanwhile. Honest: multi-day, best done in a session that stands up that amalgamation
build so each step is verified.

#### Amalgamation harness stood up (2026-06-03) + first-build landscape
`tasmota/Plugins/xblib_03_matter_full.cpp` (gate `USE_MATTER_FULL_MOD`, also added commented to
user_config_override.h so build_plugin.py finds it): `-DMTRC_PLUGIN_BUILD`, non-static byte-loop
mem* shims, an `extern "C" matter_special_malloc` stub, `#include`s all 16 `matter/src/*.c`
(matter_c.c FIRST for matter_ctx_t + g + MODULE_MEMORY), then MODULE_DESCRIPTOR + a minimal
mod_func_execute (ALLOCMEM/RETMEM). Build: `build_plugin.py --plugin USE_MATTER_FULL_MOD --cpu esp32`.
**First build → the #1 compile blocker: jt-redirected libc/framework calls.** The framework `#define`s
`malloc`(→jcalloc), `free`(jfree), `millis`, `strlen`, `strcmp/strncmp`, `memcmp`, `snprintf`,
`realloc`(jt[189]), `strchr`(jstrchr), `atoi`(jatoi)… to jumptable calls needing `jt` in scope — only
present in functions with SETMEMREGS, which matter_c's hundreds of functions lack → "`jt` not declared"
everywhere. **NEXT ENABLER (the keystone for compiling, analogous to the `g` macro):** a
`mtrc_plugin_jt.h` that `#undef`s each redirected name and redefines it to fetch `jt` via
`gettbl()->jt[INDEX]` (no local `jt` needed) — pure-compute ones (mem*/str*) can stay byte-loop
shims, but firmware services (malloc/free/millis/snprintf) must route through the jumptable. matter_c
includes it under MTRC_PLUGIN_BUILD. After that compiles, expect a second wave (C-vs-C++ strictness,
missing externs, MODULE_MEMORY ordering), THEN the relocation discipline (statics→ctx, literals→uconst)
verified on .156 hardware. The de-risk crash showed compile-clean ≠ runtime-correct, so hardware test
is mandatory per stage. Files added this session are UNCOMMITTED (held for review).

#### Compile wave 1 CLEARED + wave 2 mapped (2026-06-03)
**Wave 1 (jt-redirect) SOLVED** with a one-liner in the harness: `#define jt (gettbl()->jt)` for the
matter-include region (then `#undef jt` before the harness's own ALLOCMEM/GET_JT). Every framework jX
macro (jmalloc/jmemcmp/jsnprintf_P/…) now resolves the jumptable per call with no local `jt`; matter
uses the firmware's real fast memcpy/snprintf. Verified safe: no matter source uses `jt` as an ident,
none uses snprintf's (void) return. **Wave 2 (next, all compile-level):**
1. **`millis` collision** — the framework `#define millis jmillis` collides with `matter_port_t`'s
   `millis` field (matter_c.h:68 + module_defines.h:146 ×14). Fix: `#undef millis` (and any other
   libc name used as a matter struct FIELD — audit `matter_port_t`/config for collisions) around the
   header/struct decls, re-`#define` for the call sites. (Built-in lib doesn't hit this: there `millis`
   is a function, not a macro.)
2. **C++ strictness** — matter_c is C, amalgamated into a `.cpp`: many `const char*`→`char*` (the jX
   macro signatures take non-const) + `int`→enum (qrcodegen) conversions. Add `-fpermissive` to the
   plugin TU (or cast at sites). Consider compiling the amalgamation `extern "C"`-wrapped / as C if
   feasible.
3. **Static name collisions across files** — `INFO_SESSION[]` defined in BOTH mtrc_case.c:15 and
   mtrc_pase.c:12 (also check other short `static const` names: INFO_*, ALG_*, etc.) → redefinition in
   the single amalgamated TU. Rename per-file (e.g. CASE_/PASE_ prefix) or `#define`-scope.
4. Harness nit: pFUNC_DEINIT's `RETMEM` needs `GET_MTBL; GET_JT;` first (mt/jt in scope).
Then expect a wave 3 (more cross-file collisions / missing externs / link), THEN the relocation
discipline (statics→ctx, literals→uconst) — hardware-verified on .156. Iterative; multi-session.

#### ✅ MILESTONE: the full 16-source matter_c AMALGAMATION COMPILES (2026-06-03, zero errors)
Wave 2 fixes (all in the harness/env, NOT the matter sources except one rename): `#undef millis`
(framework object-macro vs the matter_port_t `millis` field + the 12 `g.port.millis(...)` HAL calls —
matter never calls raw millis(), only the comments do); `-fpermissive` on the plugin env (matter_c is
C amalgamated into a .cpp → downgrade the const-char*→char* / int→enum C-isms); rename
`INFO_SESSION`→`INFO_SESSION_PASE` in mtrc_pase.c (collided with mtrc_case.c's static); harness RETMEM
`GET_MTBL`/`GET_JT`. **Result: `build_plugin.py --plugin USE_MATTER_FULL_MOD --cpu esp32` SUCCEEDS,
zero compile errors.** The xblib_03 `.o` (752 KB LTO) contains every matter fn (matter_init/loop/
add_endpoint/case_handle_sigma1/mtrc_spake2p_*/mtrc_store_*/qrcodegen_*) — proven via objdump; the
final module is only 312 B because mod_func_execute references none of them, so LTO dead-strips. So the
whole compile-port (jt-redirect, millis, C++ strictness, cross-file collisions) is DONE in 2 waves —
matter_c amalgamates as a plugin.

**Remaining (two distinct phases, both still ahead):**
1. **Wire the real plugin entry** so the code is RETAINED + callable: `BLIB_EXPORTS` for
   matter_init/loop/add_endpoint/set_attr/…, fill `matter_port_t` (HAL by pointer) + the crypto ops
   (the de-risked by-pointer seam), `mtrc_crypto_bind`, ALLOCMEM the MODULE_MEMORY, and the firmware
   side resolving the exports via tc_blib_lookup (build-time lib-else-plugin gate). Module grows to its
   real size; commission on .156.
2. **The RELOCATION discipline (RUNTIME-correctness, compile-clean but crashes — like the de-risk):**
   the gettbl `g`/`g_ptr` keystone is laid; still TODO = the 4 globals→ctx + 24 buffers→`g.arena[]` +
   ~73 literals≥2048→uconst + ~15 const tables/~195 strings→PROGMEM+EXEC_OFFSET. ONLY hardware test on
   .156 catches these. Do them incrementally with a commission/selftest after each.
Files this session (xblib_03 + the keystone + env -I/-fpermissive + the rename) are UNCOMMITTED.

#### ✅ Relocation discipline — FATAL GLOBALS category DONE (2026-06-03, amalgamation still compiles)
All 4 remaining fatal mutable file-scope globals moved into `matter_ctx_t` so they ride the `g`
keystone (was the exact class that crashed the de-risk via `g_cr`): `g_qr_ok`→`g.qr_ok`,
`g_qrbuf`→`g.qrbuf`, `g_tx`→`g.txp` (matter_c.c, via `#define g_x (g.y)` macros — source sites
unchanged; added fields to the struct; NULL-guarded `matter_qr_size`/`matter_qr_dark` with `g_ptr &&`),
`g_fab[]`→`g.fab` (mtrc_store.c, `#ifdef MTRC_PLUGIN_BUILD` macro / else static — amalgamated after
matter_c.c so `g` is in scope). `g_tx` init was all-zero → memset(&g,0) covers it. Rebuild clean
(`--force`: build_plugin.py only mtime-checks the .cpp, NOT the #included sources — touch xblib_03 +
--force after editing matter/src). **So every fatal mutable file-scope datum (g_ptr/g_cr + these 4) is
now relocation-safe.**

**Remaining relocation work (compile-clean, RUNTIME-crashy — hardware-verify on .156):** the 24
function-local `static` scratch buffers → `g.arena[]` (stack-discipline or non-overlap analysis);
~73 inline literals ≥2048 → `const uint32_t[] PROGMEM`+GUI32p; ~15 `static const` tables + ~195
strings → PROGMEM+EXEC_OFFSET (GU8/PSTR). THEN wire the real entry (BLIB_EXPORTS + HAL + crypto-bind +
ALLOCMEM) and commission. None of this is compile-catchable — needs the device.

#### ⛔ BLOCKER (framework): the module extractor doesn't capture a large amalgamation
Wired `BLIB_EXPORTS` (matter_init/start/loop/udp_rx/add_endpoint/set_attr_uint/qr_uri/factory_reset)
+ an unreachable `if (sel==0xFFFFFFFF)` retention branch that CALLS them (a const pointer table alone
doesn't stop LTO GC). matter IS now retained — objdump of firmware.elf: matter_init @ 0x400d5a74 …
mtrc_store_alloc @ 0x400ddf68, ≈34 KB in `.flash.text`. **But the extracted module is still only 764 B.**
Root cause: `patch_linker_file.py` gathers the module (between the sync markers) ONLY from
`*(.text.mod_desc/mod_string/mod_*/mod_part/mod_end)` — i.e. what `MODULE_PART`
(`__attribute__((section(".text.mod_part")))`) emits. matter_c's hundreds of PLAIN functions land in
Tasmota's default `.flash.text` → OUTSIDE the bracket → not extracted; the module's export pointers
would then dangle (point into the build firmware's `.flash.text`, absent on-device). The de-risk only
worked because LTO INLINED its few tiny crypto wrappers into the one `MODULE_PART` selftest; matter_c
is far too large to inline. `-fno-lto` (added to the esp32 plugin env) does NOT help — it's section
PLACEMENT, not GC. **Needs a framework decision (gemu owns patch_linker_file.py + the module model):**
1. **Build `-ffunction-sections` + an object-scoped gather** in patch_linker_file.py:
   `*xblib_03_matter_full*.o(.flash.text .flash.text.* .text .text.*)` into the module section (+ its
   literals) — pulls the WHOLE plugin object's code into the bracket regardless of MODULE_PART. Cleanest
   + generic for any large amalgamation. (Risk: keep mod_desc first / mod_end last around it.)
2. OR force the xblib_03 TU's default text section to `.text.mod_part` (no clean per-TU GCC pragma).
3. OR first confirm the LOADER can EXEC_OFFSET-relocate a multi-KB code blob at all — existing modules
   are ≤~1 KB; a ~40 KB matter module + its relocations is a new regime (verify on .156's 128 KB custom
   partition before investing more).
This is the gate to ANY on-device matter test. Until resolved, the module loads but contains no matter
code. Progress backed up on branch `matter-plugin-stage3-wip`.
