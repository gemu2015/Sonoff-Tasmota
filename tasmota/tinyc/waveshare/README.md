# Waveshare ESP32-S3-CAM with the standard TinyC firmware

No special firmware is needed: `tinyc32s3` (the normal S3 build) runs this board. The board-specific part is configuration.

## 1. Pins (Tasmota console, then `Restart 1`)

```
Gpio8   640     # I2C SDA   (camera SCCB, codecs, expander)
Gpio7   608     # I2C SCL
Gpio16  8832    # SDIO CLK   (SD card, 1-bit)
Gpio43  8800    # SDIO CMD
Gpio44  8864    # SDIO D0
```

The codes are the ones of the current build (`Gpios 1` ... `Gpios 8`, look for "SDIO CMD" etc.). GPIO43/44 are the UART0 pins: when a
template gives them to the SD card, the firmware releases the serial console by itself (`CMD: Serial console released ...`).

## 2. Start state of the I/O expander: `/expander.cfg` on the FLASH file system

`expander.cfg` (this folder) contains `0x24,0x67,0x7F` = I2C address, output levels, direction. The firmware reads it from the flash file
system BEFORE the SD card is initialised and writes it to the expander (CH32V003, registers 0x02 direction, 0x03 outputs, 0x04 pins):

| Pin | Function | Level in 0x67 |
|---|---|---|
| EXIO0 | touch reset | 1 |
| EXIO1 | display reset | 1 |
| EXIO2 | **SD card select (DAT3/CS)** - must be high or the card does not answer | 1 |
| EXIO3 | camera PWDN (active high) | 0 = camera on |
| EXIO4 | loudspeaker amplifier | 0 = off (the I2SAUDIO plugin switches it) |
| EXIO5 | battery switch | 1 |
| EXIO6 | power LED | 1 |
| EXIO7 | charger detect (input) | - |

After power-up all expander outputs are low, and the expander resets its pins once more during its own start-up, so the firmware writes the
state and reads it back after a pause until it is stable (log line `Expander 0x24 set: ... N round(s)`).

Upload to the flash (not the SD card; port 83 path `/ffs/` = flash):

```
curl --data-binary @expander.cfg "http://<ip>:83/ufs/ffs/expander.cfg"
```

## 3. Audio

`I2SAUDIO.cfg` (this folder) and the plugins `I2SAUDIO_32.bin`, `PICOTTS_32.bin`; codec 5 = this board (see the plugin source).

## 4. Camera

`examples/webcam_tinyc.tc` built with `BOARD_WAVESHARE`. A good OV2640 module works; the OV5640 that came with one board got very hot.
