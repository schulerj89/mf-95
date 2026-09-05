#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool test_boot_subroutine(void) {
    mf_game_t game;
    memset(&game, 0xCC, sizeof(game)); /* Dirty memory test pattern */

    mf_game_init(&game);
    if (game.state != MF_GAME_STATE_RESET) return false;
    if (game.current_pc != MF_SNES_ADDR_RESET_VECTOR) return false;

    mf_boot_reset(&game);

    if (game.state != MF_GAME_STATE_BOOT) return false;
    if (game.interrupts_enabled != false) return false;
    if (game.nmi_enabled != false) return false;
    if (game.current_pc != MF_SNES_ADDR_BOOT_ENTRY) return false;
    if (game.data_bank != 0x80) return false;
    if (game.stack_pointer != 0x1FFF) return false;
    if (game.direct_page != 0x0000) return false;
    if (!game.ready_for_jump) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_SYSTEM) return false;

    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("====================================================\n");
    printf("  Madden NFL '95 C Port (mf-95) Unit & Subroutine   \n");
    printf("====================================================\n\n");

    int failures = 0;

    /* 1. Test PPU Scaffolding */
    printf("[*] Running PPU Mode-1 Compositor & Scanline Self-Test...\n");
    if (mf_ppu_self_test()) {
        printf("    [PASS] PPU Self-Test: Mode-1 backgrounds, tile extraction,\n");
        printf("           CGRAM BGR555 conversion, sprite OAM priorities verified.\n");
    } else {
        printf("    [FAIL] PPU Self-Test: Color composition or priority failure.\n");
        failures++;
    }

    /* 2. Test Audio Scaffolding */
    printf("\n[*] Running Multi-Sound Audio Mixer Self-Test...\n");
    if (mf_audio_self_test()) {
        printf("    [PASS] Audio Self-Test: Multi-voice simultaneous playback,\n");
        printf("           additive mixing, 16.16 resampling, voice lifecycle verified.\n");
    } else {
        printf("    [FAIL] Audio Self-Test: Multi-sound mixer or voice control failure.\n");
        failures++;
    }

    /* 3. Test Boot Subroutine ($00:CB63 / $C0:CB6F) */
    printf("\n[*] Running Boot Subroutine Self-Test ($00:CB63 -> $C0:CB6F)...\n");
    if (test_boot_subroutine()) {
        printf("    [PASS] Boot Routine: $00:CB63 -> $C0:CB6F correctly configured,\n");
        printf("           registers/stack initialized, ready to jump to $C1:0000.\n");
    } else {
        printf("    [FAIL] Boot Routine: State transition or register verification failed.\n");
        failures++;
    }

    printf("\n----------------------------------------------------\n");
    if (failures == 0) {
        printf("Result: ALL TESTS PASSED (PPU + Audio + Boot Subroutine)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("Result: %d TEST(S) FAILED\n", failures);
        printf("====================================================\n");
        return 1;
    }
}
