/*
  xsns_46_MLX90614_dual.ino — native shim for the dual-format MLX90614
  driver (real source at tasmota/Plugins/xsns_46_MLX90614_dual.cpp).
*/
#ifdef USE_I2C
#ifdef USE_MLX90614_DUAL
#define MLX90614_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0
#include "../Plugins/xsns_46_MLX90614_dual.cpp"
#endif
#endif
