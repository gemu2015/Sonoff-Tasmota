/*
  xdsp_17_universal.ino -  universal display driver support for Tasmota

  Copyright (C) 2021  Gerhard Mutz and  Theo Arends

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
#ifdef USE_UNIVERSAL_DISPLAY

#define XDSP_17                17

#include <uDisplay.h>

uDisplay *udisp;
bool udisp_init_done = false;
extern uint8_t color_type;
extern uint16_t fg_color;
extern uint16_t bg_color;

#ifdef USE_UFILESYS
extern FS *ufsp;
#endif

#define DISPDESC_SIZE 1000

/*********************************************************************************************/

void Init_uDisp(void) {
char *ddesc = 0;
char *fbuff;

  if (1) {
    Settings.display_model = XDSP_17;

    fg_color = 1;
    bg_color = 0;
    color_type = COLOR_BW;

    fbuff = (char*)calloc(DISPDESC_SIZE, 1);
    if (!fbuff) return;

#ifdef USE_UFILESYS
    if (ufsp  && !TasmotaGlobal.no_autoexec) {
      File fp;
      fp = ufsp->open("/dispdesc.txt", "r");
      if (fp > 0) {
        uint32_t size = fp.size();
        fp.read((uint8_t*)fbuff, size);
        fp.close();
        ddesc = fbuff;
        AddLog(LOG_LEVEL_INFO, PSTR("DSP: File descriptor used"));
      }
    }
#endif


#ifdef USE_SCRIPT
    if (bitRead(Settings.rule_enabled, 0) && !ddesc) {
      uint8_t dfound = Run_Scripter(">d",-2,0);
      if (dfound == 99) {
        char *lp = glob_script_mem.section_ptr + 2;
        while (*lp != '\n') lp++;
        memcpy(fbuff, lp + 1, DISPDESC_SIZE - 1);
        ddesc = fbuff;
        AddLog(LOG_LEVEL_INFO, PSTR("DSP: Script descriptor used"));
      }
    }
#endif // USE_SCRIPT


    if (!ddesc) {
      AddLog(LOG_LEVEL_INFO, PSTR("DSP: No valid descriptor found"));
      return;
    }
    // now replace tasmota vars before passing to driver
    char *cp = strstr(ddesc, "I2C");
    if (cp) {
      cp += 7;
      //,3c,22,21,-1
      // i2c addr
      //if (*cp == '*') {
      //  Settings.display_address
      //}
      //replacepin(&cp, Settings.display_address);
      replacepin(&cp, Pin(GPIO_I2C_SCL));
      replacepin(&cp, Pin(GPIO_I2C_SDA));
      replacepin(&cp, Pin(GPIO_OLED_RESET));
    }

    // init renderer
    udisp  = new uDisplay(ddesc);

    // release desc buffer
    if (fbuff) free(fbuff);

    renderer = udisp->Init();
    if (!renderer) return;

    Settings.display_width = renderer->width();
    Settings.display_height = renderer->height();
    fg_color = udisp->fgcol();
    bg_color = udisp->bgcol();

    renderer->DisplayInit(DISPLAY_INIT_MODE, Settings.display_size, Settings.display_rotate, Settings.display_font);


#ifdef SHOW_SPLASH
    udisp->Splash();
#endif

    udisp_init_done = true;
    AddLog(LOG_LEVEL_INFO, PSTR("DSP: %s!"), udisp->devname());
  }
}

/*********************************************************************************************/

void replacepin(char **cp, uint16_t pin) {
  char *lp = *cp;
  if (*lp == '*') {
    char val[8];
    itoa(pin, val, 10);
    uint16_t slen = strlen(val);
    AddLog(LOG_LEVEL_INFO, PSTR("replace pin: %s), val);
    memmove(lp + slen, lp + 1, strlen(lp) - slen);
    memmove(lp, val, slen);
  }
  char *np = strchr(lp, ',');
  if (np) {
    *cp = np + 1;
  }
}


/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdsp17(uint8_t function)
{
  bool result = false;

  if (FUNC_DISPLAY_INIT_DRIVER == function) {
    Init_uDisp();
  }
  else if (udisp_init_done && (XDSP_17 == Settings.display_model)) {
    switch (function) {
      case FUNC_DISPLAY_MODEL:
        result = true;
        break;
    }
  }
  return result;
}

#endif  // USE_UNIVERSAL_DISPLAY
#endif  // USE_DISPLAY
