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
- Mesen 2 and Python Pillow for generating the local EA intro presentation stream.

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

The extractor verifies the exact USA ROM SHA-256 before producing
**`assets/madden95.pak`** (gitignored), which packages:
- **`table_c8_2c8b`**: Controller button mapping table (42 bytes)
- **`table_c8_2bd0`**: Team field roster & player assignment tables (132 bytes)
- **`ea_intro_frames`**: BGR555/RLE presentation frames rendered from the original
  `$C1:4DE2` path by Mesen. This is a visible bridge across the not-yet-converted
  C6 object loader and scheduled animation tasks; it is not stored in Git.
- **`menu_palette_ui`**, **`menu_palette_gradient`**, and
  **`menu_palette_highlight`**: verified menu palette data

The earlier extractor labels for `$C1:01A6`, `$C1:1A18`, and `$C1:5467` were
removed because those offsets are executable routines, not raw palette or audio
streams.

The desktop build displays the unobstructed 256×224 game image by default. Press
`F1` to toggle the diagnostic overlay and `Enter` for Start.

### Verifying an Asset Pack

```powershell
python tools/extract_assets.py --verify --output "assets/madden95.pak"
```
