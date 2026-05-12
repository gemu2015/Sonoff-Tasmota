/*
  xdrv_28_pcf8574_dual.cpp — PCF8574 / PCF8574A I/O expander
  driver, dual-format (BinPlugin + native firmware).

  Original copyright preserved:
    Copyright (C) 2019  Stefan Bode

  Plugin: USE_PCF8574_DUAL_MOD via build_plugin.py.
  Native: USE_PCF8574_DUAL via the shim at
          tasmota/tasmota_xdrv_driver/xdrv_28_pcf8574_dual.ino.

  Differences vs the existing legacy xdrv_28_pcf8574.cpp plugin:
   - MODULE_TYPE_DRIVER (not _SENSOR) — first dual-format example
     of a driver/Xdrv plugin (relay/output controller).
   - Probes BOTH I2C buses (ESP32) thanks to the dual-format compat
     header's bus-aware Wire pointer; the legacy form was Wire-only.
   - All 14 valid PCF8574/A addresses (0x20..0x26 + 0x39..0x3F) are
     scanned on each bus; per-device address+bus is stored so the
     periodic SET_POWER write hits the right bus.

  PCF8574  = 0x20..0x27 (0x27 reserved)
  PCF8574A = 0x38..0x3F (0x38 reserved)
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_PCF8574_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  include "../Tasmota/include/i18n.h"
#endif

// Top-level gate. See dual_format_compat.h for the rationale —
// the plugin compile activates via USE_PCF8574_DUAL_MOD; the
// native firmware activates via USE_PCF8574_DUAL + the .ino
// shim's PCF8574_DUAL_NATIVE_INCLUDE flag.
#if BUILD_AS_PLUGIN
#  ifdef USE_PCF8574_DUAL_MOD
#    define _PCF8574_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_PCF8574_DUAL) && defined(PCF8574_DUAL_NATIVE_INCLUDE)
#    define _PCF8574_DUAL_ENABLED 1
#  endif
#endif

#ifdef _PCF8574_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define PCF8574_ADDR1 0x20  // PCF8574  base
#define PCF8574_ADDR2 0x38  // PCF8574A base

#ifndef MAX_PCF8574
#  define MAX_PCF8574 4     // matches Tasmota's tasmota.h default
#endif

// --------------------------------------------------------------------
// Per-device record (address + bus). Up to MAX_PCF8574 boards in any
// combination across the two I2C buses.
// --------------------------------------------------------------------
typedef struct {
  uint8_t address;
  uint8_t bus;
  uint8_t pin_mask;
} PCF_DEV;

typedef struct {
  int      error;
  uint8_t  pin[64];                 // GPIO-relay-index → device-pin mapping
  PCF_DEV  dev[MAX_PCF8574];
  uint8_t  max_connected_ports;     // number of relay outputs claimed
  uint8_t  max_devices;             // number of PCF8574 boards detected
  char     stype[9];                // "PCF8574" or "PCF8574A" (last seen)
  bool     type;                    // any device found
} PCF8574;

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// In native mode every macro below is empty per dual_format_compat.h
// so this reduces to plain C++ forward decls. In plugin mode the
// MODULE_PART decls are placed in SECTION_PART between the descriptor
// (SECTION_DESC) and MODULE_END (SECTION_END), as the loader expects.
// Only mod_func_execute is plugin-only (native dispatches via Xdrv28).
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("PCF8574", MODULE_TYPE_DRIVER, 1 << 16 | 5,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART void    Pcf8574SwitchRelay(void);
MODULE_PART int32_t Pcf8574Init(void);
MODULE_PART void    HandlePcf8574(void);
MODULE_PART void    Pcf8574SaveSettings(void);
MODULE_PART void    Pcf8574_AddButton(void);
MODULE_PART void    PCF8574_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t mod_func_execute(uint32_t function);
#endif
MODULE_END

// --------------------------------------------------------------------
// State storage — heap in both modes (lazy alloc; zero RAM when
// no PCF8574 is present on the bus)
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#define DUAL_NATIVE_NAME    pcf8574
#define DUAL_NATIVE_STATE_T pcf8574_state_t
#include "dual_format_native_state.h"
typedef struct {
  TWIp   *xWire;
  PCF8574 Pcf8574;
  bool    handler_up;
  bool    initialized_flag;
} MODULE_MEMORY;

#define Pcf8574    mem->Pcf8574
#define handler_up mem->handler_up

#if !BUILD_AS_PLUGIN

DUAL_NATIVE_STATE_PTR_DECL
#  define XDRV_28     28
#  define XI2C_02     2

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Driver core — shared body. The only branches are TasmotaGlobal
// pointer-vs-instance and XdrvMailbox pointer-vs-instance, both
// localised below with minimal #if BUILD_AS_PLUGIN gates.
// --------------------------------------------------------------------

// Helper to read XdrvMailbox.index in a mode-agnostic way.
#if BUILD_AS_PLUGIN
#  define _PCF_MAILBOX_INDEX     (XdrvMailbox->index)
#  define _PCF_TG_DEVICES        (TasmotaGlobal->devices_present)
#  define _PCF_TG_REL_INVERTED   (TasmotaGlobal->rel_inverted)
#else
#  define _PCF_MAILBOX_INDEX     (XdrvMailbox.index)
#  define _PCF_TG_DEVICES        (TasmotaGlobal.devices_present)
#  define _PCF_TG_REL_INVERTED   (TasmotaGlobal.rel_inverted)
#endif

void Pcf8574SwitchRelay(void) {
  SETREGS
  STGLOB
  for (uint32_t i = 0; i < _PCF_TG_DEVICES; i++) {
    uint8_t relay_state = bitRead(_PCF_MAILBOX_INDEX, i);
    if (Pcf8574.max_devices > 0 && Pcf8574.pin[i] < 99) {
      uint8_t board     = Pcf8574.pin[i] >> 3;
      uint8_t old_mask  = Pcf8574.dev[board].pin_mask;
      uint8_t inverted  = bitRead(_PCF_TG_REL_INVERTED, i);
      uint8_t _val      = inverted ? !relay_state : relay_state;

      if (_val) {
        Pcf8574.dev[board].pin_mask |=  (1 << (Pcf8574.pin[i] & 0x7));
      } else {
        Pcf8574.dev[board].pin_mask &= ~(1 << (Pcf8574.pin[i] & 0x7));
      }
      if (old_mask != Pcf8574.dev[board].pin_mask) {
        I2C_SETWIRE(Pcf8574.dev[board].bus);
        I2C_beginTransmission(Pcf8574.dev[board].address);
        I2C_write(Pcf8574.dev[board].pin_mask);
        Pcf8574.error = I2C_endTransmission(true);
      }
    }
  }
}

int32_t Pcf8574Init(void) {
  ALLOCMEM
  STGLOB

  // Probe both I2C buses × all 14 valid PCF8574/A addresses.
  // Stop when MAX_PCF8574 boards have been claimed.
  for (uint32_t bus = 0; bus < MAX_I2C_Busses && Pcf8574.max_devices < MAX_PCF8574; bus++) {
    I2C_SETWIRE(bus);
    uint8_t addr = PCF8574_ADDR1;
    while (Pcf8574.max_devices < MAX_PCF8574 && addr < PCF8574_ADDR2 + 8) {
      if (I2C_SetDevice(addr, bus)) {
        Pcf8574.dev[Pcf8574.max_devices].address  = addr;
        Pcf8574.dev[Pcf8574.max_devices].bus      = bus;
        Pcf8574.dev[Pcf8574.max_devices].pin_mask = 0;
        Pcf8574.type = true;
        strcpy_P(Pcf8574.stype, (addr >= PCF8574_ADDR2) ? PSTR("PCF8574A")
                                                        : PSTR("PCF8574"));
        I2C_SetActiveFound(addr, Pcf8574.stype, bus);
        Pcf8574.max_devices++;
      }
      addr++;
      if ((PCF8574_ADDR1 + 7) == addr) {
        addr = PCF8574_ADDR2 + 1;     // skip 0x27 → 0x39 (avoid reserved)
      }
    }
  }

  if (Pcf8574.type) {
    for (uint32_t i = 0; i < sizeof(Pcf8574.pin); i++) { Pcf8574.pin[i] = 99; }

    // Reset to avoid duplicate ports on duplicate init.
    _PCF_TG_DEVICES = _PCF_TG_DEVICES - Pcf8574.max_connected_ports;
    Pcf8574.max_connected_ports = 0;

    for (uint32_t idx = 0; idx < Pcf8574.max_devices; idx++) {
      AddLog(LOG_LEVEL_DEBUG, PSTR("PCF: Device %d @ bus %d addr 0x%02X config 0x%02X"),
             idx + 1,
             (int)Pcf8574.dev[idx].bus,
             Pcf8574.dev[idx].address,
             Settings->pcf8574_config[idx]);

      for (uint32_t i = 0; i < 8; i++) {
        uint8_t enabled = (Settings->pcf8574_config[idx] >> i) & 1;
        if (enabled) {
          Pcf8574.pin[_PCF_TG_DEVICES] = i + 8 * idx;
          // SetOption81 — invert all PCF8574 ports
          bitWrite(_PCF_TG_REL_INVERTED, _PCF_TG_DEVICES,
                   Settings->flag3.pcf8574_ports_inverted);
          _PCF_TG_DEVICES++;
          Pcf8574.max_connected_ports++;
        }
      }
    }
    AddLog(LOG_LEVEL_INFO, PSTR("PCF: Total devices %d, output ports %d"),
           Pcf8574.max_devices, Pcf8574.max_connected_ports);
  }

  if (!Pcf8574.type) {
    PCF8574_Deinit();
    return false;
  }

  initialized = true;
  return true;
}

// --------------------------------------------------------------------
// Web UI — config form
// --------------------------------------------------------------------
#ifdef USE_WEBSERVER

#define WEB_HANDLE_PCF8574 "pcf"
#define BUTTON_CONFIGURATION 3

const char HTTP_TABLE100_PCF[] PROGMEM = "<table style='width:100%%'>";

const char HTTP_FORM_END_PCF[] PROGMEM =
    "<br>"
    "<button name='save' type='submit' class='button bgrn'>" D_SAVE
    "</button>"
    "</form></fieldset>";

const char HTTP_BTN_MENU_PCF8574[] PROGMEM =
    "<p><form action='" WEB_HANDLE_PCF8574 "' method='get'>"
    "<button>" D_CONFIGURE_PCF8574 "</button></form></p>";

const char HTTP_FORM_I2C_PCF8574_1[] PROGMEM =
    "<fieldset><legend><b>&nbsp;" D_PCF8574_PARAMETERS "&nbsp;</b></legend>"
    "<form method='get' action='" WEB_HANDLE_PCF8574 "'>"
    "<p><input id='b1' name='b1' type='checkbox'%s><b>" D_INVERT_PORTS
    "</b></p><hr/>";

const char HTTP_FORM_I2C_PCF8574_2[] PROGMEM =
    "<tr><td><b>" D_DEVICE " %d " D_PORT " %d</b></td>"
    "<td style='width:100px'><select id='i2cs%d' name='i2cs%d'>"
    "<option%s value='0'>" D_DEVICE_INPUT  "</option>"
    "<option%s value='1'>" D_DEVICE_OUTPUT "</option>"
    "</select></td></tr>";

void HandlePcf8574(void) {
  SETREGS

  if (!HttpCheckPriviledgedAccess()) { return; }

  AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_HTTP D_CONFIGURE_PCF8574));

  if (WebServer_hasArg(PSTR("save"))) {
    Pcf8574SaveSettings();
    WebRestart(1);
    return;
  }

  WSContentStart_P(PSTR(D_CONFIGURE_PCF8574));
  WSContentSendStyle();

  char s1[12]; char s2[2];
  strcpy_P(s1, PSTR(" checked"));
  s2[0] = 0;

  WSContentSend_P(GSTR(HTTP_FORM_I2C_PCF8574_1),
                  Settings->flag3.pcf8574_ports_inverted ? s1 : s2);
  WSContentSend_P(GSTR(HTTP_TABLE100_PCF));

  strcpy_P(s1, PSTR(" selected"));
  s2[0] = ' '; s2[1] = 0;

  for (uint32_t idx = 0; idx < Pcf8574.max_devices; idx++) {
    for (uint32_t idx2 = 0; idx2 < 8; idx2++) {
      uint8_t  helper = 1 << idx2;
      uint32_t idn    = idx2 + 8 * idx;
      WSContentSend_P(GSTR(HTTP_FORM_I2C_PCF8574_2), idx + 1, idx2, idn, idn,
                      ((helper & Settings->pcf8574_config[idx]) >> idx2 == 0) ? s1 : s2,
                      ((helper & Settings->pcf8574_config[idx]) >> idx2 == 1) ? s1 : s2);
    }
  }

  WSContentSend_P(PSTR("</table>"));
  WSContentSend_P(GSTR(HTTP_FORM_END_PCF));
  WSContentSpaceButton(BUTTON_CONFIGURATION, true);
  WSContentStop();
}

void Pcf8574SaveSettings(void) {
  SETREGS
  STGLOB

  char stemp[7];
  char tmp[100];

  Settings->flag3.pcf8574_ports_inverted = WebServer_hasArg(PSTR("b1"));

  for (uint8_t idx = 0; idx < Pcf8574.max_devices; idx++) {
    uint8_t count = 0;
    uint8_t n = Settings->pcf8574_config[idx];
    while (n != 0) { n &= (n - 1); count++; }
    if (count <= _PCF_TG_DEVICES) {
      _PCF_TG_DEVICES -= count;
    }

    for (uint8_t i = 0; i < 8; i++) {
      snprintf_P(stemp, sizeof(stemp), PSTR("i2cs%d"), i + 8 * idx);
      WebGetArg(stemp, tmp, sizeof(tmp));
      uint8_t _value = (!strlen(tmp)) ? 0 : atoi(tmp);
      if (_value) {
        Settings->pcf8574_config[idx] |= (1 << i);
        _PCF_TG_DEVICES++;
        Pcf8574.max_connected_ports++;
      } else {
        Settings->pcf8574_config[idx] &= ~(1 << i);
      }
    }
  }
}
#endif  // USE_WEBSERVER

void Pcf8574_AddButton(void) {
  SETREGS
#ifdef USE_WEBSERVER
  WSContentSend_P(GSTR(HTTP_BTN_MENU_PCF8574));
  if (!handler_up) {
    WebServer_on(PSTR("/pcf"), HandlePcf8574);
    handler_up = true;
  }
#endif
}

void PCF8574_Deinit(void) {
  SETREGS
  // Release every claimed (addr, bus) pair so a re-init can re-scan.
  for (uint32_t i = 0; i < Pcf8574.max_devices; i++) {
    I2C_ResetActive(Pcf8574.dev[i].address, Pcf8574.dev[i].bus);
  }
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher — single divergence point between modes
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t function) {
  bool result = false;
  switch (function) {
    case pFUNC_INIT:           result = Pcf8574Init();   break;
    case pFUNC_SET_POWER:      Pcf8574SwitchRelay();     break;
    case pFUNC_WEB_ADD_BUTTON: Pcf8574_AddButton();      break;
    case pFUNC_DEINIT:         PCF8574_Deinit();         break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xdrv28(uint32_t function) {
  if (!I2cEnabled(XI2C_02)) { return false; }
  bool result = false;
  if (FUNC_PRE_INIT == function || FUNC_INIT == function) {
    if (!pcf8574_state || !initialized) {
      Pcf8574Init();
    }
  } else if (pcf8574_state && initialized) {
    switch (function) {
      case FUNC_SET_POWER:
        Pcf8574SwitchRelay();
        break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_ADD_BUTTON:
        Pcf8574_AddButton();
        break;
      case FUNC_WEB_ADD_HANDLER:
        WebServer_on(PSTR("/" WEB_HANDLE_PCF8574), HandlePcf8574);
        break;
#  endif
      case FUNC_ACTIVE:
        result = true;
        break;
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup — undef all state-accessor and helper macros so they
// don't leak into other dual drivers when this .cpp is included
// from the .ino shim into the merged tasmota.ino.cpp TU.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef Pcf8574
#  undef handler_up
#endif
#undef _PCF_MAILBOX_INDEX
#undef _PCF_TG_DEVICES
#undef _PCF_TG_REL_INVERTED

#endif  // _PCF8574_DUAL_ENABLED
