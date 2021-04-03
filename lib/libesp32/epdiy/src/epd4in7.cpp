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
#include "epd_driver.h"
#include "epd_highlevel.h"

#define WAVEFORM EPD_BUILTIN_WAVEFORM

extern uint8_t *buffer;

int temperature;

EpdiyHighlevelState epd_hl_init(const EpdWaveform* waveform);


EpdiyHighlevelState hl;

Epd47::Epd47(int16_t dwidth, int16_t dheight) :  Renderer(dwidth, dheight) {
  width = dwidth;
  height = dheight;
}

int32_t Epd47::Init(void) {
  epd_init(EPD_LUT_1K);
  hl = epd_hl_init(WAVEFORM);
  buffer = epd_hl_get_framebuffer(&hl);

  return 0;
}

void Epd47::DisplayInit(int8_t p, int8_t size, int8_t rot, int8_t font) {

  if (p ==  DISPLAY_INIT_FULL) {
    epd_poweron();
    epd_clear();
    epd_poweroff();
  }
  setRotation(rot);
  setTextWrap(false);
  cp437(true);
  setTextFont(font);
  setTextSize(size);
  setCursor(0,0);
  fillScreen(15);
}

void Epd47::Updateframe() {
  epd_poweron();
  epd_hl_update_screen(&hl, MODE_GL16, temperature);
  epd_poweroff();
}

void Epd47::fillScreen(uint16_t color) {
  color &= 0xf;
   uint8_t icol = (color << 4) | color;
   memset(buffer, icol, width * height / 2);
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
