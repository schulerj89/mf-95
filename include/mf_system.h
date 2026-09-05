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

#endif /* MF_SYSTEM_H */
