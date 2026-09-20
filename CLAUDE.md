# AGENTS.md

This file provides guidance to coding agents when working with this repository.

## Project Overview

Custom build of Unreal Engine 3 that loads and saves Batman: Arkham Knight (BM4, 64-bit) packages in the UE3 editor. This branch (`bmgame4`) is a port of the `bmgame2` branch, which targeted Batman: Arkham City (BM2, 32-bit). BM4 packages are all cooked using seekfree cooking, though seekfree loading is always disabled in the editor. All changes should be in the interest of accuracy to the original game binary - we intend to turn this into a full engine reimplementation.
This is a codebase reconstruction project: compatibility changes must be direct, 1:1 ports from the reference decompilations/tools with no invented behavior. Newly-introduced features (not based on BM behavior) have no such restriction.

BM2 support does not need to be preserved on this branch - the two cannot co-exist anyway, since they need different `.uc` class sets. Replace BM2 behavior rather than adding a parallel path for it.

## Build Commands

Build uses UnrealBuildTool. Building autonomously is expected on this branch - trigger builds yourself and iterate on the results. The editor binary is launched with the `editor` argument.

**64-bit is mandatory.** AK uses offset-based property serialization, where serialized offsets correspond to the game's 64-bit in-memory class layout. A 32-bit build cannot match those offsets, so only x64 builds have any chance of loading BM4 content.

## Key Preprocessor Defines

- `BATMAN=1` — always enabled; gates all Batman-specific code paths via `#if BATMAN`

## Decompiled Reference

For BM4 (Arkham Knight) specifically:

- `C:\Users\elitk\Desktop\Batman4` - decompiled UnrealScript for retail AK. Authoritative for `.uc` class layouts, which offset-based serialization requires to be exactly right. Note it is an imperfect decompile: `atomicwhencooked` is not a real keyword, it is a misreading of `immutablewhencooked`, and unresolved default properties appear as `self[0xNNN]=`
- `D:\SteamLibrary\steamapps\common\Batman Arkham Knight\Binaries\Win64\BatmanAK.exe.c` - decompiled retail AK executable (138 MB, no symbols). The ground truth for what the game's own loader actually does
- `I:\UEViewer` - third-party asset viewer with BATMAN4 support; best reference for the BM4 binary format
- `I:\Unreal-Library` - third-party C# package library, also supports BATMAN4
- `D:\SteamLibrary\steamapps\common\Batman Arkham Knight\BmGame\CookedPCConsole (unpacked)\decompress.exe` - decompresses UE3 packages for inspection

These BM2-era decompilations remain useful for general engine behavior, but are Arkham City, not Knight:

- `I:\Gangland\Binaries\Win32\BmGame.exe.c` - decompiled source from a PC build of Arkham City, with partial symbols (has all function names but no structs). Editor-enabled and should be very accurate to the PC version
- `F:\Game Builds\Batman Arkham City (January 20, 2012)\Default.xex.[c,h]` - decompiled X360 source, with full symbols. May not be fully accurate to the PC version, but a good reference for overall behavior and game structs

## Architecture: Batman Customizations

The core work in this repo is making UE3's serialization understand BM4's cooked package format.

**Key pattern — licensee version checks:** The editor saves as the same version retail uses. Serialization changes specific to a licensee version should be guarded on it, preferring `>=` over `==`.

Cooked/non-cooked (`&& Ar.ContainsCookedData()`) can usually be used to test whether a package is editor-made or from retail. Currently used for seekfree structure adaptations, package name remapping, TFC paths, the cooked property tag format, etc.

## Shader Serialization

Do not touch serialization for existing shaders. Shader serialization is known to be fully accurate already through logging - if you think a change is needed, immediately question that mistake.

## Logging

Most log channels are silenced in UE3. Prefer "debugf(NAME_Log, ...)" or "warnf(NAME_Warning, ...)" for logging

BM4 packages are all Ver=863, LicenseeVer=227. Reading the licensee as a stock UE3 high word gives 32995 (0x80E3) = 227 | 0x8000, so the file really does report 32995 - AK changed the version format rather than anyone misreading it. Gate on 227 and preserve the 0x8000 bit for write-back. For reference, BM2 packages were Ver=805, LicenseeVer=101.

**The 0x8000 bit is an Enlighten presence flag, not part of the version.** BM4 carries a fourth version field, the Enlighten version (Geomerics Enlighten GI middleware), current value 9. Retail's summary fixup at `BatmanAK.exe.c:4122095` reads `ArVer` from the low word, `ArLicenseeVer` from the high word masked with `0x7FFF`, and takes the Enlighten version only when the FileVersion dword is negative - i.e. when bit 15 of the licensee word is set. Retail rejects packages whose Enlighten version exceeds its global 9: `"Unable to load package (%s) EnlightenVersion %d, MaxExpected %d."` at `BatmanAK.exe.c:4122217`. FArchive carries it at +20, alongside `ArVer` at +8 and `ArLicenseeVer` at +16.

**The summary tail is four separately-gated fields, not four INTs.** From the retail serializer at `BatmanAK.exe.c:4121878`: an INT gated `licensee >= 87`, an INT gated `licensee >= 175`, the Enlighten version gated on the sign bit, and a `TArray` of 48-byte entries (three FStrings) gated `licensee >= 110`. Everything in the summary before that is bit-for-bit stock UE3.

**FArchive field offsets in the decompile**, for anyone decoding archive accesses: +8 ArVer, +16 ArLicenseeVer, +20 ArEnlightenVer, +24 ArIsLoading, +28 ArIsSaving, +32 ArIsTransacting, +36 ArWantBinaryPropertySerialization, +68 ArContainsCookedData, +80 ArForceByteSwapping, +88/92/96 ArIgnore{Archetype,Outer,Class}Ref, +100 ArAllowEliminatingReferences, +116 ArPortFlags. The package tag is unchanged; grep the decompile for `-1641380927` (0x9E2A83C1).

## Class Layouts

`sizeof(UObject)` is 84 (0x54) and is enforced by a `checkAtCompileTime` in `UnObjBas.h`. Dump the editor's own layouts with `Debug-BmGame.exe DumpClassLayout -unattended` and diff against AK's; see the build/run recipe below.

**Match classes whole, never partially.** Fixing the fields one package happens to exercise leaves the rest silently wrong, and a wrong layout corrupts data without any warning: a cooked bool tag carries an entire bitfield dword, so a misordered bool block applies AK's bits to our flags with no type or offset mismatch to catch it. A class counts as done only when every `self[0xNNN]` entry in AK's `defaultproperties` for that class is accounted for, and a layout report must say which offsets were checked, not just that the checked ones matched.

Beware that a clean package load is weak evidence about layouts. Only offset-only tags reach the offset-addressed path and `ReportLayoutMismatch`; named tags are matched by name and their offset is discarded.

An offset-only tag whose offset hits no property of ours, or a property of a different type, is reported as `[LAYOUT]` and the value is read into a scratch buffer and dropped. That is deliberate: writing retail's value at retail's offset on a class whose layout has drifted corrupts the object. The warnings are the work list.

## Retail Script Packages

Classes the editor lacks are NOT meant to be hand-ported out of the decompile. The retail script packages supply them: copy the game's `Engine.upk` and `BmGame.upk` to `_Engine.upk` and `_BmGame.upk`, load them as startup packages, and their contents merge into the real `Engine`/`BmGame` packages alongside the editor's own compiled `Engine.u`/`BmGame.u`.

The plumbing already exists from the BM2 era. `BmRemapPackageName` (`Core/Src/UnLinker.cpp:3780`) strips a leading underscore, and it is applied at the forced-export, import and export creation sites (`UnLinker.cpp:2533`, `2538`, `3970`, `3982`, `4309`, `4332`). The startup package list lives in `BmGame/Config/DefaultEngine.ini` under `[Engine.StartupPackages]`, which already carries `+Package=_Engine` and `+Package=_BmGame`.

Consequence for the port: getting this merge working for BM4 is a prerequisite for maps, and it is the correct fix for missing classes. Hand-porting a `.uc` class is only right for NATIVE classes, whose C++ layout we must match anyway. Note that AK's `Engine.upk` and `BmGame.upk` have their name/export tables far past `TotalHeaderSize` (NameOffset 72446905 and 356085334); `FArchiveFileReaderWindows::InternalPrecache` clamps the resulting negative length to zero and falls back to on-demand reads, so no precache change is needed. Neither package is compressed, but `Core.upk` is (`PKG_StoreCompressed`).

**The merge rule is "the editor's object wins".** `_Engine`/`_BmGame` carry `PKG_Cooked`, so `CreateExport` looks the export up with `StaticFindObjectFastInternal` in the unprefixed package first; anything already loaded from our own `.u` is reused and never re-serialized, and retail only supplies what we lack. Native classes therefore keep their C++-backed `UClass`, and in exchange every class we also define has to match AK's layout exactly or its retail CDO lands on the wrong members.

**Script serialization, verified byte-for-byte** against every script export of retail `Engine.upk` and `BmGame.upk` (each export consumes exactly its `SerialSize`, each bytecode blob exactly its on-disk and in-memory size):

- `UStruct` drops `ScriptText`, `CppText`, `Line` and `TextPos` outright - not gated on cooked-for-console, which is FALSE for these packages.
- `UClass` adds an empty `TArray<INT>` after `AutoCollapseCategories`, then the BM INT, `ClassGroupNames` and `ClassHeaderFilename`. `UProperty` keeps `Category`/`ArraySizeEnum` and `UFunction` keeps `FriendlyName`.
- `UState`'s `StateFlags` is a WORD (all 1488 class exports of `Engine.upk` parse exactly at WORD, none at DWORD).
- A bytecode name reference is a bare 4-byte name index, in the archive and in the script buffer alike; `FLabelEntry` keeps the full `FName`.
- `EX_NameConst` (0x21) keeps the full `FName`; the index-only form is `EX_NameConstNoNumber` (0x2B). `EX_DynArrayRandomItem` (0x5B) takes one dynamic-array expression and no end token.

## Build and Run

1. `Development/Intermediate/UnrealBuildTool/Release/UnrealBuildTool.exe BmGame Win64 Debug`
2. From `Binaries/Win64`: `./Debug-BmGame.exe make -unattended -auto` (`-auto` is required or header export fails in unattended mode; `-full` rewrites every autogenerated `*Classes.h`)
3. Rebuild C++ if headers changed
4. `./Debug-BmGame.exe DumpClassLayout -unattended`

Any `.uc` change needs `make` re-run before a dump reflects it. Adding a new native `.uc` class needs its `DECLARE_CLASS` block, `StaticClass()` registration and `VERIFY` entry hand-added to the autogenerated header once to bootstrap the first compile.

When porting `.uc` classes from the AK decompile, match AK's property declaration order exactly - offset-based serialization depends on it - but keep this repo's file structure and the stock UE3 comments rather than adopting the decompile's shape.

## Comments

Use "#if BATMAN" for our changes where easy/possible (leaving original code intact), otherwise mark our changes with a "// BM" or "// BM:" comment (always // format on its own line, not a block comment).

Avoid overly descriptive comments except where necessary - the repo isn't your notepad. Don't add full comments unless they're very clearly useful for future work. Do not prefix function names with "Bm"