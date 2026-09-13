#!/usr/bin/env node
// compile_examples.mjs — compile every examples/*.tc to bytecode/<name>.tcb and
// regenerate bytecode/index.txt and index.json (the repo download list the device
// fetches; the .json carries the plain name and the info link as well).
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

// `// @name:` / `// @info:` out of the UNRESOLVED source. After the #includes
// are substituted a line from examples/common/ could get in the way.
const pragma = (src, key) => {
  const m = src.match(new RegExp('^[ \t]*//[ \t]*@' + key + ':[ \t]*(.+)$', 'm'));
  return m ? m[1].trim() : '';
};

const tcs = readdirSync(exDir).filter(f => f.endsWith('.tc')).sort();
const ok = [], failed = [], changed = [], created = [], meta = [];
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
    meta.push({ file: name + '.tcb', name: pragma(raw, 'name'), info: pragma(raw, 'info') });
    if (prev === null) created.push(name);
    else if (!prev.equals(bin)) changed.push(name);
  } catch (e) {
    failed.push([name, String(e.message).split('\n')[0].slice(0, 80)]);
  }
}

const idx = ok.map(n => n + '.tcb').sort();
writeFileSync(join(bcDir, 'index.txt'), idx.join('\n') + '\n');

// index.json beside it: the same list plus plain name and info URL.
// ⚠️ BOTH files, always — the repository box reads index.json and falls back to
// index.txt, so a firmware that does not know index.json yet still needs the old
// one. Empty fields are LEFT OUT rather than written as "": 210 entries times
// two empty keys is ~5 KB of air in a file the browser fetches every time the
// /tc page opens.
// ⚠️ The browser path (build.html) writes this file too, and it must produce the
// same bytes — a build from the command line that quietly dropped the plain
// names would show the device a list of file names.
const eintraege = meta.sort((x, y) => x.file.localeCompare(y.file)).map(m => {
  const o = { file: m.file };
  if (m.name) o.name = m.name;
  if (m.info) o.info = m.info;
  return o;
});
writeFileSync(join(bcDir, 'index.json'),
              JSON.stringify({ programs: eintraege }, null, 0) + '\n');

console.log(`examples ${tcs.length} | compiled ${ok.length} | new ${created.length} | changed ${changed.length} | skipped ${failed.length}`);
console.log(`index.txt regenerated: ${idx.length} entries`);
console.log(`index.json regenerated: ${eintraege.length} entries, `
            + `${eintraege.filter(e => e.name).length} with a plain name`);
if (created.length) console.log('NEW .tcb:', created.join(', '));
if (failed.length) {
  console.log('SKIPPED (not standalone — #include lib / no main):');
  for (const [n, e] of failed) console.log(`  ${n}: ${e}`);
}
