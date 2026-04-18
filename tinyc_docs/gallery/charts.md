# Charts

Live plots drawn with `gfxDrawLine` / `gfxFillRect` from rolling TinyC buffers,
or served as SVG/PNG via the on-device HTTP hook.

!!! tip "Adding screenshots"
    Drop PNG/JPG files into `tinyc_docs/images/gallery/charts/` and reference
    them here with `![alt](../images/gallery/charts/<file>.png){ loading=lazy }`.

## Energy history

24-hour rolling plot of PV vs. consumption.

Example: [`live_chart.tc`](../examples/live_chart.md)

## Temperature trace

Minute-by-minute temperature with min/max markers.

Example: [`lcd_chart.tc`](../examples/lcd_chart.md)

## Live meter

Modbus / SML values streamed into an SVG gauge via `addHandler`.

Example: [`web_handler.tc`](../examples/web_handler.md)
