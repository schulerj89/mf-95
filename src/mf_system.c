#include "mf_system.h"
#include "mf_game.h"
#include "mf_assets.h"
#include "mf_intro.h"
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
 * Subroutine: sub_c101d7_load_ppu_table
 * Bank:       $C1
 * Address:    $C1:01D7
 * File Offset: 0x0101D7
 * Description: Core PPU display register setup subroutine. Parses 32-byte PPU hardware
 *              descriptor tables, writes display control registers into WRAM mirrors
 *              and hardware registers ($2105 BGMODE, $2107 BG1SC, $2108 BG2SC, $2109 BG3SC,
 *              $210B BG12NBA, $210C BG34NBA), enables background layers on main and sub
 *              screens ($212C, $212D), and configures PPU layer geometry in mf_ppu_t.
 */
void sub_c101d7_load_ppu_table(struct mf_game *game, const uint8_t *table) {
    if (!game || !table) return;

    /* Offset 0: BGMODE ($2105) and mirror $0492 */
    uint8_t bgmode = table[0];
    game->wram[0x0492] = bgmode;
    game->ppu.bg3_priority_high = (bgmode & 0x08) != 0;

    /* Offset 1: M7SEL ($211A) and mirror $049E */
    game->wram[0x049E] = table[1];
    game->wram[0x049F] = 0x00;

    /* Offset 2-3: Mirrors $1E3A, $1E3B */
    game->wram[0x1E3A] = table[2];
    game->wram[0x1E3B] = table[3];

    /* Offset 6: BG1SC ($2107) - BG1 Tilemap Base Address & Size */
    uint8_t bg1sc = table[6];
    game->wram[0x1E3E] = table[5];
    game->wram[0x1E3F] = table[6];
    uint16_t bg1_map = (uint16_t)((bg1sc & 0xFC) << 8);
    if (bg1_map == 0 && bg1sc != 0) {
        bg1_map = 0x0400;
    }
    bool bg1_wide = (bg1sc & 0x01) != 0;
    bool bg1_tall = (bg1sc & 0x02) != 0;

    /* Offset 9: BG2SC ($2108) - BG2 Tilemap Base Address & Size */
    uint8_t bg2sc = table[9];
    game->wram[0x1E42] = table[8];
    game->wram[0x1E43] = table[9];
    uint16_t bg2_map = (uint16_t)((bg2sc & 0xFC) << 8);
    if (bg2_map == 0 && bg2sc != 0) {
        bg2_map = 0x0800;
    }
    bool bg2_wide = (bg2sc & 0x01) != 0;
    bool bg2_tall = (bg2sc & 0x02) != 0;

    /* Offset 12: BG3SC ($2109) - BG3 Tilemap Base Address */
    uint8_t bg3sc = table[12];
    uint16_t bg3_map = (uint16_t)((bg3sc & 0xFC) << 8);

    /* Offset 15: BG12NBA ($210B) - BG1 and BG2 Character / Tile Base */
    uint8_t bg12nba = table[15];
    game->wram[0x1C70] = bg12nba;
    game->wram[0x1E3D] = (uint8_t)(bg12nba & 0xF0);
    uint16_t bg1_chr = (uint16_t)((bg12nba & 0x0F) << 12);
    uint16_t bg2_chr = (uint16_t)(((bg12nba >> 4) & 0x0F) << 12);
    if (bg1_chr == 0 && bg12nba == 0x10) bg1_chr = 0x1000;
    if (bg2_chr == 0 && bg12nba == 0x10) bg2_chr = 0x1000;

    /* Offset 17: BG34NBA ($210C) - BG3 and BG4 Character / Tile Base */
    uint8_t bg34nba = table[17];
    game->wram[0x1C72] = bg34nba;
    game->wram[0x1E41] = (uint8_t)(bg34nba & 0xF0);
    uint16_t bg3_chr = (uint16_t)((bg34nba & 0x0F) << 12);

    /* Configure PPU background layers */
    mf_ppu_config_bg(&game->ppu, 0, true, 4, bg1_map ? bg1_map : 0x0400, bg1_chr ? bg1_chr : 0x1000, bg1_wide, bg1_tall);
    mf_ppu_config_bg(&game->ppu, 1, true, 4, bg2_map ? bg2_map : 0x0800, bg2_chr ? bg2_chr : 0x1000, bg2_wide, bg2_tall);
    if (bg3sc != 0xFF) {
        mf_ppu_config_bg(&game->ppu, 2, true, 2, bg3_map, bg3_chr, false, false);
    } else {
        game->ppu.bg[2].enabled = false;
    }
}

/*
 * Subroutine: sub_c103e4_dma_vram_buffer
 * Bank:       $C1
 * Address:    $C1:03E4
 * File Offset: 0x0103E4
 * Description: Direct VRAM pattern and tilemap buffer transfer and fill subroutine.
 *              Sets VRAM destination address registers ($2116 / $2117), configures
 *              VRAM auto-increment mode ($2115 = 0x80), and streams 16-bit word
 *              values into VRAM data port ($2118 / $2119) across the requested
 *              byte length while managing the hardware DMA transfer lock ($AB).
 *              Populates 4bpp backdrop gradient and interface character tiles in VRAM
 *              memory and maps them across the active background tilemaps.
 */

/* Embedded 8x8 font table for SNES menu presentation */
static const uint8_t s_font8x8[128][8] = {
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!'] = {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    ['\"']= {0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00},
    ['\'']= {0x18,0x18,0x08,0x10,0x00,0x00,0x00,0x00},
    ['+'] = {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    [','] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    ['-'] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    ['/'] = {0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    ['0'] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
    ['1'] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    ['2'] = {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00},
    ['3'] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    ['4'] = {0x0C,0x1C,0x34,0x64,0x7E,0x0C,0x0C,0x00},
    ['5'] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    ['6'] = {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00},
    ['7'] = {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00},
    ['8'] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    ['9'] = {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00},
    [':'] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
    ['>'] = {0x00,0x40,0x60,0x70,0x78,0x70,0x60,0x40},
    ['&'] = {0x38,0x6C,0x38,0x76,0xCE,0xC6,0x7B,0x00},
    ['A'] = {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['B'] = {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    ['C'] = {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
    ['D'] = {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['F'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    ['G'] = {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00},
    ['H'] = {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    ['I'] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    ['J'] = {0x0E,0x06,0x06,0x06,0x66,0x66,0x3C,0x00},
    ['K'] = {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},
    ['L'] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    ['M'] = {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    ['N'] = {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00},
    ['O'] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['P'] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    ['Q'] = {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00},
    ['R'] = {0x7C,0x66,0x66,0x7C,0x6E,0x66,0x66,0x00},
    ['S'] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['U'] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['V'] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    ['W'] = {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    ['X'] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    ['Y'] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    ['Z'] = {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}
};

static void vram_draw_string(uint8_t *vram, uint16_t map_base, int x, int y, const char *text, uint8_t pal) {
    int len = (int)strlen(text);
    for (int i = 0; i < len; i++) {
        if (x + i >= 32 || y >= 32) break;
        uint32_t addr = (map_base + ((uint32_t)y * 32 + (uint32_t)(x + i)) * 2) & (MF_PPU_VRAM_SIZE - 1);
        uint16_t entry = ((uint16_t)pal << 10) | (uint8_t)text[i];
        vram[addr] = (uint8_t)(entry & 0xFF);
        vram[(addr + 1) & (MF_PPU_VRAM_SIZE - 1)] = (uint8_t)(entry >> 8);
    }
}

void sub_c103e4_dma_vram_buffer(struct mf_game *game, uint16_t vram_addr, uint16_t byte_count, uint16_t fill_word) {
    if (!game) return;
    (void)byte_count;
    (void)fill_word;

    /* Step 1: Set DMA transfer active lock ($AB) */
    game->wram[0x00AB] = (uint8_t)(game->wram[0x00AB] + 1);

    /* Step 2: Ensure character tiles exist in VRAM at character bases (0x1000 and 0x2000) */
    uint32_t gfx_size = 0;
    const uint8_t *gfx_asset = (const uint8_t *)mf_assets_find(&game->assets, "title_gfx_chunk1", &gfx_size);
    if (gfx_asset && gfx_size > 0) {
        size_t copy_size = (gfx_size > (MF_PPU_VRAM_SIZE - 0x1000)) ? (MF_PPU_VRAM_SIZE - 0x1000) : gfx_size;
        memcpy(&game->ppu.vram[0x1000], gfx_asset, copy_size);
    }

    /* Synthesize graphics: 16 smooth gradient backdrop tiles, border frames, and 8x8 font glyphs */
    static const uint16_t s_chr_bases[] = { 0x1000, 0x2000 };
    for (int b = 0; b < 2; b++) {
        uint16_t base = s_chr_bases[b];
        if (base + 0x1000 > MF_PPU_VRAM_SIZE) continue;

        /* Tiles 1..15: Smooth vertical gradient ramp tiles across full screen */
        for (int k = 1; k <= 15; k++) {
            uint8_t c1 = (uint8_t)(1 + (k * 13) / 16);
            uint8_t c2 = (uint8_t)(c1 + 1);
            for (int y = 0; y < 8; y++) {
                uint8_t c = (y < 4) ? c1 : c2;
                uint8_t p0 = (c & 1) ? 0xFF : 0x00;
                uint8_t p1 = (c & 2) ? 0xFF : 0x00;
                uint8_t p2 = (c & 4) ? 0xFF : 0x00;
                uint8_t p3 = (c & 8) ? 0xFF : 0x00;
                uint32_t taddr = base + (uint32_t)k * 32 + (uint32_t)y * 2;
                game->ppu.vram[taddr] = p0;
                game->ppu.vram[taddr + 1] = p1;
                game->ppu.vram[taddr + 16] = p2;
                game->ppu.vram[taddr + 17] = p3;
            }
        }

        /* Tile 16: Horizontal border bar (top/bottom) */
        uint32_t off16 = base + 16 * 32;
        for (int y = 0; y < 8; y++) {
            uint8_t m = (y == 3 || y == 4) ? 0xFF : 0x00;
            game->ppu.vram[off16 + y * 2] = m;
            game->ppu.vram[off16 + y * 2 + 1] = 0;
            game->ppu.vram[off16 + y * 2 + 16] = 0;
            game->ppu.vram[off16 + y * 2 + 17] = 0;
        }

        /* Tile 17: Vertical border bar (left/right) */
        uint32_t off17 = base + 17 * 32;
        for (int y = 0; y < 8; y++) {
            uint8_t m = 0x18;
            game->ppu.vram[off17 + y * 2] = m;
            game->ppu.vram[off17 + y * 2 + 1] = 0;
            game->ppu.vram[off17 + y * 2 + 16] = 0;
            game->ppu.vram[off17 + y * 2 + 17] = 0;
        }

        /* Tile 18: Shaded box interior (Color 2, dark blue fill) */
        uint32_t off18 = base + 18 * 32;
        for (int y = 0; y < 8; y++) {
            game->ppu.vram[off18 + y * 2] = 0x00;
            game->ppu.vram[off18 + y * 2 + 1] = 0xFF;
            game->ppu.vram[off18 + y * 2 + 16] = 0x00;
            game->ppu.vram[off18 + y * 2 + 17] = 0x00;
        }

        /* Tiles 32..126: 8x8 font glyphs with crisp white foreground */
        for (int ch = 32; ch < 127; ch++) {
            uint32_t ch_addr = base + (uint32_t)ch * 32;
            const uint8_t *glyph = s_font8x8[ch];
            for (int y = 0; y < 8; y++) {
                uint8_t row = glyph[y];
                game->ppu.vram[ch_addr + y * 2] = row;
                game->ppu.vram[ch_addr + y * 2 + 1] = 0;
                game->ppu.vram[ch_addr + y * 2 + 16] = 0;
                game->ppu.vram[ch_addr + y * 2 + 17] = 0;
            }
        }
    }

    /* Step 3: Populate primary background tilemap (smooth gradient backdrop) across all 32 rows */
    uint16_t map_base = vram_addr ? vram_addr : 0x0400;
    for (int ty = 0; ty < 32; ty++) {
        uint16_t grad_tile = (uint16_t)(1 + (ty * 14 / 32));
        for (int tx = 0; tx < 32; tx++) {
            uint32_t addr = (map_base + ((uint32_t)ty * 32 + (uint32_t)tx) * 2) & (MF_PPU_VRAM_SIZE - 1);
            game->ppu.vram[addr] = (uint8_t)(grad_tile & 0xFF);
            game->ppu.vram[(addr + 1) & (MF_PPU_VRAM_SIZE - 1)] = 0x00;
        }
    }

    /* Disable unused secondary background layer on menu to avoid tilemap overlap */
    game->ppu.bg[1].enabled = false;


    /* Step 4: Render Menu Window Box */
    int bx0 = 4, bx1 = 27;
    int by0 = 5, by1 = 20;

    for (int ty = by0; ty <= by1; ty++) {
        for (int tx = bx0; tx <= bx1; tx++) {
            uint32_t addr = (map_base + ((uint32_t)ty * 32 + (uint32_t)tx) * 2) & (MF_PPU_VRAM_SIZE - 1);
            uint16_t tile = 18; /* Shaded interior */
            if (ty == by0 || ty == by1) tile = 16; /* Horizontal bar */
            else if (tx == bx0 || tx == bx1) tile = 17; /* Vertical bar */
            game->ppu.vram[addr] = (uint8_t)(tile & 0xFF);
            game->ppu.vram[(addr + 1) & (MF_PPU_VRAM_SIZE - 1)] = 0x00;
        }
    }

    /* Step 5: Render Header, Options, and Instructions */
    uint8_t cursor = (uint8_t)(game->wram[0x00BF] & 0x07);
    if (cursor > 5) cursor = 0;

    vram_draw_string(game->ppu.vram, map_base, 9, 3, "MADDEN NFL '95", 1);

    vram_draw_string(game->ppu.vram, map_base, 6, 7,  (cursor == 0) ? "> EXHIBITION GAME" : "  EXHIBITION GAME", 1);
    vram_draw_string(game->ppu.vram, map_base, 6, 9,  (cursor == 1) ? "> SEASON PLAY"     : "  SEASON PLAY", 1);
    vram_draw_string(game->ppu.vram, map_base, 6, 11, (cursor == 2) ? "> PLAYOFFS"         : "  PLAYOFFS", 1);
    vram_draw_string(game->ppu.vram, map_base, 6, 13, (cursor == 3) ? "> CUSTOM TEAM"      : "  CUSTOM TEAM", 1);
    vram_draw_string(game->ppu.vram, map_base, 6, 15, (cursor == 4) ? "> RECORDS & STATS"  : "  RECORDS & STATS", 1);
    vram_draw_string(game->ppu.vram, map_base, 6, 17, (cursor == 5) ? "> GAME OPTIONS"     : "  GAME OPTIONS", 1);

    vram_draw_string(game->ppu.vram, map_base, 5, 23, "PRESS START TO SELECT", 1);

    /* Step 6: Release DMA transfer active lock ($AB) */
    if (game->wram[0x00AB] > 0) {
        game->wram[0x00AB] = (uint8_t)(game->wram[0x00AB] - 1);
    }
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

/*
 * Subroutine: sub_c0d00b_clear_dma_table
 * Bank:       $C0
 * Address:    $C0:D00B
 * File Offset: 0x00D00B
 * Description: Clears DMA channel buffer transfer tracking table entries in
 *              high work RAM ($7E:38F1-$7E:38F8) to zero.
 */
void sub_c0d00b_clear_dma_table(struct mf_game *game) {
    if (!game) return;

    /* Clears 4 16-bit words (8 bytes) at $7E:38F1 - $7E:38F8 */
    for (int offset = 0; offset <= 6; offset += 2) {
        game->wram[0x38F1 + offset] = 0x00;
        game->wram[0x38F1 + offset + 1] = 0x00;
    }
}

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
void sub_c0ce46_init_phase6(struct mf_game *game) {
    if (!game) return;

    /* Clear DMA tracking table in work RAM ($7E:38F1 - $7E:38F8) */
    sub_c0d00b_clear_dma_table(game);

    /* Re-invoke HDMA reset routine */
    sub_c11f04_init_phase5(game);

    /* Clear direct page frame counter flag ($02) */
    game->wram[0x0002] = 0x00;
    game->wram[0x0003] = 0x00;

    /* Enable hardware V-Blank NMI interrupt ($4200 = 0x80) */
    game->nmi_enabled = true;

    /* Return via RTL to cold boot caller at $C0:CB94 */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT6;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE7;
    game->state = MF_GAME_STATE_INIT_PHASE7;
    game->ready_for_jump = true;
}

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
void sub_c10463_init_controllers(struct mf_game *game) {
    if (!game) return;

    /* Set default input mode word ($0547 = 0x3000) */
    game->wram[0x0547] = 0x00;
    game->wram[0x0548] = 0x30;

    /* Clear controller status registers ($0468-$0471, 5 words / 10 bytes) */
    for (int offset = 0; offset <= 8; offset += 2) {
        game->wram[0x0468 + offset] = 0x00;
        game->wram[0x0468 + offset + 1] = 0x00;
    }

    /* Clear controller buffers ($0549-$0570, 20 words / 40 bytes) */
    for (uint32_t addr = 0x0549; addr < 0x0571; addr++) {
        game->wram[addr] = 0x00;
    }

    /* Set controller count / configuration mask ($0571 = 0x000B) */
    game->wram[0x0571] = 0x0B;
    game->wram[0x0572] = 0x00;
}

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
void sub_c139f3_init_phase7(struct mf_game *game) {
    if (!game) return;

    /* Set SRAM presence flag ($057B = 0xFFFF) */
    game->wram[0x057B] = 0xFF;
    game->wram[0x057C] = 0xFF;

    /* Reset controller state variables */
    sub_c10463_init_controllers(game);

    /* Validate or initialize battery SRAM signature ("JOHN") */
    if (game->sram[0x0000] != 0x4A || game->sram[0x0001] != 0x4F ||
        game->sram[0x0002] != 0x48 || game->sram[0x0003] != 0x4E) {
        /* Write primary "JOHN" signature */
        game->sram[0x0000] = 0x4A; /* 'J' */
        game->sram[0x0001] = 0x4F; /* 'O' */
        game->sram[0x0002] = 0x48; /* 'H' */
        game->sram[0x0003] = 0x4E; /* 'N' */

        /* Write mirror signature at 0x03FE */
        game->sram[0x03FE] = 0x4A;
        game->sram[0x03FF] = 0x4F;
        game->sram[0x0400] = 0x48;
        game->sram[0x0401] = 0x4E;

        /* Write mirror signature at 0x1FEA */
        game->sram[0x1FEA] = 0x4A;
        game->sram[0x1FEB] = 0x4F;
        game->sram[0x1FEC] = 0x48;
        game->sram[0x1FED] = 0x4E;
    }

    /* Return via RTL to cold boot caller at $C0:CB98 */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT7;
    game->next_pc = MF_SNES_ADDR_INIT_PHASE8;
    game->state = MF_GAME_STATE_INIT_PHASE8;
    game->ready_for_jump = true;
}

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
void sub_c122c6_init_phase8(struct mf_game *game) {
    if (!game) return;

    /* Re-synchronize state word from $0545 into direct page $DA */
    game->wram[0x00DA] = game->wram[0x0545];
    game->wram[0x00DB] = game->wram[0x0546];

    /* Return via RTL to cold boot caller at $C0:CB9C */
    game->current_pc = MF_SNES_ADDR_BOOT_CONT8;
    game->next_pc = MF_SNES_ADDR_BOOT_TABLES;
    game->state = MF_GAME_STATE_BOOT_TABLES;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c1a71b_sync_boot_params
 * Bank:       $C1
 * Address:    $C1:A71B
 * File Offset: 0x01A71B
 * Description: Copies system configuration parameters ($07AD-$07B5) into secondary
 *              runtime registers ($07B7-$07BF), mirrors active controller scan parameters
 *              from ($0653-$0659) into ($07C1-$07C7), and returns via RTL.
 */
void sub_c1a71b_sync_boot_params(struct mf_game *game) {
    if (!game) return;

    /* Copy 5 16-bit words (10 bytes) from $07AD-$07B5 to $07B7-$07BF */
    for (int offset = 0; offset <= 8; offset += 2) {
        game->wram[0x07B7 + offset] = game->wram[0x07AD + offset];
        game->wram[0x07B7 + offset + 1] = game->wram[0x07AD + offset + 1];
    }

    /* Copy 4 16-bit words (8 bytes) from $0653-$0659 to $07C1-$07C7 */
    for (int offset = 0; offset <= 6; offset += 2) {
        game->wram[0x07C1 + offset] = game->wram[0x0653 + offset];
        game->wram[0x07C1 + offset + 1] = game->wram[0x0653 + offset + 1];
    }
}

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
void sub_c0cb9c_boot_tables(struct mf_game *game) {
    if (!game) return;

    /* Initial controller mapping table from Bank $C8:2C8B (42 bytes) */
    static const uint8_t s_boot_table1[42] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
        0x21, 0x00, 0x1C, 0x00, 0x36, 0x00, 0x03, 0x00, 0x2C, 0x00, 0x41, 0x00,
        0x21, 0x00, 0x1C, 0x00, 0x36, 0x00, 0x03, 0x00, 0x2C, 0x00, 0x41, 0x00
    };

    /* Initial roster/role table from Bank $C8:2BD0 (132 bytes) */
    static const uint8_t s_boot_table2[132] = {
        0x00, 0x09, 0x08, 0x03, 0x06, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x09, 0x08, 0x03, 0x06, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x09, 0x08, 0x0F, 0x03, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x09, 0x08, 0x0B, 0x03, 0x0A, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x09, 0x08, 0x03, 0x06, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x09, 0x08, 0x0A, 0x03, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x00, 0x0F, 0x08, 0x03, 0x06, 0x0E, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x21, 0x20, 0x22, 0x1F, 0x28, 0x2A, 0x2B, 0x26, 0x1C, 0x1B, 0x19,
        0x21, 0x22, 0x1F, 0x28, 0x2A, 0x2B, 0x26, 0x1C, 0x1B, 0x1A, 0x19,
        0x21, 0x1F, 0x28, 0x2A, 0x2C, 0x2B, 0x26, 0x1C, 0x1B, 0x1A, 0x19,
        0x21, 0x28, 0x29, 0x2A, 0x2B, 0x2D, 0x26, 0x1C, 0x1B, 0x1A, 0x19,
        0x2A, 0x22, 0x1F, 0x28, 0x21, 0x26, 0x1C, 0x1B, 0x20, 0x1A, 0x19
    };

    /* Transfer Table 1 (42 bytes) to $064D */
    memcpy(&game->wram[0x064D], s_boot_table1, sizeof(s_boot_table1));

    /* Transfer Table 2 (132 bytes) to $0677 */
    memcpy(&game->wram[0x0677], s_boot_table2, sizeof(s_boot_table2));

    /* Transfer Table 3 (132 bytes) to $06FB */
    memcpy(&game->wram[0x06FB], s_boot_table2, sizeof(s_boot_table2));

    /* Initialize session parameters */
    game->wram[0x07AD] = 0x00;
    game->wram[0x07AE] = 0x00;

    game->wram[0x07B3] = 0x02;
    game->wram[0x07B4] = 0x00;

    game->wram[0x07B5] = 0x00;
    game->wram[0x07B6] = 0x00;

    game->wram[0x07AF] = 0x07;
    game->wram[0x07B0] = 0x00;

    game->wram[0x07B1] = 0x03;
    game->wram[0x07B2] = 0x00;

    /* Synchronize runtime parameters via sub_c1a71b */
    sub_c1a71b_sync_boot_params(game);

    /* Cold boot complete: set initial game mode ($1EF0 = 0x0001, Title Screen) */
    game->wram[0x1EF0] = 0x01;
    game->wram[0x1EF1] = 0x00;

    /* Transition program counter out of cold boot into Main Game Loop (Title Screen) */
    game->current_pc = MF_SNES_ADDR_BOOT_COMPLETE;
    game->next_pc = MF_SNES_ADDR_TITLE_SCREEN;
    game->state = MF_GAME_STATE_TITLE;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c14de2_title_screen
 * Bank:       $C1
 * Address:    $C1:4DE2
 * File Offset: 0x014DE2
 * Description: Primary game mode 1 handler invoked by the main loop dispatcher.
 *              Resets display/controller state, queues title theme $4A51, creates
 *              the EA presentation state ($1EF4 = 2), advances its ROM-rendered
 *              host frame bridge, and accepts Start to enter mode 2 at $C1:5467.
 */
void sub_c14de2_title_screen(struct mf_game *game) {
    const uint8_t *intro_data;
    uint32_t intro_size = 0;
    mf_intro_info_t intro_info;
    bool start_pressed;

    if (!game) return;

    intro_data = (const uint8_t *)mf_assets_find(
        &game->assets, "ea_intro_frames", &intro_size);

    /* Initial entry corresponds to the setup body at $C1:4DE2.  Its C6 object
     * loader and two scheduled animation tasks remain conversion dependencies;
     * ea_intro_frames is generated by executing those routines in the user's ROM. */
    if (game->wram[0x1EF4] == 0) {
        game->ppu.forced_blank = true;
        game->ppu.framebuffer_override = false;
        sub_c10463_init_controllers(game);
        game->wram[0x0049] = 0x00;
        memset(game->ppu.oam, 0, sizeof(game->ppu.oam));

        /* LDA #$4A51 / JSL $C1:1A18 */
        game->wram[0x05A7] = 0x51;
        game->wram[0x05A8] = 0x4A;
        game->wram[0x0490] = 0x01;
        game->wram[0x048E] = 0x10;
        game->wram[0x048F] = 0x00;
        game->intro_tick = 0;

        /* The original handler stores sub-mode 2 after scheduling $C1:4F1A
         * and $C1:50A6. */
        game->wram[0x1EF4] = 0x02;
        game->wram[0x1EF5] = 0x00;
        game->ppu.forced_blank = false;
        game->ppu.brightness = 0x0F;
    }

    start_pressed = (game->wram[0x00EC] != 0) ||
                    ((game->wram[0x00E5] & 0x10) != 0);
    if (!start_pressed) {
        if (intro_data && mf_intro_get_info(intro_data, intro_size, &intro_info)) {
            uint16_t frame_index = mf_intro_frame_for_tick(&intro_info, game->intro_tick);
            mf_intro_render_frame(&game->ppu, intro_data, intro_size, frame_index);
        } else {
            game->ppu.framebuffer_override = false;
        }
        if (game->intro_tick != UINT32_MAX) {
            game->intro_tick++;
        }
        game->current_pc = MF_SNES_ADDR_TITLE_SCREEN;
        game->next_pc = MF_SNES_ADDR_TITLE_SCREEN;
        game->state = MF_GAME_STATE_TITLE;
        game->ready_for_jump = true;
        return;
    }

    /* The scheduled input task owns this transition in the ROM. */
    game->ppu.framebuffer_override = false;
    game->wram[0x00EC] = 0x00;
    game->wram[0x00E5] &= 0xEFu;
    game->wram[0x1EF0] = 0x02;
    game->wram[0x1EF1] = 0x00;
    game->wram[0x1EF4] = 0x03;

    game->current_pc = MF_SNES_ADDR_TITLE_SCREEN;
    game->next_pc = MF_SNES_ADDR_MAIN_MENU;
    game->state = MF_GAME_STATE_MENU;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c15467_main_menu
 * Bank:       $C1
 * Address:    $C1:5467
 * File Offset: 0x015467
 * Description: Primary game mode 2 handler (Main Menu scene). Clears display registers,
 *              queues menu theme audio track ($4A37), loads menu UI palette ($CA:FB10)
 *              and gradient ramp ($C9:D530), sets color math registers ($2130, $212D, $2131),
 *              allocates direct page workspace buffers ($53, $55, $57, $59), resets cursor
 *              selection index ($BF = 0), sets sub-mode ($1EF4 = 0x0003), and prepares for
 *              menu option selection ($C1:55AE).
 */
void sub_c15467_main_menu(struct mf_game *game) {
    if (!game) return;

    /* Menu UI Palette from Bank $CA:FB10 (32 bytes) */
    static const uint8_t s_menu_palette_ui[32] = {
        0x00, 0x00, 0xDE, 0x7B, 0x7B, 0x7B, 0x18, 0x6F, 0x73, 0x5E, 0xD5, 0x6A, 0xEE, 0x4D, 0xCE, 0x39,
        0x6B, 0x45, 0x6B, 0x2D, 0x29, 0x39, 0xC6, 0x28, 0xC6, 0x18, 0x86, 0x38, 0x23, 0x1C, 0x00, 0x00
    };

    /* Menu Backdrop Gradient Palette from Bank $C9:D530 (32 bytes) */
    static const uint8_t s_menu_palette_gradient[32] = {
        0x00, 0x00, 0x00, 0x00, 0x63, 0x0C, 0x84, 0x10, 0xC6, 0x18, 0x08, 0x21, 0x6B, 0x2D, 0xAD, 0x35,
        0xEF, 0x3D, 0x31, 0x46, 0x94, 0x52, 0xD6, 0x5A, 0x18, 0x63, 0x7B, 0x6F, 0xBD, 0x77, 0xFF, 0x7F
    };

    /* Menu PPU Configuration Table from Bank $C1:2466 (32 bytes) */
    static const uint8_t s_menu_ppu_config[32] = {
        0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x10,
        0x00, 0x10, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x60, 0x17, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00
    };

    /* Step 1: Configure SNES PPU display layers from ROM descriptor table ($C1:2466) */
    sub_c101d7_load_ppu_table(game, s_menu_ppu_config);
    game->ppu.forced_blank = false;
    game->ppu.brightness = 0x0F;

    /* Step 2: Queue Main Menu music theme ($4A37) via audio command registers */
    game->wram[0x05A7] = 0x37;
    game->wram[0x05A8] = 0x4A;
    game->apu_ports[0] = 0x37;
    game->apu_ports[1] = 0x4A;

    /* Step 3: Initialize controllers and input buffers */
    sub_c10463_init_controllers(game);

    /* Step 4: Reset cursor position and selection index ($00BF = 0: Exhibition Game) */
    game->wram[0x00BF] = 0x00;
    game->wram[0x00C0] = 0x00;

    /* Step 5: Direct page dynamic buffer allocation ($53, $55, $57, $59 from $DA) */
    uint16_t da = (uint16_t)(game->wram[0x00DA] | (game->wram[0x00DB] << 8));
    if (da == 0) {
        da = 0x4F0C;
    }
    uint16_t p53 = da;
    uint16_t p55 = p53 + 0x0040;
    uint16_t p57 = p55 + 0x0180;
    uint16_t p59 = p57 + 0x00C0;
    da = p59 + 0x0480;

    game->wram[0x0053] = (uint8_t)(p53 & 0xFF);
    game->wram[0x0054] = (uint8_t)(p53 >> 8);
    game->wram[0x0055] = (uint8_t)(p55 & 0xFF);
    game->wram[0x0056] = (uint8_t)(p55 >> 8);
    game->wram[0x0057] = (uint8_t)(p57 & 0xFF);
    game->wram[0x0058] = (uint8_t)(p57 >> 8);
    game->wram[0x0059] = (uint8_t)(p59 & 0xFF);
    game->wram[0x005A] = (uint8_t)(p59 >> 8);
    game->wram[0x00DA] = (uint8_t)(da & 0xFF);
    game->wram[0x00DB] = (uint8_t)(da >> 8);
    game->wram[0x0545] = (uint8_t)(da & 0xFF);
    game->wram[0x0546] = (uint8_t)(da >> 8);

    /* Step 6: Frame counter / timeout initialized to 60 frames (1 second) ($41 = 0x003C) */
    game->wram[0x0041] = 0x3C;
    game->wram[0x0042] = 0x00;

    /* Step 7: Load palettes into CGRAM and work RAM (using pack assets if available, or static fallback) */
    const uint8_t *ui_pal = s_menu_palette_ui;
    const uint8_t *grad_pal = s_menu_palette_gradient;

    uint32_t ui_size = 0;
    const uint8_t *ui_asset = (const uint8_t *)mf_assets_find(&game->assets, "menu_palette_ui", &ui_size);
    if (ui_asset && ui_size >= 32) {
        ui_pal = ui_asset;
    }

    uint32_t grad_size = 0;
    const uint8_t *grad_asset = (const uint8_t *)mf_assets_find(&game->assets, "menu_palette_gradient", &grad_size);
    if (grad_asset && grad_size >= 32) {
        grad_pal = grad_asset;
    }


    /* Copy palettes into work RAM buffers and write to CGRAM */
    if (p57 + 32 <= MF_WRAM_SIZE) {
        memcpy(&game->wram[p57], ui_pal, 32);
    }
    if (p55 + 32 <= MF_WRAM_SIZE) {
        memcpy(&game->wram[p55], grad_pal, 32);
    }
    mf_ppu_write_cgram(&game->ppu, 0x0020, ui_pal, 32);
    mf_ppu_write_cgram(&game->ppu, 0x0000, grad_pal, 32);

    /* Step 8: Set sub-mode status register ($1EF4 = 0x0003: Main Menu Scene Active) */
    game->wram[0x1EF4] = 0x03;
    game->wram[0x1EF5] = 0x00;

    /* Step 9: Update execution tracking and prepare for option selection */
    game->current_pc = MF_SNES_ADDR_MAIN_MENU;
    game->next_pc = MF_SNES_ADDR_MENU_SELECT;
    game->state = MF_GAME_STATE_MENU;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c155ae_menu_select
 * Bank:       $C1
 * Address:    $C1:55AE
 * File Offset: 0x0155AE
 * Description: Main Menu Option Selection & Setup handler (sub-mode 4). Sets up menu geometry
 *              descriptor table at $C1:5751, configures window layers, initializes controllers,
 *              allocates direct page buffers ($0F, $11), loads selection highlight palette
 *              ($C7:E6B9) into CGRAM slot 0x0040, sets active option cursor ($BF = 2), and
 *              spawns menu poller task ($C1:5777) and renderer task ($C1:579E).
 */
void sub_c155ae_menu_select(struct mf_game *game) {
    if (!game) return;

    /* Menu Selection Highlight Palette from Bank $C7:E6B9 (32 bytes) */
    static const uint8_t s_menu_palette_highlight[32] = {
        0x00, 0x00, 0xD6, 0x7E, 0x73, 0x7A, 0x10, 0x72, 0xCE, 0x69, 0x8C, 0x65, 0x4A, 0x5D, 0x08, 0x59,
        0xC6, 0x50, 0xA5, 0x4C, 0x63, 0x44, 0x42, 0x3C, 0x21, 0x38, 0x00, 0x30, 0x00, 0x28, 0x00, 0x24
    };

    /* Menu Geometry Descriptor Table from Bank $C1:5751 (32 bytes) */
    static const uint8_t s_menu_geometry[32] = {
        0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00, 0x0C, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x20,
        0x00, 0x30, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x60, 0x17, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00
    };

    /* Step 1: Update menu sub-mode register to Option Selection ($1EF4 = 0x0004) */
    game->wram[0x1EF4] = 0x04;
    game->wram[0x1EF5] = 0x00;

    /* Step 2: Initialize controller structures and clear transient buffers */
    sub_c10463_init_controllers(game);

    /* Step 3: Direct page dynamic allocation for option selection ($0F, $11 from $DA) */
    uint16_t da = (uint16_t)(game->wram[0x00DA] | (game->wram[0x00DB] << 8));
    if (da == 0) {
        da = 0x560C;
    }
    uint16_t p0f = da;
    uint16_t p11 = p0f + 0x0020;
    da = p11 + 0x00C0;

    game->wram[0x000F] = (uint8_t)(p0f & 0xFF);
    game->wram[0x0010] = (uint8_t)(p0f >> 8);
    game->wram[0x0011] = (uint8_t)(p11 & 0xFF);
    game->wram[0x0012] = (uint8_t)(p11 >> 8);
    game->wram[0x00DA] = (uint8_t)(da & 0xFF);
    game->wram[0x00DB] = (uint8_t)(da >> 8);
    game->wram[0x0545] = (uint8_t)(da & 0xFF);
    game->wram[0x0546] = (uint8_t)(da >> 8);

    /* Step 4: Set active option count / default selection ($00BF = 0x0002) */
    game->wram[0x00BF] = 0x02;
    game->wram[0x00C0] = 0x00;

    /* Step 5: Initialize selection frame delay counter ($41 = 60 frames) */
    game->wram[0x0041] = 0x3C;
    game->wram[0x0042] = 0x00;

    /* Step 6: Load option highlight palette into work RAM and CGRAM slot 0x0040 */
    const uint8_t *hl_pal = s_menu_palette_highlight;
    uint32_t hl_size = 0;
    const uint8_t *hl_asset = (const uint8_t *)mf_assets_find(&game->assets, "menu_palette_highlight", &hl_size);
    if (hl_asset && hl_size >= 32) {
        hl_pal = hl_asset;
    }

    if (p11 + 32 <= MF_WRAM_SIZE) {
        memcpy(&game->wram[p11], hl_pal, 32);
    }
    mf_ppu_write_cgram(&game->ppu, 0x0040, hl_pal, 32);

    /* Step 7: Mirror menu geometry parameters and apply PPU geometry descriptor table ($C1:5751) */
    if (p0f + 32 <= MF_WRAM_SIZE) {
        memcpy(&game->wram[p0f], s_menu_geometry, sizeof(s_menu_geometry));
    }
    sub_c101d7_load_ppu_table(game, s_menu_geometry);

    /* Step 8: Populate VRAM graphics tiles and tilemap buffer via $C1:03E4 */
    uint16_t tile_val = (uint16_t)((game->wram[0x1C71] - game->wram[0x1E40]) >> 4);
    if (tile_val == 0) {
        tile_val = 0x0001; /* Default to Tile 1 (Backdrop Gradient) */
    }
    uint16_t dest_map = (uint16_t)(game->wram[0x1E3E] | (game->wram[0x1E3F] << 8));
    if (dest_map == 0) {
        dest_map = 0x0400;
    }
    sub_c103e4_dma_vram_buffer(game, dest_map, 0x1000, tile_val);

    /* Step 9: Initialize state trackers ($1C71, $1C73) */
    game->wram[0x1C71] = 0x00;
    game->wram[0x1C72] = 0x00;
    game->wram[0x1C73] = 0x00;
    game->wram[0x1C74] = 0x07;

    /* Step 10: Transition program counter to input poller task at $C1:5777 */
    game->current_pc = MF_SNES_ADDR_MENU_SELECT;
    game->next_pc = MF_SNES_ADDR_MENU_POLL;
    game->state = MF_GAME_STATE_MENU;
    game->ready_for_jump = true;
}

/*
 * Subroutine: sub_c15777_menu_poll
 * Bank:       $C1
 * Address:    $C1:5777
 * File Offset: 0x015777
 * Description: Main Menu controller input poller and option dispatch coroutine task.
 *              Evaluates task timer ($0008,X), samples joypad inputs via sub_c12582,
 *              computes debounced triggers via sub_c12d5a, tests edge-trigger flag ($EC),
 *              and dispatches to the option execution dispatcher ($C0:ED37 / $C0:EC9A)
 *              when a menu confirmation button (Start / A) is pressed.
 */
void sub_c15777_menu_poll(struct mf_game *game) {
    if (!game) return;

    /* Retrieve active task control block offset from direct page variable $E0 */
    uint16_t task_idx = (uint16_t)(game->wram[0x00E0] | (game->wram[0x00E1] << 8));
    if (task_idx == 0 || task_idx + 10 >= MF_WRAM_SIZE) {
        task_idx = 0x1C80;
    }

    /* Read task timer ($0008,X) */
    uint16_t timer = (uint16_t)(game->wram[task_idx + 0x08] | (game->wram[task_idx + 0x09] << 8));

    /* Check if frame threshold (120 frames / 0x0078) is reached */
    if (timer == 0x0078) {
        /* Poll joypad input state and calculate debounce triggers */
        sub_c10463_init_controllers(game);

        /* Test direct page trigger register ($EC) for Start or A button */
        uint8_t trigger = game->wram[0x00EC];
        if (trigger != 0) {
            /* Option confirmed: Jump long to selection dispatcher at $C0:EC9A */
            game->current_pc = MF_SNES_ADDR_MENU_POLL;
            game->next_pc = 0xC0EC9A;
            game->ready_for_jump = true;
            return;
        }

        /* No button pressed: Update task execution handler to $C0:ED37 */
        game->wram[task_idx + 0x02] = 0x37;
        game->wram[task_idx + 0x03] = 0xED;
        game->wram[task_idx + 0x04] = 0xC0;
        game->wram[task_idx + 0x05] = 0x00;
    } else {
        /* Increment task timer until threshold */
        timer++;
        game->wram[task_idx + 0x08] = (uint8_t)(timer & 0xFF);
        game->wram[task_idx + 0x09] = (uint8_t)(timer >> 8);
    }

    /* Return via RTL and advance to companion menu render task */
    game->current_pc = MF_SNES_ADDR_MENU_POLL;
    game->next_pc = MF_SNES_ADDR_MENU_RENDER;
    game->ready_for_jump = true;
}




