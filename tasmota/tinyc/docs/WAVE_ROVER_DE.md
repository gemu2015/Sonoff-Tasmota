# WAVE ROVER mit TinyC (Waveshare General Driver for Robots)

Wie man den Waveshare **WAVE ROVER** (4WD-Metallfahrgestell) mit Tasmota/TinyC auf seiner ESP32-Treiberplatine betreibt, statt mit der
Waveshare-Firmware. Programm: `examples/wave_rover.tc`. English: [WAVE_ROVER.md](WAVE_ROVER.md).

> **Stand: nach der Waveshare-Dokumentation und dem Quellcode geschrieben, bevor der Rover da war. Auf dem Rover ist davon noch nichts gelaufen.**
> Die Fahrlogik (Mischung, PWM-Abbildung, Batterieschutz) wurde im IDE-Simulator auf dem Mac geprueft, Pins und I2C-Adressen stammen aus dem
> Waveshare-Quellcode. Bitte diese Datei nach dem ersten Test korrigieren.

## Die Hardware (Waveshare-Wiki und `waveshareteam/ugv_base_general`)

- Treiberplatine "General Driver for Robots": **ESP32-WROOM-32UE**, IPEX-Antennenbuchse (**Antenne angeschlossen lassen**), **16 MB Flash**
  (mit esptool gemessen am 11.11.2026: Hersteller 0x46, Baustein 0x4018; die Werks-Firmware nutzt nur die ersten 4 MB), kein PSRAM. Zwei USB-C-Buchsen: **"USB"** (ESP32, CP2102, automatische Download-Schaltung,
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

Bau `tinyc32-4M-plain` (klassischer ESP32, Partitionstabelle fuer 4 MB, ohne Matter und Kamera; etwa 238 KB Flash frei; er laeuft auf dem 16-MB-Chip und nutzt die ersten 4 MB). In *Konfiguration Modul* das generische Modul und nur
**GPIO32 = I2C SDA, GPIO33 = I2C SCL** setzen. **Die Motorpins (17, 21, 22, 23, 25, 26) unbelegt lassen**: TinyC lehnt Pins ab, die Tasmota fuer eine Funktion genommen hat.
Tasmotas eigener INA219-Treiber kennt die Adresse 0x42 nicht (noch pruefen); `wave_rover.tc` liest das Register der Busspannung selbst.
Lagesensor, Kompass: kein Tasmota-Treiber, die Register aus TinyC lesen (der Waveshare-Quellcode hat `QMI8658.cpp` / `AK09918.cpp` als Vorlage).

## Vor dem Flashen: das Original sichern

1. Rover oeffnen und ein USB-Kabel in die Buchse **USB** der Platine stecken (nicht LIDAR). Laut Waveshare-Wiki muss der Roboter dafuer zerlegt werden.
2. Chip und Flashgroesse pruefen, dann den ganzen Flash auslesen (esptool aus PlatformIO; esptool 5 schreibt `read-flash`, aeltere Versionen `read_flash`):

```bash
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX flash-id
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 230400 read-flash 0 0x1000000 wave_rover_werk_flash_16MB.bin
```

3. **Diese Datei aufheben** (nicht ins Repository: sie enthaelt die WLAN-Daten des Rovers, sobald sie eingetragen sind). Zurueck geht es mit
   `... write-flash 0 wave_rover_werk_flash_16MB.bin`. Den Quellcode der Waveshare-Firmware und einen Bau gibt es in `waveshareteam/ugv_base_general`
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
Am 10.11.2026 mit echten Daten geprueft (STL-19P an einem Mac: 1262 Pakete, 0 CRC-Fehler, 9,92 Umdrehungen pro Sekunde, rund 416 Pakete = 5000 Punkte pro Sekunde); **noch nicht auf einem ESP32 mit echtem Lidar gelaufen**.

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
- **Parkmodus** fuer einen stehenden Rover (laden oder nicht): `RoverPark 1` (oder `/rover_cmd?p=1`) verweigert die Motoren und bedient Bodensensoren, IMU, Kompass und Display nicht mehr (die Chips laufen weiter, nur ihr I2C-Verkehr und das Display stehen), die Schleife schlaeft, nur die Batterie wird alle 5 s gelesen. Ein Fahrbefehl oder `RoverPark 0` weckt den Rover; die Bodensensoren muessen erst wieder antworten, bevor er sich bewegen darf. `PARK_AUTO_S` > 0 parkt ihn nach so vielen Sekunden ohne Befehl. Die grossen Verbraucher sind nicht Teil des Programms: ein Lidar mit 230 mA braucht seinen eigenen 5-V-Schalter, der S3 schlaeft fuer sich. In der Simulation auf einem ESP32-S3 ausprobiert: parken, wecken per Fahrbefehl, wecken per `p=0`, automatisches Parken.
- Noch nicht am Rover selbst und nicht mit echten VL53L0X-Sensoren getestet.

## Am Rover gemessen (11.11.2026)

- **Flash mit 230400 Baud lesen**: bei 921600 und 460800 kamen "Invalid head of packet" / "Corrupt data" am CP2102. 16 MB dauern bei 230400 rund 13 Minuten. `verify-flash` gegen den Chip stimmte.
- I2C-Scan (GPIO32 = SDA, GPIO33 = SCL, gesetzt mit `Gpio32 640`, `Gpio33 608`, dann `Restart 1`): **0x0C** AK09918C, **0x3C** SSD1306, **0x42** INA219, **0x6B** QMI8658C, **0x7E** nicht zugeordnet; BMP280 (0x77) ist nicht bestueckt.
- **TinyC lehnte IO17 ab ("Forbidden pin")**: Tasmota sperrt GPIO16/17 beim klassischen ESP32 (PSRAM/Flash bei WROVER/PICO). Der WROOM-32UE hat keinen PSRAM, und der Rover nutzt IO17 (AIN2) und IO16 (Geber B). `tc_pin_red()` in `xdrv_124_tinyc_vm.h` gibt GPIO16/17 frei, wenn `FoundPSRAM()` falsch ist. Braucht eine Firmware, die nach dem 11.11.2026 gebaut wurde.
- Nach frischem Flashen ist die HTTP-Befehlsschnittstelle aus: einmal in der Web-Konsole `SetOption128 1` eingeben.
- Fahrtest aufgebockt: beide Seiten drehen richtig; das Chassis ist symmetrisch, "vorn" ist also Festlegung (vor dem Anbau der Bodensensoren und der Kamera entscheiden).


## Radgeber (11.11.2026)

Bibliothek `examples/common/wheel_enc.tc`, von `rover.tc` benutzt: Ticks, Strecke, Geschwindigkeit, Odometrie (x, y, Kurs) und ein **Blockierschutz** fuer die beiden Antriebsseiten. Gezaehlt wird mit dem **Hardware-Impulszaehler** des ESP32 (`pcntBegin`, mit Glitchfilter; ABI 35); er faellt auf Tasmotas eigene Zaehler (`Counter1` / `Counter2`, interruptgesteuert) zurueck, wenn er nicht starten kann. **Den Hardware-Zaehler benutzen:** Neben dem Motor-PWM liefen die Zaehler von Tasmota auf voller Geschwindigkeit, obwohl das Rad festgehalten wurde (4826 / 8093 Ticks in 3 s, 13.11.2026), eine Entprellung in Millisekunden kann die 2000..5000 echten Impulse pro Sekunde nicht von einer Spitze unterscheiden. Mit `pcntBegin` und 10 us Filter zaehlt das gehaltene Rad 172 Ticks in 3 s statt rund 8000, und beide Seiten zaehlen bei gleichem Befehl gleich (4399 / 4391).

1. Waveshare-Geber-Motoren an die Buchsen H3 / H4 stecken. Seite A (links) C1 = GPIO34, Seite B (rechts) C1 = GPIO27.
2. **Pins in Tasmota freigeben:** `Gpio34 0`, `Gpio27 0`, `Restart 1` (am Rover am 13.11.2026 gesetzt). Der Hardware-Zaehler braucht die Pins FREI (im Log steht `PCNT unit 0 on GPIO34, filter 10000 ns`). Mit `Gpio34 352` / `Gpio27 353` (`Counter1` / `Counter2`) scheitert `pcntBegin`, und `wheel_enc.tc` faellt auf diese Zaehler zurueck, mit der Stoerung. Rad von Hand drehen: die Ticks in `RoverEnc` laufen. Verdrahtung pruefen: Jede Motorbuchse muss den GEBER-Motor ihrer eigenen Seite tragen; laufen zwei Motoren einer Seite auf einem Kanal, darf das Drehen eines Rades von Hand kein Rad auf der anderen Seite mitdrehen (13.11.2026: zwei Motoren mussten getauscht werden).
3. `RoverEnc` zeigt Ticks und Zustand; `RoverEncZero`, den Rover eine gemessene Strecke rollen, `RoverEncCal <mm>` setzt die mm je Tick (der Vorgabewert ist **1078 Ticks je Radumdrehung**, am 13.11.2026 gemessen, indem jedes Rad des Rovers mit den Getriebe-Gebermotoren von Hand eine Umdrehung gedreht wurde: 1031 und 1125, Mittel 1078; die fruehere Schaetzung 350 = 7 Impulse x 50 : 1 war dreimal zu klein. Fuer einen genaueren Wert ein Rad 5 Umdrehungen drehen).
4. `RoverEnc 1` schaltet den Blockierschutz ein: eine Seite, die Befehl bekommt, aber 800 ms keinen Tick liefert, haelt den Rover an ("MOTOR STALL"), bis der Stick losgelassen wird. Er ist **standardmaessig aus**, weil ohne Geber jede Fahrt als Blockade zaehlen wuerde. Am Rover am 13.11.2026 mit festgehaltenem Rad und dem Hardware-Zaehler geprueft: Der Rover haelt rund 0,8 s nach dem letzten Tick der blockierten Seite an und bleibt aus, bis der Stick losgelassen wird. Der Schutz sieht nur die Raeder MIT Geber; ein blockiertes Rad ohne Geber neben einem drehenden Geber-Rad wird nicht erkannt.

Gezaehlt wird nur ein Kanal, die Richtung kommt also aus dem Motorbefehl (die letzte Richtung ungleich null). `ENC_BOTH 1` zaehlt auch Kanal B (`Gpio35 354`, `Gpio16 355`) und verdoppelt die Aufloesung. Mit `ROVER_SIM 1` werden die Ticks aus dem Befehl erzeugt (`RoverEncSim <Ticks je Sekunde>`, 0 = blockiertes Rad). In der Simulation auf der Rover-Platine geprueft; **noch nicht mit echten Gebern**. GPIO34 / GPIO27 sind mit Gebern nicht mehr frei (eine Lidarleitung von `CP_RX` braucht dann einen anderen Pin).

## RAM (11.11.2026, am Rover gemessen, kein PSRAM)

- `rover.tc` allein laesst etwa **50 KB** freien Heap (groesster Block 45 KB). Ein **zweiter Slot kostet weit mehr als sein Programm**: ein Testprogramm mit 4,8 KB in Slot 1 senkte den freien Heap von 48 KB auf 14 KB. Der Rover bleibt in EINEM Slot; Bankprogramme (`examples/cliff_test.tc`) laufen anstelle davon, nicht daneben. Ziel: 30 KB frei oder mehr.
- Die Joystick-Seite ist die Datei **`/rover.html`** (`examples/rover.html`), ausgeliefert mit `webSendFile`; einmal zusammen mit dem Programm hochladen (`curl -X POST "http://<ip>/ufsu?fsz=<Groesse>" -F "ufsu=@rover.html;filename=rover.html"`). Ohne die Datei ist `/rover` leer. Das nahm 2 KB Konstanten aus dem Bytecode.
- `ROVER_SIM` und `DSP_EXTRA` sind Uebersetzungsschalter (`#if`), Simulation und TFT-Feld stecken also nicht im Bytecode des echten Rovers: 16,1 KB -> 13,3 KB mit `DSP_EXTRA 0`.
- Weitere Einsparungen, noch nicht gemacht: Firmware ohne die Repo-IDE-Seite (`-DTINYC_NO_REPO_IDE`, etwa 4 KB statisch), weitere ungenutzte TinyC-Funktionsgruppen, MQTT/mDNS zur Laufzeit aus (nicht gemessen).

**RAM, dritte Runde (13.11.2026): kein Bluetooth im SDK, +37 KB.** Die Standard-Libs von Tasmota lassen Bluetooth eingeschaltet (`CONFIG_BT_ENABLED=y`). Das reserviert auf einem klassischen ESP32 rund 36 KB internen RAM, auch wenn es nie benutzt wird. Mit `custom_sdkconfig = CONFIG_BT_ENABLED=n` (und ohne die NimBLE-Bibliotheken) hat der Rover mit laufendem `rover.tc` **100 KB** frei statt 63 KB, der größte Block wächst von etwa 53 KB auf 98 304 B. Das Rover-Env erbt es von `tinyc32-4M-plain`. Gemessen mit `TinyCHeap` (interner Heap insgesamt 248 KB statt rund 207 KB).
