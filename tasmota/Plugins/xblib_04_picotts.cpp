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

// Debug trace log: 8 words (32 bytes) in the shipped plugin; define PICOTTS_DEBUG and regenerate with
// gen_picotts.py --trace ... to record up to 2048 words (see picotts/test/README.md)
#ifdef PICOTTS_DEBUG
#define PICO_DBG_WORDS 2048
#else
#define PICO_DBG_WORDS 8
#endif

// ---- module memory ------------------------------------------------------------------------
// The strings blob first: PICO_S(n) points into it. Allocated with the module at pFUNC_INIT.
typedef struct {
  uint32_t    strs[PICO_STR_WORDS];
  void       *sys;            // pico_System
  void       *rta;            // pico_Resource (text analysis)
  void       *rsg;            // pico_Resource (signal generation)
  void       *eng;            // pico_Engine
  uint32_t    dbg[PICO_DBG_WORDS];   // PICO_TRACE log (debug aid, read with blibtest picotts_pv)
  uint32_t    ndbg, rd;
} MODULE_MEMORY;

#define PICO_MEM ((MODULE_MEMORY *)gettbl()->mod_memory)
// A function used as a value (callback / vtable style struct member) is the address in the LINKED module.
// The module runs from a different address: add the execution offset, as xdrv_42 does for its callbacks.
#define PICO_FP(f) ((decltype(&f))((uintptr_t)&(f) + (uintptr_t)((FLASH_MODULE *)gettbl()->mod_addr)->execution_offset))
#ifdef PICOTTS_TRACE_BARRIER
// experiment: evaluate the arguments and act as a compiler memory barrier, write nothing
#define PICO_TRACE(a, b) do { (void)(a); (void)(b); __asm__ __volatile__("" ::: "memory"); } while (0)
#else
#define PICO_TRACE(a, b) do { MODULE_MEMORY *tm_ = PICO_MEM; if (tm_->ndbg + 2 <= PICO_DBG_WORDS) { tm_->dbg[tm_->ndbg++] = (a); tm_->dbg[tm_->ndbg++] = (b); } } while (0)
#endif
#define PICO_S(n) ((const char *)((const uint8_t *)PICO_MEM->strs + pico_soff_##n))

// -Os: -O2 was tried (25 % bigger) and brings four blib_audit findings (merged literals, a ROM call)
// like the matter plugin: -Os instead of the plugins' usual -Og
_Pragma("GCC push_options")
// no-tree-loop-distribute-patterns: otherwise GCC turns the string/memory loops of the platform layer
// back into strlen/memset/memcpy calls (endless recursion inside the very functions that implement them)
_Pragma("GCC optimize (\"-Os\", \"no-tree-loop-distribute-patterns\")")

// memset/memcpy the COMPILER emits (zero-init, struct copies) must not call the host's: redeclare
// the builtins with an asm label that points at the module-local copies (same trick as xblib_03)
// the firmware's libc (jt[91] memset, jt[92] memmove) copies word wise; a byte loop in the module is
// several times slower on PSRAM
#define PICO_JT_EARLY (gettbl()->jt)
extern "C" MODULE_PART void *pico_memset(void *d, int c, size_t n) {
  return (( void *(*)(void *, int, size_t) ) PICO_JT_EARLY[91])(d, c, n);
}
extern "C" MODULE_PART void *pico_memcpy(void *d, const void *s, size_t n) {
  return (( void *(*)(void *, const void *, size_t) ) PICO_JT_EARLY[92])(d, s, n);
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
// The firmware (xdrv_123_plugins.ino, "PicoTTS from the BLIB") owns the task, the text queue, the
// arena and the voice data; these four functions are the synthesis steps.
MODULE_PART static void pico_voice_name(pico_Char *voice) {
  // built from characters: a string literal would live in the host's .rodata
  voice[0] = 'P'; voice[1] = 'i'; voice[2] = 'c'; voice[3] = 'o'; voice[4] = 'V';
  voice[5] = 'o'; voice[6] = 'i'; voice[7] = 'c'; voice[8] = 'e'; voice[9] = 0;
}

MODULE_PART int32_t picotts_close(void) {
  MODULE_MEMORY *m = PICO_MEM;
  pico_System sys = (pico_System)m->sys;
  pico_Char voice[10];
  pico_voice_name(voice);
  if (m->eng) { pico_Engine e = (pico_Engine)m->eng; pico_disposeEngine(sys, &e); pico_releaseVoiceDefinition(sys, voice); m->eng = NULL; }
  if (m->rsg) { pico_Resource r = (pico_Resource)m->rsg; esp_pico_unloadResource(sys, &r); m->rsg = NULL; }
  if (m->rta) { pico_Resource r = (pico_Resource)m->rta; esp_pico_unloadResource(sys, &r); m->rta = NULL; }
  if (sys) { pico_terminate(&sys); m->sys = NULL; }
  return 0;
}

MODULE_PART int32_t picotts_open(void *arena, uint32_t arena_size, const void *ta, const void *sg) {
  MODULE_MEMORY *m = PICO_MEM;
  pico_System sys = NULL;
  pico_Resource rta = NULL, rsg = NULL;
  pico_Engine eng = NULL;
  pico_Retstring name;
  pico_Char voice[10];
  int ret;

  pico_voice_name(voice);
  ret = pico_initialize(arena, arena_size, &sys);
  if (ret) { return ret; }
  m->sys = sys;
  ret = esp_pico_loadResource(sys, (const char *)ta, &rta);
  if (ret) { picotts_close(); return ret; }
  m->rta = rta;
  ret = esp_pico_loadResource(sys, (const char *)sg, &rsg);
  if (ret) { picotts_close(); return ret; }
  m->rsg = rsg;
  ret = pico_createVoiceDefinition(sys, voice);
  if (ret) { picotts_close(); return ret; }
  ret = pico_getResourceName(sys, rta, name);
  if (!ret) { ret = pico_addResourceToVoiceDefinition(sys, voice, (const pico_Char *)name); }
  if (!ret) { ret = pico_getResourceName(sys, rsg, name); }
  if (!ret) { ret = pico_addResourceToVoiceDefinition(sys, voice, (const pico_Char *)name); }
  if (!ret) { ret = pico_newEngine(sys, voice, &eng); }
  if (ret) { picotts_close(); return ret; }
  m->eng = eng;
  return 0;
}

// text in (UTF-8), returns pico status; *used = bytes consumed
MODULE_PART int32_t picotts_put(const uint8_t *text, int32_t len, int32_t *used) {
  int16_t u = 0;
  int ret = pico_putTextUtf8((pico_Engine)PICO_MEM->eng, text, (int16_t)len, &u);
  *used = u;
  return ret;
}

// samples out: returns PICO_STEP_BUSY (201) while more is coming, PICO_STEP_IDLE (200) when done
MODULE_PART int32_t picotts_get(int16_t *out, int32_t max_bytes, int32_t *bytes) {
  int16_t b = 0, type = 0;
  int ret = pico_getData((pico_Engine)PICO_MEM->eng, out, (int16_t)max_bytes, &b, &type);
  *bytes = b;
  return ret;
}

// Self check, called by the firmware at load time and logged. 0 = everything as expected, otherwise a
// bit mask: 1 text blob differs from the build, 2 fdiv, 4 sqrtf, 8 expf, 16 sinf, 32 cosf, 64 memmove.
MODULE_PART int32_t picotts_probe(void) {
  MODULE_MEMORY *m = PICO_MEM;
  int32_t bad = 0;
  uint32_t ck = 2166136261u;
  for (uint32_t i = 0; i < PICO_STR_WORDS; i++) { ck = (ck ^ m->strs[i]) * 16777619u; }
  if (ck != PICO_STR_CHECKSUM) { bad |= 1; }
  float a = PICO_FDIV(1.0f, 3.0f);
  float b = PICO_SQRTF(2.0f);
  float c = PICO_EXPF(1.0f);
  float d = PICO_SINF(1.0f);
  float e = PICO_COSF(1.0f);
  // single precision results with a tolerance of 1e-6 (compare via integer scaling, no float compare helpers)
  if ((int32_t)(a * 1000000.0f) != 333333)  { bad |= 2; }
  if ((int32_t)(b * 1000000.0f) != 1414213) { bad |= 4; }
  if ((int32_t)(c * 1000000.0f) != 2718281) { bad |= 8; }
  if ((int32_t)(d * 1000000.0f) != 841470)  { bad |= 16; }
  if ((int32_t)(e * 1000000.0f) != 540302)  { bad |= 32; }
  uint8_t buf[8];                                      // (an initialiser would be a copy from host .rodata)
  for (uint8_t i = 0; i < 8; i++) { buf[i] = (uint8_t)(i + 1); }
  picopal_mem_copy(buf, buf + 2, 6);                   // overlapping copy
  if (buf[2] != 1 || buf[7] != 6) { bad |= 64; }
  return bad;
}

// Debug aid: the float bits of the jump table math results, one per call. blibtest hands the hex
// string length in as `len`, so `blibtest picotts_pv 00` is index 1 ... `blibtest picotts_pv 0000000000` is 5.
MODULE_PART int32_t picotts_pv(uint8_t *buf, int len) {
  float v;
  switch (len) {
    case 1: v = PICO_FDIV(1.0f, 3.0f); break;
    case 2: v = PICO_SQRTF(2.0f); break;
    case 3: v = PICO_EXPF(1.0f); break;
    case 4: v = PICO_SINF(1.0f); break;
    case 5: v = PICO_COSF(1.0f); break;
    case 6: return (int32_t)PICO_MEM->ndbg;                    // number of trace words
    case 7: PICO_MEM->rd = 0; return 0;                        // rewind
    case 8: { uint32_t k = PICO_MEM->rd++; return (int32_t)(k < PICO_MEM->ndbg ? PICO_MEM->dbg[k] : 0xFFFFFFFFu); }  // next word
    default: PICO_MEM->ndbg = 0; PICO_MEM->rd = 0; return 0;   // 9: clear
  }
  union { float f; int32_t i; } u;
  u.f = v;
  return u.i;
}

// ---- BLIB exports table --------------------------------------------------------------------
static const char NAME_PICOTTS_OPEN[]  PROGMEM = "picotts_open";
static const char NAME_PICOTTS_CLOSE[] PROGMEM = "picotts_close";
static const char NAME_PICOTTS_PUT[]   PROGMEM = "picotts_put";
static const char NAME_PICOTTS_GET[]   PROGMEM = "picotts_get";
static const char NAME_PICOTTS_PROBE[] PROGMEM = "picotts_probe";
static const char NAME_PICOTTS_PV[]    PROGMEM = "picotts_pv";

const TC_EXPORT BLIB_EXPORTS[] PROGMEM = {
  { NAME_PICOTTS_OPEN,  (void *)picotts_open,  0, TC_RET_INT, { TC_ARG_END } },
  { NAME_PICOTTS_CLOSE, (void *)picotts_close, 0, TC_RET_INT, { TC_ARG_END } },
  { NAME_PICOTTS_PUT,   (void *)picotts_put,   0, TC_RET_INT, { TC_ARG_END } },
  { NAME_PICOTTS_GET,   (void *)picotts_get,   0, TC_RET_INT, { TC_ARG_END } },
  { NAME_PICOTTS_PV,    (void *)picotts_pv,    2, TC_RET_INT, { TC_ARG_BUF, TC_ARG_INT, TC_ARG_END } },
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
    if (mt->mod_memory && ((MODULE_MEMORY *)mt->mod_memory)->sys) { picotts_close(); }
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
