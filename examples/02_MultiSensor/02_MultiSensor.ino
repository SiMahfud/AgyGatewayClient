#include <Arduino.h>
#include <AgyGatewayClient.h>

const char* WIFI_SSID = "Khumaira";
const char* WIFI_PASS = "internet";
const char* WS_HOST = "192.168.1.100";
const uint16_t WS_PORT = 3050;
const char* WS_PATH = "/ws";
const char* DEVICE_ID = "esp-weather-station";
const char* DEVICE_KEY = "wemos-secret-key-3377";

AgyGatewayClient iot;

// Contoh fungsi simulasi / pembacaan sensor manual
float readTemperature() {
  return 27.5 + (random(-10, 10) / 10.0);
}

float readHumidity() {
  return 60.0 + (random(-5, 5) / 1.0);
}

float readSoilAnalog() {
  return analogRead(A0);
}

void setup() {
  Serial.begin(115200);

  // 1. Aktifkan Hardware Watchdog Timer (8 detik)
  iot.enableWatchdog(8);

  // 2. Aktifkan Status LED
  iot.enableStatusLed(LED_BUILTIN, true);

  // 3. Tambah sensor suhu manual (dibaca otomatis setiap 5 detik)
  iot.addSensor("temp_dht", "Suhu Ruangan", "°C", readTemperature, 5000);

  // 4. Tambah sensor kelembapan manual (dibaca otomatis setiap 5 detik)
  iot.addSensor("hum_dht", "Kelembapan Udara", "%", readHumidity, 5000);

  // 5. Tambah sensor tanah analog di pin A0 (dibaca otomatis setiap 10 detik)
  iot.addSensor("soil_moist", "Kelembapan Tanah", "ADC", readSoilAnalog, 10000);

  // 6. Tambah aktuator saklar relay untuk otomatisasi pompa
  iot.addSwitch("relay_pump", 14, "Pompa Penyiram", true);

  // Konek ke Gateway Server
  iot.begin(WIFI_SSID, WIFI_PASS, WS_HOST, WS_PORT, WS_PATH, DEVICE_ID, DEVICE_KEY);
}

void loop() {
  iot.loop();
}
