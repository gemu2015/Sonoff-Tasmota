/*
  xsns_70_veml6075_dual.cpp — Vishay VEML6075 UVA / UVB / UV-Index
  light sensor driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2021 Martin Wagner

  Plugin: USE_VEML6075_DUAL_MOD via build_plugin.py.
  Native: USE_VEML6075_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_70_veml6075_dual.ino.

  Single fixed I2C address (0x10). Probe scans both buses; first
  responder with the right chip ID (0x26) wins.

  Console commands (D_CMND_VEML6075_*):
    VEML6075power     0|1|2     — power down (0), normal (1), forced (2)
    VEML6075dynamic   0|1       — high-dynamic-range bit
    VEML6075inttime   0..4      — integration time index
                                    0=50ms, 1=100ms, 2=200ms, 3=400ms, 4=800ms
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_VEML6075_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  include "../Tasmota/include/i18n.h"
#endif

#if BUILD_AS_PLUGIN
#  ifdef USE_VEML6075_DUAL_MOD
#    define _VEML6075_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_VEML6075_DUAL) && defined(VEML6075_DUAL_NATIVE_INCLUDE)
#    define _VEML6075_DUAL_ENABLED 1
#  endif
#endif

#ifdef _VEML6075_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define VEML6075_ADDR    0x10
#define VEML6075_CHIP_ID 0x26

#define VEML6075_REG_CONF    0x00
#define VEML6075_REG_UVA     0x07
#define VEML6075_REG_DARK    0x08
#define VEML6075_REG_UVB     0x09
#define VEML6075_REG_UVCOMP1 0x0A
#define VEML6075_REG_UVCOMP2 0x0B
#define VEML6075_REG_ID      0x0C

// Calibration coefficients (no-coverglass defaults from Vishay AN84339)
#define VEML6075_DEFAULT_UVA_A_COEFF      2.22
#define VEML6075_DEFAULT_UVA_B_COEFF      1.33
#define VEML6075_DEFAULT_UVB_C_COEFF      2.95
#define VEML6075_DEFAULT_UVB_D_COEFF      1.74
#define UVA_RESPONSIVITY_100MS_UNCOVERED  0.001461
#define UVB_RESPONSIVITY_100MS_UNCOVERED  0.002591

const float VEML_UVA_RESPONSIVITY[] PROGMEM = {
    UVA_RESPONSIVITY_100MS_UNCOVERED / 0.5016286645,  // 50ms
    UVA_RESPONSIVITY_100MS_UNCOVERED,                 // 100ms
    UVA_RESPONSIVITY_100MS_UNCOVERED / 2.039087948,   // 200ms
    UVA_RESPONSIVITY_100MS_UNCOVERED / 3.781758958,   // 400ms
    UVA_RESPONSIVITY_100MS_UNCOVERED / 7.371335505    // 800ms
};
const float VEML_UVB_RESPONSIVITY[] PROGMEM = {
    UVB_RESPONSIVITY_100MS_UNCOVERED / 0.5016286645,
    UVB_RESPONSIVITY_100MS_UNCOVERED,
    UVB_RESPONSIVITY_100MS_UNCOVERED / 2.039087948,
    UVB_RESPONSIVITY_100MS_UNCOVERED / 3.781758958,
    UVB_RESPONSIVITY_100MS_UNCOVERED / 7.371335505
};

// Display / JSON strings — file-unique `VEML_` prefix.
// (D_UVA_INTENSITY / D_UVB_INTENSITY aren't in tasmota/i18n.h; they
// were defined locally in the legacy plugin source and are kept here.)
#define D_NAME_VEML6075 "VEML6075"
#ifndef D_UVA_INTENSITY
#  define D_UVA_INTENSITY "UVA intensity"
#endif
#ifndef D_UVB_INTENSITY
#  define D_UVB_INTENSITY "UVB intensity"
#endif

const char VEML_HTTP_UVA[]     PROGMEM = "{s}%s " D_UVA_INTENSITY "{m}%d " D_UNIT_WATT_METER_QUADRAT "{e}";
const char VEML_HTTP_UVB[]     PROGMEM = "{s}%s " D_UVB_INTENSITY "{m}%d " D_UNIT_WATT_METER_QUADRAT "{e}";
const char VEML_HTTP_UVINDEX[] PROGMEM = "{s}%s " D_UV_INDEX "{m}%s {e}";
const char VEML_JSON[]         PROGMEM =
    ",\"%s\":{\"" D_JSON_UVA_INTENSITY "\":%d,\""
                  D_JSON_UVB_INTENSITY "\":%d,\""
                  D_JSON_UV_INDEX     "\":%s}";
const char VEML_S_JSON_NVALUE[] PROGMEM =
    "{\"" D_NAME_VEML6075 "\":{\"%s\":%d}}";

const char kVEML_Commands[] PROGMEM =
    D_CMND_VEML6075_POWER   "|"
    D_CMND_VEML6075_DYNAMIC "|"
    D_CMND_VEML6075_INTTIME;

enum VEML6075_Commands {
  CMND_VEML6075_PWR,
  CMND_VEML6075_SET_HD,
  CMND_VEML6075_SET_UVIT,
};

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("VEML6075", MODULE_TYPE_SENSOR, 1 << 16 | 5,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART uint16_t VEML6075read16(uint8_t reg);
MODULE_PART void     VEML6075write16(uint8_t reg, uint16_t val);
MODULE_PART float    VEML6075calcUVA(void);
MODULE_PART float    VEML6075calcUVB(void);
MODULE_PART float    VEML6075calcUVI(void);
MODULE_PART void     VEML6075SetHD(uint8_t val);
MODULE_PART uint8_t  VEML6075ReadHD(void);
MODULE_PART void     VEML6075SetUvIt(uint8_t val);
MODULE_PART uint8_t  VEML6075GetUvIt(void);
MODULE_PART void     VEML6075Pwr(uint8_t val);
MODULE_PART uint8_t  VEML6075GetPwr(void);
MODULE_PART void     VEML6075ReadData(void);
MODULE_PART bool     VEML6075init(void);
MODULE_PART bool     VEML6075Detect(void);
MODULE_PART void     VEML6075EverySecond(void);
MODULE_PART bool     VEML6075Cmd(void);
MODULE_PART void     VEML6075Show(bool json);
MODULE_PART void     VEML6075_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t function);
#endif
MODULE_END

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
typedef struct {
  char     types[9];
  uint8_t  address;
  uint8_t  bus;            // I2C bus the sensor was found on
  uint8_t  inttime;
  uint16_t uva;
  uint16_t uvb;
  uint16_t uva_raw;
  uint16_t uvb_raw;
  uint16_t comp1;
  uint16_t comp2;
  uint16_t conf;
  float    uvi;
} VEML6075STRUCT;

typedef union {
  struct {
    uint8_t pwr            : 1;  // shut-down (0) / normal (1)
    uint8_t forded_auto    : 1;  // auto vs forced
    uint8_t forced_trigger : 1;  // trigger forced mode
    uint8_t hd             : 1;  // high-dynamic
    uint8_t inttime        : 3;  // integration time
    uint8_t spare7         : 1;
  };
  uint16_t config;
} veml6075configRegister;

// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  veml6075_state_t
#endif

typedef struct {
  TWIp                  *xWire;
  uint8_t                veml6075_active;
  veml6075configRegister veml6075Config;
  VEML6075STRUCT         veml6075_sensor;
  bool                   initialized_flag;
} MODULE_MEMORY;

#define veml6075_active mem->veml6075_active
#define veml6075Config  mem->veml6075Config
#define veml6075_sensor mem->veml6075_sensor

#if !BUILD_AS_PLUGIN

static veml6075_state_t *veml6075_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = veml6075_state;
#  define ALLOCMEM \
       if (!veml6075_state) veml6075_state = (veml6075_state_t *)calloc(1, sizeof(veml6075_state_t)); \
       if (!veml6075_state) return -1; \
       MODULE_MEMORY *mem = veml6075_state;
#  define RETMEM \
       if (veml6075_state) { free(veml6075_state); veml6075_state = nullptr; }
#  define initialized     mem->initialized_flag

#  define XSNS_70         70
#  define XI2C_49         49

#endif  // !BUILD_AS_PLUGIN

// XdrvMailbox access pattern (pointer in plugin, instance in native)
#if BUILD_AS_PLUGIN
#  define _VEML_MB_TOPIC    (XdrvMailbox->topic)
#  define _VEML_MB_DATA_LEN (XdrvMailbox->data_len)
#  define _VEML_MB_PAYLOAD  (XdrvMailbox->payload)
#else
#  define _VEML_MB_TOPIC    (XdrvMailbox.topic)
#  define _VEML_MB_DATA_LEN (XdrvMailbox.data_len)
#  define _VEML_MB_PAYLOAD  (XdrvMailbox.payload)
#endif

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------

// VEML6075 is little-endian on the wire but I2C 16-bit helpers
// return MSB-first; swap to get the native value.
uint16_t VEML6075read16(uint8_t reg) {
  SETREGS
  uint16_t swap = I2C_Read16(VEML6075_ADDR, reg, 0);
  return ((swap & 0xFF) << 8) | (swap >> 8);
}

void VEML6075write16(uint8_t reg, uint16_t val) {
  SETREGS
  uint16_t swap = ((val & 0xFF) << 8) | (val >> 8);
  I2C_Write16(VEML6075_ADDR, reg, swap, 0);
}

float VEML6075calcUVA(void) {
  SETREGS
  float fvar1 = fmul(VEML6075_DEFAULT_UVA_A_COEFF, floatunsisf(veml6075_sensor.comp1));
  float fvar2 = fmul(VEML6075_DEFAULT_UVA_B_COEFF, floatunsisf(veml6075_sensor.comp2));
  return fdiff(floatunsisf(veml6075_sensor.uva_raw), fadd(fvar1, fvar2));
}

float VEML6075calcUVB(void) {
  SETREGS
  float fvar1 = fmul(VEML6075_DEFAULT_UVB_C_COEFF, floatunsisf(veml6075_sensor.comp1));
  float fvar2 = fmul(VEML6075_DEFAULT_UVB_D_COEFF, floatunsisf(veml6075_sensor.comp2));
  return fdiff(floatunsisf(veml6075_sensor.uvb_raw), fadd(fvar1, fvar2));
}

float VEML6075calcUVI(void) {
  SETREGS
  float faresp = *GFLT(&VEML_UVA_RESPONSIVITY[veml6075_sensor.inttime]);
  float fbresp = *GFLT(&VEML_UVB_RESPONSIVITY[veml6075_sensor.inttime]);
  float fvar1 = fmul(floatunsisf(veml6075_sensor.uva), faresp);
  float fvar2 = fmul(floatunsisf(veml6075_sensor.uvb), fbresp);
  return fadd(fvar1, fvar2);
}

void VEML6075SetHD(uint8_t val) {
  SETREGS
  veml6075Config.hd = val;
  VEML6075write16(VEML6075_REG_CONF, veml6075Config.config);
}

uint8_t VEML6075ReadHD(void) {
  SETREGS
  veml6075Config.config = VEML6075read16(VEML6075_REG_CONF);
  return veml6075Config.hd;
}

void VEML6075SetUvIt(uint8_t val) {
  SETREGS
  veml6075Config.inttime = val;
  VEML6075Pwr(1);
  VEML6075write16(VEML6075_REG_CONF, veml6075Config.config);
  VEML6075Pwr(0);
}

uint8_t VEML6075GetUvIt(void) {
  SETREGS
  veml6075Config.config = VEML6075read16(VEML6075_REG_CONF);
  return veml6075Config.inttime;
}

void VEML6075Pwr(uint8_t val) {
  SETREGS
  veml6075Config.pwr = val;
  VEML6075write16(VEML6075_REG_CONF, veml6075Config.config);
}

uint8_t VEML6075GetPwr(void) {
  SETREGS
  veml6075Config.config = VEML6075read16(VEML6075_REG_CONF);
  return veml6075Config.pwr;
}

void VEML6075ReadData(void) {
  SETREGS
  I2C_SETWIRE(veml6075_sensor.bus);
  veml6075_sensor.uva_raw = VEML6075read16(VEML6075_REG_UVA);
  veml6075_sensor.uvb_raw = VEML6075read16(VEML6075_REG_UVB);
  veml6075_sensor.comp1   = VEML6075read16(VEML6075_REG_UVCOMP1);
  veml6075_sensor.comp2   = VEML6075read16(VEML6075_REG_UVCOMP2);
  veml6075_sensor.inttime = VEML6075GetUvIt();
  veml6075_sensor.uva     = fixunssfsi(VEML6075calcUVA());
  veml6075_sensor.uvb     = fixunssfsi(VEML6075calcUVB());
  veml6075_sensor.uvi     = VEML6075calcUVI();
}

bool VEML6075init(void) {
  SETREGS
  uint8_t id = VEML6075read16(VEML6075_REG_ID);
  return (id == VEML6075_CHIP_ID);
}

bool VEML6075Detect(void) {
  ALLOCMEM

  veml6075_sensor.address = VEML6075_ADDR;
  strcpy_P(veml6075_sensor.types, PSTR(D_NAME_VEML6075));

  // Probe both I2C buses. First responder with the right chip-ID wins.
  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(VEML6075_ADDR, bus)) { continue; }
    if (VEML6075init()) {
      veml6075_sensor.bus = bus;
      I2C_SetActiveFound(VEML6075_ADDR, veml6075_sensor.types, bus);
      VEML6075write16(VEML6075_REG_CONF, 0x10);  // sensible default
      veml6075_active = 1;
      initialized = true;
      return true;
    }
    I2C_ResetActive(VEML6075_ADDR, bus);
  }

  VEML6075_Deinit();
  return false;
}

void VEML6075EverySecond(void) {
  SETREGS
  VEML6075ReadData();
}

bool VEML6075Cmd(void) {
  SETREGS
  char    command[CMDSZ];
  uint8_t name_len = 8;  // strlen("VEML6075")

  if (strncasecmp_P(_VEML_MB_TOPIC, PSTR(D_NAME_VEML6075), name_len) != 0) {
    return false;
  }
  uint32_t command_code = GetCommandCode(command, sizeof(command),
                                         _VEML_MB_TOPIC + name_len,
                                         GSTR(kVEML_Commands));
  // Pin the wire to this sensor's bus before any I2C traffic the
  // sub-handlers will issue.
  I2C_SETWIRE(veml6075_sensor.bus);

  switch (command_code) {
    case CMND_VEML6075_PWR:
      if (_VEML_MB_DATA_LEN && (2 >= _VEML_MB_PAYLOAD)) {
        VEML6075Pwr(_VEML_MB_PAYLOAD);
      }
      Response_P(GSTR(VEML_S_JSON_NVALUE), command, VEML6075GetPwr());
      break;
    case CMND_VEML6075_SET_HD:
      if (_VEML_MB_DATA_LEN && (2 >= _VEML_MB_PAYLOAD)) {
        VEML6075SetHD(_VEML_MB_PAYLOAD);
      }
      Response_P(GSTR(VEML_S_JSON_NVALUE), command, VEML6075ReadHD());
      break;
    case CMND_VEML6075_SET_UVIT:
      if (_VEML_MB_DATA_LEN && (4 >= _VEML_MB_PAYLOAD)) {
        VEML6075SetUvIt(_VEML_MB_PAYLOAD);
      }
      Response_P(GSTR(VEML_S_JSON_NVALUE), command, VEML6075GetUvIt());
      break;
    default:
      return false;
  }
  return true;
}

void VEML6075Show(bool json) {
  SETREGS
  char s_uvindex[FLOATSZ];
  ftostrfd(veml6075_sensor.uvi, 1, s_uvindex);

  if (json) {
    ResponseAppend_P(GSTR(VEML_JSON), veml6075_sensor.types,
                     veml6075_sensor.uva, veml6075_sensor.uvb, s_uvindex);
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(VEML_HTTP_UVA),     veml6075_sensor.types, veml6075_sensor.uva);
    WSContentSend_PD(GSTR(VEML_HTTP_UVB),     veml6075_sensor.types, veml6075_sensor.uvb);
    WSContentSend_PD(GSTR(VEML_HTTP_UVINDEX), veml6075_sensor.types, s_uvindex);
#endif
  }
}

void VEML6075_Deinit(void) {
  SETREGS
  I2C_ResetActive(VEML6075_ADDR, veml6075_sensor.bus);
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t function) {
  bool result = false;
  switch (function) {
    case pFUNC_INIT:         result = VEML6075Detect(); break;
    case pFUNC_EVERY_SECOND: VEML6075EverySecond();     break;
    case pFUNC_COMMAND:      result = VEML6075Cmd();    break;
    case pFUNC_JSON_APPEND:  VEML6075Show(1);           break;
    case pFUNC_WEB_SENSOR:   VEML6075Show(0);           break;
    case pFUNC_DEINIT:       VEML6075_Deinit();         break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns70(uint32_t function) {
  if (!I2cEnabled(XI2C_49)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    VEML6075Detect();
  } else if (veml6075_state && veml6075_active) {
    switch (function) {
      case FUNC_EVERY_SECOND: VEML6075EverySecond();    break;
      case FUNC_COMMAND:      result = VEML6075Cmd();   break;
      case FUNC_JSON_APPEND:  VEML6075Show(1);          break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   VEML6075Show(0);          break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef all state-accessor and helper macros
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef veml6075_active
#  undef veml6075Config
#  undef veml6075_sensor
#  undef mem_veml
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef _VEML_MB_TOPIC
#undef _VEML_MB_DATA_LEN
#undef _VEML_MB_PAYLOAD

#endif  // _VEML6075_DUAL_ENABLED
