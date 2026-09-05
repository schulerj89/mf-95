#ifndef MF_PPU_H
#define MF_PPU_H

#include "mf_types.h"
#include <stdio.h>

#define MF_PPU_VRAM_SIZE     0x10000 /* 64 KiB VRAM */
#define MF_PPU_CGRAM_SIZE    0x200   /* 512 bytes / 256 BGR555 entries */
#define MF_PPU_OAM_SIZE      544     /* 512 bytes low + 32 bytes high table */
#define MF_PPU_SPRITE_COUNT  128

typedef enum {
    MF_LAYER_BACKDROP = 0,
    MF_LAYER_BG1,
    MF_LAYER_BG2,
    MF_LAYER_BG3,
    MF_LAYER_OBJ
} mf_layer_t;

typedef struct {
    bool enabled;
    uint8_t bits_per_pixel;   /* 4 for BG1/BG2, 2 for BG3 */
    bool wide;                /* 64x32 tiles (2 screen blocks horizontally) */
    bool tall;                /* 32x64 tiles (2 screen blocks vertically) */
    uint16_t map_base;        /* VRAM byte address for tilemap */
    uint16_t chr_base;        /* VRAM byte address for tile patterns */
    int16_t scroll_x;
    int16_t scroll_y;
} mf_bg_config_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t tile;
    uint8_t palette;          /* 0..7 (maps to CGRAM entries 128..255) */
    uint8_t priority;         /* 0..3 */
    bool flip_h;
    bool flip_v;
    bool large_size;          /* false = 8x8, true = 16x16 */
} mf_sprite_t;

typedef struct {
    uint8_t vram[MF_PPU_VRAM_SIZE];
    uint8_t cgram[MF_PPU_CGRAM_SIZE];
    uint8_t oam[MF_PPU_OAM_SIZE];

    mf_bg_config_t bg[3];     /* BG1, BG2, BG3 */
    uint16_t oam_chr_base;    /* Sprite tile pattern base address in VRAM */
    bool bg3_priority_high;
    bool obj_enabled;
    uint8_t brightness;       /* 0 (blank/black) to 15 (full) */
    bool forced_blank;

    uint32_t framebuffer[MF_SCREEN_PIXELS];
} mf_ppu_t;

/* Core PPU Lifecycle & Configuration */
void mf_ppu_init(mf_ppu_t *ppu);
void mf_ppu_reset(mf_ppu_t *ppu);
void mf_ppu_set_brightness(mf_ppu_t *ppu, uint8_t brightness);
void mf_ppu_set_forced_blank(mf_ppu_t *ppu, bool blank);
void mf_ppu_set_oam_chr_base(mf_ppu_t *ppu, uint16_t chr_base);

/* Memory Transfer */
void mf_ppu_write_vram(mf_ppu_t *ppu, uint16_t addr, const uint8_t *data, size_t size);
void mf_ppu_write_cgram(mf_ppu_t *ppu, uint16_t addr, const uint8_t *data, size_t size);
void mf_ppu_write_oam(mf_ppu_t *ppu, uint16_t addr, const uint8_t *data, size_t size);

/* Background Configuration */
void mf_ppu_config_bg(mf_ppu_t *ppu, int bg_idx, bool enabled, uint8_t bpp,
                      uint16_t map_base, uint16_t chr_base, bool wide, bool tall);
void mf_ppu_set_scroll(mf_ppu_t *ppu, int bg_idx, int16_t scroll_x, int16_t scroll_y);

/* Sprite (OAM) Configuration */
void mf_ppu_set_sprite(mf_ppu_t *ppu, int sprite_idx, int16_t x, int16_t y,
                       uint16_t tile, uint8_t palette, uint8_t priority,
                       bool flip_h, bool flip_v, bool large_size);
void mf_ppu_parse_sprite(const mf_ppu_t *ppu, int sprite_idx, mf_sprite_t *out_sprite);

/* Rendering & Composition */
uint32_t mf_ppu_cgram_to_argb(const uint8_t *cgram, int color_idx, uint8_t brightness);
void mf_ppu_render_scanline(mf_ppu_t *ppu, int y);
void mf_ppu_render_frame(mf_ppu_t *ppu);

/* Diagnostic & Export */
bool mf_ppu_save_bmp(const mf_ppu_t *ppu, const char *filepath);
bool mf_ppu_self_test(void);

#endif /* MF_PPU_H */
