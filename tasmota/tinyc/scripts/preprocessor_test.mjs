#!/usr/bin/env node
// tasmota/tinyc/scripts/preprocessor_test.mjs
//
//     node tasmota/tinyc/scripts/preprocessor_test.mjs
//
// ⚠️ WARUM DAS GEPRUEFT GEHOERT. Der Praeprozessor entscheidet, WELCHER Code
// ueberhaupt uebersetzt wird. Faellt er falsch, ist das Skript selbst tadellos
// und trotzdem falsch gebaut — man sucht den Fehler dann an der falschen
// Stelle. Genau das ist am 06.09.2026 passiert.
import { preprocess } from '../idesrc/src/preprocessor.js';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HIER = dirname(fileURLToPath(import.meta.url));
let gut = 0, schlecht = 0;
const ist = (name, hat, soll) => {
    if (hat === soll) { gut++; return; }
    schlecht++;
    console.log(`  ⚠ ${name.padEnd(52)} bekommen ${JSON.stringify(hat)}, erwartet ${JSON.stringify(soll)}`);
};

const quelle = `// @defines: -DBOARD_DFROBOT
#ifdef BOARD_DFROBOT
#define HAS_NIGHT
int pins[] = {1};
#endif
#ifdef BOARD_GOOUUU
int pins[] = {2};
#endif
#ifdef HAS_NIGHT
NACHT
#else
KEINE_NACHT
#endif
`;
const drin = (d, was) => preprocess(quelle, d).includes(was);

console.log('── @defines ist eine VORGABE, kein Zusatz ──');
// ⚠️⚠️ DER FALL, DER SCHIEFGING: `webcam_tinyc.tc` traegt
// `@defines: -DBOARD_DFROBOT`. Wer mit `-DBOARD_GOOUUU` uebersetzte, bekam
// VORHER beide Boards — der DFRobot-Block lief mit, HAS_NIGHT entstand, und
// `campins` wurde zweimal angelegt (gemu: „mit BOARD_GOOUUU sollte HAS_NIGHT
// nicht definiert sein, ist es aber").
ist('ohne -D greift das Pragma',            drin([], 'int pins[] = {1};'), true);
ist('… und damit auch HAS_NIGHT',           drin([], 'NACHT'), true);
ist('mit -DBOARD_GOOUUU: das andere Board', drin(['BOARD_GOOUUU'], 'int pins[] = {2};'), true);
ist('… NICHT das aus dem Pragma',           drin(['BOARD_GOOUUU'], 'int pins[] = {1};'), false);
ist('… und KEIN HAS_NIGHT',                 drin(['BOARD_GOOUUU'], 'KEINE_NACHT'), true);
ist('mit einem dritten Board auch nicht',   drin(['BOARD_AITHINKER'], 'int pins[] = {1};'), false);

console.log('── der Vektor des Aufrufers bleibt unberuehrt ──');
// ⚠️ Vorher wurde in `predefined` hineingeschrieben. Wer zweimal uebersetzte,
// hatte beim zweiten Mal alles doppelt drin.
{
    const d = ['BOARD_GOOUUU'];
    preprocess(quelle, d);
    preprocess(quelle, d);
    ist('nach zwei Laeufen unveraendert', d.join(','), 'BOARD_GOOUUU');
}

console.log('── am echten Webcam-Skript ──');
{
    const src = readFileSync(join(HIER, '../examples/webcam_tinyc.tc'), 'utf8');
    const zaehle = (b, muster) => (preprocess(src, [b]).match(muster) || []).length;
    ist('DFROBOT: eine campins-Zeile',  zaehle('BOARD_DFROBOT', /int campins\[\]/g), 1);
    ist('GOOUUU: eine campins-Zeile',   zaehle('BOARD_GOOUUU',  /int campins\[\]/g), 1);
    ist('DFROBOT hat Nachtsicht',       /LUX_LEN/.test(preprocess(src, ['BOARD_DFROBOT'])), true);
    ist('GOOUUU hat sie NICHT',         /LUX_LEN/.test(preprocess(src, ['BOARD_GOOUUU'])), false);
    ist('AITHINKER hat sie NICHT',      /LUX_LEN/.test(preprocess(src, ['BOARD_AITHINKER'])), false);
}

console.log(`\n${gut} gut, ${schlecht} schlecht`);
process.exit(schlecht === 0 ? 0 : 1);
