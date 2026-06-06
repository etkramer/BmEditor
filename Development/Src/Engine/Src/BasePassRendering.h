/*=============================================================================
	BasePassRendering.h: Base pass rendering definitions.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "LightMapRendering.h"
#include "TessellationRendering.h"

/** Whether to render the dominant lights in the base pass, which is significantly faster than using an extra light pass. */
extern const UBOOL GOnePassDominantLight;

/** Whether to use deferred shading, where the base pass outputs G buffer attributes, and lighting passes fetch these attributes and do shading based on them. */
extern UBOOL GAllowDeferredShading;

/** Returns TRUE if the given material and primitive can be lit in a deferred pass. */
extern UBOOL MeshSupportsDeferredLighting(const FMaterial* Material, const FPrimitiveSceneInfo* PrimitiveSceneInfo);

/** Returns TRUE if the engine should use deferred shading instead of forward lighting. */
inline UBOOL ShouldUseDeferredShading()
{
	return GAllowDeferredShading && GRHIShaderPlatform == SP_PCD3D_SM5;
}

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassVertexShader : public FMeshMaterialVertexShader, public LightMapPolicyType::VertexParametersType
{
	DECLARE_SHADER_TYPE(TBasePassVertexShader,MeshMaterial);

protected:

	TBasePassVertexShader() {}
	TBasePassVertexShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
#if !BATMAN
		HeightFogParameters.Bind(Initializer.ParameterMap);
#endif
		FogVolumeParameters.Bind(Initializer.ParameterMap);
#if BATMAN
		DOFParameters.Bind(Initializer.ParameterMap);
		ObjectFogColorParameter.Bind(Initializer.ParameterMap, TEXT("ObjectFogColor"), TRUE);
#endif
	}

public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Opaque and modulated materials shouldn't apply fog volumes in their base pass.
		const EBlendMode BlendMode = Material->GetBlendMode();
		const UBOOL bUseFogVolume = IsTranslucentBlendMode(BlendMode) && BlendMode != BLEND_Modulate;
		const UBOOL bIsFogVolumeShader = FogDensityPolicyType::DensityFunctionType != FVDF_None;
		return	(bUseFogVolume || !bIsFogVolumeShader) &&
				FogDensityPolicyType::ShouldCache(Platform,Material,VertexFactoryType) && 
				LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FogDensityPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
#if !BATMAN
		Ar << HeightFogParameters;
#endif
		Ar << MaterialParameters;
#if BATMAN
		Ar << DOFParameters;
		Ar << ObjectFogColorParameter;
#endif
		Ar << FogVolumeParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		const UBOOL bAllowGlobalFog
		)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
#if !BATMAN
		HeightFogParameters.SetVertexShader(VertexFactory, MaterialRenderProxy, &View, bAllowGlobalFog, this);
#else
		DOFParameters.SetVS(this, View.DepthOfFieldParams);
#endif
	}

#if BATMAN
	void SetObjectFogColor(const FLinearColor& ObjectFogColor)
	{
		SetVertexShaderValue(GetVertexShader(), ObjectFogColorParameter, ObjectFogColor);
	}
#endif

	void SetFogVolumeParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		typename FogDensityPolicyType::ElementDataType FogVolumeElementData
		)
	{
		FogVolumeParameters.SetVertexShader(View,MaterialRenderProxy, this, FogVolumeElementData);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshElement& Mesh,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;

#if !BATMAN
	FHeightFogShaderParameters HeightFogParameters;
#endif

	typename FogDensityPolicyType::ShaderParametersType FogVolumeParameters;

#if BATMAN
	FDOFShaderParameters DOFParameters;
	FShaderParameter ObjectFogColorParameter;
#endif
};

#if WITH_D3D11_TESSELLATION

/**
 * The base shader type for hull shaders.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(TBasePassHullShader,MeshMaterial);

protected:

	TBasePassHullShader() {}

	TBasePassHullShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}
};

/**
 * The base shader type for Domain shaders.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(TBasePassDomainShader,MeshMaterial);

protected:

	TBasePassDomainShader() {}

	TBasePassDomainShader(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer)
	{
		FogVolumeParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

public:

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FBaseDomainShader::Serialize(Ar);
		Ar << HeightFogParameters;
		Ar << FogVolumeParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FBaseDomainShader::SetParameters(MaterialRenderProxy, View);
		HeightFogParameters.SetDomainShader(VertexFactory, MaterialRenderProxy, &View, this);
	}

	void SetFogVolumeParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		typename FogDensityPolicyType::ElementDataType FogVolumeElementData
		)
	{
		FogVolumeParameters.SetDomainShader(View,MaterialRenderProxy, this, FogVolumeElementData);
	}

private:

	/** The parameters needed to calculate the fog contribution from height fog layers. */
	FHeightFogShaderParameters HeightFogParameters;

	/** The parameters needed to calculate the fog contribution from an intersecting fog volume. */
	typename FogDensityPolicyType::ShaderParametersType FogVolumeParameters;
};

#endif

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TBasePassPixelShaderBaseType : public FMeshMaterialPixelShader, public LightMapPolicyType::PixelParametersType
{
public:

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType,UBOOL bEnableSkyLight)
	{
		return LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType,bEnableSkyLight);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassPixelShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialPixelShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		MaterialParameters.Bind(Initializer.ParameterMap);
		AmbientColorAndSkyFactorParameter.Bind(Initializer.ParameterMap,TEXT("AmbientColorAndSkyFactor"),TRUE);
		UpperSkyColorParameter.Bind(Initializer.ParameterMap,TEXT("UpperSkyColor"),TRUE);
		LowerSkyColorParameter.Bind(Initializer.ParameterMap,TEXT("LowerSkyColor"),TRUE);
#if BATMAN
		MotionBlurMaskParameter.Bind(Initializer.ParameterMap,TEXT("MotionBlurMask"),TRUE);
#else
		DeferredRenderingParameters.Bind(Initializer.ParameterMap,TEXT("DeferredRenderingParameters"),TRUE);
#endif
	}
	TBasePassPixelShaderBaseType() {}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View,UBOOL bDrawLitTranslucencyUnlit)
	{
		VertexFactoryParameters.Set(this, VertexFactory, *View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, View->Family->CurrentWorldTime, View->Family->CurrentRealTime, View);
		MaterialParameters.Set(this,MaterialRenderContext);

		if(AmbientColorAndSkyFactorParameter.IsBound())
		{
			// Draw the surface unlit if it's an unlit view, or it's a lit material without a light-map.
			const FMaterial* Material = MaterialRenderProxy->GetMaterial();
			const UBOOL bIsTranslucentLitMaterial = IsTranslucentBlendMode(Material->GetBlendMode()) && Material->GetLightingModel() != MLM_Unlit;
			const UBOOL bIsUnlitView = !(View->Family->ShowFlags & SHOW_Lighting);
			const UBOOL bDrawSurfaceUnlit = bIsUnlitView || (LightMapPolicyType::bDrawLitTranslucencyUnlit && bDrawLitTranslucencyUnlit && bIsTranslucentLitMaterial);
			SetPixelShaderValue(
				GetPixelShader(),
				AmbientColorAndSkyFactorParameter,
				bDrawSurfaceUnlit ? FLinearColor(1,1,1,0) : FLinearColor(0,0,0,1)
				);
		}
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshElement& Mesh,const FSceneView& View,UBOOL bBackFace)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,View,bBackFace);

#if !BATMAN && PLATFORM_SUPPORTS_D3D10_PLUS
		if (DeferredRenderingParameters.IsBound() && PrimitiveSceneInfo)
		{
			const FMaterial* Material = Mesh.MaterialRenderProxy->GetMaterial();
			const UBOOL bSupportsDeferredLighting = MeshSupportsDeferredLighting(Material, PrimitiveSceneInfo);

			SetPixelShaderValue(
				GetPixelShader(),
				DeferredRenderingParameters,
				FVector4(
					bSupportsDeferredLighting ? 1.0f : 0.0f, 
					Material->GetImageReflectionNormalDampening(),
					PrimitiveSceneInfo->LightingChannels.GetDeferredShadingChannelMask()
					)
				);
		}
#endif
	}

#if BATMAN
	void SetMotionBlurMask(FLOAT MotionBlurMask)
	{
		const FLOAT MaskValue = MotionBlurMask != 0.0f ? 1.0f : 0.0f;
		SetPixelShaderValue(GetPixelShader(), MotionBlurMaskParameter, MaskValue);
	}
#endif

	void SetSkyColor(const FLinearColor& UpperSkyColor,const FLinearColor& LowerSkyColor)
	{
		SetPixelShaderValue(GetPixelShader(),UpperSkyColorParameter,UpperSkyColor);
		SetPixelShaderValue(GetPixelShader(),LowerSkyColorParameter,LowerSkyColor);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
#if BATMAN
		if (Ar.IsBmCooked(TRUE))
		{
			LightMapPolicyType::PixelParametersType::Serialize(Ar);
			Ar << MaterialParameters;
			Ar << AmbientColorAndSkyFactorParameter;
			Ar << UpperSkyColorParameter;
			Ar << LowerSkyColorParameter;
			Ar << MotionBlurMaskParameter;
		}
		else
#endif
		{
			bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
			LightMapPolicyType::PixelParametersType::Serialize(Ar);
			Ar << MaterialParameters;
			Ar << AmbientColorAndSkyFactorParameter;
			Ar << UpperSkyColorParameter;
			Ar << LowerSkyColorParameter;
#if BATMAN
			Ar << MotionBlurMaskParameter;
#else
			Ar << DeferredRenderingParameters;
#endif
		}

		// set parameter names for platforms that need them
		UpperSkyColorParameter.SetShaderParamName(TEXT("UpperSkyColor"));
		LowerSkyColorParameter.SetShaderParamName(TEXT("LowerSkyColor"));

		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderParameter AmbientColorAndSkyFactorParameter;
	FShaderParameter UpperSkyColorParameter;
	FShaderParameter LowerSkyColorParameter;
#if BATMAN
	FShaderParameter MotionBlurMaskParameter;
#else
	FShaderParameter DeferredRenderingParameters;
#endif
};

#if BATMAN
template<typename LightMapPolicyType, UBOOL bEnableSkyLight>
struct TBM2BasePassPixelShaderShouldCache
{
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		const UBOOL bCacheShaders = !bEnableSkyLight || (Material->GetLightingModel() != MLM_Unlit);
		return bCacheShaders &&
			TBasePassPixelShaderBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
	}
};

template<typename LightMapPolicyType>
struct TBM2BasePassPixelShaderNeverCacheSkyLight
{
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FALSE;
	}
};

template<>
struct TBM2BasePassPixelShaderShouldCache<FDirectionalVertexLightMapPolicy, TRUE> : TBM2BasePassPixelShaderNeverCacheSkyLight<FDirectionalVertexLightMapPolicy>
{
};

template<>
struct TBM2BasePassPixelShaderShouldCache<FSimpleVertexLightMapPolicy, TRUE> : TBM2BasePassPixelShaderNeverCacheSkyLight<FSimpleVertexLightMapPolicy>
{
};

template<>
struct TBM2BasePassPixelShaderShouldCache<FDirectionalLightMapTexturePolicy, TRUE> : TBM2BasePassPixelShaderNeverCacheSkyLight<FDirectionalLightMapTexturePolicy>
{
};

template<>
struct TBM2BasePassPixelShaderShouldCache<FSimpleLightMapTexturePolicy, TRUE> : TBM2BasePassPixelShaderNeverCacheSkyLight<FSimpleLightMapTexturePolicy>
{
};
#endif

/** The concrete base pass pixel shader type, parameterized by whether sky lighting is needed. */
template<typename LightMapPolicyType,UBOOL bEnableSkyLight>
class TBasePassPixelShader : public TBasePassPixelShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassPixelShader,MeshMaterial);
public:
	
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
#if BATMAN
		return TBM2BasePassPixelShaderShouldCache<LightMapPolicyType, bEnableSkyLight>::ShouldCache(Platform, Material, VertexFactoryType);
#else
		//don't compile skylight versions if the material is unlit
		const UBOOL bCacheShaders = !bEnableSkyLight || (Material->GetLightingModel() != MLM_Unlit);
		return bCacheShaders && 
			TBasePassPixelShaderBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType, bEnableSkyLight);
#endif
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		TBasePassPixelShaderBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("ENABLE_SKY_LIGHT"),bEnableSkyLight ? TEXT("1") : TEXT("0"));
	}
	
	/** Initialization constructor. */
	TBasePassPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassPixelShaderBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TBasePassPixelShader() {}
};

#if BATMAN
template<typename LightMapPolicyType, UBOOL bEnableSkyLight>
static UBOOL BM2HasCookedBasePassShaders(const FMaterial* Material, FVertexFactoryType* VertexFactoryType)
{
	if (!Material || !VertexFactoryType)
	{
		return FALSE;
	}

	FMeshMaterialShaderType* VertexShaderType = &TVertexShaderTessellationPermutation<TBasePassVertexShader<LightMapPolicyType,FNoDensityPolicy>,0>::StaticType;
	FMeshMaterialShaderType* PixelShaderType = &TBasePassPixelShader<LightMapPolicyType,bEnableSkyLight>::StaticType;
	// Prefer the cooked BM2 cache as the source of truth; local ShouldCache can differ while reconstruction is in flight.
	const FMaterialShaderMap* MaterialShaderMap = Material->GetShaderMap();
	const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap ? MaterialShaderMap->GetMeshShaderMap(VertexFactoryType) : NULL;
	return MeshShaderMap
		&& MeshShaderMap->GetShader(VertexShaderType)
		&& MeshShaderMap->GetShader(PixelShaderType);
}

template<typename LightMapPolicyType>
static UBOOL BM2HasCookedBasePassNoSkyLightShaders(const FMaterial* Material, FVertexFactoryType* VertexFactoryType)
{
	return BM2HasCookedBasePassShaders<LightMapPolicyType,FALSE>(Material, VertexFactoryType);
}
#endif

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType,typename FogDensityPolicyType>
class TBasePassDrawingPolicy : public FMeshDrawingPolicy
{
public:

	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** The element's fog volume data. */
		typename FogDensityPolicyType::ElementDataType FogVolumeElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(
			const typename LightMapPolicyType::ElementDataType& InLightMapElementData,
			const typename FogDensityPolicyType::ElementDataType& InFogVolumeElementData
			):
			LightMapElementData(InLightMapElementData),
			FogVolumeElementData(InFogVolumeElementData)
		{}
	};

	/** Initialization constructor. */
	TBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		UBOOL bInEnableSkyLight,
		UBOOL bOverrideWithShaderComplexity = FALSE,
		UBOOL bInDrawLitTranslucencyUnlit = TRUE,
		UBOOL bInRenderingToLowResTranslucency = FALSE,
		UBOOL bInRenderingToDoFBlurBuffer = FALSE,
		UBOOL bInShouldOverwriteTranslucentAlpha = FALSE,
		UBOOL bInAllowGlobalFog = FALSE,
		const FLinearColor& InObjectFogColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,bOverrideWithShaderComplexity),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		bEnableSkyLight(bInEnableSkyLight),
		bDrawLitTranslucencyUnlit(bInDrawLitTranslucencyUnlit),
		bRenderingToLowResTranslucency(bInRenderingToLowResTranslucency),
		bRenderingToDoFBlurBuffer(bInRenderingToDoFBlurBuffer),
		bShouldOverwriteTranslucentAlpha(bInShouldOverwriteTranslucentAlpha),
		bAllowGlobalFog(bInAllowGlobalFog),
		ObjectFogColor(InObjectFogColor)
	{
		const FMaterial* MaterialResource = InMaterialRenderProxy->GetMaterial();
#if BATMAN
		if (bEnableSkyLight && !BM2HasCookedBasePassShaders<LightMapPolicyType,TRUE>(MaterialResource, InVertexFactory->GetType()))
		{
			bEnableSkyLight = FALSE;
		}
#endif

#if WITH_D3D11_TESSELLATION
		HullShader = NULL;
		DomainShader = NULL;
	
		const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetD3D11TessellationMode();

		if (GRHIShaderPlatform == SP_PCD3D_SM5
			&& InVertexFactory->GetType()->SupportsTessellationShaders() 
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			// Find the base pass tessellation shaders since the material is tessellated
			HullShader = MaterialResource->GetShader<THullShaderTessellationPermutation<TBasePassHullShader<LightMapPolicyType,FogDensityPolicyType>,0> >(VertexFactory->GetType());
			DomainShader = MaterialResource->GetShader<TDomainShaderTessellationPermutation<TBasePassDomainShader<LightMapPolicyType,FogDensityPolicyType>,0> >(VertexFactory->GetType());
		}
#endif
		VertexShader = MaterialResource->GetShader<TVertexShaderTessellationPermutation<TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>,0> >(InVertexFactory->GetType());

		// Find the appropriate shaders based on whether sky lighting is needed.
		if (bEnableSkyLight)
		{
			PixelShader = MaterialResource->GetShader<TBasePassPixelShader<LightMapPolicyType,TRUE> >(InVertexFactory->GetType());
		}
		else
		{
			PixelShader = MaterialResource->GetShader<TBasePassPixelShader<LightMapPolicyType,FALSE> >(InVertexFactory->GetType());
		}
	}

	// FMeshDrawingPolicy interface.

	UBOOL Matches(const TBasePassDrawingPolicy& Other) const
	{
#if WITH_MOBILE_RHI
		if( GUsingMobileRHI )
		{
			//For mobile use the internally computed material key to get around the "uber-shader" having many different programs behind the scenes
			INT ShaderKeyA = MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			INT ShaderKeyB = Other.MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			return FMeshDrawingPolicy::Matches(Other) &&
				(ShaderKeyA == ShaderKeyB);
		}
		else
#endif
		{
			return FMeshDrawingPolicy::Matches(Other) &&
				VertexShader == Other.VertexShader &&
				PixelShader == Other.PixelShader &&
#if WITH_D3D11_TESSELLATION
				HullShader == Other.HullShader &&
				DomainShader == Other.DomainShader &&
#endif
				bDrawLitTranslucencyUnlit == Other.bDrawLitTranslucencyUnlit &&
				bRenderingToLowResTranslucency == Other.bRenderingToLowResTranslucency &&
				bRenderingToDoFBlurBuffer == Other.bRenderingToDoFBlurBuffer &&
				bShouldOverwriteTranslucentAlpha == Other.bShouldOverwriteTranslucentAlpha &&
				bAllowGlobalFog == Other.bAllowGlobalFog &&
				ObjectFogColor == Other.ObjectFogColor &&

				LightMapPolicy == Other.LightMapPolicy;
		}
	}

	void DrawShared(const FSceneView* View,FBoundShaderStateRHIParamRef BoundShaderState) const
	{
		VertexShader->SetParameters(VertexFactory,MaterialRenderProxy,*View, bAllowGlobalFog);
#if BATMAN
		VertexShader->SetObjectFogColor(ObjectFogColor);
#endif
#if WITH_D3D11_TESSELLATION
		if(HullShader)
		{
			HullShader->SetParameters(MaterialRenderProxy,*View);
		}
		if(DomainShader)
		{
			DomainShader->SetParameters(VertexFactory,MaterialRenderProxy,*View);
		}
#endif
#if !FINAL_RELEASE
		if (bOverrideWithShaderComplexity)
		{
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing
				RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
			}

			TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap());
			const UINT NumPixelShaderInstructions = bRenderingToLowResTranslucency ? 
				// Reduce the instruction count by the resolution factor used with downsampled translucency
				//@todo - would be nice to factor in the constant composite overhead somehow
				PixelShader->GetNumInstructions() / Square(GSceneRenderTargets.GetSmallColorDepthDownsampleFactor()) :
				PixelShader->GetNumInstructions();

			const UINT NumVertexShaderInstructions = VertexShader->GetNumInstructions();
			ShaderComplexityPixelShader->SetParameters(NumVertexShaderInstructions,NumPixelShaderInstructions);
		}
		else
#endif
		{
			PixelShader->SetParameters(VertexFactory,MaterialRenderProxy,View,bDrawLitTranslucencyUnlit);

			EBlendMode EffectiveBlendMode = BlendMode;
			// Use an opaque blend mode with one layer distortion, blending will be done manually in the shader
			if (IsTranslucentBlendMode(BlendMode) && MaterialRenderProxy->GetMaterial()->UsesOneLayerDistortion())
			{
				EffectiveBlendMode = BLEND_Opaque;
			}

			switch(EffectiveBlendMode)
			{
			default:
			case BLEND_Opaque:
				RHISetBlendState(TStaticBlendState<>::GetRHI());
				break;
			case BLEND_DitheredTranslucent:
			case BLEND_Masked:
#if WITH_MOBILE_RHI
				if( GUsingMobileRHI )
				{
					// if we are using simplified, flattened materials, we won't know if masking is enabled, so enable alphatest with a default value of .33
					RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero, CF_Greater, 255/3>::GetRHI());
				}
				else
#endif
				{
					RHISetBlendState(TStaticBlendState<>::GetRHI());
				}
				break;
			case BLEND_SoftMasked:
				RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());
				break;
			case BLEND_Translucent:
				{
					UBOOL bInvOpacityInAlpha = bRenderingToLowResTranslucency;
#if !CONSOLE
					if(GSystemSettings.bAllowSeparateTranslucency && GRHIShaderPlatform == SP_PCD3D_SM5)
					{
						bInvOpacityInAlpha = TRUE;
					}
#endif
					RHISetBlendState(
						bInvOpacityInAlpha ?
							// Accumulate added color in rgb, accumulate inverse opacity in alpha.
							TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_InverseSourceAlpha>::GetRHI() :
						!bShouldOverwriteTranslucentAlpha ?
							// Blend with the existing scene color, preserve destination alpha.
							TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI() :
							// Blend with the existing scene color, overwriting destination alpha.
							TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_One,BF_Zero>::GetRHI()
						);
#if XBOX
					if(bRenderingToDoFBlurBuffer)
					{
						RHISetMRTBlendState(TStaticBlendState<BO_Max,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI(),1);
					}
#endif
				}
				break;
			case BLEND_Additive:
				if(GRHIShaderPlatform == SP_PCD3D_SM5 && GSystemSettings.bAllowSeparateTranslucency)
				{
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_InverseSourceAlpha>::GetRHI());
				}
				else
				{
					// Add to the existing scene color, preserve destination alpha.
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());
#if XBOX
					if(bRenderingToDoFBlurBuffer)
					{
						RHISetMRTBlendState(TStaticBlendState<BO_Max,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI(),1);
					}
#endif
				}
				break;
			case BLEND_Modulate:
				RHISetBlendState(
					// Modulate with the existing scene color, preserve destination alpha.
					TStaticBlendState<BO_Add,BF_DestColor,BF_Zero,BO_Add,BF_Zero,BF_One>::GetRHI()
					);
#if XBOX
				if(bRenderingToDoFBlurBuffer)
				{
					RHISetMRTBlendState(TStaticBlendState<BO_Max,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI(),1);
				}
#endif
				break;
            case BLEND_AlphaComposite:
                // Blend with existing scene color. New color is premultiplied by alpha.
                RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_InverseSourceAlpha,BO_Add,BF_One,BF_InverseSourceAlpha>::GetRHI());
                break;
			};
		}

		// Set the light-map policy.
		LightMapPolicy.Set(VertexShader,bOverrideWithShaderComplexity ? NULL : PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,View);

		// Set the actual shader & vertex declaration state
		RHISetBoundShaderState( BoundShaderState);
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0)
	{
		FVertexDeclarationRHIParamRef VertexDeclaration;
		DWORD StreamStrides[MaxVertexElementCount];

		LightMapPolicy.GetVertexDeclarationInfo(VertexDeclaration, StreamStrides, VertexFactory);
		if (DynamicStride)
		{
			StreamStrides[0] = DynamicStride;
		}

		FPixelShaderRHIParamRef PixelShaderRHIRef = PixelShader->GetPixelShader();
		FVertexShaderRHIParamRef VertexShaderRHIRef = VertexShader->GetVertexShader();

#if !FINAL_RELEASE
		if (bOverrideWithShaderComplexity)
		{
			TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap());
			PixelShaderRHIRef = ShaderComplexityAccumulatePixelShader->GetPixelShader();
		}
#endif
		FBoundShaderStateRHIRef BoundShaderState;

#if WITH_D3D11_TESSELLATION
		BoundShaderState = RHICreateBoundShaderStateD3D11(
			VertexDeclaration, 
			StreamStrides, 
			VertexShaderRHIRef,
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader), 
			PixelShaderRHIRef,
			FGeometryShaderRHIRef());
#else
			BoundShaderState = RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShaderRHIRef, PixelShaderRHIRef);
#endif

		return BoundShaderState;
	}

	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const
	{
		// Set the fog volume parameters.
		VertexShader->SetFogVolumeParameters(VertexFactory,MaterialRenderProxy,View,ElementData.FogVolumeElementData);
		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,View);

		// Set the light-map policy's mesh-specific settings.
		LightMapPolicy.SetMesh(
			View,
			PrimitiveSceneInfo,
			VertexShader,
			bOverrideWithShaderComplexity ? NULL : PixelShader,
			VertexShader,
			PixelShader,
			VertexFactory,
			MaterialRenderProxy,
			ElementData.LightMapElementData);

#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			// Set the fog volume parameters.
			HullShader->SetMesh(PrimitiveSceneInfo,Mesh,View);
			DomainShader->SetFogVolumeParameters(VertexFactory,MaterialRenderProxy,View,ElementData.FogVolumeElementData);
			DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,View);
		}
#endif

#if !FINAL_RELEASE
		//don't set the draw policies' pixel shader parameters if the shader complexity viewmode is enabled
		//since they will overwrite the FShaderComplexityAccumulatePixelShader parameters
		if(!bOverrideWithShaderComplexity)
#endif
		{
			PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,View,bBackFace);

			if(bEnableSkyLight)
			{
				FLinearColor UpperSkyLightColor = FLinearColor::Black;
				FLinearColor LowerSkyLightColor = FLinearColor::Black;
				if(PrimitiveSceneInfo)
				{
					UpperSkyLightColor = PrimitiveSceneInfo->UpperSkyLightColor;
					if (GIsEditor 
						&& LightMapPolicyType::bAllowPreviewSkyLight 
						&& PrimitiveSceneInfo->bAcceptsLights 
						&& PrimitiveSceneInfo->bStaticShadowing)
					{
						// Add the scene's preview sky color for primitives that should be lightmapped but are using FNoLightMapPolicy
						UpperSkyLightColor += PrimitiveSceneInfo->GetPreviewSkyLightColor();
					}
					LowerSkyLightColor = PrimitiveSceneInfo->LowerSkyLightColor;
				}
				PixelShader->SetSkyColor(UpperSkyLightColor,LowerSkyLightColor);
			}

#if BATMAN
			const UBOOL bMotionBlurMask = PrimitiveSceneInfo && PrimitiveSceneInfo->MotionBlurInstanceScale >= 0.0f;
			PixelShader->SetMotionBlurMask(bMotionBlurMask ? 1.0f : 0.0f);
#endif
		}

		FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,bBackFace,FMeshDrawingPolicy::ElementDataType());
	}

	friend INT Compare(const TBasePassDrawingPolicy& A,const TBasePassDrawingPolicy& B)
	{
#if WITH_MOBILE_RHI
		if( GUsingMobileRHI )
		{
			//For mobile use the internally computed material key to get around the "uber-shader" having many different programs behind the scenes
			//const UTexture2D* SimpleLightMapA = A.ElementDataType.LightMapElementData.GetTexture(0);
			INT ShaderKeyA = A.MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			INT ShaderKeyB = B.MaterialRenderProxy->GetMaterial()->GetMobileMaterialSortKey();
			if(ShaderKeyA < ShaderKeyB) 
			{ 
				return -1; 
			} 
			else if(ShaderKeyA > ShaderKeyB) 
			{ 
				return +1; 
			}
			COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
			return 0;
		}
		else
#endif
		{
			COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
			COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
#if WITH_D3D11_TESSELLATION
			COMPAREDRAWINGPOLICYMEMBERS(HullShader);
			COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
			COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
			COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);

			COMPAREDRAWINGPOLICYMEMBERS(bDrawLitTranslucencyUnlit);
			COMPAREDRAWINGPOLICYMEMBERS(bRenderingToLowResTranslucency);
			COMPAREDRAWINGPOLICYMEMBERS(bRenderingToDoFBlurBuffer);
			COMPAREDRAWINGPOLICYMEMBERS(bShouldOverwriteTranslucentAlpha);
			COMPAREDRAWINGPOLICYMEMBERS(bAllowGlobalFog);
			for (INT ColorIndex = 0; ColorIndex < 4; ColorIndex++)
			{
				if (A.ObjectFogColor.Component(ColorIndex) < B.ObjectFogColor.Component(ColorIndex)) { return -1; }
				else if (A.ObjectFogColor.Component(ColorIndex) > B.ObjectFogColor.Component(ColorIndex)) { return +1; }
			}

			return Compare(A.LightMapPolicy,B.LightMapPolicy);
		}
	}

protected:
	TBasePassVertexShader<LightMapPolicyType,FogDensityPolicyType>* VertexShader;

#if WITH_D3D11_TESSELLATION
	TBasePassHullShader<LightMapPolicyType,FogDensityPolicyType>* HullShader;
	TBasePassDomainShader<LightMapPolicyType,FogDensityPolicyType>* DomainShader;
#endif

	TBasePassPixelShaderBaseType<LightMapPolicyType>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;

	BITFIELD bEnableSkyLight : 1;
	BITFIELD bDrawLitTranslucencyUnlit : 1;
	BITFIELD bRenderingToLowResTranslucency : 1;
	BITFIELD bRenderingToDoFBlurBuffer : 1;
	BITFIELD bShouldOverwriteTranslucentAlpha : 1;
	BITFIELD bAllowGlobalFog : 1;
	FLinearColor ObjectFogColor;

	friend class FDrawTranslucentMeshAction;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = TRUE };
	struct ContextType {};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType DrawingContext = ContextType());
	static UBOOL DrawDynamicMesh(
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId
		);
	static UBOOL IsMaterialIgnored(const FMaterialRenderProxy* MaterialRenderProxy)
	{
		// Ignore non-opaque materials in the opaque base pass.
		return MaterialRenderProxy && IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial()->GetBlendMode());
	}
};

/** The parameters used to process a base pass mesh. */
class FProcessBasePassMeshParameters
{
public:

	const FMeshElement& Mesh;
	const FMaterial* Material;
	const FPrimitiveSceneInfo* PrimitiveSceneInfo;
	EBlendMode BlendMode;
	EMaterialLightingModel LightingModel;
	const UBOOL bAllowFog;

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshElement& InMesh,
		const FMaterial* InMaterial,
		const FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		UBOOL InbAllowFog
		):
		Mesh(InMesh),
		Material(InMaterial),
		PrimitiveSceneInfo(InPrimitiveSceneInfo),
		BlendMode(InMaterial->GetBlendMode()),
		LightingModel(InMaterial->GetLightingModel()),
		bAllowFog(InbAllowFog)
	{
	}
};

/** Processes a base pass mesh using a known light map policy, and unknown fog density policy. */
template<typename ProcessActionType,typename LightMapPolicyType>
void ProcessBasePassMesh_LightMapped(
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action,
	const LightMapPolicyType& LightMapPolicy,
	const typename LightMapPolicyType::ElementDataType& LightMapElementData
	)
{
	// Don't render fog on opaque or modulated materials and GPU skinned meshes.
	const UBOOL bDisableFog =
		!Parameters.bAllowFog ||
		!IsTranslucentBlendMode(Parameters.BlendMode) ||
		(Parameters.BlendMode == BLEND_Modulate) ||
		Parameters.Mesh.VertexFactory->IsGPUSkinned() ||
		!Parameters.Material->AllowsFog() ||
		// Fog volume policies don't compile for decals so force fog volumes to be disables
		Parameters.Material->IsUsedWithDecals();

	// Determine the density function of the fog volume the primitive is in.
	const EFogVolumeDensityFunction FogVolumeDensityFunction = 
		!bDisableFog && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo ?
			Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo->GetDensityFunctionType() :
			FVDF_None;

	// Define a macro to handle a specific case of fog volume density function.
	#define HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FogDensityPolicyType,FogDensityElementData) \
		case FogDensityPolicyType::DensityFunctionType: \
			Action.template Process<LightMapPolicyType,FogDensityPolicyType>(Parameters,LightMapPolicy,LightMapElementData,FogDensityElementData); \
			break;

	// Call Action.Process with the appropriate fog volume density policy type.
	switch(FogVolumeDensityFunction)
	{
#if BATMAN
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FRockAtmosDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FSphereDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
#else
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FConstantDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FLinearHalfspaceDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FSphereDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FConeDensityPolicy,Parameters.PrimitiveSceneInfo->FogVolumeSceneInfo);
#endif
		default:
		HANDLE_FOG_VOLUME_DENSITY_FUNCTION(FNoDensityPolicy,FNoDensityPolicy::ElementDataType());
	};

	#undef HANDLE_FOG_VOLUME_DENSITY_FUNCTION
}

#if BATMAN
template<typename ProcessActionType>
void BM2ProcessNoLightCompatibleBasePassMesh(
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action
	)
{
	if (BM2HasCookedBasePassNoSkyLightShaders<FNoLightMapPolicy>(Parameters.Material, Parameters.Mesh.VertexFactory->GetType()))
	{
		ProcessBasePassMesh_LightMapped<ProcessActionType, FNoLightMapPolicy>(
			Parameters,
			Action,
			FNoLightMapPolicy(),
			FNoLightMapPolicy::ElementDataType());
	}
	else
	{
	}
}
#endif

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType>
void ProcessBasePassMesh(
	const FProcessBasePassMeshParameters& Parameters,
	const ProcessActionType& Action
	)
{
	// Check for a cached light-map.
	const UBOOL bIsLitMaterial = Parameters.LightingModel != MLM_Unlit;
	const FLightMapInteraction LightMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial) ? Parameters.Mesh.LCI->GetLightMapInteraction() : FLightMapInteraction();

	UBOOL bShouldRenderDominantLight = FALSE;
	FLightInteraction DominantLightInteraction = FLightInteraction::Uncached();
	// If we're doing a one pass dominant dynamic light, check if this primitive has a dominant light associated with it
	if (GOnePassDominantLight
		&& bIsLitMaterial 
		&& Parameters.Mesh.LCI 
		&& Parameters.PrimitiveSceneInfo 
		&& Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo
		&& IsDominantLightType(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo->LightType)
		&& LightMapInteraction.GetType() != LMIT_None)
	{
		bShouldRenderDominantLight = TRUE;
		// Check if the primitive has a cached interaction with the dominant light
		DominantLightInteraction = Parameters.Mesh.LCI->GetInteraction(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
	}
	const ELightInteractionType DominantLightInteractionType = DominantLightInteraction.GetType();

	// force simple lightmaps based on system settings
	const UBOOL bAllowDirectionalLightMaps = GSystemSettings.bAllowDirectionalLightMaps && LightMapInteraction.AllowsDirectionalLightmaps();
	const UBOOL bReceiveDynamicShadows = Parameters.PrimitiveSceneInfo ? 
		Action.ShouldReceiveDominantShadows(Parameters) : 
		FALSE;
	const UBOOL bOverrideDynamicShadowsOnTranslucency = Action.ShouldOverrideDynamicShadowsOnTranslucency(Parameters);
	const UBOOL bUseTranslucencyLightAttenuation = Action.UseTranslucencyLightAttenuation(Parameters);

	if (DominantLightInteractionType == LIT_CachedShadowMap2D 
		// Handle a dominant light without static shadowing with texture lightmaps
		|| bShouldRenderDominantLight && DominantLightInteractionType == LIT_Uncached && LightMapInteraction.GetType() == LMIT_Texture)
	{
		checkSlow(bIsLitMaterial && Parameters.Mesh.LCI && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
		// Can't mix texture shadow maps with vertex lightmaps
		checkSlow(LightMapInteraction.GetType() == LMIT_Texture);
		checkSlow(bAllowDirectionalLightMaps);
		// Use a white shadow texture if the dominant light isn't using static shadowing
		// This is done to avoid adding shader combinations to handle a dominant light without static shadowing with lightmaps
		FTexture* ShadowTexture = GWhiteTexture;
		FVector2D CoordinateScale(1,1);
		FVector2D CoordinateBias(0,0);
		if (DominantLightInteractionType == LIT_CachedShadowMap2D)
		{
			ShadowTexture = DominantLightInteraction.GetShadowTexture()->Resource;
			CoordinateScale = DominantLightInteraction.GetShadowCoordinateScale();
			CoordinateBias = DominantLightInteraction.GetShadowCoordinateBias();
		}
		// Render the mesh with a texture shadow mapped directional light and texture lightmaps
		ProcessBasePassMesh_LightMapped<ProcessActionType,FShadowedDynamicLightDirectionalLightMapTexturePolicy>(
			Parameters,
			Action,
			FShadowedDynamicLightDirectionalLightMapTexturePolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation),
			FShadowedDynamicLightDirectionalLightMapTexturePolicy::ElementDataType(
				FTextureShadowedDynamicLightLightMapPolicy::ElementDataType(
					ShadowTexture,
					LightMapInteraction, 
					CoordinateScale, 
					CoordinateBias, 
					FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow())), 
				LightMapInteraction)); 
	}
	else if (DominantLightInteractionType == LIT_CachedSignedDistanceFieldShadowMap2D)
	{
		checkSlow(bIsLitMaterial && Parameters.Mesh.LCI && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
		// Can't mix texture shadow maps with vertex lightmaps
		checkSlow(LightMapInteraction.GetType() == LMIT_Texture);
		checkSlow(bAllowDirectionalLightMaps);
		// Render the mesh with a texture signed distance field shadow mapped directional light and texture lightmaps
		ProcessBasePassMesh_LightMapped<ProcessActionType,FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy>(
			Parameters,
			Action,
			FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation),
			FDistanceFieldShadowedDynamicLightDirectionalLightMapTexturePolicy::ElementDataType(
				FTextureDistanceFieldShadowedDynamicLightLightMapPolicy::ElementDataType(
					DominantLightInteraction.GetShadowTexture()->Resource,
					LightMapInteraction, 
					DominantLightInteraction.GetShadowCoordinateScale(), 
					DominantLightInteraction.GetShadowCoordinateBias(), 
					Parameters.Mesh.MaterialRenderProxy->GetDistanceFieldPenumbraScale(),
					FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow()),
					Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo), 
				LightMapInteraction)); 
	}
	else if (DominantLightInteractionType == LIT_CachedShadowMap1D
		// Handle a dominant light without static shadowing with vertex lightmaps
		|| bShouldRenderDominantLight && DominantLightInteractionType == LIT_Uncached && LightMapInteraction.GetType() == LMIT_Vertex)
	{
		checkSlow(bIsLitMaterial && Parameters.Mesh.LCI && Parameters.PrimitiveSceneInfo && Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo);
		// Can't mix vertex shadow maps with texture lightmaps
		checkSlow(LightMapInteraction.GetType() == LMIT_Vertex);
		checkSlow(bAllowDirectionalLightMaps);
		// Use a white shadow vertex buffer if the dominant light isn't using static shadowing
		// This is done to avoid adding shader combinations to handle a dominant light without static shadowing with lightmaps
		const FVertexBuffer* ShadowBuffer = DominantLightInteractionType == LIT_CachedShadowMap1D ? DominantLightInteraction.GetShadowVertexBuffer() : &GNullShadowmapVertexBuffer;
		// Render the mesh with a vertex shadow mapped directional light and vertex lightmaps
		ProcessBasePassMesh_LightMapped<ProcessActionType,FShadowedDynamicLightDirectionalVertexLightMapPolicy>(
			Parameters,
			Action,
			FShadowedDynamicLightDirectionalVertexLightMapPolicy(ShadowBuffer, Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation),
			FShadowedDynamicLightDirectionalVertexLightMapPolicy::ElementDataType(
				FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow()), 
				LightMapInteraction)); 
	}
	else
	{
		// Define a macro to handle a specific case of light-map type.
		#define HANDLE_LIGHTMAP_TYPE(LightMapInteractionType,DirectionalLightMapPolicyType,SimpleLightMapPolicyType,LightMapPolicyParameters,LightMapElementData) \
			case LightMapInteractionType: \
				if( bAllowDirectionalLightMaps ) \
				{ \
					ProcessBasePassMesh_LightMapped<ProcessActionType,DirectionalLightMapPolicyType>(Parameters,Action,DirectionalLightMapPolicyType LightMapPolicyParameters,LightMapElementData); \
				} \
				else \
				{ \
					ProcessBasePassMesh_LightMapped<ProcessActionType,SimpleLightMapPolicyType>(Parameters,Action,SimpleLightMapPolicyType LightMapPolicyParameters,LightMapElementData); \
				} \
				break;

		switch(LightMapInteraction.GetType())
		{
			HANDLE_LIGHTMAP_TYPE(LMIT_Vertex,FDirectionalVertexLightMapPolicy,FSimpleVertexLightMapPolicy,(),LightMapInteraction);
			HANDLE_LIGHTMAP_TYPE(LMIT_Texture,FDirectionalLightMapTexturePolicy,FSimpleLightMapTexturePolicy,(),LightMapInteraction);
			default:
				{
					// Check if we should use a directional light in the base pass
					if (bIsLitMaterial 
						&& Parameters.PrimitiveSceneInfo 
#if BATMAN
						&& !Parameters.Material->IsUsedWithDecals())
#else
						// Shaders not compiled with decal usage due to not enough constant registers
						&& !Parameters.Material->IsUsedWithDecals())
#endif
					{
#if BATMAN
						if (Parameters.PrimitiveSceneInfo->AmbientPlus3DLight)
						{
							if (Parameters.PrimitiveSceneInfo->bRenderAPlus3DLightInBasePass)
							{
								ProcessBasePassMesh_LightMapped<ProcessActionType, FAPlus3DLightLightMapPolicy>(
									Parameters,
									Action,
									FAPlus3DLightLightMapPolicy(),
									Parameters.PrimitiveSceneInfo->AmbientPlus3DLight);
							}
							else
							{
								static INT SpamGuard = 0;
								SpamGuard++;
								if (SpamGuard == 150 * (SpamGuard / 150))
								{
									if (Parameters.Mesh.VertexFactory->GetType()->SupportsDynamicLighting())
									{
										debugf(TEXT("MATERIAL WARNING: %s needs the material usage flag bUsedWithLightEnvironment"), *Parameters.Material->GetFriendlyName());
									}
									else
									{
										debugf(TEXT("MATERIAL WARNING: %s is using a Light Environment and supports Dynamic Lighting and Static Lighting. Tell Dustin!"), *Parameters.Material->GetFriendlyName());
									}
								}

								FMeshElement DefaultMesh(Parameters.Mesh);
								DefaultMesh.MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE, FALSE);
								ProcessBasePassMesh_LightMapped<ProcessActionType, FAPlus3DLightLightMapPolicy>(
									FProcessBasePassMeshParameters(
										DefaultMesh,
										DefaultMesh.MaterialRenderProxy->GetMaterial(),
										Parameters.PrimitiveSceneInfo,
										Parameters.bAllowFog),
									Action,
									FAPlus3DLightLightMapPolicy(),
									Parameters.PrimitiveSceneInfo->AmbientPlus3DLight);
							}
						}
						else
#endif
						{
						const FSHVectorRGB* TranslucencyMergedLighting = Action.GetTranslucencyCompositedDynamicLighting();
#if BATMAN
						const UBOOL bCanUseSHLightBasePass = BM2HasCookedBasePassNoSkyLightShaders<FSHLightLightMapPolicy>(Parameters.Material, Parameters.Mesh.VertexFactory->GetType());
						const UBOOL bCanUseDirectionalLightBasePass = BM2HasCookedBasePassNoSkyLightShaders<FDirectionalLightLightMapPolicy>(Parameters.Material, Parameters.Mesh.VertexFactory->GetType());
						const UBOOL bCanUseSHAndMultiTypeLightBasePass = BM2HasCookedBasePassNoSkyLightShaders<FSHLightAndMultiTypeLightMapPolicy>(Parameters.Material, Parameters.Mesh.VertexFactory->GetType());
						const UBOOL bCanUseDynamicMultiTypeLightBasePass = BM2HasCookedBasePassNoSkyLightShaders<FDynamicallyShadowedMultiTypeLightLightMapPolicy>(Parameters.Material, Parameters.Mesh.VertexFactory->GetType());
#endif
						// If this element is doing approximate one pass lighting for translucency, use a lightmap policy that supports this
						// Note that Action.GetTranslucencyMergedDynamicLightInfo() can still be NULL if no directional, spot or point light was found affecting the translucency
						if (TranslucencyMergedLighting
#if BATMAN
							&& bCanUseSHLightBasePass
#endif
							)
						{
							ProcessBasePassMesh_LightMapped<ProcessActionType, FSHLightLightMapPolicy>(Parameters, Action, 
								FSHLightLightMapPolicy(), 
								FSHLightLightMapPolicy::ElementDataType(
									*TranslucencyMergedLighting,
									FDirectionalLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow(), Action.GetTranslucencyMergedDynamicLightInfo())));
						}
						else if (Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo
#if BATMAN
							&& bCanUseDirectionalLightBasePass
#endif
							)
						{
							// Check if we should use a dynamically shadowed dynamic light in the base pass
							if (GOnePassDominantLight && IsDominantLightType(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo->LightType))
							{
								// No need to check PrimitiveSceneInfo->bRenderSHLightInBasePass, 
								// When combined with a dominant light the SH light can always be merged into the base pass. 
								if (Parameters.PrimitiveSceneInfo->SHLightSceneInfo
#if BATMAN
									&& bCanUseSHAndMultiTypeLightBasePass
#endif
									)
								{
									// Render the SH light in the base pass instead of as a separate pass, along with a dynamically shadowed dynamic light
									ProcessBasePassMesh_LightMapped<ProcessActionType, FSHLightAndMultiTypeLightMapPolicy>(Parameters, Action, 
										FSHLightAndMultiTypeLightMapPolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation), 
										FSHLightAndMultiTypeLightMapPolicy::ElementDataType(
											Parameters.PrimitiveSceneInfo,
											FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow())));
								}
#if BATMAN
								else if (bCanUseDynamicMultiTypeLightBasePass)
#else
								else
#endif
								{
									// Render just a dynamically shadowed light in the base pass
									ProcessBasePassMesh_LightMapped<ProcessActionType, FDynamicallyShadowedMultiTypeLightLightMapPolicy>(Parameters, Action, 
										FDynamicallyShadowedMultiTypeLightLightMapPolicy(Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo, bUseTranslucencyLightAttenuation), 
										FDynamicallyShadowedMultiTypeLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow()));
								}
#if BATMAN
								else
								{
									BM2ProcessNoLightCompatibleBasePassMesh<ProcessActionType>(Parameters,Action);
								}
#endif
							}
							else
							{
								// Using an unshadowed directional light
								// Check if we should also use a spherical harmonic light in the base pass
								if ((Parameters.PrimitiveSceneInfo->bRenderSHLightInBasePass
									// Also use an SH light in the base pass if one is affecting this primitive, the primitive is in the foreground DPG for this view and foreground self-shadowing is disabled.
									// There will be no modulated shadow between the base pass and SH light pass in this case so it is more efficient to merge them together.
									|| Parameters.PrimitiveSceneInfo->SHLightSceneInfo && !GSystemSettings.bEnableForegroundSelfShadowing && Action.GetDPG(Parameters) == SDPG_Foreground)
#if BATMAN
									&& bCanUseSHLightBasePass
#endif
									)
								{
									ProcessBasePassMesh_LightMapped<ProcessActionType, FSHLightLightMapPolicy>(Parameters, Action, 
										FSHLightLightMapPolicy(), 
										FSHLightLightMapPolicy::ElementDataType(
											*Parameters.PrimitiveSceneInfo->SHLightSceneInfo->GetSHIncidentLighting(),
											FDirectionalLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow(), Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo)));
								}
								else
								{
									ProcessBasePassMesh_LightMapped<ProcessActionType, FDirectionalLightLightMapPolicy>(Parameters, Action, 
										FDirectionalLightLightMapPolicy(), 
										FDirectionalLightLightMapPolicy::ElementDataType(bReceiveDynamicShadows, bOverrideDynamicShadowsOnTranslucency, Action.GetTranslucentPreShadow(), Parameters.PrimitiveSceneInfo->DynamicLightSceneInfo));
								}
							}
						}
						else
						{
#if BATMAN
							BM2ProcessNoLightCompatibleBasePassMesh<ProcessActionType>(Parameters,Action);
#else
							ProcessBasePassMesh_LightMapped<ProcessActionType, FNoLightMapPolicy>(Parameters,Action,FNoLightMapPolicy(),FNoLightMapPolicy::ElementDataType());
#endif
						}
						}
					}
					else
					{
#if BATMAN
						BM2ProcessNoLightCompatibleBasePassMesh<ProcessActionType>(Parameters,Action);
#else
						ProcessBasePassMesh_LightMapped<ProcessActionType, FNoLightMapPolicy>(Parameters,Action,FNoLightMapPolicy(),FNoLightMapPolicy::ElementDataType());
#endif
					}
				}
				break;
		};
	}

	#undef HANDLE_LIGHTMAP_TYPE
}
