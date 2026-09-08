# AgyGatewayClient 🚀
Universal Modular IoT Client Library for ESP8266 & ESP32.

Library Arduino & PlatformIO universal yang dirancang untuk menghubungkan mikrokontroler (Wemos D1 Mini, NodeMCU, ESP32) ke **Personal IoT Gateway Server** Anda dengan dukungan:
- 🔌 **Modular Components**: Daftarkan saklar relay, sensor digital, sensor analog, atau komponen kustom dengan mudah.
- ⚡ **Real-Time WebSocket**: Komunikasi dua arah instan (latensi rendah), auto-reconnect, dan pengamanan Device Secret Key.
- ⏱️ **Independent Hardware Countdown Timer**: Timer countdown relay berjalan mandiri di dalam chip mikrokontroler, aman meskipun koneksi internet/server terputus di tengah jalan.
- ☁️ **Over-The-Air (OTA) Updates**: Update biner firmware `.bin` jarak jauh lewat WiFi dari server dashboard tanpa mencolok kabel USB.
- 🌐 **Virtual Pins**: Kompatibel dengan konsep Virtual Pin ala Blynk (`V1`, `V2`, dll).

---

## 📦 Cara Instalasi

### A. PlatformIO (Rekomendasi)
Tambahkan dependensi pada `platformio.ini`:

```ini
[env:d1_mini]
platform = espressif8266
board = d1_mini
framework = arduino
monitor_speed = 115200

lib_deps = 
    https://github.com/SiMahfud/AgyGatewayClient.git
```
*(PlatformIO akan secara otomatis mengunduh dependensi `ArduinoJson` dan `WebSockets` dari `library.json`)*.

### B. Arduino IDE
1. Download repository ini sebagai file `.ZIP` (klik tombol hijau **Code** > **Download ZIP** di GitHub).
2. Buka **Arduino IDE**.
3. Masuk ke menu **Sketch** > **Include Library** > **Add .ZIP Library...**.
4. Pilih file `.ZIP` yang baru di-download.
5. Pastikan dependensi **ArduinoJson** (v7+) dan **WebSockets** (oleh Markus Sattler) sudah terinstal via Library Manager Arduino IDE.

---

## 🚀 Contoh Penggunaan Cepat

### 0. Universal Node (Zero-Code — Konfigurasi 100% dari Web UI)
Tanpa perlu mendefinisikan pin atau menulis driver di file C++, cukup upload sketch ini sekali:
```cpp
#include <Arduino.h>
#include <AgyGatewayClient.h>

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);

  // Aktifkan mode dinamis: semua pin & sensor diatur dari Web Dashboard
  iot.enableDynamicPins(true);

  iot.begin("WiFi_SSID", "WiFi_PASS", "192.168.1.100", 3050, "/ws", "wemos-01", "SECRET_KEY");
}

void loop() {
  iot.loop();
}
```
*Tancapkan sensor (DHT11/22, DS18B20, PIR, Analog, atau modul I2C) dan atur pinnya langsung dari antarmuka Web Dashboard!*

### 1. Smart Switch (Relay)
```cpp
#include <Arduino.h>
#include <AgyGatewayClient.h>

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);

  // Daftarkan komponen relay
  iot.addSwitch("relay_1", 5,  "Lampu Kamar", true);
  iot.addSwitch("relay_2", 4,  "Kipas Angin", true);

  // Mulai koneksi
  iot.begin("WiFi_SSID", "WiFi_PASS", "192.168.1.100", 3050, "/ws", "wemos-01", "SECRET_KEY");
}

void loop() {
  iot.loop();
}
```

### 2. Multi-Sensor Station
```cpp
#include <Arduino.h>
#include <AgyGatewayClient.h>

AgyGatewayClient iot;

float getTemp() {
  return 28.5; // ganti dengan pembacaan sensor DHT/BMP
}

void setup() {
  Serial.begin(115200);

  // Tambah sensor dengan fungsi pembacaan otomatis setiap 5 detik
  iot.addSensor("temp_sensor", "Suhu Ruangan", "°C", getTemp, 5000);

  iot.begin("WiFi_SSID", "WiFi_PASS", "192.168.1.100", 3050, "/ws", "weather-01", "SECRET_KEY");
}

void loop() {
  iot.loop();
}
```

---

## 📡 Format Protokol WebSocket

### 1. Registrasi Otomatis (`event: "register"`)
Dikirim otomatis oleh library ke server saat pertama kali WebSocket terhubung:
```json
{
  "event": "register",
  "deviceId": "wemos-01",
  "key": "SECRET_KEY",
  "info": {
    "chip": "ESP8266",
    "firmware": "1.0.0",
    "uptime": 120,
    "rssi": -65
  },
  "components": [
    { "id": "relay_1", "name": "Lampu Kamar", "type": "switch", "access": "rw", "value": "false", "pin": 5 },
    { "id": "temp_sensor", "name": "Suhu Ruangan", "type": "sensor", "access": "r", "unit": "°C", "value": "28.50" }
  ]
}
```

### 2. Telemetri Sensor (`event: "telemetry"`)
Dikirim saat ada pembaruan data sensor atau saklar:
```json
{
  "event": "telemetry",
  "deviceId": "wemos-01",
  "key": "SECRET_KEY",
  "uptime": 150,
  "rssi": -63,
  "data": {
    "temp_sensor": "28.70",
    "relay_1": "true"
  }
}
```

### 3. Kontrol dari Server (`action: "set_component"`)
Diterima oleh mikrokontroler dari server atau dashboard web:
```json
{
  "action": "set_component",
  "target": "wemos-01",
  "componentId": "relay_1",
  "value": true,
  "duration": 60
}
```

### 4. Remote OTA Update (`action: "ota_update"`)
```json
{
  "action": "ota_update",
  "url": "http://192.168.1.100:3050/firmwares/firmware_v2.bin"
}
```

---

## 🛠️ Kompatibilitas Hardware
- ✅ **ESP8266**: Wemos D1 Mini, NodeMCU, ESP-01, ESP-12F
- ✅ **ESP32**: ESP32-WROOM, NodeMCU-32S, ESP32-C3, ESP32-S3

---

## 📄 Lisensi
MIT License. Bebas digunakan, dimodifikasi, dan didistribusikan untuk keperluan pribadi maupun komersial.
