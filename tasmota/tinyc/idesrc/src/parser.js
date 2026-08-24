import { TokenType } from './lexer.js';

// Supports: functions, variables, arrays, if/else, while, for, switch, expressions


export class ParseError extends Error {
    constructor(message, token) {
        super(`Parse error at ${token.line}:${token.col}: ${message}`);
        this.token = token;
    }
}

// AST Node types
export const NodeType = {
    Program:        'Program',
    FunctionDecl:   'FunctionDecl',
    VarDecl:        'VarDecl',
    ArrayDecl:      'ArrayDecl',
    DefineDecl:     'DefineDecl',
    Block:          'Block',
    IfStmt:         'IfStmt',
    WhileStmt:      'WhileStmt',
    ForStmt:        'ForStmt',
    SwitchStmt:     'SwitchStmt',
    CaseClause:     'CaseClause',
    ReturnStmt:     'ReturnStmt',
    BreakStmt:      'BreakStmt',
    ContinueStmt:   'ContinueStmt',
    ExprStmt:       'ExprStmt',
    Assignment:     'Assignment',
    ArrayAssign:    'ArrayAssign',
    BinaryExpr:     'BinaryExpr',
    UnaryExpr:      'UnaryExpr',
    PostfixExpr:    'PostfixExpr',
    CallExpr:       'CallExpr',
    ArrayAccess:    'ArrayAccess',
    Identifier:     'Identifier',
    IntLiteral:     'IntLiteral',
    FloatLiteral:   'FloatLiteral',
    CharLiteral:    'CharLiteral',
    StringLiteral:  'StringLiteral',
    BoolLiteral:    'BoolLiteral',
    CastExpr:       'CastExpr',
    TernaryExpr:    'TernaryExpr',
    DoWhileStmt:    'DoWhileStmt',
    StructDecl:     'StructDecl',
    StructVarDecl:  'StructVarDecl',
    MemberAccess:       'MemberAccess',
    MemberAssign:       'MemberAssign',
    MemberArrayAccess:  'MemberArrayAccess',
    MemberArrayAssign:  'MemberArrayAssign',
    TypedefDecl:        'TypedefDecl',
};

export class Parser {
    constructor(tokens) {
        this.tokens = tokens;
        this.pos = 0;
        this.structTypes = new Map(); // tag  → [{ name, type }]
        this.typeAliases  = new Map(); // name → base type string ('int', 'float', 'struct:Tag', 'fnptr:Alias')
        // `byte` is a built-in type NAME, not a keyword: two of our own examples
        // name a variable `byte` (lcd_i2c.tc:87, onewire.tc:184) and other
        // people's scripts surely do too. As an alias it behaves like a typedef:
        // a type where a type belongs, an ordinary name everywhere else. That
        // keeps everything existing intact.
        this.typeAliases.set('byte', 'byte');
        this.typeAliases.set('uint8_t', 'byte');   // uint8_t == byte (packed, unsigned 0..255)
        this.fnPtrTypes   = new Map(); // alias → { returnType, params: [{type, name?, isArray?}] }
    }

    // ─── Token helpers ───────────────────────────────────────────

    current() {
        return this.tokens[this.pos];
    }

    peek(offset = 0) {
        return this.tokens[this.pos + offset];
    }

    advance() {
        const tok = this.tokens[this.pos];
        this.pos++;
        return tok;
    }

    expect(type, message) {
        const tok = this.current();
        if (tok.type !== type) {
            throw new ParseError(
                message || `Expected ${type} but got ${tok.type} ('${tok.value}')`,
                tok
            );
        }
        return this.advance();
    }

    match(...types) {
        if (types.includes(this.current().type)) {
            return this.advance();
        }
        return null;
    }

    check(...types) {
        return types.includes(this.current().type);
    }

    isType() {
        // Skip const/static qualifiers when checking for a type
        let offset = 0;
        while (this.peek(offset).type === TokenType.KW_CONST ||
               this.peek(offset).type === TokenType.KW_STATIC) offset++;
        const tok = this.peek(offset);
        const t = tok.type;
        if (t === TokenType.KW_STRUCT) return true;
        if (t === TokenType.IDENTIFIER && this.typeAliases.has(tok.value)) {
            // An alias name is only a TYPE where a name follows it: `byte b[4]`
            // yes, `byte = byte >> 1` no. Otherwise every variable named like a
            // type would be unusable — which is exactly what happened with
            // `byte` in onewire.tc:190. Applies to all aliases, typedef names
            // used as variable names included.
            const naechst = this.peek(offset + 1).type;
            return naechst === TokenType.IDENTIFIER || naechst === TokenType.STAR;
        }
        if (t === TokenType.IDENTIFIER && this.structTypes.has(tok.value)) return true;
        return t === TokenType.KW_INT || t === TokenType.KW_FLOAT ||
               t === TokenType.KW_CHAR || t === TokenType.KW_BYTE ||
               t === TokenType.KW_BOOL || t === TokenType.KW_VOID;
    }

    parseType() {
        // Consume optional const/static qualifiers
        let isStatic = false;
        let isConst  = false;
        while (this.check(TokenType.KW_CONST) || this.check(TokenType.KW_STATIC)) {
            if (this.check(TokenType.KW_STATIC)) isStatic = true;
            if (this.check(TokenType.KW_CONST))  isConst  = true;
            this.advance();
        }
        const tok = this.advance();
        let baseType;
        switch (tok.type) {
            case TokenType.KW_INT:   baseType = 'int';   break;
            case TokenType.KW_FLOAT: baseType = 'float'; break;
            case TokenType.KW_CHAR:  baseType = 'char';  break;
            case TokenType.KW_BYTE:  baseType = 'byte';  break;
            case TokenType.KW_BOOL:  baseType = 'bool';  break;
            case TokenType.KW_VOID:  baseType = 'void';  break;
            case TokenType.KW_STRUCT: {
                const tag = this.expect(TokenType.IDENTIFIER).value;
                if (!this.structTypes.has(tag)) {
                    throw new ParseError(`Unknown struct type '${tag}'`, tok);
                }
                baseType = `struct:${tag}`;
                break;
            }
            case TokenType.IDENTIFIER: {
                // typedef'd name — resolve to its base type
                const resolved = this.typeAliases.get(tok.value);
                if (resolved) { baseType = resolved; break; }
                // Bare struct tag (no preceding 'struct' keyword)
                if (this.structTypes.has(tok.value)) {
                    baseType = `struct:${tok.value}`;
                    break;
                }
                throw new ParseError(`Unknown type '${tok.value}'`, tok);
            }
            default:
                throw new ParseError(`Expected type, got ${tok.type}`, tok);
        }
        this._lastIsStatic = isStatic;
        this._lastIsConst  = isConst;
        return baseType;
    }

    // ─── Top-level parsing ───────────────────────────────────────

    parse() {
        const program = { type: NodeType.Program, body: [], line: 1 };

        while (!this.check(TokenType.EOF)) {
            // #define
            if (this.check(TokenType.HASH)) {
                program.body.push(this.parseDefine());
                continue;
            }

            // typedef
            if (this.check(TokenType.KW_TYPEDEF)) {
                program.body.push(this.parseTypedef());
                continue;
            }

            // enum { A=0, B, C } — expand to #define constants
            if (this.check(TokenType.KW_ENUM)) {
                this.parseEnum().forEach(d => program.body.push(d));
                continue;
            }

            // struct Foo { ... }; — struct type declaration
            // struct Foo var;      — global struct variable (handled via isType below)
            if (this.check(TokenType.KW_STRUCT) && this.peek(2).type === TokenType.LBRACE) {
                program.body.push(this.parseStructDecl());
                continue;
            }

            // Check for storage qualifiers: persist, watch, global, const, static (combinable)
            let isPersist = false;
            let isWatch = false;
            let isGlobal = false;
            while (this.check(TokenType.KW_PERSIST) || this.check(TokenType.KW_WATCH) ||
                   this.check(TokenType.KW_GLOBAL)  || this.check(TokenType.KW_CONST) ||
                   this.check(TokenType.KW_STATIC)) {
                if (this.check(TokenType.KW_PERSIST))      { this.advance(); isPersist = true; }
                else if (this.check(TokenType.KW_WATCH))   { this.advance(); isWatch = true; }
                else if (this.check(TokenType.KW_GLOBAL))  { this.advance(); isGlobal = true; }
                else { this.advance(); } // consume const/static at top-level (no special meaning for globals)
            }

            // Must be a type: function or global var
            if (!this.isType()) {
                throw new ParseError(
                    `Expected type or #define at top level, got ${this.current().type}`,
                    this.current()
                );
            }

            // Look ahead to distinguish function from variable
            const type = this.parseType();
            const name = this.expect(TokenType.IDENTIFIER).value;

            if (type.startsWith('struct:')) {
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

            if (this.check(TokenType.LPAREN)) {
                // Function declaration
                if (isPersist) {
                    throw new ParseError("'persist' cannot be applied to functions", this.current());
                }
                if (isWatch) {
                    throw new ParseError("'watch' cannot be applied to functions", this.current());
                }
                if (isGlobal) {
                    throw new ParseError("'global' cannot be applied to functions", this.current());
                }
                program.body.push(this.parseFunctionDecl(type, name));
            } else if (this.check(TokenType.LBRACKET)) {
                // Global array declaration
                if (isWatch) {
                    throw new ParseError("'watch' can only be applied to scalar variables (int, float)", this.current());
                }
                if (isGlobal && type !== 'float' && type !== 'char') {
                    throw new ParseError("'global' can only be applied to float or char[] variables", this.current());
                }
                const node = this.parseArrayDecl(type, name);
                if (isPersist) node.isPersist = true;
                if (isGlobal) node.isGlobal = true;
                program.body.push(node);
            } else {
                // Global variable declaration
                if (isWatch && type !== 'int' && type !== 'float') {
                    throw new ParseError("'watch' can only be applied to int or float variables", this.current());
                }
                if (isGlobal && type !== 'float') {
                    throw new ParseError("'global' can only be applied to float variables", this.current());
                }
                const node = this.parseVarDeclRest(type, name);
                if (isPersist) node.isPersist = true;
                if (isWatch) node.isWatch = true;
                if (isGlobal) node.isGlobal = true;
                program.body.push(node);
            }
        }

        return program;
    }

    // ─── #define ─────────────────────────────────────────────────

    parseDefine() {
        const line = this.current().line;
        this.expect(TokenType.HASH);
        this.expect(TokenType.KW_DEFINE, "Expected 'define' after '#'");
        const name = this.expect(TokenType.IDENTIFIER).value;
        const value = this.parseExpression();
        // No semicolon needed for #define
        return { type: NodeType.DefineDecl, name, value, line };
    }

    parseStructDecl() {
        const line = this.current().line;
        this.expect(TokenType.KW_STRUCT);
        const tag = this.expect(TokenType.IDENTIFIER).value;
        this.expect(TokenType.LBRACE);
        const members = [];
        while (!this.check(TokenType.RBRACE) && !this.check(TokenType.EOF)) {
            // Allow const in member types (ignore it)
            while (this.check(TokenType.KW_CONST)) this.advance();
            const memberType = this.parseType();
            const memberName = this.expect(TokenType.IDENTIFIER).value;
            let memberIsArray = false;
            let memberArraySize = 0;
            if (this.match(TokenType.LBRACKET)) {
                const sizeExpr = this.parseExpression();
                if (sizeExpr.type !== NodeType.IntLiteral) {
                    throw new ParseError('Struct array member size must be a constant integer', this.current());
                }
                memberArraySize = sizeExpr.value;
                memberIsArray = true;
                this.expect(TokenType.RBRACKET);
            }
            this.expect(TokenType.SEMICOLON);
            members.push({ name: memberName, type: memberType, isArray: memberIsArray, arraySize: memberArraySize });
        }
        this.expect(TokenType.RBRACE);
        this.match(TokenType.SEMICOLON);
        this.structTypes.set(tag, members);
        return { type: NodeType.StructDecl, tag, members, line };
    }

    parseStructVarDecl(structType, name, isGlobal = false) {
        const line = this.tokens[this.pos - 1].line;
        const tag = structType.slice(7); // strip 'struct:'
        // Array of structs: struct Point pts[3];
        if (this.match(TokenType.LBRACKET)) {
            const sizeExpr = this.parseExpression();
            if (sizeExpr.type !== NodeType.IntLiteral) {
                throw new ParseError('Struct array size must be a constant integer', this.current());
            }
            const arraySize = sizeExpr.value;
            this.expect(TokenType.RBRACKET);
            this.expect(TokenType.SEMICOLON);
            return { type: NodeType.StructVarDecl, structType, name, tag, isArray: true, arraySize, init: null, line };
        }
        // Initializer: positional list  Point p = {1, 2};
        //         OR    expression       Point p = make_point(1, 2);
        let init = null;
        let initExpr = null;
        if (this.match(TokenType.ASSIGN)) {
            if (this.check(TokenType.LBRACE)) {
                this.advance();
                init = [];
                while (!this.check(TokenType.RBRACE) && !this.check(TokenType.EOF)) {
                    init.push(this.parseExpression());
                    if (!this.match(TokenType.COMMA)) break;
                }
                this.expect(TokenType.RBRACE);
            } else {
                initExpr = this.parseExpression();
            }
        }
        this.expect(TokenType.SEMICOLON);
        return { type: NodeType.StructVarDecl, structType, name, tag, isArray: false, arraySize: 0, init, initExpr, line };
    }

    parseTypedef() {
        const line = this.current().line;
        this.expect(TokenType.KW_TYPEDEF);

        // ── typedef struct { ... } Alias; — anonymous struct ──────────────────
        if (this.check(TokenType.KW_STRUCT) && this.peek(1).type === TokenType.LBRACE) {
            this.advance(); // consume 'struct'
            // Generate a unique internal tag for the anonymous struct
            const anonTag = `__anon_${this.structTypes.size}`;
            this.expect(TokenType.LBRACE);
            const members = [];
            while (!this.check(TokenType.RBRACE) && !this.check(TokenType.EOF)) {
                while (this.check(TokenType.KW_CONST)) this.advance();
                const memberType = this.parseType();
                const memberName = this.expect(TokenType.IDENTIFIER).value;
                let memberIsArray = false;
                let memberArraySize = 0;
                if (this.match(TokenType.LBRACKET)) {
                    const sizeExpr = this.parseExpression();
                    if (sizeExpr.type !== NodeType.IntLiteral) {
                        throw new ParseError('Struct array member size must be a constant integer', this.current());
                    }
                    memberArraySize = sizeExpr.value;
                    memberIsArray = true;
                    this.expect(TokenType.RBRACKET);
                }
                this.expect(TokenType.SEMICOLON);
                members.push({ name: memberName, type: memberType, isArray: memberIsArray, arraySize: memberArraySize });
            }
            this.expect(TokenType.RBRACE);
            const alias = this.expect(TokenType.IDENTIFIER).value;
            this.expect(TokenType.SEMICOLON);
            // Register under both the anonymous tag and the alias (so struct lookup works)
            this.structTypes.set(anonTag, members);
            this.structTypes.set(alias, members);
            this.typeAliases.set(alias, `struct:${alias}`);
            // Return a StructDecl so the codegen pre-pass registers the type
            return {
                type: NodeType.TypedefDecl,
                alias,
                baseType: `struct:${alias}`,
                structDecl: { type: NodeType.StructDecl, tag: alias, members, line },
                line,
            };
        }

        // ── typedef struct Tag Alias; — named struct alias ────────────────────
        if (this.check(TokenType.KW_STRUCT)) {
            this.advance(); // consume 'struct'
            const tag = this.expect(TokenType.IDENTIFIER).value;
            const alias = this.expect(TokenType.IDENTIFIER).value;
            this.expect(TokenType.SEMICOLON);
            this.typeAliases.set(alias, `struct:${tag}`);
            return { type: NodeType.TypedefDecl, alias, baseType: `struct:${tag}`, structDecl: null, line };
        }

        // ── typedef primitive|existing-type Alias; ────────────────────────────
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
        this.typeAliases.set(alias, `fnptr:${alias}`);
        return {
            type: NodeType.TypedefDecl,
            alias,
            baseType: `fnptr:${alias}`,
            fnPtrSig: { returnType, params },
            structDecl: null,
            line,
        };
    }

    parseEnum() {
        // enum [Name] { A [= val], B, C, ... } [;]
        const line = this.current().line;
        this.expect(TokenType.KW_ENUM);
        // Optional enum name — consume and ignore
        if (this.check(TokenType.IDENTIFIER)) this.advance();
        this.expect(TokenType.LBRACE);
        const defines = [];
        let counter = 0;
        while (!this.check(TokenType.RBRACE) && !this.check(TokenType.EOF)) {
            const name = this.expect(TokenType.IDENTIFIER).value;
            if (this.match(TokenType.ASSIGN)) {
                // Parse integer value (optional leading minus)
                const neg = this.match(TokenType.MINUS);
                const valTok = this.advance();
                if (valTok.type !== TokenType.INT_LITERAL) {
                    throw new ParseError('enum value must be an integer', valTok);
                }
                counter = neg ? -valTok.value : valTok.value;
            }
            defines.push({
                type: NodeType.DefineDecl,
                name,
                value: { type: NodeType.IntLiteral, value: counter, line },
                line,
            });
            counter++;
            if (!this.match(TokenType.COMMA)) break;
            // Allow trailing comma
            if (this.check(TokenType.RBRACE)) break;
        }
        this.expect(TokenType.RBRACE);
        this.match(TokenType.SEMICOLON); // optional ;
        return defines;
    }

    // ─── Function declaration ────────────────────────────────────

    parseFunctionDecl(returnType, name) {
        const line = this.current().line;
        this.expect(TokenType.LPAREN);

        const params = [];
        if (!this.check(TokenType.RPAREN)) {
            do {
                const pType = this.parseType();
                // Scalar reference: `int& a` — pass-by-reference for primitives.
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
        }
        this.expect(TokenType.RPAREN);

        const body = this.parseBlock();

        return {
            type: NodeType.FunctionDecl,
            returnType,
            name,
            params,
            body,
            line,
        };
    }

    // ─── Block ───────────────────────────────────────────────────

    parseBlock() {
        const line = this.current().line;
        this.expect(TokenType.LBRACE);
        const stmts = [];
        while (!this.check(TokenType.RBRACE) && !this.check(TokenType.EOF)) {
            stmts.push(this.parseStatement());
        }
        this.expect(TokenType.RBRACE);
        return { type: NodeType.Block, body: stmts, line };
    }

    // ─── Statements ──────────────────────────────────────────────

    parseStatement() {
        // typedef inside a function body
        if (this.check(TokenType.KW_TYPEDEF)) {
            return this.parseTypedef();
        }

        // struct Foo { ... }; — struct type declaration inside a function
        if (this.check(TokenType.KW_STRUCT) && this.peek(2).type === TokenType.LBRACE) {
            return this.parseStructDecl();
        }

        // Variable/array declaration (with optional const/static)
        if (this.isType()) {
            this._lastIsStatic = false;
            this._lastIsConst  = false;
            const type = this.parseType();
            const isStatic = this._lastIsStatic;
            // struct Foo var; — struct variable declaration inside a function
            if (type.startsWith('struct:')) {
                const name = this.expect(TokenType.IDENTIFIER).value;
                const node = this.parseStructVarDecl(type, name);
                if (isStatic) node.isStatic = true;
                return node;
            }
            const name = this.expect(TokenType.IDENTIFIER).value;
            let node;
            if (this.check(TokenType.LBRACKET)) {
                node = this.parseArrayDecl(type, name);
            } else {
                node = this.parseVarDeclRest(type, name);
            }
            if (isStatic) node.isStatic = true;
            return node;
        }

        // enum inside function (inline constants)
        if (this.check(TokenType.KW_ENUM)) {
            const line = this.current().line;
            const defs = this.parseEnum();
            // Wrap multiple defines in a block
            return { type: NodeType.Block, body: defs, line };
        }

        // Control flow
        if (this.check(TokenType.KW_IF))       return this.parseIf();
        if (this.check(TokenType.KW_WHILE))    return this.parseWhile();
        if (this.check(TokenType.KW_DO))       return this.parseDoWhile();
        if (this.check(TokenType.KW_FOR))      return this.parseFor();
        if (this.check(TokenType.KW_SWITCH))   return this.parseSwitch();
        if (this.check(TokenType.KW_RETURN))   return this.parseReturn();
        if (this.check(TokenType.KW_BREAK))    return this.parseBreak();
        if (this.check(TokenType.KW_CONTINUE)) return this.parseContinue();
        if (this.check(TokenType.LBRACE))      return this.parseBlock();

        // Expression statement (assignment, function call, etc.)
        return this.parseExpressionStatement();
    }

    parseDoWhile() {
        const line = this.current().line;
        this.expect(TokenType.KW_DO);
        const body = this.parseBlock();
        this.expect(TokenType.KW_WHILE);
        this.expect(TokenType.LPAREN);
        const condition = this.parseExpression();
        this.expect(TokenType.RPAREN);
        this.expect(TokenType.SEMICOLON);
        return { type: NodeType.DoWhileStmt, body, condition, line };
    }

    // ─── Variable declaration ────────────────────────────────────

    parseVarDeclRest(varType, name) {
        const line = this.tokens[this.pos - 1].line;
        let init = null;
        if (this.match(TokenType.ASSIGN)) {
            init = this.parseExpression();
        }
        this.expect(TokenType.SEMICOLON, "Expected ';' after variable declaration");
        return { type: NodeType.VarDecl, varType, name, init, line };
    }

    // ─── Array declaration ───────────────────────────────────────

    parseArrayDecl(varType, name) {
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
            // Phase 2: int / float / char 2D arrays. The runtime opcodes
            // (LOAD_HEAP_ARR, ADDR_HEAP_OFF) don't care about element
            // type — they index by int32 slots. char arrays still pack
            // 1 byte per slot at the syscall boundary; int / float
            // arrays use the full 32 bits per slot.
            this.advance();
            cols = this.parseExpression();
            this.expect(TokenType.RBRACKET);
        }

        let init = null;
        let stringInit = null;
        if (this.match(TokenType.ASSIGN)) {
            if (this.check(TokenType.STRING_LITERAL)) {
                // String literal initializer: char buf[32] = "hello"
                stringInit = this.advance().value;
            } else {
                // Array initializer: {1, 2, 3}
                this.expect(TokenType.LBRACE);
                init = [];
                if (!this.check(TokenType.RBRACE)) {
                    do {
                        init.push(this.parseExpression());
                    } while (this.match(TokenType.COMMA));
                }
                this.expect(TokenType.RBRACE);
            }
        }
        this.expect(TokenType.SEMICOLON, "Expected ';' after array declaration");

        return { type: NodeType.ArrayDecl, varType, name, size, cols, init, stringInit, line };
    }

    // ─── Control flow ────────────────────────────────────────────

    parseIf() {
        const line = this.current().line;
        this.expect(TokenType.KW_IF);
        this.expect(TokenType.LPAREN);
        const condition = this.parseExpression();
        this.expect(TokenType.RPAREN);
        const consequent = this.parseStatement();
        let alternate = null;
        if (this.match(TokenType.KW_ELSE)) {
            alternate = this.parseStatement();
        }
        return { type: NodeType.IfStmt, condition, consequent, alternate, line };
    }

    parseWhile() {
        const line = this.current().line;
        this.expect(TokenType.KW_WHILE);
        this.expect(TokenType.LPAREN);
        const condition = this.parseExpression();
        this.expect(TokenType.RPAREN);
        const body = this.parseStatement();
        return { type: NodeType.WhileStmt, condition, body, line };
    }

    parseFor() {
        const line = this.current().line;
        this.expect(TokenType.KW_FOR);
        this.expect(TokenType.LPAREN);

        // Init: could be var decl or expression
        let init = null;
        if (this.isType()) {
            const type = this.parseType();
            const name = this.expect(TokenType.IDENTIFIER).value;
            init = this.parseVarDeclRest(type, name);
            // parseVarDeclRest already consumed the semicolon
        } else if (!this.check(TokenType.SEMICOLON)) {
            init = this.parseExpression();
            this.expect(TokenType.SEMICOLON);
        } else {
            this.expect(TokenType.SEMICOLON);
        }

        // Condition
        let condition = null;
        if (!this.check(TokenType.SEMICOLON)) {
            condition = this.parseExpression();
        }
        this.expect(TokenType.SEMICOLON);

        // Update
        let update = null;
        if (!this.check(TokenType.RPAREN)) {
            update = this.parseExpression();
        }
        this.expect(TokenType.RPAREN);

        const body = this.parseStatement();

        return { type: NodeType.ForStmt, init, condition, update, body, line };
    }

    parseSwitch() {
        const line = this.current().line;
        this.expect(TokenType.KW_SWITCH);
        this.expect(TokenType.LPAREN);
        const discriminant = this.parseExpression();
        this.expect(TokenType.RPAREN);
        this.expect(TokenType.LBRACE);

        const cases = [];
        while (!this.check(TokenType.RBRACE) && !this.check(TokenType.EOF)) {
            if (this.check(TokenType.KW_CASE)) {
                this.advance();
                const value = this.parseExpression();
                this.expect(TokenType.COLON);
                const body = [];
                while (!this.check(TokenType.KW_CASE) &&
                       !this.check(TokenType.KW_DEFAULT) &&
                       !this.check(TokenType.RBRACE)) {
                    body.push(this.parseStatement());
                }
                cases.push({ type: NodeType.CaseClause, value, body, isDefault: false, line: value.line });
            } else if (this.check(TokenType.KW_DEFAULT)) {
                this.advance();
                this.expect(TokenType.COLON);
                const body = [];
                while (!this.check(TokenType.KW_CASE) &&
                       !this.check(TokenType.KW_DEFAULT) &&
                       !this.check(TokenType.RBRACE)) {
                    body.push(this.parseStatement());
                }
                cases.push({ type: NodeType.CaseClause, value: null, body, isDefault: true, line });
            }
        }
        this.expect(TokenType.RBRACE);

        return { type: NodeType.SwitchStmt, discriminant, cases, line };
    }

    parseReturn() {
        const line = this.current().line;
        this.expect(TokenType.KW_RETURN);
        let value = null;
        if (!this.check(TokenType.SEMICOLON)) {
            value = this.parseExpression();
        }
        this.expect(TokenType.SEMICOLON, "Expected ';' after return");
        return { type: NodeType.ReturnStmt, value, line };
    }

    parseBreak() {
        const line = this.current().line;
        this.expect(TokenType.KW_BREAK);
        this.expect(TokenType.SEMICOLON);
        return { type: NodeType.BreakStmt, line };
    }

    parseContinue() {
        const line = this.current().line;
        this.expect(TokenType.KW_CONTINUE);
        this.expect(TokenType.SEMICOLON);
        return { type: NodeType.ContinueStmt, line };
    }

    // ─── Expression statement ────────────────────────────────────

    parseExpressionStatement() {
        const expr = this.parseExpression();
        this.expect(TokenType.SEMICOLON, "Expected ';' after expression");
        return { type: NodeType.ExprStmt, expr, line: expr.line };
    }

    // ─── Expressions (precedence climbing) ───────────────────────

    parseExpression() {
        return this.parseAssignment();
    }

    parseAssignment() {
        const left = this.parseTernary();

        // Check for assignment operators
        const assignOps = [
            TokenType.ASSIGN, TokenType.PLUS_ASSIGN,
            TokenType.MINUS_ASSIGN, TokenType.STAR_ASSIGN,
            TokenType.SLASH_ASSIGN, TokenType.PERCENT_ASSIGN,
            TokenType.AND_ASSIGN, TokenType.OR_ASSIGN,
            TokenType.XOR_ASSIGN, TokenType.LSHIFT_ASSIGN,
            TokenType.RSHIFT_ASSIGN,
        ];

        if (this.check(...assignOps)) {
            const op = this.advance();
            const right = this.parseAssignment(); // right-associative

            if (left.type === NodeType.Identifier) {
                return {
                    type: NodeType.Assignment,
                    name: left.name,
                    op: op.value,
                    value: right,
                    line: left.line,
                };
            }
            if (left.type === NodeType.ArrayAccess) {
                return {
                    type: NodeType.ArrayAssign,
                    name: left.name,
                    index: left.index,
                    index2: left.index2,        // 2D: forward second subscript
                    op: op.value,
                    value: right,
                    line: left.line,
                };
            }
            if (left.type === NodeType.MemberAccess) {
                return {
                    type: NodeType.MemberAssign,
                    object: left.object,
                    field: left.field,
                    op: op.value,
                    value: right,
                    line: left.line,
                };
            }
            if (left.type === NodeType.MemberArrayAccess) {
                return {
                    type: NodeType.MemberArrayAssign,
                    object: left.object,
                    field: left.field,
                    index: left.index,
                    op: op.value,
                    value: right,
                    line: left.line,
                };
            }
            throw new ParseError('Invalid assignment target', op);
        }

        return left;
    }

    parseTernary() {
        const condition = this.parseOr();
        if (this.match(TokenType.QUESTION)) {
            const line = this.tokens[this.pos - 1].line;
            const consequent = this.parseAssignment(); // right-assoc
            this.expect(TokenType.COLON);
            const alternate = this.parseAssignment();
            return { type: NodeType.TernaryExpr, condition, consequent, alternate, line };
        }
        return condition;
    }

    parseOr() {
        let left = this.parseAnd();
        while (this.match(TokenType.OR)) {
            const right = this.parseAnd();
            left = { type: NodeType.BinaryExpr, op: '||', left, right, line: left.line };
        }
        return left;
    }

    parseAnd() {
        let left = this.parseBitwiseOr();
        while (this.match(TokenType.AND)) {
            const right = this.parseBitwiseOr();
            left = { type: NodeType.BinaryExpr, op: '&&', left, right, line: left.line };
        }
        return left;
    }

    parseBitwiseOr() {
        let left = this.parseBitwiseXor();
        while (this.match(TokenType.PIPE)) {
            const right = this.parseBitwiseXor();
            left = { type: NodeType.BinaryExpr, op: '|', left, right, line: left.line };
        }
        return left;
    }

    parseBitwiseXor() {
        let left = this.parseBitwiseAnd();
        while (this.match(TokenType.CARET)) {
            const right = this.parseBitwiseAnd();
            left = { type: NodeType.BinaryExpr, op: '^', left, right, line: left.line };
        }
        return left;
    }

    parseBitwiseAnd() {
        let left = this.parseEquality();
        while (this.match(TokenType.AMPERSAND)) {
            const right = this.parseEquality();
            left = { type: NodeType.BinaryExpr, op: '&', left, right, line: left.line };
        }
        return left;
    }

    parseEquality() {
        let left = this.parseComparison();
        while (this.check(TokenType.EQ, TokenType.NEQ)) {
            const op = this.advance().value;
            const right = this.parseComparison();
            left = { type: NodeType.BinaryExpr, op, left, right, line: left.line };
        }
        return left;
    }

    parseComparison() {
        let left = this.parseShift();
        while (this.check(TokenType.LT, TokenType.GT, TokenType.LTE, TokenType.GTE)) {
            const op = this.advance().value;
            const right = this.parseShift();
            left = { type: NodeType.BinaryExpr, op, left, right, line: left.line };
        }
        return left;
    }

    parseShift() {
        let left = this.parseAddition();
        while (this.check(TokenType.LSHIFT, TokenType.RSHIFT)) {
            const op = this.advance().value;
            const right = this.parseAddition();
            left = { type: NodeType.BinaryExpr, op, left, right, line: left.line };
        }
        return left;
    }

    parseAddition() {
        let left = this.parseMultiplication();
        while (this.check(TokenType.PLUS, TokenType.MINUS)) {
            const op = this.advance().value;
            const right = this.parseMultiplication();
            left = { type: NodeType.BinaryExpr, op, left, right, line: left.line };
        }
        return left;
    }

    parseMultiplication() {
        let left = this.parseUnary();
        while (this.check(TokenType.STAR, TokenType.SLASH, TokenType.PERCENT)) {
            const op = this.advance().value;
            const right = this.parseUnary();
            left = { type: NodeType.BinaryExpr, op, left, right, line: left.line };
        }
        return left;
    }

    parseUnary() {
        if (this.check(TokenType.MINUS, TokenType.NOT, TokenType.TILDE)) {
            const op = this.advance();
            const operand = this.parseUnary();
            return { type: NodeType.UnaryExpr, op: op.value, operand, line: op.line };
        }

        // Pre-increment/decrement
        if (this.check(TokenType.INC, TokenType.DEC)) {
            const op = this.advance();
            const operand = this.parseUnary();
            return { type: NodeType.UnaryExpr, op: 'pre' + op.value, operand, line: op.line };
        }

        // Cast: (int)expr, (float)expr
        if (this.check(TokenType.LPAREN)) {
            const next = this.peek(1);
            if (next && (next.type === TokenType.KW_INT || next.type === TokenType.KW_FLOAT ||
                         next.type === TokenType.KW_CHAR || next.type === TokenType.KW_BYTE ||
                         next.type === TokenType.KW_BOOL)) {
                const afterType = this.peek(2);
                if (afterType && afterType.type === TokenType.RPAREN) {
                    this.advance(); // (
                    const castType = this.parseType();
                    this.advance(); // )
                    const operand = this.parseUnary();
                    return { type: NodeType.CastExpr, castType, operand, line: operand.line };
                }
            }
        }

        return this.parsePostfix();
    }

    parsePostfix() {
        let expr = this.parsePrimary();

        while (true) {
            // Function call
            if (this.check(TokenType.LPAREN) && expr.type === NodeType.Identifier) {
                this.advance();
                const args = [];
                if (!this.check(TokenType.RPAREN)) {
                    do {
                        args.push(this.parseExpression());
                    } while (this.match(TokenType.COMMA));
                }
                this.expect(TokenType.RPAREN);
                expr = { type: NodeType.CallExpr, name: expr.name, args, line: expr.line };
            }
            // Array access — 1D: arr[i]  /  2D: arr[i][j]
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
            }
            // Member access: p.field  or  p.arr[i]  or  arr[i].field
            else if (this.check(TokenType.DOT)) {
                this.advance(); // consume .
                const field = this.expect(TokenType.IDENTIFIER).value;
                // If expr is an ArrayAccess (e.g. gauges[i]), preserve the full
                // node so codegen can compute the per-element struct offset.
                // Otherwise keep the name string (simple `obj.field` case).
                const objectValue = (expr.type === NodeType.ArrayAccess) ? expr : (expr.name || expr);
                if (this.check(TokenType.LBRACKET)) {
                    // Array field subscript: obj.arr[i]  or  arr[i].arr[j]
                    this.advance();
                    const index = this.parseExpression();
                    this.expect(TokenType.RBRACKET);
                    expr = { type: NodeType.MemberArrayAccess, object: objectValue, field, index, line: expr.line };
                } else {
                    expr = { type: NodeType.MemberAccess, object: objectValue, field, line: expr.line };
                }
            }
            // Indirect call after a MemberAccess / MemberArrayAccess:
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
        }

        return expr;
    }

    parsePrimary() {
        const tok = this.current();

        // Integer literal
        if (tok.type === TokenType.INT_LITERAL) {
            this.advance();
            return { type: NodeType.IntLiteral, value: tok.value, line: tok.line };
        }

        // Float literal
        if (tok.type === TokenType.FLOAT_LITERAL) {
            this.advance();
            return { type: NodeType.FloatLiteral, value: tok.value, line: tok.line };
        }

        // Char literal
        if (tok.type === TokenType.CHAR_LITERAL) {
            this.advance();
            return { type: NodeType.CharLiteral, value: tok.value, line: tok.line };
        }

        // String literal
        if (tok.type === TokenType.STRING_LITERAL) {
            this.advance();
            return { type: NodeType.StringLiteral, value: tok.value, line: tok.line };
        }

        // Boolean
        if (tok.type === TokenType.KW_TRUE) {
            this.advance();
            return { type: NodeType.BoolLiteral, value: true, line: tok.line };
        }
        if (tok.type === TokenType.KW_FALSE) {
            this.advance();
            return { type: NodeType.BoolLiteral, value: false, line: tok.line };
        }

        // Identifier
        if (tok.type === TokenType.IDENTIFIER) {
            this.advance();
            return { type: NodeType.Identifier, name: tok.value, line: tok.line };
        }

        // Parenthesized expression
        if (tok.type === TokenType.LPAREN) {
            this.advance();
            const expr = this.parseExpression();
            this.expect(TokenType.RPAREN, "Expected ')'");
            return expr;
        }

        throw new ParseError(`Unexpected token: ${tok.type} ('${tok.value}')`, tok);
    }
}

