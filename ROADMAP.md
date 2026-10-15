# Roadmap

Vehicle-data status was updated on **15 October 2026** from the owner's
confirmation on the 2011 Juke F15 1.6 DIG-T manual. Speed, odometer, RPM,
steering, indicators, doors/trunk, handbrake, lights, battery voltage and
engine coolant temperature work on this vehicle. Remaining data is
approximate, unvalidated or unavailable and needs further captures,
calibration or decoding work. See the current status and evidence in the
[English mapping](docs/JUKE_F15_CAN_MAPPING.md#sensors-confirmed-on-the-vehicle)
and [French mapping](docs/JUKE_F15_CAN_MAPPING.fr.md#capteurs-confirmés-sur-le-véhicule).

## ✅ Completed

### Confirmed Vehicle Data

| Feature | Notes |
|---------|-------|
| Steering / Dynamic Guidelines | Owner uses `steerScale=500`, offset 0 and inversion; JSON default remains 1000 |
| Engine RPM | `0x180` bytes 0–1 BE `/8`; owner confirmation and OBD comparison |
| Vehicle Speed | `0x284` bytes 4–5 BE `/100`; owner confirmation and OBD comparison |
| Odometer | `0x5C5` bytes 1–3 BE; owner confirmation |
| Battery Voltage | Owner confirmation; consistent with OBD references |
| Door Status | All 4 doors + trunk |
| Indicators | Left / Right |
| Lights | Parking lights, low beam and high beam; Manager parking-light parsing check documented separately |
| Handbrake | `0x5C5 B0 bit2`, available through `SYS DATA` |
| Engine Coolant Temperature | `0x551 B0−40`; owner confirmation and OBD comparison; outside temperature remains incorrect |

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

### Capture and Validation Tooling

The capture tools are implemented. Remaining validation concerns the
unconfirmed data below, not the availability of CAN/OBD logging.

| Feature | Status |
| --- | --- |
| Timestamped raw CAN capture | Implemented with `LOG ON` |
| OBD service 01 references | Implemented for advertised PIDs; diagnostic lines start with `ODB` |
| Truncated/extended/RTR frame protection | Implemented before any decoded-data update |
| Driver loss counters | `ODB DRIVER`, at most once per second |
| CAN/OBD comparison | Host tools available; no automatic preset changes |
| Remaining sensor calibration | CAN/OBD captures analysed; additional references needed for the unconfirmed data below |

## 🚧 Upcoming

### v1.8 — Additional Vehicle Data

| Task | Status | Notes |
|------|--------|-------|
| External Temperature | Unvalidated / incorrect | `0x540 B0−40` gives 28 °C versus approximately 20 °C reported; engine coolant is separately confirmed |
| Fuel Tank Level | Unavailable | False `0x5C5 B0` gauge removed; identify the actual level source |
| Instant / Average Fuel Consumption | Approximate | `0x551 B1` counter plus `0x284` speed; provisional 80 mm³/tick still needs calibration |
| Distance to Empty | Unvalidated | `0x54C` units and scale need a cluster reference |
| Brake Pedal | Unvalidated | Verify physical action and polarity of `0x180 B7 bit0` |
| Reverse / Gear | Unidentified on this vehicle | Capture separately annotated neutral/reverse transitions |
| Handbrake on radio UART | Unvalidated | CAN decoding and USB dashboard operation are confirmed; a dedicated RAV4 UART representation is not established |
| CAN Frame Recorder | Implemented | Raw CAN plus OBD references with `LOG ON` |
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
