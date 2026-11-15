// espnow_button_esp8266.ino - battery wall button on a bare ESP8266 (ESP-01 / ESP-12), 11.11.2026
//
// The button sits between RST and GND. Pressing it resets the chip out of deep sleep, setup() sends ONE ESP-NOW packet to the receiver and
// the chip goes back to deep sleep (ESP.deepSleep(0): wakes only by RST, nothing else is running). About 100-250 ms awake per press.
//
// Packet (4 bytes, see tasmota/tinyc/examples/gm24g_lamp.tc):   'B', BUTTON_ID, ACTION, counter
//   BUTTON_ID 1 = all lamps, 2..5 = zone 1..4;  ACTION 0 = off, 1 = on, 2 = toggle (the receiver keeps the state).
//
// The packet goes to the receiver's MAC on CHANNEL. The receiver is on the router's channel (fix it in the router!), so CHANNEL has to be it.
// Compatible with the ESP-NOW of the ESP32 (TinyC: espnowBegin / espnowRecv).
//
// Hardware: 3.0 .. 3.6 V (see README: no 3.3 V regulator with a big quiescent current, no USB-serial chip, no LED on the module).
//
// CONFIG MODE (14.11.2026, tried on an ESP-12 on 15.11.2026: form, save, test burst, captive portal): pull CFG_PIN to GND (a jumper or a second button) while the chip starts - by pressing
// RST, or by inserting the battery - and the button opens an access point "Taster-XXXXXX" (open, or with AP_PASS) with a web form at
// http://192.168.4.1/ (a captive portal: a phone opens the form by itself, a DNS server answers every name with the button's address): receiver MAC, channel, button id, action, broadcast. "Speichern" stores them in the flash (EEPROM emulation), "Test senden"
// sends one burst with them. After CFG_TIMEOUT_S without a request the chip goes back to deep sleep. A button that was never configured
// uses the values of the #defines below, so one binary works for every button.
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
extern "C" {
#include <espnow.h>
#include "user_interface.h"
}

// ---- adapt these ------------------------------------------------------------------------------------------------
static uint8_t RECEIVER[6] = { 0xE0, 0x72, 0xA1, 0xE7, 0xE6, 0xF4 };   // MAC of the lamp controller
#define CHANNEL    11            // channel of the router / receiver
#define BUTTON_ID  1             // 1 = all, 2..5 = zone 1..4
#define ACTION     2             // 0 off, 1 on, 2 toggle
#define COPY_GAP_MS 25           // pause between the copies: spreads them over ~125 ms so one fading dip does not swallow all
#define TX_POWER_DBM 14.0        // lower peak current (~200 mA instead of ~350 mA); a weak 3.3 V supply sags on the full 20.5 dBm burst. Range at 1 m is no problem
#define USE_BROADCAST 1          // 1 = send to FF:FF:FF:FF:FF:FF (no ACK needed, one clean transmission per copy); 0 = unicast to RECEIVER (the ESP8266 never sees the ESP32's ACK)
#define TRIES      5             // copies sent (the receiver drops repeats by the counter byte). NB: an ESP8266 never sees the ESP32's ACK (status 1), so all copies go out
// -------------------------------------------------------------------------------------------------------------------

#define CFG_PIN    4             // config mode while this pin is LOW at start (GPIO4: ESP-12, not on an ESP-01; there use 3 = RX). NOT GPIO0 / 2 / 15 (boot pins)
#define CFG_TIMEOUT_S 300        // config mode ends (deep sleep) after this many seconds without a request: it must not drain the battery
#define AP_PASS    ""            // "" = open access point; else at least 8 characters

#define DEBUG 0                  // 1 = print reset reason and send status on the serial port (115200); costs ~30 ms awake, set 0 for the battery build
#define SEND_ON_POWER_ON 0       // 0 = a battery insertion / power-up sends nothing (it would toggle the lamp); only the RST button does

// ---- stored configuration -------------------------------------------------------------------------------------------
struct Cfg {
  uint8_t magic;
  uint8_t mac[6];       // receiver MAC (used only with broadcast = 0)
  uint8_t channel;      // 1..13
  uint8_t button;       // 1 = all lamps, 2..5 = zone 1..4
  uint8_t action;       // 0 off, 1 on, 2 toggle
  uint8_t bcast;        // 1 = broadcast, 0 = unicast to mac
  uint8_t crc;
};
#define CFG_MAGIC 0xB7
static Cfg cfg;

static uint8_t cfgCrc(const Cfg &c) {
  const uint8_t *p = (const uint8_t *)&c;
  uint8_t x = 0x5A;
  for (size_t i = 0; i < sizeof(Cfg) - 1; i++) { x ^= p[i]; x = (x << 1) | (x >> 7); }
  return x;
}

static void cfgDefaults() {
  cfg.magic = CFG_MAGIC;
  memcpy(cfg.mac, RECEIVER, 6);
  cfg.channel = CHANNEL;
  cfg.button = BUTTON_ID;
  cfg.action = ACTION;
  cfg.bcast = USE_BROADCAST;
  cfg.crc = cfgCrc(cfg);
}

static void cfgLoad() {
  EEPROM.begin(sizeof(Cfg));
  EEPROM.get(0, cfg);
  if (cfg.magic != CFG_MAGIC || cfg.crc != cfgCrc(cfg) || cfg.channel < 1 || cfg.channel > 13 || cfg.button < 1 || cfg.button > 5 || cfg.action > 2) {
    cfgDefaults();                                         // never configured (or damaged): the #defines above
  }
}

static void cfgSave() {
  cfg.magic = CFG_MAGIC;
  cfg.crc = cfgCrc(cfg);
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

// ---- sending --------------------------------------------------------------------------------------------------------
static volatile int sendStatus = -1;                       // -1 waiting, 0 = delivered, other = failed

static void onSent(uint8_t *mac, uint8_t status) {
  sendStatus = status;
}

// role: ESP_NOW_ROLE_CONTROLLER in normal operation (station), ESP_NOW_ROLE_COMBO while the access point of the config mode is up
static void sendPackets(uint8_t role) {
  if (0 != esp_now_init()) { return; }
  esp_now_set_self_role(role);
  esp_now_register_send_cb(onSent);
  static uint8_t BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  uint8_t *dest = cfg.bcast ? BCAST : cfg.mac;
  esp_now_add_peer(dest, ESP_NOW_ROLE_SLAVE, cfg.channel, NULL, 0);
  uint8_t pkt[4] = { 'B', cfg.button, cfg.action, (uint8_t)(os_random() & 0xFF) };   // the counter makes a repeated packet recognisable
  for (int t = 0; t < TRIES && (cfg.bcast || sendStatus != 0); t++) {
    sendStatus = -1;
    int rc = esp_now_send(dest, pkt, sizeof(pkt));
    uint32_t t0 = millis();
    while (sendStatus == -1 && (millis() - t0) < 40) { delay(1); }
    delay(COPY_GAP_MS);
#if DEBUG
    Serial.printf("[btn] try %d: send rc=%d status=%d (%u ms)\n", t, rc, sendStatus, (unsigned)(millis() - t0));
#endif
  }
}

// ---- config mode ----------------------------------------------------------------------------------------------------
static ESP8266WebServer server(80);
static DNSServer dns;                                      // captive portal: every name resolves to the button, so a phone opens the form by itself
static uint32_t lastRequest = 0;
static char apName[20];
static String note;                                        // message shown above the form

static String macText(const uint8_t *m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

static bool parseMac(const String &s, uint8_t *out) {
  unsigned v[6];
  if (6 != sscanf(s.c_str(), "%2x%*[:-]%2x%*[:-]%2x%*[:-]%2x%*[:-]%2x%*[:-]%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5])) { return false; }
  for (int i = 0; i < 6; i++) { out[i] = (uint8_t)v[i]; }
  return true;
}

static void page() {
  lastRequest = millis();
#if DEBUG
  Serial.printf("[btn] request %s from %s\n", server.uri().c_str(), server.client().remoteIP().toString().c_str());
#endif
  String h = F("<!DOCTYPE html><meta name=viewport content='width=device-width,initial-scale=1'><title>Taster</title>"
               "<style>body{font-family:sans-serif;margin:14px;max-width:420px}input,select{width:100%;padding:7px;margin:3px 0 10px;box-sizing:border-box}"
               "button{padding:9px;margin:4px 0;width:100%}.n{background:#dfd;padding:8px}</style><h3>ESP-NOW-Taster</h3>");
  if (note.length()) { h += "<p class=n>" + note + "</p>"; note = ""; }
  h += F("<form method=post action=/save>"
         "Empf&auml;nger-MAC (Lampensteuerung, Tasmota <code>Status 5</code>)<input name=mac value='");
  h += macText(cfg.mac);
  h += F("'>Kanal (wie am Router, 1&ndash;13)<input name=ch type=number min=1 max=13 value=");
  h += cfg.channel;
  h += F(">Tastencode<select name=btn>");
  const char *bn[] = { "1 = alle Lampen", "2 = Zone 1", "3 = Zone 2", "4 = Zone 3", "5 = Zone 4" };
  for (int i = 1; i <= 5; i++) { h += "<option value=" + String(i) + (cfg.button == i ? " selected>" : ">") + bn[i - 1] + "</option>"; }
  h += F("</select>Aktion<select name=act>");
  const char *an[] = { "0 = aus", "1 = ein", "2 = umschalten" };
  for (int i = 0; i <= 2; i++) { h += "<option value=" + String(i) + (cfg.action == i ? " selected>" : ">") + an[i] + "</option>"; }
  h += F("</select><label><input type=checkbox name=bc value=1 style='width:auto' ");
  h += cfg.bcast ? "checked" : "";
  h += F("> Broadcast senden (empfohlen: der ESP8266 sieht die Best&auml;tigung des ESP32 nie)</label><br>"
         "<button>Speichern</button><button formaction=/test>Speichern und Test senden</button><button formaction=/done>Beenden (Tiefschlaf)</button></form>");
  server.send(200, "text/html", h);
}

static bool readForm() {
  uint8_t m[6];
  int ch = server.arg("ch").toInt();
  int btn = server.arg("btn").toInt();
  int act = server.arg("act").toInt();
  if (!parseMac(server.arg("mac"), m)) { note = "MAC nicht lesbar (Form AA:BB:CC:DD:EE:FF)"; return false; }
  if (ch < 1 || ch > 13) { note = "Kanal muss 1 bis 13 sein"; return false; }
  if (btn < 1 || btn > 5 || act < 0 || act > 2) { note = "Tastencode oder Aktion ung&uuml;ltig"; return false; }
  memcpy(cfg.mac, m, 6);
  cfg.channel = ch;
  cfg.button = btn;
  cfg.action = act;
  cfg.bcast = server.arg("bc") == "1" ? 1 : 0;
  cfgSave();
  WiFi.softAP(apName, strlen(AP_PASS) >= 8 ? AP_PASS : NULL, cfg.channel);   // the access point follows the new channel, so the test uses it
  return true;
}

// captive portal: the probes phones / laptops send to find out whether the network has internet (Android /generate_204, iOS
// /hotspot-detect.html, Windows /connecttest.txt ...) and every other unknown page go to the form
static void redirectToForm() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

static void configMode() {
  cfgLoad();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  snprintf(apName, sizeof(apName), "Taster-%02X%02X%02X", mac[3], mac[4], mac[5]);
  WiFi.softAP(apName, strlen(AP_PASS) >= 8 ? AP_PASS : NULL, cfg.channel);
  server.on("/", page);
  server.on("/save", HTTP_POST, []() {
    note = readForm() ? "Gespeichert." : note;
    page();
  });
  server.on("/test", HTTP_POST, []() {
    if (readForm()) {
      sendPackets(ESP_NOW_ROLE_COMBO);
      note = "Gespeichert, Test gesendet (5 Kopien an Kanal " + String(cfg.channel) + ").";
    }
    page();
  });
  server.on("/done", HTTP_POST, []() {
    server.send(200, "text/html", F("<meta charset=utf-8><p>Fertig, der Taster schl&auml;ft. Zum Senden den Taster dr&uuml;cken.</p>"));
    delay(300);
    ESP.deepSleep(0);
  });
  server.onNotFound(redirectToForm);
  dns.start(53, "*", WiFi.softAPIP());
  server.begin();
  lastRequest = millis();
#if DEBUG
  Serial.printf("[btn] config mode, AP %s, http://192.168.4.1/\n", apName);
#endif
  uint32_t hb = millis();
  while (millis() - lastRequest < (uint32_t)CFG_TIMEOUT_S * 1000UL) {
    dns.processNextRequest();
    server.handleClient();
    delay(2);
#if DEBUG
    if (millis() - hb > 3000) {
      hb = millis();
      Serial.printf("[btn] alive, %d station(s), channel %u, ip %s\n", WiFi.softAPgetStationNum(), (unsigned)wifi_get_channel(), WiFi.softAPIP().toString().c_str());
    }
#endif
  }
  ESP.deepSleep(0);                                        // nobody came: sleep, the battery must not be drained
}

void setup() {
#if DEBUG
  Serial.begin(115200);
  Serial.printf("\n[btn] start, CFG_PIN %d\n", CFG_PIN);
#endif
  // Config mode first: CFG_PIN low at start (jumper), whether started by RST or by the battery.
  pinMode(CFG_PIN, INPUT_PULLUP);
  delay(2);
#if DEBUG
  Serial.printf("[btn] CFG_PIN reads %d (0 = config mode)\n", digitalRead(CFG_PIN));
#endif
  if (digitalRead(CFG_PIN) == LOW) { configMode(); }

  // A key press out of deep sleep is reported as EXT_SYS_RST *or* DEEP_SLEEP_AWAKE (SDK-dependent); only a real power-up (DEFAULT_RST,
  // battery inserted) or a watchdog / exception reset is skipped.
  uint32_t why = ESP.getResetInfoPtr()->reason;
#if DEBUG
  Serial.printf("[btn] reset reason %u\n", (unsigned)why);
#endif
  if (!SEND_ON_POWER_ON && why != REASON_EXT_SYS_RST && why != REASON_DEEP_SLEEP_AWAKE) {
    ESP.deepSleep(0);                                      // power-up / watchdog: not a key press, sleep at once
  }
  cfgLoad();
  WiFi.persistent(false);                                  // never write the WiFi settings to flash
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(TX_POWER_DBM);
  wifi_station_disconnect();                               // no access point: ESP-NOW only
  wifi_promiscuous_enable(1);                              // wifi_set_channel() is only honoured in promiscuous mode on the ESP8266
  wifi_set_channel(cfg.channel);
  wifi_promiscuous_enable(0);
#if DEBUG
  Serial.printf("[btn] channel now %u (want %u)\n", (unsigned)wifi_get_channel(), (unsigned)cfg.channel);
#endif
  sendPackets(ESP_NOW_ROLE_CONTROLLER);
#if DEBUG
  Serial.println("[btn] sleeping");
  Serial.flush();
#endif
  ESP.deepSleep(0);                                        // wake only by the RST button
}

void loop() {
}
