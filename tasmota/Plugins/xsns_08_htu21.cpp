/*
  xsns_08_htu21.ino - HTU21 temperature and humidity sensor support for Tasmota

  Copyright (C) 2021  Heiko Krupp and Theo Arends

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

#ifdef USE_HTU_MOD
#include "module.h"
#include "module_defines.h"
/*********************************************************************************************\
 * HTU21 - Temperature and Humidity
 *
 * Source: Heiko Krupp
 *
 * I2C Address: 0x40
\*********************************************************************************************/


#define HTU21_ADDR          0x40

#define SI7013_CHIPID       0x0D
#define SI7020_CHIPID       0x14
#define SI7021_CHIPID       0x15
#define HTU21_CHIPID        0x32

#define HTU21_READTEMP      0xE3
#define HTU21_READHUM       0xE5
#define HTU21_WRITEREG      0xE6
#define HTU21_READREG       0xE7
#define HTU21_RESET         0xFE
#define HTU21_HEATER_WRITE  0x51
#define HTU21_HEATER_READ   0x11
#define HTU21_SERIAL2_READ1 0xFC    /* Read 3rd two Serial bytes */
#define HTU21_SERIAL2_READ2 0xC9    /* Read 4th two Serial bytes */

#define HTU21_HEATER_ON     0x04
#define HTU21_HEATER_OFF    0xFB

#define HTU21_RES_RH12_T14  0x00  // Default
#define HTU21_RES_RH8_T12   0x01
#define HTU21_RES_RH10_T13  0x80
#define HTU21_RES_RH11_T11  0x81

#define HTU21_CRC8_POLYNOM  0x13100


#define HTU_REV  1<<16

// define calls
MODULE_DESCRIPTOR("HTU21",MODULE_TYPE_SENSOR,HTU_REV,"",0,"",0,"",0,"",0)
MODULE_PART int32_t MOD_FUNC(HTU_Detect);
MODULE_PART void MOD_FUNC(HTU_Show, bool json);
MODULE_PART void MOD_FUNC(HTU_Deinit);
MODULE_PART uint8_t HtuCheckCrc8(uint16_t data);
MODULE_PART uint8_t MOD_FUNC(HtuReadDeviceId);
MODULE_PART void MOD_FUNC(HtuSetResolution, uint8_t resolution);
MODULE_PART void MOD_FUNC(HtuReset);
MODULE_PART void MOD_FUNC(HtuHeater, uint8_t heater);
MODULE_PART void MOD_FUNC(HTU_Init);
MODULE_PART bool MOD_FUNC(HTU_Read);
MODULE_PART void MOD_FUNC(HTU_EverySecond);
MODULE_PART int32_t MOD_FUNC(mod_func_execute, uint32_t sel);
MODULE_END

// define strings used
const char kHtuTypes[] PROGMEM = "HTU21|SI7013|SI7020|SI7021|T/RH?";

typedef struct {
  float temperature;
  float humidity;
  uint8_t address;
  uint8_t type;
  uint8_t jdelay_temp;
  uint8_t jdelay_humidity;
  uint8_t valid;
  uint8_t cnt;
  char types[7];
} HTU;

// define memory used
typedef struct {
  HTU Htu;
} MODULE_MEMORY;

#define Htu mem->Htu


/*********************************************************************************************/

uint8_t HtuCheckCrc8(uint16_t data) {
  for (uint32_t bit = 0; bit < 16; bit++) {
    if (data & 0x8000) {
      data =  (data << 1) ^ HTU21_CRC8_POLYNOM;
    } else {
      data <<= 1;
    }
  }
  return data >>= 8;
}

uint8_t MOD_FUNC(HtuReadDeviceId) {
  SETREGS

  CALL_MOD_FUNC(HtuReset);  // Fixes ESP32 sensor loss at restart

  uint16_t deviceID = 0;
  uint8_t checksum = 0;

  beginTransmission(HTU21_ADDR);
  write(HTU21_SERIAL2_READ1);
  write(HTU21_SERIAL2_READ2);
  endTransmission(0);

  requestFrom(HTU21_ADDR, 3);
  deviceID  = read() << 8;
  deviceID |= read();
  checksum  = read();
  if (HtuCheckCrc8(deviceID) == checksum) {
    deviceID = deviceID >> 8;
  } else {
    deviceID = 0;
  }
  return (uint8_t)deviceID;
}

void MOD_FUNC(HtuSetResolution, uint8_t resolution) {
  SETREGS
  uint8_t current = I2cRead8(HTU21_ADDR, HTU21_READREG);
  current &= 0x7E;          // Replace current resolution bits with 0
  current |= resolution;    // Add new resolution bits to register
  I2cWrite8(HTU21_ADDR, HTU21_WRITEREG, current);
}

void MOD_FUNC(HtuReset) {
  SETREGS
  beginTransmission(HTU21_ADDR);
  write(HTU21_RESET);
  endTransmission(0);
  delay(15);                // Reset takes 15ms
}

void MOD_FUNC(HtuHeater, uint8_t heater) {
  SETREGS
  uint8_t current = I2cRead8(HTU21_ADDR, HTU21_READREG);

  switch(heater)
  {
    case HTU21_HEATER_ON  : current |= heater;
                            break;
    case HTU21_HEATER_OFF : current &= heater;
                            break;
    default               : current &= heater;
                            break;
  }
  I2cWrite8(HTU21_ADDR, HTU21_WRITEREG, current);
}

void MOD_FUNC(HTU_Init) {
  SETREGS
  CALL_MOD_FUNC(HtuReset);
  CALL_MOD_FUNC(HtuHeater, HTU21_HEATER_OFF);
  CALL_MOD_FUNC(HtuSetResolution, HTU21_RES_RH12_T14);
}

bool MOD_FUNC(HTU_Read) {
  SETREGS
  uint8_t  checksum = 0;
  uint16_t sensorval = 0;

  if (Htu.valid) { Htu.valid--; }

  beginTransmission(HTU21_ADDR);
  write(HTU21_READTEMP);
  if (endTransmission(0) != 0) { return false; }           // In case of error
  delay(Htu.jdelay_temp);                                       // Sensor time at max resolution

  requestFrom(HTU21_ADDR, 3);
  if (3 == available()) {
    sensorval = read() << 8;                              // MSB
    sensorval |= read();                                  // LSB
    checksum = read();
  }
  if (HtuCheckCrc8(sensorval) != checksum) { return false; }   // Checksum mismatch

  //Htu.temperature = jConvertTemp(0.002681 * (float)sensorval - 46.85);
  Htu.temperature = ConvertTemp(jfscale(sensorval, 0.002681, 46.85));

  beginTransmission(HTU21_ADDR);
  write(HTU21_READHUM);
  if (endTransmission(0) != 0) { return false; }           // In case of error
  delay(Htu.jdelay_humidity);                                   // Sensor time at max resolution

  requestFrom(HTU21_ADDR, 3);
  if (3 <= available()) {
    sensorval = read() << 8;                              // MSB
    sensorval |= read();                                  // LSB
    checksum = read();
  }
  if (HtuCheckCrc8(sensorval) != checksum) { return false; }   // Checksum mismatch

  sensorval ^= 0x02;                                           // clear status bits
  //Htu.humidity = 0.001907 * (float)sensorval - 6;
  Htu.humidity = jfdiff(jfmul(0.001907 , jtofloat(sensorval)),6);

  //if (Htu.humidity > 100) { Htu.humidity = 100.0; }
  if ( jgtsf2(Htu.humidity,100)) { Htu.humidity = 100.0; }

  //if (Htu.humidity < 0) { Htu.humidity = 0.01; }
  if (jltsf2(Htu.humidity,0)) { Htu.humidity = 0.01; }

  //if ((0.00 == Htu.humidity) && (0.00 == Htu.temperature)) {
  if ((jeqsf2(0.00,Htu.humidity)) && (jeqsf2(0.00,Htu.temperature))) {
    Htu.humidity = 0.0;
  }
  //if ((Htu.temperature > 0.00) && (Htu.temperature < 80.00)) {
  if ((jgtsf2(Htu.temperature,0)) && (jltsf2(Htu.temperature,80))) {
    //Htu.humidity = (-0.15) * (25 - Htu.temperature) + Htu.humidity;
    Htu.humidity = jfadd( jfmul(-0.15 , jfdiff(25 , Htu.temperature) ) ,Htu.humidity);
  }
  Htu.humidity = ConvertHumidity(Htu.humidity);

  Htu.valid = SENSOR_MAX_MISS;
  return true;
}

/********************************************************************************************/

int32_t MOD_FUNC(HTU_Detect) {
  ALLOCMEM

  Htu.jdelay_humidity = 6;

  Htu.address = HTU21_ADDR;
  //if (I2cActive(Htu.address)) {
  if (!I2cSetDevice(Htu.address)) { 
    CALL_MOD_FUNC(HTU_Deinit);
    return - 1;
  }

  Htu.type = CALL_MOD_FUNC(HtuReadDeviceId);
  if (Htu.type) {
    uint8_t index = 0;
    CALL_MOD_FUNC(HTU_Init);
    switch (Htu.type) {
      case HTU21_CHIPID:
        Htu.jdelay_temp = 50;
        Htu.jdelay_humidity = 16;
        break;
      case SI7021_CHIPID:
        index++;  // 3
      case SI7020_CHIPID:
        index++;  // 2
      case SI7013_CHIPID:
        index++;  // 1
        Htu.jdelay_temp = 12;
        Htu.jdelay_humidity = 23;
        break;
      default:
        index = 4;
        Htu.jdelay_temp = 50;
        Htu.jdelay_humidity = 23;
    }
    GetTextIndexed(Htu.types, sizeof(Htu.types), index, GSTR(kHtuTypes));
    I2cSetActiveFound(Htu.address, Htu.types, 0);
    initialized = true;
  }
  return 0;
}

void MOD_FUNC(HTU_EverySecond) {
  SETREGS
  Htu.cnt++;
  if (Htu.cnt & 1) {  // Every 2 seconds
    // HTU21: 68mS, SI70xx: 37mS
    if (!CALL_MOD_FUNC(HTU_Read)) {
      AddLogMissed(Htu.types, Htu.valid);
    }
  }
}

void MOD_FUNC(HTU_Show, bool json) {
  SETREGS
  if (Htu.valid) {
    TempHumDewShow(json, (0 == GetTasmotaGlobal(1)), Htu.types, Htu.temperature, Htu.humidity);
  }
}

void MOD_FUNC(HTU_Deinit) {
  SETREGS
  I2cResetActive(Htu.address,1);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/


int32_t MOD_FUNC(mod_func_execute, uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = CALL_MOD_FUNC(HTU_Detect);
      break;
    case FUNC_EVERY_SECOND:
      CALL_MOD_FUNC(HTU_EverySecond);
      break;
    case FUNC_JSON_APPEND:
      CALL_MOD_FUNC(HTU_Show, 1);
      break;
    case FUNC_WEB_SENSOR:
      CALL_MOD_FUNC(HTU_Show, 0);
      break;
    case FUNC_DEINIT:
      CALL_MOD_FUNC(HTU_Deinit);
      break;
  }
  return result;
}

#endif  // USE_HTU_MOD
