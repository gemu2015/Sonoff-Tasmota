/*
  xdrv_28_pcf8574_dual.ino — native-mode shim for the dual-format
  PCF8574 / PCF8574A I/O-expander driver.

  The actual driver source lives at
  `tasmota/Plugins/xdrv_28_pcf8574_dual.cpp` and is structured to
  compile as either a plugin (`-DBUILD_AS_PLUGIN=1` + USE_PCF8574_DUAL_MOD)
  or a native xdrv_28 driver. This shim is what wires the native form
  into the firmware build.

  The .cpp is INERT in native mode unless `PCF8574_DUAL_NATIVE_INCLUDE`
  is defined. This shim sets that flag, pulls in the .cpp, and is the
  SOLE producer of native-driver code in the firmware build — the
  standalone .cpp compile in Plugins/ becomes empty.

  Activation
  ----------
  Set `USE_PCF8574_DUAL` in user_config_override.h. Make sure the
  legacy `USE_PCF8574_V2` (xdrv_28_pcf8574_v2.ino) is NOT set at the
  same time, otherwise both drivers register Xdrv28 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_PCF8574_DUAL

#define PCF8574_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xdrv_28_pcf8574_dual.cpp"

#endif  // USE_PCF8574_DUAL
#endif  // USE_I2C
