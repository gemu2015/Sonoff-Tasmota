/*
  xdrv_14_mp3_dual.cpp — DFRobot DFPlayer Mini / DY-SV17F MP3 player
  driver, dual-format (BinPlugin + native firmware).

  Original copyright preserved:
    Copyright (C) 2021 gemu2015, mike2nl, Theo Arends

  Plugin: USE_MP3_PLAYER_DUAL_MOD via build_plugin.py.
  Native: USE_MP3_PLAYER_DUAL via the shim at
          tasmota/tasmota_xdrv_driver/xdrv_14_mp3_dual.ino.

  First non-I2C dual-format driver — exercises the TasmotaSerial
  bridge in dual_format_compat.h (NewTS/beginTS/writeTS/flushTS/
  deleteTS) instead of the I2C wrappers used by the sensor duals
  and PCF8574.

  Supports two protocols:
    DVP_MINI  (0) — DFRobot DFPlayer Mini (10-byte 0x7E ... 0xEF frames)
    DY_SV17F  (1) — DY-SV17F (0xAA prefixed checksummed frames)

  Pin / type config:
   - Plugin: BinPlugin params mp->ms[0]=TXD pin, mp->ms[1]=type (0/1).
   - Native: TXD comes from Pin(GPIO_MP3_DFR562) (Tasmota template),
             falls back to MP3_DEFAULT_TX_PIN if the GPIO is unmapped.
             Player type defaults to MP3_DEFAULT_TYPE; override via
             USE_MP3_PLAYER_TYPE_DY_SV17F at build time.
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_MP3_PLAYER_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

// TasmotaSerial header — needed by dual_format_compat.h's serial
// macros in native mode AND by the plugin's NewTS jumptable target.
#include <TasmotaSerial.h>

#include "dual_format_compat.h"

// Top-level enable gate.
#if BUILD_AS_PLUGIN
#  ifdef USE_MP3_PLAYER_DUAL_MOD
#    define _MP3_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_MP3_PLAYER_DUAL) && defined(MP3_DUAL_NATIVE_INCLUDE)
#    define _MP3_DUAL_ENABLED 1
#  endif
#endif

#ifdef _MP3_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define MP3_DEFAULT_TX_PIN 10
#define MP3PLAYER_REV      (1 << 16 | 6)

#define DVP_MINI 0
#define DY_SV17F 1

#ifndef MP3_DEFAULT_TYPE
#  ifdef USE_MP3_PLAYER_TYPE_DY_SV17F
#    define MP3_DEFAULT_TYPE DY_SV17F
#  else
#    define MP3_DEFAULT_TYPE DVP_MINI
#  endif
#endif

#ifndef MP3_VOLUME
#  define MP3_VOLUME 30
#endif

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE without an `#if` gate.
// In native mode every macro below is empty per dual_format_compat.h,
// so this reduces to plain C++ forward decls. In plugin mode the
// MODULE_PART decls are placed in SECTION_PART between the descriptor
// (SECTION_DESC) and MODULE_END (SECTION_END), as the loader expects.
// Only mod_func_execute is plugin-only (native dispatches via Xdrv14).
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("MP3PLAYER", MODULE_TYPE_DRIVER, MP3PLAYER_REV,
                  "TXD",  MP3_DEFAULT_TX_PIN,
                  "TYPE", 0x01000101,
                  "", 0, "", 0)
MODULE_PART uint16_t MP3_Checksum(uint8_t *array);
MODULE_PART int32_t  MP3PlayerInit(void);
MODULE_PART int32_t  MP3_Init(void);
MODULE_PART void     MP3_SendCmd(uint8_t *scmd, uint8_t len);
MODULE_PART void     MP3_CMD(uint8_t mp3cmd, uint16_t val);
MODULE_PART bool     MP3PlayerCmd(void);
MODULE_PART void     MP3Player_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// Constants table — keep PROGMEM strings file-unique to avoid
// collisions with any future co-enabled dual driver.
// --------------------------------------------------------------------
#define D_CMND_MP3 "MP3"

const char S_JSON_MP3_COMMAND_NVALUE_DUAL[] PROGMEM = "{\"" D_CMND_MP3 "%s\":%d}";
const char S_JSON_MP3_COMMAND_DUAL[]        PROGMEM = "{\"" D_CMND_MP3 "%s\"}";
const char mS_JSON_COMMAND_SVALUE_DUAL[]    PROGMEM = "{\"%s\":\"%s\"}";
// "Mode" is the preferred user-visible name for the protocol switch
// (DVP_MINI vs DY_SV17F). "TYPE" is kept as a backward-compat alias —
// both pipe-positions map to the same handler block below.
const char kMP3_Commands_DUAL[]             PROGMEM =
    "Track|Play|Pause|Stop|Volume|EQ|Device|Reset|DAC|TYPE|Mode";
const char d_mp3_DUAL[]                     PROGMEM = "MP3";
const char mp3_started_DUAL[]               PROGMEM = "MP3 initialized: TX pin %d mode %s";
// Mode names — index by player_type (0=DVP_MINI, 1=DY_SV17F).
const char mp3_mode_names_DUAL[]            PROGMEM = "DFPlayer|DY_SV17F";
const char S_JSON_MP3_MODE_DUAL[]           PROGMEM = "{\"" D_CMND_MP3 "Mode\":\"%s\"}";

enum MP3_Commands {
  CMND_MP3_TRACK,
  CMND_MP3_PLAY,
  CMND_MP3_PAUSE,
  CMND_MP3_STOP,
  CMND_MP3_VOLUME,
  CMND_MP3_EQ,
  CMND_MP3_DEVICE,
  CMND_MP3_RESET,
  CMND_MP3_DAC,
  CMND_MP3_SETTYPE,
  CMND_MP3_MODE
};

#define MP3_CMD_RESET_VALUE 0x00
#define MP3_CMD_TRACK       0x03
#define MP3_CMD_PLAY        0x0d
#define MP3_CMD_PAUSE       0x0e
#define MP3_CMD_STOP        0x16
#define MP3_CMD_VOLUME      0x06
#define MP3_CMD_EQ          0x07
#define MP3_CMD_DEVICE      0x09
#define MP3_CMD_RESET       0x0C
#define MP3_CMD_DAC         0x1A

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#define DUAL_NATIVE_NAME    mp3
#define DUAL_NATIVE_STATE_T mp3_state_t
#include "dual_format_native_state.h"
typedef struct {
  uint8_t player_type;
  uint8_t player_txpin;
  void   *ts;            // TasmotaSerial * — opaque to the plugin
  bool    initialized_flag;
} MODULE_MEMORY;

#define mp3_player_type   mem->player_type
#define mp3_player_txpin  mem->player_txpin
#define mp3_ts            mem->ts

#if !BUILD_AS_PLUGIN

DUAL_NATIVE_STATE_PTR_DECL
#  define XDRV_14           14

#endif  // !BUILD_AS_PLUGIN

// XdrvMailbox access pattern (pointer in plugin, instance in native)
#if BUILD_AS_PLUGIN
#  define _MP3_MB_TOPIC     (XdrvMailbox->topic)
#  define _MP3_MB_DATA_LEN  (XdrvMailbox->data_len)
#  define _MP3_MB_DATA      (XdrvMailbox->data)
#  define _MP3_MB_PAYLOAD   (XdrvMailbox->payload)
#else
#  define _MP3_MB_TOPIC     (XdrvMailbox.topic)
#  define _MP3_MB_DATA_LEN  (XdrvMailbox.data_len)
#  define _MP3_MB_DATA      (XdrvMailbox.data)
#  define _MP3_MB_PAYLOAD   (XdrvMailbox.payload)
#endif

// --------------------------------------------------------------------
// Driver core
// --------------------------------------------------------------------
uint16_t MP3_Checksum(uint8_t *array) {
  uint16_t checksum = 0;
  for (uint32_t i = 0; i < 6; i++) { checksum += array[i]; }
  checksum ^= 0xFFFF;
  return checksum + 1;
}

int32_t MP3PlayerInit(void) {
  ALLOCMEM

#if BUILD_AS_PLUGIN
  mp3_player_type  = mp->ms[1].value & 0x03;
  mp3_player_txpin = mp->ms[0].value & 0xff;
#else
  // Native: prefer the user's Tasmota template (GPIO_MP3_DFR562);
  // fall back to the build-time default if no GPIO is mapped.
#  ifdef GPIO_MP3_DFR562
  int8_t gp = Pin(GPIO_MP3_DFR562);
  mp3_player_txpin = (gp >= 0) ? (uint8_t)gp : (uint8_t)MP3_DEFAULT_TX_PIN;
#  else
  mp3_player_txpin = MP3_DEFAULT_TX_PIN;
#  endif
  mp3_player_type  = MP3_DEFAULT_TYPE;
#endif

  if (!MP3_Init()) {
    initialized = true;
    return 0;
  }
  MP3Player_Deinit();
  return -1;
}

int32_t MP3_Init(void) {
  SETREGS

  mp3_ts = NewTS(-1, mp3_player_txpin);
  if (!mp3_ts) { return -1; }

  if (!beginTS(mp3_ts, ICONST(9600))) { return -1; }

  flushTS(mp3_ts);
  delay(10);
  MP3_CMD(MP3_CMD_RESET, MP3_CMD_RESET_VALUE);
  delay(100);
  MP3_CMD(MP3_CMD_VOLUME, MP3_VOLUME);
  char mode_buf[10];
  GetTextIndexed(mode_buf, sizeof(mode_buf),
                 (uint32_t)(mp3_player_type & 1), GSTR(mp3_mode_names_DUAL));
  AddLog(LOG_LEVEL_INFO, GSTR(mp3_started_DUAL),
         (int)mp3_player_txpin, mode_buf);
  return 0;
}

void MP3_SendCmd(uint8_t *scmd, uint8_t len) {
  SETREGS
  uint16_t sum = 0;
  for (uint32_t cnt = 0; cnt < len; cnt++) { sum += scmd[cnt]; }
  scmd[len] = (uint8_t)sum;
  writeTS(mp3_ts, scmd, len + 1);
}

void MP3_CMD(uint8_t mp3cmd, uint16_t val) {
  SETREGS

  if (mp3_player_type == DVP_MINI) {
    uint8_t cmd[10];
    cmd[0] = 0x7E;
    cmd[1] = 0xFF;
    cmd[2] = 6;
    cmd[3] = mp3cmd;
    cmd[4] = 0;
    cmd[5] = val >> 8;
    cmd[6] = val & 0xFF;
    cmd[7] = 0;
    cmd[8] = 0;
    cmd[9] = 0xEF;
    uint16_t chks = MP3_Checksum(&cmd[1]);
    cmd[7] = chks >> 8;
    cmd[8] = chks & 0xFF;
    writeTS(mp3_ts, cmd, sizeof(cmd));
    delay(1000);
    if (mp3cmd == MP3_CMD_RESET) {
      MP3_CMD(MP3_CMD_VOLUME, MP3_VOLUME);
    }
  } else {
    uint8_t scmd[8];
    uint8_t len = 0;
    scmd[0] = 0xAA;
    switch (mp3cmd) {
      case MP3_CMD_TRACK:
        scmd[1] = 0x07;
        scmd[2] = 0x02;
        scmd[3] = val >> 8;
        scmd[4] = val & 0xFF;
        MP3_SendCmd(scmd, 5);
        // fallthrough — original behavior: track also auto-starts play
      case MP3_CMD_PLAY:
        scmd[1] = 0x02;
        scmd[2] = 0x00;
        scmd[3] = 0xAC;
        len = 4;
        break;
      case MP3_CMD_STOP:
        scmd[1] = 0x10;
        scmd[2] = 0x00;
        scmd[3] = 0xBA;
        len = 4;
        break;
      case MP3_CMD_VOLUME:
        scmd[1] = 0x13;
        scmd[2] = 0x01;
        scmd[3] = val & 0xFF;
        len = 4;
        break;
      default:
        return;
    }
    MP3_SendCmd(scmd, len);
  }
}

bool MP3PlayerCmd(void) {
  SETREGS
  char    command[CMDSZ];
  bool    serviced = true;
  uint8_t disp_len = strlen((char *)GSTR(d_mp3_DUAL));

  if (strncasecmp_P(_MP3_MB_TOPIC, GSTR(d_mp3_DUAL), disp_len) != 0) {
    return false;
  }
  int command_code = GetCommandCode(command, sizeof(command),
                                    _MP3_MB_TOPIC + disp_len,
                                    GSTR(kMP3_Commands_DUAL));

  switch (command_code) {
    case CMND_MP3_TRACK:
    case CMND_MP3_VOLUME:
    case CMND_MP3_EQ:
    case CMND_MP3_DEVICE:
    case CMND_MP3_DAC:
      if (_MP3_MB_DATA_LEN > 0) {
        switch (command_code) {
          case CMND_MP3_TRACK:   MP3_CMD(MP3_CMD_TRACK,  _MP3_MB_PAYLOAD); break;
          case CMND_MP3_VOLUME:  MP3_CMD(MP3_CMD_VOLUME, iscale(_MP3_MB_PAYLOAD, 30, 100)); break;
          case CMND_MP3_EQ:      MP3_CMD(MP3_CMD_EQ,     _MP3_MB_PAYLOAD); break;
          case CMND_MP3_DEVICE:  MP3_CMD(MP3_CMD_DEVICE, _MP3_MB_PAYLOAD); break;
          case CMND_MP3_DAC:     MP3_CMD(MP3_CMD_DAC,    _MP3_MB_PAYLOAD); break;
        }
      }
      Response_P(GSTR(S_JSON_MP3_COMMAND_NVALUE_DUAL), command, _MP3_MB_PAYLOAD);
      break;

    // MP3Mode (preferred) and MP3TYPE (legacy alias) — switch the
    // wire-protocol the driver speaks at runtime. Works in both
    // BinPlugin and native firmware modes; no rebuild needed.
    //   MP3Mode           → query, returns current mode name
    //   MP3Mode 0         → DFPlayer Mini protocol (10-byte 0x7E…0xEF)
    //   MP3Mode 1         → DY-SV17F protocol (0xAA-prefixed)
    //   MP3Mode DFPlayer  → same as 0 (string form)
    //   MP3Mode DY_SV17F  → same as 1 (string form)
    // After a mode change a RESET is sent to the player so its own
    // state matches what the driver will be sending next.
    case CMND_MP3_SETTYPE:
    case CMND_MP3_MODE:
      if (_MP3_MB_DATA_LEN > 0) {
        uint8_t new_type;
        if (strcasecmp_P(_MP3_MB_DATA, PSTR("DFPlayer")) == 0
            || strcasecmp_P(_MP3_MB_DATA, PSTR("DVP_MINI")) == 0
            || strcasecmp_P(_MP3_MB_DATA, PSTR("MINI")) == 0) {
          new_type = DVP_MINI;
        } else if (strcasecmp_P(_MP3_MB_DATA, PSTR("DY_SV17F")) == 0
                   || strcasecmp_P(_MP3_MB_DATA, PSTR("SV17F")) == 0) {
          new_type = DY_SV17F;
        } else {
          new_type = (uint8_t)(_MP3_MB_PAYLOAD & 1);
        }
        if (new_type != mp3_player_type) {
          mp3_player_type = new_type;
          // Re-sync the player to defaults under the new protocol.
          MP3_CMD(MP3_CMD_RESET, MP3_CMD_RESET_VALUE);
          delay(100);
          MP3_CMD(MP3_CMD_VOLUME, MP3_VOLUME);
        }
      }
      {
        char mode_buf[10];
        GetTextIndexed(mode_buf, sizeof(mode_buf),
                       (uint32_t)(mp3_player_type & 1),
                       GSTR(mp3_mode_names_DUAL));
        Response_P(GSTR(S_JSON_MP3_MODE_DUAL), mode_buf);
      }
      break;

    case CMND_MP3_PAUSE:
    case CMND_MP3_STOP:
    case CMND_MP3_RESET:
    play_default:
      switch (command_code) {
        case CMND_MP3_PLAY:  MP3_CMD(MP3_CMD_PLAY,  0); break;
        case CMND_MP3_PAUSE: MP3_CMD(MP3_CMD_PAUSE, 0); break;
        case CMND_MP3_STOP:  MP3_CMD(MP3_CMD_STOP,  0); break;
        case CMND_MP3_RESET: MP3_CMD(MP3_CMD_RESET, 0); break;
      }
      Response_P(GSTR(S_JSON_MP3_COMMAND_DUAL), command);
      break;

    case CMND_MP3_PLAY:
      // DY_SV17F supports playback by filename (MP3Play /path).
      // Other types fall back to the generic CMD_PLAY behavior.
      if (mp3_player_type != DY_SV17F) { goto play_default; }
      if (_MP3_MB_DATA_LEN > 0) {
        uint8_t scmd[64];
        scmd[0] = 0xAA;
        scmd[1] = 0x08;
        scmd[2] = (uint8_t)(_MP3_MB_DATA_LEN + 1);
        scmd[3] = 2;
        char *cp = _MP3_MB_DATA;
        scmd[4] = (uint8_t)*cp;
        for (int i = 1; i < _MP3_MB_DATA_LEN; i++) {
          scmd[i + 4] = (cp[i] == '.') ? '*' : (uint8_t)toupper((unsigned char)cp[i]);
        }
        MP3_SendCmd(scmd, (uint8_t)(_MP3_MB_DATA_LEN + 4));
        Response_P(GSTR(mS_JSON_COMMAND_SVALUE_DUAL), command, _MP3_MB_DATA);
      } else {
        MP3_CMD(MP3_CMD_PLAY, 0);
        Response_P(GSTR(S_JSON_MP3_COMMAND_DUAL), command);
      }
      break;

    default:
      serviced = false;
      break;
  }
  return serviced;
}

void MP3Player_Deinit(void) {
  SETREGS
  if (mp3_ts) {
    deleteTS(mp3_ts);
    mp3_ts = nullptr;
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
    case pFUNC_INIT:    result = MP3PlayerInit(); break;
    case pFUNC_COMMAND: result = MP3PlayerCmd();  break;
    case pFUNC_DEINIT:  MP3Player_Deinit();       break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xdrv14(uint32_t function) {
  bool result = false;
  if (FUNC_PRE_INIT == function || FUNC_INIT == function) {
    if (!mp3_state || !initialized) {
      MP3PlayerInit();
    }
  } else if (mp3_state && initialized) {
    switch (function) {
      case FUNC_COMMAND:
        result = MP3PlayerCmd();
        break;
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
#  undef mp3_player_type
#  undef mp3_player_txpin
#  undef mp3_ts
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef _MP3_MB_TOPIC
#undef _MP3_MB_DATA_LEN
#undef _MP3_MB_DATA
#undef _MP3_MB_PAYLOAD

#endif  // _MP3_DUAL_ENABLED
