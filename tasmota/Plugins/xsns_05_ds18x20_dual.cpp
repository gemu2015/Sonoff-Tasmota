/*
  xsns_05_ds18x20_dual.cpp — Maxim DS18B20 / DS18S20 / DS1822 /
  MAX31850 1-Wire temperature sensor driver, dual-format.

  Original copyright preserved:
    Copyright (C) 2021 Theo Arends, md5sum-as

  Plugin: USE_DS18X20_DUAL_MOD via build_plugin.py.
  Native: USE_DS18X20_DUAL via the shim at
          tasmota/tasmota_xsns_sensor/xsns_05_ds18x20_dual.ino.

  First non-I2C, non-serial dual driver — uses bit-banged 1-Wire on
  a single GPIO with optional split DAT-out / DAT-in pins (dual-pin
  mode for level-shifter setups, e.g. some Shelly devices).

  Self-contained OneWire impl (the legacy plugin source already
  carried its own bit-bang library). Native fast-GPIO comes from
  the static-inline `directRead/directWriteLow/directWriteHigh/
  directModeInput/directModeOutput` helpers in dual_format_compat.h
  (mirroring TasmotaOneWire-2.3.3).

  Up to DS18X20_MAX_SENSORS (default 8) sensors per pin. Optional
  features (compile-time):
    USE_DS18x20_RECONFIGURE  — if a sensor disappears, re-init the bus.
    DS18x20_USE_ID_AS_NAME   — name sensors by last 3 ROM bytes.
    DS18x20_USE_ID_ALIAS     — `DS18Alias <id>,<name>` console command
                               for stable user-friendly names.
    W1_PARASITE_POWER        — single-shot conversions for parasite-power
                               wiring (one sensor active per cycle).
*/

#include "tasmota_options.h"

#ifndef BUILD_AS_PLUGIN
#  ifdef USE_DS18X20_DUAL_MOD
#    define BUILD_AS_PLUGIN 1
#  else
#    define BUILD_AS_PLUGIN 0
#  endif
#endif

#include "dual_format_compat.h"

#if BUILD_AS_PLUGIN
#  include "../Tasmota/include/i18n.h"
#endif

#if BUILD_AS_PLUGIN
#  ifdef USE_DS18X20_DUAL_MOD
#    define _DS18X20_DUAL_ENABLED 1
#  endif
#else
#  if defined(USE_DS18X20_DUAL) && defined(DS18X20_DUAL_NATIVE_INCLUDE)
#    define _DS18X20_DUAL_ENABLED 1
#  endif
#endif

#ifdef _DS18X20_DUAL_ENABLED

// --------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------
#define DS18S20_CHIPID    0x10   // ±0.5 °C  9-bit
#define DS1822_CHIPID     0x22   // ±2   °C 12-bit
#define DS18B20_CHIPID    0x28   // ±0.5 °C 12-bit
#define MAX31850_CHIPID   0x3B   // ±0.25°C 14-bit (thermocouple)

#define W1_SKIP_ROM       0xCC
#define W1_CONVERT_TEMP   0x44
#define W1_WRITE_EEPROM   0x48
#define W1_WRITE_SCRATCHPAD 0x4E
#define W1_READ_SCRATCHPAD  0xBE
#define W1_MATCH_ROM      0x55
#define W1_SEARCH_ROM     0xF0

#ifndef DS18X20_MAX_SENSORS
#  define DS18X20_MAX_SENSORS 8
#endif
#define DS18X20_ALIAS_LEN 17
#define DS18X20_DEFAULT_DAT 16    // GPIO 16 fallback if template unmapped

#ifndef SENSOR_MAX_MISS
#  define SENSOR_MAX_MISS 5
#endif

// Critical-section macros — ESP32 uses portMUX_TYPE; ESP8266 uses
// the global noInterrupts/interrupts pair. Bit-bang 1-Wire timing
// is microsecond-precise and intolerant of interrupt jitter.
#ifdef ESP32
#  define t_noInterrupts() { portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; vTaskEnterCritical(&mux)
#  define t_interrupts()   vTaskExitCritical(&mux); }
#  define DIRECT_WRITE_LOW(A)    directWriteLow(A)
#  define DIRECT_WRITE_HIGH(A)   directWriteHigh(A)
#  define DIRECT_MODE_OUTPUT(A)  directModeOutput(A)
#  define DIRECT_MODE_INPUT(A,B) directModeInput(A)
#  define DIRECT_READ(A)         directRead(A)
#else
#  define t_noInterrupts         noInterrupts
#  define t_interrupts           interrupts
#  define DIRECT_WRITE_LOW(A)    digitalWrite(A, LOW)
#  define DIRECT_WRITE_HIGH(A)   digitalWrite(A, HIGH)
#  define DIRECT_MODE_OUTPUT(A)  pinMode(A, OUTPUT)
#  define DIRECT_MODE_INPUT(A,B) pinMode(A, B)
#  define DIRECT_READ(A)         digitalRead(A)
#endif

#define MAX_DSB 1   // single 1-Wire bus per driver instance

// --------------------------------------------------------------------
// Per-driver type definitions
// --------------------------------------------------------------------
typedef struct {
  float   temperature;
  float   temp_sum;
  uint16_t numread;
  uint8_t address[8];
  uint8_t index;
  uint8_t valid;
  int8_t  pins_id;
#ifdef DS18x20_USE_ID_ALIAS
  char    alias[DS18X20_ALIAS_LEN];
#endif
} DSS;

typedef struct {
  int8_t pin;
  int8_t pin_out;     // single-pin (default) → ignored. dual-pin → DAT out
  bool   dual_mode;
} DSPINS;

typedef struct {
#ifdef W1_PARASITE_POWER
  uint32_t w1_power_until;
  uint8_t  current_sensor;
#endif
  char     name[17];
  uint8_t  sensors;
  uint8_t  gpios;
  uint8_t  input_mode;     // INPUT or INPUT_PULLUP (SetOption74)
  int8_t   pin;
  int8_t   pin_out;
  bool     dual_mode;
} DSX;

const char kDs18x20Types[] PROGMEM = "DS18x20|DS18S20|DS1822|DS18B20|MAX31850";
const uint8_t ds18x20_chipids[] PROGMEM = {
  0, DS18S20_CHIPID, DS1822_CHIPID, DS18B20_CHIPID, MAX31850_CHIPID
};

// --------------------------------------------------------------------
// State storage — heap in both modes
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

typedef struct {
  uint8_t onewire_last_discrepancy;
  uint8_t onewire_last_family_discrepancy;
  bool    onewire_last_device_flag;
  uint8_t onewire_rom_id[8];
  uint8_t delay_low[2];
  uint8_t delay_high[2];
  DSX     DS18X20Data;
  DSPINS  ds18x20_gpios[MAX_DSB];
  DSS     ds18x20_sensor[DS18X20_MAX_SENSORS];
} MODULE_MEMORY;

#  define onewire_last_discrepancy        mem->onewire_last_discrepancy
#  define onewire_last_family_discrepancy mem->onewire_last_family_discrepancy
#  define onewire_last_device_flag        mem->onewire_last_device_flag
#  define onewire_rom_id                  mem->onewire_rom_id
#  define DS18X20Data                     mem->DS18X20Data
#  define ds18x20_gpios                   mem->ds18x20_gpios
#  define ds18x20_sensor                  mem->ds18x20_sensor
#  define delay_low                       mem->delay_low
#  define delay_high                      mem->delay_high

#else  // native

typedef struct {
  uint8_t onewire_last_discrepancy;
  uint8_t onewire_last_family_discrepancy;
  bool    onewire_last_device_flag;
  uint8_t onewire_rom_id[8];
  uint8_t delay_low[2];
  uint8_t delay_high[2];
  DSX     DS18X20Data;
  DSPINS  ds18x20_gpios[MAX_DSB];
  DSS     ds18x20_sensor[DS18X20_MAX_SENSORS];
  bool    initialized_flag;
} ds18x20_state_t;

static ds18x20_state_t *ds18x20_state = nullptr;

#  define onewire_last_discrepancy        ds18x20_state->onewire_last_discrepancy
#  define onewire_last_family_discrepancy ds18x20_state->onewire_last_family_discrepancy
#  define onewire_last_device_flag        ds18x20_state->onewire_last_device_flag
#  define onewire_rom_id                  ds18x20_state->onewire_rom_id
#  define DS18X20Data                     ds18x20_state->DS18X20Data
#  define ds18x20_gpios                   ds18x20_state->ds18x20_gpios
#  define ds18x20_sensor                  ds18x20_state->ds18x20_sensor
#  define delay_low                       ds18x20_state->delay_low
#  define delay_high                      ds18x20_state->delay_high
#  define initialized                     ds18x20_state->initialized_flag

#  define ALLOCMEM                        DUAL_ALLOCMEM(ds18x20)
#  define RETMEM                          DUAL_RETMEM(ds18x20)

#  define XSNS_05                         5

#endif  // BUILD_AS_PLUGIN

// XdrvMailbox accessor (pointer in plugin, instance in native)
#if BUILD_AS_PLUGIN
#  define _DS_MB_DATA  (XdrvMailbox->data)
#else
#  define _DS_MB_DATA  (XdrvMailbox.data)
#endif

// --------------------------------------------------------------------
// Plugin descriptor block (canonical layout — empty in native)
// --------------------------------------------------------------------
PUSH_OPTIONS
MODULE_DESCRIPTOR("DS18X20", MODULE_TYPE_SENSOR, 1 << 16 | 5,
                  "DAT", DS18X20_DEFAULT_DAT,
                  "DM",  0x01ff10ff,
                  "", 0, "", 0)
MODULE_PART uint8_t  OneWireReset(void);
MODULE_PART void     OneWireWriteBit(uint8_t v);
MODULE_PART uint8_t  OneWire1ReadBit(void);
MODULE_PART uint8_t  OneWire2ReadBit(void);
MODULE_PART void     OneWireWrite(uint8_t v);
MODULE_PART uint8_t  OneWireRead(void);
MODULE_PART void     OneWireSelect(const uint8_t rom[8]);
MODULE_PART uint8_t  OneWireSearch(uint8_t *newAddr);
MODULE_PART bool     OneWireCrc8(uint8_t *addr);
MODULE_PART int32_t  Ds18x20Init(void);
MODULE_PART void     Ds18x20Convert(void);
MODULE_PART bool     Ds18x20Read(uint8_t sensor);
MODULE_PART void     Ds18x20Name(uint8_t sensor);
MODULE_PART void     Ds18x20EverySecond(void);
MODULE_PART void     Ds18x20Show(bool json);
#ifdef DS18x20_USE_ID_ALIAS
MODULE_PART void     CmndDSAlias(void);
#endif
MODULE_PART void     DS18X20_Deinit(void);
#if BUILD_AS_PLUGIN
MODULE_PART int32_t  mod_func_execute(uint32_t function);
#endif
MODULE_END

// --------------------------------------------------------------------
// Embedded tuned OneWire library — bit-banged on the configured pin.
// Byte-for-byte port of the legacy plugin, except SETREGS is now
// dual-mode safe (it brings the heap state's `mem` local into scope
// in plugin, and the native `mem`-equivalent through the macros).
// --------------------------------------------------------------------

uint8_t OneWireReset(void) {
  SETREGS
  uint8_t r;
  uint8_t retries = 125;

  if (!DS18X20Data.dual_mode) {
    t_noInterrupts();
    DIRECT_MODE_INPUT(DS18X20Data.pin, DS18X20Data.input_mode);
    do {
      if (--retries == 0) { return 0; }
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
      if (--retries == 0) { return 0; }
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
      if (OneWire1ReadBit()) { r |= bit_mask; }
    }
    t_interrupts();
  } else {
    for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1) {
      if (OneWire2ReadBit()) { r |= bit_mask; }
    }
  }
  return r;
}

void OneWireSelect(const uint8_t rom[8]) {
  SETREGS
  OneWireWrite(W1_MATCH_ROM);
  for (uint32_t i = 0; i < 8; i++) { OneWireWrite(rom[i]); }
}

uint8_t OneWireSearch(uint8_t *newAddr) {
  SETREGS
  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  uint8_t search_result = 0;
  uint8_t id_bit, cmp_id_bit;
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
        id_bit     = OneWire1ReadBit();
        cmp_id_bit = OneWire1ReadBit();
      } else {
        id_bit     = OneWire2ReadBit();
        cmp_id_bit = OneWire2ReadBit();
      }
      if ((id_bit == 1) && (cmp_id_bit == 1)) { break; }

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
          if (last_zero < 9) { onewire_last_family_discrepancy = last_zero; }
        }
      }
      if (search_direction == 1) { onewire_rom_id[rom_byte_number] |=  rom_byte_mask; }
      else                       { onewire_rom_id[rom_byte_number] &= ~rom_byte_mask; }
      OneWireWriteBit(search_direction);
      id_bit_number++;
      rom_byte_mask <<= 1;
      if (rom_byte_mask == 0) {
        rom_byte_number++;
        rom_byte_mask = 1;
      }
    } while (rom_byte_number < 8);

    if (!(id_bit_number < 65)) {
      onewire_last_discrepancy = last_zero;
      if (onewire_last_discrepancy == 0) { onewire_last_device_flag = true; }
      search_result = true;
    }
  }
  if (!search_result || !onewire_rom_id[0]) {
    onewire_last_discrepancy = 0;
    onewire_last_device_flag = false;
    onewire_last_family_discrepancy = 0;
    search_result = false;
  }
  for (uint32_t i = 0; i < 8; i++) { newAddr[i] = onewire_rom_id[i]; }
  return search_result;
}

bool OneWireCrc8(uint8_t *addr) {
  SETREGS
  uint8_t crc = 0;
  uint8_t len = 8;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint32_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) { crc ^= 0x8C; }
      inbyte >>= 1;
    }
  }
  return (crc == *addr);
}

// --------------------------------------------------------------------
// Driver hooks
// --------------------------------------------------------------------
int32_t Ds18x20Init(void) {
  ALLOCMEM

  delay_low[0]  = 65;
  delay_low[1]  = 10;
  delay_high[0] = 5;
  delay_high[1] = 55;

#if BUILD_AS_PLUGIN
  ds18x20_gpios[0].pin = (int8_t)(mp->ms[0].value & 0xff);
  int8_t sel = (int8_t)(mp->ms[1].value & 0xff);
  ds18x20_gpios[0].dual_mode = false;
  if (sel >= 0) { ds18x20_gpios[0].pin_out = sel; }
#else
  // Native: prefer Tasmota template Pin(GPIO_DSB), fall back to default.
#  ifdef GPIO_DSB
  int8_t gp = Pin(GPIO_DSB);
  ds18x20_gpios[0].pin = (gp >= 0) ? gp : (int8_t)DS18X20_DEFAULT_DAT;
#  else
  ds18x20_gpios[0].pin = (int8_t)DS18X20_DEFAULT_DAT;
#  endif
  ds18x20_gpios[0].dual_mode = false;
#  ifdef GPIO_DSB_OUT
  int8_t go = Pin(GPIO_DSB_OUT);
  if (go >= 0) {
    ds18x20_gpios[0].pin_out  = go;
    ds18x20_gpios[0].dual_mode = true;
  }
#  endif
#endif
  DS18X20Data.gpios = 1;

  // ESP32 needs an initial OUTPUT mode to seed the GPIO matrix
  pinMode(ds18x20_gpios[0].pin, OUTPUT);

  uint64_t ids[DS18X20_MAX_SENSORS];
  DS18X20Data.sensors    = 0;
  DS18X20Data.input_mode = Settings->flag3.ds18x20_internal_pullup ? INPUT_PULLUP : INPUT;

  for (uint32_t pins = 0; pins < DS18X20Data.gpios; pins++) {
    DS18X20Data.pin       = ds18x20_gpios[pins].pin;
    DS18X20Data.dual_mode = ds18x20_gpios[pins].dual_mode;
    if (ds18x20_gpios[pins].dual_mode) {
      DS18X20Data.pin_out = ds18x20_gpios[pins].pin_out;
      DIRECT_MODE_OUTPUT(DS18X20Data.pin_out);
      DIRECT_MODE_INPUT(DS18X20Data.pin, DS18X20Data.input_mode);
    }

    onewire_last_discrepancy        = 0;
    onewire_last_device_flag        = false;
    onewire_last_family_discrepancy = 0;
    for (uint32_t i = 0; i < 8; i++) { onewire_rom_id[i] = 0; }

    while (DS18X20Data.sensors < DS18X20_MAX_SENSORS) {
      if (!OneWireSearch(ds18x20_sensor[DS18X20Data.sensors].address)) { break; }
      uint8_t fam = ds18x20_sensor[DS18X20Data.sensors].address[0];
      if (OneWireCrc8(ds18x20_sensor[DS18X20Data.sensors].address)
          && (fam == DS18S20_CHIPID || fam == DS1822_CHIPID
           || fam == DS18B20_CHIPID || fam == MAX31850_CHIPID)) {
        ds18x20_sensor[DS18X20Data.sensors].index = DS18X20Data.sensors;
        ids[DS18X20Data.sensors] = fam;
        for (uint32_t j = 6; j > 0; j--) {
          ids[DS18X20Data.sensors] = (ids[DS18X20Data.sensors] << 8)
                                     | ds18x20_sensor[DS18X20Data.sensors].address[j];
        }
#ifdef DS18x20_USE_ID_ALIAS
        ds18x20_sensor[DS18X20Data.sensors].alias[0] = '0';
#endif
        ds18x20_sensor[DS18X20Data.sensors].pins_id = pins;
        DS18X20Data.sensors++;
      }
    }
  }

  // Sort sensors ascending by ROM ID for stable enumeration.
  for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
    for (uint32_t j = i + 1; j < DS18X20Data.sensors; j++) {
      if (ids[ds18x20_sensor[i].index] > ids[ds18x20_sensor[j].index]) {
        std::swap(ds18x20_sensor[i].index, ds18x20_sensor[j].index);
      }
    }
  }

  AddLog(LOG_LEVEL_INFO, PSTR(D_LOG_DSB D_SENSORS_FOUND " %d"), DS18X20Data.sensors);

  if (!DS18X20Data.sensors) {
    DS18X20_Deinit();
    return false;
  }
  initialized = true;
  return true;
}

void Ds18x20Convert(void) {
  SETREGS
  for (uint8_t i = 0; i < DS18X20Data.gpios; i++) {
    DS18X20Data.pin       = ds18x20_gpios[i].pin;
    DS18X20Data.dual_mode = ds18x20_gpios[i].dual_mode;
    DS18X20Data.pin_out   = ds18x20_gpios[i].pin_out;
    OneWireReset();
#ifdef W1_PARASITE_POWER
    if (++DS18X20Data.current_sensor >= DS18X20Data.sensors) {
      DS18X20Data.current_sensor = 0;
    }
    OneWireSelect(ds18x20_sensor[DS18X20Data.current_sensor].address);
#else
    OneWireWrite(W1_SKIP_ROM);     // address all sensors at once
#endif
    OneWireWrite(W1_CONVERT_TEMP);
  }
}

bool Ds18x20Read(uint8_t sensor) {
  SETREGS
  float    temperature = 0.0f;
  uint8_t  data[9];
  int8_t   sign = 1;

  uint8_t index = ds18x20_sensor[sensor].index;
  DS18X20Data.pin       = ds18x20_gpios[ds18x20_sensor[index].pins_id].pin;
  DS18X20Data.pin_out   = ds18x20_gpios[ds18x20_sensor[index].pins_id].pin_out;
  DS18X20Data.dual_mode = ds18x20_gpios[ds18x20_sensor[index].pins_id].dual_mode;
  if (ds18x20_sensor[index].valid) { ds18x20_sensor[index].valid--; }

  for (uint32_t retry = 0; retry < 3; retry++) {
    OneWireReset();
    OneWireSelect(ds18x20_sensor[index].address);
    OneWireWrite(W1_READ_SCRATCHPAD);
    for (uint32_t i = 0; i < 9; i++) { data[i] = OneWireRead(); }

    if (OneWireCrc8(data)) {
      switch (ds18x20_sensor[index].address[0]) {
        case DS18S20_CHIPID: {
          int16_t tempS = (((data[1] << 8) | (data[0] & 0xFE)) << 3) | ((0x10 - data[6]) & 0x0F);
          float t = jfmul(jtofloat(tempS), 0.0625f);
          t = jfdiff(t, 0.250f);
          temperature = ConvertTemp(t);
          break;
        }
        case DS1822_CHIPID:
        case DS18B20_CHIPID: {
          if (data[4] != 0x7F) {
            // Force 12-bit resolution on first read.
            data[4] = 0x7F;
            OneWireReset();
            OneWireSelect(ds18x20_sensor[index].address);
            OneWireWrite(W1_WRITE_SCRATCHPAD);
            OneWireWrite(data[2]);   // Th
            OneWireWrite(data[3]);   // Tl
            OneWireWrite(data[4]);   // config
            OneWireSelect(ds18x20_sensor[index].address);
            OneWireWrite(W1_WRITE_EEPROM);
#ifdef W1_PARASITE_POWER
            DS18X20Data.w1_power_until = millis() + 10;
#endif
          }
          uint16_t temp12 = (data[1] << 8) + data[0];
          if (temp12 > 2047) { temp12 = (~temp12) + 1; sign = -1; }
          float t = jfmul(jtofloat(sign * temp12), 0.0625f);
          temperature = ConvertTemp(t);
          break;
        }
        case MAX31850_CHIPID: {
          int16_t temp14 = (data[1] << 8) + (data[0] & 0xFC);
          float t = jfmul(jtofloat(temp14), 0.0625f);
          temperature = ConvertTemp(t);
          break;
        }
      }
      ds18x20_sensor[index].temperature = temperature;
      if (Settings->flag5.ds18x20_mean) {
        if (ds18x20_sensor[index].numread++ == 0) {
          ds18x20_sensor[index].temp_sum = 0;
        }
        ds18x20_sensor[index].temp_sum = jfadd(ds18x20_sensor[index].temp_sum, temperature);
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
  uint8_t  ids[index];
  memmove_P(ids, GSTR(ds18x20_chipids), sizeof(ids));

  while (--index) {
    if (ds18x20_sensor[sensor_index].address[0] == ids[index]) { break; }
  }
  GetTextIndexed(DS18X20Data.name, sizeof(DS18X20Data.name), index, GSTR(kDs18x20Types));

#ifdef DS18x20_USE_ID_AS_NAME
  char address[17];
  for (uint32_t j = 0; j < 3; j++) {
    sprintf_P(address + 2 * j, PSTR("%02X"), ds18x20_sensor[sensor_index].address[3 - j]);
  }
  snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name),
             PSTR("%s%c%s"), DS18X20Data.name, IndexSeparator(), address);
  return;
#elif defined(DS18x20_USE_ID_ALIAS)
  if (ds18x20_sensor[sensor_index].alias[0] && (ds18x20_sensor[sensor_index].alias[0] != '0')) {
    if (isdigit(ds18x20_sensor[sensor_index].alias[0])) {
      snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name),
                 PSTR("DS18Sens%c%d"), IndexSeparator(),
                 atoi(ds18x20_sensor[sensor_index].alias));
    } else {
      snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name),
                 PSTR("%s"), ds18x20_sensor[sensor_index].alias);
    }
    return;
  }
#endif

  if (DS18X20Data.sensors > 1) {
    snprintf_P(DS18X20Data.name, sizeof(DS18X20Data.name),
               PSTR("%s%c%d"), DS18X20Data.name, IndexSeparator(), sensor + 1);
  }
}

void Ds18x20EverySecond(void) {
  SETREGS
  STGLOB
  if (!DS18X20Data.sensors) { return; }

#ifdef W1_PARASITE_POWER
  if (millis() < DS18X20Data.w1_power_until) { return; }
#endif

#if BUILD_AS_PLUGIN
  uint32_t up = TasmotaGlobal->uptime;
#else
  uint32_t up = TasmotaGlobal.uptime;
#endif

  // Conversion / read on alternating ticks. Multi-sensor parasite-power
  // setups need a conversion every cycle (one sensor at a time).
  bool do_convert = (up & 1)
#ifdef W1_PARASITE_POWER
                  || (DS18X20Data.sensors >= 2)
#endif
                  ;
  if (do_convert) {
    Ds18x20Convert();
    return;
  }

  for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
    if (!Ds18x20Read(i)) {
      Ds18x20Name(i);
      AddLogMissed(DS18X20Data.name, ds18x20_sensor[ds18x20_sensor[i].index].valid);
#ifdef USE_DS18x20_RECONFIGURE
      if (!ds18x20_sensor[ds18x20_sensor[i].index].valid) {
        memset(&ds18x20_sensor, 0, sizeof(ds18x20_sensor));
        Ds18x20Init();
      }
#endif
    }
  }
}

void Ds18x20Show(bool json) {
  SETREGS
  STGLOB
  for (uint32_t i = 0; i < DS18X20Data.sensors; i++) {
    uint8_t index = ds18x20_sensor[i].index;
    if (!ds18x20_sensor[index].valid) { continue; }

    Ds18x20Name(i);
    if (json) {
      if (Settings->flag5.ds18x20_mean) {
#if BUILD_AS_PLUGIN
        uint16_t tele = TasmotaGlobal->tele_period;
#else
        uint16_t tele = TasmotaGlobal.tele_period;
#endif
        if ((0 == tele) && ds18x20_sensor[index].numread) {
          ds18x20_sensor[index].temperature =
              jfdiv(ds18x20_sensor[index].temp_sum,
                    jtofloat(ds18x20_sensor[index].numread));
          ds18x20_sensor[index].numread = 0;
        }
      }
      char address[17];
      address[0] = 0;
      for (uint32_t j = 0; j < 6; j++) {
        sprintf_P(address + 2 * j, PSTR("%02X"),
                  ds18x20_sensor[index].address[6 - j]);
      }
      char tstr[10];
      ftostrfd(ds18x20_sensor[index].temperature,
               Settings->flag2.temperature_resolution, tstr);
      ResponseAppend_P(PSTR(",\"%s\":{\"" D_JSON_ID "\":\"%s\",\""
                            D_JSON_TEMPERATURE "\":%s}"),
                       DS18X20Data.name, address, tstr);

#ifdef USE_DOMOTICZ
#  if BUILD_AS_PLUGIN
      if ((0 == TasmotaGlobal->tele_period) && (0 == i)) {
#  else
      if ((0 == TasmotaGlobal.tele_period) && (0 == i)) {
#  endif
        DomoticzFloatSensor(DZ_TEMP, ds18x20_sensor[index].temperature);
      }
#endif
#ifdef USE_KNX
#  if BUILD_AS_PLUGIN
      if ((0 == TasmotaGlobal->tele_period) && (0 == i)) {
#  else
      if ((0 == TasmotaGlobal.tele_period) && (0 == i)) {
#  endif
        KnxSensor(KNX_TEMPERATURE, ds18x20_sensor[index].temperature);
      }
#endif

#ifdef USE_WEBSERVER
    } else {
      WSContentSend_Temp(DS18X20Data.name, ds18x20_sensor[index].temperature);
#endif
    }
  }
}

#ifdef DS18x20_USE_ID_ALIAS
const char kds18Commands[] PROGMEM = "DS18|" D_CMND_DS_ALIAS;

void CmndDSAlias(void);
void (*const DSCommand[])(void) PROGMEM = { &CmndDSAlias };

void CmndDSAlias(void) {
  SETREGS
  char *Argument1, *Argument2;
  char address[17];

  char *cp = strchr(_DS_MB_DATA, ',');
  if (cp) {
    *cp = 0;
    Argument1 = trimm(_DS_MB_DATA);
    Argument2 = trimm(cp + 1);
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
    for (uint32_t j = 0; j < 8; j++) {
      sprintf_P(address + 2 * j, PSTR("%02X"),
                ds18x20_sensor[ds18x20_sensor[i].index].address[7 - j]);
    }
    ResponseAppend_P(PSTR("\"%s\":{\"" D_JSON_ID "\":\"%s\"}"), DS18X20Data.name, address);
    if (i < DS18X20Data.sensors - 1) { ResponseAppend_P(PSTR(",")); }
  }
  ResponseAppend_P(PSTR("}"));
}
#endif  // DS18x20_USE_ID_ALIAS

void DS18X20_Deinit(void) {
  SETREGS
  RETMEM
}

// --------------------------------------------------------------------
// Dispatcher
// --------------------------------------------------------------------
#if BUILD_AS_PLUGIN

int32_t mod_func_execute(uint32_t function) {
  bool result = false;
  switch (function) {
    case pFUNC_INIT:         result = Ds18x20Init();   break;
    case pFUNC_EVERY_SECOND: Ds18x20EverySecond();     break;
    case pFUNC_JSON_APPEND:  Ds18x20Show(1);           break;
#  ifdef USE_WEBSERVER
    case pFUNC_WEB_SENSOR:   Ds18x20Show(0);           break;
#  endif
#  ifdef DS18x20_USE_ID_ALIAS
    case pFUNC_COMMAND: {
      SETREGS
      result = DecodeCommand(kds18Commands, DSCommand);
    } break;
#  endif
    case pFUNC_DEINIT:       DS18X20_Deinit();         break;
  }
  return result;
}

PULL_OPTIONS

#else  // native

bool Xsns05(uint32_t function) {
  bool result = false;
  if (FUNC_INIT == function) {
    Ds18x20Init();
  } else if (ds18x20_state && initialized) {
    switch (function) {
      case FUNC_EVERY_SECOND: Ds18x20EverySecond(); break;
      case FUNC_JSON_APPEND:  Ds18x20Show(1);       break;
#  ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:   Ds18x20Show(0);       break;
#  endif
#  ifdef DS18x20_USE_ID_ALIAS
      case FUNC_COMMAND:
        result = DecodeCommand(kds18Commands, DSCommand);
        break;
#  endif
    }
  }
  return result;
}

#endif  // BUILD_AS_PLUGIN

// --------------------------------------------------------------------
// Cleanup
// --------------------------------------------------------------------
#if !BUILD_AS_PLUGIN
#  undef onewire_last_discrepancy
#  undef onewire_last_family_discrepancy
#  undef onewire_last_device_flag
#  undef onewire_rom_id
#  undef DS18X20Data
#  undef ds18x20_gpios
#  undef ds18x20_sensor
#  undef delay_low
#  undef delay_high
#  undef initialized
#  undef ALLOCMEM
#  undef RETMEM
#endif
#undef _DS_MB_DATA
#undef MAX_DSB

#endif  // _DS18X20_DUAL_ENABLED
