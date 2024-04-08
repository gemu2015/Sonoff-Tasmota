

/*!
  \brief No shaping.
*/
#define RADIOLIB_SHAPING_NONE                                   (0x00)

/*!
  \brief Gaussian shaping filter, BT = 0.3
*/
#define RADIOLIB_SHAPING_0_3                                    (0x01)

/*!
  \brief Gaussian shaping filter, BT = 0.5
*/
#define RADIOLIB_SHAPING_0_5                                    (0x02)

/*!
  \brief Gaussian shaping filter, BT = 0.7
*/
#define RADIOLIB_SHAPING_0_7                                    (0x03)

/*!
  \brief Gaussian shaping filter, BT = 1.0
*/
#define RADIOLIB_SHAPING_1_0                                    (0x04)

/*!
  \}
*/

/*!
  \defgroup config_encoding Encoding type aliases.

  \{
*/

/*!
  \brief Non-return to zero - no encoding.
*/
#define RADIOLIB_ENCODING_NRZ                                   (0x00)

/*!
  \brief Manchester encoding.
*/
#define RADIOLIB_ENCODING_MANCHESTER                            (0x01)

/*!
  \brief Whitening.
*/
#define RADIOLIB_ENCODING_WHITENING                             (0x02)

/*!
  \}
*/

/*!
  \defgroup config_standby Standby mode type aliases.

  \{
*/

/*!
  \brief Default standby used by the module
*/
#define RADIOLIB_STANDBY_DEFAULT                                (0x00)

/*!
  \brief Warm standby (e.g. crystal left running).
*/
#define RADIOLIB_STANDBY_WARM                                   (0x01)

/*!
  \brief Cold standby (e.g. only internal RC oscillator running).
*/
#define RADIOLIB_STANDBY_COLD                                   (0x02)

/*!
  \}
*/

/*!
  \defgroup status_codes Status Codes

  \{
*/

// common status codes

/*!
  \brief No error, method executed successfully.
*/
#define RADIOLIB_ERR_NONE                                      (0)

/*!
  \brief There was an unexpected, unknown error. If you see this, something went incredibly wrong.
  Your Arduino may be possessed, contact your local exorcist to resolve this error.
*/
#define RADIOLIB_ERR_UNKNOWN                                   (-1)

// SX127x/RFM9x status codes

/*!
  \brief Radio chip was not found during initialization. This can be caused by specifying wrong chip type in the constructor
  (i.e. calling SX1272 constructor for SX1278 chip) or by a fault in your wiring (incorrect slave select pin).
*/
#define RADIOLIB_ERR_CHIP_NOT_FOUND                            (-2)

/*!
  \brief Failed to allocate memory for temporary buffer. This can be cause by not enough RAM or by passing invalid pointer.
*/
#define RADIOLIB_ERR_MEMORY_ALLOCATION_FAILED                  (-3)

/*!
  \brief Packet supplied to transmission method was longer than limit.
*/
#define RADIOLIB_ERR_PACKET_TOO_LONG                           (-4)

/*!
  \brief Timed out waiting for transmission finish.
*/
#define RADIOLIB_ERR_TX_TIMEOUT                                (-5)

/*!
  \brief Timed out waiting for incoming transmission.
*/
#define RADIOLIB_ERR_RX_TIMEOUT                                (-6)

/*!
  \brief The calculated and expected CRCs of received packet do not match.
  This means that the packet was damaged during transmission and should be sent again.
*/
#define RADIOLIB_ERR_CRC_MISMATCH                              (-7)

/*!
  \brief The supplied bandwidth value is invalid for this module.
*/
#define RADIOLIB_ERR_INVALID_BANDWIDTH                         (-8)

/*!
  \brief The supplied spreading factor value is invalid for this module.
*/
#define RADIOLIB_ERR_INVALID_SPREADING_FACTOR                  (-9)

/*!
  \brief The supplied coding rate value is invalid for this module.
*/
#define RADIOLIB_ERR_INVALID_CODING_RATE                       (-10)

/*!
  \brief Internal only.
*/
#define RADIOLIB_ERR_INVALID_BIT_RANGE                         (-11)

/*!
  \brief The supplied frequency value is invalid for this module.
*/
#define RADIOLIB_ERR_INVALID_FREQUENCY                         (-12)

/*!
  \brief The supplied output power value is invalid for this module.
*/
#define RADIOLIB_ERR_INVALID_OUTPUT_POWER                      (-13)

/*!
  \brief LoRa preamble was detected during channel activity detection.
  This means that there is some LoRa device currently transmitting in your channel.
*/
#define RADIOLIB_PREAMBLE_DETECTED                             (-14)

/*!
  \brief No LoRa preambles were detected during channel activity detection. Your channel is free.
*/
#define RADIOLIB_CHANNEL_FREE                                  (-15)

/*!
  \brief Real value in SPI register does not match the expected one. This can be caused by faulty SPI wiring.
*/
#define RADIOLIB_ERR_SPI_WRITE_FAILED                          (-16)

/*!
  \brief The supplied current limit value is invalid.
*/
#define RADIOLIB_ERR_INVALID_CURRENT_LIMIT                     (-17)

/*!
  \brief The supplied preamble length is invalid.
*/
#define RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH                   (-18)

/*!
  \brief The supplied gain value is invalid.
*/
#define RADIOLIB_ERR_INVALID_GAIN                              (-19)

/*!
  \brief User tried to execute modem-exclusive method on a wrong modem.
  For example, this can happen when you try to change LoRa configuration when FSK modem is active.
*/
#define RADIOLIB_ERR_WRONG_MODEM                               (-20)

/*!
  \brief The supplied number of RSSI samples is invalid.
*/
#define RADIOLIB_ERR_INVALID_NUM_SAMPLES                       (-21)

/*!
  \brief The supplied RSSI offset is invalid.
*/
#define RADIOLIB_ERR_INVALID_RSSI_OFFSET                       (-22)

/*!
  \brief The supplied encoding is invalid.
*/
#define RADIOLIB_ERR_INVALID_ENCODING                          (-23)

/*!
  \brief LoRa packet header has been damaged.
*/
#define RADIOLIB_ERR_LORA_HEADER_DAMAGED                       (-24)

/*!
  \brief The requested functionality is not supported for this device
*/
#define RADIOLIB_ERR_UNSUPPORTED                               (-25)

/*!
  \brief The specified DIO pin does not exist on this device
*/
#define RADIOLIB_ERR_INVALID_DIO_PIN                           (-26)

/*!
  \brief The supplied RSSI threshold is invalid.
*/
#define RADIOLIB_ERR_INVALID_RSSI_THRESHOLD                    (-27)

/*!
  \brief A `NULL` pointer has been encountered. If you see this, there may be a potential security vulnerability.
*/
#define RADIOLIB_ERR_NULL_POINTER                              (-28)

// RF69-specific status codes

/*!
  \brief The supplied bit rate value is invalid.
*/
#define RADIOLIB_ERR_INVALID_BIT_RATE                          (-101)

/*!
  \brief The supplied frequency deviation value is invalid.
*/
#define RADIOLIB_ERR_INVALID_FREQUENCY_DEVIATION               (-102)

/*!
  \brief The supplied bit rate to bandwidth ratio is invalid. See the module datasheet for more information.
*/
#define RADIOLIB_ERR_INVALID_BIT_RATE_BW_RATIO                 (-103)

/*!
  \brief The supplied receiver bandwidth value is invalid.
*/
#define RADIOLIB_ERR_INVALID_RX_BANDWIDTH                      (-104)

/*!
  \brief The supplied FSK sync word is invalid.
*/
#define RADIOLIB_ERR_INVALID_SYNC_WORD                         (-105)

/*!
  \brief The supplied FSK data shaping option is invalid.
*/
#define RADIOLIB_ERR_INVALID_DATA_SHAPING                      (-106)

/*!
  \brief The current modulation is invalid for the requested operation.
*/
#define RADIOLIB_ERR_INVALID_MODULATION                        (-107)

/*!
  \brief Supplied Peak type is invalid.
*/
#define RADIOLIB_ERR_INVALID_OOK_RSSI_PEAK_TYPE                (-108)

// APRS status codes

/*!
  \brief Supplied APRS symbol is invalid.
*/
#define RADIOLIB_ERR_INVALID_SYMBOL                            (-201)

/*!
  \brief Mic-E Telemetry is invalid.
*/
#define RADIOLIB_ERR_INVALID_MIC_E_TELEMETRY                   (-202)

/*!
  \brief Mic-E Telemetry length is invalid (only 0, 2 or 5 is allowed).
*/
#define RADIOLIB_ERR_INVALID_MIC_E_TELEMETRY_LENGTH            (-203)

/*!
  \brief Mic-E message cannot contain both telemetry and status text.
*/
#define RADIOLIB_ERR_MIC_E_TELEMETRY_STATUS                    (-204)

// SSDV status codes

/*!
  \brief SSDV mode is invalid.
*/
#define RADIOLIB_ERR_INVALID_SSDV_MODE                         (-301)

/*!
  \brief Image size is invalid.
*/
#define RADIOLIB_ERR_INVALID_IMAGE_SIZE                        (-302)

/*!
  \brief Image quality is invalid.
*/
#define RADIOLIB_ERR_INVALID_IMAGE_QUALITY                     (-303)

/*!
  \brief Image subsampling is invalid.
*/
#define RADIOLIB_ERR_INVALID_SUBSAMPLING                       (-304)

// RTTY status codes

/*!
  \brief Supplied RTTY frequency shift is invalid for this module.
*/
#define RADIOLIB_ERR_INVALID_RTTY_SHIFT                        (-401)

/*!
  \brief Supplied RTTY encoding is invalid.
*/
#define RADIOLIB_ERR_UNSUPPORTED_ENCODING                      (-402)

// nRF24-specific status codes

/*!
  \brief Supplied data rate is invalid.
*/
#define RADIOLIB_ERR_INVALID_DATA_RATE                         (-501)

/*!
  \brief Supplied address width is invalid.
*/
#define RADIOLIB_ERR_INVALID_ADDRESS_WIDTH                     (-502)

/*!
  \brief Supplied data pipe number is invalid.
*/
#define RADIOLIB_ERR_INVALID_PIPE_NUMBER                       (-503)

/*!
  \brief ACK packet from destination module was not received within 15 retries.
*/
#define RADIOLIB_ERR_ACK_NOT_RECEIVED                          (-504)

// CC1101-specific status codes

/*!
  \brief Supplied number of broadcast addresses is invalid.
*/
#define RADIOLIB_ERR_INVALID_NUM_BROAD_ADDRS                   (-601)

// SX126x-specific status codes

/*!
  \brief Supplied CRC configuration is invalid.
*/
#define RADIOLIB_ERR_INVALID_CRC_CONFIGURATION                 (-701)

/*!
  \brief Detected LoRa transmission while scanning channel.
*/
#define RADIOLIB_LORA_DETECTED                                 (-702)

/*!
  \brief Supplied TCXO reference voltage is invalid.
*/
#define RADIOLIB_ERR_INVALID_TCXO_VOLTAGE                      (-703)

/*!
  \brief Bit rate / bandwidth / frequency deviation ratio is invalid. See SX126x datasheet for details.
*/
#define RADIOLIB_ERR_INVALID_MODULATION_PARAMETERS             (-704)

/*!
  \brief SX126x timed out while waiting for complete SPI command.
*/
#define RADIOLIB_ERR_SPI_CMD_TIMEOUT                           (-705)

/*!
  \brief SX126x received invalid SPI command.
*/
#define RADIOLIB_ERR_SPI_CMD_INVALID                           (-706)

/*!
  \brief SX126x failed to execute SPI command.
  Often this means that the module is trying to use TCXO while
  XTAL is connected (or vice versa). Make sure your crystal setup
  (e.g. TCXO reference voltage) matches your hardware by setting
  "tcxoVoltage" to 0 when using XTAL module, or to appropriate value
  when using TCXO module.
*/
#define RADIOLIB_ERR_SPI_CMD_FAILED                            (-707)

/*!
  \brief The supplied sleep period is invalid.

  The specified sleep period is shorter than the time necessary to sleep and wake the hardware
  including TCXO delay, or longer than the maximum possible
*/
#define RADIOLIB_ERR_INVALID_SLEEP_PERIOD                      (-708)

/*!
  \brief The supplied Rx period is invalid.

  The specified Rx period is shorter or longer than the hardware can handle.
*/
#define RADIOLIB_ERR_INVALID_RX_PERIOD                         (-709)

// AX.25-specific status codes

/*!
  \brief The provided callsign is invalid.

  The specified callsign is longer than 6 ASCII characters.
*/
#define RADIOLIB_ERR_INVALID_CALLSIGN                          (-801)

/*!
  \brief The provided repeater configuration is invalid.

  The specified number of repeaters does not match number of repeater IDs or their callsigns.
*/
#define RADIOLIB_ERR_INVALID_NUM_REPEATERS                     (-802)

/*!
  \brief One of the provided repeater callsigns is invalid.

  The specified callsign is longer than 6 ASCII characters.
*/
#define RADIOLIB_ERR_INVALID_REPEATER_CALLSIGN                 (-803)

// SX128x-specific status codes

/*!
  \brief Timed out waiting for ranging exchange finish.
*/
#define RADIOLIB_ERR_RANGING_TIMEOUT                           (-901)

// Pager-specific status codes

/*!
  \brief The provided payload data configuration is invalid.
*/
#define RADIOLIB_ERR_INVALID_PAYLOAD                            (-1001)

/*!
  \brief The requested address was not found in the received data.
*/
#define RADIOLIB_ERR_ADDRESS_NOT_FOUND                          (-1002)

/*!
  \brief The function code is invalid. 2 Bits only.
*/
#define RADIOLIB_ERR_INVALID_FUNCTION                           (-1003)

// LoRaWAN-specific status codes

/*!
  \brief Unable to restore existing LoRaWAN session because this node did not join any network yet.
*/
#define RADIOLIB_ERR_NETWORK_NOT_JOINED                         (-1101)

/*!
  \brief Malformed downlink packet received from network server.
*/
#define RADIOLIB_ERR_DOWNLINK_MALFORMED                         (-1102)

/*!
  \brief Network server requested switch to unsupported LoRaWAN revision.
*/
#define RADIOLIB_ERR_INVALID_REVISION                           (-1103)

/*!
  \brief Invalid LoRaWAN uplink port requested by user.
*/
#define RADIOLIB_ERR_INVALID_PORT                               (-1104)

/*!
  \brief User did not enable downlink in time.
*/
#define RADIOLIB_ERR_NO_RX_WINDOW                               (-1105)

/*!
  \brief No valid channel for the currently active LoRaWAN band was found.
*/
#define RADIOLIB_ERR_INVALID_CHANNEL                            (-1106)

/*!
  \brief Invalid LoRaWAN MAC command ID.
*/
#define RADIOLIB_ERR_INVALID_CID                                (-1107)

/*!
  \brief User requested to start uplink while still inside RX window or under dutycycle.
*/
#define RADIOLIB_ERR_UPLINK_UNAVAILABLE                         (-1108)

/*!
  \brief Unable to push new MAC command because the queue is full.
*/
#define RADIOLIB_ERR_COMMAND_QUEUE_FULL                         (-1109)

/*!
  \brief Unable to delete MAC command because it was not found in the queue.
*/
#define RADIOLIB_ERR_COMMAND_QUEUE_ITEM_NOT_FOUND               (-1110)

/*!
  \brief Unable to join network because JoinNonce is not higher than saved value.
*/
#define RADIOLIB_ERR_JOIN_NONCE_INVALID                         (-1111)

/*!
  \brief Received downlink Network frame counter is invalid (lower than last heard value).
*/
#define RADIOLIB_ERR_N_FCNT_DOWN_INVALID                        (-1112)

/*!
  \brief Received downlink Application frame counter is invalid (lower than last heard value).
*/
#define RADIOLIB_ERR_A_FCNT_DOWN_INVALID                        (-1113)

/*!
  \brief Uplink payload length at this datarate exceeds the active dwell time limitations.
*/
#define RADIOLIB_ERR_DWELL_TIME_EXCEEDED                        (-1114)

/*!
  \brief The buffer integrity check did not match the supplied checksum value.
*/
#define RADIOLIB_ERR_CHECKSUM_MISMATCH                          (-1115)

/*!
  \brief No downlink was received - most likely none was sent from the server.
*/
#define RADIOLIB_LORAWAN_NO_DOWNLINK                            (-1116)

#define RADIOLIB_ASSERT(STATEVAR) { if((STATEVAR) != RADIOLIB_ERR_NONE) { return(STATEVAR); } }
#define RADIOLIB_CHECK_RANGE(VAR, MIN, MAX, ERR) { if(!(((VAR) >= (MIN)) && ((VAR) <= (MAX)))) { return(ERR); } }

#define GpioLevelLow LOW
#define GpioLevelHigh HIGH

enum OpMode_t {
      /*!
        \brief End of table marker, use \ref END_OF_MODE_TABLE constant instead.
        Value is zero to ensure zero-initialized mode ends the table.
      */
      MODE_END_OF_TABLE = 0,

      /*! \brief Idle mode */
      MODE_IDLE,

      /*! \brief Receive mode */
      MODE_RX,

      /*! \brief Transmission mode */
      MODE_TX,
    };


// module
MODULE_PART void mod_setRfSwitchState(uint8_t mode);
MODULE_PART int16_t mod_SPIgetRegValue(uint16_t reg, uint8_t msb, uint8_t lsb);
MODULE_PART int16_t mod_SPIsetRegValue(uint16_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval, uint8_t checkMask);
MODULE_PART void mod_SPIreadRegisterBurst(uint16_t reg, size_t numBytes, uint8_t* inBytes);
MODULE_PART uint8_t mod_SPIreadRegister(uint16_t reg);
MODULE_PART void mod_SPIwriteRegister(uint16_t reg, uint8_t data);
MODULE_PART void mod_SPIwriteRegisterBurst(uint16_t reg, uint8_t* data, size_t numBytes);
MODULE_PART void mod_setRfSwitchPins(uint32_t rxEn, uint32_t txEn);
MODULE_PART void mod_setRfSwitchTable(const uint32_t (&pins)[3], const RfSwitchMode_t table[]);
MODULE_PART void mod_SPItransfer(uint8_t cmd, uint16_t reg, uint8_t* dataOut, uint8_t* dataIn, size_t numBytes);

void mod_setRfSwitchState(uint8_t mode) {
  SETREGS
  const RfSwitchMode_t *row = mod_findRfSwitchMode(mode);
  if(!row) {
    // RF switch control is disabled or does not have this mode
    return;
  }

  // set pins
  const uint32_t *value = &row->values[0];
  for (size_t i = 0; i < RFSWITCH_MAX_PINS; i++) {
    uint32_t pin = this->rfSwitchPins[i];
    if (pin != RADIOLIB_NC)
      digitalWrite(pin, *value);
    ++value;
  }
}

int16_t mod_SPIgetRegValue(uint16_t reg, uint8_t msb, uint8_t lsb) {
  SETREGS
  if((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return(RADIOLIB_ERR_INVALID_BIT_RANGE);
  }

  uint8_t rawValue = mod_SPIreadRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return(maskedValue);
}

int16_t mod_SPIsetRegValue(uint16_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval, uint8_t checkMask) {
  SETREGS
  if((msb > 7) || (lsb > 7) || (lsb > msb)) {
    return(RADIOLIB_ERR_INVALID_BIT_RANGE);
  }

  uint8_t currentValue = mod_SPIreadRegister(reg);
  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
  uint8_t newValue = (currentValue & ~mask) | (value & mask);
  mod_SPIwriteRegister(reg, newValue);

  #if RADIOLIB_SPI_PARANOID
    // check register value each millisecond until check interval is reached
    // some registers need a bit of time to process the change (e.g. SX127X_REG_OP_MODE)
    uint32_t start = this->hal->micros();
    uint8_t readValue = 0x00;
    while(this->hal->micros() - start < (checkInterval * 1000)) {
      readValue = mod_SPIreadRegister(reg);
      if((readValue & checkMask) == (newValue & checkMask)) {
        // check passed, we can stop the loop
        return(RADIOLIB_ERR_NONE);
      }
    }

    // check failed, print debug info
    RADIOLIB_DEBUG_SPI_PRINTLN();
    RADIOLIB_DEBUG_SPI_PRINTLN("address:\t0x%X", reg);
    RADIOLIB_DEBUG_SPI_PRINTLN("bits:\t\t%d %d", msb, lsb);
    RADIOLIB_DEBUG_SPI_PRINTLN("value:\t\t0x%X", value);
    RADIOLIB_DEBUG_SPI_PRINTLN("current:\t0x%X", currentValue);
    RADIOLIB_DEBUG_SPI_PRINTLN("mask:\t\t0x%X", mask);
    RADIOLIB_DEBUG_SPI_PRINTLN("new:\t\t0x%X", newValue);
    RADIOLIB_DEBUG_SPI_PRINTLN("read:\t\t0x%X", readValue);

    return(RADIOLIB_ERR_SPI_WRITE_FAILED);
  #else
    return(RADIOLIB_ERR_NONE);
  #endif
}


void mod_SPIreadRegisterBurst(uint16_t reg, size_t numBytes, uint8_t* inBytes) {
  SETREGS
  if(!SPIstreamType) {
    mod_SPItransfer(SPIreadCommand, reg, NULL, inBytes, numBytes);
  } else {
    uint8_t cmd[] = { SPIreadCommand, (uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF) };
    mod_SPItransferStream(cmd, 3, false, NULL, inBytes, numBytes, true, RADIOLIB_MODULE_SPI_TIMEOUT);
  }
}

uint8_t mod_SPIreadRegister(uint16_t reg) {
  SETREGS
  uint8_t resp = 0;
  if(!SPIstreamType) {
    mod_SPItransfer(SPIreadCommand, reg, NULL, &resp, 1);
  } else {
    uint8_t cmd[] = { SPIreadCommand, (uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF) };
    mod_SPItransferStream(cmd, 3, false, NULL, &resp, 1, true, RADIOLIB_MODULE_SPI_TIMEOUT);
  }
  return(resp);
}

void mod_SPIwriteRegisterBurst(uint16_t reg, uint8_t* data, size_t numBytes) {
  SETREGS
  if(!SPIstreamType) {
    mod_SPItransfer(SPIwriteCommand, reg, data, NULL, numBytes);
  } else {
    uint8_t cmd[] = { SPIwriteCommand, (uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF) };
    mod_SPItransferStream(cmd, 3, true, data, NULL, numBytes, true, RADIOLIB_MODULE_SPI_TIMEOUT);
  }
}

void mod_SPIwriteRegister(uint16_t reg, uint8_t data) {
  SETREGS
  if(!SPIstreamType) {
    mod_SPItransfer(SPIwriteCommand, reg, &data, NULL, 1);
  } else {
    uint8_t cmd[] = { SPIwriteCommand, (uint8_t)((reg >> 8) & 0xFF), (uint8_t)(reg & 0xFF) };
    mod_SPItransferStream(cmd, 3, true, &data, NULL, 1, true, RADIOLIB_MODULE_SPI_TIMEOUT);
  }
}

void mod_setRfSwitchPins(uint32_t rxEn, uint32_t txEn) {
  SETREGS
  // This can be on the stack, setRfSwitchTable copies the contents
  const uint32_t pins[] = {
    rxEn, txEn, RADIOLIB_NC,
  };
  
  // This must be static, since setRfSwitchTable stores a reference.
  static const RfSwitchMode_t table[] = {
    { MODE_IDLE,  {this->hal->GpioLevelLow,  this->hal->GpioLevelLow} },
    { MODE_RX,    {this->hal->GpioLevelHigh, this->hal->GpioLevelLow} },
    { MODE_TX,    {this->hal->GpioLevelLow,  this->hal->GpioLevelHigh} },
    END_OF_MODE_TABLE,
  };
  mod_setRfSwitchTable(pins, table);
}

void mod_setRfSwitchTable(const uint32_t (&pins)[3], const RfSwitchMode_t table[]) {
  SETREGS
  memcpy(this->rfSwitchPins, pins, sizeof(this->rfSwitchPins));
  this->rfSwitchTable = table;
  for(size_t i = 0; i < RFSWITCH_MAX_PINS; i++)
    pinMode(pins[i], this->hal->GpioModeOutput);
}

void mod_SPItransfer(uint8_t cmd, uint16_t reg, uint8_t* dataOut, uint8_t* dataIn, size_t numBytes) {
  SETREGS
  // prepare the buffers
  size_t buffLen = this->SPIaddrWidth/8 + numBytes;
  uint8_t buffOut[RADIOLIB_STATIC_ARRAY_SIZE];
  uint8_t buffIn[RADIOLIB_STATIC_ARRAY_SIZE];
  uint8_t* buffOutPtr = buffOut;

  // copy the command
  if (this->SPIaddrWidth <= 8) {
    *(buffOutPtr++) = reg | cmd;
  } else {
    *(buffOutPtr++) = (reg >> 8) | cmd;
    *(buffOutPtr++) = reg & 0xFF;
  }

  // copy the data
  if (cmd == SPIwriteCommand) {
    memcpy(buffOutPtr, dataOut, numBytes);
  } else {
    memset(buffOutPtr, this->SPInopCommand, numBytes);
  }

  // do the transfer
  spiBeginTransaction();
  digitalWrite(this->csPin, GpioLevelLow);
  spiTransfer(buffOut, buffLen, buffIn);
  digitalWrite(this->csPin, GpioLevelHigh);
  spiEndTransaction();
  
  // copy the data
  if (cmd == SPIreadCommand) {
    memcpy(dataIn, &buffIn[this->SPIaddrWidth/8], numBytes);
  }

  // print debug information
  #if RADIOLIB_DEBUG_SPI
    uint8_t* debugBuffPtr = NULL;
    if(cmd == SPIwriteCommand) {
      RADIOLIB_DEBUG_SPI_PRINT("W\t%X\t", reg);
      debugBuffPtr = &buffOut[this->SPIaddrWidth/8];
    } else if(cmd == SPIreadCommand) {
      RADIOLIB_DEBUG_SPI_PRINT("R\t%X\t", reg);
      debugBuffPtr = &buffIn[this->SPIaddrWidth/8];
    }
    for(size_t n = 0; n < numBytes; n++) {
      RADIOLIB_DEBUG_SPI_PRINT_NOTAG("%X\t", debugBuffPtr[n]);
    }
    RADIOLIB_DEBUG_SPI_PRINTLN_NOTAG();
  #endif

}



 MODULE_PART  CC1101_CC1101(Module* module);
 MODULE_PART  int16_t CC1101_begin(float freq = 434.0, float br = 4.8,
                float freqDev = 5.0, float rxBw = 58.0,
                int8_t pwr = 10, uint8_t preambleLength = 16);
 MODULE_PART  void CC1101_reset();
 MODULE_PART  int16_t CC1101_transmit(uint8_t* data, size_t len, uint8_t addr = 0) override;
 MODULE_PART  int16_t CC1101_receive(uint8_t* data, size_t len) override;
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_standby() override;
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_standby(uint8_t mode) override;
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_transmitDirect(uint32_t frf = 0) override;
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_receiveDirect() override;
 MODULE_PART  int16_t CC1101_transmitDirectAsync(uint32_t frf = 0);
 MODULE_PART  int16_t CC1101_receiveDirectAsync();
 MODULE_PART  int16_t CC1101_packetMode();
 MODULE_PART  void CC1101_setGdo0Action(void (*func)(void), uint32_t dir);
 MODULE_PART  void CC1101_clearGdo0Action();
 MODULE_PART  void CC1101_setGdo2Action(void (*func)(void), uint32_t dir);
 MODULE_PART  void CC1101_clearGdo2Action();
 MODULE_PART  void CC1101_setPacketReceivedAction(void (*func)(void));
 MODULE_PART  void CC1101_clearPacketReceivedAction();
 MODULE_PART  void CC1101_setPacketSentAction(void (*func)(void));
 MODULE_PART  void CC1101_clearPacketSentAction();
 MODULE_PART  int16_t CC1101_startTransmit(uint8_t* data, size_t len, uint8_t addr = 0) override;
 MODULE_PART  int16_t CC1101_finishTransmit() override;
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_startReceive();
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_startReceive(uint32_t timeout, uint16_t irqFlags, uint16_t irqMask, size_t len);
 MODULE_PART  int16_t CC1101_readData(uint8_t* data, size_t len) override;
 MODULE_PART  int16_t CC1101_setFrequency(float freq);
 MODULE_PART  int16_t CC1101_setBitRate(float br);
 MODULE_PART  int16_t CC1101_setRxBandwidth(float rxBw);
 MODULE_PART  int16_t CC1101_autoSetRxBandwidth();
 MODULE_PART  int16_t CC1101_setFrequencyDeviation(float freqDev) override;
 MODULE_PART  int16_t CC1101_getFrequencyDeviation(float* freqDev);
 MODULE_PART  int16_t CC1101_setOutputPower(int8_t pwr);
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_setSyncWord(uint8_t syncH, uint8_t syncL, uint8_t maxErrBits = 0, bool requireCarrierSense = false);
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_setSyncWord(uint8_t* syncWord, uint8_t len, uint8_t maxErrBits = 0, bool requireCarrierSense = false);
 MODULE_PART  int16_t CC1101_setPreambleLength(uint8_t preambleLength, uint8_t qualityThreshold);
 MODULE_PART  int16_t CC1101_setNodeAddress(uint8_t nodeAddr, uint8_t numBroadcastAddrs = 0);
 MODULE_PART  int16_t CC1101_disableAddressFiltering();
 MODULE_PART  int16_t CC1101_setOOK(bool enableOOK);
 MODULE_PART  float CC1101_getRSSI();
 MODULE_PART  uint8_t CC1101_getLQI() const;
 MODULE_PART  size_t CC1101_getPacketLength(bool update = true) override;
 MODULE_PART  int16_t CC1101_fixedPacketLengthMode(uint8_t len = 63);
 MODULE_PART  int16_t CC1101_variablePacketLengthMode(uint8_t maxLen = 63);
 MODULE_PART  int16_t CC1101_enableSyncWordFiltering(uint8_t maxErrBits = 0, bool requireCarrierSense = false);
 MODULE_PART  int16_t CC1101_disableSyncWordFiltering(bool requireCarrierSense = false);
 MODULE_PART  int16_t CC1101_setCrcFiltering(bool enable = true);
 MODULE_PART  int16_t CC1101_setPromiscuousMode(bool enable = true, bool requireCarrierSense = false);
 MODULE_PART  bool CC1101_getPromiscuousMode();
 MODULE_PART  int16_t CC1101_setDataShaping(uint8_t sh) override;
 MODULE_PART  int16_t CC1101_setEncoding(uint8_t encoding) override;
 MODULE_PART  void CC1101_setRfSwitchPins(uint32_t rxEn, uint32_t txEn);
 MODULE_PART  void CC1101_setRfSwitchTable(const uint32_t (&pins)[RFSWITCH_MAX_PINS], const RfSwitchMode_t table[]);
 MODULE_PART  uint8_t CC1101_randomByte();
 MODULE_PART  int16_t CC1101_getChipVersion();
 MODULE_PART  void CC1101_setDirectAction(void (*func)(void));
 MODULE_PART  void CC1101_readBit(uint32_t pin);
 MODULE_PART  int16_t CC1101_setDIOMapping(uint32_t pin, uint32_t value);
 protected:
 MODULE_PART  Module* CC1101_getMod();
 MODULE_PART  int16_t CC1101_SPIgetRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);
 MODULE_PART  int16_t CC1101_SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0, uint8_t checkInterval = 2);
 MODULE_PART  void CC1101_SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes);
 MODULE_PART  uint8_t CC1101_SPIreadRegister(uint8_t reg);
 MODULE_PART  void CC1101_SPIwriteRegisterBurst(uint8_t reg, uint8_t* data, size_t len);
 MODULE_PART  void CC1101_SPIwriteRegister(uint8_t reg, uint8_t data);
 MODULE_PART  void CC1101_SPIsendCommand(uint8_t cmd);
 


 MODULE_PART  int16_t CC1101_config();
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_transmitDirect(bool sync, uint32_t frf);
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_receiveDirect(bool sync);
 MODULE_PART  int16_t CC1101_directMode(bool sync);
 MODULE_PART  static void CC1101_getExpMant(float target, uint16_t mantOffset, uint8_t divExp, uint8_t expMax, uint8_t& exp, uint8_t& mant);
 MODULE_PART  int16_t CC1101_setPacketMode(uint8_t mode, uint16_t len);


int16_t CC1101_begin(float freq, float br, float freqDev, float rxBw, int8_t pwr, uint8_t preambleLength) {
  SETREGS
  this->mod.SPIreadCommand = 0b10000000;
  this->mod.SPIwriteCommand = 0b00000000;
  //mod_init();
  pinModethis->irqPin, GpioModeInput);
  uint8_t i = 0;
  bool flagFound = false;
  while ((i < 10) && !flagFound) {
    int16_t version = CC1101_getChipVersion();
    if ((version == 0x14) || (version == 0x04) ||
        (version == 0x17)) {
      flagFound = true;
    } else {
      RADIOLIB_DEBUG_BASIC_PRINTLN("CC1101 not found! (%d of 10 tries) RADIOLIB_CC1101_REG_VERSION == 0x%04X, expected 0x0004/0x0014",
                                   i + 1, version);
      delay(10);
      i++;
    }
  }
  if (!flagFound) {
    RADIOLIB_DEBUG_BASIC_PRINTLN("No CC1101 found!");
    //mod_term();
    return (RADIOLIB_ERR_CHIP_NOT_FOUND);
  } else {
    RADIOLIB_DEBUG_BASIC_PRINTLN("M\tCC1101");
  }
  int16_t state = CC1101_config();
  RADIOLIB_ASSERT(state);
  state = CC1101_setFrequency(freq);
  RADIOLIB_ASSERT(state);
  state = CC1101_setBitRate(br);
  RADIOLIB_ASSERT(state);
  state = CC1101_setRxBandwidth(rxBw);
  RADIOLIB_ASSERT(state);
  state = CC1101_setFrequencyDeviation(freqDev);
  RADIOLIB_ASSERT(state);
  state = CC1101_setOutputPower(pwr);
  RADIOLIB_ASSERT(state);
  state = CC1101_variablePacketLengthMode();
  RADIOLIB_ASSERT(state);
  state = CC1101_setPreambleLength(preambleLength, preambleLength - 4);
  RADIOLIB_ASSERT(state);
  state = CC1101_setDataShaping(RADIOLIB_SHAPING_NONE);
  RADIOLIB_ASSERT(state);
  state = CC1101_setEncoding(RADIOLIB_ENCODING_NRZ);
  RADIOLIB_ASSERT(state);
  uint8_t sw[2] = { 0x12, 0xAD };
  state = CC1101_CC1101_setSyncWord(sw[0], sw[1], 0, false);
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x3A);
  CC1101_SPIsendCommand(0x3B);
  return (state);
}

void CC1101_reset() {
  SETREGS
  digitalWrite(this->csPin, GpioLevelLow);
  delayMicroseconds(5);
  digitalWrite(this->csPin, GpioLevelHigh);
  delayMicroseconds(40);
  digitalWrite(this->csPin, GpioLevelLow);
  delay(10);
  CC1101_SPIsendCommand(0x30);
}
int16_t CC1101_transmit(uint8_t* data, size_t len, uint8_t addr) {
  SETREGS
  uint32_t timeout = 5 + (uint32_t)((((float)(len * 8)) / mem->bitRate) * 5);
  int16_t state = CC1101_startTransmit(data, len, addr);
  RADIOLIB_ASSERT(state);
  uint32_t start = millis();
  while (!digitalRead(this->gpioPin)) {
    yield();
    if (millis() - start > timeout) {
      CC1101_finishTransmit();
      return (RADIOLIB_ERR_TX_TIMEOUT);
    }
  }
  start = millis();
  while (digitalRead(this->gpioPin)) {
    yield();
    if (millis() - start > timeout) {
      CC1101_finishTransmit();
      return (RADIOLIB_ERR_TX_TIMEOUT);
    }
  }
  return (CC1101_finishTransmit());
}
int16_t CC1101_receive(uint8_t* data, size_t len) {
  SETREGS
  uint32_t timeout = 500 + (1.0 / (mem->bitRate)) * (63 * 400.0);
  int16_t state = CC1101_CC1101_startReceive();
  RADIOLIB_ASSERT(state);
  uint32_t start = millis();
  while (digitalRead(this->irqPin)) {
    yield();
    if (millis() - start > timeout) {
      CC1101_CC1101_standby();
      CC1101_SPIsendCommand(0x3A);
      return (RADIOLIB_ERR_RX_TIMEOUT);
    }
  }
  start = millis();
  while (!digitalRead(this->irqPin)) {
    yield();
    if (millis() - start > timeout) {
      CC1101_CC1101_standby();
      CC1101_SPIsendCommand(0x3A);
      return (RADIOLIB_ERR_RX_TIMEOUT);
    }
  }
  return (CC1101_readData(data, len));
}
int16_t CC1101_CC1101_standby() {
  SETREGS
  CC1101_SPIsendCommand(0x36);
  uint32_t start = millis();
  while (CC1101_SPIgetRegValue(0x35, 4, 0) != 0x01) {
    mod->hal->yield();
    if (millis() - start > 100) {
      return (RADIOLIB_ERR_UNKNOWN);
    }
  };
  mod_setRfSwitchState(MODE_IDLE);
  return (RADIOLIB_ERR_NONE);
}
int16_t CC1101_CC1101_standby(uint8_t mode) {
  SETREGS
  (void)mode;
  return (CC1101_CC1101_standby());
}
int16_t CC1101_CC1101_transmitDirect(uint32_t frf) { return CC1101_CC1101_transmitDirect(true, frf); }
int16_t CC1101_transmitDirectAsync(uint32_t frf) { return CC1101_CC1101_transmitDirect(false, frf); }
int16_t CC1101_CC1101_transmitDirect(bool sync, uint32_t frf) {
  mod_setRfSwitchState(MODE_TX);
  if (frf != 0) {
    CC1101_SPIwriteRegister(0x0D, (frf & 0xFF0000) >> 16);
    CC1101_SPIwriteRegister(0x0E, (frf & 0x00FF00) >> 8);
    CC1101_SPIwriteRegister(0x0F, frf & 0x0000FF);
    CC1101_SPIsendCommand(0x35);
    return (RADIOLIB_ERR_NONE);
  }
  int16_t state = CC1101_directMode(sync);
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x35);
  return (state);
}
int16_t CC1101_CC1101_receiveDirect() { 
  SETREGS
  return CC1101_CC1101_receiveDirect(true);
}
int16_t CC1101_receiveDirectAsync() {
  SETREGS
  return CC1101_CC1101_receiveDirect(false); }
}
int16_t CC1101_CC1101_receiveDirect(bool sync) {
  SETREGS
  mod_setRfSwitchState(MODE_RX);
  int16_t state = CC1101_directMode(sync);
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x34);
  return (RADIOLIB_ERR_NONE);
}
int16_t CC1101_packetMode() {
  SETREGS
  int16_t state = CC1101_SPIsetRegValue(0x07,
                                 0b00000000 | 0b00000100 | 0b00000000, 3, 0);
  state |= CC1101_SPIsetRegValue(0x08, 0b00000000 | 0b00000000, 6, 4);
  state |= CC1101_SPIsetRegValue(0x08, 0b00000100 | mem->packetLengthConfig, 2, 0);
  return (state);
}
void CC1101_setGdo0Action(void (*func)(void), uint32_t dir) {
  SETREGS
  attachInterrupt(pinToInterrupt(this->irqPin), func, dir);
}
void CC1101_clearGdo0Action() { 
  SETREGS
  detachInterrupt(pinToInterrupt(this->irqPin)); 
}
void CC1101_setPacketReceivedAction(void (*func)(void)) { 
  SETREGS
  mem->CC1101_setGdo0Action(func, GpioInterruptRising);
}
void CC1101_clearPacketReceivedAction() {
  SETREGS
  mem->CC1101_clearGdo0Action();
}
void CC1101_setPacketSentAction(void (*func)(void)) {
  SETREGS
  mem->CC1101_setGdo2Action(func, GpioInterruptFalling);
}
void CC1101_clearPacketSentAction() {
  SETREGS
  mem->CC1101_clearGdo2Action();
}
void CC1101_setGdo2Action(void (*func)(void), uint32_t dir) {
  SETREGS
  if (this->gpioPin == RADIOLIB_NC) {
    return;
  }
  pinMode(this->gpioPin, GpioModeInput);
  attachInterrupt(pinToInterrupt(this->gpioPin, func, dir);
}
void CC1101_clearGdo2Action() {
  SETREGS
  if (this->gpioPin == RADIOLIB_NC) {
    return;
  }
  detachInterrupt(pinToInterrupt(this->gpioPin));
}
int16_t CC1101_startTransmit(uint8_t* data, size_t len, uint8_t addr) {
  SETREGS
  if (len > 63) {
    return (RADIOLIB_ERR_PACKET_TOO_LONG);
  }
  CC1101_CC1101_standby();
  CC1101_SPIsendCommand(0x3B);
  int16_t state = CC1101_SPIsetRegValue(0x00, 0x06, 5, 0);
  RADIOLIB_ASSERT(state);
  if (mem->packetLengthConfig == 0b00000001) {
    CC1101_SPIwriteRegister(0x3F, len);
  }
  uint8_t filter = CC1101_SPIgetRegValue(0x07, 1, 0);
  if (filter != 0b00000000) {
    CC1101_SPIwriteRegister(0x3F, addr);
  }
  CC1101_SPIwriteRegisterBurst(0x3F, data, len);
  mod_setRfSwitchState(MODE_TX);
  CC1101_SPIsendCommand(0x35);
  return (state);
}
int16_t CC1101_finishTransmit() {
  SETREGS
  int16_t state = CC1101_CC1101_standby();
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x3B);
  return (state);
}
int16_t CC1101_CC1101_startReceive() {
  SETREGS
  int16_t state = CC1101_CC1101_standby();
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x3A);
  state = CC1101_SPIsetRegValue(0x02, 0b01000000 | 0x06, 6, 0);
  RADIOLIB_ASSERT(state);
  mod_setRfSwitchState(MODE_RX);
  CC1101_SPIsendCommand(0x34);
  return (state);
}
int16_t CC1101_CC1101_startReceive(uint32_t timeout, uint16_t irqFlags, uint16_t irqMask, size_t len) {
  SETREGS
  (void)timeout;
  (void)irqFlags;
  (void)irqMask;
  (void)len;
  return (CC1101_CC1101_startReceive());
}
int16_t CC1101_readData(uint8_t* data, size_t len) {
  SETREGS
  size_t length = CC1101_getPacketLength();
  if ((len != 0) && (len < length)) {
    length = len;
  }
  uint8_t filter = CC1101_SPIgetRegValue(0x07, 1, 0);
  if (filter != 0b00000000) {
    CC1101_SPIreadRegister(0x3F);
  }
  CC1101_SPIreadRegisterBurst(0x3F, length, data);
  bool isAppendStatus = CC1101_SPIgetRegValue(0x07, 2, 2) == 0b00000100;
  int16_t state = RADIOLIB_ERR_NONE;
  if (isAppendStatus) {
    mem->rawRSSI = CC1101_SPIgetRegValue(0x3F);
    uint8_t val = CC1101_SPIgetRegValue(0x3F);
    mem->rawLQI = val & 0x7F;
    if (mem->crcOn && (val & 0b10000000) == 0b00000000) {
      mem->packetLengthQueried = false;
      state = RADIOLIB_ERR_CRC_MISMATCH;
    }
  }
  mem->packetLengthQueried = false;
  if (CC1101_SPIgetRegValue(0x17, 3, 2) == 0b00000000) {
    CC1101_CC1101_standby();
    CC1101_SPIsendCommand(0x3A);
  }
  return (state);
}
int16_t CC1101_setFrequency(float freq) {
  SETREGS
  if (!(((freq > 300.0) && (freq < 348.0)) || ((freq > 387.0) && (freq < 464.0)) || ((freq > 779.0) && (freq < 928.0)))) {
    return (RADIOLIB_ERR_INVALID_FREQUENCY);
  }
  CC1101_SPIsendCommand(0x36);
  uint32_t base = 1;
  uint32_t FRF = (freq * (base << 16)) / 26.0;
  int16_t state = CC1101_SPIsetRegValue(0x0D, (FRF & 0xFF0000) >> 16, 7, 0);
  state |= CC1101_SPIsetRegValue(0x0E, (FRF & 0x00FF00) >> 8, 7, 0);
  state |= CC1101_SPIsetRegValue(0x0F, FRF & 0x0000FF, 7, 0);
  if (state == RADIOLIB_ERR_NONE) {
    mem->frequency = freq;
  }
  return (CC1101_setOutputPower(mem->power));
}
int16_t CC1101_setBitRate(float br) {
  SETREGS
  RADIOLIB_CHECK_RANGE(br, 0.025, 600.0, RADIOLIB_ERR_INVALID_BIT_RATE);
  CC1101_SPIsendCommand(0x36);
  uint8_t e = 0;
  uint8_t m = 0;
  CC1101_getExpMant(br * 1000.0, 256, 28, 14, e, m);
  int16_t state = CC1101_SPIsetRegValue(0x10, e, 3, 0);
  state |= CC1101_SPIsetRegValue(0x11, m);
  if (state == RADIOLIB_ERR_NONE) {
    mem->bitRate = br;
  }
  return (state);
}
int16_t CC1101_setRxBandwidth(float rxBw) {
  SETREGS
  RADIOLIB_CHECK_RANGE(rxBw, 58.0, 812.0, RADIOLIB_ERR_INVALID_RX_BANDWIDTH);
  CC1101_SPIsendCommand(0x36);
  for (int8_t e = 3; e >= 0; e--) {
    for (int8_t m = 3; m >= 0; m--) {
      float point = (26.0 * 1000000.0) / (8 * (m + 4) * ((uint32_t)1 << e));
      if (fabs((rxBw * 1000.0) - point) <= 1000) {
        return (CC1101_SPIsetRegValue(0x10, (e << 6) | (m << 4), 7, 4));
      }
    }
  }
  return (RADIOLIB_ERR_INVALID_RX_BANDWIDTH);
}
int16_t CC1101_autoSetRxBandwidth() {
  SETREGS
  float uncertainty = ((mem->frequency) * 40 * 2);
  uncertainty = (uncertainty / 1000);
  float minbw = ((mem->bitRate) + uncertainty);
  int possibles[16] = {58, 68, 81, 102, 116, 135, 162, 203, 232, 270, 325, 406, 464, 541, 650, 812};
  for (int i = 0; i < 16; i++) {
    if (possibles[i] > minbw) {
      int16_t state = CC1101_setRxBandwidth(possibles[i]);
      return (state);
    }
  }
  return (RADIOLIB_ERR_UNKNOWN);
}
int16_t CC1101_setFrequencyDeviation(float freqDev) {
  SETREGS
  float newFreqDev = freqDev;
  if (freqDev < 0.0) {
    newFreqDev = 1.587;
  }
  if (freqDev != 0) {
    RADIOLIB_CHECK_RANGE(newFreqDev, 1.587, 380.8, RADIOLIB_ERR_INVALID_FREQUENCY_DEVIATION);
  }
  CC1101_SPIsendCommand(0x36);
  uint8_t e = 0;
  uint8_t m = 0;
  CC1101_getExpMant(newFreqDev * 1000.0, 8, 17, 7, e, m);
  int16_t state = CC1101_SPIsetRegValue(0x15, (e << 4), 6, 4);
  state |= CC1101_SPIsetRegValue(0x15, m, 2, 0);
  return (state);
}
int16_t CC1101_getFrequencyDeviation(float* freqDev) {
  SETREGS
  if (freqDev == __null) {
    return (RADIOLIB_ERR_NULL_POINTER);
  }
  if (this->modulation == 0b00110000) {
    *freqDev = 0.0;
    return (RADIOLIB_ERR_NONE);
  }
  uint8_t e = (uint8_t)(CC1101_SPIgetRegValue(0x15, 6, 4) >> 4);
  uint8_t m = (uint8_t)CC1101_SPIgetRegValue(0x15, 2, 0);
  *freqDev = (1000.0 / (uint32_t(1) << 17)) - (8 + m) * (uint32_t(1) << e);
  return (RADIOLIB_ERR_NONE);
}
int16_t CC1101_setOutputPower(int8_t pwr) {
  SETREGS
  uint8_t f;
  if (mem->frequency < 374.0) {
    f = 0;
  } else if (mem->frequency < 650.5) {
    f = 1;
  } else if (mem->frequency < 891.5) {
    f = 2;
  } else {
    f = 3;
  }
  uint8_t paTable[8][4] = {{0x12, 0x12, 0x03, 0x03}, {0x0D, 0x0E, 0x0F, 0x0E}, {0x1C, 0x1D, 0x1E, 0x1E}, {0x34, 0x34, 0x27, 0x27},
                           {0x51, 0x60, 0x50, 0x8E}, {0x85, 0x84, 0x81, 0xCD}, {0xCB, 0xC8, 0xCB, 0xC7}, {0xC2, 0xC0, 0xC2, 0xC0}};
  uint8_t powerRaw;
  switch (pwr) {
    case -30:
      powerRaw = paTable[0][f];
      break;
    case -20:
      powerRaw = paTable[1][f];
      break;
    case -15:
      powerRaw = paTable[2][f];
      break;
    case -10:
      powerRaw = paTable[3][f];
      break;
    case 0:
      powerRaw = paTable[4][f];
      break;
    case 5:
      powerRaw = paTable[5][f];
      break;
    case 7:
      powerRaw = paTable[6][f];
      break;
    case 10:
      powerRaw = paTable[7][f];
      break;
    default:
      return (RADIOLIB_ERR_INVALID_OUTPUT_POWER);
  }
  mem->power = pwr;
  if (this->modulation == 0b00110000) {
    uint8_t paValues[2] = {0x00, powerRaw};
    CC1101_SPIwriteRegisterBurst(0x3E, paValues, 2);
    return (RADIOLIB_ERR_NONE);
  } else {
    return (CC1101_SPIsetRegValue(0x3E, powerRaw));
  }
}
int16_t CC1101_CC1101_setSyncWord(uint8_t* syncWord, uint8_t len, uint8_t maxErrBits, bool requireCarrierSense) {
  SETREGS
  if ((maxErrBits > 1) || (len != 2)) {
    return (RADIOLIB_ERR_INVALID_SYNC_WORD);
  }
  for (uint8_t i = 0; i < len; i++) {
    if (syncWord[i] == 0x00) {
      return (RADIOLIB_ERR_INVALID_SYNC_WORD);
    }
  }
  int16_t state = CC1101_enableSyncWordFiltering(maxErrBits, requireCarrierSense);
  RADIOLIB_ASSERT(state);
  state = CC1101_SPIsetRegValue(0x04, syncWord[0]);
  state |= CC1101_SPIsetRegValue(0x05, syncWord[1]);
  return (state);
}
int16_t CC1101_CC1101_setSyncWord(uint8_t syncH, uint8_t syncL, uint8_t maxErrBits, bool requireCarrierSense) {
  SETREGS
  uint8_t syncWord[] = {syncH, syncL};
  return (CC1101_CC1101_setSyncWord(syncWord, sizeof(syncWord), maxErrBits, requireCarrierSense));
}
int16_t CC1101_setPreambleLength(uint8_t preambleLength, uint8_t qualityThreshold) {
  SETREGS
  uint8_t value;
  switch (preambleLength) {
    case 16:
      value = 0b00000000;
      break;
    case 24:
      value = 0b00010000;
      break;
    case 32:
      value = 0b00100000;
      break;
    case 48:
      value = 0b00110000;
      break;
    case 64:
      value = 0b01000000;
      break;
    case 96:
      value = 0b01010000;
      break;
    case 128:
      value = 0b01100000;
      break;
    case 192:
      value = 0b01110000;
      break;
    default:
      return (RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH);
  }
  uint8_t pqt = qualityThreshold / 4;
  if (pqt > 7) {
    pqt = 7;
  }
  int16_t state = CC1101_SPIsetRegValue(0x07, pqt << 5, 7, 5);
  state |= CC1101_SPIsetRegValue(0x13, value, 6, 4);
  return (state);
}
int16_t CC1101_setNodeAddress(uint8_t nodeAddr, uint8_t numBroadcastAddrs) {
  SETREGS
  RADIOLIB_CHECK_RANGE(numBroadcastAddrs, 1, 2, RADIOLIB_ERR_INVALID_NUM_BROAD_ADDRS);
  int16_t state = CC1101_SPIsetRegValue(0x07, numBroadcastAddrs + 0x01, 1, 0);
  RADIOLIB_ASSERT(state);
  return (CC1101_SPIsetRegValue(0x09, nodeAddr));
}
int16_t CC1101_disableAddressFiltering() {
  SETREGS
  int16_t state = CC1101_SPIsetRegValue(0x07, 0b00000000, 1, 0);
  RADIOLIB_ASSERT(state);
  return (CC1101_SPIsetRegValue(0x09, 0x00));
}
int16_t CC1101_setOOK(bool enableOOK) {
  SETREGS
  if (enableOOK) {
    int16_t state = CC1101_SPIsetRegValue(0x12, 0b00110000, 6, 4);
    RADIOLIB_ASSERT(state);
    state = CC1101_SPIsetRegValue(0x22, 1, 2, 0);
    RADIOLIB_ASSERT(state);
    this->modulation = 0b00110000;
  } else {
    int16_t state = CC1101_SPIsetRegValue(0x12, 0b00000000, 6, 4);
    RADIOLIB_ASSERT(state);
    state = CC1101_SPIsetRegValue(0x22, 0, 2, 0);
    RADIOLIB_ASSERT(state);
    this->modulation = 0b00000000;
  }
  return (CC1101_setOutputPower(mem->power));
}
float CC1101_getRSSI() {
  SETREGS
  float rssi;
  if (mem->directModeEnabled) {
    if (mem->rawRSSI >= 128) {
      rssi = (((float)mem->rawRSSI - 256.0) / 2.0) - 74.0;
    } else {
      rssi = (((float)mem->rawRSSI) / 2.0) - 74.0;
    }
  } else {
    uint8_t rawRssi = CC1101_SPIreadRegister(0x34);
    if (rawRssi >= 128) {
      rssi = ((rawRssi - 256) / 2) - 74;
    } else {
      rssi = (rawRssi / 2) - 74;
    }
  }
  return (rssi);
}
uint8_t CC1101_getLQI() const {
  SETREGS
  return (mem->rawLQI);
}
size_t CC1101_getPacketLength(bool update) {
  SETREGS
  if (!mem->packetLengthQueried && update) {
    if (mem->packetLengthConfig == 0b00000001) {
      mem->packetLength = CC1101_SPIreadRegister(0x3F);
    } else {
      mem->packetLength = CC1101_SPIreadRegister(0x06);
    }
    mem->packetLengthQueried = true;
  }
  return (mem->packetLength);
}
int16_t CC1101_fixedPacketLengthMode(uint8_t len) {
  SETREGS
  if (len == 0) {
    int16_t state = CC1101_SPIsetRegValue(0x08, 0b00000010, 1, 0);
    RADIOLIB_ASSERT(state);
  }
  return (CC1101_setPacketMode(0b00000000, len));
}
int16_t CC1101_variablePacketLengthMode(uint8_t maxLen) {
  SETREGS
  return (CC1101_setPacketMode(0b00000001, maxLen));
}
int16_t CC1101_enableSyncWordFiltering(uint8_t maxErrBits, bool requireCarrierSense) {
  SETREGS
  int16_t state = RADIOLIB_ERR_NONE;
  switch (maxErrBits) {
    case 0:
      state |= CC1101_SPIsetRegValue(0x12,
                              (requireCarrierSense ? 0b00000110 : 0b00000010), 2, 0);
      break;
    case 1:
      state |= CC1101_SPIsetRegValue(0x12,
                              (requireCarrierSense ? 0b00000101 : 0b00000001), 2, 0);
      break;
    default:
      state = RADIOLIB_ERR_INVALID_SYNC_WORD;
      break;
  }
  return (state);
}
int16_t CC1101_disableSyncWordFiltering(bool requireCarrierSense) {
  SETREGS
  int16_t state = CC1101_SPIsetRegValue(0x12,
                                 (requireCarrierSense ? 0b00000100 : 0b00000000), 2, 0);
  return (state);
}
int16_t CC1101_setCrcFiltering(bool enable) {
  SETREGS
  mem->crcOn = enable;
  if (mem->crcOn == true) {
    return (CC1101_SPIsetRegValue(0x08, 0b00000100, 2, 2));
  } else {
    return (CC1101_SPIsetRegValue(0x08, 0b00000000, 2, 2));
  }
}
int16_t CC1101_setPromiscuousMode(bool enable, bool requireCarrierSense) {
  SETREGS
  int16_t state = RADIOLIB_ERR_NONE;
  if (mem->promiscuous == enable) {
    return (state);
  }
  if (enable) {
    state = CC1101_setPreambleLength(16, 0);
    RADIOLIB_ASSERT(state);
    state = CC1101_disableSyncWordFiltering(requireCarrierSense);
    RADIOLIB_ASSERT(state);
    state = CC1101_setCrcFiltering(false);
  } else {
    state = CC1101_setPreambleLength(16, 16 / 4);
    RADIOLIB_ASSERT(state);
    state = CC1101_enableSyncWordFiltering();
    RADIOLIB_ASSERT(state);
    state = CC1101_setCrcFiltering(true);
  }
  mem->promiscuous = enable;
  return (state);
}
bool CC1101_getPromiscuousMode() {
  SETREGS
  return (mem->promiscuous);
}
int16_t CC1101_setDataShaping(uint8_t sh) {
  SETREGS
  int16_t state = CC1101_CC1101_standby();
  RADIOLIB_ASSERT(state);
  switch (sh) {
    case RADIOLIB_SHAPING_NONE:
      state = CC1101_SPIsetRegValue(0x12, 0b00000000, 6, 4);
      break;
    case RADIOLIB_SHAPING_0_5:
      state = CC1101_SPIsetRegValue(0x12, 0b00010000, 6, 4);
      break;
    default:
      return (RADIOLIB_ERR_INVALID_DATA_SHAPING);
  }
  return (state);
}
int16_t CC1101_setEncoding(uint8_t encoding) {
  SETREGS
  int16_t state = CC1101_CC1101_standby();
  RADIOLIB_ASSERT(state);
  switch (encoding) {
    case RADIOLIB_ENCODING_NRZ:
      state = CC1101_SPIsetRegValue(0x12, 0b00000000, 3, 3);
      RADIOLIB_ASSERT(state);
      return (CC1101_SPIsetRegValue(0x08, 0b00000000, 6, 6));
    case RADIOLIB_ENCODING_MANCHESTER:
      state = CC1101_SPIsetRegValue(0x12, 0b00001000, 3, 3);
      RADIOLIB_ASSERT(state);
      return (CC1101_SPIsetRegValue(0x08, 0b00000000, 6, 6));
    case RADIOLIB_ENCODING_WHITENING:
      state = CC1101_SPIsetRegValue(0x12, 0b00000000, 3, 3);
      RADIOLIB_ASSERT(state);
      return (CC1101_SPIsetRegValue(0x08, 0b01000000, 6, 6));
    default:
      return (RADIOLIB_ERR_INVALID_ENCODING);
  }
}
void CC1101_setRfSwitchPins(uint32_t rxEn, uint32_t txEn) {
  SETREGS
  mod_CC1101_setRfSwitchPins(rxEn, txEn);
}
void CC1101_setRfSwitchTable(const uint32_t (&pins)[RFSWITCH_MAX_PINS], const RfSwitchMode_t table[]) {
  SETREGS
  mod_CC1101_setRfSwitchTable(pins, table);
}
uint8_t CC1101_randomByte() {
  SETREGS
  CC1101_SPIsendCommand(0x34);
  delay(10);
  uint8_t randByte = 0x00;
  for (uint8_t i = 0; i < 8; i++) {
    randByte |= ((CC1101_SPIreadRegister(0x34) & 0x01) << i);
  }
  CC1101_SPIsendCommand(0x36);
  return (randByte);
}
int16_t CC1101_getChipVersion() {
  SETREGS
  return (CC1101_SPIgetRegValue(0x31));
}
void CC1101_setDirectAction(void (*func)(void)) {
  SETREGS
  CC1101_setGdo0Action(func, GpioInterruptRising);
}
void CC1101_readBit(uint32_t pin) {
  SETREGS
  updateDirectBuffer((uint8_t)digitalRead(pin));
}
int16_t CC1101_setDIOMapping(uint32_t pin, uint32_t value) {
  SETREGS
  if (pin > 2) {
    return (RADIOLIB_ERR_INVALID_DIO_PIN);
  }
  return (CC1101_SPIsetRegValue(0x02 - pin, value));
}
int16_t CC1101_config() {
  SETREGS
  CC1101_reset();
  delay(150);
  CC1101_CC1101_standby();
  int16_t state = CC1101_SPIsetRegValue(0x18, 0b00010000, 5, 4);
  state |= CC1101_SPIsetRegValue(0x18, 0b00000000, 1, 1);
  RADIOLIB_ASSERT(state);
  state = CC1101_SPIsetRegValue(0x02, 0x2E, 5, 0);
  state |= CC1101_SPIsetRegValue(0x00, 0x2E, 5, 0);
  RADIOLIB_ASSERT(state);
  state = CC1101_packetMode();
  return (state);
}
int16_t CC1101_directMode(bool sync) {
  SETREGS
  CC1101_SPIsendCommand(0x36);
  int16_t state = 0;
  mem->directModeEnabled = sync;
  if (sync) {
    state |= CC1101_SPIsetRegValue(0x02, 0x0B, 5, 0);
    state |= CC1101_SPIsetRegValue(0x00, 0x0C, 5, 0);
    state |= CC1101_SPIsetRegValue(0x08, 0b00010000, 5, 4);
  } else {
    state |= CC1101_SPIsetRegValue(0x02, 0x0D, 5, 0);
    state |= CC1101_SPIsetRegValue(0x08, 0b00110000, 5, 4);
  }
  state |= CC1101_SPIsetRegValue(0x08, 0b00000010, 1, 0);
  return (state);
}
void CC1101_getExpMant(float target, uint16_t mantOffset, uint8_t divExp, uint8_t expMax, uint8_t& exp, uint8_t& mant) {
  SETREGS
  float origin = (mantOffset * 26.0 * 1000000.0) / ((uint32_t)1 << divExp);
  for (int8_t e = expMax; e >= 0; e--) {
    float intervalStart = ((uint32_t)1 << e) * origin;
    if (target >= intervalStart) {
      exp = e;
      float stepSize = intervalStart / (float)mantOffset;
      mant = ((target - intervalStart) / stepSize);
      return;
    }
  }
}
int16_t CC1101_setPacketMode(uint8_t mode, uint16_t len) {
  SETREGS
  if (len > 63) {
    return (RADIOLIB_ERR_PACKET_TOO_LONG);
  }
  int16_t state = CC1101_SPIsetRegValue(0x08, mode, 1, 0);
  RADIOLIB_ASSERT(state);
  state = CC1101_SPIsetRegValue(0x06, len);
  RADIOLIB_ASSERT(state);
  mem->packetLength = len;
  mem->packetLengthConfig = mode;
  return (state);
}
Module* CC1101_getMod() {
  SETREGS
  return (this->mod);
}
int16_t CC1101_SPIgetRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) {
  SETREGS
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (mod_CC1101_SPIgetRegValue(reg, msb, lsb));
}
int16_t CC1101_SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval) {
  SETREGS
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (mod_CC1101_SPIsetRegValue(reg, value, msb, lsb, checkInterval));
}
void CC1101_SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
  SETREGS
  mod_CC1101_SPIreadRegisterBurst(reg | 0b01000000, numBytes, inBytes);
}
uint8_t CC1101_SPIreadRegister(uint8_t reg) {
  SETREGS
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (mod_CC1101_SPIreadRegister(reg));
}
void CC1101_SPIwriteRegister(uint8_t reg, uint8_t data) {
  SETREGS
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (mod_CC1101_SPIwriteRegister(reg, data));
}
void CC1101_SPIwriteRegisterBurst(uint8_t reg, uint8_t* data, size_t len) {
  SETREGS
  mod_CC1101_SPIwriteRegisterBurst(reg | 0b01000000, data, len);
}
void CC1101_SPIsendCommand(uint8_t cmd) {
  SETREGS
  digitalWrite(this->csPin, GpioLevelLow);
  spiBeginTransaction();
  uint8_t status = 0;
  spiTransfer(&cmd, 1, &status);
  spiEndTransaction();
  digitalWrite(this->csPin, GpioLevelHigh);
  RADIOLIB_DEBUG_SPI_PRINTLN("CMD\tW\t%02X\t%02X", cmd, status);
  (void)status;
}
