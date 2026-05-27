/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class Material extends MaterialInterface
	native(Material)
	hidecategories(object);

enum EParticleDownsampling
{
	PDS_Full,
	PDS_Half,
	PDS_Quarter,
};

enum MatLoadedPhysMaterial
{
	LPM_NoLoadedPhysMat,
};

struct MaterialInput
{
	var MaterialExpression	Expression;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
	var int					GCC64_Padding;
};

struct ColorMaterialInput extends MaterialInput
{
	var bool	UseConstant;
	var color	Constant;
};

struct ScalarMaterialInput extends MaterialInput
{
	var bool	UseConstant;
	var float	Constant;
};

struct VectorMaterialInput extends MaterialInput
{
	var bool	UseConstant;
	var vector	Constant;
};

struct Vector2MaterialInput extends MaterialInput
{
	var bool	UseConstant;
	var float	ConstantX,
				ConstantY;
};

var(PhysicalMaterial)	Texture2D			PhysMaterialMask;
var(PhysicalMaterial)	INT					PhysMaterialMaskUVChannel;
var(PhysicalMaterial)	PhysicalMaterial	BlackPhysicalMaterial;
var(PhysicalMaterial)	PhysicalMaterial	WhitePhysicalMaterial;

var(Translucency) nontransactional EParticleDownsampling NvidiaParticleDownsampling;

var() MatLoadedPhysMaterial PhysMaterialOverrideDropDown;

var() EBlendMode BlendMode;
var() EMaterialLightingModel LightingModel;

var() bool PhysMaterialOverrideDropDownUPDATELIST;

var() bool EnableSubsurfaceScattering;
var() bool TwoSided;
var(Translucency) bool TwoSidedSeparatePass;

var(RS_Options) bool bSpecularBlinnPhong;
var(RS_Options) bool bSpecularConserveEnergy;
var(RS_Options) bool bSpecularMaskByShading;
var(RS_Options) bool SortWithSceneTextureSampleMaterials;
var(RS_Options) bool bDisableTwoSidedLighting;

var(Translucency) bool bDisableDepthTest;
var(Translucency) bool bAllowFog;
var(Translucency) bool bTranslucencyReceiveDominantShadowsFromStatic;
var(Translucency) bool bTranslucencyInheritDominantShadowsFromOpaque;
var(Translucency) bool bAllowTranslucencyDoF;
var(Translucency) bool bUseOneLayerDistortion;
var(Translucency) bool bUseLitTranslucencyDepthPass;
var(Translucency) bool bUseLitTranslucencyPostRenderDepthPass;
var(Translucency) bool bCastLitTranslucencyShadowAsMasked;

var(Usage) editoronly const bool bLockUsageFlags;
var(MutuallyExclusiveUsage) const bool bUsedAsLightFunction;
var(MutuallyExclusiveUsage) const bool bUsedWithFogVolumes;
var(Usage) const bool bUsedWithVertexLighting;
var(Usage) const bool bUsedWithStaticModulatedShadows;
var(Usage) bool bUsedWithPerVertexRockAtmosFog;
var(Usage) const bool bUsedWithLightEnvironment;
var(Usage) bool bRecievesDynamicDirectionalLights;
var(Usage) bool bRecievesDynamicSpotLights;
var(Usage) bool bRecievesDynamicPointLights;
var(Usage) bool bUsedWithStaticMesh;
var const editconst bool bUsedAsSpecialEngineMaterial;
var(Usage) const bool bUsedWithSkeletalMesh;
var(Usage) const bool bUsedWithTerrain;
var(Usage) const bool bUsedWithLandscape;
var(Usage) const bool bUsedWithFracturedMeshes;
var		   const bool bUsedWithParticleSystem;
var(Usage) const bool bUsedWithParticleSprites;
var(Usage) const bool bUsedWithBeamTrails;
var(Usage) const bool bUsedWithParticleSubUV;
var(Usage) const bool bUsedWithSpeedTree;
var(Usage) const bool bUsedWithStaticLighting;
var(Usage) const bool bUsedWithLensFlare;
var(Usage) const bool bUsedWithGammaCorrection;
var(Usage) const bool bUsedWithInstancedMeshParticles;
var(Usage) const bool bUsedWithFluidSurfaces;
var(Usage) const bool bUsedWithMaterialEffect;
var(Usage) const bool bUsedWithMorphTargets;
var(Usage) const bool bUsedWithRadialBlur;
var(Usage) const bool bUsedWithInstancedMeshes;
var(Usage) const bool bUsedWithSplineMeshes;
var(Usage) const bool bUsedWithAPEXMeshes;
var(Usage) const bool bUsedWithApexSprites;
var(Usage) const bool bUsedWithScreenDoorFade;
var(Usage) const bool bUsedWithD3D11Tessellation;
var(D3D11) const bool bUsedWithTessellationFlat;
var(D3D11) const bool bUsedWithTessellationPN;
var(D3D11) const bool bUsedWithTessellationPhong;
var(D3D11) const bool bUsedWithTessellationMeshDicing;
var(D3D11) const bool bUsedWithTessellationWaterTightNormals;

var(D3D11) bool bUseImageBasedReflections;

var() bool Wireframe;
var() bool bPerPixelCameraVector;
var() bool bAllowLightmapSpecular;
var() bool CanStripNormalsAndTangents;
var() bool CanStripVertexColours;
var bool UseFastLODRendering;

var deprecated bool bIsFallbackMaterial;

var private bool bUsesDistortion;
var private bool bIsMasked;
var transient duplicatetransient private bool bIsPreviewMaterial;

var ColorMaterialInput		DiffuseColor;
var ScalarMaterialInput		DiffusePower;
var ColorMaterialInput		SpecularColor;
var ScalarMaterialInput		SpecularPower;
var VectorMaterialInput		Normal;

var ColorMaterialInput		EmissiveColor;

var ScalarMaterialInput		Opacity;
var ScalarMaterialInput		OpacityMask;

var() float OpacityMaskClipValue;
var() float OpacityMaskClipValuePostDepth;

var Vector2MaterialInput	Distortion;

var ColorMaterialInput		CustomLighting;
var ColorMaterialInput		CustomSkylightDiffuse;
var VectorMaterialInput		AnisotropicDirection;

var ScalarMaterialInput		FresnelMin;
var ScalarMaterialInput		FresnelExponent;

var ColorMaterialInput		LightWrapping;

var VectorMaterialInput		SSSNormal;
var ColorMaterialInput		SSSMask;
var ScalarMaterialInput		SSSRadius;

var ColorMaterialInput		SpecularColor2;
var ScalarMaterialInput		SpecularPower2;

var ScalarMaterialInput		TwoSidedLightingMask;
var ColorMaterialInput		TwoSidedLightingColor;

var VectorMaterialInput		WorldPositionOffset;
var VectorMaterialInput		WorldDisplacement;
var ScalarMaterialInput		TangentDisplacement;

var ColorMaterialInput		SubsurfaceInscatteringColor;
var ColorMaterialInput		SubsurfaceAbsorptionColor;
var ScalarMaterialInput		SubsurfaceScatteringRadius;

var(RS_Options) const LinearColor SSSColourDiffuse;
var(RS_Options) const LinearColor SSSColourEpidermal;
var(RS_Options) const LinearColor SSSColourSubdermal;
var(RS_Options) const LinearColor SSSColourTransmittance;

var() int MaxBonesPerBatch;

var const native editconst pointer MaterialResources[2]{FMaterialResource};

var const native editconst pointer DefaultMaterialInstances[3]{class FDefaultMaterialInstance};

var int		EditorX,
			EditorY,
			EditorPitch,
			EditorYaw;

var array<MaterialExpression>			Expressions;

var editoronly array<MaterialExpressionComment>	EditorComments;

var editoronly array<MaterialExpressionCompound> EditorCompounds;

var native map{FName, TArray<UMaterialExpression*>} EditorParameters;

var private editoronly const array<texture> ReferencedTextures;

var private editoronly const array<guid> ReferencedTextureGuids;

cpptext
{
	// Constructor.
	UMaterial();

	/** @return TRUE if the material uses distortion */
	UBOOL HasDistortion() const;
	/** @return TRUE if the material uses the scene color texture */
	UBOOL UsesSceneColor() const;

	/**
	 * Allocates a material resource off the heap to be stored in MaterialResource.
	 */
	virtual FMaterialResource* AllocateResource();

	/** Returns the textures used to render this material for the given platform. */
	virtual void GetUsedTextures(TArray<UTexture*> &OutTextures, EMaterialShaderPlatform Platform = MSP_BASE, UBOOL bAllPlatforms = FALSE);

	virtual UBOOL UsesTexture(const UTexture* Texture);

	virtual void OverrideTexture( UTexture* InTextureToOverride, UTexture* OverrideTexture );

private:

	/** Sets the value associated with the given usage flag. */
	void SetUsageByFlag(EMaterialUsage Usage, UBOOL NewValue);

public:

	/** Gets the name of the given usage flag. */
	FString GetUsageName(EMaterialUsage Usage) const;

	/** Gets the value associated with the given usage flag. */
	UBOOL GetUsageByFlag(EMaterialUsage Usage) const;

	virtual UBOOL CheckMaterialUsage(EMaterialUsage Usage);

	UBOOL SetMaterialUsage(UBOOL &bNeedsRecompile, EMaterialUsage Usage);

	template<typename ExpressionType>
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);

	void GetAllVectorParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllScalarParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllTextureParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllFontParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllStaticSwitchParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllStaticComponentMaskParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllNormalParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);
	void GetAllTerrainLayerWeightParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds);

	template<typename ExpressionType>
	ExpressionType* FindExpressionByGUID(const FGuid &InGUID)
	{
		ExpressionType* Result = NULL;

		for(INT ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
		{
			ExpressionType* ExpressionPtr =
				Cast<ExpressionType>(Expressions(ExpressionIndex));

			if(ExpressionPtr && ExpressionPtr->ExpressionGUID.IsValid() && ExpressionPtr->ExpressionGUID==InGUID)
			{
				Result = ExpressionPtr;
				break;
			}
		}

		return Result;
	}

	// UMaterialInterface interface.

	virtual UMaterial* GetMaterial(EMaterialShaderPlatform Platform = GCurrentMaterialPlatform);
    virtual UBOOL GetParameterDesc(FName ParameterName, FString& OutDesc);
    virtual UBOOL GetVectorParameterValue(FName ParameterName,FLinearColor& OutValue);
    virtual UBOOL GetScalarParameterValue(FName ParameterName,FLOAT& OutValue);
    virtual UBOOL GetTextureParameterValue(FName ParameterName,class UTexture*& OutValue);
	virtual UBOOL GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue,INT& OutFontPage);

	virtual UBOOL GetStaticSwitchParameterValue(FName ParameterName,UBOOL &OutValue,FGuid &OutExpressionGuid);

	virtual UBOOL GetStaticComponentMaskParameterValue(FName ParameterName, UBOOL &R, UBOOL &G, UBOOL &B, UBOOL &A, FGuid &OutExpressionGuid);

	virtual UBOOL GetNormalParameterValue(FName ParameterName, BYTE& OutCompressionSettings, FGuid &OutExpressionGuid);

	virtual UBOOL GetTerrainLayerWeightParameterValue(FName ParameterName, INT& OutWeightmapIndex, FGuid &OutExpressionGuid);

	virtual FMaterialRenderProxy* GetRenderProxy(UBOOL Selected, UBOOL bHovered=FALSE) const;
	virtual UPhysicalMaterial* GetPhysicalMaterial() const;

	UBOOL CompileStaticPermutation(
		FStaticParameterSet* StaticParameters,
		FMaterialResource* StaticPermutation,
		EShaderPlatform Platform,
		EMaterialShaderPlatform MaterialPlatform,
		UBOOL bFlushExistingShaderMaps,
		UBOOL bDebugDump);

	void CacheResourceShaders(EShaderPlatform Platform, UBOOL bFlushExistingShaderMaps=FALSE, UBOOL bForceAllPlatforms=FALSE);

private:
	virtual void FlushResourceShaderMaps();

public:
	virtual FMaterialResource* GetMaterialResource(EMaterialShaderPlatform Platform = GCurrentMaterialPlatform);

	virtual FLOAT GetSurfaceWidth() const;
	virtual FLOAT GetSurfaceHeight() const;

	// UObject interface.
	void PreSave();

	virtual void AddReferencedObjects(TArray<UObject*>& ObjectArray);
	virtual void Serialize(FArchive& Ar);
	virtual void PostDuplicate();
	virtual void PostLoad();
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();

	virtual INT GetResourceSize();

	void RemoveExpressions(UBOOL bRemoveAllExpressions=FALSE);

	UBOOL IsFallbackMaterial() { return bIsFallbackMaterial_DEPRECATED; }

	static void UpdateMaterialShaders(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush);

	virtual UBOOL AddExpressionParameter(UMaterialExpression* Expression);

	virtual UBOOL RemoveExpressionParameter(UMaterialExpression* Expression);

	virtual void PropagateExpressionParameterChanges(UMaterialExpression* Parameter);

	virtual void UpdateExpressionParameterName(UMaterialExpression* Expression);

	virtual void BuildEditorParameterList();

	virtual UBOOL HasDuplicateParameters(UMaterialExpression* Expression);

	virtual UBOOL HasDuplicateDynamicParameters(UMaterialExpression* Expression);

	virtual void UpdateExpressionDynamicParameterNames(UMaterialExpression* Expression);

	static UBOOL GetExpressionParameterName(UMaterialExpression* Expression, FName& OutName);

	static UBOOL CopyExpressionParameters(UMaterialExpression* Source, UMaterialExpression* Destination);

	static UBOOL IsParameter(UMaterialExpression* Expression);

	static UBOOL IsDynamicParameter(UMaterialExpression* Expression);

	inline INT GetNumEditorParameters() const
	{
		return EditorParameters.Num();
	}

	inline void EmptyEditorParameters()
	{
		EditorParameters.Empty();
	}

	virtual UTexture2D* GetPhysicalMaterialMaskTexture() const { return PhysMaterialMask; }

	virtual UPhysicalMaterial* GetBlackPhysicalMaterial() const { return BlackPhysicalMaterial; }

	virtual UPhysicalMaterial* GetWhitePhysicalMaterial() const { return WhitePhysicalMaterial; }

	virtual INT GetPhysMaterialMaskUVChannel() const { return PhysMaterialMaskUVChannel; }

protected:
	void SetStaticParameterOverrides(const FStaticParameterSet* Permutation);

	void ClearStaticParameterOverrides();

public:
	static const TCHAR* GetMaterialLightingModelString(EMaterialLightingModel InMaterialLightingModel);
	static EMaterialLightingModel GetMaterialLightingModelFromString(const TCHAR* InMaterialLightingModelStr);
	static const TCHAR* GetBlendModeString(EBlendMode InBlendMode);
	static EBlendMode GetBlendModeFromString(const TCHAR* InBlendModeStr);

	virtual UBOOL UpdateLightmassTextureTracking();

	FExpressionInput* GetExpressionInputForProperty(EMaterialProperty InProperty);

	virtual UBOOL GetAllReferencedExpressions(TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet);

	virtual UBOOL GetExpressionsInPropertyChain(EMaterialProperty InProperty,
		TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet);

	virtual UBOOL GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,
		TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet);

protected:
	virtual UBOOL RecursiveGetExpressionChain(UMaterialExpression* InExpression, TArray<FExpressionInput*>& InOutProcessedInputs,
		TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet);

	void RecursiveUpdateRealtimePreview(UMaterialExpression* InExpression, TArray<UMaterialExpression*>& InOutExpressionsToProcess);


	friend class FLightmassMaterialProxy;
};

defaultproperties
{
	BlendMode=BLEND_Opaque
	DiffuseColor=(Constant=(R=128,G=128,B=128))
	DiffusePower=(Constant=1.0)
	SpecularColor=(Constant=(R=128,G=128,B=128))
	SpecularPower=(Constant=15.0)
	Distortion=(ConstantX=0,ConstantY=0)
	Opacity=(Constant=1)
	OpacityMask=(Constant=1)
	OpacityMaskClipValue=0.3333
	OpacityMaskClipValuePostDepth=0.3333
	TwoSidedLightingColor=(Constant=(R=255,G=255,B=255))
	SubsurfaceInscatteringColor=(Constant=(R=255,G=255,B=255))
	SubsurfaceAbsorptionColor=(Constant=(R=230,G=200,B=200))
	SSSColourDiffuse=(R=0.225,G=0.270,B=0.300,A=1.0)
	SSSColourEpidermal=(R=0.500,G=0.425,B=0.300,A=1.0)
	SSSColourSubdermal=(R=0.380,G=0.200,B=0.080,A=1.0)
	SSSColourTransmittance=(R=0.350,G=0.050,B=0.050,A=1.0)
	bAllowFog=TRUE
	bUsedWithStaticMesh=TRUE
	bAllowLightmapSpecular=TRUE
	PhysMaterialMaskUVChannel=-1
}
