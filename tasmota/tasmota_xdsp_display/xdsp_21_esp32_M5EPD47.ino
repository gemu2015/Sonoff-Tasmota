/*
  xdsp_21_esp32_M5EPD47.ino - M5Stack e-paper 4.7 inch support for Tasmota

  Copyright (C) 2021  Theo Arends, Gerhard Mutz and M5Stack

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

#ifdef ESP32
#ifdef USE_DISPLAY
#ifdef USE_M5EPD47

#define XDSP_21               21

#define M5EPD47_BLACK  0
#define M5EPD47_WHITE 15

#include <m5epd4in7.h>


M5Epd47 *m5epd47;
bool m5epd47_init_done = false;
extern uint8_t color_type;
extern uint16_t fg_color;
extern uint16_t bg_color;

/*********************************************************************************************/

void M5EpdInitDriver47(void) {

  //if (PinUsed(GPIO_EPD_DATA)) {
  if (TasmotaGlobal.gpio_optiona.udisplay_driver) {
  //  if (1) {

    Settings->display_model = XDSP_21;

    if (Settings->display_width != M5EPD47_WIDTH) {
      Settings->display_width = M5EPD47_WIDTH;
    }
    if (Settings->display_height != M5EPD47_HEIGHT) {
      Settings->display_height = M5EPD47_HEIGHT;
    }

    // init renderer
    m5epd47  = new M5Epd47(Settings->display_width, Settings->display_height);
    m5epd47->Init();

    renderer = m5epd47;
    renderer->DisplayInit(DISPLAY_INIT_MODE, Settings->display_size, Settings->display_rotate, Settings->display_font);
    renderer->setTextColor(M5EPD47_BLACK, M5EPD47_WHITE);

#ifdef SHOW_SPLASH
    // Welcome text
    renderer->setTextFont(2);
    renderer->DrawStringAt(50, 50, "M5Stack 4.7 E-Paper Display!", M5EPD47_BLACK, 0);
    renderer->Updateframe();
#endif

    fg_color = M5EPD47_BLACK;
    bg_color = M5EPD47_WHITE;
    color_type = COLOR_COLOR;

#ifdef USE_TOUCH_BUTTONS
    // start digitizer
    GT911_Touch_Init(&Wire1, -1, -1, M5EPD47_HEIGHT, M5EPD47_WIDTH);
#endif // USE_TOUCH_BUTTONS

    m5epd47_init_done = true;
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: M5 E-Paper 4.7"));
  }
}

/*********************************************************************************************/


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdsp21(uint32_t function)
{
  bool result = false;

  if (FUNC_DISPLAY_INIT_DRIVER == function) {
    M5EpdInitDriver47();
  }
  else if (m5epd47_init_done && (XDSP_21 == Settings->display_model)) {
    switch (function) {
      case FUNC_DISPLAY_MODEL:
        result = true;
        break;
    }
  }
  return result;
}

#endif  // USE_M5EPD47
#endif  // USE_DISPLAY
#endif  // ESP32
