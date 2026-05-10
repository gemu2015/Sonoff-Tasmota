/*
  xsns_44_sps30_dual.ino — native-mode shim for the dual-format
  Sensirion SPS30 particulate-matter sensor driver.

  Driver source: tasmota/Plugins/xsns_44_sps30_dual.cpp.

  Activation
  ----------
  Set `USE_SPS30_DUAL` in user_config_override.h. Make sure the
  legacy `USE_SPS30` is NOT set at the same time — both register
  Xsns44 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_SPS30_DUAL

#define SPS30_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_44_sps30_dual.cpp"

#endif  // USE_SPS30_DUAL
#endif  // USE_I2C
