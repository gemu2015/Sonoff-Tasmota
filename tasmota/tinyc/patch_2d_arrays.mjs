#!/usr/bin/env node
// patch_2d_arrays.mjs — add Phase 1 2D-char-array support to tinyc_ide.html.gz.
//
// Phase 1 scope:
//   • Declare `char buf[N][M]` (2D char arrays only — int/float deferred).
//   • Element access `buf[i][j]` (compiles to flat index i*M + j).
//   • Row reference `buf[i]` passed to char[] params → ADDR_HEAP_OFF
//     with offset = i*M (works for heap-resident arrays only — auto-heap
//     threshold is 16 elements, and any practical 2D dimension exceeds that).
//   • `buf` (whole array) passed to char[] params → entire flat ref (R*M chars).
//
// VM unchanged: ADDR_HEAP_OFF + LOAD/STORE_HEAP_ARR opcodes already exist.
//
// Usage: node patch_2d_arrays.mjs <input.html.gz> [output.html.gz]
//   default output = same path (in-place)

import fs from 'node:fs';
import zlib from 'node:zlib';

const inPath  = process.argv[2] || '/tmp/ide.html.gz';
const outPath = process.argv[3] || inPath;

let html = zlib.gunzipSync(fs.readFileSync(inPath)).toString('utf-8');
const orig = html;

function patch(name, find, replace, opts = {}) {
    const allowMissing = opts.allowMissing === true;
    if (!html.includes(find)) {
        if (allowMissing) {
            console.log(`[skip] ${name} (anchor not found, allowed)`);
            return;
        }
        throw new Error(`patch "${name}" — anchor not found:\n${find.slice(0, 80)}…`);
    }
    const before = html;
    html = html.replace(find, replace);
    if (html === before) throw new Error(`patch "${name}" — no replacement happened`);
    console.log(`[ok]   ${name}`);
}

// ─── 1. Parser: parseArrayDecl accepts a second [M] for char arrays ───
patch('parseArrayDecl: accept [N][M]',
    `    parseArrayDecl(varType, name) {
        const line = this.tokens[this.pos - 1].line;
        this.expect(TokenType.LBRACKET);
        let size = null;
        if (!this.check(TokenType.RBRACKET)) {
            size = this.parseExpression();
        }
        this.expect(TokenType.RBRACKET);

        let init = null;
        let stringInit = null;`,

    `    parseArrayDecl(varType, name) {
        const line = this.tokens[this.pos - 1].line;
        this.expect(TokenType.LBRACKET);
        let size = null;
        if (!this.check(TokenType.RBRACKET)) {
            size = this.parseExpression();
        }
        this.expect(TokenType.RBRACKET);

        // ── Phase 1: optional second dim for char[N][M]. Flat-stored
        //     row-major; dims live in symbol-table for compile-time
        //     index arithmetic and row-reference offsets.
        let cols = null;
        if (this.check(TokenType.LBRACKET)) {
            if (varType !== 'char') {
                throw new ParseError(
                    "2D arrays are currently supported for 'char' only " +
                    "(use #define stride + 1D for int/float)", line);
            }
            this.advance();
            cols = this.parseExpression();
            this.expect(TokenType.RBRACKET);
        }

        let init = null;
        let stringInit = null;`);

// ─── 2. Parser: include `cols` in the returned ArrayDecl node ──────────
patch('parseArrayDecl: emit cols field',
    `        return { type: NodeType.ArrayDecl, varType, name, size, init, stringInit, line };`,
    `        return { type: NodeType.ArrayDecl, varType, name, size, cols, init, stringInit, line };`);

// ─── 3. Parser: postfix [i][j] populates index2 on ArrayAccess ────────
patch('postfix expr: chained [i][j]',
    `            // Array access
            else if (this.check(TokenType.LBRACKET) && expr.type === NodeType.Identifier) {
                this.advance();
                const index = this.parseExpression();
                this.expect(TokenType.RBRACKET);
                expr = { type: NodeType.ArrayAccess, name: expr.name, index, line: expr.line };
            }`,

    `            // Array access — 1D: arr[i]  /  2D: arr[i][j]
            else if (this.check(TokenType.LBRACKET) && expr.type === NodeType.Identifier) {
                this.advance();
                const index = this.parseExpression();
                this.expect(TokenType.RBRACKET);
                let index2 = null;
                if (this.check(TokenType.LBRACKET)) {
                    this.advance();
                    index2 = this.parseExpression();
                    this.expect(TokenType.RBRACKET);
                }
                expr = { type: NodeType.ArrayAccess, name: expr.name, index, index2, line: expr.line };
            }`);

// ─── 4. Codegen: inferArraySize accounts for cols ─────────────────────
//   inferArraySize is the helper that turns a size expression into a number.
//   We need a wrapper that returns the FLAT size (size * cols if 2D).
//   Find the original and inject a flat-aware sibling.
patch('codegen: helper for flat 2D size',
    `    compileArrayDecl(node) {
        const size = this.inferArraySize(node);`,

    `    // Phase 1 2D helper — returns rows count, cols count (1 for 1D),
    // and total flat slot count.
    arrayDims(node) {
        const rows = this.inferArraySize(node);
        let cols = 1;
        if (node.cols != null) {
            // node.cols is an AST expression; reuse the size-evaluator
            cols = this.inferArraySize({ size: node.cols });
            if (!Number.isInteger(cols) || cols < 1) {
                throw new CodeGenError("2D array column count must be a positive integer constant", node.line);
            }
        }
        return { rows, cols, flat: rows * cols };
    }

    compileArrayDecl(node) {
        const dims = this.arrayDims(node);
        const size = dims.flat;`);

// ─── 5. Codegen: persist scope.define with cols metadata ───────────────
//   Local-array path. Need to remember cols on the symbol so accesses can
//   compute flat indices and row-refs later.
patch('compileArrayDecl: store cols on local sym (heap path)',
    `        if (size > HEAP_THRESHOLD) {
            const heapInfo = this.defineHeapArray(node.name, node.varType, size);
            this.scope.define(node.name, node.varType, true, size, { handle: heapInfo.heapHandle });`,

    `        if (size > HEAP_THRESHOLD) {
            const heapInfo = this.defineHeapArray(node.name, node.varType, size);
            const localSym = this.scope.define(node.name, node.varType, true, size, { handle: heapInfo.heapHandle });
            if (dims.cols > 1) { localSym.cols = dims.cols; localSym.rows = dims.rows; }`);

patch('compileArrayDecl: store cols on local sym (stack path)',
    `        const info = this.scope.define(node.name, node.varType, true, size);
        if (node.stringInit) {`,

    `        const info = this.scope.define(node.name, node.varType, true, size);
        if (dims.cols > 1) { info.cols = dims.cols; info.rows = dims.rows; }
        if (node.stringInit) {`);

// ─── 6. Codegen: global-decl pass — flat size + cols on global sym ─────
patch('global pass: flat size + cols metadata',
    `            } else if (node.type === NodeType.ArrayDecl) {
                const size = this.inferArraySize(node);
                const g = this.defineGlobal(node.name, node.varType, true, size, node.isPersist);`,

    `            } else if (node.type === NodeType.ArrayDecl) {
                const dims = this.arrayDims(node);
                const size = dims.flat;
                const g = this.defineGlobal(node.name, node.varType, true, size, node.isPersist);
                if (dims.cols > 1) { g.cols = dims.cols; g.rows = dims.rows; }`);

// ─── 7. Codegen: compileArrayAccess handles 2D ────────────────────────
//   For 2D `arr[i][j]`: push i, push cols, MUL, push j, ADD, then load.
//   For 2D `arr[i]` in value context: error ("ambiguous — use arr[i][j] or pass as char[]").
patch('compileArrayAccess: 2D element access',
    `    compileArrayAccess(node) {
        this.compileExpr(node.index);

        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local && local.isArray) {`,

    `    compileArrayAccess(node) {
        // Phase 1 2D: prefer scope/global lookup so we can compute flat
        // index from i * cols + j when both subscripts are present.
        const sym = (this.scope && this.scope.lookup(node.name)) ||
                    this.globals.get(node.name) || null;
        const is2D = sym && sym.cols && sym.cols > 1;
        if (is2D) {
            if (node.index2 == null) {
                throw new CodeGenError(
                    \`'\${node.name}' is a 2D char array — use \${node.name}[i][j] for an element, \` +
                    \`or pass \${node.name}[i] / \${node.name} to a char[] parameter for a row/whole reference\`,
                    node.line);
            }
            // Stack: i, cols, MUL, j, ADD  →  flat index
            this.compileExpr(node.index);
            this.emitPushInt(sym.cols);
            this.emit(Op.MUL);
            this.compileExpr(node.index2);
            this.emit(Op.ADD);
        } else {
            if (node.index2 != null) {
                throw new CodeGenError(
                    \`'\${node.name}' is a 1D array — second subscript not allowed\`, node.line);
            }
            this.compileExpr(node.index);
        }

        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local && local.isArray) {`);

// ─── 8. Codegen: emitArrayRef handles ArrayAccess of 2D (row passing) ──
//   emitArrayRef is called by string syscalls + UDF calls expecting char[].
//   We need to handle the case where the AST node is an ArrayAccess of a
//   2D char array — emit ADDR_HEAP_OFF with offset = i*cols.
patch('emitArrayRef: row reference for 2D arrays',
    `    // Emit an array address ref (for string functions)
    emitArrayRef(node) {
        // String concatenation expression: str1 + str2, str1 + "lit", etc.
        // Compiles to STRCONCAT syscall which pushes scratch_ref onto the stack.
        if (node.type === NodeType.BinaryExpr && node.op === '+' && this.isStringNode(node)) {
            this.compileBinaryExpr(node);
            return;
        }`,

    `    // Emit an array address ref (for string functions)
    emitArrayRef(node) {
        // String concatenation expression: str1 + str2, str1 + "lit", etc.
        // Compiles to STRCONCAT syscall which pushes scratch_ref onto the stack.
        if (node.type === NodeType.BinaryExpr && node.op === '+' && this.isStringNode(node)) {
            this.compileBinaryExpr(node);
            return;
        }
        // ── Phase 1 2D row reference: arr[i] of a 2D char array → ADDR_HEAP_OFF
        if (node.type === NodeType.ArrayAccess && node.index2 == null) {
            const sym = (this.scope && this.scope.lookup(node.name)) ||
                        this.globals.get(node.name) || null;
            if (sym && sym.cols && sym.cols > 1) {
                if (!sym.isHeap) {
                    throw new CodeGenError(
                        \`Row references for 2D arrays require heap storage \` +
                        \`(total >= \${typeof HEAP_THRESHOLD === 'number' ? HEAP_THRESHOLD + 1 : 17} \` +
                        \`elements). '\${node.name}' is too small (\${sym.rows}x\${sym.cols}).\`,
                        node.line);
                }
                // Emit row offset = index * cols, then ADDR_HEAP_OFF.
                this.compileExpr(node.index);
                this.emitPushInt(sym.cols);
                this.emit(Op.MUL);
                this.emit(Op.ADDR_HEAP_OFF);
                this.emitByte(sym.heapHandle);
                return;
            }
        }`);

// ─── 9. Codegen: UDF call routes 2D arr[i] arg through emitArrayRef ──
//   Currently, UDF arg compilation only routes Identifiers through
//   emitArrayRefByName. For a 2D-array `arr[i]` arg destined for a
//   char[] param, we need to call emitArrayRef on the ArrayAccess node
//   so it emits the row-offset reference.
patch('compileCallExpr: route 2D ArrayAccess args via emitArrayRef',
    `            if (isArrayParam &&
                node.args[i].type === NodeType.Identifier &&
                this.isArrayVar(node.args[i].name)) {
                // Array parameter (char[] or int[]): push array reference, not element value
                this.emitArrayRefByName(node.args[i].name, node.line);
            } else if (isArrayParam && node.args[i].type === NodeType.StringLiteral && param.type === 'char') {`,

    `            if (isArrayParam &&
                node.args[i].type === NodeType.Identifier &&
                this.isArrayVar(node.args[i].name)) {
                // Array parameter (char[] or int[]): push array reference, not element value
                this.emitArrayRefByName(node.args[i].name, node.line);
            } else if (isArrayParam &&
                       node.args[i].type === NodeType.ArrayAccess &&
                       node.args[i].index2 == null &&
                       (() => { const s = (this.scope && this.scope.lookup(node.args[i].name)) || this.globals.get(node.args[i].name); return s && s.cols && s.cols > 1; })()) {
                // 2D char array row reference: arr[i] passed to char[] param
                this.emitArrayRef(node.args[i]);
            } else if (isArrayParam && node.args[i].type === NodeType.StringLiteral && param.type === 'char') {`);

// ─── Phase 2 ─────────────────────────────────────────────────────────
// Two additions on top of Phase 1:
//   • Allow int / float 2D arrays in addition to char.
//   • Teach sprintf %s recognizer to accept arr[i] of a 2D char array
//     (was already accepted as Identifier; the new shape is ArrayAccess).
// All other 2D plumbing already type-agnostic — VM opcodes don't care
// about element type, and ADDR_HEAP_OFF / LOAD_HEAP_ARR work for any
// heap-resident array.

// ─── 11. Parser: drop the "char only" restriction on 2D ──────────────
patch('parseArrayDecl: allow int/float 2D arrays',
    `        let cols = null;
        if (this.check(TokenType.LBRACKET)) {
            if (varType !== 'char') {
                throw new ParseError(
                    "2D arrays are currently supported for 'char' only " +
                    "(use #define stride + 1D for int/float)", line);
            }
            this.advance();
            cols = this.parseExpression();
            this.expect(TokenType.RBRACKET);
        }`,

    `        let cols = null;
        if (this.check(TokenType.LBRACKET)) {
            // Phase 2: int / float / char 2D arrays. The runtime opcodes
            // (LOAD_HEAP_ARR, ADDR_HEAP_OFF) don't care about element
            // type — they index by int32 slots. char arrays still pack
            // 1 byte per slot at the syscall boundary; int / float
            // arrays use the full 32 bits per slot.
            this.advance();
            cols = this.parseExpression();
            this.expect(TokenType.RBRACKET);
        }`);

// ─── 12. Codegen helper: tell ArrayAccess of 2D apart ────────────────
//   Used by the sprintf-chain dispatcher (and any future caller) to
//   recognise that arr[i] of a 2D char array should be passed via
//   emitArrayRef rather than compileExpr.
patch('codegen: add is2DCharArrayAccess helper',
    `    // Emit an array address ref (for string functions)
    emitArrayRef(node) {`,

    `    // Phase 2 helper: returns true when the AST node is arr[i] of a
    // 2D char array (i.e. a row reference candidate). Called from the
    // sprintf dispatcher to route %s args through emitArrayRef.
    is2DCharArrayAccess(node) {
        if (!node || node.type !== NodeType.ArrayAccess) return false;
        if (node.index2 != null) return false;     // element access [i][j]
        const sym = (this.scope && this.scope.lookup(node.name)) ||
                    this.globals.get(node.name) || null;
        return !!(sym && sym.cols && sym.cols > 1 && sym.type === 'char');
    }

    // Emit an array address ref (for string functions)
    emitArrayRef(node) {`);

// ─── 13. sprintfUnified single-value path: recognise 2D row arg ──────
patch('sprintfUnified single-value: 2D row recognizer',
    `        // Single value path (3 args)
        const valArg = node.args[2];
        const resolvedVal = resolveArg(valArg);
        const valType = this.inferType(resolvedVal);
        const isCharArr = (resolvedVal.type === NodeType.Identifier && this.isCharArrayVar(resolvedVal.name));
        const isStringLit = (resolvedVal.type === NodeType.StringLiteral);`,

    `        // Single value path (3 args)
        const valArg = node.args[2];
        const resolvedVal = resolveArg(valArg);
        const valType = this.inferType(resolvedVal);
        const isCharArr = (resolvedVal.type === NodeType.Identifier && this.isCharArrayVar(resolvedVal.name)) ||
                          this.is2DCharArrayAccess(resolvedVal);
        const isStringLit = (resolvedVal.type === NodeType.StringLiteral);`);

// ─── 14. emitSprintfChain (variadic 4+ args): same 2D recognizer ─────
patch('emitSprintfChain: 2D row recognizer per segment',
    `            const useAppend = isAppend || firstEmitted;
            const valArg = valArgs[valIdx++];
            const resolvedVal = resolveArg(valArg);
            const isCharArr   = resolvedVal.type === NodeType.Identifier && this.isCharArrayVar(resolvedVal.name);
            const isStringLit = resolvedVal.type === NodeType.StringLiteral;`,

    `            const useAppend = isAppend || firstEmitted;
            const valArg = valArgs[valIdx++];
            const resolvedVal = resolveArg(valArg);
            const isCharArr   = (resolvedVal.type === NodeType.Identifier && this.isCharArrayVar(resolvedVal.name)) ||
                                this.is2DCharArrayAccess(resolvedVal);
            const isStringLit = resolvedVal.type === NodeType.StringLiteral;`);


// ─── 15. compileArrayAssign: 2D element write `arr[i][j] = …` ────────
//   The Phase 1 patch covered READS via compileArrayAccess. WRITES go
//   through compileArrayAssign which ALSO pushes the index — and was
//   only honouring the first one. Mirror the same i*cols+j math here.
patch('compileArrayAssign: 2D element write',
    `    compileArrayAssign(node) {
        this.compileExpr(node.index);
        const arrType = this.getVarType(node.name);`,

    `    compileArrayAssign(node) {
        // Phase 2 2D: arr[i][j] = expr → flatten i*cols + j onto the stack.
        const _sym = (this.scope && this.scope.lookup(node.name)) ||
                     this.globals.get(node.name) || null;
        const _is2D = _sym && _sym.cols && _sym.cols > 1;
        if (_is2D) {
            if (node.index2 == null) {
                throw new CodeGenError(
                    \`'\${node.name}' is a 2D array — assignment requires both indices, e.g. \${node.name}[i][j] = …\`,
                    node.line);
            }
            this.compileExpr(node.index);
            this.emitPushInt(_sym.cols);
            this.emit(Op.MUL);
            this.compileExpr(node.index2);
            this.emit(Op.ADD);
        } else {
            if (node.index2 != null) {
                throw new CodeGenError(
                    \`'\${node.name}' is a 1D array — second subscript not allowed in assignment\`, node.line);
            }
            this.compileExpr(node.index);
        }
        const arrType = this.getVarType(node.name);`);


// ─── 16. parseAssignment: ArrayAssign also carries index2 ───────────
//   When parsing `arr[i][j] = expr`, the LHS is parsed as ArrayAccess
//   with both `index` and `index2`. The Assignment-target conversion
//   only copied `index`, so the resulting ArrayAssign was 1D-shaped
//   and compileArrayAssign couldn't see the second subscript.
patch('parseAssignment: copy index2 to ArrayAssign',
    `            if (left.type === NodeType.ArrayAccess) {
                return {
                    type: NodeType.ArrayAssign,
                    name: left.name,
                    index: left.index,
                    op: op.value,
                    value: right,
                    line: left.line,
                };
            }`,

    `            if (left.type === NodeType.ArrayAccess) {
                return {
                    type: NodeType.ArrayAssign,
                    name: left.name,
                    index: left.index,
                    index2: left.index2,        // 2D: forward second subscript
                    op: op.value,
                    value: right,
                    line: left.line,
                };
            }`);


// ─── Sanity: ensure every patch landed by counting changes ────────────
const changedLines = (orig.split('\n').length - html.split('\n').length);
console.log(`\n${orig.length} → ${html.length} bytes (line delta ${changedLines})`);

if (html === orig) throw new Error('No changes applied — bailing.');

const gz = zlib.gzipSync(html, { level: 9 });
fs.writeFileSync(outPath, gz);
console.log(`Wrote ${outPath} (${gz.length} bytes gzipped)`);
