/*
 * I2C Scanner for ESP32-WROOM
 * Serial Monitor: 115200 baud.
 * Expected: 0x68 (MPU6050), 0x77 (BMP085/BMP180)
 *
 * Wiring: BOTH sensors share the same 4 wires:
 *   VCC -> 3V3 (not 5V/VIN)   GND -> GND
 *   SDA -> GPIO 21            SCL -> GPIO 22
 * If no devices found, try swapping SDA and SCL.
 */

#include <Wire.h>

#if defined(ESP32)
  #define SDA_PIN 21
  #define SCL_PIN 22
#endif

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- I2C Scanner (ESP32-WROOM) ---");

#if defined(ESP32)
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Serial.println("SDA=GPIO21, SCL=GPIO22, 100 kHz");
  Serial.println("VCC->3V3, GND->GND for both sensors.");
  delay(150);
#else
  Wire.begin();
  Wire.setClock(100000);
  Serial.println("Using default I2C pins");
  delay(150);
#endif

  Serial.println("Scanning...\n");
}

void loop() {
  int n = 0;

  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    delay(2);

    if (err == 0) {
      Serial.print("  Device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      if (addr == 0x68) Serial.print(" (MPU6050)");
      else if (addr == 0x77 || addr == 0x76) Serial.print(" (BMP085/BMP180)");
      Serial.println();
      n++;
    }
  }

  if (n == 0) {
    Serial.println("  No I2C devices found.");
    Serial.println("  -> Try ONLY one sensor. Swap SDA/SCL if still none.");
  } else {
    Serial.println("  Done.");
  }

  Serial.println();
  delay(4000);
}
