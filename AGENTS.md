# AGENTS.md

This file provides guidance to coding agents when working with this repository.

## Project Overview

Custom build of Unreal Engine 3 that loads and saves Batman: Arkham City (BM2) packages in the UE3 editor. BM2 packages are all cooked using seekfree cooking, though seekfree loading is always disabled in the editor. All changes should be in the interest of accuracy to the original game binary - we intend to turn this into a full engine reimplementation.

## Build Commands

Build uses UnrealBuildTool. Do not try to trigger builds on your own. The editor binary is `Binaries/Win32/Debug-BmGame.exe` and is launched with the `editor` argument.

## Key Preprocessor Defines

- `BATMAN=1` — always enabled; gates all Batman-specific code paths via `#if BATMAN`
- `GAMENAME=BMGAME`, `IS_BMGAME=1` — game identification
- These are set in `Development/Src/UnrealBuildTool/Configuration/UE3BuildBmGame.cs`

## Decompiled Reference

There are two useful decompilations to use as reference on the retail BM2 game:

- `I:\Gangland\Binaries\Win32\BmGame.exe.c` - decompiled source from a PC build of Arkham City, with partial symbols (has all function names but no structs). Should be very accurate to the PC version and a strong reference
- `F:\Game Builds\Batman Arkham City (January 20, 2012)\Default.xex.[c,h]` - decompiled X360 source, with full symbols. May not be fully accurate to the PC version, but a good reference for overall behavior and game structs

## Architecture: Batman Customizations

The core work in this repo is making UE3's serialization understand BM2's cooked package format.

**Key pattern — `IsBmCooked(BOOL IncludeEditor)`:** Defined in `Core/Inc/UnArc.h`. When "TRUE" is passed, this returns whether the package is from the BM2 game OR whether it's a BM2-format package made by the editor. It should be "TRUE" for nearly all serialization cases. When "FALSE" is passed, it returns true only if the package is from the BM2 game, causing editor-made packages to behave differently.

## Logging

Most log channels are silenced in UE3. Prefer "debugf(NAME_Log, ...)" or "warnf(NAME_Warning, ...)" for logging

## Subagents

In major investigations - use subagents where possible, instead of performing research in-context. Context compaction is a huge setback that can be avoided through careful use of subagents - try to act as a manager (of subagents) rather than a worker

BM2 packages are all Ver=805, LicenseeVer=101

Use "#if BATMAN" for our changes where it makes sense, otherwise mark our changes with a "// BM" comment (always // format, not a block comment). "// BM" itself works, so does "// BM: Short comment"