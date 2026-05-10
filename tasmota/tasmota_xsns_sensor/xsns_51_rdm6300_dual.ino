/*
  xsns_51_rdm6300_dual.ino — native-mode shim for the dual-format
  Seeed Grove / RDM630 / RDM6300 125 kHz RFID tag reader driver.

  Driver source: tasmota/Plugins/xsns_51_rdm6300_dual.cpp.

  Activation
  ----------
  Set `USE_RDM6300_DUAL` in user_config_override.h. There may be a
  legacy native RDM6300 driver registered as Xsns51; if so, the
  legacy must be disabled to avoid Xsns-slot conflict.

  Pin: tries Pin(GPIO_RDM6300_RX) from the Tasmota template first;
  falls back to GPIO 3 (UART0 RX) if unmapped.
*/

#ifdef USE_RDM6300_DUAL

#define RDM6300_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_51_rdm6300_dual.cpp"

#endif  // USE_RDM6300_DUAL
