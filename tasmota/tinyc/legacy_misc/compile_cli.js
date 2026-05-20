#!/usr/bin/env node
// CLI compiler: node compile_cli.js <input.tc> [output.tcb] [-DNAME ...]
// Compiles TinyC source to .tcb bytecode binary

import { readFileSync, writeFileSync } from 'fs';
import { compile } from './src/compiler.js';

const args = process.argv.slice(2);
if (args.length < 1) {
    console.error('Usage: node compile_cli.js <input.tc> [output.tcb] [-DNAME ...]');
    process.exit(1);
}

// Separate file args from -D flags
const defines = [];
const fileArgs = [];
for (const arg of args) {
    if (arg.startsWith('-D')) {
        defines.push(arg.slice(2));  // strip -D prefix
    } else {
        fileArgs.push(arg);
    }
}

const inputFile = fileArgs[0];
const outputFile = fileArgs[1] || inputFile.replace(/\.tc$/, '.tcb');

try {
    const source = readFileSync(inputFile, 'utf-8');
    const result = compile(source, { defines });
    const binary = new Uint8Array(result.binary);
    writeFileSync(outputFile, binary);
    console.log(`Compiled ${inputFile} -> ${outputFile} (${binary.length} bytes)`);
} catch (e) {
    console.error(`Error: ${e.message}`);
    process.exit(1);
}
