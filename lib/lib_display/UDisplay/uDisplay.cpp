/*
  uDisplay.cpp -  universal display driver support for Tasmota

  Copyright (C) 2021  Gerhard Mutz and  Theo Arends

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

#include <Arduino.h>
#include <Wire.h>
#include "uDisplay.h"


extern uint8_t *buffer;

uDisplay::uDisplay(char *lp) : Renderer(800, 600) {
  // analyse decriptor
  uint8_t section = 0;
  i2c_ncmds = 0;
  char linebuff[128];
  while (*lp) {

    uint16_t llen = strlen_ln(lp);
    strncpy(linebuff, lp, llen);
    linebuff[llen] = 0;
    lp += llen;
    char *lp1 = linebuff;

    if (*lp1 == '#') break;
    if (*lp1 == '\n') lp1++;
    while (*lp1 == ' ') lp1++;
    //Serial.printf(">> %s\n",lp1);
    if (*lp1 != ';') {
      // check ids:
      if (*lp1 == ':') {
        // id line
        lp1++;
        section = *lp1++;
      } else {
        switch (section) {
          case 'H':
            // header line
            // SD1306,128,64,1,I2C,5a,*,*,*
            str2c(&lp1, dname, sizeof(dname));
            char ibuff[16];
            str2c(&lp1, ibuff, sizeof(ibuff));
            setwidth(atoi(ibuff));
            str2c(&lp1, ibuff, sizeof(ibuff));
            setheight(atoi(ibuff));
            bpp = next_val(&lp1);
            str2c(&lp1, ibuff, sizeof(ibuff));
            if (!strncmp(ibuff, "I2C", 3)) {
              interface = _UDSP_I2C;
              i2caddr = next_hex(&lp1);
              i2c_scl = next_val(&lp1);
              i2c_sda = next_val(&lp1);
              reset = next_val(&lp1);
              section = 0;
            } else if (!strncmp(ibuff, "SPI", 3)) {
              interface = _UDSP_SPI;
              section = 0;
            }
            break;
          case 'S':
            splash_font = next_val(&lp1);
            splash_size = next_val(&lp1);
            fg_col = next_val(&lp1);
            bg_col = next_val(&lp1);
            splash_xp = next_val(&lp1);
            splash_yp = next_val(&lp1);
            break;
          case 'I':
            // init data
            i2c_cmds[i2c_ncmds++] = next_hex(&lp1);
            if (!str2c(&lp1, ibuff, sizeof(ibuff))) {
              i2c_cmds[i2c_ncmds++] = strtol(ibuff, 0, 16);
            }
            break;
          case '0':
            str2c(&lp1, ibuff, sizeof(ibuff));
            i2c_off = strtol(ibuff, 0, 16);
            break;
          case '1':
            str2c(&lp1, ibuff, sizeof(ibuff));
            i2c_on = strtol(ibuff, 0, 16);
            break;
        }
      }
    }
    if (*lp == '\n') {
      lp++;
    } else {
      lp = strchr(lp, '\n');
      if (!lp) break;
      lp++;
    }
  }
}


Renderer *uDisplay::Init(void) {

  if (reset >= 0) {
    pinMode(reset, OUTPUT);
    digitalWrite(reset, HIGH);
    digitalWrite(reset, LOW);
    delay(5);
    digitalWrite(reset, HIGH);
  }

  if (interface == _UDSP_I2C) {
    Wire.begin(i2c_sda, i2c_scl);
    if (bpp < 16) {
      if (buffer) free(buffer);
      buffer = (uint8_t*)calloc((width()*height()*bpp)/8, 1);

      for (uint32_t cnt = 0; cnt < i2c_ncmds; cnt++) {
        i2c_command(i2c_cmds[cnt]);
      }
    }
  }
  if (interface == _UDSP_SPI) {

  }
  return this;
}

void uDisplay::DisplayInit(int8_t p,int8_t size,int8_t rot,int8_t font) {
    setRotation(rot);
    invertDisplay(false);
    setTextWrap(false);
    cp437(true);
    setTextFont(font);
    setTextSize(size);
    setTextColor(fg_col, bg_col);
    setCursor(0,0);
    fillScreen(bg_col);
    Updateframe();
}

void uDisplay::i2c_command(uint8_t val) {
  //Serial.printf("%02x\n",val );
  Wire.beginTransmission(i2caddr);
  Wire.write(0);
  Wire.write(val);
  Wire.endTransmission();
}

#define SH1106_SETLOWCOLUMN 0
#define SH1106_SETHIGHCOLUMN 0x10
#define SH1106_SETSTARTLINE 0x40


void uDisplay::Updateframe(void) {

  if (interface == _UDSP_I2C) {
    i2c_command(SH1106_SETLOWCOLUMN | 0x0);  // low col = 0
    i2c_command(SH1106_SETHIGHCOLUMN | 0x0);  // hi col = 0
    i2c_command(SH1106_SETSTARTLINE | 0x0); // line #0

	  uint8_t ys = height() >> 3;
	  uint8_t xs = width() >> 3;
    //uint8_t xs = 132 >> 3;
	  uint8_t m_row = 0;
	  uint8_t m_col = 2;

	  uint16_t p = 0;

	  uint8_t i, j, k = 0;

	  for ( i = 0; i < ys; i++) {
		    // send a bunch of data in one xmission
        i2c_command(0xB0 + i + m_row);//set page address
        i2c_command(m_col & 0xf);//set lower column address
        i2c_command(0x10 | (m_col >> 4));//set higher column address

        for( j = 0; j < 8; j++){
			      Wire.beginTransmission(i2caddr);
            Wire.write(0x40);
            for ( k = 0; k < xs; k++, p++) {
		            Wire.write(buffer[p]);
            }
            Wire.endTransmission();
        	}
	      }
    }
}


void uDisplay::Splash(void) {
  setTextFont(splash_font);
  setTextSize(splash_size);
  DrawStringAt(splash_xp, splash_yp, dname, UDISP_WHITE, 0);
  Updateframe();
}


void uDisplay::DisplayOnff(int8_t on) {
  if (on) {
    i2c_command(i2c_on);
  } else {
    i2c_command(i2c_off);
  }
}

uint8_t uDisplay::strlen_ln(char *str) {
  for (uint32_t cnt = 0; cnt < 256; cnt++) {
    if (!str[cnt] || str[cnt] == '\n') return cnt;
  }
  return 0;
}

char *uDisplay::devname(void) {
  return dname;
}

uint32_t uDisplay::str2c(char **sp, char *vp, uint32_t len) {
    char *lp = *sp;
    if (len) len--;
    char *cp = strchr(lp, ',');
    if (cp) {
        while (1) {
            if (*lp == ',') {
                *vp = 0;
                *sp = lp + 1;
                return 0;
            }
            if (len) {
                *vp++ = *lp++;
                len--;
            } else {
                lp++;
            }
        }
    } else {
      uint8_t slen = strlen(lp);
      if (slen) {
        strlcpy(vp, *sp, len);
        *sp = lp + slen;
        return 0;
      }
    }
    return 1;
}

int32_t uDisplay::next_val(char **sp) {
  char ibuff[16];
  str2c(sp, ibuff, sizeof(ibuff));
  return atoi(ibuff);
}

uint32_t uDisplay::next_hex(char **sp) {
  char ibuff[16];
  str2c(sp, ibuff, sizeof(ibuff));
  return strtol(ibuff, 0, 16);
}
