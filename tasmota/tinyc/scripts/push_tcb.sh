#!/bin/bash
# push_tcb.sh — ein .tc-Programm uebersetzen und auf ein Tasmota-Geraet laden
#
#     scripts/push_tcb.sh <datei.tc> [geraet-ip] [slot]
#
# ⚠️ DAS SKRIPT WAR KAPUTT. Es rief `scripts/compile_cli.js` auf; die Datei
# liegt seit einer Umraeumung in legacy_misc/. Node brach mit MODULE_NOT_FOUND
# ab, und weil `set -e` gilt, endete es dort -- ohne verstaendliche Meldung
# (04.09.2026). Uebersetzt wird jetzt mit compile_one.mjs, also mit demselben
# Compiler wie die Sammelstrecke.
#
# ⚠️ NACH DEM HOCHLADEN ERST ENTLADEN, DANN STARTEN. `/tc_api?cmd=run` laedt
# die Datei nur nach, wenn der Slot NICHT geladen ist -- ein blosses stop+run
# startete das ALTE Programm aus dem Speicher neu, und die frisch geschobene
# Datei laege unbenutzt im Dateisystem.
set -o errexit -o pipefail

EINGABE="${1:?Aufruf: push_tcb.sh <datei.tc> [geraet-ip] [slot]}"
GERAET="${2:-192.168.188.128}"
SLOT="${3:-0}"
NAME="$(basename "$EINGABE" .tc)"
TCB="/tmp/${NAME}.tcb"
HIER="$(cd "$(dirname "$0")" && pwd)"

echo "1/5  uebersetzen …"
node "$HIER/compile_one.mjs" "$EINGABE" "$TCB"

GROESSE=$(stat -f%z "$TCB" 2>/dev/null || stat -c%s "$TCB" 2>/dev/null)
echo "2/5  hochladen: ${NAME}.tcb (${GROESSE} Byte) -> ${GERAET}"
CODE=$(curl -s -o /dev/null -w "%{http_code}" \
    -X POST "http://${GERAET}/ufsu?fsz=${GROESSE}" \
    -F "ufsu=@${TCB};filename=${NAME}.tcb")
[ "$CODE" = "200" ] || { echo "   Hochladen fehlgeschlagen (HTTP $CODE)"; exit 1; }

echo "3/5  Slot ${SLOT} entladen …"
curl -s "http://${GERAET}/cm?cmnd=TinyCUnload%20${SLOT}" ; echo

# ⚠️ OHNE `load` STARTET EIN FRISCHER SLOT NICHT. `/tc_api?cmd=run` laedt nur
# nach, wenn der Slot schon einen gemerkten Dateinamen hat (slot_config) --
# den bekommt er erst durch `load`. Slot 0/1 hatten ihn aus der IDE, ein
# leerer Slot 2 antwortete "no program loaded" (05.09.2026). Genau diesen
# Schritt hatte ich am 04.09. als ueberfluessig gestrichen.
echo "4/5  laden …"
curl -s "http://${GERAET}/tc?cmd=load&file=/${NAME}.tcb&slot=${SLOT}" -o /dev/null

echo "5/5  starten …"
curl -s "http://${GERAET}/tc_api?cmd=run&slot=${SLOT}" ; echo

echo
echo "Zustand:"
curl -s "http://${GERAET}/cm?cmnd=TinyC"
echo
