

Smart Message Language #define USE_SML_M
===================================================
works with binary SML and ASCI OBIS (can easily be modified)

convert eg a sonoff basic to a smart meter reader
simply connect serial rec pin to phototransistor with 1 kOhm Pullup and apply to smart meter ir diode
e.g. Phototransistor TEKT5400S works very well.

uses RX pin of serial port
please note that the normal serial port is used
Also added an STL file for printing a housing for the Phototransistor (TEKT5400)
you may set sleep to at least 50 to reduce total power consumptions to about  0,3 Watts


EBUS (Wolf) #define USE_EBUS
===================================================
convert eg a sonoff basic to an EBUS reader
connect rec pin with level converter for ebus levels (read only mode)

uses RX pin of serial port



LedBar #define USE_LEDBAR
===================================================
implements a LED bar display on WS2812 led chain to show values

default is +-5000 units in 5 steps each

ledbar xxx => show value
ledbar rxxx => set range  positiv values => positiv up, else negativ up
ledbar sxx  => set steps

only in use when LED Power is off, else ignored

VL5310x #define USE_VL53L0X
===================================================
time of flight range sensor with median filter 0-2000 mm

TCS34725 #define USE_TCS34725
===================================================
high dynamic lux and color temperatur


RDM6300 #define USE_RDM6300
===================================================
RFID Reader connect to RX PIN

+ various display drivers see doku 
