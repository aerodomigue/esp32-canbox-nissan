# Roadmap

## ✅ Completed

### v1.0 — Core Vehicle Data

| Feature | Notes |
|---------|-------|
| Steering / Dynamic Guidelines | Calibrated for Juke F15 |
| Engine RPM | |
| Vehicle Speed | |
| Fuel Tank Level | Calibrated 45L |
| Battery Voltage | |
| Door Status | All 4 doors + trunk |
| Handbrake | |
| Indicators | Left / Right |
| Lights | Headlights, high beam, parking |

### v1.7 — Configuration & Connectivity

| Feature | Notes |
|---------|-------|
| Toyota RAV4 Protocol | Migrated from VW Polo |
| NVS Persistent Storage | Calibration saved to flash |
| Serial Commands | CFG, LOG, SYS, CAN |
| JSON CAN Config | Multi-vehicle support via LittleFS |
| Mock Data Generator | Testing without vehicle |
| OTA Firmware Update | Via USB serial |
| Android Companion App | [ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager) |

---

## 🚧 Upcoming

### v1.8 — External CANBox Passthrough

Allow external CAN boxes (parking sensors, blind spot monitors, etc.) to communicate with the head unit through our device.

| Task | Status |
|------|--------|
| Add UART2 (GPIO 2/3) | 🔲 |
| Frame parser (0x2E protocol) | 🔲 |
| TX Merge (insert between our frames) | 🔲 |
| RX Filter (forward unknown commands) | 🔲 |
| Serial commands (`PT ON/OFF/STATUS`) | 🔲 |

📄 See **[Passthrough Architecture](docs/technical/PASSTHROUGH.md)**

---

## 💡 Ideas

| Feature | Notes |
|---------|-------|
| External Temperature | CAN data not yet extracted |
| Instant Fuel Consumption | CAN data not yet extracted |
| Distance to Empty | CAN data not yet extracted |
| Steering Auto-Calibration | Detect center offset automatically |
| CAN Frame Recorder | For debugging |
| Community Presets | Share vehicle configs |
