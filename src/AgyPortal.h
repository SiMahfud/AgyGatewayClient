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

  void start(const char* customApName = nullptr, const char* customApPass = nullptr) {
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

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    if (customApPass && strlen(customApPass) >= 8) {
      WiFi.softAP(apName.c_str(), customApPass);
      Serial.printf("\n[PORTAL] Mengaktifkan AP Terproteksi: %s (Pass: %s, IP: 192.168.4.1)\n", apName.c_str(), customApPass);
    } else {
      WiFi.softAP(apName.c_str());
      Serial.printf("\n[PORTAL] Mengaktifkan AP Terbuka: %s (IP: 192.168.4.1)\n", apName.c_str());
    }

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
      WiFi.mode(WIFI_STA); // Nonaktifkan mode AP radio seutuhnya
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

    // Muat konfigurasi tersimpan jika ada, atau buat nilai default cerdas
    String curSsid, curPass, curHost, curPath, curDevId, curDevKey;
    uint16_t curPort = 3050;
    bool curSsl = false;
    bool hasSaved = AgyStorage::loadNetworkConfig(curSsid, curPass, curHost, curPort, curPath, curDevId, curDevKey, curSsl);

    if (!hasSaved || curDevId.length() == 0) {
#if defined(ESP8266)
      curDevId = "node-" + String(ESP.getChipId(), HEX);
#elif defined(ESP32)
      uint32_t chipId = 0;
      for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
      curDevId = "node-" + String(chipId, HEX);
#else
      curDevId = "node-01";
#endif
      curDevId.toLowerCase();
    }

    if (!hasSaved || curDevKey.length() == 0) {
      curDevKey = "wemos-secret-key-3377"; // Default secret key server
    }
    if (curPort == 0) curPort = 3050;
    if (curPath.length() == 0) curPath = "/ws";

    auto escapeHtml = [](const String& in) -> String {
      String s = in;
      s.replace("\"", "&quot;");
      return s;
    };

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
    );
    html += "<input type='text' id='ssid' name='ssid' value='" + escapeHtml(curSsid) + "' placeholder='Nama WiFi' required>";
    html += F(
      "<label>WiFi Password</label>"
    );
    html += "<input type='password' name='pass' value='" + escapeHtml(curPass) + "' placeholder='Password WiFi'>";
    html += F(
      "<hr style='border:0;border-top:1px solid #334155;margin:16px 0'>"
      "<label>Gateway Server Host / IP</label>"
    );
    html += "<input type='text' name='host' value='" + escapeHtml(curHost) + "' placeholder='192.168.1.100 atau domain' required>";
    html += F(
      "<div style='display:flex;gap:10px'>"
      "<div style='flex:1'><label>Port</label>"
    );
    html += "<input type='number' name='port' value='" + String(curPort) + "' required></div>";
    html += F(
      "<div style='flex:1'><label>WS Path</label>"
    );
    html += "<input type='text' name='path' value='" + escapeHtml(curPath) + "' required></div></div>";
    html += F(
      "<div style='margin-bottom:16px;display:flex;align-items:center;gap:8px'>"
    );
    html += "<input type='checkbox' id='ssl' name='ssl' value='1' style='width:auto;margin:0'" + String(curSsl ? " checked" : "") + ">";
    html += F(
      "<label for='ssl' style='margin:0;cursor:pointer;text-transform:none;font-size:13px'>Gunakan Koneksi Aman SSL / WSS</label>"
      "</div>"
      "<label>Device ID</label>"
    );
    html += "<input type='text' name='devId' value='" + escapeHtml(curDevId) + "' placeholder='contoh: node-01' required>";
    html += F(
      "<label>Device Secret Key</label>"
    );
    html += "<input type='text' name='devKey' value='" + escapeHtml(curDevKey) + "' placeholder='Kunci rahasia device' required>";
    html += F(
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
    String portStr = _server.arg("port");
    String path = _server.arg("path");
    String devId = _server.arg("devId");
    String devKey = _server.arg("devKey");
    bool useSsl = _server.hasArg("ssl") && (_server.arg("ssl") == "1" || _server.arg("ssl") == "on");

    ssid.trim();
    pass.trim();
    host.trim();
    path.trim();
    devId.trim();
    devKey.trim();

    // Sanitasi karakter kontrol yang berpotensi merusak file JSON Flash
    auto sanitize = [](String& str) {
      str.replace("\r", "");
      str.replace("\n", "");
      str.replace("\"", "");
      str.replace("\\", "");
    };
    sanitize(ssid);
    sanitize(host);
    sanitize(devId);
    sanitize(devKey);

    // Validasi field utama
    if (ssid.length() == 0 || host.length() == 0 || devId.length() == 0 || devKey.length() == 0) {
      String errHtml = F(
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Input Tidak Lengkap</title>"
        "<style>"
        "body{background:#0f172a;color:#f8fafc;font-family:sans-serif;padding:30px;text-align:center}"
        ".box{background:#1e293b;border-radius:16px;padding:30px;max-width:400px;margin:auto;border:1px solid #ef4444}"
        "h3{color:#f87171}"
        "p{color:#94a3b8;font-size:14px;line-height:1.5}"
        "a{display:inline-block;margin-top:15px;color:#38bdf8;text-decoration:none;font-weight:bold}"
        "</style></head><body>"
        "<div class='box'>"
        "<h3>❌ Input Belum Lengkap</h3>"
        "<p>SSID WiFi, Host Gateway, Device ID, dan Device Key wajib diisi!</p>"
        "<a href='/'>&larr; Kembali ke Form Pengaturan</a>"
        "</div></body></html>"
      );
      _server.send(400, "text/html", errHtml);
      return;
    }

    long p = portStr.toInt();
    uint16_t port = (p > 0 && p <= 65535) ? (uint16_t)p : 3050;
    if (path.length() == 0) path = "/ws";
    if (!path.startsWith("/")) path = "/" + path;
    if (port == 443) useSsl = true;

    AgyStorage::saveNetworkConfig(ssid, pass, host, port, path, devId, devKey, useSsl);

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
