# Solar-Manager — BYD-Batterie + SMA-Wechselrichter

Ein in der Community entstandenes TinyC-Projekt, das aus einem
Waveshare ESP32-S3-ETH einen vollwertigen Energiemanagement-Controller
fuer eine private Photovoltaik-Anlage macht.

**Hardware**: Waveshare ESP32-S3-ETH · BYD HVS 10.24 Batterie ·
SMA Tripower 10.0 SE Wechselrichter · SMA Home Manager 2.0 ·
Wallbox Warp3 · Shelly-Schaltaktoren.

**Software**: reines TinyC, ohne Berry / Scripter — nutzt Modbus/TCP,
HTTP-WebUI-Hooks, die `forecast.solar`-API, die Day-Ahead-Preis-API von
Energy Charts und die Orchestrierung mehrerer Geraete.

---

## Statusanzeige

Ein angebundenes TFT zeigt live den Batteriestatus — SOC, SoH,
Lade-/Entladeleistung, lebenslange Energiemengen, AC/DC-Wirkungsgrad
sowie die naechsten astronomischen Eckpunkte (Sonnenaufgang, Sonnenmittag,
Sonnenuntergang).

![Statusanzeige](../images/gallery/solar/image.png){ loading=lazy }

## Haupt-WebUI — Batterie-Steuerregeln

Das im Browser bereitgestellte Bedienpanel. Der Bediener schaltet die Batterie
getrennt fuer Laden und Entladen zwischen `Normal`, `Blockiert` und
`Erzwungen` um. Darunter lassen sich beliebig viele Lade-/Entladeregeln
definieren, die Zeitfenster, SOC-Schwellen, Strompreis, PV-Prognose und
aktuelle PV-Leistung als UND-Bedingungen kombinieren.

![Batterie-Steuerregeln](../images/gallery/solar/image-2.png){ loading=lazy }

## SOC-Verlauf

Ein rollender Tagesplot des Batterieladestands mit farbcodierten
Betriebszustaenden (Netzladung / Netzeinspeisung / Normal / PV-Ueberschuss /
Eigenverbrauch / Blockiert). Unterhalb des Diagramms: Eigenverbrauchsanteil,
Autarkiegrad und Batteriezyklenzaehler.

![SOC-Verlaufsdiagramm](../images/gallery/solar/image-3.png){ loading=lazy }

## PV-Ertragsprognose

Aus dem Dienst `api.forecast.solar` fuer zwei Dachausrichtungen (NW und SO);
die tatsaechlich gemessene DC-Leistung wird zur Kalibrierung ueberlagert.
Die Summen fuer heute und morgen stehen oben.

![PV-Prognose](../images/gallery/solar/image-4.png){ loading=lazy }

## Strompreise — fest / dynamisch / Zeitzonen

Es stehen drei Preismodelle zur Auswahl. Der Festtarif-Rechner schluesselt den
Verbraucherpreis in seine regulatorischen Bestandteile auf (Netzentgelt,
Abgaben, Steuern) gemaess dem deutschen Rahmen *14a EnWG*.

![Strompreis-Konfiguration](../images/gallery/solar/image-5.png){ loading=lazy }

## Flexible Netzentgelte (HT / NT / Hochlast)

Zeitzonen-Tarife mit bis zu drei Fenstern fuer Niedriglast / Standard /
Hochlast sowie eine vollstaendig editierbare Liste von Netzzuschlaegen und
Umlagen.

![Flexible Netzentgelte](../images/gallery/solar/image-6.png){ loading=lazy }

## Netto-Preisberechnung

Rueckrechnung vom Brutto-Verbraucherpreis auf den reinen Energie-Anteil
(wird vom Regelwerk fuer Break-even-Entscheidungen genutzt).

![Netto-Preisberechnung](../images/gallery/solar/image-7.png){ loading=lazy }

## Boersenstrompreis (Day-Ahead)

Bei aktivem Dynamikpreis-Modell holt die Boersenstrompreis-Seite den
Day-Ahead-Spotpreis (EPEX) und stellt ihn als farbigen Balken dar —
gruen fuer guenstig, orange fuer teure Stunden.

![Day-Ahead-Boerse](../images/gallery/solar/image-8.png){ loading=lazy }

## Shelly-Orchestrierung

Bis zu N Shelly-Geraete (Plug S 1PM, Plug EM, Mini-Relais …) werden
automatisch entdeckt oder per IP konfiguriert. Jedes Geraet hat eine eigene
Regelliste mit denselben Kombinatoren wie die Batterieregeln, sodass
Verbraucher bei PV-Ueberschuss, guenstigen Netzfenstern oder SOC-Schwellen
geschaltet werden.

![Shelly-Steuerung](../images/gallery/solar/image-11.png){ loading=lazy }

## Einstellungen — Wechselrichter & PV-Anlage

Modbus/TCP-Adresse des SMA-Wechselrichters, maximale Lade-/Entladeleistung,
Anlagenstandort (Lat / Lon) sowie pro Dach Ausrichtung (Neigung + Azimut + kWp).
Ein Prognosekorrekturfaktor gleicht das Modell an die Messung an.

![Wechselrichter- / Anlageneinstellungen](../images/gallery/solar/image-9.png){ loading=lazy }

## Einstellungen — Netzgeraete & Wirkungsgrad

Alle verbundenen Geraete auf einer Seite (Home Manager, Wechselrichter, BMU,
Wallbox), sowie die monatlichen / jaehrlichen / lebenslangen AC- und DC-Energie-
zaehler, aus denen der Rundum-Wirkungsgrad berechnet wird.

![Netzgeraete & Wirkungsgrad](../images/gallery/solar/image-12.png){ loading=lazy }

---

!!! tip "Etwas Aehnliches selbst bauen?"
    Der vollstaendige TinyC-Quelltext gehoert noch nicht zu den Standardbeispielen —
    Kontakt mit dem Autor aufnehmen oder auf einen kuenftigen Drop auf der
    [Versionen-Seite](../releases.md) warten.
