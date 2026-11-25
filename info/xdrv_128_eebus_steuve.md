# SteuVE – steuerbare Verbrauchseinrichtung über EEBUS (`xdrv_128_eebus_steuve`)

Stand: 25.11.2026 · **Firmware v042** (Treiber `xdrv_128` mit derselben Nummer) · Skript `steuve` v022
(`// @version: 20261122_2000`) · Seite `steuve.html`.
Ausführliche Bedienungsanleitung mit Bildern: [`tasmota/tinyc/docs/eebus_anleitungen/ANLEITUNG-SteuVE.pdf`](../tasmota/tinyc/docs/eebus_anleitungen/ANLEITUNG-SteuVE.pdf).

> **Testgerät, nicht zertifiziert.** Die SteuVE dient dazu, die EEBUS-Steuerung nach § 14a EnWG an einem HEMS oder
> einer Steuerbox zu prüfen und auszuprobieren. Sie ist keine geprüfte steuerbare Verbrauchseinrichtung im Sinne von
> § 14a EnWG / BNetzA BK6-22-300, hat keine EEBUS-Zertifizierung und ist nicht für den regulären Betrieb beim
> Netzbetreiber gedacht. Arbeiten an 230 V und am Steuereingang von Wärmepumpe oder Wallbox nur durch eine
> Elektrofachkraft.

## Wozu

Mit der SteuVE wird ein Tasmota-Gerät für einen Energiemanager zu **einer steuerbaren Verbrauchseinrichtung
nach § 14a EnWG**, so wie eine Wärmepumpe oder Wallbox mit EEBUS. Gibt der Netzbetreiber über die Steuerbox eine
Bezugsgrenze vor, verteilt der Energiemanager (HEMS) sie auf seine Verbraucher. Den Anteil, der bei der SteuVE
ankommt, setzt ein TinyC-Skript um. Dahinter hängen bis zu vier Shelly- oder Tasmota-Geräte:

- **Verbraucher an Steckdose oder Schalter** – die SteuVE misst ihre Leistung und schaltet sie nach Priorität ab,
  wenn die Summe über der Grenze liegt.
- **Steuerkontakt (Shelly 1)** – bei einer Begrenzung legt die SteuVE einen **potentialfreien Kontakt** um, der an
  den Steuereingang eines Geräts geht (EVU-Sperr- oder SG-Ready-Eingang der Wärmepumpe, Freigabe- bzw.
  Begrenzungseingang der Wallbox). Die Stellung wird zurückgelesen und erst dann zugesagt (s. u.).

```
Netzbetreiber → Steuerbox → Energiemanager (HEMS) → SteuVE (dieser Treiber) → Skript → Shellys/Tasmota → Verbraucher
```

Ohne HEMS kann die Steuerbox die SteuVE auch direkt steuern. Nach BK6-22-300 Anlage 1 Ziff. 4.4 gilt für jede SteuVE
entweder das eine oder das andere: Sie vertraut **entweder dem HEMS oder der Steuerbox, nie beiden**.

Der Treiber kennt die Verbraucher nicht. Er nimmt die Grenze an, meldet sie nach außen (Ereignis, Sensordaten,
Befehl), meldet Nennleistung und aktuelle Leistung, die ihm das Skript liefert, an den Energy Guard zurück und sagt
eine Grenze nur zu, wenn das Skript bestätigt, dass sie eingehalten werden kann.

## Einbau

In `user_config_override.h`:

```c
#define USE_EEBUS_STEUVE
```

**Nicht zusammen mit `USE_EEBUS_GUARD` (Steuerbox, `xdrv_126`)** – beide Treiber haben dieselben Befehle und
Funktionsnamen. Sind beide gesetzt, bricht der Bau mit `#error` ab. Für eine Steuerbox und eine SteuVE braucht
man also zwei Geräte.

Erprobt auf einem ESP32-S3 mit Ethernet (Waveshare ESP32-S3-ETH, Vorlage „Waveshare ESP32-S3-ETH SDIO“) mit
SD-Karte. Fertige Firmware: Image `tinyc32s3-eth-steuve` im Release „testing“.

- Mit dem LAN-Chip W5500 die Firmware mit `CONFIG_FREERTOS_IDLE_TASK_STACKSIZE=3072` bauen (mit 1024 Byte
  Absturz unter Last, DebugException beim Taskwechsel).
- **HTTP API aktivieren** (SetOption128 1), sonst bleibt die Seite beim direkten Aufruf leer.
- `steuve.html` (bzw. `steuve.html.gz`) auf die SD-Karte, das Skript `steuve` in TinyC-Slot 0 mit Autostart.

## Wie der Treiber arbeitet

### Anmeldung beim Energy Guard

- Das Gerät startet in **Rolle 3** (steuerbare Verbrauchseinrichtung) und kündigt sich per mDNS (`_ship._tcp`) an.
- Use Cases: **LPC als ControllableSystem** (Bezugsgrenze empfangen) und **MPC als MonitoredUnit**
  (Leistung und Zählerstand melden).
- Einen EEBUS-Gerätetyp „SteuVE“ gibt es nicht. Die SteuVE weist sich mit einem vorhandenen Typ aus (`EEBusCsTyp`),
  erprobt ist **`wallbox`** – das ist nur das Kennzeichen.
- Die LoadControl-Grenze beschreibt sie mit `limitType` **`signDependentAbsValueLimit`**, wie eine Ladestation.
- Die Nennleistung (`powerConsumptionNominalMax`) kommt per `EEBusCsMax` vom Skript (Summe der eingetragenen
  Verbraucher) und wird im Flash gehalten (`/eebus_cs_max.txt`), damit auch ohne laufendes Skript der letzte
  bekannte Wert gemeldet wird statt 0 W.

### SHIP nach Norm (ab Firmware v038)

Fristen beim Verbindungsaufbau (CMI, Hello mit Verlängerung nur auf Anfrage, Protokoll-Handshake, PIN-Zustand),
Prüfung der SKI-Erweiterung im Zertifikat, Servername (SNI) = mDNS-Name, WebSocket-Regeln (RFC 6455, zerstückelte
Nachrichten werden zusammengesetzt), `txtvers=1` im mDNS-Eintrag, eigenes Verbindungsende mit Grund „unspecific“,
auf „announce“ der Gegenstelle Antwort „confirm“ (v040). Kein Pong 10 s nach Ping → Verbindung abbauen.

### Vertrauen

Genau **ein** Energy Guard ist vertraut (`EEBusCsTrust`, gespeichert in `/eebus_cs_trust.txt` im Flash), normal
das HEMS, ohne HEMS die Steuerbox.

- **Erkennung über das Zertifikat:** Der Energy Guard wird an der SKI aus seinem Zertifikat erkannt, nicht an der
  Adresse (IPv4 oder IPv6, jede Adresse).
- **Anrufer nur mit Zertifikat und nur, wenn vertraut** (ab Firmware v038). Jeden anderen Anrufer weist die SteuVE
  schon beim Verbindungsaufbau ab (SHIP-Hello „aborted“). **Ein neues Gerät braucht deshalb zuerst
  `EEBusCsTrust <SKI>` bzw. `EEBusCsTrust hm`, erst dann koppeln.**
- Writes von anderen werden abgewiesen und gezählt („Abgewiesen“). Ohne Vertrauen werden alle Writes abgewiesen.
- Notweg nur zum Testen: `EEBusCsStreng 0` lässt jeden Anrufer zu und vertraut auch ohne Zertifikat. Wird nicht
  gespeichert, nach einem Neustart gilt wieder die strenge Prüfung.

### Binding-Pflicht (ab Firmware v039)

Grenze (LoadControl) und Failsafe-Werte (DeviceConfiguration) nimmt die SteuVE nur vom **Binding-Partner** des
beschriebenen Features an (SPINE, LPC TS BIND_002), sonst Fehler 9 („Binding nötig“). Der Energy Guard muss also
LoadControl **und** DeviceConfiguration binden, bevor er schreibt. NodeManagement beantwortet die Abfragen der
Abos und Bindings (`nodeManagementSubscriptionData` / `BindingData`).

### Grenze übernehmen und zusagen

Schreibt der vertraute Energy Guard eine Grenze (`loadControlLimitListData`), übernimmt die SteuVE Wert,
aktiv/inaktiv und Geltungsdauer, meldet sie beim Lesen und per Notify zurück und hebt sie nach Ablauf der Dauer
selbst auf.

Ein **ACK ist eine Zusage, die Grenze einzuhalten**, keine bloße Empfangsbestätigung (LPC TS [LPC-002]/[LPC-003],
LPC IG 2.4). Deshalb meldet das Skript in jeder Runde mit `EEBusCsReady`, auf welche Leistung es gerade herunterkommt:

| Fall (Grenze aktiv) | Antwort |
|---|---|
| Grenze ≥ Zusage, Zusage jünger als 60 s | ACK |
| Grenze < Zusage oder Zusage unbekannt | NACK (errorNumber 7, „Grenze nicht erreichbar“), Zustand bleibt ([LPC-907]) |
| Skript meldet nicht (keine Zusage in den letzten 60 s oder noch nie) | NACK („Skript läuft nicht“) |
| Dauer 0 mit „aktiv“ | Freigabe mit ACK |

Läuft das Skript nicht (z. B. hat der TinyC-Boot-Schutz nach zwei Stromausfällen binnen 30 s den Autostart
abgeschaltet), nimmt die SteuVE also keine Grenze an. Ein Messwert (`EEBusCsPower`), der länger als 60 s nicht
erneuert wurde, wird als **unbekannt** gemeldet (valueState error) statt als Zahl.

`EEBusCsFail [Text]` deaktiviert eine angenommene Grenze und informiert den Energy Guard ([LPC-923]). Das Skript v022
benutzt es nicht mehr: Nach LPC ist das einer Einrichtung, die nicht selbst Energiemanager ist, nur aus Selbstschutz,
Sicherheit oder rechtlichen Gründen erlaubt.

In Anlauf, Failsafe und autonom wird eine Grenze nur angenommen, wenn binnen 60 s davor ein Herzschlag kam
(LPC TS Z. 372-375).

### Zustände, Herzschlag, Failsafe

```
Anlauf ──(Grenze oder Herzschlag)──> gesteuert ──(120 s kein Herzschlag)──> Failsafe ──(Mindestdauer um,
   │                                     ▲                                       weiter kein Herzschlag)──> autonom
   └──(120 s kein Energy Guard)──> autonom ──(Herzschlag)──────────────────────────────────────────────────┘
```

| Zustand | Nr. | Bedeutung | wirksame Grenze |
|---|---|---|---|
| Anlauf | 0 | nach dem Start, noch kein Energy Guard | keine |
| gesteuert | 1 | Energy Guard da, Herzschlag kommt | die geschriebene Grenze, falls aktiv |
| Failsafe | 2 | 120 s kein Herzschlag | Failsafe-Leistung |
| autonom | 3 | Failsafe-Mindestdauer abgelaufen, weiter kein Energy Guard | keine |

- Die Failsafe-Werte (Leistung, Mindestdauer) schreibt der Energy Guard als DeviceConfiguration. Sie werden im
  Flash gehalten (`/eebus_cs.txt`). Bis er sie schreibt, gilt die eigene Vorgabe **4200 W für 2 h**
  (§ 14a-Mindestleistung), änderbar mit `EEBusCsFs` (2 … 24 h). Eine Mindestdauer über 24 h wird abgelehnt
  und auf 24 h gesetzt. Änderungen gehen per Notify an die Abonnenten.
- Auch im Failsafe legt das Skript die Steuerkontakte in die Begrenzungsstellung.
- **Herzschlag-Abo:** Die SteuVE abonniert den Herzschlag ihres Energy Guard und prüft dessen Quittung. Lehnt er
  ab, liest sie den Herzschlag alle 30 s selbst. Nach geänderter Discovery und nach Herzschlagverlust abonniert sie
  neu. Eigener Herzschlag geht nur an Abonnenten, `heartbeatTimeout` PT60S.
- **Wiederverbinden (ab Firmware v039):** Beendet der Energy Guard eine Verbindung, die er aufgebaut hatte (z. B.
  HEMS-Neustart), ruft die SteuVE nach 10 s selbst beim gemerkten Partner an.
- **Ein Platz je Gegenstelle (ab Firmware v042):** Bleibt nach einem gescheiterten eigenen Verbindungsversuch ein
  Platz derselben Gegenstelle mit Adresse und SKI zurück, wird er jede Sekunde geleert, sobald die Verbindung auf
  einem anderen Platz steht. Der Platz für den eigenen Wiederaufbau bleibt erhalten, ruft aber nicht an, solange die
  Verbindung steht. Bis v041 konnten nach einem HEMS-Neustart zwei Einträge entstehen, und die Seite zeigte „nicht
  verbunden“ trotz Herzschlag.

### Nach außen

- **Ereignis** bei jeder Änderung von Zustand oder wirksamer Grenze: `{"EEBusCs":{...}}` (Regeln, MQTT).
- **Sensordaten** (Telemetrie, `Status 10`):
  `"EEBusCs":{"Begrenzt":0|1,"Grenze":W,"Zustand":0..3,"RestS":s,"Leistung":W,"Bekannt":0|1,"Max":W}`
  (`RestS` −1 = unbefristet). Ein TinyC-Skript liest sie mit `sensorGet("EEBusCs#Begrenzt")` usw.
  (`tasmCmd("EEBusCs")` liefert dem Skript eine leere Antwort.)
- **Messwerte an den Energy Guard** (`EEBusCsPower`) gehen gebremst hinaus: bei Änderung ab 10 W bzw. 2 %,
  höchstens alle 10 s, sonst einmal pro Minute. Die Herkunft (`valueSource`) stellt `EEBusCsQuelle` ein.

## Befehle des Treibers

| Befehl | Wirkung |
|---|---|
| `EEBusCs` | Zustand als JSON: Zustand, Begrenzt, Grenze, RestS, Energy Guard, Failsafe, Max, Leistung, Zähler, Herzschlag, Vertrauen, Skript, eigene SKI und Kennung |
| `EEBusCsPower <W> [Wh]` | aktuelle Leistung (und Zählerstand) der Verbraucher melden (vom Skript) |
| `EEBusCsReady <W>\|unbekannt` | Zusage: auf welche Leistung das Skript gerade herunterkommt (vom Skript, Grundlage für ACK/NACK) |
| `EEBusCsFail [Text]` | angenommene Grenze kann nicht eingehalten werden → Grenze deaktivieren, Energy Guard informieren |
| `EEBusCsQuelle m\|c\|e` | valueSource der gemeldeten Leistung: gemessen / berechnet / geschätzt |
| `EEBusCsMax <W>` | Nennleistung der Einrichtung (Summe der Verbraucher), im Flash |
| `EEBusCsFs <W> [s]` | eigene Failsafe-Vorgabe (2 … 24 h), bis der Energy Guard schreibt |
| `EEBusCsSim <W> [s]` / `EEBusCsSim 0` | Grenze lokal simulieren – **nur zum Testen des Skripts** |
| `EEBusCsTrust [<ski>\|hm\|0]` | Vertrauen: volle SKI oder eindeutiger Anfang aus der Fundliste, `hm` = das HEMS aus der Fundliste, `0` = entziehen (trennt sofort, danach autonom ohne Failsafe). Ohne Argument: Stand |
| `EEBusCsStreng [1\|0]` | 1 (Vorgabe) = nur vertrauter Energy Guard mit Zertifikat; 0 = Notweg zum Testen, nicht gespeichert |
| `EEBusCsTyp [wallbox\|wp\|steuerbox]` | Kennzeichen gegenüber dem Energy Guard (erprobt: `wallbox`), kündigt sofort neu an |
| `EEBusCsMsg` | letzte 60 Meldungen vom/an den Energy Guard (ohne Herzschläge), seitenweise |
| `EEBusCsHbTest [1\|0]` | **nur Testnetz:** Herzschlag des Energy Guard nicht abonnieren (verstößt gegen LPC), nach Neustart aus |

Dazu die allgemeinen EEBUS-Befehle (`EEBusScan`, `EEBusPeers`, `EEBusConnect`, `EEBusDisconnect`, `EEBusStatus`,
`EEBusCert` …). Die Steuerbox-Befehle (`EEBusLpc`, `EEBusLpp` …) sind im Treiber noch enthalten, werden in Rolle 3
aber nicht gebraucht.

## Bedienseite `/steuve`

Erreichbar über den Knopf „SteuVE“ auf der Tasmota-Hauptseite. Das Gerät liefert zuerst `steuve.html.gz` aus.

| Reiter | Inhalt |
|---|---|
| Übersicht | Grenze mit Restzeit, Verbraucher gesamt, Nennleistung, Kette Steuerbox → (HEMS) → SteuVE → Verbraucher mit Anteil, Verbraucherliste (Art M/S/K, Leistung bzw. Kontaktstellung, Zustand, Erreichbarkeit) |
| EEBus | HEMS / Energy Guard: vertrauter Partner, Steuerweg (über HEMS oder Direktsteuerung), Fundliste nach SKI · Eigene Kennung: SHIP-ID/SKI (verdeckt), QR-Codes, Sichern, Neue Identität, Neu ankündigen, Zurückspielen |
| Verbraucher | Plätze 1–4 (Priorität) + Melder: IP, Name, Nennleistung, abschaltbar, Steuerkontakt NC/NO (nur Shelly 1), erkanntes Gerät, Fake-Test |
| Meldungen | Meldungen vom/an den Energy Guard mit Daten, Herzschlag-Zähler, letzte Grenze, letzte Failsafe-Werte |
| Protokoll | Verlauf der Begrenzungen (`/steuve_log.csv`) und Kontakt-Protokoll (`/steuve_kontakt.csv`) |
| Test & Einstellungen | Grenze simulieren (nur Inbetriebnahme), Kenndaten, Erklärung der Zustände |
| Status | Gerätedaten, Kopplungen, Zustand der Skript-Slots (nur lesend) |

## Das Skript `steuve`

- **Plätze 1–4** = Verbraucher, Platz = Priorität. Prio 4 wird zuerst abgeschaltet, Prio 1 zuletzt.
  **Platz 5** = Melder (Shelly Plus Plug S oder Plug PM): LED-Ring grün = frei, gelb = begrenzt, rot = Failsafe,
  blau = ohne Energy Guard. Der Melder schaltet nichts ab.
- **Shelly** Gen1 und Gen2+ werden über `/shelly` erkannt: Schalter mit Messung (S), reine Messgeräte wie PM Mini,
  EM, Pro EM, Pro 3EM, Shelly EM/3EM (M, werden nie geschaltet) und die Shelly-1-Familie (K, Steuerkontakt).
- **Tasmota-Geräte** mit Energiemessung (ab Skript v008): Antwortet ein Gerät nicht wie ein Shelly, fragt das Skript
  es mit `Status` und `Status 10` ab. Mit Relais = Schalter (bei mehreren Relais alle zusammen, `Power0`), ohne
  Relais = Messgerät. Voraussetzung am Verbraucher: HTTP API an, kein Web-Passwort. Tasmota-Geräte können kein
  Steuerkontakt sein.
- IP leer oder 1. Oktett 0 = Platz frei. **Nennleistung leer = 3680 W** (16 A × 230 V).
- Alle **10 s**:
  1. Leistung aller belegten Plätze lesen (beim Steuerkontakt die Relaisstellung), Summe per `EEBusCsPower`.
     Antwortet ein Verbraucher 3 Runden in Folge nicht, wird „unbekannt“ gemeldet statt einer erfundenen Zahl.
  2. Zustand per `sensorGet` lesen.
  3. **Grenze aktiv:** zuerst alle Steuerkontakte in die Begrenzungsstellung legen und zurücklesen.
  4. **Summe > Grenze:** abschaltbare Verbraucher, die gerade Leistung ziehen, abschalten, höchste Prio-Zahl
     zuerst. Kann ein Gerät nicht stufenlos reduziert werden, gilt der nächste technisch mögliche Wert, also „aus“
     (BK6-22-300 Anlage 1 Ziff. 4.6).
  5. **Nach der Begrenzung:** Steuerkontakte sofort zurück. Abgeschaltete Verbraucher gestaffelt wieder ein, einer
     je 60 s, Prio 1 zuerst (Ziff. 4.3), nur was das Skript selbst abgeschaltet hat. Während einer laufenden
     Begrenzung nur, wenn das Gerät mindestens 5 min aus war und **Summe + seine Nennleistung ≤ Grenze − 200 W**.
  6. Zusage per `EEBusCsReady`: abschaltbare Plätze, die antworten, 0 W · nicht abschaltbare ihre aktuelle Leistung ·
     Plätze, die nicht antworten, ihre Nennleistung · bestätigte Steuerkontakte 0 W · Steuerkontakt nicht
     erreichbar oder nicht bestätigt → unbekannt.
  7. Protokoll `/steuve_log.csv`: minütlich während einer Begrenzung und 10 min danach.
- **Fake-Plätze** simulieren Verbraucher mit Testprofilen (Waschmaschine, Trockner, Geschirrspüler, Heizstab),
  ohne etwas im Netz zu schalten. `STVMODE` legt fest, was ein simuliertes Gerät nach dem Abschalten tut
  (0 = hält an und läuft weiter, 1 = startet von vorn, 2 = bleibt aus).

### Steuerkontakt mit Shelly 1

- Geeignet: **Shelly-1-Familie** (potentialfrei, ohne Messung) – Shelly 1 (Gen1), Plus 1, Plus 1 Mini, 1 Gen3,
  1 Gen4, 1 Mini Gen3, 1 Mini Gen4. Am Gerät erprobt ist der Shelly 1 Mini Gen3 (auch vier Kontakte gleichzeitig
  und Stromausfall des Shelly mitten in der Begrenzung).
- Der Shelly 1 schaltet **nur den Steuereingang, nie die Last**. Wie weit das Gerät herunterregelt, legt das Gerät
  selbst fest; das misst der Shelly 1 nicht.
- **NC** = Steuereingang normal geschlossen (begrenzt bei offenem Kontakt), **NO** = normal offen (begrenzt bei
  geschlossenem Kontakt). Ein Platz ist nur Steuerkontakt, wenn NC/NO eingetragen, „abschaltbar“ gesetzt und das
  Gerät ein Shelly 1 ist.
- Ablauf: Grenze aktiv → Kontakt in Begrenzungsstellung → **Relaisstellung zurücklesen** (Gen1 `/relay/0`, Gen2+
  `Switch.GetStatus`), nicht der Schaltantwort trauen → erst bei bestätigter Stellung zählt der Kontakt mit 0 W in
  die Zusage → ACK. Antwortet der Shelly nicht oder steht der Kontakt falsch: Zusage unbekannt → NACK für jede neue
  aktive Grenze. Während einer angenommenen Grenze bleibt diese aktiv; der Energy Guard sieht die Lage am
  gemeldeten Verbrauch („unbekannt“).
- Ohne Begrenzung stellt die SteuVE jeden Steuerkontakt in die Normalstellung, auch nach einem Stromausfall des
  Shelly oder wenn jemand von Hand geschaltet hat.
- Am Shelly einstellen: Einschaltzustand „letzten Modus wiederherstellen“, Eingang „getrennt“ (detached), keine
  Zeitschaltungen/Szenen, feste IP.
- **Kontakt-Protokoll `/steuve_kontakt.csv`** (nur anhängen, nie überschreiben): Zeit; Ereignis (Begrenzung
  Beginn/Ende, umgelegt, zurück, bestätigt, NICHT bestätigt, nicht eingehalten - Grenze bleibt aktiv,
  Normalstellung wiederhergestellt); Prio; Name; NC/NO; OPEN/CLOSE; Grenze W; Zustand – als Nachweis
  (BK6-22-300 Anlage 1 Ziff. 7.2–7.4, LPC IG 4.1.5).

### Skript-Befehle (Präfix `STV`)

| Befehl | Wirkung |
|---|---|
| `STVSTAT` | Kurzstand (Zustand, Begrenzt, Grenze, Summe, Bekannt) |
| `STVLIST` | Messwerte je Platz (Leistung, an/aus, von SteuVE abgeschaltet, Fehler, Kontakt-Bestätigung), Melder, Modus, Zusage |
| `STVCFG` | eingetragene Liste je Platz |
| `STVSET p,fake,abs,ip,nenn,profil,name` | Platz p (1 … 5) setzen und speichern; neue Adresse setzt den Steuerkontakt zurück |
| `STVKONT` · `STVKONT p,NC\|NO\|-` | Steuerkontakte anzeigen `[Platz,NC/NO/-,W,"-",Bestätigung,Relais,aktiv]` · Platz p als Steuerkontakt NC/NO · keiner |
| `STVPROBE p` | Gerät auf Platz p neu erkennen |
| `STVMODE 0\|1\|2` | Verhalten simulierter Geräte nach dem Abschalten |
| `STVSIM <W> [s]` · `STVSIM 0` | Grenze simulieren (ruft `EEBusCsSim`) · aufheben |

⚠️ **Update von Skript v011 oder älter:** Mit dem Steuerkontakt (ab v012) sind neue gespeicherte Werte
dazugekommen; TinyC verwirft dabei die alte `.pvs`, die Verbraucherliste ist danach leer. Vorher `STVCFG` und
`STVKONT` notieren.

## Einrichtung Schritt für Schritt

1. Firmware mit `USE_EEBUS_STEUVE` flashen (oder Image `tinyc32s3-eth-steuve` aus dem Release), Vorlage setzen,
   HTTP API an, `steuve.html.gz` auf die SD-Karte.
2. Skript `steuve` in Slot 0 laden, Autostart an. Kennung setzen: `EEBusCsTyp wallbox`.
3. Auf `/steuve` im Reiter **Verbraucher** die Geräte eintragen: IP, Name, Nennleistung, abschaltbar, bei einem
   Shelly 1 Steuerkontakt NC oder NO.
4. **Ohne Energy Guard testen:** im Reiter **Test & Einstellungen** eine Grenze simulieren (`EEBusCsSim 2000 300`)
   und prüfen, dass die richtigen Verbraucher in der richtigen Reihenfolge ab- und wieder einschalten und die
   Steuerkontakte umgelegt und bestätigt werden.
5. **Vertrauen zuerst:** `EEBusScan`, dann `EEBusCsTrust hm` (oder die SKI, im Reiter EEBus „Vertrauen“). Ohne
   Vertrauen lässt die SteuVE den Energy Guard gar nicht herein.
6. **Kopplung:** im HEMS die SteuVE aufnehmen bzw. ihre SKI bestätigen.
7. Prüfen: `EEBusCs` zeigt „gesteuert“, im Reiter **Meldungen** kommen Herzschläge, Bindings und die
   Failsafe-Werte an.

⚠️ **Identität:** Die SKI hängt am Zertifikat im Flash. Ein Flashen mit Löschen erzeugt eine neue SKI, und der
Energy Guard kennt das Gerät dann nicht mehr. Vorher im Reiter EEBus „Sichern“, danach „Zurückspielen“ – oder nur
ohne Löschen aktualisieren (Web-Upgrade). Nie zwei Geräte mit derselben Identität gleichzeitig betreiben.

## Was man über Energiemanager wissen sollte

Gemessen an einem handelsüblichen Energiemanager:

- Er **teilt die Grenze der Steuerbox starr durch die Zahl der aktiven EEBUS-Geräte**, nicht nach Bedarf oder
  Nennleistung. Mit einer SteuVE neben Wärmepumpe und Wallbox bekommt jedes Gerät ein Drittel. Auch die
  Failsafe-Werte werden so geteilt.
- Er zählt **jedes aktive EEBUS-Gerät** mit, egal welche Use Cases es anmeldet.
- Ohne `signDependentAbsValueLimit` in der Grenzbeschreibung schreibt er keine Grenze, nur Failsafe-Werte.

Eine SteuVE lohnt sich also vor allem dort, wo es **sonst keine** steuerbaren Verbraucher am Energiemanager gibt,
wo sie mehrere kleine Verbraucher bündelt oder wo ein Gerät nur einen Steuereingang statt EEBUS hat.

Rechtlich: Nach BK6-22-300 Anlage 1 Ziff. 4.4 b und 4.5.2 Satz 6 bekommt das Energiemanagementsystem einen
Gesamtwert, den der Betreiber „nach eigener Maßgabe“ einsetzen darf. Er bleibt dafür verantwortlich, dass der
Gesamtwert eingehalten wird.

## Grenzen und Offenes

- Die Rückmeldung des Steuerkontakts bestätigt die Kontaktstellung, nicht die Leistung des Geräts dahinter.
- Verbraucher werden nur geschaltet, nicht stufenlos gedimmt.
- Doppelverbindungen nach SHIP (beide Seiten rufen gleichzeitig an) werden bewusst nicht nach der SHIP-Regel
  aufgelöst; doppelte Verbindungen räumt das HEMS ab.
- Was am Gerät geprüft ist und was nur laut Programmcode beschrieben ist, steht in der Anleitung, Abschnitte 14
  und 15.

---

Shelly und Waveshare sind Marken ihrer jeweiligen Inhaber. Sie werden nur genannt, um anzugeben, mit welchen Geräten die SteuVE erprobt ist; eine Verbindung zu den Herstellern besteht nicht.
