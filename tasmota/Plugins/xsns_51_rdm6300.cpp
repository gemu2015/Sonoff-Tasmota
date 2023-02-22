/*
  xsns_51_rdm6300.ino - Support for RDM630(0) 125kHz NFC Tag Reader on Tasmota

  Copyright (C) 2021  Gerhard Mutz and Theo Arends

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
#include "tasmota_options.h"


#ifdef USE_RDM6300_MOD

#include "module.h"
#include "module_defines.h"


/*********************************************************************************************\
 * Seeed studio Grove / RDM630 / RDM6300 125kHz rfid reader
 *
 * Expected 14 byte data:
 *  0  1  2  3  4  5  6  7  8  9 10 11 12 13
 * Hd ------ ASCII characters ----- Chksm Tl
 * 02 31 34 30 30 38 45 43 37 39 33 43 45 03 = 02-14-008EC793-CE-03
 *    Versn --------- Tag ---------
\*********************************************************************************************/

#define XSNS_51            51

#define RDM6300_DEFAULT_REC_PIN 5
#define RDM6300_BAUDRATE   9600
#define RDM_TIMEOUT        100
#define RDM6300_BLOCK      2 * 10   // 2 seconds block time


#define RDM6300_REV 1<<16

MODULE_DESCRIPTOR("RDM6300",MODULE_TYPE_SENSOR,RDM6300_REV,"REC",RDM6300_DEFAULT_REC_PIN,"",0,"",0,"",0)

// all functions must be declared MUDULE_PART
MODULE_PART uint8_t MOD_FUNC(RDM6300_HexNibble, char chr);
MODULE_PART void MOD_FUNC(RDM6300_HexStringToArray, uint8_t array[], uint8_t len, char buffer[]);
MODULE_PART int32_t MOD_FUNC(RDM6300_Init);
MODULE_PART void MOD_FUNC(RDM6300_Deinit);
MODULE_PART void MOD_FUNC(RDM6300_ScanForTag);
void MOD_FUNC(RDM6300_Show);
MODULE_PART int32_t MOD_FUNC(mod_func_execute, uint32_t sel);

MODULE_END

DPSTR(started,"mp3 inizialized with TRX pin %d");

typedef struct {
  uint32_t uid;
  uint8_t block_time;
} Rdm;

typedef struct {
  Rdm rdm;
  void *ts;
  int8_t recpin;
  uint8_t ready;
} MODULE_MEMORY;

#define rdm mem->rdm
#define ts mem->ts
#define recpin mem->recpin
#define ready mem->ready

/********************************************************************************************/

uint8_t MOD_FUNC(RDM6300_HexNibble, char chr) {
  SETREGS
  uint8_t rVal = 0;
  if (isdigit(chr)) { rVal = chr - '0'; }
  else if (chr >= 'A' && chr <= 'F') { rVal = chr + 10 - 'A'; }
  else if (chr >= 'a' && chr <= 'f') { rVal = chr + 10 - 'a'; }
  return rVal;
}

// Convert hex string to int array
void MOD_FUNC(RDM6300_HexStringToArray, uint8_t array[], uint8_t len, char buffer[]) {
  SETREGS
  char *cp = buffer;
  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = CALL_MOD_FUNC(RDM6300_HexNibble, *cp++) << 4;
    array[i] = val | CALL_MOD_FUNC(RDM6300_HexNibble, *cp++);
  }
}

/********************************************************************************************/

int32_t MOD_FUNC(RDM6300_Init) {
  ALLOCMEM

  ready = false;
  recpin = mp->ms[0].value;

  ts = NewTS(recpin, -1);

  if (ts) {
    if (beginTS(ts, RDM6300_BAUDRATE)) {
      //if (ts->hardwareSerial()) {
      //  ClaimSerial();
      //}
      initialized = true;
      ready = true;
      return 0;
    }
  }
  CALL_MOD_FUNC(Sr04T_Deinit);
  return -1;
}

void MOD_FUNC(RDM6300_ScanForTag) {
  SETREGS
  if (!ready) { return; }

  if (Rdm.block_time > 0) {
    Rdm.block_time--;
    while (availTS(ts)) {
      readbTS(ts);               // Flush serial buffer
    }
    return;
  }

  if (availTS(ts)) {

    char c = readbTS(ts);
    if (c != 2) { return; }                // Head marker

    // read rest of message 11 more bytes
    char rdm_buffer[14];
    uint8_t rdm_index = 0;

    rdm_buffer[rdm_index++] = c;

    uint32_t cmillis = millis();
    while (1) {
      if (availTS(ts)) {
        c = readbTS(ts);
        rdm_buffer[rdm_index++] = c;

        if (3 == c) { break; }             // Tail marker
        if (rdm_index > 14) { break; }     // Illegal message
      }
      if ((millis() - cmillis) > RDM_TIMEOUT) {
        return;                            // Timeout
      }
    }

    AddLogBuffer(LOG_LEVEL_DEBUG, (uint8_t*)rdm_buffer, sizeof(rdm_buffer));

    if (rdm_buffer[13] != 3) { return; }   // Tail marker

    Rdm.block_time = RDM6300_BLOCK;        // Block for 2 seconds

    uint8_t rdm_array[6];
    CALL_MOD_FUNC(RDM6300_HexStringToArray, rdm_array, sizeof(rdm_array), (char*)rdm_buffer +1);
    uint8_t accu = 0;
    for (uint32_t count = 0; count < 5; count++) {
      accu ^= rdm_array[count];            // Calc checksum,
    }
    if (accu != rdm_array[5]) { return; }  // Checksum error

    rdm_buffer[11] = '\0';
    uint32_t uid = strtoul(rdm_buffer +3, nullptr, 16);
    if (uid > 0) {                         // Ignore false positive all zeros
      Rdm.uid = uid;
      ResponseTime_P(PSTR(",\"RDM6300\":{\"UID\":\"%08X\"}}"), Rdm.uid);
      MqttPublishTeleSensor();
    }
  }
}


void MOD_FUNC(RDM6300_Show) {
  SETREGS
  if (!ready) { return; }
  WSContentSend_PD(PSTR("{s}RDM6300 UID{m}%08X {e}"), Rdm.uid);
}

void MOD_FUNC(RDM6300_Deinit) {
  SETREGS
  if (ts) deleteTS(ts);
  ts = nullptr;
  RETMEM
}
/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t MOD_FUNC(mod_func_execute, uint32_t sel) {
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      CALL_MOD_FUNC(RDM6300_Init);
      break;
    case FUNC_EVERY_100_MSECOND:
      CALL_MOD_FUNC(RDM6300_ScanForTag);
      break;
    case FUNC_WEB_SENSOR:
      CALL_MOD_FUNC(RDM6300_Show);
      break;
  }
  return result;
}

#endif  // USE_RDM6300
