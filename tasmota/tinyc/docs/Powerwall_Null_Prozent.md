# Powerwall 2: Entladung unter die Reserve bis auf 0 %

*Recherche vom 16.09.2026. Stand: Hypothese, Freischaltung von Netzladen bei
Tesla beantragt. Prüfkriterium siehe unten.*

## Befund

Die Powerwall 2 entlädt sich seit Langem **unter die eingestellte Backup-Reserve
bis auf 0 %** — im Winter dauerhaft, im Sommer bei hoher Last. Tesla hat auf
mehrere Anfragen nicht reagiert. Auch mit dem Seitenschalter auf AUS zeigte sie
danach 0 %.

Der Wert `pwl`, den `energy_dashboard.tc` von der Gateway-API bezieht, ist der
**Rohwert**. Die App versteckt darunter einen Puffer: App-0 % entspricht
Roh-5 % (bei manchen Geräten inzwischen 7 %), oben laufen beide zusammen.

## Was die 0 % physikalisch sind

| | Beleg |
|---|---|
| Reserve 30 % gesetzt | Entladung stoppt reproduzierbar bei 33,5 % roh — Tesla legt einen weiteren Puffer *über* die Einstellung |
| unter 10 % roh | offizieller Standby, die Powerwall liefert nicht mehr |
| bei 5 % roh | die Firmware lädt selbst aus dem Netz auf 7 % nach — **wenn sie darf** (siehe Hypothese) |
| Zellschaden (Kupferauflösung) | nahe 0 V je Zelle — weit unter jedem BMS-Boden |

Roh-0 % ist ein Softwareboden, keine Zellspannung null. Ein akuter Schaden ist
sehr unwahrscheinlich. **Real** ist beschleunigte Alterung: NMC-Zellen altern
bei tiefer Entladung nachweislich schneller (Impedanzwachstum an der Kathode),
und *Verweilen* bei niedrigem Ladestand ist schlimmer als ein einzelnes tiefes
Ereignis.

Absicherung: deutsche Garantie 10 Jahre, **80 %** Restkapazität, unbegrenzte
Zyklen bei Eigenverbrauch — solange die Powerwall am Internet hängt (sonst 4
Jahre). Alterung durch Teslas eigene Firmware ist damit Teslas Problem.

## Mechanismen, die zu den Symptomen passen

1. **Netzausfall / Inselbetrieb.** Offiziell so gewollt: *„During an outage,
   your Powerwall may discharge below your set reserve percentage."* Sackt der
   Netzanschluss unter hoher Last weg, islandet das Gateway und fährt das Haus
   aus dem Akku — durch die Reserve hindurch.
2. **Überschwingen bei schneller Entladung.** Mehrere Besitzer beschreiben, dass
   die Firmware bei einem plötzlich grossen Verbraucher über die Reserve
   hinausschiesst und in eine Lade-Entlade-Schleife gerät.
3. **Standby-Eigenbedarf aus der Batterie.** Deutsche Besitzer haben gemessen,
   dass der Verbrauch nach einem Firmware-Update von ~100 auf ~700 Wh je Tag
   stieg, weil die ~40 W Eigenbedarf seither aus der **Batterie** statt aus dem
   Netz kommen. 40 W × 24 h ≈ 1 kWh ≈ 7 % von 13,5 kWh **am Tag**.

**Gestrichen:** die Zellheizung. Der Raum wird nie kalt.

**Der Seitenschalter:** Teslas eigene Fehlerbehebung für „Powerwall zeigt 0 %"
nennt als ersten Schritt *„Ensure the Enable switch on the side of the unit is
turned ON"* — mit dem Schalter auf AUS fällt die **Anzeige** auf 0 %. Was der
Akku ausgeschaltet wirklich verliert, sind ~300–340 Wh je Tag (~0,1 %/h).

## ⭐ Hypothese: „Netzladen" ist aus

In Deutschland ist Netzladen **ab Werk aus** und wird per Mail an
`EnergysupportEMEA@tesla.com` freigeschaltet. Mit aktivem Netzladen *„wird die
Powerwall 2 immer mindestens bis zur Backup-Reserve geladen"* (TFF-Forum).

Ohne Netzladen kann der Bodenschutz (5 % → 7 % nachladen) **nicht laufen**.
Dann bleibt nur Mechanismus 3, und der zieht in weniger als einem Tag von 5 %
auf 0. Das erklärt alles auf einmal:

| Beobachtung | Erklärung |
|---|---|
| Winter dauerhaft 0 % | am Boden angekommen, kein PV, kein Netznachladen |
| Sommer bei hoher Last | Überschwingen unter den Boden, kein Nachladen — bis die Sonne am Morgen zurückholt |
| ausgeschaltet „trotzdem auf null" | Anzeigeeffekt des Schalters; die Elektronik zieht ohnehin weiter |

## Massnahmen

1. **Netzladen freischalten lassen** — beantragt am 16.09.2026. ⚠️ „Keine
   Genehmigung des Netzbetreibers mehr nötig" stammt aus dem Forum, nicht von
   Tesla. Bei KfW-275-Förderung war Netzladen in der Bindungsfrist untersagt.
   Preis: die ~40 W kommen dann im Winter aus dem Netz, ~1 kWh am Tag.
2. **Reserve höher (15–20 %)** — nur *mit* Netzladen sinnvoll.
3. **Last lokal steuern** — bei Roh-SOC < ~10 % Wallbox und Wärmepumpe drosseln.
   Das Einzige, was ohne Cloud noch geht.
4. **Cloud-Automatisierung** nur bei Bedarf: `pypowerwall` im Cloud-Modus kann
   `set_reserve` (≤ 80 %) und `set_mode("backup")`.

⚠️ **Die lokale Gateway-API setzt die Reserve nicht mehr** (ging früher). Tesla
räumt seit Firmware 23.44 lokale Schnittstellen ab.

## Prüfkriterium nach der Freischaltung

Die Powerwall-SOC-Kurve auf .118 (`energy_dashboard.tc`, Ring `soc_pw`) muss
nachts am Boden **waagerecht** liegen, mit kleinen Nachladezacken, statt mit
~0,3 %/h abzufallen. `pwl` darf nie mehr unter ~5 % roh fallen. Kippt die Kurve
weiter, war es etwas anderes — dann Netzausfall-Ereignisse aus dem Gateway
dagegenlegen.

## Was nicht belegt werden konnte

Teslas Support-Seiten und das TFF-Forum weisen direkte Abrufe ab; die Zitate
stammen aus Suchtreffern. Die Heizleistung der Powerwall 2 ist nirgends
beziffert (irrelevant, da Raum warm). Dass es eine Powerwall **2** ist, ist aus
der Form der API-Daten geschlossen.

## Quellen

* Tesla: Powerwall 2 zeigt 0 % — Fehlerbehebung
  <https://energylibrary.tesla.com/docs/Public/EnergyStorage/Powerwall/2/TroubleshootingManual/SetupAndInstall/en-us/GUID-3E4F8FA4-93A4-4070-93FC-B9D86E6A8931.html>
* Tesla: Garantie Europa (deutsch)
  <https://www.tesla.com/sites/default/files/pdfs/powerwall/powerwall_2_ac_warranty_europe_1-3_german.pdf>
* TFF: Powerwall „verliert" Strom über Nacht
  <https://tff-forum.de/t/powerwall-verliert-strom-ueber-nacht/163585>
* TFF: Powerwall mit Nachttarif laden (Netzladen-Freischaltung)
  <https://tff-forum.de/t/powerwall-mit-nachttarif-laden/20461>
* TMC: Will PW prevent itself from completely discharging
  <https://teslamotorsclub.com/tmc/threads/will-pw-prevent-itself-from-completely-discharging.312771/>
* TMC: Powerwall 2 strange behaviour (Nachladen 5 → 7 %)
  <https://teslamotorsclub.com/tmc/threads/powerwall-2-strange-behaviour.107285/>
* TMC: Powerwall 2 Vampire Drain (Standby-Verbrauch)
  <https://teslamotorsclub.com/tmc/threads/powerwall-2-vampire-drain.152592/>
* TMC: Discrepancy between app and Powerwall API (33,5 % bei Reserve 30 %)
  <https://teslamotorsclub.com/tmc/threads/discrepancy-between-app-and-powerwall-api.104903/>
* pypowerwall (Cloud-Schreibzugriff, ≤ 80 %)
  <https://github.com/jasonacox/pypowerwall>
* Firmware 23.44 — lokale Vitals entfernt
  <https://github.com/jasonacox/Powerwall-Dashboard/discussions/402>
* NMC-Kathodenimpedanz bei tiefer Entladung
  <https://pmc.ncbi.nlm.nih.gov/articles/PMC13485041/>
