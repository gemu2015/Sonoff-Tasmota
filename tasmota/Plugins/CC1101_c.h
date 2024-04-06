CC1101 : public PhysicalLayer {
 
  using PhysicalLayer::readData;
  using PhysicalLayer::receive;
  using PhysicalLayer::startTransmit;
  using PhysicalLayer::transmit;
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
 MODULE_PART  void CC1101_setRfSwitchTable(const uint32_t (&pins)[Module::RFSWITCH_MAX_PINS], const Module::RfSwitchMode_t table[]);
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
 
  Module* mod;
  float frequency = 434.0;
  float bitRate = 4.8;
  uint8_t rawRSSI = 0;
  uint8_t rawLQI = 0;
  uint8_t modulation = 0b00000000;
  size_t packetLength = 0;
  bool packetLengthQueried = false;
  uint8_t packetLengthConfig = 0b00000001;
  bool promiscuous = false;
  bool crcOn = true;
  bool directModeEnabled = true;
  int8_t power = 10;
 MODULE_PART  int16_t CC1101_config();
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_transmitDirect(bool sync, uint32_t frf);
 MODULE_PART MODULE_PART  int16_t CC1101_CC1101_receiveDirect(bool sync);
 MODULE_PART  int16_t CC1101_directMode(bool sync);
 MODULE_PART  static void CC1101_getExpMant(float target, uint16_t mantOffset, uint8_t divExp, uint8_t expMax, uint8_t& exp, uint8_t& mant);
 MODULE_PART  int16_t CC1101_setPacketMode(uint8_t mode, uint16_t len);
};
CC1101_CC1101(Module* module) : PhysicalLayer(396.7285156, 63) {
  this->mod = module;
}
int16_t CC1101_begin(float freq, float br, float freqDev, float rxBw, int8_t pwr, uint8_t preambleLength) {
  this->mod->SPIreadCommand = 0b10000000;
  this->mod->SPIwriteCommand = 0b00000000;
  this->mod->init();
  pinMode(this->mod->getIrq(), GpioModeInput);
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
    this->mod->term();
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
  digitalWrite(this->mod->getCs(), GpioLevelLow);
  delayMicroseconds(5);
  digitalWrite(this->mod->getCs(), GpioLevelHigh);
  delayMicroseconds(40);
  digitalWrite(this->mod->getCs(), GpioLevelLow);
  delay(10);
  CC1101_SPIsendCommand(0x30);
}
int16_t CC1101_transmit(uint8_t* data, size_t len, uint8_t addr) {
  uint32_t timeout = 5 + (uint32_t)((((float)(len * 8)) / this->bitRate) * 5);
  int16_t state = CC1101_startTransmit(data, len, addr);
  RADIOLIB_ASSERT(state);
  uint32_t start = millis();
  while (!digitalRead(this->mod->getGpio())) {
    yield();
    if (millis() - start > timeout) {
      CC1101_finishTransmit();
      return (RADIOLIB_ERR_TX_TIMEOUT);
    }
  }
  start = millis();
  while (digitalRead(this->mod->getGpio())) {
    yield();
    if (millis() - start > timeout) {
      CC1101_finishTransmit();
      return (RADIOLIB_ERR_TX_TIMEOUT);
    }
  }
  return (CC1101_finishTransmit());
}
int16_t CC1101_receive(uint8_t* data, size_t len) {
  uint32_t timeout = 500 + (1.0 / (this->bitRate)) * (63 * 400.0);
  int16_t state = CC1101_CC1101_startReceive();
  RADIOLIB_ASSERT(state);
  uint32_t start = millis();
  while (digitalRead(this->mod->getIrq())) {
    yield();
    if (millis() - start > timeout) {
      CC1101_CC1101_standby();
      CC1101_SPIsendCommand(0x3A);
      return (RADIOLIB_ERR_RX_TIMEOUT);
    }
  }
  start = millis();
  while (!digitalRead(this->mod->getIrq())) {
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
  CC1101_SPIsendCommand(0x36);
  uint32_t start = millis();
  while (CC1101_SPIgetRegValue(0x35, 4, 0) != 0x01) {
    mod->hal->yield();
    if (millis() - start > 100) {
      return (RADIOLIB_ERR_UNKNOWN);
    }
  };
  this->mod->setRfSwitchState(Module::MODE_IDLE);
  return (RADIOLIB_ERR_NONE);
}
int16_t CC1101_CC1101_standby(uint8_t mode) {
  (void)mode;
  return (CC1101_CC1101_standby());
}
int16_t CC1101_CC1101_transmitDirect(uint32_t frf) { return CC1101_CC1101_transmitDirect(true, frf); }
int16_t CC1101_transmitDirectAsync(uint32_t frf) { return CC1101_CC1101_transmitDirect(false, frf); }
int16_t CC1101_CC1101_transmitDirect(bool sync, uint32_t frf) {
  this->mod->setRfSwitchState(Module::MODE_TX);
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
int16_t CC1101_CC1101_receiveDirect() { return CC1101_CC1101_receiveDirect(true); }
int16_t CC1101_receiveDirectAsync() { return CC1101_CC1101_receiveDirect(false); }
int16_t CC1101_CC1101_receiveDirect(bool sync) {
  this->mod->setRfSwitchState(Module::MODE_RX);
  int16_t state = CC1101_directMode(sync);
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x34);
  return (RADIOLIB_ERR_NONE);
}
int16_t CC1101_packetMode() {
  int16_t state = CC1101_SPIsetRegValue(0x07,
                                 0b00000000 | 0b00000100 | 0b00000000, 3, 0);
  state |= CC1101_SPIsetRegValue(0x08, 0b00000000 | 0b00000000, 6, 4);
  state |= CC1101_SPIsetRegValue(0x08, 0b00000100 | this->packetLengthConfig, 2, 0);
  return (state);
}
void CC1101_setGdo0Action(void (*func)(void), uint32_t dir) {
  attachInterrupt(pinToInterrupt(this->mod->getIrq()), func, dir);
}
void CC1101_clearGdo0Action() { detachInterrupt(pinToInterrupt(this->mod->getIrq())); }
void CC1101_setPacketReceivedAction(void (*func)(void)) { this->CC1101_setGdo0Action(func, GpioInterruptRising); }
void CC1101_clearPacketReceivedAction() { this->CC1101_clearGdo0Action(); }
void CC1101_setPacketSentAction(void (*func)(void)) { this->CC1101_setGdo2Action(func, GpioInterruptFalling); }
void CC1101_clearPacketSentAction() { this->CC1101_clearGdo2Action(); }
void CC1101_setGdo2Action(void (*func)(void), uint32_t dir) {
  if (this->mod->getGpio() == RADIOLIB_NC) {
    return;
  }
  pinMode(this->mod->getGpio(), GpioModeInput);
  attachInterrupt(pinToInterrupt(this->mod->getGpio()), func, dir);
}
void CC1101_clearGdo2Action() {
  if (this->mod->getGpio() == RADIOLIB_NC) {
    return;
  }
  detachInterrupt(pinToInterrupt(this->mod->getGpio()));
}
int16_t CC1101_startTransmit(uint8_t* data, size_t len, uint8_t addr) {
  if (len > 63) {
    return (RADIOLIB_ERR_PACKET_TOO_LONG);
  }
  CC1101_CC1101_standby();
  CC1101_SPIsendCommand(0x3B);
  int16_t state = CC1101_SPIsetRegValue(0x00, 0x06, 5, 0);
  RADIOLIB_ASSERT(state);
  if (this->packetLengthConfig == 0b00000001) {
    CC1101_SPIwriteRegister(0x3F, len);
  }
  uint8_t filter = CC1101_SPIgetRegValue(0x07, 1, 0);
  if (filter != 0b00000000) {
    CC1101_SPIwriteRegister(0x3F, addr);
  }
  CC1101_SPIwriteRegisterBurst(0x3F, data, len);
  this->mod->setRfSwitchState(Module::MODE_TX);
  CC1101_SPIsendCommand(0x35);
  return (state);
}
int16_t CC1101_finishTransmit() {
  int16_t state = CC1101_CC1101_standby();
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x3B);
  return (state);
}
int16_t CC1101_CC1101_startReceive() {
  int16_t state = CC1101_CC1101_standby();
  RADIOLIB_ASSERT(state);
  CC1101_SPIsendCommand(0x3A);
  state = CC1101_SPIsetRegValue(0x02, 0b01000000 | 0x06, 6, 0);
  RADIOLIB_ASSERT(state);
  this->mod->setRfSwitchState(Module::MODE_RX);
  CC1101_SPIsendCommand(0x34);
  return (state);
}
int16_t CC1101_CC1101_startReceive(uint32_t timeout, uint16_t irqFlags, uint16_t irqMask, size_t len) {
  (void)timeout;
  (void)irqFlags;
  (void)irqMask;
  (void)len;
  return (CC1101_CC1101_startReceive());
}
int16_t CC1101_readData(uint8_t* data, size_t len) {
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
    this->rawRSSI = CC1101_SPIgetRegValue(0x3F);
    uint8_t val = CC1101_SPIgetRegValue(0x3F);
    this->rawLQI = val & 0x7F;
    if (this->crcOn && (val & 0b10000000) == 0b00000000) {
      this->packetLengthQueried = false;
      state = RADIOLIB_ERR_CRC_MISMATCH;
    }
  }
  this->packetLengthQueried = false;
  if (CC1101_SPIgetRegValue(0x17, 3, 2) == 0b00000000) {
    CC1101_CC1101_standby();
    CC1101_SPIsendCommand(0x3A);
  }
  return (state);
}
int16_t CC1101_setFrequency(float freq) {
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
    this->frequency = freq;
  }
  return (CC1101_setOutputPower(this->power));
}
int16_t CC1101_setBitRate(float br) {
  RADIOLIB_CHECK_RANGE(br, 0.025, 600.0, RADIOLIB_ERR_INVALID_BIT_RATE);
  CC1101_SPIsendCommand(0x36);
  uint8_t e = 0;
  uint8_t m = 0;
  CC1101_getExpMant(br * 1000.0, 256, 28, 14, e, m);
  int16_t state = CC1101_SPIsetRegValue(0x10, e, 3, 0);
  state |= CC1101_SPIsetRegValue(0x11, m);
  if (state == RADIOLIB_ERR_NONE) {
    this->bitRate = br;
  }
  return (state);
}
int16_t CC1101_setRxBandwidth(float rxBw) {
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
  float uncertainty = ((this->frequency) * 40 * 2);
  uncertainty = (uncertainty / 1000);
  float minbw = ((this->bitRate) + uncertainty);
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
  uint8_t f;
  if (this->frequency < 374.0) {
    f = 0;
  } else if (this->frequency < 650.5) {
    f = 1;
  } else if (this->frequency < 891.5) {
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
  this->power = pwr;
  if (this->modulation == 0b00110000) {
    uint8_t paValues[2] = {0x00, powerRaw};
    CC1101_SPIwriteRegisterBurst(0x3E, paValues, 2);
    return (RADIOLIB_ERR_NONE);
  } else {
    return (CC1101_SPIsetRegValue(0x3E, powerRaw));
  }
}
int16_t CC1101_CC1101_setSyncWord(uint8_t* syncWord, uint8_t len, uint8_t maxErrBits, bool requireCarrierSense) {
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
  uint8_t syncWord[] = {syncH, syncL};
  return (CC1101_CC1101_setSyncWord(syncWord, sizeof(syncWord), maxErrBits, requireCarrierSense));
}
int16_t CC1101_setPreambleLength(uint8_t preambleLength, uint8_t qualityThreshold) {
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
  RADIOLIB_CHECK_RANGE(numBroadcastAddrs, 1, 2, RADIOLIB_ERR_INVALID_NUM_BROAD_ADDRS);
  int16_t state = CC1101_SPIsetRegValue(0x07, numBroadcastAddrs + 0x01, 1, 0);
  RADIOLIB_ASSERT(state);
  return (CC1101_SPIsetRegValue(0x09, nodeAddr));
}
int16_t CC1101_disableAddressFiltering() {
  int16_t state = CC1101_SPIsetRegValue(0x07, 0b00000000, 1, 0);
  RADIOLIB_ASSERT(state);
  return (CC1101_SPIsetRegValue(0x09, 0x00));
}
int16_t CC1101_setOOK(bool enableOOK) {
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
  return (CC1101_setOutputPower(this->power));
}
float CC1101_getRSSI() {
  float rssi;
  if (this->directModeEnabled) {
    if (this->rawRSSI >= 128) {
      rssi = (((float)this->rawRSSI - 256.0) / 2.0) - 74.0;
    } else {
      rssi = (((float)this->rawRSSI) / 2.0) - 74.0;
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
uint8_t CC1101_getLQI() const { return (this->rawLQI); }
size_t CC1101_getPacketLength(bool update) {
  if (!this->packetLengthQueried && update) {
    if (this->packetLengthConfig == 0b00000001) {
      this->packetLength = CC1101_SPIreadRegister(0x3F);
    } else {
      this->packetLength = CC1101_SPIreadRegister(0x06);
    }
    this->packetLengthQueried = true;
  }
  return (this->packetLength);
}
int16_t CC1101_fixedPacketLengthMode(uint8_t len) {
  if (len == 0) {
    int16_t state = CC1101_SPIsetRegValue(0x08, 0b00000010, 1, 0);
    RADIOLIB_ASSERT(state);
  }
  return (CC1101_setPacketMode(0b00000000, len));
}
int16_t CC1101_variablePacketLengthMode(uint8_t maxLen) { return (CC1101_setPacketMode(0b00000001, maxLen)); }
int16_t CC1101_enableSyncWordFiltering(uint8_t maxErrBits, bool requireCarrierSense) {
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
  int16_t state = CC1101_SPIsetRegValue(0x12,
                                 (requireCarrierSense ? 0b00000100 : 0b00000000), 2, 0);
  return (state);
}
int16_t CC1101_setCrcFiltering(bool enable) {
  this->crcOn = enable;
  if (this->crcOn == true) {
    return (CC1101_SPIsetRegValue(0x08, 0b00000100, 2, 2));
  } else {
    return (CC1101_SPIsetRegValue(0x08, 0b00000000, 2, 2));
  }
}
int16_t CC1101_setPromiscuousMode(bool enable, bool requireCarrierSense) {
  int16_t state = RADIOLIB_ERR_NONE;
  if (this->promiscuous == enable) {
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
  this->promiscuous = enable;
  return (state);
}
bool CC1101_getPromiscuousMode() { return (this->promiscuous); }
int16_t CC1101_setDataShaping(uint8_t sh) {
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
void CC1101_setRfSwitchPins(uint32_t rxEn, uint32_t txEn) { this->mod->CC1101_setRfSwitchPins(rxEn, txEn); }
void CC1101_setRfSwitchTable(const uint32_t (&pins)[Module::RFSWITCH_MAX_PINS], const Module::RfSwitchMode_t table[]) {
  this->mod->CC1101_setRfSwitchTable(pins, table);
}
uint8_t CC1101_randomByte() {
  CC1101_SPIsendCommand(0x34);
  delay(10);
  uint8_t randByte = 0x00;
  for (uint8_t i = 0; i < 8; i++) {
    randByte |= ((CC1101_SPIreadRegister(0x34) & 0x01) << i);
  }
  CC1101_SPIsendCommand(0x36);
  return (randByte);
}
int16_t CC1101_getChipVersion() { return (CC1101_SPIgetRegValue(0x31)); }
void CC1101_setDirectAction(void (*func)(void)) { CC1101_setGdo0Action(func, GpioInterruptRising); }
void CC1101_readBit(uint32_t pin) { updateDirectBuffer((uint8_t)digitalRead(pin)); }
int16_t CC1101_setDIOMapping(uint32_t pin, uint32_t value) {
  if (pin > 2) {
    return (RADIOLIB_ERR_INVALID_DIO_PIN);
  }
  return (CC1101_SPIsetRegValue(0x02 - pin, value));
}
int16_t CC1101_config() {
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
  CC1101_SPIsendCommand(0x36);
  int16_t state = 0;
  this->directModeEnabled = sync;
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
  if (len > 63) {
    return (RADIOLIB_ERR_PACKET_TOO_LONG);
  }
  int16_t state = CC1101_SPIsetRegValue(0x08, mode, 1, 0);
  RADIOLIB_ASSERT(state);
  state = CC1101_SPIsetRegValue(0x06, len);
  RADIOLIB_ASSERT(state);
  this->packetLength = len;
  this->packetLengthConfig = mode;
  return (state);
}
Module* CC1101_getMod() { return (this->mod); }
int16_t CC1101_SPIgetRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) {
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (this->mod->CC1101_SPIgetRegValue(reg, msb, lsb));
}
int16_t CC1101_SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval) {
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (this->mod->CC1101_SPIsetRegValue(reg, value, msb, lsb, checkInterval));
}
void CC1101_SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t* inBytes) {
  this->mod->CC1101_SPIreadRegisterBurst(reg | 0b01000000, numBytes, inBytes);
}
uint8_t CC1101_SPIreadRegister(uint8_t reg) {
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (this->mod->CC1101_SPIreadRegister(reg));
}
void CC1101_SPIwriteRegister(uint8_t reg, uint8_t data) {
  if ((reg > 0x2E) && (reg < 0x3E)) {
    reg |= 0b01000000;
  }
  return (this->mod->CC1101_SPIwriteRegister(reg, data));
}
void CC1101_SPIwriteRegisterBurst(uint8_t reg, uint8_t* data, size_t len) {
  this->mod->CC1101_SPIwriteRegisterBurst(reg | 0b01000000, data, len);
}
void CC1101_SPIsendCommand(uint8_t cmd) {
  digitalWrite(this->mod->getCs(), GpioLevelLow);
  spiBeginTransaction();
  uint8_t status = 0;
  spiTransfer(&cmd, 1, &status);
  spiEndTransaction();
  digitalWrite(this->mod->getCs(), GpioLevelHigh);
  RADIOLIB_DEBUG_SPI_PRINTLN("CMD\tW\t%02X\t%02X", cmd, status);
  (void)status;
}
