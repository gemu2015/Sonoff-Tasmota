/*
  xsns_45_vl53l0x_dual.cpp — VL53L0X time-of-flight distance driver,
  dual-format. Compat shim in dual_format_compat.h.

  Plugin: USE_VL53L0X_DUAL_MOD via build_plugin.py.
  Native: USE_VL53L0X_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_45_vl53l0x_dual.ino.

  Original copyright preserved.
  Copyright (C) 2021 Theo Arends, Gerhard Mutz, Adrian Scillato
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_VL53L0X_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  include "../Tasmota/include/i18n.h"
#endif

#if BUILD_AS_PLUGIN
#  ifdef USE_VL53L0X_DUAL_MOD
#    define _VL53L0X_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_VL53L0X_DUAL) && defined(VL53L0X_DUAL_NATIVE_INCLUDE)
#    define _VL53L0X_DUAL_ENABLED 1
#  endif
#endif

#ifdef _VL53L0X_DUAL_ENABLED

#define USE_VL_MEDIAN
#define USE_VL_MEDIAN_SIZE   5
#define VL53L0X_ADDRESS      0x29
#ifndef VL53L0X_XSHUT_ADDRESS
#  define VL53L0X_XSHUT_ADDRESS 0x78
#endif

#include "VL53L0X.h"     // chip driver header (uses I2C_* via compat)

typedef struct {
  uint16_t distance;
  uint16_t buffer[USE_VL_MEDIAN_SIZE];
  uint8_t  ready;
  uint8_t  index;
} VLX_DATA;

// File-unique name — avoid collision with FP_CONST in other duals.
const float FP_CONST_VL53[] PROGMEM = {10};
#undef  FLTC
#if BUILD_AS_PLUGIN
// ESP32-S3 lsi-from-PROGMEM trap: see xsns_09_bmp_dual.cpp note.
#  define FLTC(idx) ({ \
      volatile uint32_t _tmp = ((const volatile uint32_t*)((char*)FP_CONST_VL53 + EXEC_OFFSET))[(idx)]; \
      float _f; \
      __builtin_memcpy(&_f, (void*)&_tmp, 4); \
      _f; \
    })
#else
#  define FLTC(idx)                             (FP_CONST_VL53[(idx)])
#endif

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
#define VL53L0_REV (1 << 16 | 5)
PUSH_OPTIONS
#ifdef USE_SOFTWIRE
#  define DEFAULT_SDA_PIN 41
#  define DEFAULT_SCL_PIN 40
MODULE_DESCRIPTOR("VL53L0", MODULE_TYPE_SENSOR, VL53L0_REV,
                  "SDA", DEFAULT_SDA_PIN, "SCL", DEFAULT_SCL_PIN, "", 0, "", 0)
#else
MODULE_DESCRIPTOR("VL53L0", MODULE_TYPE_SENSOR, VL53L0_REV,
                  "RMODE", 0x01000300, "I2CBUS", 0x01000200, "", 0, "", 0)
#endif
MODULE_PART int32_t VL53L0X_Detect();
MODULE_PART void    VL53L0X_Every_250MSecond(void);
MODULE_PART void    VL53L0X_Show(boolean json);
MODULE_PART void    VL53L0X_Deinit();
#if BUILD_AS_PLUGIN
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);
#endif
MODULE_END

// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  vl53l0x_state_t
#endif

typedef struct {
  TWIp    *xWire;
  bool     VL53L0X_xshut;
  uint8_t  range_mode;
  uint8_t  i2c_bus;
  bool     VL53L0X_detected;
  VLX_DATA Vl53l0x_data;
  VLX_MEM  vlx_mem;
  bool     initialized_flag;
} MODULE_MEMORY;

#define VL53L0X_xshut             mem->VL53L0X_xshut
#define address                   mem->vlx_mem.address
#define VL53L0X_detected          mem->VL53L0X_detected
#define Vl53l0x_data              mem->Vl53l0x_data
#define io_timeout                mem->vlx_mem.io_timeout
#define did_timeout               mem->vlx_mem.did_timeout
#define timeout_start_ms          mem->vlx_mem.timeout_start_ms
#define stop_variable             mem->vlx_mem.stop_variable
#define measurement_timing_budget_us mem->vlx_mem.measurement_timing_budget_us
#define last_status               mem->vlx_mem.last_status
#define range_mode                mem->range_mode
#define i2c_bus                   mem->i2c_bus

#if BUILD_AS_PLUGIN

#  ifdef USE_SOFTWIRE
#    include "Softwire/Softwire_cpp.h"
#  endif

#else  // native

static vl53l0x_state_t *vl53l0x_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = vl53l0x_state;
#  define ALLOCMEM \
       if (!vl53l0x_state) vl53l0x_state = (vl53l0x_state_t *)calloc(1, sizeof(vl53l0x_state_t)); \
       if (!vl53l0x_state) return -1; \
       MODULE_MEMORY *mem = vl53l0x_state;
#  define RETMEM \
       if (vl53l0x_state) { free(vl53l0x_state); vl53l0x_state = nullptr; }
#  define initialized               mem->initialized_flag

#  define XSNS_45                   45
#  define XI2C_31                   31

#endif  // !BUILD_AS_PLUGIN

// Library impl — uses the field accessors above.
#include "VL53L0X_c.h"

int32_t VL53L0X_Detect(void) {
  ALLOCMEM
  VL53L0X_detected = false;

  // Bus selection policy:
  //   - Plugin mode: if user-set I2CBUS param is non-zero, force that
  //     bus; if 0, auto-probe both buses.
  //   - Native mode: always auto-probe.
  uint8_t bus_lo = 0;
  uint8_t bus_hi = MAX_I2C_Busses;  // exclusive upper

#if BUILD_AS_PLUGIN
  range_mode = mp->ms[0].value & 0xff;
  uint8_t user_bus = mp->ms[1].value & 0xff;
  if (user_bus == 1 || user_bus == 2) {
    // 1 = bus 0, 2 = bus 1 (1-based for user-friendliness in the UI;
    // 0 = "auto" sentinel). Adjust by -1 to get the actual bus index.
    bus_lo = user_bus - 1;
    bus_hi = user_bus;
  }
#else
  range_mode = 0;
#endif

  // Probe at the default address (0x29). First responder wins.
  // Drivers that need a non-default address (XSHUT-remapped) should
  // still configure that explicitly.
  bool found = false;
  for (uint32_t bus = bus_lo; bus < bus_hi; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(VL53L0X_ADDRESS, bus)) { continue; }
    if (VL53L0X_init(0)) {
      i2c_bus = bus;
      found = true;
      I2C_SetActiveFound(VL53L0X_ADDRESS, PSTR("VL53L0X"), bus);
      AddLog(LOG_LEVEL_INFO,
             PSTR(D_LOG_I2C D_SENSOR " VL53L0X bus %d " D_SENSOR_DETECTED
                  " - " D_NEW_ADDRESS " 0x%02X"),
             (int)bus, VL53L0X_ADDRESS);
      break;
    }
    I2C_ResetActive(VL53L0X_ADDRESS, bus);
  }
  if (!found) {
    VL53L0X_Deinit();
    return false;
  }

  VL53L0X_setTimeout(500);

  if (range_mode == 1) {
    // Long range
    VL53L0X_setSignalRateLimit(0.1);
    VL53L0X_setVcselPulsePeriod(VcselPeriodPreRange,   18);
    VL53L0X_setVcselPulsePeriod(VcselPeriodFinalRange, 14);
  }
  if (range_mode == 2) {
    VL53L0X_setMeasurementTimingBudget(ICONST(20000));   // high speed
  }
  if (range_mode == 3) {
    VL53L0X_setMeasurementTimingBudget(ICONST(200000));  // high accuracy
  }
  VL53L0X_startContinuous(0);

  Vl53l0x_data.ready = 1;
  Vl53l0x_data.index = 0;
  VL53L0X_detected   = true;
  initialized        = true;
  return VL53L0X_detected;
}

void VL53L0X_Every_250MSecond(void) {
  SETREGS
  if (!VL53L0X_detected) { return; }
  I2C_SETWIRE(i2c_bus);

  uint16_t dist = VL53L0X_readRangeContinuousMillimeters();
  if ((0 == dist) || (dist > ICONST(2200))) { dist = ICONST(9999); }

#ifdef USE_VL_MEDIAN
  Vl53l0x_data.buffer[Vl53l0x_data.index] = dist;
  Vl53l0x_data.index++;
  if (Vl53l0x_data.index >= USE_VL_MEDIAN_SIZE) { Vl53l0x_data.index = 0; }

  uint16_t tbuff[USE_VL_MEDIAN_SIZE];
  memmove(tbuff, Vl53l0x_data.buffer, sizeof(tbuff));
  uint16_t tmp;
  uint8_t  flag;
  for (uint32_t ocnt = 0; ocnt < USE_VL_MEDIAN_SIZE; ocnt++) {
    flag = 0;
    for (uint32_t cnt = 0; cnt < USE_VL_MEDIAN_SIZE - 1; cnt++) {
      if (tbuff[cnt] > tbuff[cnt + 1]) {
        tmp = tbuff[cnt]; tbuff[cnt] = tbuff[cnt + 1]; tbuff[cnt + 1] = tmp;
        flag = 1;
      }
    }
    if (!flag) { break; }
  }
  Vl53l0x_data.distance = tbuff[(USE_VL_MEDIAN_SIZE - 1) / 2];
#else
  Vl53l0x_data.distance = dist;
#endif
}

void VL53L0X_Show(boolean json) {
  SETREGS
  if (!VL53L0X_detected) { return; }

  float distance = fdiv(tofloat(Vl53l0x_data.distance), FLTC(0));

  if (json) {
    ResponseAppend_P(PSTR(",\"VL53L0X\":{\"Distance\":%1_f}"), &distance);
  } else {
    char s1[32];
    WSContentSend_PD(PSTR("{s}VL53L0X %s{m}%1_f cm{e}"),
                     Plugin_Get_SensorNames(s1, iD_DISTANCE), &distance);
  }

  if (VL53L0X_timeoutOccurred()) {
    AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_I2C "Timeout waiting for VL53L0X"));
  }
}

void VL53L0X_Deinit() {
  SETREGS
  I2C_ResetActive(VL53L0X_ADDRESS, i2c_bus);
  RETMEM
}

// Dispatcher
#if BUILD_AS_PLUGIN

MOD_RESULT mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:             result = VL53L0X_Detect();   break;
    case pFUNC_EVERY_250_MSECOND: VL53L0X_Every_250MSecond(); break;
    case pFUNC_JSON_APPEND:      VL53L0X_Show(1);             break;
    case pFUNC_WEB_SENSOR:       VL53L0X_Show(0);             break;
    case pFUNC_DEINIT:           VL53L0X_Deinit();            break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns45(uint32_t function) {
  if (!I2cEnabled(XI2C_31)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    VL53L0X_Detect();
  }
  else if (vl53l0x_state) {
    switch (function) {
      case FUNC_EVERY_250_MSECOND: VL53L0X_Every_250MSecond(); break;
      case FUNC_JSON_APPEND:       VL53L0X_Show(1);            break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:        VL53L0X_Show(0);            break;
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
#  undef VL53L0X_xshut
#  undef address
#  undef VL53L0X_detected
#  undef Vl53l0x_data
#  undef io_timeout
#  undef did_timeout
#  undef timeout_start_ms
#  undef stop_variable
#  undef measurement_timing_budget_us
#  undef last_status
#  undef range_mode
#  undef i2c_bus
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef FLTC

#endif  // _VL53L0X_DUAL_ENABLED
