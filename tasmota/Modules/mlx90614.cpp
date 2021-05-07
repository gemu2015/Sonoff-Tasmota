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

/*




*/
#define USE_MLX90614_MOD

#include "module.h"
#include "tasmota_options.h"
#include <Wire.h>
#include <Stream.h>
#include <HardwareSerial.h>

#ifdef USE_MLX90614_MOD

#define MLX90614_REV  1

//void __REDIRECT (memcpy, (void *dest, const void *src, size_t n), CopyMem);

/*
__asm__ __volatile__ ("__floatunsisf:");
__asm__ __volatile__ ("__mulsf3:");
__asm__ __volatile__ ("__subsf3:");
__asm__ __volatile__ ("__extendsfdf2:");
*/


#pragma OPTIMIZE OFF

MODULE_DESC module_header = {
  MODULE_SYNC,
  CURR_ARCH,
  MODULE_TYPE_SENSOR,
  MLX90614_REV,
  "MLX90614",
  mod_func_execute,
  end_of_module
};

#define I2_ADR_IRT      0x5a

#define MLX90614_RAWIR1 0x04
#define MLX90614_RAWIR2 0x05
#define MLX90614_TA     0x06
#define MLX90614_TOBJ1  0x07
#define MLX90614_TOBJ2  0x08

typedef struct {
  union {
    uint16_t value;
    uint32_t i2c_buf;
    };
  float obj_temp;
  float amb_temp;
  bool ready;
} MLX9014_MEMORY;


int32_t Init_MLX90614(MODULES_TABLE *mt);
void MLX90614_Show(MODULES_TABLE *mt, uint32_t json);
uint16_t MLX90614_read16(MODULES_TABLE *mt, uint8_t addr, uint8_t a);
uint8_t MLX90614_jcrc8(uint8_t *addr, uint8_t len);

DEFSTR(initmsg:,"Hello world\n");
EXTSTR(initmsg);
DEFSTR(mlxdev:,"MLX90614");
EXTSTR(mlxdev);
DEFSTR(HTTP_IRTMP:,"{s}MXL90614 OBJ-TEMP{m}%s C{e} {s}MXL90614 AMB-TEMP {m}%s C{e}");
EXTSTR(HTTP_IRTMP);
DEFSTR(JSON_IRTMP:,",\"MLX90614\":{\"OBJTMP\":%s,\"AMBTMP\":%s}");
EXTSTR(JSON_IRTMP);


MODULE_PART int32_t Init_MLX90614(MODULES_TABLE *mt) {
  ALLOCMEM(MLX9014_MEMORY)

  // now init variables here
  mod_mem->ready = false;

  jsettings->temperature_resolution = 2;

  mt->flags.initialized = true;

  //HardwareSerial *sp = jSerial;
  //sp->printf_P(GSTR(initmsg));
  //sprint(GSTR(initmsg));

  if (!jI2cSetDevice(I2_ADR_IRT)) {
    return -1;
  }
  jI2cSetActiveFound(I2_ADR_IRT, GSTR(mlxdev), 0);


//    uint32_t state;
  //  RSIL(state);
//    return state;

//    WSR_PS(state);


  mod_mem->ready = true;

  return 0;
}

MODULE_PART void MLX90614_Deinit(MODULES_TABLE *mt) {
  if (mt->mem_size) {
    free(mt->mod_memory);
    mt->mem_size = 0;
  }
}

MODULE_END void  end_of_module(void) {
}

float MLX90614_GetValue(MODULES_TABLE *mt, uint32_t reg);

MODULE_PART void MLX90614_Every_Second(MODULES_TABLE *mt) {
  SETREGS

  // test message
  //HardwareSerial *sp = jSerial;
  //sp->printf_P(GSTR(initmsg));
  sprint(GSTR(initmsg));

  if (mod_mem->ready == false) return;

  mod_mem->obj_temp = MLX90614_GetValue(mt, MLX90614_TOBJ1);
  mod_mem->amb_temp = MLX90614_GetValue(mt, MLX90614_TA);

}

MODULE_PART float MLX90614_GetValue(MODULES_TABLE *mt, uint32_t reg) {
  SETREGS
  uint16_t val = 0;
  float ret = 0;
  val = MLX90614_read16(mt, I2_ADR_IRT, reg);
  if (val & 0x8000) {
    ret = -999;
  } else {
    ret = jfscale(val, (float)0.02, (float)273.15);
    //ret = ((float)val * (float)0.02) - (float)273.15;
  }
  return ret;
}

MODULE_PART void MLX90614_Show(MODULES_TABLE *mt, uint32_t json) {
  SETREGS

  mySettings *jsettings = mt->settings;

  if (mod_mem->ready == false) return;
  char obj_tstr[16];
  jftostrfd(mod_mem->obj_temp, jsettings->temperature_resolution, obj_tstr);
  char amb_tstr[16];
  jftostrfd(mod_mem->amb_temp, jsettings->temperature_resolution, amb_tstr);

  if (json) {
    jResponseAppend_P(GSTR(JSON_IRTMP), obj_tstr, amb_tstr);
  } else {
    jWSContentSend_PD(GSTR(HTTP_IRTMP), obj_tstr, amb_tstr);
  }
}

MODULE_PART uint16_t MLX90614_read16(MODULES_TABLE *mt, uint8_t addr, uint8_t a) {
  SETREGS
  uint16_t ret;

  jbeginTransmission(jWire,addr);
  jwrite(jWire,a);
  jendTransmission(jWire,false);

  jrequestFrom(jWire, addr, (size_t)3);
  uint8_t buff[5];
  buff[0] = addr << 1;
  buff[1] = a;
  buff[2] = (addr << 1) | 1;
  buff[3] = jread(jWire);
  buff[4] = jread(jWire);
  ret = buff[3] | (buff[4] << 8);
  uint8_t pec = jread(jWire);

  return ret;

  uint8_t cpec = MLX90614_jcrc8(buff, sizeof(buff));
  //AddLog(LOG_LEVEL_INFO,PSTR("%x - %x"),pec, cpec);

  if (pec != cpec) {
    //jAddLog(LOG_LEVEL_INFO,PSTR("mlx checksum error"));
  }
  return ret;
}


MODULE_PART uint8_t MLX90614_jcrc8(uint8_t *addr, uint8_t len) {
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

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

MODULE_PART int32_t mod_func_execute(MODULES_TABLE *mt, uint32_t sel) {
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

#pragma OPTIMIZE ON
#endif // USE_MLX90614
