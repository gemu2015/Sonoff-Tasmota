/*
  xsns_51_rdm6300_dual.cpp — Seeed Studio Grove / RDM630 / RDM6300
  125 kHz RFID tag reader driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2023 Gerhard Mutz

  Plugin: USE_RDM6300_DUAL_MOD via build_plugin.py.
  Native: USE_RDM6300_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_51_rdm6300_dual.ino.

  Wire format (14 bytes at 9600 baud, RX-only):

    0  1  2  3  4  5  6  7  8  9 10 11 12 13
    Hd ----------- ASCII chars ----------- Cs Tl
    02 31 34 30 30 38 45 43 37 39 33 43 45 03   = 02-14-008EC793-CE-03
       Vers --------- Tag --------- Chksm

    Tag is 8 hex chars (32-bit UID); checksum is XOR of the four
    preceding hex bytes treated as a hex pair string.

  When a tag is recognized the driver publishes
    {"RDM6300":{"UID":"<hex8>"}}
  via MqttPublishTeleSensor and blocks further reads for 2 seconds
  to avoid duplicate events from the same card swipe.
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_RDM6300_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include <TasmotaSerial.h>
#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_RDM6300_DUAL_MOD
#    define _RDM6300_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_RDM6300_DUAL) && defined(RDM6300_DUAL_NATIVE_INCLUDE)
#    define _RDM6300_DUAL_ENABLED 1
#  endif
#endif

#ifdef _RDM6300_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define RDM6300_DEFAULT_REC_PIN 3
#define RDM6300_BAUDRATE        9600
#define RDM_TIMEOUT             100   // ms — give up if frame doesn't complete
#define RDM6300_BLOCK           20    // 100ms ticks → 2 seconds debounce

#define RDM6300_REV (1 << 16 | 5)

// --------------------------------------------------------------------
// Plugin descriptor block (canonical layout — see dual_format_compat.h
// for why this works in native mode without an `#if` gate).
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("RDM6300", MODULE_TYPE_SENSOR, RDM6300_REV,
                  "RXD", RDM6300_DEFAULT_REC_PIN,
                  "", 0, "", 0, "", 0)
MODULE_PART uint8_t  RDM6300_HexNibble(char chr);
MODULE_PART void     RDM6300_HexStringToArray(uint8_t array[], uint8_t len, char buffer[]);
MODULE_PART int32_t  RDM6300_Init(void);
MODULE_PART void     RDM6300_Deinit(void);
MODULE_PART void     RDM6300_ScanForTag(void);
MODULE_PART void     RDM6300_Show(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t sel);
#endif
MODULE_END

const char RDM_started_DUAL[] PROGMEM = "RDM6300 initialized: REC pin %d";
const char RDM_HHTP_UID_DUAL[] PROGMEM = "{s}RDM6300 UID{m}%08X {e}";
const char RDM_JSON_UID_DUAL[] PROGMEM = ",\"RDM6300\":{\"UID\":\"%08X\"}}";

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  rdm6300_state_t
#endif

typedef struct {
  uint8_t        recpin;
  uint8_t        ready;
  uint32_t       uid;
  uint8_t        block_time;
  TasmotaSerial *ts;
  bool           initialized_flag;
} MODULE_MEMORY;

#define rdm_ts          mem->ts
#define rdm_recpin      mem->recpin
#define rdm_ready       mem->ready
#define rdm_uid         mem->uid
#define rdm_block_time  mem->block_time

#if !BUILD_AS_PLUGIN

static rdm6300_state_t *rdm6300_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = rdm6300_state;
#  define ALLOCMEM \
       if (!rdm6300_state) rdm6300_state = (rdm6300_state_t *)calloc(1, sizeof(rdm6300_state_t)); \
       if (!rdm6300_state) return -1; \
       MODULE_MEMORY *mem = rdm6300_state;
#  define RETMEM \
       if (rdm6300_state) { free(rdm6300_state); rdm6300_state = nullptr; }
#  define initialized     mem->initialized_flag

#  define XSNS_51         51

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------
int32_t RDM6300_Init(void) {
  ALLOCMEM
  rdm_ready = false;

#if BUILD_AS_PLUGIN
  rdm_recpin = (uint8_t)(mp->ms[0].value & 0xff);
#else
#  ifdef GPIO_RDM6300_RX
  int8_t gp = Pin(GPIO_RDM6300_RX);
  rdm_recpin = (gp >= 0) ? (uint8_t)gp : (uint8_t)RDM6300_DEFAULT_REC_PIN;
#  else
  rdm_recpin = RDM6300_DEFAULT_REC_PIN;
#  endif
#endif

  // RX-only TasmotaSerial — TX pin = -1.
  rdm_ts = (TasmotaSerial *)NewTS(rdm_recpin, -1);
  if (rdm_ts && beginTS(rdm_ts, ICONST(RDM6300_BAUDRATE))) {
    if (hardwareSerial(rdm_ts)) {
      ClaimSerial();
    }
    AddLog(LOG_LEVEL_INFO, GSTR(RDM_started_DUAL), (int)rdm_recpin);
    initialized = true;
    rdm_ready   = true;
    return 0;
  }
  RDM6300_Deinit();
  return -1;
}

uint8_t RDM6300_HexNibble(char chr) {
  if (isdigit(chr)) {
    return chr - '0';
  } else if (chr >= 'A' && chr <= 'F') {
    return chr + 10 - 'A';
  } else if (chr >= 'a' && chr <= 'f') {
    return chr + 10 - 'a';
  }
  return 0;
}

void RDM6300_HexStringToArray(uint8_t array[], uint8_t len, char buffer[]) {
  char *cp = buffer;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = RDM6300_HexNibble(*cp++) << 4;
    array[i] = val | RDM6300_HexNibble(*cp++);
  }
}

void RDM6300_ScanForTag(void) {
  SETREGS
  if (!rdm_ready) { return; }

  // Debounce window after a successful read — drain whatever's in the
  // RX FIFO and bail.
  if (rdm_block_time > 0) {
    rdm_block_time--;
    while (availTS(rdm_ts)) { (void)readbTS(rdm_ts); }
    return;
  }

  if (!availTS(rdm_ts)) { return; }

  char c = (char)readbTS(rdm_ts);
  if (c != 2) { return; }   // not the head marker — discard

  // Read the rest of the 14-byte frame, time-bounded.
  char    rdm_buffer[14];
  uint8_t rdm_index = 0;
  rdm_buffer[rdm_index++] = c;

  uint32_t cmillis = millis();
  while (1) {
    if (availTS(rdm_ts)) {
      c = (char)readbTS(rdm_ts);
      rdm_buffer[rdm_index++] = c;
      if (3 == c)         { break; }  // tail marker
      if (rdm_index > 14) { break; }  // overflow guard
    }
    if ((millis() - cmillis) > RDM_TIMEOUT) { return; }
  }

  AddLogBuffer(LOG_LEVEL_DEBUG, (uint8_t *)rdm_buffer, sizeof(rdm_buffer));

  if (rdm_buffer[13] != 3) { return; }   // tail marker missing → bad frame

  // Block further reads for 2 s — typical RFID swipe re-emits the
  // same UID several times in a row.
  rdm_block_time = RDM6300_BLOCK;

  uint8_t rdm_array[6];
  RDM6300_HexStringToArray(rdm_array, sizeof(rdm_array), (char *)rdm_buffer + 1);
  uint8_t accu = 0;
  for (uint32_t count = 0; count < 5; count++) { accu ^= rdm_array[count]; }
  if (accu != rdm_array[5]) { return; }    // checksum mismatch

  rdm_buffer[11] = '\0';
  uint32_t tuid = strtoul(rdm_buffer + 3, nullptr, 16);
  if (tuid > 0) {                          // ignore the all-zeros false positive
    rdm_uid = tuid;
    ResponseTime_P(GSTR(RDM_JSON_UID_DUAL), rdm_uid);
    MqttPublishTeleSensor();
  }
}

void RDM6300_Show(void) {
  SETREGS
  if (!rdm_ready) { return; }
#ifdef USE_WEBSERVER
  WSContentSend_PD(GSTR(RDM_HHTP_UID_DUAL), rdm_uid);
#endif
}

void RDM6300_Deinit(void) {
  SETREGS
  if (rdm_ts) {
    deleteTS(rdm_ts);
    rdm_ts = nullptr;
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
    case pFUNC_INIT:              RDM6300_Init();        break;
    case pFUNC_EVERY_100_MSECOND: RDM6300_ScanForTag();  break;
    case pFUNC_WEB_SENSOR:        RDM6300_Show();        break;
    case pFUNC_DEINIT:            RDM6300_Deinit();      break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns51(uint32_t function) {
  bool result = false;
  if (FUNC_INIT == function) {
    RDM6300_Init();
  } else if (rdm6300_state && rdm_ready) {
    switch (function) {
      case FUNC_EVERY_100_MSECOND: RDM6300_ScanForTag(); break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:        RDM6300_Show();       break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor and helper macros so they don't
// leak into other dual drivers in the merged tasmota.ino.cpp TU.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef rdm_ts
#  undef rdm_recpin
#  undef rdm_ready
#  undef rdm_uid
#  undef rdm_block_time
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif

#endif  // _RDM6300_DUAL_ENABLED
