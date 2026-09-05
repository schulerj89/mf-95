#ifndef MF_GAME_H
#define MF_GAME_H

#include "mf_types.h"
#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_system.h"

#define MF_SNES_ADDR_RESET_VECTOR  0x00CB63
#define MF_SNES_ADDR_BOOT_ENTRY    0xC0CB6F
#define MF_SNES_ADDR_INIT_SYSTEM   0xC10000
#define MF_SNES_ADDR_BOOT_CONT1    0xC0CB80
#define MF_SNES_ADDR_INIT_PHASE2   0xC122C0
#define MF_SNES_ADDR_BOOT_CONT2    0xC0CB84
#define MF_SNES_ADDR_INIT_PHASE3   0xC11823
#define MF_SNES_ADDR_BOOT_CONT3    0xC0CB88
#define MF_SNES_ADDR_INIT_PHASE4   0xC10966
#define MF_SNES_ADDR_BOOT_CONT4    0xC0CB8C
#define MF_SNES_ADDR_INIT_PHASE5   0xC11F04
#define MF_SNES_ADDR_BOOT_CONT5    0xC0CB90
#define MF_SNES_ADDR_INIT_PHASE6   0xC0CE46

#define MF_SNES_FILE_OFFSET_RESET_VECTOR  0x00CB63
#define MF_SNES_FILE_OFFSET_BOOT_ENTRY    0x00CB6F
#define MF_SNES_FILE_OFFSET_INIT_SYSTEM   0x010000
#define MF_SNES_FILE_OFFSET_BOOT_CONT1    0x00CB80
#define MF_SNES_FILE_OFFSET_INIT_PHASE2   0x0122C0
#define MF_SNES_FILE_OFFSET_BOOT_CONT2    0x00CB84
#define MF_SNES_FILE_OFFSET_INIT_PHASE3   0x011823
#define MF_SNES_FILE_OFFSET_BOOT_CONT3    0x00CB88
#define MF_SNES_FILE_OFFSET_INIT_PHASE4   0x010966
#define MF_SNES_FILE_OFFSET_BOOT_CONT4    0x00CB8C
#define MF_SNES_FILE_OFFSET_INIT_PHASE5   0x011F04
#define MF_SNES_FILE_OFFSET_BOOT_CONT5    0x00CB90
#define MF_SNES_FILE_OFFSET_INIT_PHASE6   0x00CE46

#define MF_WRAM_SIZE 0x20000 /* 128 KiB SNES Work RAM */

typedef enum {
    MF_GAME_STATE_RESET = 0,
    MF_GAME_STATE_BOOT,
    MF_GAME_STATE_INIT_SYSTEM,
    MF_GAME_STATE_INIT_PHASE2,
    MF_GAME_STATE_INIT_PHASE3,
    MF_GAME_STATE_INIT_PHASE4,
    MF_GAME_STATE_INIT_PHASE5,
    MF_GAME_STATE_INIT_PHASE6,
    MF_GAME_STATE_TITLE,
    MF_GAME_STATE_MENU,
    MF_GAME_STATE_GAMEPLAY
} mf_game_state_t;

typedef struct mf_game {
    mf_ppu_t ppu;
    mf_audio_t audio;
    mf_game_state_t state;
    uint8_t wram[MF_WRAM_SIZE];
    uint8_t apu_ports[4];

    /* Architectural state placeholders */
    bool interrupts_enabled;
    bool nmi_enabled;
    bool fastrom_enabled;
    bool hdma_enabled;
    bool warm_boot;
    uint16_t direct_page;
    uint16_t stack_pointer;
    uint8_t data_bank;
    uint32_t current_pc;
    uint32_t next_pc;
    bool ready_for_jump;
} mf_game_t;

/* Core Game Lifecycle */
void mf_game_init(mf_game_t *game);

/*
 * Subroutine: mf_boot_reset
 * Bank:       $00 / $C0
 * Address:    $00:CB63 / $C0:CB6F
 * File Offset: 0x00CB63 / 0x00CB6F
 * Description: Cold reset boot handler. Disables interrupts and auto-joypad,
 *              switches CPU to native execution mode, sets up data bank and stack,
 *              and prepares to dispatch to the initial system setup routine at $C1:0000.
 */
void mf_boot_reset(mf_game_t *game);

/*
 * Subroutine: sub_c0ce46_init_phase6 (Placeholder / Next Target)
 * Bank:       $C0
 * Address:    $C0:CE46
 * File Offset: 0x00CE46
 * Description: Sixth system initialization routine called after sub_c11f04 returns.
 */
void sub_c0ce46_init_phase6(mf_game_t *game);

#endif /* MF_GAME_H */
