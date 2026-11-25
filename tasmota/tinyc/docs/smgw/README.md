# SMGW reader 1.2.2: smart meter gateway over the HAN interface with TinyC

By Andreas (forum name meierchen006), taken over into the repository with his consent. Reads the official 15-minute meter
readings (consumption 1.8.0, feed-in 2.8.0) from a PPC smart meter gateway over its HAN interface, keeps them as text on the SD card, shows day / month / year
and offers CSV and an Excel query. Optional daily FTP backup. Optional electricity cost calculation from the same readings (slot 2 and `smgw_kosten.html`).

**Version 1.2.2 (25.11.2026)** – replaces 1.2.1. The parts carry their own versions: slot 0 **1.1.6**, slot 1 **1.1.0**, slot 2 **1.1.1**, `smgw.html` **1.2.1**, `smgw_kosten.html` and the guide **1.2.2**.
The scripts and the guide have the version in their file names, the two pages keep their names (the firmware serves them as `/smgw.html.gz` and `/smgw_kosten.html.gz`) and show it inside.
`VERSION-1.2.2.txt` lists what goes where on the device and the md5 sums of all files.

New in 1.2.2 (details in `VERSION-1.2.2.txt`):
- **One download for all files**: the release "testing" also has `tinyc32s3-eth-smgw-dateien.zip` with everything for the SD card (folder `1-SD-Karte`) and the slots
  (folder `2-TinyC-IDE-Slots`), plus README, VERSION, the guide and the Excel example. No more single downloads from GitHub, and Windows cannot append `.txt` to the `.tc` files.
- **IDE** (release 1.6.83): shows "Close" also when opened via `smgw.local`, and after "Compile" says why no device answers ("Device requires a password (HTTP 401)",
  "No device address entered", "No device reachable at <address>") instead of always "No device reachable". `TinyCIde` / "IDE aktualisieren" now fetch the same IDE.
- **Guide 1.2.2**: section 5.1 starts with the zip download; notes on the IDE updated (5.2, 5.3). The ready-made image was tested on 25.11.2026 on a fresh
  ESP32-S3-ETH, including a cold start after several minutes without power (SD card over SPI found at once, all slots started).
- Scripts and pages unchanged.

New in 1.2.1:
- **Ready-made firmware** for the ESP32-S3-ETH: image `tinyc32s3-eth-smgw` in the release "testing" (`tinyc32s3-eth-smgw.factory.bin` for the first flash over USB,
  `tinyc32s3-eth-smgw.bin` for updates). No Wi-Fi, no MQTT, no location, **no fixed address** (LAN and Wi-Fi by DHCP) and **no FTP server**; hotspot and host name `SMGW`,
  mDNS on (`http://smgw.local/`). Build it yourself only for the ESP32-P4 or for own changes (`firmware/`, guide 4.1–4.3).
- **LAN address of the reader**: the HAN socket has no DHCP server, so the user enters a free address in the gateway's network once, tab "Einstellungen" of the reader page (guide 5.4).
  `smgw.html` 1.2.1 leaves the field empty while the LAN runs on DHCP (before it showed e.g. "0.0.0.0 (192.168.x.y)").
- **Cost page 1.2.2**: saving the tariff sheet also writes a **price plan** for other devices in the home network (e.g. an energy manager), so they can compute the
  energy price per quarter hour themselves: `/smgw_preisplan.json` and the same numbers as one line in `/smgw_preisplan.txt` (for TinyC scripts). Main meter only, no names, no variants.
  "So sieht ein Tag aus" shows the 96 quarter hours with equal width, grouped by hour.
- **Guide 1.2.1**: commissioning rewritten step by step as tested on a fresh reader (25.11.2026): pages via "Werkzeuge → Dateisystem verwalten" onto the SD card, scripts via the
  TinyC IDE into the slots, autostart "A" green (with pictures); direct download links; Windows may append `.txt` to downloaded `.tc` files.

New in 1.2.0:

- **Gateway logbook** (slot 0 1.1.6, tab "Logbuch" on the reader page): fetches the consumer logbook of the gateway only on a button press (`SMGWLogbuch`), day by day, into `/smgw_logbuch.txt`;
  identifiers are masked, the reader's own logins are left out. The page shows the last 12 errors and the days with fewer than 96 quarter hours.
- **Login / logout switch** on the reader and the cost page (only when a web password is set), so commands such as "download logbook" no longer seem to do nothing without a login.
- **Tariff variants** for the tariff comparison, **monthly advance payment per tariff sheet**, charts with a **scale** (kW per quarter hour, kWh or EUR per month / billing year).
- `SMGWTestTag` stores the raw answer as `/smgw_testtag.secret` (it contains the meter number; port 82 blocks `.secret` files).

- `slot0-smgw_1.1.6.tc`: the reader (TLS, digest login or client certificate, evaluation in 1 KB pieces, files, clock-driven schedule, FTP backup, console `SMGW...`).
  The first fetch starts at 01.01.2024 (§14a EnWG) and takes everything the gateway has: days for which the gateway answers "Keine Zählerdaten vorhanden"
  before the first reading are skipped and not stored; any other answer without values stops the fetch with an error instead of skipping silently.
  Once a day it stores the validity of the gateway's TLS certificate in `/smgw_gateway.json` (needs the firmware command `TinyCTlsGueltig`, see below).
  `SMGWTestTag YYYY-MM-DD` queries one day without storing anything (log line + raw answer in `/smgw_testtag.secret`), only after a successful login.
- `slot1-smgwweb_1.1.0.tc`: button on the main page, `/smgw`, `/smgwj`, `/smgwf`, `/smgwzugang`. Own slot because slot 0 is busy during a fetch.
- `slot2-smgwkosten_1.1.1.tc` (optional, needs TinyC 1.6.78 or later): fetches the day-ahead exchange prices DE-LU (15 min) from api.energy-charts.info, one request per month file
  (`/smgw_boerse_JJJJ-MM.txt`), from the first year with readings (at most six years back, seven calendar years kept), the next day from 14:00;
  never while slot 0 talks to the gateway. Serves `/smgwkosten` and `/smgwb`.
- `smgw.html` (+ `.gz`, version 1.2.1 inside): the reader page. Only `smgw.html.gz` belongs on the SD card (the device always serves the `.gz` and falls back to the
  plain `.html` only when no `.gz` exists); `smgw.html` is the readable source (repack with `gzip -9 -k -n`). The switch "SMGW-Leser | Stromkosten" appears only when slot 2 answers.
  Day chart: the tooltip shows the whole quarter hour ("16:45 – 17:00 Uhr"); on the October DST day the stored quarter 02:45 covers 75 minutes
  (the gateway sends 02:00–02:45 twice, the first one is kept) and is shown as such. Button "Als PDF drucken" (one A4 page) in day, month, year and all years.
- `smgw_kosten.html` (+ `.gz`, optional extension, only the `.gz` on the SD card): costs per quarter hour, day, month and billing year, with a box "§14a EnWG – calculated benefit"
  (module 1 flat rate per BNetzA BK8-22/010-A Rn. 92/93, 80 EUR gross + 3,750 kWh × grid price gross × 0.2; module 3 = cost with module 1 minus cost with modules 1 + 3).
  Tariffs as one **tariff sheet per year**. All time windows (supplier, module 3, offers) are quarter hours: "from" is included, "to" is not
  (22:00–06:00 = 22:00:00 to 05:59:59). Tariffs are stored in `/smgw_tarif.json` (written over port 83, read back and compared); before every save the previous
  version is kept as `/smgw_tarif.bak`, and saving is blocked if an existing sheet could not be read (so it is never overwritten with the example values).
  A recalculation without warranty; only the supplier's bill counts.
- `Muster-15min-Werte_1.0.1.xlsx`: example workbook for the Excel import of 15-minute values (guide, picture 11).
- `ANLEITUNG-SMGW-Leser_1.2.2.pdf` (+ `quellen/Anleitung-SMGW-Leser-quelle.html` and `quellen/bilder/`): where to get the files and where each one goes (section 5.1), hardware,
  network, finding the reader again, ready-made firmware or build variant, commissioning, credentials, files, operation, FTP backup, certificate, FAQ, and in section 11 the optional cost
  calculation (slot 2) including "so wird gerechnet" (all formulas).

**Own use, own risk.** Ready-made firmware for the ESP32-S3-ETH in the release (see above); for the ESP32-P4 build it yourself (see the guide). Tested only with the hardware named in the guide (Waveshare ESP32-S3-ETH or
ESP32-P4-WIFI6-POE-ETH, both with LAN to the HAN socket and WLAN to the home network), a PPC gateway and a meter with consumption and feed-in.
Other gateways and meters without 2.8.0 are not tested.

**Firmware requirements when building it yourself** (the ready-made image already has all of this):
- TinyC branch `universal` from 18.11.2026 (commit `dcf163558`, `TC_MAX_FILE_HANDLES` adjustable) with `#define TC_MAX_FILE_HANDLES 8` in the device block (guide 4.3).
  Also needed: TinyC 1.6.78 or later (for `TinyCTlsGueltig`); `xdrv_124_tinyc_vm.h` changes (`TC_TLS_RX_BUF`, `tlsConnect` offering ECDSA) and,
  for login by client certificate, `TinyCTlsCert` (needs `USE_MQTT_CLIENT_CERT`).
- No fixed LAN address in the device block and no FTP server (`#undef USE_FTP`: it would hand out the `.secret` files; the nightly backup is the FTP client in the script).
- Finding the reader by name: `USE_DISCOVERY` + `WEBSERVER_ADVERTISE` in the device block, topic `SMGW` and `SetOption55 1` (`http://smgw.local/`).
  If the reader replaces an older device with the same name, delete the old entry in the FRITZ!Box network list.

**Security:** no credentials, meter numbers or gateway identifiers are in these files. The credentials live only in `*.secret` files on the device; TinyC blocks them on
ports 82/83, but Tasmota's own file manager (port 80) can download them as long as no web password is set – set one (Configuration → Other).
The pictures in the guide use example tariffs; addresses, identifiers and serial numbers are hidden or invented.
`smgw.html` loads Chart.js from a CDN (cdnjs) and, only when a certificate file is opened, node-forge (with an integrity hash).
Exchange prices: Bundesnetzagentur | SMARD.de via energy-charts.info (Fraunhofer ISE), licence CC BY 4.0; the source is named on the cost page.
