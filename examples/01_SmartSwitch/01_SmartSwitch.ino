#include <Arduino.h>
#include <AgyGatewayClient.h>

// Kredensial WiFi
const char* WIFI_SSID = "Khumaira";
const char* WIFI_PASS = "internet";

// Kredensial Server Gateway Anda
const char* WS_HOST = "192.168.1.100"; // Ganti dengan IP PC atau domain server Anda
const uint16_t WS_PORT = 3050;
const char* WS_PATH = "/ws";
const char* DEVICE_ID = "wemos-switch-01";
const char* DEVICE_KEY = "wemos-secret-key-3377";

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);

  // Daftarkan saklar / relay modular yang terpasang pada pin Wemos D1 Mini:
  // D1 = GPIO 5, D2 = GPIO 4, D5 = GPIO 14, D6 = GPIO 12
  iot.addSwitch("relay_1", 5,  "Lampu Utama", true);
  iot.addSwitch("relay_2", 4,  "Kipas Angin", true);
  iot.addSwitch("relay_3", 14, "Pompa Air",   true);
  iot.addSwitch("relay_4", 12, "Stopkontak",  true);

  // Konek ke WiFi & Server Gateway
  iot.begin(WIFI_SSID, WIFI_PASS, WS_HOST, WS_PORT, WS_PATH, DEVICE_ID, DEVICE_KEY);
}

void loop() {
  iot.loop(); // Menangani koneksi, kontrol relay, timer countdown, dan OTA
}
