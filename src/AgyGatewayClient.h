#ifndef AGY_GATEWAY_CLIENT_H
#define AGY_GATEWAY_CLIENT_H

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

class AgyGatewayClient {
public:
  AgyGatewayClient();

  // Inisialisasi dengan URL WebSocket lengkap (e.g. "ws://192.168.1.10:3050/ws" atau "wss://iot.domain.com/ws")
  void begin(const char* ssid, const char* pass, const char* wsHost, uint16_t wsPort, const char* wsPath, const char* deviceId, const char* deviceKey, bool useSsl = false);

  // Overload ringkas
  void begin(const char* ssid, const char* pass, const char* wsUrl, const char* deviceId, const char* deviceKey);

  // Main loop yang harus dipanggil di void loop() mikrokontroler
  void loop();

  // -------------------------------------------------------------
  // Dynamic Pin Management (Zero-Code Web UI Configuration)
  // -------------------------------------------------------------
  void enableDynamicPins(bool enable = true);
  bool isDynamicPinsEnabled() const { return _dynamicPinsEnabled; }
  bool applyPinConfig(const JsonArray& compArray);
  bool configurePin(const JsonObject& compObj);
  bool removePin(const String& compId);
  void scanAndReportI2C(int sdaPin = -1, int sclPin = -1);

  // -------------------------------------------------------------
  // Registrasi Komponen & Sensor Modular (C++ Manual)
  // -------------------------------------------------------------
  // Tambah Saklar Fisik (Relay)
  AgyComponent* addSwitch(const String& id, int pin, const String& name = "", bool activeLow = true, bool initialState = false);

  // Tambah Sensor dengan Fungsi Pembacaan Otomatis (misal: DHT, Analog, dll)
  AgyComponent* addSensor(const String& id, const String& name, const String& unit, std::function<float()> readFn, unsigned long intervalMs = 5000);

  // Tambah Komponen Custom / Generic
  AgyComponent* addComponent(const AgyComponent& comp);

  // Update nilai sensor secara manual dari sketch
  void updateSensor(const String& id, float value);
  void updateSensor(const String& id, const String& value);

  // Update status switch secara manual (misal dari tombol fisik)
  void setSwitchState(const String& id, bool state);

  // -------------------------------------------------------------
  // Virtual Pins (Blynk style)
  // -------------------------------------------------------------
  void virtualWrite(const String& vPin, const String& value);
  void virtualWrite(const String& vPin, int value);
  void virtualWrite(const String& vPin, float value);
  void onVirtualWrite(const String& vPin, AgyVirtualWriteCallback callback);

  // Callback listener kustom saat ada perintah kontrol dari server
  void onCommand(AgyCommandCallback callback);
  void onConnection(AgyConnectionCallback callback);

  // Kirim manifest registrasi ke server
  void sendRegisterManifest();

  // Kirim data telemetri ke server
  void sendTelemetry();

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

  AgyComponent* findComponent(const String& id);
};

#endif // AGY_GATEWAY_CLIENT_H
