/*
  xsns_99_vl53l0x.ino - VL53L0X

  Copyright (C) 2018  Theo Arends and Gerhard Mutz

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

#ifdef USE_I2C
#ifdef USE_VL53L0X
/*********************************************************************************************\
 * VL53L1X
 *
 * Source:
 *
 * I2C Address: 0x52
\*********************************************************************************************/

#include <Wire.h>
#include "VL53L0X.h"
VL53L0X sensor;

uint8_t vl53l0x_ready = 0;
uint16_t distance;
uint16_t Vl53l0_buffer[5];
uint8_t Vl53l0_index;
uint8_t Vl53l0_cnt;

/********************************************************************************************/

void Vl53l0Detect()
{
  if (vl53l0x_ready) {
    AddLog_P(LOG_LEVEL_DEBUG, PSTR("VL53L1X is ready"));
    return;
  }

  sensor.init();
  sensor.setTimeout(500);

  // Start continuous back-to-back mode (take readings as
  // fast as possible).  To use continuous timed mode
  // instead, provide a desired inter-measurement period in
  // ms (e.g. sensor.startContinuous(100)).
  sensor.startContinuous();
  vl53l0x_ready = 1;

  Vl53l0_index=0;
  Vl53l0_cnt=0;

  snprintf_P(log_data, sizeof(log_data), S_LOG_I2C_FOUND_AT, "VL53L0X", sensor.getAddress());
  AddLog(LOG_LEVEL_DEBUG);
}

#define D_UNIT_MILLIMETER "mm"

#ifdef USE_WEBSERVER
const char HTTP_SNS_VL53L0X[] PROGMEM =
 "%s{s}VL53L0X " D_DISTANCE "{m}%d" D_UNIT_MILLIMETER "{e}"; // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
#endif  // USE_WEBSERVER

void Vl53l0Every_50MSecond() {
  uint16_t tbuff[5],tmp;
  uint8_t flag;

  Vl53l0_cnt++;
  if (Vl53l0_cnt<4) return;
  Vl53l0_cnt=0;

  // every 200 ms
  uint16_t dist = sensor.readRangeContinuousMillimeters();
  if (dist==0 || dist>2000) {
    dist=9999;
  }
  // store in ring buffer
  Vl53l0_buffer[Vl53l0_index]=dist;
  Vl53l0_index++;
  if (Vl53l0_index>=5) Vl53l0_index=0;

  // sort list and take median
  memmove(tbuff,Vl53l0_buffer,sizeof(tbuff));
  for (byte ocnt=0; ocnt<5; ocnt++) {
    flag=0;
    for (byte count=0; count<4; count++) {
      if (tbuff[count]>tbuff[count+1]) {
        tmp=tbuff[count];
        tbuff[count]=tbuff[count+1];
        tbuff[count+1]=tmp;
        flag=1;
      }
    }
    if (!flag) break;
  }
  distance=tbuff[2];
}

void Vl53l0Show(boolean json)
{
  if (!vl53l0x_ready) {
    return;
  }

  if (json) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"VL53L0X\":{\"" D_JSON_DISTANCE "\":%d}"), mqtt_data, distance);
#ifdef USE_WEBSERVER
  } else {
    snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_VL53L0X, mqtt_data, distance);
#endif
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XSNS_99

boolean Xsns99(byte function)
{
  boolean result = false;

  if (i2c_flg) {
    switch (function) {
      case FUNC_INIT:
        Vl53l0Detect();
        break;
      case FUNC_EVERY_50_MSECOND:
        Vl53l0Every_50MSecond();
        break;
      case FUNC_JSON_APPEND:
        Vl53l0Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        Vl53l0Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_VL53L0X
#endif  // USE_I2C
