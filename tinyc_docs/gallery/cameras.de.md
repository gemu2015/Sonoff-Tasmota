# Kameras

ESP32-S3-Kameraboards von TinyC angesteuert — MJPEG-Stream-Server,
Schnappschuss bei Bewegung, Einspeisung von Frames in die Tasmota-WebUI.

!!! tip "Screenshots hinzufuegen"
    PNG/JPG-Dateien nach `tinyc_docs/images/gallery/cameras/` ablegen und hier mit
    `![alt](../images/gallery/cameras/<datei>.jpg){ loading=lazy }` einbinden.

## DFRobot DFR1154 (OV3660)

3-MP-Sensor, MJPEG-Stream mit 15 fps auf `tinyc32s3-cam-dfrobot`.

Beispiel: [`webcam.tc`](../examples/webcam.md)

## Goouuu ESP32-S3-CAM (OV2640)

2-MP-Sensor, guenstige Platine. MJPEG plus Schnappschuss-Knopf im Web.

Beispiel: [`webcam_tinyc.tc`](../examples/webcam_tinyc.md)

## Text-Overlay

Zeitstempel und aktueller Sensorwert vor dem Streamen ins JPEG-Bild eingesetzt.

Beispiel: [`text_on_image.tc`](../examples/text_on_image.md)
