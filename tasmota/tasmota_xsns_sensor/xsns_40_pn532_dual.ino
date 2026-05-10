/*
  xsns_40_pn532_dual.ino — native-mode shim for the dual-format
  NXP PN532 NFC reader driver (I2C + HSU/serial transport).

  Driver source: tasmota/Plugins/xsns_40_pn532_dual.cpp.

  Activation
  ----------
  Set `USE_PN532_DUAL` in user_config_override.h. Make sure the
  legacy `USE_PN532_HSU` is NOT set at the same time — both register
  Xsns40 and conflict.

  Transport pick (native build):
    - Define `USE_PN532_I2C` to default to I2C transport
      (chip address 0x24, both buses probed).
    - Otherwise defaults to HSU/serial @ 115200 baud
      (RXD/TXD pulled from Pin(GPIO_PN532_RXD/TXD), fallback GPIO 3/1).

  In plugin mode the BinPlugin descriptor exposes the same RXD/TXD/MODE
  params the legacy plugin uses (mp->ms[0..2]).
*/

#ifdef USE_PN532_DUAL

#define PN532_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_40_pn532_dual.cpp"

#endif  // USE_PN532_DUAL
