#!/usr/bin/env node
// check_fusion.mjs — eine Verschmelzung muss GENAU dasselbe rechnen wie die
// Opcodes, die sie ersetzt. Von überall aufrufbar:
//
//     node tasmota/tinyc/scripts/check_fusion.mjs
//
// ⚠️ WARUM. Eine Superinstruktion ist eine reine Verschmelzung: sie bringt KEINE
// neue Ausdruckskraft, die unverschmolzene Form bleibt gültig, und ein Programm
// muss beides Mal dasselbe bedeuten. Genau das ist aber nicht selbstverständlich
// — sie wird an vier Stellen gebaut (Emitter in codegen.js, Opcode-Tabelle,
// JS-VM, Firmware-Interpreter), und jede kann für sich falsch sein. Ein Fehler
// darin fällt nicht beim Übersetzen auf, sondern erst, wenn ein Programm im Feld
// eine andere Zahl ausrechnet als im Probelauf.
//
// Geprüft wird, indem DASSELBE Programm zweimal übersetzt wird — einmal für ein
// älteres ABI (das die Verschmelzung nicht kennt, also den langen Weg nimmt) und
// einmal für das heutige — und beide in der JS-VM laufen. Ergebnis und
// Fehlermeldung müssen übereinstimmen.
//
// ⚠️ DIE FEHLERSTELLE (PC) WIRD DABEI AUSGEBLENDET. Eine verschmolzene
// Instruktion steht naturgemäß an einer anderen Adresse als die sechs, die sie
// ersetzt; verglichen wird der Wert oder die Fehlerart.
//
// ⚠️ DIESE PRÜFUNG DECKT DIE FIRMWARE NICHT AB. Sie vergleicht zwei Wege
// innerhalb der JS-VM. Dass der C-Interpreter dasselbe tut, muss am Gerät
// nachgemessen werden — die JS-VM ist das Modell, nicht der Beweis.
//
// Gefunden hat sie am 03.09.2026 auf Anhieb zwei Diagnosemängel, die ÄLTER waren
// als der neue Opcode und alle Verschmelzungen seit dem 08.08. betrafen: der
// Superinstruktionspfad gab die Fehlerstelle nicht mit (`PC=undefined`) und
// meldete `% 0` als „Division by zero" statt „Modulo by zero".

import { readFileSync, readdirSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const HIER = dirname(fileURLToPath(import.meta.url));
const WURZEL = join(HIER, '..');
const { compile } = await import(join(WURZEL, 'idesrc/src/compiler.js'));
const { VM } = await import(join(WURZEL, 'idesrc/src/vm.js'));

// ⚠️ Das ABI, das die zu prüfende Verschmelzung EINFÜHRT, und eines davor.
// Wer eine neue hinzufügt, trägt sie hier ein — sonst prüft dieses Werkzeug
// weiter nur die alten und schweigt zur neuen.
const FUSIONEN = [
    { name: 'x = y OP (z OP k)   (LLK_OP2_ST, ABI 29)', ohne: 28, mit: 29 },
    { name: 'x = y OP z / y OP k (LK/LL_OP_ST, ABI 22)', ohne: 21, mit: 22 },
];

const OPS = ['+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>'];
const KONST = [1, 2, 3, 7, -1, -3, 127, -128];
const START = [[0, 0], [7, 3], [-9, 4], [123456, 17],
               [-2147483648, 3], [2147483647, 5], [5, -7]];

function laufen(erg) {
    const vm = new VM();
    try {
        vm.load(erg);
        return String(vm.run());
    } catch (e) {
        // Die Adresse ausblenden -- siehe Kopf.
        return 'E:' + String(e.message || e).replace(/PC=[0-9]+/, 'PC=*');
    }
}

function pruefe(fusion, quellen) {
    let faelle = 0, abweichungen = 0;
    for (const src of quellen) {
        let a, b;
        try { a = laufen(compile(src, { defines: [], targetAbi: fusion.ohne })); }
        catch (e) { a = 'C:' + e.message; }
        try { b = laufen(compile(src, { defines: [], targetAbi: fusion.mit })); }
        catch (e) { b = 'C:' + e.message; }
        faelle++;
        if (a !== b) {
            if (abweichungen < 5) {
                console.log('  ⚠ unverschmolzen=%s  verschmolzen=%s', a, b);
                console.log('     %s', src.replace(/\n/g, ' ').replace(/\s+/g, ' ').slice(0, 110));
            }
            abweichungen++;
        }
    }
    return [faelle, abweichungen];
}

// ── 1) Die geschachtelte Gestalt, erschöpfend ───────────────────────────────
const geschachtelt = [];
for (const o1 of OPS) for (const o2 of OPS) for (const k of KONST) for (const [y, z] of START) {
    if ((o2 === '/' || o2 === '%') && k === 0) continue;
    geschachtelt.push(
        `int f() { int y = ${y}; int z = ${z}; int x = 0; x = y ${o1} (z ${o2} ${k}); return x; }\n` +
        `int main(){ return f(); }`);
}

// ── 2) Die flache Gestalt, damit die alten Verschmelzungen mitgeprüft werden ─
const flach = [];
for (const o of OPS) for (const k of KONST) for (const [y, z] of START) {
    if ((o === '/' || o === '%') && k === 0) continue;
    flach.push(`int f() { int y = ${y}; int z = ${z}; int x = 0; x = y ${o} ${k}; return x; }\n` +
               `int main(){ return f(); }`);
    flach.push(`int f() { int y = ${y}; int z = ${z}; int x = 0; x = y ${o} z; return x; }\n` +
               `int main(){ return f(); }`);
}

let fehler = 0;
for (const f of FUSIONEN) {
    const quellen = f.mit === 29 ? geschachtelt : flach;
    const [n, ab] = pruefe(f, quellen);
    console.log('  %s  %d Fälle, %d Abweichung(en)', f.name.padEnd(44), n, ab);
    fehler += ab;
}

// ── 3) Der ganze Beispielbestand muss weiter übersetzen ─────────────────────
//
// ⚠️ Kein Ausführen: die meisten Beispiele reden mit Geräten, die es hier nicht
// gibt. Geprüft wird, dass die neue Verschmelzung keinen Übersetzungsfehler
// einführt und dass sie das Bytecode nie GRÖSSER macht.
let n = 0, neu_kaputt = 0, kleiner = 0, groesser = 0;
for (const ort of ['examples', 'examples/tests']) {
    let liste;
    try { liste = readdirSync(join(WURZEL, ort)); } catch { continue; }
    for (const d of liste.filter(x => x.endsWith('.tc'))) {
        const src = readFileSync(join(WURZEL, ort, d), 'utf-8');
        let a;
        try { a = compile(src, { defines: [], targetAbi: 28 }); } catch { continue; }
        n++;
        try {
            const b = compile(src, { defines: [], targetAbi: 29 });
            if (b.binary.length < a.binary.length) kleiner++;
            else if (b.binary.length > a.binary.length) { groesser++; console.log('  ⚠ wird GRÖSSER:', d); }
        } catch (e) {
            neu_kaputt++;
            console.log('  ⚠ übersetzt nur noch ohne die Verschmelzung:', d, e.message);
        }
    }
}
console.log('  %s  %d Beispiele, %d neu kaputt, %d kleiner, %d größer',
            'Beispielbestand'.padEnd(44), n, neu_kaputt, kleiner, groesser);
fehler += neu_kaputt + groesser;

// ── 4) Die FÜNF Stellen der Firmware, nicht vier ────────────────────────────
//
// ⚠️ Ein neuer Opcode braucht in der Firmware ZWEI Implementierungen. `tc_vm_step()`
// hat ein `switch` über alle Opcodes, aber das führt nur EINZELSCHRITTE aus;
// Programme laufen über `tc_vm_run_slice_ex()`, das per `_dispatch[]` zu
// Sprungmarken springt. Wer nur das `switch` bedient, bekommt eine Firmware, die
// übersetzt, linkt und bootet — und beim ersten Erreichen der Schleife
// „Unknown opcode" meldet. Genau so am 03.09.2026 auf .39 passiert, nachdem das
// `switch` allein vollständig ausgesehen hatte.
const kopf = readFileSync(join(WURZEL, '..', 'include', 'xdrv_124_tinyc_vm.h'), 'utf-8');
const imSwitch = new Set([...kopf.matchAll(/case (OP_[A-Z0-9_]+):/g)].map(m => m[1]));
const nummern = new Map([...kopf.matchAll(/(OP_[A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+)/g)]
                        .map(m => [m[1], parseInt(m[2], 16)]));
const inTabelle = new Set([...kopf.matchAll(/_dispatch\[(0x[0-9A-Fa-f]+)\]/g)]
                          .map(m => parseInt(m[1], 16)));
let luecken = 0;
for (const [name, nr] of nummern) {
    // Nur die Superinstruktionen prüfen: darunter gibt es Opcodes, die der
    // schnelle Weg absichtlich anders behandelt.
    if (nr < 0xB0) continue;
    const hatSwitch = imSwitch.has(name);
    const hatMarke = inTabelle.has(nr);
    if (hatSwitch && !hatMarke) {
        console.log('  ⚠ %s (0x%s) steht im switch, aber NICHT in _dispatch[] —',
                    name, nr.toString(16), '');
        console.log('     die Firmware würde „Unknown opcode" melden, sobald ein Programm ihn erreicht');
        luecken++;
    } else if (hatMarke && !hatSwitch) {
        console.log('  ⚠ %s (0x%s) steht in _dispatch[], aber nicht im switch von tc_vm_step()',
                    name, nr.toString(16));
        luecken++;
    }
}
console.log('  %s  %d Superinstruktionen, %d Lücke(n)',
            'Firmware: switch und Sprungtabelle'.padEnd(44),
            [...nummern].filter(([, nr]) => nr >= 0xB0).length, luecken);
fehler += luecken;

if (fehler === 0) {
    console.log('✅ jede Verschmelzung rechnet wie die Opcodes, die sie ersetzt');
    process.exit(0);
}
console.log('\n⚠ %d Abweichung(en)', fehler);
process.exit(1);
