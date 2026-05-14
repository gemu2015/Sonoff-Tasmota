# TinyC IDE source

Canonical sources for the browser-based TinyC IDE that's served from the
device at `http://<tasmota>/ide`. The IDE bundles into a single
`tinyc_ide.html.gz` (~163-167 KB compressed, ~740-825 KB unzipped) that
sits at `../tinyc_ide.html.gz` (one level up — alongside the rest of the
TinyC tree at `tasmota/tinyc/`).

## Layout

```
idesrc/
├── README.md            ← this file
├── bundle.py            ← inline modules + examples → tinyc_ide.html(.gz)
├── index.html           ← HTML/CSS/inline JS template (UI shell + glue)
└── src/                 ← compiler + VM as ES modules
    ├── preprocessor.js  ← #include / #define / #ifdef resolution
    ├── opcodes.js       ← Op + Syscall enums; CALLBACK_NAMES; BUILTINS
    ├── lexer.js
    ├── parser.js
    ├── codegen.js       ← AST → bytecode; this is where new syscalls
    │                       get registered as IDE-side BUILTINS entries
    ├── vm.js            ← in-browser simulator (for `Run in Browser`)
    └── compiler.js      ← top-level entry point glueing the above
```

## Build

```bash
cd tasmota/tinyc/idesrc
python3 bundle.py
```

Outputs `tinyc_ide.html` (debuggable) and `tinyc_ide.html.gz` (what the
device serves), and **auto-copies the .gz to `../tinyc_ide.html.gz`** so
the next `git add` picks up the new IDE for device deploys.

The bundler strips `import`/`export` statements from each ES module
file and inlines them into a single `<script>` block inside the HTML
template. Examples are read from `../examples/*.tc` and inlined as
template literals; the per-example dropdown list is hard-coded at the
top of `bundle.py` (the `EXAMPLES` array).

---

## Drift status (2026-05-14)

| Component | Status |
|---|---|
| `src/preprocessor.js` | ✅ Synced to live |
| `src/opcodes.js`      | ✅ Synced to live (335 Syscall entries, TINYC_RELEASE = 1.3.20) |
| `src/lexer.js`        | ✅ Synced to live |
| `src/parser.js`       | ✅ Synced to live |
| `src/codegen.js`      | ✅ Synced to live (305 BUILTINS entries — all recent patches included) |
| `src/vm.js`           | ✅ Synced to live |
| `src/compiler.js`     | ✅ Synced to live |
| `index.html`          | ⚠️ **STALE** — Apr-23 state, missing post-Apr HTML template changes |

**`src/*.js` modules are now consistent with the live
`../tinyc_ide.html.gz`** — extracted from the bundled HTML and
re-wrapped with their original ES-module `import`/`export` structure.
All recent BUILTINS additions are present: `shareDump`, `udp(10)`,
11 `ui*` widgets, 3 image↔cam-bridge functions, crypto, TWAI, string
ops, TCP tuning, `webRawMode/Write/KeepAlive`, `bcall`, etc.

**`index.html` is still on the Apr-23 state.** Missing template-side
features that live in the deployed IDE:
- Multi-file upload (`multiple` attr on `#fileInput`)
- "Save .tcb" button + `saveTcb()` handler (Ctrl+Shift+S)
- "Target Slot" dropdown (`#targetSlot`)
- `#include "file.tc"` preprocessor directive UX
- IDE `<h1>` label (currently shows 1.3.11 vs live 1.6.5)
- Visible device-IP field, smlRestart no-cors

Net: re-running `bundle.py` right now produces an IDE that:
- Has all the right COMPILER behavior (modules are current)
- Lacks the post-Apr HTML/UI changes (template is stale)

⚠️ **DO NOT re-bundle to overwrite `../tinyc_ide.html.gz` yet** —
the bundled output would lose the HTML-template patches. Continue
to maintain `../tinyc_ide.html.gz` as the source of truth and apply
new patches via `../patch_*.mjs` scripts targeting the gzipped HTML,
**OR** apply changes to both `src/*.js` AND `../tinyc_ide.html.gz`
simultaneously for cleaner bookkeeping.

**Pending work:**
1. Reconstruct `index.html` from the live `../tinyc_ide.html.gz` (split
   out the bundled JS block, recover the original HTML template +
   `<script type="module">import` line + EXAMPLES placeholders).
2. Verify `bundle.py` from the in-tree sources produces a byte-
   equivalent (or functionally equivalent) `tinyc_ide.html.gz` to live.
3. Switch the patch workflow over: future changes edit `src/*.js` /
   `index.html` directly, re-bundle, commit both sources + bundle.

## History

This folder used to live at `/Volumes/vp_dev/TinyC/` (an external dev
volume separate from the Tasmota repo). It moved into the Tasmota tree
on 2026-05-14 so that:

- The canonical IDE sources are version-controlled alongside the
  rest of `tasmota/tinyc/`.
- Future syscall additions can edit `src/*.js` directly and re-bundle,
  instead of accumulating in `patch_*.mjs` scripts that target the
  gzipped HTML.
- New contributors can see the full IDE source without needing access
  to the external dev volume.
