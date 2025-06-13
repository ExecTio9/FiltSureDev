#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <RH_RF95.h>
#include <esp32-hal-rgb-led.h>
#include <driver/usb_serial_jtag.h>
#include <esp_sleep.h>

#define BATTERY_PIN       0
#define DIAG_PIN          2
#define WIND_PIN          3
#define SCK               4
#define MOSI              5
#define REGULATOR_EN_PIN  7
#define RGB_BUILTIN       8
#define MISO              15
#define BME_CS            23
#define RFID_CS           22
#define LORA_CS           21
#define LORA_RST          20
#define LORA_IRQ          18

#define RF95_FREQ 915.0
#define us_S 1000000ULL
#define sleep_Time 600
#define LORA_NODE_ID "NODE_01"  // Manually assigned LoRa ID

Adafruit_BME280 bme(BME_CS);
MFRC522DriverPinSimple rfidPin(RFID_CS);
MFRC522DriverSPI rfidDriver(rfidPin);
MFRC522 mfrc522(rfidDriver);
RH_RF95 radio(LORA_CS, LORA_IRQ);

RTC_DATA_ATTR int bootCount = 0;

String ID = "01.19";
String Web_App_Path = "/macros/s/AKfycbzyinhl792ozO-Szi3PwUx6tGTd164YckAYTUUvRkZu2g-h25iGzGqcjx3fGKGF6NEs/exec";
String DataURL = "", Status_Read_Sensor = "", rfid = "";
float Temp, Humd, Prs;
float batteryVoltage = 0.0, batteryPercent = 0.0, windSpeed = 0.0;

volatile bool diagRequested = false;

float getBatteryPercentage(float voltage) {
  voltage = constrain(voltage, 2.0, 3.0);
  return (voltage - 2.0) * 100.0;
}

void flashRGB(uint8_t r, uint8_t g, uint8_t b, int count = 1, int delayMs = 300) {
  for (int i = 0; i < count; i++) {
    rgbLedWrite(RGB_BUILTIN, r, g, b);
    delay(delayMs);
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    delay(delayMs);
  }
}

void connectToHotspot() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin("ESP32_HOTSPOT", "FiltSure_Rules");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(500);
}

void IRAM_ATTR onDiagButton() {
  diagRequested = true;
}

void runDiagnostics(bool bmeOK, bool rfidOK, float battPct, bool wifiOK, bool loraOK) {
  flashRGB(255, 255, 255, 2, 150);
  if (bmeOK && rfidOK) flashRGB(0, 255, 0);
  else if (bmeOK) flashRGB(255, 255, 0);
  else if (rfidOK) flashRGB(0, 0, 255);
  else flashRGB(255, 0, 0);

  if (battPct >= 75) flashRGB(0, 255, 0);
  else if (battPct >= 40) flashRGB(255, 255, 0);
  else flashRGB(255, 0, 0);

  if (wifiOK && loraOK) flashRGB(0, 255, 0);
  else if (wifiOK) flashRGB(255, 255, 0);
  else if (loraOK) flashRGB(0, 0, 255);
  else flashRGB(255, 0, 0);
  diagRequested = false;

}

void checkDiagnosticsMode(bool bmeOK, bool rfidOK, float battPct, bool wifiOK, bool loraOK) {
  pinMode(DIAG_PIN, INPUT_PULLUP);
  if (usb_serial_jtag_is_connected()) {
    runDiagnostics(bmeOK, rfidOK, battPct, wifiOK, loraOK);
    delay(2000);
  } else if (digitalRead(DIAG_PIN) == LOW) {
    delay(2000);
    if (digitalRead(DIAG_PIN) == LOW) {
      runDiagnostics(bmeOK, rfidOK, battPct, wifiOK, loraOK);
    }
  }
}

void prepareRegulatorPin() {
  pinMode(REGULATOR_EN_PIN, OUTPUT);
  digitalWrite(REGULATOR_EN_PIN, HIGH);
  gpio_hold_dis((gpio_num_t)REGULATOR_EN_PIN);
  gpio_hold_en((gpio_num_t)REGULATOR_EN_PIN);
}

String urlEncode(const String& s) {
  String encoded = "";
  const char* hex = "0123456789ABCDEF";
  for (char c : s) {
    if (isalnum(c)) encoded += c;
    else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0xF];
      encoded += hex[c & 0xF];
    }
  }
  return encoded;
}

void buildURL(bool diagnosticMode = false) {
  DataURL = Web_App_Path;
  DataURL += diagRequested ? "?sts=diag" : "?sts=write";
  DataURL += "&id=" + ID;
  DataURL += "&bc=" + String(bootCount);
  DataURL += "&bat=" + String(batteryPercent, 2);
  DataURL += "&srs=" + Status_Read_Sensor;
  DataURL += "&temp=" + String(Temp);
  DataURL += "&humd=" + String(Humd);
  DataURL += "&Prs=" + String(Prs);
  DataURL += "&wind=" + String(windSpeed, 2);
  DataURL += "&rfid=" + rfid;
  DataURL += "&lnid=" + String(LORA_NODE_ID);  // LoRa Node ID
}

void readSensors() {
  digitalWrite(BME_CS, LOW);
  Temp = bme.readTemperature();
  Humd = bme.readHumidity();
  Prs  = bme.readPressure() / 100.0F;
  digitalWrite(BME_CS, HIGH);
  Status_Read_Sensor = (isnan(Temp) || isnan(Humd) || isnan(Prs)) ? "Failed" : "Success";

  digitalWrite(RFID_CS, LOW);
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    rfid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++)
      rfid += String(mfrc522.uid.uidByte[i], HEX);
  }
  digitalWrite(RFID_CS, HIGH);
}

void HTTP_send() {
  HTTPClient http;
  String url = "http://192.168.4.1/data?url=" + urlEncode(DataURL);
  http.begin(url);
  http.GET();
  http.end();
}

bool LoRa_sendAndVerify() {
  digitalWrite(LORA_RST, LOW);
  delay(10);
  digitalWrite(LORA_RST, HIGH);
  delay(10);
  if (!radio.init()) {
    Serial.println("❌ LoRa init failed");
    return false;
  }

  radio.setFrequency(RF95_FREQ);
  radio.setTxPower(20, false);
  radio.send((uint8_t *)DataURL.c_str(), DataURL.length());
  radio.waitPacketSent();

  // Wait for ACK
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (radio.waitAvailableTimeout(1000)) {
    if (radio.recv(buf, &len)) {
      Serial.print("✅ ACK received: ");
      Serial.println((char*)buf);
      return true;
    } else {
      Serial.println("⚠️ ACK receive failed");
      return false;
    }
  } else {
    Serial.println("⚠️ No ACK received");
    return false;
  }
}

bool LoRa_pingAndWaitForPong() {
  if (!radio.init()) {
    Serial.println("❌ LoRa init failed");
    return false;
  }

  radio.setFrequency(RF95_FREQ);
  radio.setTxPower(20, false);
  radio.send((uint8_t *)"ping", 4);
  radio.waitPacketSent();

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (radio.waitAvailableTimeout(1000)) {
    if (radio.recv(buf, &len)) {
      String reply = String((char*)buf).substring(0, len);
      if (reply == "pong") {
        Serial.println("✅ Pong received from LoRa");
        return true;
      }
    }
  }

  Serial.println("⚠️ Pong not received");
  return false;
}

void setup() {
  // ---------- Serial + Boot Count ----------
  Serial.begin(115200);
  delay(200);
  ++bootCount;
  Serial.printf("🔄 Boot #%d\n", bootCount);

  // ---------- GPIO Setup ----------
  pinMode(BME_CS, OUTPUT);    digitalWrite(BME_CS, HIGH);
  pinMode(RFID_CS, OUTPUT);   digitalWrite(RFID_CS, HIGH);
  pinMode(LORA_CS, OUTPUT);   digitalWrite(LORA_CS, HIGH);
  pinMode(LORA_RST, OUTPUT);  digitalWrite(LORA_RST, HIGH);
  pinMode(DIAG_PIN, INPUT_PULLUP); // GPIO2 = External diagnostics trigger
  pinMode(REGULATOR_EN_PIN, OUTPUT); digitalWrite(REGULATOR_EN_PIN, HIGH);

  // ---------- Retain Regulator During Sleep ----------
  gpio_hold_dis((gpio_num_t)REGULATOR_EN_PIN);
  gpio_hold_en((gpio_num_t)REGULATOR_EN_PIN);

  // ---------- SPI Init ----------
  SPI.begin(SCK, MISO, MOSI);

  // ---------- Sensor Init ----------
  if (!bme.begin()) Serial.println("❌ BME280 init failed");
  else              Serial.println("✅ BME280 ready");

  mfrc522.PCD_Init();
  Serial.println("✅ MFRC522 ready");

  // ---------- LoRa Init ----------
  Serial.println("🔁 Resetting LoRa module...");
  digitalWrite(LORA_RST, LOW); delay(10);
  digitalWrite(LORA_RST, HIGH); delay(100);

  digitalWrite(BME_CS, HIGH);   // Prevent SPI contention
  digitalWrite(RFID_CS, HIGH);

  bool loraOK = false;
  if (radio.init()) {
    radio.setFrequency(RF95_FREQ);
    radio.setTxPower(20, false);
    Serial.println("✅ LoRa radio initialized.");
    loraOK = true;
  } else {
    Serial.println("❌ LoRa radio init failed.");
  }

  // ---------- WiFi Init ----------
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin("ESP32_HOTSPOT", "FiltSure_Rules");

  bool wifiOK = false;
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    delay(500);
    Serial.print(".");
  }
  wifiOK = (WiFi.status() == WL_CONNECTED);
  Serial.println(wifiOK ? "\n✅ WiFi connected" : "\n❌ WiFi failed");

  // ---------- Read Sensors ----------
  batteryVoltage = analogRead(BATTERY_PIN) * (5.0 / 4095.0) * 2.0;
  batteryPercent = getBatteryPercentage(batteryVoltage);
  windSpeed = analogRead(WIND_PIN) * (5.0 / 4095.0);
  readSensors();

  bool isDiagnostic = diagRequested || usb_serial_jtag_is_connected() || digitalRead(DIAG_PIN) == LOW;
  buildURL(isDiagnostic);
  Serial.println("📛 LoRa Node ID: " + String(LORA_NODE_ID));

  if (diagRequested) {
    Status_Read_Sensor = "diag";
    loraOK = LoRa_pingAndWaitForPong();  // try to verify LoRa
    diagRequested = false;  // <- RESET FLAG IMMEDIATELY AFTER USE
  }

  // ---------- Diagnostics Check ----------
  bool bmeOK = !isnan(Temp);
  bool rfidOK = !rfid.isEmpty();
  checkDiagnosticsMode(bmeOK, rfidOK, batteryPercent, wifiOK, loraOK);

  // ---------- Data Upload ----------
  if (wifiOK) {
    HTTP_send();
  } else if (loraOK) {
    LoRa_sendAndVerify();  // fallback
  }

  // ---------- Wake Sources ----------
  esp_sleep_enable_ext1_wakeup(1ULL << DIAG_PIN, ESP_EXT1_WAKEUP_ALL_LOW);  // External button
  esp_sleep_enable_timer_wakeup(sleep_Time * us_S);                         // Timer wake

  // ---------- Sleep ----------
  prepareRegulatorPin();
  Serial.println("💤 Going to deep sleep...");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {}