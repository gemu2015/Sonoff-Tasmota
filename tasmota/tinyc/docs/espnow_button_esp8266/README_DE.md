# ESP-NOW-Wandtaster auf einem nackten ESP8266

Ein Batterie-Taster fuer die funkgesteuerten Lampen (Empfaenger: `examples/gm24g_lamp.tc` auf einem ESP32 mit ESP-NOW, siehe `TinyC_Reference_DE.md` → ESP-NOW). Ein einfacher Arduino-Sketch, kein TinyC: Der ESP8266 schlaeft, ein Tastendruck setzt ihn zurueck, `setup()` sendet ein Paket und er schlaeft wieder (etwa 100–250 ms wach, ohne WLAN-Verbindung).

**Paket** (4 Byte): `'B'`, Tasternummer (1 = alle Lampen, 2..5 = Zone 1..4), Aktion (0 aus, 1 ein, 2 umschalten - der Empfaenger fuehrt den Zustand), Zaehler (Zufallsbyte).

## Verdrahtung
- Taster zwischen **RST und GND**. Er weckt den Chip aus dem Tiefschlaf (`ESP.deepSleep(0)`: ohne Zeitgeber, ohne Draht an GPIO16).
- ESP-01 / ESP-12: CH_PD/EN mit 10 kΩ an VCC, GPIO0 und GPIO2 mit 10 kΩ an VCC (normaler Start), GPIO15 an GND (ESP-12), RST mit 10 kΩ an VCC.
- Versorgung 3,0 .. 3,6 V. **Der Schlafstrom entscheidet die Laufzeit**: ein nacktes Modul schlaeft bei etwa 20 µA, ein NodeMCU / Wemos mit USB-Chip und Regler bei mehreren mA (zum Ausprobieren gut, fuer die Batterie unbrauchbar). Regler mit kleinem Ruhestrom (HT7333, MCP1700: wenige µA) oder zwei Lithium-AAA (3,0–3,6 V frisch, flache Kurve). 100–470 µF nahe am Modul (die Sendespitze liegt bei etwa 300 mA). **Keine Diodenkette an einem LiPo**: der Spannungsabfall einer Diode waechst mit dem Strom (etwa 0,3 V bei wenigen µA, 0,8 V und mehr bei den 70–300 mA des Sendeimpulses), das Modul sieht also genau beim Senden 2 V und weniger, und nur das obere Ende des LiPo-Bereichs ist nutzbar. Besser ein LDO mit kleinem Ruhestrom (MCP1700 3,3 V: 1,6 µA, Dropout 0,18 V bei 250 mA) oder eine **LiFePO4-Zelle (3,2 V, 14500 / 18650) direkt am Modul**, ganz ohne Regler. Der Sketch ignoriert das Einschalten (Batterie einlegen), damit die Lampe dabei nicht umschaltet.
- Grobe Zahlen: 25 µA Schlaf + 20 Druecke am Tag zu je 5 µAh = 0,7 mAh am Tag, also halten 500 mAh etwa zwei Jahre; den Schlafstrom messen.

## Einstellen und flashen (`src/espnow_button_esp8266.ino`)
`RECEIVER` = MAC der Lampensteuerung (Tasmota-Hauptseite, `Status 5`), `CHANNEL` = Kanal des Routers (**im Router festlegen**, ESP-NOW erreicht nur einen Empfaenger auf demselben Kanal), `BUTTON_ID`, `ACTION`. Bauen `pio run -e esp01_1m`, ueber einen seriellen Adapter mit GPIO0 an GND flashen (`-t upload`). Uebersetzt mit dem ESP8266-Arduino-Kern von PlatformIO (282 KB Flash).

Ohne Schluessel kann jeder in Reichweite ein Paket senden; bei einer Lampe ist das vertretbar, sonst eine geschluesselte Pruefung in die Nutzdaten legen.

## Konfigurationsmodus: ein Programm fuer jeden Taster (am 15.11.2026 auf einem ESP-12 erprobt)
Statt jeden Taster zu uebersetzen: **`CFG_PIN` (Vorgabe GPIO4 auf einem ESP-12; ein ESP-01 hat kein GPIO4, dort `CFG_PIN 3` = RX setzen; nicht GPIO0 / 2 / 15, das sind Boot-Pins) beim Start an GND legen**, mit einer Steckbruecke oder einem zweiten Taster, dann RST druecken oder die Batterie einlegen. Der Taster oeffnet einen Zugangspunkt **`Taster-XXXXXX`** (offen, oder mit `AP_PASS`); das Formular oeffnet sich von selbst auf Handy oder Tablet (Captive Portal: ein DNS-Server beantwortet jeden Namen mit der Adresse des Tasters, unbekannte Seiten gehen zum Formular), sonst **http://192.168.4.1/** oeffnen (mit `http://`, und die mobilen Daten ausschalten, sonst schickt das Handy die Anfrage ueber das Mobilfunknetz):

| Feld | Bedeutung |
|---|---|
| Empfaenger-MAC | MAC der Lampensteuerung (Tasmota `Status 5`); nur wirksam, wenn "Broadcast" aus ist |
| Kanal (1..13) | Kanal des Routers: **im Router festlegen** |
| Tastencode | 1 = alle Lampen, 2..5 = Zone 1..4 |
| Aktion | 0 aus, 1 ein, 2 umschalten |
| Broadcast | ein (empfohlen): der ESP8266 sieht die Bestaetigung des ESP32 nie |

*Speichern* legt die Werte im Flash ab (EEPROM-Emulation, ueberlebt einen Batteriewechsel); *Speichern und Test senden* schickt zusaetzlich einen Satz aus fuenf Paketen (auf dem Kanal, auf dem der Zugangspunkt steht, und der folgt dem eingegebenen Kanal); *Beenden* schickt den Chip schlafen. Ohne Zugriff schlaeft er nach `CFG_TIMEOUT_S` (300 s) von selbst: der Konfigurationsmodus darf die Batterie nicht leeren. Ein Taster, der nie eingestellt wurde, nimmt die `#define`-Werte des Sketches. Die Steckbruecke darf im Normalbetrieb offen bleiben: der interne Pull-up zieht nur Strom, wenn der Pin nach unten gezogen wird. Erprobt am 15.11.2026: Formular, Speichern, "Speichern und Test senden" (die Lampensteuerung hat den Satz empfangen und geschaltet), Captive Portal auf einem iPad und einem Handy. ⚠️ Noch nicht erprobt: das Lesen der gespeicherten Werte nach einem Batteriewechsel und der Ruhestrom mit offenem Konfigurationspin.

## Auf Hardware gemessen (11.11.2026)

- **Als Broadcast senden, nicht gezielt.** An die MAC des Empfaengers bekommt ein ESP8266 die Bestaetigung der ESP32 nie (`status=1` bei jeder Kopie); die Pakete kamen nur teilweise an (ein Tastendruck musste bis zu dreimal wiederholt werden). Als Broadcast (`USE_BROADCAST 1`) meldet jede Kopie `status=0`, und 11 von 11 Druecken kamen an. Der Empfaenger nimmt nur Pakete an, die mit `'B'` und einer bekannten Tasternummer beginnen.
- **`wifi_set_channel()` wirkt am ESP8266 nur im Promiscuous-Modus** (der Sketch schaltet ihn kurz ein und wieder aus); `DEBUG 1` gibt den tatsaechlichen Kanal aus.
- Fuenf Kopien im Abstand von 25 ms (etwa 125 ms wach); der Empfaenger erkennt Wiederholungen am Zaehlerbyte.
- Der Tastendruck aus dem Tiefschlaf meldet Reset-Grund 5 (`DEEP_SLEEP_AWAKE`) oder 6 (`EXT_SYS_RST`); nur ein echtes Einschalten (0) wird ignoriert.
- `TX_POWER_DBM 14` haelt die Stromspitze klein; 20,5 wurde nicht verglichen.

