import { preprocess, PreprocessorError } from './preprocessor.js';
import { Lexer, LexerError } from './lexer.js';
import { Parser, ParseError } from './parser.js';
import { CodeGenerator, CodeGenError } from './codegen.js';
import { VM, VMError } from './vm.js';

// Re-export everything for convenient single-file import
export { preprocess, PreprocessorError } from './preprocessor.js';
export { Lexer, LexerError } from './lexer.js';
export { Parser, ParseError, NodeType } from './parser.js';
export { CodeGenerator, CodeGenError } from './codegen.js';
export { VM, VMError } from './vm.js';
export { Op, OpName, Syscall, SyscallName } from './opcodes.js';

// Combines Preprocessor + Lexer + Parser + CodeGen into one convenient interface












export class CompilerError extends Error {
    constructor(phase, originalError) {
        super(`[${phase}] ${originalError.message}`);
        this.phase = phase;
        this.originalError = originalError;
        this.line = originalError.line || originalError.token?.line || 0;
        this.col = originalError.col || originalError.token?.col || 0;
    }
}

export function compile(source, options = {}) {
    const predefined = options.defines || [];
    // Phase 0: Preprocess (#ifdef, #ifndef, #if, #else, #endif)
    let preprocessed;
    try {
        preprocessed = preprocess(source, predefined);
    } catch (e) {
        if (e instanceof PreprocessorError) throw new CompilerError('Preprocessor', e);
        throw e;
    }

    // Phase 1: Tokenize
    let tokens;
    try {
        const lexer = new Lexer(preprocessed);
        tokens = lexer.tokenize();
    } catch (e) {
        if (e instanceof LexerError) throw new CompilerError('Lexer', e);
        throw e;
    }

    // Phase 2: Parse
    let ast;
    try {
        const parser = new Parser(tokens);
        ast = parser.parse();
    } catch (e) {
        if (e instanceof ParseError) throw new CompilerError('Parser', e);
        throw e;
    }

    // Phase 3: Generate bytecode
    let compiled;
    try {
        const codegen = new CodeGenerator();
        compiled = codegen.compile(ast);
    } catch (e) {
        if (e instanceof CodeGenError) throw new CompilerError('CodeGen', e);
        throw e;
    }

    return {
        ...compiled,
        tokens,
        ast,
    };
}

export function compileAndRun(source, options = {}) {
    const compiled = compile(source);
    const vm = new VM(options);
    vm.load(compiled);
    vm.run();
    return {
        compiled,
        vm,
        state: vm.getState(),
    };
}


        // ─── Internationalization ────────────────────────
        const I18N = {
            en: {
                subtitle: 'ESP32/ESP8266 Bytecode Compiler & VM',
                btn_new: 'New', btn_open: 'Open', btn_save: 'Save',
                btn_compile: 'Compile', btn_save_tcb: 'Save .tcb', btn_run: 'Run',
                btn_upload: 'Upload', btn_run_device: 'Run on Device',
                btn_save_file: 'Save File', btn_close: '\u2715 Close',
                load_example: 'Load Example...', device_files: 'Device Files...',
                device_ip_placeholder: 'Device IP',
                confirm_unsaved_example: 'You have unsaved changes. Load example anyway?',
                confirm_unsaved_new: 'You have unsaved changes. Discard and create a new file?',
                confirm_unsaved_device: 'You have unsaved changes. Load device file anyway?',
                no_results: 'No results'
            },
            de: {
                subtitle: 'ESP32/ESP8266 Bytecode-Compiler & VM',
                btn_new: 'Neu', btn_open: '\u00D6ffnen', btn_save: 'Speichern',
                btn_compile: 'Kompilieren', btn_save_tcb: '.tcb speichern', btn_run: 'Ausf\u00FChren',
                btn_upload: 'Hochladen', btn_run_device: 'Auf Ger\u00E4t',
                btn_save_file: 'Datei speichern', btn_close: '\u2715 Schlie\u00DFen',
                load_example: 'Beispiel laden...', device_files: 'Ger\u00E4tedateien...',
                device_ip_placeholder: 'Ger\u00E4te-IP',
                confirm_unsaved_example: 'Ungespeicherte \u00C4nderungen. Beispiel trotzdem laden?',
                confirm_unsaved_new: 'Ungespeicherte \u00C4nderungen. Verwerfen und neue Datei?',
                confirm_unsaved_device: 'Ungespeicherte \u00C4nderungen. Ger\u00E4tedatei trotzdem laden?',
                no_results: 'Keine Treffer'
            }
        };
        let curLang = localStorage.getItem('tinyc_lang') || 'en';
        function t(key) { return I18N[curLang][key] || I18N.en[key] || key; }
        function setLang(lang) {
            curLang = lang;
            localStorage.setItem('tinyc_lang', lang);
            document.querySelector('.subtitle').textContent = t('subtitle');
            document.getElementById('btnNew').textContent = t('btn_new');
            document.getElementById('btnOpen').textContent = t('btn_open');
            document.getElementById('btnSave').textContent = t('btn_save');
            document.querySelector('[onclick="compileCode()"]').textContent = t('btn_compile');
            document.getElementById('btnSaveTcb').textContent = t('btn_save_tcb');
            document.querySelector('[onclick="runCode()"]').textContent = t('btn_run');
            document.querySelector('[onclick="uploadWiFi()"]').textContent = t('btn_upload');
            document.querySelector('[onclick="runOnDevice()"]').textContent = t('btn_run_device');
            document.querySelector('[onclick="saveToDevice()"]').textContent = t('btn_save_file');
            document.getElementById('btnCloseIde').textContent = t('btn_close');
            document.getElementById('deviceIp').placeholder = t('device_ip_placeholder');
            // Update examples dropdown first option
            var ex = document.getElementById('examples');
            if (ex && ex.options.length > 0) ex.options[0].textContent = t('load_example');
            // Update device files dropdown first option
            var df = document.getElementById('deviceFiles');
            if (df && df.options.length > 0) df.options[0].textContent = t('device_files');
            // Update language selector
            var ls = document.getElementById('langSelect');
            if (ls) ls.value = lang;
        }

        // ─── State ──────────────────────────────────────
        let lastCompiled = null;
        let currentFileHandle = null;
        let currentFileName = 'editor.tc';
        let isDirty = false;
        const hasFileSystemAccess = ('showOpenFilePicker' in window);

        // ─── Tab switching (right pane) ────────────────────
        document.querySelectorAll('.right-pane .tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('.right-pane .tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
                tab.classList.add('active');
                const panel = document.getElementById(tab.dataset.panel);
                if (panel) panel.classList.add('active');
            });
        });

        // ─── Tab switching (left pane) ─────────────────────
        document.querySelectorAll('#leftTabBar .tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('#leftTabBar .tab').forEach(t => t.classList.remove('active'));
                tab.classList.add('active');
                const editorEls = [document.getElementById('findBar'), document.querySelector('.editor-container')];
                const smlPanel = document.getElementById('smlPanel');
                if (tab.dataset.tab === 'sml') {
                    editorEls.forEach(el => { if (el) el.style.display = 'none'; });
                    smlPanel.classList.add('active');
                } else {
                    editorEls.forEach(el => { if (el) el.style.display = ''; });
                    smlPanel.classList.remove('active');
                }
            });
        });

        // ─── SML Descriptor ────────────────────────────────
        function getDeviceIp() {
            return document.getElementById('deviceIp').value.trim();
        }

        // ─── #include local-files map (v1.5.2) ─────────────
        // When the user opens main + companion files via "Open" (multi-
        // select), companion files land here keyed by basename. The
        // resolver below consults this map BEFORE making a network call
        // to the device — so PC-side development with a local folder of
        // .tc files works without uploading every helper file to the
        // device first.
        //
        // The user can also drop files explicitly via "Add include from
        // PC" button (TODO future). Cleared on full reload of the IDE
        // (in-memory only — no localStorage to keep things obvious).
        const tcLocalFiles = new Map();   // basename -> source text

        // ─── #include resolver (v1.5.2) ────────────────────
        // Recursively walks the source for `#include "filename.tc"` lines,
        // resolves each from (1) the local-files map (companion files
        // opened in the same multi-select), then (2) the device's UFS via
        // /tc_api?cmd=readfile, and returns a fully-inlined source string
        // that can be passed straight to compile().
        //
        // Why textual inclusion before preprocess():
        //   - #ifdef/#define inside included files behave as if typed inline
        //   - #defines from main file are visible in included files (and
        //     vice versa) — natural C semantics
        //   - first-include-only deduplication acts as an automatic
        //     header-guard — simple libraries don't need #ifndef/#define
        //
        // Cycle detection via a stack; max depth 16 (catches accidental
        // infinite recursion). Visited set ensures each file is inlined
        // at most once even if multiple files request it (or the same
        // file requests something twice).
        //
        // Falls back gracefully: if no #include directives are present,
        // returns the source unchanged without making any network calls
        // and without requiring a device IP.
        async function tcResolveIncludes(source) {
            const includeRe = /^[ \t]*#include[ \t]+"([^"]+)"/;
            // Quick scan — no #include? skip everything (no IP needed,
            // no network calls, transparent fallthrough for single-file
            // scripts).
            if (!includeRe.test(source) && !source.split('\n').some(l => includeRe.test(l))) {
                return source;
            }
            // IP only required if at least one #include can't be resolved
            // from the local-files map; deferred to first UFS fetch below.
            const visited = new Set();          // filename -> already-inlined
            const MAX_DEPTH = 16;

            // Helper: strip leading slashes so map lookups by basename match
            // both `#include "foo.tc"` and `#include "/foo.tc"`.
            function tcLocalKey(name) {
                return name.replace(/^\/+/, '');
            }

            async function fetchFile(filename) {
                // Order: (1) local-files map (companion files opened from
                // PC), (2) device UFS over HTTP. Local takes priority so
                // PC-side iteration doesn't depend on first uploading the
                // helper files to the device.
                const key = tcLocalKey(filename);
                if (tcLocalFiles.has(key)) {
                    return tcLocalFiles.get(key);
                }
                const ip = getDeviceIp();
                if (!ip) {
                    throw new Error(
                        `#include "${filename}" — not in local files and no ` +
                        `device IP set. Either open companion files from PC ` +
                        `via the multi-select Open dialog, or set the device IP ` +
                        `to fetch from the device's UFS.`);
                }
                const path = filename.startsWith('/') ? filename : '/' + filename;
                const url = `http://${ip}/tc_api?cmd=readfile&path=${encodeURIComponent(path)}`;
                let resp;
                try {
                    resp = await fetch(url, { mode: 'cors' });
                } catch (e) {
                    throw new Error(`#include "${filename}" — fetch failed: ${e.message}`);
                }
                if (!resp.ok) {
                    let detail = '';
                    try { detail = await resp.text(); } catch {}
                    throw new Error(`#include "${filename}" — HTTP ${resp.status} (${detail.slice(0, 80) || resp.statusText})`);
                }
                return await resp.text();
            }

            async function expand(text, stack) {
                if (stack.length > MAX_DEPTH) {
                    throw new Error(`#include depth exceeded (${MAX_DEPTH}): ${stack.join(' -> ')}`);
                }
                const lines = text.split('\n');
                const out = [];
                for (const ln of lines) {
                    const m = ln.match(includeRe);
                    if (!m) {
                        out.push(ln);
                        continue;
                    }
                    const fname = m[1];
                    if (stack.includes(fname)) {
                        throw new Error(`Circular #include: ${stack.concat([fname]).join(' -> ')}`);
                    }
                    if (visited.has(fname)) {
                        out.push(`// #include "${fname}" — already included, skipped`);
                        continue;
                    }
                    visited.add(fname);
                    const content = await fetchFile(fname);
                    const expanded = await expand(content, stack.concat([fname]));
                    out.push(`// === BEGIN #include "${fname}" ===`);
                    out.push(expanded);
                    out.push(`// === END #include "${fname}" ===`);
                }
                return out.join('\n');
            }

            return await expand(source, []);
        }

        function smlSetStatus(msg, cls) {
            const el = document.getElementById('smlStatus');
            el.textContent = msg;
            el.className = 'sml-status' + (cls ? ' ' + cls : '');
        }

        async function smlLoad() {
            const ip = getDeviceIp();
            if (!ip) { smlSetStatus('No device IP', 'error'); return; }
            smlSetStatus('Loading...');
            try {
                const resp = await fetch(`http://${ip}/tc_api?cmd=readfile&path=/sml_meter.def`, { mode: 'cors' });
                if (!resp.ok) {
                    const err = await resp.text().catch(() => resp.statusText);
                    smlSetStatus(`Load failed: ${err}`, 'error');
                    return;
                }
                const text = await resp.text();
                document.getElementById('smlEditor').value = text;
                updateSmlHighlight();
                smlSetStatus(`Loaded ${text.length} bytes`, 'success');
            } catch (e) {
                smlSetStatus(`Error: ${e.message}`, 'error');
            }
        }

        // Extract only the >M section from .tas file content
        function smlExtractMSection(text) {
            const lines = text.split('\n');
            let inM = false;
            const mLines = [];
            for (const line of lines) {
                if (/^>M\b/.test(line)) { inM = true; mLines.push(line); continue; }
                if (inM) {
                    // another >section ends >M
                    if (/^>[A-Z]/.test(line)) break;
                    mLines.push(line);
                }
            }
            return mLines.length > 0 ? mLines.join('\n') : text;
        }

        // Extract TinyC callback functions from SML editor content
        // Looks for void EverySecond() { ... } and void Every100ms() { ... } etc.
        function smlExtractCallbacks(text) {
            const callbacks = {};
            // Match: void FuncName() { ... } with brace counting
            const funcRe = /void\s+(EverySecond|Every100ms|Every50ms|EveryLoop)\s*\(\s*\)\s*\{/g;
            let m;
            while ((m = funcRe.exec(text)) !== null) {
                const name = m[1];
                let depth = 1;
                let i = funcRe.lastIndex;
                const bodyStart = i;
                while (i < text.length && depth > 0) {
                    if (text[i] === '{') depth++;
                    else if (text[i] === '}') depth--;
                    i++;
                }
                // body is between opening { and closing }
                const body = text.substring(bodyStart, i - 1).trim();
                if (body) callbacks[name] = body;
            }
            return callbacks;
        }

        // Merge SML callback bodies into main source before compilation
        function smlMergeCallbacks(mainSource) {
            const smlText = document.getElementById('smlEditor').value;
            const smlCallbacks = smlExtractCallbacks(smlText);
            const names = Object.keys(smlCallbacks);
            if (names.length === 0) return mainSource;

            let merged = mainSource;
            for (const name of names) {
                const smlBody = smlCallbacks[name];
                // Find the function in main source and inject SML body at end of its body
                const re = new RegExp('(void\\s+' + name + '\\s*\\(\\s*\\)\\s*\\{)');
                const match = re.exec(merged);
                if (match) {
                    // Function exists in main — find its closing brace, insert SML code before it
                    let depth = 1;
                    let pos = match.index + match[0].length;
                    while (pos < merged.length && depth > 0) {
                        if (merged[pos] === '{') depth++;
                        else if (merged[pos] === '}') depth--;
                        if (depth === 0) break;
                        pos++;
                    }
                    // pos is at the closing }, insert SML body before it
                    const indent = '    ';
                    const comment = `${indent}// --- from SML descriptor ---\n`;
                    const indentedBody = smlBody.split('\n').map(l => indent + l).join('\n');
                    merged = merged.substring(0, pos) + '\n' + comment + indentedBody + '\n' + merged.substring(pos);
                } else {
                    // Function doesn't exist in main — append it
                    merged += '\n\nvoid ' + name + '() {\n    // --- from SML descriptor ---\n';
                    merged += smlBody.split('\n').map(l => '    ' + l).join('\n');
                    merged += '\n}\n';
                }
            }
            return merged;
        }

        async function smlSave() {
            const ip = getDeviceIp();
            if (!ip) { smlSetStatus('No device IP', 'error'); return; }
            const raw = document.getElementById('smlEditor').value;
            // Only save the >M descriptor section to the device
            const mSection = smlExtractMSection(raw);
            const content = smlReplacePins(mSection);
            // warn if unreplaced placeholders remain
            const remaining = content.match(/%0\w+%/g);
            if (remaining) {
                smlSetStatus(`Warning: unresolved placeholders: ${remaining.join(', ')}`, 'error');
                return;
            }
            smlSetStatus('Saving...');
            try {
                const resp = await fetch(`http://${ip}/tc_api?cmd=writefile&path=/sml_meter.def`, {
                    method: 'POST', body: content, mode: 'cors',
                    headers: { 'Content-Type': 'text/plain' }
                });
                if (!resp.ok) {
                    const err = await resp.text().catch(() => resp.statusText);
                    smlSetStatus(`Save failed: ${err}`, 'error');
                    return;
                }
                const json = await resp.json().catch(() => ({}));
                smlSetStatus(`Saved ${json.size || content.length} bytes`, 'success');
            } catch (e) {
                smlSetStatus(`Error: ${e.message}`, 'error');
            }
        }

        async function smlRestart() {
            const ip = getDeviceIp();
            if (!ip) { smlSetStatus('No device IP', 'error'); return; }
            smlSetStatus('Restarting SML...');
            // mode: 'no-cors' so Safari accepts the opaque response when the
            // IDE was loaded cross-origin (e.g. served from :82 vs. /cm on
            // :80). The fire-and-forget GET still reaches Tasmota and runs
            // the command; we just can't read the JSON ack. If the request
            // makes it through the network stack, treat as success — only a
            // genuine network error (unreachable / DNS / etc.) throws to the
            // catch branch.
            try {
                await fetch(`http://${ip}/cm?cmnd=sensor53+r`, { mode: 'no-cors' });
                smlSetStatus('SML restart sent', 'success');
            } catch (e) {
                smlSetStatus(`Error: ${e.message}`, 'error');
            }
        }

        // ─── SML Meter Database ─────────────────────────
        const SML_METER_DEFAULT = 'https://raw.githubusercontent.com/ottelo9/tasmota-sml-script/main/script-list-menu/meters/';
        let SML_METER_BASE = SML_METER_DEFAULT;
        let smlMetersLoaded = false;
        let smlGpiosLoaded = false;

        // Load custom meter database URL from /sml_meter_url.txt on device
        async function smlLoadUrl() {
            const ip = getDeviceIp();
            if (!ip) return;
            try {
                const resp = await fetch(`http://${ip}/tc_api?cmd=readfile&path=/sml_meter_url.txt`, { mode: 'cors' });
                if (!resp.ok) return;
                const text = await resp.text();
                const url = text.trim();
                if (url && url.startsWith('http')) {
                    SML_METER_BASE = url.endsWith('/') ? url : url + '/';
                }
            } catch (e) { /* use default */ }
        }

        async function smlLoadMeterList() {
            if (smlMetersLoaded) return;
            await smlLoadUrl();
            const sel = document.getElementById('smlMeterSelect');
            try {
                smlSetStatus('Loading meter list...');
                const resp = await fetch(SML_METER_BASE + 'smartmeter.json');
                if (!resp.ok) { smlSetStatus('Failed to load meter list', 'error'); return; }
                const data = await resp.json();
                data.smartmeter.forEach(m => {
                    if (!m.filename) return;
                    const opt = document.createElement('option');
                    opt.value = m.filename;
                    opt.textContent = m.label;
                    sel.appendChild(opt);
                });
                smlMetersLoaded = true;
                smlSetStatus(`${data.smartmeter.length - 1} meters available`);
            } catch (e) {
                smlSetStatus(`Error: ${e.message}`, 'error');
            }
        }

        async function smlLoadGpios() {
            if (smlGpiosLoaded) return;
            const ip = getDeviceIp();
            if (!ip) return;
            try {
                const resp = await fetch(`http://${ip}/tc_api?cmd=freegpio`, { mode: 'cors' });
                if (!resp.ok) return;
                const data = await resp.json();
                if (!data.ok || !data.gpios) return;
                const rxSel = document.getElementById('smlRxPin');
                const txSel = document.getElementById('smlTxPin');
                data.gpios.forEach(pin => {
                    const o1 = document.createElement('option');
                    o1.value = pin; o1.textContent = 'GPIO' + pin;
                    rxSel.appendChild(o1);
                    const o2 = document.createElement('option');
                    o2.value = pin; o2.textContent = 'GPIO' + pin;
                    txSel.appendChild(o2);
                });
                smlGpiosLoaded = true;
                // restore last used pins and filter from localStorage
                try {
                    const lastRx = localStorage.getItem('sml_rxpin');
                    const lastTx = localStorage.getItem('sml_txpin');
                    const lastFilt = localStorage.getItem('sml_filter');
                    if (lastRx) rxSel.value = lastRx;
                    if (lastTx) txSel.value = lastTx;
                    if (lastFilt) document.getElementById('smlFilter').value = lastFilt;
                } catch(e) {}
            } catch (e) {
                // silently fail — pins can be entered manually in the descriptor
            }
        }

        async function smlLoadMeter(filename) {
            if (!filename) return;
            smlSetStatus('Loading meter definition...');
            try {
                const url = SML_METER_BASE + encodeURIComponent(filename).replace(/%2F/g, '/');
                const resp = await fetch(url);
                if (!resp.ok) { smlSetStatus(`Failed: HTTP ${resp.status}`, 'error'); return; }
                const text = await resp.text();
                document.getElementById('smlEditor').value = text;
                updateSmlHighlight();
                smlSetStatus(`Loaded: ${filename.split('/').pop()}`, 'success');
            } catch (e) {
                smlSetStatus(`Error: ${e.message}`, 'error');
            }
        }

        function smlReplacePins(text) {
            const rx = document.getElementById('smlRxPin').value;
            const tx = document.getElementById('smlTxPin').value;
            const filt = document.getElementById('smlFilter').value;
            // Leading "0" in placeholders is optional: %0rxpin% and %rxpin% both match.
            if (rx) {
                text = text.replace(/%0?rxpin%/g, rx);
                try { localStorage.setItem('sml_rxpin', rx); } catch(e) {}
            }
            if (tx) {
                text = text.replace(/%0?txpin%/g, tx);
                try { localStorage.setItem('sml_txpin', tx); } catch(e) {}
            }
            if (filt) {
                text = text.replace(/%0?smlf%/g, filt);
                try { localStorage.setItem('sml_filter', filt); } catch(e) {}
            }
            return text;
        }

        // Load meter list and GPIOs when SML tab is first clicked
        document.querySelectorAll('#leftTabBar .tab').forEach(tab => {
            tab.addEventListener('click', () => {
                if (tab.dataset.tab === 'sml') {
                    smlLoadMeterList();
                    smlLoadGpios();
                }
            });
        });

        // ─── Examples (auto-generated by bundle.py from examples/*.tc) ──
        const EXAMPLES = {             editor: `// TinyC - Hello World
// Compile (Ctrl+Enter) then Run (Ctrl+Shift+Enter)

#define LED 8
#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x05
#define INPUT_PULLDOWN 0x09

int square(int x) {
    return x * x;
}

int main() {
    printStr("Hello TinyC!");

    // Calculate some squares
    int i = 1;
    while (i <= 10) {
        int sq = square(i);
        print(sq);
        i++;
    }

    // Blink LED 5 times
    pinMode(LED, OUTPUT);
    int count = 0;
    while (count < 5) {
        digitalWrite(LED, 1);
        delay(250);
        digitalWrite(LED, 0);
        delay(250);
        count++;
    }

    printStr("Done!");
    return 0;
}`,
            blink: `// Blink LED on pin 8 (classic Arduino blink)
#define LED_PIN 8
#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x05
#define INPUT_PULLDOWN 0x09

int main() {
    pinMode(LED_PIN, OUTPUT);

    int i = 0;
    while (i < 10) {
        digitalWrite(LED_PIN, 1);
        delay(500);
        digitalWrite(LED_PIN, 0);
        delay(500);
        i++;
    }
    return 0;
}`,
            fibonacci: `// Fibonacci sequence calculator

int fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    int i = 0;
    while (i < 15) {
        int fib = fibonacci(i);
        print(fib);
        i++;
    }
    return 0;
}`,
            strings: `// String operations with char arrays
int main() {
    char greeting[32] = "Hello";
    char name[16] = "World";
    char buf[64] = "";

    // Copy and concatenate
    strcpy(buf, greeting);
    strcat(buf, ", ");
    strcat(buf, name);
    strcat(buf, "!\\n");

    printString(buf);   // Hello, World!
    print(strlen(buf)); // 14 (incl. newline)

    // Compare strings
    char a[8] = "abc";
    char b[8] = "abc";
    char c[8] = "xyz";

    print(strcmp(a, b));  // 0  (equal)
    print(strcmp(a, c));  // -1 (a < c)

    // Modify chars via array access
    char msg[16] = "HELLO\\n";
    msg[0] = 'h';
    printString(msg);  // hELLO

    return 0;
}`,
            sensor_read: `// Read analog sensor and display via serial
#define SENSOR_PIN 34
#define LED_PIN 2
#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x05
#define INPUT_PULLDOWN 0x09
#define THRESHOLD 2000

int main() {
    serialBegin(3, 1, 115200, 3, 64);  // RX=3, TX=1, 115200 baud, 8N1, 64 byte buf
    pinMode(LED_PIN, OUTPUT);

    int count = 0;
    while (count < 20) {
        int value = analogRead(SENSOR_PIN);

        serialPrint("Sensor: ");
        serialPrintInt(value);
        serialPrintln("");

        if (value > THRESHOLD) {
            digitalWrite(LED_PIN, 1);
        } else {
            digitalWrite(LED_PIN, 0);
        }

        delay(100);
        count++;
    }
    return 0;
}`,
            sort: `// Bubble sort an array of integers

int main() {
    int arr[10] = {64, 34, 25, 12, 22, 11, 90, 1, 55, 42};
    int n = 10;

    // Print original
    printStr("Before sort:");
    int k = 0;
    while (k < n) {
        print(arr[k]);
        k++;
    }

    // Bubble sort
    int i = 0;
    while (i < n - 1) {
        int j = 0;
        while (j < n - i - 1) {
            if (arr[j] > arr[j + 1]) {
                // Swap
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
            j++;
        }
        i++;
    }

    // Print sorted
    printStr("After sort:");
    k = 0;
    while (k < n) {
        print(arr[k]);
        k++;
    }

    return 0;
}`,
            file_io: `// File I/O demo — write and read back
// On ESP32: uses LittleFS, in browser: simulated virtual filesystem

int main() {
    char data[64];
    char buf[64];

    // Prepare data to write
    strcpy(data, "Hello from TinyC!\\n");

    // Write to file (r=read, w=write, a=append)
    int f = fileOpen("/test.txt", w);
    if (f >= 0) {
        fileWrite(f, data, strlen(data));
        fileClose(f);
        printStr("Written OK\\n");
    }

    // Read back
    f = fileOpen("/test.txt", r);
    if (f >= 0) {
        int n = fileRead(f, buf, 63);
        buf[n] = 0;  // null-terminate
        fileClose(f);
        printStr("Read back: ");
        printString(buf);
    }

    // Check file info
    if (fileExists("/test.txt")) {
        printStr("File size: ");
        print(fileSize("/test.txt"));
        printStr("\\n");
    }

    // Clean up
    fileDelete("/test.txt");
    printStr("File deleted\\n");

    return 0;
}`,
            callbacks: `// Callback functions — Tasmota integration
// EverySecond, JsonCall, WebCall are called automatically by Tasmota

int counter = 0;

void EverySecond() {
    counter++;
}

void JsonCall() {
    // Appends to Tasmota MQTT telemetry JSON
    char buf[64];
    sprintf(buf, ",\\"TinyC\\":{\\"Count\\":%d}", counter);
    responseAppend(buf);
}

void WebCall() {
    // Adds a row to the Tasmota web page
    char buf[64];
    sprintf(buf, "{s}TinyC Counter{m}%d{e}", counter);
    webSend(buf);
}

int main() {
    counter = 0;
    printStr("Callbacks active\\n");
    return 0;
}`,
            chart: `// Google Line Chart — 1000 data points stored in array
// Demonstrates: heap arrays, WebPage, WebCall, webSend, webFlush, random()
// WebPage draws the chart once, EverySecond shifts new data in
// WebCall shows a sensor-style status row (refreshed periodically)

#define N 1000

int data[N];       // ring buffer of sensor values (heap-allocated, >255)
int pos = 0;       // write position (ring buffer head)
int counter = 0;

// Simple integer sine approximation (x in 0..399 period, returns -100..100)
int isin(int x) {
    int p = x % 400;
    if (p < 0) p = p + 400;
    if (p < 100) return p;
    if (p < 200) return 200 - p;
    if (p < 300) return -(p - 200);
    return -(400 - p);
}

void EverySecond() {
    // Add a new data point each second (sine + noise)
    int s = isin(counter);
    int noise = random(-15, 15);
    data[pos] = s + noise;
    pos++;
    if (pos >= N) pos = 0;
    counter++;
}

void WebPage() {
    // Called once when page is drawn (FUNC_WEB_ADD_MAIN_BUTTON)
    // Perfect for charts, custom HTML, scripts that should load once
    char buf[128];
    int i;

    // ── Chart container and Google Charts loader ──
    webSend("<div id=\\"tc_chart\\" style=\\"width:100%;height:400px;\\"></div>");
    webSend("<script src=\\"https://www.gstatic.com/charts/loader.js\\"><\/script>");
    webSend("<script>");
    webSend("google.charts.load('current',{packages:['corechart']});");
    webSend("google.charts.setOnLoadCallback(function(){");
    webSend("var d=new google.visualization.DataTable();");
    webSend("d.addColumn('number','X');");
    webSend("d.addColumn('number','Value');");
    webSend("d.addColumn('number','Avg');");
    webSend("d.addRows([");
    webFlush();

    // ── Output all 1000 data points from ring buffer ──
    int avg = 0;
    i = 0;
    while (i < N) {
        // Read from ring buffer: oldest first
        int idx = pos + i;
        if (idx >= N) idx = idx - N;
        int val = data[idx];

        // Running average (smoothing filter)
        avg = (avg * 9 + val * 10) / 10;
        int smooth = avg / 10;

        // Format: [x, value, avg],
        sprintf(buf, "[%d,%d,%d]", i, val, smooth);
        if (i < N - 1) {
            strcat(buf, ",");
        }
        webSend(buf);

        // Flush every 100 rows to keep buffer manageable
        if (i % 100 == 99) {
            webFlush();
        }
        i++;
    }

    // ── Chart options and render ──
    webSend("]);");
    webSend("var o={title:'TinyC Live Data (1000 pts)',");
    webSend("curveType:'none',legend:{position:'bottom'},");
    webSend("hAxis:{title:'Sample'},vAxis:{title:'Value'},");
    webSend("chartArea:{width:'80%',height:'70%'},");
    webSend("series:{0:{lineWidth:1,color:'#89b4fa'},");
    webSend("1:{lineWidth:2,color:'#f38ba8'}}};");
    webSend("var c=new google.visualization.LineChart(");
    webSend("document.getElementById('tc_chart'));");
    webSend("c.draw(d,o);});");
    webSend("<\/script>");
    webFlush();
}

void WebCall() {
    // Called periodically for sensor display (FUNC_WEB_SENSOR)
    // Shows a status row that auto-refreshes
    char buf[64];
    sprintf(buf, "{s}TinyC Chart{m}%d samples{e}", counter);
    webSend(buf);
}

void JsonCall() {
    char buf[64];
    sprintf(buf, ",\\"TinyC\\":{\\"Samples\\":%d}", counter);
    responseAppend(buf);
}

int main() {
    // Fill array with initial data
    int i = 0;
    while (i < N) {
        data[i] = isin(i) + random(-15, 15);
        i++;
    }
    pos = 0;
    counter = 0;
    printStr("Chart demo active\\n");
    printStr("Open Tasmota web page to see the chart\\n");
    return 0;
}`,
            live_chart: `// Live Moving Line Chart — demonstrates _Q() macro for Google Charts
// The chart polls the device every second via fetch() and shows a
// sliding 60-second window of simulated temperature and humidity.
//
// _Q(temperature,humidity) is expanded at compile time to index-based
// query parameters (e.g. "0f;1f") — no variable names in the binary.

float temperature = 20.0;
float humidity = 50.0;
int tick = 0;

// Simple integer sine (period 400, range -100..100)
int isin(int x) {
    int p = x % 400;
    if (p < 0) p = p + 400;
    if (p < 100) return p;
    if (p < 200) return 200 - p;
    if (p < 300) return -(p - 200);
    return -(400 - p);
}

void EverySecond() {
    // Simulate sensor readings: sine wave + random noise
    int s = isin(tick * 4);
    temperature = 22.0 + s / 20.0 + random(-10, 10) / 10.0;
    humidity = 55.0 + s / 10.0 + random(-20, 20) / 10.0;
    tick++;
}

// Button on Tasmota main page linking to the chart
void WebPage() {
    webSend("<p><form action='/chart' method='get'>");
    webSend("<button>Live Chart</button></form></p>");
}

// Standalone chart page at /chart — no interference from Tasmota refresh
void WebOn() {
    int h = webHandler();
    if (h != 1) return;

    // Full HTML page
    webSend("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    webSend("<meta name='viewport' content='width=device-width'>");
    webSend("<title>TinyC Live Chart</title></head><body>");
    webSend("<div id='ch' style='width:800px;height:400px;'></div>");
    webSend("<script src='https://www.gstatic.com/charts/loader.js'><\/script>");
    webSend("<script>");
    webFlush();

    // Load Google Charts
    webSend("google.charts.load('current',{packages:['corechart']});");
    webSend("google.charts.setOnLoadCallback(init);");
    webSend("var T=[],H=[],N=60,ch;");

    // Init: create chart, start polling
    webSend("function init(){");
    webSend("ch=new google.visualization.LineChart(");
    webSend("document.getElementById('ch'));");
    webSend("poll();setInterval(poll,1000);}");
    webFlush();

    // Poll: fetch variables, update chart
    // _Q(temperature,humidity) expands to index+type at compile time
    webSend("function poll(){");
    webSend("fetch('/cm?cmnd=TinyC+%3F_Q(temperature,humidity)')");
    webSend(".then(r=>r.json()).then(j=>{");
    webSend("var v=j.TinyC;");
    webSend("T.push(v[0]);H.push(v[1]);");
    webSend("if(T.length>N){T.shift();H.shift();}");
    webSend("draw();});}");
    webFlush();

    // Draw chart — rows always [0..N-1] so x-axis stays fixed
    webSend("function draw(){");
    webSend("var t=new google.visualization.DataTable();");
    webSend("t.addColumn('number','Sec');");
    webSend("t.addColumn('number','Temp (C)');");
    webSend("t.addColumn('number','Humidity (%)');");
    webSend("for(var i=0;i<T.length;i++)");
    webSend("t.addRow([i,T[i],H[i]]);");
    webSend("ch.draw(t,{");
    webSend("title:'TinyC Live Sensor Data',");
    webSend("curveType:'function',");
    webSend("legend:{position:'bottom'},");
    webSend("hAxis:{viewWindow:{min:0,max:N},textPosition:'none'},");
    webSend("vAxis:{viewWindow:{min:10,max:80}},");
    webSend("chartArea:{width:'85%',height:'70%'},");
    webSend("series:{0:{color:'#e74c3c'},1:{color:'#3498db'}}");
    webSend("});}");

    webSend("<\/script></body></html>");
    webFlush();
}

void WebCall() {
    char buf[80];
    sprintf(buf, "{s}Temperature{m}%.1f &deg;C{e}", temperature);
    webSend(buf);
    sprintf(buf, "{s}Humidity{m}%.1f %%{e}", humidity);
    webSend(buf);
}

void JsonCall() {
    char buf[80];
    sprintf(buf, ",\\"TinyC\\":{\\"Temp\\":%.1f", temperature);
    responseAppend(buf);
    sprintf(buf, ",\\"Hum\\":%.1f}", humidity);
    responseAppend(buf);
}

int main() {
    webOn(1, "/chart");
    printStr("Live chart demo active\\n");
    printStr("Open http://<device>/chart to see the live chart\\n");
    return 0;
}`,
            udp: `// UDP multicast — share float variables between devices
// Compatible with Tasmota Scripter protocol (239.255.255.250:1999)
//
// Scalar global floats auto-send via UDP on every assignment (STORE_GLOBAL_UDP).
// Incoming UDP packets auto-update global float variables.
// UdpCall() fires when any UDP variable is received.

global float temperature;
int counter = 0;

void EverySecond() {
    counter++;
    // Assignment to global float auto-sends via UDP multicast
    temperature = 20.0 + sin(counter) * 5.0;
}

void UdpCall() {
    // This callback fires when any UDP global variable is updated.
    // The variable value is already updated — just read it directly.
    char buf[64];
    sprintf(buf, "UDP rx: temperature = %.1f\\n", temperature);
    printString(buf);
}

void WebCall() {
    char buf[64];
    sprintf(buf, "{s}Temperature{m}%.1f °C{e}", temperature);
    webSend(buf);
}

void JsonCall() {
    char buf[64];
    sprintf(buf, ",\\"TinyC\\":{\\"Temp\\":%.1f}", temperature);
    responseAppend(buf);
}

int main() {
    printStr("UDP multicast demo active\\n");
    printStr("Sending temperature every second\\n");
    return 0;
}`,
            sht31: `// SHT31 Temperature & Humidity Sensor Driver
// Scans both I2C buses (0 and 1) and both addresses (0x44, 0x45)
// Uses i2cSetDevice/i2cSetActiveFound to properly claim the address
// Demonstrates: I2C bus scan, CRC-8, WebCall, JsonCall

#define SHT_ADDR1  0x44
#define SHT_ADDR2  0x45

float sht_temp = 0.0;
float sht_humi = 0.0;
float sht_dewp = 0.0;
float sht_absh = 0.0;
int sht_ok = 0;
int sht_addr = 0;
int sht_bus = 0;
char sht_lbl[32];

// Shared buffer for I2C data
char sht_data[6];

// CRC-8 for SHT31 (polynomial 0x31)
int sht_crc8(int start, int len) {
    int crc = 0xFF;
    int i = 0;
    while (i < len) {
        crc = crc ^ sht_data[start + i];
        int bit = 0;
        while (bit < 8) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
            crc = crc & 0xFF;
            bit++;
        }
        i++;
    }
    return crc;
}

// Dewpoint (Magnus formula), returns °C
float sht_calc_dewpoint(float t, float h) {
    if (h <= 0.0) return 0.0;
    float gamma = (17.271 * t) / (237.7 + t) + log(h / 100.0);
    return (237.7 * gamma) / (17.271 - gamma);
}

// Absolute humidity in g/m³
float sht_calc_abshumi(float t, float h) {
    float ah = 6.112 * exp((17.67 * t) / (t + 243.5)) * h * 2.1674;
    return ah / (273.15 + t);
}

// Scan both buses and addresses using Tasmota's I2C claiming
// i2cSetDevice checks: not already claimed AND device responds
int sht_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(SHT_ADDR1, bus)) {
            sht_addr = SHT_ADDR1;
            sht_bus = bus;
            i2cSetActiveFound(sht_addr, "SHT3X", sht_bus);
            return 1;
        }
        if (i2cSetDevice(SHT_ADDR2, bus)) {
            sht_addr = SHT_ADDR2;
            sht_bus = bus;
            i2cSetActiveFound(sht_addr, "SHT3X", sht_bus);
            return 1;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!sht_addr) {
        if (!sht_scan()) {
            sht_ok = 0;
            return;
        }
    }

    // Send measurement command: clock stretching, high repeatability (0x2C06)
    if (!i2cWrite8(sht_addr, 0x2C, 0x06, sht_bus)) {
        sht_ok = 0;
        sht_addr = 0;  // force rescan next time
        return;
    }

    // SHT31 needs ~15ms for high repeatability measurement
    delay(30);

    // Read 6 bytes: [temp_msb, temp_lsb, temp_crc, humi_msb, humi_lsb, humi_crc]
    if (!i2cRead0(sht_addr, sht_data, 6, sht_bus)) {
        sht_ok = 0;
        sht_addr = 0;
        return;
    }

    // Verify CRC for temperature (bytes 0-1, CRC in byte 2)
    if (sht_crc8(0, 2) != sht_data[2]) {
        sht_ok = 0;
        return;
    }

    // Verify CRC for humidity (bytes 3-4, CRC in byte 5)
    if (sht_crc8(3, 2) != sht_data[5]) {
        sht_ok = 0;
        return;
    }

    // Temperature: -45 + 175 * raw / 65535
    int raw_t = (sht_data[0] << 8) | sht_data[1];
    sht_temp = -45.0 + 175.0 * (float)raw_t / 65535.0;

    // Humidity: 100 * raw / 65535
    int raw_h = (sht_data[3] << 8) | sht_data[4];
    sht_humi = 100.0 * (float)raw_h / 65535.0;

    sht_dewp = sht_calc_dewpoint(sht_temp, sht_humi);
    sht_absh = sht_calc_abshumi(sht_temp, sht_humi);

    sht_ok = 1;
}

void sht_web_label(int idx) {
    char vt[32];
    LGetString(idx, sht_lbl);
    strcpy(vt, "{s}SHT31 ");
    strcat(vt, sht_lbl);
    strcat(vt, "{m}");
    webSend(vt);
}

void WebCall() {
    char buf[32];
    if (sht_ok) {
        sht_web_label(0);
        sprintf(buf, "%.1f &deg;C{e}", sht_temp);
        webSend(buf);
        sht_web_label(1);
        sprintf(buf, "%.1f %{e}", sht_humi);
        webSend(buf);
        sht_web_label(3);
        sprintf(buf, "%.1f &deg;C{e}", sht_dewp);
        webSend(buf);
        sht_web_label(20);
        sprintf(buf, "%.1f g/m&sup3;{e}", sht_absh);
        webSend(buf);
    } else {
        webSend("{s}SHT31{m}not found{e}");
    }
}

void JsonCall() {
    if (!sht_ok) return;
    char buf[96];
    sprintf(buf, ",\\"SHT3X\\":{\\"Temperature\\":%.1f", sht_temp);
    responseAppend(buf);
    sprintf(buf, ",\\"Humidity\\":%.1f", sht_humi);
    responseAppend(buf);
    sprintf(buf, ",\\"DewPoint\\":%.1f", sht_dewp);
    responseAppend(buf);
    sprintf(buf, ",\\"AbsHumidity\\":%.1f}", sht_absh);
    responseAppend(buf);
}

// Called before VM stops — release I2C address so driver can restart
void OnExit() {
    if (sht_addr) {
        I2cResetActive(sht_addr, sht_bus);
        sht_addr = 0;
    }
}

int main() {
    char buf[48];
    sht_ok = 0;
    sht_addr = 0;

    if (sht_scan()) {
        sprintf(buf, "SHT3X found at 0x%x on bus %d", sht_addr, sht_bus);
        addLog(buf);
    } else {
        addLog("SHT3X not found on any bus");
    }
    return 0;
}`,
            bmx280: `// BMx280 Temperature, Pressure & (optional) Humidity Sensor Driver
// Auto-detects BMP280 (ID 0x58) vs BME280 (ID 0x60)
// I2C addresses: 0x76 (SDO=GND) or 0x77 (SDO=VCC)
// Scans both I2C buses, claims via Tasmota I2C system
// Reads every second, displays on web UI + JSON teleperiod
// Includes 6h chart history

#define BMX_ADDR1  0x76
#define BMX_ADDR2  0x77
#define BMP_ID     0x58
#define BME_ID     0x60

// Measurement results
float bmx_temp = 0.0;
float bmx_humi = 0.0;
float bmx_pres = 0.0;
float bmx_dewp = 0.0;
float bmx_absh = 0.0;
int bmx_ok = 0;
int bmx_addr = 0;
int bmx_bus = 0;
int bmx_has_humi = 0;   // 1 = BME280, 0 = BMP280

// I2C data buffer
char bmx_buf[26];

// Calibration data (registers 0x88..0x9F)
// Temperature
int dig_T1;
int dig_T2;
int dig_T3;
// Pressure
int dig_P1;
int dig_P2;
int dig_P3;
int dig_P4;
int dig_P5;
int dig_P6;
int dig_P7;
int dig_P8;
int dig_P9;
// Humidity (BME280 only)
int dig_H1;
int dig_H2;
int dig_H3;
int dig_H4;
int dig_H5;
int dig_H6;

// t_fine shared between temp and pressure/humidity compensation
int t_fine;

#ifdef USE_CHARTS
// Chart history (6h at 1 sample/min = 360 points)
#define CHART_LEN 360
float hist_temp[CHART_LEN];
float hist_humi[CHART_LEN];
float hist_pres[CHART_LEN];
int hist_pos;
int hist_tick;
#endif

// Chip name for display
char bmx_name[8];
char bmx_lbl[32];

int sign16(int val) {
    if (val >= 32768) return val - 65536;
    return val;
}

int sign8(int val) {
    if (val >= 128) return val - 256;
    return val;
}

// Read calibration data
int bmx_read_calib() {
    // Read 24 bytes from 0x88..0x9F (temp + pressure)
    if (!i2cRead(bmx_addr, 0x88, bmx_buf, 24, bmx_bus)) return 0;

    dig_T1 = bmx_buf[0] | (bmx_buf[1] << 8);
    dig_T2 = sign16(bmx_buf[2] | (bmx_buf[3] << 8));
    dig_T3 = sign16(bmx_buf[4] | (bmx_buf[5] << 8));

    dig_P1 = bmx_buf[6] | (bmx_buf[7] << 8);
    dig_P2 = sign16(bmx_buf[8] | (bmx_buf[9] << 8));
    dig_P3 = sign16(bmx_buf[10] | (bmx_buf[11] << 8));
    dig_P4 = sign16(bmx_buf[12] | (bmx_buf[13] << 8));
    dig_P5 = sign16(bmx_buf[14] | (bmx_buf[15] << 8));
    dig_P6 = sign16(bmx_buf[16] | (bmx_buf[17] << 8));
    dig_P7 = sign16(bmx_buf[18] | (bmx_buf[19] << 8));
    dig_P8 = sign16(bmx_buf[20] | (bmx_buf[21] << 8));
    dig_P9 = sign16(bmx_buf[22] | (bmx_buf[23] << 8));

    if (bmx_has_humi) {
        // H1 at 0xA1
        dig_H1 = i2cRead8(bmx_addr, 0xA1, bmx_bus);
        // Read 7 bytes from 0xE1..0xE7
        if (!i2cRead(bmx_addr, 0xE1, bmx_buf, 7, bmx_bus)) return 0;
        dig_H2 = sign16(bmx_buf[0] | (bmx_buf[1] << 8));
        dig_H3 = bmx_buf[2];
        dig_H4 = sign16((bmx_buf[3] << 4) | (bmx_buf[4] & 0x0F));
        dig_H5 = sign16((bmx_buf[5] << 4) | ((bmx_buf[4] >> 4) & 0x0F));
        dig_H6 = sign8(bmx_buf[6]);
    }
    return 1;
}

int bmx_configure() {
    if (bmx_has_humi) {
        // ctrl_hum (0xF2): humidity oversampling x1
        if (!i2cWrite8(bmx_addr, 0xF2, 0x01, bmx_bus)) return 0;
    }
    // config (0xF5): standby 1000ms, filter off = 0xA0
    if (!i2cWrite8(bmx_addr, 0xF5, 0xA0, bmx_bus)) return 0;
    // ctrl_meas (0xF4): temp os x1, press os x1, normal mode = 0x27
    // must be written AFTER ctrl_hum for BME280
    if (!i2cWrite8(bmx_addr, 0xF4, 0x27, bmx_bus)) return 0;
    return 1;
}

// Scan both buses and addresses, auto-detect chip type
int bmx_scan() {
    int bus = 0;
    while (bus < 2) {
        int addr = BMX_ADDR1;
        while (addr <= BMX_ADDR2) {
            if (i2cSetDevice(addr, bus)) {
                int id = i2cRead8(addr, 0xD0, bus);
                if (id == BME_ID || id == BMP_ID) {
                    bmx_addr = addr;
                    bmx_bus = bus;
                    if (id == BME_ID) {
                        bmx_has_humi = 1;
                        strcpy(bmx_name, "BME280");
                    } else {
                        bmx_has_humi = 0;
                        strcpy(bmx_name, "BMP280");
                    }
                    i2cSetActiveFound(bmx_addr, "BMx280", bmx_bus);
                    return 1;
                }
            }
            addr++;
        }
        bus++;
    }
    return 0;
}

// Temperature compensation — sets t_fine
int bmx_comp_temp(int adc_T) {
    int var1 = ((((adc_T >> 3) - (dig_T1 << 1))) * dig_T2) >> 11;
    int var2 = (((((adc_T >> 4) - dig_T1) * ((adc_T >> 4) - dig_T1)) >> 12) * dig_T3) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

// Pressure compensation — returns Pa
float bmx_comp_pres(int adc_P) {
    float var1 = (float)t_fine / 2.0 - 64000.0;
    float var2 = var1 * var1 * (float)dig_P6 / 32768.0;
    var2 = var2 + var1 * (float)dig_P5 * 2.0;
    var2 = var2 / 4.0 + (float)dig_P4 * 65536.0;
    var1 = ((float)dig_P3 * var1 * var1 / 524288.0 + (float)dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (float)dig_P1;
    if (var1 == 0.0) return 0.0;
    float p = 1048576.0 - (float)adc_P;
    p = (p - var2 / 4096.0) * 6250.0 / var1;
    var1 = (float)dig_P9 * p * p / 2147483648.0;
    var2 = p * (float)dig_P8 / 32768.0;
    p = p + (var1 + var2 + (float)dig_P7) / 16.0;
    return p;
}

// Humidity compensation (BME280 only) — returns %RH
float bmx_comp_humi(int adc_H) {
    float h = (float)t_fine - 76800.0;
    if (h == 0.0) return 0.0;
    h = ((float)adc_H - ((float)dig_H4 * 64.0 + (float)dig_H5 / 16384.0 * h)) *
        ((float)dig_H2 / 65536.0 * (1.0 + (float)dig_H6 / 67108864.0 * h *
        (1.0 + (float)dig_H3 / 67108864.0 * h)));
    h = h * (1.0 - (float)dig_H1 * h / 524288.0);
    if (h > 100.0) h = 100.0;
    if (h < 0.0) h = 0.0;
    return h;
}

// Dewpoint (Magnus formula), returns °C
float bmx_calc_dewpoint(float t, float h) {
    if (h <= 0.0) return 0.0;
    float gamma = (17.271 * t) / (237.7 + t) + log(h / 100.0);
    return (237.7 * gamma) / (17.271 - gamma);
}

// Absolute humidity in g/m³
float bmx_calc_abshumi(float t, float h) {
    float ah = 6.112 * exp((17.67 * t) / (t + 243.5)) * h * 2.1674;
    return ah / (273.15 + t);
}

void EverySecond() {
    if (!bmx_addr) {
        if (!bmx_scan()) { bmx_ok = 0; return; }
        if (!bmx_read_calib()) { bmx_ok = 0; bmx_addr = 0; return; }
        if (!bmx_configure()) { bmx_ok = 0; bmx_addr = 0; return; }
    }

    // Read from 0xF7: press[3] + temp[3] + humi[2] (8 for BME, 6 for BMP)
    int rlen = 6;
    if (bmx_has_humi) rlen = 8;
    if (!i2cRead(bmx_addr, 0xF7, bmx_buf, rlen, bmx_bus)) {
        bmx_ok = 0;
        bmx_addr = 0;
        return;
    }

    int adc_P = (bmx_buf[0] << 12) | (bmx_buf[1] << 4) | (bmx_buf[2] >> 4);
    int adc_T = (bmx_buf[3] << 12) | (bmx_buf[4] << 4) | (bmx_buf[5] >> 4);

    int T100 = bmx_comp_temp(adc_T);
    bmx_temp = (float)T100 / 100.0;
    bmx_pres = bmx_comp_pres(adc_P) / 100.0;

    if (bmx_has_humi) {
        int adc_H = (bmx_buf[6] << 8) | bmx_buf[7];
        bmx_humi = bmx_comp_humi(adc_H);
        bmx_dewp = bmx_calc_dewpoint(bmx_temp, bmx_humi);
        bmx_absh = bmx_calc_abshumi(bmx_temp, bmx_humi);
    }

    bmx_ok = 1;

#ifdef USE_CHARTS
    // Chart history every minute
    hist_tick++;
    if (hist_tick >= 60) {
        hist_tick = 0;
        hist_temp[hist_pos % CHART_LEN] = bmx_temp;
        hist_pres[hist_pos % CHART_LEN] = bmx_pres;
        if (bmx_has_humi) {
            hist_humi[hist_pos % CHART_LEN] = bmx_humi;
        }
        hist_pos++;
    }
#endif
}

#ifdef USE_CHARTS
void WebPage() {
    int n = hist_pos;
    if (n > CHART_LEN) n = CHART_LEN;
    if (n > 0) {
        WebChart('l', "Temperature", "\\u00b0C", 0xe74c3c, hist_pos, CHART_LEN, hist_temp, 1, 0, 0, 0);
        WebChart('l', "Pressure", "hPa", 0x27ae60, hist_pos, CHART_LEN, hist_pres, 1, 0, 0, 0);
        if (bmx_has_humi) {
            WebChart('l', "Humidity", "%RH", 0x3498db, hist_pos, CHART_LEN, hist_humi, 1, 0, 0, 0);
        }
    }
}
#endif

void bmx_web_label(int idx) {
    char vt[32];
    LGetString(idx, bmx_lbl);
    strcpy(vt, "{s}");
    strcat(vt, bmx_name);
    strcat(vt, " ");
    strcat(vt, bmx_lbl);
    strcat(vt, "{m}");
    webSend(vt);
}

void WebCall() {
    char vt[32];
    if (bmx_ok) {
        bmx_web_label(0);
        sprintf(vt, "%.1f &deg;C{e}", bmx_temp);
        webSend(vt);
        bmx_web_label(2);
        sprintf(vt, "%.1f hPa{e}", bmx_pres);
        webSend(vt);
        if (bmx_has_humi) {
            bmx_web_label(1);
            sprintf(vt, "%.1f %{e}", bmx_humi);
            webSend(vt);
            bmx_web_label(3);
            sprintf(vt, "%.1f &deg;C{e}", bmx_dewp);
            webSend(vt);
            bmx_web_label(20);
            sprintf(vt, "%.1f g/m&sup3;{e}", bmx_absh);
            webSend(vt);
        }
    } else {
        webSend("{s}BMx280{m}not found{e}");
    }
}

void JsonCall() {
    if (!bmx_ok) return;
    char buf[96];
    // ,\\"BME280\\":{\\"Temperature\\":23.5,\\"Pressure\\":1013.2,\\"Humidity\\":45.3}
    strcpy(buf, ",\\"");
    strcat(buf, bmx_name);
    strcat(buf, "\\":{");
    responseAppend(buf);
    sprintf(buf, "\\"Temperature\\":%.1f", bmx_temp);
    responseAppend(buf);
    sprintf(buf, ",\\"Pressure\\":%.1f", bmx_pres);
    responseAppend(buf);
    if (bmx_has_humi) {
        sprintf(buf, ",\\"Humidity\\":%.1f", bmx_humi);
        responseAppend(buf);
        sprintf(buf, ",\\"DewPoint\\":%.1f", bmx_dewp);
        responseAppend(buf);
        sprintf(buf, ",\\"AbsHumidity\\":%.1f", bmx_absh);
        responseAppend(buf);
    }
    responseAppend("}");
}

// Called before VM stops — release I2C address so driver can restart
void OnExit() {
    if (bmx_addr) {
        I2cResetActive(bmx_addr, bmx_bus);
        bmx_addr = 0;
    }
}

int main() {
    char buf[64];
    bmx_ok = 0;
    bmx_addr = 0;
#ifdef USE_CHARTS
    hist_pos = 0;
    hist_tick = 0;
#endif

    if (bmx_scan()) {
        strcpy(buf, bmx_name);
        sprintfAppend(buf, " found at 0x%x on bus %d", bmx_addr, bmx_bus);
        addLog(buf);
        if (bmx_read_calib() && bmx_configure()) {
            addLog("Calibration loaded, sensor active");
        } else {
            addLog("Calibration/config failed");
            bmx_addr = 0;
        }
    } else {
        addLog("BMx280 not found on any bus");
    }
    return 0;
}`,
            ads1115: `// ADS1115 16-bit 4-Channel ADC Driver
// I2C addresses: 0x48 (ADDR=GND), 0x49 (ADDR=VDD), 0x4A (ADDR=SDA), 0x4B (ADDR=SCL)
// Single-ended mode: reads AIN0..AIN3 vs GND
// Configurable PGA gain, single-shot conversion
// Displays voltage on web UI + JSON teleperiod + 6h chart

#define ADS_ADDR1  0x48
#define ADS_ADDR2  0x4B

// PGA gain settings (config bits 11-9)
// Change ADS_PGA to match your voltage range
#define PGA_6144   0   // ±6.144V  (LSB = 187.5µV)
#define PGA_4096   1   // ±4.096V  (LSB = 125µV)
#define PGA_2048   2   // ±2.048V  (LSB = 62.5µV)  — default
#define PGA_1024   3   // ±1.024V  (LSB = 31.25µV)
#define PGA_0512   4   // ±0.512V  (LSB = 15.625µV)
#define PGA_0256   5   // ±0.256V  (LSB = 7.8125µV)

// *** Configuration — adjust to your needs ***
#define ADS_PGA       PGA_4096    // gain setting
#define ADS_CHANNELS  4           // number of channels to read (1..4)

// LSB voltage in µV for each PGA setting (index = PGA value)
// We store as int µV to avoid float array init issues
// PGA_6144=187500, PGA_4096=125000, PGA_2048=62500, PGA_1024=31250, PGA_0512=15625, PGA_0256=7813

int ads_addr = 0;
int ads_bus = 0;
int ads_ok = 0;

// Raw ADC values and voltages per channel
int ads_raw[4];
float ads_volt[4];

// I2C buffer for 2-byte reads/writes
char ads_buf[4];

// Chart history (6h at 1/min = 360)
#define CHART_LEN 360
float hist_ch0[CHART_LEN];
float hist_ch1[CHART_LEN];
float hist_ch2[CHART_LEN];
float hist_ch3[CHART_LEN];
int hist_pos;
int hist_tick;

// Get LSB size in µV for current PGA setting
float ads_get_lsb() {
    if (ADS_PGA == 0) return 187.5;
    if (ADS_PGA == 1) return 125.0;
    if (ADS_PGA == 2) return 62.5;
    if (ADS_PGA == 3) return 31.25;
    if (ADS_PGA == 4) return 15.625;
    return 7.8125;
}

// Sign-extend 16-bit to signed int
int sign16(int val) {
    if (val >= 32768) return val - 65536;
    return val;
}

// Scan for ADS1115 on both buses
int ads_scan() {
    int bus = 0;
    while (bus < 2) {
        int addr = ADS_ADDR1;
        while (addr <= ADS_ADDR2) {
            if (i2cSetDevice(addr, bus)) {
                // Read config register — default after reset is 0x8583
                if (i2cRead(addr, 0x01, ads_buf, 2, bus)) {
                    int cfg = (ads_buf[0] << 8) | ads_buf[1];
                    // Check bits that should be default: DR=100 (128SPS), MODE=1
                    // Default config = 0x8583, but just check device responds
                    if (cfg != 0 && cfg != 0xFFFF) {
                        ads_addr = addr;
                        ads_bus = bus;
                        i2cSetActiveFound(ads_addr, "ADS1115", ads_bus);
                        return 1;
                    }
                }
            }
            addr++;
        }
        bus++;
    }
    return 0;
}

// Read one channel (0-3) in single-shot mode, returns raw signed value
int ads_read_channel(int ch) {
    // Config register: OS=1 (start), MUX=1xx (single-ended AINx), PGA, MODE=1 (single-shot)
    // DR=100 (128SPS), COMP_QUE=11 (disable comparator)
    // Byte 0 (high): OS[15] MUX[14:12] PGA[11:9] MODE[8]
    //   OS=1, MUX = 100 + ch, PGA = ADS_PGA, MODE = 1
    // Byte 1 (low): DR[7:5] COMP_MODE[4] COMP_POL[3] COMP_LAT[2] COMP_QUE[1:0]
    //   DR=100 (128SPS), rest=0, COMP_QUE=11
    //   = 10000011 = 0x83

    int mux = 4 + ch;    // 100=AIN0, 101=AIN1, 110=AIN2, 111=AIN3
    int hi = 0x80 | (mux << 4) | (ADS_PGA << 1) | 0x01;
    int lo = 0x83;        // 128 SPS, comparator disabled

    // Write config register
    ads_buf[0] = hi;
    ads_buf[1] = lo;
    if (!i2cWrite(ads_addr, 0x01, ads_buf, 2, ads_bus)) return 0;

    // Wait for conversion (128 SPS = ~8ms, use 10ms margin)
    delay(10);

    // Read conversion register (2 bytes, MSB first)
    if (!i2cRead(ads_addr, 0x00, ads_buf, 2, ads_bus)) return 0;

    int raw = (ads_buf[0] << 8) | ads_buf[1];
    return sign16(raw);
}

void EverySecond() {
    if (!ads_addr) {
        if (!ads_scan()) { ads_ok = 0; return; }
    }

    float lsb = ads_get_lsb();
    int ch = 0;
    while (ch < ADS_CHANNELS) {
        ads_raw[ch] = ads_read_channel(ch);
        // Convert to volts: raw * lsb_µV / 1000000
        ads_volt[ch] = (float)ads_raw[ch] * lsb / 1000000.0;
        ch++;
    }
    ads_ok = 1;

    // Chart history every minute
    hist_tick++;
    if (hist_tick >= 60) {
        hist_tick = 0;
        int p = hist_pos % CHART_LEN;
        hist_ch0[p] = ads_volt[0];
        if (ADS_CHANNELS > 1) hist_ch1[p] = ads_volt[1];
        if (ADS_CHANNELS > 2) hist_ch2[p] = ads_volt[2];
        if (ADS_CHANNELS > 3) hist_ch3[p] = ads_volt[3];
        hist_pos++;
    }
}

void WebPage() {
    int n = hist_pos;
    if (n > CHART_LEN) n = CHART_LEN;
    if (n > 0) {
        WebChart('l', "ADS1115 Voltages", "AIN0 (V)", 0xe74c3c, hist_pos, CHART_LEN, hist_ch0, 1, 0, 0, 0);
        if (ADS_CHANNELS > 1) {
            WebChart('l', "", "AIN1 (V)", 0x3498db, hist_pos, CHART_LEN, hist_ch1, 1, 0, 0, 0);
        }
        if (ADS_CHANNELS > 2) {
            WebChart('l', "", "AIN2 (V)", 0x27ae60, hist_pos, CHART_LEN, hist_ch2, 1, 0, 0, 0);
        }
        if (ADS_CHANNELS > 3) {
            WebChart('l', "", "AIN3 (V)", 0x9b59b6, hist_pos, CHART_LEN, hist_ch3, 1, 0, 0, 0);
        }
    }
}

void WebCall() {
    char buf[80];
    if (ads_ok) {
        int ch = 0;
        while (ch < ADS_CHANNELS) {
            sprintf(buf, "{s}ADS1115 AIN%d{m}", ch);
            webSend(buf);
            sprintf(buf, "%.4f V{e}", ads_volt[ch]);
            webSend(buf);
            ch++;
        }
    } else {
        webSend("{s}ADS1115{m}not found{e}");
    }
}

void JsonCall() {
    if (!ads_ok) return;
    char buf[64];
    responseAppend(",\\"ADS1115\\":{");
    int ch = 0;
    while (ch < ADS_CHANNELS) {
        if (ch > 0) responseAppend(",");
        sprintf(buf, "\\"A%d\\":", ch);
        responseAppend(buf);
        sprintf(buf, "%.4f", ads_volt[ch]);
        responseAppend(buf);
        ch++;
    }
    responseAppend("}");
}

void OnExit() {
    if (ads_addr) {
        I2cResetActive(ads_addr, ads_bus);
        ads_addr = 0;
    }
}

int main() {
    char buf[64];
    ads_ok = 0;
    ads_addr = 0;
    hist_pos = 0;
    hist_tick = 0;

    if (ads_scan()) {
        sprintf(buf, "ADS1115 found at 0x%x on bus %d", ads_addr, ads_bus);
        addLog(buf);
        sprintf(buf, "PGA gain: %d, channels: %d", ADS_PGA, ADS_CHANNELS);
        addLog(buf);
    } else {
        addLog("ADS1115 not found on any bus");
    }
    return 0;
}`,
            vl53l0x: `// vl53l0x.tc — VL53L0X Time-of-Flight distance sensor driver
// Pure TinyC — no native syscalls, uses i2cRead8/i2cWrite8/i2cRead/i2cWrite
// Based on working Pololu/ST plugin (VL53L0X_c.h)
// Includes full timing budget calculation (required for calibration)

#define VL_ADDR   0x29
#define VL_ID     0xEE
#define VL_TIMEOUT 500

// ── Registers ────────────────────────────────────────
#define SYSRANGE_START                  0x00
#define SYSTEM_SEQUENCE_CONFIG          0x01
#define SYSTEM_INTERMEASUREMENT_PERIOD  0x04
#define SYSTEM_INTERRUPT_CONFIG_GPIO    0x0A
#define SYSTEM_INTERRUPT_CLEAR          0x0B
#define RESULT_INTERRUPT_STATUS         0x13
#define RESULT_RANGE_STATUS             0x14
#define MSRC_CONFIG_CONTROL             0x60
#define FINAL_RANGE_CONFIG_MIN_COUNT_RATE 0x44
#define MSRC_CONFIG_TIMEOUT_MACROP      0x46
#define PRE_RANGE_CONFIG_VCSEL_PERIOD   0x50
#define PRE_RANGE_CONFIG_TIMEOUT_HI     0x51
#define FINAL_RANGE_CONFIG_VCSEL_PERIOD 0x70
#define FINAL_RANGE_CONFIG_TIMEOUT_HI   0x71
#define GPIO_HV_MUX_ACTIVE_HIGH        0x84
#define VHV_CONFIG_PAD_SCL_SDA_HV       0x89
#define GLOBAL_CONFIG_SPAD_ENABLES_REF_0 0xB0
#define GLOBAL_CONFIG_REF_EN_START_SEL  0xB6
#define GLOBAL_CONFIG_VCSEL_WIDTH       0x32
#define DYNAMIC_SPAD_NUM_REQ            0x4E
#define DYNAMIC_SPAD_REF_EN_START       0x4F
#define IDENTIFICATION_MODEL_ID         0xC0
#define ALGO_PHASECAL_LIM               0x30

// ── State ────────────────────────────────────────────
int vl_bus;
int vl_stop_var;
int vl_budget_us;
int vl_ok;
int vl_valid;       // 1 = valid reading, 0 = no target
float vl_range;
char vl_buf[12];
int vl_tstart;

// ── I2C wrappers — match plugin's Wire transactions ──

void wr(int reg, int val) {
  i2cWrite8(VL_ADDR, reg, val, vl_bus);
}

int rd(int reg) {
  // Use buffer read (STOP mode) — matches plugin exactly
  i2cRead(VL_ADDR, reg, vl_buf, 1, vl_bus);
  return vl_buf[0];
}

void wr16(int reg, int val) {
  vl_buf[0] = val / 256;
  vl_buf[1] = val & 255;
  i2cWrite(VL_ADDR, reg, vl_buf, 2, vl_bus);
}

int rd16(int reg) {
  i2cRead(VL_ADDR, reg, vl_buf, 2, vl_bus);
  return vl_buf[0] * 256 + vl_buf[1];
}

void wr32(int reg, int val) {
  vl_buf[0] = (val / 16777216) & 255;
  vl_buf[1] = (val / 65536) & 255;
  vl_buf[2] = (val / 256) & 255;
  vl_buf[3] = val & 255;
  i2cWrite(VL_ADDR, reg, vl_buf, 4, vl_bus);
}

// ── Timeout helpers ──────────────────────────────────

void startTO() { vl_tstart = millis(); }

int timedOut() {
  int e = millis() - vl_tstart;
  if (e < 0) { e = e + 2147483647; }
  return e > VL_TIMEOUT;
}

// ── VCSEL period encode/decode ───────────────────────

int decodeVcsel(int rv) { return (rv + 1) * 2; }
int encodeVcsel(int p) { return p / 2 - 1; }

// macro period in nanoseconds, avoids overflow
int macroPeriodNs(int pclks) {
  // (2304 * pclks * 1655 + 500) / 1000
  // max pclks=18: 2304*18=41472, 41472*1655=68636160, +500/1000=68636 — fits
  return (2304 * pclks * 1655 + 500) / 1000;
}

// ── Timeout encode/decode ────────────────────────────

int decodeTimeout(int rv) {
  int lsb = rv & 255;
  int msb = rv / 256;
  int i = 0;
  while (i < msb) { lsb = lsb * 2; i = i + 1; }
  return lsb + 1;
}

int encodeTimeout(int mclks) {
  if (mclks <= 0) { return 0; }
  int ls = mclks - 1;
  int ms = 0;
  while (ls > 255) { ls = ls / 2; ms = ms + 1; }
  return ms * 256 + (ls & 255);
}

// convert mclks to microseconds — overflow-safe
int mclksToUs(int mclks, int pclks) {
  int mns = macroPeriodNs(pclks);
  // (mclks * mns + mns/2) / 1000 — could overflow for large mclks
  // use: mclks * (mns/10) / 100 for safety
  return (mclks * (mns / 10) + 50) / 100;
}

int usToMclks(int us, int pclks) {
  int mns = macroPeriodNs(pclks);
  return (us * 1000 + mns / 2) / mns;
}

// ── Get sequence step enables + timeouts ─────────────
// stored in local vars via pointers not available, use globals

int en_tcc;
int en_dss;
int en_msrc;
int en_pre;
int en_final;
int pre_pclks;
int final_pclks;
int msrc_mclks;
int pre_mclks;
int final_mclks;
int msrc_us;
int pre_us;
int final_us;

void getEnablesAndTimeouts() {
  int sc = rd(SYSTEM_SEQUENCE_CONFIG);
  en_tcc   = (sc / 16) & 1;
  en_dss   = (sc / 8) & 1;
  en_msrc  = (sc / 4) & 1;
  en_pre   = (sc / 64) & 1;
  en_final = (sc / 128) & 1;

  pre_pclks = decodeVcsel(rd(PRE_RANGE_CONFIG_VCSEL_PERIOD));
  msrc_mclks = rd(MSRC_CONFIG_TIMEOUT_MACROP) + 1;
  msrc_us = mclksToUs(msrc_mclks, pre_pclks);
  pre_mclks = decodeTimeout(rd16(PRE_RANGE_CONFIG_TIMEOUT_HI));
  pre_us = mclksToUs(pre_mclks, pre_pclks);

  final_pclks = decodeVcsel(rd(FINAL_RANGE_CONFIG_VCSEL_PERIOD));
  final_mclks = decodeTimeout(rd16(FINAL_RANGE_CONFIG_TIMEOUT_HI));
  if (en_pre) { final_mclks = final_mclks - pre_mclks; }
  final_us = mclksToUs(final_mclks, final_pclks);
}

// ── Get measurement timing budget (microseconds) ────

int getBudget() {
  int b = 1910 + 960;
  getEnablesAndTimeouts();
  if (en_tcc) { b = b + msrc_us + 590; }
  if (en_dss) { b = b + 2 * (msrc_us + 690); }
  else { if (en_msrc) { b = b + msrc_us + 660; } }
  if (en_pre) { b = b + pre_us + 660; }
  if (en_final) { b = b + final_us + 550; }
  return b;
}

// ── Set measurement timing budget ────────────────────

int setBudget(int budget) {
  if (budget < 20000) { return 0; }
  int used = 1320 + 960;
  getEnablesAndTimeouts();
  if (en_tcc) { used = used + msrc_us + 590; }
  if (en_dss) { used = used + 2 * (msrc_us + 690); }
  else { if (en_msrc) { used = used + msrc_us + 660; } }
  if (en_pre) { used = used + pre_us + 660; }
  if (en_final) {
    used = used + 550;
    if (used > budget) { return 0; }
    int fr_us = budget - used;
    int fr_mclks = usToMclks(fr_us, final_pclks);
    if (en_pre) { fr_mclks = fr_mclks + pre_mclks; }
    wr16(FINAL_RANGE_CONFIG_TIMEOUT_HI, encodeTimeout(fr_mclks));
    vl_budget_us = budget;
  }
  return 1;
}

// ── Single reference calibration ─────────────────────

int singleRefCal(int vhv_init) {
  wr(SYSRANGE_START, 1 | vhv_init);
  startTO();
  while ((rd(RESULT_INTERRUPT_STATUS) & 7) == 0) {
    if (timedOut()) {
      addLog("VL53L0X: cal timeout");
      return 0;
    }
  }
  wr(SYSTEM_INTERRUPT_CLEAR, 1);
  wr(SYSRANGE_START, 0);
  return 1;
}

// ── Get SPAD info from NVM ───────────────────────────

int spad_count;
int spad_aperture;

int getSpadInfo() {
  wr(0x80, 0x01);
  wr(0xFF, 0x01);
  wr(0x00, 0x00);
  wr(0xFF, 0x06);
  wr(0x83, rd(0x83) | 0x04);
  wr(0xFF, 0x07);
  wr(0x81, 0x01);
  wr(0x80, 0x01);
  wr(0x94, 0x6B);
  wr(0x83, 0x00);
  startTO();
  while (rd(0x83) == 0) {
    if (timedOut()) { return 0; }
  }
  wr(0x83, 0x01);
  int tmp = rd(0x92);
  spad_count = tmp & 127;
  spad_aperture = tmp / 128;
  wr(0x81, 0x00);
  wr(0xFF, 0x06);
  wr(0x83, rd(0x83) & 251);
  wr(0xFF, 0x01);
  wr(0x00, 0x01);
  wr(0xFF, 0x00);
  wr(0x80, 0x00);
  return 1;
}

// ── Full init — follows plugin VL53L0X_init(true) exactly ──

int vlInit() {
  // check model ID
  int id = rd(IDENTIFICATION_MODEL_ID);
  if (id != VL_ID) { return 0; }

  // ── DataInit ──
  // 2V8 mode
  wr(VHV_CONFIG_PAD_SCL_SDA_HV, rd(VHV_CONFIG_PAD_SCL_SDA_HV) | 0x01);

  // "Set I2C standard mode"
  wr(0x88, 0x00);

  wr(0x80, 0x01);
  wr(0xFF, 0x01);
  wr(0x00, 0x00);
  vl_stop_var = rd(0x91);
  wr(0x00, 0x01);
  wr(0xFF, 0x00);
  wr(0x80, 0x00);

  // disable SIGNAL_RATE_MSRC and PRE_RANGE limit checks
  wr(MSRC_CONFIG_CONTROL, rd(MSRC_CONFIG_CONTROL) | 0x12);

  // signal rate limit 0.25 MCPS: 0.25 * 128 = 32 in Q9.7
  wr16(FINAL_RANGE_CONFIG_MIN_COUNT_RATE, 32);

  wr(SYSTEM_SEQUENCE_CONFIG, 0xFF);

  // ── StaticInit ──

  // SPAD configuration
  if (!getSpadInfo()) { return 0; }

  // read SPAD map
  char spad[6];
  i2cRead(VL_ADDR, GLOBAL_CONFIG_SPAD_ENABLES_REF_0, spad, 6, vl_bus);

  // set reference SPADs
  wr(0xFF, 0x01);
  wr(DYNAMIC_SPAD_REF_EN_START, 0x00);
  wr(DYNAMIC_SPAD_NUM_REQ, 0x2C);
  wr(0xFF, 0x00);
  wr(GLOBAL_CONFIG_REF_EN_START_SEL, 0xB4);

  int first = 0;
  if (spad_aperture) { first = 12; }
  int enabled = 0;
  int i = 0;
  while (i < 48) {
    int bi = i / 8;
    int bit = i - bi * 8;
    // compute mask = 1 << bit
    int mask = 1;
    int j = 0;
    while (j < bit) { mask = mask * 2; j = j + 1; }
    if (i < first || enabled == spad_count) {
      spad[bi] = spad[bi] & (255 - mask);
    } else {
      if (spad[bi] & mask) { enabled = enabled + 1; }
    }
    i = i + 1;
  }
  i2cWrite(VL_ADDR, GLOBAL_CONFIG_SPAD_ENABLES_REF_0, spad, 6, vl_bus);

  // ── Load tuning settings ──
  wr(0xFF, 0x01); wr(0x00, 0x00);
  wr(0xFF, 0x00); wr(0x09, 0x00); wr(0x10, 0x00); wr(0x11, 0x00);
  wr(0x24, 0x01); wr(0x25, 0xFF); wr(0x75, 0x00);
  wr(0xFF, 0x01); wr(0x4E, 0x2C); wr(0x48, 0x00); wr(0x30, 0x20);
  wr(0xFF, 0x00);
  wr(0x30, 0x09); wr(0x54, 0x00); wr(0x31, 0x04); wr(0x32, 0x03);
  wr(0x40, 0x83); wr(0x46, 0x25); wr(0x60, 0x00); wr(0x27, 0x00);
  wr(0x50, 0x06); wr(0x51, 0x00); wr(0x52, 0x96); wr(0x56, 0x08);
  wr(0x57, 0x30); wr(0x61, 0x00); wr(0x62, 0x00); wr(0x64, 0x00);
  wr(0x65, 0x00); wr(0x66, 0xA0);
  wr(0xFF, 0x01);
  wr(0x22, 0x32); wr(0x47, 0x14); wr(0x49, 0xFF); wr(0x4A, 0x00);
  wr(0xFF, 0x00);
  wr(0x7A, 0x0A); wr(0x7B, 0x00); wr(0x78, 0x21);
  wr(0xFF, 0x01);
  wr(0x23, 0x34); wr(0x42, 0x00); wr(0x44, 0xFF); wr(0x45, 0x26);
  wr(0x46, 0x05); wr(0x40, 0x40); wr(0x0E, 0x06); wr(0x20, 0x1A);
  wr(0x43, 0x40);
  wr(0xFF, 0x00);
  wr(0x34, 0x03); wr(0x35, 0x44);
  wr(0xFF, 0x01);
  wr(0x31, 0x04); wr(0x4B, 0x09); wr(0x4C, 0x05); wr(0x4D, 0x04);
  wr(0xFF, 0x00);
  wr(0x44, 0x00); wr(0x45, 0x20); wr(0x47, 0x08); wr(0x48, 0x28);
  wr(0x67, 0x00); wr(0x70, 0x04); wr(0x71, 0x01); wr(0x72, 0xFE);
  wr(0x76, 0x00); wr(0x77, 0x00);
  wr(0xFF, 0x01); wr(0x0D, 0x01);
  wr(0xFF, 0x00); wr(0x80, 0x01); wr(0x01, 0xF8);
  wr(0xFF, 0x01);
  wr(0x8E, 0x01); wr(0x00, 0x01); wr(0xFF, 0x00); wr(0x80, 0x00);

  // ── GPIO config: new sample ready, active low ──
  wr(SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
  wr(GPIO_HV_MUX_ACTIVE_HIGH, rd(GPIO_HV_MUX_ACTIVE_HIGH) & 239);
  wr(SYSTEM_INTERRUPT_CLEAR, 0x01);

  // ── Timing budget: get, change sequence, re-apply ──
  // THIS IS CRITICAL — without it, calibration fails
  vl_budget_us = getBudget();

  // disable MSRC and TCC
  wr(SYSTEM_SEQUENCE_CONFIG, 0xE8);

  // re-apply budget with new sequence config
  setBudget(vl_budget_us);

  // ── VHV calibration ──
  wr(SYSTEM_SEQUENCE_CONFIG, 0x01);
  if (!singleRefCal(0x40)) { return 0; }

  // ── Phase calibration ──
  wr(SYSTEM_SEQUENCE_CONFIG, 0x02);
  if (!singleRefCal(0x00)) { return 0; }

  // restore sequence config
  wr(SYSTEM_SEQUENCE_CONFIG, 0xE8);

  return 1;
}

// ── Read single-shot range (mm) ──────────────────────

int readRange() {
  wr(0x80, 0x01);
  wr(0xFF, 0x01);
  wr(0x00, 0x00);
  wr(0x91, vl_stop_var);
  wr(0x00, 0x01);
  wr(0xFF, 0x00);
  wr(0x80, 0x00);

  wr(SYSRANGE_START, 0x01);

  // wait start bit cleared
  startTO();
  while (rd(SYSRANGE_START) & 1) {
    if (timedOut()) { return 65535; }
  }

  // wait measurement complete
  startTO();
  while ((rd(RESULT_INTERRUPT_STATUS) & 7) == 0) {
    if (timedOut()) { return 65535; }
  }

  // range at RESULT_RANGE_STATUS + 10 = 0x1E
  int range = rd16(0x1E);

  wr(SYSTEM_INTERRUPT_CLEAR, 0x01);

  return range;
}

// ── Callbacks ────────────────────────────────────────

void EverySecond() {
  if (!vl_ok) { return; }

  int dist = readRange();

  if (dist < 8190) {
    vl_range = (float)dist;
    vl_valid = 1;
  } else {
    vl_valid = 0;
  }
}

void WebCall() {
  if (!vl_ok) { return; }
  char buf[64];
  if (vl_valid) {
    sprintf(buf, "{s}VL53L0X{m}%.0f mm{e}", vl_range);
  } else {
    strcpy(buf, "{s}VL53L0X{m}---{e}");
  }
  webSend(buf);
}

void JsonCall() {
  if (!vl_ok) { return; }
  char buf[64];
  if (vl_valid) {
    sprintf(buf, ",\\"VL53L0X\\":{\\"Distance\\":%.0f}", vl_range);
  } else {
    strcpy(buf, ",\\"VL53L0X\\":{\\"Distance\\":null}");
  }
  responseAppend(buf);
}

// ── Command handler ───────────────────────────────────
// VLMode 0 = default (33ms), 1 = fast (20ms), 2 = accurate (200ms)
// VLBudget <us> = set custom timing budget in microseconds
// VLStatus = show current settings

void Command(char cmd[]) {
  char buf[80];
  char arg[16];
  int val;

  if (strFind(cmd, "MODE") == 0) {
    if (strlen(cmd) > 4) { strSub(arg, cmd, 4, 0); val = atoi(arg); }
    else { val = 0; }
    int b = 33000;
    if (val == 1) { b = 20000; }
    if (val == 2) { b = 200000; }
    if (setBudget(b)) {
      sprintf(buf, "{\\"VLMode\\":%d}", val);
      responseCmnd(buf);
    } else {
      responseCmnd("{\\"VLMode\\":\\"failed\\"}");
    }
  }
  else if (strFind(cmd, "BUDGET") == 0) {
    if (strlen(cmd) > 6) { strSub(arg, cmd, 6, 0); val = atoi(arg); }
    else { val = 33000; }
    if (val < 20000) { val = 20000; }
    if (setBudget(val)) {
      sprintf(buf, "{\\"VLBudget\\":%d}", vl_budget_us);
      responseCmnd(buf);
    } else {
      responseCmnd("{\\"VLBudget\\":\\"failed\\"}");
    }
  }
  else if (strFind(cmd, "STATUS") == 0) {
    sprintf(buf, "{\\"VLStatus\\":{\\"Budget\\":%d", vl_budget_us);
    if (vl_valid) {
      sprintf(arg, ",\\"Range\\":%.0f", vl_range);
    } else {
      strcpy(arg, ",\\"Range\\":null");
    }
    strcat(buf, arg);
    sprintf(arg, ",\\"Bus\\":%d}}", vl_bus);
    strcat(buf, arg);
    responseCmnd(buf);
  }
  else {
    responseCmnd("{\\"VL\\":\\"commands: VLMode 0|1|2, VLBudget <us>, VLStatus\\"}");
  }
}

// ── Main ─────────────────────────────────────────────

void OnExit() {
  if (vl_ok) {
    I2cResetActive(VL_ADDR, vl_bus);
  }
}

int main() {
  vl_ok = 0;
  vl_valid = 0;
  vl_bus = 0;

  addCommand("VL");

  // scan both buses
  while (vl_bus < 2) {
    if (i2cSetDevice(VL_ADDR, vl_bus)) {
      if (rd(IDENTIFICATION_MODEL_ID) == VL_ID) {
        i2cSetActiveFound(VL_ADDR, "VL53L0X", vl_bus);
        break;
      }
    }
    vl_bus = vl_bus + 1;
  }
  if (vl_bus >= 2) {
    addLog("VL53L0X: not found");
    return 0;
  }

  if (vlInit()) {
    vl_ok = 1;
    addLog("VL53L0X: init OK");
  } else {
    addLog("VL53L0X: init FAILED");
  }

  return 0;
}`,
            mlx90614: `// MLX90614 Infrared Non-Contact Thermometer Driver
// I2C address: 0x5A, no initialization needed
// Reads object temperature (0x07) and ambient temperature (0x06)
// SMBus protocol: LSB-first, 3 bytes per read (LSB, MSB, PEC)
// PEC = CRC-8 with polynomial 0x07

#define MLX_ADDR   0x5A
#define MLX_TA     0x06
#define MLX_TOBJ1  0x07

float mlx_obj = 0.0;
float mlx_amb = 0.0;
int mlx_ok = 0;
int mlx_addr = 0;
int mlx_bus = 0;

char mlx_buf[6];

// CRC-8 for SMBus PEC (polynomial 0x07, init 0x00)
int mlx_crc8(int start, int len) {
    int crc = 0;
    int i = 0;
    while (i < len) {
        crc = crc ^ mlx_buf[start + i];
        int bit = 0;
        while (bit < 8) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
            crc = crc & 0xFF;
            bit++;
        }
        i++;
    }
    return crc;
}

// Read 16-bit value from register with PEC verification
// Returns raw value or -1 on error
int mlx_read16(int reg) {
    if (!i2cReadRS(mlx_addr, reg, mlx_buf, 3, mlx_bus)) {
        return -1;
    }

    int lsb = mlx_buf[0];
    int msb = mlx_buf[1];
    int pec = mlx_buf[2];

    // Build PEC check buffer: [addr_W, reg, addr_R, LSB, MSB]
    mlx_buf[0] = mlx_addr << 1;        // address + write bit
    mlx_buf[1] = reg;                   // register
    mlx_buf[2] = (mlx_addr << 1) | 1;  // address + read bit
    mlx_buf[3] = lsb;
    mlx_buf[4] = msb;

    int cpec = mlx_crc8(0, 5);
    if (cpec != pec) {
        return -1;  // CRC mismatch
    }

    int raw = lsb | (msb << 8);
    if (raw & 0x8000) {
        return -1;  // error flag
    }
    return raw;
}

int mlx_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(MLX_ADDR, bus)) {
            mlx_addr = MLX_ADDR;
            mlx_bus = bus;
            i2cSetActiveFound(mlx_addr, "MLX90614", mlx_bus);
            return 1;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!mlx_addr) {
        if (!mlx_scan()) { mlx_ok = 0; return; }
    }

    // Read object temperature (register 0x07)
    int raw = mlx_read16(MLX_TOBJ1);
    if (raw < 0) {
        mlx_ok = 0;
        return;
    }
    mlx_obj = (float)raw * 0.02 - 273.15;

    // Read ambient temperature (register 0x06)
    raw = mlx_read16(MLX_TA);
    if (raw < 0) {
        mlx_ok = 0;
        return;
    }
    mlx_amb = (float)raw * 0.02 - 273.15;

    mlx_ok = 1;
}

void WebCall() {
    char buf[64];
    if (mlx_ok) {
        sprintf(buf, "{s}MLX90614 Object{m}%.1f °C{e}", mlx_obj);
        webSend(buf);
        sprintf(buf, "{s}MLX90614 Ambient{m}%.1f °C{e}", mlx_amb);
        webSend(buf);
    } else {
        webSend("{s}MLX90614{m}no data{e}");
    }
}

void JsonCall() {
    if (!mlx_ok) return;
    char buf[96];
    sprintf(buf, ",\\"MLX90614\\":{\\"OBJTMP\\":%.1f", mlx_obj);
    responseAppend(buf);
    sprintf(buf, ",\\"AMBTMP\\":%.1f}", mlx_amb);
    responseAppend(buf);
}

void OnExit() {
    if (mlx_addr) {
        I2cResetActive(mlx_addr, mlx_bus);
        mlx_addr = 0;
    }
}

int main() {
    char buf[48];
    mlx_ok = 0;
    mlx_addr = 0;

    if (mlx_scan()) {
        sprintf(buf, "MLX90614 found at 0x%x on bus %d", mlx_addr, mlx_bus);
        addLog(buf);
    } else {
        addLog("MLX90614 not found");
    }
    return 0;
}`,
            tcs34725: `// TCS34725 RGB Color Sensor Driver
// I2C address: 0x29, requires initialization
// Reads RGBC (Red, Green, Blue, Clear) + calculates Lux and Color Temperature
// DN40 algorithm for lux/CT calculation
// Command bit 0x80 must be OR'd with register address
// Auto-increment 0xA0 for block reads

#define TCS_ADDR    0x29
#define TCS_CMD     0x80
#define TCS_AUTOINC 0xA0

// Registers
#define TCS_ENABLE  0x00
#define TCS_ATIME   0x01
#define TCS_CONTROL 0x0F
#define TCS_ID      0x12
#define TCS_STATUS  0x13
#define TCS_CDATAL  0x14

// Integration time 154ms (0xC0) — good range, max ~65535 counts
#define TCS_ATIME_VAL  0xC0
#define TCS_ATIME_MS   154

// Gain 4X
#define TCS_GAIN_VAL   0x01
#define TCS_GAIN_FACT  4

int tcs_addr = 0;
int tcs_bus = 0;
int tcs_ok = 0;
int tcs_r = 0;
int tcs_g = 0;
int tcs_b = 0;
int tcs_c = 0;
float tcs_lux = 0.0;
float tcs_ct = 0.0;

char tcs_buf[8];

int tcs_init() {
    // Verify chip ID (0x44 = TCS34725, 0x10 = TCS34721)
    int id = i2cRead8(tcs_addr, TCS_CMD | TCS_ID, tcs_bus);
    if (id != 0x44 && id != 0x10) {
        return 0;
    }

    // Power ON (PON bit)
    i2cWrite8(tcs_addr, TCS_CMD | TCS_ENABLE, 0x01, tcs_bus);
    delay(3);

    // Set integration time
    i2cWrite8(tcs_addr, TCS_CMD | TCS_ATIME, TCS_ATIME_VAL, tcs_bus);

    // Set gain
    i2cWrite8(tcs_addr, TCS_CMD | TCS_CONTROL, TCS_GAIN_VAL, tcs_bus);

    // Enable RGBC (PON | AEN)
    i2cWrite8(tcs_addr, TCS_CMD | TCS_ENABLE, 0x03, tcs_bus);

    return 1;
}

int tcs_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(TCS_ADDR, bus)) {
            tcs_addr = TCS_ADDR;
            tcs_bus = bus;
            if (tcs_init()) {
                i2cSetActiveFound(tcs_addr, "TCS34725", tcs_bus);
                return 1;
            }
            tcs_addr = 0;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!tcs_addr) {
        if (!tcs_scan()) { tcs_ok = 0; return; }
    }

    // Check AVALID bit (bit 0) in STATUS register
    int status = i2cRead8(tcs_addr, TCS_CMD | TCS_STATUS, tcs_bus);
    if (!(status & 0x01)) {
        return;  // conversion not ready
    }

    // Block read 8 bytes: C_L, C_H, R_L, R_H, G_L, G_H, B_L, B_H
    if (!i2cReadRS(tcs_addr, TCS_AUTOINC | TCS_CDATAL, tcs_buf, 8, tcs_bus)) {
        tcs_ok = 0;
        return;
    }

    tcs_c = tcs_buf[0] | (tcs_buf[1] << 8);
    tcs_r = tcs_buf[2] | (tcs_buf[3] << 8);
    tcs_g = tcs_buf[4] | (tcs_buf[5] << 8);
    tcs_b = tcs_buf[6] | (tcs_buf[7] << 8);

    // DN40 lux and color temperature calculation
    if (tcs_c > 0 && tcs_r > 0) {
        // IR estimate
        int ir = (tcs_r + tcs_g + tcs_b - tcs_c) / 2;
        if (ir < 0) ir = 0;

        // CPL = (atime_ms * gain) / (GA * DF)
        // GA=1.0, DF=310.0, with 154ms and 4X: CPL = 1.987
        float cpl = (float)(TCS_ATIME_MS * TCS_GAIN_FACT) / 310.0;

        // Lux (DN40 coefficients)
        float r_comp = (float)(tcs_r - ir);
        float g_comp = (float)(tcs_g - ir);
        float b_comp = (float)(tcs_b - ir);

        tcs_lux = (0.136 * r_comp + 1.0 * g_comp - 0.444 * b_comp) / cpl;
        if (tcs_lux < 0.0) tcs_lux = 0.0;

        // Color temperature (Kelvin)
        if (r_comp > 0.0) {
            tcs_ct = 3810.0 * b_comp / r_comp + 1391.0;
        } else {
            tcs_ct = 0.0;
        }
    } else {
        tcs_lux = 0.0;
        tcs_ct = 0.0;
    }

    tcs_ok = 1;
}

void WebCall() {
    char buf[80];
    if (tcs_ok) {
        sprintf(buf, "{s}TCS34725 Red{m}%d{e}", tcs_r);
        webSend(buf);
        sprintf(buf, "{s}TCS34725 Green{m}%d{e}", tcs_g);
        webSend(buf);
        sprintf(buf, "{s}TCS34725 Blue{m}%d{e}", tcs_b);
        webSend(buf);
        sprintf(buf, "{s}TCS34725 Clear{m}%d{e}", tcs_c);
        webSend(buf);
        sprintf(buf, "{s}TCS34725 Lux{m}%.0f lx{e}", tcs_lux);
        webSend(buf);
        sprintf(buf, "{s}TCS34725 CT{m}%.0f K{e}", tcs_ct);
        webSend(buf);
    } else {
        webSend("{s}TCS34725{m}no data{e}");
    }
}

void JsonCall() {
    if (!tcs_ok) return;
    char buf[96];
    sprintf(buf, ",\\"TCS34725\\":{\\"Red\\":%d", tcs_r);
    responseAppend(buf);
    sprintf(buf, ",\\"Green\\":%d", tcs_g);
    responseAppend(buf);
    sprintf(buf, ",\\"Blue\\":%d", tcs_b);
    responseAppend(buf);
    sprintf(buf, ",\\"Clear\\":%d", tcs_c);
    responseAppend(buf);
    sprintf(buf, ",\\"Lux\\":%.0f", tcs_lux);
    responseAppend(buf);
    sprintf(buf, ",\\"CT\\":%.0f}", tcs_ct);
    responseAppend(buf);
}

void OnExit() {
    if (tcs_addr) {
        // Power off sensor
        i2cWrite8(tcs_addr, TCS_CMD | TCS_ENABLE, 0x00, tcs_bus);
        I2cResetActive(tcs_addr, tcs_bus);
        tcs_addr = 0;
    }
}

int main() {
    char buf[48];
    tcs_ok = 0;
    tcs_addr = 0;

    if (tcs_scan()) {
        sprintf(buf, "TCS34725 found at 0x%x on bus %d", tcs_addr, tcs_bus);
        addLog(buf);
    } else {
        addLog("TCS34725 not found");
    }
    return 0;
}`,
            veml6075: `// VEML6075 UV Light Sensor Driver
// I2C address: 0x10, 16-bit registers (LSB first)
// Reads UVA, UVB with compensation and calculates UV Index
// Compensation removes visible light and IR interference
// DN40 responsivity for UV Index calculation

#define VEML_ADDR    0x10
#define VEML_CONF    0x00
#define VEML_UVA     0x07
#define VEML_UVB     0x09
#define VEML_UVCOMP1 0x0A
#define VEML_UVCOMP2 0x0B
#define VEML_ID      0x0C

int veml_addr = 0;
int veml_bus = 0;
int veml_ok = 0;
int veml_uva = 0;
int veml_uvb = 0;
float veml_uvi = 0.0;

char veml_buf[2];

// Read 16-bit register (LSB first)
int veml_read16(int reg) {
    if (!i2cReadRS(veml_addr, reg, veml_buf, 2, veml_bus)) {
        return -1;
    }
    return veml_buf[0] | (veml_buf[1] << 8);
}

// Write 16-bit register (LSB first)
int veml_write16(int reg, int val) {
    veml_buf[0] = val & 0xFF;
    veml_buf[1] = (val >> 8) & 0xFF;
    return i2cWrite(veml_addr, reg, veml_buf, 2, veml_bus);
}

int veml_init() {
    // Verify chip ID (expect 0x0026)
    int id = veml_read16(VEML_ID);
    if (id != 0x26) {
        return 0;
    }
    // Config: 100ms integration (inttime=1 in bits 6:4), active mode
    veml_write16(VEML_CONF, 0x10);
    return 1;
}

int veml_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(VEML_ADDR, bus)) {
            veml_addr = VEML_ADDR;
            veml_bus = bus;
            if (veml_init()) {
                i2cSetActiveFound(veml_addr, "VEML6075", veml_bus);
                return 1;
            }
            veml_addr = 0;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!veml_addr) {
        if (!veml_scan()) { veml_ok = 0; return; }
    }

    int uva_raw = veml_read16(VEML_UVA);
    int uvb_raw = veml_read16(VEML_UVB);
    int comp1   = veml_read16(VEML_UVCOMP1);
    int comp2   = veml_read16(VEML_UVCOMP2);

    if (uva_raw < 0 || uvb_raw < 0 || comp1 < 0 || comp2 < 0) {
        veml_ok = 0;
        return;
    }

    // Compensate for visible light and IR interference
    float uva_comp = (float)uva_raw - 2.22 * (float)comp1 - 1.33 * (float)comp2;
    float uvb_comp = (float)uvb_raw - 2.95 * (float)comp1 - 1.74 * (float)comp2;
    if (uva_comp < 0.0) uva_comp = 0.0;
    if (uvb_comp < 0.0) uvb_comp = 0.0;

    veml_uva = (int)uva_comp;
    veml_uvb = (int)uvb_comp;

    // UV Index (100ms responsivity: UVA=0.001461, UVB=0.002591)
    veml_uvi = (uva_comp * 0.001461 + uvb_comp * 0.002591) / 2.0;

    veml_ok = 1;
}

void WebCall() {
    char buf[64];
    if (veml_ok) {
        sprintf(buf, "{s}VEML6075 UVA{m}%d{e}", veml_uva);
        webSend(buf);
        sprintf(buf, "{s}VEML6075 UVB{m}%d{e}", veml_uvb);
        webSend(buf);
        sprintf(buf, "{s}VEML6075 UV Index{m}%.1f{e}", veml_uvi);
        webSend(buf);
    } else {
        webSend("{s}VEML6075{m}no data{e}");
    }
}

void JsonCall() {
    if (!veml_ok) return;
    char buf[96];
    sprintf(buf, ",\\"VEML6075\\":{\\"UVA\\":%d", veml_uva);
    responseAppend(buf);
    sprintf(buf, ",\\"UVB\\":%d", veml_uvb);
    responseAppend(buf);
    sprintf(buf, ",\\"UVI\\":%.1f}", veml_uvi);
    responseAppend(buf);
}

void OnExit() {
    if (veml_addr) {
        // Shutdown sensor (set pwr bit)
        veml_write16(VEML_CONF, 0x01);
        I2cResetActive(veml_addr, veml_bus);
        veml_addr = 0;
    }
}

int main() {
    char buf[48];
    veml_ok = 0;
    veml_addr = 0;

    if (veml_scan()) {
        sprintf(buf, "VEML6075 found at 0x%x on bus %d", veml_addr, veml_bus);
        addLog(buf);
    } else {
        addLog("VEML6075 not found");
    }
    return 0;
}`,
            scd30: `// SCD30 CO2/Temperature/Humidity Sensor Driver
// I2C address: 0x61
// Measures: CO2 (ppm), Temperature (C), Humidity (%RH)
// Data as IEEE754 floats, CRC8 poly 0x31, init 0xFF (Sensirion)
// Continuous measurement mode, interval 2s

int scd_addr = 0;
int scd_bus = 0;
int scd_ok = 0;
int scd_retry = 0;
float scd_co2 = 0.0;
float scd_temp = 0.0;
float scd_humi = 0.0;
float scd_dewp = 0.0;
float scd_absh = 0.0;
char scd_buf[20];
char scd_lbl[32];

// CRC8 Sensirion: poly 0x31, init 0xFF, over 2 data bytes
int scd_crc(int b1, int b2) {
    int crc = 0xFF ^ b1;
    int j = 0;
    while (j < 8) {
        if (crc & 0x80) crc = ((crc << 1) ^ 0x31) & 0xFF;
        else crc = (crc << 1) & 0xFF;
        j++;
    }
    crc = crc ^ b2;
    j = 0;
    while (j < 8) {
        if (crc & 0x80) crc = ((crc << 1) ^ 0x31) & 0xFF;
        else crc = (crc << 1) & 0xFF;
        j++;
    }
    return crc;
}

// Send 16-bit command (no data)
void scd_cmd(int cmd) {
    scd_buf[0] = cmd & 0xFF;
    i2cWrite(scd_addr, cmd >> 8, scd_buf, 1, scd_bus);
}

// Send 16-bit command with 16-bit argument + CRC
void scd_cmd_arg(int cmd, int arg) {
    scd_buf[0] = cmd & 0xFF;
    scd_buf[1] = arg >> 8;
    scd_buf[2] = arg & 0xFF;
    scd_buf[3] = scd_crc(scd_buf[1], scd_buf[2]);
    i2cWrite(scd_addr, cmd >> 8, scd_buf, 4, scd_bus);
}

// Reconstruct IEEE754 float from 6-byte response (2 words + 2 CRCs)
// Layout: [hi_MSB, hi_LSB, CRC, lo_MSB, lo_LSB, CRC]
// Returns float via intBitsToFloat, or 0.0 on CRC error
float scd_get_float(int offset) {
    int hi_msb = scd_buf[offset];
    int hi_lsb = scd_buf[offset + 1];
    int lo_msb = scd_buf[offset + 3];
    int lo_lsb = scd_buf[offset + 4];
    // Validate both CRCs
    if (scd_crc(hi_msb, hi_lsb) != scd_buf[offset + 2]) return 0.0;
    if (scd_crc(lo_msb, lo_lsb) != scd_buf[offset + 5]) return 0.0;
    int bits = (hi_msb << 24) | (hi_lsb << 16) | (lo_msb << 8) | lo_lsb;
    return intBitsToFloat(bits);
}

// Dewpoint (Magnus formula), returns °C
float scd_calc_dewpoint(float t, float h) {
    if (h <= 0.0) return 0.0;
    float gamma = (17.271 * t) / (237.7 + t) + log(h / 100.0);
    return (237.7 * gamma) / (17.271 - gamma);
}

// Absolute humidity in g/m³
float scd_calc_abshumi(float t, float h) {
    float ah = 6.112 * exp((17.67 * t) / (t + 243.5)) * h * 2.1674;
    return ah / (273.15 + t);
}

int scd_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(0x61, bus)) {
            scd_addr = 0x61;
            scd_bus = bus;
            // Get firmware version to verify sensor (cmd 0xD100)
            scd_cmd(0xD100);
            delay(3);
            if (i2cRead0(scd_addr, scd_buf, 3, scd_bus)) {
                if (scd_crc(scd_buf[0], scd_buf[1]) == scd_buf[2]) {
                    // Set measurement interval to 2 seconds (cmd 0x4600, arg 2)
                    scd_cmd_arg(0x4600, 2);
                    delay(3);
                    // Start continuous measurement, no pressure compensation (cmd 0x0010, arg 0)
                    scd_cmd_arg(0x0010, 0);
                    delay(3);
                    i2cSetActiveFound(scd_addr, "SCD30", scd_bus);
                    return 1;
                }
            }
            scd_addr = 0;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!scd_addr) {
        // Retry scan every 5 seconds if sensor wasn't found at boot
        scd_retry++;
        if (scd_retry >= 5) {
            scd_retry = 0;
            if (scd_scan()) {
                char buf[48];
                sprintf(buf, "SCD30 found at 0x%x on bus %d (retry)", scd_addr, scd_bus);
                addLog(buf);
            }
        }
        return;
    }

    // Check data ready (cmd 0x0202)
    scd_cmd(0x0202);
    delay(3);
    if (!i2cRead0(scd_addr, scd_buf, 3, scd_bus)) return;
    if (scd_crc(scd_buf[0], scd_buf[1]) != scd_buf[2]) return;
    int ready = (scd_buf[0] << 8) | scd_buf[1];
    if (!ready) return;

    // Read measurement (cmd 0x0300) — 18 bytes: 3 floats × 6 bytes each
    scd_cmd(0x0300);
    delay(3);
    if (!i2cRead0(scd_addr, scd_buf, 18, scd_bus)) return;

    float co2 = scd_get_float(0);
    float temp = scd_get_float(6);
    float humi = scd_get_float(12);

    // Sanity check CO2 (valid range ~400-10000 ppm)
    if (co2 > 0.0) {
        scd_co2 = co2;
        scd_temp = temp;
        scd_humi = humi;
        scd_dewp = scd_calc_dewpoint(temp, humi);
        scd_absh = scd_calc_abshumi(temp, humi);
        scd_ok = 1;
    }
}

void scd_web_label(int idx) {
    char vt[32];
    LGetString(idx, scd_lbl);
    strcpy(vt, "{s}SCD30 ");
    strcat(vt, scd_lbl);
    strcat(vt, "{m}");
    webSend(vt);
}

void WebCall() {
    char vt[32];
    if (scd_ok) {
        scd_web_label(4);
        sprintf(vt, "%.0f ppm{e}", scd_co2);
        webSend(vt);
        scd_web_label(0);
        sprintf(vt, "%.1f &deg;C{e}", scd_temp);
        webSend(vt);
        scd_web_label(1);
        sprintf(vt, "%.1f %{e}", scd_humi);
        webSend(vt);
        scd_web_label(3);
        sprintf(vt, "%.1f &deg;C{e}", scd_dewp);
        webSend(vt);
        scd_web_label(20);
        sprintf(vt, "%.1f g/m&sup3;{e}", scd_absh);
        webSend(vt);
    } else {
        webSend("{s}SCD30{m}not ready{e}");
    }
}

void JsonCall() {
    if (!scd_ok) return;
    char buf[64];
    sprintf(buf, ",\\"SCD30\\":{\\"CarbonDioxide\\":%.0f", scd_co2);
    responseAppend(buf);
    sprintf(buf, ",\\"Temperature\\":%.1f", scd_temp);
    responseAppend(buf);
    sprintf(buf, ",\\"Humidity\\":%.1f", scd_humi);
    responseAppend(buf);
    sprintf(buf, ",\\"DewPoint\\":%.1f", scd_dewp);
    responseAppend(buf);
    sprintf(buf, ",\\"AbsHumidity\\":%.1f}", scd_absh);
    responseAppend(buf);
}

void OnExit() {
    if (scd_addr) {
        // Stop continuous measurement (cmd 0x0104)
        scd_cmd(0x0104);
        delay(3);
        I2cResetActive(scd_addr, scd_bus);
    }
}

int main() {
    scd_ok = 0;
    scd_addr = 0;

    if (scd_scan()) {
        char buf[48];
        sprintf(buf, "SCD30 found at 0x%x on bus %d", scd_addr, scd_bus);
        addLog(buf);
    } else {
        addLog("SCD30 not found");
    }
    return 0;
}`,
            sgp30: `// SGP30 VOC/eCO2 Sensor Driver
// I2C address: 0x58
// Measures: eCO2 (ppm), TVOC (ppb)
// CRC8: poly 0x31, init 0xFF (Sensirion)
// Must measure every second for IAQ baseline algorithm

int sgp_addr = 0;
int sgp_bus = 0;
int sgp_ok = 0;
int sgp_eco2 = 0;
int sgp_tvoc = 0;
int sgp_tick = 0;
char sgp_buf[10];
char sgp_lbl[32];

// CRC8 Sensirion: poly 0x31, init 0xFF, over 2 data bytes
int sgp_crc(int b1, int b2) {
    int crc = 0xFF ^ b1;
    int j = 0;
    while (j < 8) {
        if (crc & 0x80) crc = ((crc << 1) ^ 0x31) & 0xFF;
        else crc = (crc << 1) & 0xFF;
        j++;
    }
    crc = crc ^ b2;
    j = 0;
    while (j < 8) {
        if (crc & 0x80) crc = ((crc << 1) ^ 0x31) & 0xFF;
        else crc = (crc << 1) & 0xFF;
        j++;
    }
    return crc;
}

// Send 16-bit I2C command
void sgp_cmd(int cmd) {
    sgp_buf[0] = cmd & 0xFF;
    i2cWrite(sgp_addr, cmd >> 8, sgp_buf, 1, sgp_bus);
}

int sgp_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(0x58, bus)) {
            sgp_addr = 0x58;
            sgp_bus = bus;
            // Get serial number to verify sensor
            sgp_buf[0] = 0x82;
            i2cWrite(sgp_addr, 0x36, sgp_buf, 1, sgp_bus);
            delay(10);
            if (i2cRead0(sgp_addr, sgp_buf, 9, sgp_bus)) {
                // Verify first word CRC
                if (sgp_crc(sgp_buf[0], sgp_buf[1]) == sgp_buf[2]) {
                    // Init IAQ algorithm
                    sgp_cmd(0x2003);
                    delay(10);
                    i2cSetActiveFound(sgp_addr, "SGP30", sgp_bus);
                    return 1;
                }
            }
            sgp_addr = 0;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!sgp_addr) return;

    if (sgp_tick == 0) {
        // First call: send IAQ Measure command
        sgp_cmd(0x2008);
        sgp_tick = 1;
    } else {
        // Read response from previous command (>12ms ago — well exceeded)
        if (i2cRead0(sgp_addr, sgp_buf, 6, sgp_bus)) {
            if (sgp_crc(sgp_buf[0], sgp_buf[1]) == sgp_buf[2] &&
                sgp_crc(sgp_buf[3], sgp_buf[4]) == sgp_buf[5]) {
                sgp_eco2 = (sgp_buf[0] << 8) | sgp_buf[1];
                sgp_tvoc = (sgp_buf[3] << 8) | sgp_buf[4];
                sgp_ok = 1;
            }
        }
        // Send next command immediately
        sgp_cmd(0x2008);
    }
}

void WebCall() {
    char buf[48];
    if (sgp_ok) {
        LGetString(5, sgp_lbl);  // eCO2
        strcpy(buf, "{s}SGP30 ");
        strcat(buf, sgp_lbl);
        strcat(buf, "{m}");
        webSend(buf);
        sprintf(buf, "%d ppm{e}", sgp_eco2);
        webSend(buf);
        LGetString(6, sgp_lbl);  // TVOC
        strcpy(buf, "{s}SGP30 ");
        strcat(buf, sgp_lbl);
        strcat(buf, "{m}");
        webSend(buf);
        sprintf(buf, "%d ppb{e}", sgp_tvoc);
        webSend(buf);
    } else {
        webSend("{s}SGP30{m}not ready{e}");
    }
}

void JsonCall() {
    if (!sgp_ok) return;
    char buf[64];
    sprintf(buf, ",\\"SGP30\\":{\\"eCO2\\":%d", sgp_eco2);
    responseAppend(buf);
    sprintf(buf, ",\\"TVOC\\":%d}", sgp_tvoc);
    responseAppend(buf);
}

void OnExit() {
    if (sgp_addr) {
        I2cResetActive(sgp_addr, sgp_bus);
    }
}

int main() {
    sgp_ok = 0;
    sgp_addr = 0;
    sgp_tick = 0;

    if (sgp_scan()) {
        char buf[48];
        sprintf(buf, "SGP30 found at 0x%x on bus %d", sgp_addr, sgp_bus);
        addLog(buf);
    } else {
        addLog("SGP30 not found");
    }
    return 0;
}`,
            sps30: `// SPS30 Particulate Matter Sensor Driver
// I2C address: 0x69
// Measures: PM1.0, PM2.5, PM4.0, PM10.0 (ug/m3)
//           NCPM0.5, NCPM1.0, NCPM2.5, NCPM4.0, NCPM10 (#/cm3)
//           Typical Particle Size (um)
// Data as IEEE754 floats, CRC8 poly 0x31, init 0xFF (Sensirion)
// Duty-cycled: sleeps between measurements to extend laser lifetime (~8yr continuous)
// Default: measure every 5 minutes (fan runs 30s to stabilize)
// Console: SPS30 Measure (trigger now), SPS30 Interval <seconds>

#define SPS_INTERVAL 300  // default: measure every 300s (5 min)
#define SPS_FANTIME   30  // fan stabilization time (seconds)

// States
#define ST_SLEEP   0
#define ST_WAKE    1
#define ST_START   2
#define ST_STABLE  3
#define ST_READ    4
#define ST_STOP    5

int sps_addr = 0;
int sps_bus = 0;
int sps_ok = 0;
// Mass concentrations (ug/m3)
float sps_pm1 = 0.0;
float sps_pm25 = 0.0;
float sps_pm4 = 0.0;
float sps_pm10 = 0.0;
// Number concentrations (#/cm3)
float sps_nc05 = 0.0;
float sps_nc1 = 0.0;
float sps_nc25 = 0.0;
float sps_nc4 = 0.0;
float sps_nc10 = 0.0;
// Typical particle size (um)
float sps_tps = 0.0;
int sps_state = 0;
int sps_tick = 0;
int sps_interval = SPS_INTERVAL;
int sps_fan_count = 0;
char sps_buf[62];

// CRC8 Sensirion: poly 0x31, init 0xFF, over 2 data bytes
int sps_crc(int b1, int b2) {
    int crc = 0xFF ^ b1;
    int j = 0;
    while (j < 8) {
        if (crc & 0x80) crc = ((crc << 1) ^ 0x31) & 0xFF;
        else crc = (crc << 1) & 0xFF;
        j++;
    }
    crc = crc ^ b2;
    j = 0;
    while (j < 8) {
        if (crc & 0x80) crc = ((crc << 1) ^ 0x31) & 0xFF;
        else crc = (crc << 1) & 0xFF;
        j++;
    }
    return crc;
}

// Send 16-bit command (no data)
void sps_cmd(int cmd) {
    sps_buf[0] = cmd & 0xFF;
    i2cWrite(sps_addr, cmd >> 8, sps_buf, 1, sps_bus);
}

// Send 16-bit command with 16-bit argument + CRC
void sps_cmd_arg(int cmd, int arg) {
    sps_buf[0] = cmd & 0xFF;
    sps_buf[1] = arg >> 8;
    sps_buf[2] = arg & 0xFF;
    sps_buf[3] = sps_crc(sps_buf[1], sps_buf[2]);
    i2cWrite(sps_addr, cmd >> 8, sps_buf, 4, sps_bus);
}

// Reconstruct IEEE754 float from 6-byte response chunk
// Layout: [hi_MSB, hi_LSB, CRC, lo_MSB, lo_LSB, CRC]
float sps_get_float(int offset) {
    int hi_msb = sps_buf[offset];
    int hi_lsb = sps_buf[offset + 1];
    int lo_msb = sps_buf[offset + 3];
    int lo_lsb = sps_buf[offset + 4];
    if (sps_crc(hi_msb, hi_lsb) != sps_buf[offset + 2]) return 0.0;
    if (sps_crc(lo_msb, lo_lsb) != sps_buf[offset + 5]) return 0.0;
    int bits = (hi_msb << 24) | (hi_lsb << 16) | (lo_msb << 8) | lo_lsb;
    return intBitsToFloat(bits);
}

void sps_sleep() {
    sps_cmd(0x0104);   // stop measurement
    delay(20);
    sps_cmd(0x1001);   // sleep (disables I2C, fan off, laser off)
    delay(5);
    sps_state = ST_SLEEP;
    sps_tick = 0;
}

void sps_wake() {
    // Sleep mode disables I2C — send wake cmd twice
    // (first reactivates interface, second is the actual wake)
    sps_cmd(0x1103);
    delay(5);
    sps_cmd(0x1103);
    delay(100);
    sps_state = ST_WAKE;
}

int sps_scan() {
    int bus = 0;
    while (bus < 2) {
        if (i2cSetDevice(0x69, bus)) {
            sps_addr = 0x69;
            sps_bus = bus;
            i2cSetActiveFound(sps_addr, "SPS30", sps_bus);
            return 1;
        }
        bus++;
    }
    return 0;
}

void EverySecond() {
    if (!sps_addr) return;

    if (sps_state == ST_SLEEP) {
        sps_tick++;
        if (sps_tick >= sps_interval) {
            // Time to measure — wake up
            sps_wake();
        }
        return;
    }

    if (sps_state == ST_WAKE) {
        // Start measurement: cmd 0x0010, arg 0x0300 (big-endian float output)
        sps_cmd_arg(0x0010, 0x0300);
        delay(20);
        sps_fan_count = 0;
        sps_state = ST_STABLE;
        return;
    }

    if (sps_state == ST_STABLE) {
        // Wait for fan to stabilize
        sps_fan_count++;
        if (sps_fan_count >= SPS_FANTIME) {
            sps_state = ST_READ;
        }
        return;
    }

    if (sps_state == ST_READ) {
        // Check data ready (cmd 0x0202)
        sps_cmd(0x0202);
        delay(20);
        if (!i2cRead0(sps_addr, sps_buf, 3, sps_bus)) {
            sps_sleep();
            return;
        }
        if (sps_crc(sps_buf[0], sps_buf[1]) != sps_buf[2]) {
            sps_sleep();
            return;
        }
        int ready = (sps_buf[0] << 8) | sps_buf[1];
        if (!ready) return;  // wait another second

        // Read all 10 floats = 60 bytes
        sps_cmd(0x0300);
        delay(20);
        if (i2cRead0(sps_addr, sps_buf, 60, sps_bus)) {
            sps_pm1  = sps_get_float(0);
            sps_pm25 = sps_get_float(6);
            sps_pm4  = sps_get_float(12);
            sps_pm10 = sps_get_float(18);
            sps_nc05 = sps_get_float(24);
            sps_nc1  = sps_get_float(30);
            sps_nc25 = sps_get_float(36);
            sps_nc4  = sps_get_float(42);
            sps_nc10 = sps_get_float(48);
            sps_tps  = sps_get_float(54);
            sps_ok = 1;
        }
        // Done — go back to sleep
        sps_sleep();
        return;
    }
}

void Command(char cmd[]) {
    char buf[64];
    char arg[16];

    if (strFind(cmd, "MEASURE") == 0) {
        // Trigger immediate measurement
        if (sps_state == ST_SLEEP) {
            sps_wake();
            responseCmnd("Waking up, measurement in ~30s");
        } else {
            responseCmnd("Already measuring");
        }
    } else if (strFind(cmd, "INTERVAL") == 0) {
        if (strlen(cmd) > 9) {
            strSub(arg, cmd, 9, 0);
            int val = atoi(arg);
            if (val >= 60) {
                sps_interval = val;
            }
        }
        sprintf(buf, "Interval: %d s", sps_interval);
        responseCmnd(buf);
    } else {
        responseCmnd("Measure|Interval <sec>");
    }
}

void WebCall() {
    char buf[80];
    if (sps_ok) {
        // Mass concentrations
        sprintf(buf, "{s}SPS30 PM 1.0{m}%.2f &micro;g/m&sup3;{e}", sps_pm1);
        webSend(buf);
        sprintf(buf, "{s}SPS30 PM 2.5{m}%.2f &micro;g/m&sup3;{e}", sps_pm25);
        webSend(buf);
        sprintf(buf, "{s}SPS30 PM 4.0{m}%.2f &micro;g/m&sup3;{e}", sps_pm4);
        webSend(buf);
        sprintf(buf, "{s}SPS30 PM 10{m}%.2f &micro;g/m&sup3;{e}", sps_pm10);
        webSend(buf);
        // Number concentrations
        sprintf(buf, "{s}SPS30 NCPM 0.5{m}%.2f #/cm&sup3;{e}", sps_nc05);
        webSend(buf);
        sprintf(buf, "{s}SPS30 NCPM 1.0{m}%.2f #/cm&sup3;{e}", sps_nc1);
        webSend(buf);
        sprintf(buf, "{s}SPS30 NCPM 2.5{m}%.2f #/cm&sup3;{e}", sps_nc25);
        webSend(buf);
        sprintf(buf, "{s}SPS30 NCPM 4.0{m}%.2f #/cm&sup3;{e}", sps_nc4);
        webSend(buf);
        sprintf(buf, "{s}SPS30 NCPM 10{m}%.2f #/cm&sup3;{e}", sps_nc10);
        webSend(buf);
        // Typical particle size
        sprintf(buf, "{s}SPS30 TYPSIZ{m}%.2f &micro;m{e}", sps_tps);
        webSend(buf);
        // Status
        if (sps_state == ST_SLEEP) {
            sprintf(buf, "{s}SPS30 Status{m}sleeping, next in %d s{e}", sps_interval - sps_tick);
        } else {
            webSend("{s}SPS30 Status{m}measuring...{e}");
        }
        webSend(buf);
    } else {
        webSend("{s}SPS30{m}waiting for first measurement{e}");
    }
}

void JsonCall() {
    if (!sps_ok) return;
    char buf[64];
    sprintf(buf, ",\\"SPS30\\":{\\"PM1_0\\":%.2f", sps_pm1);
    responseAppend(buf);
    sprintf(buf, ",\\"PM2_5\\":%.2f", sps_pm25);
    responseAppend(buf);
    sprintf(buf, ",\\"PM4_0\\":%.2f", sps_pm4);
    responseAppend(buf);
    sprintf(buf, ",\\"PM10\\":%.2f", sps_pm10);
    responseAppend(buf);
    sprintf(buf, ",\\"NCPM0_5\\":%.2f", sps_nc05);
    responseAppend(buf);
    sprintf(buf, ",\\"NCPM1_0\\":%.2f", sps_nc1);
    responseAppend(buf);
    sprintf(buf, ",\\"NCPM2_5\\":%.2f", sps_nc25);
    responseAppend(buf);
    sprintf(buf, ",\\"NCPM4_0\\":%.2f", sps_nc4);
    responseAppend(buf);
    sprintf(buf, ",\\"NCPM10\\":%.2f", sps_nc10);
    responseAppend(buf);
    sprintf(buf, ",\\"TYPSIZ\\":%.2f}", sps_tps);
    responseAppend(buf);
}

void OnExit() {
    if (sps_addr) {
        if (sps_state != ST_SLEEP) {
            sps_cmd(0x0104);  // stop measurement
            delay(20);
            sps_cmd(0x1001);  // sleep
            delay(5);
        }
        I2cResetActive(sps_addr, sps_bus);
    }
}

int main() {
    sps_ok = 0;
    sps_addr = 0;
    sps_state = ST_SLEEP;
    sps_tick = 0;
    sps_interval = SPS_INTERVAL;

    if (sps_scan()) {
        char buf[48];
        sprintf(buf, "SPS30 found at 0x%x on bus %d", sps_addr, sps_bus);
        addLog(buf);
        addCommand("SPS30");
        // Do first measurement immediately
        sps_wake();
    } else {
        addLog("SPS30 not found");
    }
    return 0;
}`,
            ccs811: `// CCS811 Digital Air Quality Sensor (eCO2 + TVOC)
// I2C address: 0x5A (default) or 0x5B (ADDR pin high)
// HW_ID: 0x81
//
// Reports: eCO2 (400-8192 ppm), TVOC (0-1187 ppb)
// Measurement mode 1: 1-second interval
// Needs ~20 min warm-up for stable readings

#define CCS_ADDR1 0x5A
#define CCS_ADDR2 0x5B
#define CCS_HW_ID 0x81

// registers
#define CCS_STATUS   0x00
#define CCS_MEAS     0x01
#define CCS_ALG_DATA 0x02
#define CCS_ENV_DATA 0x05
#define CCS_HW_ID_REG 0x20
#define CCS_ERR_REG  0xE0
#define CCS_APP_START 0xF4
#define CCS_SW_RESET 0xFF

int ccs_addr = 0;
int ccs_bus = 0;
int ccs_ok = 0;
int ccs_eco2 = 0;
int ccs_tvoc = 0;
int ccs_err = 0;
char ccs_name[16];
char ccs_buf[16];
char ccs_lbl[32];

int ccs_scan() {
    int bus = 0;
    while (bus < 2) {
        int addr = CCS_ADDR1;
        while (addr <= CCS_ADDR2) {
            if (i2cSetDevice(addr, bus)) {
                int id = i2cRead8(addr, CCS_HW_ID_REG, bus);
                if (id == CCS_HW_ID) {
                    ccs_addr = addr;
                    ccs_bus = bus;
                    i2cSetActiveFound(ccs_addr, "CCS811", ccs_bus);
                    return 1;
                }
            }
            addr = addr + 1;
        }
        bus = bus + 1;
    }
    return 0;
}

int ccs_init() {
    // read status — check APP_VALID (bit 4)
    int status = i2cRead8(ccs_addr, CCS_STATUS, ccs_bus);
    if ((status & 0x10) == 0) {
        return 0;  // no valid app firmware
    }

    // if in boot mode (FW_MODE bit 7 = 0), send APP_START
    if ((status & 0x80) == 0) {
        i2cWrite0(ccs_addr, CCS_APP_START, ccs_bus);
        delay(100);
        // verify transition
        status = i2cRead8(ccs_addr, CCS_STATUS, ccs_bus);
        if ((status & 0x80) == 0) {
            return 0;  // failed to enter app mode
        }
    }

    // set measurement mode 1 (1-second interval), no interrupts
    i2cWrite8(ccs_addr, CCS_MEAS, 0x10, ccs_bus);
    return 1;
}

void EverySecond() {
    if (!ccs_ok) return;

    // check status: DATA_READY (bit 3)
    int status = i2cRead8(ccs_addr, CCS_STATUS, ccs_bus);
    if ((status & 0x04) != 0) {
        // error flag set
        ccs_err = i2cRead8(ccs_addr, CCS_ERR_REG, ccs_bus);
    }
    if ((status & 0x08) == 0) {
        return;  // data not ready
    }

    // read ALG_RESULT_DATA: 8 bytes from register 0x02
    // [eco2_hi, eco2_lo, tvoc_hi, tvoc_lo, status, error, raw_hi, raw_lo]
    int ret = i2cRead(ccs_addr, CCS_ALG_DATA, ccs_buf, 8, ccs_bus);
    if (ret) {
        ccs_eco2 = ((ccs_buf[0] & 0xFF) << 8) | (ccs_buf[1] & 0xFF);
        ccs_tvoc = ((ccs_buf[2] & 0xFF) << 8) | (ccs_buf[3] & 0xFF);
    }
}

void ccs_web_label(int idx) {
    char vt[48];
    LGetString(idx, ccs_lbl);
    strcpy(vt, "{s}");
    strcat(vt, ccs_name);
    strcat(vt, " ");
    strcat(vt, ccs_lbl);
    strcat(vt, "{m}");
    webSend(vt);
}

void WebCall() {
    char vt[32];
    if (ccs_ok) {
        // eCO2
        ccs_web_label(5);  // 5 = eCO2
        sprintf(vt, "%d ppm{e}", ccs_eco2);
        webSend(vt);
        // TVOC
        ccs_web_label(6);  // 6 = TVOC
        sprintf(vt, "%d ppb{e}", ccs_tvoc);
        webSend(vt);
    } else {
        webSend("{s}CCS811{m}not found{e}");
    }
}

void JsonCall() {
    if (!ccs_ok) return;
    char buf[64];
    strcpy(buf, ",\\"");
    strcat(buf, ccs_name);
    strcat(buf, "\\":{");
    responseAppend(buf);
    sprintf(buf, "\\"eCO2\\":%d", ccs_eco2);
    responseAppend(buf);
    sprintf(buf, ",\\"TVOC\\":%d}", ccs_tvoc);
    responseAppend(buf);
}

void OnExit() {
    if (ccs_addr) {
        // set mode 0 (idle) before releasing
        i2cWrite8(ccs_addr, CCS_MEAS, 0x00, ccs_bus);
        I2cResetActive(ccs_addr, ccs_bus);
        ccs_addr = 0;
    }
}

int main() {
    delay(10000);

    if (!ccs_scan()) return 0;

    // build name with address
    if (ccs_addr == CCS_ADDR1) {
        strcpy(ccs_name, "CCS811");
    } else {
        strcpy(ccs_name, "CCS811b");
    }

    if (!ccs_init()) {
        char dt[48];
        sprintf(dt, "CCS811 init failed (addr 0x%x)", ccs_addr);
        addLog(dt);
        return 0;
    }

    return 0;
}`,
            ld2410: `// HLK-LD2410 24GHz mmWave Human Presence Sensor
// Serial: 256000 baud 8N1, continuous reporting every ~50ms
// Default wiring: ESP RX=GPIO8 ← LD2410 TX
//                 ESP TX=GPIO7 → LD2410 RX
//                 VCC=3.3V, GND=GND
//
// Reports: target state (none/moving/stationary/both),
//          moving + stationary distance (cm) and energy (0-100),
//          overall detection distance (cm)
//
// Frame format (normal mode, 23 bytes):
//   Header:  F4 F3 F2 F1
//   Length:  0D 00 (13 bytes)
//   Data:    02 AA state move_lo move_hi move_e
//            stat_lo stat_hi stat_e det_lo det_hi 55 00
//   Footer:  F8 F7 F6 F5

#define LD_BAUD 256000
#define LD_SBUF 256

int ld_rxpin = 8;
int ld_txpin = 7;
int ld_serial = 0;
int ld_ok = 0;

// parsed sensor values
int ld_state = 0;       // 0=none 1=moving 2=stationary 3=both
int ld_mdist = 0;       // moving target distance (cm)
int ld_menrg = 0;       // moving target energy (0-100)
int ld_sdist = 0;       // stationary target distance (cm)
int ld_senrg = 0;       // stationary target energy (0-100)
int ld_dist = 0;        // detection distance (cm)

// frame parser state machine
char ld_buf[64];
int ld_fsm = 0;
int ld_idx = 0;
int ld_dlen = 0;

// web label
char ld_lbl[32];

// ---- frame decoder ----
void ld_decode() {
    // buf[0..1]=length, [2]=type, [3]=0xAA head
    // [4]=state, [5..6]=move_dist, [7]=move_energy
    // [8..9]=stat_dist, [10]=stat_energy, [11..12]=det_dist
    if (ld_buf[3] != 0xAA) return;
    ld_state = ld_buf[4] & 0x03;
    ld_mdist = (ld_buf[5] & 0xFF) | ((ld_buf[6] & 0xFF) << 8);
    ld_menrg = ld_buf[7] & 0xFF;
    ld_sdist = (ld_buf[8] & 0xFF) | ((ld_buf[9] & 0xFF) << 8);
    ld_senrg = ld_buf[10] & 0xFF;
    ld_dist  = (ld_buf[11] & 0xFF) | ((ld_buf[12] & 0xFF) << 8);
    ld_ok = 1;
}

// ---- serial reader with header sync ----
void ld_read() {
    while (serialAvailable() > 0) {
        int b = serialRead();
        if (b < 0) break;

        // find header F4 F3 F2 F1
        if (ld_fsm == 0) {
            if (b == 0xF4) ld_fsm = 1;
        } else if (ld_fsm == 1) {
            if (b == 0xF3) {
                ld_fsm = 2;
            } else if (b != 0xF4) {
                ld_fsm = 0;
            }
        } else if (ld_fsm == 2) {
            if (b == 0xF2) {
                ld_fsm = 3;
            } else if (b == 0xF4) {
                ld_fsm = 1;
            } else {
                ld_fsm = 0;
            }
        } else if (ld_fsm == 3) {
            if (b == 0xF1) {
                ld_fsm = 4;
                ld_idx = 0;
                ld_dlen = 0;
            } else if (b == 0xF4) {
                ld_fsm = 1;
            } else {
                ld_fsm = 0;
            }
        } else {
            // reading frame payload
            ld_buf[ld_idx] = b;
            ld_idx = ld_idx + 1;

            // after 2 bytes we know the data length
            if (ld_idx == 2) {
                ld_dlen = (ld_buf[0] & 0xFF) | ((ld_buf[1] & 0xFF) << 8);
                if (ld_dlen < 10 || ld_dlen > 50) {
                    ld_fsm = 0;
                }
            }

            // complete frame: 2(len) + dlen(data) + 4(footer)
            if (ld_idx >= 2 && ld_idx == ld_dlen + 6) {
                // verify footer F8 F7 F6 F5
                int fi = ld_dlen + 2;
                if ((ld_buf[fi] & 0xFF) == 0xF8 &&
                    (ld_buf[fi + 1] & 0xFF) == 0xF7 &&
                    (ld_buf[fi + 2] & 0xFF) == 0xF6 &&
                    (ld_buf[fi + 3] & 0xFF) == 0xF5) {
                    ld_decode();
                }
                ld_fsm = 0;
            }

            // buffer overflow guard
            if (ld_idx >= 60) {
                ld_fsm = 0;
            }
        }
    }
}

// ---- callbacks ----

void EverySecond() {
    if (ld_serial) ld_read();
}

void WebCall() {
    char vt[48];
    if (!ld_serial) {
        webSend("{s}LD2410{m}serial error{e}");
        return;
    }
    if (!ld_ok) {
        webSend("{s}LD2410{m}no data{e}");
        return;
    }

    // presence state
    webSend("{s}LD2410 Presence{m}");
    if (ld_state == 0) {
        webSend("None{e}");
    } else if (ld_state == 1) {
        webSend("Moving{e}");
    } else if (ld_state == 2) {
        webSend("Stationary{e}");
    } else {
        webSend("Mov+Stat{e}");
    }

    // moving target: distance / energy
    sprintf(vt, "{s}LD2410 Moving{m}%d cm / ", ld_mdist);
    webSend(vt);
    sprintf(vt, "%d %{e}", ld_menrg);
    webSend(vt);

    // stationary target: distance / energy
    sprintf(vt, "{s}LD2410 Static{m}%d cm / ", ld_sdist);
    webSend(vt);
    sprintf(vt, "%d %{e}", ld_senrg);
    webSend(vt);

    // detection distance (localized label)
    LGetString(16, ld_lbl);
    strcpy(vt, "{s}LD2410 ");
    strcat(vt, ld_lbl);
    strcat(vt, "{m}");
    webSend(vt);
    sprintf(vt, "%d cm{e}", ld_dist);
    webSend(vt);
}

void JsonCall() {
    if (!ld_ok) return;
    char buf[64];
    responseAppend(",\\"LD2410\\":{");
    sprintf(buf, "\\"State\\":%d", ld_state);
    responseAppend(buf);
    sprintf(buf, ",\\"MovingDist\\":%d", ld_mdist);
    responseAppend(buf);
    sprintf(buf, ",\\"MovingEnergy\\":%d", ld_menrg);
    responseAppend(buf);
    sprintf(buf, ",\\"StaticDist\\":%d", ld_sdist);
    responseAppend(buf);
    sprintf(buf, ",\\"StaticEnergy\\":%d", ld_senrg);
    responseAppend(buf);
    sprintf(buf, ",\\"Distance\\":%d}", ld_dist);
    responseAppend(buf);
}

void Command(char cmd[]) {
    char buf[96];
    if (!ld_ok) {
        responseCmnd("no data");
        return;
    }
    sprintf(buf, "State=%d", ld_state);
    responseCmnd(buf);
    sprintf(buf, " MovDist=%d", ld_mdist);
    responseAppend(buf);
    sprintf(buf, " MovE=%d", ld_menrg);
    responseAppend(buf);
    sprintf(buf, " StatDist=%d", ld_sdist);
    responseAppend(buf);
    sprintf(buf, " StatE=%d", ld_senrg);
    responseAppend(buf);
    sprintf(buf, " Dist=%d", ld_dist);
    responseAppend(buf);
}

void WebUI() {
    webPulldown(ld_rxpin, "RX Pin", "@getfreepins");
    webPulldown(ld_txpin, "TX Pin", "@getfreepins");
}

void OnExit() {
    serialClose();
}

int main() {
    delay(10000);

    // restore persisted pin config
    webPulldown(ld_rxpin, "RX Pin", "@getfreepins");
    webPulldown(ld_txpin, "TX Pin", "@getfreepins");

    int ret = serialBegin(ld_rxpin, ld_txpin, LD_BAUD, 3, LD_SBUF);
    if (ret == 1) {
        ld_serial = 1;
    }

    addCommand("Radar");

    return 0;
}`,
            dysv17f: `// DY-SV17F MP3 Player Driver (Serial TX)
// UART 9600 baud, TX only
// Packet: [0xAA] [CMD] [data_len] [data...] [checksum]
// Checksum = sum of all bytes (incl. 0xAA) & 0xFF
// Play by filename: cmd 0x08, data = [device][path_transformed]
// Path: /prefix, dots→stars, uppercase. E.g. "Sound.mp3" → "/SOUND*MP3"
// Console: MP3 Play Sound.mp3, MP3 Stop, MP3 Pause, MP3 Next, MP3 Prev, MP3 Vol 20

#define DY_DEVICE  2    // 0=USB, 1=SD, 2=FLASH (built-in)

int dy_txpin = 10;      // TX pin — selectable via web UI
int dy_ok = 0;
int dy_vol = 15;
char dy_file[32];
char dy_pkt[34];    // global packet buffer for dy_send

// Send raw packet: [0xAA][cmd][len][data from dy_pkt][checksum]
// Builds complete packet in dy_buf and sends as single buffer write
char dy_buf[38];    // complete packet buffer (header + data + checksum)

void dy_send(int cmd, int len) {
    dy_buf[0] = 0xAA;
    dy_buf[1] = cmd;
    dy_buf[2] = len;
    int cs = 0xAA + cmd + len;
    int i = 0;
    while (i < len) {
        dy_buf[3 + i] = dy_pkt[i];
        cs = cs + dy_pkt[i];
        i++;
    }
    dy_buf[3 + len] = cs & 0xFF;
    serialWriteBytes(dy_buf, len + 4);
}

// Simple commands (no data)
void dy_play()  { dy_send(0x02, 0); }
void dy_stop()  { dy_send(0x04, 0); }
void dy_pause() { dy_send(0x03, 0); }
void dy_next()  { dy_send(0x06, 0); }
void dy_prev()  { dy_send(0x05, 0); }

void dy_volume(int v) {
    if (v < 0) v = 0;
    if (v > 30) v = 30;
    dy_vol = v;
    dy_pkt[0] = v;
    dy_send(0x13, 1);
}

// Play file by name on specified device
// Reads filename from dy_file global
// Transforms: dots→stars, uppercase
// Sends cmd 0x08: [device][transformed_path]
void dy_play_file() {
    dy_pkt[0] = DY_DEVICE;
    int i = 0;
    int o = 1;
    // Auto-prepend '/' if missing (DY-SV17F needs absolute path)
    if (dy_file[0] != '/') {
        dy_pkt[o] = '/';
        o++;
    }
    while (dy_file[i] && o < 33) {
        int c = dy_file[i];
        if (c == '.') {
            dy_pkt[o] = '*';
        } else if (c >= 'a' && c <= 'z') {
            dy_pkt[o] = c - 32;  // uppercase
        } else {
            dy_pkt[o] = c;
        }
        o++;
        i++;
    }
    dy_send(0x08, o);
}

void Command(char cmd[]) {
    char buf[64];
    char arg[32];

    // Tasmota uppercases the topic, so subcommands arrive as PLAY, STOP, etc.
    // Data after the space (filenames, numbers) keeps original case.
    if (strFind(cmd, "PLAY") == 0) {
        // "PLAY" or "PLAY Sound.mp3"
        if (strlen(cmd) > 5) {
            strSub(dy_file, cmd, 5, 0);
            dy_play_file();
            sprintf(buf, "Playing %s", dy_file);
            responseCmnd(buf);
        } else {
            dy_play();
            responseCmnd("Playing");
        }
    } else if (strFind(cmd, "STOP") == 0) {
        dy_stop();
        responseCmnd("Stopped");
    } else if (strFind(cmd, "PAUSE") == 0) {
        dy_pause();
        responseCmnd("Paused");
    } else if (strFind(cmd, "NEXT") == 0) {
        dy_next();
        responseCmnd("Next track");
    } else if (strFind(cmd, "PREV") == 0) {
        dy_prev();
        responseCmnd("Previous track");
    } else if (strFind(cmd, "VOL") == 0) {
        if (strlen(cmd) > 4) {
            strSub(arg, cmd, 4, 0);
            int val = atoi(arg);
            dy_volume(val);
        }
        sprintf(buf, "Volume: %d", dy_vol);
        responseCmnd(buf);
    } else {
        responseCmnd("Play [file]|Stop|Pause|Next|Prev|Vol <n>");
    }
}

void WebUI() {
    webPulldown(dy_txpin, "TX Pin", "@getfreepins");
}

void WebCall() {
    char buf[64];
    if (dy_ok) {
        sprintf(buf, "{s}MP3 Volume{m}%d{e}", dy_vol);
        webSend(buf);
        if (dy_file[0]) {
            sprintf(buf, "{s}MP3 File{m}%s{e}", dy_file);
            webSend(buf);
        }
    } else {
        webSend("{s}MP3{m}not ready{e}");
    }
}

void OnExit() {
    dy_stop();
    delay(50);
    serialClose();
}

int main() {
    dy_file[0] = 0;
    // Wait for system to fully initialize (serial/GPIO not ready during early boot)
    delay(10000);
    // Restore persisted pin from web UI before opening serial
    webPulldown(dy_txpin, "TX Pin", "@getfreepins");
    int ret = serialBegin(-1, dy_txpin, 9600, 3, 64);  // 3 = 8N1
    if (ret == 1) {
        dy_ok = 1;
        addCommand("MP3");
        delay(100);
        dy_volume(dy_vol);
        char buf[48];
        sprintf(buf, "DY-SV17F ready on GPIO %d", dy_txpin);
        addLog(buf);
    } else {
        addLog("DY-SV17F: serial init failed");
    }
    return 0;
}`,
            max31855: `// MAX31855 Thermocouple Sensor Driver (SPI)
// Reads 14-bit thermocouple temperature via SPI
// Demonstrates: spiInit, spiSetCS, spiTransfer, WebCall, JsonCall

#define CS_PIN   5
#define SPI_MHZ  4

float tc_temp = 0.0;
int tc_ok = 0;
int tc_fault = 0;
char tc_lbl[32];

void EverySecond() {
    // Read 4 bytes from MAX31855 (read-only device, MOSI not needed)
    char buf[4];
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    int n = spiTransfer(1, buf, 4, 1);
    if (n != 4) {
        tc_ok = 0;
        return;
    }

    // Check fault bit (bit 16 = buf[1] bit 0)
    if (buf[1] & 0x01) {
        tc_fault = buf[3] & 0x07;  // fault code in lowest 3 bits of byte 3
        tc_ok = 0;
        return;
    }

    // Thermocouple temp: bits 31..18 = 14-bit signed value in buf[0..1]
    // buf[0] = bits 31..24, buf[1] = bits 23..16
    int raw = ((buf[0] << 8) | buf[1]) >> 2;  // shift out bits 17..16
    if (raw & 0x2000) {
        raw = raw - 16384;  // sign extend 14-bit to int
    }
    tc_temp = (float)raw * 0.25;
    tc_ok = 1;
    tc_fault = 0;
}

void WebCall() {
    char out[64];
    if (tc_ok) {
        LGetString(0, tc_lbl);
        strcpy(out, "{s}MAX31855 ");
        strcat(out, tc_lbl);
        strcat(out, "{m}");
        webSend(out);
        sprintf(out, "%.1f &deg;C{e}", tc_temp);
        webSend(out);
    } else if (tc_fault) {
        sprintf(out, "{s}MAX31855{m}Fault 0x%02X{e}", tc_fault);
        webSend(out);
    } else {
        webSend("{s}MAX31855{m}no data{e}");
    }
}

void JsonCall() {
    if (!tc_ok) return;
    char out[64];
    sprintf(out, ",\\"MAX31855\\":{\\"Temperature\\":%.1f}", tc_temp);
    responseAppend(out);
}

int main() {
    // Init hardware SPI at 4 MHz (read-only, no MOSI needed)
    spiInit(-1, -1, -1, SPI_MHZ);
    spiSetCS(1, CS_PIN);

    tc_ok = 0;
    tc_fault = 0;
    tc_temp = 0.0;
    addLog("MAX31855 thermocouple driver ready");
    return 0;
}`,
            epaper29: `// EPaper 2.9" Display Controller & Data Logger
// Sensors + EPaper display + 24h logging + WebChart + UDP globals

// 24h data logging arrays (96 x 15-min intervals)
// Index 0 = ring buffer position pointer, 1-96 = data
persist float aco2[97];
persist float atvc[97];
persist float tmpa[97];
persist float axhum[97];

// Sensor readings — shared via UDP (auto-send on assignment)
global float wtemp = 0.0;
global float whumi = 0.0;
global float wtvoc = 0.0;
global float wco2 = 0.0;

// Local sensor readings (not shared)
float eco2 = 0;
float ahum = 0.0;

// Received via UDP from other devices
global float bpress = 0.0;
global float sedc = 0.0;
global float wrga = 0.0;
global float wrgh = 0.0;
global float wrgg = 0.0;
global float atmp = 0.0;
global float pwl = 0.0;

// Averaging accumulators
float tmps = 0.0;
float hums = 0.0;
float tvcs = 0.0;
float co2s = 0.0;
int mcnt = 0;

// State
int last_m15 = -1;
int cnt = 0;

char dt[128];
char lbl[32];

void read_sensors() {
    wtemp = sensorGet("BME280#Temperature");
    whumi = sensorGet("BME280#Humidity");
    bpress = sensorGet("BME280#Pressure");
    wtvoc = sensorGet("SGP30#TVOC");
    eco2 = sensorGet("SGP30#eCO2");
    ahum = sensorGet("BME280#AbsHumidity");
    wco2 = sensorGet("SCD30#CarbonDioxide");
}

void update_display() {
    // Row 1: Temperature, Humidity, Pressure, Time
    sprintf(dt, "[f1p7x0y5]%.1f C", wtemp);
    dspText(dt);
    sprintf(dt, "[p7x70y5]%.1f %% [x250y5t] ", whumi);
    dspText(dt);
    //dspText("[x250y5t]");
    sprintf(dt, "[p11x140y5]%.1f hPa", bpress);
    dspText(dt);

    // Row 2: TVOC, eCO2, Abs Humidity
    sprintf(dt, "[p18x30y25]TVOC: %.1f ppb", wtvoc);
    dspText(dt);
    sprintf(dt, "[p18x160y25]eCO2: %.1f ppm", eco2);
    dspText(dt);
    sprintf(dt, "[p18c26l5]ahum: %.1f g^m3", ahum);
    dspText(dt);

    // solar inverters
    sprintf(dt, "[p25c1l5]WR 1 (Dach)  : %.1f W", sedc);
    dspText(dt);
    sprintf(dt, "[p25c1l6]WR 2 (Garage): %.1f W", -wrga);
    dspText(dt);
    sprintf(dt, "[p25c1l7]WR 3 (G-Haus): %.1f W", -wrgh);
    dspText(dt);
    sprintf(dt, "[p25c1l8]WR 4 (Garten): %.1f W", -wrgg);
    dspText(dt);

    // CO2
    sprintf(dt, "[p25c1l10]CO2          : %.0f ppm", wco2);
    dspText(dt);

    sprintf(dt, "[x170y95r120:30f2p6x185y100] %.0f %", pwl);
    dspText(dt);

    sprintf(dt, "[f0s2p10x210y70] %.0f C", atmp);
    dspText(dt);


    // Flush to EPaper
    dspText("[d]");
}

void EverySecond() {
    read_sensors();
    cnt = cnt + 1;

    // Accumulate for averaging
    tmps = tmps + wtemp;
    hums = hums + whumi;
    tvcs = tvcs + wtvoc;
    co2s = co2s + wco2;
    mcnt = mcnt + 1;

    // 15-minute data logging
    int m15 = (tasm_hour * 60 + tasm_minute) / 15 + 1;
    if (m15 != last_m15 && last_m15 > 0) {
        if (mcnt > 0) {
            aco2[last_m15] = co2s / (float)mcnt;
            atvc[last_m15] = tvcs / (float)mcnt;
            tmpa[last_m15] = tmps / (float)mcnt;
            axhum[last_m15] = hums / (float)mcnt;
            saveVars();
        }
        mcnt = 0;
        co2s = 0.0;
        tvcs = 0.0;
        tmps = 0.0;
        hums = 0.0;
    }
    last_m15 = m15;

    // Sync graph position with time
    int pos = m15;
    if (pos >= 96) {
        pos = 0;
    }

    // Update display every 10 seconds
    int ups = tasm_uptime;
    if (ups % 10 == 0) {
        update_display();
    }

    // EPaper refresh every 5 min (prevent ghosting)
    if (ups % 300 == 0) {
        dspText("[Id]");
        dspText("[id]");
    }

    // Sensor readings auto-send via UDP on assignment (global float)
}

void web_label(int idx) {
    LGetString(idx, lbl);
    dt = "{s}";
    dt += lbl;
    dt += "{m}";
    webSend(dt);
}

void WebCall() {
    web_label(0);
    sprintf(dt, "%.1f C{e}", wtemp);
    webSend(dt);
    web_label(1);
    sprintf(dt, "%.1f %{e}", whumi);
    webSend(dt);
    web_label(20);
    sprintf(dt, "%.1f g/m3{e}", ahum);
    webSend(dt);
    web_label(4);
    sprintf(dt, "%.0f ppm{e}", wco2);
    webSend(dt);
    web_label(2);
    sprintf(dt, "%.1f hPa{e}", bpress);
    webSend(dt);
    sprintf(dt, "{s}Heap{m}%d kb{e}", tasm_heap/1024);
    webSend(dt);
}

void WebPage() {
    WebChartSize(640, 200);
    // Dual Y-axis: TVOC (left 0-3000 ppb) + CO2 (right 0-2000 ppm)
    WebChart(0, "Air Quality", "TVOC|ppb", 0xFF0000, atvc[0], 96, atvc, 0, 15, 0.0, 3000.0);
    WebChart(0, "", "CO2|ppm", 0x0000FF, aco2[0], 96, aco2, 0, 15, 0.0, 2000.0);
    // Dual Y-axis: Humidity (left 0-100 %) + Temperature (right 0-40 C)
    WebChart(0, "Climate", "Humidity|%", 0xFF0000, axhum[0], 96, axhum, 1, 15, 0.0, 100.0);
    WebChart(0, "", "Temperature|C", 0x0000FF, tmpa[0], 96, tmpa, 1, 15, 0.0, 40.0);
}

int main() {
    // Initial display setup: clear, draw separator lines
    dspText("[zD0]");
    dspText("[x0y20h296x0y40h296]");
    dspText("[d]");

    // Set initial time slot
    last_m15 = (tasm_hour * 60 + tasm_minute) / 15 + 1;

    print("EPaper display started\\n");
    return 0;
}`,
            watch_demo: `// Watch Variables Demo
// Demonstrates change detection for IOT monitoring
// watch keyword tracks variable changes automatically

watch float power;
watch int relay;
int cycle;

void EverySecond() {
    cycle++;

    // Simulate sensor reading changing every 3 seconds
    if (cycle % 3 == 0) {
        power = (float)(cycle * 10);
    }

    // Simulate relay toggle every 5 seconds
    if (cycle % 5 == 0) {
        if (relay) { relay = 0; } else { relay = 1; }
    }

    // Check for power changes
    if (changed(power)) {
        float diff = delta(power);
        print(power);
        print(diff);
        snapshot(power);
    }

    // Check for relay changes
    if (written(relay)) {
        print(relay);
        snapshot(relay);
    }
}

int main() {
    power = 0.0;
    relay = 0;
    cycle = 0;
    snapshot(power);
    snapshot(relay);
    return 0;
}`,
            chart_types: `// Chart Types Demo — shows all supported WebChart types
// Generates synthetic data and displays one of each chart type
// WebChart(type, title, unit, color, pos, count, array, decimals, interval, ymin, ymax)

#define SAMPLES 24

float d_line[SAMPLES];
float d_col[SAMPLES];
float d_bar[SAMPLES];
float d_hist[SAMPLES];
float d_stk1[SAMPLES];
float d_stk2[SAMPLES];
float d_tbl1[7];
float d_tbl2[7];
float d_tbl3[7];

int pos;
int ready;

int main() {
    int i;
    // generate sample data
    for (i = 0; i < SAMPLES; i = i + 1) {
        d_line[i] = 20.0 + (i % 7) * 1.5 - (i % 3) * 0.8;
        d_col[i]  = 5.0 + (i % 5) * 2.0;
        d_bar[i]  = 10.0 + (i % 6) * 3.0;
        d_hist[i] = 15.0 + (i % 8) * 1.2 - (i % 4) * 0.5;
        d_stk1[i] = 3.0 + (i % 4) * 1.5;
        d_stk2[i] = 2.0 + (i % 5) * 1.0;
    }
    // table data: 7 days
    d_tbl1[0] = 12.5; d_tbl1[1] = 14.2; d_tbl1[2] = 11.8;
    d_tbl1[3] = 15.1; d_tbl1[4] = 13.7; d_tbl1[5] = 16.3; d_tbl1[6] = 10.9;
    d_tbl2[0] = 3.2;  d_tbl2[1] = 4.1;  d_tbl2[2] = 2.8;
    d_tbl2[3] = 5.5;  d_tbl2[4] = 3.9;  d_tbl2[5] = 6.1;  d_tbl2[6] = 2.4;
    d_tbl3[0] = 0.5;  d_tbl3[1] = 1.2;  d_tbl3[2] = 0.0;
    d_tbl3[3] = 2.1;  d_tbl3[4] = 0.3;  d_tbl3[5] = 0.0;  d_tbl3[6] = 1.8;

    pos = SAMPLES;
    ready = 1;
    print("Chart types demo ready. Open web page to view.\\n");
    return 0;
}

void WebPage() {
    if (!ready) {
        webSend("<p>Loading...</p>");
        return;
    }

    // Line chart
    WebChart('l', "Line Chart", "\\u00b0C", 0xe74c3c, pos, SAMPLES, d_line, 1, 30, 0, 0);

    // Column chart
    WebChart('c', "Column Chart", "kWh", 0x3498db, pos, SAMPLES, d_col, 1, 30, 0, 0);

    // Bar chart
    WebChart('b', "Bar Chart", "W", 0x27ae60, pos, SAMPLES, d_bar, 1, 30, 0, 0);

    // Histogram
    WebChart('h', "Histogram", "count", 0x9b59b6, pos, SAMPLES, d_hist, 1, 30, 0, 0);

    // Stacked column (2 series)
    WebChart('s', "Stacked Column", "Series A", 0xe67e22, pos, SAMPLES, d_stk1, 1, 30, 0, 0);
    WebChart('s', "",               "Series B", 0x2980b9, pos, SAMPLES, d_stk2, 1, 30, 0, 0);

    // Table with row labels
    WebChart('t', "Weekly Energy|So|Mo|Di|Mi|Do|Fr|Sa", "Haus", 0, 7, 7, d_tbl1, 1, 0, 0, 0);
    WebChart('t', "",                                   "Solar", 0, 7, 7, d_tbl2, 1, 0, 0, 0);
    WebChart('t', "",                                   "Regen", 0, 7, 7, d_tbl3, 1, 0, 0, 0);
}`,
            webcall_demo: `// WebCall Demo — Widgets on the Tasmota main page
// Demonstrates interactive widgets rendered in the sensor section
//
// Upload compiled .tcb to device, check main page for widgets

int power;
int brightness;
int mode;
char devname[32];

void WebCall() {
    // These widgets appear in the sensor section of the main page
    webButton(power, "Power");
    webSlider(brightness, 0, 100, "Brightness");
    webPulldown(mode, "Mode", "Off|Auto|Manual");
    webText(devname, 32, "Device Name");
}

void EverySecond() {
    if (mode == 1) {
        if (brightness > 50) {
            power = 1;
        } else {
            power = 0;
        }
    }
}

int main() {
    power = 0;
    brightness = 50;
    mode = 0;
    strcpy(devname, "Test");
    return 0;
}`,
            webui_demo: `// WebUI Demo — Smart Relay Controller
// Demonstrates all 8 widget types on a /tc_ui dashboard
//
// Upload compiled .tcb to device, navigate to http://<device>/tc_ui

int power;
int brightness;
int mode;
int schedule;
int alarm_time;
int fade_speed;
char devname[32];

// mode: 0=Off, 1=Auto, 2=Manual, 3=Timer
// schedule: 0=Disabled, 1=Weekdays, 2=Weekend, 3=Daily

void WebUI() {
    webButton(power, "Power");
    webSlider(brightness, 0, 100, "Brightness");
    webPulldown(mode, "Mode", "Off|Auto|Manual|Timer");
    webCheckbox(schedule, "Schedule Active");
    webTime(alarm_time, "Turn-on Time");
    webNumber(fade_speed, 1, 10, "Fade Speed");
    webRadio(schedule, "Disabled|Weekdays|Weekend|Daily");
    webText(devname, 32, "Device Name");
}

void EverySecond() {
    // Auto mode: turn on if brightness > 50
    if (mode == 1) {
        if (brightness > 50) {
            power = 1;
        } else {
            power = 0;
        }
    }

    // Timer mode: check alarm
    if (mode == 3 && schedule > 0) {
        if (tasm_time == alarm_time) {
            power = 1;
        }
    }

    // Apply power state to GPIO
    if (power) {
        analogWrite(2, brightness * 10);
    } else {
        analogWrite(2, 0);
    }
}

int main() {
    // Defaults
    power = 0;
    brightness = 50;
    mode = 2;
    schedule = 0;
    alarm_time = 700;
    fade_speed = 5;
    strcpy(devname, "Living Room");

    webPageLabel(0, "Smart Relay");
    gpioInit(2, 1);
    return 0;
}`,
            multipage_demo: `// Multi-Page WebUI Demo
// Demonstrates multiple button pages on the Tasmota main page
// Each button opens a different page with different widgets

int power;
int brightness;
int mode;
int alarm_time;
char devname[32];

void WebUI() {
    int page = webPage();  // which page are we rendering?

    if (page == 0) {
        // Control page
        webButton(power, "Power");
        webSlider(brightness, 0, 100, "Brightness");
        webPulldown(mode, "Mode", "Off|Auto|Manual");
    }
    if (page == 1) {
        // Settings page
        webTime(alarm_time, "Wake-up Time");
        webText(devname, 32, "Device Name");
    }
}

void EverySecond() {
    if (mode == 1 && brightness > 50) {
        power = 1;
    }
}

int main() {
    power = 0;
    brightness = 50;
    mode = 0;
    alarm_time = 700;
    strcpy(devname, "Living Room");

    webPageLabel(0, "Controls");
    webPageLabel(1, "Settings");
    return 0;
}`,
            web_handler: `// Custom Web Handler Demo
// Registers HTTP endpoints on the Tasmota web server
// Access: http://<device>/api/hello and /api/status

char buf[128];
int counter;

void WebOn() {
    int h = webHandler();
    if (h == 1) {
        // GET /api/hello?name=xxx
        char name[32];
        int len = webArg("name", name);
        if (len > 0) {
            strcpy(buf, "{\\"greeting\\":\\"Hello, ");
            strcat(buf, name);
            strcat(buf, "!\\"}");
        } else {
            strcpy(buf, "{\\"greeting\\":\\"Hello, World!\\"}");
        }
        webSend(buf);
    }
    if (h == 2) {
        // GET /api/status
        webSend("{\\"status\\":\\"ok\\"}");
    }
}

void EverySecond() {
    counter = counter + 1;
}

int main() {
    webOn(1, "/api/hello");
    webOn(2, "/api/status");
    printStr("Web handlers registered\\n");
    printStr("Try: /api/hello?name=YourName\\n");
    printStr("Try: /api/status\\n");
    return 0;
}`,
            bresser: `// Bresser Weather Station Receiver (CC1101 868 MHz)
// Receives data from Bresser 5-in-1/6-in-1/7-in-1 weather sensors
// Supports weather station + soil moisture sensor (s_type 4)
// Demonstrates: SPI, CC1101 radio config, multi-protocol data decoding
// Hardware: CC1101 module on SPI bus, GDO0 on GPIO for packet detect

#define CS_PIN     8
#define GDO0_PIN   7
#define SPI_MHZ    4

// CC1101 Register addresses
#define REG_IOCFG2   0x00
#define REG_IOCFG0   0x02
#define REG_FIFOTHR  0x03
#define REG_SYNC1    0x04
#define REG_SYNC0    0x05
#define REG_PKTLEN   0x06
#define REG_PKTCTRL0 0x07
#define REG_PKTCTRL1 0x08
#define REG_ADDR     0x09
#define REG_CHANNR   0x0A
#define REG_FSCTRL1  0x0B
#define REG_FSCTRL0  0x0C
#define REG_FREQ2    0x0D
#define REG_FREQ1    0x0E
#define REG_FREQ0    0x0F
#define REG_MDMCFG4  0x10
#define REG_MDMCFG3  0x11
#define REG_MDMCFG2  0x12
#define REG_MDMCFG1  0x13
#define REG_MDMCFG0  0x14
#define REG_DEVIATN  0x15
#define REG_MCSM2    0x16
#define REG_MCSM1    0x17
#define REG_MCSM0    0x18
#define REG_FOCCFG   0x19
#define REG_BSCFG    0x1A
#define REG_AGCCTRL2 0x1B
#define REG_AGCCTRL1 0x1C
#define REG_AGCCTRL0 0x1D
#define REG_FREND1   0x21
#define REG_FREND0   0x22
#define REG_FSCAL3   0x23
#define REG_FSCAL2   0x24
#define REG_FSCAL1   0x25
#define REG_FSCAL0   0x26
#define REG_TEST2    0x2C
#define REG_TEST1    0x2D
#define REG_TEST0    0x2E
#define REG_PATABLE  0x3E
#define REG_FIFO     0x3F

// CC1101 status registers (read with burst bit 0xC0)
#define REG_RSSI     0x34
#define REG_LQI      0x33
#define REG_RXBYTES  0x3B

// CC1101 command strobes
#define CMD_SRES  0x30
#define CMD_SCAL  0x33
#define CMD_SRX   0x34
#define CMD_SIDLE 0x36
#define CMD_SPWD  0x39
#define CMD_SFRX  0x3A

// SPI access modes
#define WRITE_SINGLE 0x00
#define READ_SINGLE  0x80
#define READ_BURST   0xC0
#define WRITE_BURST  0x40

// Bresser protocol
#define MSG_BUF_SIZE 27  // FIFO read size: D4 prefix + 26 data bytes
#define DATA_SIZE    26  // decoded data size (after stripping D4)

// Sensor data - weather station
float br_temp = 0.0;
int   br_hum = 0;
float br_wind_gust = 0.0;
float br_wind_avg = 0.0;
float br_wind_dir = 0.0;
float br_rain = 0.0;
float br_lux = 0.0;
float br_uvi = 0.0;
float br_rssi = 0.0;
int   br_ok = 0;
int   br_id = 0;
int   br_batt = 0;
int   br_cnt = 0;

// Sensor data - soil moisture
float soil_temp = 0.0;
int   soil_moisture = 0;
float soil_rssi = 0.0;
int   soil_ok = 0;
int   soil_id = 0;
int   soil_batt = 0;
int   soil_cnt = 0;
char  br_lbl[32];

// Raw packet buffer (global for decoder access, 26 bytes after D4)
char msg[DATA_SIZE];

// SPI transfer buffer (reused for all SPI ops)
char spi[28];

