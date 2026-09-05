#include "test_common.h"

static bool test_title_screen_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);
    sub_c10966_init_phase4(&game);
    sub_c11f04_init_phase5(&game);
    sub_c0ce46_init_phase6(&game);
    sub_c139f3_init_phase7(&game);
    sub_c122c6_init_phase8(&game);
    sub_c0cb9c_boot_tables(&game);

    if (game.state != MF_GAME_STATE_TITLE) return false;
    if (game.next_pc != MF_SNES_ADDR_TITLE_SCREEN) return false;

    /* Execute Mode 1 handler */
    sub_c14de2_title_screen(&game);

    /* Verify screen unblanked after asset load */
    if (game.ppu.forced_blank) return false;
    if (game.ppu.brightness != 0x0F) return false;

    /* Verify title audio track queued ($4A51) */
    if (game.wram[0x05A7] != 0x51 || game.wram[0x05A8] != 0x4A) return false;

    /* Verify palette setup flag set */
    if (game.wram[0x0490] != 0x01) return false;

    /* Verify transition to Mode 2 (Main Menu, $1EF0 = 0x0002) */
    if (game.wram[0x1EF0] != 0x02 || game.wram[0x1EF1] != 0x00) return false;
    if (game.current_pc != MF_SNES_ADDR_TITLE_SCREEN) return false;
    if (game.next_pc != MF_SNES_ADDR_MAIN_MENU) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_MENU) return false;

    return true;
}

static bool test_main_menu_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);
    sub_c10966_init_phase4(&game);
    sub_c11f04_init_phase5(&game);
    sub_c0ce46_init_phase6(&game);
    sub_c139f3_init_phase7(&game);
    sub_c122c6_init_phase8(&game);
    sub_c0cb9c_boot_tables(&game);
    sub_c14de2_title_screen(&game);

    if (game.state != MF_GAME_STATE_MENU) return false;
    if (game.next_pc != MF_SNES_ADDR_MAIN_MENU) return false;

    /* Execute Mode 2 Main Menu handler */
    sub_c15467_main_menu(&game);

    /* Verify screen unblanked and brightened */
    if (game.ppu.forced_blank) return false;
    if (game.ppu.brightness != 0x0F) return false;

    /* Verify Menu music track ($4A37) queued to APU */
    if (game.wram[0x05A7] != 0x37 || game.wram[0x05A8] != 0x4A) return false;
    if (game.apu_ports[0] != 0x37 || game.apu_ports[1] != 0x4A) return false;

    /* Verify initial cursor position ($BF = 0: Exhibition Game) */
    if (game.wram[0x00BF] != 0x00 || game.wram[0x00C0] != 0x00) return false;

    /* Verify frame delay counter ($41 = 60 frames) */
    if (game.wram[0x0041] != 0x3C || game.wram[0x0042] != 0x00) return false;

    /* Verify menu sub-mode status register ($1EF4 = 0x0003: Active Menu) */
    if (game.wram[0x1EF4] != 0x03 || game.wram[0x1EF5] != 0x00) return false;

    /* Verify direct page buffer allocations ($53, $55, $57, $59, $DA) */
    uint16_t p53 = (uint16_t)(game.wram[0x0053] | (game.wram[0x0054] << 8));
    uint16_t p55 = (uint16_t)(game.wram[0x0055] | (game.wram[0x0056] << 8));
    uint16_t p57 = (uint16_t)(game.wram[0x0057] | (game.wram[0x0058] << 8));
    uint16_t p59 = (uint16_t)(game.wram[0x0059] | (game.wram[0x005A] << 8));
    uint16_t da  = (uint16_t)(game.wram[0x00DA] | (game.wram[0x00DB] << 8));

    if (p55 != p53 + 0x0040) return false;
    if (p57 != p55 + 0x0180) return false;
    if (p59 != p57 + 0x00C0) return false;
    if (da  != p59 + 0x0480) return false;

    /* Verify palette data loaded in CGRAM */
    /* UI palette starts at CGRAM 0x0020, Gradient palette at 0x0000 */
    if (game.ppu.cgram[0x0022] != 0xDE || game.ppu.cgram[0x0023] != 0x7B) return false;

    /* Verify program counter transition to sub_c155ae_menu_select ($C1:55AE) */
    if (game.current_pc != MF_SNES_ADDR_MAIN_MENU) return false;
    if (game.next_pc != MF_SNES_ADDR_MENU_SELECT) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_MENU) return false;

    return true;
}

static bool test_menu_select_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);
    sub_c10966_init_phase4(&game);
    sub_c11f04_init_phase5(&game);
    sub_c0ce46_init_phase6(&game);
    sub_c139f3_init_phase7(&game);
    sub_c122c6_init_phase8(&game);
    sub_c0cb9c_boot_tables(&game);
    sub_c14de2_title_screen(&game);
    sub_c15467_main_menu(&game);

    if (game.state != MF_GAME_STATE_MENU) return false;
    if (game.next_pc != MF_SNES_ADDR_MENU_SELECT) return false;

    /* Execute Mode 2 Option Selection setup handler */
    sub_c155ae_menu_select(&game);

    /* Verify menu sub-mode updated ($1EF4 = 0x0004: Option Selection) */
    if (game.wram[0x1EF4] != 0x04 || game.wram[0x1EF5] != 0x00) return false;

    /* Verify active option cursor ($BF = 0x0002) */
    if (game.wram[0x00BF] != 0x02 || game.wram[0x00C0] != 0x00) return false;

    /* Verify selection frame delay ($41 = 60 frames) */
    if (game.wram[0x0041] != 0x3C || game.wram[0x0042] != 0x00) return false;

    /* Verify direct page buffer allocations ($0F, $11, $DA) */
    uint16_t p0f = (uint16_t)(game.wram[0x000F] | (game.wram[0x0010] << 8));
    uint16_t p11 = (uint16_t)(game.wram[0x0011] | (game.wram[0x0012] << 8));
    uint16_t da  = (uint16_t)(game.wram[0x00DA] | (game.wram[0x00DB] << 8));

    if (p11 != p0f + 0x0020) return false;
    if (da  != p11 + 0x00C0) return false;

    /* Verify selection highlight palette loaded in CGRAM slot 0x0040 */
    if (game.ppu.cgram[0x0042] != 0xD6 || game.ppu.cgram[0x0043] != 0x7E) return false;
    if (game.ppu.cgram[0x0044] != 0x73 || game.ppu.cgram[0x0045] != 0x7A) return false;

    /* Verify state trackers ($1C73 = 0x0700) */
    if (game.wram[0x1C73] != 0x00 || game.wram[0x1C74] != 0x07) return false;

    /* Verify transition to menu input poller task at $C1:5777 */
    if (game.current_pc != MF_SNES_ADDR_MENU_SELECT) return false;
    if (game.next_pc != MF_SNES_ADDR_MENU_POLL) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_MENU) return false;

    return true;
}

int run_scene_tests(void) {
    int failures = 0;

    printf("\n[*] Running Title Screen Scene Handler Self-Test ($C1:4DE2)...\n");
    if (test_title_screen_subroutine()) {
        printf("    [PASS] Subroutine $C1:4DE2: PPU screen setup, title audio track ($4A51)\n");
        printf("           queued, palette configured, and transition to Main Menu ($1EF0 = 0x0002,\n");
        printf("           $C1:5467) verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:4DE2: Title screen scene execution failed.\n");
        failures++;
    }

    printf("\n[*] Running Main Menu Scene Handler Self-Test ($C1:5467)...\n");
    if (test_main_menu_subroutine()) {
        printf("    [PASS] Subroutine $C1:5467: Video unblanked, menu music ($4A37)\n");
        printf("           queued, direct page buffers allocated ($53-$59), palettes loaded,\n");
        printf("           sub-mode 3 set, and transition to Option Select ($C1:55AE) verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:5467: Main menu scene execution failed.\n");
        failures++;
    }

    printf("\n[*] Running Menu Option Select Handler Self-Test ($C1:55AE)...\n");
    if (test_menu_select_subroutine()) {
        printf("    [PASS] Subroutine $C1:55AE: Sub-mode 4 set, buffers ($0F, $11) allocated,\n");
        printf("           highlight palette loaded, active options set ($BF = 2), and\n");
        printf("           transition to Menu Poller ($C1:5777) verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:55AE: Menu option select execution failed.\n");
        failures++;
    }

    return failures;
}

