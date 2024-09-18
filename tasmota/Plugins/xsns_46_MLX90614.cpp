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


#include "tasmota_options.h" 

#ifdef USE_MLX90614_MOD

#include "module.h"
#include "module_defines.h"

#define MLX90614_REV  1 << 16 | 4

PUSH_OPTIONS

// this is the structure of the module:
// descripotr, code, end
MODULE_DESCRIPTOR("MLX90614", MODULE_TYPE_SENSOR, MLX90614_REV,"",0,"",0,"",0,"",0)

// all functions must be declared MUDULE_PART
MODULE_PART int32_t Init_MLX90614();
MODULE_PART uint16_t MLX90614_read16(uint8_t addr, uint8_t a);
MODULE_PART uint8_t MLX90614_jcrc8(uint8_t *addr, uint8_t len);
MODULE_PART void MLX90614_Deinit();
MODULE_PART float MLX90614_GetValue(uint32_t reg);
MODULE_PART void MLX90614_Every_Second();
MODULE_PART void MLX90614_Show(uint32_t json);
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);

MODULE_END

#define I2_ADR_IRT      0x5a

#define MLX90614_RAWIR1 0x04
#define MLX90614_RAWIR2 0x05
#define MLX90614_TA     0x06
#define MLX90614_TOBJ1  0x07
#define MLX90614_TOBJ2  0x08


// all memory must be in struct MODULE_MEMORY
typedef struct {
  TwoWire *xWire;
  float obj_temp;
  float amb_temp;
  bool ready;
  STRBUFFER
} MODULE_MEMORY;

// ease memory objects
#define obj_temp mem->obj_temp
#define amb_temp mem->amb_temp
#define ready mem->ready

// text defines
const char HTTP_IRTMP[] PROGMEM = "{s}MXL90614 OBJ-TEMP{m}%s C{e} {s}MXL90614 AMB-TEMP {m}%s C{e}";
const char JSON_IRTMP[] PROGMEM = ",\"MLX90614\":{\"OBJTMP\":%s,\"AMBTMP\":%s}";
const char mlxdev[] PROGMEM = "MLX90614";


int32_t Init_MLX90614() {
  ALLOCMEM 

  I2C_SETWIRE(0);
 
  // now init variables here
  ready = false;

  if (!I2C_SetDevice(I2_ADR_IRT, 0)) {
    MLX90614_Deinit();
    return -1;
  }
 // char *cp = copyStr(GSTR(mlxdev));
  I2C_SetActiveFound(I2_ADR_IRT, GSTR(mlxdev), 0);
 // free(cp);
  initialized = true;
  ready = true;
  return ready;
}

void MLX90614_Every_Second() {
  SETREGS

  if (ready == false) return;

  obj_temp = MLX90614_GetValue(MLX90614_TOBJ1);
  amb_temp = MLX90614_GetValue(MLX90614_TA);

}

// all float constants must be in progmem
const float FP_CONST[] PROGMEM = {-999, 0.02, 273.15};

float MLX90614_GetValue(uint32_t reg) {
  SETREGS
  uint16_t val = 0;
  float ret = 0;
  val = MLX90614_read16(I2_ADR_IRT, reg);
  if (val & 0x8000) {
    ret = FLTC(0);
  } else {
    //ret = fscale(val, (float)0.02, (float)273.15);
    //ret = ((float)val * (float)0.02) - (float)273.15;
    ret = fscale(val, FLTC(1), FLTC(2));
  }
  return ret;
}

void MLX90614_Show(uint32_t json) {
  SETREGS

  if (ready == false) return;
  char obj_tstr[16];
  ftostrfd(obj_temp, Settings->flag2.temperature_resolution, obj_tstr);
  char amb_tstr[16];
  ftostrfd(amb_temp, Settings->flag2.temperature_resolution, amb_tstr);
  if (json) {
    ResponseAppend_P(GSTR(JSON_IRTMP), obj_tstr, amb_tstr);
  } else {
    WSContentSend_PD(GSTR(HTTP_IRTMP), obj_tstr, amb_tstr);
  }
}

uint16_t MLX90614_read16(uint8_t addr, uint8_t a) {
  SETREGS
  uint16_t ret;

  I2C_beginTransmission(addr);
  I2C_write(a);
  I2C_endTransmission(false);

  I2C_requestFrom(addr, (size_t)3);
  uint8_t buff[5];
  buff[0] = addr << 1;
  buff[1] = a;
  buff[2] = (addr << 1) | 1;
  buff[3] = I2C_read();
  buff[4] = I2C_read();
  ret = buff[3] | (buff[4] << 8);
  uint8_t pec = I2C_read();

  return ret;

  uint8_t cpec = MLX90614_jcrc8(buff, sizeof(buff));
  //AddLog(LOG_LEVEL_INFO,PSTR("%x - %x"),pec, cpec);

  if (pec != cpec) {
    AddLog(LOG_LEVEL_INFO,PSTR("mlx checksum error"));
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

void MLX90614_Deinit() {
  SETREGS
  I2C_ResetActive(I2_ADR_IRT, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
MOD_RESULT mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:
      result = Init_MLX90614();
      break;
    case pFUNC_JSON_APPEND:
      MLX90614_Show(1);
      break;
    case pFUNC_WEB_SENSOR:
      MLX90614_Show(0);
      break;
    case pFUNC_EVERY_SECOND:
      MLX90614_Every_Second();
      break;
    case pFUNC_DEINIT:
      MLX90614_Deinit();
      break;
  }
  return result; 
}

PULL_OPTIONS
#endif // USE_MLX90614
