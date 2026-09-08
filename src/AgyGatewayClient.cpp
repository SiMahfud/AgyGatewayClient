#include "AgyGatewayClient.h"

AgyGatewayClient::AgyGatewayClient() {
  _components.reserve(16);
}

AgyGatewayClient::~AgyGatewayClient() {
  for (AgyComponent* comp : _components) {
    if (comp) delete comp;
  }
  _components.clear();
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

  // Parse URL jika ada format "ws://" atau "wss://"
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

bool AgyGatewayClient::autoConnect(const char* apName, const char* apPass, uint32_t timeoutSec) {
  enableStatusLed();

  String ssid, pass, host, path, devId, devKey;
  uint16_t port = 3050;
  bool useSsl = false;

  bool hasConfig = AgyStorage::loadNetworkConfig(ssid, pass, host, port, path, devId, devKey, useSsl);
  if (hasConfig) {
    Serial.printf("[AUTOCONNECT] Konfigurasi Flash ditemukan: SSID=%s, Host=%s:%d (SSL: %s)\n", 
      ssid.c_str(), host.c_str(), port, useSsl ? "YES" : "NO");
    begin(ssid.c_str(), pass.c_str(), host.c_str(), port, path.c_str(), devId.c_str(), devKey.c_str(), useSsl);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < timeoutSec * 1000)) {
      feedWatchdog();
      updateStatusLed();
      delay(100);
      yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[AUTOCONNECT] Berhasil tersambung ke WiFi!");
      return true;
    }
    Serial.println("[AUTOCONNECT] Gagal menyambung ke WiFi tersimpan dalam batas waktu.");
  } else {
    Serial.println("[AUTOCONNECT] Belum ada konfigurasi jaringan di Flash.");
  }

  // Nyalakan Captive Portal (dengan opsi WPA2 password)
  startPortal(apName, apPass);
  return false;
}

void AgyGatewayClient::startPortal(const char* apName, const char* apPass) {
  _portalActive = true;
  _portal.start(apName, apPass);
}

void AgyGatewayClient::enableWatchdog(uint32_t timeoutSec) {
  _wdtTimeoutSec = timeoutSec;
  _wdtEnabled = true;

#if defined(ESP8266)
  ESP.wdtEnable(timeoutSec * 1000);
#elif defined(ESP32)
  #if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t config = {
      .timeout_ms = timeoutSec * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&config);
  #else
    esp_task_wdt_init(timeoutSec, true);
    esp_task_wdt_add(NULL);
  #endif
#endif

  Serial.printf("[WDT] Hardware Watchdog diaktifkan (%u detik)\n", (unsigned)timeoutSec);
}

void AgyGatewayClient::disableWatchdog() {
  _wdtEnabled = false;
#if defined(ESP8266)
  ESP.wdtDisable();
#elif defined(ESP32)
  #if !defined(ESP_IDF_VERSION_MAJOR) || ESP_IDF_VERSION_MAJOR < 5
    esp_task_wdt_delete(NULL);
  #endif
#endif
  Serial.println("[WDT] Hardware Watchdog dinonaktifkan.");
}

void AgyGatewayClient::feedWatchdog() {
  if (!_wdtEnabled) return;
#if defined(ESP8266)
  ESP.wdtFeed();
#elif defined(ESP32)
  esp_task_wdt_reset();
#endif
}

void AgyGatewayClient::enableStatusLed(int pin, bool activeLow) {
  _statusLedPin = pin;
  _statusLedActiveLow = activeLow;
  pinMode(_statusLedPin, OUTPUT);
  digitalWrite(_statusLedPin, _statusLedActiveLow ? HIGH : LOW); // Matikan awal
}

void AgyGatewayClient::disableStatusLed() {
  if (_statusLedPin >= 0) {
    digitalWrite(_statusLedPin, _statusLedActiveLow ? HIGH : LOW);
    _statusLedPin = -1;
  }
}

void AgyGatewayClient::updateStatusLed() {
  if (_statusLedPin < 0) return;

  unsigned long now = millis();
  unsigned long interval = 1000;

  if (_portalActive) {
    interval = 100; // Kedip sangat cepat: mode AP Captive Portal aktif
  } else if (WiFi.status() != WL_CONNECTED) {
    interval = 250; // Kedip cepat: mencari/menghubungkan WiFi
  } else if (!_wsConnected) {
    interval = 600; // Kedip lambat: WiFi tersambung, mencari Gateway Server
  } else {
    // Siap & terhubung: LED mati (agar tidak silau)
    digitalWrite(_statusLedPin, _statusLedActiveLow ? HIGH : LOW);
    return;
  }

  if (now - _lastLedBlink >= interval) {
    _lastLedBlink = now;
    _ledCurrentState = !_ledCurrentState;
    digitalWrite(_statusLedPin, _statusLedActiveLow ? (_ledCurrentState ? LOW : HIGH) : (_ledCurrentState ? HIGH : LOW));
  }
}

void AgyGatewayClient::setupWiFi() {
  Serial.printf("[WIFI] Menyambungkan ke: %s\n", _ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(_ssid.c_str(), _pass.c_str());
}

void AgyGatewayClient::checkWiFiConnection() {
  if (_portalActive) return;

  unsigned long now = millis();
  if (now - _lastWifiCheck >= _wifiCheckInterval) {
    _lastWifiCheck = now;
    if (_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
      Serial.printf("[WIFI] Memeriksa koneksi WiFi... Reconnecting ke %s\n", _ssid.c_str());
      WiFi.reconnect();
    }
  }
}

void AgyGatewayClient::initWebSocket() {
  Serial.printf("[WS INIT] Target Server: %s:%d%s (SSL: %s)\n", 
    _wsHost.c_str(), _wsPort, _wsPath.c_str(), _useSsl ? "YES" : "NO");

  if (_useSsl) {
    _ws.beginSSL(_wsHost.c_str(), _wsPort, _wsPath.c_str());
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
  // Feed hardware watchdog agar sistem tidak reboot saat loop normal
  feedWatchdog();

  // 0. Update Pola Kedip Status LED
  updateStatusLed();

  // 0b. Handler Captive Portal jika aktif
  if (_portalActive) {
    _portal.loop();
    if (_portal.isSaved()) {
      Serial.println("[PORTAL] Konfigurasi baru tersimpan! Me-restart modul dalam 1 detik...");
      delay(1000);
      ESP.restart();
    }
    return;
  }

  // 1. Jaga koneksi WiFi secara non-blocking
  checkWiFiConnection();

  // 1b. Polling WebSocket
  if (WiFi.status() == WL_CONNECTED) {
    _ws.loop();
  }

  // 1c. Kirim pending telemetry jika tertunda karena rate limiting
  if (_telemetryPending && (millis() - _lastTelemetrySend >= _minTelemetryInterval)) {
    sendTelemetry();
  }

  // 2. Cek pembacaan sensor terjadwal (fungsi C++)
  checkSensorIntervals();

  // 2b. Cek sensor dinamis & input digital jika Dynamic Pins aktif
  if (_dynamicPinsEnabled) {
    checkDigitalInputs();
    checkDynamicSensors();
  }

  // 3. Cek timer countdown pada switch (overflow-safe)
  checkCountdownTimers();

  // 4. Heartbeat berkala (Full Telemetry Sync setiap 30 detik)
  if (_wsConnected && (millis() - _lastHeartbeat > _heartbeatInterval)) {
    _lastHeartbeat = millis();
    sendFullTelemetry();
  }
}

// -------------------------------------------------------------
// Registrasi & Manajemen Komponen
// -------------------------------------------------------------
AgyComponent* AgyGatewayClient::addSwitch(const String& id, int pin, const String& name, bool activeLow, bool initialState) {
  AgyComponent* comp = findComponent(id);
  if (!comp) {
    comp = new AgyComponent();
    _components.push_back(comp);
  }
  comp->id = id;
  comp->name = name.length() > 0 ? name : id;
  comp->type = AGY_SWITCH;
  comp->driverType = AGY_DRIVER_SWITCH;
  comp->access = "rw";
  comp->unit = "";
  comp->pin = pin;
  comp->activeLow = activeLow;
  comp->value = initialState ? "true" : "false";

  if (pin >= 0) {
    pinMode(pin, OUTPUT);
    applySwitchState(*comp, initialState);
  }

  return comp;
}

AgyComponent* AgyGatewayClient::addDimmer(const String& id, int pin, const String& name, const String& unit, int initialValue) {
  AgyComponent* comp = findComponent(id);
  if (!comp) {
    comp = new AgyComponent();
    _components.push_back(comp);
  }
  comp->id = id;
  comp->name = name.length() > 0 ? name : id;
  comp->type = AGY_DIMMER;
  comp->driverType = AGY_DRIVER_DIMMER;
  comp->access = "rw";
  comp->unit = unit.length() > 0 ? unit : "%";
  comp->pin = pin;
  comp->value = String(initialValue);

  if (pin >= 0) {
    applyDimmerValue(*comp, initialValue);
  }

  return comp;
}

AgyComponent* AgyGatewayClient::addSensor(const String& id, const String& name, const String& unit, std::function<float()> readFn, unsigned long intervalMs) {
  AgyComponent* comp = findComponent(id);
  if (!comp) {
    comp = new AgyComponent();
    _components.push_back(comp);
  }
  comp->id = id;
  comp->name = name.length() > 0 ? name : id;
  comp->type = AGY_SENSOR;
  comp->driverType = AGY_DRIVER_CUSTOM;
  comp->access = "r";
  comp->unit = unit;
  comp->value = "0";
  comp->readSensorFn = readFn;
  comp->readIntervalMs = intervalMs;
  comp->lastReadMs = millis();

  if (readFn) {
    float val = readFn();
    comp->value = String(val, 2);
  }

  return comp;
}

AgyComponent* AgyGatewayClient::addComponent(const AgyComponent& comp) {
  AgyComponent* existing = findComponent(comp.id);
  if (existing) {
    *existing = comp;
    return existing;
  }
  AgyComponent* newComp = new AgyComponent(comp);
  _components.push_back(newComp);
  return newComp;
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
        sendTelemetry(); // Delta Telemetry
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
      sendTelemetry(); // Delta Telemetry
    }
  }
}

void AgyGatewayClient::setDimmerValue(const String& id, int value) {
  AgyComponent* comp = findComponent(id);
  if (comp && comp->type == AGY_DIMMER) {
    applyDimmerValue(*comp, value);
    comp->value = String(value);
    comp->isChanged = true;
    if (_wsConnected) {
      sendTelemetry(); // Delta Telemetry
    }
  }
}

void AgyGatewayClient::applyDimmerValue(AgyComponent& comp, int value) {
  if (comp.pin >= 0) {
    pinMode(comp.pin, OUTPUT);
    int pwm = value;
    if (pwm < 0) pwm = 0;
    if (comp.unit == "%") {
      if (pwm > 100) pwm = 100;
      pwm = (pwm * 255) / 100;
    } else {
      if (pwm > 255) pwm = 255;
    }
    analogWrite(comp.pin, pwm);
    Serial.printf("[DIMMER] %s (Pin %d) -> %d (PWM: %d)\n", comp.id.c_str(), comp.pin, value, pwm);
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
    if (_components[i] && _components[i]->id == id) {
      return _components[i];
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
  const char* evt = doc["event"] | "";

  // 0. Handshake Autentikasi / Token Sesi
  if (strcmp(action, "auth_ok") == 0 || strcmp(evt, "auth_ok") == 0 || strcmp(action, "authenticated") == 0) {
    _authToken = doc["token"] | doc["authToken"] | "";
    Serial.printf("[AUTH] Autentikasi berhasil dikonfirmasi! Token sesi: %s\n", 
      _authToken.length() > 0 ? (_authToken.substring(0, 4) + "****").c_str() : "ACTIVE");
    return;
  }

  // 1. Perintah Kontrol Komponen Universal (Switch, Dimmer/PWM, Custom)
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

        if (duration > 0 && state) {
          comp->timer.active = true;
          comp->timer.startMs = millis();
          comp->timer.durationMs = duration * 1000;
          comp->timer.endTimeMs = millis() + (duration * 1000);
          comp->timer.targetState = false;
          Serial.printf("[TIMER] Timer aktif untuk %s selama %lu detik\n", compId.c_str(), duration);
        } else {
          comp->timer.active = false;
        }
      } else if (comp->type == AGY_DIMMER) {
        int dimVal = valStr.toInt();
        applyDimmerValue(*comp, dimVal);
        comp->value = String(dimVal);
      } else {
        comp->value = valStr;
      }

      comp->isChanged = true;
      sendTelemetry(); // Delta Telemetry

      if (_commandCallback) {
        _commandCallback(compId, valStr);
      }
    }
    return;
  }

  // 2. Backwards-compatibility: Format 'set_relay'
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

  // 4. Perintah OTA Update (Dengan Progress Feedback & Feed Watchdog)
  if (strcmp(action, "ota_update") == 0) {
    String binUrl = doc["url"] | "";
    if (binUrl.length() > 0) {
      Serial.printf("[OTA] Menerima instruksi OTA Update: %s\n", binUrl.c_str());
      AgyOTA::setProgressCallback([this](size_t cur, size_t total, int percent) {
        feedWatchdog();
        static int lastSentPct = -1;
        if (percent != lastSentPct && (percent % 10 == 0 || percent == 100)) {
          lastSentPct = percent;
          if (_wsConnected) {
            JsonDocument pDoc;
            pDoc["event"] = "ota_progress";
            pDoc["deviceId"] = _deviceId;
            pDoc["percent"] = percent;
            pDoc["current"] = cur;
            pDoc["total"] = total;
            String pStr;
            serializeJson(pDoc, pStr);
            _ws.sendTXT(pStr);
          }
        }
      });
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
    savePinConfigToStorage();
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
    AgyComponent* comp = _components[i];
    if (!comp) continue;
    if (comp->type == AGY_SENSOR && comp->readSensorFn) {
      if (now - comp->lastReadMs >= comp->readIntervalMs) {
        comp->lastReadMs = now;
        float val = comp->readSensorFn();
        String newVal = String(val, 2);
        if (newVal != comp->value) {
          comp->value = newVal;
          comp->isChanged = true;
          anyChanged = true;
        }
      }
    }
  }

  if (anyChanged && _wsConnected) {
    sendTelemetry(); // Delta Telemetry
  }
}

void AgyGatewayClient::checkCountdownTimers() {
  unsigned long now = millis();
  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent* comp = _components[i];
    if (!comp) continue;
    if (comp->type == AGY_SWITCH && comp->timer.active) {
      // Perhitungan aman dari overflow millis()
      if ((long)(now - comp->timer.endTimeMs) >= 0) {
        Serial.printf("[TIMER] Selesai untuk %s! Mematikan switch...\n", comp->id.c_str());
        comp->timer.active = false;
        applySwitchState(*comp, comp->timer.targetState);
        comp->value = comp->timer.targetState ? "true" : "false";
        comp->isChanged = true;
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
  if (enable) {
    loadPinConfigFromStorage();
  }
}

void AgyGatewayClient::loadPinConfigFromStorage() {
  JsonDocument doc;
  if (AgyStorage::loadPinConfig(doc)) {
    JsonArray arr = doc.as<JsonArray>();
    Serial.printf("[STORAGE] Memulihkan %d pin dari Flash...\n", (int)arr.size());
    for (JsonObject item : arr) {
      configurePin(item);
    }
  }
}

void AgyGatewayClient::savePinConfigToStorage() {
  AgyStorage::savePinConfig(_components);
}

void AgyGatewayClient::clearPinStorage() {
  AgyStorage::clearPinConfig();
}

void AgyGatewayClient::clearAllStorage() {
  AgyStorage::clearAll();
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
  uint8_t i2cAddr = doc["i2cAddr"] | 0;

  AgyComponent* comp = findComponent(id);
  if (!comp) {
    comp = new AgyComponent();
    _components.push_back(comp);
  }

  comp->id = id;
  comp->name = name;
  comp->pin = pin;
  comp->activeLow = activeLow;
  comp->pullup = pullup;
  comp->unit = unit;
  comp->readIntervalMs = interval;
  comp->isDynamic = true;
  comp->i2cAddress = i2cAddr;
  comp->driverType = agyStringToDriverType(driverStr);

  if (comp->driverType == AGY_DRIVER_SWITCH) {
    comp->type = AGY_SWITCH;
    comp->access = "rw";
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      bool curState = (comp->value == "true" || comp->value == "1");
      applySwitchState(*comp, curState);
    }
  } else if (comp->driverType == AGY_DRIVER_DIMMER) {
    comp->type = AGY_DIMMER;
    comp->access = "rw";
    if (unit.length() == 0) comp->unit = "%";
    if (pin >= 0) {
      int initialPwm = comp->value.toInt();
      applyDimmerValue(*comp, initialPwm);
    }
  } else if (comp->driverType == AGY_DRIVER_DIGITAL_IN) {
    comp->type = AGY_INDICATOR;
    comp->access = "r";
    if (pin >= 0) {
      pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
      comp->lastDigitalVal = digitalRead(pin);
      comp->debouncedVal = comp->lastDigitalVal;
      comp->value = (comp->debouncedVal == (activeLow ? LOW : HIGH)) ? "1" : "0";
    }
  } else if (comp->driverType == AGY_DRIVER_ANALOG) {
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "ADC";
  } else if (comp->driverType == AGY_DRIVER_DHT11 || comp->driverType == AGY_DRIVER_DHT22) {
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "°C";
    if (pin >= 0) {
      pinMode(pin, INPUT_PULLUP);
    }
    // Auto companion sensor untuk kelembapan
    String humId = id + "_hum";
    AgyComponent* humComp = findComponent(humId);
    if (!humComp) {
      humComp = new AgyComponent();
      _components.push_back(humComp);
    }
    humComp->id = humId;
    humComp->name = name + " Kelembapan";
    humComp->type = AGY_SENSOR;
    humComp->driverType = AGY_DRIVER_CUSTOM;
    humComp->access = "r";
    humComp->unit = "%";
    humComp->pin = pin;
    humComp->isDynamic = true;
    humComp->readIntervalMs = interval;
  } else if (comp->driverType == AGY_DRIVER_DS18B20) {
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "°C";
    if (pin >= 0) {
      pinMode(pin, INPUT_PULLUP);
    }
  } else if (comp->driverType == AGY_DRIVER_BH1750) {
    AgyDrivers::initI2C();
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "Lux";
  } else if (comp->driverType == AGY_DRIVER_SHT30 || comp->driverType == AGY_DRIVER_AHT10) {
    AgyDrivers::initI2C();
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "°C";
    // Auto companion sensor untuk kelembapan
    String humId = id + "_hum";
    AgyComponent* humComp = findComponent(humId);
    if (!humComp) {
      humComp = new AgyComponent();
      _components.push_back(humComp);
    }
    humComp->id = humId;
    humComp->name = name + " Kelembapan";
    humComp->type = AGY_SENSOR;
    humComp->driverType = AGY_DRIVER_CUSTOM;
    humComp->access = "r";
    humComp->unit = "%";
    humComp->i2cAddress = i2cAddr;
    humComp->isDynamic = true;
    humComp->readIntervalMs = interval;
  } else if (comp->driverType == AGY_DRIVER_BMP280) {
    AgyDrivers::initI2C();
    comp->type = AGY_SENSOR;
    comp->access = "r";
    if (unit.length() == 0) comp->unit = "°C";
    // Auto companion sensor untuk tekanan udara
    String pressId = id + "_press";
    AgyComponent* pressComp = findComponent(pressId);
    if (!pressComp) {
      pressComp = new AgyComponent();
      _components.push_back(pressComp);
    }
    pressComp->id = pressId;
    pressComp->name = name + " Tekanan";
    pressComp->type = AGY_SENSOR;
    pressComp->driverType = AGY_DRIVER_CUSTOM;
    pressComp->access = "r";
    pressComp->unit = "hPa";
    pressComp->i2cAddress = i2cAddr;
    pressComp->isDynamic = true;
    pressComp->readIntervalMs = interval;
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
  savePinConfigToStorage();
  sendRegisterManifest();
  sendFullTelemetry();
  return true;
}

bool AgyGatewayClient::removePin(const String& compId) {
  bool found = false;
  String humId = compId + "_hum";
  String pressId = compId + "_press";

  for (auto it = _components.begin(); it != _components.end(); ) {
    if ((*it) && ((*it)->id == compId || (*it)->id == humId || (*it)->id == pressId)) {
      Serial.printf("[PIN MGR] Menghapus pin %s\n", (*it)->id.c_str());
      delete *it;
      it = _components.erase(it);
      found = true;
    } else {
      ++it;
    }
  }

  if (found) {
    savePinConfigToStorage();
    sendRegisterManifest();
  }
  return found;
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
  unsigned long now = millis();
  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent* comp = _components[i];
    if (!comp) continue;
    if (comp->driverType == AGY_DRIVER_DIGITAL_IN && comp->pin >= 0) {
      int reading = digitalRead(comp->pin);

      // Filter Debounce Software
      if (reading != comp->lastDigitalVal) {
        comp->lastDigitalVal = reading;
        comp->lastDebounceTime = now;
      }

      if ((now - comp->lastDebounceTime) >= comp->debounceDelay) {
        if (reading != comp->debouncedVal) {
          comp->debouncedVal = reading;
          comp->value = (reading == (comp->activeLow ? LOW : HIGH)) ? "1" : "0";
          comp->isChanged = true;
          Serial.printf("[DIGITAL IN] %s (Pin %d) stabil -> %s\n", comp->id.c_str(), comp->pin, comp->value.c_str());
          sendTelemetry(); // Delta Telemetry otomatis dengan rate limiting
        }
      }
    }
  }
}

void AgyGatewayClient::checkDynamicSensors() {
  unsigned long now = millis();
  bool anyChanged = false;

  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent* comp = _components[i];
    if (!comp || !comp->isDynamic) continue;

    if (now - comp->lastReadMs >= comp->readIntervalMs) {
      comp->lastReadMs = now;
      String newVal = comp->value;

      if (comp->driverType == AGY_DRIVER_ANALOG) {
#if defined(ESP8266)
        int raw = analogRead(A0);
        newVal = String(raw);
#elif defined(ESP32)
        if (comp->pin >= 0) {
          // Peringatan ADC2 pada ESP32 saat WiFi aktif
          if ((comp->pin >= 0 && comp->pin <= 4) || (comp->pin >= 12 && comp->pin <= 15) || (comp->pin >= 25 && comp->pin <= 27)) {
            Serial.printf("[ADC WARN] Pin GPIO %d adalah ADC2, dapat konflik dengan WiFi ESP32!\n", comp->pin);
          }
          int raw = analogRead(comp->pin);
          newVal = String(raw);
        }
#endif
      } else if (comp->driverType == AGY_DRIVER_DHT11 || comp->driverType == AGY_DRIVER_DHT22) {
        float temp = 0.0f, hum = 0.0f;
        if (comp->pin >= 0 && AgyDrivers::readDHT(comp->pin, comp->driverType == AGY_DRIVER_DHT22, temp, hum)) {
          newVal = String(temp, 1);
          // Update companion sensor kelembapan
          AgyComponent* humComp = findComponent(comp->id + "_hum");
          if (humComp) {
            String humVal = String(hum, 1);
            if (humVal != humComp->value) {
              humComp->value = humVal;
              humComp->isChanged = true;
              anyChanged = true;
            }
          }
        }
      } else if (comp->driverType == AGY_DRIVER_DS18B20) {
        float temp = 0.0f;
        if (comp->pin >= 0 && AgyDrivers::readDS18B20(comp->pin, temp)) {
          newVal = String(temp, 1);
        }
      } else if (comp->driverType == AGY_DRIVER_BH1750) {
        float lux = 0.0f;
        uint8_t addr = (comp->i2cAddress > 0) ? comp->i2cAddress : 0x23;
        if (AgyDrivers::readBH1750(lux, addr)) {
          newVal = String(lux, 1);
        }
      } else if (comp->driverType == AGY_DRIVER_SHT30) {
        float temp = 0.0f, hum = 0.0f;
        uint8_t addr = (comp->i2cAddress > 0) ? comp->i2cAddress : 0x44;
        if (AgyDrivers::readSHT3x(temp, hum, addr)) {
          newVal = String(temp, 1);
          // Update companion sensor kelembapan
          AgyComponent* humComp = findComponent(comp->id + "_hum");
          if (humComp) {
            String humVal = String(hum, 1);
            if (humVal != humComp->value) {
              humComp->value = humVal;
              humComp->isChanged = true;
              anyChanged = true;
            }
          }
        }
      } else if (comp->driverType == AGY_DRIVER_AHT10) {
        float temp = 0.0f, hum = 0.0f;
        uint8_t addr = (comp->i2cAddress > 0) ? comp->i2cAddress : 0x38;
        if (AgyDrivers::readAHTx(temp, hum, addr)) {
          newVal = String(temp, 1);
          // Update companion sensor kelembapan
          AgyComponent* humComp = findComponent(comp->id + "_hum");
          if (humComp) {
            String humVal = String(hum, 1);
            if (humVal != humComp->value) {
              humComp->value = humVal;
              humComp->isChanged = true;
              anyChanged = true;
            }
          }
        }
      } else if (comp->driverType == AGY_DRIVER_BMP280) {
        float temp = 0.0f, press = 0.0f;
        uint8_t addr = (comp->i2cAddress > 0) ? comp->i2cAddress : 0x76;
        if (AgyDrivers::readBMP280(temp, press, addr)) {
          newVal = String(temp, 1);
          // Update companion sensor tekanan udara
          AgyComponent* pressComp = findComponent(comp->id + "_press");
          if (pressComp) {
            String pressVal = String(press, 1);
            if (pressVal != pressComp->value) {
              pressComp->value = pressVal;
              pressComp->isChanged = true;
              anyChanged = true;
            }
          }
        }
      }

      if (newVal != comp->value) {
        comp->value = newVal;
        comp->isChanged = true;
        anyChanged = true;
      }
    }
  }

  if (anyChanged && _wsConnected) {
    sendTelemetry(); // Delta Telemetry
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
  if (_authToken.length() > 0) {
    doc["token"] = _authToken;
  }

  JsonObject info = doc["info"].to<JsonObject>();
#if defined(ESP8266)
  info["chip"] = "ESP8266";
#elif defined(ESP32)
  info["chip"] = "ESP32";
#else
  info["chip"] = "Arduino";
#endif
  info["firmware"] = AGY_GATEWAY_CLIENT_VERSION;
  info["uptime"] = millis() / 1000;
  info["rssi"] = WiFi.RSSI();
  info["dynamicPins"] = _dynamicPinsEnabled;

  JsonArray compArr = doc["components"].to<JsonArray>();
  for (size_t i = 0; i < _components.size(); i++) {
    if (!_components[i]) continue;
    const AgyComponent& c = *_components[i];
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
    if (c.i2cAddress > 0) {
      item["i2cAddr"] = c.i2cAddress;
    }
    if (c.type == AGY_SWITCH && c.timer.active) {
      JsonObject tmr = item["timer"].to<JsonObject>();
      tmr["active"] = true;
      unsigned long now = millis();
      long diff = (long)(c.timer.endTimeMs - now);
      tmr["remaining"] = (diff > 0) ? (uint32_t)(diff / 1000) : 0;
      tmr["total"] = c.timer.durationMs / 1000;
    }
  }

  String output;
  serializeJson(doc, output);
  _ws.sendTXT(output);
  Serial.printf("[WS MANIFEST] Terkirim (%d komponen, Dynamic: %s, FW: %s)\n", 
    (int)_components.size(), _dynamicPinsEnabled ? "YES" : "NO", AGY_GATEWAY_CLIENT_VERSION);
}

void AgyGatewayClient::sendTelemetry(bool forceAll) {
  if (!_wsConnected) return;

  unsigned long now = millis();
  // Rate limiting delta telemetry agar tidak membanjiri buffer socket
  if (!forceAll && (now - _lastTelemetrySend < _minTelemetryInterval)) {
    _telemetryPending = true;
    return;
  }
  _telemetryPending = false;

  JsonDocument doc;
  JsonObject data = doc["data"].to<JsonObject>();
  size_t changedCount = 0;

  for (size_t i = 0; i < _components.size(); i++) {
    AgyComponent* c = _components[i];
    if (!c) continue;
    if (forceAll || c->isChanged) {
      data[c->id] = c->value;
      c->isChanged = false;
      changedCount++;
    }
  }

  // Jika Delta Telemetry dan tidak ada komponen yang berubah, batalkan transmisi
  if (!forceAll && changedCount == 0) {
    return;
  }

  doc["event"] = "telemetry";
  doc["deviceId"] = _deviceId;
  if (_authToken.length() > 0) {
    doc["token"] = _authToken;
  } else if (_sendKeyOnTelemetry) {
    doc["key"] = _deviceKey;
  }
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  if (!forceAll) {
    doc["delta"] = true;
  }

  String output;
  serializeJson(doc, output);
  _ws.sendTXT(output);
  _lastTelemetrySend = millis();
}
