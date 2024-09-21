// If possible disable interrupts whilst switching pin direction. Sadly
// there is no generic Arduino function to read the current interrupt
// status, only to enable and disable interrupts.  As a result the
// protection against spurious signals on the I2C bus is only available
// for AVR architectures where ATOMIC_BLOCK is defined.

#if 1

#define sdaLow digitalWrite(swv->sda, LOW);
#define sdaHigh pinMode(swv->sda, INPUT);

#define xsdaLow pinMode(swv->sda, OUTPUT);
#define xsdaHigh pinMode(swv->sda, INPUT);


#define sclLow digitalWrite(swv->scl, LOW);
#define sclHigh pinMode(swv->scl, INPUT);

#define xsclLow pinMode(swv->scl, OUTPUT);
#define xsclHigh pinMode(swv->scl, INPUT);

#define sdaRead digitalRead(swv->sda)
#define sclRead digitalRead(swv->scl)

#define WIRE_HAS_END 1
#define I2C_READ 1
#define I2C_WRITE 0
#define DELAY 4 // usec delay
#define I2C_MAXWAIT 2000


bool SWI2C_RepStart(uint8_t addr);
bool SWI2C_Start(uint8_t addr);
void SWI2C_Stop(void);
uint8_t SWI2C_requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop);
int SWI2C_Read(void);
bool SWI2C_i2cWrite(uint8_t value);
uint8_t SWI2C_i2cRead(bool last);

MODULE_PART void New_SWI2C(uint8_t sda, uint8_t scl) {
SETREGS
  swv = (SWI2C_VARS*)calloc(sizeof(SWI2C_VARS), 1);
  mt->mem_size += sizeof(SWI2C_VARS);
  swv->sda = sda;
  swv->scl = scl;
  swv->rxBufferIndex = 0;
  swv->rxBufferLength = I2C_BUFFER_LENGTH;
  swv->isTransmitting = false;
  swv->error = 0;

  sdaLow
  sclLow
  xsdaHigh
  xsclHigh

}

MODULE_PART void SWI2C_delete(void) {
  SETREGS
  if (swv) {
    free(swv);
  }
}

MODULE_PART void SWI2C_beginTransmission(uint8_t address) {
SETREGS
  if (swv->isTransmitting) {
    swv->error = (SWI2C_RepStart((address<<1)|I2C_WRITE) ? 0 : 2);
  } else {
    swv->error = (SWI2C_Start((address<<1)|I2C_WRITE) ? 0 : 2);
  }
    // indicate that we are isTransmitting
  swv->isTransmitting = 1;
}

MODULE_PART uint8_t SWI2C_endTransmission(uint8_t sendStop) {
SETREGS
  uint8_t transferError = swv->error;
  if (sendStop) {
    SWI2C_Stop();
    swv->isTransmitting = 0;
  }
  swv->error = 0;
  return transferError;
}

MODULE_PART uint8_t SWI2C_requestFromx(uint8_t address, uint8_t quantity, uint32_t iaddress, uint8_t isize, uint8_t sendStop) {
SETREGS
  swv->error = 0;
  uint8_t localerror = 0;
    if (isize > 0) {
      SWI2C_beginTransmission(address);
      // the maximum size of internal address is 3 bytes
      if (isize > 3){
        isize = 3;
      }
      // write internal register address - most significant byte first
      while (isize-- > 0) {
        SWI2C_i2cWrite((uint8_t)(iaddress >> (isize*8)));
      }
      SWI2C_endTransmission(false);
    }
    // clamp to buffer length
    if(quantity > I2C_BUFFER_LENGTH){
      quantity = I2C_BUFFER_LENGTH;
    }
    if (swv->isTransmitting) {
      localerror = !SWI2C_RepStart((address<<1) | I2C_READ);
    } else {
      localerror = !SWI2C_Start((address<<1) | I2C_READ);
    }
    if (swv->error == 0 && localerror) swv->error = 2;
    // perform blocking read into buffer
    for (uint8_t count=0; count < quantity; count++) {
      swv->rxBuffer[count] = SWI2C_i2cRead(count == quantity-1);
    }
    // set rx buffer iterator vars
    swv->rxBufferIndex = 0;
    swv->rxBufferLength = swv->error ? 0 : quantity;
    if (sendStop) {
      swv->isTransmitting = 0;
      SWI2C_Stop();
    }
    return swv->rxBufferLength;
}

MODULE_PART uint8_t SWI2C_requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop) {
  return SWI2C_requestFromx((uint8_t)address, (uint8_t)quantity, (uint32_t)0, (uint8_t)0, (uint8_t)sendStop);
}

MODULE_PART bool SWI2C_i2cWrite(uint8_t value) {
SETREGS
  for (uint8_t curr = 0X80; curr != 0; curr >>= 1) {
    if (curr & value) {
      xsdaHigh;
     } else {
      xsdaLow; 
     }
    xsclHigh;
    delayMicroseconds(DELAY);
    xsclLow;
  }

  xsdaHigh;
  xsclHigh;
  delayMicroseconds(DELAY/2);
  uint8_t ack = sdaRead;
  xsclLow;
  delayMicroseconds(DELAY/2);  
  xsdaLow;
  return ack == 0;
}

MODULE_PART uint8_t SWI2C_i2cRead(bool last) {
SETREGS
  uint8_t receivedByte  = 0;
  xsdaHigh;
  for (uint8_t i = 0; i < 8; i++) {
    receivedByte <<= 1;
    delayMicroseconds(DELAY);
    xsclHigh;
    if (sdaRead) receivedByte  |= 1;
    xsclLow;
  }
  if (last) {
    xsdaHigh;
  } else {
    xsdaLow;
  }
  xsclHigh;
  delayMicroseconds(DELAY/2);
  xsclLow;
  delayMicroseconds(DELAY/2);  
  xsdaLow;
  return receivedByte ;
}


MODULE_PART size_t SWI2C_Write(uint8_t data) {
SETREGS
  if (SWI2C_i2cWrite(data)) {
    return 1;
  } else {
    if (swv->error == 0) swv->error = 3;
    return 0;
  }
}

MODULE_PART bool SWI2C_Start(uint8_t addr) {
SETREGS
  xsdaLow;
  delayMicroseconds(DELAY);
  xsclLow;
  return SWI2C_i2cWrite(addr);
}

MODULE_PART bool SWI2C_StartWait(uint8_t addr) {
SETREGS
  long retry = I2C_MAXWAIT;
  while (!SWI2C_Start(addr)) {
    SWI2C_Stop();
    if (--retry == 0) return false;
  }
  return true;
}

MODULE_PART bool SWI2C_RepStart(uint8_t addr) {
SETREGS
  xsdaHigh;
  xsclHigh;
  delayMicroseconds(DELAY);
  return SWI2C_Start(addr);
}

MODULE_PART void SWI2C_Stop(void) {
SETREGS
  xsdaLow;
  delayMicroseconds(DELAY);
  xsclHigh;
  delayMicroseconds(DELAY);
  xsdaHigh;
  delayMicroseconds(DELAY);
}

MODULE_PART int SWI2C_Read(void) {
SETREGS
  int value = -1;
  if (swv->rxBufferIndex < swv->rxBufferLength){
    value = swv->rxBuffer[swv->rxBufferIndex];
    ++swv->rxBufferIndex;
  }
  return value;
}

MODULE_PART int SWI2C_peek(void) {
SETREGS
  int value = -1;

  if (swv->rxBufferIndex < swv->rxBufferLength){
    value = swv->rxBuffer[swv->rxBufferIndex];
  }
  return value;
}

#else


enum result_t {
  ack = 0,
  nack = 1,
  timedOut = 2,
};

enum i2c_mode_t {
  writeMode = 0,
  readMode = 1,
};

int32_t SWI2C_stop();
result_t SWI2C_Write(uint8_t data);
uint8_t SWI2C_endTransmissionInner(void);
result_t SWI2C_Start(uint8_t rawAddr);
result_t SWI2C_RepeatedStart(uint8_t rawAddr);
uint8_t SWI2C_Read(void);
bool SWI2C_sclHighAndStretch();
void SWI2C_begin(void);

/*
I2C_beginTransmission(addr);
I2C_write(a);
I2C_endTransmission(false);
I2C_requestFrom(addr, (size_t)3, true);
I2C_read();
*/


#define i2c_delay delayMicroseconds(swv->delay_us);

#define sdaLow digitalWrite(swv->sda, LOW);pinMode(swv->sda, OUTPUT);
#define sdaHigh pinMode(swv->sda, swv->inputMode);
#define sclLow digitalWrite(swv->scl, LOW);pinMode(swv->scl, OUTPUT);
#define sclHigh pinMode(swv->scl, swv->inputMode);
#define sdaRead digitalRead(swv->sda)
#define sclRead digitalRead(swv->scl)
#define Set_timeout swv->lastmillis = millis();
#define timeout_isExpired (millis()-swv->lastmillis) > swv->timeout_ms

#define default_delay_us 10;
#define default_timeout_ms 100;


MODULE_PART void New_SWI2C(uint8_t sda, uint8_t scl) {
SETREGS

  swv = (SWI2C_VARS*)calloc(sizeof(SWI2C_VARS), 1);

  swv->sda = sda;
  swv->scl = scl;
  swv->inputMode = INPUT; // Pullups disabled by default
  swv->delay_us = default_delay_us;
  swv->timeout_ms = default_timeout_ms;
  //swv->rxBuffer = nullptr;
  swv->rxBufferSize = 16;
  swv->rxBufferIndex = 0;
  swv->rxBufferBytesRead = 0;
  swv->txAddress = 8;  // First non-reserved address
  //swv->txBuffer = nullptr;
  swv->txBufferSize = 0;
  swv->txBufferIndex = 0;
  swv->transmissionInProgress = false;
  swv->allowClockStretch = false;

  mt->mem_size += sizeof(SWI2C_VARS);

  SWI2C_begin();
}

MODULE_PART void SWI2C_delete(void) {
  SETREGS
  if (swv) {
    free(swv);
  }
}

MODULE_PART void SWI2C_begin(void) {
  SWI2C_stop();
}


MODULE_PART int32_t SWI2C_stop() {
SETREGS
  //AsyncDelay swv->timeout(swv->timeout_ms, AsyncDelay::MILLIS);
  Set_timeout;
  swv->transmissionInProgress = false;

  // Force SCL low
  sclLow;
  i2c_delay;

  // Force SDA low
  sdaLow;
  i2c_delay;

  // Release SCL
  if (swv->allowClockStretch) {
    if (!SWI2C_sclHighAndStretch())
      return timedOut;
  } else {
    sclHigh;
  }
  i2c_delay;

  // Release SDA
  sdaHigh;
  i2c_delay;

  return ack;
}

MODULE_PART result_t SWI2C_Start(uint8_t rawAddr) {
SETREGS
  // Force SDA low
  sdaLow;
  i2c_delay;

  // Force SCL low
  sclLow;
  i2c_delay;
  return SWI2C_Write(rawAddr << 1 | writeMode);
}

MODULE_PART result_t SWI2C_RepeatedStart(uint8_t rawAddr) {
SETREGS
  //AsyncDelay swv->timeout(swv->timeout_ms, AsyncDelay::MILLIS);
  Set_timeout;
  // Force SCL low
  sclLow;
  i2c_delay;

  // Release SDA
  sdaHigh;
  i2c_delay;

  // Release SCL
  if (!SWI2C_sclHighAndStretch())
    return timedOut;
  i2c_delay;

  // Force SDA low
  sdaLow;
  i2c_delay;

  return SWI2C_Write(rawAddr << 1 | writeMode);
}


MODULE_PART int32_t SWI2C_StartWait(uint8_t rawAddr) {
SETREGS
 //AsyncDelay swv->timeout(swv->timeout_ms, AsyncDelay::MILLIS);
 Set_timeout;

  while (!(timeout_isExpired)) {
    // Force SDA low
    sdaLow;
    i2c_delay;

    switch (SWI2C_Write(rawAddr << 1 | writeMode)) {
      case ack:
        return ack;
      case nack:
        SWI2C_stop();
        return nack;
      default:
        // swv->timeout, and anything else we don't know about
        SWI2C_stop();
        return timedOut;
    }
  }
  return timedOut;
}


MODULE_PART result_t SWI2C_Write(uint8_t data) {
SETREGS
  //AsyncDelay swv->timeout(swv->timeout_ms, AsyncDelay::MILLIS);
  Set_timeout;
  for (uint8_t i = 8; i; --i) {
    // Force SCL low
    sclLow;

    if (data & 0x80) {
      // Release SDA
      sdaHigh;
    }
    else {
      // Force SDA low
      sdaLow;
    }
    i2c_delay;

    // Release SCL
    if (!SWI2C_sclHighAndStretch())
      return timedOut;

    i2c_delay;

    data <<= 1;
    if (timeout_isExpired) {
      SWI2C_stop(); // Reset bus
      return timedOut;
    }
  }

  // Get ACK
  // Force SCL low
  sclLow;

  // Release SDA
  sdaHigh;

  i2c_delay;

  // Release SCL
  if (!SWI2C_sclHighAndStretch())
    return timedOut;

  result_t res = (sdaRead == LOW ? ack : nack);

  i2c_delay;

  // Keep SCL low between bytes
  sclLow;

  return res;
}


MODULE_PART result_t SWI2C_Reada(uint8_t *data, bool sendAck) {
SETREGS
  *data = 0;
  //AsyncDelay swv->timeout(swv->timeout_ms, AsyncDelay::MILLIS);
  Set_timeout;

  for (uint8_t i = 8; i; --i) {
    *data <<= 1;

    // Force SCL low
    sclLow;

    // Release SDA (from previous ACK)
    sdaHigh;
    i2c_delay;

    // Release SCL
    if (SWI2C_sclHighAndStretch())
      return timedOut;
    i2c_delay;

    // Read clock stretch
    while (sclRead == LOW)
      if (timeout_isExpired) {
        SWI2C_stop(); // Reset bus
        return timedOut;
      }

    if (sdaRead)
      *data |= 1;
  }

  // Put ACK/NACK
  // Force SCL low
  sclLow;

  if (sendAck) {
    // Force SDA low
    sdaLow;
  }
  else {
    // Release SDA
    sdaHigh;
  }

  i2c_delay;

  // Release SCL
  if (!SWI2C_sclHighAndStretch())
    return timedOut;
  i2c_delay;

  // Wait for SCL to return high
  while (sclRead == LOW)
    if (!timeout_isExpired) {
      SWI2C_stop(); // Reset bus
      return timedOut;
    }

  i2c_delay;

  // Keep SCL low between bytes
  sclLow;

  return ack;
}


MODULE_PART int32_t SWI2C_available(void) {
SETREGS
  return swv->rxBufferBytesRead - swv->rxBufferIndex;
}


MODULE_PART int32_t SWI2C_write(uint8_t data) {
SETREGS
  if (swv->txBufferIndex >= swv->txBufferSize) {
    //setWriteError();
    return 0;
  }

  swv->txBuffer[swv->txBufferIndex++] = data;
  return 1;
}


// Unlike the Wire version this function returns the actual amount of data written into the buffer
MODULE_PART int32_t SWI2C_writen(const uint8_t *data, size_t quantity) {
SETREGS
  size_t r = 0;
  for (size_t i = 0; i < quantity; ++i) {
    r += SWI2C_write(data[i]);
  }
  return r;
}


MODULE_PART uint8_t SWI2C_Read(void) {
SETREGS
  if (swv->rxBufferIndex < swv->rxBufferBytesRead)
    return swv->rxBuffer[swv->rxBufferIndex++];
  else
    return -1;
}


MODULE_PART uint8_t SWI2C_peek(void) {
SETREGS
  if (swv->rxBufferIndex < swv->rxBufferBytesRead)
    return swv->rxBuffer[swv->rxBufferIndex];
  else
    return -1;
}


// Restore pins to inputs, with no pullups
MODULE_PART void SWI2C_end(void) {
SETREGS
  // enable pullups
  swv->inputMode = INPUT_PULLUP;
  sdaHigh;
  sclHigh;
}

/*
	.global	__udivsi3
	.literal_position
	.literal .LC2, 1000000
  */
MODULE_PART void SWI2C_setClock(uint32_t frequency) {
SETREGS
  uint32_t period_us = uint32_t(1000000UL) / frequency;
  if (period_us < 2)
    period_us = 2;
  else if (period_us > 2 * 255)
    period_us = 2 * 255;

  swv->delay_us = (period_us / 2);
}


MODULE_PART void SWI2C_beginTransmission(uint8_t address) {
SETREGS
  swv->txAddress = address;
  swv->txBufferIndex = 0;
}


MODULE_PART uint8_t SWI2C_endTransmission(uint8_t sendStop) {
SETREGS 
  uint8_t r = SWI2C_endTransmissionInner();
  if (sendStop)
    SWI2C_stop();
  else
    swv->transmissionInProgress = true;
  return r;
}

MODULE_PART uint8_t SWI2C_endTransmissionInner(void) {
SETREGS
  result_t r;
  if (swv->transmissionInProgress) {
    r = SWI2C_RepeatedStart(swv->txAddress);
  } else {
    r = SWI2C_Start(swv->txAddress);
  }
  if (r == nack)
    return 2;
  else if (r == timedOut)
    return 4;

  for (uint8_t i = 0; i < swv->txBufferIndex; ++i) {
    r = SWI2C_Write(swv->txBuffer[i]);
    if (r == nack)
      return 3;
    else if (r == timedOut)
      return 4;
  }

  return 0;
}


MODULE_PART uint8_t SWI2C_requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop) {
SETREGS
  result_t r;
  swv->rxBufferIndex = 0;
  swv->rxBufferBytesRead = 0;

  if (swv->transmissionInProgress) {
    r = SWI2C_RepeatedStart(address);
  } else {
    r = SWI2C_Start(address);
  }

  if (r == ack) {
    for (uint8_t i = 0; i < quantity; ++i) {
      if (i >= swv->rxBufferSize)
        break; // Don't write beyond buffer
      result_t res = SWI2C_Reada(&swv->rxBuffer[i],true);
      if (res != ack)
        break;

      ++swv->rxBufferBytesRead;
    }
  }

  if (sendStop) {
    SWI2C_stop();
  } else {
    swv->transmissionInProgress = true;
  }

  return swv->rxBufferBytesRead;
}



MODULE_PART bool SWI2C_sclHighAndStretch() {
  SETREGS
  sclHigh;

  // Wait for SCL to actually become high in case the slave keeps
  // it low (clock stretching).
  while (sclRead == LOW)
    //if (timeout.isExpired()) {
    if (timeout_isExpired) {
      SWI2C_stop(); // Reset bus. Do not allow clock stretching here
      return false;
    }

  return true;
}
#endif

MODULE_PART bool SWI2C_SetDevice(uint32_t addr) {
  SETREGS
  SWI2C_beginTransmission((uint8_t)addr);
  uint32_t err = SWI2C_endTransmission(true);
  if (err == 0) {
    return true;
  }
  AddLog(LOG_LEVEL_INFO, PSTR(">> %d"), err);
  return err;
}

MODULE_PART void SWI2C_SetActiveFound(uint32_t addr, const char *types) {
  SETREGS
  AddLog(LOG_LEVEL_INFO, PSTR("I2C: %s found at 0x%02x"), types, addr);
}


