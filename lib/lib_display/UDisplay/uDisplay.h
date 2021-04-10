#ifndef _UDISP_
#define _UDISP_

#include <Adafruit_GFX.h>
#include <renderer.h>

#define _UDSP_I2C 1
#define _UDSP_SPI 2

#define UDISP_WHITE 1
#define UDISP_BLACK 0

class uDisplay : public Renderer {
 public:
  uDisplay(char *);
  Renderer *Init(void);
  void DisplayInit(int8_t p,int8_t size,int8_t rot,int8_t font);
  void Updateframe();
  void DisplayOnff(int8_t on);
  void Splash(void);
  char *devname(void);
  uint16_t fgcol(void) const { return fg_col; };
  uint16_t bgcol(void) const { return bg_col; };
 protected:
 private:
   uint32_t str2c(char **sp, char *vp, uint32_t len);
   void i2c_command(uint8_t val);
   uint8_t strlen_ln(char *str);
   int32_t next_val(char **sp);
   uint32_t next_hex(char **sp);
   char dname[16];
   uint8_t bpp;
   uint8_t interface;
   uint8_t i2caddr;
   int8_t i2c_scl;
   int8_t i2c_sda;
   int8_t reset;
   uint8_t i2c_cmds[32];
   uint8_t i2c_ncmds;
   uint8_t i2c_on;
   uint8_t i2c_off;
   uint16_t splash_font;
   uint16_t splash_size;
   uint16_t splash_xp;
   uint16_t splash_yp;
   uint16_t fg_col;
   uint16_t bg_col;
};

#endif // _UDISP_
