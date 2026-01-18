# Roadmap - Nissan CAN Box ESP32

## V1 - Core Functionality (Current)

**Goal:** All basic features working and stable.

| Phase | Task | Status | Details |
| --- | --- | --- | --- |
| **V1.0** | Steering / Dynamic Guidelines | ✅ Done | Calibrated for Juke F15 |
| **V1.0** | Engine RPM | ✅ Done | |
| **V1.0** | Fuel Tank Level | ✅ Done | Calibrated 45L |
| **V1.0** | Battery Voltage | ✅ Done | |
| **V1.1** | Vehicle Speed | ⚠️ WIP | Testing and validating scaling |
| **V1.1** | External Temperature | ⚠️ WIP | Find ext. sensor on CAN or keep coolant temp |
| **V1.2** | Door Status | ⚠️ WIP | Validate bit mapping for Juke F15 |
| **V1.2** | Handbrake | ⚠️ WIP | Identify CAN signal |
| **V1.3** | Cleanup & Documentation | 🔲 Todo | Remove debug logs, finalize docs |
| **V1.3** | Stable Release | 🔲 Todo | GitHub tag v1.0.0 |

---

## V2 - Configuration & OTA via USB

**Goal:** No more recompiling to change parameters or update firmware.

### Phase 2.1 - Storage & Config (ESP32)

| Task | Details |
| --- | --- |
| 🔲 Add `Preferences.h` (NVS) | Persistent parameter storage |
| 🔲 Replace `#define` with variables | STEER_OFFSET, STEER_SCALE, TANK_SIZE, RPM_DIVISOR |
| 🔲 Load config at boot | Read NVS → apply to variables |
| 🔲 Default values | If NVS empty, use Juke F15 defaults |

### Phase 2.2 - Serial Command Protocol (ESP32)

| Task | Details |
| --- | --- |
| 🔲 Command parser | Read Serial, parse AT-style commands |
| 🔲 CONFIG commands | `CFG GET`, `CFG SET`, `CFG SAVE`, `CFG RESET` |
| 🔲 LOG commands | `LOG ON/OFF`, `LOG LEVEL`, `LOG CAN` |
| 🔲 SYSTEM commands | `SYS INFO`, `SYS REBOOT`, `SYS VERSION` |
| 🔲 JSON responses | Structured format for the app |

### Phase 2.3 - OTA Update (ESP32)

| Task | Details |
| --- | --- |
| 🔲 Integrate `Update.h` | Native ESP32 OTA library |
| 🔲 OTA commands | `OTA START <size>`, `OTA DATA`, `OTA END`, `OTA ABORT` |
| 🔲 Checksum validation | MD5 or CRC32 before reboot |
| 🔲 Rollback safety | Keep old partition if update fails |

### Phase 2.4 - Android Application (APK)

| Task | Details |
| --- | --- |
| 🔲 Android Studio project | Kotlin, Material Design |
| 🔲 USB Serial integration | `usb-serial-for-android` library |
| 🔲 Config screen | Sliders/inputs for each parameter |
| 🔲 Logs screen | Real-time CAN data display |
| 🔲 Update screen | Select .bin file, progress bar, flash |
| 🔲 Debug screen | Send manual commands, view raw responses |

### Phase 2.5 - Polish & Release

| Task | Details |
| --- | --- |
| 🔲 Full testing | Config, OTA, edge cases |
| 🔲 V2 Documentation | APK user guide |
| 🔲 Release APK | GitHub releases |
| 🔲 GitHub tag v2.0.0 | |

---

## V3 - Future Ideas

| Idea | Details |
| --- | --- |
| 💡 Multi-vehicle support | Profiles for Qashqai, Leaf, Micra... |
| 💡 Steering auto-calibration | Automatically detect center offset |
| 💡 CAN frame recorder | Record frames for debugging |
| 💡 Community presets | Share configs between users |

---

## Development Order

```
V1.1 (Speed) → V1.2 (Doors/Handbrake) → V1.3 (Cleanup) → Release V1
    │
    └──→ V2.1 (NVS) → V2.2 (Commands) → V2.3 (OTA) → V2.4 (APK) → Release V2
```
