// Compile an example .c file and save the binary
import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { CodeGenerator } from '../src/codegen.js';
import { readFileSync, writeFileSync } from 'fs';

const file = process.argv[2];
if (!file) { console.error('Usage: node compile_example.js <file.c>'); process.exit(1); }

try {
    const source = readFileSync(file, 'utf8');
    const tokens = new Lexer(source).tokenize();
    const ast = new Parser(tokens).parse();
    const result = new CodeGenerator().compile(ast);

    const outFile = file.replace(/\.c$/, '.tcb');
    writeFileSync(outFile, Buffer.from(result.binary));
    console.log(`${result.binary.length} bytes -> ${outFile}`);
    console.log('Functions:', Object.keys(result.functions).join(', '));
    console.log('Globals:', Object.entries(result.globals).map(([k,v]) => `${k}@${v.index}`).join(', '));
} catch (e) {
    console.error('ERROR:', e.message);
    process.exit(1);
}
