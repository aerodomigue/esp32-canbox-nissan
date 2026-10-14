# CAN capture and OBD references for calibration

> **Languages:** [Français](OBD_CALIBRATION.fr.md) | **English**

In real vehicle mode, `LOG ON` enables two functions: recording all frames
read from CAN with their raw bytes unchanged, and querying OBD-II **service
01** for independent references. Every diagnostic line starts with **`ODB`**,
as requested. OBD-II is the protocol name; `ODB` is the exact log prefix.
`LOG OFF` stops new queries and capture; it cannot cancel a transmission
already queued in the controller.

OBD readings change neither the preset nor the decoded values sent to the
radio. The proposed PR #10 mappings and their experimental profile have
been removed; the robustness fixes remain active. The active preset matches
PR head `495f346` exactly: RPM `/7`, speed bytes 0–1, fuel `[255,0,0,45]`,
range `0x54C ×100/338`, and instantaneous consumption byte 3. Restoring these
older mappings does not physically validate them; the capture is intended
to check them.

The [Nissan command research and coverage table](NISSAN_DIAGNOSTIC_RESEARCH.md)
explain which values can actually be checked. No additional manufacturer
query has been validated for the 2011 Juke MR16DDT with manual transmission:
doors, steering, indicators, lights, parking brake, average consumption and
remaining range currently have no queried OBD reference.

## Usage

1. Load the firmware and the active preset into LittleFS or through CANBox
   Manager, then reload the profile. An application-only OTA does not replace
   the JSON already installed on the ESP32.
2. Open the USB serial output and save the lines without adding timestamps
   or color prefixes to the file. For the PlatformIO monitor, use
   `.venv/bin/pio device monitor --filter direct`.
3. Send `LOG ON`. Wait for `ODB SUPPORTED`, then for `ODB RX` lines for the
   available parameters.
4. Annotate the actual conditions: ignition, engine start, cold/warm idle,
   displayed RPM, fuel gauge, speed and instrument cluster units. Send
   `LOG OFF` when finished.

Mock mode sends no OBD requests. Polling pauses during OTA and restarts with
a new discovery if `LOG ON` is still active.

## Parameters and transport

The client discovers capabilities using PIDs `00`, then `20` and `40` if
an ECU advertises the following pages. Responses are stored separately for
the eight possible addresses, `0x7E8` through `0x7EF`. Measurement queries
target the physical address `0x7E0` through `0x7E7` of the first ECU that
advertises the requested PID. Unadvertised parameters are not queried.

| PID | Reference | Conversion | Unit |
| --- | --- | --- | --- |
| `0C` | Engine speed | `(256×A+B)/4` | rpm |
| `0D` | Vehicle speed | `A` | km/h |
| `05` | Engine coolant temperature | `A−40` | °C |
| `2F` | Fuel level | `100×A/255` | % |
| `42` | Control module voltage | `(256×A+B)/1000` | V |
| `46` | Ambient temperature | `A−40` | °C |
| `0B` | Intake manifold absolute pressure | `A` | kPa |
| `5E` | Fuel rate | `(256×A+B)/20` | L/h |

These conversions are publicly documented in the
[CSS Electronics PID table](https://www.csselectronics.com/pages/obd2-pid-table-on-board-diagnostics-j1979).
Availability depends on the vehicle's ECUs. Fuel level in percent is not
directly a volume in liters; module voltage is not necessarily the same as
alternator voltage. No automatic conversion between L/h and L/100 km is made.

Only one request is pending at a time, with a minimum interval of **500 ms**
between attempts. The response window is **250 ms**; discovery collects
responses from multiple ECUs until that window expires. After three
discovery attempts without a usable response, the client waits five seconds
before trying again. An advertised PID that does not respond produces a
timeout, never an invented zero value.

The client accepts only standard 11-bit, non-RTR responses with DLC 8,
carried in an ISO-TP single frame whose length, service and PID match the
request. Negative responses are reported separately. Multi-frame messages,
manufacturer services and control commands are not interpreted by this client.

## Log format

**Synthetic** excerpt, available in `test/fixtures/obd_calibration_example.txt`:

```text
ODB TX t_ms=500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RX t_ms=508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

- `RX`: raw CAN frame, with unchanged ID, DLC, bytes and flags.
- `ODB RAW_RX`: raw frame received in the diagnostic range `0x7DF–0x7EF`.
- `ODB TX`: request with its exact bytes and `queued` or `failed` status.
- `ODB SUPPORTED`: received capability page, bitmap and ECU.
- `ODB RX`: decoded reference with PID, ECU, sequence, unit and latency.
- `ODB NEGATIVE`, `ODB TIMEOUT`: negative response or missing response.
- `ODB STATE`: start/stop or discovery state.
- `ODB DRIVER`: RX losses, TX failures, queue occupancy, errors and controller
  state, at most once per second.
- `ODB BUFFER`: USB queue occupancy, refused RAW/diagnostic lines, retried
  partial writes and rejected responses, at the same frequency.

`queued` means TWAI accepted the request into its queue, not that the vehicle
acknowledged it. This follows the behavior of the
[Espressif TWAI API](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32/api-reference/peripherals/twai.html).
For received frames and their associated references, `t_ms` comes from
`millis()` when dequeued from CAN, before display. `ODB TX` carries the time
of the queueing attempt; latency therefore includes software and queue
delays. It is not the hardware timestamp of the bus exchange. Timestamps
reset on reboot and wrap after approximately 49.7 days.

Reception is nonblocking, with a 128-frame queue and at most 32 frames
drained per loop. Formatting never reads more than eight bytes and does not
invent payload for an RTR frame. The decoder rejects RTR, extended,
out-of-11-bit-range frames, or frames too short for all configured fields.
Raw bytes remain visible, including for unconfigured IDs.
USB output uses a 64 KiB buffer drained without waiting for the host. Partial
writes resume at the next byte. If throughput remains insufficient and the
buffer fills, only entire new lines are refused and counted in `ODB BUFFER`.
Check these counters, `ODB DRIVER` and parser rejections before using a capture.

To separate logs from dashboard data in a client, recognize the exact
prefixes `RX 0x` and `ODB ` before command responses. Dashboard data is the
block requested by `SYS DATA`, from `=== Live Vehicle Data ===` to
`=========================`; there is no `DASH` or standalone `TX` prefix.
See the [USB routing contract](protocols/USB_SERIAL_PROTOCOL.md#routing-the-shared-stream)
and the [firmware streaming validation (French)](LOG_STREAMING_VALIDATION.fr.md).

## Comparison on the VM

```bash
.venv/bin/python -B tools/analyze_can_log.py capture.log
.venv/bin/python -B tools/compare_obd_can.py capture.log \
  --preset data/NissanJukeF15.json --max-delay-ms 200 \
  --output-dir .local-validation/obd-comparison
```

Add `--ecu 0x7E8` to select a single ECU. The tool keeps session/PID/ECU/unit
streams separate, pairs the closest timestamped frames, and produces
`comparison.json` and `matches.csv`. It compares the active preset's output
with several hypotheses, including RPM `/7`, `/8` and the two speed offsets.
PIDs without a CAN candidate remain listed in the report. Different units
do not produce a numerical error; a constant capture cannot confirm a scale.
Mappings are never changed automatically.

Boot messages, echoes and command responses may also appear in a full export.
The reader reports them as unrecognized records; inspect the rejection
reasons to distinguish these from an invalid CAN ID, DLC or payload. Keep
session markers in the file.

Changes in `ODB STATE enabled=0/1` and boot markers separate sessions: a
reference is never paired with a frame from another session. A reboot
without a marker, or captures concatenated without delimiters, must be
analyzed in separate files. References are rejected if the ECU is outside
`0x7E8–0x7EF` or its address does not match the functional request `0x7DF`
or the physical request `ECU−8`.

```bash
.venv/bin/python -B tools/compare_obd_can.py test/fixtures/obd_calibration_example.txt \
  --output-dir .local-validation/obd-example-comparison
.venv/bin/python -B -m unittest discover -s tools -p 'test_*can*.py' -v
.venv/bin/pio test -e native -e native_radio -e native_serial
```

In the synthetic example, the RPM `/8` and speed bytes 4–5 hypotheses match
the OBD references, while the original preset differs. This checks the
comparison mechanism; older frames without timestamps or OBD responses
cannot provide this calibration.
