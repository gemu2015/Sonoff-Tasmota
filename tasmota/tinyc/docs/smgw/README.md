# SMGW reader: smart meter gateway over the HAN interface with TinyC

By Andreas (forum name meierchen006), taken over into the repository on 08.11.2026 with his consent. Reads the official 15-minute meter
readings (consumption 1.8.0, feed-in 2.8.0) from a PPC smart meter gateway over its HAN interface, keeps them as text on the SD card, shows day / month / year
and offers CSV and an Excel query. Optional daily FTP backup.

- `slot0-smgw_v014.tc`: the reader (TLS, digest login, evaluation in 1 KB pieces, files, clock-driven schedule, FTP backup, console `SMGW...`).
- `slot1-smgwweb_v008.tc`: button on the main page, `/smgw`, `/smgwj`, `/smgwf`, `/smgwzugang`. Own slot because slot 0 is busy during a fetch.
- `smgw.html` (+ `.gz`): the page (belongs on the SD card).
- `ANLEITUNG-SMGW-Leser.pdf` (+ `quellen/` with the HTML source and pictures): hardware, network, build variant, commissioning, credentials file format, files, operation, FTP backup.

**Own use, own risk.** No ready-made firmware: it has to be built by yourself (see the guide). Tested only with the hardware named in the guide and a PPC gateway.
Needs the firmware changes in `xdrv_124_tinyc_vm.h` (`TC_TLS_RX_BUF`, `tlsConnect` offering ECDSA) and, for login by client certificate, `TinyCTlsCert` (needs `USE_MQTT_CLIENT_CERT`).
No credentials, meter numbers or gateway identifiers are in these files; the credentials live only in `/smgw_zugang.secret` on the device (blocked on ports 82/83).
`smgw.html` loads Chart.js from a CDN (cdnjs) and, only when a certificate file is opened, node-forge (with an integrity hash).
