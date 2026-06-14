/*
 * ES7210 4-channel ADC — I2SAUDIO BinPlugin port.
 *
 * Ported from lib/lib_deprecated/es7210/src/es7210.cpp (Espressif MIT) into the
 * plugin's I2C_* binding-macro style, mirroring p_es7243e_c.h (the other ADC) and
 * p_es8311_c.h. Talks I2C via the plugin macros; no TwoWire.
 *
 * Fixed config (matches xdrv_42_4_i2s_codecs.ino es7210_init): SLAVE, I2S normal,
 * 16-bit, 16 kHz @ MCLK = 256*fs (= 4.096 MHz). 4 mics enabled; MIC1/2 = +37.5 dB,
 * MIC3/4 = 0 dB. 16 kHz clock coefficients inlined (adc_div=1, dll=1, doubler=1,
 * osr=0x20, lrck_h=0x01, lrck_l=0x00) like p_es8311_c.h does.
 */

#include "es7210.h"

MODULE_PART esp_err_t pes7210_write_reg(uint8_t reg_addr, uint8_t data) {
SETMEMREGS
    I2C_beginTransmission(ES7210_ADDR);
    I2C_write(reg_addr);
    I2C_write(data);
    I2C_endTransmission(true);
    return 0;
}

MODULE_PART uint8_t pes7210_read_reg(uint8_t reg_addr) {
SETMEMREGS
    uint8_t data;
    I2C_beginTransmission(ES7210_ADDR);
    I2C_write(reg_addr);
    I2C_endTransmission(false);
    I2C_requestFrom(ES7210_ADDR, (size_t)1);
    data = I2C_read();
    return data;
}

MODULE_PART esp_err_t pes7210_update_reg_bit(uint8_t reg_addr, uint8_t update_bits, uint8_t data) {
SETMEMREGS
    uint8_t regv = pes7210_read_reg(reg_addr);
    regv = (regv & (~update_bits)) | (update_bits & data);
    return pes7210_write_reg(reg_addr, regv);
}

// Enable the four mics (mask bits ES7210_INPUT_MIC1..4). Mirrors es7210_mic_select.
MODULE_PART esp_err_t pes7210_mic_select(uint8_t mic) {
SETMEMREGS
    esp_err_t ret = 0;
    for (int i = 0; i < 4; i++) {
        ret |= pes7210_update_reg_bit(ES7210_MIC1_GAIN_REG43 + i, 0x10, 0x00);
    }
    ret |= pes7210_write_reg(ES7210_MIC12_POWER_REG4B, 0xff);
    ret |= pes7210_write_reg(ES7210_MIC34_POWER_REG4C, 0xff);
    if (mic & ES7210_INPUT_MIC1) {
        ret |= pes7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
        ret |= pes7210_write_reg(ES7210_MIC12_POWER_REG4B, 0x00);
        ret |= pes7210_update_reg_bit(ES7210_MIC1_GAIN_REG43, 0x10, 0x10);
    }
    if (mic & ES7210_INPUT_MIC2) {
        ret |= pes7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
        ret |= pes7210_write_reg(ES7210_MIC12_POWER_REG4B, 0x00);
        ret |= pes7210_update_reg_bit(ES7210_MIC2_GAIN_REG44, 0x10, 0x10);
    }
    if (mic & ES7210_INPUT_MIC3) {
        ret |= pes7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x15, 0x00);
        ret |= pes7210_write_reg(ES7210_MIC34_POWER_REG4C, 0x00);
        ret |= pes7210_update_reg_bit(ES7210_MIC3_GAIN_REG45, 0x10, 0x10);
    }
    if (mic & ES7210_INPUT_MIC4) {
        ret |= pes7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x15, 0x00);
        ret |= pes7210_write_reg(ES7210_MIC34_POWER_REG4C, 0x00);
        ret |= pes7210_update_reg_bit(ES7210_MIC4_GAIN_REG46, 0x10, 0x10);
    }
    return ret;
}

// gain: 0..14 (ES7210_GAIN_0DB..ES7210_GAIN_37_5DB)
MODULE_PART esp_err_t pes7210_set_gain(uint8_t mic_mask, uint8_t gain) {
SETMEMREGS
    esp_err_t ret = 0;
    if (gain > ES7210_GAIN_37_5DB) gain = ES7210_GAIN_37_5DB;
    if (mic_mask & ES7210_INPUT_MIC1) ret |= pes7210_update_reg_bit(ES7210_MIC1_GAIN_REG43, 0x0f, gain);
    if (mic_mask & ES7210_INPUT_MIC2) ret |= pes7210_update_reg_bit(ES7210_MIC2_GAIN_REG44, 0x0f, gain);
    if (mic_mask & ES7210_INPUT_MIC3) ret |= pes7210_update_reg_bit(ES7210_MIC3_GAIN_REG45, 0x0f, gain);
    if (mic_mask & ES7210_INPUT_MIC4) ret |= pes7210_update_reg_bit(ES7210_MIC4_GAIN_REG46, 0x0f, gain);
    return ret;
}

MODULE_PART uint32_t pes7210_codec_init(uint8_t *twi_bus) {
SETREGS
    uint32_t ret_val = ESP_OK;

    uint8_t bus = 0;
    I2C_SETWIRE(bus);
    if (!I2cSetDevice(ES7210_ADDR, bus)) {
        bus++;
        I2C_SETWIRE(bus);
        if (!I2cSetDevice(ES7210_ADDR, bus)) {
            return -1;
        }
    }
    I2cSetActiveFound(ES7210_ADDR, PSTR("ES7210"), bus);
    *twi_bus = bus;

    const uint8_t MIC_ALL = ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2 | ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4;

    // --- es7210_adc_init (SLAVE) ---
    ret_val |= pes7210_write_reg(ES7210_RESET_REG00, 0xff);
    ret_val |= pes7210_write_reg(ES7210_RESET_REG00, 0x41);
    ret_val |= pes7210_write_reg(ES7210_CLOCK_OFF_REG01, 0x1f);
    ret_val |= pes7210_write_reg(ES7210_TIME_CONTROL0_REG09, 0x30);
    ret_val |= pes7210_write_reg(ES7210_TIME_CONTROL1_REG0A, 0x30);
    // slave mode: no MODE_CONFIG/MASTER_CLK writes
    ret_val |= pes7210_write_reg(ES7210_ANALOG_REG40, 0xC3);
    ret_val |= pes7210_write_reg(ES7210_MIC12_BIAS_REG41, 0x70);
    ret_val |= pes7210_write_reg(ES7210_MIC34_BIAS_REG42, 0x70);
    ret_val |= pes7210_write_reg(ES7210_OSR_REG07, 0x20);
    ret_val |= pes7210_write_reg(ES7210_MAINCLK_REG02, 0xc1);
    // config_sample(16 kHz @ 4.096 MHz): reg02=0xC1, osr=0x20, lrck_h=0x01, lrck_l=0x00
    ret_val |= pes7210_write_reg(ES7210_MAINCLK_REG02, 0xC1);
    ret_val |= pes7210_write_reg(ES7210_OSR_REG07, 0x20);
    ret_val |= pes7210_write_reg(ES7210_LRCK_DIVH_REG04, 0x01);
    ret_val |= pes7210_write_reg(ES7210_LRCK_DIVL_REG05, 0x00);
    ret_val |= pes7210_mic_select(MIC_ALL);

    // --- es7210_adc_config_i2s: 16-bit + I2S normal ---
    uint8_t iface = pes7210_read_reg(ES7210_SDP_INTERFACE1_REG11) & 0x1f;
    ret_val |= pes7210_write_reg(ES7210_SDP_INTERFACE1_REG11, iface | 0x60);   // 16 bits
    iface = pes7210_read_reg(ES7210_SDP_INTERFACE1_REG11) & 0xfc;
    ret_val |= pes7210_write_reg(ES7210_SDP_INTERFACE1_REG11, iface);          // I2S normal fmt
    ret_val |= pes7210_write_reg(ES7210_SDP_INTERFACE2_REG12, 0x00);           // ADC1/2->SDOUT1, ADC3/4->SDOUT2

    // --- gains: MIC1/2 = +37.5 dB, MIC3/4 = 0 dB ---
    ret_val |= pes7210_set_gain(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2, ES7210_GAIN_37_5DB);
    ret_val |= pes7210_set_gain(ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4, ES7210_GAIN_0DB);

    // --- es7210_adc_ctrl_state(START) -> es7210_start(regv) ---
    uint8_t regv = pes7210_read_reg(ES7210_CLOCK_OFF_REG01);
    ret_val |= pes7210_write_reg(ES7210_CLOCK_OFF_REG01, regv);
    ret_val |= pes7210_write_reg(ES7210_POWER_DOWN_REG06, 0x00);
    ret_val |= pes7210_write_reg(ES7210_MIC1_POWER_REG47, 0x00);
    ret_val |= pes7210_write_reg(ES7210_MIC2_POWER_REG48, 0x00);
    ret_val |= pes7210_write_reg(ES7210_MIC3_POWER_REG49, 0x00);
    ret_val |= pes7210_write_reg(ES7210_MIC4_POWER_REG4A, 0x00);
    ret_val |= pes7210_mic_select(MIC_ALL);

    return ret_val;
}
