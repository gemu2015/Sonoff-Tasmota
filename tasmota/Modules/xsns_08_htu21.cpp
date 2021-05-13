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

#include "module.h"

#ifdef USE_HTU_MOD
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


#define HTU_REV  1

// define calls
MODULE_DESCRIPTOR("HTU21",MODULE_TYPE_SENSOR,HTU_REV)
MODULE_PART int32_t HTU_Detect(MODULES_TABLE *mt);
MODULE_PART void HTU_Show(MODULES_TABLE *mt, bool json);
MODULE_PART void HTU_Deinit(MODULES_TABLE *mt);
MODULE_PART uint8_t HtuCheckCrc8(uint16_t data);
MODULE_PART uint8_t HtuReadDeviceId(MODULES_TABLE *mt);
MODULE_PART void HtuSetResolution(MODULES_TABLE *mt, uint8_t resolution);
MODULE_PART void HtuReset(MODULES_TABLE *mt);
MODULE_PART void HtuHeater(MODULES_TABLE *mt, uint8_t heater);
MODULE_PART void HTU_Init(MODULES_TABLE *mt);
MODULE_PART bool HTU_Read(MODULES_TABLE *mt);
MODULE_PART void HTU_EverySecond(MODULES_TABLE *mt);
MODULE_PART int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel);
MODULE_END

// define strings used
DPSTR(kHtuTypes,"HTU21|SI7013|SI7020|SI7021|T/RH?");

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

uint8_t HtuReadDeviceId(MODULES_TABLE *mt) {
  SETREGS

  HtuReset(mt);  // Fixes ESP32 sensor loss at restart

  uint16_t deviceID = 0;
  uint8_t checksum = 0;

  jbeginTransmission(jWire, HTU21_ADDR);
  jwrite(jWire, HTU21_SERIAL2_READ1);
  jwrite(jWire, HTU21_SERIAL2_READ2);
  jendTransmission(jWire, 0);

  jrequestFrom(jWire, HTU21_ADDR, 3);
  deviceID  = jread(jWire) << 8;
  deviceID |= jread(jWire);
  checksum  = jread(jWire);
  if (HtuCheckCrc8(deviceID) == checksum) {
    deviceID = deviceID >> 8;
  } else {
    deviceID = 0;
  }
  return (uint8_t)deviceID;
}

void HtuSetResolution(MODULES_TABLE *mt, uint8_t resolution) {
  SETREGS
  uint8_t current = jI2cRead8(HTU21_ADDR, HTU21_READREG);
  current &= 0x7E;          // Replace current resolution bits with 0
  current |= resolution;    // Add new resolution bits to register
  jI2cWrite8(HTU21_ADDR, HTU21_WRITEREG, current);
}

void HtuReset(MODULES_TABLE *mt) {
  SETREGS
  jbeginTransmission(jWire, HTU21_ADDR);
  jwrite(jWire, HTU21_RESET);
  jendTransmission(jWire, 0);
  jdelay(15);                // Reset takes 15ms
}

void HtuHeater(MODULES_TABLE *mt, uint8_t heater) {
  SETREGS
  uint8_t current = jI2cRead8(HTU21_ADDR, HTU21_READREG);

  switch(heater)
  {
    case HTU21_HEATER_ON  : current |= heater;
                            break;
    case HTU21_HEATER_OFF : current &= heater;
                            break;
    default               : current &= heater;
                            break;
  }
  jI2cWrite8(HTU21_ADDR, HTU21_WRITEREG, current);
}

void HTU_Init(MODULES_TABLE *mt) {
  SETREGS
  HtuReset(mt);
  HtuHeater(mt, HTU21_HEATER_OFF);
  HtuSetResolution(mt, HTU21_RES_RH12_T14);
}

bool HTU_Read(MODULES_TABLE *mt) {
  SETREGS
  uint8_t  checksum = 0;
  uint16_t sensorval = 0;

  if (Htu.valid) { Htu.valid--; }

  jbeginTransmission(jWire, HTU21_ADDR);
  jwrite(jWire, HTU21_READTEMP);
  if (jendTransmission(jWire, 0) != 0) { return false; }           // In case of error
  jdelay(Htu.jdelay_temp);                                       // Sensor time at max resolution

  jrequestFrom(jWire, HTU21_ADDR, 3);
  if (3 == javailable(jWire)) {
    sensorval = jread(jWire) << 8;                              // MSB
    sensorval |= jread(jWire);                                  // LSB
    checksum = jread(jWire);
  }
  if (HtuCheckCrc8(sensorval) != checksum) { return false; }   // Checksum mismatch

  //Htu.temperature = jConvertTemp(0.002681 * (float)sensorval - 46.85);
  Htu.temperature = jConvertTemp(jfscale(sensorval, 0.002681, 46.85));

  jbeginTransmission(jWire, HTU21_ADDR);
  jwrite(jWire, HTU21_READHUM);
  if (jendTransmission(jWire, 0) != 0) { return false; }           // In case of error
  jdelay(Htu.jdelay_humidity);                                   // Sensor time at max resolution

  jrequestFrom(jWire, HTU21_ADDR, 3);
  if (3 <= javailable(jWire)) {
    sensorval = jread(jWire) << 8;                              // MSB
    sensorval |= jread(jWire);                                  // LSB
    checksum = jread(jWire);
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
  Htu.humidity = jConvertHumidity(Htu.humidity);

  Htu.valid = SENSOR_MAX_MISS;
  return true;
}

/********************************************************************************************/

int32_t HTU_Detect(MODULES_TABLE *mt) {
  ALLOCMEM

  Htu.jdelay_humidity = 6;

  Htu.address = HTU21_ADDR;
  if (jI2cActive(Htu.address)) { return - 1; }

  Htu.type = HtuReadDeviceId(mt);
  if (Htu.type) {
    uint8_t index = 0;
    HTU_Init(mt);
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
    jGetTextIndexed(Htu.types, sizeof(Htu.types), index, jPSTR(kHtuTypes));
    jI2cSetActiveFound(Htu.address, Htu.types, 0);
  }
  return 0;
}

void HTU_EverySecond(MODULES_TABLE *mt) {
  SETREGS
  Htu.cnt++;
  if (Htu.cnt & 1) {  // Every 2 seconds
    // HTU21: 68mS, SI70xx: 37mS
    if (!HTU_Read(mt)) {
      jAddLogMissed(Htu.types, Htu.valid);
    }
  }
}

void HTU_Show(MODULES_TABLE *mt, bool json) {
  SETREGS
  if (Htu.valid) {
    jTempHumDewShow(json, (0 == JGetTasmotaGlobal(1)), Htu.types, Htu.temperature, Htu.humidity);
  }
}

void HTU_Deinit(MODULES_TABLE *mt) {
  SETREGS
  jI2cResetActive(Htu.address,1);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/


int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = HTU_Detect(mt);
      break;
    case FUNC_EVERY_SECOND:
      HTU_EverySecond(mt);
      break;
    case FUNC_JSON_APPEND:
      HTU_Show(mt, 1);
      break;
    case FUNC_WEB_SENSOR:
      HTU_Show(mt, 0);
      break;
    case FUNC_DEINIT:
      HTU_Deinit(mt);
      break;
  }
  return result;
}

#endif  // USE_HTU_MOD
