#ifndef AGY_STORAGE_H
#define AGY_STORAGE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#if defined(ESP8266)
  #include <LittleFS.h>
#elif defined(ESP32)
  #include <LittleFS.h>
#endif

#include "AgyTypes.h"

class AgyStorage {
public:
  static bool init() {
    static bool fsInitialized = false;
    if (fsInitialized) return true;
#if defined(ESP8266)
    if (!LittleFS.begin()) {
      Serial.println("[STORAGE] Gagal mount LittleFS. Memformat...");
      LittleFS.format();
      fsInitialized = LittleFS.begin();
      return fsInitialized;
    }
    fsInitialized = true;
    return true;
#elif defined(ESP32)
    if (!LittleFS.begin(true)) {
      Serial.println("[STORAGE] Gagal mount LittleFS di ESP32. Memformat...");
      LittleFS.format();
      fsInitialized = LittleFS.begin(true);
      if (fsInitialized) {
        Serial.println("[STORAGE] LittleFS berhasil diformat dan dimount.");
      } else {
        Serial.println("[STORAGE ERROR] LittleFS gagal dimount setelah format!");
      }
      return fsInitialized;
    }
    fsInitialized = true;
    Serial.println("[STORAGE] LittleFS ESP32 berhasil dimount.");
    return true;
#else
    return false;
#endif
  }

  // Simpan kredensial WiFi & Konfigurasi Gateway
  static bool saveNetworkConfig(const String& ssid, const String& pass, 
                                const String& host, uint16_t port, const String& path, 
                                const String& devId, const String& devKey, bool useSsl = false) {
    if (!init()) return false;

    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["pass"] = pass;
    doc["host"] = host;
    doc["port"] = port;
    doc["path"] = path;
    doc["devId"] = devId;
    doc["devKey"] = devKey;
    doc["ssl"] = useSsl;

    File f = LittleFS.open("/agy_config.json", "w");
    if (!f) {
      Serial.println("[STORAGE ERROR] Gagal membuka /agy_config.json untuk menulis");
      return false;
    }

    serializeJson(doc, f);
    f.close();
    Serial.println("[STORAGE] Konfigurasi jaringan berhasil disimpan ke Flash!");
    return true;
  }

  // Muat kredensial WiFi & Konfigurasi Gateway (dengan info SSL)
  static bool loadNetworkConfig(String& ssid, String& pass, 
                                String& host, uint16_t& port, String& path, 
                                String& devId, String& devKey, bool& useSsl) {
    if (!init()) return false;
    if (!LittleFS.exists("/agy_config.json")) return false;

    File f = LittleFS.open("/agy_config.json", "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
      Serial.printf("[STORAGE ERROR] Gagal parse /agy_config.json: %s\n", err.c_str());
      return false;
    }

    ssid = doc["ssid"] | "";
    pass = doc["pass"] | "";
    host = doc["host"] | "";
    port = doc["port"] | 3050;
    path = doc["path"] | "/ws";
    devId = doc["devId"] | "";
    devKey = doc["devKey"] | "";
    useSsl = doc["ssl"] | (port == 443);

    return (ssid.length() > 0 && devId.length() > 0);
  }

  // Overload ringkas kompatibilitas lama
  static bool loadNetworkConfig(String& ssid, String& pass, 
                                String& host, uint16_t& port, String& path, 
                                String& devId, String& devKey) {
    bool dummySsl = false;
    return loadNetworkConfig(ssid, pass, host, port, path, devId, devKey, dummySsl);
  }

  // Simpan seluruh konfigurasi dynamic pins ke Flash
  static bool savePinConfig(const std::vector<AgyComponent*>& components) {
    if (!init()) return false;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < components.size(); i++) {
      if (!components[i]) continue;
      const AgyComponent& c = *components[i];
      if (!c.isDynamic) continue; // Hanya simpan komponen dinamis

      JsonObject item = arr.add<JsonObject>();
      item["id"] = c.id;
      item["name"] = c.name;
      item["type"] = agyComponentTypeToString(c.type);
      item["driver"] = agyDriverTypeToString(c.driverType);
      item["pin"] = c.pin;
      item["activeLow"] = c.activeLow;
      item["pullup"] = c.pullup;
      item["unit"] = c.unit;
      item["interval"] = c.readIntervalMs;
      if (c.i2cAddress > 0) {
        item["i2cAddr"] = c.i2cAddress;
      }
    }

    File f = LittleFS.open("/agy_pins.json", "w");
    if (!f) {
      Serial.println("[STORAGE ERROR] Gagal membuka /agy_pins.json");
      return false;
    }

    serializeJson(doc, f);
    f.close();
    Serial.printf("[STORAGE] %d pin dinamis berhasil disimpan ke Flash!\n", (int)arr.size());
    return true;
  }

  // Muat konfigurasi dynamic pins dari Flash
  static bool loadPinConfig(JsonDocument& doc) {
    if (!init()) return false;
    if (!LittleFS.exists("/agy_pins.json")) return false;

    File f = LittleFS.open("/agy_pins.json", "r");
    if (!f) return false;

    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
      Serial.printf("[STORAGE ERROR] Gagal parse /agy_pins.json: %s\n", err.c_str());
      return false;
    }

    return true;
  }

  // Hapus konfigurasi pin dinamis
  static void clearPinConfig() {
    if (init() && LittleFS.exists("/agy_pins.json")) {
      LittleFS.remove("/agy_pins.json");
      Serial.println("[STORAGE] File /agy_pins.json dihapus.");
    }
  }

  // Reset pabrik (hapus semua konfigurasi)
  static void clearAll() {
    if (init()) {
      LittleFS.remove("/agy_config.json");
      LittleFS.remove("/agy_pins.json");
      Serial.println("[STORAGE] Seluruh data konfigurasi di Flash dihapus.");
    }
  }
};

#endif // AGY_STORAGE_H
