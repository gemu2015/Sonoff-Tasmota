import { Op, OpName, SyscallName, MAGIC, VERSION } from './opcodes.js';

// Runs the same bytecode as the ESP32 C VM
// Used for in-browser testing and debugging


export class VMError extends Error {
    constructor(message, pc) {
        super(`VM error at PC=${pc}: ${message}`);
        this.pc = pc;
    }
}

const MAX_STACK = 256;
const MAX_FRAMES = 32;
const MAX_LOCALS = 256;
const MAX_GLOBALS = 8192;
const MAX_HEAP = 16384;          // 64KB heap (int32 slots)
const MAX_HEAP_HANDLES = 32;
const MAX_INSTRUCTIONS = 50000000; // safety limit (50M — allows benchmarks)

export class VM {
    constructor(options = {}) {
        this.stack = new Int32Array(MAX_STACK);     // operand stack (reinterpreted as float when needed)
        this.fstack = new Float32Array(this.stack.buffer); // float view of stack
        this.sp = 0;                                 // stack pointer
        this.pc = 0;                                 // program counter
        this.code = null;                            // bytecode
        this.codeOffset = 0;                         // where code starts in binary
        this.running = false;
        this.halted = false;

        // Globals
        this.globals = new Int32Array(MAX_GLOBALS);
        this.fglobals = new Float32Array(this.globals.buffer);

        // Call frames
        this.frames = [];
        this.frameLocals = [];
        for (let i = 0; i < MAX_FRAMES; i++) {
            this.frameLocals.push(new Int32Array(MAX_LOCALS));
        }
        this.fp = 0;  // frame pointer (current frame index)

        // Constant pool
        this.constants = [];

        // I/O callbacks
        this.onOutput = options.onOutput || ((text) => console.log(text));
        this.onError = options.onError || ((text) => console.error(text));
        this.onHalt = options.onHalt || (() => {});

        // GPIO simulation state
        this.pins = new Array(40).fill(0);
        this.pinModes = new Array(40).fill(0);

        // Timing
        this.startTime = 0;
        this.instructionCount = 0;
        this.maxInstructions = options.maxInstructions || MAX_INSTRUCTIONS;

        // Software timers (millis-based, 4 slots)
        this.timers = Array.from({length: 4}, () => ({ deadline: 0, active: false }));

        // HTTP request state (browser simulation)
        this.httpPendingHeaders = [];  // [{name, value}, ...]

        // Virtual filesystem (browser simulation)
        this.fileHandles = new Array(8).fill(null); // { name, data, pos, mode }
        this.virtualFS = new Map(); // name → Uint8Array

        // Heap (for large arrays > 255 elements)
        this.heapData = null;         // Int32Array, allocated on demand
        this.heapFloat = null;        // Float32Array view of heapData
        this.heapUsed = 0;
        this.heapHandles = [];        // { offset, size, alive }

        // String concat scratch buffers (for str1 + str2 operator)
        // strScratch holds the STRCONCAT result; strConstBuf is a read window for const pool strings.
        this.strScratch  = new Int32Array(512);  // result of str + str
        this.strConstBuf = new Int32Array(512);  // const-pool string as a char[] window

        // Callback function table (V3: name → code-relative address)
        this.callbackTable = new Map();

        // Persist table (V4: global entries for auto-save/load)
        this.persistEntries = [];  // { index, slotCount }

        // UDP globals table (V5: auto-update from UDP packets)
        this.udpGlobalEntries = [];  // { name, index, slotCount }

        // UDP multicast simulation (browser)
        this.udpVars = new Map();       // name → { value: float, ready: bool }

        // Debugger
        this.breakpoints = new Set();
        this.singleStep = false;
        this.onBreakpoint = options.onBreakpoint || (() => {});
        this.sourceMap = [];
    }

    // ─── Loading ─────────────────────────────────────────────

    load(compiled) {
        const { binary, sourceMap, codeOffset } = compiled;

        // Parse header
        const view = new DataView(binary.buffer, binary.byteOffset, binary.byteLength);
        const magic = view.getUint32(0, false);
        if (magic !== MAGIC) {
            throw new VMError('Invalid binary: bad magic', 0);
        }
        const version = view.getUint16(4, false);
        if (version < 2 || version > VERSION) {
            throw new VMError(`Unsupported version: ${version} (expected 2..${VERSION})`, 0);
        }

        const globalSize = view.getUint16(6, false);
        const entryPoint = view.getUint16(8, false);
        const constPoolSize = view.getUint16(10, false);
        const heapDeclSize = view.getUint16(12, false);

        // V5 adds globalsTableSize at bytes 18-19; V4 adds persistTableSize at bytes 16-17; V3 adds funcTableSize at bytes 14-15; V2 has 14-byte header
        const headerSize = version >= 5 ? 20 : (version >= 4 ? 18 : (version >= 3 ? 16 : 14));
        const funcTableSize = version >= 3 ? view.getUint16(14, false) : 0;
        const persistTableSize = version >= 4 ? view.getUint16(16, false) : 0;
        const globalsTableSize = version >= 5 ? view.getUint16(18, false) : 0;

        // Parse constant pool
        this.constants = [];
        let offset = headerSize;
        const constEnd = headerSize + constPoolSize;
        while (offset < constEnd) {
            const type = binary[offset++];
            if (type === 0x01) {
                // String
                const len = (binary[offset] << 8) | binary[offset + 1];
                offset += 2;
                const bytes = binary.slice(offset, offset + len);
                this.constants.push(new TextDecoder().decode(bytes));
                offset += len;
            } else if (type === 0x02) {
                // Float
                const fView = new DataView(binary.buffer, binary.byteOffset + offset, 4);
                this.constants.push(fView.getFloat32(0, false));
                offset += 4;
            }
        }

        // Parse heap declarations
        this.heapData = null;
        this.heapFloat = null;
        this.heapUsed = 0;
        this.heapHandles = [];
        const heapEnd = constEnd + heapDeclSize;
        if (heapDeclSize > 0) {
            const count = binary[constEnd];
            // Compute total heap size needed
            let totalHeap = 0;
            for (let i = 0; i < count; i++) {
                const sz = (binary[constEnd + 1 + i * 3 + 1] << 8) | binary[constEnd + 1 + i * 3 + 2];
                totalHeap += sz;
            }
            // Allocate heap
            const heapSize = Math.max(totalHeap, MAX_HEAP);
            this.heapData = new Int32Array(heapSize);
            this.heapFloat = new Float32Array(this.heapData.buffer);
            // Pre-allocate blocks
            for (let i = 0; i < count; i++) {
                const handle = binary[constEnd + 1 + i * 3];
                const sz = (binary[constEnd + 1 + i * 3 + 1] << 8) | binary[constEnd + 1 + i * 3 + 2];
                while (this.heapHandles.length <= handle) {
                    this.heapHandles.push(null);
                }
                this.heapHandles[handle] = { offset: this.heapUsed, size: sz, alive: true };
                this.heapUsed += sz;
            }
        }

        // Parse function table (V3)
        this.callbackTable = new Map();
        const funcTableStart = heapEnd;
        const funcTableEnd = funcTableStart + funcTableSize;
        if (funcTableSize > 0) {
            let pos = funcTableStart;
            const count = binary[pos++];
            for (let i = 0; i < count && pos < funcTableEnd; i++) {
                const nameLen = binary[pos++];
                const nameBytes = binary.slice(pos, pos + nameLen);
                const name = new TextDecoder().decode(nameBytes);
                pos += nameLen;
                const addr = (binary[pos] << 8) | binary[pos + 1];
                pos += 2;
                this.callbackTable.set(name, addr);
            }
        }

        // Parse persist table (V4)
        this.persistEntries = [];
        const persistTableStart = funcTableEnd;
        const persistTableEnd = persistTableStart + persistTableSize;
        if (persistTableSize > 0) {
            let pos = persistTableStart;
            const count = binary[pos++];
            for (let i = 0; i < count && pos < persistTableEnd; i++) {
                const index = (binary[pos] << 8) | binary[pos + 1];
                pos += 2;
                const slotCount = (binary[pos] << 8) | binary[pos + 1];
                pos += 2;
                this.persistEntries.push({ index, slotCount });
            }
        }

        // Parse globals table (V5: UDP auto-update variables)
        this.udpGlobalEntries = [];
        const globalsTableStart = persistTableEnd;
        const globalsTableEnd = globalsTableStart + globalsTableSize;
        if (globalsTableSize > 0) {
            let pos = globalsTableStart;
            const count = binary[pos++];
            for (let i = 0; i < count && pos < globalsTableEnd; i++) {
                const nameLen = binary[pos++];
                const nameBytes = binary.slice(pos, pos + nameLen);
                const name = new TextDecoder().decode(nameBytes);
                pos += nameLen;
                const index = (binary[pos] << 8) | binary[pos + 1];
                pos += 2;
                const slotCount = (binary[pos] << 8) | binary[pos + 1];
                pos += 2;
                // Type byte: 0=float scalar, 1=float array, 2=char array
                const varType = (pos < globalsTableEnd) ? binary[pos++] : 0;
                this.udpGlobalEntries.push({ name, index, slotCount, varType });
            }
        }

        // Code starts after header + constant pool + heap declarations + function table + persist table + globals table
        this.codeOffset = globalsTableEnd;
        this.code = binary;
        this.pc = this.codeOffset + entryPoint;
        this.sourceMap = sourceMap || [];
        this.sp = 0;
        this.fp = 0;
        this.frames = [];
        this.globals.fill(0);
        this.halted = false;
        this.instructionCount = 0;

        // Auto-load persist vars from virtual filesystem
        this.persistLoad();
    }

    // ─── Bytecode reading ────────────────────────────────────

    readU8() {
        return this.code[this.pc++];
    }

    readI8() {
        let v = this.code[this.pc++];
        if (v > 127) v -= 256;
        return v;
    }

    readU16() {
        const hi = this.code[this.pc++];
        const lo = this.code[this.pc++];
        return (hi << 8) | lo;
    }

    readI32() {
        const view = new DataView(this.code.buffer, this.code.byteOffset + this.pc, 4);
        this.pc += 4;
        return view.getInt32(0, false);
    }

    readF32() {
        const view = new DataView(this.code.buffer, this.code.byteOffset + this.pc, 4);
        this.pc += 4;
        return view.getFloat32(0, false);
    }

    // ─── Stack operations ────────────────────────────────────

    push(val) {
        if (this.sp >= MAX_STACK) throw new VMError('Stack overflow', this.pc);
        this.stack[this.sp++] = val;
    }

    pushf(val) {
        if (this.sp >= MAX_STACK) throw new VMError('Stack overflow', this.pc);
        this.fstack[this.sp++] = val;
    }

    pop() {
        if (this.sp <= 0) throw new VMError('Stack underflow', this.pc);
        return this.stack[--this.sp];
    }

    popf() {
        if (this.sp <= 0) throw new VMError('Stack underflow', this.pc);
        return this.fstack[--this.sp];
    }

    peek() {
        if (this.sp <= 0) throw new VMError('Stack underflow', this.pc);
        return this.stack[this.sp - 1];
    }

    // ─── UDP global auto-update (browser simulation) ───────

    updateUdpGlobal(name, floatValue) {
        for (const entry of this.udpGlobalEntries) {
            if (entry.name === name) {
                if (entry.slotCount === 1) {
                    // Scalar: store float as int32 bits
                    const tmp = new Float32Array(1);
                    tmp[0] = floatValue;
                    this.globals[entry.index] = new Int32Array(tmp.buffer)[0];
                }
                return true;
            }
        }
        return false;
    }

    updateUdpGlobalArray(name, floatArray) {
        for (const entry of this.udpGlobalEntries) {
            if (entry.name === name && entry.slotCount > 1) {
                const n = Math.min(floatArray.length, entry.slotCount);
                const tmp = new Float32Array(1);
                for (let i = 0; i < n; i++) {
                    tmp[0] = floatArray[i];
                    this.globals[entry.index + i] = new Int32Array(tmp.buffer)[0];
                }
                return true;
            }
        }
        return false;
    }

    // ─── Execution ──────────────────────────────────────────

    run() {
        this.running = true;
        this.startTime = Date.now();

        while (this.running && !this.halted) {
            if (this.instructionCount++ > this.maxInstructions) {
                throw new VMError('Instruction limit exceeded (infinite loop?)', this.pc);
            }

            // Breakpoint check
            if (this.breakpoints.has(this.pc - this.codeOffset) || this.singleStep) {
                this.running = false;
                this.onBreakpoint(this.pc - this.codeOffset);
                return;
            }

            this.step();
        }

        this.onHalt();
    }

    // ─── Callback invocation ─────────────────────────────────
    // Call a named callback function (e.g. 'JsonCall', 'WebCall', 'EverySecond')
    // VM must be halted (main returned). Globals/heap persist across calls.
    callFunction(name) {
        const addr = this.callbackTable.get(name);
        if (addr === undefined) return undefined; // callback not defined

        // Save VM state
        const savedHalted = this.halted;
        const savedPC = this.pc;
        const savedSP = this.sp;
        const savedFrameCount = this.frames.length;

        // Un-halt and set up callback frame
        this.halted = false;
        this.running = true;
        if (this.frames.length >= MAX_FRAMES) throw new VMError('Call stack overflow in callback', this.pc);
        this.frames.push({
            returnPC: 0xFFFF,  // sentinel — callback returns when frame pops
            prevFP: this.fp,
        });
        this.fp = this.frames.length - 1;
        this.pc = this.codeOffset + addr;

        // Run until the callback function returns (frame count drops back)
        const MAX_CALLBACK_INSTR = 200000;
        let count = 0;
        while (this.frames.length > savedFrameCount && !this.halted && count < MAX_CALLBACK_INSTR) {
            this.step();
            count++;
        }

        // Restore halted state; globals/heap persist
        this.halted = savedHalted;
        this.pc = savedPC;
        this.running = false;

        // Return value if one was pushed
        if (this.sp > savedSP) {
            return this.pop();
        }
        return undefined;
    }

    step() {
        const op = this.readU8();

        switch (op) {
            case Op.NOP:
                break;

            case Op.HALT:
                this.halted = true;
                this.running = false;
                break;

            // ─── Stack ────────────────────────────
            case Op.PUSH_I32:
                this.push(this.readI32());
                break;

            case Op.PUSH_F32:
                this.pushf(this.readF32());
                break;

            case Op.PUSH_I8:
                this.push(this.readI8());
                break;

            case Op.PUSH_I16: {
                let v = this.readU16();
                if (v > 32767) v -= 65536; // sign extend
                this.push(v);
                break;
            }

            case Op.POP:
                this.pop();
                break;

            case Op.DUP:
                this.push(this.peek());
                break;

            // ─── Integer arithmetic ───────────────
            case Op.ADD: {
                const b = this.pop(), a = this.pop();
                this.push((a + b) | 0);
                break;
            }
            case Op.SUB: {
                const b = this.pop(), a = this.pop();
                this.push((a - b) | 0);
                break;
            }
            case Op.MUL: {
                const b = this.pop(), a = this.pop();
                this.push(Math.imul(a, b));
                break;
            }
            case Op.DIV: {
                const b = this.pop(), a = this.pop();
                if (b === 0) throw new VMError('Division by zero', this.pc);
                this.push((a / b) | 0);
                break;
            }
            case Op.MOD: {
                const b = this.pop(), a = this.pop();
                if (b === 0) throw new VMError('Modulo by zero', this.pc);
                this.push(a % b);
                break;
            }
            case Op.NEG:
                this.push(-this.pop());
                break;

            // ─── Float arithmetic ─────────────────
            case Op.FADD: {
                const b = this.popf(), a = this.popf();
                this.pushf(a + b);
                break;
            }
            case Op.FSUB: {
                const b = this.popf(), a = this.popf();
                this.pushf(a - b);
                break;
            }
            case Op.FMUL: {
                const b = this.popf(), a = this.popf();
                this.pushf(a * b);
                break;
            }
            case Op.FDIV: {
                const b = this.popf(), a = this.popf();
                if (b === 0) throw new VMError('Float division by zero', this.pc);
                this.pushf(a / b);
                break;
            }
            case Op.FNEG:
                this.pushf(-this.popf());
                break;

            // ─── Bitwise ──────────────────────────
            case Op.BIT_AND: {
                const b = this.pop(), a = this.pop();
                this.push(a & b);
                break;
            }
            case Op.BIT_OR: {
                const b = this.pop(), a = this.pop();
                this.push(a | b);
                break;
            }
            case Op.BIT_XOR: {
                const b = this.pop(), a = this.pop();
                this.push(a ^ b);
                break;
            }
            case Op.BIT_NOT:
                this.push(~this.pop());
                break;
            case Op.SHL: {
                const b = this.pop(), a = this.pop();
                this.push(a << b);
                break;
            }
            case Op.SHR: {
                const b = this.pop(), a = this.pop();
                this.push(a >> b);
                break;
            }

            // ─── Integer comparison ───────────────
            case Op.EQ: {
                const b = this.pop(), a = this.pop();
                this.push(a === b ? 1 : 0);
                break;
            }
            case Op.NEQ: {
                const b = this.pop(), a = this.pop();
                this.push(a !== b ? 1 : 0);
                break;
            }
            case Op.LT: {
                const b = this.pop(), a = this.pop();
                this.push(a < b ? 1 : 0);
                break;
            }
            case Op.GT: {
                const b = this.pop(), a = this.pop();
                this.push(a > b ? 1 : 0);
                break;
            }
            case Op.LTE: {
                const b = this.pop(), a = this.pop();
                this.push(a <= b ? 1 : 0);
                break;
            }
            case Op.GTE: {
                const b = this.pop(), a = this.pop();
                this.push(a >= b ? 1 : 0);
                break;
            }

            // ─── Float comparison ─────────────────
            case Op.FEQ: {
                const b = this.popf(), a = this.popf();
                this.push(a === b ? 1 : 0);
                break;
            }
            case Op.FNEQ: {
                const b = this.popf(), a = this.popf();
                this.push(a !== b ? 1 : 0);
                break;
            }
            case Op.FLT: {
                const b = this.popf(), a = this.popf();
                this.push(a < b ? 1 : 0);
                break;
            }
            case Op.FGT: {
                const b = this.popf(), a = this.popf();
                this.push(a > b ? 1 : 0);
                break;
            }
            case Op.FLTE: {
                const b = this.popf(), a = this.popf();
                this.push(a <= b ? 1 : 0);
                break;
            }
            case Op.FGTE: {
                const b = this.popf(), a = this.popf();
                this.push(a >= b ? 1 : 0);
                break;
            }

            // ─── Logical ──────────────────────────
            case Op.LOGIC_AND: {
                const b = this.pop(), a = this.pop();
                this.push((a && b) ? 1 : 0);
                break;
            }
            case Op.LOGIC_OR: {
                const b = this.pop(), a = this.pop();
                this.push((a || b) ? 1 : 0);
                break;
            }
            case Op.LOGIC_NOT:
                this.push(this.pop() ? 0 : 1);
                break;

            // ─── Control flow ─────────────────────
            case Op.JMP: {
                const addr = this.readU16();
                this.pc = this.codeOffset + addr;
                break;
            }
            case Op.JZ: {
                const addr = this.readU16();
                if (this.pop() === 0) {
                    this.pc = this.codeOffset + addr;
                }
                break;
            }
            case Op.JNZ: {
                const addr = this.readU16();
                if (this.pop() !== 0) {
                    this.pc = this.codeOffset + addr;
                }
                break;
            }
            case Op.CALL: {
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
            }
            case Op.RET: {
                if (this.frames.length === 0) {
                    this.halted = true;
                    this.running = false;
                    break;
                }
                const frame = this.frames.pop();
                this.pc = frame.returnPC;
                this.fp = frame.prevFP;
                break;
            }
            case Op.RET_VAL: {
                const retVal = this.pop();
                if (this.frames.length === 0) {
                    this.push(retVal);
                    this.halted = true;
                    this.running = false;
                    break;
                }
                const frame = this.frames.pop();
                this.pc = frame.returnPC;
                this.fp = frame.prevFP;
                this.push(retVal);
                break;
            }

            // ─── Variables ────────────────────────
            case Op.LOAD_LOCAL: {
                const idx = this.readU8();
                this.push(this.frameLocals[this.fp][idx]);
                break;
            }
            case Op.STORE_LOCAL: {
                const idx = this.readU8();
                this.frameLocals[this.fp][idx] = this.pop();
                break;
            }
            case Op.LOAD_GLOBAL: {
                const idx = this.readU16();
                this.push(this.globals[idx]);
                break;
            }
            case Op.STORE_GLOBAL: {
                const idx = this.readU16();
                this.globals[idx] = this.pop();
                break;
            }
            case Op.STORE_GLOBAL_UDP: {
                const idx = this.readU16();
                const nameIdx = this.readU16();
                const val = this.pop();
                this.globals[idx] = val;
                // In simulator, just log the UDP send
                if (this.constants[nameIdx] && this.constants[nameIdx].type === 'string') {
                    this.log(`UDP send: ${this.constants[nameIdx].value} = ${this.i2f(val)}`);
                }
                break;
            }
            case Op.STORE_WATCH: {
                const varIdx = this.readU16();
                const shadowIdx = this.readU16();
                const writtenIdx = this.readU16();
                const val = this.pop();
                this.globals[shadowIdx] = this.globals[varIdx]; // save old value
                this.globals[writtenIdx] = 1;                    // set written flag
                this.globals[varIdx] = val;                      // store new value
                break;
            }

            // ─── Arrays ──────────────────────────
            case Op.LOAD_LOCAL_ARR: {
                const base = this.readU8();
                const idx = this.pop();
                this.push(this.frameLocals[this.fp][base + idx]);
                break;
            }
            case Op.STORE_LOCAL_ARR: {
                const base = this.readU8();
                const val = this.pop();
                const idx = this.pop();
                this.frameLocals[this.fp][base + idx] = val;
                break;
            }
            case Op.LOAD_GLOBAL_ARR: {
                const base = this.readU16();
                const idx = this.pop();
                this.push(this.globals[base + idx]);
                break;
            }
            case Op.STORE_GLOBAL_ARR: {
                const base = this.readU16();
                const val = this.pop();
                const idx = this.pop();
                this.globals[base + idx] = val;
                break;
            }

            // ─── Type conversion ──────────────────
            case Op.I2F: {
                const ival = this.pop();
                this.pushf(ival);
                break;
            }
            case Op.F2I: {
                const fval = this.popf();
                this.push(fval | 0);
                break;
            }
            case Op.I2C: {
                this.push(this.pop() & 0xFF);
                break;
            }

            // ─── Array address refs ───────────────
            case Op.ADDR_LOCAL: {
                const idx = this.readU8();
                // Pack: (fp << 16) | base_idx
                this.push((this.fp << 16) | idx);
                break;
            }
            case Op.ADDR_GLOBAL: {
                const idx = this.readU16();
                // Pack: 0x80000000 | base_idx
                this.push((0x80000000 | idx) | 0);
                break;
            }

            // ─── Heap arrays ─────────────────────
            case Op.LOAD_HEAP_ARR: {
                const handle = this.readU8();
                const idx = this.pop();
                const h = this.heapHandles[handle];
                if (!h || !h.alive || idx < 0 || idx >= h.size)
                    throw new VMError(`Heap bounds: handle=${handle} idx=${idx}`, this.pc);
                this.push(this.heapData[h.offset + idx]);
                break;
            }
            case Op.STORE_HEAP_ARR: {
                const handle = this.readU8();
                const val = this.pop();
                const idx = this.pop();
                const h = this.heapHandles[handle];
                if (!h || !h.alive || idx < 0 || idx >= h.size)
                    throw new VMError(`Heap bounds: handle=${handle} idx=${idx}`, this.pc);
                this.heapData[h.offset + idx] = val;
                break;
            }
            case Op.ADDR_HEAP: {
                const handle = this.readU8();
                // Pack: 0xC0000000 | handle
                this.push((0xC0000000 | handle) | 0);
                break;
            }
            case Op.ADDR_HEAP_OFF: {
                const handle = this.readU8();
                let off = this.pop();
                if (off < 0) off = 0;
                if (off > 0x3FFF) off = 0x3FFF;
                // Pack: 0xC0000000 | (offset << 16) | handle — matches tc_make_heap_ref()
                this.push((0xC0000000 | (off << 16) | handle) | 0);
                break;
            }

            // ─── Runtime array ref (ref params) ──
            case Op.LOAD_REF_ARR: {
                const localIdx = this.readU8();
                const idx = this.pop();
                const ref = this.frameLocals[this.fp][localIdx];
                const resolved = this.resolveRef(ref);
                if (resolved.maxLen !== undefined && (idx < 0 || idx >= resolved.maxLen))
                    throw new VMError(`Ref-array bounds: idx=${idx} len=${resolved.maxLen}`, this.pc);
                this.push(resolved.arr[resolved.base + idx]);
                break;
            }
            case Op.STORE_REF_ARR: {
                const localIdx = this.readU8();
                const val = this.pop();
                const idx = this.pop();
                const ref = this.frameLocals[this.fp][localIdx];
                const resolved = this.resolveRef(ref);
                if (resolved.maxLen !== undefined && (idx < 0 || idx >= resolved.maxLen))
                    throw new VMError(`Ref-array bounds: idx=${idx} len=${resolved.maxLen}`, this.pc);
                resolved.arr[resolved.base + idx] = val;
                break;
            }

            // ─── Constants ────────────────────────
            case Op.LOAD_CONST: {
                const idx = this.readU16();
                const c = this.constants[idx];
                if (typeof c === 'string') {
                    // For strings, push the constant index
                    this.push(idx);
                } else {
                    this.pushf(c);
                }
                break;
            }

            // ─── Syscalls ─────────────────────────
            case Op.SYSCALL: {
                const id = this.readU8();
                this.executeSyscall(id);
                break;
            }
            case Op.SYSCALL2: {
                const id = this.readU16();
                this.executeSyscall(id);
                break;
            }

            default:
                throw new VMError(`Unknown opcode: 0x${op.toString(16)} (${OpName[op] || '?'})`, this.pc - 1);
        }
    }

    // ─── Array ref resolution ───────────────────────────────
    // Returns { arr: Int32Array, base: number } for accessing char arrays
    resolveRef(ref) {
        const uref = ref >>> 0; // unsigned
        const tag = uref >>> 30;
        if (tag === 3) {
            const lowWord = uref & 0xFFFF;
            // 0xFFFE — scratch buffer ref (result of STRCONCAT)
            if (lowWord === 0xFFFE) {
                return { arr: this.strScratch, base: 0, maxLen: 512 };
            }
            // 0x8000..0xFFFD — const pool string ref (string literal used in str + "lit").
            // Detect via bit 15 only — the upper 16 bits now carry a slot offset for heap refs.
            if (lowWord & 0x8000) {
                const idx = lowWord & 0x7FFF;
                const s = this.constants[idx] || '';
                for (let i = 0; i < s.length && i < 511; i++) this.strConstBuf[i] = s.charCodeAt(i);
                this.strConstBuf[Math.min(s.length, 511)] = 0;
                return { arr: this.strConstBuf, base: 0, maxLen: 512 };
            }
            // Normal heap ref:
            //   bits 31-30 = 11
            //   bits 29-16 = slot offset (0..16383)  — 0 for classic ADDR_HEAP
            //   bit    15  = 0
            //   bits  7-0  = handle (0..255, TC_MAX_HEAP_HANDLES=128 in practice)
            const handle = lowWord & 0xFF;
            const offset = (uref >>> 16) & 0x3FFF;
            const h = this.heapHandles[handle];
            if (!h || !h.alive) throw new VMError(`Invalid heap ref: handle ${handle}`, this.pc);
            if (offset >= h.size) throw new VMError(`Heap ref offset ${offset} >= size ${h.size} (handle ${handle})`, this.pc);
            return { arr: this.heapData, base: h.offset + offset, maxLen: h.size - offset };
        }
        if (tag === 2) {
            // Global ref: bit 31=1, bit 30=0
            const base = uref & 0xFFFF;
            return { arr: this.globals, base };
        }
        // Local ref: bits 31-30 = 00 or 01
        const fp = (uref >> 16) & 0xFF;
        const base = uref & 0xFF;
        return { arr: this.frameLocals[fp], base };
    }

    // Read a null-terminated string from an array ref
    readStringFromRef(ref) {
        const resolved = this.resolveRef(ref);
        const { arr, base } = resolved;
        const maxSlots = resolved.maxLen || Math.min(256, arr.length - base);
        let s = '';
        for (let i = 0; i < maxSlots; i++) {
            const ch = arr[base + i];
            if (ch === 0) break;
            s += String.fromCharCode(ch & 0xFF);
        }
        return s;
    }

    // Write a string into an array ref (with null terminator)
    writeStringToRef(ref, str, maxLen = 256) {
        const resolved = this.resolveRef(ref);
        const { arr, base } = resolved;
        const limit = Math.min(resolved.maxLen || maxLen, arr.length - base);
        let i;
        for (i = 0; i < str.length && i < limit - 1; i++) {
            arr[base + i] = str.charCodeAt(i);
        }
        if (base + i < arr.length) arr[base + i] = 0;
    }

    // ─── Syscall dispatch ────────────────────────────────────

    executeSyscall(id) {
        switch (id) {
            // GPIO (simulated)
            case 0: { // PIN_MODE
                const mode = this.pop();
                const pin = this.pop();
                this.pinModes[pin] = mode;
                this.onOutput(`[GPIO] pinMode(${pin}, ${mode === 1 ? 'OUTPUT' : 'INPUT'})\n`);
                break;
            }
            case 1: { // DIGITAL_WRITE
                const val = this.pop();
                const pin = this.pop();
                this.pins[pin] = val;
                this.onOutput(`[GPIO] digitalWrite(${pin}, ${val})\n`);
                break;
            }
            case 2: { // DIGITAL_READ
                const pin = this.pop();
                this.push(this.pins[pin]);
                break;
            }
            case 3: { // ANALOG_READ
                const pin = this.pop();
                this.push(Math.floor(Math.random() * 4096)); // simulate 12-bit ADC
                break;
            }
            case 4: { // ANALOG_WRITE
                const val = this.pop();
                const pin = this.pop();
                this.onOutput(`[GPIO] analogWrite(${pin}, ${val})\n`);
                break;
            }
            case 5: { // GPIO_INIT (release from Tasmota + pinMode)
                const mode = this.pop();
                const pin = this.pop();
                this.pinModes[pin] = mode;
                this.onOutput(`[GPIO] gpioInit(${pin}, ${mode === 1 ? 'OUTPUT' : 'INPUT'}) — released from Tasmota\n`);
                break;
            }
            // 1-Wire (native bit-bang — simulated)
            case 6: { // OW_SET_PIN
                const pin = this.pop();
                this._owPin = pin;
                this.onOutput(`[OW] owSetPin(${pin})\n`);
                break;
            }
            case 7: { // OW_RESET
                this.push(1); // simulate presence detected
                this.onOutput(`[OW] owReset() → 1 (presence)\n`);
                break;
            }
            case 8: { // OW_WRITE
                const byte = this.pop();
                this.onOutput(`[OW] owWrite(0x${(byte & 0xFF).toString(16).padStart(2, '0')})\n`);
                break;
            }
            case 9: { // OW_READ
                this.push(0xFF); // simulate read
                this.onOutput(`[OW] owRead() → 0xFF\n`);
                break;
            }
            case 98: { // OW_WRITE_BIT
                const bit = this.pop();
                this.onOutput(`[OW] owWriteBit(${bit})\n`);
                break;
            }
            case 99: { // OW_READ_BIT
                this.push(1); // simulate read
                this.onOutput(`[OW] owReadBit() → 1\n`);
                break;
            }
            case 204: { // OW_SEARCH_RESET
                this.onOutput(`[OW] owSearchReset()\n`);
                break;
            }
            case 205: { // OW_SEARCH
                const buf = this.pop(); // buffer address (global index)
                this.push(0); // simulate: no more devices
                this.onOutput(`[OW] owSearch(buf@${buf}) → 0\n`);
                break;
            }

            // Timing
            case 10: { // DELAY
                const ms = this.pop();
                this.onOutput(`[TIME] delay(${ms}ms)\n`);
                break;
            }
            case 11: { // DELAY_MICRO
                const us = this.pop();
                this.onOutput(`[TIME] delayMicroseconds(${us}us)\n`);
                break;
            }
            case 12: { // MILLIS
                this.push((Date.now() - this.startTime) | 0);
                break;
            }
            case 13: { // MICROS
                this.push(((Date.now() - this.startTime) * 1000) | 0);
                break;
            }
            case 14: { // TIMER_START
                const ms = this.pop();
                const id = this.pop();
                if (id >= 0 && id < this.timers.length) {
                    this.timers[id].deadline = Date.now() + ms;
                    this.timers[id].active = true;
                }
                break;
            }
            case 15: { // TIMER_DONE
                const id = this.pop();
                let result = 1; // default: done (not started)
                if (id >= 0 && id < this.timers.length && this.timers[id].active) {
                    result = (Date.now() >= this.timers[id].deadline) ? 1 : 0;
                }
                this.push(result);
                break;
            }
            case 16: { // TIMER_STOP
                const id = this.pop();
                if (id >= 0 && id < this.timers.length) {
                    this.timers[id].active = false;
                }
                break;
            }
            case 17: { // TIMER_REMAINING
                const id = this.pop();
                let remaining = 0;
                if (id >= 0 && id < this.timers.length && this.timers[id].active) {
                    remaining = Math.max(0, this.timers[id].deadline - Date.now());
                }
                this.push(remaining | 0);
                break;
            }

            // Serial
            case 20: { // SERIAL_BEGIN (rxpin, txpin, baud, config, bufsize) -> int
                const bufsize = this.pop();
                const config = this.pop();
                const baud = this.pop();
                const txpin = this.pop();
                const rxpin = this.pop();
                const cfgNames = ['5N1','6N1','7N1','8N1','5N2','6N2','7N2','8N2',
                                  '5E1','6E1','7E1','8E1','5E2','6E2','7E2','8E2',
                                  '5O1','6O1','7O1','8O1','5O2','6O2','7O2','8O2'];
                const cfgStr = cfgNames[config] || `cfg${config}`;
                this.onOutput(`[SERIAL] begin(rx=${rxpin}, tx=${txpin}, ${baud} ${cfgStr}, buf=${bufsize})\n`);
                this.push(1); // success in simulator
                break;
            }
            case 27: { // SERIAL_CLOSE
                this.onOutput(`[SERIAL] close()\n`);
                break;
            }
            case 28: { // SERIAL_WRITE_BYTE
                const byte = this.pop();
                this.onOutput(`[SERIAL] writeByte(0x${(byte & 0xFF).toString(16).padStart(2,'0')})\n`);
                break;
            }
            case 29: { // SERIAL_WRITE_STR (buf_ref)
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(`[SERIAL] write("${str}")\n`);
                break;
            }
            case 18: { // SERIAL_WRITE_BUF (buf_ref, len)
                const len = this.pop();
                const ref = this.pop();
                const resolved = this.resolveRef(ref);
                const { arr, base } = resolved;
                let hex = '';
                for (let i = 0; i < len && base + i < arr.length; i++) {
                    hex += (arr[base + i] & 0xFF).toString(16).padStart(2, '0') + ' ';
                }
                this.onOutput(`[SERIAL] writeBytes(${len}): ${hex.trim()}\n`);
                break;
            }
            case 21: { // SERIAL_PRINT (string)
                const idx = this.pop();
                const str = this.constants[idx] || `[const:${idx}]`;
                this.onOutput(str);
                break;
            }
            case 22: { // SERIAL_PRINT_INT
                const val = this.pop();
                this.onOutput(String(val));
                break;
            }
            case 23: { // SERIAL_PRINT_FLOAT
                const val = this.popf();
                this.onOutput(val.toFixed(2));
                break;
            }
            case 24: { // SERIAL_PRINTLN (string)
                const idx = this.pop();
                const str = this.constants[idx] || `[const:${idx}]`;
                this.onOutput(str + '\n');
                break;
            }
            case 25: { // SERIAL_READ
                this.push(-1); // no input in simulator
                break;
            }
            case 26: { // SERIAL_AVAILABLE
                this.push(0);
                break;
            }

            // Math
            case 30: { // ABS
                const v = this.pop();
                this.push(Math.abs(v));
                break;
            }
            case 31: { // MIN
                const b = this.pop(), a = this.pop();
                this.push(Math.min(a, b));
                break;
            }
            case 32: { // MAX
                const b = this.pop(), a = this.pop();
                this.push(Math.max(a, b));
                break;
            }
            case 33: { // MAP
                const toHi = this.pop();
                const toLo = this.pop();
                const fromHi = this.pop();
                const fromLo = this.pop();
                const val = this.pop();
                const mapped = toLo + ((val - fromLo) * (toHi - toLo)) / (fromHi - fromLo);
                this.push(mapped | 0);
                break;
            }
            case 34: { // RANDOM
                const mx = this.pop();
                const mn = this.pop();
                this.push(mn + Math.floor(Math.random() * (mx - mn)));
                break;
            }
            case 35: { // SQRT
                const v = this.popf();
                this.pushf(Math.sqrt(v));
                break;
            }
            case 36: { // SIN
                const v = this.popf();
                this.pushf(Math.sin(v));
                break;
            }
            case 37: { // COS
                const v = this.popf();
                this.pushf(Math.cos(v));
                break;
            }
            case 38: { // FLOOR
                const v = this.popf();
                this.push(Math.floor(v));
                break;
            }
            case 39: { // CEIL
                const v = this.popf();
                this.push(Math.ceil(v));
                break;
            }
            case 40: { // ROUND
                const v = this.popf();
                this.push(Math.round(v));
                break;
            }
            case 198: { // EXP
                const v = this.popf();
                this.pushf(Math.exp(v));
                break;
            }
            case 199: { // LOG (natural)
                const v = this.popf();
                this.pushf(Math.log(v));
                break;
            }
            case 49: { // INT_BITS_TO_FLOAT — reinterpret int32 bits as IEEE754 float
                // Identity in the simulator: value stays on stack, compiler treats as float
                break;
            }

            // Debug
            // String operations
            case 50: { // STRLEN
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.push(str.length);
                break;
            }
            case 51: { // STRCPY
                const srcRef = this.pop();
                const dstRef = this.pop();
                const src = this.readStringFromRef(srcRef);
                this.writeStringToRef(dstRef, src);
                break;
            }
            case 52: { // STRCAT
                const srcRef = this.pop();
                const dstRef = this.pop();
                const dst = this.readStringFromRef(dstRef);
                const src = this.readStringFromRef(srcRef);
                this.writeStringToRef(dstRef, dst + src);
                break;
            }
            case 259: { // STRCONCAT (str1 + str2 operator)
                const rightRef = this.pop();
                const leftRef  = this.pop();
                const left  = this.readStringFromRef(leftRef);
                const right = this.readStringFromRef(rightRef);
                const combined = left + right;
                for (let i = 0; i < combined.length && i < 511; i++) {
                    this.strScratch[i] = combined.charCodeAt(i);
                }
                this.strScratch[Math.min(combined.length, 511)] = 0;
                // Push scratch ref: tag=3 (bits 31-30=11), handle=0xFFFE
                this.push((0xC000FFFE | 0));
                break;
            }
            case 53: { // STRCMP
                const bRef = this.pop();
                const aRef = this.pop();
                const a = this.readStringFromRef(aRef);
                const b = this.readStringFromRef(bRef);
                this.push(a < b ? -1 : a > b ? 1 : 0);
                break;
            }
            case 54: { // STR_PRINT
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(str);
                break;
            }
            case 55: { // STRCPY_CONST
                const constIdx = this.pop();
                const dstRef = this.pop();
                const str = this.constants[constIdx] || '';
                this.writeStringToRef(dstRef, str);
                break;
            }
            case 56: { // STRCAT_CONST
                const constIdx = this.pop();
                const dstRef = this.pop();
                const dst = this.readStringFromRef(dstRef);
                const src = this.constants[constIdx] || '';
                this.writeStringToRef(dstRef, dst + src);
                break;
            }

            // sprintf variants (browser simulation)
            case 57: { // SPRINTF_INT
                const val = this.pop();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%d';
                // Simple format: replace first %d/%u/%x/%X/%o/%c with value
                let result;
                if (fmt.includes('%x') || fmt.includes('%X')) {
                    result = fmt.replace(/%[0-9]*[xX]/, (m) => {
                        const s = (val >>> 0).toString(16);
                        return m.includes('X') ? s.toUpperCase() : s;
                    });
                } else if (fmt.includes('%o')) {
                    result = fmt.replace(/%[0-9]*o/, () => (val >>> 0).toString(8));
                } else if (fmt.includes('%c')) {
                    result = fmt.replace(/%c/, () => String.fromCharCode(val & 0xFF));
                } else if (fmt.includes('%u')) {
                    result = fmt.replace(/%[0-9]*u/, () => (val >>> 0).toString());
                } else {
                    result = fmt.replace(/%[-0-9]*d/, () => val.toString());
                }
                this.writeStringToRef(dstRef, result);
                this.push(result.length);
                break;
            }
            case 58: { // SPRINTF_FLT
                const fval = this.popf();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%.2f';
                // Parse precision from format
                const m = fmt.match(/%[-0-9]*\.?(\d*)([feEgG])/);
                const prec = m && m[1] !== '' ? parseInt(m[1]) : 2;
                const spec = m ? m[2] : 'f';
                let numStr;
                if (spec === 'e' || spec === 'E') numStr = fval.toExponential(prec);
                else if (spec === 'g' || spec === 'G') numStr = fval.toPrecision(prec || 6);
                else numStr = fval.toFixed(prec);
                const result = fmt.replace(/%[-0-9]*\.?\d*[feEgG]/, numStr);
                this.writeStringToRef(dstRef, result);
                this.push(result.length);
                break;
            }
            case 59: { // SPRINTF_STR
                const srcRef = this.pop();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%s';
                const srcStr = this.readStringFromRef(srcRef);
                const result = fmt.replace(/%[-0-9]*s/, srcStr);
                this.writeStringToRef(dstRef, result);
                this.push(result.length);
                break;
            }

            // sprintf append variants — same formatting, but append to existing string
            case 70: { // SPRINTF_INT_CAT
                const val = this.pop();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%d';
                const existing = this.readStringFromRef(dstRef);
                let result;
                if (fmt.includes('%x') || fmt.includes('%X')) {
                    result = fmt.replace(/%[0-9]*[xX]/, (m) => {
                        const s = (val >>> 0).toString(16);
                        return m.includes('X') ? s.toUpperCase() : s;
                    });
                } else if (fmt.includes('%o')) {
                    result = fmt.replace(/%[0-9]*o/, () => (val >>> 0).toString(8));
                } else if (fmt.includes('%c')) {
                    result = fmt.replace(/%c/, () => String.fromCharCode(val & 0xFF));
                } else if (fmt.includes('%u')) {
                    result = fmt.replace(/%[0-9]*u/, () => (val >>> 0).toString());
                } else {
                    result = fmt.replace(/%[-0-9]*d/, () => val.toString());
                }
                this.writeStringToRef(dstRef, existing + result);
                this.push(existing.length + result.length);
                break;
            }
            case 71: { // SPRINTF_FLT_CAT
                const fval = this.popf();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%.2f';
                const existing = this.readStringFromRef(dstRef);
                const m = fmt.match(/%[-0-9]*\.?(\d*)([feEgG])/);
                const prec = m && m[1] !== '' ? parseInt(m[1]) : 2;
                const spec = m ? m[2] : 'f';
                let numStr;
                if (spec === 'e' || spec === 'E') numStr = fval.toExponential(prec);
                else if (spec === 'g' || spec === 'G') numStr = fval.toPrecision(prec || 6);
                else numStr = fval.toFixed(prec);
                const result = fmt.replace(/%[-0-9]*\.?\d*[feEgG]/, numStr);
                this.writeStringToRef(dstRef, existing + result);
                this.push(existing.length + result.length);
                break;
            }
            case 72: { // SPRINTF_STR_CAT
                const srcRef = this.pop();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%s';
                const existing = this.readStringFromRef(dstRef);
                const srcStr = this.readStringFromRef(srcRef);
                const result = fmt.replace(/%[-0-9]*s/, srcStr);
                this.writeStringToRef(dstRef, existing + result);
                this.push(existing.length + result.length);
                break;
            }
            case 239: { // SPRINTF_STR_CONST — like SPRINTF_STR but src is const pool string
                const srcIdx = this.pop();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%s';
                const srcStr = this.constants[srcIdx] || '';
                const result = fmt.replace(/%[-0-9]*s/, srcStr);
                this.writeStringToRef(dstRef, result);
                this.push(result.length);
                break;
            }
            case 247: { // SPRINTF_STR_CAT_CONST — like SPRINTF_STR_CAT but src is const pool string
                const srcIdx = this.pop();
                const fmtIdx = this.pop();
                const dstRef = this.pop();
                const fmt = this.constants[fmtIdx] || '%s';
                const existing = this.readStringFromRef(dstRef);
                const srcStr = this.constants[srcIdx] || '';
                const result = fmt.replace(/%[-0-9]*s/, srcStr);
                this.writeStringToRef(dstRef, existing + result);
                this.push(existing.length + result.length);
                break;
            }

            // File I/O (simulated virtual filesystem)
            case 60: { // FILE_OPEN
                const mode = this.pop();       // 0=read, 1=write, 2=append
                const nameIdx = this.pop();    // constant pool index for filename
                const name = this.constants[nameIdx] || '';
                const modeStr = mode === 0 ? 'READ' : mode === 1 ? 'WRITE' : 'APPEND';
                // Find free handle slot
                let slot = -1;
                for (let i = 0; i < this.fileHandles.length; i++) {
                    if (this.fileHandles[i] === null) { slot = i; break; }
                }
                if (slot === -1) {
                    this.onOutput(`[FILE] open("${name}", ${modeStr}) — no free handles\n`);
                    this.push(-1);
                    break;
                }
                if (mode === 0) {
                    // Read: file must exist
                    if (!this.virtualFS.has(name)) {
                        this.onOutput(`[FILE] open("${name}", READ) — file not found\n`);
                        this.push(-1);
                        break;
                    }
                    this.fileHandles[slot] = { name, data: new Uint8Array(this.virtualFS.get(name)), pos: 0, mode };
                } else if (mode === 1) {
                    // Write: create/truncate
                    this.fileHandles[slot] = { name, data: new Uint8Array(0), pos: 0, mode };
                } else {
                    // Append: create if needed, pos at end
                    const existing = this.virtualFS.get(name) || new Uint8Array(0);
                    this.fileHandles[slot] = { name, data: new Uint8Array(existing), pos: existing.length, mode };
                }
                this.onOutput(`[FILE] open("${name}", ${modeStr}) → handle ${slot}\n`);
                this.push(slot);
                break;
            }
            case 61: { // FILE_CLOSE
                const handle = this.pop();
                if (handle >= 0 && handle < this.fileHandles.length && this.fileHandles[handle]) {
                    const fh = this.fileHandles[handle];
                    // Flush write/append data to virtualFS
                    if (fh.mode === 1 || fh.mode === 2) {
                        this.virtualFS.set(fh.name, fh.data);
                    }
                    this.fileHandles[handle] = null;
                    this.onOutput(`[FILE] close(${handle})\n`);
                    this.push(0);
                } else {
                    this.push(-1);
                }
                break;
            }
            case 62: { // FILE_READ
                const maxBytes = this.pop();
                const bufRef = this.pop();
                const handle = this.pop();
                if (handle >= 0 && handle < this.fileHandles.length && this.fileHandles[handle]) {
                    const fh = this.fileHandles[handle];
                    const { arr, base } = this.resolveRef(bufRef);
                    const avail = fh.data.length - fh.pos;
                    const toRead = Math.min(maxBytes, avail, arr.length - base);
                    for (let i = 0; i < toRead; i++) {
                        arr[base + i] = fh.data[fh.pos + i];
                    }
                    fh.pos += toRead;
                    this.onOutput(`[FILE] read(${handle}, ${toRead} bytes)\n`);
                    this.push(toRead);
                } else {
                    this.push(-1);
                }
                break;
            }
            case 63: { // FILE_WRITE
                const len = this.pop();
                const bufRef = this.pop();
                const handle = this.pop();
                if (handle >= 0 && handle < this.fileHandles.length && this.fileHandles[handle]) {
                    const fh = this.fileHandles[handle];
                    const { arr, base } = this.resolveRef(bufRef);
                    const toWrite = Math.min(len, arr.length - base);
                    // Grow file data if needed
                    const needed = fh.pos + toWrite;
                    if (needed > fh.data.length) {
                        const newData = new Uint8Array(needed);
                        newData.set(fh.data);
                        fh.data = newData;
                    }
                    for (let i = 0; i < toWrite; i++) {
                        fh.data[fh.pos + i] = arr[base + i] & 0xFF;
                    }
                    fh.pos += toWrite;
                    this.onOutput(`[FILE] write(${handle}, ${toWrite} bytes)\n`);
                    this.push(toWrite);
                } else {
                    this.push(-1);
                }
                break;
            }
            case 64: { // FILE_EXISTS
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                const exists = this.virtualFS.has(name) ? 1 : 0;
                this.onOutput(`[FILE] exists("${name}") → ${exists}\n`);
                this.push(exists);
                break;
            }
            case 65: { // FILE_DELETE
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                if (this.virtualFS.has(name)) {
                    this.virtualFS.delete(name);
                    this.onOutput(`[FILE] delete("${name}") → OK\n`);
                    this.push(0);
                } else {
                    this.onOutput(`[FILE] delete("${name}") → not found\n`);
                    this.push(-1);
                }
                break;
            }

            // ── _REF variants: path from char array (dynamic filenames) ──
            case 224: { // FILE_OPEN_REF
                const mode = this.pop();
                const pathRef = this.pop();
                const name = this.readStringFromRef(pathRef);
                const modeStr = mode === 0 ? 'READ' : mode === 1 ? 'WRITE' : 'APPEND';
                let slot = -1;
                for (let i = 0; i < this.fileHandles.length; i++) {
                    if (this.fileHandles[i] === null) { slot = i; break; }
                }
                if (slot === -1) {
                    this.onOutput(`[FILE] open("${name}", ${modeStr}) — no free handles\n`);
                    this.push(-1);
                    break;
                }
                if (mode === 0) {
                    if (!this.virtualFS.has(name)) {
                        this.onOutput(`[FILE] open("${name}", READ) — file not found\n`);
                        this.push(-1);
                        break;
                    }
                    this.fileHandles[slot] = { name, data: new Uint8Array(this.virtualFS.get(name)), pos: 0, mode };
                } else if (mode === 1) {
                    this.fileHandles[slot] = { name, data: new Uint8Array(0), pos: 0, mode };
                } else {
                    const existing = this.virtualFS.get(name) || new Uint8Array(0);
                    this.fileHandles[slot] = { name, data: new Uint8Array(existing), pos: existing.length, mode };
                }
                this.onOutput(`[FILE] open("${name}", ${modeStr}) → handle ${slot}\n`);
                this.push(slot);
                break;
            }
            case 225: { // FILE_EXISTS_REF
                const pathRef = this.pop();
                const name = this.readStringFromRef(pathRef);
                const exists = this.virtualFS.has(name) ? 1 : 0;
                this.onOutput(`[FILE] exists("${name}") → ${exists}\n`);
                this.push(exists);
                break;
            }
            case 226: { // FILE_DELETE_REF
                const pathRef = this.pop();
                const name = this.readStringFromRef(pathRef);
                if (this.virtualFS.has(name)) {
                    this.virtualFS.delete(name);
                    this.onOutput(`[FILE] delete("${name}") → OK\n`);
                    this.push(0);
                } else {
                    this.onOutput(`[FILE] delete("${name}") → not found\n`);
                    this.push(-1);
                }
                break;
            }

            // ── Directory listing (simulated) ──
            case 227: { // FILE_OPENDIR (const path)
                const nameIdx = this.pop();
                const dirPath = this.constants[nameIdx] || '/';
                let slot = -1;
                for (let i = 0; i < this.fileHandles.length; i++) {
                    if (this.fileHandles[i] === null) { slot = i; break; }
                }
                if (slot === -1) {
                    this.onOutput(`[FILE] openDir("${dirPath}") — no free handles\n`);
                    this.push(-1);
                    break;
                }
                // Collect filenames matching this directory prefix
                const prefix = dirPath.endsWith('/') ? dirPath : dirPath + '/';
                const entries = [];
                for (const key of this.virtualFS.keys()) {
                    if (key.startsWith(prefix)) {
                        const rest = key.substring(prefix.length);
                        if (!rest.includes('/')) entries.push(rest);  // direct children only
                    }
                }
                this.fileHandles[slot] = { name: dirPath, dirEntries: entries, dirPos: 0, mode: -1 };
                this.onOutput(`[FILE] openDir("${dirPath}") → handle ${slot} (${entries.length} entries)\n`);
                this.push(slot);
                break;
            }
            case 228: { // FILE_OPENDIR_REF (char array path)
                const pathRef = this.pop();
                const dirPath = this.readStringFromRef(pathRef);
                let slot = -1;
                for (let i = 0; i < this.fileHandles.length; i++) {
                    if (this.fileHandles[i] === null) { slot = i; break; }
                }
                if (slot === -1) {
                    this.onOutput(`[FILE] openDir("${dirPath}") — no free handles\n`);
                    this.push(-1);
                    break;
                }
                const prefix = dirPath.endsWith('/') ? dirPath : dirPath + '/';
                const entries = [];
                for (const key of this.virtualFS.keys()) {
                    if (key.startsWith(prefix)) {
                        const rest = key.substring(prefix.length);
                        if (!rest.includes('/')) entries.push(rest);
                    }
                }
                this.fileHandles[slot] = { name: dirPath, dirEntries: entries, dirPos: 0, mode: -1 };
                this.onOutput(`[FILE] openDir("${dirPath}") → handle ${slot} (${entries.length} entries)\n`);
                this.push(slot);
                break;
            }
            case 146: { // FS_INFO
                const sel = this.pop();
                // Simulate: 0=total kB, 1=free kB
                this.push(sel ? 4096 : 8192); // simulated 8MB total, 4MB free
                this.onOutput(`[FS] fsInfo(${sel}) → ${sel ? 4096 : 8192} kB\n`);
                break;
            }

            case 229: { // FILE_READDIR
                const nameRef = this.pop();
                const handle = this.pop();
                if (handle < 0 || handle >= this.fileHandles.length || !this.fileHandles[handle] ||
                    !this.fileHandles[handle].dirEntries) {
                    this.push(0);
                    break;
                }
                const fh = this.fileHandles[handle];
                if (fh.dirPos >= fh.dirEntries.length) {
                    this.onOutput(`[FILE] readDir(${handle}) → end\n`);
                    this.push(0);
                    break;
                }
                const entryName = fh.dirEntries[fh.dirPos++];
                this.writeStringToRef(nameRef, entryName);
                this.onOutput(`[FILE] readDir(${handle}) → "${entryName}"\n`);
                this.push(1);
                break;
            }

            case 260: { // FILE_RANGE — get first/last timestamps from log file
                const maxRef = this.pop();
                const minRef = this.pop();
                const handle = this.pop();
                if (handle < 0 || handle >= this.fileHandles.length || !this.fileHandles[handle]) {
                    this.push(0);
                    break;
                }
                const fh = this.fileHandles[handle];
                const content = typeof fh.content === 'string' ? fh.content : '';
                const lines = content.split('\n').filter(l => l.trim().length > 0);
                let rows = 0;
                let firstTs = '', lastTs = '';
                for (let i = 1; i < lines.length; i++) { // skip header
                    const tab = lines[i].indexOf('\t');
                    const ts = tab > 0 ? lines[i].substring(0, tab) : lines[i];
                    if (ts.length > 0) {
                        if (firstTs === '') firstTs = ts;
                        lastTs = ts;
                        rows++;
                    }
                }
                this.writeStringToRef(minRef, firstTs);
                this.writeStringToRef(maxRef, lastTs);
                this.onOutput(`[FILE] range(${handle}) → "${firstTs}" to "${lastTs}" (${rows} rows)\n`);
                this.push(rows);
                break;
            }

            case 66: { // FILE_SIZE
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                if (this.virtualFS.has(name)) {
                    const size = this.virtualFS.get(name).length;
                    this.onOutput(`[FILE] size("${name}") → ${size}\n`);
                    this.push(size);
                } else {
                    this.push(-1);
                }
                break;
            }

            case 67: { // FILE_FORMAT
                this.onOutput('[FILE] format filesystem (mock)\n');
                this.virtualFS.clear();
                this.push(0);
                break;
            }
            case 68: { // FILE_MKDIR
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                this.onOutput(`[FILE] mkdir("${name}") (mock)\n`);
                this.push(1);
                break;
            }
            case 69: { // FILE_RMDIR
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                this.onOutput(`[FILE] rmdir("${name}") (mock)\n`);
                this.push(1);
                break;
            }
            case 73: { // FILE_SEEK
                const whence = this.pop();   // 0=SET, 1=CUR, 2=END
                const offset = this.pop();
                const handle = this.pop();
                if (handle >= 0 && handle < this.fileHandles.length && this.fileHandles[handle]) {
                    const fh = this.fileHandles[handle];
                    if (whence === 0)      fh.pos = offset;
                    else if (whence === 1)  fh.pos += offset;
                    else if (whence === 2)  fh.pos = fh.data.length + offset;
                    if (fh.pos < 0) fh.pos = 0;
                    this.onOutput(`[FILE] seek(${handle}, ${offset}, ${whence}) → pos=${fh.pos}\n`);
                    this.push(1);
                } else {
                    this.push(0);
                }
                break;
            }
            case 79: { // FILE_TELL
                const handle = this.pop();
                if (handle >= 0 && handle < this.fileHandles.length && this.fileHandles[handle]) {
                    const pos = this.fileHandles[handle].pos;
                    this.onOutput(`[FILE] tell(${handle}) → ${pos}\n`);
                    this.push(pos);
                } else {
                    this.push(-1);
                }
                break;
            }

            case 220: { // FILE_DOWNLOAD — download URL to file
                const urlRef = this.pop();
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                const url = this.readStringFromRef(urlRef);
                this.onOutput(`[FILE] fileDownload("${name}", "${url}") → 200 (mock)\n`);
                this.push(200); // mock success
                break;
            }

            case 221: { // FILE_GET_STR — search file for Nth delimiter, extract string
                const endChar = this.pop();
                const index = this.pop();
                const delimIdx = this.pop();
                const handle = this.pop();
                const dstRef = this.pop();
                const delim = this.constants[delimIdx] || ',';
                // Mock: search virtualFS file for Nth delimiter occurrence
                const fh = this.openFiles ? this.openFiles.get(handle) : null;
                let result = '';
                if (fh) {
                    const text = new TextDecoder().decode(fh.data);
                    // Find Nth occurrence of delim
                    let pos = 0;
                    let count = index < 1 ? 1 : index;
                    for (let i = 0; i < count; i++) {
                        const found = text.indexOf(delim, pos);
                        if (found < 0) { pos = -1; break; }
                        pos = found + delim.length;
                    }
                    if (pos >= 0 && pos < text.length) {
                        // Extract until endChar or EOF
                        let end = text.length;
                        if (endChar > 0) {
                            const ec = text.indexOf(String.fromCharCode(endChar), pos);
                            if (ec >= 0) end = ec;
                        }
                        result = text.substring(pos, end);
                    }
                }
                this.writeStringToRef(dstRef, result);
                this.onOutput(`[FILE] fileGetStr(h=${handle}, "${delim}", ${index}, ${endChar}) → "${result}" (${result.length})\n`);
                this.push(result.length);
                break;
            }

            case 222:   // FILE_EXTRACT
            case 223: { // FILE_EXTRACT_FAST — extract time range from CSV into float arrays
                const numArrays = this.pop();
                const arrRefs = [];
                for (let i = numArrays - 1; i >= 0; i--) arrRefs[i] = this.pop();
                const accum   = this.pop();
                const colOffs = this.pop();
                const toRef   = this.pop();
                const fromRef = this.pop();
                const handle  = this.pop();
                const tsFrom = this.readStringFromRef(fromRef);
                const tsTo   = this.readStringFromRef(toRef);
                // Mock: parse virtualFS file, extract matching rows
                const fh = this.openFiles ? this.openFiles.get(handle) : null;
                let rowCount = 0;
                if (fh) {
                    const text = new TextDecoder().decode(fh.data);
                    const lines = text.split('\n');
                    for (const line of lines) {
                        if (!line.trim()) continue;
                        const cols = line.split('\t');
                        if (cols.length < 2) continue;
                        const ts = cols[0];
                        // Skip header / non-timestamp lines
                        if (!/\d/.test(ts.charAt(0))) continue;
                        // Simple string compare (works for ISO, approximate for German)
                        if (ts < tsFrom) continue;
                        if (ts > tsTo) break;
                        // Parse float values into arrays
                        for (let c = 0; c < numArrays; c++) {
                            const ci = 1 + colOffs + c;  // skip timestamp col
                            const val = ci < cols.length ? parseFloat(cols[ci]) : 0;
                            const base = this.resolveRef(arrRefs[c]);
                            if (base) {
                                const fBuf = new Float32Array(1);
                                if (accum) {
                                    fBuf[0] = base.arr[base.offset + rowCount] || 0;
                                    const dv = new DataView(fBuf.buffer);
                                    const existing = dv.getFloat32(0, true);
                                    fBuf[0] = 0;
                                    dv.setFloat32(0, existing + val, true);
                                    base.arr[base.offset + rowCount] = dv.getInt32(0, true);
                                } else {
                                    const dv = new DataView(fBuf.buffer);
                                    dv.setFloat32(0, val, true);
                                    base.arr[base.offset + rowCount] = dv.getInt32(0, true);
                                }
                            }
                        }
                        rowCount++;
                    }
                }
                const name = syscallId === 222 ? 'fileExtract' : 'fileExtractFast';
                this.onOutput(`[FILE] ${name}(h=${handle}, "${tsFrom}"→"${tsTo}", offs=${colOffs}, accum=${accum}, ${numArrays} arrays) → ${rowCount} rows\n`);
                this.push(rowCount);
                break;
            }

            // Heap (80-81) handled elsewhere

            case 82: { // FILE_READ_ARR — read tab-delimited line into int array
                const handle = this.pop();
                const arrRef = this.pop();
                let count = 0;
                const fh = this.fileHandles[handle];
                if (fh && fh.data) {
                    // Read one line from file position
                    let line = '';
                    while (fh.pos < fh.data.length) {
                        const ch = fh.data[fh.pos++];
                        if (ch === 10) break; // newline
                        if (ch !== 13) line += String.fromCharCode(ch);
                    }
                    if (line.length > 0) {
                        const tokens = line.split(/[\t,]/);
                        const base = this.resolveRef(arrRef);
                        for (let i = 0; i < tokens.length; i++) {
                            const val = parseInt(tokens[i], 10);
                            if (!isNaN(val)) {
                                this.globals[base + i] = val;
                                count++;
                            }
                        }
                    }
                }
                this.push(count);
                break;
            }
            case 83: { // FILE_WRITE_ARR — write int array as tab-delimited line
                const append = this.pop();
                const handle = this.pop();
                const arrRef = this.pop();
                const fh = this.fileHandles[handle];
                if (fh) {
                    const base = this.resolveRef(arrRef);
                    // Determine array length from ref info
                    let len = 0;
                    const ref_type = (arrRef >> 24) & 0xFF;
                    if (ref_type === 0) { // global
                        for (const [, g] of this.globalInfo) {
                            if (g.isArray && g.index === (arrRef & 0xFFFFFF)) { len = g.arraySize; break; }
                        }
                    }
                    if (len === 0) len = 16; // fallback
                    let line = '';
                    for (let i = 0; i < len; i++) {
                        if (i > 0) line += '\t';
                        line += String(this.globals[base + i] || 0);
                    }
                    if (!append) line += '\n';
                    // Append to file data
                    const bytes = new TextEncoder().encode(line);
                    const newData = new Uint8Array(fh.data.length + bytes.length);
                    newData.set(fh.data);
                    newData.set(bytes, fh.data.length);
                    fh.data = newData;
                    this.onOutput(`[FILE] writeArray handle=${handle}, ${len} elements\n`);
                }
                break;
            }
            case 84: { // FILE_LOG — append string + newline, rotate if over limit
                const limit = this.pop();
                const strRef = this.pop();
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '';
                const payload = this.readStringFromRef(strRef);
                let data = this.virtualFS.get(name) || new Uint8Array(0);
                // Append payload + newline
                const line = new TextEncoder().encode(payload + '\n');
                const newData = new Uint8Array(data.length + line.length);
                newData.set(data);
                newData.set(line, data.length);
                data = newData;
                // Rotate if over limit
                if (limit > 0 && data.length > limit) {
                    const idx = data.indexOf(10); // find first newline
                    if (idx >= 0) data = data.slice(idx + 1);
                }
                this.virtualFS.set(name, data);
                this.onOutput(`[FILE] log("${name}") size=${data.length}\n`);
                this.push(data.length);
                break;
            }

            // Time / timestamp functions (browser mock)
            case 85: { // TIME_STAMP — get current timestamp into char[]
                const bufRef = this.pop();
                const now = new Date();
                const ts = now.getFullYear() + '-' +
                    String(now.getMonth() + 1).padStart(2, '0') + '-' +
                    String(now.getDate()).padStart(2, '0') + 'T' +
                    String(now.getHours()).padStart(2, '0') + ':' +
                    String(now.getMinutes()).padStart(2, '0') + ':' +
                    String(now.getSeconds()).padStart(2, '0');
                this.writeStringToRef(bufRef, ts);
                this.onOutput(`[TIME] timeStamp() → "${ts}"\n`);
                this.push(0);
                break;
            }
            case 86: { // TIME_CONVERT — convert timestamp format
                const flg = this.pop();
                const bufRef = this.pop();
                const src = this.readStringFromRef(bufRef);
                let dst = src;
                if (flg === 1) {
                    // Web → German: "2024-01-15T12:30:45" → "15.1.24 12:30"
                    const m = src.match(/(\d{4})-(\d+)-(\d+)T(\d+):(\d+):(\d+)/);
                    if (m) {
                        dst = `${+m[3]}.${+m[2]}.${+m[1] % 100} ${+m[4]}:${m[5]}`;
                    }
                } else {
                    // German → Web: "15.1.24 12:30" → "2024-01-15T12:30:00"
                    const m = src.match(/(\d+)\.(\d+)\.(\d+)\s+(\d+):(\d+)/);
                    if (m) {
                        const y = +m[3] < 100 ? 2000 + +m[3] : +m[3];
                        dst = `${y}-${String(+m[2]).padStart(2,'0')}-${String(+m[1]).padStart(2,'0')}T${String(+m[4]).padStart(2,'0')}:${m[5]}:00`;
                    }
                }
                this.writeStringToRef(bufRef, dst);
                this.onOutput(`[TIME] timeConvert("${src}", ${flg}) → "${dst}"\n`);
                this.push(0);
                break;
            }
            case 87: { // TIME_OFFSET — add day offset to timestamp
                const zflag = this.pop();
                const days = this.pop();
                const bufRef = this.pop();
                const src = this.readStringFromRef(bufRef);
                const m = src.match(/(\d{4})-(\d+)-(\d+)T(\d+):(\d+):(\d+)/);
                let dst = src;
                if (m) {
                    const d = new Date(+m[1], +m[2]-1, +m[3], +m[4], +m[5], +m[6]);
                    d.setDate(d.getDate() + days);
                    const hh = zflag ? '00' : String(d.getHours()).padStart(2,'0');
                    const mm = zflag ? '00' : String(d.getMinutes()).padStart(2,'0');
                    const ss = zflag ? '00' : String(d.getSeconds()).padStart(2,'0');
                    dst = d.getFullYear() + '-' +
                        String(d.getMonth()+1).padStart(2,'0') + '-' +
                        String(d.getDate()).padStart(2,'0') + 'T' + hh + ':' + mm + ':' + ss;
                }
                this.writeStringToRef(bufRef, dst);
                this.onOutput(`[TIME] timeOffset("${src}", ${days}, ${zflag}) → "${dst}"\n`);
                this.push(0);
                break;
            }
            case 88: { // TIME_TO_SECS — timestamp to epoch seconds
                const bufRef = this.pop();
                const src = this.readStringFromRef(bufRef);
                let secs = 0;
                const m = src.match(/(\d{4})-(\d+)-(\d+)T(\d+):(\d+):(\d+)/);
                if (m) {
                    const d = new Date(+m[1], +m[2]-1, +m[3], +m[4], +m[5], +m[6]);
                    secs = Math.floor(d.getTime() / 1000);
                }
                this.onOutput(`[TIME] timeToSecs("${src}") → ${secs}\n`);
                this.push(secs);
                break;
            }
            case 89: { // SECS_TO_TIME — epoch seconds to timestamp
                const secs = this.pop();
                const bufRef = this.pop();
                const d = new Date(secs * 1000);
                const ts = d.getFullYear() + '-' +
                    String(d.getMonth()+1).padStart(2,'0') + '-' +
                    String(d.getDate()).padStart(2,'0') + 'T' +
                    String(d.getHours()).padStart(2,'0') + ':' +
                    String(d.getMinutes()).padStart(2,'0') + ':' +
                    String(d.getSeconds()).padStart(2,'0');
                this.writeStringToRef(bufRef, ts);
                this.onOutput(`[TIME] secsToTime(${secs}) → "${ts}"\n`);
                this.push(0);
                break;
            }

            // Tasmota command (simulated in browser)
            case 43: { // TASM_CMD
                const bufRef = this.pop();     // output buffer array ref
                const cmdIdx = this.pop();     // const pool index for command
                const cmd = this.constants[cmdIdx] || '';
                const mockResponse = `{"TasmCmd":"${cmd} (simulated)"}`;
                this.writeStringToRef(bufRef, mockResponse);
                this.onOutput(`[TASM] cmd("${cmd}") → ${mockResponse.length} bytes\n`);
                this.push(mockResponse.length);
                break;
            }

            case 248: { // TASM_CMD_REF — tasmCmd with char array command
                const bufRef = this.pop();     // output buffer array ref
                const cmdRef = this.pop();     // command char array ref
                const cmd = this.readStringFromRef(cmdRef);
                const mockResponse = `{"TasmCmd":"${cmd} (simulated)"}`;
                this.writeStringToRef(bufRef, mockResponse);
                this.onOutput(`[TASM] cmd("${cmd}") → ${mockResponse.length} bytes\n`);
                this.push(mockResponse.length);
                break;
            }

            case 250: { // DEBUG_PRINT
                const val = this.pop();
                this.onOutput(`${val}\n`);
                break;
            }
            case 251: { // DEBUG_PRINT_STR
                const idx = this.pop();
                const str = this.constants[idx] || `[const:${idx}]`;
                this.onOutput(`${str}\n`);
                break;
            }
            case 252: { // DEBUG_DUMP
                this.dumpState();
                break;
            }

            case 80: { // HEAP_ALLOC
                const size = this.pop();
                if (!this.heapData) {
                    this.heapData = new Int32Array(MAX_HEAP);
                    this.heapFloat = new Float32Array(this.heapData.buffer);
                }
                // Find free handle slot
                let handle = -1;
                for (let i = 0; i < MAX_HEAP_HANDLES; i++) {
                    if (!this.heapHandles[i] || !this.heapHandles[i].alive) {
                        handle = i; break;
                    }
                }
                if (handle < 0 || this.heapUsed + size > MAX_HEAP) {
                    this.push(-1);
                    break;
                }
                while (this.heapHandles.length <= handle) this.heapHandles.push(null);
                this.heapHandles[handle] = { offset: this.heapUsed, size, alive: true };
                this.heapUsed += size;
                this.push(handle);
                break;
            }
            case 81: { // HEAP_FREE
                const handle = this.pop();
                if (handle >= 0 && handle < this.heapHandles.length && this.heapHandles[handle]) {
                    this.heapHandles[handle].alive = false;
                }
                break;
            }

            // Tasmota output (simulated in browser — route to onOutput)
            case 90: { // RESPONSE_APPEND
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(str);
                break;
            }
            case 91: { // WEB_SEND
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(str);
                break;
            }
            case 92: { // WEB_FLUSH — no-op in browser (no chunked transfer)
                break;
            }
            case 93: { // RESPONSE_APPEND_STR — string literal variant
                const idx = this.pop();
                if (idx >= 0 && idx < this.constants.length) {
                    this.onOutput(this.constants[idx]);
                }
                break;
            }
            case 94: { // WEB_SEND_STR — string literal variant
                const idx = this.pop();
                if (idx >= 0 && idx < this.constants.length) {
                    this.onOutput(this.constants[idx]);
                }
                break;
            }

            case 95: { // LOG — AddLog (in browser, just output with [LOG] prefix)
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(`[LOG] ${str}\n`);
                break;
            }
            case 96: { // LOG_STR — AddLog string literal
                const idx = this.pop();
                if (idx >= 0 && idx < this.constants.length) {
                    this.onOutput(`[LOG] ${this.constants[idx]}\n`);
                }
                break;
            }

            case 97: { // LGETSTRING — get localized string by index
                const dstRef = this.pop();    // dst char array ref
                const strIdx = this.pop();    // string index
                const lstrings = [
                    "Temperature", "Humidity", "Pressure", "Dew point",
                    "Carbon dioxide", "eCO₂", "TVOC", "Voltage",
                    "Current", "Power", "Power Factor",
                    "Energy Today", "Energy Yesterday", "Energy Total",
                    "Frequency", "Illuminance", "Distance", "Moisture",
                    "Light", "Speed", "Abs Humidity"
                ];
                if (strIdx >= 0 && strIdx < lstrings.length) {
                    const str = lstrings[strIdx];
                    this.writeString(dstRef, str);
                    this.push(str.length);
                } else {
                    this.push(0);
                }
                break;
            }

            // ── UDP multicast (browser simulation) ──────────
            case 100: { // UDP_SEND — send binary float via multicast =>name:[4 bytes]
                // Stack: [nameIdx, floatBits] — floatBits on top
                const fval = this.pop();        // float as int32 bits
                const nameIdx = this.pop();     // const pool index for var name
                if (nameIdx >= 0 && nameIdx < this.constants.length) {
                    const name = this.constants[nameIdx];
                    // Show float value in output for browser simulation
                    const tmpArr = new Int32Array(1);
                    tmpArr[0] = fval;
                    const fv = new Float32Array(tmpArr.buffer)[0];
                    this.onOutput(`[UDP TX] =>${name}:${fv}\n`);
                    // Store locally (loopback simulation)
                    this.udpVars.set(name, { value: fval, ready: true });
                    // Auto-update global entries (V5)
                    this.updateUdpGlobal(name, fv);
                }
                break;
            }
            case 101: { // UDP_RECV — get last received float for name
                const nameIdx = this.pop();
                if (nameIdx >= 0 && nameIdx < this.constants.length) {
                    const name = this.constants[nameIdx];
                    const entry = this.udpVars.get(name);
                    this.push(entry ? entry.value : 0);
                } else {
                    this.push(0);
                }
                break;
            }
            case 102: { // UDP_READY — check if value received since last read
                const nameIdx = this.pop();
                if (nameIdx >= 0 && nameIdx < this.constants.length) {
                    const name = this.constants[nameIdx];
                    const entry = this.udpVars.get(name);
                    if (entry && entry.ready) {
                        entry.ready = false;
                        this.push(1);
                    } else {
                        this.push(0);
                    }
                } else {
                    this.push(0);
                }
                break;
            }

            // ── UDP array send/receive (browser simulation) ──
            case 103: { // UDP_SEND_ARRAY — send float array via multicast
                // Stack: [nameIdx, arrRef, count] — count on top
                const count = this.pop();
                const arrRef = this.pop();
                const nameIdx = this.pop();
                if (nameIdx >= 0 && nameIdx < this.constants.length) {
                    const name = this.constants[nameIdx];
                    const resolved = this.resolveRef(arrRef);
                    const { arr, base } = resolved;
                    const maxLen = resolved.maxLen || (arr.length - base);
                    const n = Math.min(count, maxLen);
                    const floats = [];
                    for (let i = 0; i < n; i++) {
                        const tmpArr = new Int32Array(1);
                        tmpArr[0] = arr[base + i];
                        const fv = new Float32Array(tmpArr.buffer)[0];
                        floats.push(fv.toFixed(2));
                    }
                    this.onOutput(`[UDP TX] =>${name}:[${n} floats: ${floats.join(', ')}]\n`);
                    // Store locally (loopback simulation) — store as int32 array
                    const arrData = new Int32Array(n);
                    for (let i = 0; i < n; i++) {
                        arrData[i] = arr[base + i];
                    }
                    this.udpVars.set(name, { value: 0, ready: true, arrayData: arrData });
                }
                break;
            }
            case 104: { // UDP_RECV_ARRAY — receive float array from UDP
                // Stack: [nameIdx, arrRef, maxcount] — maxcount on top
                const maxcount = this.pop();
                const arrRef = this.pop();
                const nameIdx = this.pop();
                if (nameIdx >= 0 && nameIdx < this.constants.length) {
                    const name = this.constants[nameIdx];
                    const entry = this.udpVars.get(name);
                    if (entry && entry.arrayData) {
                        const resolved = this.resolveRef(arrRef);
                        const { arr, base } = resolved;
                        const maxLen = resolved.maxLen || (arr.length - base);
                        const n = Math.min(maxcount, entry.arrayData.length, maxLen);
                        for (let i = 0; i < n; i++) {
                            arr[base + i] = entry.arrayData[i];
                        }
                        this.push(n);
                    } else {
                        this.push(0);
                    }
                } else {
                    this.push(0);
                }
                break;
            }

            case 149: { // UDP_SEND_STR — send string via ASCII multicast
                // Stack: [nameIdx, strRef] — strRef on top
                const strRef = this.pop();
                const nameIdx = this.pop();
                if (nameIdx >= 0 && nameIdx < this.constants.length) {
                    const name = this.constants[nameIdx];
                    const resolved = this.resolveRef(strRef);
                    const { arr, base } = resolved;
                    const maxLen = resolved.maxLen || (arr.length - base);
                    let str = '';
                    for (let i = 0; i < maxLen; i++) {
                        if (arr[base + i] === 0) break;
                        str += String.fromCharCode(arr[base + i] & 0xFF);
                    }
                    this.onOutput(`[UDP] Send =>` + name + `=` + str + `\n`);
                }
                break;
            }

            // ── General-purpose UDP (browser simulation) ─────────────
            case 126: { // UDP_FUNC — mode-based dispatcher
                const mode = this.pop();
                switch (mode) {
                    case 0: { // udp(0, port) → open port
                        const port = this.pop();
                        this.udpPortNum = port;
                        this.udpPortOpen = true;
                        this.onOutput(`[UDP] Open port ${port}\n`);
                        this.push(1);
                        break;
                    }
                    case 1: { // udp(1, buf) → read string
                        const bufRef = this.pop();
                        if (this.udpPortOpen && this.udpInbox) {
                            this.writeStringToRef(bufRef, this.udpInbox);
                            const len = this.udpInbox.length;
                            this.udpInbox = null;
                            this.push(len);
                        } else {
                            this.push(0);
                        }
                        break;
                    }
                    case 2: { // udp(2, str) → reply to sender
                        const strRef = this.pop();
                        const str = this.readStringFromRef(strRef);
                        this.onOutput(`[UDP] Reply → "${str}"\n`);
                        break;
                    }
                    case 3: { // udp(3, url, str) → send to url with stored port
                        const strRef = this.pop();
                        const urlRef = this.pop();
                        const url = this.readStringFromRef(urlRef);
                        const str = this.readStringFromRef(strRef);
                        this.onOutput(`[UDP] Send to ${url}:${this.udpPortNum || 0} → "${str}"\n`);
                        break;
                    }
                    case 4: { // udp(4, buf) → get remote IP
                        const bufRef = this.pop();
                        const ip = this.udpRemoteIp || '0.0.0.0';
                        this.writeStringToRef(bufRef, ip);
                        this.push(ip.length);
                        break;
                    }
                    case 5: { // udp(5) → get remote port
                        this.push(this.udpRemotePort || 0);
                        break;
                    }
                    case 6: { // udp(6, url, port, str) → send to url:port
                        const strRef = this.pop();
                        const port = this.pop();
                        const urlRef = this.pop();
                        const url = this.readStringFromRef(urlRef);
                        const str = this.readStringFromRef(strRef);
                        this.onOutput(`[UDP] Send to ${url}:${port} → "${str}"\n`);
                        this.push(0);
                        break;
                    }
                    case 7: { // udp(7, url, port, arr, count) → send array to url:port
                        const count = this.pop();
                        const arrRef = this.pop();
                        const port = this.pop();
                        const urlRef = this.pop();
                        const url = this.readStringFromRef(urlRef);
                        const resolved = this.resolveRef(arrRef);
                        const n = Math.min(count, resolved.maxLen || 256);
                        const bytes = [];
                        for (let i = 0; i < n; i++) {
                            bytes.push(resolved.arr[resolved.base + i] & 0xFF);
                        }
                        this.onOutput(`[UDP] Send ${n} bytes to ${url}:${port} → [${bytes.slice(0, 16).join(',')}${n > 16 ? '...' : ''}]\n`);
                        this.push(0);
                        break;
                    }
                    default:
                        this.onOutput(`[UDP] Unknown mode ${mode}\n`);
                        this.push(0);
                }
                break;
            }

            // ── I2C bus — browser simulation ─────────────
            case 105: { // I2C_READ8(addr, reg, bus) -> int
                const bus = this.pop();
                const reg = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] Read8 addr=0x${(addr & 0xFF).toString(16)} reg=0x${(reg & 0xFF).toString(16)} → 0\n`);
                this.push(0);  // simulated: return 0
                break;
            }
            case 106: { // I2C_WRITE8(addr, reg, val, bus) -> int
                const bus = this.pop();
                const val = this.pop();
                const reg = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] Write8 addr=0x${(addr & 0xFF).toString(16)} reg=0x${(reg & 0xFF).toString(16)} val=0x${(val & 0xFF).toString(16)}\n`);
                this.push(1);  // simulated: success
                break;
            }
            case 107: { // I2C_READ_BUF(addr, reg, buf, len, bus) -> int
                const bus = this.pop();
                const len = this.pop();
                const bufRef = this.pop();
                const reg = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] ReadBuf addr=0x${(addr & 0xFF).toString(16)} reg=0x${(reg & 0xFF).toString(16)} len=${len}\n`);
                // Fill buffer with zeros (simulated)
                const resolved = this.resolveRef(bufRef);
                if (resolved) {
                    const { arr, base } = resolved;
                    const maxLen = resolved.maxLen || (arr.length - base);
                    const n = Math.min(len, maxLen);
                    for (let i = 0; i < n; i++) { arr[base + i] = 0; }
                }
                this.push(1);  // simulated: success
                break;
            }
            case 129: { // I2C_READ_RS(addr, reg, buf, len, bus) -> int (repeated START)
                const bus = this.pop();
                const len = this.pop();
                const bufRef = this.pop();
                const reg = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] ReadRS addr=0x${(addr & 0xFF).toString(16)} reg=0x${(reg & 0xFF).toString(16)} len=${len}\n`);
                const resolved = this.resolveRef(bufRef);
                if (resolved) {
                    const { arr, base } = resolved;
                    const maxLen = resolved.maxLen || (arr.length - base);
                    const n = Math.min(len, maxLen);
                    for (let i = 0; i < n; i++) { arr[base + i] = 0; }
                }
                this.push(1);  // simulated: success
                break;
            }
            case 108: { // I2C_WRITE_BUF(addr, reg, buf, len, bus) -> int
                const bus = this.pop();
                const len = this.pop();
                const bufRef = this.pop();
                const reg = this.pop();
                const addr = this.pop();
                // Read buffer values for debug output
                const resolved = this.resolveRef(bufRef);
                let hexStr = '';
                if (resolved) {
                    const { arr, base } = resolved;
                    const n = Math.min(len, 16); // show up to 16 bytes
                    for (let i = 0; i < n; i++) { hexStr += (arr[base + i] & 0xFF).toString(16).padStart(2, '0') + ' '; }
                    if (len > 16) hexStr += '...';
                }
                this.onOutput(`[I2C${bus}] WriteBuf addr=0x${(addr & 0xFF).toString(16)} reg=0x${(reg & 0xFF).toString(16)} len=${len} [${hexStr.trim()}]\n`);
                this.push(1);  // simulated: success
                break;
            }
            case 109: { // I2C_EXISTS(addr, bus) -> int
                const bus = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] Exists addr=0x${(addr & 0xFF).toString(16)} → 1 (simulated)\n`);
                this.push(1);  // simulated: always exists
                break;
            }
            case 127: { // I2C_SET_DEVICE(addr, bus) -> int — check unclaimed & responsive
                const bus = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] SetDevice addr=0x${(addr & 0xFF).toString(16)} → 1 (simulated)\n`);
                this.push(1);  // simulated: always available
                break;
            }
            case 128: { // I2C_SET_FOUND(addr, const_type, bus) -> void — register claimed
                const bus = this.pop();
                const ci = this.pop();
                const addr = this.pop();
                const type = this.getConstant(ci) || '?';
                this.onOutput(`[I2C${bus}] SetActiveFound addr=0x${(addr & 0xFF).toString(16)} type="${type}"\n`);
                break;
            }
            case 112: { // I2C_READ_BUF0(addr, buf_ref, len, bus) -> int  (no register)
                const bus = this.pop();
                const len = this.pop();
                const bufRef = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] Read0 addr=0x${(addr & 0xFF).toString(16)} len=${len} (no reg)\n`);
                // Resolve ref and fill with simulated data
                const ref = this.resolveRef(bufRef);
                if (ref) {
                    for (let i = 0; i < len && i < ref.length; i++) { ref.arr[ref.base + i] = 0; }
                }
                this.push(1);
                break;
            }
            case 113: { // I2C_WRITE0(addr, reg, bus) -> int  (register only, no data)
                const bus = this.pop();
                const reg = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] Write0 addr=0x${(addr & 0xFF).toString(16)} reg=0x${(reg & 0xFF).toString(16)}\n`);
                this.push(1);
                break;
            }

            case 249: { // I2cResetActive(addr, bus) -> void — release claimed I2C address
                const bus = this.pop();
                const addr = this.pop();
                this.onOutput(`[I2C${bus}] I2cResetActive addr=0x${(addr & 0xFF).toString(16)}\n`);
                break;
            }

            // ── Console command callback ──────────────
            case 45: { // ADD_COMMAND(const_idx) -> void — register command prefix
                const constIdx = this.pop();
                const prefix = (constIdx >= 0 && constIdx < this.constants.length) ? this.constants[constIdx] : '?';
                this.onOutput(`[CMD] Registered command prefix: "${prefix}"\n`);
                break;
            }
            case 46: { // RESPONSE_CMND(buf_ref) -> void — send console response
                const bufRef = this.pop();
                const resolved = this.resolveRef(bufRef);
                let text = '';
                if (resolved) {
                    const { arr, base } = resolved;
                    const maxLen = resolved.maxLen || (arr.length - base);
                    for (let i = 0; i < maxLen && arr[base + i]; i++) {
                        text += String.fromCharCode(arr[base + i] & 0xFF);
                    }
                }
                this.onOutput(`[CMD] Response: ${text}\n`);
                break;
            }
            case 48: { // RESPONSE_CMND_STR(const_idx) -> void — responseCmnd with string literal
                const ci = this.pop();
                const text = (ci >= 0 && ci < this.constants.length) ? this.constants[ci] : '';
                this.onOutput(`[CMD] Response: ${text}\n`);
                break;
            }

            // ── SPI bus — browser simulation ──────────────
            case 120: { // SPI_INIT(sclk, mosi, miso, speed_mhz) -> int
                const speed = this.pop();
                const miso = this.pop();
                const mosi = this.pop();
                const sclk = this.pop();
                if (sclk < 0) {
                    this.onOutput(`[SPI] Init HW bus ${-sclk} @ ${speed}MHz\n`);
                } else {
                    this.onOutput(`[SPI] Init bitbang sclk=${sclk} mosi=${mosi} miso=${miso} @ ${speed}MHz\n`);
                }
                this.push(1);  // simulated: success
                break;
            }
            case 121: { // SPI_SET_CS(index, pin) -> void
                const pin = this.pop();
                const index = this.pop();
                this.onOutput(`[SPI] CS${index} = pin ${pin}\n`);
                break;
            }
            case 122: { // SPI_TRANSFER(cs, buf, len, mode) -> int
                const mode = this.pop();
                const len = this.pop();
                const bufRef = this.pop();
                const cs = this.pop();
                const resolved = this.resolveRef(bufRef);
                let hexStr = '';
                if (resolved) {
                    const { arr, base } = resolved;
                    const n = Math.min(len, 16);
                    for (let i = 0; i < n; i++) { hexStr += (arr[base + i] & 0xFF).toString(16).padStart(2, '0') + ' '; }
                    if (len > 16) hexStr += '...';
                    // Simulated: fill with zeros (MISO = 0)
                    const maxLen = resolved.maxLen || (arr.length - base);
                    for (let i = 0; i < Math.min(len, maxLen); i++) { arr[base + i] = 0; }
                }
                const modeStr = mode === 1 ? '8bit' : mode === 2 ? '16bit' : mode === 3 ? '24bit' : `${mode}`;
                this.onOutput(`[SPI] CS${cs} xfer ${len}×${modeStr} TX=[${hexStr.trim()}]\n`);
                this.push(len);
                break;
            }

            // ── Smart Meter (SML) — browser simulation ────
            case 110: { // SML_GET — get meter value (returns 0 in browser)
                const idx = this.pop();
                // In browser: simulate with 0 (no meters available)
                this.push(0);
                break;
            }
            case 111: { // SML_GETSTR — get meter ID string into buf
                const bufRef = this.pop();
                const idx = this.pop();
                // In browser: return 0 (no string copied)
                this.push(0);
                break;
            }
            case 114: { // SML_WRITE — send hex sequence to meter
                const bufRef = this.pop();
                const meter = this.pop();
                this.onOutput(`[SML] write to meter ${meter}\n`);
                this.push(0);
                break;
            }
            case 115: { // SML_READ — read raw meter buffer
                const bufRef = this.pop();
                const meter = this.pop();
                this.onOutput(`[SML] read from meter ${meter}\n`);
                this.push(0);
                break;
            }
            case 116: { // SML_SETBAUD — change meter baud rate
                const baud = this.pop();
                const meter = this.pop();
                this.onOutput(`[SML] set baud meter ${meter} = ${baud}\n`);
                this.push(0);
                break;
            }
            case 117: { // SML_SETWSTR — set async write string
                const bufRef = this.pop();
                const meter = this.pop();
                this.onOutput(`[SML] setWStr meter ${meter}\n`);
                this.push(0);
                break;
            }
            case 118: { // SML_SETOPT — set SML global options
                const opts = this.pop();
                this.onOutput(`[SML] setOptions ${opts}\n`);
                this.push(0);
                break;
            }
            case 119: { // SML_GETV — get/reset data valid flags
                const sel = this.pop();
                this.push(0);
                break;
            }
            case 124: { // SML_WRITE_STR — send string literal to meter
                const ci = this.pop();
                const meter = this.pop();
                const str = (ci >= 0 && ci < this.constants.length) ? this.constants[ci] : '?';
                this.onOutput(`[SML] write to meter ${meter}: "${str}"\n`);
                this.push(0);
                break;
            }
            case 125: { // SML_SETWSTR_STR — set async write from string literal
                const ci = this.pop();
                const meter = this.pop();
                const str = (ci >= 0 && ci < this.constants.length) ? this.constants[ci] : '?';
                this.onOutput(`[SML] setWStr meter ${meter}: "${str}"\n`);
                this.push(0);
                break;
            }

            // ── Tasmota system variables (simulated in browser) ──
            case 130: { // TASM_GET
                const idx = this.pop();
                const now = new Date();
                let val = 0;
                switch (idx) {
                    case 0: val = 1; break;             // tasm_wifi (always connected)
                    case 1: val = 1; break;             // tasm_mqttcon (always connected)
                    case 2: val = 300; break;            // tasm_teleperiod
                    case 3: val = Math.floor((Date.now() - this.startTime) / 1000); break; // tasm_uptime
                    case 4: val = 32768; break;          // tasm_heap
                    case 5: val = 0; break;              // tasm_power
                    case 6: val = 50; break;             // tasm_dimmer
                    case 7: { // tasm_temp (float)
                        const buf = new ArrayBuffer(4);
                        new Float32Array(buf)[0] = 22.5;
                        this.push(new Int32Array(buf)[0]);
                        break;
                    }
                    case 8: { // tasm_hum (float)
                        const buf = new ArrayBuffer(4);
                        new Float32Array(buf)[0] = 55.0;
                        this.push(new Int32Array(buf)[0]);
                        break;
                    }
                    case 9: val = now.getHours(); break;    // tasm_hour
                    case 10: val = now.getMinutes(); break;  // tasm_minute
                    case 11: val = now.getSeconds(); break;  // tasm_second
                    case 12: val = now.getFullYear(); break; // tasm_year
                    case 13: val = now.getMonth() + 1; break; // tasm_month (1-12)
                    case 14: val = now.getDate(); break;     // tasm_day (1-31)
                    case 15: val = now.getDay() + 1; break;  // tasm_wday (1=Sun..7=Sat)
                    case 16: { // tasm_cw — ISO calendar week
                        const a = Math.floor((14 - (now.getMonth() + 1)) / 12);
                        const y = now.getFullYear() + 4800 - a;
                        const m = (now.getMonth() + 1) + 12 * a - 3;
                        const jd = now.getDate() + Math.floor((153 * m + 2) / 5)
                            + 365 * y + Math.floor(y / 4) - Math.floor(y / 100)
                            + Math.floor(y / 400) - 32045;
                        const d4 = ((jd + 31741 - (jd % 7)) % 146097 % 36524 % 1461);
                        const L = Math.floor(d4 / 1460);
                        const d1 = ((d4 - L) % 365) + L;
                        val = Math.floor(d1 / 7) + 1;
                        break;
                    }
                    case 17: { // tasm_sunrise — simulate 6:30 AM
                        val = 6 * 60 + 30;
                        break;
                    }
                    case 18: { // tasm_sunset — simulate 8:00 PM
                        val = 20 * 60;
                        break;
                    }
                    case 19: { // tasm_time — minutes since midnight
                        val = now.getHours() * 60 + now.getMinutes();
                        break;
                    }
                    case 20: val = 4194304; break; // tasm_pheap (simulate 4MB PSRAM)
                    case 26: val = 28000; break;   // tasm_maxblock (sim: 28 KB max contiguous block)
                    case 27: val = 0; break;       // tasm_frag (sim: never fragmented)
                }
                if (idx !== 7 && idx !== 8) this.push(val);
                break;
            }
            case 131: { // TASM_SET
                const idx = this.pop();
                const val = this.pop();
                this.onOutput(`[TASM] set var[${idx}] = ${val}\n`);
                break;
            }

            // ── Sensor JSON parsing (browser simulation) ──
            case 132: { // SENSOR_GET
                const ci = this.pop();  // constant pool index
                const path = (ci >= 0 && ci < this.constants.length && typeof this.constants[ci] === 'string')
                    ? this.constants[ci] : '';
                // Simulate some common sensor values
                let fval = 0.0;
                const parts = path.split('#');
                if (parts.length >= 2) {
                    const key = parts[parts.length - 1].toLowerCase();
                    if (key === 'temperature') fval = 22.5;
                    else if (key === 'humidity') fval = 55.0;
                    else if (key === 'pressure') fval = 1013.25;
                }
                this.onOutput(`[SENSOR] get "${path}" = ${fval}\n`);
                // Push float as int bits
                const buf = new ArrayBuffer(4);
                new Float32Array(buf)[0] = fval;
                this.push(new Int32Array(buf)[0]);
                break;
            }

            // ── HTTP requests (browser simulation) ──
            case 140: { // HTTP_GET
                const responseRef = this.pop();
                const urlRef = this.pop();
                const url = this.readStringFromRef(urlRef);
                // Simulate HTTP GET response
                const hdrs = this.httpPendingHeaders.map(h => `${h.name}: ${h.value}`).join(', ');
                this.httpPendingHeaders = [];
                const mockResponse = `{"url":"${url}","method":"GET","status":200}`;
                this.writeStringToRef(responseRef, mockResponse);
                this.onOutput(`[HTTP] GET "${url}"${hdrs ? ' [' + hdrs + ']' : ''} → ${mockResponse.length} bytes\n`);
                this.push(mockResponse.length);
                break;
            }
            case 141: { // HTTP_POST
                const responseRef = this.pop();
                const dataRef = this.pop();
                const urlRef = this.pop();
                const url = this.readStringFromRef(urlRef);
                const postData = this.readStringFromRef(dataRef);
                // Simulate HTTP POST response
                this.httpPendingHeaders = [];
                const mockResponse = `{"url":"${url}","method":"POST","posted":${postData.length},"status":200}`;
                this.writeStringToRef(responseRef, mockResponse);
                this.onOutput(`[HTTP] POST "${url}" (${postData.length} bytes) → ${mockResponse.length} bytes\n`);
                this.push(mockResponse.length);
                break;
            }
            case 142: { // HTTP_HEADER
                const valueRef = this.pop();
                const nameRef = this.pop();
                const name = this.readStringFromRef(nameRef);
                const value = this.readStringFromRef(valueRef);
                this.httpPendingHeaders.push({ name, value });
                this.onOutput(`[HTTP] Header: ${name}: ${value}\n`);
                break;
            }

            case 143: { // WEB_PARSE — parse non-JSON web response
                const dstRef = this.pop();
                const index = this.pop();
                const ci = this.pop();
                const srcRef = this.pop();
                const src = this.readStringFromRef(srcRef);
                const delim = this.constants[ci] || '';
                let result = '';

                if (index > 0 && delim.length > 0) {
                    // Mode 1: split by delimiter, return Nth segment (1-based)
                    let wd = src;
                    let lwd = wd;
                    let count = index;
                    while (count > 0) {
                        const pos = wd.indexOf(delim);
                        if (pos >= 0) {
                            count--;
                            if (count === 0) {
                                result = lwd.substring(0, lwd.length - (wd.length - pos));
                            } else {
                                wd = wd.substring(pos + delim.length);
                                lwd = wd;
                            }
                        } else {
                            result = lwd;
                            break;
                        }
                    }
                } else if (index < 0 && delim.length > 0) {
                    // Mode 2: find "delim=value", extract value
                    const pos = src.indexOf(delim);
                    if (pos >= 0) {
                        const eqPos = src.indexOf('=', pos);
                        if (eqPos >= 0) {
                            let end = eqPos + 1;
                            while (end < src.length && src[end] !== ',' && src[end] !== ':') {
                                end++;
                            }
                            result = src.substring(eqPos + 1, end);
                        }
                    }
                }

                this.writeStringToRef(dstRef, result);
                this.onOutput(`[HTTP] webParse("${delim}", ${index}) → "${result}" (${result.length})\n`);
                this.push(result.length);
                break;
            }

            // ── String manipulation ──
            case 74: { // STR_TOKEN
                const n = this.pop();
                const delim = this.pop();
                const srcRef = this.pop();
                const dstRef = this.pop();
                const src = this.readStringFromRef(srcRef);
                const delimCh = String.fromCharCode(delim);
                const tokens = src.split(delimCh);
                const token = (n >= 1 && n <= tokens.length) ? tokens[n - 1] : '';
                this.writeStringToRef(dstRef, token);
                this.push(token.length);
                break;
            }
            case 75: { // STR_SUB
                let len = this.pop();
                let pos = this.pop();
                const srcRef = this.pop();
                const dstRef = this.pop();
                const src = this.readStringFromRef(srcRef);
                if (pos < 0) pos = src.length + pos;
                if (pos < 0) pos = 0;
                if (pos > src.length) pos = src.length;
                if (len <= 0 || pos + len > src.length) len = src.length - pos;
                const sub = src.substring(pos, pos + len);
                this.writeStringToRef(dstRef, sub);
                this.push(sub.length);
                break;
            }
            case 76: { // STR_FIND
                const needleRef = this.pop();
                const haystackRef = this.pop();
                const haystack = this.readStringFromRef(haystackRef);
                const needle = this.readStringFromRef(needleRef);
                this.push(haystack.indexOf(needle));
                break;
            }
            case 47: { // STR_FIND_CONST (haystack_ref, needle_const_idx)
                const ci = this.pop();
                const haystackRef = this.pop();
                const haystack = this.readStringFromRef(haystackRef);
                const needle = this.constants[ci] || '';
                this.push(haystack.indexOf(needle));
                break;
            }
            case 77: { // STR_TO_INT (atoi)
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.push(parseInt(str, 10) || 0);
                break;
            }
            case 78: { // STR_TO_FLOAT (atof)
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.pushFloat(parseFloat(str) || 0.0);
                break;
            }

            // ── WebUI widgets (browser mock — log generated HTML) ──
            case 150: { // WEB_BUTTON
                const labelIdx = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || 'Button';
                const nval = val ? 0 : 1;
                const txt = val ? 'ON' : 'OFF';
                this.onOutput(`[WebUI] Button: ${label}=${txt} (gidx=${resolved.base})\n`);
                break;
            }
            case 394: { // WEB_TOGGLE
                const labelIdx = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || 'Toggle';
                this.onOutput(`[WebUI] Toggle: ${label}=${val ? 'ON' : 'OFF'} (gidx=${resolved.base})\n`);
                break;
            }
            case 151: { // WEB_SLIDER
                const labelIdx = this.pop();
                const max = this.pop();
                const min = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || 'Slider';
                this.onOutput(`[WebUI] Slider: ${label}=${val} [${min}..${max}] (gidx=${resolved.base})\n`);
                break;
            }
            case 152: { // WEB_CHECKBOX
                const labelIdx = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || 'Checkbox';
                this.onOutput(`[WebUI] Checkbox: ${label}=${val ? 'checked' : 'unchecked'} (gidx=${resolved.base})\n`);
                break;
            }
            case 153: { // WEB_TEXT
                const labelIdx = this.pop();
                const maxlen = this.pop();
                const gref = this.pop();
                const text = this.readStringFromRef(gref);
                const label = this.constants[labelIdx] || 'Text';
                this.onOutput(`[WebUI] Text: ${label}="${text}" maxlen=${maxlen}\n`);
                break;
            }
            case 154: { // WEB_NUMBER
                const labelIdx = this.pop();
                const max = this.pop();
                const min = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || 'Number';
                this.onOutput(`[WebUI] Number: ${label}=${val} [${min}..${max}] (gidx=${resolved.base})\n`);
                break;
            }
            case 155: { // WEB_PULLDOWN(gref, label_const, opts_const)
                const optsIdx = this.pop();
                const labelIdx = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || '';
                const optsStr = this.constants[optsIdx] || '';
                if (optsStr === '@getfreepins') {
                    this.onOutput(`[WebUI] Pulldown "${label}": GPIO pin picker, current=${val}\n`);
                } else {
                    const opts = optsStr.split('|');
                    this.onOutput(`[WebUI] Pulldown "${label}": sel=${val} opts=[${opts.join(',')}] (gidx=${resolved.base})\n`);
                }
                break;
            }
            case 156: { // WEB_RADIO
                const optsIdx = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const opts = (this.constants[optsIdx] || '').split('|');
                this.onOutput(`[WebUI] Radio: sel=${val} opts=[${opts.join(',')}] (gidx=${resolved.base})\n`);
                break;
            }
            case 157: { // WEB_TIME
                const labelIdx = this.pop();
                const gref = this.pop();
                const resolved = this.resolveRef(gref);
                const val = resolved.arr[resolved.base];
                const hh = Math.floor(val / 100);
                const mm = val % 100;
                const label = this.constants[labelIdx] || 'Time';
                this.onOutput(`[WebUI] Time: ${label}=${String(hh).padStart(2,'0')}:${String(mm).padStart(2,'0')} (gidx=${resolved.base})\n`);
                break;
            }
            case 158: { // WEB_PAGE_LABEL
                const labelIdx = this.pop();
                const pageNum = this.pop();
                const label = this.constants[labelIdx] || 'TinyC UI';
                this.onOutput(`[WebUI] Page ${pageNum} label: "${label}"\n`);
                break;
            }
            case 159: { // WEB_PAGE
                this.push(0); // always page 0 in browser VM
                break;
            }
            case 148: { // WEB_SEND_JSON_ARRAY
                const count = this.pop();
                const arrRef = this.pop();
                const resolved = this.resolveRef(arrRef);
                const { arr, base } = resolved;
                const maxLen = resolved.maxLen || (arr.length - base);
                const n = Math.min(count, maxLen);
                const vals = [];
                for (let i = 0; i < n; i++) {
                    const tmp = new Int32Array(1);
                    tmp[0] = arr[base + i];
                    const fv = new Float32Array(tmp.buffer)[0];
                    vals.push(Math.trunc(fv));
                }
                this.onOutput('[' + vals.join(',') + ']');
                break;
            }
            case 160: { // WEB_SEND_FILE
                const fnIdx = this.pop();
                const fname = this.constants[fnIdx] || '?';
                this.onOutput(`[WebUI] Include file: "${fname}"\n`);
                break;
            }
            case 161: { // WEB_ON
                const urlIdx = this.pop();
                const handlerNum = this.pop();
                const url = this.constants[urlIdx] || '?';
                this.onOutput(`[Web] Registered handler ${handlerNum}: "${url}"\n`);
                break;
            }
            case 162: { // WEB_HANDLER
                this.push(0); // no handler active in browser VM
                break;
            }
            case 163: { // WEB_ARG
                const bufRef = this.pop();
                const nameIdx = this.pop();
                const argName = this.constants[nameIdx] || '?';
                this.onOutput(`[Web] webArg("${argName}") → "" (browser mock)\n`);
                this.push(0); // no args in browser VM
                break;
            }
            case 164: { // MDNS
                const typeIdx = this.pop();
                const macIdx = this.pop();
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '?';
                const mac = this.constants[macIdx] || '?';
                const type = this.constants[typeIdx] || '?';
                this.onOutput(`[mDNS] mdns("${name}", "${mac}", "${type}") (browser mock)\n`);
                this.push(0);
                break;
            }

            case 165: { // WEB_CONSOLE_BUTTON
                const labelIdx = this.pop();
                const urlIdx = this.pop();
                const url = this.constants[urlIdx] || '?';
                const label = this.constants[labelIdx] || '?';
                this.onOutput(`[WebUI] Console button: "${label}" -> ${url} (browser mock)\n`);
                break;
            }

            case 233: { // WEB_CHART_SIZE
                const h = this.pop();
                const w = this.pop();
                this.output(`[WebChartSize ${w}x${h}]\n`);
                break;
            }
            case 261: { // WEB_CHART_TBASE
                const mins = this.pop();
                this.output(`[WebChartTimeBase ${mins}min]\n`);
                break;
            }
            case 166: { // WEB_CHART
                const ymax_bits = this.pop();
                const ymin_bits = this.pop();
                const interval = this.pop();
                const decimals = this.pop();
                const arr_ref = this.pop();
                const count = this.pop();
                const pos = this.pop();
                const color = this.pop();
                const unitIdx = this.pop();
                const titleIdx = this.pop();
                const type = this.pop();
                const title = this.constants[titleIdx] || '(series)';
                const unit = this.constants[unitIdx] || '';
                const typeMap = {0:'line', 1:'column', 98:'bar', 99:'column', 104:'histogram', 108:'line', 115:'stacked', 116:'table'};
                const typeStr = typeMap[type] || `type(${type})`;
                const dv = new DataView(new ArrayBuffer(4));
                dv.setInt32(0, ymin_bits); const ymin = dv.getFloat32(0);
                dv.setInt32(0, ymax_bits); const ymax = dv.getFloat32(0);
                const rangeStr = (ymin === ymax) ? 'auto' : `${ymin}..${ymax}`;
                this.onOutput(`[WebChart] ${typeStr} "${title}" [${unit}] color=#${(color>>>0).toString(16)} pos=${pos} count=${count} decimals=${decimals} interval=${interval}min range=${rangeStr} (browser mock)\n`);
                break;
            }

            case 167: { // PLUGIN_QUERY — pluginQuery(dst, index, p1, p2) -> strlen
                const p2 = this.pop();
                const p1 = this.pop();
                const index = this.pop();
                const dst_ref = this.pop();
                this.onOutput(`[pluginQuery] index=${index} p1=${p1} p2=${p2} (sim: empty)\n`);
                // Simulator: write empty string to dst, return 0
                const dst = this.resolveRef(dst_ref);
                if (dst) dst.arr[dst.offset] = 0;
                this.push(0);
                break;
            }

            case 168: { // SORT_ARRAY — sortArray(arr, count, flags)
                const flags = this.pop();
                const count = this.pop();
                const arr_ref = this.pop();
                const ref = this.resolveRef(arr_ref);
                if (ref && count > 1) {
                    const arr = ref.arr;
                    const off = ref.offset;
                    const n = Math.min(count, arr.length - off);
                    const slice = arr.slice(off, off + n);
                    const isFloat = flags & 1;
                    const isDesc = flags & 2;
                    const dv = new DataView(new ArrayBuffer(4));
                    slice.sort((a, b) => {
                        let va, vb;
                        if (isFloat) {
                            dv.setInt32(0, a); va = dv.getFloat32(0);
                            dv.setInt32(0, b); vb = dv.getFloat32(0);
                        } else { va = a; vb = b; }
                        return isDesc ? vb - va : va - vb;
                    });
                    for (let i = 0; i < n; i++) arr[off + i] = slice[i];
                }
                break;
            }

            case 169: { // CAM_CONTROL — camControl(sel, p1, p2) -> int
                const p2 = this.pop();
                const p1 = this.pop();
                const sel = this.pop();
                const selNames = ['init','capture','options','width','height','stream','motion','savePic'];
                const name = selNames[sel] || `sel${sel}`;
                this.onOutput(`[CAM] camControl(${name}, ${p1}, ${p2}) (sim: 0)\n`);
                this.push(0);
                break;
            }

            // ── Display drawing (browser mocks) ──
            case 170: { // DSP_TEXT — raw DisplayText command string
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(`[DSP] dspText("${str}")\n`);
                break;
            }
            case 171: // DSP_CLEAR
                this.onOutput(`[DSP] dspClear()\n`);
                break;
            case 172: { // DSP_POS
                const y = this.pop();
                const x = this.pop();
                this.onOutput(`[DSP] dspPos(${x}, ${y})\n`);
                break;
            }
            case 173: { // DSP_FONT
                const f = this.pop();
                this.onOutput(`[DSP] dspFont(${f})\n`);
                break;
            }
            case 174: { // DSP_SIZE
                const s = this.pop();
                this.onOutput(`[DSP] dspSize(${s})\n`);
                break;
            }
            case 175: { // DSP_COLOR
                const bg = this.pop();
                const fg = this.pop();
                this.onOutput(`[DSP] dspColor(${fg}, ${bg})\n`);
                break;
            }
            case 176: { // DSP_DRAW — draw string at current pos
                const ref = this.pop();
                const str = this.readStringFromRef(ref);
                this.onOutput(`[DSP] dspDraw("${str}")\n`);
                break;
            }
            case 177: { // DSP_PIXEL
                const y = this.pop();
                const x = this.pop();
                this.onOutput(`[DSP] dspPixel(${x}, ${y})\n`);
                break;
            }
            case 178: { // DSP_LINE
                const y1 = this.pop();
                const x1 = this.pop();
                this.onOutput(`[DSP] dspLine(${x1}, ${y1})\n`);
                break;
            }
            case 179: { // DSP_RECT
                const h = this.pop();
                const w = this.pop();
                this.onOutput(`[DSP] dspRect(${w}, ${h})\n`);
                break;
            }
            case 180: { // DSP_FILL_RECT
                const h = this.pop();
                const w = this.pop();
                this.onOutput(`[DSP] dspFillRect(${w}, ${h})\n`);
                break;
            }
            case 181: { // DSP_CIRCLE
                const r = this.pop();
                this.onOutput(`[DSP] dspCircle(${r})\n`);
                break;
            }
            case 182: { // DSP_FILL_CIRCLE
                const r = this.pop();
                this.onOutput(`[DSP] dspFillCircle(${r})\n`);
                break;
            }
            case 183: { // DSP_HLINE
                const w = this.pop();
                this.onOutput(`[DSP] dspHLine(${w})\n`);
                break;
            }
            case 184: { // DSP_VLINE
                const h = this.pop();
                this.onOutput(`[DSP] dspVLine(${h})\n`);
                break;
            }
            case 185: { // DSP_ROUND_RECT
                const r = this.pop();
                const h = this.pop();
                const w = this.pop();
                this.onOutput(`[DSP] dspRoundRect(${w}, ${h}, ${r})\n`);
                break;
            }
            case 186: { // DSP_FILL_RRECT
                const r = this.pop();
                const h = this.pop();
                const w = this.pop();
                this.onOutput(`[DSP] dspFillRoundRect(${w}, ${h}, ${r})\n`);
                break;
            }
            case 187: { // DSP_TRIANGLE
                const y2 = this.pop();
                const x2 = this.pop();
                const y1 = this.pop();
                const x1 = this.pop();
                this.onOutput(`[DSP] dspTriangle(${x1}, ${y1}, ${x2}, ${y2})\n`);
                break;
            }
            case 188: { // DSP_FILL_TRI
                const y2 = this.pop();
                const x2 = this.pop();
                const y1 = this.pop();
                const x1 = this.pop();
                this.onOutput(`[DSP] dspFillTriangle(${x1}, ${y1}, ${x2}, ${y2})\n`);
                break;
            }
            case 189: { // DSP_DIM
                const val = this.pop();
                this.onOutput(`[DSP] dspDim(${val})\n`);
                break;
            }
            case 190: { // DSP_ONOFF
                const on = this.pop();
                this.onOutput(`[DSP] dspOnOff(${on})\n`);
                break;
            }
            case 191: // DSP_UPDATE
                this.onOutput(`[DSP] dspUpdate()\n`);
                break;
            case 192: { // DSP_PICTURE
                const scale = this.pop();
                const fnIdx = this.pop();
                const fname = this.constants[fnIdx] || '?';
                this.onOutput(`[DSP] dspPicture("${fname}", ${scale})\n`);
                break;
            }
            case 193: // DSP_WIDTH
                this.onOutput(`[DSP] dspWidth() -> 320 (mock)\n`);
                this.push(320);
                break;
            case 194: // DSP_HEIGHT
                this.onOutput(`[DSP] dspHeight() -> 240 (mock)\n`);
                this.push(240);
                break;

            case 195: { // DSP_TEXT_STR — DisplayText from string literal
                const ci = this.pop();
                const str = this.constants[ci] || '?';
                this.onOutput(`[DSP] dspText("${str}")\n`);
                break;
            }
            case 196: { // DSP_DRAW_STR — draw string literal at current pos
                const ci = this.pop();
                const str = this.constants[ci] || '?';
                this.onOutput(`[DSP] dspDraw("${str}")\n`);
                break;
            }
            case 197: { // DSP_PAD — set text padding
                const n = this.pop();
                this.onOutput(`[DSP] dspPad(${n})\n`);
                break;
            }
            case 262: { // DSP_LOAD_IMG — load JPG to image store
                const fnIdx = this.pop();
                const fname = this.constants[fnIdx] || '?';
                this.onOutput(`[DSP] dspLoadImage("${fname}") -> 0 (mock)\n`);
                this.push(0); // mock: always slot 0
                break;
            }
            case 263: { // DSP_IMG_RECT — push sub-rect from image to screen
                const h  = this.pop();
                const w  = this.pop();
                const dy = this.pop();
                const dx = this.pop();
                const sy = this.pop();
                const sx = this.pop();
                const slot = this.pop();
                this.onOutput(`[DSP] dspPushImageRect(${slot}, ${sx},${sy}, ${dx},${dy}, ${w},${h})\n`);
                break;
            }
            case 264: { // DSP_IMG_WIDTH
                const slot = this.pop();
                this.onOutput(`[DSP] dspImageWidth(${slot}) -> 240 (mock)\n`);
                this.push(240); // mock
                break;
            }
            case 265: { // DSP_IMG_HEIGHT
                const slot = this.pop();
                this.onOutput(`[DSP] dspImageHeight(${slot}) -> 240 (mock)\n`);
                this.push(240); // mock
                break;
            }
            case 266: { // DSP_TEXT_WIDTH
                const len = this.pop();
                const w = len * 6; // mock: 6px per char (GFX default)
                this.onOutput(`[DSP] dspTextWidth(${len}) -> ${w} (mock)\n`);
                this.push(w);
                break;
            }
            case 267: { // DSP_TEXT_HEIGHT
                this.onOutput(`[DSP] dspTextHeight() -> 16 (mock)\n`);
                this.push(16); // mock
                break;
            }
            case 268: { // DSP_IMG_TEXT — composite text on image, push once
                const ref    = this.pop();
                const align  = this.pop();
                const fieldw = this.pop();
                const color  = this.pop();
                const y      = this.pop();
                const x      = this.pop();
                const slot   = this.pop();
                const text = this.readStringFromRef(ref);
                const alignStr = ['left','right','center'][align] || '?';
                this.onOutput(`[DSP] dspImgText(${slot}, ${x},${y}, color=${color}, fw=${fieldw}, ${alignStr}, "${text}")\n`);
                break;
            }

            // ─── Canvas / RGB565 image slots (simulator mocks) ───
            case 328: { // IMG_CREATE(w, h) -> slot
                const h = this.pop();
                const w = this.pop();
                this.onOutput(`[DSP] imgCreate(${w}, ${h}) -> 0 (mock)\n`);
                this.push(0);
                break;
            }
            case 329: { // IMG_BEGIN_DRAW(slot)
                const slot = this.pop();
                this.onOutput(`[DSP] imgBeginDraw(${slot})\n`);
                break;
            }
            case 330: { // IMG_END_DRAW()
                this.onOutput(`[DSP] imgEndDraw()\n`);
                break;
            }
            case 331: { // IMG_CLEAR(slot, color)
                const color = this.pop();
                const slot  = this.pop();
                this.onOutput(`[DSP] imgClear(${slot}, 0x${(color & 0xffff).toString(16)})\n`);
                break;
            }
            case 332: { // IMG_BLIT(dst, src, sx, sy, dx, dy, w, h)
                const h  = this.pop();
                const w  = this.pop();
                const dy = this.pop();
                const dx = this.pop();
                const sy = this.pop();
                const sx = this.pop();
                const src = this.pop();
                const dst = this.pop();
                this.onOutput(`[DSP] imgBlit(${dst}, ${src}, ${sx},${sy}, ${dx},${dy}, ${w},${h})\n`);
                break;
            }
            case 333: { // IMG_INVALIDATE(slot, x, y, w, h)
                const h = this.pop();
                const w = this.pop();
                const y = this.pop();
                const x = this.pop();
                const slot = this.pop();
                this.onOutput(`[DSP] imgInvalidate(${slot}, ${x},${y}, ${w},${h})\n`);
                break;
            }
            case 334: { // IMG_FLUSH(slot, panel_x, panel_y)
                const py = this.pop();
                const px = this.pop();
                const slot = this.pop();
                this.onOutput(`[DSP] imgFlush(${slot}, ${px},${py})\n`);
                break;
            }

            // ─── Audio ───
            case 200: { // AUDIO_VOL
                const vol = this.pop();
                this.onOutput(`[AUDIO] audioVol(${vol})\n`);
                break;
            }
            case 201: { // AUDIO_PLAY
                const ci = this.pop();
                const file = this.constants[ci] || `[const:${ci}]`;
                this.onOutput(`[AUDIO] audioPlay("${file}")\n`);
                break;
            }
            case 202: { // AUDIO_SAY
                const ci = this.pop();
                const text = this.constants[ci] || `[const:${ci}]`;
                this.onOutput(`[AUDIO] audioSay("${text}")\n`);
                break;
            }

            case 203: { // PERSIST_SAVE
                this.persistSave();
                break;
            }

            // ── Deep sleep (browser stub) ─────────────────────
            case 230: { // DEEP_SLEEP
                const secs = this.pop();
                this.onOutput(`[DEEP_SLEEP] ${secs} seconds (simulated — halting VM)\n`);
                this.halted = true;
                break;
            }
            case 231: { // DEEP_SLEEP_GPIO
                const level = this.pop();
                const pin = this.pop();
                const secs = this.pop();
                this.onOutput(`[DEEP_SLEEP] ${secs}s, wake GPIO${pin} ${level ? 'HIGH' : 'LOW'} (simulated — halting VM)\n`);
                this.halted = true;
                break;
            }
            case 232: { // WAKEUP_CAUSE
                this.push(0);  // simulated: always "not deep sleep"
                break;
            }

            // ── Email (browser mock) ──────────────────────────
            case 234: { // EMAIL_BODY
                const bodyRef = this.pop();
                const body = this.readStringFromRef(bodyRef);
                this._emailBody = body;
                this.onOutput(`[EMAIL] Body set: "${body.substring(0, 60)}${body.length > 60 ? '...' : ''}"\n`);
                break;
            }
            case 238: { // EMAIL_BODY_STR — mailBody("literal")
                const ci = this.pop();
                const body = this.constants[ci] || '';
                this._emailBody = body;
                this.onOutput(`[EMAIL] Body set: "${body.substring(0, 60)}${body.length > 60 ? '...' : ''}"\n`);
                break;
            }
            case 235: { // EMAIL_ATTACH
                const ci = this.pop();
                const path = this.constants[ci] || '';
                if (!this._emailAttach) this._emailAttach = [];
                this._emailAttach.push(path);
                this.onOutput(`[EMAIL] Attachment added: ${path}\n`);
                break;
            }
            case 237: { // EMAIL_ATTACH_PIC — mailAttachPic(bufnum)
                const bufnum = this.pop();
                if (!this._emailAttach) this._emailAttach = [];
                this._emailAttach.push(`$${bufnum}`);
                this.onOutput(`[EMAIL] Picture attachment added: buffer ${bufnum}\n`);
                break;
            }
            case 236: { // EMAIL_SEND
                const paramsRef = this.pop();
                const params = this.readStringFromRef(paramsRef);
                this.onOutput(`[EMAIL] Sending: ${params}\n`);
                if (this._emailBody) this.onOutput(`[EMAIL]   Body: "${this._emailBody.substring(0, 80)}"\n`);
                if (this._emailAttach) {
                    for (const a of this._emailAttach) this.onOutput(`[EMAIL]   Attach: ${a}\n`);
                }
                this.onOutput(`[EMAIL] Send complete (simulated — returns 0)\n`);
                this._emailBody = null;
                this._emailAttach = null;
                this.push(0); // success
                break;
            }

            // ── Tesla Powerwall (browser mock) ───────────────
            case Syscall.PWL_REQUEST: { // PWL_REQUEST
                const ci = this.pop();
                const url = this.constants[ci] || `[const:${ci}]`;
                this.onOutput(`[PWL] pwlRequest("${url}") — simulated\n`);
                this.push(0);
                break;
            }
            case Syscall.PWL_GET_FLOAT: { // PWL_GET_FLOAT
                const ci = this.pop();
                const path = this.constants[ci] || `[const:${ci}]`;
                this.onOutput(`[PWL] pwlGet("${path}") — returns 0.0 (simulated)\n`);
                // Push float 0.0 as int32 bit pattern
                const buf = new ArrayBuffer(4);
                new Float32Array(buf)[0] = 0.0;
                this.push(new Int32Array(buf)[0]);
                break;
            }
            case Syscall.PWL_GET_STR: { // PWL_GET_STR
                const bufRef = this.pop();
                const ci = this.pop();
                const path = this.constants[ci] || `[const:${ci}]`;
                this.onOutput(`[PWL] pwlStr("${path}") — returns "" (simulated)\n`);
                // Write empty string to buffer
                const buf = this.resolveRef(bufRef);
                if (buf) buf[0] = 0;
                this.push(0);
                break;
            }
            case Syscall.PWL_BIND: { // PWL_BIND
                const ci = this.pop();    // path const
                const ref = this.pop();   // variable ref
                const path = this.constants[ci] || `[const:${ci}]`;
                this.onOutput(`[PWL] pwlBind(&var, "${path}") — binding registered (simulated)\n`);
                break;
            }

            // ── Touch buttons & sliders (browser mock) ────────
            case 240: case 241: case 242: { // DSP_BUTTON / DSP_TBUTTON / DSP_PBUTTON
                const textCI = this.pop();
                const ts = this.pop(); const tc = this.pop(); const fc = this.pop();
                const oc = this.pop(); const h = this.pop(); const w = this.pop();
                const y = this.pop(); const x = this.pop(); const num = this.pop();
                const text = this.constants[textCI] || `[const:${textCI}]`;
                const types = {240: 'Power', 241: 'Toggle', 242: 'Push'};
                this.onOutput(`[DSP] ${types[id]} button ${num}: ${text} at (${x},${y}) ${w}x${h}\n`);
                break;
            }
            case 243: { // DSP_SLIDER
                const bc = this.pop(); const fc = this.pop(); const bg = this.pop();
                const ne = this.pop(); const h = this.pop(); const w = this.pop();
                const y = this.pop(); const x = this.pop(); const num = this.pop();
                this.onOutput(`[DSP] Slider ${num} at (${x},${y}) ${w}x${h} elems=${ne}\n`);
                break;
            }
            case 244: { // DSP_BTN_STATE
                const val = this.pop(); const num = this.pop();
                this.onOutput(`[DSP] Button ${num} state = ${val}\n`);
                break;
            }
            case 245: { // TOUCH_BUTTON
                const num = this.pop();
                this.push(-1); // not defined in browser
                break;
            }
            case 246: { // DSP_BTN_DEL
                const num = this.pop();
                this.onOutput(`[DSP] Button delete ${num === -1 ? 'ALL' : num}\n`);
                break;
            }

            // ── TCP server (browser mock) ──────────────────────
            case 210: { // TCP_OPEN — wso(port)
                const port = this.pop();
                console.log(`[TCP] Server opened on port ${port}`);
                this._tcpPort = port;
                this._tcpOpen = true;
                this.push(0); // success
                break;
            }
            case 211: { // TCP_CLOSE — wsc()
                console.log('[TCP] Server closed');
                this._tcpOpen = false;
                break;
            }
            case 212: { // TCP_AVAILABLE — wsa()
                console.log('[TCP] Available: 0 (mock)');
                this.push(0);
                break;
            }
            case 213: { // TCP_READ_STR — wsrs(buf)
                const bufRef = this.pop();
                this.writeStringToRef(bufRef, '');
                this.push(0);
                break;
            }
            case 214: { // TCP_WRITE_STR — wsws(str)
                const strRef = this.pop();
                const str = this.readStringFromRef(strRef);
                console.log(`[TCP] Write string: "${str}"`);
                break;
            }
            case 215: { // TCP_READ_ARR — wsra(arr)
                const arrRef = this.pop();
                console.log('[TCP] Read array: 0 bytes (mock)');
                this.push(0);
                break;
            }
            case 216: { // TCP_WRITE_ARR — wswa(arr, num, type)
                const type = this.pop();
                const num = this.pop();
                const arrRef = this.pop();
                console.log(`[TCP] Write array: ${num} elements, type=${type}`);
                break;
            }

            // ── TCP outgoing client (multi-slot) — browser stub ────
            case Syscall.TCP_CONNECT: { // tcpConnect("ip", port) -> int
                const port = this.pop();
                const ipIdx = this.pop();
                const ip = this.constants[ipIdx] || '';
                this.onOutput(`[TCP] tcpConnect("${ip}", ${port}) — simulator stub, returning -2 (no net)\n`);
                this.push(-2);
                break;
            }
            case Syscall.TCP_CONNECT_REF: { // tcpConnect(char[] ip, port) -> int
                const port = this.pop();
                const ipRef = this.pop();
                const ip = this.readStringFromRef(ipRef);
                this.onOutput(`[TCP] tcpConnect(<${ip}>, ${port}) — simulator stub, returning -2\n`);
                this.push(-2);
                break;
            }
            case Syscall.TCP_DISCONNECT: { // tcpDisconnect()
                this.onOutput('[TCP] tcpDisconnect()\n');
                break;
            }
            case Syscall.TCP_CONNECTED: { // tcpConnected() -> int
                this.push(0);
                break;
            }
            case Syscall.TCP_SELECT: { // tcpSelect(slot)
                const slot = this.pop();
                this.onOutput(`[TCP] tcpSelect(${slot})\n`);
                break;
            }
            case Syscall.TCP_KEEPALIVE: { // tcpKeepalive(idle, intvl, count) -> int
                const cnt   = this.pop();
                const intvl = this.pop();
                const idle  = this.pop();
                this.onOutput(`[TCP] tcpKeepalive(idle=${idle}s, intvl=${intvl}s, count=${cnt}) — simulator stub, returning 0 (not connected)\n`);
                this.push(0);
                break;
            }
            case Syscall.TCP_NODELAY: { // tcpNoDelay(on)
                const on = this.pop();
                this.onOutput(`[TCP] tcpNoDelay(${on}) — simulator stub\n`);
                break;
            }
            case Syscall.TCP_DISCONNECT_REASON: { // tcpDisconnectReason() -> int
                this.push(0); // simulator: never used
                break;
            }
            case Syscall.TCP_TRANSACT: { // tcpTransact(req,req_len,resp,resp_max,timeout) -> int
                const timeout_ms = this.pop();
                const resp_max   = this.pop();
                const resp_ref   = this.pop();
                const req_len    = this.pop();
                const req_ref    = this.pop();
                this.onOutput(`[TCP] tcpTransact(req_len=${req_len}, resp_max=${resp_max}, timeout=${timeout_ms}ms) — simulator stub, returning -2 (not connected)\n`);
                this.push(-2);
                break;
            }
            case Syscall.BLIB_CALL: { // bcall("name", buf, len) -> int
                const len      = this.pop();
                const buf_ref  = this.pop();
                const name_ci  = this.pop();
                const name     = (this.consts && this.consts[name_ci]) || '?';
                this.onOutput(`[BLIB] bcall("${name}", buf, len=${len}) — simulator stub, returning -1 (no registry on host)\n`);
                this.push(-1);
                break;
            }
            case Syscall.TWAI_BEGIN: { // twaiBegin(rx, tx, kbps, mode) -> int
                const mode    = this.pop();
                const kbps    = this.pop();
                const tx_pin  = this.pop();
                const rx_pin  = this.pop();
                this.onOutput(`[TWAI] twaiBegin(rx=${rx_pin}, tx=${tx_pin}, ${kbps} kbps, mode=${mode}) — simulator stub, returning 0 (no CAN on host)\n`);
                this.push(0);
                break;
            }
            case Syscall.TWAI_END: { // twaiEnd()
                this.onOutput(`[TWAI] twaiEnd() — simulator stub\n`);
                break;
            }
            case Syscall.TWAI_AVAILABLE: { // twaiAvailable() -> int
                this.push(0);
                break;
            }
            case Syscall.TWAI_RECV: { // twaiRecv(meta_arr, data_buf, max) -> int
                this.pop(); this.pop(); this.pop();
                this.push(0); // no frame
                break;
            }
            case Syscall.TWAI_SEND: { // twaiSend(id, ext, dlc, buf) -> int
                const buf_ref = this.pop();
                const dlc     = this.pop();
                const ext     = this.pop();
                const id      = this.pop();
                this.onOutput(`[TWAI] twaiSend(id=0x${(id>>>0).toString(16)}, ext=${ext}, dlc=${dlc}) — simulator stub, returning 1\n`);
                this.push(1);
                break;
            }
            case Syscall.TWAI_STATUS: { // twaiStatus(stats_arr) -> int
                this.pop();
                this.push(0); // not installed
                break;
            }
            case Syscall.TWAI_FILTER: { // twaiFilter(mask, value, ext) -> int
                this.pop(); this.pop(); this.pop();
                this.push(1); // accepted
                break;
            }
            case Syscall.WEB_RAW_MODE: { // webRawMode()
                this.onOutput(`[WEB] webRawMode() — simulator stub\n`);
                break;
            }
            case Syscall.WEB_RAW_WRITE: { // webRawWrite(buf)
                const buf_ref = this.pop();
                this.onOutput(`[WEB] webRawWrite(buf=ref ${buf_ref}) — simulator stub (would write to client socket)\n`);
                break;
            }
            case Syscall.WEB_KEEP_ALIVE: { // webKeepAlive()
                this.onOutput(`[WEB] webKeepAlive() — simulator stub (would arm Webserver->setKeepAlive(true))\n`);
                break;
            }

            // ── FreeRTOS spawn/kill — browser stub ─────────────────
            case Syscall.SPAWN_TASK: { // spawnTask("Name") -> int pool_idx
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '???';
                this.onOutput(`[Task] spawnTask("${name}") — simulator stub, returning 0\n`);
                this.push(0);
                break;
            }
            case Syscall.SPAWN_TASK_STACK: { // spawnTask("Name", stack_kb) -> int
                const stack = this.pop();
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '???';
                this.onOutput(`[Task] spawnTask("${name}", ${stack} kB) — simulator stub, returning 0\n`);
                this.push(0);
                break;
            }
            case Syscall.KILL_TASK: { // killTask("Name") -> int
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '???';
                this.onOutput(`[Task] killTask("${name}")\n`);
                this.push(0);
                break;
            }
            case Syscall.TASK_RUNNING: { // taskRunning("Name") -> int
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '???';
                this.push(0);
                break;
            }

            // HomeKit — hardware-only, stub in simulator
            case Syscall.HK_SET_CODE: {
                const codeIdx = this.pop();
                const code = this.constants[codeIdx] || '???';
                this.onOutput(`[HomeKit] hkSetCode("${code}")\n`);
                break;
            }
            case Syscall.HK_ADD: {
                const type = this.pop();
                const nameIdx = this.pop();
                const name = this.constants[nameIdx] || '???';
                const typeNames = {1:'temperature',2:'humidity',3:'light_sensor',4:'battery',5:'contact',6:'switch',7:'outlet',8:'light'};
                this.onOutput(`[HomeKit] hkAdd("${name}", ${typeNames[type]||type})\n`);
                break;
            }
            case Syscall.HK_VAR: {
                const varRef = this.pop();
                this.onOutput(`[HomeKit]   hkVar(globals[${varRef}])\n`);
                break;
            }
            case Syscall.HK_READY: {
                const varRef = this.pop();
                this.push(0);  // always 0 in simulator (no HomeKit)
                break;
            }
            case Syscall.HK_START:
                this.onOutput('[HomeKit] hkStart() — not available in simulator\n');
                this.push(0);
                break;
            case Syscall.HK_INIT: {
                const descRef = this.pop();
                this.onOutput('[HomeKit] hkInit() — not available in simulator\n');
                this.push(-1);
                break;
            }
            case Syscall.HK_STOP:
                this.onOutput('[HomeKit] hkStop()\n');
                break;
            case Syscall.HK_RESET:
                this.onOutput('[HomeKit] hkReset()\n');
                break;

            // ── Matter data-model scripting (simulator stubs) ───────
            // The simulator has no Matter stack; model the data registry just
            // enough that matterSet/matterGet round-trip and the script runs.
            case Syscall.MTR_ADD: {
                const dt = this.pop();
                if (!this._mtrEp) this._mtrEp = 1;
                const ep = this._mtrEp++;
                this.onOutput(`[Matter] matterAdd(0x${(dt>>>0).toString(16)}) -> endpoint ${ep}\n`);
                this.push(ep);
                break;
            }
            case Syscall.MTR_CLUSTER: {
                const cl = this.pop(), ep = this.pop();
                this.onOutput(`[Matter] matterCluster(ep=${ep}, 0x${(cl>>>0).toString(16)})\n`);
                break;
            }
            case Syscall.MTR_ATTR: {
                const ty = this.pop(), at = this.pop(), cl = this.pop(), ep = this.pop();
                this.onOutput(`[Matter] matterAttr(ep=${ep}, cl=0x${(cl>>>0).toString(16)}, attr=0x${(at>>>0).toString(16)}, type=${ty})\n`);
                break;
            }
            case Syscall.MTR_SET: {
                const val = this.pop(), at = this.pop(), cl = this.pop(), ep = this.pop();
                if (!this._mtrVals) this._mtrVals = {};
                this._mtrVals[`${ep}/${cl>>>0}/${at>>>0}`] = val;
                this.onOutput(`[Matter] matterSet(ep=${ep}, cl=0x${(cl>>>0).toString(16)}, attr=0x${(at>>>0).toString(16)}) = ${val}\n`);
                break;
            }
            case Syscall.MTR_GET: {
                const at = this.pop(), cl = this.pop(), ep = this.pop();
                const v = (this._mtrVals && this._mtrVals[`${ep}/${cl>>>0}/${at>>>0}`]) || 0;
                this.push(v);
                break;
            }
            case Syscall.MTR_NAME: {
                const nameRef = this.pop(), ep = this.pop();
                const name = this.readStringFromRef(nameRef);
                this.onOutput(`[Matter] matterName(ep=${ep}, "${name}")\n`);
                break;
            }
            case Syscall.MTR_START:
                this.onOutput('[Matter] matterStart() — not available in simulator\n');
                this.push(0);
                break;
            case Syscall.MTR_RESET:
                this._mtrEp = 1; this._mtrVals = {};
                this.onOutput('[Matter] matterReset()\n');
                break;

            // ── Addressable LED strip (WS2812) ─────────────────────
            case Syscall.WS2812: {
                const offset = this.pop();
                const len = this.pop();
                const arrRef = this.pop();
                this.onOutput(`[WS2812] setPixels(arr, ${len}, offset=${offset})\n`);
                break;
            }
            case Syscall.RGB_LED: {
                const color = this.pop(), gpio = this.pop();
                this.onOutput(`[RGB] rgbLed(gpio=${gpio}, #${(color>>>0).toString(16).padStart(6,'0')})\n`);
                this.push(1);
                break;
            }

            // ── SML descriptor pin substitution (browser stub) ─────
            case Syscall.SML_APPLY_PINS: {
                const smlf  = this.pop();
                const tx    = this.pop();
                const rx    = this.pop();
                const pIdx  = this.pop();
                const path  = this.constants[pIdx] || '';
                this.onOutput(`[SML] smlApplyPins("${path}", rx=${rx}, tx=${tx}, smlf=${smlf}) — simulator stub, returning 0\n`);
                this.push(0);
                break;
            }

            // ── SML mini-scripter load (browser stub) ──────────────
            case Syscall.SML_SCRIPTER_LOAD: {
                const pIdx = this.pop();
                const path = this.constants[pIdx] || '';
                this.onOutput(`[SML] smlScripterLoad("${path}") — simulator stub, returning 0 (no >F/>S extracted)\n`);
                this.push(0);
                break;
            }

            // ── Repo pulldown (Scripter smlpd compatible) ──────────
            case Syscall.WEB_REPO_PULLDOWN: {
                const destIdx  = this.pop();
                const keyIdx   = this.pop();
                const urlIdx   = this.pop();
                const labelIdx = this.pop();
                const gref     = this.pop();
                const resolved = this.resolveRef(gref);
                const val   = resolved.arr[resolved.base];
                const label = this.constants[labelIdx] || '';
                const url   = this.constants[urlIdx]   || '';
                const key   = this.constants[keyIdx]   || 'files';
                const dest  = this.constants[destIdx]  || '';
                this.onOutput(`[WebUI] RepoPulldown "${label}": sel=${val} url="${url}" key="${key}" dest="${dest}" (gidx=${resolved.base})\n`);
                break;
            }

            default:
                throw new VMError(`Unknown syscall: ${id} (${SyscallName[id] || '?'})`, this.pc);
        }
    }

    // ─── Persist variables (save/load to virtual filesystem) ──

    persistSave() {
        if (this.persistEntries.length === 0) return;
        // Binary format: ['P','V'] [count u8] [index u16 LE, slotCount u16 LE, data: slotCount×4 bytes LE] × count
        const parts = [];
        parts.push(0x50, 0x56); // 'P', 'V'
        parts.push(this.persistEntries.length);
        for (const entry of this.persistEntries) {
            parts.push(entry.index & 0xFF, (entry.index >> 8) & 0xFF);           // index LE
            parts.push(entry.slotCount & 0xFF, (entry.slotCount >> 8) & 0xFF);   // slotCount LE
            for (let s = 0; s < entry.slotCount; s++) {
                const val = this.globals[entry.index + s];
                // Write as int32 LE
                parts.push(val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, (val >> 24) & 0xFF);
            }
        }
        this.virtualFS.set('/tinyc_pvars.bin', new Uint8Array(parts));
        this.onOutput('[PERSIST] saveVars: saved ' + this.persistEntries.length + ' entries\n');
    }

    persistLoad() {
        if (this.persistEntries.length === 0) return;
        const data = this.virtualFS.get('/tinyc_pvars.bin');
        if (!data || data.length < 3) return;
        if (data[0] !== 0x50 || data[1] !== 0x56) return; // 'P','V' magic
        const count = data[2];
        let pos = 3;
        for (let i = 0; i < count && pos < data.length; i++) {
            const index = data[pos] | (data[pos + 1] << 8);
            pos += 2;
            const slotCount = data[pos] | (data[pos + 1] << 8);
            pos += 2;
            // Find matching persist entry and restore
            for (const entry of this.persistEntries) {
                if (entry.index === index) {
                    const slots = Math.min(slotCount, entry.slotCount);
                    for (let s = 0; s < slots; s++) {
                        const val = data[pos + s * 4] |
                                    (data[pos + s * 4 + 1] << 8) |
                                    (data[pos + s * 4 + 2] << 16) |
                                    (data[pos + s * 4 + 3] << 24);
                        this.globals[entry.index + s] = val;
                    }
                    break;
                }
            }
            pos += slotCount * 4;
        }
        this.onOutput('[PERSIST] loadVars: restored from /tinyc_pvars.bin\n');
    }

    // ─── Debug / Inspection ──────────────────────────────────

    dumpState() {
        let out = '=== VM State ===\n';
        out += `PC: ${this.pc - this.codeOffset}  SP: ${this.sp}  FP: ${this.fp}\n`;
        out += `Stack: [${Array.from(this.stack.slice(0, this.sp)).join(', ')}]\n`;
        out += `Frames: ${this.frames.length}\n`;
        out += `Instructions: ${this.instructionCount}\n`;
        this.onOutput(out);
    }

    getState() {
        return {
            pc: this.pc - this.codeOffset,
            sp: this.sp,
            fp: this.fp,
            stack: Array.from(this.stack.slice(0, this.sp)),
            globals: Array.from(this.globals.slice(0, 16)),
            frames: this.frames.length,
            halted: this.halted,
            instructionCount: this.instructionCount,
        };
    }

    getCurrentLine() {
        const codePC = this.pc - this.codeOffset;
        let line = 0;
        for (const entry of this.sourceMap) {
            if (entry.offset <= codePC) {
                line = entry.line;
            } else {
                break;
            }
        }
        return line;
    }

    // ─── Disassembler ────────────────────────────────────────

    disassemble(compiled) {
        const { binary, codeOffset, codeSize } = compiled;
        const lines = [];
        let pc = codeOffset;

        while (pc < codeOffset + codeSize) {
            const addr = pc - codeOffset;
            const op = binary[pc++];
            const name = OpName[op] || `??? (0x${op.toString(16)})`;

            let operand = '';
            switch (op) {
                case Op.PUSH_I32:
                    operand = new DataView(binary.buffer, binary.byteOffset + pc, 4).getInt32(0, false).toString();
                    pc += 4;
                    break;
                case Op.PUSH_F32:
                    operand = new DataView(binary.buffer, binary.byteOffset + pc, 4).getFloat32(0, false).toFixed(4);
                    pc += 4;
                    break;
                case Op.PUSH_I8:
                    operand = (binary[pc] > 127 ? binary[pc] - 256 : binary[pc]).toString();
                    pc += 1;
                    break;
                case Op.PUSH_I16:
                    operand = ((binary[pc] << 8) | binary[pc + 1]).toString();
                    pc += 2;
                    break;
                case Op.LOAD_LOCAL:
                case Op.STORE_LOCAL:
                case Op.LOAD_LOCAL_ARR:
                case Op.STORE_LOCAL_ARR:
                case Op.ADDR_LOCAL:
                case Op.LOAD_HEAP_ARR:
                case Op.STORE_HEAP_ARR:
                case Op.ADDR_HEAP:
                case Op.ADDR_HEAP_OFF:
                case Op.LOAD_REF_ARR:
                case Op.STORE_REF_ARR:
                    operand = `[${binary[pc]}]`;
                    pc += 1;
                    break;
                case Op.LOAD_GLOBAL:
                case Op.STORE_GLOBAL:
                case Op.LOAD_GLOBAL_ARR:
                case Op.STORE_GLOBAL_ARR:
                case Op.ADDR_GLOBAL:
                    operand = `[${(binary[pc] << 8) | binary[pc + 1]}]`;
                    pc += 2;
                    break;
                case Op.STORE_GLOBAL_UDP:
                    operand = `global[${(binary[pc] << 8) | binary[pc + 1]}] name_const[${(binary[pc + 2] << 8) | binary[pc + 3]}]`;
                    pc += 4;
                    break;
                case Op.STORE_WATCH:
                    operand = `var[${(binary[pc] << 8) | binary[pc + 1]}] shadow[${(binary[pc + 2] << 8) | binary[pc + 3]}] written[${(binary[pc + 4] << 8) | binary[pc + 5]}]`;
                    pc += 6;
                    break;
                case Op.JMP:
                case Op.JZ:
                case Op.JNZ:
                case Op.CALL:
                    operand = `@${(binary[pc] << 8) | binary[pc + 1]}`;
                    pc += 2;
                    break;
                case Op.SYSCALL:
                    operand = SyscallName[binary[pc]] || `#${binary[pc]}`;
                    pc += 1;
                    break;
                case Op.SYSCALL2: {
                    // u16 syscall id (extended range 256+). Without this case
                    // the disassembler would walk past the operand and decode
                    // the operand bytes as opcodes — that's the cause of the
                    // stray "??? (0xNN)" lines in large scripts (bat_ctrl.tc).
                    const id2 = (binary[pc] << 8) | binary[pc + 1];
                    operand = SyscallName[id2] || `#${id2}`;
                    pc += 2;
                    break;
                }
                case Op.LOAD_CONST:
                    operand = `const[${(binary[pc] << 8) | binary[pc + 1]}]`;
                    pc += 2;
                    break;
                // No-operand opcodes: nothing to consume. Listing them here
                // makes the disassembler's intent explicit and lets the
                // default branch flag genuine unmapped opcodes loudly.
                case Op.NOP: case Op.HALT: case Op.POP: case Op.DUP:
                case Op.ADD: case Op.SUB: case Op.MUL: case Op.DIV:
                case Op.MOD: case Op.NEG:
                case Op.FADD: case Op.FSUB: case Op.FMUL: case Op.FDIV:
                case Op.FNEG:
                case Op.BIT_AND: case Op.BIT_OR: case Op.BIT_XOR:
                case Op.BIT_NOT: case Op.SHL: case Op.SHR:
                case Op.EQ: case Op.NEQ: case Op.LT: case Op.GT:
                case Op.LTE: case Op.GTE:
                case Op.FEQ: case Op.FNEQ: case Op.FLT: case Op.FGT:
                case Op.FLTE: case Op.FGTE:
                case Op.LOGIC_AND: case Op.LOGIC_OR: case Op.LOGIC_NOT:
                case Op.RET: case Op.RET_VAL:
                case Op.I2F: case Op.F2I: case Op.I2C:
                    break;
                default:
                    // Genuinely unmapped opcode — flag it clearly.
                    operand = '<-- unknown opcode, may be operand byte';
                    break;
            }

            lines.push(`${String(addr).padStart(5, '0')}  ${name.padEnd(18)} ${operand}`);
        }

        return lines.join('\n');
    }
}

