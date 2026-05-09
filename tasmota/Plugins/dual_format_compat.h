/*
  dual_format_compat.h — shared boilerplate for dual-format plugin/native
  driver source files.

  Each "dual-format" driver source compiles to either a Tasmota
  BinPlugin (.bin loaded by xdrv_123_plugins.ino) OR a native xsns_*
  driver linked into the firmware, picked by the BUILD_AS_PLUGIN
  flag. The plugin idioms (I2C_* macros via jumptable, float wrappers,
  MODULE_DESCRIPTOR etc.) and the native idioms (Wire.* directly,
  Xsns<NN> dispatcher) are bridged by the macros below — driver core
  code (Sht3xRead, Bme280Read, …) is written ONCE and works in both
  modes thanks to this compat layer.

  Usage in a dual-format driver source
  ------------------------------------
    #include "tasmota_options.h"
    #include "dual_format_compat.h"   // ← BUILD_AS_PLUGIN now defined

    // Per-plugin gate (replace XYZ with the plugin's MOD name)
    #if BUILD_AS_PLUGIN
    #  ifdef USE_XYZ_DUAL_MOD
    #    define _XYZ_DUAL_ENABLED 1
    #  endif
    #else
    #  if defined(USE_I2C) && defined(USE_XYZ_DUAL) \
          && defined(XYZ_DUAL_NATIVE_INCLUDE)
    #    define _XYZ_DUAL_ENABLED 1
    #  endif
    #endif

    #ifdef _XYZ_DUAL_ENABLED
    // ... driver code ...
    #endif

  The native shim file at tasmota/tasmota_xsns_sensor/xsns_NN_xyz_dual.ino
  is what makes the file's native form actually contribute to the
  firmware build. Without the shim's `XYZ_DUAL_NATIVE_INCLUDE` define,
  the .cpp under Plugins/ stays inert when PIO compiles it standalone
  — which prevents duplicate-symbol link errors.

  The plugin's USE_<NAME>_DUAL_MOD gate is the side build_plugin.py
  picks up (it greps Plugins/*.cpp for `#ifdef USE_..._MOD`). The
  native USE_<NAME>_DUAL gate sits in a device block of
  user_config_override.h.
*/

#ifndef DUAL_FORMAT_COMPAT_H
#define DUAL_FORMAT_COMPAT_H

// --------------------------------------------------------------------
// Build-mode detection — caller can override with -DBUILD_AS_PLUGIN
// --------------------------------------------------------------------
//
// Each dual-format driver provides a `USE_<NAME>_DUAL_MOD` gate
// (plugin) and a `USE_<NAME>_DUAL` gate (native). The mode picked
// here is decided BEFORE any of those plugin-specific gates run,
// so we can't auto-detect from them — caller must set
// BUILD_AS_PLUGIN explicitly OR rely on each driver's own
// `#ifndef BUILD_AS_PLUGIN` block (which inspects USE_<NAME>_DUAL_MOD
// directly).
//
// Default: assume native unless caller said otherwise. Drivers
// that want plugin-mode auto-detection must do their own
// `#ifndef BUILD_AS_PLUGIN ... #endif` BEFORE including this file
// (or AFTER — both work).
#ifndef BUILD_AS_PLUGIN
#  define BUILD_AS_PLUGIN 0
#endif

// --------------------------------------------------------------------
// Plugin-mode includes (only when BUILD_AS_PLUGIN==1)
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN
#  include "module.h"
#  include "module_defines.h"
#endif

// --------------------------------------------------------------------
// Native-mode compat shim — maps plugin idioms to native equivalents.
//
// ALL macros below are conditional on !BUILD_AS_PLUGIN. In plugin
// mode the originals (defined in module_defines.h) take effect.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN

// I2C — plugin's I2C_* macros route through jrequestFrom / jWire
// jumptable wrappers. Native: call Wire directly. Argument shapes
// match Wire's signatures so the call sites in driver code can stay
// identical between modes.
#  define I2C_beginTransmission(addr)         Wire.beginTransmission((uint8_t)(addr))
#  define I2C_write(b)                        Wire.write((uint8_t)(b))
#  define I2C_endTransmission(stop)           Wire.endTransmission((bool)(stop))
#  define I2C_requestFrom(addr, n)            Wire.requestFrom((uint8_t)(addr), (uint8_t)(n))
#  define I2C_read()                          Wire.read()
#  define I2C_available()                     Wire.available()
// Higher-level I2C helpers — plugin's `I2C_Read8(addr, reg)` and
// `I2C_write8(addr, reg, val)` route through tasmota helper jumptable
// entries. Native uses the equivalent Tasmota helpers I2cRead8 /
// I2cWrite8 (defined globally in support_legacy_cores.ino).
#  define I2C_Read8(addr, reg)                I2cRead8((addr), (reg))
#  define I2C_write8(addr, reg, val)          I2cWrite8((addr), (reg), (val))

// Multi-bus selection — plugin tracks bus via I2C_SETWIRE; native
// passes the bus arg explicitly to I2cSetDevice / I2cSetActiveFound
// / I2cResetActive. The shim's I2C_SETWIRE is a no-op since the bus
// is implicit downstream.
#  ifndef MAX_I2C_Busses
#    define MAX_I2C_Busses                  1
#  endif
#  define I2C_SETWIRE(bus)                    do { (void)(bus); } while (0)
#  define I2C_SetDevice(addr, bus)            I2cSetDevice((addr), (bus))
#  define I2C_SetActiveFound(addr, name, bus) I2cSetActiveFound((addr), (name), (bus))
#  define I2C_ResetActive(addr, bus)          I2cResetActive((addr), (bus))

// Float math — plugin routes through jumptable (jt[39..43]); native
// uses native operators. Expression-equivalent so the call sites in
// the driver code don't change.
//
// Bare names (`tofloat`, `fdiv`, `fdiff` …) and j-prefixed names
// (`jtofloat`, `jfdiv`, `jfdiff`, `jfadd`, `jfmul`, `jfscale`) are
// both used across drivers — the plugin's module_defines.h aliases
// them to the same jumptable wrappers. We mirror that here.
#  define tofloat(x)                          ((float)(x))
#  define fdiv(a, b)                          ((float)(a) / (float)(b))
#  define fdiff(a, b)                         ((float)(a) - (float)(b))
#  define iseq(x)                             (false)
#  define jNAN                                NAN
#  define jtofloat(x)                         ((float)(x))
#  define jfdiv(a, b)                         ((float)(a) / (float)(b))
#  define jfdiff(a, b)                        ((float)(a) - (float)(b))
#  define jfmul(a, b)                         ((float)(a) * (float)(b))
#  define jfadd(a, b)                         ((float)(a) + (float)(b))
// jfscale(x, mul, sub) ≡ x * mul - sub (used in HTU21 etc.)
#  define jfscale(x, mul, sub)                (((float)(x) * (float)(mul)) - (float)(sub))
#  define fscale(x, mul, sub)                 (((float)(x) * (float)(mul)) - (float)(sub))
// Float comparison wrappers from the plugin: jgtsf2 (>), jltsf2 (<),
// jeqsf2 (==). Native: just use C operators.
#  define jgtsf2(a, b)                        ((float)(a) >  (float)(b))
#  define jltsf2(a, b)                        ((float)(a) <  (float)(b))
#  define jeqsf2(a, b)                        ((float)(a) == (float)(b))
// Float→uint32 conversion (plugin: jt[83] / __fixunssfsi).
#  define fixunssfsi(x)                       ((uint32_t)(float)(x))

// MOD_RESULT — plugin uses this typedef for mod_func_execute's return
// type. Maps to int32_t in module_defines.h:573. Native: same int32_t.
// Predefining it here means the plugin source's `MOD_RESULT result`
// works in native too (no per-driver branching needed).
#  define MOD_RESULT                          int32_t

// Float-constant access — plugin uses FP_CONST[idx] via PROGMEM
// + GETDCONSTP indirection. Native: direct array index. Each driver
// provides its own FP_CONST[] array.
#  define FLTC(idx)                           (FP_CONST[(idx)])

// Module-system markers — plugin-only. Native expansions are empty
// (NOT `do { } while (0)`) so call sites without trailing `;` work.
//
// Plugin-side definitions of these are in module_defines.h:
//   ALLOCMEM allocates a heap MODULE_MEMORY via jcalloc, returns -1 on OOM
//   RETMEM   frees the heap MODULE_MEMORY
//   SETREGS  / STGLOB stash mt/jt/mp pointers
//
// Native equivalents for ALLOCMEM/RETMEM are PER-DRIVER (each
// driver has its own state struct + pointer name). They're defined
// inside the driver source after declaring the state struct.
// SETREGS/STGLOB are no-ops in native (no register-stash needed).
#  define SETREGS                             /* empty */
#  define STGLOB                              /* empty */

// PROGMEM / GSTR — plugin's GSTR resolves a PROGMEM string via
// EXEC_OFFSET (the BinPlugin loader's relocation offset). Native:
// the literal IS at PROGMEM directly, no offset needed.
#  define GSTR(s)                             (s)

// Module-decl macros — plugin-only. Native: empty so MODULE_PART
// forward decls reduce to plain C++ forward decls.
#  define PUSH_OPTIONS                        /* empty */
#  define PULL_OPTIONS                        /* empty */
#  define MODULE_PART                         /* empty */
#  define MODULE_END                          /* empty */

// Sensor-name lookup — plugin's `Plugin_Get_SensorNames(buf, id)`
// returns a display name string from the BinPlugin runtime. Native:
// hardcode common names. Drivers that need different names should
// override this macro AFTER including this header.
#  ifndef Plugin_Get_SensorNames
#    define iD_ABSOLUTE_HUMIDITY              0
#    define Plugin_Get_SensorNames(buf, id)   (strcpy_P((buf), PSTR("Abs.Humid")), (buf))
#  endif

// TasmotaGlobal access — plugin uses pointer (`->`); native uses
// member-access (`.`). Drivers that touch TasmotaGlobal in shared
// code should `#if BUILD_AS_PLUGIN`-branch the access (see
// xsns_14_sht3x_dual.cpp's SHT3X_Show for an example).

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Per-driver heap-state allocator macros.
//
// `DUAL_ALLOCMEM(name)` and `DUAL_RETMEM(name)` mirror plugin's
// `ALLOCMEM` / `RETMEM` semantics for the native side. `name` is the
// stem used for the state struct and pointer:
//
//   typedef struct { … } sht3x_state_t;
//   static sht3x_state_t *sht3x_state = nullptr;
//   DUAL_ALLOCMEM(sht3x);   // expands to lazy-calloc + return -1 on OOM
//   DUAL_RETMEM(sht3x);     // expands to free + null-out
//
// Each call ends in `;` so call sites with or without trailing `;`
// both compile. Use these inside a per-driver `#if !BUILD_AS_PLUGIN`
// block to bind ALLOCMEM/RETMEM to the right state.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  define DUAL_ALLOCMEM(name) \
       if (!name##_state) name##_state = (name##_state_t *)calloc(1, sizeof(name##_state_t)); \
       if (!name##_state) return -1;
#  define DUAL_RETMEM(name) \
       if (name##_state) { free(name##_state); name##_state = nullptr; }
#endif

#endif  // DUAL_FORMAT_COMPAT_H
