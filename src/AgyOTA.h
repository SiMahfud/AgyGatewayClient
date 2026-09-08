#ifndef AGY_OTA_H
#define AGY_OTA_H

#include <Arduino.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266httpUpdate.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <HTTPUpdate.h>
#endif

class AgyOTA {
public:
  static bool updateFromUrl(const String& binUrl) {
    Serial.printf("[OTA] Memulai pembaruan firmware dari: %s\n", binUrl.c_str());

    WiFiClient client;

#if defined(ESP8266)
    // ESP8266 HTTP Update
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, binUrl);

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
    // ESP32 HTTP Update
    httpUpdate.setLedPin(LED_BUILTIN, HIGH);
    t_httpUpdate_return ret = httpUpdate.update(client, binUrl);

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
