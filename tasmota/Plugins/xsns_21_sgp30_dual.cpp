/*
  xsns_21_sgp30_dual.cpp — Sensirion SGP30 / SGPC3 air-quality
  (eCO2 + TVOC) sensor driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2021 Gerhard Mutz

  Plugin: USE_SGP30_DUAL_MOD via build_plugin.py.
  Native: USE_SGP30_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_21_sgp30_dual.ino.

  Single fixed I2C address (0x58). Probe scans both buses.

  When TasmotaGlobal exposes a valid temperature + humidity (from
  another sensor like BMP280/SHT3X) the driver feeds it to the
  SGP30's humidity-compensation register every second, which
  improves the eCO2 / TVOC accuracy noticeably.
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_SGP30_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_SGP30_DUAL_MOD
#    define _SGP30_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_SGP30_DUAL) && defined(SGP30_DUAL_NATIVE_INCLUDE)
#    define _SGP30_DUAL_ENABLED 1
#  endif
#endif

#ifdef _SGP30_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define SGP30_ADDRESS         0x58
#define SGP30_REV             (1 << 16 | 5)
#define SGP30_FEATURESET      0x0020
#define SGP30_CRC8_POLYNOMIAL 0x31
#define SGP30_CRC8_INIT       0xFF

#define SAVE_PERIOD           30   // baseline-readback cadence (seconds)

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
PUSH_OPTIONS
#ifdef USE_SOFTWIRE
#  define DEFAULT_SDA_PIN 12
#  define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("SGP30S", MODULE_TYPE_SENSOR, SGP30_REV,
                  "SDA", DEFAULT_SDA_PIN, "SCL", DEFAULT_SCL_PIN,
                  "", 0, "", 0)
#else
MODULE_DESCRIPTOR("SGP30",  MODULE_TYPE_SENSOR, SGP30_REV,
                  "", 0, "", 0, "", 0, "", 0)
#endif
MODULE_PART int32_t  SGP30_Init(void);
MODULE_PART bool     SGP30_IAQinit(void);
MODULE_PART bool     SGP30_Begin(void);
MODULE_PART bool     SGP30_IAQmeasure(void);
MODULE_PART bool     getIAQBaseline(uint16_t *eco2_base, uint16_t *tvoc_base);
MODULE_PART bool     readWordFromCommand(uint8_t command[], uint8_t commandLength,
                                         uint16_t delayms, uint16_t *readdata,
                                         uint8_t readlen);
MODULE_PART uint8_t  generateCRC(uint8_t *data, uint8_t datalen);
MODULE_PART bool     setHumidity(uint32_t absolute_humidity);
MODULE_PART void     SGP30_Every_Second(void);
MODULE_PART void     SGP30_Show(bool json);
MODULE_PART void     SGP30_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// File-unique PROGMEM strings — `_SGP` suffix prevents collisions
// with other dual drivers in the merged tasmota.ino.cpp.
// --------------------------------------------------------------------
const char HTTP_SNS_SGP30_SGP[] PROGMEM = "{s}SGP30 eCO2 {m}%d ppm {e}{s}SGP30 TVOC {m}%d ppb {e}";
const char JSON_SNS_SGP30_SGP[] PROGMEM = ",\"SGP30\":{\"eCO2\":%d,\"TVOC\":%d";
const char SGP30_NAME_SGP[]     PROGMEM = "SGP30";
const char HTTP_SNS_AHUM_SGP[]  PROGMEM = "{s}SGP30 Abs Humidity{m}%s g/m3{e}";
const char JSON_SNS_AHUM_SGP[]  PROGMEM = ",\"aHumidity\":%s}";

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  sgp30_state_t
#endif

typedef struct {
  TWIp    *xWire;
  bool     sgp30_ready;
  bool     ready;
  uint8_t  bus;
  uint8_t  secs;
  uint16_t eCO2;
  uint16_t TVOC;
  uint16_t TVOC_base;
  uint16_t eCO2_base;
  float    abshum;
  bool     initialized_flag;
} MODULE_MEMORY;

#define ready          mem->ready
#define secs           mem->secs
#define sgp30_ready    mem->sgp30_ready
#define sgp_bus        mem->bus
#define TVOC           mem->TVOC
#define eCO2           mem->eCO2
#define TVOC_base      mem->TVOC_base
#define eCO2_base      mem->eCO2_base
#define abshum         mem->abshum

#if BUILD_AS_PLUGIN

#  ifdef USE_SOFTWIRE
#    include "Softwire/Softwire_cpp.h"
#  endif

#else  // native

static sgp30_state_t *sgp30_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = sgp30_state;
#  define ALLOCMEM \
       if (!sgp30_state) sgp30_state = (sgp30_state_t *)calloc(1, sizeof(sgp30_state_t)); \
       if (!sgp30_state) return -1; \
       MODULE_MEMORY *mem = sgp30_state;
#  define RETMEM \
       if (sgp30_state) { free(sgp30_state); sgp30_state = nullptr; }
#  define initialized    mem->initialized_flag

#  define XSNS_21        21
#  define XI2C_18        18

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------

uint8_t generateCRC(uint8_t *data, uint8_t datalen) {
  uint8_t crc = SGP30_CRC8_INIT;
  for (uint8_t i = 0; i < datalen; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (crc << 1) ^ SGP30_CRC8_POLYNOMIAL : (crc << 1);
    }
  }
  return crc;
}

bool readWordFromCommand(uint8_t command[], uint8_t commandLength,
                         uint16_t delayms, uint16_t *readdata, uint8_t readlen) {
  SETREGS
  I2C_SETWIRE(sgp_bus);

  I2C_beginTransmission(SGP30_ADDRESS);
  for (uint8_t i = 0; i < commandLength; i++) { I2C_write(command[i]); }
  I2C_endTransmission(true);

  delay(delayms);

  if (readlen == 0) { return true; }

  uint8_t replylen = readlen * (2 + 1);
  uint8_t reclen   = I2C_requestFrom(SGP30_ADDRESS, replylen);
  if (reclen != replylen) { return false; }

  uint8_t replybuffer[replylen];
  for (uint8_t i = 0; i < replylen; i++) { replybuffer[i] = I2C_read(); }

  for (uint8_t i = 0; i < readlen; i++) {
    uint8_t crc = generateCRC(replybuffer + i * 3, 2);
    if (crc != replybuffer[i * 3 + 2]) { return false; }
    readdata[i]  = (uint16_t)replybuffer[i * 3] << 8;
    readdata[i] |= replybuffer[i * 3 + 1];
  }
  return true;
}

bool SGP30_IAQinit(void) {
  SETREGS
  uint8_t command[2];
  command[0] = 0x20; command[1] = 0x03;
  return readWordFromCommand(command, 2, 10, 0, 0);
}

bool SGP30_Begin(void) {
  SETREGS
  uint16_t serialnumber[3];
  uint8_t  command[2];

  command[0] = 0x36; command[1] = 0x82;
  if (!readWordFromCommand(command, 2, 10, serialnumber, 3)) { return false; }

  uint16_t featureset;
  command[0] = 0x20; command[1] = 0x2F;
  if (!readWordFromCommand(command, 2, 10, &featureset, 1)) { return false; }
  if ((featureset & 0xF0) != SGP30_FEATURESET) { return false; }

  return SGP30_IAQinit();
}

bool SGP30_IAQmeasure(void) {
  SETREGS
  uint8_t  command[2];
  command[0] = 0x20; command[1] = 0x08;
  uint16_t reply[2];
  if (!readWordFromCommand(command, 2, 12, reply, 2)) { return false; }
  TVOC = reply[1];
  eCO2 = reply[0];
  return true;
}

bool getIAQBaseline(uint16_t *eco2_base, uint16_t *tvoc_base) {
  SETREGS
  uint8_t  command[2];
  command[0] = 0x20; command[1] = 0x15;
  uint16_t reply[2];
  if (!readWordFromCommand(command, 2, 10, reply, 2)) { return false; }
  *eco2_base = reply[0];
  *tvoc_base = reply[1];
  return true;
}

bool setHumidity(uint32_t absolute_humidity) {
  SETREGS
  if (absolute_humidity > ICONST(256000)) { return false; }

  uint64_t llval     = tmod__muldi3(absolute_humidity << 8, ICONST(16777));
  uint16_t ah_scaled = (uint16_t)(llval >> 24);
  uint8_t  command[5];
  command[0] = 0x20;
  command[1] = 0x61;
  command[2] = ah_scaled >> 8;
  command[3] = ah_scaled & 0xFF;
  command[4] = generateCRC(command + 2, 2);
  return readWordFromCommand(command, 5, 10, 0, 0);
}

int32_t SGP30_Init(void) {
  ALLOCMEM

  ready       = false;
  sgp30_ready = false;
  sgp_bus     = 0;

  // Probe both buses.
  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(SGP30_ADDRESS, bus)) { continue; }
    sgp_bus = bus;
    if (SGP30_Begin()) {
      ready       = true;
      initialized = true;
      I2C_SetActiveFound(SGP30_ADDRESS, GSTR(SGP30_NAME_SGP), bus);
      return 0;
    }
    I2C_ResetActive(SGP30_ADDRESS, bus);
  }
  SGP30_Deinit();
  return -1;
}

void SGP30_Every_Second(void) {
  SETREGS
  STGLOB
  if (!ready) { return; }

  sgp30_ready = false;
  if (!SGP30_IAQmeasure()) { return; }

  // Feed the ambient temp+humidity from another sensor into SGP30's
  // humidity-compensation register, if available — significantly
  // improves eCO2/TVOC accuracy.
#if BUILD_AS_PLUGIN
  if (TasmotaGlobal->global_update
      && (fixunssfsi(TasmotaGlobal->humidity) > 0)
      && !isnan(TasmotaGlobal->temperature_celsius)) {
    abshum = CalcTempHumToAbsHum(TasmotaGlobal->temperature_celsius,
                                 TasmotaGlobal->humidity);
    setHumidity(tmod__fixunssfsi(tmod__mulsf3(abshum, 1000)));
  }
#else
  if (TasmotaGlobal.global_update
      && (fixunssfsi(TasmotaGlobal.humidity) > 0)
      && !isnan(TasmotaGlobal.temperature_celsius)) {
    abshum = CalcTempHumToAbsHum(TasmotaGlobal.temperature_celsius,
                                 TasmotaGlobal.humidity);
    setHumidity(tmod__fixunssfsi(tmod__mulsf3(abshum, 1000)));
  }
#endif

  sgp30_ready = true;

  // Periodically read back the IAQ baseline (could be persisted by
  // the caller for fast-restart; this driver doesn't save it itself).
  secs++;
  if (secs >= SAVE_PERIOD) {
    secs = 0;
    getIAQBaseline(&eCO2_base, &TVOC_base);
  }
}

void SGP30_Show(bool json) {
  SETREGS
  STGLOB
  if (!sgp30_ready) { return; }

  char abs_hum[33];
#if BUILD_AS_PLUGIN
  bool ahum_available = TasmotaGlobal->global_update
                     && (fixunssfsi(TasmotaGlobal->humidity) > 0)
                     && !isnan(TasmotaGlobal->temperature_celsius);
#else
  bool ahum_available = TasmotaGlobal.global_update
                     && (fixunssfsi(TasmotaGlobal.humidity) > 0)
                     && !isnan(TasmotaGlobal.temperature_celsius);
#endif
  if (ahum_available) { ftostrfd(abshum, 4, abs_hum); }

  if (json) {
    ResponseAppend_P(GSTR(JSON_SNS_SGP30_SGP), eCO2, TVOC);
    if (ahum_available) {
      ResponseAppend_P(GSTR(JSON_SNS_AHUM_SGP), abs_hum);
    } else {
      ResponseJsonEnd();
    }
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(HTTP_SNS_SGP30_SGP), eCO2, TVOC);
    if (ahum_available) {
      WSContentSend_PD(GSTR(HTTP_SNS_AHUM_SGP), abs_hum);
    }
#endif
  }
}

void SGP30_Deinit(void) {
  SETREGS
  I2C_ResetActive(SGP30_ADDRESS, sgp_bus);
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = SGP30_Init();  break;
    case pFUNC_EVERY_SECOND: SGP30_Every_Second();   break;
    case pFUNC_JSON_APPEND:  SGP30_Show(1);          break;
    case pFUNC_WEB_SENSOR:   SGP30_Show(0);          break;
    case pFUNC_DEINIT:       SGP30_Deinit();         break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns21(uint32_t function) {
  if (!I2cEnabled(XI2C_18)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    SGP30_Init();
  } else if (sgp30_state && ready) {
    switch (function) {
      case FUNC_EVERY_SECOND: SGP30_Every_Second(); break;
      case FUNC_JSON_APPEND:  SGP30_Show(1);        break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   SGP30_Show(0);        break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef ready
#  undef secs
#  undef sgp30_ready
#  undef sgp_bus
#  undef TVOC
#  undef eCO2
#  undef TVOC_base
#  undef eCO2_base
#  undef abshum
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif

#endif  // _SGP30_DUAL_ENABLED
