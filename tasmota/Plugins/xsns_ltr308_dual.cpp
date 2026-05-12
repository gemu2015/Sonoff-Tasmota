/*
  xsns_ltr308_dual.cpp — Lite-On LTR-308ALS-01 ambient-light sensor
  driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2024 Gerhard Mutz

  Plugin: USE_LTR308_DUAL_MOD via build_plugin.py.
  Native: USE_LTR308_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_ltr308_dual.ino.

  First dual conversion that uses the in-function `MODULE_MEMORY *mem`
  local pattern (via SETMEMREGS) instead of the global-state-accessor
  #define-macro pattern of the other duals. The driver body that
  uses `mem->field` is BIT-IDENTICAL between plugin and native — no
  per-field accessor macros needed.

  This is achieved in native mode by:
    - Per-driver state typedef `ltr308_mem_t` (file-unique; can't
      collide with other dual drivers' MODULE_MEMORY in the merged
      tasmota.ino.cpp TU because we never emit a typedef called
      MODULE_MEMORY in native mode).
    - `#define MODULE_MEMORY ltr308_mem_t` so the source can still
      write `MODULE_MEMORY *mem = ...` if it wants.
    - `SETMEMREGS` redefined to `MODULE_MEMORY *mem = ltr308_state`
      — the heap state pointer. Same `mem->X` access shape.
    - `ALLOCMEM` redefined to lazy-calloc the state AND declare the
      `mem` local in the same expansion.

  Hardware:
    Address 0x53 (fixed), 16/17/18/19/20-bit ALS, gain 1/3/6/9/18×.
    Lux = 0.6 × ALS_DATA / (gain_factor × integration_time_seconds)
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_LTR308_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_LTR308_DUAL_MOD
#    define _LTR308_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_LTR308_DUAL) && defined(LTR308_DUAL_NATIVE_INCLUDE)
#    define _LTR308_DUAL_ENABLED 1
#  endif
#endif

#ifdef _LTR308_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define LTR308_ADDR         0x53
#define LTR308_REV          (1 << 16 | 5)

// Register addresses
#define LTR308_MAIN_CTRL    0x00
#define LTR308_MEAS_RATE    0x04
#define LTR308_ALS_GAIN     0x05
#define LTR308_PART_ID      0x06
#define LTR308_MAIN_STATUS  0x07
#define LTR308_ALS_DATA_0   0x0D
#define LTR308_ALS_DATA_1   0x0E
#define LTR308_ALS_DATA_2   0x0F
#define LTR308_INT_CFG      0x19
#define LTR308_INT_PST      0x1A

// MAIN_CTRL bits
#define LTR308_ALS_ENABLE   0x02
#define LTR308_SW_RESET     0x10

// ALS_GAIN values
#define LTR308_GAIN_1X      0x00
#define LTR308_GAIN_3X      0x01
#define LTR308_GAIN_6X      0x02
#define LTR308_GAIN_9X      0x03
#define LTR308_GAIN_18X     0x04

// MEAS_RATE — resolution (bits 6:4) and rate (bits 2:0)
#define LTR308_RES_20BIT    (0x00 << 4)  // 400 ms
#define LTR308_RES_19BIT    (0x01 << 4)  // 200 ms
#define LTR308_RES_18BIT    (0x02 << 4)  // 100 ms (default)
#define LTR308_RES_17BIT    (0x03 << 4)  // 50  ms
#define LTR308_RES_16BIT    (0x04 << 4)  // 25  ms
#define LTR308_RATE_25MS    0x00
#define LTR308_RATE_50MS    0x01
#define LTR308_RATE_100MS   0x02         // default
#define LTR308_RATE_500MS   0x03
#define LTR308_RATE_1000MS  0x05
#define LTR308_RATE_2000MS  0x06

// MAIN_STATUS bits
#define LTR308_DATA_STATUS  0x08
#define LTR308_POWER_ON     0x20

// Expected PART_ID for LTR-308ALS-01
#define LTR308_PART_ID_VAL  0xB1

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("LTR308", MODULE_TYPE_SENSOR, LTR308_REV,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART int32_t  LTR308_Detect(void);
MODULE_PART void     LTR308_Show(bool json);
MODULE_PART void     LTR308_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// State storage — heap in both modes; bound to a local `mem` pointer
// inside each function (via SETMEMREGS) so the access pattern is
// identical in plugin and native.
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
// (LTR308 was the first dual to use the in-function `mem` pattern;
// this brings it in line with the rest of the dual lineup.)
#define DUAL_NATIVE_NAME    ltr308
#define DUAL_NATIVE_STATE_T ltr308_mem_t
#include "dual_format_native_state.h"
typedef struct {
  TWIp   *xWire;
  uint8_t address;
  uint8_t bus;
  uint8_t gain;
  uint8_t resolution;
  float   lux;
  bool    valid;
  bool    LTR308_detected;
  bool    initialized_flag;
} MODULE_MEMORY;

#if !BUILD_AS_PLUGIN

DUAL_NATIVE_STATE_PTR_DECL
#  define XSNS_56         56
#  define XI2C_56         56

#endif  // !BUILD_AS_PLUGIN

const float FP_CONST_LTR[] PROGMEM = {
  0.6,   // [0] lux formula numerator factor
  1.0,   // [1] gain 1×
  3.0,   // [2] gain 3×
  6.0,   // [3] gain 6×
  9.0,   // [4] gain 9×
  18.0,  // [5] gain 18×
  0.4,   // [6] integration 400 ms (20-bit)
  0.2,   // [7] integration 200 ms (19-bit)
  0.1,   // [8] integration 100 ms (18-bit) — default
  0.05,  // [9] integration  50 ms (17-bit)
  0.025  // [10] integration 25 ms (16-bit)
};
// Override the generic FLTC pointing at this driver's table.
#define DUAL_FLTC_TABLE FP_CONST_LTR
#include "dual_format_fltc.h"
const char HTTP_LTR308_LUX_DUAL[] PROGMEM = "{s}LTR308 Illuminance{m}%s lux{e}";

// --------------------------------------------------------------------
// I2C helpers — use mem->address / mem->bus pinned at SETMEMREGS time.
// --------------------------------------------------------------------
bool ltr308_write_reg(uint8_t reg, uint8_t val) {
  SETMEMREGS
  I2C_SETWIRE(mem->bus);
  I2C_beginTransmission(mem->address);
  I2C_write(reg);
  I2C_write(val);
  return (I2C_endTransmission(true) == 0);
}

uint8_t ltr308_read_reg(uint8_t reg) {
  SETMEMREGS
  I2C_SETWIRE(mem->bus);
  I2C_beginTransmission(mem->address);
  I2C_write(reg);
  I2C_endTransmission(false);
  I2C_requestFrom(mem->address, 1);
  return I2C_read();
}

uint32_t ltr308_read_als_data(void) {
  SETMEMREGS
  I2C_SETWIRE(mem->bus);
  I2C_beginTransmission(mem->address);
  I2C_write(LTR308_ALS_DATA_0);
  I2C_endTransmission(false);
  I2C_requestFrom(mem->address, 3);
  uint8_t d0 = I2C_read();
  uint8_t d1 = I2C_read();
  uint8_t d2 = I2C_read();
  return (uint32_t)d0 | ((uint32_t)d1 << 8) | ((uint32_t)(d2 & 0x0F) << 16);
}

float ltr308_get_gain_factor(void) {
  SETMEMREGS
  uint8_t idx = mem->gain;
  if (idx > 4) { idx = 0; }
  return FLTC(idx + 1);
}

float ltr308_get_int_time(void) {
  SETMEMREGS
  uint8_t idx = mem->resolution;
  if (idx > 4) { idx = 2; }  // default 18-bit
  return FLTC(idx + 6);
}

// Lux = 0.6 × ALS_DATA / (gain × integration_time)
float ltr308_calc_lux(uint32_t raw) {
  SETMEMREGS
  (void)mem;  // FLTC accesses constants only; mem unused here
  float numerator   = fmul(FLTC(0), tofloat(raw));
  float denominator = fmul(ltr308_get_gain_factor(), ltr308_get_int_time());
  return fdiv(numerator, denominator);
}

void ltr308_read_sensor(void) {
  SETMEMREGS
  if (!mem->LTR308_detected) { return; }

  uint8_t status = ltr308_read_reg(LTR308_MAIN_STATUS);
  if (status & LTR308_DATA_STATUS) {
    uint32_t raw = ltr308_read_als_data();
    mem->lux   = ltr308_calc_lux(raw);
    mem->valid = true;
  }
}

// --------------------------------------------------------------------
// Detect — probe both buses for the LTR-308ALS-01 chip ID.
// --------------------------------------------------------------------
int32_t LTR308_Detect(void) {
  ALLOCMEM

  mem->address          = LTR308_ADDR;
  mem->bus              = 0;
  mem->gain             = LTR308_GAIN_3X;
  mem->resolution       = 2;             // 18-bit (FP_CONST index)
  mem->valid            = false;
  mem->lux              = tofloat(0);
  mem->LTR308_detected  = false;

  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(LTR308_ADDR, bus)) { continue; }
    mem->bus = bus;
    uint8_t part_id = ltr308_read_reg(LTR308_PART_ID);
    if (part_id != LTR308_PART_ID_VAL) {
      I2C_ResetActive(LTR308_ADDR, bus);
      continue;
    }
    // Found it. Reset, configure, enable.
    ltr308_write_reg(LTR308_MAIN_CTRL, LTR308_SW_RESET);
    delay(10);
    ltr308_write_reg(LTR308_MEAS_RATE, LTR308_RES_18BIT | LTR308_RATE_100MS);
    ltr308_write_reg(LTR308_ALS_GAIN,  LTR308_GAIN_3X);
    ltr308_write_reg(LTR308_MAIN_CTRL, LTR308_ALS_ENABLE);

    I2C_SetActiveFound(LTR308_ADDR, "LTR308", bus);
    initialized = true;
    mem->LTR308_detected = true;
    AddLog(LOG_LEVEL_INFO,
           PSTR("LTR308: found at bus %d addr 0x%02X part 0x%02X"),
           (int)bus, LTR308_ADDR, part_id);
    return true;
  }

  LTR308_Deinit();
  return false;
}

void LTR308_Show(bool json) {
  // SETREGS already declares the `mem` local; STGLOB sets up tgbl.
  // Don't add SETMEMREGS — it would re-declare `mem`.
  SETREGS
  STGLOB

  if (!mem->LTR308_detected) { return; }
  if (!mem->valid)           { return; }

  char lux_str[32];
  ftostrfd(mem->lux, 1, lux_str);

  if (json) {
    ResponseAppend_P(PSTR(",\"LTR308\":{\"Illuminance\":%s}"), lux_str);
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(HTTP_LTR308_LUX_DUAL), lux_str);
#endif
  }
}

void LTR308_Deinit(void) {
  SETREGS
  // Best-effort standby write — works only if state is alive.
  if (mem) {
    ltr308_write_reg(LTR308_MAIN_CTRL, 0x00);
    I2C_ResetActive(mem->address, mem->bus);
  }
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = LTR308_Detect(); break;
    case pFUNC_EVERY_SECOND: ltr308_read_sensor();     break;
    case pFUNC_JSON_APPEND:  LTR308_Show(1);           break;
    case pFUNC_WEB_SENSOR:   LTR308_Show(0);           break;
    case pFUNC_DEINIT:       LTR308_Deinit();          break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

// XSNS_56 picked as a free slot for the dual driver (the legacy
// xsns_ltr308.cpp had no native counterpart, so any unused Xsns slot
// is fine — adjust to whatever Tasmota slot is available).
bool Xsns56(uint32_t function) {
  if (!I2cEnabled(XI2C_56)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    LTR308_Detect();
  } else if (ltr308_state && initialized) {
    switch (function) {
      case FUNC_EVERY_SECOND: ltr308_read_sensor(); break;
      case FUNC_JSON_APPEND:  LTR308_Show(1);       break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   LTR308_Show(0);       break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor / helper macros so they don't leak
// into other dual drivers in the merged tasmota.ino.cpp TU.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef MODULE_MEMORY
#  undef SETMEMREGS
#  undef ALLOCMEM
#  undef RETMEM
#  undef initialized
#endif
#undef FLTC

#endif  // _LTR308_DUAL_ENABLED
