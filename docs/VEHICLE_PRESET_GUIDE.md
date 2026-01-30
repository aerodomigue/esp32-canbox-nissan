# Vehicle Preset Guide

How to create a CAN configuration for your vehicle.

---

## Overview

A vehicle preset is a JSON file that tells the ESP32 how to decode CAN frames from your car. Each vehicle has different CAN IDs and data formats, so you need a preset specific to your model.

---

## File Structure

```json
{
  "name": "Make Model Year",
  "isMock": false,
  "frames": [
    {
      "canId": "0x180",
      "fields": [
        {
          "target": "ENGINE_RPM",
          "startByte": 0,
          "byteCount": 2,
          "byteOrder": "BE",
          "dataType": "UINT16",
          "formula": "SCALE",
          "params": [1, 7, 0]
        }
      ]
    }
  ]
}
```

---

## Fields Reference

### Root Object

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Vehicle name (displayed in logs) |
| `isMock` | boolean | `true` = simulate data, `false` = read real CAN |
| `frames` | array | List of CAN frames to decode |

### Frame Object

| Field | Type | Description |
|-------|------|-------------|
| `canId` | string | CAN identifier in hex (e.g., `"0x180"`) |
| `fields` | array | Data fields to extract from this frame |

### Field Object

| Field | Type | Description |
|-------|------|-------------|
| `target` | string | Output variable (see [Targets](#targets)) |
| `startByte` | int | Starting byte index (0-7) |
| `byteCount` | int | Number of bytes to read (1-4) |
| `byteOrder` | string | `"BE"` (Big Endian) or `"LE"` (Little Endian) |
| `dataType` | string | How to interpret bytes (see [Data Types](#data-types)) |
| `formula` | string | Conversion formula (see [Formulas](#formulas)) |
| `params` | array | Formula parameters |

---

## Targets

Variables that can be updated from CAN data:

### Numeric Values

| Target | Description | Unit |
|--------|-------------|------|
| `STEERING` | Steering wheel angle | 0.1° |
| `ENGINE_RPM` | Engine speed | RPM |
| `VEHICLE_SPEED` | Speed | km/h |
| `FUEL_LEVEL` | Fuel in tank | Liters |
| `ODOMETER` | Total mileage | km |
| `VOLTAGE` | Battery voltage | 0.1V |
| `TEMPERATURE` | Temperature | °C |
| `DTE` | Distance to empty | km |
| `FUEL_CONS_INST` | Instant consumption | 0.1 L/100km |
| `FUEL_CONS_AVG` | Average consumption | 0.1 L/100km |

### Door Status (boolean)

| Target | Description |
|--------|-------------|
| `DOOR_DRIVER` | Driver door open |
| `DOOR_PASSENGER` | Passenger door open |
| `DOOR_REAR_LEFT` | Rear left door open |
| `DOOR_REAR_RIGHT` | Rear right door open |
| `DOOR_BOOT` | Trunk/boot open |

### Lights (boolean)

| Target | Description |
|--------|-------------|
| `INDICATOR_LEFT` | Left turn signal |
| `INDICATOR_RIGHT` | Right turn signal |
| `HEADLIGHTS` | Low beam |
| `HIGH_BEAM` | High beam |
| `PARKING_LIGHTS` | Parking lights |

---

## Data Types

| Type | Size | Range | Use Case |
|------|------|-------|----------|
| `UINT8` | 1 byte | 0 to 255 | Fuel level, voltage, temp |
| `INT8` | 1 byte | -128 to 127 | Signed single byte |
| `UINT16` | 2 bytes | 0 to 65535 | RPM, speed |
| `INT16` | 2 bytes | -32768 to 32767 | Steering angle |
| `UINT24` | 3 bytes | 0 to 16M | Odometer |
| `UINT32` | 4 bytes | 0 to 4B | Large values |
| `BITMASK` | variable | - | Extract specific bits |

---

## Formulas

### NONE

No conversion, use raw value as-is.

```json
"formula": "NONE"
```

### SCALE

Linear conversion: `result = (value × mult ÷ div) + offset`

```json
"formula": "SCALE",
"params": [mult, div, offset]
```

**Examples:**

```json
// RPM: raw / 7
"params": [1, 7, 0]

// Speed: raw / 100
"params": [1, 100, 0]

// Temperature: raw - 40
"params": [1, 1, -40]

// DTE: raw × 100 / 283
"params": [100, 283, 0]
```

### MAP_RANGE

Map value from one range to another: `map(value, inMin, inMax, outMin, outMax)`

```json
"formula": "MAP_RANGE",
"params": [inMin, inMax, outMin, outMax]
```

**Example:**

```json
// Fuel: 255=empty, 0=full → map to 0-45 liters
"params": [255, 0, 0, 45]
```

### BITMASK_EXTRACT

Extract specific bit(s): `result = (value & mask) >> shift`

```json
"formula": "BITMASK_EXTRACT",
"params": [mask, shift]
```

**Example:**

```json
// Extract bit 20 from 3-byte value (door status)
"params": [1048576, 20]
// 1048576 = 0x100000 = bit 20
```

---

## Byte Order

| Value | Description | Common In |
|-------|-------------|-----------|
| `BE` | Big Endian (MSB first) | Most vehicles (Nissan, Toyota...) |
| `LE` | Little Endian (LSB first) | Some European cars |

**Example:** Value `0x1234` in 2 bytes

- Big Endian: `[0x12, 0x34]`
- Little Endian: `[0x34, 0x12]`

---

## Complete Example

Nissan Juke F15 - Engine RPM from CAN ID 0x180:

```
CAN Frame 0x180: [0x44, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
                  ^^^^  ^^^^
                  Bytes 0-1 = 0x447E = 17534 (raw)

RPM = 17534 / 7 = 2504 RPM
```

```json
{
  "canId": "0x180",
  "fields": [
    {
      "target": "ENGINE_RPM",
      "startByte": 0,
      "byteCount": 2,
      "byteOrder": "BE",
      "dataType": "UINT16",
      "formula": "SCALE",
      "params": [1, 7, 0]
    }
  ]
}
```

---

## How to Find CAN Data for Your Vehicle

### 1. Online Resources

- **OpenDBC** (Comma.ai): https://github.com/commaai/opendbc
- **Car Hacking Wiki**: Various vehicle databases
- **Forums**: Vehicle-specific communities often have CAN documentation

### 2. Sniff Your CAN Bus

Tools needed:
- USB-CAN adapter (ELM327, Arduino + MCP2515, etc.)
- Software: SavvyCAN, can-utils, or similar

Process:
1. Connect to OBD-II port (pins 6=CAN-H, 14=CAN-L)
2. Record CAN traffic while changing vehicle state
3. Identify which CAN ID changes when you:
   - Rev the engine → RPM
   - Open a door → Door status
   - Turn the wheel → Steering angle
   - etc.

### 3. Decode the Data

1. Isolate the CAN ID
2. Determine byte position and count
3. Figure out byte order (try BE first)
4. Find the conversion formula (compare raw vs real value)

---

## Testing Your Preset

### 1. Upload to Device

Use the Android app or serial commands:

```
CAN UPLOAD START <filename>
CAN UPLOAD DATA <base64_chunk>
CAN UPLOAD END
CAN LOAD <filename>
```

### 2. Check Data

```
SYS DATA
```

Shows current values for all fields.

### 3. Enable Logging

```
LOG ON
```

Shows raw CAN frames being processed.

---

## Tips

- Start with just RPM or speed to validate your setup
- Most vehicles use Big Endian byte order
- Door/light status are usually bitmasks in a single frame
- Steering angle is often signed (INT16)
- If values seem wrong, try swapping byte order
- Use `isMock: true` to test without a vehicle

---

## Contributing

Found CAN data for your vehicle? Share your preset:

1. Create a JSON file named `MakeModelYear.json`
2. Test thoroughly
3. Submit to the community presets repository
