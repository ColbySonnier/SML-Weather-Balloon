/*
  RadioLib RFM96 Transmit Example for ESP32
  
  Wiring (Same as RFM95):
  RFM96    ESP32
  ------   -----
  VIN   -> 3.3V
  GND   -> GND
  SCK   -> GPIO 18
  MISO  -> GPIO 19
  MOSI  -> GPIO 23
  CS    -> GPIO 5
  RST   -> GPIO 14
  G0    -> GPIO 2
*/

#include <RadioLib.h>

// RFM96 pin definitions
#define RFM96_CS    5   
#define RFM96_G0    2   
#define RFM96_RST   14  

// 1. Changed class from SX1278 to RFM96
RFM96 radio = new Module(RFM96_CS, RFM96_G0, RFM96_RST, RADIOLIB_NC);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.print(F("[RFM96] Initializing ... "));
  
  // 2. Explicitly set the frequency to 434.0 MHz (Standard for RFM96)
  // begin() also sets bandwidth, spreading factor, and coding rate to defaults
  int state = radio.begin(434.0);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }
}

int count = 0;

void loop() {
  Serial.print(F("[RFM96] Transmitting packet ... "));

  String str = "Hello from RFM96! #" + String(count++);
  int state = radio.transmit(str);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
    Serial.print(F("[RFM96] Datarate:\t"));
    Serial.print(radio.getDataRate());
    Serial.println(F(" bps"));

  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    Serial.println(F("too long!"));
  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    Serial.println(F("timeout!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  delay(2000); // Increased delay slightly for stability
}