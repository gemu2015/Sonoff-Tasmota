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

/*
 * BinPlugin port of the ES8311 DAC codec for the I2SAUDIO plugin (CODEC 3).
 * Mirrors the es8156 plugin driver (p_es8156_c.h) style: every entry is a
 * MODULE_PART, all I2C goes through the I2C_* macros, the bus is auto-scanned.
 * The firmware reference is xdrv_42_4_i2s_codecs.ino ES8311_init() +
 * lib/lib_deprecated/es8311. The sample-rate clock coefficients are HARDCODED
 * for 16 kHz / MCLK=256*fs to avoid pulling a PROGMEM coeff table into the
 * plugin (PROGMEM arrays need EXEC_OFFSET fix-ups here). 16 kHz init is the
 * same fixed config the firmware S3-Box path uses; in slave mode the DAC then
 * follows the ESP's LRCK for other rates.
 */

#ifndef _P_ES8311_H
#define _P_ES8311_H

#define ES8311_ADDR         0x18

/* ES8311 register map (subset used by init) */
#define ES8311_RESET_REG00              0x00
#define ES8311_CLK_MANAGER_REG01        0x01
#define ES8311_CLK_MANAGER_REG02        0x02
#define ES8311_CLK_MANAGER_REG03        0x03
#define ES8311_CLK_MANAGER_REG04        0x04
#define ES8311_CLK_MANAGER_REG05        0x05
#define ES8311_CLK_MANAGER_REG06        0x06
#define ES8311_CLK_MANAGER_REG07        0x07
#define ES8311_CLK_MANAGER_REG08        0x08
#define ES8311_SDPIN_REG09              0x09
#define ES8311_SDPOUT_REG0A             0x0A
#define ES8311_SYSTEM_REG0B             0x0B
#define ES8311_SYSTEM_REG0C             0x0C
#define ES8311_SYSTEM_REG0D             0x0D
#define ES8311_SYSTEM_REG0E             0x0E
#define ES8311_SYSTEM_REG10             0x10
#define ES8311_SYSTEM_REG11             0x11
#define ES8311_SYSTEM_REG12             0x12
#define ES8311_SYSTEM_REG13             0x13
#define ES8311_SYSTEM_REG14             0x14
#define ES8311_ADC_REG15                0x15
#define ES8311_ADC_REG16                0x16
#define ES8311_ADC_REG17                0x17
#define ES8311_ADC_REG1B                0x1B
#define ES8311_ADC_REG1C                0x1C
#define ES8311_DAC_REG31                0x31
#define ES8311_DAC_REG32                0x32
#define ES8311_DAC_REG37                0x37
#define ES8311_GP_REG45                 0x45

/*
 * @brief Initialize the ES8311 DAC as I2S slave and start the DAC path.
 * @param amode    AUDIO_HAL_MODE_SLAVE / _MASTER
 * @param bits     AUDIO_HAL_BIT_LENGTH_16BITS / _24 / _32
 * @param twi_bus  out: the I2C bus (0/1) the codec was found on
 * @return ESP_OK, or -1 if no ES8311 answers on either bus
 */
MODULE_PART int32_t pes8311_codec_init(uint8_t amode, uint8_t bits, uint8_t *twi_bus);

/* Set DAC output volume, 0..100 (%). */
MODULE_PART esp_err_t pes8311_codec_set_voice_volume(uint8_t volume);

/* Mute(1) / unmute(0) the DAC. */
MODULE_PART esp_err_t pes8311_codec_set_voice_mute(bool enable);

#endif
