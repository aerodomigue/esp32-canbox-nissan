# 🚗 Nissan Juke (F15) to Android Auto CAN Bridge (ESP32-C3)

> [!NOTE]
> **Available Languages:** [Français 🇫🇷](README.fr.md) | **English 🇬🇧**

> [!IMPORTANT]
> ### 🚧 STATUS: WORK IN PROGRESS (WIP) 🚧
> 
> 
> **Awaiting Hardware Validation**
> * **Code:** 100% implemented (CAN Capture, UART Transmission, Watchdog, Morse LED).
> * **Hardware:** Assembly in progress (ESP32-C3 + SN65HVD230).
> * **Critical Next Step:** Measure the termination resistance ( vs ) on the original Raise box before the first vehicle connection.
> * **Last Updated:** January 2026.
> 
> 

This project is an intelligent gateway designed to integrate telemetry data from a Nissan Juke (Platform B) into an Android head unit. The ESP32-C3 intercepts **Cabin CAN bus** frames and translates them in real-time for the head unit using the **Raise (RZC) protocol**.

---

## 🛠️ Hardware & Connections

### 1. Bill of Materials (BOM)

* **Microcontroller**: ESP32-C3 (RISC-V). Chosen for its native TWAI (CAN) controller.
* **CAN Transceiver**: SN65HVD230. Essential for the 3.3V physical interface.
* **Power Supply**: DC-DC Step-Down Regulator (12V -> 5V) type MP1584EN.
* **Status LED**: GPIO 8 (Diagnostic & Heartbeat).

### 2. Pinout (Wiring)

| Component | ESP32-C3 Pin | Destination | Note |
| --- | --- | --- | --- |
| **SN65HVD230** | `3.3V` / `GND` | Power Supply | **Do not use 5V!** |
|  | `GPIO 21` | CAN-TX Pin | Output to bus |
|  | `GPIO 20` | CAN-RX Pin | Input from bus |
| **Head Unit** | `GPIO 1` (TX) | RX Wire (Radio harness) | UART 38400 baud |
| **Status LED** | `GPIO 8` | Internal LED | Morse Diagnostic & Heartbeat |

---

## ⚙️ Software Logic & Safeties

The system is designed to be 100% autonomous and resistant to vehicle electrical interference:

1. **[Capture (docs/CAN_CAPTURE.md)](docs/CAN_CAPTURE.md)**: Analyzes Nissan frames (500kbps) and updates global variables (Speed, RPM, Doors, etc.).
2. **[Emission (docs/RADIO_SEND.md)](docs/RADIO_SEND.md)**: Formats and sends data to the head unit at two distinct frequencies (Fast Stream 50ms / Slow Stream 800ms).
3. **Hardware Watchdog**: If the program freezes for more than 5s, the ESP32 automatically reboots.
4. **CAN Watchdog**: If no CAN data is received for 30s while the engine is running (voltage > 11V), the system forces a reboot.

---

## 🚦 LED Error Codes (Morse)

The LED (GPIO 8) allows for quick diagnostics without connecting a PC:

* **Off**: Power issue (VCC/GND).
* **Solid On**: ESP32 powered, but no data received on the CAN bus.
* **Fast Blinking (50ms)**: CAN controller initialization error (Check GPIO 20/21).
* **Brief Flash**: Power steering frame (0x002) received. Everything is functional.

---

## ⚠️ Precautions: Termination Resistor

The CAN bus requires precise impedance matching. The SN65HVD230 module often has a built-in **R120** resistor. **Measure your original Raise CAN box (between CAN-H and CAN-L pins):**

### CASE A: Raise box measures ~120 Ohms

> **Action: KEEP THE RESISTOR.** The original box acted as a line termination. The ESP32 must take over this role.

### CASE B: Raise box measures Infinity (OL / 1)

> **Action: REMOVE THE RESISTOR (Desolder R120).** The head unit is connected in parallel in the middle of the bus. Leaving the resistor would drop the bus impedance to 40 ohms and disrupt other ECUs.

---

## 📚 Sources & References

### 🚗 Nissan CAN & Manuals
* [NICOclub / Nissan Service Manuals](https://www.nicoclub.com/nissan-service-manuals)
* [Comma.ai / OpenDBC](https://github.com/commaai/opendbc/tree/master)
* [jackm / Carhack Nissan](https://github.com/jackm/carhack/blob/master/nissan.md)
* [balrog-kun / Nissan Qashqai CAN info](https://github.com/balrog-kun/nissan-qashqai-can-info)

### 📻 Radio Protocols (Raise/RZC)
* [smartgauges / canbox](https://github.com/smartgauges/canbox)
* [cxsichen / Raise Protocol (睿智诚)](https://github.com/cxsichen/helllo-world/tree/master/%E5%8D%8F%E8%AE%AE/%E7%9D%BF%E5%BF%97%E8%AF%9A)
* [DUDU-AUTO Forum / Qashqai 2011 CANbus](https://forum.dudu-auto.com/d/1786-nissan-qashqai-2011-canbus/6)