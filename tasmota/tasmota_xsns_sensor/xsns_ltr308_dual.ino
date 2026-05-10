/*
  xsns_ltr308_dual.ino — native-mode shim for the dual-format
  Lite-On LTR-308ALS-01 ambient light sensor.

  Driver source: tasmota/Plugins/xsns_ltr308_dual.cpp.

  Activation
  ----------
  Set `USE_LTR308_DUAL` in user_config_override.h. There's no legacy
  native LTR308 driver in Tasmota, so no Xsns-slot conflict.
  This dual takes Xsns56 (adjust if that slot is already used in
  your build).
*/

#ifdef USE_I2C
#ifdef USE_LTR308_DUAL

#define LTR308_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_ltr308_dual.cpp"

#endif  // USE_LTR308_DUAL
#endif  // USE_I2C
