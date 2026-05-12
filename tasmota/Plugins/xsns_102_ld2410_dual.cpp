/*
  xsns_102_ld2410_dual.cpp — HLK-LD2410 24 GHz mmWave motion sensor
  driver, dual-format. Compat shim in dual_format_compat.h.

  Plugin: USE_LD2410_DUAL_MOD via build_plugin.py.
  Native: USE_LD2410_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_102_ld2410_dual.ino.

  Original copyright preserved.
  SPDX-FileCopyrightText: 2022 Theo Arends, 2024 md5sum-as
  SPDX-License-Identifier: GPL-3.0-only

  Wire protocol notes (truncated — see Ld1410HandleTargetData below):
    target frame:  F4 F3 F2 F1 ... F8 F7 F6 F5
    config frame:  FD FC FB FA ... 04 03 02 01

  Console commands:
    LD2410Duration <s>              — set no-one duration (or 0 = factory reset)
    LD2410MovingSens v0,v1,...,v8   — moving-sensitivity per gate
    LD2410StaticSens v0,v1,...,v8   — static-sensitivity per gate
    LD2410Get                       — last-engineering-mode dump
    LD2410EngineeringStart / End
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_LD2410_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include <TasmotaSerial.h>
#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_LD2410_DUAL_MOD
#    define _LD2410_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_LD2410_DUAL) && defined(LD2410_DUAL_NATIVE_INCLUDE)
#    define _LD2410_DUAL_ENABLED 1
#  endif
#endif

#ifdef _LD2410_DUAL_ENABLED

#define LD2410_BUFFER_SIZE               64
#define LD2410_MAX_GATES                 8       // 0..8 inclusive — 9 gates

#define LD2410_CMND_START_CONFIGURATION  0xFF
#define LD2410_CMND_END_CONFIGURATION    0xFE
#define LD2410_CMND_SET_DISTANCE         0x60
#define LD2410_CMND_READ_PARAMETERS      0x61
#define LD2410_CMND_START_ENGINEERING    0x62
#define LD2410_CMND_END_ENGINEERING      0x63
#define LD2410_CMND_SET_SENSITIVITY      0x64
#define LD2410_CMND_GET_FIRMWARE         0xA0
#define LD2410_CMND_SET_BAUDRATE         0xA1
#define LD2410_CMND_FACTORY_RESET        0xA2
#define LD2410_CMND_REBOOT               0xA3
#define LD2410_CMND_SET_BLUETOOTH        0xA4
#define LD2410_CMND_GET_BLUETOOTH_MAC    0xA5

#define LD2410_DEFAULT_RX_PIN            5
#define LD2410_DEFAULT_TX_PIN            6
#define LD2410_BAUDRATE                  256000

typedef struct {
  uint8_t buffer[LD2410_BUFFER_SIZE];
  uint16_t moving_distance;
  uint16_t static_distance;
  uint16_t detect_distance;
  uint16_t no_one_duration;
  uint8_t moving_sensitivity[LD2410_MAX_GATES + 1];
  uint8_t static_sensitivity[LD2410_MAX_GATES + 1];
  uint8_t max_moving_distance_gate;
  uint8_t max_static_distance_gate;

  uint8_t config_header[4];
  uint8_t config_footer[4];
  uint8_t target_header[4];
  uint8_t target_footer[4];

  uint8_t moving_energy;
  uint8_t static_energy;
  uint8_t step;
  uint8_t retry;
  uint8_t settings;
  uint8_t byte_counter;
  bool    valid_response;
  uint8_t set_engin_mode;
  uint8_t web_engin_mode;
  TasmotaSerial *ts;
  struct {
    uint8_t moving_gate_energy[LD2410_MAX_GATES + 1];
    uint8_t static_gate_energy[LD2410_MAX_GATES + 1];
    uint8_t light;
    uint8_t out_pin;
  } engineering;
} LD2410_MEM;

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
#define LD2410_REV (1 << 16 | 5)
PUSH_OPTIONS
MODULE_DESCRIPTOR("LD2410", MODULE_TYPE_SENSOR, LD2410_REV,
                  "RXD", LD2410_DEFAULT_RX_PIN,
                  "TXD", LD2410_DEFAULT_TX_PIN,
                  "", 0, "", 0)
MODULE_PART uint32_t ToBcd(uint32_t value);
MODULE_PART void     Ld1410HandleTargetData(void);
MODULE_PART void     Ld1410HandleConfigData(void);
MODULE_PART bool     Ld2410Match(const uint8_t *header, uint32_t offset);
MODULE_PART void     Ld2410Input(void);
MODULE_PART void     Ld2410SendCommand(uint32_t command, uint8_t *val = nullptr, uint32_t val_len = 0);
MODULE_PART void     Ld2410SetConfigMode(void);
MODULE_PART void     Ld2410SetMaxDistancesAndNoneDuration(uint32_t max_moving_distance_range,
                                                          uint32_t max_static_distance_range,
                                                          uint32_t no_one_duration);
MODULE_PART void     Ld2410SetGateSensitivity(uint32_t gate, uint32_t moving_sensitivity, uint32_t static_sensitivity);
MODULE_PART void     Ld2410SetAllSensitivity(uint32_t sensitivity);
MODULE_PART void     Ld2410SetBaudrate(uint32_t index);
MODULE_PART void     Ld2410Every100MSecond(void);
MODULE_PART void     Ld2410EverySecond(void);
MODULE_PART int32_t  Ld2410Detect(void);
MODULE_PART uint8_t *preset_params(uint32_t cnt);
MODULE_PART void     Ld2410Response(void);
MODULE_PART void     CmndLd2410Duration(void);
MODULE_PART void     CmndLd2410MovingSensitivity(void);
MODULE_PART void     CmndLd2410StaticSensitivity(void);
MODULE_PART void     CmndLd2410last(void);
MODULE_PART void     CmndLd2410EngineeringEnd(void);
MODULE_PART void     CmndLd2410EngineeringStart(void);
MODULE_PART void     Ld2410Show(bool json);
MODULE_PART void     LD2410_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t function);
#endif
MODULE_END

// File-unique PROGMEM strings (LD2410-prefixed names already unique;
// no need for additional renaming).
const char LD2410_started[] PROGMEM     = "LD2410 inizialized with REC pin %d - TRX pin %d";
const char kLd2410Commands[] PROGMEM    = "LD2410|"
  "Duration|MovingSens|StaticSens|Get|EngineeringEnd|EngineeringStart";

#define xD_MOVING_DISTANCE   "Moving"
#define xD_STATIC_DISTANCE   "Static"
#define xD_DETECT_DISTANCE   "Detected"
#define xD_MOVING_ENERGY_T   "Moving target"
#define xD_STATIC_ENERGY_T   "Static target"
#define xD_LD2410_PIN_STATE  "Out port state"
#define xD_LD2410_LIGHT      "Light sensor"

const char HTTP_SNS_LD2410_CM[] PROGMEM =
  "{s}LD2410 %s - " xD_MOVING_DISTANCE "{m}%1_f " D_UNIT_CENTIMETER "{e}"
  "{s}LD2410 %s - " xD_STATIC_DISTANCE "{m}%1_f " D_UNIT_CENTIMETER "{e}"
  "{s}LD2410 %s - " xD_DETECT_DISTANCE "{m}%1_f " D_UNIT_CENTIMETER "{e}";
const char HTTP_SNS_LD2410_ENG[] PROGMEM =
  "{s}LD2410 " xD_MOVING_ENERGY_T "{m}%d %d %d %d %d %d %d %d %d{e}"
  "{s}LD2410 " xD_STATIC_ENERGY_T "{m}%d %d %d %d %d %d %d %d %d{e}"
  "{s}LD2410 " xD_LD2410_LIGHT "{m}%d{e}"
  "{s}LD2410 " xD_LD2410_PIN_STATE "{m}%d{e}";

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#define DUAL_NATIVE_NAME    ld2410
#define DUAL_NATIVE_STATE_T ld2410_state_t
#include "dual_format_native_state.h"
typedef struct {
  uint8_t    rxd_pin;
  uint8_t    txd_pin;
  uint8_t    ready;
  LD2410_MEM LD2410;
  bool       initialized_flag;
} MODULE_MEMORY;

#define rxd_pin     mem->rxd_pin
#define txd_pin     mem->txd_pin
#define ready       mem->ready
#define LD2410      mem->LD2410

#if !BUILD_AS_PLUGIN

DUAL_NATIVE_STATE_PTR_DECL
#  define XSNS_102    102

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------
uint32_t ToBcd(uint32_t value) {
  return ((value >> 4) * 10) + (value & 0xF);
}

void Ld1410HandleTargetData(void) {
  SETREGS

  uint8_t i;

  if (((0x0D == LD2410.buffer[4]) && (0x55 == LD2410.buffer[17]) && (0x02 == LD2410.buffer[6]))
      or ((0x23 == LD2410.buffer[4]) && (0x55 == LD2410.buffer[39]) && (0x01 == LD2410.buffer[6]))) {

    LD2410.moving_distance = 0;
    LD2410.moving_energy   = 0;
    LD2410.static_distance = 0;
    LD2410.static_energy   = 0;
    LD2410.detect_distance = 0;

    if (LD2410.buffer[8] != 0x00) {
      LD2410.moving_distance = LD2410.buffer[10] << 8 | LD2410.buffer[9];
      LD2410.moving_energy   = LD2410.buffer[11];
      LD2410.static_distance = LD2410.buffer[13] << 8 | LD2410.buffer[12];
      LD2410.static_energy   = LD2410.buffer[14];
      LD2410.detect_distance = LD2410.buffer[16] << 8 | LD2410.buffer[15];
    }
    LD2410.web_engin_mode = LD2410.buffer[6] == 1 ? 1 : 0;
    if (0x01 == LD2410.buffer[6]) {  // Engineering mode
      if (LD2410.buffer[17] < 9) {
        for (i = 0; i <= LD2410.buffer[17]; i++) {
          LD2410.engineering.moving_gate_energy[i] = LD2410.buffer[i + 19];
        }
      }
      if (LD2410.buffer[18] < 9) {
        for (i = 0; i <= LD2410.buffer[18]; i++) {
          LD2410.engineering.static_gate_energy[i] = LD2410.buffer[i + 28];
        }
      }
      LD2410.engineering.light   = LD2410.buffer[37];
      LD2410.engineering.out_pin = LD2410.buffer[38];
    }
  }
}

void Ld1410HandleConfigData(void) {
  SETREGS

  if (LD2410_CMND_READ_PARAMETERS == LD2410.buffer[6]) {           // 0x61
    LD2410.max_moving_distance_gate = LD2410.buffer[12];
    LD2410.max_static_distance_gate = LD2410.buffer[13];
    for (uint32_t i = 0; i <= LD2410_MAX_GATES; i++) {
      LD2410.moving_sensitivity[i] = LD2410.buffer[14 + i];
      LD2410.static_sensitivity[i] = LD2410.buffer[23 + i];
    }
    LD2410.no_one_duration = LD2410.buffer[33] << 8 | LD2410.buffer[32];
  }
  else if (LD2410_CMND_START_CONFIGURATION == LD2410.buffer[6]) {  // 0xFF
    LD2410.valid_response = true;
  }
  else if (LD2410_CMND_GET_FIRMWARE == LD2410.buffer[6]) {         // 0xA0
    AddLog(LOG_LEVEL_INFO, PSTR("LD2: Firmware version V%d.%02d.%02d%02d%02d%02d"),
      ToBcd(LD2410.buffer[13]), ToBcd(LD2410.buffer[12]),
      ToBcd(LD2410.buffer[17]), ToBcd(LD2410.buffer[16]),
      ToBcd(LD2410.buffer[15]), ToBcd(LD2410.buffer[14]));
  }
}

bool Ld2410Match(const uint8_t *header, uint32_t offset) {
  SETREGS
  for (uint32_t i = 0; i < 4; i++) {
    if (LD2410.buffer[offset + i] != header[i]) { return false; }
  }
  return true;
}

void Ld2410Input(void) {
  SETREGS

  while (availTS(LD2410.ts)) {
    yield();                                                    // Fix watchdogs

    LD2410.buffer[LD2410.byte_counter++] = readbTS(LD2410.ts);
    if (LD2410.byte_counter < 4) { continue; }                  // Need first four header bytes

    uint32_t header_start = LD2410.byte_counter - 4;            // Fix interrupted header transmits
    bool target_header = (Ld2410Match(LD2410.target_header, header_start));  // F4F3F2F1
    bool config_header = (Ld2410Match(LD2410.config_header, header_start));  // FDFCFBFA
    if ((target_header || config_header) && (header_start != 0)) {
      memmove(LD2410.buffer, LD2410.buffer + header_start, 4);  // Sync buffer with header
      LD2410.byte_counter = 4;
    }
    if (LD2410.byte_counter < 6) { continue; }                  // Need packet size bytes

    target_header = (Ld2410Match(LD2410.target_header, 0));     // F4F3F2F1
    config_header = (Ld2410Match(LD2410.config_header, 0));     // FDFCFBFA
    if (target_header || config_header) {
      uint32_t len = LD2410.buffer[4] + 10;                     // Total packet size
      if (len > LD2410_BUFFER_SIZE) {
        LD2410.byte_counter = 0;                                // Invalid data
        break;                                                  // Exit loop to satisfy yields
      }
      if (LD2410.byte_counter < len) { continue; }              // Need complete packet

      AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("LD2: Rcvd %*_H"), len, LD2410.buffer);

      if (target_header) {
        if (Ld2410Match(LD2410.target_footer, len - 4)) {       // F8F7F6F5
          Ld1410HandleTargetData();
        }
      }
      else if (config_header) {
        if (Ld2410Match(LD2410.config_footer, len - 4)) {       // 04030201
          Ld1410HandleConfigData();
        }
      }
    }
    LD2410.byte_counter = 0;                                    // Finished or bad received footer
    break;                                                      // Exit loop to satisfy yields
  }
}

void Ld2410SendCommand(uint32_t command, uint8_t *val, uint32_t val_len) {
  SETREGS
  uint32_t len = val_len + 12;
  uint8_t buffer[len];
  buffer[0] = 0xFD;
  buffer[1] = 0xFC;
  buffer[2] = 0xFB;
  buffer[3] = 0xFA;
  buffer[4] = val_len + 2;
  buffer[5] = 0x00;
  buffer[6] = command;
  buffer[7] = 0x00;
  if (val) {
    for (uint32_t i = 0; i < val_len; i++) {
      buffer[8 + i] = val[i];
    }
  }
  buffer[8 + val_len]  = 0x04;
  buffer[9 + val_len]  = 0x03;
  buffer[10 + val_len] = 0x02;
  buffer[11 + val_len] = 0x01;

  AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("LD2: Send %*_H"), len, buffer);

  flushTS(LD2410.ts);
  writeTS(LD2410.ts, buffer, len);
}

void Ld2410SetConfigMode(void) {
  uint8_t value[2];
  value[0] = 0x01; value[1] = 0x00;
  Ld2410SendCommand(LD2410_CMND_START_CONFIGURATION, value, sizeof(value));
}

uint8_t *preset_params(uint32_t cnt) {
  SETREGS
  uint8_t *params = (uint8_t *)calloc(cnt, 1);
  params[6]  = 0x01;
  params[12] = 0x02;
  return params;
}

void Ld2410SetMaxDistancesAndNoneDuration(uint32_t max_moving_distance_range,
                                          uint32_t max_static_distance_range,
                                          uint32_t no_one_duration) {
  SETREGS
  uint8_t lsb_nd = no_one_duration & 0xFF;
  uint8_t msb_nd = (no_one_duration >> 8) & 0xFF;

  uint8_t *value = preset_params(18);
  value[2]  = max_moving_distance_range;
  value[8]  = max_static_distance_range;
  value[14] = lsb_nd;
  value[15] = msb_nd;

  Ld2410SendCommand(LD2410_CMND_SET_DISTANCE, value, sizeof(value));
  free(value);
}

void Ld2410SetGateSensitivity(uint32_t gate, uint32_t moving_sensitivity, uint32_t static_sensitivity) {
  SETREGS
  uint8_t *value = preset_params(18);
  value[2]  = gate;
  value[8]  = moving_sensitivity;
  value[14] = static_sensitivity;
  Ld2410SendCommand(LD2410_CMND_SET_SENSITIVITY, value, sizeof(value));
  free(value);
}

void Ld2410SetAllSensitivity(uint32_t sensitivity) {
  SETREGS
  uint8_t *value = preset_params(18);
  value[2]  = 0xff;
  value[3]  = 0xff;
  value[8]  = sensitivity;
  value[14] = sensitivity;
  Ld2410SendCommand(LD2410_CMND_SET_SENSITIVITY, value, sizeof(value));
  free(value);
}

void Ld2410SetBaudrate(uint32_t index) {
  uint8_t value[2];
  value[0] = (uint8_t)index; value[1] = 0x00;
  Ld2410SendCommand(LD2410_CMND_SET_BAUDRATE, value, sizeof(value));
}

void Ld2410Every100MSecond(void) {
  SETREGS
  if (LD2410.step) {
    LD2410.step--;
    switch (LD2410.step) {
      // case 60: Set default settings
      case 59: Ld2410SetConfigMode(); break;
      case 57: Ld2410SendCommand(LD2410_CMND_FACTORY_RESET); break;
      case 56: Ld2410SendCommand(LD2410_CMND_REBOOT); break;
      case 51:
        LD2410.step = 12;
        AddLog(LOG_LEVEL_DEBUG, PSTR("LD2: Settings factory reset"));
        break;

      // case 40: Save settings
      case 39: Ld2410SetConfigMode(); break;
      case 37: Ld2410SetMaxDistancesAndNoneDuration(8, 8, LD2410.no_one_duration); break;
      case 28 ... 36: {
          uint32_t index = LD2410.step - 28;
          Ld2410SetGateSensitivity(index, LD2410.moving_sensitivity[index], LD2410.static_sensitivity[index]);
        }
        break;
      case 27:
        LD2410.step = 3;
        AddLog(LOG_LEVEL_DEBUG, PSTR("LD2: Settings saved"));
        break;

      case 17: Ld2410SetConfigMode(); break;
      case 14:
        if (0 == LD2410.set_engin_mode) {
          Ld2410SendCommand(LD2410_CMND_END_ENGINEERING);
        } else {
          Ld2410SendCommand(LD2410_CMND_START_ENGINEERING);
        }
        LD2410.step = 2;
        break;

      // case 12: Init
      case 5: Ld2410SetConfigMode(); break;
      case 3:
        if (!LD2410.valid_response && LD2410.retry) {
          LD2410.retry--;
          if (LD2410.retry) {
            LD2410.step = 7;                                    // Retry
          } else {
            LD2410.step = 0;
            AddLog(LOG_LEVEL_DEBUG, PSTR("LD2: Not detected"));
          }
        } else {
          Ld2410SendCommand(LD2410_CMND_GET_FIRMWARE);
        }
        break;
      case 2: Ld2410SendCommand(LD2410_CMND_READ_PARAMETERS); break;
      case 1: Ld2410SendCommand(LD2410_CMND_END_CONFIGURATION); break;
    }
  } else {
    if (1 == LD2410.settings) {
      LD2410.settings = 0;
      LD2410.step = 40;
    }
    else if (2 == LD2410.settings) {
      LD2410.settings = 0;
      LD2410.step = 60;
    }
  }
}

void Ld2410EverySecond(void) {
  SETREGS
  if (LD2410.moving_energy) {
    MqttPublishSensor();
  }
}

int32_t Ld2410Detect(void) {
  ALLOCMEM

#if BUILD_AS_PLUGIN
  rxd_pin = (uint8_t)(mp->ms[0].value & 0xff);
  txd_pin = (uint8_t)(mp->ms[1].value & 0xff);
#else
#  if defined(GPIO_LD2410_RX) && defined(GPIO_LD2410_TX)
  if (PinUsed(GPIO_LD2410_RX) && PinUsed(GPIO_LD2410_TX)) {
    rxd_pin = (uint8_t)Pin(GPIO_LD2410_RX);
    txd_pin = (uint8_t)Pin(GPIO_LD2410_TX);
  } else {
    LD2410_Deinit();
    return -1;
  }
#  else
  rxd_pin = LD2410_DEFAULT_RX_PIN;
  txd_pin = LD2410_DEFAULT_TX_PIN;
#  endif
#endif

  ready = false;

  LD2410.ts = (TasmotaSerial *)NewTS(rxd_pin, txd_pin);

  if (LD2410.ts) {
    if (beginTS(LD2410.ts, ICONST(LD2410_BAUDRATE))) {
      if (hardwareSerial(LD2410.ts)) {
        ClaimSerial();
      }
      AddLog(LOG_LEVEL_INFO, GSTR(LD2410_started), rxd_pin, txd_pin);

      LD2410.retry          = 4;
      LD2410.step           = 12;
      LD2410.set_engin_mode = 0;

      uint8_t c_h = 0xfd;
      uint8_t c_f = 0x04;
      uint8_t t_h = 0xf4;
      uint8_t t_f = 0xf8;
      for (uint32_t cnt = 0; cnt < 4; cnt++) {
        LD2410.config_header[cnt] = c_h--;
        LD2410.config_footer[cnt] = c_f--;
        LD2410.target_header[cnt] = t_h--;
        LD2410.target_footer[cnt] = t_f--;
      }

      ready       = true;
      initialized = true;
      return 0;
    }
  }
  LD2410_Deinit();
  return -1;
}

// --------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------
void (* const Ld2410Command[])(void) PROGMEM = {
  &CmndLd2410Duration, &CmndLd2410MovingSensitivity, &CmndLd2410StaticSensitivity,
  &CmndLd2410last,     &CmndLd2410EngineeringEnd,    &CmndLd2410EngineeringStart };

void Ld2410Response(void) {
  SETREGS
  Response_P(PSTR("{\"LD2410\":{\"Duration\":%d,\"Moving\":{\"Gates\":%d,\"Sensitivity\":["),
    LD2410.no_one_duration, LD2410.max_moving_distance_gate);
  for (uint32_t i = 0; i <= LD2410_MAX_GATES; i++) {
    ResponseAppend_P(PSTR("%s%d"), (i == 0) ? "" : ",", LD2410.moving_sensitivity[i]);
  }
  ResponseAppend_P(PSTR("]},\"Static\":{\"Gates\":%d,\"Sensitivity\":["), LD2410.max_static_distance_gate);
  for (uint32_t i = 0; i <= LD2410_MAX_GATES; i++) {
    ResponseAppend_P(PSTR("%s%d"), (i == 0) ? "" : ",", LD2410.static_sensitivity[i]);
  }
  ResponseAppend_P(PSTR("]}}}"));
}

void CmndLd2410Duration(void) {
  SETREGS
  if (0 == XdrvMailbox->payload) {
    LD2410.settings = 2;
  }
  else if ((XdrvMailbox->payload > 0) && (XdrvMailbox->payload <= 65535)) {
    LD2410.no_one_duration = XdrvMailbox->payload;
    LD2410.settings = 1;
  }
  Ld2410Response();
}

void CmndLd2410MovingSensitivity(void) {
  SETREGS
  uint32_t *parm = (uint32_t *)calloc(LD2410_MAX_GATES + 1, 4);
  uint32_t count = ParseParameters(LD2410_MAX_GATES + 1, parm);
  if (count) {
    for (uint32_t i = 0; i < count; i++) {
      if ((parm[i] >= 0) && (parm[i] <= 100)) {
        LD2410.moving_sensitivity[i] = parm[i];
      }
    }
    LD2410.settings = 1;
  }
  Ld2410Response();
  free(parm);
}

void CmndLd2410StaticSensitivity(void) {
  SETREGS
  uint32_t *parm = (uint32_t *)calloc(LD2410_MAX_GATES + 1, 4);
  uint32_t count = ParseParameters(LD2410_MAX_GATES + 1, parm);
  if (count) {
    for (uint32_t i = 0; i < count; i++) {
      if ((parm[i] >= 0) && (parm[i] <= 100)) {
        LD2410.static_sensitivity[i] = parm[i];
      }
    }
    LD2410.settings = 1;
  }
  Ld2410Response();
  free(parm);
}

void CmndLd2410last(void) {
  SETREGS
  Response_P(PSTR("{\"LD2410\":{\"Moving energy\":[%d,%d,%d,%d,%d,%d,%d,%d,%d],\"Static energy\":[%d,%d,%d,%d,%d,%d,%d,%d,%d],\"Light\":%d,\"Out_pin\":%d}}"),
          LD2410.engineering.moving_gate_energy[0], LD2410.engineering.moving_gate_energy[1], LD2410.engineering.moving_gate_energy[2],
          LD2410.engineering.moving_gate_energy[3], LD2410.engineering.moving_gate_energy[4], LD2410.engineering.moving_gate_energy[5],
          LD2410.engineering.moving_gate_energy[6], LD2410.engineering.moving_gate_energy[7], LD2410.engineering.moving_gate_energy[8],
          LD2410.engineering.static_gate_energy[0], LD2410.engineering.static_gate_energy[1], LD2410.engineering.static_gate_energy[2],
          LD2410.engineering.static_gate_energy[3], LD2410.engineering.static_gate_energy[4], LD2410.engineering.static_gate_energy[5],
          LD2410.engineering.static_gate_energy[6], LD2410.engineering.static_gate_energy[7], LD2410.engineering.static_gate_energy[8],
          LD2410.engineering.light, LD2410.engineering.out_pin);
}

void CmndLd2410EngineeringEnd(void) {
  SETREGS
  LD2410.set_engin_mode = 0;
  LD2410.step = 18;
  Response_P(PSTR("LD2410: End engineering mode"));
}

void CmndLd2410EngineeringStart(void) {
  SETREGS
  LD2410.set_engin_mode = 1;
  LD2410.step = 18;
  Response_P(PSTR("LD2410: Start engineering mode"));
}

void Ld2410Show(bool json) {
  SETREGS
  float moving_distance = floatunsisf(LD2410.moving_distance);
  float static_distance = floatunsisf(LD2410.static_distance);
  float detect_distance = floatunsisf(LD2410.detect_distance);
  if (json) {
    ResponseAppend_P(PSTR(",\"LD2410\":{\"" D_JSON_DISTANCE "\":[%1_f,%1_f,%1_f],\"" D_JSON_ENERGY "\":[%d,%d]}"),
      &moving_distance, &static_distance, &detect_distance, LD2410.moving_energy, LD2410.static_energy);
  } else {
#ifdef USE_WEBSERVER
    char s1[32];
    Plugin_Get_SensorNames(s1, iD_DISTANCE);
    WSContentSend_PD(GSTR(HTTP_SNS_LD2410_CM), s1, &moving_distance, s1, &static_distance, s1, &detect_distance);
    if (LD2410.web_engin_mode == 1) {
      WSContentSend_PD(GSTR(HTTP_SNS_LD2410_ENG),
          LD2410.engineering.moving_gate_energy[0], LD2410.engineering.moving_gate_energy[1], LD2410.engineering.moving_gate_energy[2],
          LD2410.engineering.moving_gate_energy[3], LD2410.engineering.moving_gate_energy[4], LD2410.engineering.moving_gate_energy[5],
          LD2410.engineering.moving_gate_energy[6], LD2410.engineering.moving_gate_energy[7], LD2410.engineering.moving_gate_energy[8],
          LD2410.engineering.static_gate_energy[0], LD2410.engineering.static_gate_energy[1], LD2410.engineering.static_gate_energy[2],
          LD2410.engineering.static_gate_energy[3], LD2410.engineering.static_gate_energy[4], LD2410.engineering.static_gate_energy[5],
          LD2410.engineering.static_gate_energy[6], LD2410.engineering.static_gate_energy[7], LD2410.engineering.static_gate_energy[8],
          LD2410.engineering.light, LD2410.engineering.out_pin);
    }
#endif
  }
}

void LD2410_Deinit(void) {
  SETREGS
  if (LD2410.ts) {
    deleteTS(LD2410.ts);
    LD2410.ts = nullptr;
  }
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t function) {
  bool result = false;
  switch (function) {
    case pFUNC_INIT:              result = Ld2410Detect();    break;
    case pFUNC_LOOP:
    case pFUNC_SLEEP_LOOP:        Ld2410Input();              break;
    case pFUNC_EVERY_100_MSECOND: Ld2410Every100MSecond();    break;
    case pFUNC_EVERY_SECOND:      Ld2410EverySecond();        break;
    case pFUNC_JSON_APPEND:       Ld2410Show(1);              break;
    case pFUNC_WEB_SENSOR:        Ld2410Show(0);              break;
    case pFUNC_COMMAND: {
        SETREGS
        result = DecodeCommand(kLd2410Commands, Ld2410Command);
      }
      break;
    case pFUNC_DEINIT:            LD2410_Deinit();            break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns102(uint32_t function) {
  bool result = false;
  if (FUNC_INIT == function) {
    Ld2410Detect();
  } else if (ld2410_state && ready) {
    switch (function) {
      case FUNC_LOOP:
      case FUNC_SLEEP_LOOP:        Ld2410Input();           break;
      case FUNC_EVERY_100_MSECOND: Ld2410Every100MSecond(); break;
      case FUNC_EVERY_SECOND:      Ld2410EverySecond();     break;
      case FUNC_JSON_APPEND:       Ld2410Show(1);           break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:        Ld2410Show(0);           break;
#  endif
      case FUNC_COMMAND:
        result = DecodeCommand(kLd2410Commands, Ld2410Command);
        break;
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor and helper macros so they don't leak
// into other dual drivers in the merged tasmota.ino.cpp TU.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef rxd_pin
#  undef txd_pin
#  undef ready
#  undef LD2410
#endif

#endif  // _LD2410_DUAL_ENABLED
