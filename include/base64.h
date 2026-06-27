#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Decode a base64-encoded string into binary.
 * @param input   Null-terminated base64 string
 * @param output  Output buffer
 * @param maxLen  Maximum bytes to write into output
 * @return Number of bytes decoded, or 0 on error
 */
size_t base64Decode(const char* input, uint8_t* output, size_t maxLen);

#endif // BASE64_H
