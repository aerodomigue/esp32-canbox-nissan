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
Checksum = (Command + Length + Sum(Payload bytes)) XOR 0xFF
```

> Based on official Raise VW Polo Protocol v1.4 specification.

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
| 1-2 | Engine RPM | Big Endian uint16 | RPM = Data1 × 256 + Data2 |
| 3-4 | Vehicle Speed | Big Endian uint16 | Speed (km/h) = (Data3 × 256 + Data4) × 0.01 |
| 5-6 | Battery Voltage | Big Endian uint16 | Voltage (V) = (Data5 × 256 + Data6) × 0.01 |
| 7-8 | Temperature | Big Endian int16 | Temp (°C) = (Data7 × 256 + Data8) × 0.1 |
| 9-11 | Odometer | Big Endian uint24 | Distance (km) = Data9 × 65536 + Data10 × 256 + Data11 |
| 12 | Fuel Level | uint8 | Remaining fuel in liters |

---

### 3. Door Status (Command 0x41, Sub-command 0x01)

| Field | Value |
| --- | --- |
| **Update Rate** | On change only |
| **Payload Length** | 2 bytes |
| **Sub-command** | 0x01 (Door/safety status) |

**Payload Structure:**

| Byte | Content | Format | Notes |
| --- | --- | --- | --- |
| 0 | Sub-command | 0x01 | Door mode |
| 1 | Status Bitmask | See below | VW format |

**Status Bitmask (Byte 1) - VW Format:**
| Bit | Meaning | Values |
| --- | --- | --- |
| 0 | Front Left door (Driver) | 0=closed, 1=open |
| 1 | Front Right door (Passenger) | 0=closed, 1=open |
| 2 | Rear Left door | 0=closed, 1=open |
| 3 | Rear Right door (if present) | 0=closed, 1=open |
| 4 | Trunk | 0=closed, 1=open |
| 5 | Handbrake | 0=released, 1=applied |
| 6 | Washer fluid level | 0=normal, 1=low (not available from CAN) |
| 7 | Driver seat belt | 0=fastened, 1=not fastened (not available from CAN) |

---

### 4. Handshake (HOST → SLAVE)

The head unit (HOST) sends commands to establish/maintain the communication link. The ESP32 (SLAVE) must respond appropriately.

**Start/End Connection (0x81):**
| Data0 | Meaning | Response |
| --- | --- | --- |
| 0x01 | Start connection | ACK (0xFF) |
| 0x00 | End connection | ACK (0xFF) |

**Request Control Information (0x90):**
- Data0 specifies which data type to send (0x14, 0x16, 0x21, 0x24, 0x25, 0x26, 0x30, 0x41)
- SLAVE responds by sending the requested data type proactively

**ACK/NACK Response:**
| Value | Meaning |
| --- | --- |
| 0xFF | ACK (frame OK) |
| 0xF0 | NACK (checksum error) |

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
