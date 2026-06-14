/*
 * ES7210 4-channel ADC — register map + types for the I2SAUDIO BinPlugin.
 *
 * Trimmed copy of lib/lib_deprecated/es7210/src/es7210.h: just the I2C address,
 * register defines and the mic/gain enums. The TwoWire-based API there is NOT
 * used by the plugin — the plugin talks I2C through the I2C_* binding macros in
 * p_es7210_c.h, mirroring es7243e (the other ADC) and es8311.
 */

#ifndef _P_ES7210_H
#define _P_ES7210_H

typedef enum {
    ES7210_AD1_AD0_00 = 0x40,
    ES7210_AD1_AD0_01 = 0x41,
    ES7210_AD1_AD0_10 = 0x42,
    ES7210_AD1_AD0_11 = 0x43,
} es7210_address_t;

/* ES7210 I2C address (AD1=AD0=0) */
#define ES7210_ADDR                   ES7210_AD1_AD0_00

#define  ES7210_RESET_REG00                 0x00        /* Reset control */
#define  ES7210_CLOCK_OFF_REG01             0x01        /* Used to turn off the ADC clock */
#define  ES7210_MAINCLK_REG02               0x02        /* Set ADC clock frequency division */
#define  ES7210_MASTER_CLK_REG03            0x03        /* MCLK source $ SCLK division */
#define  ES7210_LRCK_DIVH_REG04             0x04        /* lrck_divh */
#define  ES7210_LRCK_DIVL_REG05             0x05        /* lrck_divl */
#define  ES7210_POWER_DOWN_REG06            0x06        /* power down */
#define  ES7210_OSR_REG07                   0x07
#define  ES7210_MODE_CONFIG_REG08           0x08        /* Set master/slave & channels */
#define  ES7210_TIME_CONTROL0_REG09         0x09        /* Set Chip intial state period*/
#define  ES7210_TIME_CONTROL1_REG0A         0x0A        /* Set Power up state period */
#define  ES7210_SDP_INTERFACE1_REG11        0x11        /* Set sample & fmt */
#define  ES7210_SDP_INTERFACE2_REG12        0x12        /* Pins state */
#define  ES7210_ADC_AUTOMUTE_REG13          0x13        /* Set mute */
#define  ES7210_ADC34_MUTERANGE_REG14       0x14        /* Set mute range */
#define  ES7210_ANALOG_REG40                0x40        /* ANALOG Power */
#define  ES7210_MIC12_BIAS_REG41            0x41
#define  ES7210_MIC34_BIAS_REG42            0x42
#define  ES7210_MIC1_GAIN_REG43             0x43
#define  ES7210_MIC2_GAIN_REG44             0x44
#define  ES7210_MIC3_GAIN_REG45             0x45
#define  ES7210_MIC4_GAIN_REG46             0x46
#define  ES7210_MIC1_POWER_REG47            0x47
#define  ES7210_MIC2_POWER_REG48            0x48
#define  ES7210_MIC3_POWER_REG49            0x49
#define  ES7210_MIC4_POWER_REG4A            0x4A
#define  ES7210_MIC12_POWER_REG4B           0x4B        /* MICBias & ADC & PGA Power */
#define  ES7210_MIC34_POWER_REG4C           0x4C

typedef enum {
    ES7210_INPUT_MIC1 = 0x01,
    ES7210_INPUT_MIC2 = 0x02,
    ES7210_INPUT_MIC3 = 0x04,
    ES7210_INPUT_MIC4 = 0x08
} es7210_input_mics_t;

/* gain codes: GAIN_0DB=0 .. GAIN_37_5DB=14 (4-bit reg field) */
#define ES7210_GAIN_0DB     0
#define ES7210_GAIN_30DB    10
#define ES7210_GAIN_37_5DB  14

// Plugin entry — defined in p_es7210_c.h; prototype here so the codec switch in
// xdrv_42_i2s.cpp (included before p_es7210_c.h) sees it (mirrors es7243e.h).
MODULE_PART uint32_t pes7210_codec_init(uint8_t *twi_bus);

#endif /* _P_ES7210_H */
