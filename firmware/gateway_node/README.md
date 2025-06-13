# Gateway Node Firmware (ESP32-C3 / C6)

This folder contains the firmware for the FiltSure Gateway Node.

## Purpose

- Receives LoRa packets from Sensor Nodes
- Parses sensor data payload
- Forwards data via HTTP POST to Google Apps Script
- Handles PING → PONG testing for diagnostic checks
- Supports OTA update delivery to Sensor Nodes (WIP)

## Hardware Target

- ESP32-C3 or ESP32-C6 Dev Board
- RFM95 (or compatible SX127x) LoRa module
- WiFi Antenna (optional)

## Features

- LoRa RX → WiFi TX pipeline
- PING → PONG diagnostic flow
- Basic OTA delivery flow (~32 KB limit)
- CLI / Telnet control planned (future)

## Related Docs

See FiltSure Guide — Gateway Node section.
