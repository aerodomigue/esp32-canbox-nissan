# Radio Send - Technical Documentation

The `RadioSend.cpp` module communicates with the Android head unit MCU via the **VW Polo / Raise (RZC) protocol** over UART.

---

## Serial Specifications

| Parameter | Value |
| --- | --- |
| **Baud Rate** | 38400 bps |
| **Format** | 8N1 (8 data bits, No parity, 1 Stop bit) |
| **TX Pin** | GPIO 5 (ESP32 → Radio RX) |
| **RX Pin** | GPIO 6 (Radio TX → ESP32, for handshake) |

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
Checksum = 0xFF - (Command + Length + Sum(Payload bytes))
```

> Based on Junsun/Raise PSA documentation.

---

## Implemented Commands

### 1. Steering Wheel Angle (Command 0x26)

| Field | Value |
| --- | --- |
| **Update Rate** | 100ms |
| **Payload Length** | 2 bytes |
| **Format** | Little Endian signed 16-bit |
| **Unit** | 0.1 degrees |

**Payload Structure:**
```
Byte 0: Angle LSB
Byte 1: Angle MSB
```

**Notes:**
- Sign is inverted from Nissan to match VW convention
- Used for reverse camera dynamic guidelines
- If guidelines rotate wrong direction, remove the sign inversion in code

---

### 2. Dashboard Data (Command 0x41, Sub-command 0x02)

| Field | Value |
| --- | --- |
| **Update Rate** | 400ms |
| **Payload Length** | 13 bytes |
| **Sub-command** | 0x02 (Dashboard gauges) |

**Payload Structure:**

| Byte | Content | Format | Notes |
| --- | --- | --- | --- |
| 0 | Sub-command | 0x02 | Dashboard mode |
| 1-2 | Engine RPM | Big Endian uint16 | Direct RPM value |
| 3-4 | Vehicle Speed | Little Endian uint16 | km/h × 100 |
| 5-6 | Battery Voltage | Big Endian uint16 | Volts × 100 |
| 7-8 | Temperature | Big Endian int16 | °C × 10 |
| 9-10 | Reserved | 0x00 | |
| 11 | Status Flags | Bitmask | See below |
| 12 | Fuel Level | uint8 | Liters (0-45 VW scale) |

**Status Flags (Byte 11):**
| Bit | Meaning |
| --- | --- |
| 0 | Handbrake engaged |
| 2 | Engine running / Ignition on |

---

### 3. Door Status (Command 0x41, Sub-command 0x01)

| Field | Value |
| --- | --- |
| **Update Rate** | On change only |
| **Payload Length** | 13 bytes |
| **Sub-command** | 0x01 (Door status) |

**Payload Structure:**

| Byte | Content | Format | Notes |
| --- | --- | --- | --- |
| 0 | Sub-command | 0x01 | Door mode |
| 1 | Door Bitmask | See below | VW format |
| 2-12 | Reserved | 0x00 | |

**Door Bitmask (Byte 1) - VW Format:**
| Bit | Door |
| --- | --- |
| 0 | Front Left (Driver) |
| 1 | Front Right (Passenger) |
| 2 | Rear Left |
| 3 | Rear Right |
| 4 | Trunk |

---

### 4. Handshake Responses

The head unit periodically sends initialization requests. The ESP32 must respond to maintain the communication link.

**Version Query (0xC0 or 0x08):**
- Response Command: 0xF1
- Payload: `{0x02, 0x08, 0x10}` (Firmware version identifier)

**Status Query (0x90):**
- Response Command: 0x91
- Payload: `{0x41, 0x02}` (Status OK)

---

## Timing Diagram

```
Time (ms)  0    100   200   300   400   500   600   700   800
           │     │     │     │     │     │     │     │     │
Steering   ■─────■─────■─────■─────■─────■─────■─────■─────■
(0x26)     │     │     │     │     │     │     │     │     │
           │                 │                 │           
Dashboard  ■─────────────────■─────────────────■───────────
(0x41/02)                                                  
           │                                               
Doors      ■ (only on change)
(0x41/01)  

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
  vehicleSpeed         Sends frames         - Dashboard
  fuelLevel                                 - Door status
  voltBat
  tempExt
  currentDoors
```

---

## Debug Output

When connected via USB Serial, the module outputs transmission status every second:

```
>>> RADIO TX STATUS >>>
RPM: 2500 | Fuel: 32%
VW Steer: 150 | VW Doors: 0x00
VW Bat Raw: 1420 | VW Temp Raw: 850
-----------------------
```

---

## References

### Radio Protocol Documentation
- [smartgauges / canbox](https://github.com/smartgauges/canbox)
- [cxsichen / Raise Protocol](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
