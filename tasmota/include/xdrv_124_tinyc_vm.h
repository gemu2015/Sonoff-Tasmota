/*
  xdrv_124_tinyc_vm.h - TinyC Bytecode VM engine (header-only)

  Separated into .h to avoid Arduino IDE auto-prototype generation issues.
  Included by xdrv_124_tinyc.ino
*/

#ifndef _XDRV_124_TINYC_VM_H_
#define _XDRV_124_TINYC_VM_H_

#ifdef USE_UFILESYS
extern FS *ffsp;
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
  #define TC_MAX_CONSTANTS   64      // constant pool entries
  #define TC_MAX_CONST_DATA  1024    // string constant bytes
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
#define TC_FILE_NAME       "/tinyc.tcb"

// Callback support
#define TC_MAX_CALLBACKS   5           // max well-known callback functions
#ifdef ESP8266
  #define TC_CALLBACK_MAX_INSTR 20000  // instruction limit per callback (ESP8266)
#else
  #define TC_CALLBACK_MAX_INSTR 200000 // instruction limit per callback (ESP32)
#endif
#define TC_CALLBACK_NAME_MAX 16        // max callback name length

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
  // File I/O state (File objects are stored separately as statics — see below)
  bool     file_used[TC_MAX_FILE_HANDLES];
#ifdef ESP32
  // FreeRTOS task for VM execution
  TaskHandle_t task_handle;
  volatile bool task_running;   // task loop is active
  volatile bool task_stop;      // signal task to stop
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

static inline uint8_t tc_read_u8(TcVM *vm) { return vm->code[vm->pc++]; }
static inline int8_t  tc_read_i8(TcVM *vm) { return (int8_t)vm->code[vm->pc++]; }
static inline uint16_t tc_read_u16(TcVM *vm) {
  uint16_t v = ((uint16_t)vm->code[vm->pc] << 8) | vm->code[vm->pc + 1];
  vm->pc += 2; return v;
}
static inline int32_t tc_read_i32(TcVM *vm) {
  int32_t v = ((int32_t)vm->code[vm->pc] << 24) | ((int32_t)vm->code[vm->pc+1] << 16) |
              ((int32_t)vm->code[vm->pc+2] << 8) | vm->code[vm->pc+3];
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
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: digitalWrite(%d, %d)"), a, b);
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
          AddLog(LOG_LEVEL_INFO, PSTR("TCC: gpioInit(%d) releasing from Tasmota GPIO function %d"), a, TasmotaGlobal.gpio_pin[a]);
          TasmotaGlobal.gpio_pin[a] = AGPIO(GPIO_NONE);
        }
        pinMode(a, b);
        AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: gpioInit(%d, %d)"), a, b);
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
      return TC_ERR_BAD_SYSCALL;
  }
  return vm->error;
}

/*********************************************************************************************\
 * VM: Load binary
\*********************************************************************************************/

static int tc_vm_load(TcVM *vm, const uint8_t *binary, uint16_t size) {
  if (size < 14) return TC_ERR_BAD_BINARY;  // minimum header size

  uint32_t magic = ((uint32_t)binary[0] << 24) | ((uint32_t)binary[1] << 16) |
                   ((uint32_t)binary[2] << 8) | binary[3];
  if (magic != TC_MAGIC) return TC_ERR_BAD_BINARY;

  uint16_t version = (binary[4] << 8) | binary[5];
  if (version < 2 || version > TC_VERSION) return TC_ERR_BAD_BINARY;

  uint16_t entry_point = (binary[8] << 8) | binary[9];
  uint16_t const_pool_size = (binary[10] << 8) | binary[11];
  uint16_t heap_decl_size = (binary[12] << 8) | binary[13];

  // V3 adds funcTableSize at bytes 14-15; V2 header is 14 bytes
  uint16_t header_size = (version >= 3) ? 16 : 14;
  uint16_t func_table_size = (version >= 3 && size >= 16) ? ((binary[14] << 8) | binary[15]) : 0;

  if (size < header_size) return TC_ERR_BAD_BINARY;

  // Parse constant pool
  vm->const_count = 0;
  vm->const_data_used = 0;
  uint16_t offset = header_size;
  uint16_t const_end = header_size + const_pool_size;

  while (offset < const_end && vm->const_count < TC_MAX_CONSTANTS) {
    uint8_t type = binary[offset++];
    TcConstant *c = &vm->constants[vm->const_count];
    c->type = type;
    if (type == 1) {  // string
      uint16_t len = (binary[offset] << 8) | binary[offset + 1];
      offset += 2;
      if (vm->const_data_used + len + 1 > TC_MAX_CONST_DATA) break;
      c->str.ptr = &vm->const_data[vm->const_data_used];
      c->str.len = len;
      memcpy(&vm->const_data[vm->const_data_used], &binary[offset], len);
      vm->const_data[vm->const_data_used + len] = '\0';
      vm->const_data_used += len + 1;
      offset += len;
    } else if (type == 2) {  // float
      int32_t bits = ((int32_t)binary[offset] << 24) | ((int32_t)binary[offset+1] << 16) |
                     ((int32_t)binary[offset+2] << 8) | binary[offset+3];
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
    uint8_t count = binary[const_end];
    // Compute total heap needed
    uint32_t total_heap = 0;
    for (uint8_t i = 0; i < count; i++) {
      uint16_t sz = ((uint16_t)binary[const_end + 1 + i * 3 + 1] << 8) |
                     binary[const_end + 1 + i * 3 + 2];
      total_heap += sz;
    }
    if (total_heap > 0) {
      uint32_t alloc_size = total_heap > TC_MAX_HEAP ? total_heap : TC_MAX_HEAP;
      vm->heap_data = (int32_t *)calloc(alloc_size, sizeof(int32_t));
      if (!vm->heap_data) return TC_ERR_STACK_OVERFLOW;  // OOM
      // Pre-allocate each declared block
      for (uint8_t i = 0; i < count; i++) {
        uint8_t handle = binary[const_end + 1 + i * 3];
        uint16_t sz = ((uint16_t)binary[const_end + 1 + i * 3 + 1] << 8) |
                       binary[const_end + 1 + i * 3 + 2];
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
    uint8_t count = binary[pos++];
    for (uint8_t i = 0; i < count && i < TC_MAX_CALLBACKS && pos < func_table_end; i++) {
      uint8_t name_len = binary[pos++];
      if (name_len >= TC_CALLBACK_NAME_MAX) name_len = TC_CALLBACK_NAME_MAX - 1;
      memcpy(vm->callbacks[i].name, &binary[pos], name_len);
      vm->callbacks[i].name[name_len] = '\0';
      pos += name_len;  // skip full name even if truncated
      vm->callbacks[i].address = (binary[pos] << 8) | binary[pos + 1];
      pos += 2;
      vm->callback_count++;
      AddLog(LOG_LEVEL_DEBUG, PSTR("TCC: callback '%s' @%d"), vm->callbacks[i].name, vm->callbacks[i].address);
    }
  }

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
      // No delay() in callbacks — ignore
      vm->delayed = false;
      break;
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

  // Cleanup after task exits
  tc_free_all_frames(vm);
  tc_close_all_files();
  tc_output_flush();

  if (vm->halted && vm->error == TC_OK) {
    // Normal halt: main() returned successfully.
    // Globals and heap PERSIST for callback functions (JsonCall, WebCall, EverySecond).
    // Heap will be freed when program is explicitly stopped, reset, or reloaded.
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: Halted after %u instr, %d callbacks"),
      vm->instruction_count, vm->callback_count);
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
