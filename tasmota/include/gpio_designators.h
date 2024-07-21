/*
  tasmota_template.h - template settings for Tasmota

  Copyright (C) 2021  Theo Arends

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

#ifndef _TASMOTA_GPIOS_H_
#define _TASMOTA_GPIOS_H_

// User selectable GPIO functionality
// ATTENTION: Only add at the end of this list just before GPIO_SENSOR_END
//            Then add the same name(s) in a nice location in array kGpioNiceList
enum UserSelectablePins {
  GPIO_NONE,                           // Not used
  GPIO_KEY1, GPIO_KEY1_NP, GPIO_KEY1_INV, GPIO_KEY1_INV_NP, // 4 x Button
  GPIO_SWT1, GPIO_SWT1_NP,             // 8 x User connected external switches
  GPIO_REL1, GPIO_REL1_INV,            // 8 x Relays
  GPIO_LED1, GPIO_LED1_INV,            // 4 x Leds
  GPIO_CNTR1, GPIO_CNTR1_NP,           // 4 x Counter
  GPIO_PWM1, GPIO_PWM1_INV,            // 5 x PWM
  GPIO_BUZZER, GPIO_BUZZER_INV,        // Buzzer
  GPIO_LEDLNK, GPIO_LEDLNK_INV,        // Link led
  GPIO_I2C_SCL, GPIO_I2C_SDA,          // Software I2C
  GPIO_SPI_MISO, GPIO_SPI_MOSI, GPIO_SPI_CLK, GPIO_SPI_CS, GPIO_SPI_DC,        // Hardware SPI
  GPIO_SSPI_MISO, GPIO_SSPI_MOSI, GPIO_SSPI_SCLK, GPIO_SSPI_CS, GPIO_SSPI_DC,  // Software SPI
  GPIO_BACKLIGHT,                      // Display backlight control
  GPIO_OLED_RESET,                     // OLED Display Reset
  GPIO_IRSEND, GPIO_IRRECV,            // IR interface
  GPIO_RFSEND, GPIO_RFRECV,            // RF interface
  GPIO_DHT11, GPIO_DHT22, GPIO_SI7021, GPIO_DHT11_OUT,  // DHT11, DHT21, DHT22, AM2301, AM2302, AM2321
  GPIO_DSB, GPIO_DSB_OUT,              // DS18B20 or DS18S20
  GPIO_WS2812,                         // WS2812 Led string
  GPIO_MHZ_TXD, GPIO_MHZ_RXD,          // MH-Z19 Serial interface
  GPIO_PZEM0XX_TX, GPIO_PZEM004_RX, GPIO_PZEM016_RX, GPIO_PZEM017_RX, // PZEM Serial Modbus interface
  GPIO_SAIR_TX, GPIO_SAIR_RX,          // SenseAir Serial interface
  GPIO_PMS5003_TX, GPIO_PMS5003_RX,    // Plantower PMS5003 Serial interface
  GPIO_SDS0X1_TX, GPIO_SDS0X1_RX,      // Nova Fitness SDS011 Serial interface
  GPIO_SBR_TX, GPIO_SBR_RX,            // Serial Bridge Serial interface
  GPIO_SR04_TRIG, GPIO_SR04_ECHO,      // SR04 interface
  GPIO_SDM120_TX, GPIO_SDM120_RX,      // SDM120 Serial interface
  GPIO_SDM630_TX, GPIO_SDM630_RX,      // SDM630 Serial interface
  GPIO_TM1638CLK, GPIO_TM1638DIO, GPIO_TM1638STB,  // TM1638 interface
  GPIO_MP3_DFR562,                     // RB-DFR-562, DFPlayer Mini MP3 Player
  GPIO_HX711_SCK, GPIO_HX711_DAT,      // HX711 Load Cell interface
  GPIO_TX2X_TXD_BLACK,                 // TX20/TX23 Transmission Pin
  GPIO_TUYA_TX, GPIO_TUYA_RX,          // Tuya Serial interface
  GPIO_MGC3130_XFER, GPIO_MGC3130_RESET,  // MGC3130 interface
  GPIO_RF_SENSOR,                      // Rf receiver with sensor decoding
  GPIO_AZ_TXD, GPIO_AZ_RXD,            // AZ-Instrument 7798 Serial interface
  GPIO_MAX31855CS, GPIO_MAX31855CLK, GPIO_MAX31855DO,  // MAX31855 Serial interface
  GPIO_NRG_SEL, GPIO_NRG_SEL_INV, GPIO_NRG_CF1, GPIO_HLW_CF, GPIO_HJL_CF,  // HLW8012/HJL-01/BL0937 energy monitoring
  GPIO_MCP39F5_TX, GPIO_MCP39F5_RX, GPIO_MCP39F5_RST,  // MCP39F501 Energy monitoring (Shelly2)
  GPIO_PN532_TXD, GPIO_PN532_RXD,      // PN532 NFC Serial interface
  GPIO_SM16716_CLK, GPIO_SM16716_DAT, GPIO_SM16716_SEL,  // SM16716 SELECT
  GPIO_DI, GPIO_DCKI,                  // my92x1 PWM controller
  GPIO_CSE7766_TX, GPIO_CSE7766_RX,    // CSE7766 Serial interface (S31 and Pow R2)
  GPIO_ARIRFRCV, GPIO_ARIRFSEL,        // Arilux RF Receive input
  GPIO_TXD, GPIO_RXD,                  // Serial interface
  GPIO_ROT1A, GPIO_ROT1B,              // Rotary switch
  GPIO_ADC_JOY,                        // Analog joystick
  GPIO_SSPI_MAX31865_CS1,              // MAX31865 Chip Select
  GPIO_HRE_CLOCK, GPIO_HRE_DATA,       // HR-E Water Meter
  GPIO_ADE7953_IRQ,                    // ADE7953 IRQ
  GPIO_SOLAXX1_TX, GPIO_SOLAXX1_RX,    // Solax Inverter Serial interface
  GPIO_ZIGBEE_TX, GPIO_ZIGBEE_RX,      // Zigbee Serial interface
  GPIO_RDM6300_RX,                     // RDM6300 RX
  GPIO_IBEACON_TX, GPIO_IBEACON_RX,    // HM17 IBEACON Serial interface
  GPIO_A4988_DIR, GPIO_A4988_STP, GPIO_A4988_ENA, GPIO_A4988_MS1,  // A4988 interface
  GPIO_OUTPUT_HI, GPIO_OUTPUT_LO,      // Fixed output state
  GPIO_DDS2382_TX, GPIO_DDS2382_RX,    // DDS2382 Serial interface
  GPIO_DDSU666_TX, GPIO_DDSU666_RX,    // DDSU666 Serial interface
  GPIO_SM2135_CLK, GPIO_SM2135_DAT,    // SM2135 PWM controller
  GPIO_DEEPSLEEP,                      // Kill switch for deepsleep
  GPIO_EXS_ENABLE,                     // EXS MCU Enable
  GPIO_TASMOTACLIENT_TXD, GPIO_TASMOTACLIENT_RXD,      // Client Serial interface
  GPIO_TASMOTACLIENT_RST, GPIO_TASMOTACLIENT_RST_INV,  // Client Reset
  GPIO_HPMA_RX, GPIO_HPMA_TX,          // Honeywell HPMA115S0 Serial interface
  GPIO_GPS_RX, GPIO_GPS_TX,            // GPS Serial interface
  GPIO_HM10_RX, GPIO_HM10_TX,          // HM10-BLE-Mijia-bridge Serial interface
  GPIO_LE01MR_RX, GPIO_LE01MR_TX,      // F&F LE-01MR energy meter
  GPIO_CC1101_GDO0, GPIO_CC1101_GDO2,  // CC1101 Serial interface
  GPIO_HRXL_RX,                        // Data from MaxBotix HRXL sonar range sensor
  GPIO_ELECTRIQ_MOODL_TX,              // ElectriQ iQ-wifiMOODL Serial TX
  GPIO_AS3935,                         // Franklin Lightning Sensor
  GPIO_ADC_INPUT,                      // Analog input
  GPIO_ADC_TEMP,                       // Analog Thermistor
  GPIO_ADC_LIGHT,                      // Analog Light sensor
  GPIO_ADC_BUTTON, GPIO_ADC_BUTTON_INV,  // Analog Button
  GPIO_ADC_RANGE,                      // Analog Range
  GPIO_ADC_CT_POWER,                   // ANalog Current
#ifdef ESP32
  GPIO_WEBCAM_PWDN, GPIO_WEBCAM_RESET, GPIO_WEBCAM_XCLK,  // Webcam
  GPIO_WEBCAM_SIOD, GPIO_WEBCAM_SIOC,  // Webcam I2C
  GPIO_WEBCAM_DATA,
  GPIO_WEBCAM_VSYNC, GPIO_WEBCAM_HREF, GPIO_WEBCAM_PCLK,
  GPIO_WEBCAM_PSCLK,
  GPIO_WEBCAM_HSD,
  GPIO_WEBCAM_PSRCS,
#endif
  GPIO_BOILER_OT_RX, GPIO_BOILER_OT_TX,  // OpenTherm Boiler TX pin
  GPIO_WINDMETER_SPEED,                // WindMeter speed counter pin
  GPIO_KEY1_TC,                        // Touch pin as button
  GPIO_BL0940_RX,                      // BL0940 serial interface
  GPIO_TCP_TX, GPIO_TCP_RX,            // TCP to serial bridge
#ifdef ESP32
  GPIO_ETH_PHY_POWER, GPIO_ETH_PHY_MDC, GPIO_ETH_PHY_MDIO,  // Ethernet
#endif
  GPIO_TELEINFO_RX,                    // Teleinfo telemetry data receive pin
  GPIO_TELEINFO_ENABLE,                // Teleinfo Enable Receive Pin
  GPIO_LMT01,                          // LMT01 input counting pin
  GPIO_IEM3000_TX, GPIO_IEM3000_RX,    // IEM3000 Serial interface
  GPIO_ZIGBEE_RST,                     // Zigbee reset
  GPIO_DYP_RX,
  GPIO_MIEL_HVAC_TX, GPIO_MIEL_HVAC_RX,  // Mitsubishi Electric HVAC
  GPIO_WE517_TX, GPIO_WE517_RX,        // ORNO WE517 Serial interface
  GPIO_AS608_TX, GPIO_AS608_RX,        // Serial interface AS608 / R503
  GPIO_SHELLY_DIMMER_BOOT0, GPIO_SHELLY_DIMMER_RST_INV,
  GPIO_RC522_RST,                      // RC522 reset
  GPIO_P9813_CLK, GPIO_P9813_DAT,      // P9813 Clock and Data
  GPIO_OPTION_A,                       // Specific device options to be served in code
  GPIO_FTC532,                         // FTC532 touch ctrlr serial input
  GPIO_RC522_CS,
  GPIO_NRF24_CS, GPIO_NRF24_DC,
  GPIO_ILI9341_CS, GPIO_ILI9341_DC,
  GPIO_ILI9488_CS,
  GPIO_EPAPER29_CS,
  GPIO_EPAPER42_CS,
  GPIO_SSD1351_CS,
  GPIO_RA8876_CS,
  GPIO_ST7789_CS, GPIO_ST7789_DC,
  GPIO_SSD1331_CS, GPIO_SSD1331_DC,
  GPIO_SDCARD_CS,
  GPIO_ROT1A_NP, GPIO_ROT1B_NP,        // Rotary switch
  GPIO_ADC_PH,                         // Analog PH Sensor
  GPIO_BS814_CLK, GPIO_BS814_DAT,      // Holtek BS814A2 touch ctrlr
  GPIO_WIEGAND_D0, GPIO_WIEGAND_D1,    // Wiegand Data lines
  GPIO_NEOPOOL_TX, GPIO_NEOPOOL_RX,    // Sugar Valley RS485 interface
  GPIO_SDM72_TX, GPIO_SDM72_RX,        // SDM72 Serial interface
  GPIO_TM1637CLK, GPIO_TM1637DIO,      // TM1637 interface
  GPIO_PROJECTOR_CTRL_TX, GPIO_PROJECTOR_CTRL_RX,  // LCD/DLP Projector Serial Control
  GPIO_SSD1351_DC,
  GPIO_XPT2046_CS,                     // XPT2046 SPI Chip Select
  GPIO_CSE7761_TX, GPIO_CSE7761_RX,    // CSE7761 Serial interface (Dual R3)
  GPIO_VL53LXX_XSHUT1,                 // VL53LXX_XSHUT (the max number of sensors is VL53LXX_MAX_SENSORS)- Used when connecting multiple VL53LXX
  GPIO_MAX7219CLK, GPIO_MAX7219DIN, GPIO_MAX7219CS, // MAX7219 interface
  GPIO_TFMINIPLUS_TX, GPIO_TFMINIPLUS_RX,  // TFmini Plus ToF sensor
  GPIO_ZEROCROSS,
#ifdef ESP32
  GPIO_HALLEFFECT,
  GPIO_EPD_DATA,                       // Base connection EPD driver
#endif
  GPIO_INPUT,
#ifdef ESP32
  GPIO_KEY1_PD, GPIO_KEY1_INV_PD, GPIO_SWT1_PD,
#endif
  GPIO_I2S_DOUT, GPIO_I2S_BCLK, GPIO_I2S_WS, GPIO_I2S_DIN,
  GPIO_I2S_BCLK_IN,  GPIO_I2S_WS_IN,   // Spare since 20240603
  GPIO_INTERRUPT,
  GPIO_MCP2515_CS,                     // MCP2515 Chip Select
  GPIO_HRG15_TX, GPIO_HRG15_RX,        // Hydreon RG-15 rain sensor serial interface
  GPIO_VINDRIKTNING_RX,                // IKEA VINDRIKTNING Serial interface
  GPIO_BL0939_RX,                      // BL0939 Serial interface (Dual R3 v2)
  GPIO_BL0942_RX,                      // BL0942 Serial interface
  GPIO_HM330X_SET,                     // HM330X SET pin (sleep when low)
  GPIO_HEARTBEAT, GPIO_HEARTBEAT_INV,
  GPIO_SHIFT595_SRCLK, GPIO_SHIFT595_RCLK, GPIO_SHIFT595_OE, GPIO_SHIFT595_SER,   // 74x595 Shift register
  GPIO_SOLAXX1_RTS,                    // Solax Inverter Serial interface
  GPIO_OPTION_E,                       // Emulated module
  GPIO_SDM230_TX, GPIO_SDM230_RX,      // SDM230 Serial interface
  GPIO_ADC_MQ,                         // Analog MQ Sensor
  GPIO_CM11_TXD, GPIO_CM11_RXD,        // CM11 Serial interface
  GPIO_BL6523_TX, GPIO_BL6523_RX,      // BL6523 based Watt meter Serial interface
  GPIO_ADE7880_IRQ,                    // ADE7880 IRQ
  GPIO_RESET,                          // Generic reset
  GPIO_MS01,                           // Sonoff MS01 Moisture Sensor 1wire interface
  GPIO_SDIO_CMD, GPIO_SDIO_CLK, GPIO_SDIO_D0, GPIO_SDIO_D1, GPIO_SDIO_D2, GPIO_SDIO_D3, // SD Card SDIO interface, including 1-bit and 4-bit modes
  GPIO_FLOWRATEMETER_IN,               // Flowrate Meter
  GPIO_BP5758D_CLK, GPIO_BP5758D_DAT,  // BP5758D PWM controller
  GPIO_SM2335_CLK, GPIO_SM2335_DAT,    // SM2335 PWM controller
  GPIO_MP3_DFR562_BUSY,                // RB-DFR-562, DFPlayer Mini MP3 Player busy flag
  GPIO_TM1621_CS, GPIO_TM1621_WR, GPIO_TM1621_RD, GPIO_TM1621_DAT,  // Sonoff POWR3xxD and THR3xxD LCD display
  GPIO_REL1_BI, GPIO_REL1_BI_INV,      // 8 x Relays bistable
  GPIO_I2S_MCLK,
  GPIO_MBR_TX, GPIO_MBR_RX,            // Modbus Bridge Serial interface
  GPIO_ADE7953_RST,                    // ADE7953 Reset
  GPIO_NRG_MBS_TX, GPIO_NRG_MBS_RX,    // Generic Energy Modbus device
  GPIO_ADE7953_CS,                     // ADE7953 SPI Chip Select
  GPIO_DALI_RX, GPIO_DALI_TX,          // Dali
  GPIO_BP1658CJ_CLK, GPIO_BP1658CJ_DAT,// BP1658CJ
  GPIO_DINGTIAN_CLK, GPIO_DINGTIAN_SDI, GPIO_DINGTIAN_Q7, GPIO_DINGTIAN_PL, GPIO_DINGTIAN_RCK,  // Dingtian relay board - 595's & 165's pins
  GPIO_LD2410_TX, GPIO_LD2410_RX,      // HLK-LD2410
  GPIO_MBR_TX_ENA, GPIO_NRG_MBS_TX_ENA, // Modbus Bridge Serial Transmit Enable
  GPIO_ME007_TRIG, GPIO_ME007_RX,       // ME007 Serial/Trigger interface
  GPIO_TUYAMCUBR_TX, GPIO_TUYAMCUBR_RX, // TuyaMCU Bridge
  GPIO_BIOPDU_PZEM0XX_TX, GPIO_BIOPDU_PZEM016_RX, GPIO_BIOPDU_BIT, // Biomine BioPDU 625x12
  GPIO_MCP23XXX_INT, GPIO_MCP23SXX_CS,  // MCP23xxx Int and SPI Chip select
  GPIO_PCF8574_INT,                     // PCF8574 interrupt
  GPIO_LOX_O2_RX,                       // LOX-O2 RX
  GPIO_GM861_TX, GPIO_GM861_RX,         // GM861 Serial interface
  GPIO_DINGTIAN_OE,                     // New version of Dingtian relay board where PL is not shared with OE
  GPIO_HDMI_CEC,                        // Support for HDMI CEC
  GPIO_HC8_RXD,                         // HC8 Serial interface
  GPIO_I2S_DAC,                         // Audio DAC support for ESP32 and ESP32S2
  GPIO_MAGIC_SWITCH,                    // MagicSwitch as in Sonoff BasicR4
  GPIO_PIPSOLAR_TX, GPIO_PIPSOLAR_RX,   // pipsolar inverter
  GPIO_LORA_CS, GPIO_LORA_RST, GPIO_LORA_BUSY, GPIO_LORA_DI0, GPIO_LORA_DI1, GPIO_LORA_DI2, GPIO_LORA_DI3, GPIO_LORA_DI4, GPIO_LORA_DI5,  // LoRa SPI
  GPIO_TS_SPI_CS, GPIO_TS_RST, GPIO_TS_IRQ, // SPI for Universal Touch Screen
  GPIO_RN2XX3_TX, GPIO_RN2XX3_RX, GPIO_RN2XX3_RST,  // RN2XX3 LoRaWan node Serial interface
  GPIO_TCP_TX_EN,                       // TCP to serial bridge, EN pin
  GPIO_ASR650X_TX, GPIO_ASR650X_RX,     // ASR650X LoRaWan node Serial interface
  GPIO_SENSOR_END };
  
  #endif // _TASMOTA_GPIOS_H_
