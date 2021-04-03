/*
  xdsp_16_epaper_47.ino -  LILIGO47 e-paper support for Tasmota

  Copyright (C) 2021  Theo Arends, Gerhard Mutz and LILIGO

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


#ifdef USE_DISPLAY
#ifdef USE_LILYGO47

#define XDSP_16                16


#undef COLORED
#define COLORED                15
#undef UNCOLORED
#define UNCOLORED              0


#include <epd4in7.h>

Epd47 *epd47;
bool epd47_init_done = false;
extern uint8_t color_type;

/*********************************************************************************************/

void EpdInitDriver47(void) {

  if (1) {
    Settings.display_model = XDSP_16;

    if (Settings.display_width != EPD47_WIDTH) {
      Settings.display_width = EPD47_WIDTH;
    }
    if (Settings.display_height != EPD47_HEIGHT) {
      Settings.display_height = EPD47_HEIGHT;
    }

    // allocate screen buffer
    if (buffer) free(buffer);
    buffer = (unsigned char*)ps_calloc(sizeof(uint8_t), (Settings.display_width * Settings.display_height) / 2);
    if (!buffer) return;

    memset(buffer, 0xFF, Settings.display_width * Settings.display_height / 2);


    // init renderer
    epd47  = new Epd47(Settings.display_width, Settings.display_height);
    epd47->Init();

    renderer = epd47;
    renderer->DisplayInit(DISPLAY_INIT_FULL, Settings.display_size, Settings.display_rotate, Settings.display_font);
    renderer->setTextColor(0, 15);

#ifdef SHOW_SPLASH
    // Welcome text
    renderer->setTextFont(2);
    renderer->DrawStringAt(50, 50, "LILGO 4.7 E-Paper Display!", 0, 0);
    renderer->drawRect(0,0,100,100,0);
    renderer->Updateframe();
#endif

    color_type = COLOR_COLOR;

    epd47_init_done = true;
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: E-Paper 4.7"));
  }
}

/*********************************************************************************************/


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdsp16(uint8_t function)
{
  bool result = false;

  if (FUNC_DISPLAY_INIT_DRIVER == function) {
    EpdInitDriver47();
  }
  else if (epd47_init_done && (XDSP_16 == Settings.display_model)) {
    switch (function) {
      case FUNC_DISPLAY_MODEL:
        result = true;
        break;
    }
  }
  return result;
}

#endif  // USE_DISPLAY_EPAPER
#endif  // USE_DISPLAY
