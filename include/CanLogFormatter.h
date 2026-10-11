#pragma once

#include <ESP32-TWAI-CAN.hpp>
#include <stddef.h>
#include <stdint.h>

// Preserve payload, DLC and flags. The timestamp is software dequeue time.
// A malformed DLC is displayed as declared, but never reads more than 8 bytes.
bool formatCanFrameLog(const CanFrame& frame, uint32_t receivedAt, char* output, size_t capacity);
