/*
  xblib_02_matter.cpp — matter_c as a loadable BinPlugin (Fork B, BLIB model).

  STAGE 1 (this file, current): a TRIVIAL MODULE_TYPE_BLIB stub that exports
  two probe functions and nothing else. Its only job is to validate the build
  + load + tc_blib_lookup round-trip end-to-end BEFORE any matter_c source is
  amalgamated in. Modeled directly on xblib_01_crc.cpp.

      python3 tasmota/Plugins/build_plugin.py --plugin USE_MATTER_MOD --cpu esp32_riscv
      # upload build_output/firmware/Plugins/ESP32/RISC/MATTER_32r.bin via "Mod"
      # iniz N        → "BLIB: registered matter_probe / matter_abi"
      # from TinyC:  bcall("matter_probe", x, 0)  → 0x4D545201  ('MTR' v1)
      #              bcall("matter_abi",   x, 0)  → 14           (Matter 1.4)

  WHY a BLIB and not a DRIVER (design note — see matter/PLUGIN_PLAN.md):
  matter_c is a reactive, flat-API library with its I/O behind a matter_port_t
  HAL handed in at matter_init(). That maps onto a BLIB cleanly:
    - The plugin exports matter_init / add_endpoint / start / loop / set_attr…
      as native functions in BLIB_EXPORTS[].
    - The FIRMWARE keeps the lifecycle (FUNC_NETWORK_UP→init, FUNC_LOOP→loop)
      and fills matter_port_t with its OWN function addresses, passing the
      struct into matter_init — so udp/mdns/kv/log are plain indirect calls
      back into firmware, NOT jumptable shims.
    - The firmware's matter syscall layer resolves the exports via
      tc_blib_lookup("matter_*") when built lean (no built-in lib), else calls
      the built-in lib directly (build-time gate: lib → else plugin → else off).
    - The ONLY remaining host seam is the BearSSL subset matter_c needs
      (AES-CCM / EC-P256 i15 / ECDSA / HKDF / HMAC / SHA) — bundled (Fork A) or
      exported via tmod_ext_call / the jumptable (Fork B). jt[192..198] today
      export only br_gcm_* (AES-GCM, for SML decrypt) — a different subset.

  STAGE 2+ (later in this file): replace the probe exports with the real
  matter_c API, amalgamating tasmota/Plugins/matter/src/*.c via #include after
  the discipline pass (PROGMEM consts, fdiv/fmul, no 64-bit intrinsics).

  Copyright (C) 2026  Gerhard Mutz / claude
  GPL v3 (Tasmota's license)
*/

#include "tasmota_options.h"

#ifdef USE_MATTER_MOD

#define XBLIB_02  1

#include "module.h"
#include "module_defines.h"

PUSH_OPTIONS

// No per-instance state in the probe stage (matter_ctx lands here in Stage 2).
typedef struct {
  uint8_t _unused;
} MODULE_MEMORY;

// Revision must be >= MINREV (0x00010004 in xdrv_123_plugins.ino). Match the
// level of the shipping plugins (xblib_01_crc / xdrv_42_i2s use 1<<16|5).
MODULE_DESCRIPTOR("MATTER", MODULE_TYPE_BLIB, 1<<16|5,
                  "", 0, "", 0, "", 0, "", 0)

MODULE_PART int32_t matter_mod_probe(uint8_t *buf, int len);
MODULE_PART int32_t matter_mod_abi(uint8_t *buf, int len);
MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END

// ─── implementations ─────────────────────────────────────────────

// Liveness/version probe. A caller (TinyC bcall or firmware via
// tc_blib_lookup) gets a magic confirming the plugin is mapped + running and
// which stub revision it is. 'MTR' (0x4D5452) << 8 | version.
int32_t matter_mod_probe(uint8_t *buf, int len) {
  return 0x4D545201;   // 'M','T','R',0x01
}

// Matter data-model ABI revision the plugin was built against (1.4 → 14).
// In Stage 2 this returns the matter_c-reported ABI so the firmware can refuse
// a plugin/lib version mismatch before wiring the HAL.
int32_t matter_mod_abi(uint8_t *buf, int len) {
  return 14;
}

// ─── exports table ───────────────────────────────────────────────
// Read by the loader via mod_func_execute(pFUNC_GET_TINYC_EXPORTS). Names MUST
// be named PROGMEM arrays (not inline literals) so they land in the plugin's
// own mod_string section and EXEC_OFFSET-correct to valid runtime addresses
// (see the long comment in xblib_01_crc.cpp — inline literals crash).
static const char NAME_MATTER_PROBE[] PROGMEM = "matter_probe";
static const char NAME_MATTER_ABI[]   PROGMEM = "matter_abi";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_MATTER_PROBE, (void *)matter_mod_probe, 2, TC_RET_INT,
                       { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  { NAME_MATTER_ABI,   (void *)matter_mod_abi,   2, TC_RET_INT,
                       { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
  // Sentinel — name == NULL marks end of list.
  { NULL, NULL, 0, 0, { 0 } }
};

// ─── dispatch ────────────────────────────────────────────────────
// Two selectors, same contract as every BLIB:
//   pFUNC_INIT              — self-check + set initialized (read by deiniz).
//   pFUNC_GET_TINYC_EXPORTS — hand the loader BLIB_EXPORTS[] to register.
// Everything else returns 0 so the regular dispatch loops walk past us.
int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    GET_MTBL;
    initialized = 1;
    return 1;
  }
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];
  }
  return 0;
}

PULL_OPTIONS

#endif  // USE_MATTER_MOD
