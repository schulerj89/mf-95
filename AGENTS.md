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

## Architecture & Project Objectives

- **Primary Goal**: High-fidelity, modular C port and decompilation of Madden NFL '95.
- **Engine Components**:
  - **Core Logic & Simulation**: Football physics, player locomotion, AI decision tree, play execution, collision detection, and referee rules.
  - **Hardware Abstraction Layer (HAL)**: Clean separation between platform rendering (SDL2 / Direct3D / OpenGL), audio synthesis/playback, and input controllers.
  - **State Representation**: Explicit data structures for team rosters, player attributes, formations, playbooks, and game clock state.
  - **Verification / Oracle**: Comparison against original cycle/state behavior for byte-accurate verification where possible.

---

## Tooling & Verification Guidelines

1. **Deterministic & Verifiable**:
   - Verify logic against machine-derived facts from the original disassemblies/memory maps rather than guessing constants or magic numbers.
   - Document any reverse-engineered routines with comments describing the original function address, register usage, and algorithmic intent.

2. **Asset Extraction Tools**:
   - Any scripts that unpack or decode graphics/audio from the ROM or asset packs belong under `tools/`.
   - Always verify that tool outputs go to gitignored folders (such as `assets/` or `dist/assets/`).

3. **Build System**:
   - Standard MSVC / CMake / Ninja build pipelines.
   - Dev builds should support debug telemetry and memory inspection.
