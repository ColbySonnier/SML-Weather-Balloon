const int THERM_PIN = 32;      // ADC1 pin
const float VCC = 3.3;         // ESP32 supply voltage
const float R_FIXED = 10000;   // 10k series resistor

// 10k NTC thermistor constants
const float THERMISTOR_R0 = 10000;        // 10k at 25°C
const float THERMISTOR_T0 = 25 + 273.15;  // 25°C in Kelvin
const float THERMISTOR_BETA = 3950;       // Beta constant

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);                   
  analogSetPinAttenuation(THERM_PIN, ADC_11db);
}

void loop() {
  int samples = 10;
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(THERM_PIN);
    delay(5);
  }
  int raw = sum / samples;

  float voltage = (raw / 4095.0) * VCC;

  float rTherm = R_FIXED * (voltage / (VCC - voltage));

  float invT = (1.0 / THERMISTOR_T0) + (1.0 / THERMISTOR_BETA) * log(rTherm / THERMISTOR_R0);
  float tempK = 1.0 / invT;
  float tempC = tempK - 273.15;
  float tempF = tempC * 9.0 / 5.0 + 32.0;

  Serial.print("Raw: "); Serial.print(raw);
  Serial.print("  V: "); Serial.print(voltage, 3);
  Serial.print("  R: "); Serial.print(rTherm, 0);
  Serial.print(" ohm   Temp: ");
  Serial.print(tempC, 2); Serial.print(" C  ");
  Serial.print(tempF, 2); Serial.println(" F");

  delay(500);
}