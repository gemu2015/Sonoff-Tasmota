// Stack-based VM instruction set

export const Op = {
    // ─── Stack operations ─────────────────────
    NOP:            0x00,
    HALT:           0x01,
    PUSH_I32:       0x02,   // push 4-byte int32
    PUSH_F32:       0x03,   // push 4-byte float32
    PUSH_I8:        0x04,   // push 1-byte int (sign-extended to 32)
    PUSH_I16:       0x05,   // push 2-byte int (sign-extended to 32)
    POP:            0x06,
    DUP:            0x07,

    // ─── Integer arithmetic ───────────────────
    ADD:            0x10,
    SUB:            0x11,
    MUL:            0x12,
    DIV:            0x13,
    MOD:            0x14,
    NEG:            0x15,

    // ─── Float arithmetic ─────────────────────
    FADD:           0x18,
    FSUB:           0x19,
    FMUL:           0x1A,
    FDIV:           0x1B,
    FNEG:           0x1C,

    // ─── Bitwise ──────────────────────────────
    BIT_AND:        0x20,
    BIT_OR:         0x21,
    BIT_XOR:        0x22,
    BIT_NOT:        0x23,
    SHL:            0x24,
    SHR:            0x25,

    // ─── Comparison (result: 0 or 1) ──────────
    EQ:             0x30,
    NEQ:            0x31,
    LT:             0x32,
    GT:             0x33,
    LTE:            0x34,
    GTE:            0x35,
    FEQ:            0x36,
    FNEQ:           0x37,
    FLT:            0x38,
    FGT:            0x39,
    FLTE:           0x3A,
    FGTE:           0x3B,

    // ─── Logical ──────────────────────────────
    LOGIC_AND:      0x40,
    LOGIC_OR:       0x41,
    LOGIC_NOT:      0x42,

    // ─── Control flow ─────────────────────────
    JMP:            0x50,   // unconditional jump (2-byte offset)
    JZ:             0x51,   // jump if zero (2-byte offset)
    JNZ:            0x52,   // jump if not zero (2-byte offset)
    CALL:           0x53,   // call function (2-byte address)
    RET:            0x54,   // return from function
    RET_VAL:        0x55,   // return with value on stack
    CALL_INDIRECT:  0x56,   // pop addr from stack, call (function pointer)

    // ─── Variables ────────────────────────────
    LOAD_LOCAL:     0x60,   // push local[index] (1-byte index)
    STORE_LOCAL:    0x61,   // pop into local[index]
    LOAD_GLOBAL:    0x62,   // push global[index] (2-byte index)
    STORE_GLOBAL:   0x63,   // pop into global[index]
    STORE_GLOBAL_UDP:0x64,  // pop into global[index] + send via UDP (u16 index, u16 name_const)

    // ─── Arrays ───────────────────────────────
    LOAD_LOCAL_ARR: 0x68,   // push local_arr[base_idx][stack_top]
    STORE_LOCAL_ARR:0x69,   // pop val, pop idx, store to local_arr[base_idx]
    LOAD_GLOBAL_ARR:0x6A,   // push global_arr[base_idx][stack_top]
    STORE_GLOBAL_ARR:0x6B,  // pop val, pop idx, store to global_arr[base_idx]

    // ─── Type conversion ──────────────────────
    I2F:            0x70,   // int to float
    F2I:            0x71,   // float to int
    I2C:            0x72,   // int to char (mask 0xFF)

    // ─── Array address refs ────────────────
    ADDR_LOCAL:     0x78,   // push local array ref: (fp<<16)|base_idx
    ADDR_GLOBAL:    0x79,   // push global array ref: 0x80000000|base_idx

    // ─── Heap arrays ────────────────────────
    LOAD_HEAP_ARR:  0xA0,   // u8 handle; pop idx → push value
    STORE_HEAP_ARR: 0xA1,   // u8 handle; pop val, pop idx → store
    ADDR_HEAP:      0xA2,   // u8 handle → push heap ref: 0xC0000000|handle

    // ─── Runtime array ref (ref params) ────────
    LOAD_REF_ARR:   0xA3,   // u8 local_idx; pop idx → push *(resolveRef(local[local_idx])+idx)
    STORE_REF_ARR:  0xA4,   // u8 local_idx; pop val, pop idx → *(resolveRef(local[local_idx])+idx)=val

    // ─── Watch variables ─────────────────────────
    STORE_WATCH:    0xA5,   // u16 varIdx, u16 shadowIdx, u16 writtenIdx — store with change tracking

    // ─── Heap array ref with slot offset ────────
    // Used for strcpy(arr[i].field, ...) where arr is a heap-resident struct
    // array: field_offset within the struct element is computed at runtime.
    // Pops the computed offset (int32 slots) from the stack; packs it into the
    // ref so all downstream string ops see the sub-array starting at that offset.
    ADDR_HEAP_OFF:  0xA6,   // u8 handle; pop offset → push ref: 0xC0000000|(offset<<16)|handle

    // ─── System calls ─────────────────────────
    SYSCALL:        0x80,   // syscall with 1-byte function ID
    SYSCALL2:       0x81,   // syscall with 2-byte function ID (extended range 256+)

    // ─── Constants ────────────────────────────
    LOAD_CONST:     0x90,   // load from constant pool (2-byte index)
};

// Reverse lookup for disassembly
export const OpName = {};
for (const [name, code] of Object.entries(Op)) {
    OpName[code] = name;
}

// Syscall IDs for ESP32 hardware abstraction
export const Syscall = {
    // GPIO
    PIN_MODE:       0,   // (pin, mode) -> void
    DIGITAL_WRITE:  1,   // (pin, val) -> void
    DIGITAL_READ:   2,   // (pin) -> int
    ANALOG_READ:    3,   // (pin) -> int
    ANALOG_WRITE:   4,   // (pin, val) -> void
    GPIO_INIT:      5,   // (pin, mode) -> void  (release from Tasmota + pinMode)
    // 1-Wire (native bit-bang in C)
    OW_SET_PIN:     6,   // (pin) -> void
    OW_RESET:       7,   // () -> int (1=presence)
    OW_WRITE:       8,   // (byte) -> void
    OW_READ:        9,   // () -> int (byte)
    OW_WRITE_BIT:   98,  // (bit) -> void
    OW_READ_BIT:    99,  // () -> int (0 or 1)
    OW_SEARCH_RESET: 204, // () -> void — reset search state
    OW_SEARCH:      205,  // (buf) -> int — search next ROM into buf[8]

    // Timing
    DELAY:          10,  // (ms) -> void
    DELAY_MICRO:    11,  // (us) -> void
    MILLIS:         12,  // () -> int
    MICROS:         13,  // () -> int
    TIMER_START:    14,  // (id, ms) -> void — start countdown timer
    TIMER_DONE:     15,  // (id) -> int — 1 if expired, 0 if running
    TIMER_STOP:     16,  // (id) -> void — cancel timer
    TIMER_REMAINING:17,  // (id) -> int — ms remaining (0 if done)

    // Serial
    SERIAL_BEGIN:   20,  // (rxpin, txpin, baud, config, bufsize) -> int (1=ok, -1=err, -2=pins busy)
    SERIAL_PRINT:   21,  // (str_const_id) -> void
    SERIAL_PRINT_INT: 22, // (val) -> void
    SERIAL_PRINT_FLOAT: 23, // (val) -> void
    SERIAL_PRINTLN: 24,  // (str_const_id) -> void
    SERIAL_READ:    25,  // () -> int (-1 if none)
    SERIAL_AVAILABLE: 26, // () -> int
    SERIAL_CLOSE:   27,  // () -> void
    SERIAL_WRITE_BYTE: 28, // (byte) -> void — write single raw byte
    SERIAL_WRITE_STR:  29, // (buf_ref) -> void — write string from char array
    SERIAL_WRITE_BUF:  18, // (buf_ref, len) -> void — write len bytes (binary safe)

    // Math
    MATH_ABS:       30,  // (val) -> int
    MATH_MIN:       31,  // (a, b) -> int
    MATH_MAX:       32,  // (a, b) -> int
    MATH_MAP:       33,  // (val, fromLo, fromHi, toLo, toHi) -> int
    MATH_RANDOM:    34,  // (min, max) -> int
    MATH_SQRT:      35,  // (val) -> float
    MATH_SIN:       36,  // (val) -> float
    MATH_COS:       37,  // (val) -> float
    MATH_FLOOR:     38,  // (val) -> int
    MATH_CEIL:      39,  // (val) -> int
    MATH_ROUND:     40,  // (val) -> int
    MATH_EXP:       198, // (val) -> float  — e^x
    MATH_LOG:       199, // (val) -> float  — natural logarithm
    MATH_POW:       19,  // (base, exp) -> float  — x^y
    MATH_ACOS:      123, // (val) -> float  — inverse cosine
    INT_BITS_TO_FLOAT: 49, // (int_bits) -> float — reinterpret int32 as IEEE754 float

    // String operations (work with array refs)
    STRLEN:         50,  // (ref) -> int
    STRCPY:         51,  // (dst_ref, src_ref) -> void
    STRCAT:         52,  // (dst_ref, src_ref) -> void
    STRCMP:          53,  // (ref_a, ref_b) -> int
    STR_PRINT:      54,  // (ref) -> void
    STRCPY_CONST:   55,  // (dst_ref, const_idx) -> void
    STRCAT_CONST:   56,  // (dst_ref, const_idx) -> void
    STR_FIND_CONST: 47,  // (haystack_ref, needle_const_idx) -> int (-1=not found)
    RESPONSE_CMND_STR: 48, // (const_idx) -> void — responseCmnd with string literal
    SPRINTF_INT:    57,  // (dst_ref, fmt_const_idx, int_val) -> int chars
    SPRINTF_FLT:    58,  // (dst_ref, fmt_const_idx, float_val) -> int chars
    SPRINTF_STR:    59,  // (dst_ref, fmt_const_idx, src_ref) -> int chars
    // sprintf append variants (append to existing string in dst)
    SPRINTF_INT_CAT:70,  // (dst_ref, fmt_const_idx, int_val) -> total len
    SPRINTF_FLT_CAT:71,  // (dst_ref, fmt_const_idx, float_val) -> total len
    SPRINTF_STR_CAT:72,  // (dst_ref, fmt_const_idx, src_ref) -> total len

    // Tasmota command
    TASM_CMD:       43,  // (const_idx_cmd, out_buf_ref) -> int response_length
    TASM_CMD_REF:   248, // (cmd_ref, out_buf_ref) -> int response_length — cmd from char array
    TASM_DEFER:     366, // (cmd_ref) -> void — push command into tc_defer_command queue (runs on main task between callbacks)
    TASM_DEFER_STR: 367, // (cmd_const_idx) -> void — defer a string-literal command

    // File I/O (work with const pool for filenames, array refs for buffers)
    FILE_OPEN:      60,  // (const_idx_path, mode) -> int handle (-1=err)
    FILE_CLOSE:     61,  // (handle) -> int (0=ok, -1=err)
    FILE_READ:      62,  // (handle, buf_ref, maxBytes) -> int bytes_read (-1=err)
    FILE_WRITE:     63,  // (handle, buf_ref, len) -> int bytes_written (-1=err)
    FILE_EXISTS:    64,  // (const_idx_path) -> int (1=yes, 0=no)
    FILE_DELETE:    65,  // (const_idx_path) -> int (0=ok, -1=err)
    // _REF variants: path from char array instead of string literal
    FILE_OPEN_REF:    224, // (path_ref, mode) -> int handle (-1=err)
    FILE_EXISTS_REF:  225, // (path_ref) -> int (1=yes, 0=no)
    FILE_DELETE_REF:  226, // (path_ref) -> int (0=ok, -1=err)
    // Directory listing (reuses file handle slots)
    FILE_OPENDIR:     227, // (const_idx_path) -> int handle (-1=err)
    FILE_OPENDIR_REF: 228, // (path_ref) -> int handle (-1=err)
    FILE_READDIR:     229, // (handle, name_buf_ref) -> int (1=entry, 0=end)
    FILE_RANGE:       260, // (handle, min_ref, max_ref) -> int total_rows
    FILE_SIZE:      66,  // (const_idx_path) -> int bytes (-1=err)
    FILE_FORMAT:    67,  // () -> int — format LittleFS (erases all data)
    FILE_MKDIR:     68,  // (const_idx_path) -> int (1=ok, 0=fail)
    FILE_RMDIR:     69,  // (const_idx_path) -> int (1=ok, 0=fail)
    FILE_SEEK:      73,  // (handle, offset, whence) -> int (1=ok, 0=fail)
    FILE_TELL:      79,  // (handle) -> int position (-1=err)
    FILE_DOWNLOAD:  220, // (const_idx_path, url_ref) -> int HTTP status code
    FILE_GET_STR:   221, // (dst_ref, handle, const_idx_delim, index, endChar) -> int strlen
    FILE_EXTRACT:   222, // (handle, from_ref, to_ref, col_offs, accum, arr_refs..., N) -> rows
    FILE_EXTRACT_FAST: 223, // same as above but seeks to saved position

    // Heap allocation
    HEAP_ALLOC:     80,  // pop size -> push handle (-1 on fail)
    HEAP_FREE:      81,  // pop handle -> void

    // File array I/O (tab-delimited text format)
    FILE_READ_ARR:  82,  // (arr_ref, handle) -> int element_count
    FILE_WRITE_ARR: 83,  // (arr_ref, handle, append) -> void
    FILE_LOG:       84,  // (const_idx_path, str_ref, limit) -> int file_size

    // Time / timestamp functions (Scripter-compatible formats)
    TIME_STAMP:     85,  // (buf_ref) -> int — get current Tasmota timestamp into char[]
    TIME_CONVERT:   86,  // (buf_ref, flg) -> int — convert format: 0=to web, 1=to German
    TIME_OFFSET:    87,  // (buf_ref, days, zeroFlag) -> int — add day offset, opt zero time
    TIME_TO_SECS:   88,  // (buf_ref) -> int — timestamp string to epoch seconds
    SECS_TO_TIME:   89,  // (buf_ref, secs) -> int — epoch seconds to timestamp string

    // Tasmota output (for callbacks — route to Tasmota APIs)
    RESPONSE_APPEND:    90,  // (char_ref) -> void — append to JSON telemetry
    WEB_SEND:           91,  // (char_ref) -> void — send to web page
    WEB_FLUSH:          92,  // () -> void — flush web content buffer
    RESPONSE_APPEND_STR:93,  // (const_idx) -> void — append string literal to JSON
    WEB_SEND_STR:       94,  // (const_idx) -> void — send string literal to web page
    LOG:                95,  // (char_ref) -> void — AddLog to Tasmota console
    LOG_STR:            96,  // (const_idx) -> void — AddLog string literal
    LOG_LEVEL:          269, // (level, char_ref) -> void — AddLog with explicit level
    LOG_LEVEL_STR:      270, // (level, const_idx) -> void — AddLog string literal with level
    LGETSTRING:         97,  // (index, dst_ref) -> int length — get localized string

    // UDP multicast (Scripter-compatible, 239.255.255.250:1999)
    UDP_SEND:       100, // (const_idx_name, float_val) -> void — send =>name=value
    UDP_RECV:       101, // (const_idx_name) -> float — last received value (0 if none)
    UDP_READY:      102, // (const_idx_name) -> int — 1 if value received since last read
    UDP_SEND_ARRAY: 103, // (const_idx_name, arr_ref, count) -> void — send float array
    UDP_RECV_ARRAY: 104, // (const_idx_name, arr_ref, maxcount) -> int — recv float array
    UDP_SEND_STR:   149, // (const_idx_name, str_ref) -> void — send =>name=string (ASCII mode)

    // I2C bus — sensor driver support (bus: 0 or 1)
    I2C_READ8:      105, // (addr, reg, bus) -> int — read 1 byte from register
    I2C_WRITE8:     106, // (addr, reg, val, bus) -> int — write 1 byte, returns 1=ok
    I2C_READ_BUF:   107, // (addr, reg, buf_ref, len, bus) -> int — read bytes into char[]
    I2C_WRITE_BUF:  108, // (addr, reg, buf_ref, len, bus) -> int — write bytes from char[]
    I2C_EXISTS:     109, // (addr, bus) -> int — 1 if device responds on bus
    I2C_SET_DEVICE: 127, // (addr, bus) -> int — check available & responsive (not claimed)
    I2C_SET_FOUND:  128, // (addr, const_type, bus) -> void — register as claimed + log
    I2C_READ_RS:    129, // (addr, reg, buf_ref, len, bus) -> int — repeated START read (SMBus)
    I2C_READ_BUF0:  112, // (addr, buf_ref, len, bus) -> int — read without register byte
    I2C_WRITE0:     113, // (addr, reg, bus) -> int — write register only, no data
    I2C_FREE:       249, // (addr, bus) -> void — release claimed I2C address
    WEB_CHART_SIZE: 233, // (width, height) -> void — set chart div size in pixels
    WEB_CHART_TBASE: 261, // (minutes) -> void — set time base offset from "now"
    WEB_CHART_JS:   354, // (js_str) -> void — attach script JS to the last WebChart (runs in draw scope with dt,o,el; set o.done to take over the draw)

    // Console command callback
    ADD_COMMAND:    45, // (const_idx_prefix) -> void — register command prefix
    RESPONSE_CMND: 46, // (buf_ref) -> void — send console response

    // General-purpose UDP (Scripter-compatible udp() function, modes 0-7)
    UDP_FUNC:       126, // (args..., mode) -> int — general-purpose UDP dispatcher

    // SPI bus — sensor/display driver support
    SPI_INIT:       120, // (sclk, mosi, miso, speed_mhz) -> int — init SPI, returns 1=ok
    SPI_SET_CS:     121, // (index, pin) -> void — set CS pin for slot (1-4)
    SPI_TRANSFER:   122, // (cs, buf_ref, len, mode) -> int — transfer bytes, returns len

    // Smart Meter (SML) — read/write meter values when USE_SML is defined
    SML_GET:        110, // (index) -> float — get meter value (1-based, 0=count)
    SML_GETSTR:     111, // (index, buf_ref) -> int — get meter ID string into buf
    SML_WRITE:      114, // (meter, buf_ref) -> int — send hex sequence to meter
    SML_READ:       115, // (meter, buf_ref) -> int — read raw meter buffer into buf
    SML_SETBAUD:    116, // (meter, baud) -> int — change meter baud rate
    SML_SETWSTR:    117, // (meter, buf_ref) -> int — set async write string for next send
    SML_SETOPT:     118, // (options) -> int — set SML global options
    SML_GETV:       119, // (sel) -> int — get/reset data valid flags
    SML_WRITE_STR:  124, // (meter, const_idx) -> int — send string literal to meter
    SML_SETWSTR_STR:125, // (meter, const_idx) -> int — set async write from string literal

    // String manipulation
    STR_TOKEN:      74,  // (dst_ref, src_ref, delim_char, n) -> int — get nth token
    STR_SUB:        75,  // (dst_ref, src_ref, pos, len) -> int — substring
    STR_FIND:       76,  // (haystack_ref, needle_ref) -> int — find position (-1=not found)
    STR_TO_INT:     77,  // (src_ref) -> int — parse string to integer (atoi)
    STR_TO_FLOAT:   78,  // (src_ref) -> float — parse string to float (atof)

    // Tasmota sensor JSON parsing
    SENSOR_GET:     132, // (const_idx_path) -> float — read sensor by JSON path

    // HTTP
    HTTP_GET:       140, // (url_ref, response_ref) -> int length
    HTTP_POST:      141, // (url_ref, data_ref, response_ref) -> int length
    HTTP_HEADER:    142, // (name_ref, value_ref) -> void
    WEB_PARSE:      143, // (src_ref, delim_const, index, dst_ref) -> int length
    WEB_SEND_JSON_ARRAY: 148, // (arr_ref, count) -> void — send float array as JSON integer array

    // WebUI widgets (generate HTML in WebUI() callback)
    WEB_BUTTON:     150, // (gidx, label_ref) -> void — toggle button
    WEB_SLIDER:     151, // (gidx, min, max, label_ref) -> void — range slider
    WEB_CHECKBOX:   152, // (gidx, label_ref) -> void — checkbox
    WEB_TEXT:        153, // (gidx, maxlen, label_ref) -> void — text input
    WEB_NUMBER:     154, // (gidx, min, max, label_ref) -> void — number input
    WEB_PULLDOWN:   155, // (gidx, options_ref, label_ref) -> void — select dropdown
    WEB_RADIO:      156, // (gidx, options_ref, label_ref) -> void — radio buttons
    WEB_TIME:       157, // (gidx, label_ref) -> void — time input (HHMM)
    WEB_PAGE_LABEL: 158, // (page_num, label_ref) -> void — register page with button label
    WEB_PAGE:       159, // () -> int — returns current page number being rendered
    WEB_SEND_FILE:  160, // (filename_ref) -> void — send file contents to web page
    WEB_ON:         161, // (handler_num, url_ref) -> void — register custom web endpoint
    WEB_HANDLER:    162, // () -> int — returns current web handler number
    WEB_ARG:        163, // (name_ref, buf_ref) -> int — get HTTP arg into buffer
    MDNS:           164, // (name_ref, mac_ref, type_ref) -> int — register mDNS service
    WEB_CONSOLE_BUTTON: 165, // (url_const, label_const) -> void — add button to Utilities menu
    WEB_CHART:          166, // (type, title_const, unit_const, color, pos, count, array_ref, decimals, interval, ymin, ymax) -> void
    PLUGIN_QUERY:       167, // (dst_ref, index, p1, p2) -> strlen — Plugin_Query(index, (p1<<8)|p2, 0)
    SORT_ARRAY:         168, // (arr_ref, count, flags) -> void — sort array (flags: 0=int asc, 1=float asc, 2=int desc, 3=float desc)

    // Webcam (ESP32 — requires USE_WEBCAM)
    CAM_CONTROL:        169, // (sel, p1, p2) -> int — multiplexed webcam control
    CAM_INIT_PINS:      206, // (pins_ref, format, framesize, quality) -> int — init camera with custom pins

    // Hardware register peek/poke (ESP32 memory-mapped I/O)
    PEEK_REG:           207, // (addr) -> int — read 32-bit from memory-mapped address
    POKE_REG:           208, // (addr, val) -> void — write 32-bit to memory-mapped address

    // Tasmota info getters
    TASM_GET_STR:       209, // (sel, buf_ref) -> int — get Tasmota string info
    TASM_POWER:         217, // (index) -> int — power state of relay (0-based)
    TASM_SWITCH:        218, // (index) -> int — switch state (0-based)
    TASM_COUNTER:       219, // (index) -> int — pulse counter (0-based)

    // Display drawing (direct renderer calls — requires USE_DISPLAY)
    DSP_TEXT:       170, // (buf_ref) -> void — raw DisplayText command string
    DSP_CLEAR:      171, // () -> void — clear display
    DSP_POS:        172, // (x, y) -> void — set draw position
    DSP_FONT:       173, // (f) -> void — set font (0-7)
    DSP_SIZE:       174, // (s) -> void — set text size
    DSP_COLOR:      175, // (fg, bg) -> void — set fg/bg color (16-bit 565)
    DSP_DRAW:       176, // (buf_ref) -> void — draw string at current pos
    DSP_PIXEL:      177, // (x, y) -> void — draw pixel
    DSP_LINE:       178, // (x1, y1) -> void — draw line from pos to (x1,y1)
    DSP_RECT:       179, // (w, h) -> void — draw rectangle at pos
    DSP_FILL_RECT:  180, // (w, h) -> void — draw filled rectangle at pos
    DSP_CIRCLE:     181, // (r) -> void — draw circle at pos
    DSP_FILL_CIRCLE:182, // (r) -> void — draw filled circle at pos
    DSP_HLINE:      183, // (w) -> void — horizontal line from pos
    DSP_VLINE:      184, // (h) -> void — vertical line from pos
    DSP_ROUND_RECT: 185, // (w, h, r) -> void — rounded rectangle
    DSP_FILL_RRECT: 186, // (w, h, r) -> void — filled rounded rectangle
    DSP_TRIANGLE:   187, // (x1, y1, x2, y2) -> void — triangle from pos
    DSP_FILL_TRI:   188, // (x1, y1, x2, y2) -> void — filled triangle
    DSP_DIM:        189, // (val) -> void — set brightness (0-15)
    DSP_ONOFF:      190, // (on) -> void — display on/off
    DSP_UPDATE:     191, // () -> void — update display (e-paper refresh)
    DSP_PICTURE:    192, // (filename_const, scale) -> void — draw image file at pos
    DSP_WIDTH:      193, // () -> int — display width in pixels
    DSP_HEIGHT:     194, // () -> int — display height in pixels
    DSP_TEXT_STR:   195, // (const_idx) -> void — DisplayText from string literal
    DSP_DRAW_STR:   196, // (const_idx) -> void — draw string literal at current pos
    DSP_PAD:        197, // (n) -> void — set text padding for dspDraw (0=off)
    DSP_LOAD_IMG:   262, // (filename_const) -> int — load JPG to PSRAM, returns slot (0-3, -1=err)
    DSP_IMG_RECT:   263, // (slot, sx, sy, dx, dy, w, h) -> void — push sub-rect from image to screen
    DSP_IMG_WIDTH:  264, // (slot) -> int — get image width
    DSP_IMG_HEIGHT: 265, // (slot) -> int — get image height
    DSP_TEXT_WIDTH: 266, // (len) -> int — get pixel width for len chars in current font
    DSP_TEXT_HEIGHT:267, // () -> int — get pixel height for current font
    DSP_IMG_TEXT:   268, // (slot, x, y, color, fieldw, align, buf_ref) -> void — composite text on image
    DSP_LOAD_IMG_CAM: 277, // (cam_slot) -> img_slot  (-1=err)
    DSP_IMG_TEXT_BURN:278, // (slot,x,y,col,fldw,align,buf) -> void (mutates image)
    DSP_IMG_TO_CAM:   279, // (img_slot,cam_slot,quality) -> bytes  (-1=err)

    // RGB565 canvas / image slots — draw dsp* primitives into PSRAM buffers
    IMG_CREATE:     328, // (w, h) -> int slot (0..3, -1=err) — alloc blank canvas in PSRAM
    IMG_BEGIN_DRAW: 329, // (slot) -> void — redirect renderer so dsp* draws into canvas
    IMG_END_DRAW:   330, // () -> void — restore panel renderer
    IMG_CLEAR:      331, // (slot, color_rgb565) -> void — fast fill of canvas
    IMG_BLIT:       332, // (dst, src, sx, sy, dx, dy, w, h) -> void — canvas->canvas rect copy
    IMG_INVALIDATE: 333, // (slot, x, y, w, h) -> void — union rect into slot's dirty region
    IMG_FLUSH:      334, // (slot, panel_x, panel_y) -> void — push dirty region to panel + clear

    // Cross-VM shared key/value store (driver-global, mutex-protected)
    SHARE_SET_INT:  340, // (key_const_idx, val)     -> void
    SHARE_GET_INT:  341, // (key_const_idx)          -> int  (0 if missing)
    SHARE_SET_FLT:  342, // (key_const_idx, val)     -> void
    SHARE_GET_FLT:  343, // (key_const_idx)          -> float (0.0 if missing)
    SHARE_SET_STR:  344, // (key_const_idx, src_ref) -> void
    SHARE_GET_STR:  345, // (key_const_idx, dst_ref) -> int  chars copied
    SHARE_HAS:      346, // (key_const_idx)          -> int  0/1
    SHARE_DELETE:   347, // (key_const_idx)          -> int  1 if removed
    SHARE_DUMP:     352, // ()                       -> int  number of live entries
    SHARE_SET_FLT_KEY: 353, // (key_ref, val)        -> void  like shareSetFloat but key is a runtime char[] (or literal)
    PERSIST_DUMP:   395, // ()                       -> int  number of persist entries (logs each)
    DMX_WRITE:      396, // (channel, value)         -> void  set DMX512 slot (1..512) to 0..255; TX-only
    DMX_INIT:       397, // (gpio)                   -> int   configure DMX512 TX on an RMT channel at GPIO (no UART); 1=ok 0=fail
    // Matter data-model scripting (USE_MATTER_C) — append-only, never renumber
    MTR_ADD:        398, // (deviceType)             -> int   matterAdd(): new endpoint id (<0 err)
    MTR_CLUSTER:    399, // (ep, clusterId)          -> void  matterCluster(): add a cluster
    MTR_ATTR:       400, // (ep, cl, attrId, type)   -> void  matterAttr(): declare attribute
    MTR_SET:        401, // (ep, cl, attr, value)    -> void  matterSet(): publish a value
    MTR_GET:        402, // (ep, cl, attr)           -> int   matterGet(): cached value (0 if absent)
    MTR_START:      403, // ()                       -> int   matterStart(): advertise + commission, 0=ok
    MTR_RESET:      404, // ()                       -> void  matterReset(): clear data model to root
    // Addressable RGB LED (WS2812/SK6812) via RMT — no USE_LIGHT / template
    RGB_LED:        405, // (gpio, 0xRRGGBB)         -> int   rgbLed(): drive one WS2812 pixel; 1=ok
    MTR_SETF:       406, // (ep,cl,attr,fval,scale)  -> void  matterSetFloat(): store round(fval*scale) as int64 (temp/power)
    MTR_EVENT:      407, // (ep,cl,eventId,a,b)      -> void  matterEvent(): emit a Matter event (Generic Switch buttons)
    MTR_NAME:       408, // (ep, name)               -> void  matterName(): name an endpoint (bridge -> Apple Home accessory title)
    // BLE scan/observe (firmware: USE_TINYC_BLE -> USE_BLE_ESP32). 416-424 reserved for GATT client.
    BLE_SCAN:       409, // (ms)                     -> int   bleScan(): start capturing adverts (clears queue; ms>0 auto-stops)
    BLE_SCAN_STOP:  410, // ()                       -> int   bleScanStop(): stop capturing
    BLE_NEXT:       411, // ()                       -> int   bleNext(): pop next advert into current; 1=got one, 0=empty
    BLE_MAC:        412, // (buf)                    -> int   bleMac(): write 6 MAC bytes into buf[0..5]; returns 6
    BLE_RSSI:       413, // ()                       -> int   bleRssi(): RSSI of current advert (dBm, negative)
    BLE_NAME:       414, // (buf)                    -> int   bleName(): copy advert local name into char[]; returns len
    BLE_MFG:        415, // (buf)                    -> int   bleMfg(): write manufacturer-data bytes into buf; returns len
    // GATT client (non-blocking): bleTarget() then bleReadStart()/bleWriteStart(), poll bleDone(), bleResult().
    BLE_ADDRTYPE:   416, // ()                       -> int   bleAddrType(): addr type of current advert (0=public, else random)
    BLE_TARGET:     417, // (mac, addrtype, svc16)   -> int   bleTarget(): set GATT target address + service UUID
    BLE_READ_START: 418, // (notify16)               -> int   bleReadStart(): connect + subscribe notify char; 1=started, <0=busy
    BLE_WRITE_START:419, // (chr16, buf, len)        -> int   bleWriteStart(): connect + write char; 1=started, <0=busy
    BLE_DONE:       420, // ()                       -> int   bleDone(): 0=pending, >0=result len, <0=failed
    BLE_RESULT:     421, // (buf)                    -> int   bleResult(): copy received notify/read bytes; returns len
    JSON_NUM:       422, // (src_ref, path_ref)           -> float  jsonNum(): parse JSON in src by '#'-path, return numeric value
    JSON_STR:       423, // (src_ref, path_ref, dst_ref)  -> int    jsonStr(): parse JSON string value into dst, return length (-1 absent)
    VAR_IDX:        424, // (var_ref)                     -> int    varIdx(): global index of a var (for hand-built tcbtn/seva/siva HTML)
    WEB_BUTTON_V:   425, // (var_ref, label_ref)          -> void   webButtonV(): like webButton but RUNTIME label (char[])
    WEB_SLIDER_V:   426, // (var_ref, min, max, label_ref)-> void   webSliderV(): like webSlider but RUNTIME label (char[])
    FAST_MUX:       427, // (flag, time, buf_ref, len)    -> int    fastMux(): HW-timer GPIO multiplexer (classic ESP32 / ESP32-S3 + USE_TINYC_FAST_MUX; -1 if not built)
    // LVGL on-device GUI (firmware: USE_TINYC_LVGL -> USE_LVGL + USE_UNIVERSAL_DISPLAY). 452-499 reserved for widgets/events.
    LVGL_INIT:      450, // ()                            -> int    lvglInit(): start LVGL on the panel (idempotent); returns 1 if active
    LVGL_ACTIVE:    451, // ()                            -> int    lvglActive(): 1 if LVGL is running, else 0
    LVGL_OBJ:           452, // (parent)            -> int   lvglObj(): base container; parent 0 = active screen
    LVGL_LABEL:         453, // (parent)            -> int   lvglLabel()
    LVGL_BUTTON:        454, // (parent)            -> int   lvglButton()
    LVGL_SET_POS:       455, // (h,x,y)             -> void  lvglSetPos()
    LVGL_SET_SIZE:      456, // (h,w,ht)            -> void  lvglSetSize()
    LVGL_ALIGN:         457, // (h,align,dx,dy)     -> void  lvglAlign() — align: 9=CENTER,1=TOP_LEFT,5=BOTTOM_MID
    LVGL_SET_TEXT:      458, // (h,str)             -> void  lvglSetText() (label text)
    LVGL_SET_BG_COLOR:  459, // (h,rgb888)          -> void  lvglSetBgColor()
    LVGL_SET_TEXT_COLOR:460, // (h,rgb888)          -> void  lvglSetTextColor()
    LVGL_EVENT_ENABLE:  461, // (h,filter)          -> void  lvglEventEnable() — LVGL9.5 codes: 0=ALL,4=SHORT_CLICKED,10=CLICKED,11=RELEASED,35=VALUE_CHANGED
    LVGL_EVENT:         462, // ()                  -> int   lvglEvent(): pop next event; 1=got,0=empty
    LVGL_EVENT_OBJ:     463, // ()                  -> int   lvglEventObj(): handle of current event
    LVGL_EVENT_CODE:    464, // ()                  -> int   lvglEventCode(): code of current event
    LVGL_DELETE:        465, // (h)                 -> int   lvglDelete()
    LVGL_CLEAN:         466, // (h)                 -> int   lvglClean(): delete children
    LVGL_SLIDER:        467, // (parent)            -> int   lvglSlider()
    LVGL_BAR:           468, // (parent)            -> int   lvglBar()
    LVGL_ARC:           469, // (parent)            -> int   lvglArc()
    LVGL_SWITCH:        470, // (parent)            -> int   lvglSwitch()
    LVGL_CHECKBOX:      471, // (parent)            -> int   lvglCheckbox()
    LVGL_SET_VALUE:     472, // (h,v,anim)          -> void  lvglSetValue() (slider/bar/arc)
    LVGL_GET_VALUE:     473, // (h)                 -> int   lvglGetValue()
    LVGL_SET_RANGE:     474, // (h,min,max)         -> void  lvglSetRange()
    LVGL_SET_CHECKED:   475, // (h,on)              -> void  lvglSetChecked() (switch/checkbox)
    LVGL_IS_CHECKED:    476, // (h)                 -> int   lvglIsChecked()
    LVGL_SET_STYLE_INT: 477, // (h,prop,val)        -> void  lvglSetStyleInt() — prop:120=RADIUS,56=BORDER_WIDTH,112=OPA
    LVGL_CHART:         478, // (parent)            -> int   lvglChart()
    LVGL_CHART_TYPE:    479, // (h,type)            -> void  lvglChartType() — 1=LINE, 2=BAR
    LVGL_CHART_SERIES:  480, // (chart,rgb)         -> int   lvglChartSeries()
    LVGL_CHART_NEXT:    481, // (chart,series,v)    -> void  lvglChartNext()
    LVGL_CHART_RANGE:   482, // (chart,axis,min,max)-> void  lvglChartRange() — axis 0=PRIMARY_Y
    LVGL_CHART_COUNT:   483, // (chart,n)           -> void  lvglChartCount()
    LVGL_IMAGE:         484, // (parent)            -> int   lvglImage()
    LVGL_IMAGE_SRC:     485, // (h,path)            -> void  lvglImageSrc() — LVGL FS path
    UI_SCREEN:      310, // (id)                                       -> void
    UI_THEME:       311, // (bg, accent, text, border)                 -> void
    UI_CLEAR_SCREEN:312, // ()                                          -> void
    UI_LABEL:       320, // (num,x,y,w,h,text_const,align)             -> void
    UI_LABEL_SET:   321, // (num, text_ref_or_const)                   -> void
    UI_CHECKBOX:    322, // (num,x,y,w,h,label_const)                  -> void
    UI_PROGRESS:    323, // (num,x,y,w,h,value,max)                    -> void
    UI_PROGRESS_SET:324, // (num, value)                               -> void
    UI_GAUGE:       325, // (num,x,y,r,value,vmin,vmax)                -> void
    UI_ICON:        326, // (num,x,y,img_slot)                         -> void
    UI_BUTTON:      327, // (num,x,y,w,h,label_const)                  -> void

    // Per-slot TCP-client tuning (v1.5.1). Operate on the currently selected
    // slot. Useful for Modbus-TCP, MQTT-TLS, REST clients facing peers with
    // aggressive idle-timeouts (SMA Tripower etc.) or where Nagle slows down
    // small request/response pairs.
    TCP_KEEPALIVE:           348, // (idle_sec, intvl_sec, count) -> int 1=ok 0=err
    TCP_NODELAY:             349, // (on)                         -> void
    TCP_DISCONNECT_REASON:   350, // ()                           -> int 0..5
    // Atomic write-then-wait-for-response (v1.6.0). Folds the canonical
    // tcpWriteArray + delay/poll-tcpAvailable + tcpReadArray pattern into one
    // syscall. Returns bytes received (>=0), -1 timeout, -2 disconnect, -3 bad args.
    TCP_TRANSACT:            351, // (req_ref, req_len, resp_ref, resp_max, timeout_ms) -> int

    // Binary library call (xblib_*) — phase-1: (BUF, INT) -> INT only.
    BLIB_CALL:               370, // (name_const, buf_ref, len) -> int

    // TWAI / CAN-bus (380..386). ESP32-only on the device; standalone
    // IDE runs return sentinel values via the simulator stubs below.
    TWAI_BEGIN:              380, // (rxpin, txpin, bitrate_kbps, mode) -> int 1=ok 0=err
    TWAI_END:                381, // () -> void
    TWAI_AVAILABLE:          382, // () -> int frames_queued
    TWAI_RECV:               383, // (id_ref, ext_ref, dlc_ref, data_buf, max) -> int bytes | -1
    TWAI_SEND:               384, // (id, ext, dlc, data_buf) -> int 1=ok 0=err
    TWAI_STATUS:             385, // (rx_q_ref, tx_q_ref, err_ref) -> int 1=ok 0=err
    TWAI_FILTER:             386, // (id_mask, id_value, ext) -> int 1=ok 0=err

    // WebOn raw / keep-alive controls (390..392). Required for emulating
    // EcoTracker-style devices whose firmware needs an exact 3-header
    // HTTP response (no auto Server/Date/Connection) plus a kept-alive
    // TCP socket — Jackery Homepower 2000 Ultra and similar storages.
    WEB_RAW_MODE:            390, // ()           -> void  disable Tasmota auto-headers for this request
    WEB_RAW_WRITE:           391, // (str_ref)    -> void  write raw bytes to Webserver->client()
    WEB_KEEP_ALIVE:          392, // ()           -> void  keep TCP socket alive after response

    // Soft pin-availability check — does NOT halt the VM. Returns 1 if pin is
    // free, 0 if forbidden (flash/red/claimed). Use before pinMode/owSetPin/
    // i2cBegin on user-configurable pins so a stale default doesn't crash the
    // slot before the WebUI can render and let the user pick a different pin.
    PIN_FREE:                393, // (pin)        -> int (1=free, 0=forbidden)
    WEB_TOGGLE:              394, // (gidx, label_ref) -> void — latching on/off button

    // Symmetric crypto (AES-128 / SHA-256 / HMAC-SHA256). All buffers are
    // TinyC char[] (one byte per int32 slot, low 8 bits used). Lengths in
    // bytes. AES ops are in-place on the data ref. Motivating use case:
    // Local Tuya v3.3 protocol so TinyC scripts can drive Smart-Life devices
    // (pool heat pumps, plugs, switches) directly without a cloud or bridge.
    AES_ECB:         360, // (key16_ref, data16_ref, enc_flag)               -> int 1=ok 0=err
    AES_CBC:         361, // (key16_ref, iv16_ref, data_ref, len, enc_flag)  -> int 1=ok 0=err
    HMAC_SHA256:     362, // (key_ref, klen, data_ref, dlen, out32_ref)      -> int 1=ok
    SHA256:          363, // (data_ref, dlen, out32_ref)                     -> int 1=ok
    HEX2BIN:         364, // (hex_ref_or_const, hex_len, out_ref)            -> int  bytes written
    BIN2HEX:         365, // (bin_ref, bin_len, out_ref)                     -> int  chars written
    MD5:             368, // (data_ref, dlen, out16_ref)                     -> int 1=ok (16-byte digest; Tuya key derivation)

    // Audio
    AUDIO_VOL:      200, // (vol) -> void — set volume 0-100
    AUDIO_PLAY:     201, // (file_ref) -> void — play MP3 file
    AUDIO_SAY:      202, // (text_ref) -> void — text-to-speech

    // Minimal I2S output (standalone, no USE_I2S_AUDIO needed)
    I2S_BEGIN:      271, // (bclk, lrclk, dout, sampleRate) -> int — init I2S TX
    I2S_WRITE:      272, // (arr_ref, len) -> int — write PCM samples
    I2S_STOP:       273, // () -> void — stop I2S
    FILE_READ_PCM16: 274, // (handle, arr, max_samples, channels) -> samples_read

    // Persistent variables
    PERSIST_SAVE:   203, // () -> void — save all persist globals to file

    // TCP server (Scripter-compatible ws* functions)
    TCP_OPEN:       210, // (port) -> int — start TCP server (0=ok, -1=fail, -2=no net)
    TCP_CLOSE:      211, // () -> void — close TCP server
    TCP_AVAILABLE:  212, // () -> int — accept client + return bytes available
    TCP_READ_STR:   213, // (buf_ref) -> int — read string from TCP into char[]
    TCP_WRITE_STR:  214, // (str_ref) -> void — write string to TCP client
    TCP_READ_ARR:   215, // (arr_ref) -> int — read TCP bytes into int array
    TCP_WRITE_ARR:  216, // (arr_ref, count, type) -> void — write array to TCP

    // TCP outgoing client (multi-slot)
    TCP_CONNECT:     290, // (ip_const, port) -> int — connect to ip:port (0=ok, -1=fail, -2=no net)
    TCP_DISCONNECT:  291, // () -> void — close currently selected outgoing TCP slot
    TCP_CONNECTED:   292, // () -> int — 1 if selected slot connected, else 0
    TCP_SELECT:      293, // (slot) -> void — pick outgoing TCP slot 0..TC_TCP_CLI_SLOTS-1
    TCP_CONNECT_REF: 294, // (ip_ref, port) -> int — connect with IP from runtime char array

    // MQTT — gated by USE_MQTT in firmware (no-ops returning -1 if not compiled)
    MQTT_SUBSCRIBE:   295, // (topic_const) -> int slot or -1
    MQTT_UNSUBSCRIBE: 296, // (topic_const) -> int 0=ok or -1
    MQTT_PUBLISH_TO:  297, // (topic_const, payload_const) -> int 0=ok or -1

    // FreeRTOS spawn/kill of user-defined functions as background tasks
    SPAWN_TASK:       298, // (name_const) -> int pool_idx (0..3) or -1
    SPAWN_TASK_STACK: 299, // (name_const, stack_kb) -> int pool_idx or -1 (kb clamped 3..16)
    KILL_TASK:        300, // (name_const) -> int 0=signaled, -1=not running
    TASK_RUNNING:     301, // (name_const) -> int 1/0

    // String ops 1.5.0
    STR_REPLACE_CONST:   302, // (arr, old_const, new_const) -> int (count)
    STR_STARTS_CONST:    303, // (arr, prefix_const) -> int 1/0
    STR_ENDS_CONST:      304, // (arr, suffix_const) -> int 1/0
    STR_CONTAINS_CONST:  305, // (arr, substr_const) -> int 1/0
    STR_TO_UPPER:        306, // (arr) -> void
    STR_TO_LOWER:        307, // (arr) -> void
    STR_TRIM:            308, // (arr) -> int (new length)

    // Tasmota system variables (virtual — accessed as tasm_xxx)
    TASM_GET:       130, // (index) -> int/float — read Tasmota variable
    TASM_SET:       131, // (index, value) -> void — write Tasmota variable

    // Deep sleep (ESP32)
    DEEP_SLEEP:     230, // (seconds) -> void — deep sleep with timer wakeup
    DEEP_SLEEP_GPIO:231, // (seconds, pin, level) -> void — + GPIO wakeup
    WAKEUP_CAUSE:   232, // () -> int — return wakeup cause

    // Email (ESP32 — requires USE_SENDMAIL)
    EMAIL_BODY:     234, // (body_ref) -> void — set email body text (HTML)
    EMAIL_ATTACH:   235, // (path_const) -> void — add file attachment
    EMAIL_SEND:     236, // (params_ref) -> int — send email, returns 0=ok
    EMAIL_ATTACH_PIC: 237, // (bufnum) -> void — attach webcam picture from RAM buffer 1..4
    EMAIL_BODY_STR: 238, // (const_idx) -> void — set email body from string literal
    SPRINTF_STR_CONST:    239, // (dst_ref, fmt_const_idx, src_const_idx) -> int chars
    SPRINTF_STR_CAT_CONST:247, // (dst_ref, fmt_const_idx, src_const_idx) -> total len

    // Touch buttons & sliders (display GFX)
    DSP_BUTTON:     240, // (num,x,y,w,h,oc,fc,tc,ts,text_const) -> void — power button
    DSP_TBUTTON:    241, // (num,x,y,w,h,oc,fc,tc,ts,text_const) -> void — virtual toggle
    DSP_PBUTTON:    242, // (num,x,y,w,h,oc,fc,tc,ts,text_const) -> void — virtual push
    DSP_SLIDER:     243, // (num,x,y,w,h,ne,bg,fc,bc) -> void — slider
    DSP_BTN_STATE:  244, // (num,val) -> void — set button/slider state
    TOUCH_BUTTON:   245, // (num) -> int — read button/slider state (-1 if undef)
    DSP_BTN_DEL:    246, // (num) -> void — delete button/slider (-1 = delete all)

    // Tesla Powerwall (ESP32 — requires TESLA_POWERWALL + email SSL library)
    PWL_REQUEST:    133, // (url_const) -> int — pwlRequest(url): config or API request, 0=ok
    PWL_GET_FLOAT:  134, // (path_const) -> float — pwlGet(path): extract float from last response
    PWL_GET_STR:    135, // (path_const, buf_ref) -> int — pwlStr(path, buf): extract string, returns len
    PWL_BIND:       136, // (var_ref, path_const) -> void — pwlBind(&var, path): register auto-fill binding

    // HomeKit (ESP32 — requires USE_HOMEKIT)
    HK_SET_CODE:    137, // (const_idx_code) -> void — hkSetCode("111-22-333")
    HK_ADD:         138, // (const_idx_name, type) -> void — hkAdd("name", HK_TEMPERATURE)
    HK_START:       139, // () -> int — hkStart(): build descriptor + start HomeKit, 0=ok
    HK_VAR:         144, // (var_ref) -> void — hkVar(variable): bind variable to current device
    HK_READY:       145, // (var_ref) -> int — hkReady(variable): 1 if HomeKit changed it

    // Filesystem info
    FS_INFO:        146, // (sel) -> int — fsInfo(sel): 0=total kB, 1=free kB

    // Addressable LED strip (WS2812 — requires USE_WS2812)
    WS2812:         147, // (arr_ref, len, offset) -> void — set pixels from array + show

    HK_INIT:        253, // (desc_ref) -> int — hkInit(desc[]): start HomeKit (raw descriptor), 0=ok
    HK_STOP:        254, // () -> void — hkStop(): stop HomeKit
    HK_RESET:       255, // () -> void — hkReset(): factory reset pairings

    // Debug / Simulator
    DEBUG_PRINT:    250, // (val) -> void  (prints to console)
    DEBUG_PRINT_STR:251, // (str_const_id) -> void
    DEBUG_DUMP:     252, // () -> void (dump VM state)

    // Extended syscalls (256+, requires OP_SYSCALL2)
    SML_COPY:       256, // (arr_ref, count) -> int — copy SML values to float array
    ARRAY_FILL:     257, // (arr_ref, value, count) -> void — fill array with value
    ARRAY_COPY:     258, // (dst_ref, src_ref, count) -> void — copy array to array

    // String concatenation: str1 + str2 operator
    STRCONCAT:      259, // (left_ref, right_ref) -> scratch_ref — concat two char[] into scratch buffer

    // String literal variants (fix #5, #7)
    STRCMP_CONST:    275, // (arr_ref, const_idx) -> int — strcmp(char[], "literal")
    FILE_WRITE_STR:  276, // (handle, const_idx, len) -> int — fileWrite(fd, "literal", len)

    // Repo-pulldown widget (Scripter smlpd-compatible): fetches JSON directory,
    // downloads selected file, POSTs it to /tc_api?cmd=writefile&path=<dest>
    // (277, 278, 279 taken by SYS_DSP_LOAD_IMG_CAM / IMG_TEXT_BURN / IMG_TO_CAM)
    WEB_REPO_PULLDOWN: 280, // (gref, label_c, json_url_c, index_key_c, dest_path_c) -> void

    // SML descriptor pin substitution — idempotent, template-comment preserving.
    // For each line containing %0?rxpin% / %0?txpin% / %0?smlf% placeholders:
    //   first call inserts a "; <original>" template line above and replaces
    //   placeholders in the active copy; subsequent calls rebuild the active
    //   line from the template comment (so re-edits always derive from the
    //   original markers). Pass -1 for any value to leave that placeholder alone.
    SML_APPLY_PINS:  281, // (path_c, rx_pin, tx_pin, smlf) -> int (subs done, -1=err, 0=no change)
    SML_SCRIPTER_LOAD: 282, // (path_c) -> int — extract >F/>S sections, compile to mini-scripter bytecode (lnv0..9, switch/case, if/endif, sml(m,0,baud), sml(m,1,"HEX")). Returns # sections compiled (0..2), -1=err.
    VM_STACK_DEPTH:  283, // () -> int — current operand-stack depth before this syscall's return push (diagnostic; detect SP leaks)
    FILE_READ_BIN:   286, // (handle, arr, count) -> elements_read (-1=err) — read up-to count int32 elems as 4-byte LE binary; works for both int[] and float[]
    FILE_WRITE_BIN:  287, // (handle, arr, count) -> elements_written (-1=err) — write count int32 elems as 4-byte LE binary; works for both int[] and float[]
};

export const SyscallName = {};
for (const [name, id] of Object.entries(Syscall)) {
    SyscallName[id] = name;
}

// Binary format constants
export const MAGIC = 0x54434300; // "TCC\0"
export const VERSION = 6;       // V6: self-describing header (header_size + total_size) + syscall ABI revision
// Syscall ABI generation — BUMP IN LOCKSTEP with the firmware's TC_SYSCALL_ABI
// whenever syscall NUMBERS are inserted/renumbered (pure appends do NOT need a
// bump — old bytecode still calls the same numbers). The loader warns (but still
// loads) when a .tcb's abi_rev disagrees with the firmware it runs on.
export const SYSCALL_ABI = 1;

// Release version — bump on any compiler/VM/syscall change
export const TINYC_RELEASE = "1.3.23"; // RGB LED via RMT: new builtin 'rgbLed(gpio, 0xRRGGBB)' (SYS_RGB_LED=405) drives a single WS2812/SK6812 addressable LED (the onboard RGB LED on most ESP32-C3/C6/S3 dev boards) straight off the RMT peripheral — no USE_LIGHT, no GPIO template, no NeoPixelBus. Bit timing at 0.1us/tick (0=0.3/0.9us, 1=0.9/0.3us) + 50us reset; colour 0xRRGGBB sent in WS2812 G,R,B order. Mirrors the ESP-IDF RMT WS2812 example. Distinct from the existing setPixels (which drives a strip through Tasmota's light stack). Prior: Matter command callback: 'MatterInvoke(ep, cluster, cmd)' added to CALLBACK_NAMES so a .tc script can handle controller-invoked commands (the Matter twin of HomeKitWrite). The firmware routes an OnOff/app-cluster Invoke to MatterInvoke on every slot that defines it; if no script handles it, the built-in OnOff->relay default applies (no double-toggle). Device-verified on a C6: OnOff.Toggle -> MatterInvoke -> relay flip. Prior: Matter data-model scripting syscalls (398-404): matterAdd / matterCluster / matterAttr / matterSet / matterGet / matterStart / matterReset. A .tc script declares the Matter device (endpoints/clusters/attributes) on the firmware-agnostic matter_c engine and publishes attribute values; the IM Read/Subscribe paths + the report engine serve them. ESP32-only via USE_MATTER_C (replaces USE_HOMEKIT in the shared TinyC integration slot); non-Matter builds pop args + return -1/0. Predefined script constants: device types MATTER_PLUG/MATTER_ONOFF_LIGHT/MATTER_DIMM_LIGHT/MATTER_TEMP_SENSOR/MATTER_HUM_SENSOR, clusters CLUSTER_ONOFF/CLUSTER_LEVEL/CLUSTER_TEMP/CLUSTER_HUM/CLUSTER_POWER/CLUSTER_ENERGY, attr types MTR_BOOL/MTR_U8/MTR_U16/MTR_U32/MTR_U64/MTR_ENUM8. Prior: Symmetric crypto syscalls (360-365): aesEcb / aesCbc (AES-128, in-place on TinyC char[] buffers), hmacSha256, sha256, plus hex2bin / bin2hex byte-twiddling helpers. ESP32-only via mbedtls (already linked for HTTPS/MQTT-TLS); ESP8266 returns 0/no-op. Motivating use case: TinyC scripts speaking the Tuya local protocol (v3.3 = AES-128-ECB) so users can drive Smart-Life-controlled devices (pool heat pumps, plugs, switches, dehumidifiers) directly from Tasmota without a cloud round-trip or a bridge. Also enables custom signed REST APIs (HMAC-SHA256), encrypted SML decoders not covered by AmsLib, and per-device MQTT-TLS fingerprinting. Buffers follow TinyC convention (one byte per int32 slot, low 8 bits used); lengths in bytes. AES-CBC stack-allocates up to 4 KB per call, falls back to malloc above; HMAC/SHA bounded at 1024 B key / 4 KB data. AES key fixed at 128 bits (Tuya/most Smart-Life devices). Tuya v3.4 (ECDH+AES-GCM) not exposed — most Smart-Life devices still use v3.3.

