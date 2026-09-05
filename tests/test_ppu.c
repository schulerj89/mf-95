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

    return failures;
}
