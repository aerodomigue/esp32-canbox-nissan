#pragma once
#include <stdint.h>

struct CanFrame {
    uint32_t identifier;
    uint8_t  data_length_code;
    uint8_t  data[8];
    bool     extd;
    bool     rtr;
};
