# tc2plugin — TinyC → dual-format plugin C++ (proof of concept)

A local Mac web app: edit TinyC on the left, hit **Translate**, see the
generated Tasmota dual-format plugin C++ (`xsns_*_dual.cpp` shape, the
format consumed by `xdrv_123_plugins`) on the right.

## Run

Double-click **`TC2Plugin.command`** (or `python3 tc2plugin_app.py`).
It serves `http://127.0.0.1:8771/` and opens your browser. Stdlib only —
no `pip install`. Ctrl-C in the terminal to stop.

Headless test: `python3 tc2plugin_app.py --cli < some.tc`

## What it proves — and what it does NOT

This is a **proof of concept**, deliberately scoped (see the feasibility
note: a full any-driver translator is *not* possible because the
`xdrv_123` JMPTBL is a frozen hand-curated ABI). It demonstrates the
viable path: **TinyC → dual-format C++ source**, because TinyC is a
constrained C-subset.

Supported subset: int/float/char[]/array decls (+ `persist`), functions,
if/else/while/for/return, the usual expressions, and a curated syscall
map. TinyC callbacks map to the plugin dispatch
(`main→pFUNC_INIT`, `EverySecond→pFUNC_EVERY_SECOND`,
`Command→pFUNC_COMMAND`, `WebCall/WebUI→FUNC_WEB_SENSOR`).

**Init contract:** `main()` is the `pFUNC_INIT` handler. It must
`return > 0` when init succeeded (e.g. sensor found) and `return 0`
(or fall through) when it failed. The translator wires that return
value to the plugin `initialized` flag at *every* return path — the
flag defaults to 0 (fail-safe), so a failed/missing-sensor init lets
the loader retry instead of marking a dead module as up. There is no
heuristic guess: the contract is the `main()` return value, period.

Honest about its limits, by design:
- Anything the subset parser can't handle is emitted as a
  `// TC-PASSTHROUGH:` comment — **nothing is silently dropped**.
- Unknown syscalls are emitted verbatim with `/* TODO: bind via
  JMPTBL */` and listed at the end — that JMPTBL binding step is the
  one thing a translator can flag but cannot safely automate.

The default sample is a self-contained **SML energy-meter emulator**.
