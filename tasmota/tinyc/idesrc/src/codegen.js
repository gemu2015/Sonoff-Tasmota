// TinyC Code Generator - AST to Bytecode
// Walks AST and emits stack-based bytecode

import { NodeType } from './parser.js';
import { Op, Syscall, MAGIC, VERSION } from './opcodes.js';

export class CodeGenError extends Error {
    constructor(message, line) {
        super(`CodeGen error at line ${line}: ${message}`);
        this.line = line;
    }
}

// Well-known callback function names — auto-detected by compiler
const CALLBACK_NAMES = ['JsonCall', 'WebCall', 'WebPage', 'WebUI', 'WebOn', 'EverySecond', 'Every100ms', 'Every50ms', 'EveryLoop', 'UdpCall', 'TaskLoop', 'CleanUp', 'TouchButton', 'HomeKitWrite', 'Event', 'OnExit', 'Command', 'OnMqttConnect', 'OnMqttDisconnect', 'OnWifiConnect', 'OnWifiDisconnect', 'OnTimeSet', 'OnInit', 'BootInit'];

// Built-in functions mapped to syscalls
const BUILTINS = {
    // GPIO
    'pinMode':          { syscall: Syscall.PIN_MODE,        args: 2, returns: false },
    'digitalWrite':     { syscall: Syscall.DIGITAL_WRITE,   args: 2, returns: false },
    'dmxWrite':         { syscall: Syscall.DMX_WRITE,       args: 2, returns: false },
    'digitalRead':      { syscall: Syscall.DIGITAL_READ,    args: 1, returns: true },
    'analogRead':       { syscall: Syscall.ANALOG_READ,     args: 1, returns: true },
    'analogWrite':      { syscall: Syscall.ANALOG_WRITE,    args: 2, returns: false },
    'gpioInit':         { syscall: Syscall.GPIO_INIT,       args: 2, returns: false },
    'pinFree':          { syscall: Syscall.PIN_FREE,        args: 1, returns: true },
    // 1-Wire (native C bit-bang)
    'owSetPin':         { syscall: Syscall.OW_SET_PIN,     args: 1, returns: false },
    'owReset':          { syscall: Syscall.OW_RESET,       args: 0, returns: true },
    'owWrite':          { syscall: Syscall.OW_WRITE,       args: 1, returns: false },
    'owRead':           { syscall: Syscall.OW_READ,        args: 0, returns: true },
    'owWriteBit':       { syscall: Syscall.OW_WRITE_BIT,   args: 1, returns: false },
    'owReadBit':        { syscall: Syscall.OW_READ_BIT,    args: 0, returns: true },
    'owSearchReset':    { syscall: Syscall.OW_SEARCH_RESET, args: 0, returns: false },
    'owSearch':         { syscall: Syscall.OW_SEARCH,      args: 1, returns: true, strArgs: [0] },

    // Timing
    'delay':            { syscall: Syscall.DELAY,           args: 1, returns: false },
    'delayMicroseconds':{ syscall: Syscall.DELAY_MICRO,     args: 1, returns: false },
    'millis':           { syscall: Syscall.MILLIS,          args: 0, returns: true },
    'micros':           { syscall: Syscall.MICROS,          args: 0, returns: true },
    'vmStackDepth':     { syscall: Syscall.VM_STACK_DEPTH,   args: 0, returns: true },
    'timerStart':       { syscall: Syscall.TIMER_START,     args: 2, returns: false },
    'timerDone':        { syscall: Syscall.TIMER_DONE,      args: 1, returns: true  },
    'timerStop':        { syscall: Syscall.TIMER_STOP,      args: 1, returns: false },
    'timerRemaining':   { syscall: Syscall.TIMER_REMAINING, args: 1, returns: true  },

    // Serial — all functions take handle (int) as first argument; serialBegin() returns handle (0..2) or -1
    'serialBegin':      { syscall: Syscall.SERIAL_BEGIN,       args: 5, returns: true },
    'serialClose':      { syscall: Syscall.SERIAL_CLOSE,       args: 1, returns: false },
    'serialPrint':      { syscall: Syscall.SERIAL_PRINT,       args: 2, returns: false },
    'serialPrintInt':   { syscall: Syscall.SERIAL_PRINT_INT,   args: 2, returns: false },
    'serialPrintFloat': { syscall: Syscall.SERIAL_PRINT_FLOAT, args: 2, returns: false },
    'serialPrintln':    { syscall: Syscall.SERIAL_PRINTLN,     args: 2, returns: false },
    'serialRead':       { syscall: Syscall.SERIAL_READ,        args: 1, returns: true },
    'serialAvailable':  { syscall: Syscall.SERIAL_AVAILABLE,   args: 1, returns: true },
    'serialWriteByte':  { syscall: Syscall.SERIAL_WRITE_BYTE,  args: 2, returns: false },
    'serialWrite':      { syscall: Syscall.SERIAL_WRITE_STR,   args: 2, returns: false, strArgs: [1] },
    'serialWriteBytes': { syscall: Syscall.SERIAL_WRITE_BUF,   args: 3, returns: false, strArgs: [1] },

    // Math
    'abs':              { syscall: Syscall.MATH_ABS,        args: 1, returns: true },
    'min':              { syscall: Syscall.MATH_MIN,        args: 2, returns: true },
    'max':              { syscall: Syscall.MATH_MAX,        args: 2, returns: true },
    'map':              { syscall: Syscall.MATH_MAP,        args: 5, returns: true },
    'random':           { syscall: Syscall.MATH_RANDOM,     args: 2, returns: true },
    'sqrt':             { syscall: Syscall.MATH_SQRT,       args: 1, returns: true },
    'sin':              { syscall: Syscall.MATH_SIN,        args: 1, returns: true },
    'cos':              { syscall: Syscall.MATH_COS,        args: 1, returns: true },
    'floor':            { syscall: Syscall.MATH_FLOOR,      args: 1, returns: true },
    'ceil':             { syscall: Syscall.MATH_CEIL,       args: 1, returns: true },
    'round':            { syscall: Syscall.MATH_ROUND,      args: 1, returns: true },
    'exp':              { syscall: Syscall.MATH_EXP,        args: 1, returns: true },
    'log':              { syscall: Syscall.MATH_LOG,        args: 1, returns: true },
    'pow':              { syscall: Syscall.MATH_POW,        args: 2, returns: true },
    'acos':             { syscall: Syscall.MATH_ACOS,       args: 1, returns: true },
    'intBitsToFloat':   { syscall: Syscall.INT_BITS_TO_FLOAT, args: 1, returns: true },

    // String operations (take array refs, handled specially in compileCallExpr)
    'strlen':           { syscall: Syscall.STRLEN,          args: 1, returns: true,  strArgs: [0] },
    'strcpy':           { syscall: Syscall.STRCPY,          args: 2, returns: false, strArgs: [0, 1] },
    'strcat':           { syscall: Syscall.STRCAT,          args: 2, returns: false, strArgs: [0, 1] },
    'strcmp':            { syscall: Syscall.STRCMP,           args: 2, returns: true,  strArgs: [0, 1] },
    'printString':      { syscall: Syscall.STR_PRINT,       args: 1, returns: false, strArgs: [0] },

    // Tasmota command
    'tasmCmd':          { syscall: Syscall.TASM_CMD,        args: 2, returns: true,  constArgs: [0], strArgs: [1] },
    'tasmDefer':        { syscall: Syscall.TASM_DEFER,      args: 1, returns: false, strArgs: [0] },

    // Tasmota output (for callbacks — route directly to Tasmota APIs)
    'addLog':           { syscall: Syscall.LOG,              args: 1, returns: false, strArgs: [0] },
    'addLogLevel':      { syscall: Syscall.LOG_LEVEL,        args: 2, returns: false, strArgs: [1] },
    'responseAppend':   { syscall: Syscall.RESPONSE_APPEND, args: 1, returns: false, strArgs: [0] },
    'webSend':          { syscall: Syscall.WEB_SEND,        args: 1, returns: false, strArgs: [0] },
    'webFlush':         { syscall: Syscall.WEB_FLUSH,       args: 0, returns: false },

    // Localized strings
    'LGetString':       { syscall: Syscall.LGETSTRING,      args: 2, returns: true,  strArgs: [1] },

    // UDP multicast (Scripter-compatible) — scalar float auto-sends via STORE_GLOBAL_UDP opcode
    'udpSendArray':     { syscall: Syscall.UDP_SEND_ARRAY,  args: 3, returns: false, constArgs: [0], strArgs: [1] },
    'udpSendStr':       { syscall: Syscall.UDP_SEND_STR,   args: 2, returns: false, constArgs: [0], strArgs: [1] },
    // udpSend / udpRecv / udpReady / udpRecvArray — Scripter-style named-variable
    // accessors documented in TinyC_Reference.md §UDP. Firmware syscalls existed
    // (100/101/102/104) but BUILTINS were missing until 2026-05-14.
    'udpSend':          { syscall: Syscall.UDP_SEND,        args: 2, returns: false, constArgs: [0] },
    'udpRecv':          { syscall: Syscall.UDP_RECV,        args: 1, returns: true,  constArgs: [0], returnFloat: true },
    'udpReady':         { syscall: Syscall.UDP_READY,       args: 1, returns: true,  constArgs: [0] },
    'udpRecvArray':     { syscall: Syscall.UDP_RECV_ARRAY,  args: 3, returns: true,  constArgs: [0] },

    // I2C bus (last param = bus: 0 or 1)
    'i2cRead8':         { syscall: Syscall.I2C_READ8,      args: 3, returns: true },
    'i2cWrite8':        { syscall: Syscall.I2C_WRITE8,     args: 4, returns: true },
    'i2cRead':          { syscall: Syscall.I2C_READ_BUF,   args: 5, returns: true,  strArgs: [2] },
    'i2cWrite':         { syscall: Syscall.I2C_WRITE_BUF,  args: 5, returns: true,  strArgs: [2] },
    'i2cExists':        { syscall: Syscall.I2C_EXISTS,     args: 2, returns: true },
    'i2cSetDevice':     { syscall: Syscall.I2C_SET_DEVICE, args: 2, returns: true },
    'i2cSetActiveFound': { syscall: Syscall.I2C_SET_FOUND, args: 3, returns: false, constArgs: [1] },
    'i2cReadRS':        { syscall: Syscall.I2C_READ_RS,   args: 5, returns: true,  strArgs: [2] },
    'i2cRead0':         { syscall: Syscall.I2C_READ_BUF0, args: 4, returns: true,  strArgs: [1] },
    'i2cWrite0':        { syscall: Syscall.I2C_WRITE0,    args: 3, returns: true },
    'I2cResetActive':   { syscall: Syscall.I2C_FREE,     args: 2, returns: false },

    // Console command callback
    'addCommand':       { syscall: Syscall.ADD_COMMAND,    args: 1, returns: false, constArgs: [0] },
    'responseCmnd':     { syscall: Syscall.RESPONSE_CMND,  args: 1, returns: false, strArgs: [0] },

    // SPI bus
    'spiInit':          { syscall: Syscall.SPI_INIT,        args: 4, returns: true },
    'spiSetCS':         { syscall: Syscall.SPI_SET_CS,      args: 2, returns: false },
    'spiTransfer':      { syscall: Syscall.SPI_TRANSFER,    args: 4, returns: true,  strArgs: [1] },

    // Smart Meter (SML)
    'smlGet':           { syscall: Syscall.SML_GET,         args: 1, returns: true },
    'smlGetStr':        { syscall: Syscall.SML_GETSTR,      args: 2, returns: true,  strArgs: [1] },
    'smlWrite':         { syscall: Syscall.SML_WRITE,       args: 2, returns: true,  strArgs: [1] },
    'smlRead':          { syscall: Syscall.SML_READ,        args: 2, returns: true,  strArgs: [1] },
    'smlSetBaud':       { syscall: Syscall.SML_SETBAUD,     args: 2, returns: true },
    'smlSetWStr':       { syscall: Syscall.SML_SETWSTR,     args: 2, returns: true,  strArgs: [1] },
    'smlSetOptions':    { syscall: Syscall.SML_SETOPT,      args: 1, returns: true },
    'smlGetV':          { syscall: Syscall.SML_GETV,        args: 1, returns: true },

    // TCP server
    'tcpServer':        { syscall: Syscall.TCP_OPEN,        args: 1, returns: true },
    'tcpClose':         { syscall: Syscall.TCP_CLOSE,       args: 0, returns: false },
    'tcpAvailable':     { syscall: Syscall.TCP_AVAILABLE,   args: 0, returns: true },
    'tcpRead':          { syscall: Syscall.TCP_READ_STR,    args: 1, returns: true,  strArgs: [0] },
    'tcpWrite':         { syscall: Syscall.TCP_WRITE_STR,   args: 1, returns: false, strArgs: [0] },
    'tcpReadArray':     { syscall: Syscall.TCP_READ_ARR,    args: 1, returns: true,  strArgs: [0] },
    // tcpWriteArray handled as special case in compileCallExpr (optional type arg)

    // TCP outgoing client (multi-slot — pick slot with tcpSelect, then connect/disconnect)
    // tcpConnect handled as special case in compileCallExpr — string literal → TCP_CONNECT,
    // char[] runtime arg → TCP_CONNECT_REF (auto-routed by arg type, like tcpWriteArray).
    'tcpDisconnect':    { syscall: Syscall.TCP_DISCONNECT,  args: 0, returns: false },
    'tcpConnected':     { syscall: Syscall.TCP_CONNECTED,   args: 0, returns: true  },
    'tcpSelect':        { syscall: Syscall.TCP_SELECT,      args: 1, returns: false },
    // Per-slot TCP tuning (v1.5.1). Operate on the currently selected slot.
    'tcpKeepalive':         { syscall: Syscall.TCP_KEEPALIVE,         args: 3, returns: true  },
    'tcpNoDelay':           { syscall: Syscall.TCP_NODELAY,           args: 1, returns: false },
    'tcpDisconnectReason':  { syscall: Syscall.TCP_DISCONNECT_REASON, args: 0, returns: true  },
    // Atomic write-and-await-reply on the selected slot (v1.6.0). Args 0 and 2
    // are char[] refs (request bytes in, response bytes out). Args 1 (req_len),
    // 3 (resp_max), 4 (timeout_ms) are plain ints.
    'tcpTransact':          { syscall: Syscall.TCP_TRANSACT,          args: 5, returns: true,
                              strArgs: [0, 2] },

    // Binary library invocation. constArgs[0] requires the name to be a
    // string literal at the call site; it gets pushed as a const-pool
    // index that the firmware resolves to the registered fn pointer.
    'bcall':                { syscall: Syscall.BLIB_CALL,             args: 3, returns: true,
                              constArgs: [0], strArgs: [1] },

    // TWAI / CAN-bus syscalls.
    'twaiBegin':            { syscall: Syscall.TWAI_BEGIN,             args: 4, returns: true },
    'twaiEnd':              { syscall: Syscall.TWAI_END,               args: 0, returns: false },
    'twaiAvailable':        { syscall: Syscall.TWAI_AVAILABLE,         args: 0, returns: true },
    'twaiRecv':             { syscall: Syscall.TWAI_RECV,              args: 3, returns: true,
                              strArgs: [0, 1] },          // meta_arr, data_buf
    'twaiSend':             { syscall: Syscall.TWAI_SEND,              args: 4, returns: true,
                              strArgs: [3] },             // data_buf
    'twaiStatus':           { syscall: Syscall.TWAI_STATUS,            args: 1, returns: true,
                              strArgs: [0] },             // stats_arr
    'twaiFilter':           { syscall: Syscall.TWAI_FILTER,            args: 3, returns: true },

    // WebOn raw response + keep-alive controls.
    'webRawMode':           { syscall: Syscall.WEB_RAW_MODE,           args: 0, returns: false },
    'webRawWrite':          { syscall: Syscall.WEB_RAW_WRITE,          args: 1, returns: false,
                              strArgs: [0] },             // str_ref
    'webKeepAlive':         { syscall: Syscall.WEB_KEEP_ALIVE,         args: 0, returns: false },

    // spawnTask / killTask / taskRunning all handled as special cases in compileCallExpr —
    // string literal → emits syscall AND registers the name in the function table
    // (so the firmware can resolve it from vm->callbacks[]).

    // File I/O (constArgs = string literal → constant pool index, strArgs = char array ref)
    'fileOpen':         { syscall: Syscall.FILE_OPEN,       args: 2, returns: true,  constArgs: [0] },
    'fileClose':        { syscall: Syscall.FILE_CLOSE,      args: 1, returns: true },
    'fileRead':         { syscall: Syscall.FILE_READ,       args: 3, returns: true,  strArgs: [1] },
    'fileWrite':        { syscall: Syscall.FILE_WRITE,      args: 3, returns: true,  strArgs: [1] },
    // Binary array I/O — moves count int32 elements (4-byte LE) between
    // the int[]/float[] array and the file. The same syscall works for
    // both types since both are int32 in memory; the script knows the
    // type, the syscall just moves bits.
    'fileReadBin':      { syscall: Syscall.FILE_READ_BIN,   args: 3, returns: true,  strArgs: [1] },
    'fileWriteBin':     { syscall: Syscall.FILE_WRITE_BIN,  args: 3, returns: true,  strArgs: [1] },
    'fileExists':       { syscall: Syscall.FILE_EXISTS,     args: 1, returns: true,  constArgs: [0] },
    'fileDelete':       { syscall: Syscall.FILE_DELETE,     args: 1, returns: true,  constArgs: [0] },
    'fileSize':         { syscall: Syscall.FILE_SIZE,       args: 1, returns: true,  constArgs: [0] },
    'fileFormat':       { syscall: Syscall.FILE_FORMAT,    args: 0, returns: true },
    'fileMkdir':        { syscall: Syscall.FILE_MKDIR,     args: 1, returns: true,  constArgs: [0] },
    'fileRmdir':        { syscall: Syscall.FILE_RMDIR,     args: 1, returns: true,  constArgs: [0] },
    'fileSeek':         { syscall: Syscall.FILE_SEEK,      args: 3, returns: true },
    'fileTell':         { syscall: Syscall.FILE_TELL,      args: 1, returns: true },
    'fileReadArray':    { syscall: Syscall.FILE_READ_ARR,  args: 2, returns: true,  strArgs: [0] },
    // fileWriteArray handled as special case in compileCallExpr (optional append arg)
    'fileLog':          { syscall: Syscall.FILE_LOG,       args: 3, returns: true,  constArgs: [0], strArgs: [1] },
    'fileDownload':     { syscall: Syscall.FILE_DOWNLOAD, args: 2, returns: true,  constArgs: [0], strArgs: [1] },
    'fileGetStr':       { syscall: Syscall.FILE_GET_STR, args: 5, returns: true,  strArgs: [0], constArgs: [2] },
    'fileOpenDir':      { syscall: Syscall.FILE_OPENDIR,   args: 1, returns: true,  constArgs: [0] },
    'fileReadDir':      { syscall: Syscall.FILE_READDIR,   args: 2, returns: true,  strArgs: [1] },
    'fileRange':        { syscall: Syscall.FILE_RANGE,     args: 3, returns: true,  strArgs: [1, 2] },
    'fsInfo':           { syscall: Syscall.FS_INFO,        args: 1, returns: true },

    // Time / timestamp
    'timeStamp':        { syscall: Syscall.TIME_STAMP,    args: 1, returns: true,  strArgs: [0] },
    'timeConvert':      { syscall: Syscall.TIME_CONVERT,  args: 2, returns: true,  strArgs: [0] },
    // timeOffset handled as special case in compileCallExpr (optional zeroFlag arg)
    'timeToSecs':       { syscall: Syscall.TIME_TO_SECS,  args: 1, returns: true,  strArgs: [0] },
    'secsToTime':       { syscall: Syscall.SECS_TO_TIME,  args: 2, returns: true,  strArgs: [0] },

    // sprintf variants — format a single value into a char array
    'sprintfInt':       { syscall: Syscall.SPRINTF_INT,     args: 3, returns: true,  strArgs: [0], constArgs: [1], intArgs: [2] },
    'sprintfFloat':     { syscall: Syscall.SPRINTF_FLT,     args: 3, returns: true,  strArgs: [0], constArgs: [1] },
    'sprintfStr':       { syscall: Syscall.SPRINTF_STR,     args: 3, returns: true,  strArgs: [0, 2], constArgs: [1] },
    // sprintf append variants — append formatted text to existing string in dst
    'sprintfAppendInt':   { syscall: Syscall.SPRINTF_INT_CAT, args: 3, returns: true,  strArgs: [0], constArgs: [1], intArgs: [2] },
    'sprintfAppendFloat': { syscall: Syscall.SPRINTF_FLT_CAT, args: 3, returns: true,  strArgs: [0], constArgs: [1] },
    'sprintfAppendStr':   { syscall: Syscall.SPRINTF_STR_CAT, args: 3, returns: true,  strArgs: [0, 2], constArgs: [1] },

    // String manipulation
    'strToken':         { syscall: Syscall.STR_TOKEN,       args: 4, returns: true,  strArgs: [0, 1] },
    'strSub':           { syscall: Syscall.STR_SUB,         args: 4, returns: true,  strArgs: [0, 1] },
    'strFind':          { syscall: Syscall.STR_FIND,        args: 2, returns: true,  strArgs: [0, 1] },

    // ── 1.5.0 string ops (literal-needle / in-place) ──
    'strReplace':       { syscall: Syscall.STR_REPLACE_CONST,  args: 3, returns: true,  strArgs: [0], constArgs: [1, 2] },
    'strStartsWith':    { syscall: Syscall.STR_STARTS_CONST,   args: 2, returns: true,  strArgs: [0], constArgs: [1] },
    'strEndsWith':      { syscall: Syscall.STR_ENDS_CONST,     args: 2, returns: true,  strArgs: [0], constArgs: [1] },
    'strContains':      { syscall: Syscall.STR_CONTAINS_CONST, args: 2, returns: true,  strArgs: [0], constArgs: [1] },
    'strToUpper':       { syscall: Syscall.STR_TO_UPPER,       args: 1, returns: false, strArgs: [0] },
    'strToLower':       { syscall: Syscall.STR_TO_LOWER,       args: 1, returns: false, strArgs: [0] },
    'strTrim':          { syscall: Syscall.STR_TRIM,           args: 1, returns: true,  strArgs: [0] },
    'atoi':             { syscall: Syscall.STR_TO_INT,      args: 1, returns: true,  strArgs: [0] },
    'atof':             { syscall: Syscall.STR_TO_FLOAT,    args: 1, returns: true,  strArgs: [0], returnFloat: true },
    // strToInt / strToFloat are documented in TinyC_Reference.md as aliases for
    // atoi / atof — both names route to the same syscalls so doc + code agree.
    'strToInt':         { syscall: Syscall.STR_TO_INT,      args: 1, returns: true,  strArgs: [0] },
    'strToFloat':       { syscall: Syscall.STR_TO_FLOAT,    args: 1, returns: true,  strArgs: [0], returnFloat: true },

    // Sensor JSON parsing
    'sensorGet':        { syscall: Syscall.SENSOR_GET,      args: 1, returns: true,  constArgs: [0] },

    // Cross-VM shared key/value store — let two slots share named scalars/strings
    'shareSetInt':      { syscall: Syscall.SHARE_SET_INT,   args: 2, returns: false, constArgs: [0], intArgs: [1] },
    'shareGetInt':      { syscall: Syscall.SHARE_GET_INT,   args: 1, returns: true,  constArgs: [0] },
    'shareSetFloat':    { syscall: Syscall.SHARE_SET_FLT,   args: 2, returns: false, constArgs: [0] },
    'shareGetFloat':    { syscall: Syscall.SHARE_GET_FLT,   args: 1, returns: true,  constArgs: [0], returnFloat: true },
    'shareSetStr':      { syscall: Syscall.SHARE_SET_STR,   args: 2, returns: false, constArgs: [0], strArgs: [1] },
    'shareGetStr':      { syscall: Syscall.SHARE_GET_STR,   args: 2, returns: true,  constArgs: [0], strArgs: [1] },
    'shareHas':         { syscall: Syscall.SHARE_HAS,       args: 1, returns: true,  constArgs: [0] },
    'shareDelete':      { syscall: Syscall.SHARE_DELETE,    args: 1, returns: true,  constArgs: [0] },
    'shareDump':        { syscall: Syscall.SHARE_DUMP,      args: 0, returns: true },
    'dumpPersist':      { syscall: Syscall.PERSIST_DUMP,    args: 0, returns: true },
    'uiScreen':         { syscall: Syscall.UI_SCREEN,       args: 1, returns: false },
    'uiTheme':          { syscall: Syscall.UI_THEME,        args: 4, returns: false },
    'uiClearScreen':    { syscall: Syscall.UI_CLEAR_SCREEN, args: 0, returns: false },
    'uiLabel':          { syscall: Syscall.UI_LABEL,        args: 7, returns: false, constArgs: [5] },
    'uiLabelSet':       { syscall: Syscall.UI_LABEL_SET,    args: 2, returns: false, strArgs: [1] },
    'uiCheckbox':       { syscall: Syscall.UI_CHECKBOX,     args: 6, returns: false, constArgs: [5] },
    'uiProgress':       { syscall: Syscall.UI_PROGRESS,     args: 7, returns: false },
    'uiProgressSet':    { syscall: Syscall.UI_PROGRESS_SET, args: 2, returns: false },
    'uiGauge':          { syscall: Syscall.UI_GAUGE,        args: 7, returns: false },
    'uiIcon':           { syscall: Syscall.UI_ICON,         args: 4, returns: false },
    'uiButton':         { syscall: Syscall.UI_BUTTON,       args: 6, returns: false, constArgs: [5] },

    // Symmetric crypto (ESP32-only via mbedtls). Buffers must be `char[]` of
    // sufficient capacity. AES key length fixed at 128 bits (16 bytes).
    //   aesEcb(key, data, encrypt)        — single 16-byte block, in-place
    //   aesCbc(key, iv, data, len, enc)   — multi-block, in-place; iv updated
    //   hmacSha256(key,klen,data,dlen,out32) — HMAC into 32-byte out buffer
    //   sha256(data, dlen, out32)         — plain SHA-256 into 32-byte out
    //   hex2bin(hex, hex_len, out)        — hex chars → bytes; -1 on bad nibble
    //   bin2hex(bin, bin_len, out)        — bytes → lowercase hex; NUL-terminated
    'aesEcb':           { syscall: Syscall.AES_ECB,         args: 3, returns: true,  strArgs: [0, 1], intArgs: [2] },
    'aesCbc':           { syscall: Syscall.AES_CBC,         args: 5, returns: true,  strArgs: [0, 1, 2], intArgs: [3, 4] },
    'hmacSha256':       { syscall: Syscall.HMAC_SHA256,     args: 5, returns: true,  strArgs: [0, 2, 4], intArgs: [1, 3] },
    'sha256':           { syscall: Syscall.SHA256,          args: 3, returns: true,  strArgs: [0, 2], intArgs: [1] },
    // hex2bin's first arg can be a string LITERAL (constArgs) OR a char[] (strArgs).
    // The compiler resolves which based on what the caller passed; the VM-side
    // tc_is_const_ref() check distinguishes at runtime. Listing both lets the
    // type-checker accept either form without error.
    'hex2bin':          { syscall: Syscall.HEX2BIN,         args: 3, returns: true,  strArgs: [0, 2], intArgs: [1] },
    'bin2hex':          { syscall: Syscall.BIN2HEX,         args: 3, returns: true,  strArgs: [0, 2], intArgs: [1] },

    // HTTP
    'httpGet':          { syscall: Syscall.HTTP_GET,         args: 2, returns: true,  strArgs: [0, 1] },
    'httpPost':         { syscall: Syscall.HTTP_POST,        args: 3, returns: true,  strArgs: [0, 1, 2] },
    'httpHeader':       { syscall: Syscall.HTTP_HEADER,      args: 2, returns: false, strArgs: [0, 1] },
    'webParse':         { syscall: Syscall.WEB_PARSE,        args: 4, returns: true,  strArgs: [0, 3], constArgs: [1] },

    // MQTT — firmware has handlers gated on USE_MQTT; CLAUDE.md + Reference.md
    // documented them, but the BUILTINS entries were missing until 2026-05-14.
    // All take const-pool string args.
    'mqttSubscribe':    { syscall: Syscall.MQTT_SUBSCRIBE,   args: 1, returns: true,  constArgs: [0] },
    'mqttUnsubscribe':  { syscall: Syscall.MQTT_UNSUBSCRIBE, args: 1, returns: true,  constArgs: [0] },
    'mqttPublish':      { syscall: Syscall.MQTT_PUBLISH_TO,  args: 2, returns: true,  constArgs: [0, 1] },

    // WebUI widgets (refArgs[0] = variable ref for binding, strArgs = label/options)
    'webButton':        { syscall: Syscall.WEB_BUTTON,      args: 2, returns: false, refArgs: [0], constArgs: [1] },
    'webToggle':        { syscall: Syscall.WEB_TOGGLE,      args: 2, returns: false, refArgs: [0], constArgs: [1] },
    'webSlider':        { syscall: Syscall.WEB_SLIDER,      args: 4, returns: false, refArgs: [0], constArgs: [3] },
    'webCheckbox':      { syscall: Syscall.WEB_CHECKBOX,    args: 2, returns: false, refArgs: [0], constArgs: [1] },
    'webText':          { syscall: Syscall.WEB_TEXT,         args: 3, returns: false, refArgs: [0], constArgs: [2] },
    'webNumber':        { syscall: Syscall.WEB_NUMBER,      args: 4, returns: false, refArgs: [0], constArgs: [3] },
    'webPulldown':      { syscall: Syscall.WEB_PULLDOWN,    args: 3, returns: false, refArgs: [0], constArgs: [1, 2] },
    'webRepoPulldown':  { syscall: Syscall.WEB_REPO_PULLDOWN, args: 5, returns: false, refArgs: [0], constArgs: [1, 2, 3, 4] },
    // smlApplyPins("/path", rx, tx, smlf) — idempotent SML descriptor pin substitution.
    // Path is a string-literal const; rx/tx/smlf are runtime ints (-1 = leave unchanged).
    'smlApplyPins':     { syscall: Syscall.SML_APPLY_PINS,    args: 4, returns: true,  constArgs: [0] },
    // smlScripterLoad("/path") — extract >F/>S Scripter sections from descriptor, compile
    // to mini-bytecode, run on 100ms/1s ticks. Returns # sections compiled (0..2), -1 on err.
    'smlScripterLoad':  { syscall: Syscall.SML_SCRIPTER_LOAD, args: 1, returns: true,  constArgs: [0] },
    'webRadio':         { syscall: Syscall.WEB_RADIO,       args: 2, returns: false, refArgs: [0], constArgs: [1] },
    'webTime':          { syscall: Syscall.WEB_TIME,        args: 2, returns: false, refArgs: [0], constArgs: [1] },
    'webPageLabel':     { syscall: Syscall.WEB_PAGE_LABEL,  args: 2, returns: false, constArgs: [1] },
    'webPage':          { syscall: Syscall.WEB_PAGE,        args: 0, returns: true },
    'webSendFile':      { syscall: Syscall.WEB_SEND_FILE,   args: 1, returns: false, constArgs: [0] },
    'webSendJsonArray': { syscall: Syscall.WEB_SEND_JSON_ARRAY, args: 2, returns: false, strArgs: [0] },
    'webOn':            { syscall: Syscall.WEB_ON,          args: 2, returns: false, constArgs: [1] },
    'webHandler':       { syscall: Syscall.WEB_HANDLER,     args: 0, returns: true },
    'webArg':           { syscall: Syscall.WEB_ARG,         args: 2, returns: true, constArgs: [0], strArgs: [1] },
    'mdnsRegister':     { syscall: Syscall.MDNS,            args: 3, returns: true, constArgs: [0, 1, 2] },
    'webConsoleButton': { syscall: Syscall.WEB_CONSOLE_BUTTON, args: 2, returns: false, constArgs: [0, 1] },
    'WebChart':         { syscall: Syscall.WEB_CHART,           args: 11, returns: false, constArgs: [1, 2], refArgs: [6], floatArgs: [9, 10] },
    'WebChartSize':     { syscall: Syscall.WEB_CHART_SIZE,      args: 2, returns: false },
    'WebChartTimeBase': { syscall: Syscall.WEB_CHART_TBASE,     args: 1, returns: false },
    'pluginQuery':      { syscall: Syscall.PLUGIN_QUERY,        args: 4, returns: true,  strArgs: [0] },
    'sortArray':        { syscall: Syscall.SORT_ARRAY,          args: 3, returns: false, refArgs: [0] },
    'smlCopy':          { syscall: Syscall.SML_COPY,            args: 2, returns: true,  strArgs: [0] },
    'arrayFill':        { syscall: Syscall.ARRAY_FILL,          args: 3, returns: false, strArgs: [0] },
    'arrayCopy':        { syscall: Syscall.ARRAY_COPY,          args: 3, returns: false, strArgs: [0, 1] },

    // Webcam (ESP32 — requires USE_WEBCAM)
    'camControl':       { syscall: Syscall.CAM_CONTROL,         args: 3, returns: true },
    'cameraInit':       { syscall: Syscall.CAM_INIT_PINS,       args: 8, returns: true, strArgs: [0] },

    // Hardware register peek/poke (ESP32 memory-mapped I/O)
    'peekReg':          { syscall: Syscall.PEEK_REG,           args: 1, returns: true },
    'pokeReg':          { syscall: Syscall.POKE_REG,           args: 2, returns: false },

    // Tasmota info getters
    'tasmInfo':         { syscall: Syscall.TASM_GET_STR,       args: 2, returns: true, strArgs: [1] },
    'tasmPower':        { syscall: Syscall.TASM_POWER,         args: 1, returns: true },
    'tasmSwitch':       { syscall: Syscall.TASM_SWITCH,        args: 1, returns: true },
    'tasmCounter':      { syscall: Syscall.TASM_COUNTER,       args: 1, returns: true },

    // Display drawing (direct renderer calls)
    'dspText':          { syscall: Syscall.DSP_TEXT,         args: 1, returns: false, strArgs: [0] },
    'dspClear':         { syscall: Syscall.DSP_CLEAR,        args: 0, returns: false },
    'dspPos':           { syscall: Syscall.DSP_POS,          args: 2, returns: false },
    'dspFont':          { syscall: Syscall.DSP_FONT,         args: 1, returns: false },
    'dspSize':          { syscall: Syscall.DSP_SIZE,         args: 1, returns: false },
    'dspColor':         { syscall: Syscall.DSP_COLOR,        args: 2, returns: false },
    'dspDraw':          { syscall: Syscall.DSP_DRAW,         args: 1, returns: false, strArgs: [0] },
    'dspPixel':         { syscall: Syscall.DSP_PIXEL,        args: 2, returns: false },
    'dspLine':          { syscall: Syscall.DSP_LINE,         args: 2, returns: false },
    'dspRect':          { syscall: Syscall.DSP_RECT,         args: 2, returns: false },
    'dspFillRect':      { syscall: Syscall.DSP_FILL_RECT,    args: 2, returns: false },
    'dspCircle':        { syscall: Syscall.DSP_CIRCLE,       args: 1, returns: false },
    'dspFillCircle':    { syscall: Syscall.DSP_FILL_CIRCLE,  args: 1, returns: false },
    'dspHLine':         { syscall: Syscall.DSP_HLINE,        args: 1, returns: false },
    'dspVLine':         { syscall: Syscall.DSP_VLINE,        args: 1, returns: false },
    'dspRoundRect':     { syscall: Syscall.DSP_ROUND_RECT,   args: 3, returns: false },
    'dspFillRoundRect': { syscall: Syscall.DSP_FILL_RRECT,   args: 3, returns: false },
    'dspTriangle':      { syscall: Syscall.DSP_TRIANGLE,     args: 4, returns: false },
    'dspFillTriangle':  { syscall: Syscall.DSP_FILL_TRI,     args: 4, returns: false },
    'dspDim':           { syscall: Syscall.DSP_DIM,          args: 1, returns: false },
    'dspOnOff':         { syscall: Syscall.DSP_ONOFF,        args: 1, returns: false },
    'dspUpdate':        { syscall: Syscall.DSP_UPDATE,       args: 0, returns: false },
    'dspPicture':       { syscall: Syscall.DSP_PICTURE,      args: 2, returns: false, constArgs: [0] },
    'dspWidth':         { syscall: Syscall.DSP_WIDTH,        args: 0, returns: true },
    'dspHeight':        { syscall: Syscall.DSP_HEIGHT,       args: 0, returns: true },
    'dspPad':           { syscall: Syscall.DSP_PAD,          args: 1, returns: false },
    'dspLoadImage':     { syscall: Syscall.DSP_LOAD_IMG,    args: 1, returns: true,  constArgs: [0] },
    'dspPushImageRect': { syscall: Syscall.DSP_IMG_RECT,    args: 7, returns: false },
    'dspImageWidth':    { syscall: Syscall.DSP_IMG_WIDTH,   args: 1, returns: true },
    'dspImageHeight':   { syscall: Syscall.DSP_IMG_HEIGHT,  args: 1, returns: true },
    'dspTextWidth':     { syscall: Syscall.DSP_TEXT_WIDTH,  args: 1, returns: true },  // arg: strlen
    'dspTextHeight':    { syscall: Syscall.DSP_TEXT_HEIGHT, args: 0, returns: true },
    'dspImgText':       { syscall: Syscall.DSP_IMG_TEXT,   args: 7, returns: false, strArgs: [6] },
    'dspLoadImageFromCam':{ syscall: Syscall.DSP_LOAD_IMG_CAM,   args: 1, returns: true },
    'dspImgTextBurn':   { syscall: Syscall.DSP_IMG_TEXT_BURN,    args: 7, returns: false, strArgs: [6] },
    'dspImageToCam':    { syscall: Syscall.DSP_IMG_TO_CAM,       args: 3, returns: true  },  // (slot, x, y, color, fieldw, align, text_buf)

    // Canvas / in-memory RGB565 image slots (ESP32 + USE_DISPLAY + JPEG_PICTS on-device)
    'imgCreate':        { syscall: Syscall.IMG_CREATE,     args: 2, returns: true },   // (w, h) -> int slot
    'imgBeginDraw':     { syscall: Syscall.IMG_BEGIN_DRAW, args: 1, returns: false },  // (slot)
    'imgEndDraw':       { syscall: Syscall.IMG_END_DRAW,   args: 0, returns: false },  // ()
    'imgClear':         { syscall: Syscall.IMG_CLEAR,      args: 2, returns: false },  // (slot, color_rgb565)
    'imgBlit':          { syscall: Syscall.IMG_BLIT,       args: 8, returns: false },  // (dst, src, sx, sy, dx, dy, w, h)
    'imgInvalidate':    { syscall: Syscall.IMG_INVALIDATE, args: 5, returns: false },  // (slot, x, y, w, h)
    'imgFlush':         { syscall: Syscall.IMG_FLUSH,      args: 3, returns: false },  // (slot, panel_x, panel_y)

    // Touch buttons & sliders
    'dspButton':        { syscall: Syscall.DSP_BUTTON,       args: 10, returns: false, constArgs: [9] },
    'dspTButton':       { syscall: Syscall.DSP_TBUTTON,      args: 10, returns: false, constArgs: [9] },
    'dspPButton':       { syscall: Syscall.DSP_PBUTTON,      args: 10, returns: false, constArgs: [9] },
    'dspSlider':        { syscall: Syscall.DSP_SLIDER,       args: 9,  returns: false },
    'dspButtonState':   { syscall: Syscall.DSP_BTN_STATE,    args: 2,  returns: false },
    'touchButton':      { syscall: Syscall.TOUCH_BUTTON,     args: 1,  returns: true },
    'dspButtonDel':     { syscall: Syscall.DSP_BTN_DEL,     args: 1,  returns: false },

    // Audio
    'audioVol':         { syscall: Syscall.AUDIO_VOL,        args: 1, returns: false },
    'audioPlay':        { syscall: Syscall.AUDIO_PLAY,       args: 1, returns: false, constArgs: [0] },
    'audioSay':         { syscall: Syscall.AUDIO_SAY,        args: 1, returns: false, constArgs: [0] },

    // Minimal I2S output
    'i2sBegin':         { syscall: Syscall.I2S_BEGIN,        args: 4, returns: true },
    'i2sWrite':         { syscall: Syscall.I2S_WRITE,        args: 2, returns: true, strArgs: [0] },
    'i2sStop':          { syscall: Syscall.I2S_STOP,         args: 0, returns: false },
    'fileReadPCM16':    { syscall: Syscall.FILE_READ_PCM16,  args: 4, returns: true, strArgs: [1] },

    // Persistent variables
    'saveVars':         { syscall: Syscall.PERSIST_SAVE,     args: 0, returns: false },

    // Deep sleep (ESP32)
    'deepSleep':        { syscall: Syscall.DEEP_SLEEP,       args: 1, returns: false },
    'deepSleepGpio':    { syscall: Syscall.DEEP_SLEEP_GPIO,  args: 3, returns: false },
    'wakeupCause':      { syscall: Syscall.WAKEUP_CAUSE,     args: 0, returns: true },

    // Email (ESP32 — requires USE_SENDMAIL)
    'mailBody':         { syscall: Syscall.EMAIL_BODY,       args: 1, returns: false, strArgs: [0] },
    'mailAttach':       { syscall: Syscall.EMAIL_ATTACH,     args: 1, returns: false, constArgs: [0] },
    'mailAttachPic':    { syscall: Syscall.EMAIL_ATTACH_PIC, args: 1, returns: false },
    'mailSend':         { syscall: Syscall.EMAIL_SEND,       args: 1, returns: true,  strArgs: [0] },

    // Tesla Powerwall (ESP32 — requires TESLA_POWERWALL)
    'pwlRequest':       { syscall: Syscall.PWL_REQUEST,      args: 1, returns: true,  strArgs:   [0] },
    'pwlBind':          { syscall: Syscall.PWL_BIND,         args: 2, returns: false, refArgs: [0], constArgs: [1] },
    'pwlGet':           { syscall: Syscall.PWL_GET_FLOAT,    args: 1, returns: true,  constArgs: [0] },
    'pwlStr':           { syscall: Syscall.PWL_GET_STR,      args: 2, returns: true,  constArgs: [0], strArgs: [1] },

    // HomeKit (ESP32 — requires USE_HOMEKIT)
    'hkSetCode':        { syscall: Syscall.HK_SET_CODE,      args: 1, returns: false, constArgs: [0] },
    'hkAdd':            { syscall: Syscall.HK_ADD,           args: 2, returns: false, constArgs: [0] },
    'hkVar':            { syscall: Syscall.HK_VAR,           args: 1, returns: false, refArgs: [0] },
    'hkReady':          { syscall: Syscall.HK_READY,         args: 1, returns: true,  refArgs: [0] },
    'hkStart':          { syscall: Syscall.HK_START,         args: 0, returns: true },
    'hkInit':           { syscall: Syscall.HK_INIT,          args: 1, returns: true,  strArgs: [0] },
    'hkStop':           { syscall: Syscall.HK_STOP,          args: 0, returns: false },
    'hkReset':          { syscall: Syscall.HK_RESET,         args: 0, returns: false },

    // Addressable LED strip (WS2812)
    'setPixels':        { syscall: Syscall.WS2812,           args: 3, returns: false, refArgs: [0] },

    // Debug
    'print':            { syscall: Syscall.DEBUG_PRINT,     args: 1, returns: false },
    'printStr':         { syscall: Syscall.DEBUG_PRINT_STR, args: 1, returns: false },
    'dumpVM':           { syscall: Syscall.DEBUG_DUMP,      args: 0, returns: false },
};

const HEAP_THRESHOLD = 16;   // arrays > 16 elements go to heap, ≤16 stay inline

class Scope {
    constructor(parent = null) {
        this.parent = parent;
        this.locals = new Map();     // name -> { index, type, isArray, arraySize }
        this.nextIndex = parent ? parent.nextIndex : 0;
    }

    define(name, type, isArray = false, arraySize = 0, heapInfo = null) {
        if (this.locals.has(name)) {
            return this.locals.get(name);
        }
        // Heap arrays don't consume local slots
        if (heapInfo) {
            const info = {
                index: -1,
                type,
                isArray: true,
                arraySize,
                isHeap: true,
                heapHandle: heapInfo.handle,
            };
            this.locals.set(name, info);
            return info;
        }
        const slots = isArray ? arraySize : 1;
        if (this.nextIndex + slots > 256) {
            throw new CodeGenError(
                `Too many local variables (need ${this.nextIndex + slots} slots, max 256). ` +
                `Move large arrays to global scope or reduce sizes.`
            );
        }
        const info = {
            index: this.nextIndex,
            type,
            isArray,
            arraySize,
        };
        this.locals.set(name, info);
        this.nextIndex += slots;
        return info;
    }

    // Register a local name that actually lives in a global slot (for static locals)
    defineAlias(localName, globalName) {
        this.locals.set(localName, { isStaticAlias: true, globalName });
    }

    lookup(name) {
        if (this.locals.has(name)) {
            return this.locals.get(name);
        }
        if (this.parent) {
            return this.parent.lookup(name);
        }
        return null;
    }
}

export class CodeGenerator {
    constructor() {
        this.code = [];             // bytecode output
        this.constants = [];        // constant pool (strings, large numbers)
        this.globals = new Map();   // name -> { index, type, isArray, arraySize }
        this.globalIndex = 0;
        this.functions = new Map(); // name -> { address, params, returnType }
        this.spawnTargets = new Set(); // user-function names referenced by spawnTask/killTask/taskRunning — must be in function table
        this.defines = new Map();   // name -> AST node (compile-time constants)
        this.structTypes = new Map(); // tag -> [{ name, type }]
        this.fnPtrTypes  = new Map(); // alias → { returnType, params }
        this.fnAddrPatches = [];      // [{name, offset}] forward refs to function addresses (for fn-ptr assignment)

        // Predefined color constants (RGB565)
        const colorDefs = {
            BLACK: 0, WHITE: 65535, RED: 63488, GREEN: 2016, BLUE: 31,
            YELLOW: 65504, CYAN: 2047, MAGENTA: 63519, ORANGE: 64800,
            PURPLE: 30735, GREY: 33808, DARKGREY: 21130, LIGHTGREY: 50712,
            DARKGREEN: 992, NAVY: 16, MAROON: 32768, OLIVE: 33792,
        };
        // HomeKit device type constants (mapped to CID+sub_type on firmware side)
        const hkDefs = {
            HK_TEMPERATURE: 1, HK_HUMIDITY: 2, HK_LIGHT_SENSOR: 3,
            HK_BATTERY: 4, HK_CONTACT: 5,
            HK_SWITCH: 6, HK_OUTLET: 7, HK_LIGHT: 8,
        };
        // File mode constants (r=read, w=write, a=append)
        const fileDefs = { r: 0, w: 1, a: 2 };
        const allDefs = { ...colorDefs, ...hkDefs, ...fileDefs };
        for (const [name, value] of Object.entries(allDefs)) {
            this.defines.set(name, { type: 'IntLiteral', value, line: 0 });
        }

        this.currentFunction = null;
        this.scope = null;

        // Heap arrays (size > HEAP_THRESHOLD)
        this.heapArrays = new Map();   // handle -> { handle, size, type }
        this.nextHeapHandle = 0;

        // Lazy scratch buffer used by variadic addLog / addLogLevel.
        // Allocated on first use only — programs that don't use the variadic
        // form pay zero heap cost.
        this.logScratchHandle = null;
        this.LOG_SCRATCH_SIZE = 160;   // device truncates to TC_OUTPUT_SIZE (128) anyway

        // Persist globals: { index, slotCount } for binary persist table
        this.persistGlobals = [];

        // UDP global vars: { index, name, slotCount, varType } for binary globals table
        // varType: 0=float scalar, 1=float array, 2=char array
        this.udpGlobals = [];

        // Watch globals: name → { varIndex, shadowIndex, writtenIndex, type }
        this.watchGlobals = new Map();

        // For break/continue
        this.breakTargets = [];
        this.continueTargets = [];

        // Compiler warnings (non-fatal, collected and returned in compile result)
        this.warnings = [];

        // De-duplicate identical narrowing warnings (same name + line) so a loop
        // assigning the same var doesn't flood the output window.
        this._narrowingSeen = new Set();

        // Forward references (patched later)
        this.patches = [];

        // Source map: bytecode offset -> source line
        this.sourceMap = [];
    }

    // Warn on implicit float → int / char narrowing at assignment. An explicit
    // `(int)` cast routes through compileCast() and suppresses this warning.
    // Motivating bug: bat_ctrl.tc did `int g_sl; g_sl = wr_vl3 / 100.0` —
    // silently truncated float(~2.30) into an array index, out-of-bounds at
    // runtime. With this warning the root cause is flagged at compile time.
    warnNarrowing(name, line, valType, targetType) {
        const key = `${name}@${line}`;
        if (this._narrowingSeen.has(key)) return;
        this._narrowingSeen.add(key);
        this.warnings.push(
            `line ${line}: implicit narrowing — assigning ${valType} to '${name}' (declared ${targetType}). ` +
            `Truncates toward zero. Use explicit (${targetType}) cast to silence.`);
    }

    // ─── Tasmota system variables (virtual — no global slot used) ──
    // These are accessed like normal variables in TinyC code:
    //   if (tasm_wifi) { ... }
    //   tasm_teleperiod = 60;
    static TASM_VARS = {
        'tasm_wifi':        { index: 0, type: 'int',   writable: false },
        'tasm_mqttcon':     { index: 1, type: 'int',   writable: false },
        'tasm_teleperiod':  { index: 2, type: 'int',   writable: true  },
        'tasm_uptime':      { index: 3, type: 'int',   writable: false },
        'tasm_heap':        { index: 4, type: 'int',   writable: false },
        'tasm_power':       { index: 5, type: 'int',   writable: true  },
        'tasm_dimmer':      { index: 6, type: 'int',   writable: true  },
        'tasm_temp':        { index: 7, type: 'float', writable: false },
        'tasm_hum':         { index: 8, type: 'float', writable: false },
        'tasm_hour':        { index: 9, type: 'int',   writable: false },
        'tasm_minute':      { index: 10, type: 'int',  writable: false },
        'tasm_second':      { index: 11, type: 'int',  writable: false },
        'tasm_year':        { index: 12, type: 'int',  writable: false },
        'tasm_month':       { index: 13, type: 'int',  writable: false },
        'tasm_day':         { index: 14, type: 'int',  writable: false },
        'tasm_wday':        { index: 15, type: 'int',  writable: false },
        'tasm_cw':          { index: 16, type: 'int',  writable: false },
        'tasm_sunrise':     { index: 17, type: 'int',  writable: false },
        'tasm_sunset':      { index: 18, type: 'int',  writable: false },
        'tasm_time':        { index: 19, type: 'int',  writable: false },
        'tasm_pheap':       { index: 20, type: 'int',  writable: false },
        'tasm_smlj':        { index: 21, type: 'int',  writable: true  },
        'tasm_npwr':        { index: 22, type: 'int',   writable: false },
        'tasm_rule':        { index: 23, type: 'int',   writable: true  },
        'tasm_lat':         { index: 24, type: 'float', writable: true  },
        'tasm_lon':         { index: 25, type: 'float', writable: true  },
        'tasm_maxblock':    { index: 26, type: 'int',   writable: false },
        'tasm_frag':        { index: 27, type: 'int',   writable: false },
    };

    // ─── Bytecode emission ──────────────────────────────────

    emit(op) {
        this.code.push(op);
        return this.code.length - 1;
    }

    emitByte(val) {
        this.code.push(val & 0xFF);
    }

    emitSyscall(id) {
        if (id <= 255) {
            this.emit(Op.SYSCALL);
            this.emitByte(id);
        } else {
            this.emit(Op.SYSCALL2);
            this.emitU16(id);
        }
    }

    emitU16(val) {
        this.code.push((val >> 8) & 0xFF);
        this.code.push(val & 0xFF);
    }

    emitI32(val) {
        const view = new DataView(new ArrayBuffer(4));
        view.setInt32(0, val, false); // big-endian
        for (let i = 0; i < 4; i++) {
            this.code.push(view.getUint8(i));
        }
    }

    emitF32(val) {
        const view = new DataView(new ArrayBuffer(4));
        view.setFloat32(0, val, false); // big-endian
        for (let i = 0; i < 4; i++) {
            this.code.push(view.getUint8(i));
        }
    }

    emitPushInt(val) {
        if (val >= -128 && val <= 127) {
            this.emit(Op.PUSH_I8);
            this.emitByte(val);
        } else if (val >= -32768 && val <= 32767) {
            this.emit(Op.PUSH_I16);
            this.emitU16(val & 0xFFFF);
        } else {
            this.emit(Op.PUSH_I32);
            this.emitI32(val);
        }
    }

    emitPushFloat(val) {
        this.emit(Op.PUSH_F32);
        this.emitF32(val);
    }

    emitJump(op) {
        this.emit(op);
        const pos = this.code.length;
        this.emitU16(0); // placeholder
        return pos;
    }

    patchJump(pos) {
        const target = this.code.length;
        this.code[pos] = (target >> 8) & 0xFF;
        this.code[pos + 1] = target & 0xFF;
    }

    patchJumpTo(pos, target) {
        this.code[pos] = (target >> 8) & 0xFF;
        this.code[pos + 1] = target & 0xFF;
    }

    addConstant(value) {
        // Expand _Q(var1,var2,...) in string constants at compile time
        if (typeof value === 'string' && value.includes('_Q(')) {
            value = this.expandQueryMacro(value);
        }
        // Check if already in pool
        for (let i = 0; i < this.constants.length; i++) {
            if (this.constants[i] === value) return i;
        }
        const idx = this.constants.length;
        if (idx >= 1024) {
            throw new CodeGenError(
                `Constant pool limit exceeded (${idx} constants, max 1024 on ESP32). ` +
                `Reduce string literals — each unique string counts as one constant.`);
        }
        if (idx === 950 && !this._constWarnEmitted) {
            this._constWarnEmitted = true;
            this.warnings.push(`Constant pool at ${idx}/1024 — approaching ESP32 limit. Each unique string literal uses one slot.`);
        }
        this.constants.push(value);
        return idx;
    }

    // Expand _Q(var1,var2,...) → index-based query encoding for Google Charts
    // e.g. _Q(temperature,counter,label) → "0f;1i;2s16"
    // Encoding: <index><type>[<size>] where type = i(int), f(float), s<n>(char[n]), I<n>(int[n]), F<n>(float[n])
    expandQueryMacro(str) {
        return str.replace(/_Q\(([^)]+)\)/g, (match, args) => {
            const names = args.split(',').map(s => s.trim());
            const parts = [];
            for (const name of names) {
                const info = this.globals.get(name);
                if (!info) {
                    throw new CodeGenError(`_Q(): unknown variable '${name}'`);
                }
                const idx = info.index;
                if (info.isArray) {
                    const size = info.arraySize || 0;
                    if (info.type === 'char') {
                        parts.push(`${idx}s${size}`);
                    } else if (info.type === 'float') {
                        parts.push(`${idx}F${size}`);
                    } else {
                        parts.push(`${idx}I${size}`);
                    }
                } else {
                    parts.push(`${idx}${info.type === 'float' ? 'f' : 'i'}`);
                }
            }
            return parts.join(';');
        });
    }

    addSourceMap(line) {
        this.sourceMap.push({ offset: this.code.length, line });
    }

    // ─── Global definitions ─────────────────────────────────

    defineGlobal(name, type, isArray = false, arraySize = 0, isPersist = false) {
        if (this.globals.has(name)) {
            return this.globals.get(name);
        }
        // Large arrays go to heap automatically (including persist arrays)
        if (isArray && arraySize > HEAP_THRESHOLD) {
            const heapInfo = this.defineHeapArray(name, type, arraySize);
            this.globals.set(name, heapInfo);  // register in globals for name lookup
            return heapInfo;
        }
        const info = {
            index: this.globalIndex,
            type,
            isArray,
            arraySize,
        };
        this.globals.set(name, info);
        this.globalIndex += isArray ? arraySize : 1;
        return info;
    }

    defineHeapArray(name, type, size) {
        const handle = this.nextHeapHandle++;
        const info = {
            index: -1,
            type,
            isArray: true,
            arraySize: size,
            isHeap: true,
            heapHandle: handle,
        };
        this.heapArrays.set(handle, info);  // keyed by handle (unique) — not name (can collide across scopes)
        return info;
    }

    // Lazy-allocate the shared scratch buffer used by variadic
    // addLog() / addLogLevel(). Returns the heap handle. Only the first call
    // actually creates the heap entry; subsequent calls return the cached
    // handle so the buffer is shared across all variadic log call sites.
    // Safe because addLog never yields — no reentrancy concern.
    ensureLogScratch() {
        if (this.logScratchHandle === null) {
            const info = this.defineHeapArray('__tc_log_scratch', 'char', this.LOG_SCRATCH_SIZE);
            this.logScratchHandle = info.heapHandle;
        }
        return this.logScratchHandle;
    }

    emitLogScratchRef() {
        const handle = this.ensureLogScratch();
        this.emit(Op.ADDR_HEAP);
        this.emitByte(handle);
    }

    // ─── Type tracking ──────────────────────────────────────

    getVarType(name) {
        // Check locals first (locals shadow defines, like C preprocessor)
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local) return local.type;
        }

        // Check defines (only if no local with same name)
        if (this.defines.has(name)) {
            const defNode = this.defines.get(name);
            // Delegate to inferType so that wrapped literals like
            //   #define ANG_MIN -135.0   (UnaryExpr with FloatLiteral operand)
            //   #define TWO_PI  2.0 * 3.14159
            // report the correct type ('float'). The old fast-path only matched
            // bare FloatLiteral/StringLiteral nodes, so a negative float define
            // was typed as 'int' and callers emitted a spurious I2F coercion
            // that reinterpreted the float bit-pattern as an int — producing
            // huge garbage values (e.g. -135.0 → -1.02e9).  See tinyc_bugs.md #8.
            return this.inferType(defNode);
        }

        // Check Tasmota system variables
        const tasmVar = CodeGenerator.TASM_VARS[name];
        if (tasmVar) return tasmVar.type;

        // Check globals
        const global = this.globals.get(name);
        if (global) return global.type;

        return 'int'; // default
    }

    isFloatType(type) {
        return type === 'float';
    }

    // ─── Compilation entry ──────────────────────────────────

    compile(ast) {
        // Zero-th pass: collect struct type definitions (including typedef'd anonymous structs)
        for (const node of ast.body) {
            if (node.type === NodeType.StructDecl) {
                this.structTypes.set(node.tag, node.members);
            } else if (node.type === NodeType.TypedefDecl && node.structDecl) {
                this.structTypes.set(node.structDecl.tag, node.structDecl.members);
            }
        }

        // Pre-pre-pass: register fn-ptr typedef aliases (so var decls can refer to them)
        for (const node of ast.body) {
            if (node.type === NodeType.TypedefDecl && node.fnPtrSig) {
                this.fnPtrTypes.set(node.alias, node.fnPtrSig);
            }
        }
        // First pass: collect globals, functions, and defines
        for (const node of ast.body) {
            if (node.type === NodeType.DefineDecl) {
                this.defines.set(node.name, node.value);
            } else if (node.type === NodeType.FunctionDecl) {
                this.functions.set(node.name, {
                    address: -1, // filled in second pass
                    params: node.params,
                    returnType: node.returnType,
                });
            } else if (node.type === NodeType.VarDecl) {
                const g = this.defineGlobal(node.name, node.varType);
                if (node.isPersist) {
                    this.persistGlobals.push({ index: g.index, slotCount: 1 });
                }
                if (node.isGlobal) {
                    if (node.name.length > 15) throw new CodeGenError("global var name too long for UDP (max 15 chars)", node.line);
                    this.udpGlobals.push({ index: g.index, name: node.name, slotCount: 1, varType: 0 });
                }
                if (node.isWatch) {
                    const shadowIndex = this.globalIndex++;  // shadow slot
                    const writtenIndex = this.globalIndex++; // written flag slot
                    this.watchGlobals.set(node.name, {
                        varIndex: g.index,
                        shadowIndex,
                        writtenIndex,
                        type: node.varType,
                    });
                }
            } else if (node.type === NodeType.StructVarDecl) {
                const members = this.structTypes.get(node.tag);
                if (!members) throw new CodeGenError(`Unknown struct '${node.tag}'`, node.line);
                const totalSlots = this.memberSlotCount(members);
                const slots = node.isArray ? totalSlots * node.arraySize : totalSlots;
                const g = this.defineGlobal(node.name, node.structType, true, slots);
                g.isStruct = true;
                g.structTag = node.tag;
                if (node.isPersist) {
                    this.persistGlobals.push({ index: g.index, slotCount: slots });
                }
            } else if (node.type === NodeType.ArrayDecl) {
                const dims = this.arrayDims(node);
                const size = dims.flat;
                const g = this.defineGlobal(node.name, node.varType, true, size, node.isPersist);
                if (dims.cols > 1) { g.cols = dims.cols; g.rows = dims.rows; }
                if (node.isPersist) {
                    if (g.isHeap) {
                        // Heap persist: index field = 0x8000 | heapHandle (bit 15 = heap flag)
                        this.persistGlobals.push({ index: 0x8000 | g.heapHandle, slotCount: size });
                    } else {
                        this.persistGlobals.push({ index: g.index, slotCount: size });
                    }
                }
                if (node.isGlobal) {
                    if (node.name.length > 15) throw new CodeGenError("global var name too long for UDP (max 15 chars)", node.line);
                    const gtype = (node.varType === 'char') ? 2 : 1; // 2=char array, 1=float array
                    this.udpGlobals.push({ index: g.index, name: node.name, slotCount: size, varType: gtype });
                }
            }
        }

        // Second pass: compile global initializers (BEFORE CALL main)
        for (const node of ast.body) {
            if (node.type === NodeType.VarDecl && node.init && !node.isPersist) {
                this.addSourceMap(node.line);
                this.compileExpr(node.init);
                const g = this.globals.get(node.name);
                // Type coercion for global initializers (e.g., float x = 1;)
                const valType = this.inferType(node.init);
                if (node.varType === 'float' && valType !== 'float') this.emit(Op.I2F);
                else if (node.varType !== 'float' && valType === 'float') this.emit(Op.F2I);
                this.emitStoreGlobal(g.index, node.name);
            } else if (node.type === NodeType.ArrayDecl && node.stringInit) {
                const g = this.globals.get(node.name);
                const str = node.stringInit;
                const size = this.inferArraySize(node);
                const isHeap = g && g.isHeap;
                for (let i = 0; i < str.length && i < size - 1; i++) {
                    this.addSourceMap(node.line);
                    this.emitPushInt(i);
                    this.emitPushInt(str.charCodeAt(i));
                    if (isHeap) {
                        this.emit(Op.STORE_HEAP_ARR);
                        this.emitByte(g.heapHandle);
                    } else {
                        this.emit(Op.STORE_GLOBAL_ARR);
                        this.emitU16(g.index);
                    }
                }
                // Null terminator
                this.emitPushInt(Math.min(str.length, size - 1));
                this.emitPushInt(0);
                if (isHeap) {
                    this.emit(Op.STORE_HEAP_ARR);
                    this.emitByte(g.heapHandle);
                } else {
                    this.emit(Op.STORE_GLOBAL_ARR);
                    this.emitU16(g.index);
                }
            } else if (node.type === NodeType.ArrayDecl && node.init) {
                const g = this.globals.get(node.name);
                const isHeap = g && g.isHeap;
                for (let i = 0; i < node.init.length; i++) {
                    this.addSourceMap(node.line);
                    this.emitPushInt(i);        // index
                    this.compileExpr(node.init[i]); // value
                    if (isHeap) {
                        this.emit(Op.STORE_HEAP_ARR);
                        this.emitByte(g.heapHandle);
                    } else {
                        this.emit(Op.STORE_GLOBAL_ARR);
                        this.emitU16(g.index);
                    }
                }
            }
        }

        // Emit: CALL main, HALT (after global inits)
        this.emit(Op.CALL);
        const mainCallAddr = this.code.length;
        this.emitU16(0); // patched when we find main()
        // main() is always void — no POP needed (RET doesn't push a value)
        this.emit(Op.HALT);

        // Third pass: compile functions
        for (const node of ast.body) {
            if (node.type === NodeType.FunctionDecl) {
                this.compileFunction(node);
            }
        }

        // Patch main() call
        const mainFunc = this.functions.get('main');
        if (!mainFunc || mainFunc.address < 0) {
            throw new CodeGenError("No 'main' function defined", 0);
        }
        this.patchJumpTo(mainCallAddr, mainFunc.address);

        // Patch forward function calls (functions called before they were defined)
        for (const patch of this.patches) {
            const func = this.functions.get(patch.name);
            if (!func || func.address < 0) {
                throw new CodeGenError(`Unresolved forward reference to '${patch.name}'`, patch.line);
            }
            this.patchJumpTo(patch.offset, func.address);
        }
        // Patch fn-pointer address-of references (PUSH_I32 placeholders left
        // by compileIdentifier when a bare function name was used as a value).
        for (const fp of this.fnAddrPatches) {
            const func = this.functions.get(fp.name);
            if (!func || func.address < 0) {
                throw new CodeGenError(`Unresolved fn-pointer reference to '${fp.name}'`, 0);
            }
            // PUSH_I32 reads big-endian; address fits in low 16 bits.
            this.code[fp.offset]     = 0;
            this.code[fp.offset + 1] = 0;
            this.code[fp.offset + 2] = (func.address >> 8) & 0xFF;
            this.code[fp.offset + 3] =  func.address       & 0xFF;
        }

        // Constant-pool diagnostic — Andreas's request 7.2 (BUG_REPORT
        // 2026-04-25). When the pool is large enough that the user is
        // hunting for things to consolidate, append a top-N digest to
        // the warnings: 10 longest entries (fat strings) + the most
        // common short-string prefixes (sprintf format-splits).
        if (this.constants.length >= 200) {
            const stringConsts = this.constants
                .filter(c => typeof c === 'string')
                .map((s, i) => ({ s, i, len: s.length }));

            // Top 10 longest
            const longest = [...stringConsts]
                .sort((a, b) => b.len - a.len)
                .slice(0, 10);
            const longestLines = longest.map(({ s, i, len }) => {
                const preview = s.length > 60 ? s.slice(0, 57) + '...' : s;
                // escape control chars for display
                const safe = preview.replace(/[\x00-\x1F]/g, c =>
                    '\\x' + c.charCodeAt(0).toString(16).padStart(2, '0'));
                return `    [#${String(i).padStart(4)}] ${len.toString().padStart(4)}B  "${safe}"`;
            });

            // Most common short prefixes (heuristic for sprintf format splits).
            // Strings <= 32 bytes are likely format-string fragments. Group by
            // their first 6 chars (or full string if shorter).
            const prefixCount = new Map();
            for (const { s } of stringConsts) {
                if (s.length === 0 || s.length > 32) continue;
                const key = s.slice(0, Math.min(6, s.length));
                const e = prefixCount.get(key) || { count: 0, examples: [] };
                e.count++;
                if (e.examples.length < 3 && !e.examples.includes(s)) {
                    e.examples.push(s);
                }
                prefixCount.set(key, e);
            }
            const topPrefixes = [...prefixCount.entries()]
                .filter(([, v]) => v.count >= 3)
                .sort((a, b) => b[1].count - a[1].count)
                .slice(0, 8);
            const prefixLines = topPrefixes.map(([k, v]) =>
                `    "${k.replace(/[\x00-\x1F]/g, '?')}…"  ×${v.count}  e.g. ${v.examples.map(s => '"' + s.replace(/[\x00-\x1F]/g, '?') + '"').join(', ')}`);

            const total = stringConsts.length;
            const totalBytes = stringConsts.reduce((a, b) => a + b.len, 0);
            const lines = [
                `Constant pool: ${this.constants.length} total entries (${total} strings, ${totalBytes} B).`,
                `  Top ${longest.length} longest:`,
                ...longestLines,
            ];
            if (prefixLines.length > 0) {
                lines.push(`  Frequent short prefixes (likely sprintf format-splits — consolidate format strings to save slots):`);
                lines.push(...prefixLines);
            }
            this.warnings.push(lines.join('\n'));
        }

        return this.buildBinary();
    }

    // ─── Function compilation ───────────────────────────────

    compileFunction(node) {
        const funcInfo = this.functions.get(node.name);
        funcInfo.address = this.code.length;

        this.currentFunction = funcInfo;
        this.scope = new Scope();
        // Reset per-function temp slots used by struct-return receive.
        this._structRetTmp = null;

        // Register parameters as locals
        for (const param of node.params) {
            // Scalar reference param (int& a, float& a, char& a). 1-slot local
            // holding an encoded ref (ADDR_LOCAL/ADDR_GLOBAL value), accessed
            // via LOAD/STORE_REF_ARR with index 0.
            if (param.isScalarRef) {
                const info = this.scope.define(param.name, param.type, true, 1);
                info.isRef       = true;
                info.isScalarRef = true;
                continue;
            }
            // Struct-typed param: reserve memberSlotCount slots
            if (typeof param.type === 'string' && param.type.startsWith('struct:')) {
                const tag = param.type.slice(7);
                const members = this.structTypes.get(tag);
                if (!members) {
                    throw new CodeGenError(`Unknown struct '${tag}' in parameter`, node.line || 0);
                }
                const slots = this.memberSlotCount(members);
                const info = this.scope.define(param.name, param.type, true, slots);
                info.isStruct = true;
                info.structTag = tag;
                continue;
            }
            if (param.arraySize !== null) {
                if (param.arraySize === 0) {
                    // Array reference parameter (e.g., char cmd[])
                    // Occupies 1 local slot to hold a runtime heap ref
                    const info = this.scope.define(param.name, param.type, true, 1);
                    info.isRef = true;
                } else {
                    this.scope.define(param.name, param.type, true, param.arraySize);
                }
            } else {
                this.scope.define(param.name, param.type);
            }
        }

        // Emit code to pop arguments from stack into locals.
        // For multi-slot params (structs), each slot gets its own STORE_LOCAL.
        // We walk slot indices in reverse so the order matches how the
        // caller pushed (slot 0 first, slot N-1 last → pop slot N-1 first).
        // Build flat slot list across all params.
        const flatSlots = [];
        for (const param of node.params) {
            const info = this.scope.lookup(param.name);
            if (info.isStruct) {
                for (let s = 0; s < info.arraySize; s++) flatSlots.push(info.index + s);
            } else if (info.isRef || info.isArray) {
                flatSlots.push(info.index);   // array refs occupy 1 slot
            } else {
                flatSlots.push(info.index);
            }
        }
        for (let i = flatSlots.length - 1; i >= 0; i--) {
            this.emit(Op.STORE_LOCAL);
            this.emitByte(flatSlots[i]);
        }

        // Compile body
        this.compileBlock(node.body);

        // Ensure function returns (implicit return 0 for non-void)
        if (node.returnType === 'void') {
            this.emit(Op.RET);
        } else {
            this.emitPushInt(0);
            this.emit(Op.RET_VAL);
        }

        this.currentFunction = null;
        this.scope = null;
    }

    // ─── Statement compilation ──────────────────────────────

    compileBlock(node) {
        const prevScope = this.scope;
        this.scope = new Scope(prevScope);
        for (const stmt of node.body) {
            this.compileStmt(stmt);
        }
        this.scope = prevScope;
    }

    compileStmt(node) {
        this.addSourceMap(node.line);

        switch (node.type) {
            case NodeType.VarDecl:      return this.compileVarDecl(node);
            case NodeType.ArrayDecl:    return this.compileArrayDecl(node);
            case NodeType.StructDecl:    return this.compileStructDecl(node);
            case NodeType.StructVarDecl: return this.compileStructVarDecl(node);
            case NodeType.TypedefDecl:   return this.compileTypedefDecl(node);
            case NodeType.Block:        return this.compileBlock(node);
            case NodeType.IfStmt:       return this.compileIf(node);
            case NodeType.WhileStmt:    return this.compileWhile(node);
            case NodeType.DoWhileStmt:  return this.compileDoWhile(node);
            case NodeType.ForStmt:      return this.compileFor(node);
            case NodeType.SwitchStmt:   return this.compileSwitch(node);
            case NodeType.ReturnStmt:   return this.compileReturn(node);
            case NodeType.BreakStmt:    return this.compileBreak(node);
            case NodeType.ContinueStmt: return this.compileContinue(node);
            case NodeType.ExprStmt:     return this.compileExprStmt(node);
            case NodeType.DefineDecl:   return this.compileDefine(node);
            default:
                throw new CodeGenError(`Unknown statement type: ${node.type}`, node.line);
        }
    }

    compileDefine(node) {
        // #define or enum constant used as a statement — just register it
        this.defines.set(node.name, node.value);
    }

    compileVarDecl(node) {
        if (node.isStatic && this.scope) {
            // Static local: hoist to a global with mangled name, init only once
            const funcName = this.currentFunction ? this.currentFunction.name : 'global';
            const mangledName = `__s_${funcName}_${node.name}`;
            // Register in globals if not already done (first compilation pass already ran)
            let ginfo = this.globals.get(mangledName);
            if (!ginfo) {
                ginfo = this.defineGlobal(mangledName, node.varType);
            }
            // Make the local name resolve to this global by adding a scope alias
            this.scope.defineAlias(node.name, mangledName);
            // Static locals are zero-initialised by the VM's global data segment.
            // We intentionally do NOT emit the initialiser expression here: emitting it
            // would re-run on every function call, which would reset the value — the
            // opposite of "static" semantics.  Non-zero initialisers (e.g. static int n=5)
            // rely on the programmer setting the value explicitly the first time, or on a
            // future "first-call flag" mechanism.
            return;
        }
        const info = this.scope.define(node.name, node.varType);
        if (node.init) {
            this.compileExpr(node.init);
            this.emit(Op.STORE_LOCAL);
            this.emitByte(info.index);
        }
    }

    // ─── Struct compilation ─────────────────────────────────

    compileStructDecl(node) {
        // Register struct type (may already be in pre-pass for globals; needed for local definitions)
        this.structTypes.set(node.tag, node.members);
    }

    compileTypedefDecl(node) {
        // typedef is purely a parser-level alias — register the struct if it carries one
        if (node.structDecl) {
            this.structTypes.set(node.structDecl.tag, node.structDecl.members);
        }
        // No bytecode emitted
    }

    compileStructVarDecl(node) {
        // Local struct variable allocation
        const members = this.structTypes.get(node.tag);
        if (!members) throw new CodeGenError(`Unknown struct '${node.tag}'`, node.line);
        const totalSlots = this.memberSlotCount(members);
        const slots = node.isArray ? totalSlots * node.arraySize : totalSlots;

        const info = this.scope.define(node.name, node.structType, true, slots);
        info.isStruct = true;
        info.structTag = node.tag;

        // Initializer from a struct-returning expression: Point z = make_point(...)
        // The expression pushes N values; we pop them into z's slots in reverse
        // (TOS = slot N-1 first), using a temp local for the offset/value swap.
        if (node.initExpr && !node.isArray) {
            // Compile the expression (callee will push N values onto data stack)
            this.compileExpr(node.initExpr);
            // Allocate a single-slot temp once per function (lazy)
            if (this._structRetTmp == null) {
                const t = this.scope.define('$ret_tmp', 'int');
                this._structRetTmp = t.index;
            }
            const tmp = this._structRetTmp;
            for (let off = totalSlots - 1; off >= 0; off--) {
                // Stack now has [..., valOff] (valOff at TOS)
                this.emit(Op.STORE_LOCAL); this.emitByte(tmp);   // val → tmp
                this.emitPushInt(off);                            // offset on stack
                this.emit(Op.LOAD_LOCAL);  this.emitByte(tmp);   // val back on top
                // Stack: [..., offset, val]   ready for STORE_LOCAL_ARR
                this.emit(Op.STORE_LOCAL_ARR); this.emitByte(info.index);
            }
            return;
        }
        // Handle positional initializer: struct Point p = {1, 2};
        // Array fields are skipped (cannot be initialized from a scalar list).
        if (node.init && !node.isArray) {
            let offset = 0;
            let initIdx = 0;
            for (const m of members) {
                if (m.isArray) { offset += m.arraySize; continue; }
                if (initIdx >= node.init.length) break;
                this.emitPushInt(offset);
                this.compileExpr(node.init[initIdx]);
                const valType = this.inferType(node.init[initIdx]);
                if (m.type === 'float' && valType !== 'float') this.emit(Op.I2F);
                else if (m.type !== 'float' && valType === 'float') this.emit(Op.F2I);
                this.emit(Op.STORE_LOCAL_ARR);
                this.emitByte(info.index);
                offset++;
                initIdx++;
            }
        }
    }

    // Slot count of one struct member. Handles:
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
    }

    // Resolve struct member. `object` may be:
    //   • a plain variable name (string)
    //   • an ArrayAccess AST node (e.g. `gauges[i]`)
    //   • a MemberAccess AST node (nested: `a.b.c`)
    // Returns { info, isLocal, fieldOffset, fieldType, fieldIsArray, fieldArraySize,
    //          isArrayElement, indexExpr, structSlots }
    resolveMember(object, fieldName, line) {
        // Nested field path: walk the MemberAccess chain inside-out, summing
        // field offsets along the way. Reduces to a base var + total offset.
        if (typeof object === 'object' && object !== null && object.type === NodeType.MemberAccess) {
            const inner = this.resolveMember(object.object, object.field, object.line || line);
            // `inner` describes `object.field` (the parent of `fieldName` in this call).
            // The parent must itself be a struct (nested type), with tag inferred from
            // the parent member's declared type (`struct:InnerTag`).
            const parentMembers = this.structTypes.get(inner.info.structTag);
            const parentMember  = parentMembers.find(m => m.name === object.field);
            if (!parentMember || typeof parentMember.type !== 'string'
                              || !parentMember.type.startsWith('struct:')) {
                throw new CodeGenError(
                    `'${object.field}' is not a struct field — nested access '${object.field}.${fieldName}' invalid`,
                    object.line || line);
            }
            const innerTag = parentMember.type.slice(7);
            const innerMembers = this.structTypes.get(innerTag);
            if (!innerMembers) {
                throw new CodeGenError(`Unknown nested struct type 'struct:${innerTag}'`, line);
            }
            // Find the requested field within the nested struct.
            let nestedOffset = 0;
            let nestedMember = null;
            for (const m of innerMembers) {
                if (m.name === fieldName) { nestedMember = m; break; }
                nestedOffset += this.memberSlotsFor(m);
            }
            if (!nestedMember) {
                throw new CodeGenError(
                    `Nested struct '${innerTag}' has no member '${fieldName}'`, line);
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
        if (!info) throw new CodeGenError(`Undefined variable: '${objectName}'`, line);
        if (!info.isStruct) throw new CodeGenError(`'${objectName}' is not a struct`, line);

        const members = this.structTypes.get(info.structTag);
        // Compute cumulative slot offset (nested struct fields contribute
        // their own total slot count, not 1).
        let fieldOffset = 0;
        let fieldMember = null;
        for (const m of members) {
            if (m.name === fieldName) { fieldMember = m; break; }
            fieldOffset += this.memberSlotsFor(m);
        }
        if (!fieldMember) {
            throw new CodeGenError(`Struct '${info.structTag}' has no member '${fieldName}'`, line);
        }
        const structSlots = this.memberSlotCount(members);
        return {
            info, isLocal, fieldOffset,
            fieldType: fieldMember.type,
            fieldIsArray: !!fieldMember.isArray,
            fieldArraySize: fieldMember.arraySize || 0,
            isArrayElement, indexExpr, structSlots,
        };
    }

    // Emit code that pushes the base offset (in slots) of a struct member onto
    // the stack. For a simple struct var, this is just `fieldOffset`. For an
    // element of a struct array (`arr[i].field`), it's `i * structSlots + fieldOffset`.
    // Optionally adds an extra per-call offset (used for array-field subscript).
    emitStructOffset(resolvedMember, extraOffsetExpr = null) {
        const { isArrayElement, indexExpr, structSlots, fieldOffset } = resolvedMember;
        if (isArrayElement) {
            this.compileExpr(indexExpr);
            this.emitPushInt(structSlots);
            this.emit(Op.MUL);
            if (fieldOffset > 0) {
                this.emitPushInt(fieldOffset);
                this.emit(Op.ADD);
            }
        } else {
            this.emitPushInt(fieldOffset);
        }
        if (extraOffsetExpr) {
            this.compileExpr(extraOffsetExpr);
            this.emit(Op.ADD);
        }
    }

    // Emit a load of the member slot at the offset currently on top of the stack.
    emitLoadStructSlot(info, isLocal) {
        if (info.isHeap) {
            this.emit(Op.LOAD_HEAP_ARR);
            this.emitByte(info.heapHandle);
        } else if (isLocal) {
            this.emit(Op.LOAD_LOCAL_ARR);
            this.emitByte(info.index);
        } else {
            this.emit(Op.LOAD_GLOBAL_ARR);
            this.emitU16(info.index);
        }
    }

    // Emit a store of the value on top of the stack to the member slot.
    // Stack before store op: [..., offset, value]
    emitStoreStructSlot(info, isLocal) {
        if (info.isHeap) {
            this.emit(Op.STORE_HEAP_ARR);
            this.emitByte(info.heapHandle);
        } else if (isLocal) {
            this.emit(Op.STORE_LOCAL_ARR);
            this.emitByte(info.index);
        } else {
            this.emit(Op.STORE_GLOBAL_ARR);
            this.emitU16(info.index);
        }
    }

    // Returns true if `name` resolves to a struct variable in scope/globals.
    _isStructVar(name) {
        if (!name) return false;
        const local = this.scope ? this.scope.lookup(name) : null;
        if (local && local.isStruct) return true;
        const g = this.globals.get(name);
        return !!(g && g.isStruct);
    }
    // Returns true if expression `e` evaluates to a struct value:
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
        if (!info) throw new CodeGenError(`Undefined variable: '${name}'`, 0);
        if (!info.isStruct) throw new CodeGenError(`'${name}' is not a struct`, 0);
        const members = this.structTypes.get(info.structTag);
        return {
            info, isLocal,
            tag: info.structTag,
            structSlots: this.memberSlotCount(members),
            isArrayElement, indexExpr,
        };
    }
    // Push the slot offset of the `off`-th slot of a struct base onto the stack.
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
    // Whole-struct copy. `dst` is a struct lvalue descriptor (var or array element);
    // `src` is an AST node (Identifier or ArrayAccess) referring to a struct.
    _compileStructCopy(dst, src, line) {
        const dstRm = this._resolveStructAccess(dst);
        const srcRm = this._resolveStructAccess(src);
        if (dstRm.tag !== srcRm.tag) {
            throw new CodeGenError(
                `Cannot assign struct '${srcRm.tag}' to struct '${dstRm.tag}' — different types`,
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
        const rm = this.resolveMember(node.object, node.field, node.line);
        const { info, isLocal, fieldIsArray } = rm;
        if (fieldIsArray) {
            throw new CodeGenError(
                `Array field '${node.field}' cannot be used as a scalar — use 'obj.${node.field}[i]'`,
                node.line);
        }
        this.emitStructOffset(rm);
        this.emitLoadStructSlot(info, isLocal);
    }

    compileMemberAssign(node) {
        const rm = this.resolveMember(node.object, node.field, node.line);
        const { info, isLocal, fieldType, fieldIsArray } = rm;
        if (fieldIsArray) {
            throw new CodeGenError(
                `Cannot assign to array field '${node.field}' directly — use 'obj.${node.field}[i] = ...'`,
                node.line);
        }

        if (node.op === '=') {
            this.emitStructOffset(rm);
            this.compileExpr(node.value);
            const valType = this.inferType(node.value);
            if (fieldType === 'float' && valType !== 'float') this.emit(Op.I2F);
            else if (fieldType !== 'float' && valType === 'float') this.emit(Op.F2I);
        } else {
            // Compound assignment: compute offset once, DUP so both load and
            // store can consume it. Stack: [..., offset, offset, oldVal, rhs, op, newVal]
            const isFloat = fieldType === 'float';
            this.emitStructOffset(rm);
            this.emit(Op.DUP);
            this.emitLoadStructSlot(info, isLocal);
            this.compileExpr(node.value);
            const valType = this.inferType(node.value);
            if (isFloat && valType !== 'float') this.emit(Op.I2F);
            else if (!isFloat && valType === 'float') this.emit(Op.F2I);
            switch (node.op) {
                case '+=':  this.emit(isFloat ? Op.FADD : Op.ADD);  break;
                case '-=':  this.emit(isFloat ? Op.FSUB : Op.SUB);  break;
                case '*=':  this.emit(isFloat ? Op.FMUL : Op.MUL);  break;
                case '/=':  this.emit(isFloat ? Op.FDIV : Op.DIV);  break;
                case '%=':  this.emit(Op.MOD);     break;
                case '&=':  this.emit(Op.BIT_AND); break;
                case '|=':  this.emit(Op.BIT_OR);  break;
                case '^=':  this.emit(Op.BIT_XOR); break;
                case '<<=': this.emit(Op.SHL);     break;
                case '>>=': this.emit(Op.SHR);     break;
                default: throw new CodeGenError(`Unknown compound member assignment: ${node.op}`, node.line);
            }
        }
        this.emitStoreStructSlot(info, isLocal);
    }

    compileMemberArrayAccess(node) {
        const rm = this.resolveMember(node.object, node.field, node.line);
        const { info, isLocal, fieldIsArray } = rm;
        if (!fieldIsArray) {
            throw new CodeGenError(
                `Field '${node.field}' is not an array — use 'obj.${node.field}' without subscript`,
                node.line);
        }
        // offset = (structBase) + fieldOffset + index
        this.emitStructOffset(rm, node.index);
        this.emitLoadStructSlot(info, isLocal);
    }

    compileMemberArrayAssign(node) {
        const rm = this.resolveMember(node.object, node.field, node.line);
        const { info, isLocal, fieldType, fieldIsArray } = rm;
        if (!fieldIsArray) {
            throw new CodeGenError(
                `Field '${node.field}' is not an array — use 'obj.${node.field} = ...' without subscript`,
                node.line);
        }
        const isFloat = fieldType === 'float';

        if (node.op === '=') {
            this.emitStructOffset(rm, node.index);
            this.compileExpr(node.value);
            const valType = this.inferType(node.value);
            if (isFloat && valType !== 'float') this.emit(Op.I2F);
            else if (!isFloat && valType === 'float') this.emit(Op.F2I);
        } else {
            // Compound: compute (structBase + fieldOffset + index) once, DUP it
            this.emitStructOffset(rm, node.index);
            this.emit(Op.DUP);
            this.emitLoadStructSlot(info, isLocal);
            this.compileExpr(node.value);
            const valType = this.inferType(node.value);
            if (isFloat && valType !== 'float') this.emit(Op.I2F);
            else if (!isFloat && valType === 'float') this.emit(Op.F2I);
            switch (node.op) {
                case '+=':  this.emit(isFloat ? Op.FADD : Op.ADD);  break;
                case '-=':  this.emit(isFloat ? Op.FSUB : Op.SUB);  break;
                case '*=':  this.emit(isFloat ? Op.FMUL : Op.MUL);  break;
                case '/=':  this.emit(isFloat ? Op.FDIV : Op.DIV);  break;
                case '%=':  this.emit(Op.MOD);     break;
                case '&=':  this.emit(Op.BIT_AND); break;
                case '|=':  this.emit(Op.BIT_OR);  break;
                case '^=':  this.emit(Op.BIT_XOR); break;
                case '<<=': this.emit(Op.SHL);     break;
                case '>>=': this.emit(Op.SHR);     break;
                default: throw new CodeGenError(`Unknown compound member array assignment: ${node.op}`, node.line);
            }
        }
        this.emitStoreStructSlot(info, isLocal);
    }

    // Phase 1 2D helper — returns rows count, cols count (1 for 1D),
    // and total flat slot count.
    arrayDims(node) {
        const rows = this.inferArraySize(node);
        let cols = 1;
        if (node.cols != null) {
            // node.cols is an AST expression; reuse the size-evaluator
            cols = this.inferArraySize({ size: node.cols });
            if (!Number.isInteger(cols) || cols < 1) {
                throw new CodeGenError("2D array column count must be a positive integer constant", node.line);
            }
        }
        return { rows, cols, flat: rows * cols };
    }

    compileArrayDecl(node) {
        const dims = this.arrayDims(node);
        const size = dims.flat;

        // Large local arrays → heap
        if (size > HEAP_THRESHOLD) {
            const heapInfo = this.defineHeapArray(node.name, node.varType, size);
            const localSym = this.scope.define(node.name, node.varType, true, size, { handle: heapInfo.heapHandle });
            if (dims.cols > 1) { localSym.cols = dims.cols; localSym.rows = dims.rows; }
            // Emit initializers using STORE_HEAP_ARR
            if (node.stringInit) {
                const str = node.stringInit;
                for (let i = 0; i < str.length && i < size - 1; i++) {
                    this.emitPushInt(i);
                    this.emitPushInt(str.charCodeAt(i));
                    this.emit(Op.STORE_HEAP_ARR);
                    this.emitByte(heapInfo.heapHandle);
                }
                this.emitPushInt(Math.min(str.length, size - 1));
                this.emitPushInt(0);
                this.emit(Op.STORE_HEAP_ARR);
                this.emitByte(heapInfo.heapHandle);
            } else if (node.init) {
                for (let i = 0; i < node.init.length; i++) {
                    this.emitPushInt(i);
                    this.compileExpr(node.init[i]);
                    this.emit(Op.STORE_HEAP_ARR);
                    this.emitByte(heapInfo.heapHandle);
                }
            }
            return;
        }

        const info = this.scope.define(node.name, node.varType, true, size);
        if (dims.cols > 1) { info.cols = dims.cols; info.rows = dims.rows; }
        if (node.stringInit) {
            // char buf[N] = "hello" → copy string chars + null terminator
            const str = node.stringInit;
            for (let i = 0; i < str.length && i < size - 1; i++) {
                this.emitPushInt(i);
                this.emitPushInt(str.charCodeAt(i));
                this.emit(Op.STORE_LOCAL_ARR);
                this.emitByte(info.index);
            }
            // Null terminator
            this.emitPushInt(Math.min(str.length, size - 1));
            this.emitPushInt(0);
            this.emit(Op.STORE_LOCAL_ARR);
            this.emitByte(info.index);
        } else if (node.init) {
            for (let i = 0; i < node.init.length; i++) {
                this.emitPushInt(i);
                this.compileExpr(node.init[i]);
                this.emit(Op.STORE_LOCAL_ARR);
                this.emitByte(info.index);
            }
        }
    }

    compileIf(node) {
        this.compileExpr(node.condition);
        const elseJump = this.emitJump(Op.JZ);

        this.compileStmt(node.consequent);

        if (node.alternate) {
            const endJump = this.emitJump(Op.JMP);
            this.patchJump(elseJump);
            this.compileStmt(node.alternate);
            this.patchJump(endJump);
        } else {
            this.patchJump(elseJump);
        }
    }

    compileWhile(node) {
        const loopStart = this.code.length;
        this.compileExpr(node.condition);
        const exitJump = this.emitJump(Op.JZ);

        this.breakTargets.push([]);
        this.continueTargets.push([]);  // collect continue patches

        this.compileStmt(node.body);

        this.emit(Op.JMP);
        this.emitU16(loopStart);
        this.patchJump(exitJump);

        // Patch break jumps
        const breaks = this.breakTargets.pop();
        for (const bp of breaks) this.patchJump(bp);
        // Patch continue jumps → back to condition check
        const continues = this.continueTargets.pop();
        for (const cp of continues) this.patchJumpTo(cp, loopStart);
    }

    compileDoWhile(node) {
        const loopStart = this.code.length;
        this.breakTargets.push([]);
        this.continueTargets.push([]);  // collect continue patches

        this.compileStmt(node.body);

        // continue targets land here — before the condition check
        const continuePoint = this.code.length;
        const continues = this.continueTargets.pop();
        for (const cp of continues) this.patchJumpTo(cp, continuePoint);

        this.compileExpr(node.condition);
        // If true, jump back to start
        this.emit(Op.JNZ);
        this.emitU16(loopStart);

        // Patch break jumps
        const breaks = this.breakTargets.pop();
        for (const bp of breaks) this.patchJump(bp);
    }

    compileTernary(node) {
        this.compileExpr(node.condition);
        const elseJump = this.emitJump(Op.JZ);
        this.compileExpr(node.consequent);
        const endJump = this.emitJump(Op.JMP);
        this.patchJump(elseJump);
        this.compileExpr(node.alternate);
        this.patchJump(endJump);
    }

    compileFor(node) {
        // Init
        if (node.init) {
            if (node.init.type === NodeType.VarDecl) {
                this.compileVarDecl(node.init);
            } else {
                this.compileExpr(node.init);
                if (this.exprLeavesValue(node.init)) {
                    this.emit(Op.POP);
                }
            }
        }

        const loopStart = this.code.length;

        // Condition
        if (node.condition) {
            this.compileExpr(node.condition);
        } else {
            this.emitPushInt(1); // infinite loop
        }
        const exitJump = this.emitJump(Op.JZ);

        this.breakTargets.push([]);
        this.continueTargets.push([]);  // collect continue patches

        this.compileStmt(node.body);

        // Continue target is the update expression — now we know its address
        const actualUpdateStart = this.code.length;
        const continues = this.continueTargets.pop();
        for (const cp of continues) this.patchJumpTo(cp, actualUpdateStart);

        // Update
        if (node.update) {
            this.compileExpr(node.update);
            if (this.exprLeavesValue(node.update)) {
                this.emit(Op.POP);
            }
        }

        this.emit(Op.JMP);
        this.emitU16(loopStart);
        this.patchJump(exitJump);

        // Patch break jumps
        const breaks = this.breakTargets.pop();
        for (const bp of breaks) this.patchJump(bp);
    }

    compileSwitch(node) {
        this.compileExpr(node.discriminant);

        this.breakTargets.push([]);
        const caseJumps = [];
        let defaultJump = null;

        // Emit case comparisons
        for (const clause of node.cases) {
            if (clause.isDefault) {
                defaultJump = this.emitJump(Op.JMP);
            } else {
                this.emit(Op.DUP);
                this.compileExpr(clause.value);
                this.emit(Op.EQ);
                const jump = this.emitJump(Op.JNZ);
                caseJumps.push({ jump, clause });
            }
        }

        // Jump to end if no match and no default
        const endJump = defaultJump ? null : this.emitJump(Op.JMP);
        if (defaultJump) {
            this.patchJump(defaultJump);
        }

        // Emit case bodies
        for (let i = 0; i < node.cases.length; i++) {
            const clause = node.cases[i];
            // Patch case comparison jump
            const cj = caseJumps.find(c => c.clause === clause);
            if (cj) {
                this.patchJump(cj.jump);
            }
            if (clause.isDefault && defaultJump) {
                // Already patched above
            }
            for (const stmt of clause.body) {
                this.compileStmt(stmt);
            }
        }

        if (endJump) this.patchJump(endJump);
        this.emit(Op.POP); // pop discriminant

        const breaks = this.breakTargets.pop();
        for (const bp of breaks) {
            this.patchJump(bp);
        }
    }

    compileReturn(node) {
        // Struct return: callee pushes all field slots, then Op.RET (which
        // preserves data stack across frame teardown — see VM source).
        const retType = this.currentFunction && this.currentFunction.returnType;
        if (node.value && typeof retType === 'string' && retType.startsWith('struct:')) {
            if (!this._isStructValueExpr(node.value)) {
                throw new CodeGenError(
                    `Function returns struct '${retType.slice(7)}' but value is not a struct`,
                    node.line);
            }
            const rm = this._resolveStructAccess(node.value);
            if ('struct:' + rm.tag !== retType) {
                throw new CodeGenError(
                    `Return type mismatch: expected '${retType}', got 'struct:${rm.tag}'`,
                    node.line);
            }
            const slots = rm.structSlots;
            for (let off = 0; off < slots; off++) {
                this._pushStructSlotOffset(rm, off);
                this._emitStructSlotLoad(rm);
            }
            this.emit(Op.RET);
            return;
        }
        if (node.value) {
            this.compileExpr(node.value);
            this.emit(Op.RET_VAL);
        } else {
            this.emit(Op.RET);
        }
    }

    compileBreak(node) {
        if (this.breakTargets.length === 0) {
            throw new CodeGenError("'break' outside loop/switch", node.line);
        }
        const jump = this.emitJump(Op.JMP);
        this.breakTargets[this.breakTargets.length - 1].push(jump);
    }

    compileContinue(node) {
        if (this.continueTargets.length === 0) {
            throw new CodeGenError("'continue' outside loop", node.line);
        }
        // Emit a forward-jump placeholder; target is patched after the loop update is known
        const jump = this.emitJump(Op.JMP);
        this.continueTargets[this.continueTargets.length - 1].push(jump);
    }

    compileExprStmt(node) {
        this.compileExpr(node.expr);
        // If the expression leaves a value on the stack, pop it
        // (assignments and calls may or may not leave values)
        if (this.exprLeavesValue(node.expr)) {
            this.emit(Op.POP);
        }
    }

    exprLeavesValue(node) {
        if (node.type === NodeType.Assignment || node.type === NodeType.ArrayAssign ||
            node.type === NodeType.MemberAssign || node.type === NodeType.MemberArrayAssign) {
            return false;
        }
        if (node.type === NodeType.CallExpr) {
            // Watch intrinsics
            if (node.name === 'snapshot') return false;
            if (node.name === 'changed' || node.name === 'delta' || node.name === 'written') return true;
            // tcpWriteArray() and fileWriteArray() return void
            if (node.name === 'tcpWriteArray' || node.name === 'fileWriteArray') return false;
            // udp() — modes 2,3 are void; modes 0,1,4,5,6,7 return int
            if (node.name === 'udp') {
                if (node.args.length >= 1 && node.args[0].type === NodeType.IntLiteral) {
                    const mode = node.args[0].value;
                    if (mode === 2 || mode === 3) return false;
                }
                return true;
            }
            const builtin = BUILTINS[node.name];
            if (builtin) return builtin.returns;
            const func = this.functions.get(node.name);
            if (func) return func.returnType !== 'void';
            // Fn-ptr call: read return type from the alias signature.
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
    }

    // ─── Expression compilation ─────────────────────────────

    compileExpr(node) {
        switch (node.type) {
            case NodeType.IntLiteral:
                this.emitPushInt(node.value);
                break;

            case NodeType.FloatLiteral:
                this.emitPushFloat(node.value);
                break;

            case NodeType.CharLiteral:
                this.emitPushInt(node.value);
                break;

            case NodeType.StringLiteral: {
                const idx = this.addConstant(node.value);
                this.emit(Op.LOAD_CONST);
                this.emitU16(idx);
                break;
            }

            case NodeType.BoolLiteral:
                this.emitPushInt(node.value ? 1 : 0);
                break;

            case NodeType.Identifier:
                this.compileIdentifier(node);
                break;

            case NodeType.BinaryExpr:
                this.compileBinaryExpr(node);
                break;

            case NodeType.UnaryExpr:
                this.compileUnaryExpr(node);
                break;

            case NodeType.PostfixExpr:
                this.compilePostfixExpr(node);
                break;

            case NodeType.CallExpr:
                this.compileCallExpr(node);
                break;

            case NodeType.ArrayAccess:
                this.compileArrayAccess(node);
                break;

            case NodeType.Assignment:
                this.compileAssignment(node);
                break;

            case NodeType.ArrayAssign:
                this.compileArrayAssign(node);
                break;

            case NodeType.CastExpr:
                this.compileCast(node);
                break;

            case NodeType.TernaryExpr:
                this.compileTernary(node);
                break;

            case NodeType.MemberAccess:
                this.compileMemberAccess(node);
                break;

            case NodeType.MemberAssign:
                this.compileMemberAssign(node);
                break;

            case NodeType.MemberArrayAccess:
                this.compileMemberArrayAccess(node);
                break;

            case NodeType.MemberArrayAssign:
                this.compileMemberArrayAssign(node);
                break;

            default:
                throw new CodeGenError(`Unknown expression type: ${node.type}`, node.line);
        }
    }

    _lookupFnPtrVar(name) {
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local && typeof local.type === 'string' && local.type.startsWith('fnptr:')) {
                return { info: local, isLocal: true, fnPtrAlias: local.type.slice(6) };
            }
        }
        const g = this.globals.get(name);
        if (g && typeof g.type === 'string' && g.type.startsWith('fnptr:')) {
            return { info: g, isLocal: false, fnPtrAlias: g.type.slice(6) };
        }
        return null;
    }
    compileIdentifier(node) {
        // Scalar reference param: read through the encoded ref at index 0.
        if (this.scope) {
            const sr = this.scope.lookup(node.name);
            if (sr && sr.isScalarRef) {
                this.emit(Op.PUSH_I8);     this.emitByte(0);
                this.emit(Op.LOAD_REF_ARR); this.emitByte(sr.index);
                return;
            }
        }
        // Function-name reference (no parens): emit PUSH_I32 of the function's
        // bytecode address. Local/global var names override (so a local 'foo'
        // shadows a function 'foo' — same C scoping rule).
        if ((!this.scope || !this.scope.lookup(node.name)) &&
            !this.globals.has(node.name) &&
            this.functions && this.functions.has(node.name)) {
            const func = this.functions.get(node.name);
            // Emit PUSH_I32 with a placeholder; record forward-ref for patching.
            this.emit(Op.PUSH_I32);
            const offset = this.code.length;
            this.code.push(0, 0, 0, 0);
            // PUSH_I32 reads big-endian (matches readI32 / firmware tc_read_i32).
            // Address fits in low 16 bits → high two bytes stay 0.
            if (func.address >= 0) {
                this.code[offset]     = 0;                              // bits 31..24
                this.code[offset + 1] = 0;                              // bits 23..16
                this.code[offset + 2] = (func.address >> 8) & 0xFF;     // bits 15..8
                this.code[offset + 3] =  func.address       & 0xFF;     // bits  7..0
            } else {
                this.fnAddrPatches.push({ name: node.name, offset });
            }
            return;
        }
        // Check locals first (locals shadow defines, like C preprocessor)
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local) {
                if (local.isStaticAlias) {
                    // Redirect to the global
                    const g = this.globals.get(local.globalName);
                    if (!g) throw new CodeGenError(`Static global '${local.globalName}' not found`, node.line);
                    this.emit(Op.LOAD_GLOBAL);
                    this.emitU16(g.index);
                    return;
                }
                this.emit(Op.LOAD_LOCAL);
                this.emitByte(local.index);
                return;
            }
        }

        // Check defines (only if no local with same name)
        if (this.defines.has(node.name)) {
            const defNode = this.defines.get(node.name);
            this.compileExpr(defNode);
            return;
        }

        // Check Tasmota system variables (tasm_xxx)
        const tasmVar = CodeGenerator.TASM_VARS[node.name];
        if (tasmVar) {
            this.emit(Op.PUSH_I8);
            this.emitByte(tasmVar.index);
            this.emit(Op.SYSCALL);
            this.emitByte(Syscall.TASM_GET);
            return;
        }

        // Check globals
        const global = this.globals.get(node.name);
        if (global) {
            this.emit(Op.LOAD_GLOBAL);
            this.emitU16(global.index);
            return;
        }

        throw new CodeGenError(`Undefined variable: ${node.name}`, node.line);
    }

    // Returns true if node evaluates to a char[] / string ref
    isStringNode(node) {
        if (node.type === NodeType.StringLiteral) return true;
        if (node.type === NodeType.Identifier && this.isCharArrayVar(node.name)) return true;
        if (node.type === NodeType.MemberAccess) {
            try {
                const { fieldIsArray, fieldType } = this.resolveMember(node.object, node.field, 0);
                return fieldIsArray && fieldType === 'char';
            } catch (e) { return false; }
        }
        if (node.type === NodeType.BinaryExpr && node.op === '+') {
            return this.isStringNode(node.left) && this.isStringNode(node.right);
        }
        return false;
    }

    // Emit a string argument as a pointer/ref on the stack
    // Handles: char[] identifiers, string literals (const pool ref), nested str concat,
    // and #define'd identifiers that resolve to string literals.
    emitStringArg(node) {
        // Resolve #define'd identifier to the underlying literal node
        if (node.type === NodeType.Identifier && this.defines.has(node.name)) {
            node = this.defines.get(node.name);
        }
        if (node.type === NodeType.StringLiteral) {
            // Encode as a const pool ref: tag=3 (0xC0000000), handle = 0x8000|constIdx
            const idx = this.addConstant(node.value);
            this.emitPushInt((0xC0008000 | idx) | 0);
            return;
        }
        if (node.type === NodeType.BinaryExpr && node.op === '+' && this.isStringNode(node)) {
            this.compileBinaryExpr(node); // leaves scratch_ref on stack
            return;
        }
        this.emitArrayRef(node); // char[] variable, member array field
    }

    compileBinaryExpr(node) {
        // String concatenation: char_arr + char_arr, char_arr + "lit", "lit" + char_arr
        if (node.op === '+' && this.isStringNode(node.left) && this.isStringNode(node.right)) {
            this.emitStringArg(node.left);
            this.emitStringArg(node.right);
            this.emitSyscall(Syscall.STRCONCAT); // → pushes scratch_ref
            return;
        }

        const leftType = this.inferType(node.left);
        const rightType = this.inferType(node.right);
        const isFloat = this.isFloatType(leftType) || this.isFloatType(rightType);

        // Short-circuit for && and ||
        if (node.op === '&&') {
            // Short-circuit: if left is false, result is 0; else result is right
            // JZ pops left — so only right remains on stack (or 0 if short-circuited)
            this.compileExpr(node.left);
            const skipRight = this.emitJump(Op.JZ);
            this.compileExpr(node.right);
            const end = this.emitJump(Op.JMP);
            this.patchJump(skipRight);
            this.emitPushInt(0);
            this.patchJump(end);
            return;
        }
        if (node.op === '||') {
            // Short-circuit: if left is true, result is 1; else result is right
            // JNZ pops left — so only right remains on stack (or 1 if short-circuited)
            this.compileExpr(node.left);
            const skipRight = this.emitJump(Op.JNZ);
            this.compileExpr(node.right);
            const end = this.emitJump(Op.JMP);
            this.patchJump(skipRight);
            this.emitPushInt(1);
            this.patchJump(end);
            return;
        }

        this.compileExpr(node.left);
        if (isFloat && !this.isFloatType(leftType)) this.emit(Op.I2F);

        this.compileExpr(node.right);
        if (isFloat && !this.isFloatType(rightType)) this.emit(Op.I2F);

        if (isFloat) {
            switch (node.op) {
                case '+': this.emit(Op.FADD); break;
                case '-': this.emit(Op.FSUB); break;
                case '*': this.emit(Op.FMUL); break;
                case '/': this.emit(Op.FDIV); break;
                case '==': this.emit(Op.FEQ); break;
                case '!=': this.emit(Op.FNEQ); break;
                case '<':  this.emit(Op.FLT); break;
                case '>':  this.emit(Op.FGT); break;
                case '<=': this.emit(Op.FLTE); break;
                case '>=': this.emit(Op.FGTE); break;
                default:
                    throw new CodeGenError(`Float operator not supported: ${node.op}`, node.line);
            }
        } else {
            switch (node.op) {
                case '+': this.emit(Op.ADD); break;
                case '-': this.emit(Op.SUB); break;
                case '*': this.emit(Op.MUL); break;
                case '/': this.emit(Op.DIV); break;
                case '%': this.emit(Op.MOD); break;
                case '&': this.emit(Op.BIT_AND); break;
                case '|': this.emit(Op.BIT_OR); break;
                case '^': this.emit(Op.BIT_XOR); break;
                case '<<': this.emit(Op.SHL); break;
                case '>>': this.emit(Op.SHR); break;
                case '==': this.emit(Op.EQ); break;
                case '!=': this.emit(Op.NEQ); break;
                case '<':  this.emit(Op.LT); break;
                case '>':  this.emit(Op.GT); break;
                case '<=': this.emit(Op.LTE); break;
                case '>=': this.emit(Op.GTE); break;
                default:
                    throw new CodeGenError(`Unknown operator: ${node.op}`, node.line);
            }
        }
    }

    compileUnaryExpr(node) {
        this.compileExpr(node.operand);
        const type = this.inferType(node.operand);
        const isFloat = this.isFloatType(type);

        switch (node.op) {
            case '-':
                this.emit(isFloat ? Op.FNEG : Op.NEG);
                break;
            case '!':
                this.emit(Op.LOGIC_NOT);
                break;
            case '~':
                this.emit(Op.BIT_NOT);
                break;
            case 'pre++':
                this.emitPushInt(1);
                this.emit(Op.ADD);
                this.emit(Op.DUP);
                this.emitStore(node.operand);
                break;
            case 'pre--':
                this.emitPushInt(1);
                this.emit(Op.SUB);
                this.emit(Op.DUP);
                this.emitStore(node.operand);
                break;
            default:
                throw new CodeGenError(`Unknown unary op: ${node.op}`, node.line);
        }
    }

    compilePostfixExpr(node) {
        // Load current value
        this.compileExpr(node.operand);
        this.emit(Op.DUP); // keep original value on stack as result

        // Increment/decrement and store
        this.emitPushInt(1);
        if (node.op === '++') {
            this.emit(Op.ADD);
        } else {
            this.emit(Op.SUB);
        }
        this.emitStore(node.operand);
    }

    emitStore(node) {
        if (node.type === NodeType.Identifier) {
            if (this.scope) {
                const local = this.scope.lookup(node.name);
                if (local) {
                    this.emit(Op.STORE_LOCAL);
                    this.emitByte(local.index);
                    return;
                }
            }
            const global = this.globals.get(node.name);
            if (global) {
                const watchInfo = this.watchGlobals.get(node.name);
                if (watchInfo) {
                    this.emit(Op.STORE_WATCH);
                    this.emitU16(watchInfo.varIndex);
                    this.emitU16(watchInfo.shadowIndex);
                    this.emitU16(watchInfo.writtenIndex);
                } else {
                    this.emitStoreGlobal(global.index, node.name);
                }
                return;
            }
            throw new CodeGenError(`Undefined variable: ${node.name}`, node.line);
        }
        throw new CodeGenError('Cannot store to non-identifier', node.line);
    }

    // Check if a global is a scalar UDP global (auto-send on assignment)
    getUdpScalarInfo(name) {
        for (const entry of this.udpGlobals) {
            if (entry.name === name && entry.varType === 0) {
                return entry;
            }
        }
        return null;
    }

    // Emit STORE_GLOBAL or STORE_GLOBAL_UDP for a global variable
    emitStoreGlobal(globalIndex, name) {
        const udpInfo = this.getUdpScalarInfo(name);
        if (udpInfo) {
            const nameIdx = this.addConstant(name);
            this.emit(Op.STORE_GLOBAL_UDP);
            this.emitU16(globalIndex);
            this.emitU16(nameIdx);
        } else {
            this.emit(Op.STORE_GLOBAL);
            this.emitU16(globalIndex);
        }
    }

    // Check if a variable name refers to any array (char[], int[], float[])
    isArrayVar(name) {
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local && local.isArray) return true;
        }
        const global = this.globals.get(name);
        if (global && global.isArray) return true;
        return false;
    }

    // Check if a variable name refers to a char[] (string buffer)
    isCharArrayVar(name) {
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local && local.isArray && local.type === 'char') return true;
        }
        const global = this.globals.get(name);
        if (global && global.isArray && global.type === 'char') return true;
        return false;
    }

    // Emit ADDR_LOCAL, ADDR_GLOBAL, or ADDR_HEAP for a named array variable
    emitArrayRefByName(name, line) {
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local && local.isArray) {
                if (local.isRef) {
                    // Array ref parameter — local holds heap ref, just load it
                    this.emit(Op.LOAD_LOCAL);
                    this.emitByte(local.index);
                    return;
                }
                if (local.isHeap) {
                    this.emit(Op.ADDR_HEAP);
                    this.emitByte(local.heapHandle);
                    return;
                }
                this.emit(Op.ADDR_LOCAL);
                this.emitByte(local.index);
                return;
            }
        }
        const global = this.globals.get(name);
        if (global && global.isArray) {
            if (global.isHeap) {
                this.emit(Op.ADDR_HEAP);
                this.emitByte(global.heapHandle);
                return;
            }
            this.emit(Op.ADDR_GLOBAL);
            this.emitU16(global.index);
            return;
        }
        throw new CodeGenError(`Not an array: ${name}`, line);
    }

    // Phase 2 helper: returns true when the AST node is arr[i] of a
    // 2D char array (i.e. a row reference candidate). Called from the
    // sprintf dispatcher to route %s args through emitArrayRef.
    is2DCharArrayAccess(node) {
        if (!node || node.type !== NodeType.ArrayAccess) return false;
        if (node.index2 != null) return false;     // element access [i][j]
        const sym = (this.scope && this.scope.lookup(node.name)) ||
                    this.globals.get(node.name) || null;
        return !!(sym && sym.cols && sym.cols > 1 && sym.type === 'char');
    }

    // Emit an array address ref (for string functions)
    emitArrayRef(node) {
        // String concatenation expression: str1 + str2, str1 + "lit", etc.
        // Compiles to STRCONCAT syscall which pushes scratch_ref onto the stack.
        if (node.type === NodeType.BinaryExpr && node.op === '+' && this.isStringNode(node)) {
            this.compileBinaryExpr(node);
            return;
        }
        // ── Phase 1 2D row reference: arr[i] of a 2D char array → ADDR_HEAP_OFF
        if (node.type === NodeType.ArrayAccess && node.index2 == null) {
            const sym = (this.scope && this.scope.lookup(node.name)) ||
                        this.globals.get(node.name) || null;
            if (sym && sym.cols && sym.cols > 1) {
                if (!sym.isHeap) {
                    throw new CodeGenError(
                        `Row references for 2D arrays require heap storage ` +
                        `(total >= ${typeof HEAP_THRESHOLD === 'number' ? HEAP_THRESHOLD + 1 : 17} ` +
                        `elements). '${node.name}' is too small (${sym.rows}x${sym.cols}).`,
                        node.line);
                }
                // Emit row offset = index * cols, then ADDR_HEAP_OFF.
                this.compileExpr(node.index);
                this.emitPushInt(sym.cols);
                this.emit(Op.MUL);
                this.emit(Op.ADDR_HEAP_OFF);
                this.emitByte(sym.heapHandle);
                return;
            }
        }
        // Handle struct member array field: msg.text passed to strcpy(msg.text, ...)
        if (node.type === NodeType.MemberAccess) {
            const rm = this.resolveMember(node.object, node.field, node.line);
            const { info, isLocal, fieldOffset, fieldIsArray, isArrayElement } = rm;
            if (!fieldIsArray) {
                throw new CodeGenError(
                    `Field '${node.field}' is not a char array`, node.line);
            }
            // Heap-resident struct (or heap struct array element): the VM's
            // heap ref encoding carries a 14-bit slot offset packed alongside
            // the 8-bit handle. Compute `i * structSlots + fieldOffset` (or
            // just `fieldOffset` for a flat heap struct) and emit ADDR_HEAP_OFF
            // so all downstream string syscalls (strcpy, strcat, …) see the
            // sub-array starting at that offset.
            if (info.isHeap) {
                this.emitStructOffset(rm);
                this.emit(Op.ADDR_HEAP_OFF);
                this.emitByte(info.heapHandle);
                return;
            }
            // Non-heap struct-array element (rare — small arrays kept on stack
            // or in globals): the current ref encoding for ADDR_LOCAL/
            // ADDR_GLOBAL only supports a compile-time constant offset, so a
            // runtime index can't be packed in. Require char-by-char copy.
            if (isArrayElement) {
                throw new CodeGenError(
                    `Cannot take a string reference to '${node.field}' of a non-heap struct array element. ` +
                    `Copy char-by-char via 'arr[i].${node.field}[k] = src[k]', or make the array large enough to live on the heap.`,
                    node.line);
            }
            if (isLocal) {
                this.emit(Op.ADDR_LOCAL);
                this.emitByte(info.index + fieldOffset);
            } else {
                this.emit(Op.ADDR_GLOBAL);
                this.emitU16(info.index + fieldOffset);
            }
            return;
        }
        if (node.type !== NodeType.Identifier) {
            throw new CodeGenError('String functions require array variable, not expression', node.line);
        }
        // Check locals
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local && local.isArray) {
                if (local.isRef) {
                    // Array ref parameter — local holds heap ref, just load it
                    this.emit(Op.LOAD_LOCAL);
                    this.emitByte(local.index);
                    return;
                }
                if (local.isHeap) {
                    this.emit(Op.ADDR_HEAP);
                    this.emitByte(local.heapHandle);
                    return;
                }
                this.emit(Op.ADDR_LOCAL);
                this.emitByte(local.index);
                return;
            }
        }
        // Check globals
        const global = this.globals.get(node.name);
        if (global && global.isArray) {
            if (global.isHeap) {
                this.emit(Op.ADDR_HEAP);
                this.emitByte(global.heapHandle);
                return;
            }
            this.emit(Op.ADDR_GLOBAL);
            this.emitU16(global.index);
            return;
        }
        throw new CodeGenError(`'${node.name}' is not an array`, node.line);
    }

    // Emit file mode: convert string literal "r"/"w"/"a" → PUSH_INT 0/1/2
    // Also accepts integer expressions (0/1/2) directly
    emitFileMode(arg, node) {
        const resolved = (arg.type === NodeType.Identifier && this.defines.has(arg.name))
            ? this.defines.get(arg.name) : arg;
        if (resolved.type === NodeType.StringLiteral) {
            const ch = resolved.value.charAt(0);
            if (ch === 'r') { this.emitPushInt(0); return; }
            if (ch === 'w') { this.emitPushInt(1); return; }
            if (ch === 'a') { this.emitPushInt(2); return; }
            throw new CodeGenError(`fileOpen mode must be "r", "w", or "a", got "${resolved.value}"`, arg.line || node.line);
        }
        // Allow integer expression (0/1/2)
        this.compileExpr(arg);
    }

    // Emit address ref for any variable (scalar or array) — used by widget refArgs
    emitVarRef(node) {
        if (node.type !== NodeType.Identifier) {
            throw new CodeGenError('Widget functions require a variable name', node.line);
        }
        // Check locals
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local) {
                if (local.isRef) {
                    // Array ref parameter — local holds heap ref, just load it
                    this.emit(Op.LOAD_LOCAL);
                    this.emitByte(local.index);
                    return;
                }
                if (local.isHeap) {
                    this.emit(Op.ADDR_HEAP);
                    this.emitByte(local.heapHandle);
                    return;
                }
                this.emit(Op.ADDR_LOCAL);
                this.emitByte(local.index);
                return;
            }
        }
        // Check globals
        const global = this.globals.get(node.name);
        if (global) {
            if (global.isHeap) {
                this.emit(Op.ADDR_HEAP);
                this.emitByte(global.heapHandle);
                return;
            }
            this.emit(Op.ADDR_GLOBAL);
            this.emitU16(global.index);
            return;
        }
        throw new CodeGenError(`Unknown variable: '${node.name}'`, node.line);
    }

    compileCallExpr(node) {
        // Indirect call through a struct field: `obj.handler(args)` or
        // `arr[i].handler(args)`. The parser left node.callee = the
        // MemberAccess/MemberArrayAccess node and node.name = null.
        if (node.callee && (node.callee.type === NodeType.MemberAccess
                         || node.callee.type === NodeType.MemberArrayAccess)) {
            const rm = this.resolveMember(node.callee.object, node.callee.field, node.callee.line || node.line);
            if (typeof rm.fieldType !== 'string' || !rm.fieldType.startsWith('fnptr:')) {
                throw new CodeGenError(
                    `Field '${node.callee.field}' is not a function pointer — cannot call`,
                    node.callee.line || node.line);
            }
            const alias = rm.fieldType.slice(6);
            const sig = this.fnPtrTypes.get(alias);
            if (!sig) {
                throw new CodeGenError(`Unknown fn-ptr type 'fnptr:${alias}' for field '${node.callee.field}'`,
                                       node.callee.line || node.line);
            }
            if (node.args.length !== sig.params.length) {
                throw new CodeGenError(
                    `fn-ptr field '${node.callee.field}' expects ${sig.params.length} args, got ${node.args.length}`,
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

        // sizeof(Tag) — compile-time constant slot count.
        if (node.name === 'sizeof' && node.args.length === 1
                                   && node.args[0].type === NodeType.Identifier) {
            const tag = node.args[0].name;
            if (this.structTypes.has(tag)) {
                const slots = this.memberSlotCount(this.structTypes.get(tag));
                this.emitPushInt(slots);
                return;
            }
            // Primitive type names → 1 (slot)
            if (tag === 'int' || tag === 'float' || tag === 'char' || tag === 'bool') {
                this.emitPushInt(1);
                return;
            }
            throw new CodeGenError(`sizeof: unknown type '${tag}'`, node.line);
        }
        // Check watch intrinsics (changed, delta, written, snapshot)
        if (node.name === 'changed' || node.name === 'delta' ||
            node.name === 'written' || node.name === 'snapshot') {
            return this.compileWatchIntrinsic(node);
        }

        // Check general-purpose UDP function (variable args per mode)
        if (node.name === 'udp') {
            return this.compileUdpFunc(node);
        }

        // tcpWriteArray — optional type argument: tcpWriteArray(array, num) or tcpWriteArray(array, num, type)
        if (node.name === 'tcpWriteArray') {
            return this.compileTcpWriteArray(node);
        }

        // tcpConnect — string literal → TCP_CONNECT, char[] runtime arg → TCP_CONNECT_REF
        if (node.name === 'tcpConnect') {
            return this.compileTcpConnect(node);
        }

        // spawnTask("Name") or spawnTask("Name", stack_kb), killTask("Name"), taskRunning("Name")
        // — register Name in the function table so the firmware can resolve it
        // from vm->callbacks[] when the worker task starts/stops.
        if (node.name === 'spawnTask' || node.name === 'killTask' || node.name === 'taskRunning') {
            return this.compileSpawnApi(node);
        }

        // fileWriteArray — optional append flag: fileWriteArray(arr, handle) or fileWriteArray(arr, handle, append)
        if (node.name === 'fileWriteArray') {
            return this.compileFileWriteArray(node);
        }

        // timeOffset — optional zeroFlag: timeOffset(buf, days) or timeOffset(buf, days, zeroFlag)
        if (node.name === 'timeOffset') {
            return this.compileTimeOffset(node);
        }

        // sprintf(dst, fmt, val) / sprintfAppend(dst, fmt, val) — unified, auto-detects type
        if (node.name === 'sprintf' || node.name === 'sprintfAppend') {
            return this.compileSprintfUnified(node);
        }

        // fileExtract / fileExtractFast — variable number of array args
        if (node.name === 'fileExtract') {
            return this.compileFileExtract(node, Syscall.FILE_EXTRACT);
        }
        if (node.name === 'fileExtractFast') {
            return this.compileFileExtract(node, Syscall.FILE_EXTRACT_FAST);
        }

        // print(charArray) → auto-switch to printString (STR_PRINT)
        if (node.name === 'print' && node.args.length === 1 &&
            node.args[0].type === NodeType.Identifier &&
            this.isCharArrayVar(node.args[0].name)) {
            this.emitArrayRefByName(node.args[0].name, node.line);
            this.emit(Op.SYSCALL);
            this.emitByte(Syscall.STR_PRINT);
            return;
        }
        // print("literal") → auto-switch to printStr (DEBUG_PRINT_STR)
        if (node.name === 'print' && node.args.length === 1 &&
            node.args[0].type === NodeType.StringLiteral) {
            const idx = this.addConstant(node.args[0].value);
            this.emit(Op.LOAD_CONST);
            this.emitU16(idx);
            this.emit(Op.SYSCALL);
            this.emitByte(Syscall.DEBUG_PRINT_STR);
            return;
        }

        // Variadic addLog("fmt", val, ...) / addLogLevel(level, "fmt", val, ...)
        // Compile-time desugar:
        //   addLog("x=%d", x)            → sprintf(scratch, "x=%d", x); addLog(scratch);
        //   addLogLevel(LVL, "x=%d", x)  → sprintf(scratch, "x=%d", x); addLogLevel(LVL, scratch);
        // Must run before the BUILTINS arity check (which would reject the
        // extra args). The format arg has to be a string literal so we can
        // split it at compile time. Calls with no extra args fall through to
        // the existing single-arg LOG_STR / LOG_LEVEL_STR fast paths below.
        {
            const resolveArgEarly = (arg) => {
                if (arg.type === NodeType.Identifier && this.defines.has(arg.name)) {
                    return this.defines.get(arg.name);
                }
                return arg;
            };
            const isVarAddLog = (node.name === 'addLog' && node.args.length >= 2 &&
                                 resolveArgEarly(node.args[0]).type === NodeType.StringLiteral);
            const isVarAddLogLevel = (node.name === 'addLogLevel' && node.args.length >= 3 &&
                                      resolveArgEarly(node.args[1]).type === NodeType.StringLiteral);
            if (isVarAddLog || isVarAddLogLevel) {
                const fmtArgIdx = isVarAddLog ? 0 : 1;
                const fmtStr = resolveArgEarly(node.args[fmtArgIdx]).value;
                const valArgs = node.args.slice(fmtArgIdx + 1);

                this.emitSprintfChain(
                    () => this.emitLogScratchRef(),
                    fmtStr,
                    valArgs,
                    /*isAppend=*/false,
                    node.name,
                    node.line
                );
                this.emit(Op.POP);  // discard sprintf return (length) — addLog returns void

                if (isVarAddLogLevel) {
                    this.compileExpr(node.args[0]);  // log level
                    this.emitLogScratchRef();
                    this.emitSyscall(Syscall.LOG_LEVEL);
                } else {
                    this.emitLogScratchRef();
                    this.emit(Op.SYSCALL);
                    this.emitByte(Syscall.LOG);
                }
                return;
            }
        }

        // Check built-in functions
        const builtin = BUILTINS[node.name];
        if (builtin) {
            if (node.args.length !== builtin.args) {
                throw new CodeGenError(
                    `${node.name} expects ${builtin.args} args, got ${node.args.length}`,
                    node.line
                );
            }

            // Helper: resolve defines to their underlying node (for string literal detection)
            const resolveArg = (arg) => {
                if (arg.type === NodeType.Identifier && this.defines.has(arg.name)) {
                    return this.defines.get(arg.name);
                }
                return arg;
            };

            // String functions: handle special argument types
            if (builtin.strArgs) {
                // Check for string literal as source in strcpy/strcat → use CONST variant
                if ((node.name === 'strcpy' || node.name === 'strcat') &&
                    node.args.length === 2 && resolveArg(node.args[1]).type === NodeType.StringLiteral) {
                    const resolved = resolveArg(node.args[1]);
                    this.emitArrayRef(node.args[0]);  // dst ref
                    const idx = this.addConstant(resolved.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(node.name === 'strcpy' ? Syscall.STRCPY_CONST : Syscall.STRCAT_CONST);
                    return;
                }
                // strcmp(arr, "literal") or strcmp("literal", arr) → use STRCMP_CONST variant
                if (node.name === 'strcmp' && node.args.length === 2) {
                    const arg0 = resolveArg(node.args[0]);
                    const arg1 = resolveArg(node.args[1]);
                    if (arg1.type === NodeType.StringLiteral) {
                        // strcmp(arr, "literal") → STRCMP_CONST(arr_ref, const_idx)
                        this.emitArrayRef(node.args[0]);
                        const idx = this.addConstant(arg1.value);
                        this.emit(Op.LOAD_CONST); this.emitU16(idx);
                        this.emitSyscall(Syscall.STRCMP_CONST);
                        if (builtin.returns) this.pendingPush = true;
                        return;
                    }
                    if (arg0.type === NodeType.StringLiteral) {
                        // strcmp("literal", arr) → negate STRCMP_CONST(arr_ref, const_idx)
                        this.emitArrayRef(node.args[1]);
                        const idx = this.addConstant(arg0.value);
                        this.emit(Op.LOAD_CONST); this.emitU16(idx);
                        this.emitSyscall(Syscall.STRCMP_CONST);
                        this.emit(Op.NEG);  // reverse ordering for strcmp("lit", arr)
                        if (builtin.returns) this.pendingPush = true;
                        return;
                    }
                }
                // fileWrite(handle, "literal", len) → use FILE_WRITE_STR variant
                if (node.name === 'fileWrite' && node.args.length === 3) {
                    const arg1 = resolveArg(node.args[1]);
                    if (arg1.type === NodeType.StringLiteral) {
                        this.compileExpr(node.args[0]);  // handle
                        const idx = this.addConstant(arg1.value);
                        this.emit(Op.LOAD_CONST); this.emitU16(idx);
                        this.compileExpr(node.args[2]);  // len
                        this.emitSyscall(Syscall.FILE_WRITE_STR);
                        if (builtin.returns) this.pendingPush = true;
                        return;
                    }
                }
                // strFind(arr, "literal") → use STR_FIND_CONST variant
                if (node.name === 'strFind' &&
                    node.args.length === 2 && resolveArg(node.args[1]).type === NodeType.StringLiteral) {
                    const resolved = resolveArg(node.args[1]);
                    this.emitArrayRef(node.args[0]);  // haystack ref
                    const idx = this.addConstant(resolved.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(Syscall.STR_FIND_CONST);
                    if (builtin.returns) this.pendingPush = true;
                    return;
                }
                // sprintfStr/sprintfAppendStr with string literal as 3rd arg → use _CONST variant
                if ((node.name === 'sprintfStr' || node.name === 'sprintfAppendStr') &&
                    node.args.length === 3 && resolveArg(node.args[2]).type === NodeType.StringLiteral) {
                    const resolvedSrc = resolveArg(node.args[2]);
                    this.emitArrayRef(node.args[0]);  // dst ref
                    const fmtIdx = this.addConstant(resolveArg(node.args[1]).value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(fmtIdx);
                    const srcIdx = this.addConstant(resolvedSrc.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(srcIdx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(node.name === 'sprintfStr' ? Syscall.SPRINTF_STR_CONST : Syscall.SPRINTF_STR_CAT_CONST);
                    return;
                }
                // addLogLevel(level, "literal") → use LOG_LEVEL_STR variant
                if (node.name === 'addLogLevel' && node.args.length === 2 && resolveArg(node.args[1]).type === NodeType.StringLiteral) {
                    this.compileExpr(node.args[0]); // level
                    const resolved = resolveArg(node.args[1]);
                    const idx = this.addConstant(resolved.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emitSyscall(Syscall.LOG_LEVEL_STR);
                    return;
                }
                // tasmDefer("literal") → use _STR variant.
                // emitSyscall is required because TASM_DEFER_STR (367) > 255 — emitByte would truncate to 111.
                if (node.name === 'tasmDefer' && node.args.length === 1 &&
                    resolveArg(node.args[0]).type === NodeType.StringLiteral) {
                    const idx = this.addConstant(resolveArg(node.args[0]).value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emitSyscall(Syscall.TASM_DEFER_STR);
                    return;
                }
                // webSend("literal") / responseAppend("literal") / addLog("literal") / mailBody("literal") / responseCmnd("literal") → use _STR variant
                if ((node.name === 'webSend' || node.name === 'responseAppend' || node.name === 'addLog' || node.name === 'mailBody' || node.name === 'responseCmnd') &&
                    node.args.length === 1 && resolveArg(node.args[0]).type === NodeType.StringLiteral) {
                    const resolved = resolveArg(node.args[0]);
                    const idx = this.addConstant(resolved.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    const strSyscall = node.name === 'webSend' ? Syscall.WEB_SEND_STR :
                                       node.name === 'addLog' ? Syscall.LOG_STR :
                                       node.name === 'mailBody' ? Syscall.EMAIL_BODY_STR :
                                       node.name === 'responseCmnd' ? Syscall.RESPONSE_CMND_STR : Syscall.RESPONSE_APPEND_STR;
                    this.emitByte(strSyscall);
                    return;
                }
                // dspDraw("literal") / dspText("literal") → use _STR variant
                if ((node.name === 'dspDraw' || node.name === 'dspText') &&
                    node.args.length === 1 && resolveArg(node.args[0]).type === NodeType.StringLiteral) {
                    const resolved = resolveArg(node.args[0]);
                    const idx = this.addConstant(resolved.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(node.name === 'dspDraw' ? Syscall.DSP_DRAW_STR : Syscall.DSP_TEXT_STR);
                    return;
                }
                // smlWrite(meter, "hex") / smlSetWStr(meter, "hex") → use _STR variant
                if ((node.name === 'smlWrite' || node.name === 'smlSetWStr') &&
                    node.args.length === 2 && resolveArg(node.args[1]).type === NodeType.StringLiteral) {
                    this.compileExpr(node.args[0]);  // meter index
                    const resolved = resolveArg(node.args[1]);
                    const idx = this.addConstant(resolved.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(node.name === 'smlWrite' ? Syscall.SML_WRITE_STR : Syscall.SML_SETWSTR_STR);
                    return;
                }
            }

            // tasmCmd with char array command (not string literal) → use TASM_CMD_REF variant
            if (node.name === 'tasmCmd' && node.args.length === 2 &&
                resolveArg(node.args[0]).type !== NodeType.StringLiteral) {
                this.emitArrayRef(node.args[0]);  // cmd ref (char array)
                this.emitArrayRef(node.args[1]);  // response ref (char array)
                this.emit(Op.SYSCALL);
                this.emitByte(Syscall.TASM_CMD_REF);
                if (builtin.returns) this.hasValue = true;
                return;
            }

            // fileOpen with char array path (not string literal) → use FILE_OPEN_REF variant
            if (node.name === 'fileOpen' && node.args.length === 2 &&
                resolveArg(node.args[0]).type !== NodeType.StringLiteral) {
                this.emitArrayRef(node.args[0]);  // path ref (char array)
                this.emitFileMode(node.args[1], node);  // mode → int 0/1/2
                this.emit(Op.SYSCALL);
                this.emitByte(Syscall.FILE_OPEN_REF);
                if (builtin.returns) this.hasValue = true;
                return;
            }

            // fileExists with char array path → use FILE_EXISTS_REF variant
            if (node.name === 'fileExists' && node.args.length === 1 &&
                resolveArg(node.args[0]).type !== NodeType.StringLiteral) {
                this.emitArrayRef(node.args[0]);  // path ref (char array)
                this.emit(Op.SYSCALL);
                this.emitByte(Syscall.FILE_EXISTS_REF);
                if (builtin.returns) this.hasValue = true;
                return;
            }

            // fileDelete with char array path → use FILE_DELETE_REF variant
            if (node.name === 'fileDelete' && node.args.length === 1 &&
                resolveArg(node.args[0]).type !== NodeType.StringLiteral) {
                this.emitArrayRef(node.args[0]);  // path ref (char array)
                this.emit(Op.SYSCALL);
                this.emitByte(Syscall.FILE_DELETE_REF);
                if (builtin.returns) this.hasValue = true;
                return;
            }

            // fileOpenDir with char array path → use FILE_OPENDIR_REF variant
            if (node.name === 'fileOpenDir' && node.args.length === 1 &&
                resolveArg(node.args[0]).type !== NodeType.StringLiteral) {
                this.emitArrayRef(node.args[0]);  // path ref (char array)
                this.emit(Op.SYSCALL);
                this.emitByte(Syscall.FILE_OPENDIR_REF);
                if (builtin.returns) this.hasValue = true;
                return;
            }

            // fileOpen with string literal path: push path as const, mode as int 0/1/2
            if (node.name === 'fileOpen' && node.args.length === 2 &&
                resolveArg(node.args[0]).type === NodeType.StringLiteral) {
                const resolved = resolveArg(node.args[0]);
                const idx = this.addConstant(resolved.value);
                this.emit(Op.LOAD_CONST);
                this.emitU16(idx);
                this.emitFileMode(node.args[1], node);  // mode → int 0/1/2
                this.emit(Op.SYSCALL);
                this.emitByte(builtin.syscall);
                if (builtin.returns) this.hasValue = true;
                return;
            }

            // Special argument handling: strArgs (array refs), constArgs (string literal → const pool), refArgs (any variable ref), floatArgs (ensure float), intArgs (ensure int)
            if (builtin.strArgs || builtin.constArgs || builtin.refArgs || builtin.floatArgs || builtin.intArgs) {
                for (let i = 0; i < node.args.length; i++) {
                    const arg = node.args[i];
                    const resolved = resolveArg(arg);
                    if (builtin.constArgs && builtin.constArgs.includes(i)) {
                        // Must be a string literal → push as constant pool index
                        if (resolved.type !== NodeType.StringLiteral) {
                            throw new CodeGenError(`${node.name} argument ${i + 1} must be a string literal`, arg.line || node.line);
                        }
                        const idx = this.addConstant(resolved.value);
                        this.emit(Op.LOAD_CONST);
                        this.emitU16(idx);
                    } else if (builtin.refArgs && builtin.refArgs.includes(i)) {
                        // Variable ref (scalar or array) → push as address ref
                        this.emitVarRef(arg);
                    } else if (builtin.strArgs && builtin.strArgs.includes(i)) {
                        // String-literal acceptance: encode as const-pool
                        // ref so the firmware syscall handler (which reads
                        // via tc_ref_to_cstr) sees it the same as a char[]
                        // buffer. Same encoding pattern as helper-function
                        // char[] params receiving a literal. For a non-
                        // literal, fall through to emitArrayRef for the
                        // existing identifier/member-access paths.
                        if (resolved.type === NodeType.StringLiteral) {
                            const idx = this.addConstant(resolved.value);
                            this.emitPushInt((0xC0008000 | idx) | 0);
                        } else {
                            this.emitArrayRef(arg);
                        }
                    } else {
                        this.compileExpr(arg);
                        // floatArgs: ensure value is float (emit I2F if int expression)
                        if (builtin.floatArgs && builtin.floatArgs.includes(i)) {
                            const valType = this.inferType(arg);
                            if (valType !== 'float') this.emit(Op.I2F);
                        }
                        // intArgs: ensure value is int (emit F2I if float expression)
                        if (builtin.intArgs && builtin.intArgs.includes(i)) {
                            const valType = this.inferType(arg);
                            if (valType === 'float') this.emit(Op.F2I);
                        }
                    }
                }
                this.emitSyscall(builtin.syscall);
                return;
            }

            // Regular builtins: push arguments normally
            for (const arg of node.args) {
                this.compileExpr(arg);
            }
            this.emitSyscall(builtin.syscall);
            return;
        }

        // Function-pointer call site: if name resolves to a fnptr var, treat
        // the call as indirect — push args, then push the var's value (the
        // function's bytecode address), then emit Op.CALL_INDIRECT.
        const fpVar = this._lookupFnPtrVar(node.name);
        if (fpVar) {
            const sig = this.fnPtrTypes.get(fpVar.fnPtrAlias);
            if (sig) {
                if (node.args.length !== sig.params.length) {
                    throw new CodeGenError(
                        `fn-ptr '${node.name}' expects ${sig.params.length} args, got ${node.args.length}`,
                        node.line);
                }
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
                // Push the fn-ptr value (function's bytecode address)
                this.compileExpr({ type: NodeType.Identifier, name: node.name, line: node.line });
                this.emit(Op.CALL_INDIRECT);
                if (sig.returnType !== 'void') this.hasValue = true;
                return;
            }
        }
        // User-defined function
        const func = this.functions.get(node.name);
        if (!func) {
            throw new CodeGenError(`Undefined function: ${node.name}`, node.line);
        }
        if (node.args.length !== func.params.length) {
            throw new CodeGenError(
                `${node.name} expects ${func.params.length} args, got ${node.args.length}`,
                node.line
            );
        }

        // Push arguments (they become locals in the callee)
        for (let i = 0; i < node.args.length; i++) {
            const param = func.params[i];
            // Scalar-reference parameter (int& a, etc.): push the encoded
            // ref of the variable (ADDR_LOCAL or ADDR_GLOBAL), not its value.
            if (param.isScalarRef) {
                const arg = node.args[i];
                if (!arg || arg.type !== NodeType.Identifier) {
                    throw new CodeGenError(
                        `Function '${node.name}' arg ${i+1}: ref parameter requires a variable name (got ${arg && arg.type})`,
                        node.line);
                }
                const localR = this.scope ? this.scope.lookup(arg.name) : null;
                if (localR) {
                    if (localR.isStaticAlias) {
                        const g = this.globals.get(localR.globalName);
                        if (!g) throw new CodeGenError(`Static global '${localR.globalName}' missing`, node.line);
                        this.emit(Op.ADDR_GLOBAL); this.emitU16(g.index);
                    } else if (localR.isHeap) {
                        throw new CodeGenError(`Cannot pass heap-array variable '${arg.name}' as ref param (v1)`, node.line);
                    } else {
                        this.emit(Op.ADDR_LOCAL); this.emitByte(localR.index);
                    }
                } else {
                    const g = this.globals.get(arg.name);
                    if (!g) throw new CodeGenError(`Undefined variable '${arg.name}' in ref arg`, node.line);
                    this.emit(Op.ADDR_GLOBAL); this.emitU16(g.index);
                }
                continue;
            }
            // Struct-typed parameter: push N slot values from the arg.
            if (typeof param.type === 'string' && param.type.startsWith('struct:')) {
                const tag = param.type.slice(7);
                const members = this.structTypes.get(tag);
                if (!members) {
                    throw new CodeGenError(`Unknown struct '${tag}' in call`, node.line);
                }
                const slots = this.memberSlotCount(members);
                const arg = node.args[i];
                if (!this._isStructValueExpr(arg)) {
                    throw new CodeGenError(
                        `Function '${node.name}' arg ${i+1} expects struct '${tag}'`,
                        node.line);
                }
                const rm = this._resolveStructAccess(arg);
                if (rm.tag !== tag) {
                    throw new CodeGenError(
                        `Function '${node.name}' arg ${i+1}: struct '${rm.tag}' is not '${tag}'`,
                        node.line);
                }
                for (let s = 0; s < slots; s++) {
                    this._pushStructSlotOffset(rm, s);
                    this._emitStructSlotLoad(rm);
                }
                continue;
            }
            const isArrayParam = param.arraySize !== undefined;
            if (isArrayParam &&
                node.args[i].type === NodeType.Identifier &&
                this.isArrayVar(node.args[i].name)) {
                // Array parameter (char[] or int[]): push array reference, not element value
                this.emitArrayRefByName(node.args[i].name, node.line);
            } else if (isArrayParam &&
                       node.args[i].type === NodeType.ArrayAccess &&
                       node.args[i].index2 == null &&
                       (() => { const s = (this.scope && this.scope.lookup(node.args[i].name)) || this.globals.get(node.args[i].name); return s && s.cols && s.cols > 1; })()) {
                // 2D char array row reference: arr[i] passed to char[] param
                this.emitArrayRef(node.args[i]);
            } else if (isArrayParam && node.args[i].type === NodeType.StringLiteral && param.type === 'char') {
                // char[] param receiving a string literal: emit const-pool ref encoding
                // (tag=3, handle = 0x8000|constIdx). Without this, LOAD_CONST pushes a
                // plain int that the callee resolves as a garbage local ref.
                this.emitStringArg(node.args[i]);
            } else {
                this.compileExpr(node.args[i]);
                // Insert type conversion if argument type doesn't match parameter type
                if (param.arraySize === undefined || param.arraySize === null) {
                    const argType = this.inferType(node.args[i]);
                    if (param.type === 'float' && !this.isFloatType(argType)) this.emit(Op.I2F);
                    else if (param.type !== 'float' && this.isFloatType(argType)) this.emit(Op.F2I);
                }
            }
        }

        this.emit(Op.CALL);
        if (func.address < 0) {
            // Forward reference — address not yet known, patch later
            this.patches.push({ offset: this.code.length, name: node.name, line: node.line });
            this.emitU16(0xFFFF); // placeholder
        } else {
            this.emitU16(func.address);
        }
    }

    compileWatchIntrinsic(node) {
        if (node.args.length !== 1) {
            throw new CodeGenError(`${node.name}() expects 1 argument`, node.line);
        }
        const arg = node.args[0];
        if (arg.type !== NodeType.Identifier) {
            throw new CodeGenError(`${node.name}() requires a variable name`, arg.line || node.line);
        }
        const watchInfo = this.watchGlobals.get(arg.name);
        if (!watchInfo) {
            throw new CodeGenError(`${node.name}(): '${arg.name}' is not a watch variable`, arg.line || node.line);
        }
        const isFloat = this.isFloatType(watchInfo.type);

        switch (node.name) {
            case 'changed':
                // current != shadow
                this.emit(Op.LOAD_GLOBAL); this.emitU16(watchInfo.varIndex);
                this.emit(Op.LOAD_GLOBAL); this.emitU16(watchInfo.shadowIndex);
                this.emit(isFloat ? Op.FNEQ : Op.NEQ);
                break;
            case 'delta':
                // current - shadow
                this.emit(Op.LOAD_GLOBAL); this.emitU16(watchInfo.varIndex);
                this.emit(Op.LOAD_GLOBAL); this.emitU16(watchInfo.shadowIndex);
                this.emit(isFloat ? Op.FSUB : Op.SUB);
                break;
            case 'written':
                // written flag
                this.emit(Op.LOAD_GLOBAL); this.emitU16(watchInfo.writtenIndex);
                break;
            case 'snapshot':
                // shadow = current, written = 0
                this.emit(Op.LOAD_GLOBAL); this.emitU16(watchInfo.varIndex);
                this.emit(Op.STORE_GLOBAL); this.emitU16(watchInfo.shadowIndex);
                this.emitPushInt(0);
                this.emit(Op.STORE_GLOBAL); this.emitU16(watchInfo.writtenIndex);
                break;
        }
    }

    // ─── General-purpose UDP function (modes 0-7) ─────────
    // Scripter-compatible: udp(mode, ...) with variable args per mode
    compileUdpFunc(node) {
        if (node.args.length < 1) {
            throw new CodeGenError("udp() requires at least 1 argument (mode)", node.line);
        }
        const modeNode = node.args[0];
        if (modeNode.type !== NodeType.IntLiteral) {
            throw new CodeGenError("udp() first argument (mode) must be a literal integer 0-10", modeNode.line || node.line);
        }
        const mode = modeNode.value;
        const nargs = node.args.length;

        switch (mode) {
            case 0: // udp(0, port) → open listening port
                if (nargs !== 2) throw new CodeGenError("udp(0, port) expects 2 arguments", node.line);
                this.compileExpr(node.args[1]);  // port (int)
                break;
            case 1: // udp(1, buf) → read string into buf
                if (nargs !== 2) throw new CodeGenError("udp(1, buf) expects 2 arguments", node.line);
                this.emitArrayRef(node.args[1]);  // buf (char[] ref)
                break;
            case 2: // udp(2, str) → reply to sender
                if (nargs !== 2) throw new CodeGenError("udp(2, str) expects 2 arguments", node.line);
                this.emitArrayRef(node.args[1]);  // str (char[] ref)
                break;
            case 3: // udp(3, url, str) → send to url with stored port
                if (nargs !== 3) throw new CodeGenError("udp(3, url, str) expects 3 arguments", node.line);
                this.emitArrayRef(node.args[1]);  // url (char[] ref)
                this.emitArrayRef(node.args[2]);  // str (char[] ref)
                break;
            case 4: // udp(4, buf) → get remote IP into buf
                if (nargs !== 2) throw new CodeGenError("udp(4, buf) expects 2 arguments", node.line);
                this.emitArrayRef(node.args[1]);  // buf (char[] ref)
                break;
            case 5: // udp(5) → get remote port
                if (nargs !== 1) throw new CodeGenError("udp(5) expects 1 argument", node.line);
                break;
            case 6: // udp(6, url, port, str) → send string to url:port
                if (nargs !== 4) throw new CodeGenError("udp(6, url, port, str) expects 4 arguments", node.line);
                this.emitArrayRef(node.args[1]);  // url (char[] ref)
                this.compileExpr(node.args[2]);   // port (int)
                this.emitArrayRef(node.args[3]);  // str (char[] ref)
                break;
            case 7: // udp(7, url, port, arr, count) → send array bytes to url:port
                if (nargs !== 5) throw new CodeGenError("udp(7, url, port, arr, count) expects 5 arguments", node.line);
                this.emitArrayRef(node.args[1]);  // url (char[] ref)
                this.compileExpr(node.args[2]);   // port (int)
                this.emitArrayRef(node.args[3]);  // arr (int[] ref)
                this.compileExpr(node.args[4]);   // count (int)
                break;
            case 8: // udp(8, which, seconds) → set inactivity timeout (0=multicast, 1=port)
                if (nargs !== 3) throw new CodeGenError("udp(8, which, seconds) expects 3 arguments", node.line);
                this.compileExpr(node.args[1]);  // which (int: 0=multicast, 1=general port)
                this.compileExpr(node.args[2]);  // seconds (int: 0=disable)
                break;
            case 9: // udp(9, mcast_ip, port) → join multicast group on udp_port
                if (nargs !== 3) throw new CodeGenError("udp(9, mcast_ip, port) expects 3 arguments", node.line);
                this.emitStringArg(node.args[1]);  // mcast_ip (char[] or string literal)
                this.compileExpr(node.args[2]);    // port (int)
                break;
            case 10: // udp(10, mcast_ip) → IGMP-Leave on the netif (socket-independent)
                if (nargs !== 2) throw new CodeGenError("udp(10, mcast_ip) expects 2 arguments", node.line);
                this.emitStringArg(node.args[1]);  // mcast_ip (char[] or string literal)
                break;
            default:
                throw new CodeGenError(`udp() mode must be 0-10, got ${mode}`, node.line);
        }
        // Push mode as last value (VM pops it first)
        this.emitPushInt(mode);
        this.emit(Op.SYSCALL);
        this.emitByte(Syscall.UDP_FUNC);
    }

    compileTcpWriteArray(node) {
        // tcpWriteArray(array, num) or tcpWriteArray(array, num, type)
        // type is optional, default 0 (uint8)
        const nargs = node.args.length;
        if (nargs < 2 || nargs > 3) {
            throw new CodeGenError("tcpWriteArray(array, num [, type]) expects 2 or 3 arguments", node.line);
        }
        this.emitArrayRef(node.args[0]);  // array ref
        this.compileExpr(node.args[1]);   // count (int)
        if (nargs === 3) {
            this.compileExpr(node.args[2]);  // type (int)
        } else {
            this.emitPushInt(0);             // default type = 0 (uint8)
        }
        this.emit(Op.SYSCALL);
        this.emitByte(Syscall.TCP_WRITE_ARR);
    }

    compileTcpConnect(node) {
        // tcpConnect("ip", port)            → SYS_TCP_CONNECT     (string literal)
        // tcpConnect(ip_char_array, port)   → SYS_TCP_CONNECT_REF (runtime char array)
        if (node.args.length !== 2) {
            throw new CodeGenError("tcpConnect(ip, port) expects 2 arguments", node.line);
        }
        const ipArg = node.args[0];
        if (ipArg.type === NodeType.StringLiteral) {
            // String literal — push const index then port
            const idx = this.addConstant(ipArg.value);
            this.emit(Op.LOAD_CONST);
            this.emitU16(idx);
            this.compileExpr(node.args[1]);
            this.emitSyscall(Syscall.TCP_CONNECT);
        } else if (ipArg.type === NodeType.Identifier && this.isCharArrayVar(ipArg.name)) {
            // Char array — emit array ref then port
            this.emitArrayRefByName(ipArg.name, node.line);
            this.compileExpr(node.args[1]);
            this.emitSyscall(Syscall.TCP_CONNECT_REF);
        } else {
            throw new CodeGenError(
                "tcpConnect: first argument must be a string literal or a char[] variable",
                node.line);
        }
    }

    compileSpawnApi(node) {
        // spawnTask("Name")                — 1 arg, default stack
        // spawnTask("Name", stack_kb)      — 2 args, explicit stack [3..16]
        // killTask("Name")                 — 1 arg
        // taskRunning("Name")              — 1 arg
        const fn = node.name;
        const nargs = node.args.length;
        if (fn === 'spawnTask' && (nargs < 1 || nargs > 2)) {
            throw new CodeGenError("spawnTask(name [, stack_kb]) expects 1 or 2 arguments", node.line);
        }
        if ((fn === 'killTask' || fn === 'taskRunning') && nargs !== 1) {
            throw new CodeGenError(`${fn}(name) expects 1 argument`, node.line);
        }
        const nameArg = node.args[0];
        if (nameArg.type !== NodeType.StringLiteral) {
            throw new CodeGenError(
                `${fn}: first argument must be a string literal naming a user-defined function`,
                node.line);
        }
        const taskName = nameArg.value;
        // Track for the function table — the firmware looks up vm->callbacks[] by name.
        this.spawnTargets.add(taskName);

        const cidx = this.addConstant(taskName);
        this.emit(Op.LOAD_CONST);
        this.emitU16(cidx);

        if (fn === 'spawnTask') {
            if (nargs === 2) {
                this.compileExpr(node.args[1]);  // stack_kb (int)
                this.emitSyscall(Syscall.SPAWN_TASK_STACK);
            } else {
                this.emitSyscall(Syscall.SPAWN_TASK);
            }
        } else if (fn === 'killTask') {
            this.emitSyscall(Syscall.KILL_TASK);
        } else { // taskRunning
            this.emitSyscall(Syscall.TASK_RUNNING);
        }
    }

    compileFileWriteArray(node) {
        // fileWriteArray(array, handle) or fileWriteArray(array, handle, append)
        // append is optional, default 0 (add newline)
        const nargs = node.args.length;
        if (nargs < 2 || nargs > 3) {
            throw new CodeGenError("fileWriteArray(array, handle [, append]) expects 2 or 3 arguments", node.line);
        }
        this.emitArrayRef(node.args[0]);  // array ref
        this.compileExpr(node.args[1]);   // handle (int)
        if (nargs === 3) {
            this.compileExpr(node.args[2]);  // append flag (int)
        } else {
            this.emitPushInt(0);             // default: 0 = add newline
        }
        this.emit(Op.SYSCALL);
        this.emitByte(Syscall.FILE_WRITE_ARR);
    }

    compileTimeOffset(node) {
        // timeOffset(buf, days) or timeOffset(buf, days, zeroFlag)
        // zeroFlag is optional, default 0 (keep time)
        const nargs = node.args.length;
        if (nargs < 2 || nargs > 3) {
            throw new CodeGenError("timeOffset(buf, days [, zeroFlag]) expects 2 or 3 arguments", node.line);
        }
        this.emitArrayRef(node.args[0]);  // buf ref (char[])
        this.compileExpr(node.args[1]);   // days (int)
        if (nargs === 3) {
            this.compileExpr(node.args[2]);  // zeroFlag (int)
        } else {
            this.emitPushInt(0);             // default: 0 = keep time
        }
        this.emit(Op.SYSCALL);
        this.emitByte(Syscall.TIME_OFFSET);
    }

    // Parse a format string into segments — each segment ends at a % conversion specifier.
    // Returns array of { segment: string, type: 'int'|'float'|'str'|null }
    // type===null means trailing text with no specifier.
    static parseFormatSegments(fmt) {
        const segments = [];
        let i = 0;
        let start = 0;
        while (i < fmt.length) {
            if (fmt[i] !== '%') { i++; continue; }
            if (fmt[i + 1] === '%') { i += 2; continue; }  // %% — escaped, not a specifier
            // Scan past flags, width, precision, length modifier
            let j = i + 1;
            while (j < fmt.length && '-+ #0'.includes(fmt[j])) j++;
            while (j < fmt.length && fmt[j] >= '0' && fmt[j] <= '9') j++;
            if (j < fmt.length && fmt[j] === '.') {
                j++;
                while (j < fmt.length && fmt[j] >= '0' && fmt[j] <= '9') j++;
            }
            while (j < fmt.length && 'hlLqjzt'.includes(fmt[j])) j++;
            if (j >= fmt.length) break;
            const spec = fmt[j++];
            let type;
            if ('diouxXc'.includes(spec))  type = 'int';
            else if ('feEgG'.includes(spec)) type = 'float';
            else if (spec === 's')           type = 'str';
            else                             type = 'int';  // fallback
            segments.push({ segment: fmt.slice(start, j), type });
            start = j;
            i = j;
        }
        if (start < fmt.length) {
            segments.push({ segment: fmt.slice(start), type: null });  // trailing text
        }
        return segments;
    }

    // Compile-time expansion of a printf-style format string into a chain of
    // single-value sprintf / sprintfAppend syscalls writing into `dst`.
    //
    //   emitDst()  — callback that pushes a destination char[] ref (e.g. the
    //                user's buffer for sprintf, or the shared log scratch for
    //                addLog).
    //   fmtStr     — the format literal (already extracted from the call).
    //   valArgs    — array of value AST nodes (one per format specifier).
    //   isAppend   — true → use the *_CAT (append) variants from the very
    //                first segment so existing buffer contents are preserved.
    //   name, line — for error messages.
    //
    // Leaves exactly one int32 on the stack (return of the last sprintf*
    // syscall — the resulting string length). Caller decides whether to
    // expose it (sprintf → pendingPush) or discard it (addLog → POP).
    emitSprintfChain(emitDst, fmtStr, valArgs, isAppend, name, line) {
        const resolveArg = (arg) => {
            if (arg.type === NodeType.Identifier && this.defines.has(arg.name)) {
                return this.defines.get(arg.name);
            }
            return arg;
        };

        const segments = CodeGenerator.parseFormatSegments(fmtStr);
        const valueSegments = segments.filter(s => s.type !== null);
        if (valueSegments.length !== valArgs.length) {
            throw new CodeGenError(
                `${name}: format string has ${valueSegments.length} specifier(s) but ${valArgs.length} value(s) provided`,
                line
            );
        }

        let valIdx = 0;
        let firstEmitted = false;
        for (const seg of segments) {
            if (seg.type === null) {
                // Trailing text — append as a string constant via "%s" fmt.
                // Note: snprintf only honors %% inside the *format* string, not
                // inside %s arguments. So we must unescape %% → % here, otherwise
                // the trailing literal would print "%%" to the user. Bug B fix.
                emitDst();
                const fmtIdx = this.addConstant('%s');
                this.emit(Op.LOAD_CONST); this.emitU16(fmtIdx);
                const txtIdx = this.addConstant(seg.segment.replace(/%%/g, '%'));
                this.emit(Op.LOAD_CONST); this.emitU16(txtIdx);
                this.emit(Op.SYSCALL);
                this.emitByte(Syscall.SPRINTF_STR_CAT_CONST);
                this.emit(Op.POP);  // discard intermediate return
                continue;
            }

            const useAppend = isAppend || firstEmitted;
            const valArg = valArgs[valIdx++];
            const resolvedVal = resolveArg(valArg);
            const isCharArr   = (resolvedVal.type === NodeType.Identifier && this.isCharArrayVar(resolvedVal.name)) ||
                                this.is2DCharArrayAccess(resolvedVal);
            const isStringLit = resolvedVal.type === NodeType.StringLiteral;
            const segFmtIdx   = this.addConstant(seg.segment);

            if (isCharArr || isStringLit || seg.type === 'str') {
                if (isStringLit) {
                    emitDst();
                    this.emit(Op.LOAD_CONST); this.emitU16(segFmtIdx);
                    const srcIdx = this.addConstant(resolvedVal.value);
                    this.emit(Op.LOAD_CONST); this.emitU16(srcIdx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(useAppend ? Syscall.SPRINTF_STR_CAT_CONST : Syscall.SPRINTF_STR_CONST);
                } else {
                    emitDst();
                    this.emit(Op.LOAD_CONST); this.emitU16(segFmtIdx);
                    this.emitArrayRef(valArg);
                    this.emit(Op.SYSCALL);
                    this.emitByte(useAppend ? Syscall.SPRINTF_STR_CAT : Syscall.SPRINTF_STR);
                }
            } else if (seg.type === 'float') {
                emitDst();
                this.emit(Op.LOAD_CONST); this.emitU16(segFmtIdx);
                this.compileExpr(valArg);
                if (this.inferType(resolvedVal) !== 'float') this.emit(Op.I2F);
                this.emit(Op.SYSCALL);
                this.emitByte(useAppend ? Syscall.SPRINTF_FLT_CAT : Syscall.SPRINTF_FLT);
            } else {
                emitDst();
                this.emit(Op.LOAD_CONST); this.emitU16(segFmtIdx);
                this.compileExpr(valArg);
                if (this.inferType(resolvedVal) === 'float') this.emit(Op.F2I);
                this.emit(Op.SYSCALL);
                this.emitByte(useAppend ? Syscall.SPRINTF_INT_CAT : Syscall.SPRINTF_INT);
            }

            if (firstEmitted) this.emit(Op.POP);  // discard return of non-last calls
            firstEmitted = true;
        }
    }

    compileSprintfUnified(node) {
        // sprintf(dst, fmt, val, ...) / sprintfAppend(dst, fmt, val, ...)
        // Single value (3 args): emit one syscall variant.
        // Multiple values (4+ args): compile-time expansion into sprintf + sprintfAppend chain.
        const isAppend = (node.name === 'sprintfAppend');
        const nargs = node.args.length;
        if (nargs < 3) {
            throw new CodeGenError(`${node.name} expects at least 3 arguments (dst, fmt, val [, ...])`, node.line);
        }

        // Helper: resolve defines
        const resolveArg = (arg) => {
            if (arg.type === NodeType.Identifier && this.defines.has(arg.name)) {
                return this.defines.get(arg.name);
            }
            return arg;
        };

        // fmt must be a string literal
        const resolvedFmt = resolveArg(node.args[1]);
        if (resolvedFmt.type !== NodeType.StringLiteral) {
            throw new CodeGenError(`${node.name} format (arg 2) must be a string literal`, node.line);
        }
        const fmtStr = resolvedFmt.value;

        // Variadic path: 4+ args — split format string and chain syscalls
        if (nargs > 3) {
            this.emitSprintfChain(
                () => this.emitArrayRef(node.args[0]),
                fmtStr,
                node.args.slice(2),
                isAppend,
                node.name,
                node.line
            );
            this.pendingPush = true;  // last sprintf return left on stack
            return;
        }

        // Single value path (3 args)
        const valArg = node.args[2];
        const resolvedVal = resolveArg(valArg);
        const valType = this.inferType(resolvedVal);
        const isCharArr = (resolvedVal.type === NodeType.Identifier && this.isCharArrayVar(resolvedVal.name)) ||
                          this.is2DCharArrayAccess(resolvedVal);
        const isStringLit = (resolvedVal.type === NodeType.StringLiteral);

        // Determine format specifier type to drive dispatch (fixes float var with %d and vice-versa)
        const fmtSegments = CodeGenerator.parseFormatSegments(fmtStr);
        const fmtSpecType = fmtSegments.length > 0 && fmtSegments[0].type !== null ? fmtSegments[0].type : null;

        if (isCharArr || isStringLit || fmtSpecType === 'str') {
            if (isStringLit) {
                this.emitArrayRef(node.args[0]);
                const fmtIdx = this.addConstant(fmtStr);
                this.emit(Op.LOAD_CONST); this.emitU16(fmtIdx);
                const srcIdx = this.addConstant(resolvedVal.value);
                this.emit(Op.LOAD_CONST); this.emitU16(srcIdx);
                this.emit(Op.SYSCALL);
                this.emitByte(isAppend ? Syscall.SPRINTF_STR_CAT_CONST : Syscall.SPRINTF_STR_CONST);
            } else {
                this.emitArrayRef(node.args[0]);
                const fmtIdx = this.addConstant(fmtStr);
                this.emit(Op.LOAD_CONST); this.emitU16(fmtIdx);
                this.emitArrayRef(node.args[2]);
                this.emit(Op.SYSCALL);
                this.emitByte(isAppend ? Syscall.SPRINTF_STR_CAT : Syscall.SPRINTF_STR);
            }
        } else if (fmtSpecType === 'float' || (fmtSpecType === null && valType === 'float')) {
            // Float path: format says float, or no specifier and value is float
            this.emitArrayRef(node.args[0]);
            const fmtIdx = this.addConstant(fmtStr);
            this.emit(Op.LOAD_CONST); this.emitU16(fmtIdx);
            this.compileExpr(node.args[2]);
            if (valType !== 'float') this.emit(Op.I2F);  // coerce int→float
            this.emit(Op.SYSCALL);
            this.emitByte(isAppend ? Syscall.SPRINTF_FLT_CAT : Syscall.SPRINTF_FLT);
        } else {
            // Int path: format says int (or default). Coerce float→int if needed.
            this.emitArrayRef(node.args[0]);
            const fmtIdx = this.addConstant(fmtStr);
            this.emit(Op.LOAD_CONST); this.emitU16(fmtIdx);
            this.compileExpr(node.args[2]);
            if (valType === 'float') this.emit(Op.F2I);  // coerce float→int (fixes %d with float var)
            this.emit(Op.SYSCALL);
            this.emitByte(isAppend ? Syscall.SPRINTF_INT_CAT : Syscall.SPRINTF_INT);
        }
        this.pendingPush = true;
    }

    compileFileExtract(node, syscall) {
        // fileExtract(handle, ts_from, ts_to, col_offs, accum, arr1, arr2, ..., arrN)
        // fileExtractFast(handle, ts_from, ts_to, col_offs, accum, arr1, arr2, ..., arrN)
        // Variable number of array args (6+ total args, first 5 fixed)
        const nargs = node.args.length;
        if (nargs < 6) {
            throw new CodeGenError("fileExtract(handle, from, to, col_offs, accum, arr1 [, arr2, ...]) expects at least 6 arguments", node.line);
        }
        this.compileExpr(node.args[0]);       // handle (int)
        this.emitArrayRef(node.args[1]);      // ts_from (char[])
        this.emitArrayRef(node.args[2]);      // ts_to (char[])
        this.compileExpr(node.args[3]);       // col_offs (int)
        this.compileExpr(node.args[4]);       // accum (int)
        const numArrays = nargs - 5;
        for (let i = 5; i < nargs; i++) {
            this.emitArrayRef(node.args[i]);  // destination arrays
        }
        this.emitPushInt(numArrays);          // array count
        this.emit(Op.SYSCALL);
        this.emitByte(syscall);
    }

    compileArrayAccess(node) {
        // Phase 1 2D: prefer scope/global lookup so we can compute flat
        // index from i * cols + j when both subscripts are present.
        const sym = (this.scope && this.scope.lookup(node.name)) ||
                    this.globals.get(node.name) || null;
        const is2D = sym && sym.cols && sym.cols > 1;
        if (is2D) {
            if (node.index2 == null) {
                throw new CodeGenError(
                    `'${node.name}' is a 2D char array — use ${node.name}[i][j] for an element, ` +
                    `or pass ${node.name}[i] / ${node.name} to a char[] parameter for a row/whole reference`,
                    node.line);
            }
            // Stack: i, cols, MUL, j, ADD  →  flat index
            this.compileExpr(node.index);
            this.emitPushInt(sym.cols);
            this.emit(Op.MUL);
            this.compileExpr(node.index2);
            this.emit(Op.ADD);
        } else {
            if (node.index2 != null) {
                throw new CodeGenError(
                    `'${node.name}' is a 1D array — second subscript not allowed`, node.line);
            }
            this.compileExpr(node.index);
        }

        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local && local.isArray) {
                if (local.isRef) {
                    // Ref parameter — local slot holds a packed runtime ref.
                    // Resolve at runtime and index into the underlying array.
                    this.emit(Op.LOAD_REF_ARR);
                    this.emitByte(local.index);
                    return;
                }
                if (local.isHeap) {
                    this.emit(Op.LOAD_HEAP_ARR);
                    this.emitByte(local.heapHandle);
                    return;
                }
                this.emit(Op.LOAD_LOCAL_ARR);
                this.emitByte(local.index);
                return;
            }
        }

        const global = this.globals.get(node.name);
        if (global && global.isArray) {
            if (global.isHeap) {
                this.emit(Op.LOAD_HEAP_ARR);
                this.emitByte(global.heapHandle);
                return;
            }
            this.emit(Op.LOAD_GLOBAL_ARR);
            this.emitU16(global.index);
            return;
        }

        throw new CodeGenError(`Undefined array: ${node.name}`, node.line);
    }

    compileAssignment(node) {
        // ── Scalar reference assignment: a = expr;  (a is int& param)
        if (this.scope) {
            const sr = this.scope.lookup(node.name);
            if (sr && sr.isScalarRef) {
                if (node.op === '=') {
                    this.emit(Op.PUSH_I8); this.emitByte(0);     // idx 0
                    this.compileExpr(node.value);                // value
                    const valType = this.inferType(node.value);
                    if (sr.type === 'float' && valType !== 'float') this.emit(Op.I2F);
                    else if (sr.type !== 'float' && valType === 'float') this.emit(Op.F2I);
                    this.emit(Op.STORE_REF_ARR); this.emitByte(sr.index);
                } else {
                    // Compound assignment +=, -=, *=, /= etc.
                    const isFloat = sr.type === 'float';
                    // Stack: [idx, idx, oldVal, rhs, op, newVal]
                    this.emit(Op.PUSH_I8); this.emitByte(0);
                    this.emit(Op.DUP);
                    this.emit(Op.LOAD_REF_ARR); this.emitByte(sr.index);
                    this.compileExpr(node.value);
                    const valType = this.inferType(node.value);
                    if (isFloat && valType !== 'float') this.emit(Op.I2F);
                    else if (!isFloat && valType === 'float') this.emit(Op.F2I);
                    switch (node.op) {
                        case '+=':  this.emit(isFloat ? Op.FADD : Op.ADD);  break;
                        case '-=':  this.emit(isFloat ? Op.FSUB : Op.SUB);  break;
                        case '*=':  this.emit(isFloat ? Op.FMUL : Op.MUL);  break;
                        case '/=':  this.emit(isFloat ? Op.FDIV : Op.DIV);  break;
                        case '%=':  this.emit(Op.MOD);     break;
                        case '&=':  this.emit(Op.BIT_AND); break;
                        case '|=':  this.emit(Op.BIT_OR);  break;
                        case '^=':  this.emit(Op.BIT_XOR); break;
                        case '<<=': this.emit(Op.SHL);     break;
                        case '>>=': this.emit(Op.SHR);     break;
                        default: throw new CodeGenError(`Unknown compound op on ref param: ${node.op}`, node.line);
                    }
                    this.emit(Op.STORE_REF_ARR); this.emitByte(sr.index);
                }
                return;
            }
        }
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
        // ── String operations on char[] ──────────────────────
        // buf = "hello"       → strcpyConst(buf, "hello")
        // buf = other         → strcpy(buf, other)   where other is char[]
        // buf += "world"      → strcatConst(buf, "world")
        // buf += other        → strcat(buf, other)   where other is char[]
        // buf = buf + "world" → strcatConst(buf, "world")
        // buf = buf + other   → strcat(buf, other)
        if (this.isCharArrayVar(node.name)) {
            if (node.op === '=') {
                // Check for: buf = buf + expr  →  strcat
                if (node.value.type === NodeType.BinaryExpr && node.value.op === '+' &&
                    node.value.left.type === NodeType.Identifier && node.value.left.name === node.name) {
                    // buf = buf + "literal"  → strcatConst
                    if (node.value.right.type === NodeType.StringLiteral) {
                        this.emitArrayRefByName(node.name, node.line);
                        const idx = this.addConstant(node.value.right.value);
                        this.emit(Op.LOAD_CONST);
                        this.emitU16(idx);
                        this.emit(Op.SYSCALL);
                        this.emitByte(Syscall.STRCAT_CONST);
                        return;
                    }
                    // buf = buf + otherArray  → strcat
                    if (node.value.right.type === NodeType.Identifier && this.isCharArrayVar(node.value.right.name)) {
                        this.emitArrayRefByName(node.name, node.line);
                        this.emitArrayRefByName(node.value.right.name, node.line);
                        this.emit(Op.SYSCALL);
                        this.emitByte(Syscall.STRCAT);
                        return;
                    }
                }
                // buf = "literal"  → strcpyConst
                if (node.value.type === NodeType.StringLiteral) {
                    this.emitArrayRefByName(node.name, node.line);
                    const idx = this.addConstant(node.value.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(Syscall.STRCPY_CONST);
                    return;
                }
                // buf = otherArray  → strcpy
                if (node.value.type === NodeType.Identifier && this.isCharArrayVar(node.value.name)) {
                    this.emitArrayRefByName(node.name, node.line);
                    this.emitArrayRefByName(node.value.name, node.line);
                    this.emit(Op.SYSCALL);
                    this.emitByte(Syscall.STRCPY);
                    return;
                }
            }
            if (node.op === '+=') {
                // buf += "literal"  → strcatConst
                if (node.value.type === NodeType.StringLiteral) {
                    this.emitArrayRefByName(node.name, node.line);
                    const idx = this.addConstant(node.value.value);
                    this.emit(Op.LOAD_CONST);
                    this.emitU16(idx);
                    this.emit(Op.SYSCALL);
                    this.emitByte(Syscall.STRCAT_CONST);
                    return;
                }
                // buf += otherArray  → strcat
                if (node.value.type === NodeType.Identifier && this.isCharArrayVar(node.value.name)) {
                    this.emitArrayRefByName(node.name, node.line);
                    this.emitArrayRefByName(node.value.name, node.line);
                    this.emit(Op.SYSCALL);
                    this.emitByte(Syscall.STRCAT);
                    return;
                }
            }
            // Fall through: char arrays used with numeric ops (e.g., buf[i] handled by compileArrayAssign)
        }

        // ── Normal numeric assignment ────────────────────────
        const targetType = this.getVarType(node.name);
        if (node.op === '=') {
            this.compileExpr(node.value);
            // Type coercion: int→float or float→int
            const valType = this.inferType(node.value);
            if (targetType === 'float' && valType !== 'float') this.emit(Op.I2F);
            else if (targetType !== 'float' && valType === 'float') {
                this.warnNarrowing(node.name, node.line, valType, targetType);
                this.emit(Op.F2I);
            }
        } else {
            // Compound assignment: +=, -=, etc.
            const isFloat = targetType === 'float';
            this.compileExpr({ type: NodeType.Identifier, name: node.name, line: node.line });
            this.compileExpr(node.value);
            const valType = this.inferType(node.value);
            if (isFloat && valType !== 'float') this.emit(Op.I2F);
            else if (!isFloat && valType === 'float') {
                this.warnNarrowing(node.name, node.line, valType, targetType);
                this.emit(Op.F2I);
            }
            switch (node.op) {
                case '+=':  this.emit(isFloat ? Op.FADD : Op.ADD);  break;
                case '-=':  this.emit(isFloat ? Op.FSUB : Op.SUB);  break;
                case '*=':  this.emit(isFloat ? Op.FMUL : Op.MUL);  break;
                case '/=':  this.emit(isFloat ? Op.FDIV : Op.DIV);  break;
                case '%=':  this.emit(Op.MOD);     break;
                case '&=':  this.emit(Op.BIT_AND); break;
                case '|=':  this.emit(Op.BIT_OR);  break;
                case '^=':  this.emit(Op.BIT_XOR); break;
                case '<<=': this.emit(Op.SHL); break;
                case '>>=': this.emit(Op.SHR); break;
                default:
                    throw new CodeGenError(`Unknown compound assignment: ${node.op}`, node.line);
            }
        }

        // Store to Tasmota system variable (tasm_xxx)
        const tasmVarW = CodeGenerator.TASM_VARS[node.name];
        if (tasmVarW) {
            if (!tasmVarW.writable) {
                throw new CodeGenError(`Tasmota variable '${node.name}' is read-only`, node.line);
            }
            // Stack has [value]. Need [index, value] for TASM_SET.
            // Emit: PUSH index, SWAP, SYSCALL TASM_SET
            // But we don't have SWAP. Easier: store value to temp approach.
            // Actually, just re-order: emit index first, then re-compile value — but value is already compiled.
            // Simplest: use the approach of emitting index BEFORE compiling value.
            // Let's restructure — we'll handle tasm writes before the generic compile.
            // Since value is already on stack, we push index and use reversed arg order in VM.
            this.emit(Op.PUSH_I8);
            this.emitByte(tasmVarW.index);
            this.emit(Op.SYSCALL);
            this.emitByte(Syscall.TASM_SET);
            return;
        }

        // Store
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local) {
                if (local.isStaticAlias) {
                    const g = this.globals.get(local.globalName);
                    this.emitStoreGlobal(g.index, local.globalName);
                    return;
                }
                this.emit(Op.STORE_LOCAL);
                this.emitByte(local.index);
                return;
            }
        }
        const global = this.globals.get(node.name);
        if (global) {
            const watchInfo = this.watchGlobals.get(node.name);
            if (watchInfo) {
                this.emit(Op.STORE_WATCH);
                this.emitU16(watchInfo.varIndex);
                this.emitU16(watchInfo.shadowIndex);
                this.emitU16(watchInfo.writtenIndex);
            } else {
                this.emitStoreGlobal(global.index, node.name);
            }
            return;
        }
        throw new CodeGenError(`Undefined variable: ${node.name}`, node.line);
    }

    compileArrayAssign(node) {
        // ── Whole-struct copy into array element: arr[i] = b; or arr[i] = arr[j];
        if (node.op === '=' && this._isStructVar(node.name)
                            && this._isStructValueExpr(node.value)) {
            this._compileStructCopy(
                { kind: 'arr', name: node.name, index: node.index },
                node.value, node.line);
            return;
        }

        // Phase 2 2D: arr[i][j] = expr → flatten i*cols + j onto the stack.
        const _sym = (this.scope && this.scope.lookup(node.name)) ||
                     this.globals.get(node.name) || null;
        const _is2D = _sym && _sym.cols && _sym.cols > 1;
        if (_is2D) {
            if (node.index2 == null) {
                throw new CodeGenError(
                    `'${node.name}' is a 2D array — assignment requires both indices, e.g. ${node.name}[i][j] = …`,
                    node.line);
            }
            this.compileExpr(node.index);
            this.emitPushInt(_sym.cols);
            this.emit(Op.MUL);
            this.compileExpr(node.index2);
            this.emit(Op.ADD);
        } else {
            if (node.index2 != null) {
                throw new CodeGenError(
                    `'${node.name}' is a 1D array — second subscript not allowed in assignment`, node.line);
            }
            this.compileExpr(node.index);
        }
        const arrType = this.getVarType(node.name);

        if (node.op !== '=') {
            // For compound: load current, compute, then store
            const isFloat = arrType === 'float';
            this.emit(Op.DUP); // keep index
            // Load current value
            const arrInfo = this.lookupArray(node.name);
            if (arrInfo && arrInfo.isRef) {
                this.emit(Op.LOAD_REF_ARR);
                this.emitByte(arrInfo.index);
            } else if (arrInfo && arrInfo.isHeap) {
                this.emit(Op.LOAD_HEAP_ARR);
                this.emitByte(arrInfo.heapHandle);
            } else if (arrInfo && this.scope && this.scope.lookup(node.name)) {
                this.emit(Op.LOAD_LOCAL_ARR);
                this.emitByte(arrInfo.index);
            } else if (arrInfo) {
                this.emit(Op.LOAD_GLOBAL_ARR);
                this.emitU16(arrInfo.index);
            }
            this.compileExpr(node.value);
            const valType = this.inferType(node.value);
            if (isFloat && valType !== 'float') this.emit(Op.I2F);
            else if (!isFloat && valType === 'float') {
                this.warnNarrowing(node.name + '[]', node.line, valType, arrType);
                this.emit(Op.F2I);
            }
            switch (node.op) {
                case '+=': this.emit(isFloat ? Op.FADD : Op.ADD); break;
                case '-=': this.emit(isFloat ? Op.FSUB : Op.SUB); break;
                case '*=': this.emit(isFloat ? Op.FMUL : Op.MUL); break;
                case '/=': this.emit(isFloat ? Op.FDIV : Op.DIV); break;
            }
        } else {
            this.compileExpr(node.value);
            // Type coercion: int→float or float→int
            const valType = this.inferType(node.value);
            if (arrType === 'float' && valType !== 'float') this.emit(Op.I2F);
            else if (arrType !== 'float' && valType === 'float') {
                this.warnNarrowing(node.name + '[]', node.line, valType, arrType);
                this.emit(Op.F2I);
            }
        }

        const arrInfo = this.lookupArray(node.name);
        if (arrInfo && arrInfo.isRef) {
            // Ref parameter — store through the runtime-resolved ref
            this.emit(Op.STORE_REF_ARR);
            this.emitByte(arrInfo.index);
            return;
        }
        if (arrInfo && arrInfo.isHeap) {
            this.emit(Op.STORE_HEAP_ARR);
            this.emitByte(arrInfo.heapHandle);
            return;
        }
        if (this.scope) {
            const local = this.scope.lookup(node.name);
            if (local && local.isArray) {
                this.emit(Op.STORE_LOCAL_ARR);
                this.emitByte(local.index);
                return;
            }
        }
        const global = this.globals.get(node.name);
        if (global && global.isArray) {
            this.emit(Op.STORE_GLOBAL_ARR);
            this.emitU16(global.index);
            return;
        }
        throw new CodeGenError(`Undefined array: ${node.name}`, node.line);
    }

    // Helper: look up array info from local scope, globals, or heap
    lookupArray(name) {
        if (this.scope) {
            const local = this.scope.lookup(name);
            if (local && local.isArray) return local;
        }
        const global = this.globals.get(name);
        if (global && global.isArray) return global;
        return null;
    }

    compileCast(node) {
        this.compileExpr(node.operand);
        const fromType = this.inferType(node.operand);

        if (node.castType === 'float' && !this.isFloatType(fromType)) {
            this.emit(Op.I2F);
        } else if (node.castType === 'int' && this.isFloatType(fromType)) {
            this.emit(Op.F2I);
        } else if (node.castType === 'char') {
            this.emit(Op.I2C);
        }
        // Same type or bool: no-op
    }

    // ─── Type inference (simple) ────────────────────────────

    inferType(node) {
        switch (node.type) {
            case NodeType.IntLiteral:
            case NodeType.CharLiteral:
            case NodeType.BoolLiteral:
                return 'int';
            case NodeType.FloatLiteral:
                return 'float';
            case NodeType.StringLiteral:
                return 'string';
            case NodeType.Identifier:
                return this.getVarType(node.name);
            case NodeType.CastExpr:
                return node.castType;
            case NodeType.BinaryExpr: {
                if (node.op === '+' && this.isStringNode && this.isStringNode(node.left) && this.isStringNode(node.right))
                    return 'char'; // string concat result is a char[] ref
                const lt = this.inferType(node.left);
                const rt = this.inferType(node.right);
                if (lt === 'float' || rt === 'float') return 'float';
                return 'int';
            }
            case NodeType.CallExpr: {
                if (node.name === 'sizeof') return 'int';
                // Watch intrinsics
                if (node.name === 'delta' && node.args.length === 1 && node.args[0].type === NodeType.Identifier) {
                    const watchInfo = this.watchGlobals.get(node.args[0].name);
                    if (watchInfo) return watchInfo.type;
                }
                if (node.name === 'changed' || node.name === 'written') return 'int';
                if (node.name === 'udp') return 'int';
                const func = this.functions.get(node.name);
                if (func) return func.returnType;
                const builtin = BUILTINS[node.name];
                if (builtin) {
                    // Symbol-table-driven: any builtin marked returnFloat:true is a float-returner.
                    if (builtin.returnFloat === true) return 'float';
                    // Legacy hardcoded list (kept for builtins whose entries don't yet have the flag).
                    if (node.name === 'sqrt' || node.name === 'sin' || node.name === 'cos' ||
                        node.name === 'exp' || node.name === 'log' ||
                        node.name === 'pow' || node.name === 'acos' ||
                        node.name === 'smlGet' || node.name === 'sensorGet' ||
                        node.name === 'atof' || node.name === 'pwlGet' ||
                        node.name === 'intBitsToFloat')
                        return 'float';
                }
                return 'int';
            }
            case NodeType.TernaryExpr: {
                // `cond ? a : b` — float if EITHER branch is float (same
                // rule as BinaryExpr). Was missing → fell to default
                // 'int', so `global float G = cond ? f1 : f2;` inferred
                // int and broadcast/stored with the wrong type tag
                // (Andreas: SoC arrived as float-bits-as-int). An
                // explicit `(float)` worked only because CastExpr forces
                // 'float'.
                const ct = this.inferType(node.consequent);
                const at = this.inferType(node.alternate);
                if (ct === 'float' || at === 'float') return 'float';
                if (ct === 'string' || at === 'string') return 'string';
                return 'int';
            }
            case NodeType.UnaryExpr:
                return this.inferType(node.operand);
            case NodeType.ArrayAccess:
                return this.getVarType(node.name);
            case NodeType.PostfixExpr:
                return this.inferType(node.operand);
            case NodeType.MemberAccess:
            case NodeType.MemberArrayAccess: {
                try {
                    const { fieldType } = this.resolveMember(node.object, node.field, 0);
                    return fieldType;
                } catch (e) { return 'int'; }
            }
            default:
                return 'int';
        }
    }

    // ─── Array size inference ─────────────────────────────────

    inferArraySize(node) {
        if (node.size !== null) {
            return this.evaluateConstExpr(node.size);
        }
        // Infer size from initializer
        if (node.stringInit) {
            return node.stringInit.length + 1;  // +1 for null terminator
        }
        if (node.init) {
            return node.init.length;
        }
        throw new CodeGenError("Array size must be specified when no initializer is given", node.line);
    }

    // ─── Constant expression evaluation ─────────────────────

    evaluateConstExpr(node) {
        if (node.type === NodeType.IntLiteral) return node.value;
        if (node.type === NodeType.Identifier) {
            const def = this.defines.get(node.name);
            if (def) return this.evaluateConstExpr(def);
            throw new CodeGenError(`Non-constant: ${node.name}`, node.line);
        }
        if (node.type === NodeType.BinaryExpr) {
            const l = this.evaluateConstExpr(node.left);
            const r = this.evaluateConstExpr(node.right);
            switch (node.op) {
                case '+': return l + r;
                case '-': return l - r;
                case '*': return l * r;
                case '/': return Math.floor(l / r);
                default: throw new CodeGenError(`Non-constant op: ${node.op}`, node.line);
            }
        }
        throw new CodeGenError('Non-constant expression', node.line);
    }

    // ─── Binary output ──────────────────────────────────────

    buildBinary() {
        // Header V5: MAGIC(4) + VERSION(2) + globalSize(2) + entryPoint(2) + constPoolSize(2) + heapDeclSize(2) + funcTableSize(2) + persistTableSize(2) + globalsTableSize(2) = 20 bytes
        const header = [];
        const view = new DataView(new ArrayBuffer(4));

        // Magic
        view.setUint32(0, MAGIC, false);
        for (let i = 0; i < 4; i++) header.push(view.getUint8(i));

        // Version
        header.push((VERSION >> 8) & 0xFF);
        header.push(VERSION & 0xFF);

        // Global data size
        header.push((this.globalIndex >> 8) & 0xFF);
        header.push(this.globalIndex & 0xFF);

        // Entry point (always 0, we start with CALL main)
        header.push(0);
        header.push(0);

        // Encode constant pool
        const constPool = this.encodeConstantPool();
        const constPoolSize = constPool.length;
        header.push((constPoolSize >> 8) & 0xFF);
        header.push(constPoolSize & 0xFF);

        // Encode heap declarations
        const heapDecl = this.encodeHeapDeclarations();
        const heapDeclSize = heapDecl.length;
        header.push((heapDeclSize >> 8) & 0xFF);
        header.push(heapDeclSize & 0xFF);

        // Encode function table (V3+: callback names → addresses)
        const funcTable = this.encodeFunctionTable();
        const funcTableSize = funcTable.length;
        header.push((funcTableSize >> 8) & 0xFF);
        header.push(funcTableSize & 0xFF);

        // Encode persist table (V4: persist globals for auto-save/load)
        const persistTable = this.encodePersistTable();
        const persistTableSize = persistTable.length;
        header.push((persistTableSize >> 8) & 0xFF);
        header.push(persistTableSize & 0xFF);

        // Encode globals table (V5: UDP auto-update variables)
        const globalsTable = this.encodeGlobalsTable();
        const globalsTableSize = globalsTable.length;
        header.push((globalsTableSize >> 8) & 0xFF);
        header.push(globalsTableSize & 0xFF);

        // Combine: header + constant pool + heap declarations + function table + persist table + globals table + code
        const totalSize = header.length + constPool.length + heapDecl.length + funcTable.length + persistTable.length + globalsTable.length + this.code.length;
        const binary = new Uint8Array(totalSize);
        let offset = 0;
        binary.set(header, offset); offset += header.length;
        binary.set(constPool, offset); offset += constPool.length;
        binary.set(heapDecl, offset); offset += heapDecl.length;
        binary.set(funcTable, offset); offset += funcTable.length;
        binary.set(persistTable, offset); offset += persistTable.length;
        binary.set(globalsTable, offset); offset += globalsTable.length;
        binary.set(this.code, offset);

        return {
            binary,
            sourceMap: this.sourceMap,
            globals: Object.fromEntries(this.globals),
            functions: Object.fromEntries(
                [...this.functions].map(([k, v]) => [k, { address: v.address, returnType: v.returnType }])
            ),
            constants: this.constants,
            persistGlobals: this.persistGlobals,
            udpGlobals: this.udpGlobals,
            codeOffset: header.length + constPool.length + heapDecl.length + funcTable.length + persistTable.length + globalsTable.length,
            codeSize: this.code.length,
            warnings: this.warnings,
        };
    }

    encodeHeapDeclarations() {
        if (this.heapArrays.size === 0) return new Uint8Array(0);
        // ESP32 VM heap limit. AUTO-INJECTED by idesrc/bundle.py from
        // TC_MAX_HEAP (user_config_override.h wins, else the ESP32
        // default in xdrv_124_tinyc_vm.h) — do NOT hand-edit; the
        // literal below is only a fallback for running un-bundled.
        const TC_MAX_HEAP_SLOTS = 16384;
        const TC_WARN_HEAP_SLOTS = 14000;
        let totalSlots = 0;
        for (const [, info] of this.heapArrays) totalSlots += info.arraySize;
        // The DEVICE is the real enforcer: the VM heap is grown
        // dynamically and rejected at load against the *actual*
        // TC_MAX_HEAP (which user_config_override.h may raise). The
        // IDE only knows the firmware default, so past it we WARN, not
        // block — a big program is fine on a device with a raised
        // TC_MAX_HEAP. A hard error stays ONLY for an absurd overflow
        // (almost certainly a bug) so broken programs still fail fast.
        const TC_HEAP_HARD_CEIL = TC_MAX_HEAP_SLOTS * 4;
        if (totalSlots > TC_HEAP_HARD_CEIL) {
            throw new CodeGenError(
                `Heap way over limit: ${totalSlots} slots (${Math.round(totalSlots*4/1024)} KB) — far above the ` +
                `${TC_MAX_HEAP_SLOTS}-slot firmware default; almost certainly an oversized-array bug ` +
                `(char[N] = 1 slot/byte, int[N]/float[N] = 1 slot/element).`);
        }
        if (totalSlots > TC_MAX_HEAP_SLOTS) {
            this.warnings.push(
                `Heap ${totalSlots} slots (${Math.round(totalSlots*4/1024)} KB) exceeds the ${TC_MAX_HEAP_SLOTS}-slot ` +
                `firmware default — OK only if the target device has TC_MAX_HEAP raised in user_config_override.h, ` +
                `else it is rejected at load. The device is authoritative.`);
        } else if (totalSlots > TC_WARN_HEAP_SLOTS) {
            this.warnings.push(`Heap usage high: ${totalSlots}/${TC_MAX_HEAP_SLOTS} slots (${Math.round(totalSlots*4/1024)} KB). Consider reducing array sizes.`);
        }
        const bytes = [];
        bytes.push(this.heapArrays.size);  // count
        for (const [handle, info] of this.heapArrays) {
            bytes.push(info.heapHandle & 0xFF);           // handle index
            bytes.push((info.arraySize >> 8) & 0xFF);     // size high byte
            bytes.push(info.arraySize & 0xFF);             // size low byte
        }
        return new Uint8Array(bytes);
    }

    encodeFunctionTable() {
        const callbacks = [];
        for (const [name, info] of this.functions) {
            if (info.address < 0) continue;
            // Include if it's a known callback name OR was referenced by spawnTask/killTask/taskRunning
            if (CALLBACK_NAMES.includes(name) || this.spawnTargets.has(name)) {
                callbacks.push({ name, address: info.address });
            }
        }
        if (callbacks.length === 0) return new Uint8Array(0);
        const bytes = [callbacks.length];
        for (const cb of callbacks) {
            const nameBytes = new TextEncoder().encode(cb.name);
            bytes.push(nameBytes.length);
            for (const b of nameBytes) bytes.push(b);
            bytes.push((cb.address >> 8) & 0xFF, cb.address & 0xFF);
        }
        return new Uint8Array(bytes);
    }

    encodePersistTable() {
        if (this.persistGlobals.length === 0) return new Uint8Array(0);
        const bytes = [this.persistGlobals.length]; // count (u8)
        for (const entry of this.persistGlobals) {
            // index: u16, slotCount: u16
            bytes.push((entry.index >> 8) & 0xFF, entry.index & 0xFF);
            bytes.push((entry.slotCount >> 8) & 0xFF, entry.slotCount & 0xFF);
        }
        return new Uint8Array(bytes);
    }

    encodeGlobalsTable() {
        if (this.udpGlobals.length === 0) return new Uint8Array(0);
        const bytes = [this.udpGlobals.length]; // count (u8)
        for (const entry of this.udpGlobals) {
            const nameBytes = new TextEncoder().encode(entry.name);
            bytes.push(nameBytes.length);
            for (const b of nameBytes) bytes.push(b);
            bytes.push((entry.index >> 8) & 0xFF, entry.index & 0xFF);
            bytes.push((entry.slotCount >> 8) & 0xFF, entry.slotCount & 0xFF);
            bytes.push(entry.varType || 0); // type: 0=float scalar, 1=float array, 2=char array
        }
        return new Uint8Array(bytes);
    }

    encodeConstantPool() {
        const bytes = [];
        for (const c of this.constants) {
            if (typeof c === 'string') {
                bytes.push(0x01); // type: string
                const encoded = new TextEncoder().encode(c);
                bytes.push((encoded.length >> 8) & 0xFF);
                bytes.push(encoded.length & 0xFF);
                for (const b of encoded) bytes.push(b);
            } else if (typeof c === 'number') {
                bytes.push(0x02); // type: number
                const view = new DataView(new ArrayBuffer(4));
                view.setFloat32(0, c, false);
                for (let i = 0; i < 4; i++) bytes.push(view.getUint8(i));
            }
        }
        return new Uint8Array(bytes);
    }
}

