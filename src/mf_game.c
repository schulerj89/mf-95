#include "mf_game.h"
#include <string.h>

void mf_game_init(mf_game_t *game) {
    if (!game) return;
    memset(game, 0, sizeof(mf_game_t));
    mf_ppu_init(&game->ppu);
    mf_audio_init(&game->audio, MF_AUDIO_DEFAULT_RATE);
    game->state = MF_GAME_STATE_RESET;
    game->current_pc = MF_SNES_ADDR_RESET_VECTOR;
    game->next_pc = MF_SNES_ADDR_RESET_VECTOR;
    game->ready_for_jump = false;
}

/*
 * Subroutine: mf_boot_reset
 * Bank:       $00 / $C0
 * Address:    $00:CB63 / $C0:CB6F
 * File Offset: 0x00CB63 / 0x00CB6F
 * Description: Cold reset boot handler. Disables interrupts and auto-joypad,
 *              switches CPU to native execution mode, sets up data bank and stack,
 *              and prepares to dispatch to the initial system setup routine at $C1:0000.
 */
void mf_boot_reset(mf_game_t *game) {
    if (!game) return;

    /* Disable interrupts and auto-joypad read */
    game->nmi_enabled = false;
    game->interrupts_enabled = false;

    /* Long jump into bank $C0 fast ROM entry */
    game->current_pc = MF_SNES_ADDR_BOOT_ENTRY;

    /* Setup data bank register */
    game->data_bank = 0x80;

    /* Setup stack pointer and direct page */
    game->stack_pointer = 0x1FFF;
    game->direct_page = 0x0000;

    /* Advance boot state */
    game->state = MF_GAME_STATE_BOOT;

    /* Setup target address for the first system subroutine jump and halt */
    game->next_pc = MF_SNES_ADDR_INIT_SYSTEM;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c11f04_init_phase5 (Placeholder / Next Target)
 * Bank:       $C1
 * Address:    $C1:1F04
 * File Offset: 0x011F04
 * Description: Fifth system initialization routine called after sub_c10966 returns.
 */
void sub_c11f04_init_phase5(mf_game_t *game) {
    if (!game) return;
    /* Placeholder for next target */
    game->current_pc = MF_SNES_ADDR_INIT_PHASE5;
    game->state = MF_GAME_STATE_INIT_PHASE5;
    game->ready_for_jump = false;
}



