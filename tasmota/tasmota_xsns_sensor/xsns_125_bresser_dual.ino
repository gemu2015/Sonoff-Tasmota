/*
  xsns_125_bresser_dual.ino — native-mode shim for the dual-format
  Bresser Weather Sensor CC1101 driver.

  Driver source: tasmota/Plugins/xsns_125_bresser_dual.cpp.

  Activation
  ----------
  Set `USE_BRESSER_DUAL` in user_config_override.h. Make sure the
  legacy `USE_BRESSER` is NOT set at the same time — both register
  Xsns125 and conflict.

  Hardware: CC1101 RF transceiver wired via SPI (CS pin) plus a
  GDO0 input pin. Default plugin params: CS=GPIO15, GDO0=GPIO5;
  override via the BinPlugin UI in plugin mode, or via the Tasmota
  template / build defaults in native mode.
*/

#ifdef USE_BRESSER_DUAL

#define BRESSER_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_125_bresser_dual.cpp"

#endif  // USE_BRESSER_DUAL
