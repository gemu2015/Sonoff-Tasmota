/* mlx90614.cpp - Sensirion SPS30 support for Tasmota
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
#include "tasmota_options.h"
#include <Wire.h>
#include <Stream.h>
#include <HardwareSerial.h>

#ifdef USE_MLX90614


#define MLX90614_REV  1

//__attribute__((section(".text")))

//
extern const FLASH_MODULE module_header = {
  MODULE_SYNC,
  CURR_ARCH,
  MODULE_TYPE_SENSOR,
  MLX90614_REV,
  "MLX90614",
  mod_func_init,
  end_of_module
};


int32_t Init_MLX90614(MODULES_TABLE *mt);
void MLX90614_Show(MODULES_TABLE *mt, uint32_t json);

__attribute__ ((used))
int32_t mod_func_init(MODULES_TABLE *mt, uint32_t sel) {
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = Init_MLX90614(mt);
    case FUNC_JSON_APPEND:
      MLX90614_Show(mt, 1);
      break;
    case FUNC_WEB_SENSOR:
      MLX90614_Show(mt, 0);
      break;
  }
  return result;
}

typedef struct  {
  uint16_t test1;
  uint8_t murks;
} MLX9014_MEMORY;

int32_t Init_MLX90614(MODULES_TABLE *mt) {
  void (* const *jt)() = mt->jt;

  mt->mem_size = sizeof(MLX9014_MEMORY)+8;
  mt->mod_memory = calloc(mt->mem_size/4,4);
  if (!mt->mod_memory) return -1;
  MLX9014_MEMORY *mod_mem = (MLX9014_MEMORY*)mt->mod_memory;
  // now init variables here
  mod_mem->test1 = 999;
  mod_mem->murks = 222;

  // test program
  HardwareSerial *sp = jSerial;
  TwoWire *jw = jWire;
  jw->begin();
  for (uint8_t cnt = 0; cnt < 0x7f; cnt++) {
    jw->beginTransmission(cnt);
    if (!jw->endTransmission()) {
      sp->printf("found %02x\n",cnt );
    }
  }

  sprint((char*)"init ok\n");
  //jSerial.printf("Init OK\n");
  return 0;
}


void MLX90614_Show(MODULES_TABLE *mt, uint32_t json) {
}

__attribute__ ((used))
void  end_of_module(void) {
}



#endif // USE_MLX90614
