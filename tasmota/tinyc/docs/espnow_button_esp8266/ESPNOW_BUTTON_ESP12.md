# ESP-NOW wall button on an ESP-12 (ESP8266): wiring, flashing, first start

A battery button without a router connection: the chip sleeps, a press of the button resets it, it sends **one burst of ESP-NOW packets** (5 copies, about 125 ms) to the receiver and sleeps again. The receiver is `examples/gm24g_lamp.tc` on an ESP32 (radio-controlled lamps) or any TinyC program that reads `'B'`, button id, action, counter with `espnowRecv()`. One firmware file for every button: receiver, channel, button code and action are entered on a web form (config mode), no compiling.

Release file: **`espnow_button_esp8266.bin`** (ESP8266, built with the Arduino core, about 290 KB).

## Parts
- ESP-12E / ESP-12F module (or a bare ESP-12 board without USB chip and regulator: a NodeMCU or Wemos board sleeps at several mA and drains a battery).
- Supply **3.0 .. 3.6 V**: two lithium AAA cells, or a LiFePO4 cell (3.2 V), or a battery with a low-quiescent LDO (MCP1700-3.3: 1.6 uA). Not a LiPo with diodes (the voltage drops at the transmit peak of 70 .. 300 mA).
- **100 .. 470 uF** capacitor close to the module (VCC to GND).
- Four 10 k resistors, one push button (the wall button), one jumper or a second button for the config mode.
- For flashing: a USB-serial adapter with **3.3 V** logic (not 5 V).

## Wiring (ESP-12)
| Pin of the module | Connect to |
|---|---|
| VCC | + of the supply (3.0 .. 3.6 V), capacitor to GND |
| GND | - of the supply |
| EN (CH_PD) | VCC through 10 k |
| RST | VCC through 10 k; **the wall button between RST and GND** |
| GPIO0 | VCC through 10 k (normal start). To GND **only** to flash |
| GPIO2 | VCC through 10 k |
| GPIO15 | GND through 10 k |
| **GPIO4** | free; **a jumper (or button) from GPIO4 to GND = config mode** (read at start; open = normal use, no current) |
| TXD (GPIO1), RXD (GPIO3) | only for flashing: adapter RX to TXD, adapter TX to RXD, adapter GND to GND |

Sleep current decides the battery life: a bare module sleeps at about 20 uA. Measure it.

## Flash
1. Adapter wired as above. **GPIO0 to GND**, then press RST briefly (or switch the supply on): the chip waits in the boot loader.
2. Flash the file to address 0x0 (flash mode `dout`):
   ```
   esptool --chip esp8266 --port /dev/cu.usbserial-XXXX --baud 115200 write-flash --flash-mode dout 0x0 espnow_button_esp8266.bin
   ```
   (older esptool: `write_flash`). On Windows the port is `COMn`.
3. **Remove the GPIO0 to GND connection**, switch the supply off and on.

## First start: set receiver and button code
1. **Jumper GPIO4 to GND**, then press RST (or insert the battery). The button opens the WLAN **`Taster-XXXXXX`** (open, no password).
2. Join it with a phone or tablet: the form opens by itself ("log in to the WLAN" window). Otherwise open `http://192.168.4.1/` (type the `http://`; switch the mobile data off, otherwise the phone sends the request over the mobile network).
3. Enter:
   | Field | Meaning |
   |---|---|
   | Receiver MAC | MAC of the lamp controller (Tasmota `Status 5`); used only when "Broadcast" is off |
   | Channel (1 .. 13) | channel of your router. **Fix it in the router**: ESP-NOW only reaches a receiver on the same channel |
   | Button code | 1 = all lamps, 2 .. 5 = zone 1 .. 4 |
   | Action | 0 off, 1 on, 2 toggle (the receiver keeps the state) |
   | Broadcast | on (recommended): an ESP8266 never sees the acknowledgement of an ESP32 |
4. **Save and send test**: the receiver should react. The values are stored in the flash and survive a battery change.
5. Remove the jumper, press RST: from now on every press sends the stored values. The config mode ends by itself after 5 minutes without a request.

## Notes
- **Packet** (4 bytes): `'B'`, button id, action, counter. The counter makes a repeated copy recognisable.
- Without a key anyone in range can send a packet. For a lamp that is acceptable.
- A power-up (inserting the battery) sends nothing, only the RST button does.
- Tried on 15.11.2026 on one ESP-12 (form, save, test burst received by the lamp controller, captive portal on an iPad and a phone). **Not yet tried:** reading the stored values after a battery change, the sleep current with the config jumper open.
- Source and details: `tasmota/tinyc/docs/espnow_button_esp8266/` (README.md, README_DE.md, `src/espnow_button_esp8266.ino`).
