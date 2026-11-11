# SMGW reader: smart meter gateway over the HAN interface with TinyC

By Andreas (forum name meierchen006), taken over into the repository on 08.11.2026 with his consent. Reads the official 15-minute meter
readings (consumption 1.8.0, feed-in 2.8.0) from a PPC smart meter gateway over its HAN interface, keeps them as text on the SD card, shows day / month / year
and offers CSV and an Excel query. Optional daily FTP backup. Optional electricity cost calculation from the same readings (slot 2 and `smgw_kosten.html`).

- `slot0-smgw_v020.tc`: the reader (TLS, digest login, evaluation in 1 KB pieces, files, clock-driven schedule, FTP backup, console `SMGW...`).
- `slot1-smgwweb_v009.tc`: button on the main page, `/smgw`, `/smgwj`, `/smgwf`, `/smgwzugang`. Own slot because slot 0 is busy during a fetch.
- `slot2-smgwkosten_v004.tc` (optional): fetches the day-ahead exchange prices DE-LU (15 min) from api.energy-charts.info once per month file
  (`/smgw_boerse_JJJJ-MM.txt`, current and previous year kept), the next day from 14:00; never while slot 0 talks to the gateway. Serves `/smgwkosten` and `/smgwb`.
- `smgw.html` (+ `.gz`): the reader page (belongs on the SD card); the switch "SMGW-Leser | Stromkosten" in the tab row appears only when slot 2 answers.
- `smgw_kosten.html` (+ `.gz`, optional, SD card): costs per quarter hour, day, month and billing year (instalments), grid fees / levies / taxes and supplier
  tariffs (fixed, time-of-use, dynamic) as sections "from date", feed-in tariff, §14a modules 1 and 3, comparison with up to six offers on the real consumption.
  Tariffs are stored in `/smgw_tarif.json` (written over port 83, read back and compared). A recalculation without warranty; only the supplier's bill counts.
  Optional `/smgw_ergaenzung.txt` (lines `JJJJ-MM-TT HH:MM<TAB>consumption kWh<TAB>feed-in kWh`, same zero point as the gateway readings) fills the days
  before the first gateway reading, e.g. from an own optical reading head; only lines before the first gateway reading are used, switchable, marked "not official".
- `ANLEITUNG-SMGW-Leser.pdf` (+ `quellen/` with the HTML source and pictures): hardware, network, build variant, commissioning, credentials file format, files, operation, FTP backup,
  and in section 11 the optional cost calculation (slot 2).

**Own use, own risk.** No ready-made firmware: it has to be built by yourself (see the guide). Tested only with the hardware named in the guide (Waveshare ESP32-S3-ETH or
ESP32-P4-WIFI6-POE-ETH, both with LAN to the HAN socket and WLAN to the home network) and a PPC gateway.
Needs the firmware changes in `xdrv_124_tinyc_vm.h` (`TC_TLS_RX_BUF`, `tlsConnect` offering ECDSA) and, for login by client certificate, `TinyCTlsCert` (needs `USE_MQTT_CLIENT_CERT`).
No credentials, meter numbers or gateway identifiers are in these files; the credentials live only in `/smgw_zugang.secret` on the device (blocked on ports 82/83).
`smgw.html` loads Chart.js from a CDN (cdnjs) and, only when a certificate file is opened, node-forge (with an integrity hash).
Exchange prices: Bundesnetzagentur | SMARD.de via energy-charts.info (Fraunhofer ISE), licence CC BY 4.0; the source is named on the cost page.
