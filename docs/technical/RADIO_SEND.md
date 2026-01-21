# Radio Send - Technical Documentation

The `RadioSend.cpp` module communicates with the Android head unit MCU via the **Toyota RAV4 / Raise protocol** over UART.

## Commands Summary

| Command | Name | Length | Update Rate |
| --- | --- | --- | --- |
| 0x21 | Trip Info (Avg Speed, Time, Range) | 7 | 5000ms |
| 0x22 | Instantaneous Fuel Consumption | 3 | 1000ms |
| 0x23 | Average Fuel Consumption | 3 | 5000ms |
| 0x24 | Door Status | 1 | 250ms |
| 0x28 | Outside Temperature | 12 | 5000ms |
| 0x29 | Steering Wheel Angle | 2 | 200ms |
| 0x7D/01 | Lights & Indicators | 2 | 200ms |
| 0x7D/03 | Vehicle Speed | 5 | 500ms |
| 0x7D/04 | Odometer | 12 | 10000ms |
| 0x7D/0A | Engine RPM | 3 | 333ms |

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
Checksum = (Command + Length + Sum(Payload bytes)) XOR 0xFF
```

---

## Implemented Commands

### 1. Steering Wheel Angle (Command 0x29)

| Field | Value |
| --- | --- |
| **Update Rate** | 200ms |
| **Payload Length** | 2 bytes |
| **Format** | Little Endian signed 16-bit |
| **Unit** | degrees |

**Payload Structure:**
```
Byte 0: Angle LSB
Byte 1: Angle MSB
```

**Notes:**
- Range: -540 to +540 (full lock to full lock)
- Sign is inverted from Nissan to match Toyota convention
- Used for reverse camera dynamic guidelines

---

### 2. Door Status (Command 0x24)

| Field | Value |
| --- | --- |
| **Update Rate** | 250ms or on change |
| **Payload Length** | 1 byte |

**Payload Structure:**
```
Byte 0: Door status bitmask
```

**Status Bitmask:**
| Bit | Meaning | Values |
| --- | --- | --- |
| 7 | Driver door (Front Left) | 0=closed, 1=open |
| 6 | Passenger door (Front Right) | 0=closed, 1=open |
| 5 | Rear Right door | 0=closed, 1=open |
| 4 | Rear Left door | 0=closed, 1=open |
| 3 | Trunk/Boot | 0=closed, 1=open |

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
Byte 3: 0x00 (reserved)
Byte 4: 0x00 (reserved)
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
Bytes 4-11: 0x00 (reserved)
```

---

### 4. Outside Temperature (Command 0x28)

| Field | Value |
| --- | --- |
| **Update Rate** | 5000ms (optional) |
| **Payload Length** | 12 bytes |

**Payload Structure:**
```
Bytes 0-4: 0x00 (reserved)
Byte 5: Encoded temperature = (temp + 40) × 2
Bytes 6-11: 0x00 (reserved)
```

**Example:** 20°C → (20 + 40) × 2 = 120 → 0x78

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
- Same format as 0x22 but for trip average consumption
- Data from Nissan CAN 0x580 byte[4]

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
  engineRPM            Calculates CRC       - Camera lines
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

---

## References

### Radio Protocol Documentation
- [smartgauges / canbox](https://github.com/smartgauges/canbox)
- [cxsichen / Raise Protocol](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
