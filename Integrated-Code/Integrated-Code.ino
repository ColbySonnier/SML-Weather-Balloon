/*
 * SML-Weather-Balloon-Project Integrated
 * **Modular structure note: keep init and read/print in separate functions.
 */

#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <RadioLib.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <Adafruit_HTU21DF.h>

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
// output frequency: 438
// spread factor: 9
// band width: 125

#define SERIAL_BAUD     115200
#define I2C_BUS_HZ      100000
#define LOOP_DELAY_MS   100
#define I2C_SETTLE_MS   50

#if defined(ESP32)
  #define SDA_PIN       21
  #define SCL_PIN       22
#endif

// Thermistor config
#define THERM_PIN           32
#define THERM_VCC           3.3
#define THERM_R_FIXED       10000
#define THERM_R0            10000
#define THERM_T0            (25 + 273.15)
#define THERM_BETA          3950
#define THERM_ADC_SAMPLES   10

// Radio config
#define RFM96_CS    5   
#define RFM96_G0    2   
#define RFM96_RST   14  

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

Adafruit_BMP085   bmp;
Adafruit_MPU6050  mpu;
RFM96 radio = new Module(RFM96_CS, RFM96_G0, RFM96_RST, RADIOLIB_NC);
SFE_UBLOX_GNSS myGNSS;
Adafruit_HTU21DF htu = Adafruit_HTU21DF();

bool bmpOK = false;
bool mpuOK = false;
bool thermOK = false;

float bmp_Temp = 0;
float htu_Temp = 0;
float humidity = 0;
int pressure = 0;
float altitude = 0;
double latitude = 0;
double longitude = 0;
float Acc_x = 0;
float Acc_y = 0;
float Acc_z = 0;
float Rot_x = 0;
float Rot_y = 0;
float Rot_z = 0;
float imu_Temp = 0;
float therm_Temp = 0;


// -----------------------------------------------------------------------------
// Init: Radio
// -----------------------------------------------------------------------------
void initRadio(){
    int state = radio.begin(434.0);
}

// -----------------------------------------------------------------------------
// Init: Serial
// -----------------------------------------------------------------------------

void initSerial() {
  delay(2000);
  Serial.begin(SERIAL_BAUD);
  Serial.println("SML-Weather-Balloon-Project integrated - " + String(SERIAL_BAUD) + " baud");
}

// -----------------------------------------------------------------------------
// Init: I2C bus (shared by both sensors)
// -----------------------------------------------------------------------------

void initI2C() {
#if defined(ESP32)
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_BUS_HZ);
  Wire.setTimeOut(1000);
#else
  Wire.begin();
#endif
}

// -----------------------------------------------------------------------------
// Init: Pressure sensor (BMP085/BMP180)
// -----------------------------------------------------------------------------

void initPressureSensor() {
  if (!bmp.begin()) {
    Serial.println("BMP085/BMP180: not found (check wiring).");
    bmpOK = false;
    return;
  }
  Serial.println("BMP180 found!");
  bmpOK = true;
  delay(150);
}

// -----------------------------------------------------------------------------
// Init: IMU (MPU6050) and its config
// -----------------------------------------------------------------------------

void initIMUSensor() {
  if (!mpu.begin()) {
    Serial.println("MPU6050: not found (check wiring).");
    mpuOK = false;
    return;
  }
  Serial.println("MPU6050 found!");
  mpuOK = true;
  delay(100);
}

void configureIMU() {
  if (!mpuOK) return;
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

// -----------------------------------------------------------------------------
// Init: Thermistor (NTC on ADC)
// -----------------------------------------------------------------------------

void initThermistor() {
#if defined(ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(THERM_PIN, ADC_11db);
  Serial.println("Thermistor initialized on GPIO" + String(THERM_PIN));
  thermOK = true;
#else
  Serial.println("Thermistor: ESP32 only.");
  thermOK = false;
#endif
}

void initHumidity(){
  if (!htu.begin()) {
    Serial.println("Couldn't find sensor!");
    while (1);
  }
}

// -----------------------------------------------------------------------------
// Read and print: Pressure
// -----------------------------------------------------------------------------

void readPressure() {
  bmp_Temp = bmp.readTemperature();
  pressure = bmp.readPressure();
  altitude = bmp.readAltitude();
}

// -----------------------------------------------------------------------------
// Read and print: IMU (accel, gyro, temp)
// -----------------------------------------------------------------------------

void readIMU() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  Acc_x = a.acceleration.x;
  Acc_y = a.acceleration.y;
  Acc_z = a.acceleration.z;
  Rot_x = g.gyro.x;
  Rot_y = g.gyro.y;
  Rot_z = g.gyro.z;
  imu_Temp = temp.temperature;
}

// -----------------------------------------------------------------------------
// Read: Thermistor (NTC)
// -----------------------------------------------------------------------------

void readThermistor() {
  if (!thermOK) return;

  long sum = 0;
  for (int i = 0; i < THERM_ADC_SAMPLES; i++) {
    sum += analogRead(THERM_PIN);
    delay(5);
  }
  int therm_Raw = sum / THERM_ADC_SAMPLES;

  float therm_Voltage = (therm_Raw / 4095.0) * THERM_VCC;
  float therm_Resistance = THERM_R_FIXED * (therm_Voltage / (THERM_VCC - therm_Voltage));

  float invT = (1.0 / THERM_T0) + (1.0 / THERM_BETA) * log(therm_Resistance / THERM_R0);
  float tempK = 1.0 / invT;
  therm_Temp = tempK - 273.15;
}

void readHumidity(){
  htu_Temp = htu.readTemperature();
  humidity = htu.readHumidity();
}

void initGPS() {
  if (myGNSS.begin(Wire,0x42,1100,false) == false)
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring."));
  }

  myGNSS.setNavigationFrequency(10); // Update rate: 2 Hz
}

void readGps(){  // Position data
  latitude = myGNSS.getLatitude()/1e7;    // degrees * 1e-7
  longitude = myGNSS.getLongitude()/1e7;  // degrees * 1e-7
  // altitude = myGNSS.getAltitude();    // millimeters
  byte sats = myGNSS.getSIV();             // satellites in view
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------

void setup() {
  initSerial();
  initI2C();
  initPressureSensor();
  initHumidity();
  initIMUSensor();
  configureIMU();
  initThermistor();
  initGPS();
  initRadio();
  printData();
  delay(100);
}

void loop() {
  readGps();
  if (bmpOK) {
    readPressure();
    delay(I2C_SETTLE_MS);
  }

  if (mpuOK) {
    readIMU();
  }

  if (thermOK) {
    readThermistor();
  }

  if (!bmpOK && !mpuOK && !thermOK) {
    Serial.println("No sensors found. Check wiring.");
  }

  readHumidity();

  Serial.println("");
  printData();
  String outputData = generateOutputData();
  int state = radio.transmit(outputData);
  delay(LOOP_DELAY_MS);
}

double getAverageTemp(){
  return (bmp_Temp+imu_Temp+therm_Temp+htu_Temp)/4;
}

void printData() {
  Serial.println("+--------------------+---------------+");
  Serial.println("| FIELD              | VALUE         |");
  Serial.println("+--------------------+---------------+");
  Serial.printf( "| BMP Temp           | %-13s |\n", (String(bmp_Temp, 2)        + " C").c_str());
  Serial.printf( "| IMU Temp           | %-13s |\n", (String(imu_Temp, 2)        + " C").c_str());
  Serial.printf( "| Therm Temp         | %-13s |\n", (String(therm_Temp, 2)      + " C").c_str());
  Serial.printf( "| Avg Temp           | %-13s |\n", (String(getAverageTemp(), 2)+ " C").c_str());
  Serial.println("+--------------------+---------------+");
  Serial.printf( "| Humidity           | %-13s |\n", (String(humidity, 2)        + " %").c_str());
  Serial.printf( "| Pressure           | %-13s |\n", (String(pressure)           + " hPa").c_str());
  Serial.printf( "| Altitude           | %-13s |\n", (String(altitude, 2)        + " m").c_str());
  Serial.println("+--------------------+---------------+");
  Serial.printf( "| Latitude           | %-13s |\n", String(latitude, 7).c_str());
  Serial.printf( "| Longitude          | %-13s |\n", String(longitude, 7).c_str());
  Serial.println("+--------------------+---------------+");
  Serial.printf( "| Accel X            | %-13s |\n", (String(Acc_x, 3)           + " m/s2").c_str());
  Serial.printf( "| Accel Y            | %-13s |\n", (String(Acc_y, 3)           + " m/s2").c_str());
  Serial.printf( "| Accel Z            | %-13s |\n", (String(Acc_z, 3)           + " m/s2").c_str());
  Serial.println("+--------------------+---------------+");
  Serial.printf( "| Rot X              | %-13s |\n", (String(Rot_x, 3)           + " d/s").c_str());
  Serial.printf( "| Rot Y              | %-13s |\n", (String(Rot_y, 3)           + " d/s").c_str());
  Serial.printf( "| Rot Z              | %-13s |\n", (String(Rot_z, 3)           + " d/s").c_str());
  Serial.println("+--------------------+---------------+");
}

String generateOutputData() {
  String rtn = "";
  rtn += "+--------------------+---------------+\n";
  rtn += "| FIELD              | VALUE         |\n";
  rtn += "+--------------------+---------------+\n";
  rtn += "| Temp               | " + padRight(String(getAverageTemp(), 2) + " C",   13) + " |\n";
  rtn += "| Humidity           | " + padRight(String(humidity, 2)        + " %",    13) + " |\n";
  rtn += "| Pressure           | " + padRight(String(pressure)           + " hPa",  13) + " |\n";
  rtn += "| Altitude           | " + padRight(String(altitude, 2)        + " m",    13) + " |\n";
  rtn += "+--------------------+---------------+\n";
  rtn += "| Latitude           | " + padRight(String(latitude, 7),                  13) + " |\n";
  rtn += "| Longitude          | " + padRight(String(longitude, 7),                 13) + " |\n";
  rtn += "+--------------------+---------------+\n";
  rtn += "| Accel X            | " + padRight(String(Acc_x, 3)           + " m/s2", 13) + " |\n";
  rtn += "| Accel Y            | " + padRight(String(Acc_y, 3)           + " m/s2", 13) + " |\n";
  rtn += "| Accel Z            | " + padRight(String(Acc_z, 3)           + " m/s2", 13) + " |\n";
  rtn += "+--------------------+---------------+\n";
  rtn += "| Rot X              | " + padRight(String(Rot_x, 3)           + " d/s",  13) + " |\n";
  rtn += "| Rot Y              | " + padRight(String(Rot_y, 3)           + " d/s",  13) + " |\n";
  rtn += "| Rot Z              | " + padRight(String(Rot_z, 3)           + " d/s",  13) + " |\n";
  rtn += "+--------------------+---------------+\n";
  return rtn;
}

String padRight(String s, int width) {
  while (s.length() < width) s += " ";
  return s;
}