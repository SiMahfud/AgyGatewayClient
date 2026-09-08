#ifndef AGY_OTA_H
#define AGY_OTA_H

#include <Arduino.h>

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

class AgyOTA {
public:
  static bool updateFromUrl(const String& binUrl) {
    Serial.printf("[OTA] Memulai pembaruan firmware dari: %s\n", binUrl.c_str());

    bool isHttps = binUrl.startsWith("https://");

#if defined(ESP8266)
    t_httpUpdate_return ret;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

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
        Serial.printf("[OTA ERROR] Gagal (%d): %s\n", 
          ESPhttpUpdate.getLastError(), 
          ESPhttpUpdate.getLastErrorString().c_str());
        return false;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("[OTA] Tidak ada pembaruan");
        return false;
      case HTTP_UPDATE_OK:
        Serial.println("[OTA] Sukses! Restarting...");
        return true;
    }
#elif defined(ESP32)
    t_httpUpdate_return ret;
    httpUpdate.setLedPin(LED_BUILTIN, HIGH);

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
        Serial.printf("[OTA ERROR] Gagal (%d): %s\n", 
          httpUpdate.getLastError(), 
          httpUpdate.getLastErrorString().c_str());
        return false;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("[OTA] Tidak ada pembaruan");
        return false;
      case HTTP_UPDATE_OK:
        Serial.println("[OTA] Sukses! Restarting...");
        return true;
    }
#endif

    return false;
  }
};

#endif // AGY_OTA_H
