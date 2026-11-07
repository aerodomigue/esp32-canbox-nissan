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

The contract describes the analysed consumers; it is not a claim that every
documented command is implemented by the current ESP32 firmware. In particular,
the existing `7D/09` coolant frame matches FYT but can make DUDU interpret its
checksum as oil temperature. The shared format and its required oil value
are explained in the PDFs; no F15 oil source or universal unavailable marker
has been validated.

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
