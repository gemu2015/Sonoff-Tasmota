/*
  xblib_01_crc.cpp — CRC primitives for TinyC scripts.

  First example of a MODULE_TYPE_BLIB plugin: pure binary library
  exposing native functions to TinyC at full native speed, without
  participating in any Tasmota lifecycle. No FUNC_INIT, no FUNC_LOOP,
  no console commands — just a list of functions in a TC_EXPORT[]
  table that the plugin loader registers at module-map time.

  Why CRC: Modbus / Bresser / OBIS / 1-Wire / DLMS all have inner
  loops over CRCs at speeds where the TinyC VM (~30-100x slower than
  native) is the bottleneck. A 256-byte Modbus frame in TinyC is
  ~15 ms; native is ~50 µs. With this blib loaded, TinyC scripts get
  the native version transparently:

      // Modbus FC03 response framing in a TinyC script:
      char frame[260];
      int  n = serialRead(frame, 260);
      int  crc = bcall("mb_crc16", frame, n - 2);
      int  expected = (frame[n-2] & 0xff) | ((frame[n-1] & 0xff) << 8);
      if (crc == expected) {
        // good frame — parse it
      }

  Build via the existing BinPlugin pipeline:

      python3 tasmota/Plugins/build_plugin.py --plugin USE_CRC_BLIB --cpu esp32

  Upload the resulting build_output/firmware/CRC_BLIB.bin to a free
  plugin slot via Tasmota's "Mod" web form. Reboot. The blib is now
  loaded and `bcall("mb_crc16", buf, len)` works from any TinyC script
  on the device.

  Copyright (C) 2026  Gerhard Mutz / claude
  GPL v3 (Tasmota's license)
*/

#include "tasmota_options.h"

#ifdef USE_CRC_BLIB_MOD

#define XBLIB_01  1

#include "module.h"
#include "module_defines.h"

PUSH_OPTIONS

// Blibs hold no per-instance state — there's no MODULE_MEMORY here.
// (If a future blib needs a lookup table or a connection handle the
// MODULE_MEMORY pattern still works, allocated on first call by
// ALLOCMEM in mod_func_execute(pFUNC_INIT). Plain CRC is stateless.)
typedef struct {
  uint8_t _unused;
} MODULE_MEMORY;

// Revision must be >= MINREV (currently 0x00010004 in xdrv_123_plugins.ino).
// Match the level of other shipping plugins (xdrv_42_i2s uses 1<<16|5).
MODULE_DESCRIPTOR("CRC_BLIB", MODULE_TYPE_BLIB, 1<<16|5,
                  "", 0, "", 0, "", 0, "", 0)

MODULE_PART int      blib_crc16_modbus(uint8_t *buf, int len);
MODULE_PART uint32_t blib_crc32_iso(uint8_t *buf, int len);
MODULE_PART uint8_t  blib_crc8_dallas(uint8_t *buf, int len);
MODULE_PART int32_t  mod_func_execute(uint32_t sel);

MODULE_END

// ─── implementations ─────────────────────────────────────────────

// Modbus RTU CRC-16, polynomial 0xA001 (reversed 0x8005), init 0xFFFF.
// Same routine as xsns_53_sml.cpp's modbus path; the wire bytes are
// little-endian-appended to a frame, low-byte-first.
int blib_crc16_modbus(uint8_t *buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    crc ^= (uint16_t)buf[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else         crc =  crc >> 1;
    }
  }
  return (int)crc;
}

// CRC-32 ISO-3309 / Ethernet / zlib. Polynomial 0xEDB88320 (reversed
// 0x04C11DB7), init 0xFFFFFFFF, final XOR 0xFFFFFFFF. Useful for
// integrity checks on persisted state, OTA chunks, etc.
uint32_t blib_crc32_iso(uint8_t *buf, int len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (int i = 0; i < len; i++) {
    crc ^= (uint32_t)buf[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320u;
      else         crc =  crc >> 1;
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

// Dallas / Maxim 1-Wire 8-bit CRC. Polynomial 0x8C (reversed 0x31),
// init 0x00. Used by DS18B20 and similar 1-Wire devices.
uint8_t blib_crc8_dallas(uint8_t *buf, int len) {
  uint8_t crc = 0;
  for (int i = 0; i < len; i++) {
    uint8_t inbyte = buf[i];
    for (int b = 0; b < 8; b++) {
      uint8_t mix = (crc ^ inbyte) & 1;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

// ─── exports table ───────────────────────────────────────────────
// The plugin loader reads this via mod_func_execute(pFUNC_GET_TINYC_EXPORTS)
// after mapping the module. Function pointers carry their plugin-relative
// addresses; the loader applies EXEC_OFFSET once per entry before
// registering them in the global TinyC blib registry.
//
// CRITICAL: every const char* in the table MUST be a named PROGMEM array,
// not an inline string literal. Inline literals (e.g. `"mb_crc16"`) end
// up in `.flash.rodata` of the *builder firmware*, which doesn't exist
// on the device — adding EXEC_OFFSET to such an address produces a wild
// pointer (verified: crashed at 0x41a3ae23 on .39 with EXEC_OFFSET +
// builder-rodata-addr). PROGMEM-decorated named arrays land in the
// plugin's own `mod_string` section, alongside the rest of the plugin
// binary, so EXEC_OFFSET correction yields a valid runtime address.
//
// arg_types padded with TC_ARG_END so the firmware's marshalling shim
// can iterate without needing a separate length field.
static const char NAME_MB_CRC16[]    PROGMEM = "mb_crc16";
static const char NAME_CRC32[]       PROGMEM = "crc32";
static const char NAME_CRC8_DALLAS[] PROGMEM = "crc8_dallas";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  // mb_crc16(buf[], len) → int
  // Note: TC_ARG_BUF takes ONE TinyC arg (the buffer name) but the
  // marshaller pushes (void*, int) onto the native stack, so the
  // accompanying "len" argument the user passes from TinyC IS the
  // *count* the native function uses — there's no double-counting.
  { NAME_MB_CRC16,    (void *)blib_crc16_modbus, 2, TC_RET_INT,
                      { TC_ARG_BUF,  TC_ARG_INT, TC_ARG_END } },
  { NAME_CRC32,       (void *)blib_crc32_iso,    2, TC_RET_INT,
                      { TC_ARG_BUF,  TC_ARG_INT, TC_ARG_END } },
  { NAME_CRC8_DALLAS, (void *)blib_crc8_dallas,  2, TC_RET_INT,
                      { TC_ARG_BUF,  TC_ARG_INT, TC_ARG_END } },
  // Sentinel — name == NULL marks end of list.
  { NULL, NULL, 0, 0, { 0 } }
};

// ─── dispatch ────────────────────────────────────────────────────
// Blibs participate in only ONE selector: the loader's
// pFUNC_GET_TINYC_EXPORTS request returns the address of the exports
// table. Every other selector returns 0 / -1 cleanly so the regular
// xdrv/xsns dispatch loops walk past us harmlessly.
int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];
  }
  return 0;
}

PULL_OPTIONS

#endif  // USE_CRC_BLIB_MOD
