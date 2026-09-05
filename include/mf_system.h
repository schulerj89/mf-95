#ifndef MF_SYSTEM_H
#define MF_SYSTEM_H

#include "mf_types.h"

struct mf_game;

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
void sub_c10000_init_system(struct mf_game *game);

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
void sub_c122c0_init_phase2(struct mf_game *game);

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
void sub_c11823_init_phase3(struct mf_game *game);

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
void sub_c10966_init_phase4(struct mf_game *game);

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
void sub_c11f04_init_phase5(struct mf_game *game);

/*
 * Subroutine: sub_c0d00b_clear_dma_table
 * Bank:       $C0
 * Address:    $C0:D00B
 * File Offset: 0x00D00B
 * Description: Clears DMA channel buffer transfer tracking table entries in
 *              high work RAM ($7E:38F1-$7E:38F8) to zero.
 */
void sub_c0d00b_clear_dma_table(struct mf_game *game);

/*
 * Subroutine: sub_c0ce46_init_phase6
 * Bank:       $C0
 * Address:    $C0:CE46
 * File Offset: 0x00CE46
 * Description: Sixth phase of system initialization called from the cold boot
 *              dispatcher. Initializes DMA buffer transfer tracking tables in
 *              high work RAM ($7E:38F1-$7E:38F8), re-executes the HDMA reset
 *              routine, clears the direct page frame counter flag ($02), enables
 *              the SNES hardware V-Blank NMI interrupt via $4200, and returns
 *              via RTL to the boot caller at $C0:CB94.
 */
void sub_c0ce46_init_phase6(struct mf_game *game);

/*
 * Subroutine: sub_c10463_init_controllers
 * Bank:       $C1
 * Address:    $C1:0463
 * File Offset: 0x010463
 * Description: Resets controller state variables. Configures default input mode
 *              word ($0547 = 0x3000), clears input state registers ($0468-$0471),
 *              clears per-player controller buffers ($0549-$0570), sets active
 *              controller count flag ($0571 = 0x000B), and returns via RTL.
 */
void sub_c10463_init_controllers(struct mf_game *game);

/*
 * Subroutine: sub_c139f3_init_phase7
 * Bank:       $C1
 * Address:    $C1:39F3
 * File Offset: 0x0139F3
 * Description: Seventh phase of system initialization called from the cold boot
 *              dispatcher. Tests battery-backed SRAM presence, flags SRAM validity
 *              in work RAM ($057B = 0xFFFF), initializes controller state buffers
 *              via sub_c10463, verifies and formats the persistent battery SRAM
 *              "JOHN" header signature, and returns via RTL to the boot caller at $C0:CB98.
 */
void sub_c139f3_init_phase7(struct mf_game *game);

/*
 * Subroutine: sub_c122c6_init_phase8
 * Bank:       $C1
 * Address:    $C1:22C6
 * File Offset: 0x0122C6
 * Description: Eighth phase of system initialization called from the cold boot
 *              dispatcher. Re-synchronizes the primary system state word ($0545)
 *              into direct page variable ($DA) and returns via RTL to the cold
 *              boot dispatcher at $C0:CB9C.
 */
void sub_c122c6_init_phase8(struct mf_game *game);

/*
 * Subroutine: sub_c1a71b_sync_boot_params
 * Bank:       $C1
 * Address:    $C1:A71B
 * File Offset: 0x01A71B
 * Description: Copies system configuration parameters ($07AD-$07B5) into secondary
 *              runtime registers ($07B7-$07BF), mirrors active controller scan parameters
 *              from ($0653-$0659) into ($07C1-$07C7), and returns via RTL.
 */
void sub_c1a71b_sync_boot_params(struct mf_game *game);

/*
 * Subroutine: sub_c0cb9c_boot_tables
 * Bank:       $C0
 * Address:    $C0:CB9C
 * File Offset: 0x00CB9C
 * Description: Final phase of cold boot initialization. Transfers player roster
 *              and controller configuration tables from Bank $C8 into work RAM
 *              ($064D, $0677, $06FB), initializes game session timing and mode
 *              registers ($07AD-$07B5), executes parameter synchronization via
 *              sub_c1a71b, and sets initial game mode ($1EF0 = 0x0001, Title Screen).
 */
void sub_c0cb9c_boot_tables(struct mf_game *game);

#endif /* MF_SYSTEM_H */
