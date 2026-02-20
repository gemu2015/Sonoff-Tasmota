/*
  xdrv_124_tinyc_vm.h - TinyC Bytecode VM engine (header-only)

  Separated into .h to avoid Arduino IDE auto-prototype generation issues.
  Included by xdrv_124_tinyc.ino
*/

#ifndef _XDRV_124_TINYC_VM_H_
#define _XDRV_124_TINYC_VM_H_

#ifdef USE_UFILESYS
extern FS *ffsp;
extern FS *ufsp;
#endif

/*********************************************************************************************\
 * VM Configuration — ESP8266 vs ESP32
 *
 * ESP8266: ~30-35KB heap, 4KB stack → keep struct under ~4KB
 * ESP32:   ~150KB+ heap, 8KB stack → can afford larger arrays
\*********************************************************************************************/

#ifdef ESP8266
  #define TC_MAX_PROGRAM     4096    // max bytecode size
  #define TC_STACK_SIZE      64      // operand stack (256 bytes)
  #define TC_MAX_FRAMES      8       // call depth — frames are small (locals allocated dynamically)
  #define TC_MAX_LOCALS      256     // locals per frame (1KB, dynamically allocated)
  #define TC_MAX_GLOBALS     64      // global slots (256 bytes)
  #define TC_MAX_CONSTANTS   32      // constant pool entries
  #define TC_MAX_CONST_DATA  512     // string constant bytes
  #define TC_INSTR_PER_TICK  500     // instructions per 50ms tick
  #define TC_OUTPUT_SIZE     128     // output buffer for MQTT
#else  // ESP32
  #define TC_MAX_PROGRAM     16384   // max bytecode size
  #define TC_STACK_SIZE      256     // operand stack (1KB)
  #define TC_MAX_FRAMES      32      // call depth
  #define TC_MAX_LOCALS      256     // locals per frame (1KB) - enough for char arrays
  #define TC_MAX_GLOBALS     256     // global slots (1KB)
  #define TC_MAX_CONSTANTS   128     // constant pool entries
  #define TC_MAX_CONST_DATA  4096    // string constant bytes
  #define TC_INSTR_PER_TICK  1000    // instructions per 50ms tick
  #define TC_OUTPUT_SIZE     512     // output buffer for MQTT
#endif

#define TC_MAX_FILE_HANDLES  4      // max simultaneously open files

// Heap memory for large arrays (> 255 elements)
#ifdef ESP8266
  #define TC_MAX_HEAP           2048   // heap slots (8KB)
  #define TC_MAX_HEAP_HANDLES   8
#else  // ESP32
  #define TC_MAX_HEAP           8192   // heap slots (32KB)
  #define TC_MAX_HEAP_HANDLES   16
#endif

#define TC_MAGIC           0x54434300  // "TCC\0"
#define TC_VERSION         3           // V3: added function table section (callbacks)
#define TC_FILE_NAME       "/autoexec.tcb"

// Flash-safe byte read — enables execute-from-flash on ESP32
// When USE_TINYC_FLASH_EXEC is defined, bytecode can reside in memory-mapped flash.
// pgm_read_byte() handles the aligned 32-bit read + byte extraction required by Xtensa.
// Without it, direct byte access to flash causes LoadStoreAlignment exceptions.
#ifdef USE_TINYC_FLASH_EXEC
  #define TC_READ_BYTE(ptr)  pgm_read_byte(ptr)
#else
  #define TC_READ_BYTE(ptr)  (*(ptr))
#endif
// Flash-safe memcpy (for copying string constants from binary)
#ifdef USE_TINYC_FLASH_EXEC
  static inline void tc_memcpy_flash(void *dst, const uint8_t *src, uint16_t len) {
    uint8_t *d = (uint8_t *)dst;
    for (uint16_t i = 0; i < len; i++) d[i] = pgm_read_byte(&src[i]);
  }
  #define TC_MEMCPY(dst, src, len) tc_memcpy_flash(dst, src, len)
#else
  #define TC_MEMCPY(dst, src, len) memcpy(dst, src, len)
#endif

// Callback support
#define TC_MAX_CALLBACKS  10           // max well-known callback functions
#ifdef ESP8266
  #define TC_CALLBACK_MAX_INSTR 20000  // instruction limit per callback (ESP8266)
#else
  #define TC_CALLBACK_MAX_INSTR 200000 // instruction limit per callback (ESP32)
#endif
#define TC_CALLBACK_NAME_MAX 16        // max callback name length

// UDP multicast support (Scripter-compatible protocol)
#define TC_UDP_PORT          1999
#define TC_UDP_MAX_VARS      8          // max tracked UDP variable names
#define TC_UDP_VAR_NAME_MAX  16         // max variable name length
#define TC_UDP_BUF_SIZE      320        // receive buffer (max: 2+16+1+2+64*4 = 277)
#define TC_UDP_MAX_ARRAY     64         // max float array elements per UDP variable

/*********************************************************************************************\
 * VM Opcodes
\*********************************************************************************************/

enum TcOp {
  OP_NOP          = 0x00, OP_HALT         = 0x01,
  OP_PUSH_I32     = 0x02, OP_PUSH_F32     = 0x03,
  OP_PUSH_I8      = 0x04, OP_PUSH_I16     = 0x05,
  OP_POP          = 0x06, OP_DUP          = 0x07,
  // Integer arithmetic
  OP_ADD          = 0x10, OP_SUB          = 0x11,
  OP_MUL          = 0x12, OP_DIV          = 0x13,
  OP_MOD          = 0x14, OP_NEG          = 0x15,
  // Float arithmetic
  OP_FADD         = 0x18, OP_FSUB         = 0x19,
  OP_FMUL         = 0x1A, OP_FDIV         = 0x1B,
  OP_FNEG         = 0x1C,
  // Bitwise
  OP_BIT_AND      = 0x20, OP_BIT_OR       = 0x21,
  OP_BIT_XOR      = 0x22, OP_BIT_NOT      = 0x23,
  OP_SHL          = 0x24, OP_SHR          = 0x25,
  // Integer comparison
  OP_EQ           = 0x30, OP_NEQ          = 0x31,
  OP_LT           = 0x32, OP_GT           = 0x33,
  OP_LTE          = 0x34, OP_GTE          = 0x35,
  // Float comparison
  OP_FEQ          = 0x36, OP_FNEQ         = 0x37,
  OP_FLT          = 0x38, OP_FGT          = 0x39,
  OP_FLTE         = 0x3A, OP_FGTE         = 0x3B,
  // Logical
  OP_LOGIC_AND    = 0x40, OP_LOGIC_OR     = 0x41,
  OP_LOGIC_NOT    = 0x42,
  // Control flow
  OP_JMP          = 0x50, OP_JZ           = 0x51,
  OP_JNZ          = 0x52, OP_CALL         = 0x53,
  OP_RET          = 0x54, OP_RET_VAL      = 0x55,
  // Variables
  OP_LOAD_LOCAL   = 0x60, OP_STORE_LOCAL  = 0x61,
  OP_LOAD_GLOBAL  = 0x62, OP_STORE_GLOBAL = 0x63,
  // Arrays
  OP_LOAD_LOCAL_ARR  = 0x68, OP_STORE_LOCAL_ARR = 0x69,
  OP_LOAD_GLOBAL_ARR = 0x6A, OP_STORE_GLOBAL_ARR= 0x6B,
  // Type conversion
  OP_I2F          = 0x70, OP_F2I          = 0x71,
  OP_I2C          = 0x72,
  // Array address (push ref for string functions)
  OP_ADDR_LOCAL   = 0x78,  // push packed ref: (fp << 16) | base_idx
  OP_ADDR_GLOBAL  = 0x79,  // push packed ref: 0x80000000 | base_idx
  // Syscalls
  OP_SYSCALL      = 0x80,
  // Heap arrays (large arrays > 255 elements)
  OP_LOAD_HEAP_ARR  = 0xA0,  // u8 handle; pop idx -> push value
  OP_STORE_HEAP_ARR = 0xA1,  // u8 handle; pop val, pop idx -> store
  OP_ADDR_HEAP      = 0xA2,  // u8 handle -> push ref: 0xC0000000 | handle
  // Constants
  OP_LOAD_CONST   = 0x90,
};

/*********************************************************************************************\
 * Syscall IDs
\*********************************************************************************************/

enum TcSyscall {
  // GPIO
  SYS_PIN_MODE        = 0,  SYS_DIGITAL_WRITE   = 1,
  SYS_DIGITAL_READ    = 2,  SYS_ANALOG_READ     = 3,
  SYS_ANALOG_WRITE    = 4,  SYS_GPIO_INIT       = 5,
  // Timing
  SYS_DELAY           = 10, SYS_DELAY_MICRO     = 11,
  SYS_MILLIS          = 12, SYS_MICROS          = 13,
  SYS_TIMER_START     = 14, SYS_TIMER_DONE      = 15,
  SYS_TIMER_STOP      = 16, SYS_TIMER_REMAINING = 17,
  // Serial / Output
  SYS_SERIAL_BEGIN    = 20, SYS_SERIAL_PRINT     = 21,
  SYS_SERIAL_PRINT_INT= 22, SYS_SERIAL_PRINT_FLT = 23,
  SYS_SERIAL_PRINTLN  = 24, SYS_SERIAL_READ      = 25,
  SYS_SERIAL_AVAILABLE= 26,
  // Math
  SYS_MATH_ABS  = 30, SYS_MATH_MIN  = 31, SYS_MATH_MAX  = 32,
  SYS_MATH_MAP  = 33, SYS_MATH_RANDOM= 34, SYS_MATH_SQRT = 35,
  SYS_MATH_SIN  = 36, SYS_MATH_COS   = 37,
  // String operations (work with array refs from OP_ADDR_LOCAL/OP_ADDR_GLOBAL)
  SYS_STRLEN       = 50,  // (ref) -> int
  SYS_STRCPY       = 51,  // (dst_ref, src_ref) -> void
  SYS_STRCAT       = 52,  // (dst_ref, src_ref) -> void
  SYS_STRCMP        = 53,  // (ref_a, ref_b) -> int
  SYS_STR_PRINT    = 54,  // (ref) -> void  (print char array to output)
  SYS_STRCPY_CONST = 55,  // (dst_ref, const_idx) -> void  (copy string literal into array)
  SYS_STRCAT_CONST = 56,  // (dst_ref, const_idx) -> void  (append string literal to array)
  SYS_SPRINTF_INT  = 57,  // (dst_ref, fmt_const_idx, int_val) -> int chars written
  SYS_SPRINTF_FLT  = 58,  // (dst_ref, fmt_const_idx, float_val) -> int chars written
  SYS_SPRINTF_STR  = 59,  // (dst_ref, fmt_const_idx, src_ref) -> int chars written
  // sprintf append variants — same args, but append to existing string in dst
  SYS_SPRINTF_INT_CAT = 70, // (dst_ref, fmt_const_idx, int_val) -> total len
  SYS_SPRINTF_FLT_CAT = 71, // (dst_ref, fmt_const_idx, float_val) -> total len
  SYS_SPRINTF_STR_CAT = 72, // (dst_ref, fmt_const_idx, src_ref) -> total len
  // Tasmota-specific
  SYS_MQTT_PUBLISH = 40,  // publish output buffer
  SYS_GET_POWER    = 41,  // get relay state
  SYS_SET_POWER    = 42,  // set relay state
  SYS_TASM_CMD     = 43,  // (const_idx_cmd, buf_ref) -> response length
  // File I/O (LittleFS)
  SYS_FILE_OPEN    = 60,  // (const_idx_path, mode) -> handle (-1=err)
  SYS_FILE_CLOSE   = 61,  // (handle) -> 0/-1
  SYS_FILE_READ    = 62,  // (handle, buf_ref, maxBytes) -> bytes_read/-1
  SYS_FILE_WRITE   = 63,  // (handle, buf_ref, len) -> bytes_written/-1
  SYS_FILE_EXISTS  = 64,  // (const_idx_path) -> 1/0
  SYS_FILE_DELETE  = 65,  // (const_idx_path) -> 0/-1
  SYS_FILE_SIZE    = 66,  // (const_idx_path) -> bytes/-1
  // Heap allocation
  SYS_HEAP_ALLOC   = 80,  // pop size -> push handle (-1 on fail)
  SYS_HEAP_FREE    = 81,  // pop handle -> void
  // Tasmota output (for callbacks — route to Tasmota APIs)
  SYS_RESPONSE_APPEND     = 90, // (char_ref) -> void — ResponseAppend_P
  SYS_WEB_SEND            = 91, // (char_ref) -> void — WSContentSend_PD
  SYS_WEB_FLUSH           = 92, // () -> void — WSContentFlush
  SYS_RESPONSE_APPEND_STR = 93, // (const_idx) -> void — string literal variant
  SYS_WEB_SEND_STR        = 94, // (const_idx) -> void — string literal variant
  SYS_LOG                 = 95, // (char_ref) -> void — AddLog to Tasmota console
  SYS_LOG_STR             = 96, // (const_idx) -> void — AddLog string literal
  // UDP multicast (Scripter-compatible, 239.255.255.250:1999)
  SYS_UDP_SEND            = 100, // (const_idx_name, float_val) -> void — binary float
  SYS_UDP_RECV            = 101, // (const_idx_name) -> float — last received value
  SYS_UDP_READY           = 102, // (const_idx_name) -> int — 1 if new value available
  SYS_UDP_SEND_ARRAY      = 103, // (const_idx_name, arr_ref, count) -> void — float array
  SYS_UDP_RECV_ARRAY      = 104, // (const_idx_name, arr_ref, maxcount) -> int — recv array
  // I2C bus (last param = bus: 0 or 1)
  SYS_I2C_READ8           = 105, // (addr, reg, bus) -> int — read byte
  SYS_I2C_WRITE8          = 106, // (addr, reg, val, bus) -> int — write byte, 1=ok
  SYS_I2C_READ_BUF        = 107, // (addr, reg, buf_ref, len, bus) -> int — read into char[]
  SYS_I2C_WRITE_BUF       = 108, // (addr, reg, buf_ref, len, bus) -> int — write from char[]
  SYS_I2C_EXISTS           = 109, // (addr, bus) -> int — 1 if device on bus
  SYS_I2C_READ_BUF0       = 112, // (addr, buf_ref, len, bus) -> int — read without register
  SYS_I2C_WRITE0          = 113, // (addr, reg, bus) -> int — write register only (no data)
  // Smart Meter (SML)
  SYS_SML_GET             = 110, // (index) -> float — meter value (1-based, 0=count)
  SYS_SML_GETSTR          = 111, // (index, buf_ref) -> int — meter ID string into buf
  SYS_SML_WRITE           = 114, // (meter, buf_ref) -> int — send hex string to meter
  SYS_SML_READ            = 115, // (meter, buf_ref) -> int — read raw buffer into char[]
  SYS_SML_SETBAUD         = 116, // (meter, baud) -> int — change baud rate
  SYS_SML_SETWSTR         = 117, // (meter, buf_ref) -> int — set async write string
  SYS_SML_SETOPT          = 118, // (options) -> int — set SML global options
  SYS_SML_GETV            = 119, // (sel) -> int — get/reset data valid flags
  SYS_SML_WRITE_STR       = 124, // (meter, const_idx) -> int — send string literal to meter
  SYS_SML_SETWSTR_STR     = 125, // (meter, const_idx) -> int — set async write from string literal
  // SPI bus
  SYS_SPI_INIT            = 120, // (sclk, mosi, miso, speed_mhz) -> int (1=ok)
  SYS_SPI_SET_CS          = 121, // (index, pin) -> void
  SYS_SPI_TRANSFER        = 122, // (cs, buf_ref, len, mode) -> int bytes transferred
  // String manipulation
  SYS_STR_TOKEN       = 74,  // (dst_ref, src_ref, delim_char, n) -> int
  SYS_STR_SUB         = 75,  // (dst_ref, src_ref, pos, len) -> int
  SYS_STR_FIND        = 76,  // (haystack_ref, needle_ref) -> int (-1=not found)
  // Tasmota system variables (virtual — accessed as tasm_xxx in TinyC)
  SYS_TASM_GET        = 130, // (index) -> int/float — read Tasmota variable
  SYS_TASM_SET        = 131, // (index, value) -> void — write Tasmota variable
  // Sensor JSON parsing
  SYS_SENSOR_GET      = 132, // (const_idx_path) -> float — read sensor by JSON path
  // HTTP
  SYS_HTTP_GET        = 140, // (url_ref, response_ref) -> int length
  SYS_HTTP_POST       = 141, // (url_ref, data_ref, response_ref) -> int length
  SYS_HTTP_HEADER     = 142, // (name_ref, value_ref) -> void
  // WebUI widgets (generate HTML for /tc_ui page)
  SYS_WEB_BUTTON      = 150, // (gref, label_const) -> void
  SYS_WEB_SLIDER      = 151, // (gref, min, max, label_const) -> void
  SYS_WEB_CHECKBOX    = 152, // (gref, label_const) -> void
  SYS_WEB_TEXT        = 153, // (gref, maxlen, label_const) -> void
  SYS_WEB_NUMBER      = 154, // (gref, min, max, label_const) -> void
  SYS_WEB_PULLDOWN    = 155, // (gref, opts_const) -> void
  SYS_WEB_RADIO       = 156, // (gref, opts_const) -> void
  SYS_WEB_TIME        = 157, // (gref, label_const) -> void
  SYS_WEB_PAGE_LABEL  = 158, // (page_num, label_const) -> void — register page with button label
  SYS_WEB_PAGE        = 159, // () -> int — returns current page number being rendered
  SYS_WEB_SEND_FILE   = 160, // (filename_const) -> void — send file contents to web page
  SYS_WEB_ON          = 161, // (handler_num, url_const) -> void — register custom web endpoint
  SYS_WEB_HANDLER     = 162, // () -> int — returns current web handler number (in WebOn callback)
  SYS_WEB_ARG         = 163, // (name_const, buf_ref) -> int — get HTTP arg into buffer, returns length
  SYS_MDNS            = 164, // (name_const, mac_const, type_const) -> int — register mDNS service
  // Display drawing (direct renderer calls — requires USE_DISPLAY)
  SYS_DSP_TEXT        = 170, // (buf_ref) -> void — raw DisplayText command string
  SYS_DSP_CLEAR       = 171, // () -> void — clear display
  SYS_DSP_POS         = 172, // (x, y) -> void — set draw position
  SYS_DSP_FONT        = 173, // (f) -> void — set font (0-7)
  SYS_DSP_SIZE        = 174, // (s) -> void — set text size
  SYS_DSP_COLOR       = 175, // (fg, bg) -> void — set fg/bg color (16-bit 565)
  SYS_DSP_DRAW        = 176, // (buf_ref) -> void — draw string at current pos
  SYS_DSP_PIXEL       = 177, // (x, y) -> void — draw pixel
  SYS_DSP_LINE        = 178, // (x1, y1) -> void — draw line from pos to (x1,y1)
  SYS_DSP_RECT        = 179, // (w, h) -> void — draw rectangle at pos
  SYS_DSP_FILL_RECT   = 180, // (w, h) -> void — draw filled rectangle at pos
  SYS_DSP_CIRCLE      = 181, // (r) -> void — draw circle at pos
  SYS_DSP_FILL_CIRCLE = 182, // (r) -> void — draw filled circle at pos
  SYS_DSP_HLINE       = 183, // (w) -> void — horizontal line from pos
  SYS_DSP_VLINE       = 184, // (h) -> void — vertical line from pos
  SYS_DSP_ROUND_RECT  = 185, // (w, h, r) -> void — rounded rectangle
  SYS_DSP_FILL_RRECT  = 186, // (w, h, r) -> void — filled rounded rectangle
  SYS_DSP_TRIANGLE    = 187, // (x1, y1, x2, y2) -> void — triangle from pos
  SYS_DSP_FILL_TRI    = 188, // (x1, y1, x2, y2) -> void — filled triangle
  SYS_DSP_DIM         = 189, // (val) -> void — set brightness (0-15)
  SYS_DSP_ONOFF       = 190, // (on) -> void — display on/off
  SYS_DSP_UPDATE      = 191, // () -> void — update display (e-paper refresh)
  SYS_DSP_PICTURE     = 192, // (filename_const, scale) -> void — draw image file at pos
  SYS_DSP_WIDTH       = 193, // () -> int — display width in pixels
  SYS_DSP_HEIGHT      = 194, // () -> int — display height in pixels
  SYS_DSP_TEXT_STR    = 195, // (const_idx) -> void — DisplayText from string literal
  SYS_DSP_DRAW_STR    = 196, // (const_idx) -> void — draw string literal at current pos
  SYS_DSP_PAD         = 197, // (n) -> void — set text padding for dspDraw (0=off)
  // Audio
  SYS_AUDIO_VOL       = 200, // (vol) -> void — set volume 0-100
  SYS_AUDIO_PLAY      = 201, // (file_const) -> void — play MP3 file
  SYS_AUDIO_SAY       = 202, // (text_const) -> void — text-to-speech
  // Debug
  SYS_DEBUG_PRINT     = 250, SYS_DEBUG_PRINT_STR = 251,
  SYS_DEBUG_DUMP      = 252,
};

/*********************************************************************************************\
 * VM Error codes
\*********************************************************************************************/

enum {
  TC_OK = 0,
  TC_ERR_STACK_OVERFLOW, TC_ERR_STACK_UNDERFLOW,
  TC_ERR_FRAME_OVERFLOW, TC_ERR_DIV_ZERO,
  TC_ERR_BAD_OPCODE,     TC_ERR_BAD_SYSCALL,
  TC_ERR_BAD_BINARY,     TC_ERR_INSTRUCTION_LIMIT,
  TC_ERR_BOUNDS,         TC_ERR_PAUSED,
};

// Error strings in PROGMEM — saves ~120 bytes RAM on ESP8266
static const char TC_ERR_00[] PROGMEM = "OK";
static const char TC_ERR_01[] PROGMEM = "Stack overflow";
static const char TC_ERR_02[] PROGMEM = "Stack underflow";
static const char TC_ERR_03[] PROGMEM = "Call stack overflow";
static const char TC_ERR_04[] PROGMEM = "Division by zero";
static const char TC_ERR_05[] PROGMEM = "Unknown opcode";
static const char TC_ERR_06[] PROGMEM = "Unknown syscall";
static const char TC_ERR_07[] PROGMEM = "Invalid binary";
static const char TC_ERR_08[] PROGMEM = "Instruction limit";
static const char TC_ERR_09[] PROGMEM = "Bounds error";
static const char TC_ERR_10[] PROGMEM = "Paused (delay)";
static const char TC_ERR_XX[] PROGMEM = "Unknown";

static const char * const tc_error_table[] PROGMEM = {
  TC_ERR_00, TC_ERR_01, TC_ERR_02, TC_ERR_03, TC_ERR_04,
  TC_ERR_05, TC_ERR_06, TC_ERR_07, TC_ERR_08, TC_ERR_09, TC_ERR_10
};

static const char* tc_error_str(int err) {
  static char buf[24];
  const char *p;
  if (err >= 0 && err <= TC_ERR_PAUSED) {
    p = (const char *)pgm_read_ptr(&tc_error_table[err]);
  } else {
    p = TC_ERR_XX;
  }
  strncpy_P(buf, p, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  return buf;
}

/*********************************************************************************************\
 * VM Data structures
\*********************************************************************************************/

typedef struct {
  uint16_t return_pc;
  int32_t  *locals;     // dynamically allocated — TC_MAX_LOCALS int32_t's per frame
} TcFrame;

typedef struct {
  uint8_t  type;    // 1=string, 2=float
  union {
    struct { const char *ptr; uint16_t len; } str;
    float f;
  };
} TcConstant;

typedef struct {
  uint16_t offset;   // start offset in heap_data[]
  uint16_t size;     // number of int32 slots
  bool     alive;    // true if block is in use
} TcHeapHandle;

typedef struct {
  char     name[TC_CALLBACK_NAME_MAX];  // e.g. "JsonCall", "WebCall", "EverySecond"
  uint16_t address;                      // code-relative address
} TcCallback;

typedef struct {
  // Program
  const uint8_t *code;
  uint16_t code_size;
  uint16_t code_offset;
  // Execution
  uint16_t pc;
  bool     running;
  bool     halted;
  int      error;
  uint32_t instruction_count;
  // Delay support (non-blocking)
  uint32_t delay_until;    // millis() target for current delay
  bool     delayed;        // VM is waiting for delay
  // Software timers (millis-based)
#define TC_MAX_TIMERS 4
  uint32_t timer_deadline[TC_MAX_TIMERS];
  bool     timer_active[TC_MAX_TIMERS];
  // Stack
  int32_t  stack[TC_STACK_SIZE];
  uint16_t sp;
  // Globals
  int32_t  globals[TC_MAX_GLOBALS];
  // Frames
  TcFrame  frames[TC_MAX_FRAMES];
  uint8_t  fp;
  uint8_t  frame_count;
  // Constants
  TcConstant constants[TC_MAX_CONSTANTS];
  uint8_t  const_count;
  char     const_data[TC_MAX_CONST_DATA];
  uint16_t const_data_used;
  // Heap (for large arrays > 255 elements)
  int32_t      *heap_data;       // malloc'd on demand, NULL if no heap used
  uint16_t      heap_used;       // bump allocator: next free slot
  TcHeapHandle  heap_handles[TC_MAX_HEAP_HANDLES];
  uint8_t       heap_handle_count;
  // Callback function table (V3)
  TcCallback    callbacks[TC_MAX_CALLBACKS];
  uint8_t       callback_count;
} TcVM;

/*********************************************************************************************\
 * UDP multicast variable entry
\*********************************************************************************************/

typedef struct {
  char     name[TC_UDP_VAR_NAME_MAX];
  float    value;
  bool     ready;     // true if updated since last udpReady() check
  bool     used;
  // Array support (Scripter-compatible binary array protocol)
  float   *arr_data;  // malloc'd on first array receive, NULL if scalar
  uint16_t arr_count;  // number of elements in arr_data
} TcUdpVar;

/*********************************************************************************************\
 * SPI bus state
\*********************************************************************************************/

#define TC_SPI_MAX_CS  4

typedef struct {
  int8_t    sclk;            // clock pin (-1 = HW SPI bus 1, -2 = HW SPI bus 2, >=0 = bitbang)
  int8_t    mosi;            // MOSI pin (-1 = not used for bitbang)
  int8_t    miso;            // MISO pin (-1 = not used for bitbang)
  int8_t    cs[TC_SPI_MAX_CS]; // chip select pins (-1 = unused)
  bool      initialized;
#ifdef ESP32
  SPIClass *spip;            // SPI instance (hardware mode only)
  SPISettings settings;
#endif
#ifdef ESP8266
  SPIClass *spip;
  SPISettings settings;
#endif
} TcSpi;

/*********************************************************************************************\
 * Driver state
\*********************************************************************************************/

struct TINYC {
  TcVM     vm;
  uint8_t *program;
  uint32_t program_size;
  bool     loaded;
  bool     running;
  bool     autorun;
  uint32_t instr_per_tick;
  // Output buffer for MQTT
  char     output[TC_OUTPUT_SIZE];
  uint16_t output_len;
  // Upload state
  uint8_t *upload_buf;
  uint32_t upload_size;
  uint32_t upload_received;
  char     upload_filename[32];  // filename from upload (e.g. "bresser.tcb")
  // File I/O state (File objects are stored separately as statics — see below)
  bool     file_used[TC_MAX_FILE_HANDLES];
  // UDP multicast (Scripter-compatible, 239.255.255.250:1999)
  bool     udp_used;            // true if any udp* function was called
  bool     udp_connected;       // multicast socket active
  TcUdpVar udp_vars[TC_UDP_MAX_VARS];
  char     udp_last_name[TC_UDP_VAR_NAME_MAX]; // name of last received var (for UdpCall)
#if !defined(USE_SCRIPT) || !defined(USE_SCRIPT_GLOBVARS)
  // Standalone UDP socket (when Scripter is not present)
  WiFiUDP  udp;
  char     udp_buf[TC_UDP_BUF_SIZE];
#endif
  // WebUI pages (up to 6, set by wLabel(), buttons on main page)
#define TC_MAX_WEB_PAGES 6
  char     page_label[TC_MAX_WEB_PAGES][32];
  uint8_t  page_count;     // number of registered pages
  uint8_t  current_page;   // current page being rendered (for wPage())
  // Custom web handlers (webOn)
#define TC_MAX_WEB_HANDLERS 4
  char     web_handler_url[TC_MAX_WEB_HANDLERS][32];
  uint8_t  web_handler_count;
  uint8_t  current_web_handler;  // handler number during WebOn callback
  // SPI bus
  TcSpi    spi;
  // HTTP request state
#define TC_HTTP_MAX_HEADERS 4
  char     http_hdr_name[TC_HTTP_MAX_HEADERS][64];
  char     http_hdr_value[TC_HTTP_MAX_HEADERS][64];
  uint8_t  http_hdr_count;
#ifdef ESP32
  // FreeRTOS task for VM execution (main() and TaskLoop)
  TaskHandle_t task_handle;
  volatile bool task_running;   // task loop is active
  volatile bool task_stop;      // signal task to stop
  SemaphoreHandle_t vm_mutex;   // serialize VM access between task and main thread
#endif
} *Tinyc = nullptr;

// File handles stored as statics (not in calloc'd struct) so C++ File constructor runs properly
static File tc_file_handles[TC_MAX_FILE_HANDLES];

/*********************************************************************************************\
 * Helper: reinterpret int32 <-> float
\*********************************************************************************************/

static inline float i2f(int32_t i) {
  union { int32_t i; float f; } u; u.i = i; return u.f;
}
static inline int32_t f2i(float f) {
  union { int32_t i; float f; } u; u.f = f; return u.i;
}

/*********************************************************************************************\
 * VM: Read helpers (big-endian bytecode)
\*********************************************************************************************/

static inline uint8_t tc_read_u8(TcVM *vm) { return TC_READ_BYTE(&vm->code[vm->pc++]); }
static inline int8_t  tc_read_i8(TcVM *vm) { return (int8_t)TC_READ_BYTE(&vm->code[vm->pc++]); }
static inline uint16_t tc_read_u16(TcVM *vm) {
  uint16_t v = ((uint16_t)TC_READ_BYTE(&vm->code[vm->pc]) << 8) | TC_READ_BYTE(&vm->code[vm->pc + 1]);
  vm->pc += 2; return v;
}
static inline int32_t tc_read_i32(TcVM *vm) {
  int32_t v = ((int32_t)TC_READ_BYTE(&vm->code[vm->pc]) << 24) | ((int32_t)TC_READ_BYTE(&vm->code[vm->pc+1]) << 16) |
              ((int32_t)TC_READ_BYTE(&vm->code[vm->pc+2]) << 8) | TC_READ_BYTE(&vm->code[vm->pc+3]);
  vm->pc += 4; return v;
}
static inline float tc_read_f32(TcVM *vm) { return i2f(tc_read_i32(vm)); }

/*********************************************************************************************\
 * VM: Array ref encoding for string functions
 * Local ref:  (fp << 16) | base_index   (bit 31 = 0)
 * Global ref: 0x80000000 | base_index   (bit 31 = 1)
\*********************************************************************************************/

static inline int32_t tc_make_local_ref(uint8_t fp, uint8_t base) {
  return ((int32_t)fp << 16) | base;
}
static inline int32_t tc_make_global_ref(uint16_t base) {
  return (int32_t)0x80000000 | base;
}

// Resolve a packed array ref to a pointer into VM memory, returns NULL on error
// Ref encoding:  bits 31-30 = 00/01 → local, 10 → global, 11 → heap
static int32_t* tc_resolve_ref(TcVM *vm, int32_t ref) {
  uint32_t uref = (uint32_t)ref;
  uint8_t tag = uref >> 30;
  if (tag == 3) {
    // Heap ref: 0xC0000000 | handle
    uint16_t handle = uref & 0xFFFF;
    if (handle < TC_MAX_HEAP_HANDLES && vm->heap_data &&
        vm->heap_handles[handle].alive) {
      return &vm->heap_data[vm->heap_handles[handle].offset];
    }
    return nullptr;
  }
  if (tag == 2) {
    // Global ref: 0x80000000 | base_idx
    uint16_t idx = uref & 0xFFFF;
    if (idx < TC_MAX_GLOBALS) return &vm->globals[idx];
  } else {
    // Local ref: (fp << 16) | base_idx
    uint8_t fp = (uref >> 16) & 0xFF;
    uint8_t idx = uref & 0xFF;
    if (fp < TC_MAX_FRAMES && idx < TC_MAX_LOCALS && vm->frames[fp].locals) return vm->frames[fp].locals + idx;
  }
  return nullptr;
}

// How many int32 slots remain from the ref's base index to the end of the array?
static int32_t tc_ref_maxlen(TcVM *vm, int32_t ref) {
  uint32_t uref = (uint32_t)ref;
  uint8_t tag = uref >> 30;
  if (tag == 3) {
    // Heap ref
    uint16_t handle = uref & 0xFFFF;
    if (handle < TC_MAX_HEAP_HANDLES && vm->heap_handles[handle].alive) {
      return vm->heap_handles[handle].size;
    }
    return 0;
  }
  if (tag == 2) {
    uint16_t base = uref & 0xFFFF;
    return (base < TC_MAX_GLOBALS) ? TC_MAX_GLOBALS - base : 0;
  } else {
    uint8_t base = uref & 0xFF;
    return (base < TC_MAX_LOCALS) ? TC_MAX_LOCALS - base : 0;
  }
}

// Extract null-terminated C string from VM array ref into char buffer
// Returns number of chars written (excluding null terminator)
static int tc_ref_to_cstr(TcVM *vm, int32_t ref, char *out, int maxOut) {
  int32_t *buf = tc_resolve_ref(vm, ref);
  if (!buf) { out[0] = '\0'; return 0; }
  int32_t maxLen = tc_ref_maxlen(vm, ref);
  int i;
  for (i = 0; i < maxLen && i < maxOut - 1; i++) {
    if (buf[i] == 0) break;
    out[i] = (char)(buf[i] & 0xFF);
  }
  out[i] = '\0';
  return i;
}

// Stream VM string ref through a callback in chunks — no size limit
// Calls sendFn(chunk, len) for each chunk.  Uses small stack buffer.
#define TC_STREAM_CHUNK 256
typedef void (*tc_send_fn)(const char *buf, int len);

static void tc_stream_ref(TcVM *vm, int32_t ref, tc_send_fn sendFn) {
  int32_t *buf = tc_resolve_ref(vm, ref);
  if (!buf) return;
  int32_t maxLen = tc_ref_maxlen(vm, ref);
  char chunk[TC_STREAM_CHUNK];
  int ci = 0;
  for (int i = 0; i < maxLen; i++) {
    if (buf[i] == 0) break;
    chunk[ci++] = (char)(buf[i] & 0xFF);
    if (ci >= TC_STREAM_CHUNK - 1) {
      chunk[ci] = '\0';
      sendFn(chunk, ci);
      ci = 0;
    }
  }
  if (ci > 0) {
    chunk[ci] = '\0';
    sendFn(chunk, ci);
  }
}

// Stream send targets for Tasmota APIs
static void tc_send_response(const char *buf, int len) {
  ResponseAppend_P(PSTR("%s"), buf);
}
#ifdef USE_WEBSERVER
static void tc_send_web(const char *buf, int len) {
  WSContentSend(buf, len);
}
#endif

/*********************************************************************************************\
 * Smart Meter (SML) access — read meter values via SML_GetVal/SML_GetSVal
\*********************************************************************************************/

#if defined(USE_SML_M) || defined(USE_SML)
  extern double SML_GetVal(uint32_t index);
  extern char *SML_GetSVal(uint32_t index);
#ifdef USE_SML_SCRIPT_CMD
  extern uint32_t SML_Write(int32_t meter, char *hstr);
  extern uint32_t SML_Read(int32_t meter, char *str, uint32_t slen);
  extern uint32_t SML_SetBaud(uint32_t meter, uint32_t br);
  extern int32_t SML_Set_WStr(uint32_t meter, char *hstr);
  extern uint32_t SML_SetOptions(uint32_t in);
  extern uint32_t sml_getv(uint32_t sel);
#endif
#endif

/*********************************************************************************************\
 * Display renderer externs — for TinyC display drawing syscalls
\*********************************************************************************************/

#ifdef USE_DISPLAY
  #include <renderer.h>
  extern Renderer *renderer;
  extern uint16_t fg_color;
  extern uint16_t bg_color;
  extern int16_t disp_xpos;
  extern int16_t disp_ypos;
  extern void DisplayText(void);
  extern void DisplayOnOff(uint8_t on);
  extern void Draw_RGB_Bitmap(char *file, uint16_t xp, uint16_t yp, uint8_t scale, bool inverted, uint16_t xs, uint16_t ys);
  static int16_t tc_dsp_pad = 0;  // padding: 0=none, >0=left-aligned, <0=right-aligned

  // Helper: draw text with optional padding via DisplayText [pN] command
  static void tc_display_text_padded(const char *text) {
    char tbuf[256];
    if (tc_dsp_pad != 0) {
      snprintf(tbuf, sizeof(tbuf), "[p%d]%s", tc_dsp_pad, text);
    } else {
      strlcpy(tbuf, text, sizeof(tbuf));
    }
    char *savptr = XdrvMailbox.data;
    XdrvMailbox.data = tbuf;
    XdrvMailbox.data_len = strlen(tbuf);
    DisplayText();
    XdrvMailbox.data = savptr;
  }
#endif

/*********************************************************************************************\
 * UDP multicast helpers (Scripter-compatible, 239.255.255.250:1999)
 * Send: binary mode  =>name:[4 bytes float]
 * Recv: both modes   =>name=ascii  or  =>name:[4 bytes float]
 *
 * Two modes of operation:
 *   1. Scripter present (USE_SCRIPT + USE_SCRIPT_GLOBVARS):
 *      - Scripter owns the UDP socket and polls it
 *      - Scripter forwards received packets to tc_udp_on_receive()
 *      - TinyC sends via script_udp_sendvar() (Scripter's send function)
 *   2. Standalone (no Scripter):
 *      - TinyC manages its own UDP socket
 *      - tc_udp_poll() called from FUNC_LOOP
\*********************************************************************************************/

#if defined(USE_SCRIPT) && defined(USE_SCRIPT_GLOBVARS)
  // Forward declarations: Scripter's UDP functions
  extern void script_udp_sendvar(char *vname, float *fp, char *sp, uint16_t alen);
  extern void Script_udp_ensure(void);  // ensure Scripter's UDP socket is active
#endif

// Find or create a UDP variable slot by name
static TcUdpVar* tc_udp_find_var(const char *name, bool create) {
  if (!Tinyc) return nullptr;
  // Search existing
  for (int i = 0; i < TC_UDP_MAX_VARS; i++) {
    if (Tinyc->udp_vars[i].used && !strcmp(Tinyc->udp_vars[i].name, name)) {
      return &Tinyc->udp_vars[i];
    }
  }
  if (!create) return nullptr;
  // Create new slot
  for (int i = 0; i < TC_UDP_MAX_VARS; i++) {
    if (!Tinyc->udp_vars[i].used) {
      strlcpy(Tinyc->udp_vars[i].name, name, TC_UDP_VAR_NAME_MAX);
      Tinyc->udp_vars[i].value = 0;
      Tinyc->udp_vars[i].ready = false;
      Tinyc->udp_vars[i].used = true;
      Tinyc->udp_vars[i].arr_data = nullptr;
      Tinyc->udp_vars[i].arr_count = 0;
      return &Tinyc->udp_vars[i];
    }
  }
  return nullptr;  // table full
}

// Forward declaration — defined later in this file
static int tc_vm_call_callback(TcVM *vm, const char *name);

// Check if a named callback exists in the loaded program
static bool tc_has_callback(TcVM *vm, const char *name) {
  for (int i = 0; i < vm->callback_count; i++) {
    if (strcmp(vm->callbacks[i].name, name) == 0) return true;
  }
  return false;
}

// Called when a UDP variable is received (from own poll or Scripter hook)
// name: variable name, umode: '=' (ASCII) or ':' (binary), data: raw bytes after delimiter
// datalen: length of remaining data (for array detection)
void tc_udp_on_receive(const char *name, char umode, const char *data, int datalen) {
  if (!Tinyc) return;
  if (!Tinyc->udp_used) return;

  // Only update variables that TinyC already registered (via udpRecv/udpSend calls)
  // Don't create new slots — avoids filling the table with unneeded network variables
  TcUdpVar *var = tc_udp_find_var(name, false);
  if (var) {
    if (umode == '=') {
      // ASCII mode: data points to string like "23.45"
      var->value = CharToFloat((char*)data);
      // Clear any array data — this is a scalar
      var->arr_count = 0;
    } else {
      // Binary mode: either single float (4 bytes) or array (2-byte len + N*4 bytes)
      uint8_t *src = (uint8_t*)data;
      uint16_t alen = 0;
      if (datalen > 4) {
        alen = (uint16_t)src[0] | ((uint16_t)src[1] << 8);  // LE 16-bit
        // Validate: alen > 0, and remaining data after 2-byte header is exactly alen*4 bytes
        if (alen > 0 && datalen == (int)(2 + alen * sizeof(float))) {
          // Array receive
          if (alen > TC_UDP_MAX_ARRAY) alen = TC_UDP_MAX_ARRAY;
          // Allocate/resize array buffer on demand
          if (!var->arr_data || var->arr_count < alen) {
            if (var->arr_data) free(var->arr_data);
            var->arr_data = (float*)malloc(alen * sizeof(float));
          }
          if (var->arr_data) {
            var->arr_count = alen;
            uint8_t *ap = src + 2;  // skip 2-byte length
            for (uint16_t i = 0; i < alen; i++) {
              union { float f; uint8_t b[4]; } u;
              u.b[0] = ap[0]; u.b[1] = ap[1]; u.b[2] = ap[2]; u.b[3] = ap[3];
              var->arr_data[i] = u.f;
              ap += sizeof(float);
            }
            var->value = var->arr_data[0];  // first element as scalar value too
          }
        } else {
          // Not a valid array header — treat as single float
          goto single_float;
        }
      } else {
        single_float:
        // Single float: 4 bytes IEEE-754
        if (datalen >= 4) {
          union { float f; uint8_t b[4]; } u;
          u.b[0] = src[0]; u.b[1] = src[1]; u.b[2] = src[2]; u.b[3] = src[3];
          var->value = u.f;
        }
        var->arr_count = 0;
      }
    }
    var->ready = true;
  }

  // Always store name and trigger UdpCall — program can decide what to do
  strlcpy(Tinyc->udp_last_name, name, TC_UDP_VAR_NAME_MAX);

  // Trigger UdpCall callback (mutex-protected for TaskLoop concurrency)
  if (Tinyc->loaded && Tinyc->vm.halted && Tinyc->vm.error == TC_OK) {
#ifdef ESP32
    if (Tinyc->vm_mutex) xSemaphoreTake(Tinyc->vm_mutex, portMAX_DELAY);
#endif
    tc_vm_call_callback(&Tinyc->vm, "UdpCall");
#ifdef ESP32
    if (Tinyc->vm_mutex) xSemaphoreGive(Tinyc->vm_mutex);
#endif
  }
}

// Clean up SPI resources
static void tc_spi_cleanup(void) {
  if (!Tinyc) return;
  TcSpi *spi = &Tinyc->spi;
  if (!spi->initialized) return;
#ifdef ESP32
  if (spi->sclk < 0 && spi->spip) {
    spi->spip->end();
    if (spi->spip != &SPI) { delete spi->spip; }
    spi->spip = nullptr;
  }
#endif
#ifdef ESP8266
  if (spi->sclk < 0 && spi->spip) {
    spi->spip->end();
    spi->spip = nullptr;
  }
#endif
  spi->initialized = false;
  for (int i = 0; i < TC_SPI_MAX_CS; i++) { spi->cs[i] = -1; }
}

// Free all UDP variable array buffers
static void tc_udp_free_arrays(void) {
  if (!Tinyc) return;
  for (int i = 0; i < TC_UDP_MAX_VARS; i++) {
    if (Tinyc->udp_vars[i].arr_data) {
      free(Tinyc->udp_vars[i].arr_data);
      Tinyc->udp_vars[i].arr_data = nullptr;
      Tinyc->udp_vars[i].arr_count = 0;
    }
  }
}

// Send a float variable via binary multicast
static void tc_udp_send(const char *name, float value) {
#if defined(USE_SCRIPT) && defined(USE_SCRIPT_GLOBVARS)
  // Use Scripter's UDP socket — ensure it's active, then send binary
  Script_udp_ensure();
  float fv = value;
  script_udp_sendvar((char*)name, &fv, NULL, 0);
#else
  // Standalone: own UDP socket
  if (!Tinyc || !Tinyc->udp_connected) return;

  char hdr[TC_UDP_VAR_NAME_MAX + 4];   // "=>" + name + ":"
  strcpy(hdr, "=>");
  strlcat(hdr, name, sizeof(hdr) - 1);
  strlcat(hdr, ":", sizeof(hdr));

  Tinyc->udp.beginPacket(IPAddress(239, 255, 255, 250), TC_UDP_PORT);
  Tinyc->udp.write((const uint8_t*)hdr, strlen(hdr));
  Tinyc->udp.write((const uint8_t*)&value, sizeof(float));
  Tinyc->udp.endPacket();
#endif
}

// Send a float array via binary multicast: =>name:[2-byte LE count][N × 4-byte float]
static void tc_udp_send_array(const char *name, float *values, uint16_t count) {
#if defined(USE_SCRIPT) && defined(USE_SCRIPT_GLOBVARS)
  // Use Scripter's UDP socket — ensure it's active, then send binary array
  Script_udp_ensure();
  script_udp_sendvar((char*)name, values, NULL, count);
#else
  // Standalone: own UDP socket
  if (!Tinyc || !Tinyc->udp_connected) return;

  char hdr[TC_UDP_VAR_NAME_MAX + 4];
  strcpy(hdr, "=>");
  strlcat(hdr, name, sizeof(hdr) - 1);
  strlcat(hdr, ":", sizeof(hdr));

  Tinyc->udp.beginPacket(IPAddress(239, 255, 255, 250), TC_UDP_PORT);
  Tinyc->udp.write((const uint8_t*)hdr, strlen(hdr));
  // Write 2-byte LE array length
  uint8_t lenbuf[2];
  lenbuf[0] = count & 0xFF;
  lenbuf[1] = (count >> 8) & 0xFF;
  Tinyc->udp.write(lenbuf, 2);
  // Write N × 4-byte floats
  for (uint16_t i = 0; i < count; i++) {
    Tinyc->udp.write((const uint8_t*)&values[i], sizeof(float));
  }
  Tinyc->udp.endPacket();
#endif
}

// ── Standalone UDP socket management (only when Scripter is NOT present) ──
#if !defined(USE_SCRIPT) || !defined(USE_SCRIPT_GLOBVARS)

static void tc_udp_init(void) {
  if (!Tinyc) return;
  if (TasmotaGlobal.global_state.network_down) return;
  if (Tinyc->udp_connected) return;

#ifdef ESP8266
  if (Tinyc->udp.beginMulticast(WiFi.localIP(), IPAddress(239,255,255,250), TC_UDP_PORT)) {
#else
  if (Tinyc->udp.beginMulticast(IPAddress(239,255,255,250), TC_UDP_PORT)) {
#endif
    Tinyc->udp_connected = true;
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: UDP multicast started on port %d"), TC_UDP_PORT);
  } else {
    Tinyc->udp_connected = false;
    AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: UDP multicast failed"));
  }
}

static void tc_udp_stop(void) {
  if (!Tinyc) return;
  if (Tinyc->udp_connected) {
    Tinyc->udp.flush();
    Tinyc->udp.stop();
    Tinyc->udp_connected = false;
  }
  tc_udp_free_arrays();
  Tinyc->udp_used = false;
}

// Poll for incoming UDP packets — called from FUNC_LOOP (standalone only)
static void tc_udp_poll(void) {
  if (!Tinyc || !Tinyc->udp_used) return;
  if (!Tinyc->udp_connected) {
    tc_udp_init();
    return;
  }

  uint32_t timeout = millis();
  while (1) {
    uint16_t plen = Tinyc->udp.parsePacket();
    if (!plen || plen >= TC_UDP_BUF_SIZE) {
      if (plen > 0) {
        Tinyc->udp.read(Tinyc->udp_buf, TC_UDP_BUF_SIZE - 1);
        Tinyc->udp.flush();
      }
      break;
    }
    if (millis() - timeout > 100) break;  // max 100ms processing

    int32_t len = Tinyc->udp.read(Tinyc->udp_buf, TC_UDP_BUF_SIZE - 1);
    Tinyc->udp_buf[len] = 0;

    char *lp = Tinyc->udp_buf;
    if (len < 4 || lp[0] != '=' || lp[1] != '>') continue;
    lp += 2;

    // Find delimiter: '=' (ASCII) or ':' (binary)
    char *cp = lp;
    char umode = 0;
    while (*cp) {
      if (*cp == '=') { umode = '='; break; }
      if (*cp == ':') { umode = ':'; break; }
      cp++;
    }
    if (!umode) continue;

    // Extract variable name
    *cp = 0;
    char *data = cp + 1;
    int datalen = len - (data - Tinyc->udp_buf);

    // Forward to shared handler
    tc_udp_on_receive(lp, umode, data, datalen);

    optimistic_yield(100);
  }
}

#else  // Scripter is present — no own socket needed

static void tc_udp_init(void) {
  // Scripter owns the UDP socket — ensure it's active
  Script_udp_ensure();
  if (Tinyc) Tinyc->udp_connected = true;
}
static void tc_udp_stop(void) {
  if (Tinyc) {
    tc_udp_free_arrays();
    Tinyc->udp_used = false;
    Tinyc->udp_connected = false;
  }
}
static void tc_udp_poll(void) {
  // Scripter calls Script_PollUdp() which forwards to tc_udp_on_receive()
}

#endif  // USE_SCRIPT && USE_SCRIPT_GLOBVARS

/*********************************************************************************************\
 * VM: Stack macros
\*********************************************************************************************/

#define TC_PUSH(vm, val) do { \
  if ((vm)->sp >= TC_STACK_SIZE) { (vm)->error = TC_ERR_STACK_OVERFLOW; return (vm)->error; } \
  (vm)->stack[(vm)->sp++] = (val); } while(0)

#define TC_POP(vm) (((vm)->sp > 0) ? (vm)->stack[--(vm)->sp] : ((vm)->error = TC_ERR_STACK_UNDERFLOW, 0))
#define TC_PEEK(vm) ((vm)->stack[(vm)->sp - 1])
#define TC_PUSHF(vm, val) do { \
  if ((vm)->sp >= TC_STACK_SIZE) { (vm)->error = TC_ERR_STACK_OVERFLOW; return (vm)->error; } \
  (vm)->stack[(vm)->sp++] = f2i(val); } while(0)
#define TC_POPF(vm) i2f(TC_POP(vm))

/*********************************************************************************************\
 * Output: append to buffer, flush to AddLog + MQTT
\*********************************************************************************************/

static void tc_output_char(char c) {
  if (!Tinyc) return;
  if (Tinyc->output_len < TC_OUTPUT_SIZE - 1) {
    Tinyc->output[Tinyc->output_len++] = c;
    Tinyc->output[Tinyc->output_len] = '\0';
  }
  // Flush on newline or buffer full
  if (c == '\n' || Tinyc->output_len >= TC_OUTPUT_SIZE - 2) {
    // Remove trailing newline for AddLog (modify in-place, no stack copy)
    uint16_t len = Tinyc->output_len;
    if (len > 0 && Tinyc->output[len-1] == '\n') {
      Tinyc->output[len-1] = '\0';
    }
    if (Tinyc->output[0]) {
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: %s"), Tinyc->output);
    }
    Tinyc->output_len = 0;
    Tinyc->output[0] = '\0';
  }
}

static void tc_output_string(const char *s) {
  while (*s) tc_output_char(*s++);
}

static void tc_output_int(int32_t v) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", v);
  tc_output_string(buf);
}

static void tc_output_float(float v) {
  char buf[16];
  dtostrfd((double)v, 2, buf);
  tc_output_string(buf);
}

// Flush output buffer to AddLog only (safe to call from any context, including FreeRTOS task)
static void tc_output_flush(void) {
  if (!Tinyc || Tinyc->output_len == 0) return;
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: %s"), Tinyc->output);
  Tinyc->output_len = 0;
  Tinyc->output[0] = '\0';
}

// Flush output buffer AND publish to MQTT (only call from main Tasmota context, not from task)
static void tc_output_flush_mqtt(void) {
  if (!Tinyc || Tinyc->output_len == 0) return;
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: %s"), Tinyc->output);
  Response_P(PSTR("{\"TinyC\":{\"Output\":\"%s\"}}"), Tinyc->output);
  MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_TELE, PSTR("TINYC"));
  Tinyc->output_len = 0;
  Tinyc->output[0] = '\0';
}

/*********************************************************************************************\
 * Frame locals: dynamic allocation
 * Each frame's locals[] is malloc'd on OP_CALL and freed on OP_RET.
 * This saves ~2KB+ RAM on ESP8266 vs static arrays in every frame.
\*********************************************************************************************/

// Allocate locals for a frame, returns false on OOM
static bool tc_frame_alloc(TcFrame *frame) {
  frame->locals = (int32_t *)calloc(TC_MAX_LOCALS, sizeof(int32_t));
  return (frame->locals != nullptr);
}

// Free locals for a frame (safe to call if already freed)
static void tc_frame_free(TcFrame *frame) {
  if (frame->locals) {
    free(frame->locals);
    frame->locals = nullptr;
  }
}

// Free all allocated frames (call on VM stop/reset/reload)
static void tc_free_all_frames(TcVM *vm) {
  for (int i = 0; i < TC_MAX_FRAMES; i++) {
    tc_frame_free(&vm->frames[i]);
  }
  vm->frame_count = 0;
  vm->fp = 0;
}

/*********************************************************************************************\
 * Heap allocation: bump allocator with handle table
 * Used for large arrays (> 255 elements). heap_data is malloc'd on demand.
\*********************************************************************************************/

// Allocate a heap block, returns handle index or -1 on failure
static int tc_heap_alloc(TcVM *vm, uint16_t size) {
  // Lazy-allocate heap buffer
  if (!vm->heap_data) {
    vm->heap_data = (int32_t *)calloc(TC_MAX_HEAP, sizeof(int32_t));
    if (!vm->heap_data) return -1;
    vm->heap_used = 0;
  }
  // Find free handle slot
  int handle = -1;
  for (int i = 0; i < TC_MAX_HEAP_HANDLES; i++) {
    if (!vm->heap_handles[i].alive) { handle = i; break; }
  }
  if (handle < 0) return -1;
  // Check space
  if (vm->heap_used + size > TC_MAX_HEAP) return -1;
  // Bump-allocate
  vm->heap_handles[handle].offset = vm->heap_used;
  vm->heap_handles[handle].size = size;
  vm->heap_handles[handle].alive = true;
  // Zero-initialize the new block
  memset(&vm->heap_data[vm->heap_used], 0, size * sizeof(int32_t));
  vm->heap_used += size;
  if ((uint8_t)(handle + 1) > vm->heap_handle_count) vm->heap_handle_count = handle + 1;
  return handle;
}

// Mark a heap handle as dead (no compaction — bump allocator)
static void tc_heap_free_handle(TcVM *vm, int handle) {
  if (handle >= 0 && handle < TC_MAX_HEAP_HANDLES) {
    vm->heap_handles[handle].alive = false;
  }
}

// Free entire heap buffer (call on VM stop/reset/reload)
static void tc_heap_free_all(TcVM *vm) {
  if (vm->heap_data) {
    free(vm->heap_data);
    vm->heap_data = nullptr;
  }
  vm->heap_used = 0;
  vm->heap_handle_count = 0;
  memset(vm->heap_handles, 0, sizeof(vm->heap_handles));
}

/*********************************************************************************************\
 * File I/O helpers
\*********************************************************************************************/

// Close all open file handles (call on VM stop/reset)
static void tc_close_all_files(void) {
  if (!Tinyc) return;
  for (int i = 0; i < TC_MAX_FILE_HANDLES; i++) {
    if (Tinyc->file_used[i]) {
      tc_file_handles[i].close();
      Tinyc->file_used[i] = false;
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: file cleanup handle %d"), i);
    }
  }
}

// Find a free file handle slot, returns -1 if none available
static int tc_alloc_file_handle(void) {
  if (!Tinyc) return -1;
  for (int i = 0; i < TC_MAX_FILE_HANDLES; i++) {
    if (!Tinyc->file_used[i]) return i;
  }
  return -1;
}

// Get a constant string from the constant pool, returns nullptr on error
static const char* tc_get_const_str(TcVM *vm, int32_t idx) {
  if (idx >= 0 && idx < vm->const_count && vm->constants[idx].type == 1) {
    return vm->constants[idx].str.ptr;
  }
  return nullptr;
}

/*********************************************************************************************\
 * VM: sprintf helper — format into temp buf, copy to VM int32 array
 * Returns number of chars written (excluding null terminator)
\*********************************************************************************************/

static int32_t tc_sprintf_to_ref(int32_t *dst, int32_t maxSlots, const char *tmpbuf) {
  int32_t len = strlen(tmpbuf);
  if (len > maxSlots - 1) len = maxSlots - 1;
  for (int32_t i = 0; i < len; i++) {
    dst[i] = (int32_t)(uint8_t)tmpbuf[i];
  }
  dst[len] = 0;
  return len;
}

// Find null terminator in VM int32 array, returns offset (capped at maxSlots)
static int32_t tc_strlen_ref(int32_t *p, int32_t maxSlots) {
  int32_t i = 0;
  while (i < maxSlots && p[i] != 0) i++;
  return i;
}

// Format a float using dtostrfd() (Arduino-safe, no %f dependency).
// Handles format strings like "%.2f", "%f", "Value: %.3f units"
// Finds the first %[.N]f specifier, replaces it with dtostrfd output,
// copies any prefix/suffix text around it.
static int32_t tc_sprintf_float(char *out, int outSize, const char *fmt, float fval) {
  // Find the '%' that starts a float format specifier
  const char *p = fmt;
  const char *pct = nullptr;
  while (*p) {
    if (*p == '%') {
      if (*(p + 1) == '%') { p += 2; continue; }  // skip %%
      pct = p;
      break;
    }
    p++;
  }
  if (!pct) {
    // No format specifier found — just copy the format string as-is
    strncpy(out, fmt, outSize - 1);
    out[outSize - 1] = '\0';
    return strlen(out);
  }

  // Parse precision from %[width][.prec]f/e/g
  const char *sp = pct + 1;  // skip '%'
  // Skip flags: -, +, space, 0, #
  while (*sp == '-' || *sp == '+' || *sp == ' ' || *sp == '0' || *sp == '#') sp++;
  // Skip width digits
  while (*sp >= '0' && *sp <= '9') sp++;
  // Parse precision
  int prec = 2;  // default precision
  if (*sp == '.') {
    sp++;
    prec = 0;
    while (*sp >= '0' && *sp <= '9') {
      prec = prec * 10 + (*sp - '0');
      sp++;
    }
  }
  // Skip conversion char (f, e, E, g, G)
  if (*sp == 'f' || *sp == 'e' || *sp == 'E' || *sp == 'g' || *sp == 'G') sp++;
  // sp now points past the format specifier

  // Convert float to string
  char fbuf[32];
  dtostrfd((double)fval, prec, fbuf);

  // Build result: prefix + fbuf + suffix
  int pos = 0;
  // Copy prefix (text before %)
  for (const char *c = fmt; c < pct && pos < outSize - 1; c++) {
    out[pos++] = *c;
  }
  // Copy float string
  for (const char *c = fbuf; *c && pos < outSize - 1; c++) {
    out[pos++] = *c;
  }
  // Copy suffix (text after format specifier)
  for (const char *c = sp; *c && pos < outSize - 1; c++) {
    out[pos++] = *c;
  }
  out[pos] = '\0';
  return pos;
}

/*********************************************************************************************\
 * VM: Syscall dispatch
\*********************************************************************************************/

static int tc_syscall(TcVM *vm, uint8_t id) {
  int32_t a, b;
  float fa;

  switch (id) {
    // ── GPIO (with bounds check) ────────────────────────
    case SYS_PIN_MODE:
      b = TC_POP(vm); a = TC_POP(vm);
      if (a >= 0 && a < MAX_GPIO_PIN) {
        pinMode(a, b);
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: pinMode(%d, %d)"), a, b);
      }
      break;
    case SYS_DIGITAL_WRITE:
      b = TC_POP(vm); a = TC_POP(vm);
      if (a >= 0 && a < MAX_GPIO_PIN) {
        digitalWrite(a, b);
      }
      break;
    case SYS_DIGITAL_READ:
      a = TC_POP(vm);
      if (a >= 0 && a < MAX_GPIO_PIN) {
        TC_PUSH(vm, digitalRead(a));
      } else {
        TC_PUSH(vm, 0);
      }
      break;
    case SYS_ANALOG_READ:
      a = TC_POP(vm);
#ifdef ESP32
      if (a >= 0 && a < MAX_GPIO_PIN) {
        TC_PUSH(vm, analogRead(a));
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, analogRead(A0));  // ESP8266 only has A0
#endif
      break;
    case SYS_ANALOG_WRITE:
      b = TC_POP(vm); a = TC_POP(vm);
      if (a >= 0 && a < MAX_GPIO_PIN) {
        analogWrite(a, b);
      }
      break;
    case SYS_GPIO_INIT:
      // Release pin from Tasmota GPIO management and set mode
      // This allows direct GPIO control even on pins assigned to Relay etc.
      b = TC_POP(vm); a = TC_POP(vm);  // pin, mode
      if (a >= 0 && a < MAX_GPIO_PIN) {
        // Detach from Tasmota function assignment
        if (TasmotaGlobal.gpio_pin[a] != AGPIO(GPIO_NONE)) {
          TasmotaGlobal.gpio_pin[a] = AGPIO(GPIO_NONE);
        }
        pinMode(a, b);
      }
      break;

    // ── Timing ────────────────────────────────────────
    case SYS_DELAY: {
      a = TC_POP(vm);
      // Non-blocking: pause VM, set resume time
      vm->delay_until = millis() + a;
      vm->delayed = true;
      return TC_ERR_PAUSED;  // signal caller to stop executing
    }
    case SYS_DELAY_MICRO:
      a = TC_POP(vm);
      if (a < 1000) delayMicroseconds(a);  // only allow short delays
      break;
    case SYS_MILLIS:
      TC_PUSH(vm, (int32_t)millis());
      break;
    case SYS_MICROS:
      TC_PUSH(vm, (int32_t)micros());
      break;
    case SYS_TIMER_START: {
      a = TC_POP(vm);  // ms
      b = TC_POP(vm);  // id
      if (b >= 0 && b < TC_MAX_TIMERS) {
        vm->timer_deadline[b] = millis() + (uint32_t)a;
        vm->timer_active[b] = true;
      }
      break;
    }
    case SYS_TIMER_DONE: {
      a = TC_POP(vm);  // id
      int32_t result = 1;  // default: done (not started)
      if (a >= 0 && a < TC_MAX_TIMERS && vm->timer_active[a]) {
        result = ((int32_t)(millis() - vm->timer_deadline[a]) >= 0) ? 1 : 0;
      }
      TC_PUSH(vm, result);
      break;
    }
    case SYS_TIMER_STOP: {
      a = TC_POP(vm);  // id
      if (a >= 0 && a < TC_MAX_TIMERS) {
        vm->timer_active[a] = false;
      }
      break;
    }
    case SYS_TIMER_REMAINING: {
      a = TC_POP(vm);  // id
      int32_t remaining = 0;
      if (a >= 0 && a < TC_MAX_TIMERS && vm->timer_active[a]) {
        int32_t diff = (int32_t)(vm->timer_deadline[a] - millis());
        remaining = (diff > 0) ? diff : 0;
      }
      TC_PUSH(vm, remaining);
      break;
    }

    // ── Serial/Output → AddLog + MQTT ─────────────────
    case SYS_SERIAL_BEGIN:
      TC_POP(vm);  // ignore baud in Tasmota context
      break;
    case SYS_SERIAL_PRINT:
      a = TC_POP(vm);
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        tc_output_string(vm->constants[a].str.ptr);
      }
      break;
    case SYS_SERIAL_PRINT_INT:
      a = TC_POP(vm);
      tc_output_int(a);
      break;
    case SYS_SERIAL_PRINT_FLT:
      fa = TC_POPF(vm);
      tc_output_float(fa);
      break;
    case SYS_SERIAL_PRINTLN:
      a = TC_POP(vm);
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        tc_output_string(vm->constants[a].str.ptr);
      }
      tc_output_char('\n');
      break;
    case SYS_SERIAL_READ:
      TC_PUSH(vm, -1);  // no serial input in Tasmota
      break;
    case SYS_SERIAL_AVAILABLE:
      TC_PUSH(vm, 0);
      break;

    // ── Math ──────────────────────────────────────────
    case SYS_MATH_ABS:
      a = TC_POP(vm); TC_PUSH(vm, a < 0 ? -a : a); break;
    case SYS_MATH_MIN:
      b = TC_POP(vm); a = TC_POP(vm); TC_PUSH(vm, a < b ? a : b); break;
    case SYS_MATH_MAX:
      b = TC_POP(vm); a = TC_POP(vm); TC_PUSH(vm, a > b ? a : b); break;
    case SYS_MATH_MAP: {
      int32_t toHi = TC_POP(vm), toLo = TC_POP(vm);
      int32_t fromHi = TC_POP(vm), fromLo = TC_POP(vm);
      int32_t val = TC_POP(vm);
      if (fromHi == fromLo) { TC_PUSH(vm, toLo); break; }
      TC_PUSH(vm, toLo + (val - fromLo) * (toHi - toLo) / (fromHi - fromLo));
      break;
    }
    case SYS_MATH_RANDOM:
      b = TC_POP(vm); a = TC_POP(vm);
      TC_PUSH(vm, a + (random(b - a)));
      break;
    case SYS_MATH_SQRT:
      fa = TC_POPF(vm); TC_PUSHF(vm, sqrtf(fa)); break;
    case SYS_MATH_SIN:
      fa = TC_POPF(vm); TC_PUSHF(vm, sinf(fa)); break;
    case SYS_MATH_COS:
      fa = TC_POPF(vm); TC_PUSHF(vm, cosf(fa)); break;

    // ── Tasmota-specific ──────────────────────────────
    case SYS_MQTT_PUBLISH:
      // From task context, only AddLog is safe; MQTT publish is not thread-safe
      tc_output_flush();
      break;
    case SYS_GET_POWER:
      a = TC_POP(vm);  // relay index (1-based)
      TC_PUSH(vm, bitRead(TasmotaGlobal.power, a - 1));
      break;
    case SYS_SET_POWER:
      b = TC_POP(vm); a = TC_POP(vm);  // relay, state
      ExecuteCommandPower(a, b, SRC_IGNORE);
      break;
    case SYS_TASM_CMD: {
      int32_t buf_ref = TC_POP(vm);    // output buffer array ref
      int32_t ci = TC_POP(vm);         // const pool index for command
      const char *cmd = tc_get_const_str(vm, ci);
      int32_t *buf = tc_resolve_ref(vm, buf_ref);
      if (!cmd || !buf) {
        TC_PUSH(vm, -1);
        break;
      }
      // Cap to remaining slots from base index (leave room for null terminator)
      int32_t maxLen = tc_ref_maxlen(vm, buf_ref) - 1;
      if (maxLen <= 0) { TC_PUSH(vm, 0); break; }
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: tasmCmd(\"%s\")"), cmd);
      // Execute Tasmota command — response goes to global buffer
      ExecuteCommand((char*)cmd, SRC_TCL);
      // Capture response immediately before it's overwritten
      const char *resp = ResponseData();
      int32_t rlen = strlen(resp);
      if (rlen > maxLen) rlen = maxLen;
      for (int32_t i = 0; i < rlen; i++) {
        buf[i] = (int32_t)(uint8_t)resp[i];
      }
      buf[rlen] = 0;  // null-terminate
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: tasmCmd -> %d bytes"), rlen);
      TC_PUSH(vm, rlen);
      break;
    }

    // ── String operations (all bounds-checked via tc_ref_maxlen) ──
    case SYS_STRLEN: {
      a = TC_POP(vm);  // array ref
      int32_t *p = tc_resolve_ref(vm, a);
      int32_t len = 0;
      if (p) { int32_t max = tc_ref_maxlen(vm, a); while (p[len] != 0 && len < max) len++; }
      TC_PUSH(vm, len);
      break;
    }
    case SYS_STRCPY: {
      int32_t src_ref = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      int32_t *src = tc_resolve_ref(vm, src_ref);
      if (dst && src) {
        int32_t max = tc_ref_maxlen(vm, dst_ref) - 1;
        int32_t i = 0;
        while (src[i] != 0 && i < max) { dst[i] = src[i]; i++; }
        dst[i] = 0;
      }
      break;
    }
    case SYS_STRCAT: {
      int32_t src_ref = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      int32_t *src = tc_resolve_ref(vm, src_ref);
      if (dst && src) {
        int32_t max = tc_ref_maxlen(vm, dst_ref) - 1;
        int32_t di = 0;
        while (dst[di] != 0 && di < max) di++;
        int32_t si = 0;
        while (src[si] != 0 && di < max) { dst[di++] = src[si++]; }
        dst[di] = 0;
      }
      break;
    }
    case SYS_STRCMP: {
      int32_t b_ref = TC_POP(vm);
      int32_t a_ref = TC_POP(vm);
      int32_t *pa = tc_resolve_ref(vm, a_ref);
      int32_t *pb = tc_resolve_ref(vm, b_ref);
      int32_t result = 0;
      if (pa && pb) {
        int32_t max_a = tc_ref_maxlen(vm, a_ref);
        int32_t max_b = tc_ref_maxlen(vm, b_ref);
        int32_t max = max_a < max_b ? max_a : max_b;
        int32_t i = 0;
        while (pa[i] != 0 && pb[i] != 0 && pa[i] == pb[i] && i < max) i++;
        result = pa[i] - pb[i];
      }
      TC_PUSH(vm, result);
      break;
    }
    case SYS_STR_PRINT: {
      a = TC_POP(vm);  // array ref
      int32_t *p = tc_resolve_ref(vm, a);
      if (p) {
        int32_t max = tc_ref_maxlen(vm, a);
        int32_t i = 0;
        while (p[i] != 0 && i < max) {
          tc_output_char((char)(p[i] & 0xFF));
          i++;
        }
      }
      break;
    }
    case SYS_STRCPY_CONST: {
      int32_t ci = TC_POP(vm);  // constant pool index
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      if (dst && ci >= 0 && ci < vm->const_count && vm->constants[ci].type == 1) {
        const char *s = vm->constants[ci].str.ptr;
        int32_t max = tc_ref_maxlen(vm, dst_ref) - 1;
        int32_t i = 0;
        while (s[i] != 0 && i < max) { dst[i] = (int32_t)(uint8_t)s[i]; i++; }
        dst[i] = 0;
      }
      break;
    }
    case SYS_STRCAT_CONST: {
      int32_t ci = TC_POP(vm);  // constant pool index
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      if (dst && ci >= 0 && ci < vm->const_count && vm->constants[ci].type == 1) {
        const char *s = vm->constants[ci].str.ptr;
        int32_t max = tc_ref_maxlen(vm, dst_ref) - 1;
        int32_t di = 0;
        while (dst[di] != 0 && di < max) di++;
        int32_t si = 0;
        while (s[si] != 0 && di < max) { dst[di++] = (int32_t)(uint8_t)s[si++]; }
        dst[di] = 0;
      }
      break;
    }

    // ── sprintf variants ──────────────────────────────
    // All use snprintf() on device, then copy result into VM array.
    // Supports: %d %u %x %X %o %c %s %f %e %g and width/precision modifiers.
    case SYS_SPRINTF_INT: {
      int32_t val = TC_POP(vm);          // int argument
      int32_t ci  = TC_POP(vm);          // format string const index
      int32_t dst_ref = TC_POP(vm);      // destination array ref
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      const char *fmt = tc_get_const_str(vm, ci);
      if (!dst || !fmt) { TC_PUSH(vm, -1); break; }
      int32_t maxSlots = tc_ref_maxlen(vm, dst_ref);
      char tmp[64];
      snprintf(tmp, sizeof(tmp), fmt, val);
      TC_PUSH(vm, tc_sprintf_to_ref(dst, maxSlots, tmp));
      break;
    }
    case SYS_SPRINTF_FLT: {
      float fval = TC_POPF(vm);          // float argument
      int32_t ci  = TC_POP(vm);          // format string const index
      int32_t dst_ref = TC_POP(vm);      // destination array ref
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      const char *fmt = tc_get_const_str(vm, ci);
      if (!dst || !fmt) { TC_PUSH(vm, -1); break; }
      int32_t maxSlots = tc_ref_maxlen(vm, dst_ref);
      char tmp[64];
      tc_sprintf_float(tmp, sizeof(tmp), fmt, fval);
      TC_PUSH(vm, tc_sprintf_to_ref(dst, maxSlots, tmp));
      break;
    }
    case SYS_SPRINTF_STR: {
      int32_t src_ref = TC_POP(vm);      // source string array ref
      int32_t ci  = TC_POP(vm);          // format string const index
      int32_t dst_ref = TC_POP(vm);      // destination array ref
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      int32_t *src = tc_resolve_ref(vm, src_ref);
      const char *fmt = tc_get_const_str(vm, ci);
      if (!dst || !src || !fmt) { TC_PUSH(vm, -1); break; }
      int32_t maxSlots = tc_ref_maxlen(vm, dst_ref);
      // Extract source string from VM int32 array into temp char buffer
      int32_t srcMax = tc_ref_maxlen(vm, src_ref);
      char srcbuf[128];
      int32_t si = 0;
      while (src[si] != 0 && si < srcMax && si < (int32_t)sizeof(srcbuf) - 1) {
        srcbuf[si] = (char)(src[si] & 0xFF);
        si++;
      }
      srcbuf[si] = '\0';
      char tmp[128];
      snprintf(tmp, sizeof(tmp), fmt, srcbuf);
      TC_PUSH(vm, tc_sprintf_to_ref(dst, maxSlots, tmp));
      break;
    }

    // ── sprintf append variants (same as above but append to existing string) ──
    case SYS_SPRINTF_INT_CAT: {
      int32_t val = TC_POP(vm);
      int32_t ci  = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      const char *fmt = tc_get_const_str(vm, ci);
      if (!dst || !fmt) { TC_PUSH(vm, -1); break; }
      int32_t maxSlots = tc_ref_maxlen(vm, dst_ref);
      int32_t ofs = tc_strlen_ref(dst, maxSlots);
      char tmp[64];
      snprintf(tmp, sizeof(tmp), fmt, val);
      tc_sprintf_to_ref(dst + ofs, maxSlots - ofs, tmp);
      TC_PUSH(vm, ofs + (int32_t)strlen(tmp));
      break;
    }
    case SYS_SPRINTF_FLT_CAT: {
      float fval = TC_POPF(vm);
      int32_t ci  = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      const char *fmt = tc_get_const_str(vm, ci);
      if (!dst || !fmt) { TC_PUSH(vm, -1); break; }
      int32_t maxSlots = tc_ref_maxlen(vm, dst_ref);
      int32_t ofs = tc_strlen_ref(dst, maxSlots);
      char tmp[64];
      tc_sprintf_float(tmp, sizeof(tmp), fmt, fval);
      tc_sprintf_to_ref(dst + ofs, maxSlots - ofs, tmp);
      TC_PUSH(vm, ofs + (int32_t)strlen(tmp));
      break;
    }
    case SYS_SPRINTF_STR_CAT: {
      int32_t src_ref = TC_POP(vm);
      int32_t ci  = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      int32_t *src = tc_resolve_ref(vm, src_ref);
      const char *fmt = tc_get_const_str(vm, ci);
      if (!dst || !src || !fmt) { TC_PUSH(vm, -1); break; }
      int32_t maxSlots = tc_ref_maxlen(vm, dst_ref);
      int32_t ofs = tc_strlen_ref(dst, maxSlots);
      int32_t srcMax = tc_ref_maxlen(vm, src_ref);
      char srcbuf[128];
      int32_t si = 0;
      while (src[si] != 0 && si < srcMax && si < (int32_t)sizeof(srcbuf) - 1) {
        srcbuf[si] = (char)(src[si] & 0xFF); si++;
      }
      srcbuf[si] = '\0';
      char tmp[128];
      snprintf(tmp, sizeof(tmp), fmt, srcbuf);
      tc_sprintf_to_ref(dst + ofs, maxSlots - ofs, tmp);
      TC_PUSH(vm, ofs + (int32_t)strlen(tmp));
      break;
    }

    // ── File I/O (LittleFS) ────────────────────────────
    case SYS_FILE_OPEN: {
#ifdef USE_UFILESYS
      int32_t mode = TC_POP(vm);       // 0=read, 1=write, 2=append
      int32_t ci = TC_POP(vm);         // const pool index for path
      const char *path = tc_get_const_str(vm, ci);
      if (!path || !ffsp) { TC_PUSH(vm, -1); break; }
      int slot = tc_alloc_file_handle();
      if (slot < 0) {
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: fileOpen no free handle"));
        TC_PUSH(vm, -1);
        break;
      }
      const char *mode_str;
      switch (mode) {
        case 0:  mode_str = "r";  tc_file_handles[slot] = ffsp->open(path, "r"); break;
        case 1:  mode_str = "w";  tc_file_handles[slot] = ffsp->open(path, "w"); break;
        case 2:  mode_str = "a";  tc_file_handles[slot] = ffsp->open(path, "a"); break;
        default: mode_str = "?";  TC_PUSH(vm, -1); break;
      }
      if (mode > 2) break;  // invalid mode already pushed -1
      if (!tc_file_handles[slot]) {
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileOpen(\"%s\", %s) failed"), path, mode_str);
        TC_PUSH(vm, -1);
      } else {
        Tinyc->file_used[slot] = true;
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileOpen(\"%s\", %s) -> handle %d"), path, mode_str, slot);
        TC_PUSH(vm, slot);
      }
#else
      TC_POP(vm); TC_POP(vm);  // consume args
      TC_PUSH(vm, -1);
#endif
      break;
    }
    case SYS_FILE_CLOSE: {
      int32_t h = TC_POP(vm);
      if (h >= 0 && h < TC_MAX_FILE_HANDLES && Tinyc->file_used[h]) {
        tc_file_handles[h].close();
        Tinyc->file_used[h] = false;
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileClose(%d)"), h);
        TC_PUSH(vm, 0);
      } else {
        TC_PUSH(vm, -1);
      }
      break;
    }
    case SYS_FILE_READ: {
      int32_t maxBytes = TC_POP(vm);   // max bytes to read
      int32_t buf_ref = TC_POP(vm);    // array ref for destination buffer
      int32_t h = TC_POP(vm);          // file handle
      int32_t *buf = tc_resolve_ref(vm, buf_ref);
      if (!buf || h < 0 || h >= TC_MAX_FILE_HANDLES || !Tinyc->file_used[h]) {
        TC_PUSH(vm, -1);
        break;
      }
      // Limit to actual remaining slots from base index
      int32_t maxSlots = tc_ref_maxlen(vm, buf_ref);
      if (maxBytes > maxSlots) maxBytes = maxSlots;
      // Read via temp byte buffer (File reads bytes, VM stores int32 per element)
      uint8_t tmp[256];
      int32_t total = 0;
      while (total < maxBytes) {
        int32_t chunk = maxBytes - total;
        if (chunk > (int32_t)sizeof(tmp)) chunk = sizeof(tmp);
        int n = tc_file_handles[h].read(tmp, chunk);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
          buf[total + i] = (int32_t)tmp[i];
        }
        total += n;
      }
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileRead(%d, %d) -> %d bytes"), h, maxBytes, total);
      TC_PUSH(vm, total);
      break;
    }
    case SYS_FILE_WRITE: {
      int32_t len = TC_POP(vm);        // bytes to write
      int32_t buf_ref = TC_POP(vm);    // array ref for source buffer
      int32_t h = TC_POP(vm);          // file handle
      int32_t *buf = tc_resolve_ref(vm, buf_ref);
      if (!buf || h < 0 || h >= TC_MAX_FILE_HANDLES || !Tinyc->file_used[h]) {
        TC_PUSH(vm, -1);
        break;
      }
      int32_t maxSlots = tc_ref_maxlen(vm, buf_ref);
      if (len > maxSlots) len = maxSlots;
      // Convert int32 array elements to bytes and write
      uint8_t tmp[256];
      int32_t total = 0;
      while (total < len) {
        int32_t chunk = len - total;
        if (chunk > (int32_t)sizeof(tmp)) chunk = sizeof(tmp);
        for (int32_t i = 0; i < chunk; i++) {
          tmp[i] = (uint8_t)(buf[total + i] & 0xFF);
        }
        int n = tc_file_handles[h].write(tmp, chunk);
        if (n <= 0) break;
        total += n;
      }
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileWrite(%d, %d) -> %d bytes"), h, len, total);
      TC_PUSH(vm, total);
      break;
    }
    case SYS_FILE_EXISTS: {
#ifdef USE_UFILESYS
      int32_t ci = TC_POP(vm);
      const char *path = tc_get_const_str(vm, ci);
      if (!path || !ffsp) { TC_PUSH(vm, 0); break; }
      bool exists = ffsp->exists(path);
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileExists(\"%s\") -> %d"), path, exists ? 1 : 0);
      TC_PUSH(vm, exists ? 1 : 0);
#else
      TC_POP(vm);
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_FILE_DELETE: {
#ifdef USE_UFILESYS
      int32_t ci = TC_POP(vm);
      const char *path = tc_get_const_str(vm, ci);
      if (!path || !ffsp) { TC_PUSH(vm, -1); break; }
      bool ok = ffsp->remove(path);
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileDelete(\"%s\") -> %d"), path, ok ? 0 : -1);
      TC_PUSH(vm, ok ? 0 : -1);
#else
      TC_POP(vm);
      TC_PUSH(vm, -1);
#endif
      break;
    }
    case SYS_FILE_SIZE: {
#ifdef USE_UFILESYS
      int32_t ci = TC_POP(vm);
      const char *path = tc_get_const_str(vm, ci);
      if (!path || !ffsp) { TC_PUSH(vm, -1); break; }
      File f = ffsp->open(path, "r");
      if (!f) {
        TC_PUSH(vm, -1);
      } else {
        int32_t sz = f.size();
        f.close();
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: fileSize(\"%s\") -> %d"), path, sz);
        TC_PUSH(vm, sz);
      }
#else
      TC_POP(vm);
      TC_PUSH(vm, -1);
#endif
      break;
    }

    // ── Heap allocation ──────────────────────────────
    case SYS_HEAP_ALLOC: {
      int32_t size = TC_POP(vm);
      TC_PUSH(vm, tc_heap_alloc(vm, (uint16_t)size));
      break;
    }
    case SYS_HEAP_FREE: {
      int32_t handle = TC_POP(vm);
      tc_heap_free_handle(vm, handle);
      break;
    }

    // ── Tasmota output (for callbacks) ────────────────
    // Streams VM string in 256-byte chunks — no size limit
    case SYS_RESPONSE_APPEND: {
      a = TC_POP(vm);
      tc_stream_ref(vm, a, tc_send_response);
      break;
    }
    case SYS_WEB_SEND: {
      a = TC_POP(vm);
#ifdef USE_WEBSERVER
      tc_stream_ref(vm, a, tc_send_web);
#endif
      break;
    }
    case SYS_WEB_FLUSH: {
#ifdef USE_WEBSERVER
      WSContentFlush();
#endif
      break;
    }

    // ── Tasmota output — string literal variants ──────
    case SYS_RESPONSE_APPEND_STR: {
      a = TC_POP(vm);  // constant pool index
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        ResponseAppend_P(PSTR("%s"), vm->constants[a].str.ptr);
      }
      break;
    }
    case SYS_WEB_SEND_STR: {
      a = TC_POP(vm);  // constant pool index
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
#ifdef USE_WEBSERVER
        int slen = strlen(vm->constants[a].str.ptr);
        WSContentSend(vm->constants[a].str.ptr, slen);
#endif
      }
      break;
    }

    case SYS_LOG: {
      a = TC_POP(vm);  // char array ref
      char tmp[TC_OUTPUT_SIZE];
      if (tc_ref_to_cstr(vm, a, tmp, sizeof(tmp)) > 0) {
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: %s"), tmp);
      }
      break;
    }
    case SYS_LOG_STR: {
      a = TC_POP(vm);  // constant pool index
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: %s"), vm->constants[a].str.ptr);
      }
      break;
    }

    // ── UDP multicast ─────────────────────────────────
    case SYS_UDP_SEND: {
      // Stack: [const_idx_name, float_bits]  — float on top
      a = TC_POP(vm);  // float value (as int32 bits)
      b = TC_POP(vm);  // const pool index for variable name
      if (b >= 0 && b < vm->const_count && vm->constants[b].type == 1) {
        if (!Tinyc->udp_used) {
          Tinyc->udp_used = true;
          tc_udp_init();
        }
        float fv = i2f(a);
        tc_udp_send(vm->constants[b].str.ptr, fv);
      }
      break;
    }
    case SYS_UDP_RECV: {
      a = TC_POP(vm);  // const pool index for variable name
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        if (!Tinyc->udp_used) {
          Tinyc->udp_used = true;
          tc_udp_init();
        }
        TcUdpVar *var = tc_udp_find_var(vm->constants[a].str.ptr, true);
        if (var) {
          TC_PUSH(vm, f2i(var->value));
        } else {
          TC_PUSH(vm, 0);
        }
      } else {
        TC_PUSH(vm, 0);
      }
      break;
    }
    case SYS_UDP_READY: {
      a = TC_POP(vm);  // const pool index for variable name
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        TcUdpVar *var = tc_udp_find_var(vm->constants[a].str.ptr, false);
        if (var && var->ready) {
          var->ready = false;
          TC_PUSH(vm, 1);
        } else {
          TC_PUSH(vm, 0);
        }
      } else {
        TC_PUSH(vm, 0);
      }
      break;
    }

    // ── UDP array send/receive ────────────────────────
    case SYS_UDP_SEND_ARRAY: {
      // Stack: [const_idx_name, arr_ref, count] — count on top
      int32_t count = TC_POP(vm);
      int32_t arr_ref = TC_POP(vm);
      b = TC_POP(vm);  // const pool index for variable name
      if (b >= 0 && b < vm->const_count && vm->constants[b].type == 1) {
        if (!Tinyc->udp_used) {
          Tinyc->udp_used = true;
          tc_udp_init();
        }
        int32_t *arr = tc_resolve_ref(vm, arr_ref);
        int32_t maxLen = tc_ref_maxlen(vm, arr_ref);
        if (arr && count > 0) {
          if (count > maxLen) count = maxLen;
          if (count > TC_UDP_MAX_ARRAY) count = TC_UDP_MAX_ARRAY;
          // Convert int32 (float bits) to float array on stack
          float fbuf[TC_UDP_MAX_ARRAY];
          for (int32_t i = 0; i < count; i++) {
            fbuf[i] = i2f(arr[i]);
          }
          tc_udp_send_array(vm->constants[b].str.ptr, fbuf, (uint16_t)count);
        }
      }
      break;
    }
    case SYS_UDP_RECV_ARRAY: {
      // Stack: [const_idx_name, arr_ref, maxcount] — maxcount on top
      int32_t maxcount = TC_POP(vm);
      int32_t arr_ref = TC_POP(vm);
      a = TC_POP(vm);  // const pool index for variable name
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        if (!Tinyc->udp_used) {
          Tinyc->udp_used = true;
          tc_udp_init();
        }
        TcUdpVar *var = tc_udp_find_var(vm->constants[a].str.ptr, true);  // create slot to register interest
        if (var && var->arr_data && var->arr_count > 0) {
          int32_t *arr = tc_resolve_ref(vm, arr_ref);
          int32_t maxLen = tc_ref_maxlen(vm, arr_ref);
          if (arr) {
            int32_t n = var->arr_count;
            if (n > maxcount) n = maxcount;
            if (n > maxLen) n = maxLen;
            for (int32_t i = 0; i < n; i++) {
              arr[i] = f2i(var->arr_data[i]);
            }
            TC_PUSH(vm, n);
          } else {
            TC_PUSH(vm, 0);
          }
        } else {
          TC_PUSH(vm, 0);
        }
      } else {
        TC_PUSH(vm, 0);
      }
      break;
    }

    // ── I2C bus (all calls take bus as last param: 0 or 1) ──
    case SYS_I2C_READ8: {
      // Stack: [addr, reg, bus] — bus on top
      int32_t bus = TC_POP(vm);
      b = TC_POP(vm);  // reg
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      TC_PUSH(vm, (int32_t)I2cRead8((uint8_t)a, (uint8_t)b, (uint8_t)bus));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_I2C_WRITE8: {
      // Stack: [addr, reg, val, bus] — bus on top
      int32_t bus = TC_POP(vm);
      int32_t val = TC_POP(vm);
      b = TC_POP(vm);  // reg
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      TC_PUSH(vm, I2cWrite8((uint8_t)a, (uint8_t)b, (uint32_t)(val & 0xFF), (uint8_t)bus) ? 1 : 0);
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_I2C_READ_BUF: {
      // Stack: [addr, reg, buf_ref, len, bus] — bus on top
      int32_t bus = TC_POP(vm);
      int32_t len = TC_POP(vm);
      int32_t buf_ref = TC_POP(vm);
      b = TC_POP(vm);  // reg
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      int32_t *arr = tc_resolve_ref(vm, buf_ref);
      int32_t maxLen = tc_ref_maxlen(vm, buf_ref);
      if (arr && len > 0) {
        if (len > maxLen) len = maxLen;
        if (len > 255) len = 255;  // I2C practical limit
        uint8_t tmpbuf[256];
        bool err = I2cReadBuffer((uint8_t)a, (int)b, tmpbuf, (uint16_t)len, (uint8_t)bus);
        if (!err) {  // I2cReadBuffer returns 0=OK, 1=Error
          for (int32_t i = 0; i < len; i++) { arr[i] = (int32_t)tmpbuf[i]; }
          TC_PUSH(vm, 1);
        } else {
          TC_PUSH(vm, 0);
        }
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_I2C_WRITE_BUF: {
      // Stack: [addr, reg, buf_ref, len, bus] — bus on top
      int32_t bus = TC_POP(vm);
      int32_t len = TC_POP(vm);
      int32_t buf_ref = TC_POP(vm);
      b = TC_POP(vm);  // reg
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      int32_t *arr = tc_resolve_ref(vm, buf_ref);
      int32_t maxLen = tc_ref_maxlen(vm, buf_ref);
      if (arr && len > 0) {
        if (len > maxLen) len = maxLen;
        if (len > 255) len = 255;
        uint8_t tmpbuf[256];
        for (int32_t i = 0; i < len; i++) { tmpbuf[i] = (uint8_t)(arr[i] & 0xFF); }
        TC_PUSH(vm, I2cWriteBuffer((uint8_t)a, (uint8_t)b, tmpbuf, (uint16_t)len, (uint8_t)bus) ? 0 : 1);  // returns 0=OK,1=Err
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_I2C_EXISTS: {
      // Stack: [addr, bus] — bus on top
      int32_t bus = TC_POP(vm);
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      TC_PUSH(vm, I2cSetDevice((uint32_t)a, (uint8_t)bus) ? 1 : 0);
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_I2C_READ_BUF0: {
      // Stack: [addr, buf_ref, len, bus] — bus on top  (no register byte sent)
      int32_t bus = TC_POP(vm);
      int32_t len = TC_POP(vm);
      int32_t buf_ref = TC_POP(vm);
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      int32_t *arr = tc_resolve_ref(vm, buf_ref);
      int32_t maxLen = tc_ref_maxlen(vm, buf_ref);
      if (arr && len > 0) {
        if (len > maxLen) len = maxLen;
        if (len > 255) len = 255;
        uint8_t tmpbuf[256];
        bool err = I2cReadBuffer0((uint8_t)a, tmpbuf, (uint16_t)len, (uint8_t)bus);
        if (!err) {  // I2cReadBuffer0 returns 0=OK, 1=Error
          for (int32_t i = 0; i < len; i++) { arr[i] = (int32_t)tmpbuf[i]; }
          TC_PUSH(vm, 1);
        } else {
          TC_PUSH(vm, 0);
        }
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_I2C_WRITE0: {
      // Stack: [addr, reg, bus] — bus on top  (write register byte only, no data)
      int32_t bus = TC_POP(vm);
      b = TC_POP(vm);  // reg
      a = TC_POP(vm);  // addr
#ifdef USE_I2C
      TC_PUSH(vm, I2cWrite0((uint8_t)a, (uint8_t)b, (uint8_t)bus) ? 1 : 0);
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }

    // ── Smart Meter (SML) ──────────────────────────────
    case SYS_SML_GET: {
      a = TC_POP(vm);  // meter index (0=count, 1..N=values)
#if defined(USE_SML_M) || defined(USE_SML)
      double dval = SML_GetVal((uint32_t)a);
      TC_PUSH(vm, f2i((float)dval));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_GETSTR: {
      int32_t ref = TC_POP(vm);  // buf_ref for output
      a = TC_POP(vm);            // meter index
#if defined(USE_SML_M) || defined(USE_SML)
      char *sval = SML_GetSVal((uint32_t)a);
      if (sval && ref) {
        // Copy string into TinyC char array
        int32_t *buf = tc_resolve_ref(vm, ref);
        int32_t maxLen = tc_ref_maxlen(vm, ref);
        if (buf && maxLen > 0) {
          int slen = strlen(sval);
          if (slen >= maxLen) slen = maxLen - 1;
          for (int i = 0; i < slen; i++) buf[i] = (int32_t)(uint8_t)sval[i];
          buf[slen] = 0;
          TC_PUSH(vm, slen);
        } else {
          TC_PUSH(vm, 0);
        }
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }

    // ── SML advanced ─────────────────────────────────────
    case SYS_SML_WRITE: {
      // Stack: [meter, buf_ref] — buf_ref on top
      int32_t ref = TC_POP(vm);  // hex string buffer ref
      a = TC_POP(vm);            // meter index
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      char tmp[256];
      tc_ref_to_cstr(vm, ref, tmp, sizeof(tmp));
      TC_PUSH(vm, (int32_t)SML_Write(a, tmp));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_READ: {
      // Stack: [meter, buf_ref] — buf_ref on top
      int32_t ref = TC_POP(vm);  // output buffer ref
      a = TC_POP(vm);            // meter index
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      char tmp[256];
      int32_t maxLen = tc_ref_maxlen(vm, ref);
      uint32_t slen = (maxLen > 0 && maxLen < (int32_t)sizeof(tmp)) ? maxLen : sizeof(tmp) - 1;
      uint32_t got = SML_Read(a, tmp, slen);
      // Copy result into TinyC char array
      int32_t *buf = tc_resolve_ref(vm, ref);
      if (buf && got > 0) {
        for (uint32_t i = 0; i < got && (int32_t)i < maxLen; i++) {
          buf[i] = (int32_t)(uint8_t)tmp[i];
        }
        if ((int32_t)got < maxLen) buf[got] = 0;
      }
      TC_PUSH(vm, (int32_t)got);
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_SETBAUD: {
      // Stack: [meter, baud] — baud on top
      b = TC_POP(vm);  // baud rate
      a = TC_POP(vm);  // meter index
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      TC_PUSH(vm, (int32_t)SML_SetBaud((uint32_t)a, (uint32_t)b));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_SETWSTR: {
      // Stack: [meter, buf_ref] — buf_ref on top
      int32_t ref = TC_POP(vm);  // hex string buffer ref
      a = TC_POP(vm);            // meter index
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      char tmp[256];
      tc_ref_to_cstr(vm, ref, tmp, sizeof(tmp));
      TC_PUSH(vm, (int32_t)SML_Set_WStr((uint32_t)a, tmp));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_SETOPT: {
      a = TC_POP(vm);  // options bitmask
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      TC_PUSH(vm, (int32_t)SML_SetOptions((uint32_t)a));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_GETV: {
      a = TC_POP(vm);  // selector
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      TC_PUSH(vm, (int32_t)sml_getv((uint32_t)a));
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_WRITE_STR: {
      // Stack: [meter, const_idx] — const_idx on top
      int32_t ci = TC_POP(vm);  // constant pool index
      a = TC_POP(vm);           // meter index
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      if (ci >= 0 && ci < vm->const_count && vm->constants[ci].type == 1) {
        TC_PUSH(vm, (int32_t)SML_Write(a, (char*)vm->constants[ci].str.ptr));
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }
    case SYS_SML_SETWSTR_STR: {
      // Stack: [meter, const_idx] — const_idx on top
      int32_t ci = TC_POP(vm);  // constant pool index
      a = TC_POP(vm);           // meter index
#if defined(USE_SML_SCRIPT_CMD) && (defined(USE_SML_M) || defined(USE_SML))
      if (ci >= 0 && ci < vm->const_count && vm->constants[ci].type == 1) {
        TC_PUSH(vm, (int32_t)SML_Set_WStr((uint32_t)a, (char*)vm->constants[ci].str.ptr));
      } else {
        TC_PUSH(vm, 0);
      }
#else
      TC_PUSH(vm, 0);
#endif
      break;
    }

    // ── SPI bus ────────────────────────────────────────
    case SYS_SPI_INIT: {
      // Stack: [sclk, mosi, miso, speed_mhz] — speed on top
      int32_t speed_mhz = TC_POP(vm);
      int32_t miso_pin  = TC_POP(vm);
      int32_t mosi_pin  = TC_POP(vm);
      int32_t sclk_pin  = TC_POP(vm);
      if (!Tinyc) { TC_PUSH(vm, 0); break; }
      TcSpi *spi = &Tinyc->spi;
      // Clean up previous hardware SPI instance if any
#ifdef ESP32
      if (spi->initialized && spi->sclk < 0 && spi->spip) {
        spi->spip->end();
        if (spi->spip != &SPI) { delete spi->spip; }
        spi->spip = nullptr;
      }
#endif
#ifdef ESP8266
      if (spi->initialized && spi->sclk < 0 && spi->spip) {
        spi->spip->end();
        spi->spip = nullptr;
      }
#endif
      spi->sclk = (int8_t)sclk_pin;
      spi->mosi = (int8_t)mosi_pin;
      spi->miso = (int8_t)miso_pin;

      if (sclk_pin < 0) {
        // Hardware SPI mode
        uint32_t freq = (uint32_t)speed_mhz * 1000000UL;
#ifdef ESP32
        if (sclk_pin == -1) {
          // Use Tasmota's primary SPI bus
          if (TasmotaGlobal.spi_enabled) {
            SPI.begin(Pin(GPIO_SPI_CLK), Pin(GPIO_SPI_MISO), Pin(GPIO_SPI_MOSI), -1);
            spi->spip = &SPI;
          } else {
            AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SPI pins not configured in Tasmota"));
            spi->initialized = false;
            TC_PUSH(vm, 0);
            break;
          }
        } else {
          // Use HSPI (secondary bus)
          spi->spip = new SPIClass(HSPI);
          if (TasmotaGlobal.spi_enabled) {
            spi->spip->begin(Pin(GPIO_SPI_CLK, 1), Pin(GPIO_SPI_MISO, 1), Pin(GPIO_SPI_MOSI, 1), -1);
          } else {
            AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SPI pins not configured in Tasmota"));
            delete spi->spip;
            spi->spip = nullptr;
            spi->initialized = false;
            TC_PUSH(vm, 0);
            break;
          }
        }
        spi->settings = SPISettings(freq, MSBFIRST, SPI_MODE0);
#endif // ESP32
#ifdef ESP8266
        if (TasmotaGlobal.spi_enabled) {
          SPI.begin();
          spi->spip = &SPI;
        } else {
          AddLog(LOG_LEVEL_ERROR, PSTR("TIC: SPI pins not configured in Tasmota"));
          spi->initialized = false;
          TC_PUSH(vm, 0);
          break;
        }
        spi->settings = SPISettings(freq, MSBFIRST, SPI_MODE0);
#endif // ESP8266
      } else {
        // Bitbang mode — configure GPIO pins directly
        pinMode(sclk_pin, OUTPUT);
        digitalWrite(sclk_pin, 0);
        if (mosi_pin >= 0) {
          pinMode(mosi_pin, OUTPUT);
          digitalWrite(mosi_pin, 0);
        }
        if (miso_pin >= 0) {
          pinMode(miso_pin, INPUT_PULLUP);
        }
        spi->spip = nullptr;
      }
      spi->initialized = true;
      AddLog(LOG_LEVEL_INFO, PSTR("TIC: SPI init sclk=%d mosi=%d miso=%d %dMHz"),
        sclk_pin, mosi_pin, miso_pin, speed_mhz);
      TC_PUSH(vm, 1);
      break;
    }

    case SYS_SPI_SET_CS: {
      // Stack: [index, pin] — pin on top
      int32_t pin   = TC_POP(vm);
      int32_t index = TC_POP(vm);
      if (!Tinyc) break;
      // index is 1-based in the API, stored 0-based
      index = (index - 1) & (TC_SPI_MAX_CS - 1);
      Tinyc->spi.cs[index] = (int8_t)pin;
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);  // CS inactive = high
      break;
    }

    case SYS_SPI_TRANSFER: {
      // Stack: [cs, buf_ref, len, mode] — mode on top
      int32_t mode    = TC_POP(vm);   // 1=8bit, 2=16bit, 3=24bit, 4=byte-with-cs
      int32_t len     = TC_POP(vm);
      int32_t buf_ref = TC_POP(vm);
      int32_t cs_idx  = TC_POP(vm);   // 1-based CS index (0 = no CS management)

      if (!Tinyc || !Tinyc->spi.initialized) {
        TC_PUSH(vm, 0);
        break;
      }
      TcSpi *spi = &Tinyc->spi;
      int32_t *arr = tc_resolve_ref(vm, buf_ref);
      int32_t maxLen = tc_ref_maxlen(vm, buf_ref);
      if (!arr || len <= 0) { TC_PUSH(vm, 0); break; }
      if (len > maxLen) len = maxLen;
      if (mode < 1 || mode > 4) mode = 1;

      // Resolve CS pin (1-based index, 0 = no CS)
      int8_t cs_pin = -1;
      if (cs_idx > 0) {
        int ci = (cs_idx - 1) & (TC_SPI_MAX_CS - 1);
        cs_pin = spi->cs[ci];
      }

      // Assert CS (low)
      if (cs_pin >= 0 && mode != 4) {
        digitalWrite(cs_pin, LOW);
      }

      if (spi->sclk < 0 && spi->spip) {
        // ── Hardware SPI transfer ──
        spi->spip->beginTransaction(spi->settings);
        for (int32_t cnt = 0; cnt < len; cnt++) {
          uint32_t out = 0;
          uint32_t val = (uint32_t)arr[cnt];
          if (mode == 1) {
            out = spi->spip->transfer((uint8_t)val);
          } else if (mode == 2) {
            out = spi->spip->transfer16((uint16_t)val);
          } else if (mode == 3) {
            out = spi->spip->transfer((uint8_t)(val >> 16));
            out <<= 16;
            out |= spi->spip->transfer16((uint16_t)val);
          } else if (mode == 4) {
            // Per-byte CS toggle
            if (cs_pin >= 0) digitalWrite(cs_pin, LOW);
            out = spi->spip->transfer((uint8_t)val);
            if (cs_pin >= 0) digitalWrite(cs_pin, HIGH);
          }
          arr[cnt] = (int32_t)out;
        }
        spi->spip->endTransaction();
      } else if (spi->sclk >= 0) {
        // ── Bitbang SPI transfer ──
        for (int32_t cnt = 0; cnt < len; cnt++) {
          uint32_t out = 0;
          uint32_t val = (uint32_t)arr[cnt];
          int nbits;
          if (mode == 4) {
            nbits = 8;
            if (cs_pin >= 0) digitalWrite(cs_pin, LOW);
          } else {
            nbits = mode * 8;  // 8, 16, or 24 bits
            if (nbits > 24) nbits = 24;
          }
          uint32_t bit = 1UL << (nbits - 1);
          while (bit) {
            digitalWrite(spi->sclk, LOW);
            if (spi->mosi >= 0) {
              digitalWrite(spi->mosi, (val & bit) ? HIGH : LOW);
            }
            digitalWrite(spi->sclk, HIGH);
            if (spi->miso >= 0) {
              if (digitalRead(spi->miso)) out |= bit;
            }
            bit >>= 1;
          }
          arr[cnt] = (int32_t)out;
          if (mode == 4 && cs_pin >= 0) {
            digitalWrite(cs_pin, HIGH);
          }
        }
      } else {
        TC_PUSH(vm, 0);
        break;
      }

      // Deassert CS (high)
      if (cs_pin >= 0 && mode != 4) {
        digitalWrite(cs_pin, HIGH);
      }

      TC_PUSH(vm, len);
      break;
    }

    // ── Tasmota system variables ─────────────────────
    // Index mapping (must match TASM_VARS in compiler):
    //   0 = tasm_wifi       (ro) 1=network up, 0=down
    //   1 = tasm_mqttcon    (ro) 1=mqtt connected, 0=down
    //   2 = tasm_teleperiod (rw) teleperiod in seconds
    //   3 = tasm_uptime     (ro) uptime in seconds
    //   4 = tasm_heap       (ro) free heap in bytes
    //   5 = tasm_power      (rw) relay bitmask (all relays)
    //   6 = tasm_dimmer     (rw) light dimmer 0-100
    //   7 = tasm_temp       (ro) global temperature (float)
    //   8 = tasm_hum        (ro) global humidity (float)
    //   9 = tasm_hour       (ro) current hour 0-23
    //  10 = tasm_min        (ro) current minute 0-59
    //  11 = tasm_sec        (ro) current second 0-59
    //  12 = tasm_year       (ro) current year (e.g. 2026)
    //  13 = tasm_month      (ro) current month 1-12
    //  14 = tasm_day        (ro) day of month 1-31
    //  15 = tasm_wday       (ro) day of week 1=Sun..7=Sat
    //  16 = tasm_cw         (ro) ISO calendar week 1-53
    //  17 = tasm_sunrise    (ro) sunrise minutes since midnight
    //  18 = tasm_sunset     (ro) sunset minutes since midnight
    //  19 = tasm_time       (ro) current minutes since midnight
    case SYS_TASM_GET: {
      a = TC_POP(vm);  // variable index
      int32_t val = 0;
      switch (a) {
        case 0: val = TasmotaGlobal.global_state.network_down ? 0 : 1; break;
        case 1: val = TasmotaGlobal.global_state.mqtt_down ? 0 : 1; break;
        case 2: val = (int32_t)Settings->tele_period; break;
        case 3: val = (int32_t)(millis() / 1000); break;
        case 4: val = (int32_t)ESP_getFreeHeap(); break;
        case 5: val = (int32_t)TasmotaGlobal.power; break;
        case 6:
#ifdef USE_LIGHT
          val = (int32_t)Settings->light_dimmer;
#endif
          break;
        case 7: {
          float tf = TasmotaGlobal.temperature_celsius;
          uint32_t ti; memcpy(&ti, &tf, 4);
          TC_PUSH(vm, (int32_t)ti);
          goto tasm_get_done;
        }
        case 8: {
          float hf = TasmotaGlobal.humidity;
          uint32_t hi; memcpy(&hi, &hf, 4);
          TC_PUSH(vm, (int32_t)hi);
          goto tasm_get_done;
        }
        case 9: val = (int32_t)RtcTime.hour; break;
        case 10: val = (int32_t)RtcTime.minute; break;
        case 11: val = (int32_t)RtcTime.second; break;
        case 12: val = (int32_t)RtcTime.year; break;
        case 13: val = (int32_t)RtcTime.month; break;
        case 14: val = (int32_t)RtcTime.day_of_month; break;
        case 15: val = (int32_t)RtcTime.day_of_week; break;
        case 16: {
          // ISO calendar week (same algorithm as scripter calc_cw)
          float a16 = floor((14.0f - RtcTime.month) / 12.0f);
          float y16 = RtcTime.year + 4800 - a16;
          float m16 = RtcTime.month + (12 * a16) - 3;
          float jd = RtcTime.day_of_month + floor((153.0f * m16 + 2) / 5.0f) +
                     (365 * y16) + floor(y16 / 4) - floor(y16 / 100) +
                     floor(y16 / 400) - 32045;
          float d4 = (uint32_t)((uint32_t)(jd + 31741 - ((uint32_t)jd % 7)) % 146097 % 36524 % 1461);
          float L = floor(d4 / 1460);
          float d1 = ((uint32_t)(d4 - L) % 365) + L;
          int cw = (int)floor(d1 / 7) + 1;
          if (cw == 1 && RtcTime.month == 12) { }  // year rollover
          if (cw >= 52 && RtcTime.month == 1) { }   // year rollback
          val = (int32_t)cw;
          break;
        }
#ifdef USE_SUNRISE
        case 17: val = (int32_t)SunMinutes(0); break;  // tasm_sunrise
        case 18: val = (int32_t)SunMinutes(1); break;  // tasm_sunset
#endif
        case 19: val = (int32_t)MinutesPastMidnight(); break;  // tasm_time
        default: break;
      }
      TC_PUSH(vm, val);
      tasm_get_done:
      break;
    }
    case SYS_TASM_SET: {
      a = TC_POP(vm);            // variable index (pushed last by compiler)
      int32_t val = TC_POP(vm);  // value (compiled first, pushed earlier)
      switch (a) {
        case 2:  // tasm_teleperiod
          if (val >= 10 && val <= 3600) {
            Settings->tele_period = (uint16_t)val;
            TasmotaGlobal.tele_period = 0;
          }
          break;
        case 5:  // tasm_power
          for (uint32_t i = 0; i < TasmotaGlobal.devices_present; i++) {
            ExecuteCommandPower(i + 1, (val >> i) & 1, SRC_IGNORE);
          }
          break;
        case 6: // tasm_dimmer
#ifdef USE_LIGHT
          { char cmd[16];
            snprintf_P(cmd, sizeof(cmd), PSTR("Dimmer %d"), val);
            ExecuteCommand(cmd, SRC_IGNORE); }
#endif
          break;
        default: break;  // read-only variables silently ignored
      }
      break;
    }

    // ── String manipulation ────────────────────────────
    case SYS_STR_TOKEN: {
      // strToken(dst, src, delim, n) -> length of token (1-based index)
      int32_t n = TC_POP(vm);           // 1-based token index
      int32_t delim = TC_POP(vm);       // delimiter char code
      int32_t src_ref = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *src = tc_resolve_ref(vm, src_ref);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      int32_t result = 0;
      if (src && dst && n >= 1) {
        int32_t src_max = tc_ref_maxlen(vm, src_ref);
        int32_t dst_max = tc_ref_maxlen(vm, dst_ref) - 1;
        // find start of nth token
        int32_t si = 0, idx = 1;
        while (idx < n && si < src_max && src[si] != 0) {
          if (src[si] == delim) idx++;
          si++;
        }
        if (idx == n && si < src_max && src[si] != 0) {
          int32_t di = 0;
          while (si < src_max && src[si] != 0 && src[si] != delim && di < dst_max) {
            dst[di++] = src[si++];
          }
          dst[di] = 0;
          result = di;
        } else {
          dst[0] = 0;
        }
      }
      TC_PUSH(vm, result);
      break;
    }
    case SYS_STR_SUB: {
      // strSub(dst, src, pos, len) -> actual length copied
      int32_t slen = TC_POP(vm);        // number of chars to copy
      int32_t pos = TC_POP(vm);         // start position (0-based, neg=from end)
      int32_t src_ref = TC_POP(vm);
      int32_t dst_ref = TC_POP(vm);
      int32_t *src = tc_resolve_ref(vm, src_ref);
      int32_t *dst = tc_resolve_ref(vm, dst_ref);
      int32_t result = 0;
      if (src && dst) {
        int32_t src_max = tc_ref_maxlen(vm, src_ref);
        int32_t dst_max = tc_ref_maxlen(vm, dst_ref) - 1;
        // compute source string length
        int32_t srclen = 0;
        while (srclen < src_max && src[srclen] != 0) srclen++;
        if (pos < 0) pos = srclen + pos;  // negative = from end
        if (pos < 0) pos = 0;
        if (pos > srclen) pos = srclen;
        if (slen < 0 || pos + slen > srclen) slen = srclen - pos;
        if (slen > dst_max) slen = dst_max;
        for (int32_t i = 0; i < slen; i++) {
          dst[i] = src[pos + i];
        }
        dst[slen] = 0;
        result = slen;
      }
      TC_PUSH(vm, result);
      break;
    }
    case SYS_STR_FIND: {
      // strFind(haystack, needle) -> position (-1 if not found)
      int32_t needle_ref = TC_POP(vm);
      int32_t haystack_ref = TC_POP(vm);
      int32_t *haystack = tc_resolve_ref(vm, haystack_ref);
      int32_t *needle = tc_resolve_ref(vm, needle_ref);
      int32_t result = -1;
      if (haystack && needle) {
        int32_t hmax = tc_ref_maxlen(vm, haystack_ref);
        int32_t nmax = tc_ref_maxlen(vm, needle_ref);
        int32_t hlen = 0;
        while (hlen < hmax && haystack[hlen] != 0) hlen++;
        int32_t nlen = 0;
        while (nlen < nmax && needle[nlen] != 0) nlen++;
        if (nlen > 0 && nlen <= hlen) {
          for (int32_t i = 0; i <= hlen - nlen; i++) {
            bool match = true;
            for (int32_t j = 0; j < nlen; j++) {
              if (haystack[i + j] != needle[j]) { match = false; break; }
            }
            if (match) { result = i; break; }
          }
        }
      }
      TC_PUSH(vm, result);
      break;
    }

    // ── Sensor JSON parsing ─────────────────────────────
    case SYS_SENSOR_GET: {
      // sensorGet("SensorName#Key#SubKey") -> float value
      int32_t ci = TC_POP(vm);  // constant pool index with JSON path
      float fval = 0.0f;
      if (ci >= 0 && ci < vm->const_count && vm->constants[ci].type == 1) {
        const char *path = vm->constants[ci].str.ptr;
        // Build sensor JSON via MqttShowSensor
        ResponseClear();
        ResponseAppend_P(PSTR("{"));
        MqttShowSensor(true);
        ResponseJsonEnd();
        // Parse the JSON path (segments separated by #)
        char jpath[64];
        strlcpy(jpath, path, sizeof(jpath));
        JsonParser parser(ResponseData());
        JsonParserObject obj = parser.getRootObject();
        char *seg = jpath;
        char *next;
        bool valid = true;
        while (valid && seg && *seg) {
          next = strchr(seg, '#');
          if (next) { *next = 0; next++; }
          JsonParserToken tok = obj[seg];
          if (!tok.isValid()) { valid = false; break; }
          if (next && *next) {
            // intermediate object — descend
            obj = tok.getObject();
            if (!obj.isValid()) { valid = false; break; }
          } else {
            // leaf value — extract float
            fval = tok.getFloat();
          }
          seg = next;
        }
      }
      // Push float as int32 bit pattern
      uint32_t fi;
      memcpy(&fi, &fval, 4);
      TC_PUSH(vm, (int32_t)fi);
      break;
    }

    // ── HTTP requests ─────────────────────────────────
    case SYS_HTTP_GET: {
      b = TC_POP(vm);  // response buffer ref
      a = TC_POP(vm);  // url ref
      char url[256];
      tc_ref_to_cstr(vm, a, url, sizeof(url));
      WiFiClient http_client;
      HTTPClient http;
      http.setTimeout(5000);
      http.begin(http_client, url);
      // Add custom headers
      for (int i = 0; i < Tinyc->http_hdr_count; i++) {
        http.addHeader(Tinyc->http_hdr_name[i], Tinyc->http_hdr_value[i]);
      }
      Tinyc->http_hdr_count = 0;
      int httpCode = http.GET();
      int32_t result = -1;
      if (httpCode > 0) {
        String payload = http.getString();
        int32_t *buf = tc_resolve_ref(vm, b);
        if (buf) {
          int32_t maxLen = tc_ref_maxlen(vm, b);
          int len = payload.length();
          if (len > maxLen - 1) len = maxLen - 1;
          for (int i = 0; i < len; i++) buf[i] = (int32_t)(uint8_t)payload[i];
          buf[len] = 0;
          result = len;
        }
      } else {
        result = httpCode;  // negative error code
      }
      http.end();
      http_client.stop();
      TC_PUSH(vm, result);
      break;
    }
    case SYS_HTTP_POST: {
      int32_t respRef = TC_POP(vm);  // response buffer ref
      int32_t dataRef = TC_POP(vm);  // POST data ref
      a = TC_POP(vm);                // url ref
      char url[256];
      tc_ref_to_cstr(vm, a, url, sizeof(url));
      char postData[TC_OUTPUT_SIZE];
      tc_ref_to_cstr(vm, dataRef, postData, sizeof(postData));
      WiFiClient http_client;
      HTTPClient http;
      http.setTimeout(5000);
      http.begin(http_client, url);
      http.addHeader(F("Content-Type"), F("application/x-www-form-urlencoded"));
      for (int i = 0; i < Tinyc->http_hdr_count; i++) {
        http.addHeader(Tinyc->http_hdr_name[i], Tinyc->http_hdr_value[i]);
      }
      Tinyc->http_hdr_count = 0;
      int httpCode = http.POST(postData);
      int32_t result = -1;
      if (httpCode > 0) {
        String payload = http.getString();
        int32_t *buf = tc_resolve_ref(vm, respRef);
        if (buf) {
          int32_t maxLen = tc_ref_maxlen(vm, respRef);
          int len = payload.length();
          if (len > maxLen - 1) len = maxLen - 1;
          for (int i = 0; i < len; i++) buf[i] = (int32_t)(uint8_t)payload[i];
          buf[len] = 0;
          result = len;
        }
      } else {
        result = httpCode;
      }
      http.end();
      http_client.stop();
      TC_PUSH(vm, result);
      break;
    }
    case SYS_HTTP_HEADER: {
      b = TC_POP(vm);  // value ref
      a = TC_POP(vm);  // name ref
      if (Tinyc->http_hdr_count < TC_HTTP_MAX_HEADERS) {
        tc_ref_to_cstr(vm, a, Tinyc->http_hdr_name[Tinyc->http_hdr_count], 64);
        tc_ref_to_cstr(vm, b, Tinyc->http_hdr_value[Tinyc->http_hdr_count], 64);
        Tinyc->http_hdr_count++;
      }
      break;
    }

    // ── WebUI widgets ─────────────────────────────────
    // Each generates HTML for the /tc_ui AJAX page.
    // gref = variable ref (ADDR_GLOBAL/ADDR_LOCAL), label/opts = const pool index.
    // Global index extracted from ref for seva()/siva() JavaScript callbacks.
    case SYS_WEB_BUTTON: {
      int32_t ci = TC_POP(vm);   // label const idx
      int32_t gref = TC_POP(vm); // variable ref
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;
      const char *label = tc_get_const_str(vm, ci);
      if (!label) label = "?";
      int32_t nval = val ? 0 : 1;
      WSContentSend_P(PSTR("<div><button onclick='seva(%d,%d)' style='width:100%%'>%s: %s</button></div>"),
                      nval, idx, label, val ? "ON" : "OFF");
      break;
    }
    case SYS_WEB_SLIDER: {
      int32_t ci = TC_POP(vm);   // label const idx
      int32_t vmax = TC_POP(vm);
      int32_t vmin = TC_POP(vm);
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;
      const char *label = tc_get_const_str(vm, ci);
      if (!label) label = "?";
      WSContentSend_P(PSTR("<div><label><b>%s</b> <span id='sv%d'>%d</span></label>"
                           "<input type='range' min='%d' max='%d' value='%d' "
                           "oninput='document.getElementById(\"sv%d\").textContent=this.value' "
                           "onchange='seva(value,%d)'></div>"),
                      label, idx, val, vmin, vmax, val, idx, idx);
      break;
    }
    case SYS_WEB_CHECKBOX: {
      int32_t ci = TC_POP(vm);
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;
      const char *label = tc_get_const_str(vm, ci);
      if (!label) label = "?";
      int32_t nval = val ? 0 : 1;
      WSContentSend_P(PSTR("<div><label><b>%s</b> "
                           "<input type='checkbox' %s onchange='seva(%d,%d)'>"
                           "</label></div>"),
                      label, val ? "checked" : "", nval, idx);
      break;
    }
    case SYS_WEB_TEXT: {
      int32_t ci = TC_POP(vm);      // label const idx
      int32_t maxlen = TC_POP(vm);  // max char length
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      char tbuf[128];
      tc_ref_to_cstr(vm, gref, tbuf, sizeof(tbuf));
      const char *label = tc_get_const_str(vm, ci);
      if (!label) label = "?";
      WSContentSend_P(PSTR("<div><label><b>%s</b> "
                           "<input type='text' value='%s' maxlength='%d' style='width:200px' "
                           "onfocusin='pr(0)' onfocusout='pr(1)' "
                           "onchange='siva(value,%d)'>"
                           "</label></div>"),
                      label, tbuf, maxlen - 1, idx);
      break;
    }
    case SYS_WEB_NUMBER: {
      int32_t ci = TC_POP(vm);
      int32_t vmax = TC_POP(vm);
      int32_t vmin = TC_POP(vm);
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;
      const char *label = tc_get_const_str(vm, ci);
      if (!label) label = "?";
      WSContentSend_P(PSTR("<div><label><b>%s</b> "
                           "<input type='number' min='%d' max='%d' value='%d' style='width:80px' "
                           "onfocusin='pr(0)' onfocusout='pr(1)' "
                           "onchange='seva(value,%d)'>"
                           "</label></div>"),
                      label, vmin, vmax, val, idx);
      break;
    }
    case SYS_WEB_PULLDOWN: {
      int32_t ci = TC_POP(vm);   // options const idx ("opt1|opt2|opt3")
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;  // 0-based selected index
      const char *opts = tc_get_const_str(vm, ci);
      if (!opts) opts = "";
      WSContentSend_P(PSTR("<div><select onchange='seva(value,%d)'>"), idx);
      // Parse pipe-separated options
      char obuf[256];
      strlcpy(obuf, opts, sizeof(obuf));
      int oi = 0;
      char *op = obuf;
      char *sep;
      while (op && *op) {
        sep = strchr(op, '|');
        if (sep) *sep = 0;
        WSContentSend_P(PSTR("<option value='%d'%s>%s</option>"),
                        oi, (oi == val) ? " selected" : "", op);
        oi++;
        op = sep ? sep + 1 : nullptr;
      }
      WSContentSend_P(PSTR("</select></div>"));
      break;
    }
    case SYS_WEB_RADIO: {
      int32_t ci = TC_POP(vm);   // options const idx ("opt1|opt2|opt3")
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;  // 0-based selected index
      const char *opts = tc_get_const_str(vm, ci);
      if (!opts) opts = "";
      WSContentSend_P(PSTR("<div><fieldset><legend></legend>"));
      char obuf[256];
      strlcpy(obuf, opts, sizeof(obuf));
      int oi = 0;
      char *op = obuf;
      char *sep;
      while (op && *op) {
        sep = strchr(op, '|');
        if (sep) *sep = 0;
        WSContentSend_P(PSTR("<div><label><input type='radio' name='r%d' onclick='seva(%d,%d)' %s>%s</label></div>"),
                        idx, oi, idx, (oi == val) ? "checked" : "", op);
        oi++;
        op = sep ? sep + 1 : nullptr;
      }
      WSContentSend_P(PSTR("</fieldset></div>"));
      break;
    }
    case SYS_WEB_TIME: {
      int32_t ci = TC_POP(vm);
      int32_t gref = TC_POP(vm);
      uint16_t idx = ((uint32_t)gref) & 0xFFFF;
      int32_t *p = tc_resolve_ref(vm, gref);
      int32_t val = p ? *p : 0;  // HHMM format
      const char *label = tc_get_const_str(vm, ci);
      if (!label) label = "?";
      int hh = val / 100;
      int mm = val % 100;
      WSContentSend_P(PSTR("<div><label><b>%s</b> "
                           "<input type='time' value='%02d:%02d' style='width:80px' "
                           "onfocusin='pr(0)' onfocusout='pr(1)' "
                           "onchange='sivat(value,%d)'>"
                           "</label></div>"),
                      label, hh, mm, idx);
      break;
    }

    case SYS_WEB_PAGE_LABEL: {
      int32_t ci = TC_POP(vm);      // label const index
      int32_t pn = TC_POP(vm);      // page number (0-5)
      const char *label = tc_get_const_str(vm, ci);
      if (label && Tinyc && pn >= 0 && pn < TC_MAX_WEB_PAGES) {
        strlcpy(Tinyc->page_label[pn], label, sizeof(Tinyc->page_label[0]));
        if (pn >= Tinyc->page_count) Tinyc->page_count = pn + 1;
      }
      break;
    }
    case SYS_WEB_PAGE: {
      // Push current page number being rendered
      TC_PUSH(vm, Tinyc ? Tinyc->current_page : 0);
      break;
    }
    case SYS_WEB_SEND_FILE: {
      // Send file contents to web page (like Scripter's %/filename)
      int32_t ci = TC_POP(vm);
      const char *fname = tc_get_const_str(vm, ci);
#ifdef USE_UFILESYS
      if (fname && ufsp) {
        char path[48];
        if (fname[0] != '/') {
          snprintf(path, sizeof(path), "/%s", fname);
        } else {
          strlcpy(path, fname, sizeof(path));
        }
        File f = ufsp->open(path, "r");
        if (f) {
          char buf[256];
          WSContentFlush();
          while (f.available()) {
            int len = f.readBytes(buf, sizeof(buf) - 1);
            buf[len] = 0;
            WSContentSend_P(PSTR("%s"), buf);
          }
          f.close();
        } else {
          AddLog(LOG_LEVEL_INFO, PSTR("TCC: webFile '%s' not found"), path);
        }
      }
#endif
      break;
    }

    case SYS_WEB_ON: {
      // Register custom web endpoint: webOn(handler_num, "/url/path")
      int32_t ci = TC_POP(vm);   // url const index
      int32_t hn = TC_POP(vm);   // handler number (1-4)
      const char *url = tc_get_const_str(vm, ci);
#ifdef USE_WEBSERVER
      if (url && Tinyc && hn >= 1 && hn <= TC_MAX_WEB_HANDLERS) {
        strlcpy(Tinyc->web_handler_url[hn - 1], url, sizeof(Tinyc->web_handler_url[0]));
        if (hn > Tinyc->web_handler_count) Tinyc->web_handler_count = hn;
        // Register with Tasmota web server — URL persists in Tinyc struct
        Webserver->on(Tinyc->web_handler_url[hn - 1], TinyCWebOnHandlers[hn - 1]);
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: webOn(%d, \"%s\") registered"), hn, url);
      }
#endif
      break;
    }
    case SYS_WEB_HANDLER: {
      // Returns current handler number during WebOn callback
      TC_PUSH(vm, Tinyc ? Tinyc->current_web_handler : 0);
      break;
    }
    case SYS_WEB_ARG: {
      // Get HTTP request argument: webArg("name", buf) -> length
      int32_t buf_ref = TC_POP(vm);   // char array ref for result
      int32_t ci = TC_POP(vm);        // arg name const index
      int32_t result = 0;
      const char *argname = tc_get_const_str(vm, ci);
#ifdef USE_WEBSERVER
      if (argname && Webserver->hasArg(String(argname))) {
        String val = Webserver->arg(String(argname));
        int32_t *buf = tc_resolve_ref(vm, buf_ref);
        int32_t maxLen = tc_ref_maxlen(vm, buf_ref);
        if (buf && maxLen > 0) {
          int slen = val.length();
          if (slen >= maxLen) slen = maxLen - 1;
          for (int i = 0; i < slen; i++) buf[i] = (int32_t)(uint8_t)val[i];
          buf[slen] = 0;
          result = slen;
        }
      }
#endif
      TC_PUSH(vm, result);
      break;
    }

    case SYS_MDNS: {
      // Register mDNS service: mdns("name", "mac", "type") -> 0
      int32_t ci_type = TC_POP(vm);
      int32_t ci_mac  = TC_POP(vm);
      int32_t ci_name = TC_POP(vm);
      const char *name  = tc_get_const_str(vm, ci_name);
      const char *mac   = tc_get_const_str(vm, ci_mac);
      const char *xtype = tc_get_const_str(vm, ci_type);
      int32_t result = -1;
#if defined(ESP32) || defined(ESP8266)
      if (name && mac && xtype) {
        // Build MAC string
        char mdns_mac[13];
        if (mac[0] == '-') {
          String strMac = NetworkMacAddress();
          strMac.toLowerCase();
          strMac.replace(":", "");
          strlcpy(mdns_mac, strMac.c_str(), sizeof(mdns_mac));
        } else {
          strlcpy(mdns_mac, mac, sizeof(mdns_mac));
        }
        // Build hostname
        char mdns_name[48];
        if (name[0] == '-') {
          strlcpy(mdns_name, NetworkHostname(), sizeof(mdns_name));
        } else {
          strlcpy(mdns_name, name, sizeof(mdns_name));
          strlcat(mdns_name, mdns_mac, sizeof(mdns_name));
        }
        // Start mDNS responder
        const char *cMac = (const char*)mdns_mac;
        String ipStr = NetworkAddress().toString();
        if (MDNS.begin(mdns_name)) {
          if (!strcmp(xtype, "everhome")) {
            MDNS.addService("everhome", "tcp", 80);
            MDNS.addServiceTxt("everhome", "tcp", "ip", ipStr.c_str());
            MDNS.addServiceTxt("everhome", "tcp", "serial", cMac);
            MDNS.addServiceTxt("everhome", "tcp", "productid", "1137");
          } else if (!strcmp(xtype, "shelly")) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addService("shelly", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "fw_id", "20241011-114455/1.4.4-g6d2a586");
            MDNS.addServiceTxt("http", "tcp", "arch", "esp8266");
            MDNS.addServiceTxt("http", "tcp", "id", "");
            MDNS.addServiceTxt("http", "tcp", "gen", "2");
            MDNS.addServiceTxt("shelly", "tcp", "fw_id", "20241011-114455/1.4.4-g6d2a586");
            MDNS.addServiceTxt("shelly", "tcp", "arch", "esp8266");
            MDNS.addServiceTxt("shelly", "tcp", "id", "");
            MDNS.addServiceTxt("shelly", "tcp", "gen", "2");
          } else {
            MDNS.addService(xtype, "tcp", 80);
            MDNS.addServiceTxt(xtype, "tcp", "ip", ipStr.c_str());
            MDNS.addServiceTxt(xtype, "tcp", "serial", cMac);
          }
          AddLog(LOG_LEVEL_INFO, PSTR("TCC: mDNS started, service=%s hostname=%s"), xtype, mdns_name);
          result = 0;
        } else {
          AddLog(LOG_LEVEL_INFO, PSTR("TCC: mDNS failed to start"));
        }
      }
#endif
      TC_PUSH(vm, result);
      break;
    }

    // ── Display drawing (direct renderer calls) ──────
#ifdef USE_DISPLAY
    case SYS_DSP_TEXT: {
      int32_t ref = TC_POP(vm);
      char tbuf[256];
      tc_ref_to_cstr(vm, ref, tbuf, sizeof(tbuf));
      char *savptr = XdrvMailbox.data;
      XdrvMailbox.data = tbuf;
      XdrvMailbox.data_len = strlen(tbuf);
      DisplayText();
      XdrvMailbox.data = savptr;
      break;
    }
    case SYS_DSP_CLEAR:
      if (renderer) renderer->clearDisplay();
      disp_xpos = 0;
      disp_ypos = 0;
      break;
    case SYS_DSP_POS: {
      int32_t y = TC_POP(vm);
      int32_t x = TC_POP(vm);
      disp_xpos = x;
      disp_ypos = y;
      break;
    }
    case SYS_DSP_FONT: {
      int32_t f = TC_POP(vm);
      if (renderer) {
        renderer->setTextFont(f);
        if (f) renderer->setTextSize(1);
      }
      break;
    }
    case SYS_DSP_SIZE: {
      int32_t s = TC_POP(vm);
      if (renderer) renderer->setTextSize(s);
      break;
    }
    case SYS_DSP_COLOR: {
      int32_t cbg = TC_POP(vm);
      int32_t cfg = TC_POP(vm);
      fg_color = (uint16_t)cfg;
      bg_color = (uint16_t)cbg;
      if (renderer) renderer->setTextColor(fg_color, bg_color);
      break;
    }
    case SYS_DSP_DRAW: {
      int32_t ref = TC_POP(vm);
      char tbuf[128];
      tc_ref_to_cstr(vm, ref, tbuf, sizeof(tbuf));
      tc_display_text_padded(tbuf);
      break;
    }
    case SYS_DSP_PIXEL: {
      int32_t y = TC_POP(vm);
      int32_t x = TC_POP(vm);
      if (renderer) renderer->drawPixel(x, y, fg_color);
      break;
    }
    case SYS_DSP_LINE: {
      int32_t y1 = TC_POP(vm);
      int32_t x1 = TC_POP(vm);
      if (renderer) renderer->writeLine(disp_xpos, disp_ypos, x1, y1, fg_color);
      disp_xpos = x1;
      disp_ypos = y1;
      break;
    }
    case SYS_DSP_RECT: {
      int32_t h = TC_POP(vm);
      int32_t w = TC_POP(vm);
      if (renderer) renderer->drawRect(disp_xpos, disp_ypos, w, h, fg_color);
      break;
    }
    case SYS_DSP_FILL_RECT: {
      int32_t h = TC_POP(vm);
      int32_t w = TC_POP(vm);
      if (renderer) renderer->fillRect(disp_xpos, disp_ypos, w, h, fg_color);
      break;
    }
    case SYS_DSP_CIRCLE: {
      int32_t r = TC_POP(vm);
      if (renderer) renderer->drawCircle(disp_xpos, disp_ypos, r, fg_color);
      break;
    }
    case SYS_DSP_FILL_CIRCLE: {
      int32_t r = TC_POP(vm);
      if (renderer) renderer->fillCircle(disp_xpos, disp_ypos, r, fg_color);
      break;
    }
    case SYS_DSP_HLINE: {
      int32_t w = TC_POP(vm);
      if (renderer) renderer->writeFastHLine(disp_xpos, disp_ypos, w, fg_color);
      disp_xpos += w;
      break;
    }
    case SYS_DSP_VLINE: {
      int32_t h = TC_POP(vm);
      if (renderer) renderer->writeFastVLine(disp_xpos, disp_ypos, h, fg_color);
      disp_ypos += h;
      break;
    }
    case SYS_DSP_ROUND_RECT: {
      int32_t r = TC_POP(vm);
      int32_t h = TC_POP(vm);
      int32_t w = TC_POP(vm);
      if (renderer) renderer->drawRoundRect(disp_xpos, disp_ypos, w, h, r, fg_color);
      break;
    }
    case SYS_DSP_FILL_RRECT: {
      int32_t r = TC_POP(vm);
      int32_t h = TC_POP(vm);
      int32_t w = TC_POP(vm);
      if (renderer) renderer->fillRoundRect(disp_xpos, disp_ypos, w, h, r, fg_color);
      break;
    }
    case SYS_DSP_TRIANGLE: {
      int32_t y2 = TC_POP(vm);
      int32_t x2 = TC_POP(vm);
      int32_t y1 = TC_POP(vm);
      int32_t x1 = TC_POP(vm);
      if (renderer) renderer->drawTriangle(disp_xpos, disp_ypos, x1, y1, x2, y2, fg_color);
      break;
    }
    case SYS_DSP_FILL_TRI: {
      int32_t y2 = TC_POP(vm);
      int32_t x2 = TC_POP(vm);
      int32_t y1 = TC_POP(vm);
      int32_t x1 = TC_POP(vm);
      if (renderer) renderer->fillTriangle(disp_xpos, disp_ypos, x1, y1, x2, y2, fg_color);
      break;
    }
    case SYS_DSP_DIM: {
      int32_t val = TC_POP(vm);
      if (renderer) renderer->dim(val);
      break;
    }
    case SYS_DSP_ONOFF: {
      int32_t on = TC_POP(vm);
      DisplayOnOff(on);
      break;
    }
    case SYS_DSP_UPDATE:
      if (renderer) renderer->Updateframe();
      break;
    case SYS_DSP_PICTURE: {
      int32_t scale = TC_POP(vm);
      int32_t ci = TC_POP(vm);
      const char *fname = tc_get_const_str(vm, ci);
      if (fname) {
        Draw_RGB_Bitmap((char*)fname, disp_xpos, disp_ypos, (uint8_t)scale, false, 0, 0);
      }
      break;
    }
    case SYS_DSP_WIDTH:
      TC_PUSH(vm, renderer ? renderer->width() : 0);
      break;
    case SYS_DSP_HEIGHT:
      TC_PUSH(vm, renderer ? renderer->height() : 0);
      break;
    case SYS_DSP_TEXT_STR: {
      int32_t ci = TC_POP(vm);
      const char *cmd = tc_get_const_str(vm, ci);
      if (cmd) {
        char tbuf[256];
        strlcpy(tbuf, cmd, sizeof(tbuf));
        char *savptr = XdrvMailbox.data;
        XdrvMailbox.data = tbuf;
        XdrvMailbox.data_len = strlen(tbuf);
        DisplayText();
        XdrvMailbox.data = savptr;
      }
      break;
    }
    case SYS_DSP_DRAW_STR: {
      int32_t ci = TC_POP(vm);
      const char *str = tc_get_const_str(vm, ci);
      if (str) {
        tc_display_text_padded(str);
      }
      break;
    }
    case SYS_DSP_PAD:
      tc_dsp_pad = (int16_t)TC_POP(vm);
      break;
#else  // !USE_DISPLAY — pop args from stack but do nothing
    case SYS_DSP_TEXT:
    case SYS_DSP_DRAW:
      TC_POP(vm); break;
    case SYS_DSP_CLEAR:
    case SYS_DSP_UPDATE:
      break;
    case SYS_DSP_POS:
    case SYS_DSP_COLOR:
    case SYS_DSP_PIXEL:
    case SYS_DSP_LINE:
    case SYS_DSP_RECT:
    case SYS_DSP_FILL_RECT:
      TC_POP(vm); TC_POP(vm); break;
    case SYS_DSP_FONT:
    case SYS_DSP_SIZE:
    case SYS_DSP_CIRCLE:
    case SYS_DSP_FILL_CIRCLE:
    case SYS_DSP_HLINE:
    case SYS_DSP_VLINE:
    case SYS_DSP_DIM:
    case SYS_DSP_ONOFF:
      TC_POP(vm); break;
    case SYS_DSP_ROUND_RECT:
    case SYS_DSP_FILL_RRECT:
      TC_POP(vm); TC_POP(vm); TC_POP(vm); break;
    case SYS_DSP_TRIANGLE:
    case SYS_DSP_FILL_TRI:
      TC_POP(vm); TC_POP(vm); TC_POP(vm); TC_POP(vm); break;
    case SYS_DSP_PICTURE:
      TC_POP(vm); TC_POP(vm); break;
    case SYS_DSP_WIDTH:
    case SYS_DSP_HEIGHT:
      TC_PUSH(vm, 0); break;
    case SYS_DSP_TEXT_STR:
    case SYS_DSP_DRAW_STR:
    case SYS_DSP_PAD:
      TC_POP(vm); break;
#endif // USE_DISPLAY

    // ── Audio ─────────────────────────────────────────
    case SYS_AUDIO_VOL: {
      int32_t vol = TC_POP(vm);
      char cmd[24];
      snprintf(cmd, sizeof(cmd), "I2SVol %d", vol);
      ExecuteCommand(cmd, SRC_TCL);
      break;
    }
    case SYS_AUDIO_PLAY: {
      int32_t ci = TC_POP(vm);
      const char *file = tc_get_const_str(vm, ci);
      if (file) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "I2SPlay %s", file);
        ExecuteCommand(cmd, SRC_TCL);
      }
      break;
    }
    case SYS_AUDIO_SAY: {
      int32_t ci = TC_POP(vm);
      const char *text = tc_get_const_str(vm, ci);
      if (text) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "I2SSay %s", text);
        ExecuteCommand(cmd, SRC_TCL);
      }
      break;
    }

    // ── Debug ─────────────────────────────────────────
    case SYS_DEBUG_PRINT:
      a = TC_POP(vm);
      tc_output_int(a);
      tc_output_char('\n');
      break;
    case SYS_DEBUG_PRINT_STR:
      a = TC_POP(vm);
      if (a >= 0 && a < vm->const_count && vm->constants[a].type == 1) {
        tc_output_string(vm->constants[a].str.ptr);
        tc_output_char('\n');
      }
      break;
    case SYS_DEBUG_DUMP:
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: PC=%d SP=%d FP=%d Frames=%d"),
        vm->pc - vm->code_offset, vm->sp, vm->fp, vm->frame_count);
      break;

    default:
      AddLog(LOG_LEVEL_ERROR, PSTR("TIC: unknown syscall %d at PC=%d"), id, vm->pc - vm->code_offset);
      return TC_ERR_BAD_SYSCALL;
  }
  return vm->error;
}

/*********************************************************************************************\
 * VM: Load binary
\*********************************************************************************************/

static int tc_vm_load(TcVM *vm, const uint8_t *binary, uint16_t size) {
  // All binary[] reads use TC_READ_BYTE() for flash-safe access
  #define B(i) TC_READ_BYTE(&binary[i])

  if (size < 14) return TC_ERR_BAD_BINARY;  // minimum header size

  uint32_t magic = ((uint32_t)B(0) << 24) | ((uint32_t)B(1) << 16) |
                   ((uint32_t)B(2) << 8) | B(3);
  if (magic != TC_MAGIC) return TC_ERR_BAD_BINARY;

  uint16_t version = (B(4) << 8) | B(5);
  if (version < 2 || version > TC_VERSION) return TC_ERR_BAD_BINARY;

  uint16_t entry_point = (B(8) << 8) | B(9);
  uint16_t const_pool_size = (B(10) << 8) | B(11);
  uint16_t heap_decl_size = (B(12) << 8) | B(13);

  // V3 adds funcTableSize at bytes 14-15; V2 header is 14 bytes
  uint16_t header_size = (version >= 3) ? 16 : 14;
  uint16_t func_table_size = (version >= 3 && size >= 16) ? ((B(14) << 8) | B(15)) : 0;

  if (size < header_size) return TC_ERR_BAD_BINARY;

  // Parse constant pool
  vm->const_count = 0;
  vm->const_data_used = 0;
  uint16_t offset = header_size;
  uint16_t const_end = header_size + const_pool_size;

  while (offset < const_end && vm->const_count < TC_MAX_CONSTANTS) {
    uint8_t type = B(offset); offset++;
    TcConstant *c = &vm->constants[vm->const_count];
    c->type = type;
    if (type == 1) {  // string
      uint16_t len = (B(offset) << 8) | B(offset + 1);
      offset += 2;
      if (vm->const_data_used + len + 1 > TC_MAX_CONST_DATA) break;
      c->str.ptr = &vm->const_data[vm->const_data_used];
      c->str.len = len;
      TC_MEMCPY(&vm->const_data[vm->const_data_used], &binary[offset], len);
      vm->const_data[vm->const_data_used + len] = '\0';
      vm->const_data_used += len + 1;
      offset += len;
    } else if (type == 2) {  // float
      int32_t bits = ((int32_t)B(offset) << 24) | ((int32_t)B(offset+1) << 16) |
                     ((int32_t)B(offset+2) << 8) | B(offset+3);
      c->f = i2f(bits);
      offset += 4;
    }
    vm->const_count++;
  }

  // Close any open file handles from previous run
  tc_close_all_files();

  // Free any previously allocated frame locals and heap
  tc_free_all_frames(vm);
  tc_heap_free_all(vm);

  // Parse heap declarations and pre-allocate blocks
  uint16_t heap_end = const_end + heap_decl_size;
  if (heap_decl_size > 0) {
    uint8_t count = B(const_end);
    // Compute total heap needed
    uint32_t total_heap = 0;
    for (uint8_t i = 0; i < count; i++) {
      uint16_t sz = ((uint16_t)B(const_end + 1 + i * 3 + 1) << 8) |
                     B(const_end + 1 + i * 3 + 2);
      total_heap += sz;
    }
    if (total_heap > 0) {
      uint32_t alloc_size = total_heap > TC_MAX_HEAP ? total_heap : TC_MAX_HEAP;
      vm->heap_data = (int32_t *)calloc(alloc_size, sizeof(int32_t));
      if (!vm->heap_data) return TC_ERR_STACK_OVERFLOW;  // OOM
      // Pre-allocate each declared block
      for (uint8_t i = 0; i < count; i++) {
        uint8_t handle = B(const_end + 1 + i * 3);
        uint16_t sz = ((uint16_t)B(const_end + 1 + i * 3 + 1) << 8) |
                       B(const_end + 1 + i * 3 + 2);
        if (handle < TC_MAX_HEAP_HANDLES) {
          vm->heap_handles[handle].offset = vm->heap_used;
          vm->heap_handles[handle].size = sz;
          vm->heap_handles[handle].alive = true;
          vm->heap_used += sz;
          if ((uint8_t)(handle + 1) > vm->heap_handle_count) vm->heap_handle_count = handle + 1;
        }
      }
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: heap %d handles, %d/%d slots"), count, vm->heap_used, alloc_size);
    }
  }

  // Parse function table (V3)
  vm->callback_count = 0;
  uint16_t func_table_start = heap_end;
  uint16_t func_table_end = func_table_start + func_table_size;
  if (func_table_size > 0) {
    uint16_t pos = func_table_start;
    uint8_t count = B(pos); pos++;
    for (uint8_t i = 0; i < count && i < TC_MAX_CALLBACKS && pos < func_table_end; i++) {
      uint8_t name_len = B(pos); pos++;
      if (name_len >= TC_CALLBACK_NAME_MAX) name_len = TC_CALLBACK_NAME_MAX - 1;
      TC_MEMCPY(vm->callbacks[i].name, &binary[pos], name_len);
      vm->callbacks[i].name[name_len] = '\0';
      pos += name_len;  // skip full name even if truncated
      vm->callbacks[i].address = (B(pos) << 8) | B(pos + 1);
      pos += 2;
      vm->callback_count++;
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: callback '%s' @%d"), vm->callbacks[i].name, vm->callbacks[i].address);
    }
  }

  #undef B

  vm->code = binary;
  vm->code_offset = func_table_end;
  vm->code_size = size - func_table_end;
  vm->pc = vm->code_offset + entry_point;
  vm->sp = 0;
  vm->fp = 0;
  vm->frame_count = 0;
  vm->running = false;
  vm->halted = false;
  vm->delayed = false;
  vm->delay_until = 0;
  vm->error = TC_OK;
  vm->instruction_count = 0;
  memset(vm->globals, 0, sizeof(vm->globals));
  memset(vm->stack, 0, sizeof(vm->stack));

  // Allocate frame 0 for main() — program starts here without OP_CALL
  if (!tc_frame_alloc(&vm->frames[0])) {
    return TC_ERR_STACK_OVERFLOW;  // OOM
  }

  return TC_OK;
}

// Forward declaration — tc_vm_step is defined below, called by tc_vm_call_callback
static int tc_vm_step(TcVM *vm);

/*********************************************************************************************\
 * VM: Callback invocation — call a named function after main() has halted
 *
 * Execution model:
 *   After main() returns, globals & heap persist. Callbacks temporarily un-halt
 *   the VM, allocate a fresh frame, execute the function synchronously, then
 *   re-halt. Instruction limit prevents runaway callbacks.
 *
 * Thread safety (ESP32):
 *   Callbacks are called from Tasmota's main loop (FUNC_JSON_APPEND,
 *   FUNC_WEB_SENSOR, FUNC_EVERY_SECOND). The FreeRTOS task has already
 *   exited (task_running=false) when main() halted, so there is no
 *   concurrent access to the VM state.
\*********************************************************************************************/

static int tc_vm_call_callback(TcVM *vm, const char *name) {
  // Find callback by name
  int idx = -1;
  for (int i = 0; i < vm->callback_count; i++) {
    if (strcmp(vm->callbacks[i].name, name) == 0) { idx = i; break; }
  }
  if (idx < 0) return TC_OK;  // callback not defined, silently skip

  // VM must be halted (main returned) with no error
  if (!vm->halted || vm->error != TC_OK) return vm->error;

  // Save state
  uint8_t saved_frame_count = vm->frame_count;
  uint16_t saved_pc = vm->pc;
  uint16_t saved_sp = vm->sp;

  // Temporarily un-halt and set up callback frame
  vm->halted = false;
  vm->running = true;
  if (vm->frame_count >= TC_MAX_FRAMES) return TC_ERR_FRAME_OVERFLOW;
  TcFrame *frame = &vm->frames[vm->frame_count];
  frame->return_pc = 0;  // detect return by frame_count drop
  if (!tc_frame_alloc(frame)) {
    vm->halted = true;
    vm->running = false;
    return TC_ERR_STACK_OVERFLOW;
  }
  vm->fp = vm->frame_count;
  vm->frame_count++;
  vm->pc = vm->code_offset + vm->callbacks[idx].address;

  // Execute with instruction limit
  uint32_t count = 0;
  while (vm->frame_count > saved_frame_count && !vm->halted && vm->error == TC_OK) {
    int err = tc_vm_step(vm);
    if (err == TC_ERR_PAUSED) {
      // delay() in callback — execute synchronously (short delays only)
      if (vm->delayed && vm->delay_until > millis()) {
        uint32_t wait = vm->delay_until - millis();
        if (wait > 100) wait = 100;  // cap at 100ms to avoid WDT
        delay(wait);
      }
      vm->delayed = false;
      continue;  // resume callback execution after delay
    }
    if (err != TC_OK) break;
    if (++count > TC_CALLBACK_MAX_INSTR) {
      vm->error = TC_ERR_INSTRUCTION_LIMIT;
      break;
    }
  }

  // Restore halted state (globals & heap persist)
  vm->halted = true;
  vm->running = false;
  vm->pc = saved_pc;

  // Clean up any leftover frames from the callback
  while (vm->frame_count > saved_frame_count) {
    tc_frame_free(&vm->frames[--vm->frame_count]);
  }
  vm->fp = vm->frame_count > 0 ? vm->frame_count - 1 : 0;

  // Flush output to Tasmota
  tc_output_flush();

  return vm->error;
}

/*********************************************************************************************\
 * VM: Execute single instruction
\*********************************************************************************************/

static int tc_vm_step(TcVM *vm) {
  if (vm->halted || vm->error != TC_OK) return vm->error;

  // Bounds check PC before reading next instruction
  if (vm->pc < vm->code_offset || vm->pc >= vm->code_offset + vm->code_size) {
    vm->error = TC_ERR_BOUNDS;
    return TC_ERR_BOUNDS;
  }

  uint8_t op = tc_read_u8(vm);
  int32_t a, b;
  float fa, fb;
  uint16_t addr;
  uint8_t idx;

  switch (op) {
    case OP_NOP: break;
    case OP_HALT: vm->halted = true; vm->running = false; break;

    // ── Stack ──────────────────────────────
    case OP_PUSH_I32: TC_PUSH(vm, tc_read_i32(vm)); break;
    case OP_PUSH_F32: TC_PUSHF(vm, tc_read_f32(vm)); break;
    case OP_PUSH_I8:  TC_PUSH(vm, (int32_t)tc_read_i8(vm)); break;
    case OP_PUSH_I16: { int16_t sv = (int16_t)tc_read_u16(vm); TC_PUSH(vm, (int32_t)sv); break; }
    case OP_POP:  TC_POP(vm); break;
    case OP_DUP:  a = TC_PEEK(vm); TC_PUSH(vm, a); break;

    // ── Integer arithmetic ─────────────────
    case OP_ADD: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a+b); break;
    case OP_SUB: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a-b); break;
    case OP_MUL: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a*b); break;
    case OP_DIV: b=TC_POP(vm); a=TC_POP(vm); if(!b) return TC_ERR_DIV_ZERO; TC_PUSH(vm, a/b); break;
    case OP_MOD: b=TC_POP(vm); a=TC_POP(vm); if(!b) return TC_ERR_DIV_ZERO; TC_PUSH(vm, a%b); break;
    case OP_NEG: a=TC_POP(vm); TC_PUSH(vm, -a); break;

    // ── Float arithmetic ───────────────────
    case OP_FADD: fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSHF(vm, fa+fb); break;
    case OP_FSUB: fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSHF(vm, fa-fb); break;
    case OP_FMUL: fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSHF(vm, fa*fb); break;
    case OP_FDIV: fb=TC_POPF(vm); fa=TC_POPF(vm); if(fb==0.0f) return TC_ERR_DIV_ZERO; TC_PUSHF(vm, fa/fb); break;
    case OP_FNEG: fa=TC_POPF(vm); TC_PUSHF(vm, -fa); break;

    // ── Bitwise ────────────────────────────
    case OP_BIT_AND: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a&b); break;
    case OP_BIT_OR:  b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a|b); break;
    case OP_BIT_XOR: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a^b); break;
    case OP_BIT_NOT: a=TC_POP(vm); TC_PUSH(vm, ~a); break;
    case OP_SHL: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a<<b); break;
    case OP_SHR: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a>>b); break;

    // ── Integer comparison ─────────────────
    case OP_EQ:  b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a==b?1:0); break;
    case OP_NEQ: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a!=b?1:0); break;
    case OP_LT:  b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a<b?1:0); break;
    case OP_GT:  b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a>b?1:0); break;
    case OP_LTE: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a<=b?1:0); break;
    case OP_GTE: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, a>=b?1:0); break;

    // ── Float comparison ───────────────────
    case OP_FEQ:  fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSH(vm, fa==fb?1:0); break;
    case OP_FNEQ: fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSH(vm, fa!=fb?1:0); break;
    case OP_FLT:  fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSH(vm, fa<fb?1:0); break;
    case OP_FGT:  fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSH(vm, fa>fb?1:0); break;
    case OP_FLTE: fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSH(vm, fa<=fb?1:0); break;
    case OP_FGTE: fb=TC_POPF(vm); fa=TC_POPF(vm); TC_PUSH(vm, fa>=fb?1:0); break;

    // ── Logical ────────────────────────────
    case OP_LOGIC_AND: b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, (a&&b)?1:0); break;
    case OP_LOGIC_OR:  b=TC_POP(vm); a=TC_POP(vm); TC_PUSH(vm, (a||b)?1:0); break;
    case OP_LOGIC_NOT: a=TC_POP(vm); TC_PUSH(vm, a?0:1); break;

    // ── Control flow ───────────────────────
    case OP_JMP:
      addr = tc_read_u16(vm);
      vm->pc = vm->code_offset + addr;
      break;
    case OP_JZ:
      addr = tc_read_u16(vm);
      a = TC_POP(vm);
      if (a == 0) vm->pc = vm->code_offset + addr;
      break;
    case OP_JNZ:
      addr = tc_read_u16(vm);
      a = TC_POP(vm);
      if (a != 0) vm->pc = vm->code_offset + addr;
      break;

    case OP_CALL:
      addr = tc_read_u16(vm);
      if (vm->frame_count >= TC_MAX_FRAMES) return TC_ERR_FRAME_OVERFLOW;
      {
        TcFrame *frame = &vm->frames[vm->frame_count];
        frame->return_pc = vm->pc;
        if (!tc_frame_alloc(frame)) {
          return TC_ERR_STACK_OVERFLOW;  // OOM
        }
        vm->fp = vm->frame_count;
        vm->frame_count++;
        vm->pc = vm->code_offset + addr;
      }
      break;

    case OP_RET:
      if (vm->frame_count == 0) { tc_frame_free(&vm->frames[0]); vm->halted = true; vm->running = false; break; }
      { TcFrame *f = &vm->frames[--vm->frame_count];
        vm->pc = f->return_pc;
        tc_frame_free(f);  // free returning frame's locals
        vm->fp = vm->frame_count > 0 ? vm->frame_count - 1 : 0; }
      break;

    case OP_RET_VAL:
      a = TC_POP(vm);
      if (vm->frame_count == 0) { tc_frame_free(&vm->frames[0]); TC_PUSH(vm, a); vm->halted = true; vm->running = false; break; }
      { TcFrame *f = &vm->frames[--vm->frame_count];
        vm->pc = f->return_pc;
        tc_frame_free(f);  // free returning frame's locals
        vm->fp = vm->frame_count > 0 ? vm->frame_count - 1 : 0;
        TC_PUSH(vm, a); }
      break;

    // ── Variables (with bounds checks) ─────
    case OP_LOAD_LOCAL:
      idx=tc_read_u8(vm);
      if (idx >= TC_MAX_LOCALS) return TC_ERR_BOUNDS;
      TC_PUSH(vm, vm->frames[vm->fp].locals[idx]); break;
    case OP_STORE_LOCAL:
      idx=tc_read_u8(vm);
      if (idx >= TC_MAX_LOCALS) { TC_POP(vm); return TC_ERR_BOUNDS; }
      vm->frames[vm->fp].locals[idx]=TC_POP(vm); break;
    case OP_LOAD_GLOBAL:
      addr=tc_read_u16(vm);
      if (addr >= TC_MAX_GLOBALS) return TC_ERR_BOUNDS;
      TC_PUSH(vm, vm->globals[addr]); break;
    case OP_STORE_GLOBAL:
      addr=tc_read_u16(vm);
      if (addr >= TC_MAX_GLOBALS) { TC_POP(vm); return TC_ERR_BOUNDS; }
      vm->globals[addr]=TC_POP(vm); break;

    // ── Arrays (with bounds checks) ────────
    case OP_LOAD_LOCAL_ARR:
      idx=tc_read_u8(vm); a=TC_POP(vm);
      if ((uint32_t)(idx+a) >= TC_MAX_LOCALS) return TC_ERR_BOUNDS;
      TC_PUSH(vm, vm->frames[vm->fp].locals[idx+a]); break;
    case OP_STORE_LOCAL_ARR:
      idx=tc_read_u8(vm); b=TC_POP(vm); a=TC_POP(vm);
      if ((uint32_t)(idx+a) >= TC_MAX_LOCALS) return TC_ERR_BOUNDS;
      vm->frames[vm->fp].locals[idx+a]=b; break;
    case OP_LOAD_GLOBAL_ARR:
      addr=tc_read_u16(vm); a=TC_POP(vm);
      if ((uint32_t)(addr+a) >= TC_MAX_GLOBALS) return TC_ERR_BOUNDS;
      TC_PUSH(vm, vm->globals[addr+a]); break;
    case OP_STORE_GLOBAL_ARR:
      addr=tc_read_u16(vm); b=TC_POP(vm); a=TC_POP(vm);
      if ((uint32_t)(addr+a) >= TC_MAX_GLOBALS) return TC_ERR_BOUNDS;
      vm->globals[addr+a]=b; break;

    // ── Type conversion ────────────────────
    case OP_I2F: a=TC_POP(vm); TC_PUSHF(vm, (float)a); break;
    case OP_F2I: fa=TC_POPF(vm); TC_PUSH(vm, (int32_t)fa); break;
    case OP_I2C: a=TC_POP(vm); TC_PUSH(vm, a & 0xFF); break;

    // ── Array address refs (for string functions) ──
    case OP_ADDR_LOCAL:
      idx = tc_read_u8(vm);
      TC_PUSH(vm, tc_make_local_ref(vm->fp, idx));
      break;
    case OP_ADDR_GLOBAL:
      addr = tc_read_u16(vm);
      TC_PUSH(vm, tc_make_global_ref(addr));
      break;

    // ── Heap arrays ───────────────────────
    case OP_LOAD_HEAP_ARR: {
      uint8_t handle = tc_read_u8(vm);
      a = TC_POP(vm);  // index
      if (handle >= TC_MAX_HEAP_HANDLES || !vm->heap_data ||
          !vm->heap_handles[handle].alive ||
          a < 0 || (uint16_t)a >= vm->heap_handles[handle].size) {
        return TC_ERR_BOUNDS;
      }
      TC_PUSH(vm, vm->heap_data[vm->heap_handles[handle].offset + a]);
      break;
    }
    case OP_STORE_HEAP_ARR: {
      uint8_t handle = tc_read_u8(vm);
      b = TC_POP(vm);  // value
      a = TC_POP(vm);  // index
      if (handle >= TC_MAX_HEAP_HANDLES || !vm->heap_data ||
          !vm->heap_handles[handle].alive ||
          a < 0 || (uint16_t)a >= vm->heap_handles[handle].size) {
        return TC_ERR_BOUNDS;
      }
      vm->heap_data[vm->heap_handles[handle].offset + a] = b;
      break;
    }
    case OP_ADDR_HEAP: {
      uint8_t handle = tc_read_u8(vm);
      // Pack: 0xC0000000 | handle
      TC_PUSH(vm, (int32_t)(0xC0000000U | handle));
      break;
    }

    // ── Constants ──────────────────────────
    case OP_LOAD_CONST:
      addr = tc_read_u16(vm);
      if (addr < vm->const_count) {
        if (vm->constants[addr].type == 1) TC_PUSH(vm, addr);
        else TC_PUSHF(vm, vm->constants[addr].f);
      }
      break;

    // ── Syscalls ───────────────────────────
    case OP_SYSCALL:
      idx = tc_read_u8(vm);
      return tc_syscall(vm, idx);

    default:
      vm->error = TC_ERR_BAD_OPCODE;
      return TC_ERR_BAD_OPCODE;
  }
  return vm->error;
}

/*********************************************************************************************\
 * VM: Run N instructions (non-blocking slice)
\*********************************************************************************************/

static int tc_vm_run_slice(TcVM *vm, uint32_t max_instr) {
  vm->running = true;
  uint32_t count = 0;
  uint32_t start_ms = millis();

  while (vm->running && !vm->halted && vm->error == TC_OK && count < max_instr) {
    // Check for non-blocking delay
    if (vm->delayed) {
      if ((int32_t)(millis() - vm->delay_until) >= 0) {
        vm->delayed = false;  // delay elapsed, resume
      } else {
        return TC_OK;  // still waiting, yield back to Tasmota
      }
    }

    int err = tc_vm_step(vm);
    if (err == TC_ERR_PAUSED) {
      // delay() was called, VM is paused
      return TC_OK;
    }
    if (err != TC_OK) return err;
    count++;
    vm->instruction_count++;

    // Time guard: yield back to Tasmota after 10ms max to prevent WDT
    if ((count & 0x3F) == 0) {  // check every 64 instructions
      if (millis() - start_ms > 10) {
        return TC_OK;  // time limit, continue next tick
      }
    }
  }
  return vm->error;
}

/*********************************************************************************************\
 * VM: FreeRTOS task for ESP32 (runs VM in its own task with real blocking delay)
\*********************************************************************************************/

#ifdef ESP32
static void tc_vm_task(void *param) {
  struct TINYC *tc = (struct TINYC *)param;
  TcVM *vm = &tc->vm;
  tc->task_running = true;
  vm->running = true;

  AddLog(LOG_LEVEL_INFO, PSTR("TCC: VM task started"));

  // Phase 1: Execute main()
  while (!tc->task_stop && !vm->halted && vm->error == TC_OK) {
    // Handle delay as real RTOS blocking delay (feeds WDT, yields CPU)
    if (vm->delayed) {
      int32_t remaining = (int32_t)(vm->delay_until - millis());
      if (remaining > 0) {
        // Sleep in small chunks so we can check task_stop
        while (remaining > 0 && !tc->task_stop) {
          int32_t chunk = (remaining > 50) ? 50 : remaining;
          vTaskDelay(chunk / portTICK_PERIOD_MS);
          remaining = (int32_t)(vm->delay_until - millis());
        }
      }
      vm->delayed = false;
      if (tc->task_stop) break;
    }

    // Execute a batch of instructions, then yield
    uint32_t count = 0;
    while (!vm->halted && vm->error == TC_OK && count < 256 && !tc->task_stop) {
      int err = tc_vm_step(vm);
      if (err == TC_ERR_PAUSED) break;   // delay() was called, go back to outer loop
      if (err != TC_OK) break;
      count++;
      vm->instruction_count++;
    }

    yield();  // Feed WDT after each batch
  }

  // Cleanup after main() exits
  tc_free_all_frames(vm);
  tc_close_all_files();
  tc_output_flush();

  if (vm->halted && vm->error == TC_OK) {
    // Normal halt: main() returned successfully.
    // Globals and heap PERSIST for callback functions.
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Halted after %u instr, %d callbacks"),
      vm->instruction_count, vm->callback_count);

    // Phase 2: If TaskLoop callback exists, loop calling it in this task
    int tl_idx = -1;
    for (int i = 0; i < vm->callback_count; i++) {
      if (strcmp(vm->callbacks[i].name, "TaskLoop") == 0) { tl_idx = i; break; }
    }
    if (tl_idx >= 0) {
      AddLog(LOG_LEVEL_INFO, PSTR("TCC: TaskLoop running in task"));
      uint16_t tl_addr = vm->callbacks[tl_idx].address;

      while (!tc->task_stop && vm->error == TC_OK) {
        // Acquire mutex before touching VM state
        if (tc->vm_mutex) xSemaphoreTake(tc->vm_mutex, portMAX_DELAY);

        // Set up callback frame (same as tc_vm_call_callback but inline)
        uint8_t saved_frame_count = vm->frame_count;
        uint16_t saved_pc = vm->pc;
        vm->halted = false;
        vm->running = true;
        if (vm->frame_count < TC_MAX_FRAMES) {
          TcFrame *frame = &vm->frames[vm->frame_count];
          frame->return_pc = 0;
          if (tc_frame_alloc(frame)) {
            vm->fp = vm->frame_count;
            vm->frame_count++;
            vm->pc = vm->code_offset + tl_addr;

            // Execute TaskLoop body with real vTaskDelay for delay()
            uint32_t count = 0;
            while (vm->frame_count > saved_frame_count && !vm->halted && vm->error == TC_OK && !tc->task_stop) {
              int err = tc_vm_step(vm);
              if (err == TC_ERR_PAUSED) {
                // delay() — use real RTOS delay (feeds WDT, yields CPU)
                if (vm->delayed) {
                  // Temporarily restore halted state so main-thread callbacks can run
                  vm->halted = true;
                  vm->running = false;
                  // Release mutex during delay so main thread can run callbacks
                  if (tc->vm_mutex) xSemaphoreGive(tc->vm_mutex);
                  int32_t remaining = (int32_t)(vm->delay_until - millis());
                  while (remaining > 0 && !tc->task_stop) {
                    int32_t chunk = (remaining > 50) ? 50 : remaining;
                    vTaskDelay(chunk / portTICK_PERIOD_MS);
                    remaining = (int32_t)(vm->delay_until - millis());
                  }
                  vm->delayed = false;
                  // Re-acquire mutex to continue execution
                  if (tc->vm_mutex) xSemaphoreTake(tc->vm_mutex, portMAX_DELAY);
                  vm->halted = false;
                  vm->running = true;
                  if (tc->task_stop) break;
                }
                continue;
              }
              if (err != TC_OK) break;
              if (++count > TC_CALLBACK_MAX_INSTR) {
                vm->error = TC_ERR_INSTRUCTION_LIMIT;
                break;
              }
            }

            // Clean up callback frame
            while (vm->frame_count > saved_frame_count) {
              tc_frame_free(&vm->frames[--vm->frame_count]);
            }
            vm->fp = vm->frame_count > 0 ? vm->frame_count - 1 : 0;
          }
        }

        // Restore halted state for other callbacks
        vm->halted = true;
        vm->running = false;
        vm->pc = saved_pc;
        tc_output_flush();

        if (tc->vm_mutex) xSemaphoreGive(tc->vm_mutex);

        if (vm->error != TC_OK || tc->task_stop) break;

        vTaskDelay(1);  // yield at least 1 tick between iterations
      }
      if (tc->task_stop) {
        AddLog(LOG_LEVEL_INFO, PSTR("TCC: TaskLoop stopped"));
      } else if (vm->error != TC_OK) {
        AddLog(LOG_LEVEL_ERROR, PSTR("TCC: TaskLoop error: %s"), tc_error_str(vm->error));
      }
    }
  } else if (vm->error != TC_OK) {
    tc_heap_free_all(vm);
    AddLog(LOG_LEVEL_ERROR, PSTR("TCC: Error: %s (PC=%d)"),
      tc_error_str(vm->error), vm->pc - vm->code_offset);
  } else if (tc->task_stop) {
    tc_heap_free_all(vm);
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Task stopped"));
  }

  tc->running = false;
  vm->running = false;
  tc->task_running = false;
  tc->task_handle = nullptr;
  vTaskDelete(NULL);
}
#endif  // ESP32

#endif  // _XDRV_124_TINYC_VM_H_
