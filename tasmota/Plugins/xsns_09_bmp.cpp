/* BMEx.cpp - module test support for Tasmota
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

#ifdef USE_BME_MOD

#include "module.h"
#include "module_defines.h"

#define BMX_REV 1 << 16 | 3

#define BME280_I2C_ADDRESS1 (0x76)
#define BME280_I2C_ADDRESS2 (0x77)

// Calibration registers.
#define BME280_CAL_T1 (0x88)
#define BME280_CAL_T2 (0x8a)
#define BME280_CAL_T3 (0x8c)
#define BME280_CAL_P1 (0x8e)
#define BME280_CAL_P2 (0x90)
#define BME280_CAL_P3 (0x92)
#define BME280_CAL_P4 (0x94)
#define BME280_CAL_P5 (0x96)
#define BME280_CAL_P6 (0x98)
#define BME280_CAL_P7 (0x9a)
#define BME280_CAL_P8 (0x9c)
#define BME280_CAL_P9 (0x9e)
#define BME280_CAL_H1 (0xa1) /* 8 bits */
#define BME280_CAL_H2 (0xe1)
#define BME280_CAL_H3 (0xe3)  /* 8 bits */
#define BME280_CAL_H4 (0xe4)  /* 12 bits, combined with H45 */
#define BME280_CAL_H45 (0xe5) /* 12 bits, combined with H5 */
#define BME280_CAL_H5 (0xe6)  /* 8 bits */
#define BME280_CAL_H6 (0xe7)  /* 8 bits */

// Control registers.
#define BME280_ID_REGISTER (0xd0)        /* 8 bits */
#define BME280_RESET_REGISTER (0xe0)     /* 8 bits */
#define BME280_CTRL_HUM_REGISTER (0xf2)  /* 8 bits */
#define BME280_STATUS_REGISTER (0xf3)    /* 8 bits */
#define BME280_CTRL_MEAS_REGISTER (0xf4) /* 8 bits */
#define BME280_CONFIG_REGISTER (0xf5)    /* 8 bits */

// Measurement registers.
#define BME280_PRESSURE (0xf7)         /* 20 bits */
#define BME280_PRESSURE_MSB (0xf7)     /* 8 bits */
#define BME280_PRESSURE_LSB (0xf8)     /* 8 bits */
#define BME280_PRESSURE_XLSB (0xf9)    /* 8 bits */
#define BME280_TEMPERATURE (0xfa)      /* 20 bits */
#define BME280_TEMPERATURE_MSB (0xfa)  /* 8 bits */
#define BME280_TEMPERATURE_LSB (0xfb)  /* 8 bits */
#define BME280_TEMPERATURE_XLSB (0xfc) /* 8 bits */
#define BME280_HUMIDITY (0xfd)         /* 16 bits */
#define BME280_HUMIDITY_MSB (0xfd)     /* 8 bits */
#define BME280_HUMIDITY_LSB (0xfe)     /* 8 bits */

// It is recommended to read all the measurements in one go.
#define BME280_MEASUREMENT_REGISTER (BME280_PRESSURE)
#define BME280_MEASUREMENT_SIZE (8)

// Values for osrs_p & osrs_t fields of CTRL_MEAS register.
#define BME280_SKIP (0)
#define BME280_OVERSAMPLING_1X (1)
#define BME280_OVERSAMPLING_2X (2)
#define BME280_OVERSAMPLING_4X (3)
#define BME280_OVERSAMPLING_8X (4)
#define BME280_OVERSAMPLING_16X (5)

// Values for mode field of CTRL_MEAS register.
#define BME280_MODE_SLEEP (0)
#define BME280_MODE_FORCED (1)
#define BME280_MODE_NORMAL (3)

// Value for RESET register.
#define BME280_RESET (0xb6)

// Value of ID register.
#define BME280_ID (0x60)

#define BMP180_CHIPID 0x55
#define BMP280_CHIPID 0x58
#define BME280_CHIPID 0x60
#define BME680_CHIPID 0x61

// Values for t_sb field of CONFIG register
#define BME280_STANDBY_500_US (0)
#define BME280_STANDBY_10_MS (6)
#define BME280_STANDBY_20_MS (7)
#define BME280_STANDBY_63_MS (1)
#define BME280_STANDBY_125_MS (2)
#define BME280_STANDBY_250_MS (3)
#define BME280_STANDBY_500_MS (4)
#define BME280_STANDBY_1000_MS (5)

// Values for filter field of CONFIG register
#define BME280_FILTER_OFF (0)
#define BME280_FILTER_COEFF_2 (1)
#define BME280_FILTER_COEFF_4 (2)
#define BME280_FILTER_COEFF_8 (3)
#define BME280_FILTER_COEFF_16 (4)

typedef int32_t temperature_t;
typedef uint32_t pressure_t;
typedef uint32_t humidity_t;

PUSH_OPTIONS

// this is the structure of the module:
// descripotr, code, end
MODULE_DESCRIPTOR("BMXx80", MODULE_TYPE_SENSOR, BMX_REV, "", 0, "", 0, "", 0, "", 0)

// all functions must be declared MUDULE_PART
MODULE_PART int32_t Init_BME();
MODULE_PART void BME_clearCalibrationData();
MODULE_PART void BME_readCalibrationData();
MODULE_PART uint32_t BME_Read(uint8_t reg, int8_t num);
MODULE_PART uint32_t BME_Write(uint8_t reg, int8_t val);
MODULE_PART void BME_Show(uint32_t json);
MODULE_PART void BME_Deinit();
MODULE_PART void BME_Every_Second();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_PART humidity_t compensateHumidity(int32_t adc_H);
MODULE_PART temperature_t compensateTemperature(int32_t adc_T);
MODULE_PART pressure_t compensatePressure(int32_t adc_P);

MODULE_END

typedef struct {
  uint16_t _dig_T1;
  int16_t _dig_T2;
  int16_t _dig_T3;
  uint16_t _dig_P1;
  int16_t _dig_P2;
  int16_t _dig_P3;
  int16_t _dig_P4;
  int16_t _dig_P5;
  int16_t _dig_P6;
  int16_t _dig_P7;
  int16_t _dig_P8;
  int16_t _dig_P9;
  uint8_t _dig_H1;
  int16_t _dig_H2;
  uint8_t _dig_H3;
  int16_t _dig_H4;
  int16_t _dig_H5;
  int8_t _dig_H6;
} BMECAL;

// all memory must be in struct MODULE_MEMORY
typedef struct {
  TwoWire *xWire;
  float hum;
  float abshum;
  float temp;
  int32_t _t_fine;
  float press;
  uint8_t type;
  uint8_t i2c_addr;
  // Calibration data.
  BMECAL bmc;
  char typestr[8];
  bool ready;
} MODULE_MEMORY;

#define temp mem->temp
#define _t_fine mem->_t_fine
#define hum mem->hum
#define press mem->press
#define type mem->type
#define typestr mem->typestr
#define abshum mem->abshum
#define i2c_addr mem->i2c_addr
#define bmc mem->bmc

#define ready mem->ready

// all text defines must be here
const char HTTP_BMP_T[] PROGMEM = "{s}%s Temperatur{m}%s C{e}";
const char HTTP_BMP_P[] PROGMEM = "{s}%s Luftdruck {m}%s hp{e}";
const char JSON_BMP[] PROGMEM = ",\"%s\":{\"Temperature\":%s,\"Pressure\":%s";
const char JSON_BME[] PROGMEM = ",\"Humidity\":%s,\"AbsHumidity\":%s}";
const char JSON_BMPend[] PROGMEM = "}";
const char HTTP_SNS_AHUM[] PROGMEM = "{s}%s Abs Humidity{m}%s g/m3{e}";
const char BMEtypes[] PROGMEM = "BMP180|BME280|BMP280|BME680";

int32_t Init_BME() {
  ALLOCMEM

  SETWIRE(0);

  // now init variables here
  ready = false;

  i2c_addr = BME280_I2C_ADDRESS1;

  if (!I2cSetDevice(i2c_addr, 0)) {
    BME_Deinit();
    return -1;
  }

  type = BME_Read(BME280_ID_REGISTER, 1);

  uint8_t index = 0;
  if (type == BMP180_CHIPID) {
    index = 0;
  } else if (type == BME280_CHIPID) {
    index = 1;
  } else if (type == BMP280_CHIPID) {
    index = 2;
  } else if (type == BME680_CHIPID) {
    index = 3;
  }

  GetTextIndexed(typestr, sizeof(typestr), index, GSTR(BMEtypes));
  I2cSetActiveFound(i2c_addr, typestr, 0);

  BME_readCalibrationData();

  initialized = true;
  ready = true;
  return ready;
}

uint32_t BME_Read(uint8_t reg, int8_t num) {
  SETREGS

  beginTransmission(i2c_addr);
  write(reg);
  endTransmission(false);
  requestFrom(i2c_addr, abs(num));
  uint32_t result = 0;
  if (num > 0) {
    for (uint16_t cnt = 0; cnt < num; cnt++) {
      result <<= 8;
      result |= read();
    }
  } else {
    result = read();
    result |= read() << 8;
  }
  return result;
}

uint32_t BME_Write(uint8_t reg, int8_t val) {
  SETREGS

  beginTransmission(i2c_addr);
  write(reg);
  write(val);
  return endTransmission(true);
}

void BME_readCalibrationData() {
  SETREGS
  bmc._dig_T1 = (uint16_t)BME_Read(BME280_CAL_T1, -2);
  bmc._dig_T2 = (int16_t)BME_Read(BME280_CAL_T2, -2);
  bmc._dig_T3 = (int16_t)BME_Read(BME280_CAL_T3, -2);

  bmc._dig_P1 = (uint16_t)BME_Read(BME280_CAL_P1, -2);
  bmc._dig_P2 = (int16_t)BME_Read(BME280_CAL_P2, -2);
  bmc._dig_P3 = (int16_t)BME_Read(BME280_CAL_P3, -2);
  bmc._dig_P4 = (int16_t)BME_Read(BME280_CAL_P4, -2);
  bmc._dig_P5 = (int16_t)BME_Read(BME280_CAL_P5, -2);
  bmc._dig_P6 = (int16_t)BME_Read(BME280_CAL_P6, -2);
  bmc._dig_P7 = (int16_t)BME_Read(BME280_CAL_P7, -2);
  bmc._dig_P8 = (int16_t)BME_Read(BME280_CAL_P8, -2);
  bmc._dig_P9 = (int16_t)BME_Read(BME280_CAL_P9, -2);

  if (type == BME280_CHIPID) {
    bmc._dig_H1 = BME_Read(BME280_CAL_H1, 1);
    bmc._dig_H2 = (int16_t)BME_Read(BME280_CAL_H2, -2);
    bmc._dig_H3 = BME_Read(BME280_CAL_H3, 1);
    // H4 & H5 share a byte.
    uint8_t temp1 = BME_Read(BME280_CAL_H4, 1);
    uint8_t temp2 = BME_Read(BME280_CAL_H45, 1);
    uint8_t temp3 = BME_Read(BME280_CAL_H5, 1);
    bmc._dig_H4 = (temp1 << 4) | (temp2 & 0x0f);
    bmc._dig_H5 = (temp3 << 4) | (temp2 >> 4);
    bmc._dig_H6 = (int8_t)BME_Read(BME280_CAL_H6, 1);

    BME_Write(BME280_CTRL_MEAS_REGISTER, 0x00);
    BME_Write(BME280_CTRL_HUM_REGISTER, 0x01);
    BME_Write(BME280_CONFIG_REGISTER, 0xA0);
    BME_Write(BME280_CTRL_MEAS_REGISTER, 0x27);
  } else {
    BME_Write(BME280_CTRL_MEAS_REGISTER, 0xb7);
  }
}

// From the datasheet.
// Returns temperature in DegC, resolution is 0.01 DegC. Output value of 5123 equals 51.23 DegC.
// _t_fine carries fine temperature as "global" value.
temperature_t compensateTemperature(int32_t adc_T) {
  SETREGS
  int32_t var1, var2, T;
  var1 = ((((adc_T >> 3) - ((int32_t)bmc._dig_T1 << 1))) * ((int32_t)bmc._dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)bmc._dig_T1)) * ((adc_T >> 4) - ((int32_t)bmc._dig_T1))) >> 12) *
          ((int32_t)bmc._dig_T3)) >>
         14;
  _t_fine = var1 + var2;
  T = (_t_fine * 5 + 128) >> 8;
  return T;
}

// From the datasheet.
// Returns pressure in Pa as unsigned 32 bit integer. Output value of 96386 equals 96386 Pa = 963.86 hPa
pressure_t compensatePressure(int32_t adc_P) {
  SETREGS
  int32_t var1, var2;
  uint32_t p;
  var1 = (((int32_t)_t_fine) >> 1) - (int32_t)ICONST(64000);
  var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)bmc._dig_P6);
  var2 = var2 + ((var1 * ((int32_t)bmc._dig_P5)) << 1);
  var2 = (var2 >> 2) + (((int32_t)bmc._dig_P4) << 16);
  var1 = (((bmc._dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)bmc._dig_P2) * var1) >> 1)) >> 18;
  var1 = ((((ICONST(32768) + var1)) * ((int32_t)bmc._dig_P1)) >> 15);
  if (var1 == 0) {
    return 0;  // avoid exception caused by division by zero
  }
  p = (((uint32_t)(((int32_t)ICONST(1048576)) - adc_P) - (var2 >> 12))) * ICONST(3125);
  if (p < ICONST(0x80000000)) {
    // p = (p << 1) / ((uint32_t)var1);
    p = tmod__udivsi3((p << 1), (uint32_t)var1);
  } else {
    // p = (p / (uint32_t)var1) * 2;
    p = tmod__udivsi3(p, (uint32_t)var1) * 2;
  }
  var1 = (((int32_t)bmc._dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
  var2 = (((int32_t)(p >> 2)) * ((int32_t)bmc._dig_P8)) >> 13;
  p = (uint32_t)((int32_t)p + ((var1 + var2 + bmc._dig_P7) >> 4));
  return p;
}

// From the datasheet.
// Returns humidity in %RH as unsigned 32 bit integer in Q22.10 format (22 integer and 10 fractional bits).
// Output value of 47445 represents 47445/1024 = 46.333 %RH
humidity_t compensateHumidity(int32_t adc_H) {
  SETREGS
  int32_t v_x1_u32r;
  v_x1_u32r = (_t_fine - ((int32_t)ICONST(76800)));
  v_x1_u32r =
      (((((adc_H << 14) - (((int32_t)bmc._dig_H4) << 20) - (((int32_t)bmc._dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >>
        15) *
       (((((((v_x1_u32r * ((int32_t)bmc._dig_H6)) >> 10) *
            (((v_x1_u32r * ((int32_t)bmc._dig_H3)) >> 11) + ((int32_t)ICONST(32768)))) >>
           10) +
          ((int32_t)ICONST(2097152))) *
             ((int32_t)bmc._dig_H2) +
         ICONST(8192)) >>
        14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)bmc._dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > ICONST(419430400) ? ICONST(419430400) : v_x1_u32r);
  return (uint32_t)(v_x1_u32r >> 12);
}

// all float constants must be in progmem
const float FP_CONST[] PROGMEM = {0, 0.01, 0.00097656};

void BME_Every_Second() {
  SETREGS

  if (ready == false) return;

  uint32_t r_press = BME_Read(BME280_PRESSURE, 3) >> 4;
  int32_t r_temp = BME_Read(BME280_TEMPERATURE, 3) >> 4;

  r_temp = compensateTemperature(r_temp);  // First call this before calling the other compensate functions.
  r_press = compensatePressure(r_press);   // Uses value calculated by compensateTemperature.
  temp = fscale(r_temp, FLTC(1), FLTC(0));
  press = fscale(r_press, FLTC(1), FLTC(0));

  if (type == BME280_CHIPID) {
    uint16_t r_hum = BME_Read(BME280_HUMIDITY, 2);
    r_hum = compensateHumidity(r_hum);  // Uses value calculated by compensateTemperature.
    hum = fscale(r_hum, FLTC(2), FLTC(0));
    abshum = CalcTempHumToAbsHum(temp, hum);
  }
}

void BME_Show(uint32_t json) {
  SETREGS

  if (ready == false) return;

  char temp_tstr[16];
  ftostrfd(temp, Settings->flag2.temperature_resolution, temp_tstr);
  char press_tstr[16];
  ftostrfd(press, Settings->flag2.pressure_resolution, press_tstr);
  char hum_tstr[16];
  ftostrfd(hum, Settings->flag2.humidity_resolution, hum_tstr);
  char ahum_tstr[16];
  ftostrfd(abshum, 4, ahum_tstr);

  if (json) {
    ResponseAppend_P(GSTR(JSON_BMP), typestr, temp_tstr, press_tstr);
    if (type == BME280_CHIPID) {
      ResponseAppend_P(GSTR(JSON_BME), hum_tstr, ahum_tstr);
    } else {
      ResponseAppend_P(GSTR(JSON_BMPend));
    }
  } else {
    if (type == BME280_CHIPID) {
      TempHumDewShow(json, 0, typestr, temp, hum);
      WSContentSend_PD(GSTR(HTTP_SNS_AHUM), typestr, ahum_tstr);
    } else {
      WSContentSend_PD(GSTR(HTTP_BMP_T), typestr, temp_tstr);
    }
    WSContentSend_PD(GSTR(HTTP_BMP_P), typestr, press_tstr);
  }
}

void BME_Deinit() {
  SETREGS
  I2cResetActive(i2c_addr, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = Init_BME();
      break;
    case FUNC_JSON_APPEND:
      BME_Show(1);
      break;
    case FUNC_WEB_SENSOR:
      BME_Show(0);
      break;
    case FUNC_EVERY_SECOND:
      BME_Every_Second();
      break;
    case FUNC_DEINIT:
      BME_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_BME_MOD
