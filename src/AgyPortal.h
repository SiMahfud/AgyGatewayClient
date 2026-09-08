#ifndef AGY_PORTAL_H
#define AGY_PORTAL_H

#include <Arduino.h>
#include <DNSServer.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer AgyWebServer;
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  typedef WebServer AgyWebServer;
#endif

#include "AgyStorage.h"

class AgyPortal {
public:
  AgyPortal() : _server(80) {}

  void start(const char* customApName = nullptr) {
    String apName;
    if (customApName && strlen(customApName) > 0) {
      apName = customApName;
    } else {
#if defined(ESP8266)
      apName = "AGY-NODE-" + String(ESP.getChipId(), HEX);
#elif defined(ESP32)
      uint32_t chipId = 0;
      for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
      apName = "AGY-NODE-" + String(chipId, HEX);
#else
      apName = "AGY-NODE-SETUP";
#endif
      apName.toUpperCase();
    }

    Serial.printf("\n[PORTAL] Mengaktifkan AP: %s (IP: 192.168.4.1)\n", apName.c_str());

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName.c_str());

    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    setupRoutes();
    _server.begin();
    _active = true;
    _saved = false;
    Serial.println("[PORTAL] Web Server & DNS Captive Portal aktif di port 80!");
  }

  void loop() {
    if (!_active) return;
    _dnsServer.processNextRequest();
    _server.handleClient();
  }

  bool isSaved() const { return _saved; }
  bool isActive() const { return _active; }

  void stop() {
    if (_active) {
      _server.stop();
      _dnsServer.stop();
      WiFi.softAPdisconnect(true);
      _active = false;
      Serial.println("[PORTAL] Captive Portal dihentikan.");
    }
  }

private:
  AgyWebServer _server;
  DNSServer _dnsServer;
  bool _active = false;
  bool _saved = false;

  void setupRoutes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });

    // Captive Portal Redirects untuk Android, iOS, Windows
    _server.on("/generate_204", HTTP_GET, [this]() { handleRedirect(); });
    _server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRedirect(); });
    _server.on("/connecttest.txt", HTTP_GET, [this]() { handleRedirect(); });
    _server.on("/ncsi.txt", HTTP_GET, [this]() { handleRedirect(); });

    _server.onNotFound([this]() { handleRedirect(); });
  }

  void handleRedirect() {
    _server.sendHeader("Location", "http://192.168.4.1/", true);
    _server.send(302, "text/plain", "");
  }

  void handleRoot() {
    // Scan WiFi sekitar
    int n = WiFi.scanNetworks();
    String wifiOptions = "";
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      wifiOptions += "<option value='" + ssid + "'>" + ssid + " (" + String(rssi) + " dBm)</option>";
    }

    String html = F(
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Agy IoT Setup Portal</title>"
      "<style>"
      "body{background:#0f172a;color:#f8fafc;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;margin:0;padding:20px;display:flex;justify-content:center}"
      ".card{background:#1e293b;border-radius:16px;box-shadow:0 10px 25px rgba(0,0,0,0.5);width:100%;max-width:420px;padding:24px;box-sizing:border-box;border:1px solid #334155}"
      "h2{color:#38bdf8;margin-top:0;font-size:22px;display:flex;align-items:center;gap:8px}"
      "p{color:#94a3b8;font-size:14px;margin-bottom:20px}"
      "label{display:block;font-size:12px;font-weight:600;color:#cbd5e1;margin-bottom:6px;text-transform:uppercase;letter-spacing:0.5px}"
      "input,select{width:100%;padding:12px;margin-bottom:16px;border:1px solid #475569;border-radius:8px;background:#0f172a;color:#f8fafc;font-size:14px;box-sizing:border-box}"
      "input:focus,select:focus{border-color:#38bdf8;outline:none}"
      "button{width:100%;background:#0284c7;color:#fff;border:none;padding:14px;border-radius:8px;font-weight:bold;font-size:16px;cursor:pointer;transition:background 0.2s}"
      "button:hover{background:#0369a1}"
      ".footer{text-align:center;font-size:11px;color:#64748b;margin-top:20px}"
      "</style></head><body>"
      "<div class='card'>"
      "<h2><span>⚡</span> Agy Gateway Setup</h2>"
      "<p>Konfigurasi koneksi WiFi dan parameter IoT Gateway Server.</p>"
      "<form method='POST' action='/save'>"
      "<label>Pilih Jaringan WiFi</label>"
      "<select onchange=\"if(this.value) document.getElementById('ssid').value=this.value;\">"
      "<option value=''>-- Pilih SSID dari scan --</option>"
    );

    html += wifiOptions;
    html += F(
      "</select>"
      "<label>WiFi SSID</label>"
      "<input type='text' id='ssid' name='ssid' placeholder='Nama WiFi' required>"
      "<label>WiFi Password</label>"
      "<input type='password' name='pass' placeholder='Password WiFi'>"
      "<hr style='border:0;border-top:1px solid #334155;margin:16px 0'>"
      "<label>Gateway Server Host / IP</label>"
      "<input type='text' name='host' placeholder='192.168.1.100 atau domain' required>"
      "<div style='display:flex;gap:10px'>"
      "<div style='flex:1'><label>Port</label><input type='number' name='port' value='3050' required></div>"
      "<div style='flex:1'><label>WS Path</label><input type='text' name='path' value='/ws' required></div>"
      "</div>"
      "<label>Device ID</label>"
      "<input type='text' name='devId' placeholder='contoh: node-01' required>"
      "<label>Device Secret Key</label>"
      "<input type='text' name='devKey' placeholder='Kunci rahasia device' required>"
      "<button type='submit'>Simpan & Sambungkan</button>"
      "</form>"
      "<div class='footer'>AgyGatewayClient Universal IoT Framework</div>"
      "</div></body></html>"
    );

    _server.send(200, "text/html", html);
  }

  void handleSave() {
    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");
    String host = _server.arg("host");
    uint16_t port = _server.arg("port").toInt();
    String path = _server.arg("path");
    String devId = _server.arg("devId");
    String devKey = _server.arg("devKey");

    if (port == 0) port = 3050;
    if (path.length() == 0) path = "/ws";

    AgyStorage::saveNetworkConfig(ssid, pass, host, port, path, devId, devKey);

    String successHtml = F(
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Tersimpan!</title>"
      "<style>"
      "body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center}"
      ".box{background:#1e293b;border-radius:16px;padding:30px;max-width:400px;margin:auto;border:1px solid #334155}"
      "h3{color:#4ade80}"
      "p{color:#94a3b8;font-size:14px;line-height:1.5}"
      "</style></head><body>"
      "<div class='box'>"
      "<h3>✅ Konfigurasi Berhasil Disimpan!</h3>"
      "<p>Perangkat sekarang akan menghubungkan ke jaringan WiFi dan server gateway Anda.</p>"
      "<p style='color:#38bdf8'>Silakan sambungkan kembali smartphone / PC Anda ke WiFi utama.</p>"
      "</div></body></html>"
    );

    _server.send(200, "text/html", successHtml);
    _saved = true;
  }
};

#endif // AGY_PORTAL_H
