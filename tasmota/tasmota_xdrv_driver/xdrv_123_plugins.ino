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

#include "./Plugins/modules_def.h"
#include <TasmotaSerial.h>

// minimal plugin rev
#define MINREV 0x00010004
#define CURR_MINREV 0x00010005

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
  "dump" "|"
  "chkp"
#ifdef USE_FLASH_BDIR 
  "|" "list"
#endif
  ;

void (* const ModuleCommand[])(void) PROGMEM = {
  &Module_mdir,  &Module_link, &Module_unlink, &Module_iniz, &Module_deiniz, &Module_dump, &Check_partition
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
bool MT_DecodeCommand(const char* haystack, void (* const InCommand[])(void), MODULES_TABLE *mt);
size_t tmod_write1TS(TasmotaSerial *ts, uint8_t val);
#ifdef ESP32
void twi_readFrom(uint8_t address, uint8_t* data, uint8_t length);
#endif
bool tmod_I2cSetDevice(uint32_t addr, uint32_t bus);
void tmod_I2cSetActiveFound(uint32_t addr, const char *types, uint32_t bus);
int tmod_strncasecmp_P(const char* s1, const char *s2, size_t len);
char *copyStr(const char * str);
void tmod_setClockStretchLimit(TwoWire *wp, uint32_t val);
void tmod_writen(TwoWire *wp, uint8_t *buf, uint32_t len);
int tmod_snprintf_P(char *s, size_t n,  const char *format, ...);
int tmod_sprintf_P(char *s, const char *format, ...);
int tmod_ResponseAppend_P(const char* format, ...);
void tmod_WSContentSend_PD(const char* format, ...);
void tmod_WSContentSend_P(const char* format, ...);
float fl_const(int32_t m, int32_t d);
char *tm_trim(char *s);
void tmod_vTaskEnterCritical( void * );
void tmod_vTaskExitCritical( void * );
uint32_t IRAM_ATTR tmod_directRead(uint32_t pin);
void IRAM_ATTR tmod_directWriteLow(uint32_t pin);
void IRAM_ATTR tmod_directWriteHigh(uint32_t pin);
void IRAM_ATTR tmod_directModeInput(uint32_t pin);
void IRAM_ATTR tmod_directModeOutput(uint32_t pin);
char * tmod_GetTextIndexed(char* destination, size_t destination_size, uint32_t index, const char* haystack);
bool WebServer_hasArg(const char * str);
void tmod_WSContentStart_P(const char* title);
char * tmod_strcpy_P(char *dst , const char *src);
char * tmod_strncpy_P(char *dst , const char *src, size_t len);
void tmod_WebServer_on(const char * prefix, void (*func)(void), uint8_t method);
void *tmod_gtbl(void);
SPIClass *tmod_getspi(uint8_t sel);
void tmod_spi_begin(SPIClass *spi, uint8_t flg, int8_t sck, int8_t miso, int8_t mosi);
void tmod_spi_write(SPIClass *spi, uint8_t data);
void tmod_spi_writebytes(SPIClass *spi, const uint8_t * data, uint32_t size);
void tmod_Transaction(SPIClass *spi, uint8_t flg, uint32_t spibaud);
uint8_t tmod_transfer(SPIClass *spi, uint8_t data);
char* ftostrfd(float number, unsigned char prec, char *s);
class File * tmod_file_open(char *path, char mode);
void tmod_file_close(class File *fp);
int32_t tmod_file_seek(class File *fp, uint32_t pos, uint32_t mode);
int32_t tmod_file_read(class File *fp, uint8_t *buff, uint32_t size);
int32_t tmod_file_write(class File *fp, uint8_t *buff, uint32_t size);
uint32_t tmod_file_size(class File *fp);
uint32_t tmod_file_pos(class File *fp);
void tmod_AddLogData(uint32_t loglevel, const char* log_data);
char *Plugin_Get_SensorNames(char *type, uint32_t index);
char *tmod_Run_Scripter(char *sect);
double tmod_double_dispatch(uint32_t sel, double a, double b);
uint32_t tmod_task_create(TASKPARS *tp);
int64_t tmod_double2long(double in);
double tmod_long2double(int64_t in);

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
  JMPTBL&AddLog,
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&ResponseAppend_P,
  JMPTBL&WSContentSend_PD,
#else
  JMPTBL&tmod_ResponseAppend_P,
  JMPTBL&tmod_WSContentSend_PD,
#endif
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
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&snprintf_P,
#else
  JMPTBL&tmod_snprintf_P,
#endif
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
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&GetTextIndexed,
#else
  JMPTBL&tmod_GetTextIndexed,
#endif
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
  JMPTBL&tmod_GetCommandCode,
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
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&ResponseTime_P,
#else
  JMPTBL&tmod_ResponseTime_P,
#endif
  JMPTBL&ClaimSerial,
  JMPTBL&hardwareSerialTS,
  JMPTBL&millis,
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&sprintf_P,
#else
  JMPTBL&tmod_sprintf_P,
#endif
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
  JMPTBL&MT_DecodeCommand,
  JMPTBL&ResponseCmndDone,
  JMPTBL&tmod_write1TS,
  JMPTBL&memcmp_P,
  JMPTBL&ToHex_P,
  JMPTBL&memset,
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&memmove_P,
#else
  JMPTBL&tmod_memmove_P,
#endif
  JMPTBL&ResponseCmndNumber,
  JMPTBL&ResponseCmndFloat,
  JMPTBL&ResponseAppendTHD,
  JMPTBL&WSContentSend_THD,
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&strncpy_P,
#else
  JMPTBL&tmod_strncpy_P,
#endif
  JMPTBL&isprint,
  JMPTBL&tmod_isinf,
  JMPTBL&copyStr,
  JMPTBL&tmod_setClockStretchLimit,
  JMPTBL&tmod_writen,
  JMPTBL&modff,
  JMPTBL&fl_const,
  JMPTBL&WSContentSend_Temp,
  JMPTBL&delayMicroseconds,
  JMPTBL&digitalRead,
  JMPTBL&digitalWrite,
  JMPTBL&pinMode,
  JMPTBL&strchr,
  JMPTBL&tm_trim,
  JMPTBL&tmod_vTaskEnterCritical,
  JMPTBL&tmod_vTaskExitCritical,
  JMPTBL&tmod_directRead,
  JMPTBL&tmod_directWriteLow,
  JMPTBL&tmod_directWriteHigh,
  JMPTBL&tmod_directModeInput,
  JMPTBL&tmod_directModeOutput,
  JMPTBL&CalcTempHumToAbsHum,
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&WSContentSend_P,
#else
  JMPTBL&tmod_WSContentSend_P,
#endif
  JMPTBL&HttpCheckPriviledgedAccess,
  JMPTBL&tmod_WSContentStart_P,
  JMPTBL&WSContentSendStyle,
  JMPTBL&WSContentSpaceButton,
  JMPTBL&WSContentStop,
  JMPTBL&tmod_WebGetArg,
  JMPTBL&WebRestart,
  JMPTBL&WebServer_hasArg,
  JMPTBL&tmod_WebServer_on,
  JMPTBL&atoi,
  JMPTBL&tmod_strcpy_P,
  JMPTBL&SetTasmotaGlobal,
  JMPTBL&tmod_fixsfti,
  JMPTBL&tmod_gtbl,
  JMPTBL&Settings,
  JMPTBL&tmod_getspi,
  JMPTBL&tmod_spi_begin,
  JMPTBL&tmod_spi_write,
  JMPTBL&tmod_spi_writebytes,
  JMPTBL&tmod_Transaction,
  JMPTBL&tmod_transfer,
  JMPTBL&tmod_file_open,
  JMPTBL&tmod_file_close,
  JMPTBL&tmod_file_seek,
  JMPTBL&tmod_file_read,
  JMPTBL&tmod_file_write,
  JMPTBL&CharToFloat,
  JMPTBL&tmod_AddLogData,
  JMPTBL&tmod_file_exists,
#if defined(ESP8266) || defined(__riscv)
  JMPTBL&strncmp_P,
#else
  JMPTBL&tmod_strncmp_P,
#endif
  JMPTBL&special_malloc,
  JMPTBL&ResponseCmndChar,
  JMPTBL&strtol,
  JMPTBL&tmod_udp,
  JMPTBL&tmod_i2s,
#ifdef ESP32
  JMPTBL&tmod_task_create,
  JMPTBL&tmod_task_delete,
#else
  JMPTBL&tmod_dummy,
  JMPTBL&tmod_dummy,
#endif
  JMPTBL&Plugin_Get_SensorNames,
  JMPTBL&tmod_Run_Scripter,
  JMPTBL&tmod_file_size,
  JMPTBL&tmod_file_pos,
  JMPTBL&OsWatchLoop,
  JMPTBL&tmod_double_dispatch,
  JMPTBL&tmod_double2long,
  JMPTBL&tmod_long2double,
  JMPTBL&MqttPublishSensor,
  JMPTBL&ParseParameters,
  JMPTBL&tmod__modsi3,
  JMPTBL&tmod__ashldi3,
  JMPTBL&tmod__lshrdi3,
  JMPTBL&tmod_wifi
};


#define USE_DOUBLE_DISPATCH


double tmod_double_dispatch(uint32_t sel, double a, double b) {
  double result = 0;
#ifdef USE_DOUBLE_DISPATCH 
  switch (sel) {
    case 0:
      result = a + b;
      break;
    case 1:
      result = a - b;
      break;
    case 2:
      result = a * b;
      break;
    case 3:
      result = a / b;
      break;
  }
#endif
  return result;
}

int64_t tmod_double2long(double in) {
  return in;
}

double tmod_long2double(int64_t in) {
  return in;
}


char *tmod_Run_Scripter(char *sect) {
  uint8_t meter_script = Run_Scripter(sect, -2, 0);
  if (meter_script != 99) {
    return nullptr;
  }
  return glob_script_mem.section_ptr;
}

#ifdef ESP32
uint32_t tmod_task_create(TASKPARS *tp) {
  uint32_t result;
  char *cp = copyStr(tp->constpcName);
  //AddLog(LOG_LEVEL_INFO,PSTR("task Init %s - %d"), cp, tp->usStackDepth);

  result = xTaskCreatePinnedToCore(tp->pvTaskCode, cp, tp->usStackDepth, tp->constpvParameters, (UBaseType_t)tp->uxPriority, (TaskHandle_t*)tp->constpvCreatedTask, (const BaseType_t)tp->xCoreID);
  free(cp);
  return result;
}
uint32_t tmod_task_delete(uint32_t xTaskToDelete) {
  vTaskDelete((TaskHandle_t)xTaskToDelete);
  return 0;
}
#endif

uint32_t tmod_dummy() {
  return 0;
}

//WiFiClient xclient;

uint32_t tmod_wifi(uint32_t sel, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4) {
#ifdef ESP32
  WiFiClient *client =(WiFiClient*) p1;
  BearSSL::WiFiClientSecure_light *sclient =(BearSSL::WiFiClientSecure_light*) p1;
  HTTPClient *http = (HTTPClient*) p1;
  switch (sel) {
    case 0:
      client = new WiFiClient;
      //AddLog(LOG_LEVEL_INFO,PSTR(">>> %8x"),(uint32_t)client);
      return (uint32_t)client;
    case 1:
    {
      int32_t err = client->connect((char*)p2, p3);
      return err;
    }
    case 2:
      return client->connected();
    case 3:
      return client->available();
    case 4:
      return client->read();
    case 5:
      return client->read((uint8_t*)p2, p3);
    case 6:
      client->stop();
      break;
    case 7:
      delete client;
      break;

 #ifdef ESP32
    case 10:
      sclient = new BearSSL::WiFiClientSecure_light(1024,1024);;
      return (uint32_t)sclient;
    case 11:
    {
      int32_t err = sclient->connect((char*)p2, p3);
      return err;
    }
    case 12:
      return sclient->connected();
    case 13:
      return sclient->available();
    case 14:
      return sclient->read();
    case 15:
      return sclient->read((uint8_t*)p2, p3);
    case 16:
      sclient->stop();
      break;
    case 17:
      delete sclient;
      break;
   case 18:
      sclient->setInsecure();
      break;
    case 19:
      sclient->setTimeout(p2);
#endif

    // class http
    case 30:
      http = new HTTPClient;
      return (uint32_t)http;
    case 31:
      http->end();
      break;
    case 32:
      delete http;
      break;
    case 33:
      {
      WiFiClient *client = (WiFiClient*)p2;
      //AddLog(LOG_LEVEL_INFO,PSTR(">>> %8x"),(uint32_t)client);
      return http->begin(*client, (char*)p3);
      }
    case 34:
      http->setReuse(p2);
      break;
    case 35:
      return http->GET();
    case 36:
      return http->getSize();
    case 37:
      return http->connected();
    case 38:
      // returns client
      return (uint32_t)http->getStreamPtr();
    case 39:
      { 
        char *cp2 = copyStr((char*)p2);
        char *cp3 = copyStr((char*)p3);
        http->addHeader((const char*)cp2, (const char*)cp3);
        free(cp2);
        free(cp3);
        break;
      }
    case 40:
      { // gets array of char pointers without execoffset
        const char *hdr[8];
        const char **sap = (const char**)p2;
        if (p3 > 8) p3  = 8;
        MODULES_TABLE *mt = (MODULES_TABLE *)p4;
        for (uint32_t cnt = 0; cnt < p3; cnt++) {
          hdr[cnt] = sap[cnt];
          hdr[cnt] += EXEC_OFFSET;
          hdr[cnt] = copyStr(hdr[cnt]);
        }
        http->collectHeaders(hdr, p3);
        for (uint32_t cnt = 0; cnt < p3; cnt++) {
          free((void*)hdr[cnt]);
        }
      }
      break;
    case 41:
      {
      char *cp = copyStr((char*)p2);
      String hd = http->header(cp);
      free(cp);
      return (uint32_t) hd.c_str();
      }
    case 42:
      {
        char *cp = copyStr((char*)p2);
        bool hd = http->hasHeader(cp);
        free(cp);
        return hd;
      }
    case 43:
      http->setFollowRedirects((followRedirects_t)p2);
      break;
    case 44:
      {
      //return http->begin(xclient, (char*)p3);
      }
      break;
  }
#endif // ESP32
  return 0;
}

#ifdef ESP8266
#include <i2s.h>
#endif
#ifdef ESP32
#if ESP_IDF_VERSION_MAJOR >= 5
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"
#else
#include <driver/i2s.h>
#endif
#endif

uint32_t tmod_i2s(uint32_t sel, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5) {
#ifdef ESP32
i2s_chan_handle_t tx_handle = (i2s_chan_handle_t)p1;
#endif

  switch (sel) {
    case 0:
#ifdef ESP8266
      i2s_begin();
      return 0;
#endif
#ifdef ESP32
      {
      i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
      /* Allocate a new TX channel and get the handle of this channel */
      i2s_new_channel(&chan_cfg, &tx_handle, NULL);

      i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(8000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = (gpio_num_t)p3,
          .ws = (gpio_num_t)p4,
          .dout = (gpio_num_t)p2,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
          },
        },
      };

      i2s_slot_mode_t channels;
      if (0 == (p5 >> 16)) {
        channels = I2S_SLOT_MODE_MONO;
      } else {
        channels = I2S_SLOT_MODE_STEREO;
      }
      uint8_t mode = p5 & 3;
      if (mode > 2) mode = 2;
      switch (mode) {
        case 0:
          std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, channels);
          break;
        case 1:
          std_cfg.slot_cfg = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, channels);
          break;
        case 2:
          std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, channels);
          break;
      }

      /* Initialize the channel */
      i2s_channel_init_std_mode(tx_handle, &std_cfg);
      /* Before writing data, start the TX channel first */
      i2s_channel_enable(tx_handle);
      //AddLog(LOG_LEVEL_INFO,PSTR("I2S Init %d - %d - %d"), p2, p3, p4);
      return (uint32_t)tx_handle;
      }
      
#endif
      break;
    case 1:
#ifdef ESP8266
      i2s_end();
#endif
#ifdef ESP32
      {
      i2s_channel_disable(tx_handle);
      i2s_del_channel(tx_handle);
      //AddLog(LOG_LEVEL_INFO,PSTR("I2S Exit"));
      }
#endif
      break;
    case 2:
#ifdef ESP8266
      i2s_set_rate(p2);
#endif
#ifdef ESP32
      {
      i2s_channel_disable(tx_handle);

      i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(p2);
      i2s_channel_reconfig_std_clock(tx_handle, &clk_cfg);

      i2s_std_slot_config_t slot_cfg;

      i2s_slot_mode_t channels;
      if (1 == p4) {
        channels = I2S_SLOT_MODE_MONO;
      } else {
        channels = I2S_SLOT_MODE_STEREO;
      }
      uint8_t mode = p3 & 3;
      if (mode > 2) mode = 2;
      switch (mode) {
        case 0:
          slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, channels);
          break;
        case 1:
          slot_cfg = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, channels);
          break;
        case 2:
          slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, channels);
          break;
      }
      i2s_channel_reconfig_std_slot(tx_handle, &slot_cfg);

      i2s_channel_enable(tx_handle);
      //AddLog(LOG_LEVEL_INFO,PSTR("I2S Setrate %d"), p2);
      }
#endif
      break;
    case 3:
      // write samples
#ifdef ESP8266
      { 
        /*
        int16_t *left = (int16_t*)p2;
        int16_t *right = (int16_t*)p2 + 2;
        for (uint32_t cnt = 0; cnt < (p3 >> 2); cnt++) {
          i2s_write_lr(*left++, *right++);
        }
        *(uint32_t*)p4 = p3;
        */
        int16_t *swp = (int16_t*)p2;
        for (uint32_t cnt = 0; cnt < p3; cnt++) {
          i2s_write_sample(*swp++);
        }
      }
#endif 
#ifdef ESP32
      i2s_channel_write(tx_handle, (uint8_t *)p2, p3 * 2, nullptr, 100);
#endif
      break;
    case 4:
      // read samples
#ifdef ESP8266
      return i2s_read_sample((int16_t *)p2, (int16_t *)p3, p4); 
#endif

#ifdef ESP32
#endif
      break;
    case 5:
      // write one sample
#ifdef ESP8266
      i2s_write_sample(p2);
#endif // ESP8266
#ifdef ESP32
      {
        int16_t src_buf = p2;
        i2s_channel_write(tx_handle, &src_buf, 2, nullptr, 5);
      }
      break;
#endif // ESP32
    case 6:
#ifdef ESP32
      return i2s_channel_enable(tx_handle);
#endif
      break;
    case 7:
#ifdef ESP32
      return i2s_channel_disable(tx_handle);
#endif
      break;
  }
  return 0;
}


uint32_t tmod_udp(WiFiUDP *udp, uint32_t sel, uint32_t p1, uint32_t p2) {
  switch (sel & 0xff) {
    case 0:
      udp = new WiFiUDP;
      return (uint32_t)udp;
    case 1:
      udp->stop();
      break;
    case 2:
      return udp->begin(p1);
    case 3:
      return udp->parsePacket();
    case 4:
      return udp->available();
    case 5:
      return udp->read((uint8_t *)p1, p2);
    case 6:
      udp->flush();
      break;
    case 7:
      return udp->beginPacket(p1, p2);
    case 8:
      return udp->write((const uint8_t*)p1, p2);
    case 9:
      return udp->endPacket();
    case 99:
      udp->stop();
      delete udp;
      break;
  }
  return 0;
}

int tmod_strncmp_P(const char * str1P, const char * str2P, size_t size) {
  char *cp = copyStr(str2P);
  int res = strncmp(str1P, cp, size);
  free(cp);
  return res;
}


uint32_t tmod_file_exists(const char *path) {
  int32_t result = 0;
#ifdef USE_UFILESYS
  char *cpath = copyStr(path);
#ifdef USE_SCRIPT  
  FS *cfp = script_file_path(cpath);
#else
  FS *cfp = ufsp;
#endif
  result = cfp->exists(cpath);
  free(cpath);
#endif // USE_UFILESYS
  return result;
}


void tmod_AddLogData(uint32_t loglevel, const char* log_data) {
  AddLogData(loglevel,log_data);
}

static File temp_file;

class File *tmod_file_open(char *path, char mode) {
#ifdef USE_UFILESYS
  char *cpath = copyStr(path);
#ifdef USE_SCRIPT  
  FS *cfp = script_file_path(cpath);
#else
  FS *cfp = ufsp;
#endif
  switch (mode) {
    case 'r':
      temp_file = cfp->open(cpath, FS_FILE_READ);
      break;
    case 'w':
      temp_file = cfp->open(cpath, FS_FILE_WRITE);
      break;
    case 'a':
      temp_file = cfp->open(cpath, FS_FILE_APPEND);
      break;
    case 'u':
      temp_file = cfp->open(cpath, "w+");
      break;
    case 'U':
      temp_file = cfp->open(cpath, "r+");
      break;
  }
  free(cpath);
  if (temp_file > 0) {
    return &temp_file;
  } else {
    return nullptr;
  }
#else
  return nullptr;
#endif // USE_UFILESYS
}

void tmod_file_close(class File *fp) {
#ifdef USE_UFILESYS
  fp->close();
#endif
}

int32_t tmod_file_seek(class File *fp, uint32_t pos, uint32_t mode) {
#ifdef USE_UFILESYS
  return fp->seek(pos, (fs::SeekMode)mode);
#else
  return 0;
#endif
}
int32_t tmod_file_read(class File *fp, uint8_t *buff, uint32_t size) {
#ifdef USE_UFILESYS
  return fp->read(buff, size);
#else
  return 0;
#endif
}
int32_t tmod_file_write(class File *fp, uint8_t *buff, uint32_t size) {
#ifdef USE_UFILESYS
  return fp->write(buff, size);
#else
  return 0;
#endif
}

uint32_t tmod_file_size(class File *fp) {
#ifdef USE_UFILESYS
  return fp->size();
#else
  return 0;
#endif
}

uint32_t tmod_file_pos(class File *fp) {
#ifdef USE_UFILESYS
  return fp->position();
#else
  return 0;
#endif
}


SPIClass *tmod_getspi(uint8_t sel) {
  if (!sel) {
    return &SPI;
  } else {
#ifdef ESP32
    return &SPI;
    //return &SPI1;
#else
    return &SPI;
#endif    
  }
}

void tmod_spi_begin(SPIClass *spi, uint8_t flg, int8_t sck, int8_t miso, int8_t mosi) {
#ifdef ESP32 
  if (!flg) {
    if (sck < 0) {
      sck = Pin(GPIO_SPI_CLK);
    }
    if (miso < 0) {
      miso = Pin(GPIO_SPI_MISO);
    }
    if (mosi < 0) {
      mosi = Pin(GPIO_SPI_MOSI);
    }
    spi->begin(sck, miso, mosi, -1);
  } else {
    spi->end();
  }
#else
  if (!flg) {
    spi->begin();
  } else {
    spi->end();
  }
#endif
}

void tmod_spi_write(SPIClass *spi, uint8_t data) {
  spi->write(data);
}

void tmod_spi_writebytes(SPIClass *spi, const uint8_t * data, uint32_t size) {
  spi->writeBytes(data, size);
}

void tmod_Transaction(SPIClass *spi, uint8_t flg, uint32_t spibaud) {
  if (!flg) {
    SPISettings settings = SPISettings(spibaud, MSBFIRST, SPI_MODE0);
    spi->beginTransaction(settings);
  } else {
    spi->endTransaction();
  }
}

uint8_t tmod_transfer(SPIClass *spi, uint8_t data) {
  return spi->transfer(data);
}


void tmod_WebServer_on(const char * prefix, void (*func)(void), uint8_t method) {
#if defined(ESP8266) || defined(__riscv)
  WebServer_on(prefix, func, method);
#else
  char *fcopy = copyStr(prefix);
  WebServer_on(fcopy, func, method);
  free(fcopy);
#endif
}


char * tmod_strncpy_P(char *dst , const char *src, size_t len)  {
  char *out = 0;
#ifdef ESP32
  char *fcopy = copyStr(src);
  out = strncpy_P(dst, fcopy, len);
  free(fcopy);
#endif
  return out;
}

char * tmod_strcpy_P(char *dst , const char *src)  {
#if defined(ESP8266) || defined(__riscv)
  return strcpy_P(dst, src);
#else
  char *fcopy = copyStr(src);
  char *out = strcpy_P(dst, fcopy);
  free(fcopy);
  return out;
#endif
} 

bool WebServer_hasArg(const char * str) {
  //return Webserver->hasArg(str);
  char *fcopy = copyStr(str);
  bool out = Webserver->hasArg(fcopy);
  free(fcopy);
  return out;
}

void tmod_WSContentStart_P(const char* title) {
#if defined(ESP8266) || defined(__riscv)
   WSContentStart_P(title);
#else
  char *fcopy = copyStr(title);
  WSContentStart_P(fcopy);
  free(fcopy);
#endif
}

void tmod_vTaskEnterCritical( void *mux ) {
#ifdef ESP32
  *(portMUX_TYPE*)mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL((portMUX_TYPE*)mux);
#endif
}

void tmod_vTaskExitCritical( void *mux ) {
#ifdef ESP32
  portEXIT_CRITICAL((portMUX_TYPE*)mux);
#endif
}


#ifdef ESP32
#include <driver/rtc_io.h>
#endif

#if ESP_IDF_VERSION_MAJOR >= 5
#include "soc/gpio_periph.h"
#endif // ESP_IDF_VERSION_MAJOR >= 5

/* esp8266
#define DIRECT_READ(base, mask)         ((GPI & (mask)) ? 1 : 0)    //GPIO_IN_ADDRESS
#define DIRECT_MODE_INPUT(base, mask)   (GPE &= ~(mask))            //GPIO_ENABLE_W1TC_ADDRESS
#define DIRECT_MODE_OUTPUT(base, mask)  (GPE |= (mask))             //GPIO_ENABLE_W1TS_ADDRESS
#define DIRECT_WRITE_LOW(base, mask)    (GPOC = (mask))             //GPIO_OUT_W1TC_ADDRESS
#define DIRECT_WRITE_HIGH(base, mask)   (GPOS = (mask))             //GPIO_OUT_W1TS_ADDRESS
*/

uint32_t tmod_directRead(uint32_t pin) {

#ifdef ESP32
//    return digitalRead(pin);               // Works most of the time
//    return gpio_ll_get_level(&GPIO, pin);  // The hal is not public api, don't use in application code
//#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
#if SOC_GPIO_PIN_COUNT <= 32
    return (GPIO.in.val >> pin) & 0x1;
#else  // ESP32 with over 32 gpios
    if ( pin < 32 )
        return (GPIO.in >> pin) & 0x1;
    else
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
#endif
#endif
#ifdef ESP8266
  return digitalRead(pin);
#endif
}


void tmod_directWriteLow(uint32_t pin) {
    //digitalWrite(pin, 0);                  // Works most of the time
    //return;
//    gpio_ll_set_level(&GPIO, pin, 0);      // The hal is not public api, don't use in application code
#ifdef ESP32
//#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
#if SOC_GPIO_PIN_COUNT <= 32
    GPIO.out_w1tc.val = ((uint32_t)1 << pin);
#else  // ESP32 with over 32 gpios
    if ( pin < 32 )
        GPIO.out_w1tc = ((uint32_t)1 << pin);
    else
        GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
#endif
#endif
#ifdef ESP8266
  digitalWrite(pin, LOW);
#endif
}

void tmod_directWriteHigh(uint32_t pin) {
    //digitalWrite(pin, 1);                  // Works most of the time
    //return;
//    gpio_ll_set_level(&GPIO, pin, 1);      // The hal is not public api, don't use in application code

#ifdef ESP32
//#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
#if SOC_GPIO_PIN_COUNT <= 32
    GPIO.out_w1ts.val = ((uint32_t)1 << pin);
#else  // ESP32 with over 32 gpios
    if ( pin < 32 )
        GPIO.out_w1ts = ((uint32_t)1 << pin);
    else
        GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
#endif
#endif
#ifdef ESP8266
  digitalWrite(pin, HIGH);
#endif
}

void tmod_directModeInput(uint32_t pin) {
   // pinMode(pin, INPUT);                   // Too slow - doesn't work
   // return;
//    gpio_ll_output_disable(&GPIO, pin);    // The hal is not public api, don't use in application code

#ifdef ESP32
    if ( digitalPinIsValid(pin) ) {
        // Input
//#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
#if SOC_GPIO_PIN_COUNT <= 32
        GPIO.enable_w1tc.val = ((uint32_t)1 << (pin));
#else  // ESP32 with over 32 gpios
        if ( pin < 32 )
            GPIO.enable_w1tc = ((uint32_t)1 << pin);
        else
            GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
#endif
    }
#endif   
#ifdef ESP8266
  pinMode(pin, INPUT);
#endif
}


void tmod_directModeOutput(uint32_t pin) {
   // pinMode(pin, OUTPUT);                 // Too slow - doesn't work
  //return;
//    gpio_ll_output_enable(&GPIO, pin);    // The hal is not public api, don't use in application code
#ifdef ESP32
    if ( digitalPinCanOutput(pin) ) {
        // Output
//#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
#if SOC_GPIO_PIN_COUNT <= 32
        GPIO.enable_w1ts.val = ((uint32_t)1 << (pin));
#else  // ESP32 with over 32 gpios
        if ( pin < 32 )
            GPIO.enable_w1ts = ((uint32_t)1 << pin);
        else
            GPIO.enable1_w1ts.val = ((uint32_t)1 << (pin - 32));
#endif
    }
#endif
#ifdef ESP8266
  pinMode(pin, OUTPUT);
#endif
}


char *tm_trim(char *s) {
    char *ptr;
    if (!s)
        return NULL;   // handle NULL string
    if (!*s)
        return s;      // handle empty string
    for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
    ptr[1] = '\0';
    return s;
}

#ifdef ESP32
void twi_readFrom(uint8_t address, uint8_t* data, uint8_t length) {
  Wire.requestFrom(address, (size_t)length, (bool)true);
  Wire.readBytes(data, length);
}
#endif  // ESP32


float fl_const(int32_t m, int32_t d) {
  if (d == 0 ) return 0;
  return (float)m / (float)d;
}

int tmod_GetCommandCode(char* destination, size_t destination_size, const char* needle, const char* haystack) {
  char *cph = copyStr(haystack);
  int res = GetCommandCode(destination, destination_size, needle, cph);
  free(cph);
  return res;
}

// modified decode command, no synonyms
bool MT_DecodeCommand(const char* haystack, void (* const MyCommand[])(void), MODULES_TABLE *mt) {

  haystack += EXEC_OFFSET;

#ifdef ESP32
  char *cph = copyStr(haystack);
  if (!cph) {
    return false;
  }
#else
  const char *cph = haystack;
#endif

  const uint8_t *synonyms = nullptr;
  GetTextIndexed(XdrvMailbox.command, CMDSZ, 0, cph);  // Get prefix if available

  int prefix_length = strlen(XdrvMailbox.command);
  if (prefix_length) {
    char prefix[prefix_length +1];
    snprintf_P(prefix, sizeof(prefix), XdrvMailbox.topic);  // Copy prefix part only
    if (strcasecmp(prefix, XdrvMailbox.command)) {
#ifdef ESP32
      free(cph);
#endif
      return false;                                         // Prefix not in command
    }
  }
  size_t syn_count = synonyms ? pgm_read_byte(synonyms) : 0;
  int command_code = GetCommandCode(XdrvMailbox.command + prefix_length, CMDSZ, XdrvMailbox.topic + prefix_length, cph);
  if (command_code > 0) {                                   // Skip prefix
    if (command_code > syn_count) {
      // We passed the synonyms zone, it's a regular command
      XdrvMailbox.command_code = command_code - 1 - syn_count;
      uint32_t *lp = (uint32_t*)MyCommand;
      lp += EXEC_OFFSET / 4;
      uint32_t lval = lp[XdrvMailbox.command_code];
      lval += EXEC_OFFSET;
      void (*Command)(void) = (void (*)(void))lval;
      Command();

      //MyCommand[XdrvMailbox.command_code]();
    } else {
      // We have a SetOption synonym
      XdrvMailbox.index = pgm_read_byte(synonyms + command_code);
      CmndSetoptionBase(0);
    }
#ifdef ESP32
    free(cph);
#endif
    return true;
  }
#ifdef ESP32
  free(cph);
#endif
  return false;
}


void tmod_memmove_P(void *dst, const void *src, size_t size) {
  uint32_t buff[size/4 + 1];
  uint32_t *lp = (uint32_t*) src;
  for (uint32_t cnt = 0; cnt < size / 4 + 1; cnt++) {
    buff[cnt] = *lp++;
  }
  memmove(dst, buff, size);
}

char * tmod_GetTextIndexed(char* destination, size_t destination_size, uint32_t index, const char* haystack) {
  char *sx = copyStr(haystack);
  char *retval = GetTextIndexed(destination, destination_size, index, sx);
  free(sx);
  return retval;
}

int tmod_strncasecmp_P(const char *s1, const char *s2, size_t len) {
#ifdef ESP8266
  return strncasecmp_P(s1, s2, len);
#endif
#ifdef ESP32
  char *sx = copyStr(s2);
  int res = strncasecmp_P(s1, sx, len);
  free(sx);
  return res;
#endif

}

int tmod_ResponseTime_P(const char* format, ...)    // Content send snprintf_P char data
{

#ifdef ESP32
  // This uses char strings. Be aware of sending %% if % is needed
  char timestr[100];
  TasmotaGlobal.mqtt_data = ResponseGetTime(Settings->flag2.time_format, timestr);

  char *fcopy = copyStr(format);
  va_list arg;
  va_start(arg, format);
  char* mqtt_data = ext_vsnprintf_malloc_P(fcopy, arg);
  va_end(arg);
  if (mqtt_data != nullptr) {
    TasmotaGlobal.mqtt_data += mqtt_data;
    free(mqtt_data);
  }
  free(fcopy);
#endif
  return TasmotaGlobal.mqtt_data.length();
}

int tmod_snprintf_P(char *str, size_t strSize,  const char *format, ...) {
int res = 0;
#ifdef ESP32
  char *fcopy = copyStr(format);
  va_list arglist;
  va_start(arglist, format);
  res = vsnprintf_P(str, strSize, fcopy, arglist);
  va_end(arglist);
  free(fcopy);
#endif
  return res;
}

#define SIZE_IRRELEVANT 0x7fffffff

int tmod_sprintf_P(char *str, const char *format, ...) {
int res = 0;
#ifdef ESP32
  char *fcopy = copyStr(format);
  va_list arglist;
  va_start(arglist, format);
  res = vsnprintf_P(str, SIZE_IRRELEVANT, fcopy, arglist);
  va_end(arglist);
  free(fcopy);
#endif
  return res;
}

/*
int tmod_ResponseAppend_P(const char* format, ...) {
  int res = 0;
#ifdef ESP32
  char *fcopy = copyStr(format);
   // This uses char strings. Be aware of sending %% if % is needed
  va_list args;
  va_start(args, format);
  int mlen = ResponseLength();
  int len = ext_vsnprintf_P((char*)TasmotaGlobal.mqtt_data.c_str() + mlen, ResponseSize() - mlen, fcopy, args);
  va_end(args);
  res = len + mlen;
  free(fcopy);
#endif
  return res;
}
*/

int tmod_ResponseAppend_P(const char* format, ...)  // Content send snprintf_P char data
{
#ifdef ESP32
  // This uses char strings. Be aware of sending %% if % is needed
  char *fcopy = copyStr(format);
  va_list arg;
  va_start(arg, format);
  char* mqtt_data = ext_vsnprintf_malloc_P(fcopy, arg);
  va_end(arg);
  if (mqtt_data != nullptr) {
    TasmotaGlobal.mqtt_data += mqtt_data;
    free(mqtt_data);
  }
#endif
  return TasmotaGlobal.mqtt_data.length();
}


void tmod_WebGetArg(const char* arg, char* out, size_t max) {
#if defined(ESP8266) || defined(__riscv)
  WebGetArg(arg, out, max);
#else
  char *fcopy = copyStr(arg);
  WebGetArg(fcopy, out, max);
  String s = Webserver->arg(fcopy);
  strlcpy(out, s.c_str(), max);
  AddLog(LOG_LEVEL_INFO,PSTR(">>> %s - %s"), fcopy, out);
  free(fcopy);
#endif
}


void tmod_WSContentSend_PD(const char* format, ...) {
#ifdef ESP32
  char *fcopy = copyStr(format);
  va_list arg;
  va_start(arg, format);
  _WSContentSendBuffer(true, fcopy, arg);
  va_end(arg);
  free(fcopy);
#endif
}

void tmod_WSContentSend_P(const char* format, ...) {
#ifdef ESP32
  char *fcopy = copyStr(format);
  //WSContentSend_P(fcopy, va);
  va_list arg;
  va_start(arg, format);
  _WSContentSendBuffer(false, fcopy, arg);
  va_end(arg);
  free(fcopy);
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

uint8_t tmod_endTransmission(TwoWire *wp, bool flag) {
  return wp->endTransmission(flag);
}
size_t tmod_requestFrom(TwoWire *wp, uint8_t addr, uint8_t num) {
  return wp->requestFrom(addr, num);
}

bool tmod_I2cSetDevice(uint32_t addr, uint32_t bus) {
  return I2cSetDevice(addr, bus);
}

void tmod_I2cSetActiveFound(uint32_t addr, const char *types, uint32_t bus) {
#ifdef ESP8266
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

int32_t tmod_fixsfti(float in) {
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

int32_t tmod__modsi3(int32_t p1, int32_t p2) {
  return p1 % p2;
}

int64_t tmod__ashldi3(int64_t p1, uint32_t p2) {
  return p1 << p2;
}

uint64_t tmod__lshrdi3(uint64_t p1, uint32_t p2) {
  return p1 >> p2;
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


const void * TGTAB[] PROGMEM = {
  &TasmotaGlobal.tele_period,
  &TasmotaGlobal.global_update,
  &TasmotaGlobal.temperature_celsius,
  &TasmotaGlobal.humidity,
  &TasmotaGlobal.uptime,
  &TasmotaGlobal.rel_inverted,
  &TasmotaGlobal.devices_present,
  &TasmotaGlobal.spi_enabled,
  &TasmotaGlobal.soft_spi_enabled,
  &RtcTime
};

void *tmod_gtbl(void) {
  return TGTAB;
}

// deprecated
uint32_t GetTasmotaGlobal(uint32_t sel) {
  switch (sel) {
    case tele_period:
      return TasmotaGlobal.tele_period;
      break;
    case global_update:
      return TasmotaGlobal.global_update;
      break;
    case humidity:
      return TasmotaGlobal.humidity;
      break;
    case uptime:
      return TasmotaGlobal.uptime;
      break;
    case rel_inverted:
      return TasmotaGlobal.rel_inverted;
      break;
    case devices_present:
      return TasmotaGlobal.devices_present;
      break;
  }
  return 0;
}

// deprecated
void SetTasmotaGlobal(uint32_t sel, uint32_t val) {
  switch (sel) {
    case rel_inverted:
      TasmotaGlobal.rel_inverted = val;
      break;
    case devices_present:
      TasmotaGlobal.devices_present = val;
      break;
  }
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


const char plugin_sensor_names[] PROGMEM = 
D_TEMPERATURE "|"
D_PRESSURE "|"
D_HUMIDITY "|"
D_ABSOLUTE_HUMIDITY "|"
D_DISTANCE "|";


#define TYPESIZE 32
char *Plugin_Get_SensorNames(char *type, uint32_t index) {
  GetTextIndexed(type, TYPESIZE, index, plugin_sensor_names);
  return type;
}

/* ****************************** module handler ***********************************/

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
#ifdef EXECUTE_FROM_BINARY
uint16_t mod_size;
#endif
} plugins;

// 35 + 8 x MODULES_TABLE (18*8 = 144) = about 179 Bytes
MODULES_TABLE modules[MAX_PLUGINS];

#ifdef EXECUTE_FROM_BINARY
#undef Get_mod_size
#define Get_mod_size plugins.mod_size
#else
#undef Get_mod_size
#define Get_mod_size fm->size
#endif


#define MOD_EXEC(A)  fm->mod_func_execute(A)


#define ESP32_PLUGIN_HSIZE SPI_FLASH_SEC_SIZE

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
#if ESP_IDF_VERSION_MAJOR < 5 
    esp_err_t err = esp_partition_mmap(plugins.flash_pptr, 0, plugins.flash_pptr->size, SPI_FLASH_MMAP_INST, &out_ptr, &plugins.map_handle);
#else
    esp_err_t err = esp_partition_mmap(plugins.flash_pptr, 0, plugins.flash_pptr->size, ESP_PARTITION_MMAP_INST, &out_ptr, &plugins.map_handle);
#endif
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

  uint32_t offset = 0;

//  const FLASH_MODULE *xfm = (FLASH_MODULE*)&module_header;
//  AddLog(LOG_LEVEL_INFO, PSTR("Module  %x: %x"), *(uint32_t*)corr_pc, *(uint32_t*)xfm->mod_func_execute);

#ifdef EXECUTE_FROM_BINARY
  // add one testmodule
  modules[0].mod_addr = (void *) &module_header;
//  AddLog(LOG_LEVEL_INFO, PSTR("Module %x: - %x: - %x:"),(uint32_t)modules[0].mod_addr,(uint32_t)&mod_func_execute,(uint32_t)&end_of_module);

  const FLASH_MODULE *fm = (FLASH_MODULE*)modules[0].mod_addr;
  modules[0].jt = MODULE_JUMPTABLE;
  //modules[0].execution_offset = offset;
#ifdef ESP8266
  plugins.mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[0].mod_addr + 4;
#else
  plugins.mod_size = (uint32_t)fm->end_of_module - (uint32_t)modules[0].mod_addr + 8;
#endif
  //modules[0].settings = Settings;

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
      if (modules[cnt].flags.initialized) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        MOD_EXEC(sel);
      }
    }
  }
}


#ifdef USE_SCRIPT
char *Plugin_Query(uint8_t index, uint8_t sel) {
char *result = 0;
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
        result = (char*)MOD_EXEC(FUNC_QUERY_LOW | (sel << 8) | index );
        if (result) {
          return result;
        }
      }
    }
  }
  return result;
}
#endif // USE_SCRIPT

bool Module_Command(uint32_t sel) {
bool result = false;
  for (uint8_t cnt = 0; cnt < MAX_PLUGINS; cnt++) {
    if (modules[cnt].mod_addr) {
      if (modules[cnt].flags.initialized) {
        const FLASH_MODULE *fm = (FLASH_MODULE*)modules[cnt].mod_addr;
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
  //uint8_t *fdesc = (uint8_t *)heap_caps_malloc(size + 4, MALLOC_CAP_EXEC);
  uint8_t *fdesc = (uint8_t *)special_malloc(size + 4);
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
        // get module size
        const FLASH_MODULE *fm = (FLASH_MODULE*)lp;
        blocksize = (fm->size / SPI_FLASH_SEC_SIZE) + 1;
        blocksize *= SPI_FLASH_SEC_SIZE;
      } else {
        // free module block, check required size
        uint8_t blocks = (size / SPI_FLASH_SEC_SIZE) + 1;
        //AddLog(LOG_LEVEL_INFO, PSTR("needed blocks: %d"), blocks);
        uint32_t *bp = lp;
        uint8_t free = 1;
        for (uint32_t cnt = 0; cnt < blocks; cnt++) {
          if (*bp == MODULE_SYNC) {
            free = 0;
          }
          bp += SPI_FLASH_SEC_SIZE / 4;
          if ((uint32_t)bp >= plugins.free_flash_end) {
            break;
          }
          //AddLog(LOG_LEVEL_INFO, PSTR("blocks: %d - %d"), cnt, free);
        }
        if (free) {
          eeprom_block = addr;
          break;
        }
      }
      lp += (blocksize / 4);
      addr += blocksize;
      //AddLog(LOG_LEVEL_INFO, PSTR("progress: %d"), addr);
      yield();
  }
  return eeprom_block;
}

uint32_t Store_Module(uint8_t *fdesc, uint32_t size, uint32_t *ioffset, uint8_t flag, uint8_t index) {

  //AddLog(LOG_LEVEL_INFO, PSTR("store module size: %d"), size);

  uint32_t eeprom_block = Module_CheckFree(size);
  if (!eeprom_block) {
    return 0;
  }

  //AddLog(LOG_LEVEL_INFO, PSTR(" >>>"));

#ifdef ESP8266  
  const FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  uint32_t new_pc = (uint32_t)eeprom_block + plugins.flashbase;

  uint32_t offset = new_pc - fm->mod_start_org;
  uint32_t *lp = (uint32_t*)&fm->execution_offset; 
  *lp = offset;
  
  lp = (uint32_t*)&fm->mod_func_execute;
  *lp = (uint32_t)fm->mod_func_execute_org + fm->execution_offset;;
  
  lp = (uint32_t*)&fm->mtv;
  *lp = (uint32_t)&modules[index];

  lp = (uint32_t*)&fm->jtab;
  *lp = (uint32_t)&MODULE_JUMPTABLE;

  uint32_t *lwp=(uint32_t*)fdesc;
#endif // ESP8266

#ifdef ESP32
  FLASH_MODULE *fm = (FLASH_MODULE*)fdesc;
  fm->execution_offset = (uint32_t)eeprom_block - fm->mod_start_org;

  uint32_t *lp = (uint32_t*)&fm->mod_func_execute;
  *lp = (uint32_t)fm->mod_func_execute_org + fm->execution_offset;

  fm->mtv = (uint32_t)&modules[index];
  fm->jtab = (uint32_t)&MODULE_JUMPTABLE;

  uint32_t *lwp=(uint32_t*)fdesc;
  uint32_t new_pc = eeprom_block;
#endif // ESP32

#ifdef ESP8266
//  AddLog(LOG_LEVEL_INFO, PSTR("Module offset %x: %x: %x: %x: %x: %x"),old_pc, new_pc, offset, corr_pc, (uint32_t)fm->mod_func_execute, (uint32_t)&module_header);
  uint8_t blocks = (size / SPI_FLASH_SEC_SIZE) + 1;
  for (uint8_t cnt = 0; cnt < blocks; cnt++) {
    ESP.flashEraseSector(eeprom_block / SPI_FLASH_SEC_SIZE);
    ESP.flashWrite(eeprom_block , lwp, SPI_FLASH_SEC_SIZE);
    lwp += SPI_FLASH_SEC_SIZE / 4;
    eeprom_block += SPI_FLASH_SEC_SIZE;
    yield();
  }
#endif // ESP8266

#ifdef ESP32
  AddLog(LOG_LEVEL_INFO, PSTR("save module: %08x, size: %d"),eeprom_block, size);
  uint32_t offset = eeprom_block - plugins.free_flash_start;
  uint8_t blocks = (size / ESP32_PLUGIN_HSIZE) + 1;
  for (uint8_t cnt = 0; cnt < blocks; cnt++) {
    esp_err_t err = err = esp_partition_erase_range(plugins.flash_pptr, offset, ESP32_PLUGIN_HSIZE);
    uint32_t ssize = ESP32_PLUGIN_HSIZE;
    if (size < ESP32_PLUGIN_HSIZE) {
      ssize = size;
    }
    err = esp_partition_write(plugins.flash_pptr, offset, (void*)lwp, ssize);
    lwp += ESP32_PLUGIN_HSIZE / sizeof(uint32_t);
    offset += ESP32_PLUGIN_HSIZE;
    size -= ESP32_PLUGIN_HSIZE;
    yield();
    //AddLog(LOG_LEVEL_INFO, PSTR("progress: %d"),offset);
  }
#endif // ESP32
  return new_pc;
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
      //modules[module].execution_offset = fm->execution_offset;
      //modules[module].mod_size = fm->size;
      //modules[module].settings = Settings;
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
      ResponseAppend_P(PSTR("\"MOD #%d\":{\"name\":\"%s\",\"addr\":\"%08x\",\"ex-offs\":\"%08x\", \"size\":%d,\"type\":\"%s\",\"rev\":%d.%d,\"mem\":%d,\"init\":%d}"),cnt + 1, name, modules[cnt].mod_addr, fm->execution_offset,
       Get_mod_size, type, (rev>>16),(rev&0xff), modules[cnt].mem_size, modules[cnt].flags.initialized);
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
      AddLog(LOG_LEVEL_INFO,PSTR("plugin architecture error"));
      return;
    }

    if (fm->revision < MINREV) {
      free(mp);
      AddLog(LOG_LEVEL_INFO,PSTR("plugin revision to old"));
      return;
    }

    if (fm->revision < CURR_MINREV ) {
      free(mp);
      AddLog(LOG_LEVEL_INFO,PSTR("plugin hander revision to old"));
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
      cp = strchr(nam, '_');
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
#endif // ESP8266
#ifdef ESP32
      uint32_t offset = (uint32_t)modules[module].mod_addr - plugins.free_flash_start;
      AddLog(LOG_LEVEL_INFO, PSTR("part offset: %08x"), offset);
      esp_err_t err = esp_partition_read(plugins.flash_pptr, offset, (void*)buff, ESP32_PLUGIN_HSIZE);
      FLASH_MODULE *fm = (FLASH_MODULE*)buff;
      //AddLog(LOG_LEVEL_INFO,PSTR("read flash: %08x"),fm->sync);
      if (fm->sync == MODULE_SYNC) {
        AddLog(LOG_LEVEL_INFO,PSTR("modify data"));
        for (uint16_t cnt = 0; cnt < MAX_MOD_STORES; cnt++ ) {
          fm->ms[cnt].value = *data++;
        }
        err = esp_partition_erase_range(plugins.flash_pptr, offset, ESP32_PLUGIN_HSIZE);
        err = esp_partition_write(plugins.flash_pptr, offset, (void*)buff, ESP32_PLUGIN_HSIZE);
      }
#endif // ESP32
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
    uint32_t exoffs = fm->execution_offset;
    // recalc execution offset
    uint32_t mfe_org = (uint32_t)fm->mod_start_org;
    uint32_t coffs = (uint32_t)fm - mfe_org;

    if (((uint32_t)&modules[module] != mtv) || ((uint32_t)&MODULE_JUMPTABLE != jtab) || (exoffs != coffs)) {
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

          lp = (uint32_t*)&fm->execution_offset;
          *lp = coffs;

          ESP.flashEraseSector(((uint32_t)modules[module].mod_addr - plugins.flashbase) / SPI_FLASH_SEC_SIZE);
          ESP.flashWrite((uint32_t)modules[module].mod_addr - plugins.flashbase, (uint32_t*)buff, SPI_FLASH_SEC_SIZE);
        }
#endif // ESP8266

#ifdef ESP32
        uint32_t offset = (uint32_t)modules[module].mod_addr - plugins.free_flash_start;
        esp_err_t err = esp_partition_read(plugins.flash_pptr, offset, (void*)buff, ESP32_PLUGIN_HSIZE);
        FLASH_MODULE *fm = (FLASH_MODULE*)buff;
        if (fm->sync == MODULE_SYNC) {
          AddLog(LOG_LEVEL_INFO, PSTR("part update"));
          fm->mtv = (uint32_t)&modules[module];
          fm->jtab = (uint32_t)&MODULE_JUMPTABLE;
          err = esp_partition_erase_range(plugins.flash_pptr, offset, ESP32_PLUGIN_HSIZE);
          err = esp_partition_write(plugins.flash_pptr, offset, (void*)buff, ESP32_PLUGIN_HSIZE);
        }
#endif // EPS32
        free(buff);
      }
    }
    int32_t result = MOD_EXEC(MODFUNC_INIT);
    
    modules[module].flags.web_sensor = 1;
    modules[module].flags.json_append = 1;
    AddLog(LOG_LEVEL_INFO,PSTR("module %d inizialized: %08x"),module + 1, result);
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

#if 1
  if (XdrvMailbox.data_len) {
    
    char *cp = XdrvMailbox.data;
    uint16_t module = strtol(cp, &cp, 10);
    if (module >= 1 && module <= MAX_PLUGINS) {
      module--;

      int16_t block = 0;

      if (*cp == ' ') {
        cp++;
        block = strtol(cp, &cp, 10);
        if (block < 0 || block >= 8 ) {
          block = 0;
        }
      }
      uint16_t size = 512;
      uint32_t *lp = (uint32_t*) modules[module].mod_addr;
      lp += (512 / sizeof(uint32_t)) * block; 
      for (uint32_t cnt = 0; cnt < (size / 32) + 1; cnt ++) {
        AddLog(LOG_LEVEL_INFO,PSTR("%08x: %08x %08x %08x %08x %08x %08x %08x %08x"),lp,lp[0],lp[1],lp[2],lp[3],lp[4],lp[5],lp[6],lp[7]);
        lp += 8;
      }

    }
  }

#else  
  if ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= MAX_PLUGINS)) {
    uint8_t module = XdrvMailbox.payload - 1;
    if (modules[module].mod_addr) {
      uint16_t size = modules[module].mod_size;
#ifdef __riscv
      // actually should test for single core
      size = 512;
#endif      
      uint32_t *lp = (uint32_t*) modules[module].mod_addr;
      for (uint32_t cnt = 0; cnt < (size / 32) + 1; cnt ++) {
        AddLog(LOG_LEVEL_INFO,PSTR("%08x: %08x %08x %08x %08x %08x %08x %08x %08x"),lp,lp[0],lp[1],lp[2],lp[3],lp[4],lp[5],lp[6],lp[7]);
        lp += 8;
      }
    }
  }
#endif
  ResponseCmndDone();
}

#include <MD5Builder.h>

void Check_partition(void) {
  const esp_partition_t *pptr;
  
  pptr = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, "custom");
  if (pptr) {
     AddLog(LOG_LEVEL_INFO,PSTR("custom plugin partition already there!"));
  }

  // partition talble is aways at 0x8000
  esp_partition_t spiffs;

  esp_partition_iterator_t iterator = NULL;
  esp_partition_type_t part_type = ESP_PARTITION_TYPE_ANY;
  const esp_partition_t *next_partition = NULL;
  iterator = esp_partition_find(part_type, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (iterator) {
    next_partition = esp_partition_get(iterator);
    if (next_partition != NULL) {
      AddLog(LOG_LEVEL_INFO,PSTR("partition addr: 0x%06x; size: 0x%06x; label: %s"), next_partition->address, next_partition->size, next_partition->label);
      if (!pptr) {
        if (!strcmp(next_partition->label, "spiffs")) {
          //
          AddLog(LOG_LEVEL_INFO,PSTR("spiffs partition found!"));
          memmove(&spiffs, next_partition, sizeof(esp_partition_t));
        }
      }
      iterator = esp_partition_next(iterator);
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

  if (!HttpCheckPriviledgedAccess()) { 
    return;
  }

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

#ifdef EXECUTE_FROM_BINARY
  WSContentSend_P(MOD_FORM_FILE_UPG, PSTR("Plugin upload disabled"));
#else
  WSContentSend_P(MOD_FORM_FILE_UPG, PSTR("Plugin upload"));
#endif

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
      WSContentSend_P(HTTP_MODULES_COMMONa, "808080", cnt + 1, name, type, srev, Get_mod_size, modules[cnt].mem_size);

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
            int8_t from = (val32 >> 16);
            int8_t spin = val32 & 0xff;
            uint8_t to = val32 >> 8;
            for (int8_t pins = from; pins <= to; pins++) {
              char sel[10];
              if (spin == pins) {
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
    plugins.module_input_buffer = (uint8_t *)special_malloc(size + 4);
    if (!plugins.module_input_buffer) {
      AddLog(LOG_LEVEL_INFO,PSTR("memory error"));
      return false;
    }
    plugins.module_input_ptr = plugins.module_input_buffer;
    //Module_CheckFree(size, upload.filename.c_str());
  }

  delay(0);

  //AddLog(LOG_LEVEL_INFO,PSTR("progress; %d"),plugins.module_bytes_read);

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
    char *cp = strchr(plugins.mod_name, '_');
    if (cp) {
      *cp = 0;
    }
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
    case FUNC_PRE_INIT:
      InitModules();
      break;
    case FUNC_INIT:
      break;
    case FUNC_COMMAND:
    if (plugins.ready) {
        result = DecodeCommand(kModuleCommands, ModuleCommand);
        if (!result) {
          result = Module_Command(FUNC_COMMAND);
        }
      }
      break;
    case FUNC_EVERY_100_MSECOND:
    case FUNC_EVERY_250_MSECOND:
    case FUNC_EVERY_SECOND:
    case FUNC_WEB_ADD_BUTTON:
    case FUNC_SET_POWER: 
    case FUNC_LOOP:
    case FUNC_COMMAND_SENSOR:
    case FUNC_WEB_ADD_MAIN_BUTTON:
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
        Module_Execute(function);
      }
      break;
    case FUNC_ACTIVE:
      result = true;
      break;
  }
  return result;
}

#endif  // USE_BINPLUGINS
