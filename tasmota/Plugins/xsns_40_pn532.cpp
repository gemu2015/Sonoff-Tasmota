/*
  xsns_40_pn532.ino - Support for PN532 (HSU) NFC Tag Reader on Tasmota

  Copyright (C) 2021  Andre Thomas, Theo Arends and md5sum-as (https://github.com/md5sum-as)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// 50 = EEPROM
// 24 = PN532
// 76 = BMP
// 44 = SHT
// 27 = LCD

#include "tasmota_options.h" 

#ifdef USE_PN532_HSU_MOD

#include "module.h"
#include "module_defines.h"

/*********************************************************************************************\
 * PN532 - 13.56 MHz RFID and NFC reader
\*********************************************************************************************/

#define PN532_I2_ADDR                               0x24
#pragma GCC optimize ("-Og")

//SDA/TXD
//SCL/RXD
//#define USE_PN532_DATA_FUNCTION

#define PN532_INVALID_ACK                           -1
#define PN532_TIMEOUT                               -2
#define PN532_INVALID_FRAME                         -3
#define PN532_NO_SPACE                              -4

#define PN532_PREAMBLE                              0x00
#define PN532_STARTCODE1                            0x00
#define PN532_STARTCODE2                            0xFF
#define PN532_POSTAMBLE                             0x00

#define PN532_HOSTTOPN532                           0xD4
#define PN532_PN532TOHOST                           0xD5

#define PN532_ACK_WAIT_TIME                         0x0A

#define PN532_COMMAND_GETFIRMWAREVERSION            0x02
#define PN532_COMMAND_SAMCONFIGURATION              0x14
#define PN532_COMMAND_RFCONFIGURATION               0x32
#define PN532_COMMAND_INDATAEXCHANGE                0x40
#define PN532_COMMAND_INCOMMUNICATETHRU             0x42
#define PN532_COMMAND_INLISTPASSIVETARGET           0x4A
#define PN532_COMMAND_INRELEASE                     0x52
#define PN532_COMMAND_INSELECT                      0x54
#define PN532_MIFARE_ISO14443A                      0x00
#define MIFARE_CMD_READ                             0x30
#define MIFARE_CMD_AUTH_A                           0x60
#define MIFARE_CMD_AUTH_B                           0x61
#define MIFARE_CMD_WRITE                            0xA0

#define NTAG21X_CMD_GET_VERSION                     0x60
#define NTAG2XX_CMD_READ                            0x30
#define NTAG21X_CMD_FAST_READ                       0x3A
#define NTAG21X_CMD_PWD_AUTH                        0x1B
#define NTAG2XX_CMD_WRITE                           0xA2

typedef struct PN532 {
  char uids[21];                // Number of bytes in the UID. 4, 7 or 10
  uint8_t packetbuffer[64];     // Global buffer used to store packet
  uint8_t command;          // Carry command code between functions
  uint8_t scantimer;        // Prevent multiple successful reads within 2 second window
  uint16_t atqa;
#ifdef USE_PN532_DATA_FUNCTION
  uint8_t newdata[16];
  uint8_t function;
  uint32_t pwd_auth;
  uint16_t pwd_pack;
  uint32_t pwd_auth_new;
  uint16_t pwd_pack_new;
#endif  // USE_PN532_DATA_FUNCTION
} PN532;

// all memory must be in struct MODULE_MEMORY
typedef struct {
  TwoWire *xWire;
  bool ready;
  uint8_t mode;
  PN532 Pn532;
  TasmotaSerial *ts;
  uint8_t rec;
  uint8_t trx;
} MODULE_MEMORY;

#define ready mem->ready
#define Pn532 mem->Pn532
#define ts mem->ts
#define rec mem->rec
#define trx mem->trx
#define mode mem->mode

#define PN532_REV  1<<16|2

PUSH_OPTIONS

#ifdef USE_PN532_DATA_FUNCTION
MODULE_DESCRIPTOR("PN532_D", MODULE_TYPE_SENSOR, PN532_REV,"RXD",3,"TXD",1,"MODE",0x01000101,"",0)
#else
MODULE_DESCRIPTOR("PN532", MODULE_TYPE_SENSOR, PN532_REV,"RXD",3,"TXD",1,"MODE",0x01000101,"",0)
#endif
MODULE_PART bool PN532_Init();
MODULE_PART int8_t PN532_receive(uint8_t *buf, int len, uint16_t timeout);
MODULE_PART int8_t PN532_readAckFrame();
MODULE_PART int8_t PN532_writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen);
MODULE_PART int16_t PN532_readResponse(uint8_t buf[], uint8_t len, uint16_t timeout);
MODULE_PART uint32_t PN532_getFirmwareVersion();
MODULE_PART int16_t PN532_getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout);
MODULE_PART void PN532_wakeup();
MODULE_PART bool PN532_setPassiveActivationRetries(uint8_t maxRetries);
MODULE_PART bool PN532_SAMConfig();
MODULE_PART void PN532_ScanForTag();
MODULE_PART bool PN532_readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout);
MODULE_PART void PN532_Show();
MODULE_PART void PN532_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END


bool PN532_Init() {
  ALLOCMEM
  
  SETWIRE(0);
  
  ready = false;
  rec = mp->ms[0].value;
  trx = mp->ms[1].value;
  mode = mp->ms[2].value & 1;

  Pn532.scantimer = 0;

  if (mode) {
    if (I2cSetDevice(PN532_I2_ADDR, 0)) {
      I2cSetActiveFound(PN532_I2_ADDR, PSTR("PN532"), 0);
    } else {
      PN532_Deinit();
      return false;
    }
  } else {
    ts = NewTS(rec, trx);
    if (ts) {
      if (beginTS(ts, 115200)) {
        if (hardwareSerial(ts)) {
          ClaimSerial();
        }
      }
    } else {
      PN532_Deinit();
      return false;
    }
  }

  PN532_wakeup();

  uint32_t ver = PN532_getFirmwareVersion();
  if (ver) {
    AddLog(LOG_LEVEL_INFO,PSTR("NFC: PN532 NFC Reader detected v%u.%u"),(ver>>16) & 0xFF, (ver>>8) & 0xFF);
    initialized = true;
    ready = true;
    PN532_setPassiveActivationRetries(0xFF);
    PN532_SAMConfig();
  }

  if (ready == false) {
    PN532_Deinit();
  }
  return ready;
}

const uint8_t PN532_NACK[6] PROGMEM = {0,0,0xff,0xff,0,0};

int16_t PN532_getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout) {
  SETREGS

    uint16_t ctime = 0;

    do {
        if (requestFrom(PN532_I2_ADDR, 6)) {
            if (read() & 1) {  // check first byte --- status
                break;         // PN532 is ready
            }
        }

        delay(1);
        ctime++;
        if ((0 != timeout) && (ctime > timeout)) {
            return -1;
        }
    } while (1); 
    
    if (0x00 != read()      ||       // PREAMBLE
            0x00 != read()  ||       // STARTCODE1
            0xFF != read()           // STARTCODE2
        ) {
        
        return PN532_INVALID_FRAME;
    }
    
    uint8_t length = read();

    // request for last respond msg again
    beginTransmission(PN532_I2_ADDR);
    for (uint16_t i = 0; i < sizeof(PN532_NACK); ++i) {
      write(pgm_read_byte(&PN532_NACK[mt->execution_offset+i]));
    }
    endTransmission(true);

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
      if (ret >= 0) {
        break;
      }
    } while((timeout == 0) || ((millis()- start_millis ) < timeout));

    if (ret < 0) {
      if (read_bytes) {
        return read_bytes;
      } else {
        return PN532_TIMEOUT;
      }
    }
    buf[read_bytes] = (uint8_t)ret;
    read_bytes++;
  }
  return read_bytes;
}

int8_t PN532_writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0) {
  SETREGS
  Pn532.command = header[0];
  if (mode) {
    beginTransmission(PN532_I2_ADDR);
    
    write(PN532_PREAMBLE);
    write(PN532_STARTCODE1);
    write(PN532_STARTCODE2);
    
    uint8_t length = hlen + blen + 1;   // length of data field: TFI + DATA
    write(length);
    write(~length + 1);                 // checksum of length
    
    write(PN532_HOSTTOPN532);
    uint8_t sum = PN532_HOSTTOPN532;    // sum of TFI + DATA
         
    for (uint8_t i = 0; i < hlen; i++) {
        if (write(header[i])) {
          sum += header[i];
        } else {
          return PN532_INVALID_FRAME;
        }
    }

    for (uint8_t i = 0; i < blen; i++) {
        if (write(body[i])) {
          sum += body[i];
        } else {
          return PN532_INVALID_FRAME;
        }
    }
  
    uint8_t checksum = ~sum + 1;            // checksum of TFI + DATA
    write(checksum);
    write(PN532_POSTAMBLE);
    
    endTransmission(true);

  } else {
    // Clear the serial buffer just in case
    flushTS(ts);

    bwriteTS(ts, (uint8_t)PN532_PREAMBLE);
    bwriteTS(ts, (uint8_t)PN532_STARTCODE1);
    bwriteTS(ts, PN532_STARTCODE2);

    uint8_t length = hlen + blen + 1;   // length of data field: TFI + DATA
    bwriteTS(ts, length);
    bwriteTS(ts, ~length + 1);         // checksum of length

    bwriteTS(ts, PN532_HOSTTOPN532);
    uint8_t sum = PN532_HOSTTOPN532;    // sum of TFI + DATA

    writeTS(ts, (uint8_t*)header, hlen);
    for (uint32_t i = 0; i < hlen; i++) {
      sum += header[i];
    }

    if (blen) {
      writeTS(ts, (uint8_t*)body, blen);
      for (uint32_t i = 0; i < blen; i++) {
        sum += body[i];
      }
    }

    uint8_t checksum = ~sum + 1;            // checksum of TFI + DATA
    bwriteTS(ts, checksum);

    bwriteTS(ts, (uint8_t)PN532_POSTAMBLE);
  }
  return PN532_readAckFrame();
}

int16_t PN532_readResponse(uint8_t buf[], uint8_t len, uint16_t timeout = 50) {
  SETREGS

  if (mode) {
    uint16_t ctime = 0;
    uint8_t length = PN532_getResponseLength(buf, len, timeout);

    // [RDY] 00 00 FF LEN LCS (TFI PD0 ... PDn) DCS 00
    do {
        if (requestFrom(PN532_I2_ADDR, 6 + length + 2)) {
            if (read() & 1) {  // check first byte --- status
                break;         // PN532 is ready
            }
        }

        delay(1);
        ctime++;
        if ((0 != timeout) && (ctime > timeout)) {
            return -1;
        }
    } while (1); 
    
    if (0x00 != read()  ||       // PREAMBLE
        0x00 != read()  ||       // STARTCODE1
        0xFF != read()           // STARTCODE2
        ) {
        return PN532_INVALID_FRAME;
    }
    
    length = read();

    if (0 != (uint8_t)(length + read())) {   // checksum of length
        return PN532_INVALID_FRAME;
    }
    
    uint8_t cmd = Pn532.command + 1;               // response command
    if (PN532_PN532TOHOST != read() || (cmd) != read()) {
      return PN532_INVALID_FRAME;
    }
    
    length -= 2;
    if (length > len) {
      return PN532_NO_SPACE;  // not enough space
    }
    
    uint8_t sum = PN532_PN532TOHOST + cmd;
    for (uint8_t i = 0; i < length; i++) {
      buf[i] = read();
      sum += buf[i];
    }
    
    uint8_t checksum = read();
    if (0 != (uint8_t)(sum + checksum)) {
      return PN532_INVALID_FRAME;
    }
    read();         // POSTAMBLE
    
    return length;
  } else {
    uint8_t tmp[3];

    // Read preamble and start code
    if (PN532_receive(tmp, 3, timeout)<=0) {
      return PN532_TIMEOUT;
    }
    if (0 != tmp[0] || 0!= tmp[1] || 0xFF != tmp[2]) {
      return PN532_INVALID_FRAME;
    }
   
    // Get length of data to be received
    uint8_t length[2];
  
    if (PN532_receive(length, 2, timeout) <= 0) {
      return PN532_TIMEOUT;
    }

    // Validate that frame is valid
    if (0 != (uint8_t)(length[0] + length[1])) {
      return PN532_INVALID_FRAME;
    }
    length[0] -= 2;
    if (length[0] > len) {   // If this happens, then Pn532.packetbuffer is not large enough
      return PN532_NO_SPACE;
    }

    // Get the command byte
    uint8_t cmd = Pn532.command + 1;
    if (PN532_receive(tmp, 2, timeout) <= 0) { // Time out while receiving
      return PN532_TIMEOUT;
    }
    if (PN532_PN532TOHOST != tmp[0] || cmd != tmp[1]) { // Invalid frame received
      return PN532_INVALID_FRAME;
    }

    if (PN532_receive(buf, length[0], timeout) != length[0]) { // Timed out
      return PN532_TIMEOUT;
    }

    uint8_t sum = PN532_PN532TOHOST + cmd;
    for (uint32_t i=0; i<length[0]; i++) {
      sum += buf[i];
    }

    // Checksum & postamble
    if (PN532_receive(tmp, 2, timeout) <= 0) {
      return PN532_TIMEOUT;
    }
    if (0 != (uint8_t)(sum + tmp[0]) || 0 != tmp[1]) { // Checksum fail, so frame must be invalid
      return PN532_INVALID_FRAME;
    }
    return length[0];
  }
}

const uint8_t PN532_ACK[6] PROGMEM = {0,0,0xff,0,0xff,0};

int8_t PN532_readAckFrame() {
  SETREGS

  uint8_t ackBuf[sizeof(PN532_ACK)];

  if (mode) {    
    uint16_t time = 0;
    do {
      if (requestFrom(PN532_I2_ADDR,  sizeof(PN532_ACK) + 1)) {
        if (read() & 1) {  // check first byte --- status
          break;         // PN532 is ready
        }
      }

      delay(1);
      time++;
      if (time > PN532_ACK_WAIT_TIME) {
        return PN532_TIMEOUT;
      }
    } while (1); 
    
    for (uint8_t i = 0; i < sizeof(PN532_ACK); i++) {
      ackBuf[i] = read();
    }
    
  } else {
    if (PN532_receive(ackBuf, sizeof(PN532_ACK), PN532_ACK_WAIT_TIME) <= 0) {
      return PN532_TIMEOUT;
    }
  }
  if (memcmp(&ackBuf, &PN532_ACK[mt->execution_offset], sizeof(PN532_ACK))) {
    return PN532_INVALID_ACK;
  }
  return 0;
}

uint32_t PN532_getFirmwareVersion() {
  SETREGS
  int16_t status = 0;

  uint32_t response = 0;

  Pn532.packetbuffer[0] = PN532_COMMAND_GETFIRMWAREVERSION;

  status = PN532_writeCommand(Pn532.packetbuffer, 1, 0, 0);
  if (status > 0) {
    return 0;
  }

  // Read data packet
  status = PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer), 50);
  if (0 > status) {
    return 0;
  }

  response = Pn532.packetbuffer[0];
  response <<= 8;
  response |= Pn532.packetbuffer[1];
  response <<= 8;
  response |= Pn532.packetbuffer[2];
  response <<= 8;
  response |= Pn532.packetbuffer[3];

  return response;
}

void PN532_wakeup() {
  SETREGS
  if (mode) return;
  uint8_t wakeup[5];
  wakeup[0] = 0x55;
  wakeup[1] = 0x55;
  wakeup[2] = 0;
  wakeup[3] = 0;
  wakeup[4] = 0;
  writeTS(ts, wakeup, sizeof(wakeup));
  // Flush the serial buffer just in case there's garbage in there
  flushTS(ts);
}

bool PN532_setPassiveActivationRetries(uint8_t maxRetries) {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_RFCONFIGURATION;
  Pn532.packetbuffer[1] = 5;    // Config item 5 (MaxRetries)
  Pn532.packetbuffer[2] = 0xFF; // MxRtyATR (default = 0xFF)
  Pn532.packetbuffer[3] = 0x01; // MxRtyPSL (default = 0x01)
  Pn532.packetbuffer[4] = maxRetries;
  if (PN532_writeCommand(Pn532.packetbuffer, 5)) {
    return 0; // no ACK
  }
  return (0 < PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)));
}


bool PN532_SAMConfig() {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_SAMCONFIGURATION;
  Pn532.packetbuffer[1] = 0x01; // normal mode
  Pn532.packetbuffer[2] = 0x14; // timeout 50ms * 20 = 1 second
  Pn532.packetbuffer[3] = 0x00; // we don't need the external IRQ pin
  if (PN532_writeCommand(Pn532.packetbuffer, 4)) {
    return false;
  }
  return (0 < PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)));
}


bool PN532_readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout = 50) {
  SETREGS

  Pn532.packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  Pn532.packetbuffer[1] = 1;  // max 1 cards at once (we can set this to 2 later)
  Pn532.packetbuffer[2] = cardbaudrate;
  int8_t status = PN532_writeCommand(Pn532.packetbuffer, 3);
  if (status) {
    return 0x0;  // command failed
  }
  // read data packet
  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer), timeout) < 0) {
    return 0x0;
  }

  // Check some basic stuff
  //   b0              Tags Found
  //   b1              Tag Number (only one used in this example)
  //   b2..3           SENS_RES
  //   b4              SEL_RES
  //   b5              NFCID Length
  //   b6..NFCIDLen    NFCID
  

  if (Pn532.packetbuffer[0] != 1) {
    return 0;
  }

  Pn532.atqa = Pn532.packetbuffer[2];
  Pn532.atqa <<= 8;
  Pn532.atqa |= Pn532.packetbuffer[3];

  // Card appears to be Mifare Classic
  *uidLength = Pn532.packetbuffer[5];

  for (uint32_t i = 0; i < Pn532.packetbuffer[5]; i++) {
    uid[i] = Pn532.packetbuffer[6 + i];
  }
  return 1;
}



#ifdef USE_PN532_DATA_FUNCTION

const struct {
  uint8_t version[6];
  uint8_t confPage;
} NTAG[] PROGMEM ={ 
  {.version={0x04, 0x02, 0x01, 0x00, 0x0f, 0x03},.confPage=0x29}, // NTAG213
  {.version={0x04, 0x02, 0x01, 0x00, 0x11, 0x03},.confPage=0x83}, // NTAG215
  {.version={0x04, 0x02, 0x01, 0x00, 0x13, 0x03},.confPage=0xe3}, // NTAG216 
  {.version={0x04, 0x05, 0x02, 0x02, 0x13, 0x03},.confPage=0xe3}, // NT3H2111
  {.version={0x04, 0x05, 0x02, 0x02, 0x15, 0x03},.confPage=0xe3}, // NT3H2211
};

#define NTAG_CNT (sizeof(NTAG)/7) // num records in NTAG array

MODULE_PART void PN532_inRelease();
void PN532_inRelease() {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INRELEASE;
  Pn532.packetbuffer[1] = 1;
  if (PN532_writeCommand(Pn532.packetbuffer, 2)) {
    return;
  }
  PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));
}

MODULE_PART uint8_t PN532_mifareclassic_AuthenticateBlock(uint8_t *uid, uint8_t uidLen, uint32_t blockNumber, uint8_t keyNumber, uint8_t *keyData);
uint8_t PN532_mifareclassic_AuthenticateBlock(uint8_t *uid, uint8_t uidLen, uint32_t blockNumber, uint8_t keyNumber, uint8_t *keyData) {
  SETREGS
  uint8_t i;
  uint8_t _key[6];
  uint8_t _uid[7];
  uint8_t _uidLen;

  // Hang on to the key and uid data
  memcpy_P(&_key, keyData, 6);
  memcpy_P(&_uid, uid, uidLen);
  _uidLen = uidLen;

  // Prepare the authentication command //
  Pn532.packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;   /* Data Exchange Header */
  Pn532.packetbuffer[1] = 1;                              /* Max card numbers */
  Pn532.packetbuffer[2] = (keyNumber) ? MIFARE_CMD_AUTH_B : MIFARE_CMD_AUTH_A;
  Pn532.packetbuffer[3] = blockNumber;                    /* Block Number (1K = 0..63, 4K = 0..255 */
  memcpy_P(&Pn532.packetbuffer[4], &_key, 6);
  for (i = 0; i < _uidLen; i++) {
    Pn532.packetbuffer[10 + i] = _uid[i];              /* 4 bytes card ID */
  }

  if (PN532_writeCommand(Pn532.packetbuffer, 10 + _uidLen)) { return 0; }

  // Read the response packet
  PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));

  // Check if the response is valid and we are authenticated???
  // for an auth success it should be bytes 5-7: 0xD5 0x41 0x00
  // Mifare auth error is technically byte 7: 0x14 but anything other and 0x00 is not good
  if (Pn532.packetbuffer[0] != 0x00) {
    // Authentification failed
    return 0;
  }
  return 1;
}

MODULE_PART uint8_t PN532_mifareclassic_ReadDataBlock(uint8_t blockNumber, uint8_t *data);
uint8_t PN532_mifareclassic_ReadDataBlock(uint8_t blockNumber, uint8_t *data) {
  SETREGS
  // Prepare the command 
  Pn532.packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  Pn532.packetbuffer[1] = 1;                      // Card number
  Pn532.packetbuffer[2] = MIFARE_CMD_READ;        // Mifare Read command = 0x30
  Pn532.packetbuffer[3] = blockNumber;            // Block Number (0..63 for 1K, 0..255 for 4K)

  // Send the command 
  if (PN532_writeCommand(Pn532.packetbuffer, 4)) {
    return 0;
  }

  // Read the response packet
  PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer));

  // If byte 8 isn't 0x00 we probably have an error
  if (Pn532.packetbuffer[0] != 0x00) {
    return 0;
  }

  // Copy the 16 data bytes to the output buffer
  // Block content starts at byte 9 of a valid response
  memcpy_P (data, &Pn532.packetbuffer[1], 16);
  return 1;
}

MODULE_PART uint8_t PN532_mifareclassic_WriteDataBlock(uint8_t blockNumber, uint8_t *data);
uint8_t PN532_mifareclassic_WriteDataBlock(uint8_t blockNumber, uint8_t *data) {
  SETREGS
  // Prepare the first command
  Pn532.packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  Pn532.packetbuffer[1] = 1;                      // Card number
  Pn532.packetbuffer[2] = MIFARE_CMD_WRITE;       // Mifare Write command = 0xA0
  Pn532.packetbuffer[3] = blockNumber;            // Block Number (0..63 for 1K, 0..255 for 4K)
  memcpy_P(&Pn532.packetbuffer[4], data, 16);       // Data Payload

  // Send the command
  if (PN532_writeCommand(Pn532.packetbuffer, 20)) {
    return 0;
  }

  // Read the response packet
  return (0 < PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)));
}

MODULE_PART uint8_t PN532_ntag21x_probe();
uint8_t PN532_ntag21x_probe() {
  SETREGS
  uint8_t result=0;

  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG21X_CMD_GET_VERSION;       

  if (PN532_writeCommand(Pn532.packetbuffer, 2)) {
    return result;
  }

  if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer))<9){
    return result;
  }

  if (Pn532.packetbuffer[3] != 4) { // not NTAG type 
    return result;
  }

  for (uint8_t i = 0; i < NTAG_CNT; i++) {
    if (0 == memcmp_P(&Pn532.packetbuffer[3], &NTAG[(tmod__udivsi3(mt->execution_offset , 7)) + i].version[0],6)) {
      memcpy_P(&result,&NTAG[(tmod__udivsi3(mt->execution_offset , 7)) + i].confPage,sizeof(result));
    }
  }
  return result; //Return configuration page address
}

MODULE_PART bool PN532_ntag21x_auth();
bool PN532_ntag21x_auth() {
  SETREGS
  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG21X_CMD_PWD_AUTH;
  memcpy_P(&Pn532.packetbuffer[2],&Pn532.pwd_auth,4);

  if (PN532_writeCommand(Pn532.packetbuffer, 6)) {
    return false;
  }
  if ((PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)))<3){
    return false;
  }
  if (Pn532.packetbuffer[0]) { //PN532 return any error
    return false;
  }
  return memcmp(&Pn532.packetbuffer[1],&Pn532.pwd_pack,2)==0;
}

MODULE_PART bool PN532_ntag2xx_read16(const uint8_t page, char *out);
bool PN532_ntag2xx_read16(const uint8_t page, char *out) {
  SETREGS

  Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
  Pn532.packetbuffer[1] = NTAG2XX_CMD_READ;
  Pn532.packetbuffer[2] = page;
  if (PN532_writeCommand(Pn532.packetbuffer, 3)) {
    return false;
  }
  if ((PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)))<17){
    return false;
  }
  if (Pn532.packetbuffer[0]!=0) {
    return false;
  }
  memcpy_P(out,&Pn532.packetbuffer[1],16);
  return true;  
}

MODULE_PART bool PN532_ntag2xx_write4(uint8_t page, char *in);
bool PN532_ntag2xx_write4(uint8_t page, char *in) {
  SETREGS
    Pn532.packetbuffer[0] = PN532_COMMAND_INCOMMUNICATETHRU;
    Pn532.packetbuffer[1] = NTAG2XX_CMD_WRITE;
    Pn532.packetbuffer[2] = page; // first page
    memcpy_P(&Pn532.packetbuffer[3],in,4);

    if (PN532_writeCommand(Pn532.packetbuffer, 7)) {
      return false;
    }

    if (PN532_readResponse(Pn532.packetbuffer, sizeof(Pn532.packetbuffer)) < 1) {
      return false;
    }
    return true;
}

MODULE_PART bool PN532_ntag2xx_write16(uint8_t page, char *in);
bool PN532_ntag2xx_write16(uint8_t page, char *in) {
  SETREGS
  for (uint8_t i = 0; i < 4; i++) {
    if (!PN532_ntag2xx_write4(page +i, &in[i << 2])) {
      return false;
    }
  }
   return true;
}

MODULE_PART bool PN532_ntag21x_set_password(uint8_t confPage, bool unsetPasswd);
bool PN532_ntag21x_set_password(uint8_t confPage, bool unsetPasswd) {
  SETREGS
  char card_datas[16];

  if (PN532_ntag2xx_read16(confPage, card_datas)) {
    if (unsetPasswd) {
      card_datas[3]=0xFF;
      return PN532_ntag2xx_write4(confPage, card_datas);
    }
    card_datas[3]=0; // Set AUTH0 for protect all pages
    card_datas[4] |= 0x80; // Set PROT flag for read and write access is protected by the password verification
    memcpy_P(&card_datas[8],&Pn532.pwd_auth_new, 4); // New password
    memcpy_P(&card_datas[12],&Pn532.pwd_pack_new, 2); // New password ack
    return PN532_ntag2xx_write16(confPage, card_datas);
  }
  return false;
}

MODULE_PART void PN532_Erase();
void PN532_Erase() {
  SETREGS
  memset(Pn532.newdata,0,sizeof(Pn532.newdata));
  Pn532.function = 1; // Block 1 of next card/tag will be reset to 0x00...
  AddLog(LOG_LEVEL_INFO, PSTR("data block 1 (4-7 for NTAG) will be erased"));
}

MODULE_PART void PN532_Write();
void PN532_Write() {
  SETREGS
  memset(Pn532.newdata, 0, sizeof(Pn532.newdata));
  strncpy((char *)Pn532.newdata, XdrvMailbox->data, sizeof(Pn532.newdata));
  //if (strlen(argument) > 16) argument[16]=0;
  Pn532.function = 2;
  AddLog(LOG_LEVEL_INFO, PSTR("data block 1 (4-7 for NTAG) will be set to '%s'"), Pn532.newdata);
}
MODULE_PART void PN532_Auth();
void PN532_Auth() {
  SETREGS
  /*
   if (ArgC() > 1) {
      Pn532.pwd_auth=strtoul(ArgV(argument,2),nullptr,0);
    }
    if (ArgC() > 2) {
      Pn532.pwd_pack=strtoul(ArgV(argument,3),nullptr,0);
    }
    Settings->pn532_password=Pn532.pwd_auth;
    Settings->pn532_pack=Pn532.pwd_pack;
    */
}
MODULE_PART void PN532_Set_PWD();
void PN532_Set_PWD() {
  SETREGS
  /*
    Pn532.pwd_auth_new=Pn532.pwd_auth;
    Pn532.pwd_pack_new=Pn532.pwd_pack;
    if (ArgC() > 1) {
      Pn532.pwd_auth_new=strtoul(ArgV(argument,2),nullptr,0);
    }
    if (ArgC() > 2) {
      Pn532.pwd_pack_new=strtoul(ArgV(argument,3),nullptr,0);
    }
    */
  AddLog(LOG_LEVEL_INFO, PSTR("will be protected"));
  Pn532.function = 3;
}
MODULE_PART void PN532_Unset_PWD();
void PN532_Unset_PWD() {
  SETREGS
  AddLog(LOG_LEVEL_INFO, PSTR("will be unprotected"));
  Pn532.function = 4;

}
MODULE_PART void PN532_Cancel();
void PN532_Cancel() {
  SETREGS
  AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Job canceled"));
  Pn532.function = 0;
}

const char PN532Commands[] PROGMEM = "pn532|Erase|Write|Auth|Set_PWD|Unset_PWD|Cancel";
VTABLE(PN532Command) = {&PN532_Erase, &PN532_Write, &PN532_Auth, &PN532_Set_PWD, &PN532_Unset_PWD, &PN532_Cancel};

#endif  // USE_PN532_DATA_FUNCTION


void PN532_ScanForTag() {
  SETREGS
  //uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uid[7];
  memset(uid, 0, sizeof(uid));

  uint8_t uid_len = 0;
  
  if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uid_len)) {
    ToHex_P((unsigned char*)uid, uid_len, Pn532.uids, sizeof(Pn532.uids));

#ifdef USE_PN532_DATA_FUNCTION
    bool success = false;
    char card_datas[17];
    memset(card_datas, 0, sizeof(card_datas));

    enum {NOPWD, PWD_NONE, PWD_OK, PWD_NOK} str_pwd = NOPWD;

    if (Pn532.atqa == 0x44) {
      uint8_t confPage = 0;
      uint8_t nuid[7];  // = { 0, 0, 0, 0, 0, 0, 0 };
      memset(nuid, 0, sizeof(nuid));
      uint8_t nuid_len = 0;
      if ((confPage = PN532_ntag21x_probe()) > 0) {
        // NTAG EV1 found
        str_pwd = PWD_NONE;
        if (!PN532_ntag2xx_read16(4, card_datas)) {
          if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, nuid, &nuid_len)) {
            if (memcmp(uid, nuid, sizeof(uid))==0) {
              if (PN532_ntag21x_auth()) {
                str_pwd=PWD_OK;
                if (Pn532.function == 3) { // new password
                  success = PN532_ntag21x_set_password(confPage, false);
                }
                if (Pn532.function == 4) { // clear password
                  success = PN532_ntag21x_set_password(confPage, true);
                }
              } else {
                str_pwd=PWD_NOK;
              }
              if (!PN532_ntag2xx_read16(4, card_datas)) card_datas[0]=0;
            }
          }
        } else {
          if (Pn532.function == 3) { // new password
            success = PN532_ntag21x_set_password(confPage, false);
          }
        }      
      } else {
        if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, nuid, &nuid_len)) {
          if (memcmp(uid, nuid, sizeof(uid))==0) {
            if (!PN532_ntag2xx_read16(4, card_datas)) card_datas[0]=0;
          }
        }
      }
      if ((Pn532.function == 1) || (Pn532.function == 2)) {
       success = PN532_ntag2xx_write16(4, (char *)Pn532.newdata);
       if (!PN532_ntag2xx_read16(4, card_datas)) card_datas[0]=0;
      }
    }
    else if (uid_len == 4) { // Lets try to read blocks 1 & 2 of the mifare classic card for more information
      uint8_t keyuniversal[6];
      memset(keyuniversal, 0xff, 6);
      // = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

      if (PN532_mifareclassic_AuthenticateBlock(uid, uid_len, 1, 1, keyuniversal)) {
        if ((Pn532.function == 1) || (Pn532.function == 2)) {
          success = PN532_mifareclassic_WriteDataBlock(1, Pn532.newdata);
        }
        if (PN532_mifareclassic_ReadDataBlock(1, (uint8_t *)card_datas)) {
          for (uint32_t i = 0; i < 16; i++) {
            if (!isprint(card_datas[i])) {
              // do not output non-printable characters to the console 
              card_datas[i] = 0;
            }
          }
        } else {
          card_datas[0] = 0;
        }
      } else {
        sprintf_P(card_datas, PSTR("AUTHFAIL"),0);
      }
    }
    switch (Pn532.function) {
      case 0x01:
        if (success) {
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Erase success"));
        } else {
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Erase fail - exiting erase mode"));
        }
        break;
      case 0x02:
        if (success) {
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Data write successful"));
        } else{
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Write failed - exiting set mode"));
        }
        break;
      case 0x03:
        if (success) {
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Set password successful"));
        } else{
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Set password failed - exiting set mode"));
        }
        break;
      case 0x04:
        if (success) {
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Unset password successful"));
        } else{
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Unset password failed - exiting set mode"));
        }
        break;
      default:
        break;
    }

    Pn532.function = 0;
    card_datas[16] = 0;
    ResponseTime_P(PSTR(",\"PN532\":{\"UID\":\"%s\",\"Data\":\"%s\""), Pn532.uids, card_datas);
    if (str_pwd == PWD_NONE) {
      ResponseAppend_P(PSTR(",\"Auth\":\"None\""));
    } else
    if (str_pwd == PWD_OK) {
      ResponseAppend_P(PSTR(",\"Auth\":\"Ok\""));
    } else
    if (str_pwd == PWD_NOK) {
      ResponseAppend_P(PSTR(",\"Auth\":\"NOk\""));
    }
    ResponseAppend_P(PSTR("}}"));
    PN532_inRelease(); 
#else
    ResponseTime_P(PSTR(",\"PN532\":{\"UID\":\"%s\"}}"), Pn532.uids);
#endif  // USE_PN532_DATA_FUNCTION
    MqttPublishTeleSensor();
    Pn532.scantimer = 7; // Ignore tags found for two seconds
  } else {
    Pn532.uids[0] = '-';
    Pn532.uids[1] = 0;
  }
}

void PN532_Show() {
  SETREGS
  WSContentSend_PD(PSTR("{s}PN532 UID{m}%s{e}"), Pn532.uids);
}

void PN532_Deinit() {
  SETREGS

  if (mode) {
    I2cResetActive(PN532_I2_ADDR, 0);
  } else {
    if (ts) deleteTS(ts);
    ts = nullptr;
  }
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  SETREGS
  bool result = false;

  switch (sel) {
      case FUNC_INIT:
        result = PN532_Init();
        break;
      case FUNC_EVERY_250_MSECOND:
        if (Pn532.scantimer > 0) {
          Pn532.scantimer--;
        } else {
          PN532_ScanForTag();
        }
        break;
      case FUNC_WEB_SENSOR:
        PN532_Show();
        break;
#ifdef USE_PN532_DATA_FUNCTION
      case FUNC_COMMAND:
        result = DecodeCommand(PN532Commands, PN532Command);
        if (result == false) {
          AddLog(LOG_LEVEL_INFO, PSTR("NFC: PN532 - Next scanned tag"));
        } else {
          ResponseCmndDone();
        }
        break;
#endif  // USE_PN532_DATA_FUNCTION
      case FUNC_DEINIT:
        PN532_Deinit();
        break;
    
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_PN532_HSU_MOD
