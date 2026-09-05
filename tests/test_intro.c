#include "test_common.h"
#include "mf_intro.h"

static void put_le16(uint8_t *dest, uint16_t value) {
    dest[0] = (uint8_t)(value & 0xFFu);
    dest[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *dest, uint32_t value) {
    dest[0] = (uint8_t)(value & 0xFFu);
    dest[1] = (uint8_t)((value >> 8) & 0xFFu);
    dest[2] = (uint8_t)((value >> 16) & 0xFFu);
    dest[3] = (uint8_t)(value >> 24);
}

static bool test_intro_stream_decoder(void) {
    uint8_t stream[40];
    mf_intro_info_t info;
    mf_ppu_t ppu;

    memset(stream, 0, sizeof(stream));
    memcpy(stream, MF_EA_STREAM_MAGIC, MF_EA_STREAM_MAGIC_LEN);
    put_le16(stream + 8, MF_EA_STREAM_VERSION);
    put_le16(stream + 10, MF_SCREEN_WIDTH);
    put_le16(stream + 12, MF_SCREEN_HEIGHT);
    put_le16(stream + 14, 1); /* frame count */
    put_le16(stream + 16, 2); /* ticks per frame */
    put_le16(stream + 18, 0); /* loop start */
    put_le16(stream + 20, 1); /* loop end */
    put_le32(stream + 24, 32);
    put_le32(stream + 28, 40);

    /* One solid-red 256x224 frame in two maximum-bounded repeat runs. */
    put_le16(stream + 32, 0xFFFF); /* 32768 pixels */
    put_le16(stream + 34, 0x001F); /* BGR555 red */
    put_le16(stream + 36, 0xDFFF); /* 24576 pixels */
    put_le16(stream + 38, 0x001F);

    if (!mf_intro_get_info(stream, sizeof(stream), &info)) return false;
    if (info.frame_count != 1 || info.ticks_per_frame != 2) return false;
    if (mf_intro_frame_for_tick(&info, 0) != 0) return false;
    if (mf_intro_frame_for_tick(&info, 1000) != 0) return false;

    mf_ppu_init(&ppu);
    if (!mf_intro_render_frame(&ppu, stream, sizeof(stream), 0)) return false;
    if (!ppu.framebuffer_override) return false;
    if (ppu.framebuffer[0] != 0xFFFF0000u) return false;
    if (ppu.framebuffer[MF_SCREEN_PIXELS - 1] != 0xFFFF0000u) return false;

    /* Normal scanline composition must not overwrite an active direct frame. */
    mf_ppu_render_frame(&ppu);
    if (ppu.framebuffer[0] != 0xFFFF0000u) return false;

    /* A truncated frame is rejected and releases the override. */
    if (mf_intro_render_frame(&ppu, stream, sizeof(stream) - 1u, 0)) return false;
    if (ppu.framebuffer_override) return false;
    return true;
}

int run_intro_tests(void) {
    int failures = 0;
    printf("\n[*] Running ROM-Derived EA Intro Stream Self-Test...\n");
    if (test_intro_stream_decoder()) {
        printf("    [PASS] EA frame stream: header bounds, BGR555 RLE decode,\n");
        printf("           looping timeline, and PPU framebuffer handoff verified.\n");
    } else {
        printf("    [FAIL] EA frame stream validation or rendering failed.\n");
        failures++;
    }
    return failures;
}
