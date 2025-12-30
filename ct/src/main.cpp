#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID   "TMPL6zMXMta2b"
#define BLYNK_TEMPLATE_NAME "Contracker"
#define BLYNK_AUTH_TOKEN    "PnmkZq1xinsGW7JOd7PFW7GNFu_qg0Kb"

// ================= BLYNK VPIN =================
#define VPIN_MAP       V0
#define VPIN_STATUS    V1
#define VPIN_LAT       V3
#define VPIN_LON       V4
#define VPIN_MOTION    V5
#define VPIN_AX        V6
#define VPIN_AY        V7
#define VPIN_AZ        V8

// ================= TINYGSM =================
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

#include <Arduino.h>
#include <Wire.h>
#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ================= APN =================
const char apn[]  = "internet";   
const char user[] = "";
const char pass[] = "";

// ================= SIM7080G =================
#define SIM_RX  32
#define SIM_TX  33
#define PWRKEY  4
HardwareSerial sim7080(1);
TinyGsm modem(sim7080);

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= MPU6050 =================
Adafruit_MPU6050 mpu;
bool isMoving = false;
float accelThreshold = 1.5;

// ================= GPS =================
float latitude = 0.0;
float longitude = 0.0;

// ================= STATUS =================
enum GpsStatus { GPS_INIT, GPS_SEARCHING, GPS_FIX };
GpsStatus gpsStatus = GPS_INIT;

// ================= OLED =================
void oledStatus(const char* l1, const char* l2 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(l1);
  if (strlen(l2)) {
    display.setCursor(0, 35);
    display.println(l2);
  }
  display.display();
}

void oledGPSFix() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("LAT: ");
  display.println(latitude, 6);

  display.setCursor(0, 12);
  display.print("LON: ");
  display.println(longitude, 6);

  display.drawLine(0, 26, 128, 26, SSD1306_WHITE);

  display.setCursor(0, 32);
  display.println("GPS FIX");

  display.setCursor(80, 48);
  display.println(isMoving ? "MOVE" : "STOP");

  display.display();
}

void updateOLED() {
  if (gpsStatus == GPS_FIX) oledGPSFix();
  else if (gpsStatus == GPS_SEARCHING) oledStatus("GPS SEARCHING");
  else oledStatus("SIM STARTING");
}

// ================= MPU6050 =================
void readAccelerometer() {
  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  float mag = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );

  isMoving = abs(mag - 9.8) > accelThreshold;

  Blynk.virtualWrite(VPIN_AX, a.acceleration.x);
  Blynk.virtualWrite(VPIN_AY, a.acceleration.y);
  Blynk.virtualWrite(VPIN_AZ, a.acceleration.z);
  Blynk.virtualWrite(VPIN_MOTION, isMoving ? "MOVING" : "STOP");
}

// ================= SIM POWER =================
void powerOnSIM() {
  pinMode(PWRKEY, OUTPUT);
  digitalWrite(PWRKEY, HIGH);
  delay(3000);
  digitalWrite(PWRKEY, LOW);
  delay(15000);
}

// ================= GPS PARSER =================
bool readGPS(float &lat, float &lon) {
  sim7080.println("AT+CGNSINF");
  delay(300);

  while (sim7080.available()) {
    String line = sim7080.readStringUntil('\n');
    line.trim();

    if (line.startsWith("+CGNSINF:")) {
      String f[10];
      int idx = 0, last = 0;

      for (int i = 0; i < line.length() && idx < 10; i++) {
        if (line.charAt(i) == ',') {
          f[idx++] = line.substring(last, i);
          last = i + 1;
        }
      }
      f[idx] = line.substring(last);

      if (f[1] == "1") {
        lat = f[3].toFloat();
        lon = f[4].toFloat();
        return true;
      }
    }
  }
  return false;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  mpu.begin();

  updateOLED();

  sim7080.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  powerOnSIM();

  modem.restart();
  modem.gprsConnect(apn, user, pass);

  modem.sendAT("+CGNSPWR=1");
  delay(1000);

  Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);

  gpsStatus = GPS_SEARCHING;
  updateOLED();
}

// ================= LOOP =================
void loop() {
  Blynk.run();

  if (readGPS(latitude, longitude)) {
    gpsStatus = GPS_FIX;
    Blynk.virtualWrite(VPIN_MAP, latitude, longitude);
    Blynk.virtualWrite(VPIN_LAT, latitude);
    Blynk.virtualWrite(VPIN_LON, longitude);
    Blynk.virtualWrite(VPIN_STATUS, "GPS FIX");
  } else {
    gpsStatus = GPS_SEARCHING;
    Blynk.virtualWrite(VPIN_STATUS, "GPS SEARCHING");
  }

  readAccelerometer();
  updateOLED();

  delay(2000);
}
