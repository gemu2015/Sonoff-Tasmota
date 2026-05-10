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

// Wire / TwoWire pulled in unconditionally — the native compat shim
// below declares a `TwoWire *_dual_wire` for two-bus support, and
// even in plugin mode the file may be compiled standalone without
// the .cpp body being active. Wire.h is cheap and always available.
#include <Wire.h>

// --------------------------------------------------------------------
// Native-mode compat shim — maps plugin idioms to native equivalents.
//
// ALL macros below are conditional on !BUILD_AS_PLUGIN. In plugin
// mode the originals (defined in module_defines.h) take effect.
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN

// I2C — plugin's I2C_* macros route through jrequestFrom / jWire
// jumptable wrappers using the per-instance `mem->xWire` pointer
// (swapped between &Wire and &Wire1 by I2C_SETWIRE). Native must
// mirror that — a hardcoded `Wire.…` would silently break two-bus
// boards (the loop probes bus 1 via I2cSetDevice but reads via
// Wire == bus 0, sensor never replies, driver concludes "not present").
//
// Native solution: a file-static TwoWire pointer that I2C_SETWIRE(bus)
// flips between &Wire and &Wire1, and ALL low-level I2C macros
// dereference. ESP8266 only has one TwoWire (Wire) so we collapse
// to single-bus there.
//
// Each dual-format driver source ends up with its OWN copy of
// `_dual_wire` (because `static` at file scope inside this header
// is per-TU — and dual drivers are individual TUs in plugin mode,
// or merged into tasmota.ino.cpp via the .ino shims in native mode).
// In the merged-TU case all dual drivers share one `_dual_wire`,
// which is the desired behavior — the most-recent I2C_SETWIRE wins
// for the next access, exactly like the plugin's `mem->xWire` does
// per-instance (each plugin slot has its own MODULE_MEMORY).
#  ifdef ESP32
static TwoWire *_dual_wire = &Wire;
#    define I2C_SETWIRE(bus)                  do { _dual_wire = ((bus) == 1 ? &Wire1 : &Wire); } while (0)
#  else
static TwoWire *_dual_wire = &Wire;
#    define I2C_SETWIRE(bus)                  do { (void)(bus); } while (0)
#  endif
#  define I2C_beginTransmission(addr)         _dual_wire->beginTransmission((uint8_t)(addr))
#  define I2C_write(b)                        _dual_wire->write((uint8_t)(b))
#  define I2C_endTransmission(stop)           _dual_wire->endTransmission((bool)(stop))
#  define I2C_requestFrom(addr, n)            _dual_wire->requestFrom((uint8_t)(addr), (uint8_t)(n))
#  define I2C_read()                          _dual_wire->read()
#  define I2C_available()                     _dual_wire->available()
// Higher-level I2C helpers — plugin's `I2C_Read8(addr, reg)` and
// `I2C_write8(addr, reg, val)` route through tasmota helper jumptable
// entries. Native uses the equivalent Tasmota helpers I2cRead8 /
// I2cWrite8 (defined globally in support_legacy_cores.ino).
//
// Tasmota's I2cRead8/I2cWrite8 take an explicit bus arg in their
// 3-arg form — we read the current `_dual_wire` to pick which bus
// matches our last I2C_SETWIRE.
#  ifdef ESP32
#    define _DUAL_BUS_ARG                     (_dual_wire == &Wire1 ? 1 : 0)
#    define _DUAL_WIRE_FOR(bus)               ((bus) == 1 ? &Wire1 : &Wire)
#  else
#    define _DUAL_BUS_ARG                     0
#    define _DUAL_WIRE_FOR(bus)               (&Wire)
#  endif
#  define I2C_Read8(addr, reg)                I2cRead8((addr), (reg), _DUAL_BUS_ARG)
#  define I2C_write8(addr, reg, val)          I2cWrite8((addr), (reg), (val), _DUAL_BUS_ARG)
// 16-bit I2C helpers — plugin's `I2C_Read16(addr, reg, bus)` /
// `I2C_Write16(addr, reg, val, bus)` / `I2C_ValidRead16(*data, addr,
// reg, bus)` route through Tasmota's bus-aware I2cRead16/I2cWrite16/
// I2cValidRead16 helpers. Driver code typically passes a literal `0`
// for the bus arg (legacy single-bus assumption); we override that
// with `_DUAL_BUS_ARG` so the active bus from the last I2C_SETWIRE
// is used regardless of what the call site says — the source can
// stay identical between modes.
#  define I2C_Read16(addr, reg, bus)          I2cRead16((addr), (reg), _DUAL_BUS_ARG)
#  define I2C_Write16(addr, reg, val, bus)    I2cWrite16((addr), (reg), (val), _DUAL_BUS_ARG)
#  define I2C_ValidRead16(data, addr, reg, bus) I2cValidRead16((data), (addr), (reg), _DUAL_BUS_ARG)

// Multi-bus selection — Tasmota's MAX_I2C_Busses is set to 2 on
// ESP32 (both Wire and Wire1 are available) and 1 on ESP8266.
// Drivers iterate `for (bus = 0; bus < MAX_I2C_Busses; bus++)` to
// probe both buses for their device.
#  ifndef MAX_I2C_Busses
#    ifdef ESP32
#      define MAX_I2C_Busses                  2
#    else
#      define MAX_I2C_Busses                  1
#    endif
#  endif
#  define I2C_SetDevice(addr, bus)            I2cSetDevice((addr), (bus))
#  define I2C_SetActiveFound(addr, name, bus) I2cSetActiveFound((addr), (name), (bus))
#  define I2C_ResetActive(addr, bus)          I2cResetActive((addr), (bus))

// TasmotaSerial — plugin's NewTS/beginTS/writeTS/etc route through
// jt[53..64,88]. Native: instantiate TasmotaSerial directly via
// `new`, store the pointer as `void*` (matches the plugin's opaque
// handle type), and cast on each call. Argument shapes match the
// plugin macros so call sites in driver code stay identical.
//
// Note: drivers that hold a `void *ts` and call these macros must
// `#include <TasmotaSerial.h>` themselves before this header (or
// arrange for it to be in scope via their .ino shim's transitive
// includes), since we only forward-declare the class here to avoid
// pulling the heavy header into every dual driver.
class TasmotaSerial;
#  define NewTS(rpin, tpin)                   ((void *)(new TasmotaSerial((int)(rpin), (int)(tpin))))
#  define beginTS(ts, baud)                   (((TasmotaSerial *)(ts))->begin((uint32_t)(baud)))
#  define flushTS(ts)                         (((TasmotaSerial *)(ts))->flush())
#  define writeTS(ts, buf, n)                 (((TasmotaSerial *)(ts))->write((const uint8_t *)(buf), (size_t)(n)))
#  define readTS(ts, buf, n)                  (((TasmotaSerial *)(ts))->read((char *)(buf), (size_t)(n)))
#  define availableTS(ts)                     (((TasmotaSerial *)(ts))->available())
#  define bwriteTS(ts, val)                   (((TasmotaSerial *)(ts))->write((uint8_t)(val)))
#  define deleteTS(ts)                        do { delete ((TasmotaSerial *)(ts)); } while (0)

// Misc helpers used by drivers — plugin's `iscale(val, max_out, max_in)`
// scales `val` from 0..max_in to 0..max_out, equivalent to Tasmota's
// changeUIntScale(val, 0, max_in, 0, max_out).
#  define iscale(val, max_out, max_in) \
       ((uint32_t)changeUIntScale((uint32_t)(val), 0, (uint32_t)(max_in), 0, (uint32_t)(max_out)))

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

// Bare-name aliases used by some driver-core headers (VL53L0X_c.h
// uses `udivsi3`, `ltsf2`, `gtsf2`, `fmul`, `floatunsisf`,
// `FPC_0`). Plugin's module_defines.h #defines these to `j*` /
// `tmod_*` jumptable wrappers; mirror them here.
#  define ltsf2(a, b)                         ((float)(a) <  (float)(b))
#  define gtsf2(a, b)                         ((float)(a) >  (float)(b))
#  define eqsf2(a, b)                         ((float)(a) == (float)(b))
#  define fmul(a, b)                          ((float)(a) * (float)(b))
#  define fadd(a, b)                          ((float)(a) + (float)(b))
#  define floatunsisf(x)                      ((float)(uint32_t)(x))
#  define floatsisf(x)                        ((float)(int32_t)(x))
#  define udivsi3(a, b)                       ((uint32_t)(a) / (uint32_t)(b))
// FP_CONST PROGMEM-array slot aliases used by some drivers; plugin
// resolves them via jfl_const(). Drivers that use `FPC_0` etc. should
// either define their own FP_CONST[] with a matching layout, or use
// inline literals — we map FPC_0 to literal 0.0f for safety.
#  ifndef FPC_0
#    define FPC_0                             (0.0f)
#  endif

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

// PROGMEM float-array access — plugin uses GFLT to apply EXEC_OFFSET
// to a const-float-array pointer (e.g. `*GFLT(&UVA_RESPONSIVITY[i])`).
// Native: array is directly readable as flash-mapped data, just
// pass the address through.
#  define GFLT(addr)                          (addr)

// Module-decl macros — plugin-only. Native: empty so MODULE_PART
// forward decls reduce to plain C++ forward decls.
#  define PUSH_OPTIONS                        /* empty */
#  define PULL_OPTIONS                        /* empty */
#  define MODULE_PART                         /* empty */
#  define MODULE_END                          /* empty */

// Sensor-name lookup — plugin's `Plugin_Get_SensorNames(buf, id)`
// fetches a display name from the BinPlugin runtime keyed on a
// numeric sensor-type ID. Native: rewrite as a pure conditional-
// expression macro returning a string literal directly.
//
// Why not a static inline function? The compat header is included
// at the very top of every dual-format driver, BEFORE Arduino.h /
// strcpy_P / PSTR / uint32_t are pulled in. A static-inline body
// that uses any of those breaks any dual driver that ends up
// compiled with BUILD_AS_PLUGIN=0 (because !BUILD_AS_PLUGIN drops
// the `#include "module.h"` that would have brought them in).
// The conditional-expression form below has no body to parse — it
// resolves entirely in the preprocessor / constant-fold phase.
//
// `buf` is unused (the macro returns a string literal directly).
// Call sites pass `s1` as a placeholder for ABI compatibility with
// the plugin form's char-buffer-fill semantic; in native it's
// silently elided.
#  ifndef iD_TEMPERATURE
#    define iD_TEMPERATURE                    1
#    define iD_PRESSURE                       2
#    define iD_HUMIDITY                       3
#    define iD_ABSOLUTE_HUMIDITY               4
#    define iD_DEWPOINT                       5
#    define iD_ILLUMINANCE                    6
#    define iD_DISTANCE                       7
#    define iD_ECO2                           8
#    define iD_TVOC                           9
#    define Plugin_Get_SensorNames(buf, id)                     \
       (((id) == iD_TEMPERATURE)        ? "Temperature"   :     \
        ((id) == iD_PRESSURE)           ? "Pressure"      :     \
        ((id) == iD_HUMIDITY)           ? "Humidity"      :     \
        ((id) == iD_ABSOLUTE_HUMIDITY)  ? "Abs.Humid"     :     \
        ((id) == iD_DEWPOINT)           ? "DewPoint"      :     \
        ((id) == iD_ILLUMINANCE)        ? "Illuminance"   :     \
        ((id) == iD_DISTANCE)           ? "Distance"      :     \
        ((id) == iD_ECO2)               ? "eCO2"          :     \
        ((id) == iD_TVOC)               ? "TVOC"          :     \
                                          "Sensor")
#  endif

// ICONST — plugin uses this for integer constants accessed via
// PROGMEM (similar to FLTC). Native: integers are inline literals,
// no PROGMEM detour needed. Just pass the value through.
#  define ICONST(x)                           (x)

// Integer division — plugin uses tmod__udivsi3 (jt[174-ish]) for
// unsigned int32 division. Native: hardware /.
#  define tmod__udivsi3(a, b)                 ((uint32_t)(a) / (uint32_t)(b))

// 64-bit multiply / float→uint conversion / float multiply —
// plugin's `tmod__muldi3 / tmod__fixunssfsi / tmod__mulsf3` route
// through softfloat / softint jumptable wrappers (jt[82], jt[83],
// jfmul). Native: hardware ops.
#  define tmod__muldi3(a, b)                  ((int64_t)(a) * (int64_t)(b))
#  define tmod__fixunssfsi(x)                 ((uint32_t)(float)(x))
#  define tmod__mulsf3(a, b)                  ((float)(a) * (float)(b))
#  define tmod__umodsi3(a, b)                 ((uint32_t)(a) % (uint32_t)(b))

// TasmotaGlobal access — plugin uses pointer (`->`); native uses
// member-access (`.`). Drivers that touch TasmotaGlobal in shared
// code should `#if BUILD_AS_PLUGIN`-branch the access (see
// xsns_14_sht3x_dual.cpp's SHT3X_Show for an example).

#endif  // !BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Bus-pointer helper (mode-independent). Some chip-driver classes
// (e.g. Adafruit_TCS34725) accept a `TwoWire *` to pin themselves
// to a specific I2C bus. `_DUAL_WIRE_FOR(bus)` resolves to &Wire or
// &Wire1 by index — works in plugin AND native mode. ESP8266 only
// has &Wire so it always returns that.
// --------------------------------------------------------------------
#ifndef _DUAL_WIRE_FOR
#  ifdef ESP32
#    define _DUAL_WIRE_FOR(bus)               ((bus) == 1 ? &Wire1 : &Wire)
#  else
#    define _DUAL_WIRE_FOR(bus)               (&Wire)
#  endif
#endif

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
