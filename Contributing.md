# Contributing to FiltSure HVAC Monitor

Welcome!  
We value contributions from the team to improve the FiltSure system.

Please follow these guidelines when contributing:

## Branch Workflow

- **main** → stable production code (deployed to field devices)
- **dev** → integration branch (tested but not field-ready)
- **feature/xyz** → feature development branches

## How to Contribute

1. Fork or branch from `dev`.
2. Implement your feature or fix.
3. Test thoroughly on a real Sensor Node or Gateway.
4. Submit a Pull Request (PR) into `dev` branch.
5. Include a summary of changes in your PR.

## Code Style

- Arduino / C++ style: follow existing project conventions.
- Document any new modules or features with clear comments.
- Keep Diagnostic LED logic consistent across versions.
- Maintain compatibility with existing Google Sheet structure.

## OTA & Firmware Versioning

- Always increment firmware version field before merge to `main`.
- Include a release note in `RELEASE.md` for version bump.
- Test OTA flow with at least 1 real Sensor Node before release.

## Communication Testing

- Validate LoRa link:
  - PING/PONG success
  - RSSI within expected range
- Validate Google Sheet data:
  - All fields populated correctly

## Documentation

- Update README.md and Guide (DOCX) if necessary.
- If adding hardware changes, update `/hardware/` schematics.

---

Thank you for helping improve FiltSure!
