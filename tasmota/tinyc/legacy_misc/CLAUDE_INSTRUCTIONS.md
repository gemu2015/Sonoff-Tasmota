# TinyC Development Workflow — Instructions for AI Assistants

## CRITICAL: Do NOT upload .tc source files to the device

The device has very limited resources. **Never** upload raw `.tc` source code to the device
or try to compile on the device. The correct workflow is:

1. Write/edit the `.tc` source file on the **PC**
2. Compile to `.tcb` bytecode on the **PC**
3. Upload only the small `.tcb` binary to the device
4. Run and test via Tasmota commands or the API

---

## Step 1: Write the .tc source file

TinyC is a subset of C. Source files use the `.tc` extension.
See `TinyC_Reference.md` for the full language reference.

Key constraints:
- No `malloc`/`free` — use fixed-size arrays
- Strings are fixed-size `char[]` buffers
- Use `persist` keyword for variables that survive restarts
- Use callback functions: `EverySecond()`, `WebUI()`, `WebCall()`, `JsonCall()`, `TaskLoop()`, `OnExit()`
- `#define` and `#ifdef` preprocessor directives are supported

Example files are in the `examples/` directory.

---

## Step 2: Compile on the PC

### Option A: Command-line compiler (preferred for automation)

```bash
cd /Volumes/vp_dev/TinyC
node compile_cli.js <input.tc> [output.tcb] [-DNAME ...]
```

Example:
```bash
node compile_cli.js examples/my_script.tc /tmp/my_script.tcb
```

Output: `Compiled examples/my_script.tc -> /tmp/my_script.tcb (1234 bytes)`

If compilation fails, the error message includes line numbers — fix the source and retry.

### Option B: All-in-one script (compile + upload + run)

```bash
cd /Volumes/vp_dev/TinyC
./push_tcb.sh examples/my_script.tc 192.168.188.xxx
```

This compiles, uploads, stops any running program, loads the bytecode, and runs it.

### Option C: Browser IDE

Open `tinyc_ide.html` in a browser. It provides an editor, compiler, and device
connection UI. Useful for interactive development but not for AI-assisted workflows.

---

## Step 3: Upload the .tcb binary to the device

Upload uses HTTP multipart POST to the device's filesystem upload endpoint:

```bash
DEVICE_IP="192.168.188.xxx"
TCB_FILE="/tmp/my_script.tcb"
FILENAME="my_script.tcb"
SIZE=$(stat -f%z "$TCB_FILE" 2>/dev/null || stat -c%s "$TCB_FILE" 2>/dev/null)

curl -s -o /dev/null -w "%{http_code}" \
    -X POST "http://${DEVICE_IP}/ufsu?fsz=${SIZE}" \
    -F "ufsu=@${TCB_FILE};filename=${FILENAME}"
```

**IMPORTANT**: Use `-F "file=@..."` (multipart form), NOT `--data-binary`.
Using `--data-binary` will crash the device's raw upload handler.

Expected response: HTTP 200

---

## Step 4: Control the program on the device

### Via Tasmota console commands (browser: http://DEVICE_IP/cs)

| Command | Description |
|---|---|
| `TinyCRun 0 /my_script.tcb` | Load and run slot 0 from filesystem |
| `TinyCStop 0` | Stop slot 0 |
| `TinyCReset 0` | Reset slot 0 (clear state) |
| `TinyC` | Show VM status for all slots |
| `TinyCInfo 0` | Show detailed info for slot 0 |

**No spaces between command name and number** — `TinyCRun 0` not `TinyC Run 0`.

### Via HTTP API (better for automation)

```bash
# Stop current program
curl -s "http://${DEVICE_IP}/tc_api?cmd=stop"

# Load bytecode file
curl -s "http://${DEVICE_IP}/tc?cmd=load&file=/my_script.tcb"

# Run
curl -s "http://${DEVICE_IP}/tc_api?cmd=run"

# Check status
curl -s "http://${DEVICE_IP}/tc_api?cmd=status"
```

### Via Tasmota command API

```bash
# Run
curl -s "http://${DEVICE_IP}/cm?cmnd=TinyCRun%200%20/my_script.tcb"

# Stop
curl -s "http://${DEVICE_IP}/cm?cmnd=TinyCStop%200"

# Status
curl -s "http://${DEVICE_IP}/cm?cmnd=TinyC"
```

---

## Step 5: Check results

### Read Tasmota console output
```bash
curl -s "http://${DEVICE_IP}/cs?" | grep -o 'TCC:[^<]*'
```

### Read JSON telemetry (if script implements JsonCall)
```bash
curl -s "http://${DEVICE_IP}/cm?cmnd=status%2010"
```

### Read the web UI (if script implements WebUI)
Open `http://DEVICE_IP` in a browser — the script's UI appears on the main page.

### Read tc_api status
```bash
curl -s "http://${DEVICE_IP}/tc_api?cmd=status"
```

Returns JSON with VM state, uptime, memory usage.

---

## Typical AI-assisted development cycle

```bash
DEVICE_IP="192.168.188.xxx"
TC_DIR="/Volumes/vp_dev/TinyC"
SCRIPT="examples/my_script.tc"

# 1. Edit the .tc file (use your editor/Write tool)

# 2. Compile
node "$TC_DIR/compile_cli.js" "$SCRIPT" /tmp/my_script.tcb
# If error: fix the .tc file and retry

# 3. Stop any running program
curl -s "http://${DEVICE_IP}/cm?cmnd=TinyCStop%200"

# 4. Upload
SIZE=$(stat -f%z /tmp/my_script.tcb 2>/dev/null || stat -c%s /tmp/my_script.tcb)
curl -s -o /dev/null -w "%{http_code}" \
    -X POST "http://${DEVICE_IP}/ufsu?fsz=${SIZE}" \
    -F "ufsu=@/tmp/my_script.tcb;filename=my_script.tcb"

# 5. Run
curl -s "http://${DEVICE_IP}/cm?cmnd=TinyCRun%200%20/my_script.tcb"

# 6. Check status after a few seconds
sleep 3
curl -s "http://${DEVICE_IP}/tc_api?cmd=status"
```

---

## Autoexec (auto-start on boot)

To make a script run automatically on device boot:
```bash
curl -s "http://${DEVICE_IP}/tc_api?cmd=autoexec&file=/my_script.tcb"
```

To clear autoexec:
```bash
curl -s "http://${DEVICE_IP}/cm?cmnd=UfsDelete%20/tinyc.cfg"
```

---

## Common pitfalls

1. **Do NOT upload .tc source to the device** — only compiled .tcb bytecode
2. **Do NOT use `--data-binary` for upload** — use multipart `-F` form upload
3. **Compile errors** are reported with line numbers — always fix and recompile on PC
4. **Large scripts**: The VM has limited stack (256 entries) and memory. Keep it simple.
5. **Camera/display/hardware APIs**: These require specific build flags (e.g., `-DTINYC_CAMERA`). Check `TinyC_Custom_Builds.md` for build configurations.
6. **`persist` variables**: Stored in flash, survive restarts. Use `persist watch int` for change detection with `changed()` / `snapshot()`.
7. **TaskLoop vs EverySecond**: `TaskLoop()` runs in the VM task thread (fast, for hardware I/O). `EverySecond()` runs in the main Tasmota thread (for timers, logic). Don't do blocking operations in `EverySecond()`.
