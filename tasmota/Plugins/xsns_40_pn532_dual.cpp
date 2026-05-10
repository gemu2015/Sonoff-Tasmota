/*
  xsns_40_pn532_dual.cpp — NXP PN532 13.56 MHz NFC reader driver,
  dual-format (BinPlugin + native firmware).

  Original copyright preserved:
    Copyright (C) 2021 Andre Thomas, Theo Arends, md5sum-as

  Plugin: USE_PN532_DUAL_MOD via build_plugin.py.
  Native: USE_PN532_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_40_pn532_dual.ino.

  Two transport modes — PN532 supports both:
    mode = 1 : I2C, fixed address 0x24 (HSU pulled low → I2C mode).
    mode = 0 : HSU (UART, 115200 baud), uses RXD/TXD pins.

  Plugin mode reads RXD/TXD/MODE from BinPlugin params (mp->ms[0..2]).
  Native mode picks them up from the Tasmota template — GPIO_PN532_RXD
  / GPIO_PN532_TXD if mapped — otherwise falls back to the build-time
  defaults below; native mode is selected via USE_PN532_I2C (default
  serial otherwise).

  Tag operations supported (USE_PN532_DATA_FUNCTION enables the
  console commands):
    pn532Erase   — zero block 1 of next-scanned card / NTAG pages 4-7
    pn532Write <text>   — write up to 16 bytes to next-scanned card
    pn532Auth    — placeholder for password setup (legacy stub)
    pn532Set_PWD — protect NTAG with the configured password
    pn532Unset_PWD — unprotect NTAG
    pn532Cancel  — abort pending erase/write/password change
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_PN532_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include <TasmotaSerial.h>
#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_PN532_DUAL_MOD
#    define _PN532_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_PN532_DUAL) && defined(PN532_DUAL_NATIVE_INCLUDE)
#    define _PN532_DUAL_ENABLED 1
#  endif
#endif

#ifdef _PN532_DUAL_ENABLED

// Enable the data/erase/write/auth command set unless the user
// explicitly compiles it out.
#ifndef USE_PN532_DATA_FUNCTION
#  define USE_PN532_DATA_FUNCTION
#endif

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define PN532_I2_ADDR              0x24
#define PN532_REV                  (1 << 16 | 5)
#define PN532_DEFAULT_RXD          3      // UART0 RX
#define PN532_DEFAULT_TXD          1      // UART0 TX

// Build-time default for native mode: 0 = serial, 1 = I2C.
#ifndef PN532_DEFAULT_MODE
#  ifdef USE_PN532_I2C
#    define PN532_DEFAULT_MODE     1
#  else
#    define PN532_DEFAULT_MODE     0
#  endif
#endif

#define PN532_INVALID_ACK          -1
#define PN532_TIMEOUT              -2
#define PN532_INVALID_FRAME        -3
#define PN532_NO_SPACE             -4

#define PN532_PREAMBLE             0x00
#define PN532_STARTCODE1           0x00
#define PN532_STARTCODE2           0xFF
#define PN532_POSTAMBLE            0x00
#define PN532_HOSTTOPN532          0xD4
#define PN532_PN532TOHOST          0xD5
#define PN532_ACK_WAIT_TIME        0x0A

#define PN532_COMMAND_GETFIRMWAREVERSION   0x02
#define PN532_COMMAND_SAMCONFIGURATION     0x14
#define PN532_COMMAND_RFCONFIGURATION      0x32
#define PN532_COMMAND_INDATAEXCHANGE       0x40
#define PN532_COMMAND_INCOMMUNICATETHRU    0x42
#define PN532_COMMAND_INLISTPASSIVETARGET  0x4A
#define PN532_COMMAND_INRELEASE            0x52
#define PN532_COMMAND_INSELECT             0x54
#define PN532_MIFARE_ISO14443A             0x00
#define MIFARE_CMD_READ                    0x30
#define MIFARE_CMD_AUTH_A                  0x60
#define MIFARE_CMD_AUTH_B                  0x61
#define MIFARE_CMD_WRITE                   0xA0
#define NTAG21X_CMD_GET_VERSION            0x60
#define NTAG2XX_CMD_READ                   0x30
#define NTAG21X_CMD_FAST_READ              0x3A
#define NTAG21X_CMD_PWD_AUTH               0x1B
#define NTAG2XX_CMD_WRITE                  0xA2

#pragma GCC optimize("-Og")

// --------------------------------------------------------------------
// Plugin descriptor block
// --------------------------------------------------------------------
PUSH_OPTIONS
#ifdef USE_PN532_DATA_FUNCTION
MODULE_DESCRIPTOR("PN532_D", MODULE_TYPE_SENSOR, PN532_REV,
                  "RXD", PN532_DEFAULT_RXD,
                  "TXD", PN532_DEFAULT_TXD,
                  "MODE", 0x01000101, "", 0)
#else
MODULE_DESCRIPTOR("PN532",   MODULE_TYPE_SENSOR, PN532_REV,
                  "RXD", PN532_DEFAULT_RXD,
                  "TXD", PN532_DEFAULT_TXD,
                  "MODE", 0x01000101, "", 0)
#endif
MODULE_PART bool     PN532_Init(void);
MODULE_PART int8_t   PN532_receive(uint8_t *buf, int len, uint16_t timeout);
MODULE_PART int8_t   PN532_readAckFrame(void);
MODULE_PART int8_t   PN532_writeCommand(const uint8_t *header, uint8_t hlen,
                                         const uint8_t *body, uint8_t blen);
MODULE_PART int16_t  PN532_readResponse(uint8_t buf[], uint8_t len, uint16_t timeout);
MODULE_PART uint32_t PN532_getFirmwareVersion(void);
MODULE_PART int16_t  PN532_getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout);
MODULE_PART void     PN532_wakeup(void);
MODULE_PART bool     PN532_setPassiveActivationRetries(uint8_t maxRetries);
MODULE_PART bool     PN532_SAMConfig(void);
MODULE_PART void     PN532_ScanForTag(void);
MODULE_PART bool     PN532_readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid,
                                                uint8_t *uidLength, uint16_t timeout);
MODULE_PART void     PN532_Show(void);
MODULE_PART void     PN532_Deinit(void);
#ifdef USE_PN532_DATA_FUNCTION
MODULE_PART void     PN532_inRelease(void);
MODULE_PART uint8_t  PN532_mifareclassic_AuthenticateBlock(uint8_t *uid, uint8_t uidLen,
                                                            uint32_t blockNumber,
                                                            uint8_t keyNumber, uint8_t *keyData);
MODULE_PART uint8_t  PN532_mifareclassic_ReadDataBlock(uint8_t blockNumber, uint8_t *data);
MODULE_PART uint8_t  PN532_mifareclassic_WriteDataBlock(uint8_t blockNumber, uint8_t *data);
MODULE_PART uint8_t  PN532_ntag21x_probe(void);
MODULE_PART bool     PN532_ntag21x_auth(void);
MODULE_PART bool     PN532_ntag2xx_read16(const uint8_t page, char *out);
MODULE_PART bool     PN532_ntag2xx_write4(uint8_t page, char *in);
MODULE_PART bool     PN532_ntag2xx_write16(uint8_t page, char *in);
MODULE_PART bool     PN532_ntag21x_set_password(uint8_t confPage, bool unsetPasswd);
MODULE_PART void     PN532_Erase(void);
MODULE_PART void     PN532_Write(void);
MODULE_PART void     PN532_Auth(void);
MODULE_PART void     PN532_Set_PWD(void);
MODULE_PART void     PN532_Unset_PWD(void);
MODULE_PART void     PN532_Cancel(void);
#endif
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// Per-card state
// --------------------------------------------------------------------
typedef struct PN532 {
  char     uids[21];             // hex string of UID
  uint8_t  packetbuffer[64];     // global I/O buffer
  uint8_t  command;              // last sent command byte (for response routing)
  uint8_t  scantimer;            // 250ms ticks until the next scan is allowed
  uint16_t atqa;                 // SENS_RES — picks Mifare/NTAG branches
#ifdef USE_PN532_DATA_FUNCTION
  uint8_t  newdata[16];          // payload for write commands
  uint8_t  function;             // 0 idle / 1 erase / 2 write / 3 set-pwd / 4 unset-pwd
  uint32_t pwd_auth;
  uint16_t pwd_pack;
  uint32_t pwd_auth_new;
  uint16_t pwd_pack_new;
#endif
} PN532;

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

typedef struct {
  TWIp          *xWire;
  bool           ready;
  uint8_t        mode;     // 1 = I2C, 0 = HSU
  uint8_t        bus;      // I2C bus the chip was found on (mode==1 only)
  PN532          Pn532;
  TasmotaSerial *ts;
  uint8_t        rec;
  uint8_t        trx;
} MODULE_MEMORY;

#  define ready    mem->ready
#  define Pn532    mem->Pn532
#  define ts       mem->ts
#  define rec      mem->rec
#  define trx      mem->trx
#  define mode     mem->mode
#  define pn_bus   mem->bus

#else  // native

typedef struct {
  bool           ready;
  uint8_t        mode;
  uint8_t        bus;
  PN532          Pn532;
  TasmotaSerial *ts;
  uint8_t        rec;
  uint8_t        trx;
  bool           initialized_flag;
} pn532_state_t;

static pn532_state_t *pn532_state = nullptr;

#  define ready       pn532_state->ready
#  define Pn532       pn532_state->Pn532
#  define ts          pn532_state->ts
#  define rec         pn532_state->rec
#  define trx         pn532_state->trx
#  define mode        pn532_state->mode
#  define pn_bus      pn532_state->bus
#  define initialized pn532_state->initialized_flag

#  define ALLOCMEM    DUAL_ALLOCMEM(pn532)
#  define RETMEM      DUAL_RETMEM(pn532)

#  define XSNS_40     40

#endif  // BUILD_AS_PLUGIN

// XdrvMailbox accessor (pointer in plugin, instance in native)
#if BUILD_AS_PLUGIN
#  define _PN532_MB_DATA  (XdrvMailbox->data)
#else
#  define _PN532_MB_DATA  (XdrvMailbox.data)
#endif

// --------------------------------------------------------------------
// PROGMEM ack / nack frames
// --------------------------------------------------------------------
const uint8_t PN532_NACK_TAB[6] PROGMEM = {0, 0, 0xff, 0xff, 0, 0};
const uint8_t PN532_ACK_TAB[6]  PROGMEM = {0, 0, 0xff, 0,    0xff, 0};

// --------------------------------------------------------------------
// Init — pick I2C or HSU based on the user-set mode flag.
// --------------------------------------------------------------------
bool PN532_Init(void) {
  ALLOCMEM
  ready = false;

#if BUILD_AS_PLUGIN
  rec  = (uint8_t)(mp->ms[0].value & 0xff);
  trx  = (uint8_t)(mp->ms[1].value & 0xff);
  mode = (uint8_t)(mp->ms[2].value & 1);
#else
  // Native: pull pins from Tasmota template, fall back to UART0.
#  if defined(GPIO_PN532_RXD)
  int8_t gp = Pin(GPIO_PN532_RXD);  rec = (gp >= 0) ? (uint8_t)gp : (uint8_t)PN532_DEFAULT_RXD;
#  else
  rec = PN532_DEFAULT_RXD;
#  endif
#  if defined(GPIO_PN532_TXD)
  gp  = Pin(GPIO_PN532_TXD);        trx = (gp >= 0) ? (uint8_t)gp : (uint8_t)PN532_DEFAULT_TXD;
#  else
  trx = PN532_DEFAULT_TXD;
#  endif
  mode = PN532_DEFAULT_MODE;
#endif

  Pn532.scantimer = 0;

  if (mode) {
    // I2C mode — probe both buses for the chip address.
    pn_bus = 0;
    bool found = false;
    for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
      I2C_SETWIRE(bus);
      if (I2C_SetDevice(PN532_I2_ADDR, bus)) {
        pn_bus = bus;
        I2C_SetActiveFound(PN532_I2_ADDR, PSTR("PN532"), bus);
        found = true;
        break;
      }
    }
    if (!found) { PN532_Deinit(); return false; }
  } else {
    // HSU / serial mode
    ts = (TasmotaSerial *)NewTS(rec, trx);
    if (!ts || !beginTS(ts, 115200)) {
      PN532_Deinit();
      return false;
    }
    if (hardwareSerial(ts)) { ClaimSerial(); }
  }

  PN532_wakeup();

  uint32_t ver = PN532_getFirmwareVersion();
  if (ver) {
    AddLog(LOG_LEVEL_INFO,
           PSTR("NFC: PN532 NFC Reader detected v%u.%u (%s)"),
           (ver >> 16) & 0xFF, (ver >> 8) & 0xFF,
           mode ? PSTR("I2C") : PSTR("HSU"));
    initialized = true;
    ready       = true;
    PN532_setPassiveActivationRetries(0xFF);
    PN532_SAMConfig();
  }

  if (!ready) { PN532_Deinit(); }
  return ready;
}

// --------------------------------------------------------------------
// Frame I/O
// --------------------------------------------------------------------
int16_t PN532_getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout) {
  SETREGS
  I2C_SETWIRE(pn_bus);
  uint16_t ctime = 0;
  do {
    if (I2C_requestFrom(PN532_I2_ADDR, 6)) {
      if (I2C_read() & 1) { break; }      // PN532 ready bit
    }
    delay(1);
    ctime++;
    if ((0 != timeout) && (ctime > timeout)) { return -1; }
  } while (1);

  if (0x00 != I2C_read() || 0x00 != I2C_read() || 0xFF != I2C_read()) {
    return PN532_INVALID_FRAME;
  }
  uint8_t length = I2C_read();

  // Re-request the same frame (NACK) — we'll consume it in readResponse.
  I2C_beginTransmission(PN532_I2_ADDR);
  for (uint16_t i = 0; i < sizeof(PN532_NACK_TAB); ++i) {
    I2C_write(pgm_read_byte(&PN532_NACK_TAB[EXEC_OFFSET + i]));
  }
  I2C_endTransmission(true);

  return length;
}

int8_t PN532_receive(uint8_t *buf, int len, uint16_t timeout) {
  SETREGS
  int read_bytes = 0;
  int ret;
  unsigned long start_millis;
  while (read_bytes < len) {
    start_millis = millis();
    do {
      ret = readbTS(ts);
      if (ret >= 0) { break; }
    } while ((timeout == 0) || ((millis() - start_millis) < timeout));

    if (ret < 0) {
      return read_bytes ? read_bytes : (int)PN532_TIMEOUT;
    }
    buf[read_bytes++] = (uint8_t)ret;
  }
  return read_bytes;
}

int8_t PN532_writeCommand(const uint8_t *header, uint8_t hlen,
                          const uint8_t *body = 0, uint8_t blen = 0) {
  SETREGS
  Pn532.command = header[0];

  if (mode) {
    // I2C path
    I2C_SETWIRE(pn_bus);
    I2C_beginTransmission(PN532_I2_ADDR);
    I2C_write(PN532_PREAMBLE);
    I2C_write(PN532_STARTCODE1);
    I2C_write(PN532_STARTCODE2);

    uint8_t length = hlen + blen + 1;
    I2C_write(length);
    I2C_write(~length + 1);     // length checksum
    I2C_write(PN532_HOSTTOPN532);

    uint8_t sum = PN532_HOSTTOPN532;
    for (uint8_t i = 0; i < hlen; i++) {
      if (I2C_write(header[i])) { sum += header[i]; }
      else                       { return PN532_INVALID_FRAME; }
    }
    for (uint8_t i = 0; i < blen; i++) {
      if (I2C_write(body[i])) { sum += body[i]; }
      else                     { return PN532_INVALID_FRAME; }
    }

    I2C_write(~sum + 1);        // data checksum
    I2C_write(PN532_POSTAMBLE);
    I2C_endTransmission(true);

  } else {
    // HSU path
    flushTS(ts);
    bwriteTS(ts, (uint8_t)PN532_PREAMBLE);
    bwriteTS(ts, (uint8_t)PN532_STARTCODE1);
    bwriteTS(ts, PN532_STARTCODE2);

    uint8_t length = hlen + blen + 1;
    bwriteTS(ts, length);
    bwriteTS(ts, ~length + 1);
    bwriteTS(ts, PN532_HOSTTOPN532);

    uint8_t sum = PN532_HOSTTOPN532;
    writeTS(ts, (uint8_t *)header, hlen);
    for (uint32_t i = 0; i < hlen; i++) { sum += header[i]; }
    if (blen) {
      writeTS(ts, (uint8_t *)body, blen);
      for (uint32_t i = 0; i < blen; i++) { sum += body[i]; }
    }
    bwriteTS(ts, ~sum + 1);
    bwriteTS(ts, (uint8_t)PN532_POSTAMBLE);
  }
  return PN532_readAckFrame();
}

int16_t PN532_readResponse(uint8_t buf[], uint8_t len, uint16_t timeout = 50) {
  SETREGS
  if (mode) {
    I2C_SETWIRE(pn_bus);
    uint16_t ctime = 0;
    uint8_t  length = PN532_getResponseLength(buf, len, timeout);

    do {
      if (I2C_requestFrom(PN532_I2_ADDR, 6 + length + 2)) {
        if (I2C_read() & 1) { break; }
      }
      delay(1);
      ctime++;
      if ((0 != timeout) && (ctime > timeout)) { return -1; }
    } while (1);

    if (0x00 != I2C_read() || 0x00 != I2C_read() || 0xFF != I2C_read()) {
      return PN532_INVALID_FRAME;
    }
    length = I2C_read();
    if (0 != (uint8_t)(length + I2C_read())) { return PN532_INVALID_FRAME; }

    uint8_t cmd = Pn532.command + 1;
    if (PN532_PN532TOHOST != I2C_read() || (cmd) != I2C_read()) {
      return PN532_INVALID_FRAME;
    }
    length -= 2;
    if (length > len) { return PN532_NO_SPACE; }

    uint8_t sum = PN532_PN532TOHOST + cmd;
    for (uint8_t i = 0; i < length; i++) { buf[i] = I2C_read(); sum += buf[i]; }
    uint8_t checksum = I2C_read();
    if (0 != (uint8_t)(sum + checksum)) { return PN532_INVALID_FRAME; }
    I2C_read();  // POSTAMBLE
    return length;
  } else {
    uint8_t tmp[3];
    if (PN532_receive(tmp, 3, timeout) <= 0)               { return PN532_TIMEOUT; }
    if (0 != tmp[0] || 0 != tmp[1] || 0xFF != tmp[2])      { return PN532_INVALID_FRAME; }

    uint8_t length[2];
    if (PN532_receive(length, 2, timeout) <= 0)            { return PN532_TIMEOUT; }
    if (0 != (uint8_t)(length[0] + length[1]))             { return PN532_INVALID_FRAME; }
    length[0] -= 2;
    if (length[0] > len)                                   { return PN532_NO_SPACE; }

    uint8_t cmd = Pn532.command + 1;
    if (PN532_receive(tmp, 2, timeout) <= 0)               { return PN532_TIMEOUT; }
    if (PN532_PN532TOHOST != tmp[0] || cmd != tmp[1])      { return PN532_INVALID_FRAME; }

    if (PN532_receive(buf, length[0], timeout) != length[0]) { return PN532_TIMEOUT; }

    uint8_t sum = PN532_PN532TOHOST + cmd;
    for (uint32_t i = 0; i < length[0]; i++) { sum += buf[i]; }
    if (PN532_receive(tmp, 2, timeout) <= 0)               { return PN532_TIMEOUT; }
    if (0 != (uint8_t)(sum + tmp[0]) || 0 != tmp[1])       { return PN532_INVALID_FRAME; }
    return length[0];
  }
}

int8_t PN532_readAckFrame(void) {
  SETREGS
  uint8_t ackBuf[sizeof(PN532_ACK_TAB)];

  if (mode) {
    I2C_SETWIRE(pn_bus);
    uint16_t time = 0;
    do {
      if (I2C_requestFrom(PN532_I2_ADDR, sizeof(PN532_ACK_TAB) + 1)) {
        if (I2C_read() & 1) { break; }
      }
      delay(1);
      time++;
      if (time > PN532_ACK_WAIT_TIME) { return PN532_TIMEOUT; }
    } while (1);
    for (uint8_t i = 0; i < sizeof(PN532_ACK_TAB); i++) { ackBuf[i] = I2C_read(); }
  } else {
    if (PN532_receive(ackBuf, sizeof(PN532_ACK_TAB), PN532_ACK_WAIT_TIME) <= 0) {
      return PN532_TIMEOUT;
    }
  }
  if (memcmp(&ackBuf, &PN532_ACK_TAB[EXEC_OFFSET], sizeof(PN532_ACK_TAB))) {
    return PN532_INVALID_ACK;
  }
  return 0;
}

uint32_t PN532_getFirmwareVersion(void) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_GETFIRMWAREVERSION;
  if (PN532_writeCommand(Pn532.packetbuffer, 1, 0, 0) > 0) { return 0; }
  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer), 50) < 0) { return 0; }
  uint32_t r = Pn532.packetbuffer[0];
  r = (r << 8) | Pn532.packetbuffer[1];
  r = (r << 8) | Pn532.packetbuffer[2];
  r = (r << 8) | Pn532.packetbuffer[3];
  return r;
}

void PN532_wakeup(void) {
  SETREGS
  if (mode) { return; }
  uint8_t wakeup[5] = { 0x55, 0x55, 0, 0, 0 };
  writeTS(ts, wakeup, sizeof(wakeup));
  flushTS(ts);
}

bool PN532_setPassiveActivationRetries(uint8_t maxRetries) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_RFCONFIGURATION;
  Pn532.packetbuffer[1] = 5;
  Pn532.packetbuffer[2] = 0xFF;
  Pn532.packetbuffer[3] = 0x01;
  Pn532.packetbuffer[4] = maxRetries;
  if (PN532_writeCommand(Pn532.packetbuffer, 5)) { return 0; }
  return 0 < PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));
}

bool PN532_SAMConfig(void) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_SAMCONFIGURATION;
  Pn532.packetbuffer[1] = 0x01;   // normal mode
  Pn532.packetbuffer[2] = 0x14;   // timeout 50ms × 20 = 1 s
  Pn532.packetbuffer[3] = 0x00;   // no IRQ pin
  if (PN532_writeCommand(Pn532.packetbuffer, 4)) { return false; }
  return 0 < PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));
}

bool PN532_readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid,
                               uint8_t *uidLength, uint16_t timeout = 50) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  Pn532.packetbuffer[1] = 1;
  Pn532.packetbuffer[2] = cardbaudrate;
  if (PN532_writeCommand(Pn532.packetbuffer, 3)) { return false; }
  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer), timeout) < 0) {
    return false;
  }
  if (Pn532.packetbuffer[0] != 1) { return false; }

  Pn532.atqa = ((uint16_t)Pn532.packetbuffer[2] << 8) | Pn532.packetbuffer[3];
  *uidLength = Pn532.packetbuffer[5];
  for (uint32_t i = 0; i < Pn532.packetbuffer[5]; i++) {
    uid[i] = Pn532.packetbuffer[6 + i];
  }
  return true;
}

// --------------------------------------------------------------------
// USE_PN532_DATA_FUNCTION — Mifare/NTAG erase/write/auth subset
// --------------------------------------------------------------------
#ifdef USE_PN532_DATA_FUNCTION

const struct {
  uint8_t version[6];
  uint8_t confPage;
} NTAG[] PROGMEM = {
  { {0x04, 0x02, 0x01, 0x00, 0x0f, 0x03}, 0x29 },  // NTAG213
  { {0x04, 0x02, 0x01, 0x00, 0x11, 0x03}, 0x83 },  // NTAG215
  { {0x04, 0x02, 0x01, 0x00, 0x13, 0x03}, 0xe3 },  // NTAG216
  { {0x04, 0x05, 0x02, 0x02, 0x13, 0x03}, 0xe3 },  // NT3H2111
  { {0x04, 0x05, 0x02, 0x02, 0x15, 0x03}, 0xe3 },  // NT3H2211
};
#define NTAG_CNT (sizeof(NTAG) / 7)

void PN532_inRelease(void) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INRELEASE;
  Pn532.packetbuffer[1] = 1;
  if (PN532_writeCommand(Pn532.packetbuffer, 2)) { return; }
  PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));
}

uint8_t PN532_mifareclassic_AuthenticateBlock(uint8_t *uid, uint8_t uidLen,
                                              uint32_t blockNumber,
                                              uint8_t keyNumber, uint8_t *keyData) {
  SETREGS
  uint8_t _key[6], _uid[7];
  memcpy_P(&_key, keyData, 6);
  memcpy_P(&_uid, uid, uidLen);

  Pn532.packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  Pn532.packetbuffer[1] = 1;
  Pn532.packetbuffer[2] = (keyNumber) ? MIFARE_CMD_AUTH_B : MIFARE_CMD_AUTH_A;
  Pn532.packetbuffer[3] = blockNumber;
  memcpy_P(&Pn532.packetbuffer[4], &_key, 6);
  for (uint8_t i = 0; i < uidLen; i++) {
    Pn532.packetbuffer[10 + i] = _uid[i];
  }

  if (PN532_writeCommand(Pn532.packetbuffer, 10 + uidLen)) { return 0; }
  PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));
  return (Pn532.packetbuffer[0] == 0x00) ? 1 : 0;
}

uint8_t PN532_mifareclassic_ReadDataBlock(uint8_t blockNumber, uint8_t *data) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  Pn532.packetbuffer[1] = 1;
  Pn532.packetbuffer[2] = MIFARE_CMD_READ;
  Pn532.packetbuffer[3] = blockNumber;
  if (PN532_writeCommand(Pn532.packetbuffer, 4)) { return 0; }
  PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));
  if (Pn532.packetbuffer[0] != 0x00) { return 0; }
  memcpy_P(data, &Pn532.packetbuffer[1], 16);
  return 1;
}

uint8_t PN532_mifareclassic_WriteDataBlock(uint8_t blockNumber, uint8_t *data) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  Pn532.packetbuffer[1] = 1;
  Pn532.packetbuffer[2] = MIFARE_CMD_WRITE;
  Pn532.packetbuffer[3] = blockNumber;
  memcpy_P(&Pn532.packetbuffer[4], data, 16);
  if (PN532_writeCommand(Pn532.packetbuffer, 20)) { return 0; }
  return (0 < PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer))) ? 1 : 0;
}

uint8_t PN532_ntag21x_probe(void) {
  SETREGS
  uint8_t result = 0;
  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG21X_CMD_GET_VERSION;
  if (PN532_writeCommand(Pn532.packetbuffer, 2)) { return result; }
  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)) < 9) { return result; }
  if (Pn532.packetbuffer[3] != 4) { return result; }     // not an NTAG

  for (uint8_t i = 0; i < NTAG_CNT; i++) {
    if (0 == memcmp_P(&Pn532.packetbuffer[3],
                      &NTAG[(tmod__udivsi3(EXEC_OFFSET, 7)) + i].version[0], 6)) {
      memcpy_P(&result, &NTAG[(tmod__udivsi3(EXEC_OFFSET, 7)) + i].confPage, sizeof(result));
    }
  }
  return result;
}

bool PN532_ntag21x_auth(void) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG21X_CMD_PWD_AUTH;
  memcpy_P(&Pn532.packetbuffer[2], &Pn532.pwd_auth, 4);

  if (PN532_writeCommand(Pn532.packetbuffer, 6)) { return false; }
  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)) < 3) { return false; }
  if (Pn532.packetbuffer[0]) { return false; }
  return memcmp(&Pn532.packetbuffer[1], &Pn532.pwd_pack, 2) == 0;
}

bool PN532_ntag2xx_read16(const uint8_t page, char *out) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG2XX_CMD_READ;
  Pn532.packetbuffer[2] = page;
  if (PN532_writeCommand(Pn532.packetbuffer, 3)) { return false; }
  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)) < 17) { return false; }
  if (Pn532.packetbuffer[0] != 0) { return false; }
  memcpy_P(out, &Pn532.packetbuffer[1], 16);
  return true;
}

bool PN532_ntag2xx_write4(uint8_t page, char *in) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG2XX_CMD_WRITE;
  Pn532.packetbuffer[2] = page;
  memcpy_P(&Pn532.packetbuffer[3], in, 4);
  if (PN532_writeCommand(Pn532.packetbuffer, 7)) { return false; }
  return PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)) >= 1;
}

bool PN532_ntag2xx_write16(uint8_t page, char *in) {
  SETREGS
  for (uint8_t i = 0; i < 4; i++) {
    if (!PN532_ntag2xx_write4(page + i, &in[i << 2])) { return false; }
  }
  return true;
}

bool PN532_ntag21x_set_password(uint8_t confPage, bool unsetPasswd) {
  SETREGS
  char card_datas[16];
  if (PN532_ntag2xx_read16(confPage, card_datas)) {
    if (unsetPasswd) {
      card_datas[3] = 0xFF;
      return PN532_ntag2xx_write4(confPage, card_datas);
    }
    card_datas[3]  = 0;
    card_datas[4] |= 0x80;
    memcpy_P(&card_datas[8],  &Pn532.pwd_auth_new, 4);
    memcpy_P(&card_datas[12], &Pn532.pwd_pack_new, 2);
    return PN532_ntag2xx_write16(confPage, card_datas);
  }
  return false;
}

void PN532_Erase(void) {
  SETREGS
  memset(Pn532.newdata, 0, sizeof(Pn532.newdata));
  Pn532.function = 1;
  AddLog(LOG_LEVEL_INFO, PSTR("data block 1 (4-7 for NTAG) will be erased"));
}

void PN532_Write(void) {
  SETREGS
  memset(Pn532.newdata, 0, sizeof(Pn532.newdata));
  strncpy((char *)Pn532.newdata, _PN532_MB_DATA, sizeof(Pn532.newdata));
  Pn532.function = 2;
  AddLog(LOG_LEVEL_INFO, PSTR("data block 1 (4-7 for NTAG) will be set to '%s'"), Pn532.newdata);
}

void PN532_Auth(void) {
  SETREGS
  // legacy stub — original used ArgC()/ArgV() helpers that aren't
  // in the BinPlugin runtime; left as a no-op to preserve the
  // command surface.
}

void PN532_Set_PWD(void) {
  SETREGS
  AddLog(LOG_LEVEL_INFO, PSTR("will be protected"));
  Pn532.function = 3;
}

void PN532_Unset_PWD(void) {
  SETREGS
  AddLog(LOG_LEVEL_INFO, PSTR("will be unprotected"));
  Pn532.function = 4;
}

void PN532_Cancel(void) {
  SETREGS
  AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Job canceled"));
  Pn532.function = 0;
}

const char PN532Commands[] PROGMEM = "pn532|Erase|Write|Auth|Set_PWD|Unset_PWD|Cancel";
VTABLE(PN532Command) = {
  &PN532_Erase, &PN532_Write, &PN532_Auth,
  &PN532_Set_PWD, &PN532_Unset_PWD, &PN532_Cancel,
};

#endif  // USE_PN532_DATA_FUNCTION

// --------------------------------------------------------------------
// Periodic scan
// --------------------------------------------------------------------
void PN532_ScanForTag(void) {
  SETREGS
  uint8_t uid[7] = { 0 };
  uint8_t uid_len = 0;

  if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uid_len)) {
    ToHex_P((unsigned char *)uid, uid_len, Pn532.uids, sizeof(Pn532.uids));

#ifdef USE_PN532_DATA_FUNCTION
    bool success = false;
    char card_datas[17];
    memset(card_datas, 0, sizeof(card_datas));

    enum { NOPWD, PWD_NONE, PWD_OK, PWD_NOK } str_pwd = NOPWD;

    if (Pn532.atqa == 0x44) {
      uint8_t confPage = 0;
      uint8_t nuid[7] = { 0 };
      uint8_t nuid_len = 0;
      if ((confPage = PN532_ntag21x_probe()) > 0) {
        str_pwd = PWD_NONE;
        if (!PN532_ntag2xx_read16(4, card_datas)) {
          if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, nuid, &nuid_len)) {
            if (memcmp(uid, nuid, sizeof(uid)) == 0) {
              if (PN532_ntag21x_auth()) {
                str_pwd = PWD_OK;
                if (Pn532.function == 3) { success = PN532_ntag21x_set_password(confPage, false); }
                if (Pn532.function == 4) { success = PN532_ntag21x_set_password(confPage, true);  }
              } else {
                str_pwd = PWD_NOK;
              }
              if (!PN532_ntag2xx_read16(4, card_datas)) { card_datas[0] = 0; }
            }
          }
        } else {
          if (Pn532.function == 3) { success = PN532_ntag21x_set_password(confPage, false); }
        }
      } else {
        if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, nuid, &nuid_len)) {
          if (memcmp(uid, nuid, sizeof(uid)) == 0) {
            if (!PN532_ntag2xx_read16(4, card_datas)) { card_datas[0] = 0; }
          }
        }
      }
      if ((Pn532.function == 1) || (Pn532.function == 2)) {
        success = PN532_ntag2xx_write16(4, (char *)Pn532.newdata);
        if (!PN532_ntag2xx_read16(4, card_datas)) { card_datas[0] = 0; }
      }
    } else if (uid_len == 4) {
      uint8_t keyuniversal[6];
      memset(keyuniversal, 0xff, 6);
      if (PN532_mifareclassic_AuthenticateBlock(uid, uid_len, 1, 1, keyuniversal)) {
        if ((Pn532.function == 1) || (Pn532.function == 2)) {
          success = PN532_mifareclassic_WriteDataBlock(1, Pn532.newdata);
        }
        if (PN532_mifareclassic_ReadDataBlock(1, (uint8_t *)card_datas)) {
          for (uint32_t i = 0; i < 16; i++) {
            if (!isprint(card_datas[i])) { card_datas[i] = 0; }
          }
        } else {
          card_datas[0] = 0;
        }
      } else {
        sprintf_P(card_datas, PSTR("AUTHFAIL"), 0);
      }
    }
    switch (Pn532.function) {
      case 1: AddLog(LOG_LEVEL_INFO, success ? PSTR("NFC: PN532 - Erase success")            : PSTR("NFC: PN532 - Erase fail - exiting erase mode"));     break;
      case 2: AddLog(LOG_LEVEL_INFO, success ? PSTR("NFC: PN532 - Data write successful")    : PSTR("NFC: PN532 - Write failed - exiting set mode"));     break;
      case 3: AddLog(LOG_LEVEL_INFO, success ? PSTR("NFC: PN532 - Set password successful")  : PSTR("NFC: PN532 - Set password failed - exiting set mode"));  break;
      case 4: AddLog(LOG_LEVEL_INFO, success ? PSTR("NFC: PN532 - Unset password successful"): PSTR("NFC: PN532 - Unset password failed - exiting set mode")); break;
      default: break;
    }
    Pn532.function = 0;
    card_datas[16] = 0;
    ResponseTime_P(PSTR(",\"PN532\":{\"UID\":\"%s\",\"Data\":\"%s\""), Pn532.uids, card_datas);
    if      (str_pwd == PWD_NONE) { ResponseAppend_P(PSTR(",\"Auth\":\"None\"")); }
    else if (str_pwd == PWD_OK)   { ResponseAppend_P(PSTR(",\"Auth\":\"Ok\""));   }
    else if (str_pwd == PWD_NOK)  { ResponseAppend_P(PSTR(",\"Auth\":\"NOk\""));  }
    ResponseAppend_P(PSTR("}}"));
    PN532_inRelease();
#else
    ResponseTime_P(PSTR(",\"PN532\":{\"UID\":\"%s\"}}"), Pn532.uids);
#endif
    MqttPublishTeleSensor();
    Pn532.scantimer = 7;   // ignore further reads for ~1.75 s
  } else {
    Pn532.uids[0] = '-';
    Pn532.uids[1] = 0;
  }
}

void PN532_Show(void) {
  SETREGS
#ifdef USE_WEBSERVER
  WSContentSend_PD(PSTR("{s}PN532 UID{m}%s{e}"), Pn532.uids);
#endif
}

void PN532_Deinit(void) {
  SETREGS
  if (mode) {
    I2C_ResetActive(PN532_I2_ADDR, pn_bus);
  } else {
    if (ts) { deleteTS(ts); ts = nullptr; }
  }
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  SETREGS
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:              result = PN532_Init(); break;
    case pFUNC_EVERY_250_MSECOND:
      if (Pn532.scantimer > 0) { Pn532.scantimer--; }
      else                     { PN532_ScanForTag();  }
      break;
    case pFUNC_WEB_SENSOR:        PN532_Show(); break;
#ifdef USE_PN532_DATA_FUNCTION
    case pFUNC_COMMAND:
      result = DecodeCommand(PN532Commands, PN532Command);
      if (!result) { AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Next scanned tag")); }
      else         { ResponseCmndDone(); }
      break;
#endif
    case pFUNC_DEINIT:            PN532_Deinit(); break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns40(uint32_t function) {
  bool result = false;
  if (FUNC_INIT == function) {
    PN532_Init();
  } else if (pn532_state && ready) {
    switch (function) {
      case FUNC_EVERY_250_MSECOND:
        if (Pn532.scantimer > 0) { Pn532.scantimer--; }
        else                     { PN532_ScanForTag();  }
        break;
#  ifdef USE_PN532_DATA_FUNCTION
      case FUNC_COMMAND:
        result = DecodeCommand(PN532Commands, PN532Command);
        if (!result) { AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Next scanned tag")); }
        else         { ResponseCmndDone(); }
        break;
#  endif
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:        PN532_Show(); break;
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
#  undef ready
#  undef Pn532
#  undef ts
#  undef rec
#  undef trx
#  undef mode
#  undef pn_bus
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef _PN532_MB_DATA

#endif  // _PN532_DUAL_ENABLED
