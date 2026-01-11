#include "../include/base64.h"

static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

size_t base64_encode_length(size_t inputLen) {
    return ((inputLen + 2) / 3) * 4 + 1; // +1 для нулевого терминатора
}

size_t base64_encode(const uint8_t* input, size_t inputLen, char* output, size_t outputLen) {
    if (input == nullptr || output == nullptr) {
        return 0;
    }
    
    size_t requiredLen = base64_encode_length(inputLen);
    if (outputLen < requiredLen) {
        return 0;
    }
    
    size_t outPos = 0;
    size_t i = 0;
    
    while (i < inputLen) {
        uint32_t octet_a = i < inputLen ? input[i++] : 0;
        uint32_t octet_b = i < inputLen ? input[i++] : 0;
        uint32_t octet_c = i < inputLen ? input[i++] : 0;
        
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        
        output[outPos++] = base64_chars[(triple >> 18) & 0x3F];
        output[outPos++] = base64_chars[(triple >> 12) & 0x3F];
        output[outPos++] = base64_chars[(triple >> 6) & 0x3F];
        output[outPos++] = base64_chars[triple & 0x3F];
    }
    
    // Добавляем padding
    size_t mod = inputLen % 3;
    if (mod == 1) {
        output[outPos - 1] = '=';
        output[outPos - 2] = '=';
    } else if (mod == 2) {
        output[outPos - 1] = '=';
    }
    
    output[outPos] = '\0';
    return outPos;
}


