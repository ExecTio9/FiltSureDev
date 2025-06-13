# Google Apps Script for FiltSure HVAC Monitor

This folder contains the Google Apps Script used to log Sensor Node data to a Google Sheet.

## Purpose

- Receives HTTP requests from Gateway Node (via WiFi)
- Parses URL parameters and extracts:
  - Device ID
  - Boot Count
  - Battery Voltage
  - Sensor Status
  - Temperature
  - Humidity
  - Pressure
  - Wind Speed
  - RFID Tag UID
- Logs data into Google Sheet:
  - Separate tab per Sensor Node (`UNIT_<DeviceID>`)

## Notes

- Apps Script must be deployed as Web App (execute as owner, accessible to anyone with link)
- Current script URL: `https://script.google.com/macros/...`

## Related Docs

See FiltSure Guide — Communication Protocol section.
