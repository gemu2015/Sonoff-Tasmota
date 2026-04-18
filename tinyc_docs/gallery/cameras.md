# Cameras

ESP32-S3 camera boards driven by TinyC — MJPEG stream server, snapshot on
motion, pipe frames into the Tasmota webUI.

!!! tip "Adding screenshots"
    Drop PNG/JPG files into `tinyc_docs/images/gallery/cameras/` and reference
    them here with `![alt](../images/gallery/cameras/<file>.jpg){ loading=lazy }`.

## DFRobot DFR1154 (OV3660)

3MP sensor, MJPEG stream at 15 fps on `tinyc32s3-cam-dfrobot`.

Example: [`webcam.tc`](../examples/webcam.md)

## Goouuu ESP32-S3-CAM (OV2640)

2MP sensor, low-cost board. MJPEG + web snapshot button.

Example: [`webcam_tinyc.tc`](../examples/webcam_tinyc.md)

## Text overlay

Timestamp and live sensor value rendered onto the JPEG frame before streaming.

Example: [`text_on_image.tc`](../examples/text_on_image.md)
