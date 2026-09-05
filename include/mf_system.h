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

#endif /* MF_SYSTEM_H */
