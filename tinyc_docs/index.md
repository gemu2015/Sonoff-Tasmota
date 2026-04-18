# TinyC for Tasmota

**TinyC** is a C-subset compiler and VM that runs on ESP32 and ESP8266 as the Tasmota driver `XDRV_124`. Write C in the browser IDE, compile to portable bytecode, upload and run — no firmware rebuild, no on-device compiler.

![TinyC browser IDE](images/Tinyc_ide.png){ loading=lazy }

## Why TinyC

- **Portable bytecode** — compile once, run the same binary on ESP32, ESP32-S3, ESP32-C3, or ESP8266.
- **No on-device compiler** — the VM is ~12 KB of flash. Compilation happens in the browser.
- **Familiar C syntax** — `int`, `float`, arrays, functions, `for` / `while` / `if` — no new language.
- **10× faster than script interpreters** — direct-threaded bytecode dispatch with no source re-parsing.
- **True background tasks** — `TaskLoop()` runs in a dedicated FreeRTOS task (ESP32) with full `delay()` support.
- **Deep Tasmota integration** — direct access to SML, I2C, display drivers, HomeKit, UDP multicast, webcalls.

## Start here

<div class="grid cards" markdown>

- :material-rocket-launch:{ .lg .middle } __[Getting Started](getting-started.md)__

    ---
    Enable `USE_TINYC` in your build, upload the IDE, write your first program.

- :material-book-open-variant:{ .lg .middle } __[Reference](reference.md)__

    ---
    Full function reference — GPIO, I2C, SML, display, HomeKit, networking.

- :material-code-braces:{ .lg .middle } __[Examples](examples/index.md)__

    ---
    Ready-to-run programs for sensors, displays, meters, and more.

- :material-image-multiple:{ .lg .middle } __[Gallery](gallery/index.md)__

    ---
    Screenshots of real projects running on Tasmota hardware.

- :material-package-variant:{ .lg .middle } __[Releases](releases.md)__

    ---
    Download pre-built firmware for testing.

- :material-tools:{ .lg .middle } __[Custom Builds](custom-builds.md)__

    ---
    Feature flags for trimmed or extended firmware variants.

</div>

## Latest release

The latest test firmware, IDE bundle, and docs are always on the [testing](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing) release tag.
