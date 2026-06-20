/*
  xdrv_42_i2s.cpp - I2S audio support for Tasmota

  Copyright (C) 2025  gemu2015

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
codec settings access
*/

#include "tasmota_options.h"

#if defined(USE_I2S_MOD) && defined(EXECUTE_FROM_BINARY)

#define XDRV_42 42

#ifdef ESP32
#define MAX_MOD_STORES 10
#else
#define MAX_MOD_STORES 4
#endif

#ifdef ESP32
  #define USE_PSHINE
  #define USE_PICOTTS
#endif

#include "module.h"
#include "module_defines.h"

#define USE_SAY

#ifdef ESP32
#define USE_MIC
#define USE_MP3_PSRAM
#define USE_MP3
#define USE_WEBRADIO
#include <layer3.h>
#ifndef I2S_BRIDGE_PORT
#define I2S_BRIDGE_PORT 6970
#endif

#define I2S_BRIDGE_MODE_OFF 0
#define I2S_BRIDGE_MODE_READ 1
#define I2S_BRIDGE_MODE_WRITE 2


// select a codec
#define USE_AUDIO_CODECS

#ifdef USE_AUDIO_CODECS
#include "Audio/es8156/src/audio_hal.h"
#include "Audio/es8156/src/es8156.h"
#include "Audio/es7243e/src/es7243e.h"
#include "Audio/es8311/src/es8311.h"
#include "Audio/es7210/src/es7210.h"
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

#define I2S_BRIDGE_BUFFER_SIZE 1024

PUSH_OPTIONS

typedef union {
  uint8_t data;
  struct {
    uint8_t master : 1;
    uint8_t enabled : 1;
    uint8_t swap_mic : 1;
    uint8_t bmode : 2;
  };
} BRIDGE_MODE;

// I2S_BRIDGE
typedef struct {
  BRIDGE_MODE bridge_mode;
  void *i2s_bridge_udp;
  void *i2s_bridgec_udp;
  IP_ADDRESS i2s_bridge_ip;
  int8_t ptt_pin;
  uint8_t ring_pressed;
  uint8_t task_running;
} I2S_BRIDGE;


typedef struct {
#if defined(USE_SCRIPT) || defined(USE_TINYC)
  char *cmd_param;
#endif
  uint8_t gain_div;
#ifdef USE_MIC
  I2S_BRIDGE bridge;
  uint8_t adc_gain_fac;
  uint8_t is2_mic_init;
  uint8_t is2_server_init;
  uint8_t recording;
  shine_t shine_ptr;
  int16_t *shine_buffer;
  uint16_t shine_bsize;
#ifdef USE_PSHINE
  uint32_t shine_counter[5];
#endif
  void *mp3_server;
  void *mp3_client;
  uint8_t mp3_stream;
  uint8_t stream_enable;
#endif
  uint8_t i2s_busy;
  uint8_t i2s_mode;
  File_p *wf;
  bool running;
  uint8_t force_mono;
  uint8_t chans;
  uint8_t codec;
  uint32_t srate;
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
  char wr_url[256];        // persistent copy of the stream URL (for reconnect)
  uint8_t *wr_ring;        // PSRAM jitter ring buffer (buffering)
  uint32_t wr_rsize;       // ring capacity (power of two)
  uint32_t wr_rhead;       // producer index (monotonic)
  uint32_t wr_rtail;       // consumer index (monotonic)
  uint32_t wr_icycount;    // data bytes since last ICY metadata block
  uint8_t  wr_secure;      // 1 = https (TLS client)
#endif

#ifdef USE_SAY
  SamData *samdata;
  SAM_RENDER *samrender;
#endif

#ifdef USE_PICOTTS
  // PicoTTS runtime — text-to-speech via SVOX picotts (Apache 2.0).
  // Voice files (~0.9–1.4 MB per language) live on LittleFS; when the
  // first I2STTS is issued we lazy-load them into PSRAM, hand the
  // pointers to the picotts engine via picotts_set_resources(), then
  // start the picotts FreeRTOS task. Codec / I2S TX channel / pins are
  // already owned by this plugin — picotts piggy-backs on Write_Samples().
  bool picotts_initialized;
  bool picotts_init_failed;
  volatile uint32_t picotts_last_audio_ms; // heartbeat from audio_cb,
                                            // used to detect "synthesis
                                            // done" without racing the
                                            // picotts engine's own idle
                                            // signal
  uint8_t *picotts_ta_buf;                  // text-analysis voice in PSRAM
  uint32_t picotts_ta_size;
  uint8_t *picotts_sg_buf;                  // signal-generator voice
  uint32_t picotts_sg_size;
  char picotts_lang[8];                     // e.g. "en-US", "de-DE"
#endif

  I2S_PARS i2sp;

} MODULE_MEMORY;

#define stream_enable mem->stream_enable
#define mp3_stream mem->mp3_stream
#define mp3_client mem->mp3_client
#define is2_server_init mem->is2_server_init
#define mp3_server mem->mp3_server
#define shine_bsize mem->shine_bsize
#define shine_ptr mem->shine_ptr
#define shine_buffer mem->shine_buffer

#define recording mem->recording
#define is2_mic_init mem->is2_mic_init
#define i2sp mem->i2sp
#define audio_pwr_pin mem->audio_pwr_pin
#define dout_pin mem->dout_pin
#define bck_pin mem->bck_pin
#define ws_pin mem->ws_pin
#define mc_pin mem->mc_pin
#define din_pin mem->din_pin
#define i2sp mem->i2sp
#define gain_div mem->gain_div
#define adc_gain_fac mem->adc_gain_fac
#define bridge mem->bridge
#define i2s_busy mem->i2s_busy
#define i2s_mode mem->i2s_mode
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
#define wr_url mem->wr_url
#define wr_ring mem->wr_ring
#define wr_rsize mem->wr_rsize
#define wr_rhead mem->wr_rhead
#define wr_rtail mem->wr_rtail
#define wr_icycount mem->wr_icycount
#define wr_secure mem->wr_secure
#define tx_ready mem->tx_ready
#define cmd_param mem->cmd_param
#define codec_bus mem->codec_bus

#ifdef USE_PICOTTS
#define picotts_initialized   mem->picotts_initialized
#define picotts_init_failed   mem->picotts_init_failed
#define picotts_last_audio_ms mem->picotts_last_audio_ms
#define picotts_ta_buf        mem->picotts_ta_buf
#define picotts_ta_size       mem->picotts_ta_size
#define picotts_sg_buf        mem->picotts_sg_buf
#define picotts_sg_size       mem->picotts_sg_size
#define picotts_lang          mem->picotts_lang
#endif


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
#define GPIO_DIN 18
#define GPIO_APWR 49
#define GPIO_MC 49
#define GPIO_PDMC 49      // PDM mic clock — referenced in MODULE_DESCRIPTOR10 below
#else

#if 1
#define GPIO_DOUT 17
#define GPIO_DIN 16
#define GPIO_BCK 10
#define GPIO_WS 18
#define GPIO_MC 49
#define GPIO_PDMC 49
#else
#define GPIO_DOUT 15
#define GPIO_BCK 17
#define GPIO_WS 47
#endif

#define GPIO_APWR 49

#endif

#define I2S_REV 1 << 16 | 5
#ifdef ESP8266
MODULE_DESCRIPTOR(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0)
#else
MODULE_DESCRIPTOR10(MODNAME, MODULE_TYPE_DRIVER, I2S_REV, "DOUT", GPIO_DOUT, "DIN/PDD", GPIO_DIN, "BCK", GPIO_BCK, "WS", GPIO_WS, "MC", GPIO_MC,"MODE", 0x01000200,"CODEC", 0x01000401,"APWR", GPIO_APWR,"PDC",GPIO_PDMC,"",0)
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
MODULE_PART void i2s_mic_task(void *arg);
MODULE_PART void StartMicRec(void);
MODULE_PART uint32_t ChkBusy();
MODULE_PART void StopMicRec(void);
MODULE_PART void Stream_enable(void);
MODULE_PART void I2SBridge(void);
MODULE_PART uint32_t I2SBridgeCmd(uint8_t val, uint8_t flg);
MODULE_PART void SendBridgeCmd(uint8_t bmode);
MODULE_PART void i2s_bridge_init(uint8_t bmode);
MODULE_PART void make_mono(int16_t *packet_buffer, uint32_t size);
MODULE_PART void i2s_bridge_task(void *arg);
MODULE_PART void I2SBridgeInit(void);
MODULE_PART void I2SBridgeDeinit(void);
MODULE_PART void i2s_bridge_loop(void);

MODULE_PART void I2SStreamInit(void);
MODULE_PART void I2SStreamDeinit(void);
MODULE_PART void Stream_mp3(void);
MODULE_PART int32_t i2s_record_shine(char *path, uint32_t stream);


MODULE_PART void WebRadio(void);
MODULE_PART void Say(void);
MODULE_PART void AudioPwr(uint32_t pwr);
MODULE_PART void I2S_Enable(uint32_t enable);
MODULE_PART void I2S_SetRate(uint32_t freq, uint32_t channels, uint32_t rxtx);
MODULE_PART int32_t W8960_Init(void);
MODULE_PART void W8960_Write(uint8_t reg_addr, uint16_t data);
MODULE_PART void W8960_SetGain(uint8_t sel, uint16_t value);
MODULE_PART void Write_Samples(int16_t *buffer, uint32_t samples);
MODULE_PART void I2sWrShow(bool json);
MODULE_PART int32_t i2s_script_cmd(uint32_t sel);
MODULE_PART void I2SAudio_Deinit();
#ifdef USE_PICOTTS
MODULE_PART void Cmnd_TTS(void);
MODULE_PART void Cmnd_TTSLang(void);
MODULE_PART void picotts_audio_cb(int16_t *samples, unsigned count);
MODULE_PART void picotts_idle_cb(void);
MODULE_PART void picotts_error_cb(void);
MODULE_PART void PicoShutdownEngine(void);
#endif
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END


#ifdef USE_MP3
#include "Audio/MP3/mp3_decoder_c.h"
#endif

#ifdef USE_PSHINE

#include "Audio/mp3_shine_esp32/src/includes.h"

const float FP_CONST[] PROGMEM = {
  3.14159265358979f,     // 0: PI
  0.087266462599717f,    // 1: PI36
  0.049087385212f,       // 2: PI64
  0.69314718f,           // 3: LN2
  0.5f,                  // 4: 0.5
  1.5f,                  // 5: 1.5
  4.768371584e-7f,       // 6: 1024/0x7fffffff
  0.0946f,               // 7: quantize spec offset
  4.656612875e-10f,      // 8: 1/0x7fffffff
  2.0f,                  // 9: 2.0
  1e9f,                  // 10: 1e9
  2.147483647e-9f,       // 11: 0x7fffffff * 1e-9
  0.043633231299858f,    // 12: PI/72
  2.147483647e9f,        // 13: 0x7fffffff as float
  3.1f,                  // 14: reservoir PE factor
  1.0f,                  // 15: 1.0
};

const uint32_t INT_CONST[] PROGMEM = {
  0xbe6f0000,   //  0: fast-sqrt magic (p_sqrt_int)
  0x5f3759df,   //  1: SQRT_MAGIC_F (p_f_sqrt)
  101123,       //  2: sqrt branch threshold
  8192,         //  3: quantize range limit
  165140,       //  4: 8192^(4/3) check
  0x7fffffff,   //  5: max int32
  10000,        //  6: int2idx loop bound
  100000,       //  7: fail sentinel
  576,          //  8: GRANULE_SIZE
  480,          //  9: subband offset increment
  511,          // 10: HAN_SIZE-1 bitmask
  4095,         // 11: max bits limit
  4096,         // 12: BUFFER_SIZE
  2304,                        // 13: 4*GRANULE_SIZE (malloc size)
  sizeof(l3loop_t),            // 14: sizeof(l3loop_t)
  sizeof(shine_side_info_t),   // 15: sizeof(shine_side_info_t)
  sizeof(shine_global_config), // 16: sizeof(shine_global_config)
  sizeof(int32_t) * 22,       // 17: sizeof(scalefactor.l[gr][ch])
  sizeof(int32_t) * 13 * 3,   // 18: sizeof(scalefactor.s[gr][ch])
  sizeof(int32_t) * HAN_SIZE, // 19: sizeof(subband.x[i])
};

#include "Audio/mp3_shine_esp32/src/bitstream_c.h"
#include "Audio/mp3_shine_esp32/src/huffman_c.h"
#include "Audio/mp3_shine_esp32/src/tables_c.h"
#include "Audio/mp3_shine_esp32/src/l3bitstream_c.h"
#include "Audio/mp3_shine_esp32/src/l3loop_c.h"
#include "Audio/mp3_shine_esp32/src/l3mdct_c.h"
#include "Audio/mp3_shine_esp32/src/l3subband_c.h"
#include "Audio/mp3_shine_esp32/src/layer3_c.h"
#include "Audio/mp3_shine_esp32/src/reservoir_c.h"

#endif

#define OUTBUFF_SIZE 1024 * 6
#define INBUFF_SIZE 1024 * 2

const char S_JSON_FNF[] PROGMEM = "{\"File %s not found\"}";
const char S_JSON_ILLF[] PROGMEM = "{\"Illegal File format\"}";
const char S_JSON_BUSY[] PROGMEM = "{\"audio is busy\"}";
const char S_JSON_STOPSND[] PROGMEM = "{\"audio stopped\"}";

// Named PROGMEM strings for the picotts code path. Inline PSTR("…")
// inside a single command's call chain is unsafe in plugins — the
// per-function literal pool can't hold more than one cleanly, so the
// 2nd+ PSTR resolves to garbage. snprintf_P with a garbage format
// then writes garbage path bytes to ta_path/sg_path, PicoLoadVoice
// happily opens "whatever that points to", picotts_init reads the
// resulting buffer as a SVOX resource, and the engine ends up calling
// a function pointer extracted from random data — the InstrFetchProhibited
// at PC=0x150f0d56 we hit on `i2sttslang de-de`. All inline PSTR("…")
// in the picotts command path → moved here as named arrays + GSTR().
#ifdef USE_PICOTTS
const char S_PTT_TA_PATH[]    PROGMEM = "/picotts_%s_ta.bin";
const char S_PTT_SG_PATH[]    PROGMEM = "/picotts_%s_sg.bin";
const char S_PTT_OPEN_FAIL[]  PROGMEM = "PTT: open(%s) failed (upload via /ufsu)";
const char S_PTT_SIZE_RANGE[] PROGMEM = "PTT: %s size %u out of range (16..2 MB)";
const char S_PTT_ALLOC_FAIL[] PROGMEM = "PTT: PSRAM alloc %u B for %s failed";
const char S_PTT_SHORT_READ[] PROGMEM = "PTT: short read %d/%u from %s";
const char S_PTT_LOADED[]     PROGMEM = "PTT: loaded %s (%u B) -> PSRAM";
const char S_PTT_INIT_FAIL[]  PROGMEM = "PTT: picotts_init failed";
const char S_PTT_READY[]      PROGMEM = "PTT: ready (lang=%s, voice=%u+%u B from FS)";
const char S_PTT_LANG_RESP[]  PROGMEM = "{\"%s\":\"%s\"}";
// Hardcoded language code matches PICOTTS_DEFAULT_LANG defined later
// in the file. Keep both in sync. Temporarily set to "de-DE" to
// validate the German voice end-to-end while the lang-switch crash
// is being debugged — first-iniz loads German voices directly,
// avoiding the broken shutdown+reinit path.
const char S_PTT_LANG_DEFAULT[] PROGMEM = "de-DE";
const char S_PTT_LANG_TOO_LONG[] PROGMEM = "PTT: language code too long";
const char S_PTT_VOICE_LOAD_FAIL[] PROGMEM = "PTT: voice load failed (check /picotts_<lang>_ta.bin and _sg.bin on FS)";
// Cmnd_TTS error responses — were bare C-literals in plugin code,
// which lands them in .rodata WITHOUT EXEC_OFFSET applied = wild
// pointer at runtime. Routed through GSTR via named PROGMEM strings.
const char S_PTT_USAGE[]      PROGMEM = "Usage: I2STTS <utf8 text>";
const char S_PTT_INIT_FS[]    PROGMEM = "PTT: init failed (need voice files on FS)";
#endif
#if defined(USE_MP3) || defined(USE_SAY)
const char S_JSON_MEMERR[] PROGMEM = "{\"out of memory\"}";
#endif
#ifdef USE_AUDIO_CODECS
const char S_JSON_WMERR[] PROGMEM = "{\"Codec error\"}";
#endif

// Bumped 8 KB → 16 KB to give Shine encode chain enough headroom.
// p_shine_iteration_loop alone allocates 0x190 (400 B) frame and then nests
// 4-5 levels deeper into outer_loop → bin_search → calc_runlen / quantize /
// count_bit / subdivide, plus Xtensa register-window saves (16 B/level).
// At 8 KB the stack would overflow into adjacent heap and corrupt the malloc
// blocks for `config->l3_enc[ch][gr]` (a wild pointer like 0x97e19a34 then
// crashed `calc_runlen` with LoadProhibited on the next encode).
#define TASK_STACK 16384
//#define TASK_STACK 8192

#ifdef USE_SAY
#define RENDER_SIZE sizeof(SAM_RENDER)
#else
#define RENDER_SIZE 0
#endif

const char tname[] PROGMEM = "I2STASK";
const uint32_t ui32_const[7] PROGMEM = {OUTBUFF_SIZE, TASK_STACK, 0x46464952 , INBUFF_SIZE,0x7fffffff,I2S_BRIDGE_BUFFER_SIZE,16000};
const int32_t i32_const[7] PROGMEM = {32768, -32768, 22050, RENDER_SIZE, 37541, 32000, 44100};

#ifdef USE_PICOTTS
// Large constants the picotts code path uses. Kept in PROGMEM because
// values >12 bits compile to fixsfti(<imm>) on Xtensa, which is broken
// in the BinPlugin build environment (see PIC linker note in
// MEMORY.md). Loaded via `picp[N] + EXEC_OFFSET` like the other
// const arrays in this file.
//   [0] PCM sample rate (Hz)
//   [1] hard ceiling for one I2STTS call (ms)
//   [2] max accepted voice-file size (bytes)
const uint32_t pico_uconst[3] PROGMEM = {16000, 60000, 2 * 1024 * 1024};
#endif


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

  i2sp.dout = mp->ms[0].value;
  i2sp.din = mp->ms[1].value;
  i2sp.bclk = mp->ms[2].value;
  i2sp.ws = mp->ms[3].value;
  i2sp.mclk = mp->ms[4].value;
  i2sp.bmode = mp->ms[5].value;
  codec = mp->ms[6].value;
  audio_pwr_pin = mp->ms[7].value;

  i2sp.pdm_clk = mp->ms[8].value;

  if (audio_pwr_pin < GetPins()) {
    pinMode(audio_pwr_pin, OUTPUT);
    digitalWrite(audio_pwr_pin, LOW);
  } else {
    audio_pwr_pin = -1;
  }

  if (i2sp.din >= GetPins()) {
    i2sp.din = -1;
  }

  if (i2sp.mclk >= GetPins()) {
    i2sp.mclk = -1;
  }

  // if pdm_clk >= 0 we use a pdm microphone, sentinel >= GetPins()
  if (i2sp.pdm_clk >= GetPins()) {
    i2sp.pdm_clk = -1;
  }

  gain_div = 1<<6;  // = 1

  i2s_begin_t(&i2sp);

#ifdef USE_MIC
  is2_mic_init = 0;
  if (i2sp.din >= 0 || i2sp.pdm_clk >= 0) {
    // RX channel already created and initialized by i2s_begin_t
    adc_gain_fac = 1;
  }
  bridge.ptt_pin = -1;
  bridge.ring_pressed = 0;
#endif

#ifdef ESP32
  i2s_event_callbacks_t cbs;
  cbs.on_recv = NULL;
  cbs.on_recv_q_ovf = NULL;
  cbs.on_sent = i2s_tx_ready_callback;
  cbs.on_send_q_ovf = NULL;
  i2sp.cbp = (void*)&cbs;
  i2s_channel_register_event_callback_t(&i2sp);
  i2sp.timeout = 1000;
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
      if (pes7243e_codec_init(&codec_bus) < 0) {
        I2SAudio_Deinit();
        AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_WMERR));
        return -2;
      }
      break;
    case 3:   // ES8311 DAC (e.g. ESP32-P4 Nano, codec on I2C bus 1 @ 0x18)
      if (pes8311_codec_init(AUDIO_HAL_MODE_SLAVE, AUDIO_HAL_BIT_LENGTH_16BITS, &codec_bus) < 0) {
        I2SAudio_Deinit();
        AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_WMERR));
        return -2;
      }
      pes8311_codec_set_voice_volume(75);
      break;
    case 4:   // ESP32-P4 Waveshare 10.1" display board: ES8311 DAC (0x18) + ES7210 4-mic ADC (0x40), shared I2C
      if (pes8311_codec_init(AUDIO_HAL_MODE_SLAVE, AUDIO_HAL_BIT_LENGTH_16BITS, &codec_bus) < 0) {
        I2SAudio_Deinit();
        AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_WMERR));
        return -2;
      }
      pes8311_codec_set_voice_volume(75);
      pes7210_codec_init(&codec_bus);   // 4-mic ADC; non-fatal — speaker (ES8311) works without it
      break;
  }
#endif

  i2s_busy = false;
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

void I2S_Enable(uint32_t enable) {
  SETREGS
  if (enable) {
    i2s_enable_tx(&i2sp);
#ifdef USE_MIC
    if (i2sp.din >= 0 || i2sp.pdm_clk >= 0) {
      i2s_enable_rx(&i2sp);
    }
#endif
  } else {
    i2s_disable_tx(&i2sp);
#ifdef USE_MIC
    if (i2sp.din >= 0 || i2sp.pdm_clk >= 0) {
      i2s_disable_rx(&i2sp);
    }
#endif
  }
}

void I2S_SetRate(uint32_t freq, uint32_t channels, uint32_t rxtx) {
  SETREGS
  i2sp.dlen = freq;
  i2sp.channels = channels;
  if (rxtx & 1) {
    i2s_set_rate_t(&i2sp);
  }
#ifdef USE_MIC
  if (i2sp.din >= 0 || i2sp.pdm_clk >= 0) {
    if (rxtx & 2) {
      i2s_set_rate_r(&i2sp);
    }
  }
#endif
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
  I2S_Enable(0);

  AudioPwr(0);
  i2s_busy = false;

  vTaskDelete(0);
}
#endif

void I2S_Play(char *cp) {
SETREGS 

  if (i2s_busy) {
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
    I2S_SetRate(wh.Fmt.SampleRate, 1, 1);

    i2s_busy = true;

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
    I2S_Enable(0);

    AudioPwr(0);
    i2s_busy = false;
#endif
  } else {
#ifdef USE_MP3
    if (!strncmp_P(ep, PSTR("mp3"), 3)) {
      i2s_busy = true;
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

uint32_t ChkBusy() {
  SETREGS
  if (i2s_busy) {
    AddLog(LOG_LEVEL_INFO, PSTR("I2S Audio busy"));
    return true;
  }
  return false;
}

#ifdef USE_MIC
// microphone related code
// bridge
/*
I2SBridge	ip = sets the IP of the slave device
0 = stop bridge
1 = start bridge in read mode
2 = start bridge in write mode
3 = start bridge in loopback mode
4 = set bridge to master
5 = set bridge to slave
6 = set microphone to swapped
7 = set microphone to not swapped
p<x> = sets the push to talk button where x is the button's GPIO pin number
*/

void I2SBridge(void) {
  SETREGS

  if (XdrvMailbox->data_len > 0) {
    char *cp = XdrvMailbox->data;
    if (strchr(cp, '.')) {
      // enter destination ip
      bridge.i2s_bridge_ip.bytes[0] = strtol(cp, &cp, 10);
      cp++;
      bridge.i2s_bridge_ip.bytes[1] = strtol(cp, &cp, 10);
      cp++;
      bridge.i2s_bridge_ip.bytes[2] = strtol(cp, &cp, 10);
      cp++;
      bridge.i2s_bridge_ip.bytes[3] = strtol(cp, &cp, 10);
      cp++;
      char buffer[32];
      sprintf_P(buffer, PSTR("%u.%u.%u.%u"), bridge.i2s_bridge_ip.bytes[0], bridge.i2s_bridge_ip.bytes[1], bridge.i2s_bridge_ip.bytes[2], bridge.i2s_bridge_ip.bytes[3]);
      Response_P(PSTR("{\"I2S: bridge\":{\"IP\":\"%s\"}}"), buffer);
    } else if (cp = strchr(cp, 'p')) {
      // enter push to talk pin
      cp++;
      bridge.ptt_pin = atoi(cp);
      pinMode(bridge.ptt_pin, INPUT_PULLUP);
      Response_P(PSTR("{\"I2S: bridge\":{\"PTT-PIN\":%d}}"), bridge.ptt_pin);
    } else {
      if (XdrvMailbox->payload == 3) {
        i2s_bridge_init(3);
        ResponseCmndNumber(bridge.bridge_mode.bmode);
        return;
      }

      if (XdrvMailbox->payload == 99) {
        SendBridgeCmd(99);
        ResponseCmndNumber(XdrvMailbox->payload);
        return;
      }

      I2SBridgeCmd(XdrvMailbox->payload, 1);
    }
  }
}

void SendBridgeCmd(uint8_t bmode) {
  SETREGS
  char slavecmd[16];
  if (bridge.bridge_mode.master) {
    sprintf_P(slavecmd, PSTR("cmd:%d"), bmode);
    udp_beginPacket(bridge.i2s_bridgec_udp, bridge.i2s_bridge_ip.dword , I2S_BRIDGE_PORT + 1);
    udp_write(bridge.i2s_bridgec_udp, (const uint8_t*)slavecmd, strlen(slavecmd));
    udp_endPacket(bridge.i2s_bridgec_udp);
    char ipstr[20];
    sprintf_P(ipstr, PSTR("%d.%d.%d.%d"), bridge.i2s_bridge_ip.bytes[0], bridge.i2s_bridge_ip.bytes[1], bridge.i2s_bridge_ip.bytes[2], bridge.i2s_bridge_ip.bytes[3]);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge send to ip %s cmd: %s"), ipstr, slavecmd);
  }
}

#define MAX_UDP_BSIZE 1000

uint32_t I2SBridgeCmd(uint8_t val, uint8_t flg) {
  SETREGS

  if (val == 99) {
    if (webcam_GetWidth() != 0) {
      AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge request picture"));
      webcam_GetFrame(1);
      uint8_t *buff;
      int32_t len = webcam_PicStore(0, &buff);
      char cbuff[16];
      sprintf_P(cbuff, PSTR("pic:%d:"),len);
      udp_beginPacket(bridge.i2s_bridgec_udp, bridge.i2s_bridge_ip.dword , I2S_BRIDGE_PORT + 1);
      udp_write(bridge.i2s_bridgec_udp, (const uint8_t*)cbuff, strlen(cbuff));
      udp_endPacket(bridge.i2s_bridgec_udp);
      if (len) {
#ifdef CAM_PAKET        
        uint16_t plen = MAX_UDP_BSIZE;
        uint8_t *bptr = buff;
        while (len > 0) {
          if (len < plen) {
            plen = len;
          }
          udp_beginPacket(bridge.i2s_bridgec_udp, bridge.i2s_bridge_ip.dword , I2S_BRIDGE_PORT + 1);
          udp_write(bridge.i2s_bridgec_udp, (const uint8_t*)bptr, plen);
          udp_endPacket(bridge.i2s_bridgec_udp);
          len -= plen;
          bptr += plen;
        }
#endif

        /*
        char fname[16];
        strcpy_P(fname,PSTR("/pict.jpg"));
        wf = fopen(fname, 'w');
        fwrite(buff, 1, len, wf);
        fclose(wf);
        */
      } else {
        AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge request picture failed"));
      }
    }
    ResponseCmndNumber(val);
    return val;
  }

  if ((val >= 0) && (val <= 11)) {
    if (val > 3) {
      switch (val) {
        case 4:
          bridge.bridge_mode.master = 1;
          break;
        case 5:
          bridge.bridge_mode.master = 0;
          break;
        case 6:
          bridge.bridge_mode.swap_mic = 1;
          break;
        case 7:
          bridge.bridge_mode.swap_mic = 0;
          break;
      }
      Response_P(PSTR("{\"I2S: bridge\":{\"MASTER\":%d,\"SWAP_MIC\":%d}}"), bridge.bridge_mode.master, bridge.bridge_mode.swap_mic);
    } else {
      if (bridge.bridge_mode.bmode != val) {
        if ((val == I2S_BRIDGE_MODE_OFF) && (bridge.bridge_mode.bmode != I2S_BRIDGE_MODE_OFF)) {
          if (flg &&  (bridge.bridge_mode.master)) {
            // shutdown slave
            SendBridgeCmd(I2S_BRIDGE_MODE_OFF);
          }
          i2s_bridge_init(I2S_BRIDGE_MODE_OFF);
        } else {
          if (bridge.bridge_mode.bmode == I2S_BRIDGE_MODE_OFF) {
            // initial on
            i2s_bridge_init(val);
          } else {
            // change mode
            if (val & I2S_BRIDGE_MODE_READ) {
              //SpeakerMic(MODE_SPK);
            }
            if (val & I2S_BRIDGE_MODE_WRITE) {
              //SpeakerMic(MODE_MIC);
            }
          }
        }

        bridge.bridge_mode.bmode = val;

        if (flg) {
          if (bridge.bridge_mode.master) {
            // set slave to complementary mode
            if (bridge.bridge_mode.bmode && ((bridge.bridge_mode.bmode & 3) != 3)) {
              uint8_t slavemode = I2S_BRIDGE_MODE_READ;
              if (bridge.bridge_mode.bmode & I2S_BRIDGE_MODE_READ) {
                slavemode = I2S_BRIDGE_MODE_WRITE;
              }
              SendBridgeCmd(slavemode);
            }
          }
        }
      }
      ResponseCmndNumber(bridge.bridge_mode.bmode);
    }
  }
  return val;
}

void I2SBridgeInit(void) {
  SETREGS
  // create udp instances and start udp control channel
  bridge.i2s_bridge_udp = new_udp();
  bridge.i2s_bridgec_udp = new_udp();
  if (bridge.i2s_bridgec_udp) {
    udp_begin(bridge.i2s_bridgec_udp, I2S_BRIDGE_PORT + 1);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge command server created on port: %d "), I2S_BRIDGE_PORT + 1);
  }
  i2s_bridge_init(I2S_BRIDGE_MODE_OFF);
}

void I2SBridgeDeinit(void) {
  SETREGS

  if (bridge.bridge_mode.bmode) {
    i2s_bridge_init(I2S_BRIDGE_MODE_OFF);
  }

  if (bridge.i2s_bridge_udp) {
    udp_del(bridge.i2s_bridge_udp);
    bridge.i2s_bridge_udp = 0;
  }
  if (bridge.i2s_bridgec_udp) {
    udp_del(bridge.i2s_bridgec_udp);
    bridge.i2s_bridgec_udp = 0;
  }

}

#define CAM_PAKET

void i2s_bridge_loop(void) {
  SETREGS
  STGLOB
  StateBitfield test = TasmotaGlobal->global_state;
  if (test.wifi_down) {
    return;
  }

  if (!is2_mic_init) {
    I2SBridgeInit();
    is2_mic_init = 1;
  }

  if (!is2_server_init) {
    if (stream_enable) {
      I2SStreamInit();
      is2_server_init = 1;
    }
  }

  if (mp3_server) {
      WebServerHandleClient(mp3_server);
  }

  uint8_t packet_buffer[32];

  if (bridge.ptt_pin >= 0) {
    if (bridge.bridge_mode.master) {
      // master mode: PTT toggles audio direction
      if (bridge.bridge_mode.bmode != I2S_BRIDGE_MODE_OFF) {
        if (digitalRead(bridge.ptt_pin) == 0) {
          if (bridge.bridge_mode.bmode != I2S_BRIDGE_MODE_WRITE) {
            I2SBridgeCmd(I2S_BRIDGE_MODE_WRITE, 1);
          }
        } else {
          if (bridge.bridge_mode.bmode != I2S_BRIDGE_MODE_READ) {
            I2SBridgeCmd(I2S_BRIDGE_MODE_READ, 1);
          }
        }
      }
    } else {
      // slave mode: button sends RING to the app via data UDP port
      if (digitalRead(bridge.ptt_pin) == 0) {
        if (!bridge.ring_pressed) {
          bridge.ring_pressed = 1;
          if (bridge.i2s_bridge_udp && bridge.i2s_bridge_ip.dword) {
            const char *ring_msg = "RING";
            udp_beginPacket(bridge.i2s_bridge_udp, bridge.i2s_bridge_ip.dword, I2S_BRIDGE_PORT);
            udp_write(bridge.i2s_bridge_udp, (const uint8_t*)ring_msg, 4);
            udp_endPacket(bridge.i2s_bridge_udp);
            AddLog(LOG_LEVEL_INFO, PSTR("I2S: RING sent to app"));
          }
        }
      } else {
        bridge.ring_pressed = 0;
      }
    }
  }

  if (bridge.i2s_bridgec_udp && udp_parsePacket(bridge.i2s_bridgec_udp)) {
      // received control command
    memset(packet_buffer, 0, sizeof(packet_buffer));
    udp_read(bridge.i2s_bridgec_udp, packet_buffer, udp_available(bridge.i2s_bridgec_udp));
    char *cp = (char*)packet_buffer;
    if (!strncmp_P(cp, PSTR("cmd:"), 4)) {
      bridge.i2s_bridge_ip.dword = udp_remoteIP(bridge.i2s_bridgec_udp);
      cp += 4;
      uint32_t val = I2SBridgeCmd(atoi(cp), 0);
      AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge remote cmd %d"), val);
      return;
    }
    if (!strncmp_P(cp, PSTR("pic:"), 4)) {
      // received pic
      cp += 4;
      uint32_t len = strtoul(cp, &cp, 10);
      cp++;
      AddLog(LOG_LEVEL_INFO, PSTR("I2S bridge received pic size: %d"), len);
      if (len) {
#ifdef CAM_PAKET
        uint8_t *pptr = (uint8_t*)special_malloc(len + MAX_UDP_BSIZE);
        if (pptr) {
          uint8_t *dp = pptr;
          int32_t xlen = len;
          for (uint32_t cnt = 0; cnt < (len / MAX_UDP_BSIZE) + 1; cnt++) {
            uint16_t plen = udp_parsePacket(bridge.i2s_bridgec_udp);
            if (plen) {
              udp_read(bridge.i2s_bridgec_udp, dp, plen);
              dp += plen;
              xlen -= plen;
              if (xlen <= 0) {
                break;
              }
            }
            delay(1);
          }
          udp_flush(bridge.i2s_bridgec_udp);

          /*
          char fname[16];
          strcpy_P(fname,PSTR("/pict.jpg"));
          wf = fopen(fname, 'w');
          fwrite(pptr, 1, len, wf);
          fclose(wf);
          */
           
          Draw_JPEG(pptr, len, 0, 0, 0); 
          free(pptr);
        }
#endif
        return;
      }
    }
  }
}

void i2s_bridge_init(uint8_t bmode) {
SETREGS

  if (I2S_BRIDGE_MODE_OFF == bmode) {
    bridge.bridge_mode.bmode = bmode;
    // wait for task to exit
    for (int i = 0; i < 50 && bridge.task_running; i++) {
      delay(10);
    }
    udp_flush(bridge.i2s_bridge_udp);
    udp_stop(bridge.i2s_bridge_udp);
  } else {
    // don't start if another I2S operation is active
    if (i2s_busy) {
      AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge blocked, I2S busy"));
      return;
    }
    // don't create a second task
    if (bridge.task_running) {
      AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge task already running"));
      bridge.bridge_mode.bmode = bmode;
      return;
    }

    bridge.bridge_mode.bmode = bmode;

    const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
    udp_begin(bridge.i2s_bridge_udp, I2S_BRIDGE_PORT);

    TASKPARS tp;
    tp.pvTaskCode = GVOID(i2s_bridge_task);
    tp.constpcName = GSTR(tname);
    tp.usStackDepth = uicp[1];
    tp.constpvParameters = (char*)GSTR(tname);
    tp.uxPriority = 3;
    tp.constpvCreatedTask = nullptr;
    tp.xCoreID = 1;
    xTaskCreatePinnedToCore(&tp);

    if (bmode == 3) {
      AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge loop mode started"));
    } else {
      if (!bridge.bridge_mode.master) {
        AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge slave started"));
      } else {
        char buffer[32];
        sprintf_P(buffer, PSTR("%u.%u.%u.%u"), bridge.i2s_bridge_ip.bytes[0], bridge.i2s_bridge_ip.bytes[1], bridge.i2s_bridge_ip.bytes[2], bridge.i2s_bridge_ip.bytes[3]);
        AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge master started sending to ip: %s"), buffer);
      }
    }
  }
}

// make mono
void make_mono(int16_t *packet_buffer, uint32_t size) {
SETREGS

    int16_t *wp = (int16_t*)packet_buffer;
    for (uint32_t cnt = 0; cnt < size / 2; cnt += 2) {
      int16_t val;
      if (bridge.bridge_mode.swap_mic) {
        val = wp[cnt + 1] * adc_gain_fac;
      } else {
        val = wp[cnt] * adc_gain_fac;
      }
      wp[cnt] = val;
      wp[cnt + 1] = val;
    }
}

void i2s_bridge_task(void *arg) {
SETREGS

  bridge.task_running = 1;
  i2s_busy = true;
  AudioPwr(1);

  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

  uint32_t bytesize = uicp[5];

  int16_t *packet_buffer = (int16_t*)calloc((bytesize>>1)+4, 2);
  if (!packet_buffer) {
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge alloc failed"));
    bridge.task_running = 0;
    i2s_busy = false;
    AudioPwr(0);
    vTaskDelete(0);
    return;
  }

  // set to 16 khz Stereo
  I2S_SetRate(uicp[6], 2, 3);

  uint32_t bytes_read;
  i2sp.dptr = packet_buffer;

  while (I2S_BRIDGE_MODE_OFF != bridge.bridge_mode.bmode) {
    if ((bridge.bridge_mode.bmode & 3) == 3) {
      // loopback test mode
      i2sp.dlen = bytesize;
      bytes_read = i2s_read_samples_r(&i2sp);
      if (bytes_read) {
        make_mono(packet_buffer, bytes_read);
        i2sp.dlen = bytes_read;
        i2s_write_samples_t(&i2sp);
      } else {
        // No RX data — or no RX channel at all (DIN/mic pin not configured, so
        // i2s_channel_read returns ESP_ERR_INVALID_ARG immediately). Without this
        // yield the loopback busy-spins at prio 3 pinned to core 1 and starves the
        // core into a watchdog hang. The non-loopback READ branch below already
        // delays on empty; mirror it here so a missing mic pin can't hang the device.
        delay(1);
      }
    } else {
      if (bridge.bridge_mode.bmode & I2S_BRIDGE_MODE_READ) {
        if (udp_parsePacket(bridge.i2s_bridge_udp)) {
          uint32_t len = udp_available(bridge.i2s_bridge_udp);
          if (len > bytesize) {
            len = bytesize;
          }
          len = udp_read(bridge.i2s_bridge_udp, (uint8_t *)packet_buffer, len);
          udp_flush(bridge.i2s_bridge_udp);
          i2sp.dlen = len;
          i2s_write_samples_t(&i2sp);
        } else {
          delay(1);
        }
      }

      if (bridge.bridge_mode.bmode & I2S_BRIDGE_MODE_WRITE) {
        i2sp.dlen = bytesize;
        bytes_read = i2s_read_samples_r(&i2sp);
        if (bytes_read) {
          make_mono(packet_buffer, bytes_read);
          udp_beginPacket(bridge.i2s_bridge_udp, bridge.i2s_bridge_ip.dword, I2S_BRIDGE_PORT);
          udp_write(bridge.i2s_bridge_udp, (const uint8_t*)packet_buffer, bytes_read);
          udp_endPacket(bridge.i2s_bridge_udp);
        }
      }
    
    }
    //delay(1);
  }
  AddLog(LOG_LEVEL_INFO, PSTR("I2S: bridge stopped"));
  free(packet_buffer);
  AudioPwr(0);
  I2S_Enable(0);
  i2s_busy = false;
  bridge.task_running = 0;
  vTaskDelete(0);
}

// stream and rec related code

void i2s_mic_task(void *arg) {
SETREGS

  uint8_t *ucp;
  int32_t written;

  uint32_t bytes_read;
  i2sp.dptr = shine_buffer;
  i2sp.dlen = shine_bsize;

  // DIAG: log entry conditions and what we expect to see
  AddLog(LOG_LEVEL_INFO, PSTR("MICDIAG: task started, dlen=%d din=%d pdm=%d recording=%d"),
         shine_bsize, i2sp.din, i2sp.pdm_clk, recording);

  // DIAG: PSRAM cache-coherency check. shine_ptr (`config`) is allocated in PSRAM
  // by Core 0 (Shine_initialise). This task runs on Core 1. If Core 1 sees
  // *different* l3_enc values than Core 0 logged at "SHINE: init OK", it's a
  // stale-cache read — Core 0's writes haven't been flushed to PSRAM yet.
  // shine_t = shine_global_config* ; offset of l3_enc[2][2] is computed by linker.
  // We just print the same fields by direct deref.
  if (shine_ptr) {
    shine_global_config *cfg = (shine_global_config*)shine_ptr;
    AddLog(LOG_LEVEL_INFO, PSTR("MICDIAG: cfg=%p l3_enc[]={%p,%p,%p,%p} l3loop=%p"),
           cfg,
           cfg->l3_enc[0][0], cfg->l3_enc[0][1],
           cfg->l3_enc[1][0], cfg->l3_enc[1][1],
           cfg->l3loop);
  }

  int32_t lval;
  volatile const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);
  pclamp = icp[0];
  mclamp = icp[1];

  // DIAG: counters so we can see if loop runs and what i2s/encoder return
  uint32_t loop_n = 0;
  uint32_t bytes_read_total = 0;
  uint32_t mp3_bytes_total = 0;
  uint32_t fwrite_calls = 0;
  uint32_t zero_reads = 0;

  while (recording & 2) {
      bytes_read = i2s_read_samples_r(&i2sp);
      loop_n++;
      if (bytes_read) {
        bytes_read_total += bytes_read;
        if (adc_gain_fac > 1) {
          // set gain
          for (uint32_t cnt = 0; cnt < bytes_read / 2; cnt++) {
            lval = shine_buffer[cnt] * adc_gain_fac;
            if (lval < mclamp) {
              lval = mclamp;
            } else if (lval > pclamp) {
              lval = pclamp;
            }
            shine_buffer[cnt] = lval;
          }
        }
        ucp = (uint8_t*)Shine_encode_buffer_interleaved(shine_ptr, shine_buffer, &written);
        if (written > 0) mp3_bytes_total += written;
        if (!mp3_stream) {
          fwrite(ucp, 1, written, wf);
          if (written > 0) fwrite_calls++;
        } else {
          if (client_connected(mp3_client)) {
            client_write(mp3_client, (const char*)ucp, written);
          } else {
            break;
          }
        }
      } else {
        zero_reads++;
      }

      // DIAG: every ~1s of iterations (assuming ~30Hz), spit out a status line
      if ((loop_n % 30) == 0) {
        AddLog(LOG_LEVEL_INFO, PSTR("MICDIAG: loop=%u bytes_read=%u mp3_bytes=%u fwrites=%u zero_reads=%u"),
               loop_n, bytes_read_total, mp3_bytes_total, fwrite_calls, zero_reads);
      }
  }
  ucp = (uint8_t*)Shine_flush(shine_ptr, &written);

  // DIAG: final tally before flush write
  AddLog(LOG_LEVEL_INFO, PSTR("MICDIAG: loop exit - loop=%u bytes_read=%u mp3_bytes=%u fwrites=%u zero_reads=%u flush_bytes=%d"),
         loop_n, bytes_read_total, mp3_bytes_total, fwrite_calls, zero_reads, written);

  if (!mp3_stream) {
    fwrite(ucp, 1, written, wf);
    fclose(wf);
  } else {
    client_write(mp3_client, (const char*)ucp, written);
    client_stop(mp3_client);
    client_delete(mp3_client);
  }
  Shine_close(shine_ptr);

  AddLog(LOG_LEVEL_INFO, PSTR("I2S recording: stopped"));
  recording = 0;
  i2s_busy = false;
  vTaskDelete(0);
}

#define MIC_RATE 16000
#define MIC_CHANNELS 2

int32_t i2s_record_shine(char *file, uint32_t stream) {
SETREGS
  
  mp3_stream = stream;
  int32_t error = 0;
  shine_config_t  config;
  const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

  uint8_t channel = MIC_CHANNELS;

  if (file || stream) {
    AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: enter file=%p stream=%u rate=%u chans=%u din=%d pdm_clk=%d"),
           file, stream, uicp[6], channel, i2sp.din, i2sp.pdm_clk);
    Shine_set_config_mpeg_defaults(&config.mpeg);
    if (channel == 1) {
      config.mpeg.mode = MONO;
    } else {
      config.mpeg.mode = STEREO;
    }
    config.mpeg.bitr = 128;
    config.wave.samplerate = uicp[6];
    config.wave.channels = (channels)channel;
    if (Shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0) {
      AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: Shine_check_config FAILED"));
      error = -1;
      goto exit;
    }

    shine_ptr = (shine_t)Shine_initialise(&config);
    if (!shine_ptr) {
      AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: Shine_initialise FAILED"));
      error = -2;
      goto exit;
    }
    AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: Shine init OK, ptr=%p"), shine_ptr);

    if (!stream) {
      wf = fopen(file, 'w');
      if (!wf) {
        AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: fopen('%s') FAILED"), file);
        error = -3;
        goto exit;
      }
      AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: fopen OK, wf=%p"), wf);
    } else {
      client_flush(mp3_client);
      client_setTimeout(mp3_client, 5);
      client_print(mp3_client, PSTR("HTTP/1.1 200 OK\r\n"));
      client_print(mp3_client, PSTR("Content-Type: audio/mpeg;\r\n\r\n"));
    }

    uint16_t samples_per_pass;
    samples_per_pass = Shine_samples_per_pass(shine_ptr);
    shine_bsize = samples_per_pass * 2 * channel;
    shine_buffer = (int16_t*)malloc(shine_bsize);
    if (!shine_buffer) {
      AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: malloc(%d) FAILED"), shine_bsize);
      error = -4;
      goto exit;
    }
    AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: spp=%u bsize=%u buf=%p"),
           samples_per_pass, shine_bsize, shine_buffer);

    i2s_busy = true;

    // set to 16 khz Stereo
    I2S_SetRate(uicp[6], channel, 3);
    AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: I2S_SetRate(%u,%u,3) done"), uicp[6], channel);

    recording = 2;
    TASKPARS tp;
    tp.pvTaskCode = GVOID(i2s_mic_task);
    tp.constpcName = GSTR(tname);
    tp.usStackDepth = uicp[1];
    tp.constpvParameters = (char*)GSTR(tname);
    tp.uxPriority = 3;
    tp.constpvCreatedTask = nullptr;
    tp.xCoreID = 1;
    // PSRAM cache-coherency safety: `shine_ptr` (config) was malloc'd in PSRAM
    // by Core 0 in Shine_initialise. The mic task runs on Core 1 (xCoreID=1)
    // and reads many fields off `config`. ESP32-S3 PSRAM has per-core L1
    // caches; without a flush + delay, Core 1 may snoop stale bytes (the
    // 0x97e1_xxxx garbage we saw is exactly old PSRAM contents, not random).
    // Memory barrier + small delay gives Core 0's writes time to write back.
    __sync_synchronize();
    delay(50);
    uint32_t tret = xTaskCreatePinnedToCore(&tp);
    AddLog(LOG_LEVEL_INFO, PSTR("RECDIAG: xTaskCreate returned %u (1=ok, 0=fail), recording=%d"),
           tret, recording);

  } else {
    recording = 1;
    while (recording) {
      delay(1);
    }
  }

  return 0;

exit:
  if (shine_ptr) Shine_close(shine_ptr);
  if (wf) {
    fclose(wf);
  }
  return error;
}

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
    adc_gain_fac = fixunssfsi(xgain);
  } 
  gain = fixunssfsi(fmul(fdiv(floatunsisf(adc_gain_fac) , floatunsisf(64)), floatunsisf(100)));
  ResponseCmndNumber(gain);
}

void StopMicRec(void) {
  SETREGS
  i2s_record_shine(0, 0);
  AddLog(LOG_LEVEL_INFO, PSTR("I2S: rec stopped"));
  ResponseCmndDone();
}

void Stream_enable(void) {
  SETREGS
  if (XdrvMailbox->payload == 1) {
    stream_enable = 1;
  } else {
    stream_enable = 0;
  }
  ResponseCmndNumber(XdrvMailbox->payload);
}

void StartMicRec(void) {
  SETREGS
  if (XdrvMailbox->data_len > 0) {
    if (ChkBusy()) {
      return;
    }
    char *cp = XdrvMailbox->data;
    while (*cp == ' ') {
      cp++;
    }
    i2s_record_shine(cp, 0);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: rec started to file: %s"), cp);
  } else {
    i2s_record_shine(0, 0);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: rec stopped"));
  }
  ResponseCmndDone();
}

// stream section
#ifndef MP3_STREAM_PORT
#define MP3_STREAM_PORT 81
#endif

void Stream_mp3(void) {
  SETREGS
  if (!i2s_busy) {
    mp3_client = NewWebServerGetClient(mp3_server);
    i2s_record_shine(0, 1);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: Handle mp3server"));
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: can not handle client - other stream task active"));
  }
}

void I2SStreamInit(void) {
  SETREGS
  mp3_server = NewWebServer(MP3_STREAM_PORT);
  if (mp3_server) {
    uint8_t *vp = (uint8_t*)Stream_mp3;
    vp += EXEC_OFFSET;
    WebServerOn(mp3_server, PSTR("/stream.mp3"), vp);
    WebServerBegin(mp3_server);
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: mp3 server created on port: %d "), MP3_STREAM_PORT);
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("I2S: mp3 server could not been created"));
  }
}

void I2SStreamDeinit(void) {
  SETREGS
  if (mp3_server) {
    WebServerStop(mp3_server);
    WebServerDelete(mp3_server);
    mp3_server = 0;
  }
}

#endif // USE_MIC

void SetVolume(void) {
  SETREGS
  uint16_t gain;
 
  if (XdrvMailbox->data_len > 0) {
    char *cp = XdrvMailbox->data;
    while (*cp == ' ') cp++;
    gain = strtol(cp, &cp, 10);
    // 100 = unity (gain_div 64). Allow up to 400 = ~4x amplification: picotts /
    // SAM TTS output well below full-scale, so unity is too quiet on a plain I2S
    // amp. Write_Samples clamps to pclamp/mclamp, so boosting only hard-clips the
    // rare peak. gain_div is uint8_t → cap at 255 (~3.98x).
    if (gain > 400) {
        gain = 400;
    }
    if (gain < 1) {
      gain = 1;
    }
    float xgain = fmul(fdiv(floatunsisf(gain) , floatunsisf(100)), floatunsisf(64));
    uint32_t xg = fixunssfsi(xgain);
    if (xg > 255) { xg = 255; }
    gain_div = xg;
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
  i2sp.dptr = buffer;
  i2sp.dlen = samples * 2;
  i2s_write_samples_t(&i2sp);
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

  I2S_SetRate(srate, chans, 1);

  filepos = 0;

  AddLog(LOG_LEVEL_INFO, PSTR("mp3 srate = %d, channels = %d"), srate, chans); 

  I2S_Enable(1);
  
  i2s_busy = true;
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
  I2S_Enable(0);
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
  I2S_Enable(0);
  fclose(wf);
  i2s_busy = false;
  AudioPwr(0);

  vTaskDelete(0);
}
#endif

#ifdef USE_AUDIO_CODECS
#include "Audio/WM8960/p_wm8960_c.h"
#include "Audio/es8156/src/p_es8156_c.h"
#include "Audio/es7243e/src/p_es7243e_c.h"
#include "Audio/es8311/src/p_es8311_c.h"
#include "Audio/es7210/src/p_es7210_c.h"
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

  if (ChkBusy()) {
    return;
  }

  const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);

  chans = 1;
  force_mono = 1;
  srate = icp[2];
  I2S_SetRate(srate, chans, 1);

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;
  
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

  i2s_busy = true;

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
  I2S_Enable(1);

  SetInput(inbuff);

  uint8_t *vp = (uint8_t*)OutputByteCallback;
  vp += EXEC_OFFSET;

  SAMMain((void (*)(void*, unsigned char))vp, samdata);
  
  free(samdata);
  free(samrender);

  I2S_Wait_Ready();
  I2S_Enable(0);

  running = false;
  i2s_busy = false;
  AudioPwr(0);
  ResponseCmndDone();

#endif
}

// =====================================================================
// PicoTTS (SVOX, Apache 2.0) — text-to-speech via the existing I2S TX
// channel. Engine sources live in lib/libesp32_div/pico/ and are pulled
// in by the firmware build when USE_PICOTTS is defined; the firmware
// exposes a 6-entry slice of the engine API on the BinPlugin jumptable
// (slots 209..214), reached via the picotts_init / picotts_add /
// picotts_set_resources / etc. macros from module_defines.h.
//
// We do NOT include picotts.h here: it would conflict with the function-
// like jumptable macros (a `void picotts_init(...)` prototype gets eaten
// by `#define picotts_init jpicotts_init`). Instead, the few engine
// types we touch are inlined.
//
// The codec, I2S TX pins, channel setup, AudioPwr, and gain/clamp are
// all already owned by this plugin — picotts piggy-backs on them, calls
// Write_Samples() from its FreeRTOS task, and never opens its own I2S
// channel. Languages live on LittleFS as picotts_<code>_ta.bin and
// picotts_<code>_sg.bin pairs (~0.9–1.4 MB per language); the I2STTSLang
// command tears down the engine, frees the old voice from PSRAM, loads
// the new pair, and re-inits — no reflash needed to switch language.
// =====================================================================
#ifdef USE_PICOTTS

// Indices into pico_uconst[] (defined near i32_const, kept in PROGMEM
// so values >12 bits don't go through Xtensa's broken fixsfti() path).
#define PICO_K_SRATE_HZ    0   // 16000
#define PICO_K_MAX_WAIT    1   // 60000 ms hard ceiling per I2STTS call
#define PICO_K_MAX_VOICE   2   //  2 MiB max accepted voice file size
#define PICO_GET_UCP \
  const uint32_t *picp = (const uint32_t *) ((uint8_t *)pico_uconst + EXEC_OFFSET)

#ifndef PICOTTS_DEFAULT_LANG
#define PICOTTS_DEFAULT_LANG  "de-DE"   // temp: validate German voice while lang-switch crash is debugged
#endif

// Hand-rolled size constant: `sizeof(mem->picotts_lang)` would
// double-expand through the field macro into
// `sizeof(mem->mem->picotts_lang)` and fail to compile. Keep this in
// sync with the array width in MODULE_MEMORY (`char picotts_lang[8]`).
#define PICOTTS_LANG_SZ 8

// How long to keep waiting for a fresh batch of samples after the last
// audio_cb fired. picotts emits in ~50–200 ms bursts depending on
// sentence density; 500 ms of silence is a safe "synthesis is done".
// Stays as a literal — fits in 12 bits, so the compiler emits an
// `addmi`/`movi` immediate and skips the broken fixsfti() path.
#define PICOTTS_IDLE_GAP_MS    500
// The hard ceiling per I2STTS call (60000 ms) lives in pico_uconst[]
// because it doesn't fit in 12 bits. See PICO_K_MAX_WAIT.

// Read a file from LittleFS into a freshly-allocated PSRAM buffer via
// the plugin's jfile_* / special_malloc jumptable shims. Returns true
// on success; on failure, leaves *buf=NULL and logs the reason.
// One-GSTR-per-function helpers for PicoLoadVoice's AddLog variants.
// Each takes whatever runtime args the format string consumes and the
// matching GSTR is the only PSTR-ish reference the helper makes.
MODULE_PART void ptt_log_open_fail(const char *path) {
  SETREGS
  AddLog(LOG_LEVEL_ERROR, GSTR(S_PTT_OPEN_FAIL), path);
}
MODULE_PART void ptt_log_size_range(const char *path, unsigned sz) {
  SETREGS
  AddLog(LOG_LEVEL_ERROR, GSTR(S_PTT_SIZE_RANGE), path, sz);
}
MODULE_PART void ptt_log_alloc_fail(unsigned sz, const char *path) {
  SETREGS
  AddLog(LOG_LEVEL_ERROR, GSTR(S_PTT_ALLOC_FAIL), sz, path);
}
MODULE_PART void ptt_log_short_read(int got, unsigned sz, const char *path) {
  SETREGS
  AddLog(LOG_LEVEL_ERROR, GSTR(S_PTT_SHORT_READ), got, sz, path);
}
MODULE_PART void ptt_log_loaded(const char *path, unsigned sz) {
  SETREGS
  AddLog(LOG_LEVEL_INFO, GSTR(S_PTT_LOADED), path, sz);
}

MODULE_PART bool PicoLoadVoice(const char *path, uint8_t **buf, uint32_t *size) {
  SETREGS
  PICO_GET_UCP;
  *buf = NULL; *size = 0;
  void *f = jfile_open(path, 'r');
  if (!f) {
    ptt_log_open_fail(path);
    return false;
  }
  uint32_t sz = jfile_size(f);
  if (sz < 16 || sz > picp[PICO_K_MAX_VOICE]) {
    ptt_log_size_range(path, (unsigned)sz);
    jfile_close(f);
    return false;
  }
  // special_malloc prefers PSRAM on this build target — voice files are
  // 500 KB–1 MB each, too big for internal SRAM.
  uint8_t *p = (uint8_t *)special_malloc(sz);
  if (!p) {
    ptt_log_alloc_fail((unsigned)sz, path);
    jfile_close(f);
    return false;
  }
  // jfile_read maps to a single File::read() in firmware (xdrv_123 tmod_file_read),
  // and SD/FATFS read() can return FEWER bytes than requested for a large read
  // (observed: 483328 of 634996 from the SD; LittleFS happened to fill in one call).
  // Loop until the whole voice file is in, or a real EOF/error (got <= 0) stops us.
  uint32_t off = 0;
  while (off < sz) {
    int32_t got = jfile_read(f, p + off, sz - off);
    if (got <= 0) { break; }   // EOF or read error
    off += (uint32_t)got;
  }
  jfile_close(f);
  if (off != sz) {
    ptt_log_short_read((int)off, (unsigned)sz, path);
    free(p);
    return false;
  }
  ptt_log_loaded(path, (unsigned)sz);
  *buf  = p;
  *size = sz;
  return true;
}

// audio_cb runs on the picotts FreeRTOS task. The TX channel is set up
// for mono (chans=1, see Cmnd_TTS), so we hand the samples straight to
// Write_Samples — it applies gain/clamp and forwards via
// i2s_write_samples_t. DMA backpressure from i2s_channel_write throttles
// the picotts engine naturally, so no ring buffer is needed.
MODULE_PART void picotts_audio_cb(int16_t *samples, unsigned count) {
  SETREGS
  if (!count || !samples) return;
  picotts_last_audio_ms = millis();
  Write_Samples(samples, count);
}

MODULE_PART void picotts_idle_cb(void) {
  SETREGS
  // No state to flip — last_audio_ms heartbeat is what the foreground
  // command waits on. Logged at debug level only; the engine fires
  // idle whenever its input queue drains, including between sentences.
  AddLog(LOG_LEVEL_DEBUG, PSTR("PTT: synthesis idle"));
}

MODULE_PART void picotts_error_cb(void) {
  SETREGS
  AddLog(LOG_LEVEL_ERROR, PSTR("PTT: engine task aborted"));
  // Force a re-init on next call. The engine task has already exited
  // by the time this fires.
  picotts_initialized = false;
  picotts_init_failed = false;
}

// Tear down the engine + voice buffers. Codec / I2S / pins are
// untouched (owned by the plugin lifecycle).
MODULE_PART void PicoShutdownEngine(void) {
  SETREGS
  if (picotts_initialized) {
    picotts_shutdown();
    picotts_initialized = false;
  }
  picotts_set_resources(NULL, NULL);
  if (picotts_ta_buf) { free(picotts_ta_buf); picotts_ta_buf = NULL; picotts_ta_size = 0; }
  if (picotts_sg_buf) { free(picotts_sg_buf); picotts_sg_buf = NULL; picotts_sg_size = 0; }
}

// Single-GSTR-per-function helpers used by PicoLazyInit. Each owns
// exactly one PROGMEM-string reference so the per-statement literal
// pool always holds at most one entry per call.
MODULE_PART void ptt_set_lang_default(char *dst) {
  SETREGS
  strncpy(dst, GSTR(S_PTT_LANG_DEFAULT), PICOTTS_LANG_SZ - 1);
  dst[PICOTTS_LANG_SZ - 1] = '\0';
}
MODULE_PART void ptt_make_ta_path(char *dst, size_t cap, const char *lang) {
  SETREGS
  snprintf_P(dst, cap, GSTR(S_PTT_TA_PATH), lang);
}
MODULE_PART void ptt_make_sg_path(char *dst, size_t cap, const char *lang) {
  SETREGS
  snprintf_P(dst, cap, GSTR(S_PTT_SG_PATH), lang);
}
MODULE_PART void ptt_log_init_fail(void) {
  SETREGS
  AddLog(LOG_LEVEL_ERROR, GSTR(S_PTT_INIT_FAIL));
}
MODULE_PART void ptt_log_ready(const char *lang, unsigned ta_sz, unsigned sg_sz) {
  SETREGS
  AddLog(LOG_LEVEL_INFO, GSTR(S_PTT_READY), lang, ta_sz, sg_sz);
}

// Lazy-init: load voice from LittleFS, register notify cbs, start the
// picotts FreeRTOS task. Re-runs after a language switch.
MODULE_PART bool PicoLazyInit(void) {
  SETREGS
  if (picotts_initialized) return true;
  if (picotts_init_failed) return false;

  // Default language on first init — single-GSTR helper.
  if (picotts_lang[0] == 0) {
    ptt_set_lang_default(picotts_lang);
  }

  char ta_path[40], sg_path[40];
  ptt_make_ta_path(ta_path, sizeof(ta_path), picotts_lang);
  ptt_make_sg_path(sg_path, sizeof(sg_path), picotts_lang);
  // Two ways to supply the ~1 MB voice resources to the engine:
  //  (1) FS+PSRAM: load /picotts_<lang>_{ta,sg}.bin from LittleFS into PSRAM and
  //      hand the pointers to the engine. Costs ~1 MB PSRAM for the voice on top
  //      of the engine arena — fine on >=4 MB-PSRAM boards (e.g. 8 MB S3).
  //  (2) PARTITION: if the FS files are absent, leave the runtime pointers unset
  //      so the engine mmaps the picotts_ta / picotts_sg flash partitions
  //      directly — the voice then costs 0 PSRAM. This is how a 2 MB-PSRAM board
  //      fits picotts; create the partitions with "chkpt n picotts_ta <kb>" /
  //      "chkpt n picotts_sg <kb>" and upload the .bins via /partu.
  bool have_ta = PicoLoadVoice(ta_path, &picotts_ta_buf, &picotts_ta_size);
  bool have_sg = have_ta && PicoLoadVoice(sg_path, &picotts_sg_buf, &picotts_sg_size);
  if (have_ta && have_sg) {
    // FS+PSRAM mode — hand voice pointers to the engine before picotts_init.
    picotts_set_resources(picotts_ta_buf, picotts_sg_buf);
  } else {
    // Partition mode — drop any partial PSRAM buffer; the engine will mmap the
    // picotts_ta / picotts_sg partitions via find_*_bin_start().
    if (picotts_ta_buf) { free(picotts_ta_buf); picotts_ta_buf = NULL; picotts_ta_size = 0; }
    if (picotts_sg_buf) { free(picotts_sg_buf); picotts_sg_buf = NULL; picotts_sg_size = 0; }
    picotts_set_resources(NULL, NULL);
  }

  // Function pointers passed across the binplugin boundary need
  // EXEC_OFFSET applied so the firmware sees the actual code address.
  uint8_t *acb = (uint8_t *)picotts_audio_cb; acb += EXEC_OFFSET;
  uint8_t *icb = (uint8_t *)picotts_idle_cb;  icb += EXEC_OFFSET;
  uint8_t *ecb = (uint8_t *)picotts_error_cb; ecb += EXEC_OFFSET;

  picotts_set_idle_notify ((void (*)(void)) icb);
  picotts_set_error_notify((void (*)(void)) ecb);

  if (!picotts_init(5, (void (*)(int16_t *, unsigned)) acb, -1)) {
    ptt_log_init_fail();
    picotts_set_resources(NULL, NULL);
    if (picotts_ta_buf) { free(picotts_ta_buf); picotts_ta_buf = NULL; }
    if (picotts_sg_buf) { free(picotts_sg_buf); picotts_sg_buf = NULL; }
    picotts_init_failed = true;
    return false;
  }

  picotts_initialized = true;
  ptt_log_ready(picotts_lang, (unsigned)picotts_ta_size, (unsigned)picotts_sg_size);
  return true;
}

// Cmnd_TTS error responses — copy from plugin PROGMEM (MMAP_INST,
// byte-access-unsafe on ESP32-S3) into a stack RAM buffer first via
// snprintf_P, then hand the buffer to ResponseCmndChar.
MODULE_PART void ptt_send_usage(void) {
  SETREGS
  char buf[40];
  snprintf_P(buf, sizeof(buf), GSTR(S_PTT_USAGE));
  ResponseCmndChar(buf);
}
MODULE_PART void ptt_send_init_fs(void) {
  SETREGS
  char buf[64];
  snprintf_P(buf, sizeof(buf), GSTR(S_PTT_INIT_FS));
  ResponseCmndChar(buf);
}

// I2STTS <utf-8 text> — synthesise + play once.
void Cmnd_TTS(void) {
  SETREGS
  PICO_GET_UCP;

  if (XdrvMailbox->data_len <= 0) {
    ptt_send_usage();
    return;
  }
  if (ChkBusy()) {
    return;
  }
  if (!PicoLazyInit()) {
    ptt_send_init_fs();
    return;
  }

  // Set up the TX channel for picotts's native rate. chans=1 +
  // force_mono mirrors the SAM Say() pattern; the codec consumes the
  // single sample per WS edge and Write_Samples runs the gain/clamp
  // path on each int16.
  i2s_busy = true;
  chans = 1;
  force_mono = 1;
  srate = picp[PICO_K_SRATE_HZ];
  I2S_SetRate(picp[PICO_K_SRATE_HZ], 1, 1);
  AudioPwr(1);
  running = true;
  I2S_Enable(1);

  // Prime the heartbeat so the wait-loop below doesn't think we're
  // already done before the first audio_cb fires.
  picotts_last_audio_ms = millis();

  // Submit text + a NUL terminator so picotts treats this as a complete
  // sentence and starts emitting promptly (rather than buffering until
  // the next call).
  picotts_add(XdrvMailbox->data, XdrvMailbox->data_len);
  // NUL must live in RAM: picotts_add() is a firmware fn that byte-reads the
  // pointer (xQueueSendToBack → memcpy). A `static const` lands in the plugin's
  // mmap'd .rodata, and a byte load there faults (Load access fault on the P4 /
  // LoadStoreError on the S3) — see the MMAP_INST note below. Stack byte = RAM.
  char nul = 0;
  picotts_add(&nul, 1);

  // Wait for synthesis + drain. picotts_last_audio_ms gets bumped from
  // the engine task on every burst. When it hasn't been bumped for
  // PICOTTS_IDLE_GAP_MS we're done.
  uint32_t start = millis();
  while ((millis() - picotts_last_audio_ms) < PICOTTS_IDLE_GAP_MS) {
    delay(20);
    if ((millis() - start) > picp[PICO_K_MAX_WAIT]) {
      AddLog(LOG_LEVEL_ERROR, PSTR("PTT: timeout after %u ms"),
             (unsigned)(millis() - start));
      break;
    }
  }

  I2S_Wait_Ready();
  I2S_Enable(0);
  running = false;
  AudioPwr(0);
  i2s_busy = false;

  ResponseCmndChar(XdrvMailbox->data);
}

// ── PSTR/GSTR-per-function rule ────────────────────────────
// In a binplugin a function can only resolve ONE PSTR or GSTR
// per body — beyond that, the per-function literal pool is
// mishandled by the PIC linker patcher and one of the references
// returns a garbage pointer. Symptoms range from "the 2nd reference
// resolves to nothing meaningful" (silent wrong behaviour) to "garbage
// bytes get fed to a downstream consumer that interprets them as a
// pointer" (InstrFetchProhibited at random addresses, e.g. the
// 0x150f0d56 crash on `i2sttslang de-de`).
//
// To stay within the rule, every GSTR(...) reference lives in its
// own tiny MODULE_PART helper. The command handler itself uses ZERO
// GSTRs and just calls the helpers. Same pattern for PicoLazyInit and
// PicoLoadVoice — each AddLog() variant gets its own helper.
// IMPORTANT: ESP32-S3 maps the plugin partition as MMAP_INST. Byte
// loads (`l8ui`) on MMAP_INST regions crash with LoadStoreError. So
// passing a GSTR-derived pointer (which points INTO the plugin's
// PROGMEM section) to firmware functions that internally do byte
// access (Response_P, ResponseCmndChar's format string handling) is
// fatal. The fix patterns are:
//   1. If we just need to emit a {cmd: "value"} JSON, call
//      ResponseCmndChar(lang) directly — Tasmota uses its OWN PSTR
//      (firmware-side, fine) for the format and just %s-substitutes
//      the lang string from RAM.
//   2. If we need a custom format string, copy it into a stack RAM
//      buffer via snprintf_P first (uses pgm_read_byte = aligned
//      word access + shift, which works on MMAP_INST) and hand
//      the RAM buffer to the response function.
MODULE_PART void ptt_send_lang_resp(const char *cmd, const char *lang) {
  SETREGS
  // ResponseCmndChar produces {"<cmd>":"<lang>"} natively — no need
  // for a custom format string, no GSTR for the format → simplest
  // and avoids the MMAP_INST byte-access trap entirely.
  ResponseCmndChar((char *)lang);
  (void)cmd;  // ResponseCmndChar reads XdrvMailbox->command itself
}

MODULE_PART void ptt_send_too_long(void) {
  SETREGS
  char buf[40];
  snprintf_P(buf, sizeof(buf), GSTR(S_PTT_LANG_TOO_LONG));
  ResponseCmndChar(buf);
}

MODULE_PART void ptt_send_voice_load_fail(void) {
  SETREGS
  char buf[96];
  snprintf_P(buf, sizeof(buf), GSTR(S_PTT_VOICE_LOAD_FAIL));
  ResponseCmndChar(buf);
}

// Returns the default language code copied to the caller's RAM
// buffer — same MMAP_INST byte-access constraint as above. Caller
// must provide a buffer sized PICOTTS_LANG_SZ (8) at minimum.
MODULE_PART void ptt_copy_default_lang(char *dst, size_t cap) {
  SETREGS
  snprintf_P(dst, cap, GSTR(S_PTT_LANG_DEFAULT));
}

// I2STTSLang <code>      switch language ("en-US", "de-DE", "it-IT", …)
// I2STTSLang             show currently-loaded language
void Cmnd_TTSLang(void) {
  SETREGS

  if (XdrvMailbox->data_len <= 0) {
    // Default-lang fallback is copied into RAM by the helper — passing
    // the raw GSTR pointer to Response_P would crash on ESP32-S3
    // (MMAP_INST byte-access fault). The helper does snprintf_P with a
    // local buffer.
    char lang_buf[PICOTTS_LANG_SZ];
    if (picotts_lang[0]) {
      strncpy(lang_buf, picotts_lang, sizeof(lang_buf) - 1);
      lang_buf[sizeof(lang_buf) - 1] = '\0';
    } else {
      ptt_copy_default_lang(lang_buf, sizeof(lang_buf));
    }
    ptt_send_lang_resp(XdrvMailbox->command, lang_buf);
    return;
  }
  // Codes are 5 chars (e.g. "en-US"); allow 7 + NUL for variants.
  if (strlen(XdrvMailbox->data) >= PICOTTS_LANG_SZ) {
    ptt_send_too_long();
    return;
  }
  // strcmp / strcmp_P are NOT exported via the plugin JMPTBL — calling
  // them dispatches through a wild relocation slot (e.g. 0x15152554) and
  // crashes the device with InstrFetchProhibited on the first lang-switch.
  // Use strncmp_P (which IS exported); on ESP32 PROGMEM and RAM share an
  // address space so passing two RAM strings works correctly.
  if (strncmp_P(picotts_lang, XdrvMailbox->data, PICOTTS_LANG_SZ) == 0 &&
      picotts_initialized) {
    ptt_send_lang_resp(XdrvMailbox->command, picotts_lang);
    return;
  }
  // Tear down current engine + voice. Codec stays initialised. Engine
  // re-init happens eagerly so we can report success/failure synchronously.
  PicoShutdownEngine();
  picotts_init_failed = false;
  strncpy(picotts_lang, XdrvMailbox->data, PICOTTS_LANG_SZ - 1);
  picotts_lang[PICOTTS_LANG_SZ - 1] = '\0';
  if (!PicoLazyInit()) {
    ptt_send_voice_load_fail();
    return;
  }
  ptt_send_lang_resp(XdrvMailbox->command, picotts_lang);
}

#endif  // USE_PICOTTS

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

// Large webradio constants — MUST live in a PROGMEM array (loaded via
// EXEC_OFFSET); inline >= 2048 literals become l32r and miscompile in the
// relocatable plugin build.
//   [0] PSRAM ring size   [1] internal-RAM fallback ring size (both power of 2)
//   [2] underrun->reconnect timeout (ms)   [3] reconnect backoff cap (ms)
const uint32_t wr_uconst[4] PROGMEM = { 512 * 1024, 16 * 1024, 4000, 3000 };

// Body reads always go through the PLAIN client_* selectors on `wclient`:
//   http  -> wclient is the WiFiClient handed to HTTPClient
//   https -> wclient is HTTPClientLight::getStreamPtr() (a WiFiClient* that is
//            really a BearSSL secure client; read() dispatches virtually -> TLS)
#define WR_AVAIL()      client_available(wclient)
#define WR_READ()       client_read(wclient)
#define WR_READN(b,n)   client_readn(wclient,(uint8_t*)(b),(n))
// http-object ops are type-aware (Arduino HTTPClient vs Tasmota HTTPClientLight)
#define WR_CONNECTED()  (wr_secure ? httpl_connected(http) : http_connected(http))
#define WR_HTTP_END()   do { if (wr_secure) { httpl_end(http); } else { http_end(http); } } while(0)
#define WR_STOPCLIENT() do { if (!wr_secure) { client_stop(wclient); } } while(0)  // https stream owned by httpl
#define WR_MAX_RETRY 8

// (re)connect the HTTP(S) stream on the stored URL; parse ICY metaint + sample
// rate. Returns the GET status code (200 = ok). Used by initial connect AND
// every reconnect.
MODULE_PART int32_t wr_connect(void) {
  SETMEMREGS
  if (wr_secure) {
    // https: Tasmota's BearSSL HTTPClientLight (does its own TLS). No ICY
    // metadata header support on the light client -> icyMetaInt = 0.
    if (!httpl_begin(http, wr_url)) { return -1; }
    int32_t hcode = httpl_GET(http);
    if (200 == hcode) {
      wclient = httpl_getStreamPtr(http);   // body stream; read via plain client_*
      icyMetaInt = 0;
    }
    return hcode;
  }
  bool res = http_begin(http, wclient, wr_url);
  if (!res) { return -1; }
  delay(100);
  http_addHeader(http, GSTR(head_1), GSTR(head_2));
  http_collectHeaders(http, GUI32p(hdr), 4);
  http_setReuse(http, true);
  http_setFollowRedirects(http, HTTPC_FORCE_FOLLOW_REDIRECTS);
  int32_t code = http_GET(http);
  if (200 == code) {
    volatile const int32_t *icp = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);
    srate = icp[6];
    if (http_hasHeader(http, GSTR(hdr_4))) {
      char *ret = http_header(http, GSTR(hdr_4));
      srate = strtol(ret, 0, 10);
      free(ret);
    }
    if (http_hasHeader(http, GSTR(hdr_1))) {
      char *ret = http_header(http, GSTR(hdr_1));
      icyMetaInt = strtol(ret, 0, 10);
      free(ret);
    } else {
      icyMetaInt = 0;
    }
  }
  return code;
}

// Read up to `want` pure MP3 bytes into dest, stripping inline ICY metadata.
// Non-blocking: returns however many bytes were available now (0..want).
MODULE_PART uint32_t wr_read_data(uint8_t *dest, uint32_t want) {
  SETMEMREGS
  uint32_t got = 0;
  while (got < want) {
    if (icyMetaInt && wr_icycount >= icyMetaInt) {
      if (WR_AVAIL() < 1) { break; }
      uint32_t icylen = (uint32_t)WR_READ() << 4;
      wr_icycount = 0;
      if (icylen) {
        uint32_t st = millis();
        while ((uint32_t)WR_AVAIL() < icylen) {
          if (millis() - st > 300) { break; }
          delay(1);
        }
        char mbuf[272];
        if (icylen <= sizeof(mbuf)) {
          int32_t r = WR_READN(mbuf, icylen);
          if (r > 12 && !strncmp_P(mbuf, PSTR("StreamTitle="), 12)) {
            uint32_t j = 0;
            for (uint32_t i = 12; i < (uint32_t)r && j < sizeof(meta) - 1; i++) {
              char c = mbuf[i];
              if (c == 0 || c == ';') { break; }
              meta[j++] = c;
            }
            meta[j] = 0;
          }
        } else {
          for (uint32_t i = 0; i < icylen; i++) { WR_READ(); }
        }
      }
      continue;
    }
    uint32_t chunk = want - got;
    if (icyMetaInt) {
      // http: bound the read to the next ICY metadata boundary + what's buffered
      int32_t avail = WR_AVAIL();
      if (avail <= 0) { break; }
      uint32_t toMeta = icyMetaInt - wr_icycount;
      if (chunk > toMeta) { chunk = toMeta; }
      if (chunk > (uint32_t)avail) { chunk = avail; }
    }
    // https (icyMetaInt==0): read straight from the stream — read() pumps the
    // BearSSL record machine (available() can be 0 between records).
    int32_t r = WR_READN(dest + got, chunk);
    if (r <= 0) { break; }
    got += (uint32_t)r;
    wr_icycount += (uint32_t)r;
  }
  return got;
}

// webradio task
void I2sTaskWR(char *url) {
  SETREGS

  //AddLog(LOG_LEVEL_INFO, PSTR("WR Task started"));

  // WiFi.setDNS(dns1, dns2);

  running = true;
  AudioPwr(1);


  I2S_Enable(1);

  volatile const uint32_t *ucp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
  uint32_t ibsize = GET_IBS;

  uint8_t *scratch = (uint8_t*)special_malloc(ibsize + 16);
  if (!scratch) { running = false; }

  const uint32_t *wrcp = (const uint32_t *) ((uint8_t *)wr_uconst + EXEC_OFFSET);
  uint32_t t_reconn  = wrcp[2];     // underrun->reconnect timeout (ms)
  uint32_t t_backmax = wrcp[3];     // backoff cap (ms)
  uint32_t mask = wr_rsize - 1;     // wr_rsize is a power of two
  uint32_t prebuf = wr_rsize >> 3;  // pre-buffer ~1/8 of the ring before playback
  int primed = 0;                   //   (absorbs bursty TLS/network delivery)
  uint32_t retries = 0;
  uint32_t lastdata = millis();

  while (running) {

    // ---- FILL: drain the socket into the ring (read-ahead, ICY-stripped) ----
    while (running) {
      uint32_t rfree = wr_rsize - (wr_rhead - wr_rtail);
      if (rfree < ibsize) { break; }              // ring full enough for now
      if (icyMetaInt && WR_AVAIL() <= 0) { break; }  // http: nothing buffered (https pumps via read())
      uint32_t got = wr_read_data(scratch, ibsize);
      if (got == 0) { break; }
      uint32_t pos = wr_rhead & mask;
      uint32_t toend = wr_rsize - pos;
      uint32_t n1 = (got < toend) ? got : toend;
      memcpy(&wr_ring[pos], scratch, n1);
      if (n1 < got) { memcpy(&wr_ring[0], scratch + n1, got - n1); }
      wr_rhead += got;
      lastdata = millis();
    }

    // ---- DECODE one MP3 frame out of the ring (only once primed) ----
    uint32_t rcount = wr_rhead - wr_rtail;
    if (!primed && rcount >= prebuf) { primed = 1; }     // first fill reached the cushion
    if (primed && rcount >= ibsize) {
      uint32_t pos = wr_rtail & mask;
      uint32_t toend = wr_rsize - pos;
      uint32_t n1 = (ibsize < toend) ? ibsize : toend;
      memcpy(m_inBuff, &wr_ring[pos], n1);
      if (n1 < ibsize) { memcpy(m_inBuff + n1, &wr_ring[0], ibsize - n1); }

      m_bytesLeft = ibsize;
      int16_t m_decodeError = MP3Decode(m_inBuff, &m_bytesLeft, m_outBuff, 0);
      if (m_decodeError) {
        wr_rtail += 1;                             // resync: skip a byte, retry next loop
      } else {
        uint32_t bytesDecoded = ibsize - m_bytesLeft;
        if (bytesDecoded == 0) { bytesDecoded = 1; }   // guard against no progress
        wr_rtail += bytesDecoded;

        uint32_t sr = MP3GetSampRate();
        if (sr && sr != srate) {
          srate = sr;
          chans = MP3GetChannels();
          I2S_SetRate(srate, chans, 1);
        }
        uint32_t samples = MP3GetOutputSamps();
        if (samples <= (GET_OBS >> 1)) {
          Write_Samples(m_outBuff, samples);
        }
        retries = 0;                               // making progress -> reset retry budget
      }
    } else {
      // ---- still priming, ring underrun, or dropped stream ----
      if (rcount == 0) { primed = 0; }                   // full underrun -> re-buffer
      int connected = WR_CONNECTED();
      if (!connected || (millis() - lastdata > t_reconn)) {
        if (retries >= WR_MAX_RETRY) {
          AddLog(LOG_LEVEL_INFO, PSTR("WR: give up after %d retries"), retries);
          break;
        }
        retries++;
        AddLog(LOG_LEVEL_INFO, PSTR("WR: reconnect %d"), retries);
        WR_HTTP_END();
        WR_STOPCLIENT();
        uint32_t backoff = retries * 400;
        if (backoff > t_backmax) { backoff = t_backmax; }
        delay(backoff);
        if (!running) { break; }
        if (200 == wr_connect()) { lastdata = millis(); }
        // (failed reconnect: loop retries until WR_MAX_RETRY)
      } else {
        delay(2);                                  // just waiting for the ring to refill
      }
    }
  }

  if (scratch) { free(scratch); }

  WR_HTTP_END();
  if (wr_secure) { httpl_delete(http); } else { http_delete(http); }
  http = 0;
  if (wr_secure) { wclient = 0; }                          // https stream owned by httpl
  else { client_stop(wclient); client_delete(wclient); wclient = 0; }
  if (wr_ring) { free(wr_ring); wr_ring = 0; }

  I2S_Wait_Ready();
  I2S_Enable(0);

  running = false;
  i2s_busy = false;
  AudioPwr(0);

  //AddLog(LOG_LEVEL_INFO, PSTR("WR Task stopped"));
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

  if (i2s_busy == true) {
    if (!*url) {
      // stop running sound
      running = 0;
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_STOPSND));
    } else {
      AddLog(LOG_LEVEL_INFO, GSTR(S_JSON_BUSY));
    }
    return;
  }

  // persistent copy of the URL (the task needs it for reconnect) + scheme detect
  uint32_t ul = 0;
  while (url[ul] && ul < sizeof(wr_url) - 1) { wr_url[ul] = url[ul]; ul++; }
  wr_url[ul] = 0;
  wr_secure = (!strncmp_P(wr_url, PSTR("https:"), 6)) ? 1 : 0;

  // (re)create the right objects (the task deletes + nulls them on stop):
  //   http  -> Arduino HTTPClient + a plain WiFiClient
  //   https -> HTTPClientLight (its own BearSSL TLS); wclient set from getStreamPtr
  if (wr_secure) {
    if (!http) { http = New_HTTPLight(); }
    if (!http) { AddLog(LOG_LEVEL_INFO, PSTR("WR: no httpl")); return; }
  } else {
    if (!wclient) { wclient = New_WiFiClient(); }
    if (!http) { http = New_HTTP(); }
    if (!wclient || !http) { AddLog(LOG_LEVEL_INFO, PSTR("WR: no client/http")); return; }
  }

  // jitter ring buffer (power of two): a big PSRAM ring when PSRAM is present,
  // else a small internal-RAM ring. special_malloc is PSRAM-preferred and
  // returns NULL when the (large) request can't be met on a no-PSRAM device,
  // so the size adapts to what's actually available.
  if (wr_ring) { free(wr_ring); wr_ring = 0; }
  const uint32_t *wrcp = (const uint32_t *) ((uint8_t *)wr_uconst + EXEC_OFFSET);
  wr_rsize = wrcp[0];                                 // ~32 s @128 kbps (PSRAM)
  wr_ring = (uint8_t*)special_malloc(wr_rsize);
  if (!wr_ring) {
    wr_rsize = wrcp[1];                               // no PSRAM -> small internal ring (~1 s)
    wr_ring = (uint8_t*)special_malloc(wr_rsize);
  }
  if (!wr_ring) {
    AddLog(LOG_LEVEL_INFO, PSTR("WR: no ring mem"));
    return;
  }
  wr_rhead = 0; wr_rtail = 0; wr_icycount = 0;
  AddLog(LOG_LEVEL_INFO, PSTR("WR: ring %u bytes"), wr_rsize);

  int32_t code = wr_connect();
  AddLog(LOG_LEVEL_INFO, PSTR("WR result: %d (secure %d)"), code, wr_secure);

  if (200 == code) {
    i2s_busy = true;
    const uint32_t *uicp = (const uint32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);
    TASKPARS tp;
    tp.pvTaskCode = GVOID(I2sTaskWR);
    tp.constpcName = GSTR(tname);
    tp.usStackDepth = uicp[1];
    tp.constpvParameters = (void*)wr_url;
    tp.uxPriority = 3;
    tp.constpvCreatedTask = nullptr;
    tp.xCoreID = 1;
    xTaskCreatePinnedToCore(&tp);
    ResponseCmndDone();
  } else {
    // connect failed: free what we set up here (the task that frees them never launches)
    if (wr_ring) { free(wr_ring); wr_ring = 0; }
    WR_HTTP_END();
    if (wr_secure) { httpl_delete(http); } else { http_delete(http); }
    http = 0;
    if (!wr_secure) { client_delete(wclient); }
    wclient = 0;
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
        WSContentSend_PD(PSTR("{s}" "I2S: WR-Title" "{m}%s{e}"), meta);
      }
    }
}
#endif

void I2S_Play_Cmd(void) {
  SETREGS

  if (ChkBusy()) {
    return;
  }
  
  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  I2S_Play(cp);

  ResponseCmndDone();
}

const char I2S_Commands[] PROGMEM =
    "I2S|"  // Prefix
    "play|vol|say|wr|"
#ifdef USE_MIC
    "gain|rec|stop|bridge|stream|"
#endif
#ifdef USE_PICOTTS
    "tts|ttslang|"
#endif
    "";

void (*const I2S_Command[])(void) PROGMEM = {&I2S_Play_Cmd,&SetVolume,&Say,&WebRadio
#ifdef USE_MIC
,&SetGain,&StartMicRec,&StopMicRec,&I2SBridge,&Stream_enable
#endif
#ifdef USE_PICOTTS
,&Cmnd_TTS,&Cmnd_TTSLang
#endif
};



// Plugin_Query(42, sel) selectors reachable from TinyC/Scripter — the bit31 branch
// in mod_func_execute routes here. index 0/1/2 mirror the play/vol/say console
// commands (the param string arrives in cmd_param); I2S_Q_MICGAIN sets the mic gain
// (1..100, takes a param like vol); I2S_Q_MICLEVEL reads the mic loudness (no param,
// e.g. pluginQuery(buf, 42, 0, 10)).
#define I2S_Q_MICLEVEL 10
#define I2S_Q_MICGAIN  11

int32_t i2s_script_cmd(uint32_t sel) {
SETREGS

  uint8_t index = sel & 0xff;

  switch (index) {
    case 0:                                     // play <file>
      if (cmd_param) {
        I2S_Play(cmd_param);
      }
      break;

    case 1:                                     // vol <n>
    case 2:                                     // say <text>
#ifdef USE_MIC
    case I2S_Q_MICGAIN:                         // mic gain <n> (1..100)
#endif
      if (cmd_param) {
        // these cmd fns read XdrvMailbox->data — point it at the script param.
        // They also end in ResponseCmnd*, which formats XdrvMailbox->command as a
        // %s JSON key. The plugin-query path never sets ->command, so without this
        // it dereferences a stale pointer and faults. Point it at cmd_param — a
        // firmware/DRAM string. Do NOT use a plugin string literal here (e.g.
        // "PQ"): its bytes live in the INST-only-mmap'd plugin partition, and the
        // firmware reads a %s arg byte-by-byte (unlike the format, which goes via
        // pgm_read), so a plugin-pointer %s arg faults too. Plugin_Query discards
        // the response; restore ->command after so the mailbox isn't left dirty.
        char *save_cmd        = XdrvMailbox->command;
        XdrvMailbox->data     = cmd_param;
        XdrvMailbox->data_len = strlen(cmd_param);
        XdrvMailbox->payload  = atoi(cmd_param);
        XdrvMailbox->command  = cmd_param;
        if (index == 1)      { SetVolume(); }
        else if (index == 2) { Say(); }
#ifdef USE_MIC
        else                 { SetGain(); }
#endif
        XdrvMailbox->command  = save_cmd;
      }
      break;

#ifdef USE_MIC
    case I2S_Q_MICLEVEL: {                      // mic loudness (peak |sample|, 0..32767; -1 = no mic)
      char *r = (char *)special_malloc(16);
      if (!r) { break; }
      int peak = -1;                            // -1 = RX channel not available / not running
      // The firmware read handler (xdrv_123) guards on txhandle but then reads
      // rxhandle, so a null/stale rxhandle would call i2s_channel_read(NULL,...)
      // and crash. And i2sp.timeout is passed to i2s_channel_read as a FreeRTOS
      // tick count, not ms — so a one-shot read from the main loop must use 0
      // (non-blocking peek, like the firmware's own getMicLevel) to never stall
      // the loop into a watchdog reset.
      if (i2sp.rxhandle) {
        int16_t  sbuf[128];
        int16_t *save_dptr = i2sp.dptr;
        uint16_t save_dlen = i2sp.dlen;
        uint16_t save_to   = i2sp.timeout;
        i2sp.dptr    = sbuf;
        i2sp.dlen    = sizeof(sbuf);
        i2sp.timeout = 0;                       // ticks! 0 = do-not-wait peek
        uint32_t nbytes = i2s_read_samples_r(&i2sp);
        i2sp.dptr = save_dptr; i2sp.dlen = save_dlen; i2sp.timeout = save_to;
        peak = 0;
        uint32_t cnt = nbytes / 2;
        for (uint32_t i = 0; i < cnt; i++) {
          int v = sbuf[i];
          if (v < 0) { v = -v; }
          if (v > peak) { peak = v; }
        }
      }
      // Format peak by hand — NOT sprintf. A raw sprintf in a plugin passes a
      // plugin-region format-string pointer to the firmware's siprintf, whose
      // byte-wise format parse faults: the plugin partition is mmap'd INST-only
      // (xdrv_123 line ~3179), so byte reads of plugin rodata fault (Load access
      // fault in _svfiprintf_r). The rest of this plugin only formats via the
      // jumptable Response_P/ResponseCmnd*; here we must RETURN a string, so we
      // build the decimal by hand (no format pointer, no printf).
      char *p = r;
      int   v = peak;
      if (v < 0) { *p++ = '-'; v = -v; }
      char tmp[8];
      int  ti = 0;
      do { tmp[ti++] = (char)('0' + (v % 10)); v /= 10; } while (v > 0);
      while (ti > 0) { *p++ = tmp[--ti]; }
      *p = '\0';
      return (int32_t)r;                        // Plugin_Query returns this; caller frees it
    }
#endif
  }

  return 0;
}


void I2SAudio_Deinit() {
  SETREGS
#ifdef USE_PICOTTS
  // Shut down picotts before tearing down the I2S channel: the engine
  // task can still be mid-utterance and would call back into a closed
  // i2sp via Write_Samples. PicoShutdownEngine() also frees the voice
  // buffers from PSRAM and clears the runtime override pointers.
  PicoShutdownEngine();
#endif
#ifdef USE_MP3
  MP3Decoder_FreeBuffers();
  if (m_outBuff) free(m_outBuff);
  if (m_inBuff) free(m_inBuff);
#endif
  i2s_end_t(&i2sp);
#ifdef USE_AUDIO_CODECS
  switch (codec) {
    case 1:
      I2cResetActive(W8960_ADDR, codec_bus);
      break;
    case 2:
      I2cResetActive(ES8156_ADDR, codec_bus);
      I2cResetActive(ES7243_ADDR, codec_bus);
      break;
    case 3:
      I2cResetActive(ES8311_ADDR, codec_bus);
      break;
    case 4:
      I2cResetActive(ES8311_ADDR, codec_bus);
      I2cResetActive(ES7210_ADDR, codec_bus);
      break;
  }
#endif

#ifdef USE_MIC
  I2SStreamDeinit();
  I2SBridgeDeinit();
  i2s_end_r(&i2sp);
#endif

  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

static int32_t mod_func_execute(uint32_t sel) {
  SETREGS
  bool result = false;

#if defined(USE_SCRIPT) || defined(USE_TINYC)
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
#ifdef USE_MIC      
    case pFUNC_LOOP:
      i2s_bridge_loop();
      break;
#endif

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
