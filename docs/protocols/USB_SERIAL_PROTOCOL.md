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

- Command and subcommand names are **case-insensitive**; filenames and encoded payloads retain their case
- Parameters are separated by spaces
- Acknowledgments use `OK` or `ERROR:`; value reads such as `CFG GET` can return a value without either marker
- Status/list responses have command-specific `===` headers and footers
- When logging is enabled, asynchronous `RX` and `ODB` records share the connection with command responses

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

### Routing the shared stream

The exact diagnostic prefix is **`ODB `**, including the trailing space.
The firmware does not emit an `OBD ` family, a `DASH ` family or standalone
`TX ` records. Route complete lines using this order:

| Line starts with / equals | Meaning | Destination |
| --- | --- | --- |
| `RX 0x` | Received raw CAN frame, including unknown IDs | CAN capture |
| `ODB RAW_RX 0x` | Received raw standard frame on `0x7DF`–`0x7EF` | Diagnostic capture |
| `ODB TX ` | OBD request bytes and driver queue status | Diagnostic capture |
| `ODB RX ` | Decoded OBD reference, never a dashboard update | Diagnostic capture |
| `ODB ` (other subtypes) | Capabilities, timeouts, negative responses, state, driver counters | Diagnostic capture |
| `=== Live Vehicle Data ===` (exact) | Start of the response to `SYS DATA` | Dashboard response |
| `=========================` (exact) | End of that dashboard response | Finish dashboard response |
| `OK`, `OK `, `ERROR: ` | Command acknowledgment or failure | Pending command |
| Other lines | Dashboard fields while its block is open; otherwise command/boot information | Matching response / console |

Match raw CAN with `RX 0x`, not just `RX`: controller error messages such as
`RX Err: ...` are console information. Check the raw and diagnostic families
before accumulating dashboard fields. `SYS DATA` is requested by the client;
the firmware does not periodically publish that block itself. Its fields
start with `Config:`, `Mode:`, `RPM:`, `Speed:`, `Steering:`, `Fuel:`,
`Battery:`, `DTE:`, `Temp:`, `Doors:` and `Lights:`. Other commands have their
own headers and footers; a line of `=` is not a universal response terminator.

`ODB TX` covers OBD request attempts only. It is not a capture of every CAN
transmission or of the separate radio UART. With logging enabled, the firmware
attempts to export each dequeued frame once, either as `RX` or as `ODB RAW_RX`.
A valid reply may additionally produce
an `ODB RX`, `ODB SUPPORTED` or `ODB NEGATIVE` interpretation.

USB reads are arbitrary byte chunks: a chunk can end inside `ODB`, a payload
or a CRLF, and one chunk can contain several lines. Keep a persistent receive
buffer, extract only records ending in LF, remove an optional preceding CR
for parsing, and retain the unfinished suffix for the next read. Preserve
the original line and its line ending when exporting. Command completion
and OBD replies must not control when complete raw lines are processed.

This describes the intended records. The current firmware can still lose
or truncate output under USB backpressure; see
[the firmware streaming validation](../LOG_STREAMING_VALIDATION.fr.md).

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
steerScale   = 300    (scale x0.0001)
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
| `steerScale` | uint16 | 1-20000 | 300 | Scale factor (x0.0001). e.g. 200 = 0.0200x |
| `indTimeout` | uint16 | 100-2000 | 500 | Indicator timeout (ms) |
| `rpmDiv` | uint8 | 1-20 | 7 | RPM divisor |
| `tankCap` | uint8 | 20-100 | 45 | Tank capacity (liters) |
| `dteDiv` | uint16 | 100-500 | 283 | DTE divisor (x100) |

`rpmDiv`, `tankCap` and `dteDiv` are stored settings, but the current JSON
decoder does not use them for conversion. The active preset supplies its own
formulas; changing these three NVS values does not change decoded RPM, fuel
level or DTE. In particular, the active Juke preset uses DTE divisor 338,
while the stored factory default above remains 283.

---

### CAN - CAN Configuration Files (LittleFS Storage)

Manages vehicle CAN configuration files stored on LittleFS filesystem.

#### CAN STATUS
Display current CAN configuration status.

```
> CAN STATUS
=== CAN Configuration Status ===
Config: NissanJukeF15.json
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

> **Full specification:** see [`docs/protocols/OTA_PROTOCOL.md`](OTA_PROTOCOL.md)

#### Protocol Sequence (v2)

```
┌─────────────┐                              ┌─────────────┐
│   Android   │                              │    ESP32    │
│     App     │                              │   CANBox    │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │  OTA ABORT          (pre-flight)           │
       │───────────────────────────────────────────>│
       │  OTA aborted  (drain 1s)                   │
       │                                            │
       │  OTA START <size> [md5]                    │
       │───────────────────────────────────────────>│
       │                                            │
       │  OTA starting: expecting <size> bytes      │  ← info lines first
       │<───────────────────────────────────────────│
       │  MD5: <md5>                                │
       │<───────────────────────────────────────────│
       │  OK READY                                  │  ← terminal line (last)
       │<───────────────────────────────────────────│
       │                                            │
       │  OTA DATA <base64> <crc32>                 │  ← no echo from ESP32
       │───────────────────────────────────────────>│
       │                                            │
       │  OK 180/413648 (0%)                        │
       │<───────────────────────────────────────────│
       │                                            │
       │  ... (repeat until 100%)                   │
       │                                            │
       │  OTA END                                   │
       │───────────────────────────────────────────>│
       │                                            │
       │  MD5 verified OK                           │
       │<───────────────────────────────────────────│
       │  OK                                        │
       │<───────────────────────────────────────────│
       │                                            │
       ▼         [Device reboots ~2s later]         ▼
```

#### OTA START `<size>` `[md5]`

```
> OTA START 413648 a1b2c3d4e5f6789012345678abcdef01
OTA starting: expecting 413648 bytes
MD5: a1b2c3d4e5f6789012345678abcdef01
OK READY
```

Note: info lines come **before** `OK READY` (v2 change). Parse by scanning for the `OK` prefix.

#### OTA DATA `<base64_data>` `[crc32_hex]`

```
> OTA DATA UjBsR09EbGhBUUFCQUlBQUFBQUFBUC8vL3lINUJBRUFBQUFBTEFBQUFBQUJBQUVBQUFJQlJBQTc= a1b2c3d4
OK 180/413648 (0%)
```

CRC32 is the hex checksum of the **decoded binary** chunk (not the base64 string).
On CRC mismatch, the device returns `ERROR: CRC mismatch chunk ...` without aborting OTA — the same chunk can be retransmitted safely.

Auto-abort: if no `OTA DATA` is received for **60 seconds**, OTA is aborted automatically.

#### OTA END / OTA ABORT / OTA STATUS

Unchanged from v1. See [`docs/protocols/OTA_PROTOCOL.md`](OTA_PROTOCOL.md) for full details.

---

### LOG - Raw CAN Capture and OBD Reference Polling

In real vehicle mode, `LOG ON` enables raw CAN capture and read-only OBD-II
service 01 polling for calibration. Reference measurements remain separate
from the active preset and the values sent to the head unit.

#### LOG ON
```
> LOG ON
OK
CAN logging enabled
```

The following firmware output is **synthetic**, showing a vehicle that
advertises RPM only. It illustrates discovery, a physical request and its
response; it is not a captured or calibrated Juke measurement.

```text
ODB STATE t_ms=1000 enabled=1 status=discovery
ODB TX t_ms=1000 seq=1 can_id=0x7DF service=0x01 pid=0x00 status=queued data=02 01 00 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 06 41 00 00 10 00 00 00 | t_ms=1008 ext=0 rtr=0
ODB SUPPORTED t_ms=1008 seq=1 request_id=0x7DF ecu=0x7E8 pid=0x00 bitmap=0x00100000
ODB TX t_ms=1500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 11 95 00 00 00 00 00 00 | t_ms=1507 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=1508 ext=0 rtr=0
ODB RX t_ms=1508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

All OBD diagnostic records start with the exact prefix **`ODB`**, including
`ODB TX`, `ODB RAW_RX`, `ODB SUPPORTED`, `ODB RX`, `ODB NEGATIVE`,
`ODB TIMEOUT`, `ODB STATE` and `ODB DRIVER`. Standard raw frames outside
IDs `0x7DF`–`0x7EF` start with `RX`;
extended frames also keep that prefix. Command acknowledgments retain their
existing formats. Client applications must route these record families
separately rather than treat an OBD value as an `OK` response.

For received frames and their decoded references, `t_ms` is the uint32
`millis()` value when the frame leaves the software receive queue, before
printing. `ODB TX` carries the attempt time; reference latency includes
software and queue delays. These are not hardware bus timestamps. Timestamps
reset after reboot and wrap after about 49.7 days. Raw logs preserve ID, DLC,
bytes and `ext`/`rtr` flags. RTR records have no data payload; an invalid DLC is
printed as declared, but the logger reads at most eight bytes.

The client discovers supported PIDs with `00`, then `20` and `40` only when
the preceding page announces them. It queries supported RPM (`0C`), speed
(`0D`), coolant (`05`), fuel level (`2F`), module voltage (`42`), ambient
temperature (`46`), manifold absolute pressure (`0B`) and fuel rate (`5E`).
Availability depends on the ECU. Fuel level is in percent and fuel rate in
L/h; the firmware does not convert either into tank liters or L/100 km.

Discovery collects replies from `0x7E8`–`0x7EF` separately. Measurement
requests target the lowest-address ECU that advertises the PID, using
`0x7E0`–`0x7E7`. There is one request at a time, at least 500 ms between
attempts, and a 250 ms response window. Responses must be standard, non-RTR,
DLC 8 ISO-TP single frames with the expected service, PID and length.
Unsupported parameters are skipped; missing replies produce a timeout,
not a zero measurement. After three unsuccessful discovery requests, the
client waits five seconds before trying again. `queued` means the driver
accepted a request for transmission, not that the ECU acknowledged it.

#### LOG OFF
```
> LOG OFF
OK
CAN logging disabled
```

`LOG OFF` stops capture and new OBD requests, clears the pending request and
emits `ODB STATE ... enabled=0 status=stopped` when calibration was active.
A request already queued in the CAN controller cannot be cancelled by this
command. Repeated `LOG ON` commands do not restart an active discovery.

Mock mode does not poll OBD. OTA suspends capture processing and polling;
after an OTA abort or timeout, polling restarts with discovery if logging is
still enabled and the device is in real mode. A successful OTA reboots the
device, so a new `LOG ON` is required. Partial command echo is suppressed
while logging to keep raw and diagnostic lines intact.

RX is nonblocking, with a 128-frame queue and at most 32 frames drained per
loop. `ODB DRIVER` reports counters and queue occupancy once per second;
USB throughput can still cause losses. Save captures without terminal color
or timestamp prefixes, for example with
`.venv/bin/pio device monitor --filter direct`. See the
[calibration guide](../OBD_CALIBRATION.fr.md) for analysis and limitations.

---

### SYS - System Information & Control

#### SYS INFO
Display system information.

```
> SYS INFO
=== System Info ===
Firmware: 1.7.2 (2026-01-28)
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
Config:   NissanJukeF15.json
Mode:     REAL (Nissan Juke F15)
RPM:      2500
Speed:    60 km/h
Steering: -150
Fuel:     30 L
Battery:  14.1 V
DTE:      350 km
Temp:     28 C
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

#### SYS BOOTLOADER
Enter esptool bootloader mode for fast firmware flashing.

```
> SYS BOOTLOADER
Entering bootloader mode for esptool...
Use esptool.py to flash firmware.
```

After this command, the device restarts in download mode. Use `esptool.py` or an Android app implementing the esptool protocol to flash firmware at high speed (~100+ KB/s).

**Note:** This is the recommended method for firmware updates. The OTA serial commands (base64) are provided as a fallback.

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

OTA START <size> [md5]       Start firmware update
OTA DATA <base64> [crc32]    Send firmware chunk
OTA END                      Finalize and reboot
OTA ABORT               Cancel update
OTA STATUS              Show OTA status

LOG ON|OFF            Raw CAN logging + ODB service 01 reference polling

SYS INFO              System information
SYS DATA              Live vehicle data
SYS REBOOT            Restart device
SYS BOOTLOADER        Enter esptool flash mode

HELP                  This message
======================================
```

---

## Android Integration Example (Kotlin)

This simplified example covers transactions terminated by `OK` or `ERROR:`.
An application also needs a persistent serial reader that separates command
echo, raw `RX` records and `ODB` diagnostics from responses. Status/list
commands such as `CAN STATUS` finish at their documented footer rather than
an `OK` line. Stop logging before configuration or firmware uploads.

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
| 1.7.2 | 2026-01-28 | Added SYS BOOTLOADER, fuel consumption, DTE commands |
| 1.7.0 | 2026-01-26 | Added OTA firmware update commands |
| 1.6.0 | 2026-01-26 | Added CAN config upload commands |
| 1.5.0 | 2026-01-22 | Initial serial command interface |
