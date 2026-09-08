#include "AgyGatewayClient.h"

AgyGatewayClient::AgyGatewayClient() {
}

void AgyGatewayClient::begin(const char* ssid, const char* pass, const char* wsHost, uint16_t wsPort, const char* wsPath, const char* deviceId, const char* deviceKey, bool useSsl) {
  _ssid = ssid;
  _pass = pass;
  _wsHost = wsHost;
  _wsPort = wsPort;
  _wsPath = wsPath;
  _deviceId = deviceId;
  _deviceKey = deviceKey;
  _useSsl = useSsl;

  setupWiFi();
  initWebSocket();
}

void AgyGatewayClient::begin(const char* ssid, const char* pass, const char* wsUrl, const char* deviceId, const char* deviceKey) {
  _ssid = ssid;
  _pass = pass;
  _deviceId = deviceId;
  _deviceKey = deviceKey;

  // Sederhana: parse URL jika ada format "ws://" atau "wss://"
  String url = String(wsUrl);
  _useSsl = url.startsWith("wss://") || url.startsWith("https://");

  int prefixLen = 0;
  if (url.startsWith("ws://")) prefixLen = 5;
  else if (url.startsWith("wss://")) prefixLen = 6;
  else if (url.startsWith("http://")) prefixLen = 7;
  else if (url.startsWith("https://")) prefixLen = 8;

  String hostAndPort = url.substring(prefixLen);
  int slashIdx = hostAndPort.indexOf('/');
  String hostPart = (slashIdx >= 0) ? hostAndPort.substring(0, slashIdx) : hostAndPort;
  _wsPath = (slashIdx >= 0) ? hostAndPort.substring(slashIdx) : "/ws";

  int colonIdx = hostPart.indexOf(':');
  if (colonIdx >= 0) {
    _wsHost = hostPart.substring(0, colonIdx);
    _wsPort = hostPart.substring(colonIdx + 1).toInt();
  } else {
    _wsHost = hostPart;
    _wsPort = _useSsl ? 443 : 80;
  }

  setupWiFi();
  initWebSocket();
}

void AgyGatewayClient::setupWiFi() {
  Serial.printf("[WIFI] Menyambungkan ke: %s\n", _ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid.c_str(), _pass.c_str());
}

void AgyGatewayClient::initWebSocket() {
  Serial.printf("[WS INIT] Target Server: %s:%d%s (SSL: %s)\n", 
    _wsHost.c_str(), _wsPort, _wsPath.c_str(), _useSsl ? "YES" : "NO");

  if (_useSsl) {
#if defined(ESP8266)
    _ws.beginSSL(_wsHost.c_str(), _wsPort, _wsPath.c_str());
#else
    _ws.beginSSL(_wsHost.c_str(), _wsPort, _wsPath.c_str());
#endif
  } else {
    _ws.begin(_wsHost.c_str(), _wsPort, _wsPath.c_str());
  }

  _ws.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
    this->handleWsEvent(type, payload, length);
  });

  _ws.setReconnectInterval(5000);
  _ws.enableHeartbeat(15000, 3000, 2);
}

void AgyGatewayClient::loop() {
  // 1. Cek status koneksi WiFi
  if (WiFi.status() == WL_CONNECTED) {
    _ws.loop();
  }

  // 2. Cek pembacaan sensor terjadwal (fungsi C++)
  checkSensorIntervals();

  // 2b. Cek sensor dinamis & input digital jika Dynamic Pins aktif
  if (_dynamicPinsEnabled) {
    checkDigitalInputs();
    checkDynamicSensors();
  }

  // 3. Cek timer countdown pada switch
  checkCountdownTimers();

  // 4. Heartbeat berkala (lapor status / telemetri jika terhubung)
  if (_wsConnected && (millis() - _lastHeartbeat > _heartbeatInterval)) {
    _lastHeartbeat = millis();
    sendTelemetry();
  }
}

// -------------------------------------------------------------
// Registrasi & Manajemen Komponen
// -------------------------------------------------------------
AgyComponent* AgyGatewayClient::addSwitch(const String& id, int pin, const String& name, bool activeLow, bool initialState) {
  AgyComponent comp;
  comp.id = id;
  comp.name = name.length() > 0 ? name : id;
  comp.type = AGY_SWITCH;
  comp.access = "rw";
  comp.unit = "";
  comp.pin = pin;
  comp.activeLow = activeLow;
  comp.value = initialState ? "true" : "false";

  if (pin >= 0) {
    pinMode(pin, OUTPUT);
    applySwitchState(comp, initialState);
  }

  _components.push_back(comp);
  return &_components.back();
}

AgyComponent* AgyGatewayClient::addSensor(const String& id, const String& name, const String& unit, std::function<float()> readFn, unsigned long intervalMs) {
  AgyComponent comp;
  comp.id = id;
  comp.name = name.length() > 0 ? name : id;
  comp.type = AGY_SENSOR;
  comp.access = "r";
  comp.unit = unit;
  comp.value = "0";
  comp.readSensorFn = readFn;
  comp.readIntervalMs = intervalMs;
  comp.lastReadMs = millis();

  // Baca pertama kali jika ada fungsi
  if (readFn) {
    float val = readFn();
    comp.value = String(val, 2);
  }

  _components.push_back(comp);
  return &_components.back();
}

AgyComponent* AgyGatewayClient::addComponent(const AgyComponent& comp) {
  _components.push_back(comp);
  return &_components.back();
}

void AgyGatewayClient::updateSensor(const String& id, float value) {
  updateSensor(id, String(value, 2));
}

void AgyGatewayClient::updateSensor(const String& id, const String& value) {
  AgyComponent* comp = findComponent(id);
  if (comp) {
    if (comp->value != value) {
      comp->value = value;
      comp->isChanged = true;
      if (_wsConnected) {
        sendTelemetry();
      }
    }
  }
}

void AgyGatewayClient::setSwitchState(const String& id, bool state) {
  AgyComponent* comp = findComponent(id);
  if (comp && comp->type == AGY_SWITCH) {
    applySwitchState(*comp, state);
    comp->value = state ? "true" : "false";
    comp->isChanged = true;
    if (_wsConnected) {
      sendTelemetry();
    }
  }
}

void AgyGatewayClient::virtualWrite(const String& vPin, const String& value) {
  updateSensor(vPin, value);
}

void AgyGatewayClient::virtualWrite(const String& vPin, int value) {
  virtualWrite(vPin, String(value));
}

void AgyGatewayClient::virtualWrite(const String& vPin, float value) {
  virtualWrite(vPin, String(value, 2));
}

void AgyGatewayClient::onVirtualWrite(const String& vPin, AgyVirtualWriteCallback callback) {
  _virtualCallbacks[vPin] = callback;
}

void AgyGatewayClient::onCommand(AgyCommandCallback callback) {
  _commandCallback = callback;
}

void AgyGatewayClient::onConnection(AgyConnectionCallback callback) {
  _connCallback = callback;
}

AgyComponent* AgyGatewayClient::findComponent(const String& id) {
  for (size_t i = 0; i < _components.size(); i++) {
    if (_components[i].id == id) {
      return &_components[i];
    }
  }
  return nullptr;
}

void AgyGatewayClient::applySwitchState(AgyComponent& comp, bool state) {
  if (comp.pin >= 0) {
    if (comp.activeLow) {
      digitalWrite(comp.pin, state ? LOW : HIGH);
    } else {
      digitalWrite(comp.pin, state ? HIGH : LOW);
    }
    Serial.printf("[SWITCH] %s (Pin %d) -> %s\n", comp.id.c_str(), comp.pin, state ? "ON" : "OFF");
  }
}

// -------------------------------------------------------------
// Penanganan Event WebSocket & Protokol
// -------------------------------------------------------------
void AgyGatewayClient::handleWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Terhubung ke Server Gateway! (%s)\n", payload);
      _wsConnected = true;
      sendRegisterManifest();
      if (_connCallback) _connCallback(true);
      break;

    case WStype_DISCONNECTED:
      Serial.println("[WS] Terputus dari Server Gateway");
      _wsConnected = false;
      if (_connCallback) _connCallback(false);
      break;

    case WStype_TEXT: {
      String msg = String((char*)payload);
      processIncomingJson(msg);
      break;
    }

    case WStype_ERROR:
      Serial.printf("[WS ERROR] %s\n", payload);
      break;

    default:
      break;
  }
}

void AgyGatewayClient::processIncomingJson(const String& jsonStr) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.printf("[WS] Gagal parse JSON: %s\n", err.c_str());
    return;
  }

  const char* action = doc["action"] | "";

  // 1. Perintah Kontrol Komponen Universal
  if (strcmp(action, "set_component") == 0) {
    String compId = doc["componentId"] | "";
    JsonVariant valVar = doc["value"];
    String valStr = valVar.is<bool>() ? (valVar.as<bool>() ? "true" : "false") : valVar.as<String>();
    unsigned long duration = doc["duration"] | 0;

    AgyComponent* comp = findComponent(compId);
    if (comp) {
      if (comp->type == AGY_SWITCH) {
        bool state = (valStr == "true" || valStr == "1");
        applySwitchState(*comp, state);
        comp->value = state ? "true" : "false";

        // Handle countdown timer jika ada durasi
        if (duration > 0 && state) {
          comp->timer.active = true;
          comp->timer.startMs = millis();
          comp->timer.durationMs = duration * 1000;
          comp->timer.endTimeMs = millis() + (duration * 1000);
          comp->timer.targetState = false; // Matikan saat selesai
          Serial.printf("[TIMER] Timer aktif untuk %s selama %lu detik\n", compId.c_str(), duration);
        } else {
          comp->timer.active = false;
        }
      } else {
        comp->value = valStr;
      }

      comp->isChanged = true;
      sendTelemetry();

      // Trigger user command callback jika ada
      if (_commandCallback) {
        _commandCallback(compId, valStr);
      }
    }
    return;
  }

  // 2. Backwards-compatibility: Format 'set_relay' dari controller lama
  if (strcmp(action, "set_relay") == 0) {
    int channel = doc["channel"] | 0;
    bool state = doc["state"] | false;
    unsigned long duration = doc["duration"] | 0;

    String compId = "relay_" + String(channel);
    AgyComponent* comp = findComponent(compId);
    if (comp) {
      applySwitchState(*comp, state);
      comp->value = state ? "true" : "false";

      if (duration > 0 && state) {
        comp->timer.active = true;
        comp->timer.startMs = millis();
        comp->timer.durationMs = duration * 1000;
        comp->timer.endTimeMs = millis() + (duration * 1000);
        comp->timer.targetState = false;
      } else {
        comp->timer.active = false;
      }

      comp->isChanged = true;
      sendTelemetry();
    }
    return;
  }

  // 3. Batalkan Timer
  if (strcmp(action, "cancel_timer") == 0) {
    String compId = doc["componentId"] | "";
    if (compId.length() == 0 && !doc["channel"].isNull()) {
      compId = "relay_" + String(doc["channel"].as<int>());
    }
    AgyComponent* comp = findComponent(compId);
    if (comp) {
      comp->timer.active = false;
      Serial.printf("[TIMER] Timer dibatalkan untuk %s\n", compId.c_str());
      sendTelemetry();
    }
    return;
  }

  // 4. Perintah OTA Update
  if (strcmp(action, "ota_update") == 0) {
    String binUrl = doc["url"] | "";
    if (binUrl.length() > 0) {
      Serial.printf("[OTA] Menerima instruksi OTA Update: %s\n", binUrl.c_str());
      AgyOTA::updateFromUrl(binUrl);
    }
    return;
  }

  // 5. Permintaan Status Ulang dari Server
  if (strcmp(action, "get_status") == 0) {
    sendRegisterManifest();
    return;
  }

  // 6. Virtual Pin Write dari Server
  if (strcmp(action, "virtual_write") == 0) {
    String vPin = doc["pin"] | "";
    String val = doc["value"] | "";
    if (_virtualCallbacks.find(vPin) != _virtualCallbacks.end()) {
      _virtualCallbacks[vPin](vPin, val);
    }
    return;
  }

  // 7. Dynamic Pin Management & I2C Actions
  if (strcmp(action, "apply_pin_config") == 0) {
    if (doc["components"].is<JsonArray>()) {
      applyPinConfig(doc["components"].as<JsonArray>());
    }
    return;
  }

  if (strcmp(action, "configure_pin") == 0) {
    configurePin(doc.as<JsonObject>());
    sendRegisterManifest();
    return;
  }

  if (strcmp(action, "remove_pin") == 0) {
    String compId = doc["componentId"] | doc["id"] | "";
    removePin(compId);
    return;
  }

  if (strcmp(action, "scan_i2c") == 0) {
    int sda = doc["sda"] | -1;
    int scl = doc["scl"] | -1;
    scanAndReportI2C(sda, scl);
    return;
  }
}

void AgyGatewayClient::checkSensorIntervals() {
  unsigned long now = millis();
  bool anyChanged = false;

  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent& comp = _components[i];
    if (comp.type == AGY_SENSOR && comp.readSensorFn) {
      if (now - comp.lastReadMs >= comp.readIntervalMs) {
        comp.lastReadMs = now;
        float val = comp.readSensorFn();
        String newVal = String(val, 2);
        if (newVal != comp.value) {
          comp.value = newVal;
          comp.isChanged = true;
          anyChanged = true;
        }
      }
    }
  }

  if (anyChanged && _wsConnected) {
    sendTelemetry();
  }
}

void AgyGatewayClient::checkCountdownTimers() {
  unsigned long now = millis();
  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent& comp = _components[i];
    if (comp.type == AGY_SWITCH && comp.timer.active) {
      if (now >= comp.timer.endTimeMs) {
        Serial.printf("[TIMER] Selesai untuk %s! Mematikan switch...\n", comp.id.c_str());
        comp.timer.active = false;
        applySwitchState(comp, comp.timer.targetState);
        comp.value = comp.timer.targetState ? "true" : "false";
        comp.isChanged = true;
        sendTelemetry();
      }
    }
  }
}

// -------------------------------------------------------------
// Dynamic Pin Management & I2C Bus Scanner
// -------------------------------------------------------------
void AgyGatewayClient::enableDynamicPins(bool enable) {
  _dynamicPinsEnabled = enable;
  Serial.printf("[DYNAMIC PINS] Status: %s\n", enable ? "ENABLED" : "DISABLED");
}

bool AgyGatewayClient::configurePin(const JsonObject& doc) {
  String id = doc["id"] | doc["componentId"] | "";
  if (id.length() == 0) return false;

  String name = doc["name"] | id;
  String typeStr = doc["type"] | "switch";
  String driverStr = doc["driver"] | typeStr;
  int pin = doc["pin"] | -1;
  bool activeLow = doc["activeLow"].isNull() ? true : doc["activeLow"].as<bool>();
  bool pullup = doc["pullup"].isNull() ? true : doc["pullup"].as<bool>();
  String unit = doc["unit"] | "";
  unsigned long interval = doc["interval"] | doc["readIntervalMs"] | 5000;

  AgyComponent* comp = findComponent(id);
  if (!comp) {
    AgyComponent newComp;
    newComp.id = id;
    _components.push_back(newComp);
    comp = &_components.back();
  }

  comp->id = id;
  comp->name = name;
  comp->pin = pin;
  comp->activeLow = activeLow;
  comp->pullup = pullup;
  comp->unit = unit;
  comp->readIntervalMs = interval;
  comp->isDynamic = true;
  comp->driverType = agyStringToDriverType(driverStr);

  if (comp->driverType == AGY_DRIVER_SWITCH) {
    comp->type = AGY_SWITCH;
    comp->access = "rw";
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      bool curState = (comp->value == "true" || comp->value == "1");
      applySwitchState(*comp, curState);
    }
  } else if (comp->driverType == AGY_DRIVER_DIGITAL_IN) {
    comp->type = AGY_INDICATOR;
    comp->access = "r";
    if (pin >= 0) {
      pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
      comp->lastDigitalVal = digitalRead(pin);
      comp->value = (comp->lastDigitalVal == (activeLow ? LOW : HIGH)) ? "1" : "0";
    }
  } else if (comp->driverType == AGY_DRIVER_ANALOG) {
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "";
  } else if (comp->driverType == AGY_DRIVER_DHT11 || comp->driverType == AGY_DRIVER_DHT22) {
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "°C";
    if (pin >= 0) {
      pinMode(pin, INPUT_PULLUP);
    }
  } else if (comp->driverType == AGY_DRIVER_DS18B20) {
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "°C";
    if (pin >= 0) {
      pinMode(pin, INPUT_PULLUP);
    }
  } else {
    comp->type = agyStringToComponentType(typeStr);
  }

  Serial.printf("[PIN MGR] Konfigurasi %s ('%s', Pin %d, Driver %s)\n",
    id.c_str(), name.c_str(), pin, agyDriverTypeToString(comp->driverType));

  return true;
}

bool AgyGatewayClient::applyPinConfig(const JsonArray& compArray) {
  for (JsonObject item : compArray) {
    configurePin(item);
  }
  sendRegisterManifest();
  sendTelemetry();
  return true;
}

bool AgyGatewayClient::removePin(const String& compId) {
  for (auto it = _components.begin(); it != _components.end(); ++it) {
    if (it->id == compId) {
      Serial.printf("[PIN MGR] Menghapus pin %s\n", compId.c_str());
      _components.erase(it);
      sendRegisterManifest();
      return true;
    }
  }
  return false;
}

void AgyGatewayClient::scanAndReportI2C(int sdaPin, int sclPin) {
  Serial.println("[I2C SCAN] Memulai pemindaian bus I2C...");
  std::vector<AgyI2CDeviceInfo> list = AgyDrivers::scanI2C(sdaPin, sclPin);

  JsonDocument doc;
  doc["event"] = "i2c_scan_result";
  doc["deviceId"] = _deviceId;
  doc["key"] = _deviceKey;

  JsonArray arr = doc["devices"].to<JsonArray>();
  for (size_t i = 0; i < list.size(); i++) {
    const auto& d = list[i];
    JsonObject item = arr.add<JsonObject>();
    item["address"] = d.addressHex;
    item["name"] = d.name;
    item["category"] = d.category;
  }

  String output;
  serializeJson(doc, output);
  _ws.sendTXT(output);
  Serial.printf("[I2C SCAN] Selesai, ditemukan %d perangkat I2C\n", (int)list.size());
}

void AgyGatewayClient::checkDigitalInputs() {
  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent& comp = _components[i];
    if (comp.driverType == AGY_DRIVER_DIGITAL_IN && comp.pin >= 0) {
      int currentVal = digitalRead(comp.pin);
      if (currentVal != comp.lastDigitalVal) {
        comp.lastDigitalVal = currentVal;
        comp.value = (currentVal == (comp.activeLow ? LOW : HIGH)) ? "1" : "0";
        comp.isChanged = true;
        Serial.printf("[DIGITAL IN] %s (Pin %d) berubah -> %s\n", comp.id.c_str(), comp.pin, comp.value.c_str());
        sendTelemetry();
      }
    }
  }
}

void AgyGatewayClient::checkDynamicSensors() {
  unsigned long now = millis();
  bool anyChanged = false;

  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent& comp = _components[i];
    if (!comp.isDynamic || comp.pin < 0) continue;

    if (now - comp.lastReadMs >= comp.readIntervalMs) {
      comp.lastReadMs = now;
      String newVal = comp.value;

      if (comp.driverType == AGY_DRIVER_ANALOG) {
#if defined(ESP8266)
        int raw = analogRead(A0);
#else
        int raw = analogRead(comp.pin);
#endif
        newVal = String(raw);
      } else if (comp.driverType == AGY_DRIVER_DHT11 || comp.driverType == AGY_DRIVER_DHT22) {
        float temp = 0.0f, hum = 0.0f;
        bool ok = AgyDrivers::readDHT(comp.pin, comp.driverType == AGY_DRIVER_DHT22, temp, hum);
        if (ok) {
          newVal = String(temp, 1);
        }
      } else if (comp.driverType == AGY_DRIVER_DS18B20) {
        float temp = 0.0f;
        bool ok = AgyDrivers::readDS18B20(comp.pin, temp);
        if (ok) {
          newVal = String(temp, 1);
        }
      }

      if (newVal != comp.value) {
        comp.value = newVal;
        comp.isChanged = true;
        anyChanged = true;
      }
    }
  }

  if (anyChanged && _wsConnected) {
    sendTelemetry();
  }
}

// -------------------------------------------------------------
// Protokol Pengiriman Manifest & Telemetri
// -------------------------------------------------------------
void AgyGatewayClient::sendRegisterManifest() {
  if (!_wsConnected) return;

  JsonDocument doc;
  doc["event"] = "register";
  doc["deviceId"] = _deviceId;
  doc["key"] = _deviceKey;

  JsonObject info = doc["info"].to<JsonObject>();
#if defined(ESP8266)
  info["chip"] = "ESP8266";
#elif defined(ESP32)
  info["chip"] = "ESP32";
#else
  info["chip"] = "Arduino";
#endif
  info["firmware"] = "1.1.0";
  info["uptime"] = millis() / 1000;
  info["rssi"] = WiFi.RSSI();
  info["dynamicPins"] = _dynamicPinsEnabled;

  JsonArray compArr = doc["components"].to<JsonArray>();
  for (size_t i = 0; i < _components.size(); i++) {
    const AgyComponent& c = _components[i];
    JsonObject item = compArr.add<JsonObject>();
    item["id"] = c.id;
    item["name"] = c.name;
    item["type"] = agyComponentTypeToString(c.type);
    if (c.driverType != AGY_DRIVER_CUSTOM) {
      item["driver"] = agyDriverTypeToString(c.driverType);
    }
    item["access"] = c.access;
    item["unit"] = c.unit;
    item["value"] = c.value;
    if (c.pin >= 0) {
      item["pin"] = c.pin;
    }
    if (c.type == AGY_SWITCH && c.timer.active) {
      JsonObject tmr = item["timer"].to<JsonObject>();
      tmr["active"] = true;
      unsigned long now = millis();
      tmr["remaining"] = (c.timer.endTimeMs > now) ? (c.timer.endTimeMs - now) / 1000 : 0;
      tmr["total"] = c.timer.durationMs / 1000;
    }
  }

  String output;
  serializeJson(doc, output);
  _ws.sendTXT(output);
  Serial.printf("[WS MANIFEST] Terkirim (%d komponen, Dynamic: %s)\n", 
    (int)_components.size(), _dynamicPinsEnabled ? "YES" : "NO");
}

void AgyGatewayClient::sendTelemetry() {
  if (!_wsConnected) return;

  JsonDocument doc;
  doc["event"] = "telemetry";
  doc["deviceId"] = _deviceId;
  doc["key"] = _deviceKey;
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();

  JsonObject data = doc["data"].to<JsonObject>();
  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent& c = _components[i];
    data[c.id] = c.value;
    c.isChanged = false;
  }

  String output;
  serializeJson(doc, output);
  _ws.sendTXT(output);
}
