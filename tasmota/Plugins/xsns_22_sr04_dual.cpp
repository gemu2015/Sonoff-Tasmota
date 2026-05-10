/*
  xsns_22_sr04_dual.cpp — JSN-SR04T-V3 / SR04T ultrasonic distance
  sensor (UART variant) driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2023 Gerhard Mutz

  Plugin: USE_SR04T_DUAL_MOD via build_plugin.py.
  Native: USE_SR04T_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_22_sr04_dual.ino.

  SR04T-V3 in mode 2/3/4 spits out 4-byte serial frames at 9600 baud:
    [0xFF] [DIST_HI] [DIST_LO] [SUM]   sum = (FF + HI + LO) & 0xFF
  Distance is in millimetres; this driver reports cm with 1 decimal.

  This is a non-I2C, RX-only serial sensor — second non-I2C dual
  (after MP3, which is TX-only). Same TasmotaSerial bridge in
  dual_format_compat.h handles it.
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_SR04T_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include <TasmotaSerial.h>
#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_SR04T_DUAL_MOD
#    define _SR04T_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_SR04T_DUAL) && defined(SR04T_DUAL_NATIVE_INCLUDE)
#    define _SR04T_DUAL_ENABLED 1
#  endif
#endif

#ifdef _SR04T_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define SR04TV3_REV       (1 << 16 | 5)
#define SR04T_DEFAULT_RX  3       // GPIO 3 (UART0 RX) — overridable

// --------------------------------------------------------------------
// Module descriptor — plugin only
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN
PUSH_OPTIONS
MODULE_DESCRIPTOR("SR04TV3", MODULE_TYPE_SENSOR, SR04TV3_REV,
                  "RECPIN", SR04T_DEFAULT_RX,
                  "", 0, "", 0, "", 0)
MODULE_PART int32_t Sr04T_Detect(void);
MODULE_PART void    Sr04T_Show(bool json);
MODULE_PART void    Sr04T_Read(void);
MODULE_PART void    Sr04T_Deinit(void);
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END
#else
int32_t Sr04T_Detect(void);
void    Sr04T_Show(bool json);
void    Sr04T_Read(void);
void    Sr04T_Deinit(void);
#endif

const char SR04T_started_DUAL[] PROGMEM = "SR04TV3 initialized: RX pin %d";
const char HTTP_DIST_DUAL[]     PROGMEM = "{s}SR04T distance{m}%s cm{e}";
const char JSON_DIST_DUAL[]     PROGMEM = ",\"SR04T\":{\"DIST\":%s}";

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

typedef struct {
  uint8_t  recpin;
  uint8_t  ready;
  float    distance;
  void    *ts;          // TasmotaSerial *
  uint8_t  sbuff[4];    // 4-byte sliding window
} MODULE_MEMORY;

#  define sr04_ts        mem->ts
#  define sr04_recpin    mem->recpin
#  define sr04_ready     mem->ready
#  define sr04_distance  mem->distance
#  define sr04_sbuff     mem->sbuff

#else  // native

typedef struct {
  uint8_t  recpin;
  uint8_t  ready;
  float    distance;
  void    *ts;
  uint8_t  sbuff[4];
  bool     initialized_flag;
} sr04_state_t;

static sr04_state_t *sr04_state = nullptr;

#  define sr04_ts        sr04_state->ts
#  define sr04_recpin    sr04_state->recpin
#  define sr04_ready     sr04_state->ready
#  define sr04_distance  sr04_state->distance
#  define sr04_sbuff     sr04_state->sbuff
#  define initialized    sr04_state->initialized_flag

#  define ALLOCMEM       DUAL_ALLOCMEM(sr04)
#  define RETMEM         DUAL_RETMEM(sr04)

#  define XSNS_22        22

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------
int32_t Sr04T_Detect(void) {
  ALLOCMEM
  sr04_ready = false;

#if BUILD_AS_PLUGIN
  // Plugin: RX pin from BinPlugin params.
  sr04_recpin = (uint8_t)(mp->ms[0].value & 0xff);
#else
  // Native: prefer Tasmota template GPIO_SR04_ECHO if mapped,
  // fall back to the build-time default.
#  ifdef GPIO_SR04_ECHO
  int8_t gp  = Pin(GPIO_SR04_ECHO);
  sr04_recpin = (gp >= 0) ? (uint8_t)gp : (uint8_t)SR04T_DEFAULT_RX;
#  else
  sr04_recpin = SR04T_DEFAULT_RX;
#  endif
#endif

  // RX-only TasmotaSerial — TX pin = -1 means we don't drive a TX pin.
  sr04_ts = NewTS(sr04_recpin, -1);
  if (sr04_ts && beginTS(sr04_ts, ICONST(9600))) {
    AddLog(LOG_LEVEL_INFO, GSTR(SR04T_started_DUAL), (int)sr04_recpin);
    initialized = true;
    sr04_ready  = true;
    return 0;
  }
  Sr04T_Deinit();
  return -1;
}

void Sr04T_Read(void) {
  SETREGS
  if (!sr04_ready) { return; }

  int16_t wval = 0;
  // Slide each byte into a 4-byte window, then validate frame
  // (header byte 0xFF + checksum). Last valid frame in this tick wins.
  while (availTS(sr04_ts)) {
    for (uint16_t cnt = 0; cnt < 3; cnt++) {
      sr04_sbuff[cnt] = sr04_sbuff[cnt + 1];
    }
    sr04_sbuff[3] = (uint8_t)readbTS(sr04_ts);

    if (sr04_sbuff[0] == 0xff) {
      uint8_t sum = sr04_sbuff[0] + sr04_sbuff[1] + sr04_sbuff[2];
      if (sum == sr04_sbuff[3]) {
        wval = ((int16_t)sr04_sbuff[1] << 8) | sr04_sbuff[2];
      }
    }
  }
  // Convert mm → cm (×0.1).
  sr04_distance = fscale(wval, (float)0.1, (float)0);
}

void Sr04T_Show(bool json) {
  SETREGS
  if (!sr04_ready) { return; }

  char tstr[16];
  ftostrfd(sr04_distance, 1, tstr);
  if (json) {
    ResponseAppend_P(GSTR(JSON_DIST_DUAL), tstr);
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(HTTP_DIST_DUAL), tstr);
#endif
  }
}

void Sr04T_Deinit(void) {
  SETREGS
  if (sr04_ts) {
    deleteTS(sr04_ts);
    sr04_ts = nullptr;
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
    case pFUNC_INIT:         result = Sr04T_Detect(); break;
    case pFUNC_EVERY_SECOND: Sr04T_Read(); result = true; break;
    case pFUNC_JSON_APPEND:  Sr04T_Show(1); break;
    case pFUNC_WEB_SENSOR:   Sr04T_Show(0); break;
    case pFUNC_DEINIT:       Sr04T_Deinit(); break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns22(uint32_t function) {
  bool result = false;
  if (FUNC_INIT == function) {
    Sr04T_Detect();
  } else if (sr04_state && sr04_ready) {
    switch (function) {
      case FUNC_EVERY_SECOND: Sr04T_Read(); break;
      case FUNC_JSON_APPEND:  Sr04T_Show(1); break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   Sr04T_Show(0); break;
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
#  undef sr04_ts
#  undef sr04_recpin
#  undef sr04_ready
#  undef sr04_distance
#  undef sr04_sbuff
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif

#endif  // _SR04T_DUAL_ENABLED
