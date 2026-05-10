/*
  xsns_12_ads1115_dual.ino — native-mode shim for the dual-format
  ADS1115 / ADS1015 4-channel 16-bit ADC driver.

  Driver source: tasmota/Plugins/xsns_12_ads1115_dual.cpp.

  The .cpp is INERT in native mode unless `ADS1115_DUAL_NATIVE_INCLUDE`
  is defined. This shim sets that flag, pulls in the .cpp, and is the
  SOLE producer of native-driver code in the firmware build.

  Activation
  ----------
  Set `USE_ADS1115_DUAL` in user_config_override.h. Make sure the
  legacy `USE_ADS1115` (xsns_12_ads1115.ino) is NOT set at the same
  time — both register Xsns12 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_ADS1115_DUAL

#define ADS1115_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_12_ads1115_dual.cpp"

#endif  // USE_ADS1115_DUAL
#endif  // USE_I2C
