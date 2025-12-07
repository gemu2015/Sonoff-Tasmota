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


/* to doo
microphone support
codec settings access
*/

#include "tasmota_options.h"

#if defined(USE_I2S_MOD) && defined(EXECUTE_FROM_BINARY)

#define XDRV_42 42

#ifdef ESP32
#define MAX_MOD_STORES 8
#else
#define MAX_MOD_STORES 4
#endif


#include "module.h"
#include "module_defines.h"

#define USE_SAY

#ifdef ESP32
#define USE_MP3_PSRAM
#define USE_MP3
#define USE_WEBRADIO
#define USE_MIC

// select a codec
#define USE_AUDIO_CODECS

#ifdef USE_AUDIO_CODECS
#include "Audio/es8156/src/audio_hal.h"
#include "Audio/es8156/src/es8156.h"
int32_t pW8960_Init();
#endif

#endif

#ifdef USE_SAY
#include "Audio/ESP8266SAM/SamData.h"
#endif

#include "Audio/wav_header.h"

#ifdef ESP32
#define USE_I2S_TASK
#endif

#ifdef USE_MP3
#include "Audio/MP3/mp3_decoder.h"
#endif

PUSH_OPTIONS

typedef struct {
#ifdef USE_SCRIPT
  char *cmd_param;
#endif
  uint8_t dout_pin;
  uint8_t bck_pin;
  uint8_t ws_pin;
  int8_t mc_pin;
  int8_t din_pin;
  uint8_t gain_div;
#ifdef USE_MIC
  uint8_t adc_gain_div;
  uint8_t bridge_mode;
#endif
  void *i2sp;
  uint8_t busy;
  uint8_t mode;
  File_p *wf;
  bool running;
  uint8_t force_mono;
  uint8_t chans;
  uint8_t codec;
  uint16_t srate;
  uint8_t tx_ready;
  int8_t audio_pwr_pin;
  uint8_t codec_bus;
#ifdef USE_MP3
  MP3_MEM mp3m;
  int16_t *m_outBuff;
  uint8_t *m_inBuff;
  int32_t m_bytesLeft;
  uint16_t input_bytes;
  uint32_t filepos;
#endif
  int32_t pclamp;
  int32_t mclamp;
#ifdef USE_AUDIO_CODECS
 TwoWire *xWire;
#endif
#ifdef USE_WEBRADIO
  void *http;
  void *wclient;
  uint16_t icyMetaInt;
  char meta[128];
#endif

#ifdef USE_SAY
  SamData *samdata;
  SAM_RENDER *samrender;
#endif

} MODULE_MEMORY;

#define audio_pwr_pin mem->audio_pwr_pin
#define dout_pin mem->dout_pin
#define bck_pin mem->bck_pin
#define ws_pin mem->ws_pin
#define mc_pin mem->mc_pin
#define din_pin mem->din_pin
#define i2sp mem->i2sp
#define gain_div mem->gain_div
#define adc_gain_div mem->adc_gain_div
#define bridge_mode mem->bridge_mode
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
#define srate mem->srate
#define samrender mem->samrender
#define samdata mem->samdata
#define codec mem->codec
#define icyMetaInt mem->icyMetaInt
#define meta mem->meta
#define tx_ready mem->tx_ready
#define cmd_param mem->cmd_param
#define codec_bus mem->codec_bus


// esp8266 fixed i2s pins : DOUT = 3(RX), BCK = 15(D8), WS = 2(D4)

#ifdef USE_MP3
#define MODNAME "I2SAUDIO"
#else
#define MODNAME "I2SWAV"
#endif

#ifdef __riscv
#define GPIO_DOUT 6
#define GPIO_BCK 7
#define GPIO_WS 8
#else

#if 1
#define GPIO_DOUT 17
#define GPIO_BCK 10
#define GPIO_WS 18
#else
#define GPIO_DOUT 15
#define GPIO_BCK 17
#define GPIO_WS 47
#endif
#define GPIO_APWR 46
#define GPIO_DIN 16
#define GPIO_MC 2
#endif

#define I2S_REV 1 << 16 | 5
#ifdef ESP8266
MODULE_DESCRIPTOR(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0)
//MODULE_DESCRIPTOR6(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0, "", 0, "", 0)
#else
MODULE_DESCRIPTOR8(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "DOUT", GPIO_DOUT, "DIN", GPIO_DIN, "BCK", GPIO_BCK, "WS", GPIO_WS, "MC", GPIO_MC,"MODE", 0x01000200,"CODEC", 0x01000201,"APWR", GPIO_APWR)
#endif

// all functions must be declared MUDULE_PART
MODULE_PART int32_t I2SAudio_Init();
MODULE_PART void I2sTask(void);
MODULE_PART void I2sTaskMP3(void);
MODULE_PART void I2sTaskWR(char *);
MODULE_PART void I2S_Play(char *);
MODULE_PART void I2S_Play_Cmd(void);
MODULE_PART bool mp3_begin();
MODULE_PART uint32_t Get_tag(uint8_t * buff);
MODULE_PART bool mp3_loop();
MODULE_PART bool mp3_stop();
MODULE_PART void SetVolume(void);
MODULE_PART void SetGain(void);
MODULE_PART void StartMicRec(void);
MODULE_PART void StopMicRec(void);
MODULE_PART void I2SBridge(void);
MODULE_PART void WebRadio(void);
MODULE_PART void Say(void);
MODULE_PART void AudioPwr(uint32_t pwr);
MODULE_PART int32_t W8960_Init(void);
MODULE_PART void W8960_Write(uint8_t reg_addr, uint16_t data);
MODULE_PART void W8960_SetGain(uint8_t sel, uint16_t value);
MODULE_PART void Write_Samples(int16_t *buffer, uint32_t samples);
MODULE_PART void I2sWrShow(bool json);
MODULE_PART int32_t i2s_script_cmd(uint32_t sel);
MODULE_PART void I2SAudio_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END


#ifdef USE_MP3
#include "Audio/MP3/mp3_decoder_c.h"
#endif

#define OUTBUFF_SIZE 1024 * 6
#define INBUFF_SIZE 1024 * 2

const char S_JSON_FNF[] PROGMEM = "{\"File %s not found\"}";
const char S_JSON_ILLF[] PROGMEM = "{\"Illegal File format\"}";
const char S_JSON_BUSY[] PROGMEM = "{\"audio is busy\"}";
const char S_JSON_STOPSND[] PROGMEM = "{\"audio stopped\"}";
#if defined(USE_MP3) || defined(USE_SAY)
const char S_JSON_MEMERR[] PROGMEM = "{\"out of memory\"}";
#endif
#ifdef USE_AUDIO_CODECS
const char S_JSON_WMERR[] PROGMEM = "{\"Codec error\"}";
#endif

#define TASK_STACK 8192
//#define TASK_STACK 12000

#ifdef USE_SAY
#define RENDER_SIZE sizeof(SAM_RENDER)
#else
#define RENDER_SIZE 0
#endif


const char tname[] PROGMEM = "I2STASK";
const uint32_t ui32_const[5] PROGMEM = {OUTBUFF_SIZE, TASK_STACK, 0x46464952 , INBUFF_SIZE,0x7fffffff}; 
const int32_t i32_const[7] PROGMEM = {32768, -32768, 22050, RENDER_SIZE, 37541, 32000, 44100}; 

#define GET_OBS ucp[0]
#define GET_IBS ucp[3]
#define GET_TSTACK ucp[1]


#ifdef ESP32
#include "driver/i2s_types.h"
//#include "driver/i2s_common.h"

typedef struct {
    i2s_isr_callback_t on_recv;             /**< Callback of data received event, only for RX channel
                                             *   The event data includes DMA buffer address and size that just finished receiving data
                                             */
    i2s_isr_callback_t on_recv_q_ovf;       /**< Callback of receiving queue overflowed event, only for RX channel
                                             *   The event data includes buffer size that has been overwritten
                                             */
    i2s_isr_callback_t on_sent;             /**< Callback of data sent event, only for TX channel
                                             *   The event data includes DMA buffer address and size that just finished sending data
                                             */
    i2s_isr_callback_t on_send_q_ovf;       /**< Callback of sending queue overflowed event, only for TX channel
                                             *   The event data includes buffer size that has been overwritten
                                             */
} i2s_event_callbacks_t;


MODULE_PART bool i2s_tx_ready_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
SETMEMREGS
    // handle TX ready
    tx_ready = true;
    return true;
}

MODULE_PART void I2S_Wait_Ready(void) {
SETMEMREGS
  return;
  for (uint32_t cnt = 0; cnt < 500; cnt++) {
    if (tx_ready == true) {
      return;
    }
    delay(1);
  }
}
#else
MODULE_PART void I2S_Wait_Ready(void) {
}
#endif

int32_t I2SAudio_Init() {
  ALLOCMEM

  dout_pin = mp->ms[0].value;
  din_pin = mp->ms[1].value;
  bck_pin = mp->ms[2].value;
  ws_pin = mp->ms[3].value;
  mc_pin = mp->ms[4].value;
  mode = mp->ms[5].value;
  codec = mp->ms[6].value;
  audio_pwr_pin = mp->ms[7].value;

  if (audio_pwr_pin < GetPins()) {
    pinMode(audio_pwr_pin, OUTPUT);
    digitalWrite(audio_pwr_pin, LOW);
  } else {
    audio_pwr_pin = -1;
  }

  if (din_pin == GetPins()) {
    din_pin = -1;
  }

  if (mc_pin == GetPins()) {
    mc_pin = -1;
  }

  gain_div = 1<<6;  // = 1

  i2sp = i2s_begin(dout_pin, bck_pin, ws_pin, mode, mc_pin, din_pin);

#ifdef ESP32
  i2s_event_callbacks_t cbs;
  cbs.on_recv = NULL;
  cbs.on_recv_q_ovf = NULL;
  cbs.on_sent = i2s_tx_ready_callback;
  cbs.on_send_q_ovf = NULL;
  i2s_channel_register_event_callback(i2sp, &cbs, nullptr);
#endif

  // voltile is needed due to by eps8266 asm error
  volatile const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);
  pclamp = icp[0];
  mclamp = icp[1];

#ifdef USE_MP3

  uint32_t mp3mem = MP3Decoder_AllocateBuffers();

  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

  mt->mem_size += mp3mem;

#ifdef USE_MP3_PSRAM
  m_inBuff = (uint8_t*)special_malloc(uicp[3]);
  mt->mem_size += uicp[3];
#else
  m_inBuff = (uint8_t*)calloc(uicp[3], 1);
  mt->mem_size += uicp[3];
#endif

  m_outBuff = (int16_t*)calloc(uicp[0]/2, 2);
  mt->mem_size += uicp[0];
  
  if (!mp3mem || !m_outBuff || !m_inBuff) {
    I2SAudio_Deinit();
    AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_MEMERR));
    return -1;
  }
  chans = 1;
  force_mono = 1;
#endif

#ifdef USE_AUDIO_CODECS
// box lite
// ES8156_init(); DAC
// es7243e_init(); ADC
// box full
// ES8311_init(); DAC
// es7210_init(); ADC
  codec_bus = 0;
  switch (codec) {
    case 1:
      if (pW8960_Init() < 0) {
        I2SAudio_Deinit();
        AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_WMERR));
        return -2;
      }
      break;
    case 2: 
      if (pes8156_codec_init(AUDIO_HAL_MODE_SLAVE, AUDIO_HAL_BIT_LENGTH_16BITS, &codec_bus) < 0) {
        I2SAudio_Deinit();
        AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_WMERR));
        return -2;
      }
      pes8156_codec_set_voice_volume(75);
      break;
  }
#endif

  busy = false;
  initialized = true;
  return 0;
}

MODULE_PART void AudioPwr(uint32_t pwr) {
  SETMEMREGS
  if (audio_pwr_pin >= 0) {
    digitalWrite(audio_pwr_pin, pwr);
  }
  AddLog(LOG_LEVEL_INFO, PSTR("audio pwr: %d"), pwr);
}

#ifdef USE_I2S_TASK
void I2sTask(void) {
  SETREGS

  AudioPwr(1);
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

  I2S_Wait_Ready();
  i2s_disable_tx(i2sp);
  
  AudioPwr(0);
  busy = false;
  
  vTaskDelete(0);
}
#endif

void I2S_Play(char *cp) {
SETREGS 

  if (busy) {
    if (!*cp) {
      // stop running sound
      running = 0;
      AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_STOPSND));
    } else {
      AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_BUSY));
    }
    return;
  }
  
  wf = fopen(cp, 'r');

  if (!wf) {
    // file not found
    AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_FNF), cp);
    return;
  }

  // check file extension
  char *ep = strchr(cp, '.');
  if (!ep) {
    AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_ILLF), cp);
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
      AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_ILLF));
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

    uint16_t   count=0;
    while (1) {
      uint32_t bytesread = fread((char*)buffer, 1, sizeof(buffer), wf);
      if (!bytesread) {
        break;
      }
      Write_Samples(buffer, bytesread / 2);
      OsWatchLoop();

    }

    fclose(wf);

    I2S_Wait_Ready();
    i2s_disable_tx(i2sp);

    AudioPwr(0);
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
      AddLog(LOG_LEVEL_INFO,GSTR(S_JSON_ILLF), cp);
      return;
    }
#else
    AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_ILLF), cp);
    return;
#endif
  }
  return;
}

#ifdef USE_MIC
void SetGain(void) {
  SETREGS
  uint16_t gain;
 
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
    adc_gain_div = fixunssfsi(xgain);
  } 
  gain = fixunssfsi(fmul(fdiv(floatunsisf(adc_gain_div) , floatunsisf(64)), floatunsisf(100)));
  ResponseCmndNumber(gain);
}

void StartMicRec(void) {

}
void StopMicRec(void) {

}

void I2SBridge(void) {
  bridge_mode = 0;
}

#endif

void SetVolume(void) {
  SETREGS
  uint16_t gain;
 
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

  if (!samples) return;

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
  tx_ready = false;
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

  srate = MP3GetSampRate();
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
  I2S_Wait_Ready();
  i2s_disable_tx(i2sp);
  return 0;
}

void I2sTaskMP3(void) {
  SETREGS

  AudioPwr(1);

  if (!mp3_begin()) {
    while (running) {
      if (!mp3_loop()) {
        mp3_stop();
        break;
      }
    }
  }

  I2S_Wait_Ready();
  i2s_disable_tx(i2sp);
  fclose(wf);
  busy = false;
  AudioPwr(0);

  vTaskDelete(0);
}
#endif

#ifdef USE_AUDIO_CODECS
#include "Audio/WM8960/p_wm8960_c.h"
#include "Audio/es8156/src/p_es8156_c.h"
#endif

#ifdef USE_SAY
#define DEBUG_ESP8266SAM_LIB 0
#include "Audio/ESP8266SAM/esp8266sam_debug_c.h"
#include "Audio/ESP8266SAM/sam_c.h"
#include "Audio/ESP8266SAM/render_c.h"
#include "Audio/ESP8266SAM/reciter_c.h"

MODULE_PART void OutputByteCallback(void *cbdata, unsigned char b) {
  SETREGS
  int16_t s16 = b;// s16 -= 128; //s16 *= 128;
  int16_t sample[2];
  sample[0] = s16 << 7;
  sample[1] = s16 << 7;
  Write_Samples(sample, 1);
  delay(0);
}
#endif

// cuurently say is blocking also on esp32
void Say(void) {
  SETREGS
#ifdef USE_SAY

  const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);

  chans = 1;
  force_mono = 1;
  srate = icp[2];
  i2s_set_rate(i2sp, srate, mode, chans);

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;
  if (busy == true) {
    if (!*cp) {
      // stop running sound
      running = 0;
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_STOPSND));
    } else {
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_BUSY));
    }
    return;
  }

  samdata = (SamData *)special_malloc(sizeof(SamData));

  if (!samdata) {
    // memory error
    AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_MEMERR));
    return;
  }

  samrender = (SAM_RENDER *)special_malloc(icp[3]); 
  if (!samrender) {
    // memory error
    free(samdata);
    AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_MEMERR));
    return;
  }

  memset(samdata, 0, sizeof(SamData));
  memset(samrender, 0, icp[3]);

  for (uint16_t cnt = 0; cnt < 80; cnt++) {
    const uint8_t *cp = &org_freq1data[cnt];
    cp += EXEC_OFFSET;
    freq1data[cnt] = pgm_read_byte(cp);
    cp = &org_freq2data[cnt];
    cp += EXEC_OFFSET;
    freq2data[cnt] = pgm_read_byte(cp);
    cp = &org_freq3data[cnt];
    cp += EXEC_OFFSET;
    freq3data[cnt] = pgm_read_byte(cp);
  }
  for (uint16_t cnt = 0; cnt < 81; cnt++) {
    const uint8_t *cp = &org_flags1[cnt];
    cp += EXEC_OFFSET;
    xflags1[cnt] = pgm_read_byte(cp);
  }

  for (uint16_t cnt = 0; cnt < 78; cnt++) {
    const uint8_t *cp = &org_flags2[cnt];
    cp += EXEC_OFFSET;
    xflags2[cnt] = pgm_read_byte(cp);
  }

  char inbuff[256];
  memset(inbuff, 0, sizeof(inbuff));

  for (uint32_t i = 0; i < sizeof(inbuff) - 1; i++) {
    if (!*cp) {
      inbuff[i] = 0;
      break;
    }
    inbuff[i] = toupper(*cp++);
  }

  singmode = false;
  bool phonetic = false;

  speed = 72;
  pitch = 64;
  mouth = 128;
  throat = 128;

// To phonemes
  if (phonetic) {
    strncat_P(inbuff, PSTR("\x9b"), sizeof(inbuff));
  } else {
    strncat_P(inbuff, PSTR("["), sizeof(inbuff));
    if ( !TextToPhonemes(&inbuff[0]) ) {
      free(samdata);
      free(samrender);
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_MEMERR));
      return; // ERROR
    }
  }

  AudioPwr(1);
  running = true;
  i2s_enable_tx(i2sp);

  SetInput(inbuff);

  uint8_t *vp = (uint8_t*)OutputByteCallback;
  vp += EXEC_OFFSET;

  SAMMain((void (*)(void*, unsigned char))vp, samdata);
  
  free(samdata);
  free(samrender);

  I2S_Wait_Ready();
  i2s_disable_tx(i2sp);

  running = false;
  busy = false;
  AudioPwr(0);
  ResponseCmndDone();

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
const char hdr_4[] PROGMEM = "icy-sr";

const uint32_t hdr[] PROGMEM = { (uint32_t)hdr_1, (uint32_t)hdr_2, (uint32_t)hdr_3, (uint32_t)hdr_4 };

// webradio task
void I2sTaskWR(char *url) {
  SETREGS

  AddLog(LOG_LEVEL_INFO, PSTR("WR Task started"));


  // WiFi.setDNS(dns1, dns2);

  running = true;
  AudioPwr(1);


  i2s_enable_tx(i2sp);

  volatile const uint32_t *ucp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
  uint32_t ibsize = GET_IBS;

  uint32_t buffer_bytes = 0;

  int32_t bytesread;
  uint32_t icycount = 0;

  uint32_t avail;
  while (running) {
    uint32_t start = millis();
    while (1) {
      avail = client_available(wclient);
      if (avail >= ibsize) {
        break;
      }
      if (millis() - start > 1000) {
        // timeout
        avail = 0;
        break;
      }
      delay(1);
    }
    if (!avail) {
      AddLog(LOG_LEVEL_INFO, PSTR("timeout"));
      break;
    }

    uint32_t bytestoread = ibsize - buffer_bytes;
 
    uint8_t *ip = &m_inBuff[buffer_bytes];
    if (icyMetaInt) {
      if (icyMetaInt - icycount < ibsize) {
        // falls into buffer
        for (uint32_t  cnt = 0; cnt < bytestoread; cnt++) {
          *ip++ = client_read(wclient);
          icycount +=  1;
          if (icycount == icyMetaInt) {
            uint16_t icylen = (client_read(wclient) << 4);
            if (icylen) {
              //AddLog(LOG_LEVEL_INFO, PSTR("icylen = %d"), icylen);
              char buff[128];
              if (icylen <= sizeof(buff)) {
                client_readn(wclient, buff, icylen);
                if (!strncmp_P(buff, PSTR("StreamTitle="), 12)) {
                  for (uint32_t  cnt = 0; cnt < sizeof(meta); cnt++) {
                    char iob = buff[12 + cnt];
                    if (!iob || iob == ';') {
                      meta[cnt] = 0;
                      break;
                    }
                    meta[cnt] = iob;
                  }
                }
              } else {
                // throw away
                for (uint32_t cnt = 0; cnt < icylen; cnt++) {
                  client_read(wclient);
                }
              }
            }
            icycount = 0;
          }
        }
      } else {
        // read block
        bytesread = client_readn(wclient, m_inBuff + buffer_bytes, bytestoread);
        icycount += bytesread;
      }
    } else {
      for (uint32_t  cnt = 0; cnt < bytestoread; cnt++) {
        *ip++ = client_read(wclient);
      }
    }
    
    buffer_bytes = ibsize;
    m_bytesLeft = ibsize;

    int16_t m_decodeError = MP3Decode(m_inBuff, &m_bytesLeft, m_outBuff, 0);
    if (m_decodeError) {
      AddLog(LOG_LEVEL_INFO, PSTR("mp3 header error = %d"), m_decodeError);
      break;
    }

    uint32_t sr = MP3GetSampRate();
    if (sr && sr != srate) {
      srate = sr;
      chans = MP3GetChannels();
      i2s_set_rate(i2sp, srate, mode, chans);
    }

    uint32_t bytesDecoded = buffer_bytes - m_bytesLeft;
    buffer_bytes -= bytesDecoded;
    memmove(m_inBuff, m_inBuff + bytesDecoded, ibsize - bytesDecoded);
    
    //AddLog(LOG_LEVEL_INFO, PSTR(">> %d"), bytesDecoded );

    uint32_t samples = MP3GetOutputSamps();

    if (samples > GET_OBS >> 1) {
      AddLog(LOG_LEVEL_INFO, PSTR("mp3 output buffer overflow = %d"), samples);
      running = 0;
    } else {
      Write_Samples(m_outBuff, samples);
    }

    if (!http_connected(http)) {
      // should reconnect
      break;
    }

  }

  http_end(http);

  http_delete(http);
  http = 0;
  client_delete(wclient);
  wclient = 0;


  I2S_Wait_Ready();
  i2s_disable_tx(i2sp);

  running = false;
  busy = false;
  AudioPwr(0);
  
  AddLog(LOG_LEVEL_INFO, PSTR("WR Task stopped"));
  vTaskDelete(0);
}
#endif

// i2swr http://dispatcher.rndfnk.com/hr/hr3/live/mp3/48/stream.mp3
// i2swr http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3
// i2swr http://dispatcher.rndfnk.com/br/brklassik/live/mp3/low

void WebRadio(void) {
  SETREGS

#ifdef USE_WEBRADIO
  char *url = XdrvMailbox->data;
  while (*url == ' ') url++;

  if (busy == true) {
    if (!*url) {
      // stop running sound
      running = 0;
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_STOPSND));
    } else {
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_BUSY));
    }
    return;
  }

  if (!wclient) {
    wclient = New_WiFiClient();
  }

  if (!http) {
    http = New_HTTP();
  }

  bool res = http_begin(http, wclient, url);
  
  if (!res) {
    http_end(http);
    AddLog(LOG_LEVEL_INFO, PSTR("WR could not connect to %s"),url);
    return;
  }

  // without this delay it fails mostly ??????
  delay(100);

  http_addHeader(http, GSTR(head_1), GSTR(head_2));

  http_collectHeaders(http, GUI32p(hdr), 4 );

  http_setReuse(http, true);

  http_setFollowRedirects(http, HTTPC_FORCE_FOLLOW_REDIRECTS);

  int32_t code = http_GET(http);
  
  AddLog(LOG_LEVEL_INFO, PSTR("WR result: %d"), code);
  
  if (200 == code) {
    volatile const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);
    srate = icp[6];

    if (http_hasHeader(http, GSTR(hdr_4))) {
      char *ret = http_header(http, GSTR(hdr_4));
      AddLog(LOG_LEVEL_INFO, PSTR("WR sr = %s"), ret);
      srate = strtol(ret, 0, 10);
      free(ret);
    } else {
      srate = icp[6];
    }
    
    if (http_hasHeader(http, GSTR(hdr_1))) {
      char *ret = http_header(http, GSTR(hdr_1));
      AddLog(LOG_LEVEL_INFO, PSTR("ici metaint = %s"), ret);
      icyMetaInt = strtol(ret, 0, 10);
      free(ret);
    } else {
      icyMetaInt = 0;
    }

    //int32_t size = http_getSize(http);

    busy = true;

#define URL_SIZE 256
    // play webradio stream
    //char *urlcopy = (char*)calloc(URL_SIZE, 1);
    const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
    TASKPARS tp;
    tp.pvTaskCode = GVOID(I2sTaskWR);
    tp.constpcName = GSTR(tname);
    tp.usStackDepth = uicp[1];
    tp.constpvParameters = url;
    tp.uxPriority = 3;
    tp.constpvCreatedTask = nullptr;
    tp.xCoreID = 1;
    xTaskCreatePinnedToCore(&tp);
    ResponseCmndDone();
  } else {
    ResponseCmndNumber(code);
  }
#endif
}

#ifdef USE_WEBRADIO
void I2sWrShow(bool json) {
  SETMEMREGS
    if (running) {
      if (json) {
        ResponseAppend_P(PSTR(",\"WebRadio\":{\"Title\":\"%s\"}"), meta);
      } else {
        WSContentSend_PD(PSTR("{s}" "I2S_WR-Title" "{m}%s{e}"), meta);
      }
    }
}
#endif

void I2S_Play_Cmd(void) {
  SETREGS

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  I2S_Play(cp);

  ResponseCmndDone();
}

const char I2S_Commands[] PROGMEM =
    "I2S|"  // Prefix
    "play|vol|say|wr|"
#ifdef USE_MIC
    "gain|rec|stop|bridge"
#endif
    "";

void (*const I2S_Command[])(void) PROGMEM = {&I2S_Play_Cmd,&SetVolume,&Say,&WebRadio\
#ifdef USE_MIC
,&SetGain,&StartMicRec,&StopMicRec,&I2SBridge\
#endif
};



int32_t i2s_script_cmd(uint32_t sel) {
SETREGS
  
  uint8_t index = sel & 0xff;
  uint8_t type = sel >> 8;
  if (type == 0) {
    if (cmd_param) {
      I2S_Play(cmd_param);
    }
  }

  return 0;
}


void I2SAudio_Deinit() {
  SETREGS
#ifdef USE_MP3
  MP3Decoder_FreeBuffers();
  if (m_outBuff) free(m_outBuff);
  if (m_inBuff) free(m_inBuff);  
#endif
  i2s_end(i2sp);
#ifdef USE_AUDIO_CODECS
  switch (codec) {
    case 1:
      I2cResetActive(W8960_ADDR, codec_bus);
      break;
    case 2:
      I2cResetActive(ES8156_ADDR, codec_bus);
      break;
  }
#endif
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

static int32_t mod_func_execute(uint32_t sel) {
  SETREGS
  bool result = false;

#ifdef USE_SCRIPT
  uint8_t tst = sel >> 31;
  if (tst) {
    uint8_t module = sel >> 16;
    if (module == 42) {
      return i2s_script_cmd(sel);
    } else {
      return 0;
    }
  }
#endif

  switch (sel) {
    case pFUNC_INIT:
      result = I2SAudio_Init();
      break;
    case pFUNC_COMMAND:
      result = DecodeCommand(I2S_Commands, I2S_Command);
      break;
#ifdef USE_WEBRADIO
    case pFUNC_WEB_SENSOR:
      I2sWrShow(false);
      break;
    case pFUNC_JSON_APPEND:
      I2sWrShow(true);
#endif
    break;
    case pFUNC_DEINIT:
      I2SAudio_Deinit();
      break;
  }
  return result;
}
PULL_OPTIONS
#endif  // USE_I2S_MOD
