/*
  xsns_96_cc1101.ino - CC1101 radio_modem support

  Copyright (C) 2020  Gerhard Mutz and Rudolf Koenig (culfw)

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


#include <WeatherSensorCfg.h>
#include <WeatherSensor.h>

#ifdef USE_SPI
#ifdef USE_CC1101_BRESSER

#define XSNS_125 125

WeatherSensor ws;

struct CC1101_BRESSER {
    uint8_t found;
    uint8_t cs;
    uint8_t ready;
    SPISettings spi_settings;
    uint8_t decode_status;
} cc1101_bresser;


void CC1101_Bresser_Detect(void) {
  cc1101_bresser.found = 0;
  cc1101_bresser.cs = 0;
  cc1101_bresser.ready = 0;

  if (Pin(GPIO_SPI_MOSI) && Pin(GPIO_SPI_MISO) && Pin(GPIO_SPI_CLK) && Pin(GPIO_SPI_CS) && Pin(GPIO_CC1101_GDO0)) {
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

    cc1101_bresser.spi_settings = SPISettings(5000000, MSBFIRST, SPI_MODE3);

 // rec_cs, rec_irq, rec_res, rec_gpio
    ws.begin(cc1101_bresser.cs, Pin(GPIO_CC1101_GDO0), -1, -1);
    cc1101_bresser.decode_status = DECODE_INVALID;
    cc1101_bresser.ready = 1;
}


void CC1101_Bresser_task(void) {
    if (cc1101_bresser.ready) {
        // Tries to receive radio message (non-blocking) and to decode it.
        // Timeout occurs after a small multiple of expected time-on-air.
        // Clear all sensor data
        ws.clearSlots();

        if (ws.getMessage() == DECODE_OK) {
            AddLog(LOG_LEVEL_DEBUG, PSTR("received bresser meessage!"));
            cc1101_bresser.decode_status = DECODE_OK;
            memmove(ws.sensor_copy, ws.sensor, sizeof(ws.sensor));
        }
    }
}


const char HTTP_Bresser1[] PROGMEM =
 "{s}%s ID" "{m}%8x" "{e}"
 "{s}%s Type" "{m}%x" "{e}"
 "{s}%s Chan" "{m}%d" "{e}"
 "{s}%s Stat" "{m}%d" "{e}"
 "{s}%s Batt" "{m}%-3s" "{e}"
 "{s}%s RSSI" "{m}%1_f dBm" "{e}"
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

void C1101_Bresser_Show(boolean json) {
    if (cc1101_bresser.decode_status != DECODE_OK) {
        return;
    }

    // This example uses only a single slot in the sensor data array
    int const i = 0;

    if (!json) {

        for (int i = 0; i < NUM_SENSORS; i++) {

            if (ws.sensor_copy[i].rssi == 0) {
                continue;
            }

            char label[16];
            sprintf_P(label,PSTR("Bresser %1d"), i + 1);

            WSContentSend_PD(HTTP_Bresser1, label, static_cast<int> (ws.sensor_copy[i].sensor_id), label,  ws.sensor_copy[i].s_type, label, ws.sensor_copy[i].chan, label, ws.sensor_copy[i].startup, label, ws.sensor_copy[i].battery_ok ? "OK " : "Low", label, &ws.sensor_copy[i].rssi);

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

            ResponseAppend_P(PSTR(",\"Bresser %1d\":{\"ID\":%8x,\"Type\":%x,\"Chan\":%d,\"Stat\":%d,\"Batt\":\"%-3s\",\"RSSI\":%1_f"),\
                i + 1, static_cast<int> (ws.sensor_copy[i].sensor_id), ws.sensor_copy[i].s_type, ws.sensor_copy[i].chan, ws.sensor_copy[i].startup, ws.sensor_copy[i].battery_ok ? "OK " : "Low", &ws.sensor_copy[i].rssi);
        

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

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns125(uint32_t function) {
  bool result = false;

  switch (function) {
      case FUNC_INIT:
        CC1101_Bresser_Detect();
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
  }
  return result;
}

#endif  // USE_CC1101_Bresser
#endif  // USE_SPI
