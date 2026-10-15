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
  "vehicleParams": {
    "steerScale": 300,
    "steerOffset": 100,
    "steerInvert": true,
    "indTimeout": null,
    "rpmDiv": null,
    "tankCap": null,
    "dteDiv": null
  },
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
          "params": [1, 8, 0]
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
| `vehicleParams` | object (optional) | Calibration overrides for this model — see [Vehicle Parameters](#vehicle-parameters-vehicleparams) |
| `fuelConsumption` | object (optional) | Derive radio consumption from a raw counter and speed — see [Counter-based fuel consumption](#counter-based-fuel-consumption-fuelconsumption) |
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

## Vehicle Parameters (`vehicleParams`)

The optional `vehicleParams` object embeds vehicle-specific calibration directly in the preset file. When a new vehicle is loaded, these values are applied automatically and saved to persistent storage (NVS).

### Null semantics

Each field is independently nullable:

| Value | Behaviour |
|-------|-----------|
| Concrete value (`300`, `true`, …) | Overrides the in-memory config and is saved to NVS |
| `null` or key absent | No change — current NVS value or firmware default is kept |

### Priority

```
NVS (same vehicle loaded again)  ← highest priority
  vehicleParams (vehicle switch)
    firmware defaults             ← lowest priority
```

When the **same vehicle file** is loaded again (e.g., on reboot), NVS values are preserved and `vehicleParams` is **not** re-applied. This ensures user calibrations made via `CFG SET` survive reboots.

When a **different vehicle file** is loaded, NVS is reset to firmware defaults first, then `vehicleParams` is applied and saved. Use `CFG SAVE` after any further tuning to persist your customisations.

### Supported keys

| Key | Type | Range | `CFG SET` param | Description |
|-----|------|-------|-----------------|-------------|
| `steerScale` | integer | 1–20000 (unit x0.0001) | `steerScale` | Steering angle scale factor |
| `steerOffset` | integer | -500 to +500 | `steerOffset` | Steering centre offset |
| `steerInvert` | boolean | `true` / `false` | `steerInvert` | Reverse steering direction |
| `indTimeout` | integer | 100–2000 ms | `indTimeout` | Indicator blink detection timeout |
| `rpmDiv` | integer | 1–20 | `rpmDiv` | RPM raw value divisor |
| `tankCap` | integer | 20–100 L | `tankCap` | Fuel tank capacity in litres |
| `dteDiv` | integer | 100–500 | `dteDiv` | DTE divisor × 100 |

### Note on `rpmDiv`, `tankCap`, `dteDiv`

These values are stored in NVS but are not currently used for post-processing
in the CAN decoder or radio encoder. Set conversions in the formula `params`
of individual frame fields (e.g., `"params": [1, 8, 0]` for Juke RPM). The Juke preset
leaves these `vehicleParams` values at `null`.

The steering parameters (`steerScale`, `steerOffset`, `steerInvert`) have **no frame-level equivalent** — they are applied during radio output in `RadioSend.cpp` and must be set here or via `CFG SET`.

### Lifecycle example

```
1. Load NissanJukeF15.json for the first time
   → NVS reset → steerScale=1000, steerOffset=0, steerInvert=true applied and saved

2. Fine-tune via app: CFG SET steerScale 900, CFG SAVE
   → NVS now holds steerScale=900

3. Reboot — same vehicle file reloaded
   → vehicleParams NOT applied → NVS value 900 preserved ✓

4. Load a different vehicle (OtherVehicle.json)
   → NVS reset → OtherVehicle vehicleParams applied
```

---

## Counter-based fuel consumption (`fuelConsumption`)

The Juke preset adds this **root-level** object. It enables an experimental
conversion of modulo-256 counter increments from `0x551 B1` into the
Raise/Toyota RAV4 radio format. The **80 mm³/increment coefficient is
provisional on the Juke MR16DDT**: it comes from a
[Clio III calibration report](https://x0r.fr/blog/39#comment-396), not a
Juke measurement. The archived VBOX REF retains its original factor 1;
this configurable conversion does not alter that source file.

```json
{
  "fuelConsumption": {
    "enabled": true,
    "calibration": "provisional",
    "source": "https://x0r.fr/blog/39#comment-396",
    "mm3PerTick": 80,
    "windowMs": 1000,
    "maxGapMs": 500,
    "minSpeedKmh": 2,
    "minAverageDistanceM": 100,
    "maxTicksPerSecond": 400
  }
}
```

This is a root-object fragment to insert alongside `frames`, not a complete
preset. Unlike `vehicleParams`, these settings are read from the JSON on
every load and are not NVS overrides.

| Key | Meaning |
| --- | --- |
| `enabled` | Enable the estimator; omission of the object preserves older profiles' behaviour |
| `calibration` | `provisional` (default) or `calibrated`; only mark calibrated after an independent check on the vehicle |
| `source` | Documentary provenance of the coefficient; does not calibrate the signal |
| `mm3PerTick` | Required finite, positive volume per modulo-256 increment; adjustable after calibration |
| `windowMs` | Instantaneous consumption calculation window, 100–10000 ms; Juke: 1000 ms |
| `maxGapMs` | Maximum gap in either source before invalidating and reinitialising the session, 100–1000 ms; Juke: 500 ms |
| `minSpeedKmh` | Below this speed, publish instantaneous 0 with fresh sources; this display convention does not mean zero fuel flow |
| `minAverageDistanceM` | Minimum session distance for average consumption, 100 m |
| `maxTicksPerSecond` | Plausibility limit on counter progression, 400 increments/s |

An enabled estimator requires exactly one `FUEL_COUNTER_RAW` field
(`UINT8`, `byteCount: 1`, `formula: "NONE"`) and one `VEHICLE_SPEED` field.
It rejects profiles with competing direct `FUEL_CONS_INST` or
`FUEL_CONS_AVG` fields. Invalid settings leave the active profile unchanged.
The rate and gap must satisfy `maxTicksPerSecond * maxGapMs / 1000 + 1 < 256`
to keep the plausibility budget below a full counter rollover.

The firmware accumulates `(current - previous) modulo 256` and multiplies
by `mm3PerTick`. It integrates speed to obtain distance using the fractional
Juke value `U16BE(0x284 B4,B5)/100` before truncation for dashboard output.
For nonzero distance, **radio units (0.1 L/100 km) = volume in mm³ / distance
in metres**. Instantaneous consumption uses the configured window; average
consumption uses the session since initialisation, including fuel consumed
while stationary, once its distance threshold is reached.

Profile reload, reboot, a source gap beyond `maxGapMs`, or an implausible
increment resets the session. A counter reset that resembles an ordinary
rollover cannot always be detected. This is not the factory trip average
and it does not survive a restart. The short parking capture does not
calibrate the coefficient or establish long-trip accuracy.

`SYS DATA` reports availability through `FuelConsValid` and the provisional
coefficient through `FuelConsEstimated: 1`. Availability is not a calibration
claim. The radio receives unit `0x02` and unsigned big-endian values:
`0x22` has a 3-byte payload; `0x23` has a 13-byte payload with the current
average and five unavailable history slots (`0xFFFF`). An unavailable
average is also `0xFFFF`. Stale instantaneous values are not transmitted
because no invalid marker is documented for `0x22`; an existing radio
display may therefore retain its previous reading. See
[radio protocol](technical/RADIO_SEND.md) for packet details.
The radio format has no calibration flag; `FuelConsEstimated` is available
only in `SYS DATA`.

Install the matching firmware first, transfer the updated JSON separately,
then run `CAN LOAD NissanJukeF15.json`; application OTA alone does not
replace the profile in LittleFS. `RX` and `ODB` log formats are unchanged.

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
| `COOLANT_TEMP` | Engine coolant temperature | °C |
| `DTE` | Distance to empty | km |
| `FUEL_CONS_INST` | Instant consumption | 0.1 L/100km |
| `FUEL_CONS_AVG` | Average consumption | 0.1 L/100km |
| `FUEL_COUNTER_RAW` | Raw fuel-related byte, separate from consumption | No validated physical unit |

Direct `FUEL_LEVEL`, `FUEL_CONS_INST` and `FUEL_CONS_AVG` targets require
sources in the specified units. Their availability becomes true after a
configured frame supplies a value. The Juke has no fuel-level source and
derives consumption separately through `fuelConsumption` above. Mock mode
retains synthetic values. `FUEL_COUNTER_RAW` remains exposed as the unchanged
`FuelRaw` byte; it must not be displayed directly as L/h or L/100km.

### Additional vehicle states (boolean)

| Target | Description |
| --- | --- |
| `HANDBRAKE` | Juke `0x5C5 B0 bit2`; observed 1 = applied |
| `BRAKE_PEDAL` | Juke `0x180 B7 bit0`; raw switch state, action/polarity to verify |

These states and `FUEL_COUNTER_RAW` report `unknown` in `SYS DATA` before
the first configured frame. Availability resets at profile load; it does not
provide a freshness timeout or physical calibration.

Missing or unknown targets are rejected without replacing the active
profile. Install the matching new firmware **before** uploading a JSON with
these new targets: older parsers may silently map them to `STEERING`.
Application OTA does not update LittleFS; transfer the JSON separately and
activate it with `CAN LOAD <filename>`.

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

The snippets below are field fragments with explanatory comments or symbolic
parameters. Use numeric parameters and omit comments in the final JSON file.
The complete file examples elsewhere in this guide are valid JSON.

### NONE

No conversion, use raw value as-is.

```text
"formula": "NONE"
```

### SCALE

Linear conversion: `result = (value × mult ÷ div) + offset`

```text
"formula": "SCALE",
"params": [mult, div, offset]
```

**Examples:**

```text
// Corrected Juke RPM: raw / 8, corroborated by OBD
"params": [1, 8, 0]

// Speed: raw / 100
"params": [1, 100, 0]

// Temperature: raw - 40
"params": [1, 1, -40]

// Initial Juke DTE hypothesis: raw * 100 / 338
"params": [100, 338, 0]
```

### MAP_RANGE

Map value from one range to another: `map(value, inMin, inMax, outMin, outMax)`

```text
"formula": "MAP_RANGE",
"params": [inMin, inMax, outMin, outMax]
```

**Example:**

```text
// Historical, contradicted Juke fuel hypothesis; do not use as a fuel gauge
"params": [255, 0, 0, 45]
```

This historical whole-byte Juke interpretation has been removed from the
active preset: the byte contains parking-brake and other states. The example
only illustrates formula syntax. Use [the current mapping evidence](JUKE_F15_CAN_MAPPING.md)
and [OBD reference capture](OBD_CALIBRATION.md) for vehicle validation.

### BITMASK_EXTRACT

Extract specific bit(s): `result = (value & mask) >> shift`

```text
"formula": "BITMASK_EXTRACT",
"params": [mask, shift]
```

**Example:**

```text
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

Nissan Juke F15 - corrected RPM mapping from CAN ID 0x180. This
synthetic example checks the formula, not physical calibration:

```
CAN Frame 0x180: [0x44, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
                  ^^^^  ^^^^
                  Bytes 0-1 = 0x447E = 17534 (raw)

RPM = 17534 / 8 = 2191 RPM (integer division)
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
      "params": [1, 8, 0]
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
CAN UPLOAD START <filename> <size>
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

Records all received CAN frames with unchanged bytes, DLC and flags, plus
software `t_ms` timestamps. In real mode, it also starts supported OBD
service 01 reference queries; diagnostic lines begin with `ODB`. Mock mode
sends no OBD queries, and OTA pauses polling.

Save the complete output without time/color prefixes so the comparison tools
can read both raw frames and references. Stop with `LOG OFF`. See
[OBD capture and comparison](OBD_CALIBRATION.md). Reference values never
modify your preset automatically.

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
