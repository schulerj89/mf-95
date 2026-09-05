#include "mf_game.h"
#include <string.h>

void mf_game_init(mf_game_t *game) {
    if (!game) return;
    memset(game, 0, sizeof(mf_game_t));
    mf_ppu_init(&game->ppu);
    mf_audio_init(&game->audio, MF_AUDIO_DEFAULT_RATE);
    mf_assets_init(&game->assets);
    /* Attempt to load asset pack if present */
    mf_assets_load(&game->assets, "assets/madden95.pak");
    game->state = MF_GAME_STATE_RESET;
    game->current_pc = MF_SNES_ADDR_RESET_VECTOR;
    game->next_pc = MF_SNES_ADDR_RESET_VECTOR;
    game->ready_for_jump = false;
}

void mf_game_step(mf_game_t *game) {
    if (!game) return;

    switch (game->state) {
    case MF_GAME_STATE_RESET:
        mf_boot_reset(game);
        break;
    case MF_GAME_STATE_BOOT:
        sub_c10000_init_system(game);
        break;
    case MF_GAME_STATE_INIT_SYSTEM:
    case MF_GAME_STATE_INIT_PHASE2:
        sub_c122c0_init_phase2(game);
        break;
    case MF_GAME_STATE_INIT_PHASE3:
        sub_c11823_init_phase3(game);
        break;
    case MF_GAME_STATE_INIT_PHASE4:
        sub_c10966_init_phase4(game);
        break;
    case MF_GAME_STATE_INIT_PHASE5:
        sub_c11f04_init_phase5(game);
        break;
    case MF_GAME_STATE_INIT_PHASE6:
        sub_c0ce46_init_phase6(game);
        break;
    case MF_GAME_STATE_INIT_PHASE7:
        sub_c139f3_init_phase7(game);
        break;
    case MF_GAME_STATE_INIT_PHASE8:
        sub_c122c6_init_phase8(game);
        break;
    case MF_GAME_STATE_BOOT_TABLES:
        sub_c0cb9c_boot_tables(game);
        break;
    case MF_GAME_STATE_TITLE:
        sub_c14de2_title_screen(game);
        break;
    case MF_GAME_STATE_MENU:
        if (game->current_pc == MF_SNES_ADDR_TITLE_SCREEN || game->next_pc == MF_SNES_ADDR_MAIN_MENU) {
            sub_c15467_main_menu(game);
        } else if (game->next_pc == MF_SNES_ADDR_MENU_SELECT) {
            sub_c155ae_menu_select(game);
        } else if (game->next_pc == MF_SNES_ADDR_MENU_POLL) {
            sub_c15777_menu_poll(game);
        }
        break;
    default:
        break;
    }

    /* Render PPU scanlines into active framebuffer */
    mf_ppu_render_frame(&game->ppu);
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
 * Subroutine: sub_c1579e_menu_render (Placeholder / Next Target)
 * Bank:       $C1
 * Address:    $C1:579E
 * File Offset: 0x01579E
 * Description: Main Menu visual updater and cursor highlight renderer coroutine task.
 */
void sub_c1579e_menu_render(mf_game_t *game) {
    if (!game) return;
    /* Placeholder for next target */
    game->current_pc = MF_SNES_ADDR_MENU_RENDER;
    game->ready_for_jump = false;
}






