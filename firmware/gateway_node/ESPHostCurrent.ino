    #include <WiFi.h>
    #include <WebServer.h>
    #include <WiFiClient.h>
    #include <SPI.h>
    #include <RH_RF95.h>

    #define WIFI_SSID     "ESP32_HOTSPOT"
    #define WIFI_PASS     "FiltSure_Rules"

    #define RFM95_CS      2
    #define RFM95_IRQ     4
    #define RFM95_INT     5
    #define RF95_FREQ     915.0

    WebServer server(80);
    WiFiServer telnetServer(23);
    WiFiClient telnetClient;
    RH_RF95 rf95(RFM95_CS, RFM95_IRQ);

    bool forceWiFi = true;
    bool forceLoRa = false;
    bool autoComm = true;

    void handleData() {
      if (server.hasArg("url")) {
        String url = server.arg("url");
        Serial.println("http://script.google.com" + url);
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Missing 'url' parameter");
      }
    }

    void handleTelnetCommand(String cmd) {
      cmd.trim();
      if (cmd == "status") {
        telnetClient.println("📡 Comm Mode:");
        telnetClient.printf("  WiFi: %s\\n", forceWiFi ? "ENABLED" : "DISABLED");
        telnetClient.printf("  LoRa: %s\\n", forceLoRa ? "ENABLED" : "DISABLED");
        telnetClient.printf("  Auto: %s\\n", autoComm ? "ENABLED" : "DISABLED");
      } else if (cmd == "force_wifi") {
        forceWiFi = true;
        forceLoRa = false;
        autoComm = false;
        telnetClient.println("✅ WiFi forced");
      } else if (cmd == "force_lora") {
        forceWiFi = false;
        forceLoRa = true;
        autoComm = false;
        telnetClient.println("✅ LoRa forced");
      } else if (cmd == "auto_comm") {
        forceWiFi = false;
        forceLoRa = false;
        autoComm = true;
        telnetClient.println("✅ Auto comm mode enabled");
      } else if (cmd == "reboot") {
        telnetClient.println("🔁 Rebooting...");
        delay(1000);
        ESP.restart();
      } else if (cmd == "sleep") {
        telnetClient.println("💤 Going to sleep...");
        esp_deep_sleep_start();
      } else if (cmd == "diag_ping") {
        telnetClient.println("📟 Sending LoRa diagnostic ping...");

        const char* pingMsg = "ping";
        rf95.send((uint8_t*)pingMsg, strlen(pingMsg));
        rf95.waitPacketSent();

        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);

        if (rf95.waitAvailableTimeout(1000)) {
          if (rf95.recv(buf, &len)) {
            String response = String((char*)buf).substring(0, len);
            if (response == "pong") {
              telnetClient.println("✅ Pong received from node!");
              Serial.println("✅ Pong received from node!");
            } else {
              telnetClient.print("⚠️ Unexpected response: ");
              telnetClient.println(response);
            }
          } else {
            telnetClient.println("⚠️ Failed to read LoRa response");
          }
        } else {
          telnetClient.println("❌ No pong received");
        }
      } else {
        telnetClient.println("❌ Unknown command");
      }
    }

    void setup() {
      Serial.begin(115200);
      delay(1000);
      Serial.println("[BOOT] ESP32-C3 Gateway");

      WiFi.softAP(WIFI_SSID, WIFI_PASS);
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.softAPIP());

      server.on("/data", HTTP_GET, handleData);
      server.begin();
      Serial.println("[HTTP] Web server ready");

      telnetServer.begin();
      telnetServer.setNoDelay(true);
      Serial.println("[TELNET] Listening on port 23");

      SPI.begin();  // GPIO10=MOSI, GPIO9=MISO, GPIO8=SCK on ESP32-C3
      if (!rf95.init()) {
        Serial.println("[LoRa] Init FAILED");
      } else {
        rf95.setFrequency(RF95_FREQ);
        rf95.setTxPower(20, false);
        Serial.println("[LoRa] Ready");
      }
    }

    void loop() {
      server.handleClient();

      if (!telnetClient || !telnetClient.connected()) {
        telnetClient = telnetServer.available();
      }

      if (telnetClient && telnetClient.connected() && telnetClient.available()) {
        String input = telnetClient.readStringUntil('\\n');
        handleTelnetCommand(input);
      }

      if (rf95.available()) {
        uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);
        if (rf95.recv(buf, &len)) {
          Serial.print("[LoRa RX] ");
          Serial.write(buf, len);
          Serial.println();

          if (telnetClient && telnetClient.connected()) {
            telnetClient.print("[LoRa RX] ");
            telnetClient.write(buf, len);
            telnetClient.println();
          }
        }
      }
    }