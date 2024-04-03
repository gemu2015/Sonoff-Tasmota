/*
  xsns_05_ds18x20.ino - DS18x20 temperature sensor support for Tasmota

  Copyright (C) 2021  Theo Arends and md5sum-as (https://github.com/md5sum-as)

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
#ifdef USE_DS18X20_MOD
#include "module.h"
#include "module_defines.h"
#include "../Tasmota/include/i18n.h"


/*********************************************************************************************\
 * DS18B20 - Temperature - Multiple sensors
\*********************************************************************************************/

#define XSNS_05 5

//#define USE_DS18x20_RECONFIGURE    // When sensor is lost keep retrying or re-configure
//#define DS18x20_USE_ID_AS_NAME     // Use last 3 bytes for naming of sensors

/* #define DS18x20_USE_ID_ALIAS in my_user_config.h or user_config_override.h
 * Use alias for fixed sensor name in scripts by autoexec. Command: DS18Alias XXXXXXXXXXXXXXXX,N where XXXXXXXXXXXXXXXX
 * full serial and N number 1-255 Result in JSON:  "DS18Sens_2":{"Id":"000003287CD8","Temperature":26.3} (example with
 * N=2) Setting N to an alphanumeric value, the complete name is replaced with it Result in JSON:
 * "Outside1":{"Id":"000003287CD8","Temperature":26.3} (example with N=Outside1)
 */

#define DS18S20_CHIPID 0x10   // +/-0.5C 9-bit
#define DS1822_CHIPID 0x22    // +/-2C 12-bit
#define DS18B20_CHIPID 0x28   // +/-0.5C 12-bit
#define MAX31850_CHIPID 0x3B  // +/-0.25C 14-bit

#define W1_SKIP_ROM 0xCC
#define W1_CONVERT_TEMP 0x44
#define W1_WRITE_EEPROM 0x48
#define W1_WRITE_SCRATCHPAD 0x4E
#define W1_READ_SCRATCHPAD 0xBE

#ifndef DS18X20_MAX_SENSORS  // DS18X20_MAX_SENSORS fallback to 8 if not defined in user_config_override.h
#define DS18X20_MAX_SENSORS 8
#endif

#define DS18X20_ALIAS_LEN 17

const char kDs18x20Types[] PROGMEM = "DS18x20|DS18S20|DS1822|DS18B20|MAX31850";

const uint8_t ds18x20_chipids[] PROGMEM = {0, DS18S20_CHIPID, DS1822_CHIPID, DS18B20_CHIPID, MAX31850_CHIPID};

typedef struct {
  float temperature;
  float temp_sum;
  uint16_t numread;
  uint8_t address[8];
  uint8_t index;
  uint8_t valid;
  int8_t pins_id;
#ifdef DS18x20_USE_ID_ALIAS
  char alias[DS18X20_ALIAS_LEN];
#endif  // DS18x20_USE_ID_ALIAS
} DSS;

typedef struct {
  int8_t pin;          // Shelly GPIO3 input only
  int8_t pin_out;      // Shelly GPIO00 output only
  bool dual_mode;  // Single pin mode
} DSPINS; 

typedef struct {
#ifdef W1_PARASITE_POWER
  uint32_t w1_power_until;
  uint8_t current_sensor;
#endif
  char name[17];
  uint8_t sensors;
  uint8_t gpios;           // Count of GPIO found
  uint8_t input_mode;  // INPUT or INPUT_PULLUP (=2)
  int8_t pin;          // Shelly GPIO3 input only
  int8_t pin_out;      // Shelly GPIO00 output only
  bool dual_mode;  // Single pin mode
} DSX;

#ifdef ESP32
#define t_noInterrupts() {portMUX_TYPE mux; vTaskEnterCritical(&mux)
#define t_interrupts() vTaskExitCritical(&mux);}
#define DIRECT_WRITE_LOW(A) directWriteLow(A)
#define DIRECT_WRITE_HIGH(A) directWriteHigh(A)
#define DIRECT_MODE_OUTPUT(A) directModeOutput(A)
#define DIRECT_MODE_INPUT(A,B) directModeInput(A)
#define DIRECT_READ(A) directRead(A)
#else
#define t_noInterrupts noInterrupts
#define t_interrupts interrupts
#define DIRECT_WRITE_LOW(A) digitalWrite(A, LOW)
#define DIRECT_WRITE_HIGH(A) digitalWrite(A, HIGH)
#define DIRECT_MODE_OUTPUT(A) pinMode(A, OUTPUT)
#define DIRECT_MODE_INPUT(A,B) pinMode(A, B)
#define DIRECT_READ(A) digitalRead(A)
#endif

/*********************************************************************************************\
 * Embedded tuned OneWire library
\*********************************************************************************************/

#define W1_MATCH_ROM 0x55
#define W1_SEARCH_ROM 0xF0

#undef MAX_DSB
#define MAX_DSB 1

typedef struct {
uint8_t onewire_last_discrepancy;
uint8_t onewire_last_family_discrepancy;
bool onewire_last_device_flag;
uint8_t onewire_rom_id[8];
uint8_t delay_low[2];
uint8_t delay_high[2];
DSX DS18X20Data;
DSPINS ds18x20_gpios[MAX_DSB];
DSS ds18x20_sensor[DS18X20_MAX_SENSORS];
} MODULE_MEMORY;

#define onewire_last_discrepancy mem->onewire_last_discrepancy
#define onewire_last_family_discrepancy mem->onewire_last_family_discrepancy
#define onewire_last_device_flag mem->onewire_last_device_flag
#define onewire_rom_id mem->onewire_rom_id
#define DS18X20Data mem->DS18X20Data
#define ds18x20_gpios mem->ds18x20_gpios
#define ds18x20_sensor mem->ds18x20_sensor
#define delay_low mem->delay_low
#define delay_high mem->delay_high

/*------------------------------------------------------------------------------------------*/

/********************************************************************************************/
PUSH_OPTIONS
MODULE_DESCRIPTOR("DS18X20",MODULE_TYPE_SENSOR,1<<16|3,"DAT",16,"DM",0x01ff10ff,"",0,"",0)
MODULE_PART uint8_t OneWireReset(void);
MODULE_PART void OneWireWriteBit(uint8_t v);
MODULE_PART uint8_t OneWire1ReadBit(void);
MODULE_PART uint8_t OneWire2ReadBit(void);
MODULE_PART void OneWireWrite(uint8_t v);
MODULE_PART uint8_t OneWireRead(void);
MODULE_PART void OneWireSelect(const uint8_t rom[8]);
MODULE_PART uint8_t OneWireSearch(uint8_t *newAddr);
MODULE_PART bool OneWireCrc8(uint8_t *addr);
MODULE_PART int32_t Ds18x20Init(void);
MODULE_PART void Ds18x20Convert(void);
MODULE_PART bool Ds18x20Read(uint8_t sensor);
MODULE_PART void Ds18x20Name(uint8_t sensor);
MODULE_PART void Ds18x20EverySecond(void);
MODULE_PART void Ds18x20Show(bool json);
MODULE_PART void CmndDSAlias(void);
MODULE_PART void DS18X20_Deinit(void);
MODULE_PART int32_t mod_func_execute(uint32_t function);
MODULE_END
/********************************************************************************************/
uint8_t OneWireReset(void) {
SETREGS
uint8_t r;

  uint8_t retries = 125;

  if (!DS18X20Data.dual_mode) {
    t_noInterrupts();
    DIRECT_MODE_INPUT(DS18X20Data.pin, DS18X20Data.input_mode);
    do {
      if (--retries == 0) {
        return 0;
      }
      delayMicroseconds(2);
    } while (!DIRECT_READ(DS18X20Data.pin));
    DIRECT_MODE_OUTPUT(DS18X20Data.pin);
    DIRECT_WRITE_LOW(DS18X20Data.pin);
    delayMicroseconds(480);
    DIRECT_MODE_INPUT(DS18X20Data.pin, DS18X20Data.input_mode);
    delayMicroseconds(70);
    r = !DIRECT_READ(DS18X20Data.pin);
    delayMicroseconds(410);
    t_interrupts();
    return r;
  } else {
    t_noInterrupts();
    DIRECT_WRITE_HIGH(DS18X20Data.pin_out);
    do {
      if (--retries == 0) {
        return 0;
      }
      delayMicroseconds(2);
    } while (!DIRECT_READ(DS18X20Data.pin));
    DIRECT_WRITE_LOW(DS18X20Data.pin_out);
    delayMicroseconds(480);
    DIRECT_WRITE_HIGH(DS18X20Data.pin_out);
    delayMicroseconds(70);
    r = !DIRECT_READ(DS18X20Data.pin);
    delayMicroseconds(410);
    t_interrupts();
    return r;
  }
}

void OneWireWriteBit(uint8_t v) {
SETREGS

  t_noInterrupts();
  v &= 1;
  if (!DS18X20Data.dual_mode) {
    DIRECT_WRITE_LOW(DS18X20Data.pin);
    DIRECT_MODE_OUTPUT(DS18X20Data.pin);
    delayMicroseconds(delay_low[v]);
    DIRECT_WRITE_HIGH(DS18X20Data.pin);
  } else {
    DIRECT_WRITE_LOW(DS18X20Data.pin_out);
    delayMicroseconds(delay_low[v]);
    DIRECT_WRITE_HIGH(DS18X20Data.pin_out);
  }
  delayMicroseconds(delay_high[v]);
  t_interrupts();
}

uint8_t OneWire1ReadBit(void) {
SETREGS
uint8_t r;
  t_noInterrupts();
  DIRECT_MODE_OUTPUT(DS18X20Data.pin);
  DIRECT_WRITE_LOW(DS18X20Data.pin);
  delayMicroseconds(3);
  DIRECT_MODE_INPUT(DS18X20Data.pin, DS18X20Data.input_mode);
  delayMicroseconds(10);
  r = DIRECT_READ(DS18X20Data.pin);
  delayMicroseconds(53);
  t_interrupts();
  return r;
}

uint8_t OneWire2ReadBit(void) {
SETREGS
  uint8_t r;
  t_noInterrupts();
  DIRECT_WRITE_LOW(DS18X20Data.pin_out);
  delayMicroseconds(3);
  DIRECT_WRITE_HIGH(DS18X20Data.pin_out);
  delayMicroseconds(10);
  r = DIRECT_READ(DS18X20Data.pin);
  delayMicroseconds(53);
  t_interrupts();
  return r;
}

/*------------------------------------------------------------------------------------------*/

void OneWireWrite(uint8_t v) {
SETREGS

  t_noInterrupts();
  for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1) {
    OneWireWriteBit((bit_mask & v) ? 1 : 0);
  }
  t_interrupts();
}

uint8_t OneWireRead(void) {
SETREGS

  uint8_t r = 0;

  if (!DS18X20Data.dual_mode) {
    t_noInterrupts();
    for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1) {
      if (OneWire1ReadBit()) {
        r |= bit_mask;
      }
    }
    t_interrupts();
  } else {
    for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1) {
      if (OneWire2ReadBit()) {
        r |= bit_mask;
      }
    }
  }
  return r;
}

void OneWireSelect(const uint8_t rom[8]) {
SETREGS

  OneWireWrite(W1_MATCH_ROM);
  for (uint32_t i = 0; i < 8; i++) {
    OneWireWrite(rom[i]);
  }
}

uint8_t OneWireSearch(uint8_t *newAddr) {
SETREGS

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  uint8_t search_result = 0;
  uint8_t id_bit;
  uint8_t cmp_id_bit;
  uint8_t rom_byte_mask = 1;
  uint8_t search_direction;

  if (!onewire_last_device_flag) {
    if (!OneWireReset()) {
      onewire_last_discrepancy = 0;
      onewire_last_device_flag = false;
      onewire_last_family_discrepancy = 0;
      return false;
    }
    OneWireWrite(W1_SEARCH_ROM);
    do {
      if (!DS18X20Data.dual_mode) {
        id_bit = OneWire1ReadBit();
        cmp_id_bit = OneWire1ReadBit();
      } else {
        id_bit = OneWire2ReadBit();
        cmp_id_bit = OneWire2ReadBit();
      }
      if ((id_bit == 1) && (cmp_id_bit == 1)) {
        break;
      } else {
        if (id_bit != cmp_id_bit) {
          search_direction = id_bit;
        } else {
          if (id_bit_number < onewire_last_discrepancy) {
            search_direction = ((onewire_rom_id[rom_byte_number] & rom_byte_mask) > 0);
          } else {
            search_direction = (id_bit_number == onewire_last_discrepancy);
          }
          if (search_direction == 0) {
            last_zero = id_bit_number;
            if (last_zero < 9) {
              onewire_last_family_discrepancy = last_zero;
            }
          }
        }
        if (search_direction == 1) {
          onewire_rom_id[rom_byte_number] |= rom_byte_mask;
        } else {
          onewire_rom_id[rom_byte_number] &= ~rom_byte_mask;
        }
        OneWireWriteBit(search_direction);
        id_bit_number++;
        rom_byte_mask <<= 1;
        if (rom_byte_mask == 0) {
          rom_byte_number++;
          rom_byte_mask = 1;
        }
      }
    } while (rom_byte_number < 8);
    if (!(id_bit_number < 65)) {
      onewire_last_discrepancy = last_zero;
      if (onewire_last_discrepancy == 0) {
        onewire_last_device_flag = true;
      }
      search_result = true;
    }
  }
  if (!search_result || !onewire_rom_id[0]) {
    onewire_last_discrepancy = 0;
    onewire_last_device_flag = false;
    onewire_last_family_discrepancy = 0;
    search_result = false;
  }
  for (uint32_t i = 0; i < 8; i++) {
    newAddr[i] = onewire_rom_id[i];
  }
  return search_result;
}

bool OneWireCrc8(uint8_t *addr) {
SETREGS

  uint8_t crc = 0;
  uint8_t len = 8;

  while (len--) {
    uint8_t inbyte = *addr++;  // from 0 to 7
    for (uint32_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) {
        crc ^= 0x8C;
      }
      inbyte >>= 1;
    }
  }
  return (crc == *addr);  // addr 8
}

/********************************************************************************************/

int32_t Ds18x20Init(void) {
ALLOCMEM
  
  delay_low[0] = 65;
  delay_low[1] = 10;
  delay_high[0] = 5;
  delay_high[1] = 55;


  /*
  for (uint32_t pins = 0; pins < MAX_DSB; pins++) {

    if (PinUsed(GPIO_DSB, pins)) {
      ds18x20_gpios[pins].pin = Pin(GPIO_DSB, pins);

      if (PinUsed(GPIO_DSB_OUT, pins)) {
        ds18x20_gpios[pins].dual_mode = true;
        ds18x20_gpios[pins].pin_out = Pin(GPIO_DSB_OUT, pins);
      }
      DS18X20Data.gpios++;
    }
  }*/



  ds18x20_gpios[0].pin = mp->ms[0].value & 0xff;
  int8_t sel = (mp->ms[1].value & 0xff);
  if (sel < 0) {
    ds18x20_gpios[0].dual_mode = false;
  } else {
    ds18x20_gpios[0].dual_mode = false;
    ds18x20_gpios[0].pin_out = sel;
  }
  DS18X20Data.gpios = 1;

  // this needed by esp32 !!!! ???
  pinMode(ds18x20_gpios[0].pin, OUTPUT);

  uint64_t ids[DS18X20_MAX_SENSORS];
  DS18X20Data.sensors = 0;
  DS18X20Data.input_mode = Settings->flag3.ds18x20_internal_pullup ? INPUT_PULLUP : INPUT;   
 // SetOption74 - Enable internal pullup for single DS18x20 sensor

  for (uint32_t pins = 0; pins < DS18X20Data.gpios; pins++) {
    DS18X20Data.pin = ds18x20_gpios[pins].pin;
    DS18X20Data.dual_mode = ds18x20_gpios[pins].dual_mode;
    if (ds18x20_gpios[pins].dual_mode) {
      DS18X20Data.pin_out = ds18x20_gpios[pins].pin_out;
      DIRECT_MODE_OUTPUT(DS18X20Data.pin_out);
      DIRECT_MODE_INPUT(DS18X20Data.pin, DS18X20Data.input_mode);
    }

    onewire_last_discrepancy = 0;
    onewire_last_device_flag = false;
    onewire_last_family_discrepancy = 0;
    for (uint32_t i = 0; i < 8; i++) {
      onewire_rom_id[i] = 0;
    }

    while (DS18X20Data.sensors < DS18X20_MAX_SENSORS) {
      if (!OneWireSearch(ds18x20_sensor[DS18X20Data.sensors].address)) {
        break;
      }
      if (OneWireCrc8(ds18x20_sensor[DS18X20Data.sensors].address) &&
          ((ds18x20_sensor[DS18X20Data.sensors].address[0] == DS18S20_CHIPID) ||
           (ds18x20_sensor[DS18X20Data.sensors].address[0] == DS1822_CHIPID) ||
           (ds18x20_sensor[DS18X20Data.sensors].address[0] == DS18B20_CHIPID) ||
           (ds18x20_sensor[DS18X20Data.sensors].address[0] == MAX31850_CHIPID))) {
        ds18x20_sensor[DS18X20Data.sensors].index = DS18X20Data.sensors;
        ids[DS18X20Data.sensors] = ds18x20_sensor[DS18X20Data.sensors].address[0];  // Chip id
        for (uint32_t j = 6; j > 0; j--) {
          ids[DS18X20Data.sensors] = ids[DS18X20Data.sensors] << 8 | ds18x20_sensor[DS18X20Data.sensors].address[j];
        }
#ifdef DS18x20_USE_ID_ALIAS
        ds18x20_sensor[DS18X20Data.sensors].alias[0] = '0';
#endif
        ds18x20_sensor[DS18X20Data.sensors].pins_id = pins;
        DS18X20Data.sensors++;
      }
    }
  }

  for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
    for (uint32_t j = i + 1; j < DS18X20Data.sensors; j++) {
      if (ids[ds18x20_sensor[i].index] > ids[ds18x20_sensor[j].index]) {  // Sort ascending
        std::swap(ds18x20_sensor[i].index, ds18x20_sensor[j].index);
      }
    }
  }

  AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_DSB D_SENSORS_FOUND " %d"), DS18X20Data.sensors);

  if (!DS18X20Data.sensors) {
    DS18X20_Deinit();
    return false;
  } else {
    initialized = true;
    return true;
  }

}

void Ds18x20Convert(void) {
SETREGS

  for (uint8_t i = 0; i < DS18X20Data.gpios; i++) {
    DS18X20Data.pin = ds18x20_gpios[i].pin;
    DS18X20Data.dual_mode = ds18x20_gpios[i].dual_mode;
    DS18X20Data.pin_out = ds18x20_gpios[i].pin_out;
    OneWireReset();
#ifdef W1_PARASITE_POWER
    // With parasite power address one sensor at a time
    if (++DS18X20Data.current_sensor >= DS18X20Data.sensors) DS18X20Data.current_sensor = 0;
    OneWireSelect(ds18x20_sensor[DS18X20Data.current_sensor].address);
#else
    OneWireWrite(W1_SKIP_ROM);  // Address all Sensors on Bus
#endif
    OneWireWrite(W1_CONVERT_TEMP);  // start conversion, no parasite power on at the end
                                    //    delay(750);                          // 750ms should be enough for 12bit conv
  }
}

bool Ds18x20Read(uint8_t sensor) {
SETREGS

  float temperature;
  uint8_t data[9];
  int8_t sign = 1;

  uint8_t index = ds18x20_sensor[sensor].index;
  DS18X20Data.pin = ds18x20_gpios[ds18x20_sensor[index].pins_id].pin;
  DS18X20Data.pin_out = ds18x20_gpios[ds18x20_sensor[index].pins_id].pin_out;
  DS18X20Data.dual_mode = ds18x20_gpios[ds18x20_sensor[index].pins_id].dual_mode;
  if (ds18x20_sensor[index].valid) {
    ds18x20_sensor[index].valid--;
  }
  for (uint32_t retry = 0; retry < 3; retry++) {
    OneWireReset();
    OneWireSelect(ds18x20_sensor[index].address);
    OneWireWrite(W1_READ_SCRATCHPAD);
    for (uint32_t i = 0; i < 9; i++) {
      data[i] = OneWireRead();
    }
    if (OneWireCrc8(data)) {
      switch (ds18x20_sensor[index].address[0]) {
        case DS18S20_CHIPID: {
          int16_t tempS = (((data[1] << 8) | (data[0] & 0xFE)) << 3) | ((0x10 - data[6]) & 0x0F);
          //temperature = ConvertTemp(tempS * 0.0625f - 0.250f);
          float t = jfmul( jtofloat(tempS), 0.0625f);
          t = jfdiff(t, 0.250f);
          t = ConvertTemp(t);
          break;
        }
        case DS1822_CHIPID:
        case DS18B20_CHIPID: {
          if (data[4] != 0x7F) {
            data[4] = 0x7F;  // Set resolution to 12-bit
            OneWireReset();
            OneWireSelect(ds18x20_sensor[index].address);
            OneWireWrite(W1_WRITE_SCRATCHPAD);
            OneWireWrite(data[2]);  // Th Register
            OneWireWrite(data[3]);  // Tl Register
            OneWireWrite(data[4]);  // Configuration Register
            OneWireSelect(ds18x20_sensor[index].address);
            OneWireWrite(W1_WRITE_EEPROM);  // Save scratchpad to EEPROM
#ifdef W1_PARASITE_POWER
            DS18X20Data.w1_power_until = millis() + 10;  // 10ms specified duration for EEPROM write
#endif
          }
          uint16_t temp12 = (data[1] << 8) + data[0];
          if (temp12 > 2047) {
            temp12 = (~temp12) + 1;
            sign = -1;
          }
          //temperature = ConvertTemp(sign * temp12 * 0.0625f);  // Divide by 16
          float t = jfmul( jtofloat(sign * temp12), 0.0625f);
          temperature = ConvertTemp(t);
          break;
        }
        case MAX31850_CHIPID: {
          int16_t temp14 = (data[1] << 8) + (data[0] & 0xFC);
          //temperature = ConvertTemp(temp14 * 0.0625f);  // Divide by 16
          float t = jfmul( jtofloat(temp14), 0.0625f);
          temperature = ConvertTemp(t);
          break;
        }
      }
      ds18x20_sensor[index].temperature = temperature;
      if (Settings->flag5.ds18x20_mean) {
        if (ds18x20_sensor[index].numread++ == 0) {
          ds18x20_sensor[index].temp_sum = 0;
        }
        //ds18x20_sensor[index].temp_sum += temperature;
        ds18x20_sensor[index].temp_sum = jfadd(ds18x20_sensor[index].temp_sum , temperature);
      }
      ds18x20_sensor[index].valid = SENSOR_MAX_MISS;
      return true;
    }
  }
  AddLog(LOG_LEVEL_DEBUG, PSTR(D_LOG_DSB D_SENSOR_CRC_ERROR));
  return false;
}

void Ds18x20Name(uint8_t sensor) {
SETREGS

  uint32_t sensor_index = ds18x20_sensor[sensor].index;

  uint32_t index = sizeof(ds18x20_chipids);
  uint8_t ids[index];
  memmove_P(ids, GSTR(ds18x20_chipids), sizeof(ids));
  
  while (--index) {
    if (ds18x20_sensor[sensor_index].address[0] == ids[index]) {
      break;
    }
  }
  // DS18B20
  GetTextIndexed(DS18X20Data.name, sizeof(DS18X20Data.name), index, GSTR(kDs18x20Types));

#ifdef DS18x20_USE_ID_AS_NAME
  char address[17];
  for (uint32_t j = 0; j < 3; j++) {
    sprintf_P(address + 2 * j, PSTR("%02X"), ds18x20_sensor[sensor_index].address[3 - j]);  // Only last 3 bytes
  }
  // DS18B20-8EC44C
  snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name), PSTR("%s%c%s"), DS18X20Data.name, IndexSeparator(), address);
  return;
#elif defined(DS18x20_USE_ID_ALIAS)
  if (ds18x20_sensor[sensor_index].alias[0] && (ds18x20_sensor[sensor_index].alias[0] != '0')) {
    if (isdigit(ds18x20_sensor[sensor_index].alias[0])) {
      // DS18Sens-1
      snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name), PSTR("DS18Sens%c%d"), IndexSeparator(), atoi(ds18x20_sensor[sensor_index].alias));
    } else {
      // UserText
      snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name), PSTR("%s"), ds18x20_sensor[sensor_index].alias);
    }
    return;
  }
#endif  // DS18x20_USE_ID_AS_NAME or DS18x20_USE_ID_ALIAS

  if (DS18X20Data.sensors > 1) {
    // DS18B20-1
    snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name), PSTR("%s%c%d"), DS18X20Data.name, IndexSeparator(), sensor + 1);
  }
}

/********************************************************************************************/

void Ds18x20EverySecond(void) {
SETREGS
STGLOB
  if (!DS18X20Data.sensors) {
    return;
  }

#ifdef W1_PARASITE_POWER
  // skip access if there is still an eeprom write ongoing
  unsigned long now = millis();
  if (now < DS18X20Data.w1_power_until) {
    return;
  }
#endif

  //ds18x20_sensor[0].valid = 1;
  //return;

  if (TasmotaGlobal->uptime & 1
#ifdef W1_PARASITE_POWER
      // if more than 1 sensor and only parasite power: convert every cycle
      || DS18X20Data.sensors >= 2
#endif
  ) {
    // 2mS
    Ds18x20Convert();  // Start conversion, takes up to one second
  } else {
    for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
      // 12mS per device
      if (!Ds18x20Read(i)) {  // Read temperature
        Ds18x20Name(i);
        AddLogMissed(DS18X20Data.name, ds18x20_sensor[ds18x20_sensor[i].index].valid);
#ifdef USE_DS18x20_RECONFIGURE
        if (!ds18x20_sensor[ds18x20_sensor[i].index].valid) {
          memset(&ds18x20_sensor, 0, sizeof(ds18x20_sensor));
          Ds18x20Init();  // Re-configure
        }
#endif  // USE_DS18x20_RECONFIGURE
      }
    }
  }
}

void Ds18x20Show(bool json) {
SETREGS
STGLOB
  for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
    uint8_t index = ds18x20_sensor[i].index;

    if (ds18x20_sensor[index].valid) {  // Check for valid temperature
      Ds18x20Name(i);

      if (json) {
        
        if (Settings->flag5.ds18x20_mean) {
          if ((0 == TasmotaGlobal->tele_period) && ds18x20_sensor[index].numread) {
            //ds18x20_sensor[index].temperature = ds18x20_sensor[index].temp_sum / ds18x20_sensor[index].numread;
            ds18x20_sensor[index].temperature = jfdiv(ds18x20_sensor[index].temp_sum , jtofloat(ds18x20_sensor[index].numread));
            ds18x20_sensor[index].numread = 0;
          }
        }
        char address[17];
        address[0] = 0;
        for (uint32_t j = 0; j < 6; j++) {
          sprintf_P(address + 2 * j, PSTR("%02X"), ds18x20_sensor[index].address[6 - j]);  // Skip sensor type and crc
        }
        char tstr[10];
        ftostrfd(ds18x20_sensor[index].temperature, Settings->flag2.temperature_resolution, tstr);
        //this only works on esp8266:  ResponseAppend_P(PSTR(",\"%s\":{\"" D_JSON_ID "\":\"%s\",\"" D_JSON_TEMPERATURE "\":%*_f}"), DS18X20Data.name, address, Settings->flag2.temperature_resolution, &ds18x20_sensor[index].temperature);
        ResponseAppend_P(PSTR(",\"%s\":{\"" D_JSON_ID "\":\"%s\",\"" D_JSON_TEMPERATURE "\":%s}"), DS18X20Data.name, address, tstr);

 //= {"Time":"2024-03-15T16:36:15","DS18B20_1":{"Id":"1ABEE9086461","Temperature":16.6},"DS18B20_2":{"Id":"C178441F64FF","Temperature":35.4},"EBUS":{"Collector":55.5,"Solarstorage":50.4,"Solarpump":0},"TempUnit":"C"}



#ifdef USE_DOMOTICZ
        if ((0 == TasmotaGlobal->tele_period) && (0 == i)) {
          DomoticzFloatSensor(DZ_TEMP, ds18x20_sensor[index].temperature);
        }
#endif  // USE_DOMOTICZ
#ifdef USE_KNX
        if ((0 == TasmotaGlobal->tele_period) && (0 == i)) {
          KnxSensor(KNX_TEMPERATURE, ds18x20_sensor[index].temperature);
        }
#endif  // USE_KNX

#ifdef USE_WEBSERVER
      } else {
        WSContentSend_Temp(DS18X20Data.name, ds18x20_sensor[index].temperature);
#endif  // USE_WEBSERVER
      }
    }
  }
}

#ifdef DS18x20_USE_ID_ALIAS
const char kds18Commands[] PROGMEM = "DS18|"  // prefix
    D_CMND_DS_ALIAS;

void (*const DSCommand[])(void) PROGMEM = {&CmndDSAlias};

void CmndDSAlias(void) {
SETREGS

// Ds18Alias  6AB99A451F64FF28  ,  Garten

  // Ds18Alias 430516707FA6FF28,SensorName - Use SensorName instead of DS18B20
  // Ds18Alias 430516707FA6FF28,0          - Disable alias (default)
  char *Argument1;
  char *Argument2;
  char address[17];

  char *cp = strchr(XdrvMailbox->data, ',');
  if (cp) {
    *cp = 0;
    Argument1 = XdrvMailbox->data;
    Argument2 = cp + 1;
    Argument1 = trimm(Argument1);
    Argument2 = trimm(Argument2);
    for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
      for (uint32_t j = 0; j < 8; j++) {
        sprintf_P(address + 2 * j, PSTR("%02X"), ds18x20_sensor[i].address[7 - j]);
      }
      if (!strncmp(Argument1, address, 12) && Argument2[0]) {
        snprintf_P(ds18x20_sensor[i].alias, DS18X20_ALIAS_LEN, PSTR("%s"), Argument2);
        break;
      }
    }
  }
  Response_P(PSTR("{"));
  for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
    Ds18x20Name(i);
    char address[17];
    for (uint32_t j = 0; j < 8; j++) {
      sprintf_P(address + 2 * j, PSTR("%02X"), ds18x20_sensor[ds18x20_sensor[i].index].address[7 - j]);  // Skip sensor type and crc
    }
    ResponseAppend_P(PSTR("\"%s\":{\"" D_JSON_ID "\":\"%s\"}"), DS18X20Data.name, address);
    if (i < DS18X20Data.sensors - 1) {
      ResponseAppend_P(PSTR(","));
    }

  }
  ResponseAppend_P(PSTR("}"));
}
#endif  // DS18x20_USE_ID_ALIAS


void DS18X20_Deinit(void) {
SETREGS
RETMEM
}
/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t function) {
  bool result = false;
  switch (function) {
      case FUNC_INIT:
        result = Ds18x20Init();
        break;
      case FUNC_EVERY_SECOND:
        Ds18x20EverySecond();
        break;
      case FUNC_JSON_APPEND:
        Ds18x20Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        Ds18x20Show(0);
        break;
#endif  // USE_WEBSERVER
#ifdef DS18x20_USE_ID_ALIAS
      case FUNC_COMMAND:
        {
          SETREGS
          result = DecodeCommand(kds18Commands, DSCommand);
        }
        break;
#endif  // DS18x20_USE_ID_ALIAS
			case FUNC_DEINIT:
				DS18X20_Deinit();
				break;

  }
  return result;
}


PULL_OPTIONS
#endif  // USE_DS18x20
