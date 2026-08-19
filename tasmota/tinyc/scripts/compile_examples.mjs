#!/usr/bin/env node
// compile_examples.mjs — compile every examples/*.tc to bytecode/<name>.tcb and
// regenerate bytecode/index.txt (the repo download list the device fetches).
//
// Standalone programs compile and get a .tcb + an index entry. The #include
// building blocks live in examples/common/ and are not scanned at all — they
// have no main() and must not appear in the download list.
//
// Run from anywhere:  node tasmota/tinyc/scripts/compile_examples.mjs
import { readFileSync, writeFileSync, readdirSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join, basename } from 'path';
import { compile } from '../idesrc/src/compiler.js';
import { resolveIncludes } from '../idesrc/src/preprocessor.js';

const here  = dirname(fileURLToPath(import.meta.url));
const root  = join(here, '..');                 // tasmota/tinyc
const exDir = join(root, 'examples');
const bcDir = join(root, 'bytecode');

// #include "foo.tc" is resolved next to the programs first, then in
// examples/common/ — the include-only building blocks live there (ottelo's
// layout: programs on top, common/ underneath). Bare filename, like the IDE.
const getFile = (name) => {
  const bare = name.replace(/^.*[\/\\]/, '');
  for (const p of [join(exDir, bare), join(exDir, 'common', bare)]) {
    try { return readFileSync(p, 'utf-8'); } catch { /* keep looking */ }
  }
  throw new Error(`include "${name}" not found in examples/ or examples/common/`);
};

const tcs = readdirSync(exDir).filter(f => f.endsWith('.tc')).sort();
const ok = [], failed = [], changed = [], created = [];
for (const f of tcs) {
  const name = basename(f, '.tc');
  const out  = join(bcDir, name + '.tcb');
  let prev = null;
  try { prev = readFileSync(out); } catch {}
  try {
    const raw = readFileSync(join(exDir, f), 'utf-8');
    const src = resolveIncludes(raw, getFile);
    const bin = Buffer.from(new Uint8Array(compile(src, { defines: [] }).binary));
    writeFileSync(out, bin);
    ok.push(name);
    if (prev === null) created.push(name);
    else if (!prev.equals(bin)) changed.push(name);
  } catch (e) {
    failed.push([name, String(e.message).split('\n')[0].slice(0, 80)]);
  }
}

const idx = ok.map(n => n + '.tcb').sort();
writeFileSync(join(bcDir, 'index.txt'), idx.join('\n') + '\n');

console.log(`examples ${tcs.length} | compiled ${ok.length} | new ${created.length} | changed ${changed.length} | skipped ${failed.length}`);
console.log(`index.txt regenerated: ${idx.length} entries`);
if (created.length) console.log('NEW .tcb:', created.join(', '));
if (failed.length) {
  console.log('SKIPPED (not standalone — #include lib / no main):');
  for (const [n, e] of failed) console.log(`  ${n}: ${e}`);
}
