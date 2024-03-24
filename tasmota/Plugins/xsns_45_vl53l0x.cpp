/*
  xsns_45_vl53l0x.ino - VL53L0X time of flight multiple sensors support for Tasmota

  Copyright (C) 2021  Theo Arends, Gerhard Mutz and Adrian Scillato

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

#ifdef USE_VL53L0X_MOOD

#include "module.h"
#include "module_defines.h"

/*********************************************************************************************\
 * VL53L0x time of flight sensor
 *
 * I2C Addres: 0x29
 *********************************************************************************************
 *
 * Note: When using multiple VL53L0X, it is required to also wire the XSHUT pin of all those sensors
 * in order to let Tasmota change by software the I2C address of those and give them an unique address
 * for operation. The sensor don't save its address, so this procedure of changing its address is needed
 * to be performed every restart. The Addresses used for this are 120 (0x78) to 127 (0x7F). In the I2c
 * Standard (https://i2cdevices.org/addresses) those addresses are used by the PCA9685, so take into
 * account they won't work together.
 *
 * The default value of VL53LXX_MAX_SENSORS is set in the file tasmota.h
 * Changing that is backwards incompatible - Max supported devices by this driver are 8
 *
 **********************************************************************************************
 *
 * How to install this sensor: https://www.st.com/resource/en/datasheet/vl53l0x.pdf
 *
 * If you are going to use long I2C wires read this:
 * https://hackaday.com/2017/02/08/taking-the-leap-off-board-an-introduction-to-i2c-over-long-wires/
 *
\*********************************************************************************************/


// Uncomment this line to use long range mode. This
// increases the sensitivity of the sensor and extends its
// potential range, but increases the likelihood of getting
// an inaccurate reading because of reflections from objects
// other than the intended target. It works best in dark
// conditions.

//#define VL53L0X_LONG_RANGE

// Uncomment ONE of these two lines to get
// - higher speed at the cost of lower accuracy OR
// - higher accuracy at the cost of lower speed

//#define VL53L0X_HIGH_SPEED
//#define VL53L0X_HIGH_ACCURACY

#define USE_VL_MEDIAN
#define USE_VL_MEDIAN_SIZE 5   // Odd number of samples median detection

#include "../Tasmota/include/i18n.h"
#include "VL53L0X.h"

#define VL53L0X_ADDRESS 0x29
#ifndef VL53L0X_XSHUT_ADDRESS
#define VL53L0X_XSHUT_ADDRESS 0x78
#endif

#define VL53L0_REV  1<<16|2

PUSH_OPTIONS

// this is the structure of the module:
// descripotr, code, end
MODULE_DESCRIPTOR("VL53L0", MODULE_TYPE_SENSOR, VL53L0_REV,"",0,"",0,"",0,"",0)
MODULE_PART int32_t VL53L0X_Detect();
MODULE_PART void VL53L0X_Every_250MSecond(void);
MODULE_PART void VL53L0X_Show(boolean json);
MODULE_PART void VL53L0X_Deinit();
MODULE_PART MOD_RESULT mod_func_execute(uint32_t sel);
MODULE_END

#define VL53LXX_MAX_SENSORS 1

typedef struct {
  uint16_t distance;
  uint16_t buffer[USE_VL_MEDIAN_SIZE];
  uint8_t ready;
  uint8_t index;
} VLX_DATA;

typedef struct {
  bool VL53L0X_xshut;
  bool VL53L0X_detected;
  VLX_DATA Vl53l0x_data;
  VLX_MEM vlx_mem;
} MODULE_MEMORY;

// ease memory objects
#define VL53L0X_xshut mem->VL53L0X_xshut
#define address mem->vlx_mem.address
#define VL53L0X_detected mem->VL53L0X_detected
#define Vl53l0x_data mem->Vl53l0x_data
#define io_timeout mem->vlx_mem.io_timeout
#define did_timeout mem->vlx_mem.did_timeout
#define timeout_start_ms mem->vlx_mem.timeout_start_ms
#define stop_variable mem->vlx_mem.stop_variable
#define measurement_timing_budget_us mem->vlx_mem.measurement_timing_budget_us
#define last_status mem->vlx_mem.last_status

// library
#include "VL53L0Xm.h"

/********************************************************************************************/

int32_t VL53L0X_Detect(void) {
ALLOCMEM

  VL53L0X_detected = false;
  
  if (I2cSetDevice(VL53L0X_ADDRESS)) {
    I2cSetActiveFound(VL53L0X_ADDRESS, PSTR("VL53L0X"), 0);

    if (VL53L0X_init(0)) {
      AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_I2C D_SENSOR " VL53L0X %d " D_SENSOR_DETECTED " - " D_NEW_ADDRESS " 0x%02X"), 1, VL53L0X_ADDRESS);
    } else {
      VL53L0X_Deinit();
      return false;
    }

    VL53L0X_setTimeout(500);

#if defined VL53L0X_LONG_RANGE
    // lower the return signal rate limit (default is 0.25 MCPS)
    VL53L0X_setSignalRateLimit(0.1);
    // increase laser pulse periods (defaults are 14 and 10 PCLKs)
    VL53L0X_setVcselPulsePeriod(VcselPeriodPreRange, 18);
    VL53L0X_setVcselPulsePeriod(VcselPeriodFinalRange, 14);
#endif
#if defined VL53L0X_HIGH_SPEED
    // reduce timing budget to 20 ms (default is about 33 ms)
    VL53L0X_setMeasurementTimingBudget(20000);
#elif defined VL53L0X_HIGH_ACCURACY
    // increase timing budget to 200 ms
    VL53L0X_setMeasurementTimingBudget(200000);
#endif
    // Start continuous back-to-back mode (take readings as
    // fast as possible).  To use continuous timed mode
    // instead, provide a desired inter-measurement period in
    // ms (e.g. sensor.startContinuous(100)).
    VL53L0X_startContinuous(0);

    Vl53l0x_data.ready = 1;
    Vl53l0x_data.index = 0;
    VL53L0X_detected = true;
    initialized = true;

  }

  if (VL53L0X_detected == false) {
    VL53L0X_Deinit();
  }
  return VL53L0X_detected;
}

void VL53L0X_Every_250MSecond(void) {
  SETREGS

  if (!VL53L0X_detected) {
    return;
  }

  uint16_t dist = VL53L0X_readRangeContinuousMillimeters();
  if ((0 == dist) || (dist > 2200)) {
   // dist = 9999;
  }

#ifdef USE_VL_MEDIAN

  // store in ring buffer
  Vl53l0x_data.buffer[Vl53l0x_data.index] = dist;
  Vl53l0x_data.index++;
  if (Vl53l0x_data.index >= USE_VL_MEDIAN_SIZE) {
    Vl53l0x_data.index = 0;
  }

  // sort list and take median
  uint16_t tbuff[USE_VL_MEDIAN_SIZE];
  memmove(tbuff, Vl53l0x_data.buffer, sizeof(tbuff));
  uint16_t tmp;
  uint8_t flag;
  for (uint32_t ocnt = 0; ocnt < USE_VL_MEDIAN_SIZE; ocnt++) {
    flag = 0;
    for (uint32_t count = 0; count < USE_VL_MEDIAN_SIZE -1; count++) {
      if (tbuff[count] > tbuff[count +1]) {
        tmp = tbuff[count];
        tbuff[count] = tbuff[count +1];
        tbuff[count +1] = tmp;
        flag = 1;
      }
    }
    if (!flag) { break; }
  }
  Vl53l0x_data.distance = tbuff[(USE_VL_MEDIAN_SIZE -1) / 2];
#else
  Vl53l0x_data.distance = dist;
#endif

}

void VL53L0X_Show(boolean json) {
  SETREGS

  if (!VL53L0X_detected) {
    return;
  }


  //float distance = (Vl53l0x_data.distance == 9999) ? NAN : (float)Vl53l0x_data.distance / 10;  // cm 
  float distance = jfdiv(jtofloat(Vl53l0x_data.distance) , 10.0);

  char dstr[16];
  ftostrfd(distance, 1, dstr);


  if (json) {
    ResponseAppend_P(PSTR(",\"VL53L0X\":{\"Distance\":%s}"), dstr);
  } else {
    WSContentSend_PD(PSTR("{s}Distance{m}%s cm{e}"), dstr);
  }

  if (VL53L0X_timeoutOccurred()) {
    //AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_I2C "Timeout waiting for %s"), types);
  }
  
}

void  VL53L0X_Deinit() {
  SETREGS
  I2cResetActive(VL53L0X_ADDRESS, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

MOD_RESULT mod_func_execute(uint32_t sel){
  bool result = false;

  switch (sel) {
      case FUNC_INIT:
        result = VL53L0X_Detect();
        break;
      case FUNC_EVERY_250_MSECOND:
        VL53L0X_Every_250MSecond();
        break;
      case FUNC_JSON_APPEND:
        VL53L0X_Show(1);
        break;
      case FUNC_WEB_SENSOR:
        VL53L0X_Show(0);
        break;
      case FUNC_DEINIT:
        VL53L0X_Deinit();
        break;
  }
  return result;
}

#endif  // USE_VL53L0X
