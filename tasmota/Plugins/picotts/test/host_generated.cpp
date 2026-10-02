/*
 * host_generated.cpp - runs the GENERATED plugin source (engine/picotts_engine_c.h) on the host.
 *
 * The plugin build cannot be debugged on the device, but the generated amalgam is plain C++ once the
 * plugin macros are given host definitions. This file stands in for xblib_04_picotts.cpp: MODULE_PART
 * is empty, gettbl() returns a fake module table whose jump table holds host versions of the entries
 * the plugin uses (jt[39] fdiv, [73] millis, [91] memset, [92] memmove, [205] sinf, [206] cosf,
 * [208] sqrtf, [215] expf), PICO_S points into the string blob, PICO_FP is the identity.
 *
 *   ./host_generated <ta.bin> <sg.bin> "text" out.raw
 */
#ifdef PICOTTS_HOST_TEST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <math.h>

#define MODULE_PART
#define PROGMEM
#define NDEBUG 1

#include "picotts/engine/picotts_plugin_strings.h"

typedef void (*jt_fn)(void);
static float h_fdiv(float a, float b) { return a / b; }
static uint32_t h_millis(void) { return 0; }
static void *h_memset(void *d, int c, size_t n) { return memset(d, c, n); }
static void *h_memmove(void *d, const void *s, size_t n) { return memmove(d, s, n); }
static float h_err = 0.0f;
static float h_sinf(float x) { return sinf(x) * (1.0f - 2.9e-5f * h_err); }
static float h_cosf(float x) { return cosf(x) * (1.0f + 9.0e-6f * h_err); }
static float h_sqrtf(float x) { return sqrtf(x) * (1.0f + 1.5e-6f * h_err); }
static float h_expf(float x) { return expf(x); }

typedef struct {
  uint32_t strs[PICO_STR_WORDS];
  void *sys, *rta, *rsg, *eng;
  uint32_t dbg[2048]; uint32_t ndbg, rd;
} MODULE_MEMORY;

static jt_fn jt_tab[256];
typedef struct { jt_fn *jt; void *mod_memory; void *mod_addr; } ModTable;
static MODULE_MEMORY g_mem;
static ModTable g_tbl = { jt_tab, &g_mem, 0 };
static ModTable *gettbl(void) { return &g_tbl; }

#define PICO_MEM ((MODULE_MEMORY *)gettbl()->mod_memory)
#define PICO_S(n) ((const char *)((const uint8_t *)PICO_MEM->strs + pico_soff_##n))
#define PICO_TRACE(a, b) do { MODULE_MEMORY *tm_ = PICO_MEM; if (tm_->ndbg < 2046) { tm_->dbg[tm_->ndbg++] = (a); tm_->dbg[tm_->ndbg++] = (b); } } while (0)
#define PICO_FP(f) (f)
#define register

#include "picotts/engine/picoapi.h"
#include "picotts/engine/picoapid.h"
#include "picotts/engine/picoos.h"
#include "picotts/engine/picorsrc.h"
#include "picotts/engine/picotts_plugin_pal.h"
#include "picotts/engine/picotts_engine_c.h"

static const uint32_t blob[PICO_STR_WORDS] = { PICO_STR_BLOB_INIT };

static uint8_t *slurp(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *p = (uint8_t *)malloc(n);
  if (fread(p, 1, n, f) != (size_t)n) { exit(2); }
  fclose(f); *size = n; return p;
}

/* The device runs the engine on a FreeRTOS task stack that starts filled with 0xA5. Leave that pattern
 * on the host stack too (PICO_POISON=<byte>), so reads of uninitialised locals behave like on the device. */
static void __attribute__((noinline)) poison_stack(int fill) {
  volatile unsigned char buf[96 * 1024];
  for (unsigned i = 0; i < sizeof(buf); i++) { buf[i] = (unsigned char)fill; }
}

static void voice_name(pico_Char *v) { memcpy(v, "PicoVoice", 10); }

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage\n"); return 2; }
  if (getenv("PICO_POISON")) { poison_stack(atoi(getenv("PICO_POISON"))); }
  if (getenv("PICO_ERR")) { h_err = (float)atof(getenv("PICO_ERR")); }   // emulate the firmware math precision
  jt_tab[39] = (jt_fn)h_fdiv;   jt_tab[73] = (jt_fn)h_millis;
  jt_tab[91] = (jt_fn)h_memset; jt_tab[92] = (jt_fn)h_memmove;
  jt_tab[205] = (jt_fn)h_sinf;  jt_tab[206] = (jt_fn)h_cosf;
  jt_tab[208] = (jt_fn)h_sqrtf; jt_tab[215] = (jt_fn)h_expf;
  memcpy(g_mem.strs, blob, sizeof(blob));

  size_t tasz, sgsz;
  uint8_t *ta = slurp(argv[1], &tasz), *sg = slurp(argv[2], &sgsz);
  const unsigned arena_size = 1100000;
  void *arena = calloc(1, arena_size);
  pico_System sys = NULL; pico_Resource rta = NULL, rsg = NULL; pico_Engine eng = NULL;
  pico_Retstring name; pico_Char voice[10]; voice_name(voice);
  int ret = pico_initialize(arena, arena_size, &sys);
  if (!ret) ret = esp_pico_loadResource(sys, (const char *)ta, &rta);
  if (!ret) ret = esp_pico_loadResource(sys, (const char *)sg, &rsg);
  if (!ret) ret = pico_createVoiceDefinition(sys, voice);
  if (!ret) ret = pico_getResourceName(sys, rta, name);
  if (!ret) ret = pico_addResourceToVoiceDefinition(sys, voice, (const pico_Char *)name);
  if (!ret) ret = pico_getResourceName(sys, rsg, name);
  if (!ret) ret = pico_addResourceToVoiceDefinition(sys, voice, (const pico_Char *)name);
  if (!ret) ret = pico_newEngine(sys, voice, &eng);
  if (ret) { fprintf(stderr, "open failed %d\n", ret); return 1; }

  FILE *out = fopen(argv[4], "wb");
  const uint8_t *tp = (const uint8_t *)argv[3];
  size_t left = strlen(argv[3]) + 1;
  uint32_t samples = 0, hash = 2166136261u;
  // like esp_pico_run: first push every queued byte, one per call, then drain the output
  while (left) {
    int16_t used = 0;
    ret = pico_putTextUtf8(eng, tp, 1, &used);
    if (ret) { fprintf(stderr, "put %d\n", ret); return 1; }
    tp += used; left -= used;
    if (getenv("PICO_SPLIT") && left == 1) {         // text fed, NUL still pending: drain first (a fast engine task does this)
      int st;
      do {
        int16_t b[128]; int16_t by = 0, ty = 0;
        st = pico_getData(eng, b, sizeof(b), &by, &ty);
        if (by > 0) { fwrite(b, 1, by, out); samples += by / 2; for (int i = 0; i < by / 2; i++) hash = (hash ^ (uint16_t)b[i]) * 16777619u; }
      } while (st == PICO_STEP_BUSY);
    }
    if (!getenv("PICO_DEVICEFEED")) {               // alternative: drain after every byte
      int st;
      do {
        int16_t b[128]; int16_t by = 0, ty = 0;
        st = pico_getData(eng, b, sizeof(b), &by, &ty);
        if (by > 0) { fwrite(b, 1, by, out); samples += by / 2; for (int i = 0; i < by / 2; i++) hash = (hash ^ (uint16_t)b[i]) * 16777619u; }
      } while (st == PICO_STEP_BUSY);
    }
  }
  {
    int status;
    do {
      int16_t buf[128]; int16_t bytes = 0, type = 0;
      status = pico_getData(eng, buf, sizeof(buf), &bytes, &type);
      if (bytes > 0) { fwrite(buf, 1, bytes, out); for (int i = 0; i < bytes / 2; i++) hash = (hash ^ (uint16_t)buf[i]) * 16777619u; samples += bytes / 2; }
    } while (status == PICO_STEP_BUSY);
  }
  fclose(out);
  if (getenv("PICO_DUMP")) { for (uint32_t i = 0; i < g_mem.ndbg; i++) printf("%u ", g_mem.dbg[i]); printf("\n"); }
  printf("samples=%u hash=%08x\n", samples, hash);
  return 0;
}
#endif /* PICOTTS_HOST_TEST */
