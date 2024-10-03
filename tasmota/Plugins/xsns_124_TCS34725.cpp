/* xsns_124_TCS3472.ino support for TCS34725 ambient light sensor

  Copyright (C) 2018  Theo Arends , Gerhard Mutz and Adafruit

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

#ifdef USE_TCS34725_MOD

#include "module.h"
#include "module_defines.h"

#define XSNS_104       104
#define XI2C_55       55  // See I2CDEVICES.md

#include "TCS34725/Adafruit_TCS34725_cpp.h"
// about 2,2 k flash

//
// An experimental wrapper class that implements the improved lux and color temperature from
// TAOS and a basic autorange mechanism.
//
// Written by ductsoup, public domain
//

// RGB Color Sensor with IR filter and White LED - TCS34725
// I2C 7-bit address 0x29, 8-bit address 0x52
//
// http://www.adafruit.com/product/1334
// http://learn.adafruit.com/adafruit-color-sensors/overview
// http://www.adafruit.com/datasheets/TCS34725.pdf
// http://www.ams.com/eng/Products/Light-Sensors/Color-Sensor/TCS34725
// http://www.ams.com/eng/content/view/download/265215 <- DN40, calculations
// http://www.ams.com/eng/content/view/download/181895 <- DN39, some thoughts on autogain
// http://www.ams.com/eng/content/view/download/145158 <- DN25 (original Adafruit calculations)
//
// some magic numbers for this device from the DN40 application note
#define TCS34725_R_Coef 0.136
#define TCS34725_G_Coef 1.000
#define TCS34725_B_Coef -0.444
#define TCS34725_GA 1.0
#define TCS34725_DF 310.0
#define TCS34725_CT_Coef 3810.0
#define TCS34725_CT_Offset 1391.0


PUSH_OPTIONS

#define TCS34725_REV  1 << 16 | 4

MODULE_DESCRIPTOR("TCS34725", MODULE_TYPE_SENSOR, TCS34725_REV,"",0,"",0,"",0,"",0)
MODULE_END


// Autorange class for TCS34725
class tcs34725 {
public:
  tcs34725(void);
  boolean begin(void);
  void getData(void);
  void getRawData_noDelay(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
  boolean isAvailable, isSaturated;
  uint16_t againx, atime, atime_ms;
  uint16_t r, g, b, c;
  uint16_t ir;
  uint16_t r_comp, g_comp, b_comp, c_comp;
  uint16_t saturation, saturation75;
  float cratio, cpl, ct, lux, maxlux;

private:
  struct tcs_agc {
    tcs34725Gain_t ag;
    tcs34725IntegrationTime_t at;
    uint16_t mincnt;
    uint16_t maxcnt;
  };
  static const tcs_agc agc_lst[];
  uint16_t agc_cur;

  void setGainTime(void);
  Adafruit_TCS34725 tcs;
};

typedef struct {
  TwoWire *xWire;
  tcs34725 rgb_sensor;
  bool ready;
} MODULE_MEMORY;

#define ready mem->ready
#define rgb_sensor mem->rgb_sensor

//
// Gain/time combinations to use and the min/max limits for hysteresis
// that avoid saturation. They should be in order from dim to bright.
//
// Also set the first min count and the last max count to 0 to indicate
// the start and end of the list.
//
const tcs34725::tcs_agc tcs34725::agc_lst[] = {
  { TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_700MS,     0, 20000 },
  { TCS34725_GAIN_60X, TCS34725_INTEGRATIONTIME_154MS,  4990, 63000 },
  { TCS34725_GAIN_16X, TCS34725_INTEGRATIONTIME_154MS, 16790, 63000 },
  { TCS34725_GAIN_4X,  TCS34725_INTEGRATIONTIME_154MS, 15740, 63000 },
  { TCS34725_GAIN_1X,  TCS34725_INTEGRATIONTIME_154MS, 15740, 0 }
};
MODULE_PART tcs34725::tcs34725() : agc_cur(0), isAvailable(0), isSaturated(0) {
}

// initialize the sensor
MODULE_PART boolean tcs34725::begin(void) {
  tcs = Adafruit_TCS34725(agc_lst[agc_cur].at, agc_lst[agc_cur].ag);
  if ((isAvailable = tcs.begin()))
    setGainTime();
  return(isAvailable);
}

// Set the gain and integration time
MODULE_PART void tcs34725::setGainTime(void) {
  tcs.setGain(agc_lst[agc_cur].ag);
  tcs.setIntegrationTime(agc_lst[agc_cur].at);
  atime = int(agc_lst[agc_cur].at);
  atime_ms = ((256 - atime) * 2.4);
  switch(agc_lst[agc_cur].ag) {
  case TCS34725_GAIN_1X:
    againx = 1;
    break;
  case TCS34725_GAIN_4X:
    againx = 4;
    break;
  case TCS34725_GAIN_16X:
    againx = 16;
    break;
  case TCS34725_GAIN_60X:
    againx = 60;
    break;
  }
}

MODULE_PART void tcs34725::getRawData_noDelay(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c) {
  *c = tcs.read16(TCS34725_CDATAL);
  *r = tcs.read16(TCS34725_RDATAL);
  *g = tcs.read16(TCS34725_GDATAL);
  *b = tcs.read16(TCS34725_BDATAL);
}

// Retrieve data from the sensor and do the calculations
MODULE_PART void tcs34725::getData(void) {
SETREGS
  // read the sensor and autorange if necessary
  tcs.getRawData(&r, &g, &b, &c);
  //getRawData_noDelay(&r, &g, &b, &c);
  while(1) {
    if (agc_lst[agc_cur].maxcnt && c > agc_lst[agc_cur].maxcnt)
      agc_cur++;
    else if (agc_lst[agc_cur].mincnt && c < agc_lst[agc_cur].mincnt)
      agc_cur--;
    else break;

    setGainTime();
    delay((256 - atime) * 2.4 * 2); // shock absorber
    tcs.getRawData(&r, &g, &b, &c);
    //getRawData_noDelay(&r, &g, &b, &c);
    break;
  }

  // DN40 calculations
  ir = (r + g + b > c) ? (r + g + b - c) / 2 : 0;
  r_comp = r - ir;
  g_comp = g - ir;
  b_comp = b - ir;
  c_comp = c - ir;
  cratio = float(ir) / float(c);

  saturation = ((256 - atime) > 63) ? 65535 : 1024 * (256 - atime);
  saturation75 = (atime_ms < 150) ? (saturation - saturation / 4) : saturation;
  isSaturated = (atime_ms < 150 && c > saturation75) ? 1 : 0;
  cpl = (atime_ms * againx) / (TCS34725_GA * TCS34725_DF);
  maxlux = 65535 / (cpl * 3);

  lux = (TCS34725_R_Coef * float(r_comp) + TCS34725_G_Coef * float(g_comp) + TCS34725_B_Coef * float(b_comp)) / cpl;
  ct = TCS34725_CT_Coef * float(b_comp) / float(r_comp) + TCS34725_CT_Offset;
}

MODULE_PART MOD_RESULT TCS34725_Detect() {
ALLOCMEM

  I2C_SETWIRE(0);

  if (!I2C_SetDevice(TCS34725_ADDRESS, 0)) {
    return false;
  }
  if (rgb_sensor.begin() == true) {
    ready=1;
    I2C_SetActiveFound(TCS34725_ADDRESS, PSTR("TCS34725"), 0);
  } else {
    // error
    //AddLog_P(LOG_LEVEL_INFO, S_LOG_HTTP, PSTR("TCS34725 init error"));
  }
  return true;
}


MODULE_PART void TCS34725_EverySecond() {
SETREGS
  if (ready) {
    rgb_sensor.getData();
  }
}

#define D_LUX "Lux"
#define D_COLOR_TEMP "ColorTemp"
#define D_RED "red"
#define D_GREEN "green"
#define D_BLUE "blue"
#define D_AMBIENT "ambient"

#ifdef USE_WEBSERVER
const char HTTP_SNS_TCS34725[] PROGMEM =
 "{s}TCS34725 " D_LUX "{m}%d " D_LUX "{e}"
 "{s}TCS34725 " D_COLOR_TEMP "{m}%d " D_UNIT_KELVIN "{e}"
 "{s}TCS34725 " D_RED "{m}%d " "{e}"
 "{s}TCS34725 " D_GREEN "{m}%d " "{e}"
 "{s}TCS34725 " D_BLUE "{m}%d " "{e}"
 "{s}TCS34725 " D_AMBIENT "{m}%d " "{e}";
#endif  // USE_WEBSERVER


const char JSON_TCS34725[] PROGMEM = ",\"TCS34725\":{\"" D_LUX "\":%d,\"" D_COLOR_TEMP "\":%d,\"R\":%d,\"G\":%d,\"B\":%d,\"C\":%d}";


MODULE_PART void TCS34725_Show(boolean json) {
SETREGS

  if (!ready) {
    return;
  }
  if (json) {
    ResponseAppend_P(GSTR(JSON_TCS34725),(uint32_t)rgb_sensor.lux,(uint32_t)rgb_sensor.ct,(uint32_t)rgb_sensor.r,(uint32_t)rgb_sensor.g,(uint32_t)rgb_sensor.b,(uint32_t)rgb_sensor.c);
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(HTTP_SNS_TCS34725),(uint32_t)rgb_sensor.lux,(uint32_t)rgb_sensor.ct,(uint32_t)rgb_sensor.r,(uint32_t)rgb_sensor.g,(uint32_t)rgb_sensor.b,(uint32_t)rgb_sensor.c);
#endif
  }

}

MODULE_PART void TCS34725_Deinit() {
  SETREGS
  I2C_ResetActive(TCS34725_ADDRESS, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/


MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel) {
  bool result = false;
    switch (sel) {
      case pFUNC_INIT:
        result = TCS34725_Detect();
        break;
      case pFUNC_EVERY_SECOND:
        TCS34725_EverySecond();
        break;
      case pFUNC_JSON_APPEND:
        TCS34725_Show(1);
        break;
      case pFUNC_WEB_SENSOR:
        TCS34725_Show(0);
        break;
      case pFUNC_DEINIT:
        TCS34725_Deinit();
        break;
  }
  return result;
}

#endif  // USE_TCS34725
