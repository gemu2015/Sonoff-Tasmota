// Supports: int, float, char, bool, void, arrays, if/else, while, for, functions

export const TokenType = {
    // Literals
    INT_LITERAL:    'INT_LITERAL',
    FLOAT_LITERAL:  'FLOAT_LITERAL',
    CHAR_LITERAL:   'CHAR_LITERAL',
    STRING_LITERAL: 'STRING_LITERAL',

    // Identifiers & Keywords
    IDENTIFIER:     'IDENTIFIER',

    // Type keywords
    KW_INT:         'KW_INT',
    KW_FLOAT:       'KW_FLOAT',
    KW_CHAR:        'KW_CHAR',
    KW_BYTE:        'KW_BYTE',
    KW_BOOL:        'KW_BOOL',
    KW_VOID:        'KW_VOID',

    // Control flow
    KW_IF:          'KW_IF',
    KW_ELSE:        'KW_ELSE',
    KW_WHILE:       'KW_WHILE',
    KW_FOR:         'KW_FOR',
    KW_RETURN:      'KW_RETURN',
    KW_BREAK:       'KW_BREAK',
    KW_CONTINUE:    'KW_CONTINUE',
    KW_SWITCH:      'KW_SWITCH',
    KW_CASE:        'KW_CASE',
    KW_DEFAULT:     'KW_DEFAULT',
    KW_TRUE:        'KW_TRUE',
    KW_FALSE:       'KW_FALSE',
    KW_DEFINE:      'KW_DEFINE',
    KW_PERSIST:     'KW_PERSIST',
    KW_WATCH:       'KW_WATCH',
    KW_GLOBAL:      'KW_GLOBAL',
    KW_DO:          'KW_DO',
    KW_CONST:       'KW_CONST',
    KW_STATIC:      'KW_STATIC',
    KW_ENUM:        'KW_ENUM',
    KW_STRUCT:      'KW_STRUCT',
    KW_TYPEDEF:     'KW_TYPEDEF',
    DOT:            'DOT',          // .

    // Operators
    PLUS:           'PLUS',         // +
    MINUS:          'MINUS',        // -
    STAR:           'STAR',         // *
    SLASH:          'SLASH',        // /
    PERCENT:        'PERCENT',      // %
    AMPERSAND:      'AMPERSAND',    // &
    PIPE:           'PIPE',         // |
    CARET:          'CARET',        // ^
    TILDE:          'TILDE',        // ~
    LSHIFT:         'LSHIFT',       // <<
    RSHIFT:         'RSHIFT',       // >>
    AND:            'AND',          // &&
    OR:             'OR',           // ||
    NOT:            'NOT',          // !

    // Comparison
    EQ:             'EQ',           // ==
    NEQ:            'NEQ',          // !=
    LT:             'LT',          // <
    GT:             'GT',          // >
    LTE:            'LTE',         // <=
    GTE:            'GTE',         // >=

    // Assignment
    ASSIGN:         'ASSIGN',       // =
    PLUS_ASSIGN:    'PLUS_ASSIGN',  // +=
    MINUS_ASSIGN:   'MINUS_ASSIGN', // -=
    STAR_ASSIGN:    'STAR_ASSIGN',  // *=
    SLASH_ASSIGN:   'SLASH_ASSIGN', // /=
    PERCENT_ASSIGN: 'PERCENT_ASSIGN', // %=
    AND_ASSIGN:     'AND_ASSIGN',   // &=
    OR_ASSIGN:      'OR_ASSIGN',    // |=
    XOR_ASSIGN:     'XOR_ASSIGN',   // ^=
    LSHIFT_ASSIGN:  'LSHIFT_ASSIGN',// <<=
    RSHIFT_ASSIGN:  'RSHIFT_ASSIGN',// >>=

    // Increment/Decrement
    INC:            'INC',          // ++
    DEC:            'DEC',          // --

    // Delimiters
    LPAREN:         'LPAREN',       // (
    RPAREN:         'RPAREN',       // )
    LBRACE:         'LBRACE',       // {
    RBRACE:         'RBRACE',       // }
    LBRACKET:       'LBRACKET',     // [
    RBRACKET:       'RBRACKET',     // ]
    SEMICOLON:      'SEMICOLON',    // ;
    COMMA:          'COMMA',        // ,
    COLON:          'COLON',        // :
    QUESTION:       'QUESTION',     // ?
    HASH:           'HASH',         // #

    // Special
    EOF:            'EOF',
};

const KEYWORDS = {
    'int':        TokenType.KW_INT,
    'int32_t':    TokenType.KW_INT,
    'uint32_t':   TokenType.KW_INT,
    'unsigned':   TokenType.KW_INT,
    'float':      TokenType.KW_FLOAT,
    'char':       TokenType.KW_CHAR,
    // uint8_t is NOT a keyword here: it's registered as a context-sensitive
    // alias for `byte` in the parser (typeAliases), so `uint8_t buf[N]` packs
    // 1 byte/element like byte[], and the name stays usable as an identifier.
    'bool':       TokenType.KW_BOOL,
    'void':       TokenType.KW_VOID,
    'if':       TokenType.KW_IF,
    'else':     TokenType.KW_ELSE,
    'while':    TokenType.KW_WHILE,
    'for':      TokenType.KW_FOR,
    'return':   TokenType.KW_RETURN,
    'break':    TokenType.KW_BREAK,
    'continue': TokenType.KW_CONTINUE,
    'switch':   TokenType.KW_SWITCH,
    'case':     TokenType.KW_CASE,
    'default':  TokenType.KW_DEFAULT,
    'true':     TokenType.KW_TRUE,
    'false':    TokenType.KW_FALSE,
    'persist':  TokenType.KW_PERSIST,
    'watch':    TokenType.KW_WATCH,
    'global':   TokenType.KW_GLOBAL,
    'define':   TokenType.KW_DEFINE,
    'do':       TokenType.KW_DO,
    'const':    TokenType.KW_CONST,
    'static':   TokenType.KW_STATIC,
    'enum':     TokenType.KW_ENUM,
    'struct':   TokenType.KW_STRUCT,
    'typedef':  TokenType.KW_TYPEDEF,
};

export class Token {
    constructor(type, value, line, col) {
        this.type = type;
        this.value = value;
        this.line = line;
        this.col = col;
    }
    toString() {
        return `Token(${this.type}, ${JSON.stringify(this.value)}, ${this.line}:${this.col})`;
    }
}

export class LexerError extends Error {
    constructor(message, line, col) {
        super(`Lexer error at ${line}:${col}: ${message}`);
        this.line = line;
        this.col = col;
    }
}

export class Lexer {
    constructor(source) {
        this.source = source;
        this.pos = 0;
        this.line = 1;
        this.col = 1;
        this.tokens = [];
    }

    peek() {
        return this.pos < this.source.length ? this.source[this.pos] : '\0';
    }

    peekAt(offset) {
        const idx = this.pos + offset;
        return idx < this.source.length ? this.source[idx] : '\0';
    }

    advance() {
        const ch = this.source[this.pos];
        this.pos++;
        if (ch === '\n') {
            this.line++;
            this.col = 1;
        } else {
            this.col++;
        }
        return ch;
    }

    match(expected) {
        if (this.peek() === expected) {
            this.advance();
            return true;
        }
        return false;
    }

    addToken(type, value) {
        this.tokens.push(new Token(type, value, this.line, this.col));
    }

    skipWhitespace() {
        while (this.pos < this.source.length) {
            const ch = this.peek();
            if (ch === ' ' || ch === '\t' || ch === '\r' || ch === '\n') {
                this.advance();
            } else if (ch === '/' && this.peekAt(1) === '/') {
                // Single-line comment
                while (this.pos < this.source.length && this.peek() !== '\n') {
                    this.advance();
                }
            } else if (ch === '/' && this.peekAt(1) === '*') {
                // Multi-line comment
                this.advance(); // skip /
                this.advance(); // skip *
                while (this.pos < this.source.length) {
                    if (this.peek() === '*' && this.peekAt(1) === '/') {
                        this.advance(); // skip *
                        this.advance(); // skip /
                        break;
                    }
                    this.advance();
                }
            } else {
                break;
            }
        }
    }

    readNumber() {
        const startLine = this.line;
        const startCol = this.col;
        let num = '';
        let isFloat = false;

        // Hex literal
        if (this.peek() === '0' && (this.peekAt(1) === 'x' || this.peekAt(1) === 'X')) {
            num += this.advance(); // 0
            num += this.advance(); // x
            while (this.pos < this.source.length && /[0-9a-fA-F]/.test(this.peek())) {
                num += this.advance();
            }
            this.tokens.push(new Token(TokenType.INT_LITERAL, parseInt(num, 16), startLine, startCol));
            return;
        }

        // Binary literal
        if (this.peek() === '0' && (this.peekAt(1) === 'b' || this.peekAt(1) === 'B')) {
            num += this.advance(); // 0
            num += this.advance(); // b
            while (this.pos < this.source.length && (this.peek() === '0' || this.peek() === '1')) {
                num += this.advance();
            }
            this.tokens.push(new Token(TokenType.INT_LITERAL, parseInt(num.slice(2), 2), startLine, startCol));
            return;
        }

        while (this.pos < this.source.length && /[0-9]/.test(this.peek())) {
            num += this.advance();
        }

        if (this.peek() === '.' && /[0-9]/.test(this.peekAt(1))) {
            isFloat = true;
            num += this.advance(); // .
            while (this.pos < this.source.length && /[0-9]/.test(this.peek())) {
                num += this.advance();
            }
        }

        // Float suffix
        if (this.peek() === 'f' || this.peek() === 'F') {
            isFloat = true;
            this.advance();
        }

        if (isFloat) {
            this.tokens.push(new Token(TokenType.FLOAT_LITERAL, parseFloat(num), startLine, startCol));
        } else {
            this.tokens.push(new Token(TokenType.INT_LITERAL, parseInt(num, 10), startLine, startCol));
        }
    }

    readString() {
        const startLine = this.line;
        const startCol = this.col;
        this.advance(); // skip opening "
        let str = '';
        while (this.pos < this.source.length && this.peek() !== '"') {
            if (this.peek() === '\\') {
                this.advance();
                const esc = this.advance();
                switch (esc) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case 'r': str += '\r'; break;
                    case '\\': str += '\\'; break;
                    case '"': str += '"'; break;
                    case '0': str += '\0'; break;
                    case 'x': {
                        // \xHH hex escape
                        let hex = '';
                        while (hex.length < 2 && /[0-9a-fA-F]/.test(this.peek())) hex += this.advance();
                        str += String.fromCharCode(parseInt(hex, 16));
                        break;
                    }
                    default: str += esc;
                }
            } else {
                str += this.advance();
            }
        }
        if (this.peek() !== '"') {
            throw new LexerError('Unterminated string literal', startLine, startCol);
        }
        this.advance(); // skip closing "
        this.tokens.push(new Token(TokenType.STRING_LITERAL, str, startLine, startCol));
    }

    readChar() {
        const startLine = this.line;
        const startCol = this.col;
        this.advance(); // skip opening '
        let ch;
        if (this.peek() === '\\') {
            this.advance();
            const esc = this.advance();
            switch (esc) {
                case 'n': ch = '\n'; break;
                case 't': ch = '\t'; break;
                case 'r': ch = '\r'; break;
                case '\\': ch = '\\'; break;
                case '\'': ch = '\''; break;
                case '0': ch = '\0'; break;
                case 'x': {
                    let hex = '';
                    while (hex.length < 2 && /[0-9a-fA-F]/.test(this.peek())) hex += this.advance();
                    ch = String.fromCharCode(parseInt(hex, 16));
                    break;
                }
                default: ch = esc;
            }
        } else {
            ch = this.advance();
        }
        if (this.peek() !== '\'') {
            throw new LexerError('Unterminated char literal', startLine, startCol);
        }
        this.advance(); // skip closing '
        this.tokens.push(new Token(TokenType.CHAR_LITERAL, ch.charCodeAt(0), startLine, startCol));
    }

    readIdentifier() {
        const startLine = this.line;
        const startCol = this.col;
        let name = '';
        while (this.pos < this.source.length && /[a-zA-Z0-9_]/.test(this.peek())) {
            name += this.advance();
        }
        // Handle "unsigned int" as two-word type — skip the optional "int" after "unsigned"
        if (name === 'unsigned') {
            const savedPos = this.pos;
            const savedLine = this.line;
            const savedCol = this.col;
            // Skip whitespace to peek at next word
            let tmpPos = this.pos;
            while (tmpPos < this.source.length && (this.source[tmpPos] === ' ' || this.source[tmpPos] === '\t')) {
                tmpPos++;
            }
            // Check if next word is "int"
            if (this.source.slice(tmpPos, tmpPos + 3) === 'int' &&
                (tmpPos + 3 >= this.source.length || !/[a-zA-Z0-9_]/.test(this.source[tmpPos + 3]))) {
                // Consume whitespace + "int"
                while (this.pos < tmpPos) this.advance();
                for (let ci = 0; ci < 3; ci++) this.advance();
            }
        }
        const kwType = KEYWORDS[name];
        if (kwType) {
            this.tokens.push(new Token(kwType, name, startLine, startCol));
        } else {
            this.tokens.push(new Token(TokenType.IDENTIFIER, name, startLine, startCol));
        }
    }

    tokenize() {
        while (this.pos < this.source.length) {
            this.skipWhitespace();
            if (this.pos >= this.source.length) break;

            const startLine = this.line;
            const startCol = this.col;
            const ch = this.peek();

            // Numbers
            if (/[0-9]/.test(ch)) {
                this.readNumber();
                continue;
            }

            // Strings
            if (ch === '"') {
                this.readString();
                continue;
            }

            // Chars
            if (ch === '\'') {
                this.readChar();
                continue;
            }

            // Identifiers / Keywords
            if (/[a-zA-Z_]/.test(ch)) {
                this.readIdentifier();
                continue;
            }

            // Operators and delimiters
            this.advance();
            switch (ch) {
                case '+':
                    if (this.match('+')) this.addToken(TokenType.INC, '++');
                    else if (this.match('=')) this.addToken(TokenType.PLUS_ASSIGN, '+=');
                    else this.tokens.push(new Token(TokenType.PLUS, '+', startLine, startCol));
                    break;
                case '-':
                    if (this.match('-')) this.addToken(TokenType.DEC, '--');
                    else if (this.match('=')) this.addToken(TokenType.MINUS_ASSIGN, '-=');
                    else this.tokens.push(new Token(TokenType.MINUS, '-', startLine, startCol));
                    break;
                case '*':
                    if (this.match('=')) this.addToken(TokenType.STAR_ASSIGN, '*=');
                    else this.tokens.push(new Token(TokenType.STAR, '*', startLine, startCol));
                    break;
                case '/':
                    if (this.match('=')) this.addToken(TokenType.SLASH_ASSIGN, '/=');
                    else this.tokens.push(new Token(TokenType.SLASH, '/', startLine, startCol));
                    break;
                case '%':
                    if (this.match('=')) this.addToken(TokenType.PERCENT_ASSIGN, '%=');
                    else this.tokens.push(new Token(TokenType.PERCENT, '%', startLine, startCol));
                    break;
                case '&':
                    if (this.match('&')) this.addToken(TokenType.AND, '&&');
                    else if (this.match('=')) this.addToken(TokenType.AND_ASSIGN, '&=');
                    else this.tokens.push(new Token(TokenType.AMPERSAND, '&', startLine, startCol));
                    break;
                case '|':
                    if (this.match('|')) this.addToken(TokenType.OR, '||');
                    else if (this.match('=')) this.addToken(TokenType.OR_ASSIGN, '|=');
                    else this.tokens.push(new Token(TokenType.PIPE, '|', startLine, startCol));
                    break;
                case '^':
                    if (this.match('=')) this.addToken(TokenType.XOR_ASSIGN, '^=');
                    else this.tokens.push(new Token(TokenType.CARET, '^', startLine, startCol));
                    break;
                case '~':
                    this.tokens.push(new Token(TokenType.TILDE, '~', startLine, startCol));
                    break;
                case '!':
                    if (this.match('=')) this.addToken(TokenType.NEQ, '!=');
                    else this.tokens.push(new Token(TokenType.NOT, '!', startLine, startCol));
                    break;
                case '=':
                    if (this.match('=')) this.addToken(TokenType.EQ, '==');
                    else this.tokens.push(new Token(TokenType.ASSIGN, '=', startLine, startCol));
                    break;
                case '<':
                    if (this.match('<')) {
                        if (this.match('=')) this.addToken(TokenType.LSHIFT_ASSIGN, '<<=');
                        else this.addToken(TokenType.LSHIFT, '<<');
                    } else if (this.match('=')) this.addToken(TokenType.LTE, '<=');
                    else this.tokens.push(new Token(TokenType.LT, '<', startLine, startCol));
                    break;
                case '>':
                    if (this.match('>')) {
                        if (this.match('=')) this.addToken(TokenType.RSHIFT_ASSIGN, '>>=');
                        else this.addToken(TokenType.RSHIFT, '>>');
                    } else if (this.match('=')) this.addToken(TokenType.GTE, '>=');
                    else this.tokens.push(new Token(TokenType.GT, '>', startLine, startCol));
                    break;
                case '(':
                    this.tokens.push(new Token(TokenType.LPAREN, '(', startLine, startCol));
                    break;
                case ')':
                    this.tokens.push(new Token(TokenType.RPAREN, ')', startLine, startCol));
                    break;
                case '{':
                    this.tokens.push(new Token(TokenType.LBRACE, '{', startLine, startCol));
                    break;
                case '}':
                    this.tokens.push(new Token(TokenType.RBRACE, '}', startLine, startCol));
                    break;
                case '[':
                    this.tokens.push(new Token(TokenType.LBRACKET, '[', startLine, startCol));
                    break;
                case ']':
                    this.tokens.push(new Token(TokenType.RBRACKET, ']', startLine, startCol));
                    break;
                case ';':
                    this.tokens.push(new Token(TokenType.SEMICOLON, ';', startLine, startCol));
                    break;
                case ',':
                    this.tokens.push(new Token(TokenType.COMMA, ',', startLine, startCol));
                    break;
                case ':':
                    this.tokens.push(new Token(TokenType.COLON, ':', startLine, startCol));
                    break;
                case '.':
                    this.tokens.push(new Token(TokenType.DOT, '.', startLine, startCol));
                    break;
                case '?':
                    this.tokens.push(new Token(TokenType.QUESTION, '?', startLine, startCol));
                    break;
                case '#':
                    this.tokens.push(new Token(TokenType.HASH, '#', startLine, startCol));
                    break;
                default:
                    throw new LexerError(`Unexpected character: '${ch}'`, startLine, startCol);
            }
        }

        this.tokens.push(new Token(TokenType.EOF, null, this.line, this.col));
        return this.tokens;
    }
}

