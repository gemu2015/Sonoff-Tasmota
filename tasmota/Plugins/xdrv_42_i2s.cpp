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


typedef struct {
  uint16_t dummy;
} MODULE_MEMORY;

#define I2S_REV 1 << 16 | 4

PUSH_OPTIONS

MODULE_DESCRIPTOR("I2SAUDIO", MODULE_TYPE_DRIVER, I2S_REV, "", 0, "", 0, "", 0, "", 0)

// all functions must be declared MUDULE_PART
MODULE_PART int32_t I2SAudio_Init();
MODULE_PART void I2S_PlayWave(void);
MODULE_PART void I2SAudio_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END

const char S_JSON_FNF[] PROGMEM = "{\"File %s not found\"}";
const char S_JSON_ILLF[] PROGMEM = "{\"Illegal File format\"}";

int32_t I2SAudio_Init() {
  ALLOCMEM

  initialized = true;
  return 0;
}

void I2S_PlayWave(void) {
  SETREGS

  char *cp = XdrvMailbox->data;
  while (*cp == ' ') cp++;

  File_p *wf;
  wf = fopen(cp, 'r');

  if (!wf) {
    // file not found
    Response_P(GSTR(S_JSON_FNF), cp);
    return;
  }

  int16_t buffer[512]; 

  // check for RIFF
  fread((char*)buffer, 1, sizeof(wav_header_t), wf);
 
   wav_header_t *wh = (wav_header_t *)buffer;
   // 0x52494646
  if (wh->Riff.ChunkID != 0x46464952 && wh->Fmt.NumChannels != 1) {
    fclose(wf);
    Response_P(GSTR(S_JSON_ILLF));
    return;
  }

  i2s_begin();

  i2s_set_rate(wh->Fmt.SampleRate);

  while (1) {
    uint32_t bytesread = fread((char*)buffer, 1, sizeof(buffer), wf);
    if (!bytesread) {
      break;
    }
    for (uint32_t i = 0; i < bytesread / 2; i++) {
      i2s_write_sample(buffer[i]);
      OsWatchLoop();
    }
  }

  i2s_end();

  fclose(wf);

  ResponseCmndDone();

  return;
}



const char I2S_Commands[] PROGMEM =
    "I2S|"  // Prefix
    "pw";
void (*const I2S_Command[])(void) PROGMEM = {&I2S_PlayWave};

void I2SAudio_Deinit() {
  SETREGS
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
