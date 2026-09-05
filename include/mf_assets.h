#ifndef MF_ASSETS_H
#define MF_ASSETS_H

#include "mf_types.h"

#define MF_PAK_MAGIC "MF95PAK\0"
#define MF_PAK_MAGIC_LEN 8
#define MF_PAK_VERSION 1
#define MF_MAX_ASSET_NAME 32
#define MF_MAX_ASSET_ENTRIES 64

#define MF_ASSET_TYPE_GRAPHICS 1
#define MF_ASSET_TYPE_AUDIO    2
#define MF_ASSET_TYPE_PALETTE  3
#define MF_ASSET_TYPE_DATA     4

#pragma pack(push, 1)
typedef struct {
    char name[MF_MAX_ASSET_NAME];
    uint32_t type;
    uint32_t offset;
    uint32_t size;
    uint32_t crc32;
} mf_pak_entry_t;

typedef struct {
    char magic[MF_PAK_MAGIC_LEN];
    uint32_t version;
    uint32_t entry_count;
    uint32_t flags;
} mf_pak_header_t;
#pragma pack(pop)

typedef struct {
    bool is_loaded;
    uint32_t entry_count;
    mf_pak_entry_t entries[MF_MAX_ASSET_ENTRIES];
    uint8_t *raw_data;
    size_t raw_data_size;
} mf_asset_pack_t;

/* Core Asset Pack Interface */
void mf_assets_init(mf_asset_pack_t *pack);
bool mf_assets_load(mf_asset_pack_t *pack, const char *filename);
bool mf_assets_load_memory(mf_asset_pack_t *pack, const uint8_t *data, size_t size);
const void *mf_assets_find(const mf_asset_pack_t *pack, const char *name, uint32_t *out_size);
void mf_assets_close(mf_asset_pack_t *pack);

/* Self-Test & Diagnostic */
bool mf_assets_self_test(void);

#endif /* MF_ASSETS_H */
