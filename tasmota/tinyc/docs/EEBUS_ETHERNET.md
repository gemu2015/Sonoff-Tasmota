# EEBUS images for ESP32-S3 with Ethernet (W5500) / EEBUS-Images für ESP32-S3 mit Ethernet (W5500)

## English

> ⚠️ **Test devices, not certified.** The Steuerbox and the SteuVE exist to test EEBUS communication with a HEMS (home
> energy management system) and to publish the results. They are not certified and replace neither the Steuerbox of the grid operator nor a
> tested device under § 14a EnWG. Do not use them for regular operation.

Two ready-made images for an **ESP32-S3 with 16 MB flash and PSRAM and a W5500 Ethernet chip** (tested hardware:
Waveshare ESP32-S3-ETH). The two EEBUS drivers exclude each other (same commands), so there is one image for each role:

| File | Role | Driver |
|---|---|---|
| `tinyc32s3-eth-guard.bin` / `.factory.bin` | **Steuerbox** (Energy Guard): gives a HEMS limits (§ 14a EnWG / § 9 EEG) | `xdrv_126_eebus_guard`, page `/steuerbox` |
| `tinyc32s3-eth-steuve.bin` / `.factory.bin` | **SteuVE** (controllable consumer): takes the limit a HEMS assigns it and switches consumers by script | `xdrv_128_eebus_steuve`, page `/steuve` |

Both contain TinyC, Ethernet (default `EthType 8` = W5500) and the EEBUS driver. They do **not** contain camera,
Matter or LVGL — the TLS/SHIP handshake needs the heap. No Wi-Fi credentials are built in.

For a Steuerbox and a SteuVE you need **two devices**.

**Illustrated manuals (German, PDF; addresses and identifiers blacked out)** by Andreas: [SteuVE](eebus_anleitungen/ANLEITUNG-SteuVE.pdf) (driver v016, script `steuve` v010) and [Steuerbox](eebus_anleitungen/ANLEITUNG-EEBUS-Steuerbox.pdf) (v187). HTML sources with the images: [`eebus_anleitungen/quellen/`](eebus_anleitungen/quellen/).

### Set up

1. **Flash.** New device: write the `.factory.bin` from address 0x0, for example with esptool or with web.esphome.io. ⚠️ With the Tasmota web installer the file was not written for one user (the progress ran to 100 % after a second and the board stayed empty; the ESPHome web installer worked). Device that already runs Tasmota: upload the
   `.bin` (OTA). ⚠️ Coming from another firmware the settings are reset, Wi-Fi included: make a backup first.
2. **Ethernet pins.** The images start with the Waveshare ESP32-S3-ETH template as the default, so a device that starts with default settings (new, or after `Reset 5`) already has the pins and `EthType 8`; a device that keeps older settings needs this step. *Configure Module* (or the console), for the Waveshare ESP32-S3-ETH (pins as given by the
   manufacturer, confirmed on two boards by the author of the drivers):

   | GPIO | Function (Tasmota component) | Console |
   |---|---|---|
   | 13 | SPI CLK | `Gpio13 736` |
   | 12 | SPI MISO | `Gpio12 672` |
   | 11 | SPI MOSI | `Gpio11 704` |
   | 14 | ETH MDC (= W5500 chip select) | `Gpio14 5568` |
   | 10 | ETH MDIO (= W5500 interrupt) | `Gpio10 5600` |
   | 9 | ETH POWER (= W5500 reset) | `Gpio9 5536` |

   Console in one line: `Backlog Gpio13 736; Gpio12 672; Gpio11 704; Gpio14 5568; Gpio10 5600; Gpio9 5536; EthType 8`,
   then `Restart 1`. Other W5500 boards: same six functions, other GPIO numbers. After the restart *Information* shows an
   Ethernet address.

   These six pins are confirmed on two Waveshare ESP32-S3-ETH boards. The board also has an SD card slot, which the
   drivers can use for their log and for the page (`steuerbox.html` / `steuve.html`): SD card CS on GPIO 4 (`6721`),
   second SPI bus on GPIO 5 MISO (`673`), GPIO 6 MOSI (`705`), GPIO 7 CLK (`737`). **The complete template**
   (*Configure Module → Template*, paste, tick *Activate*):

   ```
   {"NAME":"Waveshare ESP32-S3-ETH","ARCH":"ESP32S3","GPIO":[0,0,0,0,6721,673,705,737,0,5536,5600,704,672,736,5568,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1],"FLAG":0,"BASE":1}
   ```

   Then `EthType 8` and `EthAddress 1`, `Restart 1`.
3. **Upload the page** to the file system (*Consoles → Manage File System*): `steuerbox.html` (Steuerbox image) or
   `steuve.html` (SteuVE image). Both are attached to this release. Then open `http://<device>/steuerbox` or
   `/steuve`.
4. **Steuerbox:** commands and the pairing are described at the top of `xdrv_126_eebus_guard.ino`; `EEBusScan`,
   `EEBusConnect <ski|idx>`, then confirm the device's SKI at the HEMS. **SteuVE:** set `EEBusCsTyp wallbox` (the HEMS it was tested with accepted it under that identity; the default `wp`
   is untested), load the script *SteuVE Verbraucher* from the TinyC page (Repository box) into slot 0; description in `xdrv_128_eebus_steuve.md`
   (attached).

⚠️ **A used SD card keeps its files.** An erase or a new image does not touch the SD card: `tinyc.cfg`, the scripts (autostart!), `.pvs` files and copies of the certificate stay. Moving a card from a SteuVE to a Steuerbox (or the other way) starts the old script there, whose `EEBusCs…` commands answer "Unknown". Clear the card first (at a PC, or file by file: `UfsList`, then `UfsDelete /name`) or delete the slots in the TinyC page.

### Building it yourself

The images above already have what the drivers need. If you build `xdrv_126` / `xdrv_128` yourself, two settings are required, otherwise the connection fails: **`USE_MQTT_CLIENT_CERT`** (without it the TLS library never sends the device's own certificate, and the HEMS / charging station aborts with `handshake_failure`, `tls connect err=296` in the console) and **`SET_ESP32_STACK_SIZE (24 * 1024)`** (with 12 KB the device crashes while signing with ECDSA). From `universal` commit "EEBUS: USE_MQTT_CLIENT_CERT und 24-KB-Stapel automatisch" on, `my_user_config.h` sets both as soon as `USE_EEBUS_GUARD` or `USE_EEBUS_STEUVE` is defined; with an older checkout add these lines to `user_config_override.h`:

```c
#define USE_MQTT_CLIENT_CERT
#undef  SET_ESP32_STACK_SIZE
#define SET_ESP32_STACK_SIZE (24 * 1024)
```

For the W5500 the SDK settings of the build are mandatory too: `CONFIG_ETH_SPI_ETHERNET_W5500=y`, `CONFIG_LWIP_UDP_RECVMBOX_SIZE=64`, `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n` (with the Wi-Fi/lwIP buffers in PSRAM the box crashed in a TLS session) and `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768`. **Ready-made templates** by Andreas for the Waveshare ESP32-S3-ETH: [`eebus_vorlage/user_config_override.h`](eebus_vorlage/user_config_override.h) (pick exactly one of `EEBUS_STEUERBOX` / `EEBUS_STEUVE`) and [`eebus_vorlage/platformio_override.ini.example`](eebus_vorlage/platformio_override.ini.example) (env `tasmota32s3-waveshare-ETH-eebus`, with these SDK settings; copy it to `platformio_override.ini` in the repository root — the name ends in `.example` because `platformio_override.ini` is git-ignored). A forum user built and connected with them.

Background and discussion: https://www.photovoltaikforum.com/thread/263104-eebus-steuerbox-simulator-auf-tasmota-basis/

⚠️ **Own certificate, own risk.** Whoever reaches the device's SHIP port is accepted (the SKI of the peer comes from
the mDNS scan and is not checked against its certificate). Use it in your own network, not exposed to the Internet.

## Deutsch

> ⚠️ **Testgeräte, nicht zertifiziert.** Steuerbox und SteuVE dienen dazu, die EEBUS-Kommunikation mit einem HEMS
> (Energiemanagementsystem) zu prüfen und die Ergebnisse öffentlich zu machen. Sie sind nicht zertifiziert und ersetzen weder die Steuerbox des
> Messstellenbetreibers noch ein geprüftes Gerät nach § 14a EnWG. Nicht für den regulären Betrieb verwenden.

Zwei fertige Images für einen **ESP32-S3 mit 16 MB Flash, PSRAM und W5500-Ethernet** (erprobt: Waveshare
ESP32-S3-ETH). Die beiden EEBUS-Treiber schließen sich aus (gleiche Befehle), deshalb gibt es je Rolle ein Image:

| Datei | Rolle | Treiber |
|---|---|---|
| `tinyc32s3-eth-guard.bin` / `.factory.bin` | **Steuerbox** (Energy Guard): gibt einem HEMS Grenzen vor (§ 14a EnWG / § 9 EEG) | `xdrv_126_eebus_guard`, Seite `/steuerbox` |
| `tinyc32s3-eth-steuve.bin` / `.factory.bin` | **SteuVE** (steuerbare Verbrauchseinrichtung): übernimmt die Grenze, die ein HEMS ihr zuteilt, und schaltet per Skript Verbraucher | `xdrv_128_eebus_steuve`, Seite `/steuve` |

Beide enthalten TinyC, Ethernet (Vorgabe `EthType 8` = W5500) und den EEBUS-Treiber, aber **keine** Kamera, kein
Matter und kein LVGL — der TLS/SHIP-Aufbau braucht den Heap. WLAN-Zugangsdaten sind nicht eingebaut.

Für Steuerbox und SteuVE braucht man **zwei Geräte**.

**Bebilderte Bedienungsanleitungen (PDF, Adressen und Kennungen unkenntlich)** von Andreas: [SteuVE](eebus_anleitungen/ANLEITUNG-SteuVE.pdf) (Treiber v016, Skript `steuve` v010) und [Steuerbox](eebus_anleitungen/ANLEITUNG-EEBUS-Steuerbox.pdf) (v187). HTML-Quellen mit den Bildern: [`eebus_anleitungen/quellen/`](eebus_anleitungen/quellen/).

### Einrichten

1. **Flashen.** Neues Gerät: die `.factory.bin` ab Adresse 0x0 schreiben, zum Beispiel mit esptool oder mit web.esphome.io. ⚠️ Mit dem Tasmota-Web-Installer wurde die Datei bei einem Nutzer nicht geschrieben (der Fortschritt stand nach einer Sekunde bei 100 % und das Board blieb leer; der ESPHome-Web-Installer funktionierte). Gerät mit Tasmota: die `.bin` per
   OTA. ⚠️ Von einer anderen Firmware kommend werden die Einstellungen samt WLAN zurückgesetzt: vorher sichern.
2. **Ethernet-Pins.** Die Images starten mit dem Template des Waveshare ESP32-S3-ETH als Vorgabe; ein Gerät, das mit Standardeinstellungen startet (neu oder nach `Reset 5`), hat die Pins und `EthType 8` schon, ein Gerät mit älteren Einstellungen braucht diesen Schritt. Setzen (*Konfiguration → Gerät konfigurieren* oder Konsole), für das Waveshare ESP32-S3-ETH
   (Belegung laut Hersteller, auf zwei Boards vom Autor der Treiber bestätigt): siehe Tabelle oben. Konsole in einer Zeile:
   `Backlog Gpio13 736; Gpio12 672; Gpio11 704; Gpio14 5568; Gpio10 5600; Gpio9 5536; EthType 8`, danach `Restart 1`.
   Andere W5500-Boards: dieselben sechs Funktionen, andere GPIO-Nummern. Nach dem Neustart zeigt *Information* eine
   Ethernet-Adresse.

   Diese sechs Pins sind auf zwei Waveshare-ESP32-S3-ETH-Boards bestätigt. Das Board hat auch einen SD-Kartenplatz, den
   die Treiber für ihr Protokoll und für die Seite (`steuerbox.html` / `steuve.html`) nutzen können: SD-Karte CS an
   GPIO 4 (`6721`), zweiter SPI-Bus an GPIO 5 MISO (`673`), GPIO 6 MOSI (`705`), GPIO 7 CLK (`737`). **Das komplette
   Template** (*Gerät konfigurieren → Template*, einfügen, *Aktivieren* ankreuzen):

   ```
   {"NAME":"Waveshare ESP32-S3-ETH","ARCH":"ESP32S3","GPIO":[0,0,0,0,6721,673,705,737,0,5536,5600,704,672,736,5568,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1],"FLAG":0,"BASE":1}
   ```

   Danach `EthType 8` und `EthAddress 1`, `Restart 1`.
3. **Seite hochladen** ins Dateisystem (*Konsolen → Dateisystem verwalten*): `steuerbox.html` (Steuerbox-Image) bzw.
   `steuve.html` (SteuVE-Image), beide hängen an diesem Release. Dann `http://<Gerät>/steuerbox` bzw. `/steuve` öffnen.
4. **Steuerbox:** Befehle und Kopplung stehen am Anfang von `xdrv_126_eebus_guard.ino`; `EEBusScan`,
   `EEBusConnect <ski|idx>`, dann die SKI des Geräts beim HEMS bestätigen. **SteuVE:** `EEBusCsTyp wallbox` setzen (unter dieser Kennung hat das erprobte HEMS sie angenommen, die
   Vorgabe `wp` ist ungetestet), auf der TinyC-Seite (Repository-Box) das Skript *SteuVE Verbraucher* in Slot 0 laden; Beschreibung in `xdrv_128_eebus_steuve.md`
   (angehängt).

⚠️ **Eine benutzte SD-Karte behält ihre Dateien.** Ein Erase oder ein neues Image rührt die SD-Karte nicht an: `tinyc.cfg`, die Skripte (Autostart!), `.pvs`-Dateien und Kopien des Zertifikats bleiben. Wer eine Karte von einer SteuVE in eine Steuerbox steckt (oder umgekehrt), startet dort das alte Skript, dessen `EEBusCs…`-Befehle mit „Unknown“ antworten. Die Karte vorher leeren (am PC oder Datei für Datei: `UfsList`, dann `UfsDelete /name`) oder die Slots auf der TinyC-Seite löschen.

### Selbst bauen

Die Images oben haben schon, was die Treiber brauchen. Wer `xdrv_126` / `xdrv_128` selbst baut, braucht zwei Einstellungen, sonst scheitert die Verbindung: **`USE_MQTT_CLIENT_CERT`** (ohne sie schickt die TLS-Bibliothek das eigene Zertifikat nie, und das HEMS bzw. die Ladestation bricht mit `handshake_failure` ab, in der Konsole `tls connect err=296`) und **`SET_ESP32_STACK_SIZE (24 * 1024)`** (mit 12 KB stürzt das Gerät beim Signieren mit ECDSA ab). Ab dem `universal`-Commit „EEBUS: USE_MQTT_CLIENT_CERT und 24-KB-Stapel automatisch“ setzt `my_user_config.h` beides, sobald `USE_EEBUS_GUARD` oder `USE_EEBUS_STEUVE` definiert ist; bei einem älteren Stand diese Zeilen in die `user_config_override.h`:

```c
#define USE_MQTT_CLIENT_CERT
#undef  SET_ESP32_STACK_SIZE
#define SET_ESP32_STACK_SIZE (24 * 1024)
```

Für den W5500 sind außerdem die SDK-Einstellungen des Baus zwingend: `CONFIG_ETH_SPI_ETHERNET_W5500=y`, `CONFIG_LWIP_UDP_RECVMBOX_SIZE=64`, `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=n` (mit den WLAN-/lwIP-Puffern im PSRAM stürzte die Box in einer TLS-Sitzung ab) und `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768`. **Fertige Vorlagen** von Andreas für das Waveshare ESP32-S3-ETH: [`eebus_vorlage/user_config_override.h`](eebus_vorlage/user_config_override.h) (genau eines von `EEBUS_STEUERBOX` / `EEBUS_STEUVE` wählen) und [`eebus_vorlage/platformio_override.ini.example`](eebus_vorlage/platformio_override.ini.example) (Env `tasmota32s3-waveshare-ETH-eebus`, mit diesen SDK-Einstellungen; ins Hauptverzeichnis des Repositorys als `platformio_override.ini` kopieren — der Name endet auf `.example`, weil `platformio_override.ini` von git ignoriert wird). Ein Forum-Nutzer hat damit gebaut und sich verbunden.

Hintergrund und Diskussion: https://www.photovoltaikforum.com/thread/263104-eebus-steuerbox-simulator-auf-tasmota-basis/

⚠️ **Eigenes Zertifikat, eigenes Risiko.** Wer den SHIP-Port des Geräts erreicht, wird angenommen (die SKI der Gegenstelle
stammt aus dem mDNS-Scan und wird nicht gegen ihr Zertifikat geprüft). Im eigenen Netz betreiben, nicht aus dem Internet erreichbar machen.
