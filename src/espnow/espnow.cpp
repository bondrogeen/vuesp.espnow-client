#include "./espnow.h"
#include "./vlp.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 6
#endif
#ifndef CLIENT_BUTTON_GPIO
#define CLIENT_BUTTON_GPIO 4
#endif

static const uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static String clientId;
static uint8_t hubMac[6];
static bool hubKnown = false;
static uint32_t seq = 0;
static uint32_t lastHello = 0;
static uint32_t lastHeartbeat = 0;
static uint32_t lastTemp = 0;
static uint32_t lastButtonPoll = 0;
static int lastButton = -1;
static String pendingEvent;

static void addPeer(const uint8_t* mac) {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static bool sendText(const String& text, bool broadcast = false) {
  const uint8_t* mac = broadcast || !hubKnown ? broadcastMac : hubMac;
  addPeer(mac);
  return esp_now_send(mac, (const uint8_t*)text.c_str(), text.length()) == ESP_OK;
}

static void sendHello() {
  sendText(vlpHello(clientId, seq++, millis(), "0.1.0", "esp32-c3", "btn,temp", ESPNOW_CHANNEL), true);
}

static void sendEvent(const String& kind, int gpio, float value, const String& unit = "") {
  String frame = vlpEvent(clientId, seq++, millis(), kind, gpio, value, unit);
  if (!hubKnown) {
    pendingEvent = frame;
    sendHello();
    return;
  }
  if (!sendText(frame)) pendingEvent = frame;
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* mac = info->src_addr;
#else
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
#endif
  VlpFrame frame;
  if (!vlpParse(data, len, frame)) return;

  if (frame.type == "HELLO_ACK" || frame.type == "HB") {
    memcpy(hubMac, mac, 6);
    hubKnown = true;
    Serial.printf("ESP-NOW: hub %s found\n", frame.src.c_str());
    if (pendingEvent.length()) {
      sendText(pendingEvent);
      pendingEvent = "";
    }
    return;
  }

  if (frame.type == "CMD") {
    Serial.printf("ESP-NOW: command %s gpio=%d value=%.2f\n", frame.action.c_str(), frame.gpio, frame.value);
    if (frame.gpio >= 0 && (frame.action == "relay" || frame.action == "led" || frame.action == "output")) {
      pinMode(frame.gpio, OUTPUT);
      digitalWrite(frame.gpio, frame.value > 0 ? HIGH : LOW);
    }
    sendText(vlpAck(clientId, frame.mid, true));
  }
}

static void onSent(const uint8_t* mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) Serial.println("ESP-NOW: send failed");
}

void espnowSetup() {
  clientId = vlpDeviceId();

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW: init failed");
    return;
  }

  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);
  addPeer(broadcastMac);

  pinMode(CLIENT_BUTTON_GPIO, INPUT_PULLUP);
  lastButton = digitalRead(CLIENT_BUTTON_GPIO);

  Serial.printf("ESP-NOW client %s, channel %d, MAC %s\n", clientId.c_str(), ESPNOW_CHANNEL, WiFi.macAddress().c_str());
  sendHello();
}

void espnowLoop(uint32_t now) {
  if (now - lastHello > 5000) {
    lastHello = now;
    if (!hubKnown) sendHello();
  }

  if (now - lastHeartbeat > 30000) {
    lastHeartbeat = now;
    if (hubKnown) sendText(vlpHeartbeat(clientId, seq++, now));
  }

  if (now - lastTemp > 30000) {
    lastTemp = now;
    float temperature = temperatureRead();
    sendEvent("temp", -1, temperature, "C");
  }

  if (now - lastButtonPoll > 20) {
    lastButtonPoll = now;
    int value = digitalRead(CLIENT_BUTTON_GPIO);
    if (value != lastButton) {
      lastButton = value;
      sendEvent("btn", CLIENT_BUTTON_GPIO, value ? 1 : 0);
      Serial.printf("button %d\n", value);
    }
  }
}

const char* espnowClientId() {
  return clientId.c_str();
}
