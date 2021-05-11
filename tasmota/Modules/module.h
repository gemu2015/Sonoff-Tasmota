//#include <Arduino.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <HardwareSerial.h>

#include "tasmota_options.h"

/*
#include "tasmota_version.h"                // Tasmota version information
#include "tasmota.h"                        // Enumeration used in my_user_config.h
#include "my_user_config.h"                 // Fixed user configurable options
#include "tasmota_globals.h"                // Function prototypes and global configuration
#include "i18n.h"                           // Language support configured by my_user_config.h
*/

#ifndef PROGMEM
#define PROGMEM
#endif

enum {MODULE_TYPE_SENSOR, MODULE_TYPE_LIGHT, MODULE_TYPE_ENERGY};
enum {ARCH_ESP8266, ARCH_ESP32};

#define MODULE_SYNC 0x55aaFC4A

#define SETTINGS mySettings
//#define SETTINGS struct TSettings

//extern SETTINGS Settings;

/* linker sections
*(.text.mod_desc)
*(.text.mod_string)
*(.text.mod*)
*(.text.mod_end)
*/

#undef CURR_ARCH
#ifdef ESP8266
#define CURR_ARCH ARCH_ESP8266
#else
#define CURR_ARCH ARCH_ESP32
#endif

#define FUNC_DEINIT 999

// should import Tasmota settings later
typedef struct {
  uint8_t temperature_resolution;
} mySettings;

typedef union {
  uint8_t data;
  struct {
    uint8_t spare1 : 1;
    uint8_t spare2 : 1;
    uint8_t spare3 : 1;
    uint8_t spare4 : 1;
    uint8_t every_second : 1;
    uint8_t web_sensor : 1;
    uint8_t json_append : 1;
    uint8_t initialized : 1;
  };
} MOD_FLAGS;

typedef struct {
  void *mod_addr;
  uint16_t mod_size;
  void (* const *jt)(void);
  void *mod_memory;
  uint16_t mem_size;
  uint32_t execution_offset;
  SETTINGS *settings;
  MOD_FLAGS flags;
} MODULES_TABLE;


#define MD_TYPE uint32_t
// this descriptor is in .text so only 32 bit access allowed
typedef struct {
  MD_TYPE sync;
  MD_TYPE arch;
  MD_TYPE type;
  MD_TYPE revision;
  char name[16];
  int32_t (*mod_func_execute)(MODULES_TABLE *, uint32_t);
  void (*end_of_module)(void);
  uint32_t size;
  uint32_t execution_offset;
} FLASH_MODULE;


int32_t mod_func_execute(MODULES_TABLE *, uint32_t);
void end_of_module(void);


#define MODULE_DESCRIPTOR  const FLASH_MODULE


#define jWire (TwoWire*)(jt[0])
#define jWire1 (TwoWire*)(jt[1])
#define jSerial (HardwareSerial*)(jt[2])
#define jI2cSetDevice(A) ((bool (*)(uint32_t))(jt[3]))(A)
#define jI2cSetActiveFound(A,B,C)((void (*)(uint32_t,const char *, uint32_t))(jt[4]))(A,B,C)
#define jAddLog(A,B) ((void (*)(uint32_t, PGM_P, ...))(jt[5]))(A,B)
#define jResponseAppend_P(A,B,C) ((void (*)(const char * formatP, ...))(jt[6]))(A,B,C)
#define jWSContentSend_PD(A,B,C) ((void (*)(const char * formatP, ...))(jt[7]))(A,B,C)
#define jftostrfd(A,B,C)((char *(*)(float, uint8_t, char*))(jt[8]))(A,B,C)
#define jcalloc(A,B)((void *(*)(size_t, size_t))(jt[9]))(A,B)
#define jfscale(A,B,C)((float (*)(int32_t, float, float))(jt[10]))(A,B,C)
#define sprint(A) ((void (*)(const char*))(jt[11]))(A)
#define jbeginTransmission(BUS,ADDR) ((void (*)(TwoWire*,uint8_t))(jt[12]))(BUS,ADDR)
#define jwrite(BUS,VAL) ((void (*)(TwoWire*,uint8_t))(jt[13]))(BUS,VAL)
#define jendTransmission(BUS,VAL) ((void (*)(TwoWire*,bool))(jt[14]))(BUS,VAL)
#define jrequestFrom(BUS,ADDR,NUM) ((void (*)(TwoWire*,uint8_t,uint8_t))(jt[15]))(BUS,ADDR,NUM)
#define jread(BUS) ((uint8_t (*)(TwoWire*))(jt[16]))(BUS)
#define fshowhex(VAL) ((void (*)(uint32_t))(jt[17]))(VAL)
#define jfree(MEM) ((void (*)(void*))(jt[18]))(MEM)


extern void AddLog(uint32_t loglevel, PGM_P formatP, ...);

#define MODULE_DESC __attribute__((section(".text.mod_desc"))) extern const FLASH_MODULE
#define MODULE_PART __attribute__((section(".text.mod_part")))
#define MODULE_END __attribute__((section(".text.mod_end"))) void  end_of_module(void) {__asm__ __volatile__(".word 0x4AFCAA55");}

#define CAT2(a,b) a##b
#define CAT(a,b) CAT2(a,b)
#define UNIQUE_ID CAT(_uid_,__COUNTER__)


#define DPSTR(LABEL,TEXT) extern "C" {  const char *LABEL(void);} __asm__  (\
  ".section .text.mod_string\n"\
  ".align 4\n"\
  ".global " #LABEL "\n"\
  #LABEL": .asciz "#TEXT" \n"\
);

#define GPSTR(VAR,FUNC) const char *VAR = (const char*)&FUNC + mt->execution_offset; fshowhex((uint32_t)VAR);
//#define jPSTR(LABEL) (__extension__({ (const char *)&LABEL[0]+mt->execution_offset;}))
#define jPSTR(LABEL) (const char *)LABEL+mt->execution_offset

//#define jPSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))

#define SETREGS MLX9014_MEMORY *mod_mem = (MLX9014_MEMORY*)mt->mod_memory;void (* const *jt)() = mt->jt;

#define ALLOCMEM(A) void (* const *jt)() = mt->jt;mt->mem_size = sizeof(A);mt->mem_size += mt->mem_size % 4;mt->mod_memory = jcalloc(mt->mem_size / 4, 4);if (!mt->mod_memory) {return -1;};MLX9014_MEMORY *mod_mem = (MLX9014_MEMORY*)mt->mod_memory;SETTINGS *jsettings = mt->settings;

#define MODULE_SYNC_END __attribute__((section(".text.mod_end"))); __asm__ __volatile__ (".align 4");
