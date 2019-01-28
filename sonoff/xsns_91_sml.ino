/*
  xsns_91_sml.ino - SML smart meter interface for Sonoff-Tasmota

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

#ifdef USE_SML

#define XSNS_91 91

// Baudrate des D0 Ausgangs, sollte bei den meisten Zählern 9600 sein
#define SML_BAUDRATE 9600

// diese Version verwendet den serial REC Pin des ESP
// kann mit jeder aktuellen Version von Tasmota kombiniert werden
// dazu muss z.B. in der user_config_override #define USE_SML angegeben werden
// Als "Lesekopf" kann ein Fototransistor (z.B. TEKT5400 oder BPW78A) zwischen
// Masse und dem ESP REC pin verwendet werden. Eventuell ist ein zusätzlicher
// Pullup Widerstand zwischen REC und VCC 3.3 Volt erforderlich

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

// JASON Strings NICHT übersetzen
// max 23 Zeichen
#define DJ_TPWRIN "Total_in"
#define DJ_TPWROUT "Total_out"
#define DJ_TPWRCURR "Power_curr"
#define DJ_TPWRCURR1 "Power_p1"
#define DJ_TPWRCURR2 "Power_p2"
#define DJ_TPWRCURR3 "Power_p3"
#define DJ_METERNR "Meter_number"


// Zählerliste , hier neue Zähler eintragen
//=====================================================
#define EHZ161_0 1
#define EHZ161_1 2
#define EHZ363 3
#define EHZH 4
#define EDL300 5
#define Q3B 6

// diesen Zähler auswählen
#define METER EHZ161_1

//=====================================================
// Einträge in Liste
// start bis @ Zeichen => Sequenz von OBIS als ASCI, oder SML als HEX ASCI
// Skalierungsfaktor (Divisor) (kann auch negativ sein oder kleiner 0 z.B. 0.1 => Ergebnis * 10)
// statt des Skalierungsfaktors kann hier (nur in einer Zeile) ein # Zeichen stehen (bisher nur OBIS)
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


#if METER==EHZ161_0
#define USE_OBIS 1
const uint8_t meter[]=
"1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1-0:2.8.0*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"1-0:21.7.0*255(@1," D_TPWRCURR1 ",W," DJ_TPWRCURR1 ",0|"
"1-0:41.7.0*255(@1," D_TPWRCURR2 ",W," DJ_TPWRCURR2 ",0|"
"1-0:61.7.0*255(@1," D_TPWRCURR3 ",W," DJ_TPWRCURR3 ",0|"
"=m 3+4+5 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";

#endif

//=====================================================

#if METER==EHZ161_1
#define USE_OBIS 1
const uint8_t meter[]=
"1-0:1.8.1*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
"1-0:2.8.1*255(@1," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
"=d 2 10 @1," D_TPWRCURR ",W," DJ_TPWRCURR ",0|"
"1-0:0.0.0*255(@#)," D_METERNR ",," DJ_METERNR ",0";
#endif

//=====================================================

#if METER==EHZ363
#define USE_OBIS 0
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x00,0xff
"77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x10,0x07,0x00,0xff
"77070100100700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==EHZH
#define USE_OBIS 0
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
// verbrauch total
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"77070100020800ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x0f,0x07,0x00,0xff
"770701000f0700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==EDL300
#define USE_OBIS 0
// 2 Richtungszähler EHZ SML 8 bit 9600 baud, binär
// verbrauch total
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x00,0xff
"77070100010800ff@1000," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"77070100020801ff@1000," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x0f,0x07,0x00,0xff
"770701000f0700ff@1," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
#endif

//=====================================================

#if METER==Q3B
#define USE_OBIS 0
const uint8_t meter[]=
//0x77,0x07,0x01,0x00,0x01,0x08,0x01,0xff
"77070100010800ff@100," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
//0x77,0x07,0x01,0x00,0x02,0x08,0x01,0xff
"77070100020801ff@100," D_TPWROUT ",KWh," DJ_TPWROUT ",4|"
//0x77,0x07,0x01,0x00,0x01,0x07,0x00,0xff
"77070100010700ff@100," D_TPWRCURR ",W," DJ_TPWRCURR ",0";
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
#define MAX_DVARS 2
double dvalues[MAX_DVARS];
uint32_t dtimes[MAX_DVARS];

// serial buffer
#define SML_BSIZ 28
uint8_t smltbuf1[SML_BSIZ];

#define METER_ID_SIZE 22
char meter_id[METER_ID_SIZE];

#if USE_OBIS>0
#define DJ_TYPE "OBIS"
#else
#define DJ_TYPE "SML"
#endif

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
// dazu in Konsole sensor91 d1 bzw. d0 für Ein und Ausschalten angeben

char sml_start;
uint8_t dump2log=0;

#define SML_SAVAILABLE Serial.available()
#define SML_SREAD Serial.read()
#define SML_SPEAK Serial.peek()

void Dump2log(void) {
int16_t index=0,hcnt=0;
uint32_t d_lastms;
  uint8_t dchars[16];

  if (!SML_SAVAILABLE) return;

  if (dump2log>1) {
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
#if USE_OBIS>0
          if (index<LOGSZ-2) {
            char c=SML_SREAD&0x7f;
            if (c=='\n' || c=='\r') break;
            log_data[index]=c;
            index++;
#else
          if (index<LOGSZ-4) {
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
#endif
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


#if USE_OBIS==0
// get sml binary value
int64_t sml_getvalue(unsigned char *cp) {
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
        case 0x51:
            // int8
            value=(signed char)*cp;
            break;
        case 0x52:
            // int16;
            value=((int16_t)*cp<<8)|*(cp+1);
            break;
        case 0x54:
            // int32;
            value=((int32_t)*cp<<24)|((int32_t)*(cp+1)<<16)|((int32_t)*(cp+2)<<8)|(*(cp+3));
            break;
        case 0x55:
            // int32+1;
            cp++;
            value=((int32_t)*cp<<24)|((int32_t)*(cp+1)<<16)|((int32_t)*(cp+2)<<8)|(*(cp+3));
            break;

        case 0x58:
            // int64;
            value=((int64_t)*cp<<56)|((int64_t)*(cp+1)<<48)|((int64_t)*(cp+2)<<40)|((int64_t)*(cp+3)<<32)|((int64_t)*(cp+4)<<24)|((int64_t)*(cp+5)<<16)|((int64_t)*(cp+6)<<8)|(*(cp+7));
            break;
        case 0x61:
            // uint8;
            value=(unsigned char)*cp;
            break;
        case 0x62:
            // uint16;
            value=((uint16_t)*cp<<8)|(*(cp+1));
            break;
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
              // octet string
              uint8_t cnt;
              for (cnt=0;cnt<type-1;cnt++ ) {
                meter_id[cnt]=*cp++;
              }
              meter_id[cnt]=0;
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
#endif

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

void SML_Poll(void) {
    uint16_t count;

    while (SML_SAVAILABLE) {
        // shift in
        for (count=0; count<sizeof(smltbuf1)-1; count++) {
            smltbuf1[count]=smltbuf1[count+1];
        }
#if USE_OBIS>0
        smltbuf1[sizeof(smltbuf1)-1]=(uint8_t)SML_SREAD&0x7f;
#else
        smltbuf1[sizeof(smltbuf1)-1]=(uint8_t)SML_SREAD;
#endif

        sb_counter++;
        const uint8_t *mp=meter;
        uint8_t *cp=smltbuf1;
        char *cpos=0;

        uint8_t index=0,found=1,comparing=1,dindex=0;
        for (count=0; count<sizeof(meter); count++) {
            // check list of defines
            if (*mp=='|') {
                // new section
                cp=smltbuf1;
                mp++;
                found=1;
                comparing=1;
                cpos=0;
                index++;
                // should never happen!
                if (index>=MAX_VARS) return;
            } else {
                if (comparing) {
                    // compare part
                    if (*mp=='@') {
                        // end of compare section
                        // here we have a match, process params
                        if (found) {
                          mp++;
                          if (*mp=='#') {
                            mp++;
                            // read meter id string
#if USE_OBIS>0
                            for (uint8_t p=0;p<sizeof(meter_id);p++) {
                              if (*cp==*mp) {
                                meter_id[p]=0;
                                break;
                              }
                              meter_id[p]=*cp++;
                            }
#else
                            sml_getvalue(cp);
                            //strlcpy(meter_id,"not yet",sizeof(meter_id));
#endif

                          } else {
                            double dval;
#if USE_OBIS>0
                            dval=xCharToDouble((char*)cp);
#else
                            dval=sml_getvalue(cp);
#endif

#ifdef USE_MEDIAN_FILTER
                            meter_vars[index]=median(&sml_mf[index],dval);
#else
                            meter_vars[index]=dval;
#endif

                            // get scaling factor
                            double fac=xCharToDouble((char*)mp);
                            //char tpowstr[24];
                            //dtostrfd(fac,5,tpowstr);
                            //sprintf(log_data,"%s",tpowstr);
                            //AddLog(LOG_LEVEL_INFO);
                            meter_vars[index]/=fac;
                            SML_Immediate_MQTT((const char*)mp,index);
                          }
                          break;
                        }

                        // check if we have a = section

                        if (cpos) {
                          // calculated entry, check syntax
                          // do math m 1+2+3
                          if (*cpos=='m' && !sb_counter) {
                            // only every 256 th byte
                            // else it would be calculated every single serial byte
                              cpos++;
                              while (*cpos==' ') cpos++;
                              // 1. index
                              double dvar;
                              uint8_t ind,opr;
                              ind=atoi(cpos);
                              while (*cpos>='0' && *cpos<='9') cpos++;
                              if (ind<1 || ind>MAX_VARS) ind=1;
                              dvar=meter_vars[ind-1];
                              for (uint8_t p=0;p<2;p++) {
                                if (*cpos=='@') {
                                  // store result
                                  meter_vars[index]=dvar;
                                  cpos++;
                                  SML_Immediate_MQTT((const char*)cpos,index);
                                  break;
                                }
                                opr=*cpos;
                                cpos++;
                                ind=atoi(cpos);
                                while (*cpos>='0' && *cpos<='9') cpos++;
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
                                while (*cpos==' ') cpos++;
                                if (*cpos=='@') {
                                  // store result
                                  meter_vars[index]=dvar;
                                  cpos++;
                                  SML_Immediate_MQTT((const char*)cpos,index);
                                  break;
                                }
                              }

                          } else if (*cpos=='d') {
                            // calc deltas d ind 10 (eg every 10 secs)
                            if (dindex<MAX_DVARS) {
                              // only 2 indexes
                              cpos++;
                              while (*cpos==' ') cpos++;
                              uint8_t ind=atoi(cpos);
                              while (*cpos>='0' && *cpos<='9') cpos++;
                              if (ind<1 || ind>MAX_VARS) ind=1;
                              uint32_t delay=atoi(cpos)*1000;
                              uint32_t dtime=millis()-dtimes[dindex];
                              if (dtime>delay) {
                                // calc difference
                                dtimes[dindex]=millis();
                                double vdiff = meter_vars[ind-1]-dvalues[dindex];
                                dvalues[dindex]=meter_vars[ind-1];
                                meter_vars[index]=(double)360000.0*vdiff/((double)dtime/10000.0);
                                cpos=strchr(cpos,'@');
                                if (cpos) {
                                  cpos++;
                                  SML_Immediate_MQTT((const char*)cpos,index);
                                }

                              }
                              dindex++;
                            }
                          }
                          cpos=0;
                        }
                        comparing=0;
                    } else {
                        if (*mp=='=') {
                          // calculated entries, remember position
                          cpos=(char*)mp+1;
                        }
#if USE_OBIS>0
                        if (*mp!=*cp) {
                            found=0;
                          }
                          mp++;
                          cp++;
#else
                        // hex nibbles
                        uint8_t val = hexnibble(*mp++) << 4;
                        val |= hexnibble(*mp++);
                        if (val!=*cp) {
                          found=0;
                        }
                        cp++;
                        count++;
#endif

                    }
                } else {
                    mp++;
                }
            }
        }
    }
}

//"1-0:1.8.0*255(@1," D_TPWRIN ",KWh," DJ_TPWRIN ",4|"
void SML_Immediate_MQTT(const char *mp,uint8_t index) {
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
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":{ \"%s\":%s}}"), mqtt_data,DJ_TYPE,jname,tpowstr);
          MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
        }
      }
    }
  }
}


// web + jason interface
void SML_Show(boolean json) {
  uint8_t count;
  char tpowstr[32];
  char name[24];
  char unit[8];
  char jname[24];

    int8_t index=0,mid=0;
    char *mp=(char*)meter;
    while (mp != NULL) {
        // setup sections
        // skip compare section
        char *cp=strchr(mp,'@');
        if (cp) {
          cp++;
          if (*cp=='#') {
            // meter id
            sprintf(tpowstr,"\"%s\"",meter_id);
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
              if (index==0) snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":{\"%s\":%s", mqtt_data,DJ_TYPE,jname,tpowstr);
              else snprintf_P(mqtt_data, sizeof(mqtt_data), "%s,\"%s\":%s", mqtt_data,jname,tpowstr);
            } else {
              // web ui export
              snprintf_P(mqtt_data, sizeof(mqtt_data), "%s{s}%s %s: {m}%s %s{e}", mqtt_data,DJ_TYPE,name,tpowstr,unit);
            }
          }
        }
        index++;
        // should never happen!
        if (index>=MAX_VARS) return;
        // next section
        mp = strchr(cp, '|');
    }
    if (json) snprintf_P(mqtt_data, sizeof(mqtt_data), "%s}", mqtt_data);
}


void SML_Init(void) {
  SetSerialBaudrate(SML_BAUDRATE);
  // request serial line
  ClaimSerial();
}


bool XSNS_91_cmd(void) {
  boolean serviced = true;
  const char S_JSON_SML[] = "{\"" D_CMND_SENSOR "%d\":%s:%d}";
  if (XdrvMailbox.data_len > 0) {
      char *cp=XdrvMailbox.data;
      if (*cp=='d') {
        cp++;
        dump2log=*cp&7;
        snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_SML, XSNS_91,"dump_mode",dump2log);
      } else {
        serviced=false;
      }
  }
  return serviced;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xsns91(byte function) {
  boolean result = false;
    switch (function) {
      case FUNC_INIT:
        SML_Init();
        break;
      case FUNC_EVERY_50_MSECOND:
        if (dump2log) Dump2log();
        break;
      case FUNC_EVERY_100_MSECOND:
        if (!dump2log) SML_Poll();
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
        if (XSNS_91 == XdrvMailbox.index) {
          result = XSNS_91_cmd();
        }
        break;
    }
  return result;
}

#endif  // USE_SML
