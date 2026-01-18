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
| **0x002** | Steering Angle | [0-1] | `(int16)(Data[0]<<8 \| Data[1])` | 0.1° | Signed, Big Endian. Center = 0 |
| **0x180** | Engine RPM | [0-1] | `(uint16)(Data[0]<<8 \| Data[1]) / 4` | RPM | Scale factor 0.25 |
| **0x284** | Vehicle Speed | [0-1] | `(uint16)(Data[0]<<8 \| Data[1]) / 100` | km/h | Wheel speed sensor |

### 2. Instrument Cluster (CAN ID 0x5C5)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[0]** | Fuel Level | Direct (0-100%) | Mapped to 0-45L for VW display |
| **[1-3]** | Odometer | `(Data[1]<<16 \| Data[2]<<8 \| Data[3])` | Total km (24-bit) |

### 3. Power Management

| CAN ID | Signal | Bytes | Formula | Unit | Notes |
| --- | --- | --- | --- | --- | --- |
| **0x6F6** | Battery Voltage | [0] | `Data[0] * 0.1` | V | Alternator output (e.g., 141 = 14.1V) |
| **0x551** | Coolant Temp | [0] | `Data[0] - 40` | °C | Used as exterior temp substitute |

### 4. Body Control Module (CAN ID 0x60D)

**Byte [0] - Door Status** (Bitmask):

| Bit | Door |
| --- | --- |
| 0 | Driver (Front Left) |
| 1 | Passenger (Front Right) |
| 2 | Rear Left |
| 3 | Rear Right |
| 4 | Trunk / Hatch |

The raw Nissan bits are remapped to a generic format for VW protocol compatibility:
- Nissan bit 0 → Internal bit 7 (Driver)
- Nissan bit 1 → Internal bit 6 (Passenger)
- Nissan bit 2 → Internal bit 5 (Rear Left)
- Nissan bit 3 → Internal bit 4 (Rear Right)
- Nissan bit 4 → Internal bit 3 (Trunk)

### 5. Trip Computer (CAN ID 0x54C)

| Byte | Signal | Formula | Notes |
| --- | --- | --- | --- |
| **[4-5]** | Distance to Empty | `(Data[4]<<8 \| Data[5])` | Estimated range in km |

---

## Software Implementation

The capture relies on the **ESP32-TWAI-CAN** library.

### Activity Monitoring

- **Heartbeat**: The steering frame (0x002) is continuously emitted by the power steering system. The LED on GPIO 8 toggles on each reception, confirming that the SN65HVD230 transceiver is working properly.

### Non-blocking Design

- The `handleCanCapture()` function processes one frame at a time to avoid blocking the main loop.
- All decoded values are stored in global variables (defined in `GlobalData.cpp`) for access by the radio transmission module.

### Debug Logging

When connected via USB Serial, the module outputs decoded values every second:

```
--- NISSAN DATA DECODED ---
RPM: 2500 | Speed: 45 | Volt: 14.2V | Temp: 85 C
Fuel: 32 L (VW scale) | Steer: -150
Doors Raw: 0x00
---------------------------
```

---

## Data Flow

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│  Nissan     │ CAN  │  ESP32       │ Vars │  RadioSend  │
│  ECUs       │ ───► │  CanCapture  │ ───► │  Module     │
└─────────────┘      └──────────────┘      └─────────────┘
     0x002                                     
     0x180           Decodes frames           Reads global
     0x284           Stores in globals        variables
     0x5C5                                    Sends to radio
     0x551
     0x60D
     0x6F6
     0x54C
```

---

## References

### Nissan CAN Reverse Engineering
- [NICOclub / Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals)
- [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
- [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
- [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)
