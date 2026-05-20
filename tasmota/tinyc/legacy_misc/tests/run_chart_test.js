import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { CodeGenerator } from '../src/codegen.js';
import { VM } from '../src/vm.js';
import { readFileSync } from 'fs';

const file = process.argv[2] || '../examples/chart_types.tc';
const source = readFileSync(file, 'utf8');

try {
    const tokens = new Lexer(source).tokenize();
    const ast = new Parser(tokens).parse();
    const result = new CodeGenerator().compile(ast);
    console.log(`Compiled: ${result.binary.length} bytes`);
    console.log('Functions:', Object.keys(result.functions).join(', '));
    console.log('Globals:', result.globalCount);

    const vm = new VM();
    vm.onOutput = (msg) => process.stdout.write(msg);
    vm.load(result);

    // Run main
    console.log('\n--- Running main ---');
    try {
        vm.run();
        console.log('main() completed OK');
    } catch (e) {
        console.error('main() error:', e.message);
        console.error('Stack:', e.stack.split('\n').slice(0, 5).join('\n'));
    }

    // Simulate WebPage callback
    if (result.functions['WebPage'] !== undefined) {
        console.log('\n--- Calling WebPage ---');
        try {
            vm.callFunction('WebPage');
            console.log('WebPage() completed OK');
        } catch (e) {
            console.error('WebPage() error:', e.message);
            console.error('Stack:', e.stack.split('\n').slice(0, 5).join('\n'));
        }
    }
} catch (e) {
    console.error('Error:', e.message);
    if (e.stack) console.error(e.stack.split('\n').slice(0, 5).join('\n'));
}
