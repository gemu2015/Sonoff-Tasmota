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

#define CCS811_REV  1<<16|2

PUSH_OPTIONS

MODULE_DESCRIPTOR("CCS811", MODULE_TYPE_SENSOR, CCS811_REV,"",0,"",0,"",0,"",0)
// all functions must be declared MUDULE_PART
MODULE_PART bool CCS811_Detect(void);
MODULE_PART void CCS811_Update(void);
MODULE_PART void CCS811_Show(bool json);
MODULE_PART void CCS811_Deinit();
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);

MODULE_END


/********************************************************************************************/
#define D_UNIT_PARTS_PER_MILLION "ppm"
#define D_UNIT_PARTS_PER_BILLION "ppb"
#define D_ECO2 "eCO₂"
#define D_TVOC "TVOC"
#define D_JSON_ECO2 "eCO2"
#define D_JSON_TVOC "TVOC"

const char HTTP_SNS_CCS811[] PROGMEM =
  "{s}CCS811 " D_ECO2 "{m}%d " D_UNIT_PARTS_PER_MILLION "{e}"                // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
  "{s}CCS811 " D_TVOC "{m}%d " D_UNIT_PARTS_PER_BILLION "{e}";
const char JSON_SNS_CCS811[] PROGMEM =
  ",\"CCS811\":{\"" D_JSON_ECO2 "\":%d,\"" D_JSON_TVOC "\":%d}";

const char CCS811_dev[] PROGMEM = "CCS811";

typedef union  {
	uint8_t data;
	struct {
		uint8_t ERROR: 1;
    uint8_t reserved1 : 2;
    uint8_t DATA_READY: 1;
    uint8_t APP_VALID: 1;
		uint8_t reserved2 : 2;
    uint8_t FW_MODE: 1;
	};
} CSS811_STAT;

typedef union  {
	uint8_t data;
	struct {
    uint8_t reserved1 : 2;
		uint8_t THRESH: 1;
    uint8_t INTERRUPT: 1;
    uint8_t DRIVE_MODE: 3;
		uint8_t reserved2 : 1;
	};
} CSS811_MEAS;

typedef struct {
  uint8_t i2c_addr;;
  uint16_t _TVOC;
  uint16_t _eCO2;
  CSS811_STAT stat;
  CSS811_MEAS meas;
} CCS811;

typedef struct {
uint8_t CCS811_ready;
uint16_t eCO2;
uint16_t TVOC;
uint8_t tcnt;
uint8_t ecnt;
bool ready;
CCS811 ccs;
} MODULE_MEMORY;

// ease memory objects
#define CCS811_ready mem->CCS811_ready
#define eCO2 mem->eCO2
#define TVOC mem->TVOC
#define tcnt mem->tcnt
#define ecnt mem->ecnt
#define ready mem->ready
#define ccs mem->ccs

#include "CCS811.h"

#define CCS811_ADDRESS  0x5A

bool CCS811_Detect(void) {
  ALLOCMEM

  ready = false;
  tcnt = 0;
  ecnt = 0;
  CCS811_ready = 0;

  if (!I2cSetDevice(CCS811_ADDRESS)) {
    CCS811_Deinit();
    return false;
  }

  if (!CCS811_begin(CCS811_ADDRESS)) {
    char *cp = copyStr(GSTR(CCS811_dev));
    I2cSetActiveFound(CCS811_ADDRESS, cp, 0);
    free(cp);
    ready = true;
    initialized = true;
    return true;
  }

  return false;
}

// Perform every n second
void CCS811_Update(void) {
  SETREGS
  STGLOB

  if (!ready) {
    return;
  }
  tcnt++;
  if (tcnt >= EVERYNSECONDS) {
    tcnt = 0;
    CCS811_ready = 0;
    if (CCS811_available()) {
      if (!CCS811_readData()){
        TVOC = CCS811_getTVOC();
        eCO2 = CCS811_geteCO2();
        CCS811_ready = 1;
        if (TasmotaGlobal->global_update && (floatunsisf(TasmotaGlobal->humidity) > 0) && !isnan(TasmotaGlobal->temperature_celsius)) {
          uint16_t hum = floatunsisf(TasmotaGlobal->humidity);
          float temp = TasmotaGlobal->temperature_celsius;
          CCS811_setEnvironmentalData(hum, temp);
        }
        ecnt = 0;
      }
    } else {
      // failed, count up
      ecnt++;
      if (ecnt > 6) {
        // after 30 seconds, restart
        CCS811_begin(CCS811_ADDRESS);
      }
    }
  }
}

void CCS811_Show(bool json) {
  SETREGS

  if (!ready) {
    return;
  }

  if (CCS811_ready) {
    if (json) {
      ResponseAppend_P(GSTR(JSON_SNS_CCS811), eCO2, TVOC);
    } else {
      WSContentSend_PD(GSTR(HTTP_SNS_CCS811), eCO2, TVOC);
    }
  }
}

void CCS811_Deinit(void) {
  SETREGS
  I2cResetActive(CCS811_ADDRESS, 0);
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

MOD_RESULT mod_func_execute(uint32_t sel) {
MOD_RESULT result = false;

  switch (sel) {
      case FUNC_INIT:
        result = CCS811_Detect();
        break;
      case FUNC_EVERY_SECOND:
        CCS811_Update();
        break;
      case FUNC_JSON_APPEND:
        CCS811_Show(1);
        break;
      case FUNC_WEB_SENSOR:
        CCS811_Show(0);
        break;
      case FUNC_DEINIT:
        CCS811_Deinit();
        break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_CCS811

