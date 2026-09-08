#include <Arduino.h>
#include <AgyGatewayClient.h>

const char* WIFI_SSID = "Khumaira";
const char* WIFI_PASS = "internet";
const char* WS_HOST = "192.168.1.100";
const uint16_t WS_PORT = 3050;
const char* WS_PATH = "/ws";
const char* DEVICE_ID = "esp-virtual-node";
const char* DEVICE_KEY = "wemos-secret-key-3377";

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);

  // Daftarkan listener untuk Virtual Pin V1 dari server/dashboard
  iot.onVirtualWrite("V1", [](const String& pin, const String& value) {
    Serial.printf("[VIRTUAL] Pin %s menerima perintah: %s\n", pin.c_str(), value.c_str());
    // Lakukan aksi kustom Anda di sini
  });

  // Listener kontrol universal
  iot.onCommand([](const String& compId, const String& value) {
    Serial.printf("[COMMAND] Komponen %s diatur ke: %s\n", compId.c_str(), value.c_str());
  });

  iot.begin(WIFI_SSID, WIFI_PASS, WS_HOST, WS_PORT, WS_PATH, DEVICE_ID, DEVICE_KEY);
}

void loop() {
  iot.loop();

  // Kirim data telemetri kustom ke Virtual Pin V2 setiap 5 detik
  static unsigned long lastTime = 0;
  if (millis() - lastTime > 5000) {
    lastTime = millis();
    int dummyCounter = random(0, 100);
    iot.virtualWrite("V2", dummyCounter);
  }
}
