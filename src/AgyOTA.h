#ifndef AGY_OTA_H
#define AGY_OTA_H

#include <Arduino.h>

#if !defined(LED_BUILTIN)
  #define LED_BUILTIN 2
#endif

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
  #include <ESP8266httpUpdate.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
  #include <HTTPUpdate.h>
#endif

#include "AgyTypes.h"

class AgyOTA {
public:
  static void setProgressCallback(AgyOTAProgressCallback cb) {
    _progressCb = cb;
  }

  static bool updateFromUrl(const String& binUrl, uint8_t maxRetries = 3, uint16_t retryDelayMs = 2000) {
    Serial.printf("[OTA] Memulai pembaruan firmware dari: %s (Maksimal %d percobaan)\n", binUrl.c_str(), maxRetries);

    bool isHttps = binUrl.startsWith("https://");

    for (uint8_t attempt = 1; attempt <= maxRetries; attempt++) {
      if (attempt > 1) {
        Serial.printf("[OTA] Menjalankan percobaan ke-%d dari %d...\n", attempt, maxRetries);
        delay(retryDelayMs);
      }

      yield();

#if defined(ESP8266)
      ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
      ESPhttpUpdate.onProgress([](int cur, int total) {
        yield();
        int percent = (total > 0) ? (int)(((long)cur * 100) / total) : 0;
        static int lastPct = -1;
        if (percent / 10 != lastPct / 10) {
          lastPct = percent;
          Serial.printf("[OTA] Mengunduh: %d%% (%d / %d B)\n", percent, cur, total);
        }
        if (_progressCb) {
          _progressCb((size_t)cur, (size_t)total, percent);
        }
      });

      t_httpUpdate_return ret;
      if (isHttps) {
        WiFiClientSecure secureClient;
        secureClient.setInsecure(); // Terima self-signed SSL atau gateway lokal
        ret = ESPhttpUpdate.update(secureClient, binUrl);
      } else {
        WiFiClient client;
        ret = ESPhttpUpdate.update(client, binUrl);
      }

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("[OTA ERROR] Percobaan %d gagal (%d): %s\n", 
            attempt,
            ESPhttpUpdate.getLastError(), 
            ESPhttpUpdate.getLastErrorString().c_str());
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("[OTA] Tidak ada pembaruan pada server");
          return false;
        case HTTP_UPDATE_OK:
          Serial.println("[OTA] Pembaruan Berhasil! Memulai ulang sistem...");
          return true;
      }

#elif defined(ESP32)
      httpUpdate.setLedPin(LED_BUILTIN, HIGH);
      httpUpdate.onProgress([](size_t cur, size_t total) {
        yield();
        int percent = (total > 0) ? (int)(((long)cur * 100) / total) : 0;
        static int lastPct = -1;
        if (percent / 10 != lastPct / 10) {
          lastPct = percent;
          Serial.printf("[OTA] Mengunduh: %d%% (%u / %u B)\n", percent, (unsigned)cur, (unsigned)total);
        }
        if (_progressCb) {
          _progressCb(cur, total, percent);
        }
      });

      t_httpUpdate_return ret;
      if (isHttps) {
        WiFiClientSecure secureClient;
        secureClient.setInsecure();
        ret = httpUpdate.update(secureClient, binUrl);
      } else {
        WiFiClient client;
        ret = httpUpdate.update(client, binUrl);
      }

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("[OTA ERROR] Percobaan %d gagal (%d): %s\n", 
            attempt,
            httpUpdate.getLastError(), 
            httpUpdate.getLastErrorString().c_str());
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("[OTA] Tidak ada pembaruan pada server");
          return false;
        case HTTP_UPDATE_OK:
          Serial.println("[OTA] Pembaruan Berhasil! Memulai ulang sistem...");
          return true;
      }
#endif
    }

    Serial.println("[OTA ERROR] Seluruh percobaan pembaruan firmware gagal.");
    return false;
  }

private:
  static AgyOTAProgressCallback _progressCb;
};

// Definisi variabel statis callback
__attribute__((weak)) AgyOTAProgressCallback AgyOTA::_progressCb = nullptr;

#endif // AGY_OTA_H
