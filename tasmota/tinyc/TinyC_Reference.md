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
14. [enum](#enum)
15. [Structs](#structs)
16. [typedef](#typedef)
17. [const Keyword](#const-keyword)
18. [static Local Variables](#static-local-variables)
19. [do-while Loop](#do-while-loop)
20. [Ternary Operator](#ternary-operator)
21. [Built-in Functions](#built-in-functions)
22. [Multi-VM Slots (ESP32)](#multi-vm-slots-esp32)
23. [VM Limits](#vm-limits)
24. [Device File Management (IDE)](#device-file-management-ide)
25. [Keyboard Shortcuts (IDE)](#keyboard-shortcuts-ide)
26. [Examples](#examples)

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

### Persistent Variables
Global variables declared with the `persist` keyword are automatically saved to flash and restored on program restart. This is equivalent to `p:` variables in Tasmota Scripter.

```c
persist float totalEnergy = 0.0;   // saved/restored across reboots
persist int bootCount;              // scalar — 4 bytes in file
persist char deviceName[32];        // array — 32 slots in file
```

- Only global variables can be `persist` (not local variables or function parameters)
- Persist variables are **automatically loaded** from a `.pvs` file (derived from the `.tcb` filename, e.g. `/weather.pvs` for `/weather.tcb`) when the program starts
- Persist variables are **automatically saved** when the program is stopped (`TinyCStop`)
- Call `saveVars()` to manually save at any time (e.g., after midnight counter updates)
- Maximum 32 persist entries per program
- Binary format — compact and fast (raw int32 values, floats stored as bit-cast int32)
- File stored on user filesystem (same as `.tcb` files)

```c
persist float dval = 0.0;
persist float mval = 0.0;

void EverySecond() {
    if (tasm_hour == 0 && last_hr != 0) {
        dval = smlGet(2);  // update daily counter
        saveVars();         // save immediately
    }
}
```

### Watch Variables (Change Detection)
Global variables declared with the `watch` keyword automatically track changes. Every write saves the old value as a shadow, enabling change detection — essential for IOT monitoring scenarios.

```c
watch float power;
watch int relay;
```

- Only scalar globals can be `watch` (int, float — not arrays or locals)
- Every write automatically saves the previous value and sets a written flag
- Uses 2 extra global slots per watch variable (shadow + written flag)

**Intrinsic functions:**

| Function | Returns | Description |
|----------|---------|-------------|
| `changed(var)` | `int` | 1 if current value differs from shadow |
| `delta(var)` | `int/float` | current - shadow (signed difference) |
| `written(var)` | `int` | 1 if variable was assigned since last `snapshot()` |
| `snapshot(var)` | `void` | set shadow = current, clear written flag |

```c
watch float power;

void EverySecond() {
    power = sensorGet("ENERGY#Power");
    if (changed(power)) {
        float diff = delta(power);
        // react to power change
        snapshot(power);  // acknowledge change
    }
}
```

**Out-of-band writes are tracked too.** When a watched global is written from outside the script — `webButton` / `webSlider` (`?sv=N_V` URL), the future MQTT setvar bridge, or a UDP-global update — the firmware mirrors what `STORE_WATCH` would have done: bumps the shadow and sets the written-flag. So `written(var)` fires correctly inside `EverySecond` and `changed(var)` reports the right delta. This makes the natural pattern below work end-to-end:

```c
watch int target;                       // bound to a webSlider

void EverySecond() {
    if (written(target)) {
        addLog("user dragged slider — sending setpoint");
        // ... act on new value ...
        snapshot(target);
    }
}
void WebCall() { webSlider(target, 20, 28, "Target"); }
```

(Mechanism: at load time, the VM scans bytecode for `STORE_WATCH` and records each watched var-slot. The URL handler checks that index before raw-writing. `TC_MAX_WATCH = 16` watched globals per slot.)

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
| Op    | Description                                       |
|-------|---------------------------------------------------|
| `=`   | Assign (for `char[]`: string copy)                |
| `+=`  | Add and assign (for `char[]`: string append)      |
| `-=`  | Subtract and assign                               |
| `*=`  | Multiply and assign                               |
| `/=`  | Divide and assign                                 |
| `%=`  | Modulo and assign (int only)                      |
| `&=`  | Bitwise AND and assign                            |
| `\|=` | Bitwise OR and assign                             |
| `^=`  | Bitwise XOR and assign                            |
| `<<=` | Left shift and assign                             |
| `>>=` | Right shift and assign                            |

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
13. Assignment: `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`
14. Ternary: `? :`

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

### do-while Loop
The body executes **at least once** before the condition is checked:
```c
int i = 0;
do {
    process(i);
    i++;
} while (i < 10);

// Body runs once even if condition is initially false:
do {
    init();
} while (0);
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
| `Every100ms()` | FUNC_EVERY_100_MSECOND | Every 100 ms (10x/sec) | Medium-rate polling, display updates, debouncing |
| `EverySecond()` | FUNC_EVERY_SECOND | Every 1 second | Periodic tasks, counters, slow polling |
| `JsonCall()` | FUNC_JSON_APPEND | Telemetry cycle (~300s) | Add JSON to MQTT telemetry |
| `WebPage()` | FUNC_WEB_ADD_MAIN_BUTTON | Page load (once) | Charts, custom HTML, scripts |
| `WebCall()` | FUNC_WEB_SENSOR | Web page refresh (~1s) | Add sensor rows to Tasmota web UI |
| `WebUI()` | AJAX /tc_ui refresh | Every 2s + on widget change | Interactive widget dashboard (buttons, sliders, etc.) |
| `UdpCall()` | UDP packet received | On each multicast variable | Process incoming UDP variables |
| `WebOn()` | Custom HTTP endpoint | On request to `webOn()` URL | REST APIs, JSON endpoints, webhooks |
| `TaskLoop()` | FreeRTOS task (ESP32) | Continuous loop in own task | Background processing, independent of main thread |
| `CleanUp()` | FUNC_SAVE_BEFORE_RESTART | Before device restart | Close files, flush data, release resources |
| `TouchButton(btn, val)` | Touch event | On GFX button/slider touch | Handle touch button presses and slider changes |
| `HomeKitWrite(dev, var, val)` | HomeKit write | When Apple Home changes a value | Control lights, switches, outlets from Apple Home |
| `Command(char cmd[])` | Custom console command | When user types registered prefix in console | Handle custom Tasmota commands (e.g., MP3Play, MP3Stop) |
| `Event(char cmd[])` | Tasmota event rule trigger | On `Event` command from rules or console | React to Tasmota rule events |
| `OnExit()` | Script stop | When VM is stopped or script replaced | Close serial ports, release resources |
| `OnMqttConnect()` | FUNC_MQTT_INIT | MQTT broker connected | Subscribe topics, publish status |
| `OnMqttDisconnect()` | mqtt_disconnected flag | MQTT broker disconnected | Set offline state, stop publishing |
| `OnMqttData(char topic[], char payload[])` | FUNC_MQTT_DATA | Message arrives on a subscribed topic | Handle remote commands, ingest sensor data |
| `OnInit()` | First FUNC_NETWORK_UP | Once after first WiFi connect | One-time init: start services, subscribe MQTT |
| `OnWifiConnect()` | FUNC_NETWORK_UP | WiFi/network connected (every time) | Reconnect handling |
| `OnWifiDisconnect()` | FUNC_NETWORK_DOWN | WiFi/network lost | Pause network-dependent tasks |
| `OnTimeSet()` | FUNC_TIME_SYNCED | NTP time synchronized | Schedule time-based actions |

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
| `webSend(buf)` | Send char array to web page (→ `WSContentSend`) | `WebPage()` / `WebCall()` / `WebOn()` |
| `webSend("literal")` | Send string literal to web page | `WebPage()` / `WebCall()` / `WebOn()` |
| `webFlush()` | Flush web content buffer to client (→ `WSContentFlush`) | `WebPage()` / `WebCall()` / `WebOn()` |
| `webSendFile("filename")` | Send file contents from filesystem to web page | `WebPage()` / `WebCall()` / `WebUI()` / `WebOn()` |
| `addCommand("prefix")` | Register custom console command prefix (e.g., `"MP3"` → MP3Play, MP3Stop) | `main()` |
| `responseCmnd(buf)` | Send char array as console command response | `Command()` |
| `responseCmnd("literal")` | Send string literal as console command response | `Command()` |

> **`responseCmnd(buf)` length cap:** the char-array form is copied through a
> stack buffer of `TC_RESPONSE_MAX` bytes (default 512 on ESP32, 256 on
> ESP8266; `#ifndef`-guarded — raise it in `user_config_override.h`). Output
> longer than the cap is truncated; since that usually cuts JSON mid-object
> Tasmota then shows an empty `{}`. As of 1.6.9 a truncation logs
> `TCC: responseCmnd output truncated at N chars …` (no longer silent). For
> very large responses, split into multiple commands. The string-literal form
> has no such cap (bounded only by Tasmota's ~700 B `RESPONSE_MAX_SIZE`).

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
    sprintf(buf, ",\"TinyC\":{\"Count\":%d}", counter);
    responseAppend(buf);
}

void WebCall() {
    // Adds a row to the Tasmota web page
    char buf[64];
    sprintf(buf, "{s}TinyC Counter{m}%d{e}", counter);
    webSend(buf);
}

int main() {
    counter = 0;
    return 0;
}
```

**Result:** After uploading and running, the Tasmota web page shows a "TinyC Counter" row that increments every second, and MQTT telemetry includes `,"TinyC":{"Count":N}`.

### Custom Console Commands

Scripts can register custom Tasmota console commands using `addCommand("prefix")`. When a user types e.g. `MP3Play Sound.mp3` in the console, Tasmota matches the prefix `"MP3"`, extracts the subcommand `"PLAY SOUND.MP3"`, and calls `Command("PLAY SOUND.MP3")` on the script.

**Note:** Tasmota uppercases the command topic, so subcommands arrive as `"PLAY"`, `"STOP"`, etc. Data after a space (filenames, numbers) keeps its original case.

```c
int volume = 15;

void Command(char cmd[]) {
    char buf[64];
    if (strFind(cmd, "PLAY") == 0) {
        // handle play
        responseCmnd("Playing");
    } else if (strFind(cmd, "STOP") == 0) {
        responseCmnd("Stopped");
    } else if (strFind(cmd, "VOL") == 0) {
        char arg[16];
        strSub(arg, cmd, 4, 0);  // extract everything after "VOL "
        volume = atoi(arg);
        sprintf(buf, "Volume: %d", volume);
        responseCmnd(buf);
    } else {
        responseCmnd("Unknown: Play|Stop|Vol");
    }
}

int main() {
    addCommand("MP3");   // register "MP3" prefix
    return 0;
}
```

**Result:** Typing `MP3Play` in the Tasmota console calls `Command("PLAY")`, typing `MP3Vol 20` calls `Command("VOL 20")`.

### TaskLoop Example (ESP32)

```c
int counter = 0;

void TaskLoop() {
    counter++;
    char buf[64];
    sprintf(buf, "TaskLoop count=%d", counter);
    addLog(buf);       // appears in Tasmota console log
    delay(1000);       // real 1-second delay, doesn't block Tasmota
}

void JsonCall() {
    char buf[64];
    sprintf(buf, ",\"TinyC\":{\"Count\":%d}", counter);
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
- `Every100ms()` suits display refreshes, button debouncing, and medium-rate sensor reads
- Use `WebPage()` for one-time page content (charts, scripts) — called once when page loads
- Use `WebCall()` for sensor-style rows that refresh periodically
- Use `UdpCall()` to process incoming UDP multicast variables
- `TaskLoop()` runs in a dedicated FreeRTOS task (ESP32 only) — can use `delay()` freely, VM access is mutex-serialized with main-thread callbacks

### Dynamic Task Spawn (ESP32)

Beyond the fixed `TaskLoop()`, you can launch **up to 4 additional background tasks** on demand by name. Each spawned task shares the calling VM's state — globals, heap, constants — and runs concurrently with `main()`, callbacks, and `TaskLoop()`. Ideal replacements for one-shot timers, delayed jobs, or long background workers.

| Function | Description |
|----------|-------------|
| `spawnTask(char name[])` | Start a FreeRTOS task that calls `name()`. Returns pool slot 0..3, or -1 on error. Default stack **5 KB** |
| `spawnTask(char name[], int stack_kb)` | Same, but with custom stack size (clamped 3..16 KB) |
| `killTask(char name[])` | Cooperative stop: sets a flag the task observes at next instruction or `delay()` boundary. Returns 0 if signaled, -1 if not running |
| `taskRunning(char name[])` | Returns 1 if a task with that name is active on this slot, 0 otherwise |

**Name must be a string literal** — the compiler enforces this and registers the target in the bytecode function table at compile time. Dynamic names (variables/expressions) are not supported. A literal that doesn't resolve to a user-defined function is a compile-time error.

**Stack sizing guide** (default 5 KB works for most workers):
- **3 KB** — absolute minimum, only safe if the worker uses no `addLog` / `sprintf*` / VM syscalls beyond basic arithmetic and `delay()`
- **5 KB** (default) — trivial workers with `addLog` + `delay` loops
- **6–8 KB** — `httpGet` over HTTP (plain), small JSON parsing
- **10–16 KB** — `httpGet` over HTTPS/TLS, large JSON parsing, complex worker pipelines

**Semantics:**

- **Shared VM**: spawned tasks see and mutate the same globals/heap as `main()`. Use this for worker jobs that update global state.
- **One name per slot**: a second `spawnTask("foo")` while `foo` is still running returns -1. Use `killTask("foo")` + `taskRunning("foo")` poll first.
- **Cooperative kill**: `killTask` is non-blocking. The task will self-terminate at its next instruction boundary or after the current `delay()` wakes up. Use `while (taskRunning("foo")) delay(10);` to wait.
- **Mutex discipline**: spawnTasks honor the same mutex as `TaskLoop()`. `delay()` inside a spawn task releases the mutex so other tasks and callbacks can run.
- **Auto-cleanup**: when the script stops (TinyCStop) all spawned tasks are signaled and given 2 s to exit.
- **No arguments**: the spawned function takes no parameters and its return value is ignored.

**Example — one-shot delayed job:**

```c
void Blinker() {
    for (int i = 0; i < 5; i++) {
        gpioWrite(2, 1); delay(200);
        gpioWrite(2, 0); delay(200);
    }
}

void Command(char s[]) {
    if (strcmp(s, "BLINK") == 0) {
        if (taskRunning("Blinker")) {
            addLog("Blinker already active");
        } else {
            spawnTask("Blinker");
        }
    }
}

int main() { return 0; }
```

Typing `TinyCCmd BLINK` in the console spawns the blinker without blocking the console. A second `TinyCCmd BLINK` while blinking is refused.

**Example — parallel background downloader:**

```c
char url[] = "http://example.com/data.json";
int download_done = 0;
char body[2048];

void Downloader() {
    int rc = httpGet(url, body, sizeof(body));
    download_done = (rc > 0) ? 1 : -1;
}

void EverySecond() {
    if (download_done == 1) {
        addLog("download ok");
        download_done = 0;
    } else if (download_done == -1) {
        addLog("download failed");
        download_done = 0;
    }
}

int main() {
    spawnTask("Downloader", 6);  // 6 KB stack for HTTPS
    return 0;
}
```

**Example — killable worker:**

```c
int worker_ticks = 0;

void Worker() {
    while (1) {
        worker_ticks++;
        delay(500);
    }
}

void Command(char s[]) {
    if (strcmp(s, "START") == 0 && !taskRunning("Worker")) spawnTask("Worker");
    if (strcmp(s, "STOP")  == 0) killTask("Worker");
}

int main() { return 0; }
```

**Limits:**

- Max 4 concurrent spawned tasks **per device** (shared pool across all VM slots)
- Function name max 23 chars
- Stack 2..12 KB, default 3 KB — bump to 6+ for HTTPS / JSON / large buffers
- ESP8266: all four calls return -1 (not supported)

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
| `tasm_sunrise` | int | read | Sunrise, minutes since midnight (requires USE_SUNRISE) |
| `tasm_sunset` | int | read | Sunset, minutes since midnight (requires USE_SUNRISE) |
| `tasm_time` | int | read | Current time, minutes since midnight |
| `tasm_pheap` | int | read | Free PSRAM in bytes (ESP32 only, 0 on ESP8266) |
| `tasm_smlj` | int | read/write | SML JSON output enable/disable (requires USE_SML_M) |
| `tasm_npwr` | int | read | Number of power (relay) devices |
| `tasm_rule` | int | read/write | Rule1 enabled (bit 0 of `Settings->rule_enabled`). Read returns 0 or 1. Write any non-zero to enable, 0 to disable. Equivalent to the console `Rule1 1` / `Rule1 0` commands. Note: some Tasmota subsystems (SML descriptors) check this flag at init and silently skip when Rule1 is disabled — flip with `tasm_rule = 1` before starting them. |
| `tasm_lat` | float | read/write | Device latitude in decimal degrees (e.g. 48.137). Backed by `Settings->latitude` (stored ×1 000 000 as int). Used by `tasm_sunrise` / `tasm_sunset` calculations. |
| `tasm_lon` | float | read/write | Device longitude in decimal degrees (e.g. 11.575). Backed by `Settings->longitude`. |
| `tasm_maxblock` | int | read | Largest contiguous free heap block in bytes (ESP32 only) — diagnoses heap fragmentation: free heap can be high while `maxblock` is low |
| `tasm_frag` | int | read | Heap fragmentation 0..100 % (ESP32 only) — derived from `1 - maxblock/free_heap` |

### Indexed Tasmota State Functions

| Function | Description |
|----------|-------------|
| `int tasmPower(int index)` | Power state of relay `index` (0-based). Returns 0 or 1 |
| `int tasmSwitch(int index)` | Switch state (0-based, Switch1 = index 0). Returns -1 if invalid |
| `int tasmCounter(int index)` | Pulse counter value (0-based, Counter1 = index 0). Requires USE_COUNTER |

### Tasmota String Info

`int tasmInfo(int sel, char buf[])` — fills `buf` with a Tasmota info string. Returns string length.

| sel | Content |
|-----|---------|
| 0 | MQTT topic |
| 1 | MAC address |
| 2 | Local IP address |
| 3 | Friendly name |
| 4 | Device name |
| 5 | MQTT group topic |
| 6 | Reset reason (string) |

**Example:**
```c
char topic[64];
tasmInfo(0, topic);    // get MQTT topic
char ip[20];
tasmInfo(2, ip);       // get local IP
```

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

// Sunrise/sunset automation
int now = tasm_time;     // minutes since midnight
if (now > tasm_sunset || now < tasm_sunrise) {
    tasm_power = 1;      // night — turn on light
}

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
    sprintf(buf, "{s}Temp{m}%.1f C{e}", tasm_temp);
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
char greeting[] = "Hello World";    // size inferred from string (12)
int flags[] = {1, 0, 1, 1};        // size inferred from initializer (4)
```

When the size is omitted (`[]`), the compiler infers it automatically:
- **String initializer:** size = string length + 1 (for null terminator)
- **Array initializer:** size = number of elements

### Access
```c
int x = data[0];       // read
data[3] = 42;          // write
data[i + 1] = data[i]; // computed index
```

### Scope
- **Small arrays (≤16 elements)** — stored inline in global data or local frame (fast direct access)
- **Large arrays (>16 elements)** — automatically allocated on the VM heap

### Array Memory

Arrays with up to 16 elements are stored inline in the global or local frame for fast direct access. Arrays with more than 16 elements are automatically routed to the VM heap by the compiler — no special syntax needed:

```c
int rgb[3];            // inline (3 ≤ 16) — fast direct access
char buf[128];         // heap (128 > 16) — automatic allocation
float data[2000];      // heap (2000 > 16)

int main() {
    rgb[0] = 255;       // direct frame access
    buf[0] = 'H';       // heap access — same syntax
    data[1999] = 3.14;  // heap access
    return 0;
}
```

Both inline and heap arrays support all the same operations: element access, string operations on `char[]`, passing to functions, etc.

**Heap limits:**

**Heap limits:**

| Platform | Max Heap Slots | Max Handles |
|----------|---------------|-------------|
| ESP8266  | 2,048 (8 KB)  | 8           |
| ESP32    | 8,192 (32 KB) | 16          |
| Browser  | 16,384 (64 KB)| 32          |

### 2D Arrays *(since 1.3.38)*

Two-dimensional arrays for `char`, `int`, and `float` work like in
standard C, with row-major flat storage in the heap:

```c
char names[7][16];           //   7 rows × 16 cols   (char  → heap)
int  ltab[5][4];             //   5 rows ×  4 cols   (int   → heap)
float coef[3][2];            //   3 rows ×  2 cols   (float → heap)

int main() {
    // Element access
    ltab[2][3] = 42;
    int v = ltab[r][c];
    float k = coef[i][j];

    // String ops on char rows
    strcpy(names[0], "Sonntag");
    strcat(names[1], " ergänzt");
    int eq = strcmp(names[0], names[1]);

    // Pass a row to a function expecting a 1D array of the same type
    show_row_int(ltab[3], 4);
    show_row_str(names[i]);

    // sprintf %s with a 2D char row works with constant or variable index
    char buf[64];
    sprintf(buf, "name=%s len=%d", names[i], strlen(names[i]));
    return 0;
}

void show_row_int(int row[], int n) { /* row is the i-th row of the caller */ }
void show_row_str(char s[])         { addLog(s); }
```

**Memory & limits:**

- Total flat size = `rows × cols`. Subject to the same heap caps as 1D
  arrays in the table above. `char buf[8][32]` = 256 elements (heap).
- **Row references require heap storage.** Auto-promotion happens at
  >16 total elements (the regular 1D threshold), so any practical 2D
  size qualifies. If you write a tiny 2D like `char buf[2][3]` (= 6
  elements, stays inline) and try `func(buf[0])`, the compiler emits
  a clear error — make the array bigger or assemble the row manually.
- **`buf` (no index)** passed to a function expecting that type's array
  is treated as the entire flat data (length `rows × cols`).
- **`buf[i]` (one index)** passed to a function expecting a 1D array
  is the i-th row (length `cols`).
- **`buf[i][j]`** is one element.

**Limitations:**

- 3D and higher dimensions are not supported. Use a 2D array with
  manual stride math for the few cases that need it.
- Mixed-type promotion in 2D element expressions follows the same
  rules as 1D — int↔float coercion happens automatically.
- Initialiser literals for 2D are not yet accepted (`int m[2][3] =
  {{1,2,3},{4,5,6}};` currently errors). Initialise in `main()` with
  a loop or per-element assignments.

**Internals:** the runtime is unchanged — the compiler flattens
`buf[i][j]` to `buf[i*cols + j]` at the existing 1D heap-array opcodes,
and emits an offset-bearing reference (`ADDR_HEAP_OFF`) for `buf[i]`
in row-passing contexts. So 2D is purely an ergonomic layer on top of
the 1D heap; no new VM features.

---

## Structs *(since 1.4.0)*

Structs are records — composite values that group named fields. TinyC structs
follow C-style by-value semantics: copying a struct copies all fields,
passing one to a function gives the callee its own copy.

### Definition

```c
struct Point {
    int x;
    int y;
}

struct WriteLog {
    int  addr;
    int  val;
    int  ms;
    char src;
}

struct Sample {
    int   duration_ms;
    float ratio;
    char  label[16];          // char-array field
}

struct Rect {
    Point tl;                 // nested struct field
    Point br;
}
```

- `struct` keyword introduces a type definition.
- Field types: `int`, `float`, `char`, fixed-size 1D arrays of those, or
  nested struct types declared earlier in the file.
- Trailing semicolon after `}` is optional.

### Variables and access

```c
Point p;                       // local, zero-initialized
Point q = {3, 4};              // positional initializer
Point arr[5];                  // array of struct
WriteLog g_w;                  // global

p.x = 10;
int v = p.y;
arr[i].x = i * 10;
g_w.src = 'O';
strcpy(g_w_or_other.label, "boost");

// Nested
Rect r;
r.tl.x = 0;
r.br.y = 200;
```

The `struct` keyword can be omitted when declaring a variable of an
already-defined type (`Point p;` is equivalent to `struct Point p;`).

### Whole-struct assignment

```c
Point a; a.x = 5; a.y = 7;
Point b;
b = a;                         // field-by-field copy

WriteLog ev;
ev.addr = 0x40; /* etc */
wlog[i] = ev;                  // var → array element

WriteLog x;
x = wlog[3];                   // array element → var
```

The compiler unrolls the copy at compile time (one `LOAD`/`STORE` pair
per slot). For a 4-slot WriteLog: 8 ops + a temp for the offset/value
order. No new VM opcodes.

### Functions

Structs are passed **by value** — the callee gets its own copy.
Mutations to a struct param do NOT propagate back to the caller.

```c
void log_write(WriteLog w) {
    char m[80];
    sprintf(m, "[%d] addr=%d val=%d", w.ms, w.addr, w.val);
    addLog(m);
}

log_write(wlog[5]);            // passes a copy
```

Returning a struct also works — the callee pushes all field values,
and the caller's local-decl initializer pops them via a per-function
temp slot:

```c
WriteLog make_obs(int addr, int val) {
    WriteLog w;
    w.addr = addr;
    w.val  = val;
    w.ms   = millis();
    w.src  = 'O';
    return w;
}

WriteLog x = make_obs(0x40, 0xFF00);
```

### `sizeof(StructTag)`

Returns the slot count at compile time:

```c
int n = sizeof(Point);          // 2
int m = sizeof(Sample);         // 18  (1 + 1 + 16)
int r = sizeof(Rect);           // 4   (2× Point inner struct = 2+2)
```

`sizeof(int)` / `sizeof(float)` / `sizeof(char)` return 1 (the slot count
in TinyC's int32 model). Note: `sizeof(int)` requires the `int` keyword,
which the parser accepts only in this position.

### Memory & layout

Each field occupies a fixed number of int32 slots:

| Field type            | Slots                                 |
| --------------------- | ------------------------------------- |
| `int`, `char`, `bool` | 1                                     |
| `float`               | 1                                     |
| `int arr[N]`          | N                                     |
| `char name[N]`        | N (one byte per slot, low 8 bits)     |
| `float ys[N]`         | N                                     |
| Nested struct         | inner struct's total slot count       |

Total struct slots = sum of all field slots. Nested structs flatten — a
`Rect` containing two `Point` (2 slots each) has total slot count 4
with `br.x` at offset 2.

Heap-promotion follows existing TinyC rules: a single struct value of
≤16 slots lives in stack/globals; any struct array totaling >16 slots
auto-promotes to heap (so almost all struct arrays are on heap).

### Persist (durable globals)

`persist WriteLog wlog[16];` works — the slot count is included in the
persist hash, so adding/removing fields invalidates the `.pvs` file.

**Limitation:** the v1 hash does NOT include field-name list, so silently
**reordering** fields within a struct decl after persist data exists won't
invalidate. The reordered layout reads the old data with shifted offsets.
Workaround: add and remove a dummy field (which does change the hash), or
manually delete `<name>.pvs`. Future v2 will include field names in the hash.

### Forbidden in v1

Each produces a clear compile-time error:

- `struct Node { Node next; }` — self-referential structs need pointer support.
- `struct B { struct B child; }` — same as above (mutual recursion via no-pointer self).
- `struct S { int grid[3][3]; }` — 2D-array fields. Flatten or nest a struct.
- `if (a == b)` for two structs — equality not implemented; compare fields manually.
- `Foo a = { .x = 1, .y = 2 };` — designated initializers (use positional `{1, 2}`).

### Real-world example

The classic "ring buffer of records" pattern:

```c
struct WriteEvent {
    int  addr;
    int  val;
    int  ms;
    char src;
}

WriteEvent wlog[16];
int        wlog_pos   = 0;
int        wlog_count = 0;

void wlog_push(int addr, int val, char src) {
    WriteEvent ev;
    ev.addr = addr;
    ev.val  = val;
    ev.ms   = millis();
    ev.src  = src;
    wlog[wlog_pos] = ev;          // single struct copy
    wlog_pos = (wlog_pos + 1) % 16;
    if (wlog_count < 16) wlog_count = wlog_count + 1;
}
```

Compared to the pre-1.4 idiom (4 parallel arrays + 4 manual writes per
push site), structs eliminate a class of "the arrays got out of sync"
bugs entirely. See `examples/structs_demo.tc` for the full pattern + a
nested-struct example.

---

## Function Pointers *(since 1.4.1)*

A function pointer holds the bytecode address of a function. Stored,
passed, and called like an `int`-sized value, but invoked with the same
syntax as a regular function call.

### Declaring a fn-ptr type

Function-pointer types **must** be introduced via `typedef`. Inline
`void (*p)(int);` syntax is not supported in v1.

```c
typedef int  (*int_op)(int, int);              // signature: int(int, int)
typedef void (*greet_fn)(char who[]);          // signature: void(char[])
typedef int  (*cmp_fn)(char a[], char b[]);
```

The typedef syntax is identical to C: `typedef <retType> (*<alias>)(<params>);`.

### Variables, assignment, calls

```c
int my_add(int a, int b) { return a + b; }
int my_mul(int a, int b) { return a * b; }

int_op op;
op = my_add;            // assignment from a bare function name (no `&`)
int s = op(3, 4);       // s = 7

op = my_mul;            // reassign; same signature only
int p = op(3, 4);       // p = 12
```

Three things to know:

1. **Bare function name** is the address — no `&fn` syntax. `op = &my_add`
   would be a parse error. Just `op = my_add`.
2. **Reassignment is fine** as long as the new function's signature
   matches the typedef. The compiler doesn't currently check this, so
   wrong signatures will fail at runtime in unpredictable ways.
3. **Forward references work** — you can assign or call a function
   defined later in the source. Addresses are patched after the
   function-compile pass.

### As a function parameter

```c
int run_op(int_op f, int a, int b) {
    return f(a, b);
}

int s = run_op(my_add, 10, 20);   // 30
int p = run_op(my_mul, 10, 20);   // 200
```

Pass the bare name; the callee receives an address-valued local.

### As a global

```c
greet_fn g_handler;

void hello(char who[]) {
    char m[64];
    sprintf(m, "Hello, %s!", who);
    addLog(m);
}

int main() {
    g_handler = hello;
    g_handler("world");
    return 0;
}
```

### Dispatch tables (the killer use case)

The pattern that motivated this feature — clean command dispatch:

```c
typedef void (*cmd_handler)(char args[]);

void do_on(char args[])  { addLog("ON");  /* ... */ }
void do_off(char args[]) { addLog("OFF"); /* ... */ }
void do_set(char args[]) { addLog("SET"); /* ... */ }

struct CmdEntry {
    char         name[12];
    cmd_handler  handler;
}
CmdEntry cmds[3];

int main() {
    strcpy(cmds[0].name, "ON");   cmds[0].handler = do_on;
    strcpy(cmds[1].name, "OFF");  cmds[1].handler = do_off;
    strcpy(cmds[2].name, "SET");  cmds[2].handler = do_set;
    return 0;
}

void Command(char cmd[]) {
    for (int i = 0; i < 3; i = i + 1) {
        if (strcmp(cmd, cmds[i].name) == 0) {
            cmds[i].handler(cmd);
            responseCmnd("ok");
            return;
        }
    }
    responseCmnd("unknown");
}
```

*(Function-pointer fields inside structs work in 1.4.2+ — the call site
`cmds[i].handler(args)` routes through `OP_CALL_INDIRECT` correctly.)*

### How it works

A fn-ptr value is just the function's bytecode address (16-bit, fits
in an `int`). The compiler emits:

- **Address-of**: `op = my_add;` → `PUSH_I32 <addr>; STORE_LOCAL/GLOBAL`. The
  4-byte int32 holds the address in its low 16 bits.
- **Indirect call**: `op(args)` → push args, push the var's value
  (`LOAD_LOCAL`/`LOAD_GLOBAL`), then `OP_CALL_INDIRECT` (0x56). This new
  opcode pops the address from the stack, sets up a frame, and jumps —
  same semantics as `OP_CALL` minus the bytecode-embedded operand.

Frame setup is identical to a direct call, so existing `RET` / `RET_VAL`
handle returns transparently.

### Out of v1

| Feature                                     | Status |
|---------------------------------------------|--------|
| Function pointers as struct fields          | ✅ since 1.4.2 |
| Inline `void (*p)(int)` without typedef     | not in v1 |
| `&fn` (explicit address-of) syntax          | not in v1 (use bare `fn`) |
| Comparison `fn1 == fn2`                     | not in v1 |
| Returning a fn-ptr from a function          | not in v1 |
| Anonymous function literals (lambdas)       | never (no closure mechanism) |
| Signature checking on assignment            | not in v1 (silent at compile time) |

---

## Reference Parameters *(since 1.4.3)*

Function parameters declared with `&` after the type are **passed by
reference** — the callee's reads and writes go directly to the caller's
variable. Mutations are visible after the call returns.

### Syntax

```c
void swap(int& a, int& b) {
    int tmp = a;
    a = b;
    b = tmp;
}

int x = 5;
int y = 7;
swap(x, y);
// x is now 7, y is now 5
```

The `&` goes after the type, before the parameter name. Same syntax as C++.

### Use cases

**Multi-out parsers** — return multiple values without packaging into an array:

```c
void parse_pair(int input, int& low, int& high) {
    low  = input & 0xFF;
    high = (input >> 8) & 0xFF;
}

int lo = 0;
int hi = 0;
parse_pair(0xABCD, lo, hi);
// lo == 0xCD, hi == 0xAB
```

**In-place mutation with compound operators**:

```c
void inc_by(int& n, int amount) {
    n += amount;          // compound assignment on a ref param works
}

int counter = 10;
inc_by(counter, 5);
inc_by(counter, 5);
inc_by(counter, 5);
// counter == 25
```

**Globals as ref args** — safe pattern for accumulators:

```c
int g_count = 0;
int g_total = 0;

void accumulate(int& count, int& total, int sample) {
    count += 1;
    total += sample;
}

accumulate(g_count, g_total, 100);
accumulate(g_count, g_total, 200);
// g_count == 2, g_total == 300
```

### What can be passed as a ref arg

The argument expression must be a **plain variable name** — a local or
global. Anything else gets a clear compile error:

| Expression                              | Allowed? |
| --------------------------------------- | -------- |
| `swap(x, y)` — locals                   | ✅       |
| `swap(g_count, g_total)` — globals      | ✅       |
| `swap(g_count, x)` — mix                | ✅       |
| `swap(arr[i], y)` — array element       | ❌ (v1 not yet) |
| `swap(obj.field, y)` — struct field     | ❌ (v1 not yet) |
| `swap(some_int_array, y)` — int[] var   | ❌ (heap arrays disallowed in v1) |

For array elements and struct fields, you currently need to copy into a
temporary local, pass that, then copy back. Future v2 polish will
remove this restriction.

### Type compatibility

The ref parameter's declared type must match the argument's type. Today
the compiler doesn't enforce this strictly — silent miscompilation is
possible if you pass `float` to `int&`. v1 limitation.

### What about arrays?

Array parameters (`int arr[]`, `char buf[]`) are already pass-by-
reference in TinyC — that's how they've always worked. The new `&` is
specifically for **scalars** (int, float, char). For an array, just use
`int arr[]` like before.

### How it works (no new VM opcodes)

The implementation reuses the existing reference-encoding machinery:

- Caller emits `ADDR_LOCAL <slot>` (or `ADDR_GLOBAL <gindex>`) — these
  push an encoded reference value onto the stack.
- Callee stores the encoded reference in a 1-slot local with an
  internal `isScalarRef` flag.
- Inside the body, reading the ref param compiles to
  `PUSH_I8 0; LOAD_REF_ARR <slot>` (load index 0 of the referenced
  variable). Writing compiles to `PUSH_I8 0; <value>; STORE_REF_ARR <slot>`.

Scalar refs are conceptually "array refs always accessed at index 0",
which is why no new opcodes were needed — TinyC has had array refs
since day one.

### Out of v1

| Feature                                  | Status |
| ---------------------------------------- | ------ |
| `int&` / `float&` / `char&`              | ✅ since 1.4.3 |
| `swap(arr[i], y)` (array element as ref) | not in v1 |
| `swap(obj.field, y)` (struct field as ref) | not in v1 |
| Heap-array variable as ref arg           | not in v1 |
| Signature mismatch detection             | not in v1 |
| Reference to a struct (`Point& p`)       | not in v1 (use `Point` by value or `Point arr[]`) |

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

`sprintf` supports multiple values in a single call. The compiler auto-detects each value's type from the format specifier:

```c
char buf[128];
int id = 1;
float temp = 23.5;
char name[] = "sensor";

// Multiple values in one call:
sprintf(buf, "id=%d temp=%.1f name=%s", id, temp, name);
// buf = "id=1 temp=23.5 name=sensor"

// Single value also works as before:
sprintf(buf, "x = %d", 42);              // "x = 42"
sprintf(buf, "pi = %.2f", 3.14);         // "pi = 3.14"
```

### Building Multi-Value Strings (sprintfAppend)

Use `sprintfAppend` to append multiple values to an existing string:

```c
char report[128];
sprintf(report, "Sensor %d", 1);               // "Sensor 1"
sprintfAppend(report, " val=%.1f", 3.14);      // "Sensor 1 val=3.1"
printString(report);
```

| Function | Description |
|----------|-------------|
| `sprintf(char dst[], "fmt", val, ...)` | Format one or more values into dst (overwrites). Type auto-detected. |
| `sprintfAppend(char dst[], "fmt", val, ...)` | Format one or more values and append to dst. Type auto-detected. |

### Variadic addLog

`addLog` accepts the same `printf`-style format + args as `sprintf` directly —
no scratch buffer needed for one-shot log lines:

```c
addLog("boot ok");                                  // string literal (cheapest)
addLog("counter=%d", counter);                      // single int
addLog("id=%d temp=%.1f name=%s", id, temp, name);  // multi-value
```

Internally the compiler routes the variadic form through the same `sprintf`
machinery, formats into a stack buffer, then emits the `AddLog` syscall at
`LOG_LEVEL_INFO`. Prefer this over the `sprintf(buf, ...); addLog(buf);` pair
whenever you don't need the formatted string for anything else — it's shorter
and skips the explicit buffer declaration.

`addLogLevel(level, "fmt", val, ...)` is the level-selectable variant
(`1=ERROR / 2=INFO / 3=DEBUG / 4=DEBUG_MORE`).

> **Legacy aliases:** The explicit-type variants `sprintfInt`, `sprintfFloat`, `sprintfStr`, `sprintfAppendInt`, `sprintfAppendFloat`, `sprintfAppendStr` are still supported for backward compatibility.

**Format specifiers:** `%d` (int), `%f` `%.2f` `%e` `%g` (float), `%s` (string).

### String Manipulation

```c
char src[64] = "hello,world,test";
char dst[32];

// Extract nth token (1-based) by delimiter
int len = strToken(dst, src, ',', 2);  // dst = "world", len = 5

// Substring (0-based position, length)
strSub(dst, src, 6, 5);               // dst = "world"
strSub(dst, src, -4, 4);              // dst = "test" (negative = from end)

// Find substring position (-1 if not found)
int pos = strFind(src, "world");       // pos = 6
int no = strFind(src, "xyz");          // no = -1
```

| Function | Description |
|----------|-------------|
| `strToken(char dst[], char src[], int delim, int n)` | Copy nth token (1-based) delimited by char `delim` into dst. Returns token length. |
| `strSub(char dst[], char src[], int pos, int len)` | Copy `len` chars starting at `pos` (0-based, negative=from end) into dst. `len=0` copies to end of string. Returns actual length. |
| `strFind(char haystack[], char needle[])` | Find first occurrence of needle in haystack. Returns position (0-based) or -1 if not found. |
| `int strToInt(char str[])` | Convert string to integer (like `atoi`). Returns 0 if not a valid number. |
| `float strToFloat(char str[])` | Convert string to float (like `atof`). Returns 0.0 if not a valid number. |

### Array Sort

Sort an array in-place:

| Function | Description |
|----------|-------------|
| `sortArray(int arr[], int count, int flags)` | Sort array in-place. `flags`: 0=int ascending, 1=float ascending, 2=int descending, 3=float descending |
| `arrayFill(int arr[], int value, int count)` | Fill first `count` elements with `value` |
| `arrayCopy(int dst[], int src[], int count)` | Copy `count` elements from `src` to `dst` |
| `int smlCopy(int arr[], int count)` | Copy SML decoder values into float array. Returns number copied (requires USE_SML_M) |

```c
int data[5] = {42, 7, 99, 3, 55};
sortArray(data, 5, 0);    // ascending: {3, 7, 42, 55, 99}
sortArray(data, 5, 2);    // descending: {99, 55, 42, 7, 3}
arrayFill(data, 0, 5);    // zero all elements
```

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
| `\xNN` | Hex character code (e.g. `\x41` = 'A') |

### Higher-level string ops *(since 1.5.0)*

Beyond the C-style `strcpy` / `strcat` / `strcmp` / `strlen` /
`strFind` / `strSub` / `strToken` primitives, TinyC 1.5.0 adds 7
built-ins for the most common text-handling patterns. All operate
**in-place** on a `char[]` buffer; the second argument is always a
**string literal** (compiled to a const-pool index — runtime-needle
variants intentionally not exposed in v1).

```c
// Find/replace all occurrences of a literal old → literal new.
// Handles both grow ("a"→"AA") and shrink ("hello"→"hi") with full
// buffer-overflow guard. Returns the number of replacements made.
int n = strReplace(buf, "old", "new");

// Prefix / suffix / substring tests. Return 1 if true, 0 if false.
// (Empty needle: startsWith returns 1, endsWith returns 1, contains returns 0.)
if (strStartsWith(cmd, "MBUS"))    { /* dispatch MBUS… */ }
if (strEndsWith(name, ".tcb"))     { /* it's a TinyC binary */ }
if (strContains(html, "<error>"))  { /* something failed */ }

// In-place ASCII case conversion. UTF-8 multi-byte chars (>=0x80) pass through.
strToUpper(buf);
strToLower(buf);

// In-place whitespace trim (leading + trailing ' \t\n\r').
// Shifts the buffer down so buf[0] is the first non-whitespace byte.
// Returns the new length.
int newlen = strTrim(buf);
```

**Real-world use case — config-line parsing:**

```c
char line[80] = "  TARGET_TEMP =   23.4   ";
strTrim(line);                                   // "TARGET_TEMP =   23.4"
if (strStartsWith(line, "TARGET_TEMP")) {
    strReplace(line, " = ", "=");                // normalize space-around-equals
    strReplace(line, "= ", "=");
    strReplace(line, " =", "=");
    // line is now "TARGET_TEMP=23.4"
    int eq = strFind(line, "=");
    char value[32];
    strSub(value, line, eq + 1, 99);
    float v = atof(value);
}
```

**What's NOT included in v1** (deferred — sprintf + strToken cover
their niches today):

| Feature                           | Use what instead                       |
| --------------------------------- | -------------------------------------- |
| Runtime-needle variants           | Use literal needles or write a loop    |
| `split` returning a list          | `strToken(dst, src, delim, n)`         |
| `join` of an array                | `strcat` in a loop                     |
| `padLeft` / `padRight`            | sprintf with `%-Ns` / `%Ns` formatters |
| `repeat` (n × char)               | `strcat` in a loop, or pre-build       |
| Regex                             | not planned                            |

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

## sizeof Operator

`sizeof` is a **compile-time** operator that resolves to an integer literal. There is no runtime cost — the compiler folds the value directly into the bytecode.

### Forms

```c
sizeof(type)      // int, float, char, bool, struct Tag, or a typedef name
sizeof(name)      // a declared variable, array, or struct
sizeof name       // same as above, without parentheses
```

### Sizes (bytes, following C conventions)

| Thing              | sizeof |
|--------------------|--------|
| `int`, `float`     | 4      |
| `char`, `bool`     | 1      |
| `char buf[40]`     | 40     |
| `int arr[10]`      | 40     |
| `float ff[5]`      | 20     |
| `struct Foo`       | sum of member byte sizes |
| `struct Foo v[N]`  | N × sizeof(struct Foo) |

**Note:** in TinyC's VM every scalar occupies a 32-bit slot internally, but `sizeof` always reports bytes as a C programmer would expect. `sizeof(char)` is **1**, not 4.

### Examples

```c
char buf[80];
int  arr[10];

int a = sizeof(int);              // 4
int b = sizeof(buf);              // 80
int n = sizeof(arr) / sizeof(int); // 10 (element count idiom)

struct Frame { int id; char name[8]; float v; };
int s = sizeof(struct Frame);     // 16 (4 + 8 + 4)
```

### Use in constant expressions

`sizeof` can appear in array-size expressions since it folds to a constant:

```c
char header[8];
char packet[sizeof(header) + 32];   // packet[40]
```

### Not supported

```c
sizeof(arr[0])     // ERROR — arbitrary expressions not allowed
sizeof(x + y)      // ERROR
```

Workaround: for the element-size of an array use `sizeof(type)` directly, e.g. `sizeof(arr) / sizeof(int)`.

---

## Ternary Operator

The conditional expression `condition ? value_if_true : value_if_false`:

```c
int abs_val = (x >= 0) ? x : -x;
float clamped = (t > 100.0) ? 100.0 : t;

// Nested ternary
int grade = (score >= 90) ? 3 : (score >= 70) ? 2 : 1;
```

Result type follows normal int/float promotion rules.

---

## enum

Named integer constants expanded at compile time:

```c
// Global enum
enum Color { RED = 0, GREEN = 1, BLUE = 2 };

// Negative values supported
enum Status { OK = 0, WARN = 1, ERR = -1 };

// Auto-increment (starts at 0, or after last explicit value)
enum Day { MON, TUE, WED, THU, FRI, SAT, SUN };
// MON=0, TUE=1, WED=2 ...

// Inline enum inside a function
void process() {
    enum Mode { IDLE = 0, RUN = 1, STOP = 2 };
    int mode = RUN;
}
```

- Enum values are treated as `int` constants — identical to `#define`
- The enum tag name is optional: `enum { A, B, C }` is valid
- No enum type checking — values are plain integers

---

## const Keyword

The `const` qualifier is accepted on variable declarations:

```c
const int MAX_RETRIES = 5;
const float PI = 3.14159;
const int TABLE_SIZE = 64;
```

- `const` has no runtime effect in TinyC — it is a documentation hint only
- The variable can technically be written to (no enforcement)
- Accepted on both global and local variables
- Accepted in combination with `static`: `static const int N = 10;`

---

## static Local Variables

A local variable declared `static` is stored in the global data segment but is only accessible by name within its declaring function. Its value **persists across function calls** — it is initialised to zero when the program starts and retains its value between calls.

```c
// Call counter — value survives across calls
int nextId() {
    static int id = 0;
    id++;
    return id;
}

void main() {
    int a = nextId();  // a = 1
    int b = nextId();  // b = 2
    int c = nextId();  // c = 3
}
```

- Initialiser value (e.g. `static int n = 5`) is **not emitted** — the variable is always zero-initialised at program start. Set a non-zero initial value explicitly on first call if needed.
- `static` global variables behave the same as regular globals (no difference in TinyC)

---

## do-while Loop

See [Control Flow → do-while Loop](#do-while-loop) above.

---

## Structs

A `struct` groups multiple fields into a single named variable. Each field is a separate VM slot — no padding, no alignment requirements.

### Declaration

```c
struct Point {
    float x;
    float y;
};

struct Sensor {
    float temperature;
    float humidity;
    int   status;
};
```

### Variable declaration and member access

```c
struct Point p;          // local struct variable
p.x = 3.14;
p.y = 2.71;
float dist = p.x + p.y;

// Positional initializer list
struct Point origin = {0.0, 0.0};
struct Point corner = {100.0, 200.0};
```

### Global structs

```c
struct Sensor g_sensor;     // global — persists between callbacks

void EverySecond() {
    g_sensor.temperature = sensorGet("DS18B20#Temperature");
    g_sensor.status = (g_sensor.temperature > 30.0) ? 1 : 0;
}
```

### Compound member assignment

All compound operators work on struct fields:

```c
struct Counter c;
c.val = 10;
c.val += 5;    // c.val = 15
c.val *= 2;    // c.val = 30
c.flags |= 0x01;
```

### Array fields in structs

A struct field can itself be an array — specify the element count in brackets:

```c
struct Msg {
    int  id;
    char text[32];   // 32-element char array field
};

struct Stats {
    int  count;
    int  vals[8];    // int array field
    float avg;
};
```

**Element access** uses the same `obj.field[index]` syntax:

```c
struct Msg m;
m.id = 1;
m.text[0] = 'H';
m.text[1] = 'i';
m.text[2] = 0;

// Index from a variable works too
int i;
for (i = 0; i < 8; i++) {
    m.text[i] = 65 + i;  // 'A'…'H'
}
```

**Compound assignments** work on array field elements:

```c
struct Stats s;
s.vals[0] = 10;
s.vals[0] += 5;   // 15
s.vals[0] *= 2;   // 30
```

**Passing a char array field to string functions** — use `obj.field` (without subscript) as the array reference:

```c
struct Frame {
    int  seq;
    char payload[64];
};

struct Frame f;
strcpy(f.payload, "hello");
sprintf(f.payload, "seq=%d", f.seq);
addLog(f.payload);
```

**Layout**: array fields occupy consecutive VM slots immediately after any preceding scalar fields. The total slot count for a struct is the sum of all field sizes (scalar fields = 1 slot each, array fields = arraySize slots).

### Struct inside function parameters

Structs cannot be passed by value to functions. Pass a scalar field, or use a global.

### Notes

- Field access `p.x` compiles to an array slot offset — no new VM opcodes
- Scalar fields can be `int`, `float`, `char`, `bool`
- Array fields declared as `type name[N]` within the struct body
- Nested structs **are supported** as field type — `struct Outer { Point tl; }`,
  access via `o.tl.x`. Self-referential structs (`struct Node { Node next; }`)
  are not, because they require pointers.
- Function-pointer fields supported since 1.4.2 (typedef'd fn-ptr types) —
  `cmds[i].handler(args)` routes through `OP_CALL_INDIRECT`.
- No 2D-array fields. No unions. No bit-fields. No pointer types.

---

## typedef

`typedef` creates a type alias. Used with both primitive types and structs.

### Primitive alias

```c
typedef int   pin_t;
typedef float celsius_t;
typedef int   millisec_t;

pin_t led = 5;
celsius_t temp = 23.5;
millisec_t timeout = 1000;
```

### Named struct alias

Allows using the type name without the `struct` keyword:

```c
struct Vec2 { float x; float y; };
typedef struct Vec2 Vec2;

Vec2 v;          // no 'struct' prefix needed
v.x = 1.0;
```

### Anonymous struct typedef

Define and name a struct in one declaration:

```c
typedef struct {
    int r;
    int g;
    int b;
} Color;

Color red = {255, 0, 0};
Color sky;
sky.b = 235;
```

### Chained aliases

```c
typedef int myint;
typedef myint counter_t;   // alias of an alias
counter_t n = 0;
```

### typedef inside functions

`typedef` may appear inside a function body. The alias is visible for the rest of the function.

```c
void process() {
    typedef float weight_t;
    weight_t kg = 72.5;
}
```

---

## Built-in Functions

### Output

| Function                | Description                      |
|-------------------------|----------------------------------|
| `print(int value)`      | Print integer + newline          |
| `print("literal")`     | Print string literal (auto-detected) |
| `print(char buf[])`    | Print char array as string (auto-detected) |
| `printStr("literal")`   | Print string literal (explicit)  |
| `printString(char arr[])` | Print null-terminated char array (explicit) |

> **Note:** `print()` auto-detects the argument type. When passed a string literal, it prints the string. When passed a `char[]` array, it prints the array contents as a string. When passed an `int`, it prints the numeric value. The explicit `printStr`/`printString` functions are still available but rarely needed.

### GPIO

| Function                             | Description                          |
|--------------------------------------|--------------------------------------|
| `pinMode(int pin, int mode)`         | Set pin mode (1=INPUT, 3=OUTPUT, 5=INPUT_PULLUP, 9=INPUT_PULLDOWN) |
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

### Software Timers

4 independent countdown timers (IDs 0-3) based on `millis()`. Timers run independently of callbacks — set a timer in `main()` or any callback, check it in `EveryLoop()`.

| Function                              | Description                                          |
|---------------------------------------|------------------------------------------------------|
| `timerStart(int id, int ms)`          | Start timer `id` (0-3) with `ms` millisecond timeout |
| `int timerDone(int id)`               | Returns 1 if timer expired (or never started), 0 if running |
| `timerStop(int id)`                   | Cancel timer                                         |
| `int timerRemaining(int id)`          | Milliseconds remaining (0 if expired/stopped)        |

**Example — repeating timer with timeout:**
```c
int counter;

void main() {
    counter = 0;
    timerStart(0, 5000);    // timer 0: every 5 seconds
    timerStart(1, 60000);   // timer 1: stop after 1 minute
}

void EveryLoop() {
    if (timerDone(0)) {
        counter++;
        print(counter);
        timerStart(0, 5000);  // restart for next interval
    }
    if (timerDone(1)) {
        timerStop(0);         // stop repeating timer
    }
}
```

### Serial

Up to 3 serial ports can be open simultaneously. `serialBegin()` returns a **handle** (0–2) that must be passed to all other serial functions. Returns -1 on failure.

| Function                                      | Description                                          |
|-----------------------------------------------|------------------------------------------------------|
| `int serialBegin(int rx, int tx, int baud, int config, int bufsize)` | Open serial port, returns handle (0–2) or -1 on failure |
| `serialPrint(int h, "literal")`               | Print string to serial port `h`                      |
| `serialPrintInt(int h, int value)`            | Print integer to serial port `h`                     |
| `serialPrintFloat(int h, float value)`        | Print float to serial port `h`                       |
| `serialPrintln(int h, "literal")`             | Print string + newline to serial port `h`            |
| `int serialRead(int h)`                       | Read byte from port `h` (-1 if none available)       |
| `int serialAvailable(int h)`                  | Bytes available to read on port `h`                  |
| `serialClose(int h)`                          | Close serial port `h`                                |
| `serialWriteByte(int h, int b)`               | Write single byte to serial port `h`                 |
| `serialWrite(int h, char str[])`              | Write char array to serial port `h` (binary-safe)    |
| `serialWriteBytes(int h, char buf[], int len)`| Write `len` bytes from buffer to serial port `h`     |

**`serialBegin` parameters:**
- `rx` — GPIO pin for receive (-1 to disable RX, e.g. TX-only devices)
- `tx` — GPIO pin for transmit (-1 to disable TX, e.g. RX-only devices)
- `baud` — baud rate (e.g. 9600, 115200)
- `config` — serial frame format (see table below), default 3 = 8N1
- `bufsize` — receive buffer size in bytes (64–2048)

**Serial config values:**

| Value | Format | Value | Format | Value | Format |
|-------|--------|-------|--------|-------|--------|
| 0     | 5N1    | 8     | 5E1    | 16    | 5O1    |
| 1     | 6N1    | 9     | 6E1    | 17    | 6O1    |
| 2     | 7N1    | 10    | 7E1    | 18    | 7O1    |
| **3** | **8N1**| 11    | 8E1    | 19    | 8O1    |
| 4     | 5N2    | 12    | 5E2    | 20    | 5O2    |
| 5     | 6N2    | 13    | 6E2    | 21    | 6O2    |
| 6     | 7N2    | 14    | 7E2    | 22    | 7O2    |
| 7     | 8N2    | 15    | 8E2    | 23    | 8O2    |

**Example — single port:**
```c
// Open serial for LD2410 radar sensor: RX=pin 16, TX=pin 17, 256000 baud, 8N1, 256 byte buffer
int ser = serialBegin(16, 17, 256000, 3, 256);
if (ser < 0) { addLog("Serial open failed"); }

// TX-only for MP3 module: no RX, TX=pin 4, 9600 baud
int mp3 = serialBegin(-1, 4, 9600, 3, 64);
serialWriteByte(mp3, 0x7E);
```

**Example — two ports simultaneously:**
```c
int radar = serialBegin(16, 17, 256000, 3, 256);  // handle 0
int gps   = serialBegin(18, 19,   9600, 3, 256);  // handle 1

void EverySecond() {
  while (serialAvailable(gps) > 0) {
    int b = serialRead(gps);
    // process GPS byte...
  }
}
```

### 1-Wire

| Function                          | Description                                                |
|-----------------------------------|------------------------------------------------------------|
| `owSetPin(int pin)`               | Set GPIO pin for native 1-Wire bus                         |
| `int owReset()`                   | Send reset pulse, return 1 if presence detected            |
| `owWrite(int byte)`               | Write one byte to the bus                                  |
| `int owRead()`                    | Read one byte from the bus                                 |
| `owWriteBit(int bit)`             | Write a single bit (0 or 1)                                |
| `int owReadBit()`                 | Read a single bit                                          |
| `owSearchReset()`                 | Reset the ROM search state                                 |
| `int owSearch(char rom[])`        | Find next device, store 8-byte ROM in `rom[]`, return 1 if found |

> The native 1-Wire functions use hardware-timed bit-banging in C — no external library needed. Requires a 4.7 kΩ pull-up resistor on the data line. For long buses or noisy environments, use a DS2480B serial-to-1-Wire bridge (see `examples/onewire.tc`).

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
| `float exp(float x)`                                | Exponential (e^x)               |
| `float log(float x)`                                | Natural logarithm (ln x)       |
| `float pow(float base, float exp)`                   | Power (base^exp)                |
| `float acos(float x)`                               | Inverse cosine (radians)        |
| `float intBitsToFloat(int bits)`                     | Reinterpret int as IEEE 754 float |
| `int floor(float x)`                                | Integer part (round toward −∞)  |
| `int ceil(float x)`                                 | Integer part + 1 (round toward +∞) |
| `int round(float x)`                                | Round to nearest integer        |

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

Format one or more values into a char array in a single call. The compiler auto-detects each value's type from the format specifier and expands multiple arguments into chained syscalls at compile time.

| Function | Description |
|----------|-------------|
| `int sprintf(char dst[], "fmt", val, ...)` | Format value(s) into dst (overwrites). Type auto-detected. |
| `int sprintfAppend(char dst[], "fmt", val, ...)` | Format value(s), append to end of dst. Type auto-detected. |

> **Legacy aliases:** `sprintfInt`, `sprintfFloat`, `sprintfStr`, `sprintfAppendInt`, `sprintfAppendFloat`, `sprintfAppendStr` still work.

**Format specifiers:** `%d` `%i` `%x` (int), `%f` `%.Nf` `%e` `%g` (float), `%s` (string).
All functions return the total string length.

```c
char buf[128];
char name[] = "sensor";
int id = 1;
float temp = 23.5;

// Multiple values in one call:
sprintf(buf, "id=%d temp=%.1f name=%s", id, temp, name);
// buf = "id=1 temp=23.5 name=sensor"

// sprintfAppend chains onto existing content:
sprintf(buf, "ID=%d", id);
sprintfAppend(buf, " val=%.1f", temp);
// buf = "ID=1 val=23.5"
```

### File I/O

Read and write files on the ESP32 filesystem (LittleFS). In the browser IDE, files are simulated in a virtual filesystem.

| Function                                   | Description                                      |
|--------------------------------------------|--------------------------------------------------|
| `int fileOpen("path", mode)`               | Open file, returns handle (0–3) or -1 on error   |
| `int fileClose(handle)`                    | Close file handle, returns 0 or -1               |
| `int fileRead(handle, char buf[], max)`    | Read up to max bytes into buf, returns count     |
| `int fileWrite(handle, char buf[], len)`   | Write len bytes from buf, returns count          |
| `int fileReadBin(handle, int arr[], count)`  | Read up to `count` int32 elements as 4-byte little-endian binary; returns elements actually read (or -1 on bad args). Works for both `int[]` and `float[]` since both are int32 in memory |
| `int fileWriteBin(handle, int arr[], count)` | Write `count` int32 elements as 4-byte little-endian binary; returns elements actually written (or -1 on bad args). Same dual-type semantics as `fileReadBin` — useful for chart-history persistence and similar fixed-record formats |
| `int fileExists("path")`                   | Check if file exists: 1=yes, 0=no                |
| `int fileDelete("path")`                   | Delete file, returns 0=ok, -1=error              |
| `int fileSize("path")`                     | Get file size in bytes, -1 on error              |
| `int fileSeek(handle, offset, whence)`     | Seek to position. Returns 1=ok, 0=fail           |
| `int fileTell(handle)`                     | Get current position in file, -1 on error        |
| `int fsInfo(int sel)`                      | Filesystem info: sel=0 → total KB, sel=1 → free KB |
| `int fileOpenDir("path")`                  | Open directory for listing, returns handle or -1 |
| `int fileReadDir(handle, char name[])`     | Read next filename into name. Returns 1=entry, 0=end |

**File modes:** `0` = read, `1` = write (create/truncate), `2` = append

**Seek whence:** `0` = SEEK_SET (from start), `1` = SEEK_CUR (from current), `2` = SEEK_END (from end)

**Notes:**
- File paths can be string literals (e.g., `"/data.txt"`) or `char[]` variables
- **Filesystem selection** (Scripter-compatible): default is SD card (`ufsp`). Use `/ffs/` prefix for flash, `/sdfs/` prefix for SD card explicitly: `fileOpen("/ffs/config.txt", 0)` opens from flash, `fileOpen("/data.txt", 0)` opens from SD card
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

// Example: List files in a directory
char fname[64];
int dir = fileOpenDir("/images");
if (dir >= 0) {
    while (fileReadDir(dir, fname)) {
        printString(fname);
        print("\n");
    }
    fileClose(dir);
}
```

**Directory listing notes:**
- `fileOpenDir` uses a file handle slot (same pool as `fileOpen`), close with `fileClose` when done
- `fileReadDir` returns filenames only (no path prefix), skips subdirectories
- Path argument can be a string literal or a char array variable

### Extended File Operations

Filesystem management, structured array I/O, and log file rotation.

| Function | Description |
|----------|-------------|
| `int fileFormat()` | Format LittleFS filesystem (erases all data). Returns 0=ok |
| `int fileMkdir("path")` | Create directory. Returns 1=ok, 0=fail |
| `int fileRmdir("path")` | Remove directory. Returns 1=ok, 0=fail |
| `int fileReadArray(int arr[], handle)` | Read one tab-delimited line into int array. Returns element count |
| `fileWriteArray(int arr[], handle)` | Write array as tab-delimited line with trailing newline |
| `fileWriteArray(int arr[], handle, append)` | Write with append flag: 1=omit newline (for appending multiple arrays on one line) |
| `int fileLog("fname", char str[], limit)` | Append string + newline to file. Remove first line if file exceeds `limit` bytes. Returns file size |
| `int fileDownload("fname", char url[])` | Download URL content to file. Returns HTTP status code (200=ok). Compatible with Scripter's `frw()` |
| `int fileGetStr(char dst[], handle, "delim", index, endChar)` | Search file from start for Nth occurrence of delimiter, extract string until endChar. Returns string length. Compatible with Scripter's `fcs()` |

**fileReadArray / fileWriteArray format:** Values are stored as decimal text separated by TAB characters, one array per line. This is compatible with Scripter's `fra()`/`fwa()` format.

```c
// Example: Save and load array data
int values[5];
values[0] = 100; values[1] = 200; values[2] = 300;
values[3] = 400; values[4] = 500;

int f = fileOpen("/data.tab", 1);    // write mode
fileWriteArray(values, f);           // writes "100\t200\t300\t400\t500\n"
fileClose(f);

int loaded[5];
f = fileOpen("/data.tab", 0);       // read mode
int n = fileReadArray(loaded, f);   // n = 5
fileClose(f);
```

```c
// Example: Rolling log file (max 4096 bytes)
char msg[64];
strcpy(msg, "Sensor reading: 23.5C");
fileLog("/log.txt", msg, 4096);
// Appends line, removes oldest line if file > 4096 bytes
```

```c
// Example: Download file from web
char url[128];
strcpy(url, "http://192.168.1.100/data.csv");
int status = fileDownload("/data.csv", url);
// status = 200 on success, negative on error
```

```c
// Example: Extract 2nd comma-delimited field from CSV file
// File content: "name,temperature,humidity\nSensor1,23.5,65\n"
int f = fileOpen("/data.csv", 0);       // open for reading
char value[32];
int len = fileGetStr(value, f, ",", 2, '\n');
// value = "23.5", len = 4 (content between 2nd comma and newline)
fileClose(f);
```

### File Data Extract (IoT Time-Series)

Extract a time range from tab-delimited CSV data files into float arrays for analysis. Designed for IoT data collectors that log sensor readings at regular intervals.

**Data file format:** First column is a timestamp (ISO or German locale), followed by tab-separated float values. First line may be a header (auto-skipped).

| Function | Description |
|----------|-------------|
| `int fileExtract(handle, char from[], char to[], col_offs, accum, int arr1[], ...)` | Extract rows where `from <= timestamp <= to`. Always seeks from file start. Returns row count |
| `int fileExtractFast(handle, char from[], char to[], col_offs, accum, int arr1[], ...)` | Same but caches file position for efficient sequential time-range queries |

**Parameters:**
- `handle` — open file handle (from `fileOpen`)
- `from`, `to` — timestamp range as char[] (ISO `2024-01-15T12:00:00` or German `15.1.24 12:00`)
- `col_offs` — skip this many data columns before distributing to arrays (0 = start at first data column)
- `accum` — 0: store values, 1: add to existing array values (for combining multiple extracts)
- `arr1, arr2, ...` — variable number of int arrays, one per column to extract (up to 16). Values are stored as IEEE 754 float bit patterns — use float variables or casts to read them

```c
// Example: Extract temperature and humidity for one day
int temp[96], hum[96];  // 96 = 24h * 4 (15-min intervals)
char from[24], to[24];
strcpy(from, "15.12.21 00:00");
strcpy(to, "16.12.21 00:00");

int f = fileOpen("/daily.csv", 0);
// col_offs=4 skips WB,WR1,WR2,WR3 → starts at ATMP_a (5th data col)
int rows = fileExtract(f, from, to, 4, 0, temp, hum);
fileClose(f);
// rows = number of 15-min samples, temp[] and hum[] filled with floats
```

```c
// Example: Sequential daily queries with fileExtractFast
int energy[96];
char from[24], to[24];
int f = fileOpen("/yearly.csv", 0);

strcpy(from, "1.1.24 00:00");
strcpy(to, "2.1.24 00:00");
int r1 = fileExtractFast(f, from, to, 0, 0, energy);
// Next day — fileExtractFast skips already-scanned data
strcpy(from, "2.1.24 00:00");
strcpy(to, "3.1.24 00:00");
int r2 = fileExtractFast(f, from, to, 0, 0, energy);
fileClose(f);
```

### Time / Timestamp Functions

Timestamp conversion and arithmetic. Supports ISO web format (`2024-01-15T12:30:45`) and German locale format (`15.1.24 12:30`). Compatible with Scripter's `tstamp`, `cts`, `tso`, `tsn`, `s2t`.

| Function | Description |
|----------|-------------|
| `int timeStamp(char buf[])` | Get current Tasmota local timestamp into buf. Returns 0 |
| `int timeConvert(char buf[], flg)` | Convert timestamp format in-place. 0=German→Web, 1=Web→German. Returns 0 |
| `int timeOffset(char buf[], days)` | Add `days` offset to timestamp in buf (in-place). Returns 0 |
| `int timeOffset(char buf[], days, zeroFlag)` | With `zeroFlag`=1: also zero the time portion (HH:MM:SS→00:00:00) |
| `int timeToSecs(char buf[])` | Convert timestamp string to epoch seconds. Returns seconds |
| `int secsToTime(char buf[], secs)` | Convert epoch seconds to ISO timestamp string in buf. Returns 0 |

**Format auto-detection:** `timeConvert` and `timeOffset` auto-detect the input format (ISO if contains `T`, German otherwise) and preserve or convert accordingly.

```c
// Example: Get current time and convert formats
char ts[24];
timeStamp(ts);               // ts = "2024-06-15T14:30:00"

char de[24];
strcpy(de, ts);
timeConvert(de, 1);          // de = "15.6.24 14:30"

timeConvert(de, 0);          // de = "2024-06-15T14:30:00" (back to web)
```

```c
// Example: Date arithmetic
char ts[24];
timeStamp(ts);               // "2024-06-15T14:30:00"
timeOffset(ts, 7);           // "2024-06-22T14:30:00" (+ 7 days)
timeOffset(ts, -3, 1);       // "2024-06-19T00:00:00" (- 3 days, zero time)
```

```c
// Example: Convert to seconds and back
char ts[24];
timeStamp(ts);
int secs = timeToSecs(ts);   // epoch seconds

secs = secs + 3600;          // add 1 hour
secsToTime(ts, secs);        // back to timestamp string
```

### Tasmota Command

Execute any Tasmota console command and capture the JSON response.

| Function                                     | Description                                    |
|----------------------------------------------|------------------------------------------------|
| `int tasmCmd("command", char response[])`    | Execute command (string literal), store response, return length |
| `int tasmCmd(char cmd[], char response[])`   | Execute command (char array), store response, return length |

**Notes:**
- Command can be a string literal (e.g., `"Status 0"`) or a `char[]` variable for dynamic commands
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

### Sensor JSON Parsing

Read any Tasmota sensor value by its JSON path. Path segments are separated by `#` (same convention as Tasmota Scripter).

| Function | Description |
|----------|-------------|
| `float sensorGet("Sensor#Key")` | Read sensor value, returns float |

The function internally triggers a sensor status read and navigates the JSON tree. Supports up to 3 levels of nesting.

```c
// Read BME280 sensor
float temp = sensorGet("BME280#Temperature");
float hum = sensorGet("BME280#Humidity");
float press = sensorGet("BME280#Pressure");

// Read SHT3X on address 0x44
float t = sensorGet("SHT3X_0x44#Temperature");

// Read energy meter (if USE_ENERGY_SENSOR defined)
float power = sensorGet("ENERGY#Power");
float voltage = sensorGet("ENERGY#Voltage");
float today = sensorGet("ENERGY#Today");

// Nested: Zigbee device
float zt = sensorGet("ZbReceived#0x2342#Temperature");
```

**Notes:**
- Path must be a string literal (resolved at compile time)
- Returns 0.0 if the sensor or key is not found
- Returns a float — assign to a `float` variable
- In the browser IDE, simulates Temperature=22.5, Humidity=55.0, Pressure=1013.25

### Localized Strings

Retrieve Tasmota's localized display strings at runtime. The strings match the firmware's compile-time language setting (e.g. `en_GB.h`, `de_DE.h`). Use these for web UI labels; JSON keys stay in English.

| Function | Description |
|----------|-------------|
| `int LGetString(int index, char dst[])` | Copy localized string to `dst`, returns length (0 if invalid index) |

**String Index Table:**

| Index | Tasmota Define | English |
|-------|---------------|---------|
| 0 | D_TEMPERATURE | Temperature |
| 1 | D_HUMIDITY | Humidity |
| 2 | D_PRESSURE | Pressure |
| 3 | D_DEWPOINT | Dew point |
| 4 | D_CO2 | Carbon dioxide |
| 5 | D_ECO2 | eCO2 |
| 6 | D_TVOC | TVOC |
| 7 | D_VOLTAGE | Voltage |
| 8 | D_CURRENT | Current |
| 9 | D_POWERUSAGE | Power |
| 10 | D_POWER_FACTOR | Power Factor |
| 11 | D_ENERGY_TODAY | Energy Today |
| 12 | D_ENERGY_YESTERDAY | Energy Yesterday |
| 13 | D_ENERGY_TOTAL | Energy Total |
| 14 | D_FREQUENCY | Frequency |
| 15 | D_ILLUMINANCE | Illuminance |
| 16 | D_DISTANCE | Distance |
| 17 | D_MOISTURE | Moisture |
| 18 | D_LIGHT | Light |
| 19 | D_SPEED | Speed |
| 20 | D_ABSOLUTE_HUMIDITY | Abs Humidity |

**Example:**
```c
char lbl[32];
char buf[80];

void web_row(int idx, float val, char unit[]) {
    LGetString(idx, lbl);
    strcpy(buf, "{s}");
    strcat(buf, lbl);
    strcat(buf, "{m}");
    webSend(buf);
    sprintf(buf, "%.1f ", val);
    strcat(buf, unit);
    strcat(buf, "{e}");
    webSend(buf);
}

void WebCall() {
    web_row(0, temperature, "&deg;C");  // "Temperature" or localized
    web_row(1, humidity, "%");           // "Humidity" or localized
    web_row(2, pressure, "hPa");         // "Pressure" or localized
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
| `void addLogLevel(int level, char buf[])` | Write to Tasmota log at specific level (1=ERROR, 2=INFO, 3=DEBUG, 4=DEBUG_MORE) |
| `void addLogLevel(int level, "literal")` | Write string literal to Tasmota log at specific level |
| `webSendJsonArray(float arr[], int count)` | Emit float array as JSON integer array in web response |

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

### HTTP Requests

Make HTTP GET/POST requests to external APIs. URLs can be string literals or dynamically built in char arrays. Requests are blocking with a 5-second timeout.

| Function | Description |
|----------|-------------|
| `int httpGet(char url[], char response[])` | HTTP GET, returns response length or negative error |
| `int httpPost(char url[], char data[], char response[])` | HTTP POST, returns response length or negative error |
| `void httpHeader(char name[], char value[])` | Set custom header for the next request |
| `int webParse(char source[], "delim", int index, char result[])` | Parse non-JSON response text (see below) |

**Return values:** `> 0` = response body length, `0` = empty response, negative = HTTP error code (e.g., -404).

**Example — Daikin aircon sensor query:**
```c
char url[64];
char response[256];
char token[32];
int len;
int pos;

void main() {
    strcpy(url, "http://192.168.188.43/aircon/get_sensor_info");
    len = httpGet(url, response);
    // response = "ret=OK,htemp=19.0,hhum=-,otemp=7.0,err=0,cmpfreq=0"

    if (len > 0) {
        // Extract indoor temperature (htemp)
        pos = strFind(response, token);  // find "htemp="
        strToken(token, response, ',', 3);  // 3rd token = "htemp=19.0"
        printString(token);
    }
}
```

**Example — Tasmota command to another device:**
```c
char url[128];
char response[512];
int len;

void EverySecond() {
    strcpy(url, "http://192.168.1.100/cm?cmnd=Status%200");
    len = httpGet(url, response);
    if (len > 0) {
        print(len);
        // parse response with strFind/strToken...
    }
}
```

**Example — POST with custom header:**
```c
char url[128];
char data[128];
char hname[32];
char hval[64];
char response[512];

void main() {
    strcpy(url, "http://192.168.1.100/api/data");
    strcpy(data, "{\"value\":42}");
    strcpy(hname, "Content-Type");
    strcpy(hval, "application/json");
    httpHeader(hname, hval);  // set header before request
    int len = httpPost(url, data, response);
}
```

**webParse() — Parse non-JSON web responses**

Equivalent to Scripter's `gwr()`. Extracts data from plain-text HTTP responses (key=value, CSV, line-based formats).

**Two modes:**
- **index > 0** — Split `source` by `delim`, return the Nth segment (1-based). Returns length.
- **index < 0** — Find `delim=value` pattern, extract value (stops at `,`, `:`, or NUL). Returns length.
- **index == 0** — No-op, returns 0.

**Example — Daikin aircon with webParse:**
```c
char url[64];
char response[256];
char value[32];

void main() {
    strcpy(url, "http://192.168.188.43/aircon/get_sensor_info");
    int len = httpGet(url, response);
    // response = "ret=OK,htemp=19.0,hhum=-,otemp=7.0,err=0,cmpfreq=0"

    if (len > 0) {
        // name=value mode: extract value after "htemp="
        webParse(response, "htemp", -1, value);  // value = "19.0"
        float temp = atof(value);
        print(temp);  // 19.0

        // split mode: get 4th comma-separated field
        webParse(response, ",", 4, value);  // value = "otemp=7.0"
        printString(value);
    }
}
```

### TCP Server

Start a TCP stream server to accept incoming connections. Only one client is served at a time.

| Function | Description |
|----------|-------------|
| `int tcpServer(int port)` | Start TCP server on `port`. Returns 0=ok, -1=fail, -2=no network |
| `tcpClose()` | Close TCP server and disconnect client |
| `int tcpAvailable()` | Accept pending client and return bytes available to read |
| `int tcpRead(char buf[])` | Read string from TCP client into `buf`. Returns bytes read |
| `tcpWrite(char str[])` | Write string to TCP client |
| `int tcpReadArray(int arr[])` | Read available bytes into int array (one byte per element). Returns count |
| `tcpWriteArray(int arr[], int num)` | Write `num` array elements as uint8 bytes to TCP client |
| `tcpWriteArray(int arr[], int num, int type)` | Write with type: 0=uint8, 1=uint16 BE, 2=sint16 BE, 3=float BE |

**Example — Simple TCP echo server:**
```c
char buf[128];

void main() {
    tcpServer(8888);   // listen on port 8888
}

void Every50ms() {
    int n = tcpAvailable();  // accept client + check available
    if (n > 0) {
        tcpRead(buf);        // read incoming string
        tcpWrite(buf);       // echo it back
    }
}
```

**Example — Binary data streaming:**
```c
int data[100];

void main() {
    tcpServer(9000);
}

void EverySecond() {
    int n = tcpAvailable();
    if (n > 0) {
        // read raw bytes into array
        int count = tcpReadArray(data);
        print(count);
        // send back as uint16 big-endian
        tcpWriteArray(data, count, 1);
    }
}
```

### TCP Client

Open outgoing TCP connections to remote hosts. Up to **4 parallel client slots** are supported; a selector picks the active slot, and all read/write calls operate on that slot. Slot 0 additionally falls back to the server-accepted client from `tcpServer()`, so the same `tcpRead`/`tcpWrite`/`tcpAvailable` API works for both roles.

| Function | Description |
|----------|-------------|
| `int tcpConnect("host", port)` | Open a TCP connection from the active slot to `host:port`. Returns 0=connected, -1=fail, -2=no network |
| `int tcpConnect(char host[], port)` | Same, with a char-array host (IP or DNS name) instead of a literal |
| `int tcpConnected()` | Returns 1 if the active slot has an open connection, 0 otherwise |
| `tcpDisconnect()` | Close the active slot's client connection |
| `tcpSelect(int slot)` | Select the active client slot (0–3). All subsequent client calls target this slot |

**Notes:**
- `tcpRead(buf)`, `tcpWrite(buf)`, `tcpAvailable()`, `tcpReadArray()`, `tcpWriteArray()` all operate on the **active** slot. Call `tcpSelect(n)` to switch.
- `tcpWrite()` still requires a `char[]` — string literals are not accepted (declare `char msg[] = "hello\n"; tcpWrite(msg);`).
- Slot 0 is special: if no outgoing client is open on slot 0, it transparently falls back to the server-side client from `tcpServer()`. This lets existing server-only scripts keep working unchanged.
- Connections are non-blocking-ish but have a short socket-level timeout — a failed `tcpConnect()` returns quickly with -1.

**Example — Periodic TCP client sending a heartbeat:**
```c
char rxbuf[128];
char msg[]  = "ping\n";

void EverySecond() {
    tcpSelect(0);                          // active slot = 0
    if (!tcpConnected()) {
        int r = tcpConnect("192.168.1.50", 1234);
        if (r != 0) { return; }            // retry next tick
    }
    tcpWrite(msg);
    delay(150);                            // give server a beat to reply
    if (tcpAvailable() > 0) {
        int n = tcpRead(rxbuf);
        print(n);                          // e.g. 24 bytes echoed back
    }
}

void OnExit() {
    tcpDisconnect();                       // clean up on script stop
}
```

**Example — Two independent TCP clients in parallel:**
```c
char buf[128];
char hello[] = "hello\n";

void main() {
    tcpSelect(0);
    tcpConnect("10.0.0.10", 9000);         // slot 0 → metrics server

    tcpSelect(1);
    tcpConnect("10.0.0.11", 9001);         // slot 1 → command server
}

void EverySecond() {
    // Push heartbeat on slot 0
    tcpSelect(0);
    if (tcpConnected()) { tcpWrite(hello); }

    // Poll replies on slot 1
    tcpSelect(1);
    if (tcpConnected() && tcpAvailable() > 0) {
        tcpRead(buf);
        // dispatch command in buf...
    }
}
```

#### TCP Client tuning *(since 1.5.1)*

Four per-slot helpers for production-grade outgoing TCP work. All
operate on the **currently selected** slot, so call `tcpSelect(N)`
first. Solve the recurring SMA / Solar-Edge / Powerwall idle-
disconnect pattern and the Modbus-TCP request-response boilerplate.

| Function | Description |
|----------|-------------|
| `int tcpKeepalive(int idle_sec, int intvl_sec, int count)` | Enable SO_KEEPALIVE on the active slot and set TCP_KEEPIDLE / TCP_KEEPINTVL / TCP_KEEPCNT via direct setsockopt. Returns 1=ok, 0=err. Typical SMA Tripower setting: `tcpKeepalive(30, 10, 3)` — after 30 s idle, send up to 3 probes spaced 10 s apart before declaring dead. Solves the "peer drops idle connection after 60 s" pattern. |
| `tcpNoDelay(int on)` | Toggle Nagle's algorithm on the active slot. `tcpConnect()` already calls `setNoDelay(true)` by default; use this to re-enable Nagle for high-throughput bulk transfers. |
| `int tcpDisconnectReason()` | Returns last disconnect reason for the active slot: 0=NEVER (never connected), 1=CONNECTED (still open), 2=PEER_CLOSED (FIN), 3=TIMEOUT, 4=NETWORK (down), 5=USER_CLOSED. Lets a reconnect watchdog react intelligently to RST/FIN vs. network errors instead of blind retries. |
| `int tcpTransact(char req[], int req_len, char resp[], int resp_max, int timeout_ms)` | Atomic write-and-await-reply on the active slot — folds the `tcpWriteArray + poll-tcpAvailable + tcpReadArray` pattern into a single syscall. Returns bytes received on success (all immediately-available bytes up to `resp_max`); -1 timeout; -2 not connected or peer dropped mid-wait (`tcpDisconnectReason()` set to PEER_CLOSED); -3 bad arguments. Holds the calling slot's `vm_mutex` throughout — designed to run from a `spawnTask` worker, blocking that slot's other callbacks for ≤200 ms is fine. Suitable for protocols where the response fits in one TCP segment (Modbus-TCP, ≤256 B). |

**Example — Modbus-TCP poll with one-shot request/response:**
```c
char req[12]  = {0,1, 0,0, 0,6,  1, 3, 0,0x10, 0,4};  // FC03 read 4 regs
char resp[260];

void main() {
    tcpSelect(0);
    tcpConnect("192.168.1.50", 502);
    tcpKeepalive(30, 10, 3);                  // SMA-style keep-alive
}

void EverySecond() {
    tcpSelect(0);
    int n = tcpTransact(req, 12, resp, 260, 200);   // ≤200 ms
    if (n > 0) {
        // resp[0..n-1] = MBAP header + FC03 response
    } else if (n == -2) {
        int reason = tcpDisconnectReason();
        if (reason == 2 || reason == 4) tcpConnect("192.168.1.50", 502);
    }
}
```

See `examples/modbus_lib.tc` for the canonical `mbFC03/04/06/16`
helpers built on `tcpTransact`.

### MQTT Subscribe / Publish

Subscribe to MQTT topics and react to inbound messages, or publish arbitrary payloads. Requires `USE_MQTT` in the firmware build (enabled by default).

| Function | Description |
|----------|-------------|
| `int mqttSubscribe("topic")` | Subscribe to `topic`. Returns the subscription slot (0–9) on success, -1 on failure (no free slot, broker down) |
| `int mqttSubscribe(char topic[])` | Same, with a char-array topic (runtime-built) |
| `int mqttUnsubscribe("topic")` | Unsubscribe from a previously subscribed topic. Returns 0=ok, -1=not found |
| `mqttPublish("topic", "payload")` | Publish `payload` to `topic` (both literals or char arrays accepted) |

**Notes:**
- Up to **10 subscriptions** per VM, topic max **128 chars**.
- Wildcard `'#'` is supported as a **trailing prefix match** only (`"sensors/#"` matches `sensors/temp`, `sensors/humi/1`, etc.). MQTT's `+` single-level wildcard is not supported.
- Matching topics trigger the `OnMqttData(char topic[], char payload[])` callback. The two strings are copied into the VM heap for the duration of the callback.
- Subscriptions persist across `TinyCRun` reloads of the same slot. Call `mqttUnsubscribe()` in `OnExit()` if you want a clean slate on restart.
- Subscriptions are automatically re-sent to the broker on reconnect (hooked into `FUNC_MQTT_INIT`).

**Example — Remote control via MQTT:**
```c
char reply[64];

void main() {
    mqttSubscribe("cmnd/room1/#");         // wildcard prefix
    mqttSubscribe("home/heartbeat");       // exact match
}

void OnMqttData(char topic[], char payload[]) {
    if (strcmp(topic, "home/heartbeat") == 0) {
        mqttPublish("stat/room1/alive", "ok");
        return;
    }
    // cmnd/room1/light → toggle GPIO etc.
    sprintf(reply, "got %s = %s", topic, payload);
    addLogLevel(2, reply);
}

void OnExit() {
    mqttUnsubscribe("cmnd/room1/#");
    mqttUnsubscribe("home/heartbeat");
}
```

### mDNS Service Advertisement

Register the device as an mDNS service on the local network, enabling device emulation (Everhome ecotracker, Shelly, or custom services).

| Function | Description |
|----------|-------------|
| `int mdnsRegister("name", "mac", "type")` | Start mDNS responder and advertise service. Returns 0 on success |

**Parameters (all string literals):**
- **name** — hostname prefix. Use `"-"` for Tasmota's default hostname, or a custom prefix (MAC is appended automatically)
- **mac** — MAC address. Use `"-"` for device's own MAC (lowercase, no colons), or provide a custom string
- **type** — service type: `"everhome"` (ecotracker), `"shelly"`, or any custom service name

**Built-in emulation types:**
- `"everhome"` — registers `_everhome._tcp` with IP, serial, productid TXT records
- `"shelly"` — registers `_http._tcp` and `_shelly._tcp` with firmware metadata TXT records
- Any other string — registers `_<type>._tcp` with IP and serial TXT records

**Example — Everhome ecotracker emulation:**
```c
int main() {
    mdnsRegister("ecotracker-", "-", "everhome");
    return 0;
}
```

This is equivalent to Scripter's `mdnsRegister("ecotracker-", "-", "everhome")`.

### WebUI Widgets

Create interactive dashboards using widget functions. Widgets can appear in two places:

1. **Dedicated `/tc_ui` page** — use the `WebUI()` callback
2. **Tasmota main page** (sensor section) — use the `WebCall()` callback

Both callbacks use the same widget functions.

| Function | Description |
|----------|-------------|
| `webButton(var, "label")` | Momentary action button — pulses `var` to 1 on click (script reads it, acts, resets to 0). No ON/OFF suffix. Optional `"Idle\|Active"` label shows the `Active` text on the button for ~2.5 s as a click confirmation, then reverts (generic ✓ if no `\|`) |
| `webToggle(var, "label")` | Latching on/off button (0/1) — **green when `var`≠0, grey when 0**, click flips it. Optional `"On\|Off"` label shows different text/emoji per state (e.g. `"💡 An\|🌙 Aus"`); no `\|` → same text both states, colour only |
| `webSlider(var, min, max, "label")` | Range slider — drag to set value |
| `webCheckbox(var, "label")` | Checkbox (0/1) — check/uncheck toggles |
| `webText(chararray, maxlen, "label")` | Text input — edit string variable |
| `webNumber(var, min, max, "label")` | Number input with min/max bounds |
| `webPulldown(var, "label", "opt0\|opt1\|opt2")` | Dropdown select with label — pipe-separated options, 0-based index. Use `"@getfreepins"` as options to show available GPIO pins |
| `webRadio(var, "opt0\|opt1\|opt2")` | Radio button group — pipe-separated options, 0-based index |
| `webTime(var, "label")` | Time picker (HH:MM) — stored as HHMM integer (e.g., 1430 = 14:30) |
| `webPageLabel(page, "label")` | Register page 0–5 with a button label on the main page |
| `int webPage()` | Returns current page number being rendered (use in `WebUI()` to branch) |
| `webConsoleButton("/url", "label")` | Register button in Tasmota Utilities menu (max 4). Navigates to URL on click |

The first argument of widget functions is always a **global variable** that the widget reads from and writes to. The compiler automatically passes the variable's address to the syscall.

**Example — Widgets on the main page:**
```c
int relay;
int brightness;

void WebCall() {
    webToggle(relay, "Power");
    webSlider(brightness, 0, 100, "Brightness");
}
```

**Example — Multiple pages with custom buttons:**

Up to 6 pages can be registered with `webPageLabel()`. Each creates a button on the Tasmota main page. Use `webPage()` inside `WebUI()` to render different widgets per page.

```c
int power;
int brightness;
int mode;
int alarm_time;
char devname[32];

void WebUI() {
    int page = webPage();
    if (page == 0) {
        webToggle(power, "Power");
        webSlider(brightness, 0, 100, "Brightness");
        webPulldown(mode, "Mode", "Off|Auto|Manual");
    }
    if (page == 1) {
        webTime(alarm_time, "Wake-up Time");
        webText(devname, 32, "Device Name");
    }
}

int main() {
    webPageLabel(0, "Controls");   // first button on main page
    webPageLabel(1, "Settings");   // second button on main page
    return 0;
}
```

If no `webPageLabel()` is called but `WebUI()` exists, a single "TinyC UI" button appears.

**How it works:**
1. `WebCall()` renders widgets in the sensor section of the Tasmota main page
2. `WebUI()` renders widgets on dedicated pages at `http://<device>/tc_ui?p=N`
3. `webPageLabel(N, "text")` registers page N (0–5) with a button on the main page
4. `webPage()` returns the current page number so `WebUI()` can show different widgets
5. When you move a slider / click a button, JavaScript sends the new value via AJAX
6. The server writes the value directly into the TinyC global variable
7. The page auto-refreshes to show updated state
8. Text and number inputs pause auto-refresh while you're editing (resumes on blur)

### WebChart — Automatic Google Charts

`WebChart()` renders Google Charts on the Tasmota main page with a single function call per data series. It automatically loads the Google Charts library and generates all required JavaScript.

```c
void WebChart(int type, "title", "unit", int color, int pos, int count,
              float array[], int decimals, int interval, float ymin, float ymax)
```

| Parameter | Description |
|-----------|-------------|
| `type` | Chart type: `0` = line chart, `1` = column chart |
| `"title"` | Chart title (string literal). Empty `""` = add series to previous chart |
| `"unit"` | Y-axis unit label (string literal, e.g. `"°C"`, `"%"`, `"m/s"`) |
| `color` | Line/bar color as hex RGB (e.g. `0xe74c3c` for red) |
| `pos` | Current write position in the ring buffer |
| `count` | Number of valid data points (≤ array size) |
| `array` | Float array containing the data (ring buffer) |
| `decimals` | Number of decimal places for data values (0–6) |
| `interval` | Minutes between data points (for X-axis time labels) |
| `ymin` | Y-axis minimum. If `ymin >= ymax`, chart auto-scales |
| `ymax` | Y-axis maximum. If `ymin >= ymax`, chart auto-scales |

**Example — 24h weather charts:**
```c
#define NPTS 288       // 24h at 5-min intervals
persist float h_temp[NPTS];
persist float h_hum[NPTS];
persist int h_pos = 0;
persist int h_count = 0;

void WebPage() {
    if (h_count < 1) return;
    WebChart(0, "Temperature", "\u00b0C", 0xe74c3c, h_pos, h_count, h_temp, 1, 5, -20, 50);
    WebChart(0, "Humidity",    "%",        0x3498db, h_pos, h_count, h_hum,  1, 5, 0, 100);
}
```

**Chart size:** Use `webChartSize(width, height)` to set custom chart dimensions in pixels before a `WebChart()` call. Pass 0 for either parameter to use the default size.

- Use **fixed range** for data with known bounds (humidity 0–100, UV index 0–12)
- Use **auto-scale** (`0, 0`) for data with variable range (brightness, wind, rain)
- Call from `WebPage()` callback — each call emits one data series
- Multiple series on one chart: first call has a title, subsequent calls use `""` as title

**Including HTML from files:**

Use `webSendFile("filename")` to send the contents of a file from the device filesystem directly to the web page. This is useful for large HTML, CSS, or JavaScript that would be too big to compile into bytecode constants.

```c
void WebPage() {
    webSendFile("chart.html");  // include chart library from /chart.html
}
```

The file is read in 256-byte chunks and sent via `WSContentSend`. The filename can be with or without leading `/`.

### Custom Web Handlers

Register custom HTTP endpoints on the Tasmota web server. When a request arrives, the `WebOn()` callback is invoked with the handler number accessible via `webHandler()`.

| Function | Description |
|----------|-------------|
| `webOn(int num, "url")` | Register handler 1–4 for the given URL path |
| `int webHandler()` | Returns the handler number (1–4) inside `WebOn()` callback |
| `int webArg("name", buf)` | Read HTTP request argument into char buffer, returns length (0 if missing) |

Use `webSend(buf)` to emit the response body. The response content type is `text/plain` by default.

**Example — JSON API endpoint:**
```c
char buf[128];

void WebOn() {
    int h = webHandler();
    if (h == 1) {
        // GET /v1/json?id=xxx
        char id[32];
        int len = webArg("id", id);
        sprintf(buf, "{\"handler\":1,\"id\":\"%s\",\"value\":42}", id);
        webSend(buf);
    }
}

int main() {
    webOn(1, "/v1/json");
    return 0;
}
```

**Example — Multiple endpoints:**
```c
void WebOn() {
    int h = webHandler();
    char buf[64];
    if (h == 1) {
        sprintf(buf, "{\"temp\":%.1f}", smlGet(1));
        webSend(buf);
    }
    if (h == 2) {
        webSend("OK");
    }
}

int main() {
    webOn(1, "/api/sensor");
    webOn(2, "/api/ping");
    return 0;
}
```

**Notes:**
- Up to 4 handlers can be registered (1–4)
- URLs must start with `/` (e.g., `/v1/json`, `/api/data`)
- `webOn()` is called in `main()` — handlers are registered at program start
- `WebOn()` callback runs after `main()` has returned (same as other callbacks)
- `webArg()` reads both GET query parameters and POST form fields
- Equivalent to Scripter's `won(N, "/url")` + `>onN` section
- CORS is enabled so endpoints are accessible from external apps

#### Raw HTTP responses + keep-alive *(since 1.6.0)*

By default, `webSend()` inside a `WebOn()` handler routes through
Tasmota's `WSContentSend` which auto-emits a chunked HTML response with
`Content-Type: text/html` + `Connection: close`. That's wrong for clients
that expect specific headers (Jackery EcoTracker emu, plain JSON APIs)
or that need to keep the socket open across multiple requests (HTTP/1.1
keep-alive). The raw-mode trio bypasses `WSContentSend`:

| Function | Description |
|----------|-------------|
| `webRawMode()` | Inside a `WebOn()` handler: switch the response builder to raw-bytes mode. After this, NOTHING is auto-emitted — you must write the full HTTP response yourself (status line + headers + blank line + body). |
| `webRawWrite(char buf[])` | Write `buf` raw bytes to the underlying TCP client. Streams via the standard `tc_stream_ref` chunking (256 B at a time), so unlimited-size payloads work. Replaces `webSend()` in raw mode. |
| `webKeepAlive()` | Mark the response as keep-alive. The framework keeps the TCP socket open after the handler returns so the same client can send another request without reconnecting. Requires `USE_HTTP_KEEPALIVE` in firmware (ESP32 default). |

**Example — Jackery EcoTracker emulation (exactly 3 headers, JSON body, keep-alive):**
```c
char hdr[160];

void WebOn() {
    if (webHandler() != 1) return;
    webRawMode();
    char body[64];
    sprintf(body, "{\"power\":%d,\"powerAvg\":%d,\"energyCounterIn\":%d,\"energyCounterOut\":%d}",
            p, p_avg, e_in, e_out);
    sprintf(hdr, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n",
            strlen(body));
    webRawWrite(hdr);
    webRawWrite(body);
    webKeepAlive();
}

int main() { webOn(1, "/"); return 0; }
```

This is what `examples/ecotracker.tc` and `examples/marstek_emu.tc` use.
No `webSend()` calls in the handler at all — the raw 3-header response
is byte-identical to what a real EcoTracker emits.

### UDP Multicast (Scripter-compatible)

Share float variables between Tasmota devices via UDP multicast on 239.255.255.250:1999.
Compatible with Tasmota Scripter's global variable protocol.

| Function | Description |
|----------|-------------|
| `float udpRecv("name")` | Get last received value for named variable (0 if none) |
| `int udpReady("name")` | Returns 1 if new value received since last check |
| `void udpSendArray("name", float_arr, count)` | Broadcast a float array via binary multicast |
| `int udpRecvArray("name", float_arr, maxcount)` | Receive float array, returns actual count |
| `udpSendStr("name", char str[])` | Send string via multicast (ASCII mode `=>name=...`) |

**Protocol:**
- Single float: send `=>name:[4 bytes IEEE-754 float]`
- Float array: send `=>name:[2-byte LE count][N × 4-byte float]`
- Receive: both ASCII (`=>name=value`) and binary (single or array)
- Multicast group: `239.255.255.250`, port `1999`
- Max 8 tracked variable names, 16 chars each
- Max 64 floats per array

**Callback:** Define `void UdpCall()` to be notified on each received variable.
UDP socket is auto-initialized on first global variable write, `udpRecv()`, or `udpReady()` call.
Scalar `global` float variables automatically broadcast via UDP when assigned (no explicit send needed).

**Socket Watchdog:** The multicast socket has a built-in inactivity watchdog (default: 60 seconds). If no packet is received within the timeout period, the socket is automatically closed and re-opened. This recovers from the known ESP32 issue where the UDP receive path silently stops working after a variable amount of time. Use `udp(8, 0, seconds)` to change the timeout (0 = disable).

**Example (scalar — auto-broadcast):**
```c
global float temperature = 0.0;  // declared as 'global' → auto-broadcasts on write

void EverySecond() {
    temperature = 20.0 + sin(counter) * 5.0;
    // No udpSend() needed — assigning a 'global' variable auto-broadcasts it
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

### General-Purpose UDP

Scripter-compatible `udp()` function for arbitrary UDP communication. Uses a separate socket from the multicast variable sharing above.

| Function | Description |
|----------|-------------|
| `int udp(0, int port)` | Open a listening UDP port. Returns 1 on success |
| `int udp(1, char buf[])` | Read received string into buf. Returns byte count (0 = nothing) |
| `void udp(2, char str[])` | Reply to sender's IP and port |
| `void udp(3, char url[], char str[])` | Send string to url using the port from `udp(0)` |
| `int udp(4, char buf[])` | Get remote sender IP as string. Returns length |
| `int udp(5)` | Get remote sender port number |
| `int udp(6, char url[], int port, char str[])` | Send string to arbitrary url:port |
| `int udp(7, char url[], int port, int arr[], int count)` | Send array as raw bytes to url:port |
| `int udp(8, int which, int seconds)` | Set socket inactivity timeout (which: 0=multicast, 1=general port; 0=disable) |
| `int udp(9, char mcast_ip[], int port)` | Join arbitrary UDP multicast group, bind to port. Returns 1 on success |

**Notes:**
- The first argument (mode) must be a literal integer (0-9)
- Modes 6 and 7 create a temporary socket for each send (no prior `udp(0)` needed)
- Mode 1 is non-blocking: returns 0 immediately if no packet is available
- Mode 7 sends the lower byte of each array element
- Mode 8 configures the socket watchdog: if no packet is received within `seconds`, the socket is automatically reset. Default is 60 seconds. Set to 0 to disable.
- Mode 9 joins a custom multicast group (e.g. SMA Speedwire `239.12.255.254:9522`). Reuses the `udp(1, buf)` read path. Replaces any unicast `udp(0, ...)` binding on the same socket; call `udp(0, port)` again to switch back to unicast.

```c
char buf[128];
char ip[20];

void main() {
    udp(0, 5000);  // listen on port 5000
}

void Every50ms() {
    int n = udp(1, buf);  // check for incoming
    if (n > 0) {
        udp(4, ip);               // get sender IP
        int port = udp(5);        // get sender port
        udp(2, "ACK");            // reply to sender
    }
}

void sendData() {
    char msg[64];
    strcpy(msg, "hello");
    udp(6, "192.168.1.100", 5000, msg);  // send to specific IP:port
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
| `int i2cSetDevice(int addr, int bus)` | Check if address is **unclaimed** and responsive. Returns 1=available |
| `i2cSetActiveFound(int addr, "type", int bus)` | Register address as claimed by your driver. Logs discovery |
| `int i2cReadRS(int addr, int reg, char buf[], int len, int bus)` | Read with repeated-start (SMBus). Keeps bus held between write and read phase |
| `I2cResetActive(int addr, int bus)` | Release a previously claimed I2C address (undo `i2cSetActiveFound`) |

**Notes:**
- `bus` = 0 or 1 — selects which I2C bus to use
- Address is 7-bit (0x00–0x7F), e.g. `0x48` for TMP102
- Register is 8-bit (0x00–0xFF)
- Buffer functions use `char[]` arrays — each element holds one byte (0–255)
- Maximum buffer length is 255 bytes
- Returns 0 if I2C is not compiled in or the operation fails
- Use `i2cSetDevice` + `i2cSetActiveFound` to properly claim I2C addresses and prevent conflicts with Tasmota's built-in drivers

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
        sprintf(out, "TMP102: %.2f °C\n", temp);
        printString(out);
    }
}
```

### Smart Meter (SML)

Read meter values and control meters via Tasmota's SML driver (requires `USE_SML` or `USE_SML_M`).

SML can run **without Scripter** — only `USE_UFILESYS` is needed for file-based meter descriptors.
The IDE's SML Descriptor tab manages the meter definition file (`/sml_meter.def`) on the device.

> **⚠ Gotcha: Rule1 is shared with Scripter.** The SML driver gates on
> `bitRead(rule_enabled, 0)` and only runs when **Rule1** is on (set via
> `tasm_rule = 1` from TinyC, or the `Rule1 1` console command). The same
> bit also enables any Scripter `>S` section that's still on the device.
> If a legacy `*.tas` script is left in flash (e.g. an old ottelo
> `1_SML_Chart.tas` or `2_SML_Chart_PV.tas`), it will start emitting its
> own chart HTML / `setOnLoadCallback` registrations alongside your
> TinyC `WebPage()` output the moment you enable SML — chart targets
> collide, JS callbacks overwrite each other, and the main page renders
> as a mess of half-drawn charts.
>
> **Fix when porting from Scripter:** delete the Scripter source via the
> IDE's *Tools → Edit Script* (clear the text area, save). The SML
> descriptor in `/sml_meter.def` is independent and stays put.

#### Reading Meter Values

| Function | Description |
|----------|-------------|
| `float smlGet(int index)` | Get meter value. Index 0 returns count, 1..N returns values |
| `int smlGetStr(int index, char buf[])` | Positive index: meter ID/OBIS string. Negative index: full-precision numeric value as string (4 decimals) — equivalent to Scripter's `smls[-x]` |

**Notes:**
- Index is 1-based: `smlGet(1)` returns the first meter value
- `smlGet(0)` returns the total number of meter variables
- Returns 0 if SML is not compiled in or index is out of range
- `smlGet()` values match Scripter's `sml[x]` syntax (single-precision float)
- `smlGetStr(-i, buf)` formats the underlying `double` SML value with 4 decimal places — use when cumulative energy meters exceed `float`'s ~7-digit precision

**Example:**
```c
void WebCall() {
    char buf[64];
    int n = smlGet(0);  // total meters
    int i = 1;
    while (i <= n) {
        float val = smlGet(i);
        sprintf(buf, "{s}Meter %d{m}%.2f{e}", val);
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
    sprintf(out, "Thermocouple: %.2f °C\n", temp);
    printString(out);
    return 0;
}
```

### TWAI / CAN Bus (ESP32)

ESP32 has a built-in TWAI (Two-Wire Automotive Interface, electrically
identical to CAN 2.0) controller — most ESP32-S3 / ESP32-C3 / ESP32-C6
boards expose it through any two GPIOs via the GPIO matrix. A 3.3 V
CAN transceiver (SN65HVD230, TCAN332, TJA1051 with level shifter, etc.)
is required between MCU and the differential bus pair (CAN_H / CAN_L).

| Function | Description |
|----------|-------------|
| `int twaiBegin(int rx_pin, int tx_pin, int kbits, int mode)` | Install + start the TWAI driver. `kbits` ∈ {10, 25, 50, 100, 125, 250, 500, 800, 1000}. `mode`: 0=NORMAL (ACK on bus), 1=NO_ACK (self-test, useful with loopback jumper for software validation). Returns 0=ok, -1=err |
| `twaiEnd()` | Stop driver, release pins. Required before any subsequent `twaiBegin()` (driver is single-instance) |
| `int twaiAvailable()` | Number of RX frames waiting in the driver queue. 0 if empty |
| `int twaiRecv(int meta[], char data[], int max_dlc)` | Drain one RX frame. `meta[0]=ID, meta[1]=ext_flag, meta[2]=dlc`. `data[0..dlc-1]` filled with payload bytes. Returns the number of payload bytes read (0 = queue empty, < 0 = driver error) |
| `int twaiSend(int id, char data[], int dlc, int ext_flag)` | Transmit one frame. Returns 0=ok, -1=err. `ext_flag=0` standard 11-bit ID, `=1` extended 29-bit |
| `int twaiStatus(int counters[])` | Snapshot of driver state. Fills `counters[]` with `[state, tx_err, rx_err, tx_failed, rx_missed, arb_lost, bus_err]`. Returns state code: 0=stopped, 1=running, 2=bus-off, 3=recovering |
| `int twaiFilter(int id_acc, int id_mask, int ext_flag)` | Install acceptance filter. `id_acc` matches `RX_ID & ~id_mask`; `id_mask=0` accepts only the exact ID, `id_mask=0x1FFFFFFF` accepts all. Set BEFORE `twaiBegin()` for it to take effect (driver re-install required to change). Returns 0=ok |

**Notes:**
- Mode 1 (NO_ACK) is for software bring-up only — the controller drives TX but never expects an ACK, letting you validate the protocol stack with a TX→RX jumper on the same MCU before a transceiver is wired up.
- After a bus-off (state 2), call `twaiEnd()` + `twaiBegin()` to recover. Some drivers also support `twai_initiate_recovery()` via state 3 but the simpler restart is preferred from script.
- ESP32-C3 GPIO 9 is a BOOT strap pin with weak pull-up. It works as TWAI-TX but **NOT** as TWAI-RX (the strap holds the line and the controller sees a permanent dominant). Pick RX on a non-strap pin (10, 18, 19, etc.).

**Example — sniffer that logs every frame:**
```c
int rx_meta[4];
char rx_data[8];
int rx_total = 0;

void EveryLoop() {
    while (twaiAvailable() > 0) {
        int n = twaiRecv(rx_meta, rx_data, 8);
        if (n <= 0) break;
        rx_total = rx_total + 1;
        char ext = rx_meta[1] ? 'E' : 'S';
        addLog("CAN RX #%d %cID=0x%X DLC=%d  %02X %02X %02X %02X %02X %02X %02X %02X",
            rx_total, ext, rx_meta[0], rx_meta[2],
            rx_data[0], rx_data[1], rx_data[2], rx_data[3],
            rx_data[4], rx_data[5], rx_data[6], rx_data[7]);
    }
}

int main() {
    twaiBegin(38, 39, 250, 0);   // rx=38, tx=39, 250 kbit/s, NORMAL
    addLog("CAN sniffer ready");
    return 0;
}
```

See `examples/slcan_bridge_tcp.tc` for a full SLCAN-over-TCP bridge.

### Display Drawing

Requires a Tasmota build with `USE_DISPLAY` enabled and a configured display driver. All drawing functions operate on the Tasmota display renderer directly — much more efficient than building DisplayText command strings.

#### Setup & Control

| Function | Description |
|----------|-------------|
| `dspClear()` | Clear display, reset position to (0,0) |
| `dspPos(x, y)` | Set current draw position (pixels) |
| `dspFont(f)` | Set font (0-7), resets text size to 1 for non-GFX fonts |
| `dspSize(s)` | Set text size multiplier |
| `dspColor(fg, bg)` | Set foreground and background color (16-bit RGB565) |
| `dspPad(n)` | Set text padding for `dspDraw()`: positive = left-aligned padded to n chars, negative = right-aligned padded to n chars, 0 = off |
| `dspDim(val)` | Set display brightness (0-15) |
| `dspOnOff(on)` | Turn display on (1) or off (0) |
| `dspUpdate()` | Force display update (required for e-paper displays) |
| `dspWidth()` | Returns display width in pixels |
| `dspHeight()` | Returns display height in pixels |

#### Drawing Primitives

All primitives use the current position set by `dspPos()` and the current foreground color set by `dspColor()`.

| Function | Description |
|----------|-------------|
| `dspDraw(buf)` | Draw text string at current position |
| `dspPixel(x, y)` | Draw single pixel at (x,y) |
| `dspLine(x1, y1)` | Draw line from current pos to (x1,y1), updates pos |
| `dspHLine(w)` | Horizontal line from current pos, width w, updates pos |
| `dspVLine(h)` | Vertical line from current pos, height h, updates pos |
| `dspRect(w, h)` | Draw rectangle outline at current pos |
| `dspFillRect(w, h)` | Draw filled rectangle at current pos |
| `dspCircle(r)` | Draw circle outline at current pos with radius r |
| `dspFillCircle(r)` | Draw filled circle at current pos |
| `dspRoundRect(w, h, r)` | Rounded rectangle at current pos with corner radius r |
| `dspFillRoundRect(w, h, r)` | Filled rounded rectangle |
| `dspTriangle(x1, y1, x2, y2)` | Triangle from current pos to (x1,y1) and (x2,y2) |
| `dspFillTriangle(x1, y1, x2, y2)` | Filled triangle |

#### Image & Raw Commands

| Function | Description |
|----------|-------------|
| `dspPicture("file.jpg", scale)` | Draw image file from filesystem at current pos (scale: 0=original) |
| `int dspLoadImage("file.jpg")` | Load JPG into PSRAM as RGB565 pixel store, returns slot 0-3 (-1 on error). Stays in memory until VM stops. ESP32+JPEG_PICTS only |
| `int imgCreate(w, h)` | Allocate a **blank** RGB565 canvas (w×h pixels, 2 bytes/pixel) in PSRAM and return its image slot id (0-3, -1 on OOM / no free slot). The slot behaves exactly like a JPG-loaded slot for `dspPushImageRect`/`dspImageWidth`/`dspImageHeight`/`dspImgText[Burn]`, but additionally supports `imgBeginDraw()`. Max 1024×1024. Freed automatically on `TinyCStop` |
| `imgBeginDraw(slot)` | **Redirect all `dsp*` drawing primitives** (dspLine, dspFillCircle, dspText, dspRect, dspPixel, dspTriangle, …) into the canvas slot's pixel buffer instead of the physical display. Must be paired with `imgEndDraw()`. Nested begin-calls are ignored (Phase 1 = single redirect at a time). Slot must have been created with `imgCreate` (JPG-loaded slots are rejected) |
| `imgEndDraw()` | Restore the physical display as the target for `dsp*` primitives. No-op if no redirect is active |
| `imgClear(slot, color)` | Fast fill of the whole canvas with an RGB565 color. Equivalent to `imgBeginDraw(slot); dspColor(color,color); dspFillRect(...); imgEndDraw();` but much faster (memset-based). Marks the whole canvas dirty |
| `imgBlit(dst, src, sx, sy, dx, dy, w, h)` | Copy a rectangle **from one canvas to another** (or within the same canvas — memmove-safe). Source rect (sx,sy,w,h) → dest rect at (dx,dy). Both sides are clipped, so out-of-bounds args are harmless. The destination's dirty region is unioned with the touched rect automatically. Typical use: keep a clean reference canvas alongside a working canvas, then `imgBlit(work, clean, x, y, x, y, w, h)` to restore a small area before redrawing |
| `imgInvalidate(slot, x, y, w, h)` | Manually union a rect into the slot's dirty region — useful after direct buffer mutations, or to force a flush of an area you know you changed but `markDirty` didn't catch |
| `imgFlush(slot, panel_x, panel_y)` | Blit only the **dirty region** of a canvas to the panel at (panel_x+dx, panel_y+dy) and then clear the dirty rect. Draw primitives into a canvas and call `imgFlush` at the end of a frame — no manual bounding-box math needed. Must NOT be called while an `imgBeginDraw` redirect is active. No-op if dirty is empty |
| `dspPushImageRect(slot, sx, sy, dx, dy, w, h)` | Push a sub-rectangle from a loaded image (JPG or canvas) to screen. Reads from image at (sx,sy), writes to screen at (dx,dy), size w×h. Use for dirty-rect background restore (e.g., analog clock hands over a watchface, or needle over a procedurally-drawn gauge face). **Do not call while a canvas redirect is active** — it would no-op, since setAddrWindow/pushColors are stubbed on the canvas target. Unlike `imgFlush`, does NOT consult or clear the canvas's dirty region |
| `int dspImageWidth(slot)` | Get width of loaded image in slot (0 if invalid) |
| `int dspImageHeight(slot)` | Get height of loaded image in slot (0 if invalid) |
| `int dspTextWidth(len)` | Get pixel width for `len` characters in current font and text size. For transparent text on image backgrounds: measure text, draw text, later restore background with `dspPushImageRect` using the measured bounds |
| `int dspTextHeight()` | Get pixel height for current font and text size |
| `dspImgText(slot, x, y, color, fieldWidth, align, text)` | Composite text onto an image sub-rect in RAM and push the result in a single SPI transaction (flicker-free). The image buffer provides the background pixels; only foreground font pixels are overwritten. `slot`: image slot from `dspLoadImage()`. `x, y`: pixel position on the image (and screen). `color`: RGB565 text color. `fieldWidth`: total field width in characters — if larger than text length, remaining area shows image background; use 0 for auto (fits text exactly). `align`: 0=left, 1=right, 2=center (alignment within the field). `text`: the string to render. Works with EPD fonts 1-4 (set via `dspText("[f1]")`..`dspText("[f4]")`) at any text size. Example: `dspText("[f2s1]"); dspImgText(img, 10, 10, 0, 28, 0, buf);` |
| `int dspLoadImageFromCam(cam_slot)` | Decode the JPEG already held in a PSRAM **cam slot** (1-4, captured via `camControl(10, ...)`) into a free RGB565 **image slot** (0-3). Returns the new image slot, or -1 on failure. The source cam slot is untouched. ESP32+JPEG_PICTS+camera only |
| `dspImgTextBurn(slot, x, y, color, fieldWidth, align, text)` | Write glyph pixels directly **into** an image buffer — unlike `dspImgText` this does NOT touch the display. Use on headless cam boards (no TFT attached) to burn timestamps/labels into a frame before re-encoding. Falls back to built-in Font12 at size 1 if no renderer is active; if a display IS attached, honors its current font + size (`dspText("[f2s1]")` etc.). Parameters identical to `dspImgText` |
| `int dspImageToCam(img_slot, cam_slot, quality)` | Re-encode an RGB565 image slot back into a cam slot as JPEG (via esp32-camera `fmt2jpg`). `quality` range 1..63 (esp_camera convention, lower=better; 12 ≈ JPEG Q=85). Returns encoded byte size, or -1 on failure. Result is ready for `camControl(11, cam_slot, fh)` to save-to-file, email attach, or any other cam-slot consumer |
| `dspText(buf)` | Execute raw DisplayText command string (e.g., `"[z][x50][y20]Hello"`) |

#### Predefined Color Constants (RGB565)

The following color constants are **predefined** — no `#define` needed:

| Constant | Value | Constant | Value |
|----------|-------|----------|-------|
| `BLACK` | 0 | `WHITE` | 65535 |
| `RED` | 63488 | `GREEN` | 2016 |
| `BLUE` | 31 | `YELLOW` | 65504 |
| `CYAN` | 2047 | `MAGENTA` | 63519 |
| `ORANGE` | 64800 | `PURPLE` | 30735 |
| `GREY` | 33808 | `DARKGREY` | 21130 |
| `LIGHTGREY` | 50712 | `DARKGREEN` | 992 |
| `NAVY` | 16 | `MAROON` | 32768 |
| `OLIVE` | 33792 | | |

User `#define` overrides take precedence over predefined colors.

#### Example

```c
int counter;
char buf[32];

void EverySecond() {
    counter++;

    dspClear();
    dspColor(WHITE, BLACK);    // white on black

    // Title
    dspFont(2);
    dspSize(2);
    dspPos(10, 10);
    dspDraw("TinyC Display");

    // Counter
    dspFont(1);
    dspSize(1);
    sprintf(buf, "Count: %d", counter);
    dspPos(10, 60);
    dspDraw(buf);

    // Draw a red box around the counter
    dspColor(RED, BLACK);
    dspPos(5, 55);
    dspRect(150, 25);

    // Draw a blue filled circle
    dspColor(BLUE, BLACK);
    dspPos(200, 80);
    dspFillCircle(20);

    dspUpdate();  // needed for e-paper
}

int main() {
    counter = 0;
    dspClear();
    return 0;
}
```

### Touch Buttons & Sliders

Create GFX touch buttons and sliders on the display. Colors are RGB565 values (use predefined constants like WHITE, BLUE, etc.).

#### Button Creation

| Function | Description |
|----------|-------------|
| `dspButton(num, x, y, w, h, oc, fc, tc, ts, "text")` | Create power button (controls relay `num`) |
| `dspTButton(num, x, y, w, h, oc, fc, tc, ts, "text")` | Create virtual toggle button (MQTT TBT) |
| `dspPButton(num, x, y, w, h, oc, fc, tc, ts, "text")` | Create virtual push button (MQTT PBT) |
| `dspSlider(num, x, y, w, h, nelem, bg, fc, bc)` | Create slider |

Parameters: `num` = button index (0-15), `x,y` = position, `w,h` = size, `oc` = outline color, `fc` = fill color, `tc` = text color, `ts` = text size, `nelem` = slider segments, `bg` = background color, `bc` = bar color.

#### State Control & Reading

| Function | Description |
|----------|-------------|
| `dspButtonState(num, val)` | Set button state (0/1) or slider value (0-100) |
| `int touchButton(num)` | Read button state: 0/1 for buttons, -1 if undefined |
| `dspButtonDel(num)` | Delete button/slider `num`, or all if `num` is -1 |

#### Touch Callback

The `TouchButton` callback is called on touch events with the button index and value:

```c
void TouchButton(int btn, int val) {
    if (btn == 0) {
        // Toggle button pressed, val = 0 or 1
        char buf[16];
        sprintf(buf, "%d", val);
        tasmCmd("Power1", buf);
    }
    if (btn == 1) {
        // Slider moved, val = 0-100
        char buf[16];
        sprintf(buf, "%d", val);
        tasmCmd("Dimmer", buf);
    }
}

int main() {
    dspTButton(0, 10, 10, 100, 50, WHITE, BLUE, WHITE, 2, "Light");
    dspSlider(1, 10, 80, 200, 40, 10, DARKGREY, WHITE, CYAN);
    return 0;
}
```

### TinyUI — Retained-Mode Widget Layer

A thin, retained-mode UI layer on top of the primitive `dsp*` calls. It adds:

* **Screens** — keep up to 256 logical screens; switching clears the canvas, removes interactive widgets, and re-draws passive widgets tagged with the new screen.
* **Theme** — a single global colour palette + padding applied to all widgets.
* **Passive widgets** (`uiLabel`, `uiProgress`, `uiGauge`) — stored in a separate pool (`tc_ui_widgets[TC_UI_MAX_WIDGETS]`, default 16 entries). They survive `uiScreen()` switches and redraw automatically.
* **Interactive widgets** (`uiCheckbox`, `uiIcon`) — backed by the existing VButton pool (`MAX_TOUCH_BUTTONS` entries). They dispatch through the normal `TouchButton(num, state)` callback. **Different index space from passive widgets.**

TinyUI is "really tiny": ~400 LOC of C, zero extra RAM when unused, no extra dependencies, and it reuses the existing display renderer. Compare to LVGL (~150–500 KB flash, 10–30 KB RAM).

#### API

| Function | Description |
|---|---|
| `uiScreen(int id)` | Switch to screen `id` (0..255). Clears canvas with `theme.bg`, deletes all VButtons, redraws passive widgets tagged with the new screen. Call your `build_screenN()` afterwards to re-create interactive widgets. |
| `uiTheme(bg, accent, text, border)` | Set global palette (RGB565). Used by widgets created afterwards. |
| `uiClearScreen()` | Fill canvas with `theme.bg`. |
| `uiLabel(num, x, y, w, h, "text", align)` | Passive text label in widget pool slot `num` (0..15). `align`: `-1`=right, `0`=centre, `1`=left. |
| `uiLabelSet(num, "text")` or `uiLabelSet(num, buf)` | Update a label's text and redraw. Accepts a const string literal or a `char[]` buffer. |
| `uiProgress(num, x, y, w, h, value, max)` | Horizontal progress bar. Range `0..max`. |
| `uiProgressSet(num, value)` | Update bar value + redraw. |
| `uiGauge(num, x, y, r, value, vmin, vmax)` | 240° arc gauge centred at `x,y`, radius `r`. Calling again with the same `num` re-renders (needle sweeps). |
| `uiCheckbox(num, x, y, w, h, "label")` | Interactive latching toggle using VButton slot `num`, with caller-sized hit area (`w` × `h`, minimum 8×8). One `TouchButton(num, state)` per tap, `state` = new latched value (0/1). |
| `uiButton(num, x, y, w, h, "label")` | Momentary pushbutton in VButton slot `num` (same hit-area rules as `uiCheckbox`). Fires `TouchButton(num, 1)` on press and `TouchButton(num, 0)` on release — useful for trigger actions (pulse, bell, next). |
| `uiIcon(num, x, y, img_slot)` | *(reserved)* image-backed icon — wiring to the image slot subsystem is pending. |

Passive widgets (Label/Progress/Gauge) use one index space (0..15). Checkboxes / pushbuttons / icons share the VButton index space (0..MAX_TOUCH_BUTTONS-1). The two spaces do **not** collide with each other.

#### Example

```c
int current = 1;
float power = 0;

void build_screen1() {
    uiLabel(0,   0,  0, 320, 30, "Dashboard",   0);
    uiLabel(1,  10, 50, 150, 20, "Power:  0 W", 1);
    uiProgress(3, 10, 80, 300, 18, 0, 1000);
}

void main() {
    uiTheme(0x0000, 0x07FF, 0xFFFF, 0x39E7);  // bg, accent, text, border
    uiScreen(1);
    build_screen1();
}

void EverySecond() {
    power = power + 50; if (power > 1000) power = 0;
    char buf[32];
    sprintfFloat(buf, "Power: %.0f W", power);
    uiLabelSet(1, buf);
    uiProgressSet(3, power);
}
```

See `examples/tinyui_demo.tc` for a 3-screen demo with live values, an arc gauge, and interactive checkboxes.

#### Compile-time limits

| Constant | Default | Purpose |
|---|---|---|
| `TC_UI_MAX_WIDGETS` | 16 | Passive widget pool size (`tc_ui_widgets[]`) |
| `MAX_TOUCH_BUTTONS` | 16 | Interactive VButton pool (shared with `dspButton/dspTButton/…`) |

### Audio

| Function | Description |
|---|---|
| `audioVol(int vol)` | Set audio volume (0-100) |
| `audioPlay("file.mp3")` | Play MP3 file from filesystem |
| `audioSay("hello")` | Text-to-speech output |

Requires I2S audio driver configured on the device.

```c
audioVol(50);              // set volume to 50%
audioPlay("/alarm.mp3");   // play MP3 file
audioSay("sensor alert");  // speak text
```

### Persistent Variables

| Function | Description |
|---|---|
| `saveVars()` | Save all `persist` globals to the program's `.pvs` file |

Persist variables are automatically loaded on program start and saved on `TinyCStop`. Use `saveVars()` to save at critical points (e.g., after midnight counter updates).

### Watch Variables (Change Detection)

| Function | Description |
|---|---|
| `changed(var)` | Returns 1 if watch variable differs from its shadow value |
| `delta(var)` | Returns current - shadow (int or float depending on variable type) |
| `written(var)` | Returns 1 if variable was assigned since last `snapshot()` |
| `snapshot(var)` | Update shadow to current value and clear written flag |

Watch variables are compiler intrinsics — they generate inline comparison code with zero runtime overhead (no syscall).

### Deep Sleep (ESP32)

| Function | Description |
|---|---|
| `deepSleep(int seconds)` | Enter deep sleep with timer wakeup after `seconds` |
| `deepSleepGpio(int seconds, int pin, int level)` | Deep sleep with timer + GPIO wakeup (0=low, 1=high) |
| `int wakeupCause()` | Returns ESP32 wakeup cause (0=reset, 2=EXT0, 3=EXT1, 4=timer, 5=touchpad, ...) |

Persist variables and settings are saved automatically before entering deep sleep.

```c
// Wake every 5 minutes to read sensor
int cause = wakeupCause();
if (cause == 4) {
    // woke from timer — read sensor, send data
}
deepSleep(300);  // sleep 300 seconds

// Sleep until GPIO12 goes HIGH (or 1 hour max)
deepSleepGpio(3600, 12, 1);
```

### Hardware Registers (ESP32)

Direct read/write access to ESP32 memory-mapped peripheral registers. Only addresses in the peripheral range are allowed (0x3FF00000–0x3FFFFFFF or 0x60000000–0x600FFFFF).

| Function | Description |
|---|---|
| `int peekReg(int addr)` | Read 32-bit value from peripheral register |
| `pokeReg(int addr, int val)` | Write 32-bit value to peripheral register |

**Warning:** Incorrect register writes can crash or damage the device. Only use if you know what you're doing.

### Email (ESP32 — requires USE_SENDMAIL)

| Function | Description |
|---|---|
| `mailBody(body)` | Set email body text (HTML). `body` is a `char[]` array |
| `mailAttach("/path")` | Add file attachment from filesystem (string literal, up to 8) |
| `int mailSend(params)` | Send email. `params` is `char[]` with `[server:port:user:passwd:from:to:subject]`. Returns 0=ok |

For simple emails without attachments, put body text after the `]` in params:
```c
char cmd[200];
strcpy(cmd, "[smtp.gmail.com:465:user:pass:from@x.com:to@y.com:Alert] Sensor triggered!");
int result = mailSend(cmd);
```

For emails with file attachments, use `mailBody()` and `mailAttach()` before `mailSend()`:
```c
// Build body
char body[200];
sprintf(body, "<h1>Daily Report</h1><p>Temperature: %d C</p>", "%.1f");

// Register body and attachments
mailBody(body);
mailAttach("/data.csv");
mailAttach("/log.txt");

// Send — params only need [server:port:user:passwd:from:to:subject]
char params[200];
strcpy(params, "[*:*:*:*:*:to@example.com:Daily Report]");
int result = mailSend(params);
// result: 0=ok, 1=parse error, 4=memory error
```

Use `*` for server/port/user/password/from fields to use `#define` defaults from `user_config_override.h`.

### Tesla Powerwall (ESP32 — requires TESLA_POWERWALL)

Access Tesla Powerwall local API via HTTPS. Uses the email library's SSL implementation (standard Arduino SSL does not work with Powerwall).

**Requires:** `#define TESLA_POWERWALL` in `user_config_override.h` and the ESP-Mail-Client library.

| Function | Description |
|----------|-------------|
| `int pwlRequest(url)` | Config command or API request. Returns 0=ok, -1=fail |
| `pwlBind(&var, path)` | Register a global float variable for auto-fill. Path uses `#` separator (max 24 bindings) |
| `float pwlGet(path)` | Extract float from last response. Supports `[N]` suffix for nth occurrence |
| `int pwlStr(path, buf)` | Extract string from last response into `char[]` buffer. Returns length |

**Recommended approach — `pwlBind` (parse once, fill all):**

Register global variables with JSON paths in `Setup()`. When `pwlRequest()` receives a response, the JSON is parsed **once** and all matching bound variables are filled directly. No string replacements, no repeated parsing.

```c
float sip, sop, bip, hip, pwl, rper;

void Setup() {
    pwlRequest("@D192.168.188.60,email@example.com,mypassword");
    pwlRequest("@C0x000004714B006CCD,0x000004714B007969");

    // Register bindings — use original JSON key names
    pwlBind(&sip, "site#instant_power");
    pwlBind(&sop, "solar#instant_power");
    pwlBind(&bip, "battery#instant_power");
    pwlBind(&hip, "load#instant_power");
    pwlBind(&pwl, "percentage");
    pwlBind(&rper, "backup_reserve_percent");
}

void Loop() {
    // All matching bindings filled automatically:
    pwlRequest("/api/meters/aggregates");
    // sip, sop, bip, hip are now set

    pwlRequest("/api/system_status/soe");
    // pwl is now set

    pwlRequest("/api/operation");
    // rper is now set
}
```

**Configuration prefixes:**
| Prefix | Description |
|--------|-------------|
| `@Dip,email,password` | Configure IP and credentials |
| `@Ccts1,cts2` | Configure CTS serial numbers (masked in responses) |
| `@N` | Clear auth cookie (force re-authentication) |

**Common API endpoints:**
| Endpoint | Data |
|----------|------|
| `/api/meters/aggregates` | Site, battery, load, solar power (W) |
| `/api/system_status/soe` | State of energy / battery percentage |
| `/api/system_status` | System status info |
| `/api/operation` | Operation mode, reserve percentage |
| `/api/meters/readings` | Detailed meter readings per CTS |

**Nth-occurrence extraction:** `pwlGet("key[N]")` extracts the Nth occurrence of a repeated key from the JSON response. Useful for `/api/meters/readings` which has multiple CTS objects with the same key names:

```c
// Per-phase grid readings — CTS2 grid phases are occurrences 6,7,8 of "p_W"
phs1 = pwlGet("p_W[6]");
phs2 = pwlGet("p_W[7]");
phs3 = pwlGet("p_W[8]");
```

**Ad-hoc access:** `pwlGet()` and `pwlStr()` are available for one-off value extraction from the last response, but `pwlBind()` is preferred for repeated polling since it avoids re-parsing.

### Addressable LED Strip (WS2812 — requires USE_WS2812)

Control WS2812 / NeoPixel addressable LED strips directly from TinyC.

**Requires:** `#define USE_WS2812` in `user_config_override.h`.

| Function | Description |
|----------|-------------|
| `setPixels(array, len, offset)` | Set `len` pixels from `array`, starting at strip position `offset & 0x7FF`. Updates strip immediately. |

**Color format:** Each array element is `0xRRGGBB` (24-bit RGB packed into an int).

**RGBW mode:** Set bit 12 of offset (`offset | 0x1000`) for RGBW mode. In RGBW mode, two consecutive array elements encode one pixel (high word = `0x00RG`, low word = `0xBW00`).

**Example — Rainbow effect:**
```c
int leds[60];

void setup() {
    for (int i = 0; i < 60; i++) {
        int hue = (i * 256) / 60;
        leds[i] = hueToRGB(hue);
    }
    setPixels(leds, 60, 0);
}

int hueToRGB(int h) {
    int r, g, b;
    int region = h / 43;
    int remainder = (h - region * 43) * 6;
    switch (region) {
        case 0:  r = 255; g = remainder; b = 0; break;
        case 1:  r = 255 - remainder; g = 255; b = 0; break;
        case 2:  r = 0; g = 255; b = remainder; break;
        case 3:  r = 0; g = 255 - remainder; b = 255; break;
        case 4:  r = remainder; g = 0; b = 255; break;
        default: r = 255; g = 0; b = 255 - remainder; break;
    }
    return (r << 16) | (g << 8) | b;
}
```

---

### ESP Camera (ESP32)

Camera support for ESP32 boards with OV2640/OV3660/OV5640 sensors. Two modes available:

- **Tasmota webcam driver** (sel 0-7): Uses the standard `USE_WEBCAM` driver. Define `USE_WEBCAM` in `user_config_override.h`.
- **TinyC integrated camera** (sel 8-18): Direct esp_camera driver with board-specific pins, MJPEG streaming on port 81, and PSRAM slot management. Define `USE_TINYC_CAMERA` (via `-DTINYC_CAMERA` build flag). No `USE_WEBCAM` dependency.

Both modes support `mailAttachPic()` for email picture attachments (up to 4 pictures per email).

> **Build-flag gated:** the camera builtins (`cameraInit`, `camControl`,
> `dspLoadImageFromCam`, `dspImageToCam`) are only compiled in when the
> firmware was built with `-DTINYC_CAMERA` (or `USE_WEBCAM`). On firmware
> without it they don't exist — a script using them won't compile/run.
> ESP32-C3 (RISC-V) and most 4 MB builds without the flag have no camera.

#### Camera Init with Custom Pins (TinyC integrated mode)

```c
// Pin array order: pwdn, reset, xclk, sda, scl, d7..d0, vsync, href, pclk
int campins[] = {-1, -1, 15, 4, 5, 16, 17, 18, 12, 10, 8, 9, 11, 6, 7, 13};
int ok = cameraInit(campins, PIXFORMAT_JPEG, FRAMESIZE_VGA, 12, 0, 0, -1);
```

| Function | Description |
|----------|-------------|
| `cameraInit(pins[], format, framesize, quality, fb_count, grab_mode, xclk_freq)` | Init camera with pin array. Returns 0=ok, non-zero=error. `fb_count`=0 auto, `grab_mode`=0 auto, `xclk_freq`=-1 default 20MHz. |

#### Camera Control (camControl)

All camera operations use `camControl(sel, p1, p2)`:

**Tasmota webcam driver (sel 0-7, requires USE_WEBCAM):**

| sel | Function | Description |
|-----|----------|-------------|
| 0 | `camControl(0, resolution, 0)` | Init via Tasmota driver (WcSetup) |
| 1 | `camControl(1, bufnum, 0)` | Capture to Tasmota pic buffer (1-4) |
| 2 | `camControl(2, option, value)` | Set options (WcSetOptions) |
| 3 | `camControl(3, 0, 0)` | Get width |
| 4 | `camControl(4, 0, 0)` | Get height |
| 5 | `camControl(5, on_off, 0)` | Start/stop Tasmota stream server |
| 6 | `camControl(6, param, 0)` | Motion detection (-1=read motion, -2=read brightness, ms=interval) |

**TinyC integrated camera (sel 7-18, requires USE_WEBCAM or USE_TINYC_CAMERA):**

| sel | Function | Description |
|-----|----------|-------------|
| 7 | `camControl(7, bufnum, fileHandle)` | Save picture buffer to file, returns bytes written |
| 8 | `camControl(8, 0, 0)` | Get sensor PID (e.g. 0x2642 = OV2640, 0x3660 = OV3660) |
| 9 | `camControl(9, param, value)` | Set sensor parameter (see table below) |
| 10 | `camControl(10, slot, 0)` | Capture to PSRAM slot (1-4), returns JPEG size in bytes |
| 11 | `camControl(11, slot, fileHandle)` | Save PSRAM slot to file, returns bytes written |
| 12 | `camControl(12, slot, 0)` | Free PSRAM slot (0 = free all slots) |
| 13 | `camControl(13, 0, 0)` | Deinit camera + free all slots + stop stream |
| 14 | `camControl(14, slot, 0)` | Get slot size in bytes (0 if empty) |
| 15 | `camControl(15, on_off, 0)` | Start/stop MJPEG stream server on port 81 |
| 16 | `camControl(16, interval_ms, threshold)` | Enable motion detection (0=disable) |
| 17 | `camControl(17, sel, 0)` | Get motion value: 0=trigger, 1=brightness, 2=triggered, 3=interval |
| 18 | `camControl(18, 0, 0)` | Free motion reference buffer |
| 19 | `camControl(19, addr, mask)` | Read raw sensor register at `addr`, masked by `mask` |
| 20 | `camControl(20, addr, val)` | Write raw value `val` to sensor register at `addr` |

Capture (sel 10) copies the JPEG from the camera framebuffer to a PSRAM slot and immediately returns the camera framebuffer, allowing fast consecutive captures. Up to 4 slots can hold pictures simultaneously.

**Important:** Camera capture (`camControl(10, ...)`) must run in `TaskLoop()` (VM task thread). Calling from `EverySecond()` (main thread) will freeze the device.

**Stream server (sel 15):** Starts an MJPEG server on port 81 with `/stream`, `/cam.mjpeg`, and `/cam.jpg` endpoints. Automatically deferred if WiFi is not ready yet (safe for autoexec). The stream is embedded on the Tasmota main page via `FUNC_WEB_ADD_MAIN_BUTTON`.

#### Sensor Parameters (sel=9)

| param | Setting | Range |
|-------|---------|-------|
| 0 | vflip | 0/1 |
| 1 | brightness | -2..2 |
| 2 | saturation | -2..2 |
| 3 | hmirror | 0/1 |
| 4 | contrast | -2..2 |
| 5 | framesize | FRAMESIZE_* |
| 6 | quality | 10..63 |
| 7 | sharpness | -2..2 |

#### Email Picture Attachments

Pictures captured to PSRAM slots are available for email via `mailAttachPic()`. Up to 4 pictures can be attached per email:

```c
// Capture 2 pictures to slots 1 and 2
camControl(10, 1, 0);
camControl(10, 2, 0);

// Send email with both pictures attached
mailBody("Motion alarm");
mailAttachPic(1);
mailAttachPic(2);
mailSend("[*:*:*:*:*:user@example.com:Alarm]");
```

#### Capture and Save Example

```c
// Capture to PSRAM slot 1
int size = camControl(10, 1, 0);

// Save slot 1 to file
int fh = fileOpen(path, 1);    // open for write
int written = camControl(11, 1, fh);
fileClose(fh);

// Start MJPEG stream on port 81
camControl(15, 1, 0);
```

#### Timestamp / Text Overlay on JPEG Frames (cam ↔ image bridge)

Three helper syscalls bridge **cam slots** (JPEG in PSRAM, written by `camControl(10)`) and **image slots** (RGB565 in PSRAM, used by the display subsystem). Together they form a **capture → decode → overlay text → re-encode → save/stream** pipeline on any cam board, with or without a physically wired display.

| Function | Description |
|----------|-------------|
| `int dspLoadImageFromCam(cam_slot)` | Decode JPEG from cam slot into a free RGB565 image slot. Returns image slot, -1 on failure. Source cam slot untouched |
| `dspImgTextBurn(slot, x, y, color, fieldw, align, text)` | Write glyph pixels directly into the image buffer (no display push). Same parameters as `dspImgText`. Works headless |
| `int dspImageToCam(img_slot, cam_slot, quality)` | Re-encode image slot back into cam slot as JPEG. Returns bytes, -1 on failure. `quality` 1..63 (12 ≈ Q=85) |

**Build prerequisites:** `USE_WEBCAM` or `USE_TINYC_CAMERA`, plus `USE_DISPLAY` (for the font tables — the TFT does not need to be wired), on ESP32 with `JPEG_PICTS` / PSRAM.

**Example — burn timestamp into a VGA capture, save to filesystem:**
```c
#define CAM_IN   1     // cam slot holding the raw capture
#define CAM_OUT  2     // cam slot that will hold the stamped JPEG
int  counter;
char path[40];
char line[48];

void main() {
    camControl(0, 8);  // init camera, FRAMESIZE_VGA (8 = VGA)
}

void TaskLoop() {
    if (camControl(10, CAM_IN, 0) <= 0) { return; }            // capture JPEG

    int img = dspLoadImageFromCam(CAM_IN);                      // → RGB565
    if (img < 0) { return; }

    sprintf(line, "%04d-%02d-%02d %02d:%02d:%02d",
            tasm_year, tasm_month, tasm_day,
            tasm_hour, tasm_minute, tasm_second);
    dspImgTextBurn(img, 10, 10, YELLOW, 0, 0, line);            // overlay

    int jlen = dspImageToCam(img, CAM_OUT, 12);                 // re-encode
    if (jlen <= 0) { return; }

    counter = counter + 1;
    sprintf(path, "/snap_%04d.jpg", counter);
    int fh = fileOpen(path, 1);
    if (fh >= 0) {
        camControl(11, CAM_OUT, fh);                            // save to FS
        fileClose(fh);
    }

    delay(60000);   // one snapshot per minute
}
```

**Notes:**
- Must run capture + decode + re-encode in `TaskLoop()` (VM task thread). Calling from `EverySecond()` freezes the device (same rule as plain `camControl(10)`).
- Font selection follows the display stack: call `dspText("[f2s1]")` before `dspImgTextBurn` to pick a larger font. On a headless board with no display driver loaded, falls back to Font12 at size 1.
- The result in `CAM_OUT` behaves exactly like a fresh capture — you can `camControl(11)` it to a file, `mailAttachPic()` it, or route it through the stream server.

See `snap_with_timestamp.tc` for the full pipeline above.

#### Complete Camera Script

See `webcam_tinyc.tc` for a full security camera example with MJPEG streaming, motion detection, PIR alarm, email alerts, timelapse, and auto-cleanup. See `webcam.tc` for the equivalent using the Tasmota webcam driver.

---

### HomeKit (ESP32 — requires USE_HOMEKIT)

Apple HomeKit integration — expose devices directly from TinyC as HomeKit accessories. Sensors, lights, switches, and outlets become controllable via Apple Home. All HomeKit-bound variables use **native float values** — no x10 scaling needed.

**Requires:** firmware built with `-DTINYC_HOMEKIT` (which enables
`USE_HOMEKIT`). The HomeKit builtins (`hkInit`, `hkAdd`, `hkStart`,
`hkStop`, `hkReset`, `hkReady`, `hkVar`, `hkSetCode`) and the
`HomeKitWrite` callback are **only compiled in with that flag** — a
script using them on firmware built without it won't compile/run. By
policy the pre-built **4 MB ESP32 and C3 firmware ship without HomeKit**
(see `TinyC_Custom_Builds.md` → Flash Budget); use an **ESP32-S3 / 16 MB**
build for HomeKit scripts.

#### Predefined HomeKit Constants

| Constant | Value | HAP Category | Variables |
|----------|-------|--------------|-----------|
| `HK_TEMPERATURE` | 1 | Sensor (Temperature) | 1: temperature in °C |
| `HK_HUMIDITY` | 2 | Sensor (Humidity) | 1: humidity in % |
| `HK_LIGHT_SENSOR` | 3 | Sensor (Ambient Light) | 1: lux value |
| `HK_BATTERY` | 4 | Sensor (Battery) | 3: level, low-battery flag, charging state |
| `HK_CONTACT` | 5 | Sensor (Contact) | 1: open/closed |
| `HK_SWITCH` | 6 | Switch | 1: on/off |
| `HK_OUTLET` | 7 | Outlet | 1: on/off |
| `HK_LIGHT` | 8 | Light (Color) | 4: power, hue, saturation, brightness |

#### HomeKit Functions

| Function | Description |
|----------|-------------|
| `hkSetCode(code)` | Set pairing code (format: `"XXX-XX-XXX"`) |
| `hkAdd(name, type)` | Add device — name and type (e.g. `HK_TEMPERATURE`) |
| `hkVar(variable)` | Bind a float variable to the current device |
| `int hkReady(variable)` | Returns 1 if HomeKit changed this variable since last check (auto-clears) |
| `int hkStart()` | Finalize descriptor and start HomeKit. Returns 0=ok |
| `int hkInit(char descriptor[])` | Start HomeKit with a raw descriptor char array (advanced — bypasses builder pattern) |
| `hkReset()` | Erase all pairing data (factory reset). Re-pair after reboot |
| `hkStop()` | Stop HomeKit server |

#### hkReady() — Change Polling

`hkReady(var)` works like `udpReady()` — it returns 1 if Apple Home has changed this variable since the last call, and automatically clears the flag. The firmware writes the value directly into the global variable, so no manual assignment is needed. Use `hkReady()` to forward changed values via UDP:

```c
void EverySecond() {
    // global variables auto-broadcast on assignment — no explicit udpSend needed
}
```

#### HomeKitWrite Callback (Optional)

Called when Apple Home changes a value. The value is already written to the global variable before this callback runs — use it only for local side effects like relay forwarding:

```c
void HomeKitWrite(int dev, int var, float val) {
    // dev = device index (order of hkAdd calls, starting at 0)
    // var = variable index (order of hkVar calls per device, starting at 0)
    // val = new float value from Apple Home (already stored in global)
    // Only needed for side effects like tasm_power = 1
}
```

#### Builder Pattern (hkAdd + hkVar)

Devices are defined step by step. `hkAdd()` starts a device, `hkVar()` binds float variables to it. Use multiple `hkVar()` calls for devices with multiple characteristics (e.g. color light):

```c
// Color light — 4 variables: power, hue, saturation, brightness
float pwr, hue, sat, bri;

hkSetCode("111-22-333");
hkAdd("Lamp", HK_LIGHT);
hkVar(pwr); hkVar(hue); hkVar(sat); hkVar(bri);

// Simple sensor — 1 variable
float temp;
hkAdd("Temperature", HK_TEMPERATURE);
hkVar(temp);

hkStart();
```

#### Full Example — Office with Light + Sensors

```c
// HomeKit-bound variables (native float values)
float mh_pwr, mh_hue, mh_sat, mh_bri;  // color light
float elamp;     // corner light on/off
float btemp;     // temperature (e.g. 22.5)
float bhumi;     // humidity (e.g. 55.0)
int last_pwr;

// Only needed for relay forwarding — value is already in the global
void HomeKitWrite(int dev, int var, float val) {
    if (dev == 0 && var == 0) {
        int pwr;
        pwr = 0;
        if (val > 0.0) { pwr = 1; }
        if (pwr != last_pwr) { tasm_power = pwr; last_pwr = pwr; }
    }
}

void EverySecond() {
    // Receive sensor values via UDP
    if (udpReady("btemp")) { btemp = udpRecv("btemp"); }
    if (udpReady("bhumi")) { bhumi = udpRecv("bhumi"); }

    // global variables auto-broadcast on assignment — no explicit udpSend needed
}

int main() {
    mh_pwr = 0.0; mh_hue = 0.0; mh_sat = 0.0; mh_bri = 50.0;
    elamp = 0.0; btemp = 22.0; bhumi = 50.0;
    last_pwr = -1;

    hkSetCode("111-11-111");
    hkAdd("Light", HK_LIGHT);
    hkVar(mh_pwr); hkVar(mh_hue); hkVar(mh_sat); hkVar(mh_bri);
    hkAdd("Corner Light", HK_OUTLET);       hkVar(elamp);
    hkAdd("Temperature", HK_TEMPERATURE);    hkVar(btemp);
    hkAdd("Humidity", HK_HUMIDITY);           hkVar(bhumi);
    hkStart();
    return 0;
}
```

#### Pairing

1. Compile and flash firmware with `USE_HOMEKIT`
2. Compile and upload TinyC program using `hkSetCode()` / `hkAdd()` / `hkStart()`
3. Scan QR code at `http://<device>/hk` with iPhone
4. After configuration changes, run `hkReset()` once, then re-pair

#### Predefined File Constants

Shorthand constants for `fileOpen()`:

| Constant | Value | Description |
|----------|-------|-------------|
| `r` | 0 | Read |
| `w` | 1 | Write |
| `a` | 2 | Append |

```c
int f = fileOpen("/data.csv", r);   // instead of fileOpen("/data.csv", 0)
f = fileOpen("/log.txt", a);         // instead of fileOpen("/log.txt", 2)
```

### Plugin Query (Binary Plugins)

Query loaded binary plugins (PIC modules) for data.

| Function | Description |
|---|---|
| `int pluginQuery(char dst[], int index, int p1, int p2)` | Call plugin at `index` with parameters `p1`, `p2`. Result string copied to `dst`. Returns string length |

### Cross-VM Share Table (ESP32)

A driver-global named key/value store, mutex-protected, that lets two or more TinyC slots share scalars and short strings. Use it when one program outgrows a single slot (`TC_MAX_PROGRAM = 128 KB`) and is split across slots, or when multiple cooperating programs need to exchange state without going through MQTT or the filesystem.

Capacity (override via `user_config_override.h`): **`TC_SHARE_MAX = 32`** entries · **`TC_SHARE_KEY_LEN = 16`** char key · **`TC_SHARE_STR_LEN = 64`** char value. Worst-case footprint ≈ 2.6 KB DRAM. Mutex is created lazily on first use.

| Function | Description |
|---|---|
| `void shareSetInt(char key[], int v)`     | Set integer value for `key` (creates entry if missing, overwrites type) |
| `void shareSetFloat(char key[], float v)` | Set float value for `key` |
| `void shareSetStr(char key[], char v[])`  | Set string value for `key` (truncated to `TC_SHARE_STR_LEN`) |
| `int shareGetInt(char key[])`             | Read integer; **0** if key missing or wrong type |
| `float shareGetFloat(char key[])`         | Read float; **0.0** if missing |
| `int shareGetStr(char key[], char dst[])` | Read string into `dst`; returns chars copied, **0** + empty `dst` if missing |
| `int shareHas(char key[])`                | **1** if key exists, **0** if not |
| `int shareDelete(char key[])`             | Delete entry; returns **1** if it existed, **0** otherwise |

**Key constraint:** every `key` argument must be a **string literal** (resolved to a constant-pool index at compile time). Variable keys are not supported. Keys are case-sensitive.

**Missing-key semantics:** reads never raise an error. Use `shareHas()` to distinguish "key absent" from "key exists with value 0". Re-`shareSet*` with a different type silently rewrites the entry.

**Example — slot 0 writer + slot 1 reader:**
```c
// slot 0 (writer)
int counter = 0;
void EverySecond() {
    counter = counter + 1;
    shareSetInt("counter", counter);
    shareSetFloat("kwh", counter * 0.1);
    char nm[32];
    sprintf(nm, "tick=%d", counter);
    shareSetStr("name", nm);
}
int main() { return 0; }
```
```c
// slot 1 (reader)
void Command(char cmd[]) {
    if (strcmp(cmd, "ALL") == 0) {
        int   c = shareGetInt("counter");
        float f = shareGetFloat("kwh");
        char  n[32];
        shareGetStr("name", n);
        char r[160];
        sprintf(r, "counter=%d kwh=%.1f name=%s", c, f, n);
        responseCmnd(r);
    } else {
        responseCmnd("RDR: ALL");
    }
}
int main() { addCommand("RDR"); return 0; }
```

#### shareDump() *(since 1.6.2)*

Diagnostic-only — walk the entire `tc_share_table[]` under the share
mutex and log every live entry via `AddLog`. Returns the number of
live entries to the calling VM. Pure read-only, non-allocating.

```c
int n = shareDump();
// → Tasmota log:
//   TCC: share[0] key="brutto"    type=FLT value=25.290000
//   TCC: share[3] key="price"     type=FLT value=12.605000
//   TCC: share[5] key="soc"       type=INT value=87
//   TCC: share[12] key="disp_html" type=STR value="<table>..."
//   TCC: shareDump: 8/32 live entries
```

Useful for diagnosing cross-VM share anomalies: did the write actually
land? at what index? with what type and value? Doc-side this avoids
having to patch the firmware with strategically-placed debug logs.

#### dumpPersist() *(since 1.6.9)*

The persist-layer counterpart of `shareDump()`. Logs every `persist`
entry — its global index, slot count, and the raw int32 words (chunked
16/line so long arrays are never silently truncated) — plus a header
with the `.pvs` filename and the FNV-1a layout hash. Returns the number
of persist entries.

```c
int n = dumpPersist();
// → Tasmota log:
//   TCC: persistDump file="/bat_ctrl.pvs" hash=0x9F3A21C7 entries=12
//   TCC: persist p[0] i=4 n=1 @0:42
//   TCC: persist p[1] i=6 n=40 @0:1078530011,1067030938,...   (16/line)
//   TCC: persist p[1] i=6 n=40 @16:...
```

**Primary use — back up before a layout-change flash.** Adding/removing/
reordering ANY `persist` variable invalidates the whole `.pvs` and resets
**all** persist values to defaults on next boot (not just the changed
ones). Call `dumpPersist()` from a `Command()` handler, copy the log
lines, and you have a bit-exact snapshot to restore from afterwards.
Raw int32 words are emitted (not float-formatted) precisely so the
restore is bit-exact regardless of whether a slot holds an int or a
float — persist stores no per-slot type. One-shot diagnostic; heavy on
serial like `shareDump()`.

### Symmetric Crypto (ESP32)

AES-128 (ECB + CBC), HMAC-SHA256, SHA-256, plus hex⇄binary helpers — backed by mbedtls (already linked for HTTPS / MQTT-TLS, so no extra flash cost). All operations work in-place on TinyC `char[]` buffers. ESP8266 stubs return `0` / no-op.

Motivating use case: TinyC scripts speaking the **Tuya local protocol** (v3.3 = AES-128-ECB + CRC32) so users can drive Smart-Life-controlled devices (pool heat pumps, plugs, switches, dehumidifiers) directly from Tasmota without a cloud round-trip — see `examples/pool_pump.tc`. Also useful for signed REST APIs (HMAC-SHA256), encrypted SML decoders, and per-device MQTT-TLS fingerprinting.

| Function | Description |
|---|---|
| `int aesEcb(char key[], char data[], int enc_flag)` | AES-128-ECB on **one 16-byte block** in-place. `key` must be exactly 16 bytes. `enc_flag`: 1=encrypt, 0=decrypt. Returns 1=ok, 0=err. For multi-block buffers, call in a `for` loop over 16-byte chunks |
| `int aesCbc(char key[], char iv[], char data[], int len, int enc_flag)` | AES-128-CBC in-place. `key` and `iv` are 16 bytes each. `len` must be a multiple of 16. Stack-allocates up to 4 KB; falls back to malloc above. Returns 1=ok, 0=err |
| `int hmacSha256(char key[], int klen, char data[], int dlen, char out[])` | HMAC-SHA256. `key` ≤ 1024 B, `data` ≤ 4 KB, `out` must be ≥ 32 B. Returns 1=ok |
| `int sha256(char data[], int dlen, char out[])` | SHA-256 of `data[0..dlen-1]` into `out[0..31]`. Returns 1=ok |
| `int hex2bin(char hex[], int hex_len, char out[])` | Decode hex string → bytes. Returns bytes written (= `hex_len / 2`). Tolerates odd hex_len by truncating the trailing nibble |
| `int bin2hex(char bin[], int bin_len, char out[])` | Encode bytes → lowercase hex string. Writes `bin_len * 2` chars + NUL terminator. Returns chars written (excluding NUL) |

**Buffer convention:** TinyC `char[]` is one byte per int32 slot — only the low 8 bits are used. Lengths are in bytes and must fit the ref's allocated capacity.

**Limits:** AES-CBC stack-allocates up to 4 KB per call. HMAC/SHA bounded at 1024 B key / 4 KB data per call — bigger payloads need to be hashed in chunks (future enhancement may expose hash-state).

**Not yet exposed:** AES-GCM, ECDH (Tuya v3.4 needs both — most Smart-Life devices are still v3.3 so this is rarely a blocker).

```c
// Example — encrypt a JSON command for a Tuya v3.3 device
char key[16];                            // 16-byte AES key
char body[64];                           // PKCS#7-padded plaintext (JSON command)
strcpy(key, "u9eUO{aw1Kxc}uk^");
int n = strlen(body);
// pad to 16
int pad = 16 - (n & 15);
for (int i = 0; i < pad; i = i + 1) body[n + i] = pad;
n = n + pad;
// encrypt block-by-block (ECB)
for (int b = 0; b < n; b = b + 16) {
    aesEcb(key, body + b, 1);            // 1 = encrypt; in-place
}

// Example — verify an HMAC-SHA256 signature
char key[32];
char data[256];
char expected_sig[32];                   // signature received over the wire
char actual_sig[32];
hmacSha256(key, 32, data, strlen(data), actual_sig);
int ok = 1;
for (int i = 0; i < 32; i = i + 1) {
    if (actual_sig[i] != expected_sig[i]) { ok = 0; break; }
}
```

### Debug

| Function      | Description                |
|---------------|----------------------------|
| `dumpVM()`    | Dump VM state to console   |

---

## Multi-VM Slots (ESP32)

On ESP32, up to **6 independent TinyC programs** can run simultaneously in separate VM slots. Each slot has its own bytecode, globals, stack, heap, and output buffer. Memory is allocated dynamically — empty slots cost zero bytes, and non-autoexec slots use lazy loading (only ~33 bytes until first run). ESP8266 supports only 1 slot.

### Slot Configuration

Slot assignments and autoexec flags are stored in `/tinyc.cfg` on the filesystem. This file is created and updated automatically whenever a program is loaded, uploaded, or the autoexec flag is toggled. There is no need to edit it manually.

Example `/tinyc.cfg`:
```
/weather.tcb,1
/display.tcb,1
/logger.tcb,0
,0
,0
/mp3player.tcb,1
_info,0
```

Each line corresponds to a slot (0–5): `filename,autoexec_flag`. The last line `_info,<0|1>` controls whether debug status rows are shown on the Tasmota main web page.

### Tasmota Commands

All commands default to slot 0 if no slot number is given (backward-compatible).

| Command                       | Description                                      |
|-------------------------------|--------------------------------------------------|
| `TinyC`                       | Show status for all slots (JSON)                 |
| `TinyCRun [slot] [/file.tcb]` | Run slot (optionally load file first)            |
| `TinyCStop [slot]`            | Stop slot                                        |
| `TinyCReset [slot]`           | Stop and reset slot                              |
| `TinyCExec <n>`               | Set instructions per tick (default 1000)         |
| `TinyCInfo 0\|1`              | Show/hide VM debug rows on main web page         |
| `TinyC ?<query>`              | Query global variables by index (see below)      |
| `TinyCChkpt`                  | Show partition table (ESP32 only)                |
| `TinyCChkpt p`                | Pack: shrink `app0` to fit, expand `spiffs`      |
| `TinyCChkpt p <KB>`           | Pack with explicit `app0` size in KB (1024..3904)|

**Examples:**
```
TinyCRun                    → run slot 0
TinyCRun /weather.tcb       → load file into slot 0 and run
TinyCRun 2 /logger.tcb      → load file into slot 2 and run
TinyCStop 1                 → stop slot 1
TinyCReset 3                → reset slot 3
TinyCInfo 1                 → show debug info on main page

TinyCChkpt                  → list all partitions with offsets, sizes, labels
TinyCChkpt p                → auto-pack: app0 = current sketch size + 192 KB
                              headroom (rounded to 64 KB), all remaining
                              flash up to the start of `custom` (or end of
                              chip) becomes `spiffs`
TinyCChkpt p 2880           → set app0 to exactly 2880 KB; spiffs = rest
```

**`TinyCChkpt p` — partition repack**

A safe in-firmware way to reclaim flash for the filesystem on devices
that were initially provisioned with a larger `app0` than the current
firmware needs. Common use case: a 16 MB device flashed with a generic
`app0=3008 KB` partition layout, where the actual sketch is only 1.5 MB
— the rest of the app slot is wasted, while spiffs sits at a small
default size. Packing returns the unused app space to spiffs.

**Mechanics:**

1. Reads the current partition table from `0x8000`.
2. Locates `app0`, `spiffs`, and (if present) `custom`. Verifies a
   `safeboot` partition exists — if not, the pack is **refused**: no
   safeboot means no recovery if the new app slot turns out to be too
   small for a future build, so the operation is unsafe.
3. Computes new sizes:
   - `app0` → requested KB (or auto: sketch + ~192 KB, 64 KB-aligned).
   - `spiffs` → start = end of new app0; end = start of `custom`
     partition (if any) or end of flash chip.
4. Updates the partition table entries, recomputes the trailer MD5,
   writes the 4 KB sector at `0x8000` back atomically.
5. **Formats LittleFS** — the spiffs partition's offset and size both
   change, so the existing FS layout is no longer valid. Anything you
   want to keep MUST be backed up first.

**Safety:**

- Refused without a `safeboot` partition (no recovery → no go).
- Refused if the new app size is smaller than the current sketch
  (would brick on next OTA).
- Refused if there's no room for spiffs (custom partition or chip
  end is closer than new spiffs would need).
- The `custom` partition (if present) is preserved in place; spiffs
  expands up to its boundary, not over it.

**Recommended workflow:**

```
TinyCChkpt                  # see current layout — note app0 size and FS size
# → in WebUI's File Manager, back up Settings.json and any .tcb / .pvs
#   you don't want to lose
TinyCChkpt p                # auto-pack
# → device reboots, FS is empty; re-upload your scripts and Settings
```

Don't run `TinyCChkpt p` if you're not prepared to lose the filesystem
contents.

### Web Console (`/tc`)

The TinyC console page at `/tc` shows a compact overview of all slots:

- **Status indicator**: green dot = active (running or callback-ready), orange = loaded but not running, grey = empty
- **Run / Stop buttons**: context-aware — Run is greyed out when active, Stop is greyed out when idle
- **A button**: toggles auto-execute on boot (green = enabled). Saved to `/tinyc.cfg` immediately
- **Load Program**: file selector with slot dropdown to load any `.tcb` file into any slot
- **Repository**: if `/tinyc_repo.cfg` exists on the filesystem, a remote program repository is shown (see below)
- **Upload Program**: file upload with slot dropdown to upload and load a `.tcb` file directly

### Program Repository

TinyC supports downloading pre-compiled `.tcb` programs from a remote repository directly on the device.

**Setup:**
1. Create a file `/tinyc_repo.cfg` on the device filesystem containing the base URL of the repository (one line):
   ```
   https://raw.githubusercontent.com/gemu2015/Sonoff-Tasmota/universal/tasmota/tinyc/bytecode
   ```
2. The repository must contain an `index.txt` file listing available `.tcb` files (one filename per line):
   ```
   blink.tcb
   bme280.tcb
   lcd_i2c.tcb
   onewire.tcb
   ```
3. The `.tcb` files must be accessible at `<base_url>/<filename>`

**Usage:**
When `/tinyc_repo.cfg` is present, the TinyC console page shows an additional **Repository** fieldset with:
- A dropdown listing all `.tcb` files from the remote `index.txt`
- A slot selector
- A **Download & Load** button that downloads the selected file to the device filesystem and loads it into the chosen slot

The default repository at `https://raw.githubusercontent.com/gemu2015/Sonoff-Tasmota/universal/tasmota/tinyc/bytecode` contains example programs for sensors, displays, charts, and more. Upload the provided `tinyc_repo.cfg` to your device to enable it.

### API Endpoints

The JSON API at `/tc_api` supports a `slot` parameter:

```
GET /tc_api?cmd=run&slot=2     → run slot 2
GET /tc_api?cmd=stop&slot=1    → stop slot 1
GET /tc_api?cmd=status         → status of all slots
POST /tc_upload?slot=3&api=1   → upload .tcb to slot 3 (JSON response)
```

### Variable Query — `_Q()` Macro (Google Charts)

TinyC global variables can be queried via HTTP as JSON, enabling live dashboards with Google Charts or any JavaScript charting library.

The `_Q()` macro is expanded at **compile time** inside string literals. The compiler resolves variable names to their index and type, so the binary contains no variable names — only compact index-based queries.

**Syntax:** `_Q(var1, var2, ...)`

The compiler replaces `_Q(...)` with an index-encoded query string:
- `<index>i` — int scalar
- `<index>f` — float scalar
- `<index>s<n>` — char[n] string
- `<index>I<n>` — int array of n elements
- `<index>F<n>` — float array of n elements

**Example:** Given globals `float temperature; int counter;`, the string:
```c
"TinyC+%3F_Q(temperature,counter)"
```
expands at compile time to:
```
"TinyC+%3F0f;1i"
```

**Response format:** JSON array in the order requested:
```json
{"TinyC":[23.5,42]}
```

**Usage in WebPage callback:**
```c
float temperature = 23.5;
int counter = 0;

void WebPage() {
    webSend("<script>fetch('/cm?cmnd=TinyC+%3F_Q(temperature,counter)')");
    webSend(".then(r=>r.json()).then(d=>{var v=d.TinyC;");
    webSend("// v[0]=temperature, v[1]=counter");
    webSend("});</script>");
}
```

For a specific slot, prefix with the slot number:
```
TinyC ?2 0f;1i      → query slot 2
```

### Boot Sequence

On boot, TinyC reads `/tinyc.cfg` and:
1. Loads each configured `.tcb` file into its slot
2. Auto-runs slots that have the autoexec flag set (`1`)

If no `/tinyc.cfg` exists (first boot), no programs are loaded.

### Resource Usage

Each VM slot uses approximately **3.2 KB RAM** (struct only, without program bytecode). Slots are allocated dynamically — only active slots consume memory. The slot pointer array itself is just 24 bytes. Non-autoexec slots use lazy loading: only the filename (~33 bytes) is stored until the slot is first run.

| Resource              | Cost                         |
|-----------------------|------------------------------|
| Pointer array         | 24 bytes (6 pointers)        |
| Per-slot struct       | ~3.2 KB                      |
| Program bytecode      | variable (malloc'd)          |
| Heap (large arrays)   | 32 KB max, allocated on demand |
| Autoexec stagger      | 100 ms delay between starts  |

### Callbacks with Multiple Slots

Each slot receives its own callbacks independently:

- `EverySecond()`, `Every100ms()`, `Every50ms()` — dispatched to all active slots
- `WebCall()` — each slot can add its own sensor rows to the main page
- `JsonCall()` — each slot appends its own telemetry data
- `TaskLoop()` — runs in slot's own FreeRTOS task (ESP32)
- `CleanUp()` — called on all slots before device restart

Shared resources (UDP, SPI, file handles) are global — only one slot should use each at a time.

### Example: Two Programs Side by Side

Slot 0 — Temperature monitor:
```c
int temp = 0;
void EverySecond() { temp = tasm_analog0; }
void WebCall() {
    char buf[64];
    sprintf(buf, "{s}Temperature{m}%d{e}", temp);
    webSend(buf);
}
int main() { return 0; }
```

Slot 1 — Uptime counter:
```c
int uptime = 0;
void EverySecond() { uptime++; }
void WebCall() {
    char buf[64];
    sprintf(buf, "{s}Uptime{m}%d s{e}", uptime);
    webSend(buf);
}
int main() { return 0; }
```

Both display their sensor rows on the Tasmota main page simultaneously.

---

## VM Limits

| Resource          | ESP8266  | ESP32    | Browser  | Notes                              |
|-------------------|----------|----------|----------|------------------------------------|
| Stack depth       | 64       | 256      | 256      | Operand stack entries              |
| Call frames       | 8        | 32       | 32       | Maximum recursion / call depth     |
| Locals per frame  | 256      | 256      | 256      | Scalars + small arrays ≤16 inline  |
| Global variables  | 64       | 256      | 256      | Scalars + small arrays ≤16 inline  |
| Code size         | 4 KB     | 128 KB   | 64 KB    | Bytecode; ESP32 spills to PSRAM on DRAM OOM |
| Heap memory       | 8 KB     | 32 KB    | 64 KB    | For arrays >16 elements (auto alloc)   |
| Heap handles      | 8        | 32       | 32       | Max simultaneous heap allocations  |
| Constant pool     | 32       | 1024     | 65536    | String & float constants (DRAM, spills to PSRAM on ESP32) |
| Instruction limit | 1M       | 1M       | 1M       | Safety limit per execution         |
| GPIO pins         | 40       | 40       | 40       | Pins 0–39 (simulated in browser)   |
| File handles      | 4        | 4        | 8        | Simultaneously open files          |
| VM slots          | 1        | 6        | 1        | Simultaneous programs              |
| Cross-VM share    | n/a      | 32 keys  | n/a      | Driver-global shared scalar/string table (ESP32 only) |

**ESP32 PSRAM fallback (since v1.3.19):** `TC_MAX_PROGRAM` raised 64 KB → 128 KB. The bytecode buffer (`s->program`) and the constant data pool (`vm->const_data`) are allocated from internal DRAM first; on OOM they automatically spill to `heap_caps_malloc(MALLOC_CAP_SPIRAM)`. Small/normal scripts stay in fast static RAM; only edge-case 100+ KB programs spill to PSRAM. An `AddLog` INFO line is emitted when the PSRAM path is taken.

---

## Device File Management (IDE)

### IDE Installation

The IDE file (`tinyc_ide.html.gz`) can reside on either the **flash filesystem** or the **SD card** — whichever is mounted as the user filesystem (`ufsp`). Upload `tinyc_ide.html.gz` via the Tasmota **Manage File System** page.

> **Note:** TinyC scripts and data files (`.tc`, `.tcb`, etc.) are also stored on the user filesystem (`ufsp`).

### File Operations

The IDE toolbar includes controls for managing files on the Tasmota device filesystem:

- **Device Files dropdown** — Lists all files on the device. Select a file to load it into the editor. The list shows filename and size (e.g. `config.tc (1.2KB)`).
- **Save File button** — Saves the current editor content as a file on the device. Prompts for a filename (defaults to the current filename).
- **Auto-refresh** — The file list refreshes automatically when the device IP is entered or changed, and after each save.

All file operations use the `/tc_api` endpoint with CORS support, so the IDE can be used from any browser — it doesn't need to be served from the device.

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/tc_api?cmd=listfiles` | GET | Returns JSON list of files: `{"ok":true,"files":[{"name":"x","size":123},...]}` |
| `/tc_api?cmd=readfile&path=/name` | GET | Returns file content as plain text |
| `/tc_api?cmd=readfile&path=/name@from_to` | GET | Returns time-filtered CSV data (see below) |
| `/tc_api?cmd=writefile&path=/name` | POST | Writes POST body to file, returns `{"ok":true,"size":N}` |
| `/tc_api?cmd=deletefile&path=/name` | GET | Deletes a file from the filesystem |

### Time-Range Filtered File Access

Append `@from_to` to the file path to extract only rows within a timestamp range from a CSV data file. This is useful for serving IoT time-series data to chart libraries.

**URL format:**
```
/tc_api?cmd=readfile&path=/data.csv@DD.MM.YY-HH:MM_DD.MM.YY-HH:MM
```

**Example:**
```
/tc_api?cmd=readfile&path=/sml.csv@1.1.24-00:00_31.1.24-23:59
```

Both German (`DD.MM.YY HH:MM`) and ISO (`YYYY-MM-DDTHH:MM:SS`) timestamp formats are supported. The `_` (underscore) separates the from and to timestamps.

**Response:** The header line (first line) is always included, followed by only those data lines whose first-column timestamp falls within `[from..to]`. Lines past the end timestamp are skipped efficiently (early break).

**Performance optimization:** If an index file exists (same name with `.ind` extension, containing `timestamp\tbyte_offset` lines), byte offsets are used to seek directly to the start position. Otherwise, an estimated seek is performed based on the file's first and last timestamps (similar to Scripter's `opt_fext`).

### Port 82 Download Server (ESP32)

For large database files, the time-filtered readfile on port 80 can block the main web server loop. TinyC includes a dedicated **port 82 download server** that serves files in a FreeRTOS background task, keeping the device responsive during large transfers.

**URL format:**
```
http://<ip>:82/ufs/<filename>
http://<ip>:82/ufs/<filename>@from_to
```

**Examples:**
```
http://192.168.1.100:82/ufs/sml.csv
http://192.168.1.100:82/ufs/sml.csv@1.1.24-00:00_31.1.24-23:59
```

**Features:**
- Runs in a dedicated FreeRTOS task (pinned to core 1, priority 3)
- Does not block the main Tasmota loop or web interface
- Supports the same `@from_to` time-range filtering as the `/tc_api` readfile
- Uses chunked transfer encoding for filtered responses
- Content-Disposition header for browser download
- One download at a time (returns HTTP 503 if busy)
- Automatic MIME type detection (`.csv`/`.txt` = text/plain, `.html`, `.json`)
- The port can be changed by defining `TC_DLPORT` before compilation (default: 82)

### Typical Workflow

1. Enter device IP in the toolbar
2. The **Device Files** dropdown auto-populates with all files on the device
3. Select a file to load it into the editor — or write new code
4. Click **Save File** to store the source on the device (e.g. as `myapp.tc`)
5. Click **Run on Device** to compile, upload the `.tcb` binary, and start execution

This lets you keep TinyC source files on the device alongside their compiled bytecode, making it easy to edit programs directly without needing local file storage.

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

The IDE includes 19 ready-to-use examples in the "Load Example..." dropdown — from basic blink to weather station receivers and interactive WebUI dashboards.

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
#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x05
#define INPUT_PULLDOWN 0x09

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
    sprintf(line, "count = %d", 42);
    printString(line);      // count = 42

    // Multi-value with sprintfAppend
    char report[128];
    sprintf(report, "Sensor %d", 1);
    sprintfAppend(report, " name=%s", name);
    sprintfAppend(report, " temp=%.1f", 23.5);
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

### WebUI Dashboard
```c
int power;
int brightness;
int mode;

void WebUI() {
    int page = webPage();
    if (page == 0) {
        webToggle(power, "Power");
        webSlider(brightness, 0, 100, "Brightness");
    }
    if (page == 1) {
        webPulldown(mode, "Mode", "Off|Auto|Manual");
    }
}

int main() {
    webPageLabel(0, "Controls");
    webPageLabel(1, "Settings");
    brightness = 50;
    return 0;
}
```

---

## Differences from Standard C

| Feature                  | Standard C     | TinyC                        |
|--------------------------|----------------|------------------------------|
| Pointers (data)          | Full support   | **Not supported** (no `int *p`, no `&x` for scalars, no pointer arithmetic). Strings use `char arr[N]` instead of `char*`; arrays decay to pass-by-reference into function parameters; `int& a` reference parameters (since 1.4.3) cover the common multi-out / mutate-caller case |
| Function pointers        | Full support   | **Supported since 1.4.1** — typedef-based: `typedef int (*cmp_fn)(int,int); cmp_fn fn = my_function; fn(a, b);`. As locals, globals, parameters, **and struct fields** (1.4.2). Out: inline `void (*p)(int)` decl without typedef, fn-ptr `==/!=`, returning fn-ptrs |
| Structs                  | Full support   | Supported: scalar fields, member access (including nested struct fields with correct offset arithmetic), initializer lists, whole-struct assignment, struct as parameter / return, `sizeof(Tag)`, function-pointer fields. Out: self-referential structs (`struct Node { Node next; }` needs pointers), unions, bit-fields, designated initializers, struct equality `a == b` |
| Reference parameters     | C++ feature    | **Supported since 1.4.3** — `void swap(int& a, int& b)` for scalar pass-by-reference (int, float, char). Multi-out, in-place compound (`n += 5` on a ref param), globals as ref args. Caller arg must be a plain identifier of a local or global; array elements / struct fields / heap arrays yield a clear compile error |
| Enums                    | Full support   | Supported: named/anonymous, negative values, auto-increment, inline in functions |
| Dynamic memory           | malloc/free    | Auto heap for arrays >16 elements (no explicit malloc) |
| Multi-dimensional arrays | Full support   | **2D supported since 1.3.38** — `char buf[N][M]`, `int grid[R][C]`, `float coef[R][C]`. Element access `arr[i][j]`, row passing `func(arr[i])` to 1D array params, `strcpy/strcat/strcmp` on rows, `sprintf("%s", arr[i])` for 2D char. 3D+ **not supported**. 2D literal initialisers (`int m[2][3] = {{1,2,3},{4,5,6}}`) not accepted yet — initialise in `main()` instead |
| String type              | `char*`        | `char arr[N]` only — no pointer arithmetic. `char name[] = "literal"` size-inferred (since 2026-03). String ops (replace/starts/ends/contains/upper/lower/trim) since 1.5.0 — see [String Operations](#string-operations) |
| Preprocessor             | Full CPP       | `#define` (constants + function-like macros), `#ifdef`/`#ifndef`/`#if`/`#else`/`#endif`/`#undef` (no `#include`) |
| Header files             | `#include`     | **Not supported**            |
| typedef                  | Full support   | Supported: primitive aliases, named struct aliases, anonymous struct typedef, chained aliases, local typedef, function-pointer typedefs (1.4.1+) |
| `const`                  | Type enforced  | Accepted (documentation hint, not enforced at runtime) |
| `static` locals          | Full support   | Supported: zero-initialised, persists across calls. Non-zero initialisers not emitted |
| sizeof                   | Full support   | Compile-time only: `sizeof(type)` and `sizeof(name)` supported; `sizeof(expr)` not supported. See [sizeof Operator](#sizeof-operator) |
| Ternary operator `?:`    | Full support   | Supported, including nested ternary |
| do-while                 | Full support   | Supported                    |
| Compound assignments     | Full support   | Supported: `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` |
| Hex escape `\xNN`        | Full support   | Supported in string and char literals |
| goto                     | Full support   | **Not supported**            |
| Variadic user functions  | `va_list` etc. | **Not supported** (only `sprintf`/`sprintfAppend` accept multiple args via compile-time expansion) |
| Standard library         | stdio, stdlib  | Built-in functions only (see [Built-in Functions](#built-in-functions)) |

---

*Generated from TinyC source — lexer.js, parser.js, codegen.js, opcodes.js, vm.js*
