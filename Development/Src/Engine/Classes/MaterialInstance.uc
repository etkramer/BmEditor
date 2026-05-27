/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class MaterialInstance extends MaterialInterface
	abstract
	native(Material);


var() const MaterialInterface Parent;

var(PhysicalMaterialMask)	Texture2D	PhysMaterialMask;
var(PhysicalMaterialMask)	INT	PhysMaterialMaskUVChannel;
var(PhysicalMaterialMask)	PhysicalMaterial BlackPhysicalMaterial;
var(PhysicalMaterialMask)	PhysicalMaterial WhitePhysicalMaterial;

var bool bHasStaticPermutationResource;

var native transient bool bStaticPermutationDirty;

var private const native bool ReentrantFlag;

var private const transient bool bNeedsMaterialFlattening;

var const native editconst pointer StaticParameters[2]{FStaticParameterSet};

var const native editconst pointer StaticPermutationResources[2]{FMaterialResource};

var const native editconst pointer Resources[3]{class FMaterialInstanceResource};

var private editoronly const array<texture> ReferencedTextures;

var private editoronly const array<guid> ReferencedTextureGuids;

var private const Guid ParentLightingGuid;

cpptext
{
	// Constructor.
	UMaterialInstance();

	FMaterialResource* AllocateResource();

	virtual void InitResources();

	virtual FMaterialResource* GetMaterialResource(EMaterialShaderPlatform Platform = GCurrentMaterialPlatform);

	// UMaterialInterface interface.

	virtual UMaterial* GetMaterial(EMaterialShaderPlatform Platform = GCurrentMaterialPlatform);

	virtual void GetUsedTextures(TArray<UTexture*> &OutTextures, EMaterialShaderPlatform Platform = MSP_BASE, UBOOL bAllPlatforms = FALSE);

	virtual UBOOL UsesTexture(const UTexture* Texture);

	virtual void OverrideTexture( UTexture* InTextureToOverride, UTexture* OverrideTexture );

	virtual UBOOL CheckMaterialUsage(EMaterialUsage Usage);

	virtual UBOOL GetStaticSwitchParameterValue(FName ParameterName,UBOOL &OutValue,FGuid &OutExpressionGuid);

	virtual UBOOL GetStaticComponentMaskParameterValue(FName ParameterName, UBOOL &R, UBOOL &G, UBOOL &B, UBOOL &A,FGuid &OutExpressionGuid);

	virtual UBOOL GetNormalParameterValue(FName ParameterName, BYTE& OutCompressionSettings, FGuid &OutExpressionGuid);

	virtual UBOOL GetTerrainLayerWeightParameterValue(FName ParameterName, INT& OutWeightmapIndex, FGuid &OutExpressionGuid);

	virtual UBOOL IsDependent(UMaterialInterface* TestDependency);
	virtual FMaterialRenderProxy* GetRenderProxy(UBOOL Selected, UBOOL bHovered=FALSE) const;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const;

	void GetStaticParameterValues(FStaticParameterSet* StaticParameters);

	UBOOL SetStaticParameterValues(const FStaticParameterSet* EditorParameters);

	virtual void CheckStaticParameterValues(FStaticParameterSet* EditorParameters);

	void UpdateStaticPermutation();

	void InitStaticPermutation();

	void CacheResourceShaders(EShaderPlatform ShaderPlatform, UBOOL bFlushExistingShaderMaps=FALSE, UBOOL bForceAllPlatforms=FALSE, UBOOL bDebugDump=FALSE);

	UBOOL CompileStaticPermutation(
		FStaticParameterSet* Permutation,
		FMaterialResource* StaticPermutation,
		EShaderPlatform Platform,
		EMaterialShaderPlatform MaterialPlatform,
		UBOOL bFlushExistingShaderMaps,
		UBOOL bDebugDump);

	void AllocateStaticPermutations();

	void ReleaseStaticPermutations();

	virtual FLOAT GetSurfaceWidth() const;
	virtual FLOAT GetSurfaceHeight() const;

	// UObject interface.
	virtual void AddReferencedObjects(TArray<UObject*>& ObjectArray);
	void PreSave();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();

	virtual void UpdateParameterNames();

#if !FINAL_RELEASE
	void CheckSafeToModifyInGame(const TCHAR* FuncName) const;
#endif

	virtual UBOOL UpdateLightmassTextureTracking();

	virtual UBOOL GetCastShadowAsMasked() const;
	virtual FLOAT GetEmissiveBoost() const;
	virtual FLOAT GetDiffuseBoost() const;
	virtual FLOAT GetSpecularBoost() const;
	virtual FLOAT GetExportResolutionScale() const;
	virtual FLOAT GetDistanceFieldPenumbraScale() const;

	virtual UBOOL GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,
		TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet);

	virtual UTexture2D* GetPhysicalMaterialMaskTexture() const;

	virtual UPhysicalMaterial* GetBlackPhysicalMaterial() const;

	virtual UPhysicalMaterial* GetWhitePhysicalMaterial() const;

	virtual INT GetPhysMaterialMaskUVChannel() const;
};

// SetParent - Updates the parent.

native function SetParent(MaterialInterface NewParent);

// Set*ParameterValue - Updates the entry in ParameterValues for the named parameter, or adds a new entry.

native function SetVectorParameterValue(name ParameterName, const out LinearColor Value);
native function SetScalarParameterValue(name ParameterName, float Value);
native function SetScalarCurveParameterValue(name ParameterName, const out InterpCurveFloat Value);
native function SetTextureParameterValue(name ParameterName, Texture Value);

native function SetFontParameterValue(name ParameterName, Font FontValue, int FontPage);

/** Removes all parameter values */
native function ClearParameterValues();

native function bool IsInMapOrTransientPackage() const;

defaultproperties
{
	bHasStaticPermutationResource=False
	PhysMaterialMaskUVChannel=-1
}
