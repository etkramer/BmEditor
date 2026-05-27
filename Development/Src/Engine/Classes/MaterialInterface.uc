/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class MaterialInterface extends Surface
	abstract
	forcescriptorder(true)
	native(Material);

enum EMaterialUsage
{
	MATUSAGE_SkeletalMesh,
	MATUSAGE_FracturedMeshes,
	MATUSAGE_ParticleSprites,
	MATUSAGE_BeamTrails,
	MATUSAGE_ParticleSubUV,
	MATUSAGE_SpeedTree,
	MATUSAGE_StaticLighting,
	MATUSAGE_GammaCorrection,
	MATUSAGE_LensFlare,
	MATUSAGE_InstancedMeshParticles,
	MATUSAGE_FluidSurface,
	MATUSAGE_Decals,
	MATUSAGE_MaterialEffect,
	MATUSAGE_MorphTargets,
	MATUSAGE_FogVolumes,
	MATUSAGE_VertexLighting,
	MATUSAGE_StaticModulatedShadows,
	MATUSAGE_PerVertexRockAtmosFog,
	MATUSAGE_LightEnvironments,
	MATUSAGE_StaticMesh,
	MATUSAGE_RadialBlur,
	MATUSAGE_InstancedMeshes,
	MATUSAGE_SplineMesh,
	MATUSAGE_ScreenDoorFade,
	MATUSAGE_APEXMesh,
	MATUSAGE_Terrain,
	MATUSAGE_Landscape,
};

struct native PhysicalMaterialColorPair
{
	var() color					Color;
	var() PhysicalMaterial		PhysMaterial;
};

/**
 * BM2 declares this struct on UMaterialInterface but doesn't expose it as a var.
 * Kept here only so MaterialInterface's script-visible struct list matches BM2.
 */
struct native LightmassMaterialInterfaceSettings
{
	var(Material)	bool		bCastShadowAsMasked;
	var(Material)	float		EmissiveBoost;
	var(Material)	float		DiffuseBoost;
	var				float		SpecularBoost;
	var(Material)	float		ExportResolutionScale;
	var(Material)	float		DistanceFieldPenumbraScale;
	var bool bOverrideCastShadowAsMasked;
	var bool bOverrideEmissiveBoost;
	var bool bOverrideDiffuseBoost;
	var bool bOverrideSpecularBoost;
	var bool bOverrideExportResolutionScale;
	var bool bOverrideDistanceFieldPenumbraScale;

	structdefaultproperties
	{
		bCastShadowAsMasked=false
		EmissiveBoost=1.0
		DiffuseBoost=1.0
		SpecularBoost=1.0
		ExportResolutionScale=1.0
		DistanceFieldPenumbraScale=1.0
	}
};

/** A fence to track when the primitive is no longer used as a parent */
var native const transient RenderCommandFence_Mirror ParentRefFence{FRenderCommandFence};

/** Per-texel physical material mask texture. */
var() RPhysicalMaterialTexture PhysicalMaterialTexture;

/** Cached list of physical materials referenced by the physical material texture. */
var transient array<PhysicalMaterial> PhysicalMaterialLookup;

/** Mapping of mask colors to physical materials. */
var array<PhysicalMaterialColorPair> PhysMaterialColourMap;

/** Physical material to use for this graphics material. Used for sounds, effects etc. */
var(PhysicalMaterial) PhysicalMaterial PhysMaterial;

/** The mesh used by the material editor to preview the material. */
var() editoronly string PreviewMesh;

/** Unique ID for this material, used for caching during distributed lighting */
var private editoronly const Guid LightingGuid;

cpptext
{
	/**
	 * Get the material which this is an instance of.
	 * Warning - This is platform dependent!  Do not call GetMaterial(GCurrentMaterialPlatform) and save that reference,
	 * as it will be different depending on the current platform.  Instead call GetMaterial(MSP_BASE) to get the base material and save that.
	 * When getting the material for rendering/checking usage, GetMaterial(GCurrentMaterialPlatform) is fine.
	 *
	 * @param Platform - The platform to get material for.
	 */
	virtual class UMaterial* GetMaterial(EMaterialShaderPlatform Platform = GCurrentMaterialPlatform) PURE_VIRTUAL(UMaterialInterface::GetMaterial,return NULL;);

	/**Fix up for deprecated properties*/
	virtual void PostLoad ();

	/**
	* Tests this material for dependency on a given material.
	* @param	TestDependency - The material to test for dependency upon.
	* @return	True if the material is dependent on TestDependency.
	*/
	virtual UBOOL IsDependent(UMaterialInterface* TestDependency) { return TestDependency == this; }

	/**
	* Returns a pointer to the FMaterialRenderProxy used for rendering.
	*
	* @param	Selected	specify TRUE to return an alternate material used for rendering this material when part of a selection
	*						@note: only valid in the editor!
	*
	* @return	The resource to use for rendering this material instance.
	*/
	virtual FMaterialRenderProxy* GetRenderProxy(UBOOL Selected, UBOOL bHovered=FALSE) const PURE_VIRTUAL(UMaterialInterface::GetRenderProxy,return NULL;);

	/**
	* Returns a pointer to the physical material used by this material instance.
	* @return The physical material.
	*/
	virtual UPhysicalMaterial* GetPhysicalMaterial() const PURE_VIRTUAL(UMaterialInterface::GetPhysicalMaterial,return NULL;);

	/** Returns the textures used to render this material for the given platform. */
	virtual void GetUsedTextures(TArray<UTexture*> &OutTextures, EMaterialShaderPlatform Platform = MSP_BASE, UBOOL bAllPlatforms = FALSE)
		PURE_VIRTUAL(UMaterialInterface::GetUsedTextures,);

	/**
	* Checks whether the specified texture is needed to render the material instance.
	* @param Texture	The texture to check.
	* @return UBOOL - TRUE if the material uses the specified texture.
	*/
	virtual UBOOL UsesTexture(const UTexture* Texture) PURE_VIRTUAL(UMaterialInterface::UsesTexture,return FALSE;);

	/**
	 * Overrides a specific texture (transient)
	 *
	 * @param InTextureToOverride The texture to override
	 * @param OverrideTexture The new texture to use
	 */
	virtual void OverrideTexture( UTexture* InTextureToOverride, UTexture* OverrideTexture ) PURE_VIRTUAL(UMaterialInterface::OverrideTexture,return;);

	/**
	 * Checks if the material can be used with the given usage flag.
	 * If the flag isn't set in the editor, it will be set and the material will be recompiled with it.
	 * @param Usage - The usage flag to check
	 * @return UBOOL - TRUE if the material can be used for rendering with the given type.
	 */
	virtual UBOOL CheckMaterialUsage(EMaterialUsage Usage) PURE_VIRTUAL(UMaterialInterface::CheckMaterialUsage,return FALSE;);

	/**
	* Allocates a new material resource
	* @return	The allocated resource
	*/
	virtual FMaterialResource* AllocateResource() PURE_VIRTUAL(UMaterialInterface::AllocateResource,return NULL;);

	/**
	 * Gets the static permutation resource if the instance has one
	 * @return - the appropriate FMaterialResource if one exists, otherwise NULL
	 */
	virtual FMaterialResource* GetMaterialResource(EMaterialShaderPlatform Platform = GCurrentMaterialPlatform) { return NULL; }

	/** BM2 has no mobile rendering path — always return NULL. */
	virtual UTexture* GetMobileTexture(const INT /* EMobileTextureUnit */ MobileTextureUnit) { return NULL; }

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 *
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	/**
	 * Compiles a FMaterialResource on the given platform with the given static parameters
	 */
	virtual UBOOL CompileStaticPermutation(
		FStaticParameterSet* StaticParameters,
		FMaterialResource* StaticPermutation,
		EShaderPlatform Platform,
		EMaterialShaderPlatform MaterialPlatform,
		UBOOL bFlushExistingShaderMaps,
		UBOOL bDebugDump)
		PURE_VIRTUAL(UMaterialInterface::CompileStaticPermutation,return FALSE;);

	virtual UBOOL GetStaticSwitchParameterValue(FName ParameterName,UBOOL &OutValue,FGuid &OutExpressionGuid)
		PURE_VIRTUAL(UMaterialInterface::GetStaticSwitchParameterValue,return FALSE;);

	virtual UBOOL GetStaticComponentMaskParameterValue(FName ParameterName, UBOOL &R, UBOOL &G, UBOOL &B, UBOOL &A, FGuid &OutExpressionGuid)
		PURE_VIRTUAL(UMaterialInterface::GetStaticComponentMaskParameterValue,return FALSE;);

	virtual UBOOL GetNormalParameterValue(FName ParameterName, BYTE& OutCompressionSettings, FGuid &OutExpressionGuid)
		PURE_VIRTUAL(UMaterialInterface::GetNormalParameterValue,return FALSE;);

	virtual UBOOL GetTerrainLayerWeightParameterValue(FName ParameterName, INT& OutWeightmapIndex, FGuid &OutExpressionGuid)
		PURE_VIRTUAL(UMaterialInterface::GetTerrainLayerWeightParameterValue,return FALSE;);

	virtual UBOOL IsFallbackMaterial() { return FALSE; }

	/** @return The material's view relevance. */
	FMaterialViewRelevance GetViewRelevance();

	INT GetWidth() const;
	INT GetHeight() const;

	// USurface interface
	virtual FLOAT GetSurfaceWidth() const { return GetWidth(); }
	virtual FLOAT GetSurfaceHeight() const { return GetHeight(); }

	// UObject interface
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void Serialize(FArchive& Ar);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	FMaterialShaderMap* SerializeShaderMap(FMaterialShaderMap* InShaderMap, FArchive& Ar);

	virtual UBOOL UpdateLightmassTextureTracking() { return FALSE; }

	/**
	 *	Get all of the textures in the expression chain for the given property.
	 */
	virtual UBOOL GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,  TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
		PURE_VIRTUAL(UMaterialInterface::GetTexturesInPropertyChain,return FALSE;);

	/** Lightmass accessor stubs (BM2 has no per-material lightmass override data). */
	virtual UBOOL GetCastShadowAsMasked() const { return FALSE; }
	virtual FLOAT GetEmissiveBoost() const { return 1.0f; }
	virtual FLOAT GetDiffuseBoost() const { return 1.0f; }
	virtual FLOAT GetSpecularBoost() const { return 1.0f; }
	virtual FLOAT GetExportResolutionScale() const { return 1.0f; }
	virtual FLOAT GetDistanceFieldPenumbraScale() const { return 1.0f; }

	/**
	 * Returns the lookup texture to be used in the physical material mask.
	 */
	virtual UTexture2D* GetPhysicalMaterialMaskTexture() const { return NULL; }

	/**
	 * Returns the black physical material to be used in the physical material mask.
	 */
	virtual UPhysicalMaterial* GetBlackPhysicalMaterial() const { return NULL; }

	/**
	 * Returns the white physical material to be used in the physical material mask.
	 */
	virtual UPhysicalMaterial* GetWhitePhysicalMaterial() const { return NULL; }

	/**
	 * Returns the UV channel that should be used to look up physical material mask information
	 */
	virtual INT GetPhysMaterialMaskUVChannel() const { return -1; }

	/**
	 * Returns True if this material has a valid physical material mask setup.
 	 */
	UBOOL HasValidPhysicalMaterialMask() const;

	/**
	 * Determines the texel on the physical material mask that was hit and returns the physical material corresponding to hit texel's color
	 */
	UPhysicalMaterial* DetermineMaskedPhysicalMaterialFromUV( const FVector2D& HitUV ) const;
}

native final noexport function Material GetMaterial();

native final noexport function PhysicalMaterial GetPhysicalMaterial() const;

native function bool GetParameterDesc(name ParameterName, out String OutDesc);
native function bool GetFontParameterValue(name ParameterName,out font OutFontValue, out int OutFontPage);
native function bool GetScalarParameterValue(name ParameterName, out float OutValue);
native function bool GetScalarCurveParameterValue(name ParameterName, out InterpCurveFloat OutValue);
native function bool GetTextureParameterValue(name ParameterName, out Texture OutValue);
native function bool GetVectorParameterValue(name ParameterName, out LinearColor OutValue);
native function bool GetVectorCurveParameterValue(name ParameterName, out InterpCurveVector OutValue);

native function SetForceMipLevelsToBeResident( bool OverrideForceMiplevelsToBeResident, bool bForceMiplevelsToBeResidentValue, float ForceDuration, optional int CinematicTextureGroups = 0 );

defaultproperties
{
}
