#define BLYNK_PRINT Serial
#define TINY_GSM_MODEM_SIM7080

#define BLYNK_TEMPLATE_ID "TMPL6zMXMta2b"
#define BLYNK_TEMPLATE_NAME "Contracker"
#define BLYNK_AUTH_TOKEN "PnmkZq1xinsGW7JOd7PFW7GNFu_qg0Kb"

#define VPIN_MAP     V0
#define VPIN_STATUS  V1
#define VPIN_LAT     V3
#define VPIN_LON     V4

#include <Arduino.h>
#include <Wire.h>
#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>
#include <Adafruit_SSD1306.h>

// ================= SIM7080 =================
#define SIM_RX  32
#define SIM_TX  33
#define PWRKEY  4
HardwareSerial sim7080(1);
TinyGsm modem(sim7080);
TinyGsmClient client(modem);

// ================= APN =================
const char apn[]  = "internet";   // SESUAI PROVIDER
const char user[] = "";
const char pass[] = "";

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ================= GPS =================
float latitude = 0.0;
float longitude = 0.0;
bool gpsFix = false;

// ================= SIM POWER =================
void powerOnSIM() {
  pinMode(PWRKEY, OUTPUT);
  digitalWrite(PWRKEY, HIGH);
  delay(5000);
  digitalWrite(PWRKEY, LOW);
  delay(20000);
  digitalWrite(PWRKEY, HIGH);
  delay(10000);
}

// ================= GPS PARSER =================
bool parseCGNSINF(String line, float &lat, float &lon, bool &fix) {
  if (!line.startsWith("+CGNSINF:")) return false;

  line.replace("+CGNSINF: ", "");
  String field[10];
  int idx = 0, last = 0;

  for (int i = 0; i < line.length(); i++) {
    if (line.charAt(i) == ',') {
      field[idx++] = line.substring(last, i);
      last = i + 1;
    }
  }
  field[idx] = line.substring(last);

  fix = (field[1] == "1");
  lat = field[3].toFloat();
  lon = field[4].toFloat();
  return true;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(3000);

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  sim7080.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  powerOnSIM();

  Serial.println("Initializing modem...");
  modem.restart();

  Serial.println("Connecting to cellular...");
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println("GPRS FAILED");
    while (1);
  }
  Serial.println("Cellular connected");

  // 🔒 GPS ONLY
  sim7080.println("AT+CGNSCFG=1,1");
  delay(1000);
  sim7080.println("AT+CGNSPWR=1");
  delay(1000);

  // 🔗 Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);
}

// ================= LOOP =================
void loop() {
  Blynk.run();

  sim7080.println("AT+CGNSINF");
  delay(500);

  while (sim7080.available()) {
    String gpsLine = sim7080.readStringUntil('\n');
    gpsLine.trim();

    float latTmp, lonTmp;
    bool fixTmp;

    if (parseCGNSINF(gpsLine, latTmp, lonTmp, fixTmp)) {
      if (fixTmp &&
          latTmp >= -90 && latTmp <= 90 &&
          lonTmp >= -180 && lonTmp <= 180) {

        latitude = latTmp;
        longitude = lonTmp;

        Blynk.virtualWrite(VPIN_MAP, latitude, longitude);
        Blynk.virtualWrite(VPIN_LAT, latitude);
        Blynk.virtualWrite(VPIN_LON, longitude);
        Blynk.virtualWrite(VPIN_STATUS, "GPS FIX");

        Serial.print("LAT: "); Serial.println(latitude, 6);
        Serial.print("LON: "); Serial.println(longitude, 6);
      } else {
        Blynk.virtualWrite(VPIN_STATUS, "SEARCHING");
      }
    }
  }

  delay(2000);
}
