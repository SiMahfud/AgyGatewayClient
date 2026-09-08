/*
  UniversalNode.ino - Zero-Code IoT Firmware (AgyGatewayClient)
  
  Cukup upload sekali ke Wemos D1 Mini / NodeMCU ESP8266 / ESP32.
  Semua konfigurasi sensor (DHT11/22, DS18B20, Analog, PIR) dan saklar relay
  serta pemindaian I2C diatur sepenuhnya melalui Web Dashboard!
*/

#include <Arduino.h>
#include <AgyGatewayClient.h>

AgyGatewayClient iot;

// Konfigurasi Jaringan & Server
const char* WIFI_SSID   = "Nama_WiFi_Anda";
const char* WIFI_PASS   = "Password_WiFi";
const char* WS_SERVER   = "192.168.1.100";  // Alamat IP Server Gateway
const uint16_t WS_PORT  = 3050;             // Port Server
const char* WS_PATH     = "/ws";
const char* DEVICE_ID   = "node-universal-01";
const char* DEVICE_KEY  = "SECRET_KEY";

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n==========================================");
  Serial.println("  Universal IoT Node (Zero-Code Firmware)");
  Serial.println("==========================================");

  // 1. Aktifkan Mode Dynamic Pins (Konfigurasi 100% dari Web Dashboard)
  iot.enableDynamicPins(true);

  // 2. Sambungkan ke WiFi & Server Gateway
  iot.begin(WIFI_SSID, WIFI_PASS, WS_SERVER, WS_PORT, WS_PATH, DEVICE_ID, DEVICE_KEY);
}

void loop() {
  // Loop utama engine IoT (Auto WiFi reconnect, WebSocket, dynamic pin drivers, OTA)
  iot.loop();
}
