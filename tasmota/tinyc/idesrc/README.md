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
| `src/opcodes.js`      | ✅ Synced to live (Syscall table + `VERSION`/`SYSCALL_ABI` — the compat-decisive pair the IDE shows vs the device. `TINYC_RELEASE` is an internal opcodes-changelog tag, NOT the displayed release: the version banner is auto-injected from the firmware's `TC_RELEASE` by `bundle.py`.) |
| `src/lexer.js`        | ✅ Synced to live |
| `src/parser.js`       | ✅ Synced to live |
| `src/codegen.js`      | ✅ Synced to live (305 BUILTINS entries — all recent patches included) |
| `src/vm.js`           | ✅ Synced to live |
| `src/compiler.js`     | ✅ Synced to live |
| `index.html`          | ✅ Synced to live (H1 = v1.6.5, Save .tcb button, Target Slot dropdown, multi-file upload, `#include` UX, visible device-IP, smlRestart no-cors — all in) |

**Full backport complete.** `python3 bundle.py` from this folder now
reproduces a `tinyc_ide.html.gz` that is functionally equivalent to
the deployed `../tinyc_ide.html.gz` — the only diff (~77 lines)
comes from updated example sources in `../examples/*.tc` getting
inlined as template literals (e.g. the post-fix `sensor_read.tc`,
the bridge-function-using `snap_with_timestamp.tc`).

## Going-forward workflow

For new syscalls / IDE changes, edit the in-tree sources directly:

  1. New syscall → register in firmware (`xdrv_124_tinyc_vm.h`)
  2. Add `Syscall` enum entry in `src/opcodes.js`
  3. Add `BUILTINS` entry in `src/codegen.js`
  4. (If the syscall needs a custom compile path — like multi-arg
     `sprintf` or `udp(N, …)` — extend `compileCallExpr` in
     `src/codegen.js`)
  5. `python3 bundle.py` → auto-copies `tinyc_ide.html.gz` to `../`
  6. `git add ../tinyc_ide.html.gz src/{opcodes,codegen}.js`

The `../patch_*.mjs` scripts that used to live under `tasmota/tinyc/`
have been removed — they were idempotent rewrites of the gzipped
HTML to compensate for the stale idesrc/. With idesrc/ now in sync,
all those patches are present in `src/*.js` directly and the scripts
are obsolete. To inspect what each patch did, see the matching
commit message in `git log -- tasmota/tinyc/` (commits referenced in
each patch's docstring).

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
