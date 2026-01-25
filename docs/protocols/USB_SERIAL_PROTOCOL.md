# USB Serial Protocol Documentation

## Overview

The ESP32 CANBox exposes a USB CDC (Communications Device Class) serial interface for configuration and monitoring. This document describes the command protocol for Android app integration.

## Connection Parameters

| Parameter | Value |
|-----------|-------|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Flow Control | None |
| Line Ending | `\n` (LF) or `\r\n` (CRLF) |

## Command Format

Commands follow an AT-style format:
```
COMMAND [SUBCOMMAND] [PARAMETERS]\n
```

- Commands are **case-insensitive** (converted to uppercase internally)
- Parameters are separated by spaces
- Responses always start with `OK` or `ERROR:`
- Multi-line responses are delimited by `===` markers

## Response Format

### Success Response
```
OK
[optional additional info]
```

### Error Response
```
ERROR: <error description>
```

---

## Command Reference

### CFG - Calibration Parameters (NVS Storage)

Manages calibration values stored in ESP32's Non-Volatile Storage (NVS).

#### CFG GET `<param>`
Read a calibration parameter value.

```
> CFG GET steerOffset
steerOffset = 100
```

#### CFG SET `<param>` `<value>`
Set a calibration parameter (in memory only).

```
> CFG SET steerOffset 150
OK
Value set (use CFG SAVE to persist)
```

#### CFG LIST
Display all calibration parameters.

```
> CFG LIST
=== Current Configuration ===
steerOffset  = 100    (center offset)
steerInvert  = 1      (invert direction)
steerScale   = 4      (scale x0.01)
indTimeout   = 500    (indicator ms)
rpmDiv       = 7      (RPM divisor)
tankCap      = 45     (tank liters)
dteDiv       = 283    (DTE divisor x100)
=============================
```

#### CFG SAVE
Persist current values to NVS flash.

```
> CFG SAVE
OK
Configuration saved to NVS
```

#### CFG RESET
Reset all values to factory defaults.

```
> CFG RESET
OK
Configuration reset to defaults
```

#### Available Parameters

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `steerOffset` | int16 | -500 to +500 | 100 | Steering center offset |
| `steerInvert` | bool | 0/1 | 1 | Invert steering direction |
| `steerScale` | uint8 | 1-200 | 4 | Scale factor (x0.01) |
| `indTimeout` | uint16 | 100-2000 | 500 | Indicator timeout (ms) |
| `rpmDiv` | uint8 | 1-20 | 7 | RPM divisor |
| `tankCap` | uint8 | 20-100 | 45 | Tank capacity (liters) |
| `dteDiv` | uint16 | 100-500 | 283 | DTE divisor (x100) |

---

### CAN - CAN Configuration Files (LittleFS Storage)

Manages vehicle CAN configuration files stored on LittleFS filesystem.

#### CAN STATUS
Display current CAN configuration status.

```
> CAN STATUS
=== CAN Configuration Status ===
Mode: REAL (CAN bus)
Profile: Nissan Juke F15
Frames processed: 12345
Unknown frames: 67
================================
```

#### CAN LIST
List all JSON config files on filesystem.

```
> CAN LIST
=== CAN Config Files ===
  NissanJukeF15.json (2048 bytes)
  MockDemo.json (512 bytes)
Total: 2 file(s)
========================
```

#### CAN LOAD `<filename>`
Load and activate a specific config file.

```
> CAN LOAD NissanJukeF15.json
OK
Loaded: Nissan Juke F15
Mode: REAL
```

#### CAN GET
Output the current active config file as JSON.

```
> CAN GET
=== BEGIN JSON ===
{
  "name": "Nissan Juke F15",
  "isMock": false,
  "frames": [...]
}
=== END JSON ===
```

#### CAN DELETE `<filename>`
Delete a config file from filesystem.

```
> CAN DELETE OldConfig.json
OK
Deleted: /OldConfig.json
```

#### CAN RELOAD
Reload configuration (searches for /vehicle.json or /NissanJukeF15.json).

```
> CAN RELOAD
Reloading CAN configuration...
OK
Loaded: Nissan Juke F15 (REAL mode)
```

---

### CAN UPLOAD - File Upload Protocol

Upload a new CAN configuration file using base64-encoded chunks.

#### Protocol Sequence

```
┌─────────────┐                              ┌─────────────┐
│   Android   │                              │    ESP32    │
│     App     │                              │   CANBox    │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │  CAN UPLOAD START <filename> <size>        │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK READY                                  │
       │<───────────────────────────────────────────│
       │  Awaiting <size> bytes for <filename>      │
       │                                            │
       │  CAN UPLOAD DATA <base64_chunk_1>          │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK 128/1024                               │
       │<───────────────────────────────────────────│
       │                                            │
       │  CAN UPLOAD DATA <base64_chunk_2>          │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK 256/1024                               │
       │<───────────────────────────────────────────│
       │                                            │
       │  ... (repeat until all data sent)          │
       │                                            │
       │  CAN UPLOAD END                            │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK                                        │
       │<───────────────────────────────────────────│
       │  Saved: /vehicle.json (1024 bytes)         │
       │  Use 'CAN RELOAD' to activate              │
       │                                            │
```

#### CAN UPLOAD START `<filename>` `<size>`
Begin a new upload session.

- `filename`: Target filename (with or without leading `/`)
- `size`: Exact size in bytes of the **raw JSON** (before base64 encoding)

```
> CAN UPLOAD START vehicle.json 2048
OK READY
Awaiting 2048 bytes for /vehicle.json
```

**Limits:**
- Maximum file size: 8192 bytes (8KB)
- Filename must end with `.json`

#### CAN UPLOAD DATA `<base64_data>`
Send a chunk of base64-encoded data.

```
> CAN UPLOAD DATA eyJuYW1lIjoiTmlzc2FuIEp1a2UgRjE1IiwiaXNNb2NrIjpmYWxzZSwiZnJhbWVzIjpb...
OK 256/2048
```

**Chunk Size Recommendations:**
- Recommended chunk size: 128-192 bytes of raw data
- Base64 expands data by ~33% (128 bytes → 172 base64 chars)
- Max command line: 256 characters (including `CAN UPLOAD DATA `)

#### CAN UPLOAD END
Finalize the upload and validate JSON.

```
> CAN UPLOAD END
OK
Saved: /vehicle.json (2048 bytes)
Use 'CAN RELOAD' to activate
```

**Validation Checks:**
1. JSON syntax valid
2. Required fields present: `name`, `frames`
3. File written successfully to LittleFS

#### CAN UPLOAD ABORT
Cancel an in-progress upload.

```
> CAN UPLOAD ABORT
Upload aborted
```

---

### OTA - Firmware Updates

Update the device firmware over USB serial using base64-encoded chunks.

#### Protocol Sequence

```
┌─────────────┐                              ┌─────────────┐
│   Android   │                              │    ESP32    │
│     App     │                              │   CANBox    │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │  OTA START <size> [md5_hash]               │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK READY                                  │
       │<───────────────────────────────────────────│
       │  OTA started: expecting <size> bytes       │
       │                                            │
       │  OTA DATA <base64_chunk_1>                 │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK 128/413648 (0%)                        │
       │<───────────────────────────────────────────│
       │                                            │
       │  OTA DATA <base64_chunk_2>                 │
       │───────────────────────────────────────────>│
       │                                            │
       │  OK 256/413648 (0%)                        │
       │<───────────────────────────────────────────│
       │                                            │
       │  ... (repeat until 100%)                   │
       │                                            │
       │  OTA END                                   │
       │───────────────────────────────────────────>│
       │                                            │
       │  MD5 verified OK (if provided)             │
       │<───────────────────────────────────────────│
       │  OK                                        │
       │  Firmware updated successfully!            │
       │  Rebooting in 2 seconds...                 │
       │                                            │
       ▼         [Device reboots]                   ▼
```

#### OTA START `<size>` `[md5]`
Begin a firmware update session.

- `size`: Exact firmware size in bytes (from the `.bin` file)
- `md5`: Optional MD5 hash (32 hex characters) for verification

```
> OTA START 413648 a1b2c3d4e5f6789012345678abcdef01
OK READY
OTA started: expecting 413648 bytes
MD5 verification: a1b2c3d4e5f6789012345678abcdef01
```

**Without MD5 verification:**
```
> OTA START 413648
OK READY
OTA started: expecting 413648 bytes
```

**Limits:**
- Maximum size: Limited by free sketch space (~900KB on ESP32-C3)
- Use `OTA STATUS` to check available space

#### OTA DATA `<base64_data>`
Send a chunk of base64-encoded firmware data.

```
> OTA DATA UjBsR09EbGhBUUFCQUlBQUFBQUFBUC8vL3lINUJBRUFBQUFBTEFBQUFBQUJBQUVBQUFJQlJBQTc=
OK 128/413648 (0%)
```

**Chunk Size Recommendations:**
- Recommended chunk size: 128-192 bytes of raw firmware data
- Base64 expands data by ~33%
- Response includes progress percentage

#### OTA END
Finalize the update, verify MD5 (if provided), and reboot.

```
> OTA END
MD5 verified OK
OK
Firmware updated successfully!
Rebooting in 2 seconds...
```

**Verification Steps:**
1. Check all data received (size matches)
2. Verify MD5 hash (if provided at START)
3. Write to flash partition
4. Automatic reboot after 2 seconds

**If MD5 mismatch:**
```
> OTA END
ERROR: MD5 mismatch!
  Expected: a1b2c3d4e5f6789012345678abcdef01
  Got:      deadbeefcafe1234567890abcdef0123
OTA aborted
```

#### OTA ABORT
Cancel an in-progress update.

```
> OTA ABORT
OTA aborted
```

#### OTA STATUS
Show current OTA status and available space.

```
> OTA STATUS
=== OTA Status ===
Update in progress: NO
Free sketch space: 917504 bytes
Current firmware size: 413648 bytes
==================
```

**During update:**
```
> OTA STATUS
=== OTA Status ===
Update in progress: YES
Progress: 206824 / 413648 bytes (50%)
Expected MD5: a1b2c3d4e5f6789012345678abcdef01
Free sketch space: 917504 bytes
Current firmware size: 413648 bytes
==================
```

---

### LOG - CAN Frame Logging

Enable/disable real-time CAN frame logging for debugging.

#### LOG ON
```
> LOG ON
OK
CAN logging enabled
```

After enabling, each received CAN frame is printed:
```
RX 0x002 [8]: 00 0B 60 00 00 00 00 00
RX 0x180 [8]: 45 E0 00 00 00 00 00 00
```

#### LOG OFF
```
> LOG OFF
OK
CAN logging disabled
```

---

### SYS - System Information & Control

#### SYS INFO
Display system information.

```
> SYS INFO
=== System Info ===
Firmware: 1.7.0 (2026-01-26)
Uptime: 3600 sec
Free heap: 245000 bytes
CPU freq: 160 MHz
Chip: ESP32-C3 rev3
===================
```

#### SYS DATA
Display current vehicle data values.

```
> SYS DATA
=== Live Vehicle Data ===
RPM:      2500
Speed:    60 km/h
Steering: -150
Fuel:     30 L
Battery:  14.1 V
DTE:      350 km
Temp:     85 C
Doors:    0x00
Lights:   H=1 P=0 HB=0 L=0 R=0
=========================
```

#### SYS REBOOT
Restart the device.

```
> SYS REBOOT
Rebooting...
```

---

### HELP
Display command summary.

```
> HELP
=== ESP32 CANBox Command Interface ===

CFG GET <param>       Read calibration value
CFG SET <param> <val> Set calibration value
CFG LIST              Show all calibration
CFG SAVE              Save to flash (NVS)
CFG RESET             Reset to defaults

CAN STATUS            CAN config status
CAN LIST              List config files
CAN LOAD <file>       Load config file
CAN GET               Output current config
CAN DELETE <file>     Delete config file
CAN UPLOAD START/DATA/END  Upload config
CAN RELOAD            Reload configuration

OTA START <size> [md5]  Start firmware update
OTA DATA <base64>       Send firmware chunk
OTA END                 Finalize update
OTA ABORT               Cancel update
OTA STATUS              Show OTA status

LOG ON|OFF            CAN frame logging

SYS INFO              System information
SYS DATA              Live vehicle data
SYS REBOOT            Restart device

HELP                  This message
======================================
```

---

## Android Integration Example (Kotlin)

```kotlin
class CanBoxSerial(private val usbConnection: UsbDeviceConnection) {

    private val inputStream: InputStream
    private val outputStream: OutputStream

    fun sendCommand(command: String): String {
        outputStream.write("$command\n".toByteArray())
        outputStream.flush()

        // Read response until OK or ERROR
        val response = StringBuilder()
        val reader = BufferedReader(InputStreamReader(inputStream))
        var line: String?
        while (reader.readLine().also { line = it } != null) {
            response.appendLine(line)
            if (line!!.startsWith("OK") || line!!.startsWith("ERROR:")) {
                break
            }
        }
        return response.toString()
    }

    fun uploadConfig(filename: String, jsonContent: String): Boolean {
        val bytes = jsonContent.toByteArray(Charsets.UTF_8)

        // Start upload
        var response = sendCommand("CAN UPLOAD START $filename ${bytes.size}")
        if (!response.contains("OK READY")) {
            return false
        }

        // Send data in chunks (128 bytes raw = ~172 base64 chars)
        val chunkSize = 128
        var offset = 0
        while (offset < bytes.size) {
            val end = minOf(offset + chunkSize, bytes.size)
            val chunk = bytes.copyOfRange(offset, end)
            val base64 = Base64.encodeToString(chunk, Base64.NO_WRAP)

            response = sendCommand("CAN UPLOAD DATA $base64")
            if (!response.startsWith("OK")) {
                sendCommand("CAN UPLOAD ABORT")
                return false
            }
            offset = end
        }

        // Finalize
        response = sendCommand("CAN UPLOAD END")
        return response.startsWith("OK")
    }

    fun getStatus(): CanStatus {
        val response = sendCommand("CAN STATUS")
        // Parse response...
        return CanStatus(...)
    }
}
```

---

## Error Handling

| Error Message | Cause | Resolution |
|---------------|-------|------------|
| `Unknown command` | Invalid command name | Check spelling, use HELP |
| `Unknown parameter` | Invalid CFG parameter | Use CFG LIST to see valid params |
| `Value must be X to Y` | Value out of range | Check parameter limits |
| `No upload in progress` | UPLOAD DATA/END without START | Send UPLOAD START first |
| `Invalid size (max 8KB)` | File too large | Reduce config file size |
| `Out of memory` | Heap exhausted | Reduce file size, reboot device |
| `Base64 decode failed` | Corrupted data chunk | Resend chunk or abort |
| `JSON parse failed` | Invalid JSON syntax | Validate JSON before upload |
| `Invalid config: missing 'name' or 'frames'` | Required fields missing | Add name and frames to JSON |
| `File not found` | File doesn't exist | Check filename with CAN LIST |
| `Failed to create file` | Filesystem error | Check LittleFS space |
| `OTA already in progress` | START sent twice | Send OTA ABORT first |
| `Firmware too large` | Size exceeds free space | Check OTA STATUS for available space |
| `OTA not started` | DATA/END without START | Send OTA START first |
| `OTA write failed` | Flash write error | Abort and retry, check device health |
| `MD5 mismatch!` | Firmware corrupted | Re-download firmware and retry |
| `Not enough data received` | Incomplete transfer | Resend missing chunks |

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.7.0 | 2026-01-26 | Added OTA firmware update commands |
| 1.6.0 | 2026-01-26 | Added CAN config upload commands |
| 1.5.0 | 2026-01-22 | Initial serial command interface |
