/*
  04_UniversalNode.ino - Zero-Code IoT Firmware (AgyGatewayClient v1.1.0)
  
  Firmware universal: upload sekali, konfigurasi sepenuhnya dari Web Dashboard & Captive Portal.
  
  Fitur Utama:
  - WiFi Captive Portal: Konfigurasi SSID, Password, IP Server dari HP/Laptop.
  - Flash Persistence: Konfigurasi pin & kredensial tersimpan permanen di Flash.
  - Dynamic Pins: Tambah/hapus sensor & relay dari Web Dashboard tanpa flash ulang.
  - Auto I2C Scan: Deteksi otomatis modul I2C (BMP280, BH1750, SHT30, AHT20, dll).
  - Delta Telemetry: Hanya mengirim data yang berubah untuk menghemat bandwidth.
  - Status LED: Indikator visual pola kedip untuk diagnosa koneksi tanpa Serial Monitor.
*/

#include <Arduino.h>
#include <AgyGatewayClient.h>

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n==========================================");
  Serial.println("  Universal IoT Node (Zero-Code Firmware)");
  Serial.printf("  AgyGatewayClient v%s\n", AGY_GATEWAY_CLIENT_VERSION);
  Serial.println("==========================================");

  // 1. Aktifkan LED Status (Indikator visual koneksi)
  iot.enableStatusLed(LED_BUILTIN, true);

  // 2. Aktifkan Mode Dynamic Pins (Konfigurasi 100% dari Web Dashboard)
  iot.enableDynamicPins(true);

  // 3. Auto-Connect: Baca kredensial dari Flash, jika gagal buka Captive Portal
  //    Buka browser di HP/Laptop, sambungkan ke AP "AGY-NODE-XXXXXX",
  //    lalu isi SSID WiFi, Password, IP Server Gateway, Device ID & Key.
  iot.autoConnect();
}

void loop() {
  // Loop utama engine IoT
  // (Auto WiFi reconnect, WebSocket, Dynamic Pin Drivers, OTA, Delta Telemetry)
  iot.loop();
}
