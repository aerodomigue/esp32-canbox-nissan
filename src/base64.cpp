#include "base64.h"
#include <libb64/cdecode.h>
#include <string.h>

size_t base64Decode(const char* input, uint8_t* output, size_t maxLen) {
    base64_decodestate state;
    base64_init_decodestate(&state);
    size_t inputLen = strlen(input);
    int decoded = base64_decode_block(input, (int)inputLen, (char*)output, &state);
    return (decoded > 0 && (size_t)decoded <= maxLen) ? (size_t)decoded : 0;
}
