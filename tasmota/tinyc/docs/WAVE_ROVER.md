# WAVE ROVER with TinyC (Waveshare General Driver for Robots)

How to run the Waveshare **WAVE ROVER** (4WD metal chassis) with Tasmota/TinyC on its ESP32 driver board instead of the
Waveshare firmware. Program: `examples/wave_rover.tc`. Deutsch: [WAVE_ROVER_DE.md](WAVE_ROVER_DE.md).

> **Status: written from the Waveshare documentation and source before the rover arrived. Nothing here has run on the rover yet.**
> The drive logic (mixing, PWM mapping, battery guard) was checked in the IDE simulator on the Mac, the pins and I2C
> addresses come from the Waveshare source. Please correct this file after the first test.

## The hardware (Waveshare wiki and `waveshareteam/ugv_base_general`)

- Driver board "General Driver for Robots": **ESP32-WROOM-32UE**, IPEX antenna connector (**keep the antenna connected**),
  **4 MB flash** (the original partition table ends at 0x400000), no PSRAM. Two USB-C sockets: **"USB"** (ESP32, CP2102,
  automatic download circuit, no boot button needed) and **"LIDAR"** (a second CP2102 that only forwards a lidar's data to a host,
  it is **not** the ESP32).
- Motors: four N20 gear motors **without encoders**, two on each side in parallel, driven by one **TB6612FNG**.
  Channel **A = left, B = right**. No wheel speed, no odometry. Below PWM 23 the motors do not turn.
- On the I2C bus: **QMI8658C** (accelerometer + gyro, + temperature), **AK09918C** (compass), **INA219 at 0x42** (battery voltage/current),
  **SSD1306 OLED 128x32 at 0x3C**. Also on the board: SD card slot, serial bus servo interface (ST3215), two 12 V switched outputs (IO4, IO5, PWM) for lights.
- 3S 18650 holder with charger (the 12.6 V adapter is in the box, the cells are not).

| Function | GPIO |
|---|---|
| TB6612 PWM A / B | 25 / 26 |
| direction A: AIN1, AIN2 | 21, 17 |
| direction B: BIN1, BIN2 | 22, 23 |
| I2C SDA / SCL | 32 / 33 |
| bus servos RX / TX (half duplex via board circuit) | 18 / 19 |
| IO4 / IO5 (12 V switch outputs) | 4 / 5 |
| encoders (not used on the WAVE ROVER) | 35, 34, 27, 16 |
| SD card slot | **not found**, see the Waveshare schematic |

Forward = IN1 high, IN2 low (WAVE ROVER, `SET_MOTOR_DIR = false` in the Waveshare source). The Waveshare firmware drives the PWM at 100 kHz,
8 bit. In TinyC `analogWrite()` uses the Arduino default (1 kHz): it works, the motors whine a little.

## Tasmota settings

Build `tinyc32-4M-plain` (classic ESP32, 4 MB, no Matter, no camera; about 238 KB flash free). In *Configure Module* use the generic module and set only
**GPIO32 = I2C SDA, GPIO33 = I2C SCL**. **Leave the motor pins (17, 21, 22, 23, 25, 26) unassigned**: TinyC refuses pins that Tasmota has taken for a function.
Tasmota's own INA219 driver does not know address 0x42 (to be checked); `wave_rover.tc` reads the bus voltage register itself.
IMU and compass: no Tasmota driver, read the registers from TinyC (the Waveshare source has `QMI8658.cpp` / `AK09918.cpp` as a reference).

## Before you flash: save the original

1. Open the rover and plug a USB cable into the board's **USB** socket (not LIDAR). The Waveshare wiki says the robot has to be taken apart for this.
2. Check the chip and flash size, then read the whole flash (esptool from PlatformIO; esptool 5 writes `read-flash`, older versions `read_flash`):

```bash
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX flash-id
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 921600 read-flash 0 0x400000 wave_rover_original_4MB.bin
```

3. **Keep that file** (not in the repository: it contains the rover's WiFi settings once they are entered). To go back:
   `... write-flash 0 wave_rover_original_4MB.bin`. The Waveshare firmware source and a build are in `waveshareteam/ugv_base_general`
   (`General_Driver/build/...`), and Waveshare offers an "ESP32 Download Tool" for the factory image.

## Flash Tasmota/TinyC

```bash
pio run -e tinyc32-4M-plain --target upload --upload-port /dev/cu.usbserial-XXXX
```

The partition layout changes (the original has a 3 MB app and no OTA slot). After the first start: join the `tasmota-xxxx` hotspot, enter the WiFi,
set the two I2C pins, then load `wave_rover.tcb` (IDE or `scripts/push_tcb.sh examples/wave_rover.tc <ip>`), start it. Later updates go over WiFi (OTA).

## First drive (do these in this order)

1. **Wheels off the ground** (rover on a box), battery in. If the LED on the power board lights when you insert the cells, they are the wrong way round: take them out.
2. Open `http://<ip>/rover`. The speed limit starts at **40 %**. Touch the pad: both sides must turn forward when you push the dot up, left side faster when you push right.
   If a side turns the wrong way or left/right are swapped, change `DIR_L` / `DIR_R` in the program (or swap the roles in the pad).
3. Take the finger away: the motors must stop. Close the tab while driving: they must stop within about 0.3 s. Disable WiFi on the phone: same.
4. The main page shows the battery voltage (INA219) and the number of watchdog stops. Below 9.3 V the motors are refused.
5. Only then put it on the floor.

## Sensor drivers (TinyC libraries)

Three libraries in `examples/common/`, each `#include`-able, no `main()`, no callbacks. They claim their I2C address (`i2cSetActiveFound`) so other drivers stay away.

| Library | Chip | Calls | Results |
|---|---|---|---|
| `ina219.tc` | INA219 at 0x42 | `ina_init(addr, bus)`, `ina_poll()`, `ina_release()` | `ina_mv` mV, `ina_ma` mA, `ina_mw` mW, `ina_shunt_uv`, `ina_ovf` |
| `qmi8658.tc` | QMI8658C at 0x6B / 0x6A | `qmi_init(bus, acc_range, gyr_range, odr)`, `qmi_poll()`, `qmi_cal_gyro(n)`, `qmi_yaw_reset()` | `qmi_ax..az` g, `qmi_gx..gz` dps, `qmi_temp` C, `qmi_yaw` degrees (integrated gyro) |
| `ak09918.tc` | AK09918C at 0x0C | `ak_init(bus, mode)`, `ak_poll()`, `ak_heading()`, `ak_cal_begin()` / `ak_cal_end()` | `ak_x/y/z` uT, `ak_heading()` degrees 0..360 |

`examples/wave_rover_sensors.tc` shows all of them on the Tasmota main page and in the telemetry (with the commands `RoverCalGyro`, `RoverYaw0`, `RoverCalMag 1|0`).
`examples/wave_sensors_test.tc` tests the decoding of all three **without hardware** (47 checks: byte order, signs, scale factors, overflow flag, calibration, heading);
it runs on any TinyC device. It cannot show that the I2C access itself works. Details (units, register layout, calibration) are in the header of each library.
Written from the datasheets and the Waveshare source (GPL, used as a register reference only), **not tested on the rover yet**. Open: which chip axis points "forward" on the board, and how
strongly the motors disturb the compass (it sits next to them: calibrate with the motors off and check the field while driving).

## Not done yet

- SD card pins, OLED text, the 12 V light outputs (IO4/IO5).
- A faster PWM carrier (the Waveshare firmware uses 100 kHz).
- The Waveshare JSON commands (`{"T":1,"L":0.2,"R":0.2}` etc.) are **not** implemented: a Raspberry Pi running Waveshare's `ugv_rpi` program would need them over the UART
  (the commands are simple, they can be added in TinyC when needed).
- The camera, lidar and arm of the Waveshare line are not part of this.
- A distance sensor: a VL53L0X/VL53L1X goes on the I2C expansion port (address 0x29, no clash with 0x3C, 0x42, 0x0C and the IMU).
