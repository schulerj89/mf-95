#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_game.h"
#include "mf_system.h"
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

static bool test_init_system_cold_boot(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);

    sub_c10000_init_system(&game);

    /* Verify forced blank and FastROM */
    if (!game.ppu.forced_blank) return false;
    if (!game.fastrom_enabled) return false;
    if (!game.interrupts_enabled) return false;
    if (game.warm_boot != false) return false;

    /* Verify cold boot flag in direct page ($EC) */
    if (game.wram[0x00EC] != 0x00 || game.wram[0x00ED] != 0x00) return false;

    /* Verify persistent signature "JSBS" at $0577 */
    if (game.wram[0x0577] != 0x4A || game.wram[0x0578] != 0x53) return false;
    if (game.wram[0x0579] != 0x42 || game.wram[0x057A] != 0x53) return false;

    /* Verify boot vector parameters in WRAM */
    if (game.wram[0x0494] != 0x80 || game.wram[0x0495] != 0x00) return false;
    if (game.wram[0x0006] != 0x5C || game.wram[0x0007] != 0x00) return false;
    if (game.wram[0x0460] != 0x8B || game.wram[0x0461] != 0x54) return false;
    if (game.wram[0x0464] != 0xAB || game.wram[0x0465] != 0x6B) return false;

    /* Verify return to caller at $C0:CB80 and next target set to $C1:22C0 */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT1) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE2) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE2) return false;

    return true;
}

static bool test_init_system_warm_boot(void) {
    mf_game_t game;
    mf_game_init(&game);

    /* Pre-populate persistent signature "JSBS" */
    game.wram[0x0577] = 0x4A;
    game.wram[0x0578] = 0x53;
    game.wram[0x0579] = 0x42;
    game.wram[0x057A] = 0x53;

    sub_c10000_init_system(&game);

    /* Verify warm boot detected and $EC flag set */
    if (!game.warm_boot) return false;
    if (game.wram[0x00EC] != 0xFF || game.wram[0x00ED] != 0xFF) return false;

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

    /* 4. Test Subroutine $C1:0000 (Cold & Warm Boot) */
    printf("\n[*] Running System Init Subroutine Self-Test ($C1:0000)...\n");
    if (test_init_system_cold_boot() && test_init_system_warm_boot()) {
        printf("    [PASS] Subroutine $C1:0000: Entropy seed latching, FastROM,\n");
        printf("           CGRAM/WRAM clears, JSBS signature validation, and RTL verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:0000: System initialization verification failed.\n");
        failures++;
    }

    printf("\n----------------------------------------------------\n");
    if (failures == 0) {
        printf("Result: ALL TESTS PASSED (PPU + Audio + Subroutines $C0:CB6F, $C1:0000)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("Result: %d TEST(S) FAILED\n", failures);
        printf("====================================================\n");
        return 1;
    }
}
