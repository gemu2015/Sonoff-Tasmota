/**
 *  @filename   :   epd4in2.h
 *  @brief      :   Header file for Dual-color e-paper library epd4in2.cpp
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

#ifndef EPD4IN7_H
#define EPD4IN7_H

#include <renderer.h>

#define DISPLAY_INIT_MODE 0
#define DISPLAY_INIT_PARTIAL 1
#define DISPLAY_INIT_FULL 2

/* Config Reggister Control */
#define CFG_DATA GPIO_NUM_23
#define CFG_CLK GPIO_NUM_18
#define CFG_STR GPIO_NUM_0

/* Control Lines */
#define CKV GPIO_NUM_25
#define STH GPIO_NUM_26

/* Edges */
#define CKH GPIO_NUM_5

/* Data Lines */
#define D7 GPIO_NUM_22
#define D6 GPIO_NUM_21
#define D5 GPIO_NUM_27
#define D4 GPIO_NUM_2
#define D3 GPIO_NUM_19
#define D2 GPIO_NUM_4
#define D1 GPIO_NUM_32
#define D0 GPIO_NUM_33

#define CLEAR_BYTE 0B10101010
#define DARK_BYTE 0B01010101

// Display resolution
#define EPD47_WIDTH   960
#define EPD47_HEIGHT  540

#define EPD_LINE_BYTES EPD47_WIDTH / 4

typedef struct {
  bool ep_latch_enable : 1;
  bool power_disable : 1;
  bool pos_power_enable : 1;
  bool neg_power_enable : 1;
  bool ep_stv : 1;
  bool ep_scan_direction : 1;
  bool ep_mode : 1;
  bool ep_output_enable : 1;
} epd_config_register_t;

typedef struct {
    /// Horizontal position.
    int x;
    /// Vertical position.
    int y;
    /// Area / image width, must be positive.
    int width;
    /// Area / image height, must be positive.
    int height;
} Rect_t;

/// The image drawing mode.
enum DrawMode {
    /// Draw black / grayscale image on a white display.
    BLACK_ON_WHITE = 1 << 0,
    /// "Draw with white ink" on a white display.
    WHITE_ON_WHITE = 1 << 1,
    /// Draw with white ink on a black display.
    WHITE_ON_BLACK = 1 << 2,
};

typedef struct {
    uint8_t *data_ptr;
    SemaphoreHandle_t done_smphr;
    Rect_t area;
    int frame;
    DrawMode mode;
} OutputParams;

class Epd47 : public Renderer  {
public:
    Epd47(int16_t width, int16_t height);
    int  Init(void);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void epd_poweron(void);
    void epd_poweroff(void);
    void epd_poweroff_all(void);
    void fillScreen(uint16_t color);
    void Updateframe();
    void provide_out(OutputParams *params);
    void feed_display(OutputParams *params);
    void DisplayInit(int8_t p,int8_t size,int8_t rot,int8_t font);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
private:
  int16_t width;
  int16_t height;
  void busy_delay(uint32_t cycles);
  void push_cfg_bit(bool bit);
  void push_cfg(epd_config_register_t *cfg);
  void epd_base_init(uint32_t epd_row_width);
  void epd_clear_area(Rect_t area);
  void epd_clear_area_cycles(Rect_t area, int cycles, int cycle_time);
  void epd_push_pixels(Rect_t area, short time, int color);
  void reorder_line_buffer(uint32_t *line_data);
  void write_row(uint32_t output_time_dus);
  void skip_row(uint8_t pipeline_finish_time);
  void epd_draw_image(Rect_t area, uint8_t *data, enum DrawMode mode);
  void reset_lut(uint8_t *lut_mem, enum DrawMode mode);
  void update_LUT(uint8_t *lut_mem, uint8_t k, enum DrawMode mode);
  void calc_epd_input_4bpp(uint32_t *line_data, uint8_t *epd_input, uint8_t k, uint8_t *conversion_lut);
  void nibble_shift_buffer_right(uint8_t *buf, uint32_t len);
  void bit_shift_buffer_right(uint8_t *buf, uint32_t len, int shift);



  void epd_start_frame();
  void latch_row();
  void epd_skip();
  void epd_output_row(uint32_t output_time_dus);
  void epd_end_frame();
  void epd_switch_buffer();
  uint8_t *epd_get_current_buffer();
  uint32_t skipping;
  uint8_t *conversion_lut;
  QueueHandle_t output_queue;
  epd_config_register_t config_reg;
};

static Epd47 *epd47p;

#endif /* EPD4IN7_H */
