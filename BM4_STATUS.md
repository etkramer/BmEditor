# BM4 Port Status

Where the `bmgame4` branch stands, what is known-wrong, and what to do next. Read `CLAUDE.md` first for the rules and the format reference; this file is the state and the work list.

## What works

Retail Arkham Knight content loads in the editor. Verified by loading, not asserted:

- **Package format** - Ver 863 / licensee 227 (masked from `0x80E3`), the Enlighten version, the summary tail, 64-bit bulk offsets, zlib and LZO, TFC paths.
- **Script packages** - AK's `UClass`/`UStruct`/`UState`/`UFunction` and bytecode formats are byte-proven: every export and bytecode blob in `_Engine.upk` and `_BmGame.upk` parses exactly. Both packages load.
- **Assets** - textures, materials and parameter expressions, static and skeletal meshes, animation, BSP and level geometry, navigation meshes, Umbra occlusion, Wwise banks.
- **Maps** - `Clocktower`, `Clocktower_C1`, `JokerBoss`, `JokerBoss_C1`, `JokerBoss_B1` all deserialize to completion.

The editor builds and runs as x64 (`UnrealBuildTool.exe BmGame Win64 Debug`) and reaches full engine init.

## Honest measurements

One package per process, since the diagnostic counters only mean anything that way:

```
Clocktower       10955 exports   0 uncreated   291 diagnostics
Clocktower_C1     3961 exports   0 uncreated   293 diagnostics
JokerBoss         9075 exports   0 uncreated   288 diagnostics
JokerBoss_C1      4994 exports   3 uncreated   199 diagnostics
JokerBoss_B1      7087 exports   3 uncreated   285 diagnostics
<startup merge>                               6307 diagnostics
```

Reproduce with:

```
cd Binaries/Win64
./Debug-BmGame.exe CheckPackageLoad <package> -startup -unattended -forcelogflush -abslog=<path>
```

`-startup` is required; without it no BmGame classes exist. Each run prints a `[PKGSTAT]` line per package with exports, uncreated exports and a 12-way diagnostic breakdown.

**Deserializing is not the same as loading correctly.** Offset-only tag acceptance checks the property class, the struct for Vector/Rotator, and alignment - nothing else. Drift that lands an Int tag on a different Int, or a bool dword on a different bool dword, is accepted and written with no warning.

## Known silent corruption

These produce no diagnostic and will not show up in any count:

- **`ActorComponent` bool bits** - our `bLevelHidden` occupies the bit AK assigns to `bNeedsFinalInit` (index 7 of the dword at offset 116). Sizes and offsets match, so nothing warns, but a cooked bool tag carries the whole dword, so every component in every map takes AK's bit into the wrong member.
- **`RequiredBones` truncation** - `Engine/Src/UnSkeletalMesh.cpp:1295` truncates AK's INT bone indices into a BYTE array, which *aliases* rather than drops: bone 256 becomes bone 0. `Orphan.upk` hits it 231 times in one load.

## Work list

Ranked by impact on real map content, not by warning count. A detector exists for this: compare retail's `UProperty` export order from `_Engine.upk`/`_BmGame.upk` against `DumpClassLayout` output. It currently reports **255 diverged classes**.

1. **`LightComponent`** - wrong from its first property (76 retail properties against our 65; `GelLayer`/`CubeTexture`/`Layer0Multiplier`/`Layer1Multiplier`/`Layer0Layer1Multiplier` all precede `SceneInfo`). Base of every light in every map. Highest value remaining.
2. **`ActorComponent` bit order** and **`RequiredBones`** - the silent corruption above.
3. **`WorldInfo`** - 183 retail properties against our 146, diverging at index 2. Present in every map.
4. **`AnimNode`** - missing `NodeEndEventTick`, an INT that shifts the whole animation tree by 4 bytes.
5. **`AnimSet`** (40 vs 21) and **`PhysicalMaterial`** (65 vs 48) - both diverge at their first property.
6. **`SVehicleWheel`** (80 vs 43), **`ParticleModuleRequired`** (45 vs 33), `RVehicleTank`, `RVehicleCar`, `StaticMesh`, `PhysicsAsset`, `SkeletalMeshComponent`.
7. **`PenguinCache_B3` and `Hideout_B2` segfault** - exit 139, no `appError`, no callstack, no `[PKGSTAT]`. A different failure class from the assertions fixed so far, and uninvestigated.
8. **Cross-map `BmScript` collisions** - every map's embedded script classes share one global `BmScript` namespace, so export creation depends on load order. Currently reported rather than resolved.
9. **Coverage** - 12 of 1159 map packages have ever been measured. `Orphan.upk` reports 838 diagnostics, more than double anything in the target set, so the target maps are not representative.

## Caveats

- **Nothing on this branch has ever saved a package.** Every format change made here has an untested save side: packed navmesh polys, the kDOP block, `PylonBuildID`, the Umbra serializers, AkBank bulk write-back, `FVert`, `VER_LATEST_NAVMESH` 42 -> 46. A single save-and-reload round trip would test a lot at once.
- **Materials cannot be compiled and rendering is unported.** Expressions are stripped from cooked content; the BM2 approach of loading retail shader caches is deliberately deferred. Expect materials not to render correctly.
- **Standing workarounds** are listed in `CLAUDE.md`. Each is a disclosed compromise, not a solved problem.
- A spurious `implements(...)` on a class adds a `VfTable` property and shifts its whole subtree by 8 bytes. This has been found three times (`Pawn`, `Controller`, `Pylon`); assume more exist.

## Tooling

- `Debug-BmGame.exe CheckPackageLoad <pkgs> -startup -unattended` - load check with `[PKGSTAT]` output.
- `Debug-BmGame.exe DumpClassLayout -unattended` - every class's property offsets, to `BmGame/Logs/ClassLayout.txt`. Stable and diffable.
- A retail CDO's cooked tag stream is a free exact offset oracle for its class; a subclass's first property pins the superclass's size. Using both removes all slack from a hand-computed layout.
- Scratch Python tools were used throughout for parsing uncompressed packages and decoding property keywords from retail flags. They live outside the repo and are straightforward to rebuild from the notes in `CLAUDE.md`.
