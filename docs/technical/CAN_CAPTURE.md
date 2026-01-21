# CAN Capture - Technical Documentation

This module handles passive listening on the Nissan Juke F15 **High Speed CAN bus** (Cabin network). It decodes binary vehicle frames into normalized variables for use by the rest of the system.

---

## Physical Specifications

| Parameter | Value |
| --- | --- |
| **Bus Speed** | 500 kbps |
| **ID Format** | 11-bit (Standard CAN 2.0A) |
| **Wiring** | Twisted pair (CAN-H / CAN-L) |
| **Access Point** | OBD-II port: Pin 6 (CAN-H), Pin 14 (CAN-L) |

---

## Decoded CAN Frames

The `CanCapture.cpp` module processes frames in real-time using the following identifiers:

### 1. Steering & Dynamics

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x002** | Steering Angle | [1-2] | `(int16)(Data[1]<<8 \| Data[2])` | 0.1° | Signed, Big Endian. Center ≈ 2912 |
| **0x180** | Engine RPM | [0-1] | `(uint16)(Data[0]<<8 \| Data[1]) / 7` | RPM | Scale factor ~0.14 (calibrated) |
| **0x284** | Vehicle Speed | [0-1] | `(uint16)(Data[0]<<8 \| Data[1]) / 100` | km/h | Wheel speed sensor |

### 2. Instrument Cluster (CAN ID 0x5C5)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[0]** | Fuel Level | `map(Data[0], 255, 0, 0, 45)` | Inverted scale (255=empty, 0=full) mapped to 0-45L (Juke F15 tank capacity) |
| **[1-3]** | Odometer | `(Data[1]<<16 \| Data[2]<<8 \| Data[3])` | Total km (24-bit) |

### 3. Power Management

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x6F6** | Battery Voltage | [0] | `Data[0] * 0.1` | V | Alternator output (e.g., 141 = 14.1V) |
| **0x551** | Coolant Temp | [0] | `Data[0] - 40` | °C | Used as exterior temp substitute |

### 4. Body Control Module (CAN ID 0x60D)

Bytes [0-2] form a **24-bit status word** containing doors, lights, and indicators:

```cpp
uint32_t status = (Data[0] << 16) | (Data[1] << 8) | Data[2];
```

**Status Word Bit Mapping:**

| Bit | Signal | Notes |
| --- | --- | --- |
| 11 | High Beam | 1 = on |
| 13 | Left Indicator | Pulse when active |
| 14 | Right Indicator | Pulse when active |
| 17 | Headlights (low beam) | 1 = on |
| 18 | Parking Lights | 1 = on |
| 19 | Passenger Door | 1 = open |
| 20 | Driver Door | 1 = open |
| 21 | Rear Left Door | 1 = open |
| 22 | Rear Right Door | 1 = open |
| 23 | Boot/Trunk | 1 = open |

**Indicator Detection:**
- Indicators pulse on CAN only when the blinker relay is active
- A 500ms timeout is used to detect if indicator is currently blinking
- Timestamps are recorded for each pulse to track indicator state

**Output Remapping (for Toyota RAV4 protocol):**

*Doors* → `currentDoors` bitmask:
- Bit 20 → 0x80 (Driver)
- Bit 19 → 0x40 (Passenger)
- Bit 21 → 0x20 (Rear Left)
- Bit 22 → 0x10 (Rear Right)
- Bit 23 → 0x08 (Boot)

*Lights* → Individual boolean variables:
- `headlightsOn`, `highBeamOn`, `parkingLightsOn`
- `lastLeftIndicatorTime`, `lastRightIndicatorTime` (timestamps)

### 5. Trip Computer (CAN ID 0x54C)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[4-5]** | Distance to Empty | `(Data[4]<<8 \| Data[5])` | Estimated range in km |

### 6. Fuel Consumption (CAN ID 0x580)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[1]** | Instant. Consumption | `Data[1] * 10` | 0.1 L/100km units (needs calibration) |
| **[4]** | Average Consumption | `Data[4] * 10` | 0.1 L/100km units (needs calibration) |

**Note:** Alternative ID 0x358 byte[1] may also contain consumption data. See CanCapture.cpp comments.

---

## Software Implementation

The capture relies on the **ESP32-TWAI-CAN** library.

### Activity Monitoring

- **Heartbeat**: The steering frame (0x002) is continuously emitted by the power steering system. The LED on GPIO 8 toggles on each reception, confirming that the SN65HVD230 transceiver is working properly.

### Non-blocking Design

- The `handleCanCapture()` function processes one frame at a time to avoid blocking the main loop.
- All decoded values are stored in global variables (defined in `GlobalData.cpp`) for access by the radio transmission module.

### Debug Logging

When connected via USB Serial (`pio device monitor`), the system outputs each received CAN frame:

```
RX ID: 0x180 | DLC: 8 | Data: 00 00 45 6A 7A 00 32 10
RX ID: 0x284 | DLC: 8 | Data: 00 00 00 2D 00 00 D6 5C
RX ID: 0x60D | DLC: 8 | Data: 08 06 00 00 00 00 00 00
```

---

## Data Flow

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│  Nissan     │ CAN  │  ESP32       │ Vars │  RadioSend  │
│  ECUs       │ ───► │  CanCapture  │ ───► │  Module     │
└─────────────┘      └──────────────┘      └─────────────┘
     0x002            currentSteer
     0x180            engineRPM              Reads global
     0x284            vehicleSpeed           variables
     0x5C5            fuelLevel, currentOdo  Sends to radio
     0x551            tempExt
     0x60D            currentDoors
     0x6F6            headlightsOn, highBeamOn
     0x54C            parkingLightsOn
                      indicatorLeft/Right
                      dteValue, voltBat
```

---

## References

### Nissan CAN Reverse Engineering
- [NICOclub / Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals)
- [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
- [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
- [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)
