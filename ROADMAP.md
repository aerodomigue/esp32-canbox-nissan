# Roadmap - Nissan CAN Box ESP32

## V1 - Core Functionality (Current)

**Goal:** All basic features working and stable.

| Phase | Task | Status | Details |
| --- | --- | --- | --- |
| **V1.0** | Steering / Dynamic Guidelines | ✅ Done | Calibrated for Juke F15 |
| **V1.0** | Engine RPM | ✅ Done | |
| **V1.0** | Fuel Tank Level | ✅ Done | Calibrated 45L |
| **V1.0** | Battery Voltage | ✅ Done | |
| **V1.1** | Vehicle Speed | ✅ Done | |
| **V1.2** | Door Status | ✅ Done | All 4 doors + trunk |
| **V1.2** | Handbrake | ✅ Done | |
| **V1.3** | Indicators | ✅ Done | Left/right turn signals |
| **V1.3** | Lights | ✅ Done | Headlights, high beam, parking lights |
| **V1.4** | Toyota RAV4 Protocol | ✅ Done | Migrated from VW Polo to Toyota RAV4 |
| **V1.4** | External Temperature | ⚠️ WIP | Shows coolant temp (no ext. sensor on CAN) |
| **V1.4** | Instant Fuel Consumption | ❌ Not working | Decoded but not displayed on head unit |
| **V1.4** | Distance to Empty | ❌ Not working | Decoded but not displayed on head unit |
| **V1.5** | Cleanup & Documentation | 🔲 Todo | Remove debug logs, finalize docs |
| **V1.5** | Stable Release | 🔲 Todo | GitHub tag v1.0.0 |

---

## V2 - Configuration & OTA via USB

**Goal:** No more recompiling to change parameters or update firmware.

### Phase 2.1 - Storage & Config (ESP32) ✅

| Task | Details | Status |
| --- | --- | --- |
| Add `Preferences.h` (NVS) | Persistent parameter storage | ✅ Done |
| Replace `#define` with variables | STEER_OFFSET, STEER_SCALE, TANK_SIZE, RPM_DIVISOR | ✅ Done |
| Load config at boot | Read NVS → apply to variables | ✅ Done |
| Default values | If NVS empty, use Juke F15 defaults | ✅ Done |

### Phase 2.2 - Serial Command Protocol (ESP32) ✅

| Task | Details | Status |
| --- | --- | --- |
| Command parser | Read Serial, parse AT-style commands | ✅ Done |
| CFG commands | `CFG GET`, `CFG SET`, `CFG LIST`, `CFG SAVE`, `CFG RESET` | ✅ Done |
| LOG commands | `LOG ON/OFF` for CAN frame logging | ✅ Done |
| SYS commands | `SYS INFO`, `SYS DATA`, `SYS REBOOT` | ✅ Done |

### Phase 2.3 - CAN Configurable System (ESP32) ✅

| Task | Details | Status |
| --- | --- | --- |
| JSON-based CAN config | Vehicle configs stored on LittleFS | ✅ Done |
| Mock data generator | Simulated data for testing without vehicle | ✅ Done |
| CAN serial commands | `CAN STATUS`, `CAN LIST`, `CAN LOAD`, `CAN GET`, `CAN DELETE` | ✅ Done |
| CAN config upload | `CAN UPLOAD START/DATA/END` via Base64 | ✅ Done |
| Multi-vehicle support | Switch configs without recompiling | ✅ Done |
| USB protocol documentation | `docs/protocols/USB_SERIAL_PROTOCOL.md` | ✅ Done |

### Phase 2.4 - OTA Firmware Update (ESP32)

| Task | Details | Status |
| --- | --- | --- |
| 🔲 Integrate `Update.h` | Native ESP32 OTA library | Todo |
| 🔲 OTA commands | `OTA START <size>`, `OTA DATA`, `OTA END`, `OTA ABORT` | Todo |
| 🔲 Checksum validation | MD5 or CRC32 before reboot | Todo |
| 🔲 Rollback safety | Keep old partition if update fails | Todo |

### Phase 2.5 - Android Application (APK)

| Task | Details | Status |
| --- | --- | --- |
| 🔲 Android Studio project | Kotlin, Material Design | Todo |
| 🔲 USB Serial integration | `usb-serial-for-android` library | Todo |
| 🔲 Config screen | Sliders/inputs for each parameter | Todo |
| 🔲 Logs screen | Real-time CAN data display | Todo |
| 🔲 Update screen | Select .bin file, progress bar, flash | Todo |
| 🔲 CAN config screen | Upload/download vehicle configs | Todo |
| 🔲 Debug screen | Send manual commands, view raw responses | Todo |

### Phase 2.6 - Polish & Release

| Task | Details | Status |
| --- | --- | --- |
| 🔲 Full testing | Config, OTA, edge cases | Todo |
| 🔲 V2 Documentation | APK user guide | Todo |
| 🔲 Release APK | GitHub releases | Todo |
| 🔲 GitHub tag v2.0.0 | | Todo |

---

## V3 - Future Ideas

| Idea | Details | Status |
| --- | --- | --- |
| ✅ Multi-vehicle support | Profiles for Qashqai, Leaf, Micra... | Done in V2.3 |
| 💡 Steering auto-calibration | Automatically detect center offset | Todo |
| 💡 CAN frame recorder | Record frames for debugging | Todo |
| 💡 Community presets | Share configs between users | Todo |

---

## Development Order

```
V1.0-1.4 (Core) ────────────────────────────────────────→ V1.5 (Cleanup) → Release V1
    │
    └──→ V2.1 (NVS) → V2.2 (Commands) → V2.3 (CAN Config) → V2.4 (OTA) → V2.5 (APK) → Release V2
              ✅            ✅               ✅              🔲           🔲
```
