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
#include "module.h"
#include "module_defines.h"

/*********************************************************************************************\
 * SHT3X and SHTC3 - Temperature and Humidity
 *
 * I2C Address: 0x44, 0x45 or 0x70 (SHTC3)
\*********************************************************************************************/


#define SHT3X_ADDR_GND      0x44       // address pin low (GND)
#define SHT3X_ADDR_VDD      0x45       // address pin high (VDD)
#define SHTC3_ADDR          0x70       // address for shtc3 sensor

#define SHT3X_MAX_SENSORS   3

#define SHT3X_REV  1


MODULE_DESCRIPTOR("SHT3X",MODULE_TYPE_SENSOR,SHT3X_REV)
MODULE_PART int32_t Sht3x_Detect(MODULES_TABLE *mt);
MODULE_PART void SHT3X_Show(MODULES_TABLE *mt, bool json);
MODULE_PART void SHT3X_Deinit(MODULES_TABLE *mt);
MODULE_PART bool Sht3xRead(MODULES_TABLE *mt, float &t, float &h, uint8_t sht3x_address);
MODULE_PART int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel);
MODULE_END


typedef struct {
  uint8_t address;    // I2C bus address
  char types[6];      // Sensor type name and address - "SHT3X-0xXX"
} SHT3XSTRUCT;


// define memory used
typedef struct {
  uint8_t sht3x_count;
  uint8_t sht3x_addresses[3];
  SHT3XSTRUCT sht3x_sensors[SHT3X_MAX_SENSORS];
} MODULE_MEMORY;

#define sht3x_count mem->sht3x_count
#define sht3x_addresses mem->sht3x_addresses
#define sht3x_sensors mem->sht3x_sensors


// define strings used
DPSTR(kShtTypes3,"SHT3X|SHT3X|SHTC3");
DPSTR(kShtTypes,"%s%c%02X");

bool Sht3xRead(MODULES_TABLE *mt, float &t, float &h, uint8_t sht3x_address) {
  SETREGS
  unsigned int data[6];

  t = jNAN;
  h = t;

  beginTransmission(sht3x_address);
  if (SHTC3_ADDR == sht3x_address) {
    write(0x35);                  // Wake from
    write(0x17);                  // sleep
    endTransmission(false);
    beginTransmission(sht3x_address);
    write(0x78);                  // Disable clock stretching ( I don't think that wire library support clock stretching )
    write(0x66);                  // High resolution
  } else {
    write(0x2C);                  // Enable clock stretching
    write(0x06);                  // High repeatability
  }
  if (endTransmission(false) != 0) {   // Stop I2C transmission
    return false;
  }
  delay(30);                           // Timing verified with logic analyzer (10 is to short)
  requestFrom(sht3x_address, (uint8_t)6);   // Request 6 bytes of data
  for (uint32_t i = 0; i < 6; i++) {
    data[i] = read();             // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  };
  t = jfdiv( jtofloat(((data[0] << 8) | data[1] ) * 175), 65535.0);
  t = jfdiff(t, 45);
  //t = jConvertTemp((float)( ( ( (data[0] << 8) | data[1] ) * 175) / 65535.0) - 45);
  t = ConvertTemp(t);

  h = t = jfdiv( jtofloat(((data[3] << 8) | data[4] ) * 100), 65535.0);
//  h = jConvertHumidity((float)((((data[3] << 8) | data[4]) * 100) / 65535.0));
  h = ConvertHumidity(h);
  return (!jisnan(t) && !jisnan(h) && !jiseq(h));
}

/********************************************************************************************/

int32_t Sht3x_Detect(MODULES_TABLE *mt) {
  ALLOCMEM
  sht3x_addresses[0] = SHT3X_ADDR_GND;
  sht3x_addresses[1] = SHT3X_ADDR_VDD;
  sht3x_addresses[2] = SHTC3_ADDR;

  for (uint32_t i = 0; i < SHT3X_MAX_SENSORS; i++) {
    if (I2cActive(sht3x_addresses[i])) { continue; }
    float t;
    float h;
    if (Sht3xRead(mt, t, h, sht3x_addresses[i])) {
      sht3x_sensors[sht3x_count].address = sht3x_addresses[i];
      GetTextIndexed(sht3x_sensors[sht3x_count].types, sizeof(sht3x_sensors[sht3x_count].types), i, jPSTR(kShtTypes3));
      I2cSetActiveFound(sht3x_sensors[sht3x_count].address, sht3x_sensors[sht3x_count].types, 0);
      sht3x_count++;
    }
  }
  return sht3x_count;
}

void SHT3X_Show(MODULES_TABLE *mt, bool json) {
  SETREGS
  for (uint32_t i = 0; i < sht3x_count; i++) {
    float t;
    float h;
    if (Sht3xRead(mt, t, h, sht3x_sensors[i].address)) {
      char types[11];
      strlcpy(types, sht3x_sensors[i].types, sizeof(types));
      if (sht3x_count > 1) {
        char *types = sht3x_sensors[i].types;
        snprintf_P(types, sizeof(types), jPSTR(kShtTypes), types, IndexSeparator(), sht3x_sensors[i].address);
        //jsnprintf_P(types, sizeof(types), jPSTR(kShtTypes), mem->ht3x_sensors[i].types, jIndexSeparator(), addr);
      }
      TempHumDewShow(json, ((0 == GetTasmotaGlobal(1)) && (0 == i)), types, t, h);
    }
  }
}

void SHT3X_Deinit(MODULES_TABLE *mt) {
  SETREGS
  for (uint32_t i = 0; i < sht3x_count; i++) {
    I2cResetActive(sht3x_sensors[i].address,1);
  }
  RETMEM
}



/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = Sht3x_Detect(mt);
      break;
    case FUNC_JSON_APPEND:
      SHT3X_Show(mt, 1);
      break;
    case FUNC_WEB_SENSOR:
      SHT3X_Show(mt, 0);
      break;
    case FUNC_DEINIT:
      SHT3X_Deinit(mt);
      break;
  }
  return result;
}

#endif  // USE_SHT3X
