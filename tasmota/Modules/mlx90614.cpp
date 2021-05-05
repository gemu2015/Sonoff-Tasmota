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

#ifdef USE_MLX90614

#include "module.h"

#define MLX90614_REV  1


__asm__  (".globl _start");
MODULE_DESCRIPTOR module_header = {
  MODULE_SYNC,
  CURR_ARCH,
  MODULE_TYPE_SENSOR,
  MLX90614_REV,
  "MLX90614",
  mod_func_init,
  end_of_module
};

int32_t Init_MLX90614(struct MODULES_TABLE *mt);
void MLX90614_Show(struct MODULES_TABLE *mt, uint32_t json);

__attribute__ ((used))
static int32_t mod_func_init(struct MODULES_TABLE *mt, uint32_t sel) {
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

int32_t Init_MLX90614(struct MODULES_TABLE *mt) {
  void (* const *jt)() = mt->jt;
  sprint("init ok\n");
  //jSerial.printf("Init OK\n");
  return 0;
}


void MLX90614_Show(struct MODULES_TABLE *mt, uint32_t json) {
}

__attribute__ ((used))
void  end_of_module(void) {
}


void setup(void) {

}
void loop(void) {

}

#endif // USE_MLX90614
