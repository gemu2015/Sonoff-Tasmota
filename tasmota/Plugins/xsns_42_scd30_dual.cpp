/*
  xsns_42_scd30_dual.cpp — Sensirion SCD30 NDIR CO2 + temperature +
  humidity sensor driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2021 Frogmore42

  Plugin: USE_SCD30_DUAL_MOD via build_plugin.py.
  Native: USE_SCD30_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_42_scd30_dual.ino.

  Single fixed I2C address (0x61). Probes both buses.

  Console commands (prefix `Scd30`):
    Scd30Alt   — altitude compensation (m)
    Scd30Auto  — auto self-calibration on (1) / off (0)
    Scd30Cal   — forced recalibration to a known CO2 (ppm)
    Scd30FW    — firmware version (read-only, returns major:minor as u16)
    Scd30Int   — measurement interval (seconds, 2..1800)
    Scd30Pres  — ambient-pressure compensation (mbar)
    Scd30TOff  — temperature offset (centi-degrees)

  Error-state machine recovers from I2C-level CRC errors, no-data
  timeouts, and stuck-bus situations via SCD30_softReset and
  SCD30_clearI2CBus. Full error counters in the state struct for
  diagnostics — peek with SCD30_DEBUG enabled at compile time.
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_SCD30_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  ifdef USE_SCD30_DUAL_MOD
#    define _SCD30_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_I2C) && defined(USE_SCD30_DUAL) && defined(SCD30_DUAL_NATIVE_INCLUDE)
#    define _SCD30_DUAL_ENABLED 1
#  endif
#endif

#ifdef _SCD30_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define SCD30_ADDRESS                          0x61
#define SCD30_REV                              (1 << 16 | 5)
#define SCD30_MAX_MISSED_READS                 3

// Error-state machine values
#define SCD30_STATE_NO_ERROR                   0
#define SCD30_STATE_ERROR_DATA_CRC             1
#define SCD30_STATE_ERROR_READ_MEAS            2
#define SCD30_STATE_ERROR_SOFT_RESET           3
#define SCD30_STATE_ERROR_I2C_RESET            4
#define SCD30_STATE_ERROR_UNKNOWN              5

// Return codes (high-bit-set values are "expected" outcomes,
// not real errors — they distinguish e.g. "no data yet" from "CRC
// failed").
#define ERROR_SCD30_NO_ERROR                   0
#define ERROR_SCD30_NO_DATA                    0x80000000
#define ERROR_SCD30_CO2_ZERO                   0x90000000
#define ERROR_SCD30_UNKNOWN_ERROR              0x1000000
#define ERROR_SCD30_CRC_ERROR                  0x2000000
#define ERROR_SCD30_NOT_ENOUGH_BYTES_ERROR     0x3000000
#define ERROR_SCD30_NOT_FOUND_ERROR            0x4000000
#define ERROR_SCD30_NOT_A_NUMBER_ERROR         0x5000000
#define ERROR_SCD30_INVALID_VALUE              0x6000000

// I2C command words (big-endian on the wire)
#define COMMAND_SCD30_CONTINUOUS_MEASUREMENT       0x0010
#define COMMAND_SCD30_MEASUREMENT_INTERVAL         0x4600
#define COMMAND_SCD30_GET_DATA_READY               0x0202
#define COMMAND_SCD30_READ_MEASUREMENT             0x0300
#define COMMAND_SCD30_CALIBRATION_TYPE             0x5306
#define COMMAND_SCD30_FORCED_RECALIBRATION_FACTOR  0x5204
#define COMMAND_SCD30_TEMPERATURE_OFFSET           0x5403
#define COMMAND_SCD30_ALTITUDE_COMPENSATION        0x5102
#define COMMAND_SCD30_SOFT_RESET                   0xD304
#define COMMAND_SCD30_GET_FW_VERSION               0xD100
#define COMMAND_SCD30_STOP_MEASUREMENT             0x0104

#define SCD30_DATA_REGISTER_BYTES              2
#define SCD30_DATA_REGISTER_WITH_CRC           3
#define SCD30_MEAS_BYTES                       18
#define SCD30_MEDIAN_FILTER_SIZE               5

// --------------------------------------------------------------------
// Plugin descriptor block — written ONCE, no `#if` gate. Native sees
// empty macros; plugin sees full FLASH_MODULE struct + section-
// attributed forward decls.
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("SCD30", MODULE_TYPE_SENSOR, SCD30_REV,
                  "", 0, "", 0, "", 0, "", 0)
MODULE_PART int32_t  SCD30_Detect(void);
MODULE_PART void     SCD30_Show(bool json);
MODULE_PART void     SCD30_Update(void);
MODULE_PART void     SCD30_Deinit(void);
MODULE_PART void     SCD30_begin(uint8_t addr);
MODULE_PART int      SCD30_softReset(void);
MODULE_PART int      SCD30_clearI2CBus(void);
MODULE_PART int      SCD30_getAltitudeCompensation(uint16_t *pHeight_meter);
MODULE_PART int      SCD30_getAmbientPressure(uint16_t *pAirPressure_mbar);
MODULE_PART int      SCD30_getCalibrationType(uint16_t *pIsAuto);
MODULE_PART int      SCD30_getFirmwareVersion(uint8_t *pMajor, uint8_t *pMinor);
MODULE_PART int      SCD30_getForcedRecalibrationFactor(uint16_t *pCo2_ppm);
MODULE_PART int      SCD30_getMeasurementInterval(uint16_t *pTime_sec);
MODULE_PART int      SCD30_getTemperatureOffset(float *pOffset_degC);
MODULE_PART int      SCD30_getTemperatureOffset(uint16_t *pOffset_centiDegC);
MODULE_PART int      SCD30_setAltitudeCompensation(uint16_t height_meter);
MODULE_PART int      SCD30_setAmbientPressure(uint16_t airPressure_mbar);
MODULE_PART int      SCD30_setAutoSelfCalibration(void);
MODULE_PART int      SCD30_setCalibrationType(bool isAuto);
MODULE_PART int      SCD30_setForcedRecalibrationFactor(uint16_t co2_ppm);
MODULE_PART int      SCD30_setManualCalibration(void);
MODULE_PART int      SCD30_setMeasurementInterval(uint16_t time_sec);
MODULE_PART int      SCD30_setTemperatureOffset(float offset_degC);
MODULE_PART int      SCD30_setTemperatureOffset(uint16_t offset_centiDegC);
MODULE_PART int      SCD30_beginMeasuring(void);
MODULE_PART int      SCD30_beginMeasuring(uint16_t airPressure_mbar);
MODULE_PART int      SCD30_isDataAvailable(bool *pIsAvailable);
MODULE_PART int      SCD30_readMeasurement(uint16_t *pCO2_ppm, uint16_t *pCO2EAvg_ppm,
                                            float *pTemperature, float *pHumidity);
MODULE_PART int      SCD30_stopMeasuring(void);
MODULE_PART uint8_t  SCD30_computeCRC8(uint8_t data[], uint8_t len);
MODULE_PART int      SCD30_sendBytes(void *pInput, uint8_t len);
MODULE_PART int      SCD30_getBytes(void *pOutput, uint8_t len);
MODULE_PART int      SCD30_sendCommand(uint16_t command);
MODULE_PART int      SCD30_sendCommandArguments(uint16_t command, uint16_t arguments);
MODULE_PART int      SCD30_get16BitRegCheckCRC(void *pInput, uint16_t *pData);
MODULE_PART int      SCD30_get32BitRegCheckCRC(void *pInput, float *pData);
MODULE_PART int      SCD30_readRegister(uint16_t registerAddress, uint16_t *pData);
MODULE_PART uint16_t SCD30_opt_med5(uint16_t *p);
MODULE_PART void     CmndScd30Altitude(void);
MODULE_PART void     CmndScd30AutoMode(void);
MODULE_PART void     CmndScd30Calibrate(void);
MODULE_PART void     CmndScd30Firmware(void);
MODULE_PART void     CmndScd30Interval(void);
MODULE_PART void     CmndScd30Pressure(void);
MODULE_PART void     CmndScd30TempOffset(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t sel);
#endif
MODULE_END

// --------------------------------------------------------------------
// Measurement + driver-level state
// --------------------------------------------------------------------
typedef struct {
  float    humidity;
  float    temperature;
  int      error_state;
  int      loop_count;
  int      data_not_available_count;
  int      good_measure_count;
  int      reset_count;
  int      error_count;
  int      co2_zero_count;
  int      i2c_reset_count;
  uint16_t interval;
  uint16_t co2;
  uint16_t co2e_avg;
  bool     init_once;
  bool     data_valid;
} SCD30;

typedef struct {
  uint8_t  i2cAddress;
  uint16_t ambientPressure;
  uint16_t co2History[SCD30_MEDIAN_FILTER_SIZE];
  uint16_t co2EAverage;
  int8_t   co2NewDataLocation;   // -1 = first reading not yet seen
} DRV;

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
// Unified MODULE_MEMORY for plugin + native (see xsns_09_bmp_dual.cpp).
#if !BUILD_AS_PLUGIN
#  define MODULE_MEMORY  scd30_state_t
#endif

typedef struct {
  TWIp   *xWire;
  uint8_t ready;
  uint8_t bus;
  SCD30   Scd30;
  DRV     drv;
  bool    initialized_flag;
} MODULE_MEMORY;

#define ready    mem->ready
#define scd_bus  mem->bus
#define Scd30    mem->Scd30
#define drv      mem->drv

#if !BUILD_AS_PLUGIN

static scd30_state_t *scd30_state = nullptr;

#  undef  SETREGS
#  define SETREGS    MODULE_MEMORY *mem = scd30_state;
#  define ALLOCMEM \
       if (!scd30_state) scd30_state = (scd30_state_t *)calloc(1, sizeof(scd30_state_t)); \
       if (!scd30_state) return -1; \
       MODULE_MEMORY *mem = scd30_state;
#  define RETMEM \
       if (scd30_state) { free(scd30_state); scd30_state = nullptr; }
#  define initialized mem->initialized_flag

#  define XSNS_42     42
#  define XI2C_29     29

#endif  // !BUILD_AS_PLUGIN

// XdrvMailbox accessors (pointer in plugin, instance in native)
#if BUILD_AS_PLUGIN
#  define _SCD_MB_DATA_LEN  (XdrvMailbox->data_len)
#  define _SCD_MB_PAYLOAD   (XdrvMailbox->payload)
#else
#  define _SCD_MB_DATA_LEN  (XdrvMailbox.data_len)
#  define _SCD_MB_PAYLOAD   (XdrvMailbox.payload)
#endif

// --------------------------------------------------------------------
// Driver core — bit-identical to the legacy plugin source. The only
// change is each function uses `I2C_SETWIRE(scd_bus)` so reads hit
// the right I2C bus when the sensor is on Wire1.
// --------------------------------------------------------------------

void SCD30_begin(uint8_t addr) {
  SETREGS
  drv.i2cAddress         = addr;
  drv.co2NewDataLocation = -1;  // first reading fills the median filter
  drv.ambientPressure    = 0;
}

// Median-of-5 (sci.image.processing classic — fewer comparisons than
// a generic sort). Mutates the buffer; pass a copy if you need to
// preserve the original.
#define PIX_SORT(a, b) { if ((a) > (b)) PIX_SWAP((a), (b)); }
#define PIX_SWAP(a, b) { uint16_t temp = (a); (a) = (b); (b) = temp; }

uint16_t SCD30_opt_med5(uint16_t *p) {
  PIX_SORT(p[0], p[1]);
  PIX_SORT(p[3], p[4]);
  PIX_SORT(p[0], p[3]);
  PIX_SORT(p[1], p[4]);
  PIX_SORT(p[1], p[2]);
  PIX_SORT(p[2], p[3]);
  PIX_SORT(p[1], p[2]);
  return p[2];
}

int SCD30_clearI2CBus(void) {
  // ESP8266 had a twi_status() check here; intentionally a no-op
  // on ESP32 (the IDF I2C driver clears stuck states internally).
  return 0;
}

uint8_t SCD30_computeCRC8(uint8_t data[], uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t x = 0; x < len; x++) {
    crc ^= data[x];
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

int SCD30_sendBytes(void *pInput, uint8_t len) {
  SETREGS
  I2C_SETWIRE(scd_bus);
  uint8_t *pBytes = (uint8_t *)pInput;
  uint8_t  errorBytes = 0;
  I2C_beginTransmission(drv.i2cAddress);
  for (uint8_t cnt = 0; cnt < len; cnt++) { I2C_write(pBytes[cnt]); }
  int result = I2C_endTransmission(true);
  result <<= 8;
  result |= errorBytes;
  return result;
}

int SCD30_getBytes(void *pOutput, uint8_t len) {
  SETREGS
  I2C_SETWIRE(scd_bus);
  uint8_t *pBytes = (uint8_t *)pOutput;
  uint8_t  result = I2C_requestFrom(drv.i2cAddress, len);
  if (len != result) { return ERROR_SCD30_NOT_ENOUGH_BYTES_ERROR; }

  if (I2C_available()) {
    for (int x = 0; x < len; x++) { pBytes[x] = I2C_read(); }
    return ERROR_SCD30_NO_ERROR;
  }
  return ERROR_SCD30_UNKNOWN_ERROR;
}

int SCD30_sendCommand(uint16_t command) {
  SETREGS
  uint8_t data[2];
  data[0] = (uint8_t)(command >> 8);
  data[1] = (uint8_t)(command & 0xFF);
  return SCD30_sendBytes(data, sizeof(data));
}

int SCD30_sendCommandArguments(uint16_t command, uint16_t arguments) {
  SETREGS
  uint8_t data[5];
  data[0] = command   >> 8;
  data[1] = command   & 0xFF;
  data[2] = arguments >> 8;
  data[3] = arguments & 0xFF;
  data[4] = SCD30_computeCRC8(&data[2], 2);  // CRC over arguments only
  return SCD30_sendBytes(data, sizeof(data));
}

int SCD30_get16BitRegCheckCRC(void *pInput, uint16_t *pData) {
  uint8_t *pBytes = (uint8_t *)pInput;
  uint8_t  expectedCRC = SCD30_computeCRC8(pBytes, SCD30_DATA_REGISTER_BYTES);
  if (expectedCRC != pBytes[SCD30_DATA_REGISTER_BYTES]) {
    return ERROR_SCD30_CRC_ERROR;
  }
  *pData = (uint16_t)pBytes[0] << 8 | pBytes[1];   // big-endian on the wire
  return ERROR_SCD30_NO_ERROR;
}

int SCD30_get32BitRegCheckCRC(void *pInput, float *pData) {
  // SETREGS sets up `jt` — needed because isnan/isinf route through
  // a jumptable wrapper in plugin mode.
  SETREGS
  uint16_t  tempU16High, tempU16Low;
  uint8_t  *pBytes = (uint8_t *)pInput;

  int error = SCD30_get16BitRegCheckCRC(pBytes, &tempU16High);
  if (error) { return error; }
  error = SCD30_get16BitRegCheckCRC(pBytes + SCD30_DATA_REGISTER_WITH_CRC, &tempU16Low);
  if (error) { return error; }

  uint32_t rawInt = ((uint32_t)tempU16High << 16) | tempU16Low;
  *pData = *(float *)&rawInt;

  if (isnan(*pData) || isinf(*pData)) {
    return ERROR_SCD30_NOT_A_NUMBER_ERROR;
  }
  return ERROR_SCD30_NO_ERROR;
}

int SCD30_readRegister(uint16_t registerAddress, uint16_t *pData) {
  SETREGS
  int error = SCD30_sendCommand(registerAddress);
  if (error) { return error; }
  delay(1);   // SCD30 uses clock-stretching; without this delay reads racing
  uint8_t data[SCD30_DATA_REGISTER_WITH_CRC];
  error = SCD30_getBytes(data, sizeof(data));
  if (error) { return error; }
  uint16_t regValue;
  error = SCD30_get16BitRegCheckCRC(data, &regValue);
  if (error) { return error; }
  *pData = regValue;
  return ERROR_SCD30_NO_ERROR;
}

int SCD30_softReset(void)                                                { SETREGS; return SCD30_sendCommand(COMMAND_SCD30_SOFT_RESET); }
int SCD30_getAltitudeCompensation(uint16_t *p)                            { SETREGS; return SCD30_readRegister(COMMAND_SCD30_ALTITUDE_COMPENSATION, p); }
int SCD30_getAmbientPressure(uint16_t *p)                                 { SETREGS; *p = drv.ambientPressure; return ERROR_SCD30_NO_ERROR; }
int SCD30_getForcedRecalibrationFactor(uint16_t *p)                       { SETREGS; return SCD30_readRegister(COMMAND_SCD30_FORCED_RECALIBRATION_FACTOR, p); }
int SCD30_getMeasurementInterval(uint16_t *p)                             { SETREGS; return SCD30_readRegister(COMMAND_SCD30_MEASUREMENT_INTERVAL, p); }
int SCD30_setAltitudeCompensation(uint16_t v)                             { SETREGS; return SCD30_sendCommandArguments(COMMAND_SCD30_ALTITUDE_COMPENSATION, v); }
int SCD30_setAmbientPressure(uint16_t v)                                  { SETREGS; drv.ambientPressure = v; return 0; }
int SCD30_setAutoSelfCalibration(void)                                    { return SCD30_setCalibrationType(true); }
int SCD30_setCalibrationType(bool isAuto)                                 { SETREGS; return SCD30_sendCommandArguments(COMMAND_SCD30_CALIBRATION_TYPE, isAuto ? 1 : 0); }
int SCD30_setForcedRecalibrationFactor(uint16_t v)                        { SETREGS; return SCD30_sendCommandArguments(COMMAND_SCD30_FORCED_RECALIBRATION_FACTOR, v); }
int SCD30_setManualCalibration(void)                                      { return SCD30_setCalibrationType(false); }
int SCD30_stopMeasuring(void)                                             { SETREGS; return SCD30_sendCommand(COMMAND_SCD30_STOP_MEASUREMENT); }

int SCD30_getCalibrationType(uint16_t *pIsAuto) {
  SETREGS
  uint16_t value = 0;
  int error = SCD30_readRegister(COMMAND_SCD30_CALIBRATION_TYPE, &value);
  if (!error) { *pIsAuto = value != 0; }
  return error;
}

int SCD30_getFirmwareVersion(uint8_t *pMajor, uint8_t *pMinor) {
  SETREGS
  uint16_t value;
  int error = SCD30_readRegister(COMMAND_SCD30_GET_FW_VERSION, &value);
  if (!error) { *pMajor = value >> 8; *pMinor = value & 0xFF; }
  return error;
}

int SCD30_getTemperatureOffset(float *pOffset_degC) {
  SETREGS
  uint16_t value;
  int error = SCD30_readRegister(COMMAND_SCD30_TEMPERATURE_OFFSET, &value);
  if (!error) { *pOffset_degC = fscale(value, 0.01, 0); }   // centi-deg → deg
  return error;
}

int SCD30_getTemperatureOffset(uint16_t *pOffset_centiDegC) {
  SETREGS
  return SCD30_readRegister(COMMAND_SCD30_TEMPERATURE_OFFSET, pOffset_centiDegC);
}

int SCD30_setMeasurementInterval(uint16_t time_sec) {
  SETREGS
  if (time_sec < 2)    { time_sec = 2; }
  if (time_sec > 1800) { time_sec = 1800; }
  return SCD30_sendCommandArguments(COMMAND_SCD30_MEASUREMENT_INTERVAL, time_sec);
}

int SCD30_setTemperatureOffset(float offset_degC) {
  SETREGS
  if (jgtsf2(offset_degC, 0)) {
    uint16_t offset_centiDegC = tmod__fixunssfsi(offset_degC) * 100;
    return SCD30_sendCommandArguments(COMMAND_SCD30_TEMPERATURE_OFFSET, offset_centiDegC);
  }
  return ERROR_SCD30_INVALID_VALUE;
}

int SCD30_setTemperatureOffset(uint16_t offset_centiDegC) {
  SETREGS
  return SCD30_sendCommandArguments(COMMAND_SCD30_TEMPERATURE_OFFSET, offset_centiDegC);
}

int SCD30_beginMeasuring(void) {
  SETREGS
  return SCD30_sendCommandArguments(COMMAND_SCD30_CONTINUOUS_MEASUREMENT, drv.ambientPressure);
}

int SCD30_beginMeasuring(uint16_t airPressure_mbar) {
  SETREGS
  drv.ambientPressure = airPressure_mbar;
  return SCD30_sendCommandArguments(COMMAND_SCD30_CONTINUOUS_MEASUREMENT, drv.ambientPressure);
}

int SCD30_isDataAvailable(bool *pIsAvailable) {
  SETREGS
  uint16_t isDataAvailable = 0;
  int error = SCD30_readRegister(COMMAND_SCD30_GET_DATA_READY, &isDataAvailable);
  if (!error) { *pIsAvailable = isDataAvailable != 0; }
  return error;
}

int SCD30_readMeasurement(uint16_t *pCO2_ppm, uint16_t *pCO2EAvg_ppm,
                          float *pTemperature, float *pHumidity) {
  SETREGS
  bool  isAvailable = false;
  int   error = SCD30_isDataAvailable(&isAvailable);
  if (error)        { return error; }
  if (!isAvailable) { return ERROR_SCD30_NO_DATA; }

  error = SCD30_sendCommand(COMMAND_SCD30_READ_MEASUREMENT);
  if (error) { return error; }
  delay(1);

  uint8_t bytes[SCD30_MEAS_BYTES];
  error = SCD30_getBytes(bytes, SCD30_MEAS_BYTES);
  if (error) { return error; }

  float tempCO2, tempTemperature, tempHumidity;
  error = SCD30_get32BitRegCheckCRC(&bytes[0],  &tempCO2);         if (error) return error;
  error = SCD30_get32BitRegCheckCRC(&bytes[6],  &tempTemperature); if (error) return error;
  error = SCD30_get32BitRegCheckCRC(&bytes[12], &tempHumidity);    if (error) return error;

  if (jeqsf2(tempCO2, 0)) { return ERROR_SCD30_CO2_ZERO; }

  // Median filter on CO2; first valid reading seeds the entire window.
  if (drv.co2NewDataLocation < 0) {
    drv.co2EAverage = tmod__fixunssfsi(tempCO2);
    for (int x = 0; x < SCD30_MEDIAN_FILTER_SIZE; x++) {
      drv.co2History[x] = tmod__fixunssfsi(tempCO2);
    }
    drv.co2NewDataLocation = 1;
  } else {
    drv.co2History[drv.co2NewDataLocation++] = tmod__fixunssfsi(tempCO2);
    if (drv.co2NewDataLocation >= SCD30_MEDIAN_FILTER_SIZE) {
      drv.co2NewDataLocation = 0;
    }
  }

  uint16_t temp[SCD30_MEDIAN_FILTER_SIZE];
  for (int x = 0; x < SCD30_MEDIAN_FILTER_SIZE; x++) { temp[x] = drv.co2History[x]; }

  *pCO2_ppm = SCD30_opt_med5(temp);
  if (pCO2EAvg_ppm) {
    int16_t delta  = (int16_t)*pCO2_ppm - (int16_t)drv.co2EAverage;
    int16_t change = delta / 32;
    drv.co2EAverage += change;
    *pCO2EAvg_ppm = drv.co2EAverage;
  }
  *pTemperature = tempTemperature;
  *pHumidity    = tempHumidity;
  return ERROR_SCD30_NO_ERROR;
}

int32_t SCD30_Detect(void) {
  ALLOCMEM
  ready              = false;
  Scd30.data_valid   = false;
  initialized        = false;
  scd_bus            = 0;

  // Probe both buses for the chip — soft-reset would fail before
  // beginMeasuring if we picked the wrong bus.
  bool found = false;
  for (uint32_t bus = 0; bus < MAX_I2C_Busses; bus++) {
    I2C_SETWIRE(bus);
    if (!I2C_SetDevice(SCD30_ADDRESS, bus)) { continue; }
    scd_bus = bus;
    SCD30_begin(SCD30_ADDRESS);

    uint8_t major = 0, minor = 0;
    if (SCD30_getFirmwareVersion(&major, &minor))         { I2C_ResetActive(SCD30_ADDRESS, bus); continue; }
    if (SCD30_getMeasurementInterval(&Scd30.interval))    { I2C_ResetActive(SCD30_ADDRESS, bus); continue; }
    if (SCD30_beginMeasuring())                           { I2C_ResetActive(SCD30_ADDRESS, bus); continue; }

    AddLog(LOG_LEVEL_INFO, PSTR("SCD30: bus %d, FW v%d.%d"), (int)bus, major, minor);
    I2C_SetActiveFound(SCD30_ADDRESS, PSTR("SCD30"), bus);
    initialized = 1;
    ready       = true;
    found       = true;
    break;
  }
  if (!found) { SCD30_Deinit(); return false; }
  return ready;
}

void SCD30_Update(void) {
  SETREGS
  Scd30.loop_count++;
  if (Scd30.loop_count > (Scd30.interval - 1)) {
    uint32_t error = 0;
    switch (Scd30.error_state) {
      case SCD30_STATE_NO_ERROR: {
        error = SCD30_readMeasurement(&Scd30.co2, &Scd30.co2e_avg,
                                      &Scd30.temperature, &Scd30.humidity);
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
            break;
          case ERROR_SCD30_CO2_ZERO:
            Scd30.co2_zero_count++;
            break;
          default:
            Scd30.error_state = SCD30_STATE_ERROR_READ_MEAS;
            return;
        }
      } break;

      case SCD30_STATE_ERROR_DATA_CRC:
        Scd30.error_state = ERROR_SCD30_NO_ERROR;
        break;

      case SCD30_STATE_ERROR_READ_MEAS: {
        Scd30.reset_count++;
        error = SCD30_softReset();
        if (error) {
          Scd30.error_state = ((error >> 8) == 4)
              ? SCD30_STATE_ERROR_SOFT_RESET
              : SCD30_STATE_ERROR_UNKNOWN;
        } else {
          Scd30.error_state = ERROR_SCD30_NO_ERROR;
        }
      } break;

      case SCD30_STATE_ERROR_SOFT_RESET: {
        Scd30.i2c_reset_count++;
        if (SCD30_clearI2CBus()) { Scd30.error_state = SCD30_STATE_ERROR_I2C_RESET; }
        else                     { Scd30.error_state = ERROR_SCD30_NO_ERROR;        }
      } break;

      default:
        Scd30.error_state = SCD30_STATE_ERROR_SOFT_RESET;   // try again
    }

    if (Scd30.loop_count > (SCD30_MAX_MISSED_READS * Scd30.interval)) {
      Scd30.data_valid = false;
    }
  }
}

// --------------------------------------------------------------------
// Console commands — DecodeCommand-driven dispatch
// --------------------------------------------------------------------
const char kScd30Commands[] PROGMEM =
    "Scd30|Alt|Auto|Cal|FW|Int|Pres|TOff";

void CmndScd30Altitude(void) {
  SETREGS
  uint16_t value = 0;
  if (_SCD_MB_DATA_LEN > 0) { value = _SCD_MB_PAYLOAD; SCD30_setAltitudeCompensation(value); }
  else                      { SCD30_getAltitudeCompensation(&value); }
  ResponseCmndNumber(value);
}

void CmndScd30AutoMode(void) {
  SETREGS
  uint16_t value = 0;
  if (_SCD_MB_DATA_LEN > 0) { value = _SCD_MB_PAYLOAD; SCD30_setCalibrationType((bool)value); }
  else                      { SCD30_getCalibrationType(&value); }
  ResponseCmndNumber(value);
}

void CmndScd30Calibrate(void) {
  SETREGS
  uint16_t value = 0;
  if (_SCD_MB_DATA_LEN > 0) { value = _SCD_MB_PAYLOAD; SCD30_setForcedRecalibrationFactor(value); }
  else                      { SCD30_getForcedRecalibrationFactor(&value); }
  ResponseCmndNumber(value);
}

void CmndScd30Firmware(void) {
  SETREGS
  uint8_t major = 0, minor = 0;
  SCD30_getFirmwareVersion(&major, &minor);
  uint16_t value = ((uint16_t)major << 8) | minor;
  ResponseCmndNumber(value);
}

void CmndScd30Interval(void) {
  SETREGS
  uint16_t value = 0;
  if (_SCD_MB_DATA_LEN > 0) {
    value = _SCD_MB_PAYLOAD;
    if (!SCD30_setMeasurementInterval(value)) { Scd30.interval = value; }
  }
  SCD30_getMeasurementInterval(&value);
  ResponseCmndNumber(value);
}

void CmndScd30Pressure(void) {
  SETREGS
  uint16_t value = 0;
  if (_SCD_MB_DATA_LEN > 0) { value = _SCD_MB_PAYLOAD; SCD30_setAmbientPressure(value); }
  else                      { SCD30_getAmbientPressure(&value); }
  ResponseCmndNumber(value);
}

void CmndScd30TempOffset(void) {
  SETREGS
  uint16_t value = 0;
  if (_SCD_MB_DATA_LEN > 0) { value = _SCD_MB_PAYLOAD; SCD30_setTemperatureOffset(value); }
  else                      { SCD30_getTemperatureOffset(&value); }
  ResponseCmndNumber(value);
}

// Command-table needs to live AFTER the function definitions because
// it takes their addresses.
void (*const kScd30Command[])(void) PROGMEM = {
  &CmndScd30Altitude, &CmndScd30AutoMode, &CmndScd30Calibrate,
  &CmndScd30Firmware, &CmndScd30Interval, &CmndScd30Pressure,
  &CmndScd30TempOffset,
};

// --------------------------------------------------------------------
// Display
// --------------------------------------------------------------------
const char xHTTP_SNS_CO2_SCD[]     PROGMEM = "{s}%s CO2{m}%d ppm{e}";
const char xHTTP_SNS_CO2EAVG_SCD[] PROGMEM = "{s}%s eCO2{m}%d ppm{e}";

void SCD30_Show(bool json) {
  SETREGS
  if (!Scd30.data_valid) { return; }

  float t = ConvertTemp(Scd30.temperature);
  float h = ConvertHumidity(Scd30.humidity);

  if (json) {
    ResponseAppend_P(PSTR(",\"SCD30\":{\"Carbondioxide\":%d,\" eCO2\":%d,"),
                     Scd30.co2, Scd30.co2e_avg);
    ResponseAppendTHD(t, h);
    ResponseJsonEnd();
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(GSTR(xHTTP_SNS_CO2EAVG_SCD), PSTR("SCD30"), Scd30.co2e_avg);
    WSContentSend_PD(GSTR(xHTTP_SNS_CO2_SCD),     PSTR("SCD30"), Scd30.co2);
    WSContentSend_THD(PSTR("SCD30"), t, h);
#endif
  }
}

void SCD30_Deinit(void) {
  SETREGS
  I2C_ResetActive(SCD30_ADDRESS, scd_bus);
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t sel) {
  bool result = false;
  switch (sel) {
    case pFUNC_INIT:         result = SCD30_Detect(); break;
    case pFUNC_EVERY_SECOND: SCD30_Update();          break;
    case pFUNC_COMMAND: {
      SETREGS
      result = DecodeCommand(kScd30Commands, kScd30Command);
    } break;
    case pFUNC_JSON_APPEND:  SCD30_Show(1);           break;
    case pFUNC_WEB_SENSOR:   SCD30_Show(0);           break;
    case pFUNC_DEINIT:       SCD30_Deinit();          break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns42(uint32_t function) {
  if (!I2cEnabled(XI2C_29)) { return false; }
  bool result = false;
  if (FUNC_INIT == function) {
    SCD30_Detect();
  } else if (scd30_state && ready) {
    switch (function) {
      case FUNC_EVERY_SECOND: SCD30_Update(); break;
      case FUNC_COMMAND:
        result = DecodeCommand(kScd30Commands, kScd30Command);
        break;
      case FUNC_JSON_APPEND:  SCD30_Show(1);  break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   SCD30_Show(0);  break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef ready
#  undef scd_bus
#  undef Scd30
#  undef drv
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef _SCD_MB_DATA_LEN
#undef _SCD_MB_PAYLOAD
#undef PIX_SORT
#undef PIX_SWAP

#endif  // _SCD30_DUAL_ENABLED
