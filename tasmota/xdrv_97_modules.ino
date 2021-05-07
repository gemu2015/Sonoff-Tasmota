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


#define EXECUTE_FROM_BINARY
//#define SAVE_DRIVER_TO_FILE

//#define EXECUTE_IN_FLASH
//#define SAVE_FLASH
//#define DO_EXECUTE

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

mySettings mysettings;


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
  JMPTBL&I2cSetDevice,
  JMPTBL&I2cSetActiveFound,
  JMPTBL&AddLog,
  JMPTBL&ResponseAppend_P,
  JMPTBL&WSContentSend_PD,
  JMPTBL&ftostrfd,
  JMPTBL&calloc,
  JMPTBL&fscale,
  JMPTBL&Serial_print,
  JMPTBL&tmod_beginTransmission,
  JMPTBL&tmod_write,
  JMPTBL&tmod_endTransmission,
  JMPTBL&tmod_requestFrom,
  JMPTBL&tmod_read
};

uint8_t *Load_Module(char *path, uint32_t *rsize);
uint32_t Store_Module(uint8_t *fdesc, uint32_t size);

#define MAXMODULES 16
MODULES_TABLE modules[MAXMODULES];

extern const FLASH_MODULE module_header;

// scan for modules and add to modules table
void InitModules(void) {

// read driver from filesystem
#if defined(EXECUTE_IN_RAM) || defined(EXECUTE_IN_FLASH)
  uint32_t size;
  uint8_t *fdesc = Load_Module((char*)"/module.bin", &size);
  if (!fdesc) return;
#endif

// this only works with esp32 and special malloc
#ifdef EXECUTE_IN_RAM
  const FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t old_pc = (uint32_t)fm->end_of_module-size;
  uint32_t new_pc = (uint32_t)fdesc;
  uint32_t offset = new_pc - old_pc;
  uint32_t corr_pc = (uint32_t)fm->mod_func_execute+offset;
  uint32_t *lp = (uint32_t*)&fm->mod_func_execute;
  *lp = corr_pc;
  AddLog(LOG_LEVEL_INFO, PSTR("Module offset %x: %x: %x: %x: %x: %x"),old_pc, new_pc, offset, corr_pc, (uint32_t)fm->mod_func_execute, (uint32_t)&module_header);
//  uint32_t mhead

  modules[0].mod_addr = (void *) &module_header;
  fm = (FLASH_MODULE*)modules[0].mod_addr;
  uint32_t pcc = *(uint32_t*)fm->mod_func_execute;
  AddLog(LOG_LEVEL_INFO, PSTR("Rom %x: "),pcc);
  modules[0].mod_addr = (void *)fdesc;
  fm = (FLASH_MODULE*)modules[0].mod_addr;
  pcc = *(uint32_t*)fm->mod_func_execute;
  AddLog(LOG_LEVEL_INFO, PSTR("Ram %x: "),pcc);
//return;
#endif

#ifdef EXECUTE_IN_FLASH
  modules[0].mod_addr = (void *) Store_Module(fdesc, size);
#endif

//  const FLASH_MODULE *xfm = (FLASH_MODULE*)&module_header;
//  AddLog(LOG_LEVEL_INFO, PSTR("Module  %x: %x"), *(uint32_t*)corr_pc, *(uint32_t*)xfm->mod_func_execute);

#ifdef EXECUTE_FROM_BINARY
  // add one testmodule
  modules[0].mod_addr = (void *) &module_header;
  AddLog(LOG_LEVEL_INFO, PSTR("Module %x: - %x: - %x:"),(uint32_t)modules[0].mod_addr,(uint32_t)&mod_func_execute,(uint32_t)&end_of_module);


/*
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
      modules[cnt].jt = MODULE_JUMPTABLE;
      modules[cnt].mod_size = (uint32_t)fm->end_of_module-(uint32_t)modules[cnt].mod_addr;
      modules[cnt].settings = &mysettings;
      modules[cnt].flags.data = 0;
      //modules[cnt].flags.initialized = false;
#ifdef DO_EXECUTE
    //  fm->mod_func_execute(&modules[0], FUNC_INIT);
#endif
    }
  }
*/
  if (ffsp) {
    File fp;
    fp = ffsp->open((char*)"/module.bin", "w");
    if (fp > 0) {
      uint32_t *fdesc = (uint32_t *)calloc(modules[0].mod_size + 4, 1);
      uint32_t *lp = (uint32_t*)modules[0].mod_addr;
      uint32_t *dp = fdesc;
      for (uint32_t cnt = 0; cnt < modules[0].mod_size; cnt += 4) {
        *dp++ = *lp++;
      }
      fp.write((uint8_t*)fdesc, modules[0].mod_size);
      fp.close();
    }
  }
#endif
}

void Module_EverySecond(void) {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.every_second) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        fm->mod_func_execute(&modules[0], FUNC_EVERY_SECOND);
      }
    }
  }
}

void ModuleWebSensor() {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.web_sensor) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        fm->mod_func_execute(&modules[0], FUNC_WEB_SENSOR);
      }
    }
  }
}

void ModuleJsonAppend() {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.json_append) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        fm->mod_func_execute(&modules[0], FUNC_JSON_APPEND);
      }
    }
  }
}


void tmod_beginTransmission(TwoWire *wp, uint8_t addr) {
  wp->beginTransmission(addr);
}
void tmod_write(TwoWire *wp, uint8_t val) {
  wp->write(val);
}
void tmod_endTransmission(TwoWire *wp, bool flag) {
  wp->endTransmission(flag);
}
void tmod_requestFrom(TwoWire *wp, uint8_t addr, uint8_t num) {
  wp->requestFrom(addr, num);
}

uint8_t tmod_read(TwoWire *wp) {
  return wp->read();
}

// convert float to string
char* ftostrfd(float number, unsigned char prec, char *s) {
  if ((isnan(number)) || (isinf(number))) {  // Fix for JSON output (https://stackoverflow.com/questions/1423081/json-left-out-infinity-and-nan-json-status-in-ecmascript)
    strcpy_P(s, PSTR("null"));
    return s;
  } else {
    return dtostrf(number, 1, prec, s);
  }
}

// scale a float number
float fscale(int32_t number, float mulfac, float subfac) {
  return (float)number * mulfac - subfac;
}

uint8_t *Load_Module(char *path, uint32_t *rsize) {
  if (!ffsp) return 0;
  File fp;
  fp = ffsp->open(path, "r");
  if (fp <= 0) return 0;
  uint32_t size = fp.size();
  uint8_t *fdesc = (uint8_t *)calloc(size / 4 , 4);
  if (!fdesc) return 0;
  fp.read(fdesc, size);
  fp.close();
  *rsize = size;
  return fdesc;
}

// patch calls and store to flash
#define SPEC_SCRIPT_FLASH 0x000F2000
uint32_t Store_Module(uint8_t *fdesc, uint32_t size) {
  uint32_t *lwp=(uint32_t*)fdesc;
  uint32_t eeprom_block = SPEC_SCRIPT_FLASH;
  const FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t old_pc = (uint32_t)fm->end_of_module-size;
  uint32_t new_pc = (uint32_t)eeprom_block + 0x40200000;
  uint32_t offset = new_pc - old_pc;
  uint32_t corr_pc = (uint32_t)fm->mod_func_execute+offset;
  uint32_t *lp = (uint32_t*)&fm->mod_func_execute;
  *lp = corr_pc;
  lp = (uint32_t*)&fm->end_of_module;
  *lp = (uint32_t)fm->end_of_module+offset;

//  AddLog(LOG_LEVEL_INFO, PSTR("Module offset %x: %x: %x: %x: %x: %x"),old_pc, new_pc, offset, corr_pc, (uint32_t)fm->mod_func_execute, (uint32_t)&module_header);
  ESP.flashEraseSector(eeprom_block / SPI_FLASH_SEC_SIZE);
  ESP.flashWrite(eeprom_block , lwp, SPI_FLASH_SEC_SIZE);
  return new_pc;
}

// show all linked modules
void Module_mdir(void) {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
      AddLog(LOG_LEVEL_INFO, PSTR("Module %d: %s %08x - %d- %d - %d - %d"), cnt + 1, fm->name, modules[cnt].mod_addr,  modules[cnt].mod_size,  fm->type, fm->revision, modules[cnt].mem_size);
      //AddLog(LOG_LEVEL_INFO, PSTR("Module %d: %s %08x"), cnt + 1, fm->name, modules[cnt].mod_addr);
    }
  }
  ResponseCmndDone();
}

// link 1 module from file or web
void Module_link(void) {
  uint8_t *fdesc = 0;

  if (XdrvMailbox.data_len) {
    uint32_t size;
    uint8_t *mp = Load_Module(XdrvMailbox.data, &size);
    if (mp) {
      // currently take pos zero
      modules[0].mod_addr = (void *) Store_Module(mp, size);
      free(mp);
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[0].mod_addr;
      modules[0].jt = MODULE_JUMPTABLE;
      modules[0].mod_size = (uint32_t)fm->end_of_module-(uint32_t)modules[0].mod_addr;
      modules[0].settings = &mysettings;
      modules[0].flags.data = 0;
    } else {
      // error
    }
  }
  ResponseCmndDone();
}

// nlink 1 module
void Module_unlink(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAXMODULES)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr) {
      if (modules[module].flags.initialized) {
        // call deiniz
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
        int32_t result = fm->mod_func_execute(&modules[module], FUNC_DEINIT);
        modules[module].flags.data = 0;
      }
      // remove from module table, erase flash
      // modules[module].mod_addr = 0;
    }
  }
  ResponseCmndDone();
}

// iniz 1 module
void Module_iniz(void) {

  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAXMODULES)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr && !modules[module].flags.initialized) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
      int32_t result = fm->mod_func_execute(&modules[module], FUNC_INIT);
      modules[module].flags.every_second = 1;
      modules[module].flags.web_sensor = 1;
      modules[module].flags.json_append = 1;
    }
  }
  ResponseCmndDone();
}

// deiniz 1 module
void Module_deiniz(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAXMODULES)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr && modules[module].flags.initialized) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
      int32_t result = fm->mod_func_execute(&modules[module], FUNC_DEINIT);
      modules[module].flags.data = 0;
    }
  }
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
    case FUNC_EVERY_SECOND:
      Module_EverySecond();
      break;
    case FUNC_WEB_SENSOR:
      ModuleWebSensor();
      break;
    case FUNC_JSON_APPEND:
      ModuleJsonAppend();
      break;
  }
  return result;
}

#endif  // USE_MODULES
