/*
  xdrv_130_cc1101.cpp - radio support for Tasmota

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

#ifdef USE_CC1101_MOD

#define XDRV_130             130

#include "module.h"
#include "module_defines.h"

/*********************************************************************************************/
typedef struct {
  uint8_t cs;
  void *spi;
  Module* mod;
  float frequency = 434.0;
  float bitRate = 4.8;
  uint8_t rawRSSI = 0;
  uint8_t rawLQI = 0;
  uint8_t modulation = 0b00000000;
  size_t packetLength = 0;
  bool packetLengthQueried = false;
  uint8_t packetLengthConfig = 0b00000001;
  bool promiscuous = false;
  bool crcOn = true;
  bool directModeEnabled = true;
  int8_t power = 10;
} MODULE_MEMORY;

#define cs mem->cs
#define spi mem->spi

#include "cc1101_c.h"

#define CC1101_REV 1<<16|3

PUSH_OPTIONS

MODULE_DESCRIPTOR("CC1101",MODULE_TYPE_DRIVER,CC1101_REV,"CS",15,"",0,"",0,"",0)

// all functions must be declared MUDULE_PART
MODULE_PART int32_t CC1101_Init();
MODULE_PART void CC1101_Deinit();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END


/*********************************************************************************************\
 * constants
\*********************************************************************************************/



int32_t CC1101_Init() { 
  ALLOCMEM

  initialized = true;
  return 0;
} 


void CC1101_Deinit() {
  SETREGS
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
//#pragma GCC optimize ("-O0")
MOD_RESULT mod_func_execute(uint32_t sel) {
  MOD_RESULT result = false;
  switch (sel) {
    case FUNC_INIT:
      result = CC1101_Init();
      break;
    case FUNC_DEINIT:
      CC1101_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_CC1101_MOD
