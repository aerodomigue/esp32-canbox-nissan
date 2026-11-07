# Raise Toyota/RAV4 UART documentation

[Français](README.fr.md)

This directory documents the UART protocol between the ESP32 CANBox and the
Android head unit. The Nissan F15 vehicle CAN mapping is documented separately
in the [F15 mapping analysis](../../JUKE_F15_CAN_MAPPING.md).

## DUDU7 encoding reference

| Document | Language | Contents |
| --- | --- | --- |
| [ENCODAGE_UART_DUDU7_RAV4.en.pdf](ENCODAGE_UART_DUDU7_RAV4.en.pdf) | English | Encoding contract, 67 pages |
| [ENCODAGE_UART_DUDU7_RAV4.fr.pdf](ENCODAGE_UART_DUDU7_RAV4.fr.pdf) | French | Encoding contract, 67 pages |

Revision: **7 November 2026**, based on the supplied DUDU7 OS archive
**26.0131D**, the FYT applications and the restored DUDU Base **1.004025**.
These documents describe **575 typed entries**: fields, settings, actions
and key mappings. This count is not a count of physically validated sensors.

The reference covers commands in both directions, payload offsets and bit
widths, signedness, byte order, units and conversions, packet lengths,
checksum calculation, receiver preconditions and complete frame examples.
`ESP32_TX` means CANBox to head unit; `ESP32_RX` means head unit to CANBox.
Internal MCU messages are identified separately from external CANBox UART.

### Validation and applicability

The reference distinguishes the independent FYT and DUDU decoders. The same
command can use different byte positions or availability rules in each.
Unknown physical meanings, units and unavailable-value markers remain explicit;
knowing a field's storage width does not validate its physical interpretation.

Validation includes **8,740 structural checks**, **271 assertions using the
original decoder instructions**, and **six encoder test methods**, including
all 4,096 signed 12-bit values. These are software checks with controlled
Android boundaries, not physical head-unit tests or vehicle calibration.

The PDFs are a dated analysis of the consumers, rather than a list of commands
implemented by the ESP32. Their warning about the former `7D/09` LEN3 packet
describes the firmware before the boot-trip update: DUDU could read its checksum
as an oil temperature. The updated F15 sender uses LEN4
`09 coolant+40 coolant+40 oil+40`, only while both sources are available and
fresh. This feeds FYT coolant at D2 and DUDU coolant/oil at D1/D3.

The F15 oil source `0x580 B4 - 40` is explicitly **provisional**, accepted for
comparison after coherent capture replays, not independently calibrated.
No oil unavailable byte is supported by the analysed DUDU decoder: zero and
255 mean -40 °C and 215 °C. Missing oil therefore suppresses the whole DUDU
temperature packet. The legacy coolant-only LEN3 packet is retained solely
for explicitly non-DUDU profiles.

The current F15 mapping also sends speed and boot average speed through
`7D/03` with **LEN6 and an explicit metric unit**, and sends the `0x21` elapsed
field in **minutes**. Instantaneous and boot-average consumption are `0.1
L/100 km` U16BE values in `0x22` and `0x23`; historical consumption slots remain
unavailable. Both `7D/04` trip slots explicitly mirror the software trip since
device boot; they are not the independent Nissan Trip A/B counters.

The DUDU temperature parser starts in Celsius. After replacing a box that sent
Fahrenheit without restarting the receiver, restart DUDU/the RAV4 profile
before checking temperatures: `7D/09` does not reset a retained Fahrenheit flag.
No artificial `0x28` climate packet is sent to force this flag. The unvalidated
F15 outside-temperature source is omitted; an absent sensor is not reported
as zero. See the [F15 boot-trip mapping guide](../../F15_DUDU_BOOT_TRIP_MAPPING.md)
for source availability, DUDU-specific ignore values, gaps and remaining limits.

See [RadioSend implementation notes](../../technical/RADIO_SEND.md) for the
current firmware behaviour and [F15 mapping evidence](../../JUKE_F15_CAN_MAPPING.md)
for the separate vehicle-side validation.

## Earlier protocol sources

- [RAV4 body/engine protocol, 2018–2020](RAV4_BODY_ENGINE_2018_2020.pdf),
  compiled and analysed by
  [Mikescotland](https://forum.dudu-auto.com/u/Mikescotland).
- [Raise Camry/RAV4 serial protocol V2.21.000, Chinese](<睿志诚软件_丰田CAMRY & RAV4 串口通讯协议  V2.21.000 (1).pdf>).

PDFs in this directory use **Git LFS**, as configured in the repository's
`.gitattributes`. Use `git lfs pull` after cloning to obtain the PDF contents
instead of their pointer files.
