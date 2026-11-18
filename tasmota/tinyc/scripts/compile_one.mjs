#!/usr/bin/env node
// compile_one.mjs — EIN .tc-Programm nach .tcb uebersetzen.
//
//     node scripts/compile_one.mjs <datei.tc> [ziel.tcb] [-DNAME ...]
//
// -DNAME wie beim IDE-Uebersetzer, z. B. -DBOARD_GOOUUU fuer webcam_tinyc.tc. Eine Board-Angabe hat dort KEINE Vorgabe mehr (18.11.2026).
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

const defs = process.argv.slice(2).filter(a => a.startsWith('-D')).map(a => a.slice(2));
const pos  = process.argv.slice(2).filter(a => !a.startsWith('-D'));
const src = pos[0];
if (!src) { console.error('Aufruf: compile_one.mjs <datei.tc> [ziel.tcb]'); process.exit(2); }
const dst = pos[1] || join('/tmp', basename(src, '.tc') + '.tcb');

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
  const quelle = resolveIncludes(roh, getFile);
  var zeilen = quelle.split('\n');
  const bin = Buffer.from(new Uint8Array(compile(quelle, { defines: defs }).binary));
  writeFileSync(dst, bin);
  console.log(`${src} -> ${dst}  (${bin.length} Byte)` + (defs.length ? `  defines: ${defs.join(' ')}` : ''));
} catch (e) {
  const msg = String(e.message).split('\n')[0];
  console.error(`Fehler: ${msg}`);
  // Eine `#error`-Zeile (z. B. "kein Board genannt") erzeugt nur "Expected 'define' after '#'": die Zeile selbst zeigen, dort steht der Grund.
  const m = msg.match(/at (\d+):/);
  if (m && typeof zeilen !== 'undefined' && /^\s*#\s*error/.test(zeilen[+m[1] - 1] || '')) console.error(`  Zeile ${m[1]}: ${zeilen[+m[1] - 1].trim()}`);
  process.exit(1);
}
