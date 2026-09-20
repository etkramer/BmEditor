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

struct MaterialInput
{
	var MaterialExpression	Expression;
	// BM
	var int					OutputIndex;
	var string				InputName;
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

// BM
struct native MaterialFunctionInfo
{
	var guid			StateId;
	var MaterialFunction Function;
};

var(PhysicalMaterialMask)	Texture2D			PhysMaterialMask;
var(PhysicalMaterialMask)	PhysicalMaterial	BlackPhysicalMaterial;
var(PhysicalMaterialMask)	PhysicalMaterial	WhitePhysicalMaterial;
var(PhysicalMaterialMask)	INT					PhysMaterialMaskUVChannel;

var(Translucency) notforconsole EParticleDownsampling NvidiaParticleDownsampling;

var() EBlendMode BlendMode;
var() EMaterialLightingModel LightingModel;

var(RS_Decal) EDecalPriority DecalPriority;
var(RS_Decal) EDecalDrawMode DecalDrawMode;

var(D3D11) const EMaterialTessellationMode D3D11TessellationMode;

var ColorMaterialInput		DiffuseColor;
var ScalarMaterialInput		DiffusePower;
var ColorMaterialInput		SpecularColor;
var ColorMaterialInput		SpecularColor2;
var ScalarMaterialInput		SpecularPower;
var ScalarMaterialInput		SpecularPower2;
var VectorMaterialInput		Normal;

var ColorMaterialInput		EmissiveColor;

var ScalarMaterialInput		Opacity;
var ScalarMaterialInput		OpacityMask;

var() float OpacityMaskClipValue;
var() float OpacityMaskClipValuePostDepth;

var float ShadowDepthBias;

var Vector2MaterialInput	Distortion;

var ColorMaterialInput		CustomLighting;
var ColorMaterialInput		CustomSkylightDiffuse;
var VectorMaterialInput		AnisotropicDirection;

var ColorMaterialInput		SSSColor;
var ScalarMaterialInput		SSSRadius;
var ScalarMaterialInput		MetalMask;
var Vector2MaterialInput	BlurDirection;
var ColorMaterialInput		FaceWorksDeepScatterColor;

var ScalarMaterialInput		TwoSidedLightingMask;
var ColorMaterialInput		TwoSidedLightingColor;

var VectorMaterialInput		WorldPositionOffset;
var VectorMaterialInput		PostWorldPositionOffset;
var VectorMaterialInput		WorldDisplacement;
var deprecated ScalarMaterialInput	TangentDisplacement;
var ScalarMaterialInput		TessellationMultiplier;

var ColorMaterialInput		SubsurfaceInscatteringColor;
var ColorMaterialInput		SubsurfaceAbsorptionColor;
var ScalarMaterialInput		SubsurfaceScatteringRadius;

var() bool bIgnoreMissingLODFadeWhenUsedInLODs;
var(D3D11) bool EnableSubsurfaceScattering;
var(D3D11) bool bEnableMaskedAntialiasing;
var() bool TwoSided;
var(Translucency) bool TwoSidedSeparatePass;

var(RS_Options) bool bThinBackScattering;
var deprecated bool bDeferredCoverageTransparency;
var(RS_Options) bool bHairForwardLighting;
var(RS_Options) bool bExpensiveForwardLighting;
var(RS_Options) bool bVertexOffsetBeforeSkinning;
var(RS_Options) bool bVertexOffsetAfterProjection;
var(RS_Options) bool bVolumeLighting;
var(RS_Options) bool bVolumeLightingPerPixel;
var(RS_Options) bool SortWithSceneTextureSampleMaterials;
var(RS_Options) bool bDisableTwoSidedLighting;
var(RS_Options) bool WorldNormalMap;
var(RS_Options) bool bDeriveTangentSpace;
var(RS_Options) bool bDeriveNormals;
var(RS_Options) bool bParticleMotionBlur;

var(RS_ViewModes) bool bRenderAsPointCloud;
var bool bVisibleAgainstStaticChannel;
var(RS_PointCloudOptions) bool bGenerateFakeNormals;
var(RS_PointCloudOptions) bool bDoPointCloudFalloff;
var(RS_PointCloudOptions) bool bDoPointCloudFalloffMin;
var(RS_PointCloudOptions) bool bPointCloudTransitionAsAlpha;
var(RS_MapPointCloudOptions) bool bSupportsMapViewPointCloud;
var(RS_MapPointCloudOptions) bool bUseMatColorInMapViewPointCloud;
var(RS_MapPointCloudOptions) bool bFadeAgainstOccludedDepth;

var(RS_Options) bool ReflectionsOnTranslucency;
var(RS_Options) bool XrayVisualsOnTranslucency;

var(RS_Decal) bool DisableDepth;
var(RS_Decal) bool DisableDiffuse;
var(RS_Decal) bool DisableEmissive;
var(RS_Decal) bool DisableMetalness;
var(RS_Decal) bool DisableNormals;
var(RS_Decal) bool DisableReflectivity;
var(RS_Decal) bool DisableRoughness;

var(Translucency) bool bDisableDepthTest;
var(Translucency) bool bDisableDepthWrite;
var(Translucency) bool bSceneTextureRenderBehindTranslucency;
var(Translucency) bool bExpensiveDrawBehindAllOtherTranslucency;
var(Translucency) bool bAllowFog;
var(Translucency) bool bAllowFogPerPixel;
var deprecated bool bTranslucencyReceiveDominantShadowsFromStatic;
var deprecated bool bTranslucencyInheritDominantShadowsFromOpaque;
var deprecated bool bAllowTranslucencyDoF;
var(Translucency) bool bUseOneLayerDistortion;
var(Translucency) bool bUseLitTranslucencyDepthPass;
var(Translucency) bool bUseLitTranslucencyPostRenderDepthPass;
var(Translucency) bool bCastLitTranslucencyShadowAsMasked;

var(Usage) editoronly const bool bLockUsageFlags;
var(MutuallyExclusiveUsage) const bool bUsedAsLightFunction;
var(MutuallyExclusiveUsage) const bool bUsedWithFogVolumes;
var(Usage) bool bUsedWithPerVertexRockAtmosFog;
var const transient bool bUsedWithLightEnvironment;
var(Usage) bool bUsedWithStaticMesh;
var const duplicatetransient bool bUsedAsSpecialEngineMaterial;
var(Usage) const bool bUsedWithSkeletalMesh;
var const transient bool bUsedWithTerrain;
var const transient bool bUsedWithLandscape;
var(Usage) const bool bUsedWithFracturedMeshes;
var		   const bool bUsedWithParticleSystem;
var(Usage) const bool bUsedWithParticleSprites;
var(Usage) const bool bUsedWithBeamTrails;
var(Usage) const bool bUsedWithParticleSubUV;
var(Usage) const bool bUsedWithParticleGPU;
var const transient bool bUsedWithSpeedTree;
var const transient bool bUsedWithStaticLighting;
var(Usage) const bool bUsedWithLensFlare;
var(Usage) const bool bUsedWithGammaCorrection;
var(Usage) const bool bUsedWithHitMasks;
var(Usage) const bool bUsedWithInstancedMeshParticles;
var(Usage) const bool bUsedWithFluidSurfaces;
var(Usage) const bool bUsedWithRockDecals;
var(Usage) const bool bUsedWithMaterialEffect;
var(Usage) const bool bUsedWithMorphTargets;
var(Usage) const bool bUsedWithRadialBlur;
var(Usage) const bool bUsedWithInstancedMeshes;
var(Usage) const bool bUsedWithSplineMeshes;
var(Usage) const bool bUsedWithAPEXMeshes;
var(Usage) const bool bUsedWithAPEXClothing;
var(Usage) const bool bUsedWithApexSprites;
var(Usage) const bool bUsedWithOpacityShadows;
var const transient bool bUsedWithScreenDoorFade;

var(D3D11) const bool bEnableCrackFreeDisplacement;
var(D3D11) bool bUseImageBasedReflections;

var(Misc) bool Wireframe;
var transient bool bPerPixelCameraVector;
var transient bool bAllowLightmapSpecular;
var() bool CanStripNormalsAndTangents;
var() bool CanStripVertexColours;
var bool UseFastLODRendering;
var() bool DisallowGlobalSamplerStates;

var deprecated bool bIsFallbackMaterial;

var private bool bUsesDistortion;
var private bool bIsMasked;
var private bool bUsesSSS;
var private bool bUseBlurDirection;
var transient duplicatetransient private bool bIsPreviewMaterial;

var(RS_PointCloudOptions) float PointCloudOcclusion;
var(RS_PointCloudOptions) float PointCloudScale;

var(RS_Options) const LinearColor SSSColourDefault;

var(D3D11) float ImageReflectionNormalDampening;

var(FaceWorks) float DeepScatterIntensity;
var(FaceWorks) float DeepScatterRadius;
var(FaceWorks) float ThicknessNormalOffset;
var(FaceWorks) float ThicknessBlurRadius;
var(FaceWorks) float ThicknessDepthSharpness;

var const native duplicatetransient pointer MaterialResources[2]{FMaterialResource};

var const native duplicatetransient pointer DefaultMaterialInstances[3]{class FDefaultMaterialInstance};

var editoronly int	EditorX,
					EditorY,
					EditorPitch,
					EditorYaw;

var array<MaterialExpression>			Expressions;

var editoronly array<MaterialExpressionComment>	EditorComments;

var array<MaterialFunctionInfo>			MaterialFunctionInfos;

var native map{FName, TArray<UMaterialExpression*>} EditorParameters;

var private deprecated editoronly const array<texture> ReferencedTextures;

var private editoronly const array<guid> ReferencedTextureGuids;

var(Source) editoronly editconst string SourceTimestamp;

var(Source) editoronly editconst string SourceAuthor;

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
	PhysMaterialMaskUVChannel=-1
	LightingModel=MLM_RockBRDF
	DecalPriority=DP_Default
	DiffuseColor=(Constant=(R=128,G=128,B=128))
	DiffusePower=(Constant=1.0)
	SpecularColor=(Constant=(R=128,G=128,B=128))
	SpecularColor2=(Constant=(R=128,G=128,B=128))
	SpecularPower=(Constant=15.0)
	SpecularPower2=(Constant=15.0)
	Opacity=(Constant=1.0)
	OpacityMask=(Constant=1.0)
	OpacityMaskClipValue=0.3333
	OpacityMaskClipValuePostDepth=0.3333
	TwoSidedLightingColor=(Constant=(R=255,G=255,B=255))
	SubsurfaceInscatteringColor=(Constant=(R=255,G=255,B=255))
	SubsurfaceAbsorptionColor=(Constant=(R=230,G=200,B=200))
	bDoPointCloudFalloff=TRUE
	bAllowFog=TRUE
	bUsedWithStaticMesh=TRUE
	bAllowLightmapSpecular=TRUE
	PointCloudOcclusion=0.2
	PointCloudScale=6.0
	SSSColourDefault=(R=0.45,G=0.2,B=0.06,A=1.0)
	ImageReflectionNormalDampening=5.0
	DeepScatterIntensity=0.3
	DeepScatterRadius=1.2
	ThicknessNormalOffset=-1.0
	ThicknessBlurRadius=0.015
	ThicknessDepthSharpness=0.1
}
