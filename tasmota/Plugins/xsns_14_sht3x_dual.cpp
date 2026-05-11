/*
  xsns_14_sht3x_dual.cpp — SHT3X / SHTC3 driver, dual-format.

  One source compiles to either a Tasmota BinPlugin or a native
  xsns_14 driver, picked by `BUILD_AS_PLUGIN`. Compat shim lives in
  `dual_format_compat.h` (shared with all other dual-format drivers).

  Plugin form: gated on USE_SHT3X_DUAL_MOD, builds via
  `tasmota32-plugin` env, output is SHT3X_*.bin.

  Native form: gated on USE_SHT3X_DUAL + USE_I2C, included via the
  thin shim at tasmota/tasmota_xsns_sensor/xsns_14_sht3x_dual.ino.
  The shim sets SHT3X_DUAL_NATIVE_INCLUDE before pulling this file
  in — without that flag the file is inert when PIO compiles it
  standalone (avoids duplicate-symbol link errors).

  Original copyright preserved from xsns_14_sht3x.{ino,cpp}.
  Copyright (C) 2021  Theo Arends
*/

#include "tasmota_options.h"

// Pick BUILD_AS_PLUGIN before including the compat header.
#ifndef BUILD_AS_PLUGIN
#  ifdef USE_SHT3X_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

// Top-level gate. See dual_format_compat.h's docstring for rationale.
#if BUILD_AS_PLUGIN
#  ifdef USE_SHT3X_DUAL_MOD
#    define _SHT3X_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_SHT3X_DUAL) && defined(SHT3X_DUAL_NATIVE_INCLUDE)
#    define _SHT3X_DUAL_ENABLED 1
#  endif
#endif

#ifdef _SHT3X_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define SHT3X_ADDR_GND   0x44
#define SHT3X_ADDR_VDD   0x45
#define SHTC3_ADDR       0x70
#define SHT3X_MAX_SENSORS 3

typedef struct {
  uint8_t address;
  uint8_t bus;            // I2C bus the sensor was found on (0 or 1)
  char    types[6];
} SHT3XSTRUCT;

const char  kShtTypes3[]    PROGMEM = "SHT3X|SHT3X|SHTC3";
const char  kShtTypes[]     PROGMEM = "%s%c%02X";
// File-unique names so multi-driver native co-link doesn't collide
// with HTTP_SNS_AHUM / FP_CONST in BMP / VL53L0X / MLX90614 / Bresser
// duals. Each driver's FLTC redefine restores `FLTC(idx)` to point
// at the local FP_CONST array.
const char  HTTP_SNS_AHUM_SHT3X[] PROGMEM = "{s}%s %s{m}%s g/m3{e}";
const float FP_CONST_SHT3X[]      PROGMEM = {65535, 45};
#undef  FLTC
#if BUILD_AS_PLUGIN
// ESP32-S3 lsi-from-PROGMEM trap: a direct `float[]` load via `lsi`
// can return garbage. Read as volatile uint32_t (forces l32i), then
// memcpy to float. EXEC_OFFSET applies the plugin loader's runtime
// relocation to the compile-time symbol address.
#  define FLTC(idx) ({ \
      volatile uint32_t _tmp = ((const volatile uint32_t*)((char*)FP_CONST_SHT3X + EXEC_OFFSET))[(idx)]; \
      float _f; \
      __builtin_memcpy(&_f, (void*)&_tmp, 4); \
      _f; \
    })
#else
#  define FLTC(idx)                             (FP_CONST_SHT3X[(idx)])
#endif

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls (which we still
// need because Sht3x_Detect calls SHT3X_Deinit before the latter's
// body appears in source order).
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
#define SHT3X_REV (1 << 16 | 5)
PUSH_OPTIONS
#ifdef USE_SOFTWIRE
#  define DEFAULT_SDA_PIN 12
#  define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("SHT3XS", MODULE_TYPE_SENSOR, SHT3X_REV,
                  "SDA", DEFAULT_SDA_PIN, "SCL", DEFAULT_SCL_PIN, "", 0, "", 0)
#else
MODULE_DESCRIPTOR("SHT3X",  MODULE_TYPE_SENSOR, SHT3X_REV,
                  "", 0, "", 0, "", 0, "", 0)
#endif
MODULE_PART int32_t Sht3x_Detect();
MODULE_PART void    SHT3X_Show(bool json);
MODULE_PART void    SHT3X_Deinit();
MODULE_PART bool    Sht3xRead(float &t, float &h, uint8_t sht3x_address);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// State storage — heap-allocated in BOTH modes. Plugin uses
// MODULE_MEMORY via the BinPlugin runtime; native uses an equivalent
// struct allocated lazily on FUNC_INIT, freed on detect-failure.
// Idle RAM cost when no SHT31 is on the bus: 0 bytes plugin / 4 bytes
// native (a single nullptr).
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp
// for the pattern rationale). `initialized_flag` is native-only —
// plugin already has `#define initialized mt->flags.initialized` from
// module_defines.h:590, so only native maps `initialized` → mem field.
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  sht3x_state_t
#endif

typedef struct {
  TWIp        *xWire;
  uint8_t      sht3x_count;
  uint8_t      sht3x_addresses[3];
  SHT3XSTRUCT  sht3x_sensors[SHT3X_MAX_SENSORS];
  bool         initialized_flag;
} MODULE_MEMORY;

#define sht3x_count       mem->sht3x_count
#define sht3x_addresses   mem->sht3x_addresses
#define sht3x_sensors     mem->sht3x_sensors

#if BUILD_AS_PLUGIN

#  ifdef USE_SOFTWIRE
#    include "Softwire/Softwire_cpp.h"
#  endif

#else  // native — override SETREGS / ALLOCMEM / RETMEM to bind `mem`
       // to the file-static state pointer. ALLOCMEM also declares `mem`
       // so callers like Init can use it immediately.

static sht3x_state_t *sht3x_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = sht3x_state;
#  define ALLOCMEM \
       if (!sht3x_state) sht3x_state = (sht3x_state_t *)calloc(1, sizeof(sht3x_state_t)); \
       if (!sht3x_state) return -1; \
       MODULE_MEMORY *mem = sht3x_state;
#  define RETMEM \
       if (sht3x_state) { free(sht3x_state); sht3x_state = nullptr; }
#  define initialized       mem->initialized_flag

#  define XSNS_14   14
#  define XI2C_15   15

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core — IDENTICAL source for both modes thanks to the compat
// layer in dual_format_compat.h.
// --------------------------------------------------------------------

bool Sht3xRead(float &t, float &h, uint8_t sht3x_address) {
  SETREGS
  unsigned int data[6];

  t = jNAN;
  h = t;

  I2C_beginTransmission(sht3x_address);
  if (SHTC3_ADDR == sht3x_address) {
    I2C_write(0x35);
    I2C_write(0x17);
    I2C_endTransmission(true);
    I2C_beginTransmission(sht3x_address);
    I2C_write(0x78);
    I2C_write(0x66);
  } else {
    I2C_write(0x2C);
    I2C_write(0x06);
  }
  if (I2C_endTransmission(true) != 0) {
    AddLog(LOG_LEVEL_INFO, PSTR("i2c error"));
    return false;
  }
  delay(30);
  I2C_requestFrom(sht3x_address, 6);
  for (uint32_t i = 0; i < 6; i++) {
    data[i] = I2C_read();
  }

  t = fdiv(tofloat(((data[0] << 8) | data[1]) * 175), FLTC(0));
  t = fdiff(t, FLTC(1));
  t = ConvertTemp(t);

  h = fdiv(tofloat(((data[3] << 8) | data[4]) * 100), FLTC(0));
  h = ConvertHumidity(h);
  return (!isnan(t) && !isnan(h) && !iseq(h));
}

int32_t Sht3x_Detect() {
  ALLOCMEM
  sht3x_addresses[0] = SHT3X_ADDR_GND;
  sht3x_addresses[1] = SHT3X_ADDR_VDD;
  sht3x_addresses[2] = SHTC3_ADDR;

  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    for (uint32_t i = 0; i < SHT3X_MAX_SENSORS; i++) {
      if (!I2C_SetDevice(sht3x_addresses[i], bus)) { continue; }
      float t;
      float h;
      if (Sht3xRead(t, h, sht3x_addresses[i])) {
        sht3x_sensors[sht3x_count].address = sht3x_addresses[i];
        sht3x_sensors[sht3x_count].bus     = bus;
        GetTextIndexed(sht3x_sensors[sht3x_count].types,
                       sizeof(sht3x_sensors[sht3x_count].types),
                       i, GSTR(kShtTypes3));
        I2C_SetActiveFound(sht3x_sensors[sht3x_count].address,
                           sht3x_sensors[sht3x_count].types, bus);
        sht3x_count++;
      } else {
        // Address scanned-positive but no valid response — release
        // so the next bus iteration can claim the address.
        I2C_ResetActive(sht3x_addresses[i], bus);
      }
    }
    if (sht3x_count) { break; }
  }

  if (sht3x_count) {
    initialized = true;
  } else {
    SHT3X_Deinit();
  }
  return sht3x_count;
}

void SHT3X_Show(bool json) {
  SETREGS
  STGLOB

  for (uint32_t i = 0; i < sht3x_count; i++) {
    float t;
    float h;
    I2C_SETWIRE(sht3x_sensors[i].bus);
    if (Sht3xRead(t, h, sht3x_sensors[i].address)) {
      char types[11];
      strlcpy(types, sht3x_sensors[i].types, sizeof(types));
      if (sht3x_count > 1) {
        snprintf_P(types, sizeof(types), GSTR(kShtTypes),
                   sht3x_sensors[i].types, IndexSeparator(),
                   sht3x_sensors[i].address);
      }
#if BUILD_AS_PLUGIN
      TempHumDewShow(json, ((0 == TasmotaGlobal->tele_period) && (0 == i)),
                     types, t, h);
#else
      TempHumDewShow(json, ((0 == TasmotaGlobal.tele_period) && (0 == i)),
                     types, t, h);
#endif
      float abshum = CalcTempHumToAbsHum(t, h);
      char  abs_hum[32];
      ftostrfd(abshum, 4, abs_hum);
      if (!json) {
        char s1[32];
        WSContentSend_PD(GSTR(HTTP_SNS_AHUM_SHT3X), types,
                         Plugin_Get_SensorNames(s1, iD_ABSOLUTE_HUMIDITY),
                         abs_hum);
      }
    }
  }
}

void SHT3X_Deinit() {
  SETREGS
  for (uint32_t i = 0; i < sht3x_count; i++) {
    I2C_ResetActive(sht3x_sensors[i].address, sht3x_sensors[i].bus);
  }
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher — the single point where the two modes diverge.
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:        result = Sht3x_Detect(); break;
    case pFUNC_JSON_APPEND: SHT3X_Show(1);           break;
    case pFUNC_WEB_SENSOR:  SHT3X_Show(0);           break;
    case pFUNC_DEINIT:      SHT3X_Deinit();          break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns14(uint32_t function) {
  if (!I2cEnabled(XI2C_15)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    Sht3x_Detect();
  }
  // sht3x_count expands to sht3x_state->sht3x_count; null-guard or
  // we crash when no SHT31 was found at boot.
  else if (sht3x_state && sht3x_count) {
    switch (function) {
      case FUNC_JSON_APPEND: SHT3X_Show(1); break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:  SHT3X_Show(0); break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor + helper macros so they don't leak
// into other dual drivers in the merged tasmota.ino.cpp TU.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef sht3x_count
#  undef sht3x_addresses
#  undef sht3x_sensors
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef FLTC

#endif  // _SHT3X_DUAL_ENABLED
