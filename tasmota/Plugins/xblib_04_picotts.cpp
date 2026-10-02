/*
  xblib_04_picotts.cpp - the SVOX Pico text-to-speech engine as a BinPlugin (BLIB).

  The engine (lib/libesp32_div/pico, 41 kLOC, Apache 2.0) is amalgamated into one translation unit
  by picotts/tools/gen_picotts.py. This file holds what cannot be generated: the plugin discipline
  glue and the exported API.

      python3 tasmota/Plugins/build_plugin.py --plugin USE_PICOTTS_MOD --cpu esp32

  What the plugin contains - and what it does not:
    * contains: the pure synthesis engine (text analysis, prosody, signal generation)
    * does NOT contain: the FreeRTOS task, the text queue, the output callback and the voice files.
      They stay in the firmware (esp_picotts_blib.c) which runs the task and calls the exports below.
      That keeps this plugin free of RTOS calls and of any hardware.

  Exports (looked up by name through tc_blib_lookup, called by the firmware directly):
      picotts_open(arena, arena_size, ta_resource, sg_resource)  -> 0 / pico error
      picotts_put(text, len, &used)                              -> 0 / pico error
      picotts_get(out, max_bytes, &bytes)                        -> pico status (BUSY/IDLE/error)
      picotts_close()

  Copyright (C) 2026  Gerhard Mutz / claude  -  GPL v3 (Tasmota's license)
*/

#ifndef NDEBUG
#define NDEBUG 1
#endif

#include "tasmota_options.h"

#ifdef USE_PICOTTS_MOD

#define XBLIB_04  1

// memset/memcpy that the COMPILER emits (zero-init, struct copies) must not call the host's: redeclare
// the builtins with an asm label that points at the module-local copies below (same trick as xblib_03).
// It has to come BEFORE any header that declares or uses them, otherwise GCC keeps the first name.
#include <stddef.h>
#include <stdint.h>
extern "C" void *(memset)(void *, int, size_t) __asm__("pico_memset");
extern "C" void *(memcpy)(void *, const void *, size_t) __asm__("pico_memcpy");

#include "module.h"
#include "module_defines.h"

#include "picotts/engine/picotts_plugin_strings.h"

// ---- module memory ------------------------------------------------------------------------
// The strings blob first: PICO_S(n) points into it. Allocated with the module at pFUNC_INIT.
typedef struct {
  uint32_t    strs[PICO_STR_WORDS];
  void       *sys;            // pico_System
  void       *rta;            // pico_Resource (text analysis)
  void       *rsg;            // pico_Resource (signal generation)
  void       *eng;            // pico_Engine
} MODULE_MEMORY;

#define PICO_MEM ((MODULE_MEMORY *)gettbl()->mod_memory)
// A function used as a value (callback / vtable style struct member) is the address in the LINKED module.
// The module runs from a different address: add the execution offset, as xdrv_42 does for its callbacks.
#define PICO_FP(f) ((decltype(&f))((uintptr_t)&(f) + (uintptr_t)((FLASH_MODULE *)gettbl()->mod_addr)->execution_offset))
#define PICO_S(n) ((const char *)((const uint8_t *)PICO_MEM->strs + pico_soff_##n))

// -Os like the matter plugin: smaller, and the engine is not time critical on an S3
_Pragma("GCC push_options")
// no-tree-loop-distribute-patterns: otherwise GCC turns the string/memory loops of the platform layer
// back into strlen/memset/memcpy calls (endless recursion inside the very functions that implement them)
_Pragma("GCC optimize (\"-Os\", \"no-tree-loop-distribute-patterns\")")

// memset/memcpy the COMPILER emits (zero-init, struct copies) must not call the host's: redeclare
// the builtins with an asm label that points at the module-local copies (same trick as xblib_03)
extern "C" MODULE_PART __attribute__((optimize("no-tree-loop-distribute-patterns")))
void *pico_memset(void *d, int c, size_t n) {
  uint8_t *p = (uint8_t *)d;
  while (n--) { *p++ = (uint8_t)c; }
  return d;
}
extern "C" MODULE_PART __attribute__((optimize("no-tree-loop-distribute-patterns")))
void *pico_memcpy(void *d, const void *s, size_t n) {
  uint8_t *p = (uint8_t *)d; const uint8_t *q = (const uint8_t *)s;
  while (n--) { *p++ = *q++; }
  return d;
}

MODULE_PART int32_t mod_func_execute(uint32_t sel);

// ---- engine headers, platform layer, engine ------------------------------------------------
#define register          // C++17 has no register storage class
#include "picotts/engine/picoapi.h"
#include "picotts/engine/picoapid.h"
#include "picotts/engine/picoos.h"
#include "picotts/engine/picorsrc.h"
#include "picotts/engine/picotts_plugin_pal.h"
#include "picotts/engine/picotts_engine_c.h"

// ---- exported API --------------------------------------------------------------------------
MODULE_PART int32_t picotts_open(void *arena, uint32_t arena_size, const void *ta, const void *sg) {
  MODULE_MEMORY *m = PICO_MEM;
  pico_System sys = NULL;
  pico_Resource rta = NULL, rsg = NULL;
  pico_Engine eng = NULL;
  pico_Retstring name;
  int ret;

  ret = pico_initialize(arena, arena_size, &sys);
  if (ret) { return ret; }
  m->sys = sys;
  ret = esp_pico_loadResource(sys, (const char *)ta, &rta);
  if (ret) { return ret; }
  m->rta = rta;
  ret = esp_pico_loadResource(sys, (const char *)sg, &rsg);
  if (ret) { return ret; }
  m->rsg = rsg;
  // voice name built from characters: a string literal would live in the host's .rodata
  pico_Char voice[10];
  voice[0] = 'P'; voice[1] = 'i'; voice[2] = 'c'; voice[3] = 'o'; voice[4] = 'V';
  voice[5] = 'o'; voice[6] = 'i'; voice[7] = 'c'; voice[8] = 'e'; voice[9] = 0;
  ret = pico_createVoiceDefinition(sys, voice);
  return ret;
}

MODULE_PART int32_t picotts_close(void) {
  return 0;
}

MODULE_PART int32_t picotts_probe(void) { return 0x50494300; }     // 'PIC'

// ---- BLIB exports table --------------------------------------------------------------------
static const char NAME_PICOTTS_OPEN[]  PROGMEM = "picotts_open";
static const char NAME_PICOTTS_CLOSE[] PROGMEM = "picotts_close";
static const char NAME_PICOTTS_PROBE[] PROGMEM = "picotts_probe";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_PICOTTS_OPEN,  (void *)picotts_open,  0, TC_RET_INT, { TC_ARG_END } },
  { NAME_PICOTTS_CLOSE, (void *)picotts_close, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_PICOTTS_PROBE, (void *)picotts_probe, 0, TC_RET_INT, { TC_ARG_END } },
  { NULL, NULL, 0, 0, { 0 } }
};

MODULE_DESCRIPTOR("PICOTTS", MODULE_TYPE_BLIB, 1<<16|5,
                  "", 0, "", 0, "", 0, "", 0)

MODULE_END

// ---- dispatch ------------------------------------------------------------------------------
int32_t mod_func_execute(uint32_t sel) {
  if (sel == pFUNC_INIT) {
    ALLOCMEM;
    // texts: word copy from the module (mapped on the instruction bus, byte reads fault there)
    static const uint32_t blob[PICO_STR_WORDS] PROGMEM = { PICO_STR_BLOB_INIT };
    const volatile uint32_t *sb = (const volatile uint32_t *)((const uint8_t *)blob + EXEC_OFFSET);
    for (uint32_t i = 0; i < PICO_STR_WORDS; i++) {
      mem->strs[i] = sb[i];
    }
    initialized = 1;
    return 1;
  }
  if (sel == pFUNC_DEINIT) {
    GET_MTBL; GET_JT;
    RETMEM
    return 0;
  }
  if (sel == pFUNC_GET_RAM) {
    return 0;
  }
  if (sel == pFUNC_GET_TINYC_EXPORTS) {
    return (int32_t)(uintptr_t)&BLIB_EXPORTS[0];
  }
  return 0;
}

_Pragma("GCC pop_options")

#endif  // USE_PICOTTS_MOD
