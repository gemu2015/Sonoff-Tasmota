// Regression test: char[] ref-param subscript read/write
// Bug #3: prior codegen emitted STORE_LOCAL_ARR for buf[i]=x inside a function
// taking `char buf[]`, overwriting the runtime ref stored in the param slot.
// Fix: new opcodes LOAD_REF_ARR / STORE_REF_ARR resolve the ref at runtime.

import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { CodeGenerator } from '../src/codegen.js';
import { VM } from '../src/vm.js';

let passed = 0;
let failed = 0;

function test(name, fn) {
    try { fn(); console.log(`  \x1b[32m✓\x1b[0m ${name}`); passed++; }
    catch (e) { console.log(`  \x1b[31m✗\x1b[0m ${name}: ${e.message}`); failed++; }
}

function compileAndRun(source) {
    const tokens = new Lexer(source).tokenize();
    const ast = new Parser(tokens).parse();
    const compiled = new CodeGenerator().compile(ast);
    let output = '';
    const vm = new VM({
        onOutput: (t) => { output += t; },
        onError: (t) => { output += '[ERR] ' + t; },
    });
    vm.load(compiled);
    vm.run();
    return output.replace(/\[LOG\] /g, '').trim();
}

console.log('\n\x1b[1mRef-param Array Subscript (Bug #3)\x1b[0m');

test('write to char[] ref param — element writes persist to caller', () => {
    const out = compileAndRun(`
        void fill(char buf[], int n) {
            int i;
            for (i = 0; i < n; i = i + 1) buf[i] = 'A' + i;
            buf[n] = 0;
        }
        int main() {
            char s[16];
            fill(s, 5);
            addLog(s);
        }
    `);
    if (!out.includes('ABCDE')) throw new Error(`expected ABCDE, got: ${out}`);
});

test('read char[] ref param — buf[0] returns correct value (not 0)', () => {
    const out = compileAndRun(`
        int firstByte(char buf[]) { return buf[0]; }
        int main() {
            char s[8];
            s[0] = 'X';
            s[1] = 0;
            int r = firstByte(s);
            char tmp[16];
            sprintfInt(tmp, "%d", r);
            addLog(tmp);
        }
    `);
    if (out !== '88') throw new Error(`expected 88, got: ${out}`);
});

test('compound += on ref-param element', () => {
    const out = compileAndRun(`
        void inc(char buf[], int n) {
            int i;
            for (i = 0; i < n; i = i + 1) buf[i] += 1;
        }
        int main() {
            char s[4];
            s[0] = 'A'; s[1] = 'B'; s[2] = 'C'; s[3] = 0;
            inc(s, 3);
            addLog(s);
        }
    `);
    if (!out.includes('BCD')) throw new Error(`expected BCD, got: ${out}`);
});

test('ref-param int[] element read/write', () => {
    const out = compileAndRun(`
        int sumArr(int arr[], int n) {
            int i; int s = 0;
            for (i = 0; i < n; i = i + 1) s = s + arr[i];
            return s;
        }
        int main() {
            int v[4];
            v[0] = 10; v[1] = 20; v[2] = 30; v[3] = 40;
            int total = sumArr(v, 4);
            char tmp[16];
            sprintfInt(tmp, "%d", total);
            addLog(tmp);
        }
    `);
    if (out !== '100') throw new Error(`expected 100, got: ${out}`);
});

test('ref-param write does NOT clobber the ref itself (multi-call distinct buffers)', () => {
    // If STORE were writing to local[param_idx+0], the first call would corrupt
    // the ref and the second call would write to the wrong buffer (or crash).
    const out = compileAndRun(`
        void setFirst(char buf[], int val) { buf[0] = val; buf[1] = 0; }
        int main() {
            char a[4]; char b[4];
            a[0] = 0; b[0] = 0;
            setFirst(a, 'X');
            setFirst(b, 'Y');
            addLog(a);
            addLog(b);
        }
    `);
    if (!out.includes('X') || !out.includes('Y'))
        throw new Error(`expected X and Y, got: ${out}`);
});

console.log(`\n\x1b[1m${passed} passed, ${failed} failed\x1b[0m\n`);
process.exit(failed ? 1 : 0);
