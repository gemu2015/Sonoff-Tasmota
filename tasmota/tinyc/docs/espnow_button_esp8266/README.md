# ESP-NOW wall button on a bare ESP8266

A battery button for the radio-controlled lamps (receiver: `examples/gm24g_lamp.tc` on an ESP32 with ESP-NOW, see `TinyC_Reference.md` → ESP-NOW). A simple Arduino sketch, no TinyC: the ESP8266 is asleep, a key press resets it, `setup()` sends one packet and the chip sleeps again (about 100–250 ms awake, no WiFi connection).

**Packet** (4 bytes): `'B'`, button id (1 = all lamps, 2..5 = zone 1..4), action (0 off, 1 on, 2 toggle - the receiver keeps the state), counter (random byte).

## Wiring
- Button between **RST and GND**. Pressing it wakes the chip out of deep sleep (`ESP.deepSleep(0)`: no timer, no GPIO16 wire needed).
- ESP-01 / ESP-12: CH_PD/EN to VCC with 10 kΩ, GPIO0 and GPIO2 to VCC with 10 kΩ (normal boot), GPIO15 to GND (ESP-12). RST with 10 kΩ to VCC.
- Supply 3.0 .. 3.6 V. **Sleep current decides the battery life**: a bare module sleeps at about 20 µA, a NodeMCU / Wemos board with its USB chip and regulator at several mA (good for trying, useless for a battery). Use a regulator with a small quiescent current (HT7333, MCP1700: a few µA) or two lithium AAA cells (3.0–3.6 V fresh, flat curve). Put 100–470 µF close to the module (the transmit peak is about 300 mA). **No diode chain on a LiPo**: a diode's drop grows with the current (about 0.3 V at a few µA, 0.8 V or more at the 70–300 mA of the transmit burst), so the module sees 2 V and below exactly when it transmits, and only the top of the LiPo's range is usable. Use a low-quiescent-current LDO (MCP1700 3.3 V: 1.6 µA, dropout 0.18 V at 250 mA) or a **LiFePO4 cell (3.2 V, 14500 / 18650) straight on the module** with no regulator at all. The sketch ignores a power-up (battery insertion) so it does not toggle the lamp.
- Rough numbers: 25 µA sleep + 20 presses a day at 5 µAh = 0.7 mAh a day, so 500 mAh last about two years; measure the sleep current.

## Configure and flash (sketch `src/espnow_button_esp8266.ino`)
`RECEIVER` = MAC of the lamp controller (Tasmota main page, `Status 5`), `CHANNEL` = channel of the router (**fix it in the router**, ESP-NOW only reaches a receiver on the same channel), `BUTTON_ID`, `ACTION`. Build `pio run -e esp01_1m`, flash over a serial adapter with GPIO0 to GND (`-t upload`). Compiles with the ESP8266 Arduino core of PlatformIO (282 KB flash).

Without a key anyone in range could send a packet; for a lamp that is acceptable, otherwise put a keyed check into the payload.

## Config mode: one binary for every button (tried on an ESP-12 on 15.11.2026)
Instead of compiling every button, **pull `CFG_PIN` (default GPIO4 on an ESP-12; an ESP-01 has no GPIO4, set `CFG_PIN 3` = RX there; not GPIO0 / 2 / 15, they are boot pins) to GND while the chip starts**: with a jumper or a second button, then press RST or insert the battery. The button opens an access point **`Taster-XXXXXX`** (open, or with `AP_PASS`); the form opens by itself on a phone or tablet (captive portal: a DNS server answers every name with the button's address, the unknown pages go to the form), else open **http://192.168.4.1/** (type `http://`, and switch the mobile data off, otherwise the phone sends the request over the mobile network):

| Field | Meaning |
|---|---|
| Receiver MAC | MAC of the lamp controller (Tasmota `Status 5`); used only when "Broadcast" is off |
| Channel (1..13) | channel of the router: **fix it in the router** |
| Button code | 1 = all lamps, 2..5 = zone 1..4 |
| Action | 0 off, 1 on, 2 toggle |
| Broadcast | on (recommended): the ESP8266 never sees the ESP32's acknowledgement |

*Save* stores the values in the flash (EEPROM emulation, survives a battery change); *Save and send test* also sends one burst of five packets with them (on the channel the access point is on, which follows the channel you entered); *Done* puts the chip to sleep. Without any request for `CFG_TIMEOUT_S` (300 s) it sleeps by itself: the config mode must not drain the battery. A button that was never configured uses the `#define` values of the sketch. The jumper may stay open in normal operation: the internal pull-up only draws current when the pin is pulled low. Tried on 15.11.2026: form, save, "Save and send test" (the lamp controller received the burst and switched), captive portal on an iPad and a phone. ⚠️ Not yet tried: the reading of the stored values after a battery change, the battery current in sleep with the config pin open.

## Measured on hardware (11.11.2026)

- **Send as broadcast, not unicast.** To the receiver's MAC an ESP8266 never gets the ESP32's ACK (`status=1` on every copy); the packets arrived only some of the time (a key press had to be repeated up to 3 times). As a broadcast (`USE_BROADCAST 1`) every copy reports `status=0` and 11 of 11 presses arrived. The receiver accepts only packets starting with `'B'` and a known button id.
- **`wifi_set_channel()` only works in promiscuous mode** on the ESP8266 (the sketch does the enable/set/disable dance); `DEBUG 1` prints the channel actually in use.
- Five copies 25 ms apart (about 125 ms awake); the receiver drops repeats by the counter byte.
- The key press out of deep sleep reports reset reason 5 (`DEEP_SLEEP_AWAKE`) or 6 (`EXT_SYS_RST`); only a real power-up (0) is ignored.
- `TX_POWER_DBM 14` keeps the current peak low; 20.5 was not compared.
- PlatformIO: use `~/.platformio/penv/bin/pio` (see the project notes on the SCons conflict).

