/*
  xsns_46_MLX90614_dual.cpp — MLX90614 IR thermometer driver,
  dual-format. Compat shim in dual_format_compat.h.

  Plugin: USE_MLX90614_DUAL_MOD via build_plugin.py.
  Native: USE_MLX90614_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_46_MLX90614_dual.ino.

  Original copyright preserved.
  Copyright (C) 2021 Gerhard Mutz
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_MLX90614_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_MLX90614_DUAL_MOD
#    define _MLX90614_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_MLX90614_DUAL) && defined(MLX90614_DUAL_NATIVE_INCLUDE)
#    define _MLX90614_DUAL_ENABLED 1
#  endif
#endif

#ifdef _MLX90614_DUAL_ENABLED

#define I2_ADR_IRT       0x5a
#define MLX90614_RAWIR1  0x04
#define MLX90614_RAWIR2  0x05
#define MLX90614_TA      0x06
#define MLX90614_TOBJ1   0x07
#define MLX90614_TOBJ2   0x08

const char HTTP_IRTMP[] PROGMEM = "{s}MXL90614 OBJ-TEMP{m}%s C{e} {s}MXL90614 AMB-TEMP {m}%s C{e}";
const char JSON_IRTMP[] PROGMEM = ",\"MLX90614\":{\"OBJTMP\":%s,\"AMBTMP\":%s}";
const char mlxdev[]     PROGMEM = "MLX90614";
const float FP_CONST[]  PROGMEM = {-999, 0.02, 273.15};

// Forward decls
MODULE_PART int32_t   Init_MLX90614();
MODULE_PART uint16_t  MLX90614_read16(uint8_t addr, uint8_t a);
MODULE_PART uint8_t   MLX90614_jcrc8(uint8_t *addr, uint8_t len);
MODULE_PART void      MLX90614_Deinit();
MODULE_PART float     MLX90614_GetValue(uint32_t reg);
MODULE_PART void      MLX90614_Every_Second();
MODULE_PART void      MLX90614_Show(uint32_t json);
#if BUILD_AS_PLUGIN
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);
#endif

// Plugin descriptor (plugin-only). Note: original ships TWO descriptor
// variants — MLX90614S (with USE_SOFTWIRE pin args) and plain MLX90614
// — gated by USE_SOFTWIRE. We replicate both so the plugin .bin
// produced matches whichever variant the build environment expects.
#if BUILD_AS_PLUGIN
#  define MLX90614_REV (1 << 16 | 4)
PUSH_OPTIONS
#  ifdef USE_SOFTWIRE
#    define DEFAULT_SDA_PIN 12
#    define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("MLX90614S", MODULE_TYPE_SENSOR, MLX90614_REV,
                  "SDA", DEFAULT_SDA_PIN, "SCL", DEFAULT_SCL_PIN, "", 0, "", 0)
#  else
MODULE_DESCRIPTOR("MLX90614",  MODULE_TYPE_SENSOR, MLX90614_REV,
                  "", 0, "", 0, "", 0, "", 0)
#  endif
MODULE_END
#endif

// State storage
#if BUILD_AS_PLUGIN

typedef struct {
  float  obj_temp;
  float  amb_temp;
  bool   ready;
  TWIp  *xWire;
} MODULE_MEMORY;

#  define obj_temp        mem->obj_temp
#  define amb_temp        mem->amb_temp
#  define ready           mem->ready

#  ifdef USE_SOFTWIRE
#    include "Softwire/Softwire_cpp.h"
#  endif

#else  // native

typedef struct {
  float  obj_temp;
  float  amb_temp;
  bool   ready;
  bool   initialized_flag;
} mlx90614_state_t;

static mlx90614_state_t *mlx90614_state = nullptr;

#  define obj_temp        mlx90614_state->obj_temp
#  define amb_temp        mlx90614_state->amb_temp
#  define ready           mlx90614_state->ready
#  define initialized     mlx90614_state->initialized_flag

#  define ALLOCMEM        DUAL_ALLOCMEM(mlx90614)
#  define RETMEM          DUAL_RETMEM(mlx90614)

#  define XSNS_46         46
#  define XI2C_32         32

#endif  // BUILD_AS_PLUGIN

// Driver core
int32_t Init_MLX90614() {
  ALLOCMEM
  ready = false;
  I2C_SETWIRE(0);
  if (!I2C_SetDevice(I2_ADR_IRT, 0)) {
    MLX90614_Deinit();
    return -1;
  }
  I2C_SetActiveFound(I2_ADR_IRT, GSTR(mlxdev), 0);
  initialized = true;
  ready = true;
  return ready;
}

float MLX90614_GetValue(uint32_t reg) {
  SETREGS
  uint16_t val = MLX90614_read16(I2_ADR_IRT, reg);
  if (val & 0x8000) {
    return FLTC(0);
  }
  return fscale(val, FLTC(1), FLTC(2));
}

void MLX90614_Every_Second() {
  SETREGS
  if (!ready) { return; }
  obj_temp = MLX90614_GetValue(MLX90614_TOBJ1);
  amb_temp = MLX90614_GetValue(MLX90614_TA);
}

void MLX90614_Show(uint32_t json) {
  SETREGS
  if (!ready) { return; }
  char obj_tstr[16];
  ftostrfd(obj_temp, Settings->flag2.temperature_resolution, obj_tstr);
  char amb_tstr[16];
  ftostrfd(amb_temp, Settings->flag2.temperature_resolution, amb_tstr);
  if (json) {
    ResponseAppend_P(GSTR(JSON_IRTMP), obj_tstr, amb_tstr);
  } else {
    WSContentSend_PD(GSTR(HTTP_IRTMP), obj_tstr, amb_tstr);
  }
}

uint16_t MLX90614_read16(uint8_t addr, uint8_t a) {
  SETREGS
  I2C_beginTransmission(addr);
  I2C_write(a);
  I2C_endTransmission(false);

  I2C_requestFrom(addr, (size_t)3);
  uint8_t buff[5];
  buff[0] = addr << 1;
  buff[1] = a;
  buff[2] = (addr << 1) | 1;
  buff[3] = I2C_read();
  buff[4] = I2C_read();
  uint16_t ret = buff[3] | (buff[4] << 8);
  uint8_t pec = I2C_read();
  (void)pec;
  return ret;
}

uint8_t MLX90614_jcrc8(uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t carry = (crc ^ inbyte) & 0x80;
      crc <<= 1;
      if (carry) { crc ^= 0x7; }
      inbyte <<= 1;
    }
  }
  return crc;
}

void MLX90614_Deinit() {
  SETREGS
  I2C_ResetActive(I2_ADR_IRT, 0);
  RETMEM
}

// Dispatcher
#if BUILD_AS_PLUGIN

MOD_RESULT mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = Init_MLX90614();      break;
    case pFUNC_JSON_APPEND:  MLX90614_Show(1);              break;
    case pFUNC_WEB_SENSOR:   MLX90614_Show(0);              break;
    case pFUNC_EVERY_SECOND: MLX90614_Every_Second();       break;
    case pFUNC_DEINIT:       MLX90614_Deinit();             break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns46(uint32_t function) {
  if (!I2cEnabled(XI2C_32)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    Init_MLX90614();
  }
  else if (mlx90614_state) {
    switch (function) {
      case FUNC_EVERY_SECOND: MLX90614_Every_Second(); break;
      case FUNC_JSON_APPEND:  MLX90614_Show(1);        break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   MLX90614_Show(0);        break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

#endif  // _MLX90614_DUAL_ENABLED
