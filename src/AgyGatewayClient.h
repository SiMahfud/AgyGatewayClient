#ifndef AGY_GATEWAY_CLIENT_H
#define AGY_GATEWAY_CLIENT_H

#define AGY_GATEWAY_CLIENT_VERSION "1.1.0"

#include <Arduino.h>
#include <vector>
#include <map>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include "AgyTypes.h"
#include "AgyOTA.h"
#include "AgyDrivers.h"
#include "AgyStorage.h"
#include "AgyPortal.h"

class AgyGatewayClient {
public:
  AgyGatewayClient();

  // Inisialisasi dengan parameter lengkap
  void begin(const char* ssid, const char* pass, const char* wsHost, uint16_t wsPort, const char* wsPath, const char* deviceId, const char* deviceKey, bool useSsl = false);

  // Overload ringkas
  void begin(const char* ssid, const char* pass, const char* wsUrl, const char* deviceId, const char* deviceKey);

  // -------------------------------------------------------------
  // Zero-Code Auto-Connect & WiFi Captive Portal
  // -------------------------------------------------------------
  // Otomatis membaca kredensial dari Flash. Jika belum ada atau gagal konek,
  // menyalakan AP Captive Portal untuk konfigurasi via HP/Laptop.
  bool autoConnect(const char* apName = nullptr, uint32_t timeoutSec = 60);

  // Paksa membuka Captive Portal (misal dipicu tombol reset)
  void startPortal(const char* apName = nullptr);

  // Main loop yang harus dipanggil di void loop() mikrokontroler
  void loop();

  // -------------------------------------------------------------
  // Status LED Feedback
  // -------------------------------------------------------------
  void enableStatusLed(int pin = LED_BUILTIN, bool activeLow = true);
  void disableStatusLed();

  // -------------------------------------------------------------
  // Dynamic Pin Management (Zero-Code Web UI Configuration)
  // -------------------------------------------------------------
  void enableDynamicPins(bool enable = true);
  bool isDynamicPinsEnabled() const { return _dynamicPinsEnabled; }
  bool applyPinConfig(const JsonArray& compArray);
  bool configurePin(const JsonObject& compObj);
  bool removePin(const String& compId);
  void scanAndReportI2C(int sdaPin = -1, int sclPin = -1);

  // Flash persistence helpers
  void savePinConfigToStorage();
  void loadPinConfigFromStorage();
  void clearPinStorage();
  void clearAllStorage();

  // -------------------------------------------------------------
  // Registrasi Komponen & Sensor Modular (C++ Manual)
  // -------------------------------------------------------------
  AgyComponent* addSwitch(const String& id, int pin, const String& name = "", bool activeLow = true, bool initialState = false);
  AgyComponent* addSensor(const String& id, const String& name, const String& unit, std::function<float()> readFn, unsigned long intervalMs = 5000);
  AgyComponent* addComponent(const AgyComponent& comp);

  void updateSensor(const String& id, float value);
  void updateSensor(const String& id, const String& value);
  void setSwitchState(const String& id, bool state);

  // -------------------------------------------------------------
  // Virtual Pins (Blynk style)
  // -------------------------------------------------------------
  void virtualWrite(const String& vPin, const String& value);
  void virtualWrite(const String& vPin, int value);
  void virtualWrite(const String& vPin, float value);
  void onVirtualWrite(const String& vPin, AgyVirtualWriteCallback callback);

  // Callback listener kustom
  void onCommand(AgyCommandCallback callback);
  void onConnection(AgyConnectionCallback callback);

  // Kirim manifest registrasi ke server
  void sendRegisterManifest();

  // Kirim data telemetri ke server
  // forceAll = false: Delta Telemetry (hanya kirim komponen yang isChanged == true)
  // forceAll = true: Full Sync Telemetry (kirim seluruh komponen)
  void sendTelemetry(bool forceAll = false);
  void sendFullTelemetry() { sendTelemetry(true); }

  // Getter status
  bool isConnected() const { return _wsConnected; }
  String getDeviceId() const { return _deviceId; }

private:
  String _ssid;
  String _pass;
  String _wsHost;
  uint16_t _wsPort = 3050;
  String _wsPath = "/ws";
  bool _useSsl = false;
  String _deviceId;
  String _deviceKey;
  bool _dynamicPinsEnabled = false;

  WebSocketsClient _ws;
  bool _wsConnected = false;
  unsigned long _lastHeartbeat = 0;
  unsigned long _heartbeatInterval = 30000;

  // WiFi Reconnection non-blocking
  unsigned long _lastWifiCheck = 0;
  unsigned long _wifiCheckInterval = 10000;

  // Status LED
  int _statusLedPin = -1;
  bool _statusLedActiveLow = true;
  unsigned long _lastLedBlink = 0;
  bool _ledCurrentState = false;

  // Captive Portal
  AgyPortal _portal;
  bool _portalActive = false;

  // Registry komponen
  std::vector<AgyComponent> _components;
  std::map<String, AgyVirtualWriteCallback> _virtualCallbacks;
  AgyCommandCallback _commandCallback = nullptr;
  AgyConnectionCallback _connCallback = nullptr;

  void setupWiFi();
  void initWebSocket();
  void handleWsEvent(WStype_t type, uint8_t* payload, size_t length);
  void processIncomingJson(const String& jsonStr);
  void applySwitchState(AgyComponent& comp, bool state);
  void checkSensorIntervals();
  void checkDynamicSensors();
  void checkDigitalInputs();
  void checkCountdownTimers();
  void checkWiFiConnection();
  void updateStatusLed();

  AgyComponent* findComponent(const String& id);
};

#endif // AGY_GATEWAY_CLIENT_H
