/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
EXPERIMENTAL VERSION  unterstützt im Prinzip mehrere Zähler
da durch die begrenzte Hardwareunterstützung das software serial nicht optimal funktioniert
ist es mit der originalen TasmotaSerial nicht möglich 3 Zähler gleichzeitig abzufragen

durch Modifikation des Tasmota Serial Drivers sollten jetzt auch mehr als 2 Zähler
funktionieren. Dazu muss auch die TasmotaSerial-2.3.0 ebenfalls kopiert werden

nur dieser Treiber wird in Zukunft weiterentwickelt
die älteren werden nicht mehr unterstützt.

>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  xsns_95_sml.ino - SML smart meter interface for Sonoff-Tasmota

  Created by Gerhard Mutz on 07.10.11.
  adapted for Tasmota

  Copyright (C) 2019  Gerhard Mutz and Theo Arends

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

#ifdef USE_SML_M

#define XSNS_95 95

// Baudrate des D0 Ausgangs, sollte bei den meisten Zählern 9600 sein
#define SML_BAUDRATE 9600

// support für mehr als 2 Meter mit spezieller Tasmota Serial Version
// dazu muss der neue Treiber => TasmotaSerial-2.3.0 ebenfalls kopiert werden

#include <TasmotaSerial.h>

// diese Version verwendet den serial REC Pin des ESP, und zusätzliche GPIO
// pins als Software serial
// und kann mit jeder aktuellen Version von Tasmota kombiniert werden
// dazu muss z.B. in der user_config_override #define USE_SML_M angegeben werden
// Als "Lesekopf" kann ein Fototransistor (z.B. TEKT5400 oder BPW78A) zwischen
// Masse und dem ESP REC pin verwendet werden. Eventuell ist ein zusätzlicher
// Pullup Widerstand zwischen REC und VCC 3.3 Volt erforderlich (1-4.7kOhm)

// max 23 Zeichen
#if DMY_LANGUAGE==de-DE
// deutsche Bezeichner
#define D_TPWRIN "Verbrauch"
#define D_TPWROUT "Einspeisung"
#define D_TPWRCURR "Aktueller Verbrauch"
#define D_TPWRCURR1 "Verbrauch P1"
#define D_TPWRCURR2 "Verbrauch P2"
#define D_TPWRCURR3 "Verbrauch P3"
#define D_METERNR "Zähler Nr"

#else
// alle anderen Sprachen
#define D_TPWRIN "Total-In"
#define D_TPWROUT "Total-Out"
#define D_TPWRCURR "Current-In/Out"
#define D_TPWRCURR1 "Current-In p1"
#define D_TPWRCURR2 "Current-In p2"
#define D_TPWRCURR3 "Current-In p3"
#define D_METERNR "Meter_number"

#endif

// JASON Strings besser NICHT übersetzen
// max 23 Zeichen
#define DJ_TPWRIN "Total_in"
#define DJ_TPWROUT "Total_out"
#define DJ_TPWRCURR "Power_curr"
#define DJ_TPWRCURR1 "Power_p1"
#define DJ_TPWRCURR2 "Power_p2"
#define DJ_TPWRCURR3 "Power_p3"
#define DJ_METERNR "Meter_number"

struct METER_DESC {
  uint8_t srcpin;
  uint8_t type;
  char prefix[6];
};

// Zählerliste , hier neue Zähler eintragen
//=====================================================
#define EHZ161_0 1
#define EHZ161_1 2
#define EHZ363 3
#define EHZH 4
#define EDL300 5
#define Q3B 6
#define COMBO3 7
#define COMBO2 8
#define COMBO3a 9

// diesen Zähler auswählen
#define METER COMBO2

//=====================================================
// Einträge in Liste
// erster Eintrag = laufende Zählernummer mit Komma getrennt
// danach bis @ Zeichen => Sequenz von OBIS als ASCI, oder SML als HEX ASCI
// Skalierungsfaktor (Divisor) (kann auch negativ sein oder kleiner 0 z.B. 0.1 => Ergebnis * 10)
// statt des Skalierungsfaktors kann hier (nur in einer Zeile) ein # Zeichen stehen (OBIS, (SML Hager))
// in diesem Fall wird ein String (keine Zahl) ausgelesen (z.B. Zähler ID)
// nach dem # Zeichen muss ein Abschlusszeichen angegeben werden, also bei OBIS ein ) Zeichen
// Name des Signals in WEBUI (max 23 Zeichen)
// Einheit des Signals in WEBUI (max 7 Zeichen)
// Name des Signals in MQTT Nachricht (max 23 Zeichen)
// Anzahl der Nachkommastellen, wird hier 16 addiert wird sofort ein MQTT für diesen Wert ausgelöst, nicht erst bei teleperiod
// Beispiel: => "1-0:2.8.0*255(@1,Einspeisung,KWh,Solar_Feed,4|"
// in allen ausser der letzten Zeile muss ein | Zeichen am Ende der Zeile stehen.
// Nur am Ende der letzen Zeile steht ein Semikolon.
// max 16 Zeilen
// =====================================================
// steht in der Sequenz ein = Zeichen am Anfang kann folgender Eintrag definiert werden:
// =m => mathe berechne Werte z.B. =m 3+4+5  addiert die Ergebnisse aus den Zeilen 3,4 und 5
// + - / * werden unterstützt
// damit kann z.B. die Summe aus 3 Phasen berechnet werden
// =d => differenz berechne Differenzwerte über die Zeit aus dem Ergebnis der Zeile
// z.B. =d 3 10 berechnet die Differenz nach jeweils 10 Sekunden des Ergebnisses aus Zeile 3
// damit kann z.B. der Momentanverbrauch aus dem Gesamtverbrauch berechnet werden, falls der Zähler das nicht direkt ausgibt
// =h => html Text Zeile (max 30 Zeichen) in WEBUI einfügen, diese Zeile zählt nicht bei Zeilenreferenzen

// der METER_DESC beschreibt die Zähler
// METERS_USED muss auf die Anzahl der benutzten Zähler gesetzt werden
// entsprechend viele Einträge muss der METER_DESC dann haben (für jeden Zähler einen)
// 1. srcpin der pin für den seriellen input 0 oder 3 => RX pin, ansonsten software serial GPIO pin
// 2. type o=obis, s=sml
// 3. jason prefix max 5 Zeichen, kann im Prinzip frei gesetzt werden
// dieses Prefix wird sowohl in der Web Anzeige als auch in der MQTT Nachricht vorangestellt

#if METER==EHZ161_0
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'o',"OBIS"}};
const uint8_t meter[]=
"1,1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.0*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,1-0:21.7.0*255(@1," D_TPWRCURR1 ",W," DJ_TPWRCURR1 ",0|"
"1,1-0:41.7.0*255(@1," D_TPWRCURR2 ",W," DJ_TPWRCURR2 ",0|"
"1,1-0:61.7.0*255(@1," D_TPWRCURR3 ",W," DJ_TPWRCURR3 ",0|"
"1,=m 3+4+5 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

//=====================================================

#if METER==EHZ161_1
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'o',"OBIS"}};
const uint8_t meter[]=
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";
#endif

//=====================================================

#if METER==EHZ363
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x00,0xff
"1,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x10,0x07,0x00,0xff
"1,77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
//0x77,0x07,0x01,0x00,0x00,0x00,0x09,0xff
"1,77070100000009ff@#," D_METERNR ",," DJ_METERNR ",0";
#endif

//=====================================================

#if METER==EHZH
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
// verbrauch total
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x0f,0x07,0x00,0xff
"1,770701000f0700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==EDL300
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',"SML"}};
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
// verbrauch total
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"1,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100020801ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x0f,0x07,0x00,0xff
"1,770701000f0700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==Q3B
#define METERS_USED 1
struct METER_DESC const meter_desc[METERS_USED]={
  [0]={0,'s',"SML"}};
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"1,77070100010800ff@100," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x01,0xff
"1,77070100020801ff@100," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x01,0x07,0x00,0xff
"1,77070100010700ff@100," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

#if METER==COMBO3
// 3 Zähler Beispiel
#define METERS_USED 3

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',"OBIS"}, // harware serial RX pin
  [1]={14,'s',"SML"}, // GPIO14 software serial
  [2]={4,'o',"OBIS2"}}; // GPIO4 software serial

// 3 Zähler definiert
const uint8_t meter[]=
"1,1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.0*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,1-0:21.7.0*255(@1," D_TPWRCURR1 ",W," DJ_TPWRCURR1 ",0|"
"1,1-0:41.7.0*255(@1," D_TPWRCURR2 ",W," DJ_TPWRCURR2 ",0|"
"1,1-0:61.7.0*255(@1," D_TPWRCURR3 ",W," DJ_TPWRCURR3 ",0|"
"1,=m 3+4+5 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"
"2,77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"2,77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"2,77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"3,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"3,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"3,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"3,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

#if METER==COMBO2
// 2 Zähler Beispiel
#define METERS_USED 2

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',"OBIS1"}, // harware serial RX pin
  [1]={14,'o',"OBIS2"}}; // GPIO14 software serial

// 2 Zähler definiert
const uint8_t meter[]=
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"

"2,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"2,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"2,=d 6 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"2,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

#if METER==COMBO3a
#define METERS_USED 3

struct METER_DESC const meter_desc[METERS_USED]={
  [0]={3,'o',"OBIS1"}, // harware serial RX pin
  [1]={14,'o',"OBIS2"},
  [2]={1,'o',"OBIS3"}};

// 3 Zähler definiert
const uint8_t meter[]=
"1,=h --- Zähler Nr 1 ---|"
"1,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1,=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"
"2,=h --- Zähler Nr 2 ---|"
"2,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"2,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"2,=d 6 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"2,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0|"
"3,=h --- Zähler Nr 3 ---|"
"3,1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"3,1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"3,=d 10 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"3,1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

//=====================================================

// median filter elimiert Ausreisser, braucht aber viel RAM und Rechenzeit
// 672 bytes extra RAM bei MAX_VARS = 16
//#define USE_MEDIAN_FILTER

// maximale Anzahl der möglichen Variablen, gegebenfalls anpassen
// um möglichst viel RAM zu sparen sollte MAX_VARS der Anzahl der Zeilen
// in der Defintion entsprechen, insbesondere bei Verwendung des Medianfilters.
#define MAX_VARS 16
double meter_vars[MAX_VARS];
// deltas berechnen
#define MAX_DVARS METERS_USED*2
double dvalues[MAX_DVARS];
uint32_t dtimes[MAX_DVARS];

// software serial pointers
TasmotaSerial *meter_ss[METERS_USED];

// serial buffers
#define SML_BSIZ 32
uint8_t smltbuf[METERS_USED][SML_BSIZ];

// meter nr as string
#define METER_ID_SIZE 22
char meter_id[METERS_USED][METER_ID_SIZE];

#ifdef USE_MEDIAN_FILTER
// median filter, should be odd size
#define MEDIAN_SIZE 5
struct MEDIAN_FILTER {
double buffer[MEDIAN_SIZE];
int8_t index;
} sml_mf[MAX_VARS];

// calc median
double median(struct MEDIAN_FILTER* mf, double in) {
  double tbuff[MEDIAN_SIZE],tmp;
  uint8_t flag;
  mf->buffer[mf->index]=in;
  mf->index++;
  if (mf->index>=MEDIAN_SIZE) mf->index=0;
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
}
#endif


// dump to log zeigt serielle Daten zu Testzwecken in der Konsole an
// muss aber für Normalbetrieb aus sein
// dazu in Konsole sensor95 d1,d2,d3 .. bzw. d0 für Ein und Ausschalten der jeweiligen Zähler dumps angeben

char sml_start;
uint8_t dump2log=0;

#define SML_SAVAILABLE Serial_available()
#define SML_SREAD Serial_read()
#define SML_SPEAK Serial_peek()

bool Serial_available() {
  uint8_t num=dump2log&7;
  if (num<1 || num>METERS_USED) return Serial.available();
  if (num==1) {
      return Serial.available();
  } else {
    return meter_ss[num-1]->available();
  }
}

uint8_t Serial_read() {
  uint8_t num=dump2log&7;
  if (num<1 || num>METERS_USED) return Serial.read();
  if (num==1) {
      return Serial.read();
  } else {
    return meter_ss[num-1]->read();
  }
}

uint8_t Serial_peek() {
  uint8_t num=dump2log&7;
  if (num<1 || num>METERS_USED) return Serial.peek();
  if (num==1) {
      return Serial.peek();
  } else {
    return meter_ss[num-1]->peek();
  }
}

void Dump2log(void) {

int16_t index=0,hcnt=0;
uint32_t d_lastms;
uint8_t dchars[16];

  if (!SML_SAVAILABLE) return;

  if (dump2log&8) {
    // combo mode
    while (SML_SAVAILABLE) {
      log_data[index]=':';
      index++;
      log_data[index]=' ';
      index++;
      d_lastms=millis();
      while ((millis()-d_lastms)<40) {
        if (SML_SAVAILABLE) {
          uint8_t c=SML_SREAD;
          sprintf(&log_data[index],"%02x ",c);
          dchars[hcnt]=c;
          index+=3;
          hcnt++;
          if (hcnt>15) {
            // line complete, build asci chars
            log_data[index]='=';
            index++;
            log_data[index]='>';
            index++;
            log_data[index]=' ';
            index++;
            for (uint8_t ccnt=0; ccnt<16; ccnt++) {
              if (isprint(dchars[ccnt])) {
                log_data[index]=dchars[ccnt];
              } else {
                log_data[index]=' ';
              }
              index++;
            }
            break;
          }
        }
      }
      if (index>0) {
        log_data[index]=0;
        AddLog(LOG_LEVEL_INFO);
        index=0;
        hcnt=0;
      }
    }
  } else {
    //while (SML_SAVAILABLE) {
      index=0;
      log_data[index]=':';
      index++;
      log_data[index]=' ';
      index++;
      d_lastms=millis();
      while ((millis()-d_lastms)<40) {
        if (SML_SAVAILABLE) {
          if (meter_desc[(dump2log&7)-1].type=='o') {
            char c=SML_SREAD&0x7f;
            if (c=='\n' || c=='\r') break;
            log_data[index]=c;
            index++;
          } else {
            unsigned char c;
            if (sml_start==0x77) {
              sml_start=0;
            } else {
              c=SML_SPEAK;
              if (c==0x77) {
                sml_start=c;
                break;
              }
            }
            c=SML_SREAD;
            sprintf(&log_data[index],"%02x ",c);
            index+=3;
          }
        }
      }
      if (index>0) {
        log_data[index]=0;
        AddLog(LOG_LEVEL_INFO);
      }
    }
  //}

}


// get sml binary value
int64_t sml_getvalue(unsigned char *cp,uint8_t index) {
short len,unit,scaler,type;
int64_t value;

    // scan for value
    // check status
    len=*cp&0x0f;
    cp+=len;
    // check time
    len=*cp&0x0f;
    cp+=len;
    // check unit
    len=*cp&0x0f;
    unit=*(cp+1);
    cp+=len;
    // check scaler
    len=*cp&0x0f;
    scaler=(signed char)*(cp+1);
    cp+=len;
    // get value
    type=*cp&0x70;
    len=*cp&0x0f;
    type|=len-1;
    cp++;
    switch (type) {
            // int8
            value=(signed char)*cp;
            break;
        case 0x52:
            // int16;
            value=((int16_t)(*cp<<8))|*(cp+1);
            break;
        case 0x53:
            // int32; // len 3
            value=((int32_t)(*cp<<16))|((int32_t)*(cp+1)<<8)|(*(cp+3));
            break;
        case 0x54:
            // int32;
            value=((int32_t)(*cp<<24))|((int32_t)*(cp+1)<<16)|((int32_t)*(cp+2)<<8)|(*(cp+3));
            break;
        case 0x55:
            // int64; len 5
            cp++;
            value=((int64_t)(*cp<<32))|((int64_t)*(cp+1)<<24)|((int64_t)*(cp+2)<<16)|((int64_t)*(cp+3)<<8)|(*(cp+4));
            break;
        case 0x58:
            // int64;
            value=((int64_t)(*cp<<56))|((int64_t)*(cp+1)<<48)|((int64_t)*(cp+2)<<40)|((int64_t)*(cp+3)<<32)|((int64_t)*(cp+4)<<24)|((int64_t)*(cp+5)<<16)|((int64_t)*(cp+6)<<8)|(*(cp+7));
            break;

        case 0x61:
            // uint8;
            value=(unsigned char)*cp;
            break;
        case 0x62:
            // uint16;
            value=((uint16_t)*cp<<8)|(*(cp+1));
            break;
        case 0x63:
            // uint32; // len 3
            value=((uint16_t)*cp<<16)|((uint16_t)*(cp+1)<<8)|(*(cp+3));
        case 0x64:
            // uint32;
            value=((uint32_t)*cp<<24)|((uint32_t)*(cp+1)<<16)|((uint32_t)*(cp+2)<<8)|(*(cp+3));
            break;
        case 0x68:
            // uint64;
            value=((uint64_t)*cp<<56)|((uint64_t)*(cp+1)<<48)|((uint64_t)*(cp+2)<<40)|((uint64_t)*(cp+3)<<32)|((uint64_t)*(cp+4)<<24)|((uint64_t)*(cp+5)<<16)|((uint64_t)*(cp+6)<<8)|(*(cp+7));
            break;



        default:
          if (!(type&0xf0)) {
              // serial number => 24 bit - 24 bit
              if (*cp==0x08) {
                cp++;
                uint32_t s1,s2;
                s1=*cp<<16|*(cp+1)<<8|*(cp+2);
                cp+=4;
                s2=*cp<<16|*(cp+1)<<8|*(cp+2);
                sprintf(&meter_id[index][0],"%u-%u",s1,s2);
              }
          } else {
            value=999999;
            scaler=0;
          }
          break;
    }

    if (scaler==-1) {
        value/=10;
    }
    return value;
}

uint8_t hexnibble(char chr) {
  uint8_t rVal = 0;
  if (isdigit(chr)) {
    rVal = chr - '0';
  } else  {
    chr=toupper(chr);
    if (chr >= 'A' && chr <= 'F') rVal = chr + 10 - 'A';
  }
  return rVal;
}

uint8_t sb_counter;

// better char to double
double xCharToDouble(char *str) {
    // simple ascii to double, because atof or strtod are too large
    char strbuf[24];
    strlcpy(strbuf, str, sizeof(strbuf));
    char *pt=strbuf;
    double left,right;
    signed char sign=1;
    if (*pt=='-') sign=-1;
    if (*pt=='-' || *pt=='+') pt++;
    if (*pt=='.') {
        // .xxx notation
        left=0;
        goto gright;
    }
    // get left part
    left = atoi(pt);
    // skip number
    while (*pt>='0' && *pt<='9') pt++;
    if (*pt=='.') {
        // decimal part
gright:
        pt++;
        right = atoi(pt);
        while (*pt>='0' && *pt<='9') {
            pt++;
            right /= 10.0;
        }
    } else {
        right=0;
    }
    double result = (left + right);
    if (sign>=0) return result;
    else return -result;
}

// if more then 1 software serial, solution would be to enable each channel for x seconds
// no longer needed because of irq driven no wait software serial
#ifdef SML_SPECMODE
#define SS_ATIME 5*10
uint8_t active_meter;
uint8_t poll_cnt;
#endif

void SML_Poll(void) {
    uint16_t count,meters;


#ifdef SML_SPECMODE
    poll_cnt++;
    if (poll_cnt>SS_ATIME) {
      poll_cnt=0;
      // disable irq of pin
      //detachInterrupt(digitalPinToInterrupt(meter_desc[active_meter+1].srcpin));
      GPC(meter_desc[active_meter+1].srcpin) &= ~(0xF << GPCI);//INT mode disabled
      active_meter++;
      if (active_meter>=METERS_USED-1) {
        active_meter=0;
      }
      // enable irq of pin
      GPC(meter_desc[active_meter+1].srcpin) |= ((FALLING & 0xF) << GPCI);//INT mode "mode"
      //attachInterrupt(meter_desc[active_meter+1].srcpin, ISRList[meter_desc[active_meter+1].srcpin], FALLING);
    }
#endif


    for (meters=0; meters<METERS_USED; meters++) {
      if (!meter_desc[meters].srcpin || meter_desc[meters].srcpin==3) {
        while (Serial.available()) {
          // shift in
          for (count=0; count<SML_BSIZ-1; count++) {
            smltbuf[meters][count]=smltbuf[meters][count+1];
          }
          if (meter_desc[meters].type=='o') {
            smltbuf[meters][SML_BSIZ-1]=(uint8_t)Serial.read()&0x7f;
          } else {
            smltbuf[meters][SML_BSIZ-1]=(uint8_t)Serial.read();
          }
          sb_counter++;
          SML_Decode(meters);
        }
      } else {

#ifdef SML_SPECMODE
        if ((meters-1)!=active_meter) {
          // scan only active meter
          continue;
        }
#endif
        while (meter_ss[meters]->available()) {
          // shift in
          for (count=0; count<SML_BSIZ-1; count++) {
            smltbuf[meters][count]=smltbuf[meters][count+1];
          }
          if (meter_desc[meters].type=='o') {
            smltbuf[meters][SML_BSIZ-1]=(uint8_t)meter_ss[meters]->read()&0x7f;
          } else {
            smltbuf[meters][SML_BSIZ-1]=(uint8_t)meter_ss[meters]->read();
          }
          sb_counter++;
          SML_Decode(meters);
        }
      }
    }
}


void SML_Decode(uint8_t index) {
  const char *mp=(const char*)meter;
  int8_t mindex;
  uint8_t *cp;
  uint8_t dindex=0,vindex=0;
  delay(0);
  while (mp != NULL) {
    // check list of defines

    // new section
    mindex=((*mp)&7)-1;

    if (mindex<0 || mindex>=METERS_USED) mindex=0;
    mp+=2;
    if (*mp=='=' && *(mp+1)=='h') {
      mp = strchr(mp, '|');
      if (mp) mp++;
      continue;
    }

    if (index!=mindex) goto nextsect;

    // start of serial source buffer
    cp=&smltbuf[mindex][0];

    // compare
    if (*mp=='=') {
      // calculated entry, check syntax
      mp++;
      // do math m 1+2+3
      if (*mp=='m' && !sb_counter) {
        // only every 256 th byte
        // else it would be calculated every single serial byte
        mp++;
        while (*mp==' ') mp++;
        // 1. index
        double dvar;
        uint8_t ind,opr;
        ind=atoi(mp);
        while (*mp>='0' && *mp<='9') mp++;
        if (ind<1 || ind>MAX_VARS) ind=1;
        dvar=meter_vars[ind-1];
        for (uint8_t p=0;p<2;p++) {
          if (*mp=='@') {
            // store result
            meter_vars[vindex]=dvar;
            mp++;
            SML_Immediate_MQTT((const char*)mp,vindex,mindex);
            break;
          }
          opr=*mp;
          mp++;
          ind=atoi(mp);
          while (*mp>='0' && *mp<='9') mp++;
          if (ind<1 || ind>MAX_VARS) ind=1;
          switch (opr) {
              case '+':
                dvar+=meter_vars[ind-1];
                break;
              case '-':
                dvar-=meter_vars[ind-1];
                break;
              case '*':
                dvar*=meter_vars[ind-1];
                break;
              case '/':
                dvar/=meter_vars[ind-1];
                break;
          }
          while (*mp==' ') mp++;
          if (*mp=='@') {
            // store result
            meter_vars[vindex]=dvar;
            mp++;
            SML_Immediate_MQTT((const char*)mp,vindex,mindex);
            break;
          }
        }
      } else if (*mp=='d') {
        // calc deltas d ind 10 (eg every 10 secs)
        if (dindex<MAX_DVARS) {
          // only n indexes
          mp++;
          while (*mp==' ') mp++;
          uint8_t ind=atoi(mp);
          while (*mp>='0' && *mp<='9') mp++;
          if (ind<1 || ind>MAX_VARS) ind=1;
          uint32_t delay=atoi(mp)*1000;
          uint32_t dtime=millis()-dtimes[dindex];
          if (dtime>delay) {
            // calc difference
            dtimes[dindex]=millis();
            double vdiff = meter_vars[ind-1]-dvalues[dindex];
            dvalues[dindex]=meter_vars[ind-1];
            meter_vars[vindex]=(double)360000.0*vdiff/((double)dtime/10000.0);

            mp=strchr(mp,'@');
            if (mp) {
              mp++;
              SML_Immediate_MQTT((const char*)mp,vindex,mindex);
            }
          }
          dindex++;
        }
      } else if (*mp=='h') {
        // skip html tag line
        mp = strchr(mp, '|');
        if (mp) mp++;
        continue;
      }
    } else {
      // compare value
      uint8_t found=1;
      while (*mp!='@') {
        if (meter_desc[mindex].type=='o') {
          if (*mp++!=*cp++) {
            found=0;
          }
        } else {
          uint8_t val = hexnibble(*mp++) << 4;
          val |= hexnibble(*mp++);
          if (val!=*cp++) {
            found=0;
          }
        }
      }
      if (found) {
        // matches, get value
        mp++;
        if (*mp=='#') {
          // get string value
          mp++;
          if (meter_desc[mindex].type=='o') {
            for (uint8_t p=0;p<METER_ID_SIZE;p++) {
              if (*cp==*mp) {
                meter_id[mindex][p]=0;
                break;
              }
              meter_id[mindex][p]=*cp++;
            }
          } else {
            sml_getvalue(cp,mindex);
          }
        } else {
          double dval;
          // get numeric values
          if (meter_desc[mindex].type=='o') {
            dval=xCharToDouble((char*)cp);
          } else {
            dval=sml_getvalue(cp,mindex);
          }
#ifdef USE_MEDIAN_FILTER
          meter_vars[vindex]=median(&sml_mf[vindex],dval);
#else
          meter_vars[vindex]=dval;
#endif
          // get scaling factor
          double fac=xCharToDouble((char*)mp);
          meter_vars[vindex]/=fac;
          SML_Immediate_MQTT((const char*)mp,vindex,mindex);
        }
      }
    }
nextsect:
    // next section
    vindex++;
    // should never happen!
    if (vindex>=MAX_VARS) return;
    mp = strchr(mp, '|');
    if (mp) mp++;
  }
}

//"1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
void SML_Immediate_MQTT(const char *mp,uint8_t index,uint8_t mindex) {
  char tpowstr[32];
  char jname[24];

  // we must skip sf,webname,unit
  char *cp=strchr(mp,',');
  if (cp) {
    cp++;
    // wn
    cp=strchr(cp,',');
    if (cp) {
      cp++;
      // unit
      cp=strchr(cp,',');
      if (cp) {
        cp++;
        // json mqtt
        for (uint8_t count=0;count<sizeof(jname);count++) {
          if (*cp==',') {
            jname[count]=0;
            break;
          }
          jname[count]=*cp++;
        }
        cp++;
        uint8_t dp=atoi(cp);
        if (dp&0x10) {
          // immediate mqtt
          dtostrfd(meter_vars[index],dp&0xf,tpowstr);
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_TIME "\":\"%s\""), GetDateAndTime(DT_LOCAL).c_str());
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":{ \"%s\":%s}}"), mqtt_data,meter_desc[mindex].prefix,jname,tpowstr);
          MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
        }
      }
    }
  }
}


// web + jason interface
void SML_Show(boolean json) {
  int8_t count,mindex;
  char tpowstr[32];
  char name[24];
  char unit[8];
  char jname[24];
  int8_t index=0,mid=0;
  char *mp=(char*)meter;
  char *cp;

    int8_t lastmind=((*mp)&7)-1;
    if (lastmind<0 || lastmind>=METERS_USED) lastmind=0;
    while (mp != NULL) {
        // setup sections
        mindex=((*mp)&7)-1;
        if (mindex<0 || mindex>=METERS_USED) mindex=0;
        mp+=2;
        if (*mp=='=' && *(mp+1)=='h') {
          mp+=2;
          // html tag
          if (json) {
            mp = strchr(mp, '|');
            if (mp) mp++;
            continue;
          }
          // web ui export
          uint8_t i;
          for (i=0;i<sizeof(tpowstr)-2;i++) {
            if (*mp=='|' || *mp==0) break;
            tpowstr[i]=*mp++;
          }
          tpowstr[i]=0;
          // export html
          snprintf_P(mqtt_data, sizeof(mqtt_data), "%s{s}%s{e}", mqtt_data,tpowstr);
          // rewind, to ensure strchr
          mp--;
          mp = strchr(mp, '|');
          if (mp) mp++;
          continue;
        }
        // skip compare section
        cp=strchr(mp,'@');
        if (cp) {
          cp++;
          if (*cp=='#') {
            // meter id
            sprintf(tpowstr,"\"%s\"",&meter_id[mindex][0]);
            mid=1;
          } else {
            mid=0;
          }
          // skip scaling
          cp=strchr(cp,',');
          if (cp) {
            // this is the name in web UI
            cp++;
            for (count=0;count<sizeof(name);count++) {
              if (*cp==',') {
                name[count]=0;
                break;
              }
              name[count]=*cp++;
            }
            cp++;

            for (count=0;count<sizeof(unit);count++) {
              if (*cp==',') {
                unit[count]=0;
                break;
              }
              unit[count]=*cp++;
            }
            cp++;

            for (count=0;count<sizeof(jname);count++) {
              if (*cp==',') {
                jname[count]=0;
                break;
              }
              jname[count]=*cp++;
            }

            cp++;

            if (!mid) {
              uint8_t dp=atoi(cp)&0xf;
              dtostrfd(meter_vars[index],dp,tpowstr);
            }

            if (json) {
              // jason export
              if (index==0) snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":{\"%s\":%s", mqtt_data,meter_desc[mindex].prefix,jname,tpowstr);
              else {
                if (lastmind!=mindex) {
                  // meter changed, close mqtt
                  snprintf_P(mqtt_data, sizeof(mqtt_data), "%s}", mqtt_data);
                  // and open new
                  snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":{\"%s\":%s", mqtt_data,meter_desc[mindex].prefix,jname,tpowstr);
                  lastmind=mindex;
                } else {
                  snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":%s", mqtt_data,jname,tpowstr);
                }
              }
            } else {
              // web ui export
              snprintf_P(mqtt_data, sizeof(mqtt_data), "%s{s}%s %s: {m}%s %s{e}", mqtt_data,meter_desc[mindex].prefix,name,tpowstr,unit);
            }
          }
        }
        index++;
        // should never happen!
        if (index>=MAX_VARS) return;
        // next section
        mp = strchr(cp, '|');
        if (mp) mp++;
    }
    if (json) snprintf_P(mqtt_data, sizeof(mqtt_data), "%s}", mqtt_data);
}


void SML_Init(void) {
  for (uint8_t meters=0; meters<METERS_USED; meters++) {
    if (!meter_desc[meters].srcpin || meter_desc[meters].srcpin==3) {
      ClaimSerial();
      SetSerialBaudrate(SML_BAUDRATE);
    } else {
#ifdef SPECIAL_SS
      meter_ss[meters] = new TasmotaSerial(meter_desc[meters].srcpin,-1,0,1);
#else
      meter_ss[meters] = new TasmotaSerial(meter_desc[meters].srcpin,-1);
#endif
      if (meter_ss[meters]->begin(SML_BAUDRATE)) {
        meter_ss[meters]->flush();
      }
    }
  }
}

bool XSNS_95_cmd(void) {
  boolean serviced = true;
  const char S_JSON_SML[] = "{\"" D_CMND_SENSOR "%d\":%s:%d}";
  if (XdrvMailbox.data_len > 0) {
      char *cp=XdrvMailbox.data;
      if (*cp=='d') {
        cp++;
        dump2log=atoi(cp);
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_SML, XSNS_95,"dump_mode",dump2log);
      } else {
        serviced=false;
      }
  }
  return serviced;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xsns95(byte function) {
  boolean result = false;
    switch (function) {
      case FUNC_INIT:
        SML_Init();
        break;
      case FUNC_EVERY_50_MSECOND:
        if (dump2log) Dump2log();
        else SML_Poll();
        break;
      case FUNC_JSON_APPEND:
        SML_Show(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        SML_Show(0);
        break;
#endif  // USE_WEBSERVER
      case FUNC_COMMAND:
        if (XSNS_95 == XdrvMailbox.index) {
          result = XSNS_95_cmd();
        }
        break;
    }
  return result;
}

#endif  // USE_SML
