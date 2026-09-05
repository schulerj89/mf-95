#include "mf_ppu.h"
#include <string.h>
#include <stdlib.h>

void mf_ppu_init(mf_ppu_t *ppu) {
    if (!ppu) return;
    memset(ppu, 0, sizeof(mf_ppu_t));
    ppu->brightness = 15;
    ppu->obj_enabled = true;
    ppu->forced_blank = false;
    ppu->oam_chr_base = 0;
    ppu->bg[0].bits_per_pixel = 4;
    ppu->bg[1].bits_per_pixel = 4;
    ppu->bg[2].bits_per_pixel = 2;
}

void mf_ppu_reset(mf_ppu_t *ppu) {
    mf_ppu_init(ppu);
}

void mf_ppu_set_brightness(mf_ppu_t *ppu, uint8_t brightness) {
    if (!ppu) return;
    ppu->brightness = (brightness > 15) ? 15 : brightness;
}

void mf_ppu_set_forced_blank(mf_ppu_t *ppu, bool blank) {
    if (!ppu) return;
    ppu->forced_blank = blank;
}

void mf_ppu_set_oam_chr_base(mf_ppu_t *ppu, uint16_t chr_base) {
    if (!ppu) return;
    ppu->oam_chr_base = chr_base;
}

void mf_ppu_write_vram(mf_ppu_t *ppu, uint16_t addr, const uint8_t *data, size_t size) {
    if (!ppu || !data) return;
    for (size_t i = 0; i < size; ++i) {
        uint32_t target = (addr + (uint32_t)i) & (MF_PPU_VRAM_SIZE - 1);
        ppu->vram[target] = data[i];
    }
}

void mf_ppu_write_cgram(mf_ppu_t *ppu, uint16_t addr, const uint8_t *data, size_t size) {
    if (!ppu || !data) return;
    for (size_t i = 0; i < size; ++i) {
        uint32_t target = (addr + (uint32_t)i) & (MF_PPU_CGRAM_SIZE - 1);
        ppu->cgram[target] = data[i];
    }
}

void mf_ppu_write_oam(mf_ppu_t *ppu, uint16_t addr, const uint8_t *data, size_t size) {
    if (!ppu || !data) return;
    for (size_t i = 0; i < size; ++i) {
        uint32_t target = (addr + (uint32_t)i);
        if (target < MF_PPU_OAM_SIZE) {
            ppu->oam[target] = data[i];
        }
    }
}

void mf_ppu_config_bg(mf_ppu_t *ppu, int bg_idx, bool enabled, uint8_t bpp,
                      uint16_t map_base, uint16_t chr_base, bool wide, bool tall) {
    if (!ppu || bg_idx < 0 || bg_idx >= 3) return;
    ppu->bg[bg_idx].enabled = enabled;
    ppu->bg[bg_idx].bits_per_pixel = bpp;
    ppu->bg[bg_idx].map_base = map_base;
    ppu->bg[bg_idx].chr_base = chr_base;
    ppu->bg[bg_idx].wide = wide;
    ppu->bg[bg_idx].tall = tall;
}

void mf_ppu_set_scroll(mf_ppu_t *ppu, int bg_idx, int16_t scroll_x, int16_t scroll_y) {
    if (!ppu || bg_idx < 0 || bg_idx >= 3) return;
    ppu->bg[bg_idx].scroll_x = scroll_x;
    ppu->bg[bg_idx].scroll_y = scroll_y;
}

void mf_ppu_set_sprite(mf_ppu_t *ppu, int sprite_idx, int16_t x, int16_t y,
                       uint16_t tile, uint8_t palette, uint8_t priority,
                       bool flip_h, bool flip_v, bool large_size) {
    if (!ppu || sprite_idx < 0 || sprite_idx >= MF_PPU_SPRITE_COUNT) return;

    /* Low table (4 bytes per sprite) */
    uint32_t low_offset = (uint32_t)sprite_idx * 4;
    ppu->oam[low_offset + 0] = (uint8_t)(x & 0xFF);
    ppu->oam[low_offset + 1] = (uint8_t)(y & 0xFF);
    ppu->oam[low_offset + 2] = (uint8_t)(tile & 0xFF);
    
    uint8_t attr = (uint8_t)((tile >> 8) & 0x01);
    attr |= (uint8_t)((palette & 0x07) << 1);
    attr |= (uint8_t)((priority & 0x03) << 4);
    if (flip_h) attr |= 0x40;
    if (flip_v) attr |= 0x80;
    ppu->oam[low_offset + 3] = attr;

    /* High table (2 bits per sprite, 4 sprites per byte) */
    uint32_t high_byte = 512 + (sprite_idx / 4);
    uint32_t shift = (sprite_idx % 4) * 2;
    uint8_t bits = (uint8_t)(((x < 0 || x >= 256) ? 1 : 0) | (large_size ? 2 : 0));
    
    ppu->oam[high_byte] &= ~(0x03 << shift);
    ppu->oam[high_byte] |= (bits << shift);
}

void mf_ppu_parse_sprite(const mf_ppu_t *ppu, int sprite_idx, mf_sprite_t *out_sprite) {
    if (!ppu || !out_sprite || sprite_idx < 0 || sprite_idx >= MF_PPU_SPRITE_COUNT) return;

    uint32_t low_offset = (uint32_t)sprite_idx * 4;
    uint8_t x_low = ppu->oam[low_offset + 0];
    uint8_t y_low = ppu->oam[low_offset + 1];
    uint8_t tile_low = ppu->oam[low_offset + 2];
    uint8_t attr = ppu->oam[low_offset + 3];

    uint32_t high_byte = 512 + (sprite_idx / 4);
    uint32_t shift = (sprite_idx % 4) * 2;
    uint8_t high_bits = (ppu->oam[high_byte] >> shift) & 0x03;

    int16_t x = (int16_t)x_low;
    if (high_bits & 0x01) {
        x -= 256;
    }

    out_sprite->x = x;
    out_sprite->y = (int16_t)y_low;
    out_sprite->tile = (uint16_t)(tile_low | ((attr & 0x01) << 8));
    out_sprite->palette = (attr >> 1) & 0x07;
    out_sprite->priority = (attr >> 4) & 0x03;
    out_sprite->flip_h = (attr & 0x40) != 0;
    out_sprite->flip_v = (attr & 0x80) != 0;
    out_sprite->large_size = (high_bits & 0x02) != 0;
}

uint32_t mf_ppu_cgram_to_argb(const uint8_t *cgram, int color_idx, uint8_t brightness) {
    if (!cgram || color_idx < 0 || color_idx >= 256) return 0xFF000000;
    if (brightness == 0) return 0xFF000000;

    uint32_t addr = ((uint32_t)color_idx * 2) & (MF_PPU_CGRAM_SIZE - 1);
    uint16_t raw = (uint16_t)(cgram[addr] | ((uint16_t)cgram[addr + 1] << 8));

    uint8_t r5 = raw & 0x1F;
    uint8_t g5 = (raw >> 5) & 0x1F;
    uint8_t b5 = (raw >> 10) & 0x1F;

    /* 5-bit to 8-bit expansion: (c * 255 + 15) / 31 */
    uint32_t r = ((uint32_t)r5 * 255 + 15) / 31;
    uint32_t g = ((uint32_t)g5 * 255 + 15) / 31;
    uint32_t b = ((uint32_t)b5 * 255 + 15) / 31;

    if (brightness < 15) {
        r = (r * brightness) / 15;
        g = (g * brightness) / 15;
        b = (b * brightness) / 15;
    }

    return MF_COLOR_ARGB(255, r, g, b);
}

/* Sample single tile pixel from VRAM */
static uint8_t snes_tile_pixel(const uint8_t *vram, uint16_t chr_base, uint16_t tile,
                               uint8_t bpp, int px, int py) {
    uint32_t tile_bytes = (uint32_t)bpp * 8;
    uint32_t offset = (chr_base + tile * tile_bytes) & (MF_PPU_VRAM_SIZE - 1);
    uint8_t bit = (uint8_t)(7 - (px & 7));
    uint8_t val = 0;

    for (uint8_t plane = 0; plane < bpp; plane += 2) {
        uint32_t row_off = (offset + (py & 7) * 2 + (plane / 2) * 16) & (MF_PPU_VRAM_SIZE - 1);
        uint8_t p0 = vram[row_off];
        uint8_t p1 = vram[(row_off + 1) & (MF_PPU_VRAM_SIZE - 1)];
        val |= (uint8_t)(((p0 >> bit) & 1) << plane);
        val |= (uint8_t)(((p1 >> bit) & 1) << (plane + 1));
    }
    return val;
}

typedef struct {
    int color_idx;      /* Final index in CGRAM (0..255), 0 if transparent */
    uint8_t priority;   /* 0 or 1 */
    bool active;
} bg_pixel_sample_t;

static void sample_bg(const mf_ppu_t *ppu, int bg_idx, int screen_x, int screen_y,
                      bg_pixel_sample_t *out_pixel) {
    out_pixel->color_idx = 0;
    out_pixel->priority = 0;
    out_pixel->active = false;

    const mf_bg_config_t *bg = &ppu->bg[bg_idx];
    if (!bg->enabled) return;

    int total_x = (screen_x + bg->scroll_x) & 0x3FF; /* Up to 1024 pixels */
    int total_y = (screen_y + bg->scroll_y) & 0x3FF;

    int tile_x = (total_x >> 3) & 0x3F;
    int tile_y = (total_y >> 3) & 0x3F;

    /* Tilemap screen block selection (32x32 per 2KB block) */
    uint32_t block = 0;
    if (bg->wide && (tile_x >= 32)) {
        block += 1;
        tile_x -= 32;
    }
    if (bg->tall && (tile_y >= 32)) {
        block += bg->wide ? 2 : 1;
        tile_y -= 32;
    }

    uint32_t entry_addr = (bg->map_base + block * 0x800 + (tile_y * 32 + tile_x) * 2) & (MF_PPU_VRAM_SIZE - 1);
    uint16_t entry = (uint16_t)(ppu->vram[entry_addr] | ((uint16_t)ppu->vram[(entry_addr + 1) & (MF_PPU_VRAM_SIZE - 1)] << 8));

    uint16_t tile = entry & 0x03FF;
    uint8_t pal = (entry >> 10) & 0x07;
    uint8_t pri = (entry >> 13) & 0x01;
    bool flip_h = (entry & 0x4000) != 0;
    bool flip_v = (entry & 0x8000) != 0;

    int px = total_x & 7;
    int py = total_y & 7;
    if (flip_h) px = 7 - px;
    if (flip_v) py = 7 - py;

    uint8_t color = snes_tile_pixel(ppu->vram, bg->chr_base, tile, bg->bits_per_pixel, px, py);
    if (color == 0) return; /* Transparent pixel */

    out_pixel->active = true;
    out_pixel->priority = pri;

    if (bg_idx == 2) {
        /* BG3: 2bpp (4 colors per palette) */
        out_pixel->color_idx = pal * 4 + color;
    } else {
        /* BG1 & BG2: 4bpp (16 colors per palette) */
        out_pixel->color_idx = pal * 16 + color;
    }
}

typedef struct {
    int color_idx;      /* 128..255 */
    uint8_t priority;   /* 0..3 */
    int oam_idx;        /* Lower wins ties */
    bool active;
} obj_pixel_sample_t;

static void sample_sprites_scanline(const mf_ppu_t *ppu, int y, obj_pixel_sample_t *row_samples) {
    for (int x = 0; x < MF_SCREEN_WIDTH; ++x) {
        row_samples[x].color_idx = 0;
        row_samples[x].priority = 0;
        row_samples[x].oam_idx = 999;
        row_samples[x].active = false;
    }

    if (!ppu->obj_enabled) return;

    /* Evaluate sprites in reverse order so lower index naturally wins priority ties */
    for (int i = MF_PPU_SPRITE_COUNT - 1; i >= 0; --i) {
        mf_sprite_t spr;
        mf_ppu_parse_sprite(ppu, i, &spr);

        int size = spr.large_size ? 16 : 8;
        if (y < spr.y || y >= spr.y + size) continue;

        int spr_py = y - spr.y;
        if (spr.flip_v) spr_py = size - 1 - spr_py;

        for (int spr_px = 0; spr_px < size; ++spr_px) {
            int screen_x = spr.x + spr_px;
            if (screen_x < 0 || screen_x >= MF_SCREEN_WIDTH) continue;

            int px = spr_px;
            if (spr.flip_h) px = size - 1 - px;

            uint16_t tile = spr.tile;
            int tile_px = px;
            int tile_py = spr_py;

            if (spr.large_size) {
                int tile_col = px / 8;
                int tile_row = spr_py / 8;
                tile = (uint16_t)(spr.tile + tile_col + tile_row * 16);
                tile_px = px % 8;
                tile_py = spr_py % 8;
            }

            /* Sprites are 4bpp, using oam_chr_base */
            uint8_t color = snes_tile_pixel(ppu->vram, ppu->oam_chr_base, tile, 4, tile_px, tile_py);
            if (color == 0) continue; /* Transparent */

            /* Winner if higher priority or (same priority and lower/equal OAM index) */
            if (!row_samples[screen_x].active ||
                spr.priority > row_samples[screen_x].priority ||
                (spr.priority == row_samples[screen_x].priority && i <= row_samples[screen_x].oam_idx)) {
                row_samples[screen_x].active = true;
                row_samples[screen_x].priority = spr.priority;
                row_samples[screen_x].color_idx = 128 + spr.palette * 16 + color;
                row_samples[screen_x].oam_idx = i;
            }
        }
    }
}

void mf_ppu_render_scanline(mf_ppu_t *ppu, int y) {
    if (!ppu || y < 0 || y >= MF_SCREEN_HEIGHT) return;

    uint32_t *dest = &ppu->framebuffer[y * MF_SCREEN_WIDTH];

    if (ppu->forced_blank || ppu->brightness == 0) {
        for (int x = 0; x < MF_SCREEN_WIDTH; ++x) {
            dest[x] = 0xFF000000;
        }
        return;
    }

    obj_pixel_sample_t sprite_row[MF_SCREEN_WIDTH];
    sample_sprites_scanline(ppu, y, sprite_row);

    uint32_t backdrop = mf_ppu_cgram_to_argb(ppu->cgram, 0, ppu->brightness);

    for (int x = 0; x < MF_SCREEN_WIDTH; ++x) {
        bg_pixel_sample_t bg1, bg2, bg3;
        sample_bg(ppu, 0, x, y, &bg1);
        sample_bg(ppu, 1, x, y, &bg2);
        sample_bg(ppu, 2, x, y, &bg3);

        const obj_pixel_sample_t *obj = &sprite_row[x];
        int winner_color = 0;

        /* SNES Mode-1 Priority Ladder (front to back):
         * 1. BG3 (priority 1) if bg3_priority_high is set
         * 2. OBJ (priority 3)
         * 3. BG1 (priority 1)
         * 4. BG2 (priority 1)
         * 5. OBJ (priority 2)
         * 6. BG1 (priority 0)
         * 7. BG2 (priority 0)
         * 8. OBJ (priority 1)
         * 9. BG3 (priority 1) (if not high)
         * 10. BG3 (priority 0)
         * 11. OBJ (priority 0)
         * 12. Backdrop */
        if (ppu->bg3_priority_high && bg3.active && bg3.priority == 1) {
            winner_color = bg3.color_idx;
        } else if (obj->active && obj->priority == 3) {
            winner_color = obj->color_idx;
        } else if (bg1.active && bg1.priority == 1) {
            winner_color = bg1.color_idx;
        } else if (bg2.active && bg2.priority == 1) {
            winner_color = bg2.color_idx;
        } else if (obj->active && obj->priority == 2) {
            winner_color = obj->color_idx;
        } else if (bg1.active && bg1.priority == 0) {
            winner_color = bg1.color_idx;
        } else if (bg2.active && bg2.priority == 0) {
            winner_color = bg2.color_idx;
        } else if (obj->active && obj->priority == 1) {
            winner_color = obj->color_idx;
        } else if (!ppu->bg3_priority_high && bg3.active && bg3.priority == 1) {
            winner_color = bg3.color_idx;
        } else if (bg3.active && bg3.priority == 0) {
            winner_color = bg3.color_idx;
        } else if (obj->active && obj->priority == 0) {
            winner_color = obj->color_idx;
        }

        if (winner_color > 0) {
            dest[x] = mf_ppu_cgram_to_argb(ppu->cgram, winner_color, ppu->brightness);
        } else {
            dest[x] = backdrop;
        }
    }
}

void mf_ppu_render_frame(mf_ppu_t *ppu) {
    if (!ppu) return;
    for (int y = 0; y < MF_SCREEN_HEIGHT; ++y) {
        mf_ppu_render_scanline(ppu, y);
    }
}

bool mf_ppu_save_bmp(const mf_ppu_t *ppu, const char *filepath) {
    if (!ppu || !filepath) return false;

    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    uint32_t width = MF_SCREEN_WIDTH;
    uint32_t height = MF_SCREEN_HEIGHT;
    uint32_t row_bytes = width * 3;
    uint32_t padding = (4 - (row_bytes % 4)) % 4;
    uint32_t image_size = (row_bytes + padding) * height;
    uint32_t file_size = 54 + image_size;

    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(file_size), (uint8_t)(file_size >> 8), (uint8_t)(file_size >> 16), (uint8_t)(file_size >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(width), (uint8_t)(width >> 8), 0, 0,
        (uint8_t)(height), (uint8_t)(height >> 8), 0, 0,
        1, 0,
        24, 0,
        0, 0, 0, 0,
        (uint8_t)(image_size), (uint8_t)(image_size >> 8), (uint8_t)(image_size >> 16), (uint8_t)(image_size >> 24),
        0x12, 0x0B, 0, 0,
        0x12, 0x0B, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    if (fwrite(header, 1, 54, f) != 54) {
        fclose(f);
        return false;
    }

    uint8_t pad_bytes[3] = {0, 0, 0};
    for (int y = (int)height - 1; y >= 0; --y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t argb = ppu->framebuffer[y * width + x];
            uint8_t bgr[3] = {
                (uint8_t)MF_GET_B(argb),
                (uint8_t)MF_GET_G(argb),
                (uint8_t)MF_GET_R(argb)
            };
            fwrite(bgr, 1, 3, f);
        }
        if (padding > 0) {
            fwrite(pad_bytes, 1, padding, f);
        }
    }

    fclose(f);
    return true;
}

bool mf_ppu_self_test(void) {
    mf_ppu_t ppu;
    mf_ppu_init(&ppu);

    /* 1. Backdrop color: Green turf (BGR555 = 0, 20, 0 -> 0x0280) */
    uint16_t green_cgram = 0 | (20 << 5) | (0 << 10);
    uint8_t green_bytes[2] = { (uint8_t)(green_cgram & 0xFF), (uint8_t)(green_cgram >> 8) };
    mf_ppu_write_cgram(&ppu, 0, green_bytes, 2);

    /* 2. CGRAM 1: White yardline color (BGR555 = 31, 31, 31 -> 0x7FFF) */
    uint16_t white_cgram = 0x7FFF;
    uint8_t white_bytes[2] = { (uint8_t)(white_cgram & 0xFF), (uint8_t)(white_cgram >> 8) };
    mf_ppu_write_cgram(&ppu, 2, white_bytes, 2);

    /* 3. CGRAM 129 (Sprite palette 0, color 1): Yellow player helmet (31, 31, 0 -> 0x03FF) */
    uint16_t yellow_cgram = 31 | (31 << 5) | (0 << 10);
    uint8_t yellow_bytes[2] = { (uint8_t)(yellow_cgram & 0xFF), (uint8_t)(yellow_cgram >> 8) };
    mf_ppu_write_cgram(&ppu, (128 + 1) * 2, yellow_bytes, 2);

    /* Setup 4bpp tile data in VRAM at 0x1000:
     * Tile 0: empty
     * Tile 1: Row 4 has color 1 (bitplane 0 set to 0xFF) */
    uint8_t tile_4bpp[32];
    memset(tile_4bpp, 0, sizeof(tile_4bpp));
    tile_4bpp[4 * 2] = 0xFF; /* plane 0, row 4 */
    mf_ppu_write_vram(&ppu, 0x1000 + 32, tile_4bpp, 32);

    /* Setup BG1 tilemap at VRAM 0x0000: tile 1 at position (0, 0) */
    uint16_t map_entry = 1; /* Tile 1, pal 0, pri 0 */
    uint8_t map_bytes[2] = { (uint8_t)(map_entry & 0xFF), (uint8_t)(map_entry >> 8) };
    mf_ppu_write_vram(&ppu, 0x0000, map_bytes, 2);

    mf_ppu_config_bg(&ppu, 0, true, 4, 0x0000, 0x1000, false, false);

    /* Set sprite tile base and add sprite at (10, 10), tile 1, priority 2 */
    mf_ppu_set_oam_chr_base(&ppu, 0x1000);
    mf_ppu_set_sprite(&ppu, 0, 10, 10, 1, 0, 2, false, false, false);

    /* Render frame */
    mf_ppu_render_frame(&ppu);

    /* Verification checks */
    /* Check 1: Pixel (0, 0): BG1 tile 1 row 0 is transparent -> backdrop green */
    uint32_t px_bg = ppu.framebuffer[0 * MF_SCREEN_WIDTH + 0];
    if (MF_GET_G(px_bg) < 100 || MF_GET_R(px_bg) > 50 || MF_GET_B(px_bg) > 50) {
        return false;
    }

    /* Check 2: Pixel (4, 4): BG1 row 4 is white yardline -> white */
    uint32_t px_yard = ppu.framebuffer[4 * MF_SCREEN_WIDTH + 4];
    if (MF_GET_R(px_yard) < 200 || MF_GET_G(px_yard) < 200 || MF_GET_B(px_yard) < 200) {
        return false;
    }

    /* Check 3: Pixel (14, 14): Sprite at (10, 10) row 4 is yellow helmet -> yellow */
    uint32_t px_spr = ppu.framebuffer[14 * MF_SCREEN_WIDTH + 14];
    if (MF_GET_R(px_spr) < 200 || MF_GET_G(px_spr) < 200 || MF_GET_B(px_spr) > 50) {
        return false;
    }

    return true;
}
