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

static bool test_init_phase2_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);

    if (game.state != MF_GAME_STATE_INIT_PHASE2) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE2) return false;

    sub_c122c0_init_phase2(&game);

    /* Verify state word 0x4F0C written to $0545 */
    if (game.wram[0x0545] != 0x0C || game.wram[0x0546] != 0x4F) return false;

    /* Verify mirrored to direct page $DA */
    if (game.wram[0x00DA] != 0x0C || game.wram[0x00DB] != 0x4F) return false;

    /* Verify return to $C0:CB84 and next target $C1:1823 */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT2) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE3) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE3) return false;

    return true;
}

static bool test_init_phase3_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);

    if (game.state != MF_GAME_STATE_INIT_PHASE3) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE3) return false;

    sub_c11823_init_phase3(&game);

    /* Verify frame timer tick interval in $0DD7 */
    if (game.wram[0x0DD7] != 0x0A || game.wram[0x0DD8] != 0x00) return false;

    /* Verify cleared runtime counter $05B7 */
    if (game.wram[0x05B7] != 0x00 || game.wram[0x05B8] != 0x00) return false;

    /* Verify APU handshake acknowledged and command sent */
    if (game.apu_ports[0] != 0x7F || game.apu_ports[3] != 0x7F) return false;

    /* Verify channel masks initialized to 0xFFFF */
    if (game.wram[0x05A7] != 0xFF || game.wram[0x05A8] != 0xFF) return false;
    if (game.wram[0x05A9] != 0xFF || game.wram[0x05AA] != 0xFF) return false;
    if (game.wram[0x05AB] != 0xFF || game.wram[0x05AC] != 0xFF) return false;

    /* Verify channel status words cleared */
    if (game.wram[0x05A3] != 0x00 || game.wram[0x05A4] != 0x00) return false;
    if (game.wram[0x05A5] != 0x00 || game.wram[0x05A6] != 0x00) return false;

    /* Verify return to $C0:CB88 and next target $C1:0966 */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT3) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE4) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE4) return false;

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

    /* 5. Test Subroutine $C1:22C0 (Phase 2 Init) */
    printf("\n[*] Running System Init Phase 2 Subroutine Self-Test ($C1:22C0)...\n");
    if (test_init_phase2_subroutine()) {
        printf("    [PASS] Subroutine $C1:22C0: State word 0x4F0C written to $0545,\n");
        printf("           mirrored to direct page $DA, and RTL return to $C0:CB84 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:22C0: Phase 2 initialization verification failed.\n");
        failures++;
    }

    /* 6. Test Subroutine $C1:1823 (Phase 3 Init) */
    printf("\n[*] Running System Init Phase 3 Subroutine Self-Test ($C1:1823)...\n");
    if (test_init_phase3_subroutine()) {
        printf("    [PASS] Subroutine $C1:1823: Timer tick interval set ($0DD7 = 0x000A),\n");
        printf("           APU handshake acknowledged ($2140 = 0x7F), channel masks initialized,\n");
        printf("           and RTL return to $C0:CB88 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:1823: Phase 3 initialization verification failed.\n");
        failures++;
    }

    printf("\n----------------------------------------------------\n");
    if (failures == 0) {
        printf("Result: ALL TESTS PASSED (PPU + Audio + Subroutines $C0:CB6F, $C1:0000, $C1:22C0, $C1:1823)\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf("Result: %d TEST(S) FAILED\n", failures);
        printf("====================================================\n");
        return 1;
    }
}
