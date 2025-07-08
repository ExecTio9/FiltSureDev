#include <WiFi.h>
#include <WiFiManager.h>
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

// ---------- Pin Definitions ----------
#define BATTERY_PIN       0   // Battery Level Pin (ADC-in)
#define DIAG_PIN          2   // Diagnostic Pin
#define WIND_PIN          3   // Wind Speed Pin
#define SCK               4   // SPI serial Clock Pin
#define MOSI              5   // SPI Master-Out-Slave-In Pin
#define REGULATOR_EN_PIN  7   // 5v Regulator Pin
#define RGB_BUILTIN       8   // Built-in RGB LED Pin
#define MISO              15  // SPI Master-In-Slave-Out Pin
#define BME_CS            23  // BME Chip Select Pin
#define RFID_CS           22  // RFID Chip Select Pin
#define LORA_CS           21  // LoRa Chip Select Pin
#define LORA_RST          20  // LoRa Chip Reset Pin
#define LORA_IRQ          18  // LoRa Chip Interrupt Pin
#define us_S              1000000ULL  // Clock Speed Reference
#define sleep_Time        60  // Sleep Time

// ---------- Hardware Object Setup ----------
Adafruit_BME280 bme(BME_CS);
MFRC522DriverPinSimple rfidPin(RFID_CS);
MFRC522DriverSPI rfidDriver(rfidPin);
MFRC522 mfrc522(rfidDriver);
RH_RF95 radio(LORA_CS, LORA_IRQ);

RTC_DATA_ATTR int bootCount = 0;  // DMA counter for booting held during sleep

// ---------- Global State Variables ----------

/*
Node info & Data variable initiation

ID = Device ID hh.hh
Web_App_Path = Specific spreadsheet ID for autoprocessing and upload to Google Sheets
DataURL = Same as Web_App_Path
Status_Read_Sensor = Boolean status for if the sensors were readable  
rfid = Optional RFID tag UID
Temp = Temperature in C
Humd = Relative humidity as a percentage
Prs = Pressure in hPa
batteryVoltage 
batteryPercent
windSpeed = Speed of wind in m/s
*/

String ID = "01.06";
String Web_App_Path = "/macros/s/AKfycbzyinhl792ozO-Szi3PwUx6tGTd164YckAYTUUvRkZu2g-h25iGzGqcjx3fGKGF6NEs/exec";
String DataURL = "", Status_Read_Sensor = "", rfid = "";
float Temp, Humd, Prs;
float batteryVoltage = 0.0, batteryPercent = 0.0, windSpeed = 0.0;

bool isEncodedHost = false;
bool tryEncodedHostFallback = true;
volatile bool diagRequested = false;

// ---------- Compute Battery Percentage from Voltage ----------
float getBatteryPercentage(float voltage) {
  voltage = constrain(voltage, 2.0, 3.0);
  return (voltage - 2.0) * 100.0;
}

// ---------- Interrupt for Diagnostic Button ----------
/*
Function in the ESP32's RAM that turns the diagnostic button on
*/
void IRAM_ATTR onDiagButton() {
  diagRequested = true;
}

// ---------- RGB Diagnostic LED Control ----------
/*
Function that flashes an RGB LED

Parameters: 
r = Red value between 0 and 255
g = Green value between 0 and 255
b = Blue value between 0 and 255
count = Max value for iterator (how many times it repeats) 
delayMs = Amount of time between the LED flashes

Work: 
- Flashes the LED with the input RGB value
- Delay
- Flashes the LED white
- Delay
*/
void flashRGB(uint8_t r, uint8_t g, uint8_t b, int count = 1, int delayMs = 300) {
  for (int i = 0; i < count; i++) {
    rgbLedWrite(RGB_BUILTIN, r, g, b);
    delay(delayMs);
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    delay(delayMs);
  }
}

// ---------- Visual Diagnostic Output ----------
/*
Function that runs diagnostic checks of various systems and flashes LED error codes

Parameters: 
bmeOK = Boolean for if the BME is functional
rfidOK = Boolean for if the RFID is functional
battPct = Float value of the battery percentage
wifiOK = Boolean for if the WiFi is functional
loraOK = Boolean for if the LoRaWAN is functional

Work: 
- Flashes the LED white twice 150 milliseconds apart
- BME-RFID Test:
(Is BME Functional?)|(Is RFID Functional?)|(Output LED Color)
False               | False               | Red
True                | False               | Yellow
False               | True                | Blue
True                | True                | Green
- Battery Percentage Test:
(Is there a Battery Percentage?)|(What is larger than_)|(Output LED Color)
False                           | N/a                  | Red
True                            | 40%                  | Yellow
True                            | 75%                  | Green
- WiFi-LoRaWAN Test: 
(Is WiFi Functional?)|(Is LoRaWAN Functional?)|(Output LED Color)
False                | False                  | Red
True                 | False                  | Yellow
False                | True                   | Blue
True                 | True                   | Green
- Set diagRequested to false
*/
void runDiagnostics(bool bmeOK, bool rfidOK, float battPct, bool wifiOK, bool loraOK) {
  flashRGB(255, 255, 255, 2, 150); // Entry flash
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

// ---------- Trigger Diagnostic Mode via USB or Button ----------
/*
Function called to enter/check the diagnostics mode

Parameters: 
bmeOK = Boolean for if the BME is functional
rfidOK = Boolean for if the RFID is functional
battPct = Float value of the battery percentage
wifiOK = Boolean for if the WiFi is functional
loraOK = Boolean for if the LoRaWAN is functional

Work: 
- Initializes the diagnostic pin
- If the USB is inserted the diagnostics are ran, and a 2 second delay occurs
- If the diagnostic pin is pulled low, a 2 second delay occurs, then if the pin is still low it runs the diagnostics
*/
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

// ---------- Compute Battery Percentage from Voltage ----------
/*
Function that grabs the battery percentage

Parameters:
voltage = Float value

Work:
voltage = Input value constrained to between 2.0 and 3.0 (=2.0 if < 2.0, =3.0 if > 3.0)

Output: 
Returns a percentage of the constrained input voltage's magnitude difference from the lower bound
*/
float getBatteryPercentage(float voltage) {
  voltage = constrain(voltage, 2.0, 3.0);
  return (voltage - 2.0) * 100.0;
}

// ---------- Encode URL Parameters ----------
/*
Function to encode a string as a url

Parameters: 
s = Input string to be encoded

Work: 
- Initializes the 'encoded' string variable
- Initializes the constant 'hex' character variable
- For each character 'c' in 's', 
  - If 'c' is alphanumeric, add 'c' to 'encoded'
  - Else, add '%' to 'encoded', add hex[(c >> 4) & 0xF] to 'encoded', add hex[c & 0xF] to 'encoded'
Output: 
Returns 'encoded'
*/
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

// ---------- Build Upload URL Depending on Target ----------
/*
Function to build the website URL

Parameters: 
diagnosticMode = Boolean that tells if diagnostic mode is enabled (defaults to false)

Work: 

*/
void buildURL() {
  if (isEncodedHost) {
    String encodedParams = "?sts=write";
    encodedParams += "&id=" + ID;
    encodedParams += "&bc=" + String(bootCount);
    encodedParams += "&bat=" + String(batteryPercent, 2);
    encodedParams += "&srs=" + Status_Read_Sensor;
    encodedParams += "&temp=" + String(Temp);
    encodedParams += "&humd=" + String(Humd);
    encodedParams += "&Prs=" + String(Prs);
    encodedParams += "&wind=" + String(windSpeed, 2);
    encodedParams += "&rfid=" + rfid;
    DataURL = "http://192.168.4.1/data?url=" + urlEncode(Web_App_Path + encodedParams);
  } else {
    DataURL = "https://script.google.com" + Web_App_Path;
    DataURL += "?sts=write";
    DataURL += "&id=" + ID;
    DataURL += "&bc=" + String(bootCount);
    DataURL += "&bat=" + String(batteryPercent, 2);
    DataURL += "&srs=" + Status_Read_Sensor;
    DataURL += "&temp=" + String(Temp);
    DataURL += "&humd=" + String(Humd);
    DataURL += "&Prs=" + String(Prs);
    DataURL += "&wind=" + String(windSpeed, 2);
    DataURL += "&rfid=" + rfid;
  }
}

// ---------- Sensor Data Acquisition ----------
/*
Function to read the sensor data

Work: 
  digitalWrite(BME_CS, LOW);              #ESP32 initiates SPI communication with BME 
  Temp = bme.readTemperature();           #^Polls the temperature sensor
  Humd = bme.readHumidity();              #^Polls the humidity sensor
  Prs  = bme.readPressure() / 100.0F;     #^Polls the pressure sensor, and converts the reading to hectoPascals
  digitalWrite(BME_CS, HIGH);             #^ESP32 ends SPI communication with BME
  Status_Read_Sensor = (isnan(Temp) || isnan(Humd) || isnan(Prs)) ? "Failed" : "Success"; #Verifies that each variable holds a number

  digitalWrite(RFID_CS, LOW);             #ESP32 initiates SPI communication with RFID (MFRC522 reader/writer)  
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {   
    rfid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++)
      rfid += String(mfrc522.uid.uidByte[i], HEX);
  }
  ^^ Verifies the Proximity Integrated Circuit Card (PICC) exists and that it can be read
  ^^ Initiates 'rfid' tag variable
  ^^ Converts the HEX rfid tag and updates the 'rfid' variable 
  digitalWrite(RFID_CS, HIGH);            #^ESP32 ends SPI communication with RFID (MFRC522 reader/writer) 
*/
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

// ---------- Attempt Upload via HTTP ----------
/*
Function to send 

Work: 
  HTTPClient http;                                                  #Initializes the 'http' object as an instance of the 'HTTPClient' class
  String url = "http://192.168.4.1/data?url=" + urlEncode(DataURL); #Sets 'url' to the IP address for the host
  http.begin(url);                                                  #Establishes connection
  http.GET();                                                       #Pulls the Server Name, Server Port, URLPath, HTTP Method, and User Agent
  http.end();                                                       #Ends connection
*/
bool HTTP_send() {
  HTTPClient http;
  http.begin(DataURL);
  int httpCode = http.GET();
  http.end();
  return (httpCode > 0 && httpCode == HTTP_CODE_OK);
}

// ---------- LoRa Transmission with ACK ----------
/*
LoRa_sendAndVerify() function that sends a message over LoRa, then verifies that the message was received

Work: 
  digitalWrite(LORA_RST, LOW);                #Initiates reset of LoRa device 
  digitalWrite(LORA_RST, HIGH);               #Ends reset of LoRa device
  if (!radio.init()) {
    Serial.println("❌ LoRa init failed");
    return false;                             #If the LoRa device failts to initiate, the function returns false
  }
  radio.setFrequency(RF95_FREQ);                              #Sets 'radio' frequency to previously specified value
  radio.setTxPower(20, false);                                #Sets transmission power to 20 (false means we have a boost)
  radio.send((uint8_t *)DataURL.c_str(), DataURL.length());   #Sends 'DataURL' as a string
  radio.waitPacketSent();                                     #^Blocks until the transmitter is no longer transmitting.
  // Wait for ACK
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);                #Defines 'len' as max buffer size for RH_RF95
  if (radio.waitAvailableTimeout(1000)) {         #Starts the receiver and blocks for a 1s
    if (radio.recv(buf, &len)) {                  #Turns the receiver on if it not already 
      Serial.print("✅ ACK received: ");           on. If there is a valid message available, 
      Serial.println((char*)buf);                   copy it to buf and return true else return false.
      return true;
    } else {
      Serial.println("⚠️ ACK receive failed");
      return false;
    }
  } else {
    Serial.println("⚠️ No ACK received");
    return false;
  }
Output:
- Returns either true or false
*/
bool LoRa_sendAndVerify() {
  digitalWrite(LORA_RST, LOW); delay(10);
  digitalWrite(LORA_RST, HIGH); delay(10);
  if (!radio.init()) return false;
  radio.setFrequency(915.0);
  radio.setTxPower(20, false);
  radio.send((uint8_t *)DataURL.c_str(), DataURL.length());
  radio.waitPacketSent();

  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  if (radio.waitAvailableTimeout(1000)) {
    if (radio.recv(buf, &len)) return true;
  }
  return false;
}

// ---------- Hold Regulator On During Deep Sleep ----------
void prepareRegulatorPin() {
  pinMode(REGULATOR_EN_PIN, OUTPUT);
  digitalWrite(REGULATOR_EN_PIN, HIGH);
  gpio_hold_dis((gpio_num_t)REGULATOR_EN_PIN);
  gpio_hold_en((gpio_num_t)REGULATOR_EN_PIN);
}

// ---------- Main Entry ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  ++bootCount;
  Serial.printf("🔄 Boot #%d\n", bootCount);
  // ---------- GPIO Setup ----------
  pinMode(BME_CS, OUTPUT);    digitalWrite(BME_CS, HIGH);
  pinMode(RFID_CS, OUTPUT);   digitalWrite(RFID_CS, HIGH);
  pinMode(DIAG_PIN, INPUT_PULLUP);
  pinMode(REGULATOR_EN_PIN, OUTPUT); digitalWrite(REGULATOR_EN_PIN, HIGH);
  pinMode(LORA_RST, OUTPUT);  digitalWrite(LORA_RST, HIGH);
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
  // ---------- WiFi Moderation ----------
  WiFiManager wm;
  bool wifiOK = wm.autoConnect("FiltSure_Setup");
  if (wifiOK) {
    Serial.println("✅ Connected to WiFi");
    String ssid = WiFi.SSID();
    isEncodedHost = ssid.startsWith("ESP32_HOST") || ssid.startsWith("FiltSure");
  } else {
    Serial.println("❌ Failed to connect to WiFi");
  }
  // ---------- Read Sensors ----------
  batteryVoltage = analogRead(BATTERY_PIN) * (5.0 / 4095.0) * 2.0;
  batteryPercent = getBatteryPercentage(batteryVoltage);
  windSpeed = analogRead(WIND_PIN) * (5.0 / 4095.0);
  readSensors();
  buildURL();
  // ---------- Diagnostics Check ----------
  bool bmeOK = !isnan(Temp);
  bool rfidOK = !rfid.isEmpty();
  bool loraOK = radio.init();
  checkDiagnosticsMode(bmeOK, rfidOK, batteryPercent, wifiOK, loraOK);
  // ---------- Upload Logic ----------
  bool uploadSuccess = false;
  if (wifiOK) {
    if (HTTP_send()) {
      Serial.println("✅ Data uploaded successfully");
      uploadSuccess = true;
    } else if (tryEncodedHostFallback) {
      Serial.println("⚠️ Upload to Sheets failed, trying encoded host fallback");
      isEncodedHost = true;
      buildURL();
      uploadSuccess = HTTP_send();
    }
  }
  // ---------- Wake Sources ----------
  if (!uploadSuccess) {
    Serial.println("📡 Trying LoRa fallback...");
    uploadSuccess = LoRa_sendAndVerify();
    Serial.println(uploadSuccess ? "✅ LoRa upload success" : "❌ LoRa upload failed");
  }
 // ---------- Sleep ----------
  esp_sleep_enable_timer_wakeup(sleep_Time * us_S);
  prepareRegulatorPin();
  Serial.println("💤 Going to deep sleep...");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {}
