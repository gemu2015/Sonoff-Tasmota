#!/usr/bin/env node
// patch_refparams_v1.mjs — reference parameters (Phase A v1) for TinyC
//
//   void swap(int& a, int& b) {
//       int tmp = a;
//       a = b;
//       b = tmp;
//   }
//
//   int x = 5, y = 7;
//   swap(x, y);                  // x and y swapped after call
//
// Implementation: zero new VM opcodes. Reuses the existing reference
// machinery (ADDR_LOCAL/ADDR_GLOBAL push encoded refs; LOAD_REF_ARR /
// STORE_REF_ARR dereference + access at an index). For a scalar `int&`
// param, the callee always reads/writes at index 0 of the ref.
//
// Caller side: for each scalar-ref arg, emit ADDR_LOCAL or ADDR_GLOBAL
// based on whether the arg is a local or global variable. Pushing the
// encoded ref onto the stack as if it were any other arg.
//
// Callee side: scalar-ref param is registered as a 1-slot local with
// isRef + isScalarRef flags. Inside the function body:
//   • read of `a`           → PUSH_I8 0; LOAD_REF_ARR <a_slot>
//   • write `a = expr`      → PUSH_I8 0; <compile expr>; STORE_REF_ARR <a_slot>
//
// v1 scope:
//   • int& only (no float& / char& / struct& yet — easy follow-up)
//   • Arg must be an Identifier of a local or global int variable
//   • Array elements (arr[i]), struct fields (obj.f) NOT yet allowed
//   • Compound assignment (a += 1) on ref-params handled in v1
//
// Idempotent. Usage: node patch_refparams_v1.mjs [in.html.gz] [out.html.gz]

import fs from 'node:fs';
import zlib from 'node:zlib';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname  = path.dirname(__filename);
const inPath  = process.argv[2] || path.join(__dirname, 'tinyc_ide.html.gz');
const outPath = process.argv[3] || inPath;

let html = zlib.gunzipSync(fs.readFileSync(inPath)).toString('utf-8');
const orig = html;

let stepNum = 0;
function patch(name, find, replace) {
    stepNum++;
    const tag = `${String(stepNum).padStart(2,'0')} ${name}`;
    if (html.includes(replace) && !html.includes(find)) {
        console.log(`[noop ] ${tag}`); return;
    }
    if (!html.includes(find)) {
        throw new Error(`patch "${name}" — anchor not found`);
    }
    html = html.replace(find, replace);
    console.log(`[ok   ] ${tag}`);
}

// ─────────────────────────────────────────────────────────────────────
// 1. Parser: accept `int& name` in function-param decls
// ─────────────────────────────────────────────────────────────────────
patch('parseFunctionDecl: accept & after type for ref params',
`        const params = [];
        if (!this.check(TokenType.RPAREN)) {
            do {
                const pType = this.parseType();
                const pName = this.expect(TokenType.IDENTIFIER).value;
                let arraySize = null;
                if (this.match(TokenType.LBRACKET)) {
                    // Array parameter: int arr[] or int arr[10]
                    if (!this.check(TokenType.RBRACKET)) {
                        arraySize = this.expect(TokenType.INT_LITERAL).value;
                    } else {
                        arraySize = 0;  // unsized array ref parameter
                    }
                    this.expect(TokenType.RBRACKET);
                }
                params.push({ type: pType, name: pName, arraySize });
            } while (this.match(TokenType.COMMA));
        }`,
`        const params = [];
        if (!this.check(TokenType.RPAREN)) {
            do {
                const pType = this.parseType();
                // Scalar reference: \`int& a\` — pass-by-reference for primitives.
                let isScalarRef = false;
                if (this.match(TokenType.AMPERSAND)) {
                    isScalarRef = true;
                }
                const pName = this.expect(TokenType.IDENTIFIER).value;
                let arraySize = null;
                if (this.match(TokenType.LBRACKET)) {
                    if (isScalarRef) {
                        throw new ParseError("'int& a' (ref) cannot be combined with '[]' (array)", this.current());
                    }
                    // Array parameter: int arr[] or int arr[10]
                    if (!this.check(TokenType.RBRACKET)) {
                        arraySize = this.expect(TokenType.INT_LITERAL).value;
                    } else {
                        arraySize = 0;  // unsized array ref parameter
                    }
                    this.expect(TokenType.RBRACKET);
                }
                params.push({ type: pType, name: pName, arraySize, isScalarRef });
            } while (this.match(TokenType.COMMA));
        }`);

// ─────────────────────────────────────────────────────────────────────
// 2. Codegen function entry: register scalar-ref param as a 1-slot
//    local with isRef + isScalarRef flags.
// ─────────────────────────────────────────────────────────────────────
patch('compileFunction: register scalar-ref param',
`        // Register parameters as locals
        for (const param of node.params) {
            // Struct-typed param: reserve memberSlotCount slots
            if (typeof param.type === 'string' && param.type.startsWith('struct:')) {`,
`        // Register parameters as locals
        for (const param of node.params) {
            // Scalar reference param (int& a, float& a, char& a). 1-slot local
            // holding an encoded ref (ADDR_LOCAL/ADDR_GLOBAL value), accessed
            // via LOAD/STORE_REF_ARR with index 0.
            if (param.isScalarRef) {
                const info = this.scope.define(param.name, param.type, true, 1);
                info.isRef       = true;
                info.isScalarRef = true;
                continue;
            }
            // Struct-typed param: reserve memberSlotCount slots
            if (typeof param.type === 'string' && param.type.startsWith('struct:')) {`);

// ─────────────────────────────────────────────────────────────────────
// 3. Caller side: when arg is for a scalar-ref param, push the encoded
//    ref via ADDR_LOCAL or ADDR_GLOBAL instead of compiling the value.
// ─────────────────────────────────────────────────────────────────────
patch('compileCallExpr: emit ADDR_LOCAL/GLOBAL for scalar-ref args',
`        // Push arguments (they become locals in the callee)
        for (let i = 0; i < node.args.length; i++) {
            const param = func.params[i];
            // Struct-typed parameter: push N slot values from the arg.
            if (typeof param.type === 'string' && param.type.startsWith('struct:')) {`,
`        // Push arguments (they become locals in the callee)
        for (let i = 0; i < node.args.length; i++) {
            const param = func.params[i];
            // Scalar-reference parameter (int& a, etc.): push the encoded
            // ref of the variable (ADDR_LOCAL or ADDR_GLOBAL), not its value.
            if (param.isScalarRef) {
                const arg = node.args[i];
                if (!arg || arg.type !== NodeType.Identifier) {
                    throw new CodeGenError(
                        \`Function '\${node.name}' arg \${i+1}: ref parameter requires a variable name (got \${arg && arg.type})\`,
                        node.line);
                }
                const localR = this.scope ? this.scope.lookup(arg.name) : null;
                if (localR) {
                    if (localR.isStaticAlias) {
                        const g = this.globals.get(localR.globalName);
                        if (!g) throw new CodeGenError(\`Static global '\${localR.globalName}' missing\`, node.line);
                        this.emit(Op.ADDR_GLOBAL); this.emitU16(g.index);
                    } else if (localR.isHeap) {
                        throw new CodeGenError(\`Cannot pass heap-array variable '\${arg.name}' as ref param (v1)\`, node.line);
                    } else {
                        this.emit(Op.ADDR_LOCAL); this.emitByte(localR.index);
                    }
                } else {
                    const g = this.globals.get(arg.name);
                    if (!g) throw new CodeGenError(\`Undefined variable '\${arg.name}' in ref arg\`, node.line);
                    this.emit(Op.ADDR_GLOBAL); this.emitU16(g.index);
                }
                continue;
            }
            // Struct-typed parameter: push N slot values from the arg.
            if (typeof param.type === 'string' && param.type.startsWith('struct:')) {`);

// ─────────────────────────────────────────────────────────────────────
// 4. Identifier read: a scalar-ref local reads through its ref at idx 0.
//    Hooked at the top of compileIdentifier alongside the v1 fn-name path.
// ─────────────────────────────────────────────────────────────────────
patch('compileIdentifier: scalar-ref local reads via LOAD_REF_ARR',
`    compileIdentifier(node) {
        // Function-name reference (no parens): emit PUSH_I32 of the function's`,
`    compileIdentifier(node) {
        // Scalar reference param: read through the encoded ref at index 0.
        if (this.scope) {
            const sr = this.scope.lookup(node.name);
            if (sr && sr.isScalarRef) {
                this.emit(Op.PUSH_I8);     this.emitByte(0);
                this.emit(Op.LOAD_REF_ARR); this.emitByte(sr.index);
                return;
            }
        }
        // Function-name reference (no parens): emit PUSH_I32 of the function's`);

// ─────────────────────────────────────────────────────────────────────
// 5. Assignment to scalar-ref local writes through the ref at idx 0.
// ─────────────────────────────────────────────────────────────────────
patch('compileAssignment: scalar-ref LHS writes via STORE_REF_ARR',
`    compileAssignment(node) {
        // ── Whole-struct copy: b = a;  b = arr[i];  ───────────`,
`    compileAssignment(node) {
        // ── Scalar reference assignment: a = expr;  (a is int& param)
        if (this.scope) {
            const sr = this.scope.lookup(node.name);
            if (sr && sr.isScalarRef) {
                if (node.op === '=') {
                    this.emit(Op.PUSH_I8); this.emitByte(0);     // idx 0
                    this.compileExpr(node.value);                // value
                    const valType = this.inferType(node.value);
                    if (sr.type === 'float' && valType !== 'float') this.emit(Op.I2F);
                    else if (sr.type !== 'float' && valType === 'float') this.emit(Op.F2I);
                    this.emit(Op.STORE_REF_ARR); this.emitByte(sr.index);
                } else {
                    // Compound assignment +=, -=, *=, /= etc.
                    const isFloat = sr.type === 'float';
                    // Stack: [idx, idx, oldVal, rhs, op, newVal]
                    this.emit(Op.PUSH_I8); this.emitByte(0);
                    this.emit(Op.DUP);
                    this.emit(Op.LOAD_REF_ARR); this.emitByte(sr.index);
                    this.compileExpr(node.value);
                    const valType = this.inferType(node.value);
                    if (isFloat && valType !== 'float') this.emit(Op.I2F);
                    else if (!isFloat && valType === 'float') this.emit(Op.F2I);
                    switch (node.op) {
                        case '+=':  this.emit(isFloat ? Op.FADD : Op.ADD);  break;
                        case '-=':  this.emit(isFloat ? Op.FSUB : Op.SUB);  break;
                        case '*=':  this.emit(isFloat ? Op.FMUL : Op.MUL);  break;
                        case '/=':  this.emit(isFloat ? Op.FDIV : Op.DIV);  break;
                        case '%=':  this.emit(Op.MOD);     break;
                        case '&=':  this.emit(Op.BIT_AND); break;
                        case '|=':  this.emit(Op.BIT_OR);  break;
                        case '^=':  this.emit(Op.BIT_XOR); break;
                        case '<<=': this.emit(Op.SHL);     break;
                        case '>>=': this.emit(Op.SHR);     break;
                        default: throw new CodeGenError(\`Unknown compound op on ref param: \${node.op}\`, node.line);
                    }
                    this.emit(Op.STORE_REF_ARR); this.emitByte(sr.index);
                }
                return;
            }
        }
        // ── Whole-struct copy: b = a;  b = arr[i];  ───────────`);

// ─────────────────────────────────────────────────────────────────────
// 6. inferType: a scalar-ref reads as its declared type
//    (currently locals report info.type which is already 'int'/'float'/'char',
//    so this is a no-op — the ref-ness is invisible at the type level).
//    Skipping explicit patch.
// ─────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────
// Write back
// ─────────────────────────────────────────────────────────────────────
if (html === orig) {
    console.log('No changes — already fully patched.');
} else {
    fs.writeFileSync(outPath, zlib.gzipSync(html, { level: 9 }));
    const sz = (fs.statSync(outPath).size / 1024).toFixed(1);
    console.log(`\nWrote ${outPath} (${sz} KB gzipped)`);
}
