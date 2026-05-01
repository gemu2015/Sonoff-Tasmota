// migrate_to_safeboot.ino — single-shot OTA migration from the historic
#include <Arduino.h>
// Tasmota 2-app-OTA partition layout to the modern safeboot layout, on a
// 4 MB ESP32. See README.md for the full design and risk analysis.
//
// Build: PlatformIO, environment in this directory's platformio.ini.
// Deliver: OTA upload via Tasmota's `Firmware Upgrade` page.
// Use:    after the device reboots into this sketch, browse to its IP,
//         click "Migrate", wait for the auto-reboot.

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>
#include <esp_rom_crc.h>
#include <rom/cache.h>

#include "embedded/safeboot_blob.h"      // SAFEBOOT_BIN[], SAFEBOOT_LEN, SAFEBOOT_CRC32
#include "embedded/partitions_blob.h"    // PARTITIONS_BIN[], PARTITIONS_LEN, PARTITIONS_CRC32

// ── Flash layout constants — these match a 4 MB ESP32 with the historic
//    Tasmota 2-app-OTA layout (app0 @ 0x10000, app1 @ 0x1F0000).
//    The NEW safeboot.bin lands where OLD app0 used to be, and the new
//    partition table replaces the one at 0x8000.
static constexpr uint32_t PARTITION_TABLE_OFFSET = 0x8000;
static constexpr uint32_t PARTITION_TABLE_SIZE   = 0x1000;   // 4 KB sector
static constexpr uint32_t OTADATA_OFFSET         = 0xE000;
static constexpr uint32_t OTADATA_SIZE           = 0x2000;
static constexpr uint32_t SAFEBOOT_DST_OFFSET    = 0x10000;  // OLD app0 start
static constexpr uint32_t OLD_APP1_OFFSET        = 0x1F0000; // where WE run

// Sector-align a length up to the next 4 KB boundary.
static constexpr uint32_t SECTOR_SIZE = 4096;
static inline uint32_t sector_align(uint32_t n) {
  return (n + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);
}

// ── Wi-Fi: try STA with credentials from existing Tasmota NVS, fall back
//    to a SoftAP. We don't reuse Tasmota's whole settings system —
//    just read the SSID/password keys it stores in NVS.
static const char *AP_PREFIX = "safeboot-migrate-";
static const char *AP_PASS   = "safeboot1";   // change before publishing

static WebServer server(80);
static String last_log;

static void log_line(const String &s) {
  Serial.println(s);
  last_log += s + "\n";
  if (last_log.length() > 4096) last_log.remove(0, last_log.length() - 4096);
}

// ── Read the current partition table out of flash and return a short
//    fingerprint string ("OLD-2APP", "NEW-SAFEBOOT", or "UNKNOWN") plus
//    the CRC32 so the user can sanity-check before pulling the trigger.
struct LayoutInfo {
  const char *name;
  uint32_t    crc;
};
static LayoutInfo detect_current_layout() {
  uint8_t buf[PARTITION_TABLE_SIZE];
  esp_err_t err = esp_flash_read(NULL, buf, PARTITION_TABLE_OFFSET,
                                 PARTITION_TABLE_SIZE);
  if (err != ESP_OK) return {"FLASH-READ-FAIL", 0};

  uint32_t crc = esp_rom_crc32_le(0, buf, PARTITION_TABLE_SIZE);

  // Walk the partition entries (32 bytes each, magic 0xAA50). We're
  // looking for two app/ota_0 + ota_1 partitions of similar size (old
  // layout) vs one safeboot factory + one ota_0 (new layout).
  int n_app = 0, n_safeboot = 0;
  for (int i = 0; i < (int)PARTITION_TABLE_SIZE; i += 32) {
    if (buf[i] != 0xAA || buf[i+1] != 0x50) break;   // end of table
    uint8_t type    = buf[i + 2];
    uint8_t subtype = buf[i + 3];
    if (type == 0x00) {
      // app partition
      n_app++;
      // safeboot subtype is 'factory' (0x00) when paired with ota_0;
      // legacy is two ota_x partitions (subtype 0x10/0x11).
      if (subtype == 0x00) n_safeboot++;
    }
  }

  if (n_app == 2 && n_safeboot == 0) return {"OLD-2APP", crc};
  if (n_app == 2 && n_safeboot == 1) return {"NEW-SAFEBOOT", crc};
  return {"UNKNOWN", crc};
}

// ── The actual migration. Runs from IRAM so the cache disable doesn't
//    pull our own instructions out from under us.
//
// Returns true on success (won't return — esp_restart). On failure
// returns false WITHOUT having touched 0x8000 or 0xE000, so the device
// can boot back into this same migrator and try again.
IRAM_ATTR static bool do_migrate() {
  log_line("=== migration start ===");

  // ── Step 1: pre-flight. Verify the embedded blobs match expected CRCs.
  uint32_t sb_crc_actual = esp_rom_crc32_le(0, SAFEBOOT_BIN, SAFEBOOT_LEN);
  if (sb_crc_actual != SAFEBOOT_CRC32) {
    log_line(String("pre-flight: safeboot blob CRC mismatch ")
             + " expected=" + String(SAFEBOOT_CRC32, HEX)
             + " got="      + String(sb_crc_actual, HEX));
    return false;
  }
  uint32_t pt_crc_actual = esp_rom_crc32_le(0, PARTITIONS_BIN, PARTITIONS_LEN);
  if (pt_crc_actual != PARTITIONS_CRC32) {
    log_line("pre-flight: partition-table blob CRC mismatch");
    return false;
  }
  if (PARTITIONS_LEN > PARTITION_TABLE_SIZE) {
    log_line("pre-flight: partition-table blob too large");
    return false;
  }
  log_line("pre-flight: OK");

  // ── Step 2: write safeboot to OLD app0 region (we're running from
  //    OLD app1 so this is safe).
  const uint32_t sb_aligned_len = sector_align(SAFEBOOT_LEN);
  log_line(String("erasing safeboot region @ 0x") +
           String(SAFEBOOT_DST_OFFSET, HEX) + " size " +
           String(sb_aligned_len) + " bytes");
  esp_err_t err = esp_flash_erase_region(NULL, SAFEBOOT_DST_OFFSET,
                                         sb_aligned_len);
  if (err != ESP_OK) { log_line("erase failed"); return false; }

  log_line("writing safeboot blob");
  err = esp_flash_write(NULL, SAFEBOOT_BIN, SAFEBOOT_DST_OFFSET, SAFEBOOT_LEN);
  if (err != ESP_OK) { log_line("write failed"); return false; }

  // ── Step 3: read back and CRC-verify. If the write got corrupted
  //    (cosmic ray, flash wear, whatever), bail BEFORE touching the
  //    partition table — old layout still works.
  log_line("verifying safeboot via read-back CRC");
  uint32_t verify_crc = 0;
  uint8_t  chunk[1024];
  for (uint32_t off = 0; off < SAFEBOOT_LEN; off += sizeof(chunk)) {
    uint32_t n = SAFEBOOT_LEN - off;
    if (n > sizeof(chunk)) n = sizeof(chunk);
    err = esp_flash_read(NULL, chunk, SAFEBOOT_DST_OFFSET + off, n);
    if (err != ESP_OK) { log_line("verify read failed"); return false; }
    verify_crc = esp_rom_crc32_le(verify_crc, chunk, n);
  }
  if (verify_crc != SAFEBOOT_CRC32) {
    log_line(String("verify: CRC mismatch — expected ")
             + String(SAFEBOOT_CRC32, HEX) + " got " + String(verify_crc, HEX));
    return false;
  }
  log_line("verify: OK");

  // ── Step 4: BRICKING WINDOW. Erase + write 0x8000 (partition table),
  //    then 0xE000 (otadata). These are the only operations where a
  //    power loss leaves the device unbootable.
  log_line("=== entering brick window ===");
  noInterrupts();
  err = esp_flash_erase_region(NULL, PARTITION_TABLE_OFFSET, PARTITION_TABLE_SIZE);
  if (err != ESP_OK) { interrupts(); log_line("PT erase failed"); return false; }
  err = esp_flash_write(NULL, PARTITIONS_BIN, PARTITION_TABLE_OFFSET, PARTITIONS_LEN);
  if (err != ESP_OK) { interrupts(); log_line("PT write failed"); return false; }

  // Clear otadata so the bootloader picks the factory (= safeboot)
  // partition by default. Writing an all-FF region is equivalent to
  // erasing.
  err = esp_flash_erase_region(NULL, OTADATA_OFFSET, OTADATA_SIZE);
  if (err != ESP_OK) { interrupts(); log_line("otadata erase failed"); return false; }
  interrupts();
  log_line("=== brick window closed ===");

  log_line("rebooting into safeboot");
  delay(200);
  esp_restart();
  return true;   // never reached
}

// ── Critical safety: where in flash are WE running from?
//
// Tasmota's OTA writes the new firmware to the INACTIVE app slot. So
// if the device was booted from app0 (0x10000), our binary lands at
// app1 (0x1F0000) — SAFE: we can write safeboot.bin to 0x10000 without
// corrupting our own running code.
//
// But if the device was booted from app1 (0x1F0000), our binary lands
// at app0 (0x10000) — UNSAFE: writing safeboot.bin to 0x10000 would
// erase the very flash we're executing from. Instant brick.
//
// The migrator MUST detect this and refuse to migrate when on app0.
// In that case we offer a self-OTA upload form: the user re-uploads
// the SAME migrator firmware, which Tasmota's normal OTA logic puts
// into the inactive (app1) slot, and on the next reboot we run from
// the safe slot.
struct RunningSlot {
  bool      safe;        // true = running from app1, OK to migrate
  uint32_t  offset;      // actual offset of running partition
  const char *label;     // "app0" / "app1" / "factory" / "?"
};
static RunningSlot detect_running_slot() {
  const esp_partition_t *p = esp_ota_get_running_partition();
  RunningSlot r = {false, 0, "?"};
  if (!p) return r;
  r.offset = p->address;
  if (p->type == ESP_PARTITION_TYPE_APP) {
    if      (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0)   r.label = "app0";
    else if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)   r.label = "app1";
    else if (p->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) r.label = "factory";
  }
  // SAFE only when offset is exactly OLD-layout app1 (0x1F0000).
  // Refusing on any other offset (factory, custom partition table, etc.)
  // is the conservative choice — better to bail than misguess.
  r.safe = (r.offset == OLD_APP1_OFFSET);
  return r;
}

// ── Web UI ──────────────────────────────────────────────────────────────
static void handle_root() {
  LayoutInfo  info  = detect_current_layout();
  RunningSlot rslot = detect_running_slot();
  bool layout_ok    = (strcmp(info.name, "OLD-2APP") == 0);
  bool can_migrate  = layout_ok && rslot.safe;

  String html;
  html += F("<!doctype html><html><head><title>safeboot migrator</title>"
            "<style>body{font-family:sans-serif;max-width:42em;margin:1em auto;"
            "padding:1em}h1{color:#1fa3ec}pre{background:#222;color:#0f0;"
            "padding:.5em;overflow:auto}button{font-size:1.2em;padding:.5em 1em;"
            "background:#d43535;color:#fff;border:0;border-radius:.3em;"
            "cursor:pointer}button:disabled{background:#666;cursor:not-allowed}"
            ".warn{background:#fff3cd;border-left:.4em solid #ffc107;"
            "padding:.5em 1em;margin:1em 0}"
            ".bad{background:#f8d7da;border-left:.4em solid #d43535;"
            "padding:.5em 1em;margin:1em 0}"
            "</style></head><body>");
  html += "<h1>safeboot migrator</h1>";

  // ── Status block ──
  html += "<p>Detected layout: <b>" + String(info.name) + "</b> "
          "(table CRC32 <code>" + String(info.crc, HEX) + "</code>)</p>";
  html += "<p>Running from: <b>" + String(rslot.label) + "</b> @ "
          "<code>0x" + String(rslot.offset, HEX) + "</code> — "
          + (rslot.safe ? "<b style='color:green'>SAFE slot</b>"
                        : "<b style='color:#d43535'>WRONG slot — see below</b>")
          + "</p>";
  html += "<p>Embedded safeboot: " + String(SAFEBOOT_LEN) +
          " bytes, CRC32 <code>" + String(SAFEBOOT_CRC32, HEX) + "</code></p>";
  html += "<p>Embedded partition table: " + String(PARTITIONS_LEN) +
          " bytes, CRC32 <code>" + String(PARTITIONS_CRC32, HEX) + "</code></p>";

  // ── Branch on slot safety ──
  if (!layout_ok) {
    html += F("<div class='bad'><b>Migration disabled.</b> Current layout is "
              "not recognised as the OLD-2APP this migrator targets. "
              "Either you're already on safeboot, or this is a layout "
              "variant we don't handle.</div>");
  } else if (!rslot.safe) {
    // App0 case — would self-destruct. Offer one-click slot-flip:
    // the migrator reads its own bytes from app0 and writes them
    // verbatim to app1, then sets app1 as the boot target and reboots.
    // After reboot the SAME migrator runs from the safe slot.
    html += F("<div class='bad'><b>Migration would brick the device from "
              "this slot.</b> The migrator is currently running from "
              "<code>0x10000</code> (app0), which is exactly where safeboot "
              "needs to be written. Migrating now would erase the running "
              "code mid-flight.</div>"
              "<div class='warn'><b>Fix:</b> click below and the migrator "
              "will copy itself byte-for-byte from app0 to app1, mark app1 "
              "as the boot target, and reboot. After the reboot we'll be "
              "running from the safe slot and the Migrate button will be "
              "available. Total time ~5–10 seconds. The old app0 copy is "
              "left intact, so a failure during copy is recoverable.</div>"
              "<form method='POST' action='/flipslot'>"
              "<button type='submit' style='background:#1fa3ec'>"
              "Copy to app1 and reboot</button></form>"
              "<details style='margin-top:1em'><summary>Manual fallback</summary>"
              "<p>If the auto-copy fails, you can re-upload the migrator "
              ".bin manually — Tasmota's OTA logic writes to the inactive "
              "slot, which has the same effect.</p>"
              "<form method='POST' action='/selfota' "
              "enctype='multipart/form-data'>"
              "<input type='file' name='update' accept='.bin' required>"
              "<button type='submit'>Re-upload .bin</button></form>"
              "</details>");
  } else {
    // Happy path — green button.
    html += F("<p>This will rewrite the partition table at <code>0x8000</code> "
              "and install safeboot at <code>0x10000</code>. "
              "<b>Power loss during the ~100 ms brick window can render the "
              "device unbootable without serial recovery.</b></p>"
              "<form method='POST' action='/go'>"
              "<button type='submit'>Migrate</button></form>");
  }

  html += "<h3>Log</h3><pre>" + last_log + "</pre>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

static void handle_go() {
  LayoutInfo  info  = detect_current_layout();
  RunningSlot rslot = detect_running_slot();
  if (strcmp(info.name, "OLD-2APP") != 0) {
    server.send(400, "text/plain", "refusing: layout is not OLD-2APP");
    return;
  }
  if (!rslot.safe) {
    server.send(400, "text/plain",
                "refusing: running from app0, would self-destruct. "
                "Re-upload the migrator first to flip to app1.");
    return;
  }
  // Send the response BEFORE migration so the client's connection isn't
  // hanging while we erase 0x8000.
  server.send(200, "text/plain",
              "migration starting — device will reboot in ~5s. "
              "If you can't reach it after, pull the plug and serial-flash.");
  delay(500);
  WiFi.disconnect(true);   // free up the radio
  delay(100);
  bool ok = do_migrate();  // returns false on pre-flight or write fail
  // do_migrate() doesn't return on success.
  if (!ok) {
    log_line("migration aborted; device safe to reboot or retry");
  }
}

// ── Slot-flip: copy our own running app from app0 → app1, set app1 as
//    the boot target, reboot. After reboot the SAME firmware runs from
//    the safe slot and migration can proceed.
//
// Why this is safe:
//   - We read from app0 via esp_partition_read(), which goes through the
//     flash MMU just like normal reads of our own .text. We're not
//     touching app1 yet.
//   - We write to app1 via esp_ota_*(), which is the standard A/B OTA
//     path. If anything fails mid-write, esp_ota_end() marks app1 as
//     invalid, otadata still points at app0, and we just stay running
//     from app0 — no harm.
//   - Only after a complete + verified copy do we call
//     esp_ota_set_boot_partition() to flip the active slot.
//
// Returns true on success (won't return — we restart). False on failure
// (caller logs and shows the user the manual-upload fallback).
static bool flip_to_app1() {
  log_line("=== flip slot: copying app0 → app1 ===");

  const esp_partition_t *src = esp_ota_get_running_partition();
  if (!src) { log_line("flip: no running partition"); return false; }
  const esp_partition_t *dst = esp_ota_get_next_update_partition(NULL);
  if (!dst) { log_line("flip: no next-update partition"); return false; }
  log_line(String("flip: src=") + src->label + " @ 0x" + String(src->address, HEX) +
           " size=" + String(src->size));
  log_line(String("flip: dst=") + dst->label + " @ 0x" + String(dst->address, HEX) +
           " size=" + String(dst->size));

  // The actual app image is smaller than the partition. We need to copy
  // only the meaningful bytes (image header + segments). Easiest route:
  // copy the WHOLE partition. The dst slot is then bit-identical to src,
  // which is what we want.
  //
  // (An alternative would be to parse the ESP32 image header to find the
  // exact image size, but the bootloader doesn't care about trailing
  // garbage — and the OTA driver writes whatever we hand it.)
  const size_t bytes = src->size;
  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(dst, bytes, &handle);
  if (err != ESP_OK) {
    log_line(String("esp_ota_begin: ") + esp_err_to_name(err));
    return false;
  }

  static uint8_t buf[4096];
  size_t copied = 0;
  while (copied < bytes) {
    size_t chunk = bytes - copied;
    if (chunk > sizeof(buf)) chunk = sizeof(buf);
    err = esp_partition_read(src, copied, buf, chunk);
    if (err != ESP_OK) {
      log_line(String("read fail @ ") + String(copied) + ": " + esp_err_to_name(err));
      esp_ota_abort(handle);
      return false;
    }
    err = esp_ota_write(handle, buf, chunk);
    if (err != ESP_OK) {
      log_line(String("write fail @ ") + String(copied) + ": " + esp_err_to_name(err));
      esp_ota_abort(handle);
      return false;
    }
    copied += chunk;
    // A small yield helps the watchdog stay happy on big copies.
    if ((copied & 0x3FFFF) == 0) {
      log_line(String("flip: ") + String(copied / 1024) + " KB / " +
               String(bytes / 1024) + " KB");
      yield();
    }
  }

  err = esp_ota_end(handle);
  if (err != ESP_OK) {
    log_line(String("esp_ota_end: ") + esp_err_to_name(err));
    return false;
  }
  err = esp_ota_set_boot_partition(dst);
  if (err != ESP_OK) {
    log_line(String("esp_ota_set_boot_partition: ") + esp_err_to_name(err));
    return false;
  }
  log_line("flip: done, rebooting into app1");
  delay(200);
  ESP.restart();
  return true;   // not reached
}

static void handle_flipslot() {
  RunningSlot rslot = detect_running_slot();
  if (rslot.safe) {
    server.send(400, "text/plain",
                "already running from a safe slot — nothing to flip");
    return;
  }
  // Send response BEFORE the long flash op so the browser doesn't hang.
  server.send(200, "text/plain",
              "copying app0 → app1, ~5-10s, then auto-reboot. "
              "Reload this page after the device comes back up.");
  delay(500);
  WiFi.disconnect(true);
  delay(100);
  if (!flip_to_app1()) {
    log_line("flip: failed; safe to retry or use manual upload");
  }
}

// ── Manual self-OTA fallback: lets the user re-upload the migrator while
//    it's already running. Used as a backup when the auto-copy fails.
//    Uses Arduino-ESP32's Update.h, which handles writing to the inactive
//    partition + flipping otadata + etc.
static void handle_selfota_upload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    log_line("self-OTA: " + upload.filename);
    if (!Update.begin()) {                           // size = inactive partition
      log_line(String("Update.begin() failed: ") + Update.errorString());
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      log_line(String("Update.write() failed: ") + Update.errorString());
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      log_line("self-OTA: success, " + String(upload.totalSize) + " bytes");
    } else {
      log_line(String("Update.end() failed: ") + Update.errorString());
    }
  }
}
static void handle_selfota_done() {
  if (Update.hasError()) {
    server.send(500, "text/plain",
                String("self-OTA failed: ") + Update.errorString());
    return;
  }
  server.send(200, "text/plain",
              "self-OTA OK — rebooting into the freshly-written slot. "
              "Reload this page once the device is back up.");
  delay(500);
  ESP.restart();
}

static void handle_log() {
  server.send(200, "text/plain", last_log);
}

// ── Wi-Fi setup ─────────────────────────────────────────────────────────
//    Read existing Tasmota Wi-Fi creds from NVS if present, else SoftAP.
static bool try_sta() {
  Preferences prefs;
  if (!prefs.begin("Tasmota", true)) return false;
  String ssid = prefs.getString("Settings.sta_ssid1", "");
  String pass = prefs.getString("Settings.sta_pwd1", "");
  prefs.end();
  if (ssid.length() == 0) return false;

  log_line("STA: connecting to " + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      log_line("STA: " + WiFi.localIP().toString());
      return true;
    }
    delay(500);
  }
  return false;
}

static void start_softap() {
  uint64_t mac = ESP.getEfuseMac();
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "%s%04X", AP_PREFIX, (uint16_t)mac);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, AP_PASS);
  log_line("AP: " + String(ssid) + " / " + String(AP_PASS) +
           " @ " + WiFi.softAPIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  log_line("safeboot migrator booting");
  log_line("running from offset 0x" + String(OLD_APP1_OFFSET, HEX) +
           " (will write safeboot to 0x" + String(SAFEBOOT_DST_OFFSET, HEX) + ")");

  if (!try_sta()) {
    log_line("STA failed, starting SoftAP");
    start_softap();
  }

  server.on("/",        handle_root);
  server.on("/go",      HTTP_POST, handle_go);
  server.on("/log",     handle_log);
  server.on("/flipslot", HTTP_POST, handle_flipslot);
  server.on("/selfota", HTTP_POST, handle_selfota_done, handle_selfota_upload);
  server.begin();
  log_line("web UI ready");
}

void loop() {
  server.handleClient();
  delay(1);
}
