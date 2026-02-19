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
| Distance to Empty | From CAN 0x54C |
| Instant Fuel Consumption | From CAN 0x580 |
| Average Fuel Consumption | From CAN 0x580 |
| Android Companion App | [ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager) |

---

## 🚧 Upcoming

### v1.8 — Additional Vehicle Data

| Task | Status | Notes |
|------|--------|-------|
| External Temperature | 🔲 | CAN data not yet extracted |
| Handbrake | 🔲 | CAN data not yet extracted |
| CAN Frame Recorder | 🔲 | For debugging |
| Steering Auto-Calibration | 🔲 | Auto-detect center offset |

> **Note:** Community Presets are managed by the [Android app](https://github.com/aerodomigue/esp32-canbox-manager) (download from GitHub or local).

### v1.9 — External CANBox Passthrough

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

## 🏆 v2.0 — Custom PCB

The end goal: a dedicated, professional-grade board ready to use.

> **Prerequisite:** Software (ESP32 firmware + Android app) must be stable and feature-complete before starting PCB development.

### Phase 1 — Prototype

| Task | Status |
|------|--------|
| Validate passthrough on perfboard | 🔲 |
| Test JST connectors | 🔲 |
| Validate 3.3V protection circuit | 🔲 |

### Phase 2 — PCB Design & Manufacturing

| Task | Status |
|------|--------|
| PCB design (KiCad) | 🔲 |
| ESP32-C3 (module or chip TBD) | 🔲 |
| Integrated CAN transceiver | 🔲 |
| JST connectors (head unit, passthrough) | 🔲 |
| 3.3V pin protection | 🔲 |
| USB-C power (from head unit) | 🔲 |
| Boot/Reset buttons | 🔲 |
| Compact form factor | 🔲 |
| 3D printed enclosure | 🔲 |
| PCBA manufacturing | 🔲 |

> **Goal:** One-click order via shared JLCPCB project link (Gerbers + BOM + assembly).
