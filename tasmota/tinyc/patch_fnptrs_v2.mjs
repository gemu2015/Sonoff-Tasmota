#!/usr/bin/env node
// patch_fnptrs_v2.mjs — function pointers v2: as struct fields
//
// Builds on Phase B v1 (typedef'd fn-ptrs as locals/globals/params).
// v2 adds: fn-ptr as struct field, callable through member access.
//
//   typedef void (*cmd_handler)(char args[]);
//
//   struct CmdEntry { char name[12]; cmd_handler handler; }
//
//   CmdEntry cmds[3];
//   cmds[0].handler = do_on;          // works in v1 — assigns address to slot
//   cmds[i].handler(args);            // ← v2 NEW: call through member access
//
// Two patches:
//   1. Parser: a `(args)` after a MemberAccess produces a CallExpr with
//      callee: <member-access-node>. Same for MemberArrayAccess (rare —
//      array fields whose element is a fn-ptr — but consistent).
//   2. Codegen compileCallExpr: when node.callee is set, the callee is
//      an expression that yields a fn-ptr value. Look up the fn-ptr
//      signature via the struct field's type, push args, compile callee
//      (which leaves the address on stack), then OP_CALL_INDIRECT.
//
// Idempotent. VM unchanged (OP_CALL_INDIRECT shipped in v1).

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
// 1. Parser postfix: handle `(args)` after MemberAccess/MemberArrayAccess.
//    The result is a CallExpr with `callee: <the-access-node>`.
// ─────────────────────────────────────────────────────────────────────
patch('parser postfix: call after member access',
`            // Post-increment/decrement
            else if (this.check(TokenType.INC, TokenType.DEC)) {
                const op = this.advance();
                expr = { type: NodeType.PostfixExpr, op: op.value, operand: expr, line: expr.line };
            }
            else {
                break;
            }
        }`,
`            // Indirect call after a MemberAccess / MemberArrayAccess:
            //   obj.handler(arg1, arg2)        — fn-ptr struct field call
            //   arr[i].handler(arg1)
            // Produces a CallExpr with .callee = the access node and no .name.
            else if ((expr.type === NodeType.MemberAccess
                   || expr.type === NodeType.MemberArrayAccess)
                  && this.check(TokenType.LPAREN)) {
                this.advance(); // consume (
                const args = [];
                if (!this.check(TokenType.RPAREN)) {
                    do {
                        args.push(this.parseExpression());
                    } while (this.match(TokenType.COMMA));
                }
                this.expect(TokenType.RPAREN);
                expr = { type: NodeType.CallExpr, name: null, callee: expr, args, line: expr.line };
            }
            // Post-increment/decrement
            else if (this.check(TokenType.INC, TokenType.DEC)) {
                const op = this.advance();
                expr = { type: NodeType.PostfixExpr, op: op.value, operand: expr, line: expr.line };
            }
            else {
                break;
            }
        }`);

// ─────────────────────────────────────────────────────────────────────
// 2. Codegen compileCallExpr: handle callee-as-MemberAccess.
//    Look up the field's fnptr signature, push args, compile callee
//    (yields the fn-ptr value on stack), emit OP_CALL_INDIRECT.
// ─────────────────────────────────────────────────────────────────────
patch('compileCallExpr: handle indirect call through struct field',
`    compileCallExpr(node) {
        // sizeof(Tag) — compile-time constant slot count.`,
`    compileCallExpr(node) {
        // Indirect call through a struct field: \`obj.handler(args)\` or
        // \`arr[i].handler(args)\`. The parser left node.callee = the
        // MemberAccess/MemberArrayAccess node and node.name = null.
        if (node.callee && (node.callee.type === NodeType.MemberAccess
                         || node.callee.type === NodeType.MemberArrayAccess)) {
            const rm = this.resolveMember(node.callee.object, node.callee.field, node.callee.line || node.line);
            if (typeof rm.fieldType !== 'string' || !rm.fieldType.startsWith('fnptr:')) {
                throw new CodeGenError(
                    \`Field '\${node.callee.field}' is not a function pointer — cannot call\`,
                    node.callee.line || node.line);
            }
            const alias = rm.fieldType.slice(6);
            const sig = this.fnPtrTypes.get(alias);
            if (!sig) {
                throw new CodeGenError(\`Unknown fn-ptr type 'fnptr:\${alias}' for field '\${node.callee.field}'\`,
                                       node.callee.line || node.line);
            }
            if (node.args.length !== sig.params.length) {
                throw new CodeGenError(
                    \`fn-ptr field '\${node.callee.field}' expects \${sig.params.length} args, got \${node.args.length}\`,
                    node.line);
            }
            // Push args (with type coercion + array-ref handling like the v1 path)
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
            // Push the fn-ptr value: re-use the member-access codegen which
            // emits LOAD_X with the field offset; this leaves the address on TOS.
            this.compileMemberAccess({
                type: NodeType.MemberAccess,
                object: node.callee.object,
                field:  node.callee.field,
                line:   node.callee.line || node.line,
            });
            this.emit(Op.CALL_INDIRECT);
            if (sig.returnType !== 'void') this.hasValue = true;
            return;
        }

        // sizeof(Tag) — compile-time constant slot count.`);

// ─────────────────────────────────────────────────────────────────────
// 3. exprLeavesValue: also handle the indirect-call-through-field case.
// ─────────────────────────────────────────────────────────────────────
patch('exprLeavesValue: callee-on-member-access uses field signature',
`            // Fn-ptr call: read return type from the alias signature.
            const fpVar2 = this._lookupFnPtrVar && this._lookupFnPtrVar(node.name);
            if (fpVar2) {
                const sig = this.fnPtrTypes.get(fpVar2.fnPtrAlias);
                if (sig) return sig.returnType !== 'void';
            }
        }
        if (node.type === NodeType.PostfixExpr) return true;
        return true;
    }`,
`            // Fn-ptr call: read return type from the alias signature.
            const fpVar2 = this._lookupFnPtrVar && this._lookupFnPtrVar(node.name);
            if (fpVar2) {
                const sig = this.fnPtrTypes.get(fpVar2.fnPtrAlias);
                if (sig) return sig.returnType !== 'void';
            }
            // Indirect call through struct field: callee is a MemberAccess.
            if (node.callee && (node.callee.type === NodeType.MemberAccess
                             || node.callee.type === NodeType.MemberArrayAccess)) {
                try {
                    const rm = this.resolveMember(node.callee.object, node.callee.field, 0);
                    if (typeof rm.fieldType === 'string' && rm.fieldType.startsWith('fnptr:')) {
                        const sig2 = this.fnPtrTypes.get(rm.fieldType.slice(6));
                        if (sig2) return sig2.returnType !== 'void';
                    }
                } catch (_) {}
                return true;
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
