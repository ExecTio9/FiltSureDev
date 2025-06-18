"""
ESP32 Serial-to-HTTPS Forwarder
--------------------------------
This script listens to an ESP32 relay node over a serial port (e.g., COM5),
reads Google Apps Script webhook URLs sent by sensor nodes, and forwards
them to the internet via HTTPS.

Used to bridge ESP32 devices that cannot directly access WiFi,
by turning a local relay ESP32-A + desktop into a transparent proxy.

Requirements:
- Python 3.x
- `requests` and `pyserial` libraries installed
"""

import serial
import requests
import socket
import urllib3

# Suppress SSL warnings (safe for testing with Google Apps Script)
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# ------------- CONFIGURATION -------------
PORT = "COM5"           # Serial port connected to ESP32-A (relay)
BAUD = 115200           # Match the baud rate of the ESP32
# ----------------------------------------

# Attempt to open the serial port
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"[INFO] Listening on {PORT} at {BAUD} baud...")
except serial.SerialException as e:
    print(f"[ERROR] Could not open serial port {PORT}: {e}")
    exit(1)

# Main listening loop
while True:
    try:
        # Check if ESP32 has sent data
        if ser.in_waiting > 0:
            line = ser.readline().decode(errors="ignore").strip()

            # Check if it's a Google Script URL
            if line.startswith("http://script.google.com"):
                print(f"[FORWARDING] {line}")

                try:
                    # Convert to HTTPS URL
                    url = line.replace("http://", "https://")

                    # Optional debug output
                    print(f"[DEBUG URL] {url}")

                    # Forward the request to Google, skipping SSL cert verify
                    response = requests.get(url, verify=False)

                    # Print brief response
                    print(f"[RESPONSE] {response.status_code} - {response.text[:200]}...")

                except Exception as req_err:
                    print(f"[REQUEST ERROR] {req_err}")

    except KeyboardInterrupt:
        print("\n[EXIT] User interrupted")
        break
    except Exception as e:
        print(f"[ERROR] {e}")