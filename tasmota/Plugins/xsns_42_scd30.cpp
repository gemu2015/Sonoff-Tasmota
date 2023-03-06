/*
  xsns_42_scd30.ino - SCD30 CO2 sensor support for Tasmota

  Copyright (C) 2021  Frogmore42

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

#ifdef USE_SCD30_MOD
/*********************************************************************************************\
 * SCD30 NDIR CO2 Temperature and Humidity sensor
\*********************************************************************************************/


//#define SCD30_DEBUG

#define SCD30_ADDRESS                 0x61

#define SCD30_MAX_MISSED_READS        3
#define SCD30_STATE_NO_ERROR          0
#define SCD30_STATE_ERROR_DATA_CRC    1
#define SCD30_STATE_ERROR_READ_MEAS   2
#define SCD30_STATE_ERROR_SOFT_RESET  3
#define SCD30_STATE_ERROR_I2C_RESET   4
#define SCD30_STATE_ERROR_UNKNOWN     5

#define ERROR_SCD30_NO_ERROR                        0
#define ERROR_SCD30_NO_DATA                0x80000000
#define ERROR_SCD30_CO2_ZERO               0x90000000
#define ERROR_SCD30_UNKNOWN_ERROR           0x1000000
#define ERROR_SCD30_CRC_ERROR               0x2000000
#define ERROR_SCD30_NOT_ENOUGH_BYTES_ERROR  0x3000000
#define ERROR_SCD30_NOT_FOUND_ERROR         0x4000000
#define ERROR_SCD30_NOT_A_NUMBER_ERROR      0x5000000
#define ERROR_SCD30_INVALID_VALUE           0x6000000

#include "module.h"
#include "module_defines.h"

#define SCD30_REV  1<<16|0

MODULE_DESCRIPTOR("SCD30", MODULE_TYPE_SENSOR, SCD30_REV,"",0,"",0,"",0,"",0)
// all functions must be declared MUDULE_PART
MODULE_PART int32_t MOD_FUNC(SCD30_Detect);
MODULE_PART void MOD_FUNC(SCD30_Show, bool json);
MODULE_PART void MOD_FUNC(SCD30_Update);
MODULE_PART void MOD_FUNC(CmndScd30Altitude);
MODULE_PART void  MOD_FUNC(CmndScd30AutoMode);
MODULE_PART void  MOD_FUNC(CmndScd30Calibrate);
MODULE_PART void  MOD_FUNC(CmndScd30Firmware);
MODULE_PART void  MOD_FUNC(CmndScd30Interval);
MODULE_PART void  MOD_FUNC(CmndScd30Pressure);
MODULE_PART void  MOD_FUNC(CmndScd30TempOffset);
MODULE_PART void MOD_FUNC(SCD30_Deinit);
MODULE_PART int32_t MOD_FUNC(mod_func_execute, uint32_t sel);

MODULE_END

/********************************************************************************************/

typedef struct {
  float humidity = 0.0f;
  float temperature = 0.0f;
  int error_state = SCD30_STATE_NO_ERROR;
  int loop_count = 0;
  int data_not_available_count = 0;
  int good_measure_count = 0;
  int reset_count = 0;
  int error_count = 0;
  int co2_zero_count = 0;
  int i2c_reset_count = 0;
  uint16_t interval;
  uint16_t co2 = 0;
  uint16_t co2e_avg = 0;
  bool init_once;
  bool found = false;
  bool data_valid = false;
} SCD30;

typedef struct {
  uint8_t ready;
  SCD30 Scd30;
} MODULE_MEMORY;

#define ready mem->ready
#define Scd30 mem->Scd30

int32_t MOD_FUNC(SCD30_Detect) {
  ALLOCMEM

  ready = false;

  Scd30.found = false;
  Scd30.data_valid = true;
  initialized = 1;

  if (I2cSetDevice(SCD30_ADDRESS)) {

    //scd30.begin();

    uint8_t major = 0;
    uint8_t minor = 0;
    /*
    if (scd30.getFirmwareVersion(&major, &minor)) { 
      goto exit; 
    }
    if (scd30.getMeasurementInterval(&Scd30.interval)) {
      goto exit; 
    }
    if (scd30.beginMeasuring()) {
      goto exit; 
    }
    */
    AddLog(LOG_LEVEL_INFO, PSTR("SCD30: FW v%d.%d"), major, minor);
    I2cSetActiveFound(SCD30_ADDRESS, PSTR("SCD30"), 0);
    Scd30.found = true;
    initialized = 1;
    ready = true;
    return ready;
  }
  
  exit:
  CALL_MOD_FUNC(SCD30_Deinit);
  return ready;
}


// gets data from the sensor every 3 seconds or so to give the sensor time to gather new data
void MOD_FUNC(SCD30_Update) {
  SETREGS
  Scd30.loop_count++;
  if (Scd30.loop_count > (Scd30.interval - 1)) {
    uint32_t error = 0;
    switch (Scd30.error_state) {
      case SCD30_STATE_NO_ERROR: {
        //error = scd30.readMeasurement(&Scd30.co2, &Scd30.co2e_avg, &Scd30.temperature, &Scd30.humidity);
        switch (error) {
          case ERROR_SCD30_NO_ERROR:
            Scd30.loop_count = 0;
            Scd30.data_valid = true;
            Scd30.good_measure_count++;
            break;

          case ERROR_SCD30_NO_DATA:
            Scd30.data_not_available_count++;
            break;

          case ERROR_SCD30_CRC_ERROR:
            Scd30.error_state = SCD30_STATE_ERROR_DATA_CRC;
            Scd30.error_count++;
#ifdef SCD30_DEBUG
            AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: CRC error, CRC error: %ld, CO2 zero: %ld, good: %ld, no data: %ld, sc30_reset: %ld, i2c_reset: %ld"),
              Scd30.error_count, Scd30.co2_zero_count, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count, Scd30.i2c_reset_count);
#endif
            break;

          case ERROR_SCD30_CO2_ZERO:
            Scd30.co2_zero_count++;
#ifdef SCD30_DEBUG
            AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: CO2 zero, CRC error: %ld, CO2 zero: %ld, good: %ld, no data: %ld, sc30_reset: %ld, i2c_reset: %ld"),
              Scd30.error_count, Scd30.co2_zero_count, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count, Scd30.i2c_reset_count);
#endif
            break;

          default: {
            Scd30.error_state = SCD30_STATE_ERROR_READ_MEAS;
#ifdef SCD30_DEBUG
            AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: Update: ReadMeasurement error: 0x%lX, counter: %ld"), error, Scd30.loop_count);
#endif
             return;
          }
          break;
        }
      }
      break;

      case SCD30_STATE_ERROR_DATA_CRC: {
        //Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: in error state: %d, good: %ld, no data: %ld, sc30 reset: %ld, i2c reset: %ld"),
          Scd30.error_state, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count, Scd30.i2c_reset_count);
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: got CRC error, try again, counter: %ld"), Scd30.loop_count);
#endif
        Scd30.error_state = ERROR_SCD30_NO_ERROR;
      }
      break;

      case SCD30_STATE_ERROR_READ_MEAS: {
        //Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: in error state: %d, good: %ld, no data: %ld, sc30 reset: %ld, i2c reset: %ld"),
          Scd30.error_state, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count, Scd30.i2c_reset_count);
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: not answering, sending soft reset, counter: %ld"), Scd30.loop_count);
#endif
        Scd30.reset_count++;
        //error = scd30.softReset();
        if (error) {
#ifdef SCD30_DEBUG
          AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: resetting got error: 0x%lX"), error);
#endif
          error >>= 8;
          if (error == 4) {
            Scd30.error_state = SCD30_STATE_ERROR_SOFT_RESET;
          } else {
            Scd30.error_state = SCD30_STATE_ERROR_UNKNOWN;
          }
        } else {
          Scd30.error_state = ERROR_SCD30_NO_ERROR;
        }
      }
      break;

      case SCD30_STATE_ERROR_SOFT_RESET: {
        //Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: in error state: %d, good: %ld, no data: %ld, sc30 reset: %ld, i2c reset: %ld"),
          Scd30.error_state, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count, Scd30.i2c_reset_count);
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: clearing i2c bus"));
#endif
        Scd30.i2c_reset_count++;
        //error = scd30.clearI2CBus();
        if (error) {
          Scd30.error_state = SCD30_STATE_ERROR_I2C_RESET;
#ifdef SCD30_DEBUG
          AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: error clearing i2c bus: 0x%lX"), error);
#endif
        } else {
          Scd30.error_state = ERROR_SCD30_NO_ERROR;
        }
      }
      break;

      default: {
        //Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: unknown error state: 0x%lX"), Scd30.error_state);
#endif
        Scd30.error_state = SCD30_STATE_ERROR_SOFT_RESET; // try again
      }
    }

    if (Scd30.loop_count > (SCD30_MAX_MISSED_READS * Scd30.interval)) {
      Scd30.data_valid = false;
    }
  }
}

/*********************************************************************************************\
 * Command Scd30
\*********************************************************************************************/
const char kScd30Commands[] PROGMEM = "Scd30|"  // Prefix
  "Alt|Auto|Cal|FW|Int|Pres|TOff";
void (*const kScd30Command[])(MODULES_TABLE*) PROGMEM = { &CmndScd30Altitude, &CmndScd30AutoMode, &CmndScd30Calibrate, &CmndScd30Firmware, &CmndScd30Interval, &CmndScd30Pressure, &CmndScd30TempOffset };


void MOD_FUNC(CmndScd30Altitude) {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    //scd30.setAltitudeCompensation(value);
  } else {
    //scd30.getAltitudeCompensation(&value);
  }
  ResponseCmndNumber(value);
};


void  MOD_FUNC(CmndScd30AutoMode) {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    //scd30.setCalibrationType(value);
  } else {
    //scd30.getCalibrationType(&value);
  }
  ResponseCmndNumber(value);
};

void  MOD_FUNC(CmndScd30Calibrate) {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    //scd30.setForcedRecalibrationFactor(value);
  } else {
    //scd30.getForcedRecalibrationFactor(&value);
  }
  ResponseCmndNumber(value);
};

void  MOD_FUNC(CmndScd30Firmware) {
  SETREGS
  uint8_t major = 0;
  uint8_t minor = 0;
  int error = 0;
  //scd30.getFirmwareVersion(&major, &minor);
  if (!error) {
    float firmware = major + ((float)minor / 100);
    ResponseCmndFloat(firmware, 2);
  }
};

void  MOD_FUNC(CmndScd30Interval) {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    int error = 0;
    //scd30.setMeasurementInterval(value);
    if (!error) {
      Scd30.interval = value;
    }
  }
  //scd30.getMeasurementInterval(&value);
  ResponseCmndNumber(value);
};

void  MOD_FUNC(CmndScd30Pressure) {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    //scd30.setAmbientPressure(value);
  } else {
    //scd30.getAmbientPressure(&value);
  }
  ResponseCmndNumber(value);
};

void  MOD_FUNC(CmndScd30TempOffset) {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    //scd30.setTemperatureOffset(value);
  } else {
    //scd30.getTemperatureOffset(&value);
  }
  ResponseCmndNumber(value);
};

/********************************************************************************************/

const char HTTP_SNS_CO2[]           PROGMEM = "{s}%s CO2{m}%d ppm{e}";
const char HTTP_SNS_CO2EAVG[]       PROGMEM = "{s}%s eCO2{m}%d ppm{e}";

void MOD_FUNC(SCD30_Show, bool json) {
  SETREGS

  if (Scd30.data_valid) {
    float t = ConvertTemp(Scd30.temperature);
    float h = ConvertHumidity(Scd30.humidity);

    if (json) {
      ResponseAppend_P(PSTR(",\"SCD30\":{\"CO2\":%d,\" eCO2\":%d,"), Scd30.co2, Scd30.co2e_avg);
      ResponseAppendTHD(t, h);
      ResponseJsonEnd();
    } else {
      WSContentSend_PD(GSTR(HTTP_SNS_CO2EAVG), PSTR("SCD30"), Scd30.co2e_avg);
      WSContentSend_PD(GSTR(HTTP_SNS_CO2), PSTR("SCD30"), Scd30.co2);
      WSContentSend_THD(PSTR("SCD30"), t, h);
    }
  }
}

void MOD_FUNC(SCD30_Deinit) {
  SETREGS
  I2cResetActive(SCD30_ADDRESS, 1);
  RETMEM
}



/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t MOD_FUNC(mod_func_execute, uint32_t sel) {
  SETREGS
  bool result = false;

  // https://github.com/arendst/Tasmota/issues/15438 and datasheet (The boot-up time is < 2 s.)
/*
  if (FUNC_INIT == function) {
    Scd30Detect();
  }
*/
  switch (sel) {
    case FUNC_INIT:
      result = CALL_MOD_FUNC(SCD30_Detect);
      break;
    case FUNC_EVERY_SECOND:
      CALL_MOD_FUNC(SCD30_Update);
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kScd30Commands, kScd30Command);
      break;
    case FUNC_JSON_APPEND:
      CALL_MOD_FUNC(SCD30_Show, 1);
      break;
    case FUNC_WEB_SENSOR:
      CALL_MOD_FUNC(SCD30_Show, 0);
      break;
    case FUNC_DEINIT:
      CALL_MOD_FUNC(SCD30_Deinit);
      break;
  }
  return result;
}

#endif  // USE_SCD30_MOD
