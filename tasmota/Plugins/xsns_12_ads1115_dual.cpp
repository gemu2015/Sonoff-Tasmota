/*
  xsns_12_ads1115_dual.cpp — Texas Instruments ADS1115 / ADS1015
  4-channel 16-bit I2C ADC driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2021 Theo Arends

  Plugin: USE_ADS1115_DUAL_MOD via build_plugin.py.
  Native: USE_ADS1115_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_12_ads1115_dual.ino.

  The ADS1115 has four selectable I2C addresses (the ADDR pin can be
  tied to GND, VDD, SDA, or SCL):
    0x48 — ADDR=GND   (default)
    0x49 — ADDR=VDD
    0x4A — ADDR=SDA
    0x4B — ADDR=SCL
  Up to four boards can therefore live on the same bus simultaneously.

  This dual form probes BOTH I2C buses (ESP32) × all four addresses
  on each bus. Each detected board records its own (address, bus)
  pair so the periodic per-channel conversion (Ads1115GetConversion)
  hits the right bus.

  Gain ranges (compile-time default below; use Ads1115StartComparator's
  config word to change at runtime):
    PGA_6_144V  2/3x   ±6.144V  1 LSB = 0.1875 mV  ← default here
    PGA_4_096V    1x   ±4.096V  1 LSB = 0.125  mV
    PGA_2_048V    2x   ±2.048V  1 LSB = 0.0625 mV
    PGA_1_024V    4x   ±1.024V  1 LSB = 0.03125mV
    PGA_0_512V    8x   ±0.512V  1 LSB = 0.015625mV
    PGA_0_256V   16x   ±0.256V  1 LSB = 0.0078125mV
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_ADS1115_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_ADS1115_DUAL_MOD
#    define _ADS1115_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_ADS1115_DUAL) && defined(ADS1115_DUAL_NATIVE_INCLUDE)
#    define _ADS1115_DUAL_ENABLED 1
#  endif
#endif

#ifdef _ADS1115_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define ADS1115_REV (1 << 16 | 5)

#define ADS1115_ADDRESS_ADDR_GND 0x48  // ADDR=GND
#define ADS1115_ADDRESS_ADDR_VDD 0x49  // ADDR=VDD
#define ADS1115_ADDRESS_ADDR_SDA 0x4A  // ADDR=SDA
#define ADS1115_ADDRESS_ADDR_SCL 0x4B  // ADDR=SCL

#define ADS1115_CONVERSIONDELAY 8      // ms

// Pointer register
#define ADS1115_REG_POINTER_CONVERT     (0x00)
#define ADS1115_REG_POINTER_CONFIG      (0x01)

// Config register fields (only the ones the driver actually uses;
// the full set is documented in the legacy plugin).
#define ADS1115_REG_CONFIG_MUX_SINGLE_0 (0x4000)
#define ADS1115_REG_CONFIG_PGA_6_144V   (0x0000)
#define ADS1115_REG_CONFIG_MODE_CONTIN  (0x0000)
#define ADS1115_REG_CONFIG_MODE_SINGLE  (0x0100)
#define ADS1115_REG_CONFIG_DR_6000SPS   (0x00E0)
#define ADS1115_REG_CONFIG_CMODE_TRAD   (0x0000)
#define ADS1115_REG_CONFIG_CPOL_ACTVLOW (0x0000)
#define ADS1115_REG_CONFIG_CLAT_NONLAT  (0x0000)
#define ADS1115_REG_CONFIG_CQUE_NONE    (0x0003)

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// Native: every macro is empty, so this reduces to plain C++ forward
// decls. Plugin: MODULE_PART decls are placed in SECTION_PART between
// the descriptor and MODULE_END (canonical legacy plugin layout).
// --------------------------------------------------------------------
PUSH_OPTIONS
#ifdef USE_SOFTWIRE
#  define DEFAULT_SDA_PIN 12
#  define DEFAULT_SCL_PIN 14
MODULE_DESCRIPTOR("ADS1115S", MODULE_TYPE_SENSOR, ADS1115_REV,
                  "SDA", DEFAULT_SDA_PIN, "SCL", DEFAULT_SCL_PIN,
                  "", 0, "", 0)
#else
MODULE_DESCRIPTOR("ADS1115", MODULE_TYPE_SENSOR, ADS1115_REV,
                  "", 0, "", 0, "", 0, "", 0)
#endif
MODULE_PART int32_t Init_ADS1115(void);
MODULE_PART void    Ads1115Label(char *label, uint32_t maxsize, uint8_t address);
MODULE_PART void    AdsEvery250ms(void);
MODULE_PART void    ADS1115_Show(bool json);
MODULE_PART int16_t Ads1115GetConversion(uint8_t channel, uint8_t bus_idx);
MODULE_PART void    Ads1115StartComparator(uint8_t channel, uint16_t mode);
MODULE_PART void    ADS1115_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// Per-driver PROGMEM strings — file-unique `_ADS` suffix to avoid
// any collision with other dual drivers in the merged tasmota.ino.cpp.
// --------------------------------------------------------------------
const char ADS_moddev[]      PROGMEM = "ADS1115";
const char ADS_moddev_lbl[]  PROGMEM = "ADS1115%c%02X";
const char ADS_json_open[]   PROGMEM = "{\"%s\":{";
const char ADS_json_open2[]  PROGMEM = ",\"%s\":{";
const char ADS_json_changed[] PROGMEM = "%s\"A%ddiv10\":%d";
const char ADS_json_full[]   PROGMEM = "\"A1\":%d,\"A2\":%d,\"A3\":%d,\"A4\":%d}";
const char ADS_sep_comma[]   PROGMEM = ",";
const char ADS_sep_empty[]   PROGMEM = "";
const char ADS_html_row[]    PROGMEM = "{s}%s Analog %d{m}%d{e}";

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
#define ADS1115_MAX_BOARDS 4

typedef struct {
  uint8_t count;                                  // number of boards detected
  uint8_t address;                                // currently-selected address
  uint8_t addresses[ADS1115_MAX_BOARDS];          // canonical address-pin layout
  uint8_t bus[ADS1115_MAX_BOARDS];                // I2C bus per detected board
  uint8_t found[ADS1115_MAX_BOARDS];              // 1 = present
  int16_t last_values[ADS1115_MAX_BOARDS][4];     // for change-tracking in EVERY_250ms
} ADS1115;

// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#define DUAL_NATIVE_NAME    ads1115
#define DUAL_NATIVE_STATE_T ads1115_state_t
#include "dual_format_native_state.h"
typedef struct {
  TWIp   *xWire;
  ADS1115 Ads1115;
  bool    ready;
  bool    initialized_flag;
} MODULE_MEMORY;

#define Ads1115     mem->Ads1115
#define ready       mem->ready

#if BUILD_AS_PLUGIN

#  ifdef USE_SOFTWIRE
#    include "Softwire/Softwire_cpp.h"
#  endif

#else  // native

DUAL_NATIVE_STATE_PTR_DECL
#  define XSNS_12     12
#  define XI2C_13     13

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------

void Ads1115StartComparator(uint8_t channel, uint16_t mode) {
  SETREGS
  uint16_t config = mode | ADS1115_REG_CONFIG_CQUE_NONE
                  | ADS1115_REG_CONFIG_CLAT_NONLAT
                  | ADS1115_REG_CONFIG_PGA_6_144V
                  | ADS1115_REG_CONFIG_CPOL_ACTVLOW
                  | ADS1115_REG_CONFIG_CMODE_TRAD
                  | ADS1115_REG_CONFIG_DR_6000SPS;
  config |= (ADS1115_REG_CONFIG_MUX_SINGLE_0 + (0x1000 * channel));
  // bus arg gets overridden by the compat header to track _dual_wire.
  I2C_Write16(Ads1115.address, ADS1115_REG_POINTER_CONFIG, config, 0);
}

int16_t Ads1115GetConversion(uint8_t channel, uint8_t bus_idx) {
  SETREGS
  // Caller has already pinned Ads1115.address to this board AND
  // called I2C_SETWIRE(bus_idx) — but be defensive about the wire
  // selection in case a downstream call resets it.
  I2C_SETWIRE(bus_idx);

  Ads1115StartComparator(channel, ADS1115_REG_CONFIG_MODE_SINGLE);
  delay(ADS1115_CONVERSIONDELAY);
  I2C_Read16(Ads1115.address, ADS1115_REG_POINTER_CONVERT, 0);

  Ads1115StartComparator(channel, ADS1115_REG_CONFIG_MODE_CONTIN);
  delay(ADS1115_CONVERSIONDELAY);
  uint16_t res = I2C_Read16(Ads1115.address, ADS1115_REG_POINTER_CONVERT, 0);
  return (int16_t)res;
}

int32_t Init_ADS1115(void) {
  ALLOCMEM

  Ads1115.addresses[0] = ADS1115_ADDRESS_ADDR_GND;
  Ads1115.addresses[1] = ADS1115_ADDRESS_ADDR_VDD;
  Ads1115.addresses[2] = ADS1115_ADDRESS_ADDR_SDA;
  Ads1115.addresses[3] = ADS1115_ADDRESS_ADDR_SCL;

  // Probe every (bus, address) combination. A board's "found" slot
  // is keyed by address index — at most one board per address even
  // if cross-bus, so the 4-slot found[] array is enough.
  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    for (uint32_t i = 0; i < fldsiz(ADS1115, addresses); i++) {
      if (Ads1115.found[i]) { continue; }
      uint8_t probe_addr = Ads1115.addresses[i];
      if (!I2C_SetDevice(probe_addr, bus)) { continue; }
      uint16_t buffer;
      // Validate the device by reading both CONVERT and CONFIG —
      // any device acking 0x48..0x4B isn't necessarily an ADS1115;
      // these two reads both succeeding makes a strong hint.
      Ads1115.address = probe_addr;
      if (I2C_ValidRead16(&buffer, probe_addr, ADS1115_REG_POINTER_CONVERT, 0)
          && I2C_ValidRead16(&buffer, probe_addr, ADS1115_REG_POINTER_CONFIG, 0)) {
        Ads1115StartComparator(i, ADS1115_REG_CONFIG_MODE_CONTIN);
        I2C_SetActiveFound(probe_addr, GSTR(ADS_moddev), bus);
        Ads1115.bus[i]   = bus;
        Ads1115.found[i] = 1;
        Ads1115.count++;
      } else {
        I2C_ResetActive(probe_addr, bus);
      }
    }
  }

  if (Ads1115.count) {
    initialized = true;
    ready = true;
  } else {
    ADS1115_Deinit();
  }
  return Ads1115.count;
}

void Ads1115Label(char *label, uint32_t maxsize, uint8_t address) {
  SETREGS
  if (1 == Ads1115.count) {
    snprintf_P(label, maxsize, GSTR(ADS_moddev));
  } else {
    snprintf_P(label, maxsize, GSTR(ADS_moddev_lbl), IndexSeparator(), address);
  }
}

#if defined(USE_RULES) || defined(USE_SCRIPT)
// Detect ≥1% changes on any analog input and emit a JSON event so
// rules/scripts can react. Only fires for boards present in `found[]`.
void AdsEvery250ms(void) {
  SETREGS
  for (uint32_t t = 0; t < fldsiz(ADS1115, addresses); t++) {
    if (!Ads1115.found[t]) { continue; }

    uint8_t old_address = Ads1115.address;
    Ads1115.address     = Ads1115.addresses[t];
    I2C_SETWIRE(Ads1115.bus[t]);

    uint32_t changed = 0;
    int16_t  value;
    for (uint32_t i = 0; i < 4; i++) {
      value = Ads1115GetConversion(i, Ads1115.bus[t]);
      // ~1% threshold against last stored value (16-bit full scale,
      // 327 ≈ 1% of 32767).
      if (value >= Ads1115.last_values[t][i] + 327
          || value <= Ads1115.last_values[t][i] - 327) {
        Ads1115.last_values[t][i] = value;
        bitSet(changed, i);
      }
    }
    Ads1115.address = old_address;

    if (changed) {
      char label[15];
      Ads1115Label(label, sizeof(label), Ads1115.addresses[t]);
      Response_P(GSTR(ADS_json_open), label);

      bool first = true;
      for (uint32_t i = 0; i < 4; i++) {
        if (bitRead(changed, i)) {
#ifdef ESP8266
          ResponseAppend_P(GSTR(ADS_json_changed),
                           first ? GSTR(ADS_sep_empty) : GSTR(ADS_sep_comma),
                           i, Ads1115.last_values[t][i]);
#endif
#ifdef ESP32
          // ESP32 LTO occasionally messes up nested PSTR-from-PROGMEM
          // arg passing; copyStr (jt[100]) on plugin / strdup-ish on
          // native side keeps it stable.
#  if BUILD_AS_PLUGIN
          char *s_empty = copyStr(GSTR(ADS_sep_empty));
          char *s_comma = copyStr(GSTR(ADS_sep_comma));
#  else
          char *s_empty = strdup_P(ADS_sep_empty);
          char *s_comma = strdup_P(ADS_sep_comma);
#  endif
          ResponseAppend_P(GSTR(ADS_json_changed),
                           first ? s_empty : s_comma,
                           i, Ads1115.last_values[t][i]);
          if (s_empty) free(s_empty);
          if (s_comma) free(s_comma);
#endif
          first = false;
        }
      }
      ResponseJsonEndEnd();
      XdrvRulesProcess(0);
    }
  }
}
#endif  // USE_RULES || USE_SCRIPT

void ADS1115_Show(bool json) {
  SETREGS
  int16_t values[4];

  for (uint32_t t = 0; t < fldsiz(ADS1115, addresses); t++) {
    if (!Ads1115.found[t]) { continue; }

    uint8_t old_address = Ads1115.address;
    Ads1115.address     = Ads1115.addresses[t];
    I2C_SETWIRE(Ads1115.bus[t]);

    for (uint32_t i = 0; i < 4; i++) {
      values[i] = Ads1115GetConversion(i, Ads1115.bus[t]);
    }
    Ads1115.address = old_address;

    char label[15];
    Ads1115Label(label, sizeof(label), Ads1115.addresses[t]);

    if (json) {
      ResponseAppend_P(GSTR(ADS_json_open2), label);
      ResponseAppend_P(GSTR(ADS_json_full),
                       values[0], values[1], values[2], values[3]);
      ResponseJsonEnd();
    } else {
      for (uint32_t i = 0; i < 4; i++) {
        WSContentSend_PD(GSTR(ADS_html_row), label, i + 1, values[i]);
      }
    }
  }
}

void ADS1115_Deinit(void) {
  SETREGS
  for (uint32_t t = 0; t < fldsiz(ADS1115, addresses); t++) {
    if (Ads1115.found[t]) {
      I2C_ResetActive(Ads1115.addresses[t], Ads1115.bus[t]);
    }
  }
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher — single divergence point between modes
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:              result = Init_ADS1115();   break;
    case pFUNC_JSON_APPEND:       ADS1115_Show(1);           break;
    case pFUNC_WEB_SENSOR:        ADS1115_Show(0);           break;
#  if defined(USE_RULES) || defined(USE_SCRIPT)
    case pFUNC_EVERY_250_MSECOND: AdsEvery250ms();           break;
#  endif
    case pFUNC_DEINIT:            ADS1115_Deinit();          break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns12(uint32_t function) {
  if (!I2cEnabled(XI2C_13)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    Init_ADS1115();
  } else if (ads1115_state && ready) {
    switch (function) {
#  if defined(USE_RULES) || defined(USE_SCRIPT)
      case FUNC_EVERY_250_MSECOND: AdsEvery250ms(); break;
#  endif
      case FUNC_JSON_APPEND:       ADS1115_Show(1); break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:        ADS1115_Show(0); break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef state-accessor and helper macros so they don't
// leak into other dual drivers in the merged tasmota.ino.cpp TU.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef Ads1115
#  undef ready
#endif

#endif  // _ADS1115_DUAL_ENABLED
