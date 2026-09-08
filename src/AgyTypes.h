#ifndef AGY_TYPES_H
#define AGY_TYPES_H

#include <Arduino.h>
#include <functional>

enum AgyComponentType {
  AGY_SWITCH,    // Relay, digital output (ON/OFF)
  AGY_SENSOR,    // Suhu, kelembapan, analog (Read-Only float/int)
  AGY_DIMMER,    // Slider / PWM (0-100 atau 0-255)
  AGY_INDICATOR, // Digital In (PIR, Sensor Pintu, dsb)
  AGY_VIRTUAL    // Virtual Pin ala Blynk (V0, V1, dst)
};

inline const char* agyComponentTypeToString(AgyComponentType t) {
  switch (t) {
    case AGY_SWITCH:    return "switch";
    case AGY_SENSOR:    return "sensor";
    case AGY_DIMMER:    return "dimmer";
    case AGY_INDICATOR: return "indicator";
    case AGY_VIRTUAL:   return "virtual";
    default:            return "custom";
  }
}

inline AgyComponentType agyStringToComponentType(const String& str) {
  if (str == "switch")    return AGY_SWITCH;
  if (str == "sensor")    return AGY_SENSOR;
  if (str == "dimmer")    return AGY_DIMMER;
  if (str == "indicator") return AGY_INDICATOR;
  return AGY_VIRTUAL;
}

enum AgyDriverType {
  AGY_DRIVER_CUSTOM = 0,
  AGY_DRIVER_SWITCH,      // Digital Output (Relay, Saklar, LED)
  AGY_DRIVER_DIGITAL_IN,  // Digital Input (PIR, Tombol, Sensor Pintu)
  AGY_DRIVER_ANALOG,      // ADC / Analog In (A0)
  AGY_DRIVER_DHT11,       // Sensor Suhu & Kelembapan DHT11
  AGY_DRIVER_DHT22,       // Sensor Suhu & Kelembapan DHT22 / AM2302
  AGY_DRIVER_DS18B20,     // Sensor Suhu 1-Wire Dallas DS18B20
  AGY_DRIVER_I2C          // Sensor I2C Cerdas (Auto-Detect)
};

inline const char* agyDriverTypeToString(AgyDriverType d) {
  switch (d) {
    case AGY_DRIVER_SWITCH:     return "switch";
    case AGY_DRIVER_DIGITAL_IN: return "digital_in";
    case AGY_DRIVER_ANALOG:     return "analog";
    case AGY_DRIVER_DHT11:      return "dht11";
    case AGY_DRIVER_DHT22:      return "dht22";
    case AGY_DRIVER_DS18B20:    return "ds18b20";
    case AGY_DRIVER_I2C:        return "i2c";
    default:                    return "custom";
  }
}

inline AgyDriverType agyStringToDriverType(const String& str) {
  if (str == "switch" || str == "relay")     return AGY_DRIVER_SWITCH;
  if (str == "digital_in" || str == "pir" || str == "button" || str == "indicator") return AGY_DRIVER_DIGITAL_IN;
  if (str == "analog" || str == "adc")       return AGY_DRIVER_ANALOG;
  if (str == "dht11")                        return AGY_DRIVER_DHT11;
  if (str == "dht22" || str == "am2302")     return AGY_DRIVER_DHT22;
  if (str == "ds18b20" || str == "onewire")  return AGY_DRIVER_DS18B20;
  if (str == "i2c")                          return AGY_DRIVER_I2C;
  return AGY_DRIVER_CUSTOM;
}

// Timer countdown lokal untuk aktuator (berjalan mandiri di chip)
struct AgyCountdownTimer {
  bool active = false;
  unsigned long endTimeMs = 0;
  bool targetState = false;
  unsigned long durationMs = 0;
  unsigned long startMs = 0;
};

// Definisi Komponen Universal
struct AgyComponent {
  String id;                          // e.g. "relay_1", "temp", "V1"
  String name;                        // e.g. "Lampu Kamar"
  AgyComponentType type = AGY_SENSOR;
  AgyDriverType driverType = AGY_DRIVER_CUSTOM;
  String unit;                        // e.g. "°C", "%", "Lux"
  String access = "rw";               // "r", "rw"
  String value = "0";                 // Nilai saat ini ("true", "false", "28.5")
  int pin = -1;                       // Pin GPIO fisik (-1 jika virtual)
  bool activeLow = true;              // Khusus relay (active LOW)
  bool pullup = true;                 // Khusus digital input (INPUT_PULLUP)
  int lastDigitalVal = -1;            // Deteksi perubahan input fisik
  bool isChanged = false;             // Flag apakah nilai berubah & butuh dikirim ke server
  bool isDynamic = false;             // Komponen dikonfigurasi secara dinamis via Web UI

  // Auto sensor reader (jika diset manual via C++)
  std::function<float()> readSensorFn = nullptr;
  unsigned long readIntervalMs = 5000;
  unsigned long lastReadMs = 0;

  // Countdown timer untuk aktuator
  AgyCountdownTimer timer;
};

// Definisi Callback
typedef std::function<void(const String& compId, const String& value)> AgyCommandCallback;
typedef std::function<void(const String& vPin, const String& value)> AgyVirtualWriteCallback;
typedef std::function<void(bool isConnected)> AgyConnectionCallback;

#endif // AGY_TYPES_H
