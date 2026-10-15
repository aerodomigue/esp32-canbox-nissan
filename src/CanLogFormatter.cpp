#include "CanLogFormatter.h"
#include <stdio.h>

bool formatCanFrameLog(const CanFrame& frame, uint32_t receivedAt, char* output, size_t capacity) {
    if (!output || capacity < 160) return false;
    const bool diagnostic = !frame.extd && frame.identifier >= 0x7DF && frame.identifier <= 0x7EF;
    int used = snprintf(output, capacity, "%s 0x%03lX [%u]:",
                        diagnostic ? "ODB RAW_RX" : "RX",
                        static_cast<unsigned long>(frame.identifier), frame.data_length_code);
    const uint8_t bytes = frame.data_length_code > 8 ? 8 : frame.data_length_code;
    if (!frame.rtr) {
        for (uint8_t i = 0; i < bytes; ++i) {
            used += snprintf(output + used, capacity - used, " %02X", frame.data[i]);
        }
    }
    snprintf(output + used, capacity - used, " | t_ms=%lu ext=%u rtr=%u",
             static_cast<unsigned long>(receivedAt), frame.extd ? 1U : 0U, frame.rtr ? 1U : 0U);
    return true;
}
