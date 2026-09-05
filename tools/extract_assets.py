#!/usr/bin/env python3
"""
Madden NFL '95 (SNES) - Asset Extractor & Pack Generator

Extracts verified data from an authentic Madden NFL '95 SNES ROM and
generates an external asset container (madden95.pak).  The EA Sports intro
currently crosses several not-yet-ported C6 object/scheduler routines, so the
extractor also asks Mesen to render that sequence from the user's ROM and packs
the resulting BGR555 frames.  This is a temporary presentation bridge, not a
claim that those dependent routines have already been decompiled.

STRICT POLICY:
This tool generates local external assets only. Under no circumstances
should the resulting .pak or raw extracted files be committed to git.
"""

import argparse
import glob
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

PAK_MAGIC = b"MF95PAK\x00"
PAK_VERSION = 1
EA_STREAM_MAGIC = b"MF95EA1\x00"
EA_STREAM_VERSION = 1
EXPECTED_ROM_SHA256 = "0AD77AE7AF231313E1369A52D1622B88E3751AA5EC774628DF7071F9E4244ABC"
CAPTURE_STEP = 2
CAPTURE_LAST_FRAME = 330

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

def find_mesen(explicit_path=None):
    if explicit_path:
        candidate = os.path.abspath(explicit_path)
        return candidate if os.path.isfile(candidate) else None

    candidate = shutil.which("mesen") or shutil.which("Mesen.exe")
    if candidate:
        return candidate

    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        pattern = os.path.join(
            local_app_data,
            "Microsoft", "WinGet", "Packages", "SourMesen.Mesen2_*", "Mesen.exe"
        )
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[-1]
    return None

def validate_rom(base_data):
    digest = hashlib.sha256(base_data).hexdigest().upper()
    if digest != EXPECTED_ROM_SHA256:
        print("[ERROR] ROM revision does not match Madden NFL '95 (USA).")
        print(f"        Expected SHA-256: {EXPECTED_ROM_SHA256}")
        print(f"        Actual SHA-256:   {digest}")
        return False
    print(f"[*] ROM SHA-256 verified: {digest}")
    return True

def encode_rle_frame(colors):
    """Encode one BGR555 frame as bounded repeat/literal packets."""
    encoded = bytearray()
    pixel_count = len(colors)
    index = 0

    while index < pixel_count:
        repeat = 1
        while (index + repeat < pixel_count and
               colors[index + repeat] == colors[index] and
               repeat < 0x8000):
            repeat += 1

        if repeat >= 3:
            encoded.extend(struct.pack("<HH", 0x8000 | (repeat - 1), colors[index]))
            index += repeat
            continue

        literal_start = index
        index += repeat
        while index < pixel_count and (index - literal_start) < 0x8000:
            next_repeat = 1
            while (index + next_repeat < pixel_count and
                   colors[index + next_repeat] == colors[index] and
                   next_repeat < 3):
                next_repeat += 1
            if next_repeat >= 3:
                break
            index += next_repeat

        literal_count = index - literal_start
        encoded.extend(struct.pack("<H", literal_count - 1))
        encoded.extend(struct.pack(f"<{literal_count}H", *colors[literal_start:index]))

    return bytes(encoded)

def png_to_bgr555(path):
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError(
            "Pillow is required for EA intro extraction (python -m pip install Pillow)."
        ) from exc

    with Image.open(path) as image:
        image = image.convert("RGB")
        if image.size != (256, 224):
            raise RuntimeError(f"Unexpected Mesen capture size {image.size} for {path}")
        colors = []
        for red, green, blue in image.getdata():
            red5 = (red * 31 + 127) // 255
            green5 = (green * 31 + 127) // 255
            blue5 = (blue * 31 + 127) // 255
            colors.append(red5 | (green5 << 5) | (blue5 << 10))
        return colors

def build_ea_intro_stream(frame_paths, capture_step):
    frames = [encode_rle_frame(png_to_bgr555(path)) for path in frame_paths]
    if not frames:
        raise RuntimeError("Mesen produced no EA intro frames")

    # Header: magic, version, dimensions, frame count, ticks per frame,
    # loop start/end, reserved, then frame_count + 1 absolute offsets.
    header_size = 24 + (len(frames) + 1) * 4
    offsets = [header_size]
    for frame in frames:
        offsets.append(offsets[-1] + len(frame))

    header = bytearray(EA_STREAM_MAGIC)
    header.extend(struct.pack(
        "<8H",
        EA_STREAM_VERSION,
        256,
        224,
        len(frames),
        capture_step,
        len(frames) - 1,
        len(frames),
        0,
    ))
    header.extend(struct.pack(f"<{len(offsets)}I", *offsets))
    return bytes(header) + b"".join(frames)

def capture_ea_intro(rom_path, mesen_path):
    script_path = Path(__file__).with_name("capture_ea_intro.lua")
    if not script_path.is_file():
        raise RuntimeError(f"Mesen capture script is missing: {script_path}")

    with tempfile.TemporaryDirectory(prefix="mf95_ea_intro_") as capture_dir:
        # Mesen resolves settings beside a copied executable.  Keeping this
        # private makes extraction deterministic and leaves the user's emulator
        # configuration and save directory untouched.
        runtime_dir = os.path.join(capture_dir, "portable-mesen")
        save_dir = os.path.join(capture_dir, "isolated-saves")
        os.makedirs(runtime_dir)
        os.makedirs(save_dir)
        capture_mesen = os.path.join(runtime_dir, "Mesen.exe")
        shutil.copyfile(mesen_path, capture_mesen)
        settings = {
            "Debug": {"ScriptWindow": {
                "AllowIoOsAccess": True,
                "ScriptTimeout": 60,
                "SaveScriptBeforeRun": False,
            }},
            "Preferences": {
                "SingleInstance": False,
                "PauseWhenInBackground": False,
                "AutoLoadPatches": False,
                "OverrideSaveDataFolder": True,
                "SaveDataFolder": save_dir,
            },
            "Snes": {
                "Port1": {"Type": "SnesController"},
                "Port2": {"Type": "None"},
                "DisableFrameSkipping": True,
                "EnableRandomPowerOnState": False,
                "RamPowerOnState": "AllZeros",
                "ForceFixedResolution": False,
                "Overscan": {"Top": 7, "Bottom": 8, "Left": 0, "Right": 0},
            },
            "Video": {
                "VideoFilter": "None",
                "AspectRatio": "NoStretching",
                "Brightness": 0,
                "Contrast": 0,
                "Hue": 0,
                "Saturation": 0,
                "ScanlineIntensity": 0,
                "UseBilinearInterpolation": False,
                "ScreenRotation": "None",
            },
        }
        with open(os.path.join(runtime_dir, "settings.json"), "w", encoding="utf-8") as settings_file:
            json.dump(settings, settings_file, indent=2)

        env = os.environ.copy()
        env["MF95_CAPTURE_DIR"] = capture_dir.replace("\\", "/")
        env["MF95_CAPTURE_STEP"] = str(CAPTURE_STEP)
        env["MF95_CAPTURE_LAST"] = str(CAPTURE_LAST_FRAME)
        env["MF95_CAPTURE_BOOT"] = "57"

        command = [
            capture_mesen,
            "--testrunner",
            "--timeout=60",
            os.path.abspath(rom_path),
            str(script_path.resolve()),
        ]
        print(f"[*] Capturing original EA Sports sequence with Mesen ({CAPTURE_STEP}-frame sampling)...")
        process = subprocess.Popen(
            command,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        done_path = os.path.join(capture_dir, "capture.done")
        deadline = time.monotonic() + 60.0
        while time.monotonic() < deadline:
            if os.path.isfile(done_path):
                break
            if process.poll() is not None:
                break
            time.sleep(0.05)

        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

        output = process.stdout.read() if process.stdout else ""
        frame_paths = sorted(Path(capture_dir).glob("frame_*.png"))
        expected_count = (CAPTURE_LAST_FRAME // CAPTURE_STEP) + 1
        if not os.path.isfile(done_path) or len(frame_paths) != expected_count:
            if output.strip():
                print(output.strip())
            raise RuntimeError(
                "Mesen capture incomplete: "
                f"script_loaded={os.path.isfile(os.path.join(capture_dir, 'script.loaded'))}, "
                f"expected {expected_count} frames, found {len(frame_paths)}"
            )

        stream = build_ea_intro_stream(frame_paths, CAPTURE_STEP)
        print(f"[+] Captured {len(frame_paths)} authentic frames ({len(stream):,} bytes packed).")
        return stream

def extract_assets(rom_path, output_pak, raw_dir=None, mesen_path=None):
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

    if not validate_rom(base_data):
        return False

    internal_title = base_data[0x00FFC0:0x00FFD5].decode("ascii", errors="replace").strip()
    print(f"[*] Found SNES Internal Title: '{internal_title}'")

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

    # 3. Original EA Sports presentation, rendered by the original code at
    #    $C1:4DE2.  This bridges the unported C6 object/scheduler dependency.
    mesen = find_mesen(mesen_path)
    if not mesen:
        print("[ERROR] Mesen 2 was not found. Install it or pass --mesen <Mesen.exe>.")
        return False
    try:
        ea_intro = capture_ea_intro(rom_path, mesen)
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(f"[ERROR] Could not capture EA intro: {exc}")
        return False
    assets.append({
        "name": "ea_intro_frames",
        "type": TYPE_GRAPHICS,
        "data": ea_intro,
        "desc": "ROM-rendered $C1:4DE2 EA Sports presentation (BGR555 RLE)"
    })

    # 4. Main Menu UI Palette ($CA:FB10, 32 bytes)
    menu_ui_pal = base_data[0x0AFB10:0x0AFB10 + 32]
    assets.append({
        "name": "menu_palette_ui",
        "type": TYPE_PALETTE,
        "data": menu_ui_pal,
        "desc": "Main Menu UI Color Palette ($CA:FB10, 32 bytes)"
    })

    # 5. Main Menu Backdrop Gradient Palette ($C9:D530, 32 bytes)
    menu_grad_pal = base_data[0x09D530:0x09D530 + 32]
    assets.append({
        "name": "menu_palette_gradient",
        "type": TYPE_PALETTE,
        "data": menu_grad_pal,
        "desc": "Main Menu Backdrop Gradient Color Palette ($C9:D530, 32 bytes)"
    })

    # 6. Main Menu Selection Highlight Palette ($C7:E6B9, 32 bytes)
    menu_hl_pal = base_data[0x07E6B9:0x07E6B9 + 32]
    assets.append({
        "name": "menu_palette_highlight",
        "type": TYPE_PALETTE,
        "data": menu_hl_pal,
        "desc": "Main Menu Selection Highlight Color Palette ($C7:E6B9, 32 bytes)"
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
    parser.add_argument("--mesen", help="Optional path to Mesen.exe for original intro capture")
    parser.add_argument("--verify", "-v", action="store_true", help="Verify an existing asset pack")
    args = parser.parse_args()

    if args.verify:
        sys.exit(0 if verify_pak(args.output) else 1)

    rom = args.rom or find_rom()
    if not rom:
        print("[ERROR] No ROM path specified and no default ROM found.")
        print("Please provide --rom <path_to_rom.sfc>.")
        sys.exit(1)

    success = extract_assets(rom, args.output, args.raw, args.mesen)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
