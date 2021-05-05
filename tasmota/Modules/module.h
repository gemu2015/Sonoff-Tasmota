//#include <Arduino.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <HardwareSerial.h>

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
  void (* const *jt)(void);
  void *mod_memory;
  uint16_t mem_size;
} MODULES_TABLE;

typedef struct {
  uint16_t sync;
  uint16_t arch;
  uint16_t type;
  uint16_t revision;
  char name[16];
  int32_t (*mod_func_init)(MODULES_TABLE *, uint32_t);
  void (*end_of_module)(void);
} FLASH_MODULE;


int32_t mod_func_init(MODULES_TABLE *, uint32_t);
void end_of_module(void);

#define MODULE_DESCRIPTOR  const FLASH_MODULE


#define jWire (TwoWire*)(jt[0])
#define jWire1 (TwoWire*)(jt[1])
#define jSerial (HardwareSerial*)(jt[2])
#define sprint(A) ((void (*)(char*))(jt[3]))(A)
