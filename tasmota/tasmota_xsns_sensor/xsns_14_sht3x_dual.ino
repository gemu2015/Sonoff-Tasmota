/*
  xsns_14_sht3x_dual.ino — native-mode shim for the dual-format
  SHT3X driver POC.

  The actual driver source lives at
  `tasmota/Plugins/xsns_14_sht3x_dual.cpp` and is structured to
  compile as either a plugin (`-DBUILD_AS_PLUGIN=1` + USE_SHT3X_DUAL_MOD)
  or a native xsns_14 driver. This shim is what wires the native
  form into the firmware build.

  Why a separate shim file
  ------------------------
  PIO's default src_filter compiles every .cpp under tasmota/, so
  `Plugins/xsns_14_sht3x_dual.cpp` is compiled standalone — and the
  Arduino preprocessor also merges this .ino into tasmota.ino.cpp.
  If both paths produced native-mode code, link errors follow
  (duplicate Xsns14 etc).

  Solution: the .cpp is INERT in native mode unless
  `SHT3X_DUAL_NATIVE_INCLUDE` is defined. This shim sets that flag,
  pulls in the .cpp, and is the SOLE producer of native driver
  code in the firmware. The standalone .cpp compile in Plugins/
  becomes empty — the unused-source-file is fine.

  Activation
  ----------
  Set `USE_SHT3X_DUAL` in user_config_override.h (any device block
  that wants the dual-format SHT3X driver). USE_SHT3X must NOT be
  set at the same time, otherwise xsns_14_sht3x.ino would also
  register Xsns14 and conflict.
*/

#ifdef USE_I2C
#ifdef USE_SHT3X_DUAL

// Tell the dual .cpp it's being included by us (the native shim),
// so its native gate activates.
#define SHT3X_DUAL_NATIVE_INCLUDE
// Force native mode. (The .cpp would default to BUILD_AS_PLUGIN=0
// here anyway since USE_SHT3X_DUAL_MOD isn't set, but being explicit
// makes the intent clear.)
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xsns_14_sht3x_dual.cpp"

#endif  // USE_SHT3X_DUAL
#endif  // USE_I2C
