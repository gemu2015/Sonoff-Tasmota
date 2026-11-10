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
| SD card slot (SPI) CS / MOSI / SCK / MISO | 15 / 13 / 14 / 12 (from the schematic; **IO12 is a strapping pin**: a card that pulls MISO high at reset can stop the boot) |
| lidar data line (LD19 TX) | net `CP_RX` on header **P3** (7 pins, last pin by the schematic order; check with a meter): a jumper from there to the free encoder pin IO16 or IO27 (P3 pins 4 / 2) gives the ESP32 the lidar data (1 k series resistor to the CP2102 stays) |

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

## Lidar driver and the timing question (`common/ld19.tc`)

`examples/common/ld19.tc` reads an LDROBOT **LD19 / STL-19P / D500** 360 degree lidar (UART 230400 baud, 47-byte packets, about 375 packets = 4500 points per second,
10 revolutions per second), checks the CRC8 and sorts every point into one-degree bins: `lid_min(330, 30)` is the nearest thing in front. Demo: `examples/wave_lidar.tc`. A 360 degree radar display as a web page (`/lidar`, with a simulation switch for trying it without a sensor): `examples/lidar_scope.tc`.
Self test with synthetic packets: `examples/wave_lidar_test.tc` (35 checks, nine deliberate mutations of the driver are caught). Written from the LD19 data sheet,
Checked on real data on 10.11.2026 (STL-19P on a Mac: 1262 packets, 0 CRC errors, 9.92 revolutions per second, about 416 packets = 5000 points per second); **not yet run on an ESP32 with a real lidar**.

**Is it time critical in TinyC? Measured on an ESP32-S3 (.190), the parser fed with 400 packets = 1.07 s of real lidar data:**

| | time | share of the real-time data rate |
|---|---|---|
| parsing with CRC check | 238 ms | **22 %** |
| parsing without CRC check (`lid_crc_on = 0`) | 206 ms | 19 % |

- **Not hard real time.** The serial hardware buffers the stream: the driver opens the port with a 2048-byte ring, which holds **116 ms** of lidar data. The program has to
  poll about every 50 ms, a gap of 100 ms still loses nothing; a longer gap only loses a few packets, the parser finds the next packet start by itself (header + CRC).
- **But it is a real load:** about a fifth of the VM's time (roughly 2000 VM instructions per packet, about 3 million per second on the S3). Do it in `TaskLoop()` (its own task),
  not in a callback tick that shares the instruction budget with the rest. The classic ESP32 of the WAVE ROVER board should be in the same range (also 240 MHz), not measured.
- The CRC is cheap (13 % of the parsing); the 12-point angle loop dominates. If the VM is needed for more, the load can be cut by skipping points or by running only part of the scan.
- Mapping (SLAM) or planning is not for this VM: the ESP32 delivers the nearest distances per sector (obstacle stop, wall following), a computer (Mac Studio) would build maps from the raw scan.
- **On the WAVE ROVER board the lidar socket goes to a CP2102 USB bridge, not to the ESP32.** To read the lidar with TinyC its TX line has to go to a free GPIO of an ESP32 (a
  separate S3 board, for instance); the rover's driver board has no free UART.

## Not done yet

- SD card pins, OLED text, the 12 V light outputs (IO4/IO5).
- A faster PWM carrier (the Waveshare firmware uses 100 kHz).
- The Waveshare JSON commands (`{"T":1,"L":0.2,"R":0.2}` etc.) are **not** implemented: a Raspberry Pi running Waveshare's `ugv_rpi` program would need them over the UART
  (the commands are simple, they can be added in TinyC when needed).
- The camera, lidar and arm of the Waveshare line are not part of this.
- A distance sensor: a VL53L0X/VL53L1X goes on the I2C expansion port (address 0x29, no clash with 0x3C, 0x42, 0x0C and the IMU).

## One program for everything: `examples/rover.tc`

`rover.tc` puts all of it into **one TinyC slot**: motors with dead-man watchdog, battery guard, IMU and compass, **four VL53L0X floor sensors behind a TCA9548A against the staircase**, the joystick page and the display. One slot, because on a classic ESP32 with WiFi (`tinyc32-4M-plain`, measured on a board without the rover) every slot costs about **9 KB of RAM on its own**, whatever its code does; the whole program (13 KB bytecode) costs about 49 KB (30 KB internal plus the bytecode and constants, which a PSRAM board hides). The free heap after loading it is about 60 KB on the rover.

- **Staircase guard:** channels 0..3 = front left, front right, rear left, rear right, looking outward and down about 45 degrees. A reading outside the floor window `RoverCliff <min> <max>` (default 25..110 mm) or a silent sensor is a cliff. Front cliff: only straight reverse. Rear cliff: only straight forward. Both, or a sensor fault: no movement at all. `RoverCliff 0` switches the guard off (logged). It runs in this program, whoever sends the commands. Driver: `common/vl53l0x_mux.tc` (derived from the single-sensor driver, state per channel).
- **Display:** the four 21-character lines of the Waveshare OLED (title, IP, state, battery), 128x32 pixels in the top left corner; `DSP_EXTRA 1` adds a larger panel on a TFT.
- **Test without the rover:** `ROVER_SIM 1` leaves the motor pins alone and takes battery and floor distances from `RoverVbat <mV>` / `RoverTof <ch> <mm>` (`-1` = silent sensor). Done this way on an ESP32 with an ILI9341: forward / reverse / turn were allowed or refused as listed above for every combination of front / rear / both / too-close / silent sensor / low battery / guard off, and the watchdog stopped the motors.
- **Park mode** for a rover that stands still (charging or not): `RoverPark 1` (or `/rover_cmd?p=1`) refuses the motors and stops serving the floor sensors, IMU, compass and display (the chips keep running; only their I2C traffic and the display stop), the loop sleeps and only the battery is read every 5 s. A drive command or `RoverPark 0` wakes the rover; the floor sensors must answer again before it may move. `PARK_AUTO_S` > 0 parks it after that many seconds without a command. The big consumers are outside this program: a lidar of 230 mA needs its own 5 V switch, the S3 sleeps on its own. Tried in the simulation on an ESP32-S3: park, wake by drive command, wake by `p=0`, automatic parking.
- Not tested on the rover itself and not with real VL53L0X sensors yet.
