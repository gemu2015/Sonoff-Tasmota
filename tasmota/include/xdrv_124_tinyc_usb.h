/*
  xdrv_124_tinyc_usb.h — an FTDI USB-serial adapter as a TinyC primitive.

  Gives a TinyC script a SERIAL link to any device that hangs on the ESP32's USB
  host port behind an FTDI chip. The protocol on top lives in the script, exactly
  like the SPP family: the same primitive serves a VarioLab, a GPS mouse, an
  inverter or a laboratory instrument, and it changes without reflashing.

  The reason it exists: gemu's VarioLab has its FTDI soldered in and no way to
  reach the UART before it, so the only way in is to BE a USB host (12.09.2026).

  ---------------------------------------------------------------------------------------
  ESP32-S3 / S2 ONLY, AND ONLY WITH TWO USB PORTS.
  ---------------------------------------------------------------------------------------
  USB host needs the OTG peripheral, which the original ESP32 does not have at all
  and the C3/C6 have only as a device. On the S3 the host uses GPIO19/20 — the same
  lines as the native USB. A board with ONE socket must therefore give up the USB
  console (ARDUINO_USB_CDC_ON_BOOT) to be a host. gemu's devkit has USB-COM *and*
  USB-OTG, so the console stays where it is and the OTG socket is free.

  ⚠️ The host must supply VBUS. If nothing enumerates although the cable is right,
  that is the first thing to measure — many OTG sockets have no 5 V switch.

  ⭐ NOTHING HAD TO BE ADDED TO THE FRAMEWORK. Tasmota's own arduino-esp32 branch
  (3.3.8 on ESP-IDF 5.5) already ships libusb.a for the S3 with usb_host_install,
  usb_host_client_register, usb_host_lib_handle_events and usb_host_transfer_alloc
  exported, plus usb/usb_host.h and the CONFIG_USB_HOST_* settings. Verified by
  linking and running on 12.09.2026: the VarioLab enumerated as 0403:6001 (FT232R).

  ---------------------------------------------------------------------------------------
  WHY NO CDC-ACM COMPONENT.
  ---------------------------------------------------------------------------------------
  An FTDI is NOT a CDC device — it is vendor-specific, so no class driver adopts it
  anyway. Espressif's usb_host_cdc_acm + usb_host_ftdi_vcp would do it, but that is
  over 70 KB of source with its own component layout that does not fit PlatformIO's
  library folder. What an FT232 actually needs is four control requests and one bulk
  pair, and that is what is below.

  ⚠️ THE ONE FTDI ODDITY: every IN packet starts with TWO STATUS BYTES (modem and
  line status). They are stripped in the completion callback. Forget that and the
  data has two bytes of rubbish every 62 bytes — which looks like a baud rate error
  and sends you hunting in the wrong place.

  ---------------------------------------------------------------------------------------
  READS DO NOT BLOCK — same rule as SPP.
  ---------------------------------------------------------------------------------------
  usbRead() returns what has arrived and comes back at once. The script waits by
  itself and keeps control; firmware that waited would hang the VM on the peer's
  timeouts.
*/

#ifndef _XDRV_124_TINYC_USB_H_
#define _XDRV_124_TINYC_USB_H_

#if defined(USE_TINYC_USBSERIAL) && (defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2))

#include "usb/usb_host.h"
#include "freertos/semphr.h"

// Zustände, wie sie usbState() meldet — bewusst dieselbe Staffelung wie bei SPP.
#define TC_USB_AUS      0   // Stapel nicht angemeldet
#define TC_USB_BEREIT   1   // Host läuft, kein Gerät
#define TC_USB_GERAET   2   // FTDI erkannt, noch nicht geöffnet
#define TC_USB_OFFEN    3   // offen, Daten fliessen
#define TC_USB_FEHLER   4   // Öffnen ist fehlgeschlagen

// ⚠️ GROSS GENUG FÜR EINEN WLAN-AUSSETZER. Bei 230400 Bd laufen 23 kB/s
// herein; 2048 Byte waren damit 89 ms Vorrat, und genau so lange darf das
// Skript dann nicht ins Stocken geraten. Am 12.09.2026 gemessen: bei 17,7 kB/s
// aus einem echten VarioLab gingen 3 Byte verloren — für dieses Protokoll
// heisst das ein verschobener Satz, nicht ein fehlender Wert. 16 kB sind
// 0,7 s Vorrat und kosten im Verhältnis nichts: der S3 hat 8 MB PSRAM frei.
#define TC_USB_RING     16384   // Empfangsring
#define TC_USB_MPS      64      // volle Geschwindigkeit, Bulk
#define TC_USB_IN_N     4       // gleichzeitig laufende IN-Übertragungen

// FTDI-Steuerbefehle (herstellereigen, bmRequestType 0x40)
#define FTDI_RESET          0x00
#define FTDI_MODEM_CTRL     0x01
#define FTDI_SET_FLOW       0x02
#define FTDI_SET_BAUD       0x03
#define FTDI_SET_DATA       0x04

struct TcUsbLage {
  usb_host_client_handle_t klient = nullptr;
  usb_device_handle_t      geraet = nullptr;
  TaskHandle_t             aufgabe = nullptr;
  SemaphoreHandle_t        ctrl_fertig = nullptr;
  SemaphoreHandle_t        out_fertig = nullptr;
  usb_transfer_t          *ctrl = nullptr;
  usb_transfer_t          *out = nullptr;
  usb_transfer_t          *in[TC_USB_IN_N] = { nullptr };
  uint8_t   ep_in = 0, ep_out = 0, schnittstelle = 0;
  volatile uint8_t  state = TC_USB_AUS;
  volatile bool     weiter = false;
  volatile bool     angesteckt = false;   // Gerät liegt an, muss geöffnet werden
  uint8_t   adresse = 0;
  uint16_t  vid = 0, pid = 0;
  uint32_t  baud = 0;
  volatile uint32_t rx = 0, tx = 0, verloren = 0;
  // Ring
  volatile uint16_t kopf = 0, fuss = 0;
  uint8_t   ring[TC_USB_RING];
  portMUX_TYPE sperre = portMUX_INITIALIZER_UNLOCKED;
} TcUsb;

/*───────────────────────── Ringpuffer ─────────────────────────────────────*/
static inline uint16_t TcUsbAvailable(void) {
  uint16_t k = TcUsb.kopf, f = TcUsb.fuss;
  return (k >= f) ? (k - f) : (uint16_t)(TC_USB_RING - f + k);
}
static void TcUsbPush(const uint8_t *d, uint16_t len) {
  taskENTER_CRITICAL(&TcUsb.sperre);
  for (uint16_t i = 0; i < len; i++) {
    uint16_t neu = (TcUsb.kopf + 1) % TC_USB_RING;
    if (neu == TcUsb.fuss) { TcUsb.verloren++; break; }   // voll — ältere behalten
    TcUsb.ring[TcUsb.kopf] = d[i];
    TcUsb.kopf = neu;
  }
  taskEXIT_CRITICAL(&TcUsb.sperre);
}
static uint16_t TcUsbPull(uint8_t *ziel, uint16_t max) {
  uint16_t n = 0;
  taskENTER_CRITICAL(&TcUsb.sperre);
  while (n < max && TcUsb.fuss != TcUsb.kopf) {
    ziel[n++] = TcUsb.ring[TcUsb.fuss];
    TcUsb.fuss = (TcUsb.fuss + 1) % TC_USB_RING;
  }
  taskEXIT_CRITICAL(&TcUsb.sperre);
  return n;
}

/*───────────────────────── Übertragungen ──────────────────────────────────*/
// ⚠️ Alle Rückrufe laufen in der USB-Aufgabe. Hier wird NICHT gerechnet und
// nicht geloggt, nur abgelegt und weitergereicht.
struct TcUsbTeile {

  static void in_fertig(usb_transfer_t *t) {
    if (USB_TRANSFER_STATUS_COMPLETED == t->status && t->actual_num_bytes > 2) {
      // ⚠️ DIE ERSTEN ZWEI BYTES SIND MODEM- UND LEITUNGSSTATUS, KEINE DATEN.
      TcUsbPush(t->data_buffer + 2, (uint16_t)(t->actual_num_bytes - 2));
      TcUsb.rx += (t->actual_num_bytes - 2);
    }
    if (TcUsb.weiter && TcUsb.geraet) { usb_host_transfer_submit(t); }
  }

  static void out_fertig(usb_transfer_t *t) {
    if (TcUsb.out_fertig) { xSemaphoreGive(TcUsb.out_fertig); }
  }

  static void ctrl_fertig(usb_transfer_t *t) {
    if (TcUsb.ctrl_fertig) { xSemaphoreGive(TcUsb.ctrl_fertig); }
  }

  static void ereignis(const usb_host_client_event_msg_t *msg, void *arg) {
    if (USB_HOST_CLIENT_EVENT_NEW_DEV == msg->event) {
      usb_device_handle_t g;
      if (usb_host_device_open(TcUsb.klient, msg->new_dev.address, &g) != ESP_OK) { return; }
      const usb_device_desc_t *d = nullptr;
      if (usb_host_get_device_descriptor(g, &d) == ESP_OK && d) {
        if (0x0403 == d->idVendor) {              // FTDI
          TcUsb.geraet = g;
          TcUsb.adresse = msg->new_dev.address;
          TcUsb.vid = d->idVendor;
          TcUsb.pid = d->idProduct;
          TcUsb.angesteckt = true;                // das Öffnen macht usbOpen()
          if (TC_USB_OFFEN != TcUsb.state) { TcUsb.state = TC_USB_GERAET; }
          return;                                 // offen lassen!
        }
      }
      usb_host_device_close(TcUsb.klient, g);     // fremdes Gerät
    }
    else if (USB_HOST_CLIENT_EVENT_DEV_GONE == msg->event) {
      TcUsb.angesteckt = false;
      TcUsb.state = TC_USB_BEREIT;
      TcUsb.geraet = nullptr;                     // aufgeräumt wird in usbClose()
    }
  }

  static void aufgabe(void *arg) {
    while (TcUsb.weiter) {
      uint32_t flags;
      // ⚠️⚠️ HIER NICHT WARTEN — das war die Bremse der ganzen Bruecke.
      //
      // Bibliotheksereignisse sind An- und Abstecken, also Sekunden
      // auseinander. Wer hier 50 ms blockiert, haelt damit die
      // UEBERTRAGUNGEN auf, denn die werden in DERSELBEN Aufgabe abgefertigt:
      // je Runde kommt nur ab, was gerade fertig ist.
      //
      // Gemessen am 12.09.2026 gegen ein VarioLab an 230400 Bd:
      //   Empfangen  4 IN-Uebertragungen a 62 B je Runde = 4,96 kB/s
      //   Senden     1 OUT-Uebertragung a 64 B je Runde  = 1,2 kB/s
      // Beides sind Rechenergebnisse der 50-ms-Runde, nicht der Leitung —
      // die traegt 23 kB/s. Die Zahlen stimmten auf das Byte, und genau
      // daran war der Fehler zu erkennen.
      //
      // Gewartet wird jetzt dort, wo die Ereignisse anfallen: im
      // Klienten-Handler, der zurueckkehrt, SOBALD eines da ist.
      usb_host_lib_handle_events(0, &flags);
      if (TcUsb.klient) { usb_host_client_handle_events(TcUsb.klient, pdMS_TO_TICKS(50)); }
      else              { vTaskDelay(pdMS_TO_TICKS(5)); }   // ohne Klient nicht drehen
    }
    TcUsb.aufgabe = nullptr;
    vTaskDelete(nullptr);
  }
};

/*───────────────────────── Steuerbefehle an den FTDI ──────────────────────*/
static bool TcUsbCtrl(uint8_t befehl, uint16_t wert, uint16_t index) {
  if (!TcUsb.geraet || !TcUsb.ctrl) { return false; }
  usb_setup_packet_t *s = (usb_setup_packet_t *)TcUsb.ctrl->data_buffer;
  s->bmRequestType = 0x40;                  // heraus, herstellereigen, Gerät
  s->bRequest = befehl;
  s->wValue = wert;
  s->wIndex = index;
  s->wLength = 0;
  TcUsb.ctrl->num_bytes = sizeof(usb_setup_packet_t);
  TcUsb.ctrl->device_handle = TcUsb.geraet;
  TcUsb.ctrl->bEndpointAddress = 0;
  TcUsb.ctrl->callback = TcUsbTeile::ctrl_fertig;
  TcUsb.ctrl->context = nullptr;
  if (usb_host_transfer_submit_control(TcUsb.klient, TcUsb.ctrl) != ESP_OK) { return false; }
  if (xSemaphoreTake(TcUsb.ctrl_fertig, pdMS_TO_TICKS(500)) != pdTRUE) { return false; }
  return (USB_TRANSFER_STATUS_COMPLETED == TcUsb.ctrl->status);
}

/*  Der Teiler des FT232 auf 3 MHz Grundtakt, mit den drei Bruchbits.
    ⚠️ Die Zuordnung der Achtel ist NICHT fortlaufend — sie steht so im
    Datenblatt und im Linux-Treiber. 230400 ergibt Teiler 13 und damit
    230769 Baud, also 0,16 % daneben; das tun alle Treiber so.              */
static uint32_t TcUsbTeiler(uint32_t baud) {
  static const uint8_t bruch[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
  if (!baud) { return 0; }
  uint32_t mal8 = (3000000UL * 8) / baud;
  uint32_t ganz = mal8 >> 3;
  uint32_t rest = mal8 & 7;
  uint32_t code = ganz | ((uint32_t)bruch[rest] << 14);
  if (1 == code)      { code = 0; }         // 2 MBaud
  else if (0x4001 == code) { code = 1; }    // 3 MBaud
  return code;
}

static bool TcUsbBaud(uint32_t baud) {
  uint32_t code = TcUsbTeiler(baud);
  if (!TcUsbCtrl(FTDI_SET_BAUD, (uint16_t)(code & 0xFFFF), (uint16_t)(code >> 16))) { return false; }
  TcUsb.baud = baud;
  return true;
}

/*───────────────────────── Anmelden und Abbauen ───────────────────────────*/
static bool TcUsbInit(void) {
  if (TcUsb.state != TC_USB_AUS) { return true; }
  const usb_host_config_t hk = { .skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1 };
  esp_err_t e = usb_host_install(&hk);
  if (e != ESP_OK) {
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbInit — usb_host_install %s"), esp_err_to_name(e));
    return false;
  }
  const usb_host_client_config_t kk = {
    .is_synchronous = false,
    .max_num_event_msg = 5,
    .async = { .client_event_callback = TcUsbTeile::ereignis, .callback_arg = nullptr },
  };
  if (usb_host_client_register(&kk, &TcUsb.klient) != ESP_OK) {
    usb_host_uninstall();
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbInit — client_register fehlgeschlagen"));
    return false;
  }
  TcUsb.ctrl_fertig = xSemaphoreCreateBinary();
  TcUsb.out_fertig  = xSemaphoreCreateBinary();
  TcUsb.weiter = true;
  if (xTaskCreatePinnedToCore(TcUsbTeile::aufgabe, "tc_usb", 4096, nullptr, 5, &TcUsb.aufgabe, 0) != pdPASS) {
    TcUsb.weiter = false;
    usb_host_client_deregister(TcUsb.klient); TcUsb.klient = nullptr;
    usb_host_uninstall();
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbInit — Aufgabe liess sich nicht anlegen"));
    return false;
  }
  TcUsb.state = TC_USB_BEREIT;
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbInit — Host laeuft, warte auf FTDI"));
  return true;
}

static void TcUsbClose(void) {
  TcUsb.weiter = false;                  // stoppt das Nachladen der IN-Übertragungen
  delay(60);
  for (int i = 0; i < TC_USB_IN_N; i++) {
    if (TcUsb.in[i]) { usb_host_transfer_free(TcUsb.in[i]); TcUsb.in[i] = nullptr; }
  }
  if (TcUsb.out)  { usb_host_transfer_free(TcUsb.out);  TcUsb.out = nullptr; }
  if (TcUsb.ctrl) { usb_host_transfer_free(TcUsb.ctrl); TcUsb.ctrl = nullptr; }
  if (TcUsb.geraet) {
    usb_host_interface_release(TcUsb.klient, TcUsb.geraet, TcUsb.schnittstelle);
    usb_host_device_close(TcUsb.klient, TcUsb.geraet);
    TcUsb.geraet = nullptr;
  }
  TcUsb.weiter = true;                   // die Aufgabe läuft weiter
  TcUsb.kopf = TcUsb.fuss = 0;
  if (TcUsb.state != TC_USB_AUS) { TcUsb.state = TC_USB_BEREIT; }
}

static void TcUsbDeinit(void) {
  if (TC_USB_AUS == TcUsb.state) { return; }
  TcUsbClose();
  TcUsb.weiter = false;
  delay(150);                            // der Aufgabe Zeit zum Aussteigen
  if (TcUsb.klient) { usb_host_client_deregister(TcUsb.klient); TcUsb.klient = nullptr; }
  usb_host_uninstall();
  if (TcUsb.ctrl_fertig) { vSemaphoreDelete(TcUsb.ctrl_fertig); TcUsb.ctrl_fertig = nullptr; }
  if (TcUsb.out_fertig)  { vSemaphoreDelete(TcUsb.out_fertig);  TcUsb.out_fertig = nullptr; }
  TcUsb.state = TC_USB_AUS;
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbDeinit — Stapel abgebaut"));
}

/*───────────────────────── Öffnen ─────────────────────────────────────────*/
/*  Sucht in der aktiven Einstellung die erste Schnittstelle mit einem
    Bulk-Paar, belegt sie und stellt 8N1 ohne Flusssteuerung ein.          */
static bool TcUsbOpen(uint32_t baud) {
  if (!TcUsbInit()) { return false; }
  if (TC_USB_OFFEN == TcUsb.state) { return TcUsbBaud(baud); }
  if (!TcUsb.geraet) { return false; }                  // noch nichts angesteckt

  const usb_config_desc_t *cfg = nullptr;
  if (usb_host_get_active_config_descriptor(TcUsb.geraet, &cfg) != ESP_OK || !cfg) { return false; }

  int off = 0;
  TcUsb.ep_in = TcUsb.ep_out = 0;
  const usb_intf_desc_t *intf = usb_parse_interface_descriptor(cfg, 0, 0, &off);
  if (!intf) { return false; }
  TcUsb.schnittstelle = intf->bInterfaceNumber;
  for (int i = 0; i < intf->bNumEndpoints; i++) {
    int eoff = off;
    const usb_ep_desc_t *ep = usb_parse_endpoint_descriptor_by_index(intf, i, cfg->wTotalLength, &eoff);
    if (!ep) { continue; }
    if (USB_BM_ATTRIBUTES_XFER_BULK != (ep->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK)) { continue; }
    if (ep->bEndpointAddress & 0x80) { TcUsb.ep_in = ep->bEndpointAddress; }
    else                             { TcUsb.ep_out = ep->bEndpointAddress; }
  }
  if (!TcUsb.ep_in || !TcUsb.ep_out) {
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbOpen — kein Bulk-Paar gefunden"));
    TcUsb.state = TC_USB_FEHLER;
    return false;
  }
  if (usb_host_interface_claim(TcUsb.klient, TcUsb.geraet, TcUsb.schnittstelle, 0) != ESP_OK) {
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbOpen — Schnittstelle nicht zu belegen"));
    TcUsb.state = TC_USB_FEHLER;
    return false;
  }

  usb_host_transfer_alloc(sizeof(usb_setup_packet_t) + 8, 0, &TcUsb.ctrl);
  usb_host_transfer_alloc(TC_USB_MPS, 0, &TcUsb.out);
  if (!TcUsb.ctrl || !TcUsb.out) { TcUsbClose(); return false; }

  // Reset, 8 Datenbits ohne Parität mit einem Stoppbit, keine Flusssteuerung.
  TcUsbCtrl(FTDI_RESET, 0, 0);
  if (!TcUsbBaud(baud)) {
    AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbOpen — Baudrate wurde nicht angenommen"));
    TcUsbClose(); TcUsb.state = TC_USB_FEHLER; return false;
  }
  TcUsbCtrl(FTDI_SET_DATA, 0x0008, 0);
  TcUsbCtrl(FTDI_SET_FLOW, 0, 0);
  TcUsbCtrl(FTDI_MODEM_CTRL, 0x0303, 0);      // DTR und RTS an

  for (int i = 0; i < TC_USB_IN_N; i++) {
    if (usb_host_transfer_alloc(TC_USB_MPS, 0, &TcUsb.in[i]) != ESP_OK) { break; }
    TcUsb.in[i]->device_handle = TcUsb.geraet;
    TcUsb.in[i]->bEndpointAddress = TcUsb.ep_in;
    TcUsb.in[i]->callback = TcUsbTeile::in_fertig;
    TcUsb.in[i]->context = nullptr;
    TcUsb.in[i]->num_bytes = TC_USB_MPS;
    usb_host_transfer_submit(TcUsb.in[i]);
  }
  TcUsb.state = TC_USB_OFFEN;
  AddLog(LOG_LEVEL_INFO, PSTR("TCC: usbOpen — %04X:%04X offen, %u Baud, EP %02X/%02X"),
         TcUsb.vid, TcUsb.pid, (unsigned)baud, TcUsb.ep_in, TcUsb.ep_out);
  return true;
}

/*───────────────────────── Senden ─────────────────────────────────────────*/
static int32_t TcUsbWrite(const uint8_t *d, uint16_t len) {
  if (TC_USB_OFFEN != TcUsb.state || !TcUsb.out) { return -1; }
  uint16_t gesamt = 0;
  while (gesamt < len) {
    uint16_t teil = len - gesamt;
    if (teil > TC_USB_MPS) { teil = TC_USB_MPS; }
    memcpy(TcUsb.out->data_buffer, d + gesamt, teil);
    TcUsb.out->device_handle = TcUsb.geraet;
    TcUsb.out->bEndpointAddress = TcUsb.ep_out;
    TcUsb.out->callback = TcUsbTeile::out_fertig;
    TcUsb.out->context = nullptr;
    TcUsb.out->num_bytes = teil;
    if (usb_host_transfer_submit(TcUsb.out) != ESP_OK) { return gesamt ? gesamt : -1; }
    if (xSemaphoreTake(TcUsb.out_fertig, pdMS_TO_TICKS(300)) != pdTRUE) { return gesamt ? gesamt : -1; }
    if (USB_TRANSFER_STATUS_COMPLETED != TcUsb.out->status) { return gesamt ? gesamt : -1; }
    gesamt += teil;
    TcUsb.tx += teil;
  }
  return gesamt;
}

#endif  // USE_TINYC_USBSERIAL && S3/S2
#endif  // _XDRV_124_TINYC_USB_H_
