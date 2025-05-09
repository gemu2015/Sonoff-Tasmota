/*
  xsns_21_sgp30.ino - SGP30 gas and air quality sensor support for Tasmota

  Copyright (C) 2021  Gerhard Mutz

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

#ifdef USE_SGP30_MOD

//#define USE_SOFTWIRE

#include "module.h"
#include "module_defines.h"

#define SGP30_ADDRESS 0x58

// all memory must be in struct MODULE_MEMORY
typedef struct {
  TWIp *xWire;
  bool sgp30_ready;
  bool ready;
  uint8_t secs;
  uint16_t eCO2;
  uint16_t TVOC;
  uint16_t TVOC_base;
  uint16_t eCO2_base;
  float abshum;
} MODULE_MEMORY;

#define ready mem->ready
#define secs mem->secs
#define sgp30_ready mem->sgp30_ready
#define TVOC mem->TVOC
#define eCO2 mem->eCO2
#define TVOC_base mem->TVOC_base
#define eCO2_base mem->eCO2_base
#define abshum mem->abshum

#ifdef USE_SOFTWIRE
#include "Softwire/Softwire_cpp.h"
#endif 

#define SGP30_REV 1 << 16 | 4

PUSH_OPTIONS

// all functions must be declared MUDULE_PART
#ifdef USE_SOFTWIRE
// software i2c needs to define pins
#define DEFAULT_SDA_PIN 12
#define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("SGP30S", MODULE_TYPE_SENSOR, SGP30_REV,"SDA",DEFAULT_SDA_PIN,"SCL",DEFAULT_SCL_PIN,"",0,"",0)
#else
MODULE_DESCRIPTOR("SGP30", MODULE_TYPE_SENSOR, SGP30_REV, "", 0, "", 0, "", 0, "", 0)
#endif
MODULE_PART int32_t SGP30_Init();
MODULE_PART bool SGP30_IAQinit();
MODULE_PART bool SGP30_Begin();
MODULE_PART bool SGP30_IAQmeasure();
MODULE_PART bool getIAQBaseline(uint16_t *eco2_base, uint16_t *tvoc_base);
MODULE_PART bool readWordFromCommand(uint8_t command[], uint8_t commandLength, uint16_t delayms, uint16_t *readdata,
                                     uint8_t readlen);
MODULE_PART uint8_t generateCRC(uint8_t *data, uint8_t datalen);
MODULE_PART bool setHumidity(uint32_t absolute_humidity);
MODULE_PART void SGP30_Every_Second();
MODULE_PART void SGP30_Show(bool json);
MODULE_PART void SGP30_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END

// all text defs must appear here
const char HTTP_SNS_SGP30[] PROGMEM = "{s}SGP30 eCO2 {m}%d ppm {e}{s}SGP30 TVOC {m}%d ppb {e}";
const char JSON_SNS_SGP30[] PROGMEM = ",\"SGP30\":{\"eCO2\":%d,\"TVOC\":%d";
const char SGP30[] PROGMEM = "SGP30";
const char HTTP_SNS_AHUM[] PROGMEM = "{s}SGP30 Abs Humidity{m}%s g/m3{e}";
const char JSON_SNS_AHUM[] PROGMEM = ",\"aHumidity\":%s}";

// DPSTR(SGP30SN,"SGP: Serialnumber 0x%04X-0x%04X-0x%04X");
/********************************************************************************************/

int32_t SGP30_Init() {
  ALLOCMEM

  I2C_SETWIRE(0);

  ready = false;
  sgp30_ready = false;

  if (!I2C_SetDevice(SGP30_ADDRESS, 0)) {
    goto exit;
  }

  if (SGP30_Begin()) {
    ready = true;
    initialized = true;
    I2C_SetActiveFound(SGP30_ADDRESS, GSTR(SGP30), 0);
    return 0;
  }

exit:
  SGP30_Deinit();
  return -1;
}

#define SGP30_FEATURESET 0x0020  ///< The required set for this library

bool SGP30_Begin() {
  SETREGS

  uint16_t serialnumber[3];
  uint8_t command[2];
  command[0] = 0x36;
  command[1] = 0x82;
  if (!readWordFromCommand(command, 2, 10, serialnumber, 3)) {
    return false;
  }

  uint16_t featureset;
  command[0] = 0x20;
  command[1] = 0x2F;
  if (!readWordFromCommand(command, 2, 10, &featureset, 1)) {
    return false;
  }
  if ((featureset & 0xF0) != SGP30_FEATURESET) {
    return false;
  }

  // AddLog(LOG_LEVEL_INFO, GSTR(SGP30SN), serialnumber[0], serialnumber[1], serialnumber[2]);

  if (!SGP30_IAQinit()) {
    return false;
  }

  return true;
}

bool SGP30_IAQinit() {
  SETREGS
  uint8_t command[2];
  command[0] = 0x20;
  command[1] = 0x03;
  return readWordFromCommand(command, 2, 10, 0, 0);
}

bool SGP30_IAQmeasure() {
  SETREGS
  uint8_t command[2];
  command[0] = 0x20;
  command[1] = 0x08;
  uint16_t reply[2];
  if (!readWordFromCommand(command, 2, 12, reply, 2)) {
    return false;
  }
  TVOC = reply[1];
  eCO2 = reply[0];
  return true;
}

bool getIAQBaseline(uint16_t *eco2_base, uint16_t *tvoc_base) {
  SETREGS
  uint8_t command[2];
  command[0] = 0x20;
  command[1] = 0x15;
  uint16_t reply[2];
  if (!readWordFromCommand(command, 2, 10, reply, 2)) {
    return false;
  }
  *eco2_base = reply[0];
  *tvoc_base = reply[1];
  return true;
}

bool setHumidity(uint32_t absolute_humidity) {
  SETREGS
  if (absolute_humidity > ICONST(256000)) {
    return false;
  }

  // uint16_t ah_scaled = (uint16_t)(((uint64_t)absolute_humidity * 256 * 16777) >> 24);
  uint64_t llval = tmod__muldi3(absolute_humidity << 8, ICONST(16777));
  uint16_t ah_scaled = llval >> 24;
  uint8_t command[5];
  command[0] = 0x20;
  command[1] = 0x61;
  command[2] = ah_scaled >> 8;
  command[3] = ah_scaled & 0xFF;
  command[4] = generateCRC(command + 2, 2);

  return readWordFromCommand(command, 5, 10, 0, 0);
}

bool readWordFromCommand(uint8_t command[], uint8_t commandLength, uint16_t delayms, uint16_t *readdata,
                         uint8_t readlen) {
  SETREGS
  I2C_beginTransmission(SGP30_ADDRESS);
  for (uint8_t i = 0; i < commandLength; i++) {
    I2C_write(command[i]);
  }
  I2C_endTransmission(false);

  delay(delayms);

  if (readlen == 0) {
    return true;
  }

  uint8_t replylen = readlen * (2 + 1);
  if (I2C_requestFrom(SGP30_ADDRESS, replylen) != replylen) {
    return false;
  }
  uint8_t replybuffer[replylen];
  for (uint8_t i = 0; i < replylen; i++) {
    replybuffer[i] = I2C_read();
  }

  for (uint8_t i = 0; i < readlen; i++) {
    uint8_t crc = generateCRC(replybuffer + i * 3, 2);
    if (crc != replybuffer[i * 3 + 2]) {
      return false;
    }
    // success! store it
    readdata[i] = replybuffer[i * 3];
    readdata[i] <<= 8;
    readdata[i] |= replybuffer[i * 3 + 1];
  }
  return true;
}

#define SGP30_CRC8_POLYNOMIAL 0x31  ///< Seed for SGP30's CRC polynomial
#define SGP30_CRC8_INIT 0xFF        ///< Init value for CRC

uint8_t generateCRC(uint8_t *data, uint8_t datalen) {
  // calculates 8-Bit checksum with given polynomial
  uint8_t crc = SGP30_CRC8_INIT;

  for (uint8_t i = 0; i < datalen; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ SGP30_CRC8_POLYNOMIAL;
      else
        crc <<= 1;
    }
  }
  return crc;
}

#define SAVE_PERIOD 30

void SGP30_Every_Second() {
  SETREGS
  STGLOB

  if (!ready) return;

  sgp30_ready = false;
  if (!SGP30_IAQmeasure()) {
    return;  // Measurement failed
  }

  // if (TasmotaGlobal.global_update && (TasmotaGlobal.humidity > 0) && !isnan(TasmotaGlobal.temperature_celsius)) {

  if (TasmotaGlobal->global_update && (fixunssfsi(TasmotaGlobal->humidity) > 0) &&
      !isnan(TasmotaGlobal->temperature_celsius)) {
    // abs hum in mg/m3
    abshum = CalcTempHumToAbsHum(TasmotaGlobal->temperature_celsius, TasmotaGlobal->humidity);
    setHumidity(tmod__fixunssfsi(tmod__mulsf3(abshum, 1000)));
  }

  sgp30_ready = true;

  secs++;
  // these should normally be stored permanently and used for fast restart
  if (secs >= SAVE_PERIOD) {
    secs = 0;
    // store settings every N seconds
    if (!getIAQBaseline(&eCO2_base, &TVOC_base)) {
      return;  // Failed to get baseline readings
    }
    //  AddLog(LOG_LEVEL_DEBUG, GSTR("SGP: Baseline values eCO2 0x%04X, TVOC 0x%04X"), eCO2_base, TVOC_base);
  }
}

void SGP30_Show(bool json) {
  SETREGS
  STGLOB

  if (sgp30_ready) {
    char abs_hum[33];
    bool ahum_available = TasmotaGlobal->global_update && (fixunssfsi(TasmotaGlobal->humidity) > 0) &&
                          !isnan(TasmotaGlobal->temperature_celsius);
    if (ahum_available) {
      // has humidity + temperature
      ftostrfd(abshum, 4, abs_hum);
    }

    if (json) {
      ResponseAppend_P(GSTR(JSON_SNS_SGP30), eCO2, TVOC);
      if (ahum_available) {
        ResponseAppend_P(GSTR(JSON_SNS_AHUM), abs_hum);
      } else {
        ResponseJsonEnd();
      }
    } else {
      WSContentSend_PD(GSTR(HTTP_SNS_SGP30), eCO2, TVOC);
      if (ahum_available) {
        WSContentSend_PD(GSTR(HTTP_SNS_AHUM), abs_hum);
      }
    }
  }
}

void SGP30_Deinit() {
  SETREGS
  I2C_ResetActive(SGP30_ADDRESS, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;

  switch (sel) {
    case pFUNC_INIT:
      result = SGP30_Init();
      break;
    case pFUNC_EVERY_SECOND:
      SGP30_Every_Second();
      break;
    case pFUNC_JSON_APPEND:
      SGP30_Show(1);
      break;
    case pFUNC_WEB_SENSOR:
      SGP30_Show(0);
      break;
    case pFUNC_DEINIT:
      SGP30_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_SGP30_MOD
