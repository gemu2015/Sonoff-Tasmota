# Sensors

Environmental, power, and presence sensors driven from TinyC — wiring,
drivers, and live readouts.

!!! tip "Adding screenshots"
    Drop PNG/JPG files into `tinyc_docs/images/gallery/sensors/` and reference
    them here with `![alt](../images/gallery/sensors/<file>.jpg){ loading=lazy }`.

## LD2410 24 GHz presence radar

Motion + static presence over UART.

Example: [`ld2410.tc`](../examples/ld2410.md)

## DS18B20 / 1-Wire

Multi-drop temperature string with auto-discovery.

Example: [`onewire.tc`](../examples/onewire.md)

## SML smart meter

Full-precision energy totals read from an IR head, broadcast via Modbus TCP.

Example: [`sml_ebus.tc`](../examples/sml_ebus.md)

## SMA Speedwire inverter

UDP multicast discovery + live PV power.

Example: [`sma_speedwire.tc`](../examples/sma_speedwire.md)
