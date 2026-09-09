/*
  Universal IoT Node Firmware (AgyGatewayClient v1.2.0)
  Universal zero-code firmware for ESP8266 & ESP32 (termasuk ESP32-CAM).
  Flash once, configure completely via Captive Portal & Web Dashboard.
*/

#include <Arduino.h>

// =====================================================
// [FIX] Nonaktifkan Brownout Detector untuk ESP32/ESP32-CAM
// Modul ESP32-CAM sangat sensitif terhadap drop tegangan
// saat radio WiFi dinyalakan. Ini adalah workaround standar.
// =====================================================
#if defined(ESP32)
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif

#include <AgyGatewayClient.h>

AgyGatewayClient iot;

void setup() {
  // Nonaktifkan brownout detector seawal mungkin (sebelum WiFi aktif)
#if defined(ESP32)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

  Serial.begin(115200);
  delay(500);

  Serial.println("\n==========================================");
  Serial.println("  Universal IoT Node (Zero-Code Firmware)");
  Serial.printf("  AgyGatewayClient v%s\n", AGY_GATEWAY_CLIENT_VERSION);
#if defined(ESP8266)
  Serial.println("  Platform: ESP8266");
#elif defined(ESP32)
  Serial.println("  Platform: ESP32");
  Serial.println("  Brownout Detector: DISABLED");
#endif
  Serial.println("==========================================");

#if !defined(LED_BUILTIN)
  #define LED_BUILTIN 2
#endif

  // 1. Hardware Watchdog Timer (8 detik)
  iot.enableWatchdog(8);

  // 2. LED Status Indikator
  iot.enableStatusLed(LED_BUILTIN, true);

  // 3. Dynamic Pin Manager (Zero-Code Onboarding)
  iot.enableDynamicPins(true);

  // 4. Auto-Connect / Captive Portal
  // Otomatis membuka Open AP AGY-NODE-[CHIPID] jika belum ada config WiFi tersimpan
  iot.autoConnect(nullptr, nullptr, 60);
}

void loop() {
  iot.loop();
}
