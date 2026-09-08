#include <Arduino.h>
#include <AgyGatewayClient.h>

// Kredensial WiFi
const char* WIFI_SSID = "Khumaira";
const char* WIFI_PASS = "internet";

// Kredensial Server Gateway Anda
const char* WS_HOST = "192.168.1.100"; // Ganti dengan IP atau domain gateway Anda
const uint16_t WS_PORT = 3050;
const char* WS_PATH = "/ws";
const char* DEVICE_ID = "wemos-switch-01";
const char* DEVICE_KEY = "wemos-secret-key-3377";

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);

  // 1. Aktifkan Hardware Watchdog Timer (8 detik)
  // Mencegah mikrokontroler hang permanen
  iot.enableWatchdog(8);

  // 2. Aktifkan LED Status (Pin bawaan modul)
  iot.enableStatusLed(LED_BUILTIN, true);

  // 3. Daftarkan saklar relay & dimmer PWM modular:
  // Pin pada Wemos D1 Mini: D1 = GPIO 5, D2 = GPIO 4, D5 = GPIO 14, D6 = GPIO 12
  iot.addSwitch("relay_1", 5,  "Lampu Utama", true);
  iot.addSwitch("relay_2", 4,  "Kipas Angin", true);
  iot.addSwitch("relay_3", 14, "Pompa Air",   true);

  // Daftarkan actuator Dimmer / PWM slider (0-100%) di GPIO 12
  // Contoh: Pengatur kecerahan lampu tidur atau kecepatan kipas DC
  iot.addDimmer("dimmer_1", 12, "Lampu Tidur (Dimmer)", "%", 50);

  // 4. Konek ke WiFi & Server Gateway
  iot.begin(WIFI_SSID, WIFI_PASS, WS_HOST, WS_PORT, WS_PATH, DEVICE_ID, DEVICE_KEY);
}

void loop() {
  // Menangani koneksi, kontrol relay, dimmer PWM, timer countdown mandiri, WDT, dan OTA
  iot.loop();
}
