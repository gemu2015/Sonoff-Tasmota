# SMGW reader 1.0.0: smart meter gateway over the HAN interface with TinyC

By Andreas (forum name meierchen006), taken over into the repository with his consent. Reads the official 15-minute meter
readings (consumption 1.8.0, feed-in 2.8.0) from a PPC smart meter gateway over its HAN interface, keeps them as text on the SD card, shows day / month / year
and offers CSV and an Excel query. Optional daily FTP backup. Optional electricity cost calculation from the same readings (slot 2 and `smgw_kosten.html`).

**Version 1.0.0 (13.11.2026)** – first published release. All parts carry the same version; the scripts and the guide have it in their file names,
the two pages keep their names (the firmware serves them as `/smgw.html.gz` and `/smgw_kosten.html.gz`) and show it inside.

- `slot0-smgw_1.0.0.tc`: the reader (TLS, digest login or client certificate, evaluation in 1 KB pieces, files, clock-driven schedule, FTP backup, console `SMGW...`).
  The first fetch starts at 01.01.2024 (§14a EnWG) and takes everything the gateway has: days for which the gateway answers "Keine Zählerdaten vorhanden"
  before the first reading are skipped and not stored; any other answer without values stops the fetch with an error instead of skipping silently.
  Once a day it stores the validity of the gateway's TLS certificate in `/smgw_gateway.json` (needs the firmware command `TinyCTlsGueltig`, see below).
  `SMGWTestTag YYYY-MM-DD` queries one day without storing anything (log line + raw answer in `/smgw_testtag.html`), only after a successful login.
- `slot1-smgwweb_1.0.0.tc`: button on the main page, `/smgw`, `/smgwj`, `/smgwf`, `/smgwzugang`. Own slot because slot 0 is busy during a fetch.
- `slot2-smgwkosten_1.0.0.tc` (optional, needs TinyC 1.6.78 or later): fetches the day-ahead exchange prices DE-LU (15 min) from api.energy-charts.info, one request per month file
  (`/smgw_boerse_JJJJ-MM.txt`), from the first year with readings (at most six years back, seven calendar years kept), the next day from 14:00;
  never while slot 0 talks to the gateway. Serves `/smgwkosten` and `/smgwb`.
- `smgw.html` (+ `.gz`, version 1.0.0 inside): the reader page. Only `smgw.html.gz` belongs on the SD card (the device always serves the `.gz` and falls back to the
  plain `.html` only when no `.gz` exists); `smgw.html` is the readable source (repack with `gzip -9 -k -n`). The switch "SMGW-Leser | Stromkosten" appears only when slot 2 answers.
  Day chart: the tooltip shows the whole quarter hour ("16:45 – 17:00 Uhr"); on the October DST day the stored quarter 02:45 covers 75 minutes
  (the gateway sends 02:00–02:45 twice, the first one is kept) and is shown as such. Button "Als PDF drucken" (one A4 page) in day, month, year and all years.
- `smgw_kosten.html` (+ `.gz`, optional extension, only the `.gz` on the SD card): costs per quarter hour, day, month and billing year, with a box "§14a EnWG – calculated benefit"
  (module 1 flat rate per BNetzA BK8-22/010-A Rn. 92/93, 80 EUR gross + 3,750 kWh × grid price gross × 0.2; module 3 = cost with module 1 minus cost with modules 1 + 3).
  Tariffs as one **tariff sheet per year**. All time windows (supplier, module 3, offers) are quarter hours: "from" is included, "to" is not
  (22:00–06:00 = 22:00:00 to 05:59:59). Tariffs are stored in `/smgw_tarif.json` (written over port 83, read back and compared); before every save the previous
  version is kept as `/smgw_tarif.bak`, and saving is blocked if an existing sheet could not be read (so it is never overwritten with the example values).
  A recalculation without warranty; only the supplier's bill counts.
- `ANLEITUNG-SMGW-Leser_1.0.0.pdf` (+ `quellen/Anleitung-SMGW-Leser_1.0.0.html` and `quellen/bilder/`): where to get the files and where each one goes (section 5.1), hardware,
  network, finding the reader again, build variant, commissioning, credentials, files, operation, FTP backup, certificate, FAQ, and in section 11 the optional cost
  calculation (slot 2) including "so wird gerechnet" (all formulas).

**Own use, own risk.** No ready-made firmware: it has to be built by yourself (see the guide). Tested only with the hardware named in the guide (Waveshare ESP32-S3-ETH or
ESP32-P4-WIFI6-POE-ETH, both with LAN to the HAN socket and WLAN to the home network), a PPC gateway and a meter with consumption and feed-in.
Other gateways and meters without 2.8.0 are not tested.

**Firmware requirements:**
- TinyC 1.6.78, branch `universal` from 12.11.2026 (for `TinyCTlsGueltig`); `xdrv_124_tinyc_vm.h` changes (`TC_TLS_RX_BUF`, `tlsConnect` offering ECDSA) and,
  for login by client certificate, `TinyCTlsCert` (needs `USE_MQTT_CLIENT_CERT`).
- Finding the reader by name: `USE_DISCOVERY` + `WEBSERVER_ADVERTISE` in the device block and `SetOption55 1` on the device (`http://smgw.local/` with topic `SMGW`).
  If the reader replaces an older device with the same name, delete the old entry in the FRITZ!Box network list.

**Security:** no credentials, meter numbers or gateway identifiers are in these files. The credentials live only in `*.secret` files on the device; TinyC blocks them on
ports 82/83, but Tasmota's own file manager (port 80) can download them as long as no web password is set – set one (Configuration → Other).
The pictures in the guide use example tariffs; addresses, identifiers and serial numbers are hidden or invented.
`smgw.html` loads Chart.js from a CDN (cdnjs) and, only when a certificate file is opened, node-forge (with an integrity hash).
Exchange prices: Bundesnetzagentur | SMARD.de via energy-charts.info (Fraunhofer ISE), licence CC BY 4.0; the source is named on the cost page.
