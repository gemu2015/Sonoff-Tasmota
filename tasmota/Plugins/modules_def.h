#ifndef MODULE_TYPE_SENSOR
enum {MODULE_TYPE_SENSOR, MODULE_TYPE_LIGHT, MODULE_TYPE_ENERGY, MODULE_TYPE_DRIVER};
enum {ARCH_ESP8266, ARCH_ESP32,ARCH_ESP32_RV};
#endif

enum {iD_TEMPERATURE,iD_PRESSURE,iD_HUMIDITY,iD_ABSOLUTE_HUMIDITY,iD_DISTANCE};

#define MODULE_SYNC 0x55aaFC4A

#undef CURR_ARCH
#ifdef ESP8266
#define CURR_ARCH ARCH_ESP8266
#else
#ifdef __riscv
#define CURR_ARCH ARCH_ESP32_RV
#else
#define CURR_ARCH ARCH_ESP32
#endif
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
  void (* const *jt)(void);
  void *mod_memory;
  uint16_t mem_size;
 // uint32_t execution_offset;
  MOD_FLAGS flags;
} MODULES_TABLE;


//#define EXEC_OFFSET mt->execution_offset

#define EXEC_OFFSET ((FLASH_MODULE*)mt->mod_addr)->execution_offset


#define MD_TYPE uint32_t

#define MOD_STORE_NAMESIZE 8

typedef struct {
  char name[MOD_STORE_NAMESIZE];
  volatile MD_TYPE value;
} MODULE_STORE;

#define MAX_MOD_STORES 4

// this descriptor is in .text so only 32 bit access allowed
#pragma pack(4)
typedef struct {
  MD_TYPE sync;
  MD_TYPE arch;
  MD_TYPE type;
  MD_TYPE revision;
  char name[16];
  // 32 => 0x20
  int32_t (*mod_func_execute)(uint32_t);
  void (*end_of_module)(void);
  MD_TYPE size;
  // 40 => 0x28
  MD_TYPE execution_offset;
  // 44 => 0x2c
  MD_TYPE mtv;
  MD_TYPE jtab;
  // 52 = 0x34
  uint32_t mod_start_org;
  int32_t (*mod_func_execute_org)(uint32_t);
  
  // 56
  MODULE_STORE ms[MAX_MOD_STORES];
} FLASH_MODULE;

enum {
  temperature_celsius = 0, tele_period, global_update, humidity, uptime, rel_inverted, devices_present 
};


#define FUNC_QUERY_LOW 0x80000000
#define FUNC_QUERY_HIGH 0xffffffff

//slow RTC MEM
#define GLOB_MOD_REG 0x50001ff0
//#define GLOB_MOD_REG RTC_SLOW_MEM
