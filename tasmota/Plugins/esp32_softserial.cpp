#if 0
// ESP32 combined hardware and software serial driver, software read only
#ifdef ESP32
#ifdef USE_ESP32_SW_SERIAL
#ifndef ESP32_SWS_BUFFER_SIZE
#define ESP32_SWS_BUFFER_SIZE 256
#endif

class SML_ESP32_SERIAL : public Stream {
public:
	SML_ESP32_SERIAL(uint32_t uart_index);
  virtual ~SML_ESP32_SERIAL();
  bool begin(uint32_t speed, uint32_t smode, int32_t recpin, int32_t trxpin, int32_t invert);
  int peek(void);
  int read(void) override;
  size_t write(uint8_t byte) override;
  int available(void) override;
  void flush(void) override;
  void setRxBufferSize(uint32_t size);
  void updateBaudRate(uint32_t baud);
  void rxRead(void);
  void end();
  using Print::write;
private:
  // Member variables
  void setbaud(uint32_t speed);
  uint32_t uart_index;
  int8_t m_rx_pin;
  int8_t m_tx_pin;
  uint32_t cfgmode;
  uint32_t ss_byte;
  uint32_t ss_bstart;
  uint32_t ss_index;
  uint32_t m_bit_time;
  uint32_t m_in_pos;
  uint32_t m_out_pos;
  uint16_t serial_buffer_size;
  bool m_valid;
  uint8_t *m_buffer;
  HardwareSerial *hws;
};


void IRAM_ATTR sml_callRxRead(void *self) { ((SML_ESP32_SERIAL*)self)->rxRead(); };

SML_ESP32_SERIAL::SML_ESP32_SERIAL(uint32_t index) {
  uart_index = index;
  m_valid = true;
}

SML_ESP32_SERIAL::~SML_ESP32_SERIAL(void) {
  if (hws) {
    hws->end();
		delete(hws);
  } else {
    detachInterrupt(m_rx_pin);
    if (m_buffer) {
      free(m_buffer);
    }
  }
}

void SML_ESP32_SERIAL::setbaud(uint32_t speed) {
#ifdef __riscv
  m_bit_time = 1000000 / speed;
#else
  m_bit_time = ESP.getCpuFreqMHz() * 1000000 / speed;
#endif
}

void SML_ESP32_SERIAL::end(void) {
  if (m_buffer) {
    free(m_buffer);
  }
}

bool SML_ESP32_SERIAL::begin(uint32_t speed, uint32_t smode, int32_t recpin, int32_t trxpin, int32_t invert) {
  if (!m_valid) { return false; }

  m_buffer = 0;
  if (recpin < 0) {
    setbaud(speed);
    m_rx_pin = -recpin;
    serial_buffer_size = ESP32_SWS_BUFFER_SIZE;
    m_buffer = (uint8_t*)malloc(serial_buffer_size);
    if (m_buffer == NULL) return false;
    pinMode(m_rx_pin, INPUT_PULLUP);
    attachInterruptArg(m_rx_pin, sml_callRxRead, this, CHANGE);
    m_in_pos = m_out_pos = 0;
    hws = nullptr;
  } else {
    cfgmode = smode;
    m_rx_pin = recpin;
    m_tx_pin = trxpin;
    hws = new HardwareSerial(uart_index);
    if (hws) {
      hws->begin(speed, cfgmode, m_rx_pin, m_tx_pin, invert);
    }
  }
  return true;
}

void SML_ESP32_SERIAL::flush(void) {
  if (hws) {
    hws->flush();
  } else {
    m_in_pos = m_out_pos = 0;
  }
}

int SML_ESP32_SERIAL::peek(void) {
  if (hws) {
    return  hws->peek();
  } else {
    if (m_in_pos == m_out_pos) return -1;
    return m_buffer[m_out_pos];
  }
}

int SML_ESP32_SERIAL::read(void) {
  if (hws) {
    return hws->read();
  } else {
    if (m_in_pos == m_out_pos) return -1;
    uint32_t ch = m_buffer[m_out_pos];
    m_out_pos = (m_out_pos + 1) % serial_buffer_size;
    return ch;
  }
}

int SML_ESP32_SERIAL::available(void) {
  if (hws) {
    return hws->available();
  } else {
    int avail = m_in_pos - m_out_pos;
    if (avail < 0) avail += serial_buffer_size;
    return avail;
  }
}

size_t SML_ESP32_SERIAL::write(uint8_t byte) {
  if (hws) {
    return hws->write(byte);
  }
  return 0;
}

void SML_ESP32_SERIAL::setRxBufferSize(uint32_t size) {
  if (hws) {
    hws->setRxBufferSize(size);
  } else {
    if (m_buffer) {
        free(m_buffer);
    }
    serial_buffer_size = size;
    m_buffer = (uint8_t*)malloc(size);
  }
}
void SML_ESP32_SERIAL::updateBaudRate(uint32_t baud) {
  if (hws) {
    hws->updateBaudRate(baud);
  } else {
    setbaud(baud);
  }
}

// no wait mode only 8N1  (or 7X1, obis only, ignoring parity)
void IRAM_ATTR SML_ESP32_SERIAL::rxRead(void) {
  uint32_t diff;
  uint32_t level;

#define SML_LASTBIT 9

  level = digitalRead(m_rx_pin);

  if (!level && !ss_index) {
    // start condition
#ifdef __riscv
    ss_bstart = micros() - (m_bit_time / 4);
#else
    ss_bstart = ESP.getCycleCount() - (m_bit_time / 4);
#endif
    ss_byte = 0;
    ss_index++;
  } else {
    // now any bit changes go here
    // calc bit number
#ifdef __riscv
    diff = (micros() - ss_bstart) / m_bit_time;
#else
    diff = (ESP.getCycleCount() - ss_bstart) / m_bit_time;
#endif

    if (!level && diff > SML_LASTBIT) {
      // start bit of next byte, store  and restart
      // leave irq at change
      for (uint32_t i = ss_index; i <= SML_LASTBIT; i++) {
        ss_byte |= (1 << i);
      }
      uint32_t next = (m_in_pos + 1) % serial_buffer_size;
      if (next != (uint32_t)m_out_pos) {
        m_buffer[m_in_pos] = ss_byte >> 1;
        m_in_pos = next;
      }
#ifdef __riscv
      ss_bstart = micros() - (m_bit_time / 4);
#else
      ss_bstart = ESP.getCycleCount() - (m_bit_time / 4);
#endif
      ss_byte = 0;
      ss_index = 1;
      return;
    }
    if (diff >= SML_LASTBIT) {
      // bit zero was 0,
      uint32_t next = (m_in_pos + 1) % serial_buffer_size;
      if (next != (uint32_t)m_out_pos) {
        m_buffer[m_in_pos] = ss_byte >> 1;
        m_in_pos = next;
      }
      ss_byte = 0;
      ss_index = 0;
    } else {
      // shift in
      for (uint32_t i = ss_index; i < diff; i++) {
        if (!level) ss_byte |= (1 << i);
      }
      ss_index = diff;
    }
  }
}
#endif // USE_ESP32_SW_SERIAL
#endif // ESP32
#endif