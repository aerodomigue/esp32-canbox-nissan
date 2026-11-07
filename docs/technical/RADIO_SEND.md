# Radio Send - Technical Documentation

The `RadioSend.cpp` module communicates with the Android head unit MCU via the **Toyota RAV4 / Raise protocol** over UART.

This guide describes the current implementation and its remaining protocol
limitations. Consumption uses the documented three-byte `0x22` and 13-byte
`0x23` payloads. Door command `0x24` keeps its documented two-byte payload.
OBD calibration runs on the USB logging path and does not replace the preset's
values sent over UART. See the [F15 boot-trip mapping guide](../F15_DUDU_BOOT_TRIP_MAPPING.md)
for vehicle sources, confidence and the explicit `radioReceiver: "DUDU"` contract.

## Commands Summary

| Command | Name | Length | Update Rate |
| --- | --- | --- | --- |
| 0x21 | Trip Info (Avg Speed, Time, Range) | 7 | 5000ms |
| 0x22 | Instantaneous Fuel Consumption | 3 | 1000ms |
| 0x23 | Current Trip / Historical Fuel Consumption | 13 | 5000ms |
| 0x24 | Door Status + IG Proxy | 2 | 250ms |
| 0x28 | Outside Temperature | 12 | 5000ms |
| 0x7D / 0x09 | Coolant + oil (DUDU/shared), coolant only (legacy FYT) | 4 / 3 | 5000ms, fresh sources only |
| 0x29 | Steering Wheel Angle | 2 | 200ms |
| 0x7D/01 | Lights & Indicators | 2 | 200ms |
| 0x7D/03 | Speed + boot average speed + metric unit | 6 | 500ms |
| 0x7D/04 | Odometer + two virtual boot-trip mirrors | 12 | 10000ms |
| 0x7D/0A | Engine RPM | 3 | 333ms |

`0x22` requires an available consumption value in the stated UART units.
The Juke preset's `fuelConsumption` configuration enables an experimental
conversion from the `0x551 B1` counter and vehicle speed. Its provisional
80 mm³ per increment factor is not calibrated for the Juke MR16DDT.
`0x23` sends the average since device boot only when available. Neither current
consumption value uses `0xFFFF` as a universal unavailable marker: DUDU displays
instantaneous `FFFF` as 6553.5 and ignores an average `FFFF`, while FYT maps
both to zero. Invalid current consumption is withheld; five historical slots
remain `FFFF`. Mock mode retains synthetic values in the same wire units.

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
  This was the starting calibration. On 15 October 2026, the owner reported
  that it turned the guidelines too far and confirmed scale **500**, keeping
  offset 0 and inversion: `clamp(-trunc(raw / 20), -500, 500)`. This is the
  calibration now specified by the repository JSON and reapplied on every
  load/reboot, taking priority over local NVS overrides.
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
| **Payload Length** | 6 bytes |

```text
D0: 03
D1..D2: speed U16LE in 0.01 km/h
D3..D4: boot average speed U16LE in 0.01 km/h
D5: 00, explicit metric unit for DUDU
```

The F15 preset decodes `0x280 B4..B5 / 100` and preserves fractional speed
internally. The previous LEN5 packet omitted D5, making DUDU consume the
checksum as a unit byte. Example: 93.21 km/h and 56.18 km/h average encode as
`2E 7D 06 03 69 24 F2 15 00 E5`. DUDU truncates these displays to integer km/h.

Only fresh speed is sent. With explicit DUDU selection, an incomplete boot
trip uses average `FFFF`: the original DUDU decoder ignores that field and
retains its old average while updating current speed. This is not a clearing
marker and is not applied to generic FYT profiles, which require a known mean.

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
Bytes 6-8: Virtual Trip A, U24LE in 0.1 km
Bytes 9-11: Virtual Trip B, U24LE in 0.1 km (same boot-trip value)
```

---

Both trip slots explicitly mirror distance integrated since device boot in
RAM. They are not the independent Nissan Trip A/B counters. The complete
packet requires a fresh odometer and a complete boot trip. DUDU may retain the
previous odometer when an incomplete trip prevents this compound packet from
being sent; no safe per-trip unavailable sentinel is known. DUDU truncates its
Trip A/B display to whole kilometres even though the wire uses 0.1 km.

### 4. Outside Temperature (Command 0x28)

| Field | Value |
| --- | --- |
| **Update Rate** | 5000ms |
| **Payload Length** | 12 bytes |

**Payload Structure:**
```
Bytes 0-4: 0x00 (unimplemented climate fields)
Byte 5: Encoded temperature = (temp + 40) × 2
Bytes 6-11: 0x00 (reserved)
```

**Example:** 20°C → (20 + 40) × 2 = 120 → 0x78

The previous coolant write in D0 has been removed: FYT `j0/c3` interprets
that byte as climate status bits. It never represented engine temperature.
The conversion is unchanged, but unrepresentable/sentinel values are rejected
before encoding instead of wrapping. Transmission requires a configured fresh
source. The incorrect F15 `0x540` mapping has been removed, so this packet is
not sent by the F15 profile. No replacement ambient value or artificial HVAC
state is invented from the single 14 °C photographed reference.

### 4b. Coolant and Oil (Command 0x7D, Sub-command 0x09)

The shared payload is **`09 CC CC OO`**, LEN4, with `CC = coolant_C + 40`
and `OO = oil_C + 40`; both are U8. Example: coolant 80 °C and oil 90 °C
produce **`2E 7D 04 09 78 78 82 03`**.

| Consumer | Coolant | Oil |
| --- | --- | --- |
| Classic FYT `j0/c3` | D2 − 40 °C | Not read in this branch |
| Supplied DUDU Base Toyota `m3` | D1 − 40 °C; raw0 ignored | D3 − 40 °C |

The sender duplicates coolant to serve both consumers. It requires **both
sources received and less than two seconds old**. No oil unavailable byte is
implemented by DUDU: raw0 means −40 °C and raw255 means215 °C. Oil must not be
replaced with coolant or an invented zero. If either source is absent/stale,
DUDU receives no temperature packet and may retain its previous values.

F15 coolant remains the OBD-correlated `0x551 B0 − 40`. Oil is the explicitly
selected **provisional** `0x580 B4 − 40` candidate. Its dynamics are coherent
with the captures, but physical identity and calibration remain unconfirmed;
`SYS DATA` marks its estimated status. UART itself has no provisional flag.

A fresh DUDU `m3` decoder initializes its wire-unit flag to Celsius. The flag
can be changed by `0x28 D4 bit0`; `7D/09` does not reset it. If the receiver
retained Fahrenheit from an earlier box, restart DUDU or reinitialize the RAV4
profile before checking temperatures. Sending a fabricated `0x28` merely to
force Celsius would also overwrite unrelated climate states, so it is avoided.
Temperature-card visibility depends separately on the installed RAV4 profile.

**Historical FYT-only path:** the former LEN3 payload `09 00 CC` was verified
against [`com.syu.ms`26.0507](https://github.com/PimpinPumpkin/fyt7862-mods/blob/4c545e945a1ef6e2faadae29d9a95e9c714822ca/artifacts/apps/com.syu.ms.apk),
SHA256 `6892e90067437c2a97cfb8e75396edade5d2a1772fd0ef7681d81c28f64fdbdc`.
`j0/c3.V2` reads unsigned D2, subtracts40 and emits `1049 [2, temperature_C]`.
That observation did not prove compatibility with the separate DUDU parser:
DUDU reads its retained checksum as the missing D3 oil byte. With coolant80 °C,
`2E 7D 03 09 00 78 FE` therefore produced a false214 °C oil reading. The updated
sender never sends this LEN3 form to an explicit DUDU receiver. It remains
available only for legacy non-DUDU profiles that have coolant but no oil source.

Both-byte encodings and receiver offsets are software-verified; this does not
establish every physical sensor or guarantee display in every head-unit layout.
See the [supplied-OS UART reference](../protocols/raise-toyota-rav4/README.md)
and [F15 source evidence](../JUKE_F15_CAN_MAPPING.md#engine-temperatures-to-the-head-unit).

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

**Historical unavailable value:** each history slot may be `0xFFFF`. Current
`0xFFFF` is not a shared unavailable/clear value for FYT and DUDU.

**Example:** 8.2 L/100km → 82 → `0x0052`, with five unknown historical trips:
`02 00 52 FF FF FF FF FF FF FF FF FF FF`.

**Notes:**

- The payload follows Raise V2.21 page 5. The former three-byte implementation
  omitted required historical fields and has been replaced.
- The Juke conversion calculates current-trip consumption from total fuel
  divided by total distance **since this device boot**. This RAM-only estimate
  uses the same provisional factor as instantaneous consumption, includes fuel
  consumed while stationary, and requires at least100 m. It is not the Nissan
  cluster's historical average. The oil candidate `0x580 B4` is never used as fuel.
- `RadioSend.cpp` only encodes the supplied average. It does not calculate or
  persist trips; all five historical values are `0xFFFF`.
- Unknown, stale or incomplete current averages produce no `0x23` packet.
  DUDU ignores current `FFFF`, retaining its previous mean; FYT converts it
  to zero. Neither behavior is a universal clearing contract.
- `LOG ON`/`LOG OFF` and identical JSON reloads do not reset this boot trip.
  A missing source interval or changed fuel/source calibration invalidates the
  full-boot mean rather than silently starting a shorter trip. Instantaneous
  output can recover. A device reboot starts a new trip; no history is persisted.
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
Byte 2: Elapsed Time MSB (MINUTES)
Byte 3: Elapsed Time LSB
Byte 4: Range MSB (km)
Byte 5: Range LSB
Byte 6: Unit (0x02 = km)
```

**Formulas:**
- `average_speed_kmh = (Byte0 << 8 | Byte1) / 10.0`
- `elapsed_time_minutes = (Byte2 << 8 | Byte3)`
- `range_km = (Byte4 << 8) | Byte5`

**Example:** 45.0 km/h average,60 minutes elapsed,540 km range →
payload `01 C2 00 3C 02 1C 02`.

Average speed is integrated distance divided by boot uptime, including stops.
Global `elapsedTime` is32-bit seconds; the sender divides by60 and saturates at
65534minutes rather than wrapping. FYT interprets this field as minutes; the
supplied DUDU `m3` ignores it, so correct encoding does not promise a duration
card in that UI.

The F15 DTE source is `0x2DE B6..B7 U16BE / 10`, replacing incorrect `0x54C`.
The compound packet requires fresh DTE and a complete boot mean for classic
FYT. Explicit DUDU permits `FFFF` for one missing companion: its original
parser ignores average speed ≥300 km/h or DTE ≥3000 km, preserving that field's
last value while updating the known companion. This is not a clear operation.
No unknown F15 DTE is substituted with zero.

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
  speedCentiKmh        Sends frames         - RPM gauge
  currentOdo                                - Speedometer
  currentDoors                              - Door status
  coolantTemp/oilTemperatureC                - Temperatures
  tempExt (valid source only)
  dteValue                                  - Remaining range
  averageSpeed                              - Average speed
  elapsedTime                               - Trip minutes (FYT; ignored by DUDU m3)
  fuelConsumptionInst                       - Instant consumption
  fuelConsumptionAvg                        - Average consumption
  headlightsOn                              - Lights status
  highBeamOn                                - Indicators
  parkingLightsOn
  indicatorLeft/Right
```

---

## Debug Notes

The firmware sends data periodically. Incoming UART requests are currently
drained; request/reply or a negotiation handshake is not implemented.

`LOG ON` separately enables timestamped raw CAN logging and read-only OBD
service01 references and bounded F15 manufacturer-session experiments on USB;
OBD diagnostic lines start with the exact `ODB` prefix. `LOG OFF`, mock mode
and OTA stop polling and perform the documented best-effort session close.
These references do not update GlobalData or the UART packets.
See the [USB LOG command](../protocols/USB_SERIAL_PROTOCOL.md#log---raw-can-capture-and-obd-reference-polling)
and [calibration guide](../OBD_CALIBRATION.md) for capture instructions.

---

## References

### Radio Protocol Documentation

- [Raise Toyota CAMRY/RAV4 V2.21](<../protocols/raise-toyota-rav4/睿志诚软件_丰田CAMRY & RAV4 串口通讯协议  V2.21.000 (1).pdf>) - Page 5 documents `0x23` and `0x24`; Git LFS download is needed to read the PDF.
- [Mikescotland / Toyota RAV4 Protocol Analysis](../protocols/raise-toyota-rav4/RAV4_BODY_ENGINE_2018_2020.pdf) - Compiled and analyzed by Mikescotland ([Source](https://forum.dudu-auto.com/u/Mikescotland)); page 4 shows an older one-byte door packet.
- [smartgauges / canbox](https://github.com/smartgauges/canbox)
- [cxsichen / Raise Protocol](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
