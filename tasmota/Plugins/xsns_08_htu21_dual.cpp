/*
  xsns_08_htu21_dual.cpp — HTU21 / SI70xx temperature + humidity driver,
  dual-format. Compiles to either the BinPlugin or a native xsns_08
  driver depending on BUILD_AS_PLUGIN. Compat shim is in
  dual_format_compat.h.

  Plugin: USE_HTU_DUAL_MOD via build_plugin.py.
  Native: USE_HTU_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_08_htu21_dual.ino.

  Original copyright preserved from xsns_08_htu21.{ino,cpp}.
  Copyright (C) 2021 Heiko Krupp and Theo Arends
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_HTU_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_HTU_DUAL_MOD
#    define _HTU_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_HTU_DUAL) && defined(HTU_DUAL_NATIVE_INCLUDE)
#    define _HTU_DUAL_ENABLED 1
#  endif
#endif

#ifdef _HTU_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define HTU21_ADDR             0x40
#define SI7013_CHIPID          0x0D
#define SI7020_CHIPID          0x14
#define SI7021_CHIPID          0x15
#define HTU21_CHIPID           0x32
#define HTU21_READTEMP         0xE3
#define HTU21_READHUM          0xE5
#define HTU21_WRITEREG         0xE6
#define HTU21_READREG          0xE7
#define HTU21_RESET            0xFE
#define HTU21_HEATER_WRITE     0x51
#define HTU21_HEATER_READ      0x11
#define HTU21_SERIAL2_READ1    0xFC
#define HTU21_SERIAL2_READ2    0xC9
#define HTU21_HEATER_ON        0x04
#define HTU21_HEATER_OFF       0xFB
#define HTU21_RES_RH12_T14     0x00
#define HTU21_RES_RH8_T12      0x01
#define HTU21_RES_RH10_T13     0x80
#define HTU21_RES_RH11_T11     0x81
#define HTU21_CRC8_POLYNOM     0x13100

const char kHtuTypes[] PROGMEM = "HTU21|SI7013|SI7020|SI7021|T/RH?";

typedef struct {
  float   temperature;
  float   humidity;
  uint8_t address;
  uint8_t type;
  uint8_t jdelay_temp;
  uint8_t jdelay_humidity;
  uint8_t valid;
  uint8_t cnt;
  char    types[7];
} HTU;

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
#define HTU_REV (1 << 16 | 5)
PUSH_OPTIONS
MODULE_DESCRIPTOR("HTU21", MODULE_TYPE_SENSOR, HTU_REV,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART int32_t HTU_Detect();
MODULE_PART void    HTU_Show(bool json);
MODULE_PART void    HTU_Deinit();
MODULE_PART uint8_t HtuCheckCrc8(uint16_t data);
MODULE_PART uint8_t HtuReadDeviceId();
MODULE_PART void    HtuSetResolution(uint8_t resolution);
MODULE_PART void    HtuReset();
MODULE_PART void    HtuHeater(uint8_t heater);
MODULE_PART void    HTU_Init();
MODULE_PART bool    HTU_Read();
MODULE_PART void    HTU_EverySecond();
#if BUILD_AS_PLUGIN
MODULE_PART int32_t mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  htu_state_t
#endif

typedef struct {
  TWIp   *xWire;
  HTU     Htu;
  uint8_t i2c_bus;
} MODULE_MEMORY;

#define Htu               mem->Htu
#define htu_bus           mem->i2c_bus

#if !BUILD_AS_PLUGIN

static htu_state_t *htu_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = htu_state;
#  define ALLOCMEM \
       if (!htu_state) htu_state = (htu_state_t *)calloc(1, sizeof(htu_state_t)); \
       if (!htu_state) return -1; \
       MODULE_MEMORY *mem = htu_state;
#  define RETMEM \
       if (htu_state) { free(htu_state); htu_state = nullptr; }

#  define XSNS_08           8
#  define XI2C_09           9

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core — shared
// --------------------------------------------------------------------

uint8_t HtuCheckCrc8(uint16_t data) {
  for (uint32_t bit = 0; bit < 16; bit++) {
    if (data & 0x8000) { data = (data << 1) ^ HTU21_CRC8_POLYNOM; }
    else               { data <<= 1; }
  }
  return data >>= 8;
}

uint8_t HtuReadDeviceId() {
  SETREGS
  HtuReset();
  uint16_t deviceID = 0;
  uint8_t  checksum = 0;

  I2C_beginTransmission(HTU21_ADDR);
  I2C_write(HTU21_SERIAL2_READ1);
  I2C_write(HTU21_SERIAL2_READ2);
  I2C_endTransmission(0);

  I2C_requestFrom(HTU21_ADDR, 3);
  deviceID  = I2C_read() << 8;
  deviceID |= I2C_read();
  checksum  = I2C_read();
  if (HtuCheckCrc8(deviceID) == checksum) { deviceID = deviceID >> 8; }
  else                                    { deviceID = 0; }
  return (uint8_t)deviceID;
}

void HtuSetResolution(uint8_t resolution) {
  SETREGS
  uint8_t current = I2C_Read8(HTU21_ADDR, HTU21_READREG);
  current &= 0x7E;
  current |= resolution;
  I2C_write8(HTU21_ADDR, HTU21_WRITEREG, current);
}

void HtuReset() {
  SETREGS
  I2C_beginTransmission(HTU21_ADDR);
  I2C_write(HTU21_RESET);
  I2C_endTransmission(0);
  delay(15);
}

void HtuHeater(uint8_t heater) {
  SETREGS
  uint8_t current = I2C_Read8(HTU21_ADDR, HTU21_READREG);
  switch (heater) {
    case HTU21_HEATER_ON:  current |= heater; break;
    case HTU21_HEATER_OFF: current &= heater; break;
    default:               current &= heater; break;
  }
  I2C_write8(HTU21_ADDR, HTU21_WRITEREG, current);
}

void HTU_Init() {
  SETREGS
  HtuReset();
  HtuHeater(HTU21_HEATER_OFF);
  HtuSetResolution(HTU21_RES_RH12_T14);
}

bool HTU_Read() {
  SETREGS
  uint8_t  checksum  = 0;
  uint16_t sensorval = 0;

  if (Htu.valid) { Htu.valid--; }

  I2C_beginTransmission(HTU21_ADDR);
  I2C_write(HTU21_READTEMP);
  if (I2C_endTransmission(0) != 0) { return false; }
  delay(Htu.jdelay_temp);

  I2C_requestFrom(HTU21_ADDR, 3);
  if (3 == I2C_available()) {
    sensorval  = I2C_read() << 8;
    sensorval |= I2C_read();
    checksum   = I2C_read();
  }
  if (HtuCheckCrc8(sensorval) != checksum) { return false; }

  Htu.temperature = ConvertTemp(jfscale(sensorval, 0.002681, 46.85));

  I2C_beginTransmission(HTU21_ADDR);
  I2C_write(HTU21_READHUM);
  if (I2C_endTransmission(0) != 0) { return false; }
  delay(Htu.jdelay_humidity);

  I2C_requestFrom(HTU21_ADDR, 3);
  if (3 <= I2C_available()) {
    sensorval  = I2C_read() << 8;
    sensorval |= I2C_read();
    checksum   = I2C_read();
  }
  if (HtuCheckCrc8(sensorval) != checksum) { return false; }

  sensorval ^= 0x02;
  Htu.humidity = jfdiff(jfmul(0.001907, jtofloat(sensorval)), 6);

  if (jgtsf2(Htu.humidity, 100)) { Htu.humidity = 100.0; }
  if (jltsf2(Htu.humidity, 0))   { Htu.humidity = 0.01; }
  if ((jeqsf2(0.00, Htu.humidity)) && (jeqsf2(0.00, Htu.temperature))) {
    Htu.humidity = 0.0;
  }
  if ((jgtsf2(Htu.temperature, 0)) && (jltsf2(Htu.temperature, 80))) {
    Htu.humidity = jfadd(jfmul(-0.15, jfdiff(25, Htu.temperature)), Htu.humidity);
  }
  Htu.humidity = ConvertHumidity(Htu.humidity);

  Htu.valid = SENSOR_MAX_MISS;
  return true;
}

int32_t HTU_Detect() {
  ALLOCMEM

  Htu.jdelay_humidity = 6;
  Htu.address = HTU21_ADDR;
  htu_bus = 0;

  // Probe both I2C buses (ESP32) until the device responds.
  bool found = false;
  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(Htu.address, bus)) { continue; }
    Htu.type = HtuReadDeviceId();
    if (Htu.type) { htu_bus = bus; found = true; break; }
    // Sensor address scanned-positive but ID read failed — release
    // the active marker so the next bus iteration can claim it.
    I2C_ResetActive(Htu.address, bus);
  }
  if (!found) {
    HTU_Deinit();
    return -1;
  }

  uint8_t index = 0;
  HTU_Init();
  switch (Htu.type) {
    case HTU21_CHIPID:
      Htu.jdelay_temp = 50;
      Htu.jdelay_humidity = 16;
      break;
    case SI7021_CHIPID: index++;  // 3
    case SI7020_CHIPID: index++;  // 2
    case SI7013_CHIPID: index++;  // 1
      Htu.jdelay_temp = 12;
      Htu.jdelay_humidity = 23;
      break;
    default:
      index = 4;
      Htu.jdelay_temp = 50;
      Htu.jdelay_humidity = 23;
  }
  GetTextIndexed(Htu.types, sizeof(Htu.types), index, GSTR(kHtuTypes));
  I2cSetActiveFound(Htu.address, Htu.types, htu_bus);
  initialized = true;
  return 0;
}

void HTU_EverySecond() {
  SETREGS
  Htu.cnt++;
  if (Htu.cnt & 1) {
    if (!HTU_Read()) {
      AddLogMissed(Htu.types, Htu.valid);
    }
  }
}

void HTU_Show(bool json) {
  SETREGS
  STGLOB
  if (Htu.valid) {
#if BUILD_AS_PLUGIN
    TempHumDewShow(json, (0 == TasmotaGlobal->tele_period),
                   Htu.types, Htu.temperature, Htu.humidity);
#else
    TempHumDewShow(json, (0 == TasmotaGlobal.tele_period),
                   Htu.types, Htu.temperature, Htu.humidity);
#endif
  }
}

void HTU_Deinit() {
  SETREGS
  I2C_ResetActive(Htu.address, htu_bus);
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher — single divergence point
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = HTU_Detect(); break;
    case pFUNC_EVERY_SECOND: HTU_EverySecond();      break;
    case pFUNC_JSON_APPEND:  HTU_Show(1);            break;
    case pFUNC_WEB_SENSOR:   HTU_Show(0);            break;
    case pFUNC_DEINIT:       HTU_Deinit();           break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns08(uint32_t function) {
  if (!I2cEnabled(XI2C_09)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    HTU_Detect();
  }
  else if (htu_state) {
    switch (function) {
      case FUNC_EVERY_SECOND: HTU_EverySecond(); break;
      case FUNC_JSON_APPEND:  HTU_Show(1);       break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   HTU_Show(0);       break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor macros so they don't leak.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef Htu
#  undef htu_bus
#endif

#endif  // _HTU_DUAL_ENABLED
