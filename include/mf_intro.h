#ifndef MF_INTRO_H
#define MF_INTRO_H

#include "mf_ppu.h"

#define MF_EA_STREAM_MAGIC "MF95EA1\0"
#define MF_EA_STREAM_MAGIC_LEN 8
#define MF_EA_STREAM_VERSION 1

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t frame_count;
    uint16_t ticks_per_frame;
    uint16_t loop_start;
    uint16_t loop_end;
} mf_intro_info_t;

/* ROM-derived EA presentation stream validation and rendering. */
bool mf_intro_get_info(const uint8_t *data, size_t size, mf_intro_info_t *out_info);
uint16_t mf_intro_frame_for_tick(const mf_intro_info_t *info, uint32_t tick);
bool mf_intro_render_frame(mf_ppu_t *ppu, const uint8_t *data, size_t size,
                           uint16_t frame_index);

#endif /* MF_INTRO_H */
