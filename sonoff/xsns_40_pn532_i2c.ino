/*
  xsns_40_pn532.ino - Support for PN532 (I2C) NFC Tag Reader

  Copyright (C) 2019  Andre Thomas and Theo Arends

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

#ifdef USE_I2C
#ifdef USE_PN532_I2C

// may block config by switch
//#define BLOCK_SWITCH

#ifdef LEARN_TAGS
#define PN532_Settings pngs
#define MAX_TAGS 10

const char S_JSON_PN532_SHOWENTRY[] PROGMEM = "{\"" D_CMND_SENSOR "%d\":%s:%d:%d:%d}";
const char S_JSON_PN532_DEL[] PROGMEM = "{\"" D_CMND_SENSOR "%d\":%s:%d}";


struct RFID {
  uint8_t uid[7];
  uint8_t bits;
// 1 bit lenght
#define LENGTH_MASK 0x01
// 3 bits relays
#define RELAY_MASK  0x0E
// 2 bits mode
#define MODE_MASK   0x30
// 2 bits pulse time info, 4 steps
#define PTMASK      0xC0
};

struct PNCFG {
  struct RFID tags[MAX_TAGS];
} pngs;

// 2 bits length 0 = not used entry
#define LENGTH_MASK 0x03
// 3 bits relays
#define RELAY_MASK  0x1C
#define RELAY_BPOS 2
// 2 bits mode
#define MODE_MASK   0x60
#define MODE_BPOS 5
// 1 bits pulse time info, 2 steps
#define PT_MASK      0x80
#define PT_BPOS 7

#define BITL4 1
#define BITL7 2

// mask UID of learned tags
#define MASK_UID


char learn=0;
int16_t pn532_tag_relay;
int16_t pn532_tag_mode;
int16_t pn532_tag_time;
#endif

/*********************************************************************************************\
 * PN532 - Near Field Communication (NFC) controller
 *
 * Datasheet at https://www.nxp.com/docs/en/nxp/data-sheets/PN532_C1.pdf
 *
 * I2C Address: 0x24
\*********************************************************************************************/

#define XSNS_40                           40

#define PN532_I2C_ADDRESS                 0x24

#define PN532_COMMAND_GETFIRMWAREVERSION  0x02
#define PN532_COMMAND_SAMCONFIGURATION    0x14
#define PN532_COMMAND_INLISTPASSIVETARGET 0x4A

#define PN532_PREAMBLE                    0x00
#define PN532_STARTCODE1                  0x00
#define PN532_STARTCODE2                  0xFF
#define PN532_POSTAMBLE                   0x00

#define PN532_HOSTTOPN532                 0xD4
#define PN532_PN532TOHOST                 0xD5

#define PN532_INVALID_ACK                 -1
#define PN532_TIMEOUT                     -2
#define PN532_INVALID_FRAME               -3
#define PN532_NO_SPACE                    -4

#define PN532_MIFARE_ISO14443A            0x00

uint8_t pn532_i2c_detected = 0;
uint8_t pn532_i2c_packetbuffer[64];
uint8_t pn532_i2c_scan_defer_report = 0;         // If a valid card was found we will not scan for one again in the same two seconds so we set this to 19 if a card was found
uint8_t pn532_i2c_command;
uint8_t pn532_i2c_disable = 0;

int16_t PN532_getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout) {
  const uint8_t PN532_NACK[] = {0, 0, 0xFF, 0xFF, 0, 0};
  uint16_t time = 0;
  do {
    if (Wire.requestFrom(PN532_I2C_ADDRESS, 6)) {
      if (Wire.read() & 1) {  // check first byte --- status
        break;         // PN532 is ready
      }
    }
    delay(1);
    time++;
    if ((0 != timeout) && (time > timeout)) {
      return -1;
    }
  } while (1);

  if ((0x00 != Wire.read()) || (0x00 != Wire.read()) || (0xFF != Wire.read())) { // PREAMBLE || STARTCODE1 || STARTCODE2
    return PN532_INVALID_FRAME;
  }

  uint8_t length = Wire.read();

  // request for last respond msg again
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  for (uint16_t i = 0;i < sizeof(PN532_NACK); ++i) {
    Wire.write(PN532_NACK[i]);
  }
  Wire.endTransmission();
  return length;
}


int16_t PN532_readResponse(uint8_t buf[], uint8_t len)
{
  uint16_t time = 0;
  uint8_t length;
  uint8_t timeout = 10;

  length = PN532_getResponseLength(buf, len, timeout);

  // [RDY] 00 00 FF LEN LCS (TFI PD0 ... PDn) DCS 00

  do {
    if (Wire.requestFrom(PN532_I2C_ADDRESS, 6 + length + 2)) {
      if (Wire.read() & 1) {  // check first byte --- status
        break;                // PN532 is ready
      }
    }
    delay(1);
    time++;
    if ((0 != timeout) && (time > timeout)) {
      return -1;
    }
  } while (1);

  if ((0x00 != Wire.read()) || (0x00 != Wire.read()) || (0xFF != Wire.read())) { // PREAMBLE || STARTCODE1 || STARTCODE2
    return PN532_INVALID_FRAME;
  }

  length = Wire.read();

  if (0 != (uint8_t)(length + Wire.read())) {   // checksum of length
    return PN532_INVALID_FRAME;
  }

  uint8_t cmd = pn532_i2c_command + 1;               // response command
  if ((PN532_PN532TOHOST != Wire.read()) || ((cmd) != Wire.read())) {
    return PN532_INVALID_FRAME;
  }
  length -= 2;
  if (length > len) {
    return PN532_NO_SPACE;  // not enough space
  }
  uint8_t sum = PN532_PN532TOHOST + cmd;
  for (uint8_t i = 0; i < length; i++) {
    buf[i] = Wire.read();
    sum += buf[i];
  }
  uint8_t checksum = Wire.read();
  if (0 != (uint8_t)(sum + checksum)) {
    return PN532_INVALID_FRAME;
  }
  Wire.read();         // POSTAMBLE
  return length;
}


int8_t PN532_readAckFrame(void)
{
  const uint8_t PN532_ACK[] = {0, 0, 0xFF, 0, 0xFF, 0};
  uint8_t ackBuf[sizeof(PN532_ACK)];

  uint16_t time = 0;

  do {
    if (Wire.requestFrom(PN532_I2C_ADDRESS,  sizeof(PN532_ACK) + 1)) {
      if (Wire.read() & 1) {  // check first byte --- status
        break;         // PN532 is ready
      }
    }
    delay(1);
    time++;
        if (time > 10) { // We time out after 10ms
            return PN532_TIMEOUT;
        }
  } while (1);

  for (uint8_t i = 0; i < sizeof(PN532_ACK); i++) {
    ackBuf[i] = Wire.read();
  }

  if (memcmp(ackBuf, PN532_ACK, sizeof(PN532_ACK))) {
    return PN532_INVALID_ACK;
  }

  return 0;
}

int8_t PN532_writeCommand(const uint8_t *header, uint8_t hlen)
{
  pn532_i2c_command = header[0];
  Wire.beginTransmission(PN532_I2C_ADDRESS);
  Wire.write(PN532_PREAMBLE);
  Wire.write(PN532_STARTCODE1);
  Wire.write(PN532_STARTCODE2);
  uint8_t length = hlen + 1;          // TFI + DATA
  Wire.write(length);
  Wire.write(~length + 1);            // checksum of length
  Wire.write(PN532_HOSTTOPN532);
  uint8_t sum = PN532_HOSTTOPN532;    // Sum of TFI + DATA
  for (uint8_t i = 0; i < hlen; i++) {
    if (Wire.write(header[i])) {
      sum += header[i];
    } else {
      return PN532_INVALID_FRAME;
    }
  }
  uint8_t checksum = ~sum + 1;        // Checksum of TFI + DATA
  Wire.write(checksum);
  Wire.write(PN532_POSTAMBLE);
  Wire.endTransmission();
  return PN532_readAckFrame();
}

uint32_t PN532_getFirmwareVersion(void)
{
    uint32_t response;
    pn532_i2c_packetbuffer[0] = PN532_COMMAND_GETFIRMWAREVERSION;
    if (PN532_writeCommand(pn532_i2c_packetbuffer, 1)) {
        return 0;
    }
    int16_t status = PN532_readResponse(pn532_i2c_packetbuffer, sizeof(pn532_i2c_packetbuffer));
    if (0 > status) {
        return 0;
    }
    response = pn532_i2c_packetbuffer[0];
    response <<= 8;
    response |= pn532_i2c_packetbuffer[1];
    response <<= 8;
    response |= pn532_i2c_packetbuffer[2];
    response <<= 8;
    response |= pn532_i2c_packetbuffer[3];
    return response;
}

bool PN532_SAMConfig(void)
{
    pn532_i2c_packetbuffer[0] = PN532_COMMAND_SAMCONFIGURATION;
    pn532_i2c_packetbuffer[1] = 0x01; // normal mode;
    pn532_i2c_packetbuffer[2] = 0x14; // timeout 50ms * 20 = 1 second
    pn532_i2c_packetbuffer[3] = 0x01; // use IRQ pin!

    if (PN532_writeCommand(pn532_i2c_packetbuffer, 4))
        return false;

    return (0 < PN532_readResponse(pn532_i2c_packetbuffer, sizeof(pn532_i2c_packetbuffer)));
}

void PN532_Detect(void)
{
  if ((pn532_i2c_detected) || (pn532_i2c_disable)) { return; }

  uint32_t ver = PN532_getFirmwareVersion();
  if (ver) {
    pn532_i2c_detected = 1;
    snprintf_P(log_data, sizeof(log_data), S_LOG_I2C_FOUND_AT, "PN532 NFC Reader (V%u.%u)", PN532_I2C_ADDRESS);
    snprintf_P(log_data, sizeof(log_data), log_data, (ver>>16) & 0xFF, (ver>>8) & 0xFF);
    AddLog(LOG_LEVEL_DEBUG);
    PN532_SAMConfig();

#ifdef LEARN_TAGS
    ReadFromEEPROM();
    uint64_t *lptr=(uint64_t *)&PN532_Settings.tags[0].uid;
    if (*lptr==0xffffffffffffffff) {
      // assume eeprom not erased
      EraseEEPROM();
    }
#endif
  }
}

boolean PN532_readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength)
{
  pn532_i2c_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  pn532_i2c_packetbuffer[1] = 1;
  pn532_i2c_packetbuffer[2] = cardbaudrate;

  if (PN532_writeCommand(pn532_i2c_packetbuffer, 3)) {
    return false;  // command failed
  }

  if (PN532_readResponse(pn532_i2c_packetbuffer, sizeof(pn532_i2c_packetbuffer)) < 0) { // No data packet so no tag was found
    Wire.beginTransmission(PN532_I2C_ADDRESS);
    Wire.endTransmission();
    return false;
  }

  if (pn532_i2c_packetbuffer[0] != 1) { return false; } // Not a valid tag

  *uidLength = pn532_i2c_packetbuffer[5];
  for (uint8_t i = 0;i < pn532_i2c_packetbuffer[5]; i++) {
    uid[i] = pn532_i2c_packetbuffer[6 + i];
  }
  return true;
}


uint8_t last_uid[7];  // Buffer to store the returned UID
uint8_t last_uidLength=0;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

void PN532_ScanForTag(void)
{

  if (pn532_i2c_disable) { return; }
  uint8_t found;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uid_len = 0;
  if (PN532_readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uid_len)) {
    if (pn532_i2c_scan_defer_report > 0) {
      pn532_i2c_scan_defer_report--;
    } else {
      char uids[15];

#ifdef LEARN_TAGS

      if (learn) {
        // in learn mode, store or delete next tag
        int16_t result=learn_tag(uid,uid_len,learn);
        // should report sucess here
        const char *cp;
        if (result==0) {
          if (learn==1) cp="tag %s added";
          else cp="tag %s deleted";
        } else {
          cp="tag list is full";
        }
        array_to_hstring(uid,uid_len,uids);
        snprintf_P(log_data, sizeof(log_data),cp,uids);
        AddLog(LOG_LEVEL_INFO);
        learn=0;
        return;
      }

      // check if known tag
      found=compare_tag(uid,uid_len);
      if (found) {

#ifdef MASK_UID
        // learned, set dummy uid with index
        memset(uid,0,uid_len);
        uid[uid_len-1]=found;
#endif

        // and execute with paramters
        pn532_execute(found);
      }

#endif

      last_uidLength=uid_len;
      memmove(last_uid,uid,uid_len);

      sprintf(uids,"");
      for (uint8_t i = 0;i < uid_len;i++) {
        sprintf(uids,"%s%X",uids,uid[i]);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_TIME "\":\"%s\""), GetDateAndTime(DT_LOCAL).c_str());
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"PN532\":{\"UID\":\"%s\"}}"), mqtt_data, uids);
      MqttPublishPrefixTopic_P(TELE, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
      char command[27];
      sprintf(command,"event PN532=%s",uids);
      ExecuteCommand(command, SRC_RULE);
      pn532_i2c_scan_defer_report = 19;
    }
  } else {
    if (pn532_i2c_scan_defer_report > 0) { pn532_i2c_scan_defer_report--; }
  }
}

#ifdef LEARN_TAGS
// execute tag cmd
void pn532_execute(uint8_t index) {
  uint8_t mode,relay,time;
  index--;
  relay=(PN532_Settings.tags[index].bits&RELAY_MASK)>>RELAY_BPOS;
  relay++;
  mode=(PN532_Settings.tags[index].bits&MODE_MASK)>>MODE_BPOS;
  time=(PN532_Settings.tags[index].bits&PT_MASK)>>PT_BPOS;
  switch (mode&3) {
    case 0:
      // relay off
      ExecuteCommandPower(relay, POWER_OFF, SRC_BUTTON);
      break;
    case 1:
      // relay on
      ExecuteCommandPower(relay, POWER_ON, SRC_BUTTON);
      break;
    case 2:
      // toggle relay
      ExecuteCommandPower(relay, POWER_TOGGLE, SRC_BUTTON);
      break;
    case 3:
      // pulse relay 250 or 500 ms
      ExecuteCommandPower(relay, POWER_ON, SRC_BUTTON);
      delay((time+1)*250);
      ExecuteCommandPower(relay, POWER_OFF, SRC_BUTTON);
      break;
  }
}

// returns entry number from 1-10 or zero
int16_t compare_tag(uint8_t *uid,uint8_t uidLength) {
uint16_t count,len;
  for (count=0; count<MAX_TAGS; count++) {
    len=PN532_Settings.tags[count].bits&LENGTH_MASK;
    if (len) {
      if (!memcmp(uid,PN532_Settings.tags[count].uid,uidLength)) {
        // did match
        return count+1;
      }
    }
  }
  return 0;
}

// learn or delete tag mode=1 => learn, mode=2 => delete
int16_t learn_tag(uint8_t *uid,uint8_t uidLength,uint8_t mode) {
uint16_t count,len;
uint8_t bits;
  // compare if already there
  uint8_t found=0;
  for (count=0; count<MAX_TAGS; count++) {
    len=PN532_Settings.tags[count].bits&LENGTH_MASK;
    if (len) {
      if (!memcmp(uid,PN532_Settings.tags[count].uid,uidLength)) {
        found=count+1;
        break;
      }
    }
  }
  if (found) {
    if (mode==1) {
      // replace old entry
      memmove(PN532_Settings.tags[found-1].uid,uid,uidLength);
      if (uidLength==4) bits=BITL4;
      else bits=BITL7;
      bits|=(pn532_tag_relay-1)<<RELAY_BPOS;
      bits|=(pn532_tag_mode)<<MODE_BPOS;
      bits|=(pn532_tag_time-1)<<PT_BPOS;
      PN532_Settings.tags[found-1].bits=bits;
    } else {
      // delete old entry
      PN532_Settings.tags[found-1].bits=0;
    }
  } else {
    // look for free entry
    found=0;
    for (count=0; count<MAX_TAGS; count++) {
      if (!PN532_Settings.tags[count].bits) {
        // copy new entry
        memmove(PN532_Settings.tags[count].uid,uid,uidLength);
        if (uidLength==4) bits=BITL4;
        else bits=BITL7;
        bits|=(pn532_tag_relay-1)<<RELAY_BPOS;
        bits|=pn532_tag_mode<<MODE_BPOS;
        bits|=(pn532_tag_time-1)<<PT_BPOS;
        PN532_Settings.tags[count].bits=bits;
        found=1;
        break;
      }
    }
    if (!found) {
      // list is full
      return -1;
    }
  }
  Save2EEPROM();
  return 0;
}


// save ram to eeprom
void Save2EEPROM(void) {
  uint8_t *mptr=(uint8_t*)&pngs;
  for (uint16_t count=0; count<sizeof(struct PNCFG); count++) {
    EepromWrite(count, *mptr++);
  }
  EepromCommit();
}

void EraseEEPROM(void) {
  uint8_t *mptr=(uint8_t*)&pngs;
  for (uint16_t count=0; count<sizeof(struct PNCFG); count++) {
    *mptr++=0;
  }
  Save2EEPROM();
}

// read tag structure from eeprom
void ReadFromEEPROM(void) {
  EepromBegin(sizeof(struct PNCFG));
  uint8_t *mptr=(uint8_t*)&pngs;
  for (uint16_t count=0; count<sizeof(struct PNCFG); count++) {
    *mptr++=EepromRead(count);
  }
}
// get asci number until delimiter and return asci number lenght and value
uint8_t pn532_atoiv(char *cp, int16_t *res)
{
  uint8_t index = 0;
  // skip leading spaces
  while (*cp==' ') {
    cp++;
    index++;
  }

  *res = atoi(cp);
  while (*cp) {
    if ((*cp>='0' && *cp<='9') || (*cp=='-')) {
      cp++;
      index++;
    } else {
      break;
    }
  }
  return index;
}

// execute sensor40 cmds
bool PN532_cmd(void)
{
  boolean serviced = true;
  uint8_t var;
  int16_t wvar;
  char uid_str[16];

#ifdef BLOCK_SWITCH
  // block cmds with switch 1
  if (pin[GPIO_SWT1]<99) {
      if (digitalRead(pin[GPIO_SWT1])) {
        return false;
      }
  }
#endif

  if (XdrvMailbox.data_len > 0) {
    char *cp=XdrvMailbox.data;
    if (*cp=='a') {
      // add tag  => a relaynr mode time and wait for tag
      // mode 0 = off, 1 = on, 2 = toggle, 3 = pulse with time in ms, bit 7 => report as index UID
      cp++;
      if (*cp) {
        var = pn532_atoiv(cp, &pn532_tag_relay);
        cp += var;
      }
      if (pn532_tag_relay<1 || pn532_tag_relay>8) pn532_tag_relay=1;
      if (*cp) {
        var = pn532_atoiv(cp, &pn532_tag_mode);
        cp += var;
      }
      pn532_tag_mode&=3;

      if (*cp) {
        var = pn532_atoiv(cp, &pn532_tag_time);
        cp += var;
      }
      if (pn532_tag_time<1 || pn532_tag_time>2) pn532_tag_time=1;
      learn=1;

      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_PN532_SHOWENTRY, XSNS_40,"wait for tag",pn532_tag_relay,pn532_tag_mode,pn532_tag_time);
    } else if (*cp=='d') {
      // delete entry num  => d num
      cp++;
      if (*cp) {
        var = pn532_atoiv(cp, &wvar);
        cp += var;
        if (wvar>=1 && wvar<=MAX_TAGS) {
          PN532_Settings.tags[wvar-1].bits=0;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_PN532_DEL, XSNS_40,"delete tag index",wvar);
    } else if (*cp=='D') {
      // delete tag => D and wait for tag
      cp++;
      learn=2;
      snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_PN532_DEL, XSNS_40,"wait for tag to delete",0);
    }
    else if (*cp=='s') {
      // show entry num => s index
      cp++;
      if (*cp) {
        var = pn532_atoiv(cp, &wvar);
        cp += var;
        if (wvar>=1 && wvar<=MAX_TAGS) {
          wvar--;
          uint8_t len;
          if (PN532_Settings.tags[wvar].bits&LENGTH_MASK==BITL4) len=4;
          else len=7;
          array_to_hstring(PN532_Settings.tags[wvar].uid,len,uid_str);
          snprintf_P(mqtt_data, sizeof(mqtt_data), S_JSON_PN532_SHOWENTRY, XSNS_40,uid_str,(((PN532_Settings.tags[wvar].bits&RELAY_MASK)>>RELAY_BPOS)&7)+1,((PN532_Settings.tags[wvar].bits&MODE_MASK)>>MODE_BPOS)&3,((PN532_Settings.tags[wvar].bits&PT_MASK)>>PT_BPOS)+1);
        }
      }
    }
    else  {
      // other option
    }
  }

  return serviced;
}
#endif

// convert a byte array to a hex string
// sprintf hex formatter not supported in all versions of arduino ?
void array_to_hstring(uint8_t array[], uint8_t len, char buffer[])
{

#if 1
    for (uint8_t i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
#else
    for (uint8_t i = 0;i < len;i++) {
      sprintf(buffer,"%s%X",buffer,array[i]);
    }
#endif
}

#ifdef USE_WEBSERVER
const char HTTP_PN532[] PROGMEM = "%s"
 "{s}PN532 " "UID" "{m}%s" "{e}";

void PN532_Show(void) {
char uid_str[16];

	if (!last_uidLength) {
    // shows "----" if ic not detected
    if (pn532_i2c_detected) strcpy(uid_str,"????");
    else strcpy(uid_str,"----");
  }
  else array_to_hstring(last_uid,last_uidLength,uid_str);

  snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_PN532, mqtt_data,uid_str);

}
#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xsns40(byte function)
{
  boolean result = false;

  if (i2c_flg) {
    switch (function) {
      case FUNC_EVERY_250_MSECOND:
        if (pn532_i2c_detected) {
          PN532_ScanForTag();
        }
        break;
      case FUNC_EVERY_SECOND:
        PN532_Detect();
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        PN532_Show();
        break;
#endif  // USE_WEBSERVER

#ifdef LEARN_TAGS
      case FUNC_COMMAND:
        if (XSNS_40 == XdrvMailbox.index) {
          result = PN532_cmd();
        }
        break;
#endif

    // this does not work, because save cmd calls it
      case FUNC_SAVE_BEFORE_RESTART:
        if (!pn532_i2c_disable) {
          //pn532_i2c_disable = 1;
          snprintf_P(log_data, sizeof(log_data), S_LOG_I2C_FOUND_AT, "PN532 NFC Reader - Disabling for reboot", PN532_I2C_ADDRESS);
          AddLog(LOG_LEVEL_DEBUG);
        }
        break;
    }

  }
  return result;
}

#endif  // USE_PN532_I2C
#endif  // USE_I2C
