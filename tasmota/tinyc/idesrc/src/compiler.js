import { preprocess, PreprocessorError } from './preprocessor.js';
import { Lexer, LexerError } from './lexer.js';
import { Parser, ParseError } from './parser.js';
import { CodeGenerator, CodeGenError } from './codegen.js';
import { VM, VMError } from './vm.js';

// Re-export everything for convenient single-file import
export { preprocess, PreprocessorError } from './preprocessor.js';
export { Lexer, LexerError } from './lexer.js';
export { Parser, ParseError, NodeType } from './parser.js';
export { CodeGenerator, CodeGenError } from './codegen.js';
export { VM, VMError } from './vm.js';
export { Op, OpName, Syscall, SyscallName, SYSCALL_ABI, VERSION } from './opcodes.js';

// Combines Preprocessor + Lexer + Parser + CodeGen into one convenient interface












export class CompilerError extends Error {
    constructor(phase, originalError) {
        super(`[${phase}] ${originalError.message}`);
        this.phase = phase;
        this.originalError = originalError;
        this.line = originalError.line || originalError.token?.line || 0;
        this.col = originalError.col || originalError.token?.col || 0;
    }
}

// Pull the plain name and info URL out of the source. Same shape as the
// existing `// @defines:` in the preprocessor -- a comment at the top of the
// file, not a new language construct:
//
//   // @name: SML Chart CT002
//   // @info: https://github.com/.../wiki/sml_chart_ct002
//
// ⚠️ The FIRST match wins. compile() sees the source with #includes already
// substituted, and a block from examples/common/ could carry such a line
// itself. In the usual layout the program header precedes the includes and
// therefore wins on its own; where it actually matters (build.html writing
// index.json) the value is handed in via options.meta from the UNRESOLVED
// file, so nothing has to be guessed.
export function pragmaMeta(source) {
    const hol = (schluessel) => {
        const m = String(source || '').match(
            new RegExp('^[ \t]*//[ \t]*@' + schluessel + ':[ \t]*(.+)$', 'm'));
        return m ? m[1].trim() : '';
    };
    return { name: hol('name'), info: hol('info') };
}

export function compile(source, options = {}) {
    const predefined = options.defines || [];
    const meta = options.meta || pragmaMeta(source);
    // Phase 0: Preprocess (#ifdef, #ifndef, #if, #else, #endif)
    let preprocessed;
    try {
        preprocessed = preprocess(source, predefined);
    } catch (e) {
        if (e instanceof PreprocessorError) throw new CompilerError('Preprocessor', e);
        throw e;
    }

    // Phase 1: Tokenize
    let tokens;
    try {
        const lexer = new Lexer(preprocessed);
        tokens = lexer.tokenize();
    } catch (e) {
        if (e instanceof LexerError) throw new CompilerError('Lexer', e);
        throw e;
    }

    // Phase 2: Parse
    let ast;
    try {
        const parser = new Parser(tokens);
        ast = parser.parse();
    } catch (e) {
        if (e instanceof ParseError) throw new CompilerError('Parser', e);
        throw e;
    }

    // Phase 3: Generate bytecode
    let compiled;
    try {
        // options.targetAbi = fuer WELCHE Firmware wird uebersetzt. Die IDE reicht hier
        // die ABI des Geraets durch, das im Feld "Device IP" steht — also genau des
        // Geraets, auf dem die .tcb landet. Ohne Angabe: volles SYSCALL_ABI wie bisher.
        const codegen = new CodeGenerator(options.targetAbi);
        codegen._metaName = meta.name || '';
        codegen._metaInfo = meta.info || '';
        compiled = codegen.compile(ast);
    } catch (e) {
        if (e instanceof CodeGenError) throw new CompilerError('CodeGen', e);
        throw e;
    }

    return {
        ...compiled,
        meta,
        tokens,
        ast,
    };
}

export function compileAndRun(source, options = {}) {
    const compiled = compile(source);
    const vm = new VM(options);
    vm.load(compiled);
    vm.run();
    return {
        compiled,
        vm,
        state: vm.getState(),
    };
}
