# F15 CAN to DUDU RAV4: units and boot-trip data

> **Languages:** **English** | [Français](F15_DUDU_BOOT_TRIP_MAPPING.fr.md)

Revision: **7 November 2026**. This guide describes the current F15 implementation
and supersedes older preset examples for speed, range, temperatures and trip
lifetime. Older capture reports retain their historical evidence and limitations.
The vehicle is the owner's **2011 Nissan Juke F15, MR16DDT 1.6 DIG-T, manual**,
with CAN connected to OBD pins 6/14. The ESP32 translates Nissan vehicle CAN to
the head unit's **Toyota RAV4 UART** profile; no Toyota vehicle CAN frame is sent.

## Source values and confidence

Byte indexes start at zero. `BE`/`LE` apply to the extracted integer, before its
conversion into physical units.

| Value | F15 source | Physical conversion | Qualification |
| --- | --- | --- | --- |
| Speed | `0x280 B4..B5`, U16BE | raw / 100 km/h | Long-trip distance agrees with odometer increments; replaces `0x284` |
| Odometer | `0x5C5 B1..B3`, U24BE | raw km | Controlled capture matches the photographed 247,498 km |
| Engine speed | `0x180 B0..B1`, U16BE | raw / 8 rpm | OBD-correlated |
| Coolant | `0x551 B0`, U8 | raw − 40 °C | Owner-confirmed and OBD-correlated |
| Engine oil | `0x580 B4`, U8 | raw − 40 °C | **Provisional**, explicitly enabled by the owner; thermal behaviour is plausible, physical identity not independently confirmed |
| Distance to empty | `0x2DE B6..B7`, U16BE | raw / 10 km | Strongly corroborated by the controlled capture and photographed range |
| Fuel increments | `0x551 B1`, U8 modulo 256 | delta × 80 mm³ | **Provisional volume calibration**; dynamic response agrees with load and deceleration |
| Factory historical average | `0x2DE B3..B4`, U16BE | 16000 / raw L/100 km, raw > 0 | Corroborated by the photographed 9.9; documentary reference only, **not the boot-trip average sent to DUDU** |

Raw `0xFFFF` is excluded for speed and rpm, and raw `0xFF` for coolant. The
Nissan MWI data-monitor documentation identifies their converted values
655.35 km/h, 8191.875 rpm and 215 °C as fault indications. DTE raw `0xFFFF` is
excluded as unavailable. These exclusions apply **before** conversion.
`estimated: true` marks the oil source as provisional; it does not improve
its accuracy. No equivalent oil invalid marker has been established here.

The incorrect `0x540 B0−40` outside-temperature mapping is removed. No replacement
is invented from the single photographed 14 °C point. There is no quantitative
fuel-level source, so the firmware does not send a fabricated fuel quantity.
Existing working door, boot, indicator, lighting and steering mappings are retained.
Reverse is a dedicated wire to the head unit on this installation, rather than a
CAN signal required for the camera.

## Consumption and trip lifetime

The trip belongs to **one device boot**, lives only in RAM, and is not stored
in NVS. `LOG ON`/`LOG OFF` do not start or reset it. A reload with the same
estimator calibration and source configuration preserves its totals, counter
baseline and instantaneous window. A reboot starts a new trip. A changed source
or calibration after collection starts must not silently create a new average
labelled as covering the whole boot.

The estimator uses paired, fresh fuel-counter and fractional-speed samples:

```text
counter delta = (current − previous) modulo 256
fuel volume in mm³ = sum(counter delta) × mm³ per increment
trip distance in m = integrated speed in km/h × elapsed time in ms / 3600
average L/100 km = fuel volume in mm³ / (10 × distance in m)
UART consumption in 0.1 L/100 km = fuel volume in mm³ / distance in m
```

Idle fuel is included in average consumption. Average output requires at least
**100 m**, fresh sources and a complete trip. Instantaneous output uses the
configured **1 s** window. Below the configured **2 km/h**, its fresh display
value is zero; this convention is not a claim of zero fuel flow. The approximately
**0.97 L/h** observed at idle is an independent theoretical reference, not the
unit sent to the head unit and not a constant used while driving.

A source gap beyond **500 ms**, invalid input, implausible counter progression,
or a material estimator/source change makes the trip incomplete for the rest
of that boot. Already collected paired totals remain in RAM. Instantaneous
consumption can recover after fresh samples; the full-boot average is not
silently restarted or represented as complete. Counter resets that resemble a
normal modulo rollover cannot always be detected. Nothing reconstructs fuel
consumed before the first usable sample pair.

On the 17 October capture, the replay gives **14.942555 km**, **1.20552 L** with
80 mm³/increment, and therefore approximately **8.07 L/100 km**, encoded as
**8.1 L/100 km**. This supports the behaviour, not an independently calibrated
fuel volume. Engine rpm alone does not determine consumption: deceleration can
keep rpm high while injection and counter increments fall to zero.

Average speed uses distance divided by **elapsed boot time, including stops**.
Elapsed trip time is device uptime, converted from seconds to **whole minutes**
for UART. Virtual Trip A and Trip B both mirror the same integrated boot distance;
they are **not the Nissan Trip A/B counters** and do not imply two independent
reset controls. A missing distance interval invalidates these virtual totals
rather than hiding an incomplete trip behind an apparently exact number.

## Receiver selection and UART contract

The preset root explicitly selects:

```json
"radioReceiver": "DUDU"
```

Omission preserves the generic **FYT** behaviour. This selector is needed because
DUDU and FYT do not give every unavailable value the same meaning. Keep the
head unit on its Toyota RAV4 CAN profile.

UART framing remains `2E CMD LEN PAYLOAD CHECKSUM` at **38400 baud, 8N1**.
`CHECKSUM = (CMD + LEN + sum(PAYLOAD)) XOR FF`, using the low eight bits.
Offsets below are payload indexes; the `7D` subcommand itself is `D0` and counts
in `LEN`. Numbers are unsigned unless explicitly stated.

| Command | Length | Payload and units |
| --- | --- | --- |
| `7D / 03` | 6 | `D0=03`; `D1..2` speed U16LE in **0.01 km/h**; `D3..4` boot average speed U16LE in **0.01 km/h**; `D5=00` metric |
| `7D / 04` | 12 | `D0=04`; `D1..3` odometer U24LE in **km**; `D4..5=F2 08` retained legacy bytes with unverified meaning; `D6..8` virtual Trip A U24LE in **0.1 km**; `D9..11` virtual Trip B, same value/unit |
| `7D / 0A` | 3 | `D0=0A`; `D1..2` U16LE = **rpm × 4** |
| `21` | 7 | `D0..1` average speed U16BE in **0.1 km/h**; `D2..3` elapsed U16BE in **minutes**; `D4..5` DTE U16BE in **km**; `D6=02` metric |
| `22` | 3 | `D0=02`, L/100 km; `D1..2` current consumption U16BE in **0.1 L/100 km** |
| `23` | 13 | `D0=02`; `D1..2` boot average U16BE in **0.1 L/100 km**; `D3..12` five unavailable history slots, each `FFFF` |
| `7D / 09` | 4 | `[09, coolant+40, coolant+40, oil+40]`; each temperature is **U8, offset −40 °C** |
| `29` | 2 | Calibrated steering units, signed 12-bit LE, limited to **−500..+500**; these are guideline units, not raw Nissan tenths of a degree |

The temperature packet duplicates coolant because FYT consumes `D2`, while DUDU
uses `D1` for coolant and `D3` for oil. A three-byte packet is too short for the
DUDU oil field and can expose its checksum as a false oil value. The four-byte
packet is sent only when **both configured sources are fresh** (less than 2 s)
and representable. Oil is never replaced by a copy of coolant. A fresh DUDU decoder defaults to Celsius, but `7D/09` does not reset a previously
selected Fahrenheit flag. If an earlier box left that flag active, restart the
DUDU decoder/head unit or reinitialize its RAV4 profile before validating the
display. No unvalidated `0x28` climate packet is emitted merely to force this
flag: it would also update unrelated HVAC fields. Exterior temperature itself
is not sent without a valid source.

For DUDU, `FFFF` in the average-speed and DTE fields means **ignore that field**,
not clear its previous display. This allows fresh speed or range to continue
while the other field is unavailable. Generic FYT packets are withheld when a
required companion field is unknown. An invalid current consumption is withheld:
`FFFF` would be displayed as 6553.5 by DUDU's instantaneous consumer, and the
average consumers do not share a universal clearing sentinel. History slots
remain unavailable. Elapsed minutes saturate at 65534 instead of wrapping.
A withheld field can leave an older value visible on the head unit.
The audited DUDU Toyota `m3` consumer ignores the elapsed-time bytes in `0x21`;
encoding minutes correctly is not proof that this DUDU screen displays duration.
Temperature-card visibility also depends on the selected vehicle-information layout.
In the audited build, standard RAV4 profiles `393328` (config16) and `786544`
(config15) allow the oil/coolant cards; RAV4 AMP `458864` (config18) excludes
those cards. An excluded card cannot be enabled merely by sending its UART value.

The boot odometer/trip packet requires fresh odometer input and a complete boot
trip. DUDU may truncate its Trip A/B text to whole kilometres despite the wire's
0.1 km resolution. Internal `SYS DATA` validity and provisional flags are not
encoded as extra bits in the Toyota protocol.

## Fields deliberately not fabricated

- **Outside temperature and tank quantity:** no validated F15 source.
- **Handbrake UART:** its CAN decoding and `SYS DATA` remain available. `7D/06`
  also updates an unverified seat-belt field, so this change does not send it.
- **External battery-voltage card:** the audited DUDU card uses an internal MCU
  measurement; a working Nissan voltage decoder is not proof of an external
  Toyota UART command capable of updating that card.
- **Unknown warnings, belt state and historical trips:** no synthetic values are
  presented as measured vehicle data.

## Bounded Nissan diagnostic experiment

Diagnostics remain capture/reference data and do not overwrite the preset or
radio values. Standard service `01` polls only advertised PIDs. For the exact
F15 profile in real `LOG ON`, the additional engine path is physical TX `7E0`,
RX `7E8`, with one request pending, 500 ms minimum attempt spacing and a 250 ms
response window. Each of these short groups is attempted once per `LOG ON`,
with at least 10 s between group starts:

```text
10 C0 → 22 11 01 → 22 11 1F → 10 81
10 C0 → 22 11 14 → 22 11 83 → 10 81
10 C0 → 22 13 04             → 10 81
```

Only matched positive `50 C0` authorizes that group's reads. `1101` is a coolant
control candidate and `111F` an oil candidate, both U8−50 °C after `62 + DID`.
The other three DIDs, `1114` (fuel), `1183` (gear) and `1304` (clutch), remain raw. A control refusal does not prove that the
oil DID is absent. An opening refusal/timeout or unconfirmed close stops the
remaining groups. Delayed processing closes the session instead of extending
its read phase. `LOG OFF`, profile change, mock or OTA cancellation attempts a
bounded best-effort close; if spacing or transmission prevents confirmation,
`ODB SESSION_STATE` reports an unknown state. No keepalive, DID/ECU range scan,
security access, actuator, reset or fault-clear command is added.

The existing capture has four `7F 22 80` refusals **without an opening session**.
Car Scanner's original Nissan CAN11 implementation explicitly declares `10C0`
before and `1081` after its manufacturer readings. This justifies an experiment;
it does **not** establish session acceptance or DID support on the owner's ECU.
NRC `80` must not be assigned an invented generic UDS meaning. Raw `RX` and
`ODB RAW_RX` capture continues without blocking while replies are pending.
New `ODB SESSION_RX` / `ODB SESSION_STATE` lines complement existing `ODB TX`,
`NEGATIVE`, `TIMEOUT` and `PROBE_RX`; USB line framing is unchanged.

## Evidence and installation

- [F15 mapping evidence](JUKE_F15_CAN_MAPPING.md) and [archived VBOX extraction](vehicle/nissan-juke-f15/Juke_F15_CAN_REF_decode.txt): broadcast-source history and provenance.
- [DUDU7/FYT UART reference, English PDF](protocols/raise-toyota-rav4/ENCODAGE_UART_DUDU7_RAV4.en.pdf): supplied DUDU OS **26.0131D**, FYT `j0/c3` and restored DUDU Base **1.004025** Toyota `m3` consumers. Original decoder instructions determine widths/units; software execution does not prove an untested head-unit UI configuration.
- [MY2011 Juke MR16DDT EOT diagnosis](https://charm.li/Nissan-Datsun/2011/Juke%20FWD%20L4-1.6L%20Turbo%20%28MR16DDT%29/Repair%20and%20Diagnosis/A%20L%20L%20%20Diagnostic%20Trouble%20Codes%20%28%20DTC%20%29/Testing%20and%20Inspection/P%20Code%20Charts/P0197/) and [CONSULT ECM monitors](https://charm.li/Nissan-Datsun/2011/Juke%20FWD%20L4-1.6L%20Turbo%20%28MR16DDT%29/Repair%20and%20Diagnosis/Powertrain%20Management/Computers%20and%20Control%20Systems/Testing%20and%20Inspection/Scan%20Tool%20Testing%20and%20Procedures/CONSULT-III%20Function%20%28ECM%29/): factory content on a public mirror, confirming an oil sensor and °C monitor in this model family, not the identity of `580` or DID `111F` on this VIN.
- [Car Scanner official beta source](https://www.carscanner.info/beta/), analysed APK **2.2.42**: original CIL Nissan session declarations and U8−50 formulas. ECM fuel-level voltage and cluster fuel quantity are distinct; conflicting generic `1114` scales are not resolved by an app label.

Flash the application OTA **and transfer the updated `NissanJukeF15.json`
separately**, then `CAN LOAD NissanJukeF15.json`. OTA alone does not replace
LittleFS presets. Keep the complete `RX`/`ODB` log when checking the new references.
Unit/serialization tests and recorded-trip replay verify software behaviour;
provisional oil identity and absolute fuel volume still need independent vehicle
references. Other vehicle profiles keep FYT as their default receiver.

## Software validation

[Validation record](validation/F15_DUDU_BOOT_TRIP_20261107.json): 296 native tests,
36 Python tests, plus the full 1,235,533-frame capture replay (no parser or decoder
rejection). Independent checks executed 1,829 assertions on the original receiver
instructions and 208 assertions from actual production C++ UART bytes through
that receiver. These validate software units and framing; they do not confirm the
physical identity of the provisional oil source or calibrate fuel volume.
