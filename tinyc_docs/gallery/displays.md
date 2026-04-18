# Displays

OLED, TFT, and e-paper panels driven from TinyC — status screens, meter
readouts, Home Assistant-style dashboards.

!!! tip "Adding screenshots"
    Drop PNG/JPG files into `tinyc_docs/images/gallery/displays/` and reference
    them here with `![alt](../images/gallery/displays/<file>.jpg){ loading=lazy }`.

## Sunton 800×480 TFT

Full-screen energy dashboard with kW live gauge, daily bar chart, and weather.

Example: [`sunton_display.tc`](../examples/sunton_display.md)

## 2.9" e-paper

Low-power partial-refresh meter screen with PV/consumption totals.

Example: [`epaper29.tc`](../examples/epaper29.md)

## SSD1306 128×64 OLED

Minimal three-line status panel — IP, uptime, current sensor reading.

Example: [`lcd_i2c.tc`](../examples/lcd_i2c.md)
