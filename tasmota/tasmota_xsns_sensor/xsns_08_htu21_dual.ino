/*
  xsns_08_htu21_dual.ino — native-mode shim for the dual-format HTU21
  driver. The real source is at tasmota/Plugins/xsns_08_htu21_dual.cpp;
  see dual_format_compat.h for the architecture.

  Activate via USE_HTU_DUAL in user_config_override.h. USE_HTU
  must NOT be set at the same time, or xsns_08_htu21.ino would
  also register Xsns08 → duplicate-symbol link error.
*/
#ifdef USE_I2C
#ifdef USE_HTU_DUAL
#define HTU_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0
#include "../Plugins/xsns_08_htu21_dual.cpp"
#endif
#endif
