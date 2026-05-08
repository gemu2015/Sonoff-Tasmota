/*
  xdrv_42_8_i2s_picotts.ino - PicoTTS text-to-speech via I2S (PoC).

  Copyright (C) 2026  Gerhard Mutz

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

// =====================================================================
// PicoTTS proof-of-concept integration.
//
// Wraps SVOX picotts (Apache 2.0; sources at lib/libesp32_div/pico/
// from upstream Android AOSP, glued into ESP32 land via DiUS/esp-picotts).
// One language compiled in (en-US, ~1.4 MB voice data); selectable later
// via a build flag once we move to LittleFS-loaded voices.
//
// Why this driver opens its own I2S channel instead of going through
// the existing audio_i2s.out abstraction:
// the tasmota32s3-devkit env loads I2S audio support via BinPlugins
// (xdrv_42_i2s.cpp gated on USE_I2S_MOD + EXECUTE_FROM_BINARY) — the
// audio_i2s global isn't compiled into the main firmware, so any
// `audio_i2s.out->...` reference fails to link. Standalone I2S TX
// keeps picotts self-contained and lets it run without the I2S
// binplugin loaded.
//
// Build requirements:
//   * ESP32-S3 with PSRAM and ≥ 16 MB flash
//   * USE_PICOTTS defined to enable this driver
//
// Build-flag pin overrides (defaults match the WM8960 audio board on
// .39: BCK=10 / WS=18 / DOUT=17; DIN=16 is reserved for the codec's
// mic ADC and not used by this TX-only TTS path):
//   -DPICOTTS_BCLK_PIN=N      bit clock          (default 10)
//   -DPICOTTS_LRCLK_PIN=N     word-select / LR   (default 18)
//   -DPICOTTS_DOUT_PIN=N      data out           (default 17)
//
// Console commands:
//   I2STTS <utf8 text>            synthesise + play `text` once
//   I2STTSPin <bclk> <ws> <dout>  override pins at runtime (volatile;
//                                  re-init required, takes effect on
//                                  next I2STTS call)
//
// Resource use (measured on ESP32-S3 + 8 MB PSRAM):
//   * picotts engine code            ~250-350 KB flash
//   * en-US voice data               ~1.4 MB flash (compiled-in)
//   * picotts working memory         1.1 MB heap (PSRAM-friendly)
//   * I2S TX DMA buffer              8 × 512 stereo frames = ~16 KB
//   * Synthesis CPU                  ~3-5× realtime (TBD on real board)
//
// Architecture:
//   1. First I2STTS call calls PicoTtsLazyInit() which spawns a
//      FreeRTOS task (priority 5, no fixed core) that pulls UTF-8
//      chars off a queue and feeds them to picotts_putTextUtf8().
//   2. Whenever picotts has 16-bit mono PCM ready, its task calls
//      `picotts_audio_cb(samples, count)` directly. We push that into
//      a PSRAM ring buffer (16 KB ≈ 250 ms of audio).
//   3. FUNC_LOOP drains the ring → I2S TX, duplicating mono → stereo
//      and converting 16 kHz native to whatever rate the I2S channel
//      runs at (we just match: 16 kHz stereo, 16-bit per sample).
//   4. Idle callback fires when picotts has no more text → we
//      drain the ring then close the I2S channel until the next
//      I2STTS call.
//
// PoC limitations (not in this commit):
//   * Multi-language switching at runtime — needs LittleFS-loaded voices
//   * Volume / pitch / rate control via SVOX command markers in text
//   * Cancel / interrupt synthesis mid-sentence
//   * Streaming long text without queue overflow (queue is 1 KB chars)
//   * Picking up pin assignments from Tasmota's GPIO template
// =====================================================================

#if defined(USE_PICOTTS)

#define XDRV_125  125

extern "C" {
#include "picotts.h"
}

#include <driver/i2s_std.h>
#include <freertos/ringbuf.h>
// WM8960 codec is gated separately — many ESP32-S3 audio boards use
// codec-less DACs (MAX98357, PCM5102, UDA1334) that don't need any
// I2C config; for those, leave USE_WM8960_CODEC undefined and the
// driver skips the I2C dance entirely. Define USE_WM8960_CODEC in
// platformio_override.ini's env build_flags if your board uses it.
#ifdef USE_WM8960_CODEC
#include <Wire.h>
#include "wm8960.h"     // codec init + gain control (lib/lib_deprecated/wm8960)
#endif
#ifdef USE_UFILESYS
extern FS *ffsp;       // Tasmota's LittleFS handle (mounted at boot
                       // when USE_UFILESYS is enabled).
#endif

// Voice file naming convention. Both files per language live on
// LittleFS; the driver picks the pair based on the current language
// code. Upload via:
//   curl -F "file=@picotts_en-US_ta.bin" http://<ip>/u3
//   curl -F "file=@picotts_en-US_sg.bin" http://<ip>/u3
//   curl -F "file=@picotts_de-DE_ta.bin" http://<ip>/u3
//   curl -F "file=@picotts_de-DE_sg.bin" http://<ip>/u3
#ifndef PICOTTS_DEFAULT_LANG
#define PICOTTS_DEFAULT_LANG  "en-US"
#endif

// Default pins — match the user's WM8960-board wiring on .39:
//   GPIO 10 = BCK (bit clock)
//   GPIO 18 = WS  (word-select / LRCLK)
//   GPIO 17 = DOUT (data out → codec)
//   GPIO 16 = DIN  (data in ← codec mic, unused on TX-only TTS path)
// Override at build time via -DPICOTTS_BCLK_PIN=N etc., or at runtime
// via the I2STTSPin command.
#ifndef PICOTTS_BCLK_PIN
#define PICOTTS_BCLK_PIN   10
#endif
#ifndef PICOTTS_LRCLK_PIN
#define PICOTTS_LRCLK_PIN  18
#endif
#ifndef PICOTTS_DOUT_PIN
#define PICOTTS_DOUT_PIN   17
#endif

// PSRAM ring buffer between picotts task and main loop. Sized for ~250 ms
// of buffered audio at 16 kHz / 16-bit / mono so synthesis can run ahead
// of playback during sentence-start bursts without blocking.
#ifndef PICOTTS_PCM_RING_BYTES
#define PICOTTS_PCM_RING_BYTES (16 * 1024)
#endif

// How many bytes to drain per FUNC_LOOP invocation. 4 KB = 2 KB samples
// = ~125 ms of mono audio — within Tasmota's loop-tolerance budget.
#define PICOTTS_DRAIN_CHUNK_BYTES 4096

struct PicoTtsRuntime {
  bool             initialized   = false;
  bool             init_failed   = false;
  bool             synthesizing  = false;        // raised on first audio_cb,
                                                  // lowered on idle_cb
  RingbufHandle_t  pcm_ring      = nullptr;
  i2s_chan_handle_t tx_handle    = nullptr;      // own I2S channel
  bool             tx_enabled    = false;
  uint8_t          bclk_pin      = PICOTTS_BCLK_PIN;
  uint8_t          lrclk_pin     = PICOTTS_LRCLK_PIN;
  uint8_t          dout_pin      = PICOTTS_DOUT_PIN;
  uint32_t         bytes_pushed  = 0;             // diag — total samples queued
  uint32_t         bytes_drained = 0;             // diag — total samples played
  // Voice data loaded from LittleFS at init, freed on shutdown.
  uint8_t         *ta_buf        = nullptr;
  size_t           ta_size       = 0;
  uint8_t         *sg_buf        = nullptr;
  size_t           sg_size       = 0;
  // Currently-loaded language code (e.g. "en-US", "de-DE"). Changing
  // this requires picotts shutdown + voice reload + picotts re-init.
  char             lang[8]       = PICOTTS_DEFAULT_LANG;
  bool             codec_inited  = false;        // WM8960 codec — init
                                                  // once (i2c writes
                                                  // are idempotent but
                                                  // wasteful on each
                                                  // I2STTS call)
};
static PicoTtsRuntime *picotts = nullptr;

// Read a file from LittleFS into a freshly-allocated PSRAM buffer.
// Returns true on success; on failure logs the error, leaves *buf=nullptr.
static bool PicoTtsLoadFromFS(const char *path, uint8_t **buf, size_t *size) {
  *buf = nullptr; *size = 0;
#ifdef USE_UFILESYS
  if (!ffsp) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: LittleFS not mounted"));
    return false;
  }
  if (!ffsp->exists(path)) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: %s missing — upload via /u3"), path);
    return false;
  }
  File f = ffsp->open(path, "r");
  if (!f) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: open(%s) failed"), path);
    return false;
  }
  size_t sz = f.size();
  if (sz < 16 || sz > 2 * 1024 * 1024) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: %s size %u out of range (16..2 MB)"),
           path, (unsigned)sz);
    f.close();
    return false;
  }
  // Always alloc in PSRAM — voice files are 500 KB-1 MB each, too big
  // for internal SRAM. The picotts engine reads from this pointer
  // directly (memory-mapped style), so the buffer must outlive the
  // engine. Owned by the runtime struct, freed in cleanup.
  uint8_t *p = (uint8_t *)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: PSRAM alloc %u B for %s failed"),
           (unsigned)sz, path);
    f.close();
    return false;
  }
  size_t got = f.read(p, sz);
  f.close();
  if (got != sz) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: short read %u/%u from %s"),
           (unsigned)got, (unsigned)sz, path);
    free(p);
    return false;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("PTT: loaded %s (%u B) → PSRAM"),
         path, (unsigned)sz);
  *buf  = p;
  *size = sz;
  return true;
#else
  AddLog(LOG_LEVEL_ERROR, PSTR("PTT: USE_UFILESYS not enabled — can't read voice"));
  return false;
#endif
}

// ─── picotts task callbacks (run on the picotts FreeRTOS task) ────────
static void picotts_audio_cb(int16_t *samples, unsigned count) {
  if (!picotts || !picotts->pcm_ring) return;
  size_t want = count * sizeof(int16_t);
  // Block briefly if ring is momentarily full — synthesis is faster than
  // playback during the first burst of a sentence.
  xRingbufferSend(picotts->pcm_ring, samples, want, pdMS_TO_TICKS(500));
  picotts->bytes_pushed += want;
  picotts->synthesizing = true;
}

static void picotts_idle_cb(void) {
  if (picotts) picotts->synthesizing = false;
  AddLog(LOG_LEVEL_DEBUG, PSTR("PTT: synthesis idle (pushed=%u drained=%u)"),
         picotts ? picotts->bytes_pushed  : 0,
         picotts ? picotts->bytes_drained : 0);
}

static void picotts_error_cb(void) {
  AddLog(LOG_LEVEL_ERROR, PSTR("PTT: picotts task aborted"));
  if (picotts) picotts->synthesizing = false;
}

// ─── I2S channel lifecycle (open on first frame, close on idle) ───────
static bool PicoTtsI2SOpen() {
  if (!picotts || picotts->tx_handle) return picotts && picotts->tx_handle;

  i2s_chan_config_t chan_cfg =
    I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 8;
  chan_cfg.dma_frame_num = 512;

  if (i2s_new_channel(&chan_cfg, &picotts->tx_handle, NULL) != ESP_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: i2s_new_channel failed"));
    picotts->tx_handle = nullptr;
    return false;
  }

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(PICOTTS_SAMPLE_FREQ_HZ),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)picotts->bclk_pin,
      .ws   = (gpio_num_t)picotts->lrclk_pin,
      .dout = (gpio_num_t)picotts->dout_pin,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
    },
  };

  if (i2s_channel_init_std_mode(picotts->tx_handle, &std_cfg) != ESP_OK) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: i2s_channel_init_std_mode failed"));
    i2s_del_channel(picotts->tx_handle);
    picotts->tx_handle = nullptr;
    return false;
  }
  i2s_channel_enable(picotts->tx_handle);
  picotts->tx_enabled = true;
  AddLog(LOG_LEVEL_INFO,
         PSTR("PTT: I2S TX up (bclk=%d ws=%d dout=%d rate=%d)"),
         picotts->bclk_pin, picotts->lrclk_pin, picotts->dout_pin,
         (int)PICOTTS_SAMPLE_FREQ_HZ);
  return true;
}

static void PicoTtsI2SClose() {
  if (!picotts || !picotts->tx_handle) return;
  if (picotts->tx_enabled) {
    i2s_channel_disable(picotts->tx_handle);
    picotts->tx_enabled = false;
  }
  i2s_del_channel(picotts->tx_handle);
  picotts->tx_handle = nullptr;
}

// Tear-down the picotts engine, free voice + ring buffers, but keep the
// runtime struct (lang, pin config, codec_inited flag survive). Used for
// language switching and on init error.
static void PicoTtsShutdownEngine() {
  if (!picotts) return;
  if (picotts->initialized) {
    picotts_shutdown();
    picotts->initialized = false;
  }
  picotts_set_resources(NULL, NULL);
  if (picotts->pcm_ring) {
    vRingbufferDeleteWithCaps(picotts->pcm_ring);
    picotts->pcm_ring = nullptr;
  }
  if (picotts->ta_buf) { free(picotts->ta_buf); picotts->ta_buf = nullptr; }
  if (picotts->sg_buf) { free(picotts->sg_buf); picotts->sg_buf = nullptr; }
  picotts->ta_size = 0;
  picotts->sg_size = 0;
  picotts->synthesizing = false;
  picotts->bytes_pushed = 0;
  picotts->bytes_drained = 0;
  PicoTtsI2SClose();
}

// ─── one-shot lazy init (also re-runs after a language switch) ────────
static bool PicoTtsLazyInit() {
  if (picotts && picotts->initialized) return true;
  if (picotts && picotts->init_failed) return false;
  if (!picotts) picotts = new PicoTtsRuntime();

#ifdef USE_WM8960_CODEC
  // Step 1: bring up the WM8960 audio codec via I2C. Without this
  // sequence, BCLK/LRCLK/DOUT carry the data but the DAC silicon is
  // muted/asleep and no sound comes out the analog jack. Tasmota's
  // primary `Wire` is set up from the GPIO template at boot — assumes
  // the user has SDA/SCL configured before flashing this firmware.
  // Idempotent (i2c writes are pure register pokes) so we still gate
  // on codec_inited to avoid the ~1 ms hit on every TTS call.
  //
  // For boards with a codec-less I2S DAC (MAX98357A class-D amp,
  // PCM5102, UDA1334), leave USE_WM8960_CODEC undefined — the
  // BCLK/WS/DOUT signals are already self-clocking and the DAC starts
  // converting on the first frame.
  if (!picotts->codec_inited) {
    W8960_Init(&Wire);
    picotts->codec_inited = true;
    AddLog(LOG_LEVEL_INFO, PSTR("PTT: WM8960 codec initialised on Wire (addr 0x1A)"));
  }
#endif

  // Step 2: load language-specific voice data from LittleFS into PSRAM.
  // Both files (text-analysis + signal-generator) are required.
  char ta_path[40], sg_path[40];
  snprintf(ta_path, sizeof(ta_path), "/picotts_%s_ta.bin", picotts->lang);
  snprintf(sg_path, sizeof(sg_path), "/picotts_%s_sg.bin", picotts->lang);
  if (!PicoTtsLoadFromFS(ta_path, &picotts->ta_buf, &picotts->ta_size) ||
      !PicoTtsLoadFromFS(sg_path, &picotts->sg_buf, &picotts->sg_size)) {
    if (picotts->ta_buf) { free(picotts->ta_buf); picotts->ta_buf = nullptr; }
    if (picotts->sg_buf) { free(picotts->sg_buf); picotts->sg_buf = nullptr; }
    picotts->init_failed = true;
    return false;
  }
  // Hand the buffers to the picotts engine via the runtime override.
  // Must come BEFORE picotts_init() so its internal `find_*_bin_start()`
  // sees them.
  picotts_set_resources(picotts->ta_buf, picotts->sg_buf);

  // Step 3: PCM ring buffer (PSRAM). Bounded for ~250 ms of audio at
  // 16 kHz mono 16-bit so synthesis can run ahead of playback during
  // sentence-start bursts without blocking the picotts task.
  picotts->pcm_ring = xRingbufferCreateWithCaps(
      PICOTTS_PCM_RING_BYTES, RINGBUF_TYPE_BYTEBUF,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!picotts->pcm_ring) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: ring alloc fail (need %d B PSRAM)"),
           PICOTTS_PCM_RING_BYTES);
    picotts->init_failed = true;
    return false;
  }

  picotts_set_idle_notify(picotts_idle_cb);
  picotts_set_error_notify(picotts_error_cb);

  // Step 4: launch the picotts engine task. Priority 5, no core
  // affinity. picotts_init internally allocates ~1.1 MB for engine
  // memory — spills to PSRAM on this board.
  if (!picotts_init(5, picotts_audio_cb, -1)) {
    AddLog(LOG_LEVEL_ERROR, PSTR("PTT: picotts_init failed"));
    if (picotts->pcm_ring) {
      vRingbufferDeleteWithCaps(picotts->pcm_ring);
      picotts->pcm_ring = nullptr;
    }
    picotts_set_resources(NULL, NULL);
    if (picotts->ta_buf) { free(picotts->ta_buf); picotts->ta_buf = nullptr; }
    if (picotts->sg_buf) { free(picotts->sg_buf); picotts->sg_buf = nullptr; }
    picotts->init_failed = true;
    return false;
  }

  picotts->initialized = true;
  AddLog(LOG_LEVEL_INFO, PSTR("PTT: ready (lang=%s, voice=%u+%u B from LittleFS)"),
         picotts->lang, (unsigned)picotts->ta_size, (unsigned)picotts->sg_size);
  return true;
}

// ─── FUNC_LOOP drain: ring → I2S TX, mono→stereo on the way out ──────
static void PicoTtsLoopDrain() {
  if (!picotts || !picotts->initialized || !picotts->pcm_ring) return;

  size_t got = 0;
  uint8_t *chunk = (uint8_t *)xRingbufferReceiveUpTo(
      picotts->pcm_ring, &got, 0, PICOTTS_DRAIN_CHUNK_BYTES);
  if (!chunk) {
    // Nothing to drain. Auto-close I2S if synthesis is also idle.
    if (picotts->tx_handle && !picotts->synthesizing) {
      PicoTtsI2SClose();
      AddLog(LOG_LEVEL_DEBUG, PSTR("PTT: I2S closed"));
    }
    return;
  }

  if (!PicoTtsI2SOpen()) {
    vRingbufferReturnItem(picotts->pcm_ring, chunk);
    return;
  }

  // Convert mono int16 → interleaved stereo int16 (L=R=mono sample).
  size_t mono_frames   = got / sizeof(int16_t);
  size_t stereo_bytes  = mono_frames * 2 * sizeof(int16_t);
  // Stack-allocate up to 4 KB of stereo (2 KB mono frames × 2 channels)
  // The ring chunk is bounded by PICOTTS_DRAIN_CHUNK_BYTES = 4 KB so
  // the stereo expansion is at most 8 KB — fine on the loop task stack
  // (8 KB on ESP32-S3 default).
  static int16_t stereo[PICOTTS_DRAIN_CHUNK_BYTES];   // 4 KB / 2 = 2 KB samples
                                                       // = 1 KB stereo pairs
                                                       // Actually need 2 × MONO,
                                                       // so size as bytes (good)
  int16_t *src = (int16_t *)chunk;
  for (size_t i = 0; i < mono_frames; i++) {
    stereo[i * 2 + 0] = src[i];
    stereo[i * 2 + 1] = src[i];
  }

  size_t written = 0;
  i2s_channel_write(picotts->tx_handle, stereo, stereo_bytes, &written,
                    pdMS_TO_TICKS(200));
  vRingbufferReturnItem(picotts->pcm_ring, chunk);
  picotts->bytes_drained += got;
}

// ─── console commands ────────────────────────────────────────────────
void CmndI2SPicoTTS(void) {
  if (XdrvMailbox.data_len <= 0) {
    ResponseCmndChar("Usage: I2STTS <utf8 text>");
    return;
  }
  if (!PicoTtsLazyInit()) {
    ResponseCmndChar("PTT: init failed (need ESP32-S3 + PSRAM)");
    return;
  }
  // Append a NUL so picotts treats this as a complete sentence and
  // starts emitting audio promptly.
  picotts_add(XdrvMailbox.data, XdrvMailbox.data_len);
  static const char nul = '\0';
  picotts_add(&nul, 1);
  ResponseCmndChar(XdrvMailbox.data);
}

void CmndI2SPicoTTSPin(void) {
  // I2STTSPin <bclk> <ws> <dout> — re-pin (forces I2S close on next loop).
  int bclk, ws, dout;
  if (XdrvMailbox.data_len <= 0 ||
      sscanf(XdrvMailbox.data, "%d %d %d", &bclk, &ws, &dout) != 3) {
    if (picotts) {
      Response_P(PSTR("{\"%s\":{\"BCLK\":%d,\"WS\":%d,\"DOUT\":%d}}"),
                 XdrvMailbox.command, picotts->bclk_pin,
                 picotts->lrclk_pin, picotts->dout_pin);
    } else {
      ResponseCmndChar("Usage: I2STTSPin <bclk> <ws> <dout>");
    }
    return;
  }
  if (!picotts) picotts = new PicoTtsRuntime();
  // Force I2S re-open with new pins on next drain.
  PicoTtsI2SClose();
  picotts->bclk_pin  = (uint8_t)bclk;
  picotts->lrclk_pin = (uint8_t)ws;
  picotts->dout_pin  = (uint8_t)dout;
  Response_P(PSTR("{\"%s\":{\"BCLK\":%d,\"WS\":%d,\"DOUT\":%d}}"),
             XdrvMailbox.command, bclk, ws, dout);
}

void CmndI2SPicoTTSLang(void) {
  // I2STTSLang <code>      switch language ("en-US", "de-DE")
  // I2STTSLang             show the currently-selected language
  if (XdrvMailbox.data_len <= 0) {
    Response_P(PSTR("{\"%s\":\"%s\"}"),
               XdrvMailbox.command,
               picotts ? picotts->lang : PICOTTS_DEFAULT_LANG);
    return;
  }
  // Trim and bound the input. Codes are like "en-US" / "de-DE" — fixed
  // 5 chars; the lang[] field is sized for 7 + NUL (room for variants
  // like "en-GB" or future longer codes). Compare against the literal
  // size, NOT a `sizeof(ternary)` expression — in C++ both arms decay
  // to `char *` and sizeof returns the pointer size (4 on ESP32),
  // which would reject any code longer than 3 chars.
  const char *req = XdrvMailbox.data;
  if (strlen(req) >= 8) {
    ResponseCmndChar("PTT: language code too long");
    return;
  }
  if (!picotts) picotts = new PicoTtsRuntime();
  if (strcmp(picotts->lang, req) == 0 && picotts->initialized) {
    // Same language already loaded — no-op
    Response_P(PSTR("{\"%s\":\"%s\"}"), XdrvMailbox.command, picotts->lang);
    return;
  }
  // Tear down current engine + voice. Codec stays initialised. Engine
  // re-init happens on the next I2STTS call (or right now eagerly so
  // we can report success/failure synchronously).
  PicoTtsShutdownEngine();
  picotts->init_failed = false;
  strncpy(picotts->lang, req, sizeof(picotts->lang) - 1);
  picotts->lang[sizeof(picotts->lang) - 1] = '\0';
  if (!PicoTtsLazyInit()) {
    ResponseCmndChar("PTT: voice load failed (check /picotts_<lang>_ta.bin and _sg.bin on FS)");
    return;
  }
  Response_P(PSTR("{\"%s\":\"%s\"}"), XdrvMailbox.command, picotts->lang);
}

// =====================================================================
// XDRV_125 dispatcher — standalone, no dependency on the xdrv_42 family
// (which is gated by USE_I2S_AUDIO and ships as a binplugin on this
// build target). Picotts opens its own I2S TX channel above and runs
// its synthesis task independently.
// =====================================================================
const char kPicoTtsCommands[] PROGMEM = "|"   // empty prefix: I2S* are top-level
  "I2STTS|"
  "I2STTSPin|"
  "I2STTSLang";

void (*const PicoTtsCommand[])(void) PROGMEM = {
  &CmndI2SPicoTTS,
  &CmndI2SPicoTTSPin,
  &CmndI2SPicoTTSLang,
};

bool Xdrv125(uint32_t function) {
  bool result = false;
  switch (function) {
    case FUNC_LOOP:
      PicoTtsLoopDrain();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kPicoTtsCommands, PicoTtsCommand);
      break;
    case FUNC_ACTIVE:
      result = true;
      break;
  }
  return result;
}

#endif  // USE_PICOTTS
