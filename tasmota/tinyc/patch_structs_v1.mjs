#!/usr/bin/env node
// patch_structs_v1.mjs — finish struct support in tinyc_ide.html.gz
//
// The IDE already has substantial struct scaffolding (parseStructDecl,
// parseStructVarDecl, MemberAccess/Assign, MemberArrayAccess/Assign,
// resolveMember, etc.). This patcher adds incremental missing pieces.
//
// Strategy: each patch is independently idempotent. Run, probe with a
// smoke test, fix the next gap, repeat.
//
// Currently lands:
//   1. Parser: identifier matching a struct tag is a type
//      → enables `Point p;` (not just `struct Point p;`)
//   2. Parser: parseType resolves the bare-tag case to `struct:Tag`
//
// Next gaps (tested by examples/test_structs_v1.tc, will patch as found):
//   • whole-struct assignment a = b
//   • struct as function param
//   • struct as return value
//   • persist hash includes layout
//   • sizeof(StructTag)
//
// Idempotent: re-running on an already-patched file is a no-op (each
// patch detects via "already applied" check).
//
// Usage: node patch_structs_v1.mjs [input.html.gz] [output.html.gz]
//   default in/out  = ./tinyc_ide.html.gz (in-place)

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
// 1. isType() treats a known struct tag identifier as a type
// ─────────────────────────────────────────────────────────────────────
patch('isType: struct tag identifier counts as type',
`        if (t === TokenType.KW_STRUCT) return true;
        if (t === TokenType.IDENTIFIER && this.typeAliases.has(tok.value)) return true;
        return t === TokenType.KW_INT || t === TokenType.KW_FLOAT ||
               t === TokenType.KW_CHAR || t === TokenType.KW_BOOL || t === TokenType.KW_VOID;`,
`        if (t === TokenType.KW_STRUCT) return true;
        if (t === TokenType.IDENTIFIER && this.typeAliases.has(tok.value)) return true;
        if (t === TokenType.IDENTIFIER && this.structTypes.has(tok.value)) return true;
        return t === TokenType.KW_INT || t === TokenType.KW_FLOAT ||
               t === TokenType.KW_CHAR || t === TokenType.KW_BOOL || t === TokenType.KW_VOID;`);

// ─────────────────────────────────────────────────────────────────────
// 2. parseType() resolves a bare struct-tag identifier to struct:Tag
// ─────────────────────────────────────────────────────────────────────
patch('parseType: identifier resolves to struct:Tag if known',
`            case TokenType.IDENTIFIER: {
                // typedef'd name — resolve to its base type
                const resolved = this.typeAliases.get(tok.value);
                if (!resolved) {
                    throw new ParseError(\`Unknown type '\${tok.value}'\`, tok);
                }
                baseType = resolved;
                break;
            }`,
`            case TokenType.IDENTIFIER: {
                // typedef'd name — resolve to its base type
                const resolved = this.typeAliases.get(tok.value);
                if (resolved) { baseType = resolved; break; }
                // Bare struct tag (no preceding 'struct' keyword)
                if (this.structTypes.has(tok.value)) {
                    baseType = \`struct:\${tok.value}\`;
                    break;
                }
                throw new ParseError(\`Unknown type '\${tok.value}'\`, tok);
            }`);

// ─────────────────────────────────────────────────────────────────────
// 3. Top-level parse: `struct Tag funcName(...)` is a function decl,
//    not a struct-variable decl. Move the LPAREN check ahead of the
//    struct-var dispatch.
// ─────────────────────────────────────────────────────────────────────
patch('top-level: struct return type → function decl when next is "("',
`            if (type.startsWith('struct:')) {
                // Global struct variable declaration
                const node = this.parseStructVarDecl(type, name, true);
                if (isPersist) node.isPersist = true;
                program.body.push(node);
                continue;
            }

            if (this.check(TokenType.LPAREN)) {`,
`            if (type.startsWith('struct:')) {
                // Function decl with struct return type takes precedence
                if (this.check(TokenType.LPAREN)) {
                    if (isPersist) throw new ParseError("'persist' cannot be applied to functions", this.current());
                    if (isWatch)   throw new ParseError("'watch' cannot be applied to functions", this.current());
                    if (isGlobal)  throw new ParseError("'global' cannot be applied to functions", this.current());
                    program.body.push(this.parseFunctionDecl(type, name));
                    continue;
                }
                // Global struct variable declaration
                const node = this.parseStructVarDecl(type, name, true);
                if (isPersist) node.isPersist = true;
                program.body.push(node);
                continue;
            }

            if (this.check(TokenType.LPAREN)) {`);

// ─────────────────────────────────────────────────────────────────────
// 4. Nested field access: r.tl.x  (MemberAccess inside MemberAccess)
//
// Today resolveMember only handles object = Identifier or ArrayAccess.
// For nested struct fields the object is itself a MemberAccess node.
// We walk the chain inside-out, accumulating field offsets and
// validating each step's struct tag.
// ─────────────────────────────────────────────────────────────────────
patch('resolveMember: recurse through nested MemberAccess chains',
`    // Resolve struct member. \`object\` may be a plain variable name (string) or
    // an ArrayAccess AST node (e.g. \`gauges[i]\`) when the struct is an element
    // of a struct array.
    // Returns { info, isLocal, fieldOffset, fieldType, fieldIsArray, fieldArraySize,
    //          isArrayElement, indexExpr, structSlots }
    resolveMember(object, fieldName, line) {
        let objectName = object;
        let indexExpr = null;
        let isArrayElement = false;
        if (typeof object === 'object' && object !== null) {
            if (object.type === NodeType.ArrayAccess) {
                objectName = object.name;
                indexExpr = object.index;
                isArrayElement = true;
            } else if (object.name) {
                objectName = object.name;
            }
        }
        let info = null;
        let isLocal = false;

        if (this.scope) {
            const local = this.scope.lookup(objectName);
            if (local) {
                info = local;
                isLocal = true;
            }
        }
        if (!info) {
            info = this.globals.get(objectName);
        }
        if (!info) throw new CodeGenError(\`Undefined variable: '\${objectName}'\`, line);
        if (!info.isStruct) throw new CodeGenError(\`'\${objectName}' is not a struct\`, line);

        const members = this.structTypes.get(info.structTag);
        // Compute cumulative slot offset
        let fieldOffset = 0;
        let fieldMember = null;
        for (const m of members) {
            if (m.name === fieldName) { fieldMember = m; break; }
            fieldOffset += m.isArray ? m.arraySize : 1;
        }
        if (!fieldMember) {
            throw new CodeGenError(\`Struct '\${info.structTag}' has no member '\${fieldName}'\`, line);`,

`    // Resolve struct member. \`object\` may be:
    //   • a plain variable name (string)
    //   • an ArrayAccess AST node (e.g. \`gauges[i]\`)
    //   • a MemberAccess AST node (nested: \`a.b.c\`)
    // Returns { info, isLocal, fieldOffset, fieldType, fieldIsArray, fieldArraySize,
    //          isArrayElement, indexExpr, structSlots }
    resolveMember(object, fieldName, line) {
        // Nested field path: walk the MemberAccess chain inside-out, summing
        // field offsets along the way. Reduces to a base var + total offset.
        if (typeof object === 'object' && object !== null && object.type === NodeType.MemberAccess) {
            const inner = this.resolveMember(object.object, object.field, object.line || line);
            // \`inner\` describes \`object.field\` (the parent of \`fieldName\` in this call).
            // The parent must itself be a struct (nested type), with tag inferred from
            // the parent member's declared type (\`struct:InnerTag\`).
            const parentMembers = this.structTypes.get(inner.info.structTag);
            const parentMember  = parentMembers.find(m => m.name === object.field);
            if (!parentMember || typeof parentMember.type !== 'string'
                              || !parentMember.type.startsWith('struct:')) {
                throw new CodeGenError(
                    \`'\${object.field}' is not a struct field — nested access '\${object.field}.\${fieldName}' invalid\`,
                    object.line || line);
            }
            const innerTag = parentMember.type.slice(7);
            const innerMembers = this.structTypes.get(innerTag);
            if (!innerMembers) {
                throw new CodeGenError(\`Unknown nested struct type 'struct:\${innerTag}'\`, line);
            }
            // Find the requested field within the nested struct.
            let nestedOffset = 0;
            let nestedMember = null;
            for (const m of innerMembers) {
                if (m.name === fieldName) { nestedMember = m; break; }
                nestedOffset += m.isArray ? m.arraySize : 1;
            }
            if (!nestedMember) {
                throw new CodeGenError(
                    \`Nested struct '\${innerTag}' has no member '\${fieldName}'\`, line);
            }
            // Total offset: parent's field offset + nested field's offset within it.
            return {
                info:           inner.info,
                isLocal:        inner.isLocal,
                fieldOffset:    inner.fieldOffset + nestedOffset,
                fieldType:      nestedMember.type,
                fieldIsArray:   !!nestedMember.isArray,
                fieldArraySize: nestedMember.arraySize || 0,
                isArrayElement: inner.isArrayElement,
                indexExpr:      inner.indexExpr,
                structSlots:    inner.structSlots,
            };
        }
        let objectName = object;
        let indexExpr = null;
        let isArrayElement = false;
        if (typeof object === 'object' && object !== null) {
            if (object.type === NodeType.ArrayAccess) {
                objectName = object.name;
                indexExpr = object.index;
                isArrayElement = true;
            } else if (object.name) {
                objectName = object.name;
            }
        }
        let info = null;
        let isLocal = false;

        if (this.scope) {
            const local = this.scope.lookup(objectName);
            if (local) {
                info = local;
                isLocal = true;
            }
        }
        if (!info) {
            info = this.globals.get(objectName);
        }
        if (!info) throw new CodeGenError(\`Undefined variable: '\${objectName}'\`, line);
        if (!info.isStruct) throw new CodeGenError(\`'\${objectName}' is not a struct\`, line);

        const members = this.structTypes.get(info.structTag);
        // Compute cumulative slot offset
        let fieldOffset = 0;
        let fieldMember = null;
        for (const m of members) {
            if (m.name === fieldName) { fieldMember = m; break; }
            fieldOffset += m.isArray ? m.arraySize : 1;
        }
        if (!fieldMember) {
            throw new CodeGenError(\`Struct '\${info.structTag}' has no member '\${fieldName}'\`, line);`);

// ─────────────────────────────────────────────────────────────────────
// 5. memberSlotCount + resolveMember offset: count nested struct
//    fields by their actual slot count, not 1.
//
// Bug spotted with Rect{tl: Point, br: Point}: each Point is 2 slots,
// so Rect should be 4 slots and br's offset should be 2 — the existing
// code computed Rect=2 slots and br offset=1, causing reads/writes to
// stomp adjacent fields.
// ─────────────────────────────────────────────────────────────────────
patch('memberSlotCount: nested struct counts by inner total',
`    // Returns total slot count for a struct's members (array fields use arraySize slots)
    memberSlotCount(members) {
        return members.reduce((sum, m) => sum + (m.isArray ? m.arraySize : 1), 0);
    }`,
`    // Slot count of one struct member. Handles:
    //   • array field (uses arraySize)
    //   • nested struct field (recurses into inner struct's total)
    //   • everything else = 1 slot
    memberSlotsFor(m) {
        if (m.isArray) return m.arraySize;
        if (typeof m.type === 'string' && m.type.startsWith('struct:')) {
            const innerTag = m.type.slice(7);
            const innerMembers = this.structTypes.get(innerTag);
            if (!innerMembers) return 1;   // unknown — fail-safe; resolver will throw
            return this.memberSlotCount(innerMembers);
        }
        return 1;
    }
    // Returns total slot count for a struct's members.
    memberSlotCount(members) {
        return members.reduce((sum, m) => sum + this.memberSlotsFor(m), 0);
    }`);

// resolveMember's offset accumulation needs the same treatment.
patch('resolveMember: offset accumulator uses memberSlotsFor',
`        const members = this.structTypes.get(info.structTag);
        // Compute cumulative slot offset
        let fieldOffset = 0;
        let fieldMember = null;
        for (const m of members) {
            if (m.name === fieldName) { fieldMember = m; break; }
            fieldOffset += m.isArray ? m.arraySize : 1;
        }`,
`        const members = this.structTypes.get(info.structTag);
        // Compute cumulative slot offset (nested struct fields contribute
        // their own total slot count, not 1).
        let fieldOffset = 0;
        let fieldMember = null;
        for (const m of members) {
            if (m.name === fieldName) { fieldMember = m; break; }
            fieldOffset += this.memberSlotsFor(m);
        }`);

// And the nested-recursion path I just added has the same accumulator.
patch('nested recursion: offset accumulator uses memberSlotsFor',
`            // Find the requested field within the nested struct.
            let nestedOffset = 0;
            let nestedMember = null;
            for (const m of innerMembers) {
                if (m.name === fieldName) { nestedMember = m; break; }
                nestedOffset += m.isArray ? m.arraySize : 1;
            }`,
`            // Find the requested field within the nested struct.
            let nestedOffset = 0;
            let nestedMember = null;
            for (const m of innerMembers) {
                if (m.name === fieldName) { nestedMember = m; break; }
                nestedOffset += this.memberSlotsFor(m);
            }`);

// ─────────────────────────────────────────────────────────────────────
// 8. Whole-struct assignment: b = a;  arr[i] = b;  b = arr[i];  arr[i] = arr[j];
//
// Strategy: at the top of compileAssignment (and compileArrayAssign),
// detect struct-to-struct cases and emit per-slot LOAD/STORE pairs in
// dst-first order so the stack ends up correct without a SWAP opcode.
//
// Per slot:
//   PUSH dst_offset        # stack: [dst_off]
//   PUSH src_offset        # stack: [dst_off, src_off]
//   LOAD_X srcIdx          # stack: [dst_off, value]
//   STORE_X dstIdx         # stack: []
// ─────────────────────────────────────────────────────────────────────
patch('compileAssignment: detect struct-to-struct, route to compileStructCopy',
`    compileAssignment(node) {
        // ── String operations on char[] ──────────────────────`,
`    compileAssignment(node) {
        // ── Whole-struct copy: b = a;  b = arr[i];  ───────────
        // (LHS is an Identifier-style assignment; arr[i] = ... goes
        //  through compileArrayAssign instead.)
        if (node.op === '=' && this._isStructVar(node.name)
                            && this._isStructValueExpr(node.value)) {
            this._compileStructCopy(
                { kind: 'var', name: node.name },
                node.value, node.line);
            return;
        }
        // ── String operations on char[] ──────────────────────`);

// Add the helper methods. We place them at the top of the CodeGenerator
// class — anchor on compileMemberAccess which is well-located.
patch('codegen: add struct-copy helpers (_isStructVar, _isStructValueExpr, _compileStructCopy)',
`    compileMemberAccess(node) {
        const rm = this.resolveMember(node.object, node.field, node.line);`,
`    // Returns true if \`name\` resolves to a struct variable in scope/globals.
    _isStructVar(name) {
        if (!name) return false;
        const local = this.scope ? this.scope.lookup(name) : null;
        if (local && local.isStruct) return true;
        const g = this.globals.get(name);
        return !!(g && g.isStruct);
    }
    // Returns true if expression \`e\` evaluates to a struct value:
    // either an Identifier of a struct var, or an ArrayAccess into a
    // struct array. Future: add CallExpr returning struct (Day-5).
    _isStructValueExpr(e) {
        if (!e) return false;
        if (e.type === NodeType.Identifier) return this._isStructVar(e.name);
        if (e.type === NodeType.ArrayAccess) return this._isStructVar(e.name);
        return false;
    }
    // Resolve a struct lvalue/rvalue descriptor into:
    //   { info, isLocal, tag, structSlots, isArrayElement, indexExpr }
    _resolveStructAccess(desc) {
        let name, indexExpr = null, isArrayElement = false;
        if (desc.kind === 'var')   { name = desc.name; }
        else if (desc.kind === 'arr') { name = desc.name; indexExpr = desc.index; isArrayElement = true; }
        else if (desc.type === NodeType.Identifier) { name = desc.name; }
        else if (desc.type === NodeType.ArrayAccess) { name = desc.name; indexExpr = desc.index; isArrayElement = true; }
        else throw new CodeGenError("Internal: invalid struct access shape", 0);
        let info = null, isLocal = false;
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local) { info = local; isLocal = true; }
        }
        if (!info) info = this.globals.get(name);
        if (!info) throw new CodeGenError(\`Undefined variable: '\${name}'\`, 0);
        if (!info.isStruct) throw new CodeGenError(\`'\${name}' is not a struct\`, 0);
        const members = this.structTypes.get(info.structTag);
        return {
            info, isLocal,
            tag: info.structTag,
            structSlots: this.memberSlotCount(members),
            isArrayElement, indexExpr,
        };
    }
    // Push the slot offset of the \`off\`-th slot of a struct base onto the stack.
    // For a whole struct var: PUSH off. For an array element: i*structSlots + off.
    _pushStructSlotOffset(rm, off) {
        if (rm.isArrayElement) {
            this.compileExpr(rm.indexExpr);
            this.emitPushInt(rm.structSlots);
            this.emit(Op.MUL);
            if (off > 0) { this.emitPushInt(off); this.emit(Op.ADD); }
        } else {
            this.emitPushInt(off);
        }
    }
    // Emit a load of the struct slot whose offset is currently on top of stack.
    _emitStructSlotLoad(rm) {
        if (rm.info.isHeap) { this.emit(Op.LOAD_HEAP_ARR);   this.emitByte(rm.info.heapHandle); }
        else if (rm.isLocal){ this.emit(Op.LOAD_LOCAL_ARR);  this.emitByte(rm.info.index); }
        else                { this.emit(Op.LOAD_GLOBAL_ARR); this.emitU16(rm.info.index); }
    }
    // Emit a store. Stack must hold [..., dst_offset, value].
    _emitStructSlotStore(rm) {
        if (rm.info.isHeap) { this.emit(Op.STORE_HEAP_ARR);   this.emitByte(rm.info.heapHandle); }
        else if (rm.isLocal){ this.emit(Op.STORE_LOCAL_ARR);  this.emitByte(rm.info.index); }
        else                { this.emit(Op.STORE_GLOBAL_ARR); this.emitU16(rm.info.index); }
    }
    // Whole-struct copy. \`dst\` is a struct lvalue descriptor (var or array element);
    // \`src\` is an AST node (Identifier or ArrayAccess) referring to a struct.
    _compileStructCopy(dst, src, line) {
        const dstRm = this._resolveStructAccess(dst);
        const srcRm = this._resolveStructAccess(src);
        if (dstRm.tag !== srcRm.tag) {
            throw new CodeGenError(
                \`Cannot assign struct '\${srcRm.tag}' to struct '\${dstRm.tag}' — different types\`,
                line || 0);
        }
        const slots = dstRm.structSlots;
        for (let off = 0; off < slots; off++) {
            // PUSH dst_offset
            this._pushStructSlotOffset(dstRm, off);
            // PUSH src_offset
            this._pushStructSlotOffset(srcRm, off);
            // LOAD src
            this._emitStructSlotLoad(srcRm);
            // STORE dst (consumes [dst_off, value])
            this._emitStructSlotStore(dstRm);
        }
    }

    compileMemberAccess(node) {
        const rm = this.resolveMember(node.object, node.field, node.line);`);

// Also handle ArrayAssign for struct array element on LHS:  arr[i] = b;
patch('compileArrayAssign: detect struct array element, route to copy',
`    compileArrayAssign(node) {`,
`    compileArrayAssign(node) {
        // ── Whole-struct copy into array element: arr[i] = b; or arr[i] = arr[j];
        if (node.op === '=' && this._isStructVar(node.name)
                            && this._isStructValueExpr(node.value)) {
            this._compileStructCopy(
                { kind: 'arr', name: node.name, index: node.index },
                node.value, node.line);
            return;
        }
`);

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
