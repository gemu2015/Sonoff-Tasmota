Smart Message Language \#define USE\_SML\_M

===================================================

works with binary SML and ASCI OBIS (can easily be modified)

convert eg a sonoff basic to a smart meter reader

simply connect serial rec pin to phototransistor with 1 kOhm Pullup and
apply to smart meter ir diode

e.g. Phototransistor TEKT5400S works very well.

uses RX pin of serial port

please note that the normal serial port is used

now supports reading of more then one meter via special software serial
(no wait irq driven)

Also added an STL file for printing a housing for the Phototransistor
(TEKT5400)

you may set sleep to at least 50 to reduce total power consumptions to
about 0,3 Watts

EBUS (Wolf) \#define USE\_EBUS

===================================================

convert eg a sonoff basic to an EBUS reader

connect rec pin with level converter for ebus levels (read only mode)

uses RX pin of serial port

LedBar \#define USE\_LEDBAR

===================================================

implements a LED bar display on WS2812 led chain to show values

default is +-5000 units in 5 steps each

ledbar xxx =\> show value

ledbar rxxx =\> set range positiv values =\> positiv up, else negativ up

ledbar sxx =\> set steps

only in use when LED Power is off, else ignored

VL5310x \#define USE\_VL53L0X

===================================================

time of flight range sensor with median filter 0-2000 mm

TCS34725 \#define USE\_TCS34725

===================================================

high dynamic lux and color temperatur

PN532 \#define USE\_PN532\_I2C

===================================================

RFID Reader connect via I2C

this is an extended tasmota version with the \"learn\" option \#define
LEARN\_TAGS

you can learn tags and define master tags that enable learning or
deleting of other tags

the learn option can store up to 3000 tags when using external I2C
eeprom AT24C256 \#define USE\_24C256

otherwise with the ES8266 simulated eeprom about 30

// current commands

a relay mode time =\> wait for tag to learn

A UID relay mode time =\> enter UID in list directly

AM UID relay mode time =\> enter UID in list directly as a learning
master

AD UID relay mode time =\> enter UID in list directly as a deleting
master

d index delete tag of index

D wait for tag to delete

e erase tag list

s index show list entry of index

// for these commands the tag must be placed onto the reader

// format a card to NDEF

f =\> format to NDEF

// reformat an NDEF card to Mifare

fm =\>reformat an NDEF tag to Mifare

// add a new NDEF record

n sector (1-15) type record =\> (up to 38 asci chars )

type can be found on the net NDEF Type definitions

n=new entry nu=update existing entry

// write a block to Mifare card

w blocknr 16 bytes of hex date without spaces

// read a block from Mifare card

r blocknr

RDM6300 \#define USE\_RDM6300

===================================================

RFID Reader connect to RX PIN

sends TAG UID via MQTT

====================================

sendmail #define USE_SENDMAIL

usage => sendmail[smptserver:port:user:passw:<from>:<to>:subject] message

send email with TLS on ports !=25 (on 25 without TLS)

uses a lot of ram (>20k) and crashes on to few ram. does therfore not work from WEB console but works in rules and serial monitor
and about 70k flash

example:

sendmail [smtp.gmail.com:465:user:passwd:<misterx@gmail.com>:<missesx@gmail.com>:TASMOTA mail] Hallo TASMOTA




\+ various display drivers see separate doku
