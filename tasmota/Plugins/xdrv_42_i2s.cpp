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

//#define USE_SAY

#ifdef ESP32
//#define USE_MP3_PSRAM
#define USE_MP3
#define USE_WEBRADIO
// select a codec
#define USE_WM8960
#endif

//#define USE_MP3

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
#include "Audio/MP3/mp3_decoder.h"
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
  bool running;
#ifdef USE_MP3
  MP3_MEM mp3m;
  int16_t *m_outBuff;
  uint8_t *m_inBuff;
  int32_t m_bytesLeft;
  uint8_t chans;
  uint8_t force_mono;
  uint16_t input_bytes;
  uint32_t filepos;
#endif
  int32_t pclamp;
  int32_t mclamp;
#ifdef USE_WM8960
 TwoWire *xWire;
#endif
#ifdef USE_WEBRADIO
  void *http;
  void *wclient;
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
#define wclient mem->wclient
#define http mem->http
#define pclamp mem->pclamp
#define mclamp mem->mclamp
#define force_mono mem->force_mono

// esp8266 fixed i2s pins : DOUT = 3(RX), BCK = 15(D8), WS = 2(D4)

#ifdef USE_MP3
#define MODNAME "I2SAUDIO"
#else
#define MODNAME "I2SWAV"
#endif

#define I2S_REV 1 << 16 | 5
#ifdef ESP8266
MODULE_DESCRIPTOR(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0)
#else
MODULE_DESCRIPTOR(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "DOUT", 17, "BCK", 10, "WS", 18, "MODE", 0x01000200)
#endif

// all functions must be declared MUDULE_PART
MODULE_PART int32_t I2SAudio_Init();
MODULE_PART void I2sTask(void);
MODULE_PART void I2sTaskMP3(void);
MODULE_PART void I2sTaskWR(char *);
MODULE_PART void I2S_Play(void);
MODULE_PART bool mp3_begin();
MODULE_PART uint32_t Get_tag(uint8_t * buff);
MODULE_PART bool mp3_loop();
MODULE_PART bool mp3_stop();
MODULE_PART void SetVolume(void);
MODULE_PART void WebRadio(void);
MODULE_PART void Say(void);
MODULE_PART int32_t W8960_Init(void);
MODULE_PART void W8960_Write(uint8_t reg_addr, uint16_t data);
MODULE_PART void W8960_SetGain(uint8_t sel, uint16_t value);
MODULE_PART void Write_Samples(int16_t *buffer, uint32_t samples);
MODULE_PART void I2SAudio_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END


#ifdef USE_MP3
#include "Audio/MP3/mp3_decoder_c.h"
#endif

#define OUTBUFF_SIZE 1024 * 6
#define INBUFF_SIZE 1024

const char S_JSON_FNF[] PROGMEM = "{\"File %s not found\"}";
const char S_JSON_ILLF[] PROGMEM = "{\"Illegal File format\"}";
const char S_JSON_BUSY[] PROGMEM = "{\"audio is busy\"}";
const char S_JSON_STOPSND[] PROGMEM = "{\"audio stopped\"}";
#ifdef USE_MP3
const char S_JSON_MEMERR[] PROGMEM = "{\"out of memory\"}";
#endif
#ifdef USE_WM8960
const char S_JSON_WMERR[] PROGMEM = "{\"WM8960 error\"}";
#endif


const char tname[] PROGMEM = "I2STASK";
const uint32_t ui32_const[4] PROGMEM = {OUTBUFF_SIZE, 8192, 0x46464952 , INBUFF_SIZE}; 
const int32_t i32_const[2] PROGMEM = {32768, -32768}; 


int32_t I2SAudio_Init() {
  ALLOCMEM

  dout_pin = mp->ms[0].value;
  bck_pin = mp->ms[1].value;
  ws_pin = mp->ms[2].value;
  mode = mp->ms[3].value;

  gain_div = 1<<6;  // = 1

  i2sp = i2s_begin(dout_pin, bck_pin, ws_pin, mode);

  const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);
  pclamp = icp[0];
  mclamp = icp[1];

#ifdef USE_MP3

  uint32_t mp3mem = MP3Decoder_AllocateBuffers();

  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

  mt->mem_size += mp3mem;
  m_outBuff = (int16_t*)calloc(uicp[0]/2, 2);
  mt->mem_size += uicp[0];
  m_inBuff = (uint8_t*)calloc(uicp[3], 1);
  mt->mem_size += uicp[3];

  if (!mp3mem || !m_outBuff || !m_inBuff) {
    I2SAudio_Deinit();
    Response_P(GSTR(S_JSON_MEMERR));
    return -1;
  }

  force_mono = 1;
#endif

#ifdef USE_WM8960
  if (W8960_Init() < 0) {
      I2SAudio_Deinit();
      Response_P(GSTR(S_JSON_WMERR));
      return -2;
  }
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

  while (running) {
    uint32_t bytesread = fread((char*)buffer, 1, sizeof(buffer), wf);
    if (!bytesread) {
      running = 0;
      break;
    }

    Write_Samples(buffer, bytesread / 2);

  }
  
  fclose(wf);

  i2s_disable_tx(i2sp);
  
  busy = false;
  
  vTaskDelete(0);
}
#endif

void I2S_Play(void) {
  SETREGS

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  if (busy) {
    if (!*cp) {
      // stop running sound
      running = 0;
      Response_P(GSTR(S_JSON_STOPSND));
    } else {
      Response_P(GSTR(S_JSON_BUSY));
    }
    return;
  }

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

  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

  if (!strncmp_P(ep, PSTR("wav"), 3)) {
    // play wav file
    wav_header_t wh;

    // check for RIFF
    fread((char*)&wh, 1, sizeof(wav_header_t), wf);
 
    // 0x52494646
    if (wh.Riff.ChunkID != uicp[2] && wh.Fmt.NumChannels != 1) {
      fclose(wf);
      Response_P(GSTR(S_JSON_ILLF));
      return;
    }
  
    // default is 1 channel
    i2s_set_rate(i2sp, wh.Fmt.SampleRate, mode, 1);

    busy = true;

    running = true;

#ifdef USE_I2S_TASK
    TASKPARS tp;
    tp.pvTaskCode = GVOID(I2sTask);
    tp.constpcName = GSTR(tname);
    tp.usStackDepth = uicp[1];
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
      Write_Samples(buffer, bytesread / 2);
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
      tp.usStackDepth = uicp[1];
      tp.constpvParameters = cp;
      tp.uxPriority = 3;
      tp.constpvCreatedTask = nullptr;
      tp.xCoreID = 1;
      int32_t err = xTaskCreatePinnedToCore(&tp);
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
  uint16_t gain;
  // virtual bool SetGain(float f) { if (f>4.0) f = 4.0; if (f<0.0) f=0.0; gainF2P6 = (uint8_t)(f*(1<<6)); return true; }

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
    float xgain = fmul(fdiv(floatunsisf(gain) , floatunsisf(100)), floatunsisf(64));
    gain_div = fixunssfsi(xgain);
  } 
  gain = fixunssfsi(fmul(fdiv(floatunsisf(gain_div) , floatunsisf(64)), floatunsisf(100)));
  ResponseCmndNumber(gain);
}


void Write_Samples(int16_t *buffer, uint32_t samples) {
SETMEMREGS

  if (chans == 2 && force_mono) {
    for (uint32_t i = 0; i < samples; i += 2) {
      int16_t s_left;
      int32_t v = (buffer[i] * gain_div) >> 6;
      if (v < mclamp) {
        v = mclamp;
      } else if (v > pclamp) {
        v = pclamp;
      }
      s_left = (int16_t)(v & 0xffff);
      int16_t s_right;
      v = (buffer[i + 1] * gain_div) >> 6;
      if (v < mclamp) {
        v = mclamp;
      } else if (v > pclamp) {
        v = pclamp;
      }
      s_right = (int16_t)(v & 0xffff);
      v = ((int32_t)s_left + (int32_t)s_right) / 2;
      buffer[i] = v;
      buffer[i + 1] = v;
    }
  } else {
    // Stereo + Mono
    for (uint32_t i = 0; i < samples; i++) {
      int32_t v = (buffer[i] * gain_div) >> 6;
      if (v < mclamp) {
        v = mclamp;
      } else if (v > pclamp) {
        v = pclamp;
      }
      buffer[i] = (int16_t)(v & 0xffff);
    }
  }
  i2s_write_samples(i2sp, buffer, samples);
}  


#ifdef USE_MP3

uint32_t Get_tag(uint8_t * buff) {
  if (buff[0] == 'T' && buff[1] == 'A' && buff[2] == 'G') {
    return 128;
  } 
  if (buff[0] == 'I' && buff[1] == 'D' && buff[2] == '3') {
    uint32_t size = buff[6] << 21 | buff[7] << 14 | buff[8] << 7 | buff[9];
    return size + 10; 
  }
  return 0;
}

bool mp3_begin() {
  SETREGS

  input_bytes = fread((char*)m_inBuff, 1, 1024, wf);
  uint32_t tag = Get_tag(m_inBuff);
  uint8_t *cp = m_inBuff;
  cp += tag;
  input_bytes -= tag;
  int16_t m_decodeError = MP3GetNextFrameInfo(cp);
  if (m_decodeError) {
    AddLog(LOG_LEVEL_INFO, PSTR("mp3 header error = %d"), m_decodeError);
    return true;
  }

  uint32_t srate = MP3GetSampRate();
  chans = MP3GetChannels();

  i2s_set_rate(i2sp, srate, mode, chans);

  filepos = 0;

  AddLog(LOG_LEVEL_INFO, PSTR("mp3 srate = %d, channels = %d"), srate, chans); 

  i2s_enable_tx(i2sp);
  
  busy = true;
  running = true;
  return false;
}

bool mp3_loop() {
SETREGS

  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

  uint32_t bytesread;
  uint32_t tag = 1;
  while (tag) {
    fseek(wf, filepos, SEEK_SET);
    bytesread = fread((char*)m_inBuff, 1, uicp[3], wf);
    if (!bytesread) {
      running = false;
      return false;
    }
    tag = Get_tag(m_inBuff);
    filepos += tag;
  }
  m_bytesLeft = bytesread;

  int16_t m_decodeError = MP3Decode(m_inBuff, &m_bytesLeft, m_outBuff, 0);
  if (m_decodeError) {
    AddLog(LOG_LEVEL_INFO, PSTR("mp3 header error = %d"), m_decodeError);
    running = false;
    return false;
  }

  uint32_t bytesDecoded = bytesread - m_bytesLeft;

  filepos += bytesDecoded;

  uint32_t samples = MP3GetOutputSamps();

  if (samples > uicp[0] >> 1) {
    AddLog(LOG_LEVEL_INFO, PSTR("mp3 buffer overflow = %d"), samples);
    running = 0;
  } else {

    uint32_t m_validSamples = samples; // chans;

    Write_Samples(m_outBuff, m_validSamples);

  }
  return running;
}

bool mp3_stop() {
SETREGS
  i2s_disable_tx(i2sp);
  return 0;
}

void I2sTaskMP3(void) {
  SETREGS

  if (!mp3_begin()) {
    while (running) {
      if (!mp3_loop()) {
        mp3_stop();
        break;
      }
    }
  }

  i2s_disable_tx(i2sp);
  fclose(wf);
  busy = false;
  
  vTaskDelete(0);
}
#endif

#ifdef USE_WM8960
#include "Audio/WM8960/wm8960_c.h"
#endif

#ifdef USE_SAY
#define DEBUG_ESP8266SAM_LIB 0
#define PrintRule
#define PrintPhonemes
#define PrintOutput
#include "Audio/ESP8266SAM/sam_c.h"
#include "Audio/ESP8266SAM/render_c.h"
#include "Audio/ESP8266SAM/reciter_c.h"
#endif

void Say(void) {
  SETREGS
#ifdef USE_SAY
#endif
}

#ifdef USE_WEBRADIO

#ifdef ESP32
#include <HTTPClient.h>
#endif

const char head_1[] PROGMEM = "Icy-MetaData";
const char head_2[] PROGMEM = "1";

const char hdr_1[] PROGMEM = "icy-metaint";
const char hdr_2[] PROGMEM = "icy-name";
const char hdr_3[] PROGMEM = "icy-genre";
const char hdr_4[] PROGMEM = "icy-br";

const uint32_t hdr[] PROGMEM = { (uint32_t)hdr_1, (uint32_t)hdr_2, (uint32_t)hdr_3, (uint32_t)hdr_4 };

// webradio task
void I2sTaskWR(char *url) {
  SETREGS

  AddLog(LOG_LEVEL_INFO, PSTR("WR Task started"));

  int32_t res = icecast_open(url);

  AddLog(LOG_LEVEL_INFO, PSTR("icecast res: %d"),res);

/*
  if (res < 0) {
    icecast_end();
    AddLog(LOG_LEVEL_INFO, PSTR("WR could not connect to %s err: %d"),url, res);
    busy = false;
    free(url);
    vTaskDelete(0);
  }
  */

  http = icecast_http();

  wclient = http_getStreamPtr(http);

  AddLog(LOG_LEVEL_INFO, PSTR("icecast >>: %8x - %8x"),(uint32_t)http,(uint32_t)wclient);


  icecast_end();

  free(url);
  vTaskDelete(0);

/*
  icecast_end();
  busy = false;
  free(url);
  vTaskDelete(0);
*/

  //WDR2	i2swr http://wdr-wdr2-aachenundregion.icecastssl.wdr.de/wdr/wdr2/aachenundregion/mp3/128/stream.mp3
 //       i2swr http://icecast.ndr.de/ndr/njoy/live/mp3/128/stream.mp3

#if 0
  if (!wclient) {
    wclient = New_WiFiClient();
  }

  if (!http) {
    http = New_HTTP();
  }

  bool res = http_begin(http, wclient, url);
  
  int32_t code = 0;

exit:
    http_end(http);
    //http_delete(http);
    AddLog(LOG_LEVEL_INFO, PSTR("WR could not connect to %s err: %d"),url, code);
    busy = false;
    free(url);
    vTaskDelete(0);
  }

  http_addHeader(http, GSTR(head_1), GSTR(head_2));

  http_collectHeaders(http, GUI32p(hdr), 4 );

  http_setReuse(http, false);

  http_setFollowRedirects(http, HTTPC_FORCE_FOLLOW_REDIRECTS);

  code = http_GET(http);
  
  if (code < 0) {
    goto exit;
  }

  uint32_t has_bitrate;

  if (http_hasHeader(http, PSTR("icy-br"))) {
    char *ret = http_header(http, PSTR("icy-br"));
    AddLog(LOG_LEVEL_INFO, PSTR("WR br = %d"),ret);
    has_bitrate = 1; // ret.toInt();
  } else {
    has_bitrate = 0;
  }

  int32_t size = http_getSize(http);

  AddLog(LOG_LEVEL_INFO, PSTR("WR code %d - %d"),code, size);

#endif

#if 0
  http_end(http);
  //http_delete(http);
  busy = false;
#else
  running = true;
  i2s_enable_tx(i2sp);

  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
  uint32_t len = uicp[3];

  //wclient = http_getStreamPtr(http);

  AddLog(LOG_LEVEL_INFO, PSTR("WR Task started 2"));

  uint32_t count = 0;
  while (running) {
    uint32_t start = millis();
    while ((client_available(wclient) < len) && (millis() - start < 500)) {
      delay(1);
    }

    uint32_t avail = client_available(wclient);
    if (!avail) {
      break;
    }

    count++;
    if (count%10 == 0) {
      AddLog(LOG_LEVEL_INFO, PSTR(">> rec %d - %d"), avail, count);
    }
    client_readn(wclient, m_outBuff, len);

    if (!http_connected(http)) {
      break;
    }

  }

  AddLog(LOG_LEVEL_INFO, PSTR("WR Task stop"));

  //http_end(http);
  //http_delete(http);
  icecast_end();

  i2s_disable_tx(i2sp);

  running = false;
  busy = false;
  
#endif

  AddLog(LOG_LEVEL_INFO, PSTR("WR Task stopped"));
  free(url);
  vTaskDelete(0);
}
#endif


void WebRadio(void) {
  SETREGS

#ifdef USE_WEBRADIO
  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  if (busy == true) {
    if (!*cp) {
      // stop running sound
      running = 0;
      Response_P(GSTR(S_JSON_STOPSND));
    } else {
      Response_P(GSTR(S_JSON_BUSY));
    }
    return;
  }

  busy = true;

#define URL_SIZE 256
// play webradio file
  char *urlcopy = (char*)special_malloc(URL_SIZE);
  strncpy (urlcopy, cp, URL_SIZE);
  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
  TASKPARS tp;
  tp.pvTaskCode = GVOID(I2sTaskWR);
  tp.constpcName = GSTR(tname);
  tp.usStackDepth = uicp[1];
  tp.constpvParameters = urlcopy;
  tp.uxPriority = 3;
  tp.constpvCreatedTask = nullptr;
  tp.xCoreID = 1;
  xTaskCreatePinnedToCore(&tp);
#endif

  ResponseCmndDone();
}

const char I2S_Commands[] PROGMEM =
    "I2S|"  // Prefix
    "play|vol|say|wr";
void (*const I2S_Command[])(void) PROGMEM = {&I2S_Play,&SetVolume,&Say,&WebRadio};

void I2SAudio_Deinit() {
  SETREGS
#ifdef USE_MP3
  MP3Decoder_FreeBuffers();
  if (m_outBuff) free(m_outBuff);
  if (m_inBuff) free(m_inBuff);  
#endif
  i2s_end(i2sp);
#ifdef USE_WM8960
  I2cResetActive(W8960_ADDR, 0);
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
