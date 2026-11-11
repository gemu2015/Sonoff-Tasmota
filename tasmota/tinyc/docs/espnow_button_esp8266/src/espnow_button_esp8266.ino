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
#define TRIES      4             // sends until the radio reports "delivered"
// -------------------------------------------------------------------------------------------------------------------

#define SEND_ON_POWER_ON 0       // 0 = a battery insertion / power-up sends nothing (it would toggle the lamp); only the RST button does

static volatile int sendStatus = -1;                       // -1 waiting, 0 = delivered, other = failed

static void onSent(uint8_t *mac, uint8_t status) {
  sendStatus = status;
}

void setup() {
  if (!SEND_ON_POWER_ON && ESP.getResetInfoPtr()->reason != REASON_EXT_SYS_RST) {
    ESP.deepSleep(0);                                      // power-up / watchdog / flashing: not a key press, sleep at once
  }
  WiFi.persistent(false);                                  // never write the WiFi settings to flash
  WiFi.mode(WIFI_STA);
  wifi_station_disconnect();                               // no access point: ESP-NOW only
  wifi_set_channel(CHANNEL);

  if (0 == esp_now_init()) {
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_register_send_cb(onSent);
    esp_now_add_peer(RECEIVER, ESP_NOW_ROLE_SLAVE, CHANNEL, NULL, 0);
    uint8_t pkt[4] = { 'B', BUTTON_ID, ACTION, (uint8_t)(os_random() & 0xFF) };   // the counter makes a repeated packet recognisable
    for (int t = 0; t < TRIES && sendStatus != 0; t++) {
      sendStatus = -1;
      esp_now_send(RECEIVER, pkt, sizeof(pkt));
      uint32_t t0 = millis();
      while (sendStatus == -1 && (millis() - t0) < 40) { delay(1); }
    }
  }
  ESP.deepSleep(0);                                        // wake only by the RST button
}

void loop() {
}
