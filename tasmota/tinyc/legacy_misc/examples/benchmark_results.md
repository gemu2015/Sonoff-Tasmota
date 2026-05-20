# TinyC Benchmark Comparison

Platform: ESP32 (240 MHz dual-core)
Date: 2026-02-24

## Results

| Test                  | Scripter    | TinyC       | Berry       |
|-----------------------|-------------|-------------|-------------|
| Integer  (500k ops)   | 22,288 ms   |             |             |
| Float    (250k ops)   | 10,442 ms   |             |             |
| Array    (100k access) | 20,411 ms  |             |             |
| Calls    (100k calls) | 31,207 ms   |             |             |
| Strings  (10k iters)  |    792 ms   |             |             |
| **Total**             | **85,142 ms** | **8,284 ms** | **3,597 ms** |

## Speed Ratios (relative to Berry)

| Engine   | Total    | Factor  |
|----------|----------|---------|
| Berry    | 3,597 ms |  1.0x   |
| TinyC    | 8,284 ms |  2.3x   |
| Scripter | 85,142 ms | 23.7x  |

## Notes

- Scripter: pure text interpreter, re-parses script on every statement
- TinyC: stack-based bytecode VM with computed goto dispatch
- Berry: register-based bytecode VM, mature optimized implementation
- All Scripter numbers are float (no native integer type)
- Scripter used ct() timer to avoid watchdog timeout
- TinyC result is after computed goto optimization (was 8,896 ms before)
