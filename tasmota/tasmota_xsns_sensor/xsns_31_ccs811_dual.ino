/*
  xsns_31_ccs811_dual.ino — native shim for the dual-format CCS811
  driver (real source at tasmota/Plugins/xsns_31_ccs811_dual.cpp).

  Activate via USE_CCS811_DUAL. USE_CCS811 must NOT be set.
*/
#ifdef USE_I2C
#ifdef USE_CCS811_DUAL
#define CCS811_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0
#include "../Plugins/xsns_31_ccs811_dual.cpp"
#endif
#endif
