/*=============================================================================
	BatmanLightMap.h: Batman-specific light map policies.
=============================================================================*/

#ifndef __BATMANLIGHTMAP_H__
#define __BATMANLIGHTMAP_H__

#if BATMAN

// Forward-declared; full definition in Inc/AmbientPlus3DirectionalLightSceneInfo.h.
class FAmbientPlus3DirectionalLightSceneInfo;

/**
 * BM2's "Ambient Plus 3 Directional Lights" lightmap policy.
 * Used for characters and other dynamic objects in Arkham City. The "lightmap"
 * is actually a small uniform block (3 light directions, 3 light colors, 1 ambient color)
 * supplied per-primitive from a FAmbientPlus3DirectionalLightSceneInfo proxy attached by
 * UAmbientPlus3DirectionalLightComponent.
 */
class FAPlus3DLightLightMapPolicy
{
public:

	typedef const FAmbientPlus3DirectionalLightSceneInfo* ElementDataType;

	static const UBOOL bDrawLitTranslucencyUnlit = FALSE;
	static const UBOOL bAllowPreviewSkyLight = FALSE;

	struct VertexParametersType
	{
		FShaderParameter APlus3DLightVertexInfoParameter;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			APlus3DLightVertexInfoParameter.Bind(ParameterMap, TEXT("APlus3DLightVertexInfo"), TRUE);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << APlus3DLightVertexInfoParameter;
		}
	};

	struct PixelParametersType
	{
		FShaderParameter APlus3DLightPixelInfoParameter;

		void Bind(const FShaderParameterMap& ParameterMap)
		{
			APlus3DLightPixelInfoParameter.Bind(ParameterMap, TEXT("APlus3DLightPixelInfo"), TRUE);
		}

		void Serialize(FArchive& Ar)
		{
			Ar << APlus3DLightPixelInfoParameter;
		}
	};

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType, UBOOL bEnableSkyLight=FALSE)
	{
		// Mirrors BM2's FAPlus3DLightLightMapPolicy::ShouldCache (sub_10C0F30): no skylight variant,
		// no unlit materials, compile for any vertex factory that supports dynamic OR static lighting.
		if (bEnableSkyLight)
		{
			return FALSE;
		}
		if (Material->GetLightingModel() == MLM_Unlit)
		{
			return FALSE;
		}
		return VertexFactoryType->SupportsDynamicLighting() || VertexFactoryType->SupportsStaticLighting();
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("ENABLE_A_PLUS_THREE_D_LIGHT"), TEXT("1"));
		OutEnvironment.Definitions.Set(TEXT("NUM_LIGHTMAP_COEFFICIENTS"),*FString::Printf(TEXT("%u"),NUM_DIRECTIONAL_LIGHTMAP_COEF));
	}

	void Set(
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView* View
		) const
	{
		check(VertexFactory);
		VertexFactory->Set();
	}

	void GetVertexDeclarationInfo(FVertexDeclarationRHIParamRef& VertexDeclaration, DWORD* StreamStrides, const FVertexFactory* VertexFactory) const
	{
		check(VertexFactory);
		VertexFactory->GetStreamStrides(StreamStrides);
		VertexDeclaration = VertexFactory->GetDeclaration();
	}

	void SetMesh(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const VertexParametersType* VertexShaderParameters,
		const PixelParametersType* PixelShaderParameters,
		FShader* VertexShader,
		FShader* PixelShader,
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const ElementDataType& AmbientPlus3DirectionalLight
		) const;

	friend UBOOL operator==(const FAPlus3DLightLightMapPolicy A, const FAPlus3DLightLightMapPolicy B)
	{
		return TRUE;
	}

	friend INT Compare(const FAPlus3DLightLightMapPolicy& A, const FAPlus3DLightLightMapPolicy& B)
	{
		return 0;
	}
};

#endif // BATMAN

#endif // __BATMANLIGHTMAP_H__
