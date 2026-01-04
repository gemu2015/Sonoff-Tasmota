/*
  xdrv_86_esp32_m5epd47.ino - ESP32 M5Stack M5Paper EPD47 support for Tasmota

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

#ifdef ESP32
#ifdef USE_M5EPD47
/*********************************************************************************************\
 * M5Stack Epaper 47 support
 *
 * Module 19


  {"NAME":"M5Stack_EPD47","GPIO":[7616,1,1,1,6720,1,1,1,704,672,736,1,1,1,1,1,0,641,609,1,0,640,1,0,0,0,0,0,608,1,1,1,0,0,0,1],"FLAG":0,"BASE":1}


internal i2c use port 2
external use port 1

internal I2C devices
 FM24C02 = 0x50
 BM8563 RTC = 0x51
 STH3X = 0x44
 GT911 touch = 0x5d


\*********************************************************************************************/

#define XDRV_97        97

#include "M5EPD.h"
#include "BM8563.h"

BM8563 *Get_BM8563(void);

struct M5EPD_globs {
  BM8563 *Rtc;
  M5EPD m5epd;
  bool ready;
  int32_t shutdownseconds;
  uint8_t wakeup_hour;
  uint8_t wakeup_minute;
  uint8_t shutdowndelay;
} M5EPD_globs;

/*********************************************************************************************/

void M5EPDDoShutdown(void) {
  if (!M5EPD_globs.Rtc) return;

  SettingsSaveAll();
  RtcSettingsSave();
  M5EPD_globs.Rtc->clearIRQ();
  if (M5EPD_globs.shutdownseconds > 0) {
    M5EPD_globs.Rtc->SetAlarmIRQ(M5EPD_globs.shutdownseconds);
  } else {
    RTC_TimeTypeDef wut;
    wut.Hours = M5EPD_globs.wakeup_hour;
    wut.Minutes = M5EPD_globs.wakeup_minute;
    M5EPD_globs.Rtc->SetAlarmIRQ(wut);
  }
  delay(10);
  M5EPD_globs.m5epd.shutdown();
}

/*********************************************************************************************/


void M5EPDRtcInit(void) {
  M5EPD_globs.Rtc = Get_BM8563();
}

void M5EPDModuleInit(void) {
  if (!TasmotaGlobal.i2c_enabled[1]) {
    //I2c2Begin(21, 22, 400000);
    //I2cBegin(21, 22, 1);
  }
  
  //M5EPD_globs.m5epd.begin();

/*
#define M5EPD_MAIN_PWR_PIN 2
#define M5EPD_CS_PIN 15
#define M5EPD_SCK_PIN 14
#define M5EPD_MOSI_PIN 12
#define M5EPD_BUSY_PIN 27
#define M5EPD_MISO_PIN 13
#define M5EPD_EXT_PWR_EN_PIN 5
#define M5EPD_EPD_PWR_EN_PIN 23
#define M5EPD_KEY_RIGHT_PIN 39
#define M5EPD_KEY_PUSH_PIN 38
#define M5EPD_KEY_LEFT_PIN 37
#define M5EPD_BAT_VOL_PIN 35
#define M5EPD_PORTC_W_PIN 19
#define M5EPD_PORTC_Y_PIN 18
#define M5EPD_PORTB_W_PIN 33
#define M5EPD_PORTB_Y_PIN 26
#define M5EPD_PORTA_W_PIN 32
#define M5EPD_PORTA_Y_PIN 25

#define M5EPD_CS_SD_PIN 4
*/

#define enableEXTPower() digitalWrite(M5EPD_EXT_PWR_EN_PIN, 1)
#define enableEPDPower() digitalWrite(M5EPD_EPD_PWR_EN_PIN, 1)
#define enableMainPower() digitalWrite(M5EPD_MAIN_PWR_PIN, 1)

  pinMode(M5EPD_MAIN_PWR_PIN, OUTPUT);
  enableMainPower();

  pinMode(M5EPD_CS_PIN, OUTPUT);
  digitalWrite(M5EPD_CS_PIN, 1);

  //pinMode(M5EPD_CS_SD_PIN, OUTPUT);
  //digitalWrite(M5EPD_CS_SD_PIN, 1);

  pinMode(M5EPD_EXT_PWR_EN_PIN, OUTPUT);
  pinMode(M5EPD_EPD_PWR_EN_PIN, OUTPUT);

  pinMode(M5EPD_KEY_RIGHT_PIN, INPUT);
  pinMode(M5EPD_KEY_PUSH_PIN, INPUT);
  pinMode(M5EPD_KEY_LEFT_PIN, INPUT);
  delay(100);

  enableEPDPower();
 // enableEXTPower();
  AddLog(LOG_LEVEL_INFO, PSTR("DRV: M5 E-Paper 4.7"));

  // must reinitialyze SD card
  //SPI.end();
  //UfsCheckSDCardInit();

  M5EPD_globs.ready = true;
}

void M5EPDEverySecond(void) {
  if (M5EPD_globs.ready) {
    if (M5EPD_globs.shutdowndelay) {
      M5EPD_globs.shutdowndelay--;
      if (!M5EPD_globs.shutdowndelay) {
        M5EPDDoShutdown();
      }
    }
  }
}

#define BATTERY "Battery"

#define BAT_ADC_CHANNEL 35

void M5EPDShow(uint32_t json) {
  if (!M5EPD_globs.ready) return;
  //float bvolt = (float)M5EPD_globs.m5epd.getBatteryVoltage()/1000.0;
  float bvolt = (float)analogReadMilliVolts(BAT_ADC_CHANNEL)/500.0;

  if (json) {
    ResponseAppend_P(PSTR(",\"M5EPD\":{\"BV\":%3_f}"), &bvolt);
  } else {
    WSContentSend_Voltage(BATTERY, bvolt);
  }
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

const char kM5EPDCommands[] PROGMEM = "M5EPD|"
  "Shutdown|Init";

void (* const M5EPDCommand[])(void) PROGMEM = {
  &CmndM5EPDShutdown,&M5EPDMInit};

void M5EPDMInit(void) {
  enableEXTPower();
  ResponseCmndDone();
}

void CmndM5EPDShutdown(void) {
  char *mp = strchr(XdrvMailbox.data, ':');
  if (mp) {
    M5EPD_globs.wakeup_hour = atoi(XdrvMailbox.data);
    M5EPD_globs.wakeup_minute = atoi(mp+1);
    M5EPD_globs.shutdownseconds = -1;
    M5EPD_globs.shutdowndelay = 10;
    char tbuff[16];
    sprintf(tbuff, "%02.2d" D_HOUR_MINUTE_SEPARATOR "%02.2d", M5EPD_globs.wakeup_hour, M5EPD_globs.wakeup_minute );
    ResponseCmndChar(tbuff);
  } else {
    if (XdrvMailbox.payload >= 30)  {
      M5EPD_globs.shutdownseconds = XdrvMailbox.payload;
      M5EPD_globs.shutdowndelay = 10;
    }
    ResponseCmndNumber(XdrvMailbox.payload);
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv97(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_EVERY_SECOND:
      //M5EPDEverySecond();
      break;
    case FUNC_JSON_APPEND:
      M5EPDShow(1);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      M5EPDShow(0);
      break;
#endif
    case FUNC_COMMAND:
      result = DecodeCommand(kM5EPDCommands, M5EPDCommand);
      break;
    case FUNC_INIT:
      //M5EPDRtcInit();
      //break;
    case FUNC_MODULE_INIT:
      M5EPDModuleInit();
      break;
  }
  return result;
}

#endif  // USE_M5STACK_M5EPD
#endif  // ESP32
