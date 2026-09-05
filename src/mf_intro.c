#include "mf_intro.h"
#include <string.h>

#define MF_EA_STREAM_FIXED_HEADER_SIZE 24u

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t bgr555_to_argb(uint16_t color) {
    uint32_t red5 = color & 0x1Fu;
    uint32_t green5 = (color >> 5) & 0x1Fu;
    uint32_t blue5 = (color >> 10) & 0x1Fu;
    /* Match the SNES/Mesen 5-bit expansion exactly: abcde -> abcdeabc. */
    uint32_t red = (red5 << 3) | (red5 >> 2);
    uint32_t green = (green5 << 3) | (green5 >> 2);
    uint32_t blue = (blue5 << 3) | (blue5 >> 2);
    return MF_COLOR_ARGB(255, red, green, blue);
}

bool mf_intro_get_info(const uint8_t *data, size_t size, mf_intro_info_t *out_info) {
    mf_intro_info_t info;
    size_t offset_table_size;
    uint32_t previous_offset;

    if (!data || size < MF_EA_STREAM_FIXED_HEADER_SIZE ||
        memcmp(data, MF_EA_STREAM_MAGIC, MF_EA_STREAM_MAGIC_LEN) != 0 ||
        read_le16(data + 8) != MF_EA_STREAM_VERSION) {
        return false;
    }

    info.width = read_le16(data + 10);
    info.height = read_le16(data + 12);
    info.frame_count = read_le16(data + 14);
    info.ticks_per_frame = read_le16(data + 16);
    info.loop_start = read_le16(data + 18);
    info.loop_end = read_le16(data + 20);

    if (info.width != MF_SCREEN_WIDTH || info.height != MF_SCREEN_HEIGHT ||
        info.frame_count == 0 || info.ticks_per_frame == 0 ||
        info.loop_start >= info.loop_end || info.loop_end > info.frame_count) {
        return false;
    }

    offset_table_size = ((size_t)info.frame_count + 1u) * sizeof(uint32_t);
    if (offset_table_size > size - MF_EA_STREAM_FIXED_HEADER_SIZE) {
        return false;
    }

    previous_offset = read_le32(data + MF_EA_STREAM_FIXED_HEADER_SIZE);
    if (previous_offset < MF_EA_STREAM_FIXED_HEADER_SIZE + offset_table_size ||
        previous_offset > size) {
        return false;
    }

    for (uint32_t i = 1; i <= info.frame_count; ++i) {
        uint32_t offset = read_le32(
            data + MF_EA_STREAM_FIXED_HEADER_SIZE + (size_t)i * sizeof(uint32_t));
        if (offset < previous_offset || offset > size) {
            return false;
        }
        previous_offset = offset;
    }

    if (out_info) {
        *out_info = info;
    }
    return true;
}

uint16_t mf_intro_frame_for_tick(const mf_intro_info_t *info, uint32_t tick) {
    uint32_t logical_frame;
    uint32_t loop_length;

    if (!info || info->frame_count == 0 || info->ticks_per_frame == 0) {
        return 0;
    }

    logical_frame = tick / info->ticks_per_frame;
    if (logical_frame < info->frame_count) {
        return (uint16_t)logical_frame;
    }

    if (info->loop_start >= info->loop_end || info->loop_end > info->frame_count) {
        return (uint16_t)(info->frame_count - 1u);
    }

    loop_length = (uint32_t)info->loop_end - info->loop_start;
    return (uint16_t)(info->loop_start +
        ((logical_frame - info->loop_start) % loop_length));
}

bool mf_intro_render_frame(mf_ppu_t *ppu, const uint8_t *data, size_t size,
                           uint16_t frame_index) {
    mf_intro_info_t info;
    uint32_t start;
    uint32_t end;
    uint32_t cursor;
    size_t output_index = 0;

    if (!ppu || !mf_intro_get_info(data, size, &info) || frame_index >= info.frame_count) {
        if (ppu) ppu->framebuffer_override = false;
        return false;
    }

    start = read_le32(data + MF_EA_STREAM_FIXED_HEADER_SIZE +
                      (size_t)frame_index * sizeof(uint32_t));
    end = read_le32(data + MF_EA_STREAM_FIXED_HEADER_SIZE +
                    ((size_t)frame_index + 1u) * sizeof(uint32_t));
    cursor = start;
    ppu->framebuffer_override = false;

    while (cursor < end && output_index < MF_SCREEN_PIXELS) {
        uint16_t control;
        size_t run_length;

        if (end - cursor < 2u) return false;
        control = read_le16(data + cursor);
        cursor += 2u;
        run_length = (size_t)(control & 0x7FFFu) + 1u;
        if (run_length > MF_SCREEN_PIXELS - output_index) return false;

        if ((control & 0x8000u) != 0) {
            uint32_t argb;
            if (end - cursor < 2u) return false;
            argb = bgr555_to_argb(read_le16(data + cursor));
            cursor += 2u;
            for (size_t i = 0; i < run_length; ++i) {
                ppu->framebuffer[output_index++] = argb;
            }
        } else {
            size_t bytes = run_length * 2u;
            if (bytes > end - cursor) return false;
            for (size_t i = 0; i < run_length; ++i) {
                ppu->framebuffer[output_index++] = bgr555_to_argb(read_le16(data + cursor));
                cursor += 2u;
            }
        }
    }

    if (cursor != end || output_index != MF_SCREEN_PIXELS) {
        return false;
    }

    ppu->framebuffer_override = true;
    return true;
}
