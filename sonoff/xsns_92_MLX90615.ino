/*
  xsns_92_MLX90615.ino - Support for MLX ir temperature sensor

  Copyright (C) 2019  Theo Arends

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

#ifdef USE_MLX90615

#define XSNS_92                          92

// mlx90615
#define I2_ADR_IRT      0x5b

void MLX90615_Init() {
  if (!i2c_flg) return;
  Wire.begin();
  delay(500);

  // force i2c
  //digitalWrite(pin[GPIO_I2C_SCL], LOW);
  //delay(3);
  //digitalWrite(pin[GPIO_I2C_SCL], HIGH);

}

// return ir temp
// 0 = chip, 1 = object temperature
// * 0.02 - 273.15
uint16_t read_irtmp(uint8_t flag) {
    uint8_t hig,low;
    uint16_t val;

    Wire.beginTransmission(I2_ADR_IRT);
    if (!flag) Wire.write(0x26);
    else Wire.write(0x27);
    Wire.endTransmission(false);

    Wire.requestFrom(I2_ADR_IRT, (uint8_t)3);
    hig=Wire.read();
    low=Wire.read();
    Wire.read();

    val=((uint16_t)hig<<8)|low;
    //val-=13657;
    //val/=5;
    return val;
}

#ifdef USE_WEBSERVER
 const char HTTP_IRTMP[] PROGMEM = "%s"
  "{s}MXL90615 " D_TEMPERATURE "{m}%s C" "{e}";

void MLX90615_Show(uint8_t json) {
  double irtmp;
  uint16_t uval=read_irtmp(1);
  if (uval&0x8000) {
    irtmp=-999;
  } else {
    irtmp=((double)uval*0.02)-273.15;
  }
  char temperature[16];
  dtostrfd(irtmp, Settings.flag2.temperature_resolution, temperature);


  if (json) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"MLX90615\":{\"" D_JSON_TEMPERATURE "\":%s}"), mqtt_data, temperature);
#ifdef USE_WEBSERVER
  } else {
    snprintf_P(mqtt_data, sizeof(mqtt_data), HTTP_IRTMP, mqtt_data,temperature);
#endif
  }

}
#endif  // USE_WEBSERVER

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

boolean Xsns92(byte function)
{
  boolean result = false;

    switch (function) {
      case FUNC_INIT:
        MLX90615_Init();
        break;
      case FUNC_JSON_APPEND:
        MLX90615_Show(1);
          break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_APPEND:
        MLX90615_Show(0);
        break;
#endif  // USE_WEBSERVER
    }
    return result;
}

#endif  // USE_MLX90615
