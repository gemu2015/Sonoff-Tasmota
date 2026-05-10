/*
  xsns_09_bmp_dual.ino — native shim for the dual-format BMP/BME driver.
*/
#ifdef USE_I2C
#ifdef USE_BME_DUAL
#define BME_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0
#include "../Plugins/xsns_09_bmp_dual.cpp"
#endif
#endif
