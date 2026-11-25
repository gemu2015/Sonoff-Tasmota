/*
  xdrv_126_eebus_guard.ino - EEBUS Energy Guard / Steuerbox

  Rolle: Energy Guard — tritt als vorgelagerte
  Steuerbox auf und gibt einer Anlage Leistungsgrenzen vor.

  v247 (24.11.2026, meierchen006): DeviceConfig-Schluessel als Paare keyId -> Bedeutung (bis 8 je Entity) statt keyId als
  Tabellenplatz (nur 0..7); Geraete mit hoeherer keyId (Wallbox: 11/12) wurden sonst nicht erkannt.
  Gleich in HEMS Firmware v042 (xdrv_129) und SteuVE Firmware v041 (xdrv_128).
  Ausserdem tote Plaetze: ein gescheiterter eigener Verbindungsversuch blieb mit Adresse/SKI stehen, die
  echte Verbindung lief auf einem anderen Platz -> zwei Eintraege je Gegenstelle, Suche fand den toten (SteuVE-Seite
  "nicht verbunden" trotz Herzschlag, 24.11.). Neu: EebusTotePlaetzeRaeumen() im Sekundentakt leert tote Plaetze
  derselben Gegenstelle; ein Keep-Alive-Platz bleibt (braucht SKI fuer den Wiederanruf), ruft aber nicht an, solange die
  Verbindung steht. EebusConnBySki() nimmt zuerst einen verbundenen Platz.
  Gleich in HEMS FW v044 und SteuVE FW v042.
  Ausserdem (wie HEMS FW v043): FREIGABE nie mit Wert 0 - statt dessen Nennleistung des Geraets (sonst 11.000 W);
  das Waermepumpen-Gateway setzte eine inaktive 0 kurz als Grenze am eBUS um (Verdichterstopp, 23.11. 17:35 nach Box-Neustart).

  v246 (23.11.2026, Abgleich SPINE-Testspezifikation V1.1.0 + SPINE-IG V1.0.0 gegen diesen Treiber):
  A1 (TS SUBS_001/002, BIND_001): nodeManagementSubscriptionData/BindingData lesen wurde mit Fehler 6 beantwortet, obwohl
     in der Discovery angeboten. Jetzt fuehrt jede Verbindung eine Liste der Abos/Bindings, die die Gegenstelle bei UNS
     angelegt hat (EEBUS_GB_MAX), und liefert sie aus (Abo auf NodeManagement = Eintrag Client 0/0 - Server 0/0).
  A2 (TS BIND_001): Binding auf unser NodeManagement wird abgelehnt (Fehler 7), unbekannte Calls mit Fehler 6 - bisher
     wurde jeder Call mit 0 bestaetigt. Ablehnungen gehen IMMER raus, unabhaengig von ackRequest.
  A3 (TS FC_001): NodeManagement-Befehle (Calls und nodeManagement*-Reads) an ein anderes Ziel als Entity 0/Feature 0
     werden mit Fehler 6 abgelehnt.
  A4 (TS RTC_003, IG 2.2 Z. 136-138): useCaseAvailable eines Servers wird ignoriert (Client SHALL ignore) - v243 hatte
     bei false LPP/GridConnectionPoint verworfen.
  B1: SubscriptionDelete ohne Feature-/Entity-Angabe loescht alle passenden Abos; Herzschlag-/LoadControl-/NodeManagement-
     Merker werden aus der Liste neu bestimmt (vorher nur bei Feature 5/2 und nie fuer LoadControl).
  B2: BindingDelete der Anlage auf unser LoadControl-Binding und Fehler 9 ("Binding noetig") auf einen Write setzen
     lpc_bound zurueck -> der naechste Write bindet neu.
  B3: Abo-Ziel (Herzschlag/LoadControl/NodeManagement) auch aus serverAddress erkannt - serverFeatureType ist nur empfohlen.
  B4: reply mit ackRequest wird quittiert (wie notify). B5: Quittungen auf write/call mit unserer Zieladresse als Absender
     statt fest 0/0.
  C2: ackRequest und serverFeatureType werden auch mit Leerraum um den Doppelpunkt erkannt.
  Bewusst NICHT: IG 2.6.1 Z. 233 "gueltiges NACK SHALL NOT Neuversuch ausloesen" - die LPC-Testspezifikation (EGMessages_002,
  LPC-906) verlangt nach Fehler 7 den Neuversuch; die speziellere Use-Case-Regel geht vor. Format von specificationVersion
  nur Warnung (wie bisher). Writes/Reads auf nicht angebotene Features (Fehler 6) erst nach Mitschnitt Wallbox/VR921.

  v245 (23.11.2026, Befund EGMessages_002 am 23.11.): im Betriebslogbuch fehlten abgelehnte Wiederholungen - der Eintrag
  "Vorgabe" wurde unterdrueckt, wenn Richtung/Wert/Ergebnis dem letzten glichen (v215), und die erste Ablehnung vor dem
  sofortigen Neuversuch (LPC-906) wurde gar nicht protokolliert. Fuer die Nachweiskette (VDE-FNN-Empfehlung Tenorziffer 2a:
  Nichtausfuehrbarkeit protokollieren) jetzt: jede Ablehnung/jeder Fehlschlag (LPC_FAIL) wird eingetragen, ab der 1.
  Wiederholung mit deren Nummer; die erste Ablehnung vor dem Neuversuch bekommt einen eigenen Eintrag. Bestaetigungen
  weiter ohne Doppel. Menge begrenzt: Wiederholungen 60 s, 120 s ... hoechstens alle 10 min (EEBUS_WDH_MAX_MS).

  v244 (23.11.2026, Befund EGConnection_002 mit 6 min Netzausfall am 22.11.): nach langem Ausfall sah der Wiederaufbau
  nur noch alle 30 s nach (Ping), ohne Ping-Antwort verband er nur alle 120 s. Kam das Netz kurz nach einem Nachsehen
  zurueck, lag die Freigabe bei ~50 s oder mehr - die 60-s-Frist nach Verbindungsaufbau ([LPC-913]/[LPP-913]) wurde knapp
  bzw. verfehlt. Jetzt: Wechsel "Netz weg" -> "Netz da" (FUNC_NETWORK_DOWN/UP; UP kommt jede Sekunde, nur der Wechsel
  zaehlt) stoesst das Nachsehen SOFORT an (0,5 s) und startet die schnelle Phase (alle 5 s) neu, weil die Gegenstelle
  spaeter zurueckkommen kann als die Box. Logbuch "Netz wieder da - Wiederaufbau sofort" je wartender Verbindung.

  v243 (22.11.2026, Pruefung MGCP TS 1.0.0 + LPP TS 1.0.0 mit Testspezifikationen V1.0.2):
  MGCP: (1) valueState "error"/"outOfRange" -> Wert ignorieren, Eintrag ohne Wert = nicht vorhanden (MGCP Z. 424-426,
  859-874, [MGCP-003]); (2) scale nur aus DEM Eintrag, sonst 0 (Tab. 14-20); (3) Phase und Bezug aus
  electricalConnectionParameterDescription (Tab. 18/19, [MGCP-051], [MGCP-061]), Leiter-Leiter-Spannungen eigene Felder
  ("VoltageLL"); (4) positiveEnergyDirection "produce" -> Vorzeichen von Leistung/Strom umkehren (Tab. 15-19);
  (5) measurementType/unit muessen zur Bedeutung passen (Tab. 15-20); (6) pvCurtailmentLimitFactor nur mit Einheit pct
  und 0-100 % (Tab. 14), 4 DeviceConfig-Entities; (7) Netzanschlusspunkt auch aus der Use-Case-Auskunft (Z. 444-467);
  (8) erfundene Messwert-Antworten nur noch in den Pruefrollen 0/2/3 - Rolle 1 bietet diese Server nicht an (Z. 913-914);
  Messwert-Leser vertraegt Leerraum, Zuordnungen je Sitzung neu. Bewusst NICHT: measurementConstraints auswerten
  (SHOULD) - ein Wert ausserhalb des Bereichs wird nach Z. 425 ohnehin ignoriert.
  LPP: (9) Failsafe-Werte erst nach dem ersten Grenz-Write JEDER vorhandenen Richtung (LPP Z. 389-392, LPC Z. 372-375);
  (10) 60-s-Frist je Richtung, Logbuch "LPP-913 Frist verfehlt" ([LPP-913] Z. 386-388); (11) neue Nennleistung Erzeugung ->
  Failsafe neu ([LPP-021]); (12) Nennleistung nur mit characteristicContext entity, die der gesteuerten Entity hat Vorrang
  (Tab. 18/19); (13) Einspeisegrenze nur, wenn die Anlage LPP als ControllableSystem anbietet (LPP 3.1.2 Z. 766-788);
  (14) Failsafe-Schluessel mit isValueChangeable false nicht schreiben (Tab. 15/16). Bewusst so: ohne gemeldete
  Nennleistung kein Einspeise-Failsafe (60 % von "unbekannt" waere geraten).

  v242 (22.11.2026, SHIP-Pruefung gegen SHIP TS 1.1.0 + SHIP-Testspezifikation 1.0.0, meierchen006: "Box normgerecht"):
  (1) CMI als Client ohne Blockieren, CmiTimeout 15 s, nur MessageType+CmiHead auswerten (SHIP Z. 2052-2084); Server:
  falsche CMI -> CMI senden und schliessen (Z. 2061-2067). (2) Hello nach SHIP 13.4.4.1: Wait-For-Ready 60 s, "pending"
  ohne Verlaengerungswunsch ignorieren, prolongationRequest +60 s und Update (Z. 2202-2313), Server schickt sein Hello
  sofort; Abbruch sendet "aborted" (Z. 2292-2297). (3) Protocol Handshake: select pruefen (Client, Z. 2403-2416), Spiegel
  pruefen (Server, Z. 2417-2428), Wait-Timer 10 s, Fehler 1/2/3 senden (Z. 2441-2446). (4) PIN: Peer-pinState binnen
  10 s, sonst schliessen; Datenphase erst danach, auch als Server (Z. 2658-2664, 2747-2754). (5) Anrufer nur mit
  Zertifikat (Z. 692-693) und nur, wenn vertraut (Partnerliste, EEBusTrust, Zuordnung; Z. 2125-2126, 2255-2256).
  (6) SubjectKeyIdentifier im Zertifikat muss zur SKI des Schluessels passen (Z. 1265-1269, TC_SHIP_SEC_001/002).
  (7) Doppelverbindung: groessere SKI schliesst sofort die aeltere, kleinere nach 3 s (Z. 1353-1366). (8) Server-Records
  <= 1024 B (Z. 714-716). (9) SNI = mDNS-Hostname (Z. 723, 804). (10) WebSocket-Header pruefen (Z. 802), Text-/reservierte
  Opcodes -> Close 1003/1002, Close beantworten (Z. 826-830). (11) Pong-Frist 10 s (Z. 854-855). (12) accessMethods mit
  dnsSd_mDns (Tab. 22); connectionClose nur in der Datenphase (Z. 3103-3105). (13) JSON mit Leerraum (TC_SHIP_MSG_003).
  (14) mDNS-Eintraege ohne txtvers=1 verwerfen (Z. 570-573). Bewusst NICHT: Kennung der Gegenstelle dauerhaft speichern
  (Z. 2929-2930) - die Box findet Partner ueber die SKI im mDNS, die id kommt dort jedes Mal neu mit.

  v241 (22.11.2026, Normpruefung SPINE/LPC + LPC-Testspezifikation V1.0.2, meierchen006: "Box normgerecht machen"):
  (1) Grenzauswahl nur noch nach LPC TS Tab. 22 / Z. 1509-1511: limitType signDependentAbsValueLimit, limitCategory
  obligation, limitDirection consume (LPP: produce), scopeType activePowerLimit; ohne measurementId ist die Beschreibung
  ungueltig (LPC IG 3.4 Z. 646-649) - kein Rueckfall mehr auf maxValueLimit oder die erste limitId. Eigene Beschreibung
  (Rollen 0/2/3) ebenso. (2) isLimitChangeable false -> nicht schreiben (LPC TS Tab. 23). (3) Abo-Ergebnis auswerten,
  bei Ablehnung regelmaessig lesen (LPC TS Z. 1466); Herzschlag-Abo bei Herzschlagverlust erneuern (SPINE Z. 2406-2409).
  (4) maxResponseDelay der Anlage als Antwortfrist (SPINE Z. 1196-1198). (5) Hinweis bei Schreiben < 5 min (LPC IG 2.10,
  SHOULD; bewusste Abweichung: die Box gibt Vorgaben unverzoegert weiter, Testfall EGMessages_003 schreibt 25x).
  (6) "BESTAETIGT aktiv" nach Ablauf auch ohne Rueckmeldung der Dauer richtigstellen. (7) Rest eines Frames: 10 s statt
  1 s warten, nicht blockierend (SHIP 8.3/8.4 Z. 669-686 TCP-Wiederholung 1+3+6 s, SHIP Z. 841-842 Verbindung halten).
  (8) Sicherung erst mit Herzschlag-Abo der Anlage (LPC-913, TS Tab. 15-17). (9) Herzschlag nach > 110 s Pause gilt als
  zurueck -> Herzschlag + Grenze (LPC-911/913, EGConnection_002). (10) Wiederaufbau nach Neustart auch ohne Uhr nach
  60 s (EGConnection_001/003) und ohne Aufgeben (EGConnection_003 "automatically reconnects after a black start").

  v240 (22.11.2026, meierchen006: "normgerechte Geraete"): heartbeatTimeout PT60S (LPC TS Tab. 13); Herzschlag-Notify nur
  an Abonnenten, blosses Lesen nur beantworten (SPINE Z. 2417-2419); Binding-Quittung ueber msgCounterReference der
  eigenen LoadControl-Bindung (LPC TS Z. 1450-1452).

  v239 (22.11.2026, Normpruefung, Bericht 2026-11-22--1815): (1) Antwortfrist der LPC-Kette auch bei eingehenden
  Verbindungen (SPINE 5.2.5.3); (2) Herzschlag vor dem Write auch im SteuVE-Betrieb, wenn > 50 s (LPC-913); (3) abgelehnte
  Failsafe-Werte spaeter erneut (IG 2.15); (4) LPC-913-Frist und fehlendes Herzschlag-Abo der Anlage in Log/Logbuch;
  (5) Stumm-Abbau erst nach 2x Herzschlag-Zusage; (6) Abbruch-Erkennung auch bei reply; (7) unbekannter Read -> errorNumber 6,
  notify-Quittung, Abo-Loeschung + geaenderte Geraeteliste (SPINE Z. 1088-1093, 1134-1145, LPC TS Z. 1455, 1470-1477);
  (8) unvollstaendige Grenzen-Beschreibung melden (IG 3.4). Binding-Zuordnung bleibt bewusst wie bisher (erprobt).

  Kann:
    SHIP als Client UND als Server (Gegenstellen bauen die Verbindung teils selbst zu uns auf),
      TLS 1.2 mit eigenem EC-Zertifikat, mDNS (_ship._tcp), bis zu EEBUS_MAX_CONN Gegenstellen
      gleichzeitig — davon EINE eingehend
    SPINE: Detailed Discovery, Bindings, Subscriptions, Heartbeat
    § 14a EnWG - Bezugsbegrenzung    (LPC, limitationOfPowerConsumption)
    § 9 EEG    - Einspeisebegrenzung (LPP, limitationOfPowerProduction)
    Messwerte am Netzanschlusspunkt (MGCP) lesen
    Weboberflaeche unter /steuerbox (liest steuerbox.html aus dem Dateisystem)

  Offen: mehrere eingehende Verbindungen gleichzeitig. Doppelverbindungen (SHIP Z. 1353-1366) loest seit v242 die Box.

  Identitaet der Gegenstelle (Stand v231): in BEIDEN Richtungen ueber die SKI aus ihrem Zertifikat.
    - eigener Anruf (v192/v193): tls_mini liefert den oeffentlichen Schluessel der Gegenstelle
      (BearSSL::ShipPeerPubKey), die SKI = SHA-1 ueber den Punkt muss zur SKI aus der Fundliste passen,
      sonst wird nicht verbunden. Ohne EC-Schluessel wird seit v231 ebenfalls nicht verbunden.
    - Anrufer (v190): der TLS-Server fordert das Client-Zertifikat an, EebusSrvX509 liest den Schluessel.
      Schickt ein Anrufer KEIN Zertifikat, gilt als Rueckfall die Zuordnung ueber Adresse/Kennung aus
      der Fundliste (v185/v188); dann steht im Slot die Adresse als Pseudo-SKI.

  Portierungs-Quelle (nur gelesen, nie dort gebaut): Tinkerforge esp32-firmware modules/eebus
  (LGPL v2+, Copyright Tinkerforge GmbH) — SHIP-mDNS-Logik aus ship.cpp, Zertifikat nach
  cert_generator.cpp. Guard-Sequenzen nach eebus-go/evcc.

  Aktivierung in user_config_override.h:
    #define USE_EEBUS_GUARD

  Kommandos — Verbindung:
    EEBusScan                     mDNS-Suche nach SHIP-Diensten (asynchron, ~1,5 s)
    EEBusPeers                    gefundene Geraete mit SKI, Typ und Betriebsart
    EEBusCert [2]                 SKI anzeigen; mit 2 Zertifikat NEU erzeugen (Pairings verfallen)
    EEBusConnect <ski|idx>        Verbindung aufbauen (TLS+WS+CMI, Handshake laeuft asynchron
                                  weiter). Beim ersten Kontakt meldet die Gegenstelle "pending" —
                                  dann unsere SKI dort bestaetigen. Bis EEBUS_MAX_CONN parallel.
    EEBusConnectIp <ip[:port]>    Verbindung ohne Scan (auch [ipv6]:port); die SKI kommt aus dem
                                  Zertifikat der Gegenstelle (v231)
    EEBusDisconnect [ski|idx]     ohne Argument alle trennen
    EEBusStatus                   Verbindungs-Slots mit Handshake-Phase und letztem Ergebnis
    EEBusData                     Ist-Zustaende und Messwerte aller Verbindungen als JSON
    EEBusAdvertise 0|1            eigene mDNS-Ankuendigung (laeuft ab Boot automatisch)
    EEBusRole 0..3                0 = nur lesen, 1 = Steuerbox, 2/3 = abweichende Identitaeten
    EEBusHems 0|1                 Betriebsart: 0 = einzelne Verbrauchseinrichtung, 1 = Energie-
                                  manager. Wird beim Verbinden aus dem Geraetetyp abgeleitet und
                                  gilt JE VERBINDUNG (v231); der Schalter ist nur die Vorgabe fuer
                                  Verbindungen ohne Typangabe.
    EEBusPeerMode <ski> <art>     Betriebsart je Geraet: hems | steuve | del; ohne Arg = Liste
    EEBusAnmeld 0|1               nach dem Verbinden je eine Freigabe fuer Bezug und Einspeisung
                                  senden (kein Limit) — damit gilt die Steuerbox als verbunden

  Kommandos — § 14a EnWG (Bezug):
    EEBusLpc <ziel> <watt> [s]    Bezugsgrenze setzen; ohne Dauer gilt die Vorgabe
    EEBusRelease <ziel>           Bezugsgrenze freigeben
    EEBusReleaseAll               alle Bezugsgrenzen freigeben (wirkt NICHT auf die Einspeisung)

  Kommandos — § 9 EEG (Einspeisung):
    EEBusLpp <ziel> <watt> [s]    Einspeisegrenze setzen (Betrag angeben, Vorzeichen setzt der
                                  Treiber)
    EEBusLppFrei <ziel>           Einspeisegrenze freigeben

  Kommandos — Auslesen und Diagnose:
    EEBusDelDur -1|0|1            Geltungsdauer der Gegenstelle vor dem Schreiben loeschen:
                                  -1 automatisch, 0 nie, 1 immer
    EEBusAutoConn                 Zustand des selbsttaetigen Wiederaufbaus anzeigen
    EEBusAutoConn <sekunden>      Wartezeit nach dem Hochfahren (Vorgabe 120, 0 = abgeschaltet).
                                  ⚠️ Wirkt nur bis zum naechsten Neustart — die Vorgabe steht im
                                  Code. Bewusst so: der Wert ist erprobt, und ein nur zeitweise
                                  verstellbarer Wert verleitet mehr zu Irrtuemern als er nuetzt.
    EEBusAutoConn -1              gemerkte Gegenstelle vergessen (kein Wiederaufbau mehr)
    EEBusMess <ski>               Messwerte und Kenngroessen erneut abfragen (nur lesen)
    EEBusStruct <ski> [suchwort]  Entities, Features, Actors, Use Cases der Gegenstelle
    EEBusRead <ski> <ent> <feat> <typ>   ein Feature gezielt auslesen; <typ> ist ein Kuerzel
                                  (Measurement, LoadControl, DeviceConfiguration, ...) ODER direkt
                                  ein Funktionsname auf ...Data
    EEBusRead                     die zuletzt aufgehobene Antwort ausgeben
    EEBusLog 0|1|2                Mitschnitt aus/an; 2 = auf das Dateisystem schreiben
    EEBusTarget <ent> <feat>      Schreibziel von Hand setzen (-1 = wieder automatisch)
    EEBusProbe <ent> <v> <b> [1]  Feature-Sweep; mit 1 vorher Subscribe und Binding
    EEBusProvide <watt>           eigenes LoadControl mit diesem Wert bereitstellen (0 = aus)
    EEBusOpen 0|1                 Discovery-Eroeffnung: 0 reaktiv adressiert, 1 adresslos
    EEBusTrust [<SKI>|0]          v242: erlaubte Anrufer (ohne Argument: Liste, SKI: aufnehmen, 0: Liste leeren).
                                  Seit v190 ist die SKI eines Anrufers mit Zertifikat bekannt; die
                                  Liste selbst ist noch nicht gebaut, der Schalter tut nichts.

  Wiederaufbau nach einem Neustart
  --------------------------------
  Die Fundliste des Scans liegt nur im Arbeitsspeicher. Nach einem Neustart wusste das Geraet
  deshalb nicht mehr, mit wem es verbunden war — jemand musste von Hand scannen und verbinden.
  Jetzt wird die SKI der Gegenstelle gemerkt, sobald eine Verbindung die Datenphase erreicht,
  und beim Hochfahren einmal selbsttaetig wiederhergestellt (danach uebernimmt der Keep-Alive).
  Ein bewusstes EEBusDisconnect legt den Wiederaufbau fuer den laufenden Betrieb stumm, LOESCHT
  die Merkung aber NICHT: "vor dem Update trennen" ist die haeufigste Art zu trennen, und dabei
  meint niemand "komm nicht wieder". Geloescht wird nur auf ausdrueckliche Ansage: EEBusAutoConn -1.

  Herzschlag der Gegenstelle
  --------------------------
  Ueberwacht wurde bisher nur, wann zuletzt irgendetwas hereinkam. Das taugt nicht als Mass fuer
  die Beziehung: ein Energiemanager schickt zwei Messwert-Meldungen je Sekunde, dieser Zeitpunkt
  ist also nie alt — auch dann nicht, wenn die Gegenstelle uns laengst abgemeldet hat und in ihren
  Failsafe gefallen ist. Genau das ist vorgekommen: die Verbindung lief danach dreizehn Stunden
  weiter, und die Anzeige meldete elf Stunden lang einen Zustand, den es nicht mehr gab.
  Der Herzschlag der Gegenstelle traegt dagegen eine ZUSAGE ("heartbeatTimeout", z.B. PT2M).
  Sie wird jetzt ausgewertet: bleibt er laenger als das Doppelte davon aus, gilt die Beziehung als
  verloren, es erscheint eine Zeile mit Uhrzeit im Protokoll, und EEBusStatus fuehrt Zaehler und
  Zeitpunkt weiter (damit sich Vorfaelle auch spaeter noch nachlesen lassen).
  ⚠️ Die Anmeldung wird nur dann selbsttaetig erneuert, wenn KEINE eigene Grenze aktiv ist —
  eine Wiederholung wuerde eine gesetzte Begrenzung sonst aufheben.

  ⚠️ Der Wiederaufbau beginnt ERST, wenn Netz UND Uhr stehen, und dann noch mit Wartezeit:
    - Die Uhr, weil der Herzschlag einen UTC-Zeitstempel traegt. Ohne Zeitsynchronisation meldet
      sich das Geraet mit einer Uhrzeit aus dem Jahr 1970 an; die Gegenstelle haelt den Herzschlag
      fuer laengst abgelaufen und lehnt danach JEDES Limit ab — ohne dass irgendwo ein Fehler
      erscheint. Gemessen: nach dem Einschalten stand das Netz rund zwei Minuten vor der Uhr.
    - Die Wartezeit, weil ein zu frueher Verbindungsversuch in "cmi resp n=-1" endet und der
      Verbindungs-Slot DANACH bis zum naechsten Neustart im Fehler bleibt. Nach einer
      Spannungswiederkehr kommt hinzu, dass Switch, Router und die Gegenstelle selbst erst
      hochfahren — per mDNS ist dann noch gar nichts zu finden.
  Aus demselben Grund wachsen die Abstaende zwischen den Versuchen (30/60/120/300 s, dann Schluss),
  und meldet die Gegenstelle "pending", wird 5 Minuten statt 8 Sekunden gewartet: "pending" heisst,
  dass dort ein MENSCH eine Freigabe erteilen muss, und dichtes Wiederholen schadet nur.

  Dateien (Flash-FS): /eebus_cert.der, /eebus_key.der, /eebus_peer.txt (gemerkte Gegenstelle)
  Mitschnitt (SD):    /eebus_ship.log — jede SHIP-Nachricht TX/RX mit Zeitstempel
                      (Rohdaten-Basis fuer das spaetere Pruefprotokoll)
  (DER statt PEM: die mbedTLS-PEM-Write-Funktionen sind im Tasmota-Framework nicht
  einkompiliert, die DER-Varianten schon. SHIP/TLS arbeitet intern ohnehin mit DER.)
*/

#ifdef ESP32
#ifdef USE_EEBUS_GUARD

#define XDRV_126                126

// WICHTIG (Heap-Korruptions-Fallebewiesen): xdrv_123_plugins.ino setzt
// "#pragma pack(4)" und setzt es NIE zurueck. Da alle .ino zu EINER tasmota.ino.cpp
// zusammengesetzt werden, wuerden alle hier inkludierten Header (BearSSL-Klasse,
// mbedtls-Kontexte) mit pack(4) geparst — die tls_mini-Lib und libmbedcrypto sind aber
// mit Default-Alignment gebaut. Folge war: sizeof(WiFiClientSecure_light) .ino=200 vs.
// lib=208 -> new() 8 Bytes zu klein -> Konstruktor zerschiesst tlsf-Heap-Header ->
// sporadischer StoreProhibited im W5500/tcpip_thread; ausserdem schrieben die inline-
// Setter (setECDSA/setRSAOnly) an falsche Member-Offsets (mitten in _last_error).
// Deshalb: Packing fuer UNSERE Includes auf Default zuruecksetzen, danach wieder pack(4)
// (pop), damit nachfolgende Treiber unveraendert bleiben.
#pragma pack(push)
#pragma pack()
#include "mdns.h"
#include <mbedtls/version.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/oid.h>
#include <mbedtls/base64.h>
#include <lwip/sockets.h>   // v205: send(..., MSG_DONTWAIT) im Server-Sendepfad
#include "ping/ping_sock.h"   // v229: Erreichbarkeit vor dem Wiederaufbau pruefen (ESP-IDF esp_ping, eigene Task)
#include "esp_task.h"        // v229: ESP_TASK_PING_STACK fuer ESP_PING_DEFAULT_CONFIG
#include <SHA1Builder.h>   // Arduino-Core Hash-Lib — mbedtls_sha1 ist im Tasmota-mbedTLS wegkonfiguriert
#include <t_bearssl.h>   // BearSSL br_sha1 (korrekte SHA1; SHA1Builder lieferte falsche Hashes)
#include <NetworkServer.h> // eingehender TCP-Listener (ESP32-Core, funktioniert ueber WiFi + Ethernet)
#include <NetworkClient.h>
// TLS-Transport: Tasmotas mbedTLS hat KEINE SSL-Schicht (mbedtls_ssl_* fehlen komplett),
// Tasmota nutzt BearSSL. Wir verwenden denselben Client wie MQTT/AWS-IoT (Client-EC-Zertifikat).
#include "WiFiClientSecureLightBearSSL.h"
#pragma pack(pop)   // pack(4) von xdrv_123 fuer den Rest der TU wiederherstellen (s.o.)

#define EEBUS_MAX_PEERS         8
#define EEBUS_SCAN_TIMEOUT_MS   1500
#define EEBUS_CERT_FILE         "/eebus_cert.der"
#define EEBUS_KEY_FILE          "/eebus_key.der"
   // Gemerkte Peer-SKI fuer den selbsttaetigen Wiederaufbau nach einem Neustart. Die Fundliste
   // aus dem Scan liegt NUR im RAM — nach dem Hochfahren weiss das Geraet sonst nicht mehr,
   // mit wem es verbunden war, und ein Mensch muesste jedes Mal von Hand scannen und verbinden.
#define EEBUS_PEER_FILE         "/eebus_peer.txt"
#define EEBUS_AC_FILE           "/eebus_ac.txt"   // v208: Wartezeit des Wiederaufbaus (EEBusAutoConn), uebersteht den Neustart
#define EEBUS_FS_FILE           "/eebus_fs.txt"     // v196: Geraeteliste + Einspeise-Vorgabe fuer den Failsafe
#define EEBUS_CERT_DER_SIZE     1024
#define EEBUS_KEY_DER_SIZE      256

// M3: eigene mDNS-Ankuendigung als SHIP-Dienst (damit wir bei Energiemanager bzw. Hersteller-App unter
// "Verfuegbare Geraete" auftauchen). id/type/brand/model in den TXT-Records nach SHIP 7.3.2.
#define EEBUS_ADV_PORT          4712
#define EEBUS_ADV_TYPE          "ChargingStation"   // wir simulieren einen steuerbaren Verbraucher
#define EEBUS_ADV_BRAND         "Tasmota"
#define EEBUS_ADV_MODEL         "EEBusGuard"
// EINE Kennung = Brand-Model-Serial. Dieselbe Kennung dient als mDNS-id = accessMethods.id =
// SPINE-deviceCode; eine Gegenstelle inventarisiert uns ueber deviceCode + serialNumber.
//
// ⚠ DIE SERIENNUMMER MUSS GERAETE-INDIVIDUELL SEIN. Waere sie fest im Code, meldete sich JEDES
// Geraet mit diesem Treiber unter derselben Kennung — im selben Netz koennte dann nur das erste
// gekoppelt werden, alle weiteren wuerden als dasselbe Geraet gesehen und abgewiesen. Genau dieses
// Bild ist von Serienprodukten bekannt, bei denen ab Werk eine gemeinsame Seriennummer vergeben war.
//
// Deshalb: Seriennummer = die letzten 8 Hex-Zeichen der eigenen SKI (EebusBuildIdent). Sie ist damit
// so eindeutig wie das Zertifikat und aendert sich GEMEINSAM mit ihm. Das ist wichtig, weil Kennung
// und SKI zusammenpassen muessen: eine geaenderte Kennung bei unveraenderter SKI laesst Gegenstellen
// die Verbindung sofort schliessen — nur wenn beides zugleich neu ist, entsteht sauber ein neues Geraet.
#define EEBUS_ADV_SERIAL_FB     "000000001"   // Rueckfall, solange keine SKI vorliegt
char eebus_adv_serial[12] = EEBUS_ADV_SERIAL_FB;
char eebus_adv_id[48]     = EEBUS_ADV_BRAND "-" EEBUS_ADV_MODEL "-" EEBUS_ADV_SERIAL_FB;

#ifdef USE_UFILESYS
extern FS *ffsp;   // Flash-FS, definiert in xdrv_50_filesystem.ino (kompiliert NACH uns)
extern FS *ufsp;   // aktives FS (SD-Karte wenn gemountet) — fuer den SHIP-Mitschnitt
#endif

#define D_PRFX_EEBUS            "EEBus"

// Die beiden Limit-Befehle tragen die offiziellen Use-Case-Kuerzel der Norm:
//   EEBusLpc = limitationOfPowerConsumption (Paragraf 14a EnWG, Bezugsbegrenzung)
//   EEBusLpp = limitationOfPowerProduction  (Paragraf 9 EEG,   Einspeisebegrenzung)
const char kEebusCommands[] PROGMEM = D_PRFX_EEBUS "|"   // Prefix
  "Scan|Peers|Cert|Connect|Disconnect|Status|Trust|ConnectIp|Advertise|Log|"
  "Role|Lpc|ReleaseAll|Release|Target|Probe|Provide|Open|Hems|PeerMode|DelDur|"
  "LppFrei|Lpp|Anmeld|Data|Mess|Struct|Read|AutoConn|Failsafe";   // ReleaseAll VOR Release, LppFrei VOR Lpp (Praefix-Match!)

void (* const EebusCommand[])(void) PROGMEM = {
  &CmndEebusScan, &CmndEebusPeers, &CmndEebusCert,
  &CmndEebusConnect, &CmndEebusDisconnect, &CmndEebusStatus, &CmndEebusTrust,
  &CmndEebusConnectIp, &CmndEebusAdvertise, &CmndEebusLog,
  &CmndEebusRole, &CmndEebusLimit, &CmndEebusReleaseAll, &CmndEebusRelease,
  &CmndEebusTarget, &CmndEebusProbe, &CmndEebusProvide, &CmndEebusOpen, &CmndEebusHems,
  &CmndEebusPeerMode, &CmndEebusDelDur,
  &CmndEebusLppFrei, &CmndEebusLpp, &CmndEebusAnmeld, &CmndEebusData,
  &CmndEebusMess, &CmndEebusStruct, &CmndEebusRead, &CmndEebusAutoConn, &CmndEebusFailsafe };

// Blind-Adressierung (Waermepumpen-Gateway liefert ihre Discovery nicht — VR940-Karte als Vorlage:
// LoadControl/server auf entity 3 "HeatPumpAppliance"):
//   EEBusTarget <ent> <feat>  LPC-Ziel manuell setzen (statt Ladestation-Default/Discovery)
//   EEBusTarget -1            Override aus (wieder automatisch)
//   EEBusProbe <ent> <von> <bis>  je 1 unverbindlicher LimitDescription-Read pro Sekunde an
//                             entity/feature-Bereich — JEDE Antwort (auch Fehler) zeigt, dass
//                             die Entity existiert/spricht. Ergebnis im Log/Mitschnitt.
int8_t  eebus_tgt_ent  = -1;   // -1 = kein Override
int8_t  eebus_tgt_feat = -1;
int16_t eebus_probe_ent  = -1;   // -1 = kein Sweep aktiv
uint8_t eebus_probe_feat = 0;
uint8_t eebus_probe_end  = 0;
bool    eebus_probe_bind = false; // erst Subscribe+Binding aufs Ziel-Feature, DANN Read
   // (Referenz-Umsetzung eg/lpc connected()-Reihenfolge — These: die Waermepumpen-Gateway
   // ignoriert nackte Reads von nicht gebundenen Partnern)

// SERVER-MODELL (fuer HEMS wie Energiemanager): wir stellen ein ECHTES §14a-Limit auf UNSEREM
// LoadControl-Server (ent1/feat6) bereit, das der HEMS liest/abonniert und dann an seine
// steuerbaren Verbraucher verteilt (Architektur: Netzbetreiber -> Steuerbox -> HEMS -> SteuVE).
// Anders als EEBusLpc (aktiv auf FREMDES LoadControl schreiben, Ladestation-Modell) — hier PASSIV bereitstellen.
bool     eebus_serve_active = false;   // stellen wir gerade ein Limit bereit?
uint32_t eebus_serve_watt   = 0;   // bereitgestellter Wert — bleibt 0, bis EEBusProvide ihn setzt

// Steuerbox-Rolle (Grundtor): 0 = nur Lesen (Prüfmodus, sicher),
// 1 = Steuerbox/Energy-Guard darf Limits SCHREIBEN. Auch in Rolle 1 wird NIE von selbst
// ein Limit gesendet — nur bei explizitem EEBusLpc/EEBusProvide-Befehl.
// Default 1 — das Geraet startet ab Boot als Energy Guard und kuendigt
// sich per mDNS von Anfang an als ElectricitySupplySystem/cat=1 (Steuerbox) an. Zuvor kuendigte
// es sich in der Default-Rolle 0 als ChargingStation (Verbraucher/Last) an -> der Energiemanager konnte
// uns aus dieser Boot-Phase als Last cachen (Verdacht err7-Ursache). Rolle 0 bleibt jederzeit
// per "EEBusRole 0" erreichbar (Lesemodus).
uint8_t eebus_role = 1;

// Peer-/Betriebs-Modus fuer den §14a-Write (Befehl EEBusHems <0|1>). Trennt die ZWEI Anwendungsfaelle
// sauber, damit eine Aenderung fuer den einen den anderen nicht bricht:
//   0 = SteuVE-Modus (Ladestation / Waermepumpen-Gateway) — unveraendertes, bewiesenes Verhalten: unser EBG steuert
//       den Verbraucher DIREKT, Write MIT timePeriod (Ladestation verlangt eine Dauer, loadcontrol.cpp:265),
//       bisherige Minimalsequenz (Bind -> Desc -> Ist-LimitData -> DeviceConfig ent7 -> Write).
//       Per "EEBusHems 0" jederzeit erreichbar (fuer Ladestation/Waermepumpen-Gateway-Tests).
//   1 = HEMS-Modus (Energiemanager): volle Lesesequenz vor dem Write — NodeManagement-Subscribe,
//       DeviceConfig-Beschreibung und -Werte, loadControlLimitListData mit Selektor fuer beide
//       Grenzen, measurementListData, dazu ein Heartbeat unmittelbar vor dem Write.
//       In Modus 0 bleibt das aus (kuerzere Sequenz fuer einzelne Verbrauchseinrichtungen).
//
//   BEIDE Betriebsarten senden eine Geltungsdauer: HEMS 900 s, SteuVE 3600 s, ueberschreibbar
//   als drittes Argument. Ohne Dauer schreibt nur, wer ausdruecklich 0 angibt; dann raeumt der
//   Loeschfilter zusaetzlich eine alte Endzeit weg. Der Unterschied der Betriebsarten liegt im
//   ABLAUF, nicht im Weglassen der Dauer.
//
// Default 1. Die Betriebsart wird beim Verbinden aus dem gemeldeten GERAETETYP abgeleitet
// (Energiemanager -> HEMS, alles andere -> SteuVE); der Default greift nur, wenn kein Typ
// vorliegt. Eine ausdrueckliche Zuordnung (EEBusPeerMode) hat Vorrang.
uint8_t eebus_hems_mode = 1;   // v231: Betriebsart des AKTUELLEN Slots (ESp), wird bei jedem Slotwechsel neu geladen
uint8_t eebus_hems_def  = 1;   // v231: Vorgabe (EEBusHems) fuer Slots ohne Zuordnung/Typ - frueher war das dieselbe
                               // Variable, und die Betriebsart der zuletzt verbundenen Gegenstelle galt fuer ALLE.

// CONNECT-EXPERIMENT (Laufzeit-Schalter — Varianten ohne Neu-Flash testbar):
// 0 = bisher (reaktiver, ADRESSIERTER Discovery-Read nach erstem Peer-Frame; Subscribe zuerst).
// 1 = Eroeffnungszug: sofort bei Done einen ADRESSLOSEN DetailedDiscovery-Read senden und den
//     reaktiven adressierten Read unterdruecken. Strenge HEMS antworten nachweislich (am
//     Referenz-Werkzeug verifiziert) erst auf diesen adresslosen Eroeffnungs-Read.
uint8_t eebus_open_mode = 0;

// beim §14a-Write zusaetzlich die GELTUNGSDAUER (timePeriod) des Limits LOESCHEN.
// Hintergrund (Live-Befund): der Write wird angenommen (errorNumber 0, Steuerbox gilt
// als verbunden), der Peer meldet das Limit aber mit "timePeriod endTime PT0S" zurueck und stuft die
// Begrenzung deshalb sofort wieder als DEAKTIVIERT ein. Ursache ist kein Feld in unserem Frame,
// sondern ein ALTBESTAND auf der Gegenseite: wir schreiben "partial", dabei bleibt stehen, was wir
// nicht mitsenden — und aus frueheren Laeufen (die noch ein timePeriod schickten) liegt dort eine
// Restlaufzeit von null. Die Referenz-Steuerbox hat dieses Feld nie gesetzt; bei ihr kommt das Limit
// ohne timePeriod zurueck und die Begrenzung bleibt aktiv.
// Abhilfe = derselbe Weg, den die Referenz dafuer vorsieht: ein zweiter Filter-Eintrag mit
// cmdControl "delete" + Selektor (limitId) + Element (timePeriod) VOR dem partial-Filter, im selben
// Write. Ein Loeschen eines gar nicht gesetzten Feldes ist folgenlos, der Aufruf also wiederholbar.
// 0 = aus — exakt der bewiesene Frame
// 1 = an (DEFAULT inzwischen) — Write traegt zusaetzlich den delete-Filter
// Nur HEMS: im SteuVE-Modus ist das timePeriod gewollt (die dortigen Gegenstellen verlangen eine
// Dauer), dort wird der Schalter ignoriert.
// Default 0 -> 1. Grund (live belegt ): die Altlast entsteht NICHT einmalig, sondern
// bei JEDER neuen Sitzung neu. wurde die Geltungsdauer geloescht und das Portal
// meldete "Begrenzung auf 8360 W"; nach Verbindungsabbruch und Failsafe stand im
// Ist-Zustand des Peers wieder "limitId 0 ... timePeriod endTime PT0S" — der Write wurde
// zwar angenommen (Portal: Steuerbox verbunden), die Begrenzung aber in derselben Sekunde wieder als
// deaktiviert gemeldet. Ohne den Loeschfilter waere also JEDE Sitzung von Hand nachzuziehen; das
// verhindert genau den unbeaufsichtigten Betrieb, um den es bei der Automatisierung geht.
// Das Loeschen eines nicht gesetzten Feldes ist folgenlos (die Einspeisegrenze limitId 1 hatte nie
// eine Geltungsdauer und wurde mit demselben Frame anstandslos angenommen), der neue
// Default ist damit auch fuer frische Gegenstellen unschaedlich. Abschaltbar bleibt er per
// "EEBusDelDur 0".
//
// Der Schalter wird DREIWERTIG und steht ab Werk auf AUTOMATIK (-1).
//   -1 = automatisch (DEFAULT): der Loeschfilter laeuft genau dann mit, wenn der Schreibauftrag
//        KEINE Geltungsdauer traegt. Genau das ist die Bedeutung von "Dauer 0" im Steuerungsprofil,
//        Tabelle 8: "Bei einem Wert 0 ist die Dauer nicht ueber diesen Eintrag ... begrenzt".
//        Wer unbefristet schreibt, meint unbefristet — dann darf keine alte Endzeit stehenbleiben.
//    0 = erzwungen AUS  (Pruefbetrieb: die Vererbung einer alten Geltungsdauer absichtlich zulassen)
//    1 = erzwungen AN   (Pruefbetrieb: auch dann loeschen, wenn eine Dauer mitgeht)
// Warum dreiwertig statt "Bedeutung umdrehen": die erzwungenen Stellungen werden zum Pruefen
// gebraucht, und ein Schalter, der still seine Bedeutung wechselt, macht jede aeltere Anleitung
// unlesbar.
int8_t eebus_del_dur = -1;

// Geltungsdauer eines Schreibauftrags — Grenzen und Vorgabe.
// Wertebereich aus dem Steuerungsprofil:
//   "Dauer der Leistungsbegrenzung (duration)  Integer (0..86400)" — Sekunden ab Startzeitpunkt.
// Die 7200 s, die man haeufig als "Maximum" hoert, sind etwas anderes: Tabelle 9 nennt sie als
// VORGABEWERT der "Begrenzungsdauer bei Ausfall (failsafeDuration, 300..86400)" — also die
// MINDESTdauer der Begrenzung bei Kommunikationsausfall, nicht die Hoechstdauer des Limits.
// Genau diesen Wert meldet der Energiemanager als "FsDur PT2H" zurueck.
// Die Vorgabe 900 s (15 min) ist dagegen UNSERE Wahl, keine Vorschrift: Messwerte laufen im
// Viertelstundenraster, kuerzer ergibt fuer eine Steuerbox im Betrieb keinen Sinn. Kuerzere Werte
// bleiben zulaessig — als Prueffenster (ein Testlauf mit 900 s kostet sonst jedes Mal 15 Minuten).
#define EEBUS_LPC_DUR_DEFAULT_S  900L
#define EEBUS_LPC_DUR_MAX_S      86400L

// wie viele Measurement- bzw. ElectricalConnection-Instanzen der Gegenstelle wir uns merken.
// Der Energiemanager fuehrt davon mehrere (: vier EC- und drei Measurement-Server ueber
// Netzanschlusspunkt, Speicher und Erzeugungsanlage verteilt). Vier reichen fuer eine Hausanlage.
#define EEBUS_ADR_MAX  4

// Norm-Zeitdauer <-> Sekunden. Die Gegenstelle meldet die Restlaufzeit als ISO-8601-Dauer
// zurueck und normiert dabei nach eigenem Gutduenken: aus unserem "PT120S" wird "PT2M", aus 889 s
// wird "PT14M49S". Zum Mitrechnen brauchen wir Zahlen, zum Anzeigen wieder Text.
// Bewusst schlank gehalten: nur Stunden/Minuten/Sekunden, keine Tage/Monate — laenger als 24 h
// darf eine Leistungsbegrenzung ohnehin nicht gelten (0..86400).
long EebusIsoDurSecs(const char *s) {
  if ((nullptr == s) || ('P' != s[0])) { return -1; }
  // Gegenpruefung 06.11.: auch Tage vor dem T ("P1D", "P1DT2H", ISO 8601), wie andere Energy Guards sie schreiben.
  // Bisher ergab "P1D" -1 und "P1DT0H" 0 - eine gueltige Failsafe-Mindestdauer von 24 h wurde dann abgelehnt.
  long total = 0, num = 0; bool any = false, zeit = false;
  for (const char *p = s + 1; *p; p++) {
    if ((*p >= '0') && (*p <= '9')) {
      if (num > 10000000L) { return -1; }   // unsinnig gross -> kein Ueberlauf
      num = num * 10 + (*p - '0'); any = true;
    }
    else if (!zeit && ('T' == *p)) { zeit = true; num = 0; }
    else if (!zeit && ('D' == *p)) { total += num * 86400; num = 0; }
    else if (zeit && ('H' == *p)) { total += num * 3600; num = 0; }
    else if (zeit && ('M' == *p)) { total += num * 60;   num = 0; }
    else if (zeit && ('S' == *p)) { total += num;        num = 0; }
    else { return -1; }   // unbekanntes Zeichen (auch Jahr/Monat/Woche) -> lieber nichts behaupten
  }
  return any ? total : -1;
}

void EebusSecsToIso(long s, char *out, size_t n) {
  if (s < 0) { s = 0; }
  long h = s / 3600, m = (s % 3600) / 60, sec = s % 60;
  if (h)      { snprintf(out, n, "PT%ldH%ldM%ldS", h, m, sec); }
  else if (m) { snprintf(out, n, "PT%ldM%ldS", m, sec); }
  else        { snprintf(out, n, "PT%ldS", sec); }
}

// ANMELDUNG NACH DEM VERBINDEN SELBSTTAETIG ABSCHLIESSEN.
// Die Verbindung allein genuegt der Gegenstelle nicht: nach [LPC-902]/[LPP-902] bleibt ein
// Controllable System im Zustand "init", bis es einen Heartbeat UND ein darauffolgendes erstes,
// akzeptiertes Limit-Kommando erhalten hat. Solange fuehrt es uns nicht als Steuerbox — im Portal
// steht "Steuerbox nicht verbunden", obwohl SHIP steht.
// Es muss aber KEIN begrenzendes Limit sein: [LPC-905]/[LPP-905] — "empfaengt das CS im init ein
// DEAKTIVIERTES Limit, wechselt es nach unlimited/controlled". Eine blosse Freigabe meldet uns also
// an, ohne dass irgendetwas gedrosselt wird. live bestaetigt: nur "Freigeben" fuer beide
// Richtungen geklickt -> das Portal meldete beide Zeilen als verbunden, die Anlage blieb frei.
// Das ist der richtige Ruhezustand einer Steuerbox: angemeldet, ueberwacht, unbeschraenkt — und wenn
// der Netzbetreiber etwas vorgibt, wird nur noch der Wert geschrieben, ohne Neuverbinden.
// 1 = Default (nur HEMS): nach dem Onboarding je eine Freigabe fuer consume und produce.
// 0 = aus: nichts wird von selbst geschrieben (Verhalten wie frueher).
// Bewusst NUR im HEMS-Modus: an einer einzelnen SteuVE (Wallbox, Waermepumpe) wuerde eine
// automatische Freigabe ein dort gesetztes Limit AUFHEBEN — das darf nur auf Befehl geschehen.
uint8_t eebus_auto_reg = 1;

// Abstand der WebSocket-Pings. Die Referenz-Steuerbox pingt alle 50 s und rechnet mit 60 s
// Pong-Frist; wir haben bisher gar nicht gepingt. Ein stiller Kanal wird von Gegenstellen und von
// Netzwerkkomponenten irgendwann abgeraeumt — genau das passierte.
#define EEBUS_WS_PING_MS   50000

// v175 (07.10.): Herzschlag im HEMS-Betrieb wieder wie die echte Steuerbox — heartbeatTimeout PT10S
// (siehe EebusSpineSendHeartbeat), Sendetakt 4 s statt 8 s, damit die Reserve nicht wie im Juli nur
// eine Sekunde betraegt. Anlass: seit dem HEMS-Neustart am 05.10. lehnt das HEMS nach jedem neuen
// Verbindungsaufbau jeden Schreibbefehl mit err7 ab. Vergleich der Mitschnitte: am 27.07. (PT10S)
// nahm er nach einem Wiederverbinden ohne Kopplung sofort an, heute (PT60S) nicht.
#define EEBUS_HEMS_HB_MS    4000

// v176 (07.10.): eine abgelehnte Anmeldung (Freigabe §14a/§9) wird wiederholt, statt nach den vier
// Versuchen in ~15 s liegen zu bleiben. Anlass: nach einem Neustart des HEMS ist sein Steuerbox-Teil erst
// ~3 min nach dem Verbindungsaufbau bereit (07.10.: Gateway 13:37:03, Steuerbox-Teil 13:40:19). Die Box
// meldete sich um 13:37:41 an, bekam err7 und versuchte es nie wieder; eine Freigabe von Hand um 13:43
// ging sofort durch. Jetzt: alle 60 s erneut, hoechstens 10-mal (10 min), nur solange keine eigene
// Grenze aktiv ist (die Anmeldung ist eine Freigabe und wuerde sie aufheben).
#define EEBUS_REG_RETRY_MS     60000
#define EEBUS_REG_RETRY_MAX    10

// v177 (08.10.): Telegramm-Zeilen ("EBG: SHIP TX ...") nicht mehr in die Antwort auf /cm mischen.
// Tasmota setzt die Antwort eines Web-Befehls aus ALLEN Log-Zeilen zusammen, die waehrend des Befehls
// entstehen und eine '{' enthalten (HandleHttpCommand). EEBusLpc/EEBusLpp/EEBusRelease senden sofort
// das erste Telegramm; dessen Stuecke landeten vor der eigentlichen Antwort und machten sie zu
// ungueltigem JSON (gemessen 08.10. 18:14: 457 Zeichen, Telegramm-Anfang + "EEBusLpc":{"Started":true,...
// abgeschnitten). Jetzt werden diese Zeilen waehrend eines Web-Befehls zurueckgehalten und im naechsten
// 100-ms-Takt unveraendert ins Log geschrieben. Konsole und Mitschnitte zeigen weiter alles.
#define EEBUS_DEFLOG_SZ        4096

// v178 (08.10.): Nutzerbefehl (EEBusLpc/EEBusLpp/EEBusRelease) bricht ein laufendes oder haengen-
// gebliebenes Onboarding ab, statt von ihm als "Onboarding-Ende" verschluckt zu werden (EebusLpcStart).
// Ausserdem entfaellt die ERSTE automatische Anmeldung, wenn schon eine Grenze steht — wie bisher nur
// bei deren Wiederholung (v176); sonst hob die Anmeldung (= Freigabe) eine frisch befohlene Grenze auf.

// v179 (08.10.): nach endgueltigem err7 ("Write abgelehnt") im HEMS-Betrieb die Grenzen spaeter noch
// dreimal nachlesen. Das HEMS hat am 06.–08.10. abgelehnte Writes 70 s bis 6:43 min spaeter doch umgesetzt,
// ohne es der Steuerbox zu melden; die Anzeige stand dann falsch auf "Write abgelehnt". Stimmt der
// nachgelesene Zustand mit dem Auftrag ueberein (aktiv/inaktiv und bei aktiv auch der Wert), wird das
// Ergebnis zu "BESTAETIGT ... (verspaetet nach err7)". Ein neuer Auftrag bricht das Nachlesen ab.
static const uint16_t kEebusLateReadS[3] = { 60, 180, 420 };

// v180 (08.10.): Wiederaufbau mit neuem Scan, wenn die Gegenstelle nicht mehr unter der gemerkten IP
// erreichbar ist (DHCP-Wechsel). Nach so vielen gescheiterten Versuchen wird neu gesucht.
#define EEBUS_KA_SCAN_AFTER    3

// v181 (08.10.): Wiederaufbau-Partner wird nur noch gemerkt, wenn der bisher gemerkte nicht mehr
// verbunden ist (siehe SHIP-Handshake KOMPLETT). Zusaetzliche Test-Verbindungen aendern ihn nicht.

// v182 (08.10.): Kommt so lange GAR NICHTS von der Gegenstelle, gilt die Verbindung als tot und wird
// neu aufgebaut. Ein Energiemanager meldet etwa jede Sekunde Messwerte und spaetestens alle 2 min einen
// Herzschlag (PT2M) — 3 min Stille sind kein Zufall mehr.
#define EEBUS_SILENT_S         180

// v226 (05.11.): ausgehende Verbindungen senden nicht mehr blockierend (tls_mini setSendWaitMs). Nimmt der Socket
// so lange kein einziges Byte, gilt das Senden als gescheitert (wie der Server-Weg seit v204/v205: 3 s).
#define EEBUS_SEND_WAIT_MS     3000
#define EEBUS_STOP_WAIT_MS     100    // beim Abbau: close_notify/Rest nur noch kurz versuchen

// v183 (08.10.): Wartezeit des Wiederaufbaus waechst mit jedem gescheiterten Versuch, statt starr alle
// 8 s neu zu verbinden. Jeder Versuch baut TLS im Hauptablauf auf und blockiert ihn kurz; bei einer
// dauerhaft fehlenden Gegenstelle lief die Box so zaeh (06.10.: LoadAvg 2791, eine Minute ohne Logzeile).
// Index = Versuche seit der letzten stehenden Verbindung (ka_try + 3 je Scan), letzter Wert bleibt.
// Obergrenze 2 min: kommt die Gegenstelle zurueck, ist die Box spaetestens
// 2 min danach wieder verbunden.
// v229 (05.11.): ersetzt. Die Stufen bis 120 s liessen eine Gegenstelle nach kurzem Ausfall in den Failsafe
// laufen (Kabeltest 16:23-16:25: Kabel 81 s weg, danach noch 26 s bis zur Verbindung -> .22 Failsafe nach 120 s).
// Jetzt: in den ersten EEBUS_KA_FAST_PHASE_MS nach dem Verlust alle EEBUS_KA_FAST_MS nachsehen, danach alle
// EEBUS_KA_SLOW_MS. "Nachsehen" ist ein Ping (laeuft in eigener Task, blockiert nichts); verbunden wird nur,
// wenn er antwortet - oder spaetestens alle EEBUS_KA_TRY_MS trotzdem (Geraete, die Pings sperren). Damit
// blockiert ein Ausfall die Box nicht mehr mit TCP-Versuchen (der Grund fuer v183 bleibt erfuellt).
// Regel: SHIP 1.1.0 Kap. 5 Z. 384-386 ("SHALL cyclically retry"), Kap. 6 Z. 480-485 (Wiederverbinden "at any
// time", "without any delay"), Kap. 8.2 Z. 640-656 (vor dem TCP-Versuch "SHOULD be detected as online", Ping
// "MAY"; ohne Ping-Antwort nicht als offline werten, wenn mDNS kommt), Kap. 8.3 Z. 668-671 (SYN zweimal
// wiederholen -> TCP-Wartezeit bleibt unveraendert). LPC [LPC-911]/[LPC-912] (Failsafe nach 120 s),
// [LPC-913]; VDE-FNN-Empfehlung Tenorziffer 2a BK6-22-300 (Verbindung zeitnah wieder aufbauen).
#define EEBUS_KA_FAST_MS        5000UL     // Nachsehen in der ersten Phase
#define EEBUS_KA_FAST_PHASE_MS  300000UL   // erste Phase: 5 min nach dem Verlust
#define EEBUS_KA_SLOW_MS        30000UL    // danach
#define EEBUS_KA_TRY_MS         30000UL    // ohne Ping-Antwort trotzdem spaetestens so oft verbinden
#define EEBUS_KA_TRY_PING_MS    120000UL   // v230: dito, wenn die Gegenstelle sonst auf Ping antwortet (spart Blockieren)
#define EEBUS_TCP_CONN_MS       3000UL     // v231: TCP-Verbindungsaufbau beim eigenen Anruf (vorher 10 s Stream-Frist)
#define EEBUS_TLS_LOOP_MS       5000UL     // v231: Obergrenze je TLS-Lauf (Handshake), tls_mini setLoopTimeout, vorher fest 10 s
#define EEBUS_HS_WAIT_MS        5000UL     // v231: Warten auf WS-Upgrade-Antwort und CMI (vorher je 8 s)
#define EEBUS_LIFE_PING_MS      3000UL     // v231: Lebenszeichen-Ping bei stehender Verbindung (SHIP Kap. 8.2)
#define EEBUS_LIFE_LOST_MS      15000UL    // v231: so lange weder Ping-Antwort noch Nachricht -> Verbindung abbauen (seit v233 ungenutzt)
#define EEBUS_LIFE_SILENT_MS    25000UL    // v233: Gegenstelle mit Herzschlag so lange ganz still -> Verbindung abbauen (ersetzt den Dauer-Ping)
#define EEBUS_KA_SCAN_MIN_MS    60000UL    // neue mDNS-Suche hoechstens so oft (laeuft nebenher)

// v233 (07.11. nachmittags, meierchen006): KEIN Lebenszeichen-Ping mehr bei stehender Verbindung. Box v231/v232 stuerzte im
// Testnetz am 07.11. viermal ab (DebugException in vTaskSwitchContext, EPC 4037e9bd - dieselbe Adresse wie der bewiesene
// Ping-Absturz von v229), Box v224 im Hauptnetz (ohne diesen Ping) lief zur selben Zeit 2 Tage 16 h ohne Absturz. Der
// v231-Ping legte alle 3 s eine esp_ping-Task an und loeschte sie wieder. Ersatz ohne Task: bei einer Gegenstelle, die
// Herzschlaege schickt, gilt die Verbindung nach EEBUS_LIFE_SILENT_MS (25 s) ohne jede Nachricht als tot. Der Ping VOR
// dem Wiederaufbau (v229/v230) bleibt. Gegenprobe: Laufzeit ohne Absturz im Testnetz.
// v232 (07.11., Test B2 Schritt 5 mit Box v231 / HEMS v015): Freigaben schicken den letzten eigenen Wert der Richtung
// mit (lpc_val_dir[d], wie EEBusRelease seit 06.11.), nicht mehr fest 0 W - EEBusReleaseAll, die Sicherung, die
// Anmeldung und ihre Wiederholung (ganze Fehlerklasse, gleich im HEMS 129 v034 / Firmware v016). Nach einem Neustart
// ist der Wert 0, dann bleibt alles wie bisher. Die Freigabe wirkte auch vorher (isLimitActive false); nur der Wert
// wich von der Vergleichs-Steuerbox ab (B2: "jede Richtung mit IHREM Wert inaktiv", ReleaseAll lieferte Lim0 inaktiv 0
// statt 8160). Ausserdem liest EEBusLppFrei die Zahl streng (Eebus126ArgNum) wie EEBusLpp - bisher atol, "abc" = 0 W.
// v231 (05.11./06.11.): Sammelrunde nach der Pruefung aller Treiber vom 06.11. (Befundliste 2026-11-06--1245, Nummern
// in Klammern). Eine Version fuer alles, keine Einzelschritte.
//  Nachtrag 06.11. abends (meierchen006): EEBusStatus meldet je Platz zusaetzlich "HerzschlagGesendetS" (Alter unseres
//  letzten Herzschlags an die Gegenstelle, hb_tx_at); die Statuszeile von steuerbox.html zeigt "gesendet · empfangen".
//  Fehlerklassen komplett: (6) ALLE SKI-Vergleiche ohne Gross-/Kleinschreibung (10 Stellen, u. a. EebusKaPeerZurueck,
//   Wiederaufbau, "schon verbunden"); (4.12) jeder Timer "millis() + T" ueber Eebus126At (0 = aus kann nicht mehr
//   entstehen); (2.1) WS-Laengenpruefung ohne Ueberlauf (len > max - total).
//  Speicher: (2.2) scale aus dem Frame begrenzt (Schleife ohne Ende); (2.3) EEBusMess haengt Text puffersicher an.
//  Norm: (3.8) heartbeatCounter nur bei notify, nie bei reply (LPC Tab. 17 [LPC-031]); (3.9) connectionClose "announce"
//   wird mit "confirm" beantwortet, eigener Grund "unspecific" statt "unplug" (SHIP 13.4.8.1.1/13.4.8.1.2);
//   (4.11) ohne gestellte Uhr Herzschlag ohne Zeitstempel statt 1970 (LPC Tab. 17 "R timestamp"); (3.10) PT10S bleibt
//   bewusst (Kommentar an der Stelle).
//  Verbindungen: (4.6) Betriebsart HEMS/SteuVE je Verbindung statt global (Eebus126ModeLoad bei jedem Slotwechsel;
//   EEBusHems ist nur noch die Vorgabe); (4.5) ein Slot mit gescheitertem Handshake kommt wieder in den Wiederaufbau;
//   (4.7) eigener Anruf ohne EC-Schluessel im Zertifikat wird nicht mehr verbunden; (4.8) Write-Quittungen werden ueber
//   msgCounterReference zugeordnet; (4.9) Empfang mitten im Frame abgebrochen -> Abbau statt Strom aus dem Takt;
//   (4.10) der Server-Weg setzt zerstueckelte WS-Nachrichten zusammen; (4.13) fertige Ping-Sitzungen werden auch auf
//   verbundenen/uebernommenen Slots geloescht; (4.15) haengt der Sendeweg (sendStuck), gilt ein Frame nicht als gesendet.
//  Blockieren (1.10): TCP-Aufbau beim eigenen Anruf hoechstens 3 s (vorher 10 s), WS-Upgrade/CMI je 5 s (vorher 8 s),
//   Server-Frames in EINEM Stueck (vorher Kopf und Nutzlast mit je bis zu 3 s).
//  Neu: Lebenszeichen per Ping bei stehender ausgehender Verbindung (SHIP Kap. 8.2, wie HEMS v012): alle 3 s ein Ping;
//   hat die Gegenstelle einmal geantwortet und kommt 15 s lang weder Ping-Antwort noch Nachricht, wird abgebaut und der
//   Wiederaufbau (v229/v230) uebernimmt. Bisher erst nach 180 s Stille.
//  Befehle (5): EEBusLpc/EEBusLpp/EEBusRelease lesen Zahlen streng ("abc" sendete 0 W), die Dauer wird erst geprueft und
//   dann gemerkt; EEBusAutoConn "abc" schreibt keine 0 mehr in den Flash; EEBusFailsafe weist nan/inf und zu grosse
//   Geraetewerte ab, die Datei wird beim Laden geprueft; EEBusConnectIp haengt an die Fundliste an (statt peers[0] zu
//   ueberschreiben), kann IPv6 und uebernimmt die SKI aus dem Zertifikat.
//  Kleinigkeiten (7): Logtexte (Herzschlag-Takt, "State gesendet", ausgehandelte TLS-Suite), Kopfkommentar zur
//   Identitaet richtiggestellt, Herstellernamen in Kommentaren neutral.
// v230 (05.11.): Absturz aus v229 behoben. Die Ping-Sitzung loeschte sich in ihrer eigenen Rueckruffunktion
// (esp_ping_delete_session -> vTaskDelete auf die eigene Task, kehrt nie zurueck, Socket und Speicher blieben liegen);
// Kabeltest 20:24-20:28 (3 min, rund 36 Pings ohne Antwort) endete 20:28:04 mit DebugException in vTaskSwitchContext
// (EPC 4037e9bd). Jetzt meldet die Sitzung nur "fertig", geloescht wird sie im Tasmota-Ablauf (EebusKaPingAufraeumen).
// Ausserdem: hat die Gegenstelle schon einmal auf Ping geantwortet, gibt es den Versuch ohne Ping-Antwort nur noch alle
// 120 s statt alle 30 s (jeder blockiert ca. 3 s; Kabeltest v229 Zug 2/3). SHIP Kap. 5 "cyclically retry" bleibt erfuellt.
// v229 (05.11.): Wiederaufbau schneller und ohne Blockieren. Kabeltest v228 16:23-16:25: Kabel 81 s weg, danach
// brauchte die Box noch 26 s (Stufen 8/15/30/60/120 s, nach 3 Fehlversuchen Suche mit 60 s Pause) -> das HEMS ging
// nach 120 s ohne Herzschlag in den Failsafe. Jetzt: (1) in den ersten 5 min nach dem Verlust alle 5 s nachsehen,
// danach alle 30 s; (2) die mDNS-Suche laeuft nur nebenher (hoechstens alle 60 s), liefert sie die Gegenstelle nicht,
// gilt die zuletzt benutzte Adresse weiter; (3) vor dem Verbinden ein Ping (ESP-IDF esp_ping, eigene Task): verbunden
// wird, sobald er antwortet, sonst spaetestens alle 30 s (Geraete, die Pings sperren). Die TCP-Wartezeit bleibt
// unveraendert (SHIP Kap. 8.3: SYN zweimal wiederholen). Erwartet: Verbindung ca. 5 s nach Rueckkehr der Gegenstelle;
// ein Failsafe am Geraet nur noch bei Ausfaellen ueber ca. 110 s. Regeln: siehe Kopf bei EEBUS_KA_FAST_MS.
// v228 (05.11., v227 war ein Neubau der v226-Quellen): Kabeltest 15:52-15:54 mit v226 - die Box stand noch rund 15 s
// (15:53:26-15:53:41), weil die 3-s-Wartezeit der TLS-Schicht bei JEDEM Aufruf neu begann (write, flush und die
// Empfangsabfrage available() in jedem Durchlauf; deren Fehlschlag meldet niemand). Jetzt zaehlt tls_mini die Wartezeit
// ueber alle Aufrufe hinweg; ist sie einmal abgelaufen, kehrt jeder weitere Aufruf sofort mit Fehler zurueck (sendStuck),
// und available() wartet nie auf den Socket. Der Treiber prueft sendStuck() im 100-ms-Poll und baut sofort ab und wieder
// auf. Erwartet: hoechstens einmal etwa 3 s Stillstand. Regel unveraendert: [LPC-031]/[LPC-005] (Herzschlag in jeder
// heartbeatTimeout-Periode, Zusage PT10S, gegenueber allen anderen Gegenstellen), [LPC-913] (Herzschlag + Grenze binnen
// 60 s nach Wiederkehr), VDE-FNN-Empfehlung Tenorziffer 2a BK6-22-300 (Stoerung protokollieren, zeitnah wieder aufbauen).
// v226 (05.11.): Die Box stand bis zu 30 s komplett (Web, Log, alle Verbindungen), wenn eine Gegenstelle der ausgehenden
// Verbindung ploetzlich wegfiel (Kabeltest 05.11.: 12:26:27-12:26:49 und 12:28:44-12:29:10, ICMP antwortete durchgehend).
// Ursache: der Client-Sendeweg ging ueber NetworkClient::write, das je Aufruf bis 10 x 1 s auf einen schreibbaren Socket
// wartet. Jetzt sendet die TLS-Schicht mit MSG_DONTWAIT (tls_mini setSendWaitMs) und gibt nach 3 s ohne ein einziges
// gesendetes Byte auf, wie der Server-Weg seit v204/v205; beim Abbau wird nur noch 100 ms versucht. Regel: [LPC-031]/[LPC-005]
// (heartbeatData "SHALL be notified at least each heartbeatTimeout period", die Box sagt PT10S zu) gegenueber allen anderen
// Gegenstellen; VDE-FNN-Empfehlung Tenorziffer 2a, Verbindungsueberwachung. Dazu: geht eine Vorgabe nicht raus, steht im
// Log und im Betriebslogbuch "nicht ausfuehrbar - Senden abgebrochen" statt "Limit ... geschrieben" (FNN: Nichtausfuehrbarkeit
// protokollieren). Inhalt und Reihenfolge der Nachrichten bleiben gleich.
// v225 (04.11.): drei Luecken aus der Normpruefung. (1) Ein Herzschlag-Ausfall bei stehender Verbindung und seine Rueckkehr stehen
// jetzt im Betriebslogbuch (VDE-FNN-Empfehlung Tenorziffer 2a, Verbindungsueberwachung: Kommunikationsstoerung protokollieren).
// (2) Kommt nur der Herzschlag zurueck, ohne neue Verbindung, schreibt die Box je Richtung eine Vorgabe: laufende Grenze mit
// Restzeit, sonst Freigabe ([LPC-913] Z. 369-371, [LPP-913] Z. 386-388). Bisher kam dann keine Vorgabe. (3) Scheitert eine Vorgabe
// ohne Antwort oder ohne Bestaetigung, wird sie wie eine abgelehnte (v214) nach 60 s wiederholt. Eine nicht bestaetigte oder
// abgelehnte FREIGABE bleibt dabei Freigabe: bisher haette die Wiederholung die noch gelesene Grenze neu geschrieben.
// v224 (04.11.): Die Pruefung der Anmeldung lief je Durchgang nur fuer EINE Richtung; im selben Durchgang wurde die
// naechste Richtung ungeprueft geschrieben (Test v223 18:50:37.985: Bezug entfaellt, in derselben ms Einspeise-Freigabe,
// 5000 W weg). Jetzt Schleife ueber alle faelligen Richtungen. Das war die eigentliche Ursache; die Vermutung im Kopf
// v223 (limitId 2 ungelesen) trifft auf das HEMS nicht zu, es liefert immer beide Grenzen. Das Zusatz-Lesen bleibt fuer
// Geraete, die den Selektor beachten. Regeln wie v223.
// v223 (04.11.): nach einem Neustart der Box kennt sie ihre Grenzen nur aus der Anlage. (1) Sie liest jetzt auch die in der
// Beschreibung genannten limitId > 1 (Test v222 17:23:40: HEMS Bezug 1, Einspeisung 2; 2 wurde nie gelesen, die Einspeise-
// grenze 5000 W wurde freigegeben). (2) Die Sicherung gibt eine aktiv gelesene Grenze ohne Merker nicht mehr frei. Regel:
// LPC Z. 379-380 / LPP entsprechend (deaktivieren nur ohne Bedarf); BK6-22-300 Anl. 1 Ziff. 4.3 Satz 1.
// Lesen 0 und 1 bleibt wie bisher (Vergleichs-Steuerbox), die zusaetzlichen Reads nur bei hoeheren Nummern.
// v222 (04.11.): die Anmeldung entfaellt nur noch je Richtung. Bisher strich eine aktive Grenze in EINER Richtung die
// Anmeldung fuer BEIDE (Test v221 16:00:25: Bezug 6000 W erneut, Einspeisung danach ohne Vorgabe, auch eine laufende
// Einspeisegrenze waere verloren). Regeln: [LPC-913] Z. 369-371 und [LPP-913] Z. 386-388 gelten je Anwendungsfall.
// Eine fremde aktive Grenze ohne eigenen Merker wird weiter nicht aufgehoben (LPC Z. 379-380). Wiederholung genauso.
// v221 (04.11.): eine noch gueltige eigene Grenze ueberlebt die Wiederverbindung. Bisher schrieb die Anmeldung nach
// jedem Wiederverbinden eine Freigabe und hob damit auch eine laufende Grenze auf (Test 04.11. 15:27: 6000 W mit
// 29 min Rest, HEMS neu gestartet, 25 s spaeter frei). Jetzt schreibt die Anmeldung je Richtung die gemerkte Grenze
// mit Restdauer; nur ohne gueltige Grenze die Freigabe. Regeln: LPC [LPC-913] Z. 369-371 (nach Wiederkehr Herzschlag
// + Grenze binnen 60 s), Z. 379-380 (deaktivieren nur ohne Bedarf), [LPC-904]; BK6-22-300 Anl. 1 Ziff. 4.3 Satz 1.
// Bleibt die Anmeldung, statt sie auszulassen: die Grenze muss vor den Failsafe-Werten kommen (LPC Z. 372-375).
// Der Merker gilt je SKI ueber alle Slots (EebusEigeneGrenze); in anderen Slots wird er beim Schreiben geloescht,
// damit eine aufgehobene Grenze nicht aus einem alten Slot wieder auflebt. Nach Neustart der Box ist er weg (nur RAM).
// v217 (28.10., gemu 27.10. §3.1): ohne SD-Karte wird das Betriebslogbuch nicht mehr STILL ausgelassen - einmal je Start
// Logzeile "EBG: Betriebslogbuch aus - keine SD-Karte", in EEBusStatus das Feld "Betriebslogbuch" (aktiv / aus - keine SD-Karte).
// Grund: die VDE-FNN-Empfehlung verlangt das Protokoll als Nachweis; fehlt es, muss man das sehen.
// v216 (26.10.): Betriebslogbuch - Zeitstempel ohne doppeltes "Z" (Tasmota haengt bei UTC selbst Z an);
// Failsafe-Uebertragung als eigenes Ereignis "Failsafe-Werte" mit Wert und Dauer (vorher als "Vorgabe Freigabe").

// v215 (26.10.): Betriebslogbuch auf der SD (/betriebslogbuch/JJJJ-MM.csv, 30 Monate) nach VDE FNN Empfehlung zu
// Tenorziffer 2a BK6-22-300 Kap. 4.1.2.2 und S. 25: Vorgaben mit (Nicht-)Bestaetigung, Verbindung hergestellt /
// Kommunikationsstoerung, abgewiesene Steuerbefehle. Steuerbefehle nur noch an STEHENDE Verbindungen (Index-Weg
// nahm vorher auch einen Platz im Wiederaufbau an).

// v214 (26.10., meierchen006: "das muss alles so funktionieren"): eine ABGELEHNTE Vorgabe gilt nicht als geschrieben;
// die Sicherung wird nach 60 s wiederholt (eigene Grenze erneut, sonst Freigabe), bis die Gegenstelle annimmt.
// Vorher blieb eine Anlage, die uns beim Verbinden noch nicht vertraute, nach dem Vertrauen dauerhaft "autonom".

// v213 (26.10.): Wiederaufbau nach Neustart - ein Partner, der im Scan fehlt (z. B. ersetzte Identitaet), haelt die
// anderen nicht mehr auf: gefundene Partner kommen sofort vor den fehlenden (vorher 4 Versuche, rund 5 min Failsafe
// beim echten HEMS). Neu: EEBusAutoConn -<SKI> nimmt genau einen Partner aus der Liste (-1 weiterhin alle).

// v212 (26.10.): mDNS kuendigt auch die IPv4 an (mdns_netif_action ENABLE/ANNOUNCE_IP4 nach mdns_init). Bisher nur
// AAAA, weil mdns_init erst nach dem Ereignis "IPv4 bekommen" lief; Geraete ohne IPv6 konnten uns nicht anrufen.

// v211 (26.10., Normpruefung Teil B, meierchen006): ein DIREKT gesteuertes Geraet wird immer genau auf seine
// Mindestleistung gedimmt - 4200 W (BK6-22-300 Anl. 1 Ziff. 4.5.1), WP/Klima ueber 11 kW 0,4 x P (Satz 2); sie ist
// Untergrenze und Dimmwert zugleich (FNN-Empfehlung: "Dimmen (zugesicherte Mindestbezugsleistung § 14a)"). Andere
// Eingaben werden ersetzt und geloggt. Failsafe direkt = derselbe Wert. Log-Text Failsafe korrigiert.
// v209 (26.10., Normpruefung Teil C): die Failsafe-Werte gehen jetzt in BEIDEN Wegen an die Anlage - auch nach der
// Sicherung (bisher nur nach der Anmeldung: dort war lpc_onboarded gesetzt, beim Wiederaufbau nicht). VDE FNN
// Empfehlung zu Tenorziffer 2a BK6-22-300 Kap. 4.1.2.5: "bei bestehender Kommunikationsverbindung [werden] die
// FailSafe-Leistungsgrenzen (Bezug und Einspeisung) und deren Dauer an die steuerbare Einrichtung uebertragen".
// Wert (meierchen006): direkt gesteuert 4200 W je SteuVE (BK6 Ziff. 4.5.1), hinter dem HEMS Pmin der Anlage (Ziff. 4.5.2).
// Dazu: Betriebsart (HEMS/direkt) auch aus der Discovery der Gegenstelle, wenn sie (noch) nicht in der Fundliste steht.
// v208 (26.10., Normpruefung Teil A): FNN-Empfehlung zu Tenorziffer 2a BK6-22-300 (Verbindungsueberwachung): die
// Steuerungseinrichtung muss "einen zeitnahen Wiederaufbau der Kommunikationsverbindung veranlassen". Wartezeit nach
// Netz+Uhr 30 s statt 120 s (Test 26.10. 11:24: Anruf nach 30 s ohne "cmi resp n=-1", keine SteuVE im Failsafe;
// vorher 91 s Failsafe). Der Wert aus EEBusAutoConn steht jetzt im Flash (/eebus_ac.txt). Sicherung an DIREKT
// gesteuerte Geraete nach 5 s statt 30 s (dort gibt es keinen Anmelde-Weg); beim HEMS bleiben 30 s.
// v207 (26.10., Normpruefung Teil D): auch die automatische ANMELDUNG nach dem Onboarding schickt keinen Paragraf 9
// mehr an Geraete ohne Einspeisegrenze (Test 26.10. 10:13:50 .188.23: "FEHLER: keine produce-Grenze (EEG9)").
// Die Korrektur aus v203 galt bisher nur fuer Sicherung und "alle". LPP gilt nur fuer Geraete mit produce-Grenze.
// v206 (26.10.): Test v205 (10:01:28): der Server konnte nicht einmal 4 B senden, weil die SteuVE ihrerseits in ihren
// 4444-B-Discovery (mehrere TLS-Records) hing; v204/v205 lasen waehrend des Wartens nur EINEN Record (danach wartet die
// Engine, bis die App-Daten abgeholt sind). Jetzt landen die App-Daten waehrend des Wartens in einem Zwischenspeicher
// (ESrv.hold) und gehen in der naechsten Runde in den Parser - die Engine nimmt beliebig viele Records an.
// v205 (26.10.): Test v204 bewies die Ursache (.188.22 09:50:56: "Sendepuffer voll - warte" und "Senden nach 3 s
// abgebrochen" in DERSELBEN Millisekunde): die 3 s vergehen in NetworkClient::write() (wartet bis 10 x 1 s, will
// alles loswerden), nicht in unserer Schleife. Jetzt sendet der Server-Pfad nicht blockierend (EebusSrvSockSend,
// send MSG_DONTWAIT) - nur so viel, wie der Socket nimmt; dazwischen wird gelesen.
// v204 (26.10.): Ruft eine SteuVE die Steuerbox an (Steuerbox = SHIP-Server), kam keine Vorgabe an (Test 25.10.
// 22:05, .188.21): beide Seiten schickten gleichzeitig mehrere KB (Discovery), der Server-Sendepfad
// (EebusSrvAppWrite) wartete bis 3 s auf Platz im Socket OHNE zu lesen, gab dann STUMM auf und hinterliess einen
// halben WS-Frame -> Datenstrom zerstoert, nach 60 s Abbruch, [LPC-913] (Vorgabe binnen 60 s) verfehlt.
// Jetzt: waehrend des Wartens eingehende TLS-Records lesen; Warten und Abbruch werden geloggt; nach einem Abbruch
// wird die Verbindung sauber beendet (die Gegenstelle baut neu auf). Am Inhalt/der Reihenfolge der Nachrichten
// aendert sich nichts.
// v203 (25.10., Normpruefung meierchen006 "regelkonform"): Sicherung nach 30 s statt 60 s. LPC V1.0.0 [LPC-913]
// (Z. 369-371): "After restoration of communication, the Energy Guard SHALL send a Heartbeat and a following power
// limit within 60 seconds"; die Freigabe ist dabei ausdruecklich zulaessig ([LPC-916]/[LPC-920], Z. 376-386: "MAY also
// deactivate ... if there is no need for setting a limit" -> CS wechselt nach "unlimited/controlled").
// v203 (25.10.): Geraete OHNE Einspeisegrenze (SteuVE: Beschreibung nennt nur consume) bekommen keine Einspeise-
// Vorgabe mehr - weder von der Sicherung noch ueber "alle". Bisher endete die Sicherung dort mit "keine produce-Grenze
// (EEG9)" und ueberschrieb das gute Ergebnis der Bezugs-Freigabe. Erkannt an der gelesenen Beschreibung: Bezug-limitId
// bekannt, Einspeise-limitId nicht (EebusOhneEinspeisung).
// v202 (25.10., meierchen006): Wiederaufbau zu MEHREREN Partnern. Bisher merkte sich die Steuerbox genau EINE SKI
// (/eebus_peer.txt) - bei Direktsteuerung von zwei SteuVE kam nach einem Neustart nur eine zurueck (Test 20:43).
// Jetzt eine Liste mit bis zu EEBUS_AC_MAX SKIs (eine je Zeile, alte Datei mit einer SKI bleibt lesbar). Jede
// Verbindung, die die Datenphase erreicht (ausgehend, oder eingehend mit Zertifikat), kommt dazu - unbekannte Geraete
// werden NIE selbst angerufen, die Liste fuellt sich nur durch bewusstes Verbinden. Nach dem Start: ein Scan, dann
// nacheinander zu jedem gemerkten Partner, der nicht verbunden ist (je Partner bis zu EEBUS_AC_MAX_TRY Versuche).
// EEBusAutoConn zeigt die Liste, EEBusAutoConn -1 loescht sie.
// v201 (25.10., Test v200 an der Test-Steuerbox mit zwei DIREKT gesteuerten SteuVE): die Sicherung griff nie, weil
// sie auf das LPC-Onboarding wartete - das gibt es nur beim Energiemanager; eine direkt gesteuerte SteuVE wird erst
// beim ersten Write gebunden. Folge: keine Vorgabe, kein Herzschlag, SteuVE "autonom", und nach 180 s ohne Nachricht
// baute die Steuerbox die Verbindung als "stumm" ab und neu auf (alle 3 min). Jetzt startet die Sicherung 60 s nach
// Datenphase + Discovery der Gegenstelle (lpc_disco), unabhaengig vom Onboarding; EebusLpcStart bindet selbst.
// v200 (25.10., Vorfall 19:57 nach dem v199-Flash): das HEMS ging bei der Wiederverbindung in den Failsafe und blieb
// dort, weil die Steuerbox nach dem Onboarding KEINE Freigabe schrieb (die Anmeldung fehlte im Ablauf; Ursache ohne
// Log nicht mehr beweisbar, beim Neustart 20:08 trat es nicht auf). Ein Geraet im Failsafe verlaesst ihn nach LPC erst
// durch einen Write des Energy Guard - bis dahin galt der Failsafe bis zur Mindestdauer (2 h). Neu: Sicherungs-
// Sicherung je Verbindung. 60 s nach dem Onboarding prueft der Treiber, ob in DIESER Sitzung schon eine Vorgabe
// geschrieben wurde (Freigabe oder Grenze, ueber EebusLpcStart; die Failsafe-Werte zaehlen nicht). Wenn nicht, je
// Richtung (Bezug, Einspeisung): laeuft eine EIGENE Grenze fuer diese SKI noch (gemerkt ueber Wiederverbindungen,
// mit Endzeit) -> sie wird mit der Restdauer erneut geschrieben; sonst -> Freigabe. Unabhaengig von der globalen
// Betriebsart und von Grenzen, die das Geraet als aktiv meldet. Jede Entscheidung steht im Log. (Nach einem NEUSTART
// der Steuerbox ist die eigene Grenze vergessen - dann Freigabe.)
// v199 (25.10., meierchen006): Direktsteuerung mehrerer SteuVE. 6 Verbindungsplaetze fuer eigene Anrufe (statt 3) plus
// der Serverplatz: VDE FNN Hinweis "Schnittstellen der Steuerungseinrichtung" v1.0 (03/2025) Kap. 3 S. 12 nennt
// nach Lastenheft Steuerbox 1.4 bis zu VIER steuerbare Einrichtungen je Steuerbox; Haushalt WP + Klima + 2 Wallboxen
// + Speicher = 5, dazu 1 Reserve. SHIP 1.1.0 Kap. 8.1: bei x Plaetzen hoechstens x-1 als Client und immer ein
// Serverport - hier 6 Client + 1 Server. Neues Ziel "alle" fuer EEBusLpc/EEBusRelease/EEBusLpp/EEBusLppFrei: dieselbe
// Vorgabe an jede stehende Verbindung, Ergebnis je Platz (BK6-22-300 Anl. 1 Ziff. 4.5.1: in der Direktsteuerung
// erhaelt jede SteuVE ihre Vorgabe). Einzeln adressieren (SKI) bleibt - der Netzbetreiber steuert je Einrichtung.
// v198 (24.10.): tls_mini haelt den Schluessel der Gegenstelle je Verbindung (Tabelle mit 4 Plaetzen: Client,
// x509-Kontext, Schluessel), Getter BearSSL::ShipPeerPubKey(client, &len). Vorher eine globale Variable: ein anderer
// TLS-Aufbau (z. B. HTTPS eines TinyC-Slots) zwischen connect() und dem Lesen konnte sie ueberschreiben (Hinweis gemu
// 23.10.). Weiter alles hinter USE_EEBUS_*, ein Bau ohne EEBUS bleibt byte-gleich.
// v194 (23.10.): Ist-Zustand der Grenzen nach RICHTUNG statt nach limitId-Nummer. Die limitId vergibt jedes
// Geraet frei, ihre Bedeutung steht nur in der Beschreibung (limitDirection consume/produce, LPC-011). Bisher
// landete Lim0/Lim1 nach der Nummer: am Energiemanager (0 Bezug, 1 Einspeisung) passte das zufaellig, an einem
// HEMS mit limitId 1 = Bezug stand dessen Bezugsgrenze unter § 9. Die Richtungen werden jetzt aus JEDER
// eintreffenden Limit-Beschreibung gelernt, nicht nur waehrend eines Schreibauftrags. Ausserdem: eine neue
// Sitzung loescht die mitgelesenen Werte der vorherigen Gegenstelle (Failsafe, Grenzen, Messwerte) — vorher
// standen nach einem Wechsel noch die Werte des alten Geraets in der Anzeige. EEBusCert 2 kuendigt die neue
// Identitaet sofort per mDNS an (vorher lief bis zum Neustart die alte SKI im Netz weiter).
// v193 (22.10., Wunsch gemu): tls_mini liefert nur noch den oeffentlichen Schluessel der Gegenstelle (BearSSL::ShipPeerPubKey,
// alles hinter USE_EEBUS_*, ein Bau ohne EEBUS bleibt byte-gleich), die SKI bildet der Treiber (SHA-1 ueber den Punkt).
// Ohne EC-Schluessel im Zertifikat: Log "nicht vertraut". Sonst unveraendert zu v192.
// v192 (22.10.): Auch beim EIGENEN Anruf wird die SKI aus dem Zertifikat der Gegenstelle geprueft (tls_mini
// getRecvPubKeySki). Passt sie nicht zur SKI aus der Fundliste, wird nicht verbunden. Passt sie, steht SkiQuelle
// "Zertifikat". Damit ist die Identitaet in beiden Richtungen an das Zertifikat gebunden (Konstruktionsfehler seit 31.07.).
// v191 (22.10.): Ein Anrufer, der sich mit Zertifikat ausgewiesen hat (v190, SkiQuelle "Zertifikat"), wird jetzt auch
// als Wiederaufbau-Partner gemerkt. Bisher nur eigene Anrufe - eingehend stand frueher nur die IP als Pseudo-SKI. Anlass:
// HM rief an, Steuerbox merkte ihn nicht, nach dem naechsten Neustart suchte sie nichts (22.10. 11:56, HM rief nicht zurueck).
// v190 (22.10., aus HEMS v006): Ein Anrufer wird an der SKI aus SEINEM ZERTIFIKAT erkannt, nicht mehr an der IP.
// Der TLS-Server fordert das Client-Zertifikat an (leerer Name, BR_OPT_TOLERATE_NO_CLIENT_AUTH), EebusSrvX509 liest den
// EC-Schluessel, SKI = SHA-1 ueber den Punkt; BearSSL prueft die CertificateVerify-Signatur. Ohne Zertifikat gilt der
// alte Weg (Fundliste/IP, v185/v188) als Rueckfall. EEBusStatus zeigt je Slot "SkiQuelle". Konstruktionsfehler seit
// 31.07.: die Identitaet hing an der Adresse (Brief an gemu 22.10. 11:29).
// v189 (21.10.): Absturz beim Anrufen eines Geraets, das per mDNS nur IPv6 meldet (fromString("") -> fail -> stop() auf
// nie verbundenem TLS-Client -> Load access fault). Zieladresse vorher pruefen, ersatzweise IPv6, sonst kein Versuch.
// v188 (21.10.): eingehende Verbindung zusaetzlich ueber die AccessMethods-Kennung der SKI zuordnen (EebusSrvSkiFromId),
// wenn die Adresse nicht passt (Geraet mit mehreren IPv6-Adressen ruft ueber eine andere an, als der Scan gemerkt hat).
// v187 (20.10.): Senden robust. (1) Frame in EINEM Puffer + EINEM write() statt 64-B-Stuecken (je ein TLS-Record).
// (2) Geht ein Frame nur teilweise raus (Log v186: 3456 von 3622 B an die SteuVE, danach auch 0 B an den HEMS),
// wird nichts mehr gesendet und in der Datenphase sauber abgebaut (EebusSendBroken, send_broken), statt mit
// abgeschnittenem Frame weiterzulaufen. Ursache der Blockade (beide Seiten senden, keine liest) behebt SteuVE v015.
// v186 (20.10.): nur Diagnose, Verhalten unveraendert. Logzeile "WS-SENDEN UNVOLLSTAENDIG", wenn der TLS-Client
// einen Kopf oder ein 64-B-Stueck nicht ganz annimmt (Frame bleibt abgeschnitten im Strom), und "SPINE-Senden
// FEHLGESCHLAGEN" an der zentralen SPINE-Sendestelle (Ergebnis wurde bisher nicht ausgewertet). Anlass: Direkt-
// verbindung zur SteuVE - deren Empfaenger las Text als WS-Kopf (49-B-"Fragmente", Schein-Close), Strom aus dem Takt.
// v185 (20.10.): IPv6. Eine Gegenstelle rief ueber IPv6 an (fd37:...); peer_ip hatte 16 Zeichen und schnitt die
// Adresse ab (auch die Pseudo-SKI eingehender Verbindungen). Jetzt 46 Zeichen. Der Scan merkt sich zusaetzlich
// die erste IPv6 je Geraet (ip6), damit eine eingehende IPv6-Verbindung ihre echte SKI aus der Fundliste bekommt
// (vorher nur IPv4 -> Seite und SKI-Auswahl fanden die Verbindung nicht). EEBusPeers zeigt "Ip6".
// v184 (08.10.): limitId beider Richtungen aus EINER Beschreibung merken (lpc_lid_dir). Bisher loeschte
// ein Richtungswechsel die limitId, und die Anmeldung der Einspeisung las Beschreibung, Ist-Werte und
// Geraetekonfiguration noch einmal, die das Onboarding Sekunden vorher gelesen hatte.

// LPC-Schreib-Sequenz je Verbindung (Steuerbox → Controllable System):
// Binding → Limit-Beschreibung lesen (limitId) → Ist-LimitData lesen (isLimitChangeable) →
// Write → Read-Back verifizieren → Done.  LPC_READDATA MUSS zwischen READDESC und WRITE bleiben
// (Reihenfolge relevant fuer den Phasen-Timeout-Bereich BIND..VERIFY).
// LPC_FAILSAFE zwischen READEL und WRITE — der §14a-Failsafe-Write (DeviceConfig) laeuft VOR dem
// eigentlichen Limit-Write. Bleibt im Phasen-Timeout-Bereich BIND..VERIFY (Reihenfolge relevant).
enum EebusLpcState : uint8_t {
  LPC_IDLE = 0, LPC_BIND, LPC_READDESC, LPC_READDATA, LPC_READCFG, LPC_READEL, LPC_FAILSAFE, LPC_WRITE, LPC_VERIFY, LPC_DONE, LPC_FAIL
};

// Unsere eigene LoadControl-Adresse als CONTROLLER (Client-Feature): ent1/feat2. Nur bei Rolle EIN
// deklariert (damit ein Binding gueltig ist). Der Peer wird VON hier aus geschrieben.
// feat2 statt feat7 — der akzeptierte Vergleichs-Steuerbox-Write hat addressSource ent1/feat2
// ; unser feat7-Write bekam errorNumber:7. Byte-Diff der beiden
// Write-Frames = nur feat7/feat2 + timePeriod (s.u.). Role-1-Discovery deklariert LC-Client jetzt feat2.
#define EEBUS_LPC_CLIENT_ENT   1
#define EEBUS_LPC_CLIENT_FEAT  2
// Rolle 1 deklariert ZWEI Client-Features: LoadControl-client (feat7) = Controller-Binding und
// DeviceDiagnosis-client (feat4) = Heartbeat-Ausloeser. Das feat4-client ist zwingend: manche
// Gegenstellen suchen genau dieses Feature, um unseren Heartbeat zu abonnieren, und nutzen dann
// zusaetzlich unser DeviceDiagnosis-SERVER feat2 als Quelle. Fehlt feat4, gibt es kein Abo — die
// Gegenstelle sieht keinen Heartbeat und wendet ihren Failsafe-Wert an statt unseres Limits.
// Die uebrigen Bedingungen erfuellt EebusSpineAnswerUseCase: actor=EnergyGuard,
// useCaseName=limitationOfPowerConsumption, useCaseAvailable=true.
#define EEBUS_LPC_CLIENT_FEATURE ",[{\"description\":[{\"featureAddress\":[{\"entity\":[1]},{\"feature\":7}]},"\
  "{\"featureType\":\"LoadControl\"},{\"role\":\"client\"},{\"supportedFunction\":[]},"\
  "{\"description\":\"Load Control Client\"}]}]"\
  ",[{\"description\":[{\"featureAddress\":[{\"entity\":[1]},{\"feature\":4}]},"\
  "{\"featureType\":\"DeviceDiagnosis\"},{\"role\":\"client\"},{\"supportedFunction\":["\
  "[{\"function\":\"deviceDiagnosisHeartbeatData\"},{\"possibleOperations\":[{\"read\":[]}]}]]},"\
  "{\"description\":\"Device Diagnosis Client\"}]}]"
// Wartezeit je LPC-Phase. Frueher 8 s — zu kurz: der Energiemanager antwortet zeitweise erst nach
// 12-13 s (05.10. gemessen: Limit-Beschreibung 11,8 s). Nach 8 s gab die Box den Grenzwert-Teil auf.
#define EEBUS_LPC_PHASE_TIMEOUT_MS 20000UL
// Read-Back-Timing. Das Geraet uebernimmt den geschriebenen Limit-Zustand mit Latenz —
// sofortiges Zuruecklesen liefert faelschlich noch den ALTEN Zustand ("Limit NICHT aktiv").
// Deshalb den 1. Read-Back verzoegern und bei Nichtuebereinstimmung mehrfach nachpollen.
#define EEBUS_LPC_VERIFY_DELAY_MS  2500UL   // Wartezeit vor dem 1. Read-Back nach dem Write
#define EEBUS_LPC_VERIFY_RETRY_MS  1500UL   // Abstand der Nachpoll-Versuche
#define EEBUS_LPC_VERIFY_MAX_TRIES 4   // so oft nachpollen, bevor "nicht aktiv" gemeldet wird
// err7-Retry nach der EEBus-LPC-Spec V1.0.0. [LPC-906]: die erste Ablehnung kann reines
// Timing sein (CS gerade im "init") -> "the EG may simply send Heartbeat and write command once more".
// [LPC-914]: der Write wird nur bewertet, wenn UNMITTELBAR davor ein Heartbeat kam und der Write <60 s
// folgt. Daher: bei errorNumber:7 automatisch Heartbeat+Write erneut, mehrmals. Nur HEMS-Modus.
#define EEBUS_LPC_WRITE_MAX_TRIES  1   // v237: so oft HB+Write nach err7 SOFORT wiederholen - nur einmal und nur kurz nach dem
                                      // Verbinden (LPC TS 2.2 Hinweis / LPC IG 2.5 Z. 269-281: "once more", Zustand "init").
                                      // Bis v236 dreimal bei JEDEM err7 -> Test 22.11.: alle ~13 s eine Grenze ans HEMS.
#define EEBUS_LPC_INIT_FENSTER_MS 120000UL   // v237: so lange nach dem Verbinden kann die Anlage noch in "init" sein (LPC-906)
#define EEBUS_WDH_MAX_MS          600000UL   // v237: laengste Wartezeit der Wiederholung (LPC IG 2.5 Z. 283-286: jedes Mal laenger)
#define EEBUS_LPC_WRITE_RETRY_MS   2000UL   // Pause zwischen den HB+Write-Retries (CS "init" absetzen lassen)
// §14a verlangt zwingend einen Failsafe (Rueckfall-Limit + Mindestdauer, falls die Steuerbox-
// Verbindung abreisst). Die Vergleichs-Steuerbox SCHREIBT vor dem Consumption-Limit die Failsafe-DeviceConfig
// (WriteFailsafeConsumptionActivePowerLimit + WriteFailsafeDurationMinimum); wir lasen sie bisher nur.
// Werte wie die Vergleichs-Steuerbox (Mitschnitt): 1.000.000 W Rueckfall-Limit, Mindestdauer 2 h.

// Ein per mDNS gefundener SHIP-Dienst (TXT-Records nach SHIP-Spec 7.3.2)
typedef struct {
  char ski[41];   // 40 Hex-Zeichen + NUL — die Identität des Peers
  char id[49];   // TXT "id"
  char model[33];   // TXT "model" (leer -> mDNS instance name)
  char type[25];   // TXT "type" (z.B. EnergyManagementSystem, ChargingStation)
  char brand[25];   // TXT "brand"
  char path[17];   // TXT "path" — WSS-Pfad, Default "/ship/"
  char cat[12];   // TXT "cat" — DeviceCategory (z.B. "1"=GridConnectionHub); "" = nicht gesetzt
  char reg[8];   // TXT "register" — "true"/"false" (Auto-Accept-Flag nach SHIP 7.3.2)
  char ip[16];   // erste IPv4
  char ip6[46];  // v185: erste IPv6 (eingehende IPv6-Verbindungen der echten SKI zuordnen)
  char host[33];   // mDNS-Hostname
  uint16_t port;
} EebusPeer;

struct {
  mdns_search_once_t *scan = nullptr;
  uint16_t scan_polls = 0;   // Sicherheits-Timeout beim Pollen (100-ms-Ticks)
  bool mdns_ready = false;
  bool advertised = false;   // M3: eigene _ship._tcp-Ankuendigung aktiv?
  bool cert_ok = false;
  char own_ski[41] = { 0 };
  uint8_t peer_count = 0;
  EebusPeer peers[EEBUS_MAX_PEERS];
   // Letztes Verbindungsergebnis JE Peer (per SKI, nicht per Index — die Scan-Reihenfolge
   // wechselt!) fuer die Web-Uebersicht: eine Zeile pro Geraet statt einer wechselnden Zeile.
  uint8_t stat_count = 0;
  char    stat_ski[EEBUS_MAX_PEERS][41];
  uint8_t stat_state[EEBUS_MAX_PEERS];   // SHIP_IDLE=nie versucht, SHIP_CMI_OK, SHIP_ERROR
  char    stat_err[EEBUS_MAX_PEERS][24];
   // Pro-SKI-Betriebsmodus (Geraet -> HEMS-Partner oder SteuVE-Partner). Loest den globalen
   // EEBusHems-Schalter geraeteweise ab: beim Verbinden/Schreiben wird der Modus aus der peer-SKI
   // aufgeloest und in eebus_hems_mode gesetzt (alle bewiesenen Write-Zweige unveraendert). RAM-only.
  uint8_t mode_count = 0;
  char    mode_ski[EEBUS_MAX_PEERS][41];
  uint8_t mode_val[EEBUS_MAX_PEERS];   // 0 = SteuVE-Partner, 1 = HEMS-Partner
} Eebus;

/*********************************************************************************************\
 * Selbsttaetiger Wiederaufbau nach einem Neustart
 *
 * Der vorhandene Auto-Reconnect haelt eine LAUFENDE Verbindung; nach einem Neustart greift er
 * nicht, weil BEIDES weg ist: die Fundliste (nur RAM) und der Slot mit der gemerkten SKI.
 * Deshalb: SKI in einer Datei merken, beim Hochfahren einmal suchen und verbinden.
 *
 * ⚠️ ZWEI BEDINGUNGEN, beide aus Schaden gelernt:
 *   1. NICHT sofort verbinden. Ein zu schneller Verbindungsversuch endet in "cmi resp n=-1",
 *      und der Slot bleibt DANACH BIS ZUM NAECHSTEN NEUSTART im Fehler. Nach einer
 *      Spannungswiederkehr kommt hinzu, dass Switch, Router und die Gegenstelle selbst erst
 *      hochfahren — per mDNS ist dann noch gar nichts zu finden.
 *   2. ERST WENN DIE UHR STIMMT. Unser Herzschlag traegt einen UTC-Zeitstempel. Ohne
 *      Zeitsynchronisation meldet sich die Box mit einer Uhrzeit aus dem Jahr 1970 an — die
 *      Gegenstelle haelt den Herzschlag fuer uralt und lehnt JEDES Limit ab, ohne dass
 *      irgendwo ein Fehler erscheint. Genau diese Klasse Fehler hat uns Wochen gekostet.
\*********************************************************************************************/
#define EEBUS_AC_DELAY_DEFAULT_S    30   // Wartezeit nach Netz+Uhr, bevor gesucht wird (v208: 30 statt 120 s, Test 26.10.)
#define EEBUS_AC_SCAN_WAIT_MS     6000   // Zeit, die der mDNS-Scan zum Fuellen der Liste bekommt
#define EEBUS_AC_MAX_TRY             4   // danach aufgeben — ein Mensch muss ran

   // Abstaende zwischen den Versuchen: wachsend, NICHT die starren 8 s des Keep-Alive.
   // Zu dichte Wiederholungen sind genau der Weg in "cmi resp n=-1".
   // ⚠️ Es sind EINS WENIGER als Versuche: bei vier Versuchen gibt es drei Zwischenraeume.
   // Frueher stand hier ein vierter Wert (300 s), der NIE gelesen wurde — der Zaehler laeuft
   // 1, 2, 3, und beim vierten Versuch wird aufgegeben, bevor indiziert wird. Kein Ueberlauf,
   // aber Text und Code widersprachen sich. Bewusst die Tabelle gekuerzt statt die Zahl der
   // Versuche erhoeht: so aendert sich am Verhalten nichts, nur die Beschreibung stimmt wieder.
const uint16_t kEebusAcRetryS[EEBUS_AC_MAX_TRY - 1] PROGMEM = { 30, 60, 120 };

enum EebusAcState : uint8_t { AC_IDLE, AC_WAIT, AC_SCAN, AC_DONE };

uint16_t eebus_ac_delay_s = EEBUS_AC_DELAY_DEFAULT_S;   // 0 = Wiederaufbau abgeschaltet
bool     eebus_ac_delay_loaded = false;   // v208: Wert aus EEBUS_AC_FILE schon gelesen?
char     eebus_ac_ski[41] = { 0 };   // SKI, die der Wiederaufbau GERADE bearbeitet ("" = keine)
#define  EEBUS_AC_MAX 6              // v202: so viele Partner merkt sich der Wiederaufbau (= Verbindungsplaetze)
char     eebus_ac_list[EEBUS_AC_MAX][41] = { { 0 } };   // v202: gemerkte Partner-SKIs
uint8_t  eebus_ac_n       = 0;       // v202: Anzahl gemerkter Partner
uint8_t  eebus_ac_cur     = 0;       // v202: Index des Partners, den der Wiederaufbau gerade bearbeitet
uint8_t  eebus_ac_state   = AC_IDLE;
uint32_t eebus_ac_next    = 0;       // millis() fuer den naechsten Schritt
uint8_t  eebus_ac_try     = 0;       // Zaehler der Verbindungsversuche

// Peer-SKI merken. Abgelegt wie Zertifikat und Schluessel im Flash-FS, damit sie einen
// Neustart ueberlebt. Nur ECHTE SKIs (40 Hex-Zeichen) — der Platzhalter "manual" aus
// EEBusConnectIp identifiziert kein Geraet und taugt zum Wiederfinden nicht.
// v202: ist eine SKI gerade verbunden (Datenphase)?
bool EebusAcConnected(const char *ski);
// v202: Partnerliste speichern, eine SKI je Zeile (leer -> Datei weg)
void EebusPeerSave(void) {
#ifdef USE_UFILESYS
  char buf[EEBUS_AC_MAX * 41 + 1];
  size_t l = 0;
  for (uint8_t i = 0; i < eebus_ac_n; i++) { memcpy(buf + l, eebus_ac_list[i], 40); l += 40; buf[l++] = '\n'; }
  if (l) { TfsSaveFile(EEBUS_PEER_FILE, (const uint8_t*)buf, l); }
  else if (TfsFileExists(EEBUS_PEER_FILE)) { TfsDeleteFile(EEBUS_PEER_FILE); }
#endif
}
void EebusPeerRemember(const char *ski) {
  if ((nullptr == ski) || (40 != strlen(ski))) { return; }
  for (uint8_t i = 0; i < eebus_ac_n; i++) { if (0 == strcasecmp(eebus_ac_list[i], ski)) { return; } }   // schon gemerkt
  uint8_t pl = eebus_ac_n;
  if (eebus_ac_n >= EEBUS_AC_MAX) {   // voll: den ersten gemerkten Partner ersetzen, der gerade NICHT verbunden ist
    pl = EEBUS_AC_MAX;
    for (uint8_t i = 0; i < EEBUS_AC_MAX; i++) { if (!EebusAcConnected(eebus_ac_list[i])) { pl = i; break; } }
    if (EEBUS_AC_MAX == pl) { AddLog(LOG_LEVEL_INFO, PSTR("EBG: Partnerliste voll - %s nicht gemerkt"), ski); return; }
  } else {
    eebus_ac_n++;
  }
  strlcpy(eebus_ac_list[pl], ski, sizeof(eebus_ac_list[pl]));
  EebusPeerSave();
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Partner %s gemerkt (%u gemerkt) - nach einem Neustart wird selbsttaetig verbunden"),
         ski, eebus_ac_n);
}

// Bewusstes Trennen soll NICHT beim naechsten Start rueckgaengig gemacht werden.
void EebusPeerForget(void) {
  eebus_ac_ski[0] = '\0';
  eebus_ac_n      = 0;   // v202: die ganze Liste
  eebus_ac_state  = AC_DONE;
#ifdef USE_UFILESYS
  if (TfsFileExists(EEBUS_PEER_FILE)) {
    TfsDeleteFile(EEBUS_PEER_FILE);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: gemerkter Peer geloescht - kein selbsttaetiger Wiederaufbau mehr"));
  }
#endif
}

// Gemerkte SKI beim Hochfahren einlesen. true, wenn eine vollstaendige SKI vorliegt.
bool EebusPeerRecall(void) {
#ifdef USE_UFILESYS
  eebus_ac_ski[0] = '\0';
  eebus_ac_n = 0;
  if (!TfsFileExists(EEBUS_PEER_FILE)) { return false; }
  File f = ffsp->open(EEBUS_PEER_FILE, "r");
  if (!f) { return false; }
  char buf[EEBUS_AC_MAX * 41 + 1];
  int n = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  if (n < 0) { n = 0; }
  buf[n] = '\0';
   // v202: je Zeile eine SKI (40 Hex-Zeichen); die alte Datei ohne Zeilenende enthaelt genau eine
  char *s = buf;
  while (*s && (eebus_ac_n < EEBUS_AC_MAX)) {
    while (*s && !isxdigit((unsigned char)*s)) { s++; }
    size_t k = 0;
    while (isxdigit((unsigned char)s[k])) { k++; }
    if (40 == k) { memcpy(eebus_ac_list[eebus_ac_n], s, 40); eebus_ac_list[eebus_ac_n][40] = '\0'; eebus_ac_n++; }
    s += k;
  }
  if (eebus_ac_n) { strlcpy(eebus_ac_ski, eebus_ac_list[0], sizeof(eebus_ac_ski)); }
  return (eebus_ac_n > 0);
#else
  return false;
#endif
}

// SKI -> Betriebsmodus. Index des Eintrags einer SKI, -1 wenn nicht zugeordnet.
int EebusModeFind(const char *ski) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return -1; }
  for (uint32_t i = 0; i < Eebus.mode_count; i++) {
    if (0 == strcasecmp(Eebus.mode_ski[i], ski)) { return (int)i; }
  }
  return -1;
}
// Modus einer SKI: 1 = HEMS-Partner, 0 = SteuVE-Partner, -1 = keine Zuordnung (globaler Default gilt).
int EebusModeForSki(const char *ski) {
  int i = EebusModeFind(ski);
  return (i < 0) ? -1 : (int)Eebus.mode_val[i];
}
// Betriebsart aus dem GEMELDETEN GERAETETYP ableiten (aus dem mDNS-Scan, Feld "type").
// Ein Energiemanager ist ein HEMS, alles andere (Gateway, ChargingStation, …) eine steuerbare
// Verbrauchseinrichtung. Warum das noetig wurde: die Zuordnung per EEBusPeerMode liegt nur im
// Arbeitsspeicher und ist nach JEDEM Neustart weg — danach galt wieder der globale Vorgabewert,
// und der stand inzwischen auf HEMS. An einer Wallbox ging der Write damit ohne Geltungsdauer raus
// und wurde verworfen (real passiert, sah aus wie ein Protokollfehler und war keiner).
// Der Typ dagegen steht bei jedem Scan neu zur Verfuegung und kann nicht "vergessen" werden.
// Rueckgabe: 1 = HEMS, 0 = SteuVE, -1 = kein Typ gemeldet (dann entscheidet der globale Default).
int EebusModeFromType(const char *ski) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return -1; }
  for (uint32_t i = 0; i < Eebus.peer_count; i++) {
    if (0 != strcasecmp(Eebus.peers[i].ski, ski)) { continue; }
   // Leerzeichen raus + kleinschreiben: die Geraete melden mal "Energy Manager",
   // mal "EnergyManagementSystem" — beides ist dasselbe.
    char t[32]; size_t o = 0;
    for (const char *s = Eebus.peers[i].type; *s && (o < sizeof(t) - 1); s++) {
      if (' ' != *s) { t[o++] = (char)tolower((unsigned char)*s); }
    }
    t[o] = '\0';
    if ('\0' == t[0]) { return -1; }   // nichts gemeldet -> keine Aussage
    return (nullptr != strstr(t, "energymanag")) ? 1 : 0;
  }
  return -1;   // SKI nicht in der Fundliste
}

// die WIRKSAME Betriebsart einer SKI, in dieser Reihenfolge:
//   1. ausdrueckliche Zuordnung (EEBusPeerMode) — der Mensch hat entschieden, das gilt
//   2. abgeleitet aus dem gemeldeten Geraetetyp   — das Geraet sagt selbst, was es ist
//   3. -1  -> der globale EEBusHems-Default bleibt stehen (z.B. bei EEBusConnectIp ohne Scan)
int EebusModeFromDisco(const char *ski);   // v209: Definition hinter EConn[] (braucht die Verbindungs-Slots)
int EebusModeEffective(const char *ski) {
  int m = EebusModeForSki(ski);
  if (m >= 0) { return m; }
  m = EebusModeFromType(ski);
  if (m >= 0) { return m; }
  return EebusModeFromDisco(ski);   // v209
}

// Setzt den Session-Modus eebus_hems_mode fuer diese Gegenstelle. Greift weder Zuordnung noch
// Typ, bleibt der globale EEBusHems-Default stehen. So gelten pro Geraet die richtigen Regeln.
void EebusModeApply(const char *ski) {
  int m = EebusModeForSki(ski);
  const char *quelle = "Zuordnung";
  if (m < 0) { m = EebusModeFromType(ski); quelle = "Geraetetyp"; }
  if (m < 0) { m = EebusModeFromDisco(ski); quelle = "Discovery"; }   // v209
  if (m >= 0) { Eebus126ModeSetSki(ski, m, quelle); }   // v231: am Slot dieser Gegenstelle merken, nicht global
  Eebus126ModeLoad();   // v231: Betriebsart des aktuellen Slots (ESp) neu laden
}
// Zuordnung setzen (mode 0/1) oder loeschen (mode < 0). true = ok.
bool EebusModeSet(const char *ski, int mode) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return false; }
  int i = EebusModeFind(ski);
  if (mode < 0) {   // loeschen
    if (i < 0) { return false; }
    for (uint32_t j = (uint32_t)i + 1; j < Eebus.mode_count; j++) {
      strlcpy(Eebus.mode_ski[j - 1], Eebus.mode_ski[j], sizeof(Eebus.mode_ski[0]));
      Eebus.mode_val[j - 1] = Eebus.mode_val[j];
    }
    Eebus.mode_count--;
    return true;
  }
  if (i < 0) {   // neu anlegen
    if (Eebus.mode_count >= EEBUS_MAX_PEERS) { return false; }
    i = (int)Eebus.mode_count++;
    strlcpy(Eebus.mode_ski[i], ski, sizeof(Eebus.mode_ski[0]));
  }
  Eebus.mode_val[i] = (uint8_t)mode;
  return true;
}

// WICHTIG (.ino-Falle): PlatformIOs Prototypen-Generator hebt Funktionssignaturen an den
// Dateianfang VOR alle Includes und ignoriert #ifdef. Deshalb duerfen .ino-Funktions-
// signaturen hier KEINE mbedtls-/mdns-Typen verwenden (nur void*/Basistypen) und es gibt
// keinen mdns-Notifier-Callback — stattdessen wird der Scan in der Loop gepollt.

/*********************************************************************************************\
 * Zertifikat + SKI (SHIP-Identität)
 *
 * SHIP verlangt ein (selbstsigniertes) X.509-Zertifikat mit EC-Schlüssel; die SKI
 * (Subject Key Identifier) ist der SHA-1 über den öffentlichen Schlüssel und dient
 * als Geräte-Identität beim Pairing.
\*********************************************************************************************/

// SKI = SHA-1 GENAU über den UNKOMPRIMIERTEN EC-Punkt (0x04||X||Y, 65 B bei P-256).
// FIX: Das ist die norm-korrekte SHIP-Berechnung — identisch zu Referenz-Umsetzung cert.go
// SkiFromCertificate(): `sha1.Sum(ecdhKey.Bytes())`, wobei ecdhKey.Bytes() der unkomprimierte
// Punkt ist (RFC 3280 4.2.1.2 Methode 1). Der ALTE Weg hashte `mbedtls_pk_write_pubkey`-Output
// -> falsche Bytes -> falsche SKI (…05f3 statt d0ed…599d) -> strenge SHIP-Peers (Referenz-Umsetzung)
// verwarfen unser Zert als "invalid SKI". Ladestation/Waermepumpen-Gateway/Energiemanager tolerierten es, Referenz-Umsetzung NICHT.
// pk_key ist ein mbedtls_pk_context* (void* wegen .ino-Prototypen-Falle, s.o.)
bool EebusSkiRawFromKey(void *pk_key, uint8_t *hash20) {
  mbedtls_pk_context *key = (mbedtls_pk_context *)pk_key;
   // mbedtls 3.x: mbedtls_ecp_keypair ist opaque (kein Direktzugriff auf grp/Q). Wir holen die
   // SubjectPublicKeyInfo per DER-Writer; deren LETZTE 65 Byte sind GENAU der unkomprimierte
   // EC-Punkt (0x04||X||Y) = Referenz-Umsetzungs ecdhKey.Bytes(). SHA1 davon = norm-korrekte SKI.
  unsigned char der[160];   // SPKI P-256 ~91 B + Reserve
  int len = mbedtls_pk_write_pubkey_der(key, der, sizeof(der));
  if (len < 65) { return false; }
   // mbedtls_pk_write_pubkey_der schreibt ans PUFFER-ENDE (mbedtls-Konvention): gueltige Daten liegen
   // bei der[sizeof-len .. sizeof-1]. Die SPKI endet mit der BIT STRING (03 42 00 04 X Y), deren LETZTE
   // 65 Byte = 04||X||Y = unkomprimierter EC-Punkt = Referenz-Umsetzungs ecdhKey.Bytes(). SHA1 davon = norm-korrekte
   // SKI (per DER-Volldump + openssl gegen das echte Geraete-Zert 047aeb95->9a5fc9e3 bewiesen).
  const unsigned char *point = der + sizeof(der) - 65;
   // BearSSL br_sha1 (bekannt-korrekt); SHA1Builder lieferte in fruehen Versionen falsche Hashes.
  br_sha1_context sc;
  br_sha1_init(&sc);
  br_sha1_update(&sc, point, 65);
  br_sha1_out(&sc, hash20);
  return true;
}

bool EebusSkiFromKey(void *pk_key, char *ski_hex41) {
  uint8_t hash[20];
  if (!EebusSkiRawFromKey(pk_key, hash)) { return false; }
  for (uint32_t i = 0; i < 20; i++) {
    snprintf_P(ski_hex41 + (i * 2), 3, PSTR("%02x"), hash[i]);
  }
  return true;
}

// Key (DER) aus Datei laden und SKI berechnen. true wenn beides klappt.
bool EebusLoadCert(void) {
#ifdef USE_UFILESYS
  if (!TfsFileExists(EEBUS_CERT_FILE) || !TfsFileExists(EEBUS_KEY_FILE)) { return false; }
  File f = ffsp->open(EEBUS_KEY_FILE, "r");
  if (!f) { return false; }
  size_t key_len = f.size();
  if ((0 == key_len) || (key_len > EEBUS_KEY_DER_SIZE)) { f.close(); return false; }
  uint8_t *key_der = (uint8_t*)calloc(1, EEBUS_KEY_DER_SIZE);
  if (nullptr == key_der) { f.close(); return false; }
  bool ok = (key_len == f.read(key_der, key_len));
  f.close();
  if (ok) {
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
    ok = (0 == mbedtls_pk_parse_key(&key, key_der, key_len, nullptr, 0,
                                    mbedtls_ctr_drbg_random, &ctr_drbg));
#else
    ok = (0 == mbedtls_pk_parse_key(&key, key_der, key_len, nullptr, 0));
#endif
    if (ok) { ok = EebusSkiFromKey(&key, Eebus.own_ski); }
    mbedtls_pk_free(&key);
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
#endif
  }
  free(key_der);
  return ok;
#else
  return false;
#endif   // USE_UFILESYS
}

// EC-Key (secp256r1) + selbstsigniertes Zertifikat erzeugen und im Flash-FS ablegen.
bool EebusGenerateCert(void) {
#ifndef USE_UFILESYS
  AddLog(LOG_LEVEL_ERROR, PSTR("EBG: Kein Dateisystem (USE_UFILESYS) - Zertifikat nicht persistierbar"));
  return false;
#else
  bool ok = false;
  mbedtls_pk_context key;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_x509write_cert crt;
  mbedtls_pk_init(&key);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_x509write_crt_init(&crt);

  uint8_t *cert_der = (uint8_t*)calloc(1, EEBUS_CERT_DER_SIZE);
  uint8_t *key_der = (uint8_t*)calloc(1, EEBUS_KEY_DER_SIZE);

  do {
    if ((nullptr == cert_der) || (nullptr == key_der)) { break; }
    if (0 != mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                   (const unsigned char*)"eebus_guard", 11)) { break; }
    if (0 != mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY))) { break; }
    if (0 != mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key),
                                 mbedtls_ctr_drbg_random, &ctr_drbg)) { break; }

    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    mbedtls_x509write_crt_set_issuer_key(&crt, &key);   // selbstsigniert
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
    unsigned char serial[1] = { 0x01 };
    if (0 != mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial))) { break; }
#else
    mbedtls_mpi serial;
    mbedtls_mpi_init(&serial);
    mbedtls_mpi_lset(&serial, 1);
    int sret = mbedtls_x509write_crt_set_serial(&crt, &serial);
    mbedtls_mpi_free(&serial);
    if (0 != sret) { break; }
#endif
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    if (0 != mbedtls_x509write_crt_set_subject_name(&crt, "C=DE,O=Tasmota,CN=EEBUS_GUARD")) { break; }
    if (0 != mbedtls_x509write_crt_set_issuer_name(&crt, "C=DE,O=Tasmota,CN=EEBUS_GUARD")) { break; }
    if (0 != mbedtls_x509write_crt_set_validity(&crt, "20260101000000", "21260101000000")) { break; }
    if (0 != mbedtls_x509write_crt_set_basic_constraints(&crt, 1, -1)) { break; }

   // KeyUsage = digitalSignature (critical) — byte-exakt wie das Referenz-Umsetzung/Vergleichs-Steuerbox-Zert,
   // das der Energiemanager AKZEPTIERT (Referenz-Umsetzung cert.go: `KeyUsage: x509.KeyUsageDigitalSignature`).
   // Unser Zert hatte BISHER KEIN KeyUsage — der einzige strukturelle Unterschied zum Vergleichs-Steuerbox-Zert.
   // KeyUsage ist ein BIT STRING; digitalSignature = Bit 0 -> DER: 03 02 07 80 (7 unused bits, 0x80).
    uint8_t ku_ext[4] = { 0x03, 0x02, 0x07, 0x80 };
    if (0 != mbedtls_x509write_crt_set_extension(&crt, MBEDTLS_OID_KEY_USAGE,
                                                 MBEDTLS_OID_SIZE(MBEDTLS_OID_KEY_USAGE),
                                                 1 /*critical*/, ku_ext, sizeof(ku_ext))) { break; }

   // SubjectKeyIdentifier-Extension manuell setzen —
   // mbedtls_x509write_crt_set_subject_key_identifier() ist im Tasmota-mbedTLS nicht
   // einkompiliert. Inhalt identisch: OCTET STRING (0x04, 20) + SHA1(Public Key).
   // SHIP-Peers lesen unsere SKI aus genau dieser Extension!
    uint8_t ski_raw[20];
    if (!EebusSkiRawFromKey(&key, ski_raw)) { break; }
    uint8_t ski_ext[22];
    ski_ext[0] = 0x04;   // ASN.1 OCTET STRING
    ski_ext[1] = sizeof(ski_raw);   // Laenge 20
    memcpy(ski_ext + 2, ski_raw, sizeof(ski_raw));
    if (0 != mbedtls_x509write_crt_set_extension(&crt, MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER,
                                                 MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_KEY_IDENTIFIER),
                                                 0, ski_ext, sizeof(ski_ext))) { break; }

   // DER-Writer schreiben ans PUFFER-ENDE und liefern die Laenge zurueck
    int key_len = mbedtls_pk_write_key_der(&key, key_der, EEBUS_KEY_DER_SIZE);
    if (key_len <= 0) { break; }
    int cert_len = mbedtls_x509write_crt_der(&crt, cert_der, EEBUS_CERT_DER_SIZE,
                                             mbedtls_ctr_drbg_random, &ctr_drbg);
    if (cert_len <= 0) { break; }

    if (!TfsSaveFile(EEBUS_KEY_FILE, key_der + EEBUS_KEY_DER_SIZE - key_len, key_len)) { break; }
    if (!TfsSaveFile(EEBUS_CERT_FILE, cert_der + EEBUS_CERT_DER_SIZE - cert_len, cert_len)) { break; }

    ok = EebusSkiFromKey(&key, Eebus.own_ski);
  } while (0);

  free(cert_der);
  free(key_der);
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&key);
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  if (ok) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Zertifikat erzeugt, SKI %s"), Eebus.own_ski);
  } else {
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: Zertifikat-Erzeugung fehlgeschlagen"));
  }
  return ok;
#endif   // USE_UFILESYS
}

// Geraete-Kennung aus der eigenen SKI ableiten (s. Kommentar bei eebus_adv_serial).
// Muss laufen, sobald own_ski steht, und VOR jeder mDNS-Ankuendigung.
void EebusBuildIdent(void) {
  size_t l = strlen(Eebus.own_ski);
  if (l >= 8) {
    strlcpy(eebus_adv_serial, Eebus.own_ski + l - 8, sizeof(eebus_adv_serial));
  } else {
    strlcpy(eebus_adv_serial, EEBUS_ADV_SERIAL_FB, sizeof(eebus_adv_serial));
  }
  snprintf_P(eebus_adv_id, sizeof(eebus_adv_id), PSTR("%s-%s-%s"),
             EEBUS_ADV_BRAND, EEBUS_ADV_MODEL, eebus_adv_serial);
}

bool EebusEnsureCert(bool force_new) {
  if (Eebus.cert_ok && !force_new) { return true; }
  Eebus.cert_ok = false;
  Eebus.own_ski[0] = '\0';
  if (!force_new && EebusLoadCert()) {
    Eebus.cert_ok = true;
    EebusBuildIdent();
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Zertifikat geladen, SKI %s, Kennung %s"),
           Eebus.own_ski, eebus_adv_id);
    return true;
  }
  Eebus.cert_ok = EebusGenerateCert();
  if (Eebus.cert_ok) { EebusBuildIdent(); }
  return Eebus.cert_ok;
}

/*********************************************************************************************\
 * mDNS-Discovery: _ship._tcp suchen (asynchron, Callback laeuft im mDNS-Task!)
\*********************************************************************************************/

bool EebusMdnsBegin(void) {
  if (Eebus.mdns_ready) { return true; }
  esp_err_t err = mdns_init();
  if ((ESP_OK != err) && (ESP_ERR_INVALID_STATE != err)) {   // INVALID_STATE = laeuft schon
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: mdns_init Fehler %d"), err);
    return false;
  }
  mdns_hostname_set(TasmotaGlobal.hostname);
   // v212: IPv4 ausdruecklich anmelden. mdns_init laeuft hier erst spaet (Netz + Uhr); das Ereignis "IPv4 bekommen"
   // ist dann laengst vorbei, mDNS kannte nur die spaeter kommenden IPv6-Adressen -> Antworten nur mit AAAA, kein A
   // (26.10.: eine Wallbox ohne eigenes IPv6 listete uns nur mit IPv6 und konnte uns nicht anrufen; HEMS v028 bewiesen).
  static const char * const kIfKeys[] = { "ETH_DEF", "WIFI_STA_DEF" };
  for (uint32_t k = 0; k < sizeof(kIfKeys) / sizeof(kIfKeys[0]); k++) {
    esp_netif_t *nif = esp_netif_get_handle_from_ifkey(kIfKeys[k]);
    esp_netif_ip_info_t ipi;
    if (!nif || !esp_netif_is_netif_up(nif) || (ESP_OK != esp_netif_get_ip_info(nif, &ipi)) || (0 == ipi.ip.addr)) { continue; }
    esp_err_t e = mdns_netif_action(nif, (mdns_event_actions_t)(MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4));
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: mDNS IPv4 %s an %s angemeldet (%d)"),
           IPAddress(ipi.ip.addr).toString().c_str(), kIfKeys[k], e);
  }
  Eebus.mdns_ready = true;
  return true;
}

// M3: uns selbst als SHIP-Dienst (_ship._tcp) im Netz ankuendigen, damit Energiemanager bzw. Hersteller-App uns
// unter "Verfuegbare Geraete" listen. Braucht ein gueltiges Zertifikat (fuer die SKI im TXT).
// Ohne diese Ankuendigung findet uns keine EEBUS-App (nur die Ladestation etc. tauchen auf).
bool EebusMdnsAdvertise(void) {
  if (!EebusMdnsBegin()) { return false; }
  if (!Eebus.cert_ok && !EebusEnsureCert(false)) { return false; }   // SKI noetig

  mdns_service_remove("_ship", "_tcp");   // evtl. alte Ankuendigung ersetzen

   // TXT-Records nach SHIP 7.3.2 (Pflicht: txtvers,path,id,ski,register; Rest beschreibend).
   // Energiemanager-Fix: "type" = DeviceType (NICHT DeviceCategory!) + separates "cat"=DeviceCategory-Zahlen.
   // Referenz-Umsetzung mdns.go: txt "type"=deviceType (z.B. ElectricitySupplySystem), "cat"=comma-sep. Kategorien.
   // FRUEHER FALSCH: wir setzten type="GridConnectionHub" (das ist eine KATEGORIE, kein Typ) + kein cat
   //  -> der Energiemanager konnte uns nicht als Steuerbox-Kategorie erkennen und ordnete uns als Appliance ein.
   // Rolle EIN (Steuerbox): DeviceType ElectricitySupplySystem + cat=1 (GridConnectionHub, wie Referenz-Umsetzung
   //  Vergleichs-Steuerbox). Rolle AUS (Lesemodus): DeviceType ChargingStation, kein cat (unveraendert).
  mdns_txt_item_t txt[] = {
    { (char*)"txtvers",  (char*)"1" },
    { (char*)"path",     (char*)"/ship/" },
    { (char*)"id",       (char*)eebus_adv_id },
    { (char*)"ski",      (char*)Eebus.own_ski },
    { (char*)"register", (char*)"false" },   // 1:1 wie Vergleichs-Steuerbox/Energiemanager/Waermepumpen-Gateway/Ladestation — in Referenz-Umsetzung ist "register" das
   // autoaccept-Flag (mdns.go:316); die Vergleichs-Steuerbox ruft SetAutoAccept NIE ->
   // sendet register:false. register:true (frueher) signalisiert dem Energiemanager
   // "Auto-Accept/registrierbares Geraet" -> er behandelt uns als Appliance
   // statt als fest identifizierten Controller. Alle echten Geraete = false.
    { (char*)"brand",    (char*)EEBUS_ADV_BRAND },
    { (char*)"type",     (char*)((2 == eebus_role) ? "EnergyManagementSystem" :   // CEM-Identitaet
                                 (3 == eebus_role) ? "ElectricitySupplySystem" :   // ControllableSystem-Anbieter
                                 (1 == eebus_role) ? "ElectricitySupplySystem" : EEBUS_ADV_TYPE) },
    { (char*)"model",    (char*)EEBUS_ADV_MODEL },
    { (char*)"serial",   (char*)eebus_adv_serial },   // geraete-individuell, aus der SKI abgeleitet
    { (char*)"cat",      (char*)"1" },   // GridConnectionHub — nur in Rolle 1+3 gesendet (s. n unten; MUSS letztes Element bleiben)
  };
  size_t n_txt = sizeof(txt) / sizeof(txt[0]);
   // cat=1 nur in Rolle 1 (Steuerbox) + Rolle 3 (ControllableSystem-Anbieter); Rolle 2 = OHNE cat
   // (EMS wie VR940-Bridge), Rolle 0 unveraendert ohne cat.
  if ((1 != eebus_role) && (3 != eebus_role)) { n_txt -= 1; }
  esp_err_t err = mdns_service_add(eebus_adv_id, "_ship", "_tcp", EEBUS_ADV_PORT,
                                   txt, n_txt);
  if (ESP_OK != err) {
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: mDNS-Ankuendigung Fehler %d"), err);
    return false;
  }
  Eebus.advertised = true;
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: als SHIP-Dienst angekuendigt (_ship._tcp:%d, id %s, SKI %s)"),
         EEBUS_ADV_PORT, eebus_adv_id, Eebus.own_ski);
  return true;
}

bool EebusStartScan(void) {
  if (Eebus.scan) { return false; }   // Scan laeuft bereits
  if (!EebusMdnsBegin()) { return false; }
  Eebus.scan = mdns_query_async_new(nullptr, "_ship", "_tcp", MDNS_TYPE_PTR,
                                    EEBUS_SCAN_TIMEOUT_MS, INT8_MAX, nullptr);
  if (nullptr == Eebus.scan) {
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: mDNS-Query fehlgeschlagen"));
    return false;
  }
  Eebus.scan_polls = 0;
  return true;
}

// Wird alle 100 ms gepollt, solange ein Scan laeuft. get_results liefert erst true,
// wenn die Suche (EEBUS_SCAN_TIMEOUT_MS) abgeschlossen ist.
void EebusPollScan(void) {
  if (nullptr == Eebus.scan) { return; }

  mdns_result_t *results = nullptr;
  bool have = mdns_query_async_get_results(Eebus.scan, 0, &results, nullptr);
  if (!have) {
    if (++Eebus.scan_polls < 100) { return; }   // laeuft noch (max 10 s Sicherheit)
  }
  mdns_query_async_delete(Eebus.scan);
  Eebus.scan = nullptr;

  Eebus.peer_count = 0;
  if (have && results) {
    for (mdns_result_t *cur = results; cur && (Eebus.peer_count < EEBUS_MAX_PEERS); cur = cur->next) {
      EebusPeer *p = &Eebus.peers[Eebus.peer_count];
      memset(p, 0, sizeof(EebusPeer));
      strlcpy(p->path, "/ship/", sizeof(p->path));   // SHIP-Default
      bool txtvers1 = false;   // v242 (SHIP Z. 570-573): ohne txtvers=1 verwerfen
      for (size_t i = 0; i < cur->txt_count; i++) {
        const char *k = cur->txt[i].key;
        const char *v = cur->txt[i].value;
        if ((nullptr == k) || (nullptr == v)) { continue; }
        if (0 == strcmp(k, "txtvers"))    { txtvers1 = (0 == strcmp(v, "1")); }
        else if (0 == strcmp(k, "ski"))   { strlcpy(p->ski, v, sizeof(p->ski)); }
        else if (0 == strcmp(k, "id"))    { strlcpy(p->id, v, sizeof(p->id)); }
        else if (0 == strcmp(k, "path"))  { strlcpy(p->path, v, sizeof(p->path)); }
        else if (0 == strcmp(k, "model")) { strlcpy(p->model, v, sizeof(p->model)); }
        else if (0 == strcmp(k, "brand")) { strlcpy(p->brand, v, sizeof(p->brand)); }
        else if (0 == strcmp(k, "type"))  { strlcpy(p->type, v, sizeof(p->type)); }
        else if (0 == strcmp(k, "cat"))   { strlcpy(p->cat, v, sizeof(p->cat)); }   // DeviceCategory
        else if (0 == strcmp(k, "register")) { strlcpy(p->reg, v, sizeof(p->reg)); }   // Auto-Accept-Flag
      }
      if ('\0' == p->model[0] && cur->instance_name) {
        strlcpy(p->model, cur->instance_name, sizeof(p->model));
      }
      if (cur->hostname) { strlcpy(p->host, cur->hostname, sizeof(p->host)); }
      p->port = cur->port;
      p->ip[0] = '\0'; p->ip6[0] = '\0';
      for (mdns_ip_addr_t *a = cur->addr; a; a = a->next) {   // erste IPv4 und erste IPv6
        if ((ESP_IPADDR_TYPE_V4 == a->addr.type) && ('\0' == p->ip[0])) {
          strlcpy(p->ip, IPAddress(a->addr.u_addr.ip4.addr).toString().c_str(), sizeof(p->ip));
        }
        if ((ESP_IPADDR_TYPE_V6 == a->addr.type) && ('\0' == p->ip6[0])) {   // v185
          IPAddress a6(IPv6, (const uint8_t *)a->addr.u_addr.ip6.addr);   // gleiche Schreibweise wie remoteIP()
          strlcpy(p->ip6, a6.toString().c_str(), sizeof(p->ip6));
        }
      }
   // SKI ist die Identitaet — ohne SKI ist der Eintrag fuer uns wertlos (SHIP 7.3.2 Pflichtfeld)
      if ('\0' == p->ski[0]) { continue; }
      if (!txtvers1) { AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: mDNS-Eintrag %s ohne txtvers=1 verworfen"), p->ski); continue; }   // v242
   // Dedup nach SKI. mDNS liefert denselben Dienst mitunter MEHRFACH (mehrere Interfaces/
   // Adressen — z.B. der Referenz-CS an 169.254 doppelt). Jedes Geraet soll genau einmal in der
   // Liste stehen (SKI = Identitaet), sonst ist die Geraeteauswahl uneindeutig.
      bool dup = false;
      for (uint32_t d = 0; d < Eebus.peer_count; d++) {
        if (0 == strcasecmp(Eebus.peers[d].ski, p->ski)) { dup = true; break; }
      }
      if (dup) { continue; }
      Eebus.peer_count++;
    }
    mdns_query_results_free(results);
  }

   // Fundliste STABIL nach SKI sortieren. Bisher stand sie in der Reihenfolge, in der die
   // mDNS-Antworten eintrudelten — also bei jedem Scan anders. wanderte der Energiemanager
   // dadurch innerhalb einer Stunde von Platz 0 auf Platz 2, und ein Klick auf "Verbinden" traf die
   // Waermepumpe. Adressiert wird ohnehin ueber die SKI (siehe EebusPeerIdxArg); die feste Sortierung
   // sorgt zusaetzlich dafuer, dass die Anzeige nicht mehr unter dem Anwender springt.
  for (uint32_t i = 1; i < Eebus.peer_count; i++) {
    EebusPeer tmp = Eebus.peers[i];
    int j = (int)i - 1;
    while ((j >= 0) && (strcasecmp(Eebus.peers[j].ski, tmp.ski) > 0)) {
      Eebus.peers[j + 1] = Eebus.peers[j];
      j--;
    }
    Eebus.peers[j + 1] = tmp;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Scan fertig, %d SHIP-Dienst(e) gefunden"), Eebus.peer_count);
  EebusResponsePeers(PSTR(D_PRFX_EEBUS "Scan"));
  MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_STAT, PSTR(D_PRFX_EEBUS "Scan"));
}

/*********************************************************************************************\
 * Kommandos
\*********************************************************************************************/

void EebusResponsePeers(const char *label) {
  Response_P(PSTR("{\"%s\":{\"Found\":%d,\"Peers\":["), label, Eebus.peer_count);
  for (uint32_t i = 0; i < Eebus.peer_count; i++) {
    EebusPeer *p = &Eebus.peers[i];
   // "Mode" ist jetzt die WIRKSAME Betriebsart (Zuordnung ODER aus dem Geraetetyp
   // abgeleitet), "ModeSrc" sagt, woher sie stammt. Die Bedienoberflaeche muss damit nicht mehr
   // selbst raten und kann anzeigen, womit das Geraet tatsaechlich angesprochen wird.
    int pz = EebusModeForSki(p->ski);   // ausdrueckliche Zuordnung (oder -1)
    int pm = (pz >= 0) ? pz : EebusModeFromType(p->ski);
    const char *src = (pz >= 0) ? "zuordnung" : ((pm >= 0) ? "typ" : "global");
    ResponseAppend_P(PSTR("%s{\"Ski\":\"%s\",\"Id\":\"%s\",\"Model\":\"%s\",\"Brand\":\"%s\","
                          "\"Type\":\"%s\",\"Cat\":\"%s\",\"Register\":\"%s\","
                          "\"Ip\":\"%s\",\"Ip6\":\"%s\",\"Host\":\"%s\",\"Port\":%d,\"Path\":\"%s\","
                          "\"Mode\":\"%s\",\"ModeSrc\":\"%s\"}"),
                     (i) ? "," : "", p->ski, p->id, p->model, p->brand,
                     p->type, (p->cat[0]) ? p->cat : "", (p->reg[0]) ? p->reg : "",
                     p->ip, p->ip6, p->host, p->port, p->path,
                     (pm < 0) ? "-" : (pm ? "hems" : "steuve"), src);
  }
  ResponseAppend_P(PSTR("]}}"));
}

// Peer aus der FUNDLISTE waehlen — per SKI-Praefix, per "hm", oder (nur noch als Notnagel)
// per Listennummer. Anders als EebusSelectPeerArg, das eine STEHENDE Verbindung sucht, arbeitet
// dies auf Eebus.peers[] und wird deshalb zum VERBINDEN gebraucht (da steht ja noch nichts).
// Hintergrund (Regel aus der Praxis, zweimal real schiefgegangen): die Listennummer ist KEIN
// Bezeichner — jeder Scan sortiert die Liste neu. stand der Energiemanager auf
// Platz 0 und auf Platz 2. Ein gemerkter Index trifft danach ein anderes Geraet: einmal
// ging ein Limit an die Waermepumpe statt an den Energiemanager, einmal ein Verbinden-Klick.
// Rueckgabe: Index in Eebus.peers[] oder -1 (Fehlermeldung ist dann schon gesetzt).
int EebusPeerIdxArg(const char *arg) {
  if ((nullptr == arg) || ('\0' == arg[0])) {
    ResponseCmndChar_P(PSTR("Ziel noetig: <ski-praefix|hm|idx>")); return -1;
  }
  if ((0 == strcasecmp(arg, "hm")) || (0 == strcasecmp(arg, "auto"))) {
    int found = -1, n = 0;
    for (uint32_t i = 0; i < Eebus.peer_count; i++) {
      if (0 == strcasecmp(Eebus.peers[i].type, "EnergyManagementSystem") ||
          nullptr != strstr(Eebus.peers[i].type, "Manager")) { found = (int)i; n++; }
    }
    if (1 == n) { return found; }
    ResponseCmndChar_P(n ? PSTR("mehrere Energiemanager gefunden - SKI-Praefix angeben")
                         : PSTR("kein Energiemanager in der Fundliste - erst EEBusScan"));
    return -1;
  }
  bool is_ski = false;   // SKIs sind Hex -> enthalten a-f, Indizes nie
  for (const char *c = arg; *c; c++) { if (strchr("abcdefABCDEF", *c)) { is_ski = true; break; } }
  if (is_ski) {
    size_t pl = strlen(arg);
    int found = -1, n = 0;
    for (uint32_t i = 0; i < Eebus.peer_count; i++) {
      if (0 == strncasecmp(Eebus.peers[i].ski, arg, pl)) { found = (int)i; n++; }
    }
    if (1 == n) { return found; }
    ResponseCmndChar_P(n ? PSTR("SKI-Praefix nicht eindeutig - mehr Stellen angeben")
                         : PSTR("keine SKI in der Fundliste passt - erst EEBusScan"));
    return -1;
  }
  int idx = atoi(arg);
  if ((idx < 0) || (idx >= (int)Eebus.peer_count)) {
    ResponseCmndChar_P(PSTR("Index ausserhalb der Fundliste - besser die SKI angeben")); return -1;
  }
  return idx;
}

void CmndEebusScan(void) {
  if (EebusStartScan()) {
    ResponseCmndChar_P(PSTR("Scanning"));   // Ergebnis folgt als EEBusScan-Telegramm
  } else {
    ResponseCmndChar_P((Eebus.scan) ? PSTR("Busy") : PSTR("Failed"));
  }
}

void CmndEebusPeers(void) {
  EebusResponsePeers(PSTR(D_PRFX_EEBUS "Peers"));
}

void CmndEebusCert(void) {
  bool force_new = (2 == XdrvMailbox.payload);
  if (EebusEnsureCert(force_new)) {
   // v194: neue Identitaet sofort ankuendigen, sonst steht bis zum Neustart die ALTE SKI im mDNS
    if (force_new && Eebus.advertised) {
      Eebus.advertised = false;
      mdns_service_remove("_ship", "_tcp");
      EebusMdnsAdvertise();
    }
   // Kennung mitliefern: sie wird aus der SKI abgeleitet und ist das, was eine Gegenstelle in
   // ihrem Geraeteverzeichnis anzeigt — sie gehoert neben die SKI, nicht nur ins Log.
    Response_P(PSTR("{\"%s\":{\"Ski\":\"%s\",\"Id\":\"%s\",\"Serial\":\"%s\",\"New\":%s}}"),
               XdrvMailbox.command, Eebus.own_ski, eebus_adv_id, eebus_adv_serial,
               (force_new) ? "true" : "false");
  } else {
    ResponseCmndChar_P(PSTR("Failed"));
  }
}

/*********************************************************************************************\
 * MEILENSTEIN 2a — SHIP-Transport: TLS + WebSocket + CMI
 *
 * Rolle Client (wir = Energy Guard verbinden zum Geraet). Ablauf:
 *   1. TCP + TLS (mbedTLS roh) mit unserem Client-Zertifikat, authmode NONE (SHIP-Vorgabe)
 *   2. Peer-Zertifikat lesen -> dessen SKI extrahieren (Identitaet des Geraets)
 *   3. WebSocket-Upgrade (Subprotocol "ship")
 *   4. CMI-Handshake (SHIP 13.4.3): binaer [0x00,0x00] senden, [0x00,0x00] erwarten
 * Danach haelt M2a die Verbindung offen (Zustand CmiOk). Der SHIP-JSON-Handshake
 * (Hello/ProtocolHandshake/Pin -> Done) kommt in M2b.
 *
 * WS-Klassifizierer (erstes Byte jeder SHIP-Nutzlast): 0=CMI/Init, 1=Control, 2=Data, 3=End.
\*********************************************************************************************/

enum EebusShipState : uint8_t {
  SHIP_IDLE = 0, SHIP_TLS_OK, SHIP_WS_OK, SHIP_CMI_OK, SHIP_ERROR
};

// M2b: SME-Handshake-Phasen (SHIP 13.4.4). Laeuft ASYNCHRON nach dem CMI
// (100-ms-Polling), weil das Hello-Pending des Peers Minuten dauern kann
// (Nutzer muss unsere SKI im Geraete-UI bestaetigen) — nichts darf blockieren.
enum EebusSmeState : uint8_t {
  SME_OFF = 0,   // kein Handshake aktiv
  SME_HELLO,   // hello(ready) gesendet, warte auf hello(ready) des Peers
  SME_PROT,   // announceMax gesendet, warte auf select
  SME_PIN,   // select bestaetigt + pinState:none gesendet, warte auf Peer-pinState (Referenz-Umsetzung-Gate)
  SME_ACCESS,   // accessMethodsRequest gesendet, warte auf Peer-accessMethods (Referenz-Umsetzung-Gate)
  SME_DONE,   // Handshake komplett -> Datenphase (SPINE = M3)
  SME_FAIL,
  SME_CMI   // v242: CMI gesendet, warte auf die CMI des Servers (ohne Blockieren, SHIP Z. 2068-2084)
};

#define EEBUS_CMI_TIMEOUT_MS         15000UL   // v242: CmiTimeout (SHIP Z. 2084: 10-30 s)
#define EEBUS_SME_PIN_TIMEOUT_MS     10000UL   // v242: Peer-pinState (SHIP Z. 2663-2664)
#define EEBUS_WS_PONG_MS             10000UL   // v242: Pong-Frist (SHIP Z. 854-855)
#define EEBUS_TRUST_FILE        "/eebus_trust.txt"   // v242: erlaubte Anrufer (EEBusTrust)
#define EEBUS_SME_HELLO_TIMEOUT_MS   60000UL   // SHIP SME_INIT_TIMEOUT (wird bei Pending verlaengert)
#define EEBUS_SME_HELLO_MAX_MS      600000UL   // Obergrenze Pairing-Wartezeit gesamt (10 min).
   // Muss den kompletten Bedien-UI-Ablauf abdecken:
   // Verbindungsaufbau (bis 60 s) + Freigabefenster (2 min),
   // plus Luft, falls die Freigabe erst spaeter angestossen wird.
#define EEBUS_SME_PROT_TIMEOUT_MS    10000UL   // SHIP PROTOCOL_HANDSHAKE_TIMEOUT
#define EEBUS_SME_GATE_TIMEOUT_MS     5000UL   // Wartezeit auf Peer-pinState/-accessMethods;
   //       laeuft sie ab -> trotzdem in die Datenphase (Fallback)
#define EEBUS_SHIP_LOGFILE           "/eebus_ship.log"
// M3: SPINE-Datenphase
#define EEBUS_WS_REST_MS          10000UL   // v241: so lange auf den Rest eines Frames warten (SHIP 8.3/8.4: 1+3+6 s TCP-Wiederholung)
#define EEBUS_WSRAW_SIZE          (EEBUS_SPINE_RXBUF + 16)   // v241: Rohpuffer = ein ganzer Frame samt Kopf
#define EEBUS_ABO_MAX                  12   // v241: gemerkte eigene Abos je Verbindung
#define EEBUS_GB_MAX                   10   // v246: Abos/Bindings der Gegenstelle bei UNS je Verbindung (SPINE TS SUBS_001/BIND_001)
#define EEBUS_ABO_POLL_MS          30000UL  // v241: Lesetakt fuer abgelehnte Abos (LPC TS Z. 1466)
#define EEBUS_HB_LUECKE_MS        110000UL  // v241: Herzschlag-Pause, nach der die Anlage im Failsafe sein kann (LPC-911: 120 s)
#define EEBUS_AC_ENDLOS_MS        120000UL  // v241: Wiederaufbau nach dem Neustart gibt nicht auf, neuer Durchlauf alle 120 s
#define EEBUS_SPINE_RXBUF            32768   // PSRAM-Empfangspuffer fuer SPINE-Datagramme. 8192 war ZU KLEIN: Energiemanager-DetailedDiscovery = 9713 B, Waermepumpen-Gateway = 10969 B -> WsRecv (Z.913) verwarf den Frame STILL ("WS-Nachricht > 8191 B, verworfen", nur DEBUG) -> wir bekamen die Peer-Discovery NIE. Live bewiesen . 32 KB deckt reale Discovery + Reserve (PSRAM, pro Slot).
#define EEBUS_SPINE_VERSION          "1.3.0"   // SUPPORTED_SPINE_VERSION (wie Ladestation/Energiemanager/Waermepumpen-Gateway)
#define EEBUS_SPINE_KEEPALIVE_MS     8000UL   // Auto-Reconnect, wenn nach Done die Leitung abfaellt
   // Hat die Gegenstelle "pending" gemeldet, wartet dort ein MENSCH auf eine Freigabe
   // (Portal/Hersteller-App). Dichtes Wiederholen bringt dann nichts, erzeugt aber
   // "cmi resp n=-1" — und DAS legt den Slot bis zum naechsten Neustart lahm.
#define EEBUS_PENDING_RETRY_MS     300000UL   // 5 min statt 8 s, solange der Peer "pending" meldet

// MULTI-CONNECTION: bis zu 6 SHIP-Verbindungen GLEICHZEITIG (v199; vorher 3: Ladestation + Waermepumpen-Gateway + Energiemanager).
// Muster: ein Verbindungs-Slot pro Peer; ALLE bestehenden Funktionen arbeiten unveraendert
// ueber den globalen "aktuellen Slot"-Zeiger ESp (frueher: eine globale EShip-Struktur).
// Die Schleifen (Poll/Heartbeat/Keep-Alive/Teardown) setzen ESp pro Slot — alles laeuft
// im selben Arduino-Loop-Task, daher ist der Zeiger race-frei.
#define EEBUS_ABGELEHNT_WDH_MS 60000UL     // v214: abgelehnte Vorgabe nach 60 s erneut (Sicherung wiederholen)
#define EEBUS_SICHERUNG_DIREKT_MS 5000UL   // v208: direkt gesteuertes Geraet - es gibt keinen Anmelde-Weg, auf den gewartet
                                          // werden muesste; 5 s nur, damit Binden/Abos nach der Discovery durch sind
#define EEBUS_SICHERUNG_MS 30000UL   // v203: 30 s (v200: 60 s) - LPC [LPC-913]: nach Wiederkehr der Verbindung SHALL der Energy Guard
                                      // Herzschlag + Grenze INNERHALB 60 s senden; 30 s lassen Zeit fuer Binden, Herzschlag, Write
#define EEBUS_MAX_CONN 6   // v199: Direktsteuerung bis zu 5 SteuVE + 1 Reserve (s. Kopf v199)

typedef struct {
  BearSSL::WiFiClientSecure_light *client = nullptr;
  uint8_t *cert_der = nullptr;   // bleibt allokiert waehrend der Verbindung (BearSSL zeigt drauf)
  size_t   cert_len = 0;
  uint8_t  key_scalar[32] = { 0 };   // roher EC-Private-Skalar (secp256r1), BearSSL-Format
  br_x509_certificate br_cert[1];   // Client-Zertifikat fuer BearSSL
  br_ec_private_key    br_key;   // Client-Key fuer BearSSL
  bool     active = false;
  uint8_t  state = SHIP_IDLE;
  char     peer_ski[41] = { 0 };   // Peer-Identitaet (aus mDNS; BearSSL-Light gibt Peer-Cert nicht her)
  char     peer_ip[46] = { 0 };   // v185: 46 statt 16 — IPv6
  uint16_t peer_port = 0;
  char     err[80] = { 0 };
   // M2b: SME-Handshake (asynchron)
  uint8_t  sme = SME_OFF;
  uint32_t sme_deadline = 0;   // millis()-Deadline der aktuellen Phase
  uint32_t sme_hello_start = 0;   // Beginn der Hello-Phase (Obergrenze Pairing)
  bool     sme_pending_logged = false;
  char     peer_id[64] = { 0 };   // AccessMethods-id des Peers (SHIP 13.4.6)
   // M3: SPINE-Datenphase
  uint8_t *rxbuf = nullptr;   // grosser Empfangspuffer (PSRAM) fuer SPINE-Datagramme
  size_t   rxbuf_size = 0;
  uint8_t *wsraw = nullptr;   // v241: Rohpuffer des Empfangs (PSRAM), Frames ueber mehrere Durchlaeufe
  size_t   wsraw_len = 0;
  uint32_t wsraw_at = 0;      // v241: millis() des zuletzt empfangenen Bytes
  size_t   wsf_len = 0;       // v241: Laenge der zerstueckelten Nachricht, die gerade zusammengesetzt wird
  bool     wsf_on = false;
  bool     wsf_drop = false;
  char     peer_dev[80] = { 0 };   // SPINE-Geraeteadresse des Peers (aus addressSource)
  uint32_t spine_ctr = 1;   // unser SPINE-msgCounter
  bool     disco_answered = false;   // haben wir dem Peer schon unsere Detailed Discovery geliefert?
  bool     peer_disco_read = false; // haben wir SEINE Detailed Discovery schon abgefragt (Gegenseitigkeit)?
  bool     we_nm_subscribed = false;// haben WIR das Peer-NodeManagement abonniert? (erst NACH disco_answered senden -> "invalid addresses"-Fix)
  uint32_t disco_fallback_at = 0;   // Done+3s — dann Discovery-Read ADRESSLOS (Ladestation-Deadlock-Bruch)
  int      peer_idx = -1;   // Index in Eebus.peers (fuer Auto-Reconnect)
  uint32_t last_rx = 0;   // millis() der letzten empfangenen Nachricht (Keep-Alive)
  bool     keepalive = false;   // Verbindung dauerhaft halten (nach Done wieder aufbauen)?
  uint32_t reconnect_at = 0;   // millis()-Zeitpunkt fuer den naechsten Auto-Reconnect (0=aus)
  uint8_t  ka_try = 0;         // v180: Wiederaufbau-Versuche seit der letzten stehenden Verbindung
  uint8_t  ka_scans = 0;       // v180: Scans seit der letzten stehenden Verbindung (erster sofort, weitere 60 s)
  uint32_t ka_lost_at = 0;     // v229: seit wann die Verbindung weg ist (0 = steht bzw. noch nicht gezaehlt)
  uint32_t ka_last_try = 0;    // v229: letzter echter Verbindungsversuch (TCP/TLS)
  uint32_t ka_last_scan = 0;   // v229: letzte mDNS-Suche fuer diesen Platz
  EebusPeer ka_peer;           // v229: zuletzt benutzte Adresse - Rueckfall, wenn die Suche die Gegenstelle nicht liefert
  bool     ka_peer_ok = false; // v229: ka_peer gueltig
  volatile uint8_t ka_ping_res = 0;   // v229: 0 nichts, 1/0x11 laeuft, 2 fertig mit Antwort, 3 fertig ohne (setzt die Ping-Task)
  void    *ka_ping_h = nullptr;       // v230: offene Ping-Sitzung (esp_ping_handle_t), geloescht nur im Tasmota-Ablauf
  bool     ka_ping_seen = false;     // v230: Gegenstelle hat schon einmal auf Ping geantwortet -> Versuch ohne Antwort nur alle 120 s
  uint32_t ws_ping_next = 0;   // millis() fuer den naechsten WebSocket-Ping (0 = noch nicht gesetzt)
  uint32_t pong_wait_at = 0;   // v242: millis() des Pings, auf dessen Pong wir warten (0 = keiner offen)
  uint32_t conn_at = 0;        // v242: millis() des TLS-Aufbaus (Doppelverbindung: welche ist aelter)
  uint32_t dbl_at = 0;         // v242: seit wann diese Verbindung doppelt ist
   // mitgelesene Nutzdaten fuer die Anzeige. Alle Werte als Zahl+Zehnerexponent wie auf dem Draht
   // ("scaledNumber"), damit nichts gerundet wird; die Oberflaeche rechnet zusammen.
   // Zuordnung Kennung->Bedeutung kommt aus measurementDescriptionListData (scopeType), NICHT geraten.
  int8_t   m_scope[24] = { 0 };   // je measurementId: 1=acPowerTotal 2=gridFeedIn 3=gridConsumption
   //                   4=acCurrent 5=acVoltage 6=frequency 0=unbekannt
  long     m_val[7] = { 0 };   // letzter Wert je Bedeutung (Index = Code oben)
  int8_t   m_sc[7]  = { 0 };   // zugehoeriger Zehnerexponent
  bool     m_have[7] = { false };   // schon einmal empfangen?
   // v197: Strom (Code 4) und Spannung (Code 5) je Phase L1-L3. Die Phase ist die Reihenfolge der
   // Kennungen gleicher Bedeutung in der Beschreibung (erste = L1) — so liefern es HEMS und Zaehler.
  int8_t   m_phase[24] = { 0 };   // v243: 0-2 = L1-L3 (Strom / Spannung gegen N), 3-5 = L1-L2, L2-L3, L3-L1
  uint32_t m_ph_desc = 0;         // v243: Bit je measurementId - Phase kommt aus der ParameterDescription (nicht geraten)
  long     mll_val[3] = { 0 };    // v243: Leiter-Leiter-Spannungen ([MGCP-061/4-6])
  int8_t   mll_sc[3] = { 0 };
  bool     mll_have[3] = { false };
  bool     meas_prod_dir = false; // v243: positiveEnergyDirection "produce" -> Vorzeichen umkehren
  int      gcp_uc_ent = -1;       // v243: Entity des GridConnectionPoint laut Use-Case-Auskunft (-1 = unbekannt)
  int8_t   lpp_uc = -1;           // v243: Anlage bietet LPP als ControllableSystem an: -1 unbekannt, 0 nein, 1 ja
  uint32_t sess_dir_w[2] = { 0, 0 };   // v243: erster Grenz-Write dieser Sitzung je Richtung (LPC-/LPP-913)
  bool     lpp913_warn = false;   // v243
  uint8_t  fs_ro = 0;             // v243: Bit 1/2/3 = Failsafe Bezug/Dauer/Einspeisung laut Anlage nicht beschreibbar
  long     m3_val[2][3] = { { 0 } };
  int8_t   m3_sc[2][3]  = { { 0 } };
  bool     m3_have[2][3] = { { false } };
   // ⚠ EINE measurementId ist NUR INNERHALB EINER EINHEIT eindeutig. Am 29.07. real passiert: eine
   // Leseanfrage an die Batterie (Einheit 6.1) lieferte Id 3 = Ladezustand 99 % — und weil die
   // Tabelle oben nur nach Id einsortierte, stand in der Anzeige "Spannung 99 V", dazu die
   // Batterieleistung als Frequenz und der Ladezaehler als Bezugszaehler. Beides gehoerte dem
   // Netzanschlusspunkt. Deshalb: die Felder oben gehoeren AUSSCHLIESSLICH der Einheit, die den
   // Netzanschlusspunkt fuehrt; jede andere Einheit bekommt ihren eigenen Platz.
  int      meas_ent = -1;   // Einheit des Netzanschlusspunkts (-1 = noch nicht erkannt)
  int      mo_ent[16] = { 0 };        // Werte anderer Einheiten: Einheit
  uint8_t  mo_id[16]  = { 0 };        //   Kennung innerhalb dieser Einheit
  long     mo_v[16]   = { 0 };        //   Wert
  int8_t   mo_sc[16]  = { 0 };        //   Zehnerexponent
  bool     mo_have[16] = { false };   //   schon ein Wert eingetroffen?
  char     mo_scope[16][22] = { { 0 } };   // Bedeutung, wie die Gegenstelle sie nennt
  char     mo_unit[16][6] = { { 0 } };     // Einheit, wie die Gegenstelle sie nennt (W, Wh, pct)
  uint8_t  mo_n = 0;
   // DeviceConfig: die Schluessel heissen je Entity anders (Failsafe auf der Limit-Entity,
   // pvCurtailmentLimitFactor am Netzanschlusspunkt) — beide fuehren keyId 0. Deshalb die Bedeutung
   // je QUELL-ENTITY merken, sonst landet der Prozentwert im Failsafe-Feld.
  int      cfg_ent[4] = { -1, -1, -1, -1 }; // v243: bis zu vier DeviceConfig-Entities (vorher zwei)
   // v247: Paare keyId -> Bedeutung (bis 8 je Entity). Vorher war die keyId der Index (nur 0..7).
  uint32_t cfg_kid[4][8] = { { 0 } };
  int8_t   cfg_code[4][8] = { { 0 } };   // je Paar: 1=failsafeConsW 2=failsafeDur 3=failsafeProdW 4=pvCurtailPct
  uint8_t  cfg_n[4] = { 0, 0, 0, 0 };   // belegte Paare je Entity
  long     fs_cons = 0;  int8_t fs_cons_sc = 0;  bool fs_cons_ok = false;
  long     fs_prod = 0;  int8_t fs_prod_sc = 0;  bool fs_prod_ok = false;
  char     fs_dur[16] = { 0 };   // Mindestdauer als Norm-Text, z.B. "PT2H"
  long     plf = 0;      int8_t plf_sc = 0;      bool plf_ok = false;   // Begrenzungsfaktor in Prozent
   // Nennleistungen aus electricalConnectionCharacteristicListData. DAS ist die norm-richtige
   // Bezugsgroesse fuer eine Prozent-Vorgabe des Netzbetreibers (LPP-Szenario 4, Tab. 18/19) — bisher
   // haben wir dafuer den Failsafe-Wert Erzeugung genommen, was plausibel, aber ungeprueft war.
  long     pnom_prod = 0;  int8_t pnom_prod_sc = 0;  bool pnom_prod_ok = false;   // powerProductionNominalMax
  long     pnom_cprod = 0; int8_t pnom_cprod_sc = 0; bool pnom_cprod_ok = false;  // v196: contractualProductionNominalMax [LPP-042]
  long     pnom_ccons = 0; int8_t pnom_ccons_sc = 0; bool pnom_ccons_ok = false;  // v196: contractualConsumptionNominalMax [LPC-042]
  long     pnom_cons = 0;  int8_t pnom_cons_sc = 0;  bool pnom_cons_ok = false;   // powerConsumptionNominalMax
   // Adressen der beiden Server-Features merken, damit sich Messwerte und Kenngroessen SPAETER
   // erneut abfragen lassen — bis hierher wurden sie ausschliesslich einmal beim Verbinden gelesen.
   // Wer wissen will, ob die Gegenstelle Stroeme und Spannungen ueberhaupt fuehrt, muss nachfragen
   // koennen, ohne die Verbindung neu aufzubauen.
   // NICHT nur eine Adresse, sondern ALLE. Die Gegenstelle fuehrt mehrere Instanzen — im
   // Strukturabzug vom vier ElectricalConnection- und drei Measurement-Server, verteilt ueber
   // Netzanschlusspunkt, Speicher und Erzeugungsanlage. merkte sich die ZULETZT gesehene und
   // fragte damit Entity 8 (Netzanschlusspunkt); die Nennleistung der ERZEUGUNG liegt dort nicht,
   // die Gegenstelle antwortete folgerichtig mit errorNumber 6 (CommandNotSupported).
   // Eine Nennleistung gehoert zur Erzeugungsanlage, nicht zum Anschlusspunkt — also alle fragen.
  int      m_ent[EEBUS_ADR_MAX]  = { -1, -1, -1, -1 }, m_feat[EEBUS_ADR_MAX]  = { -1, -1, -1, -1 },
           m_cli[EEBUS_ADR_MAX]  = { -1, -1, -1, -1 }; int m_n = 0;   // Measurement-Server
   // Gezieltes Auslesen EINES Features (EEBusRead): worauf wir warten und was zurueckkam.
   // Die Selbstauskunft sagt nur, WO eine Gegenstelle etwas fuehrt — nicht, WAS dort steht.
   // Genau diese Luecke schliesst der Puffer: Read absetzen, Antwort von GENAU dieser Adresse
   // aufheben, auf Nachfrage ausgeben. Ohne ihn landet die Antwort nur im Log.
  int      rd_ent = -1, rd_feat = -1;   // erwartete Absenderadresse (-1 = kein Read offen)
  char    *rd_buf = nullptr;            // Rohantwort (PSRAM)
  uint32_t rd_at  = 0;                  // millis() des Sendens (fuer "noch keine Antwort")
   // VORGEMERKTER Read: das Kommando merkt nur vor, gesendet wird im Sekunden-Tick.
   // Grund (gemessen 29.07.): wird waehrend der Kommandobearbeitung gesendet, kommt der gesendete
   // Rahmen VERMISCHT mit der Kommandoantwort aus /cm heraus — kein gueltiges JSON mehr, und die
   // Bedienoberflaeche meldete "nicht angenommen", obwohl die Anfrage sauber hinausging. Wer die
   // Antwort baut, darf zwischendurch nichts senden.
  int      rd_pend_ent = -1, rd_pend_feat = -1;   // -1 = nichts vorgemerkt
  char     rd_pend_fn[64] = { 0 };                // Lesefunktion des vorgemerkten Reads
  char     rd_pend_elist[16] = { 0 };             // Ziel-Entity als Liste ("6,1" = Untereinheit)
  int      rd_pend_sel = -1;                      // Selektor: genau diese Kennung lesen (-1 = alles)
  uint32_t rd_mc = 0;                             // msgCounter des gesendeten Reads (ordnet die Ablehnung zu)
  int      ec_ent[EEBUS_ADR_MAX] = { -1, -1, -1, -1 }, ec_feat[EEBUS_ADR_MAX] = { -1, -1, -1, -1 },
           ec_cli[EEBUS_ADR_MAX] = { -1, -1, -1, -1 }; int ec_n = 0;   // ElectricalConnection-Server
  bool     ec_char[EEBUS_ADR_MAX] = { false, false, false, false };   // bietet Kenngroessen an?
  int      pnom_prod_ent = -1;   // aus welcher Entity kam die Nennleistung Erzeugung?
   // Ist-Zustand beider Grenzen des Peers (0 = consume/§14a, 1 = produce/§9)
  int8_t   lim_act[2] = { -1, -1 }; // -1 unbekannt, 0 inaktiv, 1 aktiv
  long     lim_val[2] = { 0, 0 };
  int8_t   lim_sc[2]  = { 0, 0 };
   // die vom Peer ZURUECKGEMELDETE Geltungsdauer je Grenze (aus dem Read-Back). Damit laesst
   // sich ohne Frame-Lesen sehen, ob er eine mitgegebene Befristung uebernommen hat — und ob dort
   // eine abgelaufene Restlaufzeit steht (die beruechtigte "PT0S", die die Begrenzung sofort wieder
   // deaktiviert und gegen die der Loeschfilter aus einer frueheren Fassung arbeitet).
  char     lim_dur[2][16] = { { 0 }, { 0 } };
   // die Restlaufzeit LAEUFT jetzt mit. Bis stand in "Dauer" der Text aus der letzten
   // Rueckmeldung und fror dort ein — gemessen: neun Abfragen ueber 92 s zeigten
   // unveraendert "PT2M49S", noch fuenf Sekunden vor dem Ablauf. Das ist nicht nur unbeweglich,
   // sondern irrefuehrend: wer hinsah, las knapp drei Minuten Rest, obwohl noch gut eine
   // Minute uebrig war. Deshalb merken wir uns den EMPFANGSZEITPUNKT und die damals genannte
   // Restlaufzeit und rechnen bei jeder Abfrage neu. Das ist eine LOKALE HOCHRECHNUNG — sie stimmt
   // sich bei jeder neuen Rueckmeldung der Gegenstelle wieder mit deren Wahrheit ab (die schickt beim
   // Deaktivieren von selbst eine Meldung, binnen sieben Sekunden). Kein neuer Netzverkehr.
  long     lim_dur_s[2]  = { -1, -1 };   // Restlaufzeit in s bei Empfang; -1 = keine Dauer bekannt
  uint32_t lim_dur_at[2] = { 0, 0 };   // millis() des Empfangs
  bool     teardown = false;   // Client-Freigabe steht an (verzoegert, NIE aus dem Callback)
  bool     send_broken = false;   // v187: ein Frame ging nur teilweise raus -> nichts mehr senden, Abbau laeuft
  uint32_t hb_counter = 1;   // DeviceDiagnosis-Heartbeat-Zaehler
  uint32_t hb_tx_at = 0;     // v231 (06.11., meierchen006): Zeitpunkt unseres letzten Herzschlags an die Gegenstelle (EEBusStatus "HerzschlagGesendetS")
  uint32_t hb_next = 0;   // millis()-Zeitpunkt des naechsten Heartbeat-Sendens
  bool     peer_subscribed = false; // hat der Peer unser NodeManagement abonniert?
  bool     lc_sub = false;   // hat der Peer unser LoadControl (ent1/feat6) abonniert?
  int      lc_cli_ent = 0;   // Client-Adresse des LoadControl-Abonnenten (fuer NOTIFY)
  int      lc_cli_feat = 0;
  bool     hb_sub = false;   // hat der Peer unser DeviceDiagnosis (ent1/feat2) abonniert?
  int      hb_cli_ent = 0;   // Client-Adresse des Heartbeat-Abonnenten (aus clientAddress)
  int      hb_cli_feat = 0;
   // --- Herzschlag der GEGENSTELLE: Ueberwachung in die andere Richtung ----------------------
   // Zeitlich ueberwacht wurde bisher nur last_rx = "wann kam zuletzt IRGENDEINE Nachricht?".
   // Als Mass fuer die Beziehung ist das untauglich: ein Energiemanager schickt zwei
   // Messwert-Meldungen je Sekunde, last_rx ist damit nie alt — auch dann nicht, wenn die
   // Gegenstelle uns laengst abgemeldet hat. Wir haben die Fuetterung gemessen und daraus auf
   // die Beziehung geschlossen. Der Herzschlag der Gegenstelle traegt dagegen eine ZUSAGE
   // ("heartbeatTimeout", z.B. PT2M): "spaetestens dann hoerst du wieder von mir."
  uint32_t peer_hb_at = 0;      // millis() des zuletzt EMPFANGENEN Herzschlags der Gegenstelle
  uint32_t peer_hb_ctr = 0;     // dessen Zaehler (Anzeige/Diagnose)
  uint16_t peer_hb_tmo_s = 0;   // von der Gegenstelle ZUGESAGTE Frist in s (aus heartbeatTimeout)
  bool     peer_hb_lost = false;   // Frist gerissen und bereits gemeldet (nicht wiederholen)
  uint16_t hb_lost_cnt = 0;     // wie oft die Frist bisher riss (Vorfallszaehler)
  char     hb_lost_at[24] = { 0 };   // Zeitpunkt des letzten Vorfalls, aus der Ortszeit
   // LPC-Steuerbox-Schreib-Sequenz an die LoadControl des Peers
  uint8_t  lpc_state = LPC_IDLE;
  uint32_t lpc_value = 0;   // Zielwert als BETRAG in W (bei Freigabe egal; Vorzeichen aus lpc_dir)
  uint32_t lpc_val_dir[2] = { 0, 0 };   // 06.11.: letzter Wert je Richtung [0] Bezug §14a, [1] Einspeisung §9 (fuer Freigaben ohne Wert)
  bool     lpc_active_wish = false; // true = Limit setzen, false = freigeben (isLimitActive:false)
   // Richtung des laufenden Limit-Auftrags. 0 = consume (§14a EnWG, Bezugsbegrenzung),
   // 1 = produce (§9 EEG, Einspeisebegrenzung). Beide Grenzen liegen im SELBEN LoadControl-Feature des
   // Peers und unterscheiden sich nur in der limitId (Peer-Beschreibung: limitId 0 = consume,
   // limitId 1 = produce) -> es braucht KEINE zweite Verbindung, kein zweites Binding, kein zweites Abo.
   // Die Norm (LPP Kap. 3.3) haengt Discovery/Binding/Subscription am Feature, nicht am Use-Case.
   // Der Peer haelt beide Grenzen unabhaengig voneinander; unser partial-Write auf die eine ruehrt die
   // andere nicht an -> §14a und §9 koennen gleichzeitig stehen.
  uint8_t  lpc_dir = 0;   // 0 = consume (LPC/§14a), 1 = produce (LPP/§9 EEG)
  uint8_t  lpc_reg_step = 0;   // Anmeldung: 0=aus, 1=Freigabe consume faellig, 2=produce faellig, 3=fertig
   // v176: Wiederholung einer abgelehnten Anmeldung (siehe EEBUS_REG_RETRY_MS).
  int8_t   reg_dir_running = -1;   // Richtung der gerade laufenden Anmelde-Kette (0/1), -1 = keine
  uint8_t  reg_failed = 0;         // Bit 0 = §14a, Bit 1 = §9: Anmeldung wurde abgelehnt, Wiederholung faellig
  uint8_t  reg_retry_cnt = 0;      // bisherige Wiederholungen dieser Verbindung
  uint32_t reg_retry_at = 0;       // millis() der naechsten Wiederholung, 0 = nicht geplant
   // Geltungsdauer DIESES Schreibauftrags in Sekunden (0 = ohne timePeriod).
   // Bewusst je Auftrag statt global: so bleibt der bewiesene §14a-Frame zeichengleich, solange
   // niemand eine Dauer angibt — und wer eine angibt, bekommt sie auch im HEMS-Betrieb.
  long     lpc_dur_s = 0;
  int      lpc_limit_id = -1;   // limitId aus der LoadControl-Beschreibung des Peers (richtungsabhaengig)
  int      lpc_lid_dir[2] = { -1, -1 };   // v184: limitId je Richtung (0 Bezug, 1 Einspeisung) aus derselben Beschreibung
  int      lpc_peer_ent = 1;   // LoadControl-Feature des Peers (aus Discovery; Ladestation: ent1/feat6)
  int      lpc_peer_feat = 6;
  int      lpc_peer_dcfg_feat = 24; // DeviceConfiguration-SERVER-Feature des Peers AUS DISCOVERY
   // (Energiemanager=24, Referenz-Umsetzung-HEMS=3, andere Hersteller=?). KEIN Hardcode mehr;
   // Fallback 24 nur wenn Discovery fehlt. -1 = keine DeviceConfig -> Read ueberspringen.
  bool     lpc_bound = false;   // Binding zur Peer-LoadControl steht
  bool     lpc_onboard_only = false;// Kette laeuft bis READCFG, aber NICHT schreiben (Onboarding beim Connect)
  bool     lpc_onboarded = false;   // Onboarding fertig (gebunden+gelesen, HB laeuft) -> Write via EEBusLpc
  uint32_t lpc_deadline = 0;   // Timeout der aktuellen LPC-Phase (millis)
  uint32_t lpc_verify_at = 0;   // millis() fuer verzoegerten/nachgepollten Read-Back (0=aus)
  uint8_t  lpc_verify_tries = 0;   // Zaehler der bisherigen Read-Back-Versuche
   // Der Fehlschlag kam aus der RUECKLESUNG (nicht aus einer Ablehnung der Gegenstelle).
   // Nur dann darf eine spaeter eintreffende Meldung das Ergebnis noch richtigstellen. Belegt am
   //: der Status meldete "Limit NICHT aktiv (abgelehnt?)", waehrend die Nutzdaten in
   // derselben Sekunde "aktiv, PT2M49S" zeigten UND das Portal die Begrenzung protokollierte — die
   // Gegenstelle hatte nur spaeter geantwortet als unsere Nachpoll-Versuche reichten.
  bool     lpc_verify_failed = false;
  uint8_t  lpc_sel_step = 0;   // HEMS: Fortschritt der Selektor-Reads (0=limitId0 offen, 1=limitId1 offen)
  int      lpc_fs_val_key = -1;   // HEMS: keyId von failsafeConsumptionActivePowerLimit (aus DeviceConfig-Description)
  int      lpc_fs_dur_key = -1;   // HEMS: keyId von failsafeDurationMinimum
  int      lpc_fs_prod_key = -1;  // v196: keyId von failsafeProductionActivePowerLimit
  uint8_t  lpc_fs_step = 0;   // v196: Failsafe-Write-Fortschritt (0=nichts, 1=Bezug, 2=Einspeisung, 3=Dauer gesendet)
  bool     lpc_fs_done = false;   // v196: aktuelle Failsafe-Werte auf dieser Verbindung geschrieben -> nicht erneut
  uint8_t  lpc_write_tries = 0;   // HEMS: Zaehler der err7-Retries (Norm LPC-906/914)
  uint32_t lpc_write_retry_at = 0;   // HEMS: millis() fuer den naechsten HB+Write-Retry (0=aus)
  bool     lpc_rej_late = false;     // v179: Auftrag endete mit err7 -> spaeter nachlesen
  uint8_t  late_rd_n = 0;            // v179: Zahl der schon gesendeten Nachlesungen
  uint32_t late_rd_at = 0;           // v179: millis() der naechsten Nachlesung (0 = keine)
  uint32_t late_rd_base = 0;         // v179: millis() des endgueltigen err7
  bool     lpc_our_limit = false;   // haben WIR ein Limit an diesem Peer aktiv gesetzt? (Freigabe-Pflicht)
  uint32_t sess_at = 0;             // v200: millis() des Onboardings dieser Sitzung (0 = noch nicht)
  bool     sess_vorgabe = false;    // v200: in dieser Sitzung schon eine Vorgabe (Freigabe/Grenze) geschrieben?
  uint8_t  sich_stufe = 0;          // v200: Sicherung 0 = noch nicht geprueft, 1 = geprueft
  uint32_t sich_wdh_at = 0;         // v214: Sicherung nach Ablehnung erst ab diesem millis() wiederholen (0 = sofort)
  bool     wdh_offen = false;       // v237: eine Wiederholung ist vorgemerkt (eigene Markierung - eine andere Vorgabe, z. B. die
                                    //       Freigabe der anderen Richtung, setzt sess_vorgabe und loeschte sie bisher still)
  uint32_t bind_mc = 0;             // v240: msgCounter unseres LoadControl-Bindings (0 = keins offen)
  uint32_t fs_wdh_at = 0;           // v239: Failsafe-Werte nach einem NACK erst ab diesem millis() erneut (0 = sofort)
  bool     fs_nack = false;         // v239: in der laufenden Failsafe-Kette wurde ein Wert abgelehnt
  uint32_t sess_write_at = 0;       // v239: erste Vorgabe (Write) dieser Sitzung - Pruefung LPC-913
  uint8_t  abo_n = 0;               // v241: eigene Abos dieser Sitzung (LPC TS Z. 1466, SPINE Z. 2406-2409)
  uint8_t  abo_typ[EEBUS_ABO_MAX] = { 0 };   // = unsere Client-Feature-Nummer: 1 DeviceDiagnosis 2 LoadControl 3 DeviceConfiguration 4 ElectricalConnection 6 Measurement
  int16_t  abo_ent[EEBUS_ABO_MAX] = { 0 };
  int16_t  abo_feat[EEBUS_ABO_MAX] = { 0 };
  uint32_t abo_mc[EEBUS_ABO_MAX] = { 0 };    // msgCounter der Abo-Anfrage (0 = keine Antwort offen)
  uint8_t  abo_st[EEBUS_ABO_MAX] = { 0 };    // 0 offen, 1 bestaetigt, 2 abgelehnt -> regelmaessig lesen
  uint32_t abo_poll_at = 0;         // v241: naechstes Lesen der abgelehnten Abos
  uint8_t  gb_n = 0;                // v246: Abos (typ 0) / Bindings (typ 1) der Gegenstelle bei UNS (wir = Server)
  uint8_t  gb_typ[EEBUS_GB_MAX] = { 0 };
  int16_t  gb_ce[EEBUS_GB_MAX] = { 0 };   // Client-Entity/-Feature (Gegenstelle), -1 = nicht angegeben
  int16_t  gb_cf[EEBUS_GB_MAX] = { 0 };
  int16_t  gb_se[EEBUS_GB_MAX] = { 0 };   // Server-Entity/-Feature (bei uns)
  int16_t  gb_sf[EEBUS_GB_MAX] = { 0 };
  uint16_t mrd_s = 0;               // v241: groesste maxResponseDelay der Anlage (LoadControl/DeviceConfiguration), 0 = Vorgabe
  uint32_t ig210_at = 0;            // v241: letzte Grenze (Hinweis LPC IG 2.10)
  bool     lpc913_warn = false;     // v239: LPC-913-Frist dieser Sitzung schon als verfehlt gemeldet
  bool     hbabo_warn = false;      // v239: "Anlage hat unseren Herzschlag nicht abonniert" schon gemeldet
  uint8_t  wdh_n = 0;               // v237: Wiederholungen ohne Bestaetigung (Wartezeit 60 s x (n+1), hoechstens EEBUS_WDH_MAX_MS)
  bool     best_akt[2] = {false, false};   // v237: letzte Grenze dieser Richtung wurde BESTAETIGT (nur dann ist ein Abbruch moeglich)
  bool     hb_zurueck = false;      // v225: Herzschlag kam nach einem Ausfall zurueck, im naechsten Takt auswerten
  bool     frei_offen[2] = {false, false};   // v225: je Richtung eine Freigabe, die noch nicht bestaetigt ist
  uint8_t  sich_plan[2] = {0, 0};   // v200: je Richtung 0 = nichts, 1 = Freigabe schreiben, 2 = eigene Grenze erneut schreiben
  char     own_ski[41] = "";        // v200: fuer diese SKI gilt die gemerkte eigene Grenze (bleibt ueber Wiederverbindungen)
  bool     own_akt[2] = {false, false};   // v200: eigene Grenze je Richtung (0 Bezug, 1 Einspeisung) aktiv
  uint32_t own_w[2] = {0, 0};             // v200: ihr Wert (W)
  uint32_t own_ende[2] = {0, 0};          // v200: millis()-Ende, 0 = unbefristet
  char     lpc_result[56] = { 0 };   // letztes Ergebnis (Web/Status)
  bool     lb_conn = false;          // v215: Betriebslogbuch - zuletzt protokollierter Verbindungszustand
  char     lb_last[80] = { 0 };      // v215: zuletzt protokollierte Vorgabe (gleiche Wiederholungen nicht erneut)
  char    *lpc_disco = nullptr;   // Kopie der Peer-Discovery (PSRAM) fuer discovery-getriebenes Onboarding
   // Kopie der Use-Case-Auskunft des Peers. Sie kommt beim Onboarding als Antwort auf unseren
   // Read herein und wurde bisher NUR gelesen, um die Verbindung zu vervollstaendigen — der Inhalt
   // wanderte in den Papierkorb. Darin stehen die Actors und die Use Cases der Gegenstelle
   // (am Energiemanager nachgewiesen: 7 Actors, 8 Use Cases), also genau die beiden
   // Klapplisten, die in der Bedienoberflaeche leer blieben. Kein neuer Netzverkehr noetig.
  char    *lpc_uc = nullptr;
   // Etappe 4: Slot gehoert zu einer EINGEHENDEN Server-Verbindung (ESrv). TX laeuft
   // dann ueber EebusSrvWsSendOp (unmaskiert), RX kommt aus der ESrv-Pumpe; client/rxbuf
   // bleiben nullptr. Timeout/Close verwaltet das ESrv-Modul, nicht EebusSmePoll.
  bool     via_srv = false;
  uint8_t  ski_quelle = 0;   // v190: woher peer_ski kommt - 0 Fundliste (mDNS), 1 Zertifikat, 2 Adresse (Rueckfall)
  uint32_t lpc_mc = 0;       // v231 (4.8): msgCounter unseres letzten LPC-Binding/Write (Zuordnung der Antwort)
  int8_t   hems_m = -1;      // v231: Betriebsart DIESER Verbindung (1 HEMS, 0 SteuVE, -1 = noch offen -> eebus_hems_def)
  uint32_t ping_ok_at = 0;   // v231: Lebenszeichen per Ping - letzte Antwort (0 = Gegenstelle hat noch nie geantwortet)
  uint32_t ping_next = 0;    // v231: naechster Lebenszeichen-Ping
} EebusConn;

EebusConn EConn[EEBUS_MAX_CONN];   // die Verbindungs-Slots (v199: 6)

// v209: Geraetetyp aus der eigenen Discovery der Gegenstelle (deviceInformation -> deviceType), falls sie nicht in der
// Fundliste steht - z. B. eine SteuVE, die direkt nach dem Start der Steuerbox anruft (noch kein Scan). Ohne diese
// Quelle galt sie als HEMS (globaler Vorgabewert) und bekam Pmin statt 4200 W (Test 26.10. 11:46:14).
int EebusModeFromDisco(const char *ski) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return -1; }
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    const char *d = EConn[i].lpc_disco;
    if (!EConn[i].active || !d || strcasecmp(EConn[i].peer_ski, ski)) { continue; }
    const char *p = strstr(d, "\"deviceType\":\"");
    if (!p) { return -1; }
    p += 14;
    char t[32]; size_t o = 0;
    for (; *p && ('"' != *p) && (o < sizeof(t) - 1); p++) {
      if (' ' != *p) { t[o++] = (char)tolower((unsigned char)*p); }
    }
    t[o] = '\0';
    if ('\0' == t[0]) { return -1; }
    return (nullptr != strstr(t, "energymanag")) ? 1 : 0;
  }
  return -1;
}
EebusConn *ESp = &EConn[0];   // "aktueller Slot" — alle Bestandsfunktionen arbeiten hierueber

// v231 (Befund 4.6): eebus_hems_mode gilt je Verbindung. Bei jedem Slotwechsel (ESp = ...) wird die Betriebsart
// dieses Slots geladen. Vorher galt die zuletzt ermittelte Betriebsart fuer alle Gegenstellen - mit HEMS und SteuVE
// gleichzeitig bekam z. B. der Energiemanager heartbeatTimeout PT30S und den SteuVE-Takt (20 s) statt PT10S/4 s.
void Eebus126ModeLoad(void) {
  eebus_hems_mode = (ESp->hems_m >= 0) ? (uint8_t)ESp->hems_m : eebus_hems_def;
}
// v231: Betriebsart an allen Slots dieser Gegenstelle setzen (steht hinter EConn, EebusModeApply liegt davor)
void Eebus126ModeSetSki(const char *ski, int m, const char *quelle) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return; }
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    // auch der aktuelle Slot im Aufbau (EebusShipConnect ruft EebusModeApply vor active = true; Gegenpruefung 06.11.)
    if ((EConn[i].active || (&EConn[i] == ESp)) && EConn[i].peer_ski[0] && (0 == strcasecmp(EConn[i].peer_ski, ski))) {
      if (EConn[i].hems_m != (int8_t)m) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Betriebsart %s fuer %s (aus %s)"), m ? "HEMS" : "SteuVE", EConn[i].peer_ip, quelle);
      }
      EConn[i].hems_m = (int8_t)m;
    }
  }
}

// v194: mitgelesene Werte der Gegenstelle loeschen, wenn auf dem Slot eine neue Sitzung beginnt.
// Sie gelten nur fuer die Verbindung, auf der sie kamen; die neue Gegenstelle meldet ihre eigenen.
void EebusPeerWerteNeu(uint32_t ci) {
  if (ci >= EEBUS_MAX_CONN) { return; }
  EebusConn *c = &EConn[ci];
  for (uint8_t k = 0; k < 7; k++) { c->m_have[k] = false; }
  for (uint8_t k = 0; k < 24; k++) { c->m_scope[k] = 0; c->m_phase[k] = 0; }   // v243: Zuordnungen je Sitzung neu
  for (uint8_t k = 0; k < 4; k++) { c->cfg_ent[k] = -1; c->cfg_n[k] = 0; for (uint8_t j = 0; j < 8; j++) { c->cfg_code[k][j] = 0; c->cfg_kid[k][j] = 0; } }   // v247: + Paare
  for (uint8_t k = 0; k < 3; k++) { c->mll_have[k] = false; }
  c->m_ph_desc = 0; c->meas_prod_dir = false; c->gcp_uc_ent = -1; c->lpp_uc = -1; c->sess_dir_w[0] = c->sess_dir_w[1] = 0;
  c->lpp913_warn = false; c->fs_ro = 0;
  for (uint8_t k = 0; k < 2; k++) { for (uint8_t f = 0; f < 3; f++) { c->m3_have[k][f] = false; } }   // v197
  c->fs_cons_ok = false; c->fs_prod_ok = false; c->plf_ok = false; c->fs_dur[0] = '\0';
  c->pnom_prod_ok = false; c->pnom_cons_ok = false; c->pnom_cprod_ok = false; c->pnom_ccons_ok = false;
  c->lpc_fs_done = false; c->lpc_fs_step = 0;   // v197: neue Sitzung -> Failsafe-Werte neu schreiben
  c->hems_m = -1; c->ping_ok_at = 0; c->ping_next = 0;   // v231: Betriebsart und Lebenszeichen neu je Sitzung
  c->sess_at = 0; c->sess_vorgabe = false; c->sich_stufe = 0; c->sich_wdh_at = 0; c->wdh_n = 0; c->wdh_offen = false; c->fs_wdh_at = 0; c->bind_mc = 0; c->fs_nack = false; c->sess_write_at = 0; c->lpc913_warn = false; c->hbabo_warn = false; c->best_akt[0] = c->best_akt[1] = false; c->sich_plan[0] = c->sich_plan[1] = 0; c->frei_offen[0] = c->frei_offen[1] = false;   // v200: Sicherung neu
  c->abo_n = 0; c->abo_poll_at = 0; c->ig210_at = 0;   // v241
  for (uint8_t d = 0; d < 2; d++) {
    c->lim_act[d] = -1; c->lim_val[d] = 0; c->lim_sc[d] = 0;
    c->lim_dur[d][0] = '\0'; c->lim_dur_s[d] = -1; c->lim_dur_at[d] = 0;
  }
}

// Slot mit bestehender/zuletzt genutzter Verbindung zu dieser SKI (-1 = keiner).
int EebusConnBySki(const char *ski) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return -1; }
   // v247: gibt es zu dieser SKI einen VERBUNDENEN Platz, den nehmen - nicht einen toten/wartenden davor
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (EConn[i].active && (SME_DONE == EConn[i].sme) && (0 == strcasecmp(EConn[i].peer_ski, ski))) { return i; }
  }
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (0 == strcasecmp(EConn[i].peer_ski, ski) &&   // 03.11.2026: SKI ohne Gross-/Kleinschreibung wie alle anderen Vergleiche (sonst Doppelplatz)
        (EConn[i].active || EConn[i].client || EConn[i].keepalive || (SME_OFF != EConn[i].sme))) {
      return i;
    }
  }
  return -1;
}

// v221: gueltige eigene Grenze fuer die SKI der aktuellen Verbindung, Richtung d (0 Bezug, 1 Einspeisung).
// Sucht in allen Slots, die Gegenstelle kann nach dem Wiederverbinden einen anderen Slot haben.
// rest = Restdauer in s, 0 = unbefristet. Unter 5 s Rest gilt die Grenze als abgelaufen.
bool EebusEigeneGrenze(uint8_t d, uint32_t *watt, long *rest) {
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    const EebusConn *c = &EConn[i];
    if (!c->own_ski[0] || strcasecmp(c->own_ski, ESp->peer_ski) || !c->own_akt[d]) { continue; }
    long r = 0;
    if (c->own_ende[d]) {
      const int32_t ms = (int32_t)(c->own_ende[d] - millis());
      if (ms <= 5000) { continue; }
      r = ms / 1000;
    }
    *watt = c->own_w[d];
    *rest = r;
    return true;
  }
   // v224: kein Merker (z. B. Neustart der Box), aber die Anlage meldet eine aktive Grenze: dieselbe Grenze mit
   // ihrer Restdauer zurueckschreiben. [LPC-913]/[LPP-913] verlangen nach Wiederkehr eine Grenze, nicht Schweigen.
  if (1 == ESp->lim_act[d]) {
    long r = 0;
    if (ESp->lim_dur_s[d] >= 0) {
      r = ESp->lim_dur_s[d] - (long)((millis() - ESp->lim_dur_at[d]) / 1000UL);
      if (r <= 5) { return false; }
    }
    double w = (double)labs(ESp->lim_val[d]);
    for (int8_t sc = ESp->lim_sc[d]; sc > 0; sc--) { w *= 10.0; }
    for (int8_t sc = ESp->lim_sc[d]; sc < 0; sc++) { w /= 10.0; }
    *watt = (uint32_t)lround(w);
    *rest = r;
    return true;
  }
  return false;
}

// Slot fuer eine (neue) Verbindung zu dieser SKI besorgen: erst Wiederverwendung
// (gleicher Peer), sonst ein freier Slot. -1 = alle Slots belegt.
// v247 (24.11.2026): tote Plaetze aufraeumen. Ein gescheiterter eigener Verbindungsversuch (z. B. "tls connect
// err" waehrend die Gegenstelle neu startet) behielt Adresse und SKI; die echte Verbindung kam dann auf einem anderen
// Platz zustande (Server-Weg) -> zwei Eintraege fuer dieselbe Gegenstelle (EEBusStatus/Seiten, Suche nach SKI fand den
// toten). Sobald ein Platz verbunden ist (Sme done), werden FREIE Plaetze (gleiche Bedingung wie EebusConnAlloc) mit
// gleicher Adresse oder SKI geleert; ein Keep-Alive-Platz bleibt, ruft aber nicht an, solange die Verbindung steht.
void EebusTotePlaetzeRaeumen(void) {
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    const EebusConn *ok = &EConn[i];
    if (!ok->active || (SME_DONE != ok->sme)) { continue; }
    for (int j = 0; j < EEBUS_MAX_CONN; j++) {
      EebusConn *tot = &EConn[j];
      if ((j == i) || tot->active || (nullptr != tot->client) || tot->teardown) { continue; }
      if (!((SME_OFF == tot->sme) || (SME_FAIL == tot->sme))) { continue; }
      const bool gleich = (tot->peer_ip[0] && (0 == strcmp(tot->peer_ip, ok->peer_ip))) ||
                          (tot->peer_ski[0] && (0 == strcasecmp(tot->peer_ski, ok->peer_ski)));
      if (!gleich) { continue; }
       // Keep-Alive-Platz NICHT leeren - er braucht SKI/Adresse, um nach einem echten Abbruch wieder anzurufen. Solange
       // die Gegenstelle ueber Platz i verbunden ist: Anruf aufschieben, alten Fehler loeschen.
      if (tot->keepalive) {
        tot->reconnect_at = millis() + EebusKaWaitMs();
        if (tot->err[0] || (SHIP_IDLE != tot->state)) {
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Platz %d (%s) wartet - Gegenstelle ist ueber Platz %d verbunden"), j, tot->peer_ip, i);
          tot->err[0] = '\0'; tot->state = SHIP_IDLE;
        }
        continue;
      }
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: toter Platz %d (%s, %s) geleert - Platz %d ist verbunden"), j, tot->peer_ip, tot->err, i);
      tot->peer_ip[0] = '\0'; tot->peer_ski[0] = '\0'; tot->err[0] = '\0'; tot->state = SHIP_IDLE;
    }
  }
}

int EebusConnAlloc(const char *ski) {
  int i = EebusConnBySki(ski);
  if (i >= 0) { return i; }
  for (i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *cc = &EConn[i];
    if (!cc->active && (nullptr == cc->client) && !cc->teardown && !cc->keepalive &&
        ((SME_OFF == cc->sme) || (SME_FAIL == cc->sme))) {
      return i;
    }
  }
  return -1;
}

// Anzahl Slots mit lebender Verbindung (fuer Log/Status).
int EebusConnActive(void) {
  int n = 0;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (EConn[i].active && (SME_OFF != EConn[i].sme) && (SME_FAIL != EConn[i].sme)) { n++; }
  }
  return n;
}

// Ein DER-File laden. Rueckgabe: Laenge (0 = Fehler). buf muss max Bytes fassen.
size_t EebusLoadDer(const char *path, uint8_t *buf, size_t max) {
#ifdef USE_UFILESYS
  if (!TfsFileExists(path)) { return 0; }
  File f = ffsp->open(path, "r");
  if (!f) { return 0; }
  size_t len = f.size();
  if ((0 == len) || (len > max)) { f.close(); return 0; }
  size_t rd = f.read(buf, len);
  f.close();
  return (rd == len) ? len : 0;
#else
  return 0;
#endif
}

void EebusShipFree(void) {
  EebusHeapCheck("shipfree_entry");   // Heap VOR jeglicher Freigabe kaputt?
  if (ESp->client) {
    ESp->client->setSendWaitMs(EEBUS_STOP_WAIT_MS);   // v226: stop() -> flush() sendet Reste; bei toter Gegenstelle nicht warten
    ESp->client->stop(); delete ESp->client; ESp->client = nullptr;
  }
  EebusHeapCheck("shipfree_client");   // nach delete des BearSSL-Clients
  if (ESp->cert_der) { free(ESp->cert_der); ESp->cert_der = nullptr; }
  EebusHeapCheck("shipfree_cert");   // nach free(cert_der) (= Crash-Zeile 607!)
  if (ESp->rxbuf) { free(ESp->rxbuf); ESp->rxbuf = nullptr; ESp->rxbuf_size = 0; }
  if (ESp->wsraw) { free(ESp->wsraw); ESp->wsraw = nullptr; }   // v241
  ESp->wsraw_len = 0; ESp->wsf_len = 0; ESp->wsf_on = false; ESp->wsf_drop = false;
  EebusHeapCheck("shipfree_rxbuf");   // nach free(rxbuf, PSRAM)
  ESp->cert_len = 0;
  ESp->active = false;
  ESp->teardown = false;
}

// KRITISCH gegen Heap-Crash: den TLS-Client NIE aus dem Empfangs-Callback heraus loeschen
// (lwip/W5500 haelt den Socket dann evtl. noch in Bearbeitung -> free auf benutzten Speicher).
// Stattdessen nur markieren; das eigentliche EebusShipFree() laeuft am naechsten Poll-Tick
// bzw. Sekunden-Tick in sauberem Kontext (EebusTeardownNow).
// v229: Wartezeit bis zum naechsten Nachsehen dieses Slots: in den ersten 5 min nach dem Verlust 5 s, danach 30 s
// (Kopf v229). "pending" (Gegenstelle wartet auf Freigabe) behaelt seine festen 5 min (SHIP 13.4.4.1, Z. 387-389).
// v231 (Pruefung 4.12): Zeitpunkt "jetzt + ms" fuer Timer, bei denen 0 "aus" bedeutet. Ergab millis() + ms genau 0,
// feuerte der Timer nie (sehr selten, aber dann z. B. kein Wiederaufbau mehr). Ersetzt alle "x = millis() + T".
uint32_t Eebus126At(uint32_t ms) {
  uint32_t t = millis() + ms;
  return t ? t : 1;
}

uint32_t EebusKaWaitMs(void) {
  if (ESp->sme_pending_logged) { return EEBUS_PENDING_RETRY_MS; }
  if (!ESp->ka_lost_at) { ESp->ka_lost_at = millis(); if (!ESp->ka_lost_at) { ESp->ka_lost_at = 1; } }
  return ((millis() - ESp->ka_lost_at) < EEBUS_KA_FAST_PHASE_MS) ? EEBUS_KA_FAST_MS : EEBUS_KA_SLOW_MS;
}

// v229: Ping vor dem Wiederaufbau (SHIP Kap. 8.2 Z. 640-646). ESP-IDF esp_ping laeuft in einer eigenen Task und
// meldet nur ueber ka_ping_res: 1 laeuft (0x11 = laeuft, Antwort schon da), 2 fertig mit Antwort, 3 fertig ohne Antwort.
// v230: Die Sitzung wird NUR noch hier im Tasmota-Ablauf geloescht (EebusKaPingAufraeumen), nie in ihrer eigenen
// Rueckruffunktion. v229 loeschte sie in on_ping_end: esp_ping_delete_session ruft vTaskDelete auf die EIGENE Task,
// kehrt also nie zurueck - Socket und Speicher der Sitzung blieben liegen (je Ping einmal). Kabeltest 05.11. 20:24-20:28
// (rund 36 Pings ohne Antwort) endete mit Absturz in vTaskSwitchContext (DebugException, EPC 4037e9bd).
// Parameter als void * statt esp_ping_handle_t (gleicher Typ): die Arduino-Vorverarbeitung setzt Prototypen VOR die
// Includes, dort ist esp_ping_handle_t noch unbekannt (Build-Fehler 05.11. 19:3x).
static void EebusKaPingOk(void *h, void *a) { *(volatile uint8_t *)a |= 0x10; }
static void EebusKaPingTo(void *h, void *a) { }
static void EebusKaPingEnd(void *h, void *a) {
  volatile uint8_t *r = (volatile uint8_t *)a;
  *r = (*r & 0x10) ? 2 : 3;   // als LETZTES: danach darf der Tasmota-Ablauf die Sitzung loeschen
}
// Fertige Sitzung loeschen (aus dem Tasmota-Ablauf, nicht aus der Ping-Task). Rueckgabe: keine Sitzung mehr offen.
bool EebusKaPingAufraeumen(void) {
  if (!ESp->ka_ping_h) { return true; }
  if ((2 != ESp->ka_ping_res) && (3 != ESp->ka_ping_res)) { return false; }   // laeuft noch
  esp_ping_delete_session((esp_ping_handle_t)ESp->ka_ping_h);
  ESp->ka_ping_h = nullptr;
  return true;
}
bool EebusKaPingStart(const char *dst) {
  if (!EebusKaPingAufraeumen()) { return true; }   // laeuft noch
  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  cfg.count = 1;
  cfg.timeout_ms = 1000;
  if (!dst || !dst[0] || !ipaddr_aton(dst, &cfg.target_addr)) { return false; }
  esp_ping_callbacks_t cbs;
  cbs.cb_args = (void *)&ESp->ka_ping_res;
  cbs.on_ping_success = EebusKaPingOk;
  cbs.on_ping_timeout = EebusKaPingTo;
  cbs.on_ping_end = EebusKaPingEnd;
  esp_ping_handle_t h = nullptr;
  ESp->ka_ping_res = 1;
  if (ESP_OK != esp_ping_new_session(&cfg, &cbs, &h)) { ESp->ka_ping_res = 0; return false; }
  if (ESP_OK != esp_ping_start(h)) { esp_ping_delete_session(h); ESp->ka_ping_res = 0; return false; }
  ESp->ka_ping_h = (void *)h;
  return true;
}

// v229: die Suche liefert die Gegenstelle nicht (mehr) - die zuletzt benutzte Adresse wieder in die Fundliste
// haengen (nur anhaengen, nie umordnen), damit Ping und Verbindungsversuch weiterlaufen. Rueckgabe: Index oder -1.
int EebusKaPeerZurueck(void) {
  if (!ESp->ka_peer_ok || (0 != strcasecmp(ESp->ka_peer.ski, ESp->peer_ski))) { return -1; }   // SKI ohne Gross-/Kleinschreibung
  if (Eebus.scan) { return -1; }   // Suche laeuft gerade und schreibt die Liste neu
  if (Eebus.peer_count >= EEBUS_MAX_PEERS) { return -1; }
  Eebus.peers[Eebus.peer_count] = ESp->ka_peer;
  return (int)(Eebus.peer_count++);
}

void EebusTeardownLater(void) {
  ESp->active = false;   // sofort keine weitere Verarbeitung dieser Verbindung
  ESp->teardown = true;   // eigentliche Freigabe folgt verzoegert
}
void EebusTeardownNow(void) {
  if (ESp->teardown) {
    if (ESp->via_srv) { EebusSrvFree(); return; }   // Server-Verbindung komplett schliessen
    EebusShipFree();   //      (Socket + TLS + Slot-Unlink in einem)
  }
}

// Unsere eigene SPINE-Geraeteadresse (stabil, aus dem Hostnamen). Format wie die Peers: d:_i:<hersteller>_<id>
void EebusOwnDevice(char *out, size_t len) {
   // Adress-TYP _n (Name, frei waehlbar) statt _i (IANA-PEN, erwartet <PEN-Nummer>_<Seriennr>).
   // Unser bisheriges d:_i:Tasmota_<host> war fuer _i malformt (Name statt PEN). Die Vergleichs-Steuerbox
   // nutzt d:_n:Demo_Vergleichs-Steuerbox-123456789 (Typ _n) und der Energiemanager quittiert ihre Calls (Subscribe/Bind,
   // errorNumber 0), unsere ignorierte er still. Mitschnitt-Befund Energiemanager: Energiemanager akzeptiert uns als
   // ueberwachbares Geraet (Discovery/Abo), aber nicht als Controller mit Schreibzugriff -> Verdacht Quelladresse.
  snprintf(out, len, "d:_n:Tasmota_%s", TasmotaGlobal.hostname);
}

// Rohen 32-Byte EC-Private-Skalar (secp256r1) aus unserem Key-DER holen.
// BearSSL braucht den Skalar roh (nicht DER-verpackt) — analog Tasmotas MQTT-TLS-Key.
// mbedTLS-Crypto (pk/ecp) ist gelinkt; nur die SSL-Schicht fehlt.
bool EebusKeyScalar(uint8_t *out32) {
  uint8_t der[EEBUS_KEY_DER_SIZE];
  size_t klen = EebusLoadDer(EEBUS_KEY_FILE, der, sizeof(der));
  if (0 == klen) { return false; }
  mbedtls_pk_context pk;
  mbedtls_entropy_context en;
  mbedtls_ctr_drbg_context dr;
  mbedtls_pk_init(&pk);
  mbedtls_entropy_init(&en);
  mbedtls_ctr_drbg_init(&dr);
  mbedtls_ctr_drbg_seed(&dr, mbedtls_entropy_func, &en, nullptr, 0);
  bool ok = false;
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
  int pr = mbedtls_pk_parse_key(&pk, der, klen, nullptr, 0, mbedtls_ctr_drbg_random, &dr);
#else
  int pr = mbedtls_pk_parse_key(&pk, der, klen, nullptr, 0);
#endif
  if (0 == pr) {
    mbedtls_ecp_keypair *kp = mbedtls_pk_ec(pk);
    size_t olen = 0;   // _ext-Variante (write_key ist deprecated in 3.6.5)
    if (kp && 0 == mbedtls_ecp_write_key_ext(kp, &olen, out32, 32) && olen == 32) { ok = true; }
  }
  mbedtls_pk_free(&pk);
  mbedtls_ctr_drbg_free(&dr);
  mbedtls_entropy_free(&en);
  return ok;
}

// Genau n Bytes vom BearSSL-Stream lesen (mit Timeout). Rueckgabe: gelesene Bytes oder -1.
int EebusStreamRead(uint8_t *buf, size_t n, uint32_t timeout_ms) {
  size_t got = 0;
  uint32_t start = millis();
  while (got < n) {
    if (!ESp->client) { return -1; }
    int avail = ESp->client->available();
    if (avail > 0) {
      int r = ESp->client->read(buf + got, n - got);
      if (r > 0) { got += r; start = millis(); continue; }
    }
    if (!ESp->client->connected() && ESp->client->available() <= 0) { return -1; }
    if (millis() - start > timeout_ms) { return -1; }
    delay(5);
  }
  return (int)got;
}

// v187: Ein Frame ging nur teilweise raus. Der Strom ist damit kaputt — jedes weitere Byte liest die
// Gegenstelle an der falschen Stelle (Befund 20.10.: SteuVE las Text als WS-Kopf, Schein-Close). Nichts
// mehr senden, in der Datenphase sauber abbauen; der Keep-Alive (ausgehend) bzw. die Gegenstelle
// (eingehend) baut neu auf. Waehrend des Handshakes bleibt es bei EebusSmeFail des Aufrufers.
void EebusSendBroken(void) {
  ESp->send_broken = true;
  if (SME_DONE != ESp->sme) { return; }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** Senden an %s abgebrochen - Verbindung wird abgebaut und neu aufgebaut ***"),
         ESp->peer_ip);
  EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
  ESp->sme = SME_OFF;
  ESp->state = SHIP_IDLE;
  ESp->last_rx = 0;
  if (!ESp->via_srv && ESp->keepalive && (ESp->peer_idx >= 0)) { ESp->reconnect_at = Eebus126At(EebusKaWaitMs()); }
  EebusTeardownLater();
}

// Einen WS-Client-Frame senden (FIN, maskiert — RFC 6455). Beliebige Laenge (16-/64-bit).
// v187: Kopf + maskierte Nutzlast in EINEM Puffer, EIN write() (der TLS-Client teilt selbst in Records
// <= 1024 B). Bis v186 je 64 B ein eigenes write() = eigener TLS-Record (3,6 KB -> 57 Records).
// opcode: 0x2=binary (SHIP/SPINE), 0xA=Pong.
bool EebusWsSendOp(const uint8_t *payload, size_t len, uint8_t opcode) {
  if (ESp->send_broken) { return false; }   // v187: Strom kaputt, Abbau laeuft
  if (ESp->via_srv) {   // Slot einer EINGEHENDEN Verbindung -> Server-Sendepfad
    bool ok = EebusSrvWsSendOp(payload, len, opcode);   // (unmaskiert, ueber die BearSSL-Server-Engine)
    if (!ok) { EebusSendBroken(); }   // v187
    return ok;
  }
  if (!ESp->client) { return false; }
  uint8_t hdr[14];
  size_t hl = 2;
  hdr[0] = 0x80 | (opcode & 0x0F);   // FIN + opcode
  if (len < 126) {
    hdr[1] = 0x80 | (uint8_t)len;   // MASK-Bit + Laenge
  } else if (len < 65536) {
    hdr[1] = 0x80 | 126;   // MASK-Bit + 16-bit Extended Length
    hdr[2] = (uint8_t)(len >> 8);
    hdr[3] = (uint8_t)(len & 0xFF);
    hl = 4;
  } else {
    hdr[1] = 0x80 | 127;   // MASK-Bit + 64-bit Extended Length
    for (int i = 0; i < 8; i++) { hdr[2 + i] = (uint8_t)(len >> (8 * (7 - i))); }
    hl = 10;
  }
  uint8_t mask[4];
  for (int i = 0; i < 4; i++) { mask[i] = (uint8_t)random(256); }   // Maske: nicht sicherheitsrelevant
  memcpy(hdr + hl, mask, 4);
  hl += 4;
  size_t tot = hl + len;
  uint8_t small[256];   // kleine Frames (Herzschlag, Control) auf dem Stack
  uint8_t *fb = (tot <= sizeof(small)) ? small : (uint8_t*)special_malloc(tot);
  if (nullptr != fb) {   // v187: ganzer Frame in einem Stueck
    memcpy(fb, hdr, hl);
    for (size_t i = 0; i < len; i++) { fb[hl + i] = payload[i] ^ mask[i & 3]; }
    size_t wt = ESp->client->write(fb, tot);
    if (fb != small) { free(fb); }
    if (len > 200) { EebusHeapCheckN("wsop_done", (uint32_t)len); }
    if ((wt != tot) || ESp->client->sendStuck()) {   // v231 (4.15): haengt der Sendeweg, gilt der Frame nicht als gesendet
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: WS-SENDEN UNVOLLSTAENDIG @ %s: %u von %u B (op %u) - Frame abgeschnitten"),
             ESp->peer_ip, (uint32_t)wt, (uint32_t)tot, opcode);
      EebusSendBroken();
      return false;
    }
    return true;
  }
   // Rueckfall (kein Speicher fuer den grossen Puffer): wie bis v186 stueckweise
  size_t w = ESp->client->write(hdr, hl);
  if (hl != w) {   // v186: melden; v187: abbauen
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: WS-SENDEN UNVOLLSTAENDIG @ %s: Kopf %u von %u B (Frame %u B, op %u)"),
           ESp->peer_ip, (uint32_t)w, (uint32_t)hl, (uint32_t)len, opcode);
    EebusSendBroken();
    return false;
  }
  if (len > 200) { EebusHeapCheckN("wsop_hdr", (uint32_t)len); }   // nur grosse Sendungen pruefen
  uint8_t chunk[64];   // Nutzlast blockweise maskieren + senden (klein = stack-schonend)
  for (size_t off = 0; off < len; ) {
    size_t n = (len - off > sizeof(chunk)) ? sizeof(chunk) : (len - off);
    for (size_t i = 0; i < n; i++) { chunk[i] = payload[off + i] ^ mask[(off + i) & 3]; }
    w = ESp->client->write(chunk, n);
    if (n != w) {   // v186: Frame bricht mitten in der Nutzlast ab -> Gegenstelle verliert den Takt
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: WS-SENDEN UNVOLLSTAENDIG @ %s: bei Byte %u von %u nur %u von %u B (op %u) - Frame abgeschnitten"),
             ESp->peer_ip, (uint32_t)off, (uint32_t)len, (uint32_t)w, (uint32_t)n, opcode);
      EebusSendBroken();   // v187
      return false;
    }
    off += n;
    if (len > 200) { EebusHeapCheckN("wsop_chunk", (uint32_t)off); }   // Byte-genaue Ortung
  }
  if (len > 200) { EebusHeapCheckN("wsop_done", (uint32_t)len); }
  return true;
}

bool EebusWsSend(const uint8_t *payload, size_t len) {
  return EebusWsSendOp(payload, len, 0x2);
}

// Eine komplette WS-NACHRICHT vom Server lesen — RFC 6455 INKLUSIVE FRAGMENTIERUNG.
// Befund (Konsolen-Mitschnitt Ladestation): Peers fragmentieren Nachrichten (FIN=0 +
// Continuation-Frames). Der alte Empfaenger behandelte JEDES Fragment als eigene Nachricht ->
// ab dem 2. Fragment wurden NUTZDATEN als Frame-Header gelesen -> Stream-Desync (Pseudo-
// Klassifizierer = ASCII-Zeichen, zerhackte JSONs). Jetzt: Fragmente zusammensetzen bis FIN.
// Rueckgabe: Nachrichtenlaenge, 0 = Ping/Pong/verworfen, -1 = Fehler/Timeout, -2 = Close.
int EebusWsRecv(uint8_t *out, size_t max, uint32_t timeout_ms) {
  size_t total = 0;
  bool in_msg = false;   // wir setzen gerade eine fragmentierte Nachricht zusammen
  bool drop = false;   // Nachricht unbrauchbar/zu gross -> nur noch abraeumen
  for (;;) {
    uint8_t h[2];
    if (2 != EebusStreamRead(h, 2, timeout_ms)) { return -1; }
    bool    fin    = (0 != (h[0] & 0x80));
    uint8_t opcode = h[0] & 0x0F;
    bool    masked = (0 != (h[1] & 0x80));   // Server maskiert laut RFC nie — robust trotzdem lesen
    size_t  len    = h[1] & 0x7F;
    if (126 == len) {   // 16-bit erweiterte Laenge
      uint8_t ext[2];
      if (2 != EebusStreamRead(ext, 2, timeout_ms)) { return -1; }
      len = ((size_t)ext[0] << 8) | ext[1];
    } else if (127 == len) {   // 64-bit Laenge: nur die unteren 32 Bit nutzen (reicht)
      uint8_t ext[8];
      if (8 != EebusStreamRead(ext, 8, timeout_ms)) { return -1; }
      len = ((size_t)ext[4] << 24) | ((size_t)ext[5] << 16) | ((size_t)ext[6] << 8) | ext[7];
    }
    uint8_t mask[4] = { 0, 0, 0, 0 };
    if (masked && (4 != EebusStreamRead(mask, 4, timeout_ms))) { return -1; }

    if (0x8 == opcode) { return -2; } // Close-Frame (Payload egal, Verbindung endet)

    if ((0x9 == opcode) || (0xA == opcode)) {
   // Control-Frames (Ping/Pong) duerfen ZWISCHEN Fragmenten kommen (RFC 6455 5.4) ->
   // hier behandeln und den Zusammenbau NICHT abbrechen.
      uint8_t ctl[128];   // Control-Payload max 125 B laut RFC
      size_t cl = (len > sizeof(ctl)) ? sizeof(ctl) : len;
      if (cl && ((int)cl != EebusStreamRead(ctl, cl, timeout_ms))) { return -1; }
      size_t left = len - cl;   // ueberlange (illegale) Control-Frames abraeumen
      while (left) {
        uint8_t s[16];
        size_t c = (left > sizeof(s)) ? sizeof(s) : left;
        if ((int)c != EebusStreamRead(s, c, timeout_ms)) { return -1; }
        left -= c;
      }
      if (masked) { for (size_t i = 0; i < cl; i++) { ctl[i] ^= mask[i & 3]; } }
      if (0x9 == opcode) { EebusWsSendOp(ctl, cl, 0xA); }   // Ping -> Pong gleiche Payload
      if (!in_msg) { return 0; }   // kein Zusammenbau aktiv -> fertig fuer diesen Aufruf
      continue;   // sonst: auf das naechste Fragment warten
    }

   // Daten-Frame (0x1 Text / 0x2 Binary) oder Continuation (0x0)
    if ((0x0 == opcode) && !in_msg) { drop = true; }   // Fortsetzung ohne Anfang -> verwerfen
    if (0x0 != opcode) {   // neuer Nachrichten-Anfang
      if (in_msg) { drop = true; }   // verschachtelter Anfang (illegal)
      total = 0;
      in_msg = true;
    }
    if (!drop && ((total > max) || (len > max - total))) {   // Nachricht passt nicht in den Puffer (v231: ohne Ueberlauf bei total+len)
      drop = true;
      AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: WS-Nachricht > %u B, verworfen"), (uint32_t)max);
    }
    if (drop) {   // Fragment-Payload abraeumen
      size_t left = len;
      while (left) {
        uint8_t s[64];
        size_t c = (left > sizeof(s)) ? sizeof(s) : left;
        if ((int)c != EebusStreamRead(s, c, timeout_ms)) { return -1; }
        left -= c;
      }
    } else {
      if (len && ((int)len != EebusStreamRead(out + total, len, timeout_ms))) { return -1; }
      if (masked) { for (size_t i = 0; i < len; i++) { out[total + i] ^= mask[i & 3]; } }
      total += len;
    }
    if (fin) { return drop ? 0 : (int)total; }
   // FIN=0 -> naechstes Fragment derselben Nachricht folgt
  }
}

// v241 (SHIP 8.3/8.4 Z. 669-686, SHIP Z. 841-842): Empfang der Datenphase OHNE Blockieren. Bis v240 las EebusWsRecv
// blockierend und gab nach 1 s ohne weiteres Byte mitten im Frame auf -> Abbau. Ein einziges verlorenes TCP-Segment
// (Wiederholung nach 1 s, dann 3 s, dann 6 s) trennte so die Verbindung. 10 s am Stueck zu warten geht nicht (Loop-WDT
// 5 s). Jetzt wie der Server-Weg (EebusSrvAppData): alles Verfuegbare in den Rohpuffer, nur vollstaendige Frames
// auswerten, der Rest wartet auf den naechsten Durchlauf - hoechstens EEBUS_WS_REST_MS ohne neues Byte.
// Rueckgabe: >0 Laenge einer ganzen Nachricht in out, 0 = (noch) nichts Ganzes, -1 Fehler/Frist, -2 Close.
int EebusWsRecvNb(uint8_t *out, size_t max) {
  if ((nullptr == ESp->wsraw) || (nullptr == ESp->client)) { return -1; }
  while ((ESp->wsraw_len < EEBUS_WSRAW_SIZE) && (ESp->client->available() > 0)) {
    int r = ESp->client->read(ESp->wsraw + ESp->wsraw_len, EEBUS_WSRAW_SIZE - ESp->wsraw_len);
    if (r <= 0) { break; }
    ESp->wsraw_len += (size_t)r;
    ESp->wsraw_at = millis();
  }
  for (;;) {
    uint8_t *p = ESp->wsraw;
    const size_t have = ESp->wsraw_len;
    if (have < 2) { break; }
    const bool    fin    = (0 != (p[0] & 0x80));
    const uint8_t opcode = p[0] & 0x0F;
    const bool    masked = (0 != (p[1] & 0x80));
    size_t len = p[1] & 0x7F;
    size_t ho = 2;
    if (126 == len) {
      if (have < 4) { break; }
      len = ((size_t)p[2] << 8) | p[3];
      ho = 4;
    } else if (127 == len) {   // 64-bit Laenge: nur die unteren 32 Bit (reicht)
      if (have < 10) { break; }
      len = ((size_t)p[6] << 24) | ((size_t)p[7] << 16) | ((size_t)p[8] << 8) | p[9];
      ho = 10;
    }
    if (masked) {
      if (have < ho + 4) { break; }
      ho += 4;
    }
    if (len > EEBUS_WSRAW_SIZE - ho) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: WS-Frame %u B von %s passt nie in den Puffer"), (uint32_t)len, ESp->peer_ip);
      return -1;
    }
    if (have < ho + len) { break; }   // Frame noch nicht komplett -> naechster Durchlauf
    uint8_t *pl = p + ho;
    if (masked) { const uint8_t *mk = p + ho - 4; for (size_t i = 0; i < len; i++) { pl[i] ^= mk[i & 3]; } }
    int ret = 0;
    if (0x8 == opcode) {   // v242 (RFC 6455 5.5.1, SHIP Z. 802): Close beantworten
      EebusWsSendOp(pl, (len >= 2) ? 2 : 0, 0x8);
      ret = -2;
    } else if (0x9 == opcode) {   // Ping -> Pong gleiche Payload (Control-Frames auch zwischen Fragmenten, RFC 6455 5.4)
      EebusWsSendOp(pl, (len > 125) ? 125 : len, 0xA);
    } else if (0xA == opcode) {
      ESp->pong_wait_at = 0;   // v242: Pong da
    } else if ((0x0 != opcode) && (0x2 != opcode)) {   // v242 (SHIP Z. 826-830): Text -> 1003, reserviert -> 1002
      const uint8_t cs[2] = { 0x03, (uint8_t)((0x1 == opcode) ? 0xEB : 0xEA) };
      EebusWsSendOp(cs, 2, 0x8);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s sendet WS-Opcode %u - Close %u (SHIP Z. 826-830)"), ESp->peer_ip, opcode, (0x1 == opcode) ? 1003 : 1002);
      return -3;
    } else {
      if (0x0 != opcode) { ESp->wsf_len = 0; ESp->wsf_on = true; ESp->wsf_drop = false; }   // neuer Anfang (verschachtelt: neu beginnen)
      if (ESp->wsf_on) {   // Fortsetzung ohne Anfang wird verworfen
        if (!ESp->wsf_drop && (len > max - ESp->wsf_len)) {
          ESp->wsf_drop = true;
          AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: WS-Nachricht > %u B, verworfen"), (uint32_t)max);
        }
        if (!ESp->wsf_drop) { memcpy(out + ESp->wsf_len, pl, len); ESp->wsf_len += len; }
        if (fin) {
          if (!ESp->wsf_drop && ESp->wsf_len) { ret = (int)ESp->wsf_len; }
          ESp->wsf_on = false; ESp->wsf_len = 0; ESp->wsf_drop = false;
        }
      }
    }
    const size_t used = ho + len;   // Frame aus dem Puffer nehmen
    memmove(ESp->wsraw, ESp->wsraw + used, have - used);
    ESp->wsraw_len = have - used;
    if (0 != ret) { return ret; }
  }
  if (((ESp->wsraw_len > 0) || ESp->wsf_on) && ((millis() - ESp->wsraw_at) > EEBUS_WS_REST_MS)) { return -1; }
  return 0;
}

// Letztes Verbindungsergebnis eines Peers merken (Slot per SKI suchen/anlegen).
// (Signaturen nur einfache Typen — .ino-Prototypen-Falle.)
void EebusStatSet(const char *ski, int state, const char *err) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return; }
  int slot = -1;
  for (uint32_t i = 0; i < Eebus.stat_count; i++) {
    if (0 == strcasecmp(Eebus.stat_ski[i], ski)) { slot = i; break; }
  }
  if (slot < 0) {
    if (Eebus.stat_count >= EEBUS_MAX_PEERS) { return; }
    slot = Eebus.stat_count++;
    strlcpy(Eebus.stat_ski[slot], ski, sizeof(Eebus.stat_ski[slot]));
  }
  Eebus.stat_state[slot] = (uint8_t)state;
  strlcpy(Eebus.stat_err[slot], (err) ? err : "", sizeof(Eebus.stat_err[slot]));
}

// PRAEZISIONS-STOLPERDRAHT (Taeter-Jagd Heap-Korruption): Heap-Integritaet an BENANNTEN
// Checkpunkten des Connect-/Empfangspfads pruefen. Latcht pro Boot: gemeldet wird der ERSTE
// kaputte Checkpunkt — dessen Label ist der Schritt, in dem (oder unmittelbar vor dem) der
// Heap kippte. Kostet ein paar ms pro Aufruf (Heap-Walk) — Debug-Werkzeug, spaeter entschaerfen.
bool eebus_heap_bad = false;
void EebusHeapCheck(const char *where) {
  if (eebus_heap_bad) { return; }
  if (!heap_caps_check_integrity_all(false)) {
    eebus_heap_bad = true;
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: !!! HEAP KORRUPT @ %s !!! peer=%s"), where, ESp->peer_ip);
  }
}
// Diagnose: Heap-Check mit Zahl im Label (z.B. Chunk-Offset im grossen Sendevorgang).
void EebusHeapCheckN(const char *where, uint32_t num) {
  if (eebus_heap_bad) { return; }
  if (!heap_caps_check_integrity_all(false)) {
    eebus_heap_bad = true;
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: !!! HEAP KORRUPT @ %s=%u !!! peer=%s"), where, num, ESp->peer_ip);
  }
}

// Verbindungsaufbau bis CMI. idx = Index in Eebus.peers. true = Erfolg.
// (Signatur bewusst nur mit int — kein eigener Typ, wegen .ino-Prototypen-Falle.)
bool EebusShipConnect(int idx) {
  EebusPeer *p = &Eebus.peers[idx];
   // v189: Zieladresse festlegen, BEVOR irgendetwas angelegt wird. Meldet ein Geraet per mDNS nur IPv6
   // (21.10. Testnetz: SteuVE mit WLAN + LAN antwortete am LAN nur mit AAAA), wird ueber IPv6 verbunden.
   // Ohne verwendbare Adresse kein Versuch: bisher lief fromString("") in den fail-Zweig, und EebusShipFree()
   // rief stop() auf einem TLS-Client, der nie verbunden war -> Load access fault an Adresse 0 (Neustart).
  const char *dst = p->ip[0] ? p->ip : p->ip6;
  IPAddress dst_addr;
  if (!dst[0] || !dst_addr.fromString(dst)) {
    strlcpy(ESp->err, dst[0] ? "Adresse ungueltig" : "keine Adresse (mDNS)", sizeof(ESp->err));
    ESp->state = SHIP_ERROR;
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: kein Verbindungsversuch zu %s - %s"), p->ski, ESp->err);
    return false;
  }
  ESp->ka_peer = *p; ESp->ka_peer_ok = true;   // v229: Adresse fuer den Wiederaufbau merken (Rueckfall ohne Suche)
  ESp->ka_last_try = millis();                 // v229: echter Verbindungsversuch
  char dst_host[50];   // fuer den Host-Header des WebSocket-Upgrades: IPv6 in eckigen Klammern
  if (strchr(dst, ':')) { snprintf(dst_host, sizeof(dst_host), "[%s]", dst); }
  else { strlcpy(dst_host, dst, sizeof(dst_host)); }
  if (ESp->client) {   // alte Verbindung im Slot (evtl. teardown noch anstehend):
    EebusShipFree();   // freigeben und lwip/W5500 den Socket-Abbau beenden lassen,
    delay(100);   // BEVOR der neue tcp_connect startet (Crash-Befundspaet)
  } else {
    EebusShipFree();   // raeumt Puffer-Reste ab
  }
  ESp->err[0] = '\0';
  ESp->state = SHIP_IDLE;
  ESp->sme = SME_OFF;
  ESp->peer_id[0] = '\0';
  ESp->peer_dev[0] = '\0';   // M3: SPINE-Zustand ruecksetzen
  ESp->disco_answered = false;
  ESp->peer_disco_read = false;
  ESp->we_nm_subscribed = false;   // 
  ESp->disco_fallback_at = 0;
  ESp->peer_subscribed = false;
  ESp->gb_n = 0;   // v246
  ESp->lc_sub = false; ESp->lc_cli_ent = 0; ESp->lc_cli_feat = 0;   // 
  ESp->hb_sub = false;
  ESp->hb_cli_ent = 0;
  ESp->hb_cli_feat = 0;
  ESp->hb_next = 0;
   // Herzschlag der Gegenstelle: frische Sitzung -> noch keiner empfangen, keine Zusage bekannt.
   // Zaehler und Zeitpunkt der Vorfaelle bleiben ABSICHTLICH stehen — sie sind die Vorfallsliste.
  ESp->peer_hb_at = 0; ESp->peer_hb_ctr = 0; ESp->peer_hb_tmo_s = 0; ESp->peer_hb_lost = false; ESp->hb_zurueck = false; ESp->hb_tx_at = 0;
  ESp->spine_ctr = 1;
   // LPC-Sequenz ruecksetzen (frische TLS-Session -> Binding weg, limitId neu holen)
  ESp->lpc_state = LPC_IDLE;
  ESp->lpc_bound = false;
  ESp->lpc_onboard_only = false;   // 
  ESp->lpc_onboarded = false;   // 
  ESp->lpc_limit_id = -1; ESp->lpc_lid_dir[0] = ESp->lpc_lid_dir[1] = -1;   // v184
  EebusPeerWerteNeu((uint32_t)(ESp - EConn));   // v194: Werte der alten Gegenstelle weg
  ESp->lpc_peer_ent = 1; ESp->lpc_peer_feat = 6; ESp->lpc_peer_dcfg_feat = 24;   // Ladestation-Default; /wird aus der Peer-Discovery
   // gelernt (EebusLpcLearnTarget), sobald sie eintrifft
  ESp->lpc_deadline = 0;
  ESp->lpc_verify_at = 0;   // 
  ESp->lpc_verify_tries = 0;   // 
  ESp->lpc_verify_failed = false;   // 
  ESp->lpc_sel_step = 0;   // 
  ESp->lpc_write_tries = 0;   // 
  ESp->lpc_write_retry_at = 0;   // 
  if (ESp->lpc_disco) { free(ESp->lpc_disco); ESp->lpc_disco = nullptr; }   // alte Discovery weg
  if (ESp->lpc_uc)    { free(ESp->lpc_uc);    ESp->lpc_uc = nullptr; }   // Use-Case-Auskunft weg
  if (ESp->rd_buf)    { free(ESp->rd_buf);    ESp->rd_buf = nullptr; }   // aufgehobene Leseantwort weg
  ESp->rd_ent = -1; ESp->rd_feat = -1;
   // auch die VORMERKUNG loeschen — sonst setzt der Sekunden-Tick eine Leseanfrage ab, die zu einer
   // laengst beendeten Sitzung gehoert, auf einer Verbindung, die inzwischen jemand anderem gehoert.
  ESp->rd_pend_ent = -1; ESp->rd_pend_feat = -1; ESp->rd_pend_fn[0] = '\0';
  ESp->rd_pend_elist[0] = '\0'; ESp->rd_mc = 0; ESp->rd_pend_sel = -1;
   // Messwerte einer NEUEN Sitzung nicht mit denen der alten mischen: Einheiten-Nummern und
   // Kennungen gelten nur innerhalb einer Verbindung.
  ESp->meas_ent = -1; ESp->mo_n = 0;
  for (uint8_t k = 0; k < 16; k++) {
    ESp->mo_have[k] = false; ESp->mo_scope[k][0] = '\0'; ESp->mo_unit[k][0] = '\0';
  }
  ESp->lpc_our_limit = false;
  ESp->lpc_result[0] = '\0';
  ESp->peer_idx = idx;
  ESp->last_rx = millis();
  strlcpy(ESp->peer_ip, dst, sizeof(ESp->peer_ip));   // v189: IPv4 oder (ersatzweise) IPv6
  strlcpy(ESp->peer_ski, p->ski, sizeof(ESp->peer_ski));   // Peer-Identitaet aus mDNS
  ESp->ski_quelle = 0;   // v190: ausgehend - SKI aus der Fundliste (Pruefung gegen das Zertifikat folgt, HEMS-Plan Schritt 2)
  EebusModeApply(ESp->peer_ski);   // Betriebsmodus (HEMS/SteuVE) aus SKI-Zuordnung
  ESp->peer_port = p->port;

   // Grosser SPINE-Empfangspuffer in PSRAM (Detailed-Discovery-Datagramme sind mehrere KB)
  ESp->rxbuf = (uint8_t*)special_malloc(EEBUS_SPINE_RXBUF);
  if (nullptr == ESp->rxbuf) { strlcpy(ESp->err, "rxbuf", sizeof(ESp->err)); goto fail; }
  ESp->rxbuf_size = EEBUS_SPINE_RXBUF;
  if (ESp->wsraw) { free(ESp->wsraw); }   // v241: Rohpuffer fuer den Empfang ohne Blockieren
  ESp->wsraw = (uint8_t*)special_malloc(EEBUS_WSRAW_SIZE);
  if (nullptr == ESp->wsraw) { strlcpy(ESp->err, "wsraw", sizeof(ESp->err)); goto fail; }
  ESp->wsraw_len = 0; ESp->wsraw_at = millis(); ESp->wsf_len = 0; ESp->wsf_on = false; ESp->wsf_drop = false;

   // Unser Client-Zertifikat (DER) laden — bleibt allokiert, BearSSL zeigt darauf
  ESp->cert_der = (uint8_t*)malloc(EEBUS_CERT_DER_SIZE);
  if (nullptr == ESp->cert_der) { strlcpy(ESp->err, "malloc", sizeof(ESp->err)); goto fail; }
  ESp->cert_len = EebusLoadDer(EEBUS_CERT_FILE, ESp->cert_der, EEBUS_CERT_DER_SIZE);
  if (0 == ESp->cert_len) { strlcpy(ESp->err, "cert load", sizeof(ESp->err)); goto fail; }
  if (!EebusKeyScalar(ESp->key_scalar)) { strlcpy(ESp->err, "key scalar", sizeof(ESp->err)); goto fail; }
  EebusHeapCheck("key_scalar");   // Checkpunkt: nach mbedtls-Key-Extraktion

   // BearSSL-Strukturen fuellen (analog Tasmota MQTT/AWS-IoT Client-Cert)
  ESp->br_cert[0].data = ESp->cert_der;
  ESp->br_cert[0].data_len = ESp->cert_len;
  ESp->br_key.curve = BR_EC_secp256r1;   // 23
  ESp->br_key.x = ESp->key_scalar;
  ESp->br_key.xlen = 32;

   // Sendepuffer 1024, Empfang 2048 (meierchen006, 16.11.2026). SHIP TS 1.1.0 Kap. 9.2: ausgehende Pakete duerfen
   // 1024 Byte (TLSPlaintext.length) NICHT ueberschreiten. setBufferSizes() in tls_mini rechnet den Overhead selbst
   // dazu: aus (2048, 2048) wurden 2373/2133 Byte, BearSSL waehlte daraus 2^11 (ssl_engine.c Z. 429-447) und bat im
   // ClientHello um 2048; gemessen: Datenbloecke von 2072 B (= 2048 + 24 B GCM). Die Fragment-Bitte richtet sich nach
   // dem KLEINEREN Puffer: mit Senden 1024 bittet BearSSL um 1024 und sendet hoechstens 1024 je Block (gemessen
   // 1048 B = 1024 + 24). Der Empfang bleibt bei 2048. Mit 4096er-Puffern wuerde 2048 angefordert, und
   // mbedtls-Gegenstellen lehnen das ClientHello mit handshake_failure (err=296) ab.
  ESp->client = new BearSSL::WiFiClientSecure_light(2048, 1024);
  if (nullptr == ESp->client) { strlcpy(ESp->err, "client new", sizeof(ESp->err)); goto fail; }
  ESp->client->setSendWaitMs(EEBUS_SEND_WAIT_MS);   // v226: nicht blockierend senden (siehe Kopf)
  ESp->client->setLoopTimeout(EEBUS_TLS_LOOP_MS);   // v231: jeder TLS-Lauf (Handshake und Datenphase) hoechstens 5 s statt fest 10 s (tls_mini)
  ESp->active = true;
  ESp->send_broken = false;   // v187: neue Verbindung, sauberer Strom
   // allowed_usages=alle; cert_issuer_key_type=BR_KEYTYPE_EC: unser Cert ist selbstsigniert
   // mit EC-Key -> muss EC sein (0 fuehrte zu handshake_failure, Cert nicht korrekt praesentiert)
  ESp->client->setClientECCert(ESp->br_cert, &ESp->br_key, 0xFFFF, BR_KEYTYPE_EC);
  ESp->client->setInsecure();   // SHIP: keine CA-Pruefung, Identitaet ueber SKI
  ESp->client->setECDSA(true);   // wir sind EC (kein RSA)
  ESp->client->setRSAOnly(false);   // ZWINGEND: _rsa_only=false -> ECDHE_ECDSA-Suite (0xc02b)
   // wird angeboten. Ohne dies bot BearSSL nur ECDHE_RSA
   // (0xc02f) an -> Ladestation (EC-Cert) hatte keine Cipher -> 296.

   // TLS-Verbindung (BearSSL macht Handshake inkl. Client-Cert).
   // WICHTIG: per IPAddress verbinden, NICHT per String! Der String-connect() nimmt den Namen
   // als SNI -> BearSSL sendet die IP als Server Name Indication, was der mbedtls-Server der
   // Ladestation ablehnt (SNI darf keine IP-Literal sein) -> handshake_failure (err=296).
   // Der IPAddress-connect() nutzt _domain (leer) fuer SNI -> es wird KEIN SNI gesendet.
   // NIE ein IP-Literal als SNI setzen — mbedtls-Gegenstellen lehnen das mit err=296 ab.
  {
     // v231 (Pruefung 1.10): TCP-Aufbau hoechstens EEBUS_TCP_CONN_MS. Ohne Angabe galt die Stream-Frist des Clients
     // (10 s), so lange stand die ganze Box (Web, Log, alle Verbindungen), wenn die Gegenstelle weg war.
     // v242 (SHIP Z. 723, 804): SNI = mDNS-Hostname des Servers (ohne Schlusspunkt), nie eine Adresse.
    if (p->host[0]) {
      bool nur_ziffern = true;
      for (const char *c = p->host; *c; c++) { if (!isdigit((unsigned char)*c) && ('.' != *c) && (':' != *c)) { nur_ziffern = false; break; } }
      if (!nur_ziffern) {
        char sni[48]; size_t hl = strlen(p->host);
        while (hl && ('.' == p->host[hl - 1])) { hl--; }
        if ((hl > 6) && (0 == strncasecmp(p->host + hl - 6, ".local", 6))) { snprintf(sni, sizeof(sni), "%.*s", (int)hl, p->host); }
        else { snprintf(sni, sizeof(sni), "%.*s.local", (int)hl, p->host); }
        ESp->client->setDomainName(sni);
      }
    }
    if (!ESp->client->connect(dst_addr, p->port, (int32_t)EEBUS_TCP_CONN_MS)) {   // v189: Adresse oben geprueft (IPv4 oder IPv6)
      snprintf(ESp->err, sizeof(ESp->err), "tls connect err=%d", (int)ESp->client->getLastError());
      goto fail;
    }
  }
  ESp->state = SHIP_TLS_OK;
  EebusHeapCheck("tls_handshake");   // Checkpunkt: nach BearSSL-TLS-Handshake (inkl. ECDSA-Signieren)
   // v192: Ist die Gegenstelle wirklich das Geraet mit dieser SKI? Die SKI aus der Fundliste hat das Geraet nur
   // angekuendigt - das Zertifikat beweist sie (BearSSL hat die Signatur im Handshake geprueft).
  {
    size_t klen = 0;
    const uint8_t *kp = BearSSL::ShipPeerPubKey(ESp->client, &klen);   /* 24.10.: je Client (tls_mini) */   // v193 (gemu 22.10.): tls_mini liefert den Punkt, die SKI bilden wir
    char zski[41] = { 0 };
    if (65 == klen) {
      uint8_t h[20];
      br_sha1_context sc;
      br_sha1_init(&sc);
      br_sha1_update(&sc, kp, 65);   // wie EebusSkiRawFromKey: SHA-1 ueber 04||X||Y
      br_sha1_out(&sc, h);
      for (uint32_t i = 0; i < 20; i++) { snprintf_P(zski + (i * 2), 3, PSTR("%02x"), h[i]); }
    }
    if (!zski[0]) {   // v231 (Pruefung 4.7): nicht mehr verbinden. Bis v230 stand hier "nicht vertraut" im Log, verbunden
       // wurde trotzdem - die Identitaet war dann gar nicht geprueft. Ohne EC-Schluessel laesst sich die SKI nicht belegen.
      AddLog(LOG_LEVEL_ERROR, PSTR("EBG: %s: Zertifikat ohne EC-Schluessel - SKI nicht pruefbar, NICHT verbunden"), ESp->peer_ip);
      strlcpy(ESp->err, "Zertifikat ohne EC-Schluessel", sizeof(ESp->err));
      goto fail;
    } else if (0 == strcmp(ESp->peer_ski, "manual")) {   // v231: EEBusConnectIp - die SKI kommt erst hier, aus dem Zertifikat
      strlcpy(ESp->peer_ski, zski, sizeof(ESp->peer_ski));
      strlcpy(p->ski, zski, sizeof(p->ski));
      strlcpy(ESp->ka_peer.ski, zski, sizeof(ESp->ka_peer.ski));
      ESp->ski_quelle = 1;
      EebusModeApply(ESp->peer_ski);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s: manuelle Verbindung, SKI %s aus dem Zertifikat"), ESp->peer_ip, ESp->peer_ski);
    } else if (0 != strcasecmp(zski, ESp->peer_ski)) {
      AddLog(LOG_LEVEL_ERROR, PSTR("EBG: %s: Zertifikat hat SKI %s, erwartet %s - NICHT verbunden"),
             ESp->peer_ip, zski, ESp->peer_ski);
      strlcpy(ESp->err, "SKI passt nicht zum Zertifikat", sizeof(ESp->err));
      goto fail;
    } else {
      ESp->ski_quelle = 1;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s: SKI %s durch Zertifikat bestaetigt"), ESp->peer_ip, ESp->peer_ski);
    }
  }
  {   // v242 (SHIP Z. 1265-1269 Fussnote 1, TC_SHIP_SEC_001/002): die SKI-Erweiterung des Zertifikats muss dieselbe SKI tragen
    size_t dl = 0;
    const uint8_t *der = BearSSL::TlsPeerCertDer(&dl);
    uint8_t ext[20];
    char eh[41] = { 0 };
    if (EebusDerSkiExt(der, dl, ext)) { for (uint32_t i = 0; i < 20; i++) { snprintf_P(eh + (i * 2), 3, PSTR("%02x"), ext[i]); } }
    if (!eh[0] || (0 != strcasecmp(eh, ESp->peer_ski))) {
      AddLog(LOG_LEVEL_ERROR, PSTR("EBG: %s: SKI-Erweiterung im Zertifikat %s (Schluessel-SKI %s) - NICHT verbunden"),
             ESp->peer_ip, eh[0] ? eh : "fehlt", ESp->peer_ski);
      strlcpy(ESp->err, "SKI-Erweiterung passt nicht", sizeof(ESp->err));
      goto fail;
    }
  }
  ESp->conn_at = millis(); if (!ESp->conn_at) { ESp->conn_at = 1; }   // v242
  ESp->pong_wait_at = 0; ESp->dbl_at = 0;

   // WebSocket-Upgrade (Subprotocol "ship")
  if (!EebusWsUpgrade(dst_host, p->port, p->path)) { goto fail; }   // err bereits gesetzt (v189: IPv6 in [])
  ESp->state = SHIP_WS_OK;
  EebusHeapCheck("ws_upgrade");   // Checkpunkt: nach HTTP-Upgrade

   // CMI-Handshake (SHIP 13.4.3): [0x00,0x00] senden, die Antwort kommt ohne Blockieren im Empfang (EebusCmiAntwort).
   // v242 (SHIP Z. 2082-2084): CmiTimeout 15 s - bis v241 wartete die Box blockierend nur 5 s.
  {
    uint8_t cmi[2] = { 0x00, 0x00 };
    if (!EebusWsSend(cmi, 2)) { strlcpy(ESp->err, "cmi send", sizeof(ESp->err)); goto fail; }
  }
  ESp->sme = SME_CMI;
  ESp->sme_deadline = Eebus126At(EEBUS_CMI_TIMEOUT_MS);
  return true;

fail:
  ESp->state = SHIP_ERROR;
  EebusStatSet(ESp->peer_ski, SHIP_ERROR, ESp->err);
  AddLog(LOG_LEVEL_ERROR, PSTR("EBG: SHIP-Connect fehlgeschlagen (%s) @ %s:%u"),
         ESp->err, ESp->peer_ip, ESp->peer_port);
  EebusShipFree();
  return false;
}

// WebSocket-Upgrade ueber die offene BearSSL-Verbindung. err setzen bei Fehler.
// (Signatur nur einfache Typen — .ino-Prototypen-Falle.)
bool EebusWsUpgrade(const char *ip, uint16_t port, const char *path_in) {
  if (!ESp->client) { strlcpy(ESp->err, "no client", sizeof(ESp->err)); return false; }
   // Sec-WebSocket-Key: 16 Zufallsbytes base64 (Nonce, nicht sicherheitskritisch)
  uint8_t rnd[16];
  for (int i = 0; i < 16; i++) { rnd[i] = (uint8_t)random(256); }
  unsigned char key_b64[32]; size_t key_len = 0;
  mbedtls_base64_encode(key_b64, sizeof(key_b64), &key_len, rnd, 16);
  key_b64[key_len] = '\0';

  const char *path = (path_in && path_in[0]) ? path_in : "/ship/";
  char req[320];
  int rl = snprintf(req, sizeof(req),
    "GET %s HTTP/1.1\r\nHost: %s:%u\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
    "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: ship\r\n\r\n",
    path, ip, port, key_b64);
  if ((size_t)rl != ESp->client->write((const uint8_t*)req, rl)) {
    strlcpy(ESp->err, "ws send", sizeof(ESp->err)); return false; }

   // Antwort-Header bis "\r\n\r\n" lesen (byteweise, klein)
  char resp[512]; size_t ri = 0;
  uint32_t start = millis();
  while (ri < sizeof(resp) - 1) {
    if (ESp->client->available() > 0) {
      uint8_t c;
      if (1 == ESp->client->read(&c, 1)) {
        resp[ri++] = (char)c;
        if (ri >= 4 && resp[ri-4]=='\r' && resp[ri-3]=='\n' && resp[ri-2]=='\r' && resp[ri-1]=='\n') { break; }
      }
    } else {
      if (!ESp->client->connected() && ESp->client->available() <= 0) {
        strlcpy(ESp->err, "ws closed", sizeof(ESp->err)); return false; }
      if (millis() - start > EEBUS_HS_WAIT_MS) { strlcpy(ESp->err, "ws resp timeout", sizeof(ESp->err)); return false; }   // v231: 5 s statt 8 s
      delay(5);
    }
  }
  resp[ri] = '\0';
  if (nullptr == strstr(resp, " 101")) {
    strlcpy(ESp->err, "ws no 101", sizeof(ESp->err)); return false; }
  {   // v242 (SHIP Z. 802, 818-821, RFC 6455 4.1): Sec-WebSocket-Accept und Subprotokoll "ship" pruefen
    uint8_t sha[20];
    SHA1Builder s1;
    s1.begin();
    s1.add(key_b64, key_len);
    s1.add((uint8_t*)"258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
    s1.calculate();
    s1.getBytes(sha);
    unsigned char erw[32]; size_t el = 0;
    mbedtls_base64_encode(erw, sizeof(erw), &el, sha, 20);
    erw[el] = '\0';
    char v[64];
    if (!EebusHdrWert(resp, "Sec-WebSocket-Accept", v, sizeof(v)) || (0 != strcmp(v, (const char*)erw))) {
      strlcpy(ESp->err, "ws accept falsch", sizeof(ESp->err)); return false; }
    if (!EebusHdrWert(resp, "Sec-WebSocket-Protocol", v, sizeof(v)) || (0 != strcasecmp(v, "ship"))) {
      strlcpy(ESp->err, "ws protocol nicht ship", sizeof(ESp->err)); return false; }
  }
  return true;
}

/*********************************************************************************************\
 * MEILENSTEIN 2b — SHIP-SME-Handshake (SHIP 13.4.4): Hello -> ProtocolHandshake ->
 * PinCheck -> AccessMethods -> Done (nach SHIP-Spezifikation).
 *
 * Laeuft ASYNCHRON (FUNC_EVERY_100_MSECOND): das Hello kann minutenlang "pending" sein,
 * bis der Nutzer unsere SKI im Geraete-UI bestaetigt (Pairing). Blockieren verboten.
 * Wir vertrauen als Prueftool JEDEM Peer (Identitaet = SKI aus mDNS) -> hello phase=ready.
 *
 * Jede SHIP-Nachricht (TX+RX) wird geloggt: Konsole (EBG:) + Mitschnitt /eebus_ship.log
 * auf SD — das ist die Rohdaten-Basis fuer das spaetere Pruefprotokoll.
\*********************************************************************************************/

// Mitschnitt: Richtung 'T'(X)/'R'(X), classifier 0=CMI 1=Control 2=Data 3=End.
// Waehrend AKTIVER Verbindung NIE auf die SD-Karte schreiben! (BEWIESENE Crash-
// Korrelation: jeder Heap-Korruptions-Crash hatte SD-Zugriff bei offener TLS-Verbindung,
// alle stabilen Laeufe hatten keinen.) Mitschnitt geht in einen PSRAM-Puffer; auf SD erst per
// EEBusLog 2 (Dump) und nur bei getrennter Verbindung.
#define EEBUS_LOGBUF_SZ 524288   // 512 KB PSRAM (war 32 KB). Fasst eine KOMPLETTE Energiemanager-Session
   // (Handshake+Discovery+Write+Antwort) inkl. langem 7-8-min-Connect
   // mit Reserve in EINEM Mitschnitt — kein Log-an/aus-Timing noetig.
   // Bleibt RAM (PSRAM, 8 MB); SD-Schreiben nur beim Dump nach Trennen.
bool  eebus_ram_log = false;   // Mitschnitt in RAM-Puffer (EEBusLog 1)
char *eebus_log_buf = nullptr;   // PSRAM-Ringpuffer (lazy)
size_t eebus_log_len = 0;
bool  eebus_log_full = false;

// v177: Log-Zeile mit Telegramm-Inhalt. Laeuft gerade ein Web-Befehl (templog_level gesetzt), wird
// sie zurueckgehalten und von EebusLogFlush() im naechsten 100-ms-Takt geschrieben (siehe EEBUS_DEFLOG_SZ).
char *eebus_deflog = nullptr;
size_t eebus_deflog_len = 0;
void EebusLogLine(const char *line) {
  if (0 == TasmotaGlobal.templog_level) { AddLog(LOG_LEVEL_INFO, PSTR("%s"), line); return; }
  if (nullptr == eebus_deflog) {
    eebus_deflog = (char*)special_malloc(EEBUS_DEFLOG_SZ);
    eebus_deflog_len = 0;
  }
  size_t l = strlen(line);
  if ((nullptr == eebus_deflog) || (eebus_deflog_len + l + 1 > EEBUS_DEFLOG_SZ)) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("%s"), line);   // kein Platz: auf DEBUG, das sammelt /cm nicht ein
    return;
  }
  memcpy(eebus_deflog + eebus_deflog_len, line, l + 1);
  eebus_deflog_len += l + 1;
}
void EebusLogFlush(void) {
  if ((0 == eebus_deflog_len) || (0 != TasmotaGlobal.templog_level)) { return; }
  size_t off = 0;
  while (off < eebus_deflog_len) {
    AddLog(LOG_LEVEL_INFO, PSTR("%s"), eebus_deflog + off);
    off += strlen(eebus_deflog + off) + 1;
  }
  eebus_deflog_len = 0;
}

void EebusShipLog(char dir, int classifier, const char *json) {
  size_t jl = strlen(json);
  char line[280];
   // lange Frames NICHT mehr bei 220 kappen, sondern in 200-Zeichen-Stuecken VOLL
   // ausgeben (bis ~1000 Zeichen; darueber, z.B. 5-9KB-Discovery, nur gekuerzt). So zeigt die
   // Konsole/der Live-Monitor das KOMPLETTE Write-/Ergebnis-Frame fuer den Roh-Diff gegen die Vergleichs-Steuerbox.
   // Der RAM-/SD-Mitschnitt (unten) speichert ohnehin den ganzen Frame.
  if (jl > 200) {
    size_t show = (jl > 1000) ? 1000 : jl;
    int parts = (int)((show + 199) / 200);
    char piece[208];
    for (int p = 0; p < parts; p++) {
      size_t off = (size_t)p * 200;
      size_t n = show - off; if (n > 200) { n = 200; }
      memcpy(piece, json + off, n); piece[n] = '\0';
      if (0 == p) {
        snprintf_P(line, sizeof(line), PSTR("EBG: SHIP %cX c=%d [%uB] 1/%d %s"), dir, classifier, (uint32_t)jl, parts, piece);
      } else if ((p == parts - 1) && (jl > 1000)) {
        snprintf_P(line, sizeof(line), PSTR("EBG: SHIP %d/%d %s..."), p + 1, parts, piece);
      } else {
        snprintf_P(line, sizeof(line), PSTR("EBG: SHIP %d/%d %s"), p + 1, parts, piece);
      }
      EebusLogLine(line);
    }
  } else {
    snprintf_P(line, sizeof(line), PSTR("EBG: SHIP %cX c=%d [%uB] %s"), dir, classifier, (uint32_t)jl, json);
    EebusLogLine(line);
  }
  if (!eebus_ram_log) { return; }
  if (nullptr == eebus_log_buf) {
    eebus_log_buf = (char*)special_malloc(EEBUS_LOGBUF_SZ);
    if (nullptr == eebus_log_buf) { eebus_ram_log = false; return; }
    eebus_log_len = 0; eebus_log_full = false;
  }
   // Zeile bounded anhaengen; wenn der Puffer voll ist, Aufnahme stoppen (kein Wrap = einfach+sicher)
  size_t room = EEBUS_LOGBUF_SZ - eebus_log_len;
  if (eebus_log_full || (room < 96)) { eebus_log_full = true; return; }
  int n = snprintf(eebus_log_buf + eebus_log_len, room, "%s %s %cX %d %.*s\n",
                   GetDateAndTime(DT_LOCAL).c_str(), ESp->peer_ip, dir, classifier,
                   (int)((jl < room - 64) ? jl : room - 64), json);
  if (n > 0) { eebus_log_len += ((size_t)n < room) ? (size_t)n : room - 1; }
}

// RAM-Mitschnitt auf SD schreiben — NUR bei getrennter Verbindung (sonst Crash-Risiko, s.o.)
void EebusLogDump(void) {
#ifdef USE_UFILESYS
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {   // ALLE Slots muessen getrennt sein
    if ((SME_OFF != EConn[i].sme) && (SME_FAIL != EConn[i].sme)) {
      ResponseCmndChar_P(PSTR("Dump nur bei getrennten Verbindungen (EEBusDisconnect zuerst)"));
      return;
    }
  }
  if ((nullptr == eebus_log_buf) || (0 == eebus_log_len)) { ResponseCmndChar_P(PSTR("Puffer leer")); return; }
  FS *fs = ufsp ? ufsp : ffsp;
  if (nullptr == fs) { ResponseCmndChar_P(PSTR("kein FS")); return; }
  File f = fs->open(EEBUS_SHIP_LOGFILE, "a");
  if (f) {
    f.write((const uint8_t*)eebus_log_buf, eebus_log_len);
    f.close();
    Response_P(PSTR("{\"EEBusLog\":{\"Dumped\":%u,\"Full\":%s}}"), (uint32_t)eebus_log_len, eebus_log_full ? "true":"false");
    eebus_log_len = 0; eebus_log_full = false;
  } else {
    ResponseCmndChar_P(PSTR("open fehlgeschlagen"));
  }
#endif
}

// Naiver String-Extraktor "key":"value" — reicht fuer die winzigen SHIP-Control-Messages
// (kontrolliertes Vokabular, keine Escapes). Kein vollwertiger JSON-Parser noetig.
bool EebusJsonStr(const char *json, const char *key, char *out, size_t outlen) {
   // v242 (SHIP-Test TC_SHIP_MSG_003): Leerraum um den Doppelpunkt zulassen. Ein Treffer, dessen Wert kein Text ist
   // (oder der gar kein Schluessel ist), wird uebersprungen - wie bisher beim Muster "key":".
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const size_t pl = strlen(pat);
  const char *p = json;
  while (nullptr != (p = strstr(p, pat))) {
    const char *q = p + pl;
    while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
    if (':' == *q) {
      q++;
      while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
      if ('"' == *q) {
        q++;
        const char *e = strchr(q, '"');
        if (nullptr == e) { return false; }
        size_t l = e - q;
        if (l >= outlen) { l = outlen - 1; }
        memcpy(out, q, l);
        out[l] = '\0';
        return true;
      }
    }
    p += pl;
  }
  return false;
}

// Naiver Integer-Extraktor "key":<zahl> (keine Anfuehrungszeichen). true wenn gefunden.
bool EebusJsonInt(const char *json, const char *key, uint32_t *out) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\"", key);   // v242: Leerraum vor dem Doppelpunkt zulassen
  const size_t pl = strlen(pat);
  const char *p = json;
  while (nullptr != (p = strstr(p, pat))) {
    const char *q = p + pl;
    while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
    if (':' == *q) {
      q++;
      while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
      if ((*q < '0') || (*q > '9')) { return false; }
      *out = (uint32_t)strtoul(q, nullptr, 10);
      return true;
    }
    p += pl;
  }
  return false;
}

// Control-/End-Message senden: [classifier][JSON] als WS-Binaerframe + Mitschnitt.
// SPINE-Datagramme (classifier 2) koennen gross sein -> dynamischer Puffer.
bool EebusShipSendJson(const char *json, int classifier) {
  size_t jl = strlen(json);
  uint8_t stackbuf[256];   // klein halten (Stack-Schonung); Groesseres -> Heap
  uint8_t *buf = stackbuf;
  bool heap = false;
  if (jl + 1 > sizeof(stackbuf)) {
    buf = (uint8_t*)special_malloc(jl + 1);
    if (nullptr == buf) { return false; }
    heap = true;
    EebusHeapCheckN("ship_malloc", (uint32_t)(jl + 1));   // nach PSRAM-Allok des grossen Puffers
  }
  buf[0] = (uint8_t)classifier;
  memcpy(buf + 1, json, jl);
  if (heap) { EebusHeapCheckN("ship_memcpy", (uint32_t)jl); }   // nach memcpy in den Puffer
  EebusShipLog('T', classifier, json);
  bool ok = EebusWsSend(buf, jl + 1);
  if (heap) { EebusHeapCheckN("ship_afterwssend", (uint32_t)jl); }
  if (heap) { free(buf); }
  EebusHeapCheck("tx_send");   // Checkpunkt: nach jedem gesendeten SHIP-Frame (auch nach free)
  return ok;
}

const char* EebusSmeName(int sme) {
  switch (sme) {
    case SME_HELLO: return "hello";
    case SME_PROT:  return "prot_hs";
    case SME_PIN:   return "pin_gate";
    case SME_ACCESS:return "access_gate";
    case SME_DONE:  return "done";
    case SME_FAIL:  return "fail";
    case SME_CMI:   return "cmi";
    default:        return "off";
  }
}

// v242: Hilfen fuer die SHIP-Pruefungen
// Kopfzeile "name: wert" (Gross-/Kleinschreibung egal) aus einem HTTP-Kopf lesen.
bool EebusHdrWert(const char *h, const char *name, char *out, size_t outlen) {
  const size_t nl = strlen(name);
  for (const char *p = h; p && *p; ) {
    if ((0 == strncasecmp(p, name, nl)) && (':' == p[nl])) {
      const char *v = p + nl + 1;
      while ((' ' == *v) || ('\t' == *v)) { v++; }
      size_t k = 0;
      while (v[k] && ('\r' != v[k]) && ('\n' != v[k]) && (k < outlen - 1)) { out[k] = v[k]; k++; }
      while (k && ((' ' == out[k - 1]) || ('\t' == out[k - 1]))) { k--; }
      out[k] = '\0';
      return true;
    }
    p = strchr(p, '\n');
    if (p) { p++; }
  }
  return false;
}
// enthaelt s den Text t (Gross-/Kleinschreibung egal)?
bool EebusTextHatI(const char *s, const char *t) {
  const size_t tl = strlen(t);
  for (; s && *s; s++) { if (0 == strncasecmp(s, t, tl)) { return true; } }
  return false;
}
// zwei JSON-Texte gleich bis auf Leerraum? (SHIP Z. 2422-2424: Spiegel des select)
bool EebusJsonGleich(const char *a, const char *b) {
  for (;;) {
    while ((' ' == *a) || ('\t' == *a) || ('\r' == *a) || ('\n' == *a)) { a++; }
    while ((' ' == *b) || ('\t' == *b) || ('\r' == *b) || ('\n' == *b)) { b++; }
    if (*a != *b) { return false; }
    if ('\0' == *a) { return true; }
    a++; b++;
  }
}
// SHIP Z. 2409-2410: genau EIN Format und das ist JSON-UTF8?
bool EebusFormatNurJson(const char *json) {
  const char *f = strstr(json, "\"format\"");
  if (nullptr == f) { return false; }
  f = strchr(f + 8, '[');
  if (nullptr == f) { return false; }
  const char *e = strchr(f, ']');
  if (nullptr == e) { return false; }
  char inh[24]; size_t k = 0;
  for (const char *c = f + 1; (c < e) && (k < sizeof(inh) - 1); c++) {
    if ((' ' != *c) && ('\t' != *c) && ('\r' != *c) && ('\n' != *c)) { inh[k++] = *c; }
  }
  inh[k] = '\0';
  return (0 == strcmp(inh, "\"JSON-UTF8\""));
}
// SubjectKeyIdentifier (OID 2.5.29.14) aus einem DER-Zertifikat lesen (SHIP Z. 1265-1269).
bool EebusDerSkiExt(const uint8_t *der, size_t len, uint8_t *out20) {
  if ((nullptr == der) || (len < 32)) { return false; }
  for (size_t i = 0; i + 5 + 4 + 20 <= len; i++) {
    if ((0x06 != der[i]) || (0x03 != der[i + 1]) || (0x55 != der[i + 2]) || (0x1D != der[i + 3]) || (0x0E != der[i + 4])) { continue; }
    size_t p = i + 5;
    if ((p + 3 <= len) && (0x01 == der[p]) && (0x01 == der[p + 1])) { p += 3; }   // critical (BOOLEAN)
    if ((p + 4 + 20 > len) || (0x04 != der[p]) || (0x16 != der[p + 1]) || (0x04 != der[p + 2]) || (0x14 != der[p + 3])) { return false; }
    memcpy(out20, der + p + 4, 20);
    return true;
  }
  return false;
}
// SKI in einer Datei mit einer SKI je Zeile (Partnerliste, Vertrauensliste)?
bool EebusSkiInDatei(const char *datei, const char *ski) {
#ifdef USE_UFILESYS
  if ((nullptr == ski) || (40 != strlen(ski)) || !TfsFileExists(datei)) { return false; }
  File f = ffsp->open(datei, "r");
  if (!f) { return false; }
  char buf[EEBUS_AC_MAX * 41 + 1];
  int n = f.read((uint8_t*)buf, sizeof(buf) - 1);
  f.close();
  if (n <= 0) { return false; }
  buf[n] = '\0';
  for (char *s = buf; *s; ) {
    while (*s && !isxdigit((unsigned char)*s)) { s++; }
    size_t k = 0;
    while (isxdigit((unsigned char)s[k])) { k++; }
    if ((40 == k) && (0 == strncasecmp(s, ski, 40))) { return true; }
    s += k;
  }
#endif
  return false;
}
// v242 (SHIP Z. 2125-2126, 2255-2256): darf diese SKI uns anrufen? Vertraut ist, wen der Betreiber kennt: gemerkte
// Partner, EEBusTrust-Liste, ausdrueckliche Betriebsart-Zuordnung, oder wen wir selbst gerade angerufen haben.
bool EebusSkiVertraut(const char *ski) {
  if ((nullptr == ski) || (40 != strlen(ski))) { return false; }
  for (uint8_t i = 0; i < eebus_ac_n; i++) { if (0 == strcasecmp(eebus_ac_list[i], ski)) { return true; } }
  if (EebusSkiInDatei(EEBUS_PEER_FILE, ski) || EebusSkiInDatei(EEBUS_TRUST_FILE, ski)) { return true; }
  if (EebusModeForSki(ski) >= 0) { return true; }
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (EConn[i].active && !EConn[i].via_srv && (0 == strcasecmp(EConn[i].peer_ski, ski))) { return true; }
  }
  return false;
}

// Handshake abbrechen: Grund merken, Verbindung freigeben, Uebersicht aktualisieren
void EebusSmeFail(const char *why) {
  snprintf(ESp->err, sizeof(ESp->err), "sme: %s", why);
  AddLog(LOG_LEVEL_ERROR, PSTR("EBG: SHIP-Handshake fehlgeschlagen (%s) @ %s"), why, ESp->peer_ip);
  EebusStatSet(ESp->peer_ski, SHIP_ERROR, ESp->err);
  ESp->sme = SME_FAIL;
  ESp->state = SHIP_ERROR;
  EebusTeardownLater();   // Client verzoegert freigeben (nie aus dem Callback)
}

// v242 (SHIP Z. 2292-2297, 2441-2446): gemeinsamer Abbruch - erst "aborted" bzw. protocolHandshakeError senden, dann
// schliessen. prot_err: 1 Timeout, 2 unerwartete Nachricht, 3 Auswahl passt nicht.
void EebusSmeAbbruch(const char *why, int prot_err) {
  if (SME_HELLO == ESp->sme) {
    EebusShipSendJson("{\"connectionHello\":[{\"phase\":\"aborted\"}]}", 1);
  } else if (SME_PROT == ESp->sme) {
    char e[64]; snprintf(e, sizeof(e), "{\"messageProtocolHandshakeError\":[{\"error\":%d}]}", prot_err ? prot_err : 2);
    EebusShipSendJson(e, 1);
  }
  EebusSmeFail(why);
}
// v242 (SHIP Z. 2298-2306): Hello-Update - "ready" mit der Restzeit des Wait-For-Ready-Timers.
void EebusSmeHelloUpdate(void) {
  int32_t rest = (int32_t)(ESp->sme_deadline - millis());
  if (rest < 0) { rest = 0; }
  char h[64]; snprintf(h, sizeof(h), "{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":%ld}]}", (long)rest);
  EebusShipSendJson(h, 1);
}
// v242: Verbindung abbauen (Doppelverbindung, Pong-Frist). abmelden: connectionClose "announce" (nur in der Datenphase).
void EebusVerbindungWeg(bool abmelden) {
  if (abmelden && (SME_DONE == ESp->sme)) {
    EebusShipSendJson("{\"connectionClose\":[{\"phase\":\"announce\"},{\"maxTime\":500},{\"reason\":\"unspecific\"}]}", 3);
  }
  EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
  ESp->sme = SME_OFF;
  ESp->state = SHIP_IDLE;
  ESp->last_rx = 0;
  ESp->pong_wait_at = 0; ESp->dbl_at = 0;
  if (ESp->via_srv) { EebusSrvFree(); return; }
  if (ESp->keepalive && (ESp->peer_idx >= 0)) { ESp->reconnect_at = Eebus126At(EebusKaWaitMs()); }
  EebusTeardownLater();
}
// v242 (SHIP Z. 2068-2075): CMI-Antwort des Servers auswerten - nur MessageType und CmiHead.
void EebusCmiAntwort(const uint8_t *rx, int n) {
  if ((n < 2) || (0x00 != rx[0]) || (0x00 != rx[1])) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s: ungueltige CMI (Typ %u, Kopf %u) - Verbindung wird geschlossen"),
           ESp->peer_ip, (n > 0) ? rx[0] : 255, (n > 1) ? rx[1] : 255);
    EebusSmeFail("cmi ungueltig");
    return;
  }
  ESp->state = SHIP_CMI_OK;
  EebusHeapCheck("cmi");
  EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP verbunden %s:%u, CMI ok, Peer-SKI %s"), ESp->peer_ip, ESp->peer_port, ESp->peer_ski);
  EebusSmeStart();
}

// Nach CMI-ok den SME-Handshake starten: wir sind sofort "ready" (die Box ruft nur Partner an, denen sie vertraut).
void EebusSmeStart(void) {
  ESp->peer_id[0] = '\0';
  ESp->sme_pending_logged = false;
   // SHIP 13.4.4.1.3: Update-Message immer mit maximaler Wartezeit melden
  if (!EebusShipSendJson("{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":60000}]}", 1)) {
    EebusSmeFail("hello send"); return;
  }
  ESp->sme = SME_HELLO;
  ESp->sme_hello_start = millis();
  ESp->sme_deadline = Eebus126At(EEBUS_SME_HELLO_TIMEOUT_MS);
}

/*********************************************************************************************\
 * MEILENSTEIN 3 — SPINE-Datenphase (classifier 2). Wir stellen uns als steuerbarer
 * Verbraucher (ChargingStation) vor, damit der Peer (Energiemanager/Waermepumpen-Gateway) uns akzeptiert und die
 * Verbindung dauerhaft haelt (grünes Kettenglied). Ablauf nach "Done":
 *   Peer liest von uns nodeManagementDetailedDiscoveryData -> wir antworten (unser Geraet,
 *   Entities, Features). Danach nodeManagementUseCaseData -> leer. Schreibt der Peer spaeter
 *   ein LPC-Limit (write), quittieren wir mit result.
 *
 * SPINE-JSON-Eigenart: Objekte = Arrays einelementiger Objekte (reihenfolge-unabhaengig),
 * entity = Array [n], feature = Zahl. Envelope wie mitgeschnitten:
 *   {"data":[{"header":[{"protocolId":"ee1.0"}]},{"payload":{"datagram":[<hdr>,<payload>]}}]}
\*********************************************************************************************/

// Ein SPINE-Datagramm senden (classifier 2). cmd_json = innerer cmd-Inhalt (ein Objekt).
// has_ref/ref = msgCounterReference (bei reply/result auf die Peer-Nachricht).
// src_ent/src_feat = unsere Absende-Adresse, dst_ent/dst_feat = Ziel beim Peer.
// wenn true, wird beim naechsten Send KEINE Ziel-device-Adresse gesetzt (nur fuer den
// DetailedDiscovery-Read noetig — s.u.). Wird direkt nach dem Send wieder zurueckgesetzt.
bool eebus_omit_destdev = false;
// dst_ent_list (optional): Ziel-Entity als fertige LISTE, z.B. "6,1" fuer eine Untereinheit.
// Warum: eine Entity-Adresse ist nach SPINE eine LISTE, nicht eine Zahl — eine Batterie IN einem
// Speicher hat die Adresse [6,1]. Mit nur einem int war sie nicht adressierbar; ein Read an [6]
// beantwortet die Gegenstelle mit errorNumber 4 "Ziel unbekannt" (gemessen 29.07. am Speicher und
// an der PV). Bleibt der Parameter leer, wird wie bisher die einzelne Zahl gesetzt — alle
// bestehenden Aufrufer bleiben damit zeichengleich.
void EebusSpineSendAddr(const char *cmd_classifier, bool has_ref, uint32_t ref, const char *cmd_json,
                        int src_ent, int src_feat, int dst_ent, int dst_feat,
                        const char *dst_ent_list = nullptr) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
  char ref_frag[48]; ref_frag[0] = '\0';
  if (has_ref) { snprintf(ref_frag, sizeof(ref_frag), "{\"msgCounterReference\":%u},", ref); }

  size_t need = strlen(cmd_json) + strlen(own) + (2 * strlen(ESp->peer_dev)) + 512;
  char *buf = (char*)special_malloc(need);
  if (nullptr == buf) { return; }
  EebusHeapCheckN("spine_malloc", (uint32_t)need);   // nach PSRAM-Allok Datagramm-Puffer

   // addressDestination bekommt die Peer-Geraeteadresse nur, wenn wir sie schon kennen.
   // AUSNAHME DetailedDiscovery-READ — der MUSS OHNE Ziel-device gesendet werden (man
   // discovert die Adresse ja gerade). Referenz-Umsetzung/Referenz-Umsetzung verwirft einen Discovery-Read MIT device als
   // "device address mismatch" (device_remote.go:274) und antwortet NIE -> lpc_disco blieb leer ->
   // Fallback -> falsches Ziel. Der Energiemanager war tolerant, deshalb fiel es dort nie auf.
  char dest_dev[96]; dest_dev[0] = '\0';
  if (ESp->peer_dev[0] && !eebus_omit_destdev) { snprintf(dest_dev, sizeof(dest_dev), "{\"device\":\"%s\"},", ESp->peer_dev); }

   // READ bekommt KEIN ackRequest (wie Referenz-Umsetzung Referenz-Umsetzung feature_local.go:419 -> Request(...,false)).
   // Die Reply IST die Antwort auf einen Read; ein zusaetzliches ackRequest:true ist bei Reads
   // nicht spec-konform -> der strenge Energiemanager/Waermepumpen-Gateway ignoriert unseren Discovery-Read damit (die
   // tolerante Ladestation beantwortete ihn trotzdem). Nur write/call behalten ackRequest:true.
   // ackRequest MUSS im SPINE-Header NACH cmdClassifier stehen (SPINE-JSON ist ein GEORDNETES Array).
   // Frame-Beweis: die Vergleichs-Steuerbox sendet ...{"cmdClassifier":"call"},{"ackRequest":true}. Wir setzten
   // es DAVOR ({"ackRequest":true},{"cmdClassifier":"call"}) -> der strenge Energiemanager/Waermepumpen-Gateway verwarf den Header und
   // verarbeitete KEINEN unserer Calls (Subscribe/Bind) -> 0 results, Bind-Timeout. Reads (ohne ackRequest)
   // gingen durch. Der tolerante Ladestation nahm auch die falsche Reihenfolge. Deshalb halfen /67/68 nie.
   // -> fuehrender Komma, KEIN abschliessendes: wird jetzt HINTER cmdClassifier eingefuegt.
  const char *ack = (has_ref
                     || (0 == strcmp(cmd_classifier, "notify"))
                     || (0 == strcmp(cmd_classifier, "read"))) ? "" : ",{\"ackRequest\":true}";

   // Ziel-Entity: fertige Liste, wenn eine mitgegeben wurde, sonst die einzelne Zahl.
  char dst_ent_txt[24];
  if ((nullptr != dst_ent_list) && dst_ent_list[0]) { snprintf(dst_ent_txt, sizeof(dst_ent_txt), "%s", dst_ent_list); }
  else                                             { snprintf(dst_ent_txt, sizeof(dst_ent_txt), "%d", dst_ent); }

  snprintf(buf, need,
    "{\"data\":[{\"header\":[{\"protocolId\":\"ee1.0\"}]},{\"payload\":{\"datagram\":[{\"header\":["
    "{\"specificationVersion\":\"%s\"},"
    "{\"addressSource\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
    "{\"addressDestination\":[%s{\"entity\":[%s]},{\"feature\":%d}]},"
    "{\"msgCounter\":%u},"
    "%s"   // ref_frag (msgCounterReference) VOR cmdClassifier
    "{\"cmdClassifier\":\"%s\"}"
    "%s"   // ackRequest NACH cmdClassifier (SPINE-Reihenfolge, wie Vergleichs-Steuerbox)
    "]},{\"payload\":[{\"cmd\":[[%s]]}]}]}}]}",
    EEBUS_SPINE_VERSION, own, src_ent, src_feat, dest_dev, dst_ent_txt, dst_feat,
    ESp->spine_ctr++, ref_frag, cmd_classifier, ack, cmd_json);
  if (!has_ref && (0 == strcmp(cmd_classifier, "write"))) {
    ESp->lpc_mc = ESp->spine_ctr - 1;   // v231 (4.8): die Antwort darauf muss diese Nummer tragen
  }

  EebusHeapCheckN("spine_format", (uint32_t)strlen(buf));   // nach snprintf ins Datagramm
  if (!EebusShipSendJson(buf, 2)) {   // v186: melden; den Abbau macht seit v187 EebusSendBroken
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SPINE-Senden FEHLGESCHLAGEN @ %s (%s, %u B)"),
           ESp->peer_ip, cmd_classifier, (uint32_t)strlen(buf));
  }
  EebusHeapCheck("spine_aftersend");   // nach dem Senden, vor free
  free(buf);
  EebusHeapCheck("spine_freed");   // nach free des Datagramm-Puffers
}

// Kompatibilitaets-Wrapper: NodeManagement-Verkehr (entity[0]/feature 0 beidseitig)
void EebusSpineSend(const char *cmd_classifier, bool has_ref, uint32_t ref, const char *cmd_json) {
  EebusSpineSendAddr(cmd_classifier, has_ref, ref, cmd_json, 0, 0, 0, 0);
}

// Unsere Detailed Discovery — FORMATGETREU nach der Ladestation (die von Energiemanager-Hersteller + Waermepumpen-Hersteller AKZEPTIERT
// wird!). Der Waermepumpen-Gateway ist STRENG beim SPINE-Format und lehnte unsere fruehere (flache) Antwort ab
// -> staendiges Nachfragen + Trennen nach ~20 s. Kernunterschied: entityInformation/
// featureInformation sind Arrays VON Arrays; jede Funktion ist [{"function":..},{"possibleOperations":..}]
// (getrennte Elemente), "read":[] (leer). Zunaechst NUR entity[0] NodeManagement (klein, crashfrei),
// LPC-Features folgen sauber danach.
// as_notify=1 -> dieselben Daten als NOTIFY an den NM-Abonnenten (statt reply auf einen
// Read). Referenz-Umsetzung liefert frischen NodeManagement-Abonnenten die Entity-Liste aktiv — alte,
// abo-orientierte Stacks (Waermepumpen-Hersteller) erwarten das moeglicherweise, bevor sie sich engagieren.
void EebusSpineAnswerDiscovery(uint32_t ref, int as_notify) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
  const size_t cap = 5504;   // 2 Entities; +LPC-Client-Features (Rolle 1); +lastStateChange/descriptions; +Measurement/ElectricalConnection/DeviceConfiguration-Server (~4,6 KB real)
  char *cmd = (char*)special_malloc(cap);   // Heap (Stack-Schutz im tiefen SPINE/BearSSL-Sendepfad)
  if (nullptr == cmd) { return; }
   // Role 1 = reine Controller-Identitaet (Netz-/EnergyGuard). Wir treten als steuernde Seite auf:
   // LoadControl/DeviceConfiguration/ElectricalConnection/Measurement als CLIENT (wir lesen/schreiben
   // beim gesteuerten System), NICHT als Server. DeviceClassification liegt auf entity[0]/feature[1],
   // NodeManagement fuehrt destinationListData. Nur so ordnet ein strenger §14a-Endpunkt uns als
   // gleichwertigen Controller ein und erwidert seine Detailed Discovery — frueher stuften strenge
   // Peers unsere Server-Features als 'steuerbares Geraet' ein und revanchierten sich nicht. Der
   // DeviceDiagnosis-Server (Heartbeat, entity[1]/feature[5]) bleibt fuer Liveness-Ueberwachung.
  if (1 == eebus_role) {
   // lastStateChange NUR im Notify anhaengen. Die akzeptierte Vergleichs-Steuerbox
   // sendet es im Reply NICHT; nur der Notify-Pfad braucht es (Referenz-Umsetzung processNotifyDetailedDiscoveryData,
   // sonst "invalid EntityInformation"). So ist der Reply feldgleich zur Vergleichs-Steuerbox, der Notify bleibt korrekt.
    const char *lsc = as_notify ? ",{\"lastStateChange\":\"added\"}" : "";
    snprintf(cmd, cap,
      "{\"nodeManagementDetailedDiscoveryData\":["
        "{\"specificationVersionList\":[{\"specificationVersion\":[\"%s\"]}]},"
        "{\"deviceInformation\":[{\"description\":["
          "{\"deviceAddress\":[{\"device\":\"%s\"}]},"
          "{\"deviceType\":\"ElectricitySupplySystem\"},"
          "{\"networkFeatureSet\":\"smart\"}"
        "]}]},"
        "{\"entityInformation\":["
          "[{\"description\":[{\"entityAddress\":[{\"entity\":[0]}]},{\"entityType\":\"DeviceInformation\"}%s]}],"   // lsc nur im Notify
          "[{\"description\":[{\"entityAddress\":[{\"entity\":[1]}]},{\"entityType\":\"GridGuard\"}%s]}]"
        "]},"
        "{\"featureInformation\":["
          "[{\"description\":["
            "{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[0]},{\"feature\":0}]},{\"featureType\":\"NodeManagement\"},{\"role\":\"special\"},"
            "{\"supportedFunction\":["
              "[{\"function\":\"nodeManagementBindingRequestCall\"},{\"possibleOperations\":[]}],"
              "[{\"function\":\"nodeManagementBindingDeleteCall\"},{\"possibleOperations\":[]}],"
              "[{\"function\":\"nodeManagementUseCaseData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
              "[{\"function\":\"nodeManagementBindingData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
              "[{\"function\":\"nodeManagementDestinationListData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
              "[{\"function\":\"nodeManagementDetailedDiscoveryData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
              "[{\"function\":\"nodeManagementSubscriptionData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
              "[{\"function\":\"nodeManagementSubscriptionRequestCall\"},{\"possibleOperations\":[]}],"
              "[{\"function\":\"nodeManagementSubscriptionDeleteCall\"},{\"possibleOperations\":[]}]"
            "]}"
          "]}],"
          "[{\"description\":["
            "{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[0]},{\"feature\":1}]},{\"featureType\":\"DeviceClassification\"},{\"role\":\"server\"},"
            "{\"supportedFunction\":[[{\"function\":\"deviceClassificationManufacturerData\"},{\"possibleOperations\":[{\"read\":[]}]}]]}"
          "]}],"
          "[{\"description\":[{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[1]},{\"feature\":1}]},{\"featureType\":\"DeviceDiagnosis\"},{\"role\":\"client\"},{\"description\":\"DeviceDiagnosis Client\"}]}],"
          "[{\"description\":[{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[1]},{\"feature\":2}]},{\"featureType\":\"LoadControl\"},{\"role\":\"client\"},{\"description\":\"LoadControl Client\"}]}],"   // feat2 = 1:1 zur Vergleichs-Steuerbox (deren akzeptierter Write kam von ent1/feat2); passt zu EEBUS_LPC_CLIENT_FEAT=2
          "[{\"description\":[{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[1]},{\"feature\":3}]},{\"featureType\":\"DeviceConfiguration\"},{\"role\":\"client\"},{\"description\":\"DeviceConfiguration Client\"}]}],"
          "[{\"description\":[{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[1]},{\"feature\":4}]},{\"featureType\":\"ElectricalConnection\"},{\"role\":\"client\"},{\"description\":\"ElectricalConnection Client\"}]}],"
          "[{\"description\":["
            "{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[1]},{\"feature\":5}]},{\"featureType\":\"DeviceDiagnosis\"},{\"role\":\"server\"},"
   // deviceDiagnosisStateData aus der Deklaration ENTFERNT -> nur
   // deviceDiagnosisHeartbeatData, EXAKT wie die akzeptierte Vergleichs-Steuerbox (5-Agenten-Diff: der EINZIGE
   // strukturelle Unterschied unserer Selbst-Deklaration war diese Zusatz-Funktion; die Vergleichs-Steuerbox deklariert
   // nur Heartbeat und antwortet auf einen State-Read trotzdem mit leerem State — das macht unser
   // Reply-Handler weiterhin). frueher hatte sie ergaenzt (Annahme, der Energiemanager brauche sie) -> nicht die Ursache.
            "{\"supportedFunction\":[[{\"function\":\"deviceDiagnosisHeartbeatData\"},{\"possibleOperations\":[{\"read\":[]}]}]]},{\"description\":\"DeviceDiagnosis Server\"}"
          "]}],"
          "[{\"description\":[{\"featureAddress\":[{\"device\":\"%s\"},{\"entity\":[1]},{\"feature\":6}]},{\"featureType\":\"Measurement\"},{\"role\":\"client\"},{\"description\":\"Measurement Client\"}]}]"
        "]}"
      "]}", EEBUS_SPINE_VERSION, own, lsc, lsc, own, own, own, own, own, own, own, own);
    if (as_notify) {
   // ein DetailedDiscovery-NOTIFY MUSS ein PARTIAL sein (function + filter cmdControl:partial)
   // VOR den Daten — sonst diff't Referenz-Umsetzung/Energiemanager gegen den bekannten Stand -> leer -> errorNumber:1
   // "invalid EntityInformation" (an der Vergleichs-Steuerbox referenz-bewiesen). Reply bleibt unveraendert (ok).
      const char *pfx = "{\"function\":\"nodeManagementDetailedDiscoveryData\"},{\"filter\":[[{\"cmdControl\":[{\"partial\":[]}]}]]},";
      size_t ncap = strlen(pfx) + strlen(cmd) + 4;
      char *ncmd = (char*)special_malloc(ncap);
      if (ncmd) { snprintf(ncmd, ncap, "%s%s", pfx, cmd); EebusSpineSend("notify", false, 0, ncmd); free(ncmd); }
    } else {
      EebusSpineSend("reply", true, ref, cmd);
    }
    free(cmd);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SPINE Detailed Discovery %s (Controller-Identitaet) @ %s"),
           as_notify ? "als NOTIFY geliefert" : "beantwortet", ESp->peer_ip);
    return;
  }
   // Rolle EIN = STEUERBOX: als ElectricitySupplySystem/GridGuard melden = die Identitaet einer
   // Steuerbox-Deklaration (eine
   // Waermepumpen-Hersteller aroTHERM nachweislich limitiert hat). Peers pruefen den Typ nicht auf Ablehnung
   // (Ladestation-Quellcode: get_address_of_feature filtert nur UseCase-Aktor "EnergyGuard" + Feature-
   // Typ/Rolle; deviceType laut deren eebus.h "can be freely defined") — der Typ ist aber das,
   // was strengere Gegenstellen (Waermepumpen-Gateway) als §14a-Controller erwarten. Rolle AUS =
   // ChargingStation/EVSE (steuerbares Geraet). Als "ChargingStation" behandelt uns die Ladestation
   // als Ladestation, NICHT als §14a-Steuergeraet.
   // Rolle 2: CEM-Identitaet (EnergyManagementSystem/CEM) — wie die VR940-Bridge, der die
   // Waermepumpen-Gateways ihre Discovery liefern. LPC-Spec 3.2: Energy Guard darf CEM ODER GridGuard sein.
   // Rolle 3: als steuerbares System auftreten (deviceType/entityType passend zum
   // ControllableSystem-Aktor), damit der Energiemanager unser LoadControl als lesenswert einordnet.
  const char *dev_type  = (3 == eebus_role) ? "ElectricitySupplySystem" :
                          (2 == eebus_role) ? "EnergyManagementSystem" :
                          (1 == eebus_role) ? "ElectricitySupplySystem" : "ChargingStation";
  const char *ent1_type = (3 == eebus_role) ? "GridConnectionHub" :
                          (2 == eebus_role) ? "CEM" :
                          (1 == eebus_role) ? "GridGuard" : "EVSE";
  const char *ent1_lbl  = (3 == eebus_role) ? "Controllable System" :
                          (eebus_role >= 1) ? "Energy Guard"       : "Controllable System";
  snprintf(cmd, cap,
    "{\"nodeManagementDetailedDiscoveryData\":["
      "{\"specificationVersionList\":[{\"specificationVersion\":[\"%s\"]}]},"
      "{\"deviceInformation\":[{\"description\":["
        "{\"deviceAddress\":[{\"device\":\"%s\"}]},"
        "{\"deviceType\":\"%s\"},"
        "{\"networkFeatureSet\":\"simple\"},"
        "{\"label\":\"Tasmota EEBus Guard\"},"
        "{\"description\":\"Steuerbox\"}"
      "]}]},"
      "{\"entityInformation\":["
        "[{\"description\":[{\"entityAddress\":[{\"entity\":[0]}]},{\"entityType\":\"DeviceInformation\"},{\"lastStateChange\":\"added\"},{\"label\":\"Node Management\"},{\"description\":\"Device Information\"}]}],"
        "[{\"description\":[{\"entityAddress\":[{\"entity\":[1]}]},{\"entityType\":\"%s\"},{\"lastStateChange\":\"added\"},{\"label\":\"%s\"},{\"description\":\"%s\"}]}]"
      "]},"
      "{\"featureInformation\":["
        "[{\"description\":["
          "{\"featureAddress\":[{\"entity\":[0]},{\"feature\":0}]},{\"featureType\":\"NodeManagement\"},{\"role\":\"special\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"nodeManagementUseCaseData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"nodeManagementDetailedDiscoveryData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"nodeManagementBindingData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"nodeManagementBindingDeleteCall\"},{\"possibleOperations\":[]}],"
            "[{\"function\":\"nodeManagementBindingRequestCall\"},{\"possibleOperations\":[]}],"
            "[{\"function\":\"nodeManagementSubscriptionData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"nodeManagementSubscriptionDeleteCall\"},{\"possibleOperations\":[]}],"
            "[{\"function\":\"nodeManagementSubscriptionRequestCall\"},{\"possibleOperations\":[]}]"
          "]},{\"description\":\"Node Management\"}"
        "]}],"
        "[{\"description\":["
          "{\"featureAddress\":[{\"entity\":[1]},{\"feature\":2}]},{\"featureType\":\"DeviceDiagnosis\"},{\"role\":\"server\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"deviceDiagnosisHeartbeatData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"deviceDiagnosisStateData\"},{\"possibleOperations\":[{\"read\":[]}]}]"
          "]},{\"description\":\"Device Diagnosis\"}"
        "]}],"
        "[{\"description\":["
          "{\"featureAddress\":[{\"entity\":[1]},{\"feature\":6}]},{\"featureType\":\"LoadControl\"},{\"role\":\"server\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"loadControlLimitDescriptionListData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"loadControlLimitListData\"},{\"possibleOperations\":[{\"read\":[]},{\"write\":[]}]}]"
          "]},{\"description\":\"Load Control\"}"
        "]}],"
        "[{\"description\":["
          "{\"featureAddress\":[{\"entity\":[1]},{\"feature\":14}]},{\"featureType\":\"DeviceClassification\"},{\"role\":\"server\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"deviceClassificationManufacturerData\"},{\"possibleOperations\":[{\"read\":[]}]}]"
          "]},{\"description\":\"Device Classification\"}"
        "]}],"
        "[{\"description\":["   // Measurement-Server (MGCP/MPC-Daten — Netzanschlusspunkt-Leistung)
          "{\"featureAddress\":[{\"entity\":[1]},{\"feature\":8}]},{\"featureType\":\"Measurement\"},{\"role\":\"server\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"measurementDescriptionListData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"measurementListData\"},{\"possibleOperations\":[{\"read\":[]}]}]"
          "]},{\"description\":\"Measurement Grid Connection\"}"
        "]}],"
        "[{\"description\":["   // ElectricalConnection-Server (MGCP-Parameter)
          "{\"featureAddress\":[{\"entity\":[1]},{\"feature\":9}]},{\"featureType\":\"ElectricalConnection\"},{\"role\":\"server\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"electricalConnectionDescriptionListData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"electricalConnectionParameterDescriptionListData\"},{\"possibleOperations\":[{\"read\":[]}]}]"
          "]},{\"description\":\"Electrical Connection\"}"
        "]}],"
        "[{\"description\":["   // DeviceConfiguration-Server (MGCP Szenario 1: PV-Curtailment-Faktor)
          "{\"featureAddress\":[{\"entity\":[1]},{\"feature\":10}]},{\"featureType\":\"DeviceConfiguration\"},{\"role\":\"server\"},"
          "{\"supportedFunction\":["
            "[{\"function\":\"deviceConfigurationKeyValueDescriptionListData\"},{\"possibleOperations\":[{\"read\":[]}]}],"
            "[{\"function\":\"deviceConfigurationKeyValueListData\"},{\"possibleOperations\":[{\"read\":[]}]}]"
          "]},{\"description\":\"Device Configuration\"}"
        "]}]"
      "%s]}"
    "]}", EEBUS_SPINE_VERSION, own, dev_type, ent1_type, ent1_lbl, ent1_lbl,
   // Rolle 3 = ControllableSystem -> KEIN LoadControl-Client (nur Server ent1/feat6); nur Rolle 1/2 (EnergyGuard) hat den Controller-Client
    ((1 == eebus_role) || (2 == eebus_role)) ? EEBUS_LPC_CLIENT_FEATURE : "");
  if (as_notify) {
    EebusSpineSend("notify", false, 0, cmd);   // aktive Lieferung an den NM-Abonnenten
  } else {
    EebusSpineSend("reply", true, ref, cmd);
  }
  free(cmd);
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SPINE Detailed Discovery %s (SPINE-Format%s) @ %s"),
         as_notify ? "als NOTIFY geliefert" : "beantwortet",
         (eebus_role >= 1) ? "+LPC-Client" : "", ESp->peer_ip);
}

// Use-Case-Daten: LPC (limitationOfPowerConsumption). ROLLE bestimmt den Aktor:
//  - Rolle AUS (Lesen): wir sind "ControllableSystem" (gesteuertes Geraet) — so akzeptiert uns der Energiemanager.
//  - Rolle EIN (Steuerbox): wir sind "EnergyGuard" (die STEUERNDE Seite) — nur so nimmt die Ladestation
//    unser Binding/Limit an (Test: als ControllableSystem ignorierte sie die Binding-Anfrage).
// Format wie Ladestation (Array von Arrays).
void EebusSpineAnswerUseCase(uint32_t ref) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
  const size_t cap = 1792;   // EnergyGuard(LPC+LPP) + MonitoringAppliance(MGCP+MPC) wie Vergleichs-Steuerbox
  char *cmd = (char*)special_malloc(cap);   // Heap statt Stack (Crash-Schutzregel c: Sendepfad ist tief)
  if (nullptr == cmd) { return; }
   // Rolle EIN: BEIDE Aktoren deklarieren. Nur ControllableSystem -> Peer engagiert sich, ignoriert
   // aber unser Binding . Nur EnergyGuard -> Peer engagiert sich GAR NICHT (Ladestation stumm).
   // Beide -> Peer liest uns weiter (wir lernen seine Adresse) UND akzeptiert uns als Controller.
   // useCaseAvailable PFLICHT: Ladestation prueft es (spine_connection.cpp:231) als Teil von get_address_of_feature
   // (DeviceDiagnosis role=CLIENT, LPC, actor=EnergyGuard) - ist es false, findet sie unser feat4-client NICHT
   // als HB-Peer -> kein initialize_heartbeat_on_feature -> kein Abo -> Failsafe 22000 statt unserem Wert.
   // "DATEN-FUTTER": zusaetzlich die PROVIDER-Seiten der Monitoring-Use-Cases deklarieren —
   // MGCP (actor GridConnectionPoint, Netzanschlusspunkt-Daten) + MPC (actor MonitoredUnit,
   // Leistungsdaten). Die Vergleichs-Steuerbox bietet 4 UCs; Waermepumpen-Gateways haben CEM-CLIENT-
   // Entities (Daten-KONSUMENTEN, vom Energiemanager via Energiemanager gefuettert) — These: die Waermepumpen-Gateway engagiert sich
   // erst mit einem Manager, der auch Daten LIEFERT. Werte: Measurement feat8 (simuliert).
   // Rolle 3 (HEMS-Modell/Energiemanager): als "ControllableSystem" auftreten — ein HEMS liest/abonniert
   // das LoadControl NUR von einem ControllableSystem (Mitschnitt-Beweis: Energiemanager liest unsere Discovery/
   // UseCaseData 31×/8×, abonniert NM+DeviceDiagnosis, rührt LoadControl aber NIE an, weil wir als
   // "EnergyGuard"=Controller auftraten). Rolle 1/2 bleibt EnergyGuard (Ladestation-Schreibpfad, unberuehrt).
  const bool is_guard = (eebus_role >= 1) && (3 != eebus_role);   // Rolle 1/2 = EnergyGuard-Controller
  if (is_guard) {
   // UseCaseData BYTE-GLEICH wie die Vergleichs-Steuerbox (Frame-Beweis):
   // EnergyGuard mit LPC **UND LPP** (ein Block), MonitoringAppliance mit MGCP[1-7] + MPC[1-5] (ein Block).
   // meldete nur LPC[1-4] + MGCP[1-4] + MPC[1-3] -> der Energiemanager las uns EINMAL aus und DISENGAGTE danach
   // (abonnierte uns nicht, quittierte 0 unserer Calls, Bind-Timeout). Live-Beweis Energiemanager: nach unserer
   // UseCaseData kam vom Energiemanager NICHTS mehr. Die Vergleichs-Steuerbox (vollstaendiger EnergyGuard LPC+LPP + volle Scenarios) wird
   // als steuerberechtigter Controller akzeptiert (14x errorNumber 0). LPP + volle scenarioSupport = Vollprofil.
    snprintf(cmd, cap,
      "{\"nodeManagementUseCaseData\":[{\"useCaseInformation\":["
        "[{\"address\":[{\"device\":\"%s\"},{\"entity\":[1]}]},{\"actor\":\"EnergyGuard\"},"
        "{\"useCaseSupport\":["
          "[{\"useCaseName\":\"limitationOfPowerConsumption\"},{\"useCaseVersion\":\"1.0.0\"},{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3,4]},{\"useCaseDocumentSubRevision\":\"release\"}],"
          "[{\"useCaseName\":\"limitationOfPowerProduction\"},{\"useCaseVersion\":\"1.0.0\"},{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3,4]},{\"useCaseDocumentSubRevision\":\"release\"}]"
        "]}],"
        "[{\"address\":[{\"device\":\"%s\"},{\"entity\":[1]}]},{\"actor\":\"MonitoringAppliance\"},"
        "{\"useCaseSupport\":["
          "[{\"useCaseName\":\"monitoringOfGridConnectionPoint\"},{\"useCaseVersion\":\"1.0.0\"},{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3,4,5,6,7]},{\"useCaseDocumentSubRevision\":\"release\"}],"
          "[{\"useCaseName\":\"monitoringOfPowerConsumption\"},{\"useCaseVersion\":\"1.0.0\"},{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3,4,5]},{\"useCaseDocumentSubRevision\":\"release\"}]"
        "]}]"
      "]}]}", own, own);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: UseCaseData EnergyGuard(LPC+LPP)+MonitoringAppliance(MGCP+MPC) @ %s"), ESp->peer_ip);
  } else {
   // Rolle 3/0 = ControllableSystem (steuerbares Geraet, unveraendert)
    snprintf(cmd, cap,
      "{\"nodeManagementUseCaseData\":[{\"useCaseInformation\":["
        "[{\"address\":[{\"device\":\"%s\"},{\"entity\":[1]}]},{\"actor\":\"ControllableSystem\"},"
        "{\"useCaseSupport\":[[{\"useCaseName\":\"limitationOfPowerConsumption\"},{\"useCaseVersion\":\"1.0.0\"},"
        "{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3,4]},{\"useCaseDocumentSubRevision\":\"release\"}]]}],"
        "[{\"address\":[{\"device\":\"%s\"},{\"entity\":[1]}]},{\"actor\":\"MonitoringAppliance\"},"
        "{\"useCaseSupport\":[[{\"useCaseName\":\"monitoringOfGridConnectionPoint\"},{\"useCaseVersion\":\"1.0.0\"},"
        "{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3,4]},{\"useCaseDocumentSubRevision\":\"release\"}]]}],"
        "[{\"address\":[{\"device\":\"%s\"},{\"entity\":[1]}]},{\"actor\":\"MonitoringAppliance\"},"
        "{\"useCaseSupport\":[[{\"useCaseName\":\"monitoringOfPowerConsumption\"},{\"useCaseVersion\":\"1.0.0\"},"
        "{\"useCaseAvailable\":true},{\"scenarioSupport\":[1,2,3]},{\"useCaseDocumentSubRevision\":\"release\"}]]}]"
      "]}]}", own, own, own);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: UseCaseData als ControllableSystem + MonitoringAppliance gesendet @ %s"), ESp->peer_ip);
  }
  EebusSpineSend("reply", true, ref, cmd);
  free(cmd);
}

// nodeManagementDestinationListData-Read beantworten wie die Vergleichs-Steuerbox (Frame-Beweis,
// Frame-Mitschnitt): mit ECHTER Selbstbeschreibung, NICHT mit resultData errorNumber:0.
// Strenge Peers (Energiemanager/Waermepumpen-Gateway) onboarden in der Kette Discovery->DestinationList->UseCaseData->Bind-Quittung;
// unsere fruehere Falsch-Antwort (resultData errorNumber:0, Read-Fallthrough Z.~1973) brach die Kette ab
// -> der Energiemanager las unsere UseCaseData nicht mehr und quittierte unseren Bind NIE (0 results ueber alle Sessions).
// Der tolerante Ladestation verzieh die Falsch-Antwort (deshalb ging er, Energiemanager/Waermepumpen-Gateway nicht). deviceType wie Discovery
// (Rolle 1 = ElectricitySupplySystem, GridGuard-Controller-Identitaet).
void EebusSpineAnswerDestinationList(uint32_t ref) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
  char *cmd = (char*)special_malloc(384);
  if (nullptr == cmd) { return; }
  snprintf(cmd, 384,
    "{\"nodeManagementDestinationListData\":[{\"nodeManagementDestinationData\":[["
      "{\"deviceDescription\":[{\"deviceAddress\":[{\"device\":\"%s\"}]},"
      "{\"deviceType\":\"ElectricitySupplySystem\"},{\"networkFeatureSet\":\"smart\"}]}"
    "]]}]}", own);
  EebusSpineSend("reply", true, ref, cmd);
  free(cmd);
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: DestinationListData beantwortet (echte Selbstbeschreibung) @ %s"), ESp->peer_ip);
}

// Entity/Feature aus einem SPINE-Adressblock ziehen (frag zeigt AUF den Adress-Schluessel).
void EebusAddrParse(const char *frag, int *ent, int *feat) {
  if (nullptr == frag) { return; }
  const char *e = strstr(frag, "\"entity\":[");
  if (e) { *ent = atoi(e + 10); }
  uint32_t f = 0;
  if (EebusJsonInt(frag, "feature", &f)) { *feat = (int)f; }
}

// HERSTELLERNEUTRAL — das SERVER-Feature eines bestimmten featureType auf einer bestimmten Entity
// aus der Detailed-Discovery holen (statt fester Feature-Nummern wie feat24). -1 = nicht vorhanden.
int EebusLpcFindServerFeat(const char *disco, int want_ent, const char *want_ftype) {
  const char *scan = disco, *fa;
  while (nullptr != (fa = strstr(scan, "\"featureAddress\""))) {
    scan = fa + 16;
    const char *nextfa = strstr(scan, "\"featureAddress\"");
    const char *ft = strstr(fa, "\"featureType\":\"");
    if ((nullptr == ft) || (nextfa && (ft > nextfa))) { continue; }
    char ftype[24] = { 0 }; EebusJsonStr(ft, "featureType", ftype, sizeof(ftype));
    if (0 != strcmp(ftype, want_ftype)) { continue; }
    const char *rl = strstr(ft, "\"role\":\"");
    if ((nullptr == rl) || (nextfa && (rl > nextfa))) { continue; }
    char role[16] = { 0 }; EebusJsonStr(rl, "role", role, sizeof(role));
    if (0 != strcmp(role, "server")) { continue; }
    int ent = -1, feat = -1; EebusAddrParse(fa, &ent, &feat);
    if ((ent == want_ent) && (feat >= 0)) { return feat; }
  }
  return -1;
}

// das LoadControl-SERVER-Feature (entity/feature) aus der Detailed-Discovery des Peers
// lernen und als LPC-Ziel setzen — ersetzt die fest verdrahtete Ladestation-Default-Adresse (ent1/feat6).
// So braucht man fuer Peers, die ihre Discovery liefern (Energiemanager, Ladestation), kein manuelles EEBusTarget mehr.
// Ein aktiver EEBusTarget-Override hat weiter Vorrang (wird in EebusLpcStart nach dem Lernen gesetzt).
// true wenn ein LoadControl-Server gefunden + uebernommen wurde.
bool EebusLpcLearnTarget(const char *disco) {
  const char *scan = disco;
  const char *fa;
  while (nullptr != (fa = strstr(scan, "\"featureAddress\""))) {
    scan = fa + 16;   // ab hier weiter (naechste Runde)
    const char *nextfa = strstr(scan, "\"featureAddress\"");
    const char *ft = strstr(fa, "\"featureType\":\"");
    if ((nullptr == ft) || (nextfa && (ft > nextfa))) { continue; }   // featureType gehoert zum naechsten Block
    char ftype[24] = { 0 };
    EebusJsonStr(ft, "featureType", ftype, sizeof(ftype));
    if (0 != strcmp(ftype, "LoadControl")) { continue; }
    const char *rl = strstr(ft, "\"role\":\"");
    if ((nullptr == rl) || (nextfa && (rl > nextfa))) { continue; }
    char role[16] = { 0 };
    EebusJsonStr(rl, "role", role, sizeof(role));
    if (0 != strcmp(role, "server")) { continue; }   // nur das steuerbare (server) LoadControl
    int ent = -1, feat = -1;
    EebusAddrParse(fa, &ent, &feat);
    if ((ent >= 0) && (feat >= 0)) {
      if ((ESp->lpc_peer_ent != ent) || (ESp->lpc_peer_feat != feat)) {
        ESp->lpc_bound = false; ESp->lpc_limit_id = -1; ESp->lpc_lid_dir[0] = ESp->lpc_lid_dir[1] = -1;   // Ziel geaendert -> Binding/limitId neu holen
        ESp->lpc_onboarded = false;   // neues Ziel -> erneut onboarden
        ESp->lpc_fs_val_key = -1; ESp->lpc_fs_dur_key = -1; ESp->lpc_fs_prod_key = -1; ESp->lpc_fs_done = false; ESp->lpc_fs_step = 0;   // neues DeviceConfig-Feat -> Failsafe-Keys neu lernen+schreiben
      }
      ESp->lpc_peer_ent = ent; ESp->lpc_peer_feat = feat;
   // DeviceConfiguration-SERVER-Feature auf DERSELBEN Entity aus der Discovery holen (kein feat24-Hardcode).
   // -1 = Peer hat auf der Ziel-Entity keine DeviceConfiguration -> Read wird spaeter uebersprungen.
      ESp->lpc_peer_dcfg_feat = EebusLpcFindServerFeat(disco, ent, "DeviceConfiguration");
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: LoadControl-Ziel aus Discovery gelernt: ent%d/feat%d, DeviceConfig-feat%d @ %s"),
             ent, feat, ESp->lpc_peer_dcfg_feat, ESp->peer_ip);
      return true;
    }
  }
  return false;
}

// v241 (LPC TS Tab. 22 + Z. 1509-1511, LPP TS entsprechend, LPC IG 3.4 Z. 646-649): die limitId GENAU der Grenze,
// die die Norm beschreibt: limitType signDependentAbsValueLimit, limitCategory obligation, limitDirection consume
// (dir 0, Paragraf 14a) bzw. produce (dir 1, Paragraf 9 EEG), scopeType activePowerLimit. Fehlt die measurementId, ist
// die Beschreibung ungueltig (IG 3.4: "SHALL consider ... as invalid"). Bis v240 Rueckfall auf maxValueLimit bzw. die
// erste limitId - damit landete eine Vorgabe womoeglich auf einer fremden Grenze. true wenn eine gueltige id gefunden.
bool EebusLpcPickLimitId(const char *desc, uint8_t dir, int *out_id) {
  const char *want = (1 == dir) ? "\"limitDirection\":\"produce\"" : "\"limitDirection\":\"consume\"";
  const char *p = desc;
  while (nullptr != (p = strstr(p, "\"limitId\":"))) {
    const char *nx = strstr(p + 10, "\"limitId\":");
    auto hat = [&](const char *k) -> bool { const char *f = strstr(p, k); return (nullptr != f) && ((nullptr == nx) || (f < nx)); };
    if (hat(want) && hat("\"limitType\":\"signDependentAbsValueLimit\"") && hat("\"limitCategory\":\"obligation\"") &&
        hat("\"scopeType\":\"activePowerLimit\"")) {
      uint32_t id = 0;
      if (EebusJsonInt(p, "limitId", &id)) {
        if (!hat("\"measurementId\":")) {
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Grenze limitId %u @ %s ohne measurementId - Beschreibung ungueltig (LPC IG 3.4)"), id, ESp->peer_ip);
          return false;
        }
        *out_id = (int)id;
        return true;
      }
    }
    p += 10;
  }
  return false;
}

// v241 (LPC TS Tab. 23): steht im Block dieser limitId der Schluessel k (z. B. "isLimitChangeable":false)?
bool EebusLimitBlockHas(const char *json, int lid, const char *k) {
  if ((nullptr == json) || (lid < 0)) { return false; }
  char pat[24]; snprintf(pat, sizeof(pat), "\"limitId\":%d", lid);
  const char *p = json;
  while (nullptr != (p = strstr(p, pat))) {
    const char c = p[strlen(pat)];
    if ((c >= '0') && (c <= '9')) { p += 10; continue; }   // limitId 1 ist nicht limitId 12
    const char *nx = strstr(p + 10, "\"limitId\":");
    const char *f = strstr(p, k);
    return (nullptr != f) && ((nullptr == nx) || (f < nx));
  }
  return false;
}

// v241 (SPINE TS 5.2.5.3 Z. 1193-1198, Z. 1214): Antwortfrist der LPC-Kette. Gibt die Anlage in ihrer Discovery eine
// maxResponseDelay an, gilt diese (SHALL) plus Uebertragungsreserve; nie kuerzer als die bisherige Frist.
uint32_t EebusLpcFristMs(void) {
  uint32_t m = (uint32_t)ESp->mrd_s * 1000UL + 10000UL;
  return (ESp->mrd_s && (m > EEBUS_LPC_PHASE_TIMEOUT_MS)) ? m : EEBUS_LPC_PHASE_TIMEOUT_MS;
}

// v241: groesste maxResponseDelay der Features LoadControl/DeviceConfiguration aus der Discovery der Anlage.
void EebusLernMrd(const char *disco) {
  ESp->mrd_s = 0;
  if (nullptr == disco) { return; }
  const char *fa = disco;
  while (nullptr != (fa = strstr(fa, "\"featureAddress\""))) {
    const char *nx = strstr(fa + 16, "\"featureAddress\"");
    const char *ft = strstr(fa, "\"featureType\":\"");
    const char *md = strstr(fa, "\"maxResponseDelay\":\"");
    if (ft && md && (!nx || ((ft < nx) && (md < nx))) &&
        ((0 == strncmp(ft + 15, "LoadControl\"", 12)) || (0 == strncmp(ft + 15, "DeviceConfiguration\"", 20)))) {
      char d[20] = { 0 };
      if (EebusJsonStr(md, "maxResponseDelay", d, sizeof(d))) {
        long s = EebusIsoDurSecs(d);
        if ((s > 0) && (s < 3600) && ((uint16_t)s > ESp->mrd_s)) { ESp->mrd_s = (uint16_t)s; }
      }
    }
    fa += 16;
  }
  if (ESp->mrd_s) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s nennt maxResponseDelay %u s - Antwortfrist %u s (SPINE 5.2.5.3)"),
           ESp->peer_ip, ESp->mrd_s, (uint32_t)(EebusLpcFristMs() / 1000));
  }
}

// v241 (LPC TS Z. 1466, SPINE Z. 2406-2416): eigene Abos merken, Ergebnis auswerten, abgelehnte regelmaessig lesen,
// Herzschlag-Abo bei Herzschlagverlust erneuern. typ = unsere Client-Feature-Nummer (wie in EebusLpcSubscribeAll).
const char *EebusAboName(uint8_t typ) {
  switch (typ) {
    case 1: return "DeviceDiagnosis";
    case 2: return "LoadControl";
    case 3: return "DeviceConfiguration";
    case 4: return "ElectricalConnection";
    case 6: return "Measurement";
  }
  return "";
}
const char *EebusAboLesen(uint8_t typ) {
  switch (typ) {
    case 1: return "deviceDiagnosisHeartbeatData";
    case 2: return "loadControlLimitListData";
    case 3: return "deviceConfigurationKeyValueListData";
    case 4: return "electricalConnectionCharacteristicListData";
    case 6: return "measurementListData";
  }
  return "";
}
void EebusAboMerken(uint8_t typ, int ent, int feat) {
  uint8_t i = 0;
  for (; i < ESp->abo_n; i++) {
    if ((ESp->abo_typ[i] == typ) && (ESp->abo_ent[i] == ent) && (ESp->abo_feat[i] == feat)) { break; }
  }
  if (i >= ESp->abo_n) {
    if (ESp->abo_n >= EEBUS_ABO_MAX) { return; }
    i = ESp->abo_n++;
    ESp->abo_typ[i] = typ; ESp->abo_ent[i] = (int16_t)ent; ESp->abo_feat[i] = (int16_t)feat;
  }
  ESp->abo_mc[i] = ESp->spine_ctr - 1;   // die gerade verbrauchte Nummer
  ESp->abo_st[i] = 0;
}
void EebusAboErgebnis(const char *json) {
  uint32_t r = 0, e = 0;
  if (!ESp->abo_n || !strstr(json, "\"resultData\"") || !EebusJsonInt(json, "msgCounterReference", &r) || !r) { return; }
  EebusJsonInt(json, "errorNumber", &e);
  for (uint8_t i = 0; i < ESp->abo_n; i++) {
    if ((ESp->abo_mc[i] != r) || (0 != ESp->abo_st[i])) { continue; }
    ESp->abo_mc[i] = 0;
    if (0 == e) { ESp->abo_st[i] = 1; return; }
    ESp->abo_st[i] = 2;
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Abo %s ent%d/feat%d @ %s abgelehnt (errorNumber %u) - wird alle %u s gelesen (LPC TS Z. 1466)"),
           EebusAboName(ESp->abo_typ[i]), ESp->abo_ent[i], ESp->abo_feat[i], ESp->peer_ip, e, (uint32_t)(EEBUS_ABO_POLL_MS / 1000));
    char t[64]; snprintf(t, sizeof(t), "%s abgelehnt (Fehler %u) - wird gelesen", EebusAboName(ESp->abo_typ[i]), e);
    EebusLbLog("Abo abgelehnt", ESp->peer_ski, ESp->peer_ip, -1, "", 0, t);
    if (!ESp->abo_poll_at) { ESp->abo_poll_at = Eebus126At(5000); }
    return;
  }
}
void EebusAboPoll(void) {
  char cmd[64];
  for (uint8_t i = 0; i < ESp->abo_n; i++) {
    if (2 != ESp->abo_st[i]) { continue; }
    snprintf(cmd, sizeof(cmd), "{\"%s\":[]}", EebusAboLesen(ESp->abo_typ[i]));
    EebusSpineSendAddr("read", false, 0, cmd, EEBUS_LPC_CLIENT_ENT, ESp->abo_typ[i], ESp->abo_ent[i], ESp->abo_feat[i]);
  }
}
void EebusAboErneuern(uint8_t typ) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
  const size_t cap = 512;
  char *cmd = (char*)special_malloc(cap);
  if (nullptr == cmd) { return; }
  for (uint8_t i = 0; i < ESp->abo_n; i++) {
    if (ESp->abo_typ[i] != typ) { continue; }
    snprintf(cmd, cap,
      "{\"nodeManagementSubscriptionRequestCall\":[{\"subscriptionRequest\":["
      "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
      "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
      "{\"serverFeatureType\":\"%s\"}]}]}",
      own, EEBUS_LPC_CLIENT_ENT, typ, ESp->peer_dev, ESp->abo_ent[i], ESp->abo_feat[i], EebusAboName(typ));
    EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
    ESp->abo_mc[i] = ESp->spine_ctr - 1; ESp->abo_st[i] = 0;
    snprintf(cmd, cap, "{\"%s\":[]}", EebusAboLesen(typ));
    EebusSpineSendAddr("read", false, 0, cmd, EEBUS_LPC_CLIENT_ENT, typ, ESp->abo_ent[i], ESp->abo_feat[i]);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Abo %s ent%d/feat%d @ %s erneuert und gelesen (SPINE Z. 2406-2409)"),
           EebusAboName(typ), ESp->abo_ent[i], ESp->abo_feat[i], ESp->peer_ip);
  }
  free(cmd);
}

// Heartbeat-JSON bauen und senden (reply auf read ODER notify an den Abonnenten).
// LPC-Pflicht: Verbindungsueberwachung, PT30S.
void EebusSpineSendHeartbeat(const char *cls, bool has_ref, uint32_t ref, int dst_ent, int dst_feat) {
  const size_t cap = 256;
  char *hb = (char*)special_malloc(cap);   // Heap statt Stack (Crash-Schutzregel c)
  if (nullptr == hb) { return; }
   // heartbeatTimeout PT60S -> PT10S = EXAKT wie die Vergleichs-Steuerbox, die der Energiemanager
   // HEUTE annimmt (Live-Mitschnitt: Vergleichs-Steuerbox sendet deviceDiagnosisHeartbeatData mit heartbeatTimeout
   // "PT10S"). Wir hielten uns bisher an die Norm (LPC-005 SHALL >=60s = PT60S) — der Energiemanager WILL aber den
   // engen 10-s-Herzschlag (Memory: "Energiemanager wertet EG erst bei engem HB <=10s als durchgaengig aktiv,
   // verlaesst init/failsafe"). Unser HEMS-Sendetakt ist bereits 8 s (< 10 s), passt also zu PT10S wie die Vergleichs-Steuerbox.
   // PT10S wurde bei uns NIE getestet (immer PT60S/PT30S). SteuVE (Ladestation) BEHAELT PT30S (verfaellt nach ~31 s,
   // 20-s-Sendetakt muss darunter bleiben -> nicht anfassen).
   // HEMS-Timeout von PT10S auf den NORM-WERT PT60S. Begruendung: die Norm verlangt
   // heartbeatTimeout = 60 s ([LPC-005]/[LPP-032]); die zehn Sekunden hatten wir nur uebernommen, weil
   // die Referenz-Steuerbox sie sendet. Bei 8 s Sendetakt blieb damit EINE Sekunde Reserve — stockt der
   // ESP kurz (SD-Zugriff, mDNS-Scan, oder die Wallbox flutet mit >100 Messwert-Meldungen je Sekunde),
   // ist der Herzschlag zu spaet und die Gegenstelle stuft uns als verschwunden ein. Genau das geschah
   //: der Peer fiel in seinen Failsafe und meldete die Steuerbox ab.
   // Mit PT60S bei unveraendertem 8-s-Takt duerfen sechs Herzschlaege ausfallen, bevor es kritisch wird.
   // Wir versprechen damit weniger, als wir halten — das ist die richtige Richtung fuer eine Steuerbox.
   // SteuVE (Ladestation) behaelt PT30S: dort ist der Sendetakt 20 s, und der bewiesene Drossel-Pfad bleibt
   // unangetastet.
   // v175: HEMS wieder PT10S wie die echte Steuerbox (Begruendung bei EEBUS_HEMS_HB_MS). Die Reserve
   // kommt jetzt aus dem kuerzeren Sendetakt (4 s), nicht aus einer groesseren Zusage.
   // v231 (Pruefung 3.10): BEWUSSTE ABWEICHUNG von LPC Tab. 17 Fn. *2 ("heartbeatTimeout SHALL be 60 seconds"). Der
   // Energiemanager lehnte mit 60 s jede Grenze ab (err7), mit PT10S wie die echte Steuerbox nicht (v175). Gesendet wird
   // ohnehin oefter als alle 60 s ([LPC-005]). Nicht aendern ohne neuen Test am Energiemanager.
   // v240 (meierchen006 22.11.: "normgerechte Geraete"): LPC TS Tab. 13 Fn. *2 (Z. 1237-1239) heartbeatTimeout SHALL 60 s.
   // Bis v239 PT10S/PT30S fuer den SMA-Energiemanager (steuert nicht mehr). Gesendet wird weiter oefter (4 s / 20 s).
  const char *hb_to = "PT60S";
   // ZEITZONEN-KENNUNG "Z" am Zeitstempel (UTC). Der Wert ist ein XSD-dateTime; OHNE Zonen-
   // angabe ist er "unqualifizierte Ortszeit" — ein strenger Empfaenger rechnet ihn als SEINE
   // Ortszeit, unser Wert ist aber UTC. In der Sommerzeit liegt unser Herzschlag damit zwei Stunden
   // in der Vergangenheit und ist bei heartbeatTimeout PT10S dauerhaft abgelaufen -> die
   // Heartbeat-Bedingung des LPC-Szenarios 3 gilt als verletzt -> das Limit wird abgelehnt
   // (errorNumber 7), waehrend Reads/Bindings/Abos unbeeindruckt mit errorNumber 0 durchgehen.
   // Tolerante Gegenstellen werten den Zeitstempel nicht aus (deshalb funktionierte die Drosselung
   // dort). Referenz-Steuerbox und Peer senden beide "...THH:MM:SSZ" (sekundengenau, UTC, kein
   // Millisekunden-Anteil) — GetDT(UtcTime()) liefert exakt diese Zeichenkette, nur ohne das "Z".
   // Nicht auf den HEMS-Modus begrenzt: das Format war schlicht falsch und ist ueberall richtig.
   // v231 (Pruefung 4.11): ohne gestellte Uhr KEIN Zeitstempel statt einem aus 1970. Das Element ist nur empfohlen
   // (LPC Tab. 17: "R*3 timestamp SHALL hold the time of creation"); ein Wert aus 1970 laesst strenge Gegenstellen
   // den Herzschlag als abgelaufen werten. Tritt nur auf, wenn eine Gegenstelle anruft, bevor die Uhr steht.
  if (RtcTime.valid) {
    snprintf(hb, cap,
      "{\"deviceDiagnosisHeartbeatData\":[{\"timestamp\":\"%sZ\"},{\"heartbeatCounter\":%u},{\"heartbeatTimeout\":\"%s\"}]}",
      GetDT(UtcTime()).c_str(), ESp->hb_counter, hb_to);
  } else {
    snprintf(hb, cap,
      "{\"deviceDiagnosisHeartbeatData\":[{\"heartbeatCounter\":%u},{\"heartbeatTimeout\":\"%s\"}]}",
      ESp->hb_counter, hb_to);
  }
   // Quell-Feature unseres DeviceDiagnosis-Servers ROLLENABHAENGIG — Role 1 (Branch A) deklariert
   // ihn auf feat5 (1:1 Vergleichs-Steuerbox; feat2 ist jetzt der LoadControl-Client), Rolle 0/2/3 (Branch B) auf feat2.
   // Bisher wurde IMMER von feat2 gesendet -> in Role 1 stimmte die HB-Quelle NICHT mit der deklarierten
   // feat5 ueberein (der Energiemanager abonniert feat5, bekam den Notify aber von feat2 -> evtl. HB nicht zugeordnet).
  int hb_src_feat = (1 == eebus_role) ? 5 : 2;
  EebusSpineSendAddr(cls, has_ref, ref, hb, 1, hb_src_feat, dst_ent, dst_feat);   // von unserem DeviceDiagnosis-Server
  free(hb);
   // heartbeatCounter bei jedem NOTIFY hochzaehlen, bei read-Reply NICHT (LPC Tab. 17 [LPC-031]: "If the device sends
   // out a reply message the Element heartbeatCounter SHALL NOT be incremented"). v231: gilt jetzt auch im SteuVE-Modus
   // (bis v230 zaehlte er dort bei jedem Senden hoch, Pruefung 3.8).
  if (0 == strcmp(cls, "notify")) { ESp->hb_counter++; }
  ESp->hb_tx_at = millis();   // v231: fuer EEBusStatus "HerzschlagGesendetS" (Statuszeile der Seite)
}

// den aktuellen Server-Limit-Zustand als NOTIFY an den LoadControl-Abonnenten des
// AKTUELLEN Slots (ESp) schicken. Aufruf bei frischem Abo und bei jeder Limit-Aenderung.
void EebusServeLimitNotify(void) {
  if (!ESp->lc_sub) { return; }
  char body[224];
  if (eebus_serve_active) {
    snprintf(body, sizeof(body),
      "{\"loadControlLimitListData\":[{\"loadControlLimitData\":["
      "[{\"limitId\":1},{\"isLimitChangeable\":true},{\"isLimitActive\":true},"
      "{\"timePeriod\":[{\"endTime\":\"PT2H\"}]},{\"value\":[{\"number\":%u},{\"scale\":0}]}]"
      "]}]}", eebus_serve_watt);
  } else {
    strcpy(body,
      "{\"loadControlLimitListData\":[{\"loadControlLimitData\":["
      "[{\"limitId\":1},{\"isLimitChangeable\":true},{\"isLimitActive\":false},{\"value\":[{\"number\":0},{\"scale\":0}]}]"
      "]}]}");
  }
   // von UNSEREM LoadControl (ent1/feat6) an die Client-Adresse des Abonnenten
  EebusSpineSendAddr("notify", false, 0, body, 1, 6, ESp->lc_cli_ent, ESp->lc_cli_feat);
}

// DEAKTIVIERT (No-Op). Die Vergleichs-Steuerbox sendet NIE ein operatingState-Notify (0x im Erfolgs-
// Mitschnitt) und bekommt trotzdem err0 -> operatingState war NICHT die err7-Ursache; frueher war eine
// Fehlannahme. Ein EG stellt norm-seitig keinen operatingState bereit (Tab.12: nur Heartbeat); einen
// Betriebszustand zu melden liess uns wie einen Verbraucher/CS aussehen. Aufrufe bleiben, tun nichts.
void EebusServeStateNotify(void) {
  return;
}

// Ergebnistext richtigstellen, sobald die Geltungsdauer eines bestaetigten Limits abgelaufen ist.
// OHNE das hier bleibt "BESTAETIGT aktiv <Wert>" stehen, obwohl die Gegenstelle laengst wieder
// unbegrenzt faehrt: der Satz beschreibt den Schreibvorgang von damals, wird aber als Aussage
// ueber den JETZIGEN Zustand gelesen. Die Restzeit rechnen wir ohnehin schon mit (Empfangszeit +
// gemeldete Restlaufzeit) — hier wird sie nur ausgewertet. Kein Netzverkehr.
void EebusLpcExpireCheck(void) {
  EebusConn *save = ESp;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *cc = &EConn[i];
    if (!cc->active || (SME_DONE != cc->sme) || (LPC_DONE != cc->lpc_state)) { continue; }
    if (0 != strncmp(cc->lpc_result, "BESTAETIGT aktiv", 16)) { continue; }   // nur gesetzte Limits
    int d = (1 == cc->lpc_dir) ? 1 : 0;
    bool aus = false;
    if (cc->lim_dur[d][0] && (cc->lim_dur_s[d] >= 0)) {
      long weg = (long)((millis() - cc->lim_dur_at[d]) / 1000UL);
      aus = ((cc->lim_dur_s[d] - weg) <= 0);
    }
   // v241: die Anlage meldet nach dem Ablauf oft nur noch isLimitActive false ohne Dauer - dann zaehlt unser eigenes Ende
    for (int j = 0; !aus && (j < EEBUS_MAX_CONN); j++) {
      const EebusConn *oc = &EConn[j];
      if (oc->own_ski[0] && !strcasecmp(oc->own_ski, cc->peer_ski) && oc->own_ende[d] && ((int32_t)(oc->own_ende[d] - millis()) <= 0)) { aus = true; }
    }
    if (!aus) { continue; }                                                   // laeuft noch
    ESp = cc; Eebus126ModeLoad();
    EebusLpcSetResult(LPC_DONE, (1 == cc->lpc_dir) ? "ausgelaufen (Einspeisung wieder frei)"
                                                   : "ausgelaufen (Bezug wieder frei)");
  }
  ESp = save; Eebus126ModeLoad();
}

// v242 (SHIP Z. 1353-1366): zwei Verbindungen zur selben SKI. Die Seite mit der groesseren SKI behaelt die neueste und
// schliesst die aeltere sofort; die kleinere wartet 3 s, dann darf sie die aeltere schliessen. Ergebnis ist in beiden
// Faellen: die aeltere geht (in der Datenphase mit connectionClose). Der Keep-Alive baut nicht neu auf, solange die
// andere Verbindung steht.
void EebusDoppelPruefen(void) {
  EebusConn *save = ESp;
  bool dbl[EEBUS_MAX_CONN] = { false };
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *a = &EConn[i];
    if (!a->active || (1 != a->ski_quelle) || (SME_OFF == a->sme) || (SME_FAIL == a->sme)) { continue; }
    for (int j = i + 1; j < EEBUS_MAX_CONN; j++) {
      EebusConn *b = &EConn[j];
      if (!b->active || (1 != b->ski_quelle) || (SME_OFF == b->sme) || (SME_FAIL == b->sme)) { continue; }
      if (0 != strcasecmp(a->peer_ski, b->peer_ski)) { continue; }
      dbl[i] = dbl[j] = true;
      EebusConn *alt = ((int32_t)(a->conn_at - b->conn_at) <= 0) ? a : b;
      if (!alt->dbl_at) { alt->dbl_at = millis(); if (!alt->dbl_at) { alt->dbl_at = 1; } }
      const bool groesser = (strcasecmp(Eebus.own_ski, alt->peer_ski) > 0);
      if (groesser || ((millis() - alt->dbl_at) >= 3000UL)) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Doppelverbindung zu %s - die aeltere (%s, Slot %d) wird geschlossen (SHIP 12.2.2, %s SKI)"),
               alt->peer_ski, alt->via_srv ? "eingehend" : "ausgehend", (int)(alt - EConn), groesser ? "groessere" : "kleinere");
        ESp = alt; Eebus126ModeLoad();
        EebusVerbindungWeg(true);
        ESp = save; Eebus126ModeLoad();
        return;   // je Sekunde hoechstens ein Abbau
      }
    }
  }
  for (int k = 0; k < EEBUS_MAX_CONN; k++) { if (!dbl[k]) { EConn[k].dbl_at = 0; } }
  ESp = save; Eebus126ModeLoad();
}

// Server-Limit an ALLE LoadControl-Abonnenten (ueber alle Slots) verteilen.
void EebusServeLimitBroadcast(void) {
  EebusConn *save = ESp;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    ESp = &EConn[i]; Eebus126ModeLoad();
    if (ESp->active && ESp->lc_sub && (SME_DONE == ESp->sme)) {
      EebusServeLimitNotify();
    }
  }
  ESp = save; Eebus126ModeLoad();
}

// Antworten auf Reads der von uns deklarierten Features (der Peer liest sie nach der Discovery).
// true = beantwortet. Formate nach SPINE (Array einelementiger Objekte).
// our_ent/our_feat = welches UNSERER Features adressiert war (Antwort kommt von dort),
// peer_ent/peer_feat = Absender-Feature des Peers (Antwort geht dorthin).
bool EebusSpineAnswerFeatureRead(const char *json, uint32_t ref, int our_ent, int our_feat, int peer_ent, int peer_feat) {
  if (strstr(json, "deviceClassificationManufacturerData")) {
   // Fuenf Felder. deviceCode ist unsere SHIP-/mDNS-Kennung, serialNumber die daraus abgeleitete
   // Seriennummer — beide geraete-individuell, damit uns eine Gegenstelle eindeutig inventarisieren
   // kann. EebusSpineSendAddr nimmt einen FERTIGEN String, deshalb hier vorher zusammensetzen.
    char mfd[256];
    snprintf_P(mfd, sizeof(mfd),
      PSTR("{\"deviceClassificationManufacturerData\":[{\"brandName\":\"" EEBUS_ADV_BRAND "\"},"
           "{\"vendorName\":\"" EEBUS_ADV_BRAND "\"},{\"deviceName\":\"" EEBUS_ADV_MODEL "\"},"
           "{\"deviceCode\":\"%s\"},{\"serialNumber\":\"%s\"}]}"),
      eebus_adv_id, eebus_adv_serial);
    EebusSpineSendAddr("reply", true, ref, mfd, our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "deviceDiagnosisStateData")) {
   // LEER antworten, byte-exakt wie die Vergleichs-Steuerbox (ihre Reply auf den Energiemanager-State-Read ist
   // "deviceDiagnosisStateData":[] — KEIN operatingState; verifiziert im Erfolgs-Mitschnitt).
   // operatingState ist norm-seitig ein CS-/Verbraucher-Konzept (Tab.12: der EG stellt als Server
   // NUR deviceDiagnosisHeartbeatData bereit). Es zu senden liess uns wie einen Verbraucher aussehen.
   // (operatingState:normalOperation) war eine Fehlannahme: die Vergleichs-Steuerbox liefert es NIE, bekommt err0.
    EebusSpineSendAddr("reply", true, ref,
      "{\"deviceDiagnosisStateData\":[]}",
      our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "deviceDiagnosisHeartbeatData")) {
    EebusSpineSendHeartbeat("reply", true, ref, peer_ent, peer_feat);   // sofortige Antwort = 1. Heartbeat
   // Die Ladestation liest unser feat2 nur EINMAL (heartbeat.cpp send_full_read aus ihrem
   // Generic-client feat4) und abonniert unser DeviceDiagnosis NICHT (Mitschnitt: nur
   // NodeManagement-SubscriptionRequestCall). Ohne LAUFENDEN Heartbeat laeuft heartbeat_received bei
   // der Ladestation ab -> failsafe_state()=22000 statt unserem Limit. Loesung: die feat4-Absenderadresse
   // dieses Reads merken und ab jetzt PROAKTIV senden. Ladestations handle_message (heartbeat.cpp Z.106-108)
   // akzeptiert notify OHNE Abo -> emit_heartbeat_received -> heartbeat_received bleibt true -> in
   // update_state greift dann UNSER Wert. Reuse des periodischen Senders unten (hb_sub-Pfad).
     // v240 (SPINE TS Z. 2417-2419: notify an einen Nicht-Abonnenten ist ein Fehler; LPC TS 3.4.3.1 Nr. 3: das Abo ist
     // Pflicht der Anlage): auf ein blosses Lesen nur die Antwort oben - KEIN laufendes Notify mehr. Abonniert die Anlage
     // nicht, meldet v239 das in Log und Betriebslogbuch. Bis v239 Sonderweg fuer die Ladestation (liest nur, Notify ohne Abo).
    if (!ESp->hb_sub) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer %s liest unseren Heartbeat nur (ohne Abo) - Antwort ja, laufend nur mit Abo (SPINE)"), ESp->peer_ip);
    }
    return true;
  }
  if (1 != eebus_role) {   // v243 (MGCP Z. 913-914): Rolle 1 (Steuerbox) bietet diese Server nicht an -> errorNumber 6 unten
   // ===== "DATEN-FUTTER" — Provider-Antworten fuer MGCP/MPC (Werte SIMULIERT: konstante
   // Platzhalter, bis echte Quellen (Messsteckdose / Energiemanager-Umfeld) angebunden sind. Zweck: die
   // datenkonsumierenden CEM-Client-Entities der Waermepumpen-Gateways zum Engagement bewegen.)
   // Reihenfolge: Description-Zweige VOR den List-Zweigen (strstr-Teilstring-Sicherheit).
  if (strstr(json, "measurementDescriptionListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"measurementDescriptionListData\":[{\"measurementDescriptionData\":["
      "[{\"measurementId\":1},{\"measurementType\":\"power\"},{\"commodityType\":\"electricity\"},{\"unit\":\"W\"},{\"scopeType\":\"acPowerTotal\"}],"
      "[{\"measurementId\":2},{\"measurementType\":\"energy\"},{\"commodityType\":\"electricity\"},{\"unit\":\"Wh\"},{\"scopeType\":\"gridFeedIn\"}],"
      "[{\"measurementId\":3},{\"measurementType\":\"energy\"},{\"commodityType\":\"electricity\"},{\"unit\":\"Wh\"},{\"scopeType\":\"gridConsumption\"}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "measurementListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"measurementListData\":[{\"measurementData\":["
      "[{\"measurementId\":1},{\"valueType\":\"value\"},{\"value\":[{\"number\":500},{\"scale\":0}]},{\"valueSource\":\"measuredValue\"}],"
      "[{\"measurementId\":2},{\"valueType\":\"value\"},{\"value\":[{\"number\":0},{\"scale\":0}]},{\"valueSource\":\"measuredValue\"}],"
      "[{\"measurementId\":3},{\"valueType\":\"value\"},{\"value\":[{\"number\":0},{\"scale\":0}]},{\"valueSource\":\"measuredValue\"}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "electricalConnectionParameterDescriptionListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"electricalConnectionParameterDescriptionListData\":[{\"electricalConnectionParameterDescriptionData\":["
      "[{\"electricalConnectionId\":0},{\"parameterId\":1},{\"measurementId\":1},{\"voltageType\":\"ac\"},"
      "{\"acMeasuredPhases\":\"abc\"},{\"acMeasurementType\":\"real\"}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "electricalConnectionDescriptionListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"electricalConnectionDescriptionListData\":[{\"electricalConnectionDescriptionData\":["
      "[{\"electricalConnectionId\":0},{\"powerSupplyType\":\"ac\"},{\"acConnectedPhases\":3},"
      "{\"positiveEnergyDirection\":\"consume\"}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "deviceConfigurationKeyValueDescriptionListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"deviceConfigurationKeyValueDescriptionListData\":[{\"deviceConfigurationKeyValueDescriptionData\":["
      "[{\"keyId\":1},{\"keyName\":\"pvCurtailmentLimitFactor\"},{\"valueType\":\"scaledNumber\"}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "deviceConfigurationKeyValueListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"deviceConfigurationKeyValueListData\":[{\"deviceConfigurationKeyValueData\":["
      "[{\"keyId\":1},{\"value\":[{\"scaledNumber\":[{\"number\":100},{\"scale\":-2}]}]},{\"isValueChangeable\":false}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);   // Faktor 1,00 = keine PV-Abregelung
    return true;
  }
  }   // v243: Ende der Pruefrollen-Antworten
   // ===== Ende Datenantworten =====
  if (strstr(json, "loadControlLimitDescriptionListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"loadControlLimitDescriptionListData\":[{\"loadControlLimitDescriptionData\":["
      "[{\"limitId\":1},{\"limitType\":\"signDependentAbsValueLimit\"},{\"limitCategory\":\"obligation\"},"   // v241 LPC TS Tab. 22
      "{\"limitDirection\":\"consume\"},{\"measurementId\":1},{\"unit\":\"W\"},{\"scopeType\":\"activePowerLimit\"}]"
      "]}]}", our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  if (strstr(json, "loadControlLimitListData")) {
   // ECHTEN Server-Limit-Zustand liefern (nicht mehr Dummy). Der HEMS liest hier das
   // von uns bereitgestellte §14a-Limit (EEBusProvide) und verteilt es an seine Verbraucher.
    char body[224];
    if (eebus_serve_active) {
      snprintf(body, sizeof(body),
        "{\"loadControlLimitListData\":[{\"loadControlLimitData\":["
        "[{\"limitId\":1},{\"isLimitChangeable\":true},{\"isLimitActive\":true},"
        "{\"timePeriod\":[{\"endTime\":\"PT2H\"}]},{\"value\":[{\"number\":%u},{\"scale\":0}]}]"
        "]}]}", eebus_serve_watt);
    } else {
      strcpy(body,
        "{\"loadControlLimitListData\":[{\"loadControlLimitData\":["
        "[{\"limitId\":1},{\"isLimitChangeable\":true},{\"isLimitActive\":false},{\"value\":[{\"number\":0},{\"scale\":0}]}]"
        "]}]}");
    }
    EebusSpineSendAddr("reply", true, ref, body, our_ent, our_feat, peer_ent, peer_feat);
    return true;
  }
  return false;
}

// GEGENSEITIGKEIT (SHIP/SPINE Initial Peer Discovery, wie Ladestation initial_peer_discovery):
// Nach Done fragen WIR die Detailed Discovery des Peers ab. Ohne das stuft der Peer die
// Beziehung als unvollstaendig ein und trennt (grünes Kettenglied fiel deshalb ab).
void EebusSpineReadPeerDiscovery(void) {
  if (!ESp->peer_dev[0]) { return; }   // Peer-Geraeteadresse noch unbekannt -> spaeter erneut (bei jedem Frame)
   // (Defekt-A-Fix, an der Vergleichs-Steuerbox referenz-bewiesen ): Das NM-Subscribe DARF erst raus,
   // NACHDEM der Peer UNSERE Discovery gelesen hat (disco_answered) — sonst kennt seine SPINE-Schicht
   // unser Geraet noch nicht -> resultData errorNumber:1 "invalid addresses" (Referenz-Umsetzung util.go:
   // Adressaufloesung scheitert, wenn remoteDevice.Address() leer ist). Die Vergleichs-Steuerbox macht es genauso:
   // erst unsere Discovery lesen (wir antworten), DANN subscriben. frueher hatte subscribe-VOR-read -> genau
   // dieser Fehler. Der Discovery-READ selbst braucht KEIN Abo (nur die Peer-Adresse) und bleibt frueh.
  if (ESp->disco_answered && !ESp->we_nm_subscribed) {
    ESp->we_nm_subscribed = true;
    char own[80]; EebusOwnDevice(own, sizeof(own));
    char *sub = (char*)special_malloc(320);
    if (nullptr != sub) {
      snprintf(sub, 320,
        "{\"nodeManagementSubscriptionRequestCall\":[{\"subscriptionRequest\":["
          "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[0]},{\"feature\":0}]},"
          "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[0]},{\"feature\":0}]},"
          "{\"serverFeatureType\":\"NodeManagement\"}"
        "]}]}", own, ESp->peer_dev);
      EebusSpineSendAddr("call", false, 0, sub, 0, 0, 0, 0);
      free(sub);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SPINE NodeManagement des Peers abonniert (NACH gegenseitiger Discovery) @ %s"), ESp->peer_ip);
    }
  }
  if (ESp->peer_disco_read) { return; }   // Peer-Discovery bereits EINMAL adressiert abgefragt
  ESp->peer_disco_read = true;
   // Discovery-Read OHNE Ziel-device senden (sonst Referenz-Umsetzung "device address mismatch" -> keine
   // Antwort). Flag nur fuer genau diesen einen Send setzen, danach sofort zuruecknehmen.
  eebus_omit_destdev = true;
  EebusSpineSend("read", false, 0, "{\"nodeManagementDetailedDiscoveryData\":[]}");   // leeres ARRAY [] (nicht {}). Mitschnitt-Beweis: die Referenz-Umsetzung-Referenz sendet [] -> der strikte Energiemanager ANTWORTET; unser {} (Annahme, nur Ladestation-tolerant) wird vom Energiemanager still verworfen. LoadControl-Reads nutzten schon [].
  eebus_omit_destdev = false;
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SPINE Detailed Discovery des Peers abgefragt (adressiert) @ %s"), ESp->peer_ip);
}

/*********************************************************************************************\
 * — STEUERBOX-ROLLE: LPC-Limit an eine Controllable-System-SteuVE SCHREIBEN.
 * Sequenz (asynchron, getrieben aus EebusSpineHandle-Antworten + EebusSmePoll-Timeout):
 *   Binding (call NM->NM) -> Limit-Beschreibung lesen (read, limitId finden) ->
 *   Write loadControlLimitListData -> Read-Back verifizieren -> Done.
 * Wert active=false => Freigabe (Limit deaktivieren). Nur bei eebus_role>=1 (Steuerbox).
\*********************************************************************************************/

/*********************************************************************************************\
 * v215: Betriebslogbuch
 * VDE FNN Empfehlung zu Tenorziffer 2a BK6-22-300 (Okt. 2024):
 *  - Kap. 4.1.2.2: Bestaetigung der Wirkleistungslimitierung "wird durch die Steuerungseinrichtung im
 *    Betriebslogbuch protokolliert. Eine nicht erfolgte Bestaetigung ... wird ebenfalls protokolliert."
 *  - S. 25 Verbindungsueberwachung: Kommunikationsstoerung protokollieren; "Steuerbefehle muessen waehrend
 *    einer Kommunikationsstoerung abgewiesen werden, die Nichtausfuehrbarkeit muss fuer spaetere
 *    Nachweiszwecke protokolliert werden."
 * Ordnername, Monatsdateien, Spalten und Aufbewahrung (30 Monate) regelt die Empfehlung NICHT - unsere
 * Festlegung (meierchen006 26.10.2026; das Lastenheft Steuerbox liegt uns nicht vor). Nur auf der SD (Flash schonen),
 * Datei je Eintrag oeffnen/anhaengen/schliessen, abholbar ueber den FTP-Server der Steuerbox.
\*********************************************************************************************/
#ifdef USE_UFILESYS
extern uint8_t ufs_type;   // wie xdrv_10: xdrv_50_filesystem kommt in der Reihenfolge erst nach diesem Treiber
extern FS *ufsp;
#ifndef UFS_TSDC
#define UFS_TSDC 1         // Wert aus xdrv_50_filesystem (SD-Karte)
#endif
#endif
#define EEBUS_LB_DIR     "/betriebslogbuch"
#define EEBUS_LB_MONATE  30
void EebusLbAufraeumen(int jahr, int monat) {
#ifdef USE_UFILESYS
  const int grenze = jahr * 12 + (monat - 1) - EEBUS_LB_MONATE;   // Monate davor werden geloescht
  File d = ufsp->open(EEBUS_LB_DIR, "r");
  if (!d) { return; }
  char weg[8][40];
  int n = 0;
  for (File e = d.openNextFile(); e && (n < 8); e = d.openNextFile()) {
    const char *nm = e.name();
    const char *b = strrchr(nm, '/');
    b = b ? b + 1 : nm;
    int y = 0, m = 0;
    if ((2 == sscanf(b, "%4d-%2d.csv", &y, &m)) && ((y * 12 + m - 1) < grenze)) {
      snprintf(weg[n++], sizeof(weg[0]), EEBUS_LB_DIR "/%s", b);
    }
    e.close();
  }
  d.close();
  for (int i = 0; i < n; i++) {
    ufsp->remove(weg[i]);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Betriebslogbuch %s geloescht (aelter als %u Monate)"), weg[i], EEBUS_LB_MONATE);
  }
#endif
}
// v217: Betriebslogbuch nur mit SD-Karte (Flash schonen)
bool EebusLbAktiv(void) {
#ifdef USE_UFILESYS
  return ufsp && (UFS_TSDC == ufs_type);
#else
  return false;
#endif
}
// Ein Eintrag: Zeit_UTC;Ereignis;Anlage;Adresse;Richtung;Wert;Dauer_s;Ergebnis (dir < 0 = ohne Richtung)
void EebusLbLog(const char *ereignis, const char *ski, const char *ip, int dir, const char *wert, long dauer, const char *ergebnis) {
#ifdef USE_UFILESYS
  if (!EebusLbAktiv()) { return; }   // nur auf der SD-Karte (ohne SD: Meldung einmal je Start, EebusLbVerbindungen)
  char zeit[28], pfad[40];
  int jahr = 0, monat = 0;
  if (RtcTime.valid) {
    strlcpy(zeit, GetDateAndTime(DT_UTC).c_str(), sizeof(zeit) - 1);   // Tasmota haengt bei UTC selbst "Z" an
    if (zeit[0] && ('Z' != zeit[strlen(zeit) - 1])) { strlcat(zeit, "Z", sizeof(zeit)); }
    sscanf(zeit, "%4d-%2d", &jahr, &monat);
    snprintf(pfad, sizeof(pfad), EEBUS_LB_DIR "/%04d-%02d.csv", jahr, monat);
  } else {
    strlcpy(zeit, "Uhr-noch-nicht-gestellt", sizeof(zeit));
    strlcpy(pfad, EEBUS_LB_DIR "/ohne-uhrzeit.csv", sizeof(pfad));
  }
  if (!ufsp->exists(EEBUS_LB_DIR)) { ufsp->mkdir(EEBUS_LB_DIR); }
  const bool neu = !ufsp->exists(pfad);
  File f = ufsp->open(pfad, "a");
  if (!f) { AddLog(LOG_LEVEL_ERROR, PSTR("EBG: Betriebslogbuch %s nicht schreibbar"), pfad); return; }
  if (neu) { f.print("Zeit_UTC;Ereignis;Anlage;Adresse;Richtung;Wert;Dauer_s;Ergebnis\n"); }
  char erg[80];
  strlcpy(erg, ergebnis ? ergebnis : "", sizeof(erg));
  for (char *c = erg; *c; c++) { if ((';' == *c) || ('\n' == *c)) { *c = ','; } }
  char z[240];
  snprintf(z, sizeof(z), "%s;%s;%s;%s;%s;%s;%ld;%s\n", zeit, ereignis, ski ? ski : "", ip ? ip : "",
           (dir < 0) ? "" : (dir ? "9-Einspeisung" : "14a-Bezug"), wert ? wert : "", dauer, erg);
  f.print(z);
  f.close();
  if (neu && jahr) { EebusLbAufraeumen(jahr, monat); }   // je neuem Monat einmal alte Monate loeschen
#endif
}
// Ergebnis einer Vorgabe (Bestaetigung oder Nicht-Bestaetigung) - gleiche Wiederholung nicht doppelt
uint32_t EebusFsBezugCi(uint32_t ci);   // weiter unten (Failsafe-Werte)
extern uint32_t eebus_fs_dur_s;
void EebusLbVorgabeWert(char *wert, size_t n) {   // v245
  if (ESp->lpc_active_wish) { snprintf(wert, n, "%u W", ESp->lpc_value); } else { strlcpy(wert, "Freigabe", n); }
}
void EebusLbVorgabe(void) {
  char wert[24];
  if (0 == strncmp(ESp->lpc_result, "Failsafe", 8)) {   // Failsafe-Kette: eigenes Ereignis mit Wert und Dauer (FNN 4.1.2.5)
    snprintf(wert, sizeof(wert), "%u W", EebusFsBezugCi((uint32_t)(ESp - EConn)));
    char k[80];
    snprintf(k, sizeof(k), "FS|%s|%s", wert, ESp->lpc_result);
    if (0 == strcmp(k, ESp->lb_last)) { return; }
    strlcpy(ESp->lb_last, k, sizeof(ESp->lb_last));
    EebusLbLog("Failsafe-Werte", ESp->peer_ski, ESp->peer_ip, 0, wert, (long)eebus_fs_dur_s, ESp->lpc_result);
    return;
  }
  EebusLbVorgabeWert(wert, sizeof(wert));
  char k[80];
  snprintf(k, sizeof(k), "%u|%s|%s", ESp->lpc_dir, wert, ESp->lpc_result);
  const bool fehl = (LPC_FAIL == ESp->lpc_state);   // v245: jede Ablehnung/jeden Fehlschlag eintragen (Nachweiskette)
  if (!fehl && (0 == strcmp(k, ESp->lb_last))) { return; }
  strlcpy(ESp->lb_last, k, sizeof(ESp->lb_last));
  char erg[80];
  if (fehl && ESp->wdh_n) { snprintf(erg, sizeof(erg), "%s (%u. Wiederholung)", ESp->lpc_result, ESp->wdh_n); }   // v245
  else { strlcpy(erg, ESp->lpc_result, sizeof(erg)); }
  EebusLbLog("Vorgabe", ESp->peer_ski, ESp->peer_ip, ESp->lpc_dir, wert, ESp->lpc_dur_s, erg);
}
// Steuerbefehl (Paragraf 14a / Paragraf 9) - nur diese werden als abgewiesen protokolliert
bool EebusLbIstSteuerbefehl(void) {
  char c[24];
  strlcpy(c, XdrvMailbox.command ? XdrvMailbox.command : "", sizeof(c));
  for (char *p = c; *p; p++) { *p = toupper((unsigned char)*p); }
  return strstr(c, "LPC") || strstr(c, "LPP") || strstr(c, "RELEASE");
}
void EebusLbAbgewiesen(const char *ski, const char *ip, const char *grund) {
  if (!EebusLbIstSteuerbefehl()) { return; }
  char wert[40];
  snprintf(wert, sizeof(wert), "%s %s", XdrvMailbox.command ? XdrvMailbox.command : "", XdrvMailbox.data ? XdrvMailbox.data : "");
  EebusLbLog("Befehl abgewiesen", ski, ip, -1, wert, 0, grund);
}
// Sekundentakt: Verbindungszustand je Platz; jede Aenderung wird protokolliert (Stoerung Beginn/Ende)
void EebusLbVerbindungen(void) {
  static bool gemeldet = false;   // v217: einmal je Start melden, wenn das Logbuch mangels SD-Karte aus ist
  if (!gemeldet && (TasmotaGlobal.uptime > 15)) {   // erst nach dem Einbinden der SD (Dateisystem-Treiber startet nach diesem)
    gemeldet = true;
    if (!EebusLbAktiv()) { AddLog(LOG_LEVEL_INFO, PSTR("EBG: Betriebslogbuch aus - keine SD-Karte")); }
  }
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *c = &EConn[i];
    const bool jetzt = c->active && (SME_DONE == c->sme);
    if (jetzt == c->lb_conn) { continue; }
    c->lb_conn = jetzt;
    if (40 != strlen(c->peer_ski)) { continue; }   // nur Anlagen mit echter SKI
    EebusLbLog(jetzt ? "Verbindung hergestellt" : "Kommunikationsstoerung - Verbindung getrennt",
               c->peer_ski, c->peer_ip, -1, "", 0, jetzt ? "" : c->err);
    if (!jetzt) { c->lb_last[0] = '\0'; }   // nach der Stoerung Vorgaben wieder vollstaendig protokollieren
  }
}

void EebusLpcSetResult(uint8_t state, const char *txt) {
  ESp->lpc_state = state;
  strlcpy(ESp->lpc_result, txt, sizeof(ESp->lpc_result));
  if ((LPC_DONE == state) && (0 == strncmp(txt, "BESTAETIGT", 10))) {   // v237: Bestaetigung -> Wiederholung von vorn
    ESp->wdh_n = 0;
    if (0 == strncmp(txt, "BESTAETIGT aktiv", 16)) { ESp->best_akt[(1 == ESp->lpc_dir) ? 1 : 0] = true; }
  }
  if ((LPC_DONE == state) || (LPC_FAIL == state)) { EebusLbVorgabe(); }   // v215: Betriebslogbuch
  if (LPC_FAIL == state) {
    AddLog(LOG_LEVEL_ERROR, PSTR("EBG: LPC @ %s FEHLER: %s"), ESp->peer_ip, txt);
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC @ %s: %s"), ESp->peer_ip, txt);
  }
}

// KOMPLETTES discovery-getriebenes Onboarding — abonniert ALLE Server-Features des Peers
// (DeviceDiagnosis/DeviceConfiguration/ElectricalConnection/LoadControl/Measurement) ueber ALLE
// Entities, wie die Vergleichs-Steuerbox; LoadControl + DeviceConfiguration zusaetzlich binden.
// Generisch (Adressen aus der Peer-Discovery, nicht hart verdrahtet) -> funktioniert mit jedem HEMS.
// Unsere Client-Features: LoadControl=7, DeviceConfiguration=3, DeviceDiagnosis=1, ElectricalConnection=4,
// Measurement=6 (so in EebusSpineAnswerDiscovery deklariert). Rueckgabe: Anzahl abonnierter Features.
int EebusLpcSubscribeAll(const char *disco) {
  if (nullptr == disco) { return 0; }
  char own[80]; EebusOwnDevice(own, sizeof(own));
  const size_t cap = 512;
  char *cmd = (char*)special_malloc(cap);
  if (nullptr == cmd) { return 0; }
  int n = 0;
  const char *scan = disco, *fa;
  while (nullptr != (fa = strstr(scan, "\"featureAddress\""))) {
    scan = fa + 16;
    const char *nextfa = strstr(scan, "\"featureAddress\"");
    const char *ft = strstr(fa, "\"featureType\":\"");
    if ((nullptr == ft) || (nextfa && (ft > nextfa))) { continue; }
    char ftype[24] = { 0 }; EebusJsonStr(ft, "featureType", ftype, sizeof(ftype));
    const char *rl = strstr(ft, "\"role\":\"");
    if ((nullptr == rl) || (nextfa && (rl > nextfa))) { continue; }
    char role[16] = { 0 }; EebusJsonStr(rl, "role", role, sizeof(role));
    if (0 != strcmp(role, "server")) { continue; }   // nur Server-Features abonnieren
   // Unter-Entities (mehrstufig, z.B. [6,1]=BatterySystem, [9,1]=PVSystem) ueberspringen.
   // Als vorgelagerte §14a-Steuerbox schreiben wir NUR an den Aggregat-Punkt des HEMS (ent7); die internen
   // Geraete auf den .1-Unter-Entities verteilt der Energiemanager selbst. Unser bisheriges Kriechen dorthin adressierte
   // die Eltern-Entity [6]/[9] (das ",1" ging im Skalar-Parser EebusAddrParse verloren) -> 5x errorNumber:1/4
   // (Phantom-Abos auf nicht-existente Adressen). Frame-Diff Vergleichs-Steuerbox-vs-EBG : die Vergleichs-Steuerbox fasst .1-Entities NIE
   // an. SteuVE-Modus unveraendert (dort steuern wir ein Einzelgeraet direkt, ggf. inkl. Unter-Entity).
    if (eebus_hems_mode) {
      const char *eb = strstr(fa, "\"entity\":[");
      if (eb) {
        const char *ebo = eb + 10;   // hinter '['  ("\"entity\":[" = 10 Zeichen)
        const char *ebc = strchr(ebo, ']');
        if (ebc && (nullptr != memchr(ebo, ',', (size_t)(ebc - ebo)))) { continue; }   // [x,y] -> ueberspringen
      }
    }
    int cli = -1; bool bind = false;
    if      (0 == strcmp(ftype, "LoadControl"))          { cli = 2; bind = true; }   // feat2 = 1:1 Vergleichs-Steuerbox
    else if (0 == strcmp(ftype, "DeviceConfiguration"))  { cli = 3; bind = true; }
    else if (0 == strcmp(ftype, "DeviceDiagnosis"))      { cli = 1; }
    else if (0 == strcmp(ftype, "ElectricalConnection")) { cli = 4; }
    else if (0 == strcmp(ftype, "Measurement"))          { cli = 6; }
    else { continue; }
    int ent = -1, feat = -1; EebusAddrParse(fa, &ent, &feat);
    if ((ent < 0) || (feat < 0)) { continue; }
    snprintf(cmd, cap,
      "{\"nodeManagementSubscriptionRequestCall\":[{\"subscriptionRequest\":["
      "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
      "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
      "{\"serverFeatureType\":\"%s\"}]}]}",
      own, EEBUS_LPC_CLIENT_ENT, cli, ESp->peer_dev, ent, feat, ftype);
    EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
    EebusAboMerken((uint8_t)cli, ent, feat);   // v241
   // (B) PRAEZISE binden: nur auf der §14a-Ziel-Entity (lpc_peer_ent) binden, nicht blind ueber alle
   // Entities. Der Energiemanager listet LoadControl/DeviceConfiguration auf mehreren Entities (ent1/3/7/8/9); ein Bind
   // auf die nicht-relevanten quittiert er mit errorNumber 1/4 ("Adding binding failed"/"DestinationUnknown")
   // = unnoetiger Konsolen-Fehler. Fuer den Write brauchen wir nur die Ziel-Entity (LoadControl+DeviceConfig
   // dort). So verhaelt sich auch die Vergleichs-Steuerbox: sie bindet gezielt, nicht auf jeder Entity.
    if (bind && (ent == ESp->lpc_peer_ent)) {
      snprintf(cmd, cap,
        "{\"nodeManagementBindingRequestCall\":[{\"bindingRequest\":["
        "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
        "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
        "{\"serverFeatureType\":\"%s\"}]}]}",
        own, EEBUS_LPC_CLIENT_ENT, cli, ESp->peer_dev, ent, feat, ftype);
      EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
      if (0 == strcmp(ftype, "LoadControl")) { ESp->bind_mc = ESp->spine_ctr - 1; }   // v240
    }
   // fuer ElectricalConnection/Measurement zusaetzlich die Beschreibungen lesen (fire-and-forget,
   // wie die Vergleichs-Steuerbox — nicht write-gatend). LoadControl/DeviceConfiguration werden gated gelesen
   // (readdesc/readdata/readcfg). Adresse: unser Client ent1/feat<cli> -> Peer-Server ent/feat.
    if (0 == strcmp(ftype, "ElectricalConnection")) {
   // jede gefundene Adresse aufnehmen, nicht die vorige ueberschreiben.
   // dazu merken, OB diese Instanz die Kenngroessen ueberhaupt anbietet. Ohne das fragte
   // EEBusMess auch dort nach, wo nichts zu holen ist — die Gegenstelle antwortete dann mit
   // errorNumber 6 (CommandNotSupported), was wie ein Fehler aussah und das Ergebnis verwischte.
      if (ESp->ec_n < EEBUS_ADR_MAX) {
        const char *ch0 = strstr(fa, "electricalConnectionCharacteristicListData");
        ESp->ec_ent[ESp->ec_n] = ent; ESp->ec_feat[ESp->ec_n] = feat;
        ESp->ec_cli[ESp->ec_n] = cli;
        ESp->ec_char[ESp->ec_n] = (nullptr != ch0) && ((nullptr == nextfa) || (ch0 < nextfa));
        ESp->ec_n++;
      }
      EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionDescriptionListData\":[]}",          EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
   // die Anschluss-Kenngroessen mitlesen — dort steht die Nennleistung. Wie bei der
   // ParameterDescription nur dann, wenn die Selbstauskunft die Funktion fuer dieses Feature
   // auffuehrt; sonst antwortet die Gegenstelle mit errorNumber 6 (CommandNotSupported).
      {
        const char *ch = strstr(fa, "electricalConnectionCharacteristicListData");
        if ((nullptr != ch) && ((nullptr == nextfa) || (ch < nextfa))) {
          EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionCharacteristicListData\":[]}",
                             EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
        }
      }
   // ParameterDescription NUR lesen, wenn die Discovery sie fuer dieses Feature auffuehrt.
   // ent7-EC unterstuetzt nur Characteristic+Description, NICHT ParameterDescription -> sonst errorNumber:6
   // (CommandNotSupported). Die Vergleichs-Steuerbox liest ParameterDescription ausschliesslich an ent8. Pruefung: kommt der
   // Funktionsname noch VOR dem naechsten featureAddress (also im supportedFunction dieses Features) vor?
   // SteuVE-Modus unveraendert (liest weiter beide).
      bool read_pd = true;
      if (eebus_hems_mode) {
        const char *pd = strstr(fa, "electricalConnectionParameterDescriptionListData");
        read_pd = (nullptr != pd) && ((nullptr == nextfa) || (pd < nextfa));
      }
      if (read_pd) {
        EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionParameterDescriptionListData\":[]}", EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
      }
    } else if (0 == strcmp(ftype, "Measurement")) {
      if (ESp->m_n < EEBUS_ADR_MAX) {   // alle Measurement-Instanzen merken
        ESp->m_ent[ESp->m_n] = ent; ESp->m_feat[ESp->m_n] = feat;
        ESp->m_cli[ESp->m_n] = cli; ESp->m_n++;
      }
      EebusSpineSendAddr("read", false, 0, "{\"measurementDescriptionListData\":[]}",  EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
      EebusSpineSendAddr("read", false, 0, "{\"measurementConstraintsListData\":[]}",  EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
      if (eebus_hems_mode) {
   // auch die Messwerte lesen (Vergleichs-Steuerbox Schritt 34: measurementListData ent8/f11).
        EebusSpineSendAddr("read", false, 0, "{\"measurementListData\":[]}", EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
      }
    } else if ((0 == strcmp(ftype, "DeviceConfiguration")) && eebus_hems_mode) {
   // DeviceConfig-Beschreibung + Werte von JEDER DeviceConfig-Entity lesen —
   // die Vergleichs-Steuerbox liest sie auf ent7 UND ent8 (Mitschnitt: deviceConfigurationKeyValueListData je 1x
   // ent7 und ent8). Fire-and-forget; die gated ent7-Antwort (peer_ent-Guard) treibt den Write, die
   // ent8-Antwort wird ignoriert. In Modus 0 unveraendert (nur die gated ent7-Kette via EebusLpcReadCfg).
   // (Voll-Frame-Diff, letzte verbliebene Abweichung): die ZIEL-Entity hier AUSLASSEN.
   // EebusLpcReadCfg liest auf lpc_peer_ent ohnehin Beschreibung + Werte (gated, treibt die
   // Zustandsmaschine und lernt die Failsafe-Keys) -> bisher ging beides an ent7 DOPPELT raus,
   // die Referenz-Steuerbox liest je Entity GENAU EINMAL. Fuer alle anderen Entities (z.B. ent8)
   // bleibt der Read hier, damit die Abdeckung wie bei der Referenz vollstaendig ist.
      if (ent != ESp->lpc_peer_ent) {
        EebusSpineSendAddr("read", false, 0, "{\"deviceConfigurationKeyValueDescriptionListData\":[]}", EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
        EebusSpineSendAddr("read", false, 0, "{\"deviceConfigurationKeyValueListData\":[]}",            EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
      }
    } else if (0 == strcmp(ftype, "DeviceDiagnosis")) {
   // (A) Energiemanager-HEARTBEAT MITLESEN — die entscheidende Verhaltens-Luecke ggue. der Vergleichs-Steuerbox:
   // der Energy-Guard muss den Herzschlag des gesteuerten Systems (CS) ueberwachen (§14a/LPC-Failsafe).
   // Die Vergleichs-Steuerbox liest deviceDiagnosisHeartbeatData von JEDER CS-DeviceDiagnosis-Entity (ent1/2/3/4/7);
   // wir taten das NIE. Ohne diese beidseitige Herzschlag-Ueberwachung nimmt ein strenger HEMS ein
   // §14a-Limit evtl. nicht an. Passiert in der BIND-Phase (vor dem Write) -> Ueberwachung steht, BEVOR
   // geschrieben wird (erklaert "wir schreiben frueh, Vergleichs-Steuerbox spaet" — nicht die Uhr, die Beziehung).
      EebusSpineSendAddr("read", false, 0, "{\"deviceDiagnosisHeartbeatData\":[]}", EEBUS_LPC_CLIENT_ENT, cli, ent, feat);
    }
    n++;
  }
  free(cmd);
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC @ %s: discovery-getrieben %d Server-Features abonniert%s"), ESp->peer_ip, n,
         eebus_hems_mode ? " (HEMS: gezielt, ohne Unter-Entities/ParamDesc)" : "");
  return n;
}

// Binding-Anfrage an die LoadControl des Peers (call, NodeManagement<->NodeManagement).
void EebusLpcSendBind(void) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
   // das proaktive UCData-NOTIFY ENTFERNT. Die akzeptierte Vergleichs-Steuerbox
   // notifyt ihre useCaseData NIE — sie antwortet nur, wenn der Energiemanager sie liest (und dann vollstaendig). Unser
   // Notify schickte zudem ein REDUZIERTES Profil (nur LPC, ohne LPP/MonitoringAppliance); ein
   // spec-konformer Energiemanager behandelt einen UCData-Notify als Voll-REPLACE und ueberschrieb damit kurz vor dem Bind
   // unser vollstaendiges Reply-Profil. frueher war eine Fehlannahme (: "Energiemanager liest unsere UCData nicht") —
   // inzwischen bewiesen: der Energiemanager LIEST unsere UCData (wir antworten voll) und quittiert alle Bindings mit err0.
  const size_t cap = 512;
  char *cmd = (char*)special_malloc(cap);
  if (nullptr == cmd) { EebusLpcSetResult(LPC_FAIL, "malloc"); return; }
   // als ERSTES das Peer-NodeManagement (ent0/f0) abonnieren — genau das tut die
   // Vergleichs-Steuerbox zuerst (Mitschnitt Z.531: client ent0/f0 -> server ent0/f0, serverFeatureType NodeManagement).
   // Ein strenger HEMS erwartet, dass eine echte Steuerbox seine NodeManagement-Aenderungen abonniert, bevor
   // er den §14a-Write freigibt. Nur im HEMS-Modus; SteuVE-Pfad unveraendert.
   // NUR abonnieren, wenn es der Pfad (nach gegenseitiger Discovery,
   // EebusSpineReadPeerDiscovery) noch NICHT getan hat. Vorher feuerten beide Stellen -> wir schickten das
   // NodeManagement-Abo ZWEIMAL, die Referenz-Steuerbox schickt es GENAU EINMAL (Mengenvergleich der
   // Abo-Inhalte: 11 von 12 Abos + beide Bindings deckungsgleich, einzige Abweichung dieses Duplikat).
   // "Wir senden mehr als die Referenz" war schon mehrfach die Sorte Abweichung, die zu beseitigen war.
  if (eebus_hems_mode && !ESp->we_nm_subscribed) {
    ESp->we_nm_subscribed = true;
    snprintf(cmd, cap,
      "{\"nodeManagementSubscriptionRequestCall\":[{\"subscriptionRequest\":["
        "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[0]},{\"feature\":0}]},"
        "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[0]},{\"feature\":0}]},"
        "{\"serverFeatureType\":\"NodeManagement\"}"
      "]}]}", own, ESp->peer_dev);
    EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
  }
   // KOMPLETTES discovery-getriebenes Onboarding — abonniert ALLE Server-Features des Peers
   // (DeviceDiagnosis/DeviceConfiguration/ElectricalConnection/LoadControl/Measurement) ueber ALLE
   // Entities + bindet LoadControl+DeviceConfiguration, exakt wie die Vergleichs-Steuerbox. Ersetzt den
   // frueheren hart verdrahteten Ein-Entity-Block ( / ). Fallback: keine gespeicherte Discovery ->
   // LoadControl an der gelernten/Default-Adresse (Minimalpfad, z.B. wenn Discovery ausblieb).
  int nsub = EebusLpcSubscribeAll(ESp->lpc_disco);
  if (0 == nsub) {
    snprintf(cmd, cap,
      "{\"nodeManagementSubscriptionRequestCall\":[{\"subscriptionRequest\":["
        "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
        "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
        "{\"serverFeatureType\":\"LoadControl\"}"
      "]}]}", own, EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT,
      ESp->peer_dev, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
    EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
    snprintf(cmd, cap,
      "{\"nodeManagementBindingRequestCall\":[{\"bindingRequest\":["
        "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
        "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
        "{\"serverFeatureType\":\"LoadControl\"}"
      "]}]}", own, EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT,
      ESp->peer_dev, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
    EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
    ESp->bind_mc = ESp->spine_ctr - 1;   // v240
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC @ %s: Fallback LoadControl abo+bind (keine Discovery)"), ESp->peer_ip);
  }
  free(cmd);
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
  EebusLpcSetResult(LPC_BIND, "Binding gesendet");
}

// LoadControl-Limit-Beschreibung des Peers lesen (limitId der consume/obligation-Grenze finden).
void EebusLpcReadDesc(void) {
  EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitDescriptionListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
  EebusLpcSetResult(LPC_READDESC, "Limit-Beschreibung angefragt");
}

// Ist-LimitData des Peers lesen, BEVOR wir schreiben — wie Referenz-Umsetzung (WriteLoadControlLimit
// liest GetLimitDataForFilter + prueft isLimitChangeable). Der strenge Energiemanager lieferte uns nie
// loadControlLimitData, weil wir sie nie angefordert haben; er erwartet die volle Sequenz
// (Bind -> Description -> LimitData -> Write). Antwort landet im LPC_READDATA-Handler.
// loadControlLimitListData MIT Selektor auf genau eine limitId lesen — byte-treu
// zur Vergleichs-Steuerbox (Mitschnitt Z.700/701): function + filter{cmdControl:partial + loadControlLimitListData-
// Selectors:limitId} + leeres loadControlLimitListData. EebusSpineSendAddr wickelt die 3 Geschwister in
// {"cmd":[[ ... ]]} — exakt die akzeptierte Vergleichs-Steuerbox-Read-Form.
void EebusLpcReadLimitSel(int limit_sel) {
  char sel[224];
  snprintf(sel, sizeof(sel),
    "{\"function\":\"loadControlLimitListData\"},"
    "{\"filter\":[[{\"cmdControl\":[{\"partial\":[]}]},{\"loadControlLimitListDataSelectors\":[{\"limitId\":%d}]}]]},"
    "{\"loadControlLimitListData\":[]}", limit_sel);
  EebusSpineSendAddr("read", false, 0, sel, EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT,
                     ESp->lpc_peer_ent, ESp->lpc_peer_feat);
}

void EebusLpcReadData(void) {
  if (eebus_hems_mode) {
   // HEMS: erst limitId 0 lesen; die Antwort loest im Reply-Handler den limitId-1-Read aus (Bauplan 30/31).
    ESp->lpc_sel_step = 0;
    EebusLpcReadLimitSel(0);
    EebusLpcSetResult(LPC_READDATA, "Ist-LimitData limitId0 angefragt");
  } else {
    EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
    EebusLpcSetResult(LPC_READDATA, "Ist-LimitData angefragt");
  }
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
}

// keyId eines DeviceConfig-Keys (z.B. failsafeConsumptionActivePowerLimit) aus der
// deviceConfigurationKeyValueDescription des Peers lernen. Je Eintrag steht die keyId VOR keyName
// ([{"keyId":N},{"keyName":"..."},...]). Wir nehmen die keyId, deren zugehoeriger keyName passt.
// -1 = nicht gefunden.
int EebusLpcFindCfgKey(const char *desc, const char *keyname) {
  const char *scan = desc, *k;
  while (nullptr != (k = strstr(scan, "\"keyId\":"))) {
    int kid = atoi(k + 8);
    const char *nextk = strstr(k + 8, "\"keyId\":");
    const char *kn = strstr(k, "\"keyName\":\"");
    scan = k + 8;
    if ((nullptr == kn) || (nextk && (kn > nextk))) { continue; }   // keyName gehoert zur naechsten keyId
    char nm[48] = { 0 }; EebusJsonStr(kn, "keyName", nm, sizeof(nm));
    if (0 == strcmp(nm, keyname)) { return kid; }
  }
  return -1;
}

// die beiden §14a-Failsafe-Keys aus der DeviceConfig-Beschreibung des Peers lernen (Schreibziele
// fuer den Failsafe-Write vor dem Limit). Setzt lpc_fs_val_key/lpc_fs_dur_key (-1 = fehlt -> Gate skippt).
void EebusLpcLearnFailsafeKeys(const char *desc) {
  int vk = EebusLpcFindCfgKey(desc, "failsafeConsumptionActivePowerLimit");
  int dk = EebusLpcFindCfgKey(desc, "failsafeDurationMinimum");
  int pk = EebusLpcFindCfgKey(desc, "failsafeProductionActivePowerLimit");   // v196
  if (vk >= 0) { ESp->lpc_fs_val_key = vk; }
  if (dk >= 0) { ESp->lpc_fs_dur_key = dk; }
  if (pk >= 0) { ESp->lpc_fs_prod_key = pk; }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Keys gelernt: Bezug=keyId%d, Einspeisung=keyId%d, Dauer=keyId%d @ %s"),
         ESp->lpc_fs_val_key, ESp->lpc_fs_prod_key, ESp->lpc_fs_dur_key, ESp->peer_ip);
}

// DeviceConfiguration (Failsafe) des Peers lesen VOR dem Write — wie die Vergleichs-Steuerbox.
// BEWIESEN(Frame-Diff): der Energiemanager genehmigte den §14a-Write NUR nach voller Referenz-Umsetzung-Sequenz;
// die Vergleichs-Steuerbox liest vor dem Write deviceConfigurationKeyValueDescriptionListData + KeyValueListData
// (Keys: failsafeConsumptionActivePowerLimit/failsafeDurationMinimum). Wir sprangen direkt zum Write ->
// deny (errorNumber:7). Adresse: unser DeviceConfiguration-Client ent1/feat3 -> Peer-LC-Entity/feat24
// (identisch zum Bind). Best-effort: bei Timeout wird trotzdem geschrieben (Waermepumpen-Gateway-Pfad-Schutz).
void EebusLpcReadCfg(void) {
   // DeviceConfig-Adresse AUS DISCOVERY (lpc_peer_dcfg_feat) statt Hardcode feat24 — herstellerneutral.
   // Hat der Peer auf der Ziel-Entity keine DeviceConfiguration (-1) -> Schritt UEBERSPRINGEN und direkt schreiben
   // (der DeviceConfig-Read ist optionale Vergleichs-Steuerbox-Vorsequenz; der Write haengt nicht davon ab, live bewiesen am Referenz-CS).
  if (ESp->lpc_peer_dcfg_feat < 0) {
    EebusLpcSetResult(LPC_READCFG, "keine DeviceConfig am Ziel -> direkt schreiben");
    EebusLpcWrite();
    return;
  }
  EebusSpineSendAddr("read", false, 0, "{\"deviceConfigurationKeyValueDescriptionListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, 3, ESp->lpc_peer_ent, ESp->lpc_peer_dcfg_feat);
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
  EebusLpcSetResult(LPC_READCFG, "DeviceConfig-Beschreibung angefragt");
}

// ElectricalConnection des Peers lesen VOR dem Write — wie die Vergleichs-Steuerbox (sie liest
// electricalConnectionDescriptionListData + electricalConnectionParameterDescriptionListData). Damit
// deckt unser Onboarding auch diese Feature-Art ab. Adresse: unser ElectricalConnection-Client
// ent1/feat4 -> Peer-LC-Entity/feat7 (Energiemanager: ent7/feat7). Best-effort: Timeout -> trotzdem schreiben.
void EebusLpcReadEl(void) {
  EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionDescriptionListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, 4, ESp->lpc_peer_ent, 7);
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
  EebusLpcSetResult(LPC_READEL, "ElectricalConnection-Beschreibung angefragt");
}

// Das eigentliche Limit schreiben (write). active_wish=true -> setzen, false -> freigeben.
// timePeriod-Dauer des §14a-Write testbar (per EEBusLpc <idx> <watt> [dauer_s]).
// Default 3600 s = PT1H = die DIMM-Dauer, die der akzeptierte Vergleichs-Steuerbox-Write hat
// . 7200 s ist die FAILSAFE-Dauer (DeviceConfiguration, separat) — wir hatten sie
// faelschlich als Limit-timePeriod (PT2H) geschrieben -> Energiemanager errorNumber:7. >0 = PT<n>S. 0 = timePeriod weglassen.
int32_t eebus_limit_dur_s = 3600;

// einen Failsafe-DeviceConfig-Wert an das DeviceConfiguration-SERVER-Feature des Peers SCHREIBEN
// (partial, gezieltes Teil-Update eines Keys), wie die Vergleichs-Steuerbox vor dem §14a-Limit-Write.
// key = gelernte keyId; valjson = der value-Block (scaledNumber fuer W bzw. duration fuer die Dauer).
// Adresse: unser DeviceConfiguration-Client ent1/feat3 -> Peer-LC-Entity / gelerntes DeviceConfig-feat.
void EebusLpcSendFailsafe(int key, const char *valjson) {
  char cmd[320];
  snprintf(cmd, sizeof(cmd),
    "{\"function\":\"deviceConfigurationKeyValueListData\"},"
    "{\"filter\":[[{\"cmdControl\":[{\"partial\":[]}]}]]},"
    "{\"deviceConfigurationKeyValueListData\":[{\"deviceConfigurationKeyValueData\":[["
      "{\"keyId\":%d},{\"value\":[%s]}"
    "]]}]}", key, valjson);
  EebusSpineSendAddr("write", false, 0, cmd,
                     EEBUS_LPC_CLIENT_ENT, 3, ESp->lpc_peer_ent, ESp->lpc_peer_dcfg_feat);
}

/*********************************************************************************************\
 * v196 (24.10., meierchen006): FAILSAFE-WRITE der Steuerbox
 *
 * VDE FNN Empfehlung zu Tenorziffer 2a BK6-22-300 (Okt. 2024), Abschnitt "FailSafe-Verhalten":
 * "Dabei uebertraegt die Steuerungseinrichtung bei bestehender Kommunikationsverbindung die Grenzwerte
 * fuer die Anwendungsfaelle Wirkleistungslimitierung Bezug und Wirkleistungslimitierung Einspeisung
 * und die maximale Zeitdauer dieser Grenzwerte zur Anlage." (LPC [LPC-021/2], LPP [LPP-021/2]).
 *   - Bezug = Pmin nach BK6-22-300 Anl. 1 Ziff. 4.5.2, gerechnet aus der Geraeteliste (EEBusFailsafe),
 *     dieselbe Formel wie der Rechner auf der Steuerbox-Seite. Kein fester Wert. Hoechstens die
 *     Anschlussleistung aus dem SLS-Nennstrom (A x 3 x 230 V), wenn dieser eingetragen ist.
 *   - Einspeisung = Prozent (Vorgabe 60 %, § 9 Abs. 2 Satz 1 Nr. 3 EEG) x Nennleistung, die die ANLAGE
 *     meldet: contractualProductionNominalMax [LPP-042] (Energiemanager), sonst powerProductionNominalMax
 *     [LPP-041]. LPP Z.256-258: der Energy Guard bekommt den Prozentwert von aussen, die Bezugsgroesse
 *     meldet die Anlage. Meldet sie keine, wird kein Einspeise-Failsafe geschrieben (kein Ersatzwert).
 *     Der Failsafe-Wert der Anlage ist KEINE Bezugsgroesse (nach unserem Write stuende dort unser Wert).
 *     Der Wert ist nach [LPP-010] immer >= 0.
 *   - Dauer = Failsafe Duration Minimum (2 bis 24 h, [LPC-022]).
 * Geschrieben wird je Verbindung einmal nach dem Onboarding und erneut, wenn sich die Werte aendern;
 * nacheinander Bezug -> Einspeisung -> Dauer, jeweils nur, wenn die Gegenstelle den Key beschreibt.
 * Eine Ablehnung wird protokolliert und nicht wiederholt (keine Schreibflut).
 * Frueher (v111, 24.07.) lehnte der HM unseren Failsafe-Write ab — damals lehnte er aber JEDEN Write
 * ab (err7 bis zum Zeitzonen-Fix 26.07.), das war kein Beleg gegen den Failsafe-Write.
\*********************************************************************************************/
// Geraeteliste in 0,1 kW: [0..3] Ladepunkte, [4..7] Waermepumpen, [8..11] Raumkuehlung, [12..15] Speicher
uint16_t eebus_fs_dev[16]  = { 0 };
uint8_t  eebus_fs_pct      = 60;      // Einspeise-Failsafe in % der Nennleistung
uint32_t eebus_fs_dur_s    = 7200;    // Failsafe Duration Minimum in s
uint16_t eebus_fs_sls_a    = 0;       // SLS-Nennstrom am Netzanschluss in A, 0 = nicht eingetragen
bool     eebus_fs_loaded   = false;

float EebusFsAnteil(float x) { return (x > 11.0f) ? x * 0.4f : ((x > 4.2f) ? 4.2f : 0.0f); }

// Pmin nach BK6-22-300 Anl. 1 Ziff. 4.5.2 — gleich wie die Seite (steuerbox.html pmin()):
// WP und Klima zaehlen je als EINE Einrichtung (Gruppe > 4,2 kW), Ladepunkte und Speicher einzeln
// (> 4,2 kW). Sockel 4,2 kW, oder 40 % der groesseren Gruppe WP/Klima, wenn eine ueber 11 kW liegt.
// Rueckgabe W, 0 = keine steuerbare Einrichtung eingetragen.
uint32_t EebusFsPminW(uint8_t *n_out, float *g_out) {
  float swp = 0, skl = 0;
  int n = 0;
  for (int i = 0; i < 4; i++) {
    swp += eebus_fs_dev[4 + i] / 10.0f;
    skl += eebus_fs_dev[8 + i] / 10.0f;
    if (eebus_fs_dev[i] > 42)      { n++; }   // Ladepunkt > 4,2 kW
    if (eebus_fs_dev[12 + i] > 42) { n++; }   // Speicher > 4,2 kW
  }
  if (swp > 4.2f) { n++; }
  if (skl > 4.2f) { n++; }
  if (n_out) { *n_out = (uint8_t)n; }
  if (0 == n) { if (g_out) { *g_out = 0; } return 0; }
  const float kGzf[] = { 1.0f, 1.0f, 0.8f, 0.75f, 0.7f, 0.65f, 0.6f, 0.55f, 0.5f };
  float g = (n > 8) ? 0.45f : kGzf[n];
  if (g_out) { *g_out = g; }
  float sockel = ((swp > 11.0f) || (skl > 11.0f)) ? fmaxf(EebusFsAnteil(swp), EebusFsAnteil(skl)) : 4.2f;
  float kw = sockel + (n - 1) * g * 4.2f;
  return (uint32_t)lroundf(kw * 1000.0f);
}

// Anschlussleistung aus dem SLS-Nennstrom (drei Phasen, 230 V), 0 = nicht eingetragen
uint32_t EebusFsAnschlussW(void) { return (uint32_t)eebus_fs_sls_a * 3 * 230; }

// Failsafe Bezug = Pmin, hoechstens die Anschlussleistung
uint32_t EebusFsBezugW(void) {
  uint32_t w = EebusFsPminW(nullptr, nullptr);
  uint32_t a = EebusFsAnschlussW();
  return (a && (w > a)) ? a : w;
}

// scaledNumber -> W
uint32_t EebusFsScaledW(long n, long sc) {
  if ((sc > 12) || (sc < -12)) { return 0; }   // v231: scale aus dem Frame begrenzen (Schleife ohne Ende)
  double w = (double)n;
  for (; sc > 0; sc--) { w *= 10.0; }
  for (; sc < 0; sc++) { w /= 10.0; }
  if (w >= 4294967295.0) { return 0xFFFFFFFFUL; }   // v231: keine undefinierte Umwandlung
  return (w > 0) ? (uint32_t)lround(w) : 0;
}

// v197: Failsafe Bezug fuer DIESE Anlage. Deckel = vertragliche Hoechstleistung, die die Anlage meldet
// (LPC [LPC-042], Z.654-662: "the Controllable System offers"); nur wenn sie keine meldet, der eigene SLS-Eintrag.
#define EEBUS_MIN_DIREKT_W 4200   // v209: BK6-22-300 Anl. 1 Ziff. 4.5.1 - Mindestleistung je direkt gesteuerter SteuVE
// v211: Mindestleistung = Dimmwert eines DIREKT gesteuerten Geraets (BK6 Ziff. 4.5.1; FNN-Empfehlung: "Dimmen
// (zugesicherte Mindestbezugsleistung § 14a)"). Satz 2: Waermepumpe (2.4.1.b) bzw. Raumkuehlung (2.4.1.c) mit mehr als
// 11 kW Netzanschlussleistung -> 0,4 x Leistung. Erkannt an der gemeldeten Hoechstleistung und der Discovery
// (HeatPump / Compressor / AirConditioner / HVAC); ohne diese Angaben gilt 4200 W.
uint32_t EebusDirektMinW(uint32_t ci) {
  const EebusConn *c = &EConn[ci];
  uint32_t p = c->pnom_ccons_ok ? EebusFsScaledW(c->pnom_ccons, c->pnom_ccons_sc) : 0;
  if (p > 11000) {
    const char *d = c->lpc_disco;
    if (d && (strstr(d, "HeatPump") || strstr(d, "Compressor") || strstr(d, "AirConditioner") || strstr(d, "HVAC"))) {
      uint32_t w = (uint32_t)(((uint64_t)p * 4 + 5) / 10);
      return (w > EEBUS_MIN_DIREKT_W) ? w : EEBUS_MIN_DIREKT_W;
    }
  }
  return EEBUS_MIN_DIREKT_W;
}
uint32_t EebusFsBezugCi(uint32_t ci) {
  const EebusConn *c = &EConn[ci];
   // v209 (meierchen006 26.10.): direkt gesteuert (Ziff. 4.4.a) -> Mindestleistung je SteuVE (Ziff. 4.5.1, v211 inkl. Satz 2);
   // hinter dem HEMS / EMS (Ziff. 4.4.b) -> Pmin der Anlage (Ziff. 4.5.2), wie bisher
  if (c->peer_ski[0] && (0 == EebusModeEffective(c->peer_ski))) { return EebusDirektMinW(ci); }
  uint32_t w = EebusFsPminW(nullptr, nullptr);
  uint32_t a = c->pnom_ccons_ok ? EebusFsScaledW(c->pnom_ccons, c->pnom_ccons_sc) : EebusFsAnschlussW();
  return (a && (w > a)) ? a : w;
}

// Nennleistung Erzeugung, die die Anlage dieser Verbindung meldet (W), 0 = keine gemeldet
// Parameter = Slot-Nummer, nicht EebusConn* — der Arduino-Vorbau setzt die Funktionskoepfe vor die Struktur.
uint32_t EebusFsNennW(uint32_t ci) {
  const EebusConn *c = &EConn[ci];
  if (c->pnom_cprod_ok) { return EebusFsScaledW(c->pnom_cprod, c->pnom_cprod_sc); }
  if (c->pnom_prod_ok)  { return EebusFsScaledW(c->pnom_prod, c->pnom_prod_sc); }
  return 0;
}

uint32_t EebusFsProdW(uint32_t ci) { return (uint32_t)(((uint64_t)EebusFsNennW(ci) * eebus_fs_pct + 50) / 100); }

void EebusFsLoad(void) {
  eebus_fs_loaded = true;
#ifdef USE_UFILESYS
  char t[160] = { 0 };
  if (!TfsLoadFile(EEBUS_FS_FILE, (uint8_t*)t, sizeof(t) - 1)) { return; }
  char *p = t, *e = nullptr;
  long v[19];
  int k = 0;
  for (; k < 19; k++) {
    v[k] = strtol(p, &e, 10);
    if (e == p) { break; }
    p = e;
  }
  if (19 != k) { return; }   // unvollstaendig -> Vorgaben behalten
   // v231 (Paket 5): Bereiche wie beim Befehl pruefen, sonst Vorgaben behalten (Datei von Hand veraendert/beschaedigt)
  if ((v[16] < 0) || (v[16] > 100) || (v[17] < 7200) || (v[17] > 86400) || (v[18] < 0) || (v[18] > 1000)) { return; }
  for (int i = 0; i < 16; i++) { if ((v[i] < 0) || (v[i] > 65535)) { return; } }
  for (int i = 0; i < 16; i++) { eebus_fs_dev[i] = (uint16_t)((v[i] < 0) ? 0 : v[i]); }
  eebus_fs_pct    = (uint8_t)v[16];
  eebus_fs_dur_s  = (uint32_t)v[17];
  eebus_fs_sls_a  = (uint16_t)v[18];
#endif
}

void EebusFsSave(void) {
#ifdef USE_UFILESYS
  char t[160];
  int l = 0;
  for (int i = 0; i < 16; i++) { l += snprintf(t + l, sizeof(t) - l, "%u ", eebus_fs_dev[i]); }
  l += snprintf(t + l, sizeof(t) - l, "%u %u %u", eebus_fs_pct, eebus_fs_dur_s, eebus_fs_sls_a);
  TfsSaveFile(EEBUS_FS_FILE, (const uint8_t*)t, l);
#endif
}

// naechsten Failsafe-Wert dieser Verbindung senden (Schritte nach "nach"), sonst abschliessen
void EebusLpcFailsafeNext(uint8_t nach) {
  for (uint8_t st = nach + 1; st <= 3; st++) {
    char v[72];
    int key = -1;
    const uint8_t code = (1 == st) ? 1 : ((2 == st) ? 3 : 2);   // v243 (Tab. 15/16): isValueChangeable false -> nicht schreiben
    if (ESp->fs_ro & (1 << code)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe %s @ %s laut Anlage nicht beschreibbar (isValueChangeable false) - uebersprungen"),
             (1 == code) ? "Bezug" : (3 == code) ? "Einspeisung" : "Dauer", ESp->peer_ip);
      continue;
    }
    if (1 == st) {
      uint32_t w = EebusFsBezugCi((uint32_t)(ESp - EConn));
      if ((ESp->lpc_fs_val_key < 0) || (0 == w)) { continue; }
      key = ESp->lpc_fs_val_key;
      snprintf(v, sizeof(v), "{\"scaledNumber\":[{\"number\":%u},{\"scale\":0}]}", w);
      const bool direkt = (0 == EebusModeEffective(ESp->peer_ski));   // v211: richtiger Text je Betriebsart
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe Bezug %u W (%s) -> keyId %d @ %s"), w,
             direkt ? "Mindestleistung direkt gesteuert, BK6 Ziff. 4.5.1" :
             ((w < EebusFsPminW(nullptr, nullptr)) ? "Pmin, gedeckelt auf Hoechstleistung" : "Pmin, BK6 Ziff. 4.5.2"),
             key, ESp->peer_ip);
    } else if (2 == st) {
      if (ESp->lpc_fs_prod_key < 0) { continue; }
      if (0 == EebusFsNennW((uint32_t)(ESp - EConn))) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: kein Failsafe Einspeisung @ %s - Anlage meldet keine Nennleistung Erzeugung"), ESp->peer_ip);
        continue;
      }
      key = ESp->lpc_fs_prod_key;
      snprintf(v, sizeof(v), "{\"scaledNumber\":[{\"number\":%u},{\"scale\":0}]}", EebusFsProdW((uint32_t)(ESp - EConn)));
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe Einspeisung %u W (%u %% von %u W) -> keyId %d @ %s"),
             EebusFsProdW((uint32_t)(ESp - EConn)), eebus_fs_pct, EebusFsNennW((uint32_t)(ESp - EConn)), key, ESp->peer_ip);
    } else {
      if (ESp->lpc_fs_dur_key < 0) { continue; }
      key = ESp->lpc_fs_dur_key;
      char iso[24];
      EebusSecsToIso((long)eebus_fs_dur_s, iso, sizeof(iso));
      snprintf(v, sizeof(v), "{\"duration\":\"%s\"}", iso);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe Mindestdauer %s -> keyId %d @ %s"), iso, key, ESp->peer_ip);
    }
    ESp->lpc_fs_step = st;
    EebusLpcSendFailsafe(key, v);
    ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
    EebusLpcSetResult(LPC_FAILSAFE, (1 == st) ? "Failsafe Bezug geschrieben"
                                  : (2 == st) ? "Failsafe Einspeisung geschrieben" : "Failsafe-Dauer geschrieben");
    return;
  }
  ESp->lpc_fs_step = 0;
  ESp->lpc_fs_done = true;
  ESp->lpc_deadline = 0;
   // v239 (LPC IG 2.15 Z. 468-479, IG 2.5): wurde ein Failsafe-Wert abgelehnt, die Werte spaeter erneut schreiben
   // (wachsender Abstand wie bei Vorgaben) - bis v238 galten sie nach einem NACK als erledigt.
  if (ESp->fs_nack) {
    ESp->fs_nack = false;
    ESp->lpc_fs_done = false;
    ESp->fs_wdh_at = Eebus126At(EebusWdhMs()); if (!ESp->fs_wdh_at) { ESp->fs_wdh_at = 1; }
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Werte @ %s abgelehnt - erneuter Versuch in %u s"), ESp->peer_ip, EebusWdhMs() / 1000);
    EebusLbLog("Failsafe-Werte abgelehnt", ESp->peer_ski, ESp->peer_ip, 0, "", 0, "erneuter Versuch spaeter");
  }
  EebusLpcSetResult(LPC_DONE, "Failsafe-Werte geschrieben");
   // v197: einmal nachlesen, damit die Anzeige ("Anlage meldet zurueck") den neuen Stand zeigt
  if (ESp->lpc_peer_dcfg_feat >= 0) {
    EebusSpineSendAddr("read", false, 0, "{\"deviceConfigurationKeyValueListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, 3, ESp->lpc_peer_ent, ESp->lpc_peer_dcfg_feat);
  }
}

// Quittung auf einen Failsafe-Write: protokollieren und mit dem naechsten Wert weitermachen.
void EebusLpcFailsafeResult(bool ok) {
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe %s %s @ %s"),
         (1 == ESp->lpc_fs_step) ? "Bezug" : (2 == ESp->lpc_fs_step) ? "Einspeisung" : "Dauer",
         ok ? "quittiert (err0)" : "ABGELEHNT", ESp->peer_ip);
  if (!ok) { ESp->fs_nack = true; }   // v239
  EebusLpcFailsafeNext(ESp->lpc_fs_step);
}

void EebusLpcWrite(void) {
   // Onboarding-Modus — die Kette (Bind/ReadDesc/ReadData/ReadCfg) lief durch, aber JETZT
   // NICHT schreiben. Die Beziehung reift (sobald der Energiemanager unser DeviceDiagnosis abonniert, laeuft der periodische
   // Heartbeat); der eigentliche §14a-Write kommt spaeter via EEBusLpc gegen die gecachten Daten — wie Referenz-Umsetzung
   // (Onboarding beim Connect, WriteConsumptionLimit ist eine separate, viel spaetere Aktion). Dieser EINE Punkt
   // faengt ALLE Ketten-Enden ab (READCFG-Abschluss, "keine DeviceConfig", Read-Timeouts rufen alle EebusLpcWrite).
  if (ESp->lpc_onboard_only) {
    ESp->lpc_onboard_only = false;
    ESp->lpc_onboarded = true;
    ESp->lpc_deadline = 0;
   // Anmeldung vormerken — der Sekunden-Loop schreibt sie, sobald drei Heartbeats durch sind.
    if (eebus_auto_reg && eebus_hems_mode) { ESp->lpc_reg_step = 1; }
    ESp->reg_dir_running = -1; ESp->reg_failed = 0; ESp->reg_retry_cnt = 0; ESp->reg_retry_at = 0;   // v176
    EebusLpcSetResult(LPC_IDLE, "onboarded - Beziehung reift, Write via EEBusLpc/EEBusLpp");
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC-Onboarding fertig @ %s - Heartbeat laeuft%s"), ESp->peer_ip,
           ESp->lpc_reg_step ? ", Anmeldung folgt sobald der Heartbeat abonniert ist"
                             : ", Write folgt spaeter (EEBusLpc/EEBusLpp)");
    return;
  }
   // Failsafe-Werte schreibt seit v196 die Sekunden-Schleife je Verbindung (EebusLpcFailsafeNext),
   // nicht mehr als Vorstufe des Limit-Writes.
  const size_t cap = 704;   // Platz fuer den zusaetzlichen delete-Filter
  char *cmd = (char*)special_malloc(cap);
  if (nullptr == cmd) { EebusLpcSetResult(LPC_FAIL, "malloc"); return; }
   // timePeriod/endTime: Ladestation lehnt Limit ohne Dauer ab (update_limit: duration<=0 -> reject,
   // loadcontrol.cpp:265). Dauer variabel; Default 7200 s = PT2H (byte-identisch wie bisher).
   // HEMS-Modus (Energiemanager2) -> KEIN timePeriod. Der EINZIGE vom Energiemanager akzeptierte §14a-Write (frische SKI,
   // Mitschnitt, errorNumber:0) enthaelt NUR limitId/isLimitActive/value, KEIN timePeriod
   // (verifiziert: kein einziger Vergleichs-Steuerbox-write-TX im Erfolgs-Mitschnitt hat timePeriod). Unser Extra-Feld war ein
   // konkreter Frame-Unterschied zum akzeptierten Write. SteuVE-Modus (Ladestation) BEHAELT timePeriod (Ladestation lehnt
   // ein Limit ohne Dauer ab, loadcontrol.cpp:265) -> deshalb modusabhaengig, nicht global entfernt.
   // Die Geltungsdauer gilt jetzt in BEIDEN Betriebsarten — sie kommt aus dem Schreibauftrag
   // (lpc_dur_s), nicht mehr aus einem globalen Wert, und nur wer eine angibt, bekommt eine.
   // Die fruehere Annahme "der Energiemanager will keine Dauer" war eine ALTLAST aus der err7-Zeit:
   // unsere damaligen Writes MIT Dauer wurden abgelehnt, aber die Ursache war der fehlende
   // Zeitzonen-Suffix im Heartbeat, NICHT das Feld. Nach dessen Behebung wurde nie wieder ein
   // Write mit Dauer geschickt. Fachlich muss ein Controllable System eine Befristung koennen —
   // sonst koennte der Netzbetreiber keine Ein-Stunden-Vorgabe machen; die Norm sieht sie in
   // [LPC-004]/[LPP-004] ausdruecklich vor. Ohne Angabe bleibt der bewiesene Frame zeichengleich.
  char tp[56] = "";
  if (3600 == ESp->lpc_dur_s) {
    strcpy(tp, "{\"timePeriod\":[{\"endTime\":\"PT1H\"}]},");
  } else if (7200 == ESp->lpc_dur_s) {
    strcpy(tp, "{\"timePeriod\":[{\"endTime\":\"PT2H\"}]},");
  } else if (ESp->lpc_dur_s > 0) {
    snprintf(tp, sizeof(tp), "{\"timePeriod\":[{\"endTime\":\"PT%ldS\"}]},", ESp->lpc_dur_s);
  }   // 0 -> kein timePeriod
   // PARTIAL-Write (SPINE cmdControl "partial"). Strenge HEMS (Frame-Diff verifiziert) lehnen
   // einen FULL-Write der loadControlLimitListData mit resultData errorNumber 7 ab, akzeptieren aber
   // das gezielte Teil-Update genau eines Eintrags per Schluessel (limitId). Dazu muessen "function"
   // + "filter"{cmdControl:partial} als Geschwister VOR die Nutzdaten (gleiche cmd-Ebene).
   // beim Deaktivieren (isLimitActive:false) sendet die Vergleichs-Steuerbox
   // den value TROTZDEM mit (letzter Wert, NICHT 0) — internal/loadcontrol.go WriteLoadControlLimit setzt
   // Value immer. Unser schrieb beim Freigeben value:0 = der EINZIGE verbliebene Frame-Unterschied
   // zur Vergleichs-Steuerbox (Aktivieren-Write war schon byte-identisch). Nur HEMS: value = lpc_value (auch bei active=false).
   // SteuVE-Modus (Ladestation/Waermepumpen-Gateway) BEHAELT value:0 beim Release -> byte-identisch  (Drossel-Pfad unberuehrt).
   // der Wert geht VORZEICHENBEHAFTET auf den Draht. lpc_value ist der BETRAG in W,
   // das Vorzeichen kommt aus der Richtung. LPP-Norm, passive Vorzeichenkonvention [LPP-001]/[LPP-011]:
   // das Active Power Production Limit ist stets <= 0 (Erzeugung negativ); "ein Limit groesser als 0 W
   // SOLL abgelehnt werden", weil ein positiver Wert nur den Bezug begrenzen wuerde. Fuer consume (§14a)
   // bleibt der Wert positiv -> der bewiesene §14a-Frame ist zeichengleich .
  long frame_val = (long)ESp->lpc_value;
  if (1 == ESp->lpc_dir) { frame_val = -frame_val; }
   // v247: bei einer FREIGABE (isLimitActive false) nie den Wert 0 schicken. Befund 24.11. (Datensammler, eBUS):
   // nach jedem Neustart von HEMS/Steuerbox ging an das WP-Gateway "Freigabe, value 0"; das Gateway setzte die 0 kurz als
   // Grenze am eBUS um (B531 = 0 an WP und Heizstab) -> Verdichterstopp ~4 min. Normgerecht war der Frame (bei inaktiver
   // Grenze zaehlt der Wert nicht), die Vergleichs-Steuerbox schickt aber nie 0 (setzt Value immer). Neu:
   // Geraete-Weg -> Nennleistung des Geraets (gemeldet, sonst 11.000 W); Energiemanager-Weg ->
   // weiter der letzte Wert, nur statt 0 ebenfalls die Nennleistung. Einspeisung mit Erzeugungs-Nennleistung, negativ.
  if (!ESp->lpc_active_wish && (!eebus_hems_mode || (0 == frame_val))) {
    long nw = 0;
    const bool prod = (1 == ESp->lpc_dir);
    if (prod ? ESp->pnom_prod_ok : ESp->pnom_cons_ok) {
      nw = labs(prod ? ESp->pnom_prod : ESp->pnom_cons);
      int8_t sc = prod ? ESp->pnom_prod_sc : ESp->pnom_cons_sc;
      while ((sc > 0) && (nw < 100000000L)) { nw *= 10; sc--; }
      while (sc < 0) { nw /= 10; sc++; }
    }
    if (nw <= 0) { nw = 11000; }   // Nennleistung unbekannt: 11 kW (wie HEMS EEBUS_HEMS_NENN0_W)
    frame_val = prod ? -nw : nw;
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Freigabe an %s mit Wert %ld W (nie 0)"), ESp->peer_ip, frame_val);
  }
   // Filter-Liste. Standard = ein einziger partial-Eintrag (Frame wie zuvor, bewiesen). Mit
   // "EEBusDelDur 1" kommt im HEMS-Modus ein delete-Eintrag DAVOR: Selektor limitId + Element
   // timePeriod = "loesche die Geltungsdauer dieses Limits". Reihenfolge delete-vor-partial wie in
   // der Referenz; im SteuVE-Modus (tp gesetzt) waere ein Loeschen widersinnig -> dort nie.
   // der Filter haengt jetzt an der GELTUNGSDAUER, nicht mehr an einem Schalter. Automatik
   // (Default) = loeschen genau dann, wenn dieser Auftrag keine Dauer traegt — "unbefristet" darf
   // keine alte Endzeit erben. Die Ausnahmewerte 0/1 sind Pruef-Uebersteuerungen (s. eebus_del_dur).
   // Sachlich ist die Automatik identisch mit "EEBusDelDur 1"; neu ist, dass die Bedingung
   // aus der Sache folgt statt aus einer Einstellung, die jemand versehentlich umlegen kann.
  const bool del_dur_now = (0 == eebus_del_dur) ? false
                         : (1 == eebus_del_dur) ? true
                                                : ('\0' == tp[0]);
   // v195 (24.10., meierchen006 "wie der Energiemanager"): das Loeschen der Geltungsdauer gilt jetzt in BEIDEN
   // Betriebsarten, nicht nur im HEMS-Modus. Anlass Nacht 23./24.10. am HEMS (gleicher Code): die
   // Freigabe an die Wallbox (SteuVE-Partner) ging ohne delete raus -> Wallbox-Log "deactivated, duration:
   // unchanged", die Wallbox lud danach die ganze Nacht mit der alten Grenze (3.591 W). Der Energiemanager schickt
   // beim Freigeben das delete -> "duration: removed", damit nie Probleme. Betrifft hier die
   // Direktsteuerung (Wallbox/WP ohne HEMS). Ein Auftrag MIT Dauer (tp gesetzt) loescht weiterhin nicht.
  char filt[208];
  if (del_dur_now) {
    snprintf(filt, sizeof(filt),
      "[{\"cmdControl\":[{\"delete\":[]}]},"
       "{\"loadControlLimitListDataSelectors\":[{\"limitId\":%d}]},"
       "{\"loadControlLimitDataElements\":[{\"timePeriod\":[]}]}],"
      "[{\"cmdControl\":[{\"partial\":[]}]}]", ESp->lpc_limit_id);
  } else {
    strcpy(filt, "[{\"cmdControl\":[{\"partial\":[]}]}]");
  }
  snprintf(cmd, cap,
    "{\"function\":\"loadControlLimitListData\"},"
    "{\"filter\":[%s]},"
    "{\"loadControlLimitListData\":[{\"loadControlLimitData\":[["
      "{\"limitId\":%d},{\"isLimitActive\":%s},%s{\"value\":[{\"number\":%ld},{\"scale\":0}]}"
    "]]}]}", filt, ESp->lpc_limit_id, ESp->lpc_active_wish ? "true" : "false", tp, frame_val);
   // (HEMS, EEBus-LPC-Spec [LPC-914]): der Energiemanager bewertet den Limit-Write NUR, wenn UNMITTELBAR davor ein
   // frischer EG-Heartbeat kam (und der Write <60 s folgt). Daher direkt vor dem Write einen Heartbeat-NOTIFY
   // an den DeviceDiagnosis-Abonnenten (Energiemanager) schicken. Nur wenn der Peer uns schon abonniert hat (hb_sub).
   // SteuVE-Pfad unveraendert (kein HB vor dem Write -> byte-identisch ).
   // v239 ([LPC-913], LPC TS Z. 369-375): auch im SteuVE-Betrieb, wenn unser letzter Herzschlag laenger als 50 s her ist
   // (sonst bleibt der Ablauf unveraendert - der Takt 20 s haelt die 60 s ohnehin ein).
  if (ESp->hb_sub && (eebus_hems_mode || !ESp->hb_tx_at || ((millis() - ESp->hb_tx_at) > 50000UL))) {
    EebusSpineSendHeartbeat("notify", false, 0, ESp->hb_cli_ent, ESp->hb_cli_feat);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Heartbeat %u (vor Write) an %s gesendet"), ESp->hb_counter - 1, ESp->peer_ip);
    EebusServeStateNotify();   // operatingState frisch vor dem Write -> Energiemanager sieht uns "aktiv"
  }
  EebusSpineSendAddr("write", false, 0, cmd,
                     EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
  free(cmd);
  if (ESp->send_broken) {   // v226: die Vorgabe ging nicht raus (Test 05.11. 12:29:08: trotzdem "Limit 7000 W geschrieben")
     // VDE-FNN-Empfehlung Tenorziffer 2a, Verbindungsueberwachung: Steuerbefehle waehrend einer Kommunikationsstoerung
     // abweisen und die Nichtausfuehrbarkeit protokollieren. LPC_FAIL schreibt den Eintrag ins Betriebslogbuch
     // (EebusLbVorgabe). Den Abbau und den Wiederaufbau hat EebusSendBroken schon veranlasst; eine noch laufende Grenze
     // schreibt die Box nach dem Wiederaufbau mit Restzeit erneut (v221, [LPC-913]).
    EebusLpcSetResult(LPC_FAIL, "nicht ausfuehrbar - Senden abgebrochen");
    return;
  }
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
  char t[56];
  if (ESp->lpc_active_wish) {
    snprintf(t, sizeof(t), (1 == ESp->lpc_dir) ? "Einspeiselimit %ld W geschrieben" : "Limit %ld W geschrieben", frame_val);
  } else {
    strlcpy(t, (1 == ESp->lpc_dir) ? "Einspeise-Freigabe geschrieben" : "Freigabe geschrieben", sizeof(t));
  }
  {   // v243 (LPC/LPP-913, LPP Z. 389-392): erster Grenz-Write dieser Sitzung je Richtung
    const uint8_t d = (1 == ESp->lpc_dir) ? 1 : 0;
    if (!ESp->sess_dir_w[d]) { ESp->sess_dir_w[d] = millis(); if (!ESp->sess_dir_w[d]) { ESp->sess_dir_w[d] = 1; } }
  }
  EebusLpcSetResult(LPC_WRITE, t);
}

// v243 (LPP Z. 389-392, LPC Z. 372-375): andere Datenpunkte wertet die Anlage erst nach Herzschlag + Grenz-Write aus.
// Failsafe-Werte deshalb erst, wenn fuer JEDE vorhandene Richtung in dieser Sitzung eine Grenze geschrieben ist.
bool EebusFsGrenzenDa(void) {
  bool eine = false;
  for (uint8_t d = 0; d < 2; d++) {
    const bool noetig = (ESp->lpc_lid_dir[d] >= 0) && !((1 == d) && EebusOhneEinspeisung());
    if (noetig && !ESp->sess_dir_w[d]) { return false; }
    if (ESp->sess_dir_w[d]) { eine = true; }
  }
  return eine;
}

// Reaktion auf errorNumber:7 ("Write failed") auf den §14a-Write. EEBus-LPC-Spec [LPC-906]: die erste
// Ablehnung kann reines Timing sein (CS gerade im "init") -> "the EG may simply send Heartbeat and write
// command once more". [LPC-914]: der Write wird nur bewertet, wenn unmittelbar davor ein Heartbeat kam.
// Also im HEMS-Modus: statt sofort zu scheitern, einen kurz verzoegerten Retry ansetzen (der HB kommt im
// Retry aus EebusLpcWrite). Bis EEBUS_LPC_WRITE_MAX_TRIES; danach erst LPC_FAIL. SteuVE-Modus (Ladestation/Waermepumpen-Gateway)
// scheitert wie bisher sofort -> Verhalten byte-identisch .
// v225: eine Vorgabe ist gescheitert -> nach EEBUS_ABGELEHNT_WDH_MS ueber die Sicherung erneut (s. v214). Nur wenn wirklich eine Vorgabe
// lief, nicht beim Onboarding, sonst verschoebe das die erste Vorgabe nach dem Verbinden ueber die 60 s aus [LPC-913] hinaus.
// v237: Wartezeit der naechsten Wiederholung: 60 s x (Anzahl bisheriger Wiederholungen + 1), hoechstens EEBUS_WDH_MAX_MS.
uint32_t EebusWdhMs(void) {
  uint32_t w = EEBUS_ABGELEHNT_WDH_MS * (uint32_t)(ESp->wdh_n + 1);
  return (w > EEBUS_WDH_MAX_MS) ? EEBUS_WDH_MAX_MS : w;
}
void EebusVorgabeWiederholen(const char *grund) {
  if (!ESp->sess_vorgabe) { return; }
  const uint8_t d = (1 == ESp->lpc_dir) ? 1 : 0;
  if (!ESp->lpc_active_wish) { ESp->frei_offen[d] = true; }
  ESp->sess_vorgabe = false;
  ESp->wdh_offen = true;   // v237
  ESp->sich_stufe = 0;
  const uint32_t warte = EebusWdhMs();   // v237: 60 s, 120 s, 180 s ... hoechstens EEBUS_WDH_MAX_MS (LPC IG 2.5 Z. 283-286)
  if (ESp->wdh_n < 255) { ESp->wdh_n++; }
  ESp->sich_wdh_at = Eebus126At(warte);
  if (!ESp->sich_wdh_at) { ESp->sich_wdh_at = 1; }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Vorgabe @ %s %s - erneuter Versuch in %u s (%u. Wiederholung)"), ESp->peer_ip, grund,
         warte / 1000, ESp->wdh_n);
}

void EebusLpcWriteRejected(void) {
  if (eebus_hems_mode && (ESp->lpc_write_tries < EEBUS_LPC_WRITE_MAX_TRIES) &&
      ESp->sess_at && ((millis() - ESp->sess_at) < EEBUS_LPC_INIT_FENSTER_MS)) {   // v237: nur im "init"-Fenster
    ESp->lpc_write_tries++;
    ESp->lpc_deadline = 0;   // Phasen-Timeout ruht bis zum Retry
    ESp->lpc_write_retry_at = Eebus126At(EEBUS_LPC_WRITE_RETRY_MS);
    char t[56]; snprintf(t, sizeof(t), "err7 -> Heartbeat+Retry %u/%u",
                         ESp->lpc_write_tries, EEBUS_LPC_WRITE_MAX_TRIES);
    EebusLpcSetResult(LPC_WRITE, t);
    {   // v245: auch diese erste Ablehnung ins Betriebslogbuch (vorher nur die nach dem Neuversuch)
      char wert[24];
      EebusLbVorgabeWert(wert, sizeof(wert));
      EebusLbLog("Vorgabe", ESp->peer_ski, ESp->peer_ip, ESp->lpc_dir, wert, ESp->lpc_dur_s,
                 "Write abgelehnt - Herzschlag+Write sofort erneut (LPC-906)");
    }
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Write err7 @ %s -> Heartbeat+Write erneut (Versuch %u/%u, LPC-906/914)"),
           ESp->peer_ip, ESp->lpc_write_tries, EEBUS_LPC_WRITE_MAX_TRIES);
    return;
  }
  EebusLpcSetResult(LPC_FAIL, "Write abgelehnt");
   // v214 (meierchen006 26.10.: "das muss alles so funktionieren", eine echte Steuerbox hat keinen Knopf): eine ABGELEHNTE
   // Vorgabe gilt nicht als geschrieben. Bisher setzte schon das Abschicken sess_vorgabe, die Sicherung lief nur einmal
   // -> nach einer Ablehnung (z. B. Gegenstelle vertraute uns noch nicht) kam nie wieder eine Vorgabe, die Anlage blieb
   // "autonom" ([LPC-902]/[LPC-916] verlangen Herzschlag UND Vorgabe). Jetzt: Sicherung nach EEBUS_ABGELEHNT_WDH_MS
   // erneut (eigene Grenze erneut, sonst Freigabe), so lange, bis die Gegenstelle annimmt.
  {
   // Zeitpunkt NICHT in sess_at legen: die Pruefung rechnet (millis() - sess_at) ohne Vorzeichen, ein Zeitpunkt in der
   // Zukunft ergaebe eine riesige Zahl -> sofort faellig (erster v214-Build: Wiederholung im Sekundentakt).
    EebusVorgabeWiederholen("abgelehnt");
  }
  if (eebus_hems_mode) {   // v179: spaeter nachlesen
    ESp->lpc_rej_late = true;
    ESp->late_rd_n = 0;
    ESp->late_rd_base = millis();
    ESp->late_rd_at = ESp->late_rd_base + (uint32_t)kEebusLateReadS[0] * 1000;
  }
}

// Read-Back: hat das Limit den gewuenschten Zustand?
void EebusLpcVerify(void) {
  EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
  ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
  EebusLpcSetResult(LPC_VERIFY, "Read-Back angefragt");
}

// Read-Back NICHT sofort, sondern verzoegert ansetzen (Latenz der Zustandsuebernahme im
// Geraet). first=true = erster Ansatz nach DELAY; first=false = Nachpoll-Versuch nach RETRY.
// Der eigentliche Read-Back-Read wird dann aus dem FUNC_EVERY_SECOND-Slot-Loop ausgeloest, wenn
// lpc_verify_at erreicht ist. Bis dahin ruht der Phasen-Timeout (lpc_deadline=0), Zustand LPC_WRITE.
void EebusLpcVerifySchedule(bool first) {
  if (first) { ESp->lpc_verify_tries = 0; ESp->lpc_verify_failed = false; }   // 
  ESp->lpc_verify_at = Eebus126At((first ? EEBUS_LPC_VERIFY_DELAY_MS : EEBUS_LPC_VERIFY_RETRY_MS));
  ESp->lpc_deadline  = 0;
  EebusLpcSetResult(LPC_WRITE, first ? "geschrieben - Read-Back verzoegert" : "Read-Back Nachpoll");
}

// Onboarding beim Connect — dieselbe Kette wie EebusLpcStart (Bind/ReadDesc/ReadData/
// ReadCfg), aber OHNE Write (EebusLpcWrite faengt lpc_onboard_only ab und setzt lpc_onboarded). Fuellt den
// Cache (lpc_bound, lpc_limit_id, DeviceConfig gelesen) und laesst die Steuerbox-Beziehung REIFEN, bevor ein
// §14a-Write kommt — genau der am Energiemanager beobachtete Reifungsbedarf (err7 solange die Beziehung frisch ist; der Energiemanager
// beginnt uns erst nach einigen Heartbeats aktiv zu ueberwachen). Nur HEMS-Modus; SteuVE (Ladestation/Waermepumpen-Gateway) bleibt
// beim bewiesenen write-getriebenen Ablauf voellig unberuehrt. Idempotent (Guards), wird 1x beim Connect angestossen.
void EebusLpcStartOnboard(void) {
  if (eebus_role < 1 || SME_DONE != ESp->sme) { return; }
  if (ESp->lpc_onboarded || ESp->lpc_onboard_only) { return; }   // schon fertig / laeuft
  if ((ESp->lpc_state >= LPC_BIND) && (ESp->lpc_state <= LPC_VERIFY)) { return; }   // Kette laeuft gerade (z.B. Write)
  ESp->lpc_onboard_only = true;
  ESp->lpc_write_tries = 0;
  ESp->lpc_write_retry_at = 0;
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC-Onboarding beim Connect (Schritt 2, ohne Write) @ %s"), ESp->peer_ip);
  if (ESp->lpc_bound) {
    if (ESp->lpc_limit_id >= 0) { EebusLpcWrite(); }   // schon alles gecacht -> sofort Onboard-Abschluss (Write faengt ab)
    else { EebusLpcReadDesc(); }
  } else {
    EebusLpcSendBind();
  }
}

// Einstieg: Limit setzen (watt>0) oder freigeben (active=false). Nur bei Rolle EIN.
// Setzt die Sequenz in Gang; braucht eine stehende Verbindung (SME_DONE).
bool EebusLpcStart(uint32_t watt, bool active, uint8_t dir, long dur_s) {
  if (eebus_role < 1) { EebusLpcSetResult(LPC_FAIL, "Rolle AUS (EEBusRole 1/2)"); return false; }
  if (SME_DONE != ESp->sme) { EebusLpcSetResult(LPC_FAIL, "keine SHIP-Verbindung (done)"); return false; }
   // v211 (meierchen006 26.10.): ein DIREKT gesteuertes Geraet wird immer genau auf seine Mindestleistung gedimmt
   // (BK6 Ziff. 4.5.1: 4200 W, Satz 2: WP/Klima > 11 kW 0,4 x P) - sie ist Untergrenze UND Dimmwert. Gilt fuer
   // Einzelbefehl, "alle" und Sicherung; beim HEMS (Ziff. 4.4.b) bleibt der Wert, wie er kommt (Gesamtwert).
  if ((0 == dir) && active && (0 == EebusModeEffective(ESp->peer_ski))) {
    const uint32_t m = EebusDirektMinW((uint32_t)(ESp - EConn));
    if (watt != m) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s direkt gesteuert: Dimmen fest auf %u W statt %u W (BK6 Ziff. 4.5.1)"),
             ESp->peer_ip, m, watt);
      watt = m;
    }
  }
  ESp->sess_vorgabe = true;   // v200: in dieser Sitzung wurde eine Vorgabe (Freigabe oder Grenze) geschrieben
  if (!ESp->sess_write_at) { ESp->sess_write_at = millis(); if (!ESp->sess_write_at) { ESp->sess_write_at = 1; } }   // v239
  if (active) {   // v241 (LPC IG 2.10, SHOULD): nur Hinweis - die Box gibt Vorgaben unverzoegert weiter (s. Kopf v241)
    if (ESp->ig210_at && ((millis() - ESp->ig210_at) < 300000UL)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Hinweis: Grenze an %s nach %u s erneut (LPC IG 2.10 empfiehlt hoechstens alle 5 min)"),
             ESp->peer_ip, (uint32_t)((millis() - ESp->ig210_at) / 1000));
    }
    ESp->ig210_at = millis(); if (!ESp->ig210_at) { ESp->ig210_at = 1; }
  }
  if (active) { ESp->frei_offen[(1 == dir) ? 1 : 0] = false; }   // v225: eine neue Grenze ersetzt die offene Freigabe
  {   // v200: eigene Grenze je Richtung und SKI merken (Grenze -> Wert/Ende, Freigabe -> weg); ueberlebt Wiederverbindungen
    const uint8_t d = (dir > 1) ? 1 : dir;
    ESp->best_akt[d] = false;   // v237: bis zur Bestaetigung dieser neuen Vorgabe gibt es nichts abzubrechen
    if (strcasecmp(ESp->own_ski, ESp->peer_ski)) {
      strlcpy(ESp->own_ski, ESp->peer_ski, sizeof(ESp->own_ski));
      ESp->own_akt[0] = ESp->own_akt[1] = false;
    }
    ESp->own_akt[d] = active;
    ESp->own_w[d] = watt;
    ESp->own_ende[d] = (active && (dur_s > 0)) ? Eebus126At((uint32_t)dur_s * 1000UL) : 0;   // v231 (4.12)
    if (active && (dur_s > 0) && (0 == ESp->own_ende[d])) { ESp->own_ende[d] = 1; }
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {   // v221: nur dieser Slot fuehrt den Merker dieser SKI und Richtung
      if ((&EConn[i] != ESp) && !strcasecmp(EConn[i].own_ski, ESp->peer_ski)) { EConn[i].own_akt[d] = false; }
    }
  }
   // v178 (08.10.): ein Nutzerbefehl hat Vorrang vor einem noch laufenden oder haengengebliebenen
   // Onboarding. Sonst faengt EebusLpcWrite das Kettenende als Onboarding ab, verwirft den Wert und
   // merkt statt der Grenze eine Anmeldung (Freigabe) vor (gesehen 06.10. 13:39:51: EEBusLpc 8160
   // endete als Freigabe). Die automatische Anmeldung wird dabei NICHT vorgemerkt — sie wuerde als
   // Freigabe die gerade befohlene Grenze aufheben; der Befehl selbst ist schon der erste Write.
  if (ESp->lpc_onboard_only) {
    ESp->lpc_onboard_only = false;
    ESp->lpc_onboarded = true;
    ESp->lpc_reg_step = 0;
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Onboarding @ %s abgebrochen - Nutzerbefehl hat Vorrang"), ESp->peer_ip);
  }
   // Richtungswechsel auf derselben Verbindung. Binding und Abo bleiben gueltig (beide
   // Grenzen haengen am selben LoadControl-Feature), aber die limitId gilt nur fuer EINE Richtung ->
   // bei Wechsel neu aus der Beschreibung holen, sonst schriebe §9 auf die §14a-Grenze.
  if (dir != ESp->lpc_dir) {
    ESp->lpc_dir = dir;
    ESp->lpc_limit_id = ESp->lpc_lid_dir[(dir > 1) ? 1 : dir];   // v184: aus derselben Beschreibung gemerkt (sonst -1 = neu lesen)
  }
   // Betriebsmodus geraeteweise aus der peer-SKI (EEBusPeerMode). Gefunden -> setzt eebus_hems_mode
   // fuer diesen Write; nicht zugeordnet (z.B. manuelle IP-Verbindung, peer_ski="manual") -> globaler
   // EEBusHems-Default bleibt. Damit gelten pro Geraet automatisch die richtigen §14a-Write-Regeln.
  {
    int pm = EebusModeEffective(ESp->peer_ski);   // Zuordnung ODER Geraetetyp
    if (pm >= 0) {
      ESp->hems_m = (int8_t)pm;   // v231: je Verbindung
      eebus_hems_mode = (uint8_t)pm;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Betriebsmodus %s aus SKI-Zuordnung @ %s"),
             pm ? "HEMS-Partner" : "SteuVE-Partner", ESp->peer_ip);
    }
  }
   // manuelles LPC-Ziel (EEBusTarget) erzwingen — Blind-Adressierung, wenn der Peer
   // (Waermepumpen-Gateway) seine Discovery nicht liefert. Zielwechsel -> Binding/limitId neu.
  if (eebus_tgt_ent >= 0) {
    if ((ESp->lpc_peer_ent != eebus_tgt_ent) || (ESp->lpc_peer_feat != eebus_tgt_feat)) {
      ESp->lpc_peer_ent = eebus_tgt_ent;
      ESp->lpc_peer_feat = eebus_tgt_feat;
      ESp->lpc_bound = false;
      ESp->lpc_limit_id = -1; ESp->lpc_lid_dir[0] = ESp->lpc_lid_dir[1] = -1;   // v184
      ESp->lpc_onboarded = false;   // 
      ESp->lpc_fs_val_key = -1; ESp->lpc_fs_dur_key = -1; ESp->lpc_fs_prod_key = -1; ESp->lpc_fs_done = false; ESp->lpc_fs_step = 0;   // 
    }
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC-Ziel manuell ent%d/feat%d (EEBusTarget) @ %s"),
           ESp->lpc_peer_ent, ESp->lpc_peer_feat, ESp->peer_ip);
  }
   // peer_dev (SPINE-Geraeteadresse) ist optional: fehlt sie, adressieren wir point-to-point
   // nur ueber entity/feature (der Peer ist auf der SHIP-Verbindung eindeutig).
  ESp->lpc_value = watt;
  ESp->lpc_val_dir[dir ? 1 : 0] = watt;
   // 06.11. (meierchen006, Box .21 bewiesen 22:21:17): Wert JE RICHTUNG - "§14a aufheben" schickte den letzten §9-Wert (11480 W)
  ESp->lpc_active_wish = active;
  ESp->lpc_dur_s = (dur_s < 0) ? 0 : dur_s;   // Geltungsdauer dieses Auftrags
  ESp->lpc_write_tries = 0;   // err7-Retry-Zaehler fuer diesen neuen Write-Auftrag zuruecksetzen
  ESp->lpc_write_retry_at = 0;
  ESp->lpc_rej_late = false; ESp->late_rd_at = 0; ESp->late_rd_n = 0;   // v179: neuer Auftrag -> kein Nachlesen
   // Binding nur einmal noetig; danach direkt Beschreibung/Write
  if (ESp->lpc_bound) {
    if (ESp->lpc_limit_id >= 0) { EebusLpcWrite(); }   // limitId schon bekannt -> direkt schreiben
    else { EebusLpcReadDesc(); }
  } else {
    EebusLpcSendBind();
  }
  return true;
}

// Ein empfangenes SPINE-Datagramm (classifier 2) verarbeiten.
// eine ganze Zahl hinter einem Schluessel lesen — VORZEICHENBEHAFTET. EebusJsonInt liefert
// uint32_t und taugt damit nicht fuer Messwerte: die Einspeisung steht als negative Zahl auf dem
// Draht (gridFeedIn -17650896 Wh), und ein Einspeiselimit ebenfalls (-11480 W).
bool EebusJsonLong(const char *s, const char *key, long *out) {
  if ((nullptr == s) || (nullptr == key)) { return false; }
  char pat[32];
  snprintf(pat, sizeof(pat), "\"%s\"", key);   // v243: Leerraum um den Doppelpunkt zulassen
  const size_t pl = strlen(pat);
  const char *p = s;
  while (nullptr != (p = strstr(p, pat))) {
    const char *q = p + pl;
    while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
    if (':' == *q) {
      q++;
      while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
      if ((('-' != *q) && ('+' != *q)) && ((*q < '0') || (*q > '9'))) { return false; }
      *out = strtol(q, nullptr, 10);
      return true;
    }
    p += pl;
  }
  return false;
}

// v243: Schluessel k ("\"name\"") zwischen from und end (end nullptr = bis Textende) suchen.
const char *EebusSegKey(const char *from, const char *end, const char *k) {
  if (nullptr == from) { return nullptr; }
  const char *f = strstr(from, k);
  return (f && (!end || (f < end))) ? f : nullptr;
}
// v243: Text-Wert eines Schluessels innerhalb des Abschnitts (leer, wenn nicht vorhanden).
void EebusSegStr(const char *from, const char *end, const char *name, char *out, size_t outlen) {
  out[0] = '\0';
  char k[40]; snprintf(k, sizeof(k), "\"%s\"", name);
  const char *p = EebusSegKey(from, end, k);
  if (p) { EebusJsonStr(p, name, out, outlen); }
}
// v243: steht der Schluessel im Abschnitt mit dem Wert false?
bool EebusSegFalse(const char *from, const char *end, const char *name) {
  char k[40]; snprintf(k, sizeof(k), "\"%s\"", name);
  const char *p = EebusSegKey(from, end, k);
  if (nullptr == p) { return false; }
  p += strlen(k);
  while ((' ' == *p) || ('\t' == *p) || ('\r' == *p) || ('\n' == *p) || (':' == *p)) { p++; }
  return (0 == strncmp(p, "false", 5));
}

// v243 (LPP 3.1.2 Z. 766-788, MGCP Z. 444-467): Use-Case-Auskunft der Anlage auswerten - Entity des
// GridConnectionPoint (MGCP) und ob die Anlage LPP als ControllableSystem anbietet (useCaseAvailable nicht false).
void EebusUcAuswerten(const char *uc) {
  ESp->gcp_uc_ent = -1;
  ESp->lpp_uc = 0;
  if (nullptr == uc) { ESp->lpp_uc = -1; return; }
  const char *a = uc;
  while (nullptr != (a = strstr(a, "\"address\""))) {
    const char *nx = strstr(a + 9, "\"address\"");
    char actor[32];
    EebusSegStr(a, nx, "actor", actor, sizeof(actor));
    int ent = -1;
    const char *ep = EebusSegKey(a, nx, "\"entity\"");
    if (ep) { const char *b = strchr(ep, '['); if (b && (!nx || (b < nx))) { ent = atoi(b + 1); } }
    const char *u = a;
    while (nullptr != (u = EebusSegKey(u, nx, "\"useCaseName\""))) {
      char name[48] = { 0 };
      EebusJsonStr(u, "useCaseName", name, sizeof(name));
      const char *un = EebusSegKey(u + 13, nx, "\"useCaseName\"");
      const bool verf = true;   // v246 (SPINE TS RTC_003, IG 2.2 Z. 136-138): useCaseAvailable eines Servers ignorieren (vorher: false = verworfen)
      if (verf && (0 == strcmp(name, "monitoringOfGridConnectionPoint")) && (0 == strcmp(actor, "GridConnectionPoint"))) {
        ESp->gcp_uc_ent = ent;
      }
      if (verf && (0 == strcmp(name, "limitationOfPowerProduction")) && (0 == strcmp(actor, "ControllableSystem"))) {
        ESp->lpp_uc = 1;
      }
      u += 13;
    }
    a += 9;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Use-Cases von %s: LPP als ControllableSystem %s, GridConnectionPoint %s%d"),
         ESp->peer_ip, (1 == ESp->lpp_uc) ? "ja" : "NEIN - keine Einspeisegrenze (LPP 3.1.2)",
         (ESp->gcp_uc_ent >= 0) ? "auf Entity " : "nicht gemeldet ", ESp->gcp_uc_ent);
}

// v243 (MGCP Tab. 15-20): passen Messart und Einheit zur Bedeutung? code wie m_scope.
bool EebusMessArtPasst(int8_t code, const char *mt, const char *un) {
  switch (code) {
    case 1: return !strcmp(mt, "power") && !strcmp(un, "W");
    case 2:
    case 3: return !strcmp(mt, "energy") && !strcmp(un, "Wh");
    case 4: return !strcmp(mt, "current") && !strcmp(un, "A");
    case 5: return !strcmp(mt, "voltage") && !strcmp(un, "V");
    case 6: return !strcmp(mt, "frequency") && !strcmp(un, "Hz");
  }
  return false;
}

// Nutzdaten aus einem eingehenden Datagramm mitlesen und im Slot behalten.
// Wird fuer JEDES Datagramm aufgerufen und greift nur, wenn der jeweilige Block enthalten ist —
// kein Eingriff in die Schreib-Zustandsmaschine, kein zusaetzlicher Netzverkehr.
// Die Bedeutung der Messwert-Kennungen wird aus der Beschreibung GELERNT (scopeType), nicht geraten;
// dieselbe Nummer heisst bei einem anderen Hersteller etwas anderes.
// Platz fuer den Wert einer FREMDEN Einheit suchen (oder anlegen). Schluessel ist Einheit + Kennung,
// nicht die Kennung allein — genau daran ist die Anzeige am 29.07. gescheitert.
int EebusMoFind(int ent, uint8_t id, bool anlegen) {
  for (uint8_t i = 0; i < ESp->mo_n; i++) {
    if ((ESp->mo_ent[i] == ent) && (ESp->mo_id[i] == id)) { return (int)i; }
  }
  if (!anlegen || (ESp->mo_n >= 16)) { return -1; }
  int i = ESp->mo_n++;
  ESp->mo_ent[i] = ent; ESp->mo_id[i] = id;
  ESp->mo_v[i] = 0; ESp->mo_sc[i] = 0; ESp->mo_have[i] = false; ESp->mo_scope[i][0] = '\0';
  return i;
}

void EebusHarvest(const char *json, int src_ent) {
  const char *p, *q, *nx;

   // --- Herzschlag der GEGENSTELLE: Ankunft und Zusage merken --------------------------------
   // Diese Funktion laeuft VOR der Verzweigung nach cmdClassifier und damit auch fuer "notify" —
   // eine Nachrichtenart, fuer die es sonst gar keinen Zweig gibt. Genau deshalb sind diese
   // Herzschlaege bisher spurlos durchgefallen, obwohl sie die ganze Zeit ankamen.
   // ⚠️ NUR ein Datensatz MIT ZAEHLER gilt als Herzschlag. Der blosse Funktionsname steht auch in
   // Nachrichten, die ueber das Leben der Gegenstelle nichts aussagen: beim Verbindungsaufbau
   // gemessen (Discovery/Abo-Verkehr setzten die Uhr, bevor der erste echte Herzschlag da war —
   // erkennbar daran, dass die zugesagte Frist noch fehlte), und ein blosser Read auf diese
   // Funktion traegt ihn ebenfalls. Zaehlten die mit, verlaengerten sie die Frist, ohne dass
   // jemand lebt — genau der Fehler, den diese Ueberwachung beheben soll.
  uint32_t hc = 0;
  if ((nullptr != strstr(json, "\"deviceDiagnosisHeartbeatData\"")) &&
      EebusJsonInt(json, "heartbeatCounter", &hc)) {
    ESp->peer_hb_ctr = hc;
    char tmo[16] = { 0 };
    if (EebusJsonStr(json, "heartbeatTimeout", tmo, sizeof(tmo))) {
      long s = EebusIsoDurSecs(tmo);   // "PT2M" -> 120
      if ((s > 0) && (s < 65535)) { ESp->peer_hb_tmo_s = (uint16_t)s; }
    }
    if (ESp->peer_hb_at && !ESp->peer_hb_lost && ((millis() - ESp->peer_hb_at) > EEBUS_HB_LUECKE_MS)) {
   // v241 (LPC-911/913, Testfall EGConnection_002): nach > 110 s Pause kann die Anlage im Failsafe sein, auch wenn die
   // doppelte Zusage (Verlust-Erkennung unten) noch nicht um ist -> wie "zurueck": Herzschlag + Grenze.
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Herzschlag von %s nach %u s Pause - Vorgabe wird erneuert (LPC-913)"),
             ESp->peer_ip, (uint32_t)((millis() - ESp->peer_hb_at) / 1000));
      ESp->hb_zurueck = true;
    }
    ESp->peer_hb_at = millis();
    if (0 == ESp->peer_hb_at) { ESp->peer_hb_at = 1; }   // 0 ist reserviert fuer "noch keiner da"
    if (ESp->peer_hb_lost) {
      ESp->peer_hb_lost = false;
      ESp->hb_zurueck = true;   // v225: Logbuch und Vorgabe im Takt, nicht im Empfangspfad
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Herzschlag von %s ist wieder da (Zaehler %u) - %s"),
             ESp->peer_ip, ESp->peer_hb_ctr, GetDateAndTime(DT_LOCAL).c_str());
    }
  }

   // --- Beschreibung der Messwerte: measurementId -> Bedeutung -------------------------------
  if (nullptr != strstr(json, "\"measurementDescriptionData\"")) {
   // Gehoert diese Beschreibung zum NETZANSCHLUSSPUNKT? Erkennungsmerkmal sind die Bedeutungen
   // gridFeedIn/gridConsumption - oder (v243, MGCP Z. 444-467) die Entity, die die Anlage in ihrer Use-Case-Auskunft
   // als GridConnectionPoint nennt (ein Netzanschlusspunkt ohne Zaehlerstaende wird so auch erkannt).
    bool ist_netz = (nullptr != strstr(json, "\"gridFeedIn\""))
                 || (nullptr != strstr(json, "\"gridConsumption\""))
                 || ((ESp->gcp_uc_ent >= 0) && (src_ent == ESp->gcp_uc_ent));
    if (ist_netz) { ESp->meas_ent = src_ent; }
    int8_t n_cur = 0, n_vol = 0;   // v197: Phasenzaehler je Bedeutung (nur Rueckfall ohne ParameterDescription)
    q = json;
    while (nullptr != (q = strstr(q, "\"measurementId\""))) {
      uint32_t id = 0;
      EebusJsonInt(q, "measurementId", &id);
      nx = strstr(q + 15, "\"measurementId\"");
      char s[28], mt[16], un[8];
      EebusSegStr(q, nx, "scopeType", s, sizeof(s));
      EebusSegStr(q, nx, "measurementType", mt, sizeof(mt));
      EebusSegStr(q, nx, "unit", un, sizeof(un));
      if (s[0] && (id < 24)) {
        if (ist_netz) {
          int8_t code = 0;
          if      (0 == strcmp(s, "acPowerTotal"))    { code = 1; }
          else if (0 == strcmp(s, "gridFeedIn"))      { code = 2; }
          else if (0 == strcmp(s, "gridConsumption")) { code = 3; }
          else if (0 == strcmp(s, "acCurrent"))       { code = 4; }
          else if (0 == strcmp(s, "acVoltage"))       { code = 5; }
          else if (0 == strcmp(s, "acFrequency"))     { code = 6; }
          if (code && !EebusMessArtPasst(code, mt, un)) {   // v243 (MGCP Tab. 15-20)
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Messwert %u von %s (%s) mit Messart '%s' / Einheit '%s' passt nicht (MGCP Tab. 15-20) - ignoriert"),
                   id, ESp->peer_ip, s, mt, un);
            code = 0;
          }
          ESp->m_scope[id] = code;
          const int8_t ph = (4 == code) ? n_cur++ : ((5 == code) ? n_vol++ : 0);
          if (!(ESp->m_ph_desc & (1UL << id))) { ESp->m_phase[id] = ph; }   // v243: Beschreibung der Phase hat Vorrang
        } else {
   // Fremde Einheit: Bedeutung im eigenen Platz merken, die Tabelle des Netzanschlusspunkts
   // bleibt unberuehrt. Sonst haette eine Batterie-Beschreibung dort "Id 3 = Ladezustand"
   // eingetragen und die Spannung des Netzanschlusspunkts waere danach falsch einsortiert.
          int i = EebusMoFind(src_ent, (uint8_t)id, true);
          if (i >= 0) {
            strlcpy(ESp->mo_scope[i], s, sizeof(ESp->mo_scope[i]));
            strlcpy(ESp->mo_unit[i], un, sizeof(ESp->mo_unit[i]));
          }
        }
      }
      q += 15;
    }
  }

   // --- v243 (MGCP Tab. 18/19, [MGCP-051], [MGCP-061]): Phase und Bezugspunkt je Messwert -------
  if ((nullptr != strstr(json, "\"electricalConnectionParameterDescriptionData\"")) &&
      ((ESp->meas_ent < 0) || (src_ent == ESp->meas_ent))) {
    q = json;
    while (nullptr != (q = strstr(q, "\"parameterId\""))) {
      nx = strstr(q + 13, "\"parameterId\"");
      long id = -1;
      const char *mp = EebusSegKey(q, nx, "\"measurementId\"");
      if (mp) { EebusJsonLong(mp, "measurementId", &id); }
      char ph[6], ref[10];
      EebusSegStr(q, nx, "acMeasuredPhases", ph, sizeof(ph));
      EebusSegStr(q, nx, "acMeasuredInReferenceTo", ref, sizeof(ref));
      int8_t p = -1;
      if ((1 == strlen(ph)) && (ph[0] >= 'a') && (ph[0] <= 'c')) {
        const int8_t i = ph[0] - 'a';
        if (!ref[0] || !strcmp(ref, "neutral")) { p = i; }   // Strom je Phase bzw. Spannung gegen N
        else if (((0 == i) && !strcmp(ref, "b")) || ((1 == i) && !strcmp(ref, "c")) || ((2 == i) && !strcmp(ref, "a"))) { p = 3 + i; }
      }
      if ((id >= 0) && (id < 24) && (p >= 0)) { ESp->m_phase[id] = p; ESp->m_ph_desc |= (1UL << id); }
      q += 13;
    }
  }
   // --- v243 (MGCP Tab. 15-19): positiveEnergyDirection des Netzanschlusspunkts ---------------
  if ((nullptr != strstr(json, "\"electricalConnectionDescriptionData\"")) &&
      ((ESp->meas_ent < 0) || (src_ent == ESp->meas_ent))) {
    char ped[12];
    EebusSegStr(json, nullptr, "positiveEnergyDirection", ped, sizeof(ped));
    if (ped[0]) {
      const bool prod = (0 == strcmp(ped, "produce"));
      if (prod != ESp->meas_prod_dir) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s meldet positiveEnergyDirection '%s'%s"), ESp->peer_ip, ped,
               prod ? " (Norm: consume) - Vorzeichen von Leistung und Strom wird umgekehrt" : "");
      }
      ESp->meas_prod_dir = prod;
    }
  }

   // --- Messwerte: je Kennung den letzten Wert behalten ---------------------------------------
   // v243 (MGCP Z. 424-426, 859-874, [MGCP-003]): valueState "error"/"outOfRange" -> ignorieren; ohne value -> nicht
   // vorhanden; scale nur aus DIESEM Eintrag (fehlt er, gilt 0).
  if (nullptr != strstr(json, "\"measurementData\"")) {
   // Solange der Netzanschlusspunkt nicht erkannt ist, verhaelt sich alles wie bisher (sonst waere
   // die Anzeige vor der ersten Beschreibung leer). Ist er erkannt, gilt er ausschliesslich.
    bool ist_netz = (ESp->meas_ent < 0) || (src_ent == ESp->meas_ent);
    q = json;
    while (nullptr != (q = strstr(q, "\"measurementId\""))) {
      uint32_t id = 0;
      EebusJsonInt(q, "measurementId", &id);
      nx = strstr(q + 15, "\"measurementId\"");
      if (id < 24) {
        char vs[16];
        EebusSegStr(q, nx, "valueState", vs, sizeof(vs));
        const char *vk = EebusSegKey(q, nx, "\"value\"");
        bool gueltig = (nullptr != vk) && (!vs[0] || (0 == strcmp(vs, "normal")));
        long num = 0, sca = 0;
        if (gueltig) {
          const char *ve = strchr(vk, ']');
          if (nx && ve && (ve > nx)) { ve = nx; }
          const char *np = EebusSegKey(vk, ve, "\"number\"");
          if (np && EebusJsonLong(np, "number", &num)) {
            const char *sp = EebusSegKey(vk, ve, "\"scale\"");
            if (sp) { EebusJsonLong(sp, "scale", &sca); }
          } else {
            gueltig = false;
          }
        }
        if (ist_netz) {
          if (ESp->m_scope[id]) {
            int8_t c = ESp->m_scope[id];
            if (gueltig && ESp->meas_prod_dir && ((1 == c) || (4 == c))) { num = -num; }   // v243: Bezug positiv
            ESp->m_val[c]  = num;
            ESp->m_sc[c]   = (int8_t)sca;
            ESp->m_have[c] = gueltig;
            int8_t ph = ESp->m_phase[id];
            if (((4 == c) || (5 == c)) && (ph >= 0) && (ph < 3)) {   // v197: L1-L3
              ESp->m3_val[c - 4][ph] = num; ESp->m3_sc[c - 4][ph] = (int8_t)sca; ESp->m3_have[c - 4][ph] = gueltig;
            } else if ((5 == c) && (ph >= 3) && (ph < 6)) {   // v243: Leiter-Leiter
              ESp->mll_val[ph - 3] = num; ESp->mll_sc[ph - 3] = (int8_t)sca; ESp->mll_have[ph - 3] = gueltig;
            }
          }
        } else {
          int i = EebusMoFind(src_ent, (uint8_t)id, true);
          if (i >= 0) { ESp->mo_v[i] = num; ESp->mo_sc[i] = (int8_t)sca; ESp->mo_have[i] = gueltig; }
        }
      }
      q += 15;
    }
  }

   // --- Nennleistungen aus den Anschluss-Kenngroessen -----------------------------------
   // electricalConnectionCharacteristicListData liefert je Kenngroesse einen characteristicType.
   // Uns interessieren powerProductionNominalMax (Nennleistung Erzeugung — Bezugsgroesse fuer eine
   // Prozent-Vorgabe nach Paragraf 9 EEG) und das Gegenstueck fuer den Bezug. Der Wert steht wie
   // ueberall als scaledNumber (Zahl + Zehnerexponent) daneben.
  if (nullptr != strstr(json, "\"electricalConnectionCharacteristicData\"")) {
    q = json;
    while (nullptr != (q = strstr(q, "\"characteristicType\":\""))) {
      char ct[40] = { 0 };
      EebusJsonStr(q, "characteristicType", ct, sizeof(ct));
      nx = strstr(q + 22, "\"characteristicType\":\"");
      const char *vp = EebusSegKey(q, nx, "\"value\"");   // v243: Leerraum erlaubt, scale nur aus diesem Wert
      const char *ve = vp ? strchr(vp, ']') : nullptr;
      const char *np = vp ? EebusSegKey(vp, ve, "\"number\"") : nullptr;
      if (np) {
        long num = 0, sca = 0;
        EebusJsonLong(np, "number", &num);
        const char *sp = EebusSegKey(vp, ve, "\"scale\"");
        if (sp) { EebusJsonLong(sp, "scale", &sca); }
        char cx[16];   // v243 (LPP Tab. 18/19): nur characteristicContext "entity"
        EebusSegStr(q, nx, "characteristicContext", cx, sizeof(cx));
        if (cx[0] && strcmp(cx, "entity")) {
          // andere Bezugsebene - nicht die Kenngroesse dieser Einheit
        } else if ((0 == strcmp(ct, "powerProductionNominalMax")) &&
                   ESp->pnom_prod_ok && (ESp->pnom_prod_ent == ESp->lpc_peer_ent) && (src_ent != ESp->lpc_peer_ent)) {
          // v243: die Nennleistung der gesteuerten Entity hat Vorrang vor der einer anderen Einheit
        } else if (0 == strcmp(ct, "powerProductionNominalMax")) {
          if (!ESp->pnom_prod_ok || (ESp->pnom_prod != num) || (ESp->pnom_prod_sc != sca)) { ESp->lpc_fs_done = false; }   // v243 [LPP-021]
          ESp->pnom_prod = num; ESp->pnom_prod_sc = (int8_t)sca; ESp->pnom_prod_ok = true;
          ESp->pnom_prod_ent = src_ent;   // festhalten, WOHER sie kam
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Nennleistung Erzeugung von Entity %d: %ld (x10^%ld)"),
                 src_ent, num, sca);
        } else if (0 == strcmp(ct, "contractualProductionNominalMax")) {   // v196: Energiemanager [LPP-042]
          if (!ESp->pnom_cprod_ok || (ESp->pnom_cprod != num) || (ESp->pnom_cprod_sc != sca)) { ESp->lpc_fs_done = false; }   // v197
          ESp->pnom_cprod = num; ESp->pnom_cprod_sc = (int8_t)sca; ESp->pnom_cprod_ok = true;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: vertragliche Nennleistung Erzeugung: %ld (x10^%ld)"), num, sca);
        } else if (0 == strcmp(ct, "contractualConsumptionNominalMax")) {   // v196: Energiemanager [LPC-042]
           // v197: die Anlage ist die Quelle (LPC Z.654-662) — aendert sie den Wert, Failsafe neu schreiben
          if (!ESp->pnom_ccons_ok || (ESp->pnom_ccons != num) || (ESp->pnom_ccons_sc != sca)) {
            ESp->lpc_fs_done = false;
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: vertragliche Hoechstleistung Bezug von %s: %ld (x10^%ld)"), ESp->peer_ip, num, sca);
          }
          ESp->pnom_ccons = num; ESp->pnom_ccons_sc = (int8_t)sca; ESp->pnom_ccons_ok = true;
        } else if (0 == strcmp(ct, "powerConsumptionNominalMax")) {
          ESp->pnom_cons = num; ESp->pnom_cons_sc = (int8_t)sca; ESp->pnom_cons_ok = true;
        }
      }
      q += 22;
    }
  }

   // --- DeviceConfig: Bedeutung der Schluessel je QUELL-ENTITY lernen -------------------------
   // Failsafe-Werte und der PV-Begrenzungsfaktor kommen von VERSCHIEDENEN Entities, fuehren aber
   // beide keyId 0. Ohne die Entity-Zuordnung landete der Prozentwert im Failsafe-Feld.
  if (nullptr != strstr(json, "\"deviceConfigurationKeyValueDescriptionData\"")) {
    int b = -1;
    for (int i = 0; i < 4; i++) { if (ESp->cfg_ent[i] == src_ent) { b = i; break; } }
    if (b < 0) { for (int i = 0; i < 4; i++) { if (ESp->cfg_ent[i] < 0) { ESp->cfg_ent[i] = src_ent; b = i; break; } } }
    if (b >= 0) {
      q = json;
      while (nullptr != (q = strstr(q, "\"keyId\":"))) {
        uint32_t kid = 0;
        EebusJsonInt(q, "keyId", &kid);
        nx = strstr(q + 8, "\"keyId\":");
        const char *kn = strstr(q, "\"keyName\":\"");
        if (kn && (!nx || (kn < nx))) {   // v247: jede keyId (vorher nur 0..7)
          char s[40] = { 0 };
          EebusJsonStr(kn, "keyName", s, sizeof(s));
          int8_t code = 0;
          if      (0 == strcmp(s, "failsafeConsumptionActivePowerLimit")) { code = 1; }
          else if (0 == strcmp(s, "failsafeDurationMinimum"))             { code = 2; }
          else if (0 == strcmp(s, "failsafeProductionActivePowerLimit"))  { code = 3; }
          else if (0 == strcmp(s, "pvCurtailmentLimitFactor"))            { code = 4; }
          if (4 == code) {   // v243 (MGCP Tab. 14): Einheit pct, Wert als scaledNumber
            char vt[16], un[8];
            EebusSegStr(q, nx, "valueType", vt, sizeof(vt));
            EebusSegStr(q, nx, "unit", un, sizeof(un));
            if (strcmp(vt, "scaledNumber") || strcmp(un, "pct")) {
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: pvCurtailmentLimitFactor von %s mit valueType '%s' / unit '%s' (MGCP Tab. 14: scaledNumber/pct) - ignoriert"),
                     ESp->peer_ip, vt, un);
              code = 0;
            }
          }
           // v247: Paar keyId -> Bedeutung
          int p = -1;
          for (int j = 0; j < ESp->cfg_n[b]; j++) { if (ESp->cfg_kid[b][j] == kid) { p = j; break; } }
          if ((p < 0) && (ESp->cfg_n[b] < 8)) { p = ESp->cfg_n[b]++; ESp->cfg_kid[b][p] = kid; }
          if (p >= 0) { ESp->cfg_code[b][p] = code; }
          else if (code) {
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: DeviceConfig von %s: keyId %u (%s) - Liste voll (8), nicht gemerkt"),
                   ESp->peer_ip, kid, s);
          }
        }
        q += 8;
      }
    }
  }

   // --- DeviceConfig: Werte einsortieren ------------------------------------------------------
  if (nullptr != strstr(json, "\"deviceConfigurationKeyValueData\"")) {
    int b = -1;
    for (int i = 0; i < 4; i++) { if (ESp->cfg_ent[i] == src_ent) { b = i; break; } }
    if (b >= 0) {
      q = json;
      while (nullptr != (q = strstr(q, "\"keyId\":"))) {
        uint32_t kid = 0;
        EebusJsonInt(q, "keyId", &kid);
        nx = strstr(q + 8, "\"keyId\":");
        int8_t code = 0;   // v247: Bedeutung aus der Paar-Liste
        for (int j = 0; j < ESp->cfg_n[b]; j++) { if (ESp->cfg_kid[b][j] == kid) { code = ESp->cfg_code[b][j]; break; } }
        if (code) {
          if ((code >= 1) && (code <= 3)) {   // v243 (LPP/LPC Tab. 15/16): isValueChangeable merken
            if (EebusSegFalse(q, nx, "isValueChangeable")) { ESp->fs_ro |= (uint8_t)(1 << code); }
            else { ESp->fs_ro &= (uint8_t)~(1 << code); }
          }
          const char *vp = EebusSegKey(q, nx, "\"scaledNumber\"");   // v243: Leerraum erlaubt, scale nur aus diesem Wert
          const char *ve = vp ? strchr(vp, ']') : nullptr;
          const char *np = vp ? EebusSegKey(vp, ve, "\"number\"") : nullptr;
          const char *dp = EebusSegKey(q, nx, "\"duration\"");
          if ((2 == code) && dp) {
            EebusJsonStr(dp, "duration", ESp->fs_dur, sizeof(ESp->fs_dur));
          } else if (np) {
            long num = 0, sca = 0;
            EebusJsonLong(np, "number", &num);
            const char *sp = EebusSegKey(vp, ve, "\"scale\"");
            if (sp) { EebusJsonLong(sp, "scale", &sca); }
            if      (1 == code) { ESp->fs_cons = num; ESp->fs_cons_sc = (int8_t)sca; ESp->fs_cons_ok = true; }
            else if (3 == code) { ESp->fs_prod = num; ESp->fs_prod_sc = (int8_t)sca; ESp->fs_prod_ok = true; }
            else if (4 == code) {   // v243 (MGCP Tab. 14): 0-100 %
              double f = (double)num;
              for (long k = sca; k > 0; k--) { f *= 10.0; }
              for (long k = sca; k < 0; k++) { f /= 10.0; }
              ESp->plf = num; ESp->plf_sc = (int8_t)sca; ESp->plf_ok = (f >= 0.0) && (f <= 100.0);
            }
          }
        }
        q += 8;
      }
    }
  }

   // v194: Richtung je limitId aus JEDER Limit-Beschreibung lernen (Antwort oder Meldung), nicht nur im
   // Schreibauftrag — die Anzeige unten braucht sie auch, wenn wir noch nie geschrieben haben.
  if (nullptr != strstr(json, "\"loadControlLimitDescriptionData\"")) {
    int o = -1;
    if (EebusLpcPickLimitId(json, 0, &o)) { ESp->lpc_lid_dir[0] = o; }
    o = -1;
    if (EebusLpcPickLimitId(json, 1, &o)) { ESp->lpc_lid_dir[1] = o; }
  }
   // --- Ist-Zustand der beiden Grenzen des Peers ----------------------------------------------
   // v194: Platz d = RICHTUNG (0 Bezug/§14a, 1 Einspeisung/§9) aus der Beschreibung, nicht die limitId-Nummer.
   // Nur solange keine Richtung bekannt ist, gilt die alte Annahme limitId 0/1 = Bezug/Einspeisung.
  if (nullptr != strstr(json, "\"loadControlLimitData\"")) {
    q = json;
    while (nullptr != (q = strstr(q, "\"limitId\":"))) {
      uint32_t lid = 0;
      EebusJsonInt(q, "limitId", &lid);
      nx = strstr(q + 10, "\"limitId\":");
      int d = -1;
      if      ((int)lid == ESp->lpc_lid_dir[0]) { d = 0; }
      else if ((int)lid == ESp->lpc_lid_dir[1]) { d = 1; }
      else if ((ESp->lpc_lid_dir[0] < 0) && (ESp->lpc_lid_dir[1] < 0) && (lid < 2)) { d = (int)lid; }
      if (d >= 0) {
        const char *ap = strstr(q, "\"isLimitActive\":");
        if (ap && (!nx || (ap < nx))) {
          const int8_t vorher = ESp->lim_act[d];   // v236
          ESp->lim_act[d] = (0 == strncmp(ap + 16, "true", 4)) ? 1 : 0;
   // v236 (22.11.2026, Test 4 HEMS v031): ABBRUCH DURCH DIE ANLAGE erkennen. [LPC-923]: im Zustand "limited" darf die
   // Anlage die Grenze selbst aufheben (beim CEM u. a. wenn nicht steuerbare Lasten das Einhalten verhindern); sie meldet
   // das per notify (LPC IG 2.4 Z. 243-257). isLimitActive ist "\RW" -> empfangene Notifys MUESSEN ausgewertet werden
   // (LPC TS 3.1.3.5 Z. 895-897). Bis v235 wurde die Notify nur mitgelesen: die Box zeigte weiter "BESTAETIGT aktiv",
   // im Betriebslogbuch stand eine Zusage, die nicht mehr galt (Nachweis BK6-22-300 Anl. 1 Ziff. 7.2-7.4).
   // Bedingungen: notify, Wechsel aktiv -> inaktiv, die Box fuehrt fuer diese Richtung eine eigene, noch gueltige Grenze
   // (bei einer eigenen Freigabe ist der Merker schon geloescht - kein Fehlalarm) und die Schreibkette ruht.
   // Reaktion: Logbuch + Ergebnis, dann die vorhandene Wiederholung (wie nach einer Ablehnung, v214): nach
   // EEBUS_ABGELEHNT_WDH_MS die eigene Grenze mit Restdauer erneut, abgelaufen -> Freigabe (LPC IG 2.5 Z. 264-286:
   // Anlage nicht aus der Liste nehmen, Befehl nach angemessener Zeit wiederholen).
          uint32_t own_w_ab = 0; long own_rest_ab = 0;
          if ((1 == vorher) && (0 == ESp->lim_act[d]) &&
              ((nullptr != strstr(json, "\"cmdClassifier\":\"notify\"")) ||
               (nullptr != strstr(json, "\"cmdClassifier\":\"reply\""))) &&   // v239: auch eine Antwort (z. B. Nachlesen)
              ((LPC_DONE == ESp->lpc_state) || (LPC_IDLE == ESp->lpc_state) || (LPC_FAIL == ESp->lpc_state)) &&
              ESp->best_akt[d] &&   // v237: nur eine BESTAETIGTE Grenze kann abgebrochen werden (nicht das Hin und Her bei einem NACK)
              EebusEigeneGrenze((uint8_t)d, &own_w_ab, &own_rest_ab)) {
            ESp->best_akt[d] = false;
            char wtxt[16]; snprintf(wtxt, sizeof(wtxt), "%u W", own_w_ab);
            char etxt[80]; snprintf(etxt, sizeof(etxt), "Grenze von der Anlage aufgehoben - erneuter Versuch in %u s",
                                    EebusWdhMs() / 1000);
            EebusLbLog(d ? "Abbruch durch Anlage (LPP-923)" : "Abbruch durch Anlage (LPC-923)", ESp->peer_ski, ESp->peer_ip, d, wtxt, own_rest_ab, etxt);
            strlcpy(ESp->lpc_result, d ? "ABBRUCH durch Anlage (LPP-923)" : "ABBRUCH durch Anlage (LPC-923)", sizeof(ESp->lpc_result));
            strlcpy(ESp->lb_last, "", sizeof(ESp->lb_last));   // die naechste gleiche Vorgabe wieder ins Logbuch
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** %s hat die %s-Grenze %s selbst AUFGEHOBEN (%s-923) ***"),
                   ESp->peer_ip, d ? "Einspeise" : "Bezugs", wtxt, d ? "LPP" : "LPC");
            ESp->sess_vorgabe = true;   // sonst greift EebusVorgabeWiederholen nicht (die Vorgabe dieser Sitzung gilt nicht mehr)
            EebusVorgabeWiederholen("von der Anlage abgebrochen");
          }
   // VERSPAETETE BESTAETIGUNG. Der Read-Back pollt nur wenige Sekunden nach; antwortet
   // die Gegenstelle spaeter (oder meldet sie den Zustand von sich aus per notify), stand das
   // Ergebnis frueher fuer immer auf "Fehler" — obwohl die Begrenzung laengst wirkte. Genau
   // so gesehen . Nur richtigstellen, wenn der Fehlschlag aus der
   // Ruecklesung kam (lpc_verify_failed) — eine echte Ablehnung der Gegenstelle bleibt stehen.
          if (ESp->lpc_verify_failed && (LPC_FAIL == ESp->lpc_state) &&
              ((int)lid == ESp->lpc_limit_id) &&
              ((1 == ESp->lim_act[d]) == ESp->lpc_active_wish)) {
            ESp->lpc_verify_failed = false;
            ESp->lpc_our_limit = ESp->lpc_active_wish;
            ESp->sess_vorgabe = true;   // v225: doch bestaetigt, die Wiederholung entfaellt
            ESp->frei_offen[d] = false;
            char t[56];
            snprintf(t, sizeof(t), ESp->lpc_active_wish ? "BESTAETIGT aktiv %u W (verspaetet)"
                                                        : "BESTAETIGT freigegeben (verspaetet)",
                     ESp->lpc_value);
            EebusLpcSetResult(LPC_DONE, t);
          }
        }
        const char *vp = strstr(q, "\"value\":[{\"number\":");
   // v179: nach err7 nachgelesen — umgesetzt, wenn Zustand UND (bei aktiv) Wert dem Auftrag entsprechen.
   // Der Wert wird geprueft, damit eine noch stehende alte Grenze nicht als Bestaetigung durchgeht.
        if (ESp->lpc_rej_late && (LPC_FAIL == ESp->lpc_state) && ((int)lid == ESp->lpc_limit_id) &&
            ap && (!nx || (ap < nx)) && ((1 == ESp->lim_act[d]) == ESp->lpc_active_wish)) {
          bool wert_ok = !ESp->lpc_active_wish;
          if (!wert_ok && vp && (!nx || (vp < nx))) {
            long num = 0, sca = 0;
            EebusJsonLong(vp, "number", &num);
            EebusJsonLong(vp, "scale",  &sca);
            if ((sca >= -9) && (sca <= 9)) {   // v231: scale aus dem Frame begrenzen (sonst Schleife ohne Ende / Ueberlauf)
              int64_t w = num;
              for (; sca > 0; sca--) { w *= 10; }
              for (; sca < 0; sca++) { w /= 10; }
              wert_ok = (llabs(w) == (int64_t)ESp->lpc_value);
            }
          }
          if (wert_ok) {
            ESp->lpc_rej_late = false; ESp->late_rd_at = 0;
            ESp->lpc_our_limit = ESp->lpc_active_wish;
            ESp->frei_offen[d] = false;   // v225
            char t[56];
            snprintf(t, sizeof(t), ESp->lpc_active_wish ? "BESTAETIGT aktiv %u W (verspaetet nach err7)"
                                                        : "BESTAETIGT frei (verspaetet nach err7)",
                     ESp->lpc_value);
            EebusLpcSetResult(LPC_DONE, t);
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC @ %s: %s"), ESp->peer_ip, t);
          }
        }
        if (vp && (!nx || (vp < nx))) {
          long num = 0, sca = 0;
          EebusJsonLong(vp, "number", &num);
          EebusJsonLong(vp, "scale",  &sca);
          ESp->lim_val[d] = num;
          ESp->lim_sc[d]  = (int8_t)sca;
        }
   // Geltungsdauer mitlesen. Steht KEINE im Eintrag, wird der gemerkte Wert geloescht —
   // sonst zeigte die Anzeige noch eine Dauer, die der Peer laengst nicht mehr fuehrt.
        const char *ep = strstr(q, "\"endTime\":\"");
        if (ep && (!nx || (ep < nx))) {
          EebusJsonStr(ep, "endTime", ESp->lim_dur[d], sizeof(ESp->lim_dur[d]));
   // Restlaufzeit in Sekunden merken + Empfangszeitpunkt stempeln -> die Anzeige
   // rechnet ab hier selbst weiter, statt den Text einzufrieren. Laesst sich die Dauer nicht
   // deuten (unbekannte Schreibweise), bleibt -1 und wir zeigen weiter den Rohtext.
          ESp->lim_dur_s[d]  = EebusIsoDurSecs(ESp->lim_dur[d]);
          ESp->lim_dur_at[d] = millis();
        } else {
          ESp->lim_dur[d][0] = '\0';
        }
      }
      q += 10;
    }
  }
}

// v246 (C2): steht der Schluessel mit dem Wert true im Text? Leerraum um den Doppelpunkt erlaubt.
bool EebusJsonTrue(const char *json, const char *key) {
  char k[40]; snprintf(k, sizeof(k), "\"%s\"", key);
  const size_t kl = strlen(k);
  const char *p = json;
  while (nullptr != (p = strstr(p, k))) {
    const char *q = p + kl;
    while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
    if (':' == *q) {
      q++;
      while ((' ' == *q) || ('\t' == *q) || ('\r' == *q) || ('\n' == *q)) { q++; }
      return (0 == strncmp(q, "true", 4));
    }
    p = q;
  }
  return false;
}
// v246: Ende des Adress-Blocks hinter dem Schluessel (Klammern zaehlen), damit fehlende Teile nicht aus dem Rest
// des Datagramms gelesen werden (EebusAddrParse sucht "feature" ungebremst weiter).
const char *EebusAddrEnd(const char *frag) {
  const char *p = strchr(frag + 1, '"');   // Ende des Schluessels
  if (nullptr == p) { return frag; }
  p++;
  while ((' ' == *p) || ('\t' == *p) || ('\r' == *p) || ('\n' == *p) || (':' == *p)) { p++; }
  if (('[' != *p) && ('{' != *p)) { return p; }
  int d = 0; bool str = false;
  for (; *p; p++) {
    if (str) { if (('\\' == *p) && p[1]) { p++; } else if ('"' == *p) { str = false; } continue; }
    if ('"' == *p) { str = true; }
    else if (('[' == *p) || ('{' == *p)) { d++; }
    else if ((']' == *p) || ('}' == *p)) { if (--d <= 0) { return p; } }
  }
  return p;
}
// v246: Adress-Block (clientAddress/serverAddress/addressDestination) begrenzt lesen; fehlende Teile = -1 bzw. "".
void EebusAddrSeg(const char *frag, int *ent, int *feat, char *dev = nullptr, size_t devlen = 0) {
  *ent = -1; *feat = -1;
  if (dev && devlen) { dev[0] = '\0'; }
  if (nullptr == frag) { return; }
  const char *end = EebusAddrEnd(frag);
  const char *p = EebusSegKey(frag, end, "\"entity\"");
  if (p) { const char *b = strchr(p + 8, '['); if (b && (b < end)) { *ent = atoi(b + 1); } }
  p = EebusSegKey(frag, end, "\"feature\"");
  if (p) { uint32_t f = 0; if (EebusJsonInt(p, "feature", &f)) { *feat = (int)f; } }
  if (dev && devlen) { p = EebusSegKey(frag, end, "\"device\""); if (p) { EebusJsonStr(p, "device", dev, devlen); } }
}
// v246 (SPINE TS SUBS_001/002, BIND_001): Abos und Bindings, die die Gegenstelle bei UNS angelegt hat (wir = Server).
void EebusGbAdd(uint8_t typ, int ce, int cf, int se, int sf) {
  for (uint8_t i = 0; i < ESp->gb_n; i++) {
    if ((ESp->gb_typ[i] == typ) && (ESp->gb_ce[i] == ce) && (ESp->gb_cf[i] == cf) &&
        (ESp->gb_se[i] == se) && (ESp->gb_sf[i] == sf)) { return; }
  }
  if (ESp->gb_n >= EEBUS_GB_MAX) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s: Liste der %s voll (%u) - Eintrag nicht gemerkt"),
           ESp->peer_ip, typ ? "Bindings" : "Abos", (uint32_t)EEBUS_GB_MAX);
    return;
  }
  const uint8_t i = ESp->gb_n++;
  ESp->gb_typ[i] = typ; ESp->gb_ce[i] = ce; ESp->gb_cf[i] = cf; ESp->gb_se[i] = se; ESp->gb_sf[i] = sf;
}
// Loeschen: ein nicht angegebener Teil (-1) passt auf alles (SubscriptionDelete/BindingDelete ohne Feature = alle).
uint8_t EebusGbDel(uint8_t typ, int ce, int cf, int se, int sf) {
  uint8_t n = 0, j = 0;
  for (uint8_t i = 0; i < ESp->gb_n; i++) {
    const bool weg = (ESp->gb_typ[i] == typ) && ((ce < 0) || (ESp->gb_ce[i] == ce)) && ((cf < 0) || (ESp->gb_cf[i] == cf)) &&
                     ((se < 0) || (ESp->gb_se[i] == se)) && ((sf < 0) || (ESp->gb_sf[i] == sf));
    if (weg) { n++; continue; }
    ESp->gb_typ[j] = ESp->gb_typ[i]; ESp->gb_ce[j] = ESp->gb_ce[i]; ESp->gb_cf[j] = ESp->gb_cf[i];
    ESp->gb_se[j] = ESp->gb_se[i]; ESp->gb_sf[j] = ESp->gb_sf[i];
    j++;
  }
  ESp->gb_n = j;
  return n;
}
void EebusGbAdr(char *o, size_t n, const char *dev, int e, int f) {
  size_t l = 0; o[0] = '\0';
  if (dev && dev[0]) { l += snprintf(o + l, n - l, "{\"device\":\"%s\"}", dev); }
  if ((e >= 0) && (l < n)) { l += snprintf(o + l, n - l, "%s{\"entity\":[%d]}", l ? "," : "", e); }
  if ((f >= 0) && (l < n)) { snprintf(o + l, n - l, "%s{\"feature\":%d}", l ? "," : "", f); }
}
// Liste ausliefern: nodeManagementSubscriptionData (typ 0) bzw. nodeManagementBindingData (typ 1).
void EebusGbAntwort(uint8_t typ, uint32_t ref, int peer_ent, int peer_feat) {
  char own[80]; EebusOwnDevice(own, sizeof(own));
  const size_t cap = 128 + (size_t)ESp->gb_n * (200 + strlen(own) + strlen(ESp->peer_dev));
  char *cmd = (char*)special_malloc(cap);
  if (nullptr == cmd) { return; }
  const char *nm = typ ? "nodeManagementBindingData" : "nodeManagementSubscriptionData";
  size_t l = snprintf(cmd, cap, "{\"%s\":[{\"%s\":[", nm, typ ? "bindingEntry" : "subscriptionEntry");
  uint8_t k = 0;
  char ca[160], sa[160];
  for (uint8_t i = 0; (i < ESp->gb_n) && (l < cap); i++) {
    if (ESp->gb_typ[i] != typ) { continue; }
    EebusGbAdr(ca, sizeof(ca), ESp->peer_dev, ESp->gb_ce[i], ESp->gb_cf[i]);
    EebusGbAdr(sa, sizeof(sa), own, ESp->gb_se[i], ESp->gb_sf[i]);
    l += snprintf(cmd + l, cap - l, "%s[{\"%s\":%u},{\"clientAddress\":[%s]},{\"serverAddress\":[%s]}]",
                  k ? "," : "", typ ? "bindingId" : "subscriptionId", (uint32_t)k, ca, sa);
    k++;
  }
  if (l < cap) { snprintf(cmd + l, cap - l, "]}]}"); }
  EebusSpineSendAddr("reply", true, ref, cmd, 0, 0, peer_ent, peer_feat);
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s an %s beantwortet (%u Eintraege)"), nm, ESp->peer_ip, (uint32_t)k);
  free(cmd);
}
// v246 (B2): Fehler 9 "Binding noetig" auf einen Write -> unser Binding gilt als weg, der naechste Write bindet neu.
void EebusErr9Pruefen(const char *json) {
  uint32_t e = 0;
  if (EebusJsonInt(json, "errorNumber", &e) && (9 == e) && ESp->lpc_bound) {
    ESp->lpc_bound = false;
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s meldet Fehler 9 (Binding noetig) - Binding wird beim naechsten Write neu angelegt"), ESp->peer_ip);
  }
}

void EebusSpineHandle(const char *json) {
  ESp->last_rx = millis();
   // Peer-Geraeteadresse aus addressSource merken (fuer unsere addressDestination)
  if (!ESp->peer_dev[0]) {
    const char *as = strstr(json, "\"addressSource\"");
    if (as) { EebusJsonStr(as, "device", ESp->peer_dev, sizeof(ESp->peer_dev)); }
  }
   // Sobald die Peer-Geraeteadresse bekannt ist, SEINE Detailed Discovery ADRESSIERT abfragen
   // (bei jedem Frame erneut versucht bis es klappt, peer_disco_read-Guard). Wie Ladestation
   // initial_peer_discovery, das bei jedem process_datagram feuert. Energiemanager-Fix: der fruehere
   // Aufruf bei SME_DONE lief mit leerer peer_dev ins Leere -> Energiemanager/Waermepumpen-Gateway antworteten nie.
  EebusSpineReadPeerDiscovery();
  char cls[12] = { 0 };
  EebusJsonStr(json, "cmdClassifier", cls, sizeof(cls));
  uint32_t their_ctr = 0;
  EebusJsonInt(json, "msgCounter", &their_ctr);
  bool ack = EebusJsonTrue(json, "ackRequest");   // v246 (C2): auch mit Leerraum
  if (0 == strcmp(cls, "result")) { EebusAboErgebnis(json); }   // v241

   // Adressen des Datagramms: von welchem Peer-Feature kam es, welches UNSERER Features ist gemeint?
  int peer_ent = 0, peer_feat = 0, our_ent = 0, our_feat = 0;
  EebusAddrParse(strstr(json, "\"addressSource\""), &peer_ent, &peer_feat);
  EebusAddrParse(strstr(json, "\"addressDestination\""), &our_ent, &our_feat);
   // v246 (SPINE TS FC_001): NodeManagement-Befehle nur an Entity 0 / Feature 0 (fehlende Angabe = 0)
  int fc_e = -1, fc_f = -1;
  EebusAddrSeg(strstr(json, "\"addressDestination\""), &fc_e, &fc_f);
  const bool nm_ziel = (fc_e <= 0) && (fc_f <= 0);

   // Nutzdaten mitlesen und behalten (Messwerte, Failsafe-Werte, Limit-Zustaende) — unabhaengig
   // von der Schreibkette, damit die Bedienoberflaeche etwas anzuzeigen hat. Bisher lief das alles
   // durch und wurde verworfen, obwohl der Peer die Messwerte im Sekundentakt schickt.
  EebusHarvest(json, peer_ent);

  if (0 == strcmp(cls, "read")) {
    if (!nm_ziel && strstr(json, "\"nodeManagement")) {   // v246 (TS FC_001)
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s liest NodeManagement an ent%d/feat%d statt 0/0 - abgelehnt (Fehler 6, SPINE TS FC_001)"),
             ESp->peer_ip, fc_e, fc_f);
      EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":6}]}", our_ent, our_feat, peer_ent, peer_feat);
    } else if (strstr(json, "nodeManagementDetailedDiscoveryData")) {
      EebusSpineAnswerDiscovery(their_ctr, 0);
      ESp->disco_answered = true;
      EebusSpineReadPeerDiscovery();   // Gegenseitigkeit: gleich SEINE Discovery abfragen
    } else if (strstr(json, "nodeManagementDestinationListData")) {
      EebusSpineAnswerDestinationList(their_ctr);   // echte Selbstbeschreibung (nicht resultData) -> strenge Peers (Energiemanager/Waermepumpen-Gateway) onboarden weiter statt abzubrechen
    } else if (strstr(json, "nodeManagementUseCaseData")) {
      EebusSpineAnswerUseCase(their_ctr);   // wir bieten LPC (ControllableSystem) an
    } else if (strstr(json, "nodeManagementBindingData")) {   // v246 (TS BIND_001)
      EebusGbAntwort(1, their_ctr, peer_ent, peer_feat);
    } else if (strstr(json, "nodeManagementSubscriptionData")) {   // v246 (TS SUBS_001/002)
      EebusGbAntwort(0, their_ctr, peer_ent, peer_feat);
    } else if (EebusSpineAnswerFeatureRead(json, their_ctr, our_ent, our_feat, peer_ent, peer_feat)) {
   // deviceDiagnosisState/Heartbeat, deviceClassification, loadControlLimit* beantwortet
    } else {
   // v239 (SPINE TS Z. 1088-1093): unbekanntes read -> IMMER result mit errorNumber != 0 an das Absender-Feature,
   // unabhaengig von ackRequest (bisher nur bei ackRequest und mit errorNumber 0). 6 = Befehl nicht unterstuetzt.
      EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":6}]}", our_ent, our_feat, peer_ent, peer_feat);
    }
  } else if (0 == strcmp(cls, "write")) {
   // Ein Steuergeraet schreibt uns ein LPC-Limit -> DAS ist das §14a-Pruefergebnis: mitschneiden!
    if (strstr(json, "loadControlLimitListData")) {
      const char *v = strstr(json, "\"value\"");
      uint32_t number = 0; bool have = v && EebusJsonInt(v, "number", &number);
      char active[8] = { 0 }; EebusJsonStr(json, "isLimitActive", active, sizeof(active));
      char nbuf[16];
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** LPC-LIMIT vom Steuergeraet %s empfangen: %s W (aktiv=%s) ***"),
             ESp->peer_ip, have ? itoa((int)number, nbuf, 10) : "?", active[0] ? active : "?");
    }
   // Quittung an das ABSENDER-Feature des Peers (nicht pauschal an sein NodeManagement) — s.u.
   // v246 (B5): Absender = das beschriebene Feature (bisher fest 0/0)
    if (ack) { EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":0}]}", our_ent, our_feat, peer_ent, peer_feat); }
  } else if (0 == strcmp(cls, "call")) {
   // v246 (SPINE TS FC_001, BIND_001, SUBS_001/002): Call genau pruefen. Ablehnungen gehen IMMER raus (wie v239 bei read).
    const bool c_sreq = (nullptr != strstr(json, "SubscriptionRequestCall"));
    const bool c_sdel = (nullptr != strstr(json, "SubscriptionDeleteCall"));
    const bool c_breq = (nullptr != strstr(json, "BindingRequestCall"));
    const bool c_bdel = (nullptr != strstr(json, "BindingDeleteCall"));
    int ce = -1, cf = -1, se = -1, sf = -1;
    char cdev[96], sdev[96], ftype[32] = { 0 };
    EebusAddrSeg(strstr(json, "\"clientAddress\""), &ce, &cf, cdev, sizeof(cdev));
    EebusAddrSeg(strstr(json, "\"serverAddress\""), &se, &sf, sdev, sizeof(sdev));
    EebusJsonStr(json, "serverFeatureType", ftype, sizeof(ftype));   // C2: Leerraum erlaubt
    uint32_t fehler = 0;
    const char *grund = "";
    if (!(c_sreq || c_sdel || c_breq || c_bdel)) { fehler = 6; grund = "unbekannter Call"; }
    else if (!nm_ziel) { fehler = 6; grund = "Ziel ist nicht NodeManagement 0/0 (SPINE TS FC_001)"; }
    else if (c_breq && ((((se <= 0) && (sf <= 0))) || (0 == strcmp(ftype, "NodeManagement")))) {
      fehler = 7; grund = "Binding auf NodeManagement nicht erlaubt (SPINE TS BIND_001)";
    }
    if (fehler) {
      char r[48]; snprintf(r, sizeof(r), "{\"resultData\":[{\"errorNumber\":%u}]}", fehler);
      EebusSpineSendAddr("result", true, their_ctr, r, our_ent, our_feat, peer_ent, peer_feat);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Call von %s abgelehnt (Fehler %u): %s"), ESp->peer_ip, fehler, grund);
    }
    const int hbf = (1 == eebus_role) ? 5 : 2;    // unser DeviceDiagnosis-Server (Entity 1) = Herzschlag-Quelle
    const int lcf = (1 == eebus_role) ? -2 : 6;   // unser LoadControl-Server (Entity 1); Rolle 1 hat keinen
   // v239 (LPC TS Z. 1475-1477) + v246 (B1): Abo geloescht -> Liste bereinigen, betroffene Merker zuruecksetzen.
   // Fehlt Entity/Feature im Loeschauftrag, gilt er fuer alle passenden Abos.
    if (!fehler && c_sdel) {
      const uint8_t n = EebusGbDel(0, ce, cf, se, sf);
   // Merker nur zuruecksetzen, wenn der Loeschauftrag sie trifft (nicht aus der Liste ableiten: ein Abo ohne
   // Adressangabe steht dort als 0/0 und wuerde sonst bei jedem fremden Loeschen den Herzschlag abschalten).
      const bool tr_hb = ((se < 0) || (1 == se)) && ((sf < 0) || (hbf == sf));
      const bool tr_lc = ((se < 0) || (1 == se)) && ((sf < 0) || (lcf == sf));
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s loescht Abo (Server ent%d/feat%d, -1 = alle): %u Eintrag/Eintraege entfernt"),
             ESp->peer_ip, se, sf, (uint32_t)n);
      if (tr_hb && ESp->hb_sub) { ESp->hb_sub = false; AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s hat das Abo auf unseren Herzschlag geloescht"), ESp->peer_ip); }
      if (tr_lc && ESp->lc_sub) { ESp->lc_sub = false; AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s hat das Abo auf unser LoadControl geloescht"), ESp->peer_ip); }
      if ((se <= 0) && (sf <= 0)) { ESp->peer_subscribed = false; }
    }
   // v246 (B2): Binding geloescht. Betrifft es UNSER Binding auf die LoadControl der Anlage, beim naechsten Write neu binden.
    if (!fehler && c_bdel) {
      char own[80]; EebusOwnDevice(own, sizeof(own));
      const bool unser = sdev[0] ? (0 == strcmp(sdev, ESp->peer_dev)) : (cdev[0] && (0 == strcmp(cdev, own)));
      if (unser) {
        if (ESp->lpc_bound && ((sf < 0) || ((se == ESp->lpc_peer_ent) && (sf == ESp->lpc_peer_feat)))) {
          ESp->lpc_bound = false;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s hat unser LoadControl-Binding geloescht - beim naechsten Write neu binden"), ESp->peer_ip);
          EebusLbLog("Binding geloescht", ESp->peer_ski, ESp->peer_ip, -1, "", 0, "Anlage hat das LoadControl-Binding geloescht - naechster Write bindet neu");
        }
      } else {
        const uint8_t n = EebusGbDel(1, ce, cf, se, sf);
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s loescht Binding bei uns (Server ent%d/feat%d): %u entfernt"), ESp->peer_ip, se, sf, (uint32_t)n);
      }
    }
    if (!fehler && c_breq) { EebusGbAdd(1, ce, cf, se, sf); }   // v246: Binding auf ein Feature von uns merken
   // z.B. nodeManagementSubscriptionRequestCall: der Peer abonniert uns -> quittieren + merken
    bool nm_sub = false;   // frisches Abo auf UNSER NodeManagement?
    if (!fehler && c_sreq) {
   // v246 (B3): Ziel-Feature auch aus serverAddress - serverFeatureType ist nur empfohlen
      const bool z_nm = (0 == strcmp(ftype, "NodeManagement")) || (!ftype[0] && (se <= 0) && (sf <= 0));
      const bool z_dd = (0 == strcmp(ftype, "DeviceDiagnosis")) || ((1 == se) && (hbf == sf));
      const bool z_lc = (0 == strcmp(ftype, "LoadControl")) || ((1 == se) && (lcf == sf));
      EebusGbAdd(0, ce, cf, (se < 0) ? 0 : se, (sf < 0) ? 0 : sf);
      nm_sub = !ESp->peer_subscribed && z_nm;
      ESp->peer_subscribed = true;
   // Abo auf UNSER DeviceDiagnosis (ent1/feat2)? -> Heartbeat-NOTIFY an den Abonnenten (HEMS 4 s, SteuVE 20 s)
      if (z_dd) {   // v246 (B3/C2)
        ESp->hb_cli_ent = 0; ESp->hb_cli_feat = 0;
        EebusAddrParse(strstr(json, "\"clientAddress\""), &ESp->hb_cli_ent, &ESp->hb_cli_feat);
        ESp->hb_sub = true;
        ESp->hb_next = Eebus126At(2000);   // ersten Heartbeat zeitnah senden
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer %s abonniert unser DeviceDiagnosis -> Heartbeat alle %u s (an ent %d feat %d)"),   // v231: Text stimmte nicht (30 s)
               ESp->peer_ip, (uint32_t)((eebus_hems_mode ? EEBUS_HEMS_HB_MS : 20000) / 1000), ESp->hb_cli_ent, ESp->hb_cli_feat);
        EebusServeStateNotify();   // derzeit ohne Wirkung (leer); v231: die Logzeile "State gesendet" entfaellt, es wurde nichts gesendet
      }
   // Abo auf UNSER LoadControl (ent1/feat6)? -> HEMS liest hier unser §14a-Limit.
   // Absenderadresse merken; sofort den aktuellen Limit-Zustand als NOTIFY nachschieben.
      if (z_lc) {   // v246 (B3/C2)
        ESp->lc_cli_ent = 0; ESp->lc_cli_feat = 0;
        EebusAddrParse(strstr(json, "\"clientAddress\""), &ESp->lc_cli_ent, &ESp->lc_cli_feat);
        ESp->lc_sub = true;
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** Peer %s abonniert unser LoadControl (HEMS-Modell!) -> Limit wird bereitgestellt (an ent %d feat %d) ***"),
               ESp->peer_ip, ESp->lc_cli_ent, ESp->lc_cli_feat);
        EebusServeLimitNotify();   // aktuellen Zustand sofort schicken
      }
    }
   // (Voll-Frame-Diff, Vergleichs-Steuerbox err0 vs wir err7): Die Quittung MUSS an das Feature zurueck,
   // das den Call gesendet hat (addressSource), nicht pauschal an entity[0]/feature 0.
   // BEWEIS aus dem Mitschnitt: der Peer abonniert unseren Heartbeat mit einem Call AUS seinem
   // DeviceDiagnosis-Client (z.B. ent7/feat1001) an unser NodeManagement. Die Referenz-Steuerbox
   // quittiert an ent7/feat1001 zurueck (und der Partner fragt danach weiter, z.B. den State ab);
   // wir quittierten an ent0/feat0 -> aus Sicht des strengen Partners ist SEIN Abo auf unseren
   // Heartbeat nie bestaetigt. Genau das erklaert, warum alle UNSERE Anfragen errorNumber 0 bekommen,
   // der Limit-Write aber abgelehnt wird: ohne bestaetigte Heartbeat-Ueberwachung des Energy Guard
   // nimmt ein normkonformes ControllableSystem kein Limit an.
   // Unsere replies waren schon korrekt adressiert (EebusSpineAnswerFeatureRead bekommt peer_ent/feat) —
   // nur die result-Quittung auf Calls war fest verdrahtet.
    if (!fehler && ack) { EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":0}]}", our_ent, our_feat, peer_ent, peer_feat); }   // v246: Ablehnung schon oben
    if (nm_sub && !eebus_hems_mode) {
   // wie Referenz-Umsetzung dem frischen NodeManagement-Abonnenten unsere Entity-Liste aktiv
   // als NOTIFY liefern — alte, abo-orientierte Stacks (Waermepumpen-Hersteller) warten evtl. genau darauf.
   // NUR fuer SteuVE (Waermepumpen-Gateway). Der Energiemanager abonniert unser NodeManagement, aber die
   // Vergleichs-Steuerbox sendet ihre DetailedDiscovery NIE als NOTIFY (0x im Mitschnitt) — nur als
   // reply auf Reads. Unser unsolicited Discovery-NOTIFY loest beim Energiemanager processNotifyDetailedDiscoveryData
   // (Re-Evaluierung) aus; die Vergleichs-Steuerbox vermeidet genau das. Redundant (der Energiemanager liest unsere Discovery ohnehin
   // per Read->Reply), daher im HEMS-Pfad weglassen = byte-gleich zur Vergleichs-Steuerbox. Einziger bestaetigter
   // Verhaltens-Unterschied im Onboarding-Ablauf (voller Vergleichs-Steuerbox-Mitschnitt-Vergleich).
      EebusSpineAnswerDiscovery(0, 1);
    }
  } else if (0 == strcmp(cls, "reply")) {
   // Wartet ein gezieltes EEBusRead auf genau diese Adresse? Dann die Rohantwort aufheben —
   // VOR allen anderen Zweigen, damit auch Antworten mitgenommen werden, die sonst niemand
   // auswertet (genau die sind beim Fehlersuchen die interessanten).
    if ((ESp->rd_ent >= 0) && (peer_ent == ESp->rd_ent) && (peer_feat == ESp->rd_feat)) {
      if (ESp->rd_buf) { free(ESp->rd_buf); ESp->rd_buf = nullptr; }
      size_t rl = strlen(json);
      char *rb = (char*)special_malloc(rl + 1);
      if (rb) { memcpy(rb, json, rl + 1); ESp->rd_buf = rb; }
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Antwort von ent%d/feat%d aufgehoben (%u B)"),
             peer_ent, peer_feat, (unsigned)rl);
    }
   // Antwort auf UNSERE Reads. Auf die Detailed Discovery des Peers folgt das Lesen seiner
   // Use-Case-Daten (vervollstaendigt die Peer-Discovery -> Peer haelt die Verbindung).
    if (strstr(json, "nodeManagementDetailedDiscoveryData")) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** PEER-DISCOVERY-ANTWORT erhalten von %s -- Peer-Struktur bekannt! ***"), ESp->peer_ip);
      bool learned = EebusLpcLearnTarget(json);   // LoadControl-Ziel (ent/feat) automatisch aus der Discovery lernen
   // Discovery kopieren (PSRAM) fuer discovery-getriebenes Onboarding (EebusLpcSubscribeAll).
      if (ESp->lpc_disco) { free(ESp->lpc_disco); ESp->lpc_disco = nullptr; }
      { size_t dl = strlen(json); char *dc = (char*)special_malloc(dl + 1);
        if (dc) { memcpy(dc, json, dl + 1); ESp->lpc_disco = dc; } }
      EebusLernMrd(ESp->lpc_disco);   // v241
      EebusModeApply(ESp->peer_ski);   // v209: Betriebsart jetzt auch aus der Discovery (ohne Fundliste)
      EebusSpineSend("read", false, 0, "{\"nodeManagementUseCaseData\":[]}");   // [] statt {} (EEBUS-Array-Form, Energiemanager-strikt), konsistent mit Discovery-Read
   // LoadControl-Ziel bekannt -> im HEMS-Modus SOFORT onboarden (Bind/ReadDesc/ReadData/
   // ReadCfg OHNE Write). Beziehung reift + Heartbeat laeuft, bevor EEBusLpc spaeter den §14a-Write gegen die
   // gecachten Daten absetzt. Idempotent (Guard in EebusLpcStartOnboard). Modus PRO SKI aufloesen (wie
   // EebusLpcStart): ein als SteuVE zugeordneter Peer (Ladestation/Waermepumpen-Gateway) wird NICHT onboardet -> bleibt write-getrieben.
      if (learned) {
        int pm = EebusModeEffective(ESp->peer_ski);   // Zuordnung ODER Geraetetyp
        bool hems = (pm >= 0) ? (0 != pm) : (0 != eebus_hems_mode);
        if (hems) { EebusLpcStartOnboard(); }
      }
    }
   // Use-Case-Auskunft des Peers aufheben (Actors + Use Cases fuer die Bedienoberflaeche).
   // Nur die ANTWORT des Peers, nicht unsere eigene Deklaration — deshalb hier im reply-Zweig und
   // mit Pruefung auf useCaseInformation; ein leeres nodeManagementUseCaseData wird nicht gemerkt.
    else if (strstr(json, "\"useCaseInformation\"")) {
      if (ESp->lpc_uc) { free(ESp->lpc_uc); ESp->lpc_uc = nullptr; }
      size_t ul = strlen(json);
      char *uc = (char*)special_malloc(ul + 1);
      if (uc) {
        memcpy(uc, json, ul + 1); ESp->lpc_uc = uc;
        EebusUcAuswerten(uc);   // v243
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Use-Case-Auskunft des Peers gemerkt (%u B)"), (unsigned)ul);
      }
    }
   // LPC-Controller: Antwort auf unsere Limit-Beschreibung -> limitId der consume-Grenze
    else if ((LPC_READDESC == ESp->lpc_state) && strstr(json, "loadControlLimitDescriptionData")) {
      int lid = -1;
      if (EebusLpcPickLimitId(json, ESp->lpc_dir, &lid)) {   // /Grenze der aktuellen Richtung waehlen
        ESp->lpc_limit_id = lid;
   // v184: die Beschreibung enthaelt beide Grenzen — die der anderen Richtung gleich mit merken, damit
   // ein spaeterer Richtungswechsel (z.B. Anmeldung Bezug -> Einspeisung) nicht alles erneut liest.
        { uint8_t d = (ESp->lpc_dir > 1) ? 1 : ESp->lpc_dir; int o = -1;
          ESp->lpc_lid_dir[d] = lid;
          if (EebusLpcPickLimitId(json, (uint8_t)(1 - d), &o)) { ESp->lpc_lid_dir[1 - d] = o; } }
        char t[56]; snprintf(t, sizeof(t), "limitId=%d erkannt", lid);
        EebusLpcSetResult(LPC_READDESC, t);
        EebusLpcReadData();   // erst Ist-LimitData lesen, DANN schreiben
      } else {
        EebusLpcSetResult(LPC_FAIL, (1 == ESp->lpc_dir) ? "keine gueltige produce-Grenze in Beschreibung (LPP TS / LPC IG 3.4)"
                                                        : "keine gueltige consume-Grenze in Beschreibung (LPC TS Tab. 22 / IG 3.4)");
        EebusLbLog("Grenzenbeschreibung ungueltig", ESp->peer_ski, ESp->peer_ip, (1 == ESp->lpc_dir) ? 1 : 0, "", 0,
                   "keine Grenze nach LPC TS Tab. 22 / IG 3.4 - nicht geschrieben");
      }
    }
   // Antwort auf den Ist-LimitData-Read (VOR dem Write). isLimitChangeable pruefen wie Referenz-Umsetzung:
   // explizit false -> Grenze nicht beschreibbar (abbrechen, klare Diagnose). Fehlt das Feld oder true
   // -> weiter zum Write (der Energiemanager hat jetzt unsere LimitData-Anfrage gesehen = volle Referenz-Umsetzung-Sequenz).
    else if ((LPC_READDATA == ESp->lpc_state) && strstr(json, "loadControlLimitListData")) {
   // Der Energiemanager hat jetzt unsere LimitData-Anfrage gesehen (= volle Referenz-Umsetzung-Sequenz). isLimitChangeable
   // NUR zur Diagnose; NICHT hart abbrechen (sonst Regressions-Risiko am funktionierenden Waermepumpen-Gateway-Pfad).
      bool ro = EebusLimitBlockHas(json, ESp->lpc_limit_id, "\"isLimitChangeable\":false");   // v241: nur UNSERE Grenze
      if (ro) {   // v241 (LPC TS Tab. 23): isLimitChangeable false -> timePeriod/value/isLimitActive nicht beschreibbar
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Grenze limitId %d @ %s laut Anlage nicht beschreibbar (isLimitChangeable false) - kein Write"),
               ESp->lpc_limit_id, ESp->peer_ip);
        EebusLbLog("Grenze nicht beschreibbar", ESp->peer_ski, ESp->peer_ip, (1 == ESp->lpc_dir) ? 1 : 0, "", 0,
                   "isLimitChangeable false (LPC TS Tab. 23) - nicht geschrieben");
        EebusLpcSetResult(LPC_FAIL, "Grenze laut Anlage nicht beschreibbar (isLimitChangeable false)");
      } else if (eebus_hems_mode && (0 == ESp->lpc_sel_step)) {
   // limitId-0-Read beantwortet -> jetzt limitId 1 lesen (2 getrennte Selektor-Reads
   // wie die Vergleichs-Steuerbox), DANN erst weiter. Bleibt in LPC_READDATA (frische Deadline).
        ESp->lpc_sel_step = 1;
        EebusLpcReadLimitSel(1);
        ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
        EebusLpcSetResult(LPC_READDATA, "LimitData limitId0 -> lese limitId1");
      } else {
   // v223: die Beschreibung kann hoehere Nummern nennen (HEMS: Bezug 1, Einspeisung 2). Auch diese lesen, sonst
   // bleibt die Grenze dieser Richtung unbekannt und die Anmeldung wuerde sie freigeben (s. Kopf).
        int extra = -1;
        if (eebus_hems_mode && (1 == ESp->lpc_sel_step) && (ESp->lpc_lid_dir[0] > 1)) {
          extra = ESp->lpc_lid_dir[0];
          ESp->lpc_sel_step = 2;
        } else if (eebus_hems_mode && (ESp->lpc_sel_step >= 1) && (ESp->lpc_sel_step <= 2) &&
                   (ESp->lpc_lid_dir[1] > 1) && (ESp->lpc_lid_dir[1] != ESp->lpc_lid_dir[0])) {
          extra = ESp->lpc_lid_dir[1];
          ESp->lpc_sel_step = 3;
        }
        if (extra >= 0) {
          EebusLpcReadLimitSel(extra);
          ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
          char t[48]; snprintf(t, sizeof(t), "LimitData -> lese limitId%d", extra);
          EebusLpcSetResult(LPC_READDATA, t);
        } else {
          EebusLpcSetResult(LPC_READDATA, ro ? "LimitData: isLimitChangeable=false (weiter)"
                                             : "Ist-LimitData erhalten -> DeviceConfig lesen");
          EebusLpcReadCfg();   // erst DeviceConfig (Failsafe) lesen, DANN schreiben
        }
      }
    }
   // DeviceConfig-Antworten. Erst die Beschreibung (KeyValueDescription), dann die WERTE
   // (KeyValueList = Failsafe) lesen -> damit ist die volle Referenz-Umsetzung-Sequenz erfuellt -> schreiben.
   // Die beiden Datentypen sind eindeutig unterscheidbar (Description enthaelt nie "...KeyValueListData").
   // Der gated Schritt haengt an der ZIEL-Entity (lpc_peer_ent, Energiemanager: ent7). Im HEMS-Modus liest
   // EebusLpcSubscribeAll DeviceConfig zusaetzlich auf ent8 (Bauplan B, fire-and-forget); deren Antworten
   // duerfen den Write NICHT vorzeitig ausloesen -> peer_ent-Guard. In Modus 0 kommt nur die ent7-Antwort
   // (kein ent8-Read) -> Guard neutral.
    else if ((LPC_READCFG == ESp->lpc_state) && (peer_ent == ESp->lpc_peer_ent) &&
             strstr(json, "deviceConfigurationKeyValueDescriptionData")) {
      EebusLpcLearnFailsafeKeys(json);   // keyIds der Failsafe-Keys aus der Beschreibung merken (Schreibziele)
      EebusSpineSendAddr("read", false, 0, "{\"deviceConfigurationKeyValueListData\":[]}",
                         EEBUS_LPC_CLIENT_ENT, 3, ESp->lpc_peer_ent, ESp->lpc_peer_dcfg_feat);   // gelernte Adresse statt feat24
      ESp->lpc_deadline = Eebus126At(EebusLpcFristMs());
      EebusLpcSetResult(LPC_READCFG, "DeviceConfig-Werte angefragt");
    }
    else if ((LPC_READCFG == ESp->lpc_state) && (peer_ent == ESp->lpc_peer_ent) &&
             strstr(json, "deviceConfigurationKeyValueListData")) {
      EebusLpcSetResult(LPC_READCFG, "DeviceConfig gelesen -> schreibe");
      EebusLpcWrite();
    }
   // LPC-Controller: Read-Back des Limit-Zustands -> verifizieren
    else if ((LPC_VERIFY == ESp->lpc_state) && strstr(json, "loadControlLimitData")) {
   // den Zustand UNSERER limitId pruefen — nicht blind das erste isLimitActive. Fremde CS liefern
   // in EINER Antwort mehrere Limits (consume + produce); das erste kann das falsche sein -> Fehlalarm
   // "Limit NICHT aktiv" (am Referenz-Umsetzung-HEMS live gesehen). Ab unserem limitId-Block auslesen.
      char idkey[24]; snprintf(idkey, sizeof(idkey), "\"limitId\":%d", ESp->lpc_limit_id);
      const char *blk = strstr(json, idkey);
   // WAHRHEITSWERT RICHTIG LESEN — das war die Wurzel des Anzeigefehlers inzwischen.
   // Bis hierher stand hier EebusJsonStr(..., "isLimitActive", ...). Dieser Extraktor sucht
   // das Muster "key":"…" — also einen Wert IN ANFUEHRUNGSZEICHEN. Im Frame steht aber ein
   // nackter Wahrheitswert: "isLimitActive":true. Das Muster passte nie, die Funktion lieferte
   // nichts, und "is_active" war IMMER false. Folge:
   //   Setzen (Wunsch aktiv)     -> passte nie   -> immer Fehlalarm "Limit NICHT aktiv"
   //   Freigeben (Wunsch inaktiv)-> passte immer -> "BESTAETIGT freigegeben" OHNE Pruefung
   // Der zweite Fall war der gefaehrlichere: eine Bestaetigung, die nichts bewies.
   // belegt: die Gegenstelle antwortete auf JEDEN der fuenf Read-Backs binnen
   // ~100 ms, die Pruefung erkannte trotzdem nichts — waehrend EEBusData aus denselben Frames
   // den richtigen Zustand las. Der Datenteil nutzt seit jeher den direkten Vergleich; genau
   // den verwenden wir jetzt auch hier, auf UNSEREN limitId-Block begrenzt.
      const char *nb2 = blk ? strstr(blk + 1, "\"limitId\":") : nullptr;
      const char *ap2 = strstr(blk ? blk : json, "\"isLimitActive\":");
      const bool have_state = (nullptr != ap2) && (!nb2 || (ap2 < nb2));
      const bool is_active  = have_state && (0 == strncmp(ap2 + 16, "true", 4));
   // Fehlt die Angabe ganz, ist das KEINE Antwort — dann weiter nachpollen statt zu raten.
   // Sonst haette eine Freigabe sich selbst bestaetigt, nur weil nichts dastand.
      if (have_state && (is_active == ESp->lpc_active_wish)) {
        ESp->lpc_our_limit = ESp->lpc_active_wish;   // gesetzt -> Freigabe-Pflicht; freigegeben -> weg
        ESp->frei_offen[(1 == ESp->lpc_dir) ? 1 : 0] = false;   // v225
        char t[56]; snprintf(t, sizeof(t), ESp->lpc_active_wish ? "BESTAETIGT aktiv %u W" : "BESTAETIGT freigegeben", ESp->lpc_value);
        EebusLpcSetResult(LPC_DONE, t);
      } else if (ESp->lpc_verify_tries < EEBUS_LPC_VERIFY_MAX_TRIES) {
   // Zustand noch nicht wie gewuenscht -> Latenz? Nachpollen statt sofort scheitern.
        ESp->lpc_verify_tries++;
        EebusLpcVerifySchedule(false);
      } else {
   // Der alte Einheitstext "Limit NICHT aktiv (abgelehnt?)" warf drei verschiedene Lagen
   // in einen Topf und unterstellte immer eine Zurueckweisung. Das ist bei einem Pruefwerkzeug
   // das Schlimmste: man kann der Anzeige in keiner Richtung trauen. Jetzt getrennt:
   //   ausgelaufen      — die Gegenstelle fuehrt Restlaufzeit 0; die Vorgabe ist schlicht
   //                      abgelaufen bzw. hat eine abgelaufene Dauer geerbt. KEINE Ablehnung.
   //   nicht uebernommen— sie meldet inaktiv ohne Zeitbezug; Grund unbekannt.
   //   Freigabe offen   — wir wollten aufheben, sie bestaetigt es nicht.
   // Eine echte Zurueckweisung meldet die Gegenstelle dagegen als errorNumber; die laeuft
   // ueber den resultData-Zweig und heisst dort weiterhin "Write abgelehnt".
        ESp->lpc_verify_failed = true;
        char endt[16] = { 0 };
        const char *nb = blk ? strstr(blk + 1, "\"limitId\":") : nullptr;
        const char *ep2 = blk ? strstr(blk, "\"endTime\":\"") : nullptr;
        if (ep2 && (!nb || (ep2 < nb))) { EebusJsonStr(ep2, "endTime", endt, sizeof(endt)); }
        const long restsec = EebusIsoDurSecs(endt);
        if (!ESp->lpc_active_wish) {
          EebusLpcSetResult(LPC_FAIL, "Freigabe nicht bestaetigt");
          EebusVorgabeWiederholen("nicht bestaetigt");   // v225
        } else if (0 == restsec) {
          EebusLpcSetResult(LPC_FAIL, "ausgelaufen - Restlaufzeit 0 (keine Ablehnung)");
        } else {
          EebusLpcSetResult(LPC_FAIL, "nicht uebernommen - keine Bestaetigung");
          EebusVorgabeWiederholen("nicht bestaetigt");   // v225
        }
      }
    }
   // Ladestation quittiert unser Binding/Write als "reply" mit resultData (NICHT als "result") ->
   // hier abfangen, sonst haengt die LPC-Sequenz im Timeout (bewiesen : errorNumber:0 "Binding request was successful").
    else if (((LPC_BIND == ESp->lpc_state) || (LPC_FAILSAFE == ESp->lpc_state) || (LPC_WRITE == ESp->lpc_state)) && strstr(json, "resultData") &&
             ((LPC_BIND == ESp->lpc_state) ? Eebus126BindRefOk(json) : Eebus126LpcRefOk(json))) {   // v231 (4.8), v240 Binding
      bool ok = (nullptr != strstr(json, "\"errorNumber\":0")) || (nullptr == strstr(json, "errorNumber"));
      if (LPC_BIND == ESp->lpc_state) {
        if (ok) { ESp->lpc_bound = true; EebusLpcSetResult(LPC_BIND, "Binding bestaetigt"); EebusLpcReadDesc(); }
        else    { EebusLpcSetResult(LPC_FAIL, "Binding abgelehnt"); }
      } else if (LPC_FAILSAFE == ESp->lpc_state) {   // Failsafe-Write quittiert -> Dauer bzw. Limit
        EebusLpcFailsafeResult(ok);
      } else {   // LPC_WRITE
        if (ok) { EebusLpcVerifySchedule(true); }   // Read-Back verzoegert
        else    { EebusErr9Pruefen(json); EebusLpcWriteRejected(); }   // err7 -> Heartbeat+Retry (HEMS), sonst FAIL; v246: err9 -> neu binden
      }
    }
  } else if (0 == strcmp(cls, "result")) {
   // Wartet ein gezieltes EEBusRead? Dann ist DIESES result seine Antwort — eine ABLEHNUNG.
   // ⚠ Warum das wichtig ist (29.07. bewiesen): ein Read an eine Untereinheit ging an [6] statt
   // [6,1], die Gegenstelle antwortete mit errorNumber 4 "Ziel unbekannt" — und weil hier nur
   // "reply" aufgehoben wurde, sah das Werkzeug SCHWEIGEN. Aus dem Schweigen wurde die falsche
   // Aussage "die Gegenstelle liefert dort nichts". Eine Ablehnung muss man sehen koennen.
   // Zuordnung ueber die Nachrichtennummer, nicht ueber "irgendein Fehler kam herein" — sonst
   // schluckt die Leseanzeige die Ablehnung eines Limit-Writes, der zufaellig gleichzeitig laeuft.
    char rd_ref[40]; snprintf_P(rd_ref, sizeof(rd_ref), PSTR("\"msgCounterReference\":%u"), ESp->rd_mc);
    if ((ESp->rd_ent >= 0) && ESp->rd_mc && (nullptr != strstr(json, rd_ref))
        && (nullptr != strstr(json, "errorNumber"))
        && (nullptr == strstr(json, "\"errorNumber\":0"))) {
      if (ESp->rd_buf) { free(ESp->rd_buf); ESp->rd_buf = nullptr; }
      size_t rl = strlen(json);
      char *rb = (char*)special_malloc(rl + 1);
      if (rb) { memcpy(rb, json, rl + 1); ESp->rd_buf = rb; }
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Leseanfrage ABGELEHNT (ent[%s]/feat%d) — Antwort aufgehoben"),
             ESp->rd_pend_elist, ESp->rd_feat);
      ESp->rd_ent = -1; ESp->rd_feat = -1;   // erledigt, sonst faengt sie fremde results ab
    }
   // LPC-Controller: Ergebnis (errorNumber) auf unser Binding (call) bzw. Write.
    bool ok = (nullptr != strstr(json, "\"errorNumber\":0")) || (nullptr == strstr(json, "errorNumber"));
    if (((LPC_FAILSAFE == ESp->lpc_state) || (LPC_WRITE == ESp->lpc_state)) && !Eebus126LpcRefOk(json)) {   // v231 (4.8): Antwort auf eine andere Nachricht - nicht als LPC-Ergebnis werten
    } else if ((LPC_BIND == ESp->lpc_state) && Eebus126BindRefOk(json)) {   // v240: nur die Quittung auf unser Binding
      if (ok) { ESp->lpc_bound = true; EebusLpcSetResult(LPC_BIND, "Binding bestaetigt"); EebusLpcReadDesc(); }
      else    { EebusLpcSetResult(LPC_FAIL, "Binding abgelehnt"); }
    } else if (LPC_FAILSAFE == ESp->lpc_state) {   // Failsafe-Write quittiert -> Dauer bzw. Limit
      EebusLpcFailsafeResult(ok);
    } else if (LPC_WRITE == ESp->lpc_state) {
      if (ok) { EebusLpcVerifySchedule(true); }   // Write quittiert -> Read-Back verzoegert
      else    { EebusErr9Pruefen(json); EebusLpcWriteRejected(); }   // err7 -> Heartbeat+Retry (HEMS), sonst FAIL; v246: err9 -> neu binden
    }
  }
   // notify vom Peer: nur mitschneiden (bereits im Log)
   // v239 (SPINE TS Z. 1107-1109, 1134-1145): notify mit ackRequest -> Quittung an das Absender-Feature
  if (((0 == strcmp(cls, "notify")) || (0 == strcmp(cls, "reply"))) && ack) {   // v246 (B4): auch reply
    EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":0}]}", our_ent, our_feat, peer_ent, peer_feat);
  }
   // v239 (LPC TS Z. 1455-1456, 1470-1477): GEAENDERTE Geraeteliste der Anlage (Teil-Meldung) -> neu lesen
  if ((0 == strcmp(cls, "notify")) && strstr(json, "nodeManagementDetailedDiscoveryData") &&
      (strstr(json, "\"partial\"") || strstr(json, "\"delete\""))) {
    ESp->peer_disco_read = false;
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Geraeteliste von %s hat sich geaendert - neu lesen"), ESp->peer_ip);
  }
}

// v231 (Pruefung 4.8): gehoert diese Antwort zu unserem letzten Write (Failsafe oder Grenze)? SPINE ordnet Antworten
// ueber msgCounterReference zu. Bis v230 galt JEDE Antwort mit resultData als Ergebnis des laufenden Schritts - eine
// verspaetete Quittung (z. B. eines frueheren Writes oder eines Abos) konnte als bestaetigter Write gelten.
// Ohne msgCounterReference in der Antwort (tolerante Gegenstellen) bleibt es beim bisherigen Verhalten. Das Binding
// bleibt bewusst wie bisher: dort gehen mehrere Bindings hinaus (LoadControl + DeviceConfiguration), die erste
// Quittung gilt - so ist es mit dem Energiemanager erprobt.
// v240 (LPC TS Z. 1450-1452): die Quittung auf UNSER LoadControl-Binding erkennen. Bis v239 galt die erste beliebige
// Quittung im Zustand BIND (oft die eines Abos oder des DeviceConfiguration-Bindings). Ohne msgCounterReference in der
// Antwort (tolerante Gegenstellen) bzw. ohne gemerkte Nummer wie bisher.
bool Eebus126BindRefOk(const char *json) {
  uint32_t r = 0;
  if (!ESp->bind_mc || !EebusJsonInt(json, "msgCounterReference", &r)) { return true; }
  return (r == ESp->bind_mc);
}
bool Eebus126LpcRefOk(const char *json) {
  uint32_t r = 0;
  if (!ESp->lpc_mc || !EebusJsonInt(json, "msgCounterReference", &r)) { return true; }
  if (r == ESp->lpc_mc) { return true; }
  AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: Antwort auf Nachricht %u @ %s gehoert nicht zum LPC-Schritt (erwartet %u)"), r, ESp->peer_ip, ESp->lpc_mc);
  return false;
}

// Handschlag SAUBER abschliessen — Datenphase (Done) erst erreichen, NACHDEM
// pinState UND accessMethods des Peers empfangen wurden (Referenz-Umsetzung-Gate CheckListen->AccessRequest->
// approveHandshake). Wird aus 3 Punkten aufgerufen: nach Peer-accessMethods (Regelfall) und aus den
// beiden Gate-Timeout-Fallbacks. Der bisherige Done-Block (Discovery-Eroeffnung) steckt jetzt hier.
void EebusSmeReachDone(const char *grund) {
  ESp->sme = SME_DONE;
  ESp->sme_deadline = 0;
  ESp->disco_fallback_at = Eebus126At(3000);   // Deadlock-Bruch, falls der Peer auf UNS wartet
  EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP-Handshake KOMPLETT (Done, %s) @ %s - Datenphase erreicht"),
         grund, ESp->peer_ip);
  ESp->ka_try = 0; ESp->ka_scans = 0;   // v180: Verbindung steht wieder
  ESp->ka_lost_at = 0; ESp->ka_last_try = 0;   // v229
   // Erst JETZT merken, nicht schon beim Verbinden: eine Verbindung, die es nicht bis in die
   // Datenphase schafft, ist kein Partner, den man beim naechsten Start wieder suchen sollte.
   // Server-Slots ausgenommen — dort steht die Peer-IP als Pseudo-SKI, die identifiziert niemanden.
   // v191: AUSSER der Anrufer hat sich mit Zertifikat ausgewiesen (ski_quelle 1) - dann ist die SKI echt.
   // v181 (08.10.): NUR merken, wenn der bisher gemerkte Partner gerade NICHT verbunden ist. Sonst
   // wurde jede zusaetzliche Verbindung (z.B. kurz zum Waermepumpen-Gateway testen) zum Wiederaufbau-
   // Partner, obwohl der eigentliche Partner verbunden blieb — nach dem naechsten Stromausfall verband
   // sich die Box dann mit dem falschen Geraet (03.08. so beinahe passiert). Ein bewusster Wechsel
   // (alten Partner trennen, neuen verbinden) wird weiter gemerkt.
  if (!ESp->via_srv || (1 == ESp->ski_quelle)) {   // v191
    EebusPeerRemember(ESp->peer_ski);   // v202: jede Verbindung mit echter SKI kommt in die Partnerliste (v181-Regel entfaellt)
  }
   // v202: der Wiederaufbau laeuft weiter, solange noch ein gemerkter Partner fehlt (er prueft je SKI selbst)
  if (eebus_open_mode >= 1) {
   // Eroeffnungszug: SOFORT einen ADRESSLOSEN DetailedDiscovery-Read senden. Reaktiven
   // adressierten Read unterdruecken (peer_disco_read=true) + 3-s-Fallback loeschen (schon gesendet).
    ESp->disco_fallback_at = 0;
    ESp->peer_disco_read = true;
    EebusSpineSend("read", false, 0, "{\"nodeManagementDetailedDiscoveryData\":[]}");   // [] statt {} (siehe Z.1785)
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: ADRESSLOSER Discovery-Eroeffnungs-Read (EEBusOpen 1) @ %s"), ESp->peer_ip);
  } else {
    EebusSpineReadPeerDiscovery();   // greift erst, sobald peer_dev bekannt ist -> real vom ersten Peer-Frame
  }
}

// Eine empfangene SHIP-Nachricht verarbeiten (rx[0]=classifier, dahinter JSON, NUL-terminiert)
void EebusSmeDispatch(const uint8_t *rx, int n) {
  int classifier = rx[0];
  const char *json = (const char*)rx + 1;

  if (3 == classifier) {   // connectionClose vom Peer
     // v231 (Pruefung 3.9): auf "announce" mit "confirm" antworten, dann schliessen (SHIP 13.4.8.1.2, Regeln fuer DEV-B
     // Z. 3158-3162: "it SHALL send an SME close message ... with sub-element phase set to confirm and no other
     // sub-element set. Afterwards, it SHALL close the connection"). Ein WS-Close (Ersatz "{}") hat keine Phase.
    const bool war_done = (SME_DONE == ESp->sme);   // Gegenpruefung 06.11.: vor dem Senden merken (Sendefehler setzt SME_OFF)
    if (strstr(json, "\"announce\"")) {
      EebusShipSendJson("{\"connectionClose\":[{\"phase\":\"confirm\"}]}", 3);
    }
    if (war_done) {   // normales Ende nach Done
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer hat Verbindung geschlossen (%s)"), ESp->peer_ip);
      EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
      ESp->sme = SME_OFF;
      ESp->state = SHIP_IDLE;
      if (ESp->keepalive && (ESp->peer_idx >= 0)) {   // Keep-Alive: nach kurzem Warten neu verbinden
        ESp->reconnect_at = Eebus126At(EebusKaWaitMs());
      }
      EebusTeardownLater();   // Client verzoegert freigeben
    } else if (SME_OFF != ESp->sme) {   // Close VOR Done = abgewiesen (nicht, wenn nur das confirm scheiterte)
      EebusSmeFail("peer close vor done (Pairing abgelehnt?)");
    }
    return;
  }
  if (2 == classifier) {   // SPINE-Datagram (Datenphase, M3)
    EebusSpineHandle(json);
    return;
  }
  if (1 != classifier) { return; }   // CMI(0) o.ae. hier nicht erwartet

   // v242 (SHIP 13.4.4.1 Z. 2202-2215): im Zustand Hello zaehlt nur ein Hello, alles andere ist Abbruch.
  const bool ist_hello = (nullptr != strstr(json, "\"connectionHello\""));
  if ((SME_HELLO == ESp->sme) && !ist_hello) { EebusSmeAbbruch("hello: unerwartete Nachricht", 0); return; }
  if (ist_hello) {
    char phase[12] = { 0 };
    EebusJsonStr(json, "phase", phase, sizeof(phase));
    if (SME_HELLO != ESp->sme) {   // spaeteres Hello: nur ein Abbruch zaehlt
      if (0 == strcmp(phase, "aborted")) { EebusSmeFail("hello aborted vom peer"); }
      return;
    }
    if (0 == strcmp(phase, "ready")) {   // HELLO_OK -> ProtocolHandshake (wir = Client -> announceMax, SHIP 13.4.4.2)
      if (!EebusShipSendJson("{\"messageProtocolHandshake\":[{\"handshakeType\":\"announceMax\"},"
                             "{\"version\":[{\"major\":1},{\"minor\":0}]},"
                             "{\"formats\":[{\"format\":[\"JSON-UTF8\"]}]}]}", 1)) {
        EebusSmeFail("prot send"); return;
      }
      ESp->sme = SME_PROT;
      ESp->sme_deadline = Eebus126At(EEBUS_SME_PROT_TIMEOUT_MS);
    } else if (0 == strcmp(phase, "pending")) {
      if (!ESp->sme_pending_logged) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer %s meldet PENDING - unsere SKI %s im Geraete-UI bestaetigen (Pairing)"),
               ESp->peer_ip, Eebus.own_ski);
        ESp->sme_pending_logged = true;
      }
      if (strstr(json, "prolongationRequest") && strstr(json, "true")) {   // SHIP Z. 2208-2211, 2307-2313
        if ((millis() - ESp->sme_hello_start) < EEBUS_SME_HELLO_MAX_MS) {
          ESp->sme_deadline += EEBUS_SME_HELLO_TIMEOUT_MS;   // T_hello_inc = T_hello_init
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s bittet um Verlaengerung - Wartezeit +60 s"), ESp->peer_ip);
        }
        EebusSmeHelloUpdate();
      }   // ohne Verlaengerungswunsch: ignorieren (SHIP Z. 2206-2207)
    } else {   // aborted oder unbekannt -> gemeinsamer Abbruch (SHIP Z. 2212-2213)
      EebusSmeAbbruch("hello aborted", 0);
    }
    return;
  }

  if (strstr(json, "\"messageProtocolHandshakeError\"")) {
    EebusSmeFail("prot error vom peer");
    return;
  }

  if (SME_PROT == ESp->sme) {   // v242 (SHIP Z. 2400-2416): Auswahl des Servers pruefen
    if (nullptr == strstr(json, "\"messageProtocolHandshake\"")) { EebusSmeAbbruch("prot: unerwartete Nachricht", 2); return; }
    char ht[16] = { 0 };
    EebusJsonStr(json, "handshakeType", ht, sizeof(ht));
    uint32_t ma = 99, mi = 99;
    EebusJsonInt(json, "major", &ma);
    EebusJsonInt(json, "minor", &mi);
    if ((0 != strcmp(ht, "select")) || (1 != ma) || (0 != mi) || !EebusFormatNurJson(json)) {
      EebusSmeAbbruch("prot: Auswahl passt nicht", 3);
      return;
    }
   // Auswahl bestaetigen (select 1:1 zurueckspiegeln, SHIP Z. 2411-2412) + pinState:none senden - dann auf den pinState
   // des Peers warten (SME_PIN), erst danach accessMethods und Datenphase.
    if (!EebusShipSendJson(json, 1)) { EebusSmeFail("prot confirm send"); return; }
    EebusShipSendJson("{\"connectionPinState\":[{\"pinState\":\"none\"}]}", 1);
    ESp->sme = SME_PIN;
    ESp->sme_deadline = Eebus126At(EEBUS_SME_PIN_TIMEOUT_MS);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: select bestaetigt + pinState:none @ %s - warte auf Peer-pinState"), ESp->peer_ip);
    return;
  }
  if (strstr(json, "\"messageProtocolHandshake\"")) { return; }   // ausserhalb des Handshakes: nicht mehr auswerten

  if (strstr(json, "\"connectionPinState\"")) {
    char pin[12] = { 0 };
    EebusJsonStr(json, "pinState", pin, sizeof(pin));
    if (0 == strcmp(pin, "required")) {   // PIN unterstuetzen wir nicht
      EebusSmeFail("peer verlangt PIN");
      return;
    }
   // none/optional: Datenphase ist frei (SHIP Z. 2747-2754) -> jetzt accessMethodsRequest (erst ab Datenphase, Z. 2882-2884)
    if (SME_PIN == ESp->sme) {
      EebusShipSendJson("{\"accessMethodsRequest\":[]}", 1);
      ESp->sme = SME_ACCESS;
      ESp->sme_deadline = Eebus126At(EEBUS_SME_GATE_TIMEOUT_MS);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer-pinState=%s -> accessMethodsRequest @ %s - warte auf Peer-accessMethods"),
             pin[0] ? pin : "none", ESp->peer_ip);
    }
    return;
  }

  if (strstr(json, "\"accessMethodsRequest\"")) {   // Peer fragt UNSERE Kennung ab
   // accessMethods.id = unsere mDNS-id (eebus_adv_id). v242 (SHIP Tab. 22): dnsSd_mDns, weil wir per mDNS auffindbar sind.
    char am[96];
    snprintf_P(am, sizeof(am), PSTR("{\"accessMethods\":[{\"id\":\"%s\"},{\"dnsSd_mDns\":[]}]}"), eebus_adv_id);
    EebusShipSendJson(am, 1);
    return;
  }

  if (strstr(json, "\"accessMethods\"")) {   // Kennung des Peers (Antwort auf unseren Request)
    EebusJsonStr(json, "id", ESp->peer_id, sizeof(ESp->peer_id));
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer AccessMethods id=%s"), ESp->peer_id);
    if (SME_ACCESS == ESp->sme) { EebusSmeReachDone("accessMethods"); }
    return;
  }
}

// 100-ms-Poll: empfangene Frames verarbeiten + Phasen-Timeouts pruefen
void EebusSmePoll(void) {
  if (ESp->via_srv) {   // Slot einer EINGEHENDEN Verbindung —
    if (ESp->teardown) {   // RX/Timeout macht das ESrv-Modul; nur ein
      EebusSrvFree();   // von SPINE angeforderter Abbau landet hier
    }   // (schliesst Socket + setzt Slot zurueck)
    return;
  }
  EebusTeardownNow();   // anstehende Client-Freigabe hier (sicherer Kontext) erledigen
  if ((SME_OFF == ESp->sme) || (SME_FAIL == ESp->sme)) { return; }
  if (!ESp->active || (nullptr == ESp->client)) { ESp->sme = SME_OFF; return; }

  // v228: Die TLS-Schicht hat ihre Sendewartezeit einmal verbraucht (Socket nahm EEBUS_SEND_WAIT_MS lang nichts an).
  // Sofort abbauen und neu aufbauen, statt bis zum naechsten fehlgeschlagenen write() zu warten.
  if (ESp->client->sendStuck()) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Senden an %s seit %u ms ohne Fortschritt - Gegenstelle nimmt nichts an"),
           ESp->peer_ip, (uint32_t)EEBUS_SEND_WAIT_MS);
    if (SME_DONE == ESp->sme) { EebusSendBroken(); } else { ESp->send_broken = true; EebusSmeFail("senden haengt"); }
    return;
  }

  if (!ESp->client->connected() && (ESp->client->available() <= 0)) {
    if (SME_DONE == ESp->sme) {   // Peer hat aufgelegt
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP-Verbindung beendet (%s)"), ESp->peer_ip);
      EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
      ESp->sme = SME_OFF;
      ESp->state = SHIP_IDLE;
      if (ESp->keepalive && (ESp->peer_idx >= 0)) {   // Keep-Alive: bald neu verbinden
        ESp->reconnect_at = Eebus126At(EebusKaWaitMs());
      }
      EebusTeardownLater();   // Client verzoegert freigeben
    } else {
      EebusSmeFail("verbindung weg");
    }
    return;
  }

   // Frames abholen (mehrere pro Tick moeglich) — grosser PSRAM-Puffer fuer SPINE-Datagramme
  uint8_t *rx = ESp->rxbuf;
  size_t rxmax = ESp->rxbuf_size ? ESp->rxbuf_size : 0;
  if ((nullptr == rx) || (0 == rxmax)) { return; }
  for (uint8_t nrx = 0; ESp->client && (nrx < 16); nrx++) {   // v241: ohne Blockieren, hoechstens 16 Nachrichten je Durchlauf
    int n = EebusWsRecvNb(rx, rxmax - 1);
    if (-2 == n) {   // WS-Close-Frame
      EebusSmeDispatch((const uint8_t*)"\x03{}", 3);   // wie connectionClose behandeln
      return;
    }
    if (n < 0) {   // v241: Rest eines Frames kam nicht binnen EEBUS_WS_REST_MS (oder Fehler)
       // v231 (Pruefung 4.9): Bytes dieses Frames sind schon gelesen, der Rest kam nicht. Bis v230 ging es im naechsten
       // Tick weiter - das naechste Byte wurde dann als Frame-Kopf gelesen, der Strom war aus dem Takt (Schein-Frames,
       // zerhackte JSON). Jetzt wie ein Abbruch behandeln: abbauen, der Keep-Alive bzw. die Gegenstelle baut neu auf.
      if (SME_DONE == ESp->sme) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** Empfang von %s: %s - Verbindung wird abgebaut und neu aufgebaut ***"),
               ESp->peer_ip, (-3 == n) ? "unzulaessiger Frame-Typ" : "Rest des Frames kam nicht binnen 10 s");
        EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
        ESp->sme = SME_OFF;
        ESp->state = SHIP_IDLE;
        ESp->last_rx = 0;
        if (ESp->keepalive && (ESp->peer_idx >= 0)) { ESp->reconnect_at = Eebus126At(EebusKaWaitMs()); }
        EebusTeardownLater();
      } else {
        EebusSmeFail("empfang abgebrochen");
      }
      return;
    }
    if (0 == n) { break; }   // v241: nichts (mehr) vollstaendig - Rest im naechsten Durchlauf
    EebusHeapCheckN("rx_recv", (uint32_t)n);   // ROH nach WsRecv, VOR Verarbeitung
    rx[n] = '\0';
    ESp->last_rx = millis();
    if (SME_CMI == ESp->sme) {   // v242: erste Nachricht = CMI des Servers
      EebusCmiAntwort(rx, n);
      if ((SME_OFF == ESp->sme) || (SME_FAIL == ESp->sme)) { return; }
      continue;
    }
    EebusShipLog('R', rx[0], (const char*)rx + 1);
    EebusSmeDispatch(rx, n);
    EebusHeapCheck("rx_dispatch");   // Checkpunkt: nach Verarbeitung jeder Empfangs-Nachricht
    if ((SME_OFF == ESp->sme) || (SME_FAIL == ESp->sme)) { return; }
  }

   // HENNE-EI-FALLBACK (Ladestation-Deadlockbewiesen): Der Peer kann seine eigene
   // Discovery nicht an uns richten, solange er nie einen Frame von uns empfing (Ladestation
   // eebus_usecases.cpp get_spine_connection -> "no spine connection found" -> ihr Read
   // wird NIE gesendet). Unser Guard wartete umgekehrt auf SEINEN ersten Frame ->
   // beidseitige Funkstille -> "Peer Degradiert" nach 10 s. Loesung: 3 s nach Done ohne
   // Peer-Frame den Discovery-Read einmalig ADRESSLOS senden (Verhalten; die Ladestation
   // toleriert das explizit RX-seitig, spine_connection.cpp:161-165 "assume message is
   // for us"; Energiemanager/Waermepumpen-Gateway ignorieren adresslose Reads nachweislich folgenlos). Sobald der
   // Peer antwortet, kennen wir peer_dev und der adressierte Pfad laeuft normal.
  if ((SME_DONE == ESp->sme) && !ESp->peer_disco_read && !ESp->peer_dev[0] &&
      ESp->disco_fallback_at && TimeReached(ESp->disco_fallback_at)) {
    ESp->disco_fallback_at = 0;
    EebusSpineSend("read", false, 0, "{\"nodeManagementDetailedDiscoveryData\":[]}");   // [] statt {} (siehe Z.1785)
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SPINE Discovery-Fallback ADRESSLOS gesendet (Peer wartet auf uns) @ %s"),
           ESp->peer_ip);
  }

   // Handschlag-Gates. Antwortet der Peer nicht rechtzeitig mit seinem
   // pinState/accessMethods, NICHT abbrechen — in die Datenphase weitergehen (Alt-Verhalten als
   // Fallback -> schuetzt tolerante Peers Ladestation/Waermepumpen-Gateway, falls sie ein Gate auslassen).
  if ((SME_CMI == ESp->sme) && TimeReached(ESp->sme_deadline)) {   // v242 (SHIP Z. 2082-2083)
    EebusSmeFail("cmi timeout (15 s)");
    return;
  }
  if (SME_PIN == ESp->sme) {   // v242 (SHIP Z. 2663-2664): ohne pinState des Peers binnen 10 s schliessen
    if (TimeReached(ESp->sme_deadline)) { EebusSmeFail("kein pinState des Peers binnen 10 s"); }
    return;
  }
  if (SME_ACCESS == ESp->sme) {   // accessMethods sind kein Tor der Norm (SHIP Z. 2882): nach 5 s trotzdem weiter
    if (TimeReached(ESp->sme_deadline)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: keine accessMethods von %s binnen 5 s - Datenphase trotzdem"), ESp->peer_ip);
      EebusSmeReachDone("gate-timeout");
    }
    return;
  }

   // Timeouts - v242 (SHIP Z. 2216-2218, 2429-2431): Wait-For-Ready bzw. Wait-Timer abgelaufen -> Abbruch. Keine eigene
   // Verlaengerung mehr: wer wartet (pending), muss selbst um Verlaengerung bitten (SHIP Z. 2266-2276).
  if (((SME_HELLO == ESp->sme) || (SME_PROT == ESp->sme)) && TimeReached(ESp->sme_deadline)) {
    EebusSmeAbbruch((SME_HELLO == ESp->sme) ? "hello timeout" : "prot timeout", 1);
    return;
  }
  EebusLpcPhasenFrist();   // v239: ausgelagert, gilt jetzt auch fuer eingehende Verbindungen
}

// v239 (Normpruefung 22.11., SPINE TS 5.2.5.3 Z. 1193-1198): Antwortfrist der LPC-Kette. Bis v238 nur in EebusSmePoll,
// das bei EINGEHENDEN Verbindungen (via_srv) vorher zurueckkehrt - bis auf READCFG hatte die Kette dort KEINE Frist und
// blieb bei ausbleibender Antwort fuer immer stehen (keine Sicherung, Wiederholung, Vorgabe mehr). Jetzt fuer beide Wege.
void EebusLpcPhasenFrist(void) {
   // LPC-Sequenz-Timeout (Binding/ReadDesc/Write/Verify haengt)
  if ((ESp->lpc_state >= LPC_BIND) && (ESp->lpc_state <= LPC_VERIFY) &&
      ESp->lpc_deadline && TimeReached(ESp->lpc_deadline)) {
    ESp->lpc_deadline = 0;
    if (LPC_READCFG == ESp->lpc_state) {   // DeviceConfig-Read best-effort -> trotzdem schreiben
      EebusLpcSetResult(LPC_READCFG, "DeviceConfig-Read Timeout -> schreibe trotzdem");
      EebusLpcWrite();
    } else if (LPC_FAILSAFE == ESp->lpc_state) {   // v196: keine Quittung -> naechster Failsafe-Wert
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Write Schritt %u ohne Quittung @ %s"), ESp->lpc_fs_step, ESp->peer_ip);
      EebusLpcFailsafeNext(ESp->lpc_fs_step);
    } else {
      EebusLpcSetResult(LPC_FAIL, "Timeout (keine Antwort)");
      EebusVorgabeWiederholen("ohne Antwort");   // v225
    }
  }
}

/*********************************************************************************************\
 * Kommandos M2
\*********************************************************************************************/

// Kern von EEBusConnect (auch der Web-Link "Verbinden" nutzt ihn): Slot besorgen
// (Wiederverwendung bei gleicher SKI, sonst freier Slot), Keep-Alive an, verbinden.
// Rueckgabe: Slot-Nr (>=0, Verbindungsergebnis steht in ESp->state/err),
// -1 = Index/Zertifikat-Problem, -2 = alle Slots belegt.
int EebusConnectPeer(int idx) {
  if ((idx < 0) || (idx >= (int)Eebus.peer_count)) { return -1; }
  if (!Eebus.cert_ok && !EebusEnsureCert(false)) { return -1; }
  int ci = EebusConnAlloc(Eebus.peers[idx].ski);
  if (ci < 0) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: alle %d Verbindungs-Slots belegt (EEBusDisconnect <idx>)"), EEBUS_MAX_CONN);
    return -2;
  }
  ESp = &EConn[ci]; Eebus126ModeLoad();
  ESp->keepalive = true;   // Verbindung dauerhaft halten (Auto-Reconnect nach Done)
  ESp->reconnect_at = 0;
  EebusShipConnect(idx);   // Ergebnis steht in ESp->state/sme/err
  return ci;
}

// Selbsttaetiger Wiederaufbau nach einem Neustart — im Sekundentakt aufgerufen.
// Uebergibt nach dem ersten geglueckten Verbindungsversuch an den vorhandenen Keep-Alive,
// der eine LAUFENDE Verbindung ohnehin schon haelt (und den Peer dabei ueber die SKI
// nachschlaegt, falls sich die Scan-Reihenfolge geaendert hat).
// v202: ist diese SKI gerade verbunden (aktiver Platz)?
bool EebusAcConnected(const char *ski) {
  if (!ski || !ski[0]) { return false; }
  for (uint32_t i = 0; i < EEBUS_MAX_CONN; i++) {
    if (EConn[i].active && (0 == strcasecmp(EConn[i].peer_ski, ski))) { return true; }
  }
  return false;
}
// v202: ab eebus_ac_cur den naechsten gemerkten Partner suchen, der NICHT verbunden ist, und als eebus_ac_ski setzen.
// false = keiner mehr -> Wiederaufbau fertig (eebus_ac_state = AC_DONE).
bool EebusAcNaechster(void) {
  while ((eebus_ac_cur < eebus_ac_n) && EebusAcConnected(eebus_ac_list[eebus_ac_cur])) { eebus_ac_cur++; eebus_ac_try = 0; }
  if (eebus_ac_cur >= eebus_ac_n) { eebus_ac_state = AC_DONE; return false; }
  strlcpy(eebus_ac_ski, eebus_ac_list[eebus_ac_cur], sizeof(eebus_ac_ski));
  return true;
}

void EebusAutoConnectRun(void) {
  if (AC_DONE == eebus_ac_state) { return; }
  if (0 == eebus_ac_delay_s) { eebus_ac_state = AC_DONE; return; }   // per EEBusAutoConn 0 abgeschaltet

  switch (eebus_ac_state) {

    case AC_IDLE:
      if (!eebus_ac_delay_loaded) {   // v208: eingestellte Wartezeit aus dem Flash (sonst Vorgabe 30 s)
        eebus_ac_delay_loaded = true;
#ifdef USE_UFILESYS
        char t[8] = { 0 };
        if (TfsLoadFile(EEBUS_AC_FILE, (uint8_t*)t, sizeof(t) - 1)) {
          long v = atol(t);
          if ((v >= 0) && (v <= 3600)) { eebus_ac_delay_s = (uint16_t)v; }
          if (0 == eebus_ac_delay_s) { eebus_ac_state = AC_DONE; return; }
        }
#endif
      }
   // Drei Bedingungen, alle noetig (Begruendung im Kopf des Abschnitts):
   // Netz steht · Uhr ist synchronisiert · eine SKI ist gemerkt.
      if (!(WifiHasIP() || ((uint32_t)EthernetLocalIP() != 0))) { return; }
      if (!RtcTime.valid && (TasmotaGlobal.uptime < 60)) { return; }   // v241: ohne Uhr spaetestens nach 60 s (seit v231 kein 1970-Zeitstempel mehr)
      if (!EebusPeerRecall()) { eebus_ac_state = AC_DONE; return; }   // nichts gemerkt
      eebus_ac_cur   = 0;   // v202
      eebus_ac_try   = 0;
      eebus_ac_next  = Eebus126At((eebus_ac_delay_s * 1000UL));
      eebus_ac_state = AC_WAIT;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau zu %u Partner(n) vorgemerkt (erster %s) - Beginn in %u s (Netz und Uhr stehen)"),
             eebus_ac_n, eebus_ac_ski, eebus_ac_delay_s);
      break;

    case AC_WAIT:
      if (!TimeReached(eebus_ac_next)) { return; }
   // Ist die Verbindung inzwischen anderweitig zustande gekommen (von Hand oder eingehend)?
   // v202: dann zum naechsten gemerkten Partner; sind alle verbunden, ist nichts mehr zu tun.
      if (!EebusAcNaechster()) { return; }
      if (!EebusStartScan()) { eebus_ac_next = Eebus126At(5000); return; }   // laeuft schon / kein mDNS
      eebus_ac_next  = Eebus126At(EEBUS_AC_SCAN_WAIT_MS);
      eebus_ac_state = AC_SCAN;
      break;

    case AC_SCAN: {
      if (!TimeReached(eebus_ac_next)) { return; }
   // Waehrend des Scans eingehend verbunden (Energiemanager ruft nach seinem Neustart selbst an)?
   // Dann nicht zusaetzlich selbst anrufen (05.10. 20:51: zwei parallele Verbindungen). v202: naechster Partner.
      if (EebusAcConnected(eebus_ac_ski) && !EebusAcNaechster()) { return; }
      int idx = -1;
      for (uint32_t k = 0; k < Eebus.peer_count; k++) {
        if (0 == strcasecmp(Eebus.peers[k].ski, eebus_ac_ski)) { idx = (int)k; break; }
      }
      eebus_ac_try++;
      if (idx >= 0) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau - verbinde mit %s (Versuch %u von %u)"),
               eebus_ac_ski, eebus_ac_try, EEBUS_AC_MAX_TRY);
        if (EebusConnectPeer(idx) >= 0) {
   // Ab hier uebernimmt der Keep-Alive: er haelt die Verbindung und baut sie bei Abriss
   // selbst wieder auf. Ob der Versuch geglueckt ist, meldet der Slot in EEBusStatus.
   // v202: weiter mit dem naechsten gemerkten Partner (ohne neuen Scan, die Fundliste steht), nach 3 s
          eebus_ac_cur++; eebus_ac_try = 0;
          if (EebusAcNaechster()) { eebus_ac_next = Eebus126At(3000); eebus_ac_state = AC_SCAN; }
          return;
        }
      } else {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau - %s im Scan nicht gefunden (Versuch %u von %u)"),
               eebus_ac_ski, eebus_ac_try, EEBUS_AC_MAX_TRY);
   // v213: ein fehlender Partner darf die anderen nicht aufhalten (26.10.: alte, ersetzte HEMS-SKI an erster
   // Stelle -> 4 Versuche, rund 5 min, so lange stand das echte HEMS im Failsafe; FNN-Empfehlung: "zeitnaher
   // Wiederaufbau"). Steht ein spaeterer, nicht verbundener Partner in DIESEM Scan, kommt er sofort vor den fehlenden.
        for (uint8_t j = eebus_ac_cur + 1; j < eebus_ac_n; j++) {
          if (EebusAcConnected(eebus_ac_list[j])) { continue; }
          bool da = false;
          for (uint32_t k = 0; k < Eebus.peer_count; k++) {
            if (0 == strcasecmp(Eebus.peers[k].ski, eebus_ac_list[j])) { da = true; break; }
          }
          if (!da) { continue; }
          char tmp[41];
          strlcpy(tmp, eebus_ac_list[j], sizeof(tmp));
          strlcpy(eebus_ac_list[j], eebus_ac_list[eebus_ac_cur], sizeof(eebus_ac_list[j]));
          strlcpy(eebus_ac_list[eebus_ac_cur], tmp, sizeof(eebus_ac_list[eebus_ac_cur]));
          strlcpy(eebus_ac_ski, tmp, sizeof(eebus_ac_ski));
          eebus_ac_try = 0;   // der fehlende behaelt seinen Platz weiter hinten, seine Versuche beginnen dort neu
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau - zuerst %s (im Scan gefunden)"), tmp);
          eebus_ac_next = millis();
          return;   // naechster Durchgang verbindet ihn (Zustand bleibt AC_SCAN)
        }
      }
      if (eebus_ac_try >= EEBUS_AC_MAX_TRY) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau zu %s nach %u Versuchen erfolglos - weiter alle %u s"), eebus_ac_ski,
               (uint32_t)EEBUS_AC_MAX_TRY, (uint32_t)(EEBUS_AC_ENDLOS_MS / 1000));
        eebus_ac_cur++; eebus_ac_try = 0;   // v202: naechster Partner
        if (EebusAcNaechster()) { eebus_ac_next = Eebus126At(3000); eebus_ac_state = AC_WAIT; return; }
   // v241 (LPC-Testspezifikation EGConnection_003: "EG automatically reconnects after a black start"): nicht aufgeben -
   // alle Partner durch, dann von vorn, solange einer fehlt.
        eebus_ac_cur = 0;
        if (EebusAcNaechster()) { eebus_ac_next = Eebus126At(EEBUS_AC_ENDLOS_MS); eebus_ac_state = AC_WAIT; }
        return;
      }
      uint16_t warte_s = pgm_read_word(&kEebusAcRetryS[eebus_ac_try - 1]);
      eebus_ac_next  = Eebus126At((warte_s * 1000UL));
      eebus_ac_state = AC_WAIT;
      break;
    }
  }
}

// EEBusAutoConn            Zustand anzeigen
// EEBusAutoConn <sekunden> Wartezeit nach Netz+Uhr, bevor selbsttaetig gesucht wird (0 = aus)
// EEBusAutoConn -1         gemerkten Peer vergessen (kein Wiederaufbau mehr)
// EEBusAutoConn -<SKI>     v213: genau diesen Partner (SKI oder eindeutiger Anfang, mind. 4 Zeichen) aus der Liste nehmen
// v196: EEBusFailsafe                       Vorgaben und berechnete Failsafe-Werte anzeigen
//       EEBusFailsafe <16 x kW> <%> <s> <A> Geraeteliste (4 Ladepunkte, 4 WP, 4 Klima, 4 Speicher in kW),
//                                           Einspeise-Prozent, Mindestdauer, SLS-Nennstrom setzen und speichern
// Nach dem Setzen werden die Failsafe-Werte an alle verbundenen Anlagen neu geschrieben.
void CmndEebusFailsafe(void) {
  if (!eebus_fs_loaded) { EebusFsLoad(); }
  if (XdrvMailbox.data_len > 0) {
    char *p = XdrvMailbox.data, *e = nullptr;
    float v[19];
    int k = 0;
    for (; k < 19; k++) {
      v[k] = strtof(p, &e);
      if (e == p) { break; }
      p = e;
      while ((',' == *p) || (' ' == *p)) { p++; }
    }
    bool gut = (19 == k);   // v231 (Paket 5): nan/inf abweisen, Geraete 0..6553,5 kW (uint16 in 0,1 kW)
    for (int i = 0; gut && (i < 19); i++) { if (!std::isfinite(v[i])) { gut = false; } }
    for (int i = 0; gut && (i < 16); i++) { if ((v[i] < 0) || (v[i] > 6553.5f)) { gut = false; } }
    if (!gut || (v[16] < 0) || (v[16] > 100) || (v[17] < 7200) || (v[17] > 86400) || (v[18] < 0) || (v[18] > 1000)) {
      Response_P(PSTR("{\"%s\":\"16 Geraete in kW (0-6553,5), Prozent 0-100, Dauer 7200-86400 s, SLS 0-1000 A\"}"), XdrvMailbox.command);
      return;
    }
    for (int i = 0; i < 16; i++) { eebus_fs_dev[i] = (uint16_t)lroundf(((v[i] < 0) ? 0 : v[i]) * 10.0f); }
    eebus_fs_pct   = (uint8_t)v[16];
    eebus_fs_dur_s = (uint32_t)v[17];
    eebus_fs_sls_a = (uint16_t)v[18];
    EebusFsSave();
    for (int i = 0; i < EEBUS_MAX_CONN; i++) { EConn[i].lpc_fs_done = false; }   // neu schreiben
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Vorgaben gespeichert - Bezug %u W (Pmin), Einspeisung %u %%, %u s"),
           EebusFsBezugW(), eebus_fs_pct, eebus_fs_dur_s);
  }
  uint8_t n = 0;
  float g = 0;
  uint32_t pmin = EebusFsPminW(&n, &g);
  char d[120];
  int l = 0;
  for (int i = 0; i < 16; i++) {
    l += snprintf(d + l, sizeof(d) - l, "%s%u.%u", i ? "," : "", eebus_fs_dev[i] / 10, eebus_fs_dev[i] % 10);
  }
  Response_P(PSTR("{\"%s\":{\"Geraete\":[%s],\"N\":%u,\"Gzf\":%u,\"Pmin\":%u,\"Bezug\":%u,\"Prozent\":%u,\"DauerS\":%u,"
                  "\"Sls\":%u,\"Anschluss\":%u,\"Anlagen\":["),
             XdrvMailbox.command, d, n, (unsigned)lroundf(g * 100), pmin, EebusFsBezugW(), eebus_fs_pct, eebus_fs_dur_s,
             eebus_fs_sls_a, EebusFsAnschlussW());
  bool erst = true;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *c = &EConn[i];
    if (!c->active || (SME_DONE != c->sme)) { continue; }
    ResponseAppend_P(PSTR("%s{\"Ip\":\"%s\",\"Bezug\":%u,\"Nenn\":%u,\"Einspeisung\":%u,\"AnschlussGemeldet\":%u,\"Geschrieben\":%s}"),
                     erst ? "" : ",", c->peer_ip, EebusFsBezugCi(i), EebusFsNennW(i), EebusFsProdW(i),
                     c->pnom_ccons_ok ? EebusFsScaledW(c->pnom_ccons, c->pnom_ccons_sc) : 0, c->lpc_fs_done ? "true" : "false");
    erst = false;
  }
  ResponseAppend_P(PSTR("]}}"));
}

void CmndEebusAutoConn(void) {
  if (XdrvMailbox.data_len > 0) {
    long v = atol(XdrvMailbox.data);
    const char *sk = XdrvMailbox.data + (('-' == XdrvMailbox.data[0]) ? 1 : 0);
    bool hex = (strlen(sk) >= 4);
    for (const char *c = sk; hex && *c; c++) { if (!isxdigit((unsigned char)*c)) { hex = false; } }
    if (('-' == XdrvMailbox.data[0]) && (0 != strcmp(sk, "1")) && !hex) {   // sonst lief z. B. "-c" als 0 = Wiederaufbau aus
      ResponseCmndChar_P(PSTR("-1 = alle vergessen, -<SKI> = einen Partner (mind. 4 Hex-Zeichen)")); return;
    }
    if (('-' == XdrvMailbox.data[0]) && hex) {
   // v213: EEBusAutoConn -<SKI oder SKI-Anfang (mind. 4 Zeichen)> nimmt GENAU EINEN Partner aus der Liste
   // (z. B. eine ersetzte Identitaet); -1 loescht wie bisher alle.
      if (!eebus_ac_n) { EebusPeerRecall(); }
      int treffer = -1, anz = 0;
      for (uint8_t i = 0; i < eebus_ac_n; i++) {
        if (0 == strncasecmp(eebus_ac_list[i], sk, strlen(sk))) { treffer = i; anz++; }
      }
      if (1 != anz) { ResponseCmndChar_P(anz ? PSTR("SKI-Anfang nicht eindeutig") : PSTR("SKI nicht in der Partnerliste")); return; }
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Partner %s aus der Liste genommen"), eebus_ac_list[treffer]);
      for (uint8_t i = treffer; i + 1 < eebus_ac_n; i++) { strlcpy(eebus_ac_list[i], eebus_ac_list[i + 1], sizeof(eebus_ac_list[i])); }
      eebus_ac_n--;
      eebus_ac_list[eebus_ac_n][0] = '\0';
      if (eebus_ac_cur > treffer) { eebus_ac_cur--; }
      EebusPeerSave();
    } else if (v < 0) {
      EebusPeerForget();
    } else if (!Eebus126ArgNum(XdrvMailbox.data, 0, 3600, &v)) {   // v231 (Paket 5): "abc" ergab 0 = Wiederaufbau aus (im Flash)
      ResponseCmndChar_P(PSTR("Wartezeit 0..3600 s (ganze Zahl), -1 = Peer vergessen"));
      return;
    } else if (v <= 3600) {
      eebus_ac_delay_loaded = true;
      if (eebus_ac_delay_s != (uint16_t)v) {   // v208: nur bei Aenderung in den Flash
        eebus_ac_delay_s = (uint16_t)v;
#ifdef USE_UFILESYS
        char t[8];
        int l = snprintf(t, sizeof(t), "%u", eebus_ac_delay_s);
        TfsSaveFile(EEBUS_AC_FILE, (const uint8_t*)t, l);
#endif
      }
      if (0 == v) { eebus_ac_state = AC_DONE; }
    } else {
      ResponseCmndChar_P(PSTR("Wartezeit 0..3600 s, -1 = Peer vergessen"));
      return;
    }
  }
  const char *zst = (AC_IDLE == eebus_ac_state) ? "wartet auf Netz und Uhr" :
                    (AC_WAIT == eebus_ac_state) ? "Wartezeit laeuft" :
                    (AC_SCAN == eebus_ac_state) ? "sucht" : "abgeschlossen";
  if (!eebus_ac_n && (AC_IDLE != eebus_ac_state)) { EebusPeerRecall(); }   // v202: Liste auch nach Abschluss zeigen
  Response_P(PSTR("{\"%s\":{\"WarteS\":%u,\"Peer\":\"%s\",\"Zustand\":\"%s\",\"Versuche\":%u,\"Partner\":["),
             XdrvMailbox.command, eebus_ac_delay_s,
             eebus_ac_n ? eebus_ac_list[0] : "-", zst, eebus_ac_try);
  for (uint8_t i = 0; i < eebus_ac_n; i++) {
    ResponseAppend_P(PSTR("%s{\"Ski\":\"%s\",\"Verbunden\":%s}"), i ? "," : "", eebus_ac_list[i],
                     EebusAcConnected(eebus_ac_list[i]) ? "true" : "false");
  }
  ResponseAppend_P(PSTR("],\"Anzahl\":%u}}"), eebus_ac_n);
}

void CmndEebusConnect(void) {
   // EEBusConnect <idx>  (Index aus EEBusPeers/EEBusScan). Bestehende Verbindungen zu
   // ANDEREN Peers bleiben stehen (Multi-Connection, EEBUS_MAX_CONN Slots).
   // Ziel per SKI-Praefix / "hm" / (Notnagel) Index — siehe EebusPeerIdxArg.
  int idx = EebusPeerIdxArg(XdrvMailbox.data);
  if (idx < 0) { return; }
  if (!Eebus.cert_ok && !EebusEnsureCert(false)) {
    ResponseCmndChar_P(PSTR("Kein Zertifikat (EEBusCert)")); return;
  }
  EebusPeer *p = &Eebus.peers[idx];
  int ci = EebusConnectPeer(idx);
  if (ci < 0) {
    ResponseCmndChar_P((-2 == ci) ? PSTR("alle Slots belegt (EEBusDisconnect <idx>)")
                                  : PSTR("Kein Zertifikat (EEBusCert)"));
    return;
  }
  bool ok = (SHIP_CMI_OK == ESp->state) || (SME_CMI == ESp->sme);   // v242: CMI laeuft ohne Blockieren
  const char *stname = (ESp->state == SHIP_CMI_OK) ? "cmi_ok" :
                       (ESp->state == SHIP_WS_OK)  ? "ws_ok"  :
                       (ESp->state == SHIP_TLS_OK) ? "tls_ok" : "error";
  Response_P(PSTR("{\"%s\":{\"Ok\":%s,\"Slot\":%d,\"State\":\"%s\",\"Sme\":\"%s\",\"Ip\":\"%s\",\"Port\":%u,"
                  "\"PeerSki\":\"%s\",\"Error\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", ci, stname, EebusSmeName(ESp->sme),
             p->ip, p->port, p->ski, ESp->err);
}

// Kern von EEBusDisconnect (auch der Web-Link "Trennen" nutzt ihn). Wirkt auf den
// AKTUELLEN Slot (ESp) — Aufrufer setzt ESp vorher.
// KRITISCH (Crash-Befundspaet): den TLS-Client NIE sofort nach dem Close-Frame
// loeschen — lwip/W5500 arbeitet noch am Verbindungsabbau, free auf den noch benutzten
// Socket korrumpiert den Heap. Deshalb verzoegerter Teardown (wie die Callback-Frees).
void EebusDisconnectNow(void) {
  ESp->keepalive = false;   // Auto-Reconnect abschalten (bewusstes Trennen)
  ESp->reconnect_at = 0;
  ESp->peer_idx = -1;
   // Bewusstes Trennen legt den Wiederaufbau fuer DIESEN Lauf still — sonst wuerde eine noch
   // laufende Wartezeit die Verbindung gleich wieder herstellen.
   // ⚠️ Die MERKUNG bleibt dabei erhalten. Frueher wurde sie hier geloescht, mit der Begruendung,
   // ein bewusstes Trennen duerfe nicht beim naechsten Start stillschweigend rueckgaengig gemacht
   // werden. In der Praxis ist das falsch herum: "vor dem Update trennen" ist die HAEUFIGSTE Art
   // zu trennen, und dabei meint niemand "komm nicht wieder" — ausgerechnet nach einem
   // Firmware-Update, also dem Fall, fuer den der Wiederaufbau gedacht ist, blieb er damit stumm.
   // Eine Steuerbox muss sich auch nach einem Update selbst verbinden. Wer die Merkung wirklich
   // loswerden will, sagt es ausdruecklich: EEBusAutoConn -1 (zum blossen Stilllegen: 0).
  if (!ESp->via_srv && (0 == strcasecmp(eebus_ac_ski, ESp->peer_ski))) { eebus_ac_state = AC_DONE; }
  if (ESp->active && ESp->client && (SME_HELLO == ESp->sme)) {   // v242 (SHIP Z. 2292-2297): im Hello abbrechen
    EebusShipSendJson("{\"connectionHello\":[{\"phase\":\"aborted\"}]}", 1);
    delay(50);
  }
  if (ESp->active && (ESp->client || ESp->via_srv) &&   // auch Server-Slots verabschieden sich
      (SME_DONE == ESp->sme)) {   // v242 (SHIP Z. 3103-3105): connectionClose erst in der Datenphase
   // Sauberer SHIP-Abschied (13.4.7): connectionClose als End-Message (classifier 3)
     // v231 (Pruefung 3.9): reason "unspecific" statt "unplug" - die XSD kennt nur unspecific/removedConnection
     // (SHIP 13.4.8.1.1 Z. 3127-3138, ConnectionCloseReasonType); "unspecific" = voruebergehende Trennung.
    EebusShipSendJson("{\"connectionClose\":[{\"phase\":\"announce\"},{\"maxTime\":500},"
                      "{\"reason\":\"unspecific\"}]}", 3);
    delay(50);   // Close-Frame den TX-Weg (BearSSL->lwip) verlassen lassen
  }
  ESp->sme = SME_OFF;
  ESp->state = SHIP_IDLE;
   // Herzschlag-Ueberwachung stilllegen. Ohne Verbindung kommt kein Herzschlag mehr, und ein
   // stehengebliebener Zeitstempel laesst die Anzeige weiter altern, als wuerde noch ueberwacht:
   // gemessen zaehlte "PeerHerzschlagS" nach dem Trennen ungebremst von 43 auf 115 hoch. Ein
   // Fehlalarm kann daraus zwar nicht entstehen (ohne Datenphase wird gar nicht geprueft), aber
   // es waere genau die Sorte Anzeige, gegen die diese Ueberwachung gebaut wurde.
   // ⚠️ Zaehler und Zeitpunkt der Vorfaelle bleiben ABSICHTLICH stehen — sie sind die Vorfallsliste.
  ESp->peer_hb_at = 0; ESp->peer_hb_ctr = 0; ESp->peer_hb_tmo_s = 0; ESp->peer_hb_lost = false; ESp->hb_zurueck = false; ESp->hb_tx_at = 0;
  if (ESp->via_srv) {
    EebusSrvFree();   // Server-Verbindung komplett schliessen (Socket+Unlink)
  } else if (ESp->client) {
    EebusTeardownLater();   // Freigabe verzoegert im sicheren Tick (nie sofort)
  } else {
    EebusShipFree();   // nichts aktiv -> nur Puffer aufraeumen
  }
}

void CmndEebusDisconnect(void) {
   // EEBusDisconnect        -> ALLE Verbindungen trennen
   // EEBusDisconnect <idx>  -> nur den Peer <idx> (Index wie bei EEBusConnect/EEBusPeers)
  if (XdrvMailbox.data_len > 0) {
    int idx = EebusPeerIdxArg(XdrvMailbox.data);   // SKI-Praefix / "hm" / Index
    if (idx < 0) { return; }
    int ci = EebusConnBySki(Eebus.peers[idx].ski);
    if (ci < 0) { ResponseCmndChar_P(PSTR("keine Verbindung zu diesem Peer")); return; }
    ESp = &EConn[ci]; Eebus126ModeLoad();
    EebusDisconnectNow();
  } else {
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      ESp = &EConn[i]; Eebus126ModeLoad();
      EebusDisconnectNow();
    }
  }
  ResponseCmndChar_P(PSTR("Disconnected"));
}

const char* EebusLpcName(int s) {
  switch (s) {
    case LPC_BIND: return "bind"; case LPC_READDESC: return "readdesc";
    case LPC_READDATA: return "readdata";   // 
    case LPC_READCFG: return "readcfg";   // 
    case LPC_READEL: return "readel";   // 
    case LPC_WRITE: return "write"; case LPC_VERIFY: return "verify";
    case LPC_DONE: return "done"; case LPC_FAIL: return "fail"; default: return "idle";
  }
}

void CmndEebusStatus(void) {
   // Alle Verbindungs-Slots auflisten (Multi-Connection) + Rolle + LPC-Zustand
  Response_P(PSTR("{\"%s\":{\"Role\":%d,\"Hems\":%d,\"Active\":%d,\"Betriebslogbuch\":\"%s\",\"Slots\":["),   // v217: Logbuch-Zustand
             XdrvMailbox.command, eebus_role, eebus_hems_mode, EebusConnActive(), EebusLbAktiv() ? "aktiv" : "aus - keine SD-Karte");
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *cc = &EConn[i];
    const char *st = "idle";
    switch (cc->state) {
      case SHIP_TLS_OK: st = "tls_ok"; break;
      case SHIP_WS_OK:  st = "ws_ok"; break;
      case SHIP_CMI_OK: st = "cmi_ok"; break;
      case SHIP_ERROR:  st = "error"; break;
      default: st = "idle";
    }
   // Herzschlag der Gegenstelle: Alter des letzten, ihre Zusage, und die Vorfaelle.
   // Nur ausgeben, wenn es etwas zu sagen gibt — leere Slots sollen die Antwort nicht aufblaehen.
   // ⚠️ Zaehler und Zeitpunkt ueberleben einen Neustart NICHT (sie liegen im Arbeitsspeicher);
   // fuer eine laengere Beobachtung gehoert der Wert regelmaessig abgeholt.
    char hbinfo[160] = { 0 };
    if (cc->peer_hb_at || cc->hb_lost_cnt || cc->hb_tx_at) {   // v231: auch nur gesendet (Gegenstelle schickt keinen)
      snprintf_P(hbinfo, sizeof(hbinfo),
                 PSTR("\"PeerHerzschlagS\":%ld,\"HerzschlagGesendetS\":%ld,\"ZusageS\":%u,\"Verloren\":%u,\"VerlorenAm\":\"%s\","),
                 cc->peer_hb_at ? (long)((millis() - cc->peer_hb_at) / 1000) : -1L,
                 cc->hb_tx_at ? (long)((millis() - cc->hb_tx_at) / 1000) : -1L,
                 cc->peer_hb_tmo_s, cc->hb_lost_cnt, cc->hb_lost_at);
    }
    const int art = cc->peer_ski[0] ? EebusModeEffective(cc->peer_ski) : -1;   // v199: Betriebsart JE Platz
    ResponseAppend_P(PSTR("%s{\"Slot\":%d,\"State\":\"%s\",\"Sme\":\"%s\",\"Ip\":\"%s\",\"Port\":%u,\"Art\":\"%s\","
                          "\"PeerSki\":\"%s\",\"SkiQuelle\":\"%s\",\"PeerId\":\"%s\",\"Lpc\":\"%s\",\"LpcResult\":\"%s\",%s\"Error\":\"%s\"}"),
                     (i) ? "," : "", i, st, EebusSmeName(cc->sme), cc->peer_ip, cc->peer_port,
                     (1 == art) ? "hems" : (0 == art) ? "direkt" : "?",
                     cc->peer_ski, (1 == cc->ski_quelle) ? "Zertifikat" : (2 == cc->ski_quelle) ? "Adresse" : "Fundliste",   // v190
                     cc->peer_id, EebusLpcName(cc->lpc_state), cc->lpc_result, hbinfo, cc->err);
  }
  ResponseAppend_P(PSTR("]}}"));
}

// v242 (SHIP Z. 2125-2126): Liste der erlaubten Anrufer. EEBusTrust = Liste zeigen, EEBusTrust <SKI> = aufnehmen,
// EEBusTrust 0 = Liste leeren. Vertraut sind ausserdem gemerkte Partner und SKIs mit Betriebsart-Zuordnung.
void CmndEebusTrust(void) {
#ifdef USE_UFILESYS
  char buf[EEBUS_AC_MAX * 41 + 1] = { 0 };
  int n = 0;
  if (TfsFileExists(EEBUS_TRUST_FILE)) {
    File f = ffsp->open(EEBUS_TRUST_FILE, "r");
    if (f) { n = f.read((uint8_t*)buf, sizeof(buf) - 42); f.close(); }
    if (n < 0) { n = 0; }
    buf[n] = '\0';
  }
  if (XdrvMailbox.data_len > 0) {
    const char *a = XdrvMailbox.data;
    if ((1 == XdrvMailbox.data_len) && ('0' == a[0])) {
      if (TfsFileExists(EEBUS_TRUST_FILE)) { TfsDeleteFile(EEBUS_TRUST_FILE); }
      buf[0] = '\0'; n = 0;
    } else {
      bool ok = (40 == XdrvMailbox.data_len);
      for (int i = 0; ok && (i < 40); i++) { ok = isxdigit((unsigned char)a[i]); }
      if (!ok) { ResponseCmndChar_P(PSTR("SKI mit 40 Hex-Zeichen angeben")); return; }
      if (!EebusSkiInDatei(EEBUS_TRUST_FILE, a)) {
        if ((size_t)n + 41 >= sizeof(buf)) { ResponseCmndChar_P(PSTR("Liste voll")); return; }
        for (int i = 0; i < 40; i++) { buf[n++] = (char)tolower((unsigned char)a[i]); }
        buf[n++] = '\n'; buf[n] = '\0';
        TfsSaveFile(EEBUS_TRUST_FILE, (const uint8_t*)buf, n);
      }
    }
  }
  for (int i = 0; i < n; i++) { if ('\n' == buf[i]) { buf[i] = ' '; } }
  Response_P(PSTR("{\"%s\":\"%s\"}"), XdrvMailbox.command, buf);
#else
  ResponseCmndChar_P(PSTR("ohne Dateisystem nicht verfuegbar"));
#endif
}

// Peer-/Betriebsart fuer den §14a-Write — 0 = SteuVE (Wallbox, Waermepumpen-Gateway),
// 1 = HEMS (Energiemanager). Seit wird sie beim Verbinden aus dem gemeldeten GERAETETYP
// abgeleitet; dieser Schalter ist nur noch der Rueckfall fuer Verbindungen ohne Typangabe
// (z.B. EEBusConnectIp) und die Handhabe zum Ausprobieren.
// (Beschreibung richtiggestellt): die frueheren Texte behaupteten "HEMS = Write OHNE
// timePeriod". Das galt frueher und ist inzwischen FALSCH — beide Betriebsarten senden eine
// Geltungsdauer (HEMS 900 s, SteuVE 3600 s), ohne Dauer geht nur, wer ausdruecklich 0 angibt.
// Die alte Regel war ohnehin eine err7-Altlast: am 27./nachgewiesen, dass der
// Energiemanager eine Dauer annimmt, herunterzaehlt und selbsttaetig deaktiviert.
void CmndEebusHems(void) {
  if (XdrvMailbox.data_len > 0) {
    eebus_hems_def = (0 != XdrvMailbox.payload) ? 1 : 0;   // v231: Vorgabe fuer Verbindungen ohne Zuordnung/Typ
    Eebus126ModeLoad();
  }
  Response_P(PSTR("{\"%s\":{\"Mode\":%d,\"Meaning\":\"%s\"}}"), XdrvMailbox.command, eebus_hems_def,   // Gegenpruefung 06.11.: die gesetzte Vorgabe, nicht der aktuelle Slot
             eebus_hems_def ? "HEMS/Energiemanager (volle Lesesequenz, Heartbeat vor dem Write, "
                               "Vorgabedauer 900 s)"
                             : "SteuVE/Wallbox-Gateway (Minimalsequenz, Vorgabedauer 3600 s)");
}

// dreiwertig. "EEBusDelDur -1|0|1", ohne Argument nur abfragen.
// -1 automatisch (Default, richtig fuer den Betrieb) / 0 erzwungen aus / 1 erzwungen an.
// Die beiden Zwangsstellungen sind PRUEFWERKZEUG: mit 0 laesst sich die stille Fehlfunktion
// gezielt herstellen (so wurde sie bewiesen), mit 1 das Verhalten aelterer Fassungen.
void CmndEebusDelDur(void) {
  if (XdrvMailbox.data_len > 0) {
    int v = atoi(XdrvMailbox.data);
    if (v < 0)      { eebus_del_dur = -1; }
    else if (0 == v) { eebus_del_dur = 0; }
    else             { eebus_del_dur = 1; }
  }
  const char *bed = (0 == eebus_del_dur) ? "ERZWUNGEN AUS - alte Geltungsdauer wird geerbt (Pruefbetrieb!)"
                  : (1 == eebus_del_dur) ? "ERZWUNGEN AN - Write loescht die Geltungsdauer immer (Pruefbetrieb)"
                                         : "automatisch - loeschen nur, wenn der Auftrag keine Dauer traegt";
  Response_P(PSTR("{\"%s\":{\"DelDur\":%d,\"Hems\":%d,\"Meaning\":\"%s\"}}"), XdrvMailbox.command,
             eebus_del_dur, eebus_hems_mode, bed);
}

// Pro-SKI-Betriebsmodus (Geraet -> HEMS-Partner oder SteuVE-Partner). Loest den globalen
// EEBusHems-Schalter geraeteweise ab. Nutzung:
//   EEBusPeerMode <ski> hems|steuve   Zuordnung setzen (1/0 statt hems/steuve erlaubt)
//   EEBusPeerMode <ski> del           Zuordnung loeschen
//   EEBusPeerMode                     aktuelle Zuordnungen als JSON auflisten
// Beim Verbinden/Schreiben wird der Modus aus der peer-SKI aufgeloest und in eebus_hems_mode gesetzt.
void CmndEebusPeerMode(void) {
  if (XdrvMailbox.data_len > 0) {
    char args[80];
    strlcpy(args, XdrvMailbox.data, sizeof(args));
    char *modestr = strchr(args, ' ');
    if (modestr) {
      *modestr++ = '\0';
      while (' ' == *modestr) { modestr++; }
    }
    if (modestr && *modestr) {
      int mode;
      if      ((0 == strcasecmp(modestr, "hems"))   || (0 == strcmp(modestr, "1"))) { mode = 1; }
      else if ((0 == strcasecmp(modestr, "steuve")) || (0 == strcmp(modestr, "0"))) { mode = 0; }
      else if ((0 == strcasecmp(modestr, "del"))    || (0 == strcasecmp(modestr, "delete"))) { mode = -1; }
      else { ResponseCmndChar_P(PSTR("Modus: hems|steuve|del")); return; }
      if (!EebusModeSet(args, mode)) {
        ResponseCmndChar_P(PSTR("Zuordnung nicht moeglich (unbekannte SKI oder Tabelle voll)"));
        return;
      }
    }
  }
  Response_P(PSTR("{\"%s\":{\"Count\":%d,\"Modes\":["), XdrvMailbox.command, Eebus.mode_count);
  for (uint32_t i = 0; i < Eebus.mode_count; i++) {
    ResponseAppend_P(PSTR("%s{\"Ski\":\"%s\",\"Mode\":\"%s\"}"), (i) ? "," : "",
                     Eebus.mode_ski[i], Eebus.mode_val[i] ? "hems" : "steuve");
  }
  ResponseAppend_P(PSTR("]}}"));
}

// GRUNDTOR: EEBusRole 0 = nur Lesen (sicher, Default), 1 = Steuerbox (darf Limits schreiben).
// Rollenwechsel kuendigt uns per mDNS NEU an (GridConnectionHub<->ChargingStation), damit die
// Peer-Geraetelisten den geaenderten Typ sehen. Rolle VOR dem Verbinden setzen (Peer liest Typ beim Connect)!
void CmndEebusRole(void) {
  if (XdrvMailbox.data_len > 0) {
   // 0=nur Lesen, 1=Steuerbox GridGuard-Identitaet, 2=Steuerbox CEM-Identitaet
   // (EnergyManagementSystem/CEM wie die VR940-Bridge — Waermepumpen-Gateways liefern ihre
   // Discovery nur ihrem "EEBUS Energiemanager"; die Ladestation ignoriert den Typ, beides ok).
    uint8_t nr = ((XdrvMailbox.payload >= 1) && (XdrvMailbox.payload <= 3)) ? (uint8_t)XdrvMailbox.payload : 0;   // +Rolle 3
    if (nr != eebus_role) {
      eebus_role = nr;
      Eebus.advertised = false;   // erzwingt Neu-Ankuendigung mit neuem Typ (naechster Tick)
      mdns_service_remove("_ship", "_tcp");
      EebusMdnsAdvertise();
      EebusSrvFree();   // aktive EINGEHENDE Verbindung trennen — der Peer dockt
   // neu an und liest Typ/UseCase-Actor der NEUEN Rolle
    }
  }
  Response_P(PSTR("{\"%s\":{\"Role\":%d,\"Meaning\":\"%s\",\"MdnsType\":\"%s\"}}"), XdrvMailbox.command, eebus_role,
             (3 == eebus_role) ? "ControllableSystem-Anbieter fuer HEMS (stellt Limit bereit, Energiemanager liest)" :
             (2 == eebus_role) ? "Steuerbox mit CEM-Identitaet (schreibt Limits)" :
             (1 == eebus_role) ? "Steuerbox (schreibt Limits)" : "nur Lesen (Pruefmodus)",
             (3 == eebus_role) ? "ElectricitySupplySystem cat=1 (ControllableSystem-Aktor)" :
             (2 == eebus_role) ? "EnergyManagementSystem (CEM)" :
             (1 == eebus_role) ? "ElectricitySupplySystem cat=1" : EEBUS_ADV_TYPE);
}

// HEMS-SERVER-MODELL. EEBusProvide <watt> stellt ein §14a-Limit auf UNSEREM LoadControl
// bereit (das ein HEMS wie der Energiemanager liest/abonniert + an seine Verbraucher verteilt).
// EEBusProvide 0 = Bereitstellung beenden. Aenderung wird sofort an alle Abonnenten notifiziert.
void CmndEebusProvide(void) {
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2 (Steuerbox)")); return; }
  if (XdrvMailbox.data_len > 0) {
    uint32_t w = (uint32_t)atoi(XdrvMailbox.data);
    eebus_serve_active = (w > 0);
    if (w > 0) { eebus_serve_watt = w; }
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LoadControl-Server stellt jetzt %s bereit (%u W)"),
           eebus_serve_active ? "LIMIT" : "KEIN Limit", eebus_serve_watt);
    EebusServeLimitBroadcast();   // Abonnenten (HEMS) sofort informieren
  }
  int subs = 0;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) { if (EConn[i].active && EConn[i].lc_sub) { subs++; } }
  Response_P(PSTR("{\"%s\":{\"Active\":%s,\"Watt\":%u,\"Subscribers\":%d}}"),
             XdrvMailbox.command, eebus_serve_active ? "true" : "false", eebus_serve_watt, subs);
}

// LPC-Ziel-Override (Blind-Adressierung). EEBusTarget <ent> <feat> | EEBusTarget -1 = aus.
void CmndEebusTarget(void) {
  if (XdrvMailbox.data_len > 0) {
    int e = atoi(XdrvMailbox.data);
    char *sp = strchr(XdrvMailbox.data, ' ');
    if (e < 0) {
      eebus_tgt_ent = -1;
      eebus_tgt_feat = -1;
    } else if (nullptr != sp) {
      eebus_tgt_ent = (int8_t)e;
      eebus_tgt_feat = (int8_t)atoi(sp + 1);
    } else {
      ResponseCmndChar_P(PSTR("Nutzung: EEBusTarget <ent> <feat> | EEBusTarget -1"));
      return;
    }
  }
  if (eebus_tgt_ent >= 0) {
    Response_P(PSTR("{\"%s\":{\"Override\":true,\"Entity\":%d,\"Feature\":%d}}"),
               XdrvMailbox.command, eebus_tgt_ent, eebus_tgt_feat);
  } else {
    Response_P(PSTR("{\"%s\":{\"Override\":false,\"Meaning\":\"automatisch (Default/Discovery)\"}}"),
               XdrvMailbox.command);
  }
}

// Feature-Sweep — je Sekunde EIN unverbindlicher loadControlLimitDescription-Read an
// entity <ent>, feature <von>..<bis> des ERSTEN verbundenen Slots. Antworten (reply/result,
// auch Fehler!) erscheinen im Log/Mitschnitt = Beweis, dass die Adresse existiert/spricht.
void CmndEebusProbe(void) {
  int ent = -1, von = 0, bis = 0, mode = 0;
  if (XdrvMailbox.data_len > 0) {
    char *s1 = strchr(XdrvMailbox.data, ' ');
    char *s2 = (s1) ? strchr(s1 + 1, ' ') : nullptr;
    char *s3 = (s2) ? strchr(s2 + 1, ' ') : nullptr;
    ent = atoi(XdrvMailbox.data);
    if ((nullptr != s1) && (nullptr != s2)) {
      von = atoi(s1 + 1);
      bis = atoi(s2 + 1);
      if (nullptr != s3) { mode = atoi(s3 + 1); }   // 1 = Subscribe+Bind vor dem Read
    } else { ent = -1; }
  }
  if ((ent < 0) || (bis < von) || (bis - von > 30)) {
    if (0 == XdrvMailbox.data_len) {   // ohne Args: laufenden Sweep stoppen/anzeigen
      eebus_probe_ent = -1;
      ResponseCmndChar_P(PSTR("Sweep gestoppt"));
    } else {
      ResponseCmndChar_P(PSTR("Nutzung: EEBusProbe <ent> <featVon> <featBis> (max 30 Features)"));
    }
    return;
  }
  int ci = -1;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (EConn[i].active && (SME_DONE == EConn[i].sme)) { ci = i; break; }
  }
  if (ci < 0) { ResponseCmndChar_P(PSTR("keine verbundene Datenphase (erst Verbindung)")); return; }
  eebus_probe_ent = (int16_t)ent;
  eebus_probe_feat = (uint8_t)von;
  eebus_probe_end = (uint8_t)bis;
  eebus_probe_bind = (1 == mode);
  Response_P(PSTR("{\"%s\":{\"Slot\":%d,\"Ip\":\"%s\",\"Entity\":%d,\"Features\":\"%d..%d\",\"Mode\":\"%s\"}}"),
             XdrvMailbox.command, ci, EConn[ci].peer_ip, ent, von, bis,
             eebus_probe_bind ? "subscribe+bind+read" : "nur read");
}

// Slot-Zeiger auf den Peer <idx> setzen (verbunden?). Rueckgabe true, sonst Fehlerantwort schon gesetzt.
bool EebusSelectConnectedPeer(int idx) {
  if ((idx < 0) || (idx >= Eebus.peer_count)) { ResponseCmndChar_P(PSTR("Index 0..N-1 aus EEBusPeers noetig")); return false; }
  int ci = EebusConnBySki(Eebus.peers[idx].ski);
  if (ci < 0) {
   // EINGEHENDE Verbindungen fuehren eine Pseudo-SKI (=IP, mangels Client-Cert) ->
   // zusaetzlich per IP matchen, damit EEBusLpc/Release auch Server-Slots erreichen.
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      if (EConn[i].via_srv && EConn[i].active && (SME_DONE == EConn[i].sme) &&
          (0 == strcmp(EConn[i].peer_ip, Eebus.peers[idx].ip))) { ci = i; break; }
    }
  }
  if ((ci >= 0) && !(EConn[ci].active && (SME_DONE == EConn[ci].sme))) { ci = -1; }   // v215: nur stehende Verbindung (FNN S. 25)
  if (ci < 0) {
    EebusLbAbgewiesen(Eebus.peers[idx].ski, Eebus.peers[idx].ip, "nicht ausfuehrbar - Kommunikationsstoerung");
    ResponseCmndChar_P(PSTR("keine Verbindung zu diesem Peer (erst EEBusConnect)")); return false;
  }
  ESp = &EConn[ci]; Eebus126ModeLoad();
  return true;
}

// Peer-Auswahl INDEX-UNABHAENGIG. Der Scan-Listen-Index wechselt bei jedem Scan/Neustart (Energiemanager mal
// 0/1/2) -> EEBusLpc <idx> trifft dann das falsche Geraet. Die go-Vergleichs-Steuerbox adressiert ALLES per SKI
// (RemoteServiceForSKI / entity.Device().Ski()), nie per Index. Daher hier auswaehlbar per:
//  - "hm"/"auto"  -> die EINZIGE stehende Verbindung (der Normalfall beim Energiemanager-Test)
//  - SKI-Praefix  -> Slot, dessen peer_ski so beginnt (SKIs enthalten Hex-Buchstaben a-f)
//  - reine Zahl   -> alter Scan-Listen-Index (rueckwaertskompatibel)
bool EebusSelectPeerArg(const char *arg) {
  if ((0 == strcasecmp(arg, "hm")) || (0 == strcasecmp(arg, "auto"))) {
    int found = -1, n = 0;
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      if (EConn[i].active && (SME_DONE == EConn[i].sme)) { found = i; n++; }
    }
    if (1 == n) { ESp = &EConn[found]; Eebus126ModeLoad(); return true; }
    ResponseCmndChar_P(n ? PSTR("mehrere Verbindungen - SKI-Praefix angeben") : PSTR("keine stehende Verbindung"));
    return false;
  }
  bool is_ski = false;
  for (const char *p = arg; *p; p++) { if (strchr("abcdefABCDEF", *p)) { is_ski = true; break; } }
  if (is_ski) {
    size_t pl = strlen(arg);
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      if (EConn[i].active && (SME_DONE == EConn[i].sme) &&
          (0 == strncasecmp(EConn[i].peer_ski, arg, pl))) { ESp = &EConn[i]; Eebus126ModeLoad(); return true; }
    }
    EebusLbAbgewiesen(arg, "", "nicht ausfuehrbar - Kommunikationsstoerung");   // v215
    ResponseCmndChar_P(PSTR("keine stehende Verbindung mit dieser SKI"));
    return false;
  }
  return EebusSelectConnectedPeer(atoi(arg));   // reine Zahl -> alter Index-Weg
}

// v199: Ziel "alle" - dieselbe Vorgabe an jede stehende DIREKT gesteuerte Verbindung (Direktsteuerung mehrerer SteuVE),
// Ergebnis je Platz. Ein verbundener Energiemanager wird uebersprungen: er bekommt seinen gesamthaften Sollwert einzeln.
// watt_je_platz: Freigaben ohne Wertangabe nehmen wie bisher den letzten Wert DIESER Verbindung (lpc_value).
// v203: Beschreibung gelesen und sie fuehrt nur eine Bezugsgrenze -> das Geraet hat keine Einspeisegrenze
bool EebusOhneEinspeisung(void) { return ((ESp->lpc_lid_dir[0] >= 0) && (ESp->lpc_lid_dir[1] < 0)) || (0 == ESp->lpp_uc); }   // v243: LPP nicht angeboten
bool EebusIstAlle(const char *sel) { return (0 == strcasecmp(sel, "alle")) || (0 == strcasecmp(sel, "all")); }
void EebusAlleStart(uint32_t watt, bool active, uint8_t dir, long dauer, bool watt_je_platz) {
  int n = 0, hems = 0;   // "alle" = alle DIREKT gesteuerten; ein Energiemanager wird uebersprungen (FNN-Hinweis 2025 Kap. 3: Mischbetrieb zulaessig)
  Response_P(PSTR("{\"%s\":{\"Ziel\":\"alle\",\"DauerS\":%ld,\"Plaetze\":["), XdrvMailbox.command, dauer);
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (!EConn[i].active || (SME_DONE != EConn[i].sme)) { continue; }
    if (1 == EebusModeEffective(EConn[i].peer_ski)) { hems++; continue; }   // Energiemanager: eigener Gesamtwert (BK6 4.4 b)
    ESp = &EConn[i]; Eebus126ModeLoad();
    if ((1 == dir) && EebusOhneEinspeisung()) {   // v203: kein Paragraf 9 an diesem Geraet
      ResponseAppend_P(PSTR("%s{\"Platz\":%d,\"Ip\":\"%s\",\"Started\":false,\"Result\":\"keine Einspeisegrenze - uebersprungen\"}"),
                       n ? "," : "", i, ESp->peer_ip);
      n++;
      continue;
    }
    const uint32_t w = watt_je_platz ? ESp->lpc_val_dir[dir ? 1 : 0] : watt;   // 06.11.: Wert dieser Richtung
    const bool ok = EebusLpcStart(w, active, dir, dauer);
    ResponseAppend_P(PSTR("%s{\"Platz\":%d,\"Ip\":\"%s\",\"Watt\":%u,\"Started\":%s,\"Lpc\":\"%s\",\"Result\":\"%s\"}"),
                     n ? "," : "", i, ESp->peer_ip, w, ok ? "true" : "false", EebusLpcName(ESp->lpc_state), ESp->lpc_result);
    n++;
  }
  for (uint8_t k = 0; k < eebus_ac_n; k++) {   // v215: bekannte, direkt gesteuerte Partner ohne Verbindung (FNN S. 25)
    if (!EebusAcConnected(eebus_ac_list[k]) && (0 == EebusModeEffective(eebus_ac_list[k]))) {
      EebusLbAbgewiesen(eebus_ac_list[k], "", "nicht ausfuehrbar - Kommunikationsstoerung");
    }
  }
  ResponseAppend_P(PSTR("],\"Anzahl\":%d,\"EnergiemanagerUebersprungen\":%d}}"), n, hems);
  if (!n) { ResponseCmndChar_P(hems ? PSTR("keine direkt gesteuerte Anlage verbunden (nur Energiemanager)") : PSTR("keine stehende Verbindung")); }
}

// v231 (Pruefung Paket 5): Zahl aus einem Befehlsargument streng lesen. atoi/atol machten aus "abc" eine 0 - "EEBusLpc hm
// abc" schickte damit eine Grenze von 0 W. Erlaubt: optionales Minus, Ziffern, danach Leerzeichen oder Ende.
bool Eebus126ArgNum(const char *p, long lo, long hi, long *out) {
  if (nullptr == p) { return false; }
  while (' ' == *p) { p++; }
  const char *q = p;
  if ('-' == *q) { q++; }
  if ((*q < '0') || (*q > '9')) { return false; }
  char *e = nullptr;
  long v = strtol(p, &e, 10);
  if ((nullptr == e) || ((' ' != *e) && ('\0' != *e))) { return false; }
  if ((v < lo) || (v > hi)) { return false; }
  *out = v;
  return true;
}
#define EEBUS_CMD_W_MAX 1000000L   // v231: Obergrenze fuer Watt-Angaben in Befehlen (1 MW)

// EEBusLpc <idx|ski|hm|alle> <watt> [dauer_s] — BEZUGS-Limit schreiben (§14a EnWG).
// hiess frueher "EEBusLpc" (Name aus einer frueheren Fassung, als es nur den §14a-Test gab). Jetzt nach dem
// offiziellen Use-Case-Namen benannt: limitationOfPowerConsumption = LPC. Gegenstueck: EEBusLpp (§9 EEG).
void CmndEebusLimit(void) {
  char *space = (XdrvMailbox.data_len > 0) ? strchr(XdrvMailbox.data, ' ') : nullptr;
  if (nullptr == space) { ResponseCmndChar_P(PSTR("Nutzung: EEBusLpc <idx|ski|hm|alle> <watt> [dauer_s]  (Bezugsgrenze)")); return; }
   // erstes Token als STRING (Index ODER SKI-Praefix ODER "hm"/"auto") -> index-unabhaengig adressieren
  char sel[48]; size_t sl = (size_t)(space - XdrvMailbox.data);
  if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
  memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
  long wl = 0;
  if (!Eebus126ArgNum(space + 1, 0, EEBUS_CMD_W_MAX, &wl)) {   // v231: keine Zahl / negativ -> nicht senden
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Leistung ungueltig - erlaubt 0..%ld W (ganze Zahl)\"}}"), XdrvMailbox.command, EEBUS_CMD_W_MAX);
    return;
  }
  uint32_t watt = (uint32_t)wl;
   // optionale Geltungsdauer in Sekunden als drittes Argument.
   // OHNE Angabe gilt jetzt auch im HEMS-Betrieb eine Dauer — die Vorgabe 900 s. Bis ging
   // dort ohne drittes Argument gar keine Dauer mit; genau dieser Fall erbt eine abgelaufene
   // Geltungsdauer der Gegenstelle und wird wirkungslos, ohne dass es irgendwo auffaellt (
   // gemessen, Beleg oben bei eebus_del_dur). Der SteuVE-Betrieb behaelt seinen bisherigen Wert —
   // dieser Pfad ist an Ladestation/Waermepumpen-Gateway bewiesen und wird nicht angefasst.
  char *space2 = strchr(space + 1, ' ');
  long dauer = -1;   // v231: ohne Angabe Vorgabe erst NACH der Zielwahl (Betriebsart gilt je Verbindung, Gegenpruefung 06.11.)
   // Wertebereich duration 0..86400. Lieber hier abweisen als einen
   // Frame senden, den die Gegenstelle stillschweigend verwirft. 0 bleibt ausdruecklich erlaubt und
   // heisst "unbefristet"; dann loescht der Frame-Bau die alte Endzeit mit (Automatik, s.o.).
   // v231: erst pruefen, DANN merken (bis v230 wurde eine ungueltige Dauer schon vor der Pruefung gespeichert).
  if ((nullptr != space2) && !Eebus126ArgNum(space2 + 1, 0, EEBUS_LPC_DUR_MAX_S, &dauer)) {
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Dauer unzulaessig - erlaubt 0..%ld s (0 = unbefristet)\"}}"),
               XdrvMailbox.command, EEBUS_LPC_DUR_MAX_S);
    return;
  }
  if (nullptr != space2) { eebus_limit_dur_s = dauer; }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2 (Steuerbox)")); return; }
  if (EebusIstAlle(sel)) { EebusAlleStart(watt, true, 0, (dauer < 0) ? eebus_limit_dur_s : dauer, false); return; }   // v199; "alle" = nur SteuVE
  if (!EebusSelectPeerArg(sel)) { return; }
  if (dauer < 0) { dauer = eebus_hems_mode ? EEBUS_LPC_DUR_DEFAULT_S : eebus_limit_dur_s; }
  bool ok = EebusLpcStart(watt, true, 0, dauer);   // /consume (§14a), Dauer aus dem Befehl
   // "DauerS" meldet die TATSAECHLICH gesendete Dauer. Bis stand hier der gemerkte
   // globale Wert — der wich vom gesendeten ab und fuehrte beim Auswerten in die Irre.
  Response_P(PSTR("{\"%s\":{\"Started\":%s,\"Ip\":\"%s\",\"Watt\":%u,\"DauerS\":%ld,\"Lpc\":\"%s\",\"Result\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", ESp->peer_ip, watt, dauer,
             EebusLpcName(ESp->lpc_state), ESp->lpc_result);
}

// Freigabe: EEBusRelease <idx> — Limit an DIESER SteuVE deaktivieren (isLimitActive:false).
void CmndEebusRelease(void) {
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusRelease <idx|ski|hm|alle> [watt]")); return; }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2")); return; }
   // Ziel wie bei EEBusLpc/EEBusLpp per Index ODER SKI-Praefix ODER "hm" waehlen. Bis ging
   // hier NUR der Listenindex (alter EebusSelectConnectedPeer) — die SKI-Adressierung aus einer frueheren Fassung war nie
   // nachgezogen worden, weil in allen frueheren Tests mit EEBusReleaseAll freigegeben wurde. Am
   //fiel es auf: "EEBusRelease <ski>" antwortete nur "Index 0..N-1 noetig". Die Bedienseite
   // adressiert per SKI, deshalb muss es hier genauso gehen.
  {
    char sel[48];
    const char *sp = strchr(XdrvMailbox.data, ' ');
    size_t sl = (nullptr != sp) ? (size_t)(sp - XdrvMailbox.data) : strlen(XdrvMailbox.data);
    if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
    memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
    if (EebusIstAlle(sel)) {   // v199: ohne Wert nimmt jede Verbindung ihren letzten Wert
      long rw = 0;
      if ((nullptr != sp) && !Eebus126ArgNum(sp + 1, 0, EEBUS_CMD_W_MAX, &rw)) {   // v231
        Response_P(PSTR("{\"%s\":{\"Fehler\":\"Leistung ungueltig - erlaubt 0..%ld W\"}}"), XdrvMailbox.command, EEBUS_CMD_W_MAX);
        return;
      }
      EebusAlleStart((uint32_t)rw, false, 0, 0, nullptr == sp);
      return;
    }
    if (!EebusSelectPeerArg(sel)) { return; }
  }
   // optionaler Watt-Wert. Die Vergleichs-Steuerbox sendet beim Deaktivieren den value MIT
   // (isLimitActive:false, value:<letzter Wert>). Ohne Arg: den letzten lpc_value behalten (0 nach Boot).
   // Fuer den §5a-Zustands-Test (erst deaktivieren, dann aktivieren) Vergleichs-Steuerbox-identisch: EEBusRelease 0 8360.
  char *space = strchr(XdrvMailbox.data, ' ');
  uint32_t watt = ESp->lpc_val_dir[0];   // 06.11.: letzter §14a-Wert (bisher lpc_value = letzter Wert irgendeiner Richtung)
  if (nullptr != space) {
    long rw = 0;
    if (!Eebus126ArgNum(space + 1, 0, EEBUS_CMD_W_MAX, &rw)) {   // v231
      Response_P(PSTR("{\"%s\":{\"Fehler\":\"Leistung ungueltig - erlaubt 0..%ld W\"}}"), XdrvMailbox.command, EEBUS_CMD_W_MAX);
      return;
    }
    watt = (uint32_t)rw;
  }
   // EEBusRelease bleibt eindeutig die §14a-Freigabe (consume). Die Einspeise-Freigabe hat mit
   // EEBusLppFrei einen eigenen Befehl — sonst haenge das Ergebnis davon ab, welche Richtung zuletzt
   // geschrieben wurde, und der bewiesene §14a-Befehl waere nicht mehr vorhersagbar.
  bool ok = EebusLpcStart(watt, false, 0, 0);   // Freigabe braucht keine Geltungsdauer
  Response_P(PSTR("{\"%s\":{\"Started\":%s,\"Ip\":\"%s\",\"Watt\":%u,\"Lpc\":\"%s\",\"Result\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", ESp->peer_ip, watt,
             EebusLpcName(ESp->lpc_state), ESp->lpc_result);
}

// EEBusLpp <idx|ski|hm> <watt> — EINSPEISE-Limit schreiben (Limitation of Power
// Production). Kuerzel nach dem offiziellen Use-Case-Namen "limitationOfPowerProduction".
// EINGABE als BETRAG in Watt: "EEBusLpp hm 6888" = hoechstens 6888 W einspeisen. Auf den Draht geht
// der Wert nach LPP-Norm NEGATIV (-6888): passive Vorzeichenkonvention, Erzeugung ist negativ
// ([LPP-001]/[LPP-011]); ein positiver Wert wuerde nur den Bezug begrenzen und "SOLL abgelehnt werden".
// Ein versehentlich negativ eingegebener Wert wird als Betrag genommen -> beide Schreibweisen fuehren
// zum selben, norm-richtigen Frame. 0 W ist zulaessig (vollstaendige Einspeise-Abregelung).
// Laeuft ueber DIESELBE Verbindung, dasselbe Binding und denselben Heartbeat wie §14a — der Unterschied
// ist allein die limitId (produce statt consume), die aus der Peer-Beschreibung gelesen wird.
void CmndEebusLpp(void) {
  char *space = (XdrvMailbox.data_len > 0) ? strchr(XdrvMailbox.data, ' ') : nullptr;
  if (nullptr == space) { ResponseCmndChar_P(PSTR("Nutzung: EEBusLpp <idx|ski|hm|alle> <watt> [dauer_s]  (Einspeisegrenze, Betrag)")); return; }
  char sel[48]; size_t sl = (size_t)(space - XdrvMailbox.data);
  if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
  memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
  long w = 0;
  if (!Eebus126ArgNum(space + 1, -EEBUS_CMD_W_MAX, EEBUS_CMD_W_MAX, &w)) {   // v231: keine Zahl -> nicht senden
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Leistung ungueltig - Betrag 0..%ld W (ganze Zahl)\"}}"), XdrvMailbox.command, EEBUS_CMD_W_MAX);
    return;
  }
  if (w < 0) { w = -w; }   // Betrag; das Vorzeichen setzt der Frame-Bau
  char *space2 = strchr(space + 1, ' ');   // optionale Geltungsdauer in Sekunden
   // gleiche Regel wie bei §14a — ohne Angabe die Vorgabedauer, Bereich 0..86400 s.
   // Die Einspeisegrenze wird von derselben Zustandsmaschine geschrieben und erbt dieselbe
   // Gefahr: eine abgelaufene Geltungsdauer der Gegenstelle macht sie lautlos wirkungslos.
  long dauer = -1;   // v231: ohne Angabe Vorgabe erst NACH der Zielwahl (Betriebsart gilt je Verbindung, Gegenpruefung 06.11.)
  if ((nullptr != space2) && !Eebus126ArgNum(space2 + 1, 0, EEBUS_LPC_DUR_MAX_S, &dauer)) {   // v231: streng lesen
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Dauer unzulaessig - erlaubt 0..%ld s (0 = unbefristet)\"}}"),
               XdrvMailbox.command, EEBUS_LPC_DUR_MAX_S);
    return;
  }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2 (Steuerbox)")); return; }
  if (EebusIstAlle(sel)) { EebusAlleStart((uint32_t)w, true, 1, (dauer < 0) ? eebus_limit_dur_s : dauer, false); return; }   // v199; "alle" = nur SteuVE
  if (!EebusSelectPeerArg(sel)) { return; }
  if (dauer < 0) { dauer = eebus_hems_mode ? EEBUS_LPC_DUR_DEFAULT_S : eebus_limit_dur_s; }
  bool ok = EebusLpcStart((uint32_t)w, true, 1, dauer);   // 1 = produce (§9 EEG), Dauer aus dem Befehl
  Response_P(PSTR("{\"%s\":{\"Started\":%s,\"Ip\":\"%s\",\"Watt\":%ld,\"AufDraht\":%ld,\"DauerS\":%ld,\"Lpc\":\"%s\",\"Result\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", ESp->peer_ip, w, -w, dauer,
             EebusLpcName(ESp->lpc_state), ESp->lpc_result);
}

// EEBusMess <idx|ski|hm> — Messwerte und Anschluss-Kenngroessen ERNEUT abfragen.
// Bis geschah das ausschliesslich einmal beim Verbinden. Die Gegenstelle schickt von sich aus
// nur Leistung und Zaehlerstaende; ob sie Stroeme, Spannungen und Frequenz ueberhaupt fuehrt, laesst
// sich nur durch Nachfragen klaeren — und dafuer musste man bisher die Verbindung neu aufbauen.
// Es wird NUR GELESEN; an der Anlage aendert sich dadurch nichts.
// EEBusRead <ziel> <ent> <feat> <typ>   ein Feature gezielt auslesen
// EEBusRead                             die zuletzt aufgehobene Antwort ausgeben
//
// Die Selbstauskunft einer Gegenstelle sagt, WO sie etwas fuehrt. Was dort tatsaechlich steht,
// sagt sie erst auf Nachfrage — und genau diese Nachfrage fehlte bisher als Werkzeug. Der Typ
// wird mitgegeben (er steht in der Selbstauskunft), daraus ergibt sich die Lesefunktion.
const char* EebusReadFuncForType(const char *typ) {
  if (!typ || !typ[0])                                 { return nullptr; }
  if (0 == strcasecmp(typ, "Measurement"))             { return "measurementListData"; }
  if (0 == strcasecmp(typ, "ElectricalConnection"))    { return "electricalConnectionParameterDescriptionListData"; }
  if (0 == strcasecmp(typ, "DeviceConfiguration"))     { return "deviceConfigurationKeyValueListData"; }
  if (0 == strcasecmp(typ, "LoadControl"))             { return "loadControlLimitListData"; }
  if (0 == strcasecmp(typ, "DeviceClassification"))    { return "deviceClassificationManufacturerData"; }
  if (0 == strcasecmp(typ, "DeviceDiagnosis"))         { return "deviceDiagnosisStateData"; }
  return nullptr;   // unbekannter Typ -> lieber nichts senden als etwas raten
}

void CmndEebusRead(void) {
   // Ohne Argumente: die aufgehobene Antwort ausgeben. Anfuehrungszeichen werden ersetzt, damit
   // die Rohantwort in unsere JSON-Antwort passt, ohne sie zu zerlegen.
  if (0 == XdrvMailbox.data_len) {
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      if (EConn[i].active && EConn[i].rd_buf) {
   // Entity als Text ausgeben, damit eine Untereinheit als "6.1" erkennbar bleibt und nicht als 6
   // erscheint — genau diese Verkuerzung hat uns heute an die falsche Adresse geschickt.
        char ez[16];
        if (EConn[i].rd_pend_elist[0]) {
          snprintf(ez, sizeof(ez), "%s", EConn[i].rd_pend_elist);
          for (char *s = ez; *s; s++) { if (',' == *s) { *s = '.'; } }
        } else {
          snprintf(ez, sizeof(ez), "%d", EConn[i].rd_ent);
        }
        Response_P(PSTR("{\"%s\":{\"Ent\":\"%s\",\"Feat\":%d,\"Antwort\":\""),
                   XdrvMailbox.command, ez, EConn[i].rd_feat);
        for (const char *p = EConn[i].rd_buf; *p; p++) {
          char c = (('"' == *p) || ('\\' == *p)) ? '\'' : *p;
          ResponseAppend_P(PSTR("%c"), c);
        }
        ResponseAppend_P(PSTR("\"}}"));
        return;
      }
    }
    ResponseCmndChar_P(PSTR("keine Antwort aufgehoben - erst EEBusRead <ziel> <ent> <feat> <typ>"));
    return;
  }
   // Mit Argumenten: Read absetzen.
  char *p = XdrvMailbox.data;
  char *a1 = strchr(p, ' ');   if (!a1) { ResponseCmndChar_P(PSTR("Nutzung: EEBusRead <ziel> <ent> <feat> <typ>")); return; }
  *a1++ = '\0';
  if (!EebusSelectPeerArg(p)) { return; }
  if (SME_DONE != ESp->sme) { ResponseCmndChar_P(PSTR("keine SHIP-Verbindung (done)")); return; }
   // Die Entity wird als TEXT gelesen, damit Untereinheiten angegeben werden koennen: "6.1" meint
   // die Batterie IM Speicher und geht als [6,1] auf den Draht. Nur eine Zahl zu lesen hiess, dass
   // solche Einheiten gar nicht erreichbar waren (Antwort der Gegenstelle: errorNumber 4).
   // typ mit 63 Zeichen: die laengsten Funktionsnamen haben 48, mit 39 wurden sie abgeschnitten und
   // fielen anschliessend durch die eigene "endet auf Data"-Pruefung.
  int feat = -1, sel = -1; char entstr[16] = { 0 }, typ[64] = { 0 };
  int argn = sscanf(a1, "%15s %d %63s %d", entstr, &feat, typ, &sel);
  if (argn < 3) {
    ResponseCmndChar_P(PSTR("Nutzung: EEBusRead <ziel> <ent[.unter]> <feat> <typ> [kennung]")); return;
  }
  if (argn < 4) { sel = -1; }
   // "6.1" -> Liste "6,1"; nur Ziffern und Trenner uebernehmen (nichts anderes gehoert in eine Adresse)
  char elist[16] = { 0 }; size_t ei = 0;
  for (const char *s = entstr; *s && (ei + 1 < sizeof(elist)); s++) {
    if ((*s >= '0') && (*s <= '9'))            { elist[ei++] = *s; }
    else if (('.' == *s) || (',' == *s))       { elist[ei++] = ','; }
  }
  elist[ei] = '\0';
  if (!elist[0]) { ResponseCmndChar_P(PSTR("Entity fehlt (z.B. 8 oder 6.1)")); return; }
  int ent = atoi(elist);   // Hauptnummer — die Gegenstelle antwortet aus ihr, danach wird zugeordnet
   // Vier Kuerzel deckten nur ab, was wir schon kannten. Wer den FUNKTIONSNAMEN kennt, soll ihn
   // direkt angeben duerfen — sonst bleiben ganze Datensaetze unerreichbar, etwa die Kenngroessen
   // (electricalConnectionCharacteristicListData; dort steht die Bezugsgroesse fuer die
   // Prozentrechnung) oder die Messwert-BESCHREIBUNG neben den Messwerten. Erkennungsmerkmal:
   // ein SPINE-Funktionsname endet auf "Data".
  size_t tl = strlen(typ);
  const char *fn = ((tl > 4) && (0 == strcmp(typ + tl - 4, "Data"))) ? typ : EebusReadFuncForType(typ);
  if (!fn) { ResponseCmndChar_P(PSTR("unbekannter Feature-Typ (oder Funktionsname auf ...Data angeben)")); return; }
  if (ESp->rd_buf) { free(ESp->rd_buf); ESp->rd_buf = nullptr; }   // alte Antwort verwerfen
  ESp->rd_ent = ent; ESp->rd_feat = feat;
   // NUR vormerken, NICHT senden — s. Kommentar an rd_pend_ent.
  ESp->rd_pend_ent = ent; ESp->rd_pend_feat = feat;
  ESp->rd_pend_sel = sel;
  strlcpy(ESp->rd_pend_fn, fn, sizeof(ESp->rd_pend_fn));
  strlcpy(ESp->rd_pend_elist, elist, sizeof(ESp->rd_pend_elist));
  if (sel >= 0) {
    Response_P(PSTR("{\"%s\":{\"Gesendet\":true,\"Ent\":\"%s\",\"Feat\":%d,\"Funktion\":\"%s\",\"Kennung\":%d}}"),
               XdrvMailbox.command, elist, feat, fn, sel);
  } else {
    Response_P(PSTR("{\"%s\":{\"Gesendet\":true,\"Ent\":\"%s\",\"Feat\":%d,\"Funktion\":\"%s\"}}"),
               XdrvMailbox.command, elist, feat, fn);
  }
}

// v231: Text an einen Puffer anhaengen, ohne ueber das Ende zu laufen. ap += snprintf(...) lief bei langen Listen
// hinter das Ende (snprintf meldet die gewollte, nicht die geschriebene Laenge; sizeof - ap wird dann riesig).
size_t Eebus126CatAt(char *buf, size_t sz, size_t ap, const char *fmt, ...) {
  if ((nullptr == buf) || (0 == sz) || (ap >= sz - 1)) { return (sz ? sz - 1 : 0); }
  va_list va;
  va_start(va, fmt);
  int r = vsnprintf(buf + ap, sz - ap, fmt, va);
  va_end(va);
  if (r < 0) { return ap; }
  ap += (size_t)r;
  return (ap >= sz) ? sz - 1 : ap;
}

void CmndEebusMess(void) {
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusMess <idx|ski|hm>")); return; }
  if (!EebusSelectPeerArg(XdrvMailbox.data)) { return; }
  if (SME_DONE != ESp->sme) { ResponseCmndChar_P(PSTR("keine SHIP-Verbindung (done)")); return; }
   // JEDE bekannte Instanz fragen, nicht nur eine. Welche antwortet und welche mit
   // errorNumber 6 ablehnt, steht anschliessend in der Konsole — so finden wir heraus, WO die
   // Gegenstelle welche Groesse fuehrt, statt es zu erraten.
  int n = 0;
  char adr[160] = { 0 }; size_t ap = 0;   // v231: Eebus126CatAt haelt ap innerhalb des Puffers
  for (int i = 0; i < ESp->m_n; i++) {
    EebusSpineSendAddr("read", false, 0, "{\"measurementDescriptionListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, ESp->m_cli[i], ESp->m_ent[i], ESp->m_feat[i]); n++;
    EebusSpineSendAddr("read", false, 0, "{\"measurementListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, ESp->m_cli[i], ESp->m_ent[i], ESp->m_feat[i]); n++;
    ap = Eebus126CatAt(adr, sizeof(adr), ap, "%sM%d/%d", ap ? " " : "", ESp->m_ent[i], ESp->m_feat[i]);
  }
   // nur dort fragen, wo die Selbstauskunft die Kenngroessen auch auffuehrt.
  int ec_gefragt = 0;
  for (int i = 0; i < ESp->ec_n; i++) {
    if (!ESp->ec_char[i]) {
      ap = Eebus126CatAt(adr, sizeof(adr), ap, "%s(E%d/%d ohne)", ap ? " " : "",
                     ESp->ec_ent[i], ESp->ec_feat[i]);
      continue;
    }
    EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionCharacteristicListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, ESp->ec_cli[i], ESp->ec_ent[i], ESp->ec_feat[i]);
    n++; ec_gefragt++;
    ap = Eebus126CatAt(adr, sizeof(adr), ap, "%sE%d/%d", ap ? " " : "", ESp->ec_ent[i], ESp->ec_feat[i]);
  }
  Response_P(PSTR("{\"%s\":{\"Gesendet\":%d,\"AnzMess\":%d,\"AnzEc\":%d,\"EcMitKenngroessen\":%d,"
                  "\"Adressen\":\"%s\",\"Hinweis\":\"%s\"}}"),
             XdrvMailbox.command, n, ESp->m_n, ESp->ec_n, ec_gefragt, adr,
             n ? "Antworten kommen asynchron - danach EEBusData abfragen"
               : "keine Measurement-/EC-Adresse bekannt (erst verbinden)");
}

// EEBusStruct <idx|ski|hm> — die Selbstauskunft der Gegenstelle auswerten.
// Sie kommt beim Verbinden vollstaendig an und liegt inzwischen als Kopie im Speicher (lpc_disco) —
// bisher wurde daraus nur gelesen, was zum Schreiben noetig ist, der Rest blieb ungenutzt.
// Hier wird sie einmal durchgezaehlt: Entities mit Typ, Features mit Typ und Rolle.
// KEIN Netzverkehr — die Daten liegen bereits vor.
void CmndEebusStruct(void) {
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusStruct <idx|ski|hm> [Suchwort]")); return; }
  {
   // nur das ERSTE Wort ist das Geraet — dahinter darf ein Suchwort stehen.
    char sel[48];
    const char *sp = strchr(XdrvMailbox.data, ' ');
    size_t sl = (nullptr != sp) ? (size_t)(sp - XdrvMailbox.data) : strlen(XdrvMailbox.data);
    if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
    memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
    if (!EebusSelectPeerArg(sel)) { return; }
  }
  if (nullptr == ESp->lpc_disco) {
    ResponseCmndChar_P(PSTR("keine Selbstauskunft vorhanden - erst verbinden"));
    return;
  }
  const char *d = ESp->lpc_disco;

   // MIT Suchwort -> alle Features auflisten, die eine Funktion dieses Namens anbieten.
   //   EEBusStruct <ski> Characteristic   -> wo gibt es electricalConnectionCharacteristicListData?
   //   EEBusStruct <ski> measurement      -> wo gibt es Messwerte?
   // Das beantwortet die Frage "wo liegt was" aus der Selbstauskunft, ohne die Gegenstelle zu
   // fragen — und ohne die errorNumber-6-Ratespiele, die entstehen, wenn man aufs Geratewohl liest.
  const char *such = strchr(XdrvMailbox.data, ' ');
  if (nullptr != such) {
    such++;
    while (' ' == *such) { such++; }
    if ('\0' == *such) { such = nullptr; }
  }
  if (nullptr != such) {
    Response_P(PSTR("{\"%s\":{\"Suche\":\"%s\",\"Treffer\":["), XdrvMailbox.command, such);
    const char *q = d; int nt = 0;
    while ((nullptr != (q = strstr(q, "\"featureAddress\""))) && (nt < 16)) {
      const char *nq  = strstr(q + 16, "\"featureAddress\"");
      size_t blen = nq ? (size_t)(nq - q) : strlen(q);
   // Suchwort NUR innerhalb dieses Feature-Blocks suchen (kein strstr ueber die Grenze hinaus).
      const char *hit = nullptr;
      size_t sl = strlen(such);
      if (sl && (sl <= blen)) {
        for (size_t k = 0; k + sl <= blen; k++) {
          if (0 == strncasecmp(q + k, such, sl)) { hit = q + k; break; }
        }
      }
      if (hit) {
        int ffeat = -1;
        const char *ep = strstr(q, "\"entity\":[");
        const char *fp = strstr(q, "\"feature\":");
   // ⚠ Die Entity-Adresse ist eine LISTE. Bisher stand hier atoi("6,1") = 6 — und damit wurde in
   // der Oberflaeche ein Knopf "Entity 6" angeboten, den es so nicht gibt: die Gegenstelle
   // antwortete darauf mit errorNumber 4 "Ziel unbekannt" (29.07. an Speicher und PV gemessen).
   // Jetzt wird die vollstaendige Adresse ausgegeben, "6.1" statt 6.
        char fadr[16] = { 0 }; size_t fi = 0;
        if (ep && (!nq || (ep < nq))) {
          for (const char *s = ep + 10; *s && (']' != *s) && (fi + 1 < sizeof(fadr)); s++) {
            if ((*s >= '0') && (*s <= '9')) { fadr[fi++] = *s; }
            else if (',' == *s)             { fadr[fi++] = '.'; }
          }
        }
        fadr[fi] = '\0';
        if (fp && (!nq || (fp < nq))) { ffeat = atoi(fp + 10); }
        char ft[32] = { 0 }, ro[16] = { 0 };
        const char *tp = strstr(q, "\"featureType\":\"");
        const char *rp = strstr(q, "\"role\":\"");
        if (tp && (!nq || (tp < nq))) { EebusJsonStr(tp, "featureType", ft, sizeof(ft)); }
        if (rp && (!nq || (rp < nq))) { EebusJsonStr(rp, "role", ro, sizeof(ro)); }
        ResponseAppend_P(PSTR("%s{\"Ent\":\"%s\",\"Feat\":%d,\"Typ\":\"%s\",\"Rolle\":\"%s\"}"),
                         nt ? "," : "", fadr, ffeat, ft, ro);
        nt++;
      }
      q += 16;
    }
    ResponseAppend_P(PSTR("],\"Anzahl\":%d}}"), nt);
    return;
  }

  Response_P(PSTR("{\"%s\":{\"Entities\":["), XdrvMailbox.command);
   // Entities: jeder Block "entityAddress" ... "entityType". Ausgegeben wird "<Adresse> <Typ>".
   // ⚠ FRUEHER wurden nur die Typen gezaehlt und die LISTENPOSITION als Adresse ausgegeben. Das ist
   // falsch, sobald die Gegenstelle UNTEREINHEITEN fuehrt: eine Batterie in einem Speicher hat die
   // Adresse 6.1, belegt in der Liste aber einen eigenen Platz und verschiebt alles dahinter.
   // Real gemessen 29.07.: Listenplatz 7 war BatterySystem (Adresse 6.1), waehrend Adresse 7 das
   // CEM ist — genau die Einheit, in die das Limit geschrieben wird. Wer die Position als Adresse
   // liest, greift daneben. Deshalb wird die Adresse jetzt AUS DER SELBSTAUSKUNFT gelesen.
  const char *q = d; int ne = 0;
  while ((nullptr != (q = strstr(q, "\"entityAddress\""))) && (ne < 24)) {
    const char *nq = strstr(q + 15, "\"entityAddress\"");
    const char *ep = strstr(q, "\"entity\":[");
    const char *tp = strstr(q, "\"entityType\":\"");
   // Beides muss zu DIESEM Block gehoeren, sonst wird die Angabe des naechsten mitgeschleppt.
    if ((nullptr == ep) || (nullptr == tp) || (nq && ((ep > nq) || (tp > nq)))) { q += 15; continue; }
    char adr[24] = { 0 }; size_t ai = 0;
    for (const char *s = ep + 10; *s && (']' != *s) && (ai + 1 < sizeof(adr)); s++) {
      if ((*s >= '0') && (*s <= '9')) { adr[ai++] = *s; }
      else if (',' == *s)             { adr[ai++] = '.'; }   // "entity":[6,1] -> 6.1
    }
    adr[ai] = '\0';
    char et[32] = { 0 };
    EebusJsonStr(tp, "entityType", et, sizeof(et));
    if (et[0] && adr[0]) { ResponseAppend_P(PSTR("%s\"%s %s\""), ne ? "," : "", adr, et); ne++; }
    q += 15;
  }
  ResponseAppend_P(PSTR("],\"Features\":["));
   // Format "<Entity>.<Feature> <Typ>/<Rolle>", z.B. "8.11 Measurement/server".
   // Grund: derselbe Typ kommt mehrfach vor — einmal je Einheit, die ihn fuehrt. Ohne Adresse liest
   // sich die Liste wie eine Wiederholung ("dreimal Measurement"), obwohl darin die eigentliche
   // Auskunft steckt: Speicher, Netzanschlusspunkt und Erzeugung fuehren je eigene Messwerte.
  q = d; int nf = 0;
  while ((nullptr != (q = strstr(q, "\"featureAddress\""))) && (nf < 40)) {
    const char *nq = strstr(q + 16, "\"featureAddress\"");
    const char *ep = strstr(q, "\"entity\":[");
    const char *fp = strstr(q, "\"feature\":");
    const char *tp = strstr(q, "\"featureType\":\"");
    const char *rp = strstr(q, "\"role\":\"");
    if ((nullptr == ep) || (nullptr == fp) || (nullptr == tp)
        || (nq && ((ep > nq) || (fp > nq) || (tp > nq)))) { q += 16; continue; }
    char fadr[16] = { 0 }; size_t fi = 0;
    for (const char *s = ep + 10; *s && (']' != *s) && (fi + 1 < sizeof(fadr)); s++) {
      if ((*s >= '0') && (*s <= '9')) { fadr[fi++] = *s; }
      else if (',' == *s)             { fadr[fi++] = '.'; }
    }
    fadr[fi] = '\0';
    char ft[32] = { 0 }, ro[16] = { 0 };
    EebusJsonStr(tp, "featureType", ft, sizeof(ft));
    if (rp && (!nq || (rp < nq))) { EebusJsonStr(rp, "role", ro, sizeof(ro)); }
    if (ft[0] && fadr[0]) {
      ResponseAppend_P(PSTR("%s\"%s.%d %s%s%s\""), nf ? "," : "", fadr, atoi(fp + 10),
                       ft, ro[0] ? "/" : "", ro);
      nf++;
    }
    q += 16;
  }
   // Actors und Use Cases aus der Use-Case-Auskunft der Gegenstelle. Jeden Namen nur EINMAL —
   // dieselbe Rolle taucht dort fuer mehrere Entities auf, in der Klappliste waere das nur Rauschen.
  ResponseAppend_P(PSTR("],\"Actors\":["));
  int na = 0;
  if (nullptr != ESp->lpc_uc) {
    char gesehen[8][24] = { { 0 } };
    const char *u = ESp->lpc_uc;
    while ((nullptr != (u = strstr(u, "\"actor\":\""))) && (na < 8)) {
      char a[24] = { 0 };
      EebusJsonStr(u, "actor", a, sizeof(a));
      bool doppelt = false;
      for (int k = 0; k < na; k++) { if (0 == strcmp(gesehen[k], a)) { doppelt = true; break; } }
      if (a[0] && !doppelt) {
        strlcpy(gesehen[na], a, sizeof(gesehen[na]));
        ResponseAppend_P(PSTR("%s\"%s\""), na ? "," : "", a);
        na++;
      }
      u += 9;
    }
  }
  ResponseAppend_P(PSTR("],\"UseCases\":["));
  int nu = 0;
  if (nullptr != ESp->lpc_uc) {
    char gesehen[10][48] = { { 0 } };
    const char *u = ESp->lpc_uc;
    while ((nullptr != (u = strstr(u, "\"useCaseName\":\""))) && (nu < 10)) {
      char a[48] = { 0 };
      EebusJsonStr(u, "useCaseName", a, sizeof(a));
      bool doppelt = false;
      for (int k = 0; k < nu; k++) { if (0 == strcmp(gesehen[k], a)) { doppelt = true; break; } }
      if (a[0] && !doppelt) {
        strlcpy(gesehen[nu], a, sizeof(gesehen[nu]));
        ResponseAppend_P(PSTR("%s\"%s\""), nu ? "," : "", a);
        nu++;
      }
      u += 15;
    }
  }
  ResponseAppend_P(PSTR("],\"AnzEntities\":%d,\"AnzFeatures\":%d,\"AnzActors\":%d,\"AnzUseCases\":%d,"
                        "\"Bytes\":%u,\"UcBytes\":%u}}"),
                   ne, nf, na, nu, (unsigned)strlen(d),
                   (unsigned)(ESp->lpc_uc ? strlen(ESp->lpc_uc) : 0));
}

// EEBusData — die mitgelesenen Nutzdaten je stehender Verbindung ausgeben (fuer die
// Bedienoberflaeche). Zahlen kommen als Paar aus Wert und Zehnerexponent heraus, genau wie sie auf
// dem Draht stehen ("scaledNumber") — so wird nichts gerundet und die Anzeige rechnet selbst.
// Fehlende Werte werden WEGGELASSEN statt auf 0 gesetzt: "noch nichts empfangen" ist etwas anderes
// als "null Watt", und eine Steuerbox darf da nichts erfinden.
void CmndEebusData(void) {
  Response_P(PSTR("{\"%s\":{\"Peers\":["), XdrvMailbox.command);
  int n = 0;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    EebusConn *cc = &EConn[i];
    if (!cc->active || (SME_DONE != cc->sme)) { continue; }
    ResponseAppend_P(PSTR("%s{\"Ski\":\"%s\",\"Ip\":\"%s\""), (n) ? "," : "", cc->peer_ski, cc->peer_ip);
    static const char kName[7][12] = { "", "Power", "FeedIn", "Consum", "Current", "Voltage", "Freq" };
    for (int c = 1; c <= 6; c++) {
      if (cc->m_have[c]) {
        ResponseAppend_P(PSTR(",\"%s\":[%ld,%d]"), kName[c], cc->m_val[c], cc->m_sc[c]);
      }
    }
    for (int k = 0; k < 2; k++) {   // v197: "Current3"/"Voltage3" = [[Zahl,Exp] je Phase] (null = fehlt)
      if (!cc->m3_have[k][0] && !cc->m3_have[k][1] && !cc->m3_have[k][2]) { continue; }
      ResponseAppend_P(PSTR(",\"%s3\":["), k ? "Voltage" : "Current");
      for (int f = 0; f < 3; f++) {
        if (cc->m3_have[k][f]) { ResponseAppend_P(PSTR("%s[%ld,%d]"), f ? "," : "", cc->m3_val[k][f], cc->m3_sc[k][f]); }
        else                   { ResponseAppend_P(PSTR("%snull"), f ? "," : ""); }
      }
      ResponseAppend_P(PSTR("]"));
    }
    if (cc->mll_have[0] || cc->mll_have[1] || cc->mll_have[2]) {   // v243: L1-L2, L2-L3, L3-L1
      ResponseAppend_P(PSTR(",\"VoltageLL\":["));
      for (int f = 0; f < 3; f++) {
        if (cc->mll_have[f]) { ResponseAppend_P(PSTR("%s[%ld,%d]"), f ? "," : "", cc->mll_val[f], cc->mll_sc[f]); }
        else                 { ResponseAppend_P(PSTR("%snull"), f ? "," : ""); }
      }
      ResponseAppend_P(PSTR("]"));
    }
    if (cc->fs_cons_ok) { ResponseAppend_P(PSTR(",\"FsCons\":[%ld,%d]"), cc->fs_cons, cc->fs_cons_sc); }
    if (cc->fs_prod_ok) { ResponseAppend_P(PSTR(",\"FsProd\":[%ld,%d]"), cc->fs_prod, cc->fs_prod_sc); }
    if (cc->fs_dur[0])  { ResponseAppend_P(PSTR(",\"FsDur\":\"%s\""), cc->fs_dur); }
   // die norm-richtigen Nennleistungen, sofern die Gegenstelle sie herausgibt.
    if (cc->pnom_prod_ok) { ResponseAppend_P(PSTR(",\"PnomProd\":[%ld,%d]"), cc->pnom_prod, cc->pnom_prod_sc); }
    if (cc->pnom_cprod_ok) { ResponseAppend_P(PSTR(",\"PnomProdVertrag\":[%ld,%d]"), cc->pnom_cprod, cc->pnom_cprod_sc); }   // v196
    if (cc->pnom_ccons_ok) { ResponseAppend_P(PSTR(",\"PnomConsVertrag\":[%ld,%d]"), cc->pnom_ccons, cc->pnom_ccons_sc); }   // v196
    if (cc->pnom_cons_ok) { ResponseAppend_P(PSTR(",\"PnomCons\":[%ld,%d]"), cc->pnom_cons, cc->pnom_cons_sc); }
    if (cc->plf_ok)     { ResponseAppend_P(PSTR(",\"Plf\":[%ld,%d]"), cc->plf, cc->plf_sc); }
   // Werte ANDERER Einheiten (Speicher, Erzeugung, ...) mit Einheit, Kennung und der Bedeutung, wie
   // die Gegenstelle sie nennt. Bewusst NICHT in die Felder oben gemischt und bewusst NICHT
   // umbenannt: "stateOfCharge" ist ihr Wort, nicht unsere Deutung.
    if (cc->mo_n) {
      int mn = 0;
      ResponseAppend_P(PSTR(",\"Einheiten\":["));
      for (uint8_t k = 0; k < cc->mo_n; k++) {
        if (!cc->mo_have[k]) { continue; }   // nur Kennungen, zu denen ein Wert eintraf
        ResponseAppend_P(PSTR("%s{\"Ent\":%d,\"Id\":%u,\"Scope\":\"%s\",\"Unit\":\"%s\",\"V\":[%ld,%d]}"),
                         (mn) ? "," : "", cc->mo_ent[k], (unsigned)cc->mo_id[k],
                         cc->mo_scope[k], cc->mo_unit[k], cc->mo_v[k], cc->mo_sc[k]);
        mn++;
      }
      ResponseAppend_P(PSTR("]"));
    }
    for (int d = 0; d < 2; d++) {
      if (cc->lim_act[d] >= 0) {
   // "Dauer" ist jetzt die MITLAUFENDE Restzeit, nicht mehr der eingefrorene Text aus der
   // letzten Rueckmeldung. Grundlage: zuletzt gemeldete Restlaufzeit minus seither vergangene
   // Zeit. Kann die Dauer nicht gedeutet werden (-1), bleibt es beim Rohtext der Gegenstelle —
   // lieber ihr Wort als eine erfundene Zahl. Zusaetzlich "RestS" in Sekunden, damit eine
   // Anzeige selbst rechnen kann, ohne den Text zerlegen zu muessen.
        char dtxt[24] = { 0 }; long rest = -1;
        if (cc->lim_dur[d][0]) {
          if (cc->lim_dur_s[d] >= 0) {
            long weg = (long)((millis() - cc->lim_dur_at[d]) / 1000UL);
            rest = cc->lim_dur_s[d] - weg;
            if (rest < 0) { rest = 0; }
            EebusSecsToIso(rest, dtxt, sizeof(dtxt));
          } else {
            strncpy(dtxt, cc->lim_dur[d], sizeof(dtxt) - 1);
          }
        }
        ResponseAppend_P(PSTR(",\"Lim%d\":{\"Aktiv\":%s,\"Wert\":[%ld,%d]%s%s%s"),
                         d, cc->lim_act[d] ? "true" : "false", cc->lim_val[d], cc->lim_sc[d],
                         dtxt[0] ? ",\"Dauer\":\"" : "",
                         dtxt[0] ? dtxt : "",
                         dtxt[0] ? "\"" : "");
        if (rest >= 0) { ResponseAppend_P(PSTR(",\"RestS\":%ld"), rest); }
        ResponseAppend_P(PSTR("}"));   // schliesst das Lim<d>-Objekt
      }
    }
    ResponseAppend_P(PSTR("}"));
    n++;
  }
  ResponseAppend_P(PSTR("]}}"));
}

// EEBusAnmeld 0|1 — Anmeldung nach dem Verbinden selbsttaetig abschliessen.
//   1 = Default: nach dem Onboarding und drei Heartbeats schreibt der Treiber je eine FREIGABE
//       fuer Bezug und Einspeisung. Die Gegenstelle verlaesst damit den Zustand "init" und fuehrt
//       uns als Steuerbox — ohne dass irgendetwas begrenzt wird ([LPC-905]/[LPP-905]).
//   0 = aus: es wird nichts von selbst geschrieben.
// Wirkt nur im HEMS-Modus (an einer einzelnen SteuVE wuerde eine Freigabe ein dort gesetztes
// Limit aufheben). Der Schalter wirkt ab der naechsten Anmeldung, nicht rueckwirkend.
void CmndEebusAnmeld(void) {
  if (XdrvMailbox.data_len > 0) {
    eebus_auto_reg = (0 != XdrvMailbox.payload) ? 1 : 0;
  }
  Response_P(PSTR("{\"%s\":{\"Anmeldung\":%d,\"Hems\":%d,\"Bedeutung\":\"%s\"}}"), XdrvMailbox.command,
             eebus_auto_reg, eebus_hems_mode,
             eebus_auto_reg ? "nach dem Verbinden je eine Freigabe fuer Bezug und Einspeisung (kein Limit)"
                            : "nichts wird von selbst geschrieben");
}

// EEBusLppFrei <idx|ski|hm> [watt] — Einspeise-Limit deaktivieren (isLimitActive:false).
// Gegenstueck zu EEBusRelease, aber auf der produce-Grenze. Wie bei §14a wird der letzte Wert
// mitgesendet (Referenz-Verhalten), nicht 0.
void CmndEebusLppFrei(void) {
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusLppFrei <idx|ski|hm|alle> [watt]")); return; }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2")); return; }
  char *space = strchr(XdrvMailbox.data, ' ');
  char sel[48]; size_t sl = (nullptr != space) ? (size_t)(space - XdrvMailbox.data) : strlen(XdrvMailbox.data);
  if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
  memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
  if (EebusIstAlle(sel)) {   // v199
    long wa = 0;
    if ((nullptr != space) && !Eebus126ArgNum(space + 1, -EEBUS_CMD_W_MAX, EEBUS_CMD_W_MAX, &wa)) {   // v232: streng wie EEBusLpp
      Response_P(PSTR("{\"%s\":{\"Fehler\":\"Leistung ungueltig - erlaubt -%ld..%ld W\"}}"), XdrvMailbox.command, EEBUS_CMD_W_MAX, EEBUS_CMD_W_MAX);
      return;
    }
    if (wa < 0) { wa = -wa; }
    EebusAlleStart((uint32_t)wa, false, 1, 0, nullptr == space);
    return;
  }
  if (!EebusSelectPeerArg(sel)) { return; }
  long w = (long)ESp->lpc_val_dir[1];   // 06.11.: letzter §9-Wert, nicht der letzte irgendeiner Richtung
  if ((nullptr != space) && !Eebus126ArgNum(space + 1, -EEBUS_CMD_W_MAX, EEBUS_CMD_W_MAX, &w)) {   // v232: "abc" ergab mit atol 0 W
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Leistung ungueltig - erlaubt -%ld..%ld W\"}}"), XdrvMailbox.command, EEBUS_CMD_W_MAX, EEBUS_CMD_W_MAX);
    return;
  }
  if (w < 0) { w = -w; }
  bool ok = EebusLpcStart((uint32_t)w, false, 1, 0);
  Response_P(PSTR("{\"%s\":{\"Started\":%s,\"Ip\":\"%s\",\"Watt\":%ld,\"Lpc\":\"%s\",\"Result\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", ESp->peer_ip, w,
             EebusLpcName(ESp->lpc_state), ESp->lpc_result);
}

// Not-Freigabe: EEBusReleaseAll — an ALLEN verbundenen SteuVE ein aktives Limit deaktivieren.
void CmndEebusReleaseAll(void) {
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2")); return; }
  int n = 0;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    ESp = &EConn[i]; Eebus126ModeLoad();
    if ((SME_DONE == ESp->sme) && ESp->peer_dev[0]) {   // nur verbundene Peers
      if (EebusLpcStart(ESp->lpc_val_dir[0], false, 0, 0)) { n++; }   // Not-Freigabe wirkt auf die §14a-Grenze; v232: mit dem letzten eigenen Wert wie EEBusRelease (bisher 0 W)
    }
  }
  Response_P(PSTR("{\"%s\":{\"Freigabe_gestartet\":%d}}"), XdrvMailbox.command, n);
}

// M3: eigene mDNS-SHIP-Ankuendigung (an)werfen — danach erscheinen wir in Energiemanager bzw. Hersteller-App
// unter "Verfuegbare Geraete". EEBusAdvertise 0 = Ankuendigung wieder entfernen.
void CmndEebusAdvertise(void) {
  if ((XdrvMailbox.data_len > 0) && (0 == XdrvMailbox.payload)) {
    mdns_service_remove("_ship", "_tcp");
    Eebus.advertised = false;
    ResponseCmndChar_P(PSTR("Ankuendigung entfernt"));
    return;
  }
  if (EebusMdnsAdvertise()) {
    Response_P(PSTR("{\"%s\":{\"Advertised\":true,\"Id\":\"%s\",\"Type\":\"%s\",\"Ski\":\"%s\",\"Port\":%d}}"),
               XdrvMailbox.command, eebus_adv_id, EEBUS_ADV_TYPE, Eebus.own_ski, EEBUS_ADV_PORT);
  } else {
    ResponseCmndChar_P(PSTR("Failed"));
  }
}

// Mitschnitt: EEBusLog 0 = aus, 1 = RAM-Aufnahme an, 2 = RAM-Puffer auf SD schreiben
// (Dump nur bei getrennter Verbindung — SD-Zugriff bei offener TLS-Verbindung crasht, s. EebusShipLog).
void CmndEebusLog(void) {
  if (XdrvMailbox.data_len > 0) {
    if (2 == XdrvMailbox.payload) { EebusLogDump(); return; }
    eebus_ram_log = (XdrvMailbox.payload != 0);
  }
  Response_P(PSTR("{\"%s\":{\"RamLog\":\"%s\",\"Bytes\":%u,\"Full\":%s}}"), XdrvMailbox.command,
             eebus_ram_log ? "ON" : "OFF", (uint32_t)eebus_log_len, eebus_log_full ? "true":"false");
}

// Connect-Experiment umschalten (siehe eebus_open_mode). Wirkt auf NEU aufgebaute Verbindungen.
void CmndEebusOpen(void) {
  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload >= 0) && (XdrvMailbox.payload <= 1)) {
    eebus_open_mode = (uint8_t)XdrvMailbox.payload;
  }
  Response_P(PSTR("{\"%s\":{\"Mode\":%u,\"Text\":\"%s\"}}"), XdrvMailbox.command, eebus_open_mode,
             (0 == eebus_open_mode) ? "reaktiv adressiert" : "adresslose Eroeffnung");
}

// EEBusConnectIp <ip>[:port] | [ipv6]:port | ipv6 — verbindet zu einer beliebigen Adresse ohne vorherigen Scan.
// v231 (Paket 5): Der Eintrag wird an die Fundliste ANGEHAENGT (bzw. ein frueherer manueller Eintrag wiederverwendet).
// Bis v230 wurde peers[0] ueberschrieben - das war meist ein echtes, verbundenes Geraet, dessen Adresse damit weg war.
// IPv6 geht jetzt auch. Die SKI kommt aus dem Zertifikat der Gegenstelle (EebusShipConnect, "manual"); bis v230
// scheiterte jede manuelle Verbindung an der Zertifikatspruefung (v192), weil "manual" nie zur echten SKI passt.
void CmndEebusConnectIp(void) {
  if (XdrvMailbox.data_len < 3) { ResponseCmndChar_P(PSTR("Nutzung: EEBusConnectIp ip[:port] | [ipv6]:port")); return; }
  if (!Eebus.cert_ok && !EebusEnsureCert(false)) { ResponseCmndChar_P(PSTR("Kein Zertifikat")); return; }
  if (Eebus.scan) { ResponseCmndChar_P(PSTR("Suche laeuft - gleich noch einmal")); return; }
  char buf[64]; strlcpy(buf, XdrvMailbox.data, sizeof(buf));
  uint16_t port = 4712;
  char *adr = buf;
  if ('[' == buf[0]) {   // [ipv6]:port
    char *zu = strchr(buf, ']');
    if (!zu) { ResponseCmndChar_P(PSTR("IPv6 in eckigen Klammern: [adresse]:port")); return; }
    *zu = '\0';
    adr = buf + 1;
    if (':' == zu[1]) {
      long pv = 0;
      if (!Eebus126ArgNum(zu + 2, 1, 65535, &pv)) { ResponseCmndChar_P(PSTR("Port 1..65535")); return; }
      port = (uint16_t)pv;
    }
  } else {
    char *c1 = strchr(buf, ':');
    if (c1 && (c1 == strrchr(buf, ':'))) {   // genau ein Doppelpunkt: ipv4:port
      *c1 = '\0';
      long pv = 0;
      if (!Eebus126ArgNum(c1 + 1, 1, 65535, &pv)) { ResponseCmndChar_P(PSTR("Port 1..65535")); return; }
      port = (uint16_t)pv;
    }   // mehrere Doppelpunkte: IPv6 ohne Port
  }
  IPAddress chk;
  if (!adr[0] || !chk.fromString(adr)) { ResponseCmndChar_P(PSTR("Adresse ungueltig")); return; }
  int pi = -1;
  for (uint32_t k = 0; k < Eebus.peer_count; k++) {   // frueheren manuellen Eintrag wiederverwenden
    if (0 == strcmp(Eebus.peers[k].ski, "manual")) { pi = (int)k; break; }
  }
  if ((pi < 0) && (Eebus.peer_count >= EEBUS_MAX_PEERS)) { ResponseCmndChar_P(PSTR("Fundliste voll")); return; }
  int ci = EebusConnAlloc("manual");   // Debug-Verbindung in eigenen Slot
  if (ci < 0) { ResponseCmndChar_P(PSTR("alle Slots belegt (EEBusDisconnect)")); return; }
  if (pi < 0) { pi = (int)Eebus.peer_count++; }   // anhaengen, nie umordnen
  ESp = &EConn[ci]; Eebus126ModeLoad();
  EebusPeer *p = &Eebus.peers[pi];
  memset(p, 0, sizeof(EebusPeer));
  if (strchr(adr, ':')) { strlcpy(p->ip6, adr, sizeof(p->ip6)); }
  else                  { strlcpy(p->ip, adr, sizeof(p->ip)); }
  strlcpy(p->path, "/ship/", sizeof(p->path));
  strlcpy(p->ski, "manual", sizeof(p->ski));
  p->port = port;
  bool ok = EebusShipConnect(pi);
  Response_P(PSTR("{\"%s\":{\"Ok\":%s,\"Ip\":\"%s\",\"Port\":%u,\"Ski\":\"%s\",\"Error\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", adr, port, ESp->peer_ski, ESp->err);
}

/*********************************************************************************************\
 * Web-Anzeige
\*********************************************************************************************/

#ifdef USE_WEBSERVER
void EebusWebSensor(void) {
   // Auf der Tasmota-Hauptseite steht bewusst NICHTS: SKI, Rolle, gefundene Geraete und
   // Verbindungszustaende haben ihren Platz auf der Bedienseite unter /steuerbox — dort in
   // einer Form, die man bedienen kann, statt als Textzeilen zwischen fremden Sensorwerten.
}

// Button auf der Hauptseite: EEBUS-Scan selbst starten (schickt Arg &eeb=1 an die Seite).
// eigene Bedienseite ausliefern. Die Datei liegt im Dateisystem (SD oder Flash) und wird hier
// als text/html gestreamt. Der eingebaute Weg /ufsd?download= schickt sie mit
// "Content-Disposition: attachment" — der Browser wuerde sie herunterladen statt anzuzeigen.
// Erst das aktive FS (i.d.R. SD) versuchen, dann Flash: so bleibt die Seite auch erreichbar, wenn
// die Karte fehlt. Aktualisieren geht per Upload derselben Datei, ohne neu zu flashen.
const char EEBUS_UI_FILE[] = "/steuerbox.html";
void EebusWebPage(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }
  File f;
   // v231 (06.11., meierchen006, wie HEMS v033): zuerst die gepackte Fassung /steuerbox.html.gz. streamFile setzt bei
   // Dateinamen auf ".gz" selbst "Content-Encoding: gzip". Kuerzere Uebertragung = Webserver schneller wieder frei.
   // Ohne .gz wie bisher die ungepackte Seite. WICHTIG: bei jeder neuen Seite BEIDE Dateien hochladen.
  if (ufsp) { f = ufsp->open("/steuerbox.html.gz", "r"); }
  if (!f && ffsp) { f = ffsp->open("/steuerbox.html.gz", "r"); }
  if (!f && ufsp) { f = ufsp->open(EEBUS_UI_FILE, "r"); }
  if (!f && ffsp) { f = ffsp->open(EEBUS_UI_FILE, "r"); }
  if (!f) {
    WSContentBegin(404, CT_PLAIN);
    WSContentSend_P(PSTR("steuerbox.html liegt nicht im Dateisystem - bitte hochladen"));
    WSContentEnd();
    return;
  }
  Webserver->sendHeader(F("Cache-Control"), F("no-cache"));
  Webserver->streamFile(f, F("text/html"));
  f.close();
}

// Identity backup / restore on the web port 80, behind the web password (HttpCheckPriviledgedAccess). The key
// cannot be fetched over port 82/83 any more (refused there), this is the way for "Sichern" and for moving a
// device. Same path and form in every EEBUS driver:
//   GET  /eebus_identity                       -> {"ok":true,"cert":"<hex DER>","key":"<hex DER>"}
//   POST /eebus_identity  cert=<hex>&key=<hex> -> {"ok":true,"restart":true}   (written to flash, takes effect after a restart)
// An error is never a 200: 401 without access, 404 no identity yet, 400 bad data, 500 write failed.
static bool EebusIdentityFromHex(const String &h, uint8_t *out, size_t max, size_t *len) {
  size_t n = h.length();
  if ((0 == n) || (n & 1) || ((n / 2) > max)) { return false; }
  for (size_t i = 0; i < n / 2; i++) {
    uint8_t v = 0;
    for (uint32_t k = 0; k < 2; k++) {
      char c = h[2 * i + k];
      uint8_t d;
      if ((c >= '0') && (c <= '9')) { d = c - '0'; }
      else if ((c >= 'a') && (c <= 'f')) { d = c - 'a' + 10; }
      else if ((c >= 'A') && (c <= 'F')) { d = c - 'A' + 10; }
      else { return false; }
      v = (v << 4) | d;
    }
    out[i] = v;
  }
  *len = n / 2;
  return true;
}

// Before anything is written: the key must load, and the certificate must carry ITS public point. A key that
// does not load would make EebusLoadCert fail at the next start, and EebusGenerateCert would then replace the
// identity WITHOUT a warning (new SKI, every pairing gone). A foreign certificate gives a device whose SKI (from
// the key) does not match what it sends. The point is the last 65 bytes of the SPKI, as in EebusSkiRawFromKey.
static bool EebusIdentityCheck(const uint8_t *cert, size_t cl, const uint8_t *key, size_t kl) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
  bool ok = (0 == mbedtls_pk_parse_key(&pk, key, kl, nullptr, 0, mbedtls_ctr_drbg_random, &ctr_drbg));
#else
  bool ok = (0 == mbedtls_pk_parse_key(&pk, key, kl, nullptr, 0));
#endif
  bool found = false;
  if (ok) {
    unsigned char der[160];
    int len = mbedtls_pk_write_pubkey_der(&pk, der, sizeof(der));
    if ((len >= 65) && (cl >= 65)) {
      const unsigned char *point = der + sizeof(der) - 65;
      for (size_t i = 0; !found && (i + 65 <= cl); i++) {
        found = (0 == memcmp(cert + i, point, 65));
      }
    }
  }
  mbedtls_pk_free(&pk);
#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
#endif
  return ok && found;
}

void EebusIdentityPage(void) {
  if (!HttpCheckPriviledgedAccess()) { return; }
  Webserver->sendHeader(F("Cache-Control"), F("no-store"));
#ifdef USE_UFILESYS
  uint8_t *cert = (uint8_t *)malloc(EEBUS_CERT_DER_SIZE);
  uint8_t *key = (uint8_t *)malloc(EEBUS_KEY_DER_SIZE);
  if (!cert || !key || !ffsp) {
    free(cert); free(key);
    Webserver->send(500, F("application/json"), F("{\"ok\":false,\"error\":\"no memory or no flash filesystem\"}"));
    return;
  }
  size_t cl = 0, kl = 0;
  if (HTTP_POST == Webserver->method()) {
    bool good = EebusIdentityFromHex(Webserver->arg(F("cert")), cert, EEBUS_CERT_DER_SIZE, &cl) &&
                EebusIdentityFromHex(Webserver->arg(F("key")), key, EEBUS_KEY_DER_SIZE, &kl) &&
                (0x30 == cert[0]) && (0x30 == key[0]);   // a DER structure starts with a SEQUENCE
    if (!good) {
      Webserver->send(400, F("application/json"), F("{\"ok\":false,\"error\":\"cert and key must be hex DER\"}"));
    } else if (!EebusIdentityCheck(cert, cl, key, kl)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Identitaet zurueckspielen abgelehnt: Schluessel nicht lesbar oder passt nicht zum Zertifikat"));
      Webserver->send(400, F("application/json"), F("{\"ok\":false,\"error\":\"key unreadable or not the key of this certificate\"}"));
    } else if (!TfsSaveFile(EEBUS_KEY_FILE, key, kl) || !TfsSaveFile(EEBUS_CERT_FILE, cert, cl)) {
      AddLog(LOG_LEVEL_ERROR, PSTR("EBG: Identitaet zurueckspielen: Schreiben fehlgeschlagen"));
      Webserver->send(500, F("application/json"), F("{\"ok\":false,\"error\":\"write failed, identity may be incomplete\"}"));
    } else {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Identitaet zurueckgespielt (%u + %u Byte), Neustart noetig"), (unsigned)cl, (unsigned)kl);
      Webserver->send(200, F("application/json"), F("{\"ok\":true,\"restart\":true}"));
    }
  } else {
    cl = EebusLoadDer(EEBUS_CERT_FILE, cert, EEBUS_CERT_DER_SIZE);
    kl = EebusLoadDer(EEBUS_KEY_FILE, key, EEBUS_KEY_DER_SIZE);
    if (!cl || !kl) {
      Webserver->send(404, F("application/json"), F("{\"ok\":false,\"error\":\"no identity on this device\"}"));
    } else {
      String r;
      r.reserve(2 * (cl + kl) + 48);
      r = F("{\"ok\":true,\"cert\":\"");
      for (size_t i = 0; i < cl; i++) { char b[3]; snprintf_P(b, sizeof(b), PSTR("%02x"), cert[i]); r += b; }
      r += F("\",\"key\":\"");
      for (size_t i = 0; i < kl; i++) { char b[3]; snprintf_P(b, sizeof(b), PSTR("%02x"), key[i]); r += b; }
      r += F("\"}");
      Webserver->send(200, F("application/json"), r);
    }
  }
  free(cert); free(key);
#else
  Webserver->send(500, F("application/json"), F("{\"ok\":false,\"error\":\"no filesystem\"}"));
#endif
}

void EebusWebAddMainButton(void) {
   // EIN Knopf zur eigenen Bedienseite, sonst nichts. Scannen, Verbinden und alle Anzeigen
   // liegen dort. Gelb mit schwarzer Schrift, damit er sich von Tasmotas eigenen Knoepfen abhebt.
  WSContentSend_P(PSTR("<button style='background:#ffc107;color:#000' "
                       "onclick='window.location.href=\"/steuerbox\";'>Steuerbox</button><p></p>"));
}

// Klick-Argumente der Hauptseite auswerten:
// &eeb=1 -> Scan starten, &eebc=<ski> -> verbinden, &eebd=<ski> -> trennen.
// KRITISCH (Exception-Befundabends): FUNC_WEB_GET_ARG laeuft IM AJAX-Handler der
// Hauptseite. Der TLS-Connect (sekundenlang, BearSSL) dort drin crashte reproduzierbar.
// Deshalb NUR VORMERKEN — ausgefuehrt wird im Sekunden-Tick (EebusWebClickRun).
char   eebus_web_ski[41] = { 0 };   // SKI des angeklickten Geraets
int8_t eebus_web_op = 0;   // 0=nichts, 1=verbinden, 2=trennen

void EebusWebGetArg(void) {
  char tmp[44];   // 40 Hex-SKI + NUL
  WebGetArg(PSTR("eeb"), tmp, sizeof(tmp));
  if (strlen(tmp)) { EebusStartScan(); return; }   // async, unkritisch
  WebGetArg(PSTR("eebd"), tmp, sizeof(tmp));
  if (strlen(tmp)) {
    strlcpy(eebus_web_ski, tmp, sizeof(eebus_web_ski));
    eebus_web_op = 2;
    return;
  }
  WebGetArg(PSTR("eebc"), tmp, sizeof(tmp));
  if (strlen(tmp)) {
    strlcpy(eebus_web_ski, tmp, sizeof(eebus_web_ski));
    eebus_web_op = 1;
  }
}

// Vorgemerkte Web-Klicks ausfuehren — laeuft NUR im Sekunden-Tick (nie im HTTP-Handler).
void EebusWebClickRun(void) {
  if (0 == eebus_web_op) { return; }
  int op = eebus_web_op;
  eebus_web_op = 0;
  if (2 == op) {   // Trennen: Slot dieses Peers suchen
    int ci = EebusConnBySki(eebus_web_ski);
    if (ci >= 0) {
      ESp = &EConn[ci]; Eebus126ModeLoad();
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Web-Trennen von %s"), ESp->peer_ip);
      EebusDisconnectNow();
    }
    return;
  }
  for (uint32_t i = 0; i < Eebus.peer_count; i++) {   // Verbinden: Peer per SKI suchen
    if (0 == strcasecmp(Eebus.peers[i].ski, eebus_web_ski)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Web-Verbinden zu %s (%s)"), Eebus.peers[i].ip,
             (Eebus.peers[i].model[0]) ? Eebus.peers[i].model : "?");
      EebusConnectPeer(i);
      return;
    }
  }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Web-Verbinden: SKI nicht (mehr) in der Peer-Liste, bitte neu scannen"));
}
#endif   // USE_WEBSERVER

/*********************************************************************************************\
 * Treiber-Interface
\*********************************************************************************************/

// ============================================================================
// SHIP-SERVER Etappe 1 — TLS-SERVER-Handshake fuer EINGEHENDE Verbindungen.
// / bewiesen: Waermepumpen-Gateway (.70) + Ladestation (.89) verbinden sich von SELBST zu uns
// (TLS-1.2-ClientHello), sobald wir als Steuerbox (mDNS cat=1) lauschen. Hier
// nehmen wir die Verbindung an und fuehren den BearSSL-SERVER-Handshake
// (TLS 1.2, ECDHE_ECDSA mit AES128 CBC 0xC023 = SHIP-Pflicht oder GCM 0xC02B, unser SHIP-Cert/Key).
// ZIEL DIESER ETAPPE: Handshake DURCH + erste entschluesselte Anwendungsbytes
// (= HTTP/WebSocket-Upgrade-Request des Geraets) im Log. NOCH KEIN WS-Upgrade/
// SHIP-Handshake (Etappe 2/3) und KEIN Client-Cert-Request (Peer-SKI-Pruefung
// folgt spaeter — fuer den Handshake-Beweis unnoetig; BearSSL fordert ohne
// X.509-Engine kein Client-Zertifikat an, die Geraete-Clients tolerieren das).
// br_ssl_server_zero/reset: lib/lib_ssl/bearssl-esp8266/src/ssl/ssl_server_min.c
// (Upstream-BearSSL; war aus der Tasmota-Lib herausgekuerzt). Server-Handshake-
// Maschine (ssl_hs_server.c) + EC-Policy (ssl_scert_single_ec.c) waren vorhanden.
// Engine-I/O-Muster = tls_mini _run_until (Client+Server identisch, nur Init anders).
// ============================================================================
NetworkServer *EebusInboundSrv = nullptr;

// Empfangspuffer voll (16 KB): falls ein Peer die SHIP-MFLN-1024-Pflicht ignoriert
// und grosse TLS-Records schickt. Sendepuffer klein: WIR senden Records <= 1024.
#define EEBUS_SRV_IBUF  BR_SSL_BUFSIZE_INPUT   // 16384+325
#define EEBUS_SRV_OBUF  2048
#define EEBUS_SRV_ABUF  32768   // Ansammelpuffer entschluesselter App-Daten.
   // 8192 war ZU KLEIN (wie Client-rxbuf) — eingehende Peer-Discovery
   // (Waermepumpen-Gateway = 10969 B) sprengte den Puffer -> Server-Pfad kappte die Verbindung
   // ("WS-Frame 10969 B > Puffer, Verbindung beendet"). 32 KB deckt reale
   // Discovery + Reserve; liegt jetzt in PSRAM (special_malloc, s.u.).
#define EEBUS_SRV_TIMEOUT_MS  30000UL
#define EEBUS_SRV_DATA_TIMEOUT_MS  90000UL   // Datenphase — Peers duerfen laenger schweigen

// v190: X.509-Leser fuer das Zertifikat des Anrufers. Keine CA-Pruefung (SHIP-Zertifikate sind selbst
// signiert) - es zaehlt der Schluessel: BearSSL prueft damit die CertificateVerify-Signatur des Anrufers, also dass
// er den geheimen Schluessel zu dieser SKI besitzt. Nur das erste Zertifikat der Kette wird gelesen.
typedef struct {
  const br_x509_class *vtable;
  br_x509_decoder_context dc;
  uint8_t  n_cert;
  bool     have;
  char     ski[41];
  uint8_t  der[1024];   // v242: erstes Zertifikat (fuer die SKI-Erweiterung)
  size_t   dl;
} EebusSrvX509;

void EebusSrvX509StartChain(const br_x509_class **ctx, const char *server_name) {
  EebusSrvX509 *x = (EebusSrvX509 *)ctx;
  x->n_cert = 0; x->have = false; x->ski[0] = '\0'; x->dl = 0;
}
void EebusSrvX509StartCert(const br_x509_class **ctx, uint32_t length) {
  EebusSrvX509 *x = (EebusSrvX509 *)ctx;
  if (0 == x->n_cert) { br_x509_decoder_init(&x->dc, nullptr, nullptr, nullptr, nullptr); }
}
void EebusSrvX509Append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
  EebusSrvX509 *x = (EebusSrvX509 *)ctx;
  if (0 == x->n_cert) {
    br_x509_decoder_push(&x->dc, buf, len);
    size_t n = sizeof(x->der) - x->dl;   // v242
    if (n > len) { n = len; }
    memcpy(x->der + x->dl, buf, n); x->dl += n;
  }
}
void EebusSrvX509EndCert(const br_x509_class **ctx) {
  EebusSrvX509 *x = (EebusSrvX509 *)ctx;
  if (0 == x->n_cert) {
    br_x509_pkey *pk = br_x509_decoder_get_pkey(&x->dc);
    if (pk && (BR_KEYTYPE_EC == pk->key_type) && (65 == pk->key.ec.qlen)) {
      uint8_t h[20];
      br_sha1_context sc;
      br_sha1_init(&sc);
      br_sha1_update(&sc, pk->key.ec.q, 65);   // wie EebusSkiRawFromKey: SHA-1 ueber 04||X||Y
      br_sha1_out(&sc, h);
      for (uint32_t i = 0; i < 20; i++) { snprintf_P(x->ski + (i * 2), 3, PSTR("%02x"), h[i]); }
      x->have = true;
      uint8_t ext[20];   // v242 (SHIP Z. 1265-1269): SKI-Erweiterung muss zum Schluessel passen
      if (!EebusDerSkiExt(x->der, x->dl, ext) || (0 != memcmp(ext, h, 20))) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV Anrufer %s: SKI-Erweiterung im Zertifikat fehlt oder passt nicht - abgewiesen"), x->ski);
        x->have = false;
      }
    }
  }
  x->n_cert++;
}
unsigned EebusSrvX509EndChain(const br_x509_class **ctx) {
  EebusSrvX509 *x = (EebusSrvX509 *)ctx;
  return x->have ? 0 : BR_ERR_X509_UNSUPPORTED;   // kein EC-Schluessel lesbar -> Handshake bricht ab
}
const br_x509_pkey *EebusSrvX509GetPkey(const br_x509_class *const *ctx, unsigned *usages) {
  EebusSrvX509 *x = (EebusSrvX509 *)ctx;
  if (usages) { *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN; }
  return x->have ? br_x509_decoder_get_pkey(&x->dc) : nullptr;
}
const br_x509_class EebusSrvX509Vtable = {
  sizeof(EebusSrvX509), EebusSrvX509StartChain, EebusSrvX509StartCert, EebusSrvX509Append,
  EebusSrvX509EndCert, EebusSrvX509EndChain, EebusSrvX509GetPkey
};
// Ein leerer Name (DER: leere SEQUENCE). BearSSL fragt nur dann nach einem Client-Zertifikat, wenn mindestens ein
// Name gesetzt ist; welcher, ist fuer selbst signierte SHIP-Zertifikate egal.
static const unsigned char eebus_srv_dn_leer[2] = { 0x30, 0x00 };
static const br_x500_name eebus_srv_dn[1] = { { (unsigned char *)eebus_srv_dn_leer, 2 } };

typedef struct {
  bool     active = false;
  bool     hs_done = false;
  NetworkClient sock;   // eingehender TCP-Socket (aus accept())
  br_ssl_server_context *sc = nullptr;   // BearSSL-Server-Kontext (~4,5 KB, DRAM)
  unsigned char *ibuf = nullptr;   // TLS-Empfangs-Records (DRAM)
  unsigned char *obuf = nullptr;   // TLS-Sende-Records (DRAM)
  uint8_t *cert_der = nullptr;   // unser Cert; BearSSL zeigt darauf
  size_t   cert_len = 0;
  uint8_t  key_scalar[32] = { 0 };
  br_x509_certificate br_cert[1];
  br_ec_private_key   br_key;
  uint32_t deadline = 0;   // millis()-Timeout der Verbindung
  char     peer_ip[46] = { 0 };   // v185: 46 statt 16 — IPv6
   // Etappe 2: WebSocket-Server + CMI
  uint8_t  ws_state = 0;   // 0=HTTP-Upgrade-Request sammeln, 1=WS aktiv (101 gesendet)
  bool     cmi_done = false;   // CMI [0,0] des Geraets beantwortet?
  uint8_t *abuf = nullptr;   // Ansammelpuffer (HTTP-Request, dann WS-Frames)
  size_t   abuf_len = 0;
   // Etappe 3: SME-Handshake als Server (Geraet = SHIP-Client)
  uint8_t  sme_state = 0;   // 0=Hello, 1=warte announceMax, 2=warte select-Spiegel, 4=warte Peer-pinState (v242), 3=DONE
  uint32_t sme_frist = 0;   // v242: Wait-For-Ready / Wait-Timer / pinState-Frist (0 = keine)
  uint32_t hello_start = 0; // v242: Beginn des Hello (Obergrenze fuer Verlaengerungen)
   // Etappe 4: Verknuepfung mit einem EConn-Slot (SPINE-Datenphase)
  int      conn_idx = -1;   // Index in EConn[] (-1 = nicht verknuepft)
  EebusSrvX509 *x509 = nullptr;   // v190: Zertifikat des Anrufers (DRAM, ~0,8 KB)
  bool     send_kaputt = false;   // v204: ein WS-Frame wurde nur teilweise gesendet -> Verbindung beenden
  uint8_t *hold = nullptr;   // v206: waehrend EebusSrvAppWrite empfangene App-Daten (dort darf abuf nicht wachsen)
  size_t   hold_len = 0;
  uint8_t *frag = nullptr;   // v231 (4.10): Zusammenbau zerstueckelter WS-Nachrichten (PSRAM, erst beim ersten Fragment)
  size_t   frag_len = 0;
  bool     frag_on = false;  // v231: ein Zusammenbau laeuft
} EebusSrvSlot;
EebusSrvSlot ESrv;   // Etappe 1: EINE eingehende Verbindung

void EebusSrvFree(void) {
  if (ESrv.conn_idx >= 0) {   // verknuepften SPINE-Slot zuruecksetzen
    EebusConn *cc = &EConn[ESrv.conn_idx];
    cc->via_srv = false;
    cc->active = false;
    cc->teardown = false;
    cc->keepalive = false;
    cc->sme = SME_OFF;
    cc->state = SHIP_IDLE;
    cc->hb_sub = false;
    cc->hb_next = 0;
    cc->peer_hb_at = 0; cc->peer_hb_ctr = 0; cc->peer_hb_tmo_s = 0; cc->peer_hb_lost = false; cc->hb_zurueck = false; cc->hb_tx_at = 0;
    cc->lpc_state = LPC_IDLE;
    cc->lpc_write_tries = 0;   // 
    cc->lpc_write_retry_at = 0;   // 
    ESrv.conn_idx = -1;
  }
  if (ESrv.sock) { ESrv.sock.stop(); }
  ESrv.sock = NetworkClient();   // Socket-Handle loslassen (shared)
  if (ESrv.sc)       { free(ESrv.sc);       ESrv.sc = nullptr; }
  if (ESrv.x509)     { free(ESrv.x509);     ESrv.x509 = nullptr; }   // v190
  if (ESrv.ibuf)     { free(ESrv.ibuf);     ESrv.ibuf = nullptr; }
  if (ESrv.obuf)     { free(ESrv.obuf);     ESrv.obuf = nullptr; }
  if (ESrv.cert_der) { free(ESrv.cert_der); ESrv.cert_der = nullptr; }
  if (ESrv.abuf)     { free(ESrv.abuf);     ESrv.abuf = nullptr; }
  if (ESrv.hold)     { free(ESrv.hold);     ESrv.hold = nullptr; }   // v206
  ESrv.hold_len = 0;
  if (ESrv.frag)     { free(ESrv.frag);     ESrv.frag = nullptr; }   // v231
  ESrv.frag_len = 0;
  ESrv.frag_on = false;
  ESrv.cert_len = 0;
  ESrv.active = false;
  ESrv.hs_done = false;
  ESrv.send_kaputt = false;   // v204
  ESrv.ws_state = 0;
  ESrv.cmi_done = false;
  ESrv.abuf_len = 0;
  ESrv.sme_state = 0; ESrv.sme_frist = 0;   // v242
}

// BearSSL-SERVER-Kontext aufsetzen und Handshake starten. Socket + peer_ip stehen
// schon in ESrv (Globals — .ino-Prototypen-Falle, keine Klassen-Typen in Signaturen).
bool EebusSrvStart(void) {
  const char *why = "";
  ESrv.hs_done = false;
  ESrv.deadline = Eebus126At(EEBUS_SRV_TIMEOUT_MS);

  ESrv.sc   = (br_ssl_server_context*)malloc(sizeof(br_ssl_server_context));
  ESrv.ibuf = (unsigned char*)malloc(EEBUS_SRV_IBUF);
  ESrv.obuf = (unsigned char*)malloc(EEBUS_SRV_OBUF);
  ESrv.cert_der = (uint8_t*)malloc(EEBUS_CERT_DER_SIZE);
  ESrv.x509 = (EebusSrvX509*)calloc(1, sizeof(EebusSrvX509));   // v190
  ESrv.abuf = (uint8_t*)special_malloc(EEBUS_SRV_ABUF);   // 32 KB -> PSRAM (DRAM schonen), wird wie bisher per free() freigegeben
  ESrv.abuf_len = 0;
  ESrv.hold = (uint8_t*)special_malloc(EEBUS_SRV_ABUF);   // v206: Zwischenspeicher, ebenfalls PSRAM
  ESrv.hold_len = 0;
  ESrv.ws_state = 0;
  ESrv.cmi_done = false;
  ESrv.sme_state = 0; ESrv.sme_frist = 0;   // v242
  ESrv.conn_idx = -1;
  if ((nullptr == ESrv.sc) || (nullptr == ESrv.ibuf) || (nullptr == ESrv.abuf) ||
      (nullptr == ESrv.obuf) || (nullptr == ESrv.cert_der) || (nullptr == ESrv.x509)) { why = "malloc"; goto fail; }   // v190: x509
  ESrv.cert_len = EebusLoadDer(EEBUS_CERT_FILE, ESrv.cert_der, EEBUS_CERT_DER_SIZE);
  if (0 == ESrv.cert_len) { why = "cert load"; goto fail; }
  if (!EebusKeyScalar(ESrv.key_scalar)) { why = "key scalar"; goto fail; }
  ESrv.br_cert[0].data = ESrv.cert_der;
  ESrv.br_cert[0].data_len = ESrv.cert_len;
  ESrv.br_key.curve = BR_EC_secp256r1;
  ESrv.br_key.x = ESrv.key_scalar;
  ESrv.br_key.xlen = 32;

  {
    br_ssl_server_zero(ESrv.sc);
    br_ssl_engine_context *eng = &ESrv.sc->eng;
    br_ssl_engine_add_flags(eng, BR_OPT_NO_RENEGOTIATION);
    br_ssl_engine_set_versions(eng, BR_TLS12, BR_TLS12);
   // SHIP §9.1 schreibt TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256
   // (0xC023) als PFLICHT-Suite vor; 0xC02B (GCM) ist optional. Unser SERVER bot bisher NUR 0xC02B
   // (der alte Kommentar "9.2 ... genau die eine Suite" war falsch) -> ein normkonformer Peer, der
   // mit der Pflicht-Suite anklopft, bekommt bei uns keinen Handshake zustande. Clientseitig ist das
   // inzwischen korrigiert (WiFiClientSecureLightBearSSL.cpp: suites_eebus fuehrt 0xC023 an); hier
   // dasselbe fuer den Server, Reihenfolge wie die Referenz: Pflicht-Suite zuerst, GCM als zweite.
    static const uint16_t srv_suites[] = {
      BR_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,   // 0xC023 = SHIP-Pflicht
      BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,   // 0xC02B = optional (bisher die einzige)
    };
    br_ssl_engine_set_suites(eng, srv_suites, (sizeof srv_suites) / (sizeof srv_suites[0]));
   // Algorithmen wie der bewaehrte Client-Pfad (tls_mini br_ssl_client_base_init)
    br_ssl_engine_set_hash(eng, br_sha256_ID, &br_sha256_vtable);
    br_ssl_engine_set_prf_sha256(eng, &br_tls12_sha256_prf);
    br_ssl_engine_set_gcm(eng, &br_sslrec_in_gcm_vtable, &br_sslrec_out_gcm_vtable);
    br_ssl_engine_set_aes_ctr(eng, &br_aes_small_ctr_vtable);
    br_ssl_engine_set_ghash(eng, &br_ghash_ctmul32);
   // CBC-Engine registrieren — ohne sie kann die oben angebotene Pflicht-Suite 0xC023 nicht
   // ausgehandelt werden. Identisch zum Client-Pfad (WiFiClientSecureLightBearSSL.cpp Z.981-982);
   // HMAC nutzt den bereits gesetzten SHA256-Hash, AES "small" wie beim vorhandenen CTR/GCM.
    br_ssl_engine_set_cbc(eng, &br_sslrec_in_cbc_vtable, &br_sslrec_out_cbc_vtable);
    br_ssl_engine_set_aes_cbc(eng, &br_aes_small_cbcenc_vtable, &br_aes_small_cbcdec_vtable);
    br_ssl_engine_set_ec(eng, &br_ec_p256_m15);
    br_ssl_server_set_single_ec(ESrv.sc, ESrv.br_cert, 1, &ESrv.br_key,
                                BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN, BR_KEYTYPE_EC,
                                &br_ec_p256_m15, br_ecdsa_sign_asn1_get_default());
    br_ssl_engine_set_buffers_bidi(eng, ESrv.ibuf, EEBUS_SRV_IBUF, ESrv.obuf, EEBUS_SRV_OBUF);
   // v190: Zertifikat des Anrufers anfordern und lesen. Ohne Zertifikat (fremdes Geraet, das bei einer nicht
   // passenden Namensliste keins schickt) laeuft der Handshake weiter - dann Rueckfall auf die Fundliste (IP).
    ESrv.x509->vtable = &EebusSrvX509Vtable;
    br_ssl_engine_set_x509(eng, &ESrv.x509->vtable);
    br_ssl_engine_set_ecdsa(eng, br_ecdsa_vrfy_asn1_get_default());   // CertificateVerify des Anrufers pruefen
    br_ssl_server_set_trust_anchor_names(ESrv.sc, eebus_srv_dn, 1);
     // v242 (SHIP Z. 692-693): gegenseitige Authentisierung ist Pflicht - kein BR_OPT_TOLERATE_NO_CLIENT_AUTH mehr.
    if (!br_ssl_server_reset(ESrv.sc)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV reset fehlgeschlagen, err=%d"),
             br_ssl_engine_last_error(eng));
      why = "reset"; goto fail;
    }
  }
  ESrv.active = true;
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV TLS-SERVER-Handshake mit %s gestartet"), ESrv.peer_ip);
  return true;

fail:
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV Start fehlgeschlagen (%s), Verbindung %s verworfen"),
         why, ESrv.peer_ip);
  return false;
}

// v205: nicht blockierend senden. NetworkClient::write() wartet bis 10 x 1 s auf einen schreibbaren Socket und will
// ALLES loswerden - in der Zeit liest die Steuerbox nichts (Beweis 26.10. 09:50:56: "warte" und "abgebrochen" in
// derselben Millisekunde). Hier nur so viel, wie der Socket gerade nimmt. Rueckgabe: gesendete Bytes (0 = voll),
// -1 = Socket kaputt.
int EebusSrvSockSend(const uint8_t *buf, size_t len) {
  int fd = ESrv.sock.fd();
  if (fd < 0) { return -1; }
  int r = send(fd, (const void*)buf, len, MSG_DONTWAIT);
  if (r >= 0) { return r; }
  if ((EAGAIN == errno) || (EWOULDBLOCK == errno)) { return 0; }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: Senden fehlgeschlagen, errno %d"), ESrv.peer_ip, errno);
  return -1;
}

// Anwendungsdaten in die TLS-Engine schreiben (Server-Sendepfad). Schreibt die
// Nutzlast in den Engine-Klartextpuffer und schiebt die verschluesselten Records
// direkt zum Socket (kleine Nachrichten; bei vollem Socket-Puffer max 3 s warten).
bool EebusSrvAppWrite(const uint8_t *data, size_t len) {
  br_ssl_engine_context *eng = &ESrv.sc->eng;
  size_t off = 0;
  uint32_t t0 = millis();
  bool gewartet = false;   // v204: musste auf Platz im Socket warten
  while (off < len) {
    unsigned state = br_ssl_engine_current_state(eng);
    if (state & BR_SSL_CLOSED) { return false; }
    if (state & BR_SSL_SENDAPP) {
      size_t alen;
      unsigned char *buf = br_ssl_engine_sendapp_buf(eng, &alen);
      size_t n = (len - off > alen) ? alen : (len - off);
      if (n > 1024) { n = 1024; }   // v242 (SHIP Z. 714-716): TLS-Records hoechstens 1024 B Klartext
      memcpy(buf, data + off, n);
      br_ssl_engine_sendapp_ack(eng, n);
      off += n;
      br_ssl_engine_flush(eng, 0);   // Record schliessen -> SENDREC
      continue;
    }
    if (state & BR_SSL_SENDREC) {   // Records zum Socket schieben (macht Platz)
      size_t rlen;
      unsigned char *rbuf = br_ssl_engine_sendrec_buf(eng, &rlen);
      int wlen = EebusSrvSockSend(rbuf, rlen);   // v205: nicht blockierend
      if (wlen < 0) { ESrv.send_kaputt = true; return false; }
      if (wlen > 0) { br_ssl_engine_sendrec_ack(eng, wlen); continue; }
    }
     // v204: Socket voll. Waehrend des Wartens eingehende TLS-Records der Gegenstelle in die Engine lesen - sonst
     // warten beide Seiten aufeinander (beide senden gleichzeitig mehr, als die Socket-Puffer fassen, keiner liest).
     // Nur Records in die Engine (RECVREC), KEINE App-Daten in abuf: wir stehen evtl. mitten im Frame-Parser.
    if ((state & BR_SSL_RECVAPP) && ESrv.hold) {   // v206: entschluesselte Daten in den Zwischenspeicher -> die Engine
      size_t alen;                                  // nimmt weitere Records an, die Gegenstelle wird ihre Nachricht los
      unsigned char *abuf = br_ssl_engine_recvapp_buf(eng, &alen);
      size_t room = EEBUS_SRV_ABUF - ESrv.hold_len;
      size_t take = (alen > room) ? room : alen;
      if (take > 0) {
        memcpy(ESrv.hold + ESrv.hold_len, abuf, take);
        ESrv.hold_len += take;
        br_ssl_engine_recvapp_ack(eng, take);
        continue;
      }
    }
    if (state & BR_SSL_RECVREC) {
      if (ESrv.sock.available() > 0) {
        size_t ilen;
        unsigned char *ibuf = br_ssl_engine_recvrec_buf(eng, &ilen);
        int r = ESrv.sock.read(ibuf, ilen);
        if (r > 0) { br_ssl_engine_recvrec_ack(eng, r); continue; }
      }
    }
    if (!gewartet) {
      gewartet = true;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: Sendepuffer voll - warte (%u von %u B gesendet)"),
             ESrv.peer_ip, (uint32_t)off, (uint32_t)len);
    }
    if (millis() - t0 > 3000) {
       // v204: bisher stumm. Ein halb gesendeter WS-Frame zerstoert den Datenstrom -> Verbindung sauber beenden,
       // die Gegenstelle baut neu auf (statt 60 s stumm haengen zu bleiben).
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: Senden nach 3 s abgebrochen (%u von %u B) - Verbindung wird beendet"),
             ESrv.peer_ip, (uint32_t)off, (uint32_t)len);
      ESrv.send_kaputt = true;
      return false;
    }
    delay(1);
  }
  if (gewartet) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: Senden nach %u ms fertig (%u B)"), ESrv.peer_ip,
           (uint32_t)(millis() - t0), (uint32_t)len);
  }
  br_ssl_engine_flush(eng, 0);   // letzten Record schliessen (Pumpe sendet)
  return true;
}

// WS-Frame als SERVER senden — UNMASKIERT (RFC 6455: nur Clients maskieren).
// opcode 0x2=binary (SHIP), 0xA=Pong, 0x8=Close.
bool EebusSrvWsSendOp(const uint8_t *payload, size_t len, uint8_t opcode) {
  uint8_t hdr[10];
  size_t hl = 2;
  hdr[0] = 0x80 | (opcode & 0x0F);   // FIN + opcode
  if (len < 126) {
    hdr[1] = (uint8_t)len;   // KEIN Mask-Bit
  } else if (len < 65536) {
    hdr[1] = 126;
    hdr[2] = (uint8_t)(len >> 8);
    hdr[3] = (uint8_t)(len & 0xFF);
    hl = 4;
  } else {
    hdr[1] = 127;
    for (int i = 0; i < 8; i++) { hdr[2 + i] = (uint8_t)((uint64_t)len >> (8 * (7 - i))); }
    hl = 10;
  }
  if (0 == len) { return EebusSrvAppWrite(hdr, hl); }
   // v231 (Pruefung 1.10): Kopf und Nutzlast in EINEM Aufruf (wie v187 im Client-Weg). Bisher zwei Aufrufe mit je bis
   // zu 3 s Wartezeit - bei voller Leitung stand die Box bis zu 6 s je Frame.
  size_t tot = hl + len;
  uint8_t small[256];
  uint8_t *fb = (tot <= sizeof(small)) ? small : (uint8_t*)special_malloc(tot);
  if (nullptr == fb) {   // kein Speicher: wie bisher in zwei Schritten
    if (!EebusSrvAppWrite(hdr, hl)) { return false; }
    return EebusSrvAppWrite(payload, len);
  }
  memcpy(fb, hdr, hl);
  memcpy(fb + hl, payload, len);
  bool ok = EebusSrvAppWrite(fb, tot);
  if (fb != small) { free(fb); }
  return ok;
}

// HTTP-Upgrade-Request beantworten. Sec-WebSocket-Accept = base64(SHA1(key+GUID)),
// Subprotocol "ship" (SHIP 10.3). Request liegt NUL-terminiert in ESrv.abuf.
bool EebusSrvWsUpgradeReply(void) {
  char *kh = strstr((char*)ESrv.abuf, "Sec-WebSocket-Key:");
  if (nullptr == kh) { return false; }
  kh += 18;
  while (' ' == *kh) { kh++; }
  char key[40];
  size_t kl = 0;
  while (*kh && ('\r' != *kh) && ('\n' != *kh) && (kl < sizeof(key) - 1)) { key[kl++] = *kh++; }
  key[kl] = '\0';
  static const char ws_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";   // RFC 6455 4.2.2
  uint8_t sha[20];
  SHA1Builder sha1;
  sha1.begin();
  sha1.add((uint8_t*)key, kl);
  sha1.add((uint8_t*)ws_guid, sizeof(ws_guid) - 1);
  sha1.calculate();
  sha1.getBytes(sha);
  unsigned char b64[32];
  size_t b64l = 0;
  if (0 != mbedtls_base64_encode(b64, sizeof(b64), &b64l, sha, 20)) { return false; }
  char resp[192];
  int rl = snprintf_P(resp, sizeof(resp),
    PSTR("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
         "Sec-WebSocket-Accept: %.*s\r\nSec-WebSocket-Protocol: ship\r\n\r\n"),
    (int)b64l, (char*)b64);
  return EebusSrvAppWrite((const uint8_t*)resp, rl);
}

// SHIP-Control-Message (classifier 1) an das Geraet senden (Server-Rolle).
// Control-Messages sind klein (<256 B) — Stack-Puffer reicht.
bool EebusSrvShipSendJson(const char *json) {
  size_t jl = strlen(json);
  uint8_t buf[256];
  if (jl + 1 > sizeof(buf)) { return false; }
  buf[0] = 1;
  memcpy(buf + 1, json, jl);
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV SHIP TX c=1 %s"), json);
  return EebusSrvWsSendOp(buf, jl + 1, 0x2);
}

// Etappe 4: nach SHIP-Done die Server-Verbindung mit einem EConn-Slot verknuepfen.
// Damit laeuft die KOMPLETTE bestehende SPINE-Maschinerie (Discovery-Antwort/-Read,
// UseCases, Subscriptions, Heartbeat, LPC-Schreibsequenz) unveraendert auch ueber die
// eingehende Verbindung — TX wird in EebusWsSendOp per via_srv umgeleitet.
// Pseudo-SKI = Peer-IP (Server-Seite kennt die echte SKI mangels Client-Cert-Request noch nicht).
void EebusSrvLinkConn(void) {
  int ci = EebusConnAlloc(ESrv.peer_ip);
  if (ci < 0) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: kein freier SPINE-Slot — Datenphase ohne SPINE!"), ESrv.peer_ip);
    return;
  }
  EebusConn *cc = &EConn[ci];
  cc->via_srv = true;   // TX-Routing + SmePoll-Ausnahme
  cc->active = true;
  cc->state = SHIP_CMI_OK;
  cc->sme = SME_DONE;   // Datenphase erreicht (Server-Handshake fertig)
  cc->err[0] = '\0';
  cc->peer_id[0] = '\0';
  cc->peer_dev[0] = '\0';
  cc->disco_answered = false;
  cc->peer_disco_read = false;
  cc->we_nm_subscribed = false;   // 
  cc->peer_subscribed = false;
  cc->gb_n = 0;   // v246
  cc->lc_sub = false; cc->lc_cli_ent = 0; cc->lc_cli_feat = 0;   // 
  cc->hb_sub = false;
  cc->hb_cli_ent = 0;
  cc->hb_cli_feat = 0;
  cc->hb_next = 0;
  cc->hb_counter = 1;
  cc->peer_hb_at = 0; cc->peer_hb_ctr = 0; cc->peer_hb_tmo_s = 0; cc->peer_hb_lost = false; cc->hb_zurueck = false; cc->hb_tx_at = 0;
  cc->spine_ctr = 1;
  cc->lpc_state = LPC_IDLE;
  cc->lpc_bound = false;
  cc->lpc_onboard_only = false;   // 
  cc->lpc_onboarded = false;   // 
  cc->lpc_sel_step = 0;   // 
  cc->lpc_fs_val_key = -1; cc->lpc_fs_dur_key = -1; cc->lpc_fs_prod_key = -1;   // Failsafe-Keys pro Verbindung neu lernen
  cc->lpc_fs_step = 0; cc->lpc_fs_done = false;   // 
  cc->lpc_write_tries = 0;   // 
  cc->lpc_write_retry_at = 0;   // 
  if (cc->lpc_disco) { free(cc->lpc_disco); cc->lpc_disco = nullptr; }   // Discovery-Kopie freigeben
  if (cc->lpc_uc)    { free(cc->lpc_uc);    cc->lpc_uc = nullptr; }   // Use-Case-Kopie freigeben
  if (cc->rd_buf)    { free(cc->rd_buf);    cc->rd_buf = nullptr; }   // Leseantwort freigeben
  cc->rd_ent = -1; cc->rd_feat = -1;
  cc->lpc_limit_id = -1; cc->lpc_lid_dir[0] = cc->lpc_lid_dir[1] = -1;   // v184
  EebusPeerWerteNeu((uint32_t)ci);   // v194: Werte der alten Gegenstelle weg
  cc->lpc_peer_ent = 1;
  cc->lpc_peer_feat = 6;   // Default; wird aus der Peer-Discovery aktualisiert
  cc->lpc_deadline = 0;
  cc->lpc_our_limit = false;
  cc->lpc_result[0] = '\0';
  cc->keepalive = false;   // Wiederverbinden ist Sache des Geraets
  cc->teardown = false;
  cc->send_broken = false;   // v187
  cc->reconnect_at = 0;
  cc->peer_idx = -1;
  cc->last_rx = millis();
  cc->conn_at = millis(); if (!cc->conn_at) { cc->conn_at = 1; }   // v242
  cc->pong_wait_at = 0; cc->dbl_at = 0; cc->ws_ping_next = 0;
  strlcpy(cc->peer_ip, ESrv.peer_ip, sizeof(cc->peer_ip));
  strlcpy(cc->peer_ski, ESrv.peer_ip, sizeof(cc->peer_ski));   // Pseudo-SKI (s.o.)
  cc->ski_quelle = 2;   // v190: erst einmal Rueckfall (Adresse)
  bool ski_zert = ESrv.x509 && ESrv.x509->have;   // v190: der Anrufer hat sich mit Zertifikat ausgewiesen
  if (ski_zert) {
    strlcpy(cc->peer_ski, ESrv.x509->ski, sizeof(cc->peer_ski));
    cc->ski_quelle = 1;
    for (uint32_t k = 0; k < Eebus.peer_count; k++) {   // Port/Index nur zur Anzeige, die Identitaet steht schon
      if (0 == strcasecmp(Eebus.peers[k].ski, cc->peer_ski)) { cc->peer_idx = (int)k; cc->peer_port = Eebus.peers[k].port; break; }
    }
    EebusModeApply(cc->peer_ski);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s weist sich mit Zertifikat aus: SKI %s"), ESrv.peer_ip, cc->peer_ski);
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s ohne Zertifikat - nur ueber die Adresse (Fundliste) zuzuordnen"), ESrv.peer_ip);
  }
   // Echte SKI des Anrufers ueber seine IP aus der Scan-Liste nachtragen, damit er wie im Client-Weg
   // erkannt wird (Betriebsart HEMS/SteuVE, Anzeige, Wiederaufbau sieht die bestehende Verbindung).
   // Nicht in der Liste -> Pseudo-SKI bleibt, globaler Default gilt.
  for (uint32_t k = 0; !ski_zert && (k < Eebus.peer_count); k++) {   // v190: nur noch Rueckfall ohne Zertifikat
    if ((0 == strcmp(Eebus.peers[k].ip, ESrv.peer_ip)) ||
        (Eebus.peers[k].ip6[0] && (0 == strcasecmp(Eebus.peers[k].ip6, ESrv.peer_ip)))) {   // v185: auch IPv6
      strlcpy(cc->peer_ski, Eebus.peers[k].ski, sizeof(cc->peer_ski));
      cc->peer_idx  = (int)k;
      cc->peer_port = Eebus.peers[k].port;
      EebusModeApply(cc->peer_ski);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s ist SKI %s (aus Scan-Liste)"), ESrv.peer_ip, cc->peer_ski);
      break;
    }
  }
  ESrv.conn_idx = ci;
  EebusStatSet(cc->peer_ski, SHIP_CMI_OK, "");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s mit SPINE-Slot %d verknuepft — Datenphase aktiv"), ESrv.peer_ip, ci);
}

// v188 (21.10.): eingehende Verbindung ueber die AccessMethods-Kennung der echten SKI zuordnen, wenn die
// Adresse nicht passte. Anlass (Forum, Waermepumpe eines Testers): das Geraet hat ZWEI IPv6-Adressen (fd37:... und
// 2a01:...), der Scan merkt sich nur die erste (ip6), angerufen wurde ueber die andere -> der Slot behielt die
// Pseudo-SKI (Adresse), und die Seite fand die stehende Verbindung per SKI nicht ("keine stehende Verbindung mit
// dieser SKI"). Die Kennung (mDNS-TXT "id" = SHIP accessMethods.id) haengt nicht an der Adresse.
void EebusSrvSkiFromId(int ci, const char *pid) {   // Slotnummer statt EebusConn* - die Arduino-Vorab-
   // Deklaration steht vor der Struct-Definition (Bau brach sonst mit 'EebusConn was not declared' ab)
  if ((ci < 0) || (ci >= EEBUS_MAX_CONN)) { return; }
  EebusConn *cc = &EConn[ci];
  if ((nullptr == pid) || ('\0' == pid[0])) { return; }
  if (40 == strlen(cc->peer_ski)) { return; }   // hat schon die echte SKI (aus der Adresse)
  for (uint32_t k = 0; k < Eebus.peer_count; k++) {
    if (Eebus.peers[k].id[0] && (0 == strcmp(Eebus.peers[k].id, pid))) {
      strlcpy(cc->peer_ski, Eebus.peers[k].ski, sizeof(cc->peer_ski));
      cc->peer_idx  = (int)k;
      cc->peer_port = Eebus.peers[k].port;
      EebusModeApply(cc->peer_ski);
      EebusStatSet(cc->peer_ski, SHIP_CMI_OK, "");
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s ist SKI %s (ueber die Kennung %s)"), cc->peer_ip, cc->peer_ski, pid);
      return;
    }
  }
}

// Etappe 3: SME-Handshake als SERVER (Geraet = SHIP-Client). Gespiegelt zum
// Client-Pfad (EebusSmeDispatch): Hello beantworten -> announceMax des Geraets mit
// "select" beantworten (Server waehlt Version, SHIP 13.4.4.2) -> Geraet spiegelt
// select -> Pin none + AccessMethods -> Done (Datenphase; SPINE via EebusSrvLinkConn).
// ACHTUNG: kann bei Fehlern EebusSrvFree() rufen — Aufrufer muss danach ESrv.active pruefen!
void EebusSrvAbbruch(const char *why, int prot_err);   // 24.11.2026 (gemu): Vorwaertsdeklarationen wie in 128/129 (stehen weiter unten)
bool EebusSrvHelloStart(void);
const char eebus_srv_select[] = "{\"messageProtocolHandshake\":[{\"handshakeType\":\"select\"},"
                                "{\"version\":[{\"major\":1},{\"minor\":0}]},{\"formats\":[{\"format\":[\"JSON-UTF8\"]}]}]}";   // v242
void EebusSrvSmeHandle(int classifier, const char *json) {
  if (3 == classifier) {   // connectionClose des Geraets
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s hat die SHIP-Verbindung geschlossen"), ESrv.peer_ip);
    if (json && strstr(json, "\"announce\"")) {   // v231 (3.9): bestaetigen, dann schliessen (SHIP 13.4.8.1.2 DEV-B Regel 1)
      static const char conf[] = "\x03{\"connectionClose\":[{\"phase\":\"confirm\"}]}";
      EebusSrvWsSendOp((const uint8_t*)conf, sizeof(conf) - 1, 0x2);
    }
    EebusSrvFree();
    return;
  }
  if (2 == classifier) {   // SPINE-Datagramm in die Datenphase einspeisen
    if (ESrv.conn_idx >= 0) {
      EebusConn *save = ESp;   // ESp auf den verknuepften Slot umschalten
      ESp = &EConn[ESrv.conn_idx]; Eebus126ModeLoad();
      ESp->last_rx = millis();
      EebusShipLog('R', 2, json);   // Server-RX in Konsole + RAM-Mitschnitt (voll)
      EebusSpineHandle(json);   // komplette bestehende SPINE-Maschinerie
      ESp = save; Eebus126ModeLoad();
    }
    return;
  }
  if (1 != classifier) { return; }

   // v242 (SHIP 13.4.4.1): Zustand 0 = Hello. Nur ein Hello zaehlt, alles andere ist Abbruch.
  const bool ist_hello = (nullptr != strstr(json, "\"connectionHello\""));
  if (0 == ESrv.sme_state) {
    if (!ist_hello) { EebusSrvAbbruch("Hello: unerwartete Nachricht", 0); return; }
    char phase[12] = { 0 };
    EebusJsonStr(json, "phase", phase, sizeof(phase));
    if (0 == strcmp(phase, "ready")) {   // HELLO_OK -> ProtocolHandshake, Wait-Timer 10 s (SHIP Z. 2371-2374)
      ESrv.sme_state = 1;
      ESrv.sme_frist = Eebus126At(EEBUS_SME_PROT_TIMEOUT_MS);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV SME-Hello mit %s beidseitig ready — warte auf ProtocolHandshake"), ESrv.peer_ip);
    } else if (0 == strcmp(phase, "pending")) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s meldet PENDING — unsere SKI %s im Geraete-UI bestaetigen"), ESrv.peer_ip, Eebus.own_ski);
      if (strstr(json, "prolongationRequest") && strstr(json, "true")) {   // SHIP Z. 2307-2313
        if ((millis() - ESrv.hello_start) < EEBUS_SME_HELLO_MAX_MS) { ESrv.sme_frist += EEBUS_SME_HELLO_TIMEOUT_MS; }
        int32_t rest = (int32_t)(ESrv.sme_frist - millis());
        if (rest < 0) { rest = 0; }
        char h[64]; snprintf(h, sizeof(h), "{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":%ld}]}", (long)rest);
        EebusSrvShipSendJson(h);
      }   // ohne Verlaengerungswunsch: ignorieren
    } else {
      EebusSrvAbbruch("Hello aborted", 0);
    }
    return;
  }
  if (ist_hello) {   // spaeteres Hello: nur ein Abbruch zaehlt
    char phase[12] = { 0 };
    EebusJsonStr(json, "phase", phase, sizeof(phase));
    if (0 == strcmp(phase, "aborted")) { AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s Hello-Abbruch"), ESrv.peer_ip); EebusSrvFree(); }
    return;
  }

  if (strstr(json, "\"messageProtocolHandshakeError\"")) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s ProtocolHandshake-Fehler"), ESrv.peer_ip);
    EebusSrvFree();
    return;
  }

  if ((1 == ESrv.sme_state) || (2 == ESrv.sme_state)) {   // v242 (SHIP Z. 2385-2428)
    if (nullptr == strstr(json, "\"messageProtocolHandshake\"")) { EebusSrvAbbruch("ProtocolHandshake: unerwartete Nachricht", 2); return; }
    char ht[16] = { 0 };
    EebusJsonStr(json, "handshakeType", ht, sizeof(ht));
    if (1 == ESrv.sme_state) {
      uint32_t ma = 0;
      EebusJsonInt(json, "major", &ma);
      if (0 != strcmp(ht, "announceMax")) { EebusSrvAbbruch("ProtocolHandshake: announceMax erwartet", 2); return; }
      if ((ma < 1) || (nullptr == strstr(json, "JSON-UTF8"))) { EebusSrvAbbruch("ProtocolHandshake: keine gemeinsame Version/Format", 3); return; }
      if (EebusSrvShipSendJson(eebus_srv_select)) {   // wir waehlen SHIP 1.0, JSON-UTF8
        ESrv.sme_state = 2;
        ESrv.sme_frist = Eebus126At(EEBUS_SME_PROT_TIMEOUT_MS);
      }
    } else {
      if (!EebusJsonGleich(json, eebus_srv_select)) { EebusSrvAbbruch("ProtocolHandshake: Spiegel passt nicht", 3); return; }
      EebusSrvShipSendJson("{\"connectionPinState\":[{\"pinState\":\"none\"}]}");
      ESrv.sme_state = 4;   // auf den pinState des Geraets warten (SHIP Z. 2658-2664)
      ESrv.sme_frist = Eebus126At(EEBUS_SME_PIN_TIMEOUT_MS);
    }
    return;
  }

  if (strstr(json, "\"connectionPinState\"")) {
    char pin[12] = { 0 };
    EebusJsonStr(json, "pinState", pin, sizeof(pin));
    if (0 == strcmp(pin, "required")) {   // PIN unterstuetzen wir nicht
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s verlangt PIN — nicht unterstuetzt, Verbindung beendet"), ESrv.peer_ip);
      EebusSrvFree();
      return;
    }
    if (4 == ESrv.sme_state) {   // beide pinStates bekannt, keiner "required" -> Datenphase (SHIP Z. 2747-2754)
      ESrv.sme_state = 3;
      ESrv.sme_frist = 0;
      ESrv.deadline = Eebus126At(EEBUS_SRV_DATA_TIMEOUT_MS);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV SHIP-HANDSHAKE KOMPLETT (Server-Rolle) mit %s — Datenphase erreicht ***"), ESrv.peer_ip);
      EebusSrvLinkConn();   // SPINE-Slot verknuepfen -> Datenphase antwortet
      EebusSrvShipSendJson("{\"accessMethodsRequest\":[]}");   // erst in der Datenphase (SHIP Z. 2882-2884)
    }
    return;
  }

  if (strstr(json, "\"accessMethodsRequest\"")) {   // Geraet fragt UNSERE Kennung ab
   // accessMethods.id MUSS die mDNS-id sein (eebus_adv_id), wie im Client-Weg. v242 (SHIP Tab. 22): dnsSd_mDns dazu.
    char reply[128];
    snprintf_P(reply, sizeof(reply), PSTR("{\"accessMethods\":[{\"id\":\"%s\"},{\"dnsSd_mDns\":[]}]}"), eebus_adv_id);
    EebusSrvShipSendJson(reply);
    return;
  }

  if (strstr(json, "\"accessMethods\"")) {   // Kennung des Geraets
    char pid[64] = { 0 };
    EebusJsonStr(json, "id", pid, sizeof(pid));
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s AccessMethods id=%s"), ESrv.peer_ip, pid);
    if (ESrv.conn_idx >= 0) {   // Kennung in den SPINE-Slot uebernehmen
      strlcpy(EConn[ESrv.conn_idx].peer_id, pid, sizeof(EConn[ESrv.conn_idx].peer_id));
      EebusSrvSkiFromId(ESrv.conn_idx, pid);   // v188
    }
    return;
  }
}

// v242: Abbruch als Server (SHIP Z. 2292-2297, 2441-2446): erst die passende Meldung, dann schliessen.
void EebusSrvAbbruch(const char *why, int prot_err) {
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: %s - Abbruch"), ESrv.peer_ip, why);
  if (0 == ESrv.sme_state) {
    EebusSrvShipSendJson("{\"connectionHello\":[{\"phase\":\"aborted\"}]}");
  } else if ((1 == ESrv.sme_state) || (2 == ESrv.sme_state)) {
    char e[64]; snprintf(e, sizeof(e), "{\"messageProtocolHandshakeError\":[{\"error\":%d}]}", prot_err ? prot_err : 2);
    EebusSrvShipSendJson(e);
  }
  EebusSrvFree();
}

// v242 (SHIP Z. 2125-2126, 2194-2199): nach der CMI sofort das eigene Hello - "ready" nur fuer vertraute Anrufer.
bool EebusSrvHelloStart(void) {
  const char *ski = (ESrv.x509 && ESrv.x509->have) ? ESrv.x509->ski : "";
  if (!EebusSkiVertraut(ski)) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s (SKI %s) ist nicht vertraut - abgewiesen (zulassen: EEBusTrust <SKI>)"),
           ESrv.peer_ip, ski[0] ? ski : "-");
    EebusLbLog("Anrufer abgewiesen - nicht vertraut", ski, ESrv.peer_ip, -1, "", 0, "SKI nicht in Partner-/Vertrauensliste (EEBusTrust)");
    EebusSrvShipSendJson("{\"connectionHello\":[{\"phase\":\"aborted\"}]}");
    EebusSrvFree();
    return false;
  }
  if (!EebusSrvShipSendJson("{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":60000}]}")) { EebusSrvFree(); return false; }
  ESrv.sme_state = 0;
  ESrv.hello_start = millis();
  ESrv.sme_frist = Eebus126At(EEBUS_SME_HELLO_TIMEOUT_MS);   // Wait-For-Ready-Timer (T_hello_init 60 s)
  return true;
}

// v231: eine vollstaendige WS-Nachricht (aus einem Frame oder zusammengesetzt) verarbeiten. pl[len] muss beschreibbar
// sein (wird kurz NUL-terminiert). Rueckgabe false = Verbindung wurde beendet, Puffer nicht mehr anfassen.
bool EebusSrvMsgHandle(uint8_t *pl, size_t len) {
  if (!ESrv.cmi_done) {
   // SHIP 13.4.3 CMI. v242 (SHIP Z. 2052-2067): nur MessageType und CmiHead auswerten; ist es keine CMI mit Kopf 0,
   // trotzdem eine CMI senden und dann schliessen.
      uint8_t cmi[2] = { 0x00, 0x00 };
      const bool gut = (len >= 2) && (0x00 == pl[0]) && (0x00 == pl[1]);
      const bool gesendet = EebusSrvWsSendOp(cmi, 2, 0x2);
      if (!gut || !gesendet) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: %s - Verbindung beendet"), ESrv.peer_ip,
               gut ? "CMI-Antwort fehlgeschlagen" : "keine gueltige CMI (SHIP 13.4.3)");
        EebusSrvFree();
        return false;
      }
      ESrv.cmi_done = true;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV CMI mit %s beantwortet — SHIP-Kanal steht ***"), ESrv.peer_ip);
      if (!EebusSrvHelloStart()) { return false; }
    } else if (len > 0) {
   // SHIP-Nachricht: [0]=Klassifizierer (1=control/SME, 2=data/SPINE, 3=close), dann JSON.
   // Payload liegt komplett im abuf -> temporaer NUL-terminieren fuer strstr-Parsing.
      uint8_t saved = pl[len];
      pl[len] = '\0';
      if (2 != pl[0]) {   // klass=2 loggt der Dispatch via EebusShipLog (voll)
        char txt[161];
        size_t show = (len > 1) ? len - 1 : 0;
        if (show > sizeof(txt) - 1) { show = sizeof(txt) - 1; }
        for (size_t i = 0; i < show; i++) {
          txt[i] = ((pl[1 + i] >= 32) && (pl[1 + i] < 127)) ? (char)pl[1 + i] : '.';
        }
        txt[show] = '\0';
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV SHIP RX von %s klass=%d len=%u: %s"),
               ESrv.peer_ip, pl[0], (uint32_t)len, txt);
      }
      EebusSrvSmeHandle(pl[0], (const char*)pl + 1);   // SME-Server-Zustandsmaschine
      if (!ESrv.active) { return false; }   // Handler hat die Verbindung beendet (Puffer weg!)
      pl[len] = saved;
  }
  return true;
}

// angesammelte App-Daten verarbeiten — Phase 0: HTTP-Upgrade-Request bis \r\n\r\n
// sammeln + 101 antworten; Phase 1: WS-Frames parsen (Client-Frames sind MASKIERT),
// CMI [0,0] beantworten, SHIP-Nachrichten an den SME-Server-Handler geben.
void EebusSrvAppData(void) {
  if (0 == ESrv.ws_state) {
    if (ESrv.abuf_len < 4) { return; }
    ESrv.abuf[ESrv.abuf_len] = '\0';
    char *hdr_end = strstr((char*)ESrv.abuf, "\r\n\r\n");
    if (nullptr == hdr_end) {
      if (ESrv.abuf_len > EEBUS_SRV_ABUF - 8) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: HTTP-Request ohne Ende, Verbindung beendet"), ESrv.peer_ip);
        EebusSrvFree();
      }
      return;   // Request noch unvollstaendig
    }
    {   // v242 (SHIP Z. 802, 818-821, RFC 6455 4.2.1): Anfrage pruefen, sonst 400
      char v[64];
      const char *h = (const char*)ESrv.abuf;
      const bool gut = EebusHdrWert(h, "Upgrade", v, sizeof(v)) && EebusTextHatI(v, "websocket") &&
                       EebusHdrWert(h, "Sec-WebSocket-Version", v, sizeof(v)) && (0 == strcmp(v, "13")) &&
                       EebusHdrWert(h, "Sec-WebSocket-Protocol", v, sizeof(v)) && EebusTextHatI(v, "ship") &&
                       EebusHdrWert(h, "Sec-WebSocket-Key", v, sizeof(v));
      if (!gut) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: WS-Anfrage ohne websocket/Version 13/ship - 400, Verbindung beendet"), ESrv.peer_ip);
        static const char r400[] = "HTTP/1.1 400 Bad Request\r\nSec-WebSocket-Version: 13\r\nContent-Length: 0\r\n\r\n";
        EebusSrvAppWrite((const uint8_t*)r400, sizeof(r400) - 1);
        EebusSrvFree();
        return;
      }
    }
    bool ok = EebusSrvWsUpgradeReply();
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s SRV WS-Upgrade von %s (101, Subprotocol ship)"),
           ok ? "***" : "FEHLER:", ESrv.peer_ip);
    if (!ok) { EebusSrvFree(); return; }
    size_t used = (uint8_t*)hdr_end + 4 - ESrv.abuf;
    memmove(ESrv.abuf, ESrv.abuf + used, ESrv.abuf_len - used);
    ESrv.abuf_len -= used;
    ESrv.ws_state = 1;
  }
  while (1 == ESrv.ws_state) {   // alle vollstaendigen WS-Frames abarbeiten
    if (ESrv.abuf_len < 2) { return; }
    uint8_t *p = ESrv.abuf;
    bool    fin    = (0 != (p[0] & 0x80));
    uint8_t opcode = p[0] & 0x0F;
    bool    masked = (0 != (p[1] & 0x80));
    size_t  len    = p[1] & 0x7F;
    size_t  ho     = 2;
    if (126 == len) {
      if (ESrv.abuf_len < 4) { return; }
      len = ((size_t)p[2] << 8) | p[3];
      ho = 4;
    } else if (127 == len) {
      if (ESrv.abuf_len < 10) { return; }
      len = ((size_t)p[6] << 24) | ((size_t)p[7] << 16) | ((size_t)p[8] << 8) | p[9];
      ho = 10;
    }
    uint8_t mask[4] = { 0, 0, 0, 0 };
    if (masked) {
      if (ESrv.abuf_len < ho + 4) { return; }
      memcpy(mask, p + ho, 4);
      ho += 4;
    }
    if (len > EEBUS_SRV_ABUF - 1 - ho) {   // Frame passt nie in den Puffer
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: WS-Frame %u B > Puffer, Verbindung beendet"),
             ESrv.peer_ip, (uint32_t)len);
      EebusSrvFree();
      return;
    }
    if (ESrv.abuf_len < ho + len) { return; }   // Frame noch nicht komplett
    uint8_t *pl = p + ho;
    if (masked) { for (size_t i = 0; i < len; i++) { pl[i] ^= mask[i & 3]; } }

    if (0x8 == opcode) {   // Close
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s sendet WS-Close"), ESrv.peer_ip);
      EebusSrvWsSendOp(nullptr, 0, 0x8);   // Close quittieren (RFC 6455 5.5.1)
      EebusSrvFree();
      return;
    } else if (0x9 == opcode) {   // Ping -> Pong gleiche Payload
      EebusSrvWsSendOp(pl, len, 0xA);
    } else if (0xA == opcode) {   // v231: Pong (lief bisher als SHIP-Nachricht in den Parser)
      if (ESrv.conn_idx >= 0) { EConn[ESrv.conn_idx].pong_wait_at = 0; }   // v242
    } else if ((0x0 != opcode) && (0x2 != opcode)) {   // v242 (SHIP Z. 826-830): Text -> 1003, reserviert -> 1002
      const uint8_t cs[2] = { 0x03, (uint8_t)((0x1 == opcode) ? 0xEB : 0xEA) };
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s sendet WS-Opcode %u - Close %u"), ESrv.peer_ip, opcode, (0x1 == opcode) ? 1003 : 1002);
      EebusSrvWsSendOp(cs, 2, 0x8);
      EebusSrvFree();
      return;
    } else if (!fin || (0x0 == opcode)) {
       // v231 (Pruefung 4.10): zerstueckelte WS-Nachricht zusammensetzen (RFC 6455 5.4), wie im Client-Weg
       // (EebusWsRecv). Bis v230 wurde jedes Fragment verworfen - eine grosse Nachricht der Gegenstelle ging verloren.
      if (0x0 != opcode) { ESrv.frag_len = 0; ESrv.frag_on = true; }   // Anfang (verschachtelter Anfang: neu beginnen)
      if (!ESrv.frag_on) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: WS-Fortsetzung ohne Anfang (%u B) verworfen"), ESrv.peer_ip, (uint32_t)len);
      } else {
        if (nullptr == ESrv.frag) { ESrv.frag = (uint8_t*)special_malloc(EEBUS_SRV_ABUF); }
        if ((nullptr == ESrv.frag) || (ESrv.frag_len > EEBUS_SRV_ABUF - 1) || (len > EEBUS_SRV_ABUF - 1 - ESrv.frag_len)) {
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: zerstueckelte WS-Nachricht zu gross oder kein Speicher - verworfen"), ESrv.peer_ip);
          ESrv.frag_on = false;
          ESrv.frag_len = 0;
        } else {
          memcpy(ESrv.frag + ESrv.frag_len, pl, len);
          ESrv.frag_len += len;
          if (fin) {   // letztes Fragment: ganze Nachricht verarbeiten
            ESrv.frag_on = false;
            size_t fl = ESrv.frag_len;
            ESrv.frag_len = 0;
            if (!EebusSrvMsgHandle(ESrv.frag, fl)) { return; }   // Verbindung beendet (Puffer weg!)
          }
        }
      }
    } else {
      if (!EebusSrvMsgHandle(pl, len)) { return; }   // Verbindung beendet (abuf weg!)
    }
    size_t used = ho + len;   // Frame aus dem Puffer nehmen
    memmove(ESrv.abuf, ESrv.abuf + used, ESrv.abuf_len - used);
    ESrv.abuf_len -= used;
  }
}

// Engine-Pumpe, nicht-blockierend, alle 100 ms: TLS-Records rein/raus, Handshake-
// Fortschritt, entschluesselte Anwendungsdaten loggen (Etappe 1: nur zeigen).
void EebusSrvPoll(void) {
  if (!ESrv.active) { return; }
  br_ssl_engine_context *eng = &ESrv.sc->eng;
  if (!ESrv.sock.connected() && (0 == ESrv.sock.available())) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s hat getrennt (Handshake %s)"),
           ESrv.peer_ip, ESrv.hs_done ? "war FERTIG" : "unvollstaendig");
    EebusSrvFree();
    return;
  }
  if (ESrv.sme_frist && TimeReached(ESrv.sme_frist)) {   // v242: Wait-For-Ready / Wait-Timer / pinState
    ESrv.sme_frist = 0;
    EebusSrvAbbruch((0 == ESrv.sme_state) ? "Hello: Wait-For-Ready abgelaufen" :
                    (4 == ESrv.sme_state) ? "kein pinState binnen 10 s" : "ProtocolHandshake: Wait-Timer abgelaufen", 1);
    return;
  }
  if (TimeReached(ESrv.deadline) && !(ESrv.cmi_done && (0 == ESrv.sme_state) && ESrv.sme_frist)) {   // v242: im Hello gilt die Hello-Frist
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV Timeout %s (Handshake %s)"),
           ESrv.peer_ip, ESrv.hs_done ? "fertig, keine Daten mehr" : "unvollstaendig");
    EebusSrvFree();
    return;
  }
  if (ESrv.hold_len && ESrv.abuf) {   // v206: waehrend des Sendens Empfangenes zuerst in den Parser
    size_t room = EEBUS_SRV_ABUF - 1 - ESrv.abuf_len;
    size_t take = (ESrv.hold_len > room) ? room : ESrv.hold_len;
    if (take > 0) {
      memcpy(ESrv.abuf + ESrv.abuf_len, ESrv.hold, take);
      ESrv.abuf_len += take;
      memmove(ESrv.hold, ESrv.hold + take, ESrv.hold_len - take);
      ESrv.hold_len -= take;
      EebusSrvAppData();
      if (!ESrv.active) { return; }
      if (ESrv.send_kaputt) { EebusSrvFree(); return; }
    }
  }
  for (int guard = 0; guard < 50; guard++) {   // Arbeit pro Tick begrenzen (Loop-Task!)
    unsigned state = br_ssl_engine_current_state(eng);
    if (state & BR_SSL_CLOSED) {
      int err = br_ssl_engine_last_error(eng);
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV TLS beendet %s, err=%d (0=sauber; 62=bad_cert-Alert des Peers)"),
             ESrv.peer_ip, err);
      EebusSrvFree();
      return;
    }
    if (state & BR_SSL_SENDREC) {   // ausgehende Records haben Vorrang
      size_t len;
      unsigned char *buf = br_ssl_engine_sendrec_buf(eng, &len);
      int wlen = EebusSrvSockSend(buf, len);   // v205: nicht blockierend
      if (wlen < 0) { EebusSrvFree(); return; }
      if (wlen > 0) { br_ssl_engine_sendrec_ack(eng, wlen); continue; }
       // v204: Socket-Sendepuffer voll - trotzdem lesen, falls die Gegenstelle etwas schickt (sonst warten beide)
      if (!(state & (BR_SSL_RECVAPP | BR_SSL_RECVREC))) { return; }   // nichts zu lesen -> naechster Tick
    }
    if (!ESrv.hs_done && (state & (BR_SSL_SENDAPP | BR_SSL_RECVAPP))) {
      ESrv.hs_done = true;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV TLS-SERVER-HANDSHAKE FERTIG mit %s (Suite 0x%04X) ***"),   // v231: ausgehandelte Suite statt fester Text
             ESrv.peer_ip, (unsigned)ESrv.sc->eng.session.cipher_suite);
    }
    if (state & BR_SSL_RECVAPP) {   // entschluesselte Daten -> Ansammelpuffer -> Parser
      if (ESrv.hold_len) { return; }   // v206: erst der Zwischenspeicher (Reihenfolge der Daten bleibt erhalten)
      size_t len;
      unsigned char *buf = br_ssl_engine_recvapp_buf(eng, &len);
      size_t room = EEBUS_SRV_ABUF - 1 - ESrv.abuf_len;   // -1: Platz fuer NUL (HTTP-Phase)
      size_t take = (len > room) ? room : len;
      if (take > 0) {
        memcpy(ESrv.abuf + ESrv.abuf_len, buf, take);
        ESrv.abuf_len += take;
        br_ssl_engine_recvapp_ack(eng, take);   // Rest bleibt in der Engine fuer die naechste Runde
      }
      EebusSrvAppData();   // HTTP-Upgrade / WS-Frames / CMI verarbeiten
      if (!ESrv.active) { return; }   // Parser hat die Verbindung beendet/freigegeben
      if (ESrv.send_kaputt) { EebusSrvFree(); return; }   // v204: Datenstrom zerstoert (siehe EebusSrvAppWrite)
      ESrv.deadline = Eebus126At(((3 == ESrv.sme_state) ? EEBUS_SRV_DATA_TIMEOUT_MS
                                                        : EEBUS_SRV_TIMEOUT_MS));
      if ((0 == take) && (ESrv.abuf_len >= EEBUS_SRV_ABUF - 1)) {
   // Puffer voll und der Parser konnte nichts freiraeumen -> Frame zu gross
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: App-Puffer voll ohne kompletten Frame, Verbindung beendet"),
               ESrv.peer_ip);
        EebusSrvFree();
        return;
      }
      continue;
    }
    if (state & BR_SSL_RECVREC) {   // Engine will TLS-Records vom Socket
      if (ESrv.sock.available() > 0) {
        size_t len;
        unsigned char *buf = br_ssl_engine_recvrec_buf(eng, &len);
        int rlen = ESrv.sock.read(buf, len);
        if (rlen > 0) { br_ssl_engine_recvrec_ack(eng, rlen); continue; }
      }
      return;   // keine Daten da -> naechster Tick
    }
    br_ssl_engine_flush(eng, 0);
    return;
  }
}

// Listener + Accept: eingehende Verbindung in den Server-Slot uebernehmen.
// NetworkServer = ESP32-Core, funktioniert ueber WiFi UND Ethernet (W5500).
void EebusInboundAccept(void) {
  if (nullptr == EebusInboundSrv) {
    if (!WifiHasIP() && (0 == (uint32_t)EthernetLocalIP())) { return; }   // Netz noetig
    EebusInboundSrv = new NetworkServer(EEBUS_ADV_PORT);
    EebusInboundSrv->begin();
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV SHIP-Server (Etappe 1: TLS) lauscht auf Port %d"), EEBUS_ADV_PORT);
  }
  NetworkClient c = EebusInboundSrv->accept();
  if (c) {
    IPAddress rip = c.remoteIP();
    if (ESrv.active) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV eingehende Verbindung von %s abgewiesen (Slot belegt durch %s)"),
             rip.toString().c_str(), ESrv.peer_ip);
      c.stop();
    } else {
      EebusSrvFree();   // Reste eines frueheren Versuchs abraeumen
      ESrv.sock = c;
      strlcpy(ESrv.peer_ip, rip.toString().c_str(), sizeof(ESrv.peer_ip));
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV EINGEHENDE Verbindung von %s -> TLS-SERVER-Handshake ***"), ESrv.peer_ip);
      if (!EebusSrvStart()) { EebusSrvFree(); }
    }
  }
  EebusSrvPoll();
}

// v244: eigenes Netz wieder da -> wartende Keep-Alive-Verbindungen sofort nachsehen lassen (Kopf v244).
static bool eebus_netz_da = false;
void EebusNetzZurueck(void) {
  for (int ci = 0; ci < EEBUS_MAX_CONN; ci++) {
    EebusConn *c = &EConn[ci];
    if ((SME_OFF != c->sme) || !c->keepalive || c->via_srv || (c->peer_idx < 0)) { continue; }
    c->ka_lost_at = millis(); if (!c->ka_lost_at) { c->ka_lost_at = 1; }   // schnelle Phase (5 s) neu
    if (3 == c->ka_ping_res) { c->ka_ping_res = 0; }   // altes "keine Antwort" verwerfen; laufenden Ping nicht anfassen
    c->reconnect_at = Eebus126At(500);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Netz wieder da - Wiederaufbau sofort (Slot %d @ %s)"), ci, c->peer_ip);
    EebusLbLog("Netz wieder da - Wiederaufbau sofort", c->peer_ski, c->peer_ip, -1, "", 0, "");
  }
}

bool Xdrv126(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_EVERY_100_MSECOND:
      EebusLogFlush();        // v177: waehrend eines Web-Befehls zurueckgehaltene Telegramm-Zeilen
      EebusInboundAccept();   // SHIP-Server Etappe 1 (TLS-Server-Handshake)
      if (Eebus.scan) { EebusPollScan(); }
      for (int ci = 0; ci < EEBUS_MAX_CONN; ci++) {   // M2b: SME-Handshake + RX je Slot
        ESp = &EConn[ci]; Eebus126ModeLoad();
        if (ESp->ka_ping_h && ((SME_OFF != ESp->sme) || ESp->via_srv)) { EebusKaPingAufraeumen(); }   // v231 (4.13): fertige
          // Ping-Sitzung auch loeschen, wenn der Slot inzwischen verbunden oder von einem Anrufer uebernommen ist
        EebusSmePoll();
      }
      break;
    case FUNC_EVERY_SECOND:
      EebusTotePlaetzeRaeumen();   // v247: tote Plaetze derselben Gegenstelle leeren
      EebusLbVerbindungen();   // v215: Betriebslogbuch - Verbindung hergestellt / Stoerung
   // HEAP-STOLPERDRAHT: Sekunden-Netz unter den Praezisions-Checkpunkten — faengt
   // Korruption, die ZWISCHEN den Checkpunkten entsteht (z.B. lwip-/W5500-Task-Kontext).
      EebusHeapCheck("sekunden_tick");
   // M3: einmalig als SHIP-Dienst ankuendigen, sobald Netzwerk steht (damit wir bei
   // Energiemanager bzw. Hersteller-App unter "Verfuegbare Geraete" erscheinen). mDNS braucht ein aktives IF.
      if (!Eebus.advertised &&
          (WifiHasIP() || ((uint32_t)EthernetLocalIP() != 0))) {
        EebusMdnsAdvertise();
      }
      EebusAutoConnectRun();   // selbsttaetiger Wiederaufbau nach einem Neustart
#ifdef USE_WEBSERVER
      EebusWebClickRun();   // vorgemerkte Verbinden/Trennen-Klicks (NIE im HTTP-Handler)
#endif
      EebusLpcExpireCheck();   // "BESTAETIGT aktiv" -> "ausgelaufen", sobald die Dauer um ist
      EebusDoppelPruefen();    // v242 (SHIP Z. 1353-1366)
   // Vorgemerkte Leseanfragen (EEBusRead) absetzen — bewusst HIER und nicht im Kommando selbst:
   // wird waehrend der Kommandobearbeitung gesendet, kommt der gesendete Rahmen vermischt mit der
   // Kommandoantwort aus /cm heraus (gemessen 29.07.). Die Verzoegerung von bis zu einer Sekunde
   // ist unerheblich, die Antwort wird ohnehin abgeholt.
      for (int rdi = 0; rdi < EEBUS_MAX_CONN; rdi++) {
        if (!EConn[rdi].active || (SME_DONE != EConn[rdi].sme) || (EConn[rdi].rd_pend_ent < 0)) { continue; }
        EebusConn *rd_save = ESp;
        ESp = &EConn[rdi]; Eebus126ModeLoad();
        int rde = ESp->rd_pend_ent, rdf = ESp->rd_pend_feat;
        char rdcmd[352];
   // OHNE Selektor: leerer Datensatz = "gib alles her". MIT Selektor: genau eine Kennung —
   // dieselbe Form, die diese Gegenstelle beim Limit-Lesen nachweislich akzeptiert
   // (function + filter{cmdControl:partial + <Funktion>Selectors} + leerer Datensatz).
   // Warum das noetig ist: ein leerer Read liefert nur, was die Gegenstelle von sich aus fuehrt.
   // Ob sie eine beschriebene, aber nicht gemeldete Groesse auf ausdrueckliche Nachfrage doch
   // herausgibt, war bis hierher UNGEPRUEFT — behauptet wurde es trotzdem.
        if (ESp->rd_pend_sel >= 0) {
          const char *idn = nullptr;
          if      (0 == strncmp(ESp->rd_pend_fn, "measurement", 11))          { idn = "measurementId"; }
          else if (0 == strncmp(ESp->rd_pend_fn, "loadControlLimit", 16))     { idn = "limitId"; }
          else if (0 == strncmp(ESp->rd_pend_fn, "deviceConfigurationKey", 22)) { idn = "keyId"; }
          else if (0 == strncmp(ESp->rd_pend_fn, "electricalConnection", 20)) { idn = "parameterId"; }
          if (nullptr == idn) {
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Selektor fuer %s nicht bekannt - Read ohne Selektor"),
                   ESp->rd_pend_fn);
            snprintf_P(rdcmd, sizeof(rdcmd), PSTR("{\"%s\":[]}"), ESp->rd_pend_fn);
          } else {
            snprintf_P(rdcmd, sizeof(rdcmd),
              PSTR("{\"function\":\"%s\"},"
                   "{\"filter\":[[{\"cmdControl\":[{\"partial\":[]}]},{\"%sSelectors\":[{\"%s\":%d}]}]]},"
                   "{\"%s\":[]}"),
              ESp->rd_pend_fn, ESp->rd_pend_fn, idn, ESp->rd_pend_sel, ESp->rd_pend_fn);
          }
        } else {
          snprintf_P(rdcmd, sizeof(rdcmd), PSTR("{\"%s\":[]}"), ESp->rd_pend_fn);
        }
        ESp->rd_pend_ent = -1; ESp->rd_pend_feat = -1;   // vor dem Senden loeschen, kein Dauerfeuer
        ESp->rd_at = millis();
        EebusSpineSendAddr("read", false, 0, rdcmd, EEBUS_LPC_CLIENT_ENT, 2, rde, rdf,
                           ESp->rd_pend_elist);   // Untereinheiten wie "6,1" gehen so korrekt hinaus
        ESp->rd_mc = ESp->spine_ctr - 1;   // die gerade verbrauchte Nummer — darauf bezieht sich eine Ablehnung
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Leseanfrage %s an ent[%s]/feat%d gesendet"),
               ESp->rd_pend_fn, ESp->rd_pend_elist, rdf);
        ESp = rd_save; Eebus126ModeLoad();
      }
   // Feature-Sweep (EEBusProbe): EIN unverbindlicher LimitDescription-Read pro Sekunde
   // an ent/feat des ersten verbundenen Slots. Jede Antwort (reply/result) im RX-Log =
   // die Adresse existiert/spricht — Blind-Kartierung der Waermepumpen-Gateway (liefert keine Discovery).
      if (eebus_probe_ent >= 0) {
        int pci = -1;
        for (int i = 0; i < EEBUS_MAX_CONN; i++) {
          if (EConn[i].active && (SME_DONE == EConn[i].sme)) { pci = i; break; }
        }
        if (pci < 0) {
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: PROBE abgebrochen (keine Verbindung mehr)"));
          eebus_probe_ent = -1;
        } else {
          ESp = &EConn[pci]; Eebus126ModeLoad();
          if (eebus_probe_bind) {
   // Referenz-Umsetzung-Reihenfolge — erst Subscribe, dann Binding aufs Ziel-Feature,
   // DANN der Read. Beides geht als call an das NodeManagement (ent0/feat0) des
   // Peers; client = unsere LoadControl-Client-Adresse (ent1/feat7).
            char own[80]; EebusOwnDevice(own, sizeof(own));
            const size_t cap = 512;
            char *cmd = (char*)special_malloc(cap);
            if (nullptr != cmd) {
              snprintf(cmd, cap,
                "{\"nodeManagementSubscriptionRequestCall\":[{\"subscriptionRequest\":["
                  "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
                  "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
                  "{\"serverFeatureType\":\"LoadControl\"}"
                "]}]}", own, EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT,
                ESp->peer_dev, (int)eebus_probe_ent, (int)eebus_probe_feat);
              EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
              snprintf(cmd, cap,
                "{\"nodeManagementBindingRequestCall\":[{\"bindingRequest\":["
                  "{\"clientAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
                  "{\"serverAddress\":[{\"device\":\"%s\"},{\"entity\":[%d]},{\"feature\":%d}]},"
                  "{\"serverFeatureType\":\"LoadControl\"}"
                "]}]}", own, EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT,
                ESp->peer_dev, (int)eebus_probe_ent, (int)eebus_probe_feat);
              EebusSpineSendAddr("call", false, 0, cmd, 0, 0, 0, 0);
              free(cmd);
            }
          }
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: PROBE %sread LimitDescription an ent%d/feat%u @ %s"),
                 eebus_probe_bind ? "subscribe+bind+" : "",
                 (int)eebus_probe_ent, eebus_probe_feat, ESp->peer_ip);
          EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitDescriptionListData\":[]}",
                             EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT,
                             (int)eebus_probe_ent, (int)eebus_probe_feat);
          if (eebus_probe_feat >= eebus_probe_end) {
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: PROBE Sweep fertig — Antworten siehe RX-Log/Mitschnitt"));
            eebus_probe_ent = -1;
          } else {
            eebus_probe_feat++;
          }
        }
      }
   // Je Verbindungs-Slot: Teardown, Heartbeat, Keep-Alive
      for (int ci = 0; ci < EEBUS_MAX_CONN; ci++) {
        ESp = &EConn[ci]; Eebus126ModeLoad();
        EebusTeardownNow();   // anstehende Client-Freigabe im sicheren Kontext
   // faelligen (verzoegerten/nachgepollten) Read-Back ausloesen. Hier statt in
   // EebusSmePoll, weil DIESER Loop auch die EINGEHENDEN (via_srv) Slots tickt — die
   // Waermepumpen-Gateway verbindet inbound, und EebusSmePoll kehrt fuer via_srv frueh zurueck.
        if (ESp->lpc_verify_at && TimeReached(ESp->lpc_verify_at)) {
          ESp->lpc_verify_at = 0;
          EebusLpcVerify();   // sendet Read-Back, setzt LPC_VERIFY + frische Deadline
        }
   // v239: ALLE Antwortfristen der LPC-Kette auch fuer via_srv (bisher nur READCFG) - EebusSmePoll kehrt dort frueh zurueck
        if (ESp->via_srv) { EebusLpcPhasenFrist(); }
   // faelliger err7-Retry -> Heartbeat+Write erneut. EebusLpcWrite() sendet im
   // HEMS-Modus selbst den Heartbeat unmittelbar vor dem Write, daher hier nur den Write erneut anstossen.
        if ((LPC_WRITE == ESp->lpc_state) && ESp->lpc_write_retry_at && TimeReached(ESp->lpc_write_retry_at)) {
          ESp->lpc_write_retry_at = 0;
          EebusLpcWrite();
        }
   // v179: faellige Nachlesung nach err7. Nur solange die Kette mit "Write abgelehnt" ruht; die
   // Antwort wertet der loadControlLimitData-Zweig aus (verspaetete Bestaetigung).
        if (ESp->lpc_rej_late && ESp->late_rd_at && TimeReached(ESp->late_rd_at)) {
          if (ESp->active && (SME_DONE == ESp->sme) && (LPC_FAIL == ESp->lpc_state) && (ESp->late_rd_n < 3)) {
            EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitListData\":[]}",
                               EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Nachlesen %u/3 nach err7 @ %s (%u s nach der Ablehnung)"),
                   ESp->late_rd_n + 1, ESp->peer_ip, kEebusLateReadS[ESp->late_rd_n]);
            ESp->late_rd_n++;
            ESp->late_rd_at = (ESp->late_rd_n < 3) ? ESp->late_rd_base + (uint32_t)kEebusLateReadS[ESp->late_rd_n] * 1000 : 0;
          } else {
            ESp->lpc_rej_late = false; ESp->late_rd_at = 0;
          }
        }
   // ANMELDUNG: nach dem Onboarding je EINE Freigabe fuer Bezug und Einspeisung schreiben.
   // Ein deaktiviertes Limit nimmt die Gegenstelle nach [LPC-905]/[LPP-905] aus "init" heraus —
   // sie fuehrt uns dann als Steuerbox, OHNE dass etwas begrenzt wird.
   // Bedingung ist NICHT eine Anzahl Heartbeats, sondern dass die Gegenstelle unseren Herzschlag
   // ABONNIERT hat (hb_sub). Die Norm verlangt genau EINEN unmittelbar vorausgehenden Heartbeat,
   // und den sendet EebusLpcWrite im HEMS-Modus ohnehin selbst direkt vor dem Write; was fehlen
   // koennte, ist allein der Empfaenger — ohne Abo kennen wir die Zieladresse nicht und der
   // Herzschlag ginge ins Leere. Die "drei Heartbeats" aus dem Erfolgslauf vom waren
   // Beobachtung, keine Regel (die Ursache war damals der Zeitstempel). Frueher angemeldet heisst
   // nach einem Stromausfall schneller wieder betriebsbereit.
   // Zwei Schritte NACHEINANDER, weil es je Verbindung nur EINE Schreib-Zustandsmaschine gibt;
   // deshalb erst weiterschalten, wenn die Kette wieder ruht.
        bool kette_ruht = (LPC_IDLE == ESp->lpc_state) || (LPC_DONE == ESp->lpc_state) || (LPC_FAIL == ESp->lpc_state);
   // v176: Ende einer Anmelde-Kette auswerten. Endete sie mit "Write abgelehnt" (LPC_FAIL), diese
   // Richtung zur Wiederholung vormerken. Muss VOR dem Start des naechsten Schritts stehen, sonst
   // ueberschreibt der neue Auftrag den Zustand der abgeschlossenen Kette.
        if ((ESp->reg_dir_running >= 0) && kette_ruht) {
          if (LPC_FAIL == ESp->lpc_state) { ESp->reg_failed |= (uint8_t)(1 << ESp->reg_dir_running); }
          ESp->reg_dir_running = -1;
        }
   // v178: steht schon eine eigene Grenze (Nutzerbefehl kurz nach dem Verbinden), entfaellt die erste
   // Anmeldung — sie ist eine Freigabe und wuerde die Grenze aufheben (wie bei der Wiederholung unten).
   // v196: Failsafe-Werte schreiben (FNN-Empfehlung Tenorziffer 2a), sobald das Onboarding durch ist und die
   // Kette ruht; einmal je Verbindung und erneut nach EEBusFailsafe.
   // v197: erst NACH der Anmeldung (lpc_reg_step 0 = keine, 3 = fertig). LPC Z.372-375: im Zustand "init"
   // wertet die Anlage andere Datenpunkte erst nach Herzschlag + erstem Limit-Write aus.
        if (!ESp->lpc_fs_done && (ESp->lpc_onboarded || ESp->sess_vorgabe) && ESp->hb_sub && kette_ruht &&   // v209: auch nach Sicherung
            (!ESp->fs_wdh_at || TimeReached(ESp->fs_wdh_at)) &&   // v239: Wartezeit nach abgelehnten Failsafe-Werten
            (ESp->reg_dir_running < 0) &&
            ((0 == ESp->lpc_reg_step) || (ESp->lpc_reg_step >= 3)) &&
            (ESp->lpc_peer_dcfg_feat >= 0) && EebusFsGrenzenDa() &&   // v243: erst nach den Grenzen beider Richtungen
            ((ESp->lpc_fs_val_key >= 0) || (ESp->lpc_fs_prod_key >= 0) || (ESp->lpc_fs_dur_key >= 0))) {
          if (!eebus_fs_loaded) { EebusFsLoad(); }
          EebusLpcFailsafeNext(0);
          kette_ruht = false;   // die Kette laeuft jetzt
        }
   // v239 ([LPC-913], LPC TS Z. 369-371): nach (Wieder-)Herstellung der Verbindung binnen 60 s Herzschlag und Grenze.
   // Kommt bis 60 s nach Beginn der Datenphase keine Vorgabe, einmal in Log und Betriebslogbuch melden (Nachweis).
        if (ESp->sess_at && ((millis() - ESp->sess_at) > 60000UL)) {   // v243: je Richtung (LPC-913 / LPP-913)
          if (!ESp->lpc913_warn && !ESp->sess_dir_w[0] && ((ESp->lpc_lid_dir[0] >= 0) || !ESp->sess_write_at)) {
            ESp->lpc913_warn = true;
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** LPC-913: %s - 60 s nach dem Verbinden noch keine Bezugs-Grenze ***"), ESp->peer_ip);
            EebusLbLog("LPC-913 Frist verfehlt", ESp->peer_ski, ESp->peer_ip, 0, "", 0, "60 s ohne Bezugs-Grenze nach dem Verbinden");
          }
          if (!ESp->lpp913_warn && !ESp->sess_dir_w[1] && (ESp->lpc_lid_dir[1] >= 0) && !EebusOhneEinspeisung()) {
            ESp->lpp913_warn = true;
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** LPP-913: %s - 60 s nach dem Verbinden noch keine Einspeise-Grenze ***"), ESp->peer_ip);
            EebusLbLog("LPP-913 Frist verfehlt", ESp->peer_ski, ESp->peer_ip, 1, "", 0, "60 s ohne Einspeise-Grenze nach dem Verbinden");
          }
        }
   // v239 (LPC TS 3.4.3.1 Nr. 3 Z. 1682, IG Z. 291-301): hat die Anlage 60 s nach Beginn der Datenphase unseren Herzschlag
   // weder abonniert noch gelesen, bekommt sie keinen und faellt nach 120 s in den Failsafe -> einmal melden.
        if (ESp->sess_at && !ESp->hb_sub && !ESp->hbabo_warn && ((millis() - ESp->sess_at) > 60000UL)) {
          ESp->hbabo_warn = true;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** %s hat unseren Herzschlag nicht abonniert - faellt nach 120 s in den Failsafe ***"), ESp->peer_ip);
          EebusLbLog("Herzschlag nicht abonniert", ESp->peer_ski, ESp->peer_ip, -1, "", 0, "Anlage abonniert unseren Herzschlag nicht (LPC TS 3.4.3.1)");
        }
   // v200: Sicherungs-Freigabe (s. Kopf v200)
        const bool sich_bereit = ESp->active && (SME_DONE == ESp->sme) && (nullptr != ESp->lpc_disco);   // v201: statt Onboarding
        if (sich_bereit && !ESp->sess_at) { ESp->sess_at = millis(); if (!ESp->sess_at) { ESp->sess_at = 1; } }
        if (sich_bereit && kette_ruht && ESp->hb_sub && (ESp->reg_dir_running < 0) && !ESp->sich_stufe &&   // v241: hb_sub (LPC-913)
            (!ESp->sich_wdh_at || TimeReached(ESp->sich_wdh_at)) &&   // v214: Wartezeit nach Ablehnung
            ESp->sess_at && ((millis() - ESp->sess_at) >=
                             ((0 == EebusModeEffective(ESp->peer_ski)) ? EEBUS_SICHERUNG_DIREKT_MS : EEBUS_SICHERUNG_MS))) {
          ESp->sich_stufe = 1;
          if (ESp->wdh_offen) {
   // v237 (Test 22.11.): WIEDERHOLUNG nach Ablehnung/Abbruch - nur die Richtungen, die es brauchen (eigene Grenze oder offene
   // Freigabe). Bis v236 wurde hier wie bei der ersten Sicherung BEIDE Richtungen geplant; die Freigabe der anderen Richtung
   // setzte sess_vorgabe, die naechste Wiederholung entfiel still ("nichts zu tun") - nach dem 1. Versuch kam keiner mehr.
            ESp->wdh_offen = false;
            for (uint8_t d = 0; d < 2; d++) {
              uint32_t own_watt; long own_rest;
              if (ESp->frei_offen[d]) { ESp->sich_plan[d] = 1; }
              else if (EebusEigeneGrenze(d, &own_watt, &own_rest)) { ESp->sich_plan[d] = 2; }
              else { ESp->sich_plan[d] = 0; }
            }
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederholung @ %s (%u.) - Bezug %s, Einspeisung %s"), ESp->peer_ip, ESp->wdh_n,
                   (2 == ESp->sich_plan[0]) ? "eigene Grenze erneut" : (ESp->sich_plan[0] ? "Freigabe" : "nichts"),
                   (2 == ESp->sich_plan[1]) ? "eigene Grenze erneut" : (ESp->sich_plan[1] ? "Freigabe" : "nichts"));
          } else if (ESp->sess_vorgabe) {
            AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: Sicherung @ %s: Vorgabe in dieser Sitzung geschrieben - nichts zu tun"), ESp->peer_ip);
          } else {
            for (uint8_t d = 0; d < 2; d++) {
              uint32_t own_watt; long own_rest;
              if (ESp->frei_offen[d]) { ESp->sich_plan[d] = 1; }   // v225: Freigabe wiederholen, nicht die noch gelesene Grenze
              else if (EebusEigeneGrenze(d, &own_watt, &own_rest)) { ESp->sich_plan[d] = 2; }   // v221: Merker slotuebergreifend
              else { ESp->sich_plan[d] = (1 == ESp->lim_act[d]) ? 0 : 1; }   // v223: eine aktive Grenze ohne Merker nicht freigeben
            }
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Sicherung @ %s: seit dem Verbinden keine Vorgabe - Bezug %s, Einspeisung %s"), ESp->peer_ip,
                   (2 == ESp->sich_plan[0]) ? "eigene Grenze erneut" : (ESp->sich_plan[0] ? "Freigabe" : "aktive Grenze bleibt"),
                   (2 == ESp->sich_plan[1]) ? "eigene Grenze erneut" : (ESp->sich_plan[1] ? "Freigabe" : "aktive Grenze bleibt"));
          }
        }
   // v225: Herzschlag zurueck ohne neue Verbindung: Logbuch und je Richtung eine Vorgabe ([LPC-913], [LPP-913])
        if (ESp->hb_zurueck) {
          ESp->hb_zurueck = false;
          EebusLbLog("Kommunikationsstoerung beendet - Herzschlag wieder da", ESp->peer_ski, ESp->peer_ip, -1, "", 0, "");
          if (sich_bereit) {
            for (uint8_t d = 0; d < 2; d++) {
              uint32_t own_watt; long own_rest;
              if (ESp->frei_offen[d]) { ESp->sich_plan[d] = 1; }
              else if (EebusEigeneGrenze(d, &own_watt, &own_rest)) { ESp->sich_plan[d] = 2; }
              else { ESp->sich_plan[d] = (1 == ESp->lim_act[d]) ? 0 : 1; }
            }
            ESp->sess_vorgabe = true;   // die Sicherung muss das nicht noch einmal planen
            ESp->sich_stufe = 1;
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Herzschlag @ %s zurueck - Bezug %s, Einspeisung %s"), ESp->peer_ip,
                   (2 == ESp->sich_plan[0]) ? "eigene Grenze erneut" : (ESp->sich_plan[0] ? "Freigabe" : "aktive Grenze bleibt"),
                   (2 == ESp->sich_plan[1]) ? "eigene Grenze erneut" : (ESp->sich_plan[1] ? "Freigabe" : "aktive Grenze bleibt"));
          }
        }
   // v200: Sicherungs-Plan abarbeiten, je Takt eine Richtung, nur bei ruhender Kette
        if ((ESp->sich_plan[0] || ESp->sich_plan[1]) && sich_bereit && kette_ruht && ESp->hb_sub && (ESp->reg_dir_running < 0)) {   // v241: hb_sub
          const uint8_t d = ESp->sich_plan[0] ? 0 : 1;
          const uint8_t was = ESp->sich_plan[d];
          ESp->sich_plan[d] = 0;
          uint32_t own_watt = 0; long own_rest = 0;
          if ((2 == was) && EebusEigeneGrenze(d, &own_watt, &own_rest)) {   // v221: zwischen Plan und Schreiben neu pruefen
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Sicherung @ %s: %s - eigene Grenze %u W erneut, %s%ld s"), ESp->peer_ip,
                   d ? "Einspeisung" : "Bezug", own_watt, own_rest ? "noch " : "unbefristet ", own_rest);
            EebusLpcStart(own_watt, true, d, own_rest);
          } else if ((1 == d) && EebusOhneEinspeisung()) {   // v203
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Sicherung @ %s: Einspeisung entfaellt - Geraet hat keine Einspeisegrenze"), ESp->peer_ip);
          } else {
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Sicherung @ %s: %s - Freigabe"), ESp->peer_ip, d ? "Einspeisung" : "Bezug");
            EebusLpcStart(ESp->lpc_val_dir[d], false, d, 0);   // v232: letzter eigener Wert dieser Richtung (bisher 0 W)
          }
          kette_ruht = false;
        }
        while (ESp->lpc_reg_step && (ESp->lpc_reg_step < 3)) {   // v222: je Richtung; v224: Schleife, s. Kopf
          const uint8_t adir = (1 == ESp->lpc_reg_step) ? 0 : 1;
          uint32_t own_watt = 0; long own_rest = 0;
          if (!((1 == ESp->lim_act[adir]) && !EebusEigeneGrenze(adir, &own_watt, &own_rest))) { break; }
          ESp->lpc_reg_step++;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung %s @ %s entfaellt - dort ist eine Grenze aktiv und darf nicht aufgehoben werden"),
                 adir ? "Einspeisung" : "Bezug", ESp->peer_ip);
        }
        if (ESp->lpc_reg_step && (ESp->lpc_reg_step < 3) && eebus_auto_reg && eebus_hems_mode &&
            ESp->lpc_onboarded && ESp->hb_sub && kette_ruht) {
          uint8_t rdir = (1 == ESp->lpc_reg_step) ? 0 : 1;
          ESp->lpc_reg_step++;
          if ((1 == rdir) && EebusOhneEinspeisung()) {   // v207: wie Sicherung/"alle" seit v203 - kein Paragraf 9
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung Einspeisung @ %s entfaellt - Geraet hat keine Einspeisegrenze"),
                   ESp->peer_ip);
          } else {
            uint32_t own_watt = 0; long own_rest = 0;
            ESp->reg_dir_running = (int8_t)rdir;   // v176
            if (EebusEigeneGrenze(rdir, &own_watt, &own_rest)) {   // v221: laufende Grenze statt Freigabe, s. Kopf
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung %s @ %s - eigene Grenze %u W erneut, %s%ld s"),
                     rdir ? "Einspeisung (Paragraf 9 EEG)" : "Bezug (Paragraf 14a EnWG)", ESp->peer_ip,
                     own_watt, own_rest ? "noch " : "unbefristet ", own_rest);
              EebusLpcStart(own_watt, true, rdir, own_rest);
            } else {
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung %s @ %s - Freigabe wird geschrieben (kein Limit)"),
                     rdir ? "Einspeisung (Paragraf 9 EEG)" : "Bezug (Paragraf 14a EnWG)", ESp->peer_ip);
              EebusLpcStart(ESp->lpc_val_dir[rdir], false, rdir, 0);   // Anmeldung: Freigabe, ohne Geltungsdauer; v232: letzter eigener Wert (bisher 0 W)
            }
          }
        }
   // v176: ABGELEHNTE ANMELDUNG WIEDERHOLEN. Erst wenn beide Schritte durch sind und keine Kette
   // laeuft; alle EEBUS_REG_RETRY_MS, hoechstens EEBUS_REG_RETRY_MAX-mal je Verbindung. Wie bei der
   // Erneuerung nach Herzschlagverlust gilt: steht eine eigene Grenze, wird NICHT freigegeben.
        else if (ESp->reg_failed && (ESp->lpc_reg_step >= 3) && (ESp->reg_dir_running < 0) &&
                 eebus_auto_reg && eebus_hems_mode && ESp->lpc_onboarded && ESp->hb_sub && kette_ruht) {
          const uint8_t adir = (ESp->reg_failed & 1) ? 0 : 1;   // v222: nur die betroffene Richtung zaehlt
          uint32_t own_watt = 0; long own_rest = 0;
          const bool grenze_aktiv = (1 == ESp->lim_act[adir]) && !EebusEigeneGrenze(adir, &own_watt, &own_rest);
          if (grenze_aktiv || (ESp->reg_retry_cnt >= EEBUS_REG_RETRY_MAX)) {
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung @ %s wird nicht mehr wiederholt (%s)"), ESp->peer_ip,
                   grenze_aktiv ? "eigene Grenze aktiv" : "Hoechstzahl erreicht");
            ESp->reg_failed = 0; ESp->reg_retry_at = 0;
          } else if (!ESp->reg_retry_at) {
            ESp->reg_retry_at = Eebus126At(EEBUS_REG_RETRY_MS);
          } else if (TimeReached(ESp->reg_retry_at)) {
            uint8_t rdir = (ESp->reg_failed & 1) ? 0 : 1;
            ESp->reg_failed &= (uint8_t)~(1 << rdir);
            ESp->reg_retry_at = 0;
            if (!((1 == rdir) && EebusOhneEinspeisung())) {   // v207: keine Einspeisegrenze -> nicht wiederholen
              ESp->reg_retry_cnt++;
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung %s @ %s war abgelehnt - Wiederholung %u/%u"),
                     rdir ? "Einspeisung (Paragraf 9 EEG)" : "Bezug (Paragraf 14a EnWG)", ESp->peer_ip,
                     ESp->reg_retry_cnt, EEBUS_REG_RETRY_MAX);
              ESp->reg_dir_running = (int8_t)rdir;
              uint32_t own_watt = 0; long own_rest = 0;
              if (EebusEigeneGrenze(rdir, &own_watt, &own_rest)) {   // v221: laufende Grenze statt Freigabe
                EebusLpcStart(own_watt, true, rdir, own_rest);
              } else {
                EebusLpcStart(ESp->lpc_val_dir[rdir], false, rdir, 0);   // v232: letzter eigener Wert dieser Richtung (bisher 0 W)
              }
            }
          }
        }
   // WEBSOCKET-PING: alle 50 s ein Ping-Frame, damit die Gegenstelle die Verbindung nicht
   // als tot abraeumt. Wir haben bisher NIE einen Ping gesendet, sondern nur auf fremde geantwortet
   // — die Referenz-Steuerbox pingt alle 50 s und rechnet mit 60 s Pong-Frist (Referenz-Umsetzung ws/types.go).
   // hat der Peer uns verloren und ist in seinen Failsafe gefallen; ein
   // stiller Kanal ist die naheliegendste Ursache. Der Ping kostet zwei Bytes.
        if (SME_DONE == ESp->sme) {
          if (!ESp->ws_ping_next) { ESp->ws_ping_next = Eebus126At(EEBUS_WS_PING_MS); }
          else if (TimeReached(ESp->ws_ping_next)) {
            ESp->ws_ping_next = Eebus126At(EEBUS_WS_PING_MS);
            EebusWsSendOp(nullptr, 0, 0x9);   // Ping; die Gegenstelle antwortet mit Pong
            if (!ESp->pong_wait_at) { ESp->pong_wait_at = millis(); if (!ESp->pong_wait_at) { ESp->pong_wait_at = 1; } }   // v242
          }
          if (ESp->pong_wait_at && ((millis() - ESp->pong_wait_at) > EEBUS_WS_PONG_MS)) {   // v242 (SHIP Z. 854-855)
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** %s: kein Pong binnen 10 s (SHIP 10.4) - Verbindung wird abgebaut ***"), ESp->peer_ip);
            EebusVerbindungWeg(false);
          }
        } else {
          ESp->ws_ping_next = 0;
        }
   // SICHERHEITSNETZ WIEDERAUFBAU: faellt eine Verbindung auf irgendeinem Weg weg, ohne dass
   // dort ein Reconnect eingeplant wurde, holt dieser Zweig es nach. Bisher planten nur ZWEI
   // Abbruchstellen den Wiederaufbau ein — blieb die Verbindung deshalb einfach weg,
   // obwohl keepalive gesetzt war. Lieber einmal zu viel geplant als eine Steuerbox, die still steht.
   // v231 (Pruefung 4.5, gleiche Klasse wie im HEMS): ein ausgehender Slot, dessen SHIP-Handshake scheiterte (SME_FAIL),
   // kam nie wieder nach SME_OFF - der Wiederaufbau unten setzt SME_OFF voraus und blieb fuer immer still. Ist der
   // Client freigegeben, gilt der Slot wieder als getrennt; die Wartezeit regelt EebusKaWaitMs (bei "pending" 5 min).
        if ((SME_FAIL == ESp->sme) && ESp->keepalive && !ESp->via_srv && !ESp->active && !ESp->teardown &&
            (nullptr == ESp->client)) {
          ESp->sme = SME_OFF;
          ESp->state = SHIP_IDLE;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: %s: Handshake war gescheitert - Wiederaufbau wird eingeplant"), ESp->peer_ip);
        }
        if ((SME_OFF == ESp->sme) && ESp->keepalive && !ESp->reconnect_at && (ESp->peer_idx >= 0)) {
          ESp->reconnect_at = Eebus126At(EebusKaWaitMs());
        }
   // Heartbeat-NOTIFY alle 20 s an den DeviceDiagnosis-Leser/Abonnenten (Verbindungsueberwachung).
   // 20 s (war 30) — muss UNTER Ladestations Ablauf (unser gemeldeter PT30S -> ~31 s) bleiben, sonst
   // faellt heartbeat_received ab und die Ladestation springt trotz gesetztem Limit auf Failsafe 22000.
        if ((SME_DONE == ESp->sme) && ESp->hb_sub && ESp->hb_next && TimeReached(ESp->hb_next)) {
          ESp->hb_next = Eebus126At((eebus_hems_mode ? EEBUS_HEMS_HB_MS : 20000));   // HEMS 4 s (v175, zu PT10S), Ladestation 20 s
          EebusSpineSendHeartbeat("notify", false, 0, ESp->hb_cli_ent, ESp->hb_cli_feat);
          AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: Heartbeat %u an %s gesendet"), ESp->hb_counter - 1, ESp->peer_ip);
        }
   // v182: STUMME VERBINDUNG. Die Leitung steht, aber von der Gegenstelle kommt gar nichts mehr —
   // kein Messwert, kein Herzschlag, keine Antwort. Bisher blieb die Box dann "verbunden" und taub.
   // Nach EEBUS_SILENT_S ohne jede Nachricht: abbauen, der Keep-Alive baut neu auf. Nur ausgehende
   // Verbindungen (eingehende baut die Gegenstelle selbst wieder auf).
        if ((SME_DONE == ESp->sme) && !ESp->via_srv && ESp->keepalive && ESp->last_rx &&
            TimeReached(ESp->last_rx + (EEBUS_SILENT_S * 1000UL))) {
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** %s seit %u s stumm - Verbindung wird abgebaut und neu aufgebaut ***"),
                 ESp->peer_ip, (uint32_t)EEBUS_SILENT_S);
          EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
          ESp->sme = SME_OFF;
          ESp->state = SHIP_IDLE;
          ESp->last_rx = 0;
          if (ESp->peer_idx >= 0) { ESp->reconnect_at = Eebus126At(EebusKaWaitMs()); }
          EebusTeardownLater();
        }
   // v231: LEBENSZEICHEN PER PING (SHIP Kap. 8.2). Faellt die Gegenstelle aus (Kabel, Strom), merkt die Box das bisher
   // erst nach EEBUS_SILENT_S (180 s) Stille; die alte TCP-Verbindung haengt so lange, und die Gegenstelle faellt nach
   // ihrer Herzschlag-Frist (120 s, [LPC-911]) in den Failsafe, obwohl sie laengst wieder da ist. Jetzt bei stehender
   // ausgehender Verbindung alle EEBUS_LIFE_PING_MS ein Ping. Erst wenn die Gegenstelle EINMAL geantwortet hat, ist die
   // Pruefung scharf (Geraete, die Ping sperren, bleiben unberuehrt). Abgebaut wird nur, wenn EEBUS_LIFE_LOST_MS lang
   // weder eine Ping-Antwort noch irgendeine Nachricht kam - ein verlorener Ping allein reicht nie. Danach baut der
   // Wiederaufbau (v229/v230, Ping vor dem Anruf) neu auf.
   // v233 (07.11., meierchen006): KEIN Dauer-Ping mehr. Box v231/v232 stuerzte im Testnetz viermal ab (07.11. ~10:09, 11:02,
   // 11:10, 13:41: DebugException in vTaskSwitchContext, EPC 4037e9bd - dieselbe Adresse wie der Ping-Absturz von v229),
   // Box v224 (ohne Ping) lief 2 Tage 16 h ohne Absturz. Der Lebenszeichen-Ping legte alle 3 s eine neue esp_ping-Task an
   // und loeschte sie wieder (rund 1200 je Stunde). Ersatz ohne eigene Task: Schickt die Gegenstelle Herzschlaege
   // (peer_hb_at gesetzt, ein HEMS z. B. alle 4-10 s) und kam EEBUS_LIFE_SILENT_MS lang GAR NICHTS mehr, ist die Verbindung
   // tot -> abbauen, der Wiederaufbau (Ping vor dem Anruf, v229/v230) baut neu auf. Gegenstellen ohne eigenen Herzschlag
   // bleiben bei EEBUS_SILENT_S (180 s), damit eine stille SteuVE nicht faelschlich getrennt wird.
        if ((SME_DONE == ESp->sme) && !ESp->via_srv && ESp->keepalive && ESp->peer_ip[0] &&
            ESp->peer_hb_at && ESp->last_rx &&
            ((millis() - ESp->last_rx) >= ((ESp->peer_hb_tmo_s * 2000UL > EEBUS_LIFE_SILENT_MS) ? ESp->peer_hb_tmo_s * 2000UL : EEBUS_LIFE_SILENT_MS))) {   // v239: mind. 2x ihre Zusage
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** %s seit %u s ohne jede Nachricht (Herzschlag bleibt aus) - Verbindung wird abgebaut und neu aufgebaut ***"),
                 ESp->peer_ip, (uint32_t)(EEBUS_LIFE_SILENT_MS / 1000));
          EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
          ESp->sme = SME_OFF;
          ESp->state = SHIP_IDLE;
          ESp->last_rx = 0;
          if (ESp->peer_idx >= 0) { ESp->reconnect_at = Eebus126At(EebusKaWaitMs()); }
          EebusTeardownLater();
        }
   // v241 (LPC TS Z. 1466): abgelehnte Abos regelmaessig lesen
        if ((SME_DONE == ESp->sme) && ESp->abo_poll_at && TimeReached(ESp->abo_poll_at)) {
          ESp->abo_poll_at = Eebus126At(EEBUS_ABO_POLL_MS);
          EebusAboPoll();
        }
   // HERZSCHLAG DER GEGENSTELLE UEBERWACHEN — die Gegenprobe zum Senden oben.
   // Bewertet wird IHRE EIGENE Zusage (heartbeatTimeout), verdoppelt als Reserve: ein einzelner
   // ausgefallener Herzschlag soll noch keinen Alarm ausloesen. Ohne je empfangenen Herzschlag
   // (peer_hb_at == 0) wird NICHT geprueft — es gibt Gegenstellen, die gar keinen schicken, und
   // die duerfen nicht dauerhaft als verloren gelten.
        if ((SME_DONE == ESp->sme) && ESp->peer_hb_at && !ESp->peer_hb_lost) {
          uint32_t frist_s = (uint32_t)(ESp->peer_hb_tmo_s ? ESp->peer_hb_tmo_s : 120) * 2;
          if (TimeReached(ESp->peer_hb_at + (frist_s * 1000UL))) {
            ESp->peer_hb_lost = true;
            ESp->hb_lost_cnt++;
            strlcpy(ESp->hb_lost_at, GetDateAndTime(DT_LOCAL).c_str(), sizeof(ESp->hb_lost_at));
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** Herzschlag von %s seit ueber %u s aus (zugesagt waren %u s) - Beziehung gilt als verloren. Vorfall %u am %s ***"),
                   ESp->peer_ip, frist_s, ESp->peer_hb_tmo_s, ESp->hb_lost_cnt, ESp->hb_lost_at);
            {   // v225: FNN-Empfehlung, Verbindungsueberwachung: Kommunikationsstoerung protokollieren
              char lbg[32]; snprintf(lbg, sizeof(lbg), "ueber %u s kein Herzschlag", frist_s);
              EebusLbLog("Kommunikationsstoerung - Herzschlag aus", ESp->peer_ski, ESp->peer_ip, -1, "", 0, lbg);
            }
            EebusAboErneuern(1);   // v241 (SPINE Z. 2406-2409 a): Herzschlag-Abo erneuern und einmal lesen
   // ⚠️ ANMELDUNG NUR ERNEUERN, SOLANGE KEINE EIGENE GRENZE AKTIV IST. Die Anmeldung ist eine
   // FREIGABE (isLimitActive:false) — sie wuerde eine gesetzte Begrenzung aufheben. Geprueft wird
   // beides: der eigene Merker aus dem letzten Schreibauftrag UND der zuletzt zurueckgelesene
   // Zustand beider Grenzen. Im Zweifel wird NICHT erneuert.
            bool grenze_aktiv = ESp->lpc_our_limit || (1 == ESp->lim_act[0]) || (1 == ESp->lim_act[1]);
            if (grenze_aktiv) {
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung wird NICHT erneuert - eine Grenze ist aktiv und darf nicht aufgehoben werden"));
            } else if (eebus_auto_reg && eebus_hems_mode && ESp->lpc_onboarded && ESp->hb_sub) {
              ESp->lpc_reg_step = 1;   // beide Richtungen nacheinander erneut anmelden
              ESp->reg_dir_running = -1; ESp->reg_failed = 0; ESp->reg_retry_cnt = 0; ESp->reg_retry_at = 0;   // v176
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung wird erneuert - keine eigene Grenze aktiv"));
            }
          }
        }
   // Keep-Alive — Verbindung nach Abbruch wieder aufbauen (gruenes Kettenglied halten).
   // Peer-Index gegen die SKI absichern: die Scan-Reihenfolge kann sich geaendert haben!
        if ((SME_OFF == ESp->sme) && ESp->keepalive &&
            ESp->reconnect_at && TimeReached(ESp->reconnect_at)) {
          ESp->reconnect_at = 0;
          int idx = ESp->peer_idx;
          if ((idx < 0) || (idx >= Eebus.peer_count) ||
              (0 != strcasecmp(Eebus.peers[idx].ski, ESp->peer_ski))) {
            idx = -1;
            for (uint32_t k = 0; k < Eebus.peer_count; k++) {
              if (0 == strcasecmp(Eebus.peers[k].ski, ESp->peer_ski)) { idx = (int)k; break; }
            }
          }
   // Hat die Gegenstelle uns inzwischen selbst angerufen (anderer Slot, gleiche SKI, aktiv)? Dann NICHT
   // zusaetzlich anrufen: der Verbindungsaufbau blockiert den Hauptablauf, die eingehende Verbindung
   // verhungert und die Gegenstelle legt auf (05.10. 21:18-21:20 belegt). Spaeter erneut nachsehen.
          bool schon_da = false;
          for (uint32_t k = 0; k < EEBUS_MAX_CONN; k++) {
            if (((int)k != ci) && EConn[k].active && (0 == strcasecmp(EConn[k].peer_ski, ESp->peer_ski))) { schon_da = true; break; }
          }
          if (schon_da) {
            ESp->reconnect_at = Eebus126At(EEBUS_SPINE_KEEPALIVE_MS);
            idx = -1;
          }
   // v180: NEUE IP DER GEGENSTELLE. Verbunden wird zur IP aus der Fundliste des letzten Scans; einen
   // neuen Scan stiess bisher niemand an. Wechselt die Gegenstelle die Adresse (DHCP), versuchte die Box
   // es endlos an der alten (29.09.: ueber 2 min, erst Verbinden von Hand half). Jetzt: steht die SKI
   // nicht mehr in der Liste oder sind EEBUS_KA_SCAN_AFTER Versuche gescheitert, erst neu scannen und
   // das Ergebnis abwarten — danach findet der SKI-Abgleich oben die neue Adresse von selbst.
   // v229: Suche nur noch NEBENHER (hoechstens alle EEBUS_KA_SCAN_MIN_MS), nicht mehr darauf warten. Liefert sie die
   // Gegenstelle nicht, gilt die zuletzt benutzte Adresse weiter (EebusKaPeerZurueck). Vor jedem Versuch ein Ping
   // (SHIP Kap. 8.2): verbunden wird, sobald er antwortet, sonst spaetestens alle EEBUS_KA_TRY_MS (Ping gesperrt).
          else {
            if (((idx < 0) || (ESp->ka_try >= EEBUS_KA_SCAN_AFTER)) &&
                (!ESp->ka_last_scan || ((millis() - ESp->ka_last_scan) >= EEBUS_KA_SCAN_MIN_MS))) {
              if (EebusStartScan()) {
                ESp->ka_last_scan = millis(); if (!ESp->ka_last_scan) { ESp->ka_last_scan = 1; }
                AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau @ %s: %s - suche neu (Scan %u), Nachsehen laeuft weiter"),
                       ESp->peer_ip, (idx < 0) ? "Gegenstelle nicht mehr in der Fundliste" : "mehrere Versuche gescheitert",
                       ESp->ka_scans + 1);
                ESp->ka_try = 0;
                if (ESp->ka_scans < 255) { ESp->ka_scans++; }
              }
            }
            if ((idx < 0) && !Eebus.scan) { idx = EebusKaPeerZurueck(); }
            if (idx < 0) {   // keine Adresse bekannt (oder Suche schreibt gerade die Liste): kurz warten
              ESp->reconnect_at = Eebus126At((Eebus.scan ? (EEBUS_SCAN_TIMEOUT_MS + 500) : EebusKaWaitMs()));
            } else {
              EebusPeer *pp = &Eebus.peers[idx];
              const char *dst = pp->ip[0] ? pp->ip : pp->ip6;
              if (!ESp->ka_last_try) { ESp->ka_last_try = millis(); if (!ESp->ka_last_try) { ESp->ka_last_try = 1; } }
              const bool ping_ok = (2 == ESp->ka_ping_res);
              if (ping_ok) { ESp->ka_ping_seen = true; }   // v230
              const bool faellig = ((millis() - ESp->ka_last_try) >= (ESp->ka_ping_seen ? EEBUS_KA_TRY_PING_MS : EEBUS_KA_TRY_MS));
              const bool ping_laeuft = !EebusKaPingAufraeumen();   // v230: fertige Sitzung hier loeschen
              if (ping_laeuft) {   // Ping laeuft noch -> gleich wieder nachsehen
                ESp->reconnect_at = Eebus126At(300);
              } else if (ping_ok || faellig) {
                ESp->ka_ping_res = 0;
                ESp->ka_try++;   // v180
                AddLog(LOG_LEVEL_INFO, PSTR("EBG: Auto-Reconnect zu Peer %d (Slot %d) - %s"), idx, ci,
                       ping_ok ? "Ping antwortet" : "regelmaessiger Versuch ohne Ping-Antwort");
                if (!EebusShipConnect(idx) && ESp->keepalive) {
                  ESp->reconnect_at = Eebus126At(EebusKaWaitMs());   // fehlgeschlagen -> spaeter erneut
                }
              } else if (3 == ESp->ka_ping_res) {   // keine Antwort: naechster Ping nach der Wartezeit (5 s bzw. 30 s)
                ESp->ka_ping_res = 0;
                const uint32_t w = EebusKaWaitMs();
                ESp->reconnect_at = Eebus126At(((w > 1200) ? (w - 1200) : w));
              } else {
                if (EebusKaPingStart(dst)) {
                  ESp->reconnect_at = Eebus126At(1200);                // Antwort abholen (Timeout 1 s)
                } else {
                  ESp->reconnect_at = Eebus126At(EebusKaWaitMs());     // Ping ging nicht los: naechstes Nachsehen
                }
              }
            }
          }
        }
      }
      break;
    case FUNC_NETWORK_UP:     // v244: kommt jede Sekunde, solange das Netz steht - nur der Wechsel zaehlt
      if (!eebus_netz_da) { eebus_netz_da = true; EebusNetzZurueck(); }
      break;
    case FUNC_NETWORK_DOWN:
      eebus_netz_da = false;
      break;
    case FUNC_COMMAND:
      // Gegenpruefung 06.11.: bei EEBus-Befehlen Leerzeichen am Ende abschneiden (Tasmota entfernt nur fuehrende). Die strenge
      // Zahlenpruefung wies sonst z. B. "EEBusCsSim 500 " ab, weil hinter dem letzten Leerzeichen nichts mehr steht.
      if (XdrvMailbox.topic && XdrvMailbox.data && (0 == strncasecmp_P(XdrvMailbox.topic, PSTR(D_PRFX_EEBUS), strlen_P(PSTR(D_PRFX_EEBUS))))) {
        while ((XdrvMailbox.data_len > 0) && (' ' == XdrvMailbox.data[XdrvMailbox.data_len - 1])) { XdrvMailbox.data[--XdrvMailbox.data_len] = '\0'; }
      }
      result = DecodeCommand(kEebusCommands, EebusCommand);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      EebusWebSensor();
      break;
    case FUNC_WEB_ADD_MAIN_BUTTON:
      EebusWebAddMainButton();
      break;
    case FUNC_WEB_ADD_HANDLER:
      Webserver->on("/steuerbox", EebusWebPage);   // eigene Bedienseite
      Webserver->on("/eebus_identity", EebusIdentityPage);
      break;
    case FUNC_WEB_GET_ARG:
      EebusWebGetArg();
      break;
#endif   // USE_WEBSERVER
    case FUNC_ACTIVE:
      result = true;
      break;
  }
  return result;
}

#endif   // USE_EEBUS_GUARD
#endif   // ESP32
