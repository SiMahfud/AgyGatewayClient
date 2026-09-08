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

// 1. Bit-banging DHT Reader (DHT11 & DHT22) — zero dependency
inline bool readDHT(uint8_t pin, bool isDHT22, float& temperature, float& humidity) {
  uint8_t data[5] = {0, 0, 0, 0, 0};

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(isDHT22 ? 2 : 20); // DHT11 perlu setidaknya 18ms
  digitalWrite(pin, HIGH);
  delayMicroseconds(30);
  pinMode(pin, INPUT_PULLUP);

  // Tunggu respon sensor: LOW ~80us lalu HIGH ~80us
  unsigned long timeout = micros() + 200;
  while (digitalRead(pin) == HIGH) {
    if (micros() > timeout) return false;
  }
  timeout = micros() + 200;
  while (digitalRead(pin) == LOW) {
    if (micros() > timeout) return false;
  }
  timeout = micros() + 200;
  while (digitalRead(pin) == HIGH) {
    if (micros() > timeout) return false;
  }

  // Baca 40 bits (5 byte)
  for (int i = 0; i < 40; i++) {
    timeout = micros() + 200;
    while (digitalRead(pin) == LOW) {
      if (micros() > timeout) return false;
    }
    unsigned long t = micros();
    timeout = micros() + 200;
    while (digitalRead(pin) == HIGH) {
      if (micros() > timeout) return false;
    }
    // Jika durasi pulsa HIGH > 40us, maka bit bernilai 1
    if ((micros() - t) > 40) {
      data[i / 8] |= (1 << (7 - (i % 8)));
    }
  }

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

// 2. Bit-banging 1-Wire DS18B20 Temperature Reader — zero dependency
inline void oneWireReset(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(480);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(480);
}

inline void oneWireWriteBit(uint8_t pin, uint8_t bit) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(bit ? 6 : 60);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(bit ? 64 : 10);
}

inline uint8_t oneWireReadBit(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(3);
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(10);
  uint8_t r = digitalRead(pin);
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
  // Trigger konversi suhu
  oneWireReset(pin);
  oneWireWriteByte(pin, 0xCC); // Skip ROM
  oneWireWriteByte(pin, 0x44); // Start conversion

  delay(20); // Tunggu konversi singkat jika sudah ready

  // Baca scratchpad
  oneWireReset(pin);
  oneWireWriteByte(pin, 0xCC); // Skip ROM
  oneWireWriteByte(pin, 0xBE); // Read Scratchpad

  uint8_t lsb = oneWireReadByte(pin);
  uint8_t msb = oneWireReadByte(pin);

  // Jika jalur terputus atau floating
  if (lsb == 0xFF && msb == 0xFF) return false;
  if (lsb == 0x00 && msb == 0x00) return false;

  int16_t raw = (msb << 8) | lsb;
  temperature = raw / 16.0f;
  return (temperature >= -55.0f && temperature <= 125.0f);
}

// 3. I2C Bus Auto-Scanner & Device Identifier
inline std::vector<AgyI2CDeviceInfo> scanI2C(int sdaPin = -1, int sclPin = -1) {
  std::vector<AgyI2CDeviceInfo> found;

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

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      AgyI2CDeviceInfo dev;
      dev.address = addr;
      char hexBuf[10];
      snprintf(hexBuf, sizeof(hexBuf), "0x%02X", addr);
      dev.addressHex = String(hexBuf);

      // Identifikasi tipe modul I2C populer
      if (addr == 0x76 || addr == 0x77) {
        dev.name = "BMP280 / BME280";
        dev.category = "Suhu, Tekanan Udara & Kelembapan";
      } else if (addr == 0x23) {
        dev.name = "BH1750";
        dev.category = "Sensor Intensitas Cahaya (Lux)";
      } else if (addr == 0x3C || addr == 0x3D) {
        dev.name = "SSD1306 OLED Display";
        dev.category = "Layar Grafis OLED (128x64 / 128x32)";
      } else if (addr == 0x68) {
        dev.name = "DS3231 RTC / MPU6050 Gyro";
        dev.category = "Real-Time Clock atau Accelerometer/Gyro";
      } else if (addr == 0x48 || addr == 0x49) {
        dev.name = "ADS1115 / PCF8591";
        dev.category = "ADC Presisi Eksternal 16-Bit";
      } else if (addr == 0x44 || addr == 0x45) {
        dev.name = "SHT30 / SHT31";
        dev.category = "Sensor Suhu & Kelembapan Sensirion";
      } else if (addr == 0x38) {
        dev.name = "AHT10 / AHT20";
        dev.category = "Sensor Suhu & Kelembapan Presisi";
      } else if (addr == 0x27 || addr == 0x3F) {
        dev.name = "PCF8574 LCD I2C Adapter";
        dev.category = "Antarmuka LCD Karakter 16x2 / 20x4";
      } else {
        dev.name = "Modul I2C Universal";
        dev.category = "Perangkat I2C";
      }

      found.push_back(dev);
    }
  }

  return found;
}

} // namespace AgyDrivers

#endif // AGY_DRIVERS_H
