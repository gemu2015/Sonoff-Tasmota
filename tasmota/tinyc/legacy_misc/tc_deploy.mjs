#!/usr/bin/env node
// TinyC deploy tool — compile .tc to bytecode and upload+run on device
// Usage: node tc_deploy.mjs <file.tc> <device-ip> [--run]
//
// Examples:
//   node tc_deploy.mjs examples/bresser_chart.tc 192.168.2.100 --run
//   node tc_deploy.mjs examples/blink.tc 192.168.2.100

import fs from 'fs';
import path from 'path';
import { preprocess } from './src/preprocessor.js';
import { Lexer } from './src/lexer.js';
import { Parser } from './src/parser.js';
import { CodeGenerator } from './src/codegen.js';

const args = process.argv.slice(2);
if (args.length < 2) {
    console.error('Usage: node tc_deploy.mjs <file.tc> <device-ip> [--run]');
    process.exit(1);
}

const tcFile = args[0];
const deviceIp = args[1];
const doRun = args.includes('--run');

// Step 1: Compile
console.log(`Compiling ${tcFile}...`);
const source = fs.readFileSync(tcFile, 'utf-8');

let prog;
try {
    const preprocessed = preprocess(source);
    const tokens = new Lexer(preprocessed).tokenize();
    const ast = new Parser(tokens).parse();
    const gen = new CodeGenerator();
    prog = gen.compile(ast);
} catch (e) {
    console.error(`Compile error: ${e.message}`);
    process.exit(1);
}

console.log(`  Code: ${prog.codeSize} bytes, Constants: ${prog.constants.length}, Binary: ${prog.binary.length} bytes`);

// Also save .tcb file locally
const tcbName = path.basename(tcFile, path.extname(tcFile)) + '.tcb';
const tcbPath = path.join(path.dirname(tcFile), tcbName);
fs.writeFileSync(tcbPath, Buffer.from(prog.binary));
console.log(`  Saved ${tcbPath}`);

// Step 2: Upload
console.log(`Uploading to ${deviceIp}...`);

const boundary = '----TinyCDeploy' + Date.now();
const binaryBuf = Buffer.from(prog.binary);

// Build multipart/form-data body manually (no FormData in Node without fetch polyfill)
const header = Buffer.from(
    `--${boundary}\r\n` +
    `Content-Disposition: form-data; name="file"; filename="${tcbName}"\r\n` +
    `Content-Type: application/octet-stream\r\n\r\n`
);
const footer = Buffer.from(`\r\n--${boundary}--\r\n`);
const body = Buffer.concat([header, binaryBuf, footer]);

try {
    // Pass `fsz` so the device can right-size its upload allocation instead
    // of always grabbing TC_MAX_PROGRAM (64 KB) — important on long-running
    // devices where heap fragmentation may not have a 64 KB contiguous block.
    const uploadResp = await fetch(`http://${deviceIp}/tc_upload?api=1&fsz=${binaryBuf.length}`, {
        method: 'POST',
        headers: {
            'Content-Type': `multipart/form-data; boundary=${boundary}`,
        },
        body: body,
    });

    if (!uploadResp.ok) {
        const text = await uploadResp.text();
        console.error(`Upload failed: HTTP ${uploadResp.status} — ${text}`);
        process.exit(1);
    }

    const uploadJson = await uploadResp.json().catch(() => ({}));
    console.log(`  Upload OK: ${uploadJson.size || binaryBuf.length} bytes on device`);
} catch (e) {
    console.error(`Upload error: ${e.message}`);
    process.exit(1);
}

// Step 3: Run (if --run flag)
if (doRun) {
    console.log('Starting program...');
    try {
        const runResp = await fetch(`http://${deviceIp}/tc_api?cmd=run`, {
            method: 'GET',
        });
        const runJson = await runResp.json().catch(() => ({}));
        if (runJson.ok) {
            console.log('  Program running on device!');
        } else {
            console.error(`  Run failed: ${runJson.error || 'unknown'}`);
        }
    } catch (e) {
        console.error(`Run error: ${e.message}`);
    }
}

console.log('Done.');
