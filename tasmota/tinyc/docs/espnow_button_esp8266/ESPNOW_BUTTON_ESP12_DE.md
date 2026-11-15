# ESP-NOW-Wandtaster auf einem ESP-12 (ESP8266): Verdrahtung, Flashen, erster Start

Ein Batterie-Taster ohne Router-Verbindung: Der Chip schlaeft, ein Druck auf den Taster setzt ihn zurueck, er sendet **einen Satz ESP-NOW-Pakete** (5 Kopien, etwa 125 ms) an den Empfaenger und schlaeft wieder. Empfaenger ist `examples/gm24g_lamp.tc` auf einem ESP32 (funkgesteuerte Lampen) oder jedes TinyC-Programm, das mit `espnowRecv()` `'B'`, Tasternummer, Aktion, Zaehler liest. Eine Firmware-Datei fuer jeden Taster: Empfaenger, Kanal, Tastencode und Aktion werden in einem Webformular eingetragen (Konfigurationsmodus), ohne Uebersetzen.

Release-Datei: **`espnow_button_esp8266.bin`** (ESP8266, mit dem Arduino-Kern gebaut, etwa 290 KB).

## Teile
- ESP-12E / ESP-12F-Modul (oder ein nacktes ESP-12-Brett ohne USB-Chip und Regler: ein NodeMCU oder Wemos schlaeft bei mehreren mA und leert eine Batterie).
- Versorgung **3,0 .. 3,6 V**: zwei Lithium-AAA oder eine LiFePO4-Zelle (3,2 V), oder eine Batterie mit einem LDO mit kleinem Ruhestrom (MCP1700-3.3: 1,6 uA). Kein LiPo mit Dioden (die Spannung bricht bei der Sendespitze von 70 .. 300 mA ein).
- **100 .. 470 uF** Kondensator nahe am Modul (VCC nach GND).
- Vier 10-k-Widerstaende, ein Taster (der Wandtaster), eine Steckbruecke oder ein zweiter Taster fuer den Konfigurationsmodus.
- Zum Flashen: ein USB-Seriell-Adapter mit **3,3 V** Pegel (nicht 5 V).

## Verdrahtung (ESP-12)
| Pin des Moduls | Verbinden mit |
|---|---|
| VCC | + der Versorgung (3,0 .. 3,6 V), Kondensator nach GND |
| GND | - der Versorgung |
| EN (CH_PD) | ueber 10 k an VCC |
| RST | ueber 10 k an VCC; **der Wandtaster zwischen RST und GND** |
| GPIO0 | ueber 10 k an VCC (normaler Start). Nur zum Flashen an GND |
| GPIO2 | ueber 10 k an VCC |
| GPIO15 | ueber 10 k an GND |
| **GPIO4** | frei; **eine Steckbruecke (oder Taster) von GPIO4 nach GND = Konfigurationsmodus** (wird beim Start gelesen; offen = Normalbetrieb, kein Strom) |
| TXD (GPIO1), RXD (GPIO3) | nur zum Flashen: RX des Adapters an TXD, TX des Adapters an RXD, GND an GND |

Der Schlafstrom entscheidet die Laufzeit: ein nacktes Modul schlaeft bei etwa 20 uA. Messen.

## Flashen
1. Adapter wie oben angeschlossen. **GPIO0 an GND**, dann RST kurz druecken (oder die Versorgung einschalten): Der Chip wartet im Boot-Lader.
2. Die Datei an Adresse 0x0 schreiben (Flash-Modus `dout`):
   ```
   esptool --chip esp8266 --port /dev/cu.usbserial-XXXX --baud 115200 write-flash --flash-mode dout 0x0 espnow_button_esp8266.bin
   ```
   (aeltere esptool: `write_flash`). Unter Windows ist der Port `COMn`.
3. Die **Verbindung GPIO0 nach GND wieder loesen**, Versorgung aus und ein.

## Erster Start: Empfaenger und Tastencode einstellen
1. **Steckbruecke GPIO4 nach GND**, dann RST druecken (oder die Batterie einlegen). Der Taster oeffnet das WLAN **`Taster-XXXXXX`** (offen, ohne Passwort).
2. Mit Handy oder Tablet verbinden: Das Formular geht von selbst auf (Fenster "Im WLAN anmelden"). Sonst `http://192.168.4.1/` oeffnen (das `http://` mit eintippen; die mobilen Daten ausschalten, sonst schickt das Handy die Anfrage ueber das Mobilfunknetz).
3. Eintragen:
   | Feld | Bedeutung |
   |---|---|
   | Empfaenger-MAC | MAC der Lampensteuerung (Tasmota `Status 5`); nur wirksam, wenn "Broadcast" aus ist |
   | Kanal (1 .. 13) | Kanal Deines Routers. **Im Router festlegen**: ESP-NOW erreicht nur einen Empfaenger auf demselben Kanal |
   | Tastencode | 1 = alle Lampen, 2 .. 5 = Zone 1 .. 4 |
   | Aktion | 0 aus, 1 ein, 2 umschalten (der Empfaenger fuehrt den Zustand) |
   | Broadcast | ein (empfohlen): ein ESP8266 sieht die Bestaetigung eines ESP32 nie |
4. **Speichern und Test senden**: Der Empfaenger sollte reagieren. Die Werte liegen im Flash und ueberleben einen Batteriewechsel.
5. Steckbruecke entfernen, RST druecken: Ab jetzt sendet jeder Druck die gespeicherten Werte. Der Konfigurationsmodus endet von selbst nach 5 Minuten ohne Anfrage.

## Hinweise
- **Paket** (4 Byte): `'B'`, Tasternummer, Aktion, Zaehler. Der Zaehler macht eine wiederholte Kopie erkennbar.
- Ohne Schluessel kann jeder in Reichweite ein Paket senden. Fuer eine Lampe ist das akzeptabel.
- Das Einschalten (Batterie einlegen) sendet nichts, nur die RST-Taste tut es.
- Erprobt am 15.11.2026 auf einem ESP-12 (Formular, Speichern, Testsatz von der Lampensteuerung empfangen, Captive Portal auf einem iPad und einem Handy). **Noch nicht erprobt:** das Lesen der gespeicherten Werte nach einem Batteriewechsel, der Schlafstrom mit offener Konfigurationsbruecke.
- Quelltext und Einzelheiten: `tasmota/tinyc/docs/espnow_button_esp8266/` (README.md, README_DE.md, `src/espnow_button_esp8266.ino`).
