#pragma once

/*

VINT:
    0XXXXXXX = N(7) = 0-127
    10XXXXXX X8 = N(14) + 128 = 128 ~ 32,895
    110XXXXX X8 X8 = N(21) + 32,896 = 32,896 ~ 2,130,047
    1110XXXX X8 X8 X8 = N(28) + 2,130,048 = 2,130,048 ~ 270,565,503
    11110XXX X8 X8 X8 X8 = N(35) + 270,565,504 = 270,565,504 ~ 34,630,303,871
    111110XX X8 X8 X8 X8 X8 = N(42) + 34,630,303,872 = 34,630,303,872 ~ 4,432,676,814,975
    1111110X X8 X8 X8 X8 X8 X8 = N(49) + 4,432,676,814,976 = 4,432,676,814,976 ~ 567,382,630,236,287
    11111110 X8 X8 X8 X8 X8 X8 X8 = N(56) + 567,382,630,236,288 = 567,382,630,236,288 ~ 72,624,976,668,164,223
    11111111 X8 X8 X8 X8 X8 X8 X8 X8 = N(64) + 72,624,976,668,164,224 = 72,624,976,668,164,224 ~ 18,519,369,050,377,715,840 > 2^64
    
*/

#include <stdint.h>
#include <intrin.h>

static const uint64_t VINT_OFFSET9 = 72624976668164224ul;
static const uint64_t VINT_OFFSETS[8] = {
    567382630236288ul, 4432676814976ul, 34630303872ul,
    270565504ul, 2130048ul, 32896ul, 128ul, 0ul
};

static inline unsigned int sizeVInt(const char* data) {
    uint8_t c = *data;
    unsigned long bit;
    unsigned char found_bits = _BitScanReverse(&bit, ~c);
    if (!found_bits) return 9;
    else return 8 - bit;
}

static inline uint64_t readVInt(const char*& data) {
    uint8_t c = *data;
    unsigned long bit;
    unsigned char found_bits = _BitScanReverse(&bit, ~c);
    data++;
    if (!found_bits) {
        uint64_t result = *(uint64_t*) data;
        data += 8;
        return _byteswap_uint64(result) + VINT_OFFSET9;
    }

    uint64_t result = c & ((1 << bit) - 1);
    uint64_t offset = VINT_OFFSETS[bit];
    while (bit < 7) {
        result <<= 8;
        result |= *data;
        data++;
        bit++;
    }
    return result + offset;
}

static inline unsigned int sizeVInt(uint64_t data) {
    if (data < VINT_OFFSETS[6])
        return 1;
    else if (data < VINT_OFFSETS[5])
        return 2;
    else if (data < VINT_OFFSETS[4])
        return 3;
    else if (data < VINT_OFFSETS[3])
        return 4;
    else if (data < VINT_OFFSETS[2])
        return 5;
    else if (data < VINT_OFFSETS[1])
        return 6;
    else if (data < VINT_OFFSETS[0])
        return 7;
    else if (data < VINT_OFFSET9)
        return 8;
    else
        return 9;
}

static inline void writeVInt(char*& result, uint64_t data) {
    for (int i = 6; i >= 0; i--) {
        if (data < VINT_OFFSETS[i]) {
            data -= VINT_OFFSETS[i + 1];
            uint8_t firstByte = ~(2 << i) & (~1 << i);
            int byteCount = 6 - i;
            firstByte |= data >> (byteCount * 8);
            *result = firstByte;
            result++;
            for (int j = byteCount - 1; j >= 0; j--) {
                *result = (data >> (j * 8)) & 0xFF;
                result++;
            }
            return;
        }
    }
    if (data > VINT_OFFSET9) {
        data -= VINT_OFFSET9;
        *result = 0xFF;
        result++;
        *(uint64_t*)result = _byteswap_uint64(data);
        result += 8;
    }
    else {
        data -= VINT_OFFSETS[0];
        *result = 0xFE;
        result++;
        for (int j = 6; j >= 0; j--) {
            *result = (data >> (j * 8)) & 0xFF;
            result++;
        }
    }
}