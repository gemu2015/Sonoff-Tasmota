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

#include "module.h"

#ifdef USE_SHT3X_MOD
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


MODULE_DESC module_header = {
  MODULE_SYNC,
  CURR_ARCH,
  MODULE_TYPE_SENSOR,
  SHT3X_REV,
  "SHT3X",
  mod_func_execute,
  end_of_module,
  0,
  0
};

MODULE_PART int32_t Sht3x_Detect(MODULES_TABLE *mt);
MODULE_PART void SHT3X_Show(MODULES_TABLE *mt, bool json);
MODULE_PART void SHT3X_Deinit(MODULES_TABLE *mt);
MODULE_PART bool Sht3xRead(MODULES_TABLE *mt, float &t, float &h, uint8_t sht3x_address);
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


// define strings used
DPSTR(kShtTypes3,"SHT3X|SHT3X|SHTC3");
DPSTR(kShtTypes,"%s%c%02X");

bool Sht3xRead(MODULES_TABLE *mt, float &t, float &h, uint8_t sht3x_address) {
  SETREGS
  unsigned int data[6];

  t = NAN;
  h = NAN;

  jbeginTransmission(jWire, sht3x_address);
  if (SHTC3_ADDR == sht3x_address) {
    jwrite(jWire,0x35);                  // Wake from
    jwrite(jWire,0x17);                  // sleep
    jendTransmission(jWire, false);
    jbeginTransmission(jWire, sht3x_address);
    jwrite(jWire, 0x78);                  // Disable clock stretching ( I don't think that wire library support clock stretching )
    jwrite(jWire, 0x66);                  // High resolution
  } else {
    jwrite(jWire, 0x2C);                  // Enable clock stretching
    jwrite(jWire, 0x06);                  // High repeatability
  }
  if (jendTransmission(jWire, false) != 0) {   // Stop I2C transmission
    return false;
  }
  jdelay(30);                           // Timing verified with logic analyzer (10 is to short)
  jrequestFrom(jWire, sht3x_address, (uint8_t)6);   // Request 6 bytes of data
  for (uint32_t i = 0; i < 6; i++) {
    data[i] = jread(jWire);             // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  };
  t = jConvertTemp((float)((((data[0] << 8) | data[1]) * 175) / 65535.0) - 45);
  h = jConvertHumidity((float)((((data[3] << 8) | data[4]) * 100) / 65535.0));
  return (!jisnan(t) && !jisnan(h) && (h != 0));
}

/********************************************************************************************/

int32_t Sht3x_Detect(MODULES_TABLE *mt) {
  ALLOCMEM
  mem->sht3x_addresses[0] = SHT3X_ADDR_GND;
  mem->sht3x_addresses[1] = SHT3X_ADDR_VDD;
  mem->sht3x_addresses[2] = SHTC3_ADDR;

  for (uint32_t i = 0; i < SHT3X_MAX_SENSORS; i++) {
    if (jI2cActive(mem->sht3x_addresses[i])) { continue; }
    float t;
    float h;
    if (Sht3xRead(mt, t, h, mem->sht3x_addresses[i])) {
      mem->sht3x_sensors[mem->sht3x_count].address = mem->sht3x_addresses[i];
      jGetTextIndexed(mem->sht3x_sensors[mem->sht3x_count].types, sizeof(mem->sht3x_sensors[mem->sht3x_count].types), i, jPSTR(kShtTypes3));
      jI2cSetActiveFound(mem->sht3x_sensors[mem->sht3x_count].address, mem->sht3x_sensors[mem->sht3x_count].types, 0);
      mem->sht3x_count++;
    }
  }
  return mem->sht3x_count;
}

void SHT3X_Show(MODULES_TABLE *mt, bool json) {
  SETREGS
  for (uint32_t i = 0; i < mem->sht3x_count; i++) {
    float t;
    float h;
    if (Sht3xRead(mt, t, h, mem->sht3x_sensors[i].address)) {
      char types[11];
      jstrlcpy(types, mem->sht3x_sensors[i].types, sizeof(types));
      if (mem->sht3x_count > 1) {
        jsnprintf_P(types, sizeof(types), jPSTR(kShtTypes), mem->ht3x_sensors[i].types, jIndexSeparator, mem->sht3x_sensors[i].address);
      }
      jTempHumDewShow(json, ((0 == JGetTasmotaGlobal(1)) && (0 == i)), types, t, h);
    }
  }
}

void SHT3X_Deinit(MODULES_TABLE *mt) {
  SETREGS
  for (uint32_t i = 0; i < mem->sht3x_count; i++) {
    jI2cResetActive(mem->sht3x_sensors[i].address,1);
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
