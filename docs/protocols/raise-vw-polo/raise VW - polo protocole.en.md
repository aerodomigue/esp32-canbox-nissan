Volkswagen Platform – CAN BUS Module to DVD Head Unit Communication Protocol 1.4
===============================================================================

This document describes the communication protocol between the DVD head unit system (HOST) and the CAN BUS decoder (SLAVE).  
It covers the **physical layer**, **data link layer** and **application layer**.

---

Physical Layer
--------------

- **Interface**: standard UART
- **Logic level**: 5 V TTL
- **UART mode**: 8N1  
  - 8 data bits  
  - no parity  
  - 1 stop bit
- **Baud rate**: fixed at **38400 bps**

### Conventions

- **HOST**: DVD / NAVI head unit
- **SLAVE**: CAN BUS decoder

---

Data Link Layer
---------------

### Frame Structure

The UART frame is defined as follows:

| Order | Field      | Description                             |
|-------|-----------|-----------------------------------------|
| 1     | Head Code | Fixed to `0x2E`                         |
| 2     | Data Type | See *Data Type Definition* table below  |
| 3     | Length    | Length of the data field                |
| 4     | Data0     |                                         |
| 5     | Data1     |                                         |
| 6..N  | …         | Data2…DataN (payload)                   |
| N     | Checksum  | See below                               |

**Checksum**:

\[
Checksum = \bigl(DataType + Length + Data0 + \dots + DataN\bigr) \oplus 0xFF
\]

---

ACK / NACK
----------

### Definition

ACK/NACK is a 1‑byte response frame:

| Value  | Meaning                    |
|--------|----------------------------|
| `0xFF` | ACK (frame OK)            |
| `0xF0` | NACK (Checksum NG / error) |

- The reply frame **consists of only one byte**.

### Timing Rules

1. After the receiver gets a complete frame, it **must reply with ACK or NACK within 10 ms**.  
2. The sender must be able to receive ACK/NACK in the range **0–100 ms** after sending the frame.  
3. If no ACK is received within 100 ms, the frame **must be retransmitted**.  
   - If the same frame has been retransmitted **three times without ACK**, all further transmissions must be stopped and an appropriate error handling procedure should be executed.

---

Application Layer
-----------------

### Data Type Definition

The `DataType` field defines the semantic of the payload.

#### DataType from SLAVE → HOST

| DataType | Name / Description                | Notes                                                                                      |
|----------|-----------------------------------|--------------------------------------------------------------------------------------------|
| `0x14`   | Backlight level information       | Sent on power‑on, whenever backlight changes, and on request                               |
| `0x16`   | Vehicle speed                     |                                                                                            |
| `0x20`   | Steering wheel key information    |                                                                                            |
| `0x21`   | Air‑conditioning information      |                                                                                            |
| `0x22`   | Rear parking radar information    |                                                                                            |
| `0x23`   | Front parking radar information   |                                                                                            |
| `0x24`   | Basic vehicle information         | Decoder sends proactively on change; can also be requested by the head unit               |
| `0x25`   | Parking assistance status         |                                                                                            |
| `0x26`   | Steering wheel angle              |                                                                                            |
| `0x27`   | Amplifier status                  |                                                                                            |
| `0x30`   | Version information               | Sent on power‑on or when queried                                                          |
| `0x41`   | Body information                  |                                                                                            |

#### DataType from HOST → SLAVE

| DataType | Name / Description            | Notes                                                                                                                             |
|----------|-------------------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| `0x81`   | Start / End                   | Connection establishment / release                                                                                                |
| `0x90`   | Request control information   | Request decoder to send specific data types (`0x14, 0x16, 0x21, 0x24, 0x25, 0x26, 0x30, 0x41`) to the head unit                   |
| `0xA0`   | Amplifier control command     | Control external amplifier parameters                                                                                             |
| `0xC0`   | Source select                 | Select active audio/video source                                                                                                  |
| `0xC1`   | ICON / play‑mode indication   | Display state/ICON for current source and play mode                                                                              |
| `0xC2`   | Tuner (radio) information     | Frequency, band and preset index                                                                                                  |
| `0xC3`   | Media playback information    | Generic playback information (track, folder, time, etc.) according to media type                                                |
| `0xC4`   | Volume OSD control            | Volume and mute indication                                                                                                       |
| `0xC6`   | Radar volume control          | Rear‑radar sound enable/disable and parking mode                                                                                 |

---

Detailed Data Formats
---------------------

### 1) Backlight Information – `0x14` (SLAVE → HOST)

| Field    | Value   | Description                  |
|----------|---------|------------------------------|
| DataType | `0x14`  | Data type                    |
| Length   | `0x01`  | Length = 1 byte             |
| Data0    | 0x00–FF | Key backlight brightness     |

- `Data0 = 0x00` → darkest  
- `Data0 = 0xFF` → brightest  
- This value reflects the **OEM key backlight brightness**.  
- If used to control screen brightness, note:  
  - When `Data0 = 0x00`, the environment is bright and key backlight is OFF, so the **screen brightness should be relatively high**.  
- This frame is sent:
  - on power‑on,  
  - whenever the value changes,  
  - and on explicit request.

---

### 2) Vehicle Speed – `0x16` (SLAVE → HOST)

| Field    | Value  | Description                        |
|----------|--------|------------------------------------|
| DataType | `0x16` | Data type                          |
| Length   | `0x02` | Length = 2 bytes                   |
| Data0    | LSB    | Least significant byte             |
| Data1    | MSB    | Most significant byte              |

Vehicle speed:

\[
Speed\,(km/h) = \frac{(Data1 \times 256 + Data0)}{16}
\]

---

### 3) Steering Wheel Keys – `0x20` (SLAVE → HOST)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0x20` | Data type          |
| Length   | `0x02` | Length = 2 bytes   |

#### Data0 – Key Code

| Value | Meaning                        |
|-------|--------------------------------|
| 0x00  | No key pressed / key released  |
| 0x01  | VOL+                           |
| 0x02  | VOL–                           |
| 0x03  | `>` / MENU UP                  |
| 0x04  | `<` / MENU DOWN                |
| 0x05  | TEL                            |
| 0x06  | MUTE                           |
| 0x07  | SRC                            |
| 0x08  | MIC                            |

#### Data1 – Key Status

| Value | Meaning                 |
|-------|-------------------------|
| 0     | Key released            |
| 1     | Key pressed             |
| 2     | Continuous press valid  |

---

### 4) Air‑Conditioning Information – `0x21` (SLAVE → HOST)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0x21` | Data type          |
| Length   | `0x05` | Length = 5 bytes   |

#### Data0 – A/C Status

- **Bit7**: A/C main ON/OFF  
  - 0: OFF  
  - 1: ON
- **Bit6**: A/C compressor ON/OFF  
  - 0: A/C OFF  
  - 1: A/C ON
- **Bit5**: Air circulation mode  
  - 0: Fresh air (outside)  
  - 1: Recirculation (inside)
- **Bit4**: AUTO (high fan) indicator  
  - 0: OFF  
  - 1: ON
- **Bit3**: AUTO (low fan) indicator  
  - 0: OFF  
  - 1: ON
- **Bit2**: DUAL indicator  
  - 0: OFF  
  - 1: ON
- **Bit1**: MAX FRONT indicator  
  - 0: OFF  
  - 1: ON
- **Bit0**: REAR indicator  
  - 0: OFF  
  - 1: ON

#### Data1 – Blower and Air Direction

- **Bit7**: Upper vents ON/OFF
- **Bit6**: Front (face‑level) vents ON/OFF
- **Bit5**: Lower vents ON/OFF
- **Bit4**: A/C display request  
  - 0: Do not display  
  - 1: Request to display A/C info
- **Bit3..Bit0**: Fan speed level  
  - 0x00–0x07 → level 0–7

#### Data2 – Driver Temperature

- `0x00`: LO
- `0x1F`: HI
- `0x01–0x11`: 18 °C – 26 °C, step 0.5 °C (other values invalid)

#### Data3 – Passenger Temperature

- Same encoding as Data2 (driver side).

#### Data4 – Seat Heating / AQS

- **Bit7**: AQS recirculation
  - 0: non‑AQS recirculation  
  - 1: AQS recirculation
- **Bit6**: reserved
- **Bit5..Bit4**: Left seat heating level  
  - 00: no display  
  - 01–11: level 1–3
- **Bit1..Bit0**: Right seat heating level  
  - 00: no display  
  - 01–11: level 1–3

---

### 5) Rear Parking Radar – `0x22` (SLAVE → HOST)

Sent **only when distances change**.

| Field    | Value  | Description                         |
|----------|--------|-------------------------------------|
| DataType | `0x22` | Data type                           |
| Length   | `0x04` | Length = 4 bytes                    |

For all distance bytes below:

- `0x00`: no display  
- `0x01`: nearest  
- `0x0A`: farthest  
- Range: `0x00–0x0A`

| Byte   | Description                             |
|--------|-----------------------------------------|
| Data0  | Rear left sensor distance               |
| Data1  | Rear left‑center sensor distance        |
| Data2  | Rear right‑center sensor distance       |
| Data3  | Rear right sensor distance              |

---

### 6) Front Parking Radar – `0x23` (SLAVE → HOST)

Also sent **only when distances change**.

| Field    | Value  | Description                         |
|----------|--------|-------------------------------------|
| DataType | `0x23` | Data type                           |
| Length   | `0x04` | Length = 4 bytes                    |

Distance encoding is the same as for `0x22`.

| Byte   | Description                             |
|--------|-----------------------------------------|
| Data0  | Front left sensor distance              |
| Data1  | Front left‑center sensor distance       |
| Data2  | Front right‑center sensor distance      |
| Data3  | Front right sensor distance             |

---

### 7) Basic Information – `0x24` (SLAVE → HOST)

Sent **proactively when data changes**, or on **explicit query** by the DVD head unit.

| Field    | Value  | Description              |
|----------|--------|--------------------------|
| DataType | `0x24` | Data type                |
| Length   | `0x01` | Length = 1 byte          |
| Data0    |        | Reverse / parking info   |

#### Data0 – Reverse / Parking Info

- **Bit7..Bit3**: reserved
- **Bit2**: Light status  
  - 0: OFF  
  - 1: ON
- **Bit1**: P‑gear or handbrake status  
  - 0: not in P / handbrake released  
  - 1: P gear or handbrake applied
- **Bit0**: Reverse gear status  
  - 0: not in reverse  
  - 1: reverse engaged

---

### 8) Parking Assistance Status – `0x25` (SLAVE → HOST)

| Field    | Value  | Description                      |
|----------|--------|----------------------------------|
| DataType | `0x25` | Data type                        |
| Length   | `0x02` | Length = 2 bytes                 |
| Data0    |        | Parking assist system status     |
| Data1    |        | Reserved                         |

#### Data0 – Bits

- **Bit7..Bit2**: reserved
- **Bit1**: Parking assist system ON/OFF  
  - 0: OFF  
  - 1: ON
- **Bit0**: Radar sound ON/OFF  
  - 0: no radar sound  
  - 1: radar sound active

---

### 9) Steering Wheel Angle – `0x26` (SLAVE → HOST)

| Field    | Value  | Description                 |
|----------|--------|-----------------------------|
| DataType | `0x26` | Data type                   |
| Length   | `0x02` | Length = 2 bytes            |
| Data0    | ESP1   | Steering angle low byte     |
| Data1    | ESP2   | Steering angle high byte    |

Interpretation example:

- `ESP = 0` → steering wheel centered  
- `ESP > 0` → turning left  
- `ESP < 0` → turning right

---

### 10) Body Information – `0x41` (SLAVE → HOST)

| Field      | Description                       |
|------------|-----------------------------------|
| DataType   | `0x41`                            |
| Length     | `N` (variable)                    |
| Data0      | Command                           |
| Data1..11  | Data (depends on command)         |

#### Command Table

##### Command `0x01` – Doors / safety / trunk / washer

- `N = 2`
- `Data[1]` bits:
  - Bit7: Driver seat belt  
    - 0: normal (belt fastened)  
    - 1: not fastened
  - Bit6: Washer fluid level  
    - 0: normal  
    - 1: low washer fluid
  - Bit5: Handbrake  
    - 0: normal (released)  
    - 1: handbrake applied
  - Bit4: Trunk status  
    - 0: closed  
    - 1: open
  - Bit3: Rear door (if present)  
    - 0: closed  
    - 1: open
  - Bit2: Left rear door  
    - 0: closed  
    - 1: open
  - Bit1: Right front door  
    - 0: closed  
    - 1: open
  - Bit0: Left front door  
    - 0: closed  
    - 1: open

##### Command `0x02` – Engine / speed / battery / outside temp / mileage / fuel

- `N = 13`

| Bytes        | Description                                       |
|--------------|---------------------------------------------------|
| Data1–Data2  | Engine RPM: `RPM = Data1 * 256 + Data2`           |
| Data3–Data4  | Instantaneous speed (km/h): `(Data3 * 256 + Data4) * 0.01` |
| Data5–Data6  | Battery voltage (V): `(Data5 * 256 + Data6) * 0.01` |
| Data7–Data8  | Outside temperature (°C): `(Data7 * 256 + Data8) * 0.1`.<br>Valid range: −50 °C to 77.5 °C (negative values in 2‑byte two’s‑complement) |
| Data9–Data11 | Trip distance (km): `Data9 * 65536 + Data10 * 256 + Data11` |
| Data12       | Remaining fuel (L): `Fuel = Data12`               |

##### Command `0x03` – Warning flags

- `N = 2`
- `Data1` bits:
  - Bit7: Low fuel warning  
    - 1: fuel low; 0: normal
  - Bit6: Battery voltage warning  
    - 1: voltage low; 0: normal
  - Bit5..Bit0: reserved

---

### 11) Amplifier Status – `0x27` (SLAVE → HOST)

Sent when amplifier settings change.

| Field    | Value  | Description                                       |
|----------|--------|---------------------------------------------------|
| DataType | `0x27` | Data type                                         |
| Length   | `0x08` | Length = 8 bytes                                  |

| Byte   | Name                     | Range / Meaning                         |
|--------|--------------------------|-----------------------------------------|
| Data0  | Amplifier type           | `0x00`: no amp; `0x01`: non‑CAN amp; `0xFF`: unknown |
| Data1  | Volume                   | 0–30 (unsigned char)                    |
| Data2  | BALANCE                  | −9 – +9 (signed char)                   |
| Data3  | FADER                    | −9 – +9 (signed char)                   |
| Data4  | BASS                     | −9 – +9 (signed char)                   |
| Data5  | MIDTONE                  | −9 – +9 (signed char)                   |
| Data6  | TREBLE                   | −9 – +9 (signed char)                   |
| Data7  | Speed‑dependent volume   | 0–7 (unsigned char)                     |

---

### 12) Start / End – `0x81` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0x81` | Data type          |
| Length   | `0x01` | Length = 1 byte    |
| Data0    |        | Command type       |

#### Data0 – Command Type

| Value | Meaning                                                                                                                                                          |
|-------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0x01  | **Start** – when the system powers up, the HOST sends this command to establish a connection. Connection is successful after SLAVE replies with ACK.           |
| 0x00  | **End** – when the system powers down, the HOST sends this command to terminate the connection. After ACK is received from SLAVE, the HOST stops communication. |

---

### 13) Request Control Information – `0x90` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0x90` | Data type          |
| Length   | `N`    | Data length        |

| Byte   | Description                        |
|--------|------------------------------------|
| Data0  | Requested content (see below)      |
| Data1  | Used only for `0x41` body info     |

**Data0 – Request content:**

- `0x14`: Backlight information  
- `0x16`: Vehicle speed  
- `0x21`: A/C information  
- `0x24`: Basic information  
- `0x25`: Parking assist status  
- `0x26`: Steering angle  
- `0x30`: Version information  
- `0x41`: Body information (command value provided in Data1)

---

### 14) Amplifier Control – `0xA0` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xA0` | Data type          |
| Length   | `0x02` | Length = 2 bytes   |

| Byte   | Description |
|--------|-------------|
| Data0  | Command     |
| Data1  | Parameter   |

#### Data0 – Command Codes

| Value | Meaning                       |
|-------|-------------------------------|
| 0x00  | Volume                        |
| 0x01  | BALANCE                       |
| 0x02  | FADER                         |
| 0x03  | BASS                          |
| 0x04  | MIDTONE                       |
| 0x05  | TREBLE                        |
| 0x06  | Speed‑dependent volume        |

#### Parameter Table (Data1)

| Index | Command                     | Parameter range                   | Default |
|-------|-----------------------------|-----------------------------------|---------|
| 0     | Volume                      | 0–30 (unsigned char)             | 28      |
| 1     | BALANCE                     | −9 – +9 (signed char)            | 0       |
| 2     | FADER                       | −9 – +9 (signed char)            | 0       |
| 3     | BASS                        | −9 – +9 (signed char)            | 0       |
| 4     | MIDTONE                     | −9 – +9 (signed char)            | 0       |
| 5     | TREBLE                      | −9 – +9 (signed char)            | 0       |
| 6     | Speed‑dependent volume      | 0–7 (unsigned char)              | 0       |

> If no explicit control commands are received, the CAN BOX opens the amplifier with these default settings.

---

### 15) Source – `0xC0` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xC0` | Data type          |
| Length   | `0x02` | Length = 2 bytes   |

#### Data0 – Source

| Value | Source                       |
|-------|------------------------------|
| 0x00  | OFF                          |
| 0x01  | Tuner                        |
| 0x02  | Disc (CD, DVD)              |
| 0x03  | TV (analog)                 |
| 0x04  | NAVI                         |
| 0x05  | Phone                        |
| 0x06  | iPod                         |
| 0x07  | AUX                          |
| 0x08  | USB                          |
| 0x09  | SD                           |
| 0x0A  | DVB‑T                        |
| 0x0B  | Phone A2DP                   |
| 0x0C  | Other                        |
| 0x0D  | CDC v1.02                    |

#### Data1 – Media Type

| Value | Media Type              |
|-------|-------------------------|
| 0x01  | Tuner                   |
| 0x10  | Simple audio media      |
| 0x11  | Enhanced audio media    |
| 0x12  | iPod                    |
| 0x20  | File‑based video        |
| 0x21  | DVD video               |
| 0x22  | Other video             |
| 0x30  | AUX / other             |
| 0x40  | Phone                   |

---

### 16) ICON – `0xC1` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xC1` | Data type          |
| Length   | `0x01` | Length = 1 byte    |

#### Data0 – Source / Play Mode Flags

- **Bit7..Bit3**: reserved  
- **Bit2..Bit1**: Play mode (v1.02)  
  - `0x00`: Normal  
  - `0x01`: Scan (CD/DVD/TUNER)  
  - `0x02`: Mix (CD/DVD only)  
  - `0x03`: Repeat (CD/DVD only)
- **Bit0**: reserved

---

### 17) Tuner (Radio) Information – `0xC2` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xC2` | Data type          |
| Length   | `0x04` | Length = 4 bytes   |

| Byte   | Description                                          |
|--------|------------------------------------------------------|
| Data0  | Radio band                                           |
| Data1  | Current frequency LSB                                |
| Data2  | Current frequency MSB                                |
| Data3  | Preset index                                         |

#### Data0 – Band

| Value | Band  |
|-------|-------|
| 0x00  | FM    |
| 0x01  | FM1   |
| 0x02  | FM2   |
| 0x03  | FM3   |
| 0x10  | AM    |
| 0x11  | AM1   |
| 0x12  | AM2   |
| 0x13  | AM3   |

Frequency:

- FM: `Freq(MHz) = X / 100`  
- AM: `Freq(kHz) = X`

Preset:

- `Data3 = 0..6`, where 0 means *not a preset station*.

---

### 18) Media Playback Information – `0xC3` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xC3` | Data type          |
| Length   | `0x06` | Length = 6 bytes   |

| Byte   | Description |
|--------|-------------|
| Data0  | Info1       |
| Data1  | Info2       |
| Data2  | Info3       |
| Data3  | Info4       |
| Data4  | Info5       |
| Data5  | Info6       |

The interpretation of Info1..Info6 depends on **Media Type**.

#### Media Type–specific Formats

| Media Type | Description              | Info1                                  | Info2                                  | Info3                                 | Info4                                 | Info5        | Info6        |
|------------|--------------------------|----------------------------------------|----------------------------------------|---------------------------------------|---------------------------------------|--------------|--------------|
| `0x01`     | Tuner                    | Unused                                 | Unused                                 | Unused                                | Unused                                | Unused       | Unused       |
| `0x10`     | Simple audio media       | Disc number (`0x00` = no disc)         | Track number (`0x01–0xFF`)             | Unused                                | Unused                                | Minute       | Second       |
| `0x11`     | Enhanced audio media     | Folder number low byte                 | Folder number high byte                | File number low byte                  | File number high byte                 | Minute       | Second       |
| `0x12`     | iPod                     | Current song number (low byte)         | Current song number (high byte)        | Total song number (low byte)          | Total song number (high byte)         | Unused       | Unused       |
| `0x20`     | File‑based video         | Folder number low byte                 | Folder number high byte                | File number low byte                  | File number high byte                 | Unused       | Unused       |
| `0x21`     | DVD video                | Current chapter                        | Total chapter                          | Unused                                | Unused                                | Minute       | Second       |
| `0x22`     | Other video              | Unused                                 | Unused                                 | Unused                                | Unused                                | Unused       | Unused       |
| `0x30`     | AUX / other              | Unused                                 | Unused                                 | Unused                                | Unused                                | Unused       | Unused       |
| `0x40`     | Phone                    | Unused                                 | Unused                                 | Unused                                | Unused                                | Unused       | Unused       |

---

### 19) Volume Display Control – `0xC4` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xC4` | Data type          |
| Length   | `0x01` | Length = 1 byte    |

#### Data0 – Volume Display

- **Bit7**: Mute control  
  - 1: mute  
  - 0: not muted
- **Bit6..Bit0**: Volume value, valid range 0–127

---

### 20) Radar Volume Control – `0xC6` (HOST → SLAVE)

| Field    | Value  | Description        |
|----------|--------|--------------------|
| DataType | `0xC6` | Data type          |
| Length   | `0x02` | Length = 2 bytes   |

| Byte   | Description |
|--------|-------------|
| Data0  | Command     |
| Data1  | Parameter   |

#### Command Table

| Index | Command | Description                        | Parameter values                            |
|-------|---------|------------------------------------|--------------------------------------------|
| 1     | 0x00    | Reverse radar sound control        | 0x00: disable reverse radar sound<br>0x01: enable reverse radar sound |
| 2     | 0x02    | Parking mode                       | 0x00: Mode 1 (standard)<br>0x01: Mode 2 (parallel parking along roadside)<br>See vehicle user manual for details |

