// Debug script: Compile bresser_chart.tc and dump bytecode around PC 4713
// Usage: node debug_pc.mjs

import { readFileSync } from 'fs';
import { compile } from './src/compiler.js';
import { Op, OpName, SyscallName } from './src/opcodes.js';

const source = readFileSync('./examples/bresser_chart.tc', 'utf-8');
const compiled = compile(source);

const { binary, codeOffset, codeSize, functions, constants, sourceMap } = compiled;

// ─── List all function entry points ──────────────────────────
console.log('=== FUNCTION ENTRY POINTS ===');
const funcList = Object.entries(functions).sort((a, b) => a[1].address - b[1].address);
for (const [name, info] of funcList) {
    console.log(`  ${String(info.address).padStart(5)}  ${name}() -> ${info.returnType}`);
}
console.log(`\nCode size: ${codeSize}, Code offset in binary: ${codeOffset}`);
console.log(`Constants: ${constants.length}`);

// ─── Determine which function PC 4713 is in ─────────────────
const TARGET_PC = 4713;
let containingFunc = '(global init / unknown)';
for (const [name, info] of funcList) {
    if (info.address <= TARGET_PC) {
        containingFunc = name;
    }
}
console.log(`\n=== TARGET PC ${TARGET_PC} is in function: ${containingFunc}() ===`);

// ─── Find source line for the target PC ─────────────────────
let sourceLine = 0;
for (const entry of sourceMap) {
    if (entry.offset <= TARGET_PC) {
        sourceLine = entry.line;
    } else {
        break;
    }
}
console.log(`Source line at PC ${TARGET_PC}: ~line ${sourceLine}`);

// ─── Disassemble around PC 4713 ─────────────────────────────
const RANGE_START = 4680;
const RANGE_END = 4750;

console.log(`\n=== BYTECODE PC ${RANGE_START}..${RANGE_END} ===`);
console.log(`${'PC'.padStart(5)}  ${'Opcode'.padEnd(18)} ${'Operand'.padEnd(20)} Hex  SrcLine`);
console.log('-'.repeat(75));

// We need to disassemble from the start of code to find instruction boundaries
let pc = codeOffset;
while (pc < codeOffset + codeSize) {
    const instrPC = pc - codeOffset;  // code-relative PC
    const op = binary[pc++];
    const opName = OpName[op] || `??? (0x${op.toString(16)})`;

    let operand = '';
    let operandDetail = '';
    let instrLen = 1; // opcode byte already read

    switch (op) {
        case Op.PUSH_I32: {
            const view = new DataView(binary.buffer, binary.byteOffset + pc, 4);
            const val = view.getInt32(0, false);
            operand = `${val}`;
            // Also show as float if it could be one
            operandDetail = `(0x${(val >>> 0).toString(16).padStart(8, '0')})`;
            pc += 4;
            break;
        }
        case Op.PUSH_F32: {
            const view = new DataView(binary.buffer, binary.byteOffset + pc, 4);
            const val = view.getFloat32(0, false);
            operand = `${val}`;
            pc += 4;
            break;
        }
        case Op.PUSH_I8: {
            let v = binary[pc];
            if (v > 127) v -= 256;
            operand = `${v}`;
            pc += 1;
            break;
        }
        case Op.PUSH_I16: {
            let v = (binary[pc] << 8) | binary[pc + 1];
            if (v > 32767) v -= 65536;
            operand = `${v}`;
            pc += 2;
            break;
        }
        case Op.LOAD_LOCAL:
        case Op.STORE_LOCAL:
        case Op.LOAD_LOCAL_ARR:
        case Op.STORE_LOCAL_ARR:
        case Op.ADDR_LOCAL:
            operand = `local[${binary[pc]}]`;
            pc += 1;
            break;
        case Op.LOAD_HEAP_ARR:
        case Op.STORE_HEAP_ARR:
        case Op.ADDR_HEAP:
            operand = `heap[${binary[pc]}]`;
            pc += 1;
            break;
        case Op.LOAD_GLOBAL:
        case Op.STORE_GLOBAL:
            operand = `global[${(binary[pc] << 8) | binary[pc + 1]}]`;
            pc += 2;
            break;
        case Op.LOAD_GLOBAL_ARR:
        case Op.STORE_GLOBAL_ARR:
        case Op.ADDR_GLOBAL:
            operand = `global_arr[${(binary[pc] << 8) | binary[pc + 1]}]`;
            pc += 2;
            break;
        case Op.JMP:
        case Op.JZ:
        case Op.JNZ: {
            const addr = (binary[pc] << 8) | binary[pc + 1];
            operand = `-> @${addr}`;
            pc += 2;
            break;
        }
        case Op.CALL: {
            const addr = (binary[pc] << 8) | binary[pc + 1];
            // Find function name for this address
            let fname = '?';
            for (const [name, info] of funcList) {
                if (info.address === addr) { fname = name; break; }
            }
            operand = `@${addr} (${fname})`;
            pc += 2;
            break;
        }
        case Op.SYSCALL: {
            const id = binary[pc];
            operand = `${SyscallName[id] || '?'} (#${id})`;
            pc += 1;
            break;
        }
        case Op.LOAD_CONST: {
            const idx = (binary[pc] << 8) | binary[pc + 1];
            const c = constants[idx];
            let preview = '';
            if (typeof c === 'string') {
                preview = `"${c.length > 30 ? c.substring(0, 30) + '...' : c}"`;
            } else {
                preview = String(c);
            }
            operand = `const[${idx}] = ${preview}`;
            pc += 2;
            break;
        }
        default:
            // No operand (single-byte instructions)
            break;
    }

    // Show if in range
    if (instrPC >= RANGE_START && instrPC <= RANGE_END) {
        // Find source line
        let line = 0;
        for (const entry of sourceMap) {
            if (entry.offset <= instrPC) line = entry.line;
            else break;
        }

        const marker = instrPC === TARGET_PC ? '>>>' : '   ';
        const hexByte = `0x${op.toString(16).padStart(2, '0')}`;
        console.log(`${marker}${String(instrPC).padStart(5)}  ${opName.padEnd(18)} ${operand.padEnd(30)} ${hexByte}  L${line}`);
    }

    if (instrPC > RANGE_END + 20) break;  // stop scanning after range
}

// ─── Also show the global variable map (to decode global[N] refs) ──
console.log('\n=== GLOBALS (first 30) ===');
const globals = compiled.globals;
const globalList = Object.entries(globals).sort((a, b) => a[1].index - b[1].index);
for (const [name, info] of globalList.slice(0, 30)) {
    const extra = info.isArray ? ` [${info.arraySize}]` : '';
    const heap = info.isHeap ? ' (HEAP)' : '';
    console.log(`  global[${String(info.index).padStart(4)}]  ${info.type}${extra}${heap}  ${name}`);
}
if (globalList.length > 30) {
    console.log(`  ... and ${globalList.length - 30} more`);
}

// ─── Show source map entries near the target PC ──────────────
console.log('\n=== SOURCE MAP near PC 4713 ===');
for (const entry of sourceMap) {
    if (entry.offset >= RANGE_START - 50 && entry.offset <= RANGE_END + 50) {
        const marker = entry.offset <= TARGET_PC ? '*' : ' ';
        console.log(`  ${marker} PC ${String(entry.offset).padStart(5)} -> line ${entry.line}`);
    }
}

// ─── Stack effect analysis around PC 4713 ────────────────────
console.log('\n=== STACK EFFECT ANALYSIS around PC 4713 ===');
console.log('(+N = pushes N values, -N = pops N values, net = change)');

// Re-scan the range to show stack effects
pc = codeOffset;
while (pc < codeOffset + codeSize) {
    const instrPC = pc - codeOffset;
    const op = binary[pc++];

    // Skip operand bytes
    let operandBytes = 0;
    switch (op) {
        case Op.PUSH_I32: case Op.PUSH_F32: operandBytes = 4; break;
        case Op.PUSH_I8: operandBytes = 1; break;
        case Op.PUSH_I16: operandBytes = 2; break;
        case Op.LOAD_LOCAL: case Op.STORE_LOCAL:
        case Op.LOAD_LOCAL_ARR: case Op.STORE_LOCAL_ARR:
        case Op.ADDR_LOCAL:
        case Op.LOAD_HEAP_ARR: case Op.STORE_HEAP_ARR: case Op.ADDR_HEAP:
        case Op.SYSCALL:
            operandBytes = 1; break;
        case Op.LOAD_GLOBAL: case Op.STORE_GLOBAL:
        case Op.LOAD_GLOBAL_ARR: case Op.STORE_GLOBAL_ARR:
        case Op.ADDR_GLOBAL:
        case Op.JMP: case Op.JZ: case Op.JNZ:
        case Op.CALL:
        case Op.LOAD_CONST:
            operandBytes = 2; break;
    }
    pc += operandBytes;

    if (instrPC >= RANGE_START && instrPC <= RANGE_END) {
        let pops = 0, pushes = 0, note = '';
        switch (op) {
            case Op.PUSH_I8: case Op.PUSH_I16: case Op.PUSH_I32: case Op.PUSH_F32:
            case Op.LOAD_LOCAL: case Op.LOAD_GLOBAL: case Op.LOAD_CONST:
            case Op.ADDR_LOCAL: case Op.ADDR_GLOBAL: case Op.ADDR_HEAP:
                pushes = 1; break;
            case Op.DUP: pushes = 1; break;
            case Op.POP: pops = 1; break;
            case Op.STORE_LOCAL: case Op.STORE_GLOBAL: pops = 1; break;
            case Op.ADD: case Op.SUB: case Op.MUL: case Op.DIV: case Op.MOD:
            case Op.FADD: case Op.FSUB: case Op.FMUL: case Op.FDIV:
            case Op.BIT_AND: case Op.BIT_OR: case Op.BIT_XOR: case Op.SHL: case Op.SHR:
            case Op.EQ: case Op.NEQ: case Op.LT: case Op.GT: case Op.LTE: case Op.GTE:
            case Op.FEQ: case Op.FNEQ: case Op.FLT: case Op.FGT: case Op.FLTE: case Op.FGTE:
                pops = 2; pushes = 1; break;
            case Op.NEG: case Op.FNEG: case Op.LOGIC_NOT: case Op.BIT_NOT:
            case Op.I2F: case Op.F2I: case Op.I2C:
                pops = 1; pushes = 1; break;
            case Op.JZ: case Op.JNZ: pops = 1; break;
            case Op.RET: break;
            case Op.RET_VAL: pops = 1; break;
            case Op.LOAD_LOCAL_ARR: case Op.LOAD_GLOBAL_ARR: case Op.LOAD_HEAP_ARR:
                pops = 1; pushes = 1; break;
            case Op.STORE_LOCAL_ARR: case Op.STORE_GLOBAL_ARR: case Op.STORE_HEAP_ARR:
                pops = 2; break;
            case Op.SYSCALL:
                note = '(varies by syscall)'; break;
            case Op.CALL:
                note = '(varies: pops args, may push retval)'; break;
        }
        const net = pushes - pops;
        const marker = instrPC === TARGET_PC ? '>>>' : '   ';
        const opName = OpName[op] || '???';
        if (note) {
            console.log(`${marker}${String(instrPC).padStart(5)}  ${opName.padEnd(18)} ${note}`);
        } else {
            console.log(`${marker}${String(instrPC).padStart(5)}  ${opName.padEnd(18)} pop=${pops} push=${pushes} net=${net >= 0 ? '+' : ''}${net}`);
        }
    }
    if (instrPC > RANGE_END + 20) break;
}
