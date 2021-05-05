//#include <Arduino.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
//#include <HardwareSerial.h>

enum XsnsFunctions {FUNC_SETTINGS_OVERRIDE, FUNC_PIN_STATE, FUNC_MODULE_INIT, FUNC_PRE_INIT, FUNC_INIT,
                    FUNC_LOOP, FUNC_EVERY_50_MSECOND, FUNC_EVERY_100_MSECOND, FUNC_EVERY_200_MSECOND, FUNC_EVERY_250_MSECOND, FUNC_EVERY_SECOND,
                    FUNC_SAVE_SETTINGS, FUNC_SAVE_AT_MIDNIGHT, FUNC_SAVE_BEFORE_RESTART,
                    FUNC_AFTER_TELEPERIOD, FUNC_JSON_APPEND, FUNC_WEB_SENSOR, FUNC_COMMAND, FUNC_COMMAND_SENSOR, FUNC_COMMAND_DRIVER,
                    FUNC_MQTT_SUBSCRIBE, FUNC_MQTT_INIT, FUNC_MQTT_DATA,
                    FUNC_SET_POWER, FUNC_SET_DEVICE_POWER, FUNC_SHOW_SENSOR, FUNC_ANY_KEY,
                    FUNC_ENERGY_EVERY_SECOND, FUNC_ENERGY_RESET,
                    FUNC_RULES_PROCESS, FUNC_TELEPERIOD_RULES_PROCESS, FUNC_SERIAL, FUNC_FREE_MEM, FUNC_BUTTON_PRESSED,
                    FUNC_WEB_ADD_BUTTON, FUNC_WEB_ADD_CONSOLE_BUTTON, FUNC_WEB_ADD_MANAGEMENT_BUTTON, FUNC_WEB_ADD_MAIN_BUTTON,
                    FUNC_WEB_ADD_HANDLER, FUNC_SET_CHANNELS, FUNC_SET_SCHEME, FUNC_HOTPLUG_SCAN,
                    FUNC_DEVICE_GROUP_ITEM };

typedef struct  {
  uint16_t test1;
  uint8_t murks;
} MODULE1_MEMORY;

MODULE1_MEMORY mem;


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
  uint16_t sync;
  uint16_t arch;
  uint16_t type;
  uint16_t revision;
  char name[16];
  int32_t (*mod_func_init)(struct MODULES_TABLE *, uint32_t);
  void (*end_of_module)(void);
} FLASH_MODULE;


struct MODULES_TABLE {
  const FLASH_MODULE *mod_addr;
  void (* const *jt)();
  void *mod_memory;
  uint16_t mem_size;
};

static int32_t mod_func_init(struct MODULES_TABLE *, uint32_t);
static void end_of_module(void);

#define MODULE_DESCRIPTOR __attribute__((section(".text"))) static const volatile FLASH_MODULE


#define jWire (TwoWire*)(jt[0])
#define jWire1 (TwoWire*)(jt[1])
#define jSerial (HardwareSerial*)(jt[2])
#define sprint(A) ((void (*)(char*))(jt[3]))(A)
