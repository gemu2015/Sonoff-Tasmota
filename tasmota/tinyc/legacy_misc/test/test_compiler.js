// TinyC Compiler Test Suite
// Run with: node --experimental-vm-modules test/test_compiler.js

import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { CodeGenerator } from '../src/codegen.js';
import { VM } from '../src/vm.js';

let passed = 0;
let failed = 0;

function test(name, fn) {
    try {
        fn();
        console.log(`  \x1b[32m✓\x1b[0m ${name}`);
        passed++;
    } catch (e) {
        console.log(`  \x1b[31m✗\x1b[0m ${name}: ${e.message}`);
        failed++;
    }
}

function assert(cond, msg) {
    if (!cond) throw new Error(msg || 'Assertion failed');
}

function assertEqual(a, b, msg) {
    if (a !== b) throw new Error(msg || `Expected ${b}, got ${a}`);
}

function compileAndRun(source) {
    const lexer = new Lexer(source);
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const codegen = new CodeGenerator();
    const compiled = codegen.compile(ast);

    let output = '';
    const vm = new VM({
        onOutput: (text) => { output += text; },
        onError: (text) => { output += '[ERR] ' + text; },
    });
    vm.load(compiled);
    vm.run();

    return { output, vm, compiled };
}

// ─── Lexer tests ────────────────────────────────────────

console.log('\n\x1b[1mLexer Tests\x1b[0m');

test('tokenizes integers', () => {
    const lexer = new Lexer('42 0xFF 0b1010');
    const tokens = lexer.tokenize();
    assertEqual(tokens[0].value, 42);
    assertEqual(tokens[1].value, 255);
    assertEqual(tokens[2].value, 10);
});

test('tokenizes floats', () => {
    const lexer = new Lexer('3.14 2.0f');
    const tokens = lexer.tokenize();
    assert(Math.abs(tokens[0].value - 3.14) < 0.001);
    assert(Math.abs(tokens[1].value - 2.0) < 0.001);
});

test('tokenizes strings', () => {
    const lexer = new Lexer('"hello world" "with\\n newline"');
    const tokens = lexer.tokenize();
    assertEqual(tokens[0].value, 'hello world');
    assertEqual(tokens[1].value, 'with\n newline');
});

test('tokenizes operators', () => {
    const lexer = new Lexer('+ - * / % == != <= >= && || ++ --');
    const tokens = lexer.tokenize();
    assertEqual(tokens[0].type, 'PLUS');
    assertEqual(tokens[4].type, 'PERCENT');
    assertEqual(tokens[5].type, 'EQ');
    assertEqual(tokens[9].type, 'AND');
    assertEqual(tokens[11].type, 'INC');
});

test('tokenizes keywords', () => {
    const lexer = new Lexer('int float void if else while for return');
    const tokens = lexer.tokenize();
    assertEqual(tokens[0].type, 'KW_INT');
    assertEqual(tokens[1].type, 'KW_FLOAT');
    assertEqual(tokens[4].type, 'KW_ELSE');
});

test('skips comments', () => {
    const lexer = new Lexer('42 // comment\n 43 /* block\ncomment */ 44');
    const tokens = lexer.tokenize();
    assertEqual(tokens[0].value, 42);
    assertEqual(tokens[1].value, 43);
    assertEqual(tokens[2].value, 44);
});

// ─── Parser tests ───────────────────────────────────────

console.log('\n\x1b[1mParser Tests\x1b[0m');

test('parses empty main', () => {
    const lexer = new Lexer('int main() { return 0; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    assertEqual(ast.body[0].type, 'FunctionDecl');
    assertEqual(ast.body[0].name, 'main');
});

test('parses variable declaration', () => {
    const lexer = new Lexer('int main() { int x = 42; return x; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const body = ast.body[0].body.body;
    assertEqual(body[0].type, 'VarDecl');
    assertEqual(body[0].name, 'x');
});

test('parses if/else', () => {
    const lexer = new Lexer('int main() { if (1) { return 1; } else { return 0; } }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const body = ast.body[0].body.body;
    assertEqual(body[0].type, 'IfStmt');
    assert(body[0].alternate !== null);
});

test('parses while loop', () => {
    const lexer = new Lexer('int main() { while (1) { break; } return 0; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const body = ast.body[0].body.body;
    assertEqual(body[0].type, 'WhileStmt');
});

test('parses for loop', () => {
    const lexer = new Lexer('int main() { for (int i = 0; i < 10; i++) {} return 0; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const body = ast.body[0].body.body;
    assertEqual(body[0].type, 'ForStmt');
});

test('parses array declaration', () => {
    const lexer = new Lexer('int main() { int arr[5] = {1, 2, 3, 4, 5}; return 0; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const body = ast.body[0].body.body;
    assertEqual(body[0].type, 'ArrayDecl');
    assertEqual(body[0].init.length, 5);
});

test('parses function calls', () => {
    const lexer = new Lexer('int foo(int x) { return x; } int main() { return foo(42); }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    assertEqual(ast.body.length, 2);
    assertEqual(ast.body[0].name, 'foo');
});

test('parses binary expressions with precedence', () => {
    const lexer = new Lexer('int main() { return 2 + 3 * 4; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const ret = ast.body[0].body.body[0];
    // Should be ADD(2, MUL(3, 4))
    assertEqual(ret.value.op, '+');
    assertEqual(ret.value.right.op, '*');
});

test('parses #define', () => {
    const lexer = new Lexer('#define LED 2\nint main() { return LED; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    assertEqual(ast.body[0].type, 'DefineDecl');
    assertEqual(ast.body[0].name, 'LED');
});

// ─── Code generation & VM tests ─────────────────────────

console.log('\n\x1b[1mCodeGen + VM Tests\x1b[0m');

test('compiles and runs hello world', () => {
    const { output } = compileAndRun(`
        int main() {
            printStr("Hello!");
            return 0;
        }
    `);
    assert(output.includes('Hello!'));
});

test('compiles arithmetic', () => {
    const { output } = compileAndRun(`
        int main() {
            int x = 3 + 4 * 2;
            print(x);
            return 0;
        }
    `);
    assert(output.includes('11'));
});

test('compiles if/else', () => {
    const { output } = compileAndRun(`
        int main() {
            int x = 10;
            if (x > 5) {
                print(1);
            } else {
                print(0);
            }
            return 0;
        }
    `);
    assert(output.includes('1'));
});

test('compiles while loop', () => {
    const { output } = compileAndRun(`
        int main() {
            int sum = 0;
            int i = 1;
            while (i <= 10) {
                sum += i;
                i++;
            }
            print(sum);
            return 0;
        }
    `);
    assert(output.includes('55'));
});

test('compiles for loop', () => {
    const { output } = compileAndRun(`
        int main() {
            int sum = 0;
            for (int i = 1; i <= 5; i++) {
                sum += i;
            }
            print(sum);
            return 0;
        }
    `);
    assert(output.includes('15'));
});

test('compiles function calls', () => {
    const { output } = compileAndRun(`
        int square(int x) {
            return x * x;
        }
        int main() {
            print(square(7));
            return 0;
        }
    `);
    assert(output.includes('49'));
});

test('compiles recursive fibonacci', () => {
    const { output } = compileAndRun(`
        int fib(int n) {
            if (n <= 1) return n;
            return fib(n - 1) + fib(n - 2);
        }
        int main() {
            print(fib(10));
            return 0;
        }
    `);
    assert(output.includes('55'));
});

test('compiles arrays', () => {
    const { output } = compileAndRun(`
        int main() {
            int arr[5] = {10, 20, 30, 40, 50};
            print(arr[0]);
            print(arr[2]);
            print(arr[4]);
            return 0;
        }
    `);
    assert(output.includes('10'));
    assert(output.includes('30'));
    assert(output.includes('50'));
});

test('compiles #define constants', () => {
    const { output } = compileAndRun(`
        #define VALUE 42
        int main() {
            print(VALUE);
            return 0;
        }
    `);
    assert(output.includes('42'));
});

test('compiles nested function calls', () => {
    const { output } = compileAndRun(`
        int add(int a, int b) { return a + b; }
        int mul(int a, int b) { return a * b; }
        int main() {
            print(add(mul(3, 4), 5));
            return 0;
        }
    `);
    assert(output.includes('17'));
});

test('compiles GPIO simulation', () => {
    const { output } = compileAndRun(`
        int main() {
            pinMode(2, 1);
            digitalWrite(2, 1);
            return 0;
        }
    `);
    assert(output.includes('[GPIO] pinMode(2'));
    assert(output.includes('[GPIO] digitalWrite(2, 1)'));
});

test('binary format is valid', () => {
    const lexer = new Lexer('int main() { return 42; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const codegen = new CodeGenerator();
    const compiled = codegen.compile(ast);

    // Check magic: "TCC\0"
    assertEqual(compiled.binary[0], 0x54);
    assertEqual(compiled.binary[1], 0x43);
    assertEqual(compiled.binary[2], 0x43);
    assertEqual(compiled.binary[3], 0x00);
    // Version 3 (V3: added function table section for callbacks)
    assertEqual(compiled.binary[4], 0);
    assertEqual(compiled.binary[5], 3);
});

test('disassembler produces output', () => {
    const lexer = new Lexer('int main() { return 42; }');
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const codegen = new CodeGenerator();
    const compiled = codegen.compile(ast);
    const vm = new VM();
    const disasm = vm.disassemble(compiled);
    assert(disasm.includes('CALL'));
    assert(disasm.includes('HALT'));
    assert(disasm.includes('RET_VAL'));
});

// ─── Callback & Tasmota output tests ────────────────────

test('compiles responseAppend syscall', () => {
    const { output } = compileAndRun(`
        int main() {
            char buf[32];
            strcpy(buf, "hello_json");
            responseAppend(buf);
            return 0;
        }
    `);
    assert(output.includes('hello_json'), `Expected 'hello_json' in output, got: ${output}`);
});

test('compiles webSend syscall', () => {
    const { output } = compileAndRun(`
        int main() {
            char buf[32];
            strcpy(buf, "hello_web");
            webSend(buf);
            return 0;
        }
    `);
    assert(output.includes('hello_web'), `Expected 'hello_web' in output, got: ${output}`);
});

test('compiles callback functions in binary', () => {
    const lexer = new Lexer(`
        void EverySecond() {}
        void JsonCall() {}
        void WebPage() {}
        void WebCall() {}
        int main() { return 0; }
    `);
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    const ast = parser.parse();
    const codegen = new CodeGenerator();
    const compiled = codegen.compile(ast);

    // Load and verify callback table
    const vm = new VM();
    vm.load(compiled);
    assert(vm.callbackTable, 'VM should have callbackTable');
    assert(vm.callbackTable.has('EverySecond'), 'Should have EverySecond callback');
    assert(vm.callbackTable.has('JsonCall'), 'Should have JsonCall callback');
    assert(vm.callbackTable.has('WebPage'), 'Should have WebPage callback');
    assert(vm.callbackTable.has('WebCall'), 'Should have WebCall callback');
});

test('callbacks example compiles and runs', () => {
    const { output, vm } = compileAndRun(`
        int counter = 0;
        void EverySecond() {
            counter++;
        }
        void JsonCall() {
            char buf[64];
            sprintfInt(buf, ",\\"TinyC\\":{\\"Count\\":%d}", counter);
            responseAppend(buf);
        }
        void WebCall() {
            char buf[64];
            sprintfInt(buf, "{s}TinyC Counter{m}%d{e}", counter);
            webSend(buf);
        }
        int main() {
            counter = 0;
            printStr("Callbacks active\\n");
            return 0;
        }
    `);
    assert(output.includes('Callbacks active'), `Expected 'Callbacks active' in output, got: ${output}`);

    // Simulate callback execution
    let cbOutput = '';
    vm.onOutput = (text) => { cbOutput += text; };

    vm.callFunction('EverySecond');
    vm.callFunction('EverySecond');
    vm.callFunction('EverySecond');
    vm.callFunction('JsonCall');
    assert(cbOutput.includes('"Count":3'), `Expected Count:3 in callback output, got: ${cbOutput}`);

    cbOutput = '';
    vm.callFunction('WebCall');
    assert(cbOutput.includes('TinyC Counter'), `Expected 'TinyC Counter' in web output, got: ${cbOutput}`);
    assert(cbOutput.includes('3'), `Expected counter value 3 in web output, got: ${cbOutput}`);
});

test('compiles UDP multicast functions', () => {
    const { output, vm } = compileAndRun(`
        float temp = 23.5;
        int main() {
            udpSend("temperature", temp);
            float rx = udpRecv("temperature");
            int ready = udpReady("temperature");
            printStr("UDP test done\\n");
            return 0;
        }
    `);
    assert(output.includes('UDP TX'), `Expected UDP TX in output, got: ${output}`);
    assert(output.includes('temperature'), `Expected 'temperature' in output, got: ${output}`);
    assert(output.includes('UDP test done'), `Expected 'UDP test done' in output, got: ${output}`);
});

test('compiles UDP array send/receive functions', () => {
    const { output, vm } = compileAndRun(`
        float data[4];
        int main() {
            data[0] = 1.0;
            data[1] = 2.0;
            data[2] = 3.0;
            data[3] = 4.0;
            udpSendArray("sensors", data, 4);
            float recv[4];
            int n = udpRecvArray("sensors", recv, 4);
            print(n);
            printStr("UDP array test done\\n");
            return 0;
        }
    `);
    assert(output.includes('UDP TX'), `Expected UDP TX in output, got: ${output}`);
    assert(output.includes('sensors'), `Expected 'sensors' in output, got: ${output}`);
    assert(output.includes('4 floats'), `Expected '4 floats' in output, got: ${output}`);
    assert(output.includes('4'), `Expected received count 4 in output, got: ${output}`);
    assert(output.includes('UDP array test done'), `Expected 'UDP array test done' in output, got: ${output}`);
});

test('compiles SML meter access functions', () => {
    const { output } = compileAndRun(`
        int main() {
            int count = smlGet(0);
            float val = smlGet(1);
            char id[32];
            int len = smlGetStr(1, id);
            printStr("SML test done\\n");
            return 0;
        }
    `);
    assert(output.includes('SML test done'), `Expected 'SML test done' in output, got: ${output}`);
});

test('compiles I2C bus functions', () => {
    const { output } = compileAndRun(`
        int main() {
            // Check device exists on bus 0
            int found = i2cExists(0x48, 0);
            print(found);
            // Read single byte from bus 0
            int val = i2cRead8(0x48, 0x00, 0);
            print(val);
            // Write single byte to bus 1
            int ok = i2cWrite8(0x48, 0x01, 0x80, 1);
            print(ok);
            // Buffer read/write on bus 0
            char buf[8];
            int rok = i2cRead(0x48, 0x00, buf, 4, 0);
            print(rok);
            buf[0] = 0xAA;
            buf[1] = 0xBB;
            int wok = i2cWrite(0x48, 0x02, buf, 2, 0);
            print(wok);
            printStr("I2C test done\\n");
            return 0;
        }
    `);
    assert(output.includes('I2C'), `Expected I2C output, got: ${output}`);
    assert(output.includes('I2C test done'), `Expected 'I2C test done' in output, got: ${output}`);
});

test('compiles SPI bus functions', () => {
    const { output } = compileAndRun(`
        int main() {
            // Init hardware SPI at 4 MHz
            int ok = spiInit(-1, -1, -1, 4);
            print(ok);
            // Set CS pin
            spiSetCS(1, 5);
            // Transfer 4 bytes
            char buf[4];
            buf[0] = 0xDE;
            buf[1] = 0xAD;
            buf[2] = 0xBE;
            buf[3] = 0xEF;
            int n = spiTransfer(1, buf, 4, 1);
            print(n);
            printStr("SPI test done\\n");
            return 0;
        }
    `);
    assert(output.includes('SPI'), `Expected SPI output, got: ${output}`);
    assert(output.includes('SPI test done'), `Expected 'SPI test done' in output, got: ${output}`);
});

test('negative float literals in expressions', () => {
    // Bug: -45.0 + 65.0 produced garbage because inferType didn't handle UnaryExpr
    const { output: o1 } = compileAndRun(`
        int main() {
            float x = -45.0 + 65.0;
            char buf[32];
            sprintfFloat(buf, "%.1f", x);
            printString(buf);
            return 0;
        }
    `);
    assertEqual(o1, '20.0', `Expected 20.0 for -45.0 + 65.0, got: ${o1}`);

    const { output: o2 } = compileAndRun(`
        int main() {
            float x = 65.0 + (-45.0);
            char buf[32];
            sprintfFloat(buf, "%.1f", x);
            printString(buf);
            return 0;
        }
    `);
    assertEqual(o2, '20.0', `Expected 20.0 for 65.0 + (-45.0), got: ${o2}`);

    const { output: o3 } = compileAndRun(`
        int main() {
            float x = -3.5 * 2.0;
            char buf[32];
            sprintfFloat(buf, "%.1f", x);
            printString(buf);
            return 0;
        }
    `);
    assertEqual(o3, '-7.0', `Expected -7.0 for -3.5 * 2.0, got: ${o3}`);

    const { output: o4 } = compileAndRun(`
        int main() {
            float x = -1.0 + -2.0;
            char buf[32];
            sprintfFloat(buf, "%.1f", x);
            printString(buf);
            return 0;
        }
    `);
    assertEqual(o4, '-3.0', `Expected -3.0 for -1.0 + -2.0, got: ${o4}`);

    // SHT31 temperature formula
    const { output: o5 } = compileAndRun(`
        int main() {
            int raw_t = 24655;
            float temp = -45.0 + 175.0 * (float)raw_t / 65535.0;
            char buf[32];
            sprintfFloat(buf, "%.1f", temp);
            printString(buf);
            return 0;
        }
    `);
    assertEqual(o5, '20.8', `Expected ~20.8 for SHT31 formula, got: ${o5}`);
});

// ─── New C Compatibility Features ────────────────────────────────────────────

console.log('\n\x1b[1mNew C Compatibility Features\x1b[0m');

test('ternary operator', () => {
  const { output } = compileAndRun(`
    int main() {
      int a = 10; int b = 20;
      int c = (a > b) ? a : b;
      int d = (a < b) ? a * 2 : b * 2;
      if (c == 20 && d == 20) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('ternary nested', () => {
  const { output } = compileAndRun(`
    int main() {
      int x = 5;
      int y = (x < 3) ? 1 : (x < 7) ? 2 : 3;
      if (y == 2) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('do-while loop', () => {
  const { output } = compileAndRun(`
    int main() {
      int i = 0;
      do { i++; } while (i < 5);
      int j = 100;
      do { j++; } while (j < 10);
      if (i == 5 && j == 101) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('enum constants', () => {
  const { output } = compileAndRun(`
    enum Color { RED, GREEN = 5, BLUE };
    enum Status { OK = 0, ERR = -1 };
    int main() {
      if (RED==0 && GREEN==5 && BLUE==6 && OK==0 && ERR==-1) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('const keyword accepted', () => {
  const { output } = compileAndRun(`
    const int MAX = 100;
    int main() { if (MAX == 100) addLog("ok"); else addLog("fail"); }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('compound assignments %=  &=  |=  ^=  <<=  >>=', () => {
  const { output } = compileAndRun(`
    int main() {
      int n = 17; n %= 5;    if (n != 2)  { addLog("fail %=");  return; }
      n = 0xFF;   n &= 0x0F; if (n != 15) { addLog("fail &=");  return; }
      n = 0;      n |= 0x55; if (n != 85) { addLog("fail |=");  return; }
      n = 0xFF;   n ^= 0xF0; if (n != 15) { addLog("fail ^=");  return; }
      n = 1;      n <<= 4;   if (n != 16) { addLog("fail <<="); return; }
                  n >>= 2;   if (n != 4)  { addLog("fail >>="); return; }
      addLog("ok");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('hex escape in string and char', () => {
  const { output } = compileAndRun(`
    int main() {
      char s[] = "\\x41\\x42\\x43";
      char c = '\\x5A';
      if (s[0]==65 && s[1]==66 && s[2]==67 && c==90) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

// ─── Summary ────────────────────────────────────────────

// ─── Struct ──────────────────────────────────────────────

console.log('\n\x1b[1mStruct Support\x1b[0m');

test('struct declaration and member access (local)', () => {
  const { output } = compileAndRun(`
    struct Point {
      int x;
      int y;
    };
    void main() {
      struct Point p;
      p.x = 10;
      p.y = 20;
      if (p.x == 10 && p.y == 20) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct global variable', () => {
  const { output } = compileAndRun(`
    struct Rect {
      int w;
      int h;
    };
    struct Rect g_rect;
    void main() {
      g_rect.w = 100;
      g_rect.h = 50;
      int area = g_rect.w * g_rect.h;
      if (area == 5000) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct with float fields', () => {
  const { output } = compileAndRun(`
    struct Vec2 {
      float x;
      float y;
    };
    void main() {
      struct Vec2 v;
      v.x = 1.5;
      v.y = 2.5;
      float sum = v.x + v.y;
      if (sum > 3.9 && sum < 4.1) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct compound member assign', () => {
  const { output } = compileAndRun(`
    struct Counter {
      int val;
      int step;
    };
    void main() {
      struct Counter c;
      c.val = 5;
      c.step = 3;
      c.val += c.step;
      if (c.val == 8) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct initializer list', () => {
  const { output } = compileAndRun(`
    struct Pair {
      int a;
      int b;
    };
    void main() {
      struct Pair p = {7, 13};
      if (p.a == 7 && p.b == 13) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

// ─── Summary ──────────────────────────────────────────────────

// ─── Typedef ──────────────────────────────────────────────────────────────────

console.log('\n\x1b[1mTypedef Support\x1b[0m');

test('typedef primitive type', () => {
  const { output } = compileAndRun(`
    typedef int myint;
    typedef float myfloat;
    void main() {
      myint   x = 42;
      myfloat y = 3.14;
      if (x == 42 && y > 3.1) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('typedef struct tag (named struct alias)', () => {
  const { output } = compileAndRun(`
    struct Vec2 { float x; float y; };
    typedef struct Vec2 Vec2;
    void main() {
      Vec2 v;
      v.x = 1.5;
      v.y = 2.5;
      if (v.x + v.y > 3.9) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('typedef anonymous struct', () => {
  const { output } = compileAndRun(`
    typedef struct { int r; int g; int b; } Color;
    void main() {
      Color c;
      c.r = 255;
      c.g = 128;
      c.b = 0;
      if (c.r == 255 && c.g == 128 && c.b == 0) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('typedef anonymous struct with initializer', () => {
  const { output } = compileAndRun(`
    typedef struct { int lo; int hi; } Range;
    void main() {
      Range r = {10, 100};
      if (r.lo == 10 && r.hi == 100) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('typedef global variable', () => {
  const { output } = compileAndRun(`
    typedef struct { float temp; int alarm; } Sensor;
    Sensor g_sensor;
    void main() {
      g_sensor.temp  = 23.5;
      g_sensor.alarm = 0;
      if (g_sensor.temp > 23.0 && g_sensor.alarm == 0) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('typedef chained alias', () => {
  const { output } = compileAndRun(`
    typedef int myint;
    typedef myint counter_t;
    void main() {
      counter_t n = 7;
      n += 3;
      if (n == 10) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('typedef inside function body', () => {
  const { output } = compileAndRun(`
    void main() {
      typedef int score_t;
      score_t best = 100;
      score_t curr = 80;
      if (best > curr) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

// ─── Struct Array Fields ──────────────────────────────────

console.log('\n\x1b[1mStruct Array Fields\x1b[0m');

test('struct with char array field — write and read elements', () => {
  const { output } = compileAndRun(`
    struct Msg {
      int  id;
      char text[16];
    };
    void main() {
      struct Msg m;
      m.id = 42;
      m.text[0] = 72;  // 'H'
      m.text[1] = 105; // 'i'
      m.text[2] = 0;
      if (m.id == 42 && m.text[0] == 72 && m.text[1] == 105) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct with int array field — compound assign on element', () => {
  const { output } = compileAndRun(`
    struct Stats {
      int count;
      int vals[4];
    };
    void main() {
      struct Stats s;
      s.vals[0] = 10;
      s.vals[1] = 20;
      s.vals[0] += 5;
      if (s.vals[0] == 15 && s.vals[1] == 20) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct array field with scalar fields before and after', () => {
  const { output } = compileAndRun(`
    struct Frame {
      int  header;
      char data[8];
      int  trailer;
    };
    void main() {
      struct Frame f;
      f.header  = 0xAA;
      f.data[0] = 1;
      f.data[1] = 2;
      f.data[7] = 99;
      f.trailer = 0xBB;
      if (f.header == 0xAA && f.data[0] == 1 && f.data[7] == 99 && f.trailer == 0xBB) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('global struct with array field', () => {
  const { output } = compileAndRun(`
    struct Buf {
      int  len;
      char raw[8];
    };
    struct Buf g_buf;
    void main() {
      g_buf.len    = 3;
      g_buf.raw[0] = 65; // 'A'
      g_buf.raw[1] = 66; // 'B'
      g_buf.raw[2] = 67; // 'C'
      if (g_buf.len == 3 && g_buf.raw[0] == 65 && g_buf.raw[2] == 67) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct array field element index from variable', () => {
  const { output } = compileAndRun(`
    struct Ring {
      int buf[5];
      int head;
    };
    void main() {
      struct Ring r;
      r.head = 0;
      int i;
      for (i = 0; i < 5; i++) {
        r.buf[i] = i * 10;
      }
      if (r.buf[0] == 0 && r.buf[3] == 30 && r.buf[4] == 40) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('struct char array field passed to strcpy/addLog', () => {
  const { output } = compileAndRun(`
    struct Frame {
      int  seq;
      char payload[32];
    };
    void main() {
      struct Frame f;
      f.seq = 7;
      strcpy(f.payload, "hello");
      addLog(f.payload);
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'hello');
});

// ─── String Concatenation (str1 + str2) ──────────────────

console.log('\n\x1b[1mString Concatenation (+)\x1b[0m');

test('char[] + char[] in addLog', () => {
  const { output } = compileAndRun(`
    void main() {
      char a[8]; char b[8];
      strcpy(a, "Hello");
      strcpy(b, " World");
      addLog(a + b);
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'Hello World');
});

test('char[] + string literal in addLog', () => {
  const { output } = compileAndRun(`
    void main() {
      char name[16];
      strcpy(name, "Alice");
      addLog(name + " says hi");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'Alice says hi');
});

test('string literal + char[] in addLog', () => {
  const { output } = compileAndRun(`
    void main() {
      char val[8];
      strcpy(val, "42");
      addLog("value: " + val);
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'value: 42');
});

test('chained: a + b + c', () => {
  const { output } = compileAndRun(`
    void main() {
      char a[8]; char b[8]; char c[8];
      strcpy(a, "foo");
      strcpy(b, "-");
      strcpy(c, "bar");
      addLog(a + b + c);
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'foo-bar');
});

test('str + "lit" + str (mixed chain)', () => {
  const { output } = compileAndRun(`
    void main() {
      char first[8]; char last[8];
      strcpy(first, "John");
      strcpy(last, "Doe");
      addLog(first + " " + last);
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'John Doe');
});

test('str concat result passed to strlen', () => {
  const { output } = compileAndRun(`
    void main() {
      char a[8]; char b[8];
      strcpy(a, "Hi");
      strcpy(b, "!");
      int n = strlen(a + b);
      if (n == 3) addLog("ok"); else addLog("fail");
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'ok');
});

test('str concat result copied with strcpy', () => {
  const { output } = compileAndRun(`
    void main() {
      char a[8]; char b[8]; char out[16];
      strcpy(a, "cat");
      strcpy(b, "dog");
      strcpy(out, a + b);
      addLog(out);
    }
  `);
  assertEqual(output.replace(/\[LOG\] /g,'').trim(), 'catdog');
});

process.exit(failed > 0 ? 1 : 0);
