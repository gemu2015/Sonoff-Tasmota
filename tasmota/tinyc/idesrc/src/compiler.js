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

// Klarname und Info-URL aus dem Quelltext ziehen. Gleiche Machart wie das
// schon vorhandene `// @defines:` im Praeprozessor -- ein Kommentar am Anfang
// der Datei, keine neue Sprachkonstruktion:
//
//   // @name: SML Chart CT002
//   // @info: https://github.com/.../wiki/sml_chart_ct002
//
// ⚠️ Der ERSTE Treffer gewinnt. `compile()` sieht den Quelltext MIT bereits
// eingesetzten `#include`s, und ein Baustein aus examples/common/ koennte
// selbst eine solche Zeile tragen. In der ueblichen Anordnung steht der
// Programmkopf vor den Includes und gewinnt damit von selbst; wo es darauf
// ankommt (build.html baut index.json), wird der Wert ueber `options.meta`
// aus der UNAUFGELOESTEN Datei mitgegeben und gar nicht erst geraten.
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
