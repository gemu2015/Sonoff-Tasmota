/* bmpx.cpp - module test support for Tasmota
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

#ifdef USE_BMP_MOD

#include "module.h"
#include "module_defines.h"

#define BMP_REV  1<<16|0

#define BME280_I2C_ADDRESS1  (0x76)
#define BME280_I2C_ADDRESS2  (0x77)

// Calibration registers.
#define BME280_CAL_T1  (0x88)
#define BME280_CAL_T2  (0x8a)
#define BME280_CAL_T3  (0x8c)
#define BME280_CAL_P1  (0x8e)
#define BME280_CAL_P2  (0x90)
#define BME280_CAL_P3  (0x92)
#define BME280_CAL_P4  (0x94)
#define BME280_CAL_P5  (0x96)
#define BME280_CAL_P6  (0x98)
#define BME280_CAL_P7  (0x9a)
#define BME280_CAL_P8  (0x9c)
#define BME280_CAL_P9  (0x9e)
#define BME280_CAL_H1  (0xa1) /* 8 bits */
#define BME280_CAL_H2  (0xe1)
#define BME280_CAL_H3  (0xe3) /* 8 bits */
#define BME280_CAL_H4  (0xe4) /* 12 bits, combined with H45 */
#define BME280_CAL_H45  (0xe5) /* 12 bits, combined with H5 */
#define BME280_CAL_H5  (0xe6) /* 8 bits */
#define BME280_CAL_H6  (0xe7) /* 8 bits */

// Control registers.
#define BME280_ID_REGISTER  (0xd0) /* 8 bits */
#define BME280_RESET_REGISTER  (0xe0) /* 8 bits */
#define BME280_CTRL_HUM_REGISTER  (0xf2) /* 8 bits */
#define BME280_STATUS_REGISTER  (0xf3) /* 8 bits */
#define BME280_CTRL_MEAS_REGISTER  (0xf4) /* 8 bits */
#define BME280_CONFIG_REGISTER  (0xf5) /* 8 bits */

// Measurement registers.
#define BME280_PRESSURE  (0xf7) /* 20 bits */
#define BME280_PRESSURE_MSB  (0xf7) /* 8 bits */
#define BME280_PRESSURE_LSB  (0xf8) /* 8 bits */
#define BME280_PRESSURE_XLSB  (0xf9) /* 8 bits */
#define BME280_TEMPERATURE  (0xfa) /* 20 bits */
#define BME280_TEMPERATURE_MSB  (0xfa) /* 8 bits */
#define BME280_TEMPERATURE_LSB  (0xfb) /* 8 bits */
#define BME280_TEMPERATURE_XLSB  (0xfc) /* 8 bits */
#define BME280_HUMIDITY  (0xfd) /* 16 bits */
#define BME280_HUMIDITY_MSB  (0xfd) /* 8 bits */
#define BME280_HUMIDITY_LSB  (0xfe) /* 8 bits */

// It is recommended to read all the measurements in one go.
#define BME280_MEASUREMENT_REGISTER  (BME280_PRESSURE)
#define BME280_MEASUREMENT_SIZE  (8)

// Values for osrs_p & osrs_t fields of CTRL_MEAS register.
#define BME280_SKIP  (0)
#define BME280_OVERSAMPLING_1X  (1)
#define BME280_OVERSAMPLING_2X  (2)
#define BME280_OVERSAMPLING_4X  (3)
#define BME280_OVERSAMPLING_8X  (4)
#define BME280_OVERSAMPLING_16X  (5)

// Values for mode field of CTRL_MEAS register.
#define BME280_MODE_SLEEP  (0)
#define BME280_MODE_FORCED  (1)
#define BME280_MODE_NORMAL  (3)

// Value for RESET register.
#define BME280_RESET  (0xb6)

// Value of ID register.
#define BME280_ID  (0x60)

// Values for t_sb field of CONFIG register
#define BME280_STANDBY_500_US  (0)
#define BME280_STANDBY_10_MS  (6)
#define BME280_STANDBY_20_MS  (7)
#define BME280_STANDBY_63_MS  (1)
#define BME280_STANDBY_125_MS  (2)
#define BME280_STANDBY_250_MS  (3)
#define BME280_STANDBY_500_MS  (4)
#define BME280_STANDBY_1000_MS  (5)

// Values for filter field of CONFIG register
#define BME280_FILTER_OFF  (0)
#define BME280_FILTER_COEFF_2  (1)
#define BME280_FILTER_COEFF_4  (2)
#define BME280_FILTER_COEFF_8  (3)
#define BME280_FILTER_COEFF_16  (4)

typedef int32_t temperature_t;
typedef uint32_t pressure_t;
typedef uint32_t humidity_t;

// this is the structure of the module:
// descripotr, code, end
MODULE_DESCRIPTOR("BMPX", MODULE_TYPE_SENSOR, BMP_REV,"",0,"",0,"",0,"",0)

// all functions must be declared MUDULE_PART
MODULE_PART int32_t MOD_FUNC(Init_BMP);
MODULE_PART void MOD_FUNC(BMP_clearCalibrationData);
MODULE_PART void MOD_FUNC(BMP_readCalibrationData);
MODULE_PART void MOD_FUNC(BMP_Show, uint32_t json);
MODULE_PART void MOD_FUNC(BMP_Deinit);
MODULE_PART void MOD_FUNC(BMP_Every_Second);
MODULE_PART int32_t MOD_FUNC(mod_func_execute, uint32_t sel);

MODULE_END

// all memory must be in struct MODULE_MEMORY
typedef struct {
  float hum;
  float temp;
  float press;
  uint8_t i2c_addr;
   // Calibration data.
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

  bool ready;
} MODULE_MEMORY;

#define temp mem->temp
#define hum mem->hum
#define press mem->press
#define i2c_addr mem->i2c_addr

#define _dig_T1 mem->_dig_T1
#define _dig_T2 mem->_dig_T2
#define _dig_T3 mem->_dig_T3

#define _dig_P1 mem->_dig_P1
#define _dig_P2 mem->_dig_P2
#define _dig_P3 mem->_dig_P3
#define _dig_P4 mem->_dig_P4
#define _dig_P5 mem->_dig_P5
#define _dig_P6 mem->_dig_P6
#define _dig_P7 mem->_dig_P7
#define _dig_P8 mem->_dig_P8
#define _dig_P9 mem->_dig_P9

#define _dig_H1 mem->_dig_H1
#define _dig_H2 mem->_dig_H2
#define _dig_H3 mem->_dig_H3
#define _dig_H4 mem->_dig_H4
#define _dig_H5 mem->_dig_H5
#define _dig_H6 mem->_dig_H6

#define ready mem->ready

// all text defines must be here
DPSTR(HTTP_BMP,"{s}BMP TEMP{m}%s C{e} {s}BMP HUM {m}%s %%{e} {s}BMP PRESS {m}%s hp{e}");
DPSTR(JSON_BMP,",\"BMP\":{\"TEMP\":%s,\"HUM\":%s,\"PRESS\":%s}");
DPSTR(bmpdev,"BMPX");


int32_t MOD_FUNC(Init_BMP) {
  ALLOCMEM
 
  // now init variables here
  ready = false;

  i2c_addr = BME280_I2C_ADDRESS1;

  if (!I2cSetDevice(i2c_addr)) {
    CALL_MOD_FUNC(BMP_Deinit);
    return -1;
  }

  I2cSetActiveFound(i2c_addr, PSTR(bmpdev), 0);

  CALL_MOD_FUNC(BMP_clearCalibrationData);
  CALL_MOD_FUNC(BMP_readCalibrationData);
  initialized = true;
  ready = true;
  return ready;
}

uint16_t BMP_Read(uint8_t reg, uint8_t num) {
  beginTransmission(addr);
  write(reg);
  endTransmission(false);
  requestFrom(addr, num);
  uint16_t result = 0;
  result = read();
  if (num == 2) {
    result <<= 8;
    result |= read();
  }
  return result;
}


void MOD_FUNC(BMP_clearCalibrationData) {
  SETREGS
  _dig_T1 = 0;
  _dig_T2 = 0;
  _dig_T3 = 0;
  _dig_P1 = 0;
  _dig_P2 = 0;
  _dig_P3 = 0;
  _dig_P4 = 0;
  _dig_P5 = 0;
  _dig_P6 = 0;
  _dig_P7 = 0;
  _dig_P8 = 0;
  _dig_P9 = 0;
  _dig_H1 = 0;
  _dig_H2 = 0;
  _dig_H3 = 0;
  _dig_H4 = 0;
  _dig_H5 = 0;
  _dig_H6 = 0;
}

i2cRead(_i2c_address,p_data,data_size);

void MOD_FUNC(BMP_readCalibrationData) {
  SETREGS
  _dig_T1 = readUint16(BME280_CAL_T1);
  _dig_T2 = (int16_t) readUint16(BME280_CAL_T2);
  _dig_T3 = (int16_t) readUint16(BME280_CAL_T3);
  _dig_P1 = readUint16(BME280_CAL_P1);
  _dig_P2 = (int16_t) readUint16(BME280_CAL_P2);
  _dig_P3 = (int16_t) readUint16(BME280_CAL_P3);
  _dig_P4 = (int16_t) readUint16(BME280_CAL_P4);
  _dig_P5 = (int16_t) readUint16(BME280_CAL_P5);
  _dig_P6 = (int16_t) readUint16(BME280_CAL_P6);
  _dig_P7 = (int16_t) readUint16(BME280_CAL_P7);
  _dig_P8 = (int16_t) readUint16(BME280_CAL_P8);
  _dig_P9 = (int16_t) readUint16(BME280_CAL_P9);
  _dig_H1 = readUint8(BME280_CAL_H1);
  _dig_H2 = (int16_t) readUint16(BME280_CAL_H2);
  _dig_H3 = readUint8(BME280_CAL_H3);
  // H4 & H5 share a byte.
  uint8_t temp1 = readUint8(BME280_CAL_H4);
  uint8_t temp2 = readUint8(BME280_CAL_H45);
  uint8_t temp3 = readUint8(BME280_CAL_H5);
  _dig_H4 = (temp1<<4) | (temp2&0x0f);
  _dig_H5 = (temp3<<4) | (temp2>>4);
  _dig_H6 = (int8_t) readUint8(BME280_CAL_H6);
}

void MOD_FUNC(BMP_Every_Second) {
  SETREGS

  if (ready == false) return;

  //temp = CALL_MOD_FUNC(MLX90614_GetValue, MLX90614_TOBJ1);
  //temp = CALL_MOD_FUNC(MLX90614_GetValue, MLX90614_TA);

}


void MOD_FUNC(BMP_Show, uint32_t json) {
  SETREGS

  if (ready == false) return;

  SETTINGS *jsettings = mt->settings;

  char temp_tstr[16];
  ftostrfd(temp, jsettings->flag2.temperature_resolution, temp_tstr);
  char hum_tstr[16];
  ftostrfd(hum, jsettings->flag2.humidity_resolution, hum_tstr);
  char press_tstr[16];
  ftostrfd(hum, jsettings->flag2.pressure_resolution, press_tstr);

  if (json) {
    ResponseAppend_P(PSTR(JSON_BMP), temp_tstr, hum_tstr, press_tstr);
  } else {
    WSContentSend_PD(PSTR(HTTP_BMP), temp_tstr, hum_tstr, press_tstr);
  }
}

void MOD_FUNC(BMP_Deinit) {
  SETREGS
  I2cResetActive(i2c_addr, 1);
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
int32_t MOD_FUNC(mod_func_execute, uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = CALL_MOD_FUNC(Init_BMP);
      break;
    case FUNC_JSON_APPEND:
      CALL_MOD_FUNC(BMP_Show, 1);
      break;
    case FUNC_WEB_SENSOR:
      CALL_MOD_FUNC(BMP_Show, 0);
      break;
    case FUNC_EVERY_SECOND:
      CALL_MOD_FUNC(BMP_Every_Second);
      break;
    case FUNC_DEINIT:
      CALL_MOD_FUNC(BMP_Deinit);
      break;
  }
  return result;
}

#endif // USE_BMP_MOD
