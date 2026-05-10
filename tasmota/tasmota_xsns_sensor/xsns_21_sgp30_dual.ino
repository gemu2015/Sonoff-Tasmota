/*
  xsns_21_sgp30_dual.ino — native-mode shim for the dual-format
  Sensirion SGP30 air-quality (eCO2 + TVOC) sensor driver.

  Driver source: tasmota/Plugins/xsns_21_sgp30_dual.cpp.

  Activation
  ----------
  Set `USE_SGP30_DUAL` in user_config_override.h. Make sure the
  legacy `USE_SGP30` (xsns_21_sgp30.ino) is NOT set at the same time
  — both register Xsns21 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_SGP30_DUAL

#define SGP30_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_21_sgp30_dual.cpp"

#endif  // USE_SGP30_DUAL
#endif  // USE_I2C
