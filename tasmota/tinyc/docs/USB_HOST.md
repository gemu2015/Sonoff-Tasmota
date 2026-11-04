# USB host on the ESP32-S3 / S2: flash drive, audio stick, FTDI

Driver `xdrv_99_usb_host.ino`. The ESP32 is the **host**: a USB flash drive becomes the file system `/usb`,
a USB audio stick plays WAV files, an FTDI adapter (VarioLab) becomes a serial port. One host, several devices
at once behind a hub. Deutsch: [USB_HOST_DE.md](USB_HOST_DE.md).

## Which chips

Only chips with a USB-OTG peripheral have a host stack: **ESP32-S2 and S3** (released here) and **ESP32-P4**
(has the stack, not ported yet). ESP32, C2/C3/C5/C6/C61/H2 have no host; on those the driver compiles to
nothing (a `#warning`). The board must **supply VBUS** at the socket, and the plug must fit (the GOOUUU S3-CAM
does not take a normal USB-A plug).

The TinyC test release image for the ESP32-S3 (`tinyc32s3`) is built with all of it, including hub support (+44 kB flash).
For your own build:

## Build switches (`user_config_override.h` or `-D` in the environment)

| Switch | Effect |
|---|---|
| `USE_USB_HOST` | host, hub, device table, commands (required) |
| `USE_USB_HOST_MSC` | USB flash drive as FAT file system `/usb` |
| `USE_USB_HOST_MSC_AUTOMOUNT` | mount a drive about 2.5 s after it appears (otherwise `UsbHostMount 1`) |
| `USE_USB_HOST_AUDIO` | USB audio class 1.0 playback (16 bit PCM, 44.1/48 kHz) |
| `USE_USB_HOST_SERIAL` | FTDI FT232 serial port (`usbInit()` … in TinyC use this driver) |
| `USE_USB_HOST_AUTOSTART` | start the host at boot (otherwise `UsbHost 1`) |

**Hubs** need `CONFIG_USB_HOST_HUBS_SUPPORTED=y` in the SDK. With PlatformIO that is
`custom_sdkconfig = CONFIG_USB_HOST_HUBS_SUPPORTED=y` in the environment — it rebuilds the shared Arduino
libraries once (about 8 minutes) and rewrites `sdkconfig.defaults` / deletes `CMakeLists.txt`: restore both with
`git checkout` before committing. Without a hub option only **one** device works at the socket.

## Commands

| Command | |
|---|---|
| `UsbHost [0\|1]` | stop / start the host; without argument the state and the attached devices |
| `UsbHostFifo rx,nptx,ptx` | FIFO lines of the host (default 80,40,80; **the sum must stay ≤ 200**, more and no device enumerates, silently) |
| `UsbHostMsc` | list the devices; with a drive attached a read test |
| `UsbHostMount [0\|1]` | unmount / mount the drives (mounting runs in its own task); two drives at once: the first is `/usb`, the second `/usb2`; the answer lists both with `Dev` (VID:PID) |
| `UsbHostMicRoute [0\|1]` | the audio plugin records from the stick's microphone instead of the I2S one |
| `UsbHostMic [rate]` | recording from the stick's microphone: open / `0` close / state and level |
| `UsbHostTone`, `UsbHostPlay <path>`, `UsbHostStress <path>` | audio: 440 Hz test tone, play a 16 bit PCM WAV (any rate, resampled), loop test |
| `UsbHostSerial <baud>`, `UsbHostSerialRead [n]`, `UsbHostSerialWrite <hex>`, `UsbHostBridge <port>` | FTDI port, TCP bridge |

Log lines start with `USB:` (web console / `/cs`).

## The drive as file system

* FAT12/16/32 with 512-byte blocks. **No exFAT, no NTFS** (the log says so).
* Mounted at `/usb`; `usbfsp` is the `FS` pointer (nullptr when nothing is mounted).
* **Tasmota file manager**: `/ufsd?dir=3` (link "USB" next to Flash and SD).
* **TinyC**: path prefix `/usb/` like `/ffs/` (flash) and `/sdfs/` (SD): `fileOpen("/usb/log.csv", 2)`. A second drive (stick and card reader at the same time) is `/usb2/`, in the file manager `/ufsd?dir=4`. A drive gets the first free mount point, so `/usb` is the one that was mounted first, not a fixed device: check `Dev` in the `UsbHostMount` answer.
* Two drives need the SDK options `CONFIG_FATFS_VOLUME_COUNT=4` (SD, `/usb`, `/usb2`) and `CONFIG_VFS_MAX_COUNT=12` (with the default 2 drives the second one fails with "no free drive number" or a VFS error). **`platformio_override.ini` is in `.gitignore`**, so these lines are not in the repository: whoever builds the env must add them to its `custom_sdkconfig` (next to `CONFIG_USB_HOST_HUBS_SUPPORTED=y`), one indented line each; the first build then rebuilds the arduino libs of all S3 envs (about 8 minutes, afterwards `git checkout CMakeLists.txt sdkconfig.defaults`). Without them everything else works, only a second drive cannot be mounted. The S3 has only 8 USB host channels: hub, stick and card reader fit, an audio stick on top does not (the claim of the reader then fails).
  Without a drive `fileOpen` fails with -1.
* Port 83 upload: `POST /ufs/usb/<path>`; new command **`UfsMkdir <prefix><path>`** creates a folder and its parents
  (prefix `/ffs/`, `/sdfs/`, `/usb/`, `/usb2/`).
* Speed: raw reads about 1.1 MB/s; through the web file manager about 105 kB/s write, 167 kB/s read.

Things the stick insists on (all handled in the driver; keep them when you touch it):

* After release and claim the host starts with data toggle 0: send the **Bulk-Only reset recovery** (class request
  `0x21/0xFF`, `CLEAR_FEATURE(HALT)` on both endpoints) before the first command, or the first command hangs and the
  watchdog resets the device.
* The first command after plug-in reports "medium changed": answer with REQUEST SENSE.
* A drive that is attached while a hub is still enumerating the other devices may answer every command with an
  error for good. The automount therefore waits 2.5 s, and the mount tries three times (release, wait 1 s, claim).
* The transfer callbacks run in the USB client task: **never mount or wait for a transfer from that task**
  (own task, only one mount at a time).

## TinyC file manager (`examples/filemanager.tc` + `filemanager.html`)

Two panes (Flash / SD / USB), drag a row to the other pane to copy (Shift = move), drop files from the computer to
upload, folders recursive. Put `filemanager.html` on the flash as `/filemanager.html`, start the script **in any slot**
(since 03.11.2026 `webOn` is answered by the slot that registered it; on 1.6.76 and older only slot 0 works) and press the
**File Manager** button on the main page (or open `http://<device>/fm`). The script copies in 64 kB slices
(`/fm/op?c=cp`), everything else uses Tasmota's `/ufsd`, port 83, `UfsMkdir` and `UfsRename`.

Buttons: **Rename** (one entry, same file system, `UfsRename /<fs>/old,/<fs>/new`), **Edit** (one text file up to 100 kB in
an editor window, line endings kept, saved through port 83), **.pvs → CSV** (downloads a persist file as
`name,index,int,float,char`, one row per value; the file only holds 32-bit words, so integer and float views are shown
side by side), **Delete**. ⚠️ `TinyCRun /x.tcb` and `webSendFile` take a file from the SD card first when one is present:
an old copy there hides the new one on the flash.

## Audio

Class 1.0 playback by isochronous OUT (6 transfers of 8 × 1 ms), sampling frequency set with `SET_CUR` on the
endpoint, linear resampling, half level. WAV only (16 bit PCM); MP3 needs a decoder and is not built. Needs the
FIFO split above (the default split does not take a 200-byte isochronous packet).

**From TinyC** the stick is the output of `i2sBegin` / `i2sWrite` / `i2sStop`: call `i2sBegin(-1, -1, -1, -2, rate)`
(data pin **-2** = USB audio stick) and write mono 16 bit samples as for an I2S amplifier. The driver converts `rate` to the
rate of the stick, `i2sWrite` blocks while its 8192-sample ring is full (that paces the script) and `i2sStop` lets the ring
play out first. The host is started if needed. `examples/wav_player.tc` does it with `#define CODEC_USB` (comment out
`CODEC_NONE`): `WAVplay /ffs/x.wav`, `WAVtone 440`, any WAV rate plays at the right speed.

**From the audio plugin** (`I2SAUDIO`: MP3, web radio, text to speech, say-time, `I2SPlay`): `UsbHostRoute 1` sends the plugin's
I2S output to the stick **while it plays**; `UsbHostRoute 0` sends it back to the I2S pins. The plugin binary is unchanged: its
I2S calls all go through one firmware function (`tmod_i2s` in `xdrv_123_plugins.ino`), which hands the samples (mono or stereo,
any rate) to the same stream as above. The stick is claimed at "enable" and given back at "disable", so it is free between two
plays. If the stick is not there or busy the plugin simply plays on I2S as before. `UsbHostLevel 1..100` sets the volume of the
stick for the streams (default 50; the plugin has its own volume on top). `UsbHostRoute` without argument shows route, level,
whether a stream is open and the underrun count. The plugin's microphone side stays on I2S.
Setup of the plugin itself, for a board without an I2S amplifier: `chkpt a2` (128 kB plugin partition, **formats the flash file
system**), upload `I2SAUDIO_32.bin` on the plugin page (`/modu`), put an `I2SAUDIO.cfg` on the flash with codec 0 and free pins
(otherwise `iniz` says "Codec error"), then `iniz <slot>` and tick "Autostart plugins at boot". Tested with an MP3 on .190.

**Recording from the audio plugin with the stick's microphone** (`I2Srec <file>`, the MP3 stream on port 81): `UsbHostMicRoute 1` makes the
plugin read the stick instead of the I2S microphone, `0` goes back. The plugin binary is unchanged. ⚠️ `I2SAUDIO.cfg` needs a valid `DIN`
pin (any free GPIO, nothing has to be connected there): without one the plugin does not read at all. The stick opens when the recording starts
and is given back 2 s after the last read. If it cannot be opened (a stick is mounted and the 8 host channels are used up: `claim failed
0x106` in the log) the plugin keeps using the I2S microphone, and the open is tried again only every 3 s. Recording and `UsbHostRoute 1`
playback together need the stick unmounted (`UsbHostMount 0`). A mono stick is recorded as two equal channels. Tested on .190 with a 1 kHz
tone from a Mac (the MP3 shows a clean 1 kHz line, other bins at the noise floor).

**Recording from the stick's microphone input** (isochronous IN, UAC1, 16 bit mono): `UsbHostMic <rate>` opens it (the ring fills, 0 closes),
`UsbHostMic` alone shows `Rms`/`Peak` of the newest samples, the packet counters and `NonEmpty` (packets that carried data). From TinyC the
existing microphone calls take the stick when the data pin is **-2**: `i2sMicBegin(-1, -1, -1, -2, 16000)`, then `i2sMicRead(buf, n)` and
`i2sMicLevel()` as for an I2S microphone, `i2sMicStop()` at the end. The driver converts the stick's 48 kHz to the requested rate by averaging
(no aliasing for 16 kHz). Playback and recording together fit in the 8 host channels only while no stick is mounted (see below).
⚠️ Found on a C-Media 0D8C:000C: after a plain claim of the recording interface every packet is empty (zero length, no error) however mute,
volume, selector and sampling frequency are set. Only an explicit `SET_INTERFACE` to alternate 0 and back to the streaming alternate starts
the engine, and it then stays started until the stick loses power. The driver always does this; it also sets the sampling frequency and
switches the microphone's mute off and its volume to the middle of the stick's range (feature unit of the recording path).

⚠️ **The OTG hardware has only 8 host channels.** Every attached device uses one (its control pipe), the hub's interrupt
endpoint one, and every claimed endpoint one: a mounted stick takes two, an audio stream one. Hub + FTDI + stick + audio stick
+ card reader are nine, so the audio claim then fails with `0x106` (`ESP_ERR_NOT_SUPPORTED`) while the stick is mounted
(it works after `UsbHostMount 0`). `UsbHostMsc` also claims the stick for its read test, so do not call it as a status query before playing. Unplug a device you do not need (the card reader) or mount the stick only when needed.

## Not done / not tested

* ESP32-P4 (probably High-Speed, which would make drives much faster; needs `peripheral_map` and its own FIFO values).
* Pulling the drive while it is mounted (the unmount path exists, not exercised on hardware).
* FTDI data from a running VarioLab through the hub, the TCP bridge, writing through the SD card reader.
