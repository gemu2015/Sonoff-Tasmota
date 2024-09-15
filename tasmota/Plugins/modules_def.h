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
  void (*pvTaskCode)(void*);
  const char *constpcName;
  uint32_t usStackDepth;
  void *constpvParameters;
  uint32_t uxPriority;
  void *constpvCreatedTask;
  uint32_t xCoreID;
} TASKPARS;

typedef struct {
  void *mod_addr;
  void (* const *jt)(void);
  void *mod_memory;
  uint16_t mem_size;
 // uint32_t execution_offset;
  MOD_FLAGS flags;
} MODULES_TABLE;

typedef struct {
    int8_t rxpin;
    int8_t txpin;
    int8_t hwfb;
    int8_t nwmode;
    uint16_t bsize;
    uint32_t speed;
    int8_t invert;
} TSPARS;

typedef struct {
  uint32_t cnt_last_ts;
  uint32_t counter_ltime;
  uint32_t counter_lfalltime;
  uint32_t counter_pulsewidth;
  uint16_t debounce;
  uint8_t cnt_updated;
  uint8_t cnt_debounce;
  uint8_t cnt_old_state;
  int8_t srcpin;
  uint8_t pinstate;
} PLUGIN_COUNTER;


#define MD_TYPE uint32_t

#define MOD_STORE_NAMESIZE 8

typedef struct {
  char name[MOD_STORE_NAMESIZE];
  volatile MD_TYPE value;
} MODULE_STORE;


enum {
  temperature_celsius = 0, tele_period, global_update, humidity, uptime, rel_inverted, devices_present 
};


#define FUNC_QUERY_LOW 0x80000000
#define FUNC_QUERY_HIGH 0xffffffff

//slow RTC MEM
#define GLOB_MOD_REG 0x50001ff0
//#define GLOB_MOD_REG RTC_SLOW_MEM
