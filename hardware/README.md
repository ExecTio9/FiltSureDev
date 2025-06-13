# Hardware Files — FiltSure HVAC Monitor

This folder contains the hardware design files for the FiltSure HVAC Monitor system.

## Purpose

- Provide schematics and PCB layouts for both Sensor Node and Gateway Node hardware
- Enable hardware review, fabrication, and future revisions

## Current Contents

### Sensor Node

- `Sensor_Node.sch` — KiCad schematic for Sensor Node
- `Sensor_Node.kicad_pcb` — KiCad PCB layout for Sensor Node

### Gateway Node

- `Gateway_Node.sch` — KiCad schematic for Gateway Node
- `Gateway_Node.kicad_pcb` — KiCad PCB layout for Gateway Node

## Notes

- All files are created using **KiCad** (version X.Y.Z recommended — specify your KiCad version here)
- Schematics include:
  - Power supply & battery monitor
  - Sensor interfaces (BME280, MFRC522, Wind Speed Fan)
  - RGB LED driver
  - LoRa module interface (RFM95)
- PCB layouts designed for easy assembly & field serviceability

## Usage

- Schematics can be viewed and edited in KiCad
- PCB files can be exported for fabrication:
  - Gerber files
  - Drill files
  - BOM (Bill of Materials)
- Recommended to generate Gerbers with KiCad default plot settings

## Related Docs

- FiltSure Guide — Hardware BOM section
- FiltSure Power Budget Spreadsheet (linked in Guide)
