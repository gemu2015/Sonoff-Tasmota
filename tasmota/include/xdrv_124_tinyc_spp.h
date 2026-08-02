/*
  xdrv_124_tinyc_spp.h — Bluetooth Classic (RFCOMM/SPP) as a TinyC primitive.

  Provides a SERIAL link to any Bluetooth Classic device. The protocol on top is
  written in the TinyC script, not in the firmware, so the same primitive serves SMA
  inverters, OBD adapters, scales, receipt printers and anything else that speaks SPP
  -- and it can be changed without reflashing.

  ---------------------------------------------------------------------------------------
  ORIGINAL ESP32 ONLY.
  ---------------------------------------------------------------------------------------
  Bluetooth Classic (BR/EDR) exists only on the original ESP32 (D0WD and friends).
  S3, C3, C6 and P4 are BLE-only, so this whole section is compiled out there.

  Tasmota also ships an Arduino framework precompiled with NimBLE, which contains NO
  Classic headers at all. An environment using USE_TINYC_SPP must therefore have the
  framework rebuilt with Bluedroid (roughly 8 minutes) and needs the include paths
  added by hand:

      custom_sdkconfig = CONFIG_BT_ENABLED=y
                         CONFIG_BT_CONTROLLER_ENABLED=y
                         CONFIG_BT_BLUEDROID_ENABLED=y
                         '# CONFIG_BT_NIMBLE_ENABLED is not set'
                         CONFIG_BT_CLASSIC_ENABLED=y
                         CONFIG_BT_SPP_ENABLED=y
                         CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y
                         '# CONFIG_BTDM_CTRL_MODE_BLE_ONLY is not set'
                         '# CONFIG_BTDM_CTRL_MODE_BTDM is not set'
      build_flags      = ... -DUSE_TINYC_SPP
                         -I<packages>/framework-espidf/components/bt/host/bluedroid/api/include/api
                         -I<packages>/framework-espidf/components/bt/common/api/include/api
                         -I<packages>/framework-espidf/components/bt/include/esp32/include

  The rebuild refreshes the LIBRARIES but NOT the shipped include list
  (tools/esp32-arduino-libs/esp32/flags/includes stays behind and still names NimBLE).
  Without those three -I flags compilation dies on "esp_bt_main.h: No such file or
  directory" even though Bluedroid was built long ago. Costs about 510 KB of flash,
  and such a build has no NimBLE -- the two host stacks are mutually exclusive.

  The rebuild itself only happens when the hash in line 1 of <project>/sdkconfig.defaults
  ("# TASMOTA__<md5>") changes. If it stays the same, edited settings are ignored in
  SILENCE -- no "Replace:" lines in the build log. Force it by deleting that file and
  tools/esp32-arduino-libs/esp32/sdkconfig.

  ---------------------------------------------------------------------------------------
  READS DO NOT BLOCK.
  ---------------------------------------------------------------------------------------
  sppRead() returns whatever has arrived and comes back immediately. The script does
  its own waiting and stays in control. If the firmware waited instead, the VM would
  hang on the peer's timeouts -- exactly the failure that once wedged the main loop
  through httpGet/sendMail. Protocols with long waits belong on a worker (spawnTask),
  not in the main loop.

  The one exception is sppScan(), which waits out the requested inquiry time. Scanning
  is a rare, deliberate act, so simplicity wins there.
*/

#ifndef _XDRV_124_TINYC_SPP_H_
#define _XDRV_124_TINYC_SPP_H_

#ifdef USE_TINYC_SPP

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

// ---------------------------------------------------------------------------------------
// KEEP THE BT CONTROLLER MEMORY. Without this the whole driver is dead on arrival.
//
// The Arduino core frees the entire BT controller memory during startup:
//
//     if (!btInUse()) { esp_bt_controller_mem_release(ESP_BT_MODE_BTDM); }   (esp32-hal-misc.c)
//
// btInUse() is weak and only reports true when Arduino's own BluetoothSerial/BLE
// libraries are linked. We talk to the IDF API directly, so it returned false, ~36 KB
// of controller memory were released, and every later esp_bt_controller_init() failed
// with ESP_ERR_INVALID_STATE (0x103) -- while esp_bt_controller_get_status() still
// cheerfully reported IDLE. That combination (status IDLE + init INVALID_STATE) is the
// fingerprint of released BT memory; nothing in the log said so.
//
// esp32-hal-bt.c names the sanctioned remedy: "Users can also provide their own strong
// btInUse() implementation." So here it is.
extern "C" bool btInUse(void) { return true; }

#define TC_SPP_RX_BUF     2048     // receive ring; DRAM is tight, do not raise without
                                   // measuring (Bluedroid Classic is a heavy eater)
#define TC_SPP_SCAN_MAX   16       // remembered inquiry results

enum TcSppState : uint8_t {
  TC_SPP_AUS = 0, TC_SPP_BEREIT = 1, TC_SPP_VERBINDET = 2, TC_SPP_OFFEN = 3
};

struct TcSppFund { uint8_t addr[6]; char name[24]; };

struct {
  volatile uint8_t  state = TC_SPP_AUS;
  volatile uint32_t handle = 0;
  volatile bool     verbinde_fehlgeschlagen = false;

  // Ring buffer with ONE writer (the Bluedroid callback) and ONE reader (the VM).
  // Lock-free is safe as long as head and tail are each advanced by one side only.
  uint8_t  rx[TC_SPP_RX_BUF];
  volatile uint16_t kopf = 0, fuss = 0;
  volatile uint32_t verworfen = 0;   // bytes dropped because the buffer was full

  // Inquiry
  volatile bool     sucht = false;
  volatile uint8_t  fund_n = 0;
  TcSppFund fund[TC_SPP_SCAN_MAX];
} TcSpp;

/*********************************************************************************************\
 * Ring buffer
\*********************************************************************************************/

static inline uint16_t TcSppAvailable(void) {
  int32_t n = (int32_t)TcSpp.kopf - (int32_t)TcSpp.fuss;
  if (n < 0) { n += TC_SPP_RX_BUF; }
  return (uint16_t)n;
}

static void TcSppPush(const uint8_t *d, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    uint16_t nx = (TcSpp.kopf + 1) % TC_SPP_RX_BUF;
    if (nx == TcSpp.fuss) { TcSpp.verworfen += (len - i); return; }  // drop, never overwrite
    TcSpp.rx[TcSpp.kopf] = d[i];
    TcSpp.kopf = nx;
  }
}

static uint16_t TcSppPull(uint8_t *ziel, uint16_t max) {
  uint16_t n = 0;
  while (n < max && TcSpp.fuss != TcSpp.kopf) {
    ziel[n++] = TcSpp.rx[TcSpp.fuss];
    TcSpp.fuss = (TcSpp.fuss + 1) % TC_SPP_RX_BUF;
  }
  return n;
}

/*********************************************************************************************\
 * Callbacks -- these run in the Bluedroid task. Buffer only, compute nothing.
\*********************************************************************************************/

static void TcSppCb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  switch (event) {
    case ESP_SPP_INIT_EVT:
      TcSpp.state = TC_SPP_BEREIT;
      break;
    case ESP_SPP_OPEN_EVT:
      TcSpp.handle = param->open.handle;
      TcSpp.state = TC_SPP_OFFEN;
      AddLog(LOG_LEVEL_DEBUG, PSTR("SPP: channel open"));
      break;
    case ESP_SPP_CLOSE_EVT:
      TcSpp.handle = 0;
      TcSpp.state = TC_SPP_BEREIT;
      break;
    case ESP_SPP_DATA_IND_EVT:
      TcSppPush(param->data_ind.data, param->data_ind.len);
      break;
    case ESP_SPP_CL_INIT_EVT:
      // Connect attempt started. A failure arrives as CLOSE without a preceding OPEN,
      // so failure is detected through the state, not through a dedicated event.
      break;
    default:
      break;
  }
}

static void TcSppGapCb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
      if (TcSpp.fund_n >= TC_SPP_SCAN_MAX) { break; }
      // Already known? An inquiry reports the same device several times.
      for (uint8_t i = 0; i < TcSpp.fund_n; i++) {
        if (0 == memcmp(TcSpp.fund[i].addr, param->disc_res.bda, 6)) { return; }
      }
      TcSppFund *f = &TcSpp.fund[TcSpp.fund_n];
      memcpy(f->addr, param->disc_res.bda, 6);
      f->name[0] = '\0';
      for (int i = 0; i < param->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = param->disc_res.prop + i;
        if (ESP_BT_GAP_DEV_PROP_BDNAME == p->type) {
          int l = p->len; if (l > (int)sizeof(f->name) - 1) { l = sizeof(f->name) - 1; }
          memcpy(f->name, p->val, l); f->name[l] = '\0';
        }
      }
      TcSpp.fund_n++;
      break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
      TcSpp.sucht = (ESP_BT_GAP_DISCOVERY_STARTED == param->disc_st_chg.state);
      break;
    default:
      break;
  }
}

/*********************************************************************************************\
 * Bring-up and connect
\*********************************************************************************************/

// Bring up the Bluedroid Classic stack. Safe to call repeatedly.
//
// EVERY step logs its return code, including the successful ones. An earlier version
// treated ESP_ERR_INVALID_STATE as harmless on the init/enable calls -- that MASKED the
// first real failure and made a later call (gap_register) look like the culprit.
// Log first, judge second.
//
// Note esp_err_to_name(0) prints "UNKNOWN ERROR" in this build: 0x0 IS success.
static bool TcSppInit(void) {
  if (TcSpp.state != TC_SPP_AUS) { return true; }
  esp_err_t r;

  AddLog(LOG_LEVEL_INFO, PSTR("SPP: status=%d (0=idle 1=inited 2=enabled) heap=%u largest=%u"),
         (int)esp_bt_controller_get_status(),
         (unsigned)ESP_getFreeHeap(),
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: cfg mode=%d"), (int)cfg.mode);

  r = esp_bt_controller_init(&cfg);
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 1 controller_init  -> 0x%x %s (status now %d)"),
         r, esp_err_to_name(r), (int)esp_bt_controller_get_status());
  if (ESP_OK != r) { return false; }

  r = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 2 controller_enable-> 0x%x %s (status now %d)"),
         r, esp_err_to_name(r), (int)esp_bt_controller_get_status());
  if (ESP_OK != r) { return false; }

  r = esp_bluedroid_init();
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 3 bluedroid_init   -> 0x%x %s (status now %d)"),
         r, esp_err_to_name(r), (int)esp_bt_controller_get_status());
  if (ESP_OK != r) { return false; }

  r = esp_bluedroid_enable();
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 4 bluedroid_enable -> 0x%x %s (status now %d)"),
         r, esp_err_to_name(r), (int)esp_bt_controller_get_status());
  if (ESP_OK != r) { return false; }

  r = esp_bt_gap_register_callback(TcSppGapCb);
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 5 gap_register     -> 0x%x %s"), r, esp_err_to_name(r));
  if (ESP_OK != r) { return false; }

  r = esp_spp_register_callback(TcSppCb);
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 6 spp_register     -> 0x%x %s"), r, esp_err_to_name(r));
  if (ESP_OK != r) { return false; }

  esp_spp_cfg_t spp = { };
  spp.mode = ESP_SPP_MODE_CB;
  spp.enable_l2cap_ertm = false;
  spp.tx_buffer_size = 0;
  r = esp_spp_enhanced_init(&spp);
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: 7 spp_init         -> 0x%x %s"), r, esp_err_to_name(r));
  if (ESP_OK != r) { return false; }

  // We only ever connect out -- nobody should be able to connect to us.
  esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

  // The SPP callback flips the state to READY; give it up to a second.
  for (uint8_t i = 0; i < 50 && TC_SPP_AUS == TcSpp.state; i++) { delay(20); }
  if (TC_SPP_AUS == TcSpp.state) {
    AddLog(LOG_LEVEL_INFO, PSTR("SPP: all calls ok but no INIT event arrived"));
    return false;
  }
  AddLog(LOG_LEVEL_INFO, PSTR("SPP: ready"));
  return true;
}

static bool TcSppAdresse(const char *text, uint8_t *aus) {
  unsigned int v[6];
  if (6 != sscanf(text, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5])) {
    return false;
  }
  for (uint8_t i = 0; i < 6; i++) { aus[i] = (uint8_t)v[i]; }
  return true;
}

// channel 0 means "let SDP discovery find it". Returns IMMEDIATELY; the script polls
// sppState() afterwards to see whether the channel came up.
static int32_t TcSppConnect(const char *adresse, int32_t kanal) {
  if (!TcSppInit()) { return -1; }
  uint8_t bda[6];
  if (!TcSppAdresse(adresse, bda)) { return -2; }
  if (TC_SPP_OFFEN == TcSpp.state) { return 0; }   // already connected

  TcSpp.fuss = TcSpp.kopf = 0;                     // drop stale bytes
  TcSpp.state = TC_SPP_VERBINDET;
  esp_bd_addr_t ziel; memcpy(ziel, bda, 6);
  esp_err_t r = (kanal > 0)
      ? esp_spp_connect(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER, (uint8_t)kanal, ziel)
      : esp_spp_start_discovery(ziel);
  if (ESP_OK != r) { TcSpp.state = TC_SPP_BEREIT; return -3; }
  return 0;
}

static void TcSppClose(void) {
  if (TcSpp.handle) { esp_spp_disconnect(TcSpp.handle); }
  TcSpp.handle = 0;
  if (TcSpp.state == TC_SPP_OFFEN || TcSpp.state == TC_SPP_VERBINDET) {
    TcSpp.state = TC_SPP_BEREIT;
  }
}

static int32_t TcSppWrite(const uint8_t *d, uint16_t len) {
  if (TC_SPP_OFFEN != TcSpp.state || !TcSpp.handle) { return -1; }
  return (ESP_OK == esp_spp_write(TcSpp.handle, len, (uint8_t*)d)) ? (int32_t)len : -1;
}

// Inquire for `sekunden` seconds and write the results as text, one "AA:BB:.. Name\n"
// per line. WAITS for the inquiry to finish, so call it from a worker, not the main loop.
//
// KNOWN BUG (2026-08-02): observed hanging on the device -- "searching" is logged but no
// result ever appears, although the loop must give up after sekunden*1000+1500 ms.
// Prime suspect is a too-small worker stack killing the task silently. Fix before
// relying on this.
static int32_t TcSppScan(char *aus, uint16_t max, int32_t sekunden) {
  if (!TcSppInit()) { return -1; }
  if (sekunden < 1) { sekunden = 5; }
  if (sekunden > 30) { sekunden = 30; }

  TcSpp.fund_n = 0;
  TcSpp.sucht = true;
  // Duration is counted in units of 1.28 s by the API.
  uint8_t dauer = (uint8_t)((sekunden * 100) / 128);
  if (dauer < 1) { dauer = 1; }
  if (ESP_OK != esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, dauer, 0)) {
    return -1;
  }
  uint32_t ende = millis() + (uint32_t)sekunden * 1000 + 1500;
  while (TcSpp.sucht && !TimeReached(ende)) { delay(50); }
  esp_bt_gap_cancel_discovery();

  uint16_t o = 0;
  if (max) { aus[0] = '\0'; }
  for (uint8_t i = 0; i < TcSpp.fund_n && o + 32 < max; i++) {
    TcSppFund *f = &TcSpp.fund[i];
    o += snprintf(aus + o, max - o, "%02X:%02X:%02X:%02X:%02X:%02X %s\n",
                  f->addr[0], f->addr[1], f->addr[2], f->addr[3], f->addr[4], f->addr[5],
                  f->name);
  }
  return TcSpp.fund_n;
}

#endif  // USE_TINYC_SPP
#endif  // _XDRV_124_TINYC_SPP_H_
