/*
  xsns_53_sml.ino - SML,OBIS,EBUS,MODBUS,VBUS,CAN,RAW,COUNTER interface for Tasmota

  Created by Gerhard Mutz on 07.10.11.
  adapted for Tasmota

  Copyright (C) 2024  Gerhard Mutz

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

/* plugin driver to doo
global:
1. tcp mode, ok needs testing
2. crypto mode (ams reader) ok but lib still in main flash because very complex driver


esp32
2. canbus, ok needs testing

*/

#include "tasmota_options.h"

#ifdef USE_SML_M_MOD


#include "module.h"
#include "module_defines.h"

#define XSNS_53 53

// this driver depends on use USE_SCRIPT !!!

// debug counter input to led for counter1 and 2
//#define DEBUG_CNT_LED1 2
//#define DEBUG_CNT_LED1 2

// disable in plugin mode
//#define NO_USE_SML_CANBUS

//#define NO_USE_SML_DECRYPT

// use special no wait serial driver, should be always on
#ifndef ESP32
#define SPECIAL_SS
#endif

// max number of meters , may be adjusted
#ifndef MAX_METERS
#define MAX_METERS 5
#endif


#define 	USE_ESP32_SW_SERIAL

/* additional defines
	USE_ESP32_SW_SERIAL
	default off, uses a special combo driver that allows more then 3 serial ports on ESP32.
	define rec pins as negativ to use software serial

	USE_SML_AUTHKEY
	rarely used , thus off by default
*/

// if you have to save more RAM you may disable these options by defines in user_config_override

#ifndef NO_SML_REPLACE_VARS
// allows to replace values in decoder section with script string variables
#undef SML_REPLACE_VARS
#define SML_REPLACE_VARS
#endif

#ifndef NO_USE_SML_SPECOPT
// allows to define special option 1 for meters that use a direction bit
#undef USE_SML_SPECOPT
#define USE_SML_SPECOPT
#endif

#ifndef NO_USE_SML_SCRIPT_CMD
// allows several sml cmds from scripts, as well as access to sml registers
#undef USE_SML_SCRIPT_CMD
#define USE_SML_SCRIPT_CMD
#endif


#ifndef NO_USE_SML_DECRYPT
// allows 256 bit AES decryption
#define USE_SML_DECRYPT
#endif

#ifndef NO_USE_SML_TCP
// modbus over TCP
#define USE_SML_TCP
#endif

#ifndef NO_SML_OBIS_LINE
// obis in line mode
#define SML_OBIS_LINE
#endif

#ifdef ESP32
#ifndef NO_USE_SML_CANBUS
// canbus support
#undef USE_SML_CANBUS
#define USE_SML_CANBUS
#endif
#else
#undef USE_SML_CANBUS
#endif

#ifdef USE_SML_TCP_SECURE
#define USE_SML_TCP_IP_STR
#endif


// median filter eliminates outliers, but uses much RAM and CPU cycles
// 672 bytes extra RAM with SML_MAX_VARS = 16
// default compile on, but must be enabled by descriptor flag 16
// may be undefined if RAM must be saved

#ifndef NO_USE_SML_MEDIAN_FILTER
#undef USE_SML_MEDIAN_FILTER
#define USE_SML_MEDIAN_FILTER
#endif

#ifdef USE_SML_DECRYPT
#include "han_Parser.h"
#endif

#ifndef SML_TRX_BUFF_SIZE
#define SML_TRX_BUFF_SIZE 1024
#endif

#ifdef USE_SML_CANBUS

#ifdef ESP8266
// esp8266 uses SPI MPC2515
#undef SML_CAN_MASKS
#undef SML_CAN_FILTERS
#define SML_CAN_MASKS 2
#define SML_CAN_FILTERS 6
#include "mcp2515.h"
#else
// esp32 uses native twai_
#undef SML_CAN_MASKS
#undef SML_CAN_FILTERS
#define SML_CAN_MASKS 1
#define SML_CAN_FILTERS 1
#include <can.h>
#include "driver/twai.h"
#endif
#endif // USE_SML_CANBUS

/* special options per meter
1:
special binary SML option for meters that use a bit in the status register to sign import or export like ED300L, AS2020 or DTZ541
a. obis code that holds the direction bit,
b. Flag identifier,
c. direction bit,
d. second Flag identifier (some meters use 2 different flags),
e. second bit,
f. obis code of value to be inverted on direction bit.
e.g. 1,=so1,00010800,65,11,65,11,00100700 for DTZ541

2:
flags, currently only bit 0 and 1
if 1 fix DWS74 bug
if 2 use asci obis line compare instead a pattern compare
e.g. 1,=so2,2  set obis line mode on meter 1

3:
serial buffers
a. serial buffer size
b. serial irq buffer size, a must be given
c. dumplog buffer size, default is 128 , a and b must be given
e.g. 1,=so3,256,256  set serial buffers on meter 1

4:
decrytion key, 16 bytes hex btw 32 chars without spaces or commas
defining key switches decryption mode on

5:
authentication key, 16 bytes hex btw 32 chars without spaces or commas
needs USE_SML_AUTHKEY

6:
synchronisation timout in milliseconds, after no serial data within this
time serial pointer is reset to zero

7:
on esp32 the uart index may be set, normally it is allocated from 2 down to 0 automatically
thus you can combine serial SML with serial script , berry or serial drivers.

8:
on esp32 1 filter mask
on esp8266 2 filter masks

9:
on esp32 1 filter
on esp8266 6 filters

A:
decryption flags (8 bits)

*/

//#define MODBUS_DEBUG

PUSH_OPTIONS
MODULE_DESCRIPTOR("SML",MODULE_TYPE_SENSOR,1<<16|4,"",0,"",0,"",0,"",0)
MODULE_PART bool begin(uint32_t speed, uint32_t smode, int32_t recpin, int32_t trxpin, int32_t invert);
MODULE_PART int peek(void);
MODULE_PART int read(void);
MODULE_PART size_t write(uint8_t byte);
MODULE_PART int available(void);
MODULE_PART void flush(void);
MODULE_PART void setRxBufferSize(uint32_t size);
MODULE_PART void updateBaudRate(uint32_t baud);
MODULE_PART void rxRead(void);
MODULE_PART void end();
MODULE_PART void setbaud(uint32_t speed);
MODULE_PART double sml_median_array(double *array, uint8_t len);
MODULE_PART double sml_median(struct SML_MEDIAN_FILTER* mf, double in);
MODULE_PART uint16_t Serial_available();
MODULE_PART uint8_t Serial_read();
MODULE_PART uint8_t Serial_peek();
MODULE_PART void sml_dump_start(char c);
MODULE_PART void dump2log(void);
MODULE_PART void Hexdump(uint8_t *sbuff, uint32_t slen);
MODULE_PART uint8_t *skip_sml(uint8_t *cp,int16_t *res);
MODULE_PART double sml_getvalue(uint8_t *cp, uint8_t index);
MODULE_PART uint8_t hexnibble(char chr);
MODULE_PART double CharToDouble(const char *str);
MODULE_PART void ebus_esc(uint8_t *ebus_buffer, uint8_t len);
MODULE_PART uint8_t check_ebus_esc(uint8_t *ebus_buffer, uint8_t len);
MODULE_PART uint8_t ebus_crc8(uint8_t data, uint8_t crc_init);
MODULE_PART uint8_t ebus_CalculateCRC( uint8_t *Data, uint16_t DataLen );
MODULE_PART void sml_empty_receiver(uint32_t meters);
MODULE_PART void sml_shift_in(uint32_t meters, uint32_t shard);
MODULE_PART void SML_Poll(void);
MODULE_PART uint32_t vbus_get_septet(uint8_t *cp);
MODULE_PART char *skip_double(char *cp);
MODULE_PART uint8_t *sml_find(uint8_t *src, uint16_t ssize, uint8_t *pattern, uint16_t psize);
MODULE_PART double sml_get_obis_value(uint8_t *data);
MODULE_PART void SML_Decode(uint8_t index);
MODULE_PART void SML_Immediate_MQTT(const char *mptr,uint8_t index,uint8_t mindex);
MODULE_PART void SML_Show(boolean json);
MODULE_PART void SML_CounterIsr(void *arg);
MODULE_PART uint32_t SML_getlinelen(char *lp);
MODULE_PART uint32_t SML_getscriptsize(char *lp);
MODULE_PART uint32_t SML_getscriptsize(char *lp);
MODULE_PART bool Gpio_used(uint8_t gpiopin);
MODULE_PART char *SpecOptions(char *cp, uint32_t mnum);
MODULE_PART uint16_t serial_dispatch(uint8_t meter, uint8_t sel);
MODULE_PART int SML_print(const char *format, ...);
MODULE_PART void reset_sml_vars(uint16_t maxmeters);
MODULE_PART void sml_free_vars(void);
MODULE_PART int32_t SML_Init_0(void);
MODULE_PART int32_t SML_Init(void);
MODULE_PART uint32_t SML_SetBaud(uint32_t meter, uint32_t br);
MODULE_PART uint32_t sml_status(uint32_t meter);
MODULE_PART uint32_t SML_Write(int32_t meter, char *hstr);
MODULE_PART uint32_t SML_Read(int32_t meter, char *str, uint32_t slen);
MODULE_PART uint32_t sml_getv(uint32_t sel);
MODULE_PART uint32_t SML_Shift_Num(uint32_t meter, uint32_t shift);
MODULE_PART double SML_GetVal(uint32_t index);
MODULE_PART char *SML_GetSVal(uint32_t index);
MODULE_PART int32_t SML_Set_WStr(uint32_t meter, char *hstr);
MODULE_PART void SetDBGLed(uint8_t srcpin, uint8_t ledpin);
MODULE_PART void SML_Counter_Poll_1s(void);
MODULE_PART void SML_Counter_Poll(void);
MODULE_PART uint32_t sml_can_check_alerts();
MODULE_PART void SML_CANBUS_Read();
MODULE_PART char *SML_Get_Sequence(char *cp,uint32_t index);
MODULE_PART void SML_Check_Send(void);
MODULE_PART void sml_hex_asci(uint32_t mindex, char *tpowstr);
MODULE_PART uint8_t sml_hexnibble(char chr);
MODULE_PART uint32_t sml_hex32(char *cp);
MODULE_PART uint16_t sml_swap(uint16_t in);
MODULE_PART void sml_tcp_send(uint32_t meter, uint8_t *sbuff, uint16_t slen);
MODULE_PART int32_t sml_tcp_init(struct METER_DESC *mptr);
MODULE_PART void sml_tcp_check(void);
MODULE_PART void SML_Send_Seq(uint32_t meter, char *seq);
MODULE_PART uint16_t MBUS_calculateCRC(uint8_t *frame, uint8_t num, uint16_t start);
MODULE_PART uint16_t KS_calculateCRC(const uint8_t *frame, uint8_t num);
MODULE_PART uint8_t SML_PzemCrc(uint8_t *data, uint8_t len);
MODULE_PART uint8_t CalcEvenParity(uint8_t data);
MODULE_PART void InjektCounterValue(uint8_t meter, uint32_t counter, double rate);
MODULE_PART void SML_CounterSaveState(void);
MODULE_PART uint32_t SML_SetOptions(uint32_t in);
MODULE_PART void SML_Restart(void);
MODULE_PART void SML_dump(void);
MODULE_PART void SML_counter(void);
MODULE_PART void SML_led(void);
MODULE_PART void SML_meter(void);
MODULE_PART uint32_t SML_Getvars(uint16_t function);
MODULE_PART void SML_Deinit(void);
MODULE_PART int32_t mod_func_execute(uint32_t function);
MODULE_END
/********************************************************************************************/

#ifdef ESP32
// redefine serial calls
#undef Del_TSerial
#define Del_TSerial Del_E32Serial
#undef TSerial_Available
#define TSerial_Available E32Serial_Available
#undef TSerial_Peek
#define TSerial_Peek E32Serial_Peek
#undef TSerial_Read
#define TSerial_Read E32Serial_Read
#undef TSerial_Write
#define TSerial_Write E32Serial_Write
#undef TSerial_Flush
#define TSerial_Flush E32Serial_Flush
#endif

typedef union {
  uint8_t data;
  struct {
    uint8_t trxenpol : 1;  // string or number
    uint8_t trxen : 1;
    uint8_t trxenpin : 6;
  };
} TRX_EN_TYPE;

typedef union {
  uint8_t data;
  struct {
    uint8_t SO_DWS74_BUG : 1;
    uint8_t SO_OBIS_LINE : 1;
    uint8_t SO_TRX_INVERT : 1;
    uint8_t SO_DISS_PULL : 1;
  };
} SO_FLAGS;

#ifndef TMSBSIZ
#define TMSBSIZ 256
#endif

#ifndef SML_STIMEOUT
#define SML_STIMEOUT 1000
#endif

#define METER_ID_SIZE 24

#define SML_CRYPT_SIZE 16

#ifndef SML_PREFIX_SIZE
#define SML_PREFIX_SIZE 8
#endif

uint32_t SML_SetOptions(uint32_t in);

typedef struct {
  uint32_t (*SML_SetBaud)(uint32_t,uint32_t);
  uint32_t (*sml_status)(uint32_t);
  uint32_t (*SML_Write)(int32_t,char*);
  uint32_t (*SML_Read)(int32_t,char*,uint32_t);
  uint32_t (*sml_getv)(uint32_t);
  uint32_t (*SML_Shift_Num)(uint32_t,uint32_t);
  double (*SML_GetVal)(uint32_t);
  char * (*SML_GetSVal)(uint32_t);
  int32_t (*SML_Set_WStr)(uint32_t,char*);
  void (*SML_Decode)(uint8_t);
  uint32_t (*SML_SetOptions)(uint32_t);
} SML_TABLE;

struct METER_DESC {
  int8_t srcpin;
  uint8_t type;
  uint16_t flag;
  int32_t params;
  char prefix[SML_PREFIX_SIZE];
  int8_t trxpin;
  uint8_t tsecs;
  char *txmem;
  uint8_t index;
  uint8_t max_index;
  char *script_str;
  uint8_t sopt;
  TRX_EN_TYPE trx_en;
  bool shift_mode;
  uint16_t sbsiz;
  uint8_t *sbuff;
  uint16_t spos;
  uint16_t sibsiz;
	uint32_t lastms;
	uint16_t tout_ms;
  SO_FLAGS so_flags;
  char meter_id[METER_ID_SIZE];

#ifdef USE_SML_SPECOPT
  uint32_t so_obis1;
  uint32_t so_obis2;
  uint8_t so_fcode1;
  uint8_t so_bpos1;
  uint8_t so_fcode2;
  uint8_t so_bpos2;
#endif // USE_SML_SPECOPT

#ifdef ESP32
#ifndef USE_ESP32_SW_SERIAL
  HardwareSerial *meter_ss;
#else
  void *meter_ss;
#endif
#endif  // ESP32

// software serial pointers
#ifdef ESP8266
  TasmotaSerial *meter_ss;
#endif  // ESP8266

#ifdef USE_SML_DECRYPT
	bool use_crypt;
  uint8_t crypflags;
	uint8_t last_iob;
	uint8_t key[SML_CRYPT_SIZE];
	Han_Parser *hp;

#ifdef USE_SML_AUTHKEY
	uint8_t auth[SML_CRYPT_SIZE];
#endif // USE_SML_AUTHKEY
#endif // USE_SML_DECRYPT


#ifdef USE_SML_TCP

#ifdef USE_SML_TCP_IP_STR
  char ip_addr[16];
#else
  IPAddress ip_addr;
#endif // USE_SML_TCP_IP_STR


#ifdef USE_SML_TCP_SECURE
  void *client;
#else
  void *client;
#endif // USE_SML_TCP_SECURE

#endif // USE_SML_TCP

#ifdef USE_SML_CANBUS
#ifdef ESP8266
  MCP2515 *mcp2515;
#else
  //twai_handle_t *canp;
#endif
  uint32_t can_masks[SML_CAN_MASKS];
  uint32_t can_filters[SML_CAN_FILTERS];
#endif // USE_SML_CANBUS

#ifdef ESP32
  int8_t uart_index;
#endif

};



#define TCP_MODE_FLG 0x7f

// Meter flags
#define PULLUP_FLG 0x01
#define ANALOG_FLG 0x02
#define MEDIAN_FILTER_FLG 0x10
#define NO_SYNC_FLG 0x20


// this driver uses double because some meter vars would not fit in float
//=====================================================

// serial buffers, may be made larger depending on telegram lenght
#ifndef SML_BSIZ
#define SML_BSIZ 48
#endif

#define VBUS_SYNC		0xaa
#define SML_SYNC		0x77
#define EBUS_SYNC		0xaa
#define EBUS_ESC    0xa9



//#ifndef FLT_MAX
//#define FLT_MAX 99999999
//#endif

// double constants, including zero
const double d_const[12] PROGMEM = {0,99999999,10,100,1000,10000,999999,-1,60000.0,360000.0,99,1};

#define GETDCONSTP volatile const double *fpc = (const double *) ((uint8_t *)d_const+EXEC_OFFSET);
#define SFPC_0 fpc[0]
#define FLT_MAX fpc[1]

#define SFPC_10 fpc[2]
#define SFPC_100 fpc[3]
#define SFPC_1000 fpc[4]
#define SFPC_10000 fpc[5]
#define SFPC_999999 fpc[6]
#define SFPC_M1 fpc[7]
#define SFPC_60000 fpc[8]
#define SFPC_360000 fpc[9]
#define SFPC_99 fpc[10]
#define SFPC_1 fpc[11]

#ifndef CNT_PULSE_TIMEOUT
#define CNT_PULSE_TIMEOUT 5000
#endif

#ifndef METER_DEF_SIZE
#define METER_DEF_SIZE 3000
#endif

const int32_t i32_const[6] PROGMEM = {0xffff,0xA001,0x10000,0x1021,CNT_PULSE_TIMEOUT,METER_DEF_SIZE}; 
#define GETICONSTP volatile const int32_t *ipc = (const int32_t *) ((uint8_t *)i32_const+EXEC_OFFSET);

#define SIPC_FFFF ipc[0]
#define SIPC_A001 ipc[1]
#define SIPC_10000 ipc[2]
#define SIPC_1021 ipc[3]
#define CNT_PULSE_TOUT ipc[4]
#define METER_DEF_SIZ ipc[5]


const uint32_t ui32_const[2] PROGMEM = {0x80000000,0x7fffffff}; 
#define GETUICONSTP volatile const int32_t *uipc = (const int32_t *) ((uint8_t *)ui32_const+EXEC_OFFSET);

#define SUIPC_0x80000000 uipc[0]
#define SUIPC_0x7fffffff uipc[1]

const uint64_t u64_const[4] PROGMEM = {10,0,1,99999999};

#define GETU64CONSTP volatile const uint64_t *u64p = (const uint64_t *) ((uint8_t *)u64_const+EXEC_OFFSET);

#define SU64C_10 u64p[0]
#define SU64C_0 u64p[1]
#define SU64C_1 u64p[2]
#define SU64C_99999999 u64p[3]

// calulate deltas
#define MAX_DVARS MAX_METERS*2

#ifndef SML_DUMP_SIZE
#define SML_DUMP_SIZE 128
#endif

// median filter, should be odd size
#define MEDIAN_SIZE 5
struct SML_MEDIAN_FILTER {
double buffer[MEDIAN_SIZE];
int8_t index;
};

typedef struct {
  uint32_t sml_cnt_last_ts;
  uint32_t sml_counter_ltime;
  uint32_t sml_counter_lfalltime;
  uint32_t sml_counter_pulsewidth;
  uint16_t sml_debounce;
  uint8_t sml_cnt_updated;
  uint8_t sml_cnt_debounce;
  uint8_t sml_cnt_old_state;
  int8_t srcpin;
  uint8_t pinstate;
} SML_COUNTER;


struct SML_GLOBS {
  uint8_t sml_send_blocks;
  uint8_t sml_100ms_cnt;
  uint8_t sml_desc_cnt;
  uint8_t meters_used;
  uint8_t maxvars;
  uint8_t *meter_p;
  double *meter_vars;
  uint8_t *dvalid;
  double dvalues[MAX_DVARS];
  uint32_t dtimes[MAX_DVARS];
  char sml_start;
  uint8_t dump2log = 0;
  uint8_t ser_act_LED_pin;
  uint8_t ser_act_meter_num = 0;
  uint16_t sml_logindex;
  char *log_data;
	uint16_t logsize = SML_DUMP_SIZE;
#if defined(ED300L) || defined(AS2020) || defined(DTZ541) || defined(USE_SML_SPECOPT)
  uint8_t sml_status[MAX_METERS];
  uint8_t g_mindex;
#endif
#ifdef USE_SML_MEDIAN_FILTER
  struct SML_MEDIAN_FILTER *sml_mf;
#endif
	uint8_t *script_meter;
	struct METER_DESC *mptr;
  uint8_t to_cnt;
  bool ready;
#ifdef USE_SML_CANBUS
  uint8_t twai_installed;
#endif // USE_SML_CANBUS
  uint8_t sml_options;
  SML_TABLE smltab;
  uint8_t sb_counter;
  SML_COUNTER sml_counters[MAX_COUNTERS];
  uint8_t sml_cnt_index[MAX_COUNTERS];
}; 


typedef struct {
  struct SML_GLOBS sml_globs;
  struct METER_DESC  meter_desc[MAX_METERS];
} MODULE_MEMORY;

#define meter_desc mem->meter_desc
#define sml_globs mem->sml_globs

#define SML_OPTIONS_JSON_ENABLE 1


//#ifdef USE_SML_DECRYPT
#if 0
#include "/AmsLib/han_Parser_cpp.h"
#include "/AmsLib/Cosem_cpp.h"
#include "/AmsLib/crc_cpp.h"
#include "/AmsLib/DataParser_cpp.h"
#include "/AmsLib/DimsParser_cpp.h"
#include "/AmsLib/GbtParser_cpp.h"
#include "/AmsLib/DsmrParser.h"
#include "/AmsLib/GcmParser_cpp.h"
#include "/AmsLib/HdlcParser.h"
#include "/AmsLib/hexutils_cpp.h"
#include "/AmsLib/LibcParser_cpp.h"
#include "/AmsLib/MbusParser_cpp.h"
#include "/AmsLib/nthil_cpp.h"
#include "/AmsLib/Time_cpp.h"
#endif


#ifdef USE_SML_MEDIAN_FILTER

double sml_median_array(double *array, uint8_t len) {
SETREGS

      GETDCONSTP
      uint8_t ind[len];
      uint8_t mind = 0, index = 0, flg;
      double min = FLT_MAX;

      for (uint8_t hcnt = 0; hcnt < len / 2 + 1; hcnt++) {
          for (uint8_t mcnt = 0; mcnt < len; mcnt++) {
              flg = 0;
              for (uint8_t icnt = 0; icnt < index; icnt++) {
                  if (ind[icnt] == mcnt) {
                      flg = 1;
                  }
              }
              if (!flg) {
                  //if (array[mcnt] < min) {
                  if (__ltdf2(array[mcnt], min)) {
                      min = array[mcnt];
                      mind = mcnt;
                  }
              }
          }
          ind[index] = mind;
          index++;
          min = FLT_MAX;
      }
      return array[ind[len / 2]];
}


// calc median
double sml_median(struct SML_MEDIAN_FILTER* mf, double in) {
SETREGS

  //double tbuff[MEDIAN_SIZE],tmp;
  //uint8_t flag;
  mf->buffer[mf->index] = in;
  mf->index++;
  if (mf->index >= MEDIAN_SIZE) mf->index = 0;

  return sml_median_array(mf->buffer, MEDIAN_SIZE);

/*
  // sort list and take median
  memmove(tbuff,mf->buffer,sizeof(tbuff));
  for (byte ocnt=0; ocnt<MEDIAN_SIZE; ocnt++) {
    flag=0;
    for (byte count=0; count<MEDIAN_SIZE-1; count++) {
      if (tbuff[count]>tbuff[count+1]) {
        tmp=tbuff[count];
        tbuff[count]=tbuff[count+1];
        tbuff[count+1]=tmp;
        flag=1;
      }
    }
    if (!flag) break;
  }
  return tbuff[MEDIAN_SIZE/2];
  */
}
#endif

#define SML_SAVAILABLE Serial_available()
#define SML_SREAD Serial_read()
#define SML_SPEEK Serial_peek()

uint16_t Serial_available() {
SETREGS

  uint8_t num = sml_globs.dump2log & 7;
  if (num < 1 || num > sml_globs.meters_used) num = 1;
  num--;
  if (meter_desc[num].srcpin != TCP_MODE_FLG) {
    if (!meter_desc[num].meter_ss) return 0;
    //return meter_desc[num].meter_ss->available();
    return TSerial_Available(meter_desc[num].meter_ss);
    
  } else {
#ifdef USE_SML_TCP   
    if (meter_desc[num].client) {
      return client_available(meter_desc[num].client);
    } else {
      return 0;
    }
#else 
    return 0;
#endif
  }
}

uint8_t Serial_read() {
SETREGS

  uint8_t num = sml_globs.dump2log & 7;
  if (num < 1 || num > sml_globs.meters_used) num = 1;
  num--;
  if (meter_desc[num].srcpin != TCP_MODE_FLG) {
    if (!meter_desc[num].meter_ss) return 0;
    //return meter_desc[num].meter_ss->read();
    return TSerial_Read(meter_desc[num].meter_ss);
  } else {
#ifdef USE_SML_TCP
    if (meter_desc[num].client) {
      return client_read(meter_desc[num].client);
    } else {
      return 0;
    }
#else
    return 0;
#endif
  }
}

uint8_t Serial_peek() {
SETREGS

  uint8_t num = sml_globs.dump2log & 7;
  if (num < 1 || num > sml_globs.meters_used) num = 1;
  num--;
  if (meter_desc[num].srcpin != TCP_MODE_FLG) {
    if (!meter_desc[num].meter_ss) return 0;
    //return meter_desc[num].meter_ss->peek();
    return TSerial_Peek(meter_desc[num].meter_ss);
  } else {
#ifdef USE_SML_TCP
    if (meter_desc[num].client) {
      return client_peek(meter_desc[num].client);
    } else {
      return 0;
    }
#else
    return 0;
#endif
  }
}

void sml_dump_start(char c) {
SETREGS

	sml_globs.log_data[0] = ':';
	sml_globs.log_data[1] = c;
	sml_globs.sml_logindex = 2;
}


#define SML_EBUS_SKIP_SYNC_DUMPS

void dump2log(void) {
SETREGS

  int16_t index = 0, hcnt = 0;
  uint32_t d_lastms;
  uint8_t dchars[16];
	uint8_t meter = (sml_globs.dump2log & 7) - 1;
  uint8_t type = sml_globs.mptr[meter].type;

  //if (!SML_SAVAILABLE) return;
	if (!sml_globs.log_data) return;

  struct METER_DESC *mptr = &meter_desc[meter];

  //AddLog(LOG_LEVEL_INFO, PSTR(">>> %d"), SML_SAVAILABLE);

#ifdef USE_SML_DECRYPT
	if (mptr->use_crypt == true) {
			d_lastms = millis();
      while ((millis() - d_lastms) < 50) {
        while (SML_SAVAILABLE) {
					d_lastms = millis();
					uint16_t logsiz;
					uint8_t *payload;
					//if (mptr->hp->readHanPort(&payload, &logsiz, mptr->crypflags)) {
          HP_PARS hpars;
          hpars.out = &payload;
          hpars.size = &logsiz;
          hpars.flags = mptr->crypflags;
          if (ReadHanPort(mptr->hp, &hpars)) {
						if (logsiz > mptr->sbsiz) {
							logsiz = mptr->sbsiz;
						}
						memmove(mptr->sbuff, payload, logsiz);
						AddLog(LOG_LEVEL_INFO, PSTR("SML: decrypted block: %d bytes"), logsiz);
						uint16_t index = 0;
						while (logsiz) {
							sml_dump_start('>');
							for (uint16_t cnt = 0; cnt < 16; cnt++) {
								sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x "), mptr->sbuff[index++]);
								if (sml_globs.sml_logindex < sml_globs.logsize - 7) {
				          sml_globs.sml_logindex += 3;
				        }
								logsiz--;
								if (!logsiz) {
									break;
								}
							}
							AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
						}
					} else {
						// dump serial buffer
						sml_dump_start(' ');
						while (index < mptr->spos) {
							sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x "), mptr->sbuff[index++]);
							if (sml_globs.sml_logindex >= 32*3+2) {
								AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
								sml_dump_start(' ');
							}
						}
					}
        }
      }
			if (sml_globs.sml_logindex > 2) {
				AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
				sml_dump_start(' ');
			}
			mptr->hp->len = 0;
			return;
	}
#endif

  if (sml_globs.dump2log & 8) {
    // combo mode
    while (SML_SAVAILABLE) {
      sml_globs.log_data[index] = ':';
      index++;
      sml_globs.log_data[index] = ' ';
      index++;
      d_lastms = millis();
      while ((millis() - d_lastms) < 40) {
        if (SML_SAVAILABLE) {
          uint8_t c = SML_SREAD;
          sprintf_P(&sml_globs.log_data[index], PSTR("%02x "), c);
          dchars[hcnt] = c;
          index += 3;
          hcnt++;
          if (hcnt > 15) {
            // line complete, build asci chars
            sml_globs.log_data[index++] = '=';
            sml_globs.log_data[index++] = '>';
            sml_globs.log_data[index++] = ' ';
            for (uint8_t ccnt = 0; ccnt < 16; ccnt++) {
              if (isprint(dchars[ccnt])) {
                sml_globs.log_data[index] = dchars[ccnt];
              } else {
                sml_globs.log_data[index] = ' ';
              }
              index++;
            }
            break;
          }
        }
      }
      if (index > 0) {
        sml_globs.log_data[index] = 0;
        AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
        index = 0;
        hcnt = 0;
      }
    }
  } else {
		switch (type) {
     	case 'o':
      	// obis
      	while (SML_SAVAILABLE) {
        	char c = SML_SREAD&0x7f;
        	if (c == '\n' || c == '\r') {
          	if (sml_globs.sml_logindex > 2) {
            	sml_globs.log_data[sml_globs.sml_logindex] = 0;
            	AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
							sml_dump_start(' ');
          	}
          	continue;
        	}
        	sml_globs.log_data[sml_globs.sml_logindex] = c;
        	if (sml_globs.sml_logindex < sml_globs.logsize - 2) {
          	sml_globs.sml_logindex++;
        	}
      	}
				break;
     	case 'v':
      	// vbus
				{ uint8_t c;
      	while (SML_SAVAILABLE) {
        	c = SML_SREAD;
        	if (c == VBUS_SYNC) {
          	sml_globs.log_data[sml_globs.sml_logindex] = 0;
          	AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
						sml_dump_start(' ');
        	}
        	sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x "), c);
        	if (sml_globs.sml_logindex < sml_globs.logsize - 7) {
          	sml_globs.sml_logindex += 3;
        	}
      	}
				}
				break;
     	case 'e':
      	// ebus
      	{ uint8_t c, p;
      	while (SML_SAVAILABLE) {
        	c = SML_SREAD;
        	if (c == EBUS_SYNC) {
          	p = SML_SPEEK;
          	if (p != EBUS_SYNC && sml_globs.sml_logindex > 5) {
            	// new packet, plot last one
            	sml_globs.log_data[sml_globs.sml_logindex] = 0;
            	AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
            	strcpy_P(&sml_globs.log_data[0], PSTR(": aa "));
            	sml_globs.sml_logindex = 5;
          	}
          	continue;
        	}
        	sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x "), c);
        	if (sml_globs.sml_logindex < sml_globs.logsize - 7) {
          	sml_globs.sml_logindex += 3;
        	}
      	}
				}
				break;
     	case 's':
      	// sml
      	{ uint8_t c;
      	while (SML_SAVAILABLE) {
        	c = SML_SREAD;
        	if (c == SML_SYNC) {
						sml_globs.log_data[sml_globs.sml_logindex] = 0;
          	AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
						sml_dump_start(' ');
        	}
        	sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x "), c);
        	if (sml_globs.sml_logindex < sml_globs.logsize - 7) {
          	sml_globs.sml_logindex += 3;
        	}
      	}
				}
				break;

#ifdef USE_SML_CANBUS       
      case 'C':
 #ifdef ESP8266     
        if (mptr->mcp2515 == nullptr) break;
        { struct can_frame canFrame;
        while (mptr->mcp2515->checkReceive()) {
            if (mptr->mcp2515->readMessage(&canFrame) == MCP2515::ERROR_OK) {
              mptr->sbuff[0] = canFrame.can_id >> 24;
              mptr->sbuff[1] = canFrame.can_id >> 16;
              mptr->sbuff[2] = canFrame.can_id >> 8;
              mptr->sbuff[3] = canFrame.can_id;
              mptr->sbuff[4] = canFrame.can_dlc;
              for (int i = 0; i < canFrame.can_dlc; i++) {
                mptr->sbuff[5 + i] = canFrame.data[i];
              }
              sml_dump_start(' ');
              for (uint8_t index = 0; index < canFrame.can_dlc + 5; index++) {
                sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x"), mptr->sbuff[index]);
                sml_globs.sml_logindex += 2;
                if (index == 3) {
                  sml_globs.log_data[sml_globs.sml_logindex] = ':';
                  sml_globs.sml_logindex++;
                  sml_globs.log_data[sml_globs.sml_logindex] = ' ';
                  sml_globs.sml_logindex++;
                }
              }
              sml_globs.log_data[sml_globs.sml_logindex] = 0;
              AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
            } else {
              if (mptr->mcp2515->checkError()) {
                uint8_t errFlags = mptr->mcp2515->getErrorFlags();
                mptr->mcp2515->clearRXnOVRFlags();
                AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: Received error %d"), errFlags);
              }
            }
        }
        }
        break;
#else
        // esp32 native CAN
        if (!sml_globs.twai_installed) break;
        {
        uint32_t alerts_triggered = sml_can_check_alerts();

        // Check if message is received
        if (alerts_triggered & TWAI_ALERT_RX_DATA) {
          twai_message_t message;
          while (ptwai_receive(&message, 0) == ESP_OK) {
            mptr->sbuff[0] = message.identifier >> 24;
            mptr->sbuff[1] = message.identifier >> 16;
            mptr->sbuff[2] = message.identifier >> 8;
            mptr->sbuff[3] = message.identifier;
            mptr->sbuff[4] = message.data_length_code;
            for (int i = 0; i < message.data_length_code; i++) {
              mptr->sbuff[5 + i] = message.data[i];
            }
            sml_dump_start(' ');
            for (uint8_t index = 0; index < message.data_length_code + 5; index++) {
              sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x"), mptr->sbuff[index]);
              sml_globs.sml_logindex += 2;
              if (index == 3) {
                  sml_globs.log_data[sml_globs.sml_logindex] = ':';
                  sml_globs.sml_logindex++;
                  sml_globs.log_data[sml_globs.sml_logindex] = ' ';
                  sml_globs.sml_logindex++;
              }
            }
            sml_globs.log_data[sml_globs.sml_logindex] = 0;
            AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
          }
        }
        }
        break;
#endif
#endif // USE_SML_CANBUS
    	default:
      	// raw dump
      	d_lastms = millis();
      	sml_dump_start(' ');
				while ((millis() - d_lastms) < 40) {
					while (SML_SAVAILABLE) {
						d_lastms = millis();
						yield();
          	sprintf_P(&sml_globs.log_data[sml_globs.sml_logindex], PSTR("%02x "), SML_SREAD);
						if (sml_globs.sml_logindex < sml_globs.logsize - 7) {
	          	sml_globs.sml_logindex += 3;
	        	}
						if (sml_globs.sml_logindex >= 32*3+2) {
							AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
							sml_dump_start(' ');
						}
					}
      	}
      	if (sml_globs.sml_logindex > 2) {
        	sml_globs.log_data[sml_globs.sml_logindex] = 0;
        	AddLogData(LOG_LEVEL_INFO, sml_globs.log_data);
      	}
				break;

	 	}
  }
}

void Hexdump(uint8_t *sbuff, uint32_t slen) {
SETREGS

  char cbuff[slen*3+10];
  char *cp = cbuff;
  *cp++ = '>';
  *cp++ = ' ';
  for (uint32_t cnt = 0; cnt < slen; cnt ++) {
    sprintf_P(cp, PSTR("%02x "), sbuff[cnt]);
    cp += 3;
  }
  AddLogData(LOG_LEVEL_INFO, cbuff);
}

#define DOUBLE2CHAR dtostrfd

// skip sml entries
uint8_t *skip_sml(uint8_t *cp,int16_t *res) {
SETREGS

    uint8_t len,len1,type;
    len = *cp & 0xf;
    type = *cp & 0x70;
    if (type == 0x70) {
        // list, skip entries
        // list
        cp++;
        while (len--) {
            len1 = *cp & 0x0f;
            cp += len1;
        }
         *res = 0;
    } else {
        // skip len
        *res = (signed char)*(cp + 1);
        cp += len;
    }
    return cp;
}

// get sml binary value
// not defined for unsigned >0x7fff ffff ffff ffff (should never happen)
double sml_getvalue(uint8_t *cp, uint8_t index) {
SETREGS

  GETDCONSTP

  uint8_t len,unit,type;
  int16_t scaler,result;
  int64_t value;
  double dval;

  // scan for values
  // check status
#ifdef ED300L
  uint8_t *cpx = cp - 5;
  // decode OBIS 0180 amd extract direction info
  if (*cp == 0x64 && *cpx == 0 && *(cpx+1) == 0x01 && *(cpx + 2) == 0x08 && *(cpx + 3) == 0) {
      sml_globs.sml_status[sml_globs.g_mindex] = *(cp+3);
  }
  if (*cp == 0x63 && *cpx == 0 && *(cpx + 1) == 0x01 && *(cpx + 2) == 0x08 && *(cpx + 3) == 0) {
      sml_globs.sml_status[sml_globs.g_mindex] = *(cp+2);
  }
#endif
#ifdef AS2020
  uint8_t *cpx=cp-5;
  // decode OBIS 0180 amd extract direction info
  if (*cp==0x64 && *cpx==0 && *(cpx+1)==0x01 && *(cpx+2)==0x08 && *(cpx+3)==0) {
      sml_globs.sml_status[sml_globs.g_mindex]=*(cp+2);
  }
  if (*cp==0x63 && *cpx==0 && *(cpx+1)==0x01 && *(cpx+2)==0x08 && *(cpx+3)==0) {
      sml_globs.sml_status[sml_globs.g_mindex]=*(cp+1);
  }
#endif
#ifdef DTZ541
  uint8_t *cpx=cp-5;
  // decode OBIS 0180 amd extract direction info
  if (*cp==0x65 && *cpx==0 && *(cpx+1)==0x01 && *(cpx+2)==0x08 && *(cpx+3)==0) {
    sml_globs.sml_status[sml_globs.g_mindex]=*(cp+3);
  }
#endif


#ifdef USE_SML_SPECOPT
 uint8_t *cpx = cp - 5;
 uint32_t ocode = (*(cpx + 0) << 24) | (*(cpx + 1) << 16) | (*(cpx + 2) << 8) | (*(cpx + 3) << 0);

 if (ocode == meter_desc[sml_globs.g_mindex].so_obis1) {
   sml_globs.sml_status[sml_globs.g_mindex] &= 0xfe;
   uint32_t flag = 0;
   uint16_t bytes = 0;
   if (*cp == meter_desc[sml_globs.g_mindex].so_fcode1) {
     cpx = cp + 1;
     bytes = (meter_desc[sml_globs.g_mindex].so_fcode1 & 0xf) - 1;
     for (uint16_t cnt = 0; cnt < bytes; cnt++) {
        flag <<= 8;
        flag |= *cpx++;
     }
     if (flag & (1 << meter_desc[sml_globs.g_mindex].so_bpos1)) {
       sml_globs.sml_status[sml_globs.g_mindex] |= 1;
     }
   }
   if (*cp == meter_desc[sml_globs.g_mindex].so_fcode2) {
     cpx = cp + 1;
     bytes = (meter_desc[sml_globs.g_mindex].so_fcode2 & 0xf) - 1;
     for (uint16_t cnt = 0; cnt < bytes; cnt++) {
       flag <<= 8;
       flag |= *cpx++;
     }
     if (flag & (1 << meter_desc[sml_globs.g_mindex].so_bpos1)) {
       sml_globs.sml_status[sml_globs.g_mindex] |= 1;
     }
   }
 }
#endif

  cp = skip_sml(cp, &result);
  // check time
  cp = skip_sml(cp, &result);
  // check unit
  cp = skip_sml(cp, &result);
  // check scaler
  cp = skip_sml(cp, &result);
  scaler = result;

  // get value
  GETU64CONSTP
  type = *cp & 0x70;
  len = *cp & 0x0f;
  cp++;
    if (type == 0x50 || type == 0x60) {
        // shift into 64 bit
        uint64_t uvalue = SU64C_0;
        uint8_t nlen = len;
        while (--nlen) {
            uvalue <<= 8;
            uvalue |= *cp++;
        }
        if (type == 0x50) {
            // signed
            switch (len - 1) {
                case 1:
                    // byte
                    value = (signed char)uvalue;
                    break;
                case 2:
                    // signed 16 bit
                    if (meter_desc[index].so_flags.SO_DWS74_BUG) {
                      if (scaler == -2) {
                        value = (uint32_t)uvalue;
                      } else {
                        value = (int16_t)uvalue;
                      }
                    } else {
                      value = (int16_t)uvalue;
                    }
                    break;
                case 3:
                  // signed 24 bit
                  value = (int32_t)(uvalue << 8);
                  value /= 256;
                  break;

                case 4:
                    // signed 32 bit
                    value = (int32_t)uvalue;
                    break;
                case 5:
                case 6:
                case 7:
                case 8:
                    // signed 64 bit
                    value = (int64_t)uvalue;
                    break;
            }
        } else {
            // unsigned
            value = uvalue;
        }

    } else {
        if (!(type & 0xf0)) {
            // octet string serial number
            // no coding found on the net
            // up to now 2 types identified on Hager
            if (len == 9) {
              // serial number on hager => 24 bit - 24 bit
                cp++;
                uint32_t s1,s2;
                s1 = *cp << 16 | *(cp + 1) <<8 | *(cp + 2);
                cp += 4;
                s2 = *cp << 16 | *(cp + 1) <<8 | *(cp + 2);
                sprintf_P(&meter_desc[index].meter_id[0], PSTR("%u-%u"), s1, s2);
            } else {
                // server id on hager
                char *str = &meter_desc[index].meter_id[0];
                for (type = 0; type < len - 1; type++) {
                    sprintf_P(str, PSTR("%02x"), *cp++);
                    str += 2;
                }
            }
            value = SU64C_0;
        } else {
          value = SU64C_99999999;
          scaler = 0;
        }
    }
    dval = __floatdidf(value);
    if (scaler == -1) {
      dval = __divdf3(dval, SFPC_10);
    } else if (scaler == -2) {
      dval = __divdf3(dval, SFPC_100);
    } else if (scaler == -3) {
      dval = __divdf3(dval, SFPC_1000);
    } else if (scaler == -4) {
      dval = __divdf3(dval, SFPC_10000);
    } else if (scaler == 1) {
      dval = __divdf3(dval, SFPC_10);
    } else if (scaler == 2) {
      dval = __divdf3(dval, SFPC_100);
    } else if (scaler == 3) {
      dval = __divdf3(dval, SFPC_1000);
    }
  #ifdef ED300L
    // decode current power OBIS 00 0F 07 00
    if (*cpx==0x00 && *(cpx+1)==0x0f && *(cpx+2)==0x07 && *(cpx+3)==0) {
        if (sml_globs.sml_status[sml_globs.g_mindex]&0x20) {
          // and invert sign on solar feed
          dval =__muldf3(dval, SFPC_M1);
        }
    }
  #endif
  #ifdef AS2020
    // decode current power OBIS 00 10 07 00
    if (*cpx==0x00 && *(cpx+1)==0x10 && *(cpx+2)==0x07 && *(cpx+3)==0) {
        if (sml_globs.sml_status[sml_globs.g_mindex]&0x08) {
          // and invert sign on solar feed
          dval =__muldf3(dval, SFPC_M1);
        }
    }
  #endif
  #ifdef DTZ541
    // decode current power OBIS 00 10 07 00
    if (*cpx==0x00 && *(cpx+1)==0x10 && *(cpx+2)==0x07 && *(cpx+3)==0) {
        if (sml_globs.sml_status[sml_globs.g_mindex]&0x08) {
          // and invert sign on solar feed
          dval =__muldf3(dval, SFPC_M1);
        }
    }
  #endif

#ifdef USE_SML_SPECOPT
    ocode = (*(cpx + 0) << 24) | (*(cpx + 1) << 16) | (*(cpx + 2) << 8) | (*(cpx + 3) << 0);
    if (ocode == meter_desc[sml_globs.g_mindex].so_obis2) {
      if (sml_globs.sml_status[sml_globs.g_mindex] & 1) {
        // and invert sign on solar feed
        dval =__muldf3(dval, SFPC_M1);
      }
    }
  #endif

    return dval;
}

uint8_t hexnibble(char chr) {
SETREGS

  uint8_t rVal = 0;
  if (isdigit(chr)) {
    rVal = chr - '0';
  } else  {
    chr=toupper(chr);
    if (chr >= 'A' && chr <= 'F') rVal = chr + 10 - 'A';
  }
  return rVal;
}

// need double precision in this driver
double CharToDouble(const char *str) {
SETREGS

  GETDCONSTP

  // simple ascii to double, because atof or strtod are too large
  char strbuf[24];

  strlcpy(strbuf, str, sizeof(strbuf));
  char *pt = strbuf;
  while ((*pt != '\0') && (*pt == ' ')) { pt++; }  // Trim leading spaces

  signed char sign = 1;
  if (*pt == '-') { sign = -1; }
  if (*pt == '-' || *pt=='+') { pt++; }            // Skip any sign

  double left = SFPC_0;
  if (*pt != '.') {
    left = __floatdidf(atoll(pt));                               // Get left part
    while (isdigit(*pt)) { pt++; }                 // Skip number
  }

  double right = SFPC_0;
  if (*pt == '.') {
    pt++;
    right = __floatdidf(atoll(pt));                              // Decimal part
    while (isdigit(*pt)) {
      pt++;
      right = __divdf3(right, SFPC_10);
    }
  }

  double result = __adddf3(left, right);
  // Add negative sign
  if (sign < 0) {
    result = __muldf3(result, SFPC_M1);                              
  }
  return result;
}


// remove ebus escapes
void ebus_esc(uint8_t *ebus_buffer, uint8_t len) {
SETREGS

    short count,count1;
    for (count = 0; count < len; count++) {
        if (ebus_buffer[count] == EBUS_ESC) {
            //found escape
            ebus_buffer[count] += ebus_buffer[count + 1];
            // remove 2. char
            count++;
            for (count1 = count; count1 < len; count1++) {
                ebus_buffer[count1] = ebus_buffer[count1 + 1];
            }
        }
    }

}

// check ebus escapes
uint8_t check_ebus_esc(uint8_t *ebus_buffer, uint8_t len) {
SETREGS

    short count,count1;
    count1 = 0;
    for (count = 0; count < len; count++) {
        if (ebus_buffer[count] == EBUS_ESC) {
            //found escape
            count1++;
        }
    }
    return count1;
}

uint8_t ebus_crc8(uint8_t data, uint8_t crc_init) {
SETREGS

	uint8_t crc;
	uint8_t polynom;
	int i;

	crc = crc_init;
	for (i = 0; i < 8; i++) {
		if (crc & 0x80) {
			polynom = (uint8_t) 0x9B;
		}
		else {
			polynom = (uint8_t) 0;
		}
		crc = (uint8_t)((crc & ~0x80) << 1);
		if (data & 0x80) {
			crc = (uint8_t)(crc | 1) ;
		}
		crc = (uint8_t)(crc ^ polynom);
		data = (uint8_t)(data << 1);
	}
	return (crc);
}

// ebus crc
uint8_t ebus_CalculateCRC( uint8_t *Data, uint16_t DataLen ) {
SETREGS

	uint16_t i;
	uint8_t Crc = 0;
	for(i = 0 ; i < DataLen ; ++i, ++Data ) {
      Crc = ebus_crc8( *Data, Crc );
   }
   return Crc;
}

void sml_empty_receiver(uint32_t meters) {
SETREGS

  //while (meter_desc[meters].meter_ss->available()) {
  //  meter_desc[meters].meter_ss->read();
  //}

  while (TSerial_Available(meter_desc[meters].meter_ss)) {
    TSerial_Read(meter_desc[meters].meter_ss);
  }
}

void sml_shift_in(uint32_t meters, uint32_t shard) {
SETREGS

  uint32_t count;

  struct METER_DESC *mptr = &meter_desc[meters];

  if (!mptr->sbuff) return;

#ifdef USE_SML_DECRYPT
	if (mptr->use_crypt) {
		if (mptr->hp) {
			uint32_t timediff = millis() - mptr->lastms;
			if (timediff > mptr->tout_ms) {
				mptr->hp->len = 0;
				mptr->spos = 0;
				AddLog(LOG_LEVEL_DEBUG, PSTR("SML: sync"));
			}
			mptr->lastms = millis();
			uint16_t len;
			uint8_t *payload;
      HP_PARS hpars;
      hpars.out = &payload;
      hpars.size = &len;
      hpars.flags = mptr->crypflags;
      if (ReadHanPort(mptr->hp, &hpars)) {
			//if (mptr->hp->readHanPort(&payload, &len, mptr->crypflags)) {
				if (len > mptr->sbsiz) {
					len = mptr->sbsiz;
				}
				memmove(mptr->sbuff, payload, len);
				AddLog(LOG_LEVEL_DEBUG, PSTR("SML: decrypted block: %d bytes"), len);
				SML_Decode(meters);
			}
		}
		return;
	}
#endif

  if (mptr->shift_mode) {
    // shift in
    for (count = 0; count < mptr->sbsiz - 1; count++) {
      mptr->sbuff[count] = mptr->sbuff[count + 1];
    }
  }
    
  uint8_t iob;
  if (mptr->srcpin != TCP_MODE_FLG) {
    //iob = (uint8_t)mptr->meter_ss->read();
    iob = (uint8_t)TSerial_Read(mptr->meter_ss); 

  } else {
#ifdef USE_SML_TCP
    if (mptr->client) {
      iob = (uint8_t)client_read(mptr->client);
    } else {
      iob = 0;
    }
#else
    iob = 0;
#endif
  }

  switch (mptr->type) {
    case 'o':
      // asci obis
      if (!(mptr->so_flags.SO_OBIS_LINE)) {
        mptr->sbuff[mptr->sbsiz - 1] = iob & 0x7f;
      } else {
        iob &= 0x7f;
        mptr->sbuff[mptr->spos] = iob;
        mptr->spos++;
        if (mptr->spos >= mptr->sbsiz) {
          mptr->spos = 0;
        }
        if ((iob == 0x0a) || (iob == 0x0d)) {
          SML_Decode(meters);
          mptr->spos = 0;
        }
      }
      break;
    case 's':
      // binary obis = sml
      mptr->sbuff[mptr->sbsiz - 1] = iob;
      if (mptr->sbuff[0] != SML_SYNC && ((mptr->flag & NO_SYNC_FLG) == 0)) {
        // Skip decoding, when buffer does not start with sync byte (0x77)
        sml_globs.sb_counter++;
        return;
      }
      break;
    case 'r':
      // raw with shift
      mptr->sbuff[mptr->sbsiz - 1] = iob;
      break;
    case 'R':
      // raw without shift
			{
			uint32_t timediff = millis() - mptr->lastms;
			if (timediff > mptr->tout_ms) {
				mptr->spos = 0;
        SML_Decode(meters);
				AddLog(LOG_LEVEL_DEBUG, PSTR("SML: sync"));
			}
			mptr->lastms = millis();
      mptr->sbuff[mptr->spos] = iob;
      mptr->spos++;
      if (mptr->spos > mptr->sbsiz) {
        mptr->spos = 0;
      }
			}
      break;
    case 'k':
      // Kamstrup
      if (iob == 0x40) {
        mptr->spos = 0;
      } else if (iob == 0x0d) {
        uint8_t index = 0;
        uint8_t *ucp = &mptr->sbuff[0];
        for (uint16_t cnt = 0; cnt < mptr->spos; cnt++) {
          uint8_t iob = mptr->sbuff[cnt] ;
          if (iob == 0x1b) {
            *ucp++ = mptr->sbuff[cnt + 1]  ^ 0xff;
            cnt++;
          } else {
            *ucp++ = iob;
          }
          index++;
        }
        uint16_t crc = KS_calculateCRC(&mptr->sbuff[0], index);
        if (!crc) {
          SML_Decode(meters);
        }
        sml_empty_receiver(meters);
        mptr->spos = 0;
      } else {
        mptr->sbuff[mptr->spos] = iob;
        mptr->spos++;
        if (mptr->spos >= mptr->sbsiz) {
          mptr->spos = 0;
        }
      }
      break;
    case 'm':
    case 'M':
      // modbus
      mptr->sbuff[mptr->spos] = iob;
      mptr->spos++;
      if (mptr->spos >= mptr->sbsiz) {
        mptr->spos = 0;
      }
      if (mptr->srcpin == TCP_MODE_FLG) {
        // tcp read
        if (mptr->spos >= 6) {
          uint8_t tlen = (mptr->sbuff[4] << 8) | mptr->sbuff[5];
          if (mptr->spos == 6 + tlen) {
            mptr->spos = 0;
            memmove(&mptr->sbuff[0], &mptr->sbuff[6], mptr->sbsiz - 6);
            SML_Decode(meters);
            if (mptr->client) {
              client_flush(mptr->client);
            }
            //Hexdump(mptr->sbuff + 6, 10);
          }
        }
        break;
      }

      if (mptr->spos >= 3) {
        uint32_t mlen = mptr->sbuff[2] + 5;
        if (mlen > mptr->sbsiz) mlen = mptr->sbsiz;
        if (mptr->spos >= mlen) {
#ifdef MODBUS_DEBUG
          AddLog(LOG_LEVEL_INFO, PSTR("receive index >> %d"), mptr->index);
          Hexdump(mptr->sbuff, 10);
#endif
          SML_Decode(meters);
          sml_empty_receiver(meters);
          mptr->spos = 0;
        }
      }
      break;
    case 'p':
      // pzem
      mptr->sbuff[mptr->spos] = iob;
      mptr->spos++;
      if (mptr->spos >= 7) {
        SML_Decode(meters);
        sml_empty_receiver(meters);
        mptr->spos = 0;
      }
      break;
    case 'v':
      // vbus
      if (iob == EBUS_SYNC) {
        sml_globs.sb_counter = 0;
        SML_Decode(meters);
        mptr->sbuff[0] = iob;
        mptr->spos = 1;
      } else {
        if (mptr->spos < mptr->sbsiz) {
          mptr->sbuff[mptr->spos] = iob;
          mptr->spos++;
        }
      }
      break;
    case 'e':
      // ebus
      if (iob == EBUS_SYNC) {
        // should be end of telegramm
        // QQ,ZZ,PB,SB,NN ..... CRC, ACK SYNC
        if (mptr->spos > 5 && mptr->spos > mptr->sbuff[4] + 5) {
          // get telegramm lenght
          uint16_t tlen = mptr->sbuff[4] + 5 + check_ebus_esc(mptr->sbuff, mptr->spos);
          // test crc
          if (mptr->sbuff[tlen] == ebus_CalculateCRC(mptr->sbuff, tlen)) {
              ebus_esc(mptr->sbuff, mptr->spos);
              SML_Decode(meters);
          } else {
              // crc error
              AddLog(LOG_LEVEL_INFO, PSTR("ebus crc error"));
          }
        }
        mptr->spos = 0;
        return;
      }
      mptr->sbuff[mptr->spos] = iob;
      mptr->spos++;
      if (mptr->spos >= mptr->sbsiz) {
        mptr->spos = 0;
      }
      break;
  }

  if (mptr->shift_mode) {
    SML_Decode(meters);
  }
}


//uint16_t sml_count = 0;

// polled every 50 ms
void SML_Poll(void) {
SETREGS

uint32_t meters;

    for (meters = 0; meters < sml_globs.meters_used; meters++) {
      struct METER_DESC *mptr = &meter_desc[meters];
      if (mptr->type == 'C') continue;
      if (mptr->type != 'c') {
        if (mptr->srcpin != TCP_MODE_FLG) {
          if (!mptr->meter_ss) continue;
          // poll for serial input
          //if (sml_globs.ser_act_LED_pin != 255 && (sml_globs.ser_act_meter_num == 0 || sml_globs.ser_act_meter_num - 1 == meters)) {
          //  digitalWrite(sml_globs.ser_act_LED_pin, mptr->meter_ss->available() && !digitalRead(sml_globs.ser_act_LED_pin)); // Invert LED, if queue is continuously full
          //}
          //while (mptr->meter_ss->available()) {
          //  sml_shift_in(meters, 0);
          //}

          if (sml_globs.ser_act_LED_pin != 255 && (sml_globs.ser_act_meter_num == 0 || sml_globs.ser_act_meter_num - 1 == meters)) {
            digitalWrite(sml_globs.ser_act_LED_pin, TSerial_Available(mptr->meter_ss) && !digitalRead(sml_globs.ser_act_LED_pin)); // Invert LED, if queue is continuously full
          }
          while (TSerial_Available(mptr->meter_ss)) {
            sml_shift_in(meters, 0);
          }
          
        } else {

#ifdef USE_SML_TCP
          if (mptr->client) {    
            while (client_available(mptr->client)) {
              sml_shift_in(meters, 0);
            }
          }
#endif
        }
      }
    }
}

#define VBUS_BAD_CRC 0
// get vbus septet with 6 bytes
uint32_t vbus_get_septet(uint8_t *cp) {
SETREGS

  uint32_t result = 0;

  //AddLog(LOG_LEVEL_INFO,PSTR("septet: %02x %02x %02x %02x %02x %02x"),cp[0] ,cp[1],cp[2],cp[3],cp[4],cp[5]);

  uint8_t Crc = 0x7F;
  for (uint32_t i = 0; i < 5; i++) {
    Crc = (Crc - cp[i]) & 0x7f;
  }
  if (Crc != cp[5]) {
    result = VBUS_BAD_CRC;
  } else {
    result = (cp[3] | ((cp[4]&8)<<4));
    result <<= 8;
    result |= (cp[2] | ((cp[4]&4)<<5));
    result <<= 8;
    result |= (cp[1] | ((cp[4]&2)<<6));
    result <<= 8;
    result |= (cp[0] | ((cp[4]&1)<<7));
  }

  //AddLog(LOG_LEVEL_INFO,PSTR("septet r: %d"),result);
  return result;
}


char *skip_double(char *cp) {
SETREGS

  if (*cp == '+' || *cp == '-') {
    cp++;
  }
  while (*cp) {
    if (*cp == '.') {
      cp++;
    }
    if (!isdigit(*cp)) {
      return cp;
    }
    cp++;
  }
  return 0;
}

uint8_t *sml_find(uint8_t *src, uint16_t ssize, uint8_t *pattern, uint16_t psize) {
SETREGS

	//AddLog(LOG_LEVEL_INFO, PSTR(">> %02x %02x %02x %02x"),pattern[0],pattern[1],pattern[2],pattern[3]);
	if (psize >= ssize) {
		return 0;
	}
	for (uint32_t cnt = 0; cnt < ssize - psize; cnt++) {
		if (!memcmp(src, pattern, psize)) {
			return src;
		}
		src++;
	}
	return 0;
}

#ifdef USE_SML_DECRYPT
double sml_get_obis_value(uint8_t *data) {
SETREGS

	double out = 0;
	CosemData *item = (CosemData *)data;
	switch (item->base.type) {
		case CosemTypeLongSigned: {
				out = ntohs(item->ls.data);
				break;
		}
		case CosemTypeLongUnsigned: {
				out = ntohs(item->lu.data);
				break;
		}
		case CosemTypeDLongSigned: {
				out = ntohl(item->dlu.data);
				break;
		}
		case CosemTypeDLongUnsigned: {
				out = ntohl(item->dlu.data);
				break;
		}
		case CosemTypeLong64Signed: {
				out = ntohll(item->l64s.data);
				break;
		}
		case CosemTypeLong64Unsigned: {
				out = ntohll(item->l64u.data);
				break;
		}
	}
	return out;
}
#endif // USE_SML_DECRYPT



void SML_Decode(uint8_t index) {
SETREGS

  GETDCONSTP
  GETICONSTP

  char *mptr = (char*)sml_globs.meter_p;
  int8_t mindex;
  uint8_t *cp;
  uint8_t dindex = 0, vindex = 0;
  delay(0);

  if (!sml_globs.ready) {
    return;
  }

  while (mptr != NULL) {
    // check list of defines
    if (*mptr == 0) break;

    // new section
    mindex = ((*mptr) & 7) - 1;

    if (mindex < 0 || mindex >= sml_globs.meters_used) mindex = 0;
    mptr += 2;
    if (*mptr == '=' && *(mptr + 1) == 'h') {
      mptr = strchr(mptr, '|');
      if (mptr) mptr++;
      continue;
    }

    if (*mptr == '=' && *(mptr + 1) == 's') {
      mptr = strchr(mptr, '|');
      if (mptr) mptr++;
      continue;
    }

    // =d must handle dindex
    if (*mptr == '=' && *(mptr + 1) == 'd') {
      if (index != mindex) {
        dindex++;
      }
    }

    if (index != mindex) goto nextsect;

    // start of serial source buffer
    cp = meter_desc[mindex].sbuff;

    // compare
    if (*mptr == '=') {
      // calculated entry, check syntax
      mptr++;
      // do math m 1+2+3
      if (*mptr == 'm' && !sml_globs.sb_counter) {
        // only every 256 th byte
        // else it would be calculated every single serial byte
        mptr++;
        while (*mptr == ' ') mptr++;
        // 1. index
        double dvar;
        uint8_t opr;
        uint8_t mind;
        int32_t ind;
        mind = strtol((char*)mptr, (char**)&mptr, 10);
        if (mind < 1 || mind > sml_globs.maxvars) mind = 1;
        mind--;
        dvar = sml_globs.meter_vars[mind];
        while (*mptr==' ') mptr++;
        for (uint8_t p = 0; p < 8; p++) {
          if (*mptr == '@') {
            // store result
            sml_globs.meter_vars[vindex] = dvar;
            mptr++;
            break;
          }
          opr = *mptr;
          mptr++;
          uint8_t iflg = 0;
          if (*mptr == '#') {
            iflg = 1;
            mptr++;
          }
          ind = strtol((char*)mptr, (char**)&mptr, 10);
          double flind = __floatunsidf(ind);
          mind = ind;
          if (mind < 1 || mind > sml_globs.maxvars) mind = 1;
          mind--;
          switch (opr) {
              case '+':
                if (iflg) dvar = __adddf3(dvar, flind);
                else dvar = __adddf3(dvar, sml_globs.meter_vars[mind]);
                break;
              case '-':
                if (iflg) dvar = __subdf3(dvar, flind);
                else dvar = __subdf3(dvar, sml_globs.meter_vars[mind]);
                break;
              case '*':
                if (iflg) dvar = __muldf3(dvar, flind);
                else dvar = __muldf3(dvar, sml_globs.meter_vars[mind]);
                break;
              case '/':
                if (iflg) dvar = __divdf3(dvar, flind);
                else dvar = __divdf3(dvar, sml_globs.meter_vars[mind]);
                break;
          }
          while (*mptr==' ') mptr++;
          if (*mptr == '@') {
            // store result
            sml_globs.meter_vars[vindex] = dvar;
            mptr++;
            break;
          }
        }
        double fac = CharToDouble((char*)mptr);
        sml_globs.meter_vars[vindex] = __divdf3(sml_globs.meter_vars[vindex], fac);
        SML_Immediate_MQTT((const char*)mptr, vindex, mindex);
        sml_globs.dvalid[vindex] = 1;
        // get sfac
      } else if (*mptr == 'd') {
        // calc deltas d ind 10 (eg every 10 secs)
        if (dindex < MAX_DVARS) {
          // only n indexes
          mptr++;
          while (*mptr == ' ') mptr++;
          uint8_t ind = atoi(mptr);
          while (*mptr >= '0' && *mptr <= '9') mptr++;
          if (ind < 1 || ind > sml_globs.maxvars) ind = 1;
          ind--;
          uint32_t delay = atoi(mptr) * 1000;
          uint32_t dtime = millis() - sml_globs.dtimes[dindex];
          if (dtime > delay) {
            // calc difference
            sml_globs.dtimes[dindex] = millis();
            //double vdiff = sml_globs.meter_vars[ind] - sml_globs.dvalues[dindex];
            double vdiff = __subdf3(sml_globs.meter_vars[ind], sml_globs.dvalues[dindex]);
            sml_globs.dvalues[dindex] = sml_globs.meter_vars[ind];
            //double dres = (double)SFPC_360000 * vdiff / ((double)dtime / SFPC_10000);
            double p1 = __muldf3(SFPC_360000, vdiff);
            double p2 = __divdf3(__floatunsidf(dtime), SFPC_10000);
            double dres = __divdf3(p1, p2);

            sml_globs.dvalid[vindex] += 1;

            if (sml_globs.dvalid[vindex] >= 2) {
              // differece is only valid after 2. calculation
              sml_globs.dvalid[vindex] = 2;

#ifdef USE_SML_MEDIAN_FILTER
              if (sml_globs.mptr[mindex].flag & MEDIAN_FILTER_FLG) {
                sml_globs.meter_vars[vindex] = sml_median(&sml_globs.sml_mf[vindex], dres);
              } else {
                sml_globs.meter_vars[vindex] = dres;
              }
#else
              sml_globs.meter_vars[vindex] = dres;
#endif
            }
            mptr=strchr(mptr,'@');
            if (mptr) {
              mptr++;
              double fac = CharToDouble((char*)mptr);
              sml_globs.meter_vars[vindex] = __divdf3(sml_globs.meter_vars[vindex], fac);
              SML_Immediate_MQTT((const char*)mptr, vindex, mindex);
            }
          }
          //sml_globs.dvalid[vindex] = 1;
          dindex++;
        }
      } else if (*mptr == 'h') {
        // skip html tag line
        mptr = strchr(mptr, '|');
        if (mptr) mptr++;
        continue;
      } else if (*mptr == 's') {
        // skip spec option tag line
        mptr = strchr(mptr, '|');
        if (mptr) mptr++;
        continue;
      }
    } else {
      // compare value
      uint8_t found = 1;
      double ebus_dval = SFPC_99;
      double mbus_dval = SFPC_99;
      while (*mptr != '@') {
        if (found == 0) {
          // skip rest of decoder part
          mptr++;
          continue;
        }
        if (sml_globs.mptr[mindex].type == 'o' || sml_globs.mptr[mindex].type == 'c') {
          if (*mptr++ != *cp++) {
            found = 0;
          }
        } else {
          if (sml_globs.mptr[mindex].type == 's') {
            // sml
            uint8_t val = hexnibble(*mptr++) << 4;
            val |= hexnibble(*mptr++);
            if (val != *cp++) {
              found = 0;
            }
          } else {
            // ebus modbus pzem vbus or raw
						if (!strncmp_P(mptr, PSTR("pm("), 3)) {
							// pattern match
							uint8_t dp = 0;
							mptr += 3;
							// default to asci obis
							uint8_t aflg = 3;
							if (*mptr == 'r') {
								aflg = 0;
								mptr++;
							} else if (*mptr == 'h') {
								aflg = 1;
								mptr++;
							}
							uint8_t pattern[64];
							// check for obis pattern
							for (uint32_t cnt = 0; cnt < sizeof(pattern); cnt++) {
								if (*mptr == '@' || !*mptr) {
									break;
								}
								if (*mptr == ')') {
									mptr++;
									if ((aflg & 2) && (dp == 2)) {
										pattern[cnt] = 0xff;
										cnt++;
									}
									pattern[cnt] = 0;
									uint8_t *ucp = sml_find(cp, meter_desc[index].sbsiz, pattern, cnt);
									if (ucp) {
										cp = ucp + cnt;
										// check auto type
										if (aflg & 1) {
											// METER_ID_SIZE
#ifdef USE_SML_DECRYPT
											CosemData *item = (CosemData *)cp;
											switch (item->base.type) {
            						case CosemTypeString:
                					memmove(meter_desc[mindex].meter_id, item->str.data, item->str.length);
                					meter_desc[mindex].meter_id[item->str.length] = 0;
                					break;
            						case CosemTypeOctetString:
                					memmove(meter_desc[mindex].meter_id, item->oct.data, item->oct.length);
                					meter_desc[mindex].meter_id[item->oct.length] = 0;
                					break;
												default:
													ebus_dval = sml_get_obis_value(cp);
        							}
#endif
										}
									} else {
                    found = 0;
                  }
									break;
								}
								uint8_t iob;
								if (aflg & 2) {
									iob = strtol((char*)mptr, (char**)&mptr, 10);
									if (*mptr == '.') {
										mptr++;
										dp++;
									}
								} else {
									iob = hexnibble(*mptr++) << 4;
									iob |= hexnibble(*mptr++);
								}
								pattern[cnt] = iob;
							}
						} else if (*mptr == 'x') {
              if (*(mptr + 1) == 'x') {
                //ignore one byte
                mptr += 2;
                cp++;
              } else {
                mptr++;
                if (isdigit(*mptr)) {
                  uint32_t skip = strtol((char*)mptr, (char**)&mptr, 10);
                  cp += skip;
                }
              }
            } else if (!strncmp_P(mptr, PSTR("U64"), 3)) {
              uint32_t valh = (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | (cp[3]<<0);
              uint32_t vall = (cp[4]<<24) | (cp[5]<<16) | (cp[6]<<8) | (cp[7]<<0);
              uint64_t val = ((uint64_t)valh<<32) | vall;
              mptr += 3;
              cp += 8;
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
            } else if (!strncmp_P(mptr, PSTR("u64"), 3)) {
              uint64_t valh = (cp[1]<<24) | (cp[0]<<16) | (cp[3]<<8) | (cp[2]<<0);
              uint64_t vall = (cp[5]<<24) | (cp[4]<<16) | (cp[7]<<8) | (cp[6]<<0);
              uint64_t val = ((uint64_t)valh<<32) | vall;
              mptr += 3;
              cp += 8;
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
            } else if (!strncmp_P(mptr, PSTR("U32"), 3)) {
              mptr += 3;
              goto U32_do;
            } else if (!strncmp_P(mptr, PSTR("UUuuUUuu"), 8)) {
              mptr += 8;
              U32_do:
              uint32_t val = (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | (cp[3]<<0);
              cp += 4;
              if (*mptr == 's') {
                mptr++;
                // swap words
                val = (val>>16) | (val<<16);
              }
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
            } else if (!strncmp_P(mptr, PSTR("u32"), 3)) {
              mptr += 3;
              goto u32_do;
            } else if (!strncmp_P(mptr, PSTR("uuUUuuUU"), 8)) {
              mptr += 8;
              u32_do:
              uint32_t val = (cp[1]<<24) | (cp[0]<<16) | (cp[3]<<8) | (cp[2]<<0);
              cp += 4;
              if (*mptr == 's') {
                mptr++;
                // swap words
                val = (val>>16) | (val<<16);
              }
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
            } else if (!strncmp_P(mptr, PSTR("UUuu"), 4)) {
              uint16_t val = cp[1] | (cp[0]<<8);
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
              mptr += 4;
              cp += 2;
            } else if (!strncmp_P(mptr, PSTR("S32"), 3)) {
              mptr += 3;
              goto S32_do;
            } else if (!strncmp_P(mptr, PSTR("SSssSSss"), 8)) {
              mptr += 8;
              S32_do:
              int32_t val = (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | (cp[3]<<0);
              cp += 4;
              if (*mptr == 's') {
                mptr++;
                // swap words
                val = ((uint32_t)val>>16) | ((uint32_t)val<<16);
              }
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
            } else if (!strncmp_P(mptr, PSTR("s32"), 3)) {
              mptr += 3;
              goto s32_do;
            } else if (!strncmp_P(mptr, PSTR("ssSSssSS"), 8)) {
              mptr += 8;
              s32_do:
              int32_t val = (cp[1]<<24) | (cp[0]<<16) | (cp[3]<<8) | (cp[2]<<0);
              cp += 4;
              if (*mptr == 's') {
                mptr++;
                // swap words
                val = ((uint32_t)val>>16) | ((uint32_t)val<<16);
              }
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
            } else if (!strncmp_P(mptr, PSTR("uuUU"), 4)) {
              uint16_t val = cp[0] | (cp[1]<<8);
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
              mptr += 4;
              cp += 2;
            } else if (!strncmp_P(mptr, PSTR("uu"), 2)) {
              uint8_t val = *cp++;
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
              mptr += 2;
            } else if (!strncmp_P(mptr, PSTR("ssSS"), 4)) {
              int16_t val = *cp | (*(cp+1)<<8);
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
              mptr += 4;
              cp += 2;
            } else if (!strncmp_P(mptr, PSTR("SSss"), 4)) {
              int16_t val = cp[1] | (cp[0]<<8);
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
              mptr += 4;
              cp += 2;
            } else if (!strncmp_P(mptr, PSTR("ss"), 2)) {
              int8_t val = *cp++;
              ebus_dval = __floatundidf(val);
              mbus_dval = ebus_dval;
              mptr += 2;
            } else if (!strncmp_P(mptr, PSTR("ffffffff"), 8)) {
              uint32_t val = (cp[0]<<24) | (cp[1]<<16) | (cp[2]<<8) | (cp[3]<<0);
              float *fp = (float*)&val;
              ebus_dval = __extendsfdf2(*fp);
              mbus_dval = ebus_dval;
              mptr += 8;
              cp += 4;
            } else if (!strncmp_P(mptr, PSTR("FFffFFff"), 8)) {
              // reverse word float
              uint32_t val = (cp[1]<<0) | (cp[0]<<8) | (cp[3]<<16) | (cp[2]<<24);
              float *fp = (float*)&val;
              ebus_dval = __extendsfdf2(*fp);
              mbus_dval = ebus_dval;
              mptr += 8;
              cp += 4;
            } else if (!strncmp_P(mptr, PSTR("eeeeee"), 6)) {
              uint32_t val = (cp[0]<<16) | (cp[1]<<8) | (cp[2]<<0);
              mbus_dval = __floatundidf(val);
              mptr += 6;
              cp += 3;
            } else if (!strncmp_P(mptr, PSTR("vvvvvv"), 6)) {
              //mbus_dval = (float)((cp[0]<<8) | (cp[1])) + ((float)cp[2]/SFPC_10);
              double p1 = __floatundidf((cp[0]<<8 | cp[1]));
              double p2 = __divdf3(__floatundidf(cp[2]), SFPC_10);
              mbus_dval = __adddf3(p2, p2);

              mptr += 6;
              cp += 3;
            } else if (!strncmp_P(mptr, PSTR("cccccc"), 6)) {
              //mbus_dval = (float)((cp[0]<<8) | (cp[1])) + ((float)cp[2]/SFPC_100);
              double p1 = __floatundidf((cp[0]<<8 | cp[1]));
              double p2 = __divdf3(__floatundidf(cp[2]), SFPC_100);
              mbus_dval = __adddf3(p2, p2);
              mptr += 6;
              cp += 3;
            } else if (!strncmp_P(mptr, PSTR("pppp"), 4)) {
              //mbus_dval = (float)((cp[0]<<8) | cp[1]);
              mbus_dval = __floatsidf((cp[0]<<8 | cp[1]));
              mptr += 4;
              cp += 2;
            }  else if (!strncmp_P(mptr, PSTR("kstr"), 4)) {
              mptr += 4;
              // decode the mantissa
              uint32_t x = 0;
              for (uint16_t i = 0; i < cp[1]; i++) {
                x <<= 8;
                x |= cp[i + 3];
              }
              // decode the exponent
              int32_t i = cp[2] & 0x3f;
              if (cp[2] & 0x40) {
                i = -1;
              };
              //float ifl = pow(10, i);
              double ifl = SFPC_1;
              for (uint16_t x = 1; x <= i; ++x) {
                ifl = __muldf3(ifl, SFPC_10);
              }
              if (cp[2] & 0x80) {
                ifl = __muldf3(ifl, SFPC_M1);
              }
              mbus_dval =  __muldf3(__floatsidf(x), ifl);

            } else if (!strncmp_P(mptr, PSTR("bcd"), 3)) {
              mptr += 3;
              uint8_t digits = strtol((char*)mptr, (char**)&mptr, 10);
              if (digits < 2) digits = 2;
              if (digits > 12) digits = 12;
              GETU64CONSTP
              uint64_t bcdval = SU64C_0;
              uint64_t mfac = SU64C_1;
              for (uint32_t cnt = 0; cnt < digits; cnt += 2) {
                uint8_t iob = *cp++;
                //bcdval += (iob & 0xf) * mfac;
                bcdval += __muldi3((iob & 0xf), mfac);
                //mfac *= SU64C_10;
                mfac = __muldi3(mfac, SU64C_10);
                //bcdval += (iob >> 4) * mfac;
                bcdval += __muldi3((iob >> 4), mfac);
                //mfac *= SU64C_10;
                mfac = __muldi3(mfac, SU64C_10);
              }
              mbus_dval = __floatundidf(bcdval);
              ebus_dval = mbus_dval;
            } else if (*mptr == 'v') {
              // vbus values vul, vsl, vuwh, vuwl, wswh, vswl, vswh
              // vub3, vsb3 etc
              mptr++;
              int16_t offset = -1;
              if (*mptr == 'o') {
                mptr++;
                offset = strtol((char*)mptr, (char**)&mptr, 10);
                cp += (offset / 4) * 6;
              }
              uint8_t usign;
              if (*mptr == 'u') {
                usign = 1;
              } else if (*mptr == 's') {
                usign = 0;
              }
              mptr++;
              switch (*mptr) {
                case 'l':
                  mptr++;
                  // get long value
                  if (usign) {
                    ebus_dval = __floatunsidf(vbus_get_septet(cp));
                  } else {
                    ebus_dval = __floatsidf((int32_t)vbus_get_septet(cp));
                  }
                  break;
                case 'w':
                  mptr++;
                  char wflg;
                  if (offset >= 0) {
                    if (offset % 4) {
                      wflg = 'h';
                    } else {
                      wflg = 'l';
                    }
                  } else {
                    wflg = *mptr;
                    mptr++;
                  }
                  // get word value
                  if (wflg == 'h') {
                    // high word
                    if (usign) {
                      ebus_dval = __floatunsidf((vbus_get_septet(cp) >> 16) & SIPC_FFFF);
                    } else {
                      ebus_dval = __floatsidf((int16_t)((vbus_get_septet(cp) >> 16) & SIPC_FFFF));
                    }
                  } else {
                    // low word
                    if (usign) {
                      ebus_dval = __floatunsidf(vbus_get_septet(cp) & SIPC_FFFF);
                    } else {
                      ebus_dval = __floatsidf((int16_t)(vbus_get_septet(cp) & SIPC_FFFF));
                    }
                  }
                  break;
                case 'b':
                  mptr++;
                  char bflg;
                  if (offset >= 0) {
                    bflg = 0x30 | (offset % 4);
                  } else {
                    bflg = *mptr;
                    mptr++;
                  }
                  switch (bflg) {
                    case '3':
                      if (usign) {
                        ebus_dval = __floatunsidf(vbus_get_septet(cp) >> 24);
                      } else {
                        ebus_dval = __floatsidf((int8_t)(vbus_get_septet(cp) >> 24));
                      }
                      break;
                    case '2':
                      if (usign) {
                        ebus_dval = __floatunsidf((vbus_get_septet(cp) >> 16) & 0xff);
                      } else {
                        ebus_dval = __floatsidf((int8_t)((vbus_get_septet(cp) >> 16) & 0xff));
                      }
                      break;
                    case '1':
                      if (usign) {
                        ebus_dval = __floatunsidf((vbus_get_septet(cp) >> 8) & 0xff);
                      } else {
                        ebus_dval = __floatsidf((int8_t)((vbus_get_septet(cp) >> 8) & 0xff));
                      }
                      break;
                    case '0':
                      if (usign) {
                        ebus_dval = __floatunsidf(vbus_get_septet(cp) & 0xff);
                      } else {
                        ebus_dval = __floatsidf((int8_t)(vbus_get_septet(cp) & 0xff));
                      }
                      break;
                  }
                  break;
                case 't':
                  mptr++;
                  { uint16_t time;
                    if (offset % 4) {
                      time = (vbus_get_septet(cp) >> 16) & SIPC_FFFF;
                    } else {
                      time = vbus_get_septet(cp) & SIPC_FFFF;
                    }
                    //sprintf_P(&meter_desc[index].meter_id[0], PSTR("%02d:%02d"), time / 60, time % 60);
                    sprintf_P(&meter_desc[index].meter_id[0], PSTR("%02d:%02d"), __udivsi3(time, 60), __umodsi3(time, 60));

                  }
                  break;
              }
              cp += 6;
            }
            else {
              uint8_t val = hexnibble(*mptr++) << 4;
              val |= hexnibble(*mptr++);
              if (val != *cp++) {
                found = 0;
              }
            }
          }
        }
      }
      if (found) {
        // matches, get value
        sml_globs.dvalid[vindex] = 1;
        mptr++;
#if defined(ED300L) || defined(AS2020) || defined(DTZ541) || defined(USE_SML_SPECOPT)
        sml_globs.g_mindex = mindex;
#endif
        if (*mptr == '#') {
          // get string value
          getstr:
          mptr++;
          if (sml_globs.mptr[mindex].type != 'v') {
            if (sml_globs.mptr[mindex].type == 'o') {
              uint32_t p;
              for (p = 0; p < METER_ID_SIZE - 2; p++) {
                if (*cp == *mptr) {
                  break;
                }
                meter_desc[mindex].meter_id[p] = *cp++;
              }
              meter_desc[mindex].meter_id[p] = 0;
            } else if (sml_globs.mptr[mindex].type == 'k') {
              // 220901
              uint32_t date = __fixunsdfsi(mbus_dval);
              //uint8_t year = date / SIPC_10000; // = 22
              //date -= year * SIPC_10000;
              //uint8_t month = date / 100; // = 09
              //uint8_t day = date % 100; // = 01
              uint8_t year = __udivsi3(date, SIPC_10000); // = 22
              date -= year * SIPC_10000;
              uint8_t month = __udivsi3(date, 100); // = 09
              uint8_t day = __umodsi3(date, 100); // = 01


              sprintf_P(&meter_desc[mindex].meter_id[0], PSTR("%02d.%02d.%02d"), day, month, year);
            } else {
              sml_getvalue(cp, mindex);
            }
          }
        } else {
          double dval;
          char type = sml_globs.mptr[mindex].type;
          if (type != 'C' && type != 'e' && type != 'r' && type != 'R' && type != 'm' && type != 'M' && type != 'k' && type != 'p' && type != 'v') {
            // get numeric values
            if (type == 'o' || type == 'c') {
              if (*mptr == '(') {
                mptr++;
                // skip this number of brackets
                uint8_t toskip = strtol((char*)mptr,(char**)&mptr, 10);
                mptr++;
                char *lcp = (char*)cp;
                if (toskip) {
                  char *bp = (char*)cp;
                  for (uint32_t cnt = 0; cnt < toskip; cnt++) {
                    bp = strchr(bp, '(');
                    if (!bp) {
                      break;
                    }
                    bp++;
                    lcp = bp;
                  }
                }
                if (*mptr == '#') {
                  cp = (uint8_t*)lcp;
                  goto getstr;
                }
                dval = CharToDouble((char*)lcp);
              } else if (*mptr == 's') {
                  mptr++;
                  char delim = *mptr;
                  mptr++;
                  uint8_t toskip = strtol((char*)mptr,(char**)&mptr, 10);
                  mptr++;
                  char *lcp = (char*)cp;
                  if (toskip) {
                    char *bp = (char*)cp;
                    for (uint32_t cnt = 0; cnt < toskip; cnt++) {
                      bp = strchr(bp, delim);
                      if (!bp) {
                        break;
                      }
                      bp++;
                      lcp = bp;
                    }
                  }
                  dval = CharToDouble((char*)lcp);
              } else {
                  dval = CharToDouble((char*)cp);
              }
            } else {
              dval = sml_getvalue(cp, mindex);
            }
          } else {
            // ebus pzem vbus or mbus or raw
            if (*mptr == 'b') {
              mptr++;
              uint8_t shift = *mptr & 7;
              ebus_dval = __floatundidf((uint32_t)__fixunsdfsi(ebus_dval) >> shift);
              ebus_dval = __floatundidf((uint32_t)__fixunsdfsi(ebus_dval) & 1);
              mptr+=2;
            }
            if (*mptr == 'i') {
              // mbus index
              mptr++;
              uint8_t mb_index = strtol((char*)mptr, (char**)&mptr, 10);
              if (mb_index != sml_globs.mptr[mindex].index) {
                goto nextsect;
              }
              if (sml_globs.mptr[mindex].type == 'k') {
                // crc is already checked, get float value
                dval = mbus_dval;
                mptr++;
              } else {
                if (meter_desc[mindex].srcpin != TCP_MODE_FLG) {
                  uint16_t pos = meter_desc[mindex].sbuff[2] + 3;
                  if (pos > (meter_desc[mindex].sbsiz - 2)) pos = meter_desc[mindex].sbsiz - 2;
                  uint16_t crc = MBUS_calculateCRC(&meter_desc[mindex].sbuff[0], pos, SIPC_FFFF);
                  if (lowByte(crc) != meter_desc[mindex].sbuff[pos]) goto nextsect;
                  if (highByte(crc) != meter_desc[mindex].sbuff[pos + 1]) goto nextsect;
                }
                dval = mbus_dval;
                //AddLog(LOG_LEVEL_INFO, PSTR(">> %s"),mptr);
                mptr++;
              }
            } else {
              if (sml_globs.mptr[mindex].type == 'p') {
                uint8_t crc = SML_PzemCrc(&meter_desc[mindex].sbuff[0],6);
                if (crc != meter_desc[mindex].sbuff[6]) goto nextsect;
                dval = mbus_dval;
              } else {
                dval = ebus_dval;
              }
            }
          }
#ifdef USE_SML_MEDIAN_FILTER
          if (sml_globs.mptr[mindex].flag & MEDIAN_FILTER_FLG) {
            sml_globs.meter_vars[vindex] = sml_median(&sml_globs.sml_mf[vindex], dval);
          } else {
            sml_globs.meter_vars[vindex] = dval;
          }
#else
          sml_globs.meter_vars[vindex] = dval;
#endif

          //AddLog(LOG_LEVEL_INFO, PSTR(">> %s"),mptr);
          // get scaling factor
          double fac = CharToDouble((char*)mptr);
          // get optional offset to calibrate meter
          char *cp = skip_double((char*)mptr);
          if (cp && (*cp == '+' || *cp == '-')) {
            double offset = CharToDouble(cp);
            sml_globs.meter_vars[vindex] =  __adddf3(sml_globs.meter_vars[vindex], offset);
          }
          sml_globs.meter_vars[vindex] = __divdf3(sml_globs.meter_vars[vindex] ,fac);
          SML_Immediate_MQTT((const char*)mptr, vindex, mindex);
        }
      }
      //AddLog(LOG_LEVEL_INFO, PSTR("set valid in line %d"), vindex);
    }
nextsect:
    // next section
    if (vindex < sml_globs.maxvars - 1) {
      vindex++;
    }
    mptr = strchr(mptr, '|');
    if (mptr) mptr++;
  }
}

//"1-0:1.8.0*255(@1," D_TPWRIN ",kWh," DJ_TPWRIN ",4|"
void SML_Immediate_MQTT(const char *mptr, uint8_t index, uint8_t mindex) {
SETREGS

  char tpowstr[32];
  char jname[24];

  // we must skip sf,webname,unit
  char *cp = strchr((char*)mptr, ',');
  if (cp) {
    cp++;
    // wn
    cp = strchr(cp,',');
    if (cp) {
      cp++;
      // unit
      cp = strchr(cp,',');
      if (cp) {
        cp++;
        // json mqtt
        for (uint8_t count = 0; count < sizeof(jname); count++) {
          if (*cp == ',') {
            jname[count] = 0;
            break;
          }
          jname[count] = *cp++;
        }
        cp++;
        uint8_t dp = atoi(cp);
        if (dp & 0x10) {
          // immediate mqtt
          DOUBLE2CHAR(sml_globs.meter_vars[index], dp & 0xf, tpowstr);
          ResponseTime_P(PSTR(",\"%s\":{\"%s\":%s}}"), sml_globs.mptr[mindex].prefix, jname, tpowstr);
          MqttPublishTeleSensor();
        }
      }
    }
  }
}

// web + json interface
void SML_Show(boolean json) {
SETREGS

  int8_t count, mindex, cindex = 0;
  char tpowstr[32];
  char name[24];
  char unit[8];
  char jname[24];
  int8_t index = 0, mid = 0;
  char *mptr = (char*)sml_globs.meter_p;
  char *cp, nojson = 0;
  //char b_mqtt_data[MESSZ];
  //b_mqtt_data[0]=0;

    if (!sml_globs.meters_used) return;

    int8_t lastmind = ((*mptr) & 7) - 1;
    if (lastmind < 0 || lastmind >= sml_globs.meters_used) lastmind = 0;
    while (mptr != NULL) {
        if (*mptr == 0) break;
        // setup sections
        mindex = ((*mptr) & 7) - 1;

        if (mindex < 0 || mindex >= sml_globs.meters_used) mindex = 0;
        if (sml_globs.mptr[mindex].prefix[0] == '*' && sml_globs.mptr[mindex].prefix[1] == 0) {
          nojson = 1;
        } else {
          nojson = 0;
        }
        mptr += 2;
        if (*mptr == '=' && *(mptr + 1) == 'h') {
          mptr += 2;
          // html tag
          if (json) {
            mptr = strchr(mptr, '|');
            if (mptr) mptr++;
            continue;
          }
          // web ui export
          uint8_t i;
          for (i = 0; i < sizeof(tpowstr) - 2; i++) {
            if (*mptr == '|' || *mptr == 0) break;
            tpowstr[i] = *mptr++;
          }
          tpowstr[i] = 0;
          // export html
          //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s{s}%s{e}", b_mqtt_data,tpowstr);
          WSContentSend_P(PSTR("<tr><td colspan=2>%s{e}"), tpowstr);
          // rewind, to ensure strchr
          mptr--;
          mptr = strchr(mptr, '|');
          if (mptr) mptr++;
          continue;
        }
        if (*mptr == '=' && *(mptr + 1) == 's') {
          mptr = strchr(mptr, '|');
          if (mptr) mptr++;
          continue;
        }
        // skip compare section
        cp = strchr(mptr, '@');
        if (cp) {
          cp++;
          tststr:
          if (*cp == '#') {
            // meter id
            if (*(cp + 1) == 'x') {
              // convert hex to asci
              sml_hex_asci(mindex, tpowstr);
            } else {
              sprintf_P(tpowstr, PSTR("\"%s\""), &meter_desc[mindex].meter_id[0]);
            }
            mid = 1;
          } else if (*cp == '(') {
            if (sml_globs.mptr[mindex].type == 'o') {
              cp++;
              strtol((char*)cp,(char**)&cp, 10);
              cp++;
              goto tststr;
            } else {
              mid = 0;
            }
          } else if (*cp == 's') {
            // skip values
            if (sml_globs.mptr[mindex].type == 'o') {
              cp += 2;
              strtol((char*)cp,(char**)&cp, 10);
              cp++;
              goto tststr;
            } else {
              mid = 0;
            }
          } else if (*cp == 'b') {
            // bit value
#ifdef SML_BIT_TEXT
            sprintf_P(tpowstr, PSTR("\"%s\""), (uint8_t)sml_globs.meter_vars[index]?D_ON:D_OFF);
            mid = 2;
#endif
          } else {
            mid = 0;
          }
          // skip scaling
          cp = strchr(cp, ',');
          if (cp) {
            // this is the name in web UI
            cp++;
            for (count = 0; count < sizeof(name); count++) {
              if (*cp == ',') {
                name[count] = 0;
                break;
              }
              name[count] = *cp++;
            }
            cp++;

            for (count = 0; count < sizeof(unit); count++) {
              if (*cp == ',') {
                unit[count] = 0;
                break;
              }
              unit[count] = *cp++;
            }
            cp++;

            for (count = 0; count < sizeof(jname); count++) {
              if (*cp == ',') {
                jname[count] = 0;
                break;
              }
              jname[count] = *cp++;
            }

            cp++;

            if (!mid) {
              uint8_t dp = atoi(cp) & 0xf;
              DOUBLE2CHAR(sml_globs.meter_vars[index], dp, tpowstr);
            }

            if (json) {
              //if (sml_globs.dvalid[index]) {

                //AddLog(LOG_LEVEL_INFO, PSTR("not yet valid line %d"), index);
              //}
              // json export
              if (index == 0) {
                  //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s,\"%s\":{\"%s\":%s", b_mqtt_data,sml_globs.mptr[mindex].prefix,jname,tpowstr);
                  if (!nojson) {
                    ResponseAppend_P(PSTR(",\"%s\":{\"%s\":%s"), sml_globs.mptr[mindex].prefix, jname, tpowstr);
                  }
              }
              else {
                if (lastmind != mindex) {
                  // meter changed, close mqtt
                  //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s}", b_mqtt_data);
                  if (!nojson) {
                     ResponseAppend_P(PSTR("}"));
                   }
                    // and open new
                    //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s,\"%s\":{\"%s\":%s", b_mqtt_data,sml_globs.mptr[mindex].prefix,jname,tpowstr);
                  if (!nojson) {
                    ResponseAppend_P(PSTR(",\"%s\":{\"%s\":%s"), sml_globs.mptr[mindex].prefix, jname, tpowstr);
                  }
                  lastmind = mindex;
                } else {
                  //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s,\"%s\":%s", b_mqtt_data,jname,tpowstr);
                  if (!nojson) {
                    ResponseAppend_P(PSTR(",\"%s\":%s"), jname, tpowstr);
                  }
                }
              }
            } else {
              // web ui export
              //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s{s}%s %s: {m}%s %s{e}", b_mqtt_data,meter_desc[mindex].prefix,name,tpowstr,unit);
              if (strncmp_P(name, PSTR("*"), 1)) {
                if (sml_globs.mptr[mindex].prefix[0] == '*') {
                  WSContentSend_P(PSTR("{s}%s{m}"), name);
                } else {
                  WSContentSend_P(PSTR("{s}%s %s{m}"), sml_globs.mptr[mindex].prefix, name);  // Do not replace decimal separator in label
                }
                WSContentSend_PD(PSTR("%s %s{e}"), tpowstr, unit); // Replace decimal separator in value
              }
            }
          }
        }
        if (index < sml_globs.maxvars - 1) {
          index++;
        }
        // next section
        mptr = strchr(cp, '|');
        if (mptr) mptr++;
    }
    if (json) {
     //snprintf_P(b_mqtt_data, sizeof(b_mqtt_data), "%s}", b_mqtt_data);
     //ResponseAppend_P(PSTR("%s"),b_mqtt_data);
     if (!nojson) {
       ResponseAppend_P(PSTR("}"));
     }
   } else {
     //WSContentSend_PD(PSTR("%s"),b_mqtt_data);
   }


#ifdef USE_DOMOTICZ
  if (json && !GetTasmotaGlobal->tele_period) {
    char str[16];
    DOUBLE2CHAR(sml_globs.meter_vars[0], 1, str);
    DomoticzSensorPowerEnergy(sml_globs.meter_vars[1], str);  // PowerUsage, EnergyToday
    DOUBLE2CHAR(sml_globs.meter_vars[2], 1, str);
    DomoticzSensor(DZ_VOLTAGE, str);  // Voltage
    DOUBLE2CHAR(sml_globs.meter_vars[3], 1, str);
    DomoticzSensor(DZ_CURRENT, str);  // Current
  }
#endif  // USE_DOMOTICZ

}

#ifdef SML_REPLACE_VARS

#ifndef SML_SRCBSIZE
#define SML_SRCBSIZE 256
#endif

uint32_t SML_getlinelen(char *lp) {
SETREGS

uint32_t cnt;
  for (cnt = 0; cnt < SML_SRCBSIZE - 1; cnt++) {
    if (lp[cnt] == SCRIPT_EOL) {
      break;
    }
  }
  return cnt;
}

uint32_t SML_getscriptsize(char *lp) {
SETREGS
GETICONSTP
uint32_t mlen = 0;
char dstbuf[SML_SRCBSIZE * 2];
  while (1) {
    Replace_Cmd_Vars(lp, 1, dstbuf, sizeof(dstbuf));
    lp += SML_getlinelen(lp) + 1;
    uint32_t slen = strlen(dstbuf);
    //AddLog(LOG_LEVEL_INFO, PSTR("%d - %s"),slen,dstbuf);
    mlen += slen + 1;
    if (*lp == '#') break;
    if (*lp == '>') break;
    if (*lp == 0) break;
  }
  //AddLog(LOG_LEVEL_INFO, PSTR("len=%d"),mlen);
  return mlen + 32;
}
#else
uint32_t SML_getscriptsize(char *lp) {
  GETICONSTP
  uint32_t mlen = 0;
  for (uint32_t cnt = 0; cnt < METER_DEF_SIZ - 1; cnt++) {
    if (lp[cnt] == '\n' && lp[cnt + 1] == '#') {
      mlen = cnt + 3;
      break;
    }
  }
  //AddLog(LOG_LEVEL_INFO, PSTR("len=%d"),mlen);
  return mlen;
}
#endif // SML_REPLACE_VARS

bool Gpio_used(uint8_t gpiopin) {
SETREGS
STGLOB
  //if ((gpiopin < nitems(TasmotaGlobal->gpio_pin)) && (TasmotaGlobal->gpio_pin[gpiopin] > 0)) {
  if (tgbl->gpio_pin[gpiopin] > 0) {
    return true;
  }
  return false;
}

#define SML_MINSB 64
char *SpecOptions(char *cp, uint32_t mnum) {
SETREGS

// special option
struct METER_DESC *mptr = &meter_desc[mnum];
	switch (*cp) {
		case '1':
			cp++;
#ifdef USE_SML_SPECOPT
			if (*cp == ',') {
		    cp++;
		    mptr->so_obis1 = strtol(cp, &cp, 16);
		  }
		  if (*cp == ',') {
		    cp++;
		    mptr->so_fcode1 = strtol(cp, &cp, 16);
		  }
		  if (*cp == ',') {
		    cp++;
		    mptr->so_bpos1 = strtol(cp, &cp, 10);
		  }
		  if (*cp == ',') {
		    cp++;
		    mptr->so_fcode2 = strtol(cp, &cp, 16);
		  }
		  if (*cp == ',') {
		    cp++;
		    mptr->so_bpos2 = strtol(cp, &cp, 10);
		  }
		  if (*cp == ',') {
		    cp++;
		    mptr->so_obis2 = strtol(cp, &cp, 16);
		  }
#endif
			break;
 		case '2':
			cp += 2;
			mptr->so_flags.data = strtol(cp, &cp, 16);
			break;
		case '3':
			cp += 2;
			mptr->sbsiz = strtol(cp, &cp, 10);
			if (*cp == ',') {
				cp++;
				mptr->sibsiz = strtol(cp, &cp, 10);
				if (mptr->sibsiz < SML_MINSB) {
					mptr->sibsiz = SML_MINSB;
				}
			}
			if (*cp == ',') {
				cp++;
				sml_globs.logsize = strtol(cp, &cp, 10);
			}
			break;
		case '4':
			cp += 2;
#ifdef USE_SML_DECRYPT
			meter_desc[mnum].use_crypt = true;
			for (uint8_t cnt = 0; cnt < (SML_CRYPT_SIZE * 2); cnt += 2) {
				mptr->key[cnt / 2] = (sml_hexnibble(cp[cnt]) << 4) | sml_hexnibble(cp[cnt + 1]);
			}
			AddLog(LOG_LEVEL_INFO, PSTR("SML: crypto mode used for meter %d"), mnum + 1);
			break;
#ifdef USE_SML_AUTHKEY
		case '5':
			cp += 2;
			for (uint8_t cnt = 0; cnt < (SML_CRYPT_SIZE * 2); cnt += 2) {
				mptr->auth[cnt / 2] = (sml_hexnibble(cp[cnt]) << 4) | sml_hexnibble(cp[cnt + 1]);
			}
			break;
#endif // USE_SML_AUTHKEY
    case 'A':
      cp += 2;
      mptr->crypflags = strtol(cp, &cp, 10);
      break;
#endif // USE_SML_DECRYPT
		case '6':
			cp += 2;
			mptr->tout_ms = strtol(cp, &cp, 10);
      break;
  	case '7':
			cp += 2;
#ifdef ESP32     
			mptr->uart_index = strtol(cp, &cp, 10);
#endif // ESP32
			break;

#ifdef USE_SML_CANBUS
     case '8':
      cp += 2;
      for (uint8_t cnt = 0; cnt < SML_CAN_MASKS; cnt++) {
				mptr->can_masks[cnt] = sml_hex32(cp);
        cp += 8;
        if (*cp != ',') {
          break;
        }
        cp++;
			}
      break;
    case '9':
      cp += 2;
      for (uint8_t cnt = 0; cnt < SML_CAN_FILTERS; cnt++) {
				mptr->can_filters[cnt] = sml_hex32(cp);
        cp += 8;
        if (*cp != ',') {
          break;
        }
        cp++;
			}
      break;

#endif // USE_SML_CANBUS
	}
	return cp;
}

#ifdef USE_SML_DECRYPT
uint16_t serial_dispatch(uint8_t meter, uint8_t sel) {
SETREGS

	struct METER_DESC *mptr = &meter_desc[meter];
	//if (!sel) {
	//	return mptr->meter_ss->available();
	//}
	//uint8_t iob = mptr->meter_ss->read();

  if (!sel) {
		return TSerial_Available(mptr->meter_ss);
	}
	uint8_t iob = TSerial_Read(mptr->meter_ss);
  
	return iob;
}

#if 0
int SML_print(const char *format, ...) {
SETREGS

	static char loc_buf[64];
	char* temp = loc_buf;
	int len;
	va_list arg;
	va_list copy;
	va_start(arg, format);
	va_copy(copy, arg);
	len = vsnprintf(NULL, 0, format, arg);
	va_end(copy);
	if (len >= sizeof(loc_buf)) {
		temp = (char*)special_malloc(len + 1);
		if (temp == NULL) {
	  	return 0;
	  }
	}
	vsnprintf(temp, len + 1, format, arg);
	AddLog(LOG_LEVEL_DEBUG, PSTR("SML: %s"),temp);
	va_end(arg);
	if (len >= sizeof(loc_buf)) {
		free(temp);
	}
	return len;
}
#endif

#endif // USE_SML_DECRYPT

void reset_sml_vars(uint16_t maxmeters) {
SETREGS

  for (uint32_t meters = 0; meters < maxmeters; meters++) {

		struct METER_DESC *mptr = &meter_desc[meters];
    mptr->spos = 0;
    mptr->sbsiz = SML_BSIZ;
    mptr->sibsiz = TMSBSIZ;
    if (mptr->sbuff) {
      free(mptr->sbuff);
      mptr->sbuff = 0;
    }
#ifdef USE_SML_SPECOPT
    mptr->so_obis1 = 0;
    mptr->so_obis2 = 0;
#endif
    mptr->so_flags.data = 0;
    // addresses a bug in meter DWS74
#ifdef DWS74_BUG
    mptr->so_flags.SO_DWS74_BUG = 1;
#endif

#ifdef SML_OBIS_LINE
    mptr->so_flags.SO_OBIS_LINE = 1;
#endif
    if (mptr->txmem) {
      free(mptr->txmem);
      mptr->txmem = 0;
    }
    mptr->txmem = 0;
    mptr->trxpin = -1;
    if (mptr->meter_ss) {
        //delete mptr->meter_ss;
        Del_TSerial(mptr->meter_ss);
        mptr->meter_ss = NULL;
    }

		mptr->lastms = millis();
		mptr->tout_ms = SML_STIMEOUT;

#ifdef ESP32
    mptr->uart_index = -1;
#endif

#ifdef USE_SML_CANBUS
    for (uint8_t cnt = 0; cnt < SML_CAN_MASKS; cnt++) {
			mptr->can_masks[cnt] = 0;
		}
    for (uint8_t cnt = 0; cnt < SML_CAN_FILTERS; cnt++) {
			mptr->can_filters[cnt] = 0;
    }
#endif // USE_SML_CANBUS

#ifdef USE_SML_DECRYPT
		if (mptr->use_crypt) {
			if (mptr->hp) {
        DelHanParser(mptr->hp);
				mptr->hp = NULL;
			}
		}
		mptr->use_crypt = 0;
#ifdef USE_SML_AUTHKEY
		memset(mptr->auth, 0, SML_CRYPT_SIZE);
#endif
#endif // USE_SML_DECRYPT
  }
}

void sml_free_vars(void) {
SETREGS
  if (sml_globs.script_meter) {
    // restart condition
    free(sml_globs.script_meter);
    if (sml_globs.meter_vars) {
      free(sml_globs.meter_vars);
      sml_globs.meter_vars = 0;
    }
    if (sml_globs.dvalid) {
      free(sml_globs.dvalid);
      sml_globs.dvalid = 0;
    }
#ifdef USE_SML_MEDIAN_FILTER
    if (sml_globs.sml_mf) {
      free(sml_globs.sml_mf);
      sml_globs.sml_mf = 0;
    }
#endif

#ifdef USE_SML_CANBUS
#ifdef ESP32
    if (sml_globs.twai_installed) {
      ptwai_stop();
      ptwai_driver_uninstall();
      sml_globs.twai_installed = false;
    }
#endif
#endif // USE_SML_CANBUS
    reset_sml_vars(sml_globs.meters_used);
  }
 }

int32_t SML_Init_0(void) {
ALLOCMEM
  int32_t result = SML_Init();
  if (result) {
    SML_Deinit();
    return result;
  }

  sml_globs.logsize = SML_DUMP_SIZE;
  sml_globs.ser_act_LED_pin = 255;
  sml_globs.sml_options = SML_OPTIONS_JSON_ENABLE;

  sml_globs.sml_cnt_index[0] = 0;
  sml_globs.sml_cnt_index[1] = 1;
  sml_globs.sml_cnt_index[2] = 2;
  sml_globs.sml_cnt_index[3] = 3;

  sml_globs.smltab.SML_SetBaud = &SML_SetBaud;
  sml_globs.smltab.sml_status = &sml_status;
  sml_globs.smltab.SML_Write = &SML_Write;
  sml_globs.smltab.SML_Read = &SML_Read;
  sml_globs.smltab.sml_getv = &sml_getv;
  sml_globs.smltab.SML_Shift_Num = &SML_Shift_Num;
  sml_globs.smltab.SML_GetVal = &SML_GetVal;
  sml_globs.smltab.SML_GetSVal = &SML_GetSVal;
  sml_globs.smltab.SML_Set_WStr = &SML_Set_WStr;
  sml_globs.smltab.SML_Decode = &SML_Decode;
  sml_globs.smltab.SML_SetOptions = &SML_SetOptions;

  uint8_t **bpt = (uint8_t**)&sml_globs.smltab;
  for (uint32_t cnt = 0; cnt < 11; cnt++) {
    //AddLog(LOG_LEVEL_INFO, PSTR(">>> 1 %08x"), (uint32_t)*bpt);
    *bpt += EXEC_OFFSET;
    //AddLog(LOG_LEVEL_INFO, PSTR(">>> 2 %08x"), (uint32_t)*bpt);
    bpt++;
  }
  return result;
}

int32_t SML_Init(void) {
SETREGS

  STGLOB

  GETDCONSTP
  GETICONSTP

  sml_globs.ready = false;

  if (!bitRead(Settings->rule_enabled, 0)) {
    return 1;
  }

	sml_globs.mptr = meter_desc;

  //uint8_t meter_script = Run_Scripter(">M", -2, 0);
  //if (meter_script != 99) {
  //  AddLog(LOG_LEVEL_INFO, PSTR("no meter section found!"));
  //  return;
 // }
//char *lp = glob_script_mem.section_ptr;

  char *lp = GetScriptSection_P(PSTR(">M"));
  if (!lp) {
    AddLog(LOG_LEVEL_INFO, PSTR("no meter section found!"));
    return 1;
  }

  char *savelp = lp;
  uint8_t new_meters_used;

  // use script definition
  sml_free_vars();

  if (*lp == '>' && *(lp + 1) == 'M') {
    lp += 2;
    sml_globs.meters_used = strtol(lp, &lp, 10);
  } else {
    return 1;
  }

  sml_globs.maxvars = 0;

  reset_sml_vars(sml_globs.meters_used);

  sml_globs.sml_desc_cnt = 0;

  sml_globs.script_meter = 0;
  uint8_t *tp = 0;
  uint16_t index = 0;
  uint8_t section = 0;
  int8_t srcpin = 0;
  uint32_t mlen;
	uint16_t memory = 0;

#ifdef ESP32
  uint32_t uart_index = E32_SOC_UART_HP_NUM - 1;
#endif

  sml_globs.sml_send_blocks = 0;
  lp = savelp;
  struct METER_DESC *mmp;
  while (lp) {
      if (!section) {
        if (*lp == '>' && *(lp + 1) == 'M') {
          lp += 2;
          section = 1;
          mlen = SML_getscriptsize(lp);
          if (mlen == 0) return 1; // missing end #
          sml_globs.script_meter = (uint8_t*)calloc(mlen, 1);
					memory += mlen;
          if (!sml_globs.script_meter) {
            goto dddef_exit;
          }
          tp = sml_globs.script_meter;
          goto next_line;
        }
      }
      else {
        if (!*lp || *lp == '#' || *lp == '>') {
          if (*(tp - 1) == '|') *(tp - 1) = 0;
          break;
        }
        if (*lp == '+') {
          // add descriptor +1,1,c,0,10,H20
          //toLogEOL(">>",lp);
          lp++;
          char *lp1;
#ifdef SML_REPLACE_VARS
          char dstbuf[SML_SRCBSIZE*2];
          Replace_Cmd_Vars(lp, 1, dstbuf, sizeof(dstbuf));
          lp += SML_getlinelen(lp);
				  lp1 = dstbuf;
#else   
          lp1 = lp;
          lp += SML_getlinelen(lp);
#endif
          index = *lp1 & 7;
          lp1 += 2;
          if (index < 1 || index > sml_globs.meters_used) {
            AddLog(LOG_LEVEL_INFO, PSTR("illegal meter number!"));
            goto next_line;
          }
          index--;
          mmp = &meter_desc[index];
          if (*lp1 == '[') {
            // sign TCP mode
            srcpin = TCP_MODE_FLG;
            lp1++;
            char str[32];
            uint8_t cnt;
            for (cnt = 0; cnt < sizeof(str) - 1; cnt++) {
              if (!*lp1 || *lp1 == '\n' || *lp1 == ']') {
                break;
              }
              str[cnt] = *lp1++;
            }
            str[cnt] = 0;
            lp1++;
#ifdef USE_SML_TCP
#ifdef USE_SML_TCP_IP_STR
            strcpy_P(mmp->ip_addr, str);
#else
            //mmp->ip_addr.fromString(str);
            ipa_fromstring(&mmp->ip_addr, str);
#endif
#endif

          } else {
            srcpin  = strtol(lp1, &lp1, 10);
            if (Gpio_used(abs(srcpin))) {
              AddLog(LOG_LEVEL_INFO, PSTR("SML: Error: Duplicate GPIO %d defined. Not usable for RX in meter number %d"), abs(srcpin), index + 1);
dddef_exit:
              if (sml_globs.script_meter) free(sml_globs.script_meter);
              sml_globs.script_meter = 0;
              return 1;
            }
            if (!ValidPin(abs(srcpin))) {
              AddLog(LOG_LEVEL_INFO, PSTR("SML: Error: forbidden GPIO %d defined. Not usable for RX in meter number %d"), abs(srcpin), index + 1);
              goto dddef_exit;
            }
          }
          mmp->srcpin = srcpin;
          if (*lp1 != ',') goto next_line;
          lp1++;
          mmp->type = *lp1;
          lp1++;
          if (*lp1 != ',') {
            switch (*lp1) {
              case 'N':
                lp1++;
                mmp->sopt = 0x10 | (*lp1 & 3);
                lp1++;
                break;
              case 'E':
                lp1++;
                mmp->sopt = 0x20 | (*lp1 & 3);
                lp1++;
                break;
              case 'O':
                lp1++;
                mmp->sopt = 0x30 | (*lp1 & 3);
                lp1++;
                break;
              default:
                mmp->sopt = *lp1&7;
                lp1++;
            }
          } else {
            mmp->sopt = 0;
          }
          lp1++;
          mmp->flag = strtol(lp1, &lp1, 10);
          if (*lp1 != ',') goto next_line;
          lp1++;
          mmp->params = strtol(lp1, &lp1, 10);
          if (*lp1 != ',') goto next_line;
          lp1++;
          for (uint32_t cnt = 0; cnt < SML_PREFIX_SIZE; cnt++) {
            if (!*lp1 || *lp1 == SCRIPT_EOL || *lp1 == ',') {
              mmp->prefix[cnt] = 0;
              break;
            }
            mmp->prefix[cnt] = *lp1++;
          }
          mmp->prefix[SML_PREFIX_SIZE - 1] = 0;

          if (*lp1 == ',') {
            lp1++;
            // get TRX pin
            mmp->trxpin = strtol(lp1, &lp1, 10);
            if (mmp->srcpin != TCP_MODE_FLG) {
              if (Gpio_used(mmp->trxpin)) {
                AddLog(LOG_LEVEL_INFO, PSTR("SML: Error: Duplicate GPIO %d defined. Not usable for TX in meter number %d"), meter_desc[index].trxpin, index + 1);
                goto dddef_exit;
              }
              if (!ValidPin(mmp->trxpin)) {
                AddLog(LOG_LEVEL_INFO, PSTR("SML: Error: forbidden GPIO %d defined. Not usable for TX in meter number %d"), meter_desc[index].trxpin, index + 1);
                goto dddef_exit;
              }
            }
            // optional transmit enable pin
            if (*lp1 == '(') {
              lp1++;
              if (*lp1 == 'i') {
                lp1++;
                mmp->trx_en.trxenpol = 1;
              } else {
                mmp->trx_en.trxenpol = 0;
              }
              mmp->trx_en.trxenpin = strtol(lp1, &lp1, 10);
              if (*lp1 != ')') {
                goto dddef_exit;
              }
              lp1++;
              if (Gpio_used(mmp->trx_en.trxenpin)) {
                AddLog(LOG_LEVEL_INFO, PSTR("SML: Error: Duplicate GPIO %d defined. Not usable for TX enable in meter number %d"), meter_desc[index].trx_en.trxenpin, index + 1);
                goto dddef_exit;
              }
              mmp->trx_en.trxen = 1;
              pinMode(mmp->trx_en.trxenpin, OUTPUT);
              digitalWrite(mmp->trx_en.trxenpin, mmp->trx_en.trxenpol);
            } else {
              mmp->trx_en.trxen = 0;
            }
            if (*lp1 != ',') goto next_line;
            lp1++;
            mmp->tsecs = strtol(lp1, &lp1, 10);
            // optional values to send
            if (*lp1 == ',') {
              lp1++;
              // look ahead, lp points to next line
              char *txbuff = (char *)special_malloc(SML_TRX_BUFF_SIZE);
              if (!txbuff) {
                goto dddef_exit;
              }
              char *txb1 = txbuff;
              char *txp = lp1;
              uint16_t tx_entries = 1;
              uint16_t txlen = 0;
              while (1) {
                if (!*lp1 || (*lp1 == SCRIPT_EOL)) {
                  if (*(lp1 - 1) == ',') {
                    // line ends with comma, add another line
                    while (*lp == SCRIPT_EOL) lp++;
#ifdef SML_REPLACE_VARS
                    Replace_Cmd_Vars(lp, 1, dstbuf, sizeof(dstbuf));
                    lp += SML_getlinelen(lp);
				            lp1 = dstbuf;
#else   
                    lp1 = lp;
                    lp += SML_getlinelen(lp);
#endif
                  } else {
                    break;
                  }
                }
                if (*lp1 == ',') tx_entries++;
                *txb1++ = *lp1++;
                txlen++;
                if (txlen >= SML_TRX_BUFF_SIZE - 2) {
                  break;
                }
              }
              // tx lines complete
              *txb1 = 0;
              //AddLog(LOG_LEVEL_INFO, PSTR("SML: >>> %s - %d - %d"), txbuff, txlen, tx_entries);
              mmp->txmem = (char*)realloc(txbuff, txlen + 2);
              memory += txlen + 2;
              mmp->index = 0;
              mmp->max_index = tx_entries;
              sml_globs.sml_send_blocks++;
              // end collect transmit values
            }
          }
          if (*lp1 == SCRIPT_EOL) lp1--;
          goto next_line;
        }
				char *lp1;
#ifdef SML_REPLACE_VARS
        char dstbuf[SML_SRCBSIZE*2];
        Replace_Cmd_Vars(lp, 1, dstbuf, sizeof(dstbuf));
        lp += SML_getlinelen(lp);
				lp1 = dstbuf;
#else
				lp1 = lp;
				lp += SML_getlinelen(lp);
#endif // SML_REPLACE_VARS

        //AddLog(LOG_LEVEL_INFO, PSTR("%s"),dstbuf);
        if (*lp1 == '-' || isdigit(*lp1)) {
          //toLogEOL(">>",lp);
          // add meters line -1,1-0:1.8.0*255(@10000,H2OIN,cbm,COUNTER,4|
          if (*lp1 == '-') lp1++;
          uint8_t mnum = strtol(lp1, 0, 10);
          if (mnum < 1 || mnum > sml_globs.meters_used) {
            AddLog(LOG_LEVEL_INFO, PSTR("illegal meter number!"));
            goto next_line;
          }
          // 1,=h—————————————
          if (!strncmp_P(lp1 + 1, PSTR(",=h"), 3) || !strncmp_P(lp1 + 1, PSTR(",=so"), 4)) {
            if (!strncmp_P(lp1 + 1, PSTR(",=so"), 4)) {
							SpecOptions(lp1 + 5, mnum - 1);
            }
          } else {
            sml_globs.maxvars++;
          }

          while (1) {
            if (*lp1 == 0) {
              *tp++ = '|';
              goto next_line;
            }
            *tp++ = *lp1++;
            index++;
            if (index >= METER_DEF_SIZ) break;
          }
        }
      }

next_line:
      if (*lp == SCRIPT_EOL) {
        lp++;
      } else {
        lp = strchr(lp, SCRIPT_EOL);
        if (!lp) break;
        lp++;
      }
    }

    *tp = 0;
    sml_globs.meter_p = sml_globs.script_meter;

    // set serial buffers
  for (uint32_t meters = 0; meters < sml_globs.meters_used; meters++ ) {
    struct METER_DESC *mptr = &meter_desc[meters];
    if (mptr->sbsiz) {
      mptr->sbuff = (uint8_t*)calloc(mptr->sbsiz, 1);
			memory += mptr->sbsiz;
    }
  }

  // initialize hardware
  typedef void (*function)();
  uint8_t cindex = 0;
  // preloud counters
  for (uint8_t i = 0; i < MAX_COUNTERS; i++) {
      RtcSettings->pulse_counter[i] = Settings->pulse_counter[i];
      sml_globs.sml_counters[i].sml_cnt_last_ts = millis();
  }

  for (uint8_t meters = 0; meters < sml_globs.meters_used; meters++) {
    METER_DESC *mptr = &meter_desc[meters];
    if (mptr->type == 'c') {
        if (mptr->flag & ANALOG_FLG) {
          // not used
        } else {
          // counters, set to input with pullup
          if (mptr->flag & PULLUP_FLG) {
            pinMode(mptr->srcpin, INPUT_PULLUP);
          } else {
            pinMode(mptr->srcpin, INPUT);
          }
          // check for irq mode
          if (mptr->params <= 0) {
            // init irq mode
            sml_globs.sml_counters[cindex].sml_cnt_old_state = meters;
            sml_globs.sml_counters[cindex].sml_debounce = -sml_globs.mptr[meters].params;
            //attachInterruptArg(mptr->srcpin, SML_CounterIsr, &sml_cnt_index[cindex], CHANGE);
            attachInterruptArg(&sml_globs.sml_counters,  mptr->srcpin, &sml_globs.sml_cnt_index[cindex], CHANGE); 
            if (digitalRead(mptr->srcpin) > 0) {
              sml_globs.sml_counters[cindex].pinstate = 1;
            }
            sml_globs.sml_counters[cindex].sml_counter_ltime = millis();
          }

          RtcSettings->pulse_counter[cindex] = Settings->pulse_counter[cindex];
          InjektCounterValue(meters, RtcSettings->pulse_counter[cindex], SFPC_0);
          cindex++;
        }
    } else if (mptr->type == 'C') {
#ifdef USE_SML_CANBUS

#ifdef ESP8266
      mptr->mcp2515 = nullptr;
      if ( PinUsed(GPIO_SPI_MISO) && PinUsed(GPIO_SPI_MOSI) && PinUsed(GPIO_SPI_CLK) ) {
        mptr->mcp2515 = new MCP2515(mptr->srcpin);
        if (MCP2515::ERROR_OK != mptr->mcp2515->reset()) {
          AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: Failed to reset module"));
          return 1;
        }

        if (MCP2515::ERROR_OK != mptr->mcp2515->setBitrate((CAN_SPEED)(mptr->params%100), (CAN_CLOCK)(mptr->params/100))) {
          AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: Failed to set module bitrate"));
          return 1;
        }

        //attachInterrupt(mptr->trxpin, sml_canbus_irq, FALLING);

        if (MCP2515::ERROR_OK != mptr->mcp2515->setConfigMode()) {
          AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: Failed to set config mode"));
        } else {
          if (mptr->can_filters[0]) mptr->mcp2515->setFilter(MCP2515::RXF0, true, mptr->can_filters[0]);
          if (mptr->can_filters[1]) mptr->mcp2515->setFilter(MCP2515::RXF1, true, mptr->can_filters[1]);
          if (mptr->can_filters[2]) mptr->mcp2515->setFilter(MCP2515::RXF2, true, mptr->can_filters[2]);
          if (mptr->can_filters[3]) mptr->mcp2515->setFilter(MCP2515::RXF3, true, mptr->can_filters[3]);
          if (mptr->can_filters[4]) mptr->mcp2515->setFilter(MCP2515::RXF4, true, mptr->can_filters[4]);
          if (mptr->can_filters[5]) mptr->mcp2515->setFilter(MCP2515::RXF5, true, mptr->can_filters[5]);

          if (mptr->can_masks[0]) mptr->mcp2515->setFilterMask(MCP2515::MASK0, true, mptr->can_masks[0]);
          if (mptr->can_masks[1]) mptr->mcp2515->setFilterMask(MCP2515::MASK1, true, mptr->can_masks[1]);

         }

        if (MCP2515::ERROR_OK != mptr->mcp2515->setNormalMode()) {
          AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: Failed to set normal mode"));
          return 1;
        }

        AddLog(LOG_LEVEL_INFO, PSTR("SML CAN: Initialized"));
      } else {
        AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: SPI not configuered"));
      }
 #else
      // Initialize configuration structures using macro initializers
      twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)mptr->trxpin, (gpio_num_t)mptr->srcpin, TWAI_MODE_NORMAL);
      uint8_t qlen = mptr->params/100;
      if (qlen < 8) {
        qlen = 8;
      }
      g_config.rx_queue_len = qlen;
      twai_timing_config_t t_config;
      switch (mptr->params%100) {
        case 0:
          t_config = TWAI_TIMING_CONFIG_25KBITS();
          break;
        case 1:
          t_config = TWAI_TIMING_CONFIG_50KBITS();
          break;
        case 2:
          t_config = TWAI_TIMING_CONFIG_100KBITS();
          break;
        case 3:
          t_config = TWAI_TIMING_CONFIG_125KBITS();
          break;
        case 4:
          t_config = TWAI_TIMING_CONFIG_250KBITS();
          break;
        case 5:
          t_config = TWAI_TIMING_CONFIG_500KBITS();
          break;
        case 6:
          t_config = TWAI_TIMING_CONFIG_800KBITS();
          break;
        case 7:
          t_config = TWAI_TIMING_CONFIG_1MBITS();
          break;
        default:
          t_config = TWAI_TIMING_CONFIG_125KBITS();
          break;
      }
    
      twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

      if (mptr->can_filters[0]) {
        f_config.acceptance_code = mptr->can_filters[0] << 3; 
        f_config.acceptance_mask = mptr->can_masks[0] << 3; 
        f_config.single_filter = true;
      }
      sml_globs.twai_installed = false;
      // Install TWAI driver
      if (ptwai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        AddLog(LOG_LEVEL_DEBUG, PSTR("Can driver installed"));
        // Start TWAI driver
        if (ptwai_start() == ESP_OK) {
          AddLog(LOG_LEVEL_DEBUG, PSTR("Can driver started"));
          // Reconfigure alerts to detect frame receive, Bus-Off error and RX queue full states
          uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR;
          if (ptwai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
            AddLog(LOG_LEVEL_DEBUG, PSTR("CAN Alerts reconfigured"));
            AddLog(LOG_LEVEL_INFO, PSTR("Can driver ready"));
            sml_globs.twai_installed = true;
          } else {
            AddLog(LOG_LEVEL_DEBUG, PSTR("Failed to reconfigure CAN alerts"));
          } 
        } else {
          AddLog(LOG_LEVEL_DEBUG, PSTR("Failed to start can driver"));
        }
      } else {
        AddLog(LOG_LEVEL_DEBUG, PSTR("Failed to install can driver"));
      }
 #endif     
#endif // USE_SML_CANBUS
    } else {
      // serial input, init
      if (mptr->srcpin == TCP_MODE_FLG) {
#ifdef USE_SML_TCP
        sml_tcp_init(mptr);
#endif
      } else {
        // serial mode
#ifdef ESP8266
        TSPARS spars;
        spars.rxpin = mptr->srcpin;
        spars.txpin = mptr->trxpin;
        spars.hwfb = 1;
        spars.nwmode = 0;
        spars.bsize = mptr->sibsiz;
        spars.speed = mptr->params;
        spars.invert = mptr->so_flags.SO_TRX_INVERT;
#ifdef SPECIAL_SS
        char type = mptr->type;
        if (type == 'm' || type == 'M' || type == 'k' || type == 'p' || type == 'R' || type == 'v') {
          mptr->meter_ss = New_TSerial(&spars);
        } else {
          spars.nwmode = 1;
          mptr->meter_ss = New_TSerial(&spars);
        }
#else
        mptr->meter_ss = New_TSerial(&spars);
#endif  // SPECIAL_SS
#endif // ESP8266

#ifdef ESP32
        // use hardware serial
        if (mptr->uart_index >= 0) {
          uart_index = mptr->uart_index;
        }
        AddLog(LOG_LEVEL_INFO, PSTR("SML: uart used: %d"),uart_index);
#ifdef USE_ESP32_SW_SERIAL
        mptr->meter_ss = New_E32Serial(uart_index);
        if (mptr->srcpin >= 0) {
          if (uart_index == 0) { ClaimSerial(); }
          uart_index--;
          if (uart_index < 0) uart_index = 0;
        }
#else
        mptr->meter_ss = new HardwareSerial(uart_index);
        if (uart_index == 0) { ClaimSerial(); }
        uart_index--;
        if (uart_index < 0) uart_index = 0;
        mptr->meter_ss->setRxBufferSize(mptr->sibsiz);
#endif // USE_ESP32_SW_SERIAL

#endif  // ESP32

        uint32_t smode = SERIAL_8N1;

        if (mptr->sopt & 0xf0) {
          // new serial config
          switch (mptr->sopt >> 4) {
            case 1:
              if ((mptr->sopt & 1) == 1) smode = SERIAL_8N1;
              else smode = SERIAL_8N2;
              break;
            case 2:
              if ((mptr->sopt & 1) == 1) smode = SERIAL_8E1;
              else smode = SERIAL_8E2;
              break;
            case 3:
              if ((mptr->sopt & 1) == 1) smode = SERIAL_8O1;
              else smode = SERIAL_8O2;
              break;
          }
        } else {
          // deprecated serial config
          if (mptr->sopt == 2) {
            smode = SERIAL_8N2;
          }
          if (mptr->type=='M') {
            smode = SERIAL_8E1;
            if (mptr->sopt == 2) {
              smode = SERIAL_8E2;
            }
          }
        }

#ifdef ESP8266
/*
        if (mptr->meter_ss->begin(mptr->params)) {
          mptr->meter_ss->flush();
        }
        if (mptr->meter_ss->hardwareSerial()) {
          Serial.begin(mptr->params, (SerialConfig)smode);
          ClaimSerial();
          if (mptr->so_flags.SO_TRX_INVERT) {
            U0C0 = U0C0 | BIT(UCRXI) | BIT(UCTXI); // Inverse RX, TX
          }
        }
*/
        
        if (TSerial_Begin(mptr->meter_ss, mptr->params, SERIAL_8N1)) {
          TSerial_Flush(mptr->meter_ss);
        }
        if (TSerial_Hardwareserial(mptr->meter_ss)) {
          //Serial.begin(mptr->params, (SerialConfig)smode);
          ClaimSerial();
        }


#endif  // ESP8266

#ifdef ESP32
        //mptr->meter_ss->begin(mptr->params, smode, mptr->srcpin, mptr->trxpin, mptr->so_flags.SO_TRX_INVERT);
        TSPARS spars;
        spars.rxpin = mptr->srcpin;
        spars.txpin = mptr->trxpin;
        spars.hwfb = 1;
        spars.nwmode = smode;
        spars.bsize = mptr->sibsiz;
        spars.speed = mptr->params;
        spars.invert = mptr->so_flags.SO_TRX_INVERT;
        E32Serial_Begin(mptr->meter_ss, &spars);
        if (mptr->so_flags.SO_DISS_PULL) {
          jgpio_pullup_dis((gpio_num_t)mptr->srcpin);
        }
#ifdef USE_ESP32_SW_SERIAL
				E32Serial_RxBufferSize(mptr->meter_ss, mptr->sibsiz);
#endif
#endif  // ESP32
      }
    }
  }

  sml_globs.meter_vars = (double*)calloc(sml_globs.maxvars, sizeof(double));
  sml_globs.dvalid = (uint8_t*)calloc(sml_globs.maxvars, sizeof(uint8_t));

#ifdef USE_SML_MEDIAN_FILTER
  sml_globs.sml_mf = (struct SML_MEDIAN_FILTER*)calloc(sml_globs.maxvars, sizeof(struct SML_MEDIAN_FILTER));
#endif

  if (!sml_globs.maxvars || !sml_globs.meter_vars || !sml_globs.dvalid
#ifdef USE_SML_MEDIAN_FILTER
   || !sml_globs.sml_mf
#endif
  ) {
    AddLog(LOG_LEVEL_INFO, PSTR("sml memory error!"));
    return 1;
  }

  memory += sizeof(sml_globs) + sizeof(meter_desc) + sml_globs.maxvars * (sizeof(double) +  sizeof(uint8_t) + sizeof(struct SML_MEDIAN_FILTER));

 // AddLog(LOG_LEVEL_INFO, PSTR(">>: %d - %d - %d"),sizeof(sml_globs), sizeof(meter_desc),sml_globs.maxvars * (sizeof(double) + sizeof(uint8_t) + sizeof(struct SML_MEDIAN_FILTER)));
 // 324 - 540 - 159

  mt->mem_size = memory;

  AddLog(LOG_LEVEL_INFO, PSTR("meters: %d , decode lines: %d, memory used: %d bytes"), sml_globs.meters_used, sml_globs.maxvars, memory);

// speed optimize shift flag
  for (uint32_t meters = 0; meters < sml_globs.meters_used; meters++ ) {
    struct METER_DESC *mptr = &meter_desc[meters];
    volatile char type = mptr->type;

    uint8_t iob = mptr->so_flags.SO_OBIS_LINE;

    if (!iob) {
      mptr->shift_mode = (type != 'e' && type != 'k' && type != 'm' && type != 'M' && type != 'p' && type != 'R' && type != 'v');
    } else {
      mptr->shift_mode = (type != 'o' && type != 'e' && type != 'k' && type != 'm' && type != 'M' && type != 'p' && type != 'R' && type != 'v');
    }

#ifdef USE_SML_DECRYPT
		if (mptr->use_crypt) {
      HP_PARS hpars;
      uint8_t *ucp = (uint8_t *)&serial_dispatch;
      ucp += EXEC_OFFSET;
      hpars.sd = (uint16_t (*)(uint8_t, uint8_t))ucp;
      hpars.meter = meters;
      hpars.key = mptr->key;
#ifdef USE_SML_AUTHKEY
      hpars.auth = mptr->auth;
      mptr->hp = NewHanParser(&hpars);
#else
      hpars.auth = nullptr;
			mptr->hp = NewHanParser(&hpars);
#endif
      mptr->crypflags = 0;
		}
#endif // USE_SML_DECRYPT
  }

  initialized = 1;
  sml_globs.ready = true;

  return 0;
}


#ifdef USE_SML_SCRIPT_CMD

uint32_t SML_SetBaud(uint32_t meter, uint32_t br) {
SETREGS

  if (sml_globs.ready == false) return 0;
  if (meter < 1 || meter > sml_globs.meters_used) return 0;
  meter--;
  if (!meter_desc[meter].meter_ss) return 0;

#ifdef ESP8266
  //if (meter_desc[meter].meter_ss->begin(br)) {
  if (TSerial_Begin(meter_desc[meter].meter_ss, br, SERIAL_8N1)) {
    //meter_desc[meter].meter_ss->flush();
    TSerial_Flush(meter_desc[meter].meter_ss);
  }
  if (TSerial_Hardwareserial(meter_desc[meter].meter_ss)) {
    if (sml_globs.mptr[meter].type=='M') {
      // >>>>>> Serial.begin(br, SERIAL_8E1);
    }
  }
#endif  // ESP8266

#ifdef ESP32
  TSerial_Flush(meter_desc[meter].meter_ss);
  E32Serial_SetBaudrate(meter_desc[meter].meter_ss, br);
  /*
  if (sml_globs.mptr[meter].type=='M') {
    meter_desc.meter_ss[meter]->begin(br,SERIAL_8E1,sml_globs.mptr[meter].srcpin,sml_globs.mptr[meter].trxpin);
  } else {
    meter_desc.meter_ss[meter]->begin(br,SERIAL_8N1,sml_globs.mptr[meter].srcpin,sml_globs.mptr[meter].trxpin);
  }*/
#endif  // ESP32
  return 1;
}

uint32_t sml_status(uint32_t meter) {
SETREGS

  if (sml_globs.ready == false) return 0;
  if (meter < 1 || meter > sml_globs.meters_used) return 0;
  meter--;
#if defined(ED300L) || defined(AS2020) || defined(DTZ541) || defined(USE_SML_SPECOPT)
  return sml_globs.sml_status[meter];
#else
  return 0;
#endif
}

uint32_t SML_Write(int32_t meter, char *hstr) {
SETREGS

  if (sml_globs.ready == false) return 0;
  int8_t flag = meter;
  meter = abs(meter);
  if (meter < 1 || meter > sml_globs.meters_used) return 0;
  meter--;
  if (meter_desc[meter].type != 'C') {
    if (!meter_desc[meter].meter_ss) return 0;
  }
  if (flag > 0) {
    SML_Send_Seq(meter, hstr);
  } else {
    // 9600:8E1, only hardware serial
    uint32_t baud = strtol(hstr, &hstr, 10);
    hstr++;
    // currently only 8 bits and ignore stopbits
    hstr++;
    uint32_t smode;
    switch (*hstr) {
      case 'N':
        smode = SERIAL_8N1;
        break;
      case 'E':
        smode = SERIAL_8E1;
        break;
      case 'O':
        smode = SERIAL_8O1;
        break;
    }

    struct METER_DESC *mptr = &meter_desc[meter];

#ifdef ESP8266
    // >>>> needs fix
    //Serial.begin(baud, (SerialConfig)smode);
    TSPARS spars;
    spars.rxpin = mptr->srcpin;
    spars.txpin = mptr->trxpin;
    spars.hwfb = 1;
    spars.nwmode = smode;
    spars.bsize = mptr->sibsiz;
    spars.speed = mptr->params;
    spars.invert = mptr->so_flags.SO_TRX_INVERT;
    mptr->meter_ss = New_TSerial(&spars);

#else
    //meter_desc[meter].meter_ss->begin(baud, smode, sml_globs.mptr[meter].srcpin, sml_globs.mptr[meter].trxpin, sml_globs.mptr[meter].so_flags.SO_TRX_INVERT);
    TSPARS spars;
    spars.rxpin = mptr->srcpin;
    spars.txpin = mptr->trxpin;
    spars.hwfb = 1;
    spars.nwmode = smode;
    spars.bsize = mptr->sibsiz;
    spars.speed = mptr->params;
    spars.invert = mptr->so_flags.SO_TRX_INVERT;
    E32Serial_Begin(mptr->meter_ss, &spars);

    if (sml_globs.mptr[meter].so_flags.SO_DISS_PULL) {
      jgpio_pullup_dis((gpio_num_t)sml_globs.mptr[meter].srcpin);
    }
#endif
  }
  return 1;
}

uint32_t SML_Read(int32_t meter, char *str, uint32_t slen) {
SETREGS

  if (sml_globs.ready == false) return 0;

  uint8_t hflg = 0;
  if (meter < 0) {
    meter = abs(meter);
    hflg = 1;
  }
  if (meter < 1 || meter > sml_globs.meters_used) return 0;
  meter--;
  if (!meter_desc[meter].meter_ss) return 0;

  struct METER_DESC *mptr = &meter_desc[meter];

  if (!mptr->spos) {
    return 0;
  }

  mptr->sbuff[mptr->spos] = 0;

  if (!hflg) {
    strlcpy(str, (char*)&mptr->sbuff[0], slen);
  } else {
    uint32_t index = 0;
    for (uint32_t cnt = 0; cnt < mptr->spos; cnt++) {
      sprintf_P(str, PSTR("%02x"), mptr->sbuff[cnt]);
      str += 2;
      index += 2;
      if (index >= slen - 2) break;
    }
  }
  mptr->spos = 0;
  return 1;
}

uint32_t sml_getv(uint32_t sel) {
SETREGS

  if (sml_globs.ready == false) return 0;
  if (!sel) {
    for (uint8_t cnt = 0; cnt < sml_globs.maxvars; cnt++) {
      sml_globs.dvalid[cnt] = 0;
    }
    sel = 0;
  } else {
    if (sel < 1 || sel > sml_globs.maxvars) { sel = 1;}
    sel = sml_globs.dvalid[sel - 1];
  }
  return sel;
}

uint32_t SML_Shift_Num(uint32_t meter, uint32_t shift) {
SETREGS

  struct METER_DESC *mptr = &sml_globs.mptr[meter];
  if (shift > mptr->sbsiz) shift = mptr->sbsiz;
  for (uint16_t cnt = 0; cnt < shift; cnt++) {
     for (uint16_t count = 0; count < mptr->sbsiz - 1; count++) {
      mptr->sbuff[count] = mptr->sbuff[count + 1];
      SML_Decode(meter);
    }
  }
  return shift;
}


double SML_GetVal(uint32_t index) {
SETREGS
  
  GETDCONSTP
  if (sml_globs.ready == false) return SFPC_0;
  if (index < 1 || index > sml_globs.maxvars) { index = 1;}
  index--;
  return sml_globs.meter_vars[index];
}

char *SML_GetSVal(uint32_t index) {
SETREGS

  if (sml_globs.ready == false) return 0;
  if (index < 1 || index > sml_globs.meters_used) { index = 1;}
  return (char*)meter_desc[index - 1].meter_id;
}

int32_t SML_Set_WStr(uint32_t meter, char *hstr) {
SETREGS

  if (sml_globs.ready == false) return 0;
  if (meter < 1 || meter > sml_globs.meters_used) return -1;
  meter--;
  if (meter_desc[meter].type != 'C') {
    if (!meter_desc[meter].meter_ss) return -2;
  }
  meter_desc[meter].script_str = hstr;
  return 0;
}

#endif // USE_SML_SCRIPT_CMD


void SetDBGLed(uint8_t srcpin, uint8_t ledpin) {
SETREGS

    pinMode(ledpin, OUTPUT);
    if (digitalRead(srcpin)) {
      digitalWrite(ledpin,LOW);
    } else {
      digitalWrite(ledpin,HIGH);
    }
}

// force channel math on counters
void SML_Counter_Poll_1s(void) {
SETREGS

	for (uint32_t meter = 0; meter < sml_globs.meters_used; meter++) {
		if (sml_globs.mptr[meter].type == 'c') {
			SML_Decode(meter);
		}
	}
}

// fast counter polling
void SML_Counter_Poll(void) {
SETREGS

GETDCONSTP
GETICONSTP
STGLOB
uint16_t meters, cindex = 0;
uint32_t ctime = millis();

  for (meters = 0; meters < sml_globs.meters_used; meters++) {
    if (sml_globs.mptr[meters].type == 'c') {
      // poll for counters and debouce   
      if (sml_globs.mptr[meters].params > 0) {
        if (ctime - sml_globs.sml_counters[cindex].sml_cnt_last_ts > sml_globs.mptr[meters].params) {
          sml_globs.sml_counters[cindex].sml_cnt_last_ts = ctime;

          if (sml_globs.mptr[meters].flag & ANALOG_FLG) {
            // analog mode, get next value
          } else {
            // poll digital input
            uint8_t state;
            sml_globs.sml_counters[cindex].sml_cnt_debounce <<= 1;
            sml_globs.sml_counters[cindex].sml_cnt_debounce |= (digitalRead(sml_globs.mptr[meters].srcpin) & 1) | 0x80;
            if (sml_globs.sml_counters[cindex].sml_cnt_debounce == 0xc0) {
              // is 1
              state = 1;
            } else {
              // is 0, means switch down
              state = 0;
            }
            if (sml_globs.sml_counters[cindex].sml_cnt_old_state != state) {
              // state has changed
              sml_globs.sml_counters[cindex].sml_cnt_old_state = state;
              if (state == 0) {
                // inc counter
                RtcSettings->pulse_counter[cindex]++;
                sml_globs.sml_counters[cindex].sml_counter_pulsewidth = ctime - sml_globs.sml_counters[cindex].sml_counter_lfalltime;
                sml_globs.sml_counters[cindex].sml_counter_lfalltime = ctime;
                InjektCounterValue(meters, RtcSettings->pulse_counter[cindex], __divdf3(SFPC_60000, __floatunsidf(sml_globs.sml_counters[cindex].sml_counter_pulsewidth)));
              }
            }
          }          
        }
#ifdef DEBUG_CNT_LED1
        if (cindex == 0) SetDBGLed(sml_globs.mptr[meters].srcpin, DEBUG_CNT_LED1);
#endif
#ifdef DEBUG_CNT_LED2
        if (cindex == 1) SetDBGLed(sml_globs.mptr[meters].srcpin, DEBUG_CNT_LED2);
#endif
      } else {
        if (ctime - sml_globs.sml_counters[cindex].sml_cnt_last_ts > 10) {
          sml_globs.sml_counters[cindex].sml_cnt_last_ts = ctime;
#ifdef DEBUG_CNT_LED1
          if (cindex == 0) SetDBGLed(sml_globs.mptr[meters].srcpin, DEBUG_CNT_LED1);
#endif
#ifdef DEBUG_CNT_LED2
          if (cindex == 1) SetDBGLed(sml_globs.mptr[meters].srcpin, DEBUG_CNT_LED2);
#endif
        }

        if (sml_globs.sml_counters[cindex].sml_cnt_updated) {
          InjektCounterValue(meters, RtcSettings->pulse_counter[cindex], __divdf3(SFPC_60000, __floatunsidf(sml_globs.sml_counters[cindex].sml_counter_pulsewidth)));
          sml_globs.sml_counters[cindex].sml_cnt_updated = 0;
        }
				// check timeout
				uint32_t time = millis();
				if ((time - sml_globs.sml_counters[cindex].sml_counter_lfalltime) > CNT_PULSE_TOUT) {
					InjektCounterValue(meters, RtcSettings->pulse_counter[cindex], SFPC_0);
					sml_globs.sml_counters[cindex].sml_counter_lfalltime = time;
				}
      }
      cindex++;
    }
  }
}

#ifdef USE_SCRIPT

#ifdef USE_SML_CANBUS

#ifdef ESP32
#define POLLING_RATE_MS 100
uint32_t sml_can_check_alerts() {
SETREGS


  uint32_t alerts_triggered;
  ptwai_read_alerts(&alerts_triggered, pdMS_TO_TICKS(POLLING_RATE_MS));
  twai_status_info_t twai_status;
  ptwai_get_status_info(&twai_status);

  // Handle alerts
  if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("Alert: TWAI controller has become error passive."));
  }
  if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus."));
    AddLog(LOG_LEVEL_DEBUG, PSTR("Bus error count: %d"), twai_status.bus_error_count);
  }
  if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("Alert: The RX queue is full causing a received frame to be lost."));
    AddLog(LOG_LEVEL_DEBUG, PSTR("RX buffered: %d"), twai_status.msgs_to_rx);
    AddLog(LOG_LEVEL_DEBUG, PSTR("RX missed: %d"), twai_status.rx_missed_count);
    AddLog(LOG_LEVEL_DEBUG, PSTR("RX overrun %d"), twai_status.rx_overrun_count);
  }

  if (alerts_triggered & TWAI_ALERT_TX_FAILED) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("Alert: The Transmission failed."));
    AddLog(LOG_LEVEL_DEBUG, PSTR("TX buffered: %d"), twai_status.msgs_to_tx);
    AddLog(LOG_LEVEL_DEBUG, PSTR("TX error: %d"), twai_status.tx_error_counter);
    AddLog(LOG_LEVEL_DEBUG, PSTR("TX failed: %d"), twai_status.tx_failed_count);
  }
  
  if (alerts_triggered & TWAI_ALERT_TX_SUCCESS) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("Alert: The Transmission was successful."));
    AddLog(LOG_LEVEL_DEBUG, PSTR("TX buffered: %d"), twai_status.msgs_to_tx);
  }

  return alerts_triggered;
}

#endif // ESP32


#define SML_CAN_MAX_FRAMES 8

void SML_CANBUS_Read() {
SETREGS

#ifdef ESP8266
  struct can_frame canFrame;

  for (uint32_t meter = 0; meter < sml_globs.meters_used; meter++) {
    struct METER_DESC *mptr = &sml_globs.mptr[meter];
    uint8_t nCounter = 0;

    if (mptr->type != 'C') continue;

    if (mptr->mcp2515 == nullptr) continue;
sf
    while (mptr->mcp2515->checkReceive() && nCounter <= SML_CAN_MAX_FRAMES) {
      if (mptr->mcp2515->readMessage(&canFrame) == MCP2515::ERROR_OK) {
          mptr->sbuff[0] = canFrame.can_id >> 24;
          mptr->sbuff[1] = canFrame.can_id >> 16;
          mptr->sbuff[2] = canFrame.can_id >> 8;
          mptr->sbuff[3] = canFrame.can_id;
          mptr->sbuff[4] = canFrame.can_dlc;
          for (int i = 0; i < canFrame.can_dlc; i++) {
            mptr->sbuff[5 + i] = canFrame.data[i];
          }
          SML_Decode(meter);
          nCounter++;
      } else {
        if (mptr->mcp2515->checkError()) {
          uint8_t errFlags = mptr->mcp2515->getErrorFlags();
          mptr->mcp2515->clearRXnOVRFlags();
          AddLog(LOG_LEVEL_DEBUG, PSTR("SML CAN: Received error %d"), errFlags);
          break;
        }
      }
    }
  }
#else

  for (uint32_t meter = 0; meter < sml_globs.meters_used; meter++) {
    struct METER_DESC *mptr = &sml_globs.mptr[meter];
    uint8_t nCounter = 0;

    if (mptr->type != 'C') continue;

    if (sml_globs.twai_installed) {
        uint32_t alerts_triggered = sml_can_check_alerts();

        // Check if message is received
        if (alerts_triggered & TWAI_ALERT_RX_DATA) {
          // One or more messages received. Handle all.
          twai_message_t message;
          while (ptwai_receive(&message, 0) == ESP_OK) {
            mptr->sbuff[0] = message.identifier >> 24;
            mptr->sbuff[1] = message.identifier >> 16;
            mptr->sbuff[2] = message.identifier >> 8;
            mptr->sbuff[3] = message.identifier;
            mptr->sbuff[4] = message.data_length_code;
            for (int i = 0; i < message.data_length_code; i++) {
              mptr->sbuff[5 + i] = message.data[i];
            }
            SML_Decode(meter);
          }
        }
        
    }
  } 

#endif
}
#endif // USE_SML_CANBUS

char *SML_Get_Sequence(char *cp,uint32_t index) {
SETREGS

  if (!index) return cp;
  uint32_t cindex = 0;
  while (cp) {
    cp = strchr(cp, ',');
    if (cp) {
      cp++;
      cindex++;
      if (cindex == index) {
        return cp;
      }
    }
  }
  return cp;
}

void SML_Check_Send(void) {
SETREGS

  sml_globs.sml_100ms_cnt++;
  char *cp;
  for (uint32_t cnt = sml_globs.sml_desc_cnt; cnt < sml_globs.meters_used; cnt++) {
    if (meter_desc[cnt].trxpin >= 0 && (meter_desc[cnt].txmem || meter_desc[cnt].script_str)) {
      //AddLog(LOG_LEVEL_INFO, PSTR("100 ms>> %d - %s - %d"),sml_globs.sml_desc_cnt,meter_desc[cnt].txmem,meter_desc[cnt].tsecs);
      if ((sml_globs.sml_100ms_cnt >= meter_desc[cnt].tsecs)) {
        sml_globs.sml_100ms_cnt = 0;
        // check for scriptsync extra output
        if (meter_desc[cnt].script_str) {
          cp = meter_desc[cnt].script_str;
          meter_desc[cnt].script_str = 0;
        } else {
          //AddLog(LOG_LEVEL_INFO, PSTR("100 ms>> 2"),cp);
          if (meter_desc[cnt].max_index > 1) {
            meter_desc[cnt].index++;
            if (meter_desc[cnt].index >= meter_desc[cnt].max_index) {
              meter_desc[cnt].index = 0;
              sml_globs.sml_desc_cnt++;
            }
            cp = SML_Get_Sequence(meter_desc[cnt].txmem, meter_desc[cnt].index);
            //SML_Send_Seq(cnt,cp);
          } else {
            cp = meter_desc[cnt].txmem;
            //SML_Send_Seq(cnt,cp);
            sml_globs.sml_desc_cnt++;
          }
        }
        //AddLog(LOG_LEVEL_INFO, PSTR(">> %s"),cp);
        SML_Send_Seq(cnt,cp);
        if (sml_globs.sml_desc_cnt >= sml_globs.meters_used) {
          sml_globs.sml_desc_cnt = 0;
        }
        break;
      }
    } else {
      sml_globs.sml_desc_cnt++;
    }

    if (sml_globs.sml_desc_cnt >= sml_globs.meters_used) {
      sml_globs.sml_desc_cnt = 0;
    }
  }
}

void sml_hex_asci(uint32_t mindex, char *tpowstr) {
SETREGS

  char *cp = meter_desc[mindex].meter_id;
  uint16_t slen = strlen(cp);
  slen &= 0xfffe;
  uint16_t cnt;
  *tpowstr++ = '"';
  for (cnt = 0; cnt < slen; cnt += 2) {
    uint8_t iob = (sml_hexnibble(cp[cnt]) << 4) | sml_hexnibble(cp[cnt + 1]);
    *tpowstr++ = iob;
  }
  *tpowstr++ = '"';
  *tpowstr = 0;
}


uint8_t sml_hexnibble(char chr) {
SETREGS

  uint8_t rVal = 0;
  if (isdigit(chr)) {
    rVal = chr - '0';
  } else  {
    if (chr >= 'A' && chr <= 'F') rVal = chr + 10 - 'A';
    if (chr >= 'a' && chr <= 'f') rVal = chr + 10 - 'a';
  }
  return rVal;
}

uint32_t sml_hex32(char *cp) {
SETREGS

  uint32_t iob = (sml_hexnibble(*cp++) << 4) | sml_hexnibble(*cp++);
  uint32_t result = iob << 24;
  iob = (sml_hexnibble(*cp++) << 4) | sml_hexnibble(*cp++);
  result |= iob << 16;
  iob = (sml_hexnibble(*cp++) << 4) | sml_hexnibble(*cp++);
  result |= iob << 8;
  iob = (sml_hexnibble(*cp++) << 4) | sml_hexnibble(*cp++);
  result |= iob;
  return result;
}

typedef struct {
  uint16_t T_ID;
  uint16_t P_ID;
  uint16_t SIZE;
  uint8_t U_ID;
  uint8_t payload[8];
 } MODBUS_TCP_HEADER;

uint16_t sml_swap(uint16_t in) {
SETREGS

  return (in << 8) | in >> 8;
}

// send modbus TCP frame with payload
// given ip addr  and port in baudrate
void sml_tcp_send(uint32_t meter, uint8_t *sbuff, uint16_t slen) {
SETREGS

MODBUS_TCP_HEADER tcph;

  GETICONSTP

  //tcph.T_ID = sml_swap(0x1234);
  tcph.T_ID = random(SIPC_FFFF);

  tcph.P_ID = 0;
  tcph.SIZE = sml_swap(6);
  tcph.U_ID = *sbuff;

  sbuff++;
  for (uint8_t cnt = 0; cnt < slen - 3; cnt++) {
    tcph.payload[cnt] = *sbuff++;
  }

#ifdef USE_SML_TCP
  // AddLog(LOG_LEVEL_INFO, PSTR("slen >> %d "),slen);
  if (meter_desc[meter].client) {
    if (client_connected(meter_desc[meter].client)) {
      client_write(meter_desc[meter].client, (uint8_t*)&tcph, 7 + slen - 3);
    }
  }
#endif
}

#ifdef USE_SML_TCP
int32_t sml_tcp_init(struct METER_DESC *mptr) {
  SETREGS
  STGLOB
  StateBitfield test = TasmotaGlobal->global_state;
  if (!test.wifi_down) {
    if (!mptr->client) {
      // tcp mode
#ifdef USE_SML_TCP_SECURE
      mptr->client = New_WiFiClientSecure();
      //client(new BearSSL::WiFiClientSecure_light(1024,1024)) {
      sclient_setInsecure(mptr->client);
#else        
      mptr->client = New_WiFiClient();
#endif // USE_SML_TCP_SECURE
    }
    int32_t err = client_connect(mptr->client, mptr->ip_addr, mptr->params);
    char ipa[32];
#ifdef USE_SML_TCP_IP_STR
    strcpy_P(ipa, mptr->ip_addr);
#else
    //strcpy_P(ipa, mptr->ip_addr.toString().c_str());
    ipa_tostring(ipa, &mptr->ip_addr);
#endif
    if (!err) {
      AddLog(LOG_LEVEL_INFO, PSTR("SML: could not connect TCP to %s:%d"),ipa, mptr->params);
    } else {
      AddLog(LOG_LEVEL_INFO, PSTR("SML: connected TCP to %s:%d"),ipa, mptr->params);
    }
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("SML: could not connect TCP since wifi is down"));
    mptr->client = nullptr;
    return -1;
  }
  return 0;
}

#ifndef TCP_TIMEOUT
#define TCP_TIMEOUT 30
#endif

void sml_tcp_check(void) {
SETREGS

  sml_globs.to_cnt++;
  if (sml_globs.to_cnt > TCP_TIMEOUT) {
    sml_globs.to_cnt = 0;
    for (uint32_t meter = 0; meter < sml_globs.meters_used; meter++) {
      struct METER_DESC *mptr = &sml_globs.mptr[meter];
		  if (mptr->srcpin == TCP_MODE_FLG) {
			  if (!mptr->client) {
          sml_tcp_init(mptr);
        } else {
          if (!client_connected(mptr->client)) {
            sml_tcp_init(mptr);
          }
        }
		  }
	  }
  }
}
#endif // USE_SML_TCP


// send sequence every N Seconds
void SML_Send_Seq(uint32_t meter, char *seq) {
SETREGS

  GETICONSTP

  uint8_t sbuff[48];
  uint8_t *ucp = sbuff, slen = 0;
  char *cp = seq;
  uint8_t rflg = 0;
  if (*cp == 'r') {
    rflg = 1;
    cp++;
  }

  struct METER_DESC *mptr = &meter_desc[meter];
  while (*cp) {
    if (!*cp || !*(cp+1)) break;
    if (*cp == ',') break;
    uint8_t iob = (sml_hexnibble(*cp) << 4) | sml_hexnibble(*(cp + 1));
    cp += 2;
    *ucp++ = iob;
    slen++;
    if (slen >= sizeof(sbuff)-6) break; // leave space for checksum
  }
  if (mptr->type == 'm' || mptr->type == 'M' || mptr->type == 'k') {
    if (mptr->type == 'k') {
      // kamstrup, append crc, cr
      *ucp++ = 0;
      *ucp++ = 0;
      slen += 2;
      uint16_t crc = KS_calculateCRC(sbuff, slen);
      ucp -= 2;
      *ucp++ = highByte(crc);
      *ucp++ = lowByte(crc);

      // now check for escapes
      uint8_t ksbuff[24];
      ucp = ksbuff;
      *ucp++ = 0x80;
      uint8_t klen = 1;
      for (uint16_t cnt = 0; cnt < slen; cnt++) {
        uint8_t iob = sbuff[cnt];
        if ((iob == 0x80) || (iob == 0x40) || (iob == 0x0d) || (iob == 0x06) || (iob == 0x1b)) {
          *ucp++ = 0x1b;
          *ucp++ = iob ^= 0xff;
          klen += 2;
        } else {
          *ucp++ = iob;
          klen++;
        }
      }
      *ucp++ = 0xd;
      slen = klen + 1;
      memmove(sbuff, ksbuff, slen);
    } else {
      if (!rflg) {
        *ucp++ = 0;
        *ucp++ = 2;
        slen += 2;
      }
      // append crc
      uint16_t crc = MBUS_calculateCRC(sbuff, slen, SIPC_FFFF);
      *ucp++ = lowByte(crc);
      *ucp++ = highByte(crc);
      slen += 2;
    }

  }
  if (mptr->type == 'o') {
    for (uint32_t cnt = 0; cnt < slen; cnt++) {
      sbuff[cnt] |= (CalcEvenParity(sbuff[cnt]) << 7);
    }
  }
  if (mptr->type == 'p') {
    *ucp++ = 0xc0;
    *ucp++ = 0xa8;
    *ucp++ = 1;
    *ucp++ = 1;
    *ucp++ = 0;
    *ucp++ = SML_PzemCrc(sbuff, 6);
    slen += 6;
  }

  if (mptr->srcpin == TCP_MODE_FLG) {
    sml_tcp_send(meter, sbuff, slen);
  } else {
    if (mptr->type == 'C') {

#ifdef USE_SML__CANBUS
#ifdef ESP8266
      if (mptr->mcp2515 != nullptr) {
        struct can_frame canMsg;
        canMsg.can_id = (uint32_t) (sbuff[0] << 24 | sbuff[1] << 16 | sbuff[2] << 8 | sbuff[3]);
        canMsg.can_dlc = sbuff[4];
        for (uint8_t i = 0; i < canMsg.can_dlc; i++) {
          canMsg.data[i] = sbuff[i + 5];
        }
        mptr->mcp2515->sendMessage(&canMsg);
      }
#else
      if (sml_globs.twai_installed) {
        twai_message_t message;
        message.identifier = (uint32_t) (sbuff[0] << 24 | sbuff[1] << 16 | sbuff[2] << 8 | sbuff[3]);
        message.data_length_code = sbuff[4];
        for (uint8_t i = 0; i < message.data_length_code; i++) {
          message.data[i] = sbuff[i + 5];
        }

        GETUICONSTP

        message.flags = 0;
        if (message.identifier & SUIPC_0x80000000) {
          message.extd = 1;
          message.identifier &= SUIPC_0x7fffffff;
        }

        ptwai_clear_receive_queue();

        // Queue message for transmission
        if (ptwai_transmit(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
          AddLog(LOG_LEVEL_DEBUG, PSTR("Can message queued for transmission"));
        } else {
          AddLog(LOG_LEVEL_DEBUG, PSTR("Failed to queue can message for transmission"));
        }
      }
#endif
#endif // USE_SML_CANBUS
    } else { 
      if (mptr->trx_en.trxen) {
        digitalWrite(meter_desc[meter].trx_en.trxenpin, meter_desc[meter].trx_en.trxenpol ^ 1);
      }
      //mptr->meter_ss->flush();
      //mptr->meter_ss->write(sbuff, slen);
      //if (mptr->trx_en.trxen) {
        // must wait for all data sent
      //  mptr->meter_ss->flush();
      //  digitalWrite(mptr->trx_en.trxenpin, mptr->trx_en.trxenpol);
      //}
      TSerial_Flush(mptr->meter_ss);
      TSerial_Write(mptr->meter_ss, sbuff, slen);
      if (mptr->trx_en.trxen) {
        // must wait for all data sent
        TSerial_Flush(mptr->meter_ss);
        digitalWrite(mptr->trx_en.trxenpin, mptr->trx_en.trxenpol);
      }

    }
  }

  if (sml_globs.dump2log) {
#ifdef SML_DUMP_OUT_ALL
    Hexdump(sbuff, slen);
#else
    uint8_t type = sml_globs.mptr[(sml_globs.dump2log&7) - 1].type;
    if (type == 'm' || type == 'M' || type == 'k' || type == 'C') {
      Hexdump(sbuff, slen);
    }
#endif
  }

#ifdef MODBUS_DEBUG
  uint8_t type = mptr->type;
  if (!sml_globs.dump2log && (type == 'm' || type == 'M' || type == 'k')) {
    AddLog(LOG_LEVEL_INFO, PSTR("transmit index >> %d"),sml_globs.mptr[meter].index);
    Hexdump(sbuff, slen);
  }
#endif

}
#endif // USE_SCRIPT

uint16_t MBUS_calculateCRC(uint8_t *frame, uint8_t num, uint16_t start) {
SETREGS

  GETICONSTP

  uint16_t crc, flag;
  //crc = 0xFFFF;
  crc = start;
  for (uint32_t i = 0; i < num; i++) {
    crc ^= frame[i];
    for (uint32_t j = 8; j; j--) {
      if ((crc & 0x0001) != 0) {        // If the LSB is set
        crc >>= 1;                      // Shift right and XOR 0xA001
        crc ^= SIPC_A001;
      } else {                          // Else LSB is not set
        crc >>= 1;                      // Just shift right
      }
    }
  }
  return crc;
}


uint16_t KS_calculateCRC(const uint8_t *frame, uint8_t num) {
SETREGS

  GETICONSTP

  uint32_t crc = 0;
  for (uint32_t i = 0; i < num; i++) {
      uint8_t mask = 0x80;
      uint8_t iob = frame[i];
      while (mask) {
          crc <<= 1;
          if (iob & mask) {
              crc |= 1;
          }
          mask >>= 1;
          if (crc & SIPC_10000) {
              crc &= SIPC_FFFF;
              crc ^= SIPC_1021;
          }
      }
  }
  return crc;
}

uint8_t SML_PzemCrc(uint8_t *data, uint8_t len) {
SETREGS

  uint16_t crc = 0;
  for (uint32_t i = 0; i < len; i++) crc += *data++;
  return (uint8_t)(crc & 0xFF);
}

// for odd parity init with 1
uint8_t CalcEvenParity(uint8_t data) {
SETREGS

uint8_t parity=0;

  while(data) {
    parity^=(data &1);
    data>>=1;
  }
  return parity;
}

void SML_Restart(void) {
SETREGS
  ResponseTime_P(PSTR(",\"SML\":{\"CMD\":\"restart\"}}"));
  SML_CounterSaveState();
  SML_Init();
  ResponseCmndDone();
}

void SML_dump(void) {
SETREGS
  uint8_t index = XdrvMailbox->payload;

  if (sml_globs.ready) {
    if ((index & 7) > sml_globs.meters_used) index = 1;
    if (index > 0 && sml_globs.mptr[(index & 7) - 1].type == 'c') {
      index = 0;
    }
		if (sml_globs.log_data) {
			free(sml_globs.log_data);
			sml_globs.log_data = 0;
		}
		if (index > 0) {
			sml_globs.log_data = (char*)calloc(sml_globs.logsize, sizeof(char));
		}
    sml_globs.dump2log = index;
    ResponseTime_P(PSTR(",\"SML\":{\"CMD\":\"dump: %d\"}}"), sml_globs.dump2log);
	}
  ResponseCmndNumber(index);
}


void SML_counter(void) {
  SETREGS
  STGLOB
  GETDCONSTP
  // set counter 1 - 4
  if ((XdrvMailbox->index > 0) && (XdrvMailbox->index <= MAX_COUNTERS)) {

    if (XdrvMailbox->data_len > 0) {
      uint8_t index = XdrvMailbox->index;
      uint32_t cval = XdrvMailbox->payload;

      RtcSettings->pulse_counter[index - 1] = cval;
  
      uint8_t cindex = 0;
      for (uint8_t meters = 0; meters < sml_globs.meters_used; meters++) {
        if (sml_globs.mptr[meters].type == 'c') {
          InjektCounterValue(meters, RtcSettings->pulse_counter[cindex], SFPC_0);
          cindex++;
        }
      }
      ResponseTime_P(PSTR(",\"SML\":{\"CMD\":\"counter%d: %d\"}}"), index, RtcSettings->pulse_counter[index - 1]);
    }
    ResponseCmndNumber(RtcSettings->pulse_counter[XdrvMailbox->index -1]);
  }
}

void SML_led(void) {
  SETREGS
  STGLOB
  // serial activity LED-GPIO
  if (XdrvMailbox->data_len > 0) {
    sml_globs.ser_act_LED_pin = XdrvMailbox->payload;
    if (Gpio_used(sml_globs.ser_act_LED_pin)) {
      AddLog(LOG_LEVEL_INFO, PSTR("SML: Error: Duplicate GPIO %d defined. Not usable for LED."), sml_globs.ser_act_LED_pin);
      sml_globs.ser_act_LED_pin = 255;
    }
    if (sml_globs.ser_act_LED_pin != 255) {
      pinMode(sml_globs.ser_act_LED_pin, OUTPUT);
    }      
  }
  ResponseTime_P(PSTR(",\"SML\":{\"CMD\":\"activity LED_pin: %d\"}}"), sml_globs.ser_act_LED_pin);
}

void SML_meter(void) {
  SETREGS
  STGLOB
  // meter number for serial activity
  if (XdrvMailbox->data_len > 0) {
    sml_globs.ser_act_meter_num = XdrvMailbox->payload;
  }
  ResponseTime_P(PSTR(",\"SML\":{\"CMD\":\"sml_globs.ser_act_meter_num: %d\"}}"), sml_globs.ser_act_meter_num);
}

#if 0
// dump to log shows serial data on console
// has to be off for normal use
// in console sensor53 d1, d2, d3 ... or d0 for normal use
// set counter => sensor53 c1 xxxx
// restart driver => sensor53 r
// meter number for monitoring serial activity => sensor53 m1, m2, m3 ... or m0 for all (default)
// LED-GPIO for monitoring serial activity => sensor53 l2, l13, l15 ... or l255 for turn off (default)
#endif

const char SML_Commands[] PROGMEM = "SML|"  // Prefix
  "restart|dump|counter|led|meter";

void (* const SML_Command[])(void) PROGMEM = {
  &SML_Restart, &SML_dump, &SML_counter , &SML_led, &SML_meter};


void InjektCounterValue(uint8_t meter, uint32_t counter, double rate) {
SETREGS

  STGLOB

  snprintf_P((char*)&meter_desc[meter].sbuff[0], meter_desc[meter].sbsiz, PSTR("1-0:1.8.0*255(%d)"), counter);
  SML_Decode(meter);

  GETDCONSTP

	char freq[16];
	freq[0] = 0;
	//if (rate != SFPC_0) {
  if (__nedf2(rate, SFPC_0)) {
		DOUBLE2CHAR(rate, 4, freq);
	}
  snprintf_P((char*)&meter_desc[meter].sbuff[0], meter_desc[meter].sbsiz, PSTR("1-0:1.7.0*255(%s)"), freq);
  SML_Decode(meter);
}

void SML_CounterSaveState(void) {
SETREGS
  STGLOB
  for (byte i = 0; i < MAX_COUNTERS; i++) {
      Settings->pulse_counter[i] = RtcSettings->pulse_counter[i];
  }
}


uint32_t SML_SetOptions(uint32_t in) {
  SETREGS
  if (in & 0x100) {
    sml_globs.sml_options = in;
  }
  return sml_globs.sml_options;
}

uint32_t SML_Getvars(uint16_t function) {
  SETREGS
  switch (function & 3) {
    case 0:
      // mark plugin present
      return 1;
    case 1:
      // must return adjusted jumptable here
      return (uint32_t)&sml_globs.smltab;
  }
  return 0;
}

void SML_Deinit(void) {
SETREGS
  sml_free_vars();
  RETMEM
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

int32_t mod_func_execute(uint32_t function) {
  SETREGS
  bool result = false;
    if ((function & 0x80000000) != 0) {
      if (((function >> 16) & 0x7ff) == XSNS_53) {
        return SML_Getvars(function);
      } else {
        return 0;
      }
    }
    switch (function) {
      case pFUNC_INIT:
        result = SML_Init_0();
        break;
      case pFUNC_LOOP:
        if (bitRead(Settings->rule_enabled, 0)) {
          if (sml_globs.ready) {
            SML_Counter_Poll();
            if (sml_globs.dump2log) {
              dump2log();
            } else {
              SML_Poll();
#ifdef USE_SML_CANBUS
              SML_CANBUS_Read();
#endif// USE_SML_CANBUS
            }
          }
        }
        break;
      case pFUNC_EVERY_100_MSECOND:
        if (bitRead(Settings->rule_enabled, 0)) {
          if (sml_globs.ready) {
            SML_Check_Send();
          }
        }
        break;
			case pFUNC_EVERY_SECOND:
				if (bitRead(Settings->rule_enabled, 0)) {
					if (sml_globs.ready) {
						SML_Counter_Poll_1s();
#ifdef USE_SML_TCP
            sml_tcp_check();
#endif
					}
				}
        break;
      case pFUNC_JSON_APPEND:
        if (sml_globs.ready) {
          if (sml_globs.sml_options & SML_OPTIONS_JSON_ENABLE) {
            SML_Show(1);
          }
        }
        break;
#ifdef USE_WEBSERVER
      case pFUNC_WEB_SENSOR:
        if (sml_globs.ready) {
          SML_Show(0);
        }
        break;
#endif  // USE_WEBSERVER

      case pFUNC_COMMAND:
        result = DecodeCommand(SML_Commands, SML_Command);
        break;

      case pFUNC_SAVE_BEFORE_RESTART:
      case pFUNC_SAVE_AT_MIDNIGHT:
        if (sml_globs.ready) {
          SML_CounterSaveState();
        }
        break;
			case pFUNC_DEINIT:
				SML_Deinit();
				break;

    }
  return result;
}


PULL_OPTIONS
#endif  // USE_SML_M_MOD
