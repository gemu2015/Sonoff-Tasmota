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

#define XDRV_97             97


#include "./Modules/module.h"
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

#define MAXMODULES 16
MODULES_TABLE modules[MAXMODULES];


extern const FLASH_MODULE module_header;

//extern const FLASH_MODULE module_header;
// scan for modules and add to modules table
void InitModules(void) {

  modules[0].mod_addr = (void *) &module_header;
  AddLog(LOG_LEVEL_INFO, PSTR("Module %x:"),(uint32_t)&module_header);

  const FLASH_MODULE *fm = (FLASH_MODULE*)modules[0].mod_addr;
  modules[0].jt = MODULE_JUMPTABLE;
  fm->mod_func_init(&modules[0], FUNC_INIT);
}

void Module_mdir(void) {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
      AddLog(LOG_LEVEL_INFO, PSTR("Module %d: %s %08x - %d - %d - %d"), cnt + 1, fm->name, modules[cnt].mod_addr,  fm->type, fm->revision, modules[cnt].mem_size);
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

#endif  // USE_MODULES
