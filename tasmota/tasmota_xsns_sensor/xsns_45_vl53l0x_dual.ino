/*
  xsns_45_vl53l0x_dual.ino — native shim for the dual-format VL53L0X driver.
*/
#ifdef USE_I2C
#ifdef USE_VL53L0X_DUAL
#define VL53L0X_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0
#include "../Plugins/xsns_45_vl53l0x_dual.cpp"
#endif
#endif
