# CAN mapping — Nissan Juke F15 2011 phase 1, 1.6 DIG-T manual

> **Languages:** [Français](JUKE_F15_CAN_MAPPING.fr.md) | **English**

Knowledge as of **15 October 2026**. Owner's vehicle: Juke F15 phase 1,
MR16DDT engine, manual transmission. The logs do not identify the market,
exact ECU versions or JSON actually loaded into LittleFS. The **baseline
preset assessed in the evidence** is `data/NissanJukeF15.json` at revision
`cd02abf`; its hash is included in the results. The published JSON/CSV
statistics remain comparisons against that historical snapshot.

## Application in the current firmware and preset

The repository's [Juke preset](../data/NissanJukeF15.json) applies the
corroborated corrections and preserves the raw byte alongside a provisional
consumption estimate:

| Target | CAN field / conversion | Scope and limitation |
| --- | --- | --- |
| `STEERING` | `0x002 B0–B1 I16LE`, raw 0.1° units | Corrected extraction; `scale=1000`, `offset=0`, inversion; UART `0x29` bounded to ±500 |
| `ENGINE_RPM` | `0x180 B0–B1 BE /8` | Integer rpm; supported by the OBD comparisons below |
| `VEHICLE_SPEED` | `0x284 B4–B5 BE /100` | Integer km/h; corroborated over the captured low-speed range |
| `HANDBRAKE` | `0x5C5 B0 bit2` | `SYS DATA` → `Handbrake`; observed 1 = applied |
| `BRAKE_PEDAL` | `0x180 B7 bit0` | `SYS DATA` → `Brake`; source-defined switch bit, action/polarity still to verify |
| `FUEL_COUNTER_RAW` | `0x551 B1`, unchanged 0–255 | `SYS DATA` → `FuelRaw`; separate counter conversion uses a provisional coefficient |

The false fuel gauge from the whole `0x5C5 B0` byte and both `0x580`
consumption fields have been removed. `Fuel: 0 L` remains for compatibility,
with `FuelValid: 0`: it is **unavailable**, not an empty-tank measurement.
The `fuelConsumption` object now enables an estimate from modulo-256
increments of `0x551 B1`, using **80 mm³/increment, not yet calibrated on
this Juke**, and distance integrated from fractional `0x284 B4–B5 /100`
speed. It supplies UART `0x22` / `0x23` in 0.1 L/100 km. `FuelConsValid`
describes availability; `FuelConsEstimated: 1` identifies the provisional
coefficient. Mock mode retains synthetic consumption output.

Instantaneous consumption uses a 1 s window and displays zero below 2 km/h
with fresh sources; this does not mean zero fuel flow. Average consumption
includes stationary fuel use and becomes available after 100 m in the
session. Reload, reboot, a source gap over 500 ms or an implausible increment
resets the session. Some counter resets cannot be distinguished from a
rollover. An unavailable instantaneous value is not sent; the radio may
retain its last display. An unavailable average and the five unused history
slots are `0xFFFF` in the corrected 13-byte `0x23` payload. See the
[configuration guide](VEHICLE_PRESET_GUIDE.md#counter-based-fuel-consumption-fuelconsumption)
and [radio protocol](technical/RADIO_SEND.md).

`Handbrake`, `Brake` and `FuelRaw` are `unknown` until their configured CAN
frame is received. Availability resets when a profile is loaded; it indicates
reception, not physical calibration or a freshness timeout for these raw
fields. Existing `RX` and `ODB` records, OBD polling and indicator handling
remain unchanged. DTE, ambient temperature and odometer retain
their legacy mappings and the limitations documented below.

The [applied-preset replay](vehicle/nissan-juke-f15/evidence/20261014-210614-applied-preset.json)
uses production integer divisions on the same capture: mean absolute error
**7.389 rpm** across 36 pairs, **0.143 km/h** across 35 pairs and
**0.029 °C** across 35 coolant pairs. These differ from the floating-point
candidate statistics below. Replaying an existing low-speed capture does not
establish high-speed/high-rpm calibration or a new hardware measurement.

Install the matching **new firmware before the JSON**. Old parsers can map
an unknown target to `STEERING`; the new parser rejects unknown or missing
targets while preserving the active profile. Application-only OTA does not
update LittleFS: transfer the JSON separately, then use
`CAN LOAD NissanJukeF15.json`. See [installation](../README.md#installation--setup)
and the [serial protocol](protocols/USB_SERIAL_PROTOCOL.md#sys-data).

### F15 steering to RAV4 UART

The capture contains 7,181 `0x002` frames. `I16LE(B0,B1)/10` ranges from
−505.7° to +388.7°, starting at −4.4° and ending at −2.4°. Its absolute
derivative correlates with VBOX rotational speed `B2×4 °/s` at 0.999162 over
7,180 intervals. This strongly supports extraction and scaling; it does not
measure physical stops or the straight-ahead zero. The former B1–B2 read
mixed angle and rotational speed.

The preset retains raw tenths of a degree. Radio output applies
`steerScale=1000`, `steerOffset=0`, `steerInvert=true`:
`UART = clamp(-trunc(raw/10), -500, 500)`. The limit matches the `0x29`
decoder in the [examined APK](technical/RADIO_SEND.md#1-steering-wheel-angle-command-0x29).
One UART unit per degree is the accepted starting calibration for this vehicle;
guideline direction and amplitude still need checking on the head unit.
Toyota CAN `0x025` and its possible factor do not establish this UART unit.

When updating the same preset, explicitly update the saved calibration after
transferring the JSON and running `CAN LOAD NissanJukeF15.json`:

```text
CFG SET steerScale 1000
CFG SET steerOffset 0
CFG SET steerInvert 1
CFG SAVE
```

Loading the same filename preserves NVS values; application OTA updates
neither the LittleFS JSON nor these settings. No CANBox Manager change is needed.

The following study records the evidence that led to these changes;
proposals are assessed independently of their inclusion in an earlier PR.

## Results against the historical baseline

- Engine speed from `0x180`, bytes 0–1 BE divided by **8**, is strongly
  supported by 36 OBD references; the baseline preset divisor **7** is contradicted.
- Vehicle speed from `0x284`, bytes **4–5** BE divided by 100, agrees well
  with OBD over the captured range, **0–16 km/h**. The baseline preset's bytes 0–1
  do not directly represent this speed with its old factor.
- Engine coolant temperature `0x551[0]−40` agrees with OBD over **61–67 °C**.
- The parking brake is strongly corroborated in **`0x5C5[0] & 0x04`**:
  a Juke-specific source, two transitions and the owner's account agree.
- `0x580[3]`, previously used as instantaneous fuel consumption, follows an exact
  cyclic counter; it does not provide a usable consumption reading.
- A Juke-specific Racelogic definition is now available with precise
  fields. Its 2011–2018 coverage does not guarantee every engine,
  transmission or market; it must be checked against the captures.

## Captures and context

| Capture | Contents and use | Limitation |
| --- | --- | --- |
| Historical export added in January 2026 | 1,013 accepted frames, 3 rejected lines, 22 IDs | Suspect old recorder, no timestamps or OBD references; see the [historical analysis in French](JUKE_2011_CAN_ANALYSIS.fr.md) |
| `canlog_20261014_193212_312.txt` | 165 OBD exchanges, 162 measurements, 82.369 s | No ordinary `RX`: validates references, not their broadcast CAN equivalents |
| `canlog_20261014_210614_663_762622981087673198.txt` | 92,610 `RX`, 36 ordinary IDs, 144 OBD exchanges, 141 measurements, 71.819 s | One sequence of low-speed manoeuvres; not calibration over the full range |

The owner describes the latest sequence: parking brake released, departure
from a parking space and a left turn, an estimated movement of roughly
30 m, reversing over another estimated distance, parking in reverse and
then applying the parking brake. These distances were not measured and no
precise action timestamps were supplied. They are not used to calibrate
speed, distance or angle. The owner separately confirms that the dashboard
indicators work again.

SHA-256 hashes, in table order:

```text
35de3a33b22716b88b28a2adfa1e6d6ed916b230cbbe2bba569b2206700df5c6
b6a28c234ec2b6425d1391b8db79fe5f34835f1a412ba8aa5fd79a5bdcf13ac2
e5c2a7539b3ad13da7c04cc15883a4b271a67c76fb3280a61878523d13311ce3
```

The latest file contains **93,188 complete CRLF lines**, with no format
rejections. Its 144 request sequence numbers are contiguous and all have
consistent raw responses and decoded values. Software latencies are
0–4 ms. There are 639–647 `RX` frames between requests spaced 500 ms apart.
302 `RX` frames occur between a request and its response, across 69
exchanges: reception continues while waiting for OBD. No loss or error
counter increases. This validates this capture; it does not prove that no
physical vehicle frame has ever been lost.

Available results: [JSON summary](vehicle/nissan-juke-f15/evidence/20261014-210614-summary.json),
[283 CAN/OBD matches](vehicle/nissan-juke-f15/evidence/20261014-210614-matches.csv),
[ID and interval inventory](vehicle/nissan-juke-f15/evidence/20261014-210614-can-ids.csv),
[signal and counter analysis](vehicle/nissan-juke-f15/evidence/20261014-210614-signals.json).
The complete source files are retained separately; CSVs include source
line numbers without inventing new measurements.

## Conventions and protocol layers

`B0` is the first byte; `bit0` is its least significant bit.
`U16BE(Ba,Bb) = 256×Ba+Bb`. `I16LE` means signed 16-bit little endian.
Periods are observed in the log, not guaranteed deadlines.

The capture contains standard 11-bit classical CAN (`ext=0`, `rtr=0`),
with periodic Nissan messages. Racelogic specifies **500 kbit/s** and OBD
pins 6/14 for this Juke family. A CAN message ID is not automatically an
ECU address.

Diagnostic exchanges are separate: service `01` requests on `0x7DF` then
`0x7E0`, responses on `0x7E8`, using ISO-TP single frames in these files.
An actual structure example: `02 01 0C ...` requests engine speed;
`04 41 0C A B ...` carries its reference `(256×A+B)/4`.
This client does not decode manufacturer-specific CONSULT services.

The repository's Raise/Toyota PDFs describe the **UART connection to the
head unit**. Their commands and units do not determine the IDs or scaling
of incoming Nissan CAN frames. The exact USB prefix remains `ODB`, although
the protocol is called OBD; see the [capture guide](OBD_CALIBRATION.md).

## Mapping table and evidence levels

“Corroborated” means agreement with references or observed actions in this
capture, not universal validation. “Candidate” still requires an independent
measurement. “Contradicted” concerns the preset interpretation, not whether
the signal exists in the vehicle.

| Signal | Field / formula assessed | Evidence and status | Difference from baseline preset (`cd02abf`) |
| --- | --- | --- | --- |
| Engine speed | `0x180 B0–B1 BE /8`; copy `0x1F9 B2–B3 /8` | **Corroborated**: 36 references within 5 ms, mean absolute error 7.55 rpm, maximum 28.125; OBD 612.5–1,762.5 | `/7` gives a mean error of 131.22 rpm |
| Vehicle speed | `0x284 B4–B5 BE /100` | **Corroborated over a limited range**: 35 references within 10 ms; mean absolute error 0.211 km/h, maximum 1.53 | Preset reads B0–B1; example 32 versus 16 km/h OBD |
| Engine coolant temperature | `0x551 B0−40`, °C | **Corroborated**: 34/35 equal, one difference of 1 °C, within 51 ms | Current formula agrees |
| Voltage | `0x6F6 B0 /10`, V | **Consistent**: mean absolute difference 0.139 V, maximum 0.44 V against ECU voltage | Keep the measurement points distinct; not battery calibration |
| Parking brake | `0x5C5 B0 bit2` | **Strongly corroborated**: 1→0→1, owner's account and Juke Racelogic definition | Baseline has no parking-brake field and uses this byte for fuel level |
| Fuel level | Whole byte `0x5C5 B0` | **Contradicted as a direct fuel gauge**: 0x44→0x40→0x44 with the parking brake | Do not interpret combined states as litres; actual gauge unidentified |
| Instantaneous fuel consumption | `0x580 B3` | **Contradicted**: 701 values, 700 exact transitions 0→2→4→6→0 | Baseline field behaves as a counter |
| Other fuel-related data | Candidate `0x551 B1`, Racelogic channel in mm³, factor 1, offset 0 | **Needs characterisation**: time/cycle basis, possible accumulation and physical interpretation on MR16DDT unresolved | No validated consumption in L/h or L/100 km |
| Average fuel consumption | `0x580 B4` | **Unvalidated**: 109→110→111; no synchronised cluster reference | Do not automatically interpret these as 10.9–11.1 L/100 km |
| Distance to empty | `0x54C B6–B7 ×100/338`; alternative `0x2DE B6–B7 /10` | **Unvalidated**: preset formula constant at 305; alternative contains 824 `0xFFFF` values, then raw 2,707→2,687→2,684 | `0xFFFF` may be a sentinel; do not display 6,553.5 km. Expected unit bit stays clear |
| Steering wheel | Current: `0x002 B0–B1 I16LE /10`, ° | **Extraction strongly supported** by VBOX rotational speed; zero, stops and display still to check | Former `B1–B2 I16BE` mixed angle and rotational speed |
| Indicators | `0x60D B1 bit5` left, `bit6` right | Racelogic source agrees, operation confirmed by owner | Neither bit active in the 720 frames of this capture; dashboard fix does not recalibrate mapping |
| Lights | `0x60D B0 bit1/bit2`, `B1 bit3` | External definition agrees with preset light fields | No annotated lighting change; distinguish command, switch and actual state |
| Doors | Bits of `0x60D B0` | **Validate each door separately**; left/right/driver convention unconfirmed | Driver/passenger labels depend on the vehicle; do not infer a door from an isolated B2 transition |
| Outside temperature | `0x540 B0−40` | **Not physically validated**: 718 constant values at 28 °C, no ambient reference | `01 46` not advertised; requires an independent reading |
| Odometer | `0x5C5 B1–B3 BE` | **Candidate**: 719 constant values at 246,945, no associated cluster photograph/reading | A realistic value establishes neither unit nor offset |

The 35 speed references include **23 zeros**. For the 12 nonzero references,
the B4–B5 candidate's mean absolute error is **0.617 km/h**; the overall mean must
not be presented as high-speed calibration. `0x280 B4–B5 /100` is also
close, but this correlation alone does not distinguish indicated speed,
a filtered estimate and wheel speed.

## Parking-brake sequence

Candidate `0x5C5[0] & 0x04` starts at 1, becomes 0 at **t=35,194 ms**,
or **+4.211 s** after `LOG ON`, then returns to 1 at **t=100,294 ms**,
or **+69.311 s**. The rest of that byte remains `0x40`. This sequence
matches releasing the parking brake to leave and applying it after parking
as described by the owner. Repeating the action while stationary with
timestamps remains useful to exclude a consistently correlated state.

Changes in other bits that coincide across several IDs are not automatically
parking-brake signals: the brake pedal, clutch and reverse gear are distinct
states. The manoeuvre capture does not establish their exact event times.
The compared speeds are unsigned: they do not by themselves identify the
reverse-driving phases described by the owner.

## Public sources and manuals

### Racelogic / VBOX: Juke-specific file

The [public VBOX list](https://vboxautomotive.co.uk/en/can-database-documents)
provides a [REF definition file](https://www.racelogic.co.uk/_downloads/vbox/Vehicle_CAN_Files/New/Nissan-Juke%20%28F15%29%202011-2018.REF)
in addition to the [Juke F15 2011–2018 PDF](https://www.racelogic.co.uk/_downloads/vbox/Vehicles/Other/Docs/Nissan-Juke%20%28F15%29%202011-2018.pdf).
Retrieved on 14 October 2026: 2,650 bytes, SHA-256
`960a5d9b2e8323ad8a69c5226ac7179a5a8addafc112c47276f70a4f1023d7f4`.

The [text extraction supplied for the project](vehicle/nissan-juke-f15/Juke_F15_CAN_REF_decode.txt)
is archived unchanged in the documentation: 37 signals across 14 CAN
identifiers. The definitions originate from the REF on the VBOX / Racelogic
website; the [source note](vehicle/nissan-juke-f15/README.md) preserves its links.

The REF provides precise fields absent from the PDF alone. It corroborates
speed, coolant temperature and the parking-brake bit. Its engine-speed
definition only uses `0x180 B0×32`: the high-byte contribution to
`U16BE(B0,B1)/8`, quantised in 32 rpm steps. This approximation does not
replace the two bytes checked against OBD. Its `Fuel_Consumption` channel
specifies `0x551`, Intel start bit 8, length 8, unsigned (`UINT8`),
**factor 1, offset 0**, unit **mm³**: numerically the unmodified `B1` byte.
The coefficient is supplied. What remains unresolved is its temporal/cycle
meaning, possible accumulation and correspondence to fuel quantity on this
MR16DDT; observed counter behaviour does not validate instantaneous consumption.

The [RLVBCAN01 manual](https://www.racelogic.co.uk/_downloads/vbox/Manuals/Input_Modules/RLVBCAN01_Manual.pdf),
pages 10–11, explains the numbering: VBOX Motorola bits 63–56 belong to B0
and 55–48 to B1; Intel bits 7–0 belong to B0 and 15–8 to B1. Thus
`start=58,length=1,Motorola` identifies B0 bit2, and
`start=8,length=8,Intel` identifies B1. This is not the Vector DBC Motorola
start-bit convention; copying the positions directly would shift signals.

### Nissan manuals in the repository

The examined PDFs are **MY2012, October 2011 revision, North America**,
not an exact specification for the owner's 2011 vehicle.
[LAN](vehicle/nissan-juke-f15/LAN.pdf), pages LAN-27 through LAN-31,
distinguishes M/T/CVT and 2WD/AWD configurations and signal transmitters/
receivers. These matrices do not provide CAN IDs or offsets.

The cluster is central to the fuel gauge and range:
[MWI](vehicle/nissan-juke-f15/MWI.pdf), MWI-15/16, describes the sensor
input and cluster calculations. [EC](vehicle/nissan-juke-f15/EC.pdf), EC-29,
then describes the fuel-level signal sent over CAN from the cluster to the
ECM, without giving its ID or bytes. Fuel level therefore remains a lead
to investigate in the frames despite the missing `01 2F` reference in the logs.
[FL](vehicle/nissan-juke-f15/FL.pdf),
FL-31, specifies **50 L for 2WD and 45 L for AWD** in this edition. The
project's 45 L setting is therefore not validated for the owner's manual
Juke by this manual. The [corrected catalogue](vehicle/nissan-juke-f15/README.md)
details provenance, actual titles and useful sections.

### Work on other Nissan models

The studies of a [2013 manual diesel Qashqai J10](https://github.com/balrog-kun/nissan-qashqai-can-info/blob/b1ffbe27a60e8808539521cfd3a00959cbf791e9/README.md)
and [2010 Sentra](https://github.com/jackm/carhack/blob/0442dc7ac08b59080da8363d6e023c884d5995fd/nissan.md)
provide leads for duplicated engine-speed signals, speeds and cluster
states. The Qashqai study describes signed little-endian `0x002 B0–B1`
at 0.1 degree per unit and `0x551 B1` as an unresolved engine/fuel counter.
Its units and powertrain do not certify those of the Juke.

The [author's code for a 2010 370Z](https://github.com/Knio/carhack/blob/64d873755ee8ce894fb871372890a52662cb5621/carhack/processors/nissan_370z/__init__.py#L41)
also reads signed little-endian steering data. For the parking brake,
[its code](https://github.com/Knio/carhack/blob/64d873755ee8ce894fb871372890a52662cb5621/carhack/processors/nissan_370z/__init__.py#L124)
uses `B0 & 0x04`, consistent with its 68/64 examples; its Markdown table
contradictorily states `0x02`. Checking the code and actual bytes avoids
copying that error.

The [examined OpenDBC revision](https://github.com/commaai/opendbc/tree/156c2c08ee6e07b6c2d0f3700c81c3ba85f4f14e/opendbc/dbc)
contains Nissan Xterra 2011, Leaf 2018 and X-Trail 2017 definitions,
but no Juke. Even steering sign differs between Xterra (`+0.1`) and
modern Nissan definitions (`−0.1`): steering-wheel and camera sign
conventions must be kept distinct. These MIT-licensed files are leads,
not calibration for our variant. The [AiM list](https://www.aim-sportline.com/en/documentation-stock-ecu-connections.htm)
offers 350Z, 370Z, GT-R and Versa sheets, with no Juke entry in the list
examined; those sheets describe channels rather than a complete public
table of IDs and offsets.

Definitions from other engines, transmissions or generations are not
imported without comparison. CONSULT command research remains in the
[diagnostic findings](NISSAN_DIAGNOSTIC_RESEARCH.md): a periodic CAN
definition does not supply a manufacturer-specific diagnostic command.

## Fuel consumption: provisional conversion and remaining calibration

Candidate `0x551 B1` has 701 samples. Its 700 differences modulo 256 are
343 zeros, 330 increments of 1, 25 of 2 and two of 3, totalling **386
accumulated units**, with one `255→0` rollover between lines 43,094 and
43,231. This behaviour supports investigating an accumulated counter,
not directly displaying the byte as instantaneous consumption. The
REF defines a factor of 1 and offset of 0 in mm³; the counter's time/cycle
basis, possible resets and relationship to injection on this MR16DDT still
require verification. The supplier's unit label does not resolve them.

A [Clio III report](https://x0r.fr/blog/39#comment-396) identifies the same
counter and obtains 80 mm³/increment by comparison with the trip computer.
That is the current **provisional** `mm3PerTick` setting, not a result of
Juke calibration and not a replacement for the archived REF's factor 1.
Applied to the 386 increments over 71.7 s, it would imply **30.88 mL** and
**1.55 L/h** on average. Plausibility does not validate that scale. The
capture is shorter than the configured 100 m average-distance threshold,
so the production estimator does not expose a trip average for this replay.
Adjust the coefficient against an independent vehicle reference before
marking `calibration` as `calibrated`; install the matching firmware and
transfer the JSON separately before `CAN LOAD NissanJukeF15.json`.

ECU `0x7E8` advertises `01 10` (MAF), `01 44` (commanded equivalence
ratio), `01 03`, `01 04`, `01 06`, `01 07` and `01 11`. The current eight
readings do not request these PIDs. `01 5E` (direct fuel rate), `01 2F`
(level), `01 46` and `01 0B` are not advertised in the captured pages.
Both recent captures advertise identical bitmaps.

Car Scanner can record calculated consumption and its input parameters
using “Always calculate and save fuel consumption”; see the
[publisher's documentation](https://www.carscanner.info/records/).
The method actually used on this vehicle remains to be identified.
A CSV export containing those inputs would allow comparison between a
MAF-based estimate and the CAN candidate. Neither mm³ without a time basis
nor engine speed alone directly gives L/h or L/100 km.

## Analysis validation

All **34 Python tests** for the analysis tools pass. CSV export and reimport
preserve the IDs, DLCs and bytes of the three captures: **1,013**, **165**
and **92,754** frames respectively, with unchanged payloads. The 283
published matches and the JSON binary transitions were checked against
the original raw lines. These checks validate the tools and documentary
evidence. This historical analysis stage performed no new hardware test or
firmware change; the later implementation is described in the application
section above, with the applied-preset replay kept separately.

## Reproduction and next captures

Using the source file with the matching hash and the current repository
preset produces a **new comparison**, not the published baseline results:

```bash
python3 -B tools/analyze_can_log.py /path/to/canlog_20261014_210614_663_762622981087673198.txt \
  --output-dir .local-validation/juke-current-raw
python3 -B tools/compare_obd_can.py /path/to/canlog_20261014_210614_663_762622981087673198.txt \
  --preset data/NissanJukeF15.json --max-delay-ms 60 \
  --output-dir .local-validation/juke-current-comparison
python3 -B -m unittest discover -s tools -p 'test_*can*.py'
```

Matching selects the nearest CAN frame in the same session, within 60 ms.
Timestamps are assigned by ESP32 software; sensor latency and ECU filtering
are unknown. OBD references are roughly two seconds apart per parameter
and are not simultaneous.

To resolve remaining fields: separate, timestamped parking-brake, door,
light and indicator actions; centred steering followed by known angles
and directions; cluster readings for fuel level, odometer, range and units;
consumption parameter capture with an independent reference, and a separate
brake-pedal action to verify `0x180 B7 bit0`. Estimated
manoeuvre distances do not replace a measurement. Additional OBD readings
and further preset changes still require their own implementation and validation.
