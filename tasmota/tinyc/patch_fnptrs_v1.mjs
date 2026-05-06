#!/usr/bin/env node
// patch_fnptrs_v1.mjs — function pointers v1 (Phase B) for tinyc_ide.html.gz
//
// Adds typedef'd function-pointer types to the IDE compiler:
//
//   typedef int (*cmp_fn)(int, int);
//   typedef void (*handler_t)(char args[]);
//
//   handler_t fn;          // local or global var
//   fn = my_handler;       // assign a named function's address
//   fn(args);              // call through the pointer
//
// One new VM opcode: CALL_INDIRECT (0x56). Pops the target address
// from the data stack (instead of reading it as next 2 bytes of
// bytecode like CALL does), pushes a frame, jumps. Existing RET works
// unchanged for both direct and indirect calls.
//
// Out of v1 scope (deferred):
//   • Inline `void (*p)(int)` syntax — must use typedef.
//   • Function pointers as struct fields.
//   • Comparing fn pointers (`==`, `!=`).
//   • Returning fn pointers from functions.
//   • &fn syntax — assignment uses bare `fn`.
//
// Idempotent. Usage: node patch_fnptrs_v1.mjs [in.html.gz] [out.html.gz]

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
        console.log(`[noop ] ${tag}`);
        return;
    }
    if (!html.includes(find)) {
        throw new Error(`patch "${name}" — anchor not found`);
    }
    html = html.replace(find, replace);
    console.log(`[ok   ] ${tag}`);
}

// ─────────────────────────────────────────────────────────────────────
// 1. Op enum: add CALL_INDIRECT = 0x56
// ─────────────────────────────────────────────────────────────────────
patch('Op enum: CALL_INDIRECT 0x56',
`    CALL:           0x53,   // call function (2-byte address)
    RET:            0x54,   // return from function
    RET_VAL:        0x55,   // return with value on stack`,
`    CALL:           0x53,   // call function (2-byte address)
    RET:            0x54,   // return from function
    RET_VAL:        0x55,   // return with value on stack
    CALL_INDIRECT:  0x56,   // pop addr from stack, call (function pointer)`);

// ─────────────────────────────────────────────────────────────────────
// 2. JS VM: implement CALL_INDIRECT
// ─────────────────────────────────────────────────────────────────────
patch('JS VM: CALL_INDIRECT handler',
`            case Op.CALL: {
                const addr = this.readU16();
                if (this.frames.length >= MAX_FRAMES) throw new VMError('Call stack overflow', this.pc);

                // Save return address and frame pointer
                // Arguments are on the stack — callee pops them via STORE_LOCAL
                this.frames.push({
                    returnPC: this.pc,
                    prevFP: this.fp,
                });
                this.fp = this.frames.length - 1;
                this.pc = this.codeOffset + addr;
                break;
            }`,
`            case Op.CALL: {
                const addr = this.readU16();
                if (this.frames.length >= MAX_FRAMES) throw new VMError('Call stack overflow', this.pc);

                // Save return address and frame pointer
                // Arguments are on the stack — callee pops them via STORE_LOCAL
                this.frames.push({
                    returnPC: this.pc,
                    prevFP: this.fp,
                });
                this.fp = this.frames.length - 1;
                this.pc = this.codeOffset + addr;
                break;
            }
            case Op.CALL_INDIRECT: {
                // Same as CALL but the target address comes from the data
                // stack (top entry, masked to u16) instead of bytecode bytes.
                // Used by function-pointer call sites: the caller pushes args,
                // then loads the fn-ptr variable's value, then emits this op.
                const addr = this.pop() & 0xFFFF;
                if (this.frames.length >= MAX_FRAMES) throw new VMError('Call stack overflow', this.pc);
                this.frames.push({
                    returnPC: this.pc,
                    prevFP: this.fp,
                });
                this.fp = this.frames.length - 1;
                this.pc = this.codeOffset + addr;
                break;
            }`);

// (CALL_INDIRECT has 0 operand bytes, falls through default in disassembler.)

// ─────────────────────────────────────────────────────────────────────
// 4. Tokenizer: nothing new — '*' is already STAR for pointer types.
//    Parser side: extend Parser ctor to also have fnPtrTypes map.
// ─────────────────────────────────────────────────────────────────────
patch('Parser ctor: add fnPtrTypes map',
`        this.typeAliases  = new Map(); // name → base type string ('int', 'float', 'struct:Tag')`,
`        this.typeAliases  = new Map(); // name → base type string ('int', 'float', 'struct:Tag', 'fnptr:Alias')
        this.fnPtrTypes   = new Map(); // alias → { returnType, params: [{type, name?, isArray?}] }`);

// ─────────────────────────────────────────────────────────────────────
// 5. parseTypedef: detect `typedef <ret> (*<alias>)(<params>);` shape
//    after the leading return type.
// ─────────────────────────────────────────────────────────────────────
patch('parseTypedef: detect fn-ptr typedef shape after the leading type',
`        // ── typedef primitive|existing-type Alias; ────────────────────────────
        const baseType = this.parseType();  // handles int/float/char/bool/void + nested typedefs
        const alias = this.expect(TokenType.IDENTIFIER).value;
        this.expect(TokenType.SEMICOLON);
        this.typeAliases.set(alias, baseType);
        return { type: NodeType.TypedefDecl, alias, baseType, structDecl: null, line };
    }`,
`        // ── typedef primitive|existing-type Alias; ────────────────────────────
        // OR
        // ── typedef <retType> (*<alias>)(<param-list>); ── function-pointer alias
        const baseType = this.parseType();  // handles int/float/char/bool/void + nested typedefs

        // Function-pointer typedef detection: after the return type we see "(* …"
        if (this.check(TokenType.LPAREN) && this.peek(1).type === TokenType.STAR) {
            return this.parseFnPtrTypedef(baseType, line);
        }

        const alias = this.expect(TokenType.IDENTIFIER).value;
        this.expect(TokenType.SEMICOLON);
        this.typeAliases.set(alias, baseType);
        return { type: NodeType.TypedefDecl, alias, baseType, structDecl: null, line };
    }

    parseFnPtrTypedef(returnType, line) {
        this.expect(TokenType.LPAREN);
        this.expect(TokenType.STAR);
        const alias = this.expect(TokenType.IDENTIFIER).value;
        this.expect(TokenType.RPAREN);
        this.expect(TokenType.LPAREN);
        const params = [];
        if (!this.check(TokenType.RPAREN)) {
            do {
                const ptype = this.parseType();
                let pname = null;
                if (this.check(TokenType.IDENTIFIER)) {
                    pname = this.advance().value;
                }
                let isArr = false;
                if (this.match(TokenType.LBRACKET)) {
                    this.expect(TokenType.RBRACKET);
                    isArr = true;
                }
                params.push({ type: ptype, name: pname, isArray: isArr });
            } while (this.match(TokenType.COMMA));
        }
        this.expect(TokenType.RPAREN);
        this.expect(TokenType.SEMICOLON);
        this.fnPtrTypes.set(alias, { returnType, params });
        this.typeAliases.set(alias, \`fnptr:\${alias}\`);
        return {
            type: NodeType.TypedefDecl,
            alias,
            baseType: \`fnptr:\${alias}\`,
            fnPtrSig: { returnType, params },
            structDecl: null,
            line,
        };
    }`);

// ─────────────────────────────────────────────────────────────────────
// 6. Codegen: ctor — mirror fnPtrTypes from parser + tracking maps
// ─────────────────────────────────────────────────────────────────────
patch('CodeGenerator ctor: add fnPtrTypes + fnAddrPatches',
`        this.structTypes = new Map(); // tag -> [{ name, type }]`,
`        this.structTypes = new Map(); // tag -> [{ name, type }]
        this.fnPtrTypes  = new Map(); // alias → { returnType, params }
        this.fnAddrPatches = [];      // [{name, offset}] forward refs to function addresses (for fn-ptr assignment)`);

// ─────────────────────────────────────────────────────────────────────
// 7. Codegen: register fnPtrTypes from TypedefDecl during pre-pass
// ─────────────────────────────────────────────────────────────────────
patch('codegen pre-pass: register fnPtrTypes from TypedefDecl nodes',
`        // First pass: collect globals, functions, and defines`,
`        // Pre-pre-pass: register fn-ptr typedef aliases (so var decls can refer to them)
        for (const node of ast.body) {
            if (node.type === NodeType.TypedefDecl && node.fnPtrSig) {
                this.fnPtrTypes.set(node.alias, node.fnPtrSig);
            }
        }
        // First pass: collect globals, functions, and defines`);

// ─────────────────────────────────────────────────────────────────────
// 8. Identifier expr: when name matches a known function, emit
//    PUSH_I32 of its bytecode address (with forward-reference patching).
//    Hooked at the top of compileIdentifier so locals/globals still take
//    precedence (a local named "foo" shadows function "foo").
// ─────────────────────────────────────────────────────────────────────
patch('compileIdentifier: function name → PUSH_I32 of bytecode address',
`    compileIdentifier(node) {
        // Check locals first (locals shadow defines, like C preprocessor)
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local) {`,
`    compileIdentifier(node) {
        // Function-name reference (no parens): emit PUSH_I32 of the function's
        // bytecode address. Local/global var names override (so a local 'foo'
        // shadows a function 'foo' — same C scoping rule).
        if ((!this.scope || !this.scope.lookup(node.name)) &&
            !this.globals.has(node.name) &&
            this.functions && this.functions.has(node.name)) {
            const func = this.functions.get(node.name);
            // Emit PUSH_I32 with a placeholder; record forward-ref for patching.
            this.emit(Op.PUSH_I32);
            const offset = this.code.length;
            this.code.push(0, 0, 0, 0);
            // PUSH_I32 reads big-endian (matches readI32 / firmware tc_read_i32).
            // Address fits in low 16 bits → high two bytes stay 0.
            if (func.address >= 0) {
                this.code[offset]     = 0;                              // bits 31..24
                this.code[offset + 1] = 0;                              // bits 23..16
                this.code[offset + 2] = (func.address >> 8) & 0xFF;     // bits 15..8
                this.code[offset + 3] =  func.address       & 0xFF;     // bits  7..0
            } else {
                this.fnAddrPatches.push({ name: node.name, offset });
            }
            return;
        }
        // Check locals first (locals shadow defines, like C preprocessor)
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local) {`);

// ─────────────────────────────────────────────────────────────────────
// 9. After functions are compiled, patch all queued fn-address references.
// ─────────────────────────────────────────────────────────────────────
patch('after function-compile pass: resolve fnAddrPatches forward refs',
`        // Patch forward function calls (functions called before they were defined)
        for (const patch of this.patches) {
            const func = this.functions.get(patch.name);
            if (!func || func.address < 0) {
                throw new CodeGenError(\`Unresolved forward reference to '\${patch.name}'\`, patch.line);
            }
            this.patchJumpTo(patch.offset, func.address);
        }`,
`        // Patch forward function calls (functions called before they were defined)
        for (const patch of this.patches) {
            const func = this.functions.get(patch.name);
            if (!func || func.address < 0) {
                throw new CodeGenError(\`Unresolved forward reference to '\${patch.name}'\`, patch.line);
            }
            this.patchJumpTo(patch.offset, func.address);
        }
        // Patch fn-pointer address-of references (PUSH_I32 placeholders left
        // by compileIdentifier when a bare function name was used as a value).
        for (const fp of this.fnAddrPatches) {
            const func = this.functions.get(fp.name);
            if (!func || func.address < 0) {
                throw new CodeGenError(\`Unresolved fn-pointer reference to '\${fp.name}'\`, 0);
            }
            // PUSH_I32 reads big-endian; address fits in low 16 bits.
            this.code[fp.offset]     = 0;
            this.code[fp.offset + 1] = 0;
            this.code[fp.offset + 2] = (func.address >> 8) & 0xFF;
            this.code[fp.offset + 3] =  func.address       & 0xFF;
        }`);

// ─────────────────────────────────────────────────────────────────────
// 10. compileCallExpr: detect call through a fn-ptr variable.
//     If \`node.name\` resolves to a local or global with type
//     starting "fnptr:", push args, push the var's value (LOAD), then
//     emit CALL_INDIRECT.
// ─────────────────────────────────────────────────────────────────────
patch('compileCallExpr: detect fn-ptr call site, route to CALL_INDIRECT',
`        // User-defined function
        const func = this.functions.get(node.name);
        if (!func) {
            throw new CodeGenError(\`Undefined function: \${node.name}\`, node.line);
        }`,
`        // Function-pointer call site: if name resolves to a fnptr var, treat
        // the call as indirect — push args, then push the var's value (the
        // function's bytecode address), then emit Op.CALL_INDIRECT.
        const fpVar = this._lookupFnPtrVar(node.name);
        if (fpVar) {
            const sig = this.fnPtrTypes.get(fpVar.fnPtrAlias);
            if (sig) {
                if (node.args.length !== sig.params.length) {
                    throw new CodeGenError(
                        \`fn-ptr '\${node.name}' expects \${sig.params.length} args, got \${node.args.length}\`,
                        node.line);
                }
                for (let i = 0; i < node.args.length; i++) {
                    const param = sig.params[i];
                    const isArrayParam = !!param.isArray;
                    if (isArrayParam &&
                        node.args[i].type === NodeType.Identifier &&
                        this.isArrayVar(node.args[i].name)) {
                        this.emitArrayRefByName(node.args[i].name, node.line);
                    } else if (isArrayParam && node.args[i].type === NodeType.StringLiteral && param.type === 'char') {
                        this.emitStringArg(node.args[i]);
                    } else {
                        this.compileExpr(node.args[i]);
                        const argType = this.inferType(node.args[i]);
                        if (param.type === 'float' && !this.isFloatType(argType)) this.emit(Op.I2F);
                        else if (param.type !== 'float' && this.isFloatType(argType)) this.emit(Op.F2I);
                    }
                }
                // Push the fn-ptr value (function's bytecode address)
                this.compileExpr({ type: NodeType.Identifier, name: node.name, line: node.line });
                this.emit(Op.CALL_INDIRECT);
                if (sig.returnType !== 'void') this.hasValue = true;
                return;
            }
        }
        // User-defined function
        const func = this.functions.get(node.name);
        if (!func) {
            throw new CodeGenError(\`Undefined function: \${node.name}\`, node.line);
        }`);

// Helper: look up a fn-ptr variable (local or global) by name.
patch('codegen helper: _lookupFnPtrVar',
`    compileIdentifier(node) {`,
`    _lookupFnPtrVar(name) {
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local && typeof local.type === 'string' && local.type.startsWith('fnptr:')) {
                return { info: local, isLocal: true, fnPtrAlias: local.type.slice(6) };
            }
        }
        const g = this.globals.get(name);
        if (g && typeof g.type === 'string' && g.type.startsWith('fnptr:')) {
            return { info: g, isLocal: false, fnPtrAlias: g.type.slice(6) };
        }
        return null;
    }
    compileIdentifier(node) {`);

// ─────────────────────────────────────────────────────────────────────
// 11. inferType: a known function name as an expression has a fn-ptr
//     value; for now just return 'int' (the address). Avoids "unknown
//     identifier" warnings when typeAliases lookup is called.
// ─────────────────────────────────────────────────────────────────────
// (The existing inferType returns 'int' as default for unknown — so this
//  is a no-op. Skip explicit patch unless tests reveal a problem.)

// ─────────────────────────────────────────────────────────────────────
// 11. exprLeavesValue: a fn-ptr call to a void-returning signature
//     leaves NO value on the stack — without this, compileExprStmt emits
//     POP for void fn-ptr calls and underflows the stack. (For non-void
//     fn-ptrs the default return-true path is correct.)
// ─────────────────────────────────────────────────────────────────────
patch('exprLeavesValue: fn-ptr call returns by signature, not by default',
`            const builtin = BUILTINS[node.name];
            if (builtin) return builtin.returns;
            const func = this.functions.get(node.name);
            if (func) return func.returnType !== 'void';
        }
        if (node.type === NodeType.PostfixExpr) return true;
        return true;
    }`,
`            const builtin = BUILTINS[node.name];
            if (builtin) return builtin.returns;
            const func = this.functions.get(node.name);
            if (func) return func.returnType !== 'void';
            // Fn-ptr call: read return type from the alias signature.
            const fpVar2 = this._lookupFnPtrVar && this._lookupFnPtrVar(node.name);
            if (fpVar2) {
                const sig = this.fnPtrTypes.get(fpVar2.fnPtrAlias);
                if (sig) return sig.returnType !== 'void';
            }
        }
        if (node.type === NodeType.PostfixExpr) return true;
        return true;
    }`);

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
