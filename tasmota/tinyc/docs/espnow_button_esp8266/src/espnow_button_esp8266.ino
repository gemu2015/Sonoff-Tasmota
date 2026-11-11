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
#include <ESP8266WiFi.h>
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

#define DEBUG 0                  // 1 = print reset reason and send status on the serial port (115200); costs ~30 ms awake, set 0 for the battery build
#define SEND_ON_POWER_ON 0       // 0 = a battery insertion / power-up sends nothing (it would toggle the lamp); only the RST button does

static volatile int sendStatus = -1;                       // -1 waiting, 0 = delivered, other = failed

static void onSent(uint8_t *mac, uint8_t status) {
  sendStatus = status;
}

void setup() {
  // A key press out of deep sleep is reported as EXT_SYS_RST *or* DEEP_SLEEP_AWAKE (SDK-dependent); only a real power-up (DEFAULT_RST,
  // battery inserted) or a watchdog / exception reset is skipped.
  uint32_t why = ESP.getResetInfoPtr()->reason;
#if DEBUG
  Serial.begin(115200);
  Serial.printf("\n[btn] reset reason %u\n", (unsigned)why);
#endif
  if (!SEND_ON_POWER_ON && why != REASON_EXT_SYS_RST && why != REASON_DEEP_SLEEP_AWAKE) {
    ESP.deepSleep(0);                                      // power-up / watchdog: not a key press, sleep at once
  }
  WiFi.persistent(false);                                  // never write the WiFi settings to flash
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(TX_POWER_DBM);
  wifi_station_disconnect();                               // no access point: ESP-NOW only
  wifi_promiscuous_enable(1);                              // wifi_set_channel() is only honoured in promiscuous mode on the ESP8266
  wifi_set_channel(CHANNEL);
  wifi_promiscuous_enable(0);
#if DEBUG
  Serial.printf("[btn] channel now %u (want %u)\n", (unsigned)wifi_get_channel(), (unsigned)CHANNEL);
#endif

  if (0 == esp_now_init()) {
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_register_send_cb(onSent);
    static uint8_t BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t *dest = USE_BROADCAST ? BCAST : RECEIVER;
    esp_now_add_peer(dest, ESP_NOW_ROLE_SLAVE, CHANNEL, NULL, 0);
    uint8_t pkt[4] = { 'B', BUTTON_ID, ACTION, (uint8_t)(os_random() & 0xFF) };   // the counter makes a repeated packet recognisable
    for (int t = 0; t < TRIES && (USE_BROADCAST || sendStatus != 0); t++) {
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
#if DEBUG
  Serial.println("[btn] sleeping");
  Serial.flush();
#endif
  ESP.deepSleep(0);                                        // wake only by the RST button
}

void loop() {
}
