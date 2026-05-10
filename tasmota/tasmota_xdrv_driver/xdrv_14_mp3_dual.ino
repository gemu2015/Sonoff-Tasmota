/*
  xdrv_14_mp3_dual.ino — native-mode shim for the dual-format
  DFRobot DFPlayer Mini / DY-SV17F MP3 player driver.

  The actual driver source lives at
  `tasmota/Plugins/xdrv_14_mp3_dual.cpp` and is structured to compile
  as either a plugin (`-DBUILD_AS_PLUGIN=1` + USE_MP3_PLAYER_MOD) or
  a native xdrv_14 driver. This shim wires the native form into the
  firmware build.

  The .cpp is INERT in native mode unless `MP3_DUAL_NATIVE_INCLUDE`
  is defined. This shim sets that flag, pulls in the .cpp, and is the
  SOLE producer of native-driver code in the firmware build.

  Activation
  ----------
  Set `USE_MP3_PLAYER_DUAL` in user_config_override.h. Make sure the
  legacy `USE_MP3_PLAYER` (xdrv_14_mp3.ino) is NOT set at the same
  time — both register Xdrv14 and conflict.

  Optional: define `USE_MP3_PLAYER_TYPE_DY_SV17F` to default to the
  DY-SV17F protocol (otherwise DFPlayer Mini protocol is used).

  Pin: tries Pin(GPIO_MP3_DFR562) first, falls back to GPIO 10
  (MP3_DEFAULT_TX_PIN) if no GPIO is mapped in the Tasmota template.
*/

#ifdef USE_MP3_PLAYER_DUAL

#define MP3_DUAL_NATIVE_INCLUDE
#define BUILD_AS_PLUGIN 0

#include "../Plugins/xdrv_14_mp3_dual.cpp"

#endif  // USE_MP3_PLAYER_DUAL
