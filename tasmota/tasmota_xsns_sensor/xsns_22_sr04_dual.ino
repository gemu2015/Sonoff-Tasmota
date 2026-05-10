/*
  xsns_22_sr04_dual.ino — native-mode shim for the dual-format
  JSN-SR04T-V3 / SR04T ultrasonic distance sensor (UART variant).

  Driver source: tasmota/Plugins/xsns_22_sr04_dual.cpp.

  Activation
  ----------
  Set `USE_SR04T_DUAL` in user_config_override.h. There may be a
  legacy native SR04 driver registered as Xsns22; if so, the legacy
  must be disabled to avoid conflict.

  Pin: tries Pin(GPIO_SR04_ECHO) from the Tasmota template first;
  falls back to GPIO 3 (UART0 RX, build-time default) if unmapped.
*/

#ifdef USE_SR04T_DUAL

#define SR04T_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_22_sr04_dual.cpp"

#endif  // USE_SR04T_DUAL
