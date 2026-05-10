/*
  xsns_102_ld2410_dual.ino — native-mode shim for the dual-format
  HLK-LD2410 24 GHz mmWave motion sensor driver.

  Driver source: tasmota/Plugins/xsns_102_ld2410_dual.cpp.

  Activation
  ----------
  Set `USE_LD2410_DUAL` in user_config_override.h. There may be a
  legacy native LD2410 driver registered as Xsns102; if so, the
  legacy must be disabled (or the original xsns_102_ld2410.ino
  removed) to avoid Xsns-slot conflict.

  Pins: takes Pin(GPIO_LD2410_RX) and Pin(GPIO_LD2410_TX) from the
  Tasmota template — both must be assigned for Init to succeed.
*/

#ifdef USE_LD2410_DUAL

#define LD2410_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_102_ld2410_dual.cpp"

#endif  // USE_LD2410_DUAL
