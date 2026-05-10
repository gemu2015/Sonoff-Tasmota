/*
  xsns_124_TCS34725_dual.ino — native-mode shim for the dual-format
  TCS34725 RGB / colour-temp / lux sensor driver.

  Driver source: tasmota/Plugins/xsns_124_TCS34725_dual.cpp.

  Activation
  ----------
  Set `USE_TCS34725_DUAL` in user_config_override.h. Make sure the
  legacy `USE_TCS34725` is NOT set at the same time — both register
  Xsns124 and conflict.

  Note: TCS34725 lives at I2C address 0x29, which CONFLICTS with
  VL53L0X. They cannot share a bus. With the dual-format two-bus
  support, you can put one on Wire and the other on Wire1.
*/

#ifdef USE_I2C
#ifdef USE_TCS34725_DUAL

#define TCS34725_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_124_TCS34725_dual.cpp"

#endif  // USE_TCS34725_DUAL
#endif  // USE_I2C
