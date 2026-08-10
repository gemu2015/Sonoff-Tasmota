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

export function compile(source, options = {}) {
    const predefined = options.defines || [];
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
        compiled = codegen.compile(ast);
    } catch (e) {
        if (e instanceof CodeGenError) throw new CompilerError('CodeGen', e);
        throw e;
    }

    return {
        ...compiled,
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
