# Matter als Plugin installieren

*English: [MATTER_PLUGIN.md](MATTER_PLUGIN.md)*

Seit 1.6.70 hat nur noch der **ESP32-S3**-Testbau Matter fest eingebaut. Die
Testbauten für C3, C6 und P4 lassen die Engine weg, weil sie auf jedem Gerät
rund 33 KB RAM kostet — auch auf allen Geräten, die Matter nie benutzen. Wer
Matter dort haben möchte, lädt es als Plugin (`MATTERF`). Skripte, Kopplung und
die Seite `/mt` sind dieselben wie mit der eingebauten Engine.

## Welche Firmware, welche Datei

| Firmware (Testing-Release) | Matter | Plugin-Datei |
|---|---|---|
| `tinyc32s3` | eingebaut — das Plugin ist freiwillig; ist es geladen, wird es statt der eingebauten Engine benutzt | `MATTERF_32.bin` |
| `tinyc32c3`, `tinyc32c6`, `tinyc32-p4-full` | **nur als Plugin** | `MATTERF_32r.bin` |
| `tinyc32-4M-plain`, `tinyc8266-4M` | nicht verfügbar | — |

Beide Plugin-Dateien hängen am
[Testing-Release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing).
`_32` ist für Xtensa (S3), `_32r` für RISC-V (C3, C6, P4). Die falsche Datei
wird beim Hochladen abgewiesen.

Eigene Bauten: `-DTINYC_MATTER -DUSE_MATTER_C_PLUGIN_ONLY` ergibt dieselbe
Nur-Plugin-Firmware; `-DTINYC_MATTER` allein behält die eingebaute Engine.

## Vorher wissen

- ⚠️ **Schritt 1 formatiert das Dateisystem.** Skripte (`.tc`/`.tcb`), die IDE,
  Persist-Dateien (`.pvs`) und eine bestehende Matter-Kopplung (`/mtr_fab`)
  sind danach weg. Was bleiben soll, vorher herunterladen: **Werkzeuge →
  Dateisystem verwalten**. Die Tasmota-Einstellungen (WLAN, MQTT, Vorlagen)
  bleiben erhalten.
- ⚠️ **Firmware 1.6.69 oder neuer.** Ältere Firmware kann beim Entfernen einer
  Partition (`chkpt r` / `chkpt d`) die Partitionstabelle ungültig machen; das
  Gerät hängt dann in einer Bootschleife (siehe CHANGELOG, Eintrag 1.6.69).
- **Speicher:** Das Plugin braucht rund 71 KB Heap am Stück. Es holt sie sich
  beim Booten, solange der Heap noch nicht zerstückelt ist. Auf einem C3 ohne
  PSRAM sinkt der freie Heap von etwa 173 KB auf 88 KB.
- **Flash:** Die Plugin-Partition ist 64 KB groß und geht vom Dateisystem ab.

## Schritt 1 — Plugin-Partition anlegen

In der Konsole:

```
chkpt a
```

Das Gerät legt hinter dem Dateisystem eine 64-KB-Partition namens `custom` an,
formatiert das Dateisystem und startet von selbst neu. `chkpt a2` legt 128 KB
an, falls weitere Plugins dazukommen sollen; in 64 KB passt `MATTERF` allein.

`chkpt` ohne Zusatz zeigt die Partitionen; die neue erscheint als
`label: custom`. Meldet das Log `custom plugin partition already there!`, gibt
es die Partition schon — dann gleich weiter mit Schritt 2.

## Schritt 2 — Plugin hochladen

1. **Werkzeuge → Plugins directory.** Den Knopf gibt es erst, wenn die
   Partition aus Schritt 1 existiert.
2. `MATTERF_32.bin` (S3) oder `MATTERF_32r.bin` (C3/C6/P4) auswählen und
   **Start** drücken.
3. Die Tabelle zeigt jetzt `MATTERF` mit seiner Größe (57–61 KB).
4. ⚠️ **Oben auf der Plugin-Seite „Autostart plugins at boot“ anhaken.**
   Beim Booten gestartet bekommt das Plugin seine 71 KB, solange der Heap noch
   am Stück ist. Ohne Autostart geht Matter trotzdem — das Plugin wird dann
   gestartet, wenn ein Skript Matter startet —, aber auf einem Gerät ohne
   PSRAM kann der Block bis dahin zerstückelt sein. Der Haken wird in
   `/plugins.auto` gespeichert (ab Firmware 1.6.70); ein Formatieren des
   Dateisystems löscht ihn, darum erst nach Schritt 1 setzen. Mit älterer
   Firmware geht der Haken beim nächsten Neustart verloren — dort stattdessen
   unter **Einstellungen → Gerät konfigurieren** einen freien GPIO auf
   **Option A** mit der Nummer **7** setzen.

## Schritt 3 — Neustart und Kontrolle

Das Gerät neu starten (**Hauptmenü → Neustart**). Das Plugin muss beim Booten
da sein, damit es seinen Speicherblock bekommt, bevor der Heap zerfällt.

Nach dem Neustart steht im Log:

```
MTR: initializing the Matter plugin (module 1)
MTR: using the Matter plugin (MATTERF)
```

Auf der Plugin-Seite zeigt `MATTERF` jetzt rund 71 KB in der RAM-Spalte. Ab hier
geht alles wie mit der eingebauten Engine: ein Matter-Skript laden (z. B.
`matter_home_bridge.tc` oder eines der anderen `matter_*.tc`-Beispiele), die
Seite **`/mt`** öffnen, **Bind** drücken und den QR-Code in Apple Home, Google
Home oder Alexa scannen.

Weil Schritt 1 das Dateisystem formatiert hat: die IDE mit `TinyCIde` neu
holen und die Skripte wieder hochladen.

## Schritt 4 — WLAN-Energiesparen abschalten (für jedes Matter-Gerät)

Eine Matter-Zentrale (der Apple-Home-Hub, Google, Alexa) spricht das Gerät jederzeit
per **Unicast** an. Ein Gerät mit WLAN-Energiesparen hört das nicht rechtzeitig:
der Router hält die Pakete zurück oder wirft sie weg, und die Zentrale zeigt
„Keine Antwort" — Befehle aus der Home-App kommen spät oder gar nicht an, obwohl
das Gerät online ist und seine Weboberfläche aufgeht. Gemessen an einem C6 mit
−71 dBm (05.10.2026): mit Energiesparen 50–90 % Paketverlust und 5 s Laufzeit,
ohne 0–5 % und 17 ms.

In der Konsole:

```
SetOption127 1
Sleep 0
Restart 1
```

⚠️ Der Neustart ist wichtig: die Option wirkt erst, wenn sich das Gerät neu im
WLAN anmeldet. Beide Einstellungen werden gespeichert; das macht man einmal je
Gerät. Schnelle Kontrolle vom PC: `ping <ip>` sollte kaum Verlust und Antworten
in wenigen zehn Millisekunden zeigen.

## Aktualisieren und entfernen

- **Aktualisieren:** in der Konsole `deiniz N` und `unlink N` (N = Platznummer
  von der Plugin-Seite) oder das 🔥-Symbol auf der Plugin-Seite; danach die neue
  Datei hochladen (Schritt 2) und neu starten. Die Kopplung liegt im
  Dateisystem und bleibt erhalten.
- **Entfernen:** wie oben, nur ohne neue Datei. Nach dem Neustart ist Matter auf
  C3/C6/P4 aus; der S3 nimmt wieder seine eingebaute Engine.
- **Partition zurückgeben:** `chkpt r`. Das formatiert das Dateisystem erneut
  (nur mit Firmware 1.6.69 oder neuer, siehe oben).

## Wenn es nicht klappt

| Anzeichen | Ursache |
|---|---|
| Kein Knopf **Plugins directory** | Keine Plugin-Partition — Schritt 1 fehlt; oder die Firmware kann keine Plugins (`tinyc32-4M-plain`, ESP8266). Log: `Plugins: Partition not found`. |
| Log: `MTR: no Matter plugin (MATTERF) - this firmware has no built-in Matter` | Plugin nicht hochgeladen, oder nach dem Hochladen nicht neu gestartet. |
| Matter geht nach einem Neustart und nach dem nächsten nicht, oder das Plugin zeigt kein RAM | Autostart nicht angehakt (Schritt 2.4): ohne ihn wird das Plugin erst gestartet, wenn ein Skript Matter startet, und auf einem Gerät ohne PSRAM ist der 71-KB-Block dann womöglich schon zerstückelt. |
| Log: `MTR: using the built-in Matter` auf dem S3 | Kein Plugin geladen — in Ordnung, wenn so gewollt, der S3 hat die Engine eingebaut. |
| Hochladen abgewiesen | Falsche Datei für die CPU (`_32` statt `_32r` oder umgekehrt), oder die Partition ist voll (weitere Plugins — `chkpt a2` nehmen). |
| Matter war gekoppelt und ist weg | Schritt 1 formatiert das Dateisystem und damit `/mtr_fab`. Das Gerät in der Steuer-App entfernen und neu koppeln. |
| Home-App meldet „Keine Antwort“, oder Befehle kommen spät oder gar nicht an, obwohl die Weboberfläche aufgeht | WLAN-Energiesparen: Schritt 4 (`SetOption127 1`, `Sleep 0`, Neustart). Die Option wirkt erst nach dem Neustart. |
