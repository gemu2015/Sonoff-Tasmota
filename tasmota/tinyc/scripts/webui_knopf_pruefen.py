#!/usr/bin/env python3
"""webui_knopf_pruefen.py — welches Skript zeigt seine Einstellungen nicht mehr?

    python3 scripts/webui_knopf_pruefen.py [ordner ...]

⚠️ ANLASS. Frueher bekam JEDES Skript mit einem `WebUI()` automatisch einen
generischen Knopf „TinyC UI" auf der Tasmota-Hauptseite. Der ist entfallen --
er loeste auf den falschen Slot auf und antwortete dann mit 503. Seitdem gilt:

    Nur ein Slot, der mit `webPageLabel(seite, "Beschriftung")` eine Seite
    anmeldet, bekommt einen Knopf.

Skripte, die das nie nachgezogen haben, HABEN ihre Einstellungen noch -- man
kommt nur nicht mehr hin. Aufgefallen ist es an `webcam_tinyc.tc` auf der
DFRobot-Kamera: „da kommt kein cam setup mehr" (gemu 04.09.2026).

⚠️ NICHT jedes `WebUI()` ohne Beschriftung ist ein Fehler. Manche Skripte
zeichnen ihre Bedienung absichtlich direkt in `WebCall()` als rohes HTML
(wallbox, marstek) und haben ein leeres oder rein zierendes `WebUI()`.
Gemeldet wird deshalb nur, wo im `WebUI()` wirklich BEDIENELEMENTE stehen.
"""
import re
import sys
from pathlib import Path

# Die Widget-Aufrufe, die nur auf der /tc_ui-Seite erscheinen.
WIDGETS = ("webCheckbox", "webNumber", "webButton", "webSlider", "webSelect",
           "webText", "webRadio", "webColor", "webTime", "webLabel",
           "webTextField", "webSubmit")


def webui_rumpf(text):
    """Der Rumpf von void WebUI() {...}, per Klammerzaehlung."""
    m = re.search(r"void\s+WebUI\s*\([^)]*\)\s*\{", text)
    if not m:
        return None
    i = m.end()
    tiefe = 1
    while i < len(text) and tiefe:
        if text[i] == "{":
            tiefe += 1
        elif text[i] == "}":
            tiefe -= 1
        i += 1
    return text[m.end():i - 1]


def main(ordner):
    # ⚠️ Ueber die Pfade vereinheitlichen. Steht "." zusammen mit "examples"
    # in der Liste, findet die Suche jede Datei zweimal -- und der Bericht
    # meldet dann doppelt so viele Fehler, wie es gibt.
    dateien = sorted({p.resolve() for o in ordner for p in Path(o).rglob("*.tc")})
    if not dateien:
        print("keine .tc-Dateien gefunden")
        return 1
    wurzel = Path(__file__).resolve().parent.parent
    kaputt, alt, ohne_widgets, mit_label = [], [], [], 0
    for p in dateien:
        text = p.read_text(encoding="utf-8", errors="replace")
        rumpf = webui_rumpf(text)
        if rumpf is None:
            continue
        if "webPageLabel" in text:
            mit_label += 1
            continue
        try:
            kurz = str(p.relative_to(wurzel))
        except ValueError:
            kurz = str(p)
        n = sum(rumpf.count(w) for w in WIDGETS)
        if not n:
            ohne_widgets.append((kurz, 0))
        elif kurz.startswith("legacy_misc"):
            # ⚠️ Altbestand wird nicht ausgeliefert (die .tcb entstehen aus
            # examples/). Er wird GENANNT, aber er lässt die Prüfung nicht
            # scheitern -- eine Prüfung, die dauerhaft rot ist, sieht bald
            # niemand mehr an.
            alt.append((kurz, n))
        else:
            kaputt.append((kurz, n))

    print(f"{len(dateien)} Skripte, {mit_label} mit angemeldeter Seite\n")
    if ohne_widgets:
        print(f"── {len(ohne_widgets)} × WebUI ohne Beschriftung, aber ohne Bedienelemente "
              f"(in Ordnung) ──")
        for f, _ in ohne_widgets:
            print(f"   {f}")
        print()
    if alt:
        print(f"── {len(alt)} × im Altbestand, wird nicht ausgeliefert ──")
        for f, n in alt:
            print(f"   {f:<52} {n} Bedienelemente")
        print()
    if kaputt:
        print(f"── ⚠ {len(kaputt)} Skript(e) ohne Zugang zu ihren Einstellungen ──")
        for f, n in sorted(kaputt, key=lambda t: -t[1]):
            print(f"   {f:<52} {n} Bedienelemente")
        print("\n   Abhilfe: webPageLabel(0, \"…\") in main() aufrufen.")
        return 1
    print("✅ jedes WebUI mit Bedienelementen ist über einen Knopf erreichbar")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["."]))
