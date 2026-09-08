# AgyGatewayClient 🚀
Universal Modular IoT Client Library for ESP8266 & ESP32.

Library Arduino & PlatformIO universal yang dirancang untuk menghubungkan mikrokontroler (Wemos D1 Mini, NodeMCU, ESP32) ke **Personal IoT Gateway Server** Anda dengan dukungan:
- 🌐 **WiFi Captive Portal**: Konfigurasi SSID, Password WiFi, IP Server, Device ID & Key langsung dari HP/Laptop lewat Access Point lokal — tanpa perlu flash ulang firmware.
- 💾 **Flash Persistence**: Kredensial jaringan & konfigurasi Dynamic Pins tersimpan permanen di Flash (LittleFS). Node tetap beroperasi mandiri saat mati listrik atau reboot.
- 🔌 **Modular Components**: Daftarkan saklar relay, sensor digital, sensor analog, atau komponen kustom dengan mudah.
- ⚡ **Real-Time WebSocket**: Komunikasi dua arah instan (latensi rendah), auto-reconnect, dan pengamanan Device Secret Key.
- 📊 **Delta Telemetry**: Hanya mengirimkan data sensor/aktuator yang nilainya berubah, menghemat bandwidth dan beban CPU secara drastis.
- ⏱️ **Independent Hardware Countdown Timer**: Timer countdown relay berjalan mandiri di dalam chip mikrokontroler, aman meskipun koneksi internet/server terputus.
- ☁️ **Over-The-Air (OTA) Updates**: Update firmware `.bin` jarak jauh lewat WiFi (HTTP & HTTPS) dari server dashboard tanpa kabel USB.
- 🌐 **Virtual Pins**: Kompatibel dengan konsep Virtual Pin ala Blynk (`V1`, `V2`, dll).
- 🔍 **Auto I2C Sensor Driver**: Pembacaan otomatis untuk modul I2C populer (BMP280, BH1750, SHT30, AHT20) tanpa library pihak ketiga.
- 💡 **Status LED Indicator**: Indikator visual pola kedip LED untuk diagnosa status koneksi tanpa Serial Monitor.

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

## 🚀 Contoh Penggunaan

### 0. Universal Node (Zero-Code — Captive Portal + Flash Persistence)
Upload sketch ini **sekali saja**. Semua konfigurasi (WiFi, Server, Pin, Sensor) dilakukan dari HP/Laptop:

```cpp
#include <Arduino.h>
#include <AgyGatewayClient.h>

AgyGatewayClient iot;

void setup() {
  Serial.begin(115200);

  // Aktifkan LED status dan mode dynamic pins
  iot.enableStatusLed(LED_BUILTIN, true);
  iot.enableDynamicPins(true);

  // Auto-Connect: baca kredensial dari Flash, jika gagal buka Captive Portal
  iot.autoConnect();
}

void loop() {
  iot.loop();
}
```

**Cara Pakai:**
1. Upload sketch di atas ke Wemos D1 Mini / ESP32.
2. Jika belum ada konfigurasi tersimpan, modul akan membuka **Access Point** bernama `AGY-NODE-XXXXXX`.
3. Sambungkan HP/Laptop ke AP tersebut. Browser otomatis terbuka ke halaman konfigurasi.
4. Isi SSID WiFi, Password, IP Server Gateway, Port, Device ID, dan Device Key.
5. Klik **Simpan & Sambungkan**. Modul akan me-restart dan langsung terhubung!
6. Konfigurasi tersimpan di Flash — aman meskipun listrik mati atau modul di-restart.

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

## 🌐 WiFi Captive Portal

Library ini memiliki **Captive Portal terintegrasi** tanpa dependensi eksternal:

| Metode | Deskripsi |
|--------|-----------|
| `iot.autoConnect()` | Otomatis baca kredensial dari Flash. Jika gagal atau belum ada, buka AP Captive Portal. |
| `iot.autoConnect("MyAP", 120)` | Sama seperti di atas, dengan nama AP custom dan timeout 120 detik. |
| `iot.startPortal()` | Paksa buka Captive Portal (misal dipicu tombol reset). |

**Pola Kedip LED Status:**
| Pola | Status |
|------|--------|
| Kedip sangat cepat (~100ms) | Captive Portal AP aktif, menunggu konfigurasi |
| Kedip cepat (~250ms) | Menghubungkan ke WiFi |
| Kedip lambat (~600ms) | WiFi OK, menghubungkan ke Gateway Server |
| Mati | Terhubung penuh dan siap |

---

## 💾 Flash Persistence

Semua konfigurasi disimpan otomatis ke Flash (LittleFS):

| File Flash | Isi |
|------------|-----|
| `/agy_config.json` | SSID WiFi, Password, Host Gateway, Port, Path, Device ID, Device Key |
| `/agy_pins.json` | Array konfigurasi komponen dinamis (relay, sensor, I2C) |

**Method Terkait:**
```cpp
iot.clearPinStorage();  // Hapus konfigurasi pin dari Flash
iot.clearAllStorage();  // Reset pabrik (hapus semua konfigurasi)
```

---

## 📊 Delta Telemetry

Secara default, `sendTelemetry()` hanya mengirimkan komponen yang nilainya berubah (**delta**). Ini secara signifikan menghemat bandwidth dan memori, terutama pada node dengan banyak sensor.

- **Heartbeat berkala** (setiap 30 detik) mengirimkan sinkronisasi penuh (`sendFullTelemetry()`).
- Server dapat membedakan paket delta dari flag `"delta": true` pada payload JSON.

---

## 📡 Format Protokol WebSocket

### 1. Registrasi Otomatis (`event: "register"`)
Dikirim otomatis saat WebSocket pertama kali terhubung:
```json
{
  "event": "register",
  "deviceId": "wemos-01",
  "key": "SECRET_KEY",
  "info": {
    "chip": "ESP8266",
    "firmware": "1.1.0",
    "uptime": 120,
    "rssi": -65,
    "dynamicPins": true
  },
  "components": [
    { "id": "relay_1", "name": "Lampu Kamar", "type": "switch", "driver": "switch", "access": "rw", "value": "false", "pin": 5 },
    { "id": "temp_sensor", "name": "Suhu", "type": "sensor", "driver": "bmp280", "access": "r", "unit": "°C", "value": "28.50", "i2cAddr": 118 }
  ]
}
```

### 2. Delta Telemetry (`event: "telemetry"`)
Hanya komponen yang berubah:
```json
{
  "event": "telemetry",
  "deviceId": "wemos-01",
  "key": "SECRET_KEY",
  "uptime": 150,
  "rssi": -63,
  "delta": true,
  "data": {
    "temp_sensor": "29.10"
  }
}
```

### 3. Full Telemetry (Heartbeat Berkala)
```json
{
  "event": "telemetry",
  "deviceId": "wemos-01",
  "key": "SECRET_KEY",
  "uptime": 180,
  "rssi": -62,
  "data": {
    "relay_1": "true",
    "temp_sensor": "29.10",
    "humidity": "65.2"
  }
}
```

### 4. Kontrol dari Server (`action: "set_component"`)
```json
{
  "action": "set_component",
  "target": "wemos-01",
  "componentId": "relay_1",
  "value": true,
  "duration": 60
}
```

### 5. Remote OTA Update (`action: "ota_update"`)
Mendukung HTTP dan HTTPS:
```json
{
  "action": "ota_update",
  "url": "https://192.168.1.100:3050/firmwares/firmware_v2.bin"
}
```

---

## 🔍 Sensor I2C Otomatis

Library ini memiliki driver native (tanpa library pihak ketiga) untuk modul I2C populer:

| Modul | Driver | Alamat I2C | Data |
|-------|--------|------------|------|
| BMP280 / BME280 | `bmp280` | 0x76 / 0x77 | Suhu & Tekanan |
| BH1750 | `bh1750` | 0x23 | Intensitas Cahaya (Lux) |
| SHT30 / SHT31 | `sht30` | 0x44 / 0x45 | Suhu & Kelembapan |
| AHT10 / AHT20 | `aht10` | 0x38 | Suhu & Kelembapan |

Sensor I2C dikonfigurasi melalui Web Dashboard atau perintah `configure_pin` via WebSocket.

---

## 🛠️ Kompatibilitas Hardware
- ✅ **ESP8266**: Wemos D1 Mini, NodeMCU, ESP-01, ESP-12F
- ✅ **ESP32**: ESP32-WROOM, NodeMCU-32S, ESP32-C3, ESP32-S3

---

## 📄 Lisensi
MIT License. Bebas digunakan, dimodifikasi, dan didistribusikan untuk keperluan pribadi maupun komersial.
