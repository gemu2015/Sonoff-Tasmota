/*
  xsns_14_sht3x.ino - SHT3X temperature and humidity sensor support for Tasmota

  Copyright (C) 2021  Theo Arends

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

#ifdef USE_SHT3X_MOD

//#define USE_SOFTWIRE

#include "module.h"
#include "module_defines.h"

/*********************************************************************************************\
 * SHT3X and SHTC3 - Temperature and Humidity
 *
 * I2C Address: 0x44, 0x45 or 0x70 (SHTC3)
\*********************************************************************************************/

#define SHT3X_ADDR_GND 0x44  // address pin low (GND)
#define SHT3X_ADDR_VDD 0x45  // address pin high (VDD)
#define SHTC3_ADDR 0x70      // address for shtc3 sensor

#define SHT3X_MAX_SENSORS 3

#define SHT3X_REV 1 << 16 | 4

PUSH_OPTIONS

#ifdef USE_SOFTWIRE
// software i2c needs to define pins
#define DEFAULT_SDA_PIN 12
#define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("SHT3XS", MODULE_TYPE_SENSOR, SHT3X_REV,"SDA",DEFAULT_SDA_PIN,"SCL",DEFAULT_SCL_PIN,"",0,"",0)
#else
MODULE_DESCRIPTOR("SHT3X", MODULE_TYPE_SENSOR, SHT3X_REV, "", 0, "", 0, "", 0, "", 0)
#endif
MODULE_PART int32_t Sht3x_Detect();
MODULE_PART void SHT3X_Show(bool json);
MODULE_PART void SHT3X_Deinit();
MODULE_PART bool Sht3xRead(float &t, float &h, uint8_t sht3x_address);
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END

typedef struct {
  uint8_t address;  // I2C bus address
  char types[6];    // Sensor type name and address - "SHT3X-0xXX"
} SHT3XSTRUCT;

// define memory used
typedef struct {
  TWIp *xWire;
  uint8_t sht3x_count;
  uint8_t sht3x_addresses[3];
  SHT3XSTRUCT sht3x_sensors[SHT3X_MAX_SENSORS];
} MODULE_MEMORY;

#ifdef USE_SOFTWIRE
#include "Softwire/Softwire_cpp.h"
#endif 

#define sht3x_count mem->sht3x_count
#define sht3x_addresses mem->sht3x_addresses
#define sht3x_sensors mem->sht3x_sensors

// define strings used
const char kShtTypes3[] PROGMEM = "SHT3X|SHT3X|SHTC3";
const char kShtTypes[] PROGMEM = "%s%c%02X";
const char HTTP_SNS_AHUM[] PROGMEM = "{s}%s %s{m}%s g/m3{e}";
const float FP_CONST[] PROGMEM = {65535, 45};

bool Sht3xRead(float &t, float &h, uint8_t sht3x_address) {
  SETREGS
  unsigned int data[6];

  t = jNAN;
  h = t;

  I2C_beginTransmission(sht3x_address);
  if (SHTC3_ADDR == sht3x_address) {
    I2C_write(0x35);  // Wake from
    I2C_write(0x17);  // sleep
    I2C_endTransmission(true);
    I2C_beginTransmission(sht3x_address);
    I2C_write(0x78);  // Disable clock stretching ( I don't think that wire library support clock stretching )
    I2C_write(0x66);  // High resolution
  } else {
    I2C_write(0x2C);  // Enable clock stretching
    I2C_write(0x06);  // High repeatability
  }
  if (I2C_endTransmission(true) != 0) {  // Stop I2C transmission
    AddLog(LOG_LEVEL_INFO, PSTR("i2c error"));
    return false;
  }
  delay(30);                      // Timing verified with logic analyzer (10 is to short)
  I2C_requestFrom(sht3x_address, 6);  // Request 6 bytes of data
  for (uint32_t i = 0; i < 6; i++) {
    data[i] = I2C_read();  // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  };

  t = fdiv(tofloat(((data[0] << 8) | data[1]) * 175), FLTC(0));
  t = fdiff(t, FLTC(1));
  // t = ConvertTemp((float)( ( ( (data[0] << 8) | data[1] ) * 175) / 65535.0) - 45);
  t = ConvertTemp(t);

  h = fdiv(tofloat(((data[3] << 8) | data[4]) * 100), FLTC(0));
  //  h = ConvertHumidity((float)((((data[3] << 8) | data[4]) * 100) / 65535.0));
  h = ConvertHumidity(h);
  return (!isnan(t) && !isnan(h) && !iseq(h));
}

/********************************************************************************************/

int32_t Sht3x_Detect() {
  ALLOCMEM
  sht3x_addresses[0] = SHT3X_ADDR_GND;
  sht3x_addresses[1] = SHT3X_ADDR_VDD;
  sht3x_addresses[2] = SHTC3_ADDR;



  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    for (uint32_t i = 0; i < SHT3X_MAX_SENSORS; i++) {
      if (!I2C_SetDevice(sht3x_addresses[i], bus)) {
        continue;
      }

      float t;
      float h;
      if (Sht3xRead(t, h, sht3x_addresses[i])) {
        sht3x_sensors[sht3x_count].address = sht3x_addresses[i];
        GetTextIndexed(sht3x_sensors[sht3x_count].types, sizeof(sht3x_sensors[sht3x_count].types), i, GSTR(kShtTypes3));
        I2C_SetActiveFound(sht3x_sensors[sht3x_count].address, sht3x_sensors[sht3x_count].types, 0);
        sht3x_count++;
      }
    }
    if (sht3x_count) {
      break;
    }
  }

  if (sht3x_count) {
    initialized = true;
  } else {
    SHT3X_Deinit();
  }
  return sht3x_count;
}

void SHT3X_Show(bool json) {
  SETREGS
  STGLOB

  for (uint32_t i = 0; i < sht3x_count; i++) {
    float t;
    float h;
    if (Sht3xRead(t, h, sht3x_sensors[i].address)) {
      char types[11];
      strlcpy(types, sht3x_sensors[i].types, sizeof(types));
      if (sht3x_count > 1) {
        char *types = sht3x_sensors[i].types;
        snprintf_P(types, sizeof(types), GSTR(kShtTypes), types, IndexSeparator(), sht3x_sensors[i].address);
      }
      TempHumDewShow(json, ((0 == TasmotaGlobal->tele_period) && (0 == i)), types, t, h);

      float abshum = CalcTempHumToAbsHum(t, h);
      char abs_hum[32];
      ftostrfd(abshum, 4, abs_hum);
      if (!json) {
        char s1[32];
        WSContentSend_PD(GSTR(HTTP_SNS_AHUM), types, Plugin_Get_SensorNames(s1, iD_ABSOLUTE_HUMIDITY), abs_hum);
      } else {
      }
    }
  }
}

void SHT3X_Deinit() {
  SETREGS
  for (uint32_t i = 0; i < sht3x_count; i++) {
    I2C_ResetActive(sht3x_sensors[i].address, 0);
  }
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:
      result = Sht3x_Detect();
      break;
    case pFUNC_JSON_APPEND:
      SHT3X_Show(1);
      break;
    case pFUNC_WEB_SENSOR:
      SHT3X_Show(0);
      break;
    case pFUNC_DEINIT:
      SHT3X_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_SHT3X
