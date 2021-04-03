/**
 *  @filename   :   epd4in2.cpp
 *  @brief      :   Implements for Dual-color e-paper library
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <epd4in7.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_attr.h>
#include <esp_assert.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_types.h>
#include <xtensa/core-macros.h>
#include <driver/gpio.h>
#include "i2s_data_bus.h"
#include "rmt_pulse.h"

extern uint8_t *buffer;
uint8_t epd42_mode;

Epd47::Epd47(int16_t dwidth, int16_t dheight) :  Renderer(dwidth, dheight) {
  width = dwidth;
  height = dheight;
}

int32_t Epd47::Init(void) {
  skipping = 0;
  epd_base_init(width);

  conversion_lut = (uint8_t *)heap_caps_malloc(1 << 16, MALLOC_CAP_8BIT);
  assert(conversion_lut != NULL);
  output_queue = xQueueCreate(64, width / 2);

  epd47p = this;

  return 0;
}

void Epd47::DisplayInit(int8_t p, int8_t size, int8_t rot, int8_t font) {

  if (p ==  DISPLAY_INIT_FULL) {
    epd_poweron();
    Rect_t area = {.x = 0, .y = 0, .width = width, .height = height};
    epd_clear_area(area);
    epd_poweroff();
  }
  setRotation(rot);
  setTextWrap(false);
  cp437(true);
  setTextFont(font);
  setTextSize(size);
  setTextColor(15,0);
  setCursor(0,0);
  fillScreen(15);
}

void IRAM_ATTR Epd47::busy_delay(uint32_t cycles) {
  volatile unsigned long counts = XTHAL_GET_CCOUNT() + cycles;
  while (XTHAL_GET_CCOUNT() < counts) {
  };
}

#define fast_gpio_set_hi(A) GPIO.out_w1ts = (1 << A)
#define fast_gpio_set_lo(A) GPIO.out_w1tc = (1 << A)


void IRAM_ATTR Epd47::push_cfg_bit(bool bit) {
  fast_gpio_set_lo(CFG_CLK);
  if (bit) {
    fast_gpio_set_hi(CFG_DATA);
  } else {
    fast_gpio_set_lo(CFG_DATA);
  }
  fast_gpio_set_hi(CFG_CLK);
}

void IRAM_ATTR Epd47::push_cfg(epd_config_register_t *cfg) {
  fast_gpio_set_lo(CFG_STR);

  // push config bits in reverse order
  push_cfg_bit(cfg->ep_output_enable);
  push_cfg_bit(cfg->ep_mode);
  push_cfg_bit(cfg->ep_scan_direction);
  push_cfg_bit(cfg->ep_stv);

  push_cfg_bit(cfg->neg_power_enable);
  push_cfg_bit(cfg->pos_power_enable);
  push_cfg_bit(cfg->power_disable);
  push_cfg_bit(cfg->ep_latch_enable);

  fast_gpio_set_hi(CFG_STR);
}

void Epd47::epd_base_init(uint32_t epd_row_width) {

  config_reg.ep_latch_enable = false;
  config_reg.power_disable = true;
  config_reg.pos_power_enable = false;
  config_reg.neg_power_enable = false;
  config_reg.ep_stv = true;
  config_reg.ep_scan_direction = true;
  config_reg.ep_mode = false;
  config_reg.ep_output_enable = false;

  /* Power Control Output/Off */
  gpio_set_direction(CFG_DATA, GPIO_MODE_OUTPUT);
  gpio_set_direction(CFG_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction(CFG_STR, GPIO_MODE_OUTPUT);
  fast_gpio_set_lo(CFG_STR);

  push_cfg(&config_reg);

  // Setup I2S
  i2s_bus_config i2s_config;
  // add an offset off dummy bytes to allow for enough timing headroom
  i2s_config.epd_row_width = epd_row_width + 32;
  i2s_config.clock = CKH;
  i2s_config.start_pulse = STH;
  i2s_config.data_0 = D0;
  i2s_config.data_1 = D1;
  i2s_config.data_2 = D2;
  i2s_config.data_3 = D3;
  i2s_config.data_4 = D4;
  i2s_config.data_5 = D5;
  i2s_config.data_6 = D6;
  i2s_config.data_7 = D7;

  i2s_bus_init(&i2s_config);

  rmt_pulse_init(CKV);
}

void Epd47::epd_poweron() {
  // POWERON
  config_reg.ep_scan_direction = true;
  config_reg.power_disable = false;
  push_cfg(&config_reg);
  busy_delay(100 * 240);
  config_reg.neg_power_enable = true;
  push_cfg(&config_reg);
  busy_delay(500 * 240);
  config_reg.pos_power_enable = true;
  push_cfg(&config_reg);
  busy_delay(100 * 240);
  config_reg.ep_stv = true;
  push_cfg(&config_reg);
  fast_gpio_set_hi(STH);
  // END POWERON
}

void Epd47::epd_poweroff() {
  // POWEROFF
  config_reg.pos_power_enable = false;
  push_cfg(&config_reg);
  busy_delay(10 * 240);
  config_reg.neg_power_enable = false;
  push_cfg(&config_reg);
  busy_delay(100 * 240);
  config_reg.power_disable = true;
  push_cfg(&config_reg);

  config_reg.ep_stv = false;
  push_cfg(&config_reg);

//   config_reg.ep_scan_direction = false;
//   push_cfg(&config_reg);

  // END POWEROFF
}

void Epd47::epd_poweroff_all() {
    memset(&config_reg, 0, sizeof(config_reg));
    push_cfg(&config_reg);
}

void Epd47::epd_start_frame() {
  while (i2s_is_busy() || rmt_busy()) {
  };
  config_reg.ep_mode = true;
  push_cfg(&config_reg);

  pulse_ckv_us(1, 1, true);

  // This is very timing-sensitive!
  config_reg.ep_stv = false;
  push_cfg(&config_reg);
  busy_delay(240);
  pulse_ckv_us(10, 10, false);
  config_reg.ep_stv = true;
  push_cfg(&config_reg);
  pulse_ckv_us(0, 10, true);

  config_reg.ep_output_enable = true;
  push_cfg(&config_reg);

  pulse_ckv_us(1, 1, true);
}

void Epd47::latch_row() {
  config_reg.ep_latch_enable = true;
  push_cfg(&config_reg);

  config_reg.ep_latch_enable = false;
  push_cfg(&config_reg);
}

void IRAM_ATTR Epd47::epd_skip() {
#if defined(CONFIG_EPD_DISPLAY_TYPE_ED097TC2)
  pulse_ckv_ticks(2, 2, false);
#else
  // According to the spec, the OC4 maximum CKV frequency is 200kHz.
  pulse_ckv_ticks(45, 5, false);
#endif
}

void IRAM_ATTR Epd47::epd_output_row(uint32_t output_time_dus) {

  while (i2s_is_busy() || rmt_busy()) {
  };
  latch_row();

  pulse_ckv_ticks(output_time_dus, 50, false);

  i2s_start_line_output();
  i2s_switch_buffer();
}

void Epd47::epd_end_frame() {
  config_reg.ep_output_enable = false;
  push_cfg(&config_reg);
  config_reg.ep_mode = false;
  push_cfg(&config_reg);
  pulse_ckv_us(1, 1, true);
  pulse_ckv_us(1, 1, true);
}

void IRAM_ATTR Epd47::epd_switch_buffer() { i2s_switch_buffer(); }

uint8_t IRAM_ATTR *Epd47::epd_get_current_buffer() {
  return (uint8_t *)i2s_get_current_buffer();
};

void Epd47::fillScreen(uint16_t color) {
  color &= 0xf;
  uint8_t icol = (color << 4) | color;
  memset(buffer, icol, width * height / 2);
}

void Epd47::epd_clear_area(Rect_t area) {
    epd_clear_area_cycles(area, 4, 50);
}

void Epd47::epd_clear_area_cycles(Rect_t area, int cycles, int cycle_time) {
    const short white_time = cycle_time;
    const short dark_time = cycle_time;

    for (int c = 0; c < cycles; c++) {
        for (int i = 0; i < 4; i++) {
            epd_push_pixels(area, dark_time, 0);
        }
        for (int i = 0; i < 4; i++) {
            epd_push_pixels(area, white_time, 1);
        }
    }
}

void Epd47::reorder_line_buffer(uint32_t *line_data) {
    for (uint32_t i = 0; i < EPD_LINE_BYTES / 4; i++) {
        uint32_t val = *line_data;
        *(line_data++) = val >> 16 | ((val & 0x0000FFFF) << 16);
    }
}

void Epd47::write_row(uint32_t output_time_dus) {
    // avoid too light output after skipping on some displays
    if (skipping) {
        //vTaskDelay(20);
    }
    skipping = 0;
    epd_output_row(output_time_dus);
}

// skip a display row
void Epd47::skip_row(uint8_t pipeline_finish_time) {
    // output previously loaded row, fill buffer with no-ops.
    if (skipping == 0) {
        epd_switch_buffer();
        memset(epd_get_current_buffer(), 0, EPD_LINE_BYTES);
        epd_switch_buffer();
        memset(epd_get_current_buffer(), 0, EPD_LINE_BYTES);
        epd_output_row(pipeline_finish_time);
        // avoid tainting of following rows by
        // allowing residual charge to dissipate
        //vTaskDelay(10);
        /*
        unsigned counts = XTHAL_GET_CCOUNT() + 50 * 240;
        while (XTHAL_GET_CCOUNT() < counts) {
        };
        */
    } else if (skipping < 2) {
        epd_output_row(10);
    } else {
        //epd_output_row(5);
        epd_skip();
    }
    skipping++;
}

void Epd47::epd_push_pixels(Rect_t area, short time, int color) {

    uint8_t row[EPD_LINE_BYTES] = {0};

    for (uint32_t i = 0; i < area.width; i++) {
        uint32_t position = i + area.x % 4;
        uint8_t mask =
            (color ? CLEAR_BYTE : DARK_BYTE) & (0b00000011 << (2 * (position % 4)));
        row[area.x / 4 + position / 4] |= mask;
    }
    reorder_line_buffer((uint32_t *)row);

    epd_start_frame();

    for (int i = 0; i < height; i++) {
        // before are of interest: skip
        if (i < area.y) {
            skip_row(time);
            // start area of interest: set row data
        } else if (i == area.y) {
            epd_switch_buffer();
            memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);
            epd_switch_buffer();
            memcpy(epd_get_current_buffer(), row, EPD_LINE_BYTES);

            write_row(time * 10);
            // load nop row if done with area
        } else if (i >= area.y + area.height) {
            skip_row(time);
            // output the same as before
        } else {
            write_row(time * 10);
        }
    }
    // Since we "pipeline" row output, we still have to latch out the last row.
    write_row(time * 10);

    epd_end_frame();
}


void Epd47::Updateframe(void) {
  epd_poweron();
  Rect_t area = {.x = 0, .y = 0, .width = width, .height = height};
  epd_draw_image(area, buffer, BLACK_ON_WHITE);
  epd_poweroff();
}

void IRAM_ATTR Epd47::provide_out(OutputParams *params) {
    uint8_t line[width / 2];
    memset(line, 255, width / 2);
    Rect_t area = params->area;
    uint8_t *ptr = params->data_ptr;

    if (params->frame == 0) {
        reset_lut(conversion_lut, params->mode);
    }

    update_LUT(conversion_lut, params->frame, params->mode);

    if (area.x < 0) {
        ptr += -area.x / 2;
    }
    if (area.y < 0) {
        ptr += (area.width / 2 + area.width % 2) * -area.y;
    }

    for (int i = 0; i < height; i++) {
        if (i < area.y || i >= area.y + area.height) {
            continue;
        }

        uint32_t *lp;
        bool shifted = false;
        if (area.width == width && area.x == 0) {
            lp = (uint32_t *)ptr;
            ptr += width / 2;
        } else {
            uint8_t *buf_start = (uint8_t *)line;
            uint32_t line_bytes = area.width / 2 + area.width % 2;
            if (area.x >= 0) {
                buf_start += area.x / 2;
            } else {
                // reduce line_bytes to actually used bytes
                line_bytes += area.x / 2;
            }
            line_bytes =
                min(line_bytes, width / 2 - (uint32_t)(buf_start - line));
            memcpy(buf_start, ptr, line_bytes);
            ptr += area.width / 2 + area.width % 2;

            // mask last nibble for uneven width
            if (area.width % 2 == 1 && area.x / 2 + area.width / 2 + 1 < width) {
                *(buf_start + line_bytes - 1) |= 0xF0;
            }
            if (area.x % 2 == 1 && area.x < width) {
                shifted = true;
                // shift one nibble to right
                nibble_shift_buffer_right(
                    buf_start, min(line_bytes + 1, (uint32_t)line + width / 2 -
                                   (uint32_t)buf_start));
            }
            lp = (uint32_t *)line;
        }
        xQueueSendToBack(output_queue, lp, portMAX_DELAY);
        if (shifted) {
            memset(line, 255, width / 2);
        }
    }

    xSemaphoreGive(params->done_smphr);
    vTaskDelay(portMAX_DELAY);
}

/* 4bpp Contrast cycles in order of contrast (Darkest first).  */
const int contrast_cycles_4[15] = {30, 30, 20, 20, 30,  30,  30, 40, 40, 50, 50, 50, 100, 200, 300};

const int contrast_cycles_4_white[15] = {10, 10, 8, 8, 8,  8,  8, 10, 10, 15, 15, 20, 20, 100, 300};



void IRAM_ATTR Epd47::feed_display(OutputParams *params)
{
    Rect_t area = params->area;
    const int *contrast_lut = contrast_cycles_4;
    switch (params->mode) {
    case WHITE_ON_WHITE:
    case BLACK_ON_WHITE:
        contrast_lut = contrast_cycles_4;
        break;
    case WHITE_ON_BLACK:
        contrast_lut = contrast_cycles_4_white;
        break;
    }

    epd_start_frame();
    for (int i = 0; i < height; i++) {
        if (i < area.y || i >= area.y + area.height) {
            skip_row(contrast_lut[params->frame]);
            continue;
        }
        uint8_t output[width / 2];
        xQueueReceive(output_queue, output, portMAX_DELAY);
        calc_epd_input_4bpp((uint32_t *)output, epd_get_current_buffer(),
                            params->frame, conversion_lut);
        write_row(contrast_lut[params->frame]);
    }
    if (!skipping) {
        // Since we "pipeline" row output, we still have to latch out the last row.
        write_row(contrast_lut[params->frame]);
    }
    epd_end_frame();

    xSemaphoreGive(params->done_smphr);
    vTaskDelay(portMAX_DELAY);
}

void IRAM_ATTR Epd47::calc_epd_input_4bpp(uint32_t *line_data, uint8_t *epd_input, uint8_t k, uint8_t *conversion_lut) {

    uint32_t *wide_epd_input = (uint32_t *)epd_input;
    uint16_t *line_data_16 = (uint16_t *)line_data;

    // this is reversed for little-endian, but this is later compensated
    // through the output peripheral.
    for (uint32_t j = 0; j < width / 16; j++) {

        uint16_t v1 = *(line_data_16++);
        uint16_t v2 = *(line_data_16++);
        uint16_t v3 = *(line_data_16++);
        uint16_t v4 = *(line_data_16++);
        uint32_t pixel = conversion_lut[v1] << 16 | conversion_lut[v2] << 24 |
                         conversion_lut[v3] | conversion_lut[v4] << 8;
        wide_epd_input[j] = pixel;
    }
}

void IRAM_ATTR Epd47::nibble_shift_buffer_right(uint8_t *buf, uint32_t len)
{
    uint8_t carry = 0xF;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t val = buf[i];
        buf[i] = (val << 4) | carry;
        carry = (val & 0xF0) >> 4;
    }
}

/*
 * bit-shift a buffer `shift` <= 7 bits to the right.
 */
void IRAM_ATTR Epd47::bit_shift_buffer_right(uint8_t *buf, uint32_t len, int shift) {
    uint8_t carry = 0x00;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t val = buf[i];
        buf[i] = (val << shift) | carry;
        carry = val >> (8 - shift);
    }
}

void IRAM_ATTR tprovide_out(OutputParams *params);
void IRAM_ATTR tfeed_display(OutputParams *params);

void IRAM_ATTR tprovide_out(OutputParams *params) {
  epd47p->provide_out(params);
}
void IRAM_ATTR tfeed_display(OutputParams *params) {
  epd47p->feed_display(params);
}

void IRAM_ATTR Epd47::epd_draw_image(Rect_t area, uint8_t *data, DrawMode mode) {
    uint8_t line[width / 2];
    memset(line, 255, width / 2);
    uint8_t frame_count = 15;

    SemaphoreHandle_t fetch_sem = xSemaphoreCreateBinary();
    SemaphoreHandle_t feed_sem = xSemaphoreCreateBinary();
    vTaskDelay(10);
    for (uint8_t k = 0; k < frame_count; k++) {

        OutputParams p1;
        p1.area = area;
        p1.data_ptr = data;
        p1.frame = k;
        p1.mode = mode;
        p1.done_smphr = fetch_sem;

        OutputParams p2;
        p2.area = area;
        p2.data_ptr = data;
        p2.frame = k;
        p2.mode = mode;
        p2.done_smphr = feed_sem;

        TaskHandle_t t1, t2;
        xTaskCreatePinnedToCore((void (*)(void *))tprovide_out, "privide_out", 8000, &p1, 10, &t1, 0);
        xTaskCreatePinnedToCore((void (*)(void *))tfeed_display, "render",     8000, &p2, 10, &t2, 1);

        xSemaphoreTake(fetch_sem, portMAX_DELAY);
        xSemaphoreTake(feed_sem, portMAX_DELAY);

        vTaskDelete(t1);
        vTaskDelete(t2);

        vTaskDelay(5);
    }
    vSemaphoreDelete(fetch_sem);
    vSemaphoreDelete(feed_sem);

}



void IRAM_ATTR Epd47::reset_lut(uint8_t *lut_mem, enum DrawMode mode) {
    switch (mode) {
    case BLACK_ON_WHITE:
        memset(lut_mem, 0x55, (1 << 16));
        break;
    case WHITE_ON_BLACK:
    case WHITE_ON_WHITE:
        memset(lut_mem, 0xAA, (1 << 16));
        break;
    default:
        ESP_LOGW("epd_driver", "unknown draw mode %d!", mode);
        break;
    }
}

void IRAM_ATTR Epd47::update_LUT(uint8_t *lut_mem, uint8_t k, enum DrawMode mode) {
    if (mode == BLACK_ON_WHITE || mode == WHITE_ON_WHITE) {
        k = 15 - k;
    }

    // reset the pixels which are not to be lightened / darkened
    // any longer in the current frame
    for (uint32_t l = k; l < (1 << 16); l += 16) {
        lut_mem[l] &= 0xFC;
    }

    for (uint32_t l = (k << 4); l < (1 << 16); l += (1 << 8)) {
        for (uint32_t p = 0; p < 16; p++) {
            lut_mem[l + p] &= 0xF3;
        }
    }
    for (uint32_t l = (k << 8); l < (1 << 16); l += (1 << 12)) {
        for (uint32_t p = 0; p < (1 << 8); p++) {
            lut_mem[l + p] &= 0xCF;
        }
    }
    for (uint32_t p = (k << 12); p < ((k + 1) << 12); p++) {
        lut_mem[p] &= 0x3F;
    }
}



void Epd47::drawPixel(int16_t x, int16_t y, uint16_t color) {

    if (x < 0 || x >= width) {
        return;
    }
    if (y < 0 || y >= height) {
        return;
    }
    uint8_t *buf_ptr = &buffer[y * width / 2 + x / 2];
    if (x % 2) {
        *buf_ptr = (*buf_ptr & 0x0F) | (color << 4);
    } else {
        *buf_ptr = (*buf_ptr & 0xF0) | (color & 0xf);
    }
}

void Epd47::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  while (h--) {
    drawPixel(x , y , color);
    y++;
  }
}
void Epd47::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  while (w--) {
    drawPixel(x , y , color);
    x++;
  }
}

/* END OF FILE */
