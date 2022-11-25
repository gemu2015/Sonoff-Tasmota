/*
  xsns_96_cc1101.ino - CC1101 radio_modem support

  Copyright (C) 2020  Gerhard Mutz and Rudolf Koenig (culfw)

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


#ifdef USE_SPI
#ifdef USE_CC1101_433

#define XSNS_96 96

#ifdef ESP32
#undef HW_SPI_MOSI
#define HW_SPI_MOSI 23
#undef HW_SPI_MISO
#define HW_SPI_MISO 19
#undef HW_SPI_CLK
#define HW_SPI_CLK 18
#else
#undef HW_SPI_MOSI
#define HW_SPI_MOSI 13
#undef HW_SPI_MISO
#define HW_SPI_MISO 12
#undef HW_SPI_CLK
#define HW_SPI_CLK 14
#endif

struct CC1101_433 {
    uint8_t found;
    uint8_t cs;
    uint8_t ready;
    SPISettings spi_settings;
    uint32_t last_task;
} cc1101_433;


void CC1101_433_Detect(void) {
  cc1101_433.found = 0;
  cc1101_433.cs = 0;
  cc1101_433.ready = 0;

  if ((Pin(GPIO_SPI_MOSI)==HW_SPI_MOSI) && (Pin(GPIO_SPI_MISO)==HW_SPI_MISO) && (Pin(GPIO_SPI_CLK)==HW_SPI_CLK) && (Pin(GPIO_SPI_CS)<99)) {
    cc1101_433.cs = Pin(GPIO_SPI_CS);
    cc1101_433.found = 1;
  } else {
    return;
  }

  pinMode(cc1101_433.cs, OUTPUT);
  digitalWrite(cc1101_433.cs, 1);
  AddLog_P2(LOG_LEVEL_INFO, PSTR("CC1101: cs=%d"), cc1101_433.cs);

#ifndef ESP32
  SPI.begin();
#else
  SPI.begin(Pin(GPIO_SPI_CLK), Pin(GPIO_SPI_MISO), Pin(GPIO_SPI_MOSI), -1);
#endif

  cc1101_433.spi_settings = SPISettings(5000000, MSBFIRST, SPI_MODE3);

  cc1101_rf_init();

  cc1101_433.ready = 1;

  cc1101_433.last_task = millis();


//  GPIO_CC1101_GDO2 17



}

#undef CC1101_DEASSERT
#undef CC1101_ASSERT
#define CC1101_DEASSERT  	digitalWrite(cc1101_433.cs,1);SPI.endTransaction();
#define CC1101_ASSERT    	SPI.beginTransaction(cc1101_433.spi_settings);digitalWrite(cc1101_433.cs, 0)

// Configuration Registers
#define CC1101_IOCFG2           0x00    // GDO2 output pin configuration
#define CC1101_IOCFG1           0x01    // GDO1 output pin configuration
#define CC1101_IOCFG0           0x02    // GDO0 output pin configuration
#define CC1101_FIFOTHR          0x03    // RX FIFO and TX FIFO thresholds
#define CC1101_SYNC1            0x04    // Sync word, high byte
#define CC1101_SYNC0            0x05    // Sync word, low byte
#define CC1101_PKTLEN           0x06    // Packet length
#define CC1101_PKTCTRL1         0x07    // Packet automation control
#define CC1101_PKTCTRL0         0x08    // Packet automation control
#define CC1101_ADDR             0x09    // Device address
#define CC1101_CHANNR           0x0A    // Channel number
#define CC1101_FSCTRL1          0x0B    // Frequency synthesizer control
#define CC1101_FSCTRL0          0x0C    // Frequency synthesizer control
#define CC1101_FREQ2            0x0D    // Frequency control word, high byte
#define CC1101_FREQ1            0x0E    // Frequency control word, middle byte
#define CC1101_FREQ0            0x0F    // Frequency control word, low byte
#define CC1101_MDMCFG4          0x10    // Modem configuration
#define CC1101_MDMCFG3          0x11    // Modem configuration
#define CC1101_MDMCFG2          0x12    // Modem configuration
#define CC1101_MDMCFG1          0x13    // Modem configuration
#define CC1101_MDMCFG0          0x14    // Modem configuration
#define CC1101_DEVIATN          0x15    // Modem deviation setting
#define CC1101_MCSM2            0x16    // Main Radio Cntrl State Machine config
#define CC1101_MCSM1            0x17    // Main Radio Cntrl State Machine config
#define CC1101_MCSM0            0x18    // Main Radio Cntrl State Machine config
#define CC1101_FOCCFG           0x19    // Frequency Offset Compensation config
#define CC1101_BSCFG            0x1A    // Bit Synchronization configuration
#define CC1101_AGCCTRL2         0x1B    // AGC control
#define CC1101_AGCCTRL1         0x1C    // AGC control
#define CC1101_AGCCTRL0         0x1D    // AGC control
#define CC1101_WOREVT1          0x1E    // High byte Event 0 timeout
#define CC1101_WOREVT0          0x1F    // Low byte Event 0 timeout
#define CC1101_WORCTRL          0x20    // Wake On Radio control
#define CC1101_FREND1           0x21    // Front end RX configuration
#define CC1101_FREND0           0x22    // Front end TX configuration
#define CC1101_FSCAL3           0x23    // Frequency synthesizer calibration
#define CC1101_FSCAL2           0x24    // Frequency synthesizer calibration
#define CC1101_FSCAL1           0x25    // Frequency synthesizer calibration
#define CC1101_FSCAL0           0x26    // Frequency synthesizer calibration
#define CC1101_RCCTRL1          0x27    // RC oscillator configuration
#define CC1101_RCCTRL0          0x28    // RC oscillator configuration
#define CC1101_FSTEST           0x29    // Frequency synthesizer cal control
#define CC1101_PTEST            0x2A    // Production test
#define CC1101_AGCTEST          0x2B    // AGC test
#define CC1101_TEST2            0x2C    // Various test settings
#define CC1101_TEST1            0x2D    // Various test settings
#define CC1101_TEST0            0x2E    // Various test settings

// Status registers
#define CC1101_PARTNUM          0x30    // Part number
#define CC1101_VERSION          0x31    // Current version number
#define CC1101_FREQEST          0x32    // Frequency offset estimate
#define CC1101_LQI              0x33    // Demodulator estimate for link quality
#define CC1101_RSSI             0x34    // Received signal strength indication
#define CC1101_MARCSTATE        0x35    // Control state machine state
#define CC1101_WORTIME1         0x36    // High byte of WOR timer
#define CC1101_WORTIME0         0x37    // Low byte of WOR timer
#define CC1101_PKTSTATUS        0x38    // Current GDOx status and packet status
#define CC1101_VCO_VC_DAC       0x39    // Current setting from PLL cal module
#define CC1101_TXBYTES          0x3A    // Underflow and # of bytes in TXFIFO
#define CC1101_RXBYTES          0x3B    // Overflow and # of bytes in RXFIFO

// Multi byte memory locations
#define CC1101_PATABLE          0x3E
#define CC1101_TXFIFO           0x3F
#define CC1101_RXFIFO           0x3F

// Definitions for burst/single access to registers
#define CC1101_WRITE_BURST      0x40
#define CC1101_READ_SINGLE      0x80
#define CC1101_READ_BURST       0xC0

// Strobe commands
#define CC1101_SRES             0x30        // Reset chip.
#define CC1101_SFSTXON          0x31        // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
                                            // If in RX/TX: Go to a wait state where only the synthesizer is
                                            // running (for quick RX / TX turnaround).
#define CC1101_SXOFF            0x32        // Turn off crystal oscillator.
#define CC1101_SCAL             0x33        // Calibrate frequency synthesizer and turn it off
                                            // (enables quick start).
#define CC1101_SRX              0x34        // Enable RX. Perform calibration first if coming from IDLE and
                                            // MCSM0.FS_AUTOCAL=1.
#define CC1101_STX              0x35        // In IDLE state: Enable TX. Perform calibration first if
                                            // MCSM0.FS_AUTOCAL=1. If in RX state and CCA is enabled:
                                            // Only go to TX if channel is clear.
#define CC1101_SIDLE            0x36        // Exit RX / TX, turn off frequency synthesizer and exit
                                            // Wake-On-Radio mode if applicable.
#define CC1101_SAFC             0x37        // Perform AFC adjustment of the frequency synthesizer
#define CC1101_SWOR             0x38        // Start automatic RX polling sequence (Wake-on-Radio)
#define CC1101_SPWD             0x39        // Enter power down mode when CSn goes high.
#define CC1101_SFRX             0x3A        // Flush the RX FIFO buffer.
#define CC1101_SFTX             0x3B        // Flush the TX FIFO buffer.
#define CC1101_SWORRST          0x3C        // Reset real time clock.
#define CC1101_SNOP             0x3D        // No operation. May be used to pad strobe commands to two


const uint8_t PROGMEM CC1101_433_CFG[] = {
        0x00,0x0D,                   //IOCFG2
        0x02,0x0D,                  //IOCFG0
        0x03,0x47,                  //FIFOTHR
        0x06,0x3E,                  //PKTLEN
        0x08,0x32,                //PKTCTRL0
        0x09,0xFF,                 //ADDR
        0x0B,0x06,                  //FSCTRL1

        // AGC Adjust for OKK
        0x1B,0x04,                 //AGCCTRL2                              eventuell Anpassen
        0x1C,0x00,                 //AGCCTRL1
        0x1D,0x90,                 //AGCCTRL0
        0x21,0x56,                   //FREND1
        ////////////////////////////////////////////////////////////////////////
        0x0D,0x10,                  //FREQ2
        0x0E,0xB1,                 //FREQ1
        0x0F,0x3B,                  //FREQ0
        0x10,0xF7,                 //MDMCFG4
        0x11,0x83,                  //MDMCFG3
        0x12,0x30,                  //MDMCFG2
        0x13,0x40,                  //MDMCFG1
        0x14,0xF7,                 //MDMCFG0
        0x15,0x14,                  //DEVIATN
        0x18,0x18,                //MCSM0
        0x19,0x16,                   //FOCCFG
        0x20,0xFB,                  //WORCTRL
        0x22,0x11,                 //FREND0
        0x23,0xE9,                  //FSCAL3
        0x24,0x2A,                 //FSCAL2
        0x25,0x00,                  //FSCAL1
        0x26,0x1F,                  //FSCAL0
        0x2C,0x81,                  //TEST2
        0x2D,0x35,                //TEST1
        0x2E,0x09,                 //TEST0
 0xff
};


const uint8_t PROGMEM ask_ook[]=
{
    CC1101_IOCFG0,      0x0D,
    CC1101_IOCFG2,      0x0D,
      CC1101_FIFOTHR,     0x47,
      CC1101_PKTCTRL0,    0x12,
      CC1101_FSCTRL1,     0x08,
      CC1101_FREQ2,       0x10,
      CC1101_FREQ1,       0xB1,
      CC1101_FREQ0,       0x3B,
      CC1101_MDMCFG4,     0xCA,
      CC1101_MDMCFG3,     0x83,
      CC1101_MDMCFG2,     0xB0,
      CC1101_DEVIATN,     0x35,
      CC1101_MCSM0,       0x18,
      CC1101_FOCCFG,      0x16,
      CC1101_AGCCTRL2,    0x43,
      CC1101_WORCTRL,     0xFB,
      CC1101_FREND0,      0x11,
      CC1101_FSCAL3,      0xE9,
      CC1101_FSCAL2,      0x2A,
      CC1101_FSCAL1,      0x00,
      CC1101_FSCAL0,      0x1F,
      CC1101_TEST2,       0x81,
      CC1101_TEST1,       0x35,
      CC1101_TEST0,       0x09,
    0xff
};

const uint8_t PROGMEM fsk[]=
{
  CC1101_IOCFG0,      0x0D,
  CC1101_IOCFG2,      0x0D,
  CC1101_IOCFG0,      0x06,
  CC1101_PKTCTRL0,    0x05,
  CC1101_FSCTRL1,     0x0C,
  CC1101_FREQ2,       0x10,
  CC1101_FREQ1,       0xB1,
  CC1101_FREQ0,       0x3B,
  CC1101_MDMCFG4,     0x29,
  CC1101_MDMCFG3,     0xA6,
  CC1101_MDMCFG2,     0x03,
  CC1101_DEVIATN,     0x62,
  CC1101_MCSM0,       0x18,
  CC1101_FOCCFG,      0x1D,
  CC1101_BSCFG,       0x1C,
  CC1101_AGCCTRL2,    0xC7,
  CC1101_AGCCTRL1,    0x00,
  CC1101_AGCCTRL0,    0xB0,
  CC1101_WORCTRL,     0xFB,
  CC1101_FREND1,      0xB6,
  CC1101_FSCAL3,      0xE9,
  CC1101_FSCAL2,      0x2A,
  CC1101_FSCAL1,      0x00,
  CC1101_FSCAL0,      0x1F,
  CC1101_TEST0,       0x09,
  0xff
};

const uint8_t PROGMEM gfsk[]=
{
  CC1101_IOCFG0,      0x0D,
  CC1101_IOCFG2,      0x0D,
  CC1101_PKTCTRL0,    0x05,
  CC1101_FSCTRL1,     0x0C,
  CC1101_FREQ2,       0x10,
  CC1101_FREQ1,       0xB1,
  CC1101_FREQ0,       0x3B,
  CC1101_MDMCFG4,     0x29,
  CC1101_MDMCFG3,     0xA6,
  CC1101_MDMCFG2,     0x13,
  CC1101_DEVIATN,     0x62,
  CC1101_MCSM0,       0x18,
  CC1101_FOCCFG,      0x1D,
  CC1101_BSCFG,       0x1C,
  CC1101_AGCCTRL2,    0xC7,
  CC1101_AGCCTRL1,    0x00,
  CC1101_AGCCTRL0,    0xB0,
  CC1101_WORCTRL,     0xFB,
  CC1101_FREND1,      0xB6,
  CC1101_FSCAL3,      0xE9,
  CC1101_FSCAL2,      0x2A,
  CC1101_FSCAL1,      0x00,
  CC1101_FSCAL0,      0x1F,
  CC1101_TEST0,       0x09,
  0xff
};


uint8_t CC1101_sendbyte(uint8_t data) {
  return SPI.transfer(data);
}

uint8_t CC1101_readReg(uint8_t addr) {
  CC1101_ASSERT;
  CC1101_sendbyte( addr|CC1101_READ_BURST );
  uint8_t ret = CC1101_sendbyte( 0 );
  CC1101_DEASSERT;
  return ret;
}

void CC1101_writeReg(uint8_t addr, uint8_t data) {
  CC1101_ASSERT;
  CC1101_sendbyte( addr|CC1101_WRITE_BURST );
  CC1101_sendbyte( data );
  CC1101_DEASSERT;
}

uint8_t ccStrobe(uint8_t strobe) {
  CC1101_ASSERT;
  uint8_t ret = CC1101_sendbyte( strobe );
  CC1101_DEASSERT;
  return ret;
}

void cc1101_rf_init(void) {
  CC1101_DEASSERT;                           // Toggle chip select signal
  delayMicroseconds(30);
  CC1101_ASSERT;
  delayMicroseconds(30);
  CC1101_DEASSERT;
  delayMicroseconds(45);

  ccStrobe( CC1101_SRES );                   // Send SRES command
  delayMicroseconds(100);

  // load configuration
  for (uint8_t i = 0; i<60; i += 2) {
    if (pgm_read_byte( &CC1101_433_CFG[i] )>0x40) {
    //if (pgm_read_byte( &ask_ook[i] )>0x40) {

      break;
    }
    CC1101_writeReg( pgm_read_byte(&CC1101_433_CFG[i]), pgm_read_byte(&CC1101_433_CFG[i+1]) );
  }

  ccStrobe( CC1101_SCAL );

  delay(4); // 4ms: Found by trial and error
  //This is ccRx() but without enabling the interrupt
  uint8_t cnt = 0xff;
  //Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1.
  //Why do it multiple times?
  while (cnt-- && (ccStrobe( CC1101_SRX ) & 0x70) != 1) {
    delayMicroseconds(10);
  }
    //checkFrequency();
}


void CC1101_433_task(void) {

}

void CC1101_433_loop(void) {
  if (cc1101_433.ready) {
    if (millis() - cc1101_433.last_task > 8) {
      CC1101_433_task();
      cc1101_433.last_task = millis();
    }
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns96(byte function) {
  bool result = false;

  switch (function) {
      case FUNC_MODULE_INIT:
        break;
      case FUNC_INIT:
        CC1101_433_Detect();
        break;
      case FUNC_LOOP:
        CC1101_433_loop();
        break;
      case FUNC_JSON_APPEND:
        break;

      case FUNC_COMMAND_SENSOR:
        if (XSNS_96 == XdrvMailbox.index) {
        //  result = XSNS_96_cmd();
        }
        break;
  }
  return result;
}

#endif  // USE_CC1101_433
#endif  // USE_SPI
