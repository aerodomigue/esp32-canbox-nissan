# Roadmap

Implementation status describes firmware behavior. The initial Juke mappings
remain in use; RPM, speed, fuel and consumption still require vehicle reference
measurements. The rejected PR comment mappings and their experimental profile
have been removed.

## ✅ Completed

### v1.0 — Core Vehicle Data

| Feature | Notes |
|---------|-------|
| Steering / Dynamic Guidelines | Calibrated for Juke F15 |
| Engine RPM | Initial `/7` mapping; calibration pending |
| Vehicle Speed | Initial bytes 0–1 `/100` mapping; calibration pending |
| Fuel Tank Level | Initial 0–45 mapping; meaning of the CAN byte unverified |
| Battery Voltage | |
| Door Status | All 4 doors + trunk |
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
| Distance to Empty | Initial CAN 0x54C mapping; calibration pending |
| Instant Fuel Consumption | Initial CAN 0x580 byte 3; CAN unit unverified |
| Average Fuel Consumption | CAN 0x580 byte 4; CAN unit unverified |
| Android Companion App | [ESP32 CANBox Manager](https://github.com/aerodomigue/esp32-canbox-manager) |

---

## 🚧 Upcoming

### Work on `feat/obd-calibration-logging`

These changes are local to the dedicated branch; the PR branch remains at `495f346`.

| Feature | Status |
| --- | --- |
| Timestamped raw CAN capture | Implemented with `LOG ON` |
| OBD service 01 references | Implemented for advertised PIDs; diagnostic lines start with `ODB` |
| Truncated/extended/RTR frame protection | Implemented before any decoded-data update |
| Driver loss counters | `ODB DRIVER`, at most once per second |
| CAN/OBD comparison | Host tools available; no automatic preset changes |
| Physical calibration on the Juke | Pending a simultaneous CAN/OBD capture |

### v1.8 — Additional Vehicle Data

| Task | Status | Notes |
|------|--------|-------|
| External Temperature | ✅ | Initial CAN 0x540; 0x551 is the coolant-temperature candidate |
| Handbrake | ❌ Not possible | Toyota RAV4 protocol marks this as TBD — no standardized command exists |
| CAN Frame Recorder | Implemented on dedicated branch | Raw CAN plus OBD references with `LOG ON` |
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
