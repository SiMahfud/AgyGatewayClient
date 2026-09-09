/*
  04_UniversalNode.ino - Zero-Code Universal IoT Firmware (AgyGatewayClient v1.2.0)
  
  Firmware universal: upload sekali, konfigurasi sepenuhnya dari Web Dashboard & Captive Portal.
  
  Fitur Utama:
  - 🌐 WiFi Captive Portal: Konfigurasi SSID, Password WiFi, IP Server, Device ID & Key via HP/Laptop.
    Mendukung proteksi kata sandi WPA2 pada Access Point lokal.
  - 💾 Flash Persistence (LittleFS): Konfigurasi pin & kredensial tersimpan permanen di Flash chip.
  - 🔌 Dynamic Pins: Tambah/hapus sensor, dimmer, dan relay dari Web Dashboard tanpa flash ulang.
  - 🔍 Auto I2C Scan & Native Drivers: Deteksi otomatis modul I2C (BMP280, BH1750, SHT30, AHT20, DHT11/22).
  - 🐕 Hardware Watchdog Timer: Otomatis me-restart mikrokontroler jika terjadi lock/freeze hardware.
  - ⚡ Delta Telemetry & Rate Limiting: Transmisi efisien hanya saat data berubah, aman dari packet burst.
  - 💡 Status LED: Indikator visual pola kedip untuk diagnosa koneksi tanpa Serial Monitor.
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

#if !defined(LED_BUILTIN)
  #define LED_BUILTIN 2
#endif

  // 1. Aktifkan Hardware Watchdog Timer (8 detik)
  // Menjaga mikrokontroler tetap tangguh dan pulih dari freeze tak terduga
  iot.enableWatchdog(8);

  // 2. Aktifkan LED Status (Indikator visual koneksi)
  iot.enableStatusLed(LED_BUILTIN, true);

  // 3. Aktifkan Mode Dynamic Pins (Konfigurasi 100% dari Web Dashboard / Flash)
  iot.enableDynamicPins(true);

  // 4. Auto-Connect:
  //    Membaca kredensial tersimpan dari Flash LittleFS.
  //    Jika belum ada atau gagal konek ke WiFi dalam timeout, otomatis membuka
  //    Access Point Captive Portal Terbuka: AGY-NODE-[CHIP_ID]
  iot.autoConnect(nullptr, nullptr, 60);
}

void loop() {
  // Loop utama engine IoT:
  // Menangani auto WiFi reconnect, WebSocket client, dynamic pin drivers,
  // pembacaan I2C Bosch/Dallas, timer countdown lokal, feed watchdog, dan OTA.
  iot.loop();
}
