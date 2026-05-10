/*
  xsns_44_sps30_dual.cpp — Sensirion SPS30 particulate-matter sensor
  driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2021 Gerhard Mutz, Theo Arends

  Plugin: USE_SPS30_DUAL_MOD via build_plugin.py.
  Native: USE_SPS30_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_44_sps30_dual.ino.

  Single fixed I2C address (0x69). Probes both buses.

  Reports PM1.0 / PM2.5 / PM4.0 / PM10 mass concentrations and
  NCPM0.5 / NCPM1.0 / NCPM2.5 / NCPM4.0 / NCPM10 number concentrations
  plus typical particle size (TYPSIZ). Has a fan auto-clean cycle
  that fires every 7 days of runtime (driven from the every-second
  tick — `Settings->sps30_inuse_hours` persists the runtime counter).

  Console commands:
    SPS30Start  — resume measurement
    SPS30Stop   — stop measurement (sensor enters idle/low-power)
    SPS30Clean  — fire fan-clean cycle now
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_SPS30_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_SPS30_DUAL_MOD
#    define _SPS30_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_SPS30_DUAL) && defined(SPS30_DUAL_NATIVE_INCLUDE)
#    define _SPS30_DUAL_ENABLED 1
#  endif
#endif

#ifdef _SPS30_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define SPS30_ADDR                     0x69
#define SPS30_REV                      (1 << 16 | 5)

#define SPS_CMD_START_MEASUREMENT      0x0010
#define SPS_CMD_START_MEASUREMENT_ARG  0x0300
#define SPS_CMD_STOP_MEASUREMENT       0x0104
#define SPS_CMD_READ_MEASUREMENT       0x0300
#define SPS_CMD_GET_DATA_READY         0x0202
#define SPS_CMD_AUTOCLEAN_INTERVAL     0x8004
#define SPS_CMD_CLEAN                  0x5607
#define SPS_CMD_GET_ACODE              0xd025
#define SPS_CMD_GET_SERIAL             0xd033
#define SPS_CMD_RESET                  0xd304

#define SPS_WRITE_DELAY_US             20000
#define SPS_MAX_SERIAL_LEN             32

#define PMDP                            2     // decimal places in particle output

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("SPS30", MODULE_TYPE_SENSOR, SPS30_REV,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART int32_t SPS30_Init(void);
MODULE_PART void    SPS30_Every_Second(void);
MODULE_PART void    SPS30_Show(bool json);
MODULE_PART void    SPS30_Deinit(void);
MODULE_PART uint8_t sps30_calc_CRC(uint8_t *data);
MODULE_PART void    sps30_cmd(uint16_t cmd);
MODULE_PART bool    SPS30_command(void);
MODULE_PART void    CmdClean(void);
MODULE_PART void    sps30_get_data(uint16_t cmd, uint8_t *data, uint8_t dlen);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// Per-driver PROGMEM strings (file-unique `_SPS` suffix where they'd
// otherwise be too generic)
// --------------------------------------------------------------------
const char SPS30_NAME_SPS[]   PROGMEM = "SPS30";
const char SPS30_serial_SPS[] PROGMEM = "sps30 found at bus %d, serial: %s";

const char HTTP_SNS_SPS30_a[] PROGMEM = "{s}SPS30 PM %0d.%0d{m}%s ug/m3{e}";
const char HTTP_SNS_SPS30_b[] PROGMEM = "{s}SPS30 NCPM %0d.%0d{m}%s #/cm3{e}";
const char HTTP_SNS_SPS30_c[] PROGMEM = "{s}SPS30 TYPSIZ {m}%s um{e}";

const char JSON_SNS_SPS30_a[] PROGMEM = ",\"SPS30\":{\"PM%0d_%0d\":%s";
const char JSON_SNS_SPS30_b[] PROGMEM = ",\"PM%0d_%0d\":%s";
const char JSON_SNS_SPS30_c[] PROGMEM = ",\"NCPM%0d_%0d\":%s";
const char JSON_SNS_SPS30_d[] PROGMEM = ",\"TYPSIZ\":%s}";

const char S_JSON_SPS30_FAN[]     PROGMEM = ",\"SPS30\":{\"CFAN\":\"true\"}}";
const char kSPS30_Commands_SPS[]  PROGMEM = "Start|Stop|Clean";
const char S_JSON_SPS30_COMMAND[] PROGMEM = "{\"SPS30\":\"%s\"}";
const char S_JSON_SPS30_r[]       PROGMEM = "running";
const char S_JSON_SPS30_s[]       PROGMEM = "stopped";

enum SPS30_Commands { CMND_SPS30_Start, CMND_SPS30_Stop, CMND_SPS30_Clean };

// --------------------------------------------------------------------
// Measurement struct + state storage — heap in both modes
// --------------------------------------------------------------------
typedef struct {
  float PM1_0;
  float PM2_5;
  float PM4_0;
  float PM10;
  float NCPM0_5;
  float NCPM1_0;
  float NCPM2_5;
  float NCPM4_0;
  float NCPM10;
  float TYPSIZ;
} SPS30_DATA;

#if BUILD_AS_PLUGIN

typedef struct {
  TWIp      *xWire;
  SPS30_DATA sps30_result;
  bool       sps30_running;
  bool       ready;
  uint8_t    bus;
  uint16_t   secs;
} MODULE_MEMORY;

#  define sps30_result   mem->sps30_result
#  define sps30_running  mem->sps30_running
#  define ready          mem->ready
#  define sps_bus        mem->bus
#  define secs           mem->secs

#else  // native

typedef struct {
  SPS30_DATA sps30_result;
  bool       sps30_running;
  bool       ready;
  uint8_t    bus;
  uint16_t   secs;
  bool       initialized_flag;
} sps30_state_t;

static sps30_state_t *sps30_state = nullptr;

#  define sps30_result   sps30_state->sps30_result
#  define sps30_running  sps30_state->sps30_running
#  define ready          sps30_state->ready
#  define sps_bus        sps30_state->bus
#  define secs           sps30_state->secs
#  define initialized    sps30_state->initialized_flag

#  define ALLOCMEM       DUAL_ALLOCMEM(sps30)
#  define RETMEM         DUAL_RETMEM(sps30)

#  define XSNS_44        44
#  define XI2C_31        31

#endif  // BUILD_AS_PLUGIN

// XdrvMailbox accessors (pointer-vs-instance)
#if BUILD_AS_PLUGIN
#  define _SPS_MB_TOPIC  (XdrvMailbox->topic)
#else
#  define _SPS_MB_TOPIC  (XdrvMailbox.topic)
#endif

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------

uint8_t sps30_calc_CRC(uint8_t *data) {
  uint8_t crc = 0xFF;
  for (uint32_t i = 0; i < 2; i++) {
    crc ^= data[i];
    for (uint32_t bit = 8; bit > 0; --bit) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31u : (crc << 1);
    }
  }
  return crc;
}

void sps30_get_data(uint16_t cmd, uint8_t *data, uint8_t dlen) {
  SETREGS
  I2C_SETWIRE(sps_bus);

  uint8_t tmp[3];
  uint8_t index = 0;
  memset(data, 0, dlen);
  uint8_t twi_buff[64];
  memset(twi_buff, 0, sizeof(twi_buff));

  I2C_beginTransmission(SPS30_ADDR);
  I2C_write(cmd >> 8);
  I2C_write(cmd);
  I2C_endTransmission(true);

  // each datum is 2 bytes value + 1 byte CRC; need (dlen/2)*3 bytes.
  dlen /= 2;
  dlen *= 3;

#ifdef ESP8266
  I2C_readFrom(SPS30_ADDR, twi_buff, dlen, 1);
#endif
#ifdef ESP32
  I2C_requestFrom((uint16_t)SPS30_ADDR, dlen);
  for (uint32_t cnt = 0; cnt < dlen; cnt++) {
    twi_buff[cnt] = I2C_read();
  }
#endif

  uint8_t bind = 0;
  while (bind < dlen) {
    tmp[0] = twi_buff[bind++];
    tmp[1] = twi_buff[bind++];
    tmp[2] = twi_buff[bind++];
    if (sps30_calc_CRC(tmp) != tmp[2]) {
      // chksum error — leave data slot zero, advance index past it
      index += 2;
    } else {
      data[index++] = tmp[0];
      data[index++] = tmp[1];
    }
  }
}

void sps30_cmd(uint16_t cmd) {
  SETREGS
  I2C_SETWIRE(sps_bus);

  unsigned char cmdb[6];
  I2C_beginTransmission(SPS30_ADDR);
  cmdb[0] = cmd >> 8;
  cmdb[1] = cmd & 0xff;

  uint8_t num = 2;
  if (cmd == SPS_CMD_START_MEASUREMENT) {
    cmdb[2] = SPS_CMD_START_MEASUREMENT_ARG >> 8;
    cmdb[3] = SPS_CMD_START_MEASUREMENT_ARG & 0xff;
    cmdb[4] = sps30_calc_CRC(&cmdb[2]);
    num = 5;
  }
  for (uint16_t cnt = 0; cnt < num; cnt++) {
    I2C_write(cmdb[cnt]);
  }
  I2C_endTransmission(true);
}

int32_t SPS30_Init(void) {
  ALLOCMEM

  sps_bus = 0;
  bool found = false;

  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(SPS30_ADDR, bus)) { continue; }
    sps_bus = bus;

    uint8_t dcode[32];
    sps30_get_data(SPS_CMD_GET_SERIAL, dcode, sizeof(dcode));
    if (dcode[0] != 0) {
      AddLog(LOG_LEVEL_INFO, GSTR(SPS30_serial_SPS), (int)bus, dcode);
      I2C_SetActiveFound(SPS30_ADDR, GSTR(SPS30_NAME_SPS), bus);
      found = true;
      break;
    }
    I2C_ResetActive(SPS30_ADDR, bus);
  }
  if (!found) {
    SPS30_Deinit();
    return -1;
  }

  sps30_cmd(SPS_CMD_START_MEASUREMENT);
  sps30_running = 1;
  ready         = 1;
  initialized   = 1;
  secs          = 0;
  return 0;
}

void SPS30_Every_Second(void) {
  SETREGS
  if (!ready)         { return; }
  if (!sps30_running) { return; }

  // Read measurements every 10 seconds (sensor only updates that often).
  if (tmod__umodsi3(secs, 10) == 0) {
    uint8_t vars[sizeof(float) * 10];
    sps30_get_data(SPS_CMD_READ_MEASUREMENT, vars, sizeof(vars));

    typedef union { uint8_t array[4]; float value; } ByteToFloat;
    ByteToFloat conv;
    float *fp = &sps30_result.PM1_0;
    for (uint32_t count = 0; count < 10; count++) {
      // SPS30 is big-endian on the wire; reverse byte order into float.
      for (uint32_t i = 0; i < 4; i++) {
        conv.array[3 - i] = vars[count * sizeof(float) + i];
      }
      *fp++ = conv.value;
    }
  }
  secs++;

  // Auto-clean once per week of runtime — Settings->sps30_inuse_hours
  // persists across reboots (Tasmota's user-settings store).
  if (secs > 3600) {
    secs = 0;
    Settings->sps30_inuse_hours++;
    if (Settings->sps30_inuse_hours > (7 * 24)) {
      CmdClean();
      Settings->sps30_inuse_hours = 0;
    }
  }
}

void SPS30_Show(bool json) {
  SETREGS
  if (!ready)         { return; }
  if (!sps30_running) { return; }

  char str[64];
  if (json) {
    ftostrfd(sps30_result.PM1_0,   PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_a), 1, 0, str);
    ftostrfd(sps30_result.PM2_5,   PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_b), 2, 5, str);
    ftostrfd(sps30_result.PM4_0,   PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_b), 4, 0, str);
    ftostrfd(sps30_result.PM10,    PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_b), 10, 0, str);
    ftostrfd(sps30_result.NCPM0_5, PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 0, 5, str);
    ftostrfd(sps30_result.NCPM1_0, PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 1, 0, str);
    ftostrfd(sps30_result.NCPM2_5, PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 2, 5, str);
    ftostrfd(sps30_result.NCPM4_0, PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 4, 0, str);
    ftostrfd(sps30_result.NCPM10,  PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_c), 10, 0, str);
    ftostrfd(sps30_result.TYPSIZ,  PMDP, str); ResponseAppend_P(GSTR(JSON_SNS_SPS30_d), str);
#ifdef USE_WEBSERVER
  } else {
    ftostrfd(sps30_result.PM1_0,   PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 1, 0, str);
    ftostrfd(sps30_result.PM2_5,   PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 2, 5, str);
    ftostrfd(sps30_result.PM4_0,   PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 4, 0, str);
    ftostrfd(sps30_result.PM10,    PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_a), 10, 0, str);
    ftostrfd(sps30_result.NCPM0_5, PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 0, 5, str);
    ftostrfd(sps30_result.NCPM1_0, PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 1, 0, str);
    ftostrfd(sps30_result.NCPM2_5, PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 2, 5, str);
    ftostrfd(sps30_result.NCPM4_0, PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 4, 0, str);
    ftostrfd(sps30_result.NCPM10,  PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_b), 10, 0, str);
    ftostrfd(sps30_result.TYPSIZ,  PMDP, str); WSContentSend_PD(GSTR(HTTP_SNS_SPS30_c), str);
#endif
  }
}

void CmdClean(void) {
  SETREGS
  sps30_cmd(SPS_CMD_CLEAN);
  ResponseTime_P(GSTR(S_JSON_SPS30_FAN));
  MqttPublishTeleSensor();
}

bool SPS30_command(void) {
  SETREGS
  char    command[CMDSZ];
  bool    serviced = false;
  uint8_t disp_len = strlen((char *)GSTR(SPS30_NAME_SPS));

  if (strncasecmp_P(_SPS_MB_TOPIC, GSTR(SPS30_NAME_SPS), disp_len) != 0) {
    return false;
  }
  serviced = true;
  int command_code = GetCommandCode(command, sizeof(command),
                                    _SPS_MB_TOPIC + disp_len,
                                    GSTR(kSPS30_Commands_SPS));
  switch (command_code) {
    case CMND_SPS30_Start:
      sps30_running = 1;
      sps30_cmd(SPS_CMD_START_MEASUREMENT);
      break;
    case CMND_SPS30_Stop:
      sps30_running = 0;
      sps30_cmd(SPS_CMD_STOP_MEASUREMENT);
      break;
    case CMND_SPS30_Clean:
      CmdClean();
      break;
    default:
      serviced = false;
  }
  Response_P(GSTR(S_JSON_SPS30_COMMAND),
             sps30_running ? GSTR(S_JSON_SPS30_r) : GSTR(S_JSON_SPS30_s));
  return serviced;
}

void SPS30_Deinit(void) {
  SETREGS
  I2C_ResetActive(SPS30_ADDR, sps_bus);
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = SPS30_Init();    break;
    case pFUNC_EVERY_SECOND: SPS30_Every_Second();     break;
    case pFUNC_JSON_APPEND:  SPS30_Show(1);            break;
    case pFUNC_WEB_SENSOR:   SPS30_Show(0);            break;
    case pFUNC_COMMAND:      result = SPS30_command(); break;
    case pFUNC_DEINIT:       SPS30_Deinit();           break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns44(uint32_t function) {
  if (!I2cEnabled(XI2C_31)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    SPS30_Init();
  } else if (sps30_state && ready) {
    switch (function) {
      case FUNC_EVERY_SECOND: SPS30_Every_Second(); break;
      case FUNC_JSON_APPEND:  SPS30_Show(1);        break;
      case FUNC_COMMAND:      result = SPS30_command(); break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   SPS30_Show(0);        break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef sps30_result
#  undef sps30_running
#  undef ready
#  undef sps_bus
#  undef secs
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef _SPS_MB_TOPIC

#endif  // _SPS30_DUAL_ENABLED
