/*
  xsns_124_TCS34725_dual.cpp — TCS34725 RGB / ambient light + colour
  temperature sensor driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2018 Theo Arends, Gerhard Mutz, Adafruit
    (autorange wrapper class: ductsoup, public domain)

  Plugin: USE_TCS34725_DUAL_MOD via build_plugin.py.
  Native: USE_TCS34725_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_124_TCS34725_dual.ino.

  Single fixed I2C address (0x29). Probe scans both buses; first
  responder with a valid TCS34725 ID (0x44 or 0x10) wins.

  Note: TCS34725's 0x29 address COLLIDES with VL53L0X. Both sensors
  cannot share a bus. The dual form lets you put one on Wire and
  the other on Wire1, which the legacy bus-0-only plugin couldn't do.

  Underlying chip driver is the Adafruit_TCS34725 class — its
  begin(addr, theWire) overload lets us pin the right Wire instance
  per detected bus, so reads after init go to the right peripheral.
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_TCS34725_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_TCS34725_DUAL_MOD
#    define _TCS34725_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_TCS34725_DUAL) && defined(TCS34725_DUAL_NATIVE_INCLUDE)
#    define _TCS34725_DUAL_ENABLED 1
#  endif
#endif

#ifdef _TCS34725_DUAL_ENABLED

#include "TCS34725/Adafruit_TCS34725_cpp.h"

// --------------------------------------------------------------------
// Constants — DN40 application-note magic numbers from AMS.
// --------------------------------------------------------------------
#define TCS34725_R_Coef     0.136
#define TCS34725_G_Coef     1.000
#define TCS34725_B_Coef    -0.444
#define TCS34725_GA         1.0
#define TCS34725_DF         310.0
#define TCS34725_CT_Coef    3810.0
#define TCS34725_CT_Offset  1391.0

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: macros are empty → plain C++ forward decls.
// Plugin: MODULE_PART decls land in SECTION_PART between descriptor
// and MODULE_END.
//
// Class out-of-line method DEFINITIONS (further down) are themselves
// MODULE_PART-decorated, so they don't need separate forward decls
// here — only the C-callable driver-hook functions do.
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("TCS34725", MODULE_TYPE_SENSOR, 1 << 16 | 5,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART MOD_RESULT TCS34725_Detect(void);
MODULE_PART void       TCS34725_EverySecond(void);
MODULE_PART void       TCS34725_Show(boolean json);
MODULE_PART void       TCS34725_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// Autorange wrapper class — ductsoup's tcs34725. Class definition
// at file scope (works in both modes; MODULE_PART markers on the
// out-of-line method bodies expand to nothing in native).
// --------------------------------------------------------------------
class tcs34725 {
public:
  tcs34725(void);
  boolean  begin(uint8_t bus);
  void     getData(void);
  void     getRawData_noDelay(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);

  boolean  isAvailable, isSaturated;
  uint16_t againx, atime, atime_ms;
  uint16_t r, g, b, c;
  uint16_t ir;
  uint16_t r_comp, g_comp, b_comp, c_comp;
  uint16_t saturation, saturation75;
  float    cratio, cpl, ct, lux, maxlux;

private:
  struct tcs_agc {
    tcs34725Gain_t            ag;
    tcs34725IntegrationTime_t at;
    uint16_t                  mincnt;
    uint16_t                  maxcnt;
  };
  // NOTE: was `static const tcs_agc agc_lst[]` with a file-scope
  // initialiser. That's banned in plugin context (Rule 1 in
  // dual_format_compat.h) — function- AND class-static initialised
  // arrays land in rodata that the plugin loader doesn't populate,
  // so reads come back as garbage and the AGC steps are wrong.
  // Now an instance member, populated per-element by begin().
  tcs_agc  agc_lst[5];
  uint16_t agc_cur;

  void              setGainTime(void);
  Adafruit_TCS34725 tcs;
};

// --------------------------------------------------------------------
// State storage — heap in both modes, holds the wrapper instance.
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

typedef struct {
  TwoWire *xWire;
  tcs34725 rgb_sensor;
  bool     ready;
  uint8_t  bus;
} MODULE_MEMORY;

#  define rgb_sensor  mem->rgb_sensor
#  define tcs_ready   mem->ready
#  define tcs_bus     mem->bus

#else  // native

typedef struct {
  tcs34725 rgb_sensor;
  bool     ready;
  uint8_t  bus;
  bool     initialized_flag;
} tcs34725_state_t;

static tcs34725_state_t *tcs34725_state = nullptr;

#  define rgb_sensor  tcs34725_state->rgb_sensor
#  define tcs_ready   tcs34725_state->ready
#  define tcs_bus     tcs34725_state->bus
#  define initialized tcs34725_state->initialized_flag

#  define ALLOCMEM    DUAL_ALLOCMEM(tcs34725)
#  define RETMEM      DUAL_RETMEM(tcs34725)

#  define XSNS_124    124
#  define XI2C_55     55

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Wrapper class — gain/integration steps and method bodies
// --------------------------------------------------------------------
MODULE_PART tcs34725::tcs34725() : agc_cur(0), isAvailable(0), isSaturated(0) {
}

MODULE_PART boolean tcs34725::begin(uint8_t bus) {
  // Populate AGC table per-element (plugin Rule 1: no static / no
  // initialiser-list arrays).
  agc_lst[0].ag = TCS34725_GAIN_60X; agc_lst[0].at = TCS34725_INTEGRATIONTIME_700MS;
  agc_lst[0].mincnt = 0;     agc_lst[0].maxcnt = 20000;
  agc_lst[1].ag = TCS34725_GAIN_60X; agc_lst[1].at = TCS34725_INTEGRATIONTIME_154MS;
  agc_lst[1].mincnt = 4990;  agc_lst[1].maxcnt = 63000;
  agc_lst[2].ag = TCS34725_GAIN_16X; agc_lst[2].at = TCS34725_INTEGRATIONTIME_154MS;
  agc_lst[2].mincnt = 16790; agc_lst[2].maxcnt = 63000;
  agc_lst[3].ag = TCS34725_GAIN_4X;  agc_lst[3].at = TCS34725_INTEGRATIONTIME_154MS;
  agc_lst[3].mincnt = 15740; agc_lst[3].maxcnt = 63000;
  agc_lst[4].ag = TCS34725_GAIN_1X;  agc_lst[4].at = TCS34725_INTEGRATIONTIME_154MS;
  agc_lst[4].mincnt = 15740; agc_lst[4].maxcnt = 0;

  tcs = Adafruit_TCS34725(agc_lst[agc_cur].at, agc_lst[agc_cur].ag);
  // Pin the underlying Adafruit driver to the bus we found the
  // sensor on — without the explicit Wire pointer, its no-arg
  // begin() defaults to &Wire (bus 0) and bus-1 reads silently fail.
  TwoWire *theWire = _DUAL_WIRE_FOR(bus);
  isAvailable = tcs.begin(TCS34725_ADDRESS, theWire);
  if (isAvailable) { setGainTime(); }
  return isAvailable;
}

MODULE_PART void tcs34725::setGainTime(void) {
  tcs.setGain(agc_lst[agc_cur].ag);
  tcs.setIntegrationTime(agc_lst[agc_cur].at);
  atime    = int(agc_lst[agc_cur].at);
  atime_ms = ((256 - atime) * 2.4);
  switch (agc_lst[agc_cur].ag) {
    case TCS34725_GAIN_1X:  againx = 1;  break;
    case TCS34725_GAIN_4X:  againx = 4;  break;
    case TCS34725_GAIN_16X: againx = 16; break;
    case TCS34725_GAIN_60X: againx = 60; break;
  }
}

MODULE_PART void tcs34725::getRawData_noDelay(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) {
  *c = tcs.read16(TCS34725_CDATAL);
  *r = tcs.read16(TCS34725_RDATAL);
  *g = tcs.read16(TCS34725_GDATAL);
  *b = tcs.read16(TCS34725_BDATAL);
}

MODULE_PART void tcs34725::getData(void) {
  SETREGS
  tcs.getRawData(&r, &g, &b, &c);

  // Autorange — climb / drop one gain step per call until c falls in
  // the current step's window. The `break` after the second read
  // makes this a single-step adjuster (matches legacy behaviour).
  while (1) {
    if      (agc_lst[agc_cur].maxcnt && c > agc_lst[agc_cur].maxcnt) { agc_cur++; }
    else if (agc_lst[agc_cur].mincnt && c < agc_lst[agc_cur].mincnt) { agc_cur--; }
    else break;
    setGainTime();
    delay((256 - atime) * 2.4 * 2);  // shock absorber
    tcs.getRawData(&r, &g, &b, &c);
    break;
  }

  // DN40 calculations — IR-compensated colour + lux + colour temp.
  ir     = (r + g + b > c) ? (r + g + b - c) / 2 : 0;
  r_comp = r - ir;
  g_comp = g - ir;
  b_comp = b - ir;
  c_comp = c - ir;
  cratio = float(ir) / float(c);

  saturation   = ((256 - atime) > 63) ? 65535 : 1024 * (256 - atime);
  saturation75 = (atime_ms < 150) ? (saturation - saturation / 4) : saturation;
  isSaturated  = (atime_ms < 150 && c > saturation75) ? 1 : 0;
  cpl          = (atime_ms * againx) / (TCS34725_GA * TCS34725_DF);
  maxlux       = 65535 / (cpl * 3);

  lux = (TCS34725_R_Coef * float(r_comp)
       + TCS34725_G_Coef * float(g_comp)
       + TCS34725_B_Coef * float(b_comp)) / cpl;
  ct  = TCS34725_CT_Coef * float(b_comp) / float(r_comp) + TCS34725_CT_Offset;
}

// --------------------------------------------------------------------
// Driver hooks — forward decls live in the descriptor block above.
// --------------------------------------------------------------------
MODULE_PART MOD_RESULT TCS34725_Detect(void) {
  ALLOCMEM

  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(TCS34725_ADDRESS, bus)) { continue; }
    if (rgb_sensor.begin(bus)) {
      tcs_bus     = bus;
      tcs_ready   = 1;
      initialized = true;
      I2C_SetActiveFound(TCS34725_ADDRESS, PSTR("TCS34725"), bus);
      return true;
    }
    I2C_ResetActive(TCS34725_ADDRESS, bus);
  }
  return true;  // alloc succeeded; absence of sensor is non-fatal
}

MODULE_PART void TCS34725_EverySecond(void) {
  SETREGS
  if (tcs_ready) {
    rgb_sensor.getData();
  }
}

#define D_LUX        "Lux"
#define D_COLOR_TEMP "ColorTemp"
#define D_RED        "red"
#define D_GREEN      "green"
#define D_BLUE       "blue"
#define D_AMBIENT    "ambient"

#ifdef USE_WEBSERVER
const char TCS_HTTP[] PROGMEM =
  "{s}TCS34725 " D_LUX        "{m}%d " D_LUX            "{e}"
  "{s}TCS34725 " D_COLOR_TEMP "{m}%d " D_UNIT_KELVIN    "{e}"
  "{s}TCS34725 " D_RED        "{m}%d " "{e}"
  "{s}TCS34725 " D_GREEN      "{m}%d " "{e}"
  "{s}TCS34725 " D_BLUE       "{m}%d " "{e}"
  "{s}TCS34725 " D_AMBIENT    "{m}%d " "{e}";
#endif

const char TCS_JSON[] PROGMEM =
  ",\"TCS34725\":{\"" D_LUX        "\":%d,\""
                      D_COLOR_TEMP "\":%d,"
                      "\"R\":%d,\"G\":%d,\"B\":%d,\"C\":%d}";

MODULE_PART void TCS34725_Show(boolean json) {
  SETREGS
  if (!tcs_ready) { return; }

  if (json) {
    ResponseAppend_P(GSTR(TCS_JSON),
                     (uint32_t)rgb_sensor.lux, (uint32_t)rgb_sensor.ct,
                     (uint32_t)rgb_sensor.r,   (uint32_t)rgb_sensor.g,
                     (uint32_t)rgb_sensor.b,   (uint32_t)rgb_sensor.c);
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(TCS_HTTP),
                     (uint32_t)rgb_sensor.lux, (uint32_t)rgb_sensor.ct,
                     (uint32_t)rgb_sensor.r,   (uint32_t)rgb_sensor.g,
                     (uint32_t)rgb_sensor.b,   (uint32_t)rgb_sensor.c);
#endif
  }
}

MODULE_PART void TCS34725_Deinit(void) {
  SETREGS
  I2C_ResetActive(TCS34725_ADDRESS, tcs_bus);
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = TCS34725_Detect(); break;
    case pFUNC_EVERY_SECOND: TCS34725_EverySecond();     break;
    case pFUNC_JSON_APPEND:  TCS34725_Show(1);           break;
    case pFUNC_WEB_SENSOR:   TCS34725_Show(0);           break;
    case pFUNC_DEINIT:       TCS34725_Deinit();          break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns124(uint32_t function) {
  if (!I2cEnabled(XI2C_55)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    TCS34725_Detect();
  } else if (tcs34725_state && tcs_ready) {
    switch (function) {
      case FUNC_EVERY_SECOND: TCS34725_EverySecond(); break;
      case FUNC_JSON_APPEND:  TCS34725_Show(1);       break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   TCS34725_Show(0);       break;
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
#  undef rgb_sensor
#  undef tcs_ready
#  undef tcs_bus
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif

#endif  // _TCS34725_DUAL_ENABLED
