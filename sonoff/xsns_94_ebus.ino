/*
 *  ebus.c
*  2400 baud ebus decoder für wolf

 *  Created by Gerhard Mutz on 07.10.11.
 		adapted for Tasmota

 *  This program is free software: you can redistribute it and/or modify
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

#ifdef USE_EBUS

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define OFF       0
#define ON        1
#define SYNC		0xaa
struct CSVDATA {
		signed char Aussensensor;
		unsigned char Warmwasser;
		unsigned char Heizkessel;
		unsigned char Kollektor;
		unsigned char Raumtemperatur;
		unsigned char Solarspeicher;
		unsigned char Ruecklauf;
		char Solarpumpe;
		char Brenner;
		char brenner; // ?
		char wasser; // ?
		char pumpe;
		char Status;
};


static unsigned char ebus_buffer[100];    /* buffer for the received message */
static short ebus_pos;	/* index counter for the receive buffer */

static struct CSVDATA alldata;

void eBus_analyze(void) {

	switch(ebus_buffer[2]) {    /* switch over the message type */

		case 0x03:          /* Servicedatenbefehle */
			if ((ebus_buffer[3] == 0x04) && (ebus_buffer[4] == 0x03)) {  /* Antwort auf Zählerstandabfrage */
				/*
				lcd_gotoxy(17,1);
				lcd_puts(itoa(ebus_buffer[8],buffer,10));
				if (ebus_buffer[7] <= 0x09) lcd_putc('0');
				lcd_puts(itoa(ebus_buffer[7],buffer,10));
				if (ebus_buffer[6] <= 0x09) lcd_putc('0');
				lcd_puts(itoa(ebus_buffer[6],buffer,10));*/
			}
			if ((ebus_buffer[3] == 0x04) && (ebus_buffer[4] == 0x04)) {  /* Antwort auf Betriebsstunden */
				/*lcd_gotoxy(7,0);
				lcd_puts(itoa(ebus_buffer[9],buffer,10));
				if (ebus_buffer[8] <= 0x09) lcd_putc('0');
				lcd_puts(itoa(ebus_buffer[8],buffer,10));
				if (ebus_buffer[7] <= 0x09) lcd_putc('0');
				lcd_puts(itoa(ebus_buffer[7],buffer,10));
				lcd_putc(':');
				if (ebus_buffer[6] <= 0x09) lcd_putc('0');
				lcd_puts(itoa(ebus_buffer[6],buffer,10));*/
			}
			break;

		case 0x05:          /* Brennersteuerbefehle */
#if 0
			if (ebus_buffer[3] == 0x01) {   /* Betriebsdaten des Reglers an Feuerungsautomaten */
				//sprintf(Aussensensor, "%d",ebus_buffer[8]);
				if ((unsigned char)ebus_buffer[5] == 0xAA) {
					alldata.brenner = ON;
				} else {
					alldata.brenner = OFF;
				}

				if ((unsigned char)ebus_buffer[5] == 0x55) {
					alldata.wasser = ON;
				} else {
					alldata.wasser = OFF;
				}
				break;
			}
#else
    if (ebus_buffer[3] == 0x07) {   // Betriebsdaten des Reglers an Feuerungsautomaten
				//sprintf(Aussensensor, "%d",ebus_buffer[8]);
				if ((ebus_buffer[5] == 0xAA) || (ebus_buffer[5] == 0xbb)) {
					alldata.brenner = ON;
				} else {
					alldata.brenner = OFF;
				}

				if ((ebus_buffer[5] == 0x55) || (ebus_buffer[5] == 0xbb)) {
					// wasserbeitung
					alldata.wasser = ON;
				} else {
					// heizung
					alldata.wasser = OFF;
				}
				break;
			}
#endif
			if (ebus_buffer[3] == 0x03) {   /* Betriebsdaten des Feuerungsautomaten an den Regler */
				alldata.Aussensensor=ebus_buffer[12];
				alldata.Warmwasser=ebus_buffer[11];
				alldata.Heizkessel=ebus_buffer[9]/2;
				if (ebus_buffer[0]==0x03 && ebus_buffer[1]==0xfe) {
					alldata.Ruecklauf=ebus_buffer[10];
					alldata.Brenner=(ebus_buffer[7]>>3)&1;
          alldata.pumpe=(ebus_buffer[7]>>6)&1;
          alldata.Status=ebus_buffer[6];
				}
				break;
			}

			break;

		case 0x07:          /* Systemdatenbefehle */
			if (ebus_buffer[3] == 0x00) {  /* Datum und Zeit Meldung */
				alldata.Aussensensor=eBus_data2b(ebus_buffer[7],ebus_buffer[6]);
				break;
				//buffer[5] = '\0';

				/* der Regler übermittelt nur eine Temperatur wenn der Aussensensor am BM angeschlossen ist */
				//        lcd_gotoxy(0,1);
				//        lcd_puts("Aussen: ");
				//        lcd_puts(buffer);

				//lcd_gotoxy(0,0);
				//lcd_puts(itoa(eBus_bcd(ebus_buffer[10]),buffer,10));
				//lcd_putc(':');

				//if (ebus_buffer[9] <= 0x09) lcd_putc('0');
				//lcd_puts(itoa(eBus_bcd(ebus_buffer[9]),buffer,10));
				//lcd_putc(' ');

				//        itoa(eBus_bcd(ebus_buffer[8]),buffer,10);
				//        buffer[2] = ' ';
				//        buffer[3] = ' ';
				//        buffer[4] = '\0';
				//        lcd_puts(buffer);

			}
			break;

		case 0x50:          /* Kromschröder */

			// temperaturen
			if (ebus_buffer[3] == 0x17) {
				alldata.Kollektor=(ebus_buffer[7]|((unsigned short)ebus_buffer[8]<<8))/16;
				alldata.Solarspeicher=(ebus_buffer[9]|((unsigned short)ebus_buffer[10]<<8))/16;
				alldata.Solarpumpe=ebus_buffer[5]&1;
				break;
			}
			// erträge
			if (ebus_buffer[3] == 0x18) {
				//sprintf(Kollektortemperatur, "%d",ebus_buffer[9]/2);
				break;
			}
			// ?????
			if (ebus_buffer[3] == 0x23) {
				//sprintf(Kollektortemperatur, "%d",ebus_buffer[9]/2);
				break;
			}
			// reglerdaten ???
			if (ebus_buffer[3] == 0x14) {
				alldata.Raumtemperatur=ebus_buffer[9];
				break;
			}
			break;
			/*
			SM -> alle
			71 FE 50 17 10 00 01 FA 01 FB 02 00 80 00 80 00 80 00 80 00 80 D9
			05  01 = Solarpumpe läuft ?

			07,08 Kollektortemperatur * 16 (LLHH)
			09,10 WW Solar Temperatur * 16 (LLHH)

			SM -> alle
			71 FE 50 18 0E 00 00 1B 00 0A 00 B0 01 4F 01 00 00 00 00 3F
			05,06 Ertrag Tag?
			^ Summe Ertrag?



		case 0x09:          // MemoryServer Befehle
			break;

		case 0x0f:          // Testnachricht
			break;

		case 0xfe:          // Fehlernachricht
			break;*/

		case 0x08:          // Regler - Regelbefehle Sollwerte
			break;

		default:
			//sprintf(ebstatus, "unknown");
			break;
	}

}

#define ESC 0xa9

void ebus_esc(unsigned char len) {
    short count,count1;
    for (count=0; count<len; count++) {
        if (ebus_buffer[count]==ESC) {
            //found escape
            ebus_buffer[count]+=ebus_buffer[count+1];
            // remove 2. char
            count++;
            for (count1=count; count1<len; count1++) {
                ebus_buffer[count1]=ebus_buffer[count1+1];
            }
        }
    }

}

void EBUS_Poll(void) {
	uint8_t iob;

	while (Serial.available()) {

		iob=(uint8_t)Serial.read();

		if (iob==SYNC) {
    	// should be end of telegramm
      // QQ,ZZ,PB,SB,NN ..... CRC, ACK SYNC
      if (ebus_pos) {
      	// get telegramm lenght
        uint8_t tlen=ebus_buffer[4]+5;
        if (ebus_pos<(tlen+1)) {
            // ????
						ebus_pos=0;
						continue;
        }
        iob=CalculateCRC(ebus_buffer,tlen);
        // test crc
        if (ebus_buffer[tlen]==iob) {
            ebus_esc(tlen);
            eBus_analyze();
						ebus_set_timeout();
        } else {
            // crc error
        }
      }
      ebus_pos=0;
      continue;
    }
		ebus_buffer[ebus_pos] = iob;
		ebus_pos++;
		if (ebus_pos>=sizeof(ebus_buffer)) {
			ebus_pos=0;
		}
	}
}

int eBus_data2b(uint8_t x, uint8_t y) {
	int temp;
	if ( x >= 0x80) {
		temp = -128;
	} else {
		temp = 0;
	}
	temp = temp + (x & 0x7f);
	return temp;
}

uint8_t crc8(uint8_t data, uint8_t crc_init) {
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

uint8_t CalculateCRC( uint8_t *Data, uint16_t DataLen ) {
	uint16_t i;
	uint8_t Crc = 0;
	for(i = 0 ; i < DataLen ; ++i, ++Data ) {
      Crc = crc8( *Data, Crc );
   }
   return Crc;
}

/*
int eBus_s_char(char x) {
	int temp;
	if ( x & 0x40) {
		temp = -64;
	} else {
		temp = 0;
	}
	temp = temp + (x & 0x3f);
	return temp;
}

int eBus_bcd(char x) {
	int temp;
	temp = x;
	temp = (((temp & 0xf0) >> 4) * 10) + (x & 0x0f);
	return temp;
}
*/




const char JSON_EBUS[] PROGMEM = ",\"%s\":{\"" "Outsidetemp" "\":%d,\"" "Roomtemp" "\":%d,\"" "Boiler" "\":%d,\"" "Returns" "\":%d,\"" "Warmwater" "\":%d,\"" "Burner" "\":%d,\"" "Status" "\":%d,\"" "Solarstorage" "\":%d,\"" "Collector" "\":%d,\"" "Solarpump" "\":%d }";

const char WEB_EBUS[] PROGMEM =
  "{s}EBUS " "Aussensensor: " "{m}%d " "C" "{e}"
  "{s}EBUS " "Raumtemperatur: " "{m}%d " "C" "{e}"
  "{s}EBUS " "Heizkessel: " "{m}%d " "C" "{e}"
	"{s}EBUS " "Rücklauf: " "{m}%d " "C" "{e}"
	"{s}EBUS " "Warmwasser: " "{m}%d " "C" "{e}"
	"{s}EBUS " "Brenner: " "{m}%d " "" "{e}"
	"{s}EBUS " "Status: " "{m}%d " "" "{e}"
	"{s}EBUS " "Solarspeicher: " "{m}%d " "C" "{e}"
	"{s}EBUS " "Kollektor: " "{m}%d " "C" "{e}"
	"{s}EBUS " "Solarpumpe: " "{m}%d " "" "{e}";

void EBUS_Show(boolean json) {
	char b_mqtt_data[128];
	b_mqtt_data[0]=0;
	if (json) {
      ResponseAppend_P(JSON_EBUS, "EBUS",alldata.Aussensensor,alldata.Raumtemperatur,alldata.Heizkessel,alldata.Ruecklauf,alldata.Warmwasser,alldata.Brenner,alldata.Status,alldata.Solarspeicher,alldata.Kollektor,alldata.Solarpumpe);
	} else {
#ifdef USE_WEBSERVER
      WSContentSend_PD(WEB_EBUS,alldata.Aussensensor,alldata.Raumtemperatur,alldata.Heizkessel,alldata.Ruecklauf,alldata.Warmwasser,alldata.Brenner,alldata.Status,alldata.Solarspeicher,alldata.Kollektor,alldata.Solarpumpe);
#endif
  }
}

unsigned char ebus_time_cnt;
// LEDPIN if you want to save power, do not use ledpin on devices that have a relais connected to the same pin
// or disconnect the relais in hardware
// sonoff basic has pin 13 to LED only (pin 12 is led +relais)
// type -1 instead to disable LED
#define EBUS_LEDPIN -1
#define SML_TIMEOUT 10
static void ebus_set_timeout() {
  ebus_time_cnt=SML_TIMEOUT;
  // led on
#if EBUS_LEDPIN>0
  digitalWrite(EBUS_LEDPIN,HIGH);
#endif
}

void EBUS_Init(void) {
	ebus_pos=0;
	SetSerialBaudrate(2400);
	/// led to OUTPUT
#if EBUS_LEDPIN>0
  pinMode(EBUS_LEDPIN,OUTPUT);
	digitalWrite(EBUS_LEDPIN,LOW);
#endif
	ebus_time_cnt=SML_TIMEOUT;
	// request serial line
  ClaimSerial();
}
/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XSNS_94

bool Xsns94(byte function) {
  bool result = false;

  //if (pin[GPIO_EBUS] < 99) {
    switch (function) {
      case FUNC_INIT:
        EBUS_Init();
        break;
      case FUNC_EVERY_SECOND:
				if (ebus_time_cnt) {
					ebus_time_cnt-=1;
					if (ebus_time_cnt==0) {
						// timed out
						// led off
						digitalWrite(EBUS_LEDPIN,LOW);
					}
				}
        EBUS_Poll();
        break;
      case FUNC_JSON_APPEND:
        EBUS_Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        EBUS_Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  //}
  return result;
}
#endif
