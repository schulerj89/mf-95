#ifndef MF_TYPES_H
#define MF_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MF_SCREEN_WIDTH   256
#define MF_SCREEN_HEIGHT  224
#define MF_SCREEN_PIXELS  (MF_SCREEN_WIDTH * MF_SCREEN_HEIGHT)

#define MF_COLOR_ARGB(a, r, g, b) \
    (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define MF_GET_A(c) (((c) >> 24) & 0xFF)
#define MF_GET_R(c) (((c) >> 16) & 0xFF)
#define MF_GET_G(c) (((c) >> 8) & 0xFF)
#define MF_GET_B(c) ((c) & 0xFF)

static inline int16_t mf_clip_s16(int32_t val) {
    if (val > 32767) return 32767;
    if (val < -32768) return -32768;
    return (int16_t)val;
}

static inline uint8_t mf_clip_u8(int32_t val) {
    if (val > 255) return 255;
    if (val < 0) return 0;
    return (uint8_t)val;
}

#endif /* MF_TYPES_H */
