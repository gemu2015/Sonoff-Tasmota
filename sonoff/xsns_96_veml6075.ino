/*
  xsns_96_veml6075.ino - VEML6075

  Copyright (C) 2018  Theo Arends and Rui Marinho

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

#ifdef USE_I2C
#ifdef USE_VEML6075

#define XSNS_96 96

/*********************************************************************************************\
 * VEML6075
 *
 * Source:
 *
 * I2C Address: 0x10
\*********************************************************************************************/

#include "SparkFun_VEML6075_Arduino_Library.h"
VEML6075 uv_sensor;

uint8_t veml6075_ready = 0;
uint16_t uva_level;
uint16_t uvb_level;
float uv_index;

/********************************************************************************************/

void Veml6075Detect()
{
  if (veml6075_ready) {
    AddLog_P(LOG_LEVEL_DEBUG, PSTR("VEML6075 is ready"));
    return;
  }

  if (!uv_sensor.begin()) {
    AddLog_P(LOG_LEVEL_DEBUG, PSTR("Unable to init uv_sensor"));
    return;
  }

  veml6075_ready = 1;

  snprintf_P(log_data, sizeof(log_data), S_LOG_I2C_FOUND_AT, "VEML6075", VEML6075_ADDRESS);
  AddLog(LOG_LEVEL_DEBUG);
}

void Veml6075_Every_Second(void) {
  if (veml6075_ready) {
    uva_level = uv_sensor.a();
    uvb_level = uv_sensor.b();
    uv_index = uv_sensor.index();
  } else {
    if (uptime%60==12) {
      Veml6075Detect();
    }
  }
}

#define D_UV_INDEX "UV-Index"
#define D_UVA_LEVEL "UVA-Level"
#define D_UVB_LEVEL "UVB-Level"
#define D_JSON_UV_INDEX "UvIndex"
#define D_JSON_UVA_LEVEL "UvALevel"
#define D_JSON_UVB_LEVEL "UvBLevel"

#ifdef USE_WEBSERVER
const char HTTP_SNS_VEML6075[] PROGMEM =
  "{s}VEML6075 " D_UVA_LEVEL "{m}%d{e}"  // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
  "{s}VEML6075 " D_UVB_LEVEL "{m}%d{e}"
  "{s}VEML6075 " D_UV_INDEX "{m}%s{e}";
#endif  // USE_WEBSERVER

void Veml6075Show(boolean json)
{
  if (!veml6075_ready) {
    return;
  }
  char uv_index_str[16];
  dtostrfd(uv_index,3,uv_index_str);

  if (json) {
    ResponseAppend_P(PSTR(",\"VEML6075\":{\"" D_JSON_UVA_LEVEL "\":%d,\"" D_JSON_UVB_LEVEL "\":%d,\"" D_JSON_UV_INDEX "\":%s}"), uva_level, uvb_level, uv_index_str);
    //snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"VEML6075\":{\"" D_JSON_UVA_LEVEL "\":%d,\"" D_JSON_UVB_LEVEL "\":%d,\"" D_JSON_UV_INDEX "\":%s}"), mqtt_data, uva_level, uvb_level, uv_index);
#ifdef USE_WEBSERVER
  } else {
    WSContentSend_PD(HTTP_SNS_VEML6075, uva_level, uvb_level, uv_index_str);
    //snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_SNS_VEML6075, mqtt_data, uva_level, uvb_level, uv_index);
#endif
  }
}

VEML6075_error_t setIntegrationTime(VEML6075::veml6075_uv_it_t time)
{
  snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_SENSOR_INDEX_SVALUE, XSNS_96, "Updating integration time");

  return uv_sensor.setIntegrationTime(time);
}

VEML6075_error_t setHighDynamicMode(VEML6075::veml6075_hd_t mode)
{
  snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_SENSOR_INDEX_SVALUE, XSNS_96, "Updating high dynamic mode");

  return uv_sensor.setHighDynamic(mode);
}

bool Veml6075Command_sensor()
{
  VEML6075_error_t error;

  switch (XdrvMailbox.payload) {
    case 0: // Set high-dynamic mode to normal
      error = setHighDynamicMode(VEML6075::DYNAMIC_NORMAL);
      break;
    case 1: // Set high-dynamic mode to high
      error = setHighDynamicMode(VEML6075::DYNAMIC_HIGH);
      break;
    case 2: // Set integration time to 50ms
      error = setIntegrationTime(VEML6075::IT_50MS);
      break;
    case 3: // Set integration time to 100ms
      error = setIntegrationTime(VEML6075::IT_100MS);
      break;
    case 4: // Set integration time to 200ms
      error = setIntegrationTime(VEML6075::IT_200MS);
      break;
    case 5: // Set integration time to 400ms
      error = setIntegrationTime(VEML6075::IT_400MS);
      break;
    case 6: // Set integration time to 800ms
      error = setIntegrationTime(VEML6075::IT_800MS);
      break;
  }

  if (error != VEML6075_ERROR_SUCCESS) {
    return false;
  }

  return true;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns96(byte function)
{
  bool result = false;

  if (i2c_flg) {
    switch (function) {
      case FUNC_INIT:
        Veml6075Detect();
        break;
      case FUNC_EVERY_SECOND:
        Veml6075_Every_Second();
        break;
      case FUNC_COMMAND_SENSOR:
        if (XSNS_96 == XdrvMailbox.index) {
          result = Veml6075Command_sensor();
        }
        break;
      case FUNC_JSON_APPEND:
        Veml6075Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        Veml6075Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_VEML6075
#endif  // USE_I2C
