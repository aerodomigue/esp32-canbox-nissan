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
V1.0-1.1 (Core) → V1.2 (Doors) → V1.3 (Lights) → V1.4 (RAV4 Protocol) → V1.5 (Cleanup) → Release V1
    │
    └──→ V2.1 (NVS) → V2.2 (Commands) → V2.3 (OTA) → V2.4 (APK) → Release V2
```
