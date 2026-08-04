// Runs BEFORE the lexer as a text-to-text transform.
//
// Supported directives:
//   #include "filename"    — include another .tc file (text paste, before preprocessing)
//   #define NAME [value]   — define a name (value passed through to parser)
//   #undef NAME            — undefine a name
//   #ifdef NAME            — include block if NAME is defined
//   #ifndef NAME           — include block if NAME is NOT defined
//   #if EXPR               — include block if EXPR evaluates to non-zero
//   #else                  — alternative block
//   #endif                 — end conditional block
//
// Include resolution runs first (resolveIncludes / resolveIncludesAsync),
// then preprocess() handles conditional compilation.
// Include guards (#ifndef/#define/#endif) work naturally since preprocessing
// runs after all includes are resolved.
//
// Skipped lines are replaced with empty lines to preserve line numbering.

export class PreprocessorError extends Error {
    constructor(message, line) {
        super(`Preprocessor error at line ${line}: ${message}`);
        this.line = line;
    }
}

// ─── #include resolution (synchronous) ───────────────────────────
// getFile(filename) must return file contents as string, or throw on error.
// Used by CLI (tc_deploy.mjs) with fs.readFileSync.
const MAX_INCLUDE_DEPTH = 8;

export function resolveIncludes(source, getFile) {
    if (!getFile) return source;  // no resolver — skip include processing
    const included = new Set();   // track included files for cycle detection
    return _resolveIncludes(source, getFile, included, 0);
}

function _resolveIncludes(source, getFile, included, depth) {
    if (depth > MAX_INCLUDE_DEPTH) {
        throw new PreprocessorError(`#include nested too deep (max ${MAX_INCLUDE_DEPTH})`, 0);
    }
    const lines = source.split('\n');
    const output = [];
    for (let i = 0; i < lines.length; i++) {
        const match = lines[i].match(/^\s*#\s*include\s+"([^"]+)"\s*$/);
        if (match) {
            const filename = match[1];
            if (included.has(filename)) {
                // Already included — skip (works with include guards too,
                // but this prevents infinite recursion for files without guards)
                output.push('// [already included: ' + filename + ']');
                continue;
            }
            included.add(filename);
            let content;
            try {
                content = getFile(filename);
            } catch (e) {
                throw new PreprocessorError(
                    `#include "${filename}": ${e.message}`, i + 1
                );
            }
            // Recursively resolve nested includes
            const resolved = _resolveIncludes(content, getFile, included, depth + 1);
            output.push('// --- included: ' + filename + ' ---');
            output.push(resolved);
            output.push('// --- end: ' + filename + ' ---');
        } else {
            output.push(lines[i]);
        }
    }
    return output.join('\n');
}

// ─── #include resolution (async) ─────────────────────────────────
// getFile(filename) must return a Promise<string>.
// Used by IDE (browser) with fetch().
export async function resolveIncludesAsync(source, getFile) {
    if (!getFile) return source;
    const included = new Set();
    return _resolveIncludesAsync(source, getFile, included, 0);
}

async function _resolveIncludesAsync(source, getFile, included, depth) {
    if (depth > MAX_INCLUDE_DEPTH) {
        throw new PreprocessorError(`#include nested too deep (max ${MAX_INCLUDE_DEPTH})`, 0);
    }
    const lines = source.split('\n');
    const output = [];
    for (let i = 0; i < lines.length; i++) {
        const match = lines[i].match(/^\s*#\s*include\s+"([^"]+)"\s*$/);
        if (match) {
            const filename = match[1];
            if (included.has(filename)) {
                output.push('// [already included: ' + filename + ']');
                continue;
            }
            included.add(filename);
            let content;
            try {
                content = await getFile(filename);
            } catch (e) {
                throw new PreprocessorError(
                    `#include "${filename}": ${e.message}`, i + 1
                );
            }
            const resolved = await _resolveIncludesAsync(content, getFile, included, depth + 1);
            output.push('// --- included: ' + filename + ' ---');
            output.push(resolved);
            output.push('// --- end: ' + filename + ' ---');
        } else {
            output.push(lines[i]);
        }
    }
    return output.join('\n');
}

export function preprocess(source, predefined = []) {
    const lines = source.split('\n');
    const output = [];
    // Auto-extract defines from @defines pragma: // @defines: -DBOARD_DFROBOT -DFOO
    for (const ln of lines) {
        const m = ln.match(/^\/\/\s*@defines:\s*(.+)/);
        if (m) {
            for (const tok of m[1].trim().split(/\s+/)) {
                if (tok.startsWith('-D')) predefined.push(tok.slice(2));
            }
        }
    }
    // Track defined names AND their replacement text. The values matter for #if:
    // `#define S 0` followed by `#if S` must be FALSE, exactly as in C. Before the
    // values were kept, an identifier in a #if only counted as "is it defined", so a
    // feature switch turned OFF still compiled its block in.
    const defines = new Set();       // defined names (seed with predefined below)
    const defineValues = new Map();  // name -> replacement text ('' = valueless flag)
    for (const p of predefined) {
        const eq = p.indexOf('=');   // accept -DNAME and -DNAME=VALUE
        const name = eq < 0 ? p : p.slice(0, eq);
        defines.add(name);
        defineValues.set(name, eq < 0 ? '' : p.slice(eq + 1));
    }
    const macros = new Map();        // function-like macros: name -> { params: [], body: string }
    const condStack = [];            // stack of { active, parentActive, elseSeen }

    for (let i = 0; i < lines.length; i++) {
        const lineNum = i + 1;
        const raw = lines[i];
        const trimmed = raw.trim();

        // Check if this is a preprocessor directive
        if (trimmed.startsWith('#')) {
            const directive = parseDirective(trimmed);

            if (!directive) {
                // Not a recognized preprocessor directive — could be #define handled by parser
                // Pass through if we're in an active block
                if (isActive(condStack)) {
                    output.push(raw);
                } else {
                    output.push('');  // preserve line number
                }
                continue;
            }

            switch (directive.type) {
                case 'define': {
                    if (isActive(condStack)) {
                        defines.add(directive.name);
                        if (!directive.params) {
                            const mv = raw.trim().match(/^#define\s+\w+(?:\s+([^\n]*))?$/);
                            defineValues.set(directive.name, mv && mv[1] ? mv[1] : '');
                        }
                        if (directive.params) {
                            // Function-like macro: #define NAME(A,B) body
                            macros.set(directive.name, { params: directive.params, body: directive.body });
                            output.push('');  // strip from output — not a value #define
                        } else if (raw.trim().match(/^#define\s+\w+\s+/)) {
                            // Pass value #define through so parser can handle #define NAME value
                            output.push(raw);
                        } else {
                            // Valueless #define (flag only) — already added to defines Set, strip from output
                            output.push('');
                        }
                    } else {
                        output.push('');
                    }
                    break;
                }

                case 'undef': {
                    if (isActive(condStack)) {
                        defines.delete(directive.name);
                        defineValues.delete(directive.name);
                    }
                    output.push('');  // strip #undef from output
                    break;
                }

                case 'ifdef': {
                    const parentActive = isActive(condStack);
                    const active = parentActive && defines.has(directive.name);
                    condStack.push({ active, parentActive, elseSeen: false });
                    output.push('');
                    break;
                }

                case 'ifndef': {
                    const parentActive = isActive(condStack);
                    const active = parentActive && !defines.has(directive.name);
                    condStack.push({ active, parentActive, elseSeen: false });
                    output.push('');
                    break;
                }

                case 'if': {
                    const parentActive = isActive(condStack);
                    let active = false;
                    if (parentActive) {
                        active = evalCondition(directive.expr, defines, lineNum, defineValues);
                    }
                    condStack.push({ active, parentActive, elseSeen: false });
                    output.push('');
                    break;
                }

                case 'else': {
                    if (condStack.length === 0) {
                        throw new PreprocessorError('#else without matching #ifdef/#ifndef/#if', lineNum);
                    }
                    const top = condStack[condStack.length - 1];
                    if (top.elseSeen) {
                        throw new PreprocessorError('duplicate #else', lineNum);
                    }
                    top.elseSeen = true;
                    // Flip: active only if parent is active AND previous branch was NOT active
                    top.active = top.parentActive && !top.active;
                    output.push('');
                    break;
                }

                case 'endif': {
                    if (condStack.length === 0) {
                        throw new PreprocessorError('#endif without matching #ifdef/#ifndef/#if', lineNum);
                    }
                    condStack.pop();
                    output.push('');
                    break;
                }

                case 'include': {
                    // Should not reach here — #include is handled in a
                    // separate textual-inlining pass before preprocess()
                    // (tcResolveIncludes), so that #ifdef/#define inside
                    // included files behave the same as if the content
                    // were typed inline. If we DO see one here it means
                    // the IDE skipped the inlining pass — emit a clear
                    // error rather than silently dropping the directive.
                    if (isActive(condStack)) {
                        throw new PreprocessorError(
                            `#include "${directive.filename}" reached preprocess() — ` +
                            `tcResolveIncludes() must run first. Make sure compileCode() ` +
                            `awaits the inlining pass.`, lineNum);
                    } else {
                        output.push('');
                    }
                    break;
                }

                default:
                    output.push(raw);
                    break;
            }
        } else {
            // Regular code line
            if (isActive(condStack)) {
                output.push(macros.size > 0 ? expandMacros(raw, macros) : raw);
            } else {
                output.push('');  // preserve line number
            }
        }
    }

    if (condStack.length > 0) {
        throw new PreprocessorError('unterminated #ifdef/#ifndef/#if (missing #endif)', lines.length);
    }

    return output.join('\n');
}

// Check if all conditions in the stack are active
function isActive(condStack) {
    if (condStack.length === 0) return true;
    return condStack[condStack.length - 1].active;
}

// Parse a directive line, returns { type, name?, expr? } or null if not a conditional directive
function parseDirective(trimmed) {
    // Match: # [spaces] directive [spaces] rest
    const m = trimmed.match(/^#\s*(\w+)(?:\s+(.*))?$/);
    if (!m) return null;

    const dir = m[1];
    const rest = (m[2] || '').trim();

    switch (dir) {
        case 'define':
            if (!rest) return null;  // #define with no name — let parser handle error
            // Check for function-like macro: NAME(A,B,...) [body]
            const macroMatch = rest.match(/^([A-Za-z_]\w*)\(([^)]*)\)\s*(.*)$/);
            if (macroMatch && rest[macroMatch[1].length] === '(') {
                const params = macroMatch[2].split(',').map(p => p.trim()).filter(p => p);
                return { type: 'define', name: macroMatch[1], params, body: macroMatch[3] };
            }
            const defName = rest.match(/^([A-Za-z_]\w*)/);
            if (!defName) return null;
            return { type: 'define', name: defName[1] };

        case 'undef':
            if (!rest) return null;
            return { type: 'undef', name: rest.match(/^([A-Za-z_]\w*)/)?.[1] || rest };

        case 'ifdef':
            if (!rest) return null;
            return { type: 'ifdef', name: rest.match(/^([A-Za-z_]\w*)/)?.[1] || rest };

        case 'ifndef':
            if (!rest) return null;
            return { type: 'ifndef', name: rest.match(/^([A-Za-z_]\w*)/)?.[1] || rest };

        case 'if':
            return { type: 'if', expr: rest };

        case 'else':
            return { type: 'else' };

        case 'endif':
            return { type: 'endif' };

        case 'include': {
            // #include "filename.tc" — content inlined by preprocess() from
            // the includes map (pre-resolved by tcResolveIncludes() before
            // compile()). Only "..." form supported (local files); <...> form
            // (system headers) intentionally omitted — TinyC has no system
            // headers. Filename is plain ASCII; quotes are stripped.
            if (!rest) return null;
            const m = rest.match(/^"([^"]+)"/);
            if (!m) return null;
            return { type: 'include', filename: m[1] };
        }

        default:
            return null;  // unknown directive — pass through to lexer/parser
    }
}

// Evaluate a #if condition expression
// Supports: integer literals, defined(NAME), NAME (1 if defined, 0 if not),
//           &&, ||, !, ==, !=, >, <, >=, <=, (, )
function evalCondition(expr, defines, lineNum, defineValues = new Map(), depth = 0) {
    // Tokenize the expression
    const tokens = tokenizeExpr(expr, lineNum);
    if (tokens.length === 0) {
        throw new PreprocessorError('#if with empty expression', lineNum);
    }

    let pos = 0;

    function peek() { return pos < tokens.length ? tokens[pos] : null; }
    function advance() { return tokens[pos++]; }

    // Recursive descent: or → and → equality → comparison → unary → primary
    function parseOr() {
        let left = parseAnd();
        while (peek()?.value === '||') {
            advance();
            const right = parseAnd();
            left = (left || right) ? 1 : 0;
        }
        return left;
    }

    function parseAnd() {
        let left = parseEquality();
        while (peek()?.value === '&&') {
            advance();
            const right = parseEquality();
            left = (left && right) ? 1 : 0;
        }
        return left;
    }

    function parseEquality() {
        let left = parseComparison();
        while (peek()?.value === '==' || peek()?.value === '!=') {
            const op = advance().value;
            const right = parseComparison();
            left = op === '==' ? (left === right ? 1 : 0) : (left !== right ? 1 : 0);
        }
        return left;
    }

    function parseComparison() {
        let left = parseUnary();
        while (peek()?.value === '>' || peek()?.value === '<' ||
               peek()?.value === '>=' || peek()?.value === '<=') {
            const op = advance().value;
            const right = parseUnary();
            switch (op) {
                case '>':  left = left > right ? 1 : 0; break;
                case '<':  left = left < right ? 1 : 0; break;
                case '>=': left = left >= right ? 1 : 0; break;
                case '<=': left = left <= right ? 1 : 0; break;
            }
        }
        return left;
    }

    function parseUnary() {
        if (peek()?.value === '!') {
            advance();
            return parseUnary() ? 0 : 1;
        }
        return parsePrimary();
    }

    function parsePrimary() {
        const tok = peek();
        if (!tok) throw new PreprocessorError('unexpected end of #if expression', lineNum);

        // Parenthesized expression
        if (tok.value === '(') {
            advance();
            const val = parseOr();
            if (peek()?.value !== ')') {
                throw new PreprocessorError('missing ) in #if expression', lineNum);
            }
            advance();
            return val;
        }

        // defined(NAME) or defined NAME
        if (tok.type === 'ident' && tok.value === 'defined') {
            advance();
            let name;
            if (peek()?.value === '(') {
                advance();
                const nt = advance();
                if (!nt || nt.type !== 'ident') {
                    throw new PreprocessorError('expected identifier in defined()', lineNum);
                }
                name = nt.value;
                if (peek()?.value !== ')') {
                    throw new PreprocessorError('missing ) in defined()', lineNum);
                }
                advance();
            } else {
                const nt = advance();
                if (!nt || nt.type !== 'ident') {
                    throw new PreprocessorError('expected identifier after defined', lineNum);
                }
                name = nt.value;
            }
            return defines.has(name) ? 1 : 0;
        }

        // Integer literal
        if (tok.type === 'number') {
            advance();
            return tok.numValue;
        }

        // Identifier — expands to its replacement text, like C. An undefined name is 0;
        // a valueless flag (`#define FOO`) is 1; `#define S 0` is 0, NOT 1.
        if (tok.type === 'ident') {
            advance();
            if (!defines.has(tok.value)) { return 0; }
            let body = (defineValues.get(tok.value) ?? '').replace(/\/\*.*?\*\//g, ' ')
                                                          .replace(/\/\/.*$/, '').trim();
            if (body === '') { return 1; }
            const num = body.match(/^[+-]?(0[xX][0-9a-fA-F]+|\d+)$/);
            if (num) { return Number(body); }
            // Non-numeric body (another macro, an expression) — evaluate it, with a
            // depth cap so `#define A A` cannot spin forever.
            if (depth >= 8) { return 1; }
            try {
                return evalCondition(body, defines, lineNum, defineValues, depth + 1) ? 1 : 0;
            } catch (e) {
                return 1;   // unparsable body: fall back to the old "defined = true"
            }
        }

        throw new PreprocessorError(`unexpected token '${tok.value}' in #if expression`, lineNum);
    }

    const result = parseOr();
    return result !== 0;
}

// Simple tokenizer for #if expressions
function tokenizeExpr(expr, lineNum) {
    const tokens = [];
    let i = 0;

    while (i < expr.length) {
        const ch = expr[i];

        // Skip whitespace
        if (ch === ' ' || ch === '\t') { i++; continue; }

        // Skip line comments
        if (ch === '/' && expr[i + 1] === '/') break;

        // Two-character operators
        if (i + 1 < expr.length) {
            const two = expr.slice(i, i + 2);
            if (two === '&&' || two === '||' || two === '==' || two === '!=' ||
                two === '>=' || two === '<=') {
                tokens.push({ type: 'op', value: two });
                i += 2;
                continue;
            }
        }

        // Single-character operators
        if ('!><()'.includes(ch)) {
            tokens.push({ type: 'op', value: ch });
            i++;
            continue;
        }

        // Number
        if (ch >= '0' && ch <= '9') {
            let num = '';
            while (i < expr.length && ((expr[i] >= '0' && expr[i] <= '9') ||
                   expr[i] === 'x' || expr[i] === 'X' ||
                   (expr[i] >= 'a' && expr[i] <= 'f') ||
                   (expr[i] >= 'A' && expr[i] <= 'F'))) {
                num += expr[i++];
            }
            tokens.push({ type: 'number', value: num, numValue: parseInt(num) });
            continue;
        }

        // Identifier
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch === '_') {
            let ident = '';
            while (i < expr.length && ((expr[i] >= 'a' && expr[i] <= 'z') ||
                   (expr[i] >= 'A' && expr[i] <= 'Z') ||
                   (expr[i] >= '0' && expr[i] <= '9') || expr[i] === '_')) {
                ident += expr[i++];
            }
            tokens.push({ type: 'ident', value: ident });
            continue;
        }

        throw new PreprocessorError(`unexpected character '${ch}' in #if expression`, lineNum);
    }

    return tokens;
}

// Expand function-like macros in a line of code
// Handles nested parentheses in arguments: LOG(foo(1,2)) works correctly
function expandMacros(line, macros) {
    let result = line;
    let changed = true;
    let iterations = 0;
    // Iterate until no more expansions (handles nested macros)
    while (changed && iterations < 10) {
        changed = false;
        iterations++;
        for (const [name, macro] of macros) {
            // Match macro name followed by (
            const re = new RegExp('\\b' + name + '\\s*\\(', 'g');
            let m;
            while ((m = re.exec(result)) !== null) {
                // Parse arguments with parenthesis depth tracking
                const argsStart = m.index + m[0].length;
                const args = [];
                let depth = 1;
                let argStart = argsStart;
                let i = argsStart;
                while (i < result.length && depth > 0) {
                    const ch = result[i];
                    if (ch === '(') depth++;
                    else if (ch === ')') {
                        depth--;
                        if (depth === 0) {
                            args.push(result.substring(argStart, i).trim());
                            break;
                        }
                    } else if (ch === ',' && depth === 1) {
                        args.push(result.substring(argStart, i).trim());
                        argStart = i + 1;
                    } else if (ch === '"') {
                        // Skip string literals
                        i++;
                        while (i < result.length && result[i] !== '"') {
                            if (result[i] === '\\') i++;
                            i++;
                        }
                    }
                    i++;
                }
                if (depth !== 0) break;  // unmatched parens — skip
                if (args.length !== macro.params.length) break;  // wrong arg count
                // i points at closing ')' — skip past it
                i++;

                // Substitute parameters in body
                let expanded = macro.body;
                for (let p = 0; p < macro.params.length; p++) {
                    // Replace whole-word only
                    expanded = expanded.replace(
                        new RegExp('\\b' + macro.params[p] + '\\b', 'g'),
                        args[p]
                    );
                }
                // Replace the macro invocation with expanded text
                // For empty body macros, also consume trailing semicolon
                if (expanded === '' && i < result.length && result[i] === ';') i++;
                result = result.substring(0, m.index) + expanded + result.substring(i);
                changed = true;
                break;  // restart scanning from beginning
            }
            if (changed) break;
        }
    }
    return result;
}

