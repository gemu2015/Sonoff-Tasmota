/*
  xsns_ltr308.cpp - LTR-308ALS-01 Ambient Light Sensor Plugin for Tasmota

  Copyright (C) 2024  Gerhard Mutz

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

#ifdef USE_LTR308_MOD

#include "module.h"
#include "module_defines.h"

/*********************************************************************************************\
 * LTR-308ALS-01 - Ambient Light Sensor
 *
 * I2C Address: 0x53
 * Range: 0.01 to 157,000 lux
 * Resolution: 16 to 20 bits
 * Lux = (0.6 * ALS_DATA) / (Gain * Integration_Time)
\*********************************************************************************************/

#define LTR308_ADDR         0x53

// Register addresses
#define LTR308_MAIN_CTRL    0x00
#define LTR308_MEAS_RATE    0x04
#define LTR308_ALS_GAIN     0x05
#define LTR308_PART_ID      0x06
#define LTR308_MAIN_STATUS  0x07
#define LTR308_ALS_DATA_0   0x0D  // LSB
#define LTR308_ALS_DATA_1   0x0E
#define LTR308_ALS_DATA_2   0x0F  // MSB
#define LTR308_INT_CFG      0x19
#define LTR308_INT_PST      0x1A

// MAIN_CTRL register bits
#define LTR308_ALS_ENABLE   0x02
#define LTR308_SW_RESET     0x10

// ALS_GAIN values
#define LTR308_GAIN_1X      0x00
#define LTR308_GAIN_3X      0x01
#define LTR308_GAIN_6X      0x02
#define LTR308_GAIN_9X      0x03
#define LTR308_GAIN_18X     0x04

// ALS_MEAS_RATE - Resolution (bits 6:4) and Rate (bits 2:0)
// Resolution: conversion time
#define LTR308_RES_20BIT    (0x00 << 4)  // 400ms
#define LTR308_RES_19BIT    (0x01 << 4)  // 200ms
#define LTR308_RES_18BIT    (0x02 << 4)  // 100ms (default)
#define LTR308_RES_17BIT    (0x03 << 4)  // 50ms
#define LTR308_RES_16BIT    (0x04 << 4)  // 25ms
// Rate
#define LTR308_RATE_25MS    0x00
#define LTR308_RATE_50MS    0x01
#define LTR308_RATE_100MS   0x02  // default
#define LTR308_RATE_500MS   0x03
#define LTR308_RATE_1000MS  0x05
#define LTR308_RATE_2000MS  0x06

// MAIN_STATUS bits
#define LTR308_DATA_STATUS  0x08  // bit 3: new data available
#define LTR308_POWER_ON     0x20  // bit 5: power on status

// Expected PART_ID
#define LTR308_PART_ID_VAL  0xB1

#define LTR308_REV 1 << 16 | 4

PUSH_OPTIONS

MODULE_DESCRIPTOR("LTR308", MODULE_TYPE_SENSOR, LTR308_REV, "", 0, "", 0, "", 0, "", 0)

MODULE_PART int32_t LTR308_Detect();
MODULE_PART void LTR308_Show(bool json);
MODULE_PART void LTR308_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END

typedef struct {
  TWIp *xWire;
  uint8_t address;
  uint8_t gain;
  uint8_t resolution;
  float lux;
  bool valid;
  bool LTR308_detected;
} MODULE_MEMORY;

// Gain multiplier table: 1, 3, 6, 9, 18
const float FP_CONST[] PROGMEM = {0.6, 1.0, 3.0, 6.0, 9.0, 18.0, 0.4, 0.2, 0.1, 0.05, 0.025};
// FP_CONST indices:
// 0 = 0.6   (lux formula numerator factor)
// 1 = 1.0   (gain 1x)
// 2 = 3.0   (gain 3x)
// 3 = 6.0   (gain 6x)
// 4 = 9.0   (gain 9x)
// 5 = 18.0  (gain 18x)
// 6 = 0.4   (integration time 400ms = 20bit)
// 7 = 0.2   (integration time 200ms = 19bit)
// 8 = 0.1   (integration time 100ms = 18bit)
// 9 = 0.05  (integration time 50ms = 17bit)
// 10 = 0.025 (integration time 25ms = 16bit)

const char HTTP_LTR308_LUX[] PROGMEM = "{s}LTR308 Illuminance{m}%s lux{e}";

/*********************************************************************************************\
 * I2C Helper Functions
\*********************************************************************************************/

MODULE_PART bool ltr308_write_reg(uint8_t reg, uint8_t val) {
  SETMEMREGS
  I2C_beginTransmission(mem->address);
  I2C_write(reg);
  I2C_write(val);
  return (I2C_endTransmission(true) == 0);
}

MODULE_PART uint8_t ltr308_read_reg(uint8_t reg) {
  SETMEMREGS
  I2C_beginTransmission(mem->address);
  I2C_write(reg);
  I2C_endTransmission(false);
  I2C_requestFrom(mem->address, 1);
  return I2C_read();
}

MODULE_PART uint32_t ltr308_read_als_data() {
  SETMEMREGS
  I2C_beginTransmission(mem->address);
  I2C_write(LTR308_ALS_DATA_0);
  I2C_endTransmission(false);
  I2C_requestFrom(mem->address, 3);
  uint8_t d0 = I2C_read();
  uint8_t d1 = I2C_read();
  uint8_t d2 = I2C_read();
  return (uint32_t)d0 | ((uint32_t)d1 << 8) | ((uint32_t)(d2 & 0x0F) << 16);
}

/*********************************************************************************************\
 * Get gain multiplier float from gain index
\*********************************************************************************************/

MODULE_PART float ltr308_get_gain_factor() {
  SETMEMREGS
  // gain index 0..4 maps to FP_CONST[1..5]
  uint8_t idx = mem->gain;
  if (idx > 4) idx = 0;
  return FLTC(idx + 1);
}

/*********************************************************************************************\
 * Get integration time in seconds from resolution setting
\*********************************************************************************************/

MODULE_PART float ltr308_get_int_time() {
  SETMEMREGS
  // resolution index 0..4 maps to FP_CONST[6..10]
  uint8_t idx = mem->resolution;
  if (idx > 4) idx = 2;  // default 18bit
  return FLTC(idx + 6);
}

/*********************************************************************************************\
 * Calculate lux: Lux = (0.6 * ALS_DATA) / (Gain * IntegrationTime)
\*********************************************************************************************/

MODULE_PART float ltr308_calc_lux(uint32_t raw) {
  SETMEMREGS
  float numerator = fmul(FLTC(0), tofloat(raw));
  float denominator = fmul(ltr308_get_gain_factor(), ltr308_get_int_time());
  return fdiv(numerator, denominator);
}

/*********************************************************************************************\
 * Read sensor and update lux value
\*********************************************************************************************/

MODULE_PART void ltr308_read_sensor() {
  SETMEMREGS

  if (!mem->LTR308_detected) return;

  uint8_t status = ltr308_read_reg(LTR308_MAIN_STATUS);
  if (status & LTR308_DATA_STATUS) {
    uint32_t raw = ltr308_read_als_data();
    mem->lux = ltr308_calc_lux(raw);
    mem->valid = true;
  }
}

/*********************************************************************************************\
 * Detect and Initialize
\*********************************************************************************************/

int32_t LTR308_Detect() {
  ALLOCMEM

  mem->address = LTR308_ADDR;
  mem->gain = LTR308_GAIN_3X;       // default 3x gain
  mem->resolution = 2;               // 18-bit (index into FP_CONST)
  mem->valid = false;
  mem->lux = tofloat(0);
  mem->LTR308_detected = false;

  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(mem->address, bus)) {
      continue;
    }

    // Check PART_ID
    uint8_t part_id = ltr308_read_reg(LTR308_PART_ID);
    if (part_id != LTR308_PART_ID_VAL) {
      continue;
    }

    // Software reset
    ltr308_write_reg(LTR308_MAIN_CTRL, LTR308_SW_RESET);
    delay(10);

    // Configure: 18-bit resolution, 100ms measurement rate
    ltr308_write_reg(LTR308_MEAS_RATE, LTR308_RES_18BIT | LTR308_RATE_100MS);

    // Configure gain: 3x
    ltr308_write_reg(LTR308_ALS_GAIN, LTR308_GAIN_3X);

    // Enable ALS
    ltr308_write_reg(LTR308_MAIN_CTRL, LTR308_ALS_ENABLE);

    I2C_SetActiveFound(mem->address, "LTR308", 0);

    initialized = true;
    AddLog(LOG_LEVEL_INFO, PSTR("LTR308: Found at 0x%02X, Part ID=0x%02X"), mem->address, part_id);
    mem->LTR308_detected = true;
    return true;
  }

  LTR308_Deinit();
  return false;
}

/*********************************************************************************************\
 * Show sensor data (Web + JSON)
\*********************************************************************************************/

MODULE_PART void LTR308_Show(bool json) {
  SETREGS
  STGLOB

  if (!mem->LTR308_detected) return;

  if (!mem->valid) return;

  char lux_str[32];
  ftostrfd(mem->lux, 1, lux_str);

  if (json) {
    ResponseAppend_P(PSTR(",\"LTR308\":{\"Illuminance\":%s}"), lux_str);
  } else {
    WSContentSend_PD(GSTR(HTTP_LTR308_LUX), lux_str);
  }
}

/*********************************************************************************************\
 * Deinit
\*********************************************************************************************/

void LTR308_Deinit() {
  SETREGS
  // Put sensor to standby
  ltr308_write_reg(LTR308_MAIN_CTRL, 0x00);
  I2C_ResetActive(mem->address, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:
      result = LTR308_Detect();
      break;
    case pFUNC_EVERY_SECOND:
      ltr308_read_sensor();
      break;
    case pFUNC_JSON_APPEND:
      LTR308_Show(1);
      break;
    case pFUNC_WEB_SENSOR:
      LTR308_Show(0);
      break;
    case pFUNC_DEINIT:
      LTR308_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_LTR308_MOD
