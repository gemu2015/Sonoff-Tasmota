# ESP-NOW-Wandtaster auf einem nackten ESP8266

Ein Batterie-Taster fuer die funkgesteuerten Lampen (Empfaenger: `examples/gm24g_lamp.tc` auf einem ESP32 mit ESP-NOW, siehe `TinyC_Reference_DE.md` → ESP-NOW). Ein einfacher Arduino-Sketch, kein TinyC: Der ESP8266 schlaeft, ein Tastendruck setzt ihn zurueck, `setup()` sendet ein Paket und er schlaeft wieder (etwa 100–250 ms wach, ohne WLAN-Verbindung).

**Paket** (4 Byte): `'B'`, Tasternummer (1 = alle Lampen, 2..5 = Zone 1..4), Aktion (0 aus, 1 ein, 2 umschalten - der Empfaenger fuehrt den Zustand), Zaehler (Zufallsbyte).

## Verdrahtung
- Taster zwischen **RST und GND**. Er weckt den Chip aus dem Tiefschlaf (`ESP.deepSleep(0)`: ohne Zeitgeber, ohne Draht an GPIO16).
- ESP-01 / ESP-12: CH_PD/EN mit 10 kΩ an VCC, GPIO0 und GPIO2 mit 10 kΩ an VCC (normaler Start), GPIO15 an GND (ESP-12), RST mit 10 kΩ an VCC.
- Versorgung 3,0 .. 3,6 V. **Der Schlafstrom entscheidet die Laufzeit**: ein nacktes Modul schlaeft bei etwa 20 µA, ein NodeMCU / Wemos mit USB-Chip und Regler bei mehreren mA (zum Ausprobieren gut, fuer die Batterie unbrauchbar). Regler mit kleinem Ruhestrom (HT7333, MCP1700: wenige µA) oder zwei Lithium-AAA (3,0–3,6 V frisch, flache Kurve). 100–470 µF nahe am Modul (die Sendespitze liegt bei etwa 300 mA).
- Grobe Zahlen: 25 µA Schlaf + 20 Druecke am Tag zu je 5 µAh = 0,7 mAh am Tag, also halten 500 mAh etwa zwei Jahre; den Schlafstrom messen.

## Einstellen und flashen (`src/espnow_button_esp8266.ino`)
`RECEIVER` = MAC der Lampensteuerung (Tasmota-Hauptseite, `Status 5`), `CHANNEL` = Kanal des Routers (**im Router festlegen**, ESP-NOW erreicht nur einen Empfaenger auf demselben Kanal), `BUTTON_ID`, `ACTION`. Bauen `pio run -e esp01_1m`, ueber einen seriellen Adapter mit GPIO0 an GND flashen (`-t upload`). Uebersetzt mit dem ESP8266-Arduino-Kern von PlatformIO (248 KB Flash).

Ohne Schluessel kann jeder in Reichweite ein Paket senden; bei einer Lampe ist das vertretbar, sonst eine geschluesselte Pruefung in die Nutzdaten legen.
