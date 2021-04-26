/*
  xsns_84_tof10120.ino - TOF10120 sensor support for Tasmota

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

#ifdef USE_I2C
#ifdef USE_AXP192

#include <M5Stick_AXP192.h>
/*********************************************************************************************\
 * AXP192
 * Internal chip found in M5Stack devices, using `Wire1` internal I2C bus
 *
 * I2C Address: 0x68
 *
\*********************************************************************************************/

#define XSNS_88                     88
#define XI2C_59                     59

#define AXP192_ADDRESS            0x34

AXP192 Axp;

struct AXP192_ADC {
  float vbus_v;
  float batt_v;
  float vbus_c;
  float batt_c;
  float temp;
  uint8_t ready;
} axp192;

/********************************************************************************************/

void AXP192GetADC(void) {
  axp192.vbus_v = Axp.GetVBusVoltage();
  axp192.batt_v = Axp.GetBatVoltage();
  axp192.vbus_c = Axp.GetVinCurrent();
  axp192.batt_c = Axp.GetBatCurrent();
  axp192.temp = ConvertTemp(Axp.GetTempInAXP192());
}

const char HTTP_MPU6686[] PROGMEM =
 "{s}MPU6886 acc_x" "{m}%3_f G" "{e}"
 "{s}MPU6886 acc_y" "{m}%3_f G" "{e}"
 "{s}MPU6886 acc_z" "{m}%3_f G" "{e}"
 "{s}MPU6886 gyr_x" "{m}%i dps" "{e}"
 "{s}MPU6886 gyr_y" "{m}%i dps" "{e}"
 "{s}MPU6886 gyr_z" "{m}%i dps" "{e}"
 ;

 void AXP192_Show(uint32_t json) {
   if (json) {
     ResponseAppend_P(PSTR(",\"AXP192\":{\"VBV\":%*_f,\"VBC\":%*_f,\"BV\":%*_f,\"BC\":%*_f,\"" D_JSON_TEMPERATURE "\":%*_f}"),
       Settings.flag2.voltage_resolution, &axp192.vbus_v,
       Settings.flag2.current_resolution, &axp192.vbus_c,
       Settings.flag2.voltage_resolution, &axp192.batt_v,
       Settings.flag2.current_resolution, &axp192.batt_c,
       Settings.flag2.temperature_resolution, &axp192.temp);
   } else {
     WSContentSend_Voltage("VBus", axp192.vbus_v);
     WSContentSend_CurrentMA("VBus", axp192.vbus_c);
     WSContentSend_Voltage("Batt", axp192.batt_v);
     WSContentSend_CurrentMA("Batt", axp192.batt_c);
     WSContentSend_Temp("AXP192", axp192.temp);
   }
 }

void AXP192Detect(void) {
  Axp.begin();
  axp192.ready = true;
}

void MPU6886Every_Second(void) {
  AXP192GetADC();
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns88(uint8_t function) {
  if (!I2cEnabled(XI2C_59)) { return false; }

  bool result = false;

  if (FUNC_INIT == function) {
    AXP192_Detect();
  }
  else if (axp192.ready) {
    switch (function) {
      case FUNC_EVERY_SECOND:
        AXP192_Every_Second();
        break;
      case FUNC_JSON_APPEND:
        AXP192_Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        AXP192_Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_MPU6886
#endif  // USE_I2C
