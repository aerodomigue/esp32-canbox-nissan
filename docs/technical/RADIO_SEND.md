# Radio Send - Technical Documentation

The `RadioSend.cpp` module communicates with the Android head unit MCU via the **Toyota RAV4 / Raise protocol** over UART.

This guide describes the current implementation, including its unresolved
differences from the protocol documents. The documented 13-byte `0x23` payload
and proposed third `0x24` byte `0x06` are not implemented. OBD calibration runs on the
USB logging path and does not replace the preset's values sent over UART.

## Commands Summary

| Command | Name | Length | Update Rate |
| --- | --- | --- | --- |
| 0x21 | Trip Info (Avg Speed, Time, Range) | 7 | 5000ms |
| 0x22 | Instantaneous Fuel Consumption | 3 | 1000ms |
| 0x23 | Average Fuel Consumption | 3 | 5000ms |
| 0x24 | Door Status + IG Proxy | 2 | 250ms |
| 0x28 | Outside Temperature | 12 | 5000ms |
| 0x29 | Steering Wheel Angle | 2 | 200ms |
| 0x7D/01 | Lights & Indicators | 2 | 200ms |
| 0x7D/03 | Vehicle Speed | 5 | 500ms |
| 0x7D/04 | Odometer | 12 | 10000ms |
| 0x7D/0A | Engine RPM | 3 | 333ms |

`0x22` and `0x23` require available consumption sources in the stated UART
units. They are not emitted by the corrected Juke preset, which has no
validated L/100km source. Mock mode retains its synthetic output.

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
| **Format** | Little Endian signed 16-bit |
| **Unit** | 0.1 degrees |

**Payload Structure:**
```
Byte 0: Angle LSB
Byte 1: Angle MSB
```

**Notes:**
- Range: -540 to +540 (full lock to full lock)
- Scale, offset and sign inversion come from ConfigManager settings
- Used for reverse camera dynamic guidelines

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
The corrected Nissan Juke preset has no such source: its former `0x580`
byte 3 counter mapping was removed, so it does not send `0x22`. Mock mode
retains synthetic consumption output. `0x551` byte 1 is exposed only as a
raw counter and is never passed to this encoder as L/100km.

---

### 5b. Average Fuel Consumption (Command 0x23)

| Field | Value |
| --- | --- |
| **Update Rate** | 5000ms |
| **Payload Length** | 3 bytes |

**Payload Structure:**
```
Byte 0: Unit (0x02 = L/100km)
Byte 1: Value MSB
Byte 2: Value LSB
```

**Formula:** `consumption_L100km = (Byte1 << 8 | Byte2) / 10.0`

**Example:** 8.2 L/100km → 82 → 0x0052 → Payload: `02 00 52`

**Notes:**

- When an available average-consumption source exists, the current sender
  uses the same three-byte shape as `0x22`. The corrected Juke preset removes
  unvalidated `0x580` byte 4 and does not send `0x23`; mock mode still does.
- No average is computed or persisted by `RadioSend.cpp`, and no historical
  trip values are appended to the packet.
- Raise V2.21 page 5 specifies **13 payload bytes** for `0x23`: unit, current
  trip consumption and five historical values, each BE uint16. `0xFFFF`
  denotes an invalid value. The current three-byte implementation differs
  from that document; the 13-byte format is not transmitted.
- The specification gates current-trip consumption display on IG ON. The
  firmware supplies only the RPM-derived IG proxy described under `0x24`;
  head-unit display of this three-byte `0x23` packet is not guaranteed.

**Synthetic wire example with an available source:** 9.8 L/100km (`98`) produces the full
seven-byte frame `2E 23 03 02 00 62 75`. This example verifies encoding,
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
