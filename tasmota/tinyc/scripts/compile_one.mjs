#!/usr/bin/env node
// compile_one.mjs — EIN .tc-Programm nach .tcb uebersetzen.
//
//     node scripts/compile_one.mjs <datei.tc> [ziel.tcb]
//
// ⚠️ WOZU NOCH EINES. `compile_examples.mjs` uebersetzt den ganzen Ordner und
// schreibt nach bytecode/ -- fuer „schnell aufs Geraet schieben" ist das zu
// viel. `push_tcb.sh` rief dafuer `scripts/compile_cli.js` auf; die Datei liegt
// laengst in legacy_misc/, das Skript war also kaputt (04.09.2026).
//
// Uebersetzt wird mit DEMSELBEN Compiler wie die Sammelstrecke -- ein zweiter
// waere ein zweiter Stand, und der faellt erst auf dem Geraet auf.
import { readFileSync, writeFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join, basename, resolve } from 'path';
import { compile } from '../idesrc/src/compiler.js';
import { resolveIncludes } from '../idesrc/src/preprocessor.js';

const here  = dirname(fileURLToPath(import.meta.url));
const root  = join(here, '..');                 // tasmota/tinyc
const exDir = join(root, 'examples');

const src = process.argv[2];
if (!src) { console.error('Aufruf: compile_one.mjs <datei.tc> [ziel.tcb]'); process.exit(2); }
const dst = process.argv[3] || join('/tmp', basename(src, '.tc') + '.tcb');

// #include zuerst neben dem Programm, dann in examples/, dann examples/common/ --
// dieselbe Reihenfolge wie in der Sammelstrecke.
const getFile = (name) => {
  const bare = name.replace(/^.*[\/\\]/, '');
  for (const p of [join(dirname(resolve(src)), bare), join(exDir, bare), join(exDir, 'common', bare)]) {
    try { return readFileSync(p, 'utf-8'); } catch { /* weitersuchen */ }
  }
  throw new Error(`include "${name}" nicht gefunden`);
};

try {
  const roh = readFileSync(src, 'utf-8');
  const bin = Buffer.from(new Uint8Array(compile(resolveIncludes(roh, getFile), { defines: [] }).binary));
  writeFileSync(dst, bin);
  console.log(`${src} -> ${dst}  (${bin.length} Byte)`);
} catch (e) {
  console.error(`Fehler: ${String(e.message).split('\n')[0]}`);
  process.exit(1);
}
