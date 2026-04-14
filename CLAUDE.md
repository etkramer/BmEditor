# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Project Overview

Custom build of Unreal Engine 3 that works with retail PC Batman: Arkham Origins (BM3) packages. BM3 packages are all cooked using seekfree cooking, though seekfree loading is always disabled in the editor. In general, our project's goal is to become as close to the original game's engine (a UE3 fork called "RockEngine") as possible.

## Decompiled Reference

There are two useful decompilations to use as reference on the retail BM3 game:

- `I:\Gangland\Binaries\Win32\BmGame.exe.c` - decompiled source from a PC build of Arkham City, with partial symbols (has all function names). Should be very accurate to the PC version and a strong reference
- `F:\Game Builds\Batman Arkham Origins (February 1, 2013)\Default.xex.[c,h]` - decompiled X360 source, with full symbols. May not be fully accurate to the PC version, but a good reference for overall behavior and game structs
- `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Origins\SinglePlayer\Binaries\Win32\BatmanOrigins.exe.c` - decompiled PC source, without symbols. This is the most accurate source, though it may be difficult to read without using X360 as reference. Probably only useful for finding specific exe offsets

## Logging

Always use the format warnf(NAME_Warning, ...) for debug logging. Most other log channels, including debugf, do not display in our debug console.

## Architecture: Batman Customizations

The core work in this repo is making UE3's serialization understand BM3's cooked package format. There are ~156 `#if BATMAN` blocks across ~48 files.

**Key pattern — `IsBmCooked(BOOL IncludeEditor)`:** Defined in `Core/Inc/UnArc.h`. When "TRUE" is passed, this returns whether the package is from the BM3 game OR whether it's a BM3-format package made by the editor. It should be "TRUE" for nearly all serialization cases. When "FALSE" is passed, it returns true only if the package is from the BM3 game, causing editor-made packages to behave differently.

Don't build the engine yourself, the user can do that.
