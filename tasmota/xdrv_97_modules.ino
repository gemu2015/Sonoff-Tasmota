/*
  xdrv_97_modules.ino - Prove of concept for flash modules

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


/* attempt to create relocatable flash module drivers
with runtime link and unlink
very early stage
*/

#ifdef USE_MODULES


//#define TEST_MODULE1

#define XDRV_97             97

//  command line commands
const char kModuleCommands[] PROGMEM = "|"// no Prefix
  "mdir" "|"
  "link" "|"
  "unlink" "|"
  "iniz" "|"
  "deiniz"
  ;

void (* const ModuleCommand[])(void) PROGMEM = {
  &Module_mdir,  &Module_link, &Module_unlink, &Module_iniz, &Module_deiniz
};

void Serial_print(char *txt) {
  Serial.printf("test: %s\n",txt);
}

#define JMPTBL (void (*)())
// this table must contain all api calls needed by module
void (* const MODULE_JUMPTABLE[])(void) PROGMEM = {
  JMPTBL&Wire,
#ifdef ESP32
  JMPTBL&Wire1,
#else
  JMPTBL&Wire,
#endif
  JMPTBL&Serial,
  JMPTBL&Serial_print
};


#define jWire (TwoWire*)(jt[0])
#define jWire1 (TwoWire*)(jt[1])
#define jSerial (HardwareSerial*)(jt[2])
#define sprint(A) ((void (*)(char*))(jt[3]))(A)

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

static int32_t mod_func_init(struct MODULES_TABLE *);
static void mod_func_web_sensor(struct MODULES_TABLE *);
static void mod_func_json_append(struct MODULES_TABLE *);
static void mod_func_every_second(struct MODULES_TABLE *);
//static const FLASH_MODULE  *end_of_module(void);


#define MAXMODULES 16
struct MODULES_TABLE {
  const FLASH_MODULE *mod_addr;
  void (* const *jt)();
  void *mod_memory;
  uint16_t mem_size;
} modules[MAXMODULES];

extern "C" {
 extern const FLASH_MODULE module_header;
}
//extern const FLASH_MODULE module_header;

// scan for modules and add to modules table
void InitModules(void) {

  modules[0].mod_addr = &module_header;
//  modules[0].mod_addr = &module_header;
  //const FLASH_MODULE *fm = modules[0].mod_addr;
//  Serial.printf("module %0x\n",modules[0].mod_addr);
  //Serial.printf("module %0x %d\n",fm->sync, fm->size);
  modules[0].jt = MODULE_JUMPTABLE;
  return;
  //fm->mod_func_init(&modules[0]);
}

void Module_mdir(void) {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      const FLASH_MODULE *fm = modules[cnt].mod_addr;
      AddLog(LOG_LEVEL_INFO, PSTR("Module %d: %08x - %d - %d - %d - %d"), cnt + 1, modules[cnt].mod_addr, fm->type, fm->revision, fm->size, modules[cnt].mem_size);
    }
  }
  ResponseCmndDone();
}
void Module_link(void) {
  ResponseCmndDone();
}
void Module_unlink(void) {
  ResponseCmndDone();
}
void Module_iniz(void) {
  ResponseCmndDone();
}
void Module_deiniz(void) {
  ResponseCmndDone();
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv97(uint8_t function) {
  bool result = false;

  switch (function) {
    case FUNC_COMMAND:
      result = DecodeCommand(kModuleCommands, ModuleCommand);
      break;
    case FUNC_INIT:
      InitModules();
      break;
  }
  return result;
}


#ifdef TEST_MODULE1
// example module

static const FLASH_MODULE module_header = {
  0x4AFC,
  MODULE_TYPE_SENSOR,
  0,
  (uint32_t)end_of_module-(uint32_t)mod_func_init + sizeof(FLASH_MODULE),
  mod_func_init,
  mod_func_web_sensor,
  mod_func_json_append,
  mod_func_every_second
};


typedef struct  {
  uint16_t test1;
  uint8_t murks;
} MODULE1_MEMORY;

// init module,
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

static void mod_func_web_sensor(struct MODULES_TABLE *mt) {

}

static void mod_func_json_append(struct MODULES_TABLE *mt) {

}

static void mod_func_every_second(struct MODULES_TABLE *mt) {

}

static const FLASH_MODULE  *end_of_module(void) {
  return &module_header;
}

#endif
#endif  // USE_MODULES
