/*
  testmodule.c - Prove of concept for flash modules

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

#ifdef ESP32

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <Wire.h>
#include <Stream.h>
#include <HardwareSerial.h>
#include <ext_printf.h>
#include <esp32-hal.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "Stream.h"

enum {MODULE_TYPE_SENSOR, MODULE_TYPE_LIGHT, MODULE_TYPE_ENERGY};

typedef struct {
  uint16_t sync;
  uint16_t type;
  uint16_t revision;
  uint32_t size;
  int32_t (*mod_func_init)(struct MODULES_TABLE *);
  void (*mod_func_web_sensor)(struct MODULES_TABLE *);
  void (*mod_func_json_append)(struct MODULES_TABLE *);
  void (*mod_func_every_Ssecond)(struct MODULES_TABLE *);
} FLASH_MODULE;

struct MODULES_TABLE {
  const FLASH_MODULE *mod_addr;
  void (* const *jt)();
  void *mod_memory;
  uint16_t mem_size;
};

static int32_t mod_func_init(struct MODULES_TABLE *);
static void mod_func_web_sensor(struct MODULES_TABLE *);
static void mod_func_json_append(struct MODULES_TABLE *);
static void mod_func_every_second(struct MODULES_TABLE *);
static void end_of_module(void);


// example module
__attribute__((section(".text")))
const FLASH_MODULE module_header = {
  0x4AFC,
  MODULE_TYPE_SENSOR,
  0,
  (uint32_t)end_of_module - (uint32_t)mod_func_init + sizeof(FLASH_MODULE),
  mod_func_init,
  mod_func_web_sensor,
  mod_func_json_append,
  mod_func_every_second
};


#define jWire (TwoWire*)(jt[0])
#define jWire1 (TwoWire*)(jt[1])
#define jSerial (HardwareSerial*)(jt[2])
#define sprint(A) ((void (*)(char*))(jt[3]))(A)

typedef struct  {
  uint16_t test1;
  uint8_t murks;
} MODULE1_MEMORY;

/*
__asm__ __volatile__ (
".global module_header"
);

__asm__ __volatile__ (
"module_header:"
".byte 0xFC, 0x4A, 0x00, 0x00, 0x00, 0x01"
);

__asm__ __volatile__ (
".word end_module-module_header, mod_func_init-module_header, mod_func_web_sensor-module_header, mod_func_json_append-module_header, mod_func_every_second-module_header"
);

// init module,
__asm__ __volatile__ (
"mod_func_init:"
);
__asm__ __volatile__ (
".text"
);

*/
static int32_t mod_func_init(struct MODULES_TABLE *mt) {
  void (* const *jt)() = mt->jt;

  // allocate memory
  mt->mem_size = sizeof(MODULE1_MEMORY)+8;
  mt->mod_memory = calloc(mt->mem_size/4,4);
  if (!mt->mod_memory) return -1;
  MODULE1_MEMORY *mod_mem = (MODULE1_MEMORY*)mt->mod_memory;
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
  return mt->mem_size;
}


__asm__ __volatile__ (
"mod_func_web_sensor:"
);
static void mod_func_web_sensor(struct MODULES_TABLE *mt) {

}

__asm__ __volatile__ (
"mod_func_json_append:"
);
static void mod_func_json_append(struct MODULES_TABLE *mt) {

}

__asm__ __volatile__ (
"mod_func_every_second:"
);
static void mod_func_every_second(struct MODULES_TABLE *mt) {

}

__asm__ __volatile__ (
"end_module:"
);


#endif
