/*
  xsns_09_bmp_dual.cpp — BMP180/BMP280/BME280/BME680 driver,
  dual-format. Compat shim in dual_format_compat.h.

  Plugin: USE_BME_DUAL_MOD via build_plugin.py.
  Native: USE_BME_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_09_bmp_dual.ino.

  Original copyright preserved.
  Copyright (C) 2021 Gerhard Mutz
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_BME_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  include "../Tasmota/include/i18n.h"   // D_JSON_TEMPERATURE etc.
#endif

#if BUILD_AS_PLUGIN
#  ifdef USE_BME_DUAL_MOD
#    define _BME_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_BME_DUAL) && defined(BME_DUAL_NATIVE_INCLUDE)
#    define _BME_DUAL_ENABLED 1
#  endif
#endif

#ifdef _BME_DUAL_ENABLED

#define BME280_I2C_ADDRESS1 (0x76)
#define BME280_I2C_ADDRESS2 (0x77)

#define BME280_CAL_T1 (0x88)
#define BME280_CAL_T2 (0x8a)
#define BME280_CAL_T3 (0x8c)
#define BME280_CAL_P1 (0x8e)
#define BME280_CAL_P2 (0x90)
#define BME280_CAL_P3 (0x92)
#define BME280_CAL_P4 (0x94)
#define BME280_CAL_P5 (0x96)
#define BME280_CAL_P6 (0x98)
#define BME280_CAL_P7 (0x9a)
#define BME280_CAL_P8 (0x9c)
#define BME280_CAL_P9 (0x9e)
#define BME280_CAL_H1 (0xa1)
#define BME280_CAL_H2 (0xe1)
#define BME280_CAL_H3 (0xe3)
#define BME280_CAL_H4 (0xe4)
#define BME280_CAL_H45 (0xe5)
#define BME280_CAL_H5 (0xe6)
#define BME280_CAL_H6 (0xe7)

#define BME280_ID_REGISTER (0xd0)
#define BME280_RESET_REGISTER (0xe0)
#define BME280_CTRL_HUM_REGISTER (0xf2)
#define BME280_STATUS_REGISTER (0xf3)
#define BME280_CTRL_MEAS_REGISTER (0xf4)
#define BME280_CONFIG_REGISTER (0xf5)
#define BME280_PRESSURE (0xf7)
#define BME280_TEMPERATURE (0xfa)
#define BME280_HUMIDITY (0xfd)

#define BMP180_CHIPID 0x55
#define BMP280_CHIPID 0x58
#define BME280_CHIPID 0x60
#define BME680_CHIPID 0x61

typedef int32_t  temperature_t;
typedef uint32_t pressure_t;
typedef uint32_t humidity_t;

typedef struct {
  uint16_t _dig_T1;
  int16_t  _dig_T2, _dig_T3;
  uint16_t _dig_P1;
  int16_t  _dig_P2, _dig_P3, _dig_P4, _dig_P5, _dig_P6, _dig_P7, _dig_P8, _dig_P9;
  uint8_t  _dig_H1;
  int16_t  _dig_H2;
  uint8_t  _dig_H3;
  int16_t  _dig_H4, _dig_H5;
  int8_t   _dig_H6;
} BMECAL;

const char  BMEtypes[]      PROGMEM = "BMP180|BME280|BMP280|BME680";
const char  HTTP_BMP_T[]    PROGMEM = "{s}%s %s{m}%s C{e}";
const char  HTTP_BMP_P[]    PROGMEM = "{s}%s %s{m}%s hp{e}";
// File-unique name to avoid collision with SHT3X dual.
const char  HTTP_SNS_AHUM_BMP[] PROGMEM = "{s}%s %s{m}%s g/m3{e}";
#if BUILD_AS_PLUGIN
const char  JSON_BMP[]      PROGMEM = ",\"%s\":{\"" D_JSON_TEMPERATURE "\":%s,\"" D_JSON_PRESSURE "\":%s";
const char  JSON_BME[]      PROGMEM = ",\"" D_JSON_HUMIDITY "\":%s,\"" D_JSON_AHUM "\":%s}";
#else
// Native build has these strings via tasmota_options.h's i18n chain
const char  JSON_BMP[]      PROGMEM = ",\"%s\":{\"" D_JSON_TEMPERATURE "\":%s,\"" D_JSON_PRESSURE "\":%s";
const char  JSON_BME[]      PROGMEM = ",\"" D_JSON_HUMIDITY "\":%s,\"" D_JSON_AHUM "\":%s}";
#endif
const char  JSON_BMPend[]   PROGMEM = "}";
// File-unique FP_CONST + per-driver FLTC redefine — each dual has
// its own constants table so the names need to be unique in the
// merged tasmota.ino.cpp TU.
const float FP_CONST_BMP[]  PROGMEM = {0, 0.01, 0.00097656};
#define DUAL_FLTC_TABLE FP_CONST_BMP
#include "dual_format_fltc.h"
// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
#define BMX_REV (1 << 16 | 5)
PUSH_OPTIONS
#ifdef USE_SOFTWIRE
#  define DEFAULT_SDA_PIN 12
#  define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("BMXx80S", MODULE_TYPE_SENSOR, BMX_REV,
                  "SDA", DEFAULT_SDA_PIN, "SCL", DEFAULT_SCL_PIN, "", 0, "", 0)
#else
MODULE_DESCRIPTOR("BMXx80",  MODULE_TYPE_SENSOR, BMX_REV,
                  "", 0, "", 0, "", 0, "", 0)
#endif
MODULE_PART int32_t        Init_BME();
MODULE_PART void           BME_clearCalibrationData();
MODULE_PART void           BME_readCalibrationData();
MODULE_PART uint32_t       BME_Read(uint8_t reg, int8_t num);
MODULE_PART uint32_t       BME_Write(uint8_t reg, int8_t val);
MODULE_PART void           BME_Show(uint32_t json);
MODULE_PART void           BME_Deinit();
MODULE_PART void           BME_Every_Second();
MODULE_PART humidity_t     compensateHumidity(int32_t adc_H);
MODULE_PART temperature_t  compensateTemperature(int32_t adc_T);
MODULE_PART pressure_t     compensatePressure(int32_t adc_P);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t        mod_func_execute(uint32_t sel);
#endif
MODULE_END

// State — unified struct definition for plugin + native. In native mode
// MODULE_MEMORY is `#define`-aliased to bme_state_t (file-unique tag so
// it doesn't collide with other duals' MODULE_MEMORY in the merged
// tasmota.ino.cpp TU). Both modes use `mem->field` access; SETREGS /
// ALLOCMEM / RETMEM declare/manage the `mem` local.
//
// Mode-specific fields stay in the unified struct:
//   xWire           — used only in plugin (TWIp* per-instance bus handle).
//                     ~4 B unused in native (acceptable, keeps code linear).
//   initialized_flag — used only in native. ~1 B unused in plugin.
#define DUAL_NATIVE_NAME    bme
#define DUAL_NATIVE_STATE_T bme_state_t
#include "dual_format_native_state.h"
typedef struct {
  TWIp    *xWire;
  float    hum;
  float    abshum;
  float    temp;
  int32_t  _t_fine;
  float    press;
  uint8_t  type;
  uint8_t  i2c_addr;
  uint8_t  i2c_bus;
  BMECAL   bmc;
  char     typestr[8];
  bool     ready;
  bool     initialized_flag;
} MODULE_MEMORY;

#define temp        mem->temp
#define _t_fine     mem->_t_fine
#define hum         mem->hum
#define press       mem->press
#define type        mem->type
#define typestr     mem->typestr
#define abshum      mem->abshum
#define i2c_addr    mem->i2c_addr
#define i2c_bus     mem->i2c_bus
#define bmc         mem->bmc
#define ready       mem->ready
// NOTE: `initialized` is plugin-loader-managed: module_defines.h has
// `#define initialized mt->flags.initialized`, which gates whether the
// loader dispatches non-INIT calls (JSON / WEB_SENSOR / EVERY_SECOND)
// to this slot. Overriding to `mem->initialized_flag` in plugin mode
// causes Init_BME's `initialized = true` to write the struct field
// instead of the loader flag → slot stays "uninitialised" → never
// renders. Map to mem->initialized_flag only in native mode (below).

#if BUILD_AS_PLUGIN

#  ifdef USE_SOFTWIRE
#    include "Softwire/Softwire_cpp.h"
#  endif

#else  // native — override SETREGS / ALLOCMEM / RETMEM to bind `mem`
       // to the file-static global state pointer. Native ALLOCMEM also
       // emits the `mem` local declaration so functions that start with
       // ALLOCMEM (e.g. Init_BME) have `mem` available afterwards —
       // matching the plugin shape.

DUAL_NATIVE_STATE_PTR_DECL
#  define XSNS_09     9
#  define XI2C_10     10

#endif  // BUILD_AS_PLUGIN

// Driver core — single bus (0), probes both possible addresses
// (0x76, 0x77) and accepts only known chip IDs. Address list is
// populated per-element (plugin Rule 1: no `static const T[] = {…}`
// — those silently return garbage in plugin context).
int32_t Init_BME() {
  ALLOCMEM

  ready   = false;
  i2c_bus = 0;

  uint8_t addrs[2];
  addrs[0] = BME280_I2C_ADDRESS1;
  addrs[1] = BME280_I2C_ADDRESS2;

  // Probe both I²C busses (MAX_I2C_Busses == 2 on ESP32) — matches
  // the SHT3X / HTU21 / CCS811 / MLX90614 / VL53L0X pattern. The
  // outer `bus` loop is required: without it, sensors on Wire1
  // (e.g. an EPD47 device using its secondary I²C for sensors)
  // are silently invisible to the BMP driver.
  bool found = false;
  for (uint32_t bus = 0; bus < MAX_I2C_Busses && !found; bus++) {
    I2C_SETWIRE(bus);
    for (uint32_t a = 0; a < 2; a++) {
      i2c_addr = addrs[a];
      if (!I2C_SetDevice(i2c_addr, bus)) { continue; }
      type = BME_Read(BME280_ID_REGISTER, 1);
      if (type == BMP180_CHIPID || type == BME280_CHIPID
          || type == BMP280_CHIPID || type == BME680_CHIPID) {
        i2c_bus = bus;
        found = true;
        break;
      }
      // Probed positive at the address bus level (someone ACK'd) but
      // the chip-ID register doesn't match anything we know — release
      // so the next iteration / driver can claim the address.
      I2C_ResetActive(i2c_addr, bus);
    }
  }

  if (!found) {
    BME_Deinit();
    return -1;
  }

  uint8_t index = 0;
  if (type == BMP180_CHIPID)      index = 0;
  else if (type == BME280_CHIPID) index = 1;
  else if (type == BMP280_CHIPID) index = 2;
  else if (type == BME680_CHIPID) index = 3;

  GetTextIndexed(typestr, sizeof(typestr), index, GSTR(BMEtypes));
  I2C_SetActiveFound(i2c_addr, typestr, i2c_bus);

  // Calibration read uses the same wire — explicitly re-select to
  // be safe in case any helper above touched I2C_SETWIRE state.
  I2C_SETWIRE(i2c_bus);
  BME_readCalibrationData();

  initialized = true;
  ready = true;
  return ready;
}

uint32_t BME_Read(uint8_t reg, int8_t num) {
  SETREGS
  I2C_beginTransmission(i2c_addr);
  I2C_write(reg);
  I2C_endTransmission(false);
  I2C_requestFrom(i2c_addr, abs(num));
  uint32_t result = 0;
  if (num > 0) {
    for (uint16_t cnt = 0; cnt < num; cnt++) {
      result <<= 8;
      result |= I2C_read();
    }
  } else {
    result = I2C_read();
    result |= I2C_read() << 8;
  }
  return result;
}

uint32_t BME_Write(uint8_t reg, int8_t val) {
  SETREGS
  I2C_beginTransmission(i2c_addr);
  I2C_write(reg);
  I2C_write(val);
  return I2C_endTransmission(true);
}

void BME_readCalibrationData() {
  SETREGS
  bmc._dig_T1 = (uint16_t)BME_Read(BME280_CAL_T1, -2);
  bmc._dig_T2 = (int16_t) BME_Read(BME280_CAL_T2, -2);
  bmc._dig_T3 = (int16_t) BME_Read(BME280_CAL_T3, -2);
  bmc._dig_P1 = (uint16_t)BME_Read(BME280_CAL_P1, -2);
  bmc._dig_P2 = (int16_t) BME_Read(BME280_CAL_P2, -2);
  bmc._dig_P3 = (int16_t) BME_Read(BME280_CAL_P3, -2);
  bmc._dig_P4 = (int16_t) BME_Read(BME280_CAL_P4, -2);
  bmc._dig_P5 = (int16_t) BME_Read(BME280_CAL_P5, -2);
  bmc._dig_P6 = (int16_t) BME_Read(BME280_CAL_P6, -2);
  bmc._dig_P7 = (int16_t) BME_Read(BME280_CAL_P7, -2);
  bmc._dig_P8 = (int16_t) BME_Read(BME280_CAL_P8, -2);
  bmc._dig_P9 = (int16_t) BME_Read(BME280_CAL_P9, -2);

  if (type == BME280_CHIPID) {
    bmc._dig_H1 = BME_Read(BME280_CAL_H1, 1);
    bmc._dig_H2 = (int16_t)BME_Read(BME280_CAL_H2, -2);
    bmc._dig_H3 = BME_Read(BME280_CAL_H3, 1);
    uint8_t temp1 = BME_Read(BME280_CAL_H4, 1);
    uint8_t temp2 = BME_Read(BME280_CAL_H45, 1);
    uint8_t temp3 = BME_Read(BME280_CAL_H5, 1);
    bmc._dig_H4 = (temp1 << 4) | (temp2 & 0x0f);
    bmc._dig_H5 = (temp3 << 4) | (temp2 >> 4);
    bmc._dig_H6 = (int8_t)BME_Read(BME280_CAL_H6, 1);

    BME_Write(BME280_CTRL_MEAS_REGISTER, 0x00);
    BME_Write(BME280_CTRL_HUM_REGISTER, 0x01);
    BME_Write(BME280_CONFIG_REGISTER, 0xA0);
    BME_Write(BME280_CTRL_MEAS_REGISTER, 0x27);
  } else {
    BME_Write(BME280_CTRL_MEAS_REGISTER, 0xb7);
  }
}

temperature_t compensateTemperature(int32_t adc_T) {
  SETREGS
  int32_t var1, var2, T;
  var1 = ((((adc_T >> 3) - ((int32_t)bmc._dig_T1 << 1))) * ((int32_t)bmc._dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)bmc._dig_T1)) * ((adc_T >> 4) - ((int32_t)bmc._dig_T1))) >> 12) *
          ((int32_t)bmc._dig_T3)) >> 14;
  _t_fine = var1 + var2;
  T = (_t_fine * 5 + 128) >> 8;
  return T;
}

pressure_t compensatePressure(int32_t adc_P) {
  SETREGS
  int32_t var1, var2;
  uint32_t p;
  var1 = (((int32_t)_t_fine) >> 1) - (int32_t)ICONST(64000);
  var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)bmc._dig_P6);
  var2 = var2 + ((var1 * ((int32_t)bmc._dig_P5)) << 1);
  var2 = (var2 >> 2) + (((int32_t)bmc._dig_P4) << 16);
  var1 = (((bmc._dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
          ((((int32_t)bmc._dig_P2) * var1) >> 1)) >> 18;
  var1 = ((((ICONST(32768) + var1)) * ((int32_t)bmc._dig_P1)) >> 15);
  if (var1 == 0) { return 0; }
  p = (((uint32_t)(((int32_t)ICONST(1048576)) - adc_P) - (var2 >> 12))) * ICONST(3125);
  if (p < ICONST(0x80000000)) {
    p = tmod__udivsi3((p << 1), (uint32_t)var1);
  } else {
    p = tmod__udivsi3(p, (uint32_t)var1) * 2;
  }
  var1 = (((int32_t)bmc._dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
  var2 = (((int32_t)(p >> 2)) * ((int32_t)bmc._dig_P8)) >> 13;
  p = (uint32_t)((int32_t)p + ((var1 + var2 + bmc._dig_P7) >> 4));
  return p;
}

humidity_t compensateHumidity(int32_t adc_H) {
  SETREGS
  int32_t v_x1_u32r;
  v_x1_u32r = (_t_fine - ((int32_t)ICONST(76800)));
  v_x1_u32r =
      (((((adc_H << 14) - (((int32_t)bmc._dig_H4) << 20) - (((int32_t)bmc._dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
       (((((((v_x1_u32r * ((int32_t)bmc._dig_H6)) >> 10) *
            (((v_x1_u32r * ((int32_t)bmc._dig_H3)) >> 11) + ((int32_t)ICONST(32768)))) >> 10) +
          ((int32_t)ICONST(2097152))) * ((int32_t)bmc._dig_H2) + ICONST(8192)) >> 14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)bmc._dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > ICONST(419430400) ? ICONST(419430400) : v_x1_u32r);
  return (uint32_t)(v_x1_u32r >> 12);
}

void BME_Every_Second() {
  SETREGS
  if (!ready) { return; }
  I2C_SETWIRE(i2c_bus);

  uint32_t r_press = BME_Read(BME280_PRESSURE, 3) >> 4;
  int32_t  r_temp  = BME_Read(BME280_TEMPERATURE, 3) >> 4;

  r_temp  = compensateTemperature(r_temp);
  r_press = compensatePressure(r_press);
  temp    = fscale(r_temp, FLTC(1), FLTC(0));
  press   = fscale(r_press, FLTC(1), FLTC(0));

  if (type == BME280_CHIPID) {
    uint16_t r_hum = BME_Read(BME280_HUMIDITY, 2);
    r_hum  = compensateHumidity(r_hum);
    hum    = fscale(r_hum, FLTC(2), FLTC(0));
    abshum = CalcTempHumToAbsHum(temp, hum);
  }
}

void BME_Show(uint32_t json) {
  SETREGS
  if (!ready) { return; }

  char temp_tstr[16];
  ftostrfd(temp,   Settings->flag2.temperature_resolution, temp_tstr);
  char press_tstr[16];
  ftostrfd(press,  Settings->flag2.pressure_resolution,    press_tstr);
  char hum_tstr[16];
  ftostrfd(hum,    Settings->flag2.humidity_resolution,    hum_tstr);
  char ahum_tstr[16];
  ftostrfd(abshum, 4, ahum_tstr);

  if (json) {
    ResponseAppend_P(GSTR(JSON_BMP), typestr, temp_tstr, press_tstr);
    if (type == BME280_CHIPID) {
      ResponseAppend_P(GSTR(JSON_BME), hum_tstr, ahum_tstr);
    } else {
      ResponseAppend_P(GSTR(JSON_BMPend));
    }
  } else {
    if (type == BME280_CHIPID) {
      TempHumDewShow(json, 0, typestr, temp, hum);
      char s1[32];
      WSContentSend_PD(GSTR(HTTP_SNS_AHUM_BMP), typestr,
                       Plugin_Get_SensorNames(s1, iD_ABSOLUTE_HUMIDITY), ahum_tstr);
    } else {
      char s1[32];
      WSContentSend_PD(GSTR(HTTP_BMP_T), typestr,
                       Plugin_Get_SensorNames(s1, iD_TEMPERATURE), temp_tstr);
    }
    char s1[32];
    WSContentSend_PD(GSTR(HTTP_BMP_P), typestr,
                     Plugin_Get_SensorNames(s1, iD_PRESSURE), press_tstr);
  }
}

void BME_Deinit() {
  SETREGS
  I2C_ResetActive(i2c_addr, i2c_bus);
  RETMEM
}

// Dispatcher
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = Init_BME();     break;
    case pFUNC_JSON_APPEND:  BME_Show(1);             break;
    case pFUNC_WEB_SENSOR:   BME_Show(0);             break;
    case pFUNC_EVERY_SECOND: BME_Every_Second();      break;
    case pFUNC_DEINIT:       BME_Deinit();            break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns09(uint32_t function) {
  if (!I2cEnabled(XI2C_10)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    Init_BME();
  }
  else if (bme_state) {
    switch (function) {
      case FUNC_EVERY_SECOND: BME_Every_Second(); break;
      case FUNC_JSON_APPEND:  BME_Show(1);        break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   BME_Show(0);        break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor macros so they don't leak into other
// dual drivers in the merged tasmota.ino.cpp TU. Particularly critical
// for `type` (rewrites VL53L0X's vcselPeriodType param), `temp`,
// `ready`, `initialized`, etc. — generic names whose pollution broke
// downstream files in earlier multi-driver firmware-link attempts.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef temp
#  undef _t_fine
#  undef hum
#  undef press
#  undef type
#  undef typestr
#  undef abshum
#  undef i2c_addr
#  undef i2c_bus
#  undef bmc
#  undef ready
#endif
#endif  // _BME_DUAL_ENABLED
