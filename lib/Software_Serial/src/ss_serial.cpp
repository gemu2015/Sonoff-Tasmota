
/*
  ss_serial.cpp - software serial receive with no wait

  Copyright (C) 2019 Gerhard Mutz

  This library is free software: you can redistribute it and/or modify
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
#include <Arduino.h>

extern "C" {
#include "gpio.h"
}

#define SS_BAUDRATE 9600
// must be power of 2
#define SS_SERIAL_BUFFER_SIZE 64

#include "ss_serial.h"

SS_Serial *ss_obj_list[16];


#include <core_version.h>                   // Arduino_Esp8266 version information (ARDUINO_ESP8266_RELEASE and ARDUINO_ESP8266_RELEASE_2_3_0)
#ifndef ARDUINO_ESP8266_RELEASE_2_3_0
  #define SS_SERIAL_USE_IRAM                // Enable to use iram (+368 bytes)
#endif

#ifdef SS_SERIAL_USE_IRAM
void ICACHE_RAM_ATTR xtms_isr_0() { ss_obj_list[0]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_1() { ss_obj_list[1]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_2() { ss_obj_list[2]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_3() { ss_obj_list[3]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_4() { ss_obj_list[4]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_5() { ss_obj_list[5]->irq_ss_read(); };
// Pin 6 to 11 can not be used
void ICACHE_RAM_ATTR xtms_isr_12() { ss_obj_list[12]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_13() { ss_obj_list[13]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_14() { ss_obj_list[14]->irq_ss_read(); };
void ICACHE_RAM_ATTR xtms_isr_15() { ss_obj_list[15]->irq_ss_read(); };
#else
void xtms_isr_0() { ss_obj_list[0]->irq_ss_read(); };
void xtms_isr_1() { ss_obj_list[1]->irq_ss_read(); };
void xtms_isr_2() { ss_obj_list[2]->irq_ss_read(); };
void xtms_isr_3() { ss_obj_list[3]->irq_ss_read(); };
void xtms_isr_4() { ss_obj_list[4]->irq_ss_read(); };
void xtms_isr_5() { ss_obj_list[5]->irq_ss_read(); };
// Pin 6 to 11 can not be used
void xtms_isr_12() { ss_obj_list[12]->irq_ss_read(); };
void xtms_isr_13() { ss_obj_list[13]->irq_ss_read(); };
void xtms_isr_14() { ss_obj_list[14]->irq_ss_read(); };
void xtms_isr_15() { ss_obj_list[15]->irq_ss_read(); };
#endif  // SS_SERIAL_USE_IRAM


static void (*xISRList[16])() = {
      xtms_isr_0,
      xtms_isr_1,
      xtms_isr_2,
      xtms_isr_3,
      xtms_isr_4,
      xtms_isr_5,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      xtms_isr_12,
      xtms_isr_13,
      xtms_isr_14,
      xtms_isr_15
};

SS_Serial::SS_Serial(uint8_t rec_pin) {
  m_rx_pin = rec_pin;
  si_buff = (uint8_t*)malloc(SS_SERIAL_BUFFER_SIZE);
  if (si_buff == NULL) return;
  m_bit_time = ESP.getCpuFreqMHz() *1000000 /SS_BAUDRATE;
  pinMode(m_rx_pin, INPUT);
  ss_obj_list[m_rx_pin] = this;
  attachInterrupt(m_rx_pin, xISRList[m_rx_pin], CHANGE);
  ss_index=0;
  si_out = si_cnt = 0;
  ss_byte=0;
}

uint8_t SS_Serial::ss_read(void) {
  uint8_t iob;

  while (!si_cnt) {
 		delay(0);
 	}
  iob=si_buff[si_out];
 	si_out++;
 	si_out&=SS_SERIAL_BUFFER_SIZE-1;
	// disable irqs
 	noInterrupts();
 	si_cnt--;
	// enable irqs
 	interrupts();

 	return iob;
}

uint8_t SS_Serial::ss_peek(void) {
  while (!si_cnt) {
 		delay(0);
 	}
 	return si_buff[si_out];
}

uint8_t SS_Serial::ss_available(void) {
  return si_cnt;
}

SS_Serial::~SS_Serial() {
  detachInterrupt(m_rx_pin);
  ss_obj_list[m_rx_pin] = NULL;
  if (si_buff) free(si_buff);
}

#define LASTBIT 9
// pin irq ESP.getCycleCount about 28 seconds max
#ifdef TM_SERIAL_USE_IRAM
void ICACHE_RAM_ATTR SS_Serial::irq_ss_read(void) {
#else
void SS_Serial::irq_ss_read(void) {
#endif

  uint8_t diff;
  uint8_t level;

  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << m_rx_pin);

  level=digitalRead(m_rx_pin);

  if (!level && !ss_index) {
    // start condition
    ss_bstart=ESP.getCycleCount()-(m_bit_time/4);
    ss_byte=0;
    ss_index++;
    //digitalWrite(1, LOW);
  } else {
    // now any bit changes go here
    // calc bit number
    diff=(ESP.getCycleCount()-ss_bstart)/m_bit_time;
    //digitalWrite(1, level);

    if (!level && diff>LASTBIT) {
      // start bit of next byte, store  and restart
      // leave irq at change
      for (uint8_t i=ss_index;i<=LASTBIT;i++) {
        ss_byte|=(1<<i);
      }
      //stobyte(0,ssp->ss_byte>>1);
      if (si_cnt<SS_SERIAL_BUFFER_SIZE-1) {
        si_buff[si_inp]=ss_byte>>1;
        si_inp++;
        si_inp&=SS_SERIAL_BUFFER_SIZE-1;
        si_cnt++;
      }

      ss_bstart=ESP.getCycleCount()-(m_bit_time/4);
      ss_byte=0;
      ss_index=1;
      return;
    }
    if (diff>=LASTBIT) {
      // bit zero was 0,
      //stobyte(0,ssp->ss_byte>>1);
      if (si_cnt<SS_SERIAL_BUFFER_SIZE-1) {
        si_buff[si_inp]=ss_byte>>1;
        si_inp++;
        si_inp&=SS_SERIAL_BUFFER_SIZE-1;
        si_cnt++;
      }
      ss_byte=0;
      ss_index=0;
    } else {
      // shift in
      for (uint8_t i=ss_index;i<diff;i++) {
        if (!level) ss_byte|=(1<<i);
      }
      ss_index=diff;
    }
  }
}
