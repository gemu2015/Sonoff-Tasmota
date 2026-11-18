# WAVE ROVER with TinyC (Waveshare General Driver for Robots)

How to run the Waveshare **WAVE ROVER** (4WD metal chassis) with Tasmota/TinyC on its ESP32 driver board instead of the
Waveshare firmware. Program: `examples/wave_rover.tc`. Deutsch: [WAVE_ROVER_DE.md](WAVE_ROVER_DE.md).

> **Status: written from the Waveshare documentation and source before the rover arrived. Nothing here has run on the rover yet.**
> The drive logic (mixing, PWM mapping, battery guard) was checked in the IDE simulator on the Mac, the pins and I2C
> addresses come from the Waveshare source. Please correct this file after the first test.

## The hardware (Waveshare wiki and `waveshareteam/ugv_base_general`)

- Driver board "General Driver for Robots": **ESP32-WROOM-32UE**, IPEX antenna connector (**keep the antenna connected**),
  **16 MB flash** (measured with esptool 11.11.2026: manufacturer 0x46, device 0x4018; the original firmware uses only the first 4 MB), no PSRAM. Two USB-C sockets: **"USB"** (ESP32, CP2102,
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

Build `tinyc32-4M-plain` (classic ESP32, 4 MB partition table, no Matter, no camera; about 238 KB flash free; it runs on the 16 MB chip and uses the first 4 MB). In *Configure Module* use the generic module and set only
**GPIO32 = I2C SDA, GPIO33 = I2C SCL**. **Leave the motor pins (17, 21, 22, 23, 25, 26) unassigned**: TinyC refuses pins that Tasmota has taken for a function.
Tasmota's own INA219 driver does not know address 0x42 (to be checked); `wave_rover.tc` reads the bus voltage register itself.
IMU and compass: no Tasmota driver, read the registers from TinyC (the Waveshare source has `QMI8658.cpp` / `AK09918.cpp` as a reference).

## Before you flash: save the original

1. Open the rover and plug a USB cable into the board's **USB** socket (not LIDAR). The Waveshare wiki says the robot has to be taken apart for this.
2. Check the chip and flash size, then read the whole flash (esptool from PlatformIO; esptool 5 writes `read-flash`, older versions `read_flash`):

```bash
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX flash-id
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 230400 read-flash 0 0x1000000 wave_rover_werk_flash_16MB.bin
```

3. **Keep that file** (not in the repository: it contains the rover's WiFi settings once they are entered). To go back:
   `... write-flash 0 wave_rover_werk_flash_16MB.bin`. The Waveshare firmware source and a build are in `waveshareteam/ugv_base_general`
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
Checked on real data on 10.11.2026 (STL-19P on a Mac: 1262 packets, 0 CRC errors, 9.92 revolutions per second, about 416 packets = 5000 points per second); run on the rover's ESP32 with an LD06 on 17.11.2026 (see "Floor sensors, lidar and driving direction" below).

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

- **Staircase guard:** channels 0..3 = rear left, rear right, front right, front left (as wired on this rover, see below), looking outward and down about 45 degrees. A reading outside the floor window `RoverCliff <min> <max>` (default 25..110 mm) or a silent sensor is a cliff. Front cliff: only straight reverse. Rear cliff: only straight forward. Both, or a sensor fault: no movement at all. `RoverCliff 0` switches the guard off (logged). It runs in this program, whoever sends the commands. Driver: `common/vl53l0x_mux.tc` (derived from the single-sensor driver, state per channel).
- **Display:** the four 21-character lines of the Waveshare OLED (title, IP, state, battery), 128x32 pixels in the top left corner; `DSP_EXTRA 1` adds a larger panel on a TFT.
- **Test without the rover:** `ROVER_SIM 1` leaves the motor pins alone and takes battery and floor distances from `RoverVbat <mV>` / `RoverTof <ch> <mm>` (`-1` = silent sensor). Done this way on an ESP32 with an ILI9341: forward / reverse / turn were allowed or refused as listed above for every combination of front / rear / both / too-close / silent sensor / low battery / guard off, and the watchdog stopped the motors.
- **Park mode** for a rover that stands still (charging or not): `RoverPark 1` (or `/rover_cmd?p=1`) refuses the motors and stops serving the floor sensors, IMU, compass and display (the chips keep running; only their I2C traffic and the display stop), the loop sleeps and only the battery is read every 5 s. A drive command or `RoverPark 0` wakes the rover; the floor sensors must answer again before it may move. `PARK_AUTO_S` > 0 parks it after that many seconds without a command. The big consumers are outside this program: a lidar of 230 mA needs its own 5 V switch, the S3 sleeps on its own. Tried in the simulation on an ESP32-S3: park, wake by drive command, wake by `p=0`, automatic parking.
- Not tested on the rover itself and not with real VL53L0X sensors yet.

## Measured on the rover (11.11.2026)

- **Read the flash at 230400 baud**: 921600 and 460800 gave "Invalid head of packet" / "Corrupt data" on the CP2102. 16 MB take about 13 minutes at 230400. `verify-flash` against the chip matched.
- I2C scan (GPIO32 = SDA, GPIO33 = SCL, set with `Gpio32 640`, `Gpio33 608`, then `Restart 1`): **0x0C** AK09918C, **0x3C** SSD1306, **0x42** INA219, **0x6B** QMI8658C, **0x7E** unidentified; BMP280 (0x77) is not fitted.
- **TinyC refused IO17 ("Forbidden pin")**: Tasmota paints GPIO16/17 red on the classic ESP32 (PSRAM/flash on WROVER/PICO modules). The WROOM-32UE has no PSRAM and the rover uses IO17 (AIN2) and IO16 (encoder B). `tc_pin_red()` in `xdrv_124_tinyc_vm.h` frees GPIO16/17 when `FoundPSRAM()` is false. Needs firmware built after 11.11.2026.
- After a fresh flash the HTTP command API is off: type `SetOption128 1` in the web console once.
- Drive test on blocks: both sides turn the right way; the chassis is symmetric, so "front" is a choice (decide it before mounting the floor sensors and the camera).


## Wheel encoders (11.11.2026)

Library `examples/common/wheel_enc.tc`, used by `rover.tc`: ticks, distance, speed, odometry (x, y, heading) and a **stall guard** for the two drive sides. It counts with the ESP32's **hardware pulse counter** (`pcntBegin`, with a glitch filter; ABI 35) and falls back to Tasmota's own counters (`Counter1` / `Counter2`, interrupt driven) when the hardware counter cannot start. **Use the hardware counter:** next to the motor PWM Tasmota's counters ran at full speed with the wheel held still (4826 / 8093 ticks in 3 s, 13.11.2026), a millisecond debounce cannot tell the 2000..5000 real pulses per second from a spike. With `pcntBegin` and a 10 us filter the held wheel counts 172 ticks in 3 s instead of about 8000, and both sides count the same at the same command (4399 / 4391).

1. Wire the Waveshare encoder motors to the sockets H3 / H4. Side A (left) C1 = GPIO34, side B (right) C1 = GPIO27.
2. **Free the pins in Tasmota:** `Gpio34 0`, `Gpio27 0`, `Restart 1` (done on the rover 13.11.2026). The hardware counter needs the pins FREE (the log says `PCNT unit 0 on GPIO34, filter 10000 ns`). With `Gpio34 352` / `Gpio27 353` (`Counter1` / `Counter2`) `pcntBegin` fails and `wheel_enc.tc` falls back to those counters, with the noise. Turn a wheel by hand: `RoverEnc` ticks run. Wiring check: each motor socket must carry the ENCODER motor of its own side; with the two motors of one side on one channel turning a wheel by hand must not turn a wheel on the other side (13.11.2026: two motors had to be swapped).
3. `RoverEnc` shows ticks and the state; `RoverEncZero`, roll the rover a measured distance, `RoverEncCal <mm>` sets the mm per tick (the default is **1078 ticks per wheel revolution**, measured on 13.11.2026 by turning each wheel of the rover with its geared encoder motors one revolution by hand: 1031 and 1125, mean 1078; the earlier estimate of 350 = 7 pulses x 50 : 1 was three times too low. Turn a wheel 5 revolutions for a better value).
4. `RoverEnc 1` switches the stall guard on: a side that is commanded but gives no tick for 800 ms stops the rover ("MOTOR STALL") until the stick is released. It is **off by default**, because without encoders every drive would count as a stall. Tested on the rover 13.11.2026 with a held wheel and the hardware counter: the rover stops about 0.8 s after the last tick of the blocked side and stays off until the stick is released. The guard sees only the wheels WITH an encoder; a blocked wheel without encoder next to a turning encoder wheel is not detected.

Only one channel is counted, so the direction comes from the motor command (the last non-zero direction). `ENC_BOTH 1` also counts channel B (`Gpio35 354`, `Gpio16 355`) for twice the resolution. In `ROVER_SIM 1` the ticks are made up from the command (`RoverEncSim <ticks per second>`, 0 = blocked wheel). Tried in the simulation on the rover board; **not yet with real encoders**. GPIO34 / GPIO27 are not free for anything else once the encoders are used (a lidar wire from `CP_RX` needs another pin).

## RAM (11.11.2026, measured on the rover board, no PSRAM)

- `rover.tc` alone leaves about **50 KB** free heap (largest block 45 KB). A **second slot costs far more than its program**: a 4.8 KB test program in slot 1 took the free heap from 48 KB to 14 KB. Keep the rover in ONE slot, and run bench programs (`examples/cliff_test.tc`) in place of it, not next to it. Aim at 30 KB free or more.
- The joystick page is the file **`/rover.html`** (`examples/rover.html`), served with `webSendFile`; upload it once together with the program (`curl -X POST "http://<ip>/ufsu?fsz=<size>" -F "ufsu=@rover.html;filename=rover.html"`). Without the file `/rover` is empty. This took 2 KB of constants out of the bytecode.
- `ROVER_SIM` and `DSP_EXTRA` are compile-time switches (`#if`), so the simulation and the TFT panel are not in the bytecode of the real rover: 16.1 KB -> 13.3 KB with `DSP_EXTRA 0`.
- Further savings, not done yet: a firmware without the repo IDE page (`-DTINYC_NO_REPO_IDE`, about 4 KB static), other unused TinyC function groups, MQTT/mDNS off at run time (not measured).

## RAM, second round (12.11.2026, measured on the rover, free internal heap in bytes)

| State | before | after |
|---|---|---|
| no program loaded, fresh boot (`tinyc32-4M-plain`) | 95 424 | 108 092 |
| `rover.tc` loaded | about 47 000 - 53 000 | **64 544** (66 720 with `TinyCStack 8192`) |

What brought it:
- **A leak in the firmware (fixed in the source):** every program start lost a 1 KB frame block (`frames[0]` overwritten by the entry CALL), 35 KB gone after 28 reloads. Details in the CHANGELOG.
- **The upload server task (8 KB) now lives only while a file arrives.**
- **A lean build for the rover**, 4.3 KB more heap and 157 KB less flash (1 700 416 -> 1 543 536 B): `tinyc32-4M-plain` without `USE_SML_M/_TCP/_SCRIPT_CMD`, `USE_HOME_ASSISTANT`, `USE_IMPROV`, `USE_PING`, `USE_DEEPSLEEP`, `USE_TIMERS/_WEB`, `USE_SUNRISE`, `USE_TOUCH_BUTTONS`, `USE_UNIVERSAL_TOUCH`, `USE_SENDMAIL`, `USE_SHINE`, `USE_KNX_WEB_MENU`. It needs the counters (`USE_COUNTER`), I2C and the display, which stay in. The switches are a block `#ifdef TINYC_ROVER_LEAN` with `#undef ...` at the end of the (git-ignored) `user_config_override.h` and a local env `[env:tinyc32-4M-rover] extends = env:tinyc32-4M-plain`, `build_flags = ${env:tinyc32-4M-plain.build_flags} -DTINYC_ROVER_LEAN`. Tasmota features cost flash far more than RAM: 157 KB of flash bought 4.3 KB of heap.
- `TinyCStack 8192` (the VM task needed 4.9 KB at most with `rover.tc`; the default stays 12288 because a script that calls `saveVars()` from `main()` needs more).

Where the RAM of a classic ESP32 without PSRAM goes (`TinyCStack` and `TinyCHeap` show it): the FreeRTOS task stacks alone are about 54 KB (loopTask 12 288, VM task 12 288, `tiT` 4 608, `arduino_events` / `esp_timer` / `Tmr Svc` 4 096 each, `wifi` 3 584 ...) of a heap of about 208 KB. The next steps would be in the WiFi / lwIP / task stack settings of the Arduino libraries (a rebuild of the libs, about 8 minutes, `custom_sdkconfig`), not in Tasmota features.

**RAM, third round (13.11.2026): no Bluetooth in the SDK, +37 KB.** The stock Tasmota arduino-libs keep Bluetooth (`CONFIG_BT_ENABLED=y`) switched on, which reserves about 36 KB of internal DRAM on a classic ESP32 even when it is never used. With `custom_sdkconfig = CONFIG_BT_ENABLED=n` (and the NimBLE libraries ignored) the rover has **100 KB** free with `rover.tc` running instead of 63 KB, the largest block grows from about 53 KB to 98 304 B. The rover env inherits it from `tinyc32-4M-plain`. Measured with `TinyCHeap` (total internal heap 248 KB instead of about 207 KB).

## Floor sensors, lidar and driving direction (17.11.2026, measured on the rover)

- **Multiplexer on the board's own bus.** The TCA9548A sits on the normal I2C bus (GPIO32 / 33, `TOF_BUS 0`), no second bus needed. Channels as wired here: 0 = rear left, 1 = rear right, 2 = front right, 3 = front left (checked by covering each sensor). On the floor the four VL53L0X read 53..75 mm, so the default window 25..110 mm fits. A sensor that answers 0x29 but reads its model ID as 0x00 is hung: switch the rover off completely once (there is no XSHUT line).
- **Display.** After a fresh flash the OLED stays dark: `DisplayModel` is 0 until a descriptor `/display.ini` is on the file system. Upload `tasmota/displaydesc/SSD1306_128x32_display.ini` as `display.ini` (`/ufsu`), then `Restart 1` (the log `DSP: SSD1306_128x32 initialized` shows with `WebLog 4`).
- **Lidar (LD06).** The data line goes to **IO4** of the 10-pin header (checked: 217 packet heads per second there, nothing on IO5; with TX on IO5 the lidar stopped, probably a 12 V switch stage, not measured). The 5.2 V of the Pi socket is enough, PWM may stay open. `ROVER_LIDAR 1`, `LIDAR_RX 4`: 4245 packets ok, 3 CRC errors, 109 ms per turn, heap 76 KB free with the floor sensors.
- **⚠️ The lidar UART took GPIO17.** `serialBegin(rx, -1, ...)` on the classic ESP32 lets the hardware UART take its default tx pin (UART2: GPIO17 = left motor AIN2); with the lidar on, the left wheel did not turn. `common/ld19.tc` therefore has `LID_TX_PIN` (default -1); `rover.tc` sets 13 (SD slot MOSI, unused). Check which pin any new `serialBegin` on this chip claims.
- **Lidar angles.** With a hand in front of the nose the echo shows at 240..299 degrees (centre 270), so ahead = 240..300 and behind = 60..120. Left and right (probably 180 / 0 degrees, clockwise) are assumed, not checked. The first picture showed 31..93 mm echoes: loose test cables in the beam.
- **Obstacle brake (`RoverObst [0|1|<mm>]`).** Forward driving is refused when the nearest lidar echo ahead is closer than `OBST_MM` (250 mm, released at +50 mm), reverse when it is closer behind; turning stays free. A silent lidar holds the motors (`RoverObst 0` for a rover without lidar). Status `why` 8 / 9 / 10, field `ob` in the `/rover_cmd` answer, text on the joystick page and the OLED. Test at 40 % (about 0.17 m/s): the brake locked at 250 mm and the rover stood at 236 mm, about 3 cm of rolling after the trigger. Not tested at higher speed.
- **Driving direction.** `d > 0` goes toward the **nose**: the end with the floor sensors FL / FR and the lidar's 270 degrees. The motors took a positive value to the tail (the first brake test drove away from the obstacle), so `rover_apply()` negates `d` when it computes the wheel values; turning is unchanged (joystick right = clockwise). The encoder ticks therefore count *negative* for forward; the odometry is in the tail's frame.
- **Staircase guard on a real edge** (rover on a table, helper at the edge, 0.3 s steps at 40 %): in front, FL / FR read 220 / 253 mm over the edge, `CLIFF AHEAD`, further forward commands answered 0 / 0, about 7 cm of rolling in the step that found the edge. Behind, RL / RR read 933 / 981 mm, `CLIFF BEHIND`, further reverse commands refused, about 3 cm of rolling after the first reading over the edge. How close the wheels then stood to the edge was judged by eye only. A short false flag (for example a hand near a sensor) can show up in `cl` for one reading; it only blocks that direction for that moment.
- **Random wander (`RoverWander <seconds>`, `RoverWander 0` stops; up to 600 s).** The rover drives straight at 40 % and, when the lidar brake or the staircase guard stops it, backs off a cliff (0.9 s) or turns on the spot in a random direction for 0.5..1.5 s until the way ahead is free again (`WAN_FREE_MM`, 400 mm). It ends by itself when the time is up, when the obstacle brake or the staircase guard is switched off, when a sensor or lidar fault lasts longer than 1.5 s (a single bad reading does not end it), when any echo is closer than 110 mm (turning would hit it), when a turn finds no free way for 8 s, or when a drive command arrives (joystick); a status poll with `d=0 t=0` leaves it alone. It does not start without the lidar. Only the lidar plane and the four floor sensors see: nothing low on the floor, nothing above the lidar, no glass. First runs of 20 s in a furnished room on 17.11.2026: five turns, one straight stretch, no collision in the log; with a free-way threshold of 600 mm it only turned. The thresholds are estimates. Stay close and keep it short.
- **Low obstacles and stalls (17.11.2026).** The lidar does not see anything below its plane, and a stall guard that stays off helps nothing: `ENC_ON 1` is now the default in `rover.tc` (a side commanded but without a tick for 800 ms stops the rover, `RoverEnc 0` switches it off; it releases when the stick is back in the middle). The floor window starts at 40 mm (`CLIFF_MIN`, was 25): on this rover the floor reads 51..73 mm, a book of 2..3 cm in front of the nose gave FL 38 / FR 32 mm and the guard stopped the rover about 4 cm later, with the lidar brake off for the test. Holding the rover tilted lowers the readings too (RR 33..37 mm) and shows as a cliff: hold a wheel with the rover flat. Not checked: obstacles under about 1.5 cm, carpet.
- **Joystick page (`rover.html`).** The page sent a drive command every 100 ms with a timer, but the rover needs about 180 ms per answer: the requests piled up in the browser and the old commands ran on after the stick was released. Now ONE request is in flight (the next starts when the answer arrives), and on release or STOP the page aborts the running request and sends `d=0` at once and again after 250 ms. Checked in a browser (17 requests in 3 s, never more than one open besides the stop), not with a finger on a device.


## ESP-NOW link, lidar page, camera, sensor recovery (18.11.2026)

- **ESP-NOW link (`ROVER_ESPNOW 1` in `rover.tc`).** A cam module (the DFRobot S3 camera on the rover) sends the same text commands as the UDP link, `d=40 t=0 m=40 p=0`, by MAC address, without a router. The rover answers every command with `v=<mV> l=<motor L> r=<motor R> w=<why> c=<cliff mask> a=<ms since the last command>`. ESP-NOW needs ABI 34 or newer (TinyC 1.6.80 is 35; a 1.6.76 / ABI 32 device has no `espnow*` calls). It rides on the WiFi channel of the access point (both ends on the same channel, here 11). The rover's page shows `ESP-NOW commands / last RSSI / age`.
  - ⚠️ **Open by default.** Without `ESPNOW_ENC` anyone in radio range can drive the rover (the log says `ESP-NOW link is OPEN`). With `ESPNOW_ENC 1`, a 16 character `ESPNOW_KEY` and the sender's MAC in `ESPNOW_PEER`, only that peer is accepted (CCMP; the sender sets the same key and lists the rover). **The encrypted mode was not tested.**
  - **Sender test:** `ENTEST 1` in a small TinyC program on the cam (`d=0 t=0` at 5 Hz, motors stay off), measured against the rover on 18.11.2026 with the cam 1 m away, RSSI about −30 dBm: **250 of 250 packets acknowledged by the radio and handled by the rover, none lost** (checked with sequence numbers `s=` that the rover logs as `ENR gap` when one is missing).
  - ⚠️ **"Lost answers" were late answers.** The first test showed only 75 % answered. The answers were all there, 100..200 ms late: `en_poll()` ran in `Every50ms()`, and a callback gets the VM only while `TaskLoop()` sleeps in `delay()`, but the TaskLoop reads the I2C sensors all the time. Now `en_poll()` runs in `TaskLoop()` between the slow I2C steps (as `lid_poll()` does); while the rover is parked the callback keeps polling. Result: 227 of 250 answers within 150 ms (91 %), mean round trip 59 ms, none lost. The rest is the slow sensor cycle (two floor sensors were silent during that measurement, each one a bus timeout); with all sensors present it should be faster. Plan the dead-man (`WD_MS` 300 ms) for 10 Hz commands, not 5 Hz.
  - `espnowSend` waits up to 100 ms for the radio's ACK; the rover answers from the TaskLoop, so a lost ACK costs one loop. The receive ring holds 8 packets.
- **Lidar page `/lidar`.** The picture of what the lidar sees, as a web page of the rover itself: `http://<ip>/lidar` (also linked from the rover's main page). The page is the file **`/lidar.html`** (`examples/lidar.html`, upload it like `rover.html`: `curl -X POST "http://<ip>/ufsu?fsz=<size>" -F "ufsu=@lidar.html;filename=lidar.html"`); the data come from `/lidar_data` (JSON, 360 distances in lidar degrees plus the brake state), a new answer every 150 ms. The nose of the rover is **up** (the page turns the lidar's 270 degrees to the top); the orange wedges are the brake sectors ahead and behind with the stop distance, the red ring is the nearest echo. Controls: range (auto up to 4 m, 0.5 … 12 m), dots or filled outline, mirror (the left / right direction of the lidar is still assumed, not checked), freeze; touch or move over the picture to read angle and distance. Checked in the browser against the live rover; the direction (mirror) was not checked with an object at the side.
- **Camera upside down (`CAM_ROTATE180` in `webcam_tinyc.tc`).** The DFRobot camera sits upside down on the rover. `#define CAM_ROTATE180 1` turns the picture by 180 degrees (vertical flip and mirror) on the OV3660; the default 0 changes nothing. Checked: the picture is upright.
- **Floor sensors that stay silent (VL53L0X).** A sensor that is stopped in the middle of its init or a measurement (a program reload at the wrong moment) kept the register page 1 and no longer answered with its ID, which looked like "floor sensor N not found" until the power was cycled. `vl_start()` (`common/vl53l0x_mux.tc`) now writes register `0xFF = 0` first and, if the ID is still wrong, does a soft reset (`0xBF`) and reads again (log `recovered by soft reset`). Checked with five program reloads / aborts in a row: all four sensors read again each time. The log line `recovered by soft reset` was never seen, so the page write alone seems to be enough; the invalid state itself could not be produced on purpose, the soft reset is therefore untested.

## Steered by the Mac Studio (RoverPilot, 18.11.2026)

A program on the Mac Studio (`/Volumes/vp_dev/RoverPilot`, README there) drives the rover with a lidar planner and a local vision model. What it needed from `rover.tc`:

- **`ROVER_LINK 1` is now the default.** The UDP link (port 5006, commands `d= t= m= s=`, answer = JSON status) answers to the sender's IP on the SAME port number. New: `s=<mm/s>` = RoverSpeed in every command (a restarted rover is back in speed control at once); the status has `ob` (obstacle mask), `sp` (speed setting), `ox`, `oy` (odometry). While somebody steers, the rover also sends the lidar scan four times a second as three datagrams `S<seq>,<first degree>,<2 chars per degree>` (16 mm steps) instead of making the controller poll `/lidar_data`: that handler keeps the VM about 300 ms per call, which starved the floor sensors (the cliff guard saw them stale and refused to drive).
- **TaskLoop vs. callbacks (measured):** with the lidar streaming, TaskLoop is busy about 97 % of the time and a callback gets the VM only in the gaps: `Every50ms` ran 2..3 times a second instead of 20. The dead-man, the wheel encoders / stall guard and the speed controller now run from `fast_tasks()`, which TaskLoop calls between its steps (about 12 per second); the link polls (UDP, ESP-NOW) are called there too. The rover's main page shows "Loop rates".
- **Floor sensors in parallel:** `vl_service(ch)` in `common/vl53l0x_mux.tc` starts / collects a measurement without waiting. `tof_step()` services all four sensors per pass (37 ms for all four; before: 43 ms for ONE, so each sensor was refreshed only every 600..800 ms and the cliff guard reacted after about 12 cm). `CLIFF_STALE_MS` is 1000 (was 600).
- **Speed controller start:** a standing wheel began at the minimum PWM (23) and needed more than the stall guard's 800 ms to reach breakaway (about 45..60 PWM for a spin on tiles). The PI now ramps +12 PWM per window until the wheel moves, and `ENC_STALL_MS` is 1500. Commands below 24 mm/s make a wheel drive in bursts without ticks: the controller side keeps every wheel stopped or at 26 mm/s at least.
- WiFi modem sleep (`Sleep 50`) gave 50 ms pings and ARP trouble: `Sleep 0` on the rover and the camera.
- Tested: UDP round trip about 0..55 ms (100 of 100 answered), the speed-mode spin and straight drive with the new ramp (no stall), explore runs of 45 s in a cluttered room. The lidar did not see a treadmill frame and a chair star base (below the beam): the pilot's progress watchdog and camera check caught those, the rover's own guards did not.
