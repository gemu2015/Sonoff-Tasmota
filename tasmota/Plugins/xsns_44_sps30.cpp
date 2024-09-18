/*
  xsns_44_sps30.ino - Sensirion SPS30 support for Tasmota

  Copyright (C) 2021  Gerhard Mutz and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tasmota_options.h"

#ifdef USE_SPS30_MOD

#include "module.h"
#include "module_defines.h"

#define SPS30_ADDR 0x69

#define SPS_CMD_START_MEASUREMENT 0x0010
#define SPS_CMD_START_MEASUREMENT_ARG 0x0300
#define SPS_CMD_STOP_MEASUREMENT 0x0104
#define SPS_CMD_READ_MEASUREMENT 0x0300
#define SPS_CMD_GET_DATA_READY 0x0202
#define SPS_CMD_AUTOCLEAN_INTERVAL 0x8004
#define SPS_CMD_CLEAN 0x5607
#define SPS_CMD_GET_ACODE 0xd025
#define SPS_CMD_GET_SERIAL 0xd033
#define SPS_CMD_RESET 0xd304
#define SPS_WRITE_DELAY_US 20000
#define SPS_MAX_SERIAL_LEN 32

// all memory must be in struct MODULE_MEMORY
typedef struct {
  float PM1_0;
  float PM2_5;
  float PM4_0;
  float PM10;
  float NCPM0_5;
  float NCPM1_0;
  float NCPM2_5;
  float NCPM4_0;
  float NCPM10;
  float TYPSIZ;
} SPS30_DATA;

typedef struct {
  TwoWire *xWire;
  SPS30_DATA sps30_result;
  bool sps30_running;
  bool ready;
  uint16_t secs;
} MODULE_MEMORY;

#define sps30_result mem->sps30_result
#define sps30_running mem->sps30_running
#define ready mem->ready
#define secs mem->secs

#define SPS30_REV 1 << 16 | 4

PUSH_OPTIONS

// all functions must be declared MUDULE_PART
MODULE_DESCRIPTOR("SPS30", MODULE_TYPE_SENSOR, SPS30_REV, "", 0, "", 0, "", 0, "", 0)
MODULE_PART int32_t SPS30_Init();
MODULE_PART void SPS30_Every_Second();
MODULE_PART void SPS30_Show(bool json);
MODULE_PART void SPS30_Deinit();
MODULE_PART uint8_t sps30_calc_CRC(uint8_t *data);
MODULE_PART void sps30_cmd(uint16_t cmd);
MODULE_PART bool SPS30_command();
MODULE_PART void CmdClean();
MODULE_PART void sps30_get_data(uint16_t cmd, uint8_t *data, uint8_t dlen);
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END

// strings
const char SPS30[] PROGMEM = "SPS30";
const char SPS30_serial[] PROGMEM = "sps30 found with serial: %s";

/********************************************************************************************/

int32_t SPS30_Init() {
  ALLOCMEM

  SETWIRE(0);

  if (!I2cSetDevice(SPS30_ADDR, 0)) {
    goto exit;
  }

  uint8_t dcode[32];
  sps30_get_data(SPS_CMD_GET_SERIAL, dcode, sizeof(dcode));
  if (dcode[0] == 0) {
  exit:
    SPS30_Deinit();
    return -1;
  }

  AddLog(LOG_LEVEL_INFO, GSTR(SPS30_serial), dcode);

  sps30_cmd(SPS_CMD_START_MEASUREMENT);
  sps30_running = 1;
  ready = 1;
  initialized = 1;
  secs = 0;
  I2cSetActiveFound(SPS30_ADDR, GSTR(SPS30), 0);
  return 0;
}

uint8_t sps30_calc_CRC(uint8_t *data) {
  uint8_t crc = 0xFF;
  for (uint32_t i = 0; i < 2; i++) {
    crc ^= data[i];
    for (uint32_t bit = 8; bit > 0; --bit) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31u;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

void sps30_get_data(uint16_t cmd, uint8_t *data, uint8_t dlen) {
  SETREGS
  uint8_t tmp[3];
  uint8_t index = 0;
  memset(data, 0, dlen);
  uint8_t twi_buff[64];
  memset(twi_buff, 0, sizeof(twi_buff));

  beginTransmission(SPS30_ADDR);
  I2cWrite(cmd >> 8);
  I2cWrite(cmd);
  endTransmission(true);  // true = default

  // need 60 bytes max
  dlen /= 2;
  dlen *= 3;

#ifdef ESP8266
  twi_readFrom(SPS30_ADDR, twi_buff, dlen, 1);
#endif  // ESP8266

#ifdef ESP32
  Wire.requestFrom((uint16_t)SPS30_ADDR, dlen, true);
  Wire.readBytes(twi_buff, dlen);
#endif  // ESP32

  uint8_t bind = 0;
  while (bind < dlen) {
    tmp[0] = twi_buff[bind++];
    tmp[1] = twi_buff[bind++];
    tmp[2] = twi_buff[bind++];
    if (sps30_calc_CRC(tmp) != tmp[2]) {
      // chksum error
      index += 2;
    } else {
      data[index++] = tmp[0];
      data[index++] = tmp[1];
    }
  }
}

void sps30_cmd(uint16_t cmd) {
  SETREGS
  unsigned char cmdb[6];
  beginTransmission(SPS30_ADDR);
  cmdb[0] = cmd >> 8;
  cmdb[1] = cmd;

  uint8_t num = 2;
  if (cmd == SPS_CMD_START_MEASUREMENT) {
    cmdb[2] = SPS_CMD_START_MEASUREMENT_ARG >> 8;
    cmdb[3] = SPS_CMD_START_MEASUREMENT_ARG & 0xff;
    cmdb[4] = sps30_calc_CRC(&cmdb[2]);
    num = 5;
  }
  for (uint16_t cnt = 0; cnt < num; cnt++) {
    I2cWrite(cmdb[cnt]);
  }
  endTransmission(true);
}

void SPS30_Every_Second() {
  SETREGS

  if (!ready) return;
  if (!sps30_running) return;

  if (tmod__umodsi3(secs, 10) == 0) {
    // every 10 seconds
    uint8_t vars[sizeof(float) * 10];
    sps30_get_data(SPS_CMD_READ_MEASUREMENT, vars, sizeof(vars));
    float *fp = &sps30_result.PM1_0;

    typedef union {
      uint8_t array[4];
      float value;
    } ByteToFloat;

    ByteToFloat conv;

    for (uint32_t count = 0; count < 10; count++) {
      for (uint32_t i = 0; i < 4; i++) {
        conv.array[3 - i] = vars[count * sizeof(float) + i];
      }
      *fp++ = conv.value;
    }
  }
  secs++;

  if (secs > 3600) {
    secs = 0;
    // should auto clean once per week runtime
    // so count hours, should be in Settings
    Settings->sps30_inuse_hours++;
    if (Settings->sps30_inuse_hours > (7 * 24)) {
      CmdClean();
      Settings->sps30_inuse_hours = 0;
    }
  }
}

#define PMDP 2

const char HTTP_SNS_SPS30_a[] PROGMEM = "{s}SPS30 PM %0d.%0d{m}%s ug/m3{e}";
const char HTTP_SNS_SPS30_b[] PROGMEM = "{s}SPS30 NCPM %0d.%0d{m}%s #/cm3{e}";
const char HTTP_SNS_SPS30_c[] PROGMEM = "{s}SPS30 TYPSIZ {m}%s um{e}";

const char JSON_SNS_SPS30_a[] PROGMEM = ",\"SPS30\":{\"PM%0d_%0d\":%s";
const char JSON_SNS_SPS30_b[] PROGMEM = ",\"PM%0d_%0d\":%s";
const char JSON_SNS_SPS30_c[] PROGMEM = ",\"NCPM%0d_%0d\":%s";
const char JSON_SNS_SPS30_d[] PROGMEM = ",\"TYPSIZ\":%s}";

void SPS30_Show(bool json) {
  SETREGS

  if (!ready) return;
  if (!sps30_running) return;

  char str[64];
  if (json) {
    ftostrfd(sps30_result.PM1_0, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_a), 1, 0, str);
    ftostrfd(sps30_result.PM2_5, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_b), 2, 5, str);
    ftostrfd(sps30_result.PM4_0, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_b), 4, 0, str);
    ftostrfd(sps30_result.PM10, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_b), 10, 0, str);

    ftostrfd(sps30_result.NCPM0_5, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 0, 5, str);
    ftostrfd(sps30_result.NCPM1_0, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 1, 0, str);
    ftostrfd(sps30_result.NCPM2_5, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 2, 5, str);
    ftostrfd(sps30_result.NCPM4_0, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 4, 0, str);
    ftostrfd(sps30_result.NCPM10, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 10, 0, str);

    ftostrfd(sps30_result.TYPSIZ, PMDP, str);
    ResponseAppend_P(GSTR(JSON_SNS_SPS30_d), str);

  } else {
    ftostrfd(sps30_result.PM1_0, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 1, 0, str);
    ftostrfd(sps30_result.PM2_5, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 2, 5, str);
    ftostrfd(sps30_result.PM4_0, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 4, 0, str);
    ftostrfd(sps30_result.PM10, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 10, 0, str);

    ftostrfd(sps30_result.NCPM0_5, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 0, 5, str);
    ftostrfd(sps30_result.NCPM1_0, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 1, 0, str);
    ftostrfd(sps30_result.NCPM2_5, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 2, 5, str);
    ftostrfd(sps30_result.NCPM4_0, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 4, 0, str);
    ftostrfd(sps30_result.NCPM10, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 10, 0, str);

    ftostrfd(sps30_result.TYPSIZ, PMDP, str);
    WSContentSend_PD(GSTR(HTTP_SNS_SPS30_c), str);
  }
}

const char S_JSON_SPS30_FAN[] PROGMEM = ",\"SPS30\":{\"CFAN\":\"true\"}}";
void CmdClean() {
  SETREGS
  sps30_cmd(SPS_CMD_CLEAN);
  ResponseTime_P(GSTR(S_JSON_SPS30_FAN));
  MqttPublishTeleSensor();
}

const char kSPS30_Commands[] PROGMEM = "Start|Stop|Clean";
enum MP3_Commands { CMND_SPS30_Start, CMND_SPS30_Stop, CMND_SPS30_Clean };
const char S_JSON_SPS30_COMMAND[] PROGMEM = "{\"SPS30\":\"%s\"}";
const char S_JSON_SPS30_r[] PROGMEM = "running";
const char S_JSON_SPS30_s[] PROGMEM = "stopped";

bool SPS30_command() {
  SETREGS
  char command[CMDSZ];
  bool serviced = false;
  uint8_t disp_len = strlen((char *)GSTR(SPS30));

  if (!strncasecmp_P(XdrvMailbox->topic, GSTR(SPS30), disp_len)) {  // prefix
    serviced = true;
    int command_code = GetCommandCode(command, sizeof(command), XdrvMailbox->topic + disp_len, GSTR(kSPS30_Commands));
    switch (command_code) {
      case CMND_SPS30_Start:
        sps30_running = 1;
        sps30_cmd(SPS_CMD_START_MEASUREMENT);
        break;
      case CMND_SPS30_Stop:
        sps30_running = 0;
        sps30_cmd(SPS_CMD_STOP_MEASUREMENT);
        break;
      case CMND_SPS30_Clean:
        CmdClean();
        break;
      default:
        serviced = false;
    }
    Response_P(GSTR(S_JSON_SPS30_COMMAND), sps30_running ? GSTR(S_JSON_SPS30_r) : GSTR(S_JSON_SPS30_s));
  }

  return serviced;
}

void SPS30_Deinit() {
  SETREGS
  I2cResetActive(SPS30_ADDR, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;

  switch (sel) {
    case pFUNC_INIT:
      result = SPS30_Init();
      break;
    case pFUNC_EVERY_SECOND:
      SPS30_Every_Second();
      break;
    case pFUNC_JSON_APPEND:
      SPS30_Show(1);
      break;
    case pFUNC_WEB_SENSOR:
      SPS30_Show(0);
      break;
    case pFUNC_COMMAND:
      result = SPS30_command();
      break;
    case pFUNC_DEINIT:
      SPS30_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_SPS30
