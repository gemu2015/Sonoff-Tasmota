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
  uint8_t SPIreadCommand;
  uint8_t SPIwriteCommand;
} MOD;

typedef struct {
  int8_t csPin;
  int8_t irqPin;
  int8_t rstPin;
  int8_t gpioPin;

  void *spi;
  //Module* mod;
  float frequency;
  float bitRate;
  uint8_t rawRSSI;
  uint8_t rawLQI;
  uint8_t modulation;
  size_t packetLength;
  bool packetLengthQueried;
  uint8_t packetLengthConfig;
  bool promiscuous;
  bool crcOn;
  bool directModeEnabled;
  int8_t power;
  MOD mod;
} MODULE_MEMORY;

//#include "cc1101_c.h"

#define CC1101_REV 1<<16|3

PUSH_OPTIONS

MODULE_DESCRIPTOR("CC1101",MODULE_TYPE_DRIVER,CC1101_REV,"CS",15,"GDO",5,"",0,"",0)

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

  STGLOB

  this->irqPin = -1;
  this->rstPin = -1;
  this->frequency = 434.0;
  this->bitRate = 4.8;
  this->modulation = 0b00000000;
  this->crcOn = true;
  this->directModeEnabled = true;
  this->power = 10;

  if (TasmotaGlobal->spi_enabled) {
    this->csPin = mp->ms[0].value;
    pinMode(this->csPin, OUTPUT);
    digitalWrite(this->csPin, HIGH);
    this->gpioPin = mp->ms[1].value;
    pinMode(this->gpioPin, INPUT_PULLUP);
    GETSPI(0)
    spi_begin();

    initialized = true;
    return true;
  }
  
  CC1101_Deinit();
  return false;
} 


void CC1101_Deinit() {
  SETREGS
  spi_end();
  RETMEM
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/
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
