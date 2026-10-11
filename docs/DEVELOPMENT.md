# Local development and validation

Run these commands from the repository root. Python 3.10+ with `venv`, Git, and a
native C++ compiler (`g++`) are required. The project pins PlatformIO's core
directory to `.pio-core` in `platformio.ini`. Dependencies and PlatformIO packages
stay inside the project; no system Python packages are modified.

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements-dev.txt
export PLATFORMIO_CORE_DIR="$PWD/.pio-core"
mkdir -p .local-validation
```

## Tests

```bash
.venv/bin/pio test -e native -e native_radio -e native_serial
.venv/bin/python -B -m unittest discover -s tools -p 'test_*can*.py' -v
```

The suites use host mocks and the production CAN decoder, radio encoder, and
serial command implementation. The CAN suites load `data/NissanJukeF15.json`
directly. The serial environment generates `include/version.h` using the same
script as the firmware build, so it works on a fresh clone.

CI runs all three environments, including the OBD calibration and raw formatter
tests, and the Python capture analysis/comparison tests.
Native test success checks software behavior;
it does not establish that a mapping matches the dashboard of a particular car.

The replay suite uses an authentic historical excerpt by default. To replay
the complete archived capture through the production decoder, use the commands
in [the PR #10 validation report](PR10_VALIDATION.fr.md#reproduire-la-relecture-complète).
The optional `CAN_LOG_PATH` input expects `RX ID: 0x180 | DLC: 8 | Data: ...`
records and reports parser errors separately from incomplete configured frames.

For byte statistics, BE/LE candidates, counter patterns and canonical CSV
export, run the standard-library-only analysis tool:

```bash
.venv/bin/python -B tools/analyze_can_log.py path/to/capture.txt \
  --output-dir .local-validation/deep-analysis
```

It accepts historical and current RX serial records, and CSV columns
`timestamp_ms,RX,can_id,dlc,data`. It writes `analysis.json` and `frames.csv`,
rejects malformed records, preserves missing timestamps, and reports numeric
hypotheses without asserting physical calibration. The historical export has
a logger provenance defect; see [the vehicle analysis](JUKE_2011_CAN_ANALYSIS.fr.md).

For a new `LOG ON` capture containing `ODB RX` references, run:

```bash
.venv/bin/python -B tools/compare_obd_can.py path/to/capture.log \
  --preset data/NissanJukeF15.json --max-delay-ms 200
```

It keeps session/ECU/PID/unit streams separate and writes `comparison.json` plus
`matches.csv`. No mapping is changed automatically. See
[ODB calibration logging](OBD_CALIBRATION.fr.md) for the protocol and limitations.

## ESP32-C3 build

```bash
.venv/bin/pio run -e esp32-c3-devkitm-1
.venv/bin/pio run -e esp32-c3-devkitm-1 -t buildfs
```

The application firmware and vehicle configuration filesystem are separate:

- `.pio/build/esp32-c3-devkitm-1/firmware.ota`: application OTA image.
- `.pio/build/esp32-c3-devkitm-1/all-firmware.bin`: merged application image.
- `.pio/build/esp32-c3-devkitm-1/littlefs.bin`: vehicle presets from `data/`.

When testing revised mappings on hardware, upload the new preset through
CANBox Manager or update LittleFS, then reload `NissanJukeF15.json`. Updating
only the application OTA image leaves the installed JSON mapping unchanged.

The firmware platform uses the repository's existing `stable` release URL;
its resolved version can change. Record the PlatformIO, Arduino framework,
and compiler versions alongside validation results.
