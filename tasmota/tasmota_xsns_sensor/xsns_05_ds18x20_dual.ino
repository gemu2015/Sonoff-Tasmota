/*
  xsns_05_ds18x20_dual.ino — native-mode shim for the dual-format
  Maxim DS18B20 / DS18S20 / DS1822 / MAX31850 1-Wire temperature
  sensor driver.

  Driver source: tasmota/Plugins/xsns_05_ds18x20_dual.cpp.

  Activation
  ----------
  Set `USE_DS18X20_DUAL` in user_config_override.h. Make sure the
  legacy `USE_DS18x20` (xsns_05_ds18x20.ino / xsns_05_esp32_ds18x20.ino)
  is NOT set at the same time — both register Xsns05 and conflict.

  Pin: tries Pin(GPIO_DSB) from the Tasmota template first; falls back
  to GPIO 16 (DS18X20_DEFAULT_DAT). For dual-pin (split DAT-out / DAT-in,
  e.g. some Shelly devices), additionally map GPIO_DSB_OUT.
*/

#ifdef USE_DS18X20_DUAL

#define DS18X20_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_05_ds18x20_dual.cpp"

#endif  // USE_DS18X20_DUAL
