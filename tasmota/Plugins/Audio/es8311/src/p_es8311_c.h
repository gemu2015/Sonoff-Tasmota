/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

// audio_hal.h / esxxx_common.h are already included by the host TU before this
// file (via the es8156 headers). Include explicitly too so the relative paths
// stay self-documenting; their guards make the re-include a no-op.
#include "../../es8156/src/audio_hal.h"
#include "es8311.h"

MODULE_PART esp_err_t pes8311_write_reg(uint8_t reg_addr, uint8_t data) {
    SETMEMREGS
    I2C_beginTransmission(ES8311_ADDR);
    I2C_write(reg_addr);
    I2C_write(data);
    I2C_endTransmission(true);
    return 0;
}

MODULE_PART uint8_t pes8311_read_reg(uint8_t reg_addr) {
    SETMEMREGS
    uint8_t data;
    I2C_beginTransmission(ES8311_ADDR);
    I2C_write(reg_addr);
    I2C_endTransmission(false);
    I2C_requestFrom(ES8311_ADDR, (size_t)1);
    data = I2C_read();
    return data;
}

// Full ES8311 bring-up: clock manager + I2S slave + DAC start + unmute.
// Ported from lib/lib_deprecated/es8311 es8311_codec_init + es8311_set_bits_per_sample
// + es8311_config_fmt(NORMAL) + es8311_start(ES_MODULE_DAC) + es8311_mute(0).
// Clock coefficients fixed for 16 kHz @ MCLK = 256*fs (= 4.096 MHz):
//   pre_div=1 pre_multi=1 adc_div=1 dac_div=1 fs_mode=0 lrck_h=0x00 lrck_l=0xff
//   bclk_div=4 adc_osr=0x10 dac_osr=0x10
MODULE_PART int32_t pes8311_codec_init(uint8_t amode, uint8_t bits, uint8_t *twi_bus) {
    SETMEMREGS

    uint8_t bus = 0;
    I2C_SETWIRE(bus);
    if (!I2cSetDevice(ES8311_ADDR, bus)) {
        bus++;
        I2C_SETWIRE(bus);
        if (!I2cSetDevice(ES8311_ADDR, bus)) {
            return -1;
        }
    }

    *twi_bus = bus;
    I2cSetActiveFound(ES8311_ADDR, PSTR("ES8311"), bus);

    uint8_t regv;

    // --- reset + clock manager preset ---
    pes8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x30);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG02, 0x00);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG03, 0x10);
    pes8311_write_reg(ES8311_ADC_REG16, 0x24);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG04, 0x10);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG05, 0x00);
    pes8311_write_reg(ES8311_SYSTEM_REG0B, 0x00);
    pes8311_write_reg(ES8311_SYSTEM_REG0C, 0x00);
    pes8311_write_reg(ES8311_SYSTEM_REG10, 0x1F);
    pes8311_write_reg(ES8311_SYSTEM_REG11, 0x7F);
    pes8311_write_reg(ES8311_RESET_REG00, 0x80);

    // --- master/slave audio interface ---
    regv = pes8311_read_reg(ES8311_RESET_REG00);
    if (AUDIO_HAL_MODE_MASTER == amode) {
        regv |= 0x40;
    } else {
        regv &= 0xBF;       // slave
    }
    pes8311_write_reg(ES8311_RESET_REG00, regv);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x3F);

    // mclk from MCLK pin
    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG01);
    regv &= 0x7F;
    pes8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);

    // --- clock parameters (hardcoded 16 kHz coeff) ---
    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG02) & 0x07;
    regv |= (1 - 1) << 5;            // pre_div = 1
    regv |= (0) << 3;               // pre_multi = 1 -> datmp 0
    pes8311_write_reg(ES8311_CLK_MANAGER_REG02, regv);

    regv = 0;                        // read(REG05)&0x00
    regv |= (1 - 1) << 4;           // adc_div = 1
    regv |= (1 - 1) << 0;           // dac_div = 1
    pes8311_write_reg(ES8311_CLK_MANAGER_REG05, regv);

    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG03) & 0x80;
    regv |= (0) << 6;               // fs_mode = 0
    regv |= 0x10;                    // adc_osr
    pes8311_write_reg(ES8311_CLK_MANAGER_REG03, regv);

    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG04) & 0x80;
    regv |= 0x10;                    // dac_osr
    pes8311_write_reg(ES8311_CLK_MANAGER_REG04, regv);

    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG07) & 0xC0;
    regv |= 0x00;                    // lrck_h
    pes8311_write_reg(ES8311_CLK_MANAGER_REG07, regv);

    regv = 0;                        // read(REG08)&0x00
    regv |= 0xff;                    // lrck_l
    pes8311_write_reg(ES8311_CLK_MANAGER_REG08, regv);

    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG06) & 0xE0;
    regv |= (4 - 1) << 0;           // bclk_div = 4 (<19)
    pes8311_write_reg(ES8311_CLK_MANAGER_REG06, regv);

    // mclk / sclk not inverted
    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG01);
    regv &= ~(0x40);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);
    regv = pes8311_read_reg(ES8311_CLK_MANAGER_REG06);
    regv &= ~(0x20);
    pes8311_write_reg(ES8311_CLK_MANAGER_REG06, regv);

    pes8311_write_reg(ES8311_SYSTEM_REG13, 0x10);
    pes8311_write_reg(ES8311_ADC_REG1B, 0x0A);
    pes8311_write_reg(ES8311_ADC_REG1C, 0x6A);

    // --- bits per sample ---
    uint8_t dac_iface = pes8311_read_reg(ES8311_SDPIN_REG09);
    uint8_t adc_iface = pes8311_read_reg(ES8311_SDPOUT_REG0A);
    switch (bits) {
        case AUDIO_HAL_BIT_LENGTH_16BITS: dac_iface |= 0x0c; adc_iface |= 0x0c; break;
        case AUDIO_HAL_BIT_LENGTH_24BITS: break;
        case AUDIO_HAL_BIT_LENGTH_32BITS: dac_iface |= 0x10; adc_iface |= 0x10; break;
        default:                          dac_iface |= 0x0c; adc_iface |= 0x0c; break;
    }
    pes8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    pes8311_write_reg(ES8311_SDPOUT_REG0A, adc_iface);

    // --- I2S normal format ---
    dac_iface = pes8311_read_reg(ES8311_SDPIN_REG09) & 0xFC;
    adc_iface = pes8311_read_reg(ES8311_SDPOUT_REG0A) & 0xFC;
    pes8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    pes8311_write_reg(ES8311_SDPOUT_REG0A, adc_iface);

    // --- start ADC + DAC (ES_MODULE_ADC_DAC: speaker out + mic in) ---
    // The ES8311 is a full-duplex mono codec on one I2S bus. The firmware's
    // S3-Box path starts DAC-only (its mic is a separate ES7210); but the P4
    // Nano's mic IS this codec's ADC (ASDOUT → ESP DIN), so enable BOTH serial
    // ports. BIT(6) set = that path muted/tri-stated; clear = enabled.
    dac_iface = pes8311_read_reg(ES8311_SDPIN_REG09) & 0xBF;
    adc_iface = pes8311_read_reg(ES8311_SDPOUT_REG0A) & 0xBF;
    adc_iface &= ~(BIT(6));          // enable ADC serial output (mic → ASDOUT → DIN)
    dac_iface &= ~(BIT(6));          // enable DAC serial input (speaker)
    pes8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    pes8311_write_reg(ES8311_SDPOUT_REG0A, adc_iface);

    pes8311_write_reg(ES8311_ADC_REG17, 0xBF);
    pes8311_write_reg(ES8311_SYSTEM_REG0E, 0x02);
    pes8311_write_reg(ES8311_SYSTEM_REG12, 0x00);
    pes8311_write_reg(ES8311_SYSTEM_REG14, 0x1A);
    regv = pes8311_read_reg(ES8311_SYSTEM_REG14);
    regv &= ~(0x40);                 // IS_DMIC = 0
    pes8311_write_reg(ES8311_SYSTEM_REG14, regv);
    pes8311_write_reg(ES8311_SYSTEM_REG0D, 0x01);
    pes8311_write_reg(ES8311_ADC_REG15, 0x40);
    pes8311_write_reg(ES8311_DAC_REG37, 0x48);
    pes8311_write_reg(ES8311_GP_REG45, 0x00);

    // --- unmute ---
    regv = pes8311_read_reg(ES8311_DAC_REG31) & 0x9f;
    pes8311_write_reg(ES8311_DAC_REG31, regv);
    pes8311_write_reg(ES8311_SYSTEM_REG12, 0x00);

    return ESP_OK;
}

MODULE_PART esp_err_t pes8311_codec_set_voice_volume(uint8_t volume) {
    SETMEMREGS
    if (volume > 100) {
        volume = 100;
    }
    uint8_t vol = (uint8_t)(((uint32_t)volume * 2550) / 1000);   // 0..100 -> 0..255
    pes8311_write_reg(ES8311_DAC_REG32, vol);
    return ESP_OK;
}

MODULE_PART esp_err_t pes8311_codec_set_voice_mute(bool enable) {
    SETMEMREGS
    uint8_t regv = pes8311_read_reg(ES8311_DAC_REG31) & 0x9f;
    if (enable) {
        pes8311_write_reg(ES8311_SYSTEM_REG12, 0x02);
        pes8311_write_reg(ES8311_DAC_REG31, regv | 0x60);
        pes8311_write_reg(ES8311_DAC_REG32, 0x00);
        pes8311_write_reg(ES8311_DAC_REG37, 0x08);
    } else {
        pes8311_write_reg(ES8311_DAC_REG31, regv);
        pes8311_write_reg(ES8311_SYSTEM_REG12, 0x00);
    }
    return ESP_OK;
}
