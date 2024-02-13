/*
  xsns_22_sr04.ino - SR04 ultrasonic sensor support for Tasmota

  Copyright (C) 2023  Gerhard Mutz

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

#ifdef USE_SR04T_MOD

#include "module.h"
#include "module_defines.h"

#define SR04TV3_REV  1<<16|2

MODULE_DESCRIPTOR("SR04TV3", MODULE_TYPE_SENSOR, SR04TV3_REV,"RECPIN",3,"",0,"",0,"",0)
// all functions must be declared MUDULE_PART
MODULE_PART int32_t Sr04T_Detect();
MODULE_PART void Sr04T_Show(bool json);
MODULE_PART void Sr04T_Read();
MODULE_PART void Sr04T_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END

const char started[] PROGMEM = "SR04TV3 inizialized with RX pin %d";
const char HTTP_DIST[] PROGMEM = "{s}SR04T distance{m}%s cm{e}";
const char JSON_DIST[] PROGMEM = ",\"SR04T\":{\"DIST\":%s}";

/*********************************************************************************************/
typedef struct {
  uint8_t recpin;
  uint8_t ready;
  float distance;
  TasmotaSerial *ts;
  uint8_t sbuff[4];
} MODULE_MEMORY;

#define ts mem->ts
#define recpin mem->recpin
#define ready mem->ready
#define distance mem->distance
#define sbuff mem->sbuff

int32_t Sr04T_Detect() {
  ALLOCMEM

  ready = false;
  recpin = mp->ms[0].value;

  ts = NewTS(recpin, -1);
 
  if (ts) {
    if (beginTS(ts, 9600)) {
      AddLog(LOG_LEVEL_INFO, GSTR(started), recpin);
      initialized = true;
      ready = true;
      return 0;
    }
  }
  Sr04T_Deinit();
  return -1;
}

void Sr04T_Read() {
  SETREGS
  if (!ready) {
    return;
  }
  int16_t wval = 0; 
  while (availTS(ts)) {
    for (uint16_t cnt = 0; cnt < 3; cnt++) {
      sbuff[cnt] = sbuff[cnt + 1];
    }
    sbuff[3] = readbTS(ts);
    
    if (sbuff[0] == 0xff) {
      uint8_t sum = sbuff[0] + sbuff[1] + sbuff[2];
      if (sum == sbuff[3]) {
        wval = sbuff[1] << 8 | sbuff[2];
      }
    }
  }
  distance = fscale(wval, (float)0.1, (float)0);
/*
  Product response FF 07 A1 A7
Where the check code SUM = A7 = (0x07 + 0xA1 + 0Xff) & 0x00ff 0x07 is the high data of the distance;
0xA1 is the lower data of the distance;
Distance value is 0x07A1; converted to decimal for 1953; unit: mm
*/

}

void Sr04T_Show(bool json) {
  SETREGS
  if (!ready) {
    return;
  }
  char tstr[16];
  ftostrfd(distance, 1, tstr);
  if (json) {
    ResponseAppend_P(GSTR(JSON_DIST), tstr);
  } else {
    WSContentSend_PD(GSTR(HTTP_DIST), tstr);
  }
}

void Sr04T_Deinit() {
  SETREGS
  if (ts) deleteTS(ts);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  int32_t result = false;

  switch (sel) {
    case FUNC_INIT:
      result = Sr04T_Detect();
      break;
    case FUNC_EVERY_SECOND:
      Sr04T_Read();
      result = true;
      break;
    case FUNC_JSON_APPEND:
      Sr04T_Show(1);
      break;
    case FUNC_WEB_SENSOR:
      Sr04T_Show(0);
      break;
    case FUNC_DEINIT:
      Sr04T_Deinit();
      break;
  }
  return result;
}

#endif  // USE_SR04T
