/*
  xsns_125_bresser.cpp - Bresser Weather Sensor CC1101 Plugin for Tasmota

  Copyright (C) 2024  Gerhard Mutz and Matthias Prinke (BresserWeatherSensorReceiver)

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

#ifdef USE_BRESSER_MOD

#include <SPI.h>
#include "module.h"
#include "module_defines.h"

/*********************************************************************************************/
// Configuration
#define NUM_SENSORS          3
#define MSG_BUF_SIZE         27
#define MAX_REJIDS           4

// Enable all decoders
#define BRESSER_5_IN_1
#define BRESSER_6_IN_1
#define BRESSER_7_IN_1
#define BRESSER_LIGHTNING
#define BRESSER_LEAKAGE

// Wind data as floating point
#define WIND_DATA_FLOATINGPOINT

// Sensor types
#define SENSOR_TYPE_WEATHER0     0
#define SENSOR_TYPE_WEATHER1     1
#define SENSOR_TYPE_THERMO_HYGRO 2
#define SENSOR_TYPE_POOL_THERMO  3
#define SENSOR_TYPE_SOIL         4
#define SENSOR_TYPE_LEAKAGE      5
#define SENSOR_TYPE_AIR_PM       8
#define SENSOR_TYPE_RAIN         9
#define SENSOR_TYPE_LIGHTNING    9
#define SENSOR_TYPE_CO2          10
#define SENSOR_TYPE_HCHO_VOC     11

// Decode status
enum DecodeStatus {
  DECODE_INVALID, DECODE_OK, DECODE_PAR_ERR,
  DECODE_CHK_ERR, DECODE_DIG_ERR, DECODE_SKIP, DECODE_FULL
};

/*********************************************************************************************/
// CC1101 Register Addresses
#define CC1101_REG_IOCFG2     0x00
#define CC1101_REG_IOCFG0     0x02
#define CC1101_REG_FIFOTHR    0x03
#define CC1101_REG_SYNC1      0x04
#define CC1101_REG_SYNC0      0x05
#define CC1101_REG_PKTLEN     0x06
#define CC1101_REG_PKTCTRL1   0x08
#define CC1101_REG_PKTCTRL0   0x07
#define CC1101_REG_ADDR       0x09
#define CC1101_REG_CHANNR     0x0A
#define CC1101_REG_FSCTRL1    0x0B
#define CC1101_REG_FSCTRL0    0x0C
#define CC1101_REG_FREQ2      0x0D
#define CC1101_REG_FREQ1      0x0E
#define CC1101_REG_FREQ0      0x0F
#define CC1101_REG_MDMCFG4    0x10
#define CC1101_REG_MDMCFG3    0x11
#define CC1101_REG_MDMCFG2    0x12
#define CC1101_REG_MDMCFG1    0x13
#define CC1101_REG_MDMCFG0    0x14
#define CC1101_REG_DEVIATN    0x15
#define CC1101_REG_MCSM2      0x16
#define CC1101_REG_MCSM1      0x17
#define CC1101_REG_MCSM0      0x18
#define CC1101_REG_FOCCFG     0x19
#define CC1101_REG_BSCFG      0x1A
#define CC1101_REG_AGCCTRL2   0x1B
#define CC1101_REG_AGCCTRL1   0x1C
#define CC1101_REG_AGCCTRL0   0x1D
#define CC1101_REG_FREND1     0x21
#define CC1101_REG_FREND0     0x22
#define CC1101_REG_FSCAL3     0x23
#define CC1101_REG_FSCAL2     0x24
#define CC1101_REG_FSCAL1     0x25
#define CC1101_REG_FSCAL0     0x26
#define CC1101_REG_TEST2      0x2C
#define CC1101_REG_TEST1      0x2D
#define CC1101_REG_TEST0      0x2E
#define CC1101_REG_PATABLE    0x3E
#define CC1101_REG_FIFO       0x3F

// CC1101 Status registers (read with burst bit)
#define CC1101_REG_RSSI       0x34
#define CC1101_REG_LQI        0x33
#define CC1101_REG_RXBYTES    0x3B

// CC1101 Command Strobes
#define CC1101_CMD_SRES       0x30
#define CC1101_CMD_SCAL       0x33
#define CC1101_CMD_SRX        0x34
#define CC1101_CMD_STX        0x35
#define CC1101_CMD_SIDLE      0x36
#define CC1101_CMD_SPWD       0x39
#define CC1101_CMD_SFRX       0x3A
#define CC1101_CMD_SFTX       0x3B

// SPI access modes
#define CC1101_WRITE_SINGLE   0x00
#define CC1101_WRITE_BURST    0x40
#define CC1101_READ_SINGLE    0x80
#define CC1101_READ_BURST     0xC0

/*********************************************************************************************/
// Weather data structs (from WeatherSensor.h, already C compatible)

struct Weather {
  bool     temp_ok;
  bool     humidity_ok;
  bool     light_ok;
  bool     uv_ok;
  bool     wind_ok;
  bool     rain_ok;
  float    temp_c;
  float    light_klx;
  float    light_lux;
  float    uv;
  float    rain_mm;
  float    wind_direction_deg;
  float    wind_gust_meter_sec;
  float    wind_avg_meter_sec;
  uint8_t  humidity;
};

struct Soil {
  float    temp_c;
  uint8_t  moisture;
};

struct Lightning {
  uint8_t  distance_km;
  uint16_t strike_count;
  uint16_t unknown1;
  uint16_t unknown2;
};

struct Leakage {
  bool     alarm;
};

struct AirPM {
  uint16_t pm_1_0;
  uint16_t pm_2_5;
  uint16_t pm_10;
  uint16_t pm_1_0_init;
  bool     pm_2_5_init;
  bool     pm_10_init;
};

struct AirCO2 {
  uint16_t co2_ppm;
  bool     co2_init;
};

struct AirVOC {
  uint16_t hcho_ppb;
  uint8_t  voc_level;
  bool     hcho_init;
  bool     voc_init;
};

struct WS_Sensor {
  uint32_t sensor_id;
  float    rssi;
  uint8_t  s_type;
  uint8_t  chan;
  uint8_t  rec_count;
  bool     startup;
  bool     battery_ok;
  bool     valid;
  bool     complete;
  union {
    struct Weather      w;
    struct Soil         soil;
    struct Lightning    lgt;
    struct Leakage      leak;
    struct AirPM        pm;
    struct AirCO2       co2;
    struct AirVOC       voc;
  };
};

/*********************************************************************************************/
// Module memory - all global state lives here

typedef struct {
  void     *spi;
  uint8_t  cs_pin;
  uint8_t  gdo0_pin;
  uint8_t  found;
  uint8_t  ready;
  uint8_t  decode_status;
  uint32_t reject_ids[MAX_REJIDS];
  float    rssi;
  struct WS_Sensor sensor[NUM_SENSORS];
  struct WS_Sensor sensor_copy[NUM_SENSORS];
  uint8_t  recvData[MSG_BUF_SIZE];
} MODULE_MEMORY;

#define BRESSER_REV 1 << 16 | 1

PUSH_OPTIONS

MODULE_DESCRIPTOR("BRESSER", MODULE_TYPE_SENSOR, BRESSER_REV, "CS", 15, "GDO", 5, "", 0, "", 0)

MODULE_PART int32_t Bresser_Init();
MODULE_PART void Bresser_Deinit();
MODULE_PART void Bresser_Task();
MODULE_PART void Bresser_Show(bool json);
MODULE_PART bool Bresser_Cmd();
MODULE_PART int32_t mod_func_execute(uint32_t sel);
MODULE_END

/*********************************************************************************************\
 * Float constants (must be in PROGMEM for PIC)
\*********************************************************************************************/

// FP_CONST indices:
// 0 = 0.1   (temp/wind/rain scaling)
// 1 = 0.001 (light klx scaling)
// 2 = 22.5  (5in1 wind direction step)
// 3 = 2.5   (professional rain gauge factor)
// 4 = 1.0   (wind direction float)
// 5 = -50.0 (3in1 temp correction threshold)
const float FP_CONST[] PROGMEM = {0.1, 0.001, 22.5, 2.5, 1.0, -50.0};


/*********************************************************************************************\
 * CC1101 Low-Level SPI Functions
\*********************************************************************************************/

MODULE_PART void cc1101_spi_write_reg(uint8_t addr, uint8_t val) {
  SETMEMREGS
  spiBeginTransaction();
  digitalWrite(mem->cs_pin, LOW);
  spiTransfer(addr | CC1101_WRITE_SINGLE);
  spiTransfer(val);
  digitalWrite(mem->cs_pin, HIGH);
  spiEndTransaction();
}

MODULE_PART uint8_t cc1101_spi_read_reg(uint8_t addr) {
  SETMEMREGS
  spiBeginTransaction();
  digitalWrite(mem->cs_pin, LOW);
  spiTransfer(addr | CC1101_READ_SINGLE);
  uint8_t val = spiTransfer(0x00);
  digitalWrite(mem->cs_pin, HIGH);
  spiEndTransaction();
  return val;
}

MODULE_PART uint8_t cc1101_spi_read_status(uint8_t addr) {
  SETMEMREGS
  spiBeginTransaction();
  digitalWrite(mem->cs_pin, LOW);
  spiTransfer(addr | CC1101_READ_BURST);
  uint8_t val = spiTransfer(0x00);
  digitalWrite(mem->cs_pin, HIGH);
  spiEndTransaction();
  return val;
}

MODULE_PART void cc1101_cmd_strobe(uint8_t cmd) {
  SETMEMREGS
  spiBeginTransaction();
  digitalWrite(mem->cs_pin, LOW);
  spiTransfer(cmd);
  digitalWrite(mem->cs_pin, HIGH);
  spiEndTransaction();
}

MODULE_PART void cc1101_read_burst(uint8_t addr, uint8_t *buf, uint8_t len) {
  SETMEMREGS
  spiBeginTransaction();
  digitalWrite(mem->cs_pin, LOW);
  spiTransfer(addr | CC1101_READ_BURST);
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = spiTransfer(0x00);
  }
  digitalWrite(mem->cs_pin, HIGH);
  spiEndTransaction();
}

MODULE_PART void cc1101_reset() {
  SETMEMREGS
  digitalWrite(mem->cs_pin, HIGH);
  delayMicroseconds(5);
  digitalWrite(mem->cs_pin, LOW);
  delayMicroseconds(10);
  digitalWrite(mem->cs_pin, HIGH);
  delayMicroseconds(41);
  cc1101_cmd_strobe(CC1101_CMD_SRES);
  delay(2);
}

// Configure CC1101 for Bresser Weather Sensor reception
// 868.3 MHz, 8.21 kbps, 2-FSK, 57.136 kHz deviation, 270 kHz BW
// Sync word: AA 2D, Packet length: 27 bytes fixed
MODULE_PART int16_t cc1101_init_bresser() {
  SETMEMREGS

  cc1101_reset();

  // Check CC1101 is present (partnum should be 0x00)
  uint8_t partnum = cc1101_spi_read_status(0x30); // PARTNUM
  uint8_t version = cc1101_spi_read_status(0x31); // VERSION
  if (version == 0x00 || version == 0xFF) {
    return -1;  // CC1101 not found
  }

  // Register configuration for Bresser reception
  // Frequency: 868.3 MHz
  // FREQ = 868.3 * 2^16 / 26 = 0x2188CA (26 MHz crystal)
  cc1101_spi_write_reg(CC1101_REG_FREQ2, 0x21);
  cc1101_spi_write_reg(CC1101_REG_FREQ1, 0x65);
  cc1101_spi_write_reg(CC1101_REG_FREQ0, 0x6A);

  // Data rate: 8.21 kbps
  // DRATE_M = 131, DRATE_E = 8
  cc1101_spi_write_reg(CC1101_REG_MDMCFG4, 0xC8); // BW=270kHz, DRATE_E=8
  cc1101_spi_write_reg(CC1101_REG_MDMCFG3, 0x83); // DRATE_M=131

  // Modulation: 2-FSK, sync mode 16/16
  cc1101_spi_write_reg(CC1101_REG_MDMCFG2, 0x02); // 2-FSK, 16/16 sync

  // Preamble: 4 bytes, channel spacing
  cc1101_spi_write_reg(CC1101_REG_MDMCFG1, 0x22); // 4 preamble bytes
  cc1101_spi_write_reg(CC1101_REG_MDMCFG0, 0xF8);

  // Frequency deviation: 57.136 kHz
  // DEVIATN = (2+M)*2^E * Fxtal/2^17  => E=4, M=7 => 57.129kHz
  cc1101_spi_write_reg(CC1101_REG_DEVIATN, 0x47); // E=4, M=7

  // IF frequency
  cc1101_spi_write_reg(CC1101_REG_FSCTRL1, 0x06);
  cc1101_spi_write_reg(CC1101_REG_FSCTRL0, 0x00);

  // Sync word: AA 2D
  cc1101_spi_write_reg(CC1101_REG_SYNC1, 0xAA);
  cc1101_spi_write_reg(CC1101_REG_SYNC0, 0x2D);

  // Fixed packet length = 27 bytes
  cc1101_spi_write_reg(CC1101_REG_PKTLEN, MSG_BUF_SIZE);
  cc1101_spi_write_reg(CC1101_REG_PKTCTRL0, 0x00); // Fixed length, no CRC
  cc1101_spi_write_reg(CC1101_REG_PKTCTRL1, 0x00); // No address check

  // GDO0: Asserts when sync word has been sent/received, deasserts at end of packet
  cc1101_spi_write_reg(CC1101_REG_IOCFG0, 0x06);
  // GDO2: high impedance (not used)
  cc1101_spi_write_reg(CC1101_REG_IOCFG2, 0x2E);

  // AGC configuration
  cc1101_spi_write_reg(CC1101_REG_AGCCTRL2, 0x43);
  cc1101_spi_write_reg(CC1101_REG_AGCCTRL1, 0x40);
  cc1101_spi_write_reg(CC1101_REG_AGCCTRL0, 0x91);

  // Frequency offset compensation
  cc1101_spi_write_reg(CC1101_REG_FOCCFG, 0x16);
  cc1101_spi_write_reg(CC1101_REG_BSCFG, 0x6C);

  // Front end config
  cc1101_spi_write_reg(CC1101_REG_FREND1, 0x56);
  cc1101_spi_write_reg(CC1101_REG_FREND0, 0x10);

  // Frequency synthesizer calibration
  cc1101_spi_write_reg(CC1101_REG_FSCAL3, 0xE9);
  cc1101_spi_write_reg(CC1101_REG_FSCAL2, 0x2A);
  cc1101_spi_write_reg(CC1101_REG_FSCAL1, 0x00);
  cc1101_spi_write_reg(CC1101_REG_FSCAL0, 0x1F);

  // Test registers
  cc1101_spi_write_reg(CC1101_REG_TEST2, 0x81);
  cc1101_spi_write_reg(CC1101_REG_TEST1, 0x35);
  cc1101_spi_write_reg(CC1101_REG_TEST0, 0x09);

  // Main Radio Control State Machine
  cc1101_spi_write_reg(CC1101_REG_MCSM2, 0x07);
  cc1101_spi_write_reg(CC1101_REG_MCSM1, 0x30); // Stay in RX after receiving
  cc1101_spi_write_reg(CC1101_REG_MCSM0, 0x18); // Auto calibrate on idle->RX/TX

  // PA Table - 10dBm
  cc1101_spi_write_reg(CC1101_REG_PATABLE, 0xC0);

  // Calibrate
  cc1101_cmd_strobe(CC1101_CMD_SCAL);
  delay(1);

  // Start RX
  cc1101_cmd_strobe(CC1101_CMD_SFRX);  // Flush RX FIFO
  cc1101_cmd_strobe(CC1101_CMD_SRX);    // Enter RX mode

  AddLog(LOG_LEVEL_INFO, PSTR("BRESSER: CC1101 init OK, partnum=%02X ver=%02X"), partnum, version);
  return 0;
}

MODULE_PART float cc1101_get_rssi() {
  SETMEMREGS
  uint8_t rawRSSI = cc1101_spi_read_status(CC1101_REG_RSSI);
  int16_t rssi;
  if (rawRSSI >= 128) {
    rssi = ((int16_t)rawRSSI - 256) / 2 - 74;
  } else {
    rssi = (int16_t)rawRSSI / 2 - 74;
  }
  return (float)rssi;
}

/*********************************************************************************************\
 * Checksum / Validation Functions (from WeatherSensor.cpp)
\*********************************************************************************************/

MODULE_PART uint16_t lfsr_digest16(const uint8_t *message, unsigned bytes, uint16_t gen, uint16_t key) {
  uint16_t sum = 0;
  for (unsigned k = 0; k < bytes; ++k) {
    uint8_t data = message[k];
    for (int i = 7; i >= 0; --i) {
      if ((data >> i) & 1)
        sum ^= key;
      if (key & 1)
        key = (key >> 1) ^ gen;
      else
        key = (key >> 1);
    }
  }
  return sum;
}

MODULE_PART int add_bytes(const uint8_t *message, unsigned num_bytes) {
  int result = 0;
  for (unsigned i = 0; i < num_bytes; ++i) {
    result += message[i];
  }
  return result;
}

MODULE_PART uint16_t crc16(const uint8_t *message, unsigned nBytes, uint16_t polynomial, uint16_t init) {
  uint16_t remainder = init;
  unsigned byte, bit;
  for (byte = 0; byte < nBytes; ++byte) {
    remainder ^= message[byte] << 8;
    for (bit = 0; bit < 8; ++bit) {
      if (remainder & 0x8000)
        remainder = (remainder << 1) ^ polynomial;
      else
        remainder = (remainder << 1);
    }
  }
  return remainder;
}

/*********************************************************************************************\
 * Slot Management (from WeatherSensor.cpp)
\*********************************************************************************************/

MODULE_PART int findSlot(uint32_t id, DecodeStatus *status) {
  SETMEMREGS
  int free_slot = -1;
  int update_slot = -1;

  for (int i = 0; i < NUM_SENSORS; i++) {
    if (!mem->sensor[i].valid && (free_slot < 0)) {
      free_slot = i;
    } else if (mem->sensor[i].valid && (mem->sensor[i].sensor_id == id)) {
      update_slot = i;
    }
  }

  if (update_slot > -1) {
    *status = DECODE_OK;
    return update_slot;
  } else if (free_slot > -1) {
    *status = DECODE_OK;
    return free_slot;
  } else {
    *status = DECODE_FULL;
    return -1;
  }
}

MODULE_PART void clearSlots() {
  SETMEMREGS
  memset(mem->sensor, 0, sizeof(mem->sensor));
}

/*********************************************************************************************\
 * Bresser 5-in-1 Decoder
\*********************************************************************************************/
#ifdef BRESSER_5_IN_1
MODULE_PART DecodeStatus decodeBresser5In1Payload(const uint8_t *msg, uint8_t msgSize) {
  SETMEMREGS

  // First 13 bytes need to match inverse of last 13 bytes
  for (unsigned col = 0; col < msgSize / 2; ++col) {
    if ((msg[col] ^ msg[col + 13]) != 0xff) {
      return DECODE_PAR_ERR;
    }
  }

  // Verify checksum (number of bits set in bytes 14-25)
  uint8_t bitsSet = 0;
  uint8_t expectedBitsSet = msg[13];
  for (uint8_t p = 14; p < msgSize; p++) {
    uint8_t currentByte = msg[p];
    while (currentByte) {
      bitsSet += (currentByte & 1);
      currentByte >>= 1;
    }
  }
  if (bitsSet != expectedBitsSet) {
    return DECODE_CHK_ERR;
  }

  uint8_t id_tmp = msg[14];
  uint8_t type_tmp = msg[15] & 0xF;
  DecodeStatus status;

  int slot = findSlot(id_tmp, &status);
  if (status != DECODE_OK)
    return status;

  mem->sensor[slot].sensor_id = id_tmp;
  mem->sensor[slot].chan = 0;
  mem->sensor[slot].startup = ((msg[15] & 0x80) == 0) ? true : false;
  mem->sensor[slot].battery_ok = (msg[25] & 0x80) ? false : true;
  mem->sensor[slot].valid = true;
  mem->sensor[slot].rssi = mem->rssi;
  mem->sensor[slot].rec_count++;
  mem->sensor[slot].complete = true;

  int temp_raw = (msg[20] & 0x0f) + ((msg[20] & 0xf0) >> 4) * 10 + (msg[21] & 0x0f) * 100;
  if (msg[25] & 0x0f) {
    temp_raw = -temp_raw;
  }
  mem->sensor[slot].w.temp_c = fmul(tofloat(temp_raw), FLTC(0));

  mem->sensor[slot].w.humidity = (msg[22] & 0x0f) + ((msg[22] & 0xf0) >> 4) * 10;

  int wind_direction_raw = ((msg[17] & 0xf0) >> 4) * 225;
  int gust_raw = ((msg[17] & 0x0f) << 8) + msg[16];
  int wind_raw = (msg[18] & 0x0f) + ((msg[18] & 0xf0) >> 4) * 10 + (msg[19] & 0x0f) * 100;

  mem->sensor[slot].w.wind_direction_deg = fmul(tofloat(wind_direction_raw), FLTC(0));
  mem->sensor[slot].w.wind_gust_meter_sec = fmul(tofloat(gust_raw), FLTC(0));
  mem->sensor[slot].w.wind_avg_meter_sec = fmul(tofloat(wind_raw), FLTC(0));

  int rain_raw = (msg[23] & 0x0f) + ((msg[23] & 0xf0) >> 4) * 10 + (msg[24] & 0x0f) * 100 + ((msg[24] & 0xf0) >> 4) * 1000;
  mem->sensor[slot].w.rain_mm = fmul(tofloat(rain_raw), FLTC(0));

  // Professional Rain Gauge (type 0x9) - rescale rain by 2.5x
  if (type_tmp == 0x9) {
    mem->sensor[slot].w.rain_mm = fmul(mem->sensor[slot].w.rain_mm, FLTC(3));
    type_tmp = SENSOR_TYPE_WEATHER0;
    mem->sensor[slot].w.humidity_ok = false;
    mem->sensor[slot].w.wind_ok = false;
  } else {
    mem->sensor[slot].w.humidity_ok = true;
    mem->sensor[slot].w.wind_ok = true;
  }

  mem->sensor[slot].s_type = type_tmp;
  mem->sensor[slot].w.temp_ok = true;
  mem->sensor[slot].w.light_ok = false;
  mem->sensor[slot].w.uv_ok = false;
  mem->sensor[slot].w.rain_ok = true;

  return DECODE_OK;
}
#endif

/*********************************************************************************************\
 * Bresser 6-in-1 Decoder
\*********************************************************************************************/
#ifdef BRESSER_6_IN_1
MODULE_PART DecodeStatus decodeBresser6In1Payload(const uint8_t *msg, uint8_t msgSize) {
  SETMEMREGS
  int const moisture_map[] = {0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99};

  bool temp_ok = false;
  bool humidity_ok = false;
  bool uv_ok = false;
  bool wind_ok = false;
  bool rain_ok = false;

  // LFSR-16 digest, generator 0x8810 init 0x5412
  int chkdgst = (msg[0] << 8) | msg[1];
  int digest = lfsr_digest16(&msg[2], 15, 0x8810, 0x5412);
  if (chkdgst != digest)
    return DECODE_DIG_ERR;

  // Checksum, add with carry
  int sum = add_bytes(&msg[2], 16);
  if ((sum & 0xff) != 0xff)
    return DECODE_CHK_ERR;

  uint32_t id_tmp = ((uint32_t)msg[2] << 24) | (msg[3] << 16) | (msg[4] << 8) | (msg[5]);
  uint8_t type_tmp = (msg[6] >> 4);
  uint8_t chan_tmp = (msg[6] & 0x7);
  uint8_t flags = (msg[16] & 0x0f);
  DecodeStatus status;

  int slot = findSlot(id_tmp, &status);
  if (status != DECODE_OK)
    return status;

  if (!mem->sensor[slot].valid) {
    mem->sensor[slot].w.temp_ok = false;
    mem->sensor[slot].w.humidity_ok = false;
    mem->sensor[slot].w.uv_ok = false;
    mem->sensor[slot].w.wind_ok = false;
    mem->sensor[slot].w.rain_ok = false;
  }
  mem->sensor[slot].sensor_id = id_tmp;
  mem->sensor[slot].s_type = type_tmp;
  mem->sensor[slot].chan = chan_tmp;
  mem->sensor[slot].startup = ((msg[6] & 0x8) == 0) ? true : false;
  mem->sensor[slot].battery_ok = (msg[13] >> 1) & 1;

  // temperature, humidity(, uv) - shared with rain counter
  temp_ok = humidity_ok = (flags == 0);
  float temp = 0;
  if (temp_ok) {
    bool sign = (msg[13] >> 3) & 1;
    int temp_raw = (msg[12] >> 4) * 100 + (msg[12] & 0x0f) * 10 + (msg[13] >> 4);
    temp = fmul(tofloat((sign) ? (temp_raw - 1000) : temp_raw), FLTC(0));

    // Correction for 3-in-1 Professional Wind Gauge
    if (ltsf2(temp, FLTC(5))) {
      temp = fmul(tofloat(-temp_raw), FLTC(0));
    }

    mem->sensor[slot].w.temp_c = temp;
    mem->sensor[slot].w.humidity = (msg[14] >> 4) * 10 + (msg[14] & 0x0f);

    uv_ok = ((~msg[15] & 0xff) <= 0x99) && ((~msg[16] & 0xf0) <= 0x90);
    if (uv_ok) {
      int uv_raw = ((~msg[15] & 0xf0) >> 4) * 100 + (~msg[15] & 0x0f) * 10 + ((~msg[16] & 0xf0) >> 4);
      mem->sensor[slot].w.uv = fmul(tofloat(uv_raw), FLTC(0));
    }
  }

  // invert 3 bytes wind speeds
  uint8_t _imsg7 = msg[7] ^ 0xff;
  uint8_t _imsg8 = msg[8] ^ 0xff;
  uint8_t _imsg9 = msg[9] ^ 0xff;

  wind_ok = (_imsg7 <= 0x99) && (_imsg8 <= 0x99) && (_imsg9 <= 0x99);
  if (wind_ok) {
    int gust_raw = (_imsg7 >> 4) * 100 + (_imsg7 & 0x0f) * 10 + (_imsg8 >> 4);
    int wavg_raw = (_imsg9 >> 4) * 100 + (_imsg9 & 0x0f) * 10 + (_imsg8 & 0x0f);
    int wind_dir_raw = ((msg[10] & 0xf0) >> 4) * 100 + (msg[10] & 0x0f) * 10 + ((msg[11] & 0xf0) >> 4);

    mem->sensor[slot].w.wind_gust_meter_sec = fmul(tofloat(gust_raw), FLTC(0));
    mem->sensor[slot].w.wind_avg_meter_sec = fmul(tofloat(wavg_raw), FLTC(0));
    mem->sensor[slot].w.wind_direction_deg = fmul(tofloat(wind_dir_raw), FLTC(4));
  }

  // rain counter, inverted 3 bytes BCD
  uint8_t _imsg12 = msg[12] ^ 0xff;
  uint8_t _imsg13 = msg[13] ^ 0xff;
  uint8_t _imsg14 = msg[14] ^ 0xff;

  rain_ok = (flags == 1) && (type_tmp == 1);
  if (rain_ok) {
    int rain_raw = (_imsg12 >> 4) * 100000 + (_imsg12 & 0x0f) * 10000 + (_imsg13 >> 4) * 1000 + (_imsg13 & 0x0f) * 100 + (_imsg14 >> 4) * 10 + (_imsg14 & 0x0f);
    mem->sensor[slot].w.rain_mm = fmul(tofloat(rain_raw), FLTC(0));
  }

  // Pool / Spa thermometer
  if (mem->sensor[slot].s_type == SENSOR_TYPE_POOL_THERMO) {
    humidity_ok = false;
  }

  // Soil moisture sensor
  if (mem->sensor[slot].s_type == SENSOR_TYPE_SOIL) {
    wind_ok = false;
    uv_ok = false;
  }

  if (mem->sensor[slot].s_type == SENSOR_TYPE_SOIL && temp_ok && mem->sensor[slot].w.humidity >= 1 && mem->sensor[slot].w.humidity <= 16) {
    humidity_ok = false;
    mem->sensor[slot].soil.moisture = moisture_map[mem->sensor[slot].w.humidity - 1];
    mem->sensor[slot].soil.temp_c = temp;
    mem->sensor[slot].rec_count++;
  }

  // Update per-slot status flags
  mem->sensor[slot].w.temp_ok |= temp_ok;
  mem->sensor[slot].w.humidity_ok |= humidity_ok;
  mem->sensor[slot].w.uv_ok |= uv_ok;
  mem->sensor[slot].w.wind_ok |= wind_ok;
  mem->sensor[slot].w.rain_ok |= rain_ok;

  mem->sensor[slot].valid = true;

  // 6-in-1 weather station data is split into two messages
  if (mem->sensor[slot].s_type == SENSOR_TYPE_WEATHER1) {
    if (mem->sensor[slot].w.temp_ok && mem->sensor[slot].w.rain_ok) {
      mem->sensor[slot].complete = true;
    }
  } else {
    mem->sensor[slot].complete = true;
  }

  mem->sensor[slot].rssi = mem->rssi;
  mem->sensor[slot].rec_count++;

  return DECODE_OK;
}
#endif

/*********************************************************************************************\
 * Bresser 7-in-1 Decoder
\*********************************************************************************************/
#ifdef BRESSER_7_IN_1
MODULE_PART DecodeStatus decodeBresser7In1Payload(const uint8_t *msg, uint8_t msgSize) {
  SETMEMREGS

  if (msg[21] == 0x00) {
    // Data sanity check
  }

  // data de-whitening
  uint8_t msgw[MSG_BUF_SIZE];
  for (unsigned i = 0; i < msgSize; ++i) {
    msgw[i] = msg[i] ^ 0xaa;
  }

  // LFSR-16 digest, generator 0x8810 key 0xba95 final xor 0x6df1
  int chkdgst = (msgw[0] << 8) | msgw[1];
  int digest = lfsr_digest16(&msgw[2], 23, 0x8810, 0xba95);
  if ((chkdgst ^ digest) != 0x6df1)
    return DECODE_DIG_ERR;

  int id_tmp = (msgw[2] << 8) | (msgw[3]);
  int s_type = msg[6] >> 4; // raw data, no de-whitening

  DecodeStatus status;
  int slot = findSlot(id_tmp, &status);
  if (status != DECODE_OK)
    return status;

  int flags = (msgw[15] & 0x0f);
  int battery_low = (flags & 0x06) == 0x06;

  mem->sensor[slot].sensor_id = id_tmp;
  mem->sensor[slot].s_type = s_type;
  mem->sensor[slot].startup = (msg[6] & 0x08) == 0x00;
  mem->sensor[slot].chan = msg[6] & 0x07;
  mem->sensor[slot].battery_ok = !battery_low;
  mem->sensor[slot].valid = true;
  mem->sensor[slot].complete = true;
  mem->sensor[slot].rssi = mem->rssi;
  mem->sensor[slot].rec_count++;

  if (s_type == SENSOR_TYPE_WEATHER1) {
    int wdir = (msgw[4] >> 4) * 100 + (msgw[4] & 0x0f) * 10 + (msgw[5] >> 4);
    int wgst_raw = (msgw[7] >> 4) * 100 + (msgw[7] & 0x0f) * 10 + (msgw[8] >> 4);
    int wavg_raw = (msgw[8] & 0x0f) * 100 + (msgw[9] >> 4) * 10 + (msgw[9] & 0x0f);
    int rain_raw = (msgw[10] >> 4) * 100000 + (msgw[10] & 0x0f) * 10000 + (msgw[11] >> 4) * 1000 + (msgw[11] & 0x0f) * 100 + (msgw[12] >> 4) * 10 + (msgw[12] & 0x0f);
    int temp_raw = (msgw[14] >> 4) * 100 + (msgw[14] & 0x0f) * 10 + (msgw[15] >> 4);
    float temp_c = fmul(tofloat(temp_raw), FLTC(0));
    if (temp_raw > 600)
      temp_c = fmul(tofloat(temp_raw - 1000), FLTC(0));
    int humidity = (msgw[16] >> 4) * 10 + (msgw[16] & 0x0f);
    int lght_raw = (msgw[17] >> 4) * 100000 + (msgw[17] & 0x0f) * 10000 + (msgw[18] >> 4) * 1000 + (msgw[18] & 0x0f) * 100 + (msgw[19] >> 4) * 10 + (msgw[19] & 0x0f);
    int uv_raw = (msgw[20] >> 4) * 100 + (msgw[20] & 0x0f) * 10 + (msgw[21] >> 4);

    mem->sensor[slot].w.temp_ok = true;
    mem->sensor[slot].w.humidity_ok = true;
    mem->sensor[slot].w.wind_ok = true;
    mem->sensor[slot].w.rain_ok = true;
    mem->sensor[slot].w.light_ok = true;
    mem->sensor[slot].w.uv_ok = true;
    mem->sensor[slot].w.temp_c = temp_c;
    mem->sensor[slot].w.humidity = humidity;
    mem->sensor[slot].w.wind_gust_meter_sec = fmul(tofloat(wgst_raw), FLTC(0));
    mem->sensor[slot].w.wind_avg_meter_sec = fmul(tofloat(wavg_raw), FLTC(0));
    mem->sensor[slot].w.wind_direction_deg = fmul(tofloat(wdir), FLTC(4));
    mem->sensor[slot].w.rain_mm = fmul(tofloat(rain_raw), FLTC(0));
    mem->sensor[slot].w.light_klx = fmul(tofloat(lght_raw), FLTC(1));
    mem->sensor[slot].w.light_lux = tofloat(lght_raw);
    mem->sensor[slot].w.uv = fmul(tofloat(uv_raw), FLTC(0));
  } else if (s_type == SENSOR_TYPE_AIR_PM) {
    mem->sensor[slot].pm.pm_1_0 = (msgw[8] & 0x0f) * 1000 + (msgw[9] >> 4) * 100 + (msgw[9] & 0x0f) * 10 + (msgw[10] >> 4);
    mem->sensor[slot].pm.pm_2_5 = (msgw[10] & 0x0f) * 1000 + (msgw[11] >> 4) * 100 + (msgw[11] & 0x0f) * 10 + (msgw[12] >> 4);
    mem->sensor[slot].pm.pm_10 = (msgw[12] & 0x0f) * 1000 + (msgw[13] >> 4) * 100 + (msgw[13] & 0x0f) * 10 + (msgw[14] >> 4);
    mem->sensor[slot].pm.pm_1_0_init = ((msgw[10] >> 4) & 0x0f) == 0x0f;
    mem->sensor[slot].pm.pm_2_5_init = ((msgw[12] >> 4) & 0x0f) == 0x0f;
    mem->sensor[slot].pm.pm_10_init = ((msgw[14] >> 4) & 0x0f) == 0x0f;
  } else if (s_type == SENSOR_TYPE_CO2) {
    mem->sensor[slot].co2.co2_ppm = ((msgw[4] & 0xf0) >> 4) * 1000 + (msgw[4] & 0x0f) * 100 + ((msgw[5] & 0xf0) >> 4) * 10 + (msgw[5] & 0x0f);
    mem->sensor[slot].co2.co2_init = (msgw[5] & 0x0f) == 0x0f;
  } else if (s_type == SENSOR_TYPE_HCHO_VOC) {
    mem->sensor[slot].voc.hcho_ppb = ((msgw[4] & 0xf0) >> 4) * 1000 + (msgw[4] & 0x0f) * 100 + ((msgw[5] & 0xf0) >> 4) * 10 + (msgw[5] & 0x0f);
    mem->sensor[slot].voc.voc_level = (msgw[22] & 0x0f);
    mem->sensor[slot].voc.hcho_init = (msgw[5] & 0x0f) == 0x0f;
    mem->sensor[slot].voc.voc_init = msgw[22] == 0x0f;
  } else if (s_type == SENSOR_TYPE_SOIL) {
    int temp_raw = (msg[20] & 0x0f) + ((msg[20] & 0xf0) >> 4) * 10 + (msg[21] & 0x0f) * 100;
    if (msg[25] & 0x0f) {
      temp_raw = -temp_raw;
    }
    mem->sensor[slot].w.temp_c = fmul(tofloat(temp_raw), FLTC(0));
    mem->sensor[slot].w.humidity = (msg[22] & 0x0f) + ((msg[22] & 0xf0) >> 4) * 10;

    int const moisture_map[] = {0, 7, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 99};
    mem->sensor[slot].w.humidity_ok = false;
    mem->sensor[slot].soil.moisture = moisture_map[mem->sensor[slot].w.humidity - 1];
    mem->sensor[slot].soil.temp_c = mem->sensor[slot].w.temp_c;
  }

  return DECODE_OK;
}
#endif

/*********************************************************************************************\
 * Bresser Lightning Decoder
\*********************************************************************************************/
#ifdef BRESSER_LIGHTNING
MODULE_PART DecodeStatus decodeBresserLightningPayload(const uint8_t *msg, uint8_t msgSize) {
  SETMEMREGS

  // data de-whitening
  uint8_t msgw[MSG_BUF_SIZE];
  for (unsigned i = 0; i < msgSize; ++i) {
    msgw[i] = msg[i] ^ 0xaa;
  }

  // LFSR-16 digest, generator 0x8810 key 0xabf9 with a final xor 0x899e
  int chk = (msgw[0] << 8) | msgw[1];
  int digest = lfsr_digest16(&msgw[2], 8, 0x8810, 0xabf9);
  if (((chk ^ digest) != 0x899e))
    return DECODE_DIG_ERR;

  int id_tmp = (msgw[2] << 8) | (msgw[3]);
  int s_type = msg[6] >> 4;
  int startup = (msg[6] & 0x8) == 0x00;

  DecodeStatus status;
  int slot = findSlot(id_tmp, &status);
  if (status != DECODE_OK)
    return status;

  // Counter as BCD (max 1599)
  uint16_t ctr = (msgw[4] >> 4) * 100 + (msgw[4] & 0xf) * 10 + (msgw[5] >> 4);
  uint8_t battery_low = (msgw[5] & 0x08) == 0x00;
  uint16_t unknown1 = ((msgw[5] & 0x0f) << 8) | msgw[6];
  uint8_t distance_km = msgw[7];
  uint16_t unknown2 = (msgw[8] << 8) | msgw[9];

  mem->sensor[slot].sensor_id = id_tmp;
  mem->sensor[slot].s_type = s_type;
  mem->sensor[slot].startup = startup;
  mem->sensor[slot].chan = 0;
  mem->sensor[slot].battery_ok = !battery_low;
  mem->sensor[slot].rssi = mem->rssi;
  mem->sensor[slot].rec_count++;
  mem->sensor[slot].valid = true;
  mem->sensor[slot].complete = true;

  mem->sensor[slot].lgt.strike_count = ctr;
  mem->sensor[slot].lgt.distance_km = distance_km;
  mem->sensor[slot].lgt.unknown1 = unknown1;
  mem->sensor[slot].lgt.unknown2 = unknown2;

  return DECODE_OK;
}
#endif

/*********************************************************************************************\
 * Bresser Leakage Decoder
\*********************************************************************************************/
#ifdef BRESSER_LEAKAGE
MODULE_PART DecodeStatus decodeBresserLeakagePayload(const uint8_t *msg, uint8_t msgSize) {
  SETMEMREGS

  // Verify CRC (CRC16/XMODEM)
  uint16_t crc_act = crc16(&msg[2], 5, 0x1021, 0x0000);
  uint16_t crc_exp = (msg[0] << 8) | msg[1];
  if (crc_act != crc_exp)
    return DECODE_CHK_ERR;

  uint32_t id_tmp = ((uint32_t)msg[2] << 24) | (msg[3] << 16) | (msg[4] << 8) | (msg[5]);
  uint8_t type_tmp = msg[6] >> 4;
  uint8_t chan_tmp = (msg[6] & 0x7);
  bool alarm = (msg[7] & 0x80) == 0x80;
  bool no_alarm = (msg[7] & 0x40) == 0x40;

  // Sanity checks
  bool decode_ok = (type_tmp == SENSOR_TYPE_LEAKAGE) &&
                   (alarm != no_alarm) &&
                   (chan_tmp != 0);
  if (!decode_ok)
    return DECODE_INVALID;

  DecodeStatus status = DECODE_OK;
  int slot = findSlot(id_tmp, &status);
  if (status != DECODE_OK)
    return status;

  mem->sensor[slot].sensor_id = id_tmp;
  mem->sensor[slot].s_type = type_tmp;
  mem->sensor[slot].chan = chan_tmp;
  mem->sensor[slot].startup = (msg[6] & 0x8) == 0x00;
  mem->sensor[slot].battery_ok = (msg[7] & 0x30) != 0x00;
  mem->sensor[slot].rssi = mem->rssi;
  mem->sensor[slot].rec_count++;
  mem->sensor[slot].valid = true;
  mem->sensor[slot].complete = true;
  mem->sensor[slot].leak.alarm = (alarm && !no_alarm);

  return DECODE_OK;
}
#endif

/*********************************************************************************************\
 * Message Decoder Dispatcher
\*********************************************************************************************/

MODULE_PART DecodeStatus decodeMessage(const uint8_t *msg, uint8_t msgSize) {
  DecodeStatus decode_res = DECODE_INVALID;

#ifdef BRESSER_7_IN_1
  decode_res = decodeBresser7In1Payload(msg, msgSize);
  if (decode_res == DECODE_OK || decode_res == DECODE_FULL || decode_res == DECODE_SKIP)
    return decode_res;
#endif
#ifdef BRESSER_6_IN_1
  decode_res = decodeBresser6In1Payload(msg, msgSize);
  if (decode_res == DECODE_OK || decode_res == DECODE_FULL || decode_res == DECODE_SKIP)
    return decode_res;
#endif
#ifdef BRESSER_5_IN_1
  decode_res = decodeBresser5In1Payload(msg, msgSize);
  if (decode_res == DECODE_OK || decode_res == DECODE_FULL || decode_res == DECODE_SKIP)
    return decode_res;
#endif
#ifdef BRESSER_LIGHTNING
  decode_res = decodeBresserLightningPayload(msg, msgSize);
  if (decode_res == DECODE_OK || decode_res == DECODE_FULL || decode_res == DECODE_SKIP)
    return decode_res;
#endif
#ifdef BRESSER_LEAKAGE
  decode_res = decodeBresserLeakagePayload(msg, msgSize);
#endif
  return decode_res;
}

/*********************************************************************************************\
 * getMessage - Poll CC1101 GDO0 and read packet
\*********************************************************************************************/

MODULE_PART DecodeStatus Bresser_getMessage() {
  SETMEMREGS
  DecodeStatus decode_res = DECODE_INVALID;

  // Check GDO0 - asserted when sync word received and packet complete
  uint8_t rxBytes = cc1101_spi_read_status(CC1101_REG_RXBYTES);

  if (rxBytes >= MSG_BUF_SIZE) {
    // Read FIFO
    cc1101_read_burst(CC1101_REG_FIFO, mem->recvData, MSG_BUF_SIZE);

    // Get RSSI
    mem->rssi = cc1101_get_rssi();

    // Flush RX FIFO and restart RX
    cc1101_cmd_strobe(CC1101_CMD_SIDLE);
    cc1101_cmd_strobe(CC1101_CMD_SFRX);
    cc1101_cmd_strobe(CC1101_CMD_SRX);

    // Verify last sync word byte is first byte of payload
    if (mem->recvData[0] == 0xD4) {
      AddLog(LOG_LEVEL_DEBUG, PSTR("BRESSER: Received packet, RSSI=%1_f"), &mem->rssi);
      decode_res = decodeMessage(&mem->recvData[1], MSG_BUF_SIZE - 1);
    }
  }

  return decode_res;
}

/*********************************************************************************************\
 * Sort sensors by type for display
\*********************************************************************************************/

MODULE_PART void bresser_bubbleSort(struct WS_Sensor *sens, uint32_t n) {
  SETMEMREGS
  for (uint32_t i = 0; i < n; i++) {
    if (iseq(sens[i].rssi)) sens[i].s_type = 99;
  }
  for (uint32_t i = 0; i < n - 1; i++) {
    bool swapped = false;
    for (uint32_t j = 0; j < n - i - 1; j++) {
      if (sens[j].s_type && (sens[j].s_type > sens[j + 1].s_type)) {
        // swap using byte copy
        uint8_t temp[sizeof(struct WS_Sensor)];
        memmove(&temp, &sens[j], sizeof(struct WS_Sensor));
        memmove(&sens[j], &sens[j + 1], sizeof(struct WS_Sensor));
        memmove(&sens[j + 1], &temp, sizeof(struct WS_Sensor));
        swapped = true;
      }
    }
    if (!swapped) break;
  }
}

/*********************************************************************************************\
 * Reject ID check
\*********************************************************************************************/

MODULE_PART bool Bresser_reject(uint32_t id) {
  SETMEMREGS
  for (int r = 0; r < MAX_REJIDS; r++) {
    if (mem->reject_ids[r] > 0) {
      if (mem->reject_ids[r] == id) {
        return true;
      }
    }
  }
  return false;
}

/*********************************************************************************************\
 * HTML Format Strings
\*********************************************************************************************/

const char HTTP_Bresser1[] PROGMEM =
  "{s}<hr>{m}<hr>{e}"
  "{s}%s ID" "{m}%08x" "{e}"
  "{s}%s Type" "{m}%x" "{e}"
  "{s}%s Chan" "{m}%d" "{e}"
  "{s}%s Stat" "{m}%d" "{e}"
  "{s}%s Batt" "{m}%-3s" "{e}"
  "{s}%s RSSI" "{m}%1_f dBm" "{e}"
  "{s}%s RCNT" "{m}%d" "{e}"
  ;

const char HTTP_Bresser2[] PROGMEM =
  "{s}%s WMAX" "{m}%1_f" "{e}"
  "{s}%s WAVG" "{m}%1_f" "{e}"
  "{s}%s CWDIR" "{m}%1_f" "{e}"
  ;

const char HTTP_Bresser3[] PROGMEM =
  "{s}%s RAIN" "{m}%1_f" "{e}"
  ;

const char HTTP_Bresser4[] PROGMEM =
  "{s}%s UVidx" "{m}%1_f" "{e}"
  ;

const char HTTP_Bresser5[] PROGMEM =
  "{s}%s Light" "{m}%1_f" "{e}"
  ;

const char HTTP_Bresser6[] PROGMEM =
  "{s}%s Temperature" "{m}%1_f" "{e}"
  "{s}%s Moisture" "{m}%d" "{e}"
  ;

const char HTTP_Bresser7[] PROGMEM =
  "{s}%s Pool Temperature" "{m}%1_f" "{e}"
  ;

/*********************************************************************************************\
 * Show sensor data (Web + JSON)
\*********************************************************************************************/

MODULE_PART void Bresser_Show(bool json) {
  SETREGS
  STGLOB

  if (mem->decode_status != DECODE_OK) {
    return;
  }

  bresser_bubbleSort(&mem->sensor_copy[0], NUM_SENSORS);

  if (!json) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (Bresser_reject(mem->sensor_copy[i].sensor_id)) continue;
      if (iseq(mem->sensor_copy[i].rssi)) continue;

      char label[16];
      sprintf_P(label, PSTR("Bresser %1d"), i + 1);

      WSContentSend_PD(GSTR(HTTP_Bresser1), label, (int)mem->sensor_copy[i].sensor_id, label, mem->sensor_copy[i].s_type, label, mem->sensor_copy[i].chan, label, mem->sensor_copy[i].startup, label, mem->sensor_copy[i].battery_ok ? "OK " : "Low", label, &mem->sensor_copy[i].rssi, label, mem->sensor_copy[i].rec_count);

      if (mem->sensor_copy[i].s_type == SENSOR_TYPE_SOIL) {
        WSContentSend_PD(GSTR(HTTP_Bresser6), label, &mem->sensor_copy[i].soil.temp_c, label, mem->sensor_copy[i].soil.moisture);
      } else if (mem->sensor_copy[i].s_type == SENSOR_TYPE_POOL_THERMO) {
        WSContentSend_PD(GSTR(HTTP_Bresser7), label, &mem->sensor_copy[i].w.temp_c);
      } else if (mem->sensor_copy[i].s_type == SENSOR_TYPE_THERMO_HYGRO) {
        if (mem->sensor_copy[i].w.temp_ok && mem->sensor_copy[i].w.humidity_ok) {
          TempHumDewShow(json, (0 == TasmotaGlobal->tele_period), label, mem->sensor_copy[i].w.temp_c, mem->sensor_copy[i].w.humidity);
        }
      } else {
        if (mem->sensor_copy[i].w.temp_ok && mem->sensor_copy[i].w.humidity_ok) {
          TempHumDewShow(json, (0 == TasmotaGlobal->tele_period), label, mem->sensor_copy[i].w.temp_c, mem->sensor_copy[i].w.humidity);
        }
        if (mem->sensor_copy[i].w.wind_ok) {
          WSContentSend_PD(GSTR(HTTP_Bresser2), label, &mem->sensor_copy[i].w.wind_gust_meter_sec, label, &mem->sensor_copy[i].w.wind_avg_meter_sec, label, &mem->sensor_copy[i].w.wind_direction_deg);
        }
        if (mem->sensor_copy[i].w.rain_ok) {
          WSContentSend_PD(GSTR(HTTP_Bresser3), label, &mem->sensor_copy[i].w.rain_mm);
        }
        if (mem->sensor_copy[i].w.uv_ok) {
          WSContentSend_PD(GSTR(HTTP_Bresser4), label, &mem->sensor_copy[i].w.uv);
        }
        if (mem->sensor_copy[i].w.light_ok) {
          WSContentSend_PD(GSTR(HTTP_Bresser5), label, &mem->sensor_copy[i].w.light_klx);
        }
      }
    }
  } else {
    // JSON output
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (iseq(mem->sensor_copy[i].rssi)) continue;
      if (Bresser_reject(mem->sensor_copy[i].sensor_id)) continue;

      ResponseAppend_P(PSTR(",\"Bresser_%1d\":{\"ID\":\"%08x\",\"Type\":%x,\"Chan\":%d,\"Stat\":%d,\"Batt\":\"%-3s\",\"RSSI\":%1_f,\"RCNT\":%d"),
        i + 1, (int)mem->sensor_copy[i].sensor_id, mem->sensor_copy[i].s_type, mem->sensor_copy[i].chan, mem->sensor_copy[i].startup, mem->sensor_copy[i].battery_ok ? "OK " : "Low", &mem->sensor_copy[i].rssi, mem->sensor_copy[i].rec_count);

      if (mem->sensor_copy[i].s_type == SENSOR_TYPE_SOIL) {
        ResponseAppend_P(PSTR(",\"STEMP\":%1_f,\"SMOIST\":%d"), &mem->sensor_copy[i].soil.temp_c, mem->sensor_copy[i].soil.moisture);
      } else if (mem->sensor_copy[i].s_type == SENSOR_TYPE_POOL_THERMO) {
        if (mem->sensor_copy[i].w.temp_ok) {
          ResponseAppend_P(PSTR(",\"Temp\":%1_f"), &mem->sensor_copy[i].w.temp_c);
        }
      } else if (mem->sensor_copy[i].s_type == SENSOR_TYPE_THERMO_HYGRO) {
        if (mem->sensor_copy[i].w.temp_ok) {
          ResponseAppend_P(PSTR(",\"Temp\":%1_f"), &mem->sensor_copy[i].w.temp_c);
        }
        if (mem->sensor_copy[i].w.humidity_ok) {
          ResponseAppend_P(PSTR(",\"Hum\":%d"), mem->sensor_copy[i].w.humidity);
        }
      } else {
        if (mem->sensor_copy[i].w.temp_ok) {
          ResponseAppend_P(PSTR(",\"Temp\":%1_f"), &mem->sensor_copy[i].w.temp_c);
        }
        if (mem->sensor_copy[i].w.humidity_ok) {
          ResponseAppend_P(PSTR(",\"Hum\":%d"), mem->sensor_copy[i].w.humidity);
        }
        if (mem->sensor_copy[i].w.wind_ok) {
          ResponseAppend_P(PSTR(",\"WMAX\":%1_f,\"WAVG\":%1_f,\"CWDIR\":%1_f"), &mem->sensor_copy[i].w.wind_gust_meter_sec, &mem->sensor_copy[i].w.wind_avg_meter_sec, &mem->sensor_copy[i].w.wind_direction_deg);
        }
        if (mem->sensor_copy[i].w.rain_ok) {
          ResponseAppend_P(PSTR(",\"Rain\":%1_f"), &mem->sensor_copy[i].w.rain_mm);
        }
        if (mem->sensor_copy[i].w.uv_ok) {
          ResponseAppend_P(PSTR(",\"UVidx\":%1_f"), &mem->sensor_copy[i].w.uv);
        }
        if (mem->sensor_copy[i].w.light_ok) {
          ResponseAppend_P(PSTR(",\"Light\":%1_f"), &mem->sensor_copy[i].w.light_klx);
        }
      }

      ResponseJsonEnd();
    }
  }
}

/*********************************************************************************************\
 * Main Functions
\*********************************************************************************************/

int32_t Bresser_Init() {
  ALLOCMEM
  STGLOB

  mem->found = 0;
  mem->ready = 0;
  mem->decode_status = DECODE_INVALID;
  memset(mem->reject_ids, 0, sizeof(mem->reject_ids));

  // Get pin assignments from MODULE_DESCRIPTOR
  mem->cs_pin = mp->ms[0].value;
  mem->gdo0_pin = mp->ms[1].value;

  if (mem->cs_pin == 0 || mem->gdo0_pin == 0) {
    AddLog(LOG_LEVEL_ERROR, PSTR("BRESSER: No CS or GDO0 pin configured"));
    Bresser_Deinit();
    return false;
  }

  pinMode(mem->cs_pin, OUTPUT);
  digitalWrite(mem->cs_pin, HIGH);
  pinMode(mem->gdo0_pin, INPUT);

  // Initialize SPI
  GETSPI(0);
  spi_begin();

  // Initialize CC1101
  int16_t res = cc1101_init_bresser();
  if (res != 0) {
    AddLog(LOG_LEVEL_ERROR, PSTR("BRESSER: CC1101 init failed"));
    Bresser_Deinit();
    return false;
  }

  // Clear all sensor data
  clearSlots();
  mem->found = 1;
  mem->ready = 1;
  initialized = true;

  AddLog(LOG_LEVEL_INFO, PSTR("BRESSER: Init OK, CS=%d GDO0=%d"), mem->cs_pin, mem->gdo0_pin);
  return true;
}

MODULE_PART void Bresser_Task() {
  SETMEMREGS
  if (mem->ready) {
    DecodeStatus res = Bresser_getMessage();
    if (res == DECODE_OK) {
      AddLog(LOG_LEVEL_DEBUG, PSTR("BRESSER: Received message!"));
      mem->decode_status = DECODE_OK;
      memmove(mem->sensor_copy, mem->sensor, sizeof(mem->sensor));
    }
  }
}

MODULE_PART bool Bresser_Cmd() {
  SETMEMREGS
  char *cp = XdrvMailbox->data;
  if (XdrvMailbox->data_len > 0) {
    if (*cp == 'r') {
      cp++;
      uint8_t index = *cp & 7;
      if (index < 1 || index > MAX_REJIDS) index = 1;
      cp++;
      while (*cp == ' ') cp++;
      if (*cp == '0' && *(cp + 1) == 'x') {
        cp += 2;
        mem->reject_ids[index - 1] = strtoul(cp, &cp, 16);
        ResponseTime_P(PSTR(",\"Bresser\":{\"CMD\":\"remove: %0x\"}}"), mem->reject_ids[index - 1]);
        return true;
      }
    }
  }
  return false;
}

void Bresser_Deinit() {
  SETREGS
  if (mem->ready) {
    cc1101_cmd_strobe(CC1101_CMD_SIDLE);
    cc1101_cmd_strobe(CC1101_CMD_SPWD);
  }
  spi_end();
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

MOD_RESULT mod_func_execute(uint32_t sel) {
  MOD_RESULT result = false;
  switch (sel) {
    case pFUNC_INIT:
      result = Bresser_Init();
      break;
    case pFUNC_EVERY_100_MSECOND:
      Bresser_Task();
      break;
    case pFUNC_JSON_APPEND:
      Bresser_Show(1);
      break;
    case pFUNC_WEB_SENSOR:
      Bresser_Show(0);
      break;
    case pFUNC_COMMAND_SENSOR:
      result = Bresser_Cmd();
      break;
    case pFUNC_DEINIT:
      Bresser_Deinit();
      break;
  }
  return result;
}

PULL_OPTIONS
#endif  // USE_BRESSER_MOD
