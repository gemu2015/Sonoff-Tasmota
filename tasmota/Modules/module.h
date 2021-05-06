//#include <Arduino.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <HardwareSerial.h>
//#include "settings.h"

typedef struct  {
  uint16_t test1;
  uint8_t murks;
} MODULE1_MEMORY;


#ifndef PROGMEM
#define PROGMEM
#endif

enum {MODULE_TYPE_SENSOR, MODULE_TYPE_LIGHT, MODULE_TYPE_ENERGY};
enum {ARCH_ESP8266, ARCH_ESP32};

#define MODULE_SYNC 0x4AFC

#undef CURR_ARCH
#ifdef ESP8266
#define CURR_ARCH ARCH_ESP8266
#else
#define CURR_ARCH ARCH_ESP32
#endif

typedef struct {
  //const FLASH_MODULE *mod_addr;
  void *mod_addr;
  uint16_t mod_size;
  void (* const *jt)(void);
  void *mod_memory;
  uint16_t mem_size;
//  Settings *settings;
} MODULES_TABLE;

#define MD_TYPE uint32_t
// this descriptor is in .text so only 32 bit access allowed
typedef struct {
  MD_TYPE sync;
  MD_TYPE arch;
  MD_TYPE type;
  MD_TYPE revision;
  char name[16];
  int32_t (*mod_func_init)(MODULES_TABLE *, uint32_t);
  void (*end_of_module)(void);
  //uint32_t size;
} FLASH_MODULE;


int32_t mod_func_init(MODULES_TABLE *, uint32_t);
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
#define jdtostrfd(A,B,C)((char *(*)(double, uint8_t, char*))(jt[8]))(A,B,C)
#define jcalloc(A,B)((void *(*)(size_t, size_t))(jt[9]))(A,B)

#define sprint(A) ((void (*)(char*))(jt[9]))(A)

extern void AddLog(uint32_t loglevel, PGM_P formatP, ...);

#define DEFSTR(LABEL,TEXT) __asm__ __volatile__ (".section .text.mod_string");__asm__ __volatile__ (".align 4");__asm__ __volatile__(#LABEL ".asciz "#TEXT"");
#define EXTSTR(LABEL) extern const char *(LABEL);

#define MODULE_DESC __attribute__((section(".text.mod_desc"))) extern const FLASH_MODULE
#define MODULE_PART __attribute__((section(".text.mod_part")))
#define MODULE_END __attribute__((section(".text.mod_end")))

#define CAT2(a,b) a##b
#define CAT(a,b) CAT2(a,b)
#define UNIQUE_ID CAT(_uid_,__COUNTER__)

#define jPSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))


#define GSTR(STRING) (const char*)&(STRING)
