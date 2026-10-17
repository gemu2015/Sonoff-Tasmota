# SteuVE – steuerbare Verbrauchseinrichtung über EEBUS (`xdrv_128_eebus_steuve`)

Stand: SteuVE v014 (Treiber), Skript `slot0-steuve_v004` (TinyC), Seite `steuve.html`.

## Wozu

Mit der SteuVE wird ein Tasmota-Gerät für einen Energiemanager zu **einer steuerbaren Verbrauchseinrichtung
nach § 14a EnWG**, so wie eine Wärmepumpe oder Wallbox mit EEBUS. Gibt der Netzbetreiber über die Steuerbox eine
Bezugsgrenze vor, verteilt der Energiemanager sie auf seine Verbraucher. Den Anteil, der bei der SteuVE ankommt,
setzt ein TinyC-Skript um: Es schaltet Verbraucher ohne eigenes EEBUS (über Shelly-Schaltsteckdosen oder
-Relais) nach Priorität ab und später gestaffelt wieder ein.

```
Netzbetreiber → Steuerbox → Energiemanager (HEMS) → SteuVE (dieser Treiber) → Skript → Shellys → Verbraucher
```

Der Treiber kennt die Verbraucher nicht. Er nimmt die Grenze an, meldet sie nach außen (Ereignis,
Sensordaten, Befehl) und meldet Nennleistung und aktuelle Leistung, die ihm das Skript liefert, an den
Energiemanager zurück.

## Einbau

In `user_config_override.h`:

```c
#define USE_EEBUS_STEUVE
```

**Nicht zusammen mit `USE_EEBUS_GUARD` (Steuerbox, `xdrv_126`)** – beide Treiber haben dieselben Befehle und
Funktionsnamen. Sind beide gesetzt, bricht der Bau mit `#error` ab. Für eine Steuerbox und eine SteuVE braucht
man also zwei Geräte.

Erprobt auf einem ESP32-S3 mit Ethernet (Waveshare ESP32-S3-ETH) mit SD-Karte. Die Seite `steuve.html` gehört in
das Dateisystem (SD oder Flash), das Skript in einen TinyC-Slot.

## Wie der Treiber arbeitet

### Anmeldung beim Energiemanager

- Das Gerät startet in **Rolle 3** (steuerbare Verbrauchseinrichtung) und kündigt sich per mDNS (`_ship._tcp`) an.
- Es meldet die Use Cases **LPC als ControllableSystem** (Bezugsgrenze empfangen) und **MPC als MonitoredUnit**
  (Leistung und Zählerstand melden).
- Einen EEBUS-Gerätetyp „SteuVE“ gibt es nicht. Die SteuVE muss sich deshalb mit einem vorhandenen Typ ausweisen
  (`EEBusCsTyp`). Angenommen hat der erprobte Energiemanager sie mit **`EEBusCsTyp wallbox`** – das ist nur das
  Kennzeichen, die SteuVE bleibt eine SteuVE mit ihren Verbrauchern.
- Die LoadControl-Grenze beschreibt die SteuVE mit `limitType` **`signDependentAbsValueLimit`**, wie eine
  Ladestation. Mit `maxValueLimit` hat der erprobte Energiemanager nie eine Grenze geschrieben (nur die
  Failsafe-Werte).
- Die Nennleistung (`powerConsumptionNominalMax`) kommt per `EEBusCsMax` vom Skript: die Summe der eingetragenen
  Verbraucher.

### Vertrauen

Genau **ein** Energy Guard darf Grenzen und Failsafe-Werte schreiben (`EEBusCsTrust`, gespeichert in
`/eebus_cs_trust.txt` im Flash). Normal ist das der Energiemanager. Zum Test ohne Energiemanager kann es auch
direkt eine Steuerbox sein. Writes von anderen werden mit `errorNumber 7` abgewiesen und gezählt. **Ohne
Vertrauen werden alle Writes abgewiesen.**

⚠️ Die SKI der Gegenstelle stammt aus dem mDNS-Scan und wird über die IP zugeordnet. Gegen das Zertifikat geprüft
wird sie nicht (die schlanke TLS-Bibliothek gibt das Zertifikat nicht heraus). Das schützt gegen Versehen, nicht
gegen einen Angreifer im eigenen Netz.

### Grenze übernehmen

Schreibt der vertraute Energy Guard eine Grenze (`loadControlLimitListData`), übernimmt die SteuVE Wert,
aktiv/inaktiv und Geltungsdauer, meldet sie beim Lesen und per Notify zurück und hebt sie nach Ablauf der Dauer
selbst auf. Eine Grenze wird in jedem Zustand angenommen, auch vor dem ersten Herzschlag.

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
  Flash gehalten (`/eebus_cs.txt`). Bis er sie schreibt, gilt die eigene Vorgabe: **4200 W für 2 h** (§ 14a
  Mindestleistung), änderbar mit `EEBusCsFs`.
- Den Herzschlag des Energy Guard abonniert die SteuVE, sobald er ihr LoadControl bindet oder abonniert.
- Verbindungsabbrüche baut der Treiber selbst wieder auf (dieselbe Logik wie die Steuerbox `xdrv_126`).

### Nach außen

- **Ereignis** bei jeder Änderung von Zustand oder wirksamer Grenze: `{"EEBusCs":{...}}` (Regeln, MQTT).
- **Sensordaten** (Telemetrie, `Status 10`): `"EEBusCs":{"Begrenzt":0|1,"Grenze":W,"Zustand":0..3,"RestS":s,"Leistung":W,"Max":W}`.
  Ein TinyC-Skript liest sie mit `sensorGet("EEBusCs#Begrenzt")` usw.
- **Messwerte an den Energy Guard** (`EEBusCsPower`) gehen gebremst hinaus: bei Änderung ab 10 W bzw. 2 %,
  höchstens alle 10 s, sonst einmal pro Minute.

## Befehle

| Befehl | Wirkung |
|---|---|
| `EEBusCs` | Zustand als JSON: Zustand, Begrenzt, Grenze, RestS, Energy Guard, Failsafe, Max, Leistung, Zähler, Herzschlag, Vertrauen, eigene SKI und Kennung |
| `EEBusCsPower <W> [Wh]` | aktuelle Leistung (und Zählerstand) der Verbraucher melden |
| `EEBusCsMax <W>` | Nennleistung der Einrichtung (Summe der Verbraucher) |
| `EEBusCsFs <W> [s]` | eigene Failsafe-Vorgabe (Flash), bis der Energy Guard schreibt |
| `EEBusCsSim <W> [s]` / `EEBusCsSim 0` | Grenze lokal simulieren – **nur zum Testen des Skripts** |
| `EEBusCsTrust [<ski>\|hm\|0]` | Vertrauen: volle SKI oder eindeutiger Anfang aus der Fundliste, `hm` = das HEMS (der Energiemanager) aus der Fundliste, `0` = entziehen (trennt sofort). Ohne Argument: Stand |
| `EEBusCsTyp [wallbox\|wp\|steuerbox]` | Kennzeichen gegenüber dem Energiemanager (s. o., erprobt: `wallbox`), kündigt sofort neu an |
| `EEBusCsMsg` | letzte 60 Meldungen vom/an den Energy Guard (ohne Herzschläge), seitenweise |

Dazu die allgemeinen EEBUS-Befehle aus der Steuerbox (`EEBusScan`, `EEBusPeers`, `EEBusConnect`,
`EEBusDisconnect`, `EEBusStatus`, `EEBusCert` …).

## Bedienseite `/steuve`

Erreichbar über einen Knopf auf der Tasmota-Hauptseite. Reiter:

| Reiter | Inhalt |
|---|---|
| SteuVE | Gerät: eigene SKI (verdeckbar, kopierbar), Kennung, Vertrauen setzen/entziehen, Fundliste nach SKI |
| Übersicht | Grenze für die SteuVE mit Restzeit, Verbraucher gesamt, Nennleistung, Kette Netzbetreiber → Steuerbox → Energiemanager → SteuVE → Verbraucher, Verbraucherliste in Abschaltreihenfolge mit Zustand |
| Shellys | Plätze 1–4 (Priorität) + Platz 5 (Melder): IP, Name, Nennleistung, Fake, abschaltbar |
| Meldungen | Meldungen vom/an den Energy Guard mit Daten, Herzschlag-Zähler, letzte Grenze, letzte Failsafe-Werte |
| Protokoll | Verlauf der Begrenzungen (`/steuve_log.csv`) |
| Test & Einstellungen | Grenze simulieren (nur Inbetriebnahme), Kenndaten (Nennleistung, Failsafe-Grenze und -Mindestdauer, Zählerstand, Grenze zuletzt von), Erklärung der Zustände |

## Das Skript `slot0-steuve_v004`

- **Plätze 1–4** = Verbraucher, Platz = Priorität. Prio 4 wird zuerst abgeschaltet, Prio 1 zuletzt.
  **Platz 5** = Melder (Shelly Plus Plug S oder Plug PM): LED-Ring grün = frei, gelb = begrenzt, rot = Failsafe,
  blau = ohne Energy Guard. Der Melder schaltet nichts ab.
- Shelly Gen1 und Gen2+ werden erkannt (`/shelly`). IP leer oder 1. Oktett 0 = Platz frei.
  **Nennleistung leer = 3680 W** (16 A × 230 V).
- Alle **10 s**:
  1. Leistung aller belegten Plätze lesen, Summe per `EEBusCsPower` an den Treiber.
  2. Zustand per `sensorGet` lesen.
  3. **Begrenzt und Summe > Grenze:** abschaltbare Verbraucher, die gerade Leistung ziehen, abschalten,
     höchste Prio-Zahl zuerst, bis die Summe unter der Grenze liegt. Kann ein Gerät nicht genau reduziert werden,
     gilt der nächstniedrigere technisch mögliche Wert, also „aus“ (BNetzA BK6-22-300, Anlage 1, Ziff. 4.6).
  4. **Wieder einschalten** gestaffelt, einer je 60 s, Prio 1 zuerst (Ziff. 4.3: schrittweise Rückkehr), nur was
     das Skript selbst abgeschaltet hat. Während einer laufenden Begrenzung nur, wenn das Gerät mindestens
     5 min aus war und **Summe + seine Nennleistung ≤ Grenze − 200 W** (mit Volllast gerechnet, nicht mit dem
     Momentanwert).
  5. Protokoll `/steuve_log.csv`: minütlich während einer Begrenzung und 10 min danach.
- **Fake-Plätze** simulieren Verbraucher mit Testprofilen (Waschmaschine, Trockner, Geschirrspüler, Heizstab),
  ohne etwas im Netz zu schalten. `STVMODE` legt fest, was ein simuliertes Gerät nach dem Abschalten tut
  (0 = hält an und läuft weiter, 1 = startet von vorn, 2 = bleibt aus).
- Befehle (Präfix `STV`): `STVSTAT`, `STVLIST`, `STVCFG`, `STVSET p,fake,abs,ip,nenn,profil,name`, `STVPROBE p`,
  `STVMODE 0|1|2`, `STVSIM <W> [s]`.

⚠️ **Dateiname des Skripts kurz halten** (bis TinyC 1.6.73 höchstens 26 Zeichen ohne `.tcb`). Bei längeren Namen
fiel die `.pvs` auf denselben Pfad wie das Programm und überschrieb es (behoben ab 1.6.74).

## Einrichtung Schritt für Schritt

1. Firmware mit `USE_EEBUS_STEUVE` bauen und flashen. `steuve.html` ins Dateisystem laden.
2. Skript übersetzen, als `slot0-steuve_v004.tcb` in Slot 0 laden, Autostart an. Kennung setzen:
   `EEBusCsTyp wallbox`.
3. Auf `/steuve` im Reiter **Shellys** die Verbraucher eintragen: IP, Name, Nennleistung, abschaltbar.
4. **Ohne Energiemanager testen:** im Reiter **Test** eine Grenze simulieren (`EEBusCsSim 2000 300`) und
   prüfen, dass die richtigen Verbraucher in der richtigen Reihenfolge ab- und wieder einschalten.
5. **Vertrauen:** `EEBusScan`, dann dem Energiemanager vertrauen: `EEBusCsTrust hm` (`hm` = das HEMS aus der Fundliste, oder seine SKI). **Vor** der
   Kopplung, sonst weist die SteuVE seine ersten Writes ab.
6. **Kopplung mit dem Energiemanager:** dort die Aufnahme neuer EEBUS-Geräte starten und, solange sie offen ist,
   die SteuVE verbinden (`EEBusConnect <ski-anfang>`). Ohne offene Kopplung antwortet der Energiemanager nur
   „pending“ und bricht nach 60 s ab.
7. Prüfen: `EEBusCs` zeigt „gesteuert“, im Reiter **Meldungen** kommen Herzschläge und die Failsafe-Werte an.

⚠️ **Identität:** Die SKI hängt am Zertifikat im Flash. Ein Flashen mit Löschen erzeugt eine neue SKI, und der
Energiemanager kennt das Gerät dann nicht mehr. Nach der Kopplung nur ohne Löschen aktualisieren (Web-Upgrade).

## Was man über Energiemanager wissen sollte

Gemessen an einem handelsüblichen Energiemanager:

- Er **teilt die Grenze der Steuerbox starr durch die Zahl der aktiven EEBUS-Geräte**, nicht nach Bedarf oder
  Nennleistung. Mit einer SteuVE neben Wärmepumpe und Wallbox bekommt jedes Gerät ein Drittel. Auch die
  Failsafe-Werte werden so geteilt.
- Er zählt **jedes aktive EEBUS-Gerät** mit, egal welche Use Cases es anmeldet. Ein Gerät, das nur mithören
  soll, verringert den Anteil der anderen trotzdem.
- Ohne `signDependentAbsValueLimit` in der Grenzbeschreibung schreibt er keine Grenze, nur Failsafe-Werte.

Eine SteuVE lohnt sich also vor allem dort, wo es **sonst keine** steuerbaren Verbraucher am Energiemanager gibt,
oder wo sie mehrere kleine Verbraucher bündelt. Neben einer Wärmepumpe und einer Wallbox nimmt sie ihnen einen
Teil ihrer Leistung weg.

## Grenzen und Offenes

- Eingehende Verbindungen werden nicht gegen die SKI der Gegenstelle geprüft (s. Vertrauen).
- Die Befehle der Steuerbox (`EEBusLpc`, `EEBusLpp` …) sind im Treiber noch enthalten. In Rolle 3 werden sie
  nicht gebraucht.
- Die Verbraucher werden nur geschaltet, nicht stufenlos gedimmt.

---

Shelly und Waveshare sind Marken ihrer jeweiligen Inhaber. Sie werden nur genannt, um anzugeben, mit welchen Geräten die SteuVE erprobt ist; eine Verbindung zu den Herstellern besteht nicht.
