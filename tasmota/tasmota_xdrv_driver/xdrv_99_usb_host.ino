/*
  xdrv_99_usb_host.ino - USB host for the ESP32-S3 / S2: device table (with hubs), USB mass storage, USB audio playback

  One driver for everything that talks to the USB OTG port as a HOST. Switches (user_config_override.h or build flags):

    USE_USB_HOST            host stack, hot-plug, device table, commands. Needed for everything below.
    USE_USB_HOST_MSC        USB mass storage (flash drive, card reader), SCSI bulk-only. With USE_UFILESYS the stick is mounted as FAT
                            file system "/usb" (FAT12/16/32 and LFN, no exFAT, 512 byte sectors) and shows in the Tasmota file manager
                            (xdrv_50: /ufsd?dir=3). For TinyC the prefix "/usb" in tc_file_path is still to be added (xdrv_124)
    USE_USB_HOST_MSC_AUTOMOUNT  mount a stick as soon as it is attached (otherwise: command UsbHostMount 1)
    USE_USB_HOST_AUDIO      USB audio class 1.0 playback (headset dongle): 440 Hz tone and 16 bit PCM WAV files
    USE_USB_HOST_SERIAL     FTDI USB-serial adapter (FT232): serial link, TCP bridge. Also serves the TinyC functions
                            usbInit/usbOpen/usbRead/usbWrite (USE_TINYC_USBSERIAL), see xdrv_124_tinyc_usb.h
    USE_USB_HOST_AUTOSTART  start the host at boot (otherwise: command UsbHost 1)

  Needs a board with a USB OTG socket that supplies 5 V (VBUS); the console must not use that socket.

  Chips: the USB host stack exists in the SDK only for the ESP32-S2, S3 and P4 (checked in the arduino libs: libusb.a and
  CONFIG_SOC_USB_OTG_SUPPORTED). The ESP32, C2, C3, C5, C6, C61 and H2 have no USB host (C3/C6 only USB Serial/JTAG).
  This driver is released for S2 and S3; every other chip is left out with a #warning. PLANNED: P4 (two OTG peripherals, probably
  High-Speed): needs usb_host_config_t.peripheral_map, its own FIFO lines (the 80/40/80 below are for the Full-Speed hardware) and
  the hub option checked, then it joins USBH_TARGET_OK. Try it on .118 / .164.

  Several devices behind a hub work only if the SDK has the hub driver: CONFIG_USB_HOST_HUBS_SUPPORTED=y (custom_sdkconfig,
  rebuilds the arduino libs). Without it the hub shows up as one device and what hangs behind it stays invisible.

  There is only ONE USB host. With USE_TINYC_USBSERIAL (TinyC usbInit) set, USE_USB_HOST_SERIAL must be set too: the TinyC
  functions then work on the host of this driver (the build stops with #error otherwise).

  ---------------------------------------------------------------------------------------------------------------------
  Commands (the answer is the state: running, busy, the devices; results go to the log, lines "USB: ...")
    UsbHost [0|1]          0 stop, 1 start (with the FIFO split of UsbHostFifo), no argument: state
    UsbHostFifo rx,nptx,ptx  FIFO lines of the host and restart. The host has about 200 lines in all (rx+nptx+ptx <= 200);
                           with 256 the devices do not even enumerate. Default 80,40,80 (room for 200 byte audio packets)
    UsbHostMsc             (MSC)   INQUIRY, capacity, boot sector, read speed over 2 MB; read only (not while mounted)
    UsbHostMount [0|1]     (MSC)   1 mount the first stick as file system /usb, 0 unmount; answer: state and size
    UsbHostTone            (AUDIO) 2 s of 440 Hz at low level
    UsbHostPlay <path>     (AUDIO) 16 bit PCM WAV (1 or 2 channels, any rate) from flash or SD, half level
    UsbHostStress <path>   (MSC+AUDIO) play the WAV and read the stick at the same time (both on one bus)
    UsbHostSerial <baud>   (SERIAL) open the FTDI at this baud rate (8N1), 0 closes; answer: state, VID/PID, counters
    UsbHostSerialRead [n]  (SERIAL) up to n received bytes as hex (default 32, at most 100)
    UsbHostSerialWrite <hex> (SERIAL) send these bytes
    UsbHostBridge <port>   (SERIAL) TCP server on this port, one client at a time, bytes go both ways; 0 stops

  Findings that are easy to lose (02.11.2026, measured with a Verbatim 1 GB stick and a C-Media headset dongle):
  * Flash drive 1141 KB/s read, also behind a hub; with a WAV playing at the same time 1023 KB/s, no audio packet lost.
  * Audio claim fails with ESP_FAIL on the default FIFO split (a 200 byte isochronous packet does not fit), hence the
    custom split. Over 200 lines the device does not enumerate at all - silently.
  * The first SCSI command after plugging in fails with "medium changed" (sense key 6): REQUEST SENSE clears it.
  * The sampling frequency is set with the class request SET_CUR on the endpoint, otherwise the stick runs at its default.
  * FTDI: every IN packet starts with TWO status bytes (modem and line status); they are stripped. IN buffers stay 64 bytes,
    throughput comes from 4 transfers in flight. The ring is 16 KB: 2 KB were only 89 ms and lost bytes at 230400 baud.
    Transfer callbacks must not compute or log: they only store and hand on (they run in the USB task).
*/

#ifdef USE_USB_HOST
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)

#if defined(USE_TINYC_USBSERIAL) && !defined(USE_USB_HOST_SERIAL)
#error "USE_USB_HOST and USE_TINYC_USBSERIAL both want the one USB host. Define USE_USB_HOST_SERIAL as well (the TinyC functions then use this driver), or unflag USE_TINYC_USBSERIAL."
#endif

#define XDRV_99     99

#include "usb/usb_host.h"
#include "freertos/semphr.h"
#include <math.h>
#if defined(USE_USB_HOST_MSC) && defined(USE_UFILESYS)
#define USBH_MSC_FS
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
#include "vfs_api.h"
extern FS *usbfsp;                  // defined in xdrv_50 (compiled before this file); nullptr while no stick is mounted
// file system object on the VFS mount point "/usb", same kind as FFat/SD
class UsbHostFS : public fs::FS {
public:
  UsbHostFS() : FS(FSImplPtr(new VFSImpl())) {}
  void mount_point(const char *mp) { _impl->mountpoint(mp); }
};
#endif

#if defined(USE_USB_HOST_MSC) || defined(USE_USB_HOST_AUDIO) || defined(USE_USB_HOST_SERIAL)
#define USBH_NEEDS_TRANSFERS
#endif

#define USBH_LINES   40
#define USBH_LINE_SZ 120
#define USBH_MAX_DEV 4

struct UsbHostS {
  // --- log lines, queued by the USB tasks and printed from the main loop (AddLog belongs to the main loop) ---
  inline static char lines[USBH_LINES][USBH_LINE_SZ];
  inline static volatile uint32_t head = 0, tail = 0;
  inline static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  // --- host state ---
  inline static bool running = false;
  inline static usb_host_client_handle_t client = nullptr;
  inline static uint16_t vid = 0, pid = 0;
  inline static TaskHandle_t t_lib = nullptr, t_cli = nullptr;
  inline static volatile bool test_busy = false;
#ifdef USE_USB_HOST_MSC
  inline static usb_device_handle_t mdev = nullptr;   // the mass storage test (and the mounted file system) works on this one
  inline static SemaphoreHandle_t msc_mtx = nullptr;  // one SCSI command (CBW, data, CSW) at a time
  inline static volatile bool msc_ready = false;      // block device usable (diskio)
  inline static bool msc_mounted = false;
  inline static uint32_t msc_blocks = 0;
#endif
#ifdef USE_USB_HOST_AUDIO
  inline static usb_device_handle_t dev = nullptr;    // the audio test works on this one (both tests may run at the same time)
#endif

  // --- what the descriptors of the device being looked at say ---
#ifdef USE_USB_HOST_MSC
  inline static int msc_if = -1, msc_in = 0, msc_out = 0, msc_mps = 64;
#endif
#ifdef USE_USB_HOST_AUDIO
  inline static int as_if = -1, as_alt = 0, as_ep = 0, as_mps = 0, as_rate = 0, as_ch = 0, as_uac = 0;
#endif
#ifdef USE_USB_HOST_SERIAL
  inline static int ft_if = -1, ft_in = 0, ft_out = 0;
#endif
  inline static uint8_t cfgcopy[512];
  inline static int cfglen = 0;

  // --- all devices that are attached (several behind a hub) ---
  struct Slot {
    usb_device_handle_t h; uint16_t vid, pid;
#ifdef USE_USB_HOST_MSC
    int msc_if, msc_in, msc_out, msc_mps;
#endif
#ifdef USE_USB_HOST_AUDIO
    int as_if, as_alt, as_ep, as_mps, as_rate, as_ch;
#endif
#ifdef USE_USB_HOST_SERIAL
    int ft_if, ft_in, ft_out;
#endif
  };
  inline static Slot slots[USBH_MAX_DEV];
  inline static int nslots = 0;
  inline static volatile uint8_t pend[8];
  inline static volatile int pend_h = 0, pend_t = 0;
  inline static usb_device_handle_t gone_q[8];
  inline static volatile int gone_h = 0, gone_t = 0;

#ifdef USE_USB_HOST_SERIAL
  inline static usb_device_handle_t sdev = nullptr;   // the FTDI that is open
  inline static uint16_t s_vid = 0, s_pid = 0;
#endif

  static void say(const char *fmt, ...) {
    char b[USBH_LINE_SZ];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    portENTER_CRITICAL(&mux);
    strlcpy(lines[head % USBH_LINES], b, USBH_LINE_SZ);
    head++;
    portEXIT_CRITICAL(&mux);
  }

  static void drain(void) {
    while (tail != head) {
      char b[USBH_LINE_SZ];
      portENTER_CRITICAL(&mux);
      strlcpy(b, lines[tail % USBH_LINES], USBH_LINE_SZ);
      tail++;
      portEXIT_CRITICAL(&mux);
      AddLog(LOG_LEVEL_INFO, PSTR("USB: %s"), b);
    }
  }

  // UTF-16 string descriptor to plain ASCII (anything else becomes '?')
  static void ustr(const usb_str_desc_t *s, char *out, size_t n) {
    out[0] = 0;
    if (!s || s->bLength < 2) { return; }
    size_t k = 0;
    for (int i = 0; (i < (s->bLength - 2) / 2) && (k < n - 1); i++) {
      uint16_t c = s->wData[i];
      out[k++] = ((c >= 32) && (c < 127) && ('"' != c) && ('\\' != c)) ? (char)c : '?';
    }
    out[k] = 0;
  }

  // ---------------- descriptor walk ----------------
  static void parse_config(void) {
#ifdef USE_USB_HOST_MSC
    msc_if = -1;
#endif
#ifdef USE_USB_HOST_AUDIO
    as_if = -1; as_uac = 0;
    int want_ep_for_as = 0;
#endif
#ifdef USE_USB_HOST_SERIAL
    ft_if = -1; ft_in = 0; ft_out = 0;
    bool ft_dev = (0x0403 == vid);                          // FTDI: the first interface with a bulk pair is the serial port
#endif
    int cur_if = -1, cur_alt = 0, cur_cls = 0, cur_sub = 0, cur_pro = 0;
    for (int i = 0; i + 2 <= cfglen; ) {
      int l = cfgcopy[i];
      if (l < 2 || i + l > cfglen) { break; }
      const uint8_t *p = cfgcopy + i;
      uint8_t type = p[1];
      if (0x04 == type && l >= 9) {                       // interface
        cur_if = p[2]; cur_alt = p[3]; cur_cls = p[5]; cur_sub = p[6]; cur_pro = p[7];
        say("  if %d alt %d class %02X/%02X/%02X eps %d", cur_if, cur_alt, cur_cls, cur_sub, cur_pro, p[4]);
#ifdef USE_USB_HOST_AUDIO
        want_ep_for_as = 0;
#endif
#ifdef USE_USB_HOST_MSC
        if (8 == cur_cls && 6 == cur_sub && 0x50 == cur_pro && msc_if < 0) {   // mass storage, SCSI, bulk-only
          msc_if = cur_if; msc_in = 0; msc_out = 0;
        }
#endif
#ifdef USE_USB_HOST_SERIAL
        if (ft_dev && ft_if < 0) { ft_if = cur_if; }
#endif
      } else if (0x05 == type && l >= 7) {                // endpoint
        int ad = p[2], at = p[3] & 3, mp = p[4] | (p[5] << 8);
        say("    ep %02X type %d mps %d interval %d", ad, at, mp, p[6]);
#ifdef USE_USB_HOST_MSC
        if (msc_if == cur_if && 8 == cur_cls && 2 == at) {
          if (ad & 0x80) { msc_in = ad; msc_mps = mp; } else { msc_out = ad; }
        }
#endif
#ifdef USE_USB_HOST_AUDIO
        if (want_ep_for_as && 1 == at && !(ad & 0x80) && as_ep == 0) { as_ep = ad; as_mps = mp & 0x7FF; }
#endif
#ifdef USE_USB_HOST_SERIAL
        if (ft_dev && cur_if == ft_if && 2 == at) { if (ad & 0x80) { ft_in = ad; } else { ft_out = ad; } }
#endif
#ifdef USE_USB_HOST_AUDIO
      } else if (0x24 == type) {                          // class specific
        if (1 == cur_cls && 1 == cur_sub && l >= 5 && 1 == p[2]) {   // audio control header
          as_uac = (p[4] >= 2) ? 2 : 1;
          say("  audio control: bcdADC %x.%02x", p[4], p[3]);
        } else if (1 == cur_cls && 2 == cur_sub && 2 == p[2] && l >= 8 && as_uac != 2) {   // UAC1 format type I
          int ch = p[4], bits = p[6], nf = p[7];
          char fb[60] = ""; int rate = 0;
          if (0 == nf && l >= 14) {
            int lo = p[8] | (p[9] << 8) | (p[10] << 16), hi = p[11] | (p[12] << 8) | (p[13] << 16);
            snprintf(fb, sizeof(fb), "%d..%d continuous", lo, hi);
            rate = (lo <= 48000 && hi >= 48000) ? 48000 : lo;
          } else {
            size_t k = 0;
            for (int f = 0; f < nf && 8 + 3 * f + 2 < l && k < sizeof(fb) - 8; f++) {
              int r = p[8 + 3 * f] | (p[9 + 3 * f] << 8) | (p[10 + 3 * f] << 16);
              k += snprintf(fb + k, sizeof(fb) - k, "%d ", r);
              if (!rate || 48000 == r) { rate = r; }
            }
          }
          say("    format: %d ch %d bit rates %s", ch, bits, fb);
          if (16 == bits && as_if < 0 && (1 == ch || 2 == ch)) {
            as_if = cur_if; as_alt = cur_alt; as_ch = ch; as_rate = rate; as_ep = 0; as_mps = 0;
            want_ep_for_as = 1;
          }
        }
#endif
      }
      i += l;
    }
#ifdef USE_USB_HOST_AUDIO
    if (as_if >= 0 && as_ep == 0) { as_if = -1; }          // the format had no isochronous OUT endpoint: not a playback format
#endif
#ifdef USE_USB_HOST_SERIAL
    if (!(ft_in && ft_out)) { ft_if = -1; }
#endif
  }

  static void attach(uint8_t addr) {
    if (nslots >= USBH_MAX_DEV) { say("too many devices, address %u ignored", addr); return; }
    usb_device_handle_t h = nullptr;
    if (usb_host_device_open(client, addr, &h) != ESP_OK) { say("open of address %u failed", addr); return; }
    usb_device_info_t info;
    const usb_device_desc_t *dd = nullptr;
    if (usb_host_device_info(h, &info) != ESP_OK || usb_host_get_device_descriptor(h, &dd) != ESP_OK || !dd) {
      say("device info failed"); usb_host_device_close(client, h); return;
    }
    vid = dd->idVendor; pid = dd->idProduct;
    char man[40], prod[40];
    ustr(info.str_desc_manufacturer, man, sizeof(man));
    ustr(info.str_desc_product, prod, sizeof(prod));
    say("device %04X:%04X %s %s addr %u speed %s class %02X bcdUSB %x.%02x", vid, pid, man, prod, addr,
        (USB_SPEED_LOW == info.speed) ? "low" : (USB_SPEED_FULL == info.speed) ? "full" : "high",
        dd->bDeviceClass, dd->bcdUSB >> 8, dd->bcdUSB & 0xFF);
#ifdef USE_USB_HOST_MSC
    msc_if = -1;
#endif
#ifdef USE_USB_HOST_AUDIO
    as_if = -1; as_uac = 0;
#endif
#ifdef USE_USB_HOST_SERIAL
    ft_if = -1;
#endif
    cfglen = 0;
    const usb_config_desc_t *cd = nullptr;
    if (usb_host_get_active_config_descriptor(h, &cd) == ESP_OK && cd) {
      cfglen = (cd->wTotalLength < sizeof(cfgcopy)) ? cd->wTotalLength : sizeof(cfgcopy);
      memcpy(cfgcopy, cd, cfglen);
      parse_config();
    }
    Slot &sl = slots[nslots++];
    sl.h = h; sl.vid = vid; sl.pid = pid;
    char what[60] = "";
#ifdef USE_USB_HOST_MSC
    sl.msc_if = msc_if; sl.msc_in = msc_in; sl.msc_out = msc_out; sl.msc_mps = msc_mps;
    if (msc_if >= 0 && msc_in && msc_out) { strlcat(what, " mass-storage", sizeof(what)); }
#endif
#ifdef USE_USB_HOST_AUDIO
    sl.as_if = as_if; sl.as_alt = as_alt; sl.as_ep = as_ep; sl.as_mps = as_mps; sl.as_rate = as_rate; sl.as_ch = as_ch;
    if (as_if >= 0) { strlcat(what, " audio-playback(UAC1,16bit)", sizeof(what)); }
    if (2 == as_uac) { strlcat(what, " UAC2(not supported)", sizeof(what)); }
#endif
#ifdef USE_USB_HOST_SERIAL
    sl.ft_if = ft_if; sl.ft_in = ft_in; sl.ft_out = ft_out;
    if (ft_if >= 0) { strlcat(what, " ftdi-serial", sizeof(what)); }
#endif
    say("usable as:%s", what[0] ? what : " nothing here");
#if defined(USBH_MSC_FS) && defined(USE_USB_HOST_MSC_AUTOMOUNT)
    if (sl.msc_if >= 0 && sl.msc_in && sl.msc_out && !msc_mounted) {   // mounting waits for transfers that this task serves: do it elsewhere
      xTaskCreatePinnedToCore(automount_task, "usbh_mnt", 6144, nullptr, 3, nullptr, 1);
    }
#endif
  }

  // makes the first attached device that fits the test the current one (kind 2 = mass storage, 3/4 = audio playback)
  static bool select(int kind) {
    for (int i = 0; i < nslots; i++) {
      Slot &sl = slots[i];
#ifdef USE_USB_HOST_MSC
      if (2 == kind) {
        if (!(sl.msc_if >= 0 && sl.msc_in && sl.msc_out)) { continue; }
        mdev = sl.h; msc_if = sl.msc_if; msc_in = sl.msc_in; msc_out = sl.msc_out; msc_mps = sl.msc_mps;
        return true;
      }
#endif
#ifdef USE_USB_HOST_SERIAL
      if (5 == kind) {
        if (sl.ft_if < 0) { continue; }
        sdev = sl.h; ft_if = sl.ft_if; ft_in = sl.ft_in; ft_out = sl.ft_out; s_vid = sl.vid; s_pid = sl.pid;
        return true;
      }
#endif
#ifdef USE_USB_HOST_AUDIO
      if (3 == kind || 4 == kind) {
        if (sl.as_if < 0) { continue; }
        dev = sl.h; as_if = sl.as_if; as_alt = sl.as_alt; as_ep = sl.as_ep; as_mps = sl.as_mps; as_rate = sl.as_rate; as_ch = sl.as_ch;
        return true;
      }
#endif
    }
    return false;
  }

  static void lib_task(void *arg) {
    while (running) {
      uint32_t fl;
      usb_host_lib_handle_events(portMAX_DELAY, &fl);
    }
    t_lib = nullptr;
    vTaskDelete(nullptr);
  }

  static void client_event(const usb_host_client_event_msg_t *m, void *arg) {
    if (USB_HOST_CLIENT_EVENT_NEW_DEV == m->event) { pend[pend_h % 8] = m->new_dev.address; pend_h++; }
    else if (USB_HOST_CLIENT_EVENT_DEV_GONE == m->event) { gone_q[gone_h % 8] = m->dev_gone.dev_hdl; gone_h++; }
  }

  static void client_task(void *arg) {
    uint8_t list[8]; int num = 0;
    if (usb_host_device_addr_list_fill(sizeof(list), list, &num) == ESP_OK) {     // devices that were there before we listened
      for (int i = 0; i < num; i++) { pend[pend_h % 8] = list[i]; pend_h++; }
    }
    while (running) {
      usb_host_client_handle_events(client, pdMS_TO_TICKS(100));
      while (gone_t != gone_h) {
        usb_device_handle_t g = gone_q[gone_t % 8]; gone_t++;
        for (int i = 0; i < nslots; i++) {
          if (slots[i].h != g) { continue; }
          say("device %04X:%04X removed", slots[i].vid, slots[i].pid);
#ifdef USE_USB_HOST_AUDIO
          if (dev == g) { dev = nullptr; }
#endif
#ifdef USE_USB_HOST_MSC
          if (mdev == g) {
            msc_ready = false;                              // pending file system calls fail at once
#ifdef USBH_MSC_FS
            msc_unmount();                                  // releases the interface, must happen before the device is closed
#endif
            mdev = nullptr;
          }
#endif
#ifdef USE_USB_HOST_SERIAL
          if (sdev == g) { ser_run = false; ser_open_f = false; sdev = nullptr; }   // transfers are freed by the next close/open
#endif
          usb_host_device_close(client, g);
          for (int k = i; k + 1 < nslots; k++) { slots[k] = slots[k + 1]; }
          nslots--;
          break;
        }
      }
      while (pend_t != pend_h) { uint8_t a = pend[pend_t % 8]; pend_t++; attach(a); }
    }
    for (int i = 0; i < nslots; i++) { usb_host_device_close(client, slots[i].h); }
    nslots = 0;
#ifdef USE_USB_HOST_AUDIO
    dev = nullptr;
#endif
#ifdef USE_USB_HOST_MSC
    mdev = nullptr;
#endif
#ifdef USE_USB_HOST_SERIAL
    ser_run = false; ser_open_f = false; sdev = nullptr;
#endif
    t_cli = nullptr;
    vTaskDelete(nullptr);
  }

  // FIFO lines of the host (the hardware has about 200 in all). With audio compiled in the split leaves room for 200 byte packets.
  inline static int f_rx = 80, f_nptx = 40, f_ptx = 80;
  static bool start(void) {
    if (running) { return true; }
    usb_host_config_t hk = {};
    hk.skip_phy_setup = false; hk.intr_flags = ESP_INTR_FLAG_LEVEL1;
    if (f_rx > 0 && f_nptx > 0) {
      hk.fifo_settings_custom.rx_fifo_lines = f_rx;
      hk.fifo_settings_custom.nptx_fifo_lines = f_nptx;
      hk.fifo_settings_custom.ptx_fifo_lines = f_ptx;
    }
#ifdef USBH_NEEDS_TRANSFERS
    if (!bsem) { bsem = xSemaphoreCreateBinary(); }
#endif
#ifdef USE_USB_HOST_SERIAL
    if (!ssem) { ssem = xSemaphoreCreateBinary(); }
#endif
#ifdef USE_USB_HOST_MSC
    if (!msc_mtx) { msc_mtx = xSemaphoreCreateMutex(); }
#endif
    esp_err_t e = usb_host_install(&hk);
    if (e != ESP_OK) { say("usb_host_install %s (already installed by another driver?)", esp_err_to_name(e)); return false; }
    const usb_host_client_config_t kk = { .is_synchronous = false, .max_num_event_msg = 5,
                                          .async = { .client_event_callback = client_event, .callback_arg = nullptr } };
    if (usb_host_client_register(&kk, &client) != ESP_OK) { usb_host_uninstall(); say("client_register failed"); return false; }
    running = true;
    xTaskCreatePinnedToCore(lib_task, "usbh_lib", 3072, nullptr, 5, &t_lib, 0);
    xTaskCreatePinnedToCore(client_task, "usbh_cli", 4096, nullptr, 5, &t_cli, 0);
    say("host started (FIFO rx %d, nptx %d, ptx %d lines), waiting for devices (VBUS must be present at the socket)", f_rx, f_nptx, f_ptx);
    return true;
  }

  static void stop(void) {
    if (!running) { return; }
#ifdef USBH_MSC_FS
    msc_unmount();
#endif
#ifdef USE_USB_HOST_SERIAL
    ser_close();
#endif
    running = false;
    vTaskDelay(pdMS_TO_TICKS(400));
    if (client) { usb_host_client_deregister(client); client = nullptr; }
    usb_host_uninstall();
    say("host stopped");
  }

#ifdef USBH_NEEDS_TRANSFERS
  // ---------------- synchronous transfers for the test tasks ----------------
  inline static SemaphoreHandle_t bsem = nullptr;
  static void xfer_done(usb_transfer_t *t) { xSemaphoreGive(bsem); }

  // control request on endpoint 0 of device h; bmrt 0x22 = class request to an endpoint, 0x40 = vendor request to the device
  static int ctrl(usb_device_handle_t h, uint8_t bmrt, uint8_t breq, uint16_t wval, uint16_t widx, const uint8_t *d, uint16_t len) {
    usb_transfer_t *t = nullptr;
    if (usb_host_transfer_alloc(USB_SETUP_PACKET_SIZE + len, 0, &t) != ESP_OK) { return -1; }
    usb_setup_packet_t *sp_ = (usb_setup_packet_t *)t->data_buffer;
    sp_->bmRequestType = bmrt; sp_->bRequest = breq; sp_->wValue = wval; sp_->wIndex = widx; sp_->wLength = len;
    if (len) { memcpy(t->data_buffer + USB_SETUP_PACKET_SIZE, d, len); }
    t->device_handle = h; t->bEndpointAddress = 0; t->num_bytes = USB_SETUP_PACKET_SIZE + len;
    t->callback = xfer_done; t->context = nullptr; t->timeout_ms = 1000;
    xSemaphoreTake(bsem, 0);
    int r = -2;
    if (usb_host_transfer_submit_control(client, t) == ESP_OK) {
      if (xSemaphoreTake(bsem, pdMS_TO_TICKS(2000)) == pdTRUE) { r = (USB_TRANSFER_STATUS_COMPLETED == t->status) ? 0 : (100 + (int)t->status); }
      else { r = -3; }
    }
    if (r != -3) { usb_host_transfer_free(t); }
    return r;
  }
#endif

#ifdef USE_USB_HOST_MSC
  static int bulk(int ep, uint8_t *buf, int len, int *actual, uint32_t tmo) {
    usb_transfer_t *t = nullptr;
    int alloc = (ep & 0x80) ? ((len + msc_mps - 1) / msc_mps) * msc_mps : len;
    if (usb_host_transfer_alloc(alloc, 0, &t) != ESP_OK) { return -1; }
    if (!(ep & 0x80)) { memcpy(t->data_buffer, buf, len); }
    t->device_handle = mdev; t->bEndpointAddress = ep; t->num_bytes = alloc;
    t->callback = xfer_done; t->context = nullptr; t->timeout_ms = tmo;
    xSemaphoreTake(bsem, 0);
    int r = -2;
    if (usb_host_transfer_submit(t) == ESP_OK) {
      if (xSemaphoreTake(bsem, pdMS_TO_TICKS(tmo + 1000)) == pdTRUE) {
        r = (USB_TRANSFER_STATUS_COMPLETED == t->status) ? 0 : (100 + (int)t->status);
        if ((ep & 0x80) && buf) { memcpy(buf, t->data_buffer, (t->actual_num_bytes < len) ? t->actual_num_bytes : len); }
        if (actual) { *actual = t->actual_num_bytes; }
      } else { r = -3; }
    }
    if (r != -3) { usb_host_transfer_free(t); }
    return r;
  }

  inline static uint32_t tag = 1;
  inline static int msc_reads = 128;                    // 16 KB reads in the speed test (more in the combined test)
  // returns CSW status (0 ok, 1 failed, 2 phase error), or <0 transport problem. out: the data goes TO the device (WRITE).
  static int bot_raw(const uint8_t *cb, int cbl, uint8_t *data, int dlen, int *got, bool out) {
    uint8_t cbw[31] = {0};
    uint32_t sig = 0x43425355, tg = tag++;
    memcpy(cbw, &sig, 4); memcpy(cbw + 4, &tg, 4); memcpy(cbw + 8, &dlen, 4);
    cbw[12] = (dlen && !out) ? 0x80 : 0; cbw[14] = cbl; memcpy(cbw + 15, cb, cbl);
    int r = bulk(msc_out, cbw, 31, nullptr, 2000);
    if (r) { return -10 + r; }
    int ac = 0;
    if (dlen) {
      int dep = out ? msc_out : msc_in;
      r = bulk(dep, data, dlen, &ac, 5000);
      if (r) {                                             // stalled data phase: clear the halt, the CSW is still to come
        usb_host_endpoint_halt(mdev, dep);
        usb_host_endpoint_flush(mdev, dep);
        usb_host_endpoint_clear(mdev, dep);
        if (got) { *got = 0; }
        dlen = 0;
      } else if (got) { *got = ac; }
    }
    uint8_t csw[64] = {0};
    r = bulk(msc_in, csw, 13, &ac, 2000);
    if (r) { return -30 + r; }
    uint32_t cs; memcpy(&cs, csw, 4);
    if (cs != 0x53425355) { return -40; }
    return csw[12];
  }
  static int bot(const uint8_t *cb, int cbl, uint8_t *data, int dlen, int *got, bool out = false) {
    if (msc_mtx && xSemaphoreTake(msc_mtx, pdMS_TO_TICKS(10000)) != pdTRUE) { return -50; }
    int r = bot_raw(cb, cbl, data, dlen, got, out);
    if (msc_mtx) { xSemaphoreGive(msc_mtx); }
    return r;
  }

  static void msc_test(void) {
    if (msc_mounted) { say("msc: the stick is mounted as /usb, unmount first (UsbHostMount 0)"); return; }
    if (!(msc_if >= 0 && msc_in && msc_out)) { say("msc: no mass storage interface on this device"); return; }
    esp_err_t e = usb_host_interface_claim(client, mdev, msc_if, 0);
    if (e != ESP_OK) { say("msc: claim failed %s", esp_err_to_name(e)); return; }
    uint8_t *b = (uint8_t *)heap_caps_malloc(512, MALLOC_CAP_INTERNAL);
    uint8_t cb[16];
    int got = 0, st;
    memset(cb, 0, 16); cb[0] = 0x12; cb[4] = 36;
    st = bot(cb, 6, b, 36, &got);
    if (st < 0) { say("msc: INQUIRY failed (%d)", st); goto done; }
    { char v[9], p[17]; memcpy(v, b + 8, 8); v[8] = 0; memcpy(p, b + 16, 16); p[16] = 0;
      for (int i = 0; i < 8; i++) { if (v[i] < 32 || v[i] > 126) v[i] = '?'; }
      for (int i = 0; i < 16; i++) { if (p[i] < 32 || p[i] > 126) p[i] = '?'; }
      say("msc: INQUIRY ok: %s %s", v, p); }
    for (int i = 0; i < 8; i++) {                           // wait until the unit is ready
      memset(cb, 0, 16); st = bot(cb, 6, nullptr, 0, nullptr);
      if (0 == st) { break; }
      uint8_t sn[32] = {0}; int sg = 0;                     // REQUEST SENSE clears a pending unit attention
      memset(cb, 0, 16); cb[0] = 0x03; cb[4] = 18;
      int ss = bot(cb, 6, sn, 18, &sg);
      say("msc: not ready (%d), sense status %d key %X asc %02X/%02X (%d bytes)", st, ss, sn[2] & 15, sn[12], sn[13], sg);
      vTaskDelay(pdMS_TO_TICKS(300));
    }
    if (st != 0) { say("msc: TEST UNIT READY not ok (%d)", st); }
    { uint32_t last = 0, bs = 0;
      memset(cb, 0, 16); cb[0] = 0x25;
      st = bot(cb, 10, b, 8, &got);
      if (st != 0) { say("msc: READ CAPACITY failed (%d)", st); goto done; }
      last = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]; bs = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
      say("msc: capacity %u blocks of %u bytes = %u MB", (unsigned)(last + 1), (unsigned)bs, (unsigned)(((uint64_t)(last + 1) * bs) >> 20));
      if (bs != 512) { say("msc: block size is not 512, stopping here"); goto done; }
      memset(cb, 0, 16); cb[0] = 0x28; cb[8] = 1;
      st = bot(cb, 10, b, 512, &got);
      if (st != 0 || got != 512) { say("msc: READ(10) sector 0 failed (%d, %d bytes)", st, got); goto done; }
      bool sig55 = (0x55 == b[510] && 0xAA == b[511]);
      say("msc: sector 0 read, boot signature %s", sig55 ? "55AA" : "MISSING");
      uint32_t fs_lba = 0;
      if (sig55) {
        if (b[0] == 0xEB || b[0] == 0xE9) { say("msc: no partition table, file system starts at sector 0"); }
        else {
          for (int i = 0; i < 4; i++) {
            const uint8_t *pe = b + 446 + 16 * i;
            uint32_t sl = pe[8] | (pe[9] << 8) | (pe[10] << 16) | (pe[11] << 24), sz = pe[12] | (pe[13] << 8) | (pe[14] << 16) | (pe[15] << 24);
            if (pe[4]) { say("msc: partition %d type %02X start %u size %u MB", i + 1, pe[4], (unsigned)sl, (unsigned)(sz >> 11)); if (!fs_lba && pe[4] != 0xEE) { fs_lba = sl; } }
          }
          if (0xEE == b[450]) { say("msc: GPT partition table (protective MBR)"); }
        }
        if (fs_lba) {
          memset(cb, 0, 16); cb[0] = 0x28; cb[2] = fs_lba >> 24; cb[3] = fs_lba >> 16; cb[4] = fs_lba >> 8; cb[5] = fs_lba; cb[8] = 1;
          st = bot(cb, 10, b, 512, &got);
        }
        if (0 == st) {
          if (!memcmp(b + 82, "FAT32", 5)) { say("msc: file system FAT32"); }
          else if (!memcmp(b + 54, "FAT16", 5) || !memcmp(b + 54, "FAT12", 5)) { say("msc: file system FAT12/16"); }
          else if (!memcmp(b + 3, "EXFAT", 5)) { say("msc: file system exFAT"); }
          else if (!memcmp(b + 3, "NTFS", 4)) { say("msc: file system NTFS"); }
          else { say("msc: file system not recognised at the first partition"); }
        }
      }
      if (last > 8192) {                                    // read speed: msc_reads x 16 KB
        uint8_t *big = (uint8_t *)heap_caps_malloc(16384, MALLOC_CAP_INTERNAL);
        if (big) {
          uint32_t t0 = millis(); int okn = 0;
          for (int i = 0; i < msc_reads; i++) {
            memset(cb, 0, 16); cb[0] = 0x28; uint32_t lba = (i % 1000) * 32; cb[2] = lba >> 24; cb[3] = lba >> 16; cb[4] = lba >> 8; cb[5] = lba; cb[8] = 32;
            st = bot(cb, 10, big, 16384, &got);
            if (st != 0 || got != 16384) { say("msc: read %d failed (%d, %d bytes)", i, st, got); break; }
            okn++;
          }
          uint32_t ms = millis() - t0;
          say("msc: read %d KB in %u ms = %u KB/s", okn * 16, (unsigned)ms, ms ? (unsigned)((okn * 16000UL) / ms) : 0);
          free(big);
        } else { say("msc: no memory for the speed test"); }
      }
    }
done:
    free(b);
    usb_host_interface_release(client, mdev, msc_if);
    say("msc: test finished");
  }
#endif  // USE_USB_HOST_MSC

#ifdef USBH_MSC_FS
  // ---------------- FAT file system on the stick: block device for FatFS, mounted as /usb ----------------
  inline static uint8_t msc_pdrv = 0xFF;
  inline static char msc_drv[3] = "";
  inline static UsbHostFS usbfs;
  inline static ff_diskio_impl_t msc_impl;

  static DSTATUS d_status(unsigned char pdrv) { return msc_ready ? 0 : STA_NOINIT; }
  static DSTATUS d_init(unsigned char pdrv) { return d_status(pdrv); }
  // up to 32 sectors (16 KB) per SCSI command
  static DRESULT d_rw(bool wr, unsigned char *buff, uint32_t sector, unsigned count) {
    if (!msc_ready) { return RES_NOTRDY; }
    while (count) {
      unsigned n = (count > 32) ? 32 : count;
      uint8_t cb[16] = {0};
      cb[0] = wr ? 0x2A : 0x28;
      cb[2] = sector >> 24; cb[3] = sector >> 16; cb[4] = sector >> 8; cb[5] = sector;
      cb[7] = n >> 8; cb[8] = n;
      int got = 0;
      int st = bot(cb, 10, buff, n * 512, &got, wr);
      if (st != 0 || (!wr && got != (int)(n * 512))) { return RES_ERROR; }
      buff += n * 512; sector += n; count -= n;
    }
    return RES_OK;
  }
  static DRESULT d_read(unsigned char pdrv, unsigned char *buff, uint32_t sector, unsigned count) { return d_rw(false, buff, sector, count); }
  static DRESULT d_write(unsigned char pdrv, const unsigned char *buff, uint32_t sector, unsigned count) { return d_rw(true, (unsigned char *)buff, sector, count); }
  static DRESULT d_ioctl(unsigned char pdrv, unsigned char cmd, void *buff) {
    switch (cmd) {
      case CTRL_SYNC: return RES_OK;                        // every write command is completed before it returns
      case GET_SECTOR_COUNT: *(DWORD *)buff = msc_blocks; return RES_OK;
      case GET_SECTOR_SIZE: *(WORD *)buff = 512; return RES_OK;
      case GET_BLOCK_SIZE: *(DWORD *)buff = 1; return RES_OK;
    }
    return RES_PARERR;
  }

  static void msc_unmount(void) {
    if (!msc_mounted) { return; }
    usbfsp = nullptr;                                       // nobody gets new access
    msc_mounted = false;
    msc_ready = false;
    say("unmount: f_mount(NULL)");
    f_mount(nullptr, msc_drv, 0);
    say("unmount: unregister VFS");
    esp_vfs_fat_unregister_path("/usb");
    say("unmount: unregister block device");
    ff_diskio_unregister(msc_pdrv);
    say("unmount: release interface");
    if (mdev) { usb_host_interface_release(client, mdev, msc_if); }
    say("mount: /usb unmounted");
  }

  // never from the client task: the transfers below are served by that task. Only one mount at a time (two sticks plugged in at once).
  inline static volatile bool msc_mounting = false;
  static bool msc_mount(void) {
    if (msc_mounted) { return true; }
    if (msc_mounting) { return false; }
    msc_mounting = true;
    bool ok = msc_mount_do();
    msc_mounting = false;
    return ok;
  }
  static bool msc_mount_do(void) {
    if (msc_mounted) { return true; }
    if (!running) { say("mount: the host is not running: UsbHost 1"); return false; }
    if (busy_msc) { say("mount: the stick test is running"); return false; }
    if (!select(2)) { say("mount: no mass storage device attached"); return false; }
    say("mount: using device %04X, interface %d", (unsigned)(uintptr_t)mdev & 0xFFFF, msc_if);
    uint8_t b[64]; uint8_t cb[16]; int got = 0, st = -1;
    // A stick that was enumerated a moment ago (or while other devices of the hub were still announcing themselves) often
    // answers the first commands with errors for good: let go of it, wait and start over, up to three times.
    for (int attempt = 0; attempt < 3; attempt++) {
      if (attempt) { say("mount: the stick does not answer, trying again (%d)", attempt + 1); }
      esp_err_t e = usb_host_interface_claim(client, mdev, msc_if, 0);
      if (e != ESP_OK) { say("mount: claim failed %s", esp_err_to_name(e)); return false; }
      say("mount: interface claimed");
      // Bulk-only reset recovery: after a release and claim the host starts with data toggle 0 while the device may still expect the
      // other one, and then drops the first packet. Reset (class request) and CLEAR_FEATURE(ENDPOINT_HALT) on both endpoints resync it.
      { int r1 = ctrl(mdev, 0x21, 0xFF, 0, msc_if, nullptr, 0);
        int r2 = ctrl(mdev, 0x02, 0x01, 0, msc_in, nullptr, 0);
        int r3 = ctrl(mdev, 0x02, 0x01, 0, msc_out, nullptr, 0);
        say("mount: reset recovery %d %d %d", r1, r2, r3); }
      for (int i = 0; i < 8; i++) {                         // the first command after plug-in reports "medium changed": REQUEST SENSE clears it
        memset(cb, 0, 16); st = bot(cb, 6, nullptr, 0, nullptr);
        say("mount: TEST UNIT READY %d -> %d", i, st);
        if (0 == st) { break; }
        memset(cb, 0, 16); cb[0] = 0x03; cb[4] = 18;
        bot(cb, 6, b, 18, &got);
        vTaskDelay(pdMS_TO_TICKS(300));
      }
      say("mount: unit ready check done (status %d)", st);
      if (0 == st) { break; }
      usb_host_interface_release(client, mdev, msc_if);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (st != 0) { return false; }
    memset(cb, 0, 16); cb[0] = 0x25;
    st = bot(cb, 10, b, 8, &got);
    uint32_t last = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3], bs = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
    if (st != 0 || bs != 512) {
      say("mount: capacity not usable (status %d, block size %u): only 512 byte blocks are supported", st, (unsigned)bs);
      usb_host_interface_release(client, mdev, msc_if);
      return false;
    }
    msc_blocks = last + 1;
    msc_ready = true;
    say("mount: capacity %u blocks, registering the block device", (unsigned)msc_blocks);
    uint8_t pdrv = 0xFF;
    if (ff_diskio_get_drive(&pdrv) != ESP_OK) { say("mount: no free drive number"); msc_ready = false; usb_host_interface_release(client, mdev, msc_if); return false; }
    msc_impl.init = d_init; msc_impl.status = d_status; msc_impl.read = d_read; msc_impl.write = d_write; msc_impl.ioctl = d_ioctl;
    ff_diskio_register(pdrv, &msc_impl);
    msc_drv[0] = '0' + pdrv; msc_drv[1] = ':'; msc_drv[2] = 0;
    FATFS *fat = nullptr;
    say("mount: drive %s, registering the VFS", msc_drv);
    if (esp_vfs_fat_register("/usb", msc_drv, 5, &fat) != ESP_OK) {
      say("mount: VFS registration failed");
      ff_diskio_unregister(pdrv); msc_ready = false; usb_host_interface_release(client, mdev, msc_if); return false;
    }
    say("mount: f_mount");
    FRESULT fr = f_mount(fat, msc_drv, 1);
    if (fr != FR_OK) {
      say("mount: f_mount failed (%d): no FAT12/16/32 file system? exFAT and NTFS are not supported", (int)fr);
      esp_vfs_fat_unregister_path("/usb"); ff_diskio_unregister(pdrv); msc_ready = false; usb_host_interface_release(client, mdev, msc_if);
      return false;
    }
    msc_pdrv = pdrv;
    usbfs.mount_point("/usb");
    msc_mounted = true;
    usbfsp = &usbfs;
    say("mount: /usb mounted, %u MB", (unsigned)(((uint64_t)msc_blocks * 512) >> 20));
    return true;
  }

  static void mount_task(void *arg) { msc_mount(); vTaskDelete(nullptr); }
  static void automount_task(void *arg) { vTaskDelay(pdMS_TO_TICKS(2500)); msc_mount(); vTaskDelete(nullptr); }   // let the other devices of a hub finish announcing themselves first
#endif  // USBH_MSC_FS

#ifdef USE_USB_HOST_AUDIO
  // ---------------- audio via isochronous OUT ----------------
  #define USBH_NT   6      // transfers in flight
  #define USBH_NP   8      // 1 ms packets per transfer
  inline static usb_transfer_t *tr[USBH_NT];
  inline static volatile bool playing = false;
  inline static volatile int in_flight = 0;
  inline static float phase = 0; inline static int frac = 0; inline static volatile int tone_err = 0;
  inline static volatile int iso_xfer_err = 0, iso_pkt_err = 0, iso_done_n = 0;

  inline static bool file_mode = false;
  inline static int16_t *pcm = nullptr;
  inline static size_t pcm_frames = 0;
  inline static int pcm_ch = 0, pcm_rate = 0;
  inline static double sp = 0;
  inline static volatile bool end_reached = false;
  inline static char wav_path[80] = "";

  // PCM stream: TinyC's i2sBegin/i2sWrite end here when the script asks for USB audio (pin -2). One producer (the script task), one consumer (fill, in the client task).
  #define USBH_RING 8192                                    // mono samples, power of two
  inline static bool st_open = false, st_mode = false;
  inline static int16_t *st_ring = nullptr;                 // USBH_RING frames of left+right (a mono source is written twice)
  inline static volatile uint32_t st_wr = 0, st_rd = 0, st_under = 0;
  inline static double st_frac = 0;
  inline static int st_rate = 16000;
  inline static int st_level = 50;                          // percent; 50 = the half level of the tone and file player
  inline static bool st_route = false;                      // the audio plugin (I2SAUDIO) sends its I2S output here: UsbHostRoute 1

  // reads a 16 bit PCM WAV completely into RAM (PSRAM if there is any)
  static bool load_wav(const char *path) {
    extern FS *ufsp; extern FS *ffsp;
    File f;
    if (ffsp) { f = ffsp->open(path, "r"); }
    if (!f && ufsp) { f = ufsp->open(path, "r"); }
    if (!f) { say("wav: %s not found in flash or on the card", path); return false; }
    uint8_t h[12];
    if (f.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { say("wav: not a RIFF/WAVE file"); f.close(); return false; }
    int fmt_tag = 0, ch = 0, bits = 0; uint32_t rate = 0, dsize = 0; bool have_fmt = false;
    for (;;) {
      uint8_t c[8];
      if (f.read(c, 8) != 8) { break; }
      uint32_t sz = c[4] | (c[5] << 8) | (c[6] << 16) | ((uint32_t)c[7] << 24);
      if (!memcmp(c, "fmt ", 4)) {
        uint8_t b[16];
        if (sz < 16 || f.read(b, 16) != 16) { break; }
        fmt_tag = b[0] | (b[1] << 8); ch = b[2] | (b[3] << 8); rate = b[4] | (b[5] << 8) | (b[6] << 16) | ((uint32_t)b[7] << 24); bits = b[14] | (b[15] << 8);
        have_fmt = true;
        if (sz > 16) { f.seek(f.position() + sz - 16 + (sz & 1)); }
      } else if (!memcmp(c, "data", 4)) { dsize = sz; break; }
      else { f.seek(f.position() + sz + (sz & 1)); }
    }
    say("wav: format tag %d, %d ch, %u Hz, %d bit, data %u bytes", fmt_tag, ch, (unsigned)rate, bits, (unsigned)dsize);
    if (!have_fmt || !dsize || (fmt_tag != 1 && fmt_tag != 0xFFFE) || 16 != bits || (ch != 1 && ch != 2)) {
      say("wav: only 16 bit PCM, 1 or 2 channels is supported here"); f.close(); return false;
    }
    if (dsize > 3000000) { dsize = 3000000; say("wav: only the first 3 MB are played"); }
    free(pcm);
    pcm = (int16_t *)heap_caps_malloc(dsize, MALLOC_CAP_SPIRAM);
    if (!pcm) { pcm = (int16_t *)heap_caps_malloc(dsize, MALLOC_CAP_8BIT); }
    if (!pcm) { say("wav: no memory for %u bytes", (unsigned)dsize); f.close(); return false; }
    size_t got = f.read((uint8_t *)pcm, dsize);
    f.close();
    pcm_ch = ch; pcm_rate = rate; pcm_frames = got / (2 * ch);
    say("wav: %u frames = %u ms loaded", (unsigned)pcm_frames, (unsigned)(pcm_frames * 1000ULL / rate));
    return pcm_frames > 0;
  }

  // one transfer = USBH_NP packets of 1 ms each; the sample rate of the file is converted linearly to the rate of the stick
  static void fill(usb_transfer_t *t) {
    int total = 0;
    for (int p = 0; p < USBH_NP; p++) {
      int n = as_rate / 1000; frac += as_rate % 1000; if (frac >= 1000) { frac -= 1000; n++; }
      int16_t *o = (int16_t *)(t->data_buffer + total);
      for (int s = 0; s < n; s++) {
        if (file_mode) {
          if (sp >= (double)pcm_frames) { end_reached = true; for (int c = 0; c < as_ch; c++) { *o++ = 0; } continue; }
          size_t i0 = (size_t)sp; double fr = sp - (double)i0; size_t i1 = (i0 + 1 < pcm_frames) ? i0 + 1 : i0;
          for (int c = 0; c < as_ch; c++) {
            int sc = (pcm_ch > 1 && c > 0) ? 1 : 0;
            double a = pcm[i0 * pcm_ch + sc], b = pcm[i1 * pcm_ch + sc];
            *o++ = (int16_t)((a + (b - a) * fr) * 0.5);       // half level
          }
          sp += (double)pcm_rate / (double)as_rate;
        } else if (st_mode) {                               // PCM from TinyC (i2sWrite): linear conversion from the script's rate
          if ((uint32_t)(st_wr - st_rd) < 2) { st_under++; for (int c = 0; c < as_ch; c++) { *o++ = 0; } continue; }   // nothing there yet: silence, position stays
          const int16_t *f0 = st_ring + 2 * (st_rd & (USBH_RING - 1)), *f1 = st_ring + 2 * ((st_rd + 1) & (USBH_RING - 1));
          double g = st_level / 100.0;
          double l = (f0[0] + (f1[0] - f0[0]) * st_frac) * g, r = (f0[1] + (f1[1] - f0[1]) * st_frac) * g;
          if (1 == as_ch) { *o++ = (int16_t)((l + r) * 0.5); }
          else { *o++ = (int16_t)l; *o++ = (int16_t)r; for (int c = 2; c < as_ch; c++) { *o++ = 0; } }
          st_frac += (double)st_rate / (double)as_rate;
          while (st_frac >= 1.0) { st_frac -= 1.0; st_rd++; }
        } else {
          int16_t v = (int16_t)(6000.0f * sinf(phase)); phase += 2.0f * 3.14159265f * 440.0f / as_rate; if (phase > 6.2831853f) { phase -= 6.2831853f; }
          for (int c = 0; c < as_ch; c++) { *o++ = v; }
        }
      }
      int bytes = n * as_ch * 2;
      t->isoc_packet_desc[p].num_bytes = bytes; total += bytes;
    }
    t->num_bytes = total;
  }

  static void iso_done(usb_transfer_t *t) {
    iso_done_n++;
    if (USB_TRANSFER_STATUS_COMPLETED != t->status) { iso_xfer_err++; }
    for (int i = 0; i < t->num_isoc_packets; i++) { if (USB_TRANSFER_STATUS_COMPLETED != t->isoc_packet_desc[i].status) { iso_pkt_err++; } }
    if (playing) {
      fill(t);
      if (usb_host_transfer_submit(t) != ESP_OK) { tone_err++; in_flight--; }
    } else { in_flight--; }
  }

  static void audio_test(void) {
    if (as_if < 0) { say("audio: no UAC1 16 bit playback format on this device"); return; }
    if (file_mode && !load_wav(wav_path)) { return; }
    esp_err_t e = usb_host_interface_claim(client, dev, as_if, as_alt);
    if (e != ESP_OK) { say("audio: claim of interface %d alt %d failed %s (0x%x)", as_if, as_alt, esp_err_to_name(e), (unsigned)e); return; }
    if (file_mode) {                                        // tell the stick which rate it gets (class request SET_CUR, sampling frequency control)
      uint8_t fr[3] = { (uint8_t)(as_rate & 0xFF), (uint8_t)((as_rate >> 8) & 0xFF), (uint8_t)((as_rate >> 16) & 0xFF) };
      int cr = ctrl(dev, 0x22, 0x01, 0x0100, as_ep, fr, 3);
      say("audio: set sampling frequency %d Hz on endpoint %02X: %s (%d)", as_rate, as_ep, cr ? "failed, stick keeps its default" : "ok", cr);
      say("audio: playing the file at half level, %d Hz %d ch, if %d alt %d ep %02X mps %d", as_rate, as_ch, as_if, as_alt, as_ep, as_mps);
    } else {
      say("audio: playing 440 Hz, %d Hz %d ch, if %d alt %d ep %02X mps %d (sampling frequency not set: the stick runs at its own default)",
          as_rate, as_ch, as_if, as_alt, as_ep, as_mps);
    }
    int maxb = (as_rate / 1000 + 1) * as_ch * 2;
    if (maxb > as_mps) { say("audio: packet of %d bytes exceeds the endpoint (%d), stopping", maxb, as_mps); goto rel; }
    {
      int made = 0;
      for (int i = 0; i < USBH_NT; i++) {
        if (usb_host_transfer_alloc(maxb * USBH_NP, USBH_NP, &tr[i]) != ESP_OK) { break; }
        made++;
        tr[i]->device_handle = dev; tr[i]->bEndpointAddress = as_ep; tr[i]->callback = iso_done; tr[i]->context = nullptr;
        tr[i]->timeout_ms = 0;   // num_isoc_packets is fixed by the allocation
      }
      if (made < USBH_NT) { say("audio: no memory for the transfers"); for (int i = 0; i < made; i++) { usb_host_transfer_free(tr[i]); } goto rel; }
      phase = 0; frac = 0; sp = 0; end_reached = false; tone_err = 0; iso_xfer_err = 0; iso_pkt_err = 0; iso_done_n = 0; in_flight = 0; playing = true;
      for (int i = 0; i < USBH_NT; i++) { fill(tr[i]); if (usb_host_transfer_submit(tr[i]) == ESP_OK) { in_flight++; } else { tone_err++; } }
      if (file_mode) {
        for (int w = 0; w < 300 && !end_reached; w++) { vTaskDelay(pdMS_TO_TICKS(100)); }   // at most 30 s
        vTaskDelay(pdMS_TO_TICKS(150));                      // let the last packets out
      } else { vTaskDelay(pdMS_TO_TICKS(2000)); }
      playing = false;
      for (int w = 0; w < 50 && in_flight > 0; w++) { vTaskDelay(pdMS_TO_TICKS(10)); }
      say("audio: stopped, %d transfers still pending, %d submit errors; %d transfers done, %d with errors, %d bad packets",
          (int)in_flight, (int)tone_err, (int)iso_done_n, (int)iso_xfer_err, (int)iso_pkt_err);
      if (0 == in_flight) { for (int i = 0; i < USBH_NT; i++) { usb_host_transfer_free(tr[i]); } }
    }
rel:
    usb_host_interface_release(client, dev, as_if);
    say("audio: test finished");
  }

  static void stream_close(void) {
    if (!st_open) { return; }
    // the script is ahead of the sound by up to the size of the ring: let it play out before the transfers stop (like the DMA of a real I2S stop)
    for (int w = 0; w < 250 && (uint32_t)(st_wr - st_rd) >= 2 && playing; w++) { vTaskDelay(pdMS_TO_TICKS(10)); }
    vTaskDelay(pdMS_TO_TICKS(USBH_NT * USBH_NP + 20));      // the packets already handed to the stick
    playing = false;
    for (int w = 0; w < 50 && in_flight > 0; w++) { vTaskDelay(pdMS_TO_TICKS(10)); }
    int pend = in_flight;
    if (0 == in_flight) { for (int i = 0; i < USBH_NT; i++) { if (tr[i]) { usb_host_transfer_free(tr[i]); tr[i] = nullptr; } } }
    esp_err_t re = usb_host_interface_release(client, dev, as_if);
    for (int w = 0; re != ESP_OK && w < 30; w++) {          // transfers that are still being cancelled: try again for a while
      vTaskDelay(pdMS_TO_TICKS(50));
      if (0 == in_flight) { for (int i = 0; i < USBH_NT; i++) { if (tr[i]) { usb_host_transfer_free(tr[i]); tr[i] = nullptr; } } }
      re = usb_host_interface_release(client, dev, as_if);
    }
    if (re != ESP_OK) { say("audio stream: release failed %s (0x%x), %d transfers were pending", esp_err_to_name(re), (unsigned)re, pend); }
    st_mode = false; st_open = false;
    free(st_ring); st_ring = nullptr;
    busy_aud = 0; test_busy = busy_msc || busy_aud;
    say("audio stream: closed, %u underruns", (unsigned)st_under);
  }

  // rate = sample rate of the samples that will be written (the stick runs at its own rate, the driver converts). 0 = ok, -1 = host, -2 = no audio device, -3 = busy/other error
  static int stream_open(int rate) {
    if (st_open) { stream_close(); }
    if (!start()) { return -1; }
    if (playing || busy_aud) { return -3; }
    for (int i = 0; i < 30 && !select(3); i++) { vTaskDelay(pdMS_TO_TICKS(100)); }   // the host may just have been started
    if (!select(3) || as_if < 0) { say("audio stream: no UAC1 16 bit playback device attached"); return -2; }
    if (!bsem) { bsem = xSemaphoreCreateBinary(); }
    busy_aud = 1; test_busy = true;
    esp_err_t e = usb_host_interface_claim(client, dev, as_if, as_alt);
    if (e != ESP_OK) { say("audio stream: claim failed %s (0x%x)", esp_err_to_name(e), (unsigned)e); busy_aud = 0; test_busy = busy_msc; return -3; }
    uint8_t fr[3] = { (uint8_t)(as_rate & 0xFF), (uint8_t)((as_rate >> 8) & 0xFF), (uint8_t)((as_rate >> 16) & 0xFF) };
    ctrl(dev, 0x22, 0x01, 0x0100, as_ep, fr, 3);
    int maxb = (as_rate / 1000 + 1) * as_ch * 2;
    st_ring = (int16_t *)heap_caps_malloc(USBH_RING * 2 * sizeof(int16_t), MALLOC_CAP_8BIT);
    int made = 0;
    if (st_ring && maxb <= as_mps) {
      for (int i = 0; i < USBH_NT; i++) {
        if (usb_host_transfer_alloc(maxb * USBH_NP, USBH_NP, &tr[i]) != ESP_OK) { tr[i] = nullptr; break; }
        made++;
        tr[i]->device_handle = dev; tr[i]->bEndpointAddress = as_ep; tr[i]->callback = iso_done; tr[i]->context = nullptr; tr[i]->timeout_ms = 0;
      }
    }
    if (made < USBH_NT) {
      say("audio stream: no memory or packet too big for the endpoint");
      for (int i = 0; i < made; i++) { usb_host_transfer_free(tr[i]); tr[i] = nullptr; }
      free(st_ring); st_ring = nullptr;
      usb_host_interface_release(client, dev, as_if);
      busy_aud = 0; test_busy = busy_msc;
      return -3;
    }
    st_rate = (rate < 4000) ? 4000 : ((rate > 96000) ? 96000 : rate);
    st_wr = 0; st_rd = 0; st_frac = 0; st_under = 0;
    file_mode = false; st_mode = true; st_open = true;
    phase = 0; frac = 0; tone_err = 0; iso_xfer_err = 0; iso_pkt_err = 0; iso_done_n = 0; in_flight = 0; playing = true;
    for (int i = 0; i < USBH_NT; i++) { fill(tr[i]); if (usb_host_transfer_submit(tr[i]) == ESP_OK) { in_flight++; } else { tone_err++; } }
    say("audio stream: open, %d Hz in, stick %d Hz %d ch", st_rate, as_rate, as_ch);
    return 0;
  }

  // blocks while the ring is full (that is what paces the writer, like the I2S DMA); returns the frames taken, short after 1 s without room.
  // x holds frames of ch (1 or 2) interleaved 16 bit samples
  static int stream_write(const int16_t *x, int n, int ch = 1) {
    if (!st_open || !st_ring) { return -1; }
    int done = 0; uint32_t t0 = millis();
    while (done < n) {
      uint32_t used = st_wr - st_rd;
      uint32_t room = (used >= USBH_RING - 2) ? 0 : (USBH_RING - 2 - used);
      if (!room) { if (millis() - t0 > 1000 || !playing) { break; } vTaskDelay(1); continue; }
      int chunk = (n - done < (int)room) ? (n - done) : (int)room;
      for (int i = 0; i < chunk; i++) {
        int16_t *d = st_ring + 2 * ((st_wr + i) & (USBH_RING - 1));
        if (2 == ch) { d[0] = x[2 * (done + i)]; d[1] = x[2 * (done + i) + 1]; } else { d[0] = d[1] = x[done + i]; }
      }
      __sync_synchronize();
      st_wr += chunk; done += chunk; t0 = millis();
    }
    return done;
  }

  // the writer's rate changed (another file, another stream): the ring keeps what it has, only the conversion step changes
  static void stream_set_rate(int rate) { st_rate = (rate < 4000) ? 4000 : ((rate > 96000) ? 96000 : rate); }
#endif  // USE_USB_HOST_AUDIO


#ifdef USE_USB_HOST_SERIAL
  // ---------------- FTDI USB-serial adapter ----------------
  #define USBH_RING   16384   // receive ring, outlasts a Wi-Fi hiccup (2 KB were 89 ms at 230400 baud and lost bytes)
  #define USBH_MPS    64      // full speed, bulk
  #define USBH_IN_N   4       // IN transfers in flight at once
  #define FTDI_RESET      0x00
  #define FTDI_MODEM_CTRL 0x01
  #define FTDI_SET_FLOW   0x02
  #define FTDI_SET_BAUD   0x03
  #define FTDI_SET_DATA   0x04
  inline static volatile bool ser_run = false;        // the IN transfers are resubmitted while this is true
  inline static volatile bool ser_open_f = false, ser_err = false;
  inline static usb_transfer_t *ser_in[USBH_IN_N] = { nullptr };
  inline static usb_transfer_t *ser_out = nullptr;
  inline static SemaphoreHandle_t ssem = nullptr;     // end of an OUT transfer
  inline static uint8_t *ring = nullptr;
  inline static volatile uint16_t rhead = 0, rtail = 0;
  inline static portMUX_TYPE rmux = portMUX_INITIALIZER_UNLOCKED;
  inline static volatile uint32_t s_rx = 0, s_tx = 0, s_lost = 0;
  inline static uint32_t s_baud = 0;

  static uint16_t ring_avail(void) {
    uint16_t h = rhead, t = rtail;
    return (h >= t) ? (h - t) : (uint16_t)(USBH_RING - t + h);
  }
  static void ring_push(const uint8_t *d, uint16_t len) {
    if (!ring) { return; }
    taskENTER_CRITICAL(&rmux);
    for (uint16_t i = 0; i < len; i++) {
      uint16_t nh = (rhead + 1) % USBH_RING;
      if (nh == rtail) { s_lost++; break; }             // full: keep the older bytes
      ring[rhead] = d[i];
      rhead = nh;
    }
    taskEXIT_CRITICAL(&rmux);
  }
  static uint16_t ring_pull(uint8_t *to, uint16_t max) {
    uint16_t n = 0;
    if (!ring) { return 0; }
    taskENTER_CRITICAL(&rmux);
    while (n < max && rtail != rhead) { to[n++] = ring[rtail]; rtail = (rtail + 1) % USBH_RING; }
    taskEXIT_CRITICAL(&rmux);
    return n;
  }

  // callbacks run in the USB task: only store and hand on, do not compute, do not log
  static void ser_in_done(usb_transfer_t *t) {
    if (USB_TRANSFER_STATUS_COMPLETED == t->status && t->actual_num_bytes > 2) {
      ring_push(t->data_buffer + 2, (uint16_t)(t->actual_num_bytes - 2));    // the first two bytes are modem and line status
      s_rx += (t->actual_num_bytes - 2);
    }
    if (ser_run && sdev) { usb_host_transfer_submit(t); }
  }
  static void ser_out_done(usb_transfer_t *t) { if (ssem) { xSemaphoreGive(ssem); } }

  static bool ser_ctrl(uint8_t req, uint16_t val, uint16_t idx) { return 0 == ctrl(sdev, 0x40, req, val, idx, nullptr, 0); }

  // FT232 divisor against its 3 MHz base clock. The eighths are NOT in order - that is how the data sheet and Linux do it.
  // 230400 gives divisor 13 = 230769 baud, 0.16 % off, as with every driver.
  static uint32_t ser_divisor(uint32_t baud) {
    static const uint8_t frac8[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
    if (!baud) { return 0; }
    uint32_t x8 = (3000000UL * 8) / baud;
    uint32_t code = (x8 >> 3) | ((uint32_t)frac8[x8 & 7] << 14);
    if (1 == code) { code = 0; }                         // 2 MBaud
    else if (0x4001 == code) { code = 1; }               // 3 MBaud
    return code;
  }
  static bool ser_baud(uint32_t baud) {
    uint32_t code = ser_divisor(baud);
    if (!ser_ctrl(FTDI_SET_BAUD, (uint16_t)(code & 0xFFFF), (uint16_t)(code >> 16))) { return false; }
    s_baud = baud;
    return true;
  }

  static void ser_free_transfers(void) {
    for (int i = 0; i < USBH_IN_N; i++) { if (ser_in[i]) { usb_host_transfer_free(ser_in[i]); ser_in[i] = nullptr; } }
    if (ser_out) { usb_host_transfer_free(ser_out); ser_out = nullptr; }
  }

  static void ser_close(void) {
    ser_run = false;                                    // stops the IN transfers being resubmitted
    if (!ser_open_f && !ser_out && !ser_in[0]) { return; }
    vTaskDelay(pdMS_TO_TICKS(60));
    ser_free_transfers();
    if (sdev) { usb_host_interface_release(client, sdev, ft_if); }
    ser_open_f = false;
    rhead = rtail = 0;
    sdev = nullptr;
  }

  // FTDI vendor requests: reset, baud, 8N1, no flow control, DTR+RTS on. false: no FTDI attached (yet) or something failed.
  static bool ser_open(uint32_t baud) {
    if (!running && !start()) { return false; }
    if (ser_open_f) { return ser_baud(baud); }
    ser_free_transfers();                               // leftovers of a device that was pulled
    ser_err = false;
    if (!select(5)) { return false; }                   // nothing attached yet: the caller tries again
    if (!ring) {
      ring = (uint8_t *)heap_caps_malloc(USBH_RING, MALLOC_CAP_SPIRAM);
      if (!ring) { ring = (uint8_t *)heap_caps_malloc(USBH_RING, MALLOC_CAP_8BIT); }
      if (!ring) { say("serial: no memory for the ring"); ser_err = true; return false; }
    }
    if (usb_host_interface_claim(client, sdev, ft_if, 0) != ESP_OK) { say("serial: interface not to be claimed"); ser_err = true; sdev = nullptr; return false; }
    usb_host_transfer_alloc(USBH_MPS, 0, &ser_out);
    if (!ser_out) { ser_close(); ser_err = true; return false; }
    ser_ctrl(FTDI_RESET, 0, 0);
    if (!ser_baud(baud)) { say("serial: baud rate not accepted"); ser_close(); ser_err = true; return false; }
    ser_ctrl(FTDI_SET_DATA, 0x0008, 0);
    ser_ctrl(FTDI_SET_FLOW, 0, 0);
    ser_ctrl(FTDI_MODEM_CTRL, 0x0303, 0);
    rhead = rtail = 0;
    ser_run = true;
    for (int i = 0; i < USBH_IN_N; i++) {
      if (usb_host_transfer_alloc(USBH_MPS, 0, &ser_in[i]) != ESP_OK) { break; }
      ser_in[i]->device_handle = sdev; ser_in[i]->bEndpointAddress = ft_in;
      ser_in[i]->callback = ser_in_done; ser_in[i]->context = nullptr; ser_in[i]->num_bytes = USBH_MPS;
      usb_host_transfer_submit(ser_in[i]);
    }
    ser_open_f = true;
    say("serial: %04X:%04X open, %u baud, ep %02X/%02X", s_vid, s_pid, (unsigned)baud, ft_in, ft_out);
    return true;
  }

  static int32_t ser_write(const uint8_t *d, uint16_t len) {
    if (!ser_open_f || !ser_out || !sdev) { return -1; }
    uint16_t total = 0;
    while (total < len) {
      uint16_t part = len - total; if (part > USBH_MPS) { part = USBH_MPS; }
      memcpy(ser_out->data_buffer, d + total, part);
      ser_out->device_handle = sdev; ser_out->bEndpointAddress = ft_out;
      ser_out->callback = ser_out_done; ser_out->context = nullptr; ser_out->num_bytes = part;
      if (usb_host_transfer_submit(ser_out) != ESP_OK) { return total ? total : -1; }
      if (xSemaphoreTake(ssem, pdMS_TO_TICKS(300)) != pdTRUE) { return total ? total : -1; }
      if (USB_TRANSFER_STATUS_COMPLETED != ser_out->status) { return total ? total : -1; }
      total += part; s_tx += part;
    }
    return total;
  }

  // 0 off, 1 host running without FTDI, 2 FTDI attached but not open, 3 open, 4 opening failed (same ladder as the TinyC usbState)
  static uint32_t ser_state(void) {
    if (!running) { return 0; }
    if (ser_open_f) { return 3; }
    if (ser_err) { return 4; }
    for (int i = 0; i < nslots; i++) { if (slots[i].ft_if >= 0) { return 2; } }
    return 1;
  }
  static uint32_t ser_get(uint8_t which) {
    switch (which) {
      case 0: return ser_state();
      case 1: case 2: {
        if (ser_open_f) { return (1 == which) ? s_vid : s_pid; }
        for (int i = 0; i < nslots; i++) { if (slots[i].ft_if >= 0) { return (1 == which) ? slots[i].vid : slots[i].pid; } }
        return 0; }
      case 3: return s_rx;
      case 4: return s_tx;
      default: return s_lost;
    }
  }

  // ---------------- TCP bridge: one client at a time, bytes both ways ----------------
  inline static volatile bool br_run = false;
  inline static uint16_t br_port = 0;
  inline static TaskHandle_t br_task = nullptr;
  static void bridge_task(void *arg) {
    WiFiServer srv(br_port);
    srv.begin();
    say("bridge: listening on port %u", br_port);
    while (br_run) {
      WiFiClient c = srv.accept();
      if (!c) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
      say("bridge: client connected");
      while (br_run && c.connected()) {
        int n = c.available();
        if (n > 0) {
          uint8_t b[64]; n = c.read(b, (n > 64) ? 64 : n);
          if (n > 0 && ser_open_f) { ser_write(b, n); }
        }
        uint8_t r[256]; uint16_t m = ser_pull_ring(r, sizeof(r));
        if (m) { c.write(r, m); }
        if (n <= 0 && !m) { vTaskDelay(pdMS_TO_TICKS(2)); }
      }
      c.stop();
      say("bridge: client gone");
    }
    srv.end();
    br_task = nullptr;
    vTaskDelete(nullptr);
  }
  static uint16_t ser_pull_ring(uint8_t *to, uint16_t max) { return ring_pull(to, max); }
  static bool bridge_start(uint16_t port) {
    if (br_run) { return true; }
    if (TasmotaGlobal.global_state.network_down) { say("bridge: no network"); return false; }
    br_port = port; br_run = true;
    return pdPASS == xTaskCreatePinnedToCore(bridge_task, "usbh_br", 4096, nullptr, 3, &br_task, 1);
  }
  static void bridge_stop(void) { br_run = false; }
#endif  // USE_USB_HOST_SERIAL

#ifdef USBH_NEEDS_TRANSFERS
  // ---------------- test tasks: one per kind, they may run at the same time ----------------
  inline static volatile int busy_msc = 0, busy_aud = 0;
  static void test_task(void *arg) {
    if (!bsem) { bsem = xSemaphoreCreateBinary(); }
    int k = (int)(intptr_t)arg;
    if (!select(k)) { say("no attached device fits this test"); }
#ifdef USE_USB_HOST_MSC
    else if (2 == k) { msc_test(); }
#endif
#ifdef USE_USB_HOST_AUDIO
    else if (3 == k) { file_mode = false; audio_test(); }
    else if (4 == k) { file_mode = true; audio_test(); file_mode = false; }
#endif
    if (2 == k) { busy_msc = 0; } else { busy_aud = 0; }
    test_busy = busy_msc || busy_aud;
    vTaskDelete(nullptr);
  }

  static void run_test(int n) {
    if (!running) { say("the host is not running: UsbHost 1"); return; }
    bool msc = (2 == n);
    if ((msc && busy_msc) || (!msc && busy_aud)) { say("a test of this kind is still running"); return; }
    if (msc) { busy_msc = 1; } else { busy_aud = 1; }
    test_busy = true;
    xTaskCreatePinnedToCore(test_task, msc ? "usbh_msc" : "usbh_aud", 6144, (void *)(intptr_t)n, 4, nullptr, 1);
  }
#endif  // USBH_NEEDS_TRANSFERS
};

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

const char kUsbHostCommands[] PROGMEM = "UsbHost|" "|Fifo"
#ifdef USE_USB_HOST_MSC
  "|Msc"
#endif
#ifdef USBH_MSC_FS
  "|Mount"
#endif
#ifdef USE_USB_HOST_AUDIO
  "|Tone|Play|Route|Level"
#endif
#if defined(USE_USB_HOST_MSC) && defined(USE_USB_HOST_AUDIO)
  "|Stress"
#endif
#ifdef USE_USB_HOST_SERIAL
  "|Serial|SerialRead|SerialWrite|Bridge"
#endif
  ;

void (* const UsbHostCommand[])(void) PROGMEM = {
  &CmndUsbHost, &CmndUsbHostFifo,
#ifdef USE_USB_HOST_MSC
  &CmndUsbHostMsc,
#endif
#ifdef USBH_MSC_FS
  &CmndUsbHostMount,
#endif
#ifdef USE_USB_HOST_AUDIO
  &CmndUsbHostTone, &CmndUsbHostPlay, &CmndUsbHostRoute, &CmndUsbHostLevel,
#endif
#if defined(USE_USB_HOST_MSC) && defined(USE_USB_HOST_AUDIO)
  &CmndUsbHostStress,
#endif
#ifdef USE_USB_HOST_SERIAL
  &CmndUsbHostSerial, &CmndUsbHostSerialRead, &CmndUsbHostSerialWrite, &CmndUsbHostBridge,
#endif
};

// {"UsbHost":{"Running":1,"Busy":0,"Devices":[{"Vid":"08EC","Pid":"0008","Msc":1,"Audio":0}, ...]}}
void UsbHostResponse(void) {
  Response_P(PSTR("{\"%s\":{\"Running\":%d,\"Busy\":%d,\"Devices\":["), XdrvMailbox.command, UsbHostS::running ? 1 : 0, UsbHostS::test_busy ? 1 : 0);
  for (int i = 0; i < UsbHostS::nslots; i++) {
    const UsbHostS::Slot &sl = UsbHostS::slots[i];
    int msc = 0, aud = 0;
#ifdef USE_USB_HOST_MSC
    msc = (sl.msc_if >= 0 && sl.msc_in && sl.msc_out) ? 1 : 0;
#endif
#ifdef USE_USB_HOST_AUDIO
    aud = (sl.as_if >= 0) ? 1 : 0;
#endif
    ResponseAppend_P(PSTR("%s{\"Vid\":\"%04X\",\"Pid\":\"%04X\",\"Msc\":%d,\"Audio\":%d}"), i ? "," : "", sl.vid, sl.pid, msc, aud);
  }
  ResponseAppend_P(PSTR("]}}"));
}

void CmndUsbHost(void) {
  if (XdrvMailbox.data_len > 0) {
    if (0 == XdrvMailbox.payload) { UsbHostS::stop(); }
    else if (1 == XdrvMailbox.payload) { UsbHostS::start(); }
  }
  UsbHostResponse();
}

// UsbHostFifo rx,nptx,ptx : stop, set the FIFO split, start again
void CmndUsbHostFifo(void) {
  int a = 0, b = 0, c = 0;
  if ((XdrvMailbox.data_len > 0) && (3 == sscanf(XdrvMailbox.data, "%d,%d,%d", &a, &b, &c))) {
    UsbHostS::stop();
    UsbHostS::f_rx = a; UsbHostS::f_nptx = b; UsbHostS::f_ptx = c;
    UsbHostS::start();
  }
  UsbHostResponse();
}

#ifdef USE_USB_HOST_MSC
void CmndUsbHostMsc(void) {
  UsbHostS::msc_reads = 128;
  UsbHostS::run_test(2);
  UsbHostResponse();
}
#endif

#ifdef USBH_MSC_FS
// UsbHostMount 1 mounts the first stick as /usb (FAT), 0 unmounts. Answer: {"UsbHostMount":{"Mounted":1,"TotalMB":983,"FreeMB":900}}
void CmndUsbHostMount(void) {
  if (XdrvMailbox.data_len > 0) {
    if (1 == XdrvMailbox.payload) { xTaskCreatePinnedToCore(UsbHostS::mount_task, "usbh_mnt", 6144, nullptr, 3, nullptr, 1); }   // takes a second: own task
    else if (0 == XdrvMailbox.payload) { UsbHostS::msc_unmount(); }
  }
  uint64_t total = 0, freeb = 0;
  if (UsbHostS::msc_mounted) { esp_vfs_fat_info("/usb", &total, &freeb); }
  Response_P(PSTR("{\"%s\":{\"Mounted\":%d,\"TotalMB\":%u,\"FreeMB\":%u}}"), XdrvMailbox.command,
             UsbHostS::msc_mounted ? 1 : 0, (unsigned)(total >> 20), (unsigned)(freeb >> 20));
}
#endif

#ifdef USE_USB_HOST_AUDIO
// UsbHostRoute 1: the audio plugin (I2SAUDIO: MP3, web radio, TTS, say time) plays on the USB audio stick instead of the I2S
// output, while it plays; 0: back to I2S. UsbHostLevel 1..100: volume of the stick for the streams (TinyC i2s and the plugin)
void UsbHostAudioStateResponse(void) {
  Response_P(PSTR("{\"%s\":{\"Route\":%d,\"Level\":%d,\"Open\":%d,\"Underruns\":%u}}"), XdrvMailbox.command,
             UsbHostS::st_route ? 1 : 0, UsbHostS::st_level, UsbHostS::st_open ? 1 : 0, (unsigned)UsbHostS::st_under);
}
void CmndUsbHostRoute(void) {
  if (XdrvMailbox.data_len > 0) { UsbHostS::st_route = (XdrvMailbox.payload != 0); }
  UsbHostAudioStateResponse();
}
void CmndUsbHostLevel(void) {
  if (XdrvMailbox.payload >= 1 && XdrvMailbox.payload <= 100) { UsbHostS::st_level = XdrvMailbox.payload; }
  UsbHostAudioStateResponse();
}

void CmndUsbHostTone(void) {
  UsbHostS::run_test(3);
  UsbHostResponse();
}

// UsbHostPlay <path> : path of a 16 bit PCM WAV file in flash or on the card; the leading '/' is optional
void CmndUsbHostPlay(void) {
  if (XdrvMailbox.data_len > 0) {
    snprintf(UsbHostS::wav_path, sizeof(UsbHostS::wav_path), "%s%s", ('/' == XdrvMailbox.data[0]) ? "" : "/", XdrvMailbox.data);
    UsbHostS::run_test(4);
  }
  UsbHostResponse();
}
#endif

#if defined(USE_USB_HOST_MSC) && defined(USE_USB_HOST_AUDIO)
// UsbHostStress <path> : play the WAV and read the stick at the same time
void CmndUsbHostStress(void) {
  if (XdrvMailbox.data_len > 0) {
    snprintf(UsbHostS::wav_path, sizeof(UsbHostS::wav_path), "%s%s", ('/' == XdrvMailbox.data[0]) ? "" : "/", XdrvMailbox.data);
    UsbHostS::msc_reads = 400;
    UsbHostS::run_test(4);
    UsbHostS::run_test(2);
  }
  UsbHostResponse();
}
#endif


#ifdef USE_USB_HOST_AUDIO
// Plain-type interface for TinyC i2sBegin/i2sWrite/i2sStop with dout pin -2 (xdrv_124_tinyc_vm.h calls these)
int UsbHostAudioOpen(int rate) { return UsbHostS::stream_open(rate); }
int UsbHostAudioWrite(const int16_t *x, int n) { return UsbHostS::stream_write(x, n, 1); }
int UsbHostAudioWriteCh(const int16_t *x, int frames, int ch) { return UsbHostS::stream_write(x, frames, ch); }
void UsbHostAudioSetRate(int rate) { UsbHostS::stream_set_rate(rate); }
bool UsbHostAudioActive(void) { return UsbHostS::st_open; }
bool UsbHostAudioRoute(void) { return UsbHostS::st_route; }
void UsbHostAudioClose(void) { UsbHostS::stream_close(); }
#endif

#ifdef USE_USB_HOST_SERIAL
// Plain-type interface for the TinyC functions usbInit/usbOpen/usbRead/usbWrite (xdrv_124_tinyc_usb.h calls these)
bool UsbHostSerInit(void) { return UsbHostS::start(); }
bool UsbHostSerOpen(uint32_t baud) { return UsbHostS::ser_open(baud); }
void UsbHostSerClose(void) { UsbHostS::ser_close(); }
uint16_t UsbHostSerAvail(void) { return UsbHostS::ring ? UsbHostS::ring_avail() : 0; }
uint16_t UsbHostSerPull(uint8_t *to, uint16_t max) { return UsbHostS::ring_pull(to, max); }
int32_t UsbHostSerWrite(const uint8_t *d, uint16_t len) { return UsbHostS::ser_write(d, len); }
uint32_t UsbHostSerGet(uint8_t which) { return UsbHostS::ser_get(which); }

void UsbHostSerialResponse(void) {
  Response_P(PSTR("{\"%s\":{\"State\":%u,\"Vid\":\"%04X\",\"Pid\":\"%04X\",\"Rx\":%u,\"Tx\":%u,\"Lost\":%u,\"Baud\":%u}}"), XdrvMailbox.command,
             (unsigned)UsbHostS::ser_get(0), (unsigned)UsbHostS::ser_get(1), (unsigned)UsbHostS::ser_get(2),
             (unsigned)UsbHostS::s_rx, (unsigned)UsbHostS::s_tx, (unsigned)UsbHostS::s_lost, (unsigned)UsbHostS::s_baud);
}

// UsbHostSerial <baud> opens (starts the host if needed), 0 closes. Without a FTDI attached yet it fails; try again after the plug-in.
void CmndUsbHostSerial(void) {
  if (XdrvMailbox.data_len > 0) {
    if (XdrvMailbox.payload > 0) { UsbHostS::ser_open((uint32_t)XdrvMailbox.payload); }
    else if (0 == XdrvMailbox.payload) { UsbHostS::ser_close(); }
  }
  UsbHostSerialResponse();
}

void CmndUsbHostSerialRead(void) {
  int want = (XdrvMailbox.payload > 0) ? XdrvMailbox.payload : 32;
  if (want > 100) { want = 100; }
  uint8_t b[100];
  int n = UsbHostS::ring_pull(b, want);
  Response_P(PSTR("{\"%s\":{\"Count\":%d,\"Hex\":\""), XdrvMailbox.command, n);
  for (int i = 0; i < n; i++) { ResponseAppend_P(PSTR("%02X"), b[i]); }
  ResponseAppend_P(PSTR("\"}}"));
}

// UsbHostSerialWrite 41 42 43 : sends these bytes (hex, spaces allowed)
void CmndUsbHostSerialWrite(void) {
  uint8_t b[100]; int n = 0, hi = -1;
  for (const char *p = XdrvMailbox.data; p && *p && n < (int)sizeof(b); p++) {
    int v = (*p >= '0' && *p <= '9') ? *p - '0' : (*p >= 'a' && *p <= 'f') ? *p - 'a' + 10 : (*p >= 'A' && *p <= 'F') ? *p - 'A' + 10 : -1;
    if (v < 0) { continue; }
    if (hi < 0) { hi = v; } else { b[n++] = (hi << 4) | v; hi = -1; }
  }
  int32_t w = n ? UsbHostS::ser_write(b, n) : 0;
  Response_P(PSTR("{\"%s\":{\"Sent\":%d}}"), XdrvMailbox.command, (int)w);
}

// UsbHostBridge <port> : TCP server, bytes both ways between the client and the open FTDI; 0 stops
void CmndUsbHostBridge(void) {
  if (XdrvMailbox.data_len > 0) {
    if (XdrvMailbox.payload > 0 && XdrvMailbox.payload < 65536) { UsbHostS::bridge_start((uint16_t)XdrvMailbox.payload); }
    else if (0 == XdrvMailbox.payload) { UsbHostS::bridge_stop(); }
  }
  Response_P(PSTR("{\"%s\":{\"Running\":%d,\"Port\":%u}}"), XdrvMailbox.command, UsbHostS::br_run ? 1 : 0, (unsigned)UsbHostS::br_port);
}
#endif  // USE_USB_HOST_SERIAL

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv99(uint32_t function) {
  bool result = false;
  switch (function) {
    case FUNC_EVERY_250_MSECOND:
      UsbHostS::drain();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand(kUsbHostCommands, UsbHostCommand);
      break;
#ifdef USE_USB_HOST_AUTOSTART
    case FUNC_INIT:
      UsbHostS::start();
      break;
#endif
  }
  return result;
}

#else
#warning "USE_USB_HOST: this chip has no USB host in the SDK (only ESP32-S2/S3 are released here, P4 is planned) - the USB host driver is left out"
#endif  // ESP32S3 / S2
#endif  // USE_USB_HOST
