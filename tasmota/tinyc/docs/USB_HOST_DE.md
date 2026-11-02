# USB-Host am ESP32-S3 / S2: Stick, Audio-Stick, FTDI

Treiber `xdrv_99_usb_host.ino`. Der ESP32 ist hier der **Host**: ein USB-Stick wird zum Dateisystem `/usb`, ein
USB-Audio-Stick spielt WAV-Dateien, ein FTDI-Adapter (VarioLab) wird zur seriellen Schnittstelle. Ein Host, mehrere
Geräte gleichzeitig hinter einem Hub. English: [USB_HOST.md](USB_HOST.md).

## Welche Chips

Nur Chips mit USB-OTG haben einen Host-Stapel: **ESP32-S2 und S3** (hier freigegeben) und **ESP32-P4** (hat den
Stapel, noch nicht portiert). ESP32, C2/C3/C5/C6/C61/H2 haben keinen Host; dort wird der Treiber zu nichts
(`#warning`). Das Board muss an der Buchse **VBUS liefern**, und der Stecker muss passen (das GOOUUU S3-CAM nimmt
keinen normalen USB-A-Stecker).

Das TinyC-Testrelease-Image für den ESP32-S3 (`tinyc32s3`) wird mit allem gebaut, auch mit Hub-Unterstützung (+44 kB Flash).
Für den eigenen Bau:

## Schalter (`user_config_override.h` oder `-D` in der Umgebung)

| Schalter | Wirkung |
|---|---|
| `USE_USB_HOST` | Host, Hub, Geräteliste, Befehle (Pflicht) |
| `USE_USB_HOST_MSC` | USB-Stick als FAT-Dateisystem `/usb` |
| `USE_USB_HOST_MSC_AUTOMOUNT` | Stick etwa 2,5 s nach dem Anstecken einhängen (sonst `UsbHostMount 1`) |
| `USE_USB_HOST_AUDIO` | USB-Audio Klasse 1.0 abspielen (16 Bit PCM, 44,1/48 kHz) |
| `USE_USB_HOST_SERIAL` | FTDI FT232 als serieller Port (`usbInit()` … in TinyC laufen über diesen Treiber) |
| `USE_USB_HOST_AUTOSTART` | Host beim Start starten (sonst `UsbHost 1`) |

**Hubs** brauchen `CONFIG_USB_HOST_HUBS_SUPPORTED=y` im SDK. Mit PlatformIO heisst das
`custom_sdkconfig = CONFIG_USB_HOST_HUBS_SUPPORTED=y` in der Umgebung — das baut die gemeinsamen Arduino-Bibliotheken
einmal neu (etwa 8 Minuten) und ändert `sdkconfig.defaults` / löscht `CMakeLists.txt`: beide vor dem Commit mit
`git checkout` zurückholen. Ohne Hub-Option geht nur **ein** Gerät an der Buchse.

## Befehle

| Befehl | |
|---|---|
| `UsbHost [0\|1]` | Host stoppen / starten; ohne Argument Zustand und angesteckte Geräte |
| `UsbHostFifo rx,nptx,ptx` | FIFO-Zeilen des Hosts (Standard 80,40,80; **die Summe muss ≤ 200 bleiben**, sonst meldet sich kein Gerät an, ohne Meldung) |
| `UsbHostMsc` | Geräte auflisten; mit Stick ein Lesetest |
| `UsbHostMount [0\|1]` | Stick aus-/einhängen (das Einhängen läuft in einem eigenen Task) |
| `UsbHostMic [Rate]` | Aufnahme vom Mikrofoneingang des Sticks: öffnen / `0` schließen / Zustand und Pegel |
| `UsbHostTone`, `UsbHostPlay <pfad>`, `UsbHostStress <pfad>` | Audio: 440-Hz-Testton, 16-Bit-PCM-WAV abspielen (jede Rate, wird umgerechnet), Dauertest |
| `UsbHostSerial <baud>`, `UsbHostSerialRead [n]`, `UsbHostSerialWrite <hex>`, `UsbHostBridge <port>` | FTDI-Port, TCP-Brücke |

Logzeilen beginnen mit `USB:` (Web-Konsole / `/cs`).

## Der Stick als Dateisystem

* FAT12/16/32 mit 512-Byte-Blöcken. **Kein exFAT, kein NTFS** (das Log sagt es).
* Eingehängt unter `/usb`; `usbfsp` ist der `FS`-Zeiger (nullptr, solange nichts eingehängt ist).
* **Tasmota-Dateimanager**: `/ufsd?dir=3` (Verweis „USB“ neben Flash und SD).
* **TinyC**: Pfadpräfix `/usb/` wie `/ffs/` (Flash) und `/sdfs/` (SD): `fileOpen("/usb/log.csv", 2)`.
  Ohne Stick liefert `fileOpen` -1.
* Upload über Port 83: `POST /ufs/usb/<pfad>`; neuer Befehl **`UfsMkdir <präfix><pfad>`** legt einen Ordner samt
  Elternordnern an (Präfix `/ffs/`, `/sdfs/`, `/usb/`).
* Geschwindigkeit: rohes Lesen etwa 1,1 MB/s; über den Web-Dateimanager etwa 105 kB/s schreiben, 167 kB/s lesen.

Was der Stick verlangt (alles im Treiber erledigt; beim Ändern beibehalten):

* Nach Release und erneutem Claim beginnt der Host mit Datentoggle 0: vor dem ersten Befehl die **Bulk-Only Reset
  Recovery** senden (Klassenanforderung `0x21/0xFF`, `CLEAR_FEATURE(HALT)` auf beide Endpunkte), sonst hängt der
  erste Befehl und der Wächter setzt das Gerät zurück.
* Der erste Befehl nach dem Anstecken meldet „medium changed“: mit REQUEST SENSE quittieren.
* Ein Stick, der angesteckt wird, während ein Hub noch die anderen Geräte anmeldet, antwortet manchmal dauerhaft mit
  Fehlern. Deshalb wartet der Automount 2,5 s, und das Einhängen versucht es dreimal (loslassen, 1 s warten, neu belegen).
* Die Rückrufe der Übertragungen laufen im USB-Klienten-Task: **dort nie einhängen oder auf eine Übertragung warten**
  (eigener Task, nur ein Einhängen zugleich).

## TinyC-Dateimanager (`examples/filemanager.tc` + `filemanager.html`)

Zwei Fenster (Flash / SD / USB), eine Zeile auf das andere Fenster ziehen kopiert (Umschalt = verschieben), Dateien vom
Rechner hineinziehen lädt hoch, Ordner rekursiv. `filemanager.html` als `/filemanager.html` auf den Flash legen, das
Skript **in Slot 0** starten (`webOn` geht nur in Slot 0), `http://<gerät>/fm` öffnen. Das Skript kopiert in
64-kB-Scheiben (`/fm/op?c=cp`), alles andere läuft über Tasmotas `/ufsd`, Port 83 und `UfsMkdir`.

## Audio

Klasse 1.0 per isochronem OUT (6 Übertragungen zu 8 × 1 ms), Abtastrate per `SET_CUR` auf den Endpunkt, lineare
Umrechnung, halber Pegel. Nur WAV (16 Bit PCM); MP3 braucht einen Decoder und ist nicht gebaut. Braucht die obige
FIFO-Aufteilung (die Standardaufteilung nimmt kein 200-Byte-Isochronpaket).

**Aus TinyC** ist der Stick der Ausgang von `i2sBegin` / `i2sWrite` / `i2sStop`: `i2sBegin(-1, -1, -1, -2, rate)` aufrufen
(Datenpin **-2** = USB-Audio-Stick) und Mono-Samples mit 16 Bit schreiben wie für einen I2S-Verstärker. Der Treiber rechnet
`rate` auf die Rate des Sticks um, `i2sWrite` blockiert, solange der Ring (8192 Samples) voll ist (das taktet das Skript),
und `i2sStop` lässt den Ring erst auslaufen. Der Host wird bei Bedarf gestartet. `examples/wav_player.tc` macht es mit
`#define CODEC_USB` (`CODEC_NONE` auskommentieren): `WAVplay /ffs/x.wav`, `WAVtone 440`, jede WAV-Rate läuft in der richtigen
Geschwindigkeit.

**Aus dem Audio-Plugin** (`I2SAUDIO`: MP3, Webradio, Sprachausgabe, Zeitansage, `I2SPlay`): `UsbHostRoute 1` schickt die
I2S-Ausgabe des Plugins **während es spielt** an den Stick; `UsbHostRoute 0` schickt sie wieder an die I2S-Pins. Das Plugin
selbst bleibt unverändert: Alle seine I2S-Aufrufe gehen durch eine Firmware-Funktion (`tmod_i2s` in `xdrv_123_plugins.ino`),
die die Samples (mono oder stereo, jede Rate) an denselben Strom wie oben übergibt. Der Stick wird bei „enable“ belegt und bei
„disable“ zurückgegeben, ist also zwischen zwei Wiedergaben frei. Fehlt der Stick oder ist er belegt, spielt das Plugin wie
bisher auf I2S. `UsbHostLevel 1..100` stellt die Lautstärke des Sticks für die Ströme ein (Standard 50; das Plugin hat seine
eigene Lautstärke zusätzlich). `UsbHostRoute` ohne Argument zeigt Route, Pegel, ob ein Strom offen ist und die Zahl der
Unterläufe. Die Mikrofonseite des Plugins bleibt am I2S.
Einrichtung des Plugins auf einem Board ohne I2S-Verstärker: `chkpt a2` (128-kB-Plugin-Partition, **formatiert das
Flash-Dateisystem**), `I2SAUDIO_32.bin` auf der Plugin-Seite (`/modu`) hochladen, eine `I2SAUDIO.cfg` mit Codec 0 und freien
Pins auf den Flash legen (sonst meldet `iniz` „Codec error“), dann `iniz <Slot>` und „Autostart plugins at boot“ anhaken.
Mit einer MP3 auf .190 geprüft.

**Aufnahme vom Mikrofoneingang des Sticks** (isochron IN, UAC1, 16 Bit mono): `UsbHostMic <Rate>` öffnet ihn (der Ring füllt sich, 0 schließt),
`UsbHostMic` allein zeigt `Rms`/`Peak` der neuesten Samples, die Paketzähler und `NonEmpty` (Pakete mit Daten). Aus TinyC nehmen die vorhandenen
Mikrofonfunktionen den Stick, wenn der Datenpin **-2** ist: `i2sMicBegin(-1, -1, -1, -2, 16000)`, dann `i2sMicRead(buf, n)` und `i2sMicLevel()`
wie bei einem I2S-Mikrofon, am Ende `i2sMicStop()`. Der Treiber rechnet die 48 kHz des Sticks durch Mitteln auf die gewünschte Rate um (kein
Aliasing bei 16 kHz). Wiedergabe und Aufnahme zusammen passen nur in die 8 Host-Kanäle, solange kein Stick eingehängt ist (siehe unten).
⚠️ An einem C-Media 0D8C:000C gefunden: Nach einem einfachen Claim der Aufnahme-Schnittstelle ist jedes Paket leer (Länge 0, kein Fehler),
egal wie Stummschaltung, Lautstärke, Selector und Abtastrate gesetzt sind. Erst ein ausdrückliches `SET_INTERFACE` auf Alternative 0 und zurück
auf die Streaming-Alternative startet die Aufnahme, und sie bleibt dann bis zum Stromverlust des Sticks gestartet. Der Treiber macht das
immer; außerdem setzt er die Abtastrate und schaltet die Stummschaltung des Mikrofons aus und die Lautstärke auf die Mitte des Bereichs
(Feature-Unit des Aufnahmepfads).

⚠️ **Die OTG-Hardware hat nur 8 Host-Kanäle.** Jedes angesteckte Gerät belegt einen (seine Steuerleitung), der Hub einen für
seinen Interrupt-Endpunkt, und jeder belegte Endpunkt einen: ein eingehängter Stick zwei, ein Audio-Strom einen. Hub + FTDI +
Stick + Audio-Stick + Kartenleser sind neun, dann scheitert die Audio-Belegung bei eingehängtem Stick mit `0x106`
(`ESP_ERR_NOT_SUPPORTED`; nach `UsbHostMount 0` geht es). `UsbHostMsc` belegt den Stick für seinen Lesetest ebenfalls, also nicht als Statusabfrage vor dem Abspielen aufrufen. Ein nicht gebrauchtes Gerät (den Kartenleser) abziehen oder den Stick
nur bei Bedarf einhängen.

## Nicht gebaut / nicht getestet

* ESP32-P4 (wohl High-Speed, Sticks wären deutlich schneller; braucht `peripheral_map` und eigene FIFO-Werte).
* Stick im eingehängten Zustand abziehen (der Aushängepfad ist da, auf Hardware nicht geübt).
* FTDI-Daten eines laufenden VarioLab durch den Hub, die TCP-Brücke, Schreiben über den SD-Kartenleser.
