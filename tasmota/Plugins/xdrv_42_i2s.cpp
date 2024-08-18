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


#ifdef ESP32
#define USE_MP3
#endif

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
  File_p *wf;
#ifdef USE_MP3
  MP3_MEM mp3m;
  int16_t *m_outBuff;
  uint8_t *m_inBuff;
  int m_bytesLeft;
  bool running;
  uint8_t chans;
  uint16_t input_bytes;
  uint32_t filepos;
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
#define wf mem->wf
#define m_outBuff mem->m_outBuff
#define m_inBuff mem->m_inBuff
#define m_bytesLeft mem->m_bytesLeft
#define running mem->running
#define chans mem->chans
#define input_bytes mem->input_bytes
#define filepos mem->filepos

// esp8266 fixed i2s pins : DOUT = 3(RX), BCK = 15(D8), WS = 2(D4)

#ifdef USE_MP3
#define MODNAME "I2SAUDIOM"
#else
#define MODNAME "I2SAUDIO"
#endif

#define I2S_REV 1 << 16 | 4
#ifdef ESP8266
MODULE_DESCRIPTOR(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0)
#else
MODULE_DESCRIPTOR(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "DOUT", 17, "BCK", 10, "WS", 18, "MODE", 0x01000200)
#endif

// all functions must be declared MUDULE_PART
MODULE_PART int32_t I2SAudio_Init();
MODULE_PART void I2sTask(void);
MODULE_PART void I2sTaskMP3(void);
MODULE_PART void I2S_Play(void);
MODULE_PART bool mp3_begin();
MODULE_PART bool mp3_isRunning();
MODULE_PART bool mp3_loop();
MODULE_PART bool mp3_stop();
MODULE_PART void SetVolume(void);
MODULE_PART void execute(void);

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
const char S_JSON_BUSY[] PROGMEM = "{\"audio is busy\"}";
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

  gain_div = 2;

  i2sp = i2s_begin(dout_pin, bck_pin, ws_pin, mode);

#ifdef USE_MP3

#define OUTBUFF_SIZE 1024 * 4
#define INBUFF_SIZE 1024

  uint32_t mp3mem = MP3Decoder_AllocateBuffers();
  if (!mp3mem) {
    Response_P(GSTR(S_JSON_MEMERR));
    return false;
  }
  mt->mem_size += mp3mem;
  m_outBuff = (int16_t*)special_malloc(OUTBUFF_SIZE);
  mt->mem_size += OUTBUFF_SIZE;
  m_inBuff = (uint8_t*)special_malloc(INBUFF_SIZE);
  mt->mem_size += INBUFF_SIZE;
#endif

  busy = false;

  initialized = true;
  return 0;
}


#ifdef USE_I2S_TASK
void I2sTask(void) {
  SETREGS

  int16_t buffer[512];
  // skip header
  fread((char*)&buffer, 1, sizeof(wav_header_t), wf);

  while (1) {
    uint32_t bytesread = fread((char*)buffer, 1, sizeof(buffer), wf);
    if (!bytesread) {
      break;
    }
    for (uint32_t i = 0; i < bytesread / 2; i++) {
      buffer[i] = __divsi3(buffer[i], gain_div);
    }
    i2s_write_samples(i2sp, buffer, bytesread / 2);
  }
  
  fclose(wf);

  i2s_disable_tx(i2sp);
  
  busy = false;
  
  vTaskDelete(0);
}
#endif

void I2S_Play(void) {
  SETREGS

  if (busy) {
    Response_P(GSTR(S_JSON_BUSY));
    return;
  }

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  wf = fopen(cp, 'r');

  if (!wf) {
    // file not found
    Response_P(GSTR(S_JSON_FNF), cp);
    return;
  }

  // check file extension
  char *ep = strchr(cp, '.');
  if (!ep) {
    Response_P(GSTR(S_JSON_ILLF), cp);
    return;
  }

  ep++;

  if (!strncmp_P(ep, PSTR("wav"), 3)) {
    // play wav file
    wav_header_t wh;

    // check for RIFF
    fread((char*)&wh, 1, sizeof(wav_header_t), wf);
 
    // 0x52494646
    if (wh.Riff.ChunkID != 0x46464952 && wh.Fmt.NumChannels != 1) {
      fclose(wf);
      Response_P(GSTR(S_JSON_ILLF));
      return;
    }
  
    // default is 1 channel
    i2s_set_rate(i2sp, wh.Fmt.SampleRate, mode, 1);

    busy = true;

#ifdef USE_I2S_TASK
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
        buffer[i] = __divsi3(buffer[i], gain_div);
      }
      i2s_write_samples(i2sp, buffer, bytesread / 2);
      OsWatchLoop();
    }

    fclose(wf);

    i2s_disable_tx(i2sp);

    busy = false;
#endif
  } else {
#ifdef USE_MP3
    if (!strncmp_P(ep, PSTR("mp3"), 3)) {
      // play mp3 file
      TASKPARS tp;
      tp.pvTaskCode = GVOID(I2sTaskMP3);
      tp.constpcName = GSTR(tname);
      tp.usStackDepth = ICONST(8192);
      tp.constpvParameters = cp;
      tp.uxPriority = 3;
      tp.constpvCreatedTask = nullptr;
      tp.xCoreID = 1;
      int32_t err = xTaskCreatePinnedToCore(&tp);
      busy = true;
    } else {
      Response_P(GSTR(S_JSON_ILLF), cp);
      return;
    }
#else
    Response_P(GSTR(S_JSON_ILLF), cp);
    return;
#endif
  }

  ResponseCmndDone();
  return;
}

void SetVolume(void) {
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
    gain_div = __divsi3(100, gain);
  } else {
    gain = __divsi3(100 , gain_div);
  }
  ResponseCmndNumber(gain);

}

#ifdef USE_MP3
bool mp3_begin() {
  SETREGS

  input_bytes = fread((char*)m_inBuff, 1, INBUFF_SIZE, wf);
  m_bytesLeft = input_bytes;

  int16_t m_decodeError = MP3GetNextFrameInfo(m_inBuff);

  uint32_t srate = MP3GetSampRate();
  chans = MP3GetChannels();

  i2s_set_rate(i2sp, srate, mode, chans);

  filepos = 0;

  AddLog(LOG_LEVEL_INFO, PSTR("mp3 srate = %d, channels = %d"), srate, chans); 

  i2s_enable_tx(i2sp);
  running = true;
  return 0;
}
bool mp3_isRunning() {
  SETREGS
  return running;
}

#define MIN_SIZE 1024

bool mp3_loop() {
SETREGS

  fseek(wf, filepos, SEEK_SET);

  uint32_t bytesread = fread((char*)m_inBuff, 1, INBUFF_SIZE, wf);
  if (!bytesread) {
    running = false;
    return false;
  }

  m_bytesLeft = bytesread;

  int16_t m_decodeError = MP3Decode(m_inBuff, &m_bytesLeft, m_outBuff, 0);
  
  uint32_t bytesDecoded = bytesread - m_bytesLeft;

  filepos += bytesDecoded;

  uint32_t samples = MP3GetOutputSamps();

  uint32_t m_validSamples = samples; // chans;

  for (uint32_t i = 0; i < m_validSamples; i++) {
    m_outBuff[i] = __divsi3(m_outBuff[i], gain_div);
  }

  i2s_write_samples(i2sp, m_outBuff, m_validSamples);
  
  OsWatchLoop();

  return running;
}

bool mp3_stop() {
SETREGS
  i2s_disable_tx(i2sp);
  return 0;
}

void I2sTaskMP3(void) {
  SETREGS

  mp3_begin();

  while (mp3_isRunning()) {
    if (!mp3_loop()) {
      mp3_stop();
      break;
    }
  }

  fclose(wf);

  i2s_disable_tx(i2sp);
  
  busy = false;
  
  vTaskDelete(0);
}
#endif

void execute(void) {
}

const char I2S_Commands[] PROGMEM =
    "I2S|"  // Prefix
    "play|vol|ex";
void (*const I2S_Command[])(void) PROGMEM = {&I2S_Play,&SetVolume,&execute};

void I2SAudio_Deinit() {
  SETREGS
#ifdef USE_MP3
  MP3Decoder_FreeBuffers();
  if (m_outBuff) free(m_outBuff);
  if (m_inBuff) free(m_inBuff);  
#endif
  i2s_end(i2sp);
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
