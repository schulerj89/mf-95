#!/usr/bin/env python3
"""
Madden NFL '95 (SNES) - Asset Extractor & Pack Generator
Extracts graphics, audio descriptors, palettes, and data tables
from an authentic Madden NFL '95 SNES ROM and generates an
external asset container (madden95.pak).

STRICT POLICY:
This tool generates local external assets only. Under no circumstances
should the resulting .pak or raw extracted files be committed to git.
"""

import os
import sys
import struct
import zlib
import argparse

PAK_MAGIC = b"MF95PAK\x00"
PAK_VERSION = 1

# Asset Types
TYPE_GRAPHICS = 1
TYPE_AUDIO    = 2
TYPE_PALETTE  = 3
TYPE_DATA     = 4

KNOWN_ROM_PATHS = [
    r"F:\Games\SNES\Madden NFL 95 (USA).sfc",
    r"F:\Games\SNES\Madden NFL 95 (USA).smc",
    r"rom\madden95.sfc",
    r"rom\madden95.smc",
    r"madden95.sfc",
]

def find_rom():
    for p in KNOWN_ROM_PATHS:
        if os.path.exists(p):
            return p
    return None

def extract_assets(rom_path, output_pak, raw_dir=None):
    if not os.path.exists(rom_path):
        print(f"[ERROR] ROM file not found: {rom_path}")
        return False

    with open(rom_path, "rb") as f:
        rom_data = f.read()

    file_size = len(rom_data)
    header_offset = 0
    if file_size % 1024 == 512:
        header_offset = 512
        print(f"[*] Detected 512-byte copier header; using offset 0x200.")

    base_data = rom_data[header_offset:]
    if len(base_data) < 1048576:
        print(f"[ERROR] ROM data too small: {len(base_data)} bytes (expected at least 1MB).")
        return False

    internal_title = base_data[0x00FFC0:0x00FFD5].decode("ascii", errors="replace").strip()
    print(f"[*] Found SNES Internal Title: '{internal_title}'")
    if "madden" not in internal_title.lower():
        print(f"[WARNING] Internal title does not contain 'MADDEN'. Proceeding with cautious offset extraction.")

    assets = []

    # 1. Controller button mapping table ($C8:2C8B, 42 bytes)
    table1 = base_data[0x082C8B:0x082C8B + 42]
    assets.append({
        "name": "table_c8_2c8b",
        "type": TYPE_DATA,
        "data": table1,
        "desc": "Controller Button Mapping Table (42 bytes)"
    })

    # 2. Team field roster table ($C8:2BD0, 132 bytes)
    table2 = base_data[0x082BD0:0x082BD0 + 132]
    assets.append({
        "name": "table_c8_2bd0",
        "type": TYPE_DATA,
        "data": table2,
        "desc": "Team Field Roster Table (132 bytes)"
    })

    # 3. Bank C6 Compressed Graphics Chunks (IDs 1..4)
    chunk_ptrs = [0x079E35, 0x079E4F, 0x079E81, 0x079E9B]
    for idx, ptr in enumerate(chunk_ptrs, start=1):
        chunk_data = base_data[ptr:ptr + 4096]
        assets.append({
            "name": f"title_gfx_chunk{idx}",
            "type": TYPE_GRAPHICS,
            "data": chunk_data,
            "desc": f"Title Graphic Chunk {idx} (Bank $C6/C7 Stream)"
        })

    # 4. Title Palette Setup Table ($C1:01A6, 512 bytes CGRAM map)
    palette_data = base_data[0x0101A6:0x0101A6 + 512]
    assets.append({
        "name": "title_palette",
        "type": TYPE_PALETTE,
        "data": palette_data,
        "desc": "Master Title Screen CGRAM Palette Data (512 bytes)"
    })

    # 5. Title Theme Audio Sequence ($C1:1A18, Track 0x4A51)
    title_audio_data = base_data[0x011A18:0x011A18 + 2048]
    assets.append({
        "name": "title_music_4a51",
        "type": TYPE_AUDIO,
        "data": title_audio_data,
        "desc": "Title Audio Track 0x4A51 & APU Sequence Stream"
    })

    # 6. Menu Theme Audio Sequence (Track 0x4A37)
    menu_audio_data = base_data[0x015467:0x015467 + 2048]
    assets.append({
        "name": "menu_music_4a37",
        "type": TYPE_AUDIO,
        "data": menu_audio_data,
        "desc": "Main Menu Audio Track 0x4A37 & APU Sequence Stream"
    })

    # 7. Main Menu UI Palette ($CA:FB10, 32 bytes)
    menu_ui_pal = base_data[0x0AFB10:0x0AFB10 + 32]
    assets.append({
        "name": "menu_palette_ui",
        "type": TYPE_PALETTE,
        "data": menu_ui_pal,
        "desc": "Main Menu UI Color Palette ($CA:FB10, 32 bytes)"
    })

    # 8. Main Menu Backdrop Gradient Palette ($C9:D530, 32 bytes)
    menu_grad_pal = base_data[0x09D530:0x09D530 + 32]
    assets.append({
        "name": "menu_palette_gradient",
        "type": TYPE_PALETTE,
        "data": menu_grad_pal,
        "desc": "Main Menu Backdrop Gradient Color Palette ($C9:D530, 32 bytes)"
    })

    # Build Asset Pack Binary
    os.makedirs(os.path.dirname(os.path.abspath(output_pak)), exist_ok=True)

    num_entries = len(assets)
    header_size = 20
    entry_size = 48
    toc_size = header_size + (num_entries * entry_size)

    current_offset = toc_size
    entries_meta = []
    payload = bytearray()

    for a in assets:
        data = a["data"]
        size = len(data)
        crc = zlib.crc32(data) & 0xFFFFFFFF
        entries_meta.append({
            "name": a["name"],
            "type": a["type"],
            "offset": current_offset,
            "size": size,
            "crc32": crc,
            "desc": a["desc"]
        })
        payload.extend(data)
        current_offset += size

    with open(output_pak, "wb") as f_out:
        f_out.write(PAK_MAGIC)
        f_out.write(struct.pack("<III", PAK_VERSION, num_entries, 0))

        for m in entries_meta:
            name_bytes = m["name"].encode("ascii")[:31]
            padded_name = name_bytes.ljust(32, b"\x00")
            f_out.write(padded_name)
            f_out.write(struct.pack("<IIII", m["type"], m["offset"], m["size"], m["crc32"]))

        f_out.write(payload)

    pak_size = os.path.getsize(output_pak)
    print(f"\n[+] Asset pack successfully generated: {output_pak} ({pak_size:,} bytes)")
    print(f"[*] Total entries packed: {num_entries}")
    for idx, m in enumerate(entries_meta, start=1):
        print(f"    {idx:2d}. {m['name']:<20} | Type: {m['type']} | Size: {m['size']:6d} B | CRC32: 0x{m['crc32']:08X} | {m['desc']}")

    if raw_dir:
        os.makedirs(raw_dir, exist_ok=True)
        for a in assets:
            raw_path = os.path.join(raw_dir, f"{a['name']}.bin")
            with open(raw_path, "wb") as rf:
                rf.write(a["data"])
        print(f"[*] Extracted raw asset binaries to: {raw_dir}")

    return True

def verify_pak(pak_path):
    if not os.path.exists(pak_path):
        print(f"[ERROR] Asset pack does not exist: {pak_path}")
        return False

    with open(pak_path, "rb") as f:
        magic = f.read(8)
        if magic != PAK_MAGIC:
            print(f"[ERROR] Invalid PAK magic: {magic}")
            return False
        version, num_entries, flags = struct.unpack("<III", f.read(12))
        print(f"[*] PAK Version: {version}, Entries: {num_entries}, Flags: {flags}")

        for i in range(num_entries):
            entry_bytes = f.read(48)
            name = entry_bytes[:32].rstrip(b"\x00").decode("ascii")
            atype, offset, size, crc = struct.unpack("<IIII", entry_bytes[32:])
            print(f"    Entry {i+1:2d}: {name:<20} Type={atype} Offset={offset} Size={size} CRC=0x{crc:08X}")
    return True

def main():
    parser = argparse.ArgumentParser(description="Extract Madden NFL '95 SNES ROM assets into madden95.pak")
    parser.add_argument("--rom", "-r", default=None, help="Path to Madden NFL '95 SNES ROM (.sfc/.smc)")
    parser.add_argument("--output", "-o", default="assets/madden95.pak", help="Output asset container path (default: assets/madden95.pak)")
    parser.add_argument("--raw", help="Optional directory to extract individual raw .bin assets")
    parser.add_argument("--verify", "-v", action="store_true", help="Verify an existing asset pack")
    args = parser.parse_args()

    if args.verify:
        sys.exit(0 if verify_pak(args.output) else 1)

    rom = args.rom or find_rom()
    if not rom:
        print("[ERROR] No ROM path specified and no default ROM found.")
        print("Please provide --rom <path_to_rom.sfc>.")
        sys.exit(1)

    success = extract_assets(rom, args.output, args.raw)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
