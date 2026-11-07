# CAN capture and OBD references for calibration

> **Languages:** [Français](OBD_CALIBRATION.fr.md) | **English**

In real vehicle mode, `LOG ON` enables two functions: recording all frames
read from CAN with their raw bytes unchanged, and querying OBD-II **service
01** for independent references. With the exact `Nissan Juke F15` profile,
it also attempts four candidate Nissan **service 22** reads. Every diagnostic
line starts with **`ODB`**, as requested. OBD-II is the protocol name; `ODB` is the exact log prefix.
`LOG OFF` stops new queries and capture; it cannot cancel a transmission
already queued in the controller.

OBD readings change neither the preset nor the decoded values sent to the
radio. The [14 October CAN/OBD capture and documented mapping](JUKE_F15_CAN_MAPPING.md)
led to explicit corrections in the repository preset: RPM `0x180 B0–B1 /8`,
speed `0x284 B4–B5 /100`, parking brake `0x5C5 B0 bit2`, brake switch
`0x180 B7 bit0` and raw fuel-related counter `0x551 B1`. These are periodic
CAN fields, not new OBD commands. Brake-switch polarity and the raw counter's
physical interpretation remain to be checked.

The whole-byte `0x5C5` fuel gauge and both `0x580` consumption mappings were
removed. In the Juke profile, `FuelValid: 0` marks the compatibility
`Fuel: 0 L` value as unavailable; `FuelConsValid: INST=0 AVG=0` prevents
consumption UART packets. `FuelRaw` is not L/h or L/100 km. Steering,
ambient temperature, odometer and `0x54C` range retain their legacy formulas
and require physical validation. The withdrawn experimental PR profile
remains historical; the new changes follow the cited vehicle evidence.

The [Nissan command research and coverage table](NISSAN_DIAGNOSTIC_RESEARCH.md)
explain which values can actually be checked. No additional manufacturer
query has been validated for the 2011 Juke MR16DDT with manual transmission:
doors, steering, indicators, lights, parking brake, average consumption and
remaining range currently have no queried OBD reference.

## Usage

1. Install the **new application OTA** containing the extended OBD client.
   This capture update does not require a new JSON: keep the existing F15
   preset and select the profile named exactly `Nissan Juke F15` to enable
   the candidate Nissan reads. Application OTA does not replace LittleFS.
2. Open the USB serial output and save **all lines**, including `RX 0x`,
   `ODB RAW_RX`, `ODB RX`, `ODB PROBE_RX` and diagnostic errors, without
   adding timestamps or color prefixes. For the PlatformIO monitor, use
   `.venv/bin/pio device monitor --filter direct`.
3. Send `LOG ON`. Wait for `ODB SUPPORTED`, then for `ODB RX` lines for
   advertised PIDs and any candidate-probe results. The ESP32 performs the
   reads itself; an ELM327 does not need to share the vehicle connector.
4. Annotate actual conditions: ignition, engine start, cold/warm idle,
   displayed RPM, fuel gauge, speed and instrument cluster units. Where
   practical, mark clutch presses and neutral/reverse changes while stopped.
   Send `LOG OFF` when finished.

Mock mode sends no OBD requests. Polling pauses during OTA and restarts with
a new discovery if `LOG ON` is still active.

## Parameters and transport

The client discovers capabilities using PIDs `00`, then `20` and `40` if
an ECU advertises the following pages. Responses are stored separately for
the eight possible addresses, `0x7E8` through `0x7EF`. Measurement queries
target the physical address `0x7E0` through `0x7E7` of the first ECU that
advertises the requested PID. Unadvertised parameters are not queried.

There are **20 supported standard PID definitions**; only PIDs advertised by
an ECU are queried. `A`, `B`, `C`, `D` below are successive measurement bytes
following the service/PID, not ISO-TP framing bytes. `AB = 256×A+B` and
`CD = 256×C+D`; both words are unsigned, most significant byte first.

| PID | Reference | Bytes | Conversion | Unit |
| --- | --- | --- | --- | --- |
| `0C` | Engine speed | 2 | `AB/4` | rpm |
| `0D` | Vehicle speed | 1 | `A` | km/h |
| `05` | Engine coolant temperature | 1 | `A−40` | °C |
| `2F` | Fuel level | 1 | `100×A/255` | % |
| `42` | Control module voltage | 2 | `AB/1000` | V |
| `46` | Ambient temperature | 1 | `A−40` | °C |
| `0B` | Intake manifold absolute pressure | 1 | `A` | kPa |
| `5E` | Fuel rate | 2 | `AB/20` | L/h |
| `03` | Fuel-system status, systems 1 and 2 | 2 | separate `A`, `B` bitmasks | bitmask |
| `04` | Calculated engine load | 1 | `100×A/255` | % |
| `06` | Short-term fuel trim, bank 1 | 1 | `100×A/128−100` | % |
| `07` | Long-term fuel trim, bank 1 | 1 | `100×A/128−100` | % |
| `0F` | Intake air temperature | 1 | `A−40` | °C |
| `10` | Mass air flow | 2 | `AB/100` | g/s |
| `11` | Throttle position | 1 | `100×A/255` | % |
| `13` | Oxygen-sensor presence | 1 | `A` bitmask | bitmask |
| `24` | Wide-range oxygen sensor 1: equivalence ratio and voltage | 4 | `AB/32768`; `CD/8192` | ratio; V |
| `44` | Commanded fuel/air equivalence ratio | 2 | `AB/32768` | ratio |
| `51` | Fuel type | 1 | `A` enumeration | enum |
| `5C` | Engine oil temperature | 1 | `A−40` | °C |

The original eight PID definitions remain available. PIDs `03` and `24`
each produce two `ODB RX` records for one response. Status/presence bitmasks
and the fuel-type enumeration are recorded as raw values, not percentages.

The log uses the literal labels `unit=bitmask`, `unit=enum`, `unit=ratio`;
standard temperatures use `unit=degC`. Distinguish the two values from one
response by `name` as well: `fuel_system_1` and `fuel_system_2` for `03`;
`wideband_equivalence_ratio_1` and `wideband_o2_voltage_1` for `24`.

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
timeout, never an invented zero value. Neither interval blocks raw CAN/USB
streaming.

After discovery, the standard scheduler allocates **two fast slots for each
normal slot**. The fast cycle is `10, 03, 10, 24, 10, 44`, skipping unsupported
PIDs. Other advertised PIDs rotate through the normal slots. This prioritizes
MAF and mixture information without claiming simultaneous measurements:
timestamps and request/ECU identifiers must be retained for comparisons.

### Candidate Nissan reads: exact F15 profile only

These are **read-only service `22` probes**, separate from standard PID
support discovery. They are enabled only when the active profile name is
exactly `Nissan Juke F15` and ECU `0x7E8` answered standard Mode 01 support
discovery. Requests then target physical address `0x7E0`; other responding
ECUs are not used for these probes. Probing runs within the standard polling
cycle and therefore also requires at least one of the 20 standard PIDs to
be advertised by a responding ECU.

| DID | Candidate purpose | Recorded data | Unit |
| --- | --- | --- | --- |
| `111F` | Oil-temperature candidate | unsigned first data byte `A`, plus candidate `A−50` | degC |
| `1114` | Fuel-gauge candidate | unsigned first data byte `A`, no percent conversion | raw |
| `1183` | Gear candidate | unsigned first data byte `A`, no gear enumeration | raw |
| `1304` | Clutch candidate | unsigned first data byte `A`, no boolean/bit interpretation | raw |

The first probe uses a normal slot after discovery. Further probes use at
most one normal slot every **10 seconds**. Candidates rotate; a positive
response remains eligible for subsequent reads. A timeout or terminal negative
response disables that DID for the current `LOG ON` session; a later positive
reply does not undo a terminal refusal. NRC `0x78` (response pending) may be
followed by a positive reply within the original 250 ms response window;
it does not extend that window. Without a positive reply, the attempt times
out and the DID is disabled for this capture. A local TX queue failure does
not disable the DID; it remains eligible in the 10-second rotation. Restarting
the capture permits a new attempt. Standard polling continues independently.

No `10 C0` session-opening request, `10 81` session-closing request, actuator
command, adaptation or fault-clearing command is sent. Some published Nissan
read definitions use a different diagnostic session: refusal or silence here
does not prove that the vehicle has no such sensor. Even a positive reply
does not physically validate the proposed label or oil-temperature formula
on this MR16DDT manual; keep the exact raw bytes for later verification.
The fuel gauge remains raw because published conversions disagree.

Standard PIDs may be absent from the advertised bitmap; candidate probes
may be refused or time out. Missing values are expected possibilities, not
zero measurements. These reads do not change the preset, dashboard values
or the data sent to the radio, and do not calibrate CAN mappings automatically.

The client accepts only standard 11-bit, non-RTR responses with DLC 8,
carried in an ISO-TP single frame whose length, service and PID or DID match
the pending request. Negative responses are reported separately. Multi-frame
messages, other manufacturer identifiers and control commands are not
interpreted by this client.

## Log format

**Synthetic** excerpt, available in `test/fixtures/obd_calibration_example.txt`:

```text
ODB TX t_ms=500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RX t_ms=508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

Another **synthetic** example shows the separate candidate read:

```text
ODB TX t_ms=2000 seq=5 can_id=0x7E0 service=0x22 did=0x111F status=queued data=03 22 11 1F 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 62 11 1F 56 00 00 00 | t_ms=2010 ext=0 rtr=0
ODB PROBE_RX t_ms=2010 seq=5 request_id=0x7E0 ecu=0x7E8 service=0x22 did=0x111F name=nissan_oil_temperature_candidate raw=0x56 value=36.000 unit=degC latency_ms=10
```

The other candidate names are `nissan_fuel_level_raw` (`1114`),
`nissan_gear_position_raw` (`1183`) and `nissan_clutch_switch_raw` (`1304`).

- `RX`: raw CAN frame, with unchanged ID, DLC, bytes and flags.
- `ODB RAW_RX`: raw frame received in the diagnostic range `0x7DF–0x7EF`.
- `ODB TX`: request with its exact bytes and `queued` or `failed` status.
- `ODB SUPPORTED`: received capability page, bitmap and ECU.
- `ODB RX`: decoded service 01 reference with PID, ECU, sequence, unit and latency.
- `ODB PROBE_RX`: candidate service 22 result, with `service=0x22`, `did=0x....`,
  `name=...`, `raw=0x..`, `value=...`, `unit=degC` or `unit=raw`, plus request,
  ECU, sequence, timestamp and latency. The raw byte remains visible even
  when a candidate conversion is displayed.
- `ODB NEGATIVE`, `ODB TIMEOUT`: negative response or missing response.
  Service 22 records use `service=0x22 did=0x....`; their `ODB TX` records also
  include the service and DID, with exact transmitted bytes. Existing service
  01 record formats and the `RX 0x` / `ODB RAW_RX` prefixes are unchanged.
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
Mappings are never changed automatically. Candidate service 22 records are
separate from service 01 `ODB RX` references: the comparison commands above
do not establish a physical calibration for the raw Nissan candidates.
Preserve `ODB PROBE_RX` and their associated raw frames in the full export.

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
