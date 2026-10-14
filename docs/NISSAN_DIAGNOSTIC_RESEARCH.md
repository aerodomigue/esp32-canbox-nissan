# Nissan diagnostics — 2011 Juke F15, 1.6 DIG-T, manual transmission

> **Languages:** [Français](NISSAN_DIAGNOSTIC_RESEARCH.fr.md) | **English**

Research conducted on **14 October 2026** to extend the `ODB` calibration
references. Target vehicle: **MR16DDT** engine, manual transmission.

## Outcome for the firmware

**No additional manufacturer-specific command was selected.** The public
sources examined provide specific leads, but none offers sufficient evidence
to add the missing measurements on this vehicle. This does not mean CONSULT
cannot read them: a verified correspondence between the vehicle's ECU, the
command and the returned bytes is still missing.

The firmware retains the eight service `01` reads and their capability
discovery. This research does not enable any service `21`/`22`, session change,
DID scan or additional CAN mapping. The log format, USB buffer and dashboard
values remain unchanged.

## Actual coverage of the values to check

A service `01` read is sent only if an ECU advertises support for the PID.
A missing response produces a timeout, never a fabricated zero value.

| Value | Reference currently requested | Comparison limitation |
| --- | --- | --- |
| Engine speed | `01 0C`, rpm | Support advertised by the ECU |
| Vehicle speed | `01 0D`, km/h | Resolution of 1 km/h |
| Fuel level | `01 2F`, % | Does not directly validate liters |
| Battery voltage | `01 42`, V | ECU voltage, not a measurement at the battery terminals |
| Steering wheel angle | None | Manufacturer-specific mapping not verified for this Juke |
| Doors | None | Manufacturer-specific mapping not verified for this Juke |
| Turn signals | None | Distinguish stalk commands from lamp states |
| Lighting | None | Distinguish manual selection, AUTO and lamp states |
| Parking brake | None | Do not use the brake pedal switch |
| Outside temperature | `01 46`, °C | Support advertised by the ECU |
| Instantaneous consumption | `01 5E`, L/h | Fuel flow rate; no automatic conversion to L/100 km |
| Average consumption | None | No verified command for the value displayed by the instrument cluster |
| Remaining range | None | No verified command for the value displayed by the instrument cluster |

The two other existing reads are coolant temperature (`01 05`) and manifold
absolute pressure (`01 0B`).
Implementation: [ObdCalibration.cpp](../src/ObdCalibration.cpp).
Received CAN frames are still captured raw, even without a decoded OBD
reference. The current preset also has no parking brake field.

The [new periodic-frame study](JUKE_F15_CAN_MAPPING.md) nevertheless identifies
its CAN bit `0x5C5[0] & 0x04`, corroborated by the owner's maneuver.
A periodic CAN reading and a diagnostic command are different kinds of evidence.
The recent captures query only four of the eight PIDs: `0C`, `0D`, `05` and `42`.
MAF `10` and commanded equivalence ratio `44` are advertised by the ECU but
are absent from the current polling list.

## Sources and leads examined

### Published responses for a 2012 Juke

The [OBDb Nissan-Juke repository, revision examined](https://github.com/OBDb/Nissan-Juke/tree/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf)
has a `signalsets/v3/default.json` file with no model-specific commands,
but contains **2012 response fixtures** and a list of supported commands.
An empty definition file therefore does not mean there is no data at all.
There is no 2011 test directory in this revision.

The engine fixtures use, among others, `7E0` → `7E8`. They do not identify
the engine, transmission and market needed to certify our configuration.
Entries annotated with `din=C0,dout=81` also include diagnostic session
configuration; the current client does not implement it.

Two distinctions to preserve:

- [`22 13 05`](https://github.com/OBDb/Nissan-Juke/blob/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf/tests/test_cases/2012/commands/7E0.7E8.221305%7Cdin%3DC0%2Cdout%3D81.yaml)
  is a **brake pedal** switch, not a parking brake reference.
- [`22 12 2B`](https://github.com/OBDb/Nissan-Juke/blob/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf/tests/test_cases/2012/commands/7E0.7E8.22122B.yaml)
  concerns the cruise control target speed, not the remaining range.

The other manufacturer-specific fixtures mainly concern voltage, ignition
timing, EVAP purge and temperatures. They do not provide the missing references
in the list above. See the
[2012 list](https://github.com/OBDb/Nissan-Juke/blob/4f343b5e014ff4c863a6499bb3e7d25512d2aeaf/tests/test_cases/2012/command_support.yaml).
Transmission commands using `7E1` → `7E9` are not selected for our manual
transmission.

### Generic Nissan definitions

The [OBDb Nissan signal set, revision c322e4d](https://github.com/OBDb/Nissan/blob/c322e4de23fa3564c9dd384ef9fc1030b653d646/signalsets/v3/default.json)
includes the following candidates, among others, with no corresponding Juke
fixture in the revision examined:

| TX → RX address | Command | Interpretation proposed by the source | Decision |
| --- | --- | --- | --- |
| `740` → `760` | `22 12 08` | ABS steering angle, signed 16-bit value /10 degrees | Applicability to the 2011 Juke not established |
| `7E0` → `7E8` | `22 20 79` | Several states, including the parking brake | Applicability to MR16DDT not established |
| `7E0` → `7E8` | `22 11 86` | Remaining fuel in liters | Applicability and scale not confirmed on this vehicle |

An identifier alone is insufficient: in the same signal set, `22 12 08`
addressed to engine ECU `7E0` describes an injection duration, whereas address
`740` gives it the meaning of an angle. Any future selection must identify
**TX, RX, service, identifier, session and ECU variant** together.

### Measurements published for a Qashqai J10

[balrog-kun's study, revision b1ffbe2](https://github.com/balrog-kun/nissan-qashqai-can-info/blob/b1ffbe27a60e8808539521cfd3a00959cbf791e9/README.md#bcm-non-standard-service-21-pids)
is based on a **2013 diesel Qashqai with a manual transmission**. It describes
the BCM at `745` → `765`, with a `10 C0` session before service `21` reads:

- `21 41`: switches, including turn signals and the brake pedal;
- `21 42` / `21 44`: door states and other inputs;
- `21 49` / `21 4A`: stalk and lighting selectors.

The lighting selector is not equivalent to the actual lamp state in AUTO
mode. Reading the instrument cluster at address `743` with `21 01` returns
a long block: it would also require multi-frame ISO-TP transport. The study
does not establish the same offsets on the Juke; these commands remain
research leads, not enabled references.

### Manufacturer and supplier documentation

A [copy of the Nissan **2011 JUKE** manual, EXL section, September 2010 revision](https://www.scribd.com/document/1016834926/EXL-JUKE-F15-SEP-10)
lists door, stalk and lighting monitors. Page EXL-22 also mentions `PKB SW`,
received from the instrument cluster over CAN, for the variant without
Intelligent Key. The parking brake is therefore documented as observable
through CONSULT on that variant; its absence from the firmware does not mean
it is absent from the bus. These pages do not provide commands or offsets.

The Nissan Juke 2012 manual,
[BCS section, Data Monitor](https://www.nicoclub.com/manuals/Juke/2012/BCS.pdf),
describes the BCM functions available for reading. It does not give the
address/service/identifier/offset combination needed to reproduce these
reads. Its model year remains distinct from 2011.

The [BRC section for the 2012 Juke with VDC](https://www.nicoclub.com/service-manual?fsm_download=Juke%2F2012%2FBRC.pdf),
pages BRC-40, 44 and 57, also describes the ABS steering angle monitor.
It provides reference values in degrees, but not the service, identifier
or bytes needed to request it.

The [Racelogic Juke F15 2011–2018 data sheet](https://www.racelogic.co.uk/_downloads/vbox/Vehicles/Other/Docs/Nissan-Juke%20%28F15%29%202011-2018.pdf)
lists several of the signals being sought, including parking brake, doors,
lighting and consumption. This list confirms coverage claimed by the supplier,
not public diagnostic commands or their formulas.

**14 October addendum:** the [public VBOX page](https://vboxautomotive.co.uk/en/can-database-documents)
also links the Juke REF file with IDs, fields and factors for periodic CAN
messages. The [documented mapping](JUKE_F15_CAN_MAPPING.md) compares those
definitions with captures, notably for parking brake and speed. The PDF
alone did not contain these offsets; the REF still does not provide the
corresponding CONSULT commands.

## Evidence needed for a verifiable integration

A diagnostic capture from the target vehicle is needed, associated with the
ECU identification and the values displayed by the tool during state changes.
It must preserve requests, responses, sessions and any ISO-TP exchanges.
An isolated positive response proves neither the meaning nor the scale of
a measurement.

Capturing this exchange must be passive. **`LOG ON` is not a passive mode**,
because it already starts service `01` reads; it must not be presented as
a simple observer of a CONSULT session. The current firmware provides no
separate capture-only command.

Once a command is supported by evidence, its decoding can be tested using
real responses, opposite states, incorrect lengths and timeouts before it
is enabled in `ODB` logs. The current dashboard values do not constitute
independent evidence of calibration.

## Validation of this assessment

The existing suites were run on 14 October 2026: **170 C++ tests passed**,
including 18 streaming tests and the replay of the complete historical log,
and **34 Python tests passed**. No firmware code or preset is changed.
These results verify existing software behavior; they do not constitute
tests of the candidate commands on the car.
