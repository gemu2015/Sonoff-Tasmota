# WAVE ROVER mit TinyC (Waveshare General Driver for Robots)

Wie man den Waveshare **WAVE ROVER** (4WD-Metallfahrgestell) mit Tasmota/TinyC auf seiner ESP32-Treiberplatine betreibt, statt mit der
Waveshare-Firmware. Programm: `examples/wave_rover.tc`. English: [WAVE_ROVER.md](WAVE_ROVER.md).

> **Stand: nach der Waveshare-Dokumentation und dem Quellcode geschrieben, bevor der Rover da war. Auf dem Rover ist davon noch nichts gelaufen.**
> Die Fahrlogik (Mischung, PWM-Abbildung, Batterieschutz) wurde im IDE-Simulator auf dem Mac geprueft, Pins und I2C-Adressen stammen aus dem
> Waveshare-Quellcode. Bitte diese Datei nach dem ersten Test korrigieren.

## Die Hardware (Waveshare-Wiki und `waveshareteam/ugv_base_general`)

- Treiberplatine "General Driver for Robots": **ESP32-WROOM-32UE**, IPEX-Antennenbuchse (**Antenne angeschlossen lassen**), **4 MB Flash**
  (die Original-Partitionstabelle endet bei 0x400000), kein PSRAM. Zwei USB-C-Buchsen: **"USB"** (ESP32, CP2102, automatische Download-Schaltung,
  kein Boot-Knopf noetig) und **"LIDAR"** (ein zweiter CP2102, der nur die Daten eines Lidars an einen Rechner weitergibt, das ist **nicht** der ESP32).
- Motoren: vier N20-Getriebemotoren **ohne Encoder**, je zwei pro Seite parallel, ueber **einen TB6612FNG**. Kanal **A = links, B = rechts**.
  Keine Raddrehzahl, keine Wegmessung. Unter PWM 23 drehen die Motoren nicht an.
- Am I2C-Bus: **QMI8658C** (Beschleunigung + Gyro, + Temperatur), **AK09918C** (Kompass), **INA219 bei 0x42** (Akkuspannung/-strom),
  **SSD1306-OLED 128x32 bei 0x3C**. Ausserdem auf der Platine: SD-Kartenschacht, Anschluss fuer serielle Bus-Servos (ST3215), zwei 12-V-Schaltausgaenge
  (IO4, IO5, PWM) fuer Licht.
- Halter fuer 3 x 18650 mit Ladeelektronik (das 12,6-V-Netzteil liegt dabei, die Zellen nicht).

| Funktion | GPIO |
|---|---|
| TB6612 PWM A / B | 25 / 26 |
| Richtung A: AIN1, AIN2 | 21, 17 |
| Richtung B: BIN1, BIN2 | 22, 23 |
| I2C SDA / SCL | 32 / 33 |
| Bus-Servos RX / TX (Halbduplex ueber die Schaltung der Platine) | 18 / 19 |
| IO4 / IO5 (12-V-Schaltausgaenge) | 4 / 5 |
| Encoder (beim WAVE ROVER ungenutzt) | 35, 34, 27, 16 |
| SD-Kartenschacht (SPI) CS / MOSI / SCK / MISO | 15 / 13 / 14 / 12 (aus dem Schaltplan; **IO12 ist ein Strapping-Pin**: zieht eine Karte MISO beim Reset hoch, kann der Start scheitern) |
| Lidar-Datenleitung (LD19 TX) | Netz `CP_RX` an der Leiste **P3** (7 Pins, letzter Pin nach Schaltplanreihenfolge; mit dem Messgeraet pruefen): eine Drahtbruecke von dort zum freien Encoder-Pin IO16 oder IO27 (P3-Pins 4 / 2) gibt dem ESP32 die Lidar-Daten (der 1-k-Vorwiderstand zum CP2102 bleibt) |

Vorwaerts = IN1 high, IN2 low (WAVE ROVER, `SET_MOTOR_DIR = false` im Waveshare-Quellcode). Die Waveshare-Firmware faehrt die PWM mit 100 kHz, 8 Bit.
In TinyC nutzt `analogWrite()` die Arduino-Vorgabe (1 kHz): es geht, die Motoren pfeifen etwas.

## Tasmota-Einstellungen

Bau `tinyc32-4M-plain` (klassischer ESP32, 4 MB, ohne Matter und Kamera; etwa 238 KB Flash frei). In *Konfiguration Modul* das generische Modul und nur
**GPIO32 = I2C SDA, GPIO33 = I2C SCL** setzen. **Die Motorpins (17, 21, 22, 23, 25, 26) unbelegt lassen**: TinyC lehnt Pins ab, die Tasmota fuer eine Funktion genommen hat.
Tasmotas eigener INA219-Treiber kennt die Adresse 0x42 nicht (noch pruefen); `wave_rover.tc` liest das Register der Busspannung selbst.
Lagesensor, Kompass: kein Tasmota-Treiber, die Register aus TinyC lesen (der Waveshare-Quellcode hat `QMI8658.cpp` / `AK09918.cpp` als Vorlage).

## Vor dem Flashen: das Original sichern

1. Rover oeffnen und ein USB-Kabel in die Buchse **USB** der Platine stecken (nicht LIDAR). Laut Waveshare-Wiki muss der Roboter dafuer zerlegt werden.
2. Chip und Flashgroesse pruefen, dann den ganzen Flash auslesen (esptool aus PlatformIO; esptool 5 schreibt `read-flash`, aeltere Versionen `read_flash`):

```bash
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX flash-id
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 921600 read-flash 0 0x400000 wave_rover_original_4MB.bin
```

3. **Diese Datei aufheben** (nicht ins Repository: sie enthaelt die WLAN-Daten des Rovers, sobald sie eingetragen sind). Zurueck geht es mit
   `... write-flash 0 wave_rover_original_4MB.bin`. Den Quellcode der Waveshare-Firmware und einen Bau gibt es in `waveshareteam/ugv_base_general`
   (`General_Driver/build/...`), ausserdem hat Waveshare ein "ESP32 Download Tool" fuer das Werksabbild.

## Tasmota/TinyC flashen

```bash
pio run -e tinyc32-4M-plain --target upload --upload-port /dev/cu.usbserial-XXXX
```

Die Partitionsaufteilung aendert sich (das Original hat 3 MB App und keinen OTA-Platz). Nach dem ersten Start: den Hotspot `tasmota-xxxx` nehmen, das WLAN eintragen,
die beiden I2C-Pins setzen, dann `wave_rover.tcb` laden (IDE oder `scripts/push_tcb.sh examples/wave_rover.tc <ip>`) und starten. Spaetere Updates gehen per WLAN (OTA).

## Erste Fahrt (in dieser Reihenfolge)

1. **Raeder in der Luft** (Rover auf einer Kiste), Akku drin. Leuchtet die LED der Stromplatine beim Einsetzen der Zellen, sind sie falsch herum: wieder herausnehmen.
2. `http://<ip>/rover` oeffnen. Die Geschwindigkeitsgrenze startet bei **40 %**. Das Feld beruehren: Beim Schieben des Punktes nach oben muessen beide Seiten vorwaerts drehen,
   nach rechts links schneller. Dreht eine Seite falsch herum oder sind links/rechts vertauscht, `DIR_L` / `DIR_R` im Programm aendern.
3. Finger weg: die Motoren muessen anhalten. Tab schliessen waehrend der Fahrt: Halt nach etwa 0,3 s. WLAN am Handy aus: dasselbe.
4. Die Hauptseite zeigt Akkuspannung (INA219) und die Zahl der Watchdog-Stopps. Unter 9,3 V werden die Motoren verweigert.
5. Erst dann auf den Boden stellen.

## Sensor-Treiber (TinyC-Bibliotheken)

Drei Bibliotheken in `examples/common/`, jeweils per `#include` einzubinden, ohne `main()` und ohne Rueckrufe. Sie belegen ihre I2C-Adresse (`i2cSetActiveFound`), damit andere Treiber sie nicht anfassen.

| Bibliothek | Chip | Aufrufe | Ergebnisse |
|---|---|---|---|
| `ina219.tc` | INA219 bei 0x42 | `ina_init(addr, bus)`, `ina_poll()`, `ina_release()` | `ina_mv` mV, `ina_ma` mA, `ina_mw` mW, `ina_shunt_uv`, `ina_ovf` |
| `qmi8658.tc` | QMI8658C bei 0x6B / 0x6A | `qmi_init(bus, acc_range, gyr_range, odr)`, `qmi_poll()`, `qmi_cal_gyro(n)`, `qmi_yaw_reset()` | `qmi_ax..az` g, `qmi_gx..gz` dps, `qmi_temp` C, `qmi_yaw` Grad (integrierter Gyro) |
| `ak09918.tc` | AK09918C bei 0x0C | `ak_init(bus, mode)`, `ak_poll()`, `ak_heading()`, `ak_cal_begin()` / `ak_cal_end()` | `ak_x/y/z` uT, `ak_heading()` Grad 0..360 |

`examples/wave_rover_sensors.tc` zeigt alle auf der Tasmota-Hauptseite und in der Telemetrie (mit den Befehlen `RoverCalGyro`, `RoverYaw0`, `RoverCalMag 1|0`).
`examples/wave_sensors_test.tc` prueft das Dekodieren aller drei **ohne Hardware** (47 Pruefungen: Byte-Reihenfolge, Vorzeichen, Skalierung, Ueberlauf-Bit, Kalibrierung, Richtung);
es laeuft auf jedem TinyC-Geraet. Es kann nicht zeigen, dass der I2C-Zugriff selbst geht. Einheiten, Registeraufbau und Kalibrierung stehen im Kopf jeder Bibliothek.
Nach den Datenblaettern und dem Waveshare-Quellcode geschrieben (GPL, nur als Registervorlage benutzt), **auf dem Rover noch nicht getestet**. Offen: welche Chip-Achse auf der Platine "vorwaerts" ist, und wie stark
die Motoren den Kompass stoeren (er sitzt direkt daneben: bei ausgeschalteten Motoren kalibrieren und beim Fahren das Feld ansehen).

## Lidar-Treiber und die Frage nach der Zeitkritik (`common/ld19.tc`)

`examples/common/ld19.tc` liest ein **LD19 / STL-19P / D500** von LDROBOT (360 Grad, UART 230400 Baud, Pakete zu 47 Byte, etwa 375 Pakete = 4500 Punkte pro Sekunde,
10 Umdrehungen pro Sekunde), prueft die CRC8 und sortiert jeden Punkt in ein Ein-Grad-Fach: `lid_min(330, 30)` ist das Naechste vor dem Fahrzeug. Demo: `examples/wave_lidar.tc`. Eine 360-Grad-Radaranzeige als Webseite (`/lidar`, mit Simulationsschalter zum Ausprobieren ohne Sensor): `examples/lidar_scope.tc`.
Selbsttest mit synthetischen Paketen: `examples/wave_lidar_test.tc` (35 Pruefungen, neun absichtliche Aenderungen am Treiber werden erkannt). Nach dem Datenblatt des LD19 geschrieben,
**mit einem echten Lidar nicht getestet** (noch keins da).

**Ist das in TinyC zeitkritisch? Gemessen auf einem ESP32-S3 (.190), der Parser mit 400 Paketen = 1,07 s echter Lidar-Daten gefuettert:**

| | Zeit | Anteil am Echtzeit-Datenstrom |
|---|---|---|
| Parsen mit CRC-Pruefung | 238 ms | **22 %** |
| Parsen ohne CRC-Pruefung (`lid_crc_on = 0`) | 206 ms | 19 % |

- **Nicht hart echtzeitkritisch.** Die serielle Hardware puffert den Strom: Der Treiber oeffnet den Port mit einem 2048-Byte-Ring, das sind **116 ms** Lidar-Daten. Das Programm
  muss etwa alle 50 ms abholen, eine Luecke von 100 ms kostet noch nichts; eine laengere verliert nur einige Pakete, der Parser findet den naechsten Paketanfang selbst (Kopf + CRC).
- **Aber es ist eine echte Last:** etwa ein Fuenftel der Zeit der VM (rund 2000 VM-Befehle je Paket, etwa 3 Millionen pro Sekunde auf dem S3). In `TaskLoop()` (eigene Aufgabe) machen,
  nicht in einem Takt-Rueckruf, der sich das Befehlsbudget mit dem Rest teilt. Der klassische ESP32 der WAVE-ROVER-Platine duerfte in derselben Groessenordnung liegen (ebenfalls 240 MHz), nicht gemessen.
- Die CRC ist billig (13 % des Parsens); die Winkelschleife ueber 12 Punkte ueberwiegt. Wird die VM fuer mehr gebraucht, laesst sich die Last durch Auslassen von Punkten oder nur einen Teil des Scans senken.
- Kartierung (SLAM) oder Planung gehoert nicht in diese VM: Der ESP32 liefert die naechsten Abstaende je Sektor (Hindernisstopp, Wandfolgen), Karten wuerde ein Rechner (Mac Studio) aus dem Rohscan bauen.
- **Auf der WAVE-ROVER-Platine geht die Lidar-Buchse an eine CP2102-USB-Bruecke, nicht an den ESP32.** Um das Lidar mit TinyC zu lesen, muss seine TX-Leitung an einen freien GPIO eines ESP32
  (zum Beispiel ein eigenes S3-Board); die Treiberplatine des Rovers hat keinen freien UART.

## Noch nicht gemacht

- SD-Karten-Pins, OLED-Text, die 12-V-Lichtausgaenge (IO4/IO5).
- Ein schnellerer PWM-Takt (die Waveshare-Firmware nimmt 100 kHz).
- Die Waveshare-JSON-Befehle (`{"T":1,"L":0.2,"R":0.2}` usw.) sind **nicht** umgesetzt: Ein Raspberry Pi mit dem Waveshare-Programm `ugv_rpi` braeuchte sie ueber den UART
  (die Befehle sind einfach, sie lassen sich bei Bedarf in TinyC ergaenzen).
- Kamera, Lidar und Arm der Waveshare-Reihe gehoeren nicht dazu.
- Ein Abstandssensor: Ein VL53L0X/VL53L1X kommt an den I2C-Erweiterungsanschluss (Adresse 0x29, keine Kollision mit 0x3C, 0x42, 0x0C und dem IMU).

## Ein Programm fuer alles: `examples/rover.tc`

`rover.tc` fasst alles in **einem TinyC-Slot** zusammen: Motoren mit Totmann-Watchdog, Batterieschutz, IMU und Kompass, **vier VL53L0X-Bodensensoren hinter einem TCA9548A gegen die Treppe**, die Joystick-Seite und das Display. Ein Slot, weil auf einem klassischen ESP32 mit WLAN (`tinyc32-4M-plain`, gemessen an einem Board ohne Rover) jeder Slot allein etwa **9 KB RAM** kostet, egal was er tut; das ganze Programm (13 KB Bytecode) kostet rund 49 KB (30 KB intern plus Bytecode und Konstanten, die ein PSRAM-Board verdeckt). Frei bleiben nach dem Laden auf dem Rover etwa 60 KB Heap.

- **Treppenschutz:** Kanal 0..3 = vorn links, vorn rechts, hinten links, hinten rechts, schraeg nach aussen und unten (etwa 45 Grad). Ein Wert ausserhalb des Bodenfensters `RoverCliff <min> <max>` (Standard 25..110 mm) oder ein stummer Sensor gilt als Kante. Kante vorn: nur gerade rueckwaerts. Kante hinten: nur gerade vorwaerts. Beides oder ein Sensorfehler: gar keine Bewegung. `RoverCliff 0` schaltet den Schutz ab (wird protokolliert). Er laeuft in diesem Programm, egal wer die Befehle schickt. Treiber: `common/vl53l0x_mux.tc` (vom Ein-Sensor-Treiber abgeleitet, Zustand je Kanal).
- **Display:** die vier 21-Zeichen-Zeilen des Waveshare-OLED (Titel, IP, Zustand, Batterie), 128x32 Pixel oben links; `DSP_EXTRA 1` fuegt auf einem TFT ein groesseres Feld dazu.
- **Test ohne den Rover:** `ROVER_SIM 1` laesst die Motorpins in Ruhe und nimmt Batterie und Bodenabstaende aus `RoverVbat <mV>` / `RoverTof <Kanal> <mm>` (`-1` = stummer Sensor). So an einem ESP32 mit ILI9341 durchgespielt: vorwaerts / rueckwaerts / drehen wurden bei jeder Kombination aus vorn / hinten / beides / zu nah / stumm / Batterie leer / Schutz aus wie oben erlaubt oder verweigert, der Watchdog stoppte die Motoren.
- Noch nicht am Rover selbst und nicht mit echten VL53L0X-Sensoren getestet.
