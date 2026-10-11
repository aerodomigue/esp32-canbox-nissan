# Local development and validation

Run these commands from the repository root. Python 3.10+ with `venv`, Git, and a
native C++ compiler (`g++`) are required. Dependencies and PlatformIO packages
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
```

The suites use host mocks and the production CAN decoder, radio encoder, and
serial command implementation. The CAN suites load `data/NissanJukeF15.json`
directly. The serial environment generates `include/version.h` using the same
script as the firmware build, so it works on a fresh clone.

CI runs all three environments. Native test success checks software behavior;
it does not establish that a mapping matches the dashboard of a particular car.

The replay suite uses an authentic historical excerpt by default. To replay
the complete archived capture through the production decoder, use the commands
in [the PR #10 validation report](PR10_VALIDATION.fr.md#reproduire-la-relecture-complète).
The optional `CAN_LOG_PATH` input expects `RX ID: 0x180 | DLC: 8 | Data: ...`
records and reports parser errors separately from incomplete configured frames.

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
