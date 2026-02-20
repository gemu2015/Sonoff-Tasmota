# TinyC Language Reference

**TinyC** is a subset of C that compiles to bytecode for a stack-based virtual machine.
It runs both in the browser (JavaScript VM) and on ESP32/ESP8266 (as Tasmota driver XDRV_124).

---

## Table of Contents

1. [Data Types](#data-types)
2. [Literals](#literals)
3. [Variables & Scope](#variables--scope)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions](#functions)
7. [Callback Functions](#callback-functions)
8. [Tasmota System Variables](#tasmota-system-variables)
9. [Arrays](#arrays)
10. [Strings](#strings)
11. [Preprocessor](#preprocessor)
12. [Comments](#comments)
13. [Type Casting](#type-casting)
14. [Built-in Functions](#built-in-functions)
15. [VM Limits](#vm-limits)
16. [Keyboard Shortcuts (IDE)](#keyboard-shortcuts-ide)
17. [Examples](#examples)

---

## Data Types

| Type    | Size   | Description                         |
|---------|--------|-------------------------------------|
| `int`   | 32-bit | Signed integer                      |
| `float` | 32-bit | IEEE 754 floating-point             |
| `char`  | 8-bit  | Unsigned character (masked to 0xFF) |
| `bool`  | 32-bit | Boolean (0 = false, non-zero = true)|
| `void`  | —      | No value (function return type)     |

### Type Aliases

| Alias          | Maps to |
|----------------|---------|
| `int32_t`      | `int`   |
| `uint32_t`     | `int`   |
| `unsigned int` | `int`   |
| `uint8_t`      | `char`  |

---

## Literals

### Integer Literals
```c
42          // decimal
0xFF        // hexadecimal (prefix 0x or 0X)
0b1010      // binary (prefix 0b or 0B)
```

### Float Literals
```c
3.14        // decimal point
2.5f        // with float suffix
0.001       // leading zero
```

### Character Literals
```c
'A'         // single character
'\n'        // escape sequence
'\0'        // null terminator
```

**Supported escape sequences:** `\n` `\t` `\r` `\\` `\'` `\"` `\0`

### String Literals
```c
"Hello"             // simple string
"Line 1\nLine 2"    // with escape sequences
```
String literals are used for `char` array initialization and as arguments to string functions.

### Boolean Literals
```c
true        // evaluates to 1
false       // evaluates to 0
```

---

## Variables & Scope

### Global Variables
Declared outside any function. Accessible from all functions.
```c
int counter = 0;
float pi = 3.14;
char buffer[64];
```

### Local Variables
Declared inside functions or blocks. Block-scoped (new scope per `{ }`).
```c
void myFunc() {
    int x = 10;        // local to myFunc
    if (x > 5) {
        int y = 20;    // local to this block
    }
    // y is not accessible here
}
```

### Function Parameters
Passed by value for scalars, by reference for arrays.
```c
void process(int value, int data[]) {
    // value is a copy, data is a reference
}
```

---

## Operators

### Arithmetic
| Op  | Description    | Types              |
|-----|----------------|--------------------|
| `+` | Addition       | int, float, char[] |
| `-` | Subtraction    | int, float         |
| `*` | Multiplication | int, float         |
| `/` | Division       | int, float         |
| `%` | Modulo         | int only           |
| `-` | Unary negation | int, float         |

**Note:** For `char[]` variables, `+` performs string concatenation (see [Strings](#strings)).

### Comparison
| Op   | Description        |
|------|--------------------|
| `==` | Equal              |
| `!=` | Not equal          |
| `<`  | Less than          |
| `>`  | Greater than       |
| `<=` | Less than or equal |
| `>=` | Greater or equal   |

### Logical
| Op     | Description                    |
|--------|--------------------------------|
| `&&`   | Logical AND (short-circuit)    |
| `\|\|` | Logical OR (short-circuit)     |
| `!`    | Logical NOT                    |

### Bitwise
| Op  | Description |
|-----|-------------|
| `&` | AND         |
| `\|`| OR          |
| `^` | XOR         |
| `~` | NOT         |
| `<<`| Left shift  |
| `>>`| Right shift |

### Assignment
| Op  | Description                                       |
|-----|---------------------------------------------------|
| `=` | Assign (for `char[]`: string copy)                |
| `+=`| Add and assign (for `char[]`: string append)      |
| `-=`| Subtract and assign                               |
| `*=`| Multiply and assign                               |
| `/=`| Divide and assign                                 |

### Increment / Decrement
```c
++x     // pre-increment
--x     // pre-decrement
x++     // post-increment
x--     // post-decrement
```

### Operator Precedence (highest to lowest)

1. Postfix: `x++` `x--` `a[i]` `f()` `(type)`
2. Unary: `++x` `--x` `-x` `!x` `~x`
3. Multiplicative: `*` `/` `%`
4. Additive: `+` `-`
5. Shift: `<<` `>>`
6. Relational: `<` `>` `<=` `>=`
7. Equality: `==` `!=`
8. Bitwise AND: `&`
9. Bitwise XOR: `^`
10. Bitwise OR: `|`
11. Logical AND: `&&`
12. Logical OR: `||`
13. Assignment: `=` `+=` `-=` `*=` `/=`

---

## Control Flow

### if / else
```c
if (condition) {
    // ...
}

if (condition) {
    // ...
} else {
    // ...
}

if (a > 0) {
    // ...
} else if (a == 0) {
    // ...
} else {
    // ...
}
```

### while Loop
```c
while (condition) {
    // ...
    if (done) break;
    if (skip) continue;
}
```

### for Loop
```c
for (int i = 0; i < 10; i++) {
    // ...
}

// all parts optional:
for (;;) {
    // infinite loop
    break;
}
```

### switch / case
```c
switch (value) {
    case 1:
        // ... fall-through!
    case 2:
        // ...
        break;
    default:
        // ...
        break;
}
```
**Note:** Cases fall through unless `break` is used (like standard C).

### break / continue
- `break;` — exit the innermost loop or switch
- `continue;` — skip to the next iteration of the innermost loop

---

## Functions

### Declaration
```c
int add(int a, int b) {
    return a + b;
}

void doSomething() {
    // no return value needed
}
```

### Entry Point
Every program must have a `main()` function:
```c
int main() {
    // program starts here
    return 0;
}
```

### Recursion
Fully supported:
```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

### Array Parameters
Arrays are passed by reference:
```c
void fill(int arr[], int size, int value) {
    for (int i = 0; i < size; i++) {
        arr[i] = value;
    }
}
```

---

## Callback Functions

TinyC supports **callback functions** that Tasmota calls automatically at specific events.
Simply define functions with these well-known names — no registration needed.

### Available Callbacks

| Function | Tasmota Hook | When Called | Use Case |
|----------|-------------|-------------|----------|
| `EveryLoop()` | FUNC_LOOP | Every main loop iteration (~1–5 ms) | Ultra-fast polling, bit-banging, time-critical I/O |
| `Every50ms()` | FUNC_EVERY_50_MSECOND | Every 50 ms (20x/sec) | Fast polling, radio receive, sensor sampling |
| `EverySecond()` | FUNC_EVERY_SECOND | Every 1 second | Periodic tasks, counters, slow polling |
| `JsonCall()` | FUNC_JSON_APPEND | Telemetry cycle (~300s) | Add JSON to MQTT telemetry |
| `WebPage()` | FUNC_WEB_ADD_MAIN_BUTTON | Page load (once) | Charts, custom HTML, scripts |
| `WebCall()` | FUNC_WEB_SENSOR | Web page refresh (~1s) | Add sensor rows to Tasmota web UI |
| `UdpCall()` | UDP packet received | On each multicast variable | Process incoming UDP variables |
| `TaskLoop()` | FreeRTOS task (ESP32) | Continuous loop in own task | Background processing, independent of main thread |

### Execution Model

1. **`main()`** runs first in a FreeRTOS task (ESP32) — `delay()` works as real blocking delay
2. After main halts, **globals and heap persist** — they are NOT freed
3. Tasmota periodically calls your callbacks, which can read/modify globals
4. Callbacks run synchronously with an instruction limit — no `delay()` allowed
5. If `TaskLoop()` is defined, it runs in the same FreeRTOS task after main() halts — `delay()` works, runs independently of Tasmota's main thread

### Tasmota Output Functions

Use these functions in callbacks to send data to Tasmota:

| Function | Description | Use In |
|----------|-------------|--------|
| `responseAppend(buf)` | Append char array to JSON telemetry (→ `ResponseAppend_P`) | `JsonCall()` |
| `responseAppend("literal")` | Append string literal to JSON telemetry | `JsonCall()` |
| `webSend(buf)` | Send char array to web page (→ `WSContentSend`) | `WebPage()` / `WebCall()` |
| `webSend("literal")` | Send string literal to web page | `WebPage()` / `WebCall()` |
| `webFlush()` | Flush web content buffer to client (→ `WSContentFlush`) | `WebPage()` / `WebCall()` |

### Web Page Format

Use Tasmota's `{s}` `{m}` `{e}` macros in `webSend()` to create table rows:
- `{s}` — start row (label column)
- `{m}` — middle (value column)
- `{e}` — end row

Example: `"{s}Temperature{m}25.3 °C{e}"` renders as a labeled row on the web page.

### JSON Telemetry Format

Use `responseAppend()` to add JSON fragments. Start with a comma:
- `",\"Sensor\":{\"Temp\":25}"` appends to the telemetry JSON

### Example

```c
int counter = 0;

void EverySecond() {
    counter++;
}

void JsonCall() {
    // Appends to Tasmota MQTT telemetry JSON
    char buf[64];
    sprintfInt(buf, ",\"TinyC\":{\"Count\":%d}", counter);
    responseAppend(buf);
}

void WebCall() {
    // Adds a row to the Tasmota web page
    char buf[64];
    sprintfInt(buf, "{s}TinyC Counter{m}%d{e}", counter);
    webSend(buf);
}

int main() {
    counter = 0;
    return 0;
}
```

**Result:** After uploading and running, the Tasmota web page shows a "TinyC Counter" row that increments every second, and MQTT telemetry includes `,"TinyC":{"Count":N}`.

### TaskLoop Example (ESP32)

```c
int counter = 0;

void TaskLoop() {
    counter++;
    char buf[64];
    sprintfInt(buf, "TaskLoop count=%d", counter);
    addLog(buf);       // appears in Tasmota console log
    delay(1000);       // real 1-second delay, doesn't block Tasmota
}

void JsonCall() {
    char buf[64];
    sprintfInt(buf, ",\"TinyC\":{\"Count\":%d}", counter);
    responseAppend(buf);
}

int main() {
    addLog("TaskLoop demo starting");
    return 0;
}
```

**Result:** `TaskLoop()` runs independently in a FreeRTOS task, incrementing the counter every second. `JsonCall()` reports the counter in MQTT telemetry. Both run concurrently — the mutex ensures safe VM access.

### Important Notes

- Callbacks must be **fast** — max 200,000 instructions (ESP32) / 20,000 (ESP8266) per invocation
- No `delay()` in callbacks (capped at 100ms if called) — except `TaskLoop()` which supports real delays
- `main()` must return (not loop forever) for callbacks to activate
- Only the eight well-known names above are recognized
- The compiler auto-detects these function names and embeds them in the binary
- `EveryLoop()` runs every main loop iteration (~1–5 ms) — keep it **very short** to avoid blocking Tasmota
- `Every50ms()` is ideal for fast, non-blocking I/O polling (SPI radio, GPIO, etc.)
- Use `WebPage()` for one-time page content (charts, scripts) — called once when page loads
- Use `WebCall()` for sensor-style rows that refresh periodically
- Use `UdpCall()` to process incoming UDP multicast variables
- `TaskLoop()` runs in a dedicated FreeRTOS task (ESP32 only) — can use `delay()` freely, VM access is mutex-serialized with main-thread callbacks

---

## Tasmota System Variables

TinyC provides virtual `tasm_*` variables that read/write Tasmota system state directly. They are used like normal variables — no function calls needed. The compiler translates them to syscalls automatically.

### Available Variables

| Variable | Type | R/W | Description |
|----------|------|-----|-------------|
| `tasm_wifi` | int | read | WiFi status (1 = connected, 0 = disconnected) |
| `tasm_mqttcon` | int | read | MQTT connection status (1 = connected) |
| `tasm_teleperiod` | int | read/write | Telemetry period in seconds (10–3600, clamped) |
| `tasm_uptime` | int | read | Device uptime in seconds |
| `tasm_heap` | int | read | Free heap memory in bytes |
| `tasm_power` | int | read/write | Relay power state (bitmask, write toggles relay) |
| `tasm_dimmer` | int | read/write | Dimmer level 0–100 (write sends Dimmer command) |
| `tasm_temp` | float | read | Temperature from Tasmota sensor (global `TempRead()`) |
| `tasm_hum` | float | read | Humidity from Tasmota sensor (global `HumRead()`) |
| `tasm_hour` | int | read | Current hour (0–23, from RTC) |
| `tasm_minute` | int | read | Current minute (0–59, from RTC) |
| `tasm_second` | int | read | Current second (0–59, from RTC) |
| `tasm_year` | int | read | Current year (e.g. 2026, from RTC) |
| `tasm_month` | int | read | Current month (1–12, from RTC) |
| `tasm_day` | int | read | Day of month (1–31, from RTC) |
| `tasm_wday` | int | read | Day of week (1=Sun, 2=Mon, … 7=Sat) |
| `tasm_cw` | int | read | ISO calendar week (1–53) |

### Usage

```c
// Read system state
if (tasm_wifi) {
    printStr("WiFi connected\n");
}

// Read sensor values (float)
float t = tasm_temp;
float h = tasm_hum;

// Read real-time clock
int h = tasm_hour;       // 0–23
int m = tasm_minute;     // 0–59
int s = tasm_second;     // 0–59
int y = tasm_year;       // e.g. 2026
int mo = tasm_month;     // 1–12
int d = tasm_day;        // 1–31
int wd = tasm_wday;      // 1=Sun..7=Sat
int cw = tasm_cw;        // ISO calendar week 1–53

// Write system state
tasm_teleperiod = 60;    // set telemetry to 60 seconds
tasm_power = 1;          // turn relay ON
tasm_dimmer = 50;        // set dimmer to 50%
```

### Notes

- **No declaration needed** — `tasm_*` names are recognized by the compiler automatically
- **No global slot used** — they don't consume global variable space
- **Read-only enforcement** — writing to read-only variables (e.g., `tasm_wifi = 1`) gives a compile-time error
- **Float type inference** — `tasm_temp` and `tasm_hum` are correctly typed as `float` in expressions
- **Write side-effects** — `tasm_power` executes `Power` command, `tasm_dimmer` executes `Dimmer` command, `tasm_teleperiod` updates Tasmota's Settings directly
- In the browser IDE, all variables return simulated values

### Example — Auto Power Control

```c
void EverySecond() {
    // Turn off relay if temperature too high
    if (tasm_temp > 30.0) {
        tasm_power = 0;
    }

    // Report via web
    char buf[64];
    sprintfFloat(buf, "{s}Temp{m}%.1f C{e}", tasm_temp);
    webSend(buf);
}

int main() {
    tasm_teleperiod = 30;  // fast telemetry for testing
    return 0;
}
```

---

## Arrays

### Declaration & Initialization
```c
int data[10];                       // uninitialized
int primes[5] = {2, 3, 5, 7, 11};  // with initializer
float values[3] = {1.5, 2.5};      // partial init
char name[32] = "TinyC";           // string init (null-terminated)
```

### Access
```c
int x = data[0];       // read
data[3] = 42;          // write
data[i + 1] = data[i]; // computed index
```

### Scope
- **Global arrays** — stored in global data space (up to 255 elements)
- **Local arrays** — stored in the function's local frame (up to 255 elements)
- **Heap arrays** — arrays with more than 255 elements are automatically stored on a dynamic heap

### Large Arrays (Heap)

Arrays larger than 255 elements are **automatically** routed to heap memory by the compiler. No special syntax is needed — the compiler detects the size and allocates on the heap transparently:

```c
float data[2000];      // auto → heap (2000 > 255)
int small[10];         // stays in globals (10 <= 255)

int main() {
    data[1999] = 3.14;  // heap access — same syntax as regular arrays
    small[0] = 42;      // global access
    return 0;
}
```

Heap arrays support all the same operations as regular arrays: element access, string operations on `char[]`, passing to functions, etc.

**Heap limits:**

| Platform | Max Heap Slots | Max Handles |
|----------|---------------|-------------|
| ESP8266  | 2,048 (8 KB)  | 8           |
| ESP32    | 8,192 (32 KB) | 16          |
| Browser  | 16,384 (64 KB)| 32          |

---

## Strings

Strings in TinyC are `char` arrays with null termination.

### Declaration
```c
char greeting[32] = "Hello";
char buffer[64];    // uninitialized buffer
```

### String Assignment & Concatenation with `+`

The `=` and `+=` operators work on `char[]` variables for intuitive string handling:

```c
char buf[64];
char name[16] = "World";

// Assign string literal or char array
buf = "Hello";          // same as strcpy(buf, "Hello")
buf = name;             // same as strcpy(buf, name)

// Append with +=
buf += " ";             // same as strcat(buf, " ")
buf += name;            // same as strcat(buf, name)

// Concatenate with +
buf = buf + "!";        // same as strcat(buf, "!")
buf = buf + name;       // same as strcat(buf, name)
```

**Note:** The `+` operator only works when the left side of `=` is the same variable as the left side of `+` (i.e., `buf = buf + ...`). Cross-variable concatenation like `a = b + c` is not supported — use `strcpy` + `strcat` for that.

### Built-in String Functions
```c
int len = strlen(greeting);             // length (excluding \0)
strcpy(buffer, greeting);               // copy array to array
strcpy(buffer, "World");                // copy literal to array
strcat(buffer, greeting);               // append array
strcat(buffer, "!");                    // append literal
int cmp = strcmp(greeting, buffer);     // compare: -1, 0, or 1
printString(greeting);                  // print string to output
```

### Formatted String Output (sprintf)

Format a single value into a char array:

```c
char line[64];
sprintfInt(line, "x = %d", 42);            // "x = 42"
sprintfFloat(line, "pi = %.2f", 3.14);     // "pi = 3.14"
sprintfStr(line, "name: %s", name);        // "name: World"
```

### Building Multi-Value Strings (sprintfAppend)

Since TinyC has no variadic functions, use `sprintfAppend` variants to chain
multiple values into one buffer. They append at the current end of the string:

```c
char report[128];
sprintfInt(report, "Sensor %d", 1);              // "Sensor 1"
sprintfAppendStr(report, " name=%s", name);       // "Sensor 1 name=World"
sprintfAppendInt(report, " val=%d", 42);          // "Sensor 1 name=World val=42"
sprintfAppendFloat(report, " temp=%.1f", 3.14);   // "Sensor 1 name=World val=42 temp=3.1"
printString(report);
```

| Function | Description |
|----------|-------------|
| `sprintfInt(char dst[], "fmt", int val)` | Format int into dst (overwrites) |
| `sprintfFloat(char dst[], "fmt", float val)` | Format float into dst (overwrites) |
| `sprintfStr(char dst[], "fmt", char src[])` | Format string into dst (overwrites) |
| `sprintfAppendInt(char dst[], "fmt", int val)` | Format int and append to dst |
| `sprintfAppendFloat(char dst[], "fmt", float val)` | Format float and append to dst |
| `sprintfAppendStr(char dst[], "fmt", char src[])` | Format string and append to dst |

**Format specifiers:** `%d` (int), `%f` `%.2f` `%e` `%g` (float), `%s` (string). Each call handles exactly one `%` specifier.

### Character Access
```c
char ch = greeting[0];     // read: 'H'
greeting[0] = 'h';         // write: now "hello"
```

### Escape Sequences in Strings
| Escape | Character      |
|--------|----------------|
| `\n`   | Newline        |
| `\t`   | Tab            |
| `\r`   | Carriage return|
| `\\`   | Backslash      |
| `\"`   | Double quote   |
| `\'`   | Single quote   |
| `\0`   | Null terminator|

---

## Preprocessor

### #define — Compile-Time Constants

Simple compile-time constants (no macro expansion):
```c
#define LED_PIN 5
#define MAX_SIZE 100
#define PI 3.14
#define DOUBLE_PI (PI * 2)
```

**Features:**
- Value must be a constant expression
- Supports arithmetic on other `#define` values: `+`, `-`, `*`, `/`
- Used for array sizes, function arguments, etc.
- Scope: entire program
- Valueless defines allowed for conditionals: `#define ESP32`

**Limitations:**
- No `#include`

### Function-Like Macros

Parameterized macros perform text substitution before compilation:

```c
#define LOG(A) addLog(A)
#define CLAMP(V, MX) min(max(V, 0), MX)
#define SQUARE(X) (X * X)
```

**Usage:**
```c
LOG("sensor init");          // → addLog("sensor init")
int v = CLAMP(reading, 100); // → int v = min(max(reading, 0), 100)
int s = SQUARE(5);           // → int s = (5 * 5)
```

**Features:**
- Parameters are replaced by whole-word matching (won't replace partial identifiers)
- Nested parentheses in arguments are handled correctly: `LOG(foo(1,2))` works
- String literal arguments are preserved: `LOG("hello, world")` — the comma inside quotes is not treated as an argument separator
- Nested macro expansion: macros in the expanded body are expanded (up to 10 iterations)
- Multiple parameters supported: `#define ADD(A, B) (A + B)`

**Empty body macros — debug stripping:**
```c
#define DBG(M)              // empty body — no replacement text

DBG("checkpoint 1");        // → stripped entirely (including semicolon)
int x = 42;                 // this line is unaffected
```

Empty-body macros remove the entire invocation including the trailing semicolon. This is useful for stripping debug calls in production builds:

```c
#ifdef DEBUG
  #define DBG(M) addLog(M)
#else
  #define DBG(M)
#endif

DBG("init done");  // logs in debug, stripped in release
```

### Conditional Compilation

```c
#define ESP32
#define USE_SENSOR

#ifdef ESP32
  int pin = 8;       // included — ESP32 is defined
#else
  int pin = 2;       // excluded
#endif

#ifndef USE_DISPLAY
  // included — USE_DISPLAY is not defined
#endif
```

| Directive               | Description                                      |
|-------------------------|--------------------------------------------------|
| `#define NAME`          | Define a name (no value, for conditionals)       |
| `#define NAME value`    | Define a name with a constant value              |
| `#define NAME(A) body`  | Function-like macro with text substitution       |
| `#undef NAME`          | Undefine a previously defined name               |
| `#ifdef NAME`          | Include block if NAME is defined                 |
| `#ifndef NAME`         | Include block if NAME is NOT defined             |
| `#if EXPR`             | Include block if expression is non-zero          |
| `#else`                | Alternative block                                |
| `#endif`               | End conditional block                            |

**`#if` expressions** support:
- Integer literals: `#if 1`, `#if 0`
- Defined names (1 if defined, 0 if not): `#if ESP32`
- `defined(NAME)` operator: `#if defined(ESP32)`
- Logical operators: `&&`, `||`, `!`
- Comparison: `==`, `!=`, `>`, `<`, `>=`, `<=`
- Parentheses for grouping

```c
#if defined(ESP32) && !defined(USE_LEGACY)
  // ESP32-specific modern code
#endif
```

**Notes:**
- Conditionals can be nested
- Skipped code is not compiled (does not need to be valid syntax)
- Line numbers in error messages are preserved

---

## Comments

```c
// Single-line comment

/* Multi-line
   comment */
```

---

## Type Casting

### Explicit Casts
```c
float f = 3.14;
int i = (int)f;         // truncates to 3

int x = 42;
float y = (float)x;     // converts to 42.0

int ch = 321;
char c = (char)ch;      // masks to 0xFF → 65 ('A')

int b = (bool)42;       // non-zero → 1
```

### Implicit Conversions
When mixing `int` and `float` in an expression, the `int` operand is automatically promoted to `float`:
```c
int a = 5;
float b = 2.5;
float c = a + b;    // a promoted to float, result = 7.5
```

---

## Built-in Functions

### Output

| Function                | Description                      |
|-------------------------|----------------------------------|
| `print(int value)`      | Print integer + newline          |
| `printStr("literal")`   | Print string literal             |
| `printString(char arr[])` | Print null-terminated char array |

### GPIO

| Function                             | Description                          |
|--------------------------------------|--------------------------------------|
| `pinMode(int pin, int mode)`         | Set pin mode (0=INPUT, 1=OUTPUT)     |
| `digitalWrite(int pin, int value)`   | Write HIGH(1) or LOW(0)             |
| `int digitalRead(int pin)`           | Read pin state                       |
| `int analogRead(int pin)`            | Read analog value (0–4095)           |
| `analogWrite(int pin, int value)`    | Write PWM value                      |
| `gpioInit(int pin, int mode)`        | Release pin from Tasmota + pinMode   |

### Timing

| Function                         | Description                     |
|----------------------------------|---------------------------------|
| `delay(int ms)`                  | Wait milliseconds               |
| `delayMicroseconds(int us)`      | Wait microseconds               |
| `int millis()`                   | Milliseconds since program start|
| `int micros()`                   | Microseconds since program start|

### Serial

| Function                          | Description                        |
|-----------------------------------|------------------------------------|
| `serialBegin(int baud)`           | Initialize serial at baud rate     |
| `serialPrint("literal")`          | Print string to serial             |
| `serialPrintInt(int value)`       | Print integer to serial            |
| `serialPrintFloat(float value)`   | Print float to serial              |
| `serialPrintln("literal")`        | Print string + newline to serial   |
| `int serialRead()`                | Read byte (-1 if none available)   |
| `int serialAvailable()`           | Bytes available to read            |

### Math

| Function                                            | Description                     |
|-----------------------------------------------------|---------------------------------|
| `int abs(int value)`                                | Absolute value                  |
| `int min(int a, int b)`                             | Minimum of two values           |
| `int max(int a, int b)`                             | Maximum of two values           |
| `int map(int val, int fLo, int fHi, int tLo, int tHi)` | Map value from one range to another |
| `int random(int min, int max)`                      | Random integer in range         |
| `float sqrt(float x)`                               | Square root                     |
| `float sin(float x)`                                | Sine (radians)                  |
| `float cos(float x)`                                | Cosine (radians)                |

### String

| Function                             | Description                         |
|--------------------------------------|-------------------------------------|
| `int strlen(char arr[])`             | String length (excluding null)      |
| `strcpy(char dst[], char src[])`     | Copy string                         |
| `strcpy(char dst[], "literal")`      | Copy literal into array             |
| `strcat(char dst[], char src[])`     | Concatenate string                  |
| `strcat(char dst[], "literal")`      | Concatenate literal                 |
| `int strcmp(char a[], char b[])`     | Compare: returns -1, 0, or 1       |
| `printString(char arr[])`            | Print string to output              |

**String operators:** `char[]` variables also support `=`, `+=`, and `+` for string assignment and concatenation — see [Strings](#strings) section.

### sprintf — Formatted Strings

Format a single value into a char array. Each function handles one `%` specifier.

| Function | Description |
|----------|-------------|
| `int sprintfInt(char dst[], "fmt", int val)` | Format int into dst (overwrites) |
| `int sprintfFloat(char dst[], "fmt", float val)` | Format float into dst (overwrites) |
| `int sprintfStr(char dst[], "fmt", char src[])` | Format string into dst (overwrites) |
| `int sprintfAppendInt(char dst[], "fmt", int val)` | Format int, append to end of dst |
| `int sprintfAppendFloat(char dst[], "fmt", float val)` | Format float, append to end of dst |
| `int sprintfAppendStr(char dst[], "fmt", char src[])` | Format string, append to end of dst |

**Format specifiers:** `%d` (int), `%f` `%.Nf` `%e` `%g` (float), `%s` (string).
All functions return the total string length.

```c
// Build a multi-value string by chaining Append calls:
char buf[128];
sprintfInt(buf, "ID=%d", 1);
sprintfAppendStr(buf, " name=%s", name);
sprintfAppendFloat(buf, " val=%.1f", 3.14);
// buf = "ID=1 name=World val=3.1"
```

### File I/O

Read and write files on the ESP32 filesystem (LittleFS). In the browser IDE, files are simulated in a virtual filesystem.

| Function                                   | Description                                      |
|--------------------------------------------|--------------------------------------------------|
| `int fileOpen("path", mode)`               | Open file, returns handle (0–3) or -1 on error   |
| `int fileClose(handle)`                    | Close file handle, returns 0 or -1               |
| `int fileRead(handle, char buf[], max)`    | Read up to max bytes into buf, returns count     |
| `int fileWrite(handle, char buf[], len)`   | Write len bytes from buf, returns count          |
| `int fileExists("path")`                   | Check if file exists: 1=yes, 0=no                |
| `int fileDelete("path")`                   | Delete file, returns 0=ok, -1=error              |
| `int fileSize("path")`                     | Get file size in bytes, -1 on error              |

**File modes:** `0` = read, `1` = write (create/truncate), `2` = append

**Notes:**
- File paths must be string literals (e.g., `"/data.txt"`)
- Maximum 4 files open simultaneously (ESP32), 8 in browser
- Buffer arguments (`buf`) must be `char` arrays, not string literals
- `fileRead` returns the number of bytes actually read (may be less than `max`)
- Always close files when done to free handles

```c
// Example: Write and read back
char data[32];
char buf[32];
strcpy(data, "Hello!\n");

int f = fileOpen("/test.txt", 1);   // write mode
fileWrite(f, data, strlen(data));
fileClose(f);

f = fileOpen("/test.txt", 0);       // read mode
int n = fileRead(f, buf, 31);
buf[n] = 0;
fileClose(f);
printString(buf);                    // prints "Hello!"

fileDelete("/test.txt");             // clean up
```

### Tasmota Command

Execute any Tasmota console command and capture the JSON response.

| Function                                     | Description                                    |
|----------------------------------------------|------------------------------------------------|
| `int tasmCmd("command", char response[])`    | Execute command, store response, return length |

**Notes:**
- Command must be a string literal (e.g., `"Status 0"`, `"Power ON"`)
- Response buffer should be a `char` array (recommended size: 256)
- Returns length of response string, or -1 on error
- In the browser IDE, returns a simulated mock response
- On ESP32, executes real Tasmota commands and captures the JSON response

```c
char resp[256];
int len = tasmCmd("Status 0", resp);
if (len > 0) {
    printString(resp);   // prints JSON response
}
```

### Tasmota Output (Callbacks)

Send data directly to Tasmota's telemetry and web systems from callback functions.

| Function | Description |
|----------|-------------|
| `void responseAppend(char buf[])` | Append string to MQTT JSON telemetry (`ResponseAppend_P`) |
| `void responseAppend("literal")` | Append string literal to JSON (no buffer needed) |
| `void webSend(char buf[])` | Send string to web page HTML (`WSContentSend`) |
| `void webSend("literal")` | Send string literal to web page (no buffer needed) |
| `void webFlush()` | Flush web content buffer to client (`WSContentFlush`) |
| `void addLog(char buf[])` | Write message to Tasmota log (`AddLog` at INFO level) |
| `void addLog("literal")` | Write string literal to Tasmota log |

**Notes:**
- `addLog`, `webSend` and `responseAppend` accept either a char array or a string literal
- String literal variants are more efficient — no copy through a buffer, sent directly from constant pool
- Use `responseAppend()` inside `JsonCall()` — appends to the MQTT telemetry JSON
- Use `webSend()` inside `WebPage()` for one-time page content (charts, scripts, custom HTML)
- Use `webSend()` inside `WebCall()` for sensor-style rows that refresh periodically
- Use `{s}Label{m}Value{e}` format in `webSend()` for sensor-style table rows
- Call `webFlush()` periodically when building large HTML pages to flush the chunked transfer buffer (500 bytes)
- Start JSON with comma: `",\"Key\":value"` to append correctly to telemetry
- In the browser IDE, both route to the output console; `webFlush()` is a no-op
- Callback instruction limit: 200,000 (ESP32), 20,000 (ESP8266)
- See [Callback Functions](#callback-functions) for full examples

### UDP Multicast (Scripter-compatible)

Share float variables between Tasmota devices via UDP multicast on 239.255.255.250:1999.
Compatible with Tasmota Scripter's global variable protocol.

| Function | Description |
|----------|-------------|
| `void udpSend("name", float_val)` | Broadcast a float variable via binary multicast |
| `float udpRecv("name")` | Get last received value for named variable (0 if none) |
| `int udpReady("name")` | Returns 1 if new value received since last check |
| `void udpSendArray("name", float_arr, count)` | Broadcast a float array via binary multicast |
| `int udpRecvArray("name", float_arr, maxcount)` | Receive float array, returns actual count |

**Protocol:**
- Single float: send `=>name:[4 bytes IEEE-754 float]`
- Float array: send `=>name:[2-byte LE count][N × 4-byte float]`
- Receive: both ASCII (`=>name=value`) and binary (single or array)
- Multicast group: `239.255.255.250`, port `1999`
- Max 8 tracked variable names, 16 chars each
- Max 64 floats per array

**Callback:** Define `void UdpCall()` to be notified on each received variable.
UDP socket is auto-initialized on first `udpSend()` or `udpRecv()` call.

**Example (scalar):**
```c
float temperature = 0.0;

void EverySecond() {
    temperature = 20.0 + sin(counter) * 5.0;
    udpSend("temperature", temperature);
}

void UdpCall() {
    float remote = udpRecv("temperature");
    // process remote value...
}
```

**Example (array):**
```c
float sensors[8];

void EverySecond() {
    // Send 8 sensor values as array
    udpSendArray("sensors", sensors, 8);
}

void UdpCall() {
    float remote[8];
    int n = udpRecvArray("sensors", remote, 8);
    // n = number of floats actually received
}
```

### I2C Bus

Direct I2C bus access for sensor drivers (requires `USE_I2C`). All functions take `bus` as the last parameter (0 or 1).

| Function | Description |
|----------|-------------|
| `int i2cExists(int addr, int bus)` | Check if device responds at address. Returns 1 if found |
| `int i2cRead8(int addr, int reg, int bus)` | Read single byte from register. Returns byte value (0–255) |
| `int i2cWrite8(int addr, int reg, int val, int bus)` | Write single byte to register. Returns 1=ok, 0=fail |
| `int i2cRead(int addr, int reg, char buf[], int len, int bus)` | Read `len` bytes into char array. Returns 1=ok |
| `int i2cWrite(int addr, int reg, char buf[], int len, int bus)` | Write `len` bytes from char array. Returns 1=ok |
| `int i2cRead0(int addr, char buf[], int len, int bus)` | Read `len` bytes without register. Returns 1=ok |
| `int i2cWrite0(int addr, int reg, int bus)` | Write register byte only (no data). Returns 1=ok |

**Notes:**
- `bus` = 0 or 1 — selects which I2C bus to use
- Address is 7-bit (0x00–0x7F), e.g. `0x48` for TMP102
- Register is 8-bit (0x00–0xFF)
- Buffer functions use `char[]` arrays — each element holds one byte (0–255)
- Maximum buffer length is 255 bytes
- Returns 0 if I2C is not compiled in or the operation fails

**Example — Read TMP102 temperature sensor on bus 0:**
```c
#define TMP102_ADDR  0x48
#define TMP102_TEMP  0x00
#define I2C_BUS      0

void EverySecond() {
    if (!i2cExists(TMP102_ADDR, I2C_BUS)) return;

    char buf[2];
    if (i2cRead(TMP102_ADDR, TMP102_TEMP, buf, 2, I2C_BUS)) {
        // TMP102: 12-bit temp in upper bits of 2 bytes
        int raw = (buf[0] << 4) | (buf[1] >> 4);
        if (raw > 2047) raw = raw - 4096;  // sign extend
        float temp = (float)raw * 0.0625;

        char out[64];
        sprintfFloat(out, "TMP102: %.2f °C\n", temp);
        printString(out);
    }
}
```

### Smart Meter (SML)

Read meter values and control meters via Tasmota's SML driver (requires `USE_SML` or `USE_SML_M`).

SML can run **without Scripter** — only `USE_UFILESYS` is needed for file-based meter descriptors.
The IDE's SML Descriptor tab manages the meter definition file (`/sml_meter.def`) on the device.

#### Reading Meter Values

| Function | Description |
|----------|-------------|
| `float smlGet(int index)` | Get meter value. Index 0 returns count, 1..N returns values |
| `int smlGetStr(int index, char buf[])` | Get meter ID string into buffer, returns length |

**Notes:**
- Index is 1-based: `smlGet(1)` returns the first meter value
- `smlGet(0)` returns the total number of meter variables
- Returns 0 if SML is not compiled in or index is out of range
- Values are the same as Scripter's `sml[x]` syntax

**Example:**
```c
void WebCall() {
    char buf[64];
    int n = smlGet(0);  // total meters
    int i = 1;
    while (i <= n) {
        float val = smlGet(i);
        sprintfFloat(buf, "{s}Meter %d{m}%.2f{e}", val);
        webSend(buf);
        i++;
    }
}
```

#### Advanced Meter Control

These functions require `USE_SML_SCRIPT_CMD` to be enabled in the firmware.

| Function | Description |
|----------|-------------|
| `int smlWrite(int meter, char buf[])` | Send hex sequence to meter (e.g. wake-up or request commands) |
| `int smlWrite(int meter, "hex")` | Same, with string literal (no temp buffer needed) |
| `int smlRead(int meter, char buf[])` | Read raw meter buffer into char array, returns bytes read |
| `int smlSetBaud(int meter, int baud)` | Change baud rate of a meter's serial port |
| `int smlSetWStr(int meter, char buf[])` | Set async write string for next scheduled send |
| `int smlSetWStr(int meter, "hex")` | Same, with string literal |
| `int smlSetOptions(int options)` | Set SML global options bitmask |
| `int smlGetV(int sel)` | Get/reset data valid flags (0=get, 1=reset) |

**Notes:**
- `meter` is the 1-based meter index from the SML descriptor
- `smlWrite` and `smlSetWStr` accept either a `char[]` array or a string literal — the compiler auto-detects which variant to use
- `smlWrite` sends a hex-encoded byte sequence (e.g. `"AA0100"`) to the meter's serial port
- `smlRead` copies the raw receive buffer into a char array for custom parsing
- `smlSetBaud` dynamically changes the meter's baud rate (useful for meters that require speed negotiation)
- `smlSetWStr` sets a hex string to be sent on the next scheduled meter poll cycle
- These functions replace Scripter's `>F`/`>S` section meter control commands

**Example — OBIS meter wake-up sequence:**
```c
void EverySecond() {
    // String literal — no temp buffer needed
    smlWrite(1, "2F3F210D0A");  // "/?!\r\n" in hex
}
```

**Example — Dynamic baud rate negotiation:**
```c
void EverySecond() {
    // Read meter response
    char buf[64];
    int n = smlRead(1, buf);
    if (n > 0 && buf[0] == 0x06) {
        // ACK received, switch to high speed
        smlSetBaud(1, 9600);
    }
}
```

#### SML Descriptor Editor (IDE)

The IDE includes an **SML Descriptor** tab in the left pane for managing meter definitions:

- **Meter database**: A dropdown loads `.tas` meter definitions from the [community database](https://github.com/ottelo9/tasmota-sml-script)
- **Custom meter URL**: The database URL is read from `/sml_meter_url.txt` on the device filesystem. To use a different meter repository, edit this file with a URL pointing to a directory containing a `smartmeter.json` index file. The default URL points to the community GitHub repository.
- **RX/TX pin selection**: Dropdowns populated from the device's free GPIOs (via `freegpio` API)
- **Pin placeholders**: `%0rxpin%` and `%0txpin%` in descriptors are replaced with selected pins on save
- **Save to Device**: Extracts only the `>M` section and saves it as `/sml_meter.def`
- **Load from Device**: Reads the current `/sml_meter.def` from the device

#### Callback Merge

Many `.tas` meter files require periodic code (Scripter's `>S` and `>F` sections) for meter communication, wake-up sequences, or baud rate negotiation. In TinyC, you write these as callback functions directly in the SML editor:

```
void EverySecond() {
    smlWrite(1, "2F3F210D0A");
}

>M 1
+1,3,s,16,9600,SML,1
1,1-0:1.8.0*255(@1,Energy In,kWh,E_in,3
#
```

**How it works:**
1. Write TinyC callback functions (`EverySecond()`, `Every100ms()`, etc.) anywhere in the SML editor — before or after the `>M` section
2. On **Save**, only the `>M` section goes to `/sml_meter.def` on the device
3. On **Compile**, the IDE automatically merges SML callbacks into the main program:
   - If the main editor already has the same callback — the SML code is appended to the existing function body
   - If the main editor doesn't have it — a new callback function is created
4. The merged source is compiled as one program — SML code and main code share the same globals and functions

### SPI Bus

Direct SPI bus access for sensors and displays. Supports both hardware SPI (using Tasmota-configured pins) and software bitbang on arbitrary GPIO pins.

| Function | Description |
|----------|-------------|
| `int spiInit(int sclk, int mosi, int miso, int speed_mhz)` | Initialize SPI bus. Returns 1=ok |
| `spiSetCS(int index, int pin)` | Set chip select pin for slot index (1–4) |
| `int spiTransfer(int cs, char buf[], int len, int mode)` | Transfer bytes. Returns bytes transferred |

**`spiInit` pin modes:**
- `sclk = -1` — Use Tasmota's primary hardware SPI bus (GPIO configured in Tasmota)
- `sclk = -2` — Use HSPI secondary hardware SPI bus (ESP32 only)
- `sclk >= 0` — Bitbang mode using GPIO pins (`sclk`, `mosi`, `miso`)
- Set `mosi` or `miso` to -1 if not needed (e.g. read-only or write-only device)
- `speed_mhz` sets clock frequency for hardware SPI (ignored for bitbang)

**`spiTransfer` modes:**
| Mode | Description |
|------|-------------|
| 1 | 8-bit per element — each `buf[]` element = 1 byte transferred |
| 2 | 16-bit per element — each `buf[]` element = 2 bytes (MSB first) |
| 3 | 24-bit per element — each `buf[]` element = 3 bytes (MSB first) |
| 4 | 8-bit with per-byte CS toggle — CS goes low/high for each byte |

**Notes:**
- `cs` parameter is 1-based CS slot index (matching `spiSetCS`). Use 0 for no automatic CS management
- Transfer is full-duplex: `buf[]` is written (MOSI) and read values (MISO) replace each element
- Maximum practical transfer length is limited by your char array size
- SPI resources are automatically cleaned up when the VM stops
- Hardware SPI requires SPI pins configured in Tasmota (Template or Module settings)

**Example — Read MAX31855 thermocouple (SPI, 32-bit read):**
```c
#define CS_PIN  5

int main() {
    spiInit(-1, -1, -1, 4);   // HW SPI at 4 MHz
    spiSetCS(1, CS_PIN);       // CS slot 1 = pin 5

    char buf[4];
    buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 0;
    spiTransfer(1, buf, 4, 1); // read 4 bytes

    // MAX31855: bits 31..18 = 14-bit thermocouple temp
    int raw = ((buf[0] << 8) | buf[1]) >> 2;
    if (raw & 0x2000) raw = raw - 16384;  // sign extend
    float temp = (float)raw * 0.25;

    char out[64];
    sprintfFloat(out, "Thermocouple: %.2f °C\n", temp);
    printString(out);
    return 0;
}
```

### Debug

| Function      | Description                |
|---------------|----------------------------|
| `dumpVM()`    | Dump VM state to console   |

---

## VM Limits

| Resource          | ESP8266  | ESP32    | Browser  | Notes                              |
|-------------------|----------|----------|----------|------------------------------------|
| Stack depth       | 64       | 256      | 256      | Operand stack entries              |
| Call frames       | 8        | 32       | 32       | Maximum recursion / call depth     |
| Locals per frame  | 256      | 256      | 256      | Includes arrays (1 slot per element)|
| Global variables  | 64       | 256      | 256      | Includes global arrays (≤255 elem) |
| Code size         | 4 KB     | 16 KB    | 64 KB    | Bytecode (16-bit addressing)       |
| Heap memory       | 8 KB     | 32 KB    | 64 KB    | For arrays >255 elements + malloc  |
| Heap handles      | 8        | 16       | 32       | Max simultaneous heap allocations  |
| Constant pool     | 32       | 64       | 65536    | String & float constants           |
| Instruction limit | 1M       | 1M       | 1M       | Safety limit per execution         |
| GPIO pins         | 40       | 40       | 40       | Pins 0–39 (simulated in browser)   |
| File handles      | 4        | 4        | 8        | Simultaneously open files          |

---

## Keyboard Shortcuts (IDE)

| Shortcut           | Action              |
|--------------------|---------------------|
| Ctrl + Enter       | Compile             |
| Ctrl + Shift + Enter | Compile & Run     |
| Ctrl + S           | Save file           |
| Ctrl + O           | Open file           |
| Ctrl + F           | Find                |
| Enter (in Find)    | Find next           |
| Shift + Enter (in Find) | Find previous  |
| Escape             | Close Find bar      |
| Tab (in editor)    | Insert 4 spaces     |

---

## Examples

### Hello World
```c
int main() {
    printStr("Hello, TinyC!\n");
    return 0;
}
```

### LED Blink
```c
#define LED 2
#define OUTPUT 1

int main() {
    gpioInit(LED, OUTPUT);
    while (true) {
        digitalWrite(LED, 1);
        delay(500);
        digitalWrite(LED, 0);
        delay(500);
    }
    return 0;
}
```

### Fibonacci
```c
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    for (int i = 0; i < 10; i++) {
        print(fib(i));
    }
    return 0;
}
```

### String Operations
```c
int main() {
    char greeting[32] = "Hello";
    char name[16] = "World";
    char buf[64];

    // Classic function style
    strcpy(buf, greeting);
    strcat(buf, ", ");
    strcat(buf, name);
    strcat(buf, "!\n");
    printString(buf);       // Hello, World!

    // Same thing with + operator
    buf = greeting;
    buf += ", ";
    buf += name;
    buf = buf + "!\n";
    printString(buf);       // Hello, World!

    // Formatted strings
    char line[64];
    sprintfInt(line, "count = %d", 42);
    printString(line);      // count = 42

    // Multi-value with sprintfAppend
    char report[128];
    sprintfInt(report, "Sensor %d", 1);
    sprintfAppendStr(report, " name=%s", name);
    sprintfAppendFloat(report, " temp=%.1f", 23.5);
    printString(report);    // Sensor 1 name=World temp=23.5

    return 0;
}
```

### Bubble Sort
```c
void bubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

int main() {
    int data[8] = {64, 34, 25, 12, 22, 11, 90, 1};
    bubbleSort(data, 8);
    for (int i = 0; i < 8; i++) {
        print(data[i]);
    }
    return 0;
}
```

---

## Differences from Standard C

| Feature                  | Standard C     | TinyC                        |
|--------------------------|----------------|------------------------------|
| Pointers                 | Full support   | **Not supported**            |
| Structs / Unions         | Full support   | **Not supported**            |
| Enums                    | Full support   | **Not supported**            |
| Dynamic memory           | malloc/free    | Auto heap for arrays >255 (no explicit malloc) |
| Multi-dimensional arrays | `int a[3][4]`  | **Not supported**            |
| String type              | `char*`        | `char arr[N]` only           |
| Preprocessor             | Full CPP       | `#define`, `#ifdef`, `#if`, `#else`, `#endif` (no `#include`, no macros) |
| Header files             | `#include`     | **Not supported**            |
| Typedef                  | Full support   | **Not supported**            |
| sizeof                   | Full support   | **Not supported**            |
| Ternary operator         | `a ? b : c`   | **Not supported**            |
| do-while                 | `do {} while`  | **Not supported**            |
| goto                     | Full support   | **Not supported**            |
| Function pointers        | Full support   | **Not supported**            |
| Variadic functions       | `printf(...)`  | **Not supported**            |
| Standard library         | stdio, stdlib  | Built-in functions only      |

---

*Generated from TinyC source — lexer.js, parser.js, codegen.js, opcodes.js, vm.js*
