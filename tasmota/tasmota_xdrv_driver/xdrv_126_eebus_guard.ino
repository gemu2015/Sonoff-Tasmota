/*
  xdrv_126_eebus_guard.ino - EEBUS Energy Guard / Steuerbox-Simulator

  Rolle: Energy Guard — tritt als vorgelagerte
  Steuerbox auf und gibt einer Anlage Leistungsgrenzen vor.

  Kann:
    SHIP als Client UND als Server (Gegenstellen bauen die Verbindung teils selbst zu uns auf),
      TLS 1.2 mit eigenem EC-Zertifikat, mDNS (_ship._tcp), bis zu EEBUS_MAX_CONN Gegenstellen
      gleichzeitig — davon EINE eingehend
    SPINE: Detailed Discovery, Bindings, Subscriptions, Heartbeat
    § 14a EnWG - Bezugsbegrenzung    (LPC, limitationOfPowerConsumption)
    § 9 EEG    - Einspeisebegrenzung (LPP, limitationOfPowerProduction)
    Messwerte am Netzanschlusspunkt (MGCP) lesen
    Weboberflaeche unter /steuerbox (liest steuerbox.html aus dem Dateisystem)

  Offen: mehrere eingehende Verbindungen gleichzeitig (SHIP 12.2.2 Doppelverbindungs-Regel).

  ACHTUNG - EINGEHENDE VERBINDUNGEN WERDEN NICHT GEGEN DIE PEER-SKI GEPRUEFT.
  Ausgehend ist die Identitaet in Ordnung: beidseitiges TLS, unser Zertifikat geht bei jedem
  Verbindungsaufbau hinaus, und die Gegenstelle vertraut uns anhand unserer SKI. Eingehend fehlt
  uns die Gegenrichtung — die schlanke TLS-Bibliothek (tls_mini) fordert kein Client-Zertifikat an
  und gibt das Peer-Zertifikat nicht heraus, also kann die SKI der Gegenstelle nicht gelesen werden.
  Als Ersatz traegt ein eingehender Slot die PEER-IP als Pseudo-SKI (s. EebusInboundAccept).
  Folgen, die man kennen muss:
    - wer den SHIP-Port erreicht, wird angenommen; wir wissen nicht, WER dort spricht
    - die Zuordnung je Geraet (EEBusPeerMode) greift bei eingehenden Verbindungen nicht,
      weil sie ueber die echte SKI geht
  Wer das schliessen will, muss in der TLS-Bibliothek ein Client-Zertifikat anfordern und das
  Peer-Zertifikat herausreichen. Das ist bewusst NICHT Teil dieses Treibers: die Bibliothek wird
  von HTTPS und MQTT mitbenutzt, ein Eingriff dort betrifft alle.

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
    EEBusConnectIp <ip[:port]>    Verbindung ohne Scan; die Peer-SKI bleibt dabei unbekannt
    EEBusDisconnect [ski|idx]     ohne Argument alle trennen
    EEBusStatus                   Verbindungs-Slots mit Handshake-Phase und letztem Ergebnis
    EEBusData                     Ist-Zustaende und Messwerte aller Verbindungen als JSON
    EEBusAdvertise 0|1            eigene mDNS-Ankuendigung (laeuft ab Boot automatisch)
    EEBusRole 0..3                0 = nur lesen, 1 = Steuerbox, 2/3 = abweichende Identitaeten
    EEBusHems 0|1                 Betriebsart: 0 = einzelne Verbrauchseinrichtung, 1 = Energie-
                                  manager. Wird beim Verbinden aus dem Geraetetyp abgeleitet;
                                  der Schalter ist nur Rueckfall bei fehlender Typangabe.
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
    EEBusTrust                    Platzhalter, noch ohne Funktion. Gedacht war eine Liste erlaubter
                                  Gegenstellen fuer EINGEHENDE Verbindungen — das setzt voraus, die
                                  SKI der Gegenstelle zu kennen. Die schlanke TLS-Bibliothek gibt
                                  das Peer-Zertifikat nicht heraus (kein Client-Cert-Request), also
                                  fuehrt ein eingehender Slot nur die Peer-IP als Pseudo-SKI. Ohne
                                  echte SKI ist eine Trust-Liste nicht umsetzbar; der Schalter
                                  bleibt deshalb wirkungslos, statt eine Pruefung vorzutaeuschen,
                                  die nicht stattfindet. Naeheres im Kopfkommentar.

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
  "LppFrei|Lpp|Anmeld|Data|Mess|Struct|Read|AutoConn";   // ReleaseAll VOR Release, LppFrei VOR Lpp (Praefix-Match!)

void (* const EebusCommand[])(void) PROGMEM = {
  &CmndEebusScan, &CmndEebusPeers, &CmndEebusCert,
  &CmndEebusConnect, &CmndEebusDisconnect, &CmndEebusStatus, &CmndEebusTrust,
  &CmndEebusConnectIp, &CmndEebusAdvertise, &CmndEebusLog,
  &CmndEebusRole, &CmndEebusLimit, &CmndEebusReleaseAll, &CmndEebusRelease,
  &CmndEebusTarget, &CmndEebusProbe, &CmndEebusProvide, &CmndEebusOpen, &CmndEebusHems,
  &CmndEebusPeerMode, &CmndEebusDelDur,
  &CmndEebusLppFrei, &CmndEebusLpp, &CmndEebusAnmeld, &CmndEebusData,
  &CmndEebusMess, &CmndEebusStruct, &CmndEebusRead, &CmndEebusAutoConn };

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
uint8_t eebus_hems_mode = 1;

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
  const char *p = strchr(s, 'T');
  if (nullptr == p) { return -1; }
  p++;
  long total = 0, num = 0; bool any = false;
  while (*p) {
    if ((*p >= '0') && (*p <= '9')) { num = num * 10 + (*p - '0'); any = true; }
    else if ('H' == *p) { total += num * 3600; num = 0; }
    else if ('M' == *p) { total += num * 60;   num = 0; }
    else if ('S' == *p) { total += num;        num = 0; }
    else { return -1; }   // unbekanntes Zeichen -> lieber nichts behaupten
    p++;
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
#define EEBUS_LPC_PHASE_TIMEOUT_MS 8000UL
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
#define EEBUS_LPC_WRITE_MAX_TRIES  3   // so oft HB+Write nach err7 wiederholen (Norm LPC-906/914)
#define EEBUS_LPC_WRITE_RETRY_MS   2000UL   // Pause zwischen den HB+Write-Retries (CS "init" absetzen lassen)
// §14a verlangt zwingend einen Failsafe (Rueckfall-Limit + Mindestdauer, falls die Steuerbox-
// Verbindung abreisst). Die Vergleichs-Steuerbox SCHREIBT vor dem Consumption-Limit die Failsafe-DeviceConfig
// (WriteFailsafeConsumptionActivePowerLimit + WriteFailsafeDurationMinimum); wir lasen sie bisher nur.
// Werte wie die Vergleichs-Steuerbox (Mitschnitt): 1.000.000 W Rueckfall-Limit, Mindestdauer 2 h.
#define EEBUS_LPC_FS_WATT          1000000UL // Failsafe-Verbrauchslimit in W
#define EEBUS_LPC_FS_DUR           "PT2H"   // FailsafeDurationMinimum = 7200 s (2 h)

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
#define EEBUS_AC_DELAY_DEFAULT_S   120   // Wartezeit nach Netz+Uhr, bevor gesucht wird
#define EEBUS_AC_SCAN_WAIT_MS     6000   // Zeit, die der mDNS-Scan zum Fuellen der Liste bekommt
#define EEBUS_AC_MAX_TRY             4   // danach aufgeben — ein Mensch muss ran

   // Abstaende zwischen den Versuchen: wachsend, NICHT die starren 8 s des Keep-Alive.
   // Zu dichte Wiederholungen sind genau der Weg in "cmi resp n=-1".
const uint16_t kEebusAcRetryS[EEBUS_AC_MAX_TRY] PROGMEM = { 30, 60, 120, 300 };

enum EebusAcState : uint8_t { AC_IDLE, AC_WAIT, AC_SCAN, AC_DONE };

uint16_t eebus_ac_delay_s = EEBUS_AC_DELAY_DEFAULT_S;   // 0 = Wiederaufbau abgeschaltet
char     eebus_ac_ski[41] = { 0 };   // gemerkte Peer-SKI ("" = keine)
uint8_t  eebus_ac_state   = AC_IDLE;
uint32_t eebus_ac_next    = 0;       // millis() fuer den naechsten Schritt
uint8_t  eebus_ac_try     = 0;       // Zaehler der Verbindungsversuche

// Peer-SKI merken. Abgelegt wie Zertifikat und Schluessel im Flash-FS, damit sie einen
// Neustart ueberlebt. Nur ECHTE SKIs (40 Hex-Zeichen) — der Platzhalter "manual" aus
// EEBusConnectIp identifiziert kein Geraet und taugt zum Wiederfinden nicht.
void EebusPeerRemember(const char *ski) {
  if ((nullptr == ski) || (40 != strlen(ski))) { return; }
  if (0 == strcmp(eebus_ac_ski, ski)) { return; }   // steht bereits so in der Datei
  strlcpy(eebus_ac_ski, ski, sizeof(eebus_ac_ski));
#ifdef USE_UFILESYS
  if (TfsSaveFile(EEBUS_PEER_FILE, (const uint8_t*)ski, 40)) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer %s gemerkt - nach einem Neustart wird selbsttaetig verbunden"), ski);
  }
#endif
}

// Bewusstes Trennen soll NICHT beim naechsten Start rueckgaengig gemacht werden.
void EebusPeerForget(void) {
  eebus_ac_ski[0] = '\0';
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
  if (!TfsFileExists(EEBUS_PEER_FILE)) { return false; }
  File f = ffsp->open(EEBUS_PEER_FILE, "r");
  if (!f) { return false; }
  int n = f.read((uint8_t*)eebus_ac_ski, 40);
  f.close();
  if (n < 0) { n = 0; }
  eebus_ac_ski[(n > 40) ? 40 : n] = '\0';
  return (40 == strlen(eebus_ac_ski));
#else
  return false;
#endif
}

// SKI -> Betriebsmodus. Index des Eintrags einer SKI, -1 wenn nicht zugeordnet.
int EebusModeFind(const char *ski) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return -1; }
  for (uint32_t i = 0; i < Eebus.mode_count; i++) {
    if (0 == strcmp(Eebus.mode_ski[i], ski)) { return (int)i; }
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
int EebusModeEffective(const char *ski) {
  int m = EebusModeForSki(ski);
  if (m >= 0) { return m; }
  return EebusModeFromType(ski);
}

// Setzt den Session-Modus eebus_hems_mode fuer diese Gegenstelle. Greift weder Zuordnung noch
// Typ, bleibt der globale EEBusHems-Default stehen. So gelten pro Geraet die richtigen Regeln.
void EebusModeApply(const char *ski) {
  int m = EebusModeForSki(ski);
  const char *quelle = "Zuordnung";
  if (m < 0) { m = EebusModeFromType(ski); quelle = "Geraetetyp"; }
  if (m >= 0) {
    if (eebus_hems_mode != (uint8_t)m) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Betriebsart %s (aus %s)"), m ? "HEMS" : "SteuVE", quelle);
    }
    eebus_hems_mode = (uint8_t)m;
  }
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
      for (size_t i = 0; i < cur->txt_count; i++) {
        const char *k = cur->txt[i].key;
        const char *v = cur->txt[i].value;
        if ((nullptr == k) || (nullptr == v)) { continue; }
        if (0 == strcmp(k, "ski"))        { strlcpy(p->ski, v, sizeof(p->ski)); }
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
      for (mdns_ip_addr_t *a = cur->addr; a; a = a->next) {   // erste IPv4 reicht
        if (ESP_IPADDR_TYPE_V4 == a->addr.type) {
          strlcpy(p->ip, IPAddress(a->addr.u_addr.ip4.addr).toString().c_str(), sizeof(p->ip));
          break;
        }
      }
   // SKI ist die Identitaet — ohne SKI ist der Eintrag fuer uns wertlos (SHIP 7.3.2 Pflichtfeld)
      if ('\0' == p->ski[0]) { continue; }
   // Dedup nach SKI. mDNS liefert denselben Dienst mitunter MEHRFACH (mehrere Interfaces/
   // Adressen — z.B. der Referenz-CS an 169.254 doppelt). Jedes Geraet soll genau einmal in der
   // Liste stehen (SKI = Identitaet), sonst ist die Geraeteauswahl uneindeutig.
      bool dup = false;
      for (uint32_t d = 0; d < Eebus.peer_count; d++) {
        if (0 == strcmp(Eebus.peers[d].ski, p->ski)) { dup = true; break; }
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
                          "\"Ip\":\"%s\",\"Host\":\"%s\",\"Port\":%d,\"Path\":\"%s\","
                          "\"Mode\":\"%s\",\"ModeSrc\":\"%s\"}"),
                     (i) ? "," : "", p->ski, p->id, p->model, p->brand,
                     p->type, (p->cat[0]) ? p->cat : "", (p->reg[0]) ? p->reg : "",
                     p->ip, p->host, p->port, p->path,
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
  SME_FAIL
};

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
#define EEBUS_SPINE_RXBUF            32768   // PSRAM-Empfangspuffer fuer SPINE-Datagramme. 8192 war ZU KLEIN: Energiemanager-DetailedDiscovery = 9713 B, Waermepumpen-Gateway = 10969 B -> WsRecv (Z.913) verwarf den Frame STILL ("WS-Nachricht > 8191 B, verworfen", nur DEBUG) -> wir bekamen die Peer-Discovery NIE. Live bewiesen . 32 KB deckt reale Discovery + Reserve (PSRAM, pro Slot).
#define EEBUS_SPINE_VERSION          "1.3.0"   // SUPPORTED_SPINE_VERSION (wie Ladestation/Energiemanager/Waermepumpen-Gateway)
#define EEBUS_SPINE_KEEPALIVE_MS     8000UL   // Auto-Reconnect, wenn nach Done die Leitung abfaellt
   // Hat die Gegenstelle "pending" gemeldet, wartet dort ein MENSCH auf eine Freigabe
   // (Portal/Hersteller-App). Dichtes Wiederholen bringt dann nichts, erzeugt aber
   // "cmi resp n=-1" — und DAS legt den Slot bis zum naechsten Neustart lahm.
#define EEBUS_PENDING_RETRY_MS     300000UL   // 5 min statt 8 s, solange der Peer "pending" meldet

// MULTI-CONNECTION: bis zu 3 SHIP-Verbindungen GLEICHZEITIG (Ladestation + Waermepumpen-Gateway + Energiemanager).
// Muster: ein Verbindungs-Slot pro Peer; ALLE bestehenden Funktionen arbeiten unveraendert
// ueber den globalen "aktuellen Slot"-Zeiger ESp (frueher: eine globale EShip-Struktur).
// Die Schleifen (Poll/Heartbeat/Keep-Alive/Teardown) setzen ESp pro Slot — alles laeuft
// im selben Arduino-Loop-Task, daher ist der Zeiger race-frei.
#define EEBUS_MAX_CONN 3

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
  char     peer_ip[16] = { 0 };
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
  uint32_t ws_ping_next = 0;   // millis() fuer den naechsten WebSocket-Ping (0 = noch nicht gesetzt)
   // mitgelesene Nutzdaten fuer die Anzeige. Alle Werte als Zahl+Zehnerexponent wie auf dem Draht
   // ("scaledNumber"), damit nichts gerundet wird; die Oberflaeche rechnet zusammen.
   // Zuordnung Kennung->Bedeutung kommt aus measurementDescriptionListData (scopeType), NICHT geraten.
  int8_t   m_scope[24] = { 0 };   // je measurementId: 1=acPowerTotal 2=gridFeedIn 3=gridConsumption
   //                   4=acCurrent 5=acVoltage 6=frequency 0=unbekannt
  long     m_val[7] = { 0 };   // letzter Wert je Bedeutung (Index = Code oben)
  int8_t   m_sc[7]  = { 0 };   // zugehoeriger Zehnerexponent
  bool     m_have[7] = { false };   // schon einmal empfangen?
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
  int      cfg_ent[2] = { -1, -1 }; // bis zu zwei DeviceConfig-Entities
  int8_t   cfg_code[2][6] = { { 0 } };   // je keyId: 1=failsafeConsW 2=failsafeDur 3=failsafeProdW 4=pvCurtailPct
  long     fs_cons = 0;  int8_t fs_cons_sc = 0;  bool fs_cons_ok = false;
  long     fs_prod = 0;  int8_t fs_prod_sc = 0;  bool fs_prod_ok = false;
  char     fs_dur[16] = { 0 };   // Mindestdauer als Norm-Text, z.B. "PT2H"
  long     plf = 0;      int8_t plf_sc = 0;      bool plf_ok = false;   // Begrenzungsfaktor in Prozent
   // Nennleistungen aus electricalConnectionCharacteristicListData. DAS ist die norm-richtige
   // Bezugsgroesse fuer eine Prozent-Vorgabe des Netzbetreibers (LPP-Szenario 4, Tab. 18/19) — bisher
   // haben wir dafuer den Failsafe-Wert Erzeugung genommen, was plausibel, aber ungeprueft war.
  long     pnom_prod = 0;  int8_t pnom_prod_sc = 0;  bool pnom_prod_ok = false;   // powerProductionNominalMax
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
  uint32_t hb_counter = 1;   // DeviceDiagnosis-Heartbeat-Zaehler
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
   // Geltungsdauer DIESES Schreibauftrags in Sekunden (0 = ohne timePeriod).
   // Bewusst je Auftrag statt global: so bleibt der bewiesene §14a-Frame zeichengleich, solange
   // niemand eine Dauer angibt — und wer eine angibt, bekommt sie auch im HEMS-Betrieb.
  long     lpc_dur_s = 0;
  int      lpc_limit_id = -1;   // limitId aus der LoadControl-Beschreibung des Peers (richtungsabhaengig)
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
  uint8_t  lpc_fs_step = 0;   // HEMS: Failsafe-Write-Fortschritt (0=nichts, 1=Value gesendet, 2=Dauer gesendet)
  bool     lpc_fs_done = false;   // HEMS: Failsafe auf dieser Verbindung schon geschrieben -> nicht erneut
  uint8_t  lpc_write_tries = 0;   // HEMS: Zaehler der err7-Retries (Norm LPC-906/914)
  uint32_t lpc_write_retry_at = 0;   // HEMS: millis() fuer den naechsten HB+Write-Retry (0=aus)
  bool     lpc_our_limit = false;   // haben WIR ein Limit an diesem Peer aktiv gesetzt? (Freigabe-Pflicht)
  char     lpc_result[56] = { 0 };   // letztes Ergebnis (Web/Status)
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
} EebusConn;

EebusConn EConn[EEBUS_MAX_CONN];   // die 3 Verbindungs-Slots
EebusConn *ESp = &EConn[0];   // "aktueller Slot" — alle Bestandsfunktionen arbeiten hierueber

// Slot mit bestehender/zuletzt genutzter Verbindung zu dieser SKI (-1 = keiner).
int EebusConnBySki(const char *ski) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return -1; }
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    if (0 == strcmp(EConn[i].peer_ski, ski) &&
        (EConn[i].active || EConn[i].client || EConn[i].keepalive || (SME_OFF != EConn[i].sme))) {
      return i;
    }
  }
  return -1;
}

// Slot fuer eine (neue) Verbindung zu dieser SKI besorgen: erst Wiederverwendung
// (gleicher Peer), sonst ein freier Slot. -1 = alle Slots belegt.
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
  if (ESp->client) { ESp->client->stop(); delete ESp->client; ESp->client = nullptr; }
  EebusHeapCheck("shipfree_client");   // nach delete des BearSSL-Clients
  if (ESp->cert_der) { free(ESp->cert_der); ESp->cert_der = nullptr; }
  EebusHeapCheck("shipfree_cert");   // nach free(cert_der) (= Crash-Zeile 607!)
  if (ESp->rxbuf) { free(ESp->rxbuf); ESp->rxbuf = nullptr; ESp->rxbuf_size = 0; }
  EebusHeapCheck("shipfree_rxbuf");   // nach free(rxbuf, PSRAM)
  ESp->cert_len = 0;
  ESp->active = false;
  ESp->teardown = false;
}

// KRITISCH gegen Heap-Crash: den TLS-Client NIE aus dem Empfangs-Callback heraus loeschen
// (lwip/W5500 haelt den Socket dann evtl. noch in Bearbeitung -> free auf benutzten Speicher).
// Stattdessen nur markieren; das eigentliche EebusShipFree() laeuft am naechsten Poll-Tick
// bzw. Sekunden-Tick in sauberem Kontext (EebusTeardownNow).
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

// Einen WS-Client-Frame senden (FIN, maskiert — RFC 6455). Beliebige Laenge (16-/64-bit),
// die Nutzlast wird in kleinen maskierten Bloecken gesendet -> kein grosser Puffer noetig.
// opcode: 0x2=binary (SHIP/SPINE), 0xA=Pong.
bool EebusWsSendOp(const uint8_t *payload, size_t len, uint8_t opcode) {
  if (ESp->via_srv) {   // Slot einer EINGEHENDEN Verbindung -> Server-Sendepfad
    return EebusSrvWsSendOp(payload, len, opcode);   // (unmaskiert, ueber die BearSSL-Server-Engine)
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
  if (hl != ESp->client->write(hdr, hl)) { return false; }
  if (len > 200) { EebusHeapCheckN("wsop_hdr", (uint32_t)len); }   // nur grosse Sendungen pruefen
  uint8_t chunk[64];   // Nutzlast blockweise maskieren + senden (klein = stack-schonend)
  for (size_t off = 0; off < len; ) {
    size_t n = (len - off > sizeof(chunk)) ? sizeof(chunk) : (len - off);
    for (size_t i = 0; i < n; i++) { chunk[i] = payload[off + i] ^ mask[(off + i) & 3]; }
    if (n != ESp->client->write(chunk, n)) { return false; }
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
    if (!drop && (total + len > max)) {   // Nachricht passt nicht in den Puffer
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

// Letztes Verbindungsergebnis eines Peers merken (Slot per SKI suchen/anlegen).
// (Signaturen nur einfache Typen — .ino-Prototypen-Falle.)
void EebusStatSet(const char *ski, int state, const char *err) {
  if ((nullptr == ski) || ('\0' == ski[0])) { return; }
  int slot = -1;
  for (uint32_t i = 0; i < Eebus.stat_count; i++) {
    if (0 == strcmp(Eebus.stat_ski[i], ski)) { slot = i; break; }
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
  ESp->lc_sub = false; ESp->lc_cli_ent = 0; ESp->lc_cli_feat = 0;   // 
  ESp->hb_sub = false;
  ESp->hb_cli_ent = 0;
  ESp->hb_cli_feat = 0;
  ESp->hb_next = 0;
   // Herzschlag der Gegenstelle: frische Sitzung -> noch keiner empfangen, keine Zusage bekannt.
   // Zaehler und Zeitpunkt der Vorfaelle bleiben ABSICHTLICH stehen — sie sind die Vorfallsliste.
  ESp->peer_hb_at = 0; ESp->peer_hb_ctr = 0; ESp->peer_hb_tmo_s = 0; ESp->peer_hb_lost = false;
  ESp->spine_ctr = 1;
   // LPC-Sequenz ruecksetzen (frische TLS-Session -> Binding weg, limitId neu holen)
  ESp->lpc_state = LPC_IDLE;
  ESp->lpc_bound = false;
  ESp->lpc_onboard_only = false;   // 
  ESp->lpc_onboarded = false;   // 
  ESp->lpc_limit_id = -1;
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
  strlcpy(ESp->peer_ip, p->ip, sizeof(ESp->peer_ip));
  strlcpy(ESp->peer_ski, p->ski, sizeof(ESp->peer_ski));   // Peer-Identitaet aus mDNS
  EebusModeApply(ESp->peer_ski);   // Betriebsmodus (HEMS/SteuVE) aus SKI-Zuordnung
  ESp->peer_port = p->port;

   // Grosser SPINE-Empfangspuffer in PSRAM (Detailed-Discovery-Datagramme sind mehrere KB)
  ESp->rxbuf = (uint8_t*)special_malloc(EEBUS_SPINE_RXBUF);
  if (nullptr == ESp->rxbuf) { strlcpy(ESp->err, "rxbuf", sizeof(ESp->err)); goto fail; }
  ESp->rxbuf_size = EEBUS_SPINE_RXBUF;

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

   // Puffergroesse NICHT aendern: 2048 fuehrt dazu, dass BearSSL max_fragment_length=1024 anfordert
   // (2^11 passt mit Overhead nicht in 2048, also 2^10). SHIP 9.2 verlangt genau MFLN=1024 — mit
   // 4096er-Puffern fordert BearSSL 2048 an, und mbedtls-Gegenstellen lehnen das ClientHello dann
   // mit handshake_failure (err=296) ab.
  ESp->client = new BearSSL::WiFiClientSecure_light(2048, 2048);
  if (nullptr == ESp->client) { strlcpy(ESp->err, "client new", sizeof(ESp->err)); goto fail; }
  ESp->active = true;
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
    IPAddress peer_addr;
    if (!peer_addr.fromString(p->ip) || !ESp->client->connect(peer_addr, p->port)) {
      snprintf(ESp->err, sizeof(ESp->err), "tls connect err=%d", (int)ESp->client->getLastError());
      goto fail;
    }
  }
  ESp->state = SHIP_TLS_OK;
  EebusHeapCheck("tls_handshake");   // Checkpunkt: nach BearSSL-TLS-Handshake (inkl. ECDSA-Signieren)

   // WebSocket-Upgrade (Subprotocol "ship")
  if (!EebusWsUpgrade(p->ip, p->port, p->path)) { goto fail; }   // err bereits gesetzt
  ESp->state = SHIP_WS_OK;
  EebusHeapCheck("ws_upgrade");   // Checkpunkt: nach HTTP-Upgrade

   // CMI-Handshake (SHIP 13.4.3): [0x00,0x00] senden, [0x00,0x00] erwarten
  {
    uint8_t cmi[2] = { 0x00, 0x00 };
    if (!EebusWsSend(cmi, 2)) { strlcpy(ESp->err, "cmi send", sizeof(ESp->err)); goto fail; }
    uint8_t rx[8];
    int n = EebusWsRecv(rx, sizeof(rx), 8000);   // Geraet antwortet sofort; 8 s reicht (WDT-schonend)
    if (n != 2 || rx[0] != 0x00 || rx[1] != 0x00) {
      snprintf(ESp->err, sizeof(ESp->err), "cmi resp n=%d", n); goto fail; }
  }
  ESp->state = SHIP_CMI_OK;
  EebusHeapCheck("cmi");   // Checkpunkt: nach CMI-Austausch
  EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP verbunden %s:%u, CMI ok, Peer-SKI %s"),
         ESp->peer_ip, ESp->peer_port, ESp->peer_ski);
  EebusSmeStart();   // M2b: SME-Handshake asynchron weiterfuehren
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
      if (millis() - start > 8000) { strlcpy(ESp->err, "ws resp timeout", sizeof(ESp->err)); return false; }
      delay(5);
    }
  }
  resp[ri] = '\0';
  if (nullptr == strstr(resp, " 101")) {
    strlcpy(ESp->err, "ws no 101", sizeof(ESp->err)); return false; }
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

void EebusShipLog(char dir, int classifier, const char *json) {
  size_t jl = strlen(json);
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
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP %cX c=%d [%uB] 1/%d %s"), dir, classifier, (uint32_t)jl, parts, piece);
      } else if ((p == parts - 1) && (jl > 1000)) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP %d/%d %s..."), p + 1, parts, piece);
      } else {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP %d/%d %s"), p + 1, parts, piece);
      }
    }
  } else {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP %cX c=%d [%uB] %s"), dir, classifier, (uint32_t)jl, json);
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
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char *p = strstr(json, pat);
  if (nullptr == p) { return false; }
  p += strlen(pat);
  const char *e = strchr(p, '"');
  if (nullptr == e) { return false; }
  size_t l = e - p;
  if (l >= outlen) { l = outlen - 1; }
  memcpy(out, p, l);
  out[l] = '\0';
  return true;
}

// Naiver Integer-Extraktor "key":<zahl> (keine Anfuehrungszeichen). true wenn gefunden.
bool EebusJsonInt(const char *json, const char *key, uint32_t *out) {
  char pat[48];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char *p = strstr(json, pat);
  if (nullptr == p) { return false; }
  p += strlen(pat);
  while ((*p == ' ') || (*p == '\t')) { p++; }
  if ((*p < '0') || (*p > '9')) { return false; }
  *out = (uint32_t)strtoul(p, nullptr, 10);
  return true;
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
    default:        return "off";
  }
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

// Nach CMI-ok den SME-Handshake starten: wir sind sofort "ready" (Tool vertraut jedem).
void EebusSmeStart(void) {
  ESp->peer_id[0] = '\0';
  ESp->sme_pending_logged = false;
   // SHIP 13.4.4.1.3: Update-Message immer mit maximaler Wartezeit melden
  if (!EebusShipSendJson("{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":60000}]}", 1)) {
    EebusSmeFail("hello send"); return;
  }
  ESp->sme = SME_HELLO;
  ESp->sme_hello_start = millis();
  ESp->sme_deadline = millis() + EEBUS_SME_HELLO_TIMEOUT_MS;
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

  EebusHeapCheckN("spine_format", (uint32_t)strlen(buf));   // nach snprintf ins Datagramm
  EebusShipSendJson(buf, 2);
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
        "{\"description\":\"Steuerbox Simulator\"}"
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
        ESp->lpc_bound = false; ESp->lpc_limit_id = -1;   // Ziel geaendert -> Binding/limitId neu holen
        ESp->lpc_onboarded = false;   // neues Ziel -> erneut onboarden
        ESp->lpc_fs_val_key = -1; ESp->lpc_fs_dur_key = -1; ESp->lpc_fs_done = false; ESp->lpc_fs_step = 0;   // neues DeviceConfig-Feat -> Failsafe-Keys neu lernen+schreiben
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

// die richtige limitId aus der LoadControl-Beschreibung waehlen — die der VERBRAUCHS-
// Obergrenze (limitDirection "consume" bzw. limitType "maxValueLimit") statt blind der ersten.
// Fallback = erste limitId (bisheriges Verhalten) -> kein Regress, wenn kein passender Block da ist.
// Die limitId steht am Blockanfang VOR limitType/-Direction -> wir nehmen die letzte limitId, die
// noch VOR der Fundstelle liegt (= die des passenden Blocks). true wenn eine id gesetzt wurde.
// richtungsabhaengig. dir 0 = consume (§14a, bisheriges Verhalten unveraendert),
// dir 1 = produce (§9 EEG). Der Peer beschreibt beide Grenzen im selben Block, z.B.
//   [{limitId 0},{limitType signDependentAbsValueLimit},{limitCategory obligation},{limitDirection consume},...]
//   [{limitId 1},{limitType signDependentAbsValueLimit},{limitCategory obligation},{limitDirection produce},...]
// Fuer produce gibt es KEINEN maxValueLimit-Ersatzweg (das waere eine Bezugs-Obergrenze) und auch keinen
// Fallback auf die erste limitId — ohne ausgewiesene produce-Grenze wird nicht geschrieben, sonst
// landete ein Einspeise-Limit versehentlich auf der Bezugs-Grenze.
bool EebusLpcPickLimitId(const char *desc, uint8_t dir, int *out_id) {
  const char *best = strstr(desc, (1 == dir) ? "\"limitDirection\":\"produce\"" : "\"limitDirection\":\"consume\"");
  if ((nullptr == best) && (1 == dir)) { return false; }   // produce nur bei ausgewiesener Grenze
  if (nullptr == best) { best = strstr(desc, "\"limitType\":\"maxValueLimit\""); }
  if (best) {
    const char *p = desc, *last = nullptr;
    while (true) {
      const char *li = strstr(p, "\"limitId\":");
      if ((nullptr == li) || (li > best)) { break; }
      last = li; p = li + 10;
    }
    uint32_t id = 0;
    if (last && EebusJsonInt(last, "limitId", &id)) { *out_id = (int)id; return true; }
  }
  if (1 == dir) { return false; }   // fuer produce KEIN Blind-Fallback
  uint32_t id = 0;   // Fallback: erste limitId
  if (EebusJsonInt(desc, "limitId", &id)) { *out_id = (int)id; return true; }
  return false;
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
  const char *hb_to = eebus_hems_mode ? "PT60S" : "PT30S";
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
  snprintf(hb, cap,
    "{\"deviceDiagnosisHeartbeatData\":[{\"timestamp\":\"%sZ\"},{\"heartbeatCounter\":%u},{\"heartbeatTimeout\":\"%s\"}]}",
    GetDT(UtcTime()).c_str(), ESp->hb_counter, hb_to);
   // Quell-Feature unseres DeviceDiagnosis-Servers ROLLENABHAENGIG — Role 1 (Branch A) deklariert
   // ihn auf feat5 (1:1 Vergleichs-Steuerbox; feat2 ist jetzt der LoadControl-Client), Rolle 0/2/3 (Branch B) auf feat2.
   // Bisher wurde IMMER von feat2 gesendet -> in Role 1 stimmte die HB-Quelle NICHT mit der deklarierten
   // feat5 ueberein (der Energiemanager abonniert feat5, bekam den Notify aber von feat2 -> evtl. HB nicht zugeordnet).
  int hb_src_feat = (1 == eebus_role) ? 5 : 2;
  EebusSpineSendAddr(cls, has_ref, ref, hb, 1, hb_src_feat, dst_ent, dst_feat);   // von unserem DeviceDiagnosis-Server
  free(hb);
   // heartbeatCounter bei jedem NOTIFY hochzaehlen, bei read-Reply NICHT. SteuVE-Modus
   // unveraendert (zaehlt wie bisher bei jedem Senden hoch -> byte-identisch ).
  if (!eebus_hems_mode || (0 == strcmp(cls, "notify"))) { ESp->hb_counter++; }
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
    if (!cc->lim_dur[d][0] || (cc->lim_dur_s[d] < 0)) { continue; }           // ohne Dauer kein Ablauf
    long weg = (long)((millis() - cc->lim_dur_at[d]) / 1000UL);
    if ((cc->lim_dur_s[d] - weg) > 0) { continue; }                           // laeuft noch
    ESp = cc;
    EebusLpcSetResult(LPC_DONE, (1 == cc->lpc_dir) ? "ausgelaufen (Einspeisung wieder frei)"
                                                   : "ausgelaufen (Bezug wieder frei)");
  }
  ESp = save;
}

// Server-Limit an ALLE LoadControl-Abonnenten (ueber alle Slots) verteilen.
void EebusServeLimitBroadcast(void) {
  EebusConn *save = ESp;
  for (int i = 0; i < EEBUS_MAX_CONN; i++) {
    ESp = &EConn[i];
    if (ESp->active && ESp->lc_sub && (SME_DONE == ESp->sme)) {
      EebusServeLimitNotify();
    }
  }
  ESp = save;
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
    ESp->hb_cli_ent  = peer_ent;
    ESp->hb_cli_feat = peer_feat;
    ESp->hb_sub      = true;
   // Heartbeat-Takt im HEMS-Modus auf 8 s (wie die Vergleichs-Steuerbox) statt 20 s. Verdacht:
   // der Energiemanager wertet den EG erst bei engem HB (<=10 s) als durchgaengig "aktiv" (verlaesst init/failsafe).
   // Ladestation/Waermepumpen-Gateway-Pfad UNVERAENDERT bei 20 s (< dessen PT30S-Timeout) -> bewiesener Drossel-Pfad unberuehrt.
    ESp->hb_next     = millis() + (eebus_hems_mode ? 8000 : 20000);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer liest unseren Heartbeat (feat %d) -> proaktiv alle %d s senden"),
           peer_feat, eebus_hems_mode ? 8 : 20);
    return true;
  }
   // ===== "DATEN-FUTTER" — Provider-Antworten fuer MGCP/MPC (Werte SIMULIERT: konstante
   // Platzhalter, bis echte Quellen (Shelly .96 / Energiemanager-Umfeld) angebunden sind. Zweck: die
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
   // ===== Ende Datenantworten =====
  if (strstr(json, "loadControlLimitDescriptionListData")) {
    EebusSpineSendAddr("reply", true, ref,
      "{\"loadControlLimitDescriptionListData\":[{\"loadControlLimitDescriptionData\":["
      "[{\"limitId\":1},{\"limitType\":\"maxValueLimit\"},{\"limitCategory\":\"obligation\"},"
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

void EebusLpcSetResult(uint8_t state, const char *txt) {
  ESp->lpc_state = state;
  strlcpy(ESp->lpc_result, txt, sizeof(ESp->lpc_result));
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
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC @ %s: Fallback LoadControl abo+bind (keine Discovery)"), ESp->peer_ip);
  }
  free(cmd);
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
  EebusLpcSetResult(LPC_BIND, "Binding gesendet");
}

// LoadControl-Limit-Beschreibung des Peers lesen (limitId der consume/obligation-Grenze finden).
void EebusLpcReadDesc(void) {
  EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitDescriptionListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
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
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
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
  if (vk >= 0) { ESp->lpc_fs_val_key = vk; }
  if (dk >= 0) { ESp->lpc_fs_dur_key = dk; }
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Keys gelernt: Value=keyId%d, Dauer=keyId%d @ %s"),
         ESp->lpc_fs_val_key, ESp->lpc_fs_dur_key, ESp->peer_ip);
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
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
  EebusLpcSetResult(LPC_READCFG, "DeviceConfig-Beschreibung angefragt");
}

// ElectricalConnection des Peers lesen VOR dem Write — wie die Vergleichs-Steuerbox (sie liest
// electricalConnectionDescriptionListData + electricalConnectionParameterDescriptionListData). Damit
// deckt unser Onboarding auch diese Feature-Art ab. Adresse: unser ElectricalConnection-Client
// ent1/feat4 -> Peer-LC-Entity/feat7 (Energiemanager: ent7/feat7). Best-effort: Timeout -> trotzdem schreiben.
void EebusLpcReadEl(void) {
  EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionDescriptionListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, 4, ESp->lpc_peer_ent, 7);
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
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

// §14a-Failsafe VOR dem Consumption-Limit setzen — Schritt 1: den Failsafe-Value schreiben.
// Die Dauer folgt, sobald die Quittung kommt (EebusLpcFailsafeResult -> EebusLpcWriteFailsafeDur).
void EebusLpcWriteFailsafe(void) {
  char v[72]; snprintf(v, sizeof(v),
    "{\"scaledNumber\":[{\"number\":%lu},{\"scale\":0}]}", (unsigned long)EEBUS_LPC_FS_WATT);
  ESp->lpc_fs_step = 1;
  EebusLpcSendFailsafe(ESp->lpc_fs_val_key, v);
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
  EebusLpcSetResult(LPC_FAILSAFE, "Failsafe-Value geschrieben");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Write %lu W (keyId %d) an ent%d/feat%d @ %s"),
         (unsigned long)EEBUS_LPC_FS_WATT, ESp->lpc_fs_val_key, ESp->lpc_peer_ent, ESp->lpc_peer_dcfg_feat, ESp->peer_ip);
}

// Schritt 2 — die FailsafeDurationMinimum schreiben (nach der Value-Quittung).
void EebusLpcWriteFailsafeDur(void) {
  ESp->lpc_fs_step = 2;
  EebusLpcSendFailsafe(ESp->lpc_fs_dur_key, "{\"duration\":\"" EEBUS_LPC_FS_DUR "\"}");
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
  EebusLpcSetResult(LPC_FAILSAFE, "Failsafe-Dauer geschrieben");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Dauer " EEBUS_LPC_FS_DUR " (keyId %d) an ent%d/feat%d @ %s"),
         ESp->lpc_fs_dur_key, ESp->lpc_peer_ent, ESp->lpc_peer_dcfg_feat, ESp->peer_ip);
}

// Quittung auf einen Failsafe-Write. Best-effort: Value -> Dauer -> Limit (die Failsafe-Werte sind
// nur Vorbedingung; das eigentliche Ziel ist der Limit-Write, der danach folgt). lpc_fs_done verhindert
// eine Endlosschleife, falls der Energiemanager die Failsafe-Writes ablehnt (der Limit-Write kommt dann trotzdem einmal).
void EebusLpcFailsafeResult(bool ok) {
  if (1 == ESp->lpc_fs_step) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Value %s @ %s"), ok ? "quittiert (err0)" : "abgelehnt", ESp->peer_ip);
    EebusLpcWriteFailsafeDur();   // Dauer folgt unabhaengig vom Ergebnis
  } else {   // step 2 (Dauer)
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Failsafe-Dauer %s @ %s -> Failsafe gesetzt, schreibe Limit"),
           ok ? "quittiert (err0)" : "abgelehnt", ESp->peer_ip);
    ESp->lpc_fs_done = true;
    ESp->lpc_fs_step = 0;
    EebusLpcWrite();   // Gate wird durch lpc_fs_done uebersprungen -> Limit-Write
  }
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
    EebusLpcSetResult(LPC_IDLE, "onboarded - Beziehung reift, Write via EEBusLpc/EEBusLpp");
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC-Onboarding fertig @ %s - Heartbeat laeuft%s"), ESp->peer_ip,
           ESp->lpc_reg_step ? ", Anmeldung folgt sobald der Heartbeat abonniert ist"
                             : ", Write folgt spaeter (EEBusLpc/EEBusLpp)");
    return;
  }
   // FAILSAFE-WRITE DEAKTIVIERT — BEWIESEN ein Phantom. Die Hypothese (der Energiemanager nimmt den Limit-Write
   // nur an, wenn wir vorher die Failsafe-DeviceConfig SCHREIBEN) ist durch direkten Vergleich mit dem
   // Vergleichs-Steuerbox-Erfolgs-Mitschnitt WIDERLEGT: die Vergleichs-Steuerbox sendet in ihrer ganzen err0-Session
   // GENAU EINEN Write (das loadControlLimit), sie schreibt die Failsafe NIE — sie LIEST sie nur (die Energiemanager-Werte
   // 1.000.000 W / PT2H sind vom Netzbetreiber/Portal schon gesetzt). Unser Failsafe-Write wurde vom Energiemanager
   // abgelehnt (er tut etwas, das die Vergleichs-Steuerbox nie tut) und der Limit-Write blieb trotzdem err7. Der Limit-Frame ist
   // byte-identisch zur Vergleichs-Steuerbox (Header/ackRequest:true/partial/limitId0/8360), Vergleichs-Steuerbox=err0, wir=err7 -> die Ursache ist
   // NICHT der Frame, sondern die Energiemanager-Autorisierung/Kategorisierung (eine Ebene unter/neben SPINE). Deshalb hier
   // KEIN Failsafe-Write mehr: direkt zum Limit-Write, exakt wie die Vergleichs-Steuerbox. Die Failsafe-Keys werden weiter aus der
   // Description GELERNT (nur als Diagnose-Log, EebusLpcLearnFailsafeKeys) — das Schreiben ist raus.
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
  if (!eebus_hems_mode && !ESp->lpc_active_wish) { frame_val = 0; }
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
  char filt[208];
  if (eebus_hems_mode && del_dur_now) {
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
  if (eebus_hems_mode && ESp->hb_sub) {
    EebusSpineSendHeartbeat("notify", false, 0, ESp->hb_cli_ent, ESp->hb_cli_feat);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Heartbeat %u (vor Write) an %s gesendet"), ESp->hb_counter - 1, ESp->peer_ip);
    EebusServeStateNotify();   // operatingState frisch vor dem Write -> Energiemanager sieht uns "aktiv"
  }
  EebusSpineSendAddr("write", false, 0, cmd,
                     EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
  free(cmd);
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
  char t[56];
  if (ESp->lpc_active_wish) {
    snprintf(t, sizeof(t), (1 == ESp->lpc_dir) ? "Einspeiselimit %ld W geschrieben" : "Limit %ld W geschrieben", frame_val);
  } else {
    strlcpy(t, (1 == ESp->lpc_dir) ? "Einspeise-Freigabe geschrieben" : "Freigabe geschrieben", sizeof(t));
  }
  EebusLpcSetResult(LPC_WRITE, t);
}

// Reaktion auf errorNumber:7 ("Write failed") auf den §14a-Write. EEBus-LPC-Spec [LPC-906]: die erste
// Ablehnung kann reines Timing sein (CS gerade im "init") -> "the EG may simply send Heartbeat and write
// command once more". [LPC-914]: der Write wird nur bewertet, wenn unmittelbar davor ein Heartbeat kam.
// Also im HEMS-Modus: statt sofort zu scheitern, einen kurz verzoegerten Retry ansetzen (der HB kommt im
// Retry aus EebusLpcWrite). Bis EEBUS_LPC_WRITE_MAX_TRIES; danach erst LPC_FAIL. SteuVE-Modus (Ladestation/Waermepumpen-Gateway)
// scheitert wie bisher sofort -> Verhalten byte-identisch .
void EebusLpcWriteRejected(void) {
  if (eebus_hems_mode && (ESp->lpc_write_tries < EEBUS_LPC_WRITE_MAX_TRIES)) {
    ESp->lpc_write_tries++;
    ESp->lpc_deadline = 0;   // Phasen-Timeout ruht bis zum Retry
    ESp->lpc_write_retry_at = millis() + EEBUS_LPC_WRITE_RETRY_MS;
    char t[56]; snprintf(t, sizeof(t), "err7 -> Heartbeat+Retry %u/%u",
                         ESp->lpc_write_tries, EEBUS_LPC_WRITE_MAX_TRIES);
    EebusLpcSetResult(LPC_WRITE, t);
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Write err7 @ %s -> Heartbeat+Write erneut (Versuch %u/%u, LPC-906/914)"),
           ESp->peer_ip, ESp->lpc_write_tries, EEBUS_LPC_WRITE_MAX_TRIES);
    return;
  }
  EebusLpcSetResult(LPC_FAIL, "Write abgelehnt");
}

// Read-Back: hat das Limit den gewuenschten Zustand?
void EebusLpcVerify(void) {
  EebusSpineSendAddr("read", false, 0, "{\"loadControlLimitListData\":[]}",
                     EEBUS_LPC_CLIENT_ENT, EEBUS_LPC_CLIENT_FEAT, ESp->lpc_peer_ent, ESp->lpc_peer_feat);
  ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
  EebusLpcSetResult(LPC_VERIFY, "Read-Back angefragt");
}

// Read-Back NICHT sofort, sondern verzoegert ansetzen (Latenz der Zustandsuebernahme im
// Geraet). first=true = erster Ansatz nach DELAY; first=false = Nachpoll-Versuch nach RETRY.
// Der eigentliche Read-Back-Read wird dann aus dem FUNC_EVERY_SECOND-Slot-Loop ausgeloest, wenn
// lpc_verify_at erreicht ist. Bis dahin ruht der Phasen-Timeout (lpc_deadline=0), Zustand LPC_WRITE.
void EebusLpcVerifySchedule(bool first) {
  if (first) { ESp->lpc_verify_tries = 0; ESp->lpc_verify_failed = false; }   // 
  ESp->lpc_verify_at = millis() + (first ? EEBUS_LPC_VERIFY_DELAY_MS : EEBUS_LPC_VERIFY_RETRY_MS);
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
   // Richtungswechsel auf derselben Verbindung. Binding und Abo bleiben gueltig (beide
   // Grenzen haengen am selben LoadControl-Feature), aber die limitId gilt nur fuer EINE Richtung ->
   // bei Wechsel neu aus der Beschreibung holen, sonst schriebe §9 auf die §14a-Grenze.
  if (dir != ESp->lpc_dir) {
    ESp->lpc_dir = dir;
    ESp->lpc_limit_id = -1;
  }
   // Betriebsmodus geraeteweise aus der peer-SKI (EEBusPeerMode). Gefunden -> setzt eebus_hems_mode
   // fuer diesen Write; nicht zugeordnet (z.B. manuelle IP-Verbindung, peer_ski="manual") -> globaler
   // EEBusHems-Default bleibt. Damit gelten pro Geraet automatisch die richtigen §14a-Write-Regeln.
  {
    int pm = EebusModeEffective(ESp->peer_ski);   // Zuordnung ODER Geraetetyp
    if (pm >= 0) {
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
      ESp->lpc_limit_id = -1;
      ESp->lpc_onboarded = false;   // 
      ESp->lpc_fs_val_key = -1; ESp->lpc_fs_dur_key = -1; ESp->lpc_fs_done = false; ESp->lpc_fs_step = 0;   // 
    }
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: LPC-Ziel manuell ent%d/feat%d (EEBusTarget) @ %s"),
           ESp->lpc_peer_ent, ESp->lpc_peer_feat, ESp->peer_ip);
  }
   // peer_dev (SPINE-Geraeteadresse) ist optional: fehlt sie, adressieren wir point-to-point
   // nur ueber entity/feature (der Peer ist auf der SHIP-Verbindung eindeutig).
  ESp->lpc_value = watt;
  ESp->lpc_active_wish = active;
  ESp->lpc_dur_s = (dur_s < 0) ? 0 : dur_s;   // Geltungsdauer dieses Auftrags
  ESp->lpc_write_tries = 0;   // err7-Retry-Zaehler fuer diesen neuen Write-Auftrag zuruecksetzen
  ESp->lpc_write_retry_at = 0;
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
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char *p = strstr(s, pat);
  if (nullptr == p) { return false; }
  *out = strtol(p + strlen(pat), nullptr, 10);
  return true;
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
    ESp->peer_hb_at = millis();
    if (0 == ESp->peer_hb_at) { ESp->peer_hb_at = 1; }   // 0 ist reserviert fuer "noch keiner da"
    if (ESp->peer_hb_lost) {
      ESp->peer_hb_lost = false;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Herzschlag von %s ist wieder da (Zaehler %u) - %s"),
             ESp->peer_ip, ESp->peer_hb_ctr, GetDateAndTime(DT_LOCAL).c_str());
    }
  }

   // --- Beschreibung der Messwerte: measurementId -> Bedeutung -------------------------------
  if (nullptr != strstr(json, "\"measurementDescriptionData\"")) {
   // Gehoert diese Beschreibung zum NETZANSCHLUSSPUNKT? Erkennungsmerkmal sind die Bedeutungen
   // gridFeedIn/gridConsumption — die fuehrt keine andere Einheit. Damit muss keine Entity-Nummer
   // fest verdrahtet werden, es bleibt geraeteunabhaengig.
    bool ist_netz = (nullptr != strstr(json, "\"gridFeedIn\""))
                 || (nullptr != strstr(json, "\"gridConsumption\""));
    if (ist_netz) { ESp->meas_ent = src_ent; }
    q = json;
    while (nullptr != (q = strstr(q, "\"measurementId\":"))) {
      uint32_t id = 0;
      EebusJsonInt(q, "measurementId", &id);
      nx = strstr(q + 16, "\"measurementId\":");
      const char *sc = strstr(q, "\"scopeType\":\"");
      if (sc && (!nx || (sc < nx)) && (id < 24)) {
        char s[28] = { 0 };
        EebusJsonStr(sc, "scopeType", s, sizeof(s));
        if (ist_netz) {
          int8_t code = 0;
          if      (0 == strcmp(s, "acPowerTotal"))    { code = 1; }
          else if (0 == strcmp(s, "gridFeedIn"))      { code = 2; }
          else if (0 == strcmp(s, "gridConsumption")) { code = 3; }
          else if (0 == strcmp(s, "acCurrent"))       { code = 4; }
          else if (0 == strcmp(s, "acVoltage"))       { code = 5; }
          else if (0 == strcmp(s, "acFrequency"))     { code = 6; }
          ESp->m_scope[id] = code;
        } else {
   // Fremde Einheit: Bedeutung im eigenen Platz merken, die Tabelle des Netzanschlusspunkts
   // bleibt unberuehrt. Sonst haette eine Batterie-Beschreibung dort "Id 3 = Ladezustand"
   // eingetragen und die Spannung des Netzanschlusspunkts waere danach falsch einsortiert.
          int i = EebusMoFind(src_ent, (uint8_t)id, true);
          if (i >= 0) {
            strlcpy(ESp->mo_scope[i], s, sizeof(ESp->mo_scope[i]));
   // Einheit mitnehmen, solange sie zu DIESER Kennung gehoert (bis zur naechsten measurementId).
            const char *up = strstr(q, "\"unit\":\"");
            if (up && (!nx || (up < nx))) { EebusJsonStr(up, "unit", ESp->mo_unit[i], sizeof(ESp->mo_unit[i])); }
          }
        }
      }
      q += 16;
    }
  }

   // --- Messwerte: je Kennung den letzten Wert behalten ---------------------------------------
  if (nullptr != strstr(json, "\"measurementData\"")) {
   // Solange der Netzanschlusspunkt nicht erkannt ist, verhaelt sich alles wie bisher (sonst waere
   // die Anzeige vor der ersten Beschreibung leer). Ist er erkannt, gilt er ausschliesslich.
    bool ist_netz = (ESp->meas_ent < 0) || (src_ent == ESp->meas_ent);
    q = json;
    while (nullptr != (q = strstr(q, "\"measurementId\":"))) {
      uint32_t id = 0;
      EebusJsonInt(q, "measurementId", &id);
      nx = strstr(q + 16, "\"measurementId\":");
      const char *vp = strstr(q, "\"value\":[{\"number\":");
      if (vp && (!nx || (vp < nx)) && (id < 24)) {
        long num = 0, sca = 0;
        EebusJsonLong(vp, "number", &num);
        EebusJsonLong(vp, "scale",  &sca);
        if (ist_netz) {
          if (ESp->m_scope[id]) {
            int8_t c = ESp->m_scope[id];
            ESp->m_val[c]  = num;
            ESp->m_sc[c]   = (int8_t)sca;
            ESp->m_have[c] = true;
          }
        } else {
          int i = EebusMoFind(src_ent, (uint8_t)id, true);
          if (i >= 0) { ESp->mo_v[i] = num; ESp->mo_sc[i] = (int8_t)sca; ESp->mo_have[i] = true; }
        }
      }
      q += 16;
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
      const char *vp = strstr(q, "\"value\":[{\"number\":");
      if (vp && (!nx || (vp < nx))) {
        long num = 0, sca = 0;
        EebusJsonLong(vp, "number", &num);
        EebusJsonLong(vp, "scale",  &sca);
        if (0 == strcmp(ct, "powerProductionNominalMax")) {
          ESp->pnom_prod = num; ESp->pnom_prod_sc = (int8_t)sca; ESp->pnom_prod_ok = true;
          ESp->pnom_prod_ent = src_ent;   // festhalten, WOHER sie kam
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Nennleistung Erzeugung von Entity %d: %ld (x10^%ld)"),
                 src_ent, num, sca);
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
    for (int i = 0; i < 2; i++) { if (ESp->cfg_ent[i] == src_ent) { b = i; break; } }
    if (b < 0) { for (int i = 0; i < 2; i++) { if (ESp->cfg_ent[i] < 0) { ESp->cfg_ent[i] = src_ent; b = i; break; } } }
    if (b >= 0) {
      q = json;
      while (nullptr != (q = strstr(q, "\"keyId\":"))) {
        uint32_t kid = 0;
        EebusJsonInt(q, "keyId", &kid);
        nx = strstr(q + 8, "\"keyId\":");
        const char *kn = strstr(q, "\"keyName\":\"");
        if (kn && (!nx || (kn < nx)) && (kid < 6)) {
          char s[40] = { 0 };
          EebusJsonStr(kn, "keyName", s, sizeof(s));
          int8_t code = 0;
          if      (0 == strcmp(s, "failsafeConsumptionActivePowerLimit")) { code = 1; }
          else if (0 == strcmp(s, "failsafeDurationMinimum"))             { code = 2; }
          else if (0 == strcmp(s, "failsafeProductionActivePowerLimit"))  { code = 3; }
          else if (0 == strcmp(s, "pvCurtailmentLimitFactor"))            { code = 4; }
          ESp->cfg_code[b][kid] = code;
        }
        q += 8;
      }
    }
  }

   // --- DeviceConfig: Werte einsortieren ------------------------------------------------------
  if (nullptr != strstr(json, "\"deviceConfigurationKeyValueData\"")) {
    int b = -1;
    for (int i = 0; i < 2; i++) { if (ESp->cfg_ent[i] == src_ent) { b = i; break; } }
    if (b >= 0) {
      q = json;
      while (nullptr != (q = strstr(q, "\"keyId\":"))) {
        uint32_t kid = 0;
        EebusJsonInt(q, "keyId", &kid);
        nx = strstr(q + 8, "\"keyId\":");
        if (kid < 6) {
          int8_t code = ESp->cfg_code[b][kid];
          const char *vp = strstr(q, "\"scaledNumber\":[{\"number\":");
          const char *dp = strstr(q, "\"duration\":\"");
          if ((2 == code) && dp && (!nx || (dp < nx))) {
            EebusJsonStr(dp, "duration", ESp->fs_dur, sizeof(ESp->fs_dur));
          } else if (vp && (!nx || (vp < nx))) {
            long num = 0, sca = 0;
            EebusJsonLong(vp, "number", &num);
            EebusJsonLong(vp, "scale",  &sca);
            if      (1 == code) { ESp->fs_cons = num; ESp->fs_cons_sc = (int8_t)sca; ESp->fs_cons_ok = true; }
            else if (3 == code) { ESp->fs_prod = num; ESp->fs_prod_sc = (int8_t)sca; ESp->fs_prod_ok = true; }
            else if (4 == code) { ESp->plf     = num; ESp->plf_sc     = (int8_t)sca; ESp->plf_ok     = true; }
          }
        }
        q += 8;
      }
    }
  }

   // --- Ist-Zustand der beiden Grenzen des Peers ----------------------------------------------
  if (nullptr != strstr(json, "\"loadControlLimitData\"")) {
    q = json;
    while (nullptr != (q = strstr(q, "\"limitId\":"))) {
      uint32_t lid = 0;
      EebusJsonInt(q, "limitId", &lid);
      nx = strstr(q + 10, "\"limitId\":");
      if (lid < 2) {
        const char *ap = strstr(q, "\"isLimitActive\":");
        if (ap && (!nx || (ap < nx))) {
          ESp->lim_act[lid] = (0 == strncmp(ap + 16, "true", 4)) ? 1 : 0;
   // VERSPAETETE BESTAETIGUNG. Der Read-Back pollt nur wenige Sekunden nach; antwortet
   // die Gegenstelle spaeter (oder meldet sie den Zustand von sich aus per notify), stand das
   // Ergebnis frueher fuer immer auf "Fehler" — obwohl die Begrenzung laengst wirkte. Genau
   // so gesehen . Nur richtigstellen, wenn der Fehlschlag aus der
   // Ruecklesung kam (lpc_verify_failed) — eine echte Ablehnung der Gegenstelle bleibt stehen.
          if (ESp->lpc_verify_failed && (LPC_FAIL == ESp->lpc_state) &&
              ((int)lid == ESp->lpc_limit_id) &&
              ((1 == ESp->lim_act[lid]) == ESp->lpc_active_wish)) {
            ESp->lpc_verify_failed = false;
            ESp->lpc_our_limit = ESp->lpc_active_wish;
            char t[56];
            snprintf(t, sizeof(t), ESp->lpc_active_wish ? "BESTAETIGT aktiv %u W (verspaetet)"
                                                        : "BESTAETIGT freigegeben (verspaetet)",
                     ESp->lpc_value);
            EebusLpcSetResult(LPC_DONE, t);
          }
        }
        const char *vp = strstr(q, "\"value\":[{\"number\":");
        if (vp && (!nx || (vp < nx))) {
          long num = 0, sca = 0;
          EebusJsonLong(vp, "number", &num);
          EebusJsonLong(vp, "scale",  &sca);
          ESp->lim_val[lid] = num;
          ESp->lim_sc[lid]  = (int8_t)sca;
        }
   // Geltungsdauer mitlesen. Steht KEINE im Eintrag, wird der gemerkte Wert geloescht —
   // sonst zeigte die Anzeige noch eine Dauer, die der Peer laengst nicht mehr fuehrt.
        const char *ep = strstr(q, "\"endTime\":\"");
        if (ep && (!nx || (ep < nx))) {
          EebusJsonStr(ep, "endTime", ESp->lim_dur[lid], sizeof(ESp->lim_dur[lid]));
   // Restlaufzeit in Sekunden merken + Empfangszeitpunkt stempeln -> die Anzeige
   // rechnet ab hier selbst weiter, statt den Text einzufrieren. Laesst sich die Dauer nicht
   // deuten (unbekannte Schreibweise), bleibt -1 und wir zeigen weiter den Rohtext.
          ESp->lim_dur_s[lid]  = EebusIsoDurSecs(ESp->lim_dur[lid]);
          ESp->lim_dur_at[lid] = millis();
        } else {
          ESp->lim_dur[lid][0] = '\0';
        }
      }
      q += 10;
    }
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
  bool ack = (nullptr != strstr(json, "\"ackRequest\":true"));

   // Adressen des Datagramms: von welchem Peer-Feature kam es, welches UNSERER Features ist gemeint?
  int peer_ent = 0, peer_feat = 0, our_ent = 0, our_feat = 0;
  EebusAddrParse(strstr(json, "\"addressSource\""), &peer_ent, &peer_feat);
  EebusAddrParse(strstr(json, "\"addressDestination\""), &our_ent, &our_feat);

   // Nutzdaten mitlesen und behalten (Messwerte, Failsafe-Werte, Limit-Zustaende) — unabhaengig
   // von der Schreibkette, damit die Bedienoberflaeche etwas anzuzeigen hat. Bisher lief das alles
   // durch und wurde verworfen, obwohl der Peer die Messwerte im Sekundentakt schickt.
  EebusHarvest(json, peer_ent);

  if (0 == strcmp(cls, "read")) {
    if (strstr(json, "nodeManagementDetailedDiscoveryData")) {
      EebusSpineAnswerDiscovery(their_ctr, 0);
      ESp->disco_answered = true;
      EebusSpineReadPeerDiscovery();   // Gegenseitigkeit: gleich SEINE Discovery abfragen
    } else if (strstr(json, "nodeManagementDestinationListData")) {
      EebusSpineAnswerDestinationList(their_ctr);   // echte Selbstbeschreibung (nicht resultData) -> strenge Peers (Energiemanager/Waermepumpen-Gateway) onboarden weiter statt abzubrechen
    } else if (strstr(json, "nodeManagementUseCaseData")) {
      EebusSpineAnswerUseCase(their_ctr);   // wir bieten LPC (ControllableSystem) an
    } else if (EebusSpineAnswerFeatureRead(json, their_ctr, our_ent, our_feat, peer_ent, peer_feat)) {
   // deviceDiagnosisState/Heartbeat, deviceClassification, loadControlLimit* beantwortet
    } else if (ack) {
   // unbekanntes read mit Ack-Wunsch -> leere Antwort, damit der Peer nicht haengt
      EebusSpineSend("reply", true, their_ctr, "{\"resultData\":[{\"errorNumber\":0}]}");
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
    if (ack) { EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":0}]}", 0, 0, peer_ent, peer_feat); }
  } else if (0 == strcmp(cls, "call")) {
   // z.B. nodeManagementSubscriptionRequestCall: der Peer abonniert uns -> quittieren + merken
    bool nm_sub = false;   // frisches Abo auf UNSER NodeManagement?
    if (strstr(json, "SubscriptionRequestCall")) {
      nm_sub = !ESp->peer_subscribed && (nullptr != strstr(json, "\"serverFeatureType\":\"NodeManagement\""));
      ESp->peer_subscribed = true;
   // Abo auf UNSER DeviceDiagnosis (ent1/feat2)? -> Heartbeat-NOTIFY alle 30 s an den Abonnenten
      if (strstr(json, "\"serverFeatureType\":\"DeviceDiagnosis\"")) {
        ESp->hb_cli_ent = 0; ESp->hb_cli_feat = 0;
        EebusAddrParse(strstr(json, "\"clientAddress\""), &ESp->hb_cli_ent, &ESp->hb_cli_feat);
        ESp->hb_sub = true;
        ESp->hb_next = millis() + 2000;   // ersten Heartbeat zeitnah senden
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer %s abonniert unser DeviceDiagnosis -> Heartbeat alle 30 s (an ent %d feat %d)"),
               ESp->peer_ip, ESp->hb_cli_ent, ESp->hb_cli_feat);
        EebusServeStateNotify();   // operatingState:normalOperation sofort nachschieben (Energiemanager braucht ihn)
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: DeviceDiagnosis-State (normalOperation) an %s gesendet"), ESp->peer_ip);
      }
   // Abo auf UNSER LoadControl (ent1/feat6)? -> HEMS liest hier unser §14a-Limit.
   // Absenderadresse merken; sofort den aktuellen Limit-Zustand als NOTIFY nachschieben.
      if (strstr(json, "\"serverFeatureType\":\"LoadControl\"")) {
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
    if (ack) { EebusSpineSendAddr("result", true, their_ctr, "{\"resultData\":[{\"errorNumber\":0}]}", 0, 0, peer_ent, peer_feat); }
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
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Use-Case-Auskunft des Peers gemerkt (%u B)"), (unsigned)ul);
      }
    }
   // LPC-Controller: Antwort auf unsere Limit-Beschreibung -> limitId der consume-Grenze
    else if ((LPC_READDESC == ESp->lpc_state) && strstr(json, "loadControlLimitDescriptionData")) {
      int lid = -1;
      if (EebusLpcPickLimitId(json, ESp->lpc_dir, &lid)) {   // /Grenze der aktuellen Richtung waehlen
        ESp->lpc_limit_id = lid;
        char t[56]; snprintf(t, sizeof(t), "limitId=%d erkannt", lid);
        EebusLpcSetResult(LPC_READDESC, t);
        EebusLpcReadData();   // erst Ist-LimitData lesen, DANN schreiben
      } else {
        EebusLpcSetResult(LPC_FAIL, (1 == ESp->lpc_dir) ? "keine produce-Grenze (EEG9) in Beschreibung"
                                                        : "keine limitId in Beschreibung");
      }
    }
   // Antwort auf den Ist-LimitData-Read (VOR dem Write). isLimitChangeable pruefen wie Referenz-Umsetzung:
   // explizit false -> Grenze nicht beschreibbar (abbrechen, klare Diagnose). Fehlt das Feld oder true
   // -> weiter zum Write (der Energiemanager hat jetzt unsere LimitData-Anfrage gesehen = volle Referenz-Umsetzung-Sequenz).
    else if ((LPC_READDATA == ESp->lpc_state) && strstr(json, "loadControlLimitListData")) {
   // Der Energiemanager hat jetzt unsere LimitData-Anfrage gesehen (= volle Referenz-Umsetzung-Sequenz). isLimitChangeable
   // NUR zur Diagnose; NICHT hart abbrechen (sonst Regressions-Risiko am funktionierenden Waermepumpen-Gateway-Pfad).
      bool ro = (nullptr != strstr(json, "\"isLimitChangeable\":false"));
      if (eebus_hems_mode && (0 == ESp->lpc_sel_step)) {
   // limitId-0-Read beantwortet -> jetzt limitId 1 lesen (2 getrennte Selektor-Reads
   // wie die Vergleichs-Steuerbox), DANN erst weiter. Bleibt in LPC_READDATA (frische Deadline).
        ESp->lpc_sel_step = 1;
        EebusLpcReadLimitSel(1);
        ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
        EebusLpcSetResult(LPC_READDATA, "LimitData limitId0 -> lese limitId1");
      } else {
        EebusLpcSetResult(LPC_READDATA, ro ? "LimitData: isLimitChangeable=false (weiter)"
                                           : "Ist-LimitData erhalten -> DeviceConfig lesen");
        EebusLpcReadCfg();   // erst DeviceConfig (Failsafe) lesen, DANN schreiben
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
      ESp->lpc_deadline = millis() + EEBUS_LPC_PHASE_TIMEOUT_MS;
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
        } else if (0 == restsec) {
          EebusLpcSetResult(LPC_FAIL, "ausgelaufen - Restlaufzeit 0 (keine Ablehnung)");
        } else {
          EebusLpcSetResult(LPC_FAIL, "nicht uebernommen - keine Bestaetigung");
        }
      }
    }
   // Ladestation quittiert unser Binding/Write als "reply" mit resultData (NICHT als "result") ->
   // hier abfangen, sonst haengt die LPC-Sequenz im Timeout (bewiesen : errorNumber:0 "Binding request was successful").
    else if (((LPC_BIND == ESp->lpc_state) || (LPC_FAILSAFE == ESp->lpc_state) || (LPC_WRITE == ESp->lpc_state)) && strstr(json, "resultData")) {
      bool ok = (nullptr != strstr(json, "\"errorNumber\":0")) || (nullptr == strstr(json, "errorNumber"));
      if (LPC_BIND == ESp->lpc_state) {
        if (ok) { ESp->lpc_bound = true; EebusLpcSetResult(LPC_BIND, "Binding bestaetigt"); EebusLpcReadDesc(); }
        else    { EebusLpcSetResult(LPC_FAIL, "Binding abgelehnt"); }
      } else if (LPC_FAILSAFE == ESp->lpc_state) {   // Failsafe-Write quittiert -> Dauer bzw. Limit
        EebusLpcFailsafeResult(ok);
      } else {   // LPC_WRITE
        if (ok) { EebusLpcVerifySchedule(true); }   // Read-Back verzoegert
        else    { EebusLpcWriteRejected(); }   // err7 -> Heartbeat+Retry (HEMS), sonst FAIL
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
    if (LPC_BIND == ESp->lpc_state) {
      if (ok) { ESp->lpc_bound = true; EebusLpcSetResult(LPC_BIND, "Binding bestaetigt"); EebusLpcReadDesc(); }
      else    { EebusLpcSetResult(LPC_FAIL, "Binding abgelehnt"); }
    } else if (LPC_FAILSAFE == ESp->lpc_state) {   // Failsafe-Write quittiert -> Dauer bzw. Limit
      EebusLpcFailsafeResult(ok);
    } else if (LPC_WRITE == ESp->lpc_state) {
      if (ok) { EebusLpcVerifySchedule(true); }   // Write quittiert -> Read-Back verzoegert
      else    { EebusLpcWriteRejected(); }   // err7 -> Heartbeat+Retry (HEMS), sonst FAIL
    }
  }
   // notify vom Peer: nur mitschneiden (bereits im Log)
}

// Handschlag SAUBER abschliessen — Datenphase (Done) erst erreichen, NACHDEM
// pinState UND accessMethods des Peers empfangen wurden (Referenz-Umsetzung-Gate CheckListen->AccessRequest->
// approveHandshake). Wird aus 3 Punkten aufgerufen: nach Peer-accessMethods (Regelfall) und aus den
// beiden Gate-Timeout-Fallbacks. Der bisherige Done-Block (Discovery-Eroeffnung) steckt jetzt hier.
void EebusSmeReachDone(const char *grund) {
  ESp->sme = SME_DONE;
  ESp->sme_deadline = 0;
  ESp->disco_fallback_at = millis() + 3000;   // Deadlock-Bruch, falls der Peer auf UNS wartet
  EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP-Handshake KOMPLETT (Done, %s) @ %s - Datenphase erreicht"),
         grund, ESp->peer_ip);
   // Erst JETZT merken, nicht schon beim Verbinden: eine Verbindung, die es nicht bis in die
   // Datenphase schafft, ist kein Partner, den man beim naechsten Start wieder suchen sollte.
   // Server-Slots ausgenommen — dort steht die Peer-IP als Pseudo-SKI, die identifiziert niemanden.
  if (!ESp->via_srv) { EebusPeerRemember(ESp->peer_ski); }
  eebus_ac_state = AC_DONE;   // Wiederaufbau erledigt (oder gar nicht noetig gewesen)
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
    if (SME_DONE == ESp->sme) {   // normales Ende nach Done
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer hat Verbindung geschlossen (%s)"), ESp->peer_ip);
      EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
      ESp->sme = SME_OFF;
      ESp->state = SHIP_IDLE;
      if (ESp->keepalive && (ESp->peer_idx >= 0)) {   // Keep-Alive: nach kurzem Warten neu verbinden
        ESp->reconnect_at = millis() + (ESp->sme_pending_logged ? EEBUS_PENDING_RETRY_MS : EEBUS_SPINE_KEEPALIVE_MS);
      }
      EebusTeardownLater();   // Client verzoegert freigeben
    } else {   // Close VOR Done = abgewiesen
      EebusSmeFail("peer close vor done (Pairing abgelehnt?)");
    }
    return;
  }
  if (2 == classifier) {   // SPINE-Datagram (Datenphase, M3)
    EebusSpineHandle(json);
    return;
  }
  if (1 != classifier) { return; }   // CMI(0) o.ae. hier nicht erwartet

  if (strstr(json, "\"connectionHello\"")) {
    char phase[12] = { 0 };
    EebusJsonStr(json, "phase", phase, sizeof(phase));
    if (0 == strcmp(phase, "ready")) {
      if (SME_HELLO == ESp->sme) {
   // Peer ist ready -> ProtocolHandshake (wir = Client -> announceMax, SHIP 13.4.4.2)
        if (!EebusShipSendJson("{\"messageProtocolHandshake\":[{\"handshakeType\":\"announceMax\"},"
                               "{\"version\":[{\"major\":1},{\"minor\":0}]},"
                               "{\"formats\":[{\"format\":[\"JSON-UTF8\"]}]}]}", 1)) {
          EebusSmeFail("prot send"); return;
        }
        ESp->sme = SME_PROT;
        ESp->sme_deadline = millis() + EEBUS_SME_PROT_TIMEOUT_MS;
      }
    } else if (0 == strcmp(phase, "pending")) {
   // Peer kennt unsere SKI noch nicht -> Pairing: SKI im Geraete-UI bestaetigen!
      if (!ESp->sme_pending_logged) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer %s meldet PENDING - unsere SKI %s im Geraete-UI bestaetigen (Pairing)"),
               ESp->peer_ip, Eebus.own_ski);
        ESp->sme_pending_logged = true;
      }
      ESp->sme_deadline = millis() + EEBUS_SME_HELLO_TIMEOUT_MS;   // Peer lebt -> weiter warten
      if (strstr(json, "prolongation")) {   // Verlaengerung gewuenscht ->
        EebusShipSendJson("{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":60000}]}", 1);
      }   // Update-Message (wir bleiben ready)
    } else {   // aborted / unbekannt
      EebusSmeFail("hello aborted");
    }
    return;
  }

  if (strstr(json, "\"messageProtocolHandshakeError\"")) {
    EebusSmeFail("prot error vom peer");
    return;
  }

  if (strstr(json, "\"messageProtocolHandshake\"")) {
    char ht[16] = { 0 };
    EebusJsonStr(json, "handshakeType", ht, sizeof(ht));
    if ((SME_PROT == ESp->sme) && (0 == strcmp(ht, "select"))) {
   // NICHT mehr vorpreschen. Auswahl bestaetigen (select 1:1 zurueckspiegeln,
   // SHIP 13.4.4.2.3) + pinState:none senden — DANN auf den pinState des Peers warten (SME_PIN),
   // erst nach dessen accessMethods die Datenphase erreichen (Referenz-Umsetzung-Gate). Frueher wurde hier
   // sofort accessMethodsRequest + SME_DONE + erster Read gefeuert -> Ueberstuerzen (err7-Ursache).
      if (!EebusShipSendJson(json, 1)) { EebusSmeFail("prot confirm send"); return; }
      EebusShipSendJson("{\"connectionPinState\":[{\"pinState\":\"none\"}]}", 1);
      ESp->sme = SME_PIN;
      ESp->sme_deadline = millis() + EEBUS_SME_GATE_TIMEOUT_MS;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: select bestaetigt + pinState:none @ %s - warte auf Peer-pinState"), ESp->peer_ip);
    } else {
      EebusSmeFail("prot unerwartet");
    }
    return;
  }

  if (strstr(json, "\"connectionPinState\"")) {
    char pin[12] = { 0 };
    EebusJsonStr(json, "pinState", pin, sizeof(pin));
    if (0 == strcmp(pin, "required")) {   // PIN unterstuetzen wir nicht
      EebusSmeFail("peer verlangt PIN");
      return;
    }
   // none/optional/leer: ok. dies ist das 1. Peer-Gate -> JETZT (nicht schon beim
   // select) accessMethodsRequest senden und auf die accessMethods des Peers warten (SME_ACCESS).
    if (SME_PIN == ESp->sme) {
      EebusShipSendJson("{\"accessMethodsRequest\":[]}", 1);
      ESp->sme = SME_ACCESS;
      ESp->sme_deadline = millis() + EEBUS_SME_GATE_TIMEOUT_MS;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer-pinState=%s -> accessMethodsRequest @ %s - warte auf Peer-accessMethods"),
             pin[0] ? pin : "none", ESp->peer_ip);
    }
    return;
  }

  if (strstr(json, "\"accessMethodsRequest\"")) {   // Peer fragt UNSERE Kennung ab
   // accessMethods.id = unsere mDNS-id (eebus_adv_id). Vorher wich der SHIP-Identifier
   // ("Tasmota-EEBusGuard-<hostname>") von der mDNS-id ab -> der Energiemanager sah zwei verschiedene Kennungen.
   // Referenz (Referenz-Umsetzung): mDNS-id == accessMethods.id == SPINE-deviceCode = EINE sd.Identifier().
    char am[80];
    snprintf_P(am, sizeof(am), PSTR("{\"accessMethods\":[{\"id\":\"%s\"}]}"), eebus_adv_id);
    EebusShipSendJson(am, 1);
    return;
  }

  if (strstr(json, "\"accessMethods\"")) {   // Kennung des Peers (Antwort auf unseren Request)
    EebusJsonStr(json, "id", ESp->peer_id, sizeof(ESp->peer_id));
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: Peer AccessMethods id=%s"), ESp->peer_id);
   // letztes Referenz-Umsetzung-Gate erfuellt -> jetzt erst Datenphase (Done) + erster Read.
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

  if (!ESp->client->connected() && (ESp->client->available() <= 0)) {
    if (SME_DONE == ESp->sme) {   // Peer hat aufgelegt
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SHIP-Verbindung beendet (%s)"), ESp->peer_ip);
      EebusStatSet(ESp->peer_ski, SHIP_CMI_OK, "");
      ESp->sme = SME_OFF;
      ESp->state = SHIP_IDLE;
      if (ESp->keepalive && (ESp->peer_idx >= 0)) {   // Keep-Alive: bald neu verbinden
        ESp->reconnect_at = millis() + (ESp->sme_pending_logged ? EEBUS_PENDING_RETRY_MS : EEBUS_SPINE_KEEPALIVE_MS);
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
  while (ESp->client && (ESp->client->available() > 0)) {
    int n = EebusWsRecv(rx, rxmax - 1, 1000);
    if (-2 == n) {   // WS-Close-Frame
      EebusSmeDispatch((const uint8_t*)"\x03{}", 3);   // wie connectionClose behandeln
      return;
    }
    if (n < 0) { break; }   // Timeout/Fehler -> naechster Tick
    if (0 == n) { EebusHeapCheck("rx_ctrl"); continue; }   // nach Ping/Pong/Skip (schrieb rxbuf?)
    EebusHeapCheckN("rx_recv", (uint32_t)n);   // ROH nach WsRecv, VOR Verarbeitung
    rx[n] = '\0';
    ESp->last_rx = millis();
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
  if ((SME_PIN == ESp->sme) || (SME_ACCESS == ESp->sme)) {
    if (TimeReached(ESp->sme_deadline)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Handschlag-Gate (%s) Timeout @ %s - gehe trotzdem in Datenphase"),
             (SME_PIN == ESp->sme) ? "pinState" : "accessMethods", ESp->peer_ip);
      EebusSmeReachDone("gate-timeout");
    }
    return;
  }

   // Timeouts
  if ((SME_HELLO == ESp->sme) || (SME_PROT == ESp->sme)) {
   // Obergrenze ZUERST pruefen (sonst wird unten endlos verlaengert)
    if ((SME_HELLO == ESp->sme) &&
        (millis() - ESp->sme_hello_start > EEBUS_SME_HELLO_MAX_MS)) {
      EebusSmeFail("pairing nicht bestaetigt");
      return;
    }
    if (TimeReached(ESp->sme_deadline)) {
   // (live bewiesen ): Steht der Peer auf "pending" (er kennt unsere SKI noch
   // nicht und wartet auf die Freigabe im Bedien-UI), dann darf die Verbindung NICHT nach 60 s
   // sterben. Genau das passierte: der Peer sendet "pending" EINMAL, die Verlaengerung unten im
   // Empfangspfad greift aber nur, wenn eine WEITERE pending-Nachricht eintrifft -> Deadline lief
   // ab -> "hello timeout" nach exakt 60 s. Das Bedien-UI braucht aber allein bis zu 60 s fuer den
   // Verbindungsaufbau und oeffnet ERST DANN sein 2-Minuten-Freigabefenster: unsere Verbindung
   // starb also regelmaessig in dem Moment, in dem das Fenster aufging — Pairing unmoeglich.
   // Jetzt halten wir die Wartephase aktiv und melden uns beim Peer periodisch als weiter
   // wartebereit (SHIP 13.4.4.1.3 Update-Message), bis die Obergrenze oben erreicht ist.
      if ((SME_HELLO == ESp->sme) && ESp->sme_pending_logged) {
   // NUR das eigene "ready" wiederholen (Update-Message). KEIN prolongationRequest: das darf
   // nach SHIP nur senden, wer SELBST wartet — also die Gegenstelle im Zustand "pending".
   // Wir sind bereits ready; eine Verlaengerungsbitte von dieser Seite ist protokollwidrig und
   // wird mit "aborted" quittiert (live gesehen: Abbruch 90 ms nach dem Senden).
        EebusShipSendJson("{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":60000}]}", 1);
        ESp->sme_deadline = millis() + EEBUS_SME_HELLO_TIMEOUT_MS;
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Pairing laeuft noch (%s) - Wartezeit verlaengert (%lu s bisher)"),
               ESp->peer_ip, (unsigned long)((millis() - ESp->sme_hello_start) / 1000));
        return;
      }
      EebusSmeFail((SME_HELLO == ESp->sme) ? "hello timeout" : "prot timeout");
      return;
    }
  }
   // LPC-Sequenz-Timeout (Binding/ReadDesc/Write/Verify haengt)
  if ((ESp->lpc_state >= LPC_BIND) && (ESp->lpc_state <= LPC_VERIFY) &&
      ESp->lpc_deadline && TimeReached(ESp->lpc_deadline)) {
    ESp->lpc_deadline = 0;
    if (LPC_READCFG == ESp->lpc_state) {   // DeviceConfig-Read best-effort -> trotzdem schreiben
      EebusLpcSetResult(LPC_READCFG, "DeviceConfig-Read Timeout -> schreibe trotzdem");
      EebusLpcWrite();
    } else if (LPC_FAILSAFE == ESp->lpc_state) {   // Failsafe-Write Timeout -> Limit trotzdem schreiben
      ESp->lpc_fs_done = true; ESp->lpc_fs_step = 0;
      EebusLpcSetResult(LPC_FAILSAFE, "Failsafe-Write Timeout -> schreibe Limit");
      EebusLpcWrite();
    } else {
      EebusLpcSetResult(LPC_FAIL, "Timeout (keine Antwort)");
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
  ESp = &EConn[ci];
  ESp->keepalive = true;   // Verbindung dauerhaft halten (Auto-Reconnect nach Done)
  ESp->reconnect_at = 0;
  EebusShipConnect(idx);   // Ergebnis steht in ESp->state/sme/err
  return ci;
}

// Selbsttaetiger Wiederaufbau nach einem Neustart — im Sekundentakt aufgerufen.
// Uebergibt nach dem ersten geglueckten Verbindungsversuch an den vorhandenen Keep-Alive,
// der eine LAUFENDE Verbindung ohnehin schon haelt (und den Peer dabei ueber die SKI
// nachschlaegt, falls sich die Scan-Reihenfolge geaendert hat).
void EebusAutoConnectRun(void) {
  if (AC_DONE == eebus_ac_state) { return; }
  if (0 == eebus_ac_delay_s) { eebus_ac_state = AC_DONE; return; }   // per EEBusAutoConn 0 abgeschaltet

  switch (eebus_ac_state) {

    case AC_IDLE:
   // Drei Bedingungen, alle noetig (Begruendung im Kopf des Abschnitts):
   // Netz steht · Uhr ist synchronisiert · eine SKI ist gemerkt.
      if (!(WifiHasIP() || ((uint32_t)EthernetLocalIP() != 0))) { return; }
      if (!RtcTime.valid) { return; }
      if (!EebusPeerRecall()) { eebus_ac_state = AC_DONE; return; }   // nichts gemerkt
      eebus_ac_next  = millis() + (eebus_ac_delay_s * 1000UL);
      eebus_ac_state = AC_WAIT;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau zu %s vorgemerkt - Beginn in %u s (Netz und Uhr stehen)"),
             eebus_ac_ski, eebus_ac_delay_s);
      break;

    case AC_WAIT:
      if (!TimeReached(eebus_ac_next)) { return; }
   // Ist die Verbindung inzwischen anderweitig zustande gekommen (von Hand oder eingehend)?
   // Dann ist nichts mehr zu tun.
      for (uint32_t i = 0; i < EEBUS_MAX_CONN; i++) {
        if (EConn[i].active && (0 == strcmp(EConn[i].peer_ski, eebus_ac_ski))) {
          eebus_ac_state = AC_DONE;
          return;
        }
      }
      if (!EebusStartScan()) { eebus_ac_next = millis() + 5000; return; }   // laeuft schon / kein mDNS
      eebus_ac_next  = millis() + EEBUS_AC_SCAN_WAIT_MS;
      eebus_ac_state = AC_SCAN;
      break;

    case AC_SCAN: {
      if (!TimeReached(eebus_ac_next)) { return; }
      int idx = -1;
      for (uint32_t k = 0; k < Eebus.peer_count; k++) {
        if (0 == strcmp(Eebus.peers[k].ski, eebus_ac_ski)) { idx = (int)k; break; }
      }
      eebus_ac_try++;
      if (idx >= 0) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau - verbinde mit %s (Versuch %u von %u)"),
               eebus_ac_ski, eebus_ac_try, EEBUS_AC_MAX_TRY);
        if (EebusConnectPeer(idx) >= 0) {
   // Ab hier uebernimmt der Keep-Alive: er haelt die Verbindung und baut sie bei Abriss
   // selbst wieder auf. Ob der Versuch geglueckt ist, meldet der Slot in EEBusStatus.
          eebus_ac_state = AC_DONE;
          return;
        }
      } else {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau - %s im Scan nicht gefunden (Versuch %u von %u)"),
               eebus_ac_ski, eebus_ac_try, EEBUS_AC_MAX_TRY);
      }
      if (eebus_ac_try >= EEBUS_AC_MAX_TRY) {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: Wiederaufbau aufgegeben - bitte von Hand scannen und verbinden"));
        eebus_ac_state = AC_DONE;
        return;
      }
      uint16_t warte_s = pgm_read_word(&kEebusAcRetryS[eebus_ac_try - 1]);
      eebus_ac_next  = millis() + (warte_s * 1000UL);
      eebus_ac_state = AC_WAIT;
      break;
    }
  }
}

// EEBusAutoConn            Zustand anzeigen
// EEBusAutoConn <sekunden> Wartezeit nach Netz+Uhr, bevor selbsttaetig gesucht wird (0 = aus)
// EEBusAutoConn -1         gemerkten Peer vergessen (kein Wiederaufbau mehr)
void CmndEebusAutoConn(void) {
  if (XdrvMailbox.data_len > 0) {
    long v = atol(XdrvMailbox.data);
    if (v < 0) {
      EebusPeerForget();
    } else if (v <= 3600) {
      eebus_ac_delay_s = (uint16_t)v;
      if (0 == v) { eebus_ac_state = AC_DONE; }
    } else {
      ResponseCmndChar_P(PSTR("Wartezeit 0..3600 s, -1 = Peer vergessen"));
      return;
    }
  }
  const char *zst = (AC_IDLE == eebus_ac_state) ? "wartet auf Netz und Uhr" :
                    (AC_WAIT == eebus_ac_state) ? "Wartezeit laeuft" :
                    (AC_SCAN == eebus_ac_state) ? "sucht" : "abgeschlossen";
  Response_P(PSTR("{\"%s\":{\"WarteS\":%u,\"Peer\":\"%s\",\"Zustand\":\"%s\",\"Versuche\":%u}}"),
             XdrvMailbox.command, eebus_ac_delay_s,
             eebus_ac_ski[0] ? eebus_ac_ski : "-", zst, eebus_ac_try);
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
  bool ok = (SHIP_CMI_OK == ESp->state);
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
  if (!ESp->via_srv && (0 == strcmp(eebus_ac_ski, ESp->peer_ski))) { eebus_ac_state = AC_DONE; }
  if (ESp->active && (ESp->client || ESp->via_srv) &&   // auch Server-Slots verabschieden sich
      (ESp->sme != SME_OFF) && (ESp->sme != SME_FAIL)) {
   // Sauberer SHIP-Abschied (13.4.7): connectionClose als End-Message (classifier 3)
    EebusShipSendJson("{\"connectionClose\":[{\"phase\":\"announce\"},{\"maxTime\":500},"
                      "{\"reason\":\"unplug\"}]}", 3);
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
  ESp->peer_hb_at = 0; ESp->peer_hb_ctr = 0; ESp->peer_hb_tmo_s = 0; ESp->peer_hb_lost = false;
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
    ESp = &EConn[ci];
    EebusDisconnectNow();
  } else {
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      ESp = &EConn[i];
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
  Response_P(PSTR("{\"%s\":{\"Role\":%d,\"Hems\":%d,\"Active\":%d,\"Slots\":["),
             XdrvMailbox.command, eebus_role, eebus_hems_mode, EebusConnActive());
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
    char hbinfo[112] = { 0 };
    if (cc->peer_hb_at || cc->hb_lost_cnt) {
      snprintf_P(hbinfo, sizeof(hbinfo),
                 PSTR("\"PeerHerzschlagS\":%ld,\"ZusageS\":%u,\"Verloren\":%u,\"VerlorenAm\":\"%s\","),
                 cc->peer_hb_at ? (long)((millis() - cc->peer_hb_at) / 1000) : -1L,
                 cc->peer_hb_tmo_s, cc->hb_lost_cnt, cc->hb_lost_at);
    }
    ResponseAppend_P(PSTR("%s{\"Slot\":%d,\"State\":\"%s\",\"Sme\":\"%s\",\"Ip\":\"%s\",\"Port\":%u,"
                          "\"PeerSki\":\"%s\",\"PeerId\":\"%s\",\"Lpc\":\"%s\",\"LpcResult\":\"%s\",%s\"Error\":\"%s\"}"),
                     (i) ? "," : "", i, st, EebusSmeName(cc->sme), cc->peer_ip, cc->peer_port,
                     cc->peer_ski, cc->peer_id, EebusLpcName(cc->lpc_state), cc->lpc_result, hbinfo, cc->err);
  }
  ResponseAppend_P(PSTR("]}}"));
}

void CmndEebusTrust(void) {
   // Platzhalter fuer M2b (Peer-SKI in Trust-Liste aufnehmen). Aktuell nur Quittung.
  ResponseCmndChar_P(PSTR("Trust-Liste kommt mit M2b (SHIP-Hello-Pairing)"));
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
    eebus_hems_mode = (0 != XdrvMailbox.payload) ? 1 : 0;
  }
  Response_P(PSTR("{\"%s\":{\"Mode\":%d,\"Meaning\":\"%s\"}}"), XdrvMailbox.command, eebus_hems_mode,
             eebus_hems_mode ? "HEMS/Energiemanager (volle Lesesequenz, Heartbeat vor dem Write, "
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
  if (ci < 0) { ResponseCmndChar_P(PSTR("keine Verbindung zu diesem Peer (erst EEBusConnect)")); return false; }
  ESp = &EConn[ci];
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
    if (1 == n) { ESp = &EConn[found]; return true; }
    ResponseCmndChar_P(n ? PSTR("mehrere Verbindungen - SKI-Praefix angeben") : PSTR("keine stehende Verbindung"));
    return false;
  }
  bool is_ski = false;
  for (const char *p = arg; *p; p++) { if (strchr("abcdefABCDEF", *p)) { is_ski = true; break; } }
  if (is_ski) {
    size_t pl = strlen(arg);
    for (int i = 0; i < EEBUS_MAX_CONN; i++) {
      if (EConn[i].active && (SME_DONE == EConn[i].sme) &&
          (0 == strncasecmp(EConn[i].peer_ski, arg, pl))) { ESp = &EConn[i]; return true; }
    }
    ResponseCmndChar_P(PSTR("keine stehende Verbindung mit dieser SKI"));
    return false;
  }
  return EebusSelectConnectedPeer(atoi(arg));   // reine Zahl -> alter Index-Weg
}

// EEBusLpc <idx|ski|hm> <watt> [dauer_s] — BEZUGS-Limit schreiben (§14a EnWG).
// hiess frueher "EEBusLpc" (Name aus einer frueheren Fassung, als es nur den §14a-Test gab). Jetzt nach dem
// offiziellen Use-Case-Namen benannt: limitationOfPowerConsumption = LPC. Gegenstueck: EEBusLpp (§9 EEG).
void CmndEebusLimit(void) {
  char *space = (XdrvMailbox.data_len > 0) ? strchr(XdrvMailbox.data, ' ') : nullptr;
  if (nullptr == space) { ResponseCmndChar_P(PSTR("Nutzung: EEBusLpc <idx|ski|hm> <watt> [dauer_s]  (Bezugsgrenze)")); return; }
   // erstes Token als STRING (Index ODER SKI-Praefix ODER "hm"/"auto") -> index-unabhaengig adressieren
  char sel[48]; size_t sl = (size_t)(space - XdrvMailbox.data);
  if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
  memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
  uint32_t watt = (uint32_t)atoi(space + 1);
   // optionale Geltungsdauer in Sekunden als drittes Argument.
   // OHNE Angabe gilt jetzt auch im HEMS-Betrieb eine Dauer — die Vorgabe 900 s. Bis ging
   // dort ohne drittes Argument gar keine Dauer mit; genau dieser Fall erbt eine abgelaufene
   // Geltungsdauer der Gegenstelle und wird wirkungslos, ohne dass es irgendwo auffaellt (
   // gemessen, Beleg oben bei eebus_del_dur). Der SteuVE-Betrieb behaelt seinen bisherigen Wert —
   // dieser Pfad ist an Ladestation/Waermepumpen-Gateway bewiesen und wird nicht angefasst.
  char *space2 = strchr(space + 1, ' ');
  long dauer = eebus_hems_mode ? EEBUS_LPC_DUR_DEFAULT_S : eebus_limit_dur_s;
  if (nullptr != space2) { dauer = atol(space2 + 1); eebus_limit_dur_s = dauer; }
   // Wertebereich duration 0..86400. Lieber hier abweisen als einen
   // Frame senden, den die Gegenstelle stillschweigend verwirft. 0 bleibt ausdruecklich erlaubt und
   // heisst "unbefristet"; dann loescht der Frame-Bau die alte Endzeit mit (Automatik, s.o.).
  if (dauer < 0 || dauer > EEBUS_LPC_DUR_MAX_S) {
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Dauer %ld s unzulaessig - erlaubt 0..%ld s (0 = unbefristet)\"}}"),
               XdrvMailbox.command, dauer, EEBUS_LPC_DUR_MAX_S);
    return;
  }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2 (Steuerbox)")); return; }
  if (!EebusSelectPeerArg(sel)) { return; }
  bool ok = EebusLpcStart(watt, true, 0, dauer);   // /consume (§14a), Dauer aus dem Befehl
   // "DauerS" meldet die TATSAECHLICH gesendete Dauer. Bis stand hier der gemerkte
   // globale Wert — der wich vom gesendeten ab und fuehrte beim Auswerten in die Irre.
  Response_P(PSTR("{\"%s\":{\"Started\":%s,\"Ip\":\"%s\",\"Watt\":%u,\"DauerS\":%ld,\"Lpc\":\"%s\",\"Result\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", ESp->peer_ip, watt, dauer,
             EebusLpcName(ESp->lpc_state), ESp->lpc_result);
}

// Freigabe: EEBusRelease <idx> — Limit an DIESER SteuVE deaktivieren (isLimitActive:false).
void CmndEebusRelease(void) {
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusRelease <idx|ski|hm> [watt]")); return; }
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
    if (!EebusSelectPeerArg(sel)) { return; }
  }
   // optionaler Watt-Wert. Die Vergleichs-Steuerbox sendet beim Deaktivieren den value MIT
   // (isLimitActive:false, value:<letzter Wert>). Ohne Arg: den letzten lpc_value behalten (0 nach Boot).
   // Fuer den §5a-Zustands-Test (erst deaktivieren, dann aktivieren) Vergleichs-Steuerbox-identisch: EEBusRelease 0 8360.
  char *space = strchr(XdrvMailbox.data, ' ');
  uint32_t watt = (nullptr != space) ? (uint32_t)atoi(space + 1) : ESp->lpc_value;
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
  if (nullptr == space) { ResponseCmndChar_P(PSTR("Nutzung: EEBusLpp <idx|ski|hm> <watt> [dauer_s]  (Einspeisegrenze, Betrag)")); return; }
  char sel[48]; size_t sl = (size_t)(space - XdrvMailbox.data);
  if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
  memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
  long w = atol(space + 1);
  if (w < 0) { w = -w; }   // Betrag; das Vorzeichen setzt der Frame-Bau
  char *space2 = strchr(space + 1, ' ');   // optionale Geltungsdauer in Sekunden
   // gleiche Regel wie bei §14a — ohne Angabe die Vorgabedauer, Bereich 0..86400 s.
   // Die Einspeisegrenze wird von derselben Zustandsmaschine geschrieben und erbt dieselbe
   // Gefahr: eine abgelaufene Geltungsdauer der Gegenstelle macht sie lautlos wirkungslos.
  long dauer = eebus_hems_mode ? EEBUS_LPC_DUR_DEFAULT_S : eebus_limit_dur_s;
  if (nullptr != space2) { dauer = atol(space2 + 1); }
  if (dauer < 0 || dauer > EEBUS_LPC_DUR_MAX_S) {
    Response_P(PSTR("{\"%s\":{\"Fehler\":\"Dauer %ld s unzulaessig - erlaubt 0..%ld s (0 = unbefristet)\"}}"),
               XdrvMailbox.command, dauer, EEBUS_LPC_DUR_MAX_S);
    return;
  }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2 (Steuerbox)")); return; }
  if (!EebusSelectPeerArg(sel)) { return; }
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

void CmndEebusMess(void) {
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusMess <idx|ski|hm>")); return; }
  if (!EebusSelectPeerArg(XdrvMailbox.data)) { return; }
  if (SME_DONE != ESp->sme) { ResponseCmndChar_P(PSTR("keine SHIP-Verbindung (done)")); return; }
   // JEDE bekannte Instanz fragen, nicht nur eine. Welche antwortet und welche mit
   // errorNumber 6 ablehnt, steht anschliessend in der Konsole — so finden wir heraus, WO die
   // Gegenstelle welche Groesse fuehrt, statt es zu erraten.
  int n = 0;
  char adr[160] = { 0 }; size_t ap = 0;
  for (int i = 0; i < ESp->m_n; i++) {
    EebusSpineSendAddr("read", false, 0, "{\"measurementDescriptionListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, ESp->m_cli[i], ESp->m_ent[i], ESp->m_feat[i]); n++;
    EebusSpineSendAddr("read", false, 0, "{\"measurementListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, ESp->m_cli[i], ESp->m_ent[i], ESp->m_feat[i]); n++;
    ap += snprintf(adr + ap, sizeof(adr) - ap, "%sM%d/%d", ap ? " " : "", ESp->m_ent[i], ESp->m_feat[i]);
  }
   // nur dort fragen, wo die Selbstauskunft die Kenngroessen auch auffuehrt.
  int ec_gefragt = 0;
  for (int i = 0; i < ESp->ec_n; i++) {
    if (!ESp->ec_char[i]) {
      ap += snprintf(adr + ap, sizeof(adr) - ap, "%s(E%d/%d ohne)", ap ? " " : "",
                     ESp->ec_ent[i], ESp->ec_feat[i]);
      continue;
    }
    EebusSpineSendAddr("read", false, 0, "{\"electricalConnectionCharacteristicListData\":[]}",
                       EEBUS_LPC_CLIENT_ENT, ESp->ec_cli[i], ESp->ec_ent[i], ESp->ec_feat[i]);
    n++; ec_gefragt++;
    ap += snprintf(adr + ap, sizeof(adr) - ap, "%sE%d/%d", ap ? " " : "", ESp->ec_ent[i], ESp->ec_feat[i]);
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
    if (cc->fs_cons_ok) { ResponseAppend_P(PSTR(",\"FsCons\":[%ld,%d]"), cc->fs_cons, cc->fs_cons_sc); }
    if (cc->fs_prod_ok) { ResponseAppend_P(PSTR(",\"FsProd\":[%ld,%d]"), cc->fs_prod, cc->fs_prod_sc); }
    if (cc->fs_dur[0])  { ResponseAppend_P(PSTR(",\"FsDur\":\"%s\""), cc->fs_dur); }
   // die norm-richtigen Nennleistungen, sofern die Gegenstelle sie herausgibt.
    if (cc->pnom_prod_ok) { ResponseAppend_P(PSTR(",\"PnomProd\":[%ld,%d]"), cc->pnom_prod, cc->pnom_prod_sc); }
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
  if (XdrvMailbox.data_len == 0) { ResponseCmndChar_P(PSTR("Nutzung: EEBusLppFrei <idx|ski|hm> [watt]")); return; }
  if (eebus_role < 1) { ResponseCmndChar_P(PSTR("Rolle AUS - erst EEBusRole 1 oder 2")); return; }
  char *space = strchr(XdrvMailbox.data, ' ');
  char sel[48]; size_t sl = (nullptr != space) ? (size_t)(space - XdrvMailbox.data) : strlen(XdrvMailbox.data);
  if (sl >= sizeof(sel)) { sl = sizeof(sel) - 1; }
  memcpy(sel, XdrvMailbox.data, sl); sel[sl] = 0;
  if (!EebusSelectPeerArg(sel)) { return; }
  long w = (nullptr != space) ? atol(space + 1) : (long)ESp->lpc_value;
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
    ESp = &EConn[i];
    if ((SME_DONE == ESp->sme) && ESp->peer_dev[0]) {   // nur verbundene Peers
      if (EebusLpcStart(0, false, 0, 0)) { n++; }   // Not-Freigabe wirkt auf die §14a-Grenze
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

// EEBusConnectIp <ip>:4712 — verbindet zu einer beliebigen Adresse ohne vorherigen Scan.
// Ueberschreibt peers[0] mit der manuellen Adresse und verbindet.
void CmndEebusConnectIp(void) {
  if (XdrvMailbox.data_len < 7) { ResponseCmndChar_P(PSTR("Nutzung: EEBusConnectIp ip:port")); return; }
  if (!Eebus.cert_ok && !EebusEnsureCert(false)) { ResponseCmndChar_P(PSTR("Kein Zertifikat")); return; }
  int ci = EebusConnAlloc("manual");   // Debug-Verbindung in eigenen Slot
  if (ci < 0) { ResponseCmndChar_P(PSTR("alle Slots belegt (EEBusDisconnect)")); return; }
  ESp = &EConn[ci];
  char buf[40]; strlcpy(buf, XdrvMailbox.data, sizeof(buf));
  uint16_t port = 4712;
  char *colon = strchr(buf, ':');
  if (colon) { *colon = '\0'; port = (uint16_t)atoi(colon + 1); }
  EebusPeer *p = &Eebus.peers[0];
  memset(p, 0, sizeof(EebusPeer));
  strlcpy(p->ip, buf, sizeof(p->ip));
  strlcpy(p->path, "/ship/", sizeof(p->path));
  strlcpy(p->ski, "manual", sizeof(p->ski));
  p->port = port;
  if (Eebus.peer_count < 1) { Eebus.peer_count = 1; }
  bool ok = EebusShipConnect(0);
  Response_P(PSTR("{\"%s\":{\"Ok\":%s,\"Ip\":\"%s\",\"Port\":%u,\"Error\":\"%s\"}}"),
             XdrvMailbox.command, ok ? "true":"false", p->ip, p->port, ESp->err);
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
  if (ufsp) { f = ufsp->open(EEBUS_UI_FILE, "r"); }
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
      ESp = &EConn[ci];
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: Web-Trennen von %s"), ESp->peer_ip);
      EebusDisconnectNow();
    }
    return;
  }
  for (uint32_t i = 0; i < Eebus.peer_count; i++) {   // Verbinden: Peer per SKI suchen
    if (0 == strcmp(Eebus.peers[i].ski, eebus_web_ski)) {
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
// (TLS 1.2, ECDHE_ECDSA_AES128_GCM = SHIP-Pflicht-Suite, unser SHIP-Cert/Key).
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
  char     peer_ip[16] = { 0 };
   // Etappe 2: WebSocket-Server + CMI
  uint8_t  ws_state = 0;   // 0=HTTP-Upgrade-Request sammeln, 1=WS aktiv (101 gesendet)
  bool     cmi_done = false;   // CMI [0,0] des Geraets beantwortet?
  uint8_t *abuf = nullptr;   // Ansammelpuffer (HTTP-Request, dann WS-Frames)
  size_t   abuf_len = 0;
   // Etappe 3: SME-Handshake als Server (Geraet = SHIP-Client)
  uint8_t  sme_state = 0;   // 0=warte Hello, 1=warte announceMax, 2=warte select-Spiegel, 3=DONE
   // Etappe 4: Verknuepfung mit einem EConn-Slot (SPINE-Datenphase)
  int      conn_idx = -1;   // Index in EConn[] (-1 = nicht verknuepft)
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
    cc->peer_hb_at = 0; cc->peer_hb_ctr = 0; cc->peer_hb_tmo_s = 0; cc->peer_hb_lost = false;
    cc->lpc_state = LPC_IDLE;
    cc->lpc_write_tries = 0;   // 
    cc->lpc_write_retry_at = 0;   // 
    ESrv.conn_idx = -1;
  }
  if (ESrv.sock) { ESrv.sock.stop(); }
  ESrv.sock = NetworkClient();   // Socket-Handle loslassen (shared)
  if (ESrv.sc)       { free(ESrv.sc);       ESrv.sc = nullptr; }
  if (ESrv.ibuf)     { free(ESrv.ibuf);     ESrv.ibuf = nullptr; }
  if (ESrv.obuf)     { free(ESrv.obuf);     ESrv.obuf = nullptr; }
  if (ESrv.cert_der) { free(ESrv.cert_der); ESrv.cert_der = nullptr; }
  if (ESrv.abuf)     { free(ESrv.abuf);     ESrv.abuf = nullptr; }
  ESrv.cert_len = 0;
  ESrv.active = false;
  ESrv.hs_done = false;
  ESrv.ws_state = 0;
  ESrv.cmi_done = false;
  ESrv.abuf_len = 0;
  ESrv.sme_state = 0;
}

// BearSSL-SERVER-Kontext aufsetzen und Handshake starten. Socket + peer_ip stehen
// schon in ESrv (Globals — .ino-Prototypen-Falle, keine Klassen-Typen in Signaturen).
bool EebusSrvStart(void) {
  const char *why = "";
  ESrv.hs_done = false;
  ESrv.deadline = millis() + EEBUS_SRV_TIMEOUT_MS;

  ESrv.sc   = (br_ssl_server_context*)malloc(sizeof(br_ssl_server_context));
  ESrv.ibuf = (unsigned char*)malloc(EEBUS_SRV_IBUF);
  ESrv.obuf = (unsigned char*)malloc(EEBUS_SRV_OBUF);
  ESrv.cert_der = (uint8_t*)malloc(EEBUS_CERT_DER_SIZE);
  ESrv.abuf = (uint8_t*)special_malloc(EEBUS_SRV_ABUF);   // 32 KB -> PSRAM (DRAM schonen), wird wie bisher per free() freigegeben
  ESrv.abuf_len = 0;
  ESrv.ws_state = 0;
  ESrv.cmi_done = false;
  ESrv.sme_state = 0;
  ESrv.conn_idx = -1;
  if ((nullptr == ESrv.sc) || (nullptr == ESrv.ibuf) || (nullptr == ESrv.abuf) ||
      (nullptr == ESrv.obuf) || (nullptr == ESrv.cert_der)) { why = "malloc"; goto fail; }
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

// Anwendungsdaten in die TLS-Engine schreiben (Server-Sendepfad). Schreibt die
// Nutzlast in den Engine-Klartextpuffer und schiebt die verschluesselten Records
// direkt zum Socket (kleine Nachrichten; bei vollem Socket-Puffer max 3 s warten).
bool EebusSrvAppWrite(const uint8_t *data, size_t len) {
  br_ssl_engine_context *eng = &ESrv.sc->eng;
  size_t off = 0;
  uint32_t t0 = millis();
  while (off < len) {
    unsigned state = br_ssl_engine_current_state(eng);
    if (state & BR_SSL_CLOSED) { return false; }
    if (state & BR_SSL_SENDAPP) {
      size_t alen;
      unsigned char *buf = br_ssl_engine_sendapp_buf(eng, &alen);
      size_t n = (len - off > alen) ? alen : (len - off);
      memcpy(buf, data + off, n);
      br_ssl_engine_sendapp_ack(eng, n);
      off += n;
      br_ssl_engine_flush(eng, 0);   // Record schliessen -> SENDREC
      continue;
    }
    if (state & BR_SSL_SENDREC) {   // Records zum Socket schieben (macht Platz)
      size_t rlen;
      unsigned char *rbuf = br_ssl_engine_sendrec_buf(eng, &rlen);
      int wlen = ESrv.sock.write(rbuf, rlen);
      if (wlen > 0) { br_ssl_engine_sendrec_ack(eng, wlen); continue; }
    }
    if (millis() - t0 > 3000) { return false; }
    delay(1);
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
  if (!EebusSrvAppWrite(hdr, hl)) { return false; }
  return (0 == len) ? true : EebusSrvAppWrite(payload, len);
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
  cc->lc_sub = false; cc->lc_cli_ent = 0; cc->lc_cli_feat = 0;   // 
  cc->hb_sub = false;
  cc->hb_cli_ent = 0;
  cc->hb_cli_feat = 0;
  cc->hb_next = 0;
  cc->hb_counter = 1;
  cc->peer_hb_at = 0; cc->peer_hb_ctr = 0; cc->peer_hb_tmo_s = 0; cc->peer_hb_lost = false;
  cc->spine_ctr = 1;
  cc->lpc_state = LPC_IDLE;
  cc->lpc_bound = false;
  cc->lpc_onboard_only = false;   // 
  cc->lpc_onboarded = false;   // 
  cc->lpc_sel_step = 0;   // 
  cc->lpc_fs_val_key = -1; cc->lpc_fs_dur_key = -1;   // Failsafe-Keys pro Verbindung neu lernen
  cc->lpc_fs_step = 0; cc->lpc_fs_done = false;   // 
  cc->lpc_write_tries = 0;   // 
  cc->lpc_write_retry_at = 0;   // 
  if (cc->lpc_disco) { free(cc->lpc_disco); cc->lpc_disco = nullptr; }   // Discovery-Kopie freigeben
  if (cc->lpc_uc)    { free(cc->lpc_uc);    cc->lpc_uc = nullptr; }   // Use-Case-Kopie freigeben
  if (cc->rd_buf)    { free(cc->rd_buf);    cc->rd_buf = nullptr; }   // Leseantwort freigeben
  cc->rd_ent = -1; cc->rd_feat = -1;
  cc->lpc_limit_id = -1;
  cc->lpc_peer_ent = 1;
  cc->lpc_peer_feat = 6;   // Default; wird aus der Peer-Discovery aktualisiert
  cc->lpc_deadline = 0;
  cc->lpc_our_limit = false;
  cc->lpc_result[0] = '\0';
  cc->keepalive = false;   // Wiederverbinden ist Sache des Geraets
  cc->teardown = false;
  cc->reconnect_at = 0;
  cc->peer_idx = -1;
  cc->last_rx = millis();
  strlcpy(cc->peer_ip, ESrv.peer_ip, sizeof(cc->peer_ip));
  strlcpy(cc->peer_ski, ESrv.peer_ip, sizeof(cc->peer_ski));   // Pseudo-SKI (s.o.)
  ESrv.conn_idx = ci;
  EebusStatSet(cc->peer_ski, SHIP_CMI_OK, "");
  AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s mit SPINE-Slot %d verknuepft — Datenphase aktiv"), ESrv.peer_ip, ci);
}

// Etappe 3: SME-Handshake als SERVER (Geraet = SHIP-Client). Gespiegelt zum
// Client-Pfad (EebusSmeDispatch): Hello beantworten -> announceMax des Geraets mit
// "select" beantworten (Server waehlt Version, SHIP 13.4.4.2) -> Geraet spiegelt
// select -> Pin none + AccessMethods -> Done (Datenphase; SPINE via EebusSrvLinkConn).
// ACHTUNG: kann bei Fehlern EebusSrvFree() rufen — Aufrufer muss danach ESrv.active pruefen!
void EebusSrvSmeHandle(int classifier, const char *json) {
  if (3 == classifier) {   // connectionClose des Geraets
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s hat die SHIP-Verbindung geschlossen"), ESrv.peer_ip);
    EebusSrvFree();
    return;
  }
  if (2 == classifier) {   // SPINE-Datagramm in die Datenphase einspeisen
    if (ESrv.conn_idx >= 0) {
      EebusConn *save = ESp;   // ESp auf den verknuepften Slot umschalten
      ESp = &EConn[ESrv.conn_idx];
      ESp->last_rx = millis();
      EebusShipLog('R', 2, json);   // Server-RX in Konsole + RAM-Mitschnitt (voll)
      EebusSpineHandle(json);   // komplette bestehende SPINE-Maschinerie
      ESp = save;
    }
    return;
  }
  if (1 != classifier) { return; }

  if (strstr(json, "\"connectionHello\"")) {
    char phase[12] = { 0 };
    EebusJsonStr(json, "phase", phase, sizeof(phase));
    if (0 == strcmp(phase, "ready")) {
      if (0 == ESrv.sme_state) {   // unser Hello schicken -> beidseitig ready
        if (EebusSrvShipSendJson("{\"connectionHello\":[{\"phase\":\"ready\"},{\"waiting\":60000}]}")) {
          ESrv.sme_state = 1;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV SME-Hello mit %s beidseitig ready — warte auf ProtocolHandshake"),
                 ESrv.peer_ip);
        }
      }
    } else if (0 == strcmp(phase, "pending")) {
   // Geraet kennt unsere SKI noch nicht -> im Geraete-UI bestaetigen (Pairing)
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s meldet PENDING — unsere SKI %s im Geraete-UI bestaetigen"),
             ESrv.peer_ip, Eebus.own_ski);
    } else {   // aborted o.ae.
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s Hello-Abbruch (%s)"), ESrv.peer_ip, phase);
      EebusSrvFree();
    }
    return;
  }

  if (strstr(json, "\"messageProtocolHandshakeError\"")) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s ProtocolHandshake-Fehler"), ESrv.peer_ip);
    EebusSrvFree();
    return;
  }

  if (strstr(json, "\"messageProtocolHandshake\"")) {
    char ht[16] = { 0 };
    EebusJsonStr(json, "handshakeType", ht, sizeof(ht));
    if ((1 == ESrv.sme_state) && (0 == strcmp(ht, "announceMax"))) {
   // Wir (Server) waehlen: SHIP 1.0, JSON-UTF8 (wie alle bekannten Geraete)
      if (EebusSrvShipSendJson("{\"messageProtocolHandshake\":[{\"handshakeType\":\"select\"},"
                               "{\"version\":[{\"major\":1},{\"minor\":0}]},"
                               "{\"formats\":[{\"format\":[\"JSON-UTF8\"]}]}]}")) {
        ESrv.sme_state = 2;   // Geraet muss unser select spiegeln
      }
    } else if ((2 == ESrv.sme_state) && (0 == strcmp(ht, "select"))) {
   // Geraet bestaetigt die Auswahl -> PinCheck + Kennungs-Austausch -> Datenphase
      EebusSrvShipSendJson("{\"connectionPinState\":[{\"pinState\":\"none\"}]}");
      EebusSrvShipSendJson("{\"accessMethodsRequest\":[]}");
      ESrv.sme_state = 3;
      ESrv.deadline = millis() + EEBUS_SRV_DATA_TIMEOUT_MS;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV SHIP-HANDSHAKE KOMPLETT (Server-Rolle) mit %s — Datenphase erreicht ***"),
             ESrv.peer_ip);
      EebusSrvLinkConn();   // SPINE-Slot verknuepfen -> Datenphase antwortet
    } else {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s ProtocolHandshake unerwartet (Zustand %d, Typ %s)"),
             ESrv.peer_ip, ESrv.sme_state, ht);
    }
    return;
  }

  if (strstr(json, "\"connectionPinState\"")) {
    char pin[12] = { 0 };
    EebusJsonStr(json, "pinState", pin, sizeof(pin));
    if (0 == strcmp(pin, "required")) {   // PIN unterstuetzen wir nicht
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s verlangt PIN — nicht unterstuetzt, Verbindung beendet"), ESrv.peer_ip);
      EebusSrvFree();
    }   // none/optional: ok
    return;
  }

  if (strstr(json, "\"accessMethodsRequest\"")) {   // Geraet fragt UNSERE Kennung ab
    char reply[128];
    snprintf_P(reply, sizeof(reply), PSTR("{\"accessMethods\":[{\"id\":\"Tasmota-EEBusGuard-%s\"}]}"),
               TasmotaGlobal.hostname);
    EebusSrvShipSendJson(reply);
    return;
  }

  if (strstr(json, "\"accessMethods\"")) {   // Kennung des Geraets
    char pid[64] = { 0 };
    EebusJsonStr(json, "id", pid, sizeof(pid));
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s AccessMethods id=%s"), ESrv.peer_ip, pid);
    if (ESrv.conn_idx >= 0) {   // Kennung in den SPINE-Slot uebernehmen
      strlcpy(EConn[ESrv.conn_idx].peer_id, pid, sizeof(EConn[ESrv.conn_idx].peer_id));
    }
    return;
  }
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
    } else if (!fin || (0x0 == opcode)) {
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV %s: fragmentierte WS-Nachricht (%u B) — Etappe 2 verworfen"),
             ESrv.peer_ip, (uint32_t)len);
    } else if (!ESrv.cmi_done && (2 == len) && (0x00 == pl[0])) {
   // SHIP 13.4.3 CMI: Geraet (WS-Client) sendet [0x00,0x00], wir antworten gleich
      uint8_t cmi[2] = { 0x00, 0x00 };
      if (EebusSrvWsSendOp(cmi, 2, 0x2)) {
        ESrv.cmi_done = true;
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV CMI mit %s beantwortet — SHIP-Kanal steht, warte auf SME-Hello ***"),
               ESrv.peer_ip);
      } else {
        AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV CMI-Antwort an %s fehlgeschlagen"), ESrv.peer_ip);
        EebusSrvFree();
        return;
      }
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
      if (!ESrv.active) { return; }   // Handler hat die Verbindung beendet (abuf weg!)
      pl[len] = saved;
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
  if (TimeReached(ESrv.deadline)) {
    AddLog(LOG_LEVEL_INFO, PSTR("EBG: SRV Timeout %s (Handshake %s)"),
           ESrv.peer_ip, ESrv.hs_done ? "fertig, keine Daten mehr" : "unvollstaendig");
    EebusSrvFree();
    return;
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
      int wlen = ESrv.sock.write(buf, len);
      if (wlen > 0) { br_ssl_engine_sendrec_ack(eng, wlen); continue; }
      return;   // Socket-Sendepuffer voll -> naechster Tick
    }
    if (!ESrv.hs_done && (state & (BR_SSL_SENDAPP | BR_SSL_RECVAPP))) {
      ESrv.hs_done = true;
      AddLog(LOG_LEVEL_INFO, PSTR("EBG: *** SRV TLS-SERVER-HANDSHAKE FERTIG mit %s (ECDHE_ECDSA_AES128_GCM) ***"),
             ESrv.peer_ip);
    }
    if (state & BR_SSL_RECVAPP) {   // entschluesselte Daten -> Ansammelpuffer -> Parser
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
      ESrv.deadline = millis() + ((3 == ESrv.sme_state) ? EEBUS_SRV_DATA_TIMEOUT_MS
                                                        : EEBUS_SRV_TIMEOUT_MS);
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

bool Xdrv126(uint32_t function) {
  bool result = false;

  switch (function) {
    case FUNC_EVERY_100_MSECOND:
      EebusInboundAccept();   // SHIP-Server Etappe 1 (TLS-Server-Handshake)
      if (Eebus.scan) { EebusPollScan(); }
      for (int ci = 0; ci < EEBUS_MAX_CONN; ci++) {   // M2b: SME-Handshake + RX je Slot
        ESp = &EConn[ci];
        EebusSmePoll();
      }
      break;
    case FUNC_EVERY_SECOND:
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
   // Vorgemerkte Leseanfragen (EEBusRead) absetzen — bewusst HIER und nicht im Kommando selbst:
   // wird waehrend der Kommandobearbeitung gesendet, kommt der gesendete Rahmen vermischt mit der
   // Kommandoantwort aus /cm heraus (gemessen 29.07.). Die Verzoegerung von bis zu einer Sekunde
   // ist unerheblich, die Antwort wird ohnehin abgeholt.
      for (int rdi = 0; rdi < EEBUS_MAX_CONN; rdi++) {
        if (!EConn[rdi].active || (SME_DONE != EConn[rdi].sme) || (EConn[rdi].rd_pend_ent < 0)) { continue; }
        EebusConn *rd_save = ESp;
        ESp = &EConn[rdi];
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
        ESp = rd_save;
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
          ESp = &EConn[pci];
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
        ESp = &EConn[ci];
        EebusTeardownNow();   // anstehende Client-Freigabe im sicheren Kontext
   // faelligen (verzoegerten/nachgepollten) Read-Back ausloesen. Hier statt in
   // EebusSmePoll, weil DIESER Loop auch die EINGEHENDEN (via_srv) Slots tickt — die
   // Waermepumpen-Gateway verbindet inbound, und EebusSmePoll kehrt fuer via_srv frueh zurueck.
        if (ESp->lpc_verify_at && TimeReached(ESp->lpc_verify_at)) {
          ESp->lpc_verify_at = 0;
          EebusLpcVerify();   // sendet Read-Back, setzt LPC_VERIFY + frische Deadline
        }
   // DeviceConfig-Read-Timeout auch fuer via_srv (Waermepumpen-Gateway) best-effort -> trotzdem schreiben
   // (EebusSmePoll kehrt fuer via_srv frueh zurueck, deckt den READCFG-Timeout dort nicht ab)
        if ((LPC_READCFG == ESp->lpc_state) && ESp->lpc_deadline && TimeReached(ESp->lpc_deadline)) {
          ESp->lpc_deadline = 0;
          EebusLpcSetResult(LPC_READCFG, "DeviceConfig-Read Timeout (via_srv) -> schreibe trotzdem");
          EebusLpcWrite();
        }
   // faelliger err7-Retry -> Heartbeat+Write erneut. EebusLpcWrite() sendet im
   // HEMS-Modus selbst den Heartbeat unmittelbar vor dem Write, daher hier nur den Write erneut anstossen.
        if ((LPC_WRITE == ESp->lpc_state) && ESp->lpc_write_retry_at && TimeReached(ESp->lpc_write_retry_at)) {
          ESp->lpc_write_retry_at = 0;
          EebusLpcWrite();
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
        if (ESp->lpc_reg_step && (ESp->lpc_reg_step < 3) && eebus_auto_reg && eebus_hems_mode &&
            ESp->lpc_onboarded && ESp->hb_sub &&
            ((LPC_IDLE == ESp->lpc_state) || (LPC_DONE == ESp->lpc_state) || (LPC_FAIL == ESp->lpc_state))) {
          uint8_t rdir = (1 == ESp->lpc_reg_step) ? 0 : 1;
          ESp->lpc_reg_step++;
          AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung %s @ %s - Freigabe wird geschrieben (kein Limit)"),
                 rdir ? "Einspeisung (Paragraf 9 EEG)" : "Bezug (Paragraf 14a EnWG)", ESp->peer_ip);
          EebusLpcStart(0, false, rdir, 0);   // Anmeldung: Freigabe, ohne Geltungsdauer
        }
   // WEBSOCKET-PING: alle 50 s ein Ping-Frame, damit die Gegenstelle die Verbindung nicht
   // als tot abraeumt. Wir haben bisher NIE einen Ping gesendet, sondern nur auf fremde geantwortet
   // — die Referenz-Steuerbox pingt alle 50 s und rechnet mit 60 s Pong-Frist (Referenz-Umsetzung ws/types.go).
   // hat der Peer uns verloren und ist in seinen Failsafe gefallen; ein
   // stiller Kanal ist die naheliegendste Ursache. Der Ping kostet zwei Bytes.
        if (SME_DONE == ESp->sme) {
          if (!ESp->ws_ping_next) { ESp->ws_ping_next = millis() + EEBUS_WS_PING_MS; }
          else if (TimeReached(ESp->ws_ping_next)) {
            ESp->ws_ping_next = millis() + EEBUS_WS_PING_MS;
            EebusWsSendOp(nullptr, 0, 0x9);   // Ping; die Gegenstelle antwortet mit Pong
          }
        } else {
          ESp->ws_ping_next = 0;
        }
   // SICHERHEITSNETZ WIEDERAUFBAU: faellt eine Verbindung auf irgendeinem Weg weg, ohne dass
   // dort ein Reconnect eingeplant wurde, holt dieser Zweig es nach. Bisher planten nur ZWEI
   // Abbruchstellen den Wiederaufbau ein — blieb die Verbindung deshalb einfach weg,
   // obwohl keepalive gesetzt war. Lieber einmal zu viel geplant als eine Steuerbox, die still steht.
        if ((SME_OFF == ESp->sme) && ESp->keepalive && !ESp->reconnect_at && (ESp->peer_idx >= 0)) {
          ESp->reconnect_at = millis() + (ESp->sme_pending_logged ? EEBUS_PENDING_RETRY_MS : EEBUS_SPINE_KEEPALIVE_MS);
        }
   // Heartbeat-NOTIFY alle 20 s an den DeviceDiagnosis-Leser/Abonnenten (Verbindungsueberwachung).
   // 20 s (war 30) — muss UNTER Ladestations Ablauf (unser gemeldeter PT30S -> ~31 s) bleiben, sonst
   // faellt heartbeat_received ab und die Ladestation springt trotz gesetztem Limit auf Failsafe 22000.
        if ((SME_DONE == ESp->sme) && ESp->hb_sub && ESp->hb_next && TimeReached(ESp->hb_next)) {
          ESp->hb_next = millis() + (eebus_hems_mode ? 8000 : 20000);   // HEMS 8 s (Vergleichs-Steuerbox-Takt), Ladestation 20 s
          EebusSpineSendHeartbeat("notify", false, 0, ESp->hb_cli_ent, ESp->hb_cli_feat);
          AddLog(LOG_LEVEL_DEBUG, PSTR("EBG: Heartbeat %u an %s gesendet"), ESp->hb_counter - 1, ESp->peer_ip);
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
   // ⚠️ ANMELDUNG NUR ERNEUERN, SOLANGE KEINE EIGENE GRENZE AKTIV IST. Die Anmeldung ist eine
   // FREIGABE (isLimitActive:false) — sie wuerde eine gesetzte Begrenzung aufheben. Geprueft wird
   // beides: der eigene Merker aus dem letzten Schreibauftrag UND der zuletzt zurueckgelesene
   // Zustand beider Grenzen. Im Zweifel wird NICHT erneuert.
            bool grenze_aktiv = ESp->lpc_our_limit || (1 == ESp->lim_act[0]) || (1 == ESp->lim_act[1]);
            if (grenze_aktiv) {
              AddLog(LOG_LEVEL_INFO, PSTR("EBG: Anmeldung wird NICHT erneuert - eine Grenze ist aktiv und darf nicht aufgehoben werden"));
            } else if (eebus_auto_reg && eebus_hems_mode && ESp->lpc_onboarded && ESp->hb_sub) {
              ESp->lpc_reg_step = 1;   // beide Richtungen nacheinander erneut anmelden
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
              (0 != strcmp(Eebus.peers[idx].ski, ESp->peer_ski))) {
            idx = -1;
            for (uint32_t k = 0; k < Eebus.peer_count; k++) {
              if (0 == strcmp(Eebus.peers[k].ski, ESp->peer_ski)) { idx = (int)k; break; }
            }
          }
          if (idx >= 0) {
            AddLog(LOG_LEVEL_INFO, PSTR("EBG: Auto-Reconnect zu Peer %d (Slot %d)"), idx, ci);
            if (!EebusShipConnect(idx) && ESp->keepalive) {
              ESp->reconnect_at = millis() + (ESp->sme_pending_logged ? EEBUS_PENDING_RETRY_MS : EEBUS_SPINE_KEEPALIVE_MS);   // fehlgeschlagen -> spaeter erneut
            }
          }
        }
      }
      break;
    case FUNC_COMMAND:
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
