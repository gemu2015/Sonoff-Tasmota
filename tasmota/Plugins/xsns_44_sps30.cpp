/*
  xsns_44_sps30.ino - Sensirion SPS30 support for Tasmota

  Copyright (C) 2021  Gerhard Mutz and Theo Arends

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

#ifdef USE_SPS30_MOD

#include "module.h"
#include "module_defines.h"

#define SPS30_ADDR 0x69

#define SPS_CMD_START_MEASUREMENT 0x0010
#define SPS_CMD_START_MEASUREMENT_ARG 0x0300
#define SPS_CMD_STOP_MEASUREMENT 0x0104
#define SPS_CMD_READ_MEASUREMENT 0x0300
#define SPS_CMD_GET_DATA_READY 0x0202
#define SPS_CMD_AUTOCLEAN_INTERVAL 0x8004
#define SPS_CMD_CLEAN 0x5607
#define SPS_CMD_GET_ACODE 0xd025
#define SPS_CMD_GET_SERIAL 0xd033
#define SPS_CMD_RESET 0xd304
#define SPS_WRITE_DELAY_US 20000
#define SPS_MAX_SERIAL_LEN 32

// all memory must be in struct MODULE_MEMORY
typedef struct {
  float PM1_0;
  float PM2_5;
  float PM4_0;
  float PM10;
  float NCPM0_5;
  float NCPM1_0;
  float NCPM2_5;
  float NCPM4_0;
  float NCPM10;
  float TYPSIZ;
  bool sps30_running;
  bool ready;
} MODULE_MEMORY;

#define PM1_0 mem->PM1_0
#define PM2_5 mem->PM2_5
#define PM4_0 mem->PM4_0
#define PM10 mem->PM10
#define NCPM0_5 mem->NCPM0_5
#define NCPM1_0 mem->NCPM1_0
#define NCPM2_5 mem->NCPM2_5
#define NCPM4_0 mem->NCPM4_0
#define NCPM10 mem->NCPM10
#define TYPSIZ mem->TYPSIZ
#define sps30_running mem->sps30_running
#define ready mem->ready


#define SPS30_REV  1<<16|1

// all functions must be declared MUDULE_PART
MODULE_DESCRIPTOR("SPS30", MODULE_TYPE_SENSOR, SPS30_REV,"",0,"",0,"",0,"",0)
MODULE_PART int32_t MOD_FUNC(SPS30_Init);
MODULE_PART void MOD_FUNC(SPS30_Every_Second);
MODULE_PART void MOD_FUNC(SPS30_Show, bool json);
MODULE_PART void MOD_FUNC(SPS30_Deinit);
MODULE_PART uint8_t sps30_calc_CRC(uint8_t *data);
MODULE_PART int32_t MOD_FUNC(mod_func_execute, uint32_t sel);
MODULE_END

// all text defs must appear here
DPSTR(HTTP_SNS_SPS30,"{s}SGP30 eCO2 {m}%d ppm {e}{s}SGP30 TVOC {m}%d ppb {e}");
DPSTR(JSON_SNS_SPS30,",\"SGP30\":{\"ECO2\":%d,\"TVOC\":%d");
DPSTR(SPS30,"SPS30");

const char HTTP_SNS_SPS30_a[] PROGMEM ="{s}SPS30 " "%s" "{m}%s ug/m3{e}";
const char HTTP_SNS_SPS30_b[] PROGMEM ="{s}SPS30 " "%s" "{m}%s #/cm3{e}";
const char HTTP_SNS_SPS30_c[] PROGMEM ="{s}SPS30 " "TYPSIZ" "{m}%s um{e}";


/********************************************************************************************/

uint8_t sps30_calc_CRC(uint8_t *data) {
    uint8_t crc = 0xFF;
    for (uint32_t i = 0; i < 2; i++) {
        crc ^= data[i];
        for (uint32_t bit = 8; bit > 0; --bit) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ 0x31u;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}


#ifdef ESP8266
unsigned char twi_readFrom(unsigned char address, unsigned char* buf, unsigned int len, unsigned char sendStop);
#endif

void MOD_FUNC(sps30_get_data, uint16_t cmd, uint8_t *data, uint8_t dlen) {
  SETREGS
  unsigned char cmdb[2];
  uint8_t tmp[3];
  uint8_t index=0;
  memset(data,0,dlen);
  uint8_t twi_buff[64];

  Wire.beginTransmission(SPS30_ADDR);
  cmdb[0]=cmd>>8;
  cmdb[1]=cmd;
  Wire.write(cmdb,2);
  Wire.endTransmission();

  // need 60 bytes max
  dlen/=2;
  dlen*=3;

#ifdef ESP8266
  twi_readFrom(SPS30_ADDR,twi_buff,dlen,1);
#endif  // ESP8266
#ifdef ESP32
  Wire.requestFrom((uint16_t)SPS30_ADDR, dlen, true);
  Wire.readBytes(twi_buff, dlen);
#endif  // ESP32

  uint8_t bind=0;
  while (bind<dlen) {
    tmp[0] = twi_buff[bind++];
    tmp[1] = twi_buff[bind++];
    tmp[2] = twi_buff[bind++];
    if (sps30_calc_CRC(tmp)!=tmp[2]) {
      // chksum error
      index+=2;
    } else {
      data[index++]=tmp[0];
      data[index++]=tmp[1];
    }
  }
}

void MOD_FUNC(sps30_cmd, uint16_t cmd) {
  SETREGS
  unsigned char cmdb[6];
  Wire.beginTransmission(SPS30_ADDR);
  cmdb[0]=cmd>>8;
  cmdb[1]=cmd;

  if (cmd==SPS_CMD_START_MEASUREMENT) {
    cmdb[2]=SPS_CMD_START_MEASUREMENT_ARG>>8;
    cmdb[3]=SPS_CMD_START_MEASUREMENT_ARG&0xff;
    cmdb[4]=sps30_calc_CRC(&cmdb[2]);
    Wire.write(cmdb,5);
  } else {
    Wire.write(cmdb,2);
  }
  Wire.endTransmission();
}

void MOD_FUNC(SPS30_Detect) {
  ALLOCMEM

  if (!I2cSetDevice(SPS30_ADDR)) { return; }
  uint8_t dcode[32];
  sps30_get_data(SPS_CMD_GET_SERIAL,dcode,sizeof(dcode));
  if(dcode[0] == 0) {
    return;
  }
  AddLog(LOG_LEVEL_DEBUG, PSTR("sps30 found with serial: %s"), dcode);
  sps30_cmd(SPS_CMD_START_MEASUREMENT);
  sps30_running = 1;
  sps30_ready = 1;
  I2cSetActiveFound(SPS30_ADDR, "SPS30");
}




#define PMDP 2

#define SPS30_HOURS Settings->sps30_inuse_hours
//#define SPS30_HOURS sps30_inuse_hours
//uint8_t sps30_inuse_hours;

void MOD_FUNC(SPS30_Every_Second) {
  SETREGS
  
  if (!sps30_running) return;

  if (TasmotaGlobal.uptime%10==0) {
    uint8_t vars[sizeof(float)*10];
    sps30_get_data(SPS_CMD_READ_MEASUREMENT,vars,sizeof(vars));
    float *fp=&sps30_result.PM1_0;

    typedef union {
    uint8_t array[4];
    float value;
    } ByteToFloat;

    ByteToFloat conv;

    for (uint32_t count=0; count<10; count++) {
      for (uint32_t i = 0; i < 4; i++){
        conv.array[3-i] = vars[count*sizeof(float)+i];
      }
      *fp++=conv.value;
    }
  }

  if (TasmotaGlobal.uptime%3600==0 && TasmotaGlobal.uptime>60) {
    // should auto clean once per week runtime
    // so count hours, should be in Settings
    SPS30_HOURS++;
    if (SPS30_HOURS>(7*24)) {
      CmdClean();
      SPS30_HOURS=0;
    }
  }

}

void MODFUNC(SPS30_Show, bool json) {
  if (!sps30_running) { return; }

  char str[64];
  if (json) {
    dtostrfd(sps30_result.PM1_0,PMDP,str);
    ResponseAppend_P(PSTR(",\"SPS30\":{\"" "PM1_0" "\":%s"), str);
    dtostrfd(sps30_result.PM2_5,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "PM2_5" "\":%s"), str);
    dtostrfd(sps30_result.PM4_0,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "PM4_0" "\":%s"), str);
    dtostrfd(sps30_result.PM10,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "PM10" "\":%s"), str);
    dtostrfd(sps30_result.NCPM0_5,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "NCPM0_5" "\":%s"), str);
    dtostrfd(sps30_result.NCPM1_0,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "NCPM1_0" "\":%s"), str);
    dtostrfd(sps30_result.NCPM2_5,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "NCPM2_5" "\":%s"), str);
    dtostrfd(sps30_result.NCPM4_0,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "NCPM4_0" "\":%s"), str);
    dtostrfd(sps30_result.NCPM10,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "NCPM10" "\":%s"), str);
    dtostrfd(sps30_result.TYPSIZ,PMDP,str);
    ResponseAppend_P(PSTR(",\"" "TYPSIZ" "\":%s}"), str);

#ifdef USE_WEBSERVER
  } else {
    dtostrfd(sps30_result.PM1_0,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_a,"PM 1.0",str);
    dtostrfd(sps30_result.PM2_5,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_a,"PM 2.5",str);
    dtostrfd(sps30_result.PM4_0,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_a,"PM 4.0",str);
    dtostrfd(sps30_result.PM10,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_a,"PM 10",str);
    dtostrfd(sps30_result.NCPM0_5,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_b,"NCPM 0.5",str);
    dtostrfd(sps30_result.NCPM1_0,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_b,"NCPM 1.0",str);
    dtostrfd(sps30_result.NCPM2_5,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_b,"NCPM 2.5",str);
    dtostrfd(sps30_result.NCPM4_0,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_b,"NCPM 4.0",str);
    dtostrfd(sps30_result.NCPM10,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_b,"NCPM 10",str);
    dtostrfd(sps30_result.TYPSIZ,PMDP,str);
    WSContentSend_PD(HTTP_SNS_SPS30_c,str);
#endif
  }
}

void MOD_FUNC(CmdCleand) {
  SETREGS
  sps30_cmd(SPS_CMD_CLEAN);
  ResponseTime_P(PSTR(",\"SPS30\":{\"CFAN\":\"true\"}}"));
  MqttPublishTeleSensor();
}

bool MOD_FUNC(SPS30_cmd) {
  SETREGS
  bool serviced = true;
  if (XdrvMailbox.data_len > 0) {
      char *cp=XdrvMailbox.data;
      if (*cp=='c') {
        // clean cmd
        CmdClean();
      } else if (*cp=='0' || *cp=='1') {
        sps30_running=*cp&1;
        sps30_cmd(sps30_running?SPS_CMD_START_MEASUREMENT:SPS_CMD_STOP_MEASUREMENT);
      } else {
        serviced=false;
      }
  }
  Response_P(PSTR("{\"SPS30\":\"%s\"}"), sps30_running?"running":"stopped");

  return serviced;
}

void MOD_FUNC(SPS30_Deinit) {
  SETREGS
  I2cResetActive(SGP30_ADDRESS, 1);
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t MOD_FUNC(mod_func_execute, uint32_t sel) {
  bool result = false;

  switch (sel) {
      case FUNC_INIT:
        result = CALL_MOD_FUNC(SPS30_Init);
        break;
      case FUNC_EVERY_SECOND:
        CALL_MOD_FUNC(SPS30_Every_Second);
        break;
      case FUNC_JSON_APPEND:
        CALL_MOD_FUNC(SPS30_Show, 1);
        break;
      case FUNC_WEB_SENSOR:
        CALL_MOD_FUNC(SPS30_Show, 0);
        break;
      case FUNC_COMMAND_SENSOR:
        result = CALL_MOD_FUNC(SPS30Cmd);
        break;
      case FUNC_DEINIT:
        CALL_MOD_FUNC(SPS30_Deinit);
        break;
  }
  return result;
}

#endif  // USE_SPS30

