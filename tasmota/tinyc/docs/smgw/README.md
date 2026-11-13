# SMGW reader: smart meter gateway over the HAN interface with TinyC

By Andreas (forum name meierchen006), taken over into the repository on 08.11.2026 with his consent. Reads the official 15-minute meter
readings (consumption 1.8.0, feed-in 2.8.0) from a PPC smart meter gateway over its HAN interface, keeps them as text on the SD card, shows day / month / year
and offers CSV and an Excel query. Optional daily FTP backup. Optional electricity cost calculation from the same readings (slot 2 and `smgw_kosten.html`).

- `slot0-smgw_v022.tc`: the reader (TLS, digest login, evaluation in 1 KB pieces, files, clock-driven schedule, FTP backup, console `SMGW...`).
  Since v021 it also stores the validity of the gateway's TLS certificate once a day in `/smgw_gateway.json` (needs the firmware command `TinyCTlsGueltig`, see below).
- `slot1-smgwweb_v009.tc`: button on the main page, `/smgw`, `/smgwj`, `/smgwf`, `/smgwzugang`. Own slot because slot 0 is busy during a fetch.
- `slot2-smgwkosten_v007.tc` (optional; v007 needs TinyC 1.6.78 or later, it searches the exchange answer with string literals instead of a global helper buffer): fetches the day-ahead exchange prices DE-LU (15 min) from api.energy-charts.info once per month file
  (`/smgw_boerse_JJJJ-MM.txt`, seven calendar years kept), the next day from 14:00; never while slot 0 talks to the gateway. Serves `/smgwkosten` and `/smgwb`.
- `smgw.html` (+ `.gz`): the reader page. Only `smgw.html.gz` belongs on the SD card (the device always serves the `.gz` and falls back to the
  plain `.html` only when no `.gz` exists); `smgw.html` is the readable source for viewing or own changes (repack with `gzip -9 -k -n`). the switch "SMGW-Leser | Stromkosten" in the tab row appears only when slot 2 answers.
  Tab "Status" shows the gateway firmware and the validity of its TLS certificate (= date by which the gateway has to be replaced; TRuDI shows the same).
  Heading "Smart Meter Gateway Leser" (the page belongs to the reader, not to the gateway), browser tab "SMGW-Leser".
  Charts: the tooltip sits in a strip above the plot (never covers bars) and only where values exist; in the day chart it shows the whole
  quarter hour ("16:45 – 17:00 Uhr", the bar "16:45" covers 16:45 to 17:00), likewise the quarter-hour table and the peak tiles; best-of lists with up to 12 entries.
  Button "Als PDF drucken" in day, month, year and all years: the view in light colours, scaled to one A4 page, with a header line.
- `smgw_kosten.html` (+ `.gz`, optional extension, only the `.gz` on the SD card): costs per quarter hour, day, month and billing year, with a box "§14a EnWG – calculated benefit"
  (module 1 flat rate from its start date, module 3 = cost with module 1 minus cost with modules 1 + 3). Tariffs as one **tariff sheet per year**
  (supplier, grid fees / levies / taxes, §14a with "module 1 / module 3 valid from", time windows in quarter hours, end exclusive), feed-in tariff in its own tab.
  Balance strip per day / month / billing year: basic charge (standing charge + metering) + electricity (energy price) - §14a flat rate - feed-in = balance,
  gross incl. VAT, each part rounded to the cent; the total is called "Kosten gesamt" everywhere.
  Tariff comparison: one consumption ("Verbrauch aus": a year, last 12 months, free; "fehlende Tage schätzen" extrapolates to 31.12.) for current year,
  **forecast** for the next year (as soon as a tariff sheet exists) and up to six offers - each row with the prices of its year. Settled previous years
  (one checkbox per year with a tariff sheet) always show their own measured consumption with their own prices, never estimated. Tariffs are stored in `/smgw_tarif.json` (written over port 83,
  read back and compared). The page shows the current year first and loads older months in the background; finished months are cached in the browser (IndexedDB).
  A recalculation without warranty; only the supplier's bill counts.
  Optional `/smgw_ergaenzung.txt` (lines `JJJJ-MM-TT HH:MM<TAB>consumption kWh<TAB>feed-in kWh`) fills the days before the first gateway reading; own month files
  `/smgw_JJJJ-MM.txt` for years before the gateway (e.g. from an optical reading head on the same meter) are read like the gateway files.
- `ANLEITUNG-SMGW-Leser.pdf` (v016, + `quellen/` with the HTML source and the changed pictures): where to get the files and where each one goes (section 5.1), hardware, network, finding the reader again (hotspot, `http://smgw.local/` via mDNS, router, USB; the LAN socket belongs to the gateway), build variant, commissioning, credentials file format, files, operation, FTP backup,
  certificate, and in section 11 the optional cost calculation (slot 2) including "so wird gerechnet" (all formulas).

**Own use, own risk.** No ready-made firmware: it has to be built by yourself (see the guide). Tested only with the hardware named in the guide (Waveshare ESP32-S3-ETH or
ESP32-P4-WIFI6-POE-ETH, both with LAN to the HAN socket and WLAN to the home network) and a PPC gateway.

**Firmware requirements:**
- `xdrv_124_tinyc_vm.h` changes (`TC_TLS_RX_BUF`, `tlsConnect` offering ECDSA) and, for login by client certificate, `TinyCTlsCert` (needs `USE_MQTT_CLIENT_CERT`).
- Slots 1 and 2 register their own web handlers with `webOn(1, ...)` / `webOn(2, ...)` in a slot other than 0; this needs firmware after 03.11.2026.
- Finding the reader by name: `USE_DISCOVERY` + `WEBSERVER_ADVERTISE` in the device block and `SetOption55 1` on the device (`http://smgw.local/` with topic `SMGW`).
  Tested on Windows, macOS and Android. If the reader replaces an older device with the same name, delete the old entry in the FRITZ!Box network list;
  otherwise Android does not find `smgw.local` and macOS needs about a minute.
- Certificate validity: `TinyCTlsGueltig` (patch 0014: tls_mini keeps validity, subject CN and serial of the peer's first certificate on the unverified path;
  the command prints them, the CN masked to its last 4 characters). Without it slot 0 only writes a log line. The command always reports the last TLS connection of the device
  (with slot 2 running often energy-charts); slot 0 reads it right after its own gateway connection.

No credentials, meter numbers or gateway identifiers are in these files; the credentials live only in `/smgw_zugang.secret` on the device (blocked on ports 82/83).
The pictures in the guide use example tariffs; addresses, identifiers and serial numbers are hidden or invented.
`smgw.html` loads Chart.js from a CDN (cdnjs) and, only when a certificate file is opened, node-forge (with an integrity hash).
Exchange prices: Bundesnetzagentur | SMARD.de via energy-charts.info (Fraunhofer ISE), licence CC BY 4.0; the source is named on the cost page.
