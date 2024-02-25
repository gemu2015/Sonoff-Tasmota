/*
  xsns_31_ccs811.ino - CCS811 gas and air quality sensor support for Tasmota

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

#ifdef USE_I2C
#ifdef USE_CCS811_MOD

#include "module.h"
#include "module_defines.h"

/*********************************************************************************************\
 * CCS811 - Gas (TVOC - Total Volatile Organic Compounds) and Air Quality (CO2)
 *
 * Source: Adafruit
 *
 * I2C Address: 0x5A assumes ADDR connected to Gnd, Wake also must be grounded
\*********************************************************************************************/

#define XSNS_31             31
#define XI2C_24             24  // See I2CDEVICES.md

#define EVERYNSECONDS 5

#include "Adafruit_CCS811.h"


#define CCS811_REV  1<<16|2

MODULE_DESCRIPTOR("CCS811", MODULE_TYPE_SENSOR, CCS811_REV,"",0,"",0,"",0,"",0)
// all functions must be declared MUDULE_PART
MODULE_PART int32_t CCS811_Detect(void);
MODULE_PART void CCS811_Update(void);
MODULE_PART void CCS811_Show(bool json);
MODULE_PART void CCS811_Deinit();
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);

MODULE_END


/********************************************************************************************/

const char HTTP_SNS_CCS811[] PROGMEM =
  "{s}CCS811 " D_ECO2 "{m}%d " D_UNIT_PARTS_PER_MILLION "{e}"                // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
  "{s}CCS811 " D_TVOC "{m}%d " D_UNIT_PARTS_PER_BILLION "{e}";


typedef struct {
Adafruit_CCS811 ccs;
uint8_t CCS811_ready = 0;
uint8_t CCS811_type = 0;;
uint16_t eCO2;
uint16_t TVOC;
uint8_t tcnt = 0;
uint8_t ecnt = 0;
} MODULE_MEMORY;

// ease memory objects
#define CCS811_ready mem->CCS811_ready
#define CCS811_type mem->CCS811_type
#define eCO2 mem->eCO2
#define TVOC mem->TVOC
#define tcnt mem->tcnt
#define ecnt mem->ecnt

#include "CSC811.h"

void CCS811_Detect(void) {
  if (!I2cSetDevice(CCS811_ADDRESS)) { return; }

  if (!ccs.begin(CCS811_ADDRESS)) {
    CCS811_type = 1;
    I2cSetActiveFound(CCS811_ADDRESS, "CCS811");
  }
}

// Perform every n second
void CCS811_Update(void) {
  tcnt++;
  if (tcnt >= EVERYNSECONDS) {
    tcnt = 0;
    CCS811_ready = 0;
    if (ccs.available()) {
      if (!ccs.readData()){
        TVOC = ccs.getTVOC();
        eCO2 = ccs.geteCO2();
        CCS811_ready = 1;
        if (TasmotaGlobal.global_update && (TasmotaGlobal.humidity > 0) && !isnan(TasmotaGlobal.temperature_celsius)) {
          ccs.setEnvironmentalData((uint8_t)TasmotaGlobal.humidity, TasmotaGlobal.temperature_celsius);
        }
        ecnt = 0;
      }
    } else {
      // failed, count up
      ecnt++;
      if (ecnt > 6) {
        // after 30 seconds, restart
        ccs.begin(CCS811_ADDRESS);
      }
    }
  }
}

void CCS811_Show(bool json) {
  if (CCS811_ready) {
    if (json) {
      ResponseAppend_P(PSTR(",\"CCS811\":{\"" D_JSON_ECO2 "\":%d,\"" D_JSON_TVOC "\":%d}"), eCO2,TVOC);
#ifdef USE_DOMOTICZ
      if (0 == TasmotaGlobal.tele_period) DomoticzSensor(DZ_AIRQUALITY, eCO2);
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
    } else {
      WSContentSend_PD(HTTP_SNS_CCS811, eCO2, TVOC);
#endif
    }
  }
}

void CCS811_Deinit(void) {
  SETREGS
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

MOD_RESULT mod_func_execute(uint32_t function) {
  if (!I2cEnabled(XI2C_24)) { return false; }

  MOD_RESULT result = false;

  if (FUNC_INIT == function) {
    CCS811_Detect();
  }
  else if (CCS811_type) {
    switch (function) {
      case FUNC_EVERY_SECOND:
        CCS811_Update();
        break;
      case FUNC_JSON_APPEND:
        CCS811_Show(1);
        break;
      case FUNC_WEB_SENSOR:
        CCS811_Show(0);
        break;
    }
  }
  return result;
}

#endif  // USE_CCS811
#endif  // USE_I2C
