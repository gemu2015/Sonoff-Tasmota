
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

#ifndef SS_Serial_h
#define SS_Serial_h

#include <inttypes.h>

class SS_Serial {
  public:
    SS_Serial(uint8_t rec_pin);
    ~SS_Serial();
    uint8_t ss_peek();
    uint8_t ss_read();
    uint8_t ss_available();
    void irq_ss_read();
  private:
    uint8_t si_cnt;
    uint8_t si_inp;
    uint8_t si_out;
    uint8_t m_rx_pin;
    uint32_t m_bit_time;
    uint32_t ss_bstart;
    uint8_t ss_index;
    uint16_t ss_byte;
    uint8_t *si_buff;
};


#endif
