#ifndef AGY_DRIVERS_H
#define AGY_DRIVERS_H

#include <Arduino.h>
#include <Wire.h>
#include <vector>

struct AgyI2CDeviceInfo {
  uint8_t address;
  String addressHex;
  String name;
  String category;
};

namespace AgyDrivers {

// -------------------------------------------------------------------
// 1. Bit-banging DHT Reader (DHT11 & DHT22) — zero dependency
//    Dilengkapi proteksi noInterrupts() agar timing mikrodetik aman dari WiFi
// -------------------------------------------------------------------
inline bool readDHT(uint8_t pin, bool isDHT22, float& temperature, float& humidity) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(isDHT22 ? 2 : 20); // DHT11 perlu setidaknya 18ms
  digitalWrite(pin, HIGH);
  delayMicroseconds(30);
  pinMode(pin, INPUT_PULLUP);

  // Bagian Kritis Waktu: Nonaktifkan Interrupt sementara
  noInterrupts();

  // Tunggu respon sensor: LOW ~80us lalu HIGH ~80us (overflow-safe)
  unsigned long startU = micros();
  while (digitalRead(pin) == HIGH) {
    if (micros() - startU > 200) { interrupts(); return false; }
  }
  startU = micros();
  while (digitalRead(pin) == LOW) {
    if (micros() - startU > 200) { interrupts(); return false; }
  }
  startU = micros();
  while (digitalRead(pin) == HIGH) {
    if (micros() - startU > 200) { interrupts(); return false; }
  }

  // Baca 40 bits (5 byte)
  for (int i = 0; i < 40; i++) {
    startU = micros();
    while (digitalRead(pin) == LOW) {
      if (micros() - startU > 200) { interrupts(); return false; }
    }
    unsigned long t = micros();
    startU = micros();
    while (digitalRead(pin) == HIGH) {
      if (micros() - startU > 200) { interrupts(); return false; }
    }
    // Jika durasi pulsa HIGH > 40us, maka bit bernilai 1
    if ((micros() - t) > 40) {
      data[i / 8] |= (1 << (7 - (i % 8)));
    }
  }

  // Aktifkan kembali Interrupt
  interrupts();

  // Verifikasi Checksum
  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    return false;
  }

  if (isDHT22) {
    humidity = ((data[0] << 8) | data[1]) * 0.1f;
    int16_t rawTemp = ((data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80) rawTemp = -rawTemp;
    temperature = rawTemp * 0.1f;
  } else {
    humidity = (float)data[0];
    temperature = (float)data[2];
  }
  return true;
}

// -------------------------------------------------------------------
// 2. Bit-banging 1-Wire DS18B20 Temperature Reader — zero dependency
// -------------------------------------------------------------------
inline bool oneWireReset(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(480);
  noInterrupts();
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(70);
  uint8_t presence = digitalRead(pin);
  interrupts();
  delayMicroseconds(410);
  return (presence == LOW); // LOW menandakan sensor hadir (presence pulse)
}

inline void oneWireWriteBit(uint8_t pin, uint8_t bit) {
  pinMode(pin, OUTPUT);
  noInterrupts();
  digitalWrite(pin, LOW);
  delayMicroseconds(bit ? 6 : 60);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(bit ? 64 : 10);
  interrupts();
}

inline uint8_t oneWireReadBit(uint8_t pin) {
  pinMode(pin, OUTPUT);
  noInterrupts();
  digitalWrite(pin, LOW);
  delayMicroseconds(3);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(10);
  uint8_t r = digitalRead(pin);
  interrupts();
  delayMicroseconds(53);
  return r;
}

inline void oneWireWriteByte(uint8_t pin, uint8_t byteVal) {
  for (uint8_t i = 0; i < 8; i++) {
    oneWireWriteBit(pin, byteVal & 0x01);
    byteVal >>= 1;
  }
}

inline uint8_t oneWireReadByte(uint8_t pin) {
  uint8_t byteVal = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (oneWireReadBit(pin)) {
      byteVal |= (1 << i);
    }
  }
  return byteVal;
}

inline bool readDS18B20(uint8_t pin, float& temperature) {
  // 1. Cek keberadaan sensor
  if (!oneWireReset(pin)) return false;

  // 2. Trigger konversi suhu
  oneWireWriteByte(pin, 0xCC); // Skip ROM
  oneWireWriteByte(pin, 0x44); // Start conversion

  // 3. Polling konversi selesai (pin bernilai HIGH saat ready)
  unsigned long startT = millis();
  while (oneWireReadBit(pin) == 0) {
    if (millis() - startT > 750) break; // Timeout maksimal 750ms
    yield();
  }

  // 4. Baca Scratchpad
  if (!oneWireReset(pin)) return false;
  oneWireWriteByte(pin, 0xCC); // Skip ROM
  oneWireWriteByte(pin, 0xBE); // Read Scratchpad

  uint8_t lsb = oneWireReadByte(pin);
  uint8_t msb = oneWireReadByte(pin);

  // Validasi nilai scratchpad
  if (lsb == 0xFF && msb == 0xFF) return false;
  if (lsb == 0x00 && msb == 0x00) return false;

  int16_t raw = (msb << 8) | lsb;
  temperature = raw / 16.0f;
  return (temperature >= -55.0f && temperature <= 125.0f);
}

// -------------------------------------------------------------------
// 3. I2C Bus Auto-Scanner & Native Sensor Drivers
// -------------------------------------------------------------------
inline void initI2C(int sdaPin = -1, int sclPin = -1) {
  static bool i2cInitialized = false;
  if (i2cInitialized && sdaPin < 0 && sclPin < 0) return;

#if defined(ESP8266)
  if (sdaPin >= 0 && sclPin >= 0) {
    Wire.begin(sdaPin, sclPin);
  } else {
    Wire.begin(4, 5); // Default D2 (GPIO4=SDA) & D1 (GPIO5=SCL) pada ESP8266
  }
#elif defined(ESP32)
  if (sdaPin >= 0 && sclPin >= 0) {
    Wire.begin(sdaPin, sclPin);
  } else {
    Wire.begin(21, 22); // Default SDA=21, SCL=22 pada ESP32
  }
#else
  Wire.begin();
#endif
  i2cInitialized = true;
}

inline std::vector<AgyI2CDeviceInfo> scanI2C(int sdaPin = -1, int sclPin = -1) {
  std::vector<AgyI2CDeviceInfo> found;

  initI2C(sdaPin, sclPin);

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      AgyI2CDeviceInfo dev;
      dev.address = addr;
      char hexBuf[10];
      snprintf(hexBuf, sizeof(hexBuf), "0x%02X", addr);
      dev.addressHex = String(hexBuf);

      if (addr == 0x76 || addr == 0x77) {
        dev.name = "BMP280 / BME280";
        dev.category = "Suhu & Tekanan Udara";
      } else if (addr == 0x23) {
        dev.name = "BH1750";
        dev.category = "Sensor Cahaya (Lux)";
      } else if (addr == 0x3C || addr == 0x3D) {
        dev.name = "SSD1306 OLED Display";
        dev.category = "Layar Grafis OLED";
      } else if (addr == 0x68) {
        dev.name = "DS3231 RTC / MPU6050";
        dev.category = "Real-Time Clock / Gyro";
      } else if (addr == 0x48 || addr == 0x49) {
        dev.name = "ADS1115 / PCF8591";
        dev.category = "ADC Presisi Eksternal";
      } else if (addr == 0x44 || addr == 0x45) {
        dev.name = "SHT30 / SHT31";
        dev.category = "Sensor Suhu & Kelembapan";
      } else if (addr == 0x38) {
        dev.name = "AHT10 / AHT20";
        dev.category = "Sensor Suhu & Kelembapan";
      } else if (addr == 0x27 || addr == 0x3F) {
        dev.name = "PCF8574 LCD I2C";
        dev.category = "Adapter LCD Karakter";
      } else {
        dev.name = "Modul I2C Universal";
        dev.category = "Perangkat I2C";
      }

      found.push_back(dev);
    }
  }

  return found;
}

// Driver Native BH1750 (Ambient Light Sensor)
inline bool readBH1750(float& lux, uint8_t addr = 0x23) {
  initI2C();
  // Power ON
  Wire.beginTransmission(addr);
  Wire.write(0x01);
  Wire.endTransmission();

  // Continuously H-Resolution Mode
  Wire.beginTransmission(addr);
  Wire.write(0x10);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  Wire.requestFrom((int)addr, 2);
  if (Wire.available() < 2) return false;
  uint16_t val = (Wire.read() << 8) | Wire.read();
  lux = val / 1.2f;
  return true;
}

// Driver Native SHT30 / SHT31 (Sensirion Temp & Humidity)
inline bool readSHT3x(float& temp, float& hum, uint8_t addr = 0x44) {
  initI2C();
  Wire.beginTransmission(addr);
  Wire.write(0x2C); // High repeatability measurement command
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;
  delay(20);
  Wire.requestFrom((int)addr, 6);
  if (Wire.available() < 6) return false;
  uint16_t rawT = (Wire.read() << 8) | Wire.read();
  Wire.read(); // crc
  uint16_t rawH = (Wire.read() << 8) | Wire.read();
  Wire.read(); // crc
  temp = -45.0f + (175.0f * (float)rawT / 65535.0f);
  hum = 100.0f * ((float)rawH / 65535.0f);
  return true;
}

// Driver Native AHT10 / AHT20 (Temp & Humidity)
inline bool readAHTx(float& temp, float& hum, uint8_t addr = 0x38) {
  initI2C();
  Wire.beginTransmission(addr);
  Wire.write(0xAC); // Trigger measurement
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(80);
  Wire.requestFrom((int)addr, 6);
  if (Wire.available() < 6) return false;
  uint8_t status = Wire.read();
  if ((status & 0x80) != 0) return false; // Masih sibuk
  uint32_t rawH = ((uint32_t)Wire.read() << 12) | ((uint32_t)Wire.read() << 4);
  uint8_t b3 = Wire.read();
  rawH |= (b3 >> 4);
  uint32_t rawT = (((uint32_t)b3 & 0x0F) << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
  hum = ((float)rawH * 100.0f) / 1048576.0f;
  temp = (((float)rawT * 200.0f) / 1048576.0f) - 50.0f;
  return true;
}

// Kalibrasi resmi Bosch Sensortec BMP280
struct BMP280Calib {
  uint16_t dig_T1 = 0;
  int16_t  dig_T2 = 0;
  int16_t  dig_T3 = 0;
  uint16_t dig_P1 = 0;
  int16_t  dig_P2 = 0;
  int16_t  dig_P3 = 0;
  int16_t  dig_P4 = 0;
  int16_t  dig_P5 = 0;
  int16_t  dig_P6 = 0;
  int16_t  dig_P7 = 0;
  int16_t  dig_P8 = 0;
  int16_t  dig_P9 = 0;
  bool loaded = false;
};

inline bool readBMP280Calib(uint8_t addr, BMP280Calib& cal) {
  Wire.beginTransmission(addr);
  Wire.write(0x88);
  if (Wire.endTransmission() != 0) return false;

  Wire.requestFrom((int)addr, 24);
  if (Wire.available() < 24) return false;

  uint8_t b[24];
  for (int i = 0; i < 24; i++) {
    b[i] = Wire.read();
  }

  cal.dig_T1 = (uint16_t)(b[1] << 8 | b[0]);
  cal.dig_T2 = (int16_t)(b[3] << 8 | b[2]);
  cal.dig_T3 = (int16_t)(b[5] << 8 | b[4]);
  cal.dig_P1 = (uint16_t)(b[7] << 8 | b[6]);
  cal.dig_P2 = (int16_t)(b[9] << 8 | b[8]);
  cal.dig_P3 = (int16_t)(b[11] << 8 | b[10]);
  cal.dig_P4 = (int16_t)(b[13] << 8 | b[12]);
  cal.dig_P5 = (int16_t)(b[15] << 8 | b[14]);
  cal.dig_P6 = (int16_t)(b[17] << 8 | b[16]);
  cal.dig_P7 = (int16_t)(b[19] << 8 | b[18]);
  cal.dig_P8 = (int16_t)(b[21] << 8 | b[20]);
  cal.dig_P9 = (int16_t)(b[23] << 8 | b[22]);
  cal.loaded = true;
  return true;
}

// Driver Native BMP280 (Temperature & Pressure) dengan Kompensasi Penuh Bosch
inline bool readBMP280(float& temp, float& pressureHpa, uint8_t addr = 0x76) {
  initI2C();
  static BMP280Calib cal76;
  static BMP280Calib cal77;
  BMP280Calib& cal = (addr == 0x77) ? cal77 : cal76;
  if (!cal.loaded) {
    if (!readBMP280Calib(addr, cal)) return false;
  }

  // Inisialisasi kontrol: Normal mode, osrs_t x1, osrs_p x1
  Wire.beginTransmission(addr);
  Wire.write(0xF4); // ctrl_meas
  Wire.write(0x27); // Normal mode (11), temp x1 (001), press x1 (001)
  if (Wire.endTransmission() != 0) return false;
  delay(10);

  // Baca raw data 0xF7..0xFC (6 byte)
  Wire.beginTransmission(addr);
  Wire.write(0xF7);
  if (Wire.endTransmission() != 0) return false;
  Wire.requestFrom((int)addr, 6);
  if (Wire.available() < 6) return false;

  uint32_t pMsb = Wire.read();
  uint32_t pLsb = Wire.read();
  uint32_t pXlsb = Wire.read();
  uint32_t tMsb = Wire.read();
  uint32_t tLsb = Wire.read();
  uint32_t tXlsb = Wire.read();

  int32_t adcT = (int32_t)((tMsb << 12) | (tLsb << 4) | (tXlsb >> 4));
  int32_t adcP = (int32_t)((pMsb << 12) | (pLsb << 4) | (pXlsb >> 4));

  if (adcT == 0x80000 || adcP == 0x80000) return false;

  // Rumus kompensasi suhu resmi Bosch Sensortec BMP280
  double var1 = (((double)adcT) / 16384.0 - ((double)cal.dig_T1) / 1024.0) * ((double)cal.dig_T2);
  double var2 = ((((double)adcT) / 131072.0 - ((double)cal.dig_T1) / 8192.0) *
                 (((double)adcT) / 131072.0 - ((double)cal.dig_T1) / 8192.0)) * ((double)cal.dig_T3);
  double t_fine = var1 + var2;
  temp = (float)(t_fine / 5120.0);

  // Rumus kompensasi tekanan resmi Bosch Sensortec BMP280
  var1 = (t_fine / 2.0) - 64000.0;
  var2 = var1 * var1 * ((double)cal.dig_P6) / 32768.0;
  var2 = var2 + var1 * ((double)cal.dig_P5) * 2.0;
  var2 = (var2 / 4.0) + (((double)cal.dig_P4) * 65536.0);
  var1 = (((double)cal.dig_P3) * var1 * var1 / 524288.0 + ((double)cal.dig_P2) * var1) / 524288.0;
  var1 = (1.0 + var1 / 32768.0) * ((double)cal.dig_P1);
  if (var1 == 0.0) {
    pressureHpa = 0.0f;
    return false;
  }

  double p = 1048576.0 - (double)adcP;
  p = (p - (var2 / 4096.0)) * 6250.0 / var1;
  var1 = ((double)cal.dig_P9) * p * p / 2147483648.0;
  var2 = p * ((double)cal.dig_P8) / 32768.0;
  p = p + (var1 + var2 + ((double)cal.dig_P7)) / 16.0;

  pressureHpa = (float)(p / 100.0); // Konversi Pascal ke hPa

  return (temp >= -40.0f && temp <= 85.0f && pressureHpa >= 300.0f && pressureHpa <= 1100.0f);
}

} // namespace AgyDrivers

#endif // AGY_DRIVERS_H
