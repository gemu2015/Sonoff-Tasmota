# Displays

OLED, TFT und E-Paper von TinyC angesteuert — Statusbildschirme,
Zaehlerwerte, Dashboards im Home-Assistant-Stil.

!!! tip "Screenshots hinzufuegen"
    PNG/JPG-Dateien nach `tinyc_docs/images/gallery/displays/` ablegen und hier mit
    `![alt](../images/gallery/displays/<datei>.jpg){ loading=lazy }` einbinden.

## Sunton 800×480 TFT

Vollbild-Energie-Dashboard mit kW-Live-Zeiger, Tages-Balkendiagramm und Wetter.

Beispiel: [`sunton_display.tc`](../examples/sunton_display.md)

## 2.9" E-Paper

Stromsparender Zaehlerbildschirm mit Teilaktualisierung (PV-/Verbrauchssummen).

Beispiel: [`epaper29.tc`](../examples/epaper29.md)

## SSD1306 128×64 OLED

Minimales Drei-Zeilen-Statuspanel — IP, Laufzeit, aktueller Messwert.

Beispiel: [`lcd_i2c.tc`](../examples/lcd_i2c.md)
