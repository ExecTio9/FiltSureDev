# FiltSure HVAC Monitor — Release Notes

## v1.0.0 (Initial Field Version) — June 2025

### Sensor Node Firmware
- Deep Sleep cycle optimized (5 min default)
- OTA Update support implemented
- Diagnostic LED 3-stage pulse added
- PING/PONG LoRa test supported
- Google Sheet Logging: Full field list
- Battery Level reporting verified
- RFID filter swap flow tested

### Gateway Node Firmware
- PING/PONG control
- LoRa → WiFi → Google Apps Script forwarding
- Basic OTA delivery tested (~32 KB max)

### Known Limitations
- Mesh routing not implemented
- OTA limited to 32 KB payload
- No MQTT support yet

---

## Upcoming (Roadmap)
- Multi-hop repeater support
- MQTT gateway fallback
- Remote configuration OTA
