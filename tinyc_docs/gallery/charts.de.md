# Diagramme

Live-Plots, gezeichnet mit `gfxDrawLine` / `gfxFillRect` aus rollenden
TinyC-Puffern, oder als SVG/PNG ueber den geraeteseitigen HTTP-Hook ausgeliefert.

!!! tip "Screenshots hinzufuegen"
    PNG/JPG-Dateien nach `tinyc_docs/images/gallery/charts/` ablegen und hier mit
    `![alt](../images/gallery/charts/<datei>.png){ loading=lazy }` einbinden.

## Energie-Verlauf

24-Stunden-Rollplot fuer PV gegen Verbrauch.

Beispiel: [`live_chart.tc`](../examples/live_chart.md)

## Temperatur-Verlauf

Temperatur pro Minute mit Min/Max-Markern.

Beispiel: [`lcd_chart.tc`](../examples/lcd_chart.md)

## Live-Messgeraet

Modbus- / SML-Werte via `addHandler` in eine SVG-Anzeige gestreamt.

Beispiel: [`web_handler.tc`](../examples/web_handler.md)
