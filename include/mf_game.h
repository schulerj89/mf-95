#ifndef MF_GAME_H
#define MF_GAME_H

#include "mf_types.h"
#include "mf_ppu.h"
#include "mf_audio.h"
#include "mf_assets.h"
#include "mf_system.h"

#define MF_SNES_ADDR_RESET_VECTOR  0x00CB63
#define MF_SNES_ADDR_BOOT_ENTRY    0xC0CB6F
#define MF_SNES_ADDR_INIT_SYSTEM   0xC10000
#define MF_SNES_ADDR_BOOT_CONT1    0xC0CB80
#define MF_SNES_ADDR_LOAD_PPU_TABLE 0xC101D7
#define MF_SNES_ADDR_INIT_PHASE2   0xC122C0
#define MF_SNES_ADDR_BOOT_CONT2    0xC0CB84
#define MF_SNES_ADDR_INIT_PHASE3   0xC11823
#define MF_SNES_ADDR_BOOT_CONT3    0xC0CB88
#define MF_SNES_ADDR_INIT_PHASE4   0xC10966
#define MF_SNES_ADDR_BOOT_CONT4    0xC0CB8C
#define MF_SNES_ADDR_INIT_PHASE5   0xC11F04
#define MF_SNES_ADDR_BOOT_CONT5    0xC0CB90
#define MF_SNES_ADDR_INIT_PHASE6   0xC0CE46
#define MF_SNES_ADDR_BOOT_CONT6    0xC0CB94
#define MF_SNES_ADDR_INIT_PHASE7   0xC139F3
#define MF_SNES_ADDR_BOOT_CONT7    0xC0CB98
#define MF_SNES_ADDR_INIT_PHASE8   0xC122C6
#define MF_SNES_ADDR_BOOT_CONT8    0xC0CB9C
#define MF_SNES_ADDR_BOOT_TABLES   0xC0CB9C
#define MF_SNES_ADDR_BOOT_COMPLETE 0xC0CBDE
#define MF_SNES_ADDR_TITLE_SCREEN  0xC14DE2
#define MF_SNES_ADDR_MAIN_MENU     0xC15467
#define MF_SNES_ADDR_MENU_SELECT   0xC155AE
#define MF_SNES_ADDR_MENU_POLL     0xC15777
#define MF_SNES_ADDR_MENU_RENDER   0xC1579E

#define MF_SNES_FILE_OFFSET_RESET_VECTOR  0x00CB63
#define MF_SNES_FILE_OFFSET_BOOT_ENTRY    0x00CB6F
#define MF_SNES_FILE_OFFSET_INIT_SYSTEM   0x010000
#define MF_SNES_FILE_OFFSET_BOOT_CONT1    0x00CB80
#define MF_SNES_FILE_OFFSET_LOAD_PPU_TABLE 0x0101D7
#define MF_SNES_FILE_OFFSET_INIT_PHASE2   0x0122C0
#define MF_SNES_FILE_OFFSET_BOOT_CONT2    0x00CB84
#define MF_SNES_FILE_OFFSET_INIT_PHASE3   0x011823
#define MF_SNES_FILE_OFFSET_BOOT_CONT3    0x00CB88
#define MF_SNES_FILE_OFFSET_INIT_PHASE4   0x010966
#define MF_SNES_FILE_OFFSET_BOOT_CONT4    0x00CB8C
#define MF_SNES_FILE_OFFSET_INIT_PHASE5   0x011F04
#define MF_SNES_FILE_OFFSET_BOOT_CONT5    0x00CB90
#define MF_SNES_FILE_OFFSET_INIT_PHASE6   0x00CE46
#define MF_SNES_FILE_OFFSET_BOOT_CONT6    0x00CB94
#define MF_SNES_FILE_OFFSET_INIT_PHASE7   0x0139F3
#define MF_SNES_FILE_OFFSET_BOOT_CONT7    0x00CB98
#define MF_SNES_FILE_OFFSET_INIT_PHASE8   0x0122C6
#define MF_SNES_FILE_OFFSET_BOOT_CONT8    0x00CB9C
#define MF_SNES_FILE_OFFSET_BOOT_TABLES   0x00CB9C
#define MF_SNES_FILE_OFFSET_BOOT_COMPLETE 0x00CBDE
#define MF_SNES_FILE_OFFSET_TITLE_SCREEN  0x014DE2
#define MF_SNES_FILE_OFFSET_MAIN_MENU     0x015467
#define MF_SNES_FILE_OFFSET_MENU_SELECT   0x0155AE
#define MF_SNES_FILE_OFFSET_MENU_POLL     0x015777
#define MF_SNES_FILE_OFFSET_MENU_RENDER   0x01579E


#define MF_WRAM_SIZE 0x20000 /* 128 KiB SNES Work RAM */
#define MF_SRAM_SIZE 0x2000  /* 8 KiB SNES Battery-Backed Save RAM ($30:6000-$7FFF) */

typedef enum {
    MF_GAME_STATE_RESET = 0,
    MF_GAME_STATE_BOOT,
    MF_GAME_STATE_INIT_SYSTEM,
    MF_GAME_STATE_INIT_PHASE2,
    MF_GAME_STATE_INIT_PHASE3,
    MF_GAME_STATE_INIT_PHASE4,
    MF_GAME_STATE_INIT_PHASE5,
    MF_GAME_STATE_INIT_PHASE6,
    MF_GAME_STATE_INIT_PHASE7,
    MF_GAME_STATE_INIT_PHASE8,
    MF_GAME_STATE_BOOT_TABLES,
    MF_GAME_STATE_TITLE,
    MF_GAME_STATE_MENU,
    MF_GAME_STATE_GAMEPLAY
} mf_game_state_t;

typedef struct mf_game {
    mf_ppu_t ppu;
    mf_audio_t audio;
    mf_asset_pack_t assets;
    mf_game_state_t state;
    uint8_t wram[MF_WRAM_SIZE];
    uint8_t sram[MF_SRAM_SIZE];
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
void mf_game_step(mf_game_t *game);


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
 * Subroutine: sub_c15777_menu_poll (Placeholder / Next Target)
 * Bank:       $C1
 * Address:    $C1:5777
 * File Offset: 0x015777
 * Description: Main Menu controller input poller and option dispatch coroutine task.
 */
void sub_c15777_menu_poll(mf_game_t *game);

#endif /* MF_GAME_H */


