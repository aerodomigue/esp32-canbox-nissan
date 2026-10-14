# Nissan Juke F15 service manuals

[Français](README.fr.md)

## Edition and applicability

This archive contains **57 Nissan service-manual PDFs** (78.5 MiB of PDF content).
[FWD.pdf](FWD.pdf#page=2) identifies the manual as **2012 Nissan Juke**.
The relevant chapters checked for this project carry **Revision: 2011 October / 2012 JUKE**.
The foreword and identification information place this collection in the **North American** service context.
A publication date in 2011 does not make it a model-year 2011 manual.

The project vehicle is a **2011 phase-one F15, 1.6 DIG-T MR16DDT, manual transmission**.
These documents are useful evidence about that vehicle family, but equipment, market and model-year differences must be checked.
LAN-27 covers MR16DDT variants with M/T or CVT, 2WD or AWD, and optional Integrated Control System;
TCM, AWD and HVAC/display functions must not be assumed present on every vehicle.

## What these manuals establish

- **LAN-24–31:** CAN wiring principles, equipment variants, and signal transmit/receive matrices.
  Signal names and transmitting units do not supply numeric arbitration IDs, byte offsets or bit masks.
- **MWI-6, MWI-15–16:** the combination meter receives engine and vehicle-speed signals;
  fuel level and ambient-temperature sensors also have direct wired inputs.
  The meter calculates current/average fuel consumption and distance to empty.
- **MWI-20–22, BRC-38–40 and EXL diagnostic sections:** CONSULT monitor names identify observable values.
  They do not specify the diagnostic CAN address, service, PID/DID and response layout needed to read them independently.
- **FL-31:** this edition specifies approximately 50.0 L for 2WD and 45.0 L for AWD.
  This does not validate a 45 L calibration for the project's 2011 manual vehicle.

The archived manuals do **not validate the active preset's CAN IDs, RPM divisor, steering offset or fuel scaling**.
A physical CAN capture and observations on the target vehicle are still needed to confirm a mapping.
Nissan vehicle CAN and the [Raise Toyota UART protocol](../../protocols/raise-toyota-rav4/) used toward the head unit are separate protocols.
See [the CAN mapping analysis](../../JUKE_F15_CAN_MAPPING.md) for the project's current evidence and limits.

## File index

English section names follow the **Quick Reference Index in FWD.pdf**.
Filenames are preserved, including the lowercase `acc.pdf`.
In particular, MWI is the meter system, FL is fuel, EXL is exterior lighting,
GW is glass/window, and PCS is power control.

| File | Section |
| --- | --- |
| [FWD.pdf](FWD.pdf) | Foreword and quick reference index |
| [GI.pdf](GI.pdf) | General information |
| [acc.pdf](acc.pdf) | Accelerator control system |
| [EM.pdf](EM.pdf) | Engine mechanical |
| [EC.pdf](EC.pdf) | Engine control system |
| [LU.pdf](LU.pdf) | Engine lubrication system |
| [CO.pdf](CO.pdf) | Engine cooling system |
| [FL.pdf](FL.pdf) | Fuel system |
| [EX.pdf](EX.pdf) | Exhaust system |
| [STR.pdf](STR.pdf) | Starting system |
| [CL.pdf](CL.pdf) | Clutch |
| [TM.pdf](TM.pdf) | Transaxle and transmission |
| [DLN.pdf](DLN.pdf) | Driveline |
| [FAX.pdf](FAX.pdf) | Front axle |
| [RAX.pdf](RAX.pdf) | Rear axle |
| [FSU.pdf](FSU.pdf) | Front suspension |
| [RSU.pdf](RSU.pdf) | Rear suspension |
| [WT.pdf](WT.pdf) | Road wheels and tires |
| [BR.pdf](BR.pdf) | Brake system |
| [PB.pdf](PB.pdf) | Parking brake system |
| [BRC.pdf](BRC.pdf) | Brake control system |
| [ST.pdf](ST.pdf) | Steering system |
| [STC.pdf](STC.pdf) | Steering control system |
| [SB.pdf](SB.pdf) | Seat belt |
| [SR.pdf](SR.pdf) | SRS airbag |
| [SRC.pdf](SRC.pdf) | SRS airbag control system |
| [VTL.pdf](VTL.pdf) | Ventilation system |
| [HA.pdf](HA.pdf) | Heater and air conditioning system |
| [HAC.pdf](HAC.pdf) | Heater and air conditioning control system |
| [INT.pdf](INT.pdf) | Interior |
| [IP.pdf](IP.pdf) | Instrument panel |
| [SE.pdf](SE.pdf) | Seat |
| [DLK.pdf](DLK.pdf) | Door and lock |
| [SEC.pdf](SEC.pdf) | Security control system |
| [GW.pdf](GW.pdf) | Glass and window system |
| [PWC.pdf](PWC.pdf) | Power window control system |
| [RF.pdf](RF.pdf) | Roof |
| [EXT.pdf](EXT.pdf) | Exterior |
| [BRM.pdf](BRM.pdf) | Body repair |
| [MIR.pdf](MIR.pdf) | Mirrors |
| [EXL.pdf](EXL.pdf) | Exterior lighting system |
| [INL.pdf](INL.pdf) | Interior lighting system |
| [WW.pdf](WW.pdf) | Wiper and washer |
| [DEF.pdf](DEF.pdf) | Defogger |
| [HRN.pdf](HRN.pdf) | Horn |
| [PWO.pdf](PWO.pdf) | Power outlet |
| [BCS.pdf](BCS.pdf) | Body control system |
| [LAN.pdf](LAN.pdf) | LAN system, including CAN communication |
| [PCS.pdf](PCS.pdf) | Power control system |
| [CHG.pdf](CHG.pdf) | Charging system |
| [PG.pdf](PG.pdf) | Power supply, ground and circuit elements |
| [MWI.pdf](MWI.pdf) | Meter, warning lamp and indicator |
| [WCS.pdf](WCS.pdf) | Warning chime system |
| [AV.pdf](AV.pdf) | Audio, visual and navigation system |
| [CCS.pdf](CCS.pdf) | Cruise control system |
| [DMS.pdf](DMS.pdf) | Drive mode system |
| [MA.pdf](MA.pdf) | Maintenance |

## Source and Git LFS

The repository archives Nissan documents obtained through
[NICOclub Nissan service manuals](https://www.nicoclub.com/nissan-service-manuals).
Document titles, edition and printed section pages are the evidence;
the directory name alone does not establish applicability to all F15 model years.

The PDFs use **Git LFS**. A file containing `version https://git-lfs.github.com/spec/v1`
is a pointer, not a readable PDF. Fetch LFS content when needed:

```sh
git lfs pull --include="docs/vehicle/nissan-juke-f15/*.pdf"
```

The service documents retain their Nissan copyright notices.
