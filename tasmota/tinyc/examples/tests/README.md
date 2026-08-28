# examples/tests/ — Regressionstests, Crash-Repros, Diagnosen

Diese Programme prüfen die **VM und den Compiler**, nicht ein Gerät. Sie
gehören nicht in die Auswahlliste auf der `/tc`-Seite: `crash_test.tc` stürzt
absichtlich ab, `gtls_atmp.tc` reproduziert einen Fehler, `dummy5.tc` ist ein
Platzhalter. Wer die Beispielsammlung durchsieht, sucht ein Programm für seine
Hardware — und findet zwischen `BME280` und `Matter Fan` einen Absturz.

## Wie sie draußen bleiben

Gar nicht durch eine Ausnahmeliste, sondern durch den Ordner. `build.html`
liest `examples/` mit `readTcFiles()`, und das nimmt nur `h.kind === 'file'` —
**keine Unterordner**. Genau so bleibt `examples/common/` seit jeher draußen.
Ein Programm hier landet also weder in `bytecode/`, noch in `index.txt`, noch
in `index.json`, ohne dass irgendwo ein Name gepflegt werden muss.

## ⚠️ Der Preis: hier baut niemand mehr automatisch

Vorher wurden diese Dateien bei jedem Lauf von `build.html` mitübersetzt. Das
war kein Test, aber es hätte auffallen lassen, wenn eine Compileränderung
`test_fnptrs_v1.tc` nicht mehr übersetzt. Dieser Schutz ist weg.

Zum Prüfen einzeln in die IDE laden, oder alle auf einmal aus `tasmota/tinyc/`:

```
node -e '
import("./idesrc/src/compiler.js").then(async c => {
  const p = await import("./idesrc/src/preprocessor.js"), fs = await import("fs");
  const lies = d => Object.fromEntries(fs.readdirSync(d).filter(n=>n.endsWith(".tc"))
    .map(n=>[n, fs.readFileSync(d+"/"+n,"utf8")]));
  const b = lies("examples/common"), t = lies("examples/tests");
  const get = n => b[n.replace(/^.*[\/\\]/,"")] ?? t[n.replace(/^.*[\/\\]/,"")];
  let ok=0, bad=0;
  for (const [n,s] of Object.entries(t)) {
    try { c.compile(p.resolveIncludes(s,get)); ok++; }
    catch(e){ bad++; console.log("FEHLER "+n+": "+e.message); }
  }
  console.log(ok+" ok, "+bad+" fehlgeschlagen");
});'
```

## Was hier NICHT hingehört

Diagnosen, die echte Hardware brauchen und dabei etwas Nützliches tun, sind in
`examples/` geblieben — `wav_diag.tc`, `epd_compare_test.tc`, `esf37_probe.tc`
und ähnliche. Die Grenze ist: prüft es die VM, oder prüft es ein Gerät?
