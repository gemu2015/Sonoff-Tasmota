# Pre-push-Schutz: keine Zugangsdaten und Kennungen ins Repository

Ein Git-Hook, der einen **Push abbricht**, wenn die neuen Commits eine Zeichenkette enthalten, die nicht öffentlich werden darf: WLAN-Daten, Zähler- und Gateway-Nummern, MAC- und IP-Adressen, Mail-Adressen, Tokens. Er schaut nur in das, was der Push **neu** hinausschickt, nicht in die ganze Geschichte, und nennt im Fehlerfall den Dateipfad und das **Etikett** des Treffers, nie den Wert.

## Einrichten (einmal je Rechner)

```
tasmota/tinyc/scripts/install_pre_push.sh
```

Das legt `.git/hooks/pre-push` an. Es ruft `scripts/pre_push_secrets.py` aus dem Arbeitsordner auf, ein Update des Skripts wirkt also ohne neues Einrichten. Ein vorhandener Hook wird nur mit `--force` ersetzt (und als `pre-push.bak` aufgehoben). Voraussetzung: `python3`.

## Woher die Zeichenketten kommen

1. **WLAN-Zugangsdaten** aus `tasmota/user_config_override.h` (steht in `.gitignore`): `STA_SSIDn` ab 6 Zeichen, `STA_PASSn` ab 4. Gibt es die Datei nicht, wird dieser Teil übersprungen. Eine andere Datei: Umgebungsvariable `CREDS_HEADER`.
2. **Eigene Liste**, eine Zeile je Eintrag, im Format `etikett|wert`, in der ersten vorhandenen Datei von:
   - `$SECRETS_LIST`
   - `.git/secrets.txt` (liegt **im** `.git`-Ordner, kann also nie committet werden; empfohlen)
   - `~/.tinyc_secrets`

   Beispiel:
   ```
   # Zeilen mit # und leere Zeilen werden ignoriert
   Zaehlernummer|1234567890
   Gateway-ID|GW-0815-4711
   MAC-Taster|aa:bb:cc:dd:ee:ff
   Mail|vorname.nachname@example.com
   ```
   Eine Zeile ohne `|` zählt ganz als Wert, das Etikett ist dann die Zeilennummer. Einträge unter 4 Zeichen werden ignoriert (zu viele falsche Treffer). Die Suche **unterscheidet Groß- und Kleinschreibung nicht**: Eine MAC-Adresse wird in Großbuchstaben wie in Kleinbuchstaben gefunden.

## Was passiert beim Push

```
pre-push: secrets in the objects being pushed - push REFUSED:
    docs/foo.md  (Zaehlernummer, MAC-Taster)
```

Dann den Wert aus der Datei entfernen und den Commit ändern: `git commit --amend`, wenn er im letzten Commit steckt, sonst `git rebase -i` (ein zusätzlicher neuer Commit reicht nicht, denn der alte mit dem Wert würde mit hinausgeschickt). Bewusst trotzdem pushen: `git push --no-verify`.

Ohne Liste und ohne `user_config_override.h` meldet der Hook "check skipped" und lässt den Push durch.

## Was er nicht kann
- Er sucht **exakte Zeichenketten aus Deiner Liste**. Er erkennt nicht von selbst, was ein Geheimnis ist (keine Muster wie "irgendeine MAC-Adresse": im Repository stehen absichtlich Beispiel-MACs).
- Er prüft nur Textinhalt der **neuen** Objekte, nicht Dateinamen und nicht Binärdateien in Archiven (gzip-Dateien werden nicht entpackt). Release-Dateien prüft `creds_scan.py`.
- Er läuft nur auf dem Rechner, auf dem er eingerichtet ist. Ein Push von einem anderen Rechner oder über die GitHub-Webseite wird nicht geprüft.
