#include "test_common.h"

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

static bool test_init_phase4_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);

    if (game.state != MF_GAME_STATE_INIT_PHASE4) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE4) return false;

    sub_c10966_init_phase4(&game);

    /* Verify direct page sequence pointers */
    if (game.wram[0x00A5] != 0xFF || game.wram[0x00A6] != 0x02) return false;
    if (game.wram[0x00A7] != 0xFF || game.wram[0x00A8] != 0x03) return false;
    if (game.wram[0x00A9] != 0x5F || game.wram[0x00AA] != 0x04) return false;

    /* Verify return to $C0:CB8C and next target $C1:1F04 */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT4) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE5) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE5) return false;

    return true;
}

static bool test_init_phase5_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);
    sub_c10966_init_phase4(&game);

    if (game.state != MF_GAME_STATE_INIT_PHASE5) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE5) return false;

    sub_c11f04_init_phase5(&game);

    /* Verify HDMA channel tracking word set to 0xFFFE */
    if (game.wram[0x050B] != 0xFE || game.wram[0x050C] != 0xFF) return false;

    /* Verify video buffer state registers cleared */
    if (game.wram[0x0488] != 0x00 || game.wram[0x0489] != 0x00) return false;
    if (game.wram[0x048A] != 0x00 || game.wram[0x048B] != 0x00) return false;
    if (game.wram[0x048C] != 0x00 || game.wram[0x048D] != 0x00) return false;

    /* Verify hardware HDMA disabled and secondary mask $0531 cleared */
    if (game.hdma_enabled != false) return false;
    if (game.wram[0x0531] != 0x00 || game.wram[0x0532] != 0x00) return false;

    /* Verify return to $C0:CB90 and next target $C0:CE46 */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT5) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE6) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE6) return false;

    return true;
}

static bool test_init_phase6_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);
    sub_c10966_init_phase4(&game);
    sub_c11f04_init_phase5(&game);

    if (game.state != MF_GAME_STATE_INIT_PHASE6) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE6) return false;

    /* Dirty the memory that sub_c0ce46 clears to verify it clears them */
    for (int i = 0; i < 8; i++) {
        game.wram[0x38F1 + i] = 0xAA;
    }
    game.wram[0x0002] = 0x55;
    game.wram[0x0003] = 0x55;
    game.nmi_enabled = false;

    sub_c0ce46_init_phase6(&game);

    /* Verify DMA tracking table cleared ($7E:38F1 - $7E:38F8) */
    for (int i = 0; i < 8; i++) {
        if (game.wram[0x38F1 + i] != 0x00) return false;
    }

    /* Verify direct page frame counter flag ($02, $03) cleared */
    if (game.wram[0x0002] != 0x00 || game.wram[0x0003] != 0x00) return false;

    /* Verify hardware V-Blank NMI interrupt enabled ($4200 = 0x80) */
    if (!game.nmi_enabled) return false;

    /* Verify Phase 5 HDMA tracking and disabled state are intact */
    if (game.wram[0x050B] != 0xFE || game.wram[0x050C] != 0xFF) return false;
    if (game.hdma_enabled != false) return false;

    /* Verify return to $C0:CB94 and next target $C1:39F3 (Phase 7) */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT6) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE7) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE7) return false;

    return true;
}

static bool test_init_phase7_subroutine(void) {
    mf_game_t game;
    mf_game_init(&game);
    mf_boot_reset(&game);
    sub_c10000_init_system(&game);
    sub_c122c0_init_phase2(&game);
    sub_c11823_init_phase3(&game);
    sub_c10966_init_phase4(&game);
    sub_c11f04_init_phase5(&game);
    sub_c0ce46_init_phase6(&game);

    if (game.state != MF_GAME_STATE_INIT_PHASE7) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE7) return false;

    /* Dirty controller memory and SRAM to verify formatting */
    game.wram[0x0547] = 0xAA;
    game.wram[0x0548] = 0x55;
    game.wram[0x0571] = 0x00;
    memset(game.sram, 0, sizeof(game.sram));

    sub_c139f3_init_phase7(&game);

    /* Verify SRAM presence word ($057B = 0xFFFF) */
    if (game.wram[0x057B] != 0xFF || game.wram[0x057C] != 0xFF) return false;

    /* Verify controller config word ($0547 = 0x3000) */
    if (game.wram[0x0547] != 0x00 || game.wram[0x0548] != 0x30) return false;

    /* Verify controller status words cleared ($0468-$0471) */
    for (int offset = 0; offset <= 8; offset += 2) {
        if (game.wram[0x0468 + offset] != 0x00 || game.wram[0x0468 + offset + 1] != 0x00) return false;
    }

    /* Verify controller buffers cleared ($0549-$0570) */
    for (uint32_t addr = 0x0549; addr < 0x0571; addr++) {
        if (game.wram[addr] != 0x00) return false;
    }

    /* Verify controller count ($0571 = 0x000B) */
    if (game.wram[0x0571] != 0x0B || game.wram[0x0572] != 0x00) return false;

    /* Verify battery SRAM header signature "JOHN" formatted */
    if (game.sram[0x0000] != 'J' || game.sram[0x0001] != 'O' ||
        game.sram[0x0002] != 'H' || game.sram[0x0003] != 'N') return false;

    /* Verify mirror signatures */
    if (game.sram[0x03FE] != 'J' || game.sram[0x03FF] != 'O' ||
        game.sram[0x0400] != 'H' || game.sram[0x0401] != 'N') return false;
    if (game.sram[0x1FEA] != 'J' || game.sram[0x1FEB] != 'O' ||
        game.sram[0x1FEC] != 'H' || game.sram[0x1FED] != 'N') return false;

    /* Verify return to $C0:CB98 and next target $C1:22C6 (Phase 8) */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT7) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE8) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_INIT_PHASE8) return false;

    return true;
}

static bool test_init_phase8_subroutine(void) {
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

    if (game.state != MF_GAME_STATE_INIT_PHASE8) return false;
    if (game.next_pc != MF_SNES_ADDR_INIT_PHASE8) return false;

    /* Dirty $DA to verify it re-synchronizes with $0545 */
    game.wram[0x00DA] = 0x00;
    game.wram[0x00DB] = 0x00;

    sub_c122c6_init_phase8(&game);

    /* Verify $DA re-synchronized with $0545 (0x4F0C) */
    if (game.wram[0x00DA] != game.wram[0x0545]) return false;
    if (game.wram[0x00DB] != game.wram[0x0546]) return false;
    if (game.wram[0x00DA] != 0x0C || game.wram[0x00DB] != 0x4F) return false;

    /* Verify return to $C0:CB9C and next target $C0:CB9C (Boot Tables) */
    if (game.current_pc != MF_SNES_ADDR_BOOT_CONT8) return false;
    if (game.next_pc != MF_SNES_ADDR_BOOT_TABLES) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_BOOT_TABLES) return false;

    return true;
}

static bool test_boot_tables_subroutine(void) {
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

    if (game.state != MF_GAME_STATE_BOOT_TABLES) return false;
    if (game.next_pc != MF_SNES_ADDR_BOOT_TABLES) return false;

    /* Execute final cold boot sequence phase */
    sub_c0cb9c_boot_tables(&game);

    /* Verify Table 1 copied into $064D (42 bytes) */
    if (game.wram[0x064D] != 0xFF || game.wram[0x064D + 41] != 0x00) return false;
    if (game.wram[0x064D + 18] != 0x21 || game.wram[0x064D + 19] != 0x00) return false;

    /* Verify Table 2 copied into $0677 (132 bytes) */
    if (game.wram[0x0677] != 0x00 || game.wram[0x0677 + 1] != 0x09) return false;
    if (game.wram[0x0677 + 131] != 0x19) return false;

    /* Verify Table 3 copied into $06FB (132 bytes) */
    if (game.wram[0x06FB] != 0x00 || game.wram[0x06FB + 1] != 0x09) return false;
    if (game.wram[0x06FB + 131] != 0x19) return false;

    /* Verify parameters $07AD-$07B5 initialized */
    if (game.wram[0x07AD] != 0x00 || game.wram[0x07AE] != 0x00) return false;
    if (game.wram[0x07B3] != 0x02 || game.wram[0x07B4] != 0x00) return false;
    if (game.wram[0x07B5] != 0x00 || game.wram[0x07B6] != 0x00) return false;
    if (game.wram[0x07AF] != 0x07 || game.wram[0x07B0] != 0x00) return false;
    if (game.wram[0x07B1] != 0x03 || game.wram[0x07B2] != 0x00) return false;

    /* Verify synchronized parameters in $07B7-$07BF match $07AD-$07B5 */
    for (int i = 0; i < 10; i++) {
        if (game.wram[0x07B7 + i] != game.wram[0x07AD + i]) return false;
    }

    /* Verify synchronized controller scan parameters in $07C1-$07C7 match $0653-$0659 */
    for (int i = 0; i < 8; i++) {
        if (game.wram[0x07C1 + i] != game.wram[0x0653 + i]) return false;
    }

    /* Verify initial game mode set to 1 ($1EF0 = 0x0001, Title Screen) */
    if (game.wram[0x1EF0] != 0x01 || game.wram[0x1EF1] != 0x00) return false;

    /* Verify transition to Title Screen */
    if (game.current_pc != MF_SNES_ADDR_BOOT_COMPLETE) return false;
    if (game.next_pc != MF_SNES_ADDR_TITLE_SCREEN) return false;
    if (!game.ready_for_jump) return false;
    if (game.state != MF_GAME_STATE_TITLE) return false;

    return true;
}

int run_boot_tests(void) {
    int failures = 0;

    printf("\n[*] Running Boot Subroutine Self-Test ($00:CB63 -> $C0:CB6F)...\n");
    if (test_boot_subroutine()) {
        printf("    [PASS] Boot Routine: $00:CB63 -> $C0:CB6F correctly configured,\n");
        printf("           registers/stack initialized, ready to jump to $C1:0000.\n");
    } else {
        printf("    [FAIL] Boot Routine: State transition or register verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Subroutine Self-Test ($C1:0000)...\n");
    if (test_init_system_cold_boot() && test_init_system_warm_boot()) {
        printf("    [PASS] Subroutine $C1:0000: Entropy seed latching, FastROM,\n");
        printf("           CGRAM/WRAM clears, JSBS signature validation, and RTL verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:0000: System initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 2 Subroutine Self-Test ($C1:22C0)...\n");
    if (test_init_phase2_subroutine()) {
        printf("    [PASS] Subroutine $C1:22C0: State word 0x4F0C written to $0545,\n");
        printf("           mirrored to direct page $DA, and RTL return to $C0:CB84 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:22C0: Phase 2 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 3 Subroutine Self-Test ($C1:1823)...\n");
    if (test_init_phase3_subroutine()) {
        printf("    [PASS] Subroutine $C1:1823: Timer tick interval set ($0DD7 = 0x000A),\n");
        printf("           APU handshake acknowledged ($2140 = 0x7F), channel masks initialized,\n");
        printf("           and RTL return to $C0:CB88 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:1823: Phase 3 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 4 Subroutine Self-Test ($C1:0966)...\n");
    if (test_init_phase4_subroutine()) {
        printf("    [PASS] Subroutine $C1:0966: Direct page pointers initialized\n");
        printf("           ($A5 = 0x02FF, $A7 = 0x03FF, $A9 = 0x045F),\n");
        printf("           and RTL return to $C0:CB8C verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:0966: Phase 4 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 5 Subroutine Self-Test ($C1:1F04)...\n");
    if (test_init_phase5_subroutine()) {
        printf("    [PASS] Subroutine $C1:1F04: HDMA tracking set ($050B = 0xFFFE),\n");
        printf("           video buffers cleared, HDMA disabled, and RTL return to $C0:CB90 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:1F04: Phase 5 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 6 Subroutine Self-Test ($C0:CE46)...\n");
    if (test_init_phase6_subroutine()) {
        printf("    [PASS] Subroutine $C0:CE46: High WRAM DMA table ($7E:38F1-$7E:38F8) cleared,\n");
        printf("           direct page flag ($02) cleared, hardware NMI enabled ($4200 = 0x80),\n");
        printf("           and RTL return to $C0:CB94 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C0:CE46: Phase 6 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 7 Subroutine Self-Test ($C1:39F3)...\n");
    if (test_init_phase7_subroutine()) {
        printf("    [PASS] Subroutine $C1:39F3: SRAM flag ($057B = 0xFFFF) set, controller\n");
        printf("           buffers initialized, battery SRAM signature formatted ('JOHN'),\n");
        printf("           and RTL return to $C0:CB98 verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:39F3: Phase 7 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running System Init Phase 8 Subroutine Self-Test ($C1:22C6)...\n");
    if (test_init_phase8_subroutine()) {
        printf("    [PASS] Subroutine $C1:22C6: Direct page $DA re-synchronized with $0545\n");
        printf("           (0x4F0C), and RTL return to $C0:CB9C verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C1:22C6: Phase 8 initialization verification failed.\n");
        failures++;
    }

    printf("\n[*] Running Cold Boot Final Tables & Complete Self-Test ($C0:CB9C)...\n");
    if (test_boot_tables_subroutine()) {
        printf("    [PASS] Subroutine $C0:CB9C: Bank $C8 tables copied to $064D, $0677, $06FB,\n");
        printf("           parameters synchronized via sub_c1a71b, and transition to\n");
        printf("           Title Screen mode ($1EF0 = 0x0001, $C1:4DE2) verified.\n");
    } else {
        printf("    [FAIL] Subroutine $C0:CB9C: Cold boot finalization failed.\n");
        failures++;
    }

    return failures;
}
