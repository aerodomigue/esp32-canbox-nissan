# CAN Capture - Technical Documentation

This module reads the vehicle **High Speed CAN bus**. It delegates frame processing to the `CanConfigProcessor`, which uses JSON configuration files to decode binary vehicle frames into normalized variables. With `LOG ON`, the main loop also sends supported OBD service 01 queries for reference measurements; `LOG OFF` stops new queries.

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

The following tables describe the corrected default `NissanJukeF15.json`.
RPM and speed changes follow the [captured CAN/OBD evidence](../JUKE_F15_CAN_MAPPING.md),
whose speed range remains limited. Steering, DTE, ambient temperature and
odometer retain their unvalidated legacy interpretations.

### 1. Steering & Dynamics

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x002** | Steering Angle | [1-2] | NONE (raw INT16) | Legacy angle assumption | Signed, Big Endian; scale, center and sign still to validate |
| **0x180** | Engine RPM | [0-1] | SCALE [1, 8, 0] | RPM | Integer division; corroborated by OBD |
| **0x180** | Brake pedal switch | [7] bit0 | BITMASK_EXTRACT [1, 0] | Raw boolean | Separate from parking brake; physical polarity needs checking |
| **0x284** | Vehicle Speed | [4-5] | SCALE [1, 100, 0] | km/h | Integer dashboard value; fractional value retained for consumption distance; corroborated at low speed |

### 2. Instrument Cluster (CAN ID 0x5C5)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[0] bit2** | Handbrake | BITMASK_EXTRACT [4, 2] | Observed 1 = applied; exposed in `SYS DATA` |
| **[1-3]** | Odometer | NONE (UINT24) | Legacy 24-bit Big Endian interpretation; unit unvalidated |

Whole-byte fuel decoding has been removed. `fuelLevel` remains zero for
compatibility while `canAuxData.fuelLevelValid` is false; this is an unavailable
measurement, not an empty tank.

### 3. Power Management

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x6F6** | Battery Voltage | [0] | NONE (UINT8) | 0.1V | Raw value in decivolts (e.g., 141 = 14.1V). Converted to float by `writeToGlobalData()` |
| **0x540** | External Temp | [0] | SCALE [1, 1, -40] | °C | Legacy ambient candidate; independent reference pending |
| **0x551** | Coolant Temp | [0] | SCALE [1, 1, -40] | °C | Corroborated by OBD over 61–67 °C |

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
| **[6-7]** | Distance to Empty | SCALE [100, 338, 0] | Initial `raw * 100 / 338` hypothesis; 0x2DE replacement rejected |

### 6. Fuel counter and derived consumption (CAN ID 0x551)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[1]** | `FUEL_COUNTER_RAW` | NONE (UINT8) | Raw 0–255, exposed unchanged as `FuelRaw`; increments feed the provisional consumption estimator |

The former `0x580` instantaneous and average-consumption fields are absent
from the Juke preset. Its root-level `fuelConsumption` settings instead
convert modulo-256 increments of `0x551 B1` using **80 mm³/increment,
provisional and not calibrated on this Juke**, and distance integrated from
the fractional `0x284 B4–B5 /100` speed. The resulting radio value is
`volume_mm3 / distance_m`, in 0.1 L/100 km.

The instantaneous window is 1 s, with a zero display below 2 km/h when both
sources are fresh. Average consumption includes stationary fuel use and
starts after 100 m in the current session. Reload, reboot, a source gap
over 500 ms or an implausible counter increment reinitialises the session.
`FuelConsEstimated: 1` distinguishes the provisional coefficient from
`FuelConsValid`, which only describes availability. UART `0x22` is omitted
when instantaneous consumption is unavailable; `0x23` sends `0xFFFF` for
an unavailable average and its five unavailable historical values. Mock
output retains its synthetic sources. See the
[configuration and limitations](../VEHICLE_PRESET_GUIDE.md#counter-based-fuel-consumption-fuelconsumption)
and [radio packet format](RADIO_SEND.md).

---

## Software Implementation

The capture relies on the **ESP32-TWAI-CAN** library.

### Boot Initialisation

Before any CAN frame is processed, `CanConfigProcessor::begin()` runs once during `setup()`:

```
1. LittleFS mounted
2. Vehicle JSON file located (NVS → /vehicle.json → /NissanJukeF15.json)
3. JSON parsed: name, isMock, vehicleParams, fuelConsumption, frames
4. Vehicle switch detection:
   ├─ Same file as previously loaded → NVS calibration preserved, vehicleParams skipped
   └─ Different file (or first boot) → configReset() → vehicleParams applied → configSave()
5. CAN frame processing begins
```

The `vehicleParams` section in the JSON file allows a preset to carry its own calibration defaults (steering scale, offsets, tank capacity, etc.). These override NVS on vehicle switch but never overwrite user customisations made via `CFG SET` on the same vehicle. See [Vehicle Parameters](../VEHICLE_PRESET_GUIDE.md#vehicle-parameters-vehicleparams) for the full key reference.

### Processing Pipeline

```
main: readFrame(frame, 0), receivedAt = millis()
  │
  ├─► CanCapture.handleCanCapture(frame, receivedAt)
  │     ├─► CanConfigProcessor.processFrame()
  │     │     ├─► reject unsupported flags/IDs/DLC
  │     │     ├─► findFrameConfig(), validate ALL configured field lengths
  │     │     └─► extractRawValue(), applyFormula(), writeToGlobalData()
  │     ├─► LED toggle on 0x002
  │     └─► format unchanged frame: RX or ODB RAW_RX, if LOG ON
  │
  ├─► ObdCalibration.receive(frame, receivedAt)
  │     └─► ODB SUPPORTED / RX / NEGATIVE for matching pending queries
  │
  └─► ObdCalibration.tick(millis()), after draining the receive queue
        └─► discovery, supported service 01 queries and timeouts
```

### Activity Monitoring

- **Heartbeat**: The steering frame (0x002) is continuously emitted by the power steering system. The LED on GPIO 8 toggles on each reception, confirming that the SN65HVD230 transceiver is working properly.

### Non-blocking Design

- The main loop uses `readFrame(frame, 0)`, drains at most 32 frames per iteration and uses a 128-frame receive queue. USB output can still block; driver missed-frame counters are logged.
- Standard 11-bit data frames are decoded only after all configured fields pass DLC validation. Extended and RTR frames remain visible in the raw log without updating decoded vehicle data.
- All decoded values are stored in global variables (defined in `GlobalData.cpp`) for access by the radio transmission module.

### Debug Logging

When CAN logging is enabled (`LOG ON` via serial), the system outputs each received CAN frame:

```
ODB TX t_ms=1000 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=1008 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=1008 ext=0 rtr=0
ODB RX t_ms=1008 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

The example is synthetic. Payload bytes are unchanged; `t_ms` is software
dequeue time, not a hardware bus timestamp. All diagnostic records begin with
the requested `ODB` prefix. See [calibration capture and comparison](../OBD_CALIBRATION.md)
for capabilities discovery, units, timeouts and host analysis commands.

Logging is disabled at boot. `LOG OFF` stops capture and new OBD requests;
mock mode does not query ECUs, and OTA pauses polling. OBD references are
logged separately and never overwrite decoded vehicle data or the preset.

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
     0x540
     0x551
     0x60D            Output variables:
     0x6F6            currentSteer, engineRPM, vehicleSpeed,
     0x54C            currentOdo, voltBat, tempExt,
                      currentDoors, headlightsOn, highBeamOn,
                      parkingLightsOn, dteValue,
                      coolantTemp, canAuxData (states/raw counter/availability),
                      lastLeftIndicatorTime, lastRightIndicatorTime
```

---

## References

### Nissan CAN Reverse Engineering
- [NICOclub / Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals)
- [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
- [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
- [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)
