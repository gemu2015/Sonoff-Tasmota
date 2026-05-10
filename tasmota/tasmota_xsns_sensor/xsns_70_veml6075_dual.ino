/*
  xsns_70_veml6075_dual.ino — native-mode shim for the dual-format
  Vishay VEML6075 UVA/UVB/UV-Index light sensor driver.

  Driver source: tasmota/Plugins/xsns_70_veml6075_dual.cpp.

  Activation
  ----------
  Set `USE_VEML6075_DUAL` in user_config_override.h. Make sure the
  legacy `USE_VEML6075` is NOT set at the same time — both register
  Xsns70 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_VEML6075_DUAL

#define VEML6075_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_70_veml6075_dual.cpp"

#endif  // USE_VEML6075_DUAL
#endif  // USE_I2C
