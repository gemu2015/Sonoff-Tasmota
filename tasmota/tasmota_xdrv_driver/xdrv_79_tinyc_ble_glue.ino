/*
  xdrv_79_tinyc_ble_glue.ino - bridge TinyC's BLE syscalls to the xdrv_79 common-BLE driver

  Why this file exists, separate from the TinyC VM:
  Tasmota concatenates all xdrv_*.ino into one translation unit in LEXICOGRAPHIC filename
  order. "xdrv_124_tinyc..." sorts BEFORE "xdrv_79_esp32_ble..." ('1' < '7'), so the TinyC VM
  (xdrv_124) is compiled before the BLE_ESP32 namespace is declared and therefore cannot
  reference BLE_ESP32 types at all (and a forward declaration would clash with xdrv_79's later
  definition in the same TU). This glue file is named to sort AFTER xdrv_79_esp32_ble.ino
  ("esp32" < "tinyc"), so BLE_ESP32 IS visible here. It touches the BLE_ESP32 advert struct and
  GATT operation struct, and exposes only plain-C functions (tc_ble_*) to xdrv_124 (declared in
  xdrv_124_tinyc_vm.h, earlier in the same TU). No BLE_ESP32 type ever crosses into xdrv_124.

  Threading: the advert callback runs on the BLE/NimBLE task -> tc_ble_push() (ring, never the VM).
  The GATT op completion callback runs on the MAIN task -> fills a result buffer and clears the
  busy flag LAST (publish-after-fill). The VM polls bleDone()/bleResult() on its own task. So no
  BLE callback ever blocks on vm_mutex (honors the spawnTask/vm_mutex + httpGet concurrency lessons).
*/

#if defined(ESP32) && defined(USE_TINYC_BLE)

// ── Scan: advert sink (BLE task) ──────────────────────────────────────────────
static int tc_ble_adv_cb(BLE_ESP32::ble_advertisment_t *p) {
  if (!p) { return 0; }
  uint8_t mfg[TC_BLE_MFGLEN];
  int mfglen = 0;
  if (p->advertisedDevice && p->advertisedDevice->haveManufacturerData()) {
    std::string m = p->advertisedDevice->getManufacturerData();
    mfglen = (m.size() > (size_t)TC_BLE_MFGLEN) ? TC_BLE_MFGLEN : (int)m.size();
    memcpy(mfg, m.data(), mfglen);
  }
  tc_ble_push(p->addr, p->addrtype, p->RSSI, p->name, mfg, mfglen);
  return 0;  // let other consumers (MI32/iBeacon) see it too
}

void tc_ble_glue_register(void) {
  BLE_ESP32::BLEEnableUnsaved = 1;  // enable BLE at runtime so scripts work without SetOption115
  BLE_ESP32::registerForAdvertismentCallbacks("TinyC", tc_ble_adv_cb);
}

// ── GATT client: one in-flight transaction ───────────────────────────────────
static struct {
  volatile uint8_t busy;                  // 1 = op queued/running
  volatile int8_t  state;                 // 0 pending, 1 done, <0 = GEN_STATE_FAILED_*
  uint8_t          len;
  uint8_t          data[MAX_BLE_DATA_LEN_TC];
} tc_gatt_res = {};

// Runs on the MAIN task (BLE_ESP32 delivers completions there). Copy out, then clear busy LAST.
static int tc_ble_gatt_done_cb(BLE_ESP32::generic_sensor_t *op) {
  int n = 0;
  if (op->notifylen) {
    n = (op->notifylen > MAX_BLE_DATA_LEN_TC) ? MAX_BLE_DATA_LEN_TC : op->notifylen;
    memcpy(tc_gatt_res.data, op->dataNotify, n);
  } else if (op->readlen) {
    n = (op->readlen > MAX_BLE_DATA_LEN_TC) ? MAX_BLE_DATA_LEN_TC : op->readlen;
    memcpy(tc_gatt_res.data, op->dataRead, n);
  }
  tc_gatt_res.len   = (uint8_t)n;
  tc_gatt_res.state = (op->state < 0) ? (int8_t)op->state : 1;  // <0 fail, else success
  tc_gatt_res.busy  = 0;                                        // publish-last
  return 1;  // consume — don't auto-post to MQTT
}

int tc_ble_gatt_start(const uint8_t *mac, int addrtype, int svc, int chr, int notify,
                      const uint8_t *wbuf, int wlen) {
  if (tc_gatt_res.busy) { return -1; }                          // one transaction at a time
  BLE_ESP32::generic_sensor_t *op = nullptr;
  if (!BLE_ESP32::newOperation(&op)) { return -2; }
  op->addr = NimBLEAddress((uint8_t *)mac, (uint8_t)addrtype);
  if (svc)    { op->serviceUUID = NimBLEUUID((uint16_t)svc); }
  if (chr)    { op->characteristicUUID = NimBLEUUID((uint16_t)chr); }
  if (notify) { op->notificationCharacteristicUUID = NimBLEUUID((uint16_t)notify); }
  if (wbuf && wlen > 0) {
    if (wlen > MAX_BLE_DATA_LEN) { wlen = MAX_BLE_DATA_LEN; }
    op->writelen = (uint8_t)wlen;
    memcpy(op->dataToWrite, wbuf, wlen);
  }
  op->completecallback = (void *)tc_ble_gatt_done_cb;
  op->context = (void *)0;
  tc_gatt_res.len = 0; tc_gatt_res.state = 0; tc_gatt_res.busy = 1;  // prime before queueing
  if (!BLE_ESP32::extQueueOperation(&op)) {
    BLE_ESP32::freeOperation(&op);
    tc_gatt_res.busy = 0;
    return -3;
  }
  return 1;
}

int tc_ble_gatt_poll(void) {
  if (tc_gatt_res.busy)        { return 0; }                    // still running
  if (tc_gatt_res.state < 0)   { return tc_gatt_res.state; }    // failed (GEN_STATE_*)
  if (tc_gatt_res.state == 1)  { return tc_gatt_res.len ? tc_gatt_res.len : 1; }  // done
  return 0;
}

int tc_ble_gatt_copy(uint8_t *out, int max) {
  int n = tc_gatt_res.len;
  if (n > max) { n = max; }
  if (n > 0) { memcpy(out, tc_gatt_res.data, n); }
  return n;
}

// ── GATT server (peripheral) — device-as-peripheral so a phone (central) connects ──
// NimBLE server build + setValue/notify run on the MAIN task via tc_ble_srv_loop()
// (hooked into xdrv_79 FUNC_EVERY_50_MSECOND); the VM bridge fns only set request
// flags + copy buffers. Incoming writes land in onWrite (NimBLE task) -> per-char
// buffer under a critical section; the VM drains them by polling (same idiom as scan).
#define TC_SRV_MAX_CHR   8
#define TC_SRV_VALLEN    MAX_BLE_DATA_LEN_TC

// NimBLE host C APIs used to quiesce GAP + reset the GATT into the configuring state
// before registering our service (scanner driver leaves the GATT in the started state).
extern "C" {
  int ble_gatts_reset(void);
  int ble_gap_adv_stop(void);
  int ble_gap_disc_cancel(void);
}

static portMUX_TYPE tc_srv_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t      s_srv_built_once = 0;   // GATT built once per boot; re-runs re-attach (reboot to reconfigure)

struct tc_srv_chr_t {
  char     uuid[40];
  uint16_t props;                  // NIMBLE_PROPERTY bits (mapped from BLE_READ/WRITE/NOTIFY)
  NimBLECharacteristic *chr;
  volatile uint8_t wnew;           // 1 = central wrote since the VM last read
  uint8_t  wlen;
  uint8_t  wdata[TC_SRV_VALLEN];   // incoming (central -> device)
  volatile uint8_t set_req;        // 1 = setValue pending, 2 = setValue + notify
  uint8_t  slen;
  uint8_t  sdata[TC_SRV_VALLEN];   // outgoing (device -> central)
};

static struct {
  volatile uint8_t start_req;
  volatile uint8_t stop_req;
  volatile uint8_t built;
  volatile uint8_t connected;
  char     name[32];
  char     svc[40];
  int      nchr;
  tc_srv_chr_t chr[TC_SRV_MAX_CHR];
  NimBLEServer  *server;
  NimBLEService *service;
} tc_srv = {};

// onConnect / onWrite run on the NimBLE host task.
class TcSrvServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { tc_srv.connected = 1; }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    tc_srv.connected = 0;
    if (tc_srv.built) { NimBLEDevice::getAdvertising()->start(); }   // re-advertise for reconnect
  }
};
class TcSrvCharCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    for (int i = 0; i < tc_srv.nchr; i++) {
      if (tc_srv.chr[i].chr != c) { continue; }
      NimBLEAttValue v = c->getValue();
      int n = v.length(); if (n > TC_SRV_VALLEN) { n = TC_SRV_VALLEN; }
      portENTER_CRITICAL(&tc_srv_mux);
      if (n > 0) { memcpy(tc_srv.chr[i].wdata, v.data(), n); }
      tc_srv.chr[i].wlen = (uint8_t)n;
      tc_srv.chr[i].wnew = 1;
      portEXIT_CRITICAL(&tc_srv_mux);
      break;
    }
  }
};
static TcSrvServerCB tc_srv_server_cb;
static TcSrvCharCB   tc_srv_char_cb;

void tc_ble_srv_init(const char *name) {
  BLE_ESP32::BLEEnableUnsaved = 1;                       // request NimBLE up (no scan needed)
  strlcpy(tc_srv.name, name ? name : "TinyC", sizeof(tc_srv.name));
  tc_srv.svc[0] = 0;
  tc_srv.nchr = 0;
  tc_srv.built = 0;
  tc_srv.start_req = 0;
}
void tc_ble_srv_service(const char *uuid) {
  strlcpy(tc_srv.svc, uuid ? uuid : "", sizeof(tc_srv.svc));
}
int tc_ble_srv_char(const char *uuid, int props) {
  if (tc_srv.built || tc_srv.nchr >= TC_SRV_MAX_CHR || !uuid) { return -1; }
  uint16_t np = 0;                                        // BLE_READ=1 | BLE_WRITE=2 | BLE_NOTIFY=4 -> NimBLE flags
  if (props & 1) { np |= NIMBLE_PROPERTY::READ; }
  if (props & 2) { np |= NIMBLE_PROPERTY::WRITE; }
  if (props & 4) { np |= NIMBLE_PROPERTY::NOTIFY; }
  int i = tc_srv.nchr++;
  strlcpy(tc_srv.chr[i].uuid, uuid, sizeof(tc_srv.chr[i].uuid));
  tc_srv.chr[i].props = np;
  tc_srv.chr[i].chr = nullptr;
  tc_srv.chr[i].wnew = 0; tc_srv.chr[i].wlen = 0; tc_srv.chr[i].set_req = 0;
  return i;
}
void tc_ble_srv_go(void)        { tc_srv.start_req = 1; }
int  tc_ble_srv_connected(void) { return tc_srv.connected ? 1 : 0; }
int  tc_ble_srv_written(int h)  {
  if (h < 0 || h >= tc_srv.nchr) { return 0; }
  return tc_srv.chr[h].wnew ? tc_srv.chr[h].wlen : 0;
}
int tc_ble_srv_read(int h, uint8_t *out, int max) {
  if (h < 0 || h >= tc_srv.nchr) { return 0; }
  int n;
  portENTER_CRITICAL(&tc_srv_mux);
  n = tc_srv.chr[h].wlen; if (n > max) { n = max; }
  if (n > 0) { memcpy(out, tc_srv.chr[h].wdata, n); }
  tc_srv.chr[h].wnew = 0;
  portEXIT_CRITICAL(&tc_srv_mux);
  return n;
}
void tc_ble_srv_set(int h, const uint8_t *buf, int len, int notify) {
  if (h < 0 || h >= tc_srv.nchr) { return; }
  if (len < 0) { len = 0; }
  if (len > TC_SRV_VALLEN) { len = TC_SRV_VALLEN; }
  if (buf && len > 0) { memcpy(tc_srv.chr[h].sdata, buf, len); }
  tc_srv.chr[h].slen = (uint8_t)len;
  tc_srv.chr[h].set_req = notify ? 2 : 1;                // applied on the main task (publish-last)
}
void tc_ble_srv_stop(void) { tc_srv.stop_req = 1; }

// MAIN task: build the server once NimBLE is up; apply pending setValue/notify.
void tc_ble_srv_loop(void) {
  if (tc_srv.stop_req) {
    tc_srv.stop_req = 0;
    if (tc_srv.built) {
      NimBLEDevice::getAdvertising()->stop();
      tc_srv.built = 0; tc_srv.connected = 0;
    }
    return;
  }
  if (tc_srv.start_req && !tc_srv.built) {
    if (!NimBLEDevice::isInitialized()) { return; }      // wait for BLE_ESP32 to bring NimBLE up
    if (!tc_srv.svc[0] || tc_srv.nchr == 0) { tc_srv.start_req = 0; return; }
    if (s_srv_built_once && tc_srv.service) {
      // Re-run without a reboot: rebuilding would create a DUPLICATE GATT service (NimBLE never
      // dedups). Re-attach the characteristic handles to the existing service so notify/set keep
      // working, and resume advertising (e.g. after bleServerStop). Reboot to change the layout.
      for (int i = 0; i < tc_srv.nchr; i++) {
        tc_srv.chr[i].chr = tc_srv.service->getCharacteristic(NimBLEUUID(tc_srv.chr[i].uuid));
      }
      NimBLEDevice::getAdvertising()->start();
      tc_srv.built = 1; tc_srv.start_req = 0;
      AddLog(LOG_LEVEL_INFO, PSTR("BLE: TinyC GATT server re-attached (reboot to change services)"));
      return;
    }
    // BLE_ESP32 already brought NimBLE up AND is running its background scan, so the GATT is not
    // in the "configuring" state -> ble_gatts_add_svcs() (in service->start) is rejected with
    // BLE_HS_EBUSY and the characteristics never register. Quiesce GAP (esp. the scan) so the
    // reset in createServer() succeeds, then build + register the service.
    ble_gap_adv_stop();
    ble_gap_disc_cancel();
    ble_gatts_reset();
    tc_srv.server = NimBLEDevice::createServer();   // re-inits the default GAP/GATT services
    tc_srv.server->setCallbacks(&tc_srv_server_cb);
    tc_srv.service = tc_srv.server->createService(NimBLEUUID(tc_srv.svc));
    for (int i = 0; i < tc_srv.nchr; i++) {
      NimBLECharacteristic *c = tc_srv.service->createCharacteristic(NimBLEUUID(tc_srv.chr[i].uuid), tc_srv.chr[i].props);
      c->setCallbacks(&tc_srv_char_cb);
      tc_srv.chr[i].chr = c;
    }
    bool svc_ok = tc_srv.service->start();
    NimBLEDevice::getServer()->start();             // ble_gatts_start before advertising
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->setName(tc_srv.name);
    adv->addServiceUUID(tc_srv.service->getUUID());
    adv->enableScanResponse(true);
    adv->start();
    tc_srv.built = 1; tc_srv.start_req = 0;
    s_srv_built_once = 1;
    AddLog(svc_ok ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR,
           PSTR("BLE: TinyC GATT server '%s' %s (%d chars)"),
           tc_srv.name, svc_ok ? "advertising" : "registration FAILED", tc_srv.nchr);
    return;
  }
  if (!tc_srv.built) { return; }
  for (int i = 0; i < tc_srv.nchr; i++) {
    uint8_t req = tc_srv.chr[i].set_req;
    if (!req || !tc_srv.chr[i].chr) { continue; }
    tc_srv.chr[i].chr->setValue(tc_srv.chr[i].sdata, tc_srv.chr[i].slen);
    if (req == 2 && tc_srv.connected) { tc_srv.chr[i].chr->notify(); }
    tc_srv.chr[i].set_req = 0;
  }
}

#endif  // USE_TINYC_BLE
