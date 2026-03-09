/*
  xsns_96_cc1101.ino - CC1101 radio_modem support

  Copyright (C) 2024  Gerhard Mutz and Matthias Prinke (BresserWetherSensorReceiver)

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


#ifdef USE_SPI
#ifdef USE_CC1101_BRESSER

#define USE_CC1101

// Uncomment to enable CC1101 diagnostic logging (register dump, RSSI, frequency test)
//#define CC1101_DIAG

#define XSNS_125 125

#include <WeatherSensorCfg.h>
#include <WeatherSensor.h>

#ifdef CC1101_DIAG
extern CC1101 radio;
#endif

WeatherSensor ws;

#define MAX_REJIDS 4
uint32_t bresser_reject_ids[MAX_REJIDS];

struct WS_Sensor {
    uint32_t sensor_id;        //!< sensor ID (5-in-1: 1 byte / 6-in-1: 4 bytes / 7-in-1: 2 bytes)
    float    rssi;             //!< received signal strength indicator in dBm
    uint8_t  s_type;           //!< sensor type
    uint8_t  chan;             //!< channel
    uint8_t  rec_count;
    bool     startup;          //!< startup after reset / battery change
    bool     battery_ok;       //!< battery o.k.
    bool     valid;            //!< data valid (but not necessarily complete)
    bool     complete;         //!< data is split into two separate messages is complete (only 6-in-1 WS)
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

struct CC1101_BRESSER {
    uint8_t found;
    uint8_t cs;
    uint8_t ready;
    SPISettings spi_settings;
    uint8_t decode_status;
#ifdef CC1101_DIAG
    uint8_t diag_counter;
    bool diag_regs_dumped;      // one-time register dump done
    uint32_t diag_rx_count;     // count of diagnostic samples
    int diag_rssi_min;          // track min RSSI seen (best signal)
#endif
} cc1101_bresser;


// Helper: write a single CC1101 config register via direct SPI
static void cc1101_write_reg(uint8_t addr, uint8_t val) {
    SPI.beginTransaction(cc1101_bresser.spi_settings);
    digitalWrite(cc1101_bresser.cs, LOW);
    SPI.transfer(addr);
    SPI.transfer(val);
    digitalWrite(cc1101_bresser.cs, HIGH);
    SPI.endTransaction();
}

// Helper: read a single CC1101 register via direct SPI
// For config registers (0x00-0x2E): use addr | 0x80
// For status registers (0x30-0x3D): use addr | 0xC0
static uint8_t cc1101_read_reg(uint8_t addr) {
    SPI.beginTransaction(cc1101_bresser.spi_settings);
    digitalWrite(cc1101_bresser.cs, LOW);
    SPI.transfer(addr);
    uint8_t val = SPI.transfer(0);
    digitalWrite(cc1101_bresser.cs, HIGH);
    SPI.endTransaction();
    return val;
}

// Helper: send CC1101 strobe command
static void cc1101_strobe(uint8_t cmd) {
    SPI.beginTransaction(cc1101_bresser.spi_settings);
    digitalWrite(cc1101_bresser.cs, LOW);
    SPI.transfer(cmd);
    digitalWrite(cc1101_bresser.cs, HIGH);
    SPI.endTransaction();
}

#ifdef CC1101_DIAG

// Helper: read CC1101 RSSI and convert to dBm
static int cc1101_read_rssi(void) {
    int8_t rssi_raw = (int8_t)cc1101_read_reg(0x34 | 0xC0);
    return (rssi_raw >= 128) ? ((rssi_raw - 256) / 2 - 74) : (rssi_raw / 2 - 74);
}

// Helper: burst read CC1101 registers
static void cc1101_burst_read(uint8_t start_addr, uint8_t *buf, uint8_t len) {
    SPI.beginTransaction(cc1101_bresser.spi_settings);
    digitalWrite(cc1101_bresser.cs, LOW);
    SPI.transfer(start_addr | 0xC0);  // burst read
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = SPI.transfer(0);
    }
    digitalWrite(cc1101_bresser.cs, HIGH);
    SPI.endTransaction();
}
#endif  // CC1101_DIAG


void CC1101_Bresser_Init(void) {
  cc1101_bresser.found = 0;
  cc1101_bresser.cs = 0;
  cc1101_bresser.ready = 0;

  if (PinUsed(GPIO_SPI_MOSI) && PinUsed(GPIO_SPI_MISO) && PinUsed(GPIO_SPI_CLK) && PinUsed(GPIO_SPI_CS) && PinUsed(GPIO_CC1101_GDO0)) {
    cc1101_bresser.cs = Pin(GPIO_SPI_CS);
    cc1101_bresser.found = 1;
  } else {
    return;
  }

  pinMode(cc1101_bresser.cs, OUTPUT);
  digitalWrite(cc1101_bresser.cs, 1);
  AddLog(LOG_LEVEL_INFO, PSTR("CC1101: cs = %d"), cc1101_bresser.cs);

#ifndef ESP32
  SPI.begin();
#else
  SPI.begin(Pin(GPIO_SPI_CLK), Pin(GPIO_SPI_MISO), Pin(GPIO_SPI_MOSI), -1);
#endif

    cc1101_bresser.spi_settings = SPISettings(5000000, MSBFIRST, SPI_MODE0);  // was SPI_MODE3

 // rec_cs, rec_irq, rec_res, rec_gpio
    ws.begin(cc1101_bresser.cs, Pin(GPIO_CC1101_GDO0), -1, -1);

    // Improve CC1101 sensitivity after RadioLib init
    // Only change AGC settings - keep RadioLib's BW/IF/freq untouched
    cc1101_strobe(0x36);  // SIDLE - must be idle to write config registers
    delayMicroseconds(100);

    // AGC: enable all gain stages for maximum sensitivity
    cc1101_write_reg(0x1B, 0x03);  // AGCCTRL2: MAX_DVGA_GAIN=00 (all enabled), MAX_LNA_GAIN=000, MAGN_TARGET=33dB
    cc1101_write_reg(0x1C, 0x40);  // AGCCTRL1: LNA priority, no carrier sense threshold
    cc1101_write_reg(0x1D, 0x91);  // AGCCTRL0: medium hysteresis, 16 samples

    // Restart RX with new settings
    cc1101_strobe(0x3A);  // SFRX - flush RX FIFO
    cc1101_strobe(0x34);  // SRX  - enter receive mode
    AddLog(LOG_LEVEL_INFO, PSTR("CC1101: AGC optimized for max sensitivity"));

#ifdef CC1101_DIAG
    // Diagnostic: dump key CC1101 registers after init (one-time, before RX matters)
    int16_t ver = radio.getChipVersion();
    uint8_t iocfg0 = cc1101_read_reg(0x02 | 0x80);
    uint8_t marcstate = cc1101_read_reg(0x35 | 0xC0) & 0x1F;
    AddLog(LOG_LEVEL_INFO, PSTR("CC1101 DIAG: version=0x%02X GDO0_pin=%d IOCFG0=0x%02X MARCSTATE=0x%02X"),
        ver, Pin(GPIO_CC1101_GDO0), iocfg0, marcstate);

    // Read config registers while still in init (safe to strobe IDLE here)
    cc1101_strobe(0x36);  // SIDLE
    delayMicroseconds(100);
    uint8_t cfgregs[8];
    cc1101_burst_read(0x0D, cfgregs, 8);  // FREQ2..MDMCFG0
    uint8_t syncreg[2];
    cc1101_burst_read(0x04, syncreg, 2);  // SYNC1, SYNC0
    uint8_t deviatn = cc1101_read_reg(0x15 | 0x80);
    AddLog(LOG_LEVEL_INFO, PSTR("CC1101 DIAG: FREQ=0x%02X%02X%02X SYNC=0x%02X%02X"),
        cfgregs[0], cfgregs[1], cfgregs[2], syncreg[0], syncreg[1]);
    AddLog(LOG_LEVEL_INFO, PSTR("CC1101 DIAG: MDMCFG4=0x%02X MDMCFG3=0x%02X MDMCFG2=0x%02X MDMCFG1=0x%02X DEVIATN=0x%02X"),
        cfgregs[3], cfgregs[4], cfgregs[5], cfgregs[6], deviatn);

    // Restart RX after register dump
    radio.startReceive();

    cc1101_bresser.diag_counter = 0;
    cc1101_bresser.diag_regs_dumped = true;
    cc1101_bresser.diag_rx_count = 0;
    cc1101_bresser.diag_rssi_min = 0;  // 0 dBm = no signal seen yet
#endif

    // Clear all sensor data
    ws.clearSlots();
    cc1101_bresser.decode_status = DECODE_INVALID;
    cc1101_bresser.ready = 1;
}


void CC1101_Bresser_task(void) {
    if (cc1101_bresser.ready) {

#ifdef CC1101_DIAG
        // NON-INVASIVE periodic diagnostic every ~10 seconds (100 * 100ms)
        // Only reads status registers — does NOT strobe IDLE, change frequency, or call startReceive
        cc1101_bresser.diag_counter++;
        if (cc1101_bresser.diag_counter >= 100) {
            cc1101_bresser.diag_counter = 0;
            cc1101_bresser.diag_rx_count++;

            // Status registers can be read safely during RX (addr | 0xC0)
            uint8_t marcstate = cc1101_read_reg(0x35 | 0xC0) & 0x1F;
            int rssi = cc1101_read_rssi();
            int gdo0_state = digitalRead(Pin(GPIO_CC1101_GDO0));

            // Track best (strongest) RSSI seen
            if (cc1101_bresser.diag_rssi_min == 0 || rssi > cc1101_bresser.diag_rssi_min) {
                cc1101_bresser.diag_rssi_min = rssi;
            }

            AddLog(LOG_LEVEL_INFO, PSTR("CC1101 DIAG: MARC=0x%02X GDO0=%d RSSI=%ddBm best=%ddBm #%d"),
                marcstate, gdo0_state, rssi, cc1101_bresser.diag_rssi_min, cc1101_bresser.diag_rx_count);

            // Only intervene if chip fell out of RX mode
            if (marcstate != 0x0D) {
                AddLog(LOG_LEVEL_INFO, PSTR("CC1101 DIAG: NOT in RX (0x%02X), restarting receive"), marcstate);
                radio.startReceive();
            }
        }
#endif  // CC1101_DIAG

        // Tries to receive radio message (non-blocking) and to decode it.
        // Timeout occurs after a small multiple of expected time-on-air.

        if (ws.getMessage() == DECODE_OK) {
            AddLog(LOG_LEVEL_DEBUG, PSTR("received bresser meessage!"));
            cc1101_bresser.decode_status = DECODE_OK;
            memmove(ws.sensor_copy, ws.sensor, sizeof(ws.sensor));
        }
    }
}


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

bool Bresser_reject(uint32_t id) {
    for (int r = 0; r < MAX_REJIDS; r++) {
        if (bresser_reject_ids[r] > 0) {
            if (bresser_reject_ids[r] == id) {
                return true;
            }
        }
    }
    return false;
}


void C1101_Bresser_Show(boolean json) {
    if (cc1101_bresser.decode_status != DECODE_OK) {
        return;
    }

    // probably should sort sensors here

    bresser_bubbleSort((struct WS_Sensor*)&ws.sensor_copy[0], NUM_SENSORS);

    if (!json) {

        for (int i = 0; i < NUM_SENSORS; i++) {

            if (Bresser_reject(ws.sensor_copy[i].sensor_id)) {
                continue;
            }

            if (ws.sensor_copy[i].rssi == 0) {
                continue;
            }

            char label[16];
            sprintf_P(label,PSTR("Bresser %1d"), i + 1);

            WSContentSend_PD(HTTP_Bresser1, label, static_cast<int> (ws.sensor_copy[i].sensor_id), label,  ws.sensor_copy[i].s_type, label, ws.sensor_copy[i].chan, label, ws.sensor_copy[i].startup, label, ws.sensor_copy[i].battery_ok ? "OK " : "Low", label, &ws.sensor_copy[i].rssi, label, ws.sensor_copy[i].rec_count);

            if (ws.sensor_copy[i].s_type == SENSOR_TYPE_SOIL) {
                WSContentSend_PD(HTTP_Bresser6, label, &ws.sensor_copy[i].soil.temp_c, label, ws.sensor_copy[i].soil.moisture);
            } else if (ws.sensor_copy[i].s_type == SENSOR_TYPE_POOL_THERMO) {
                WSContentSend_PD(HTTP_Bresser7, label, &ws.sensor_copy[i].w.temp_c);
            } else if (ws.sensor_copy[i].s_type == SENSOR_TYPE_THERMO_HYGRO) {
                if ((ws.sensor_copy[i].w.temp_ok) && (ws.sensor_copy[i].w.humidity_ok)) {
                    TempHumDewShow(json, (0 == TasmotaGlobal.tele_period), label, ws.sensor_copy[i].w.temp_c, ws.sensor_copy[i].w.humidity);
                }
            } else {

                if ((ws.sensor_copy[i].w.temp_ok) && (ws.sensor_copy[i].w.humidity_ok)) {
                    TempHumDewShow(json, (0 == TasmotaGlobal.tele_period), label, ws.sensor_copy[i].w.temp_c, ws.sensor_copy[i].w.humidity);
                }

                if (ws.sensor_copy[i].w.wind_ok) {
                    WSContentSend_PD(HTTP_Bresser2, label, &ws.sensor_copy[i].w.wind_gust_meter_sec, label, &ws.sensor_copy[i].w.wind_avg_meter_sec, label, &ws.sensor_copy[i].w.wind_direction_deg);
                }

                if (ws.sensor_copy[i].w.rain_ok) {
                    WSContentSend_PD(HTTP_Bresser3, label, &ws.sensor_copy[i].w.rain_mm);
                }

#if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
                if (ws.sensor_copy[i].w.uv_ok) {
                    WSContentSend_PD(HTTP_Bresser4, label, &ws.sensor_copy[i].w.uv);
                }
#endif

#ifdef BRESSER_7_IN_1
                if (ws.sensor_copy[i].w.light_ok) {
                    WSContentSend_PD(HTTP_Bresser5, label, &ws.sensor_copy[i].w.light_klx);
                }
#endif
            }
        }

    } else {

        for (int i = 0; i < NUM_SENSORS; i++) {

            if (ws.sensor_copy[i].rssi == 0) {
                continue;
            }

            if (Bresser_reject(ws.sensor_copy[i].sensor_id)) {
                continue;
            }

            ResponseAppend_P(PSTR(",\"Bresser_%1d\":{\"ID\":\"%08x\",\"Type\":%x,\"Chan\":%d,\"Stat\":%d,\"Batt\":\"%-3s\",\"RSSI\":%1_f,\"RCNT\":%d"),\
                i + 1, static_cast<int> (ws.sensor_copy[i].sensor_id), ws.sensor_copy[i].s_type, ws.sensor_copy[i].chan, ws.sensor_copy[i].startup, ws.sensor_copy[i].battery_ok ? "OK " : "Low", &ws.sensor_copy[i].rssi, ws.sensor_copy[i].rec_count);


            if (ws.sensor_copy[i].s_type == SENSOR_TYPE_SOIL) {
                ResponseAppend_P(PSTR(",\"STEMP\":%1_f,\"SMOIST\":%d"), &ws.sensor_copy[i].soil.temp_c, ws.sensor_copy[i].soil.moisture);
            } else if (ws.sensor_copy[i].s_type == SENSOR_TYPE_POOL_THERMO) {
                if (ws.sensor_copy[i].w.temp_ok) {
                    ResponseAppend_P(PSTR(",\"Temp\":%1_f"), &ws.sensor_copy[i].w.temp_c);
                }
            } else if (ws.sensor_copy[i].s_type == SENSOR_TYPE_THERMO_HYGRO) {
                if (ws.sensor_copy[i].w.temp_ok) {
                    ResponseAppend_P(PSTR(",\"Temp\":%1_f"), &ws.sensor_copy[i].w.temp_c);
                }
                if (ws.sensor_copy[i].w.humidity_ok) {
                    ResponseAppend_P(PSTR(",\"Hum\":%d"), ws.sensor_copy[i].w.humidity);
                }
            } else {

                if (ws.sensor_copy[i].w.temp_ok) {
                    ResponseAppend_P(PSTR(",\"Temp\":%1_f"), &ws.sensor_copy[i].w.temp_c);
                }

                if (ws.sensor_copy[i].w.humidity_ok) {
                    ResponseAppend_P(PSTR(",\"Hum\":%d"), ws.sensor_copy[i].w.humidity);
                }

                if (ws.sensor_copy[i].w.wind_ok) {
                    ResponseAppend_P(PSTR(",\"WMAX\":%1_f,\"WAVG\":%1_f,\"CWDIR\":%1_f"), &ws.sensor_copy[i].w.wind_gust_meter_sec, &ws.sensor_copy[i].w.wind_avg_meter_sec, &ws.sensor_copy[i].w.wind_direction_deg);
                }
                if (ws.sensor_copy[i].w.rain_ok) {
                    ResponseAppend_P(PSTR(",\"Rain\":%1_f"), &ws.sensor_copy[i].w.rain_mm);
                }

#if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
                if (ws.sensor_copy[i].w.uv_ok) {
                    ResponseAppend_P(PSTR(",\"UVidx\":%1_f"), &ws.sensor_copy[i].w.uv);
                }
#endif

#if defined BRESSER_7_IN_1
                if (ws.sensor_copy[i].w.light_ok) {
                    ResponseAppend_P(PSTR(",\"Light\":%1_f"), &ws.sensor_copy[i].w.light_klx);
                }
#endif
            }

            ResponseJsonEnd();
        }
    }
}



// sort by sensor type
void bresser_bubbleSort(struct WS_Sensor *sens, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (!sens[i].rssi) sens[i].s_type = 99;
    }
    for (uint32_t i = 0; i < n - 1; i++) {
        bool swapped = false;
        for (uint32_t j = 0; j < n - i - 1; j++) {
            if (sens[j].s_type && (sens[j].s_type > sens[j + 1].s_type)) {
                //swap
                uint8_t temp[sizeof(ws.sensor)];
                memmove(&temp, &sens[j], sizeof(ws.sensor));
                memmove(&sens[j], &sens[j + 1], sizeof(ws.sensor));
                memmove(&sens[j + 1], &temp, sizeof(ws.sensor));
                swapped = true;
            }
        }
        if (swapped == false) {
            break;
        }
    }
}



bool XSNS_125_cmd(void) {
    if (XdrvMailbox.data_len > 0) {
        char *cp = XdrvMailbox.data;
        if (*cp == 'r') {
            cp++;
            uint8_t index = *cp & 7;
            if (index < 1 || index > MAX_REJIDS) index = 1;
            cp++;
            while (*cp == ' ') cp++;
            if (*cp == '0' && *(cp + 1) == 'x') {
                cp += 2;
                bresser_reject_ids[index - 1] = strtoll(cp, &cp, 16);
                ResponseTime_P(PSTR(",\"Bresser\":{\"CMD\":\"remove: %0x\"}}"), bresser_reject_ids[index - 1]);
                return true;
            }
        }
    }
    return false;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns125(uint32_t function) {
  bool result = false;

  switch (function) {
      case FUNC_INIT:
        CC1101_Bresser_Init();
        break;
      case FUNC_EVERY_100_MSECOND:
        CC1101_Bresser_task();
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        C1101_Bresser_Show(0);
        break;
#endif  // USE_WEBSERVER
      case FUNC_JSON_APPEND:
        C1101_Bresser_Show(1);
        break;
      case FUNC_COMMAND_SENSOR:
        if (XSNS_125 == XdrvMailbox.index) {
            result = XSNS_125_cmd();
        }
        break;
  }
  return result;
}

#endif  // USE_CC1101_Bresser
#endif  // USE_SPI
