# AGENTS.md

This file provides guidance to coding agents when working with this repository.

## Project Overview

Custom build of Unreal Engine 3 that loads and saves Batman: Arkham City (BM2) packages in the UE3 editor. BM2 packages are all cooked using seekfree cooking, though seekfree loading is always disabled in the editor. All changes should be in the interest of accuracy to the original game binary - we intend to turn this into a full engine reimplementation.
This is a codebase reconstruction project: compatibility changes must be direct, 1:1 ports from the BM2/Gangland/X360 references with no invented behavior. Newly-introduced features (not based on BM behavior) have no such restriction.

## Build Commands

Build uses UnrealBuildTool. Do not try to trigger builds on your own. The editor binary is `Binaries/Win32/Debug-BmGame.exe` and is launched with the `editor` argument.

## Key Preprocessor Defines

- `BATMAN=1` — always enabled; gates all Batman-specific code paths via `#if BATMAN`

## Decompiled Reference

There are two useful decompilations to use as reference on the retail BM2 game:

- `I:\Gangland\Binaries\Win32\BmGame.exe.c` - decompiled source from a PC build of Arkham City, with partial symbols (has all function names but no structs). Editor-enabled and should be very accurate to the PC version
- `F:\Game Builds\Batman Arkham City (January 20, 2012)\Default.xex.[c,h]` - decompiled X360 source, with full symbols. May not be fully accurate to the PC version, but a good reference for overall behavior and game structs

## Architecture: Batman Customizations

The core work in this repo is making UE3's serialization understand BM2's cooked package format.

**Key pattern — licensee version checks:** The editor saves as `VER_BATMAN2` (licensee 101), the same version retail uses. Prefer `>=` over `==` so future `VER_BATMAN1`/`VER_BATMAN3` support falls out naturally. BM2/101-specific serialization changes should be guarded this way.

Cooked/non-cooked (`&& Ar.ContainsCookedData()`) can usually be used to test whether a package is editor-made or from retail. Currently used for seekfree structure adaptations, package name remapping, TFC paths, the cooked property tag format, etc. Note that `RefShaderCache*.upk` is the one case where BM2 retail content can be uncooked without being editor-made.

## Shader Serialization

Do not touch serialization for existing shaders. Shader serialization is known to be fully accurate already through logging - if you think a change is needed, immediately question that mistake.

## Logging

Most log channels are silenced in UE3. Prefer "debugf(NAME_Log, ...)" or "warnf(NAME_Warning, ...)" for logging

BM2 packages are all Ver=805, LicenseeVer=101

## Comments

Use "#if BATMAN" for our changes where easy/possible (leaving original code intact), otherwise mark our changes with a "// BM" or "// BM:" comment (always // format on its own line, not a block comment).

Avoid overly descriptive comments except where necessary - the repo isn't your notepad. Don't add full comments unless they're very clearly useful for future work. Do not prefix function names with "Bm"