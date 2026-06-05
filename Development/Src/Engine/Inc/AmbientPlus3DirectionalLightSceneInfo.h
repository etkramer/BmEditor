/*=============================================================================
	AmbientPlus3DirectionalLightSceneInfo.h: Render-thread proxy for the BM2
	AP3D light. Lives in a header so the AP3D base-pass light-map policy can
	include it directly when implementing SetMesh.
=============================================================================*/

#ifndef __AMBIENTPLUS3DIRECTIONALLIGHTSCENEINFO_H__
#define __AMBIENTPLUS3DIRECTIONALLIGHTSCENEINFO_H__

#if BATMAN

#include "LightSceneInfo.h"
#include "LightRendering.h"

class FAmbientPlus3DirectionalLightSceneInfo;

// Lighting policy used by BM2's standalone AP3D light pass.
class FAmbientPlus3DirectionalLightPolicy
{
public:
	typedef FAmbientPlus3DirectionalLightSceneInfo SceneInfoType;

	class VertexParametersType
	{
	public:
		FShaderParameter APlus3DLightVertexInfoParameter;

		void Bind(const FShaderParameterMap& ParameterMap);
		template<typename ShaderRHIParamRef>
		void SetLight(ShaderRHIParamRef Shader, const SceneInfoType* Light, const FSceneView* View) const;
		void Serialize(FArchive& Ar);
	};

	class PixelParametersType
	{
	public:
		FShaderParameter APlus3DLightPixelInfoParameter;

		void Bind(const FShaderParameterMap& ParameterMap);
		void SetLight(FShader* PixelShader, const SceneInfoType* Light, const FSceneView* View) const;
		void SetLightMesh(FShader* PixelShader, const FPrimitiveSceneInfo* PrimitiveSceneInfo, const SceneInfoType* Light, UBOOL bApplyLightFunctionDisabledBrightness) const {}
		void Serialize(FArchive& Ar);
	};

	static UBOOL ShouldCacheStaticLightingShaders() { return FALSE; }
	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType) { return FALSE; }
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment) {}
};

class FAmbientPlus3DirectionalLightSceneInfo : public FLightSceneInfo
{
public:

	FVector LightDirections[3];
	FVector LightColours[3];
	FVector Ambient;

	FAmbientPlus3DirectionalLightSceneInfo(const class UAmbientPlus3DirectionalLightComponent* Component);

	virtual void AttachPrimitive(const FLightPrimitiveInteraction& Interaction);
	virtual void DetachPrimitive(const FLightPrimitiveInteraction& Interaction);

	// FLightSceneInfo overrides - this is a non-spatial, non-shadow-casting light.
	virtual UBOOL AffectsBounds(const FBoxSphereBounds& Bounds) const { return TRUE; }
	virtual UBOOL GetProjectedShadowInitializer(const FBoxSphereBounds& SubjectBounds, FProjectedShadowInitializer& OutInitializer) const { return FALSE; }
	virtual const FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex) const
	{
		check(DPGIndex < SDPG_MAX_SceneRender);
		return &DPGInfos[DPGIndex];
	}
	virtual FLightSceneDPGInfoInterface* GetDPGInfo(UINT DPGIndex)
	{
		check(DPGIndex < SDPG_MAX_SceneRender);
		return &DPGInfos[DPGIndex];
	}
	virtual UBOOL DrawTranslucentMesh(
		const FSceneView& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		UBOOL bUseTranslucencyLightAttenuation,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const class FProjectedShadowInfo* TranslucentPreShadowInfo,
		FHitProxyId HitProxyId
		) const;
	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const { return NULL; }
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const { return NULL; }
	virtual FGlobalBoundShaderState* GetModShadowProjBoundShaderState(UBOOL bRenderingBeforeLight) const { return NULL; }
	virtual FGlobalBoundShaderState* GetBranchingPCFModProjBoundShaderState(UBOOL bRenderingBeforeLight) const { return NULL; }

private:
	/** If TRUE, AP3D is merged into the base pass; otherwise rendered after modulated shadows. */
	const BITFIELD bRenderBeforeModShadows : 1;

	TLightSceneDPGInfo<FAmbientPlus3DirectionalLightPolicy> DPGInfos[SDPG_MAX_SceneRender];
};

inline void FAmbientPlus3DirectionalLightPolicy::VertexParametersType::Bind(const FShaderParameterMap& ParameterMap)
{
	APlus3DLightVertexInfoParameter.Bind(ParameterMap, TEXT("APlus3DLightVertexInfo"), TRUE);
}

template<typename ShaderRHIParamRef>
inline void FAmbientPlus3DirectionalLightPolicy::VertexParametersType::SetLight(ShaderRHIParamRef Shader, const SceneInfoType* Light, const FSceneView* View) const
{
	check(Light);

	FVector4 Dirs[3];
	for (INT i = 0; i < 3; i++)
	{
		Dirs[i] = FVector4(Light->LightDirections[i], 0.0f);
	}
	SetVertexShaderValues<FVector4>(
		Shader,
		APlus3DLightVertexInfoParameter,
		Dirs,
		3);
}

inline void FAmbientPlus3DirectionalLightPolicy::VertexParametersType::Serialize(FArchive& Ar)
{
	Ar << APlus3DLightVertexInfoParameter;
}

inline void FAmbientPlus3DirectionalLightPolicy::PixelParametersType::Bind(const FShaderParameterMap& ParameterMap)
{
	APlus3DLightPixelInfoParameter.Bind(ParameterMap, TEXT("APlus3DLightPixelInfo"), TRUE);
}

inline void FAmbientPlus3DirectionalLightPolicy::PixelParametersType::SetLight(FShader* PixelShader, const SceneInfoType* Light, const FSceneView* View) const
{
	check(Light);

	FVector4 ColorsAndAmbient[4];
	for (INT i = 0; i < 3; i++)
	{
		ColorsAndAmbient[i] = FVector4(Light->LightColours[i], 0.0f);
	}
	ColorsAndAmbient[3] = FVector4(Light->Ambient, 0.0f);
	SetPixelShaderValues<FVector4>(
		PixelShader->GetPixelShader(),
		APlus3DLightPixelInfoParameter,
		ColorsAndAmbient,
		4);
}

inline void FAmbientPlus3DirectionalLightPolicy::PixelParametersType::Serialize(FArchive& Ar)
{
	Ar << APlus3DLightPixelInfoParameter;
}

#endif // BATMAN

#endif // __AMBIENTPLUS3DIRECTIONALLIGHTSCENEINFO_H__
