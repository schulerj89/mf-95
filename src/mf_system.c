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
