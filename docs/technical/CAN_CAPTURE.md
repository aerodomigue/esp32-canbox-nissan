# CAN Capture - Technical Documentation

This module handles passive listening on the vehicle **High Speed CAN bus** (Cabin network). It delegates frame processing to the `CanConfigProcessor`, which uses JSON configuration files to decode binary vehicle frames into normalized variables.

---

## Physical Specifications

| Parameter | Value |
| --- | --- |
| **Bus Speed** | 500 kbps |
| **ID Format** | 11-bit (Standard CAN 2.0A) |
| **Wiring** | Twisted pair (CAN-H / CAN-L) |
| **Access Point** | OBD-II port: Pin 6 (CAN-H), Pin 14 (CAN-L) |

---

## Architecture

Since v1.6, CAN frame decoding is **fully configurable via JSON files**. The `CanCapture.cpp` module is a thin wrapper that:

1. Receives CAN frames from the TWAI driver
2. Delegates processing to `CanConfigProcessor.processFrame()`
3. Toggles the LED on steering frame (0x002) for activity indication
4. Logs frames to serial if `LOG ON` is active

The actual decoding logic (byte extraction, formulas, target mapping) is defined in JSON configuration files stored on LittleFS. See [Vehicle Preset Guide](../VEHICLE_PRESET_GUIDE.md) for the JSON format.

---

## Decoded CAN Frames (Nissan Juke F15)

The following table describes the CAN frames decoded by the default `NissanJukeF15.json` configuration:

### 1. Steering & Dynamics

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x002** | Steering Angle | [1-2] | NONE (raw INT16) | 0.1° | Signed, Big Endian. Center offset applied by ConfigManager |
| **0x180** | Engine RPM | [0-1] | SCALE [1, 7, 0] | RPM | `raw / 7` |
| **0x284** | Vehicle Speed | [0-1] | SCALE [1, 100, 0] | km/h | `raw / 100` (wheel speed sensor) |

### 2. Instrument Cluster (CAN ID 0x5C5)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[0]** | Fuel Level | MAP_RANGE [255, 0, 0, 45] | Inverted scale (255=empty, 0=full) mapped to 0-45L |
| **[1-3]** | Odometer | NONE (UINT24) | Total km (24-bit Big Endian) |

### 3. Power Management

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x6F6** | Battery Voltage | [0] | NONE (UINT8) | 0.1V | Raw value in decivolts (e.g., 141 = 14.1V). Converted to float by `writeToGlobalData()` |
| **0x551** | Coolant Temp | [0] | SCALE [1, 1, -40] | °C | Used as exterior temp substitute |

### 4. Body Control Module (CAN ID 0x60D)

Bytes [0-2] form a **24-bit status word** (Big Endian BITMASK). Individual bits are extracted using `BITMASK_EXTRACT`:

| Bit | Mask (decimal) | Signal | Notes |
| --- | --- | --- | --- |
| 11 | 2048 | High Beam | 1 = on |
| 13 | 8192 | Left Indicator | Pulse when active |
| 14 | 16384 | Right Indicator | Pulse when active |
| 17 | 131072 | Headlights (low beam) | 1 = on |
| 18 | 262144 | Parking Lights | 1 = on |
| 19 | 524288 | Passenger Door | 1 = open |
| 20 | 1048576 | Driver Door | 1 = open |
| 21 | 2097152 | Rear Left Door | 1 = open |
| 22 | 4194304 | Rear Right Door | 1 = open |
| 23 | 8388608 | Boot/Trunk | 1 = open |

**Indicator Detection:**
- Indicators pulse on CAN only when the blinker relay is active
- A configurable timeout (default 500ms) is used to detect if indicator is currently blinking
- Timestamps are recorded for each pulse to track indicator state

**Output Mapping:**

*Doors* → `currentDoors` bitmask (Toyota RAV4 format):
- Bit 20 (Driver) → 0x80
- Bit 19 (Passenger) → 0x40
- Bit 21 (Rear Left) → 0x20
- Bit 22 (Rear Right) → 0x10
- Bit 23 (Boot) → 0x08

*Lights* → Individual boolean variables:
- `headlightsOn`, `highBeamOn`, `parkingLightsOn`
- `lastLeftIndicatorTime`, `lastRightIndicatorTime` (timestamps)

### 5. Trip Computer (CAN ID 0x54C)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[6-7]** | Distance to Empty | SCALE [100, 283, 0] | `raw * 100 / 283` → estimated range in km |

### 6. Fuel Consumption (CAN ID 0x580)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[2]** | Instant. Consumption | BITMASK_EXTRACT [127, 0] | Mask 0x7F, 0.1 L/100km units |
| **[4]** | Average Consumption | NONE (UINT8) | 0.1 L/100km units |

---

## Software Implementation

The capture relies on the **ESP32-TWAI-CAN** library.

### Boot Initialisation

Before any CAN frame is processed, `CanConfigProcessor::begin()` runs once during `setup()`:

```
1. LittleFS mounted
2. Vehicle JSON file located (NVS → /vehicle.json → /NissanJukeF15.json)
3. JSON parsed: name, isMock, vehicleParams, frames
4. Vehicle switch detection:
   ├─ Same file as previously loaded → NVS calibration preserved, vehicleParams skipped
   └─ Different file (or first boot) → configReset() → vehicleParams applied → configSave()
5. CAN frame processing begins
```

The `vehicleParams` section in the JSON file allows a preset to carry its own calibration defaults (steering scale, offsets, tank capacity, etc.). These override NVS on vehicle switch but never overwrite user customisations made via `CFG SET` on the same vehicle. See [Vehicle Parameters](../VEHICLE_PRESET_GUIDE.md#vehicle-parameters-vehicleparams) for the full key reference.

### Processing Pipeline

```
CAN Frame received
       │
       ▼
CanCapture.handleCanCapture()
       │
       ├─► CanConfigProcessor.processFrame()
       │       │
       │       ├─► findFrameConfig(canId)     → lookup JSON config
       │       ├─► extractRawValue()           → read bytes (BE/LE)
       │       ├─► applyFormula()              → SCALE / MAP_RANGE / BITMASK
       │       └─► writeToGlobalData()         → update global variables
       │
       ├─► LED toggle on 0x002 (heartbeat)
       │
       └─► Serial log if LOG ON
```

### Activity Monitoring

- **Heartbeat**: The steering frame (0x002) is continuously emitted by the power steering system. The LED on GPIO 8 toggles on each reception, confirming that the SN65HVD230 transceiver is working properly.

### Non-blocking Design

- The `handleCanCapture()` function processes one frame at a time to avoid blocking the main loop.
- All decoded values are stored in global variables (defined in `GlobalData.cpp`) for access by the radio transmission module.

### Debug Logging

When CAN logging is enabled (`LOG ON` via serial), the system outputs each received CAN frame:

```
RX 0x180 [8]: 00 00 45 6A 7A 00 32 10
RX 0x284 [8]: 00 00 00 2D 00 00 D6 5C
RX 0x60D [8]: 08 06 00 00 00 00 00 00
```

---

## Data Flow

```
┌─────────────┐      ┌──────────────────┐      ┌──────────────┐      ┌─────────────┐
│  Vehicle     │ CAN  │  CanCapture      │      │  CanConfig   │ Vars │  RadioSend  │
│  ECUs        │ ───► │  (thin wrapper)  │ ───► │  Processor   │ ───► │  Module     │
└─────────────┘      └──────────────────┘      └──────────────┘      └─────────────┘
     0x002                                       JSON config           Reads global
     0x180            Delegates to               defines how           variables
     0x284            processor                  to decode             Sends to radio
     0x5C5                                       each frame
     0x551
     0x60D            Output variables:
     0x6F6            currentSteer, engineRPM, vehicleSpeed,
     0x54C            fuelLevel, currentOdo, voltBat, tempExt,
     0x580            currentDoors, headlightsOn, highBeamOn,
                      parkingLightsOn, dteValue,
                      fuelConsumptionInst, fuelConsumptionAvg,
                      lastLeftIndicatorTime, lastRightIndicatorTime
```

---

## References

### Nissan CAN Reverse Engineering
- [NICOclub / Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals)
- [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
- [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
- [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)
