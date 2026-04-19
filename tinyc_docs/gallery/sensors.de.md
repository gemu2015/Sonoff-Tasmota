# Sensoren

Umwelt-, Leistungs- und Praesenzsensoren von TinyC angesteuert —
Verdrahtung, Treiber und Live-Messwerte.

!!! tip "Screenshots hinzufuegen"
    PNG/JPG-Dateien nach `tinyc_docs/images/gallery/sensors/` ablegen und hier mit
    `![alt](../images/gallery/sensors/<datei>.jpg){ loading=lazy }` einbinden.

## LD2410 24-GHz-Praesenzradar

Bewegung plus statische Praesenz ueber UART.

Beispiel: [`ld2410.tc`](../examples/ld2410.md)

## DS18B20 / 1-Wire

Mehrstraengige Temperaturkette mit automatischer Erkennung.

Beispiel: [`onewire.tc`](../examples/onewire.md)

## SML-Stromzaehler

Vollpraezise Energiezaehler vom IR-Kopf gelesen, per Modbus TCP verteilt.

Beispiel: [`sml_ebus.tc`](../examples/sml_ebus.md)

## SMA Speedwire-Wechselrichter

UDP-Multicast-Erkennung plus live PV-Leistung.

Beispiel: [`sma_speedwire.tc`](../examples/sma_speedwire.md)
