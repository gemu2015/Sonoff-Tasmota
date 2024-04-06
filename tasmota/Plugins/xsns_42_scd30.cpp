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

#define SCD30_ADDRESS 0x61

#define SCD30_MAX_MISSED_READS 3
#define SCD30_STATE_NO_ERROR 0
#define SCD30_STATE_ERROR_DATA_CRC 1
#define SCD30_STATE_ERROR_READ_MEAS 2
#define SCD30_STATE_ERROR_SOFT_RESET 3
#define SCD30_STATE_ERROR_I2C_RESET 4
#define SCD30_STATE_ERROR_UNKNOWN 5

#define ERROR_SCD30_NO_ERROR 0
#define ERROR_SCD30_NO_DATA 0x80000000
#define ERROR_SCD30_CO2_ZERO 0x90000000
#define ERROR_SCD30_UNKNOWN_ERROR 0x1000000
#define ERROR_SCD30_CRC_ERROR 0x2000000
#define ERROR_SCD30_NOT_ENOUGH_BYTES_ERROR 0x3000000
#define ERROR_SCD30_NOT_FOUND_ERROR 0x4000000
#define ERROR_SCD30_NOT_A_NUMBER_ERROR 0x5000000
#define ERROR_SCD30_INVALID_VALUE 0x6000000

#include "module.h"
#include "module_defines.h"

#define SCD30_REV 1 << 16 | 3

PUSH_OPTIONS

MODULE_DESCRIPTOR("SCD30", MODULE_TYPE_SENSOR, SCD30_REV, "", 0, "", 0, "", 0, "", 0)
// all functions must be declared MUDULE_PART
MODULE_PART int32_t SCD30_Detect();
MODULE_PART void SCD30_Show(bool json);
MODULE_PART void SCD30_Update();
MODULE_PART void CmndScd30Altitude();
MODULE_PART void CmndScd30AutoMode();
MODULE_PART void CmndScd30Calibrate();
MODULE_PART void CmndScd30Firmware();
MODULE_PART void CmndScd30Interval();
MODULE_PART void CmndScd30Pressure();
MODULE_PART void CmndScd30TempOffset();
MODULE_PART void SCD30_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);

MODULE_END

/********************************************************************************************/

typedef struct {
  float humidity;
  float temperature;
  int error_state;
  int loop_count;
  int data_not_available_count;
  int good_measure_count;
  int reset_count;
  int error_count;
  int co2_zero_count;
  int i2c_reset_count;
  uint16_t interval;
  uint16_t co2;
  uint16_t co2e_avg;
  bool init_once;
  bool data_valid;
} SCD30;

#define SCD30_MEDIAN_FILTER_SIZE 5

typedef struct {
  uint8_t i2cAddress;
  // TwoWire *pWire;
  uint16_t ambientPressure;
  uint16_t co2History[SCD30_MEDIAN_FILTER_SIZE];
  uint16_t co2EAverage;
  int8_t co2NewDataLocation;  // location to put new CO2 data for median filter
} DRV;

typedef struct {
  TwoWire *xWire;
  uint8_t ready;
  SCD30 Scd30;
  DRV drv;
} MODULE_MEMORY;

#define ready mem->ready
#define Scd30 mem->Scd30
#define drv mem->drv

// SCD30 driver

MODULE_PART void SCD30_begin(uint8_t addr);

MODULE_PART int SCD30_softReset();
MODULE_PART int
SCD30_clearI2CBus();  // this is a HARD reset of the IC2 bus to restore communication, it will disrupt the bus

MODULE_PART int SCD30_getAltitudeCompensation(uint16_t *pHeight_meter);
MODULE_PART int SCD30_getAmbientPressure(uint16_t *pAirPressure_mbar);
MODULE_PART int SCD30_getCalibrationType(uint16_t *pIsAuto);
MODULE_PART int SCD30_getFirmwareVersion(uint8_t *pMajor, uint8_t *pMinor);
MODULE_PART int SCD30_getForcedRecalibrationFactor(uint16_t *pCo2_ppm);
MODULE_PART int SCD30_getMeasurementInterval(uint16_t *pTime_sec);
MODULE_PART int SCD30_getTemperatureOffset(float *pOffset_degC);
MODULE_PART int SCD30_getTemperatureOffset(uint16_t *pOffset_centiDegC);

MODULE_PART int SCD30_setAltitudeCompensation(uint16_t height_meter);
MODULE_PART int SCD30_setAmbientPressure(uint16_t airPressure_mbar);
MODULE_PART int SCD30_setAutoSelfCalibration();
MODULE_PART int SCD30_setCalibrationType(bool isAuto);
MODULE_PART int SCD30_setForcedRecalibrationFactor(uint16_t co2_ppm);
MODULE_PART int SCD30_setManualCalibration();
MODULE_PART int SCD30_setMeasurementInterval(uint16_t time_sec);
MODULE_PART int SCD30_setTemperatureOffset(float offset_degC);
MODULE_PART int SCD30_setTemperatureOffset(uint16_t offset_centiDegC);

MODULE_PART int SCD30_beginMeasuring();
MODULE_PART int SCD30_beginMeasuring(uint16_t airPressure_mbar);  // also sets ambient pressure offset in mbar/hPascal
MODULE_PART int SCD30_isDataAvailable(bool *pIsAvailable);
MODULE_PART int SCD30_readMeasurement(uint16 *pCO2_ppm, uint16 *pCO2EAvg_ppm, float *pTemperature, float *pHumidity);
MODULE_PART int SCD30_stopMeasuring();

MODULE_PART uint8_t SCD30_computeCRC8(uint8_t data[], uint8_t len);
MODULE_PART int SCD30_sendBytes(void *pInput, uint8_t len);
MODULE_PART int SCD30_getBytes(void *pOutput, uint8_t len);
MODULE_PART int SCD30_sendCommand(uint16_t command);
MODULE_PART int SCD30_sendCommandArguments(uint16_t command, uint16_t arguments);
MODULE_PART int SCD30_get16BitRegCheckCRC(void *pInput, uint16_t *pData);
MODULE_PART int SCD30_get32BitRegCheckCRC(void *pInput, float *pData);
MODULE_PART int SCD30_sendCommand(uint16_t registerAddress, uint16_t *pData);
MODULE_PART int SCD30_readRegister(uint16_t registerAddress, uint16_t *pData);
MODULE_PART uint16_t SCD30_opt_med5(uint16_t *p);

#define COMMAND_SCD30_CONTINUOUS_MEASUREMENT 0x0010
#define COMMAND_SCD30_MEASUREMENT_INTERVAL 0x4600
#define COMMAND_SCD30_GET_DATA_READY 0x0202
#define COMMAND_SCD30_READ_MEASUREMENT 0x0300
#define COMMAND_SCD30_CALIBRATION_TYPE 0x5306
#define COMMAND_SCD30_FORCED_RECALIBRATION_FACTOR 0x5204
#define COMMAND_SCD30_TEMPERATURE_OFFSET 0x5403
#define COMMAND_SCD30_ALTITUDE_COMPENSATION 0x5102
#define COMMAND_SCD30_SOFT_RESET 0xD304
#define COMMAND_SCD30_GET_FW_VERSION 0xD100
#define COMMAND_SCD30_STOP_MEASUREMENT 0x0104

#define SCD30_DATA_REGISTER_BYTES 2
#define SCD30_DATA_REGISTER_WITH_CRC 3
#define SCD30_MEAS_BYTES 18

void SCD30_begin(uint8_t addr) {
  SETREGS
  drv.i2cAddress = addr;
  drv.co2NewDataLocation = -1;  // indicates there is no data, so the 1st data point needs to fill up the median filter
#ifdef ESP8266
//    this->pWire->setClockStretchLimit(200000);
#endif
  drv.ambientPressure = 0;
}

/*---------------------------------------------------------------------------
 Function : opt_med5() In : pointer to array of 5 values
 Out : a uint16_t which is the middle value of the sorted array
 Job : optimized search of the median of 5 values
 Notice : found on sci.image.processing cannot go faster unless assumptions are made on the nature of the input signal.
 ---------------------------------------------------------------------------*/
#define PIX_SORT(a, b)                 \
  {                                    \
    if ((a) > (b)) PIX_SWAP((a), (b)); \
  }
#define PIX_SWAP(a, b)   \
  {                      \
    uint16_t temp = (a); \
    (a) = (b);           \
    (b) = temp;          \
  }

uint16_t SCD30_opt_med5(uint16_t *p) {
  PIX_SORT(p[0], p[1]);
  PIX_SORT(p[3], p[4]);
  PIX_SORT(p[0], p[3]);
  PIX_SORT(p[1], p[4]);
  PIX_SORT(p[1], p[2]);
  PIX_SORT(p[2], p[3]);
  PIX_SORT(p[1], p[2]);
  return (p[2]);
}

// twi_status() attempts to read out any data left that is holding SDA low, so a new transaction can take place
// something like (http://www.forward.com.au/pfod/ArduinoProgramming/I2C_ClearBus/index.html)
int SCD30_clearI2CBus() {
  /*
  #ifdef ESP8266
      return (twi_status());
  #else
      return 0;
  #endif
  */
  return 0;
}

uint8_t SCD30_computeCRC8(uint8_t data[], uint8_t len) {
  // Computes the CRC that the SCD30 uses
  uint8_t crc = 0xFF;  // Init with 0xFF

  for (uint8_t x = 0; x < len; x++) {
    crc ^= data[x];  // XOR-in the next input byte
    for (uint8_t i = 0; i < 8; i++) {
      if ((crc & 0x80) != 0)
        crc = (uint8_t)((crc << 1) ^ 0x31);
      else
        crc <<= 1;
    }
  }
  return crc;  // No output reflection
}

// Sends stream of bytes to device
int SCD30_sendBytes(void *pInput, uint8_t len) {
  SETREGS
  uint8_t *pBytes = (uint8_t *)pInput;
  int result;
  uint8_t errorBytes = 0;
  beginTransmission(drv.i2cAddress);
  for (uint8_t cnt = 0; cnt < len; cnt++) {
    write(pBytes[cnt]);
  }
  // errorBytes = len - (write(pBytes, len));
  result = endTransmission(true);
  result <<= 8;
  result |= errorBytes;
  return (result);
}

// Gets a number of bytes from device
int SCD30_getBytes(void *pOutput, uint8_t len) {
  SETREGS
  uint8_t *pBytes = (uint8_t *)pOutput;
  uint8_t result;

  result = requestFrom(drv.i2cAddress, len);
  if (len != result) {
    return (ERROR_SCD30_NOT_ENOUGH_BYTES_ERROR);
  }

  if (available()) {
    for (int x = 0; x < len; x++) {
      pBytes[x] = read();
    }
    return (ERROR_SCD30_NO_ERROR);
  }

  return (ERROR_SCD30_UNKNOWN_ERROR);
}

// Sends just a command, no arguments, no CRC
int SCD30_sendCommand(uint16_t command) {
  SETREGS
  uint8_t data[2];
  data[0] = command >> 8;
  data[1] = command & 0xFF;
  int error = SCD30_sendBytes(data, sizeof(data));
  return error;
}

// Sends a command along with arguments and CRC
int SCD30_sendCommandArguments(uint16_t command, uint16_t arguments) {
  SETREGS
  uint8_t data[5];
  data[0] = command >> 8;
  data[1] = command & 0xFF;
  data[2] = arguments >> 8;
  data[3] = arguments & 0xFF;
  data[4] = SCD30_computeCRC8(&data[2], 2);  // Calc CRC on the arguments only, not the command
  int error = SCD30_sendBytes(data, sizeof(data));
  return error;
}

int SCD30_get16BitRegCheckCRC(void *pInput, uint16_t *pData) {
  SETREGS
  uint8_t *pBytes = (uint8_t *)pInput;
  uint8_t expectedCRC = SCD30_computeCRC8(pBytes, SCD30_DATA_REGISTER_BYTES);
  if (expectedCRC != pBytes[SCD30_DATA_REGISTER_BYTES]) {
    return (ERROR_SCD30_CRC_ERROR);
  }
  *pData = (uint16_t)pBytes[0] << 8 | pBytes[1];  // data from SCD30 is Big-Endian
  return (ERROR_SCD30_NO_ERROR);
}

// gets 32 bits, (2) 16-bit chunks, and validates the CRCs
//
int SCD30_get32BitRegCheckCRC(void *pInput, float *pData) {
  SETREGS
  uint16_t tempU16High;
  uint16_t tempU16Low;
  uint8_t *pBytes = (uint8_t *)pInput;
  uint32_t rawInt = 0;

  int error = SCD30_get16BitRegCheckCRC(pBytes, &tempU16High);
  if (error) {
    return (error);
  }

  error = SCD30_get16BitRegCheckCRC(pBytes + SCD30_DATA_REGISTER_WITH_CRC, &tempU16Low);
  if (error) {
    return (error);
  }

  // data from SCD is Big-Endian
  rawInt |= tempU16High;
  rawInt <<= 16;
  rawInt |= tempU16Low;

  *pData = *(float *)&rawInt;

  if (isnan(*pData) || isinf(*pData)) {
    return (ERROR_SCD30_NOT_A_NUMBER_ERROR);
  }

  return (ERROR_SCD30_NO_ERROR);
}

// Gets two bytes (and check CRC) from SCD30
int SCD30_readRegister(uint16_t registerAddress, uint16_t *pData) {
  SETREGS
  int error = SCD30_sendCommand(registerAddress);
  if (error) {
    return (error);
  }
  delay(1);  // the SCD30 uses clock streching to give it time to prepare data, waiting here makes it work
  uint8_t data[SCD30_DATA_REGISTER_WITH_CRC];
  error = SCD30_getBytes(data, sizeof(data));
  if (error) {
    return (error);
  }
  uint16 regValue;
  error = SCD30_get16BitRegCheckCRC(data, &regValue);
  if (error) {
    return (error);
  }

  *pData = regValue;
  return (ERROR_SCD30_NO_ERROR);
}

int SCD30_softReset() {
  SETREGS
  return (SCD30_sendCommand(COMMAND_SCD30_SOFT_RESET));
}

int SCD30_getAltitudeCompensation(uint16_t *pHeight_meter) {
  SETREGS
  return (SCD30_readRegister(COMMAND_SCD30_ALTITUDE_COMPENSATION, pHeight_meter));
}

int SCD30_getAmbientPressure(uint16_t *pAirPressure_mbar) {
  SETREGS
  *pAirPressure_mbar = drv.ambientPressure;
  return (ERROR_SCD30_NO_ERROR);
}

int SCD30_getCalibrationType(uint16_t *pIsAuto) {
  SETREGS
  uint16_t value = 0;
  int error = SCD30_readRegister(COMMAND_SCD30_CALIBRATION_TYPE, &value);
  if (!error) {
    *pIsAuto = value != 0;
  }
  return (error);
}

int SCD30_getFirmwareVersion(uint8_t *pMajor, uint8_t *pMinor) {
  SETREGS
  uint16_t value;
  int error = SCD30_readRegister(COMMAND_SCD30_GET_FW_VERSION, &value);
  if (!error) {
    *pMajor = value >> 8;
    *pMinor = value & 0xFF;
  }
  return (error);
}

int SCD30_getForcedRecalibrationFactor(uint16_t *pCo2_ppm) {
  SETREGS
  return (SCD30_readRegister(COMMAND_SCD30_FORCED_RECALIBRATION_FACTOR, pCo2_ppm));
}

int SCD30_getMeasurementInterval(uint16_t *pTime_sec) {
  SETREGS
  return (SCD30_readRegister(COMMAND_SCD30_MEASUREMENT_INTERVAL, pTime_sec));
}

int SCD30_getTemperatureOffset(float *pOffset_degC) {
  SETREGS
  uint16_t value;
  int error = SCD30_readRegister(COMMAND_SCD30_TEMPERATURE_OFFSET, &value);
  if (!error) {
    // result is in centi-degrees, need to convert to degrees
    //*pOffset_degC = (float) value / 100.0;
    *pOffset_degC = fscale(value, 0.01, 0);
  }
  return (error);
}

int SCD30_getTemperatureOffset(uint16_t *pOffset_centiDegC) {
  SETREGS
  uint16_t value;
  int error = SCD30_readRegister(COMMAND_SCD30_TEMPERATURE_OFFSET, &value);
  if (!error) {
    // result is in centi-degrees, need to convert to degrees
    *pOffset_centiDegC = value;
  }
  return (error);
}

int SCD30_setAltitudeCompensation(uint16_t height_meter) {
  SETREGS
  return (SCD30_sendCommandArguments(COMMAND_SCD30_ALTITUDE_COMPENSATION, height_meter));
}

int SCD30_setAmbientPressure(uint16_t airPressure_mbar) {
  SETREGS
  drv.ambientPressure = airPressure_mbar;
  return 0;
  // return (SCD30_beginMeasuring(drv.ambientPressure));
}

int SCD30_setAutoSelfCalibration() {
  SETREGS
  bool isAuto = true;
  return (SCD30_setCalibrationType(isAuto));
}

int SCD30_setCalibrationType(bool isAuto) {
  SETREGS
  bool value = !!isAuto;  // using NOT operator twice makes sure value is 0 or 1
  return (SCD30_sendCommandArguments(COMMAND_SCD30_CALIBRATION_TYPE, value));
}

int SCD30_setForcedRecalibrationFactor(uint16_t co2_ppm) {
  SETREGS
  return (SCD30_sendCommandArguments(COMMAND_SCD30_FORCED_RECALIBRATION_FACTOR, co2_ppm));
}

int SCD30_setManualCalibration() {
  SETREGS
  bool isAuto = false;
  return (SCD30_setCalibrationType(isAuto));
}

int SCD30_setMeasurementInterval(uint16_t time_sec) {
  SETREGS
  if (time_sec < 2) time_sec = 2;
  if (time_sec > 1800) time_sec = 1800;
  return (SCD30_sendCommandArguments(COMMAND_SCD30_MEASUREMENT_INTERVAL, time_sec));
}

int SCD30_setTemperatureOffset(float offset_degC) {
  SETREGS
  uint16_t offset_centiDegC;
  // if (offset_degC >= 0) {
  if (jgtsf2(offset_degC, 0)) {
    // offset_centiDegC = (uint16_t) offset_degC * 100;
    offset_centiDegC = tmod__fixunssfsi(offset_degC) * 100;
    return (SCD30_sendCommandArguments(COMMAND_SCD30_TEMPERATURE_OFFSET, offset_centiDegC));
  } else {
    return (ERROR_SCD30_INVALID_VALUE);
  }
}

int SCD30_setTemperatureOffset(uint16_t offset_centiDegC) {
  SETREGS
  return (SCD30_sendCommandArguments(COMMAND_SCD30_TEMPERATURE_OFFSET, offset_centiDegC));
}

int SCD30_beginMeasuring() {
  SETREGS
  return (SCD30_sendCommandArguments(COMMAND_SCD30_CONTINUOUS_MEASUREMENT, drv.ambientPressure));
  // return (SCD30_beginMeasuring(drv.ambientPressure));
}

int SCD30_beginMeasuring(uint16_t airPressure_mbar) {
  SETREGS
  drv.ambientPressure = airPressure_mbar;
  return (SCD30_sendCommandArguments(COMMAND_SCD30_CONTINUOUS_MEASUREMENT, drv.ambientPressure));
}

int SCD30_isDataAvailable(bool *pIsAvailable) {
  SETREGS
  uint16_t isDataAvailable = false;
  int error = SCD30_readRegister(COMMAND_SCD30_GET_DATA_READY, &isDataAvailable);
  if (!error) {
    *pIsAvailable = isDataAvailable != 0;
  }
  return (error);
}

int SCD30_readMeasurement(uint16 *pCO2_ppm, uint16 *pCO2EAvg_ppm, float *pTemperature, float *pHumidity) {
  SETREGS
  bool isAvailable = false;
  int error = 0;
  float tempCO2;
  float tempHumidity;
  float tempTemperature;

  error = SCD30_isDataAvailable(&isAvailable);
  if (error) {
    return (error);
  }

  if (!isAvailable) {
    return (ERROR_SCD30_NO_DATA);
  }

  error = SCD30_sendCommand(COMMAND_SCD30_READ_MEASUREMENT);
  if (error) {
    return (error);
  }
  delay(1);  // the SCD30 uses clock streching to give it time to prepare data, waiting here makes it work

  uint8_t bytes[SCD30_MEAS_BYTES];
  // there are (6) 16-bit values, each with a CRC in the measurement data
  // the chip does not seem to like sending this data, except all at once
  error = SCD30_getBytes(bytes, SCD30_MEAS_BYTES);
  if (error) {
    return (error);
  }

  error = SCD30_get32BitRegCheckCRC(&bytes[0], &tempCO2);
  if (error) {
    return (error);
  }

  error = SCD30_get32BitRegCheckCRC(&bytes[6], &tempTemperature);
  if (error) {
    return (error);
  }

  error = SCD30_get32BitRegCheckCRC(&bytes[12], &tempHumidity);
  if (error) {
    return (error);
  }

  // if (tempCO2 == 0) {
  if (jiseq(tempCO2)) {
    return (ERROR_SCD30_CO2_ZERO);
  }

  if (drv.co2NewDataLocation < 0) {
    // drv.co2EAverage =  tempCO2;
    drv.co2EAverage = jtmod__fixunssfsi(tempCO2);
    for (int x = 0; x < SCD30_MEDIAN_FILTER_SIZE; x++) {
      // drv.co2History[x] = tempCO2;
      drv.co2History[x] = tmod__fixunssfsi(tempCO2);
      drv.co2NewDataLocation = 1;
    }
  } else {
    drv.co2History[drv.co2NewDataLocation++] = tmod__fixunssfsi(tempCO2);
    if (drv.co2NewDataLocation >= SCD30_MEDIAN_FILTER_SIZE) {
      drv.co2NewDataLocation = 0;
    }
  }

  // copy array since the median filter function will re-arrange it
  uint16_t temp[SCD30_MEDIAN_FILTER_SIZE];
  for (int x = 0; x < SCD30_MEDIAN_FILTER_SIZE; x++) {
    temp[x] = drv.co2History[x];
  }

  *pCO2_ppm = SCD30_opt_med5(temp);
  if (pCO2EAvg_ppm) {
    int16_t delta = (int16_t)*pCO2_ppm - (int16_t)drv.co2EAverage;
    int16_t change = delta / 32;
    drv.co2EAverage += change;
    *pCO2EAvg_ppm = drv.co2EAverage;
  }

  *pTemperature = tempTemperature;
  *pHumidity = tempHumidity;
  return (ERROR_SCD30_NO_ERROR);
}

int SCD30_stopMeasuring() {
  SETREGS
  return (SCD30_sendCommand(COMMAND_SCD30_STOP_MEASUREMENT));
}

// end driver section

int32_t SCD30_Detect() {
  ALLOCMEM

  SETWIRE(0);

  ready = false;

  Scd30.data_valid = false;
  initialized = false;

  if (I2cSetDevice(SCD30_ADDRESS, 0)) {
    SCD30_begin(SCD30_ADDRESS);

    uint8_t major = 0;
    uint8_t minor = 0;

    if (SCD30_getFirmwareVersion(&major, &minor)) {
      goto exit;
    }

    if (SCD30_getMeasurementInterval(&Scd30.interval)) {
      goto exit;
    }
    if (SCD30_beginMeasuring()) {
      goto exit;
    }

    AddLog(LOG_LEVEL_INFO, PSTR("SCD30: FW v%d.%d"), major, minor);
    I2cSetActiveFound(SCD30_ADDRESS, PSTR("SCD30"), 0);
    initialized = 1;
    ready = true;
    return ready;
  }

exit:
  SCD30_Deinit();
  return ready;
}

// gets data from the sensor every 3 seconds or so to give the sensor time to gather new data
void SCD30_Update() {
  SETREGS
  Scd30.loop_count++;
  if (Scd30.loop_count > (Scd30.interval - 1)) {
    uint32_t error = 0;
    switch (Scd30.error_state) {
      case SCD30_STATE_NO_ERROR: {
        error = SCD30_readMeasurement(&Scd30.co2, &Scd30.co2e_avg, &Scd30.temperature, &Scd30.humidity);
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
            AddLog(LOG_LEVEL_ERROR,
                   PSTR("SCD30: CRC error, CRC error: %ld, CO2 zero: %ld, good: %ld, no data: %ld, sc30_reset: %ld, "
                        "i2c_reset: %ld"),
                   Scd30.error_count, Scd30.co2_zero_count, Scd30.good_measure_count, Scd30.data_not_available_count,
                   Scd30.reset_count, Scd30.i2c_reset_count);
#endif
            break;

          case ERROR_SCD30_CO2_ZERO:
            Scd30.co2_zero_count++;
#ifdef SCD30_DEBUG
            AddLog(LOG_LEVEL_ERROR,
                   PSTR("SCD30: CO2 zero, CRC error: %ld, CO2 zero: %ld, good: %ld, no data: %ld, sc30_reset: %ld, "
                        "i2c_reset: %ld"),
                   Scd30.error_count, Scd30.co2_zero_count, Scd30.good_measure_count, Scd30.data_not_available_count,
                   Scd30.reset_count, Scd30.i2c_reset_count);
#endif
            break;

          default: {
            Scd30.error_state = SCD30_STATE_ERROR_READ_MEAS;
#ifdef SCD30_DEBUG
            AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: Update: ReadMeasurement error: 0x%lX, counter: %ld"), error,
                   Scd30.loop_count);
#endif
            return;
          } break;
        }
      } break;

      case SCD30_STATE_ERROR_DATA_CRC: {
        // Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR,
               PSTR("SCD30: in error state: %d, good: %ld, no data: %ld, sc30 reset: %ld, i2c reset: %ld"),
               Scd30.error_state, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count,
               Scd30.i2c_reset_count);
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: got CRC error, try again, counter: %ld"), Scd30.loop_count);
#endif
        Scd30.error_state = ERROR_SCD30_NO_ERROR;
      } break;

      case SCD30_STATE_ERROR_READ_MEAS: {
        // Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR,
               PSTR("SCD30: in error state: %d, good: %ld, no data: %ld, sc30 reset: %ld, i2c reset: %ld"),
               Scd30.error_state, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count,
               Scd30.i2c_reset_count);
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: not answering, sending soft reset, counter: %ld"), Scd30.loop_count);
#endif
        Scd30.reset_count++;
        error = SCD30_softReset();
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
      } break;

      case SCD30_STATE_ERROR_SOFT_RESET: {
        // Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR,
               PSTR("SCD30: in error state: %d, good: %ld, no data: %ld, sc30 reset: %ld, i2c reset: %ld"),
               Scd30.error_state, Scd30.good_measure_count, Scd30.data_not_available_count, Scd30.reset_count,
               Scd30.i2c_reset_count);
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: clearing i2c bus"));
#endif
        Scd30.i2c_reset_count++;
        error = SCD30_clearI2CBus();
        if (error) {
          Scd30.error_state = SCD30_STATE_ERROR_I2C_RESET;
#ifdef SCD30_DEBUG
          AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: error clearing i2c bus: 0x%lX"), error);
#endif
        } else {
          Scd30.error_state = ERROR_SCD30_NO_ERROR;
        }
      } break;

      default: {
        // Scd30.data_valid = false;
#ifdef SCD30_DEBUG
        AddLog(LOG_LEVEL_ERROR, PSTR("SCD30: unknown error state: 0x%lX"), Scd30.error_state);
#endif
        Scd30.error_state = SCD30_STATE_ERROR_SOFT_RESET;  // try again
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
const char kScd30Commands[] PROGMEM =
    "Scd30|"  // Prefix
    "Alt|Auto|Cal|FW|Int|Pres|TOff";
void (*const kScd30Command[])(void) PROGMEM = {&CmndScd30Altitude,  &CmndScd30AutoMode, &CmndScd30Calibrate,
                                               &CmndScd30Firmware,  &CmndScd30Interval, &CmndScd30Pressure,
                                               &CmndScd30TempOffset};

void CmndScd30Altitude() {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    SCD30_setAltitudeCompensation(value);
  } else {
    SCD30_getAltitudeCompensation(&value);
  }
  ResponseCmndNumber(value);
};

void CmndScd30AutoMode() {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    SCD30_setCalibrationType(value);
  } else {
    SCD30_getCalibrationType(&value);
  }
  ResponseCmndNumber(value);
};

void CmndScd30Calibrate() {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    SCD30_setForcedRecalibrationFactor(value);
  } else {
    SCD30_getForcedRecalibrationFactor(&value);
  }
  ResponseCmndNumber(value);
};

void CmndScd30Firmware() {
  SETREGS
  uint8_t major = 0;
  uint8_t minor = 0;
  int error = 0;
  SCD30_getFirmwareVersion(&major, &minor);
  if (!error) {
    // float firmware = major + ((float)minor / 100);
    // float firmware = fscale(minor, 0.01, tmod__floatsisf(major));
    // ResponseCmndFloat(firmware, 2);
    uint16_t value = major << 8 | minor;
    ResponseCmndNumber(value);
  }
};

void CmndScd30Interval() {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    int error = 0;
    SCD30_setMeasurementInterval(value);
    if (!error) {
      Scd30.interval = value;
    }
  }
  SCD30_getMeasurementInterval(&value);
  ResponseCmndNumber(value);
};

void CmndScd30Pressure() {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    SCD30_setAmbientPressure(value);
  } else {
    SCD30_getAmbientPressure(&value);
  }
  ResponseCmndNumber(value);
};

void CmndScd30TempOffset() {
  SETREGS
  uint16_t value = 0;
  if (XdrvMailbox->data_len > 0) {
    value = XdrvMailbox->payload;
    SCD30_setTemperatureOffset(value);
  } else {
    SCD30_getTemperatureOffset(&value);
  }
  ResponseCmndNumber(value);
};

/********************************************************************************************/

const char HTTP_SNS_CO2[] PROGMEM = "{s}%s CO2{m}%d ppm{e}";
const char HTTP_SNS_CO2EAVG[] PROGMEM = "{s}%s eCO2{m}%d ppm{e}";

void SCD30_Show(bool json) {
  SETREGS

  if (Scd30.data_valid) {
    float t = ConvertTemp(Scd30.temperature);
    float h = ConvertHumidity(Scd30.humidity);

    if (json) {
      ResponseAppend_P(PSTR(",\"SCD30\":{\"Carbondioxide\":%d,\" eCO2\":%d,"), Scd30.co2, Scd30.co2e_avg);
      ResponseAppendTHD(t, h);
      ResponseJsonEnd();
    } else {
      WSContentSend_PD(GSTR(HTTP_SNS_CO2EAVG), PSTR("SCD30"), Scd30.co2e_avg);
      WSContentSend_PD(GSTR(HTTP_SNS_CO2), PSTR("SCD30"), Scd30.co2);
      WSContentSend_THD(PSTR("SCD30"), t, h);
    }
  }
}

void SCD30_Deinit() {
  SETREGS
  I2cResetActive(SCD30_ADDRESS, 0);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;

  // https://github.com/arendst/Tasmota/issues/15438 and datasheet (The boot-up time is < 2 s.)
  /*
    if (FUNC_INIT == function) {
      Scd30Detect();
    }
  */
  switch (sel) {
    case FUNC_INIT:
      result = SCD30_Detect();
      break;
    case FUNC_EVERY_SECOND:
      SCD30_Update();
      break;
    case FUNC_COMMAND: {
      SETREGS
      result = DecodeCommand(kScd30Commands, kScd30Command);
    } break;
    case FUNC_JSON_APPEND:
      SCD30_Show(1);
      break;
    case FUNC_WEB_SENSOR:
      SCD30_Show(0);
      break;
    case FUNC_DEINIT:
      SCD30_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_SCD30_MOD
