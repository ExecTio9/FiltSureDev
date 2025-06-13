# Sensor Node Firmware Folder

Target hardware: ESP32-C6 DevKitM-1

Features:
- Reads BME280
- Reads MFRC522
- Reads Battery Voltage
- Reads Wind Speed Sensor
- Sends data via LoRa
- Responds to PING
- Supports OTA update
- Diagnostic LED Mode

Dependencies:
- Adafruit_BME280
- MFRC522
- RadioHead RH_RF95
- WiFiManager
