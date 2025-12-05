// If possible disable interrupts whilst switching pin direction. Sadly
// there is no generic Arduino function to read the current interrupt
// status, only to enable and disable interrupts.  As a result the
// protection against spurious signals on the I2C bus is only available
// for AVR architectures where ATOMIC_BLOCK is defined.

#define sdaLow digitalWrite(swv->sda, LOW);
#define sdaHigh pinMode(swv->sda, INPUT);

#define xsdaLow pinMode(swv->sda, OUTPUT);
#define xsdaHigh pinMode(swv->sda, INPUT);

#define swv mem->swv

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
  swv->rxBufferLength = SWI2C_BUFFER_LENGTH;
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
    if(quantity > SWI2C_BUFFER_LENGTH){
      quantity = SWI2C_BUFFER_LENGTH;
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

int SWI2C_available(void) {
SETREGS
  return swv->rxBufferIndex;
}


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


// to be implemented
MODULE_PART void SWI2C_WriteN(uint8_t *buff, uint32_t len) {
SETREGS
  for (uint32_t cnt = 0; cnt < len; cnt++) {
    SWI2C_Write(*buff++);
  }
}

MODULE_PART bool SWI2C_ValidRead(uint8_t addr, uint8_t reg, uint8_t size, uint8_t bus, bool sendStop, uint32_t *I2C_buffer) {
SETREGS
  bool status = false;
  SWI2C_beginTransmission(addr);                       // start transmission to device
  SWI2C_Write(reg);                                    // sends register address to read from
  if (0 == SWI2C_endTransmission(sendStop)) {          // Try to become I2C Master, send data and collect bytes, keep master status for next request...
    SWI2C_requestFrom((int)addr, (int)size, true);           // send data n-bytes read
    if (SWI2C_available() == size) {
      for (uint32_t i = 0; i < size; i++) {
        *I2C_buffer = *I2C_buffer << 8 | SWI2C_Read();   // receive DATA
      }
      status = true;                                    // 1 = OK
    }
  }
  return status;
}


MODULE_PART bool SWI2C_ValidRead16(uint16_t *data, uint8_t addr, uint8_t reg, uint8_t bus) {
SETREGS
  uint32_t I2C_buffer;
  bool status = SWI2C_ValidRead(addr, reg, 2, 0, true, &I2C_buffer);
  *data = (uint16_t)I2C_buffer;
  return status;
}

MODULE_PART uint16_t SWI2C_Read16(uint8_t addr, uint8_t reg, uint8_t bus) {
SETREGS
  uint32_t I2C_buffer;
  SWI2C_ValidRead(addr, reg, 2, 0, true, &I2C_buffer);
  return (uint16_t)I2C_buffer;
}


MODULE_PART bool SWI2C_I2cWrite(uint8_t addr, uint8_t reg, uint32_t val, uint8_t size, uint8_t bus) {
SETREGS
  SWI2C_beginTransmission((uint8_t)addr);              // start transmission to device
  SWI2C_Write(reg);                                   // sends register address to write to
  uint8_t bytes = size;
  while (bytes--) {
    SWI2C_Write((val >> (8 * bytes)) & 0xFF);          // write data
  }
  SWI2C_endTransmission(true);
  return false;
}

MODULE_PART bool SWI2C_Write16(uint8_t addr, uint8_t reg, uint32_t val, uint8_t bus) {
SETREGS
  return SWI2C_I2cWrite(addr, reg, val, 2, 0);   
}

