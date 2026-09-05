#include "mf_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mf_assets_init(mf_asset_pack_t *pack) {
    if (!pack) return;
    memset(pack, 0, sizeof(mf_asset_pack_t));
}

bool mf_assets_load_memory(mf_asset_pack_t *pack, const uint8_t *data, size_t size) {
    if (!pack || !data || size < sizeof(mf_pak_header_t)) {
        return false;
    }

    const mf_pak_header_t *header = (const mf_pak_header_t *)data;
    if (memcmp(header->magic, MF_PAK_MAGIC, MF_PAK_MAGIC_LEN) != 0) {
        return false;
    }

    if (header->version != MF_PAK_VERSION) {
        return false;
    }

    if (header->entry_count > MF_MAX_ASSET_ENTRIES) {
        return false;
    }

    size_t toc_size = sizeof(mf_pak_header_t) + (header->entry_count * sizeof(mf_pak_entry_t));
    if (size < toc_size) {
        return false;
    }

    pack->entry_count = header->entry_count;
    const mf_pak_entry_t *src_entries = (const mf_pak_entry_t *)(data + sizeof(mf_pak_header_t));
    for (uint32_t i = 0; i < pack->entry_count; i++) {
        pack->entries[i] = src_entries[i];
        /* Ensure null-termination */
        pack->entries[i].name[MF_MAX_ASSET_NAME - 1] = '\0';
    }

    pack->raw_data = (uint8_t *)malloc(size);
    if (!pack->raw_data) {
        return false;
    }

    memcpy(pack->raw_data, data, size);
    pack->raw_data_size = size;
    pack->is_loaded = true;
    return true;
}

bool mf_assets_load(mf_asset_pack_t *pack, const char *filename) {
    if (!pack || !filename) return false;

    FILE *f = fopen(filename, "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || (size_t)sz < sizeof(mf_pak_header_t)) {
        fclose(f);
        return false;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (read_bytes != (size_t)sz) {
        free(buf);
        return false;
    }

    bool ok = mf_assets_load_memory(pack, buf, (size_t)sz);
    free(buf);
    return ok;
}

const void *mf_assets_find(const mf_asset_pack_t *pack, const char *name, uint32_t *out_size) {
    if (!pack || !pack->is_loaded || !name) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    for (uint32_t i = 0; i < pack->entry_count; i++) {
        if (strncmp(pack->entries[i].name, name, MF_MAX_ASSET_NAME) == 0) {
            uint32_t offset = pack->entries[i].offset;
            uint32_t size = pack->entries[i].size;

            if (offset + size <= pack->raw_data_size) {
                if (out_size) *out_size = size;
                return pack->raw_data + offset;
            }
        }
    }

    if (out_size) *out_size = 0;
    return NULL;
}

void mf_assets_close(mf_asset_pack_t *pack) {
    if (!pack) return;
    if (pack->raw_data) {
        free(pack->raw_data);
        pack->raw_data = NULL;
    }
    pack->raw_data_size = 0;
    pack->entry_count = 0;
    pack->is_loaded = false;
}

bool mf_assets_self_test(void) {
    /* Construct in-memory synthetic pack for self-test verification */
    uint8_t buffer[256];
    memset(buffer, 0, sizeof(buffer));

    mf_pak_header_t *hdr = (mf_pak_header_t *)buffer;
    memcpy(hdr->magic, MF_PAK_MAGIC, MF_PAK_MAGIC_LEN);
    hdr->version = MF_PAK_VERSION;
    hdr->entry_count = 2;
    hdr->flags = 0;

    mf_pak_entry_t *e1 = (mf_pak_entry_t *)(buffer + sizeof(mf_pak_header_t));
    memcpy(e1->name, "test_asset_1", 13);
    e1->type = MF_ASSET_TYPE_GRAPHICS;
    e1->offset = sizeof(mf_pak_header_t) + (2 * sizeof(mf_pak_entry_t));
    e1->size = 4;
    e1->crc32 = 0x12345678;

    mf_pak_entry_t *e2 = e1 + 1;
    memcpy(e2->name, "test_asset_2", 13);
    e2->type = MF_ASSET_TYPE_AUDIO;
    e2->offset = e1->offset + e1->size;
    e2->size = 6;
    e2->crc32 = 0x87654321;

    /* Payloads */
    uint8_t *payload1 = buffer + e1->offset;
    payload1[0] = 0xDE; payload1[1] = 0xAD; payload1[2] = 0xBE; payload1[3] = 0xEF;

    uint8_t *payload2 = buffer + e2->offset;
    payload2[0] = 'M'; payload2[1] = 'A'; payload2[2] = 'D'; payload2[3] = 'D'; payload2[4] = 'E'; payload2[5] = 'N';

    size_t total_size = e2->offset + e2->size;

    mf_asset_pack_t pack;
    mf_assets_init(&pack);

    if (!mf_assets_load_memory(&pack, buffer, total_size)) {
        return false;
    }

    uint32_t sz1 = 0;
    const uint8_t *p1 = (const uint8_t *)mf_assets_find(&pack, "test_asset_1", &sz1);
    if (!p1 || sz1 != 4 || memcmp(p1, "\xDE\xAD\xBE\xEF", 4) != 0) {
        mf_assets_close(&pack);
        return false;
    }

    uint32_t sz2 = 0;
    const uint8_t *p2 = (const uint8_t *)mf_assets_find(&pack, "test_asset_2", &sz2);
    if (!p2 || sz2 != 6 || memcmp(p2, "MADDEN", 6) != 0) {
        mf_assets_close(&pack);
        return false;
    }

    /* Verify non-existent asset lookup returns NULL */
    uint32_t sz3 = 999;
    const void *p3 = mf_assets_find(&pack, "non_existent", &sz3);
    if (p3 != NULL || sz3 != 0) {
        mf_assets_close(&pack);
        return false;
    }

    mf_assets_close(&pack);
    return true;
}
