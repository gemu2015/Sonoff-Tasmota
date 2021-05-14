
#ifndef _MODULE_H_
#define _MODULE_H_

#include <stdio.h>
#include <stddef.h>
#include <Wire.h>
#include <Stream.h>
#include <HardwareSerial.h>

#include "tasmota_options.h"
#define SerConfu8 uint8_t
#include "tasmota.h"

#define AGPIO(x) ((x)<<5)
#define BGPIO(x) ((x)>>5)

#include "i18n.h"
#include "tasmota_template.h"
#include "settings.h"

#ifndef PROGMEM
#define PROGMEM
#endif

enum {MODULE_TYPE_SENSOR, MODULE_TYPE_LIGHT, MODULE_TYPE_ENERGY, MODULE_TYPE_DRIVER};
enum {ARCH_ESP8266, ARCH_ESP32};

#define MODULE_SYNC 0x55aaFC4A

#define SETTINGS Tasmota_Settings
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



#endif // _MODULE_H_
