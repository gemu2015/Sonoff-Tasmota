/*
  xsns_42_scd30_dual.ino — native-mode shim for the dual-format
  Sensirion SCD30 NDIR CO2 sensor driver.

  Driver source: tasmota/Plugins/xsns_42_scd30_dual.cpp.

  Activation
  ----------
  Set `USE_SCD30_DUAL` in user_config_override.h. Make sure the
  legacy `USE_SCD30` is NOT set at the same time — both register
  Xsns42 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_SCD30_DUAL

#define SCD30_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_42_scd30_dual.cpp"

#endif  // USE_SCD30_DUAL
#endif  // USE_I2C
