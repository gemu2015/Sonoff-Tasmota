# Getting Started

## 1. Build Tasmota with TinyC

Add the following to your `user_config_override.h`:

```c
#define USE_TINYC         // Enable TinyC VM (XDRV_124)
#define USE_TINYC_IDE     // Self-hosted browser IDE (requires USE_UFILESYS)
```

`USE_TINYC_IDE` adds the `/tinyc_ide.html` endpoint. It requires a filesystem-enabled
build (`USE_UFILESYS`).

Or grab a pre-built binary from the [Releases](releases.md) page and flash it directly.

## 2. Upload the IDE

1. Download `tinyc_ide.html.gz` from the [testing release](https://github.com/gemu2015/Sonoff-Tasmota/releases/tag/testing).
2. In Tasmota, open **Consoles → Manage File System** (or POST to `http://<device>/ufsu`).
3. Upload `tinyc_ide.html.gz` to the root of the filesystem.
4. Open `http://<device-ip>/tinyc_ide.html` in your browser.

## 3. Your first program

```c
void main() {
    addLog("Hello from TinyC!");
}

void EverySecond() {
    float t = temperature();
    char buf[64];
    sprintf(buf, "temp=%.1f C", t);
    addLog(buf);
}
```

- **Ctrl+Enter** compiles.
- **Ctrl+Shift+Enter** uploads + runs.
- **Stop** button halts execution.

Console output appears in the Tasmota **Console** tab.

## 4. Where to go next

- Browse the [function reference](reference.md) — every syscall with signatures and examples.
- Look at the [examples](examples/index.md) — working code for common sensors, displays, and protocols.
- See the [gallery](gallery/index.md) — screenshots of projects on real hardware.
