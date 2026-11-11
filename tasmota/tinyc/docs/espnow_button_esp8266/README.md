# ESP-NOW wall button on a bare ESP8266

A battery button for the radio-controlled lamps (receiver: `examples/gm24g_lamp.tc` on an ESP32 with ESP-NOW, see `TinyC_Reference.md` → ESP-NOW). A simple Arduino sketch, no TinyC: the ESP8266 is asleep, a key press resets it, `setup()` sends one packet and the chip sleeps again (about 100–250 ms awake, no WiFi connection).

**Packet** (4 bytes): `'B'`, button id (1 = all lamps, 2..5 = zone 1..4), action (0 off, 1 on, 2 toggle - the receiver keeps the state), counter (random byte).

## Wiring
- Button between **RST and GND**. Pressing it wakes the chip out of deep sleep (`ESP.deepSleep(0)`: no timer, no GPIO16 wire needed).
- ESP-01 / ESP-12: CH_PD/EN to VCC with 10 kΩ, GPIO0 and GPIO2 to VCC with 10 kΩ (normal boot), GPIO15 to GND (ESP-12). RST with 10 kΩ to VCC.
- Supply 3.0 .. 3.6 V. **Sleep current decides the battery life**: a bare module sleeps at about 20 µA, a NodeMCU / Wemos board with its USB chip and regulator at several mA (good for trying, useless for a battery). Use a regulator with a small quiescent current (HT7333, MCP1700: a few µA) or two lithium AAA cells (3.0–3.6 V fresh, flat curve). Put 100–470 µF close to the module (the transmit peak is about 300 mA). **No diode chain on a LiPo**: a diode's drop grows with the current (about 0.3 V at a few µA, 0.8 V or more at the 70–300 mA of the transmit burst), so the module sees 2 V and below exactly when it transmits, and only the top of the LiPo's range is usable. Use a low-quiescent-current LDO (MCP1700 3.3 V: 1.6 µA, dropout 0.18 V at 250 mA) or a **LiFePO4 cell (3.2 V, 14500 / 18650) straight on the module** with no regulator at all. The sketch ignores a power-up (battery insertion) so it does not toggle the lamp.
- Rough numbers: 25 µA sleep + 20 presses a day at 5 µAh = 0.7 mAh a day, so 500 mAh last about two years; measure the sleep current.

## Configure and flash (sketch `src/espnow_button_esp8266.ino`)
`RECEIVER` = MAC of the lamp controller (Tasmota main page, `Status 5`), `CHANNEL` = channel of the router (**fix it in the router**, ESP-NOW only reaches a receiver on the same channel), `BUTTON_ID`, `ACTION`. Build `pio run -e esp01_1m`, flash over a serial adapter with GPIO0 to GND (`-t upload`). Compiles with the ESP8266 Arduino core of PlatformIO (248 KB flash).

Without a key anyone in range could send a packet; for a lamp that is acceptable, otherwise put a keyed check into the payload.
