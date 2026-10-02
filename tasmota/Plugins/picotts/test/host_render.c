/*
 * host_render.c - renders text to 16 bit PCM with the SVOX Pico engine on the host.
 *
 * Purpose: feasibility test for moving PicoTTS into a loadable BinPlugin. It drives
 * the engine the same way esp_picotts.c does (pico_initialize, two resources from
 * memory, voice definition, engine, putTextUtf8/getData), but without FreeRTOS, so
 * the engine can be compared bit for bit between build variants.
 *
 *   cc -O2 -I$PICO/lib -I$PICO ... host_render.c  (see build.sh)
 *   ./host_render <ta.bin> <sg.bin> "text" out.raw
 *
 * Variant switch: -DPICO_FLOATMATH replaces the double math of the engine by the
 * single precision functions that the plugin jump table offers (sinf, cosf, expf,
 * sqrtf), which is what a plugin build would have to use.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "picoapi.h"
#include "picoapid.h"
#include "esp_picorsrc.h"
#include "picoos.h"

#ifdef PICO_FLOATMATH
picoos_double picoos_quick_exp(const picoos_double y) { return (picoos_double)expf((float)y); }
#else
picoos_double picoos_quick_exp(const picoos_double y) { return (picoos_double)exp(y); }
#endif

static uint8_t *slurp(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *p = (uint8_t *)malloc(n);
  if (fread(p, 1, n, f) != (size_t)n) { fprintf(stderr, "short read %s\n", path); exit(2); }
  fclose(f);
  *size = (size_t)n;
  return p;
}

#define CHECK(msg) if (ret) { pico_Retstring m; pico_getSystemStatusMessage(sys, ret, m); \
  fprintf(stderr, "%s (%d): %s\n", msg, ret, m); return 1; }

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: %s ta.bin sg.bin \"text\" out.raw\n", argv[0]); return 2; }
  size_t tasz, sgsz;
  uint8_t *ta = slurp(argv[1], &tasz), *sg = slurp(argv[2], &sgsz);
  const unsigned arena = 1100000;
  void *mem = getenv("PICO_ZERO") ? calloc(1, arena) : malloc(arena);
  if (!getenv("PICO_ZERO") && getenv("PICO_FILL")) memset(mem, atoi(getenv("PICO_FILL")), arena);
  pico_System sys = NULL;
  pico_Resource rta = NULL, rsg = NULL;
  pico_Engine eng = NULL;
  static const pico_Char voice[] = "PicoVoice";
  int ret;

  ret = pico_initialize(mem, arena, &sys);                     CHECK("init");
  ret = esp_pico_loadResource(sys, ta, &rta);                  CHECK("load ta");
  ret = esp_pico_loadResource(sys, sg, &rsg);                  CHECK("load sg");
  ret = pico_createVoiceDefinition(sys, voice);                CHECK("voice");
  pico_Retstring name;
  ret = pico_getResourceName(sys, rta, name);                  CHECK("ta name");
  ret = pico_addResourceToVoiceDefinition(sys, voice, (const pico_Char *)name); CHECK("ta add");
  ret = pico_getResourceName(sys, rsg, name);                  CHECK("sg name");
  ret = pico_addResourceToVoiceDefinition(sys, voice, (const pico_Char *)name); CHECK("sg add");
  ret = pico_newEngine(sys, voice, &eng);                      CHECK("engine");

  FILE *out = fopen(argv[4], "wb");
  const char *text = argv[3];
  size_t left = strlen(text) + 1;           /* include the terminator: flushes the last sentence */
  const uint8_t *tp = (const uint8_t *)text;
  uint32_t samples = 0, hash = 2166136261u;
  while (left) {
    int16_t used = 0;
    ret = pico_putTextUtf8(eng, tp, (int16_t)left, &used);     CHECK("put");
    tp += used; left -= used;
    int status;
    do {
      int16_t buf[128]; int16_t bytes = 0, type = 0;
      status = pico_getData(eng, buf, sizeof(buf), &bytes, &type);
      if (bytes > 0) {
        fwrite(buf, 1, bytes, out);
        for (int i = 0; i < bytes / 2; i++) { hash = (hash ^ (uint16_t)buf[i]) * 16777619u; }
        samples += bytes / 2;
      }
    } while (status == PICO_STEP_BUSY);
    if (status != PICO_STEP_IDLE) { fprintf(stderr, "getData status %d\n", status); return 1; }
  }
  fclose(out);
  printf("samples=%u hash=%08x\n", samples, hash);
  return 0;
}
