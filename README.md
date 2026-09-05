# mf-95: Madden NFL '95 C Port & Decompilation

A native C port and decompilation project for **Madden NFL '95**.

---

## ⚠️ Asset & Legal Disclaimer

This repository contains **strictly source code, build scripts, and documentation**. It does **NOT** contain, distribute, or track any copyrighted game assets, ROM dumps, art assets, music, audio samples, or proprietary data tables.

- All game assets are loaded at runtime or build time from an **external asset pack** or extracted locally from a legally owned original game cartridge/ROM.
- Please refer to [AGENTS.md](AGENTS.md) for full details on our strict asset policy.

---

## Architecture & Subsystems

- **PPU (Picture Processing Unit) (`include/mf_ppu.h`, `src/mf_ppu.c`)**:
  - Full SNES Mode-1 compositor (256x224 display resolution).
  - Background layers: BG1 (4bpp field/turf), BG2 (4bpp stadium/stands), BG3 (2bpp score banner & text).
  - 15-bit BGR555 CGRAM palette handling with brightness and color conversion.
  - Sprite (OBJ) engine supporting 128 sprites, 4 priority layers, 8x8 and 16x16 dimensions, and horizontal/vertical flipping.
  - Scanline compositor following standard Mode-1 priority ladders.
  - Deterministic self-test (`mf_ppu_self_test`).

- **Audio Subsystem (`include/mf_audio.h`, `src/mf_audio.c`)**:
  - Multi-channel audio mixer supporting **up to 16 simultaneous voices**.
  - Additive mixing with 32-bit accumulation and 16-bit signed saturation clipping.
  - Per-voice volume, stereo panning, looping, and 16.16 fixed-point resampling for pitch modulation.
  - Optional Win32 `waveOut` audio thread for real-time desktop playback.
  - Built-in RIFF WAV exporter for audio diagnostics.
  - Multi-sound verification self-test (`mf_audio_self_test`).

---

## Building and Running Tests

### Prerequisites
- Windows 10/11 with Visual Studio 2022 (Desktop C++ workload).
- Or CMake 3.20+ with any C11 compatible compiler.

### One-Click Build & Test (PowerShell / Command Prompt)

```powershell
.\build.ps1
```

Or via `cmd.exe`:
```cmd
build-msvc.cmd
```

The script locates the MSVC toolchain, compiles the core engine and tests, and executes `mf95_tests.exe`.

---

## 📦 Asset Extraction & Generation

In accordance with our strict Zero-Asset Policy, copyrighted game assets are not distributed in this repository. Use the automated extraction utility to generate the local asset container from your legally dumped SNES cartridge ROM:

### Generating the Asset Pack

```powershell
python tools/extract_assets.py --rom "F:\Games\SNES\Madden NFL 95 (USA).sfc"
```

*(If `--rom` is omitted, the script automatically scans common local paths for the ROM file).*

The extractor produces **`assets/madden95.pak`** (gitignored), which packages:
- **`table_c8_2c8b`**: Controller button mapping table (42 bytes)
- **`table_c8_2bd0`**: Team field roster & player assignment tables (132 bytes)
- **`title_gfx_chunk1` through `chunk4`**: Bank `$C6/$C7` graphics stream chunks (EA Sports splash, Title stadium art, Madden portrait, NFL font)
- **`title_palette`**: Master CGRAM color palette data
- **`title_music_4a51`**: Title theme audio sequence stream
- **`menu_music_4a37`**: Main menu audio sequence stream

### Verifying an Asset Pack

```powershell
python tools/extract_assets.py --verify --output "assets/madden95.pak"
```
