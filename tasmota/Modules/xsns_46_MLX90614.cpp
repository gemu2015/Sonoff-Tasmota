/* mlx90614.cpp - module test support for Tasmota
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




#include "module.h"
#include "module_defines.h"

#ifdef USE_MLX90614_MOD

#define MLX90614_REV  1

//#pragma GCC optimize ("O0")

// this is the structure of the module:
// descripotr, code, end
MODULE_DESCRIPTOR("MLX90614",MODULE_TYPE_SENSOR,MLX90614_REV)

// all functions must be declared MUDULE_PART
MODULE_PART int32_t Init_MLX90614(MODULES_TABLE *mt);
MODULE_PART void MLX90614_Show(MODULES_TABLE *mt, uint32_t json);
MODULE_PART uint16_t MLX90614_read16(MODULES_TABLE *mt, uint8_t addr, uint8_t a);
MODULE_PART uint8_t MLX90614_jcrc8(uint8_t *addr, uint8_t len);
MODULE_PART void MLX90614_Deinit(MODULES_TABLE *mt);
MODULE_PART float MLX90614_GetValue(MODULES_TABLE *mt, uint32_t reg);
MODULE_PART void MLX90614_Every_Second(MODULES_TABLE *mt);
MODULE_PART void MLX90614_Show(MODULES_TABLE *mt, uint32_t json);
MODULE_PART int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel);

MODULE_END


#define I2_ADR_IRT      0x5a

#define MLX90614_RAWIR1 0x04
#define MLX90614_RAWIR2 0x05
#define MLX90614_TA     0x06
#define MLX90614_TOBJ1  0x07
#define MLX90614_TOBJ2  0x08

// all memory must be in struct MODULE_MEMORY
typedef struct {
  float obj_temp;
  float amb_temp;
  bool ready;
} MODULE_MEMORY;

#define obj_temp mem->obj_temp
#define amb_temp mem->amb_temp
#define ready mem->ready

// all text defines must be here
DPSTR(HTTP_IRTMP,"{s}MXL90614 OBJ-TEMP{m}%s C{e} {s}MXL90614 AMB-TEMP {m}%s C{e}");
DPSTR(JSON_IRTMP,",\"MLX90614\":{\"OBJTMP\":%s,\"AMBTMP\":%s}");
DPSTR(mlxdev,"MLX90614");


int32_t Init_MLX90614(MODULES_TABLE *mt) {
  ALLOCMEM

  // now init variables here
  ready = false;

  mt->flags.initialized = true;

  if (!I2cSetDevice(I2_ADR_IRT)) {
    return -1;
  }

  I2cSetActiveFound(I2_ADR_IRT, jPSTR(mlxdev), 0);

  ready = true;

  return ready;
}

void MLX90614_Every_Second(MODULES_TABLE *mt) {
  SETREGS


  if (ready == false) return;

  obj_temp = MLX90614_GetValue(mt, MLX90614_TOBJ1);
  amb_temp = MLX90614_GetValue(mt, MLX90614_TA);

}

float MLX90614_GetValue(MODULES_TABLE *mt, uint32_t reg) {
  SETREGS
  uint16_t val = 0;
  float ret = 0;
  val = MLX90614_read16(mt, I2_ADR_IRT, reg);
  if (val & 0x8000) {
    ret = -999;
  } else {
    ret = fscale(val, (float)0.02, (float)273.15);
    //ret = ((float)val * (float)0.02) - (float)273.15;
  }
  return ret;
}

void MLX90614_Show(MODULES_TABLE *mt, uint32_t json) {
  SETREGS

SETTINGS *jsettings = mt->settings;

  if (ready == false) return;
  char obj_tstr[16];
  ftostrfd(obj_temp, jsettings->flag2.temperature_resolution, obj_tstr);
  char amb_tstr[16];
  ftostrfd(amb_temp, jsettings->flag2.temperature_resolution, amb_tstr);

  if (json) {
    ResponseAppend_P(jPSTR(JSON_IRTMP), obj_tstr, amb_tstr);
  } else {
    WSContentSend_PD(jPSTR(HTTP_IRTMP), obj_tstr, amb_tstr);
  }
}

uint16_t MLX90614_read16(MODULES_TABLE *mt, uint8_t addr, uint8_t a) {
  SETREGS
  uint16_t ret;

  beginTransmission(addr);
  write(a);
  endTransmission(false);

  requestFrom(addr, (size_t)3);
  uint8_t buff[5];
  buff[0] = addr << 1;
  buff[1] = a;
  buff[2] = (addr << 1) | 1;
  buff[3] = read();
  buff[4] = read();
  ret = buff[3] | (buff[4] << 8);
  uint8_t pec = read();

  return ret;

  uint8_t cpec = MLX90614_jcrc8(buff, sizeof(buff));
  //AddLog(LOG_LEVEL_INFO,PSTR("%x - %x"),pec, cpec);

  if (pec != cpec) {
    //jAddLog(LOG_LEVEL_INFO,PSTR("mlx checksum error"));
  }
  return ret;
}


uint8_t MLX90614_jcrc8(uint8_t *addr, uint8_t len) {
// The PEC calculation includes all bits except the START, REPEATED START, STOP,
// ACK, and NACK bits. The PEC is a CRC-8 with polynomial X8+X2+X1+1.
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t carry = (crc ^ inbyte) & 0x80;
      crc <<= 1;
      if (carry)
        crc ^= 0x7;
      inbyte <<= 1;
    }
  }
  return crc;
}

void MLX90614_Deinit(MODULES_TABLE *mt) {
  SETREGS
  I2cResetActive(I2_ADR_IRT,1);
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = Init_MLX90614(mt);
      break;
    case FUNC_JSON_APPEND:
      MLX90614_Show(mt, 1);
      break;
    case FUNC_WEB_SENSOR:
      MLX90614_Show(mt, 0);
      break;
    case FUNC_EVERY_SECOND:
      MLX90614_Every_Second(mt);
      break;
    case FUNC_DEINIT:
      MLX90614_Deinit(mt);
      break;
  }
  return result;
}

#endif // USE_MLX90614
