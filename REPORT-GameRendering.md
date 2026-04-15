# REPORT-GameRendering

Comparison of our custom UE3 build against the decompiled Batman: Arkham City binary at `I:\Gangland\Binaries\Win32\BmGame.exe.c` (symbolised, authoritative for BM3/RockEngine semantics since BM3 is the same engine fork one game later). Scope: every function or behaviour on the shader/material/rendering path that meaningfully differs, focused on shader inputs, parameter bindings, and USF declarations. Line numbers on our side refer to the current `bmgame3-shadercache` branch; offsets on the Gangland side are IDA `*(this + N)` field offsets in the decompilation.

---

## Executive summary — the six most load-bearing bugs

Ordered by estimated impact on broken shader binding:

1. **`ShaderManager.cpp:824-860` + `ShaderCache.cpp:669-794` + `ShaderCache.cpp:1450-1475` describe a wire format that does not exist.** BM:AC's `FShader::Serialize`, `SerializeShaders`, and `UShaderCache::Load` are byte-for-byte stock UE3. There is no per-shader `(FGuid,BytecodeSize,bytes)` side-list and no two-section material-shader-map layout. Every BM3-cooked shader we load through the BATMAN branch is silently corrupt: empty `Key.Code`, wrong `ParameterMapCRC`, wrong `Id`, wrong `Hash`. This alone breaks shader binding globally.
2. **`operator<<(FArchive&, FMaterialUniformExpression*&)` at `MaterialShared.cpp:500-509` leaves `Ref = NULL` on unknown expression type names without skipping the payload.** Any BM3-specific uniform-expression class desynchronises the archive cursor for the rest of the stream. Random subsequent bindings read garbage.
3. **`FUniformExpressionSet::Serialize` wire format depends on `WITH_D3D11_TESSELLATION=1`** (`MaterialShared.cpp:613-627`). Gangland serialises 13 TArrays (Pixel×3, Cube×1, Vertex×3, Hull×3, Domain×3). Our fallback path (hull/domain disabled) writes only 7, so the vertex/hull/domain expressions silently fall into pixel slots and the rest of the stream shifts.
4. **Wrong pixel-shader parameter names in `TBasePassPixelShaderBaseType` / `FMaterialPixelShaderParameters` / `FAPlus3DLightLightMapPolicy` / `FLightMapTexturePolicy`.** Concrete cases: `DecalNearFarPlaneDistance` vs `DecalFarPlaneDistance`; `DOFPackedParameters0/1` vs the compound `FDOFShaderParameters("PackedParameters","MinMaxBlurClamp")`; `APlus3DLightInfoPixel`/`-Vertex` vs `APlus3DLightPixelInfo`/`-VertexInfo`; `LightDirection` vs `LightDirectionAndbDirectional`; `LightMapLumaChannel` does not exist. Each makes `.IsBound()` return FALSE and each `SetPixelShaderValue` becomes a silent no-op.
5. **`TBasePassPixelShaderBaseType::Serialize` BATMAN branch skips `UpperSkyColor` and `LowerSkyColor`.** Gangland's pixel-shader serialize order is `MaterialParameters → AmbientColorAndSkyFactor → UpperSkyColor → LowerSkyColor → MotionBlurMask`. Ours skips the two sky colors, so `MotionBlurMask` ends up reading the sky-color bytes as its own `FShaderParameter`. This is what the in-code comment at `BasePassRendering.h:313-320` is papering over with a forced `AmbientColor=(0,0,0,1)`.
6. **Trailing-dummy `FShaderParameter` reads/writes in `TBasePassVertexShader::Serialize`, `TDepthOnlyVertexShader<TRUE>::Serialize`, `TFogIntegralVertexShader::Serialize`, `FFogVolumeApplyVertexShader::Serialize`, `TLightVertexShader::Serialize`.** Gangland writes none of these. Each one silently over-reads 12 bytes into the next shader in the stream, so every shader after the first one in a given shader-type list gets deserialised from the wrong offset.

All six of these are independent causes of "shaders load but draw white / black / garbage" — the class of symptom the user is debugging. Any *one* of them is enough to break the pipeline; all six are live simultaneously.

---

## ShaderManager / ShaderCache

Primary finding: **Arkham City's on-disk `FShader` / `SerializeShaders` / `FShaderCache` wire format is byte-for-byte identical to stock UE3.** There is no BM3-specific `(FGuid, bytecode)` layout, no separate bytecode map, and no two-section MaterialShaderMap layout. The majority of our `#if BATMAN` patches in `ShaderManager.cpp` and `ShaderCache.cpp` describe a format that does not exist in BM:AC and therefore corrupt the load whenever `IsBmCooked()` fires. Secondary finding: BM3's `sizeof(FShaderParameter)` appears to be **24 bytes**, significantly larger than stock — does not affect wire format but may affect any code that iterates a `TArray<FShaderParameter>` by offset.

Reference locations in `BmGame.exe.c`:
- `FShader::Serialize` — 5362075
- `SerializeShaders` — 5372018
- `FShaderCache::Load` — 5376306
- `FShaderCache::Save` — 5373824
- `UShaderCache::Load` material-shader-map loop — 5376508–5376676
- `FShaderType::GetOutdatedTypes` — 5361852
- `FForwardShadowingShaderParameters::Bind` (used to infer `sizeof(FShaderParameter)`) — 4759399

### Major diffs

- **`FShader::Serialize` BM3 branch reads a hallucinated format.** Ours `ShaderManager.cpp:824-860` — under `IsBmCooked()` reads `FGuid BytecodeGuid + ParameterMapCRC + Id + FName + FSHAHash + NumInstructions` (70 bytes) and looks up bytecode in `GBmShaderBytecodeMap`. Gangland at `BmGame.exe.c:5362087-5362150` does: 2-byte Target pair, `TArray<BYTE> Code` inline, `ParameterMapCRC(4)`, `FGuid Id(16)`, `FShaderType*` as FName, `FSHAHash(20)`, `RegisterShader`, `NumInstructions(4)` — identical to stock UE3. Consequence: every shader loaded through our BATMAN branch has empty `Key.Code`, wrong `ParameterMapCRC` (ParameterMap lookups hash-miss), wrong `Id` (`FindShaderById` never returns it), and an uninitialised `Hash`. `RHICreatePixelShader`/`RHICreateVertexShader` creates a null/junk resource.
- **`SerializeShaders` BM3 branch invents a `(FGuid, INT, BYTE[])` per-shader format.** Ours `ShaderCache.cpp:1450-1472` reads a flat list and stuffs bytes into `GBmShaderBytecodeMap`, skipping every per-shader header field. Gangland at `BmGame.exe.c:5372184-5372314` reads per shader: `FName ShaderType`, `FGuid ShaderId(16)`, `FSHAHash SavedHash` (gated `Ver>=796`), `INT SkipOffset(4)`, `TArray<WORD> Serializations`, then wraps the inner archive with `FShaderLoadArchive` and calls the virtual `Shader->Serialize`. That virtual call is what actually deserializes every per-shader parameter Bind struct. We **never call it**, so every `FShaderParameter`/`FShaderResourceParameter` inside a `FShader` subclass is uninitialised. That alone is enough to break shader input binding globally.
- **`UShaderCache::Load` invents a two-section MaterialShaderMap layout.** Ours `ShaderCache.cpp:669-794` reads `NumMaterialShaderMaps` per-shader entries as "Section 1", then a separate `NumMaterialShaderMaps2` section of `FStaticParameterSet`-keyed maps. Gangland `BmGame.exe.c:5376508` onward has exactly **one** section: after `SerializeShaders` completes, `FShaderCache::Load` reads `INT NumMaterialShaderMaps`, then loops over `{FStaticParameterSet::Serialize; [INT,INT] (ShaderMapVersion/LicenseeVersion if Ver>=660); SkipOffset; FMaterialShaderMap::Serialize}`. There is no second-section count. We consume the single shader count INT as if it were a bogus "section 1 shader count", iterate reading structured records on top of what is actually `FStaticParameterSet` bytes, and then read a "section 2 count" from the middle of a material shader map. SkipOffset bounds checks trip immediately, aborting cache load — matches the `Invalid Section1 SkipOffset` / `Invalid MaterialShaderMap2 SkipOffset` warnings we see.
- **`FShaderCache::Load` BM3 compressed-cache bypass is dead code but harmless.** `ShaderCache.cpp:313-319` reads `INT BmDummy` under `IsBmCooked()`. Gangland at `BmGame.exe.c:5376337-5376341` does `if(Ver>=672){ ByteOrderSerialize(&zero,4); }`. Stock UE3 on PC serialises a `FCompressedShaderCodeCache` TMap whose empty form is exactly 4 bytes of zero — functionally equivalent. Dead code, could be deleted.
- **`FShaderCache::Save` in Gangland has no compressed-cache write at all.** `BmGame.exe.c:5373824-5373853` writes `Platform(1) + zero(4) + SerializeShaders`. Cook-time only; not a load-time issue but relevant if we ever recook from this tree.

### Lesser diffs / risk items

- **`FShader::Serialize` `bDiscardShaderSource` platform check is asymmetric.** Ours `ShaderManager.cpp:812-817` also checks `Target.Platform == SP_PCD3D_SM3`; Gangland `BmGame.exe.c:5362096-5362105` checks only `GCookingTarget & 0x280`. Cook-time only.
- **`GetOutdatedTypes` matches.** Ours `ShaderManager.cpp:378-413` against Gangland `BmGame.exe.c:5361852-5361931`. Both compare saved `Hash` against `Type->GetSourceHash()` by `appMemcmp` 20 bytes at byte offset 84. Byte layouts confirm our `FShader` storage agrees with BM3's for every field that ends up on disk.
- **`sizeof(FShaderParameter) == 24` in BM3.** Evidence: `FForwardShadowingShaderParameters::Bind` at `BmGame.exe.c:4759399-4759407` calls `FShaderParameter::Bind(this + 0/6/12/18, …)` and `FShaderResourceParameter::Bind(this + 24, …)` — stride of 6 DWORDs = 24 bytes per `FShaderParameter`. Ours is 6 bytes (D3D10+) or 4 bytes. This does not affect the on-disk format (BM3 still serialises the same 6 bytes of `BaseIndex/NumBytes/BufferIndex`), but any code that computes parameter offsets into a `TArray<TUniformParameter<FShaderParameter>>` by stride is off by ~4×.
- **Version-gate cutoffs.** Gangland uses literal `796` / `803` / `805` for `VER_FIXED_AUTO_SHADER_VERSIONING`, `VER_UNIFORMEXPRESSION_TEXTUREINDEX`, `VER_SHADER_CACHE_PRIORITY` — these are BM:AC-numbered. Our constants are numerically smaller. For a BM3 cooked package reporting `Ar.Ver()` ≈ 805+, both sides take the "post-cutoff" branch, so behaviour converges — no current bug, but it is a trap for the day somebody needs to load an older cache.

### Recommended fixes (not performed — read-only review)

1. Delete the three BATMAN blocks in `ShaderManager.cpp:824-860`, `ShaderCache.cpp:669-794`, and `ShaderCache.cpp:1450-1475`. The stock paths already describe BM3's exact wire format.
2. Delete the dead-code BATMAN block in `ShaderCache.cpp:313-319`.
3. Delete `GBmShaderBytecodeMap` entirely once the above are gone.
4. Investigate BM3's `FShaderParameter` extra fields (24-byte version) — likely an `FName Name` + `WORD SamplerIndex` — and mirror them, or verify nothing indexes by stride.
5. Leave the `BmShaderInit` diagnostic warnf at `ShaderManager.cpp:959-964`; purely informational.

---

## Material Shader Pipeline

### Pixel-shader parameter Bind list — WRONG PARAMETER NAME

- **`FMaterialPixelShaderParameters::Bind`** at `MaterialShader.cpp:665`. Ours: `DecalNearFarPlaneDistanceParameter.Bind(ParameterMap, TEXT("DecalNearFarPlaneDistance"), TRUE);`. Gangland `BmGame.exe.c:6972783`: `FShaderParameter::Bind(this+164, a2, L"DecalFarPlaneDistance", 1u)`. The BM3/RockEngine USF declares `float4 DecalFarPlaneDistance`; our `Bind()` looks up a name that does not exist in the shader, `IsBound()` stays FALSE, every subsequent `SetPixelShaderValue(..., DecalNearFarPlaneDistanceParameter, ...)` in `SetMesh` (line 871) silently no-ops. Concrete fix: rename the string literal to `"DecalFarPlaneDistance"`. Field name and serialization can stay as-is.

### Vertex-shader parameter Bind list — MISSING TWO PARAMETERS

- **`FMaterialVertexShaderParameters::Bind`** at `MaterialShader.cpp:1144-1147`. Ours only calls `FMaterialShaderParameters::Bind(ParameterMap, SF_Vertex)` and returns — zero vertex-specific binds. Gangland `BmGame.exe.c:6973005-6973012`:
  ```
  FMaterialShaderParameters::Bind(this, a2, 0);
  FShaderParameter::Bind       (this + 80, a2, L"ObjectRotation",       1u);
  FShaderResourceParameter::Bind(this + 86, a2, L"SmoothNormalsTexture", 1u);
  ```
  BM3 vertex shaders that declare `float4 ObjectRotation` and `sampler2D SmoothNormalsTexture` have no binding target. Any vertex shader relying on object-space rotation quaternion or smooth-normals texture fetch receives stale/undefined vertex constants and uses a random sampler slot. **`FMaterialVertexShaderParameters` is missing two members** (`FShaderParameter ObjectRotationParameter` + `FShaderResourceParameter SmoothNormalsTextureParameter`), matching Bind calls, serialization, and plumbing through `SetMesh`. BM3's `SetMesh` logic is at `BmGame.exe.c:6973055-6973091` behind `IsBound()` guards on `this+42` and `this+44`.

### `FMaterialShaderParameters::Bind` — LODFade placement, DOFParameters spurious

- `MaterialShader.cpp:425-428` (ours). Gangland `BmGame.exe.c:6972432` binds `LODFade` **first**, before the scalar/vector/texture loops; we bind it last (member `LODFadeParameter` at the tail of the class). Bind-order swapping doesn't change correctness (lookup is by name), but layout byte-offsets into `FMaterialShaderParameters` differ vs BM3.
- `DOFParameters.Bind(ParameterMap)` at `MaterialShader.cpp:425` is **not present in Gangland** — BM3 binds DOF parameters elsewhere. This binds names like `MinZ_MaxZRatio`, `ModulateBlurColor` against every material shader's param map, producing spurious "not found" warnings and leaving DOFParameters unbound (harmless by `IsBound()` but misleading). Consider guarding behind `#if !BATMAN`.

### `FMaterialShaderParameters::SetShader` — silent white/zero fallback hides upstream bugs

- `MaterialShader.cpp:462-546` (`#if BATMAN`). On out-of-range uniform expression indexes, substitutes `FVector4(0,0,0,0)` / `GWhiteTexture`. Gangland `BmGame.exe.c:6977944-6978352` has no fallback — a mismatched index `checkSlow`-aborts. **This is masking the real bug.** Whenever the BM3 shader map's expression array disagrees with our locally reconstructed `FUniformExpressionSet`, ours silently draws white/zero instead of crashing. Temporarily removing the fallback will surface the exact `(shader type, parameter, index)` triple that is desynced and pinpoint which of the wire-format bugs above is hitting first.

### `FMaterialPixelShaderParameters::Set` — defensive InvGamma and debug logging

- `MaterialShader.cpp:806-814`: if `IsUsedWithGammaCorrection()` is false but the shader map came from BM3 cache, we set `InvGammaParameter = 1.0f`. Gangland `BmGame.exe.c:6980230-6980235` only sets `MatInverseGamma` when gamma correction is enabled. Ours is a workaround for `UMaterial::bUsedWithGammaCorrection` not being propagated by the cooked material — keep it unless the upstream flag gets fixed.
- `MaterialShader.cpp:690-704` (`GBmLogEverySeenMaterial`) is diagnostic-only, not present in Gangland.

### `FMaterialPixelShaderParameters::Bind` — extra non-BM3 binds

- `MaterialShader.cpp:671-678` binds `EnableScreenDoorFade`, `ScreenDoorFadeSettings`, `ScreenDoorFadeSettings2`, `ScreenDoorNoiseTexture`, `AlphaSampleTexture` (BM3 also has this one), `FluidDetailNormalTexture`. Gangland `BmGame.exe.c:6972775-6972788` binds only: PixelTextureCube loop, LocalToWorldMatrix, WorldToLocalMatrix, WorldToViewMatrix, InvViewProjectionMatrix, ViewProjectionMatrix, `FSceneTextureShaderParameters::Bind`, TwoSidedSign, MatInverseGamma, **DecalFarPlaneDistance**, ObjectPostProjectionPosition, ObjectMacroUVScales, ObjectNDCPosition, OcclusionPercentage, **AlphaSampleTexture**. Our extra binds pass `bAllowMissing=TRUE` so they no-op cleanly on BM3 shaders, and the corresponding serialization at `MaterialShader.cpp:1028-1045` is guarded by `#if BATMAN`/`IsBmCooked`. Safe but noisy.

### `operator<<(FArchive&, FMaterialPixelShaderParameters&)` — zero-buffer skip is fragile

- `MaterialShader.cpp:1028-1045`. Under `Ar.IsBmCooked(FALSE)`, zero-initialises `FluidDetailNormalTextureParameter` and `DOFParameters` via a 256-byte `TArray<BYTE> ZeroData` memory reader. The 256 bytes is arbitrary; if `FDOFShaderParameters::Serialize` ever reads more than that, `FMemoryReader` crashes. Prefer default-constructing these members instead.

### `FMaterialShaderMap::FindId` — fuzzy static-switch fallback

- `MaterialShader.cpp:1340-1438` (`#if BATMAN`). On miss, linearly scans same-BaseMaterialId entries and picks any cache entry whose `StaticSwitchParameters` + `StaticComponentMaskParameters` are a superset of the lookup set. Gangland `BmGame.exe.c:6982543-6982566` has no fallback — exact hashtable match or NULL. This is a necessary workaround because BM3 cooked materials declare more static switches than our engine rederives, and it works because `GetTypeHash(FStaticParameterSet)` only hashes `BaseMaterialId.A` (`Inc/MaterialShared.h:1673`). Keep.

### `FMaterialShaderMap::Compile` / `InitShaderMap` — BM3 cache bypass

- `MaterialShared.cpp:5901-5916` (`CacheShaders`) and `MaterialShared.cpp:1266-1272` (`InitShaderMap`). Under `IsFromBmCache()`, skips compilation and the `IsComplete()` validation. Gangland always compiles through the full loop on `IsComplete()` miss — but Gangland is the shipped game and never needs to Compile because its local shader cache is a complete cover. Our bypass is required for prebaked-cache consumption; risk is only if a BM3 shader map ever falls through to an actual Compile() call, which would then skip validation. Double-check that no code path does that.

### `FMaterialShaderMap::Serialize` — Ver≥656 gate for `UniformExpressionSet`

- `MaterialShader.cpp:2045-2070`. Sequence matches Gangland `BmGame.exe.c:7004141-7004188`: `TShaderMap::Serialize → MeshShaderMaps → MaterialId → FriendlyName → FStaticParameterSet::Serialize → (if Ver>=656) FUniformExpressionSet::Serialize → Platform INT → InitVertexFactoryMap`. **Verify `VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE == 656`** in our `UnObjVer.h` or the BM3-compat define. If we bumped this higher to distinguish our own changes, BM3 caches will load without the `UniformExpressionSet.Serialize()` step, and every subsequent `Set()` will use empty expression arrays and the zero-fallback above binds zero/white for everything. **High-priority check.**

### `FStaticParameterSet::Serialize` — version-gate cutoffs

- `Inc/MaterialShared.h:1651-1662` inline. Gangland `BmGame.exe.c:5184598-5184618`: 4×ByteOrderSerialize for BaseMaterialId DWORDs, then StaticSwitchParameters, then StaticComponentMaskParameters, then `if (Ver>=631) NormalParameters`, then `if (Ver>=714) TerrainLayerWeightParameters`. **Verify `VER_ADD_NORMAL_PARAMETERS == 631`** and `VER_ADD_TERRAINLAYERWEIGHT_PARAMETERS == 714` in our build.

### `FUniformExpressionSet::Serialize` — `WITH_D3D11_TESSELLATION` required

- `MaterialShared.cpp:613-627`. With `WITH_D3D11_TESSELLATION=1` serialises 13 TArrays: `PixelExpressions × 3 + CubeTex × 1 + VertexExpressions × 3 + HullExpressions × 3 + DomainExpressions × 3`. Gangland `BmGame.exe.c:5214164-5214185` does exactly 13 `operator<<` calls in that order. Our fallback path (tessellation disabled) uses `Dummy0/Dummy1` for only 6 arrays → 7 total. **If we are building without `WITH_D3D11_TESSELLATION`, BM3 cache files completely corrupt-load from the Vertex-expressions TArray onward.** High priority — verify `WITH_D3D11_TESSELLATION` is 1 in our BATMAN build config.

### `FMaterialUniformExpressionTextureParameter::Serialize` — matches

- `MaterialShared.cpp:2134-2138` writes `ParameterName` FName first, then `Super::Serialize`. Gangland `BmGame.exe.c:5169792-5169798`: same.

### `operator<<(FArchive&, FMaterialUniformExpression*&)` — stream-desync bug

- `MaterialShared.cpp:500-509` (`#if BATMAN`). On an unknown `FMaterialUniformExpressionType` name, warns and sets `Ref = NULL; return Ar;` — **skipping** the call to `Ref->Serialize(Ar)`. If BM3's cached cache holds any uniform-expression type name we haven't registered locally (plausible: `FMaterialUniformExpressionFoldedMath`, `FMaterialUniformExpressionMax`, or any BM3-specific class), we leave the archive cursor pointing at the start of what was supposed to be the expression body. The next array element read then sees garbage. **This is probably a major cause of broken shader binding.** Fixes: (a) register serialization-only stubs for every BM3 uniform-expression type, (b) have the operator still call `Ref->Serialize` through a known base type + `SkipOffset`, or (c) have the BM3 cooker emit a per-expression size prefix. The current behaviour is certain to desync on any unknown type.

### `FMaterial::GetShader` — BM3 fallback to DefaultMaterial

- `MaterialShared.cpp:6041-6056`. On miss, walks up to `GEngine->DefaultMaterial` and tries again. Gangland `appErrorf`s. Same concern as the white-texture fallback: hides the real bug (BM3 shader map is missing a VF combo). Keep for stability but add diagnostic logging.

### `CameraWorldPositionParameter` writes `View->ViewOrigin`

- `MaterialShader.cpp:559-561`. Gangland `BmGame.exe.c:6978353-6978359` writes `*(a4+3) + 992`. `a4+3` is the 4th DWORD of `FMaterialRenderContext*` (in our layout, the view pointer). **Verify byte offset +992 inside BM3's `FSceneView` actually corresponds to `ViewOrigin` and not `PreViewTranslation`**; BM3's `FSceneView` may have been enlarged vs stock. If it writes a different float4 than we think, every `CameraWorldPos`-using expression mispositions.

### `UMaterialExpressionLightingDiffuseLambert/SpecularBlinnPhong/Heidrich/Phong::Compile`

- `MaterialExpressions.cpp:1064-1110` — all four stubbed to `Constant3(0,0,0)`. Editor-only, stripped in cooked builds, no ground truth in Gangland. Only matters if we ever re-compile material graphs at runtime. FYI.

### `FMaterial::ShaderMap` / `Id` access modifiers

- `Inc/MaterialShared.h:985-993` — `ShaderMap` made public, `Id` private under BATMAN. No behavioural diff.

### `UMaterial::GetParameterDesc` — null-expression skip

- `Material.cpp:623-629` skips NULL entries. BM3 cooked packages have stripped/null entries in `Expressions`. Keep.

---

## BasePass / Depth / Fog Rendering

### Summary of the biggest problems in this subsystem

1. `DOFPackedParameters0` / `DOFPackedParameters1` bind names do not exist in BM:AC/BM3 bytecode. Gangland uses a single compound `FDOFShaderParameters` whose own ctor binds `"PackedParameters"` and `"MinMaxBlurClamp"`. Every `.IsBound()` check against our strings fails and every `SetVertexShaderValue` no-ops.
2. `HeightFogShaderParameters` has zero references in Gangland, but our `TBasePassVertexShader` ctor unconditionally calls `HeightFogParameters.Bind` and `DrawShared` calls `HeightFogParameters.SetVertexShader`. Our BATMAN Serialize tries to paper over this by reading from a zero stream.
3. Our BATMAN serialize layouts for `TBasePassVertexShader` do not match what Gangland actually writes. The specific `Dummy; Dummy;` pair our hack inserts corresponds to nothing real in BM3. Every shader after the first is off by N bytes.
4. Fog density policy set does not match: BM:AC ships only `FNoDensityPolicy`, `FSphereDensityPolicy`, and `FRockAtmosDensityPolicy`. We instantiate shader types for `FConstantDensityPolicy`, `FLinearHalfspaceDensityPolicy`, `FSphereDensityPolicy`, `FConeDensityPolicy` — the missing `FRockAtmosDensityPolicy` is critical (used for Batman atmosphere shots), the three extras resolve to NULL in the cache and crash the drawing policy.
5. `TDepthOnlyVertexShader<TRUE>`, `TFogIntegralVertexShader`, `FFogVolumeApplyVertexShader` all read two phantom trailing `FShaderParameter`s that Gangland never writes.
6. `DeferredRenderingParameters` bind name does not exist in BM:AC (PC basepass has no deferred rendering). Our BATMAN Serialize guard avoids reading it, but `.Bind(...)` and `SetPixelShaderValue(..., DeferredRenderingParameters, ...)` still run.

### `TBasePassVertexShader<FNoLightMapPolicy, FNoDensityPolicy>`

Our ctor `BasePassRendering.h:35-47` binds: `LightMapPolicyType::VertexParametersType` (no-op for FNoLightMap), `MaterialParameters`, `HeightFogParameters`, `FogVolumeParameters`, `DOFPackedParameters0` (`"DOFPackedParameters0"`), `DOFPackedParameters1` (`"DOFPackedParameters1"`), `ObjectFogColorParameter` (`"ObjectFogColor"`).

Gangland ctor `BmGame.exe.c:7187227-7187258`:
```
FMaterialVertexShaderParameters::Bind(this + 148, pm);
FShaderParameter::Bind(this + 254, pm, L"ObjectFogColor", 1u);
// FDOFShaderParameters subobject at this+242 constructs and calls its own binds:
//   FShaderParameter::Bind(this+0, "PackedParameters")
//   FShaderParameter::Bind(this+6, "MinMaxBlurClamp")
// (ref BmGame.exe.c:4897556-4897569)
```

Gangland Serialize `BmGame.exe.c:7187187-7187198` writes exactly 4 items: `VertexFactoryParameters(120)`, `MaterialParameters(148)`, `FDOFShaderParameters(242)` (12 bytes = two 6-byte FShaderParameter via its own operator<<), `ObjectFogColorParameter(254)`. **No `FogVolumeParameters`, no `HeightFogParameters`, no dummies.**

Our Serialize `BasePassRendering.h:68-107` BATMAN path writes: `FShader::Serialize → VertexFactoryParameters → MaterialParameters → Dummy → Dummy → FogVolumeParameters → DOFPackedParameters0 → DOFPackedParameters1 → ObjectFogColorParameter`, then zero-reads `HeightFogParameters`.

Diff: we read 2 extra `Dummy` `FShaderParameter`s + a full `FFogVolumeShaderParameters` (6 × `FShaderParameter`) = 8 extra `FShaderParameter` reads = 48 bytes of archive bytes that BM3 never wrote. These bytes belong to the *next* shader in the map. Every subsequent shader deserialises from the wrong offset. Combined with the wrong DOF parameter names, this is catastrophic for every base-pass vertex shader after the first on a `FNoDensityPolicy` variant. Fix: drop HeightFogParameters and FogVolumeParameters entirely under BATMAN for `FNoDensityPolicy`, remove the two Dummies, and collapse `DOFPackedParameters0/1` into a single `FDOFShaderParameters` whose binds match BM3.

### `TBasePassVertexShader<FNoLightMapPolicy, FSphereDensityPolicy>`

Gangland ctor `BmGame.exe.c:7187359-7187395`:
```
FMaterialVertexShaderParameters::Bind(this + 148, pm);
FFogVolumeShaderParameters::Bind(this + 240, pm);     // 6 sub-params
FDOFShaderParameters::FDOFShaderParameters(this + 276, pm);  // PackedParameters + MinMaxBlurClamp
FShaderParameter::Bind(this + 288, pm, L"ObjectFogColor", 1u);
```
Gangland Serialize `BmGame.exe.c:7187309-7187329`: `VertexFactoryParameters → MaterialParameters(148) → FogVolumeBoxMin/Max/ApproxFogColor/First/Second/StartDistance(240..270) → FDOFShaderParameters(276) → ObjectFogColor(288)`.

Our Serialize uses the same BATMAN layout as FNoDensityPolicy, so it writes `Dummy; Dummy;` where Gangland wrote the actual `FFogVolumeShaderParameters` sub-fields, then writes the fog volume params, then two DOF params. Offsets drift a different amount but still drift. Gangland always orders `MaterialParameters → FFogVolumeShaderParameters → FDOFShaderParameters → ObjectFogColor`; ours inserts the two Dummies before the fog-volume block and splits DOF into two rather than one compound.

### `TBasePassVertexShader<FDirectionalVertexLightMapPolicy, FNoDensityPolicy>`

Gangland ctor `BmGame.exe.c:7187743-7187774`:
```
FShaderParameter::Bind(this + 148, pm, L"LightMapScale", 1u);
FMaterialVertexShaderParameters::Bind(this + 156, pm);
FDOFShaderParameters::FDOFShaderParameters(this + 250, pm);
FShaderParameter::Bind(this + 262, pm, L"ObjectFogColor", 1u);
```
Gangland Serialize `BmGame.exe.c:7187726-7187740`: `VertexFactoryParameters(120) → LightMapScale(148) → MaterialParameters(156) → FDOFShaderParameters(250) → ObjectFogColor(262)`. Note: **`LightMapScale` is written AFTER `VertexFactoryParameters`, not before.**

Our Serialize `BasePassRendering.h:74-75`:
```
UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
LightMapPolicyType::VertexParametersType::Serialize(Ar);  // writes LightMapScale here
bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
```

Our order: `[base] LightMapScale → VertexFactoryParameters → MaterialParameters → …`. Gangland order: `[base] VertexFactoryParameters → LightMapScale → MaterialParameters → …`. The two operator<< are swapped. Even if the Dummy padding were corrected, vertex-lightmap shaders read the `LightMapScale` FShaderParameter from bytes meant to be the `VertexFactoryParameters` TArray header, producing a bogus buffer-size allocation. **Impact: every directional/simple vertex-lightmap base-pass shader deserialises completely wrong.** These are Batman's character / cloth / vertex-lit meshes.

USF cross-check: `BasePassVertexShader.usf:12` declares `float4 LightMapScale[NUM_LIGHTMAP_COEFFICIENTS];` — so the vertex stage is the right place, but our Serialize ordering must match Gangland's. The fix is to move `LightMapPolicyType::VertexParametersType::Serialize(Ar)` below the `Ar << VertexFactoryParameters` line under BATMAN. Related: `FDirectionalLightLightMapPolicyVertexShaderParameters::Serialize` in `LightMapRendering.h` should be verified against the same expectation.

### `TBasePassPixelShaderBaseType<FNoLightMapPolicy>`

Our ctor `BasePassRendering.h:285-297` binds:
```
LightMapPolicyType::PixelParametersType::Bind(pm);   // no-op
MaterialParameters.Bind(...)
AmbientColorAndSkyFactorParameter.Bind("AmbientColorAndSkyFactor")
UpperSkyColorParameter.Bind("UpperSkyColor")
LowerSkyColorParameter.Bind("LowerSkyColor")
DeferredRenderingParameters.Bind("DeferredRenderingParameters")   // BM3 has no such string
MotionBlurMaskParameter.Bind("MotionBlurMask")   // BATMAN
```

Gangland ctor `BmGame.exe.c:7187568-7187586`:
```
FMaterialPixelShaderParameters::Bind(this + 120, pm);
FShaderParameter::Bind(this + 320, pm, L"AmbientColorAndSkyFactor", 1u);
FShaderParameter::Bind(this + 326, pm, L"UpperSkyColor", 1u);
FShaderParameter::Bind(this + 332, pm, L"LowerSkyColor", 1u);
FShaderParameter::Bind(this + 338, pm, L"MotionBlurMask", 1u);
```

Gangland Serialize `BmGame.exe.c:7187599-7187610`:
```
FShader::Serialize
MaterialParameters(120)
AmbientColorAndSkyFactor(320)
UpperSkyColor(326)
LowerSkyColor(332)
MotionBlurMask(338)
```

Our BATMAN Serialize **zero-inits `UpperSkyColorParameter`, `LowerSkyColorParameter`, and `DeferredRenderingParameters`** instead of reading them, and our order is `MaterialParameters → AmbientColorAndSkyFactor → MotionBlurMask`. But Gangland definitely serialises `UpperSkyColor` and `LowerSkyColor` (offsets 326 and 332). We are zeroing parameters that BM3 actually ships bound — sky-lit objects get unbound UpperSkyColor/LowerSkyColor and render black skylight. Worse: because we skip the two sky colours and then read `MotionBlurMask`, the next 12 bytes from the stream (which are `UpperSkyColor`'s `FShaderParameter`) are consumed as `MotionBlurMask`'s `FShaderParameter` contents. `MotionBlurMask.BufferIndex/BaseIndex/NumBytes` are all wrong. **This is almost certainly the source of the "pure white / pure black" base-pass output described by the in-code comment at `BasePassRendering.h:313-320`** — that comment is rationalising the symptom, not fixing it.

USF cross-check: `BasePassPixelShader.usf:20` declares `float MotionBlurMask;`, `:34-35` declares `half3 UpperSkyColor; half3 LowerSkyColor;`, `:43` declares `half4 AmbientColorAndSkyFactor` (non-console). All three exist. No `DeferredRenderingParameters` in any USF; that's a BATMAN D3D11-only addition irrelevant to BM3.

Fix: in the BATMAN Serialize path for `TBasePassPixelShaderBaseType`, insert `Ar << UpperSkyColorParameter; Ar << LowerSkyColorParameter;` between `AmbientColorAndSkyFactor` and `MotionBlurMask`. Do not read `DeferredRenderingParameters` at all (either guard the member out under BATMAN or leave it unbound and unused).

### `TDepthOnlyVertexShader<TRUE>` — phantom trailing `FShaderParameter`s

Ours `DepthRendering.cpp:19-23` ctor binds only `MaterialParameters`. Gangland ctor `BmGame.exe.c:7186384-7186408`: `FMeshMaterialVertexShader::FMeshMaterialVertexShader; FMaterialVertexShaderParameters::Bind(this+148, pm)`. Matches.

Gangland Serialize `BmGame.exe.c:7186349-7186358` (and `:7194475-7194484` for `<FALSE>`): `FShader::Serialize → VertexFactoryParameters(120) → MaterialParameters(148)`. **Only two operator<<'s.** Ours `DepthRendering.cpp:43-59` adds a BATMAN block that reads two throwaway `FShaderParameter`s at the end when `bUsePositionOnlyStream && IsBmCooked`. Remove it — Gangland writes nothing extra. Currently the position-only VS consumes 12 bytes from the next shader.

### `TDepthOnlyPixelShader<FALSE>` / `<TRUE>` — matches

`DepthRendering.cpp:158-162` ctor + Serialize align with Gangland `BmGame.exe.c:7186269-7186294`. No diff.

### `FDepthDrawingPolicy` — no relevant diff

`DepthRendering.cpp:241-313` matches Gangland's equivalent in logic. No shader-binding diff.

### `TFogIntegralPixelShader<FSphereDensityPolicy>`

Ours `FogVolumeRendering.h:550-563` ctor binds 9 params including `MaxDistanceParameter` (`"MaxDistance"`). Gangland ctor `BmGame.exe.c:7186416-7186441` binds **8 params** — no `MaxDistance`:
```
FMaterialPixelShaderParameters::Bind(this+120);
FShaderParameter::Bind(this+320, "DepthFilterSampleOffsets");
FShaderParameter::Bind(this+326, "ScreenToWorld");
FShaderParameter::Bind(this+332, "FogCameraPosition");
FShaderParameter::Bind(this+338, "FaceScale");
FShaderParameter::Bind(this+344, "FirstDensityFunctionParameters");
FShaderParameter::Bind(this+350, "SecondDensityFunctionParameters");
FShaderParameter::Bind(this+356, "StartDistance");
FShaderParameter::Bind(this+362, "InvMaxIntegral");
```

Our Serialize `FogVolumeRendering.h:622-642`:
```cpp
Ar << StartDistanceParameter;
Ar << MaxDistanceParameter;          // always
#if BATMAN
if (!Ar.IsBmCooked(TRUE))
{
    Ar << MaxDistanceParameter;      // again when NOT cooked
}
#endif
Ar << InvMaxIntegralParameter;
```

The BATMAN condition is **inverted**: on BM-cooked it writes `MaxDistance` once (wrong — should be zero), and on non-cooked it writes `MaxDistance` *twice* (wrong — stock UE3 writes once). Both branches are broken. Gangland Serialize `BmGame.exe.c:7186459-7186472` writes exactly 9 items with no `MaxDistance`. Fix: skip `MaxDistanceParameter` unconditionally under BATMAN. Currently, stream drifts +6 bytes per `TFogIntegralPixelShader<FSphereDensityPolicy>`.

### `TFogIntegralVertexShader<FSphereDensityPolicy>` — phantom trailing params

Ours `FogVolumeRendering.h:495-510` adds a BATMAN block reading two dummies. Gangland `BmGame.exe.c:7186555-7186564`: only `FShader::Serialize → VertexFactoryParameters(120) → MaterialParameters(148)`. Nothing else. Remove the dummy reads.

Also: Gangland only ever instantiates `TFogIntegralVertexShader` for `FSphereDensityPolicy` and `FRockAtmosDensityPolicy`. Never for `FConstantDensityPolicy`, `FLinearHalfspaceDensityPolicy`, `FConeDensityPolicy`. Our `HANDLE_FOG_VOLUME_DENSITY_FUNCTION` switch routes to types that don't exist in BM3 caches.

### `FFogVolumeApplyVertexShader` — phantom trailing params

Ours `FogVolumeRendering.h:788-802` BATMAN block reads two dummies. Gangland `BmGame.exe.c:7197620-7197630`: only `FShader::Serialize → VertexFactoryParameters(120) → MaterialParameters(148)`. Remove.

Ctor at `:778-782` binds `MaterialParameters` only. Gangland `:7197586-7197613`: identical.

### `FFogVolumeApplyPixelShader` — matches

Ours `FogVolumeRendering.h:841-848` binds `MaxIntegral`, `MaterialParameters`, `AccumulatedFrontfacesLineIntegralTexture`, `AccumulatedBackfacesLineIntegralTexture`. Gangland `BmGame.exe.c:7197700-7197715`: same names, same order. **No diff.**

### Fog density policy enumeration

`BasePassRendering.cpp:60-65` instantiates `TBasePassVertexShader` for `FNoDensityPolicy`, `FConstantDensityPolicy`, `FLinearHalfspaceDensityPolicy`, `FSphereDensityPolicy`, `FConeDensityPolicy`. Gangland instantiates only three: `FNoDensityPolicy`, `FSphereDensityPolicy`, `FRockAtmosDensityPolicy` (the latter at e.g. `BmGame.exe.c:7192334`, `:7187554` for `FRockAtmosDensityPolicy::FFogRockAtmosShaderParameters::Bind`).

Missing: `FRockAtmosDensityPolicy` — critical for Batman atmosphere shots. Extra: `FConstantDensityPolicy`, `FLinearHalfspaceDensityPolicy`, `FConeDensityPolicy` — BM3 shader maps have no cached entries, so `GetShader<...>` returns NULL and the drawing policy dereferences it. Any primitive inside a BM3-authored `UFogVolumeRockAtmosDensityComponent` will fail to find a shader.

### FogVolume USF vs C++ cross-check

Bind names used by `FFogVolumeShaderParameters::Bind` (both our side and Gangland agree): `FirstDensityFunctionParameters`, `SecondDensityFunctionParameters`, `StartDistance`, `FogVolumeBoxMin`, `FogVolumeBoxMax`, `ApproxFogColor`. All six exist in `Engine/Shaders/BasePassVertexCommon.usf` (ApproxFogColor line 25, FogVolumeBoxMin line 28, FogVolumeBoxMax line 31). No USF mismatch here.

### Notes on non-parameter-binding BATMAN blocks

- `BasePassRendering.cpp:83-85` adds `IMPLEMENT_BASEPASS_LIGHTMAPPED_SHADER_TYPE(FAPlus3DLightLightMapPolicy)` — matches Gangland `BmGame.exe.c:7192334`. Correct.
- `BasePassRendering.h:1113-1118` disables routing to `FAPlus3DLightLightMapPolicy` in `ProcessBasePassMesh()`. The comment says "causes white output" — this is a symptom of the PixelShader Serialize misalignment above. Once that is fixed, APlus3D routing should be re-enabled.
- `BasePassRendering.h:313-320` forces `AmbientColorAndSkyFactor = (0,0,0,1)` for BATMAN — a workaround for the same misalignment.

---

## Lighting / LightMap Rendering

### Meta findings

1. Our custom class is `FAPlus3DLightLightMapPolicy`; Gangland calls the scene-info type `FAmbientPlus3DirectionalLightSceneInfo` and the policy `FAmbientPlus3DirectionalLightPolicy`. The element-data type for the policy is `const FAmbientPlus3DirectionalLightSceneInfo*` passed by pointer, **not** a wrapper struct.
2. `FAPlus3DLightLightMapPolicy` in Gangland does **not** derive from `FDirectionalLightLightMapPolicy`. Its `VertexParametersType` and `PixelParametersType` are flat single-`FShaderParameter` structs. Our code derives from `FDirectionalLightLightMapPolicy`, dragging in an unwanted parent `Bind` chain and extra serialized bytes.

### Shader parameter name mismatches (highest impact)

- **`APlus3DLightInfoPixel` vs `APlus3DLightPixelInfo`** — `LightMapRendering.h:977` and `:1120` bind `TEXT("APlus3DLightInfoPixel")`. Gangland `BmGame.exe.c:7079473, :4794413, :7192500` binds `L"APlus3DLightPixelInfo"`. `.IsBound()` returns FALSE on BM3 cooked shaders; the 4-element `APlus3DLightInfo` array our `FAPlus3DLightLightMapPolicy::SetMesh` writes (`LightMapRendering.h:1050`) is silently dropped. Character lighting constants hold whatever was last in the constant register.
- **`APlus3DLightInfoVertex` vs `APlus3DLightVertexInfo`** — `LightMapRendering.h:950` and `:1076`. Gangland `BmGame.exe.c:7079484, :7079591, :7079741, :4794903, :7192179` binds `L"APlus3DLightVertexInfo"`. Same consequence on the vertex side.
- **`LightMapLumaChannel` does not exist in BM3.** Our `LightMapRendering.h:318` binds `TEXT("LightMapLumaChannel")` in `FLightMapTexturePolicy::PixelParametersType::Bind`; `:549-558` (`FDirectionalLightMapTexturePolicy::SetMesh`) unconditionally `SetPixelShaderValue`s it. Gangland's `FLightMapTexturePolicy::PixelParametersType::Bind` at `BmGame.exe.c:5021326-5021332` binds only `LightMapTextures` + `LightMapScale`. `grep LightMapLumaChannel Engine/Shaders` → 0 matches. **BATMAN-gated Serialize at `LightMapRendering.h:399-404` writes an extra `FShaderParameter` into the lightmap-pixel-param stream under `Ar.IsBmCooked(FALSE)`.** Injecting a phantom parameter into a position that never existed in BM3's writer shifts every subsequent parameter read. This is a prime suspect for corrupting BM3 cooked base-pass lightmap pixel shader loads.
- **`LightDirection` vs `LightDirectionAndbDirectional`** — our BATMAN block `LightMapRendering.h:1069-1091` adds a `LightDirectionParameter` bound as `TEXT("LightDirection")` on `FDynamicallyShadowedMultiTypeLightLightMapPolicy::VertexParametersType`. Gangland `BmGame.exe.c:7079589` binds `L"LightDirectionAndbDirectional"` (packed `float4` = direction.xyz + `.w` flag). Our `DirectionalLightVertexShader.usf:18` declares plain `float3 LightDirection`, so the name works for our own shaders; but BM3 cooked shaders won't find our name, and we never carry the `bDirectional` flag anywhere.
- **`FDirectionalLightLightMapPolicy::VertexParametersType::Bind` semantic mismatch** — `LightMapRendering.h:755` binds `TEXT("LightPositionAndInvRadius")`. Gangland `BmGame.exe.c:7079354` binds `L"LightDirectionAndbDirectional"`. Our `PointLightComponent.cpp:536-539` even uploads `FVector4(-Light->GetDirection(), 0.0f)` — a *direction* stored in a parameter named "Position". Data and name are inconsistent. BM3 cooked shaders never find this parameter.

### `FAPlus3DLightLightMapPolicy` class-shape divergences

- **Wrong base class.** `LightMapRendering.h:938` declares `class FAPlus3DLightLightMapPolicy : public FDirectionalLightLightMapPolicy`. The inherited `VertexParametersType::Bind` (`:947-951`) calls `Super::VertexParametersType::Bind(pm)` which binds `LightPositionAndInvRadius`. Gangland's `FAPlus3DLightLightMapPolicy::VertexParametersType::Bind` at `BmGame.exe.c:7079480-7079485` binds **only** `APlus3DLightVertexInfo` — no parent, no `LightPositionAndInvRadius`. Same for pixel: our `:975-979` binds `APlus3DLightInfoPixel` **and** `WorldIncidentLighting`; Gangland `:7079469-7079474` binds **only** `APlus3DLightPixelInfo`. Our class serialises extra `FShaderParameter`s that don't exist in BM3 cooked data.
- **`PixelParametersType::Serialize` writes two params; Gangland writes one.** `LightMapRendering.h:981-986`: `Ar << APlus3DLightInfoPixelParameter; Ar << WorldIncidentLightingParameter;`. Gangland `FAPlus3DLightLightMapPolicy::PixelParametersType::Serialize` at `BmGame.exe.c:7192521-7192526`: single `operator<<(a2, this)`. Adds one phantom `FShaderParameter` per BM3 cooked pixel-shader record.
- **`VertexParametersType::Serialize` ordering wrong.** `LightMapRendering.h:953-965` writes `LightDirectionParameter → Super::VertexParametersType → APlus3DLightInfoVertex`. Gangland writes a single field (only `APlus3DLightVertexInfo`). Our struct also declares `LightDirectionParameter` as a member *after* the Serialize call site (`:967`).
- **`SetMesh` ignores per-primitive data.** `LightMapRendering.h:1020-1057` hardcodes `FVector4(0,0,0,0)` for the three directional-light colours and `(1,1,1,1)` for the ambient slot; does not upload anything vertex-side. Gangland `FAPlus3DLightLightMapPolicy::SetMesh` at `BmGame.exe.c:4771369-4771399` sources both pixel and vertex data from `FAmbientPlus3DirectionalLightSceneInfo` (pointer passed as element data): pixel = 4×FVector4 at scene-info byte offset +688 (`APlus3DLightPixelInfo`), vertex = 3×FVector4 at scene-info byte offset +640 (`APlus3DLightVertexInfo`). Even with the names fixed, BM3 character lights would be black on the base-pass path.
- **`ElementDataType` shape.** Our `ElementDataType` (`LightMapRendering.h:989-995`) is a wrapper struct containing a `Super::ElementDataType`. Gangland's is `const FAmbientPlus3DirectionalLightSceneInfo*` passed by pointer (`BmGame.exe.c:4771369-4771379`, `:82057`). If any BM3 cooked `StaticMeshDrawList` uses this policy, our draw-list wiring expects the wrong element-data layout.

### `TLightPixelShader` / `TLightVertexShader` BATMAN hacks (`LightRendering.h`)

- **`LightRendering.h:468-475` dummy serialize.** Under `Ar.IsBmCooked(FALSE)`, vertex-shader Serialize reads two extra `FShaderParameter`s into throwaway locals. Gangland `TLightVertexShader<FAmbientPlus3DirectionalLightPolicy, FNoStaticShadowingPolicy>::Serialize` at `BmGame.exe.c:4794824-4794836` serialises only `FShader::Serialize → APlus3DLightVertexInfo(148) → VertexFactoryParameters(120) → FMaterialVertexShaderParameters(156)`. **No trailing FShaderParameters.** Remove the dummies. Currently desyncs every subsequent light vertex shader in the file.
- **`LightRendering.h:607-609, :677-679, :656-661` — `SpecularScaleParameter` placement.** In the non-shadowed variant Gangland `BmGame.exe.c:4794397-4794419` binds `SpecularScale` at offset +126 (immediately after `APlus3DLightPixelInfo` at +120 and before `FMaterialPixelShaderParameters::Bind(+132)`). Gangland's SDF Serialize order `:4794475-4794497` is `APlus3DLightPixelInfo → DistanceFieldParameters → ShadowTexture → SpecularScale → FMaterialPixelShaderParameters → LightAttenuationTexture → FForwardShadowingShaderParameters`. Our order interleaves empty `ShadowingTypePolicy::PS::Serialize` before `SpecularScale`; if `FNoStaticShadowingPolicy::PixelParametersType::Serialize` ever writes any bytes, our stream is offset. Verify.
- **USF check:** `grep SpecularScale Engine/Shaders` → 0 matches. Our shaders don't declare it either; `SpecularScale` is cooked-only. `IsBound()` is FALSE for our own shaders, but the Serialize ordering is still critical for reading BM3 cooked data.

### `FDynamicallyShadowedMultiTypeLightLightMapPolicy`

- **Vertex params.** Gangland `VertexParametersType::Bind` at `BmGame.exe.c:7079585-7079592` binds exactly three: `LightDirectionAndbDirectional`, `LightPositionAndInvRadius`, `APlus3DLightVertexInfo`. Ours `LightMapRendering.h:1072-1077` binds `LightDirection` (wrong name) → `FDirectionalLightLightMapPolicy::VertexParametersType::Bind` (which binds `LightPositionAndInvRadius`) → `APlus3DLightInfoVertex` (wrong name). Count matches, two names are wrong. Inheritance from `FDirectionalLightLightMapPolicy::VertexParametersType` adds extra members beyond Gangland's flat 3×`FShaderParameter` — verify `sizeof` matches Gangland's offsets 0/6/12.
- **Pixel params.** Gangland `PixelParametersType::Bind` at `BmGame.exe.c:7079600-7079615` binds 11 params in order: `bDynamicDirectionalLight(+0)`, `bDynamicSpotLight(+6)`, `LightColorAndFalloffExponent(+12)`, `SpotDirection(+18)`, `SpotAngles(+24)`, `bEnableDistanceShadowFading(+30)`, `DistanceFadeParameters(+36)`, `LightChannelMask(+42)`, `LightAttenuationTexture(+48)`, `FForwardShadowingShaderParameters(+54)`, `APlus3DLightPixelInfo(+84)`. Ours `LightMapRendering.h:1107-1122` matches except `APlus3DLightInfoPixel` (wrong name). Ours writes `APlus3DLightInfoPixelParameter` last in Serialize under `Ar.IsBmCooked(FALSE)` — matches Gangland's trailing `+84`. But the gating is fragile: Gangland always writes it in cooked paths.

### `FLightMapTexturePolicy::PixelParametersType::Serialize`

- `LightMapRendering.h:395-408` adds `Ar << LightMapLumaChannelParameter;` under `Ar.IsBmCooked(FALSE)`. Gangland equivalent `BmGame.exe.c:5021326-5021332` has no such field — struct is `LightMapTextures + LightMapScale`, and `TBasePassPixelShaderBaseType<FDirectionalLightMapTexturePolicy>` ctor at `:7189091-7189116` confirms nothing between `LightMapScale(+126)` and `FMaterialPixelShaderParameters::Bind(+132)`. **Action:** remove the BATMAN `LightMapLumaChannel` bind + Serialize + the upload block at `LightMapRendering.h:549-558`. This is the most likely root cause of corrupted BM3 cooked base-pass lightmap pixel shader deserialisation.

### `SphericalHarmonicLightComponent.cpp` / `FSHLightLightMapPolicy`

- `SphericalHarmonicLightComponent.cpp` has **no** `#if BATMAN` blocks — matches stock Epic UE3 despite being listed as modified.
- SH packing layout: `SetSHPixelParameters` at `SphericalHarmonicLightComponent.cpp:184-213` packs `[R.V0, G.V0, B.V0, 0, R.V[1..], G.V[1..], B.V[1..]]` into a flat float4 array. `SphericalHarmonicCommon.usf:22-138` expects `float4 WorldIncidentLighting[NUM_SH_VECTORS*3 + 1]` with layout `[0].rgb = constant; [1..NUM_SH_VECTORS] = R; [NUM_SH_VECTORS+1..2*NUM_SH_VECTORS] = G; [2*NUM_SH_VECTORS+1..3*NUM_SH_VECTORS] = B`. Matches. No bug.
- Gangland `FSHLightLightMapPolicy::PixelParametersType::Bind` at `BmGame.exe.c:7079438-7079445` binds `LightColorAndFalloffExponent → FForwardShadowingShaderParameters → WorldIncidentLighting(+36)`. Ours `LightMapRendering.h:879-893` matches.
- `FSHLightAndMultiTypeLightMapPolicy::PixelParametersType::Bind` — Gangland `BmGame.exe.c:7079707-7079713` and ours `LightMapRendering.h:1271-1275`: same.

### `LightComponent.cpp` / `PointLightComponent.cpp` / `SpotLightComponent.cpp`

- `LightComponent.cpp:537-543` — early-returns from `ULightComponent::Serialize` in cooked BM3 to skip `InclusionConvexVolumes`/`ExclusionConvexVolumes`. Not a shader issue.
- `PointLightComponent.cpp:479-490` — disables editor preview-radius warning. No shader impact.
- `SpotLightComponent.cpp:440-458` — disables preview cone warnings. No shader impact.

### `LightMapDensityRendering.h` — no BATMAN blocks

Parameter names `LightMapDensityParameters`, `BuiltLightingAndSelectedFlags`, `DensitySelectedColor`, `LightMapResolutionScale`, `LightMapDensityDisplayOptions`, `VertexMappedColor`, `GridTexture` all match declarations in `Engine/Shaders/LightMapDensityShader.usf:126, 135, 154, 159, 162, 236`. Unmodified — no issues.

### Parameter-name quick reference

| Our name (wrong) | Gangland (correct) | Locations |
|---|---|---|
| `APlus3DLightInfoPixel` | `APlus3DLightPixelInfo` | `LightMapRendering.h:977`, `:1120` |
| `APlus3DLightInfoVertex` | `APlus3DLightVertexInfo` | `LightMapRendering.h:950`, `:1076` |
| `LightDirection` | `LightDirectionAndbDirectional` | `LightMapRendering.h:1074` |
| `LightMapLumaChannel` | (does not exist — remove) | `LightMapRendering.h:318`, `:399-404`, `:409-411`, `:415-417`, `:549-558` |
| `LightPositionAndInvRadius` in `FDirectionalLightLightMapPolicy::VertexParametersType` | `LightDirectionAndbDirectional` (BM3) | `LightMapRendering.h:755` |

### Lighting subsystem fix priority

1. Remove the `LightMapLumaChannel` BATMAN block entirely (bind + serialize + upload).
2. Rename the four wrong shader-parameter strings.
3. Remove the two dummy `FShaderParameter` reads in `TLightVertexShader::Serialize` (`LightRendering.h:468-475`).
4. Restructure `FAPlus3DLightLightMapPolicy`: drop inheritance from `FDirectionalLightLightMapPolicy`; make `VertexParametersType` a single `FShaderParameter APlus3DLightVertexInfoParameter`; `PixelParametersType` a single `FShaderParameter APlus3DLightPixelInfoParameter` (no `WorldIncidentLighting`); `ElementDataType` = `const FAmbientPlus3DirectionalLightSceneInfo*`.
5. If BM3 shaders really expect `LightDirectionAndbDirectional` in `FDirectionalLightLightMapPolicy::VertexParametersType::Bind`, add a BATMAN-conditional rename and update `PointLightComponent.cpp:536-539` to write direction + `.w = bDirectional`.
6. `static_assert(sizeof(FDynamicallyShadowedMultiTypeLightLightMapPolicy::VertexParametersType) == 18)` to confirm the inherited layout still produces offsets 0/6/12.
7. Verify `FNoStaticShadowingPolicy::PixelParametersType::Serialize` contributes zero bytes; if not, fix `SpecularScale` Serialize ordering in `TLightPixelShader` to match Gangland's per-variant byte offsets.

---

## Consolidated fix-priority list (cross-subsystem)

Ordered by expected impact on "shaders load but render garbage":

1. **Wire format:** delete the three BATMAN blocks in `ShaderManager.cpp:824-860`, `ShaderCache.cpp:669-794`, `ShaderCache.cpp:1450-1475`. Stock UE3 `FShader::Serialize` / `SerializeShaders` / `UShaderCache::Load` already match BM:AC exactly.
2. **Stream desync:** fix `operator<<(FArchive&, FMaterialUniformExpression*&)` at `MaterialShared.cpp:500-509` to never leave the cursor misplaced on unknown expression types — either call `Ref->Serialize` through a stub class, or require a per-expression size prefix, or register all BM3 expression types explicitly.
3. **Verify build config:** `WITH_D3D11_TESSELLATION == 1` (`MaterialShared.cpp:613-627` uses 13 TArrays only with this) and `VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE == 656` (`MaterialShader.cpp:2045-2070` gate must match BM3's).
4. **PixelShader Serialize ordering:** add `UpperSkyColor` and `LowerSkyColor` to the `TBasePassPixelShaderBaseType::Serialize` BATMAN path so `MotionBlurMask` reads its own bytes rather than the sky colours'. Delete the `DeferredRenderingParameters` read.
5. **VertexShader Serialize layouts:** rewrite `TBasePassVertexShader::Serialize` BATMAN path to match Gangland per policy combination — drop `HeightFogParameters` entirely, drop `FogVolumeParameters` for `FNoDensityPolicy`, drop the two `Dummy` reads, collapse `DOFPackedParameters0/1` into a single `FDOFShaderParameters(PackedParameters, MinMaxBlurClamp)`. Also swap the order of `LightMapPolicyType::VertexParametersType::Serialize` and `Ar << VertexFactoryParameters` so `LightMapScale` lands after `VertexFactoryParameters`.
6. **Trailing-dummy removal:** delete BATMAN dummy reads in `DepthRendering.cpp:48-57`, `FogVolumeRendering.h:500-508` (`TFogIntegralVertexShader`), `FogVolumeRendering.h:793-801` (`FFogVolumeApplyVertexShader`), `LightRendering.h:468-475` (`TLightVertexShader`). Gangland writes none of these.
7. **Parameter name fixes:**
   - `MaterialShader.cpp:665`: `"DecalNearFarPlaneDistance"` → `"DecalFarPlaneDistance"`.
   - `LightMapRendering.h:977, :1120`: `"APlus3DLightInfoPixel"` → `"APlus3DLightPixelInfo"`.
   - `LightMapRendering.h:950, :1076`: `"APlus3DLightInfoVertex"` → `"APlus3DLightVertexInfo"`.
   - `LightMapRendering.h:1074`: `"LightDirection"` → `"LightDirectionAndbDirectional"` (and start writing the `.w` flag).
   - `LightMapRendering.h:755`: either rename to `"LightDirectionAndbDirectional"` under BATMAN and fix `PointLightComponent.cpp:536-539`'s upload to be a direction + flag, or audit why our own shader expects a position in this slot.
8. **Remove the entire `LightMapLumaChannel` BATMAN block** (`LightMapRendering.h:317-319, 399-404, 409-411, 415-417, 549-558`) — the field doesn't exist in BM3 and the phantom FShaderParameter it injects into the pixel-param stream shifts every subsequent serialize.
9. **Missing `FMaterialVertexShaderParameters` members:** add `ObjectRotationParameter` (`"ObjectRotation"`) and `SmoothNormalsTextureParameter` (`"SmoothNormalsTexture"`) plus the Bind/Set plumbing (`MaterialShader.cpp:1144`, see BM3 `SetMesh` at `BmGame.exe.c:6973055-6973091`).
10. **Fog density policy set:** register `FRockAtmosDensityPolicy` + `FFogRockAtmosShaderParameters::Bind` (Gangland `BmGame.exe.c:7187554`) and drop `FConstantDensityPolicy`, `FLinearHalfspaceDensityPolicy`, `FConeDensityPolicy` under BATMAN (`BasePassRendering.cpp:60-65`).
11. **`TFogIntegralPixelShader::Serialize` BATMAN inversion:** drop `MaxDistanceParameter` serialization under BATMAN unconditionally. The current BATMAN guard is inverted and the non-BATMAN path writes it twice.
12. **Restructure `FAPlus3DLightLightMapPolicy`:** drop inheritance from `FDirectionalLightLightMapPolicy`; make parameter types single-field flat structs; switch `ElementDataType` to `const FAmbientPlus3DirectionalLightSceneInfo*` and wire `SetMesh` to read the 3+4 `FVector4`s from scene-info offsets +640 / +688 instead of hardcoding zeros.
13. **Temporarily disable the silent fallback** in `FMaterialShaderParameters::SetShader` (`MaterialShader.cpp:462-546`) and `FMaterial::GetShader` (`MaterialShared.cpp:6041-6056`) to surface exact `(shader type, parameter, index)` mismatches — these are masking whichever upstream wire-format bug is triggering first.
14. **Verify version constants** — `VER_ADD_NORMAL_PARAMETERS == 631`, `VER_ADD_TERRAINLAYERWEIGHT_PARAMETERS == 714`, `VER_UNIFORM_EXPRESSIONS_IN_SHADER_CACHE == 656`, `VER_FIXED_AUTO_SHADER_VERSIONING ≤ 796`, `VER_SHADER_CACHE_PRIORITY ≤ 805` — ideally all in a BM3-compat header.
15. **Re-enable `FAPlus3DLightLightMapPolicy` routing** in `ProcessBasePassMesh` (`BasePassRendering.h:1113-1118`) and remove the `AmbientColor=(0,0,0,1)` workaround at `BasePassRendering.h:313-320` once items 4 and 12 are in place.

### Items NOT a bug (worth noting so they don't get "fixed")

- `FShader` / `FShaderType::GetOutdatedTypes` byte layouts already match BM3.
- `FMaterialShaderMap::FindId` fuzzy-match fallback is necessary and correct.
- `FFogVolumeApplyPixelShader`, `TDepthOnlyPixelShader`, `FDepthDrawingPolicy`, `LightMapDensityRendering`, and `SphericalHarmonicLightComponent` are all clean.
- `FStaticParameterSet::Serialize` and `FMaterialUniformExpressionTextureParameter::Serialize` order is correct.
- `FMaterialShaderMap::Serialize` field order is correct (verify constants only).
- `bDiscardShaderSource` platform asymmetry (`ShaderManager.cpp:812-817`) is cook-time only.
- `FShaderCache::Load` compressed-cache BATMAN gate (`ShaderCache.cpp:313-319`) is dead code but harmless.
