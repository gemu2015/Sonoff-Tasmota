# Firmware image `tinyc32s3-eth-smgw`

Ready-made firmware for the SMGW reader on the Waveshare ESP32-S3-ETH (W5500 + SD over SPI), device block by Andreas (25.11.2026).

- `platformio_override_smgw.ini.example`: the build environment (copy it into your `platformio_override.ini`).
- `user_config_override_smgw.h.example`: the device block (`#ifdef TINYC_SMGW`) for `tasmota/user_config_override.h`.

Build: `pio run -e tinyc32s3-eth-smgw` (PlatformIO core exactly 6.2.0). The first build recompiles the SDK libraries (about 8 minutes).
Result: `.pio/build/tinyc32s3-eth-smgw/firmware.bin` (updates) and `firmware.factory.bin` (first flash over USB).

The image presets no Wi-Fi, no MQTT, no location, **no FTP server**; LAN 192.168.1.21 (the HAN socket network), topic `SMGW` (`http://smgw.local/`).
Before publishing, scan the binaries: `scripts/creds_scan.py` only looks for Wi-Fi credentials, so also search them for the values of your general
`user_config_override.h` (MQTT host, FTP login, location, mail).
