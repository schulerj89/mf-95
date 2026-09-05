#include "test_common.h"
#include "mf_system.h"
#include "mf_game.h"

static bool test_load_ppu_table_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);

    static const uint8_t s_test_ppu_table[32] = {
        0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x10,
        0x00, 0x10, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x60, 0x17, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00
    };

    sub_c101d7_load_ppu_table(&game, s_test_ppu_table);

    /* Verify WRAM register mirrors */
    if (game.wram[0x0492] != 0x09) return false;
    if (game.wram[0x1C70] != 0x10) return false;

    /* Verify PPU layer configuration */
    if (!game.ppu.bg3_priority_high) return false;
    if (!game.ppu.bg[0].enabled || game.ppu.bg[0].bits_per_pixel != 4) return false;
    if (game.ppu.bg[0].map_base != 0x0400 || game.ppu.bg[0].chr_base != 0x1000) return false;

    if (!game.ppu.bg[1].enabled || game.ppu.bg[1].bits_per_pixel != 4) return false;
    if (game.ppu.bg[1].map_base != 0x0800 || game.ppu.bg[1].chr_base != 0x1000) return false;

    if (game.ppu.bg[2].enabled) return false;

    return true;
}

static bool test_dma_vram_buffer_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);

    /* Cold boot and initialize display registers */
    sub_c10000_init_system(&game);
    sub_c15467_main_menu(&game);

    /* Verify initial VRAM state is clear */
    if (game.ppu.vram[0x0400] != 0) return false;
    if (game.ppu.vram[0x1020] != 0) return false;

    /* Execute subroutine $C1:03E4 to transfer tilemap and synthesize 4bpp tile graphics */
    sub_c103e4_dma_vram_buffer(&game, 0x0400, 0x1000, 0x0001);

    /* Verify DMA transfer lock cleared ($AB = 0) */
    if (game.wram[0x00AB] != 0) return false;

    /* Verify character tiles synthesized at chr_base (0x1000) */
    /* Tile 1 (offset 0x1020..0x103F) and Tile 2 (offset 0x1040..0x105F) must contain non-zero patterns */
    bool tile1_has_data = false;
    for (int i = 0x1020; i < 0x1040; i++) {
        if (game.ppu.vram[i] != 0) tile1_has_data = true;
    }
    if (!tile1_has_data) return false;

    bool tile2_has_data = false;
    for (int i = 0x1040; i < 0x1060; i++) {
        if (game.ppu.vram[i] != 0) tile2_has_data = true;
    }
    if (!tile2_has_data) return false;

    /* Verify tilemap entries populated at map_base (0x0400) */
    if (game.ppu.vram[0x0400] != 0x01 || game.ppu.vram[0x0401] != 0x00) return false;
    if (game.ppu.vram[0x0402] != 0x01 || game.ppu.vram[0x0403] != 0x00) return false;

    /* Verify frame composition renders visible non-black pixels */
    game.ppu.forced_blank = false;
    game.ppu.brightness = 0x0F;
    mf_ppu_render_frame(&game.ppu);

    int non_black_pixels = 0;
    for (int i = 0; i < MF_SCREEN_WIDTH * MF_SCREEN_HEIGHT; i++) {
        if ((game.ppu.framebuffer[i] & 0x00FFFFFF) != 0) {
            non_black_pixels++;
        }
    }

    /* Screen must be filled with visible rendered pixels (not black) */
    if (non_black_pixels == 0) return false;

    return true;
}

int run_ppu_tests(void) {
    int failures = 0;
    printf("[*] Running PPU Mode-1 Compositor & Scanline Self-Test...\n");
    if (mf_ppu_self_test()) {
        printf("    [PASS] PPU Self-Test: Mode-1 backgrounds, tile extraction,\n");
        printf("           CGRAM BGR555 conversion, sprite OAM priorities verified.\n");
    } else {
        printf("    [FAIL] PPU Self-Test: Color composition or priority failure.\n");
        failures++;
    }

    printf("\n[*] Running PPU Display Table Loader Subroutine Self-Test ($C1:01D7)...\n");
    if (test_load_ppu_table_subroutine()) {
        printf("    [PASS] Subroutine $C1:01D7: Mode 1 descriptors parsed, WRAM mirrors written,\n");
        printf("           BG0/BG1 layers enabled, and tilemap/character bases verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:01D7: PPU display table loader failed.\n");
        failures++;
    }

    printf("\n[*] Running DMA VRAM Buffer Subroutine Self-Test ($C1:03E4)...\n");
    if (test_dma_vram_buffer_subroutine()) {
        printf("    [PASS] Subroutine $C1:03E4: Tilemap streamed, 4bpp tile patterns synthesized,\n");
        printf("           DMA lock managed, and non-black screen rendering verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:03E4: DMA VRAM buffer subroutine failed.\n");
        failures++;
    }

    return failures;
}
