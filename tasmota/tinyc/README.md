# TinyC — C Scripting for Tasmota

TinyC is a C-subset compiler and VM that runs on ESP32/ESP8266 as Tasmota driver `XDRV_124`. Write C code in the browser IDE, compile to bytecode, upload and run — no firmware rebuild needed.

## Quick Start

1. Upload `tinyc_ide.html.gz` to the device filesystem
2. Open `http://<device-ip>/tinyc_ide.html` in a browser
3. Write code, press **Ctrl+Enter** to compile, **Ctrl+Shift+Enter** to run

## Language

Standard C subset: `int`, `float`, `char`, `void`, `bool` types. Control flow with `if/else`, `while`, `for`, `switch/case`, `break`, `continue`. Functions, arrays (stack up to 255, heap for larger), `#define` preprocessor, `// line` and `/* block */` comments. No pointers, no structs.

## Tasmota Integration

Callbacks run automatically from Tasmota's main loop:

| Callback | When | Use |
|---|---|---|
| `EverySecond()` | Every 1s | Sensor polling, status updates |
| `Every50ms()` | Every 50ms | Fast I/O, radio polling |
| `Every100ms()` | Every 100ms | Medium-speed tasks |
| `JsonCall()` | MQTT telemetry | Append JSON via `responseAppend()` |
| `WebCall()` | Web page refresh | Add sensor rows via `webSend()` |
| `WebPage()` | Page load (once) | Charts, custom HTML |
| `UdpCall()` | UDP packet received | Inter-device communication |

`main()` runs once at startup. Return 0 to keep callbacks active, non-zero to stop.

## Built-in Functions

**GPIO:** `pinMode`, `digitalWrite`, `digitalRead`, `analogRead`, `analogWrite`, `gpioInit`
**Timing:** `delay`, `delayMicroseconds`, `millis`, `micros`
**Serial:** `serialBegin`, `serialPrint`, `serialPrintInt`, `serialPrintFloat`, `serialPrintln`, `serialRead`, `serialAvailable`
**Math:** `abs`, `min`, `max`, `map`, `random`, `sqrt`, `sin`, `cos`
**Strings:** `strlen`, `strcpy`, `strcat`, `strcmp`, `printString`, `printStr`
**Format:** `sprintfInt`, `sprintfFloat`, `sprintfStr`, `sprintfAppendInt`, `sprintfAppendFloat`, `sprintfAppendStr`
**I2C:** `i2cRead8`, `i2cWrite8`, `i2cRead`, `i2cWrite`, `i2cExists`, `i2cRead0`, `i2cWrite0`
**SPI:** `spiInit`, `spiSetCS`, `spiTransfer`
**Files:** `fileOpen`, `fileClose`, `fileRead`, `fileWrite`, `fileExists`, `fileDelete`, `fileSize`
**Tasmota:** `tasmCmd`, `responseAppend`, `webSend`, `webFlush`
**UDP:** `udpSend`, `udpRecv`, `udpReady`, `udpSendArray`, `udpRecvArray`
**Heap:** `malloc`, `free`
**Debug:** `print`, `dumpVM`

## Tasmota Commands

| Command | Description |
|---|---|
| `TcCompile <code>` | Compile and run inline code |
| `TcRun` | Run loaded bytecode |
| `TcStop` | Stop running program |
| `TcLoad <file>` | Load .tcb from filesystem |

REST API: `http://<ip>/tc_api?cmd=run`, `cmd=stop`, `cmd=status`

## VM Limits

| Resource | ESP8266 | ESP32 |
|---|---|---|
| Stack depth | 64 | 256 |
| Call frames | 8 | 32 |
| Globals | 64 | 256 |
| Code size | 4 KB | 16 KB |
| Heap | 8 KB | 32 KB |

## Examples

See [`examples/`](examples/) for complete working programs:

- **blink** — LED blink
- **callbacks** — Tasmota MQTT + web integration
- **sht31** — I2C temperature/humidity sensor
- **max31855** — SPI thermocouple reader
- **bresser** — CC1101 868 MHz weather station receiver (5/6/7-in-1 + soil moisture)
- **chart** — Google Charts with 1000-point ring buffer
- **udp** — Multicast data sharing between devices
- **benchmark** — VM performance measurement
- **file_io** — LittleFS read/write
- **sensor_read** — Analog sensor with serial output
- **sort** — Bubble sort algorithm
- **strings** — String operations
- **fibonacci** — Recursive function demo

## VS Code Support

Install the `vscode-tinyc` extension for `.tc` syntax highlighting with full TinyC keyword, builtin, and callback coloring. Copy or symlink to `~/.vscode/extensions/tinyc-1.0.0`.

## Reference

Full language specification: [TinyC_Reference.md](TinyC_Reference.md)
