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


//#define EXECUTE_FROM_BINARY
//#define SAVE_DRIVER_TO_FILE

//#define EXECUTE_IN_FLASH
//#define SAVE_FLASH
//#define DO_EXECUTE

#include "./Modules/module.h"


#ifdef EXECUTE_FROM_BINARY
extern const FLASH_MODULE module_header;
#else
// set dummy header to calm linker
FLASH_MODULE module_header = {
  MODULE_SYNC,
  CURR_ARCH,
  MODULE_TYPE_SENSOR,
  0,
  "MLX90614",
  0,
  0,
  0,
  0
};
#endif


//  command line commands
const char kModuleCommands[] PROGMEM = "|"// no Prefix
  "mdir" "|"
  "link" "|"
  "unlink" "|"
  "iniz" "|"
  "deiniz" "|"
  "dump"
  ;

void (* const ModuleCommand[])(void) PROGMEM = {
  &Module_mdir,  &Module_link, &Module_unlink, &Module_iniz, &Module_deiniz, &Module_dump
};

void Serial_print(char *txt) {
  //Serial.printf("test: %x %x\n",(uint32_t)txt, *(uint32_t*)txt);
  //Serial.printf("test: %x\n",(uint32_t)txt);

  Serial.printf_P("test: %s\n",txt);
}


#define JMPTBL (void (*)())
// this vector table table must contain all api calls needed by module
// and in sync with vectortable in module.h
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
  JMPTBL&tmod_read,
  JMPTBL&show_hex_address,
  JMPTBL&free,
  JMPTBL&I2cWrite16,
  JMPTBL&I2cRead16,
  JMPTBL&I2cValidRead16,
  JMPTBL&snprintf_P,
  JMPTBL&XdrvRulesProcess,
  JMPTBL&ResponseJsonEnd,
  JMPTBL&delay,
  JMPTBL&I2cActive,
  JMPTBL&ResponseJsonEndEnd,
  JMPTBL&IndexSeparator,
  JMPTBL&Response_P,
  JMPTBL&I2cResetActive,
  JMPTBL&tmod_isnan,
  JMPTBL&ConvertTemp,
  JMPTBL&ConvertHumidity,
  JMPTBL&TempHumDewShow,
  JMPTBL&strlcpy,
  JMPTBL&GetTextIndexed,
  JMPTBL&GetTasmotaGlobal,
  JMPTBL&tmod_iseq,
  JMPTBL&tmod_fdiv,
  JMPTBL&tmod_fmul,
  JMPTBL&tmod_fdiff,
  JMPTBL&tmod_tofloat
};




uint8_t *Load_Module(char *path, uint32_t *rsize);
uint32_t Store_Module(uint8_t *fdesc, uint32_t size, uint32_t *offset, uint8_t flag);

#define MAXMODULES 16
MODULES_TABLE modules[MAXMODULES];

// scan for modules in flash and add to modules table, not yet
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
  uint32_t old_pc = (uint32_t)fm->end_of_module - size - 4;
  uint32_t new_pc = (uint32_t)fdesc;
  uint32_t offset = new_pc - old_pc;
  uint32_t corr_pc = (uint32_t)fm->mod_func_execute+offset;
  uint32_t *lp = (uint32_t*)&fm->mod_func_execute;
  *lp = corr_pc;
  AddLog(LOG_LEVEL_INFO, PSTR("Module offset %x: %x: %x: %x: %x: %x"),old_pc, new_pc, offset, corr_pc, (uint32_t)fm->mod_func_execute, (uint32_t)&module_header);

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

  uint32_t offset = 0;
#ifdef EXECUTE_IN_FLASH
  modules[0].mod_addr = (void *) Store_Module(fdesc, size, &offset, 0);
#endif

//  const FLASH_MODULE *xfm = (FLASH_MODULE*)&module_header;
//  AddLog(LOG_LEVEL_INFO, PSTR("Module  %x: %x"), *(uint32_t*)corr_pc, *(uint32_t*)xfm->mod_func_execute);

#ifdef EXECUTE_FROM_BINARY
  // add one testmodule
  modules[0].mod_addr = (void *) &module_header;
  AddLog(LOG_LEVEL_INFO, PSTR("Module %x: - %x: - %x:"),(uint32_t)modules[0].mod_addr,(uint32_t)&mod_func_execute,(uint32_t)&end_of_module);

  const FLASH_MODULE *fm = (FLASH_MODULE*)modules[0].mod_addr;
  modules[0].jt = MODULE_JUMPTABLE;
  modules[0].execution_offset = offset;
  modules[0].mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[0].mod_addr + 4;

  modules[0].settings = &Settings;

  modules[0].flags.data = 0;


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
      FLASH_MODULE *flp = (FLASH_MODULE*)fdesc;
      // patch size
      flp->size = modules[0].mod_size;
      fp.write((uint8_t*)fdesc, modules[0].mod_size);
      fp.close();
    }
  }


#else

  AddModules();
#endif
}

void Module_Execute(uint32_t sel) {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.every_second) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        fm->mod_func_execute(&modules[cnt], sel);
      }
    }
  }
}

void ModuleWebSensor() {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.web_sensor) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        fm->mod_func_execute(&modules[cnt], FUNC_WEB_SENSOR);
      }
    }
  }
}

void ModuleJsonAppend() {
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.json_append) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        fm->mod_func_execute(&modules[cnt], FUNC_JSON_APPEND);
      }
    }
  }
}

// some helper functions
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

bool tmod_isnan(float val) {
  return isnan(val);
}

bool tmod_iseq(float val) {
  return val == 0.0;
}

float tmod_fdiv(float p1, float p2) {
  return p1/p2;
}
float tmod_fmul(float p1, float p2) {
  return p1*p2;
}

float tmod_fdiff(float p1, float p2) {
  return p1-p2;
}

float tmod_tofloat(uint64_t in) {
  return in;
}


uint32_t GetTasmotaGlobal(uint32_t sel) {
  switch (sel) {
    case 1:
      return TasmotaGlobal.tele_period;
      break;
  }
  return 0;
}


void show_hex_address(uint32_t addr) {
  AddLog(LOG_LEVEL_INFO,PSTR(">>> %08x"), addr);
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
#ifdef ESP8266
  uint8_t *fdesc = (uint8_t *)calloc(size / 4 , 4);
#endif
#ifdef ESP32
  uint8_t *fdesc = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_EXEC);
#endif
  if (!fdesc) return 0;
  fp.read(fdesc, size);
  fp.close();
  *rsize = size;
  return fdesc;
}

/*
ESP.getFlashChipRealSize()
ESP.getFlashChipSize()
ESP_getSketchSize()
ESP.getFreeSketchSpace()
EspFlashBaseAddress(void)
EspFlashBaseEndAddress(void)
*/

// patch calls and store to flash
// first version assumes module to be smaller then SPI_FLASH_SEC_SIZE
// we only use full sectors and align to sector size
#define SPEC_SCRIPT_FLASH 0x000F2000
#define FLASH_BASE_OFFSET 0x40200000
uint32_t Store_Module(uint8_t *fdesc, uint32_t size, uint32_t *ioffset, uint8_t flag) {
#ifdef ESP8266
uint32_t free_flash_start = ESP_getSketchSize();
uint32_t free_flash_end = (ESP_getSketchSize() + ESP.getFreeSketchSpace());
#endif
#ifdef ESP32
uint32_t free_flash_start = EspFlashBaseAddress();
uint32_t free_flash_end = EspFlashBaseEndAddress();
#endif

uint32_t aoffset;
uint32_t eeprom_block;

  // align to sector start
  free_flash_start =  (free_flash_start + SPI_FLASH_SEC_SIZE) & (SPI_FLASH_SEC_SIZE-1^0xffffffff);
  free_flash_end   =  (free_flash_end + SPI_FLASH_SEC_SIZE) & (SPI_FLASH_SEC_SIZE-1^0xffffffff);

  //AddLog(LOG_LEVEL_INFO,PSTR(">>> %08x  - %08x"),free_flash_start,free_flash_end );

  if (flag == 0) {
    aoffset = FLASH_BASE_OFFSET;
    eeprom_block = free_flash_start;
  } else {
    aoffset = 0;
    eeprom_block = (uint32_t)fdesc;
  }
  // search for free entry
  uint32_t *lp = (uint32_t*) ( aoffset + free_flash_start );
  for (uint32_t addr = free_flash_start; addr < free_flash_end; addr += SPI_FLASH_SEC_SIZE) {
      //AddLog(LOG_LEVEL_INFO,PSTR("addr, sync %08x: %08x: %04x"),addr,(uint32_t)lp, *lp);
      if (*lp != MODULE_SYNC) {
        // free module space
        eeprom_block = addr;
        break;
      } else {
        // skip address by module size
        const FLASH_MODULE *fm = (FLASH_MODULE*)addr;
        uint32_t modsize = fm->size;
        if (modsize > SPI_FLASH_SEC_SIZE) {
          // must align and increment addr
        }
      }
      lp += SPI_FLASH_SEC_SIZE/4;
  }

  uint32_t *lwp=(uint32_t*)fdesc;
  const FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t old_pc = (uint32_t)fm->end_of_module - (size - 4);
  uint32_t new_pc = (uint32_t)eeprom_block + aoffset;
  uint32_t offset = new_pc - old_pc;
  *ioffset = offset;
  uint32_t corr_pc = (uint32_t)fm->mod_func_execute + offset;
  lp = (uint32_t*)&fm->mod_func_execute;
  *lp = corr_pc;
  lp = (uint32_t*)&fm->end_of_module;
  *lp = (uint32_t)fm->end_of_module + offset;
  lp = (uint32_t*)&fm->execution_offset;
  *lp = offset;

#ifdef ESP8266
//  AddLog(LOG_LEVEL_INFO, PSTR("Module offset %x: %x: %x: %x: %x: %x"),old_pc, new_pc, offset, corr_pc, (uint32_t)fm->mod_func_execute, (uint32_t)&module_header);
  ESP.flashEraseSector(eeprom_block / SPI_FLASH_SEC_SIZE);
  ESP.flashWrite(eeprom_block , lwp, SPI_FLASH_SEC_SIZE);
#endif
  return new_pc;
}

void AddModules(void) {
#ifdef ESP8266
  uint32_t free_flash_start = ESP_getSketchSize();
  uint32_t free_flash_end = (ESP_getSketchSize() + ESP.getFreeSketchSpace());
#endif
#ifdef ESP32
  uint32_t free_flash_start = EspFlashBaseAddress();
  uint32_t free_flash_end = EspFlashBaseEndAddress();
#endif

  // align to sector start
  free_flash_start =  (free_flash_start + SPI_FLASH_SEC_SIZE) & (SPI_FLASH_SEC_SIZE-1^0xffffffff);
  free_flash_end   =  (free_flash_end + SPI_FLASH_SEC_SIZE) & (SPI_FLASH_SEC_SIZE-1^0xffffffff);

  uint16_t module = 0;
  uint32_t *lp = (uint32_t*) ( FLASH_BASE_OFFSET + free_flash_start );
  for (uint32_t addr = free_flash_start; addr < free_flash_end; addr += SPI_FLASH_SEC_SIZE) {
    //AddLog(LOG_LEVEL_INFO,PSTR("addr, sync %08x: %08x: %04x"),addr,(uint32_t)lp, *lp);
    if (*lp == MODULE_SYNC) {
      // add module
      const FLASH_MODULE *fm = (FLASH_MODULE*)lp;
      modules[module].mod_addr = (FLASH_MODULE*)lp;
      modules[module].jt = MODULE_JUMPTABLE;
      modules[module].execution_offset = fm->execution_offset;
      modules[module].mod_size = fm->size;
      modules[module].settings = &Settings;
      modules[module].flags.data = 0;
      // add addr according to module size, currently assume module < SPI_FLASH_SEC_SIZE
      module++;
      if (module >= MAXMODULES) {
        break;
      }
    }
    lp += SPI_FLASH_SEC_SIZE/4;
  }
}

// show all linked modules
void Module_mdir(void) {
  AddLog(LOG_LEVEL_INFO, PSTR("| ======== Module directory ========"));
  AddLog(LOG_LEVEL_INFO, PSTR("| nr | name            | address  | size | type | rev  | ram  | init"));
  for (uint8_t cnt = 0; cnt < MAXMODULES; cnt++) {
    if (modules[cnt].mod_addr) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
      const char *type = "xsns"; // only currently supported type
      AddLog(LOG_LEVEL_INFO, PSTR("| %2d | %-16s| %08x | %4d | %4s | %04x | %4d | %1d |"), cnt + 1, fm->name, modules[cnt].mod_addr,
       modules[cnt].mod_size,  type, fm->revision, modules[cnt].mem_size, modules[cnt].flags.initialized);
      // AddLog(LOG_LEVEL_INFO, PSTR("| %2d | %-16s| %08x | %4d | %4s | %04x | %4d | %1d | %08x"), cnt + 1, fm->name, modules[cnt].mod_addr,
      //  modules[cnt].mod_size,  type, fm->revision, modules[cnt].mem_size, modules[cnt].flags.initialized, fm->execution_offset);

      //AddLog(LOG_LEVEL_INFO, PSTR("Module %d: %s %08x"), cnt + 1, fm->name, modules[cnt].mod_addr);
    }
  }
  ResponseCmndDone();
}

// link 1 module from file (or web, not yet)
void Module_link(void) {
  uint8_t *fdesc = 0;

  if (XdrvMailbox.data_len) {
    uint32_t size;
    uint8_t cnt;
    uint8_t *mp = Load_Module(XdrvMailbox.data, &size);
    if (mp) {
      for (cnt = 0; cnt < MAXMODULES; cnt++) {
        if (!modules[cnt].mod_addr) {
          break;
        }
      }
      uint32_t offset;
#ifdef ESP32
      modules[cnt].mod_addr = (void *) Store_Module(mp, size, &offset, 1);
#else
      modules[cnt].mod_addr = (void *) Store_Module(mp, size, &offset, 0);
      free(mp);
#endif
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
      modules[cnt].jt = MODULE_JUMPTABLE;
      modules[cnt].mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[cnt].mod_addr + 4;
      modules[cnt].execution_offset = offset;
      modules[cnt].settings = &Settings;
      modules[cnt].flags.data = 0;
      AddLog(LOG_LEVEL_INFO,PSTR("module %s loaded at slot %d"), XdrvMailbox.data, 1);
    } else {
      // error
      AddLog(LOG_LEVEL_INFO,PSTR("module error"));
    }
  }
  ResponseCmndDone();
}

// unlink 1 module
void Module_unlink(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAXMODULES)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr) {
      if (modules[module].flags.initialized) {
        // call deiniz
        Deiniz_module(module);
      }
      // remove from module table, erase flash
      if ((uint32_t)modules[module].mod_addr != (uint32_t)&module_header) {
        ESP.flashEraseSector(((uint32_t)modules[module].mod_addr - FLASH_BASE_OFFSET) / SPI_FLASH_SEC_SIZE);
      }
      modules[module].mod_addr = 0;
      AddLog(LOG_LEVEL_INFO,PSTR("module %d unlinked"),module + 1);
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
      AddLog(LOG_LEVEL_INFO,PSTR("module %d inizialized"),module + 1);
    }
  }
  ResponseCmndDone();
}

void Deiniz_module(uint32_t module) {
  if (modules[module].mod_addr && modules[module].flags.initialized) {
    const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
    int32_t result = fm->mod_func_execute(&modules[module], FUNC_DEINIT);
    modules[module].flags.data = 0;
    AddLog(LOG_LEVEL_INFO,PSTR("module %d deinizialized"),module + 1);
  }
}

// deiniz 1 module
void Module_deiniz(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAXMODULES)) {
    Deiniz_module(XdrvMailbox.payload - 1);
  }
  ResponseCmndDone();
}

// dump module hex 32 bit words
void Module_dump(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAXMODULES)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr) {
      uint32_t *lp = (uint32_t*) modules[module].mod_addr;
      for (uint32_t cnt = 0; cnt < 16; cnt ++) {
        AddLog(LOG_LEVEL_INFO,PSTR("%08x: %08x %08x %08x %08x %08x %08x %08x %08x"),lp,lp[0],lp[1],lp[2],lp[3],lp[4],lp[5],lp[6],lp[7]);
        lp += 8;
      }
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
    case FUNC_EVERY_250_MSECOND:
    case FUNC_EVERY_SECOND:
      Module_Execute(function);
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
