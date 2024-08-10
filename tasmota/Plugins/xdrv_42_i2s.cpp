/*
  xdrv_42_i2s.cpp - I2S audio support for Tasmota

  Copyright (C) 2024  gemu2015

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "tasmota_options.h"

#ifdef USE_I2S_MOD
#define XDRV_42 42

#include "module.h"
#include "module_defines.h"

#define USE_MP3

// RIFF header
typedef struct {
    uint32_t ChunkID; //"RIFF"
    uint32_t ChunkSize; //"36 + sizeof(wav_data_t) + data"
    uint32_t Format; // "WAV"
} wav_riff_t;

// FMT header
typedef struct {
    uint32_t Subchunk1ID; //"fmt "
    uint32_t Subchunk1Size; //16 (PCM)
    uint16_t AudioFormat; // 1 'cause PCM
    uint16_t NumChannels; // mono = 1; stereo = 2
    uint32_t SampleRate; // 8000, 44100, etc.
    uint32_t ByteRate; //== SampleRate * NumChannels * byte
    uint16_t BlockAlign; //== NumChannels * bytePerSample
    uint16_t BytesPerSample; //8 byte = 8, 16 byte = 16, etc.
} wav_fmt_t;

// Data header
typedef struct {
    uint32_t Subchunk2ID; //"data"
    uint32_t Subchunk2Size; //== NumSamples * NumChannels * bytePerSample/8
} wav_data_t;


// complete header
typedef struct {
    wav_riff_t Riff;
    wav_fmt_t Fmt;
    wav_data_t Data;
} wav_header_t;


#ifdef ESP32
#define USE_I2S_TASK
#endif

#ifdef USE_MP3
#include "mp3-decoder/mp3_decoder.h"
#endif


PUSH_OPTIONS

typedef struct {
  uint8_t dout_pin;
  uint8_t bck_pin;
  uint8_t ws_pin;
  uint8_t gain_div;
  void *i2sp;
  uint8_t busy;
  uint8_t mode;

#ifdef USE_MP3
  MP3_MEM mp3m;
#endif

} MODULE_MEMORY;

#define dout_pin mem->dout_pin
#define bck_pin mem->bck_pin
#define ws_pin mem->ws_pin
#define i2sp mem->i2sp
#define gain_div mem->gain_div
#define busy mem->busy
#define mode mem->mode
#define mp3m mem->mp3m

#define I2S_REV 1 << 16 | 4
#ifdef ESP8266
MODULE_DESCRIPTOR("I2SAUDIO", MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0)
#else
MODULE_DESCRIPTOR("I2SAUDIO", MODULE_TYPE_DRIVER, I2S_REV, "DOUT", 17, "BCK", 10, "WS", 18, "MODE", 0x01000200)
#endif

// all functions must be declared MUDULE_PART
MODULE_PART int32_t I2SAudio_Init();
MODULE_PART void I2sTask(void *arg);
MODULE_PART void I2S_PlayWave(void);
MODULE_PART bool mp3_begin();
MODULE_PART bool mp3_isRunning();
MODULE_PART bool mp3_loop();
MODULE_PART bool mp3_stop();
MODULE_PART void I2S_PlayMP3(void);
MODULE_PART void SetGain(void);
MODULE_PART void I2SAudio_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END



#ifdef USE_MP3
#if 1
#include "mp3-decoder/mp3_decoder_c.h"
#else
#include "libhelix-mp3/bitstream_c.h"
#include "libhelix-mp3/buffers_c.h"
#include "libhelix-mp3/dct32_c.h"
#include "libhelix-mp3/dequant_c.h"
#include "libhelix-mp3/dqchan_c.h"
#include "libhelix-mp3/huffman_c.h"
#include "libhelix-mp3/hufftabs_c.h"
#include "libhelix-mp3/imdct_c.h"
#include "libhelix-mp3/mp3dec_c.h"
#include "libhelix-mp3/mp3tabs_c.h"
#include "libhelix-mp3/polyphase_c.h"
#include "libhelix-mp3/scalfact_c.h"
#include "libhelix-mp3/stproc_c.h"
#include "libhelix-mp3/subband_c.h"
#include "libhelix-mp3/trigtabs_c.h"
#endif
#endif

const char S_JSON_FNF[] PROGMEM = "{\"File %s not found\"}";
const char S_JSON_ILLF[] PROGMEM = "{\"Illegal File format\"}";
#ifdef USE_MP3
const char S_JSON_MEMERR[] PROGMEM = "{\"out of memory\"}";
#endif
const char tname[] PROGMEM = "I2STASK";

int32_t I2SAudio_Init() {
  ALLOCMEM

  dout_pin = mp->ms[0].value;
  bck_pin = mp->ms[1].value;
  ws_pin = mp->ms[2].value;
  mode = mp->ms[3].value;

/*
  double dv = 12.34;
  int64_t iv = d2i64(dv);
  double xv = i642d(iv);
*/
  gain_div = 2;

#ifdef USE_MP3
  uint32_t mp3mem = MP3Decoder_AllocateBuffers();
  if (!mp3mem) {
    Response_P(GSTR(S_JSON_MEMERR));
    return false;
  }
  mt->mem_size += mp3mem;
#endif

  busy = false;

  initialized = true;
  return 0;
}


#ifdef USE_I2S_TASK
void I2sTask(void *path) {
  SETREGS
  File_p *wf;
  wf = fopen((char*)path, 'r');

  int16_t buffer[512];
  // skip header
  fread((char*)&buffer, 1, sizeof(wav_header_t), wf);

  while (1) {
    uint32_t bytesread = fread((char*)buffer, 1, sizeof(buffer), wf);
    if (!bytesread) {
      break;
    }
    for (uint32_t i = 0; i < bytesread / 2; i++) {
      buffer[i] /= gain_div;
    }
    i2s_write_samples(i2sp, buffer, bytesread / 2);
  }

  i2s_end(i2sp);
  
  fclose(wf);

  busy = false;
  
  vTaskDelete(0);
}
#endif

void I2S_PlayWave(void) {
  SETREGS

  if (busy) {
    return;
  }

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  File_p *wf;
  wf = fopen(cp, 'r');

  if (!wf) {
    // file not found
    Response_P(GSTR(S_JSON_FNF), cp);
    return;
  }
  wav_header_t wh;

  // check for RIFF
  fread((char*)&wh, 1, sizeof(wav_header_t), wf);
 
   // 0x52494646
  if (wh.Riff.ChunkID != 0x46464952 && wh.Fmt.NumChannels != 1) {
    fclose(wf);
    Response_P(GSTR(S_JSON_ILLF));
    return;
  }
  
  i2sp = i2s_begin(dout_pin, bck_pin, ws_pin, mode);
  // default is 1 channel
  i2s_set_rate(i2sp, wh.Fmt.SampleRate, mode, 1);

  busy = true;

#ifdef USE_I2S_TASK
  fclose(wf);

  TASKPARS tp;
  tp.pvTaskCode = GVOID(I2sTask);
  tp.constpcName = GSTR(tname);
  tp.usStackDepth = ICONST(8192);
  tp.constpvParameters = cp;
  tp.uxPriority = 3;
  tp.constpvCreatedTask = nullptr;
  tp.xCoreID = 1;
  int32_t err = xTaskCreatePinnedToCore(&tp);
#else

  int16_t buffer[512]; 
  while (1) {
    uint32_t bytesread = fread((char*)buffer, 1, sizeof(buffer), wf);
    if (!bytesread) {
      break;
    }
    for (uint32_t i = 0; i < bytesread / 2; i++) {
      buffer[i] /= gain_div;
    }
    i2s_write_samples(i2sp, buffer, bytesread / 2);
    OsWatchLoop();
  }

  i2s_end(i2sp);

  fclose(wf);

  busy = false;
#endif

  ResponseCmndDone();

  return;
}

void SetGain(void) {
  SETREGS
  uint8_t gain;

  if (XdrvMailbox->data_len > 0) {
    char *cp = XdrvMailbox->data;
    while (*cp == ' ') cp++;
    gain = strtol(cp, &cp, 10);
    if (gain > 100) {
        gain = 100;
    }
    if (gain < 1) {
      gain = 1;
    }
    gain_div = 100 / gain;
  } else {
    gain = 100 / gain_div;
  }
  ResponseCmndNumber(gain);

}

bool mp3_begin() {
  return 0;
}
bool mp3_isRunning() {
  return 0;
}

bool mp3_loop() {
  return 0;
}

bool mp3_stop() {
  return 0;
}


void I2S_PlayMP3(void) {
  SETREGS

#ifdef USE_MP3

  if (busy) {
    return;
  }

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  File_p *wf;
  wf = fopen(cp, 'r');
  if (!wf) {
    // file not found
    Response_P(GSTR(S_JSON_FNF), cp);
    return;
  }

  mp3_begin();

  if (mp3_isRunning()) {
    if (!mp3_loop()) {
      mp3_stop();
      break;
    }
  }

  fclose(wf);
  
/*
  if (!MP3Decoder_AllocateBuffers()) {
    Response_P(GSTR(S_JSON_MEMERR));
    return;
  }

  MP3Decoder_FreeBuffers();
*/

#endif
}

const char I2S_Commands[] PROGMEM =
    "I2S|"  // Prefix
    "pw|gain/play";
void (*const I2S_Command[])(void) PROGMEM = {&I2S_PlayWave,&SetGain,&I2S_PlayMP3};

void I2SAudio_Deinit() {
  SETREGS
#ifdef USE_MP3
  MP3Decoder_FreeBuffers();
#endif
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

static int32_t mod_func_execute(uint32_t sel) {
  SETREGS
  bool result = false;
  switch (sel) {
    case FUNC_INIT:
      result = I2SAudio_Init();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(I2S_Commands, I2S_Command);
      break;
    case FUNC_DEINIT:
      I2SAudio_Deinit();
      break;
  }
  return result;
}
PULL_OPTIONS
#endif  // USE_I2S_MOD
