/*
  xsns_31_ccs811_dual.cpp — CCS811 air-quality (eCO2 + TVOC) driver,
  dual-format. Compat shim in dual_format_compat.h.

  Plugin: USE_CCS811_DUAL_MOD via build_plugin.py.
  Native: USE_CCS811_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_31_ccs811_dual.ino.

  Original copyright preserved.
  Copyright (C) 2021 Gerhard Mutz and Theo Arends
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_CCS811_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_CCS811_DUAL_MOD
#    define _CCS811_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_CCS811_DUAL) && defined(CCS811_DUAL_NATIVE_INCLUDE)
#    define _CCS811_DUAL_ENABLED 1
#  endif
#endif

#ifdef _CCS811_DUAL_ENABLED

#define EVERYNSECONDS    5
#define CCS811_ADDRESS   0x5A

#define D_UNIT_PARTS_PER_MILLION "ppm"
#define D_UNIT_PARTS_PER_BILLION "ppb"
#define D_ECO2 "eCO₂"
#define D_TVOC "TVOC"
#define D_JSON_ECO2 "eCO2"
#define D_JSON_TVOC "TVOC"

const char HTTP_SNS_CCS811[] PROGMEM =
  "{s}CCS811 " D_ECO2 "{m}%d " D_UNIT_PARTS_PER_MILLION "{e}"
  "{s}CCS811 " D_TVOC "{m}%d " D_UNIT_PARTS_PER_BILLION "{e}";
const char JSON_SNS_CCS811[] PROGMEM =
  ",\"CCS811\":{\"" D_JSON_ECO2 "\":%d,\"" D_JSON_TVOC "\":%d}";
const char CCS811_dev[] PROGMEM = "CCS811";

typedef union {
  uint8_t data;
  struct {
    uint8_t ERROR: 1;
    uint8_t reserved1 : 2;
    uint8_t DATA_READY: 1;
    uint8_t APP_VALID: 1;
    uint8_t reserved2 : 2;
    uint8_t FW_MODE: 1;
  };
} CSS811_STAT;

typedef union {
  uint8_t data;
  struct {
    uint8_t reserved1 : 2;
    uint8_t THRESH: 1;
    // Xtensa specreg.h #defines INTERRUPT 226 — would mangle the
    // bitfield decl below. Local #undef is safe since the plugin
    // never references the Xtensa register macro.
    #undef INTERRUPT
    uint8_t INTERRUPT: 1;
    uint8_t DRIVE_MODE: 3;
    uint8_t reserved2 : 1;
  };
} CSS811_MEAS;

typedef struct {
  uint8_t  i2c_addr;
  uint16_t _TVOC;
  uint16_t _eCO2;
  CSS811_STAT stat;
  CSS811_MEAS meas;
} CCS811;

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
#define CCS811_REV (1 << 16 | 4)
PUSH_OPTIONS
MODULE_DESCRIPTOR("CCS811", MODULE_TYPE_SENSOR, CCS811_REV,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART bool       CCS811_Detect(void);
MODULE_PART void       CCS811_Update(void);
MODULE_PART void       CCS811_Show(bool json);
MODULE_PART void       CCS811_Deinit();
#if BUILD_AS_PLUGIN
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);
#endif
MODULE_END

// State storage — heap in both modes
#if BUILD_AS_PLUGIN

typedef struct {
  TWIp     *xWire;
  uint8_t   CCS811_ready;
  uint16_t  eCO2;
  uint16_t  TVOC;
  uint8_t   tcnt;
  uint8_t   ecnt;
  uint8_t   i2c_addr;        // 0x5A or 0x5B
  uint8_t   i2c_bus;
  bool      ready;
  CCS811    ccs;
} MODULE_MEMORY;

#  define CCS811_ready    mem->CCS811_ready
#  define eCO2            mem->eCO2
#  define TVOC            mem->TVOC
#  define tcnt            mem->tcnt
#  define ecnt            mem->ecnt
#  define ready           mem->ready
#  define ccs             mem->ccs
#  define ccs_addr        mem->i2c_addr
#  define ccs_bus         mem->i2c_bus

#else  // native

typedef struct {
  uint8_t   CCS811_ready;
  uint16_t  eCO2;
  uint16_t  TVOC;
  uint8_t   tcnt;
  uint8_t   ecnt;
  uint8_t   i2c_addr;
  uint8_t   i2c_bus;
  bool      ready;
  CCS811    ccs;
  bool      initialized_flag;
} ccs811_state_t;

static ccs811_state_t *ccs811_state = nullptr;

#  define CCS811_ready    ccs811_state->CCS811_ready
#  define eCO2            ccs811_state->eCO2
#  define TVOC            ccs811_state->TVOC
#  define tcnt            ccs811_state->tcnt
#  define ecnt            ccs811_state->ecnt
#  define ready           ccs811_state->ready
#  define ccs             ccs811_state->ccs
#  define ccs_addr        ccs811_state->i2c_addr
#  define ccs_bus         ccs811_state->i2c_bus
#  define initialized     ccs811_state->initialized_flag

#  define ALLOCMEM        DUAL_ALLOCMEM(ccs811)
#  define RETMEM          DUAL_RETMEM(ccs811)

#  define XSNS_31         31
#  define XI2C_24         24

#endif  // BUILD_AS_PLUGIN

// Chip driver header — uses the field accessors above (ccs.*) so it
// must come AFTER the #defines that bind them.
#include "CCS811.h"

bool CCS811_Detect(void) {
  ALLOCMEM

  ready = false;
  tcnt = 0;
  ecnt = 0;
  CCS811_ready = 0;
  ccs_bus  = 0;
  ccs_addr = CCS811_ADDRESS;

  // Probe both buses × both possible CCS811 addresses (0x5A, 0x5B).
  static const uint8_t kCcsAddrs[] = { 0x5A, 0x5B };
  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    for (uint32_t a = 0; a < (sizeof(kCcsAddrs)/sizeof(kCcsAddrs[0])); a++) {
      uint8_t addr = kCcsAddrs[a];
      if (!I2C_SetDevice(addr, bus)) { continue; }
      if (!CCS811_begin(addr)) {
        ccs_addr = addr;
        ccs_bus  = bus;
        I2C_SetActiveFound(addr, GSTR(CCS811_dev), bus);
        ready = true;
        initialized = true;
        return true;
      }
      I2C_ResetActive(addr, bus);
    }
  }
  CCS811_Deinit();
  return false;
}

void CCS811_Update(void) {
  SETREGS
  STGLOB

  if (!ready) { return; }
  I2C_SETWIRE(ccs_bus);
  tcnt++;
  if (tcnt >= EVERYNSECONDS) {
    tcnt = 0;
    CCS811_ready = 0;
    if (CCS811_available()) {
      if (!CCS811_readData()) {
        TVOC = CCS811_getTVOC();
        eCO2 = CCS811_geteCO2();
        CCS811_ready = 1;
#if BUILD_AS_PLUGIN
        if (TasmotaGlobal->global_update
            && (fixunssfsi(TasmotaGlobal->humidity) > 0)
            && !isnan(TasmotaGlobal->temperature_celsius)) {
          uint16_t hum = fixunssfsi(TasmotaGlobal->humidity);
          float    temp = TasmotaGlobal->temperature_celsius;
          CCS811_setEnvironmentalData(hum, temp);
        }
#else
        if (TasmotaGlobal.global_update
            && (fixunssfsi(TasmotaGlobal.humidity) > 0)
            && !isnan(TasmotaGlobal.temperature_celsius)) {
          uint16_t hum = fixunssfsi(TasmotaGlobal.humidity);
          float    temp = TasmotaGlobal.temperature_celsius;
          CCS811_setEnvironmentalData(hum, temp);
        }
#endif
        ecnt = 0;
      }
    } else {
      ecnt++;
      if (ecnt > 6) {
        CCS811_begin(ccs_addr);
      }
    }
  }
}

void CCS811_Show(bool json) {
  SETREGS

  if (!ready) { return; }
  if (CCS811_ready) {
    if (json) {
      ResponseAppend_P(GSTR(JSON_SNS_CCS811), eCO2, TVOC);
    } else {
      WSContentSend_PD(GSTR(HTTP_SNS_CCS811), eCO2, TVOC);
    }
  }
}

void CCS811_Deinit(void) {
  SETREGS
  I2C_ResetActive(ccs_addr, ccs_bus);
  RETMEM
}

// Dispatcher
#if BUILD_AS_PLUGIN

MOD_RESULT mod_func_execute(uint32_t sel) {
  MOD_RESULT result = false;
  switch (sel) {
    case pFUNC_INIT:         result = CCS811_Detect(); break;
    case pFUNC_EVERY_SECOND: CCS811_Update();          break;
    case pFUNC_JSON_APPEND:  CCS811_Show(1);           break;
    case pFUNC_WEB_SENSOR:   CCS811_Show(0);           break;
    case pFUNC_DEINIT:       CCS811_Deinit();          break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns31(uint32_t function) {
  if (!I2cEnabled(XI2C_24)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    CCS811_Detect();
  }
  else if (ccs811_state) {
    switch (function) {
      case FUNC_EVERY_SECOND: CCS811_Update(); break;
      case FUNC_JSON_APPEND:  CCS811_Show(1);  break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   CCS811_Show(0);  break;
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
#  undef CCS811_ready
#  undef eCO2
#  undef TVOC
#  undef tcnt
#  undef ecnt
#  undef ready
#  undef ccs
#  undef ccs_addr
#  undef ccs_bus
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif

#endif  // _CCS811_DUAL_ENABLED
