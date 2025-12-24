/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

 #include "es7243e.h"

uint32_t pes7243e_codec_init(uint8_t *twi_bus) {
    SETREGS
    uint32_t ret_val = ESP_OK;

    uint8_t bus = 0;
    I2C_SETWIRE(bus);

    if (!I2cSetDevice(ES7243_ADDR, bus)) {
        bus++;
        I2C_SETWIRE(bus);
        if (!I2cSetDevice(ES7243_ADDR, bus)) {
            return -1;
        }
    }
    I2cSetActiveFound(ES8156_ADDR, PSTR("ES7243"), bus);

    *twi_bus = bus;

    ret_val |= pes7243e_adc_init();

    return ret_val;
}


esp_err_t pes7243e_write_reg(uint8_t reg_addr, uint8_t data) {
SETMEMREGS
    I2C_beginTransmission(ES7243_ADDR);
    I2C_write(reg_addr);
    I2C_write(data);
    I2C_endTransmission(true);
    return 0;
}

esp_err_t pes7243e_adc_init(void) {
SETMEMREGS
    esp_err_t ret = ESP_OK;

    ret |= pes7243e_write_reg(0x01, 0x3A);
    ret |= pes7243e_write_reg(0x00, 0x80);
    ret |= pes7243e_write_reg(0xF9, 0x00);
    ret |= pes7243e_write_reg(0x04, 0x02);
    ret |= pes7243e_write_reg(0x04, 0x01);
    ret |= pes7243e_write_reg(0xF9, 0x01);
    ret |= pes7243e_write_reg(0x00, 0x1E);
    ret |= pes7243e_write_reg(0x01, 0x00);

    ret |= pes7243e_write_reg(0x02, 0x00);
    ret |= pes7243e_write_reg(0x03, 0x20);
    ret |= pes7243e_write_reg(0x04, 0x01);
    ret |= pes7243e_write_reg(0x0D, 0x00);
    ret |= pes7243e_write_reg(0x05, 0x00);
    ret |= pes7243e_write_reg(0x06, 0x03); // SCLK=MCLK/4
    ret |= pes7243e_write_reg(0x07, 0x00); // LRCK=MCLK/256
    ret |= pes7243e_write_reg(0x08, 0xFF); // LRCK=MCLK/256

    ret |= pes7243e_write_reg(0x09, 0xCA);
    ret |= pes7243e_write_reg(0x0A, 0x85);
    ret |= pes7243e_write_reg(0x0B, 0x00);
    ret |= pes7243e_write_reg(0x0E, 0xBF);
    ret |= pes7243e_write_reg(0x0F, 0x80);
    ret |= pes7243e_write_reg(0x14, 0x0C);
    ret |= pes7243e_write_reg(0x15, 0x0C);
    ret |= pes7243e_write_reg(0x17, 0x02);
    ret |= pes7243e_write_reg(0x18, 0x26);
    ret |= pes7243e_write_reg(0x19, 0x77);
    ret |= pes7243e_write_reg(0x1A, 0xF4);
    ret |= pes7243e_write_reg(0x1B, 0x66);
    ret |= pes7243e_write_reg(0x1C, 0x44);
    ret |= pes7243e_write_reg(0x1E, 0x00);
    ret |= pes7243e_write_reg(0x1F, 0x0C);
    ret |= pes7243e_write_reg(0x20, 0x1E); //PGA gain +30dB
    ret |= pes7243e_write_reg(0x21, 0x1E); //PGA gain +30dB

    ret |= pes7243e_write_reg(0x00, 0x80); //Slave  Mode
    ret |= pes7243e_write_reg(0x01, 0x3A);
    ret |= pes7243e_write_reg(0x16, 0x3F);
    ret |= pes7243e_write_reg(0x16, 0x00);
    if (ret) {
        //ESP_LOGE(TAG, "Es7243e initialize failed!");
        return ESP_FAIL;
    }
    return ret;
}


const uint32_t gaintab[] PROGMEM = {0x10, 0x12, 0x20, 0x22, 0x04, 0x40, 0x06, 0x42};

void pes7243e_setgain(uint8_t gain) {
SETMEMREGS
const uint32_t *gaint = GUI32p(gaintab);

  pes7243e_write_reg(0x08,  gaint[gain & 7] | 0x09);
}

