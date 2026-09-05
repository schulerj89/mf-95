# AGENTS.md — Guidelines for Working in mf-95 (Madden NFL '95 C Port / Decompilation)

This repository contains the C port and decompilation project for **Madden NFL '95** (`mf-95`).

---

## ⚠️ ZERO ASSET POLICY (CRITICAL — NEVER COMMIT ASSETS)

**UNDER NO CIRCUMSTANCES SHOULD ANY COPYRIGHTED ASSETS BE COMMITTED TO THIS REPOSITORY.**

1. **Strictly Code and Documentation Only**:
   - The repository tracks ONLY source code (`.c`, `.h`, `.cpp`, `.hpp`), build scripts (`CMakeLists.txt`, `.cmd`, `.ps1`), configuration files, and documentation (`.md`, `.txt`).
   - **NO** ROM dumps (`.md`, `.bin`, `.gen`, `.sfc`, `.smc`, `.iso`).
   - **NO** extracted audio samples, commentary clips, or sound effects (`.wav`, `.mp3`, `.ogg`, `.pcm`, `.spc`).
   - **NO** extracted sprite tiles, background graphics, font sheets, or palettes (`.bmp`, `.tga`, `.png`, `.chr`, `.raw`).
   - **NO** binary asset packs or data containers (`.pak`, `.dat`, `.wad`).

2. **External Asset Packs & User ROMs**:
   - All game assets (textures, audio, palettes, rosters, playbooks, stadium graphics) are loaded dynamically from an external **asset pack** or extracted locally from a user-supplied original ROM.
   - Any local asset staging directories (e.g., `assets/`, `asset_pack/`, `dist/assets/`, `dumps/`) are strictly ignored in `.gitignore`.
   - Any tools designed to extract assets (e.g., in `tools/`) must write their output to gitignored paths and must never stage asset artifacts into git.

3. **Pre-Commit Verification**:
   - Before executing `git commit` or `git push`, agents and contributors **MUST** run `git status` to verify that no binary files or asset directories are staged.
   - If an asset file is accidentally tracked, immediately unstage and remove it from git tracking (`git rm --cached <file>`) and update `.gitignore`.

---

## Standardized Function Comment Format

Every converted or scaffolded subroutine in C **must** include the following comment header:

```c
/*
 * Subroutine: <function_name>
 * Bank:       $<bank_number>
 * Address:    $<bank_number>:<snes_address>
 * File Offset: 0x<hex_file_offset>
 * Description: <Concise explanation of the routine's purpose, hardware registers,
 *              memory state modified, and algorithmic behavior>
 */
```

### Formatting Rules:
- **Include**: Subroutine name, Bank, Address, File Offset (6-digit hex format, e.g. `0x010000`), and a clear functional description.
- **DO NOT Include**: Raw opcode bytes (e.g., `9C 00 42`) or assembly instruction lines (e.g., `STZ $4200`) in comments.
- **Conversion Cadence**: Convert and verify **one subroutine at a time**, locking each stage in with unit tests, commits, and pushes before advancing to the next.

---

## Architectural Principles

1. **Lightweight Main Game Loop**:
   - Keep the top-level game loop and state dispatcher extremely lightweight.
   - Subsystem modules and include headers (`mf_ppu.h`, `mf_audio.h`, `mf_system.h`) do the heavy lifting.
   - Separate hardware abstraction, memory clearing, register state management, and simulation logic into clear modular components.

2. **Deterministic & Verifiable**:
   - Verify logic against machine-derived facts from the original disassemblies and memory maps.
   - Maintain unit tests in `tests/` covering state changes, registers, and return paths for every converted routine.

3. **Build System**:
   - MSVC / CMake build pipelines. Run `build.ps1` or `build-msvc.cmd` to compile and verify all tests before committing.

---

## Asset Extraction & Packaging Pipeline

To comply with the Zero-Asset Policy while maintaining full runtime asset fidelity, game assets are extracted locally from a user-supplied ROM into an asset pack:

1. **Extraction Tool**:
   - `tools/extract_assets.py` parses the original SNES ROM and extracts graphics streams, palettes, audio music sequences, and data tables.
   - Command:
     ```powershell
     python tools/extract_assets.py --rom "path/to/Madden NFL 95 (USA).sfc" -o assets/madden95.pak
     ```

2. **Asset Container Format (`madden95.pak`)**:
   - Header:
     - `magic[8]`: `"MF95PAK\0"`
     - `version`: uint32 (currently `1`)
     - `entry_count`: uint32 (number of contained assets)
     - `flags`: uint32 (reserved, currently `0`)
   - Table of Entries (immediately follows header):
     - `name[32]`: ASCII null-terminated asset identifier (e.g. `title_gfx_chunk1`, `title_palette`)
     - `type`: uint32 (`1` = GFX, `2` = AUDIO, `3` = PALETTE, `4` = DATA)
     - `offset`: uint32 (absolute byte offset in package)
     - `size`: uint32 (uncompressed byte size)
     - `crc32`: uint32 (integrity verification)
   - Payload data chunks follow entry table.

3. **C Runtime Asset Loader**:
   - `include/mf_assets.h` and `src/mf_assets.c` provide the runtime API (`mf_assets_load`, `mf_assets_find`, `mf_assets_free`, `mf_assets_self_test`).
   - Integrated into game initialization in `src/mf_game.c` (`mf_game_init` loads `assets/madden95.pak` if present).
   - If the asset pack is absent, the engine logs a clear diagnostic and falls back gracefully.

