/*
  xdrv_123_plugins.ino - Prove of concept for flash plugins

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


/* proof of concept only
attempt to create relocatable flash module drivers
with runtime link and unlink
adds about 11,3k flash size

to doo:


*/

#ifdef USE_BINPLUGINS

#define XDRV_123             123

//#define EXECUTE_FROM_BINARY
//#define SAVE_DRIVER_TO_FILE

//#define EXECUTE_IN_FLASH
//#define SAVE_FLASH
//#define DO_EXECUTE

#include "./Plugins/modules_def.h"
#include <TasmotaSerial.h>

#ifdef EXECUTE_FROM_BINARY
extern const FLASH_MODULE module_header;
#else
// set dummy header to calm linker
FLASH_MODULE module_header = {
  0,
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

extern FS *ffsp;

#ifndef module_name
#define module_name "/module.bin"
#endif

#define MODFUNC_WEB_SENSOR FUNC_WEB_SENSOR
#define MODFUNC_JSON_APPEND FUNC_JSON_APPEND
#define MODFUNC_INIT FUNC_INIT

//  command line commands
const char kModuleCommands[] PROGMEM = "|"// no Prefix
  "mdir" "|"
  "link" "|"
  "unlink" "|"
  "iniz" "|"
  "deiniz" "|"
  "dump"
#ifdef USE_FLASH_BDIR 
  "|" "list"
#endif
  ;

void (* const ModuleCommand[])(void) PROGMEM = {
  &Module_mdir,  &Module_link, &Module_unlink, &Module_iniz, &Module_deiniz, &Module_dump
#ifdef USE_FLASH_BDIR 
   ,&BinDir_list
#endif
};

void Serial_print(const char *txt) {
  //Serial.printf("test: %x %x\n",(uint32_t)txt, *(uint32_t*)txt);
  //Serial.printf("test: %x\n",(uint32_t)txt);
  Serial.printf_P(PSTR("test: %s\n"),txt);
}

TasmotaSerial *tmod_newTS(int32_t rpin, int32_t tpin);
int tmod_beginTS(TasmotaSerial *ts, uint32_t baud);
size_t tmod_writeTS(TasmotaSerial *ts, char *buf, uint32_t size);
void tmod_flushTS(TasmotaSerial *ts);
void tmod_deleteTS(TasmotaSerial *ts);
size_t tmod_readTS(TasmotaSerial *ts, char *buf, uint32_t size);
int tmod_read1TS(TasmotaSerial *ts);
uint8_t tmod_availTS(TasmotaSerial *ts);
bool hardwareSerialTS(TasmotaSerial *ts);
void AddlogT(char* txt);
bool MT_DecodeCommand(const char* haystack, void (* const InCommand[])(void));
size_t tmod_write1TS(TasmotaSerial *ts, uint8_t val);
#ifdef ESP32
void twi_readFrom(uint8_t address, uint8_t* data, uint8_t length);
#endif
bool tmod_I2cSetDevice(uint32_t addr);
void tmod_I2cSetActiveFound(uint32_t addr, const char *types, uint32_t bus);
int tmod_strncasecmp_P(const char* s1, const char *s2, size_t len);
char *copyStr(const char * str);
void tmod_setClockStretchLimit(TwoWire *wp, uint32_t val);
void tmod_writen(TwoWire *wp, uint8_t *buf, uint32_t len);

extern "C" {
 extern void (* const MODULE_JUMPTABLE[])(void);
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
  JMPTBL&tmod_I2cSetDevice,
  //JMPTBL&I2cSetActiveFound,
  JMPTBL&tmod_I2cSetActiveFound,

  //void I2cSetActiveFound(uint32_t addr, const char *types, uint32_t bus = 0);
  //void I2cSetActiveFound(uint32_t addr, const char *types, uint32_t bus)

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
  //JMPTBL&XdrvRulesProcess,
  JMPTBL(bool (*)(bool teleperiod))&XdrvRulesProcess,
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
  JMPTBL&tmod_tofloat,
  JMPTBL&tmod_fadd,
  JMPTBL&I2cRead8,
  JMPTBL&I2cWrite8,
  JMPTBL&tmod_available,
  JMPTBL&AddLogMissed,
  JMPTBL&tmod_NAN,
  JMPTBL&tmod_gtsf2,
  JMPTBL&tmod_ltsf2,
  JMPTBL&tmod_eqsf2,
  JMPTBL&tmod_Pin,
  JMPTBL&tmod_newTS,
  JMPTBL&tmod_writeTS,
  JMPTBL&tmod_flushTS,
  JMPTBL&tmod_beginTS,
  JMPTBL&XdrvMailbox,
  JMPTBL&GetCommandCode,
  JMPTBL&strlen,
  JMPTBL&tmod_strncasecmp_P,
  JMPTBL&toupper,
  JMPTBL&iscale,
  JMPTBL&tmod_deleteTS,
  JMPTBL&tmod_readTS,
  JMPTBL&tmod_read1TS,
  JMPTBL&tmod_availTS,
  JMPTBL&MqttPublishTeleSensor,
  JMPTBL&strtoul,
  JMPTBL&AddLogBuffer,
  JMPTBL&ResponseTime_P,
  JMPTBL&ClaimSerial,
  JMPTBL&hardwareSerialTS,
  JMPTBL&millis,
  JMPTBL&sprintf_P,
  JMPTBL&AddlogT,
  JMPTBL&tmod__divsi3,
  JMPTBL&tmod__udivsi3,
  JMPTBL&tmod__floatsisf,
  JMPTBL&tmod__floatunsisf,
  JMPTBL&FastPrecisePowf,
  JMPTBL&GetTasmotaGlobalf,
  JMPTBL&tmod__muldi3,
  JMPTBL&tmod__fixunssfsi,
  JMPTBL&tmod__umodsi3,
  JMPTBL&twi_readFrom,
  JMPTBL&DecodeCommand,
  JMPTBL&ResponseCmndDone,
  JMPTBL&tmod_write1TS,
  JMPTBL&memcmp_P,
  JMPTBL&ToHex_P,
  JMPTBL&memset,
  JMPTBL&memmove_P,
  JMPTBL&ResponseCmndNumber,
  JMPTBL&ResponseCmndFloat,
  JMPTBL&ResponseAppendTHD,
  JMPTBL&WSContentSend_THD,
  JMPTBL&strncpy_P,
  JMPTBL&isprint,
  JMPTBL&tmod_isinf,
  JMPTBL&copyStr,
  JMPTBL&tmod_setClockStretchLimit,
  JMPTBL&tmod_writen,
  JMPTBL&modff
};


#ifdef ESP32
void twi_readFrom(uint8_t address, uint8_t* data, uint8_t length) {
  Wire.requestFrom(address, (size_t)length, (bool)true);
  Wire.readBytes(data, length);
}
#endif  // ESP32

/*
bool MT_DecodeCommand(const char* haystack, void (* const MyCommand[])(void)) {

  haystack += mt->execution_offset;
  MyCommand += (mt->execution_offset >> 2);
  
  GetTextIndexed(XdrvMailbox.command, CMDSZ, 0, haystack);  // Get prefix if available
  int prefix_length = strlen(XdrvMailbox.command);
  char prefix[prefix_length + 1];
  snprintf_P(prefix, sizeof(prefix), XdrvMailbox.topic);  // Copy prefix part only
  if (prefix_length) {
    if (strcasecmp(prefix, XdrvMailbox.command)) {
      return false;                                         // Prefix not in command
    }
  }
  int command_code = GetCommandCode(XdrvMailbox.command + prefix_length, CMDSZ, XdrvMailbox.topic + prefix_length, haystack);
  if (command_code > 0) {
    XdrvMailbox.command_code = command_code - 1;
    uint32_t lval = (uint32_t)MyCommand[XdrvMailbox.command_code];
    lval += mt->execution_offset;
    void (* const cmdaddr)(MODULES_TABLE *mt) = (void (* const)(MODULES_TABLE *mt))lval;
    //AddLog(LOG_LEVEL_INFO,PSTR(">>> %08x - %08x"), lval, mt->execution_offset );
    cmdaddr(mt);
    return true;
  }
  return false;
}
*/


int tmod_strncasecmp_P(const char *s1, const char *s2, size_t len) {
#ifdef ESP32
  char *sx = copyStr(s2);
  int res = strncasecmp_P(s1, sx, len);
  free(sx);
  return res;
#endif
#ifdef ESP8266
  return strncasecmp_P(s1, s2, len);
#endif
}

void AddlogT(char* txt) {
   AddLog(LOG_LEVEL_INFO ,PSTR("%s"), txt);
}
// some helper functions
void tmod_beginTransmission(TwoWire *wp, uint8_t addr) {
  wp->beginTransmission(addr);
}
void tmod_write(TwoWire *wp, uint8_t val) {
  wp->write(val);
}

void tmod_writen(TwoWire *wp, uint8_t *buf, uint32_t len) {
  wp->write(buf, len);
}

void tmod_endTransmission(TwoWire *wp, bool flag) {
  wp->endTransmission(flag);
}
void tmod_requestFrom(TwoWire *wp, uint8_t addr, uint8_t num) {
  wp->requestFrom(addr, num);
}

bool tmod_I2cSetDevice(uint32_t addr) {
  return I2cSetDevice(addr);
}

void tmod_I2cSetActiveFound(uint32_t addr, const char *types, uint32_t bus) {
//#ifdef ESP8266
#if 1  
  I2cSetActiveFound(addr, types, bus);
#else
  char *cp = copyStr(types);
  I2cSetActiveFound(addr, cp, bus);
  free(cp);
#endif
}

void tmod_setClockStretchLimit(TwoWire *wp, uint32_t val) {
 #ifdef ESP8266 
  wp->setClockStretchLimit(val);
#endif
}


int tmod_read(TwoWire *wp) {
  return wp->read();
}

uint8_t tmod_available(TwoWire *wp) {
  return wp->available();
}

bool tmod_isnan(float val) {
  return isnan(val);
}

bool tmod_isinf(float val) {
  return isinf(val);
}

float tmod_NAN(void) {
  return NAN;
}

bool tmod_gtsf2(float p1, float p2) {
  return p1 > p2;
}
bool tmod_ltsf2(float p1, float p2) {
  return p1 < p2;
}
bool tmod_eqsf2(float p1, float p2) {
  return p1 == p2;
}

bool tmod_iseq(float val) {
  return val == 0.0;
}

float tmod__floatsisf(int32_t in) {
  return in;
}

float tmod__floatunsisf(uint32_t in) {
  return in;
}

uint32_t tmod__fixunssfsi(float in) {
  return in;
}


uint32_t tmod__udivsi3(uint32_t p1, uint32_t p2) {
  return p1 / p2;
}

uint32_t tmod__umodsi3(uint32_t p1, uint32_t p2) {
  return p1 % p2;
}



int32_t tmod__divsi3(int32_t p1, int32_t p2) {
  return p1 / p2;
}

int64_t tmod__muldi3(int64_t p1, int64_t p2) {
  return p1 * p2;
}



float tmod_fdiv(float p1, float p2) {
  return p1 / p2;
}
float tmod_fmul(float p1, float p2) {
  return p1 * p2;
}

float tmod_fdiff(float p1, float p2) {
  return p1 - p2;
}

float tmod_fadd(float p1, float p2) {
  return p1 + p2;
}


float tmod_tofloat(uint64_t in) {
  return in;
}

int tmod_Pin(uint32_t pin, uint32_t index) {
  return Pin(pin, index);
}

TasmotaSerial *tmod_newTS(int32_t rpin, int32_t tpin) {
  TasmotaSerial *ts = new TasmotaSerial(rpin, tpin, 1);
  return ts;
}

int tmod_beginTS(TasmotaSerial *ts, uint32_t baud) {
  return ts->begin(baud);
}


void tmod_deleteTS(TasmotaSerial *ts) {
  delete(ts);
}

size_t tmod_writeTS(TasmotaSerial *ts, char *buf, uint32_t size) {
  return ts->write(buf, size);
}

size_t tmod_write1TS(TasmotaSerial *ts, uint8_t val) {
  return ts->write(val);
}

size_t tmod_readTS(TasmotaSerial *ts, char *buf, uint32_t size) {
  return ts->read(buf, size);
}

int tmod_read1TS(TasmotaSerial *ts) {
  return ts->read();
}

uint8_t tmod_availTS(TasmotaSerial *ts) {
  return ts->available();
}

void tmod_flushTS(TasmotaSerial *ts) {
  return ts->flush();
}

bool hardwareSerialTS(TasmotaSerial *ts) {
  return  ts->hardwareSerial();
}

uint32_t GetTasmotaGlobal(uint32_t sel) {
  switch (sel) {
    case 1:
      return TasmotaGlobal.tele_period;
      break;
    case 2:
      return TasmotaGlobal.global_update;
      break;
    case 3:
      return TasmotaGlobal.humidity;
      break;
  }
  return 0;
}

float GetTasmotaGlobalf(uint32_t sel) {
  return TasmotaGlobal.temperature_celsius;
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

int32_t iscale(int32_t number, int32_t mulfac, int32_t divfac) {
  return (number * mulfac) / divfac;
}

uint8_t *Load_Module(char *path, uint32_t *rsize);
uint32_t Store_Module(uint8_t *fdesc, uint32_t size, uint32_t *offset, uint8_t flag, uint8_t index);

#ifndef MAX_PLUGINS
#define MAX_PLUGINS 8
#endif

#define SPEC_SCRIPT_FLASH 0x000F2000

#ifdef ESP8266
#undef FLASH_BASE_OFFSET
#define FLASH_BASE_OFFSET 0x40200000
#undef MODUL_END_OFFSET
#define MODUL_END_OFFSET 4
#else
#undef FLASH_BASE_OFFSET
#define FLASH_BASE_OFFSET 0x3F400000

#ifdef __riscv
#undef MODUL_END_OFFSET
#define MODUL_END_OFFSET 4
#else
#undef MODUL_END_OFFSET
#define MODUL_END_OFFSET 8
#endif
#endif

struct PLUGINS {
uint32_t free_flash_start;
uint32_t free_flash_end;
uint32_t flashbase;
uint32_t pagesize;
#ifdef ESP32
const esp_partition_t *flash_pptr;
spi_flash_mmap_handle_t map_handle;
#endif
uint8_t *module_input_buffer;
uint8_t *module_input_ptr;
uint16_t module_bytes_read;
uint16_t module_size;
char   mod_name[16];
bool ready;
} plugins;

MODULES_TABLE modules[MAX_PLUGINS];

#define MOD_EXEC(A)  fm->mod_func_execute(A)

#ifdef ESP8266
#undef SET_MOD_REG
#define SET_MOD_REG(A)
#else
#undef SET_MOD_REG
#define SET_MOD_REG(A) *(uint32_t*)GLOB_MOD_REG=(uint32_t)&modules[A];
#endif

void Setplugins(void) {

#ifdef ESP8266
  plugins.free_flash_start = ESP_getSketchSize();
  plugins.free_flash_end = (ESP_getSketchSize() + ESP.getFreeSketchSpace());
  plugins.pagesize = SPI_FLASH_SEC_SIZE;
  plugins.flashbase = FLASH_BASE_OFFSET;
   // 00210000: 00400000: 400d758c:
  // align to sector start
  plugins.free_flash_start =  (plugins.free_flash_start + plugins.pagesize) & (plugins.pagesize-1^0xffffffff);
  plugins.free_flash_end   =  (plugins.free_flash_end + plugins.pagesize) & (plugins.pagesize-1^0xffffffff);
  plugins.ready = true;
#endif
#ifdef ESP32
  plugins.pagesize = SPI_FLASH_SEC_SIZE;
  plugins.flash_pptr = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, "custom");
  if (plugins.flash_pptr) {
    const void *out_ptr;
    //esp_err_t err = esp_partition_mmap(plugins.flash_pptr, 0, plugins.flash_pptr->size, SPI_FLASH_MMAP_DATA, &out_ptr, &plugins.map_handle);
    esp_err_t err = esp_partition_mmap(plugins.flash_pptr, 0, plugins.flash_pptr->size, SPI_FLASH_MMAP_INST, &out_ptr, &plugins.map_handle);

    plugins.free_flash_start = (uint32_t)out_ptr;
    plugins.free_flash_end = plugins.free_flash_start + plugins.flash_pptr->size;
    plugins.flashbase = 0;
    AddLog(LOG_LEVEL_INFO,PSTR("Plugins-> start: %08x, end: %08x"),plugins.free_flash_start, plugins.free_flash_end);
    plugins.ready = true;
  } else {
    plugins.ready = false;
    AddLog(LOG_LEVEL_INFO,PSTR("Plugins: Partition not found"));
  }
#endif

}

// scan for modules in flash and add to modules table
void InitModules(void) {

  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    modules[cnt].mod_addr = 0;
  }

  Setplugins();

  if (!plugins.ready) {
    return;
  }

  strlcpy(plugins.mod_name, module_name, sizeof(plugins.mod_name));

// read driver from filesystem
#if defined(EXECUTE_IN_RAM) || defined(EXECUTE_IN_FLASH)
  uint32_t size;
  uint8_t *fdesc = Load_Module((char*)plugins.mod_name, &size);
  if (!fdesc) return;
#endif

// this only works with esp32 and special malloc
#ifdef EXECUTE_IN_RAM
  const FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t old_pc = (uint32_t)fm->end_of_module - size - MODUL_END_OFFSET;
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
#endif // EXECUTE_IN_RAM

  uint32_t offset = 0;
#ifdef EXECUTE_IN_FLASH
  modules[0].mod_addr = (void *) Store_Module(fdesc, size, &offset, 0, 0);
#endif

//  const FLASH_MODULE *xfm = (FLASH_MODULE*)&module_header;
//  AddLog(LOG_LEVEL_INFO, PSTR("Module  %x: %x"), *(uint32_t*)corr_pc, *(uint32_t*)xfm->mod_func_execute);

#ifdef EXECUTE_FROM_BINARY
  // add one testmodule
  modules[0].mod_addr = (void *) &module_header;
//  AddLog(LOG_LEVEL_INFO, PSTR("Module %x: - %x: - %x:"),(uint32_t)modules[0].mod_addr,(uint32_t)&mod_func_execute,(uint32_t)&end_of_module);

  const FLASH_MODULE *fm = (FLASH_MODULE*)modules[0].mod_addr;
  modules[0].jt = MODULE_JUMPTABLE;
  modules[0].execution_offset = offset;
#ifdef ESP8266
  modules[0].mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[0].mod_addr + 4;
#else
  modules[0].mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[0].mod_addr + 8;
#endif
  modules[0].settings = Settings;

  modules[0].flags.data = 0;

  plugins.free_flash_start = (uint32_t)modules[0].mod_addr;
  plugins.free_flash_end = plugins.free_flash_start + SPI_FLASH_SEC_SIZE;
  plugins.pagesize = SPI_FLASH_SEC_SIZE;
  plugins.flashbase = 0;

#else
  AddModules();
#endif // EXECUTE_FROM_BINARY
}


void Module_Execute(uint32_t sel) {
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.every_second) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        //fm->mod_func_execute(sel);
        SET_MOD_REG(cnt);
        MOD_EXEC(sel);
      }
    }
  }
}

bool Module_Command(uint32_t sel) {
bool result = false;
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        //result = fm->mod_func_execute(sel);
        SET_MOD_REG(cnt);
        result = MOD_EXEC(sel);
        if (result) break;
      }
    }
  }
  return result;
}

void ModuleWebSensor() {
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.web_sensor) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        //fm->mod_func_execute(MODFUNC_WEB_SENSOR);
        SET_MOD_REG(cnt);
        MOD_EXEC(MODFUNC_WEB_SENSOR);
      }
    }
  }
}

void ModuleJsonAppend() {
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized && modules[cnt].flags.json_append) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        //fm->mod_func_execute(MODFUNC_JSON_APPEND);
        SET_MOD_REG(cnt);
        MOD_EXEC(MODFUNC_JSON_APPEND);
      }
    }
  }
}

uint8_t *Load_Module(char *path, uint32_t *rsize) {

 #ifdef USE_UFILESYS
  if (!ffsp) return 0;
  File fp;
  fp = ffsp->open(path, "r");
  if (fp <= 0) return 0;
  uint32_t size = fp.size();
#ifdef ESP8266
  uint8_t *fdesc = (uint8_t *)calloc(size / 4 + 4, 4);
#endif
#ifdef ESP32
  uint8_t *fdesc = (uint8_t *)heap_caps_malloc(size + 4, MALLOC_CAP_EXEC);
#endif
  if (!fdesc) return 0;
  fp.read(fdesc, size);
  fp.close();
  *rsize = size;
  return fdesc;
#else
  return 0;
#endif  
}

uint32_t Module_CheckFree(uint32_t size) {
uint32_t eeprom_block;

  eeprom_block = plugins.free_flash_start;

  // search for free entry
  uint32_t *lp = (uint32_t*) ( plugins.flashbase + plugins.free_flash_start );
  uint32_t addr = plugins.free_flash_start;
  while (addr < plugins.free_flash_end) {
      uint32_t blocksize = SPI_FLASH_SEC_SIZE;
      if (*lp == MODULE_SYNC) {
        // check if name is equal
        //const FLASH_MODULE *fr = (FLASH_MODULE*)lp;
        //const FLASH_MODULE *fd = (FLASH_MODULE*)fdesc;
        //if (!strcmp_P(fd->name, fr->name)) {
          // module already exists
          //eeprom_block = addr;
          //break;
        //}
      
        // skip address by module size
        //blocksize = (fr->size / SPI_FLASH_SEC_SIZE) + 1;
        // must align and increment addr
        //blocksize *= SPI_FLASH_SEC_SIZE;
      } else {
        // free module block, check required size
        uint8_t blocks = (size / SPI_FLASH_SEC_SIZE) + 1;
        uint32_t *bp = lp;
        uint8_t free = 1;
        for (uint32_t cnt = 0; cnt < blocks; cnt++) {
          if (*bp == MODULE_SYNC) {
            free = 0;
          }
          bp += SPI_FLASH_SEC_SIZE;
        }
        if (free) {
          eeprom_block = addr;
          break;
        }
      }
      lp += (blocksize / 4);
      addr += blocksize;
  }
  return eeprom_block;
}

uint32_t Store_Module(uint8_t *fdesc, uint32_t size, uint32_t *ioffset, uint8_t flag, uint8_t index) {
  uint32_t eeprom_block = Module_CheckFree(size);
  if (!eeprom_block) {
    return 0;
  }

#ifdef ESP8266
  uint32_t aoffset = plugins.flashbase;
  uint32_t *lwp=(uint32_t*)fdesc;
  const FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t old_pc = (uint32_t)fm->end_of_module - (size - MODUL_END_OFFSET);
  uint32_t new_pc = (uint32_t)eeprom_block + aoffset;
  uint32_t offset = new_pc - old_pc;
  *ioffset = offset;
  uint32_t corr_pc = (uint32_t)fm->mod_func_execute + offset;
  uint32_t *lp = (uint32_t*)&fm->mod_func_execute;
  *lp = corr_pc;
  lp = (uint32_t*)&fm->end_of_module;
  *lp = (uint32_t)fm->end_of_module + offset;
  lp = (uint32_t*)&fm->execution_offset;
  *lp = offset;
  lp = (uint32_t*)&fm->mtv;
  *lp = (uint32_t)&modules[index];
  lp = (uint32_t*)&fm->jtab;
  *lp = (uint32_t)&MODULE_JUMPTABLE;
#endif

#ifdef ESP32
  FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t old_mptr = (uint32_t)fm->end_of_module - (size - MODUL_END_OFFSET);
  uint32_t offset = eeprom_block - old_mptr;
  fm->execution_offset = offset;
  uint32_t *lp =  (uint32_t*)&fm->mod_func_execute;
  *lp = ((uint32_t)fm->mod_func_execute + offset);
  lp =  (uint32_t*)&fm->end_of_module;
  *lp = ((uint32_t)fm->end_of_module + offset); 
  fm->mtv = (uint32_t)&modules[index];
  fm->jtab = (uint32_t)&MODULE_JUMPTABLE;
  AddLog(LOG_LEVEL_INFO, PSTR("Module offset: %08x:"),old_mptr);
  uint32_t *lwp=(uint32_t*)fdesc;
  uint32_t new_pc = (uint32_t)eeprom_block;
  *ioffset = offset;
#endif

#ifdef ESP8266
//  AddLog(LOG_LEVEL_INFO, PSTR("Module offset %x: %x: %x: %x: %x: %x"),old_pc, new_pc, offset, corr_pc, (uint32_t)fm->mod_func_execute, (uint32_t)&module_header);
  uint8_t blocks = (size / SPI_FLASH_SEC_SIZE) + 1;
  for (uint8_t cnt = 0; cnt < blocks; cnt++) {
    ESP.flashEraseSector(eeprom_block / SPI_FLASH_SEC_SIZE);
    ESP.flashWrite(eeprom_block , lwp, SPI_FLASH_SEC_SIZE);
    lwp += SPI_FLASH_SEC_SIZE / 4;
    eeprom_block += SPI_FLASH_SEC_SIZE;
  }
#endif

#ifdef ESP32
  //AddLog(LOG_LEVEL_INFO, PSTR("save module: %08x, size: %d"),eeprom_block, size);
  esp_err_t err = esp_partition_write(plugins.flash_pptr, eeprom_block - plugins.free_flash_start, (void*)lwp, size);
#endif
  return new_pc;
}

void testcall(void) {
  AddLog(LOG_LEVEL_INFO,PSTR("was called"));
}

void AddModules(void) {
  uint16_t module = 0;
  uint32_t *lp = (uint32_t*) ( plugins.flashbase + plugins.free_flash_start );
  for (uint32_t addr = plugins.free_flash_start; addr < plugins.free_flash_end; addr += plugins.pagesize) {
    //AddLog(LOG_LEVEL_INFO,PSTR("addr, sync %08x: %08x: %04x"),addr,(uint32_t)lp, *lp);
    const volatile FLASH_MODULE *fm = (FLASH_MODULE*)lp;
    if (fm->sync == MODULE_SYNC) {
      // add module
      modules[module].mod_addr = (FLASH_MODULE*)lp;
      modules[module].jt = MODULE_JUMPTABLE;
      modules[module].execution_offset = fm->execution_offset;
      modules[module].mod_size = fm->size;
      modules[module].settings = Settings;
      modules[module].flags.data = 0;
      if (TasmotaGlobal.gpio_optiona.shelly_pro) {
        Init_module(module);
      }
      // add addr according to module size, currently assume module < SPI_FLASH_SEC_SIZE
      module++;
      if (module >= MAX_PLUGINS) {
        break;
      }
    }
    lp += plugins.pagesize/4;
  }
}

const char mod_types[] PROGMEM = "xsns|xlgt|xnrg|xdrv|";

// show all linked modules
void Module_mdir(void) {

   uint32_t *vp = (uint32_t *)calloc(sizeof(FLASH_MODULE) / 4 , 4);
  if (!vp) {
    return;
  }

 #if 1

  Response_P(PSTR("{"));
  uint8_t index = 0;
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      uint32_t *mp = (uint32_t*)modules[cnt].mod_addr;
      for (uint16_t cnt = 0; cnt < sizeof(FLASH_MODULE) / 4; cnt++) {
        vp[cnt] = mp[cnt];
      }
      const FLASH_MODULE *fm = (FLASH_MODULE*)vp;
      const uint32_t volatile mtype = fm->type;
      const uint32_t volatile rev = fm->revision;
      char name[18];
      strncpy(name, fm->name, 16);
      name[15] = 0;
      char type[6];
      GetTextIndexed(type, sizeof(type), mtype, mod_types );
      if (index > 0) {
        ResponseAppend_P(PSTR(","));
      }
      ResponseAppend_P(PSTR("\"MOD #%d\":{\"name\":\"%s\",\"addr\":\"%08x\",\"ex-offs\":\"%08x\", \"size\":%d,\"type\":\"%s\",\"rev\":%d.%d,\"mem\":%d,\"init\":%d}"),cnt + 1, name, modules[cnt].mod_addr, modules[cnt].execution_offset,
       modules[cnt].mod_size, type, (rev>>16),(rev&0xff), modules[cnt].mem_size, modules[cnt].flags.initialized);
       index++;
    }
  }
  ResponseJsonEnd();
 #else 
  AddLog(LOG_LEVEL_INFO, PSTR("| ======== Module directory ========"));
  AddLog(LOG_LEVEL_INFO, PSTR("| nr | name           | address  | size | type | rev  | ram  | init |"));
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
      const uint32_t volatile mtype = fm->type;
      const uint32_t volatile rev = fm->revision;
      // esp32 crashes when fm->name is given as addlog parameter ???, so copy to charbuffer
      // ESP8266 does not crash
      char name[16];
      strncpy(name, fm->name, 16);
      char type[6];
      GetTextIndexed(type, sizeof(type), mtype, mod_types );
      AddLog(LOG_LEVEL_INFO, PSTR("| %2d | %-15s| %08x | %4d | %4s | %04x | %4d |  %1d   |"), cnt + 1, name, modules[cnt].mod_addr,
       modules[cnt].mod_size,  type, rev, modules[cnt].mem_size, modules[cnt].flags.initialized);
      // AddLog(LOG_LEVEL_INFO, PSTR("| %2d | %-16s| %08x | %4d | %4s | %04x | %4d | %1d | %08x"), cnt + 1, fm->name, modules[cnt].mod_addr,
      //  modules[cnt].mod_size,  type, fm->revision, modules[cnt].mem_size, modules[cnt].flags.initialized, fm->execution_offset);

      //AddLog(LOG_LEVEL_INFO, PSTR("Module %d: %s %08x"), cnt + 1, fm->name, modules[cnt].mod_addr);
    }
  }
  #endif
  //ResponseCmndDone();
  free(vp);
}

void LinkModule(uint8_t *mp, uint32_t size, char *name) {
  uint8_t cnt;
  const FLASH_MODULE *fm = (FLASH_MODULE*)mp;

  if (mp) {
    if (fm->sync != MODULE_SYNC) {
      free(mp);
      AddLog(LOG_LEVEL_INFO,PSTR("module sync error"));
      return;
    }

    if (fm->arch != CURR_ARCH) {
      free(mp);
      AddLog(LOG_LEVEL_INFO,PSTR("module architecture error"));
      return;
    }

    Unlink_Named_Module(name);

    uint8_t sfree = 0; 
    for (cnt = 0; cnt < MAX_PLUGINS; cnt++) {
      if (!modules[cnt].mod_addr) {
        sfree = 1;
        break;
      }
    }
    if (!sfree) {
      free(mp);
      AddLog(LOG_LEVEL_INFO,PSTR("no free slot!"));
      return;
    }

    uint32_t offset;
  
#ifdef ESP32
    modules[cnt].mod_addr = (void *) Store_Module(mp, size, &offset, 1, cnt);
    free(mp);
#else
    modules[cnt].mod_addr = (void *) Store_Module(mp, size, &offset, 0, cnt);
    free(mp);
#endif
  
    //AddLog(LOG_LEVEL_INFO,PSTR("module stored in flash at: %08x"),modules[cnt].mod_addr);
    const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
    modules[cnt].jt = MODULE_JUMPTABLE;
    //modules[cnt].mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[cnt].mod_addr + MODUL_END_OFFSET;
    modules[cnt].mod_size = (uint32_t)fm->size;
    modules[cnt].execution_offset = offset;
    modules[cnt].settings = Settings;
    modules[cnt].flags.data = 0;
    
    AddLog(LOG_LEVEL_INFO,PSTR("module %s loaded at slot %d"), name, cnt + 1);
  } else {
    // error
    AddLog(LOG_LEVEL_INFO,PSTR("module error"));
  }
}

void Unlink_Named_Module(char *name) {
  for (uint8_t module = 0; module < MAX_PLUGINS; module++) {
    if (modules[module].mod_addr) {
      // compare name
      const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
      char nam[32];
      strcpy(nam, name);
      char *cp = strchr(nam, '.');
      if (cp) {
        *cp = 0;
      }
      uint32_t lval[4];
      uint32_t *lp = (uint32_t*)&fm->name[0];
      for (uint32_t cnt = 0; cnt < 4; cnt++) {
        lval[cnt] = *lp++;
      }

      if (!strcmp(nam, (char *)lval)) {
        Unlink_Module(module);
      }
    }
  }
}

void Unlink_Module(uint32_t module) {
  if (modules[module].mod_addr) {
    if (modules[module].flags.initialized) {
      // call deiniz
      Deiniz_module(module);
    }
    // remove from module table, erase flash
    if ((uint32_t)modules[module].mod_addr != (uint32_t)&module_header) {
#ifdef ESP8266
      ESP.flashEraseSector(((uint32_t)modules[module].mod_addr - plugins.flashbase) / SPI_FLASH_SEC_SIZE);
#endif
#ifdef ESP32
      esp_err_t err = esp_partition_erase_range(plugins.flash_pptr, (uint32_t)modules[module].mod_addr - plugins.free_flash_start, SPI_FLASH_SEC_SIZE);
#endif
    }
    modules[module].mod_addr = 0;
    AddLog(LOG_LEVEL_INFO,PSTR("module %d unlinked"),module + 1);
  }
}


void Read_Module_Data(uint32_t module, uint32_t *data) {
  if (modules[module].mod_addr) {
    FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
    if (fm->sync == MODULE_SYNC) {
      //AddLog(LOG_LEVEL_INFO,PSTR("read flash data:"));
      for (uint16_t cnt = 0; cnt < MAX_MOD_STORES; cnt++ ) {
        *data = fm->ms[cnt].value;
        data++;
      }
    }
  }
}

void Update_Module_Data(uint32_t module, uint32_t *data) {
  if (modules[module].mod_addr) {
    uint8_t flag = modules[module].flags.initialized;
    if (flag) {
      Deiniz_module(module);
    }
    uint32_t *buff = (uint32_t *)calloc(SPI_FLASH_SEC_SIZE / 4 , 4);
    if (buff) {
#ifdef ESP8266
      ESP.flashRead((uint32_t)modules[module].mod_addr - plugins.flashbase, buff, SPI_FLASH_SEC_SIZE);
      FLASH_MODULE *fm = (FLASH_MODULE*)buff;
      //AddLog(LOG_LEVEL_INFO,PSTR("read flash: %08x"),fm->sync);
      if (fm->sync == MODULE_SYNC) {
        //AddLog(LOG_LEVEL_INFO,PSTR("modify data"));
        for (uint16_t cnt = 0; cnt < MAX_MOD_STORES; cnt++ ) {
          fm->ms[cnt].value = *data++;
        }
        // rewrite modified module
        //AddLog(LOG_LEVEL_INFO,PSTR("write flash"));
        ESP.flashEraseSector(((uint32_t)modules[module].mod_addr - plugins.flashbase) / SPI_FLASH_SEC_SIZE);
        ESP.flashWrite((uint32_t)modules[module].mod_addr - plugins.flashbase, (uint32_t*)(uint32_t*)buff, SPI_FLASH_SEC_SIZE);
      }
#endif
#ifdef ESP32
      esp_err_t err = esp_partition_read(plugins.flash_pptr, (uint32_t)modules[module].mod_addr - plugins.free_flash_start, (void*)buff, SPI_FLASH_SEC_SIZE);
      FLASH_MODULE *fm = (FLASH_MODULE*)buff;
      //AddLog(LOG_LEVEL_INFO,PSTR("read flash: %08x"),fm->sync);
      if (fm->sync == MODULE_SYNC) {
        //AddLog(LOG_LEVEL_INFO,PSTR("modify data"));
        for (uint16_t cnt = 0; cnt < MAX_MOD_STORES; cnt++ ) {
          fm->ms[cnt].value = *data++;
        }
        err = esp_partition_write(plugins.flash_pptr, (uint32_t)modules[module].mod_addr - plugins.free_flash_start, (void*)buff, SPI_FLASH_SEC_SIZE);
      }
#endif
      free(buff);
    
    }
    if (flag) {
      Init_module(module);
    }
  }
}

// link 1 module from file (or web, not yet)
void Module_link(void) {
  uint8_t *fdesc = 0;

  if (XdrvMailbox.data_len) {
    uint32_t size;
#ifdef USE_UFILESYS
    uint8_t *mp = Load_Module(XdrvMailbox.data, &size);
    LinkModule(mp, size, XdrvMailbox.data);
#endif
  }
  ResponseCmndDone();
}

// unlink 1 module
void Module_unlink(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAX_PLUGINS)) {
    uint8_t module = XdrvMailbox.payload - 1;
    Unlink_Module(module);
  } if (XdrvMailbox.payload == 0) {
    for (uint8_t module = 0; module < MAX_PLUGINS; module++) {
      Unlink_Module(module);
    }
  }
  ResponseCmndDone();
}
int32_t mod_func_execute(uint32_t sel);

int32_t Init_module(uint32_t module) {
  if (modules[module].mod_addr && !modules[module].flags.initialized) {
    const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
    uint32_t mtv = fm->mtv;
    uint32_t jtab = fm->jtab;

    if (((uint32_t)&modules[module] != mtv) || ((uint32_t)&MODULE_JUMPTABLE != jtab)) {
      AddLog(LOG_LEVEL_INFO,PSTR("reinit memory link of module %d"), module + 1);
      uint32_t *buff = (uint32_t *)calloc(SPI_FLASH_SEC_SIZE / 4 , 4);
      if (buff) {
#ifdef ESP8266
        ESP.flashRead((uint32_t)modules[module].mod_addr-plugins.flashbase, buff, SPI_FLASH_SEC_SIZE);
        FLASH_MODULE *fm = (FLASH_MODULE*)buff;
        if (fm->sync == MODULE_SYNC) {
          
          uint32_t *lp = (uint32_t*)&fm->mtv;
          *lp = (uint32_t)&modules[module];

          lp = (uint32_t*)&fm->jtab;
          *lp = (uint32_t)&MODULE_JUMPTABLE;

          ESP.flashEraseSector(((uint32_t)modules[module].mod_addr - plugins.flashbase) / SPI_FLASH_SEC_SIZE);
          ESP.flashWrite((uint32_t)modules[module].mod_addr - plugins.flashbase, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
        }
#endif

#ifdef ESP32
        esp_err_t err = esp_partition_read(plugins.flash_pptr, (uint32_t)modules[module].mod_addr - plugins.free_flash_start, (void*)buff, SPI_FLASH_SEC_SIZE);
         FLASH_MODULE *fm = (FLASH_MODULE*)buff;
        if (fm->sync == MODULE_SYNC) {
          uint32_t *lp = (uint32_t*)&fm->mtv;
          *lp = (uint32_t)&modules[module];
          lp = (uint32_t*)&fm->jtab;
          *lp = (uint32_t)&MODULE_JUMPTABLE;
          err = esp_partition_write(plugins.flash_pptr, (uint32_t)modules[module].mod_addr - plugins.free_flash_start, (void*)buff, SPI_FLASH_SEC_SIZE);
        }
#endif
        free(buff);
      }
    }
    //int32_t result = fm->mod_func_execute(MODFUNC_INIT);
    SET_MOD_REG(module);
    int32_t result = MOD_EXEC(MODFUNC_INIT);
    
    modules[module].flags.every_second = 1;
    modules[module].flags.web_sensor = 1;
    modules[module].flags.json_append = 1;
    AddLog(LOG_LEVEL_INFO,PSTR("module %d inizialized: %d"),module + 1, result);
    return 1;
  }
  return 0;
}

// iniz 1 module
void Module_iniz(void) {

  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAX_PLUGINS)) {
    uint8_t module = XdrvMailbox.payload - 1;
    Init_module(module);
  } else if (XdrvMailbox.payload == 0) {
    for (uint8_t module = 0; module < MAX_PLUGINS; module++) {
      Init_module(module);
    }
  }
  ResponseCmndDone();
}

void Deiniz_module(uint32_t module) {
  if (modules[module].mod_addr && modules[module].flags.initialized) {
    const FLASH_MODULE *fm = (FLASH_MODULE*)modules[module].mod_addr;
    //int32_t result = fm->mod_func_execute(FUNC_DEINIT);
    SET_MOD_REG(module);
    int32_t result = MOD_EXEC(FUNC_DEINIT);
    modules[module].flags.data = 0;
    AddLog(LOG_LEVEL_INFO,PSTR("module %d deinizialized"),module + 1);
  }
}

// deiniz 1 module
void Module_deiniz(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAX_PLUGINS)) {
    Deiniz_module(XdrvMailbox.payload - 1);
  }
  ResponseCmndDone();
}

// dump module hex 32 bit words
void Module_dump(void) {
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAX_PLUGINS)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr) {
      uint16_t size = modules[module].mod_size;
      uint32_t *lp = (uint32_t*) modules[module].mod_addr;
      for (uint32_t cnt = 0; cnt < (size / 32) + 1; cnt ++) {
        AddLog(LOG_LEVEL_INFO,PSTR("%08x: %08x %08x %08x %08x %08x %08x %08x %08x"),lp,lp[0],lp[1],lp[2],lp[3],lp[4],lp[5],lp[6],lp[7]);
        lp += 8;
      }
    }
  }
  ResponseCmndDone();
}

const char HTTP_MODULES_CSS[] PROGMEM =
"<head><style>rc{color:red;}gc{color:green;}yc{color:yellow;}</style></head>"
"<table border='3' frame='void' style='width:800px;background-color:#00BFFF;'>"
"<tr align='center';><th>Slot</th><th align='left'>Name</th><th>Type</th><th>Vers</th><th>Size</th><th>RAM</th><th>GPIO</th><th>I</th><th>X</th></tr>";
const char HTTP_MODULES_TEND[] PROGMEM =
"</table>";

const char HTTP_MODULES_COMMONa[] PROGMEM =
"<tr align='center' style ='background-color: #%s'>"
"<td><yc>%02d</yc></td><td align='left'>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%d</td>"; // <td>%s</td>
const char HTTP_MODULES_COMMONc[] PROGMEM =
"<td><input type='checkbox' %s onchange='miva(%d,\"%s\")';></td><td><a href='modu?delete=%d' onclick=\"return confirm('delete module ?')\">&#128293;</a></td>";

const char HTTP_MODULES_SCRIPT[] PROGMEM =
"<script>function miva(par,ivar){"
  //"rfsh=1;"
  "la('&modules='+ivar+'_'+par);"
  //"rfsh=0;
  "setTimeout(function(){"
   "window.location.reload();"
  "}, 500);"
"}";



const char MOD_DIRECTORY[] PROGMEM =
  "<p><form action='" "mo_upl" "' method='get'><button>" "%s" "</button></form></p>";

const char MOD_FORM_FILE_UPG[] PROGMEM =
  "<form method='post' action='modu' enctype='multipart/form-data'>"
  "<br><input type='file' name='modu'><br>"
  "<br><button type='submit' onclick='eb(\"f1\").style.display=\"none\";eb(\"f2\").style.display=\"block\";this.form.submit();'>" D_START " %s</button></form>"
  "<br>";

const char MOD_FORM_FILE_UPGc[] PROGMEM =
  "<div style='text-align:left;color:#%06x;'>" "Max Slots" " %d - " "Free Slots" " %d";

uint16_t MOD_FreeSlots() {
  uint16_t slots = 0;
  for (uint16_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      slots += 1;
    }
  }
  return MAX_PLUGINS - slots;
}

void Modul_Check_HTML_Setvars(void) {

  if (!HttpCheckPriviledgedAccess()) { return; }

  if (Webserver->hasArg(F("modules"))) {
    String stmp = Webserver->arg(F("modules"));
    uint32_t ind;
    char *cp=(char*)stmp.c_str();
    if (!strncmp(cp, "enb", 3)) {
      // enable sensor
      cp += 3;
      ind = strtol(cp, &cp, 10);
      cp++;
      uint8_t enabled = strtol(cp, &cp, 10);
      if (enabled) {
        Init_module(ind); 
      } else {
        Deiniz_module(ind);
      }
    }

  }

  if (Webserver->hasArg(F("sv"))) {
    // set selector
    String stmp = Webserver->arg(F("sv"));
    char *cp = (char*)stmp.c_str();
    if (!strncmp(cp, "sel", 3)) {
      cp += 3;
      uint8_t mind = strtol(cp, &cp, 10);
      cp++;
      uint8_t pinn = strtol(cp, &cp, 10);
      cp++;
      uint8_t pind = strtol(cp, &cp, 10);
      
      // should better update values on closing menu
      uint32_t vals[MAX_MOD_STORES];
      Read_Module_Data(mind, vals);
      uint32_t old = vals[pinn] & 0xff;
      vals[pinn] = (vals[pinn] & 0xffffff00) | pind;
      //AddLog(LOG_LEVEL_INFO,PSTR(">>> %d - %d - %d -> %d"), mind, pinn, old, pind);
      Update_Module_Data(mind, vals);
    }
  }

}

void Module_upload() {

  if (!HttpCheckPriviledgedAccess()) { return; }

  if (Webserver->hasArg(F("delete"))) {
    String stmp = Webserver->arg(F("delete"));
    char *cp = (char*)stmp.c_str();
    // unlink module
    uint8_t module = strtol(cp, &cp, 10);
    Unlink_Module(module - 1);
  }

  WSContentStart_P(PSTR("Plugins Directory"));
  WSContentSendStyle();
  WSContentSend_P(PSTR("Plugins Directory"));

  WSContentSend_P(MOD_FORM_FILE_UPGc, WebColor(COL_TEXT), MAX_PLUGINS, MOD_FreeSlots());

  WSContentSend_P(MOD_FORM_FILE_UPG, PSTR("Plugin upload"));
  
  WSContentSend_P(PSTR("<div>"));
  WSContentSend_P(HTTP_MODULES_SCRIPT);
  WSContentSend_P(HTTP_SCRIPT_ROOT, Settings->web_refresh, Settings->web_refresh);
  WSContentSend_P(PSTR("</script>"));

  WSContentSend_P(HTTP_MODULES_CSS);


  uint32_t *vp = (uint32_t *)calloc(sizeof(FLASH_MODULE) / 4 , 4);
  if (!vp) {
    return;
  }

  for (uint16_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      uint32_t *mp = (uint32_t*)modules[cnt].mod_addr;
      for (uint16_t cnt = 0; cnt < sizeof(FLASH_MODULE) / 4; cnt++) {
        vp[cnt] = mp[cnt];
      }
#if defined(ESP32)
     // const FLASH_MODULE *fm = (const FLASH_MODULE*)vp;
     // esp_err_t err = esp_partition_read(plugins.flash_pptr, (uint32_t)modules[cnt].mod_addr - plugins.free_flash_start, vp, sizeof(FLASH_MODULE));
#endif

      const FLASH_MODULE *fm = (FLASH_MODULE*)vp;

      const uint32_t volatile mtype = fm->type;
      const uint32_t volatile rev = fm->revision;
      char name[16];
      strncpy(name, fm->name, 16);
      char type[6];
      GetTextIndexed(type, sizeof(type), mtype, mod_types );

      char srev[8];
      float frev = (float)(rev >> 16) + (float)(rev & 0xffff)/100;
      dtostrf(frev, 1, 2, srev);
      WSContentSend_P(HTTP_MODULES_COMMONa, "808080", cnt + 1, name, type, srev, modules[cnt].mod_size, modules[cnt].mem_size);

      WSContentSend_P(PSTR("<td>"));
      for (uint8_t xcnt = 0; xcnt < MAX_MOD_STORES; xcnt++) {
        char name[8];
        strncpy(name, fm->ms[xcnt].name, 8);
        if (name[0]) {
          char vn[12];
          sprintf(vn,"sel%d_%d", cnt, xcnt);
          uint32_t val32 = fm->ms[xcnt].value;
          uint8_t selector = val32 >> 24;
          WSContentSend_P(PSTR("<label for=\"p%d_%d\">%s:</label> <select  id=\"p%d_%d\" style='width: 60px;' onchange='seva(value,\"%s\")'>"),cnt,xcnt,name,cnt,xcnt,vn);
          if (!selector) {
            for (uint8_t pins = 0; pins < nitems(TasmotaGlobal.gpio_pin); pins++) {
              char sel[10];
              if ((val32 & 0xff) == pins) {
                strcpy_P(sel, PSTR("selected"));
              } else {
                sel[0] = 0;
              }
              // AddLog(LOG_LEVEL_INFO,PSTR(">>> %d - %d"), pins, TasmotaGlobal.gpio_pin[pins]);
              if (TasmotaGlobal.gpio_pin[pins] == 0) {
                WSContentSend_P(PSTR("<option value=\"%d\" %s>%d</option>"), pins, sel, pins);
              }
            }
          } else {
            // selector 1
            uint8_t from = val32 >> 16;
            uint8_t to = val32 >> 8;
            for (uint8_t pins = from; pins <= to; pins++) {
              char sel[10];
              if ((val32 & 0xff) == pins) {
                strcpy_P(sel, PSTR("selected"));
              } else {
                sel[0] = 0;
              }
              // AddLog(LOG_LEVEL_INFO,PSTR(">>> %d - %d"), pins, TasmotaGlobal.gpio_pin[pins]);
              WSContentSend_P(PSTR("<option value=\"%d\" %s>%d</option>"), pins, sel, pins);
            }
          }
          WSContentSend_P(PSTR("</select><br>"));
        }
      }
      WSContentSend_P(PSTR("</td>"));
      const char *cp;
      uint8_t uval;
      if (modules[cnt].flags.initialized) {
        cp = "checked='checked'";
        uval = 0;
      } else {
        cp = "";
        uval = 1;
      }
      char enblid[8];
      sprintf_P(enblid,PSTR("enb%d"),cnt);
      WSContentSend_P(HTTP_MODULES_COMMONc, cp, uval, enblid, cnt + 1);

    }
  }
  free(vp);      

  WSContentSend_P(HTTP_MODULES_TEND);

  WSContentSend_P(PSTR("</div>"));
  
  WSContentSpaceButton(BUTTON_MANAGEMENT);
  WSContentStop();
  
  Webserver->sendHeader(F("Location"),F("/modu"));
  Webserver->send(303);  
}

bool Module_upload_start(const char* upload_filename) {
  strlcpy(plugins.mod_name, upload_filename, sizeof(plugins.mod_name));
  plugins.module_bytes_read = 0;
  return true;
}

bool Module_upload_write(uint8_t *upload_buf, size_t current_size) {

  if (0 == plugins.module_bytes_read) {
    // 1. block
    FLASH_MODULE *fm = (FLASH_MODULE*)upload_buf;
    plugins.module_size = fm->size;
    uint32_t size = (fm->size / SPI_FLASH_SEC_SIZE) + 1 ;
    size *= SPI_FLASH_SEC_SIZE;
    plugins.module_input_buffer = (uint8_t *)calloc(size / 4 + 4, 4);
    if (!plugins.module_input_buffer) {
      return false;
    }
    plugins.module_input_ptr = plugins.module_input_buffer;
    //Module_CheckFree(size, upload.filename.c_str());
  }

  if ((plugins.module_size - plugins.module_bytes_read) > current_size) {
    memcpy(plugins.module_input_ptr, upload_buf, current_size);
    plugins.module_bytes_read += current_size;
    plugins.module_input_ptr += current_size;
    return true;
  } else {
    current_size = plugins.module_size - plugins.module_bytes_read;
    memcpy(plugins.module_input_ptr, upload_buf, current_size);
    plugins.module_bytes_read += current_size;
    plugins.module_input_ptr += current_size;
    return false;
  }
}

void Module_upload_stop(void) {
  if (plugins.module_input_buffer) {
    LinkModule(plugins.module_input_buffer, plugins.module_bytes_read, plugins.mod_name);
  }
}


void Module_HandleUploadLoop(void) {

  if (HTTP_USER == Web.state) { return; }
    
  HTTPUpload& upload = Webserver->upload();

  switch (upload.status) {
    case UPLOAD_FILE_START:
    // ***** Step1: Start upload file
      Module_upload_start(upload.filename.c_str());
      break;
    case UPLOAD_FILE_WRITE:
    // ***** Step2: Write upload file
      Module_upload_write(upload.buf, upload.currentSize);
      break;
    case UPLOAD_FILE_END:
    // ***** Step3: Finish upload file
      Module_upload_stop();
      break;
  }
}
  
/* =========================================================== */
// BINDIR section
/* =========================================================== */
#ifdef USE_FLASH_BDIR
struct BINDIR {
uint32_t address;
uint32_t size;
} bindir;

void BinDir_list(void) {
#ifdef USE_FLASH_BDIR
  flash_bindir(0, (char*)"");
  flash_bindir(1, (char*)"");
#endif
  ResponseCmndDone();
}

#define MODULE_SYNC 0x55aaFC4A
// 32 bytes header
typedef struct {
  uint32_t sync;
  uint32_t arch; // architecture EPS8266, ESP32 variants
  uint32_t type; 
  uint32_t revision;
  char name[16];
  uint32_t dummy1;
  uint32_t dummy2;
  uint32_t size; // size of payload
  uint16_t execution_offset; // execution offset, normally 32
  uint16_t CRC; // checksum over payload
} FLASH_DATA_MODULE;


enum {DATA_TYPE_SENSOR, DATA_TYPE_LIGHT, DATA_TYPE_ENERGY, DATA_TYPE_DRIVER, DATA_TYPE_SCRIPT, DATA_TYPE_BERRY};
//enum {ARCH_ESP8266, ARCH_ESP32, ARCH_ESP32S3, ARCH_ESP32C3};


uint32_t flash_getbsiz(uint32_t size) {
uint32_t psiz = (size + sizeof(FLASH_DATA_MODULE)) / SPI_FLASH_SEC_SIZE;
  if ((size + sizeof(FLASH_DATA_MODULE)) % SPI_FLASH_SEC_SIZE) {
    psiz += 1;
  }
  psiz *= SPI_FLASH_SEC_SIZE;
  return psiz;
}

int32_t flash_bindir(uint8_t sel, char *path) {
  switch (sel) {
    case 0:
#ifdef ESP32
      // init
      const esp_partition_t *part;
      part = esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, "binary");
      if (part) {
        bindir.address = part->address;
        bindir.size = part->size;
        return bindir.size;
      } else {
        bindir.address = 0;
        bindir.size = 0;
        return 0;
      }
#endif
#ifdef ESP8266
      {
        uint32_t chipsize = ESP.getFlashChipSize();
        bindir.address =  (ESP_getSketchSize() + SPI_FLASH_SEC_SIZE) & (SPI_FLASH_SEC_SIZE-1^0xffffffff);
        bindir.size = ESP.getFreeSketchSpace();
      }
#endif
      break;
    case 1:
      // list
      {
        uint8_t *buff = (uint8_t*)malloc(SPI_FLASH_SEC_SIZE);
        if (buff) {
          FLASH_DATA_MODULE *fm;
          int32_t tsize = bindir.size;
          uint32_t addr = bindir.address;
          uint32_t psiz;
          uint16_t entry = 0;
          AddLog(LOG_LEVEL_INFO,PSTR("Partition (%08x - %d kb)"), bindir.address, bindir.size / 1024);
          while (tsize> 0) {
            ESP.flashRead(addr, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
            fm = (FLASH_DATA_MODULE*)buff;
            if (fm->sync == MODULE_SYNC) {
              entry += 1;
              AddLog(LOG_LEVEL_INFO,PSTR("entry-%02d %s - %08x - %d bytes"), entry, fm->name, addr, fm->size);
              psiz = flash_getbsiz(fm->size);
            } else {
              psiz = SPI_FLASH_SEC_SIZE;
            }          
            tsize -= psiz;
            addr += psiz;
          }
          free(buff);
        }
      }
      break;
    case 2:
      // write, copy from file system
      {
        // find free entry
        uint8_t *buff = (uint8_t*)malloc(SPI_FLASH_SEC_SIZE);
        if (!buff) {
          return -1;
        }
        FLASH_DATA_MODULE *fm;
        int32_t tsize = bindir.size;
        uint32_t addr = bindir.address;
        uint32_t psiz;
        while (tsize> 0) {
#ifdef ESP8266
          ESP.flashRead(addr, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
#endif
          fm = (FLASH_DATA_MODULE*)buff;
          if (fm->sync == MODULE_SYNC) {
            if (!strcmp(fm->name, path)) {
              // replace
              break;
            }
            psiz = flash_getbsiz(fm->size);
          } else {
            break;
          }
          tsize -= psiz;
          addr += psiz;
        }
        File file = ufsp->open(path, FS_FILE_READ);
        if (file) {
          int32_t size = file.size();
          FLASH_DATA_MODULE fm;
          fm.sync = MODULE_SYNC;
#ifdef ESP8266
          fm.arch = 0;
#else          
          fm.arch = 0;
#endif
          fm.type = 0;
          fm.revision = 0;
          strncpy(fm.name, path, sizeof(fm.name));
          fm.size = size;
          fm.execution_offset = 32;
          fm.CRC = 0;
          memcpy(buff, (uint8_t*)&fm, sizeof(FLASH_DATA_MODULE));
          uint16_t s = file.read(buff + sizeof(FLASH_DATA_MODULE), SPI_FLASH_SEC_SIZE - sizeof(FLASH_DATA_MODULE));
          size -= s;
#ifdef ESP8266
          ESP.flashEraseSector(addr / SPI_FLASH_SEC_SIZE);
          ESP.flashWrite(addr, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
          addr += SPI_FLASH_SEC_SIZE;
          while (size > 0) {
            uint16_t s = file.read(buff, SPI_FLASH_SEC_SIZE);
            ESP.flashEraseSector(addr / SPI_FLASH_SEC_SIZE);
            ESP.flashWrite(addr, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
            size -= s;
          }
#endif
          free(buff);
          file.close();
          return 0;
        } else {
          free(buff);
          AddLog(LOG_LEVEL_INFO,PSTR("File %s not found"), path);
        }
      }
      break;
    case 3:
      // get execution address and size
      {
        uint8_t *buff = (uint8_t*)malloc(SPI_FLASH_SEC_SIZE);
        if (buff) {
          FLASH_DATA_MODULE *fm;
          int32_t tsize = bindir.size;
          uint32_t addr = bindir.address;
          uint32_t psiz;
          while (tsize> 0) {
#ifdef ESP8266
            ESP.flashRead(addr, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
#endif
            fm = (FLASH_DATA_MODULE*)buff;
            if (fm->sync == MODULE_SYNC) {
              if (!strcmp(fm->name, path)) {
                AddLog(LOG_LEVEL_INFO,PSTR(">>>> found %s - %d - %08x"), fm->name, fm->size, addr);
                break;
              }
              psiz = flash_getbsiz(fm->size);
            } else {
              psiz = SPI_FLASH_SEC_SIZE;
            }
            tsize -= psiz;
            addr += psiz;
          }
          free(buff);
        }
      }
      break;
  }

  return 0;
}
#endif // USE_FLASH_BDIR
/* =========================================================== */
// end BINDIR section
/* =========================================================== */


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv123(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_COMMAND:
    if (plugins.ready) {
        result = DecodeCommand(kModuleCommands, ModuleCommand);
        if (!result) {
          result = Module_Command(FUNC_COMMAND);
        }
      }
      break;
    case FUNC_INIT:
      InitModules();
      break;
    case FUNC_EVERY_100_MSECOND:
    case FUNC_EVERY_250_MSECOND:
    case FUNC_EVERY_SECOND:
      if (plugins.ready) {
        Module_Execute(function);
      }
      break;
    case FUNC_WEB_SENSOR:
      if (plugins.ready) {
        Modul_Check_HTML_Setvars();
        ModuleWebSensor();
      }
      break;
    case FUNC_JSON_APPEND:
      if (plugins.ready) {
        ModuleJsonAppend();
      }
      break;
    case FUNC_WEB_ADD_MANAGEMENT_BUTTON:
      if (plugins.ready) {
        if (XdrvMailbox.index) {
          XdrvMailbox.index++;
        } else {
          WSContentSend_P(MOD_DIRECTORY, PSTR("Plugins directory"));
        }
      }
      break;
    case FUNC_WEB_ADD_HANDLER:
      if (plugins.ready) {
        Webserver->on("/mo_upl", Module_upload);
        Webserver->on("/modu", HTTP_GET, Module_upload);
        Webserver->on("/modu", HTTP_POST,[](){Webserver->sendHeader(F("Location"),F("/modu"));Webserver->send(303);}, Module_HandleUploadLoop);
      }
      break;
      
    case FUNC_ACTIVE:
      result = true;
      break;
  }
  return result;
}


#endif  // USE_BINPLUGINS
