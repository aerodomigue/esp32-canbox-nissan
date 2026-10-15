# Radio Send - Technical Documentation

The `RadioSend.cpp` module communicates with the Android head unit MCU via the **Toyota RAV4 / Raise protocol** over UART.

This guide describes the current implementation and its remaining protocol
limitations. Consumption uses the documented three-byte `0x22` and 13-byte
`0x23` payloads. Door command `0x24` keeps its documented two-byte payload.
OBD calibration runs on the USB logging path and does not replace the preset's
values sent over UART.

## Commands Summary

| Command | Name | Length | Update Rate |
| --- | --- | --- | --- |
| 0x21 | Trip Info (Avg Speed, Time, Range) | 7 | 5000ms |
| 0x22 | Instantaneous Fuel Consumption | 3 | 1000ms |
| 0x23 | Current Trip / Historical Fuel Consumption | 13 | 5000ms |
| 0x24 | Door Status + IG Proxy | 2 | 250ms |
| 0x28 | Outside Temperature | 12 | 5000ms |
| 0x29 | Steering Wheel Angle | 2 | 200ms |
| 0x7D/01 | Lights & Indicators | 2 | 200ms |
| 0x7D/03 | Vehicle Speed | 5 | 500ms |
| 0x7D/04 | Odometer | 12 | 10000ms |
| 0x7D/0A | Engine RPM | 3 | 333ms |

`0x22` requires an available consumption value in the stated UART units.
The Juke preset's `fuelConsumption` configuration enables an experimental
conversion from the `0x551 B1` counter and vehicle speed. Its provisional
80 mm³ per increment factor is not calibrated for the Juke MR16DDT.
`0x23` sends a current-trip value when available, or `0xFFFF` while a derived
trip is unavailable. Mock mode retains synthetic consumption values with
the same documented packet formats.

---

## Serial Specifications

| Parameter | Value |
| --- | --- |
| **Baud Rate** | 38400 bps |
| **Format** | 8N1 (8 data bits, No parity, 1 Stop bit) |
| **TX Pin** | GPIO 5 (ESP32 → Radio RX) |
| **RX Pin** | GPIO 6 (Radio TX → ESP32) |

---

## Protocol Frame Format

All frames follow this structure:

```
┌────────┬─────────┬────────┬──────────────┬──────────┐
│ Header │ Command │ Length │ Payload[n]   │ Checksum │
│  0x2E  │  (1B)   │  (1B)  │ (Length B)   │   (1B)   │
└────────┴─────────┴────────┴──────────────┴──────────┘
```

### Checksum Calculation

```
Checksum = ((Command + Length + Sum(Payload bytes)) & 0xFF) XOR 0xFF
```

---

## Implemented Commands

### 1. Steering Wheel Angle (Command 0x29)

| Field | Value |
| --- | --- |
| **Update Rate** | 200ms |
| **Payload Length** | 2 bytes |
| **Format** | Two bytes LE, sign-extended; inspected FYT decoder reads signed 12 bits |
| **Unit** | Calibrated UART units, bounded to −500…+500 |

**Payload Structure:**
```
Byte 0: Angle LSB
Byte 1: Angle MSB
```

**Notes:**
- The inspected Toyota `j0/c3` decoder calls `kt.k(..., false, 500)` for
  `0x29`; its guideline displacement saturates at ±500. This is an output
  scale, not proof of physical steering stops or a universal degree unit.
- The sender limits the 32-bit calibrated result before encoding so extreme
  values cannot wrap and reverse direction in the signed 12-bit receiver.
- Scale, offset and sign inversion come from ConfigManager settings.
- F15 input: signed LE `0x002 B0–B1`, in tenths of a degree. With scale 1000,
  offset 0 and inversion enabled, output is `clamp(-trunc(raw / 10), -500, 500)`.
  This chooses one UART unit per degree as a starting calibration; check the
  visible direction and amplitude on the owner's head unit.
- Used for reverse camera dynamic guidelines

Decoder evidence: `com.syu.ms` 26.0507 / versionCode 2126050716, SHA256
`6892e90067437c2a97cfb8e75396edade5d2a1772fd0ef7681d81c28f64fdbdc`.
[Pinned APK source](https://github.com/PimpinPumpkin/fyt7862-mods/blob/4c545e945a1ef6e2faadae29d9a95e9c714822ca/artifacts/apps/com.syu.ms.apk).
This public build is not a dump of the owner's installed head unit.

---

### 2. Door Status (Command 0x24)

| Field | Value |
| --- | --- |
| **Update Rate** | 250ms or on change |
| **Payload Length** | 2 bytes |

**Payload Structure:**
```
Byte 0: Door status bitmask
Byte 1: IG status proxy; bit 1 (0x02) is set when engineRPM > 0, otherwise 0x00
```

**Status Bitmask:**
| Bit | Meaning | Values |
| --- | --- | --- |
| 7 | Driver door (Front Left) | 0=closed, 1=open |
| 6 | Passenger door (Front Right) | 0=closed, 1=open |
| 5 | Rear Right door | 0=closed, 1=open |
| 4 | Rear Left door | 0=closed, 1=open |
| 3 | Trunk/Boot | 0=closed, 1=open |

These labels describe the firmware's current mapping. Raise V2.21 page 5
names bit 7 as the right front door and bit 6 as the left front door, whereas
the firmware uses them for driver/front-left and passenger/front-right.
This left/right interpretation has not been changed by the rollback.

V2.21 documents a two-byte `0x24` payload and IG at **Data1 bit 1**.
`engineRPM > 0` is a running-engine proxy, not an independent ignition-switch
measurement: contact ON with a stopped engine is reported as IG OFF. No
third byte or forced `0x06` is transmitted. The proposed three-byte variant
is a DUDU7 report without confirmation in the local V2.21 specification.

**Synthetic wire examples:** closed doors/IG OFF is `2E 24 02 00 00 D9`;
driver door open/IG proxy ON is `2E 24 02 80 02 57`.

---

### 3. Multi-Function Command (Command 0x7D)

The multi-function command uses sub-commands for different data types.

#### 3.1 Lights & Indicators (Sub-command 0x01)

| Field | Value |
| --- | --- |
| **Update Rate** | 200ms or on change |
| **Payload Length** | 2 bytes |

**Payload Structure:**
```
Byte 0: Sub-command (0x01)
Byte 1: Lights bitmask
```

**Lights Bitmask:**
| Bit | Hex | Meaning |
| --- | --- | --- |
| 7 | 0x80 | Parking lights |
| 6 | 0x40 | Headlights (low beam) |
| 5 | 0x20 | High beam |
| 4 | 0x10 | Left indicator |
| 3 | 0x08 | Right indicator |

**Notes:**
- Indicators use timeout detection (500ms) since CAN only signals when active
- Multiple lights can be active simultaneously (OR'd together)

**Example:** Headlights + Left indicator → 0x40 | 0x10 = 0x50

---

#### 3.2 Engine RPM (Sub-command 0x0A)

| Field | Value |
| --- | --- |
| **Update Rate** | 333ms (~3 Hz) |
| **Payload Length** | 3 bytes |

**Payload Structure:**
```
Byte 0: Sub-command (0x0A)
Byte 1: RPM × 4 LSB
Byte 2: RPM × 4 MSB
```

**Example:** 2500 RPM → 2500 × 4 = 10000 → 0x10, 0x27

---

#### 3.3 Vehicle Speed (Sub-command 0x03)

| Field | Value |
| --- | --- |
| **Update Rate** | 500ms |
| **Payload Length** | 5 bytes |

**Payload Structure:**
```
Byte 0: Sub-command (0x03)
Byte 1: Speed × 100 LSB
Byte 2: Speed × 100 MSB
Byte 3: Average speed LSB, currently 0x00 (unavailable)
Byte 4: Average speed MSB, currently 0x00 (unavailable)
```

**Example:** 45 km/h → 45 × 100 = 4500 → 0x94, 0x11

---

#### 3.4 Odometer (Sub-command 0x04)

| Field | Value |
| --- | --- |
| **Update Rate** | 10000ms (10 seconds) |
| **Payload Length** | 12 bytes |

**Payload Structure:**
```
Byte 0: Sub-command (0x04)
Byte 1: Odometer LSB
Byte 2: Odometer Middle byte
Byte 3: Odometer MSB
Byte 4: 0xF2 (existing constant, meaning unverified)
Byte 5: 0x08 (existing constant, meaning unverified)
Bytes 6-8: Trip 1, currently 0x00 (unavailable)
Bytes 9-11: Trip 2, currently 0x00 (unavailable)
```

---

### 4. Outside Temperature (Command 0x28)

| Field | Value |
| --- | --- |
| **Update Rate** | 5000ms |
| **Payload Length** | 12 bytes |

**Payload Structure:**
```
Byte 0: Encoded coolant temperature = (coolant + 40) × 2, unverified hypothesis
Bytes 1-4: 0x00
Byte 5: Encoded temperature = (temp + 40) × 2
Bytes 6-11: 0x00 (reserved)
```

**Example:** 20°C → (20 + 40) × 2 = 120 → 0x78

Coolant at byte 0 is an existing implementation hypothesis, not a confirmed
Raise field. Its presence does not establish that the head unit displays
coolant temperature.

Both encoded temperatures are cast to `uint8_t` without clamping. Values
outside 0–255 wrap on the wire; for example, coolant at 100°C encodes as
`280 mod 256 = 24`. This limits the current temperature packet independently
of the OBD reference, which retains the decoded Celsius value.

---

### 5. Fuel Consumption (Command 0x22)

| Field | Value |
| --- | --- |
| **Update Rate** | 1000ms |
| **Payload Length** | 3 bytes |

**Payload Structure:**
```
Byte 0: Unit (0x02 = L/100km)
Byte 1: Value MSB
Byte 2: Value LSB
```

**Formula:** `consumption_L100km = (Byte1 << 8 | Byte2) / 10.0`

**Example:** 7.5 L/100km → 75 → 0x004B → Payload: `02 00 4B`

This is the UART encoding when an available source provides 0.1 L/100km.
The Juke preset computes an estimate from successive modulo-256 differences
of `0x551 B1` and distance integrated from vehicle speed. The raw byte is
never sent directly as L/100km. Its `fuelConsumption` configuration uses
a provisional 80 mm³ per increment, based on a
[Clio III calibration](https://x0r.fr/blog/39#comment-396), and must be calibrated
on the Juke MR16DDT before treating the result as a measured consumption.
The VBOX label and factor 1 do not independently validate this conversion.

At a confirmed stationary state, the Juke conversion sends zero for the
L/100km display, matching the Nissan cluster convention documented in
[MWI, page 16](../vehicle/nissan-juke-f15/MWI.pdf). Fuel consumed while stationary
still contributes to the trip total. This display convention does not mean
the engine uses no fuel. When the input data is unavailable or stale, no
instantaneous packet is sent: Raise does not document an invalid marker
for `0x22`. No L/h unit is defined for this command.

Mock mode retains synthetic consumption output. Head-unit display of
instantaneous consumption requires IG ON according to Raise V2.21 page 5.

---

### 5b. Current Trip / Historical Fuel Consumption (Command 0x23)

| Field | Value |
| --- | --- |
| **Update Rate** | 5000ms |
| **Payload Length** | 13 bytes |

**Payload Structure:**
```
Byte 0: Unit (0x02 = L/100km)
Bytes 1–2: Current-trip consumption, uint16 BE
Bytes 3–4: Historical trip 1, uint16 BE
Bytes 5–6: Historical trip 2, uint16 BE
Bytes 7–8: Historical trip 3, uint16 BE
Bytes 9–10: Historical trip 4, uint16 BE
Bytes 11–12: Historical trip 5, uint16 BE
```

**Formula:** `consumption_L100km = (Byte1 << 8 | Byte2) / 10.0`

**Invalid value:** `0xFFFF` independently marks each unavailable trip.

**Example:** 8.2 L/100km → 82 → `0x0052`, with five unknown historical trips:
`02 00 52 FF FF FF FF FF FF FF FF FF FF`.

**Notes:**

- The payload follows Raise V2.21 page 5. The former three-byte implementation
  omitted required historical fields and has been replaced.
- The Juke conversion calculates current-trip consumption from total fuel
  divided by total distance. This is a session estimate using the same
  provisional factor as instantaneous consumption, not the Nissan cluster's
  stored average. It does not restore the unvalidated `0x580 B4` mapping.
- `RadioSend.cpp` only encodes the supplied average. It does not calculate or
  persist trips; all five historical values are `0xFFFF`.
- When derived consumption is enabled but its average is unavailable,
  the current-trip value is also `0xFFFF`. Profiles without derived
  consumption and without an available direct source emit no `0x23` packet.
- The specification gates current-trip consumption display on IG ON. The
  firmware supplies the RPM-derived IG proxy described under `0x24`.

**Synthetic wire example with an available source:** 9.8 L/100km (`98`) produces the full
17-byte frame `2E 23 0D 02 00 62 FF FF FF FF FF FF FF FF FF FF 75`.
This example verifies encoding,
not the accuracy of the source CAN value or acceptance by DUDU7.

---

### 6. Trip Info / Remaining Range (Command 0x21)

| Field | Value |
| --- | --- |
| **Update Rate** | 5000ms |
| **Payload Length** | 7 bytes |

**Payload Structure:**
```
Byte 0: Average Speed MSB (0.1 km/h units)
Byte 1: Average Speed LSB
Byte 2: Elapsed Time MSB (seconds)
Byte 3: Elapsed Time LSB
Byte 4: Range MSB (km)
Byte 5: Range LSB
Byte 6: Unit (0x02 = km)
```

**Formulas:**
- `average_speed_kmh = (Byte0 << 8 | Byte1) / 10.0`
- `elapsed_time_sec = (Byte2 << 8 | Byte3)`
- `range_km = (Byte4 << 8) | Byte5`

**Example:** 45.0 km/h avg, 3600s elapsed, 540 km range → Payload: `01 C2 0E 10 02 1C 02`

---

## Timing Diagram

```
Time (ms)  0    200   333   400   500   600   666   800   1000
           │     │     │     │     │     │     │     │     │
Steering   ■─────■─────────────■─────────────────■─────────■
(0x29)     │                   │                 │
           │           │       │     │           │     │
RPM        ■───────────■───────────────■─────────────────■─
(0x7D/0A)              │             │
           │                         │                   │
Speed      ■─────────────────────────■─────────────────────■
(0x7D/03)
           │                   │                   │
Doors      ■───────────────────■───────────────────■───────
(0x24)     (or on change)

■ = Frame transmitted
```

---

## Data Flow

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│  Global     │ Read │  RadioSend   │ UART │  Android    │
│  Variables  │ ───► │  Module      │ ───► │  Head Unit  │
└─────────────┘      └──────────────┘      └─────────────┘
  currentSteer         Formats data         Displays:
  engineRPM            Calculates checksum  - Camera lines
  vehicleSpeed         Sends frames         - RPM gauge
  currentOdo                                - Speedometer
  currentDoors                              - Door status
  tempExt                                   - Temperature
  dteValue                                  - Remaining range
  averageSpeed                              - Average speed
  elapsedTime                               - Trip time
  fuelConsumptionInst                       - Instant consumption
  fuelConsumptionAvg                        - Average consumption
  headlightsOn                              - Lights status
  highBeamOn                                - Indicators
  parkingLightsOn
  indicatorLeft/Right
```

---

## Debug Notes

The protocol does not require a handshake - data is sent proactively. Any incoming data from the head unit is discarded.

`LOG ON` separately enables timestamped raw CAN logging and read-only OBD
service 01 references on USB; OBD diagnostic lines start with the exact
`ODB` prefix. `LOG OFF` stops new requests. Mock mode and OTA suspend OBD
polling. These references do not update GlobalData or the UART packets.
See the [USB LOG command](../protocols/USB_SERIAL_PROTOCOL.md#log---raw-can-capture-and-obd-reference-polling)
and [calibration guide](../OBD_CALIBRATION.md) for capture instructions.

---

## References

### Radio Protocol Documentation

- [Raise Toyota CAMRY/RAV4 V2.21](<../protocols/raise-toyota-rav4/睿志诚软件_丰田CAMRY & RAV4 串口通讯协议  V2.21.000 (1).pdf>) - Page 5 documents `0x23` and `0x24`; Git LFS download is needed to read the PDF.
- [Mikescotland / Toyota RAV4 Protocol Analysis](../protocols/raise-toyota-rav4/RAV4_BODY_ENGINE_2018_2020.pdf) - Compiled and analyzed by Mikescotland ([Source](https://forum.dudu-auto.com/u/Mikescotland)); page 4 shows an older one-byte door packet.
- [smartgauges / canbox](https://github.com/smartgauges/canbox)
- [cxsichen / Raise Protocol](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
