# External CANBox Passthrough

Allow external CAN boxes (parking sensors, blind spot monitors, etc.) to communicate with the Android head unit through our ESP32 device.

---

## Overview

Many aftermarket CAN boxes use the same **Raise/Toyota protocol** (0x2E frames @ 38400 baud). Instead of connecting them directly to the head unit (which would conflict with our device), we act as a **smart multiplexer**.

**How it works:**
- ESP32 sends Nissan vehicle data (translated to Toyota protocol)
- External device sends its own data (parking sensors, etc.)
- Both streams are **merged** before reaching the head unit
- Head unit commands: **FILTER** mode (consume or forward) or **DUPLICATE** mode (process and forward)

---

## Architecture

```
┌────────────────┐                                         ┌────────────────┐
│    External    │                                         │    Android     │
│    CANBox      │         ┌───────────────────┐           │    Head Unit   │
│                │         │     ESP32-C3      │           │                │
│  (parking      │   TX    │                   │   TX      │                │
│   sensors,     │────────▶│  ┌─────────────┐  │──────────▶│                │
│   BSD, etc.)   │  GPIO2  │  │    MERGE    │  │  GPIO5    │                │
│                │         │  │             │  │           │                │
│                │   RX    │  │  ESP32 data │  │   RX      │                │
│                │◀────────│  │      +      │  │◀──────────│                │
│                │  GPIO3  │  │  Ext. data  │  │  GPIO6    │                │
└────────────────┘         │  └─────────────┘  │           └────────────────┘
                           │                   │
                           │  ┌─────────────┐  │
                           │  │   FILTER    │  │
                           │  │             │  │
                           │  │ known → use │  │
                           │  │ unknown →   │  │
                           │  │   forward   │  │
                           │  └─────────────┘  │
                           └───────────────────┘
```

---

## Data Flow

### TX → Head Unit

| Source | Behavior |
|--------|----------|
| ESP32 | Send our frames (RPM, speed, steering, doors...) |
| External | Buffer complete frames, insert between ours |

> **Merge Logic:** After each transmission, check if external device has a complete frame ready. If yes, send it. No byte interleaving.

### RX ← Head Unit

Two modes available (configurable via app):

**FILTER mode** (default)

| Command | Action |
|---------|--------|
| `0x81` Start/End | Consume only |
| `0x90` Request | Forward only |
| `0xA0` Amp control | Forward only |
| `0xC0-C4` Source/Media | Forward only |
| `0xC6` Radar volume | Forward only |
| Unknown | Forward only |

**DUPLICATE mode**

| Command | Action |
|---------|--------|
| All | Process AND forward |

> In DUPLICATE mode, ESP32 handles commands it knows AND forwards everything to the external device. Useful when external device needs to see all traffic.

---

## Protocol

All devices use the same frame format:

```
┌────────┬─────────┬────────┬──────────────┬──────────┐
│  0x2E  │ Command │ Length │  Payload[n]  │ Checksum │
│ header │   1B    │   1B   │   Length B   │    1B    │
└────────┴─────────┴────────┴──────────────┴──────────┘

Checksum = (Command + Length + Σ Payload) XOR 0xFF
```

---

## Hardware

### Pinout

| Function | GPIO | Dir |
|----------|------|-----|
| External RX (from device) | 2 | IN |
| External TX (to device) | 3 | OUT |
| Radio TX (to head unit) | 5 | OUT |
| Radio RX (from head unit) | 6 | IN |

### Wiring

```
External CANBox              ESP32-C3
┌──────────────┐            ┌──────────────┐
│          TX ─┼────────────┼─▶ GPIO 2     │
│          RX ◀┼────────────┼── GPIO 3     │
│         GND ─┼────────────┼── GND        │
└──────────────┘            └──────────────┘
```

> ⚠️ External device must use **3.3V logic**. If 5V, add a voltage divider on RX.

---

## Implementation

### Frame Buffering

```cpp
uint8_t extBuffer[64];       // Frame being received
uint8_t extBufferLen = 0;
uint8_t extExpectedLen = 0;

uint8_t extReadyFrame[64];   // Complete frame ready to send
uint8_t extReadyLen = 0;
bool extFrameReady = false;
```

### Parser State Machine

1. Wait for `0x2E` header
2. Read command byte
3. Read length byte
4. Read `length` payload bytes
5. Read checksum byte
6. Validate → copy to ready buffer

### Merge Timing

```cpp
void processRadioUpdates() {
    sendSteeringAngleMessage(...);
    sendExternalFrameIfReady();    // ← insert here

    sendDoorCommand(...);
    sendExternalFrameIfReady();    // ← and here

    sendRpmMessage(...);
    sendExternalFrameIfReady();    // ← etc.
}
```

---

## Commands

| Command | Description |
|---------|-------------|
| `PT ON` | Enable passthrough |
| `PT OFF` | Disable passthrough |
| `PT STATUS` | Show status and stats |
| `PT MODE FILTER` | Set FILTER mode (default) |
| `PT MODE DUPLICATE` | Set DUPLICATE mode |

---

## Limitations

- Uses **SoftwareSerial** (ESP32-C3 has only 2 hardware UARTs)
- Max baud: **38400** (same as head unit protocol)
- **Single** external device (no daisy-chain)
- External device must not flood faster than merge rate
