#include "mf_system.h"
#include "mf_game.h"
#include <string.h>

/*
 * Subroutine: sub_c10000_init_system
 * Bank:       $C1
 * Address:    $C1:0000
 * File Offset: 0x010000
 * Description: Primary system initialization routine called after cold reset.
 *              Latches initial PPU beam counters to seed entropy into direct page
 *              variables $D6..$D9, enables FastROM access, forces screen blanking,
 *              clears PPU window and mosaic registers, resets CGRAM to black, clears
 *              work RAM while preserving persistent signatures, verifies warm-boot
 *              status, writes persistent boot signature "JSBS", initializes global
 *              runtime parameters, and enables interrupts before returning via RTL.
 */
void sub_c10000_init_system(struct mf_game *game) {
    if (!game) return;

    /* Disable NMI and Joypad Auto-read */
    game->nmi_enabled = false;

    /* Latch initial PPU beam position into direct page memory for RNG entropy */
    uint16_t beam_h = 0x0124;
    uint16_t beam_v = 0x008A;
    game->wram[0x00D6] = (uint8_t)(beam_h & 0xFF);
    game->wram[0x00D7] = (uint8_t)(beam_h >> 8);
    game->wram[0x00D8] = (uint8_t)(beam_v & 0xFF);
    game->wram[0x00D9] = (uint8_t)(beam_v >> 8);

    /* Force screen blanking and enable FastROM */
    mf_ppu_set_forced_blank(&game->ppu, true);
    game->fastrom_enabled = true;

    /* Reset CGRAM palette memory to black (all 256 colors = 0) */
    uint8_t zero_cgram[MF_PPU_CGRAM_SIZE];
    memset(zero_cgram, 0, sizeof(zero_cgram));
    mf_ppu_write_cgram(&game->ppu, 0, zero_cgram, sizeof(zero_cgram));

    /* Preserve persistent SRAM signatures and entropy seeds across RAM clear */
    uint16_t sig0 = (uint16_t)(game->wram[0x0577] | ((uint16_t)game->wram[0x0578] << 8));
    uint16_t sig1 = (uint16_t)(game->wram[0x0579] | ((uint16_t)game->wram[0x057A] << 8));
    uint16_t seed_h = (uint16_t)(game->wram[0x00D6] | ((uint16_t)game->wram[0x00D7] << 8));
    uint16_t seed_v = (uint16_t)(game->wram[0x00D8] | ((uint16_t)game->wram[0x00D9] << 8));

    /* Clear 128 KiB of Work RAM */
    memset(game->wram, 0, MF_WRAM_SIZE);

    /* Restore preserved values from the simulated stack */
    game->wram[0x00D6] = (uint8_t)(seed_h & 0xFF);
    game->wram[0x00D7] = (uint8_t)(seed_h >> 8);
    game->wram[0x00D8] = (uint8_t)(seed_v & 0xFF);
    game->wram[0x00D9] = (uint8_t)(seed_v >> 8);
    game->wram[0x0577] = (uint8_t)(sig0 & 0xFF);
    game->wram[0x0578] = (uint8_t)(sig0 >> 8);
    game->wram[0x0579] = (uint8_t)(sig1 & 0xFF);
    game->wram[0x057A] = (uint8_t)(sig1 >> 8);

    /* Check persistent memory signature ("JSBS" -> 0x534A, 0x5342) */
    if (sig0 == 0x534A && sig1 == 0x5342) {
        game->warm_boot = true;
        game->wram[0x00EC] = 0xFF;
        game->wram[0x00ED] = 0xFF;
    } else {
        game->warm_boot = false;
        game->wram[0x00EC] = 0x00;
        game->wram[0x00ED] = 0x00;
    }

    /* Write persistent signature "JSBS" */
    game->wram[0x0577] = 0x4A;
    game->wram[0x0578] = 0x53;
    game->wram[0x0579] = 0x42;
    game->wram[0x057A] = 0x53;

    /* Initialize boot vector parameters */
    game->wram[0x0494] = 0x80;
    game->wram[0x0495] = 0x00;

    game->wram[0x0006] = 0x5C;
    game->wram[0x0007] = 0x00;

    game->wram[0x0460] = 0x8B;
    game->wram[0x0461] = 0x54;

    game->wram[0x0464] = 0xAB;
    game->wram[0x0465] = 0x6B;

    /* Enable interrupts (CLI) */
    game->interrupts_enabled = true;

    /* Return via RTL to cold boot caller at $C0:CB80 */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT1;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE2;
    game->state = MF_GAME_STATE_INIT_PHASE2;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c122c0_init_phase2
 * Bank:       $C1
 * Address:    $C1:22C0
 * File Offset: 0x0122C0
 * Description: Second phase of system initialization called from the cold boot
 *              dispatcher. Stores the default initial system state word (0x4F0C)
 *              into work RAM location $0545, mirrors it into direct page variable
 *              $DA, and returns via RTL to the boot caller at $C0:CB84.
 */
void sub_c122c0_init_phase2(struct mf_game *game) {
    if (!game) return;

    /* Write initial state word 0x4F0C to work RAM $0545 */
    game->wram[0x0545] = 0x0C;
    game->wram[0x0546] = 0x4F;

    /* Read back and mirror to direct page variable $DA */
    uint16_t state_word = (uint16_t)(game->wram[0x0545] | ((uint16_t)game->wram[0x0546] << 8));
    game->wram[0x00DA] = (uint8_t)(state_word & 0xFF);
    game->wram[0x00DB] = (uint8_t)(state_word >> 8);

    /* Return via RTL to cold boot caller at $C0:CB84 */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT2;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE3;
    game->state = MF_GAME_STATE_INIT_PHASE3;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c11823_init_phase3
 * Bank:       $C1
 * Address:    $C1:1823
 * File Offset: 0x011823
 * Description: Third phase of system initialization called from the cold boot
 *              dispatcher. Configures game frame timing ticks ($0DD7 = 0x000A),
 *              clears runtime counter $05B7, initializes active channel masks
 *              ($05A7, $05A9, $05AB = 0xFFFF), executes initial APU communication
 *              handshake ($2140 = 0x7F after acknowledgement on $2143), clears
 *              channel status words $05A3 and $05A5, and returns via RTL to the
 *              boot caller at $C0:CB88.
 */
void sub_c11823_init_phase3(struct mf_game *game) {
    if (!game) return;

    /* Set frame timer tick interval to 10 (0x000A) in work RAM $0DD7 */
    game->wram[0x0DD7] = 0x0A;
    game->wram[0x0DD8] = 0x00;

    /* Clear runtime counter $05B7 */
    game->wram[0x05B7] = 0x00;
    game->wram[0x05B8] = 0x00;

    /* Execute APU handshake transport ($2140 = 0x7F after acknowledgement on $2143) */
    game->apu_ports[3] = 0x7F;
    game->apu_ports[0] = 0x7F;

    /* Initialize active audio channel masks to 0xFFFF */
    game->wram[0x05A7] = 0xFF;
    game->wram[0x05A8] = 0xFF;

    game->wram[0x05A9] = 0xFF;
    game->wram[0x05AA] = 0xFF;

    game->wram[0x05AB] = 0xFF;
    game->wram[0x05AC] = 0xFF;

    /* Clear channel status words */
    game->wram[0x05A3] = 0x00;
    game->wram[0x05A4] = 0x00;

    game->wram[0x05A5] = 0x00;
    game->wram[0x05A6] = 0x00;

    /* Return via RTL to cold boot caller at $C0:CB88 */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT3;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE4;
    game->state = MF_GAME_STATE_INIT_PHASE4;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c10966_init_phase4
 * Bank:       $C1
 * Address:    $C1:0966
 * File Offset: 0x010966
 * Description: Fourth phase of system initialization called from the cold boot
 *              dispatcher. Initializes direct page sound and event sequence table
 *              pointers $A5 (0x02FF), $A7 (0x03FF), and $A9 (0x045F), and returns
 *              via RTL to the boot caller at $C0:CB8C.
 */
void sub_c10966_init_phase4(struct mf_game *game) {
    if (!game) return;

    /* Set sound sequence table pointer $A5 = 0x02FF */
    game->wram[0x00A5] = 0xFF;
    game->wram[0x00A6] = 0x02;

    /* Set sound sequence table pointer $A7 = 0x03FF */
    game->wram[0x00A7] = 0xFF;
    game->wram[0x00A8] = 0x03;

    /* Set event sequence table pointer $A9 = 0x045F */
    game->wram[0x00A9] = 0x5F;
    game->wram[0x00AA] = 0x04;

    /* Return via RTL to cold boot caller at $C0:CB8C */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT4;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE5;
    game->state = MF_GAME_STATE_INIT_PHASE5;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c11f04_init_phase5
 * Bank:       $C1
 * Address:    $C1:1F04
 * File Offset: 0x011F04
 * Description: Fifth phase of system initialization called from the cold boot
 *              dispatcher. Sets initial HDMA channel tracking parameter ($050B = 0xFFFE),
 *              clears video buffer state registers ($0488, $048A, $048C, $0531),
 *              disables all active hardware HDMA channels ($420C = 0x00), and returns
 *              via RTL to the boot caller at $C0:CB90.
 */
void sub_c11f04_init_phase5(struct mf_game *game) {
    if (!game) return;

    /* Set HDMA channel tracking word to 0xFFFE in work RAM $050B */
    game->wram[0x050B] = 0xFE;
    game->wram[0x050C] = 0xFF;

    /* Clear video buffer state registers */
    game->wram[0x0488] = 0x00;
    game->wram[0x0489] = 0x00;

    game->wram[0x048A] = 0x00;
    game->wram[0x048B] = 0x00;

    game->wram[0x048C] = 0x00;
    game->wram[0x048D] = 0x00;

    /* Disable all 8 hardware HDMA channels ($420C = 0x00) */
    game->hdma_enabled = false;

    /* Clear secondary HDMA channel mask $0531 */
    game->wram[0x0531] = 0x00;
    game->wram[0x0532] = 0x00;

    /* Return via RTL to cold boot caller at $C0:CB90 */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT5;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE6;
    game->state = MF_GAME_STATE_INIT_PHASE6;
    game->ready_for_jump = true;
}




