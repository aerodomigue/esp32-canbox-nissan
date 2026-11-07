# CAN capture and OBD references for calibration

> **Languages:** [Français](OBD_CALIBRATION.fr.md) | **English**

In real vehicle mode, `LOG ON` records all received CAN frames with unchanged
raw bytes and queries advertised OBD-II **service 01** references. With the exact
`Nissan Juke F15` profile it also attempts **five candidate service 22 reads in
three bounded `10C0`/`1081` diagnostic groups**. Every diagnostic line starts with
**`ODB`**; OBD-II is the protocol name, `ODB` the exact log prefix.
`LOG OFF` stops capture and measurements, with a possible single best-effort
session-closing transmission. Queued controller transmissions cannot be recalled.

Diagnostic readings change neither the preset nor the values sent to the radio.
The [14 October evidence](JUKE_F15_CAN_MAPPING.md) remains tied to its historical
preset and low-speed capture; do not reinterpret those old comparisons as proof
of the current configuration. The [current F15/DUDU mapping](F15_DUDU_BOOT_TRIP_MAPPING.md)
uses speed `0x280 B4..B5 /100`, range `0x2DE B6..B7 /10`, RPM `0x180 B0..B1 /8`,
validated coolant `0x551 B0−40`, and explicitly provisional oil `0x580 B4−40`.
These periodic CAN sources are distinct from diagnostic PIDs/DIDs. Consumption
uses the provisional fuel counter calibration and a RAM-only boot-trip average.

The false whole-byte `0x5C5` fuel gauge and old `0x580` consumption interpretations
remain removed. `FuelValid: 0` distinguishes the compatibility `Fuel: 0 L` from
a measured fuel quantity. `FuelRaw` is not L/h or L/100 km. The incorrect outside
source is removed without a fabricated replacement. Current validity/provisional
flags and UART restrictions are described in the current mapping guide above.

The [Nissan command research and coverage table](NISSAN_DIAGNOSTIC_RESEARCH.md)
explain which values can actually be checked. No additional manufacturer
query has been validated for the 2011 Juke MR16DDT with manual transmission:
doors, steering, indicators, lights, parking brake, average consumption and
remaining range currently have no queried OBD reference.

## Usage

1. Install the **new application OTA** containing the extended OBD client.
   Select the profile named exactly `Nissan Juke F15` for the diagnostic groups.
   Also transfer the updated JSON separately when deploying the current F15/DUDU
   vehicle mapping: application OTA does not replace LittleFS presets.
2. Open the USB serial output and save **all lines**, including `RX 0x`,
   `ODB RAW_RX`, `ODB RX`, `ODB PROBE_RX`, `ODB SESSION_RX`, `ODB SESSION_STATE` and errors, without
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

The five **service `22` candidates** are distinct from standard PID support
pages. They run only for the exact `Nissan Juke F15` profile in real capture
mode, after engine ECU `0x7E8` answers standard capability discovery. Their
physical request address is `0x7E0`; other responding ECUs are not substituted.
The standard discovery must also find at least one of the 20 supported PID
definitions before entering its polling stage.

| DID | Candidate purpose | Recorded data | Unit |
| --- | --- | --- | --- |
| `1101` | Coolant control candidate | unsigned first data byte `A`, candidate `A−50` | degC |
| `111F` | Oil-temperature candidate | unsigned first data byte `A`, candidate `A−50` | degC |
| `1114` | Fuel-gauge candidate | unsigned first data byte `A`, no percent or volume conversion | raw |
| `1183` | Gear candidate | unsigned first data byte `A`, no gear enumeration | raw |
| `1304` | Clutch candidate | unsigned first data byte `A`, no boolean/bit interpretation | raw |

The firmware performs three short manufacturer-session groups. Each group is
attempted once per enabled capture session, with at least **10 seconds between
group starts**. Restarting real capture after OTA begins a new discovery and
probe budget. The first group starts in an ordinary polling slot:

```text
10 C0 → 22 11 01 → 22 11 1F → 10 81
10 C0 → 22 11 14 → 22 11 83 → 10 81
10 C0 → 22 13 04             → 10 81
```

Each opening is `02 10 C0 00 00 00 00 00` on `0x7E0`. Only a matched positive
`50 C0` from `0x7E8` authorizes its reads; a positive echo `50 40` is not
silently treated as equivalent. The final `02 10 81 00 00 00 00 00` requests
closure and records positive `50 81`, a refusal or timeout. These are bounded
**diagnostic state changes**, not merely bare service-22 reads. Car Scanner's
original Nissan CAN11 implementation declares this opening/closing pair.

There is still only **one pending request**, a minimum **500 ms** attempt gap
and a **250 ms** response window. A group pauses normal OBD polling while
raw CAN reception, USB streaming and radio work continue without blocking.
With two readings it normally has four attempts 500 ms apart. Once the read
phase reaches **2 seconds**, delayed processing closes instead of adding more
reads. No keepalive, security access, actuator, adaptation, DTC clear, reset,
programming session or unbounded DID/ECU scan is added.

An opening refusal/timeout means no reads are sent and remaining groups are
stopped after cleanup. An unconfirmed closing also stops further groups and
logs `state=unknown`; standard polling resumes. A data-read refusal or timeout
is logged without inventing a measurement and does not prove other DIDs absent.
In particular a rejected coolant control does not suppress the oil attempt in
an accepted session. Terminal negative replies cannot be overridden by a later
positive reply for that transaction. NRC `0x78` may precede a positive within
the original 250 ms window but never extends it. Local send failures do not
fabricate an open session or a received value.

`LOG OFF`, profile change, mock or OTA cancellation attempts one best-effort
close when an opening may have been sent and the 500 ms gap allows it. It does
not wait synchronously. A queued close is **unconfirmed**, and if spacing or
TX failure prevents cleanup, `ODB SESSION_STATE` explicitly reports unknown
state. Already queued CAN transmissions cannot be recalled. Toggling profiles
within an active capture does not re-enable groups already attempted.

The owner's earlier controlled capture contains four `7F 22 80` refusals
**without a `10C0` opening**. Those refusals do not prove sensor absence or
session acceptance. Even a future positive reply does not physically validate
a label or formula on this exact ECU. Generic published fuel-gauge conversions
disagree; the MY2011 ECM monitor exposes a voltage while the cluster exposes
litres. Consequently `1114` stays raw. The oil sensor and a CONSULT °C monitor
are documented for the MY2011 MR16DDT family, but neither `111F` support nor the
identity of broadcast `0x580 B4` has been independently confirmed here.

Only standard 11-bit, non-RTR DLC 8 responses carried in an ISO-TP single frame
are decoded. Length, physical ECU, service and PID/DID/session echo must match.
A candidate data response is exactly `62 DID-high DID-low A` (payload length 4);
session-positive payloads may contain two through seven bytes, with their
additional bytes retained in `ODB RAW_RX`. Multi-frame ISO-TP is not reassembled;
its raw frames remain in the capture without applying a fixed data offset.
Standard missing PIDs and refused candidates remain unavailable, never zero.
No diagnostic result overwrites the preset, dashboard or radio measurements.

See [the F15/DUDU contract and source links](F15_DUDU_BOOT_TRIP_MAPPING.md#bounded-nissan-diagnostic-experiment)
for session provenance and the separate vehicle-side provisional mappings.

## Log format

**Synthetic** excerpt, available in `test/fixtures/obd_calibration_example.txt`:

```text
ODB TX t_ms=500 seq=2 can_id=0x7E0 service=0x01 pid=0x0C status=queued data=02 01 0C 00 00 00 00 00
RX 0x180 [8]: 14 18 00 00 00 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=508 ext=0 rtr=0
ODB RX t_ms=508 seq=2 request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643.000 unit=rpm latency_ms=8
```

A complete **synthetic** session group, not an observed ECU exchange:

```text
ODB TX t_ms=2000 seq=5 can_id=0x7E0 service=0x10 subfunction=0xC0 status=queued data=02 10 C0 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 02 50 C0 00 00 00 00 00 | t_ms=2010 ext=0 rtr=0
ODB SESSION_RX t_ms=2010 seq=5 request_id=0x7E0 ecu=0x7E8 service=0x10 subfunction=0xC0 status=opened length=2 latency_ms=10
ODB TX t_ms=2500 seq=6 can_id=0x7E0 service=0x22 did=0x1101 status=queued data=03 22 11 01 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 62 11 01 6E 00 00 00 | t_ms=2510 ext=0 rtr=0
ODB PROBE_RX t_ms=2510 seq=6 request_id=0x7E0 ecu=0x7E8 service=0x22 did=0x1101 name=nissan_coolant_temperature_candidate raw=0x6E value=60.000 unit=degC latency_ms=10
ODB TX t_ms=3000 seq=7 can_id=0x7E0 service=0x22 did=0x111F status=queued data=03 22 11 1F 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 04 62 11 1F 6F 00 00 00 | t_ms=3010 ext=0 rtr=0
ODB PROBE_RX t_ms=3010 seq=7 request_id=0x7E0 ecu=0x7E8 service=0x22 did=0x111F name=nissan_oil_temperature_candidate raw=0x6F value=61.000 unit=degC latency_ms=10
ODB TX t_ms=3500 seq=8 can_id=0x7E0 service=0x10 subfunction=0x81 status=queued data=02 10 81 00 00 00 00 00
ODB RAW_RX 0x7E8 [8]: 02 50 81 00 00 00 00 00 | t_ms=3510 ext=0 rtr=0
ODB SESSION_RX t_ms=3510 seq=8 request_id=0x7E0 ecu=0x7E8 service=0x10 subfunction=0x81 status=closed length=2 latency_ms=10
```

The candidate names are `nissan_coolant_temperature_candidate` (`1101`),
`nissan_oil_temperature_candidate` (`111F`), `nissan_fuel_level_raw` (`1114`),
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
  include the service and DID, with exact transmitted bytes. Session records use
  `service=0x10 subfunction=0xC0` or `subfunction=0x81`. Existing service
  01 record formats and the `RX 0x` / `ODB RAW_RX` prefixes are unchanged.
- `ODB SESSION_RX`: matched positive manufacturer-session echo, `service=0x10`,
  `subfunction=0xC0`/`0x81`, opened/closed status, payload length and latency.
- `ODB SESSION_STATE`: bounded read deadline, cancellation or unconfirmed cleanup;
  `state=unknown` must not be presented as confirmed closure.
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
