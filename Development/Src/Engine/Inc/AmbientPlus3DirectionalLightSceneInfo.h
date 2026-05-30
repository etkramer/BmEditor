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

// Stand-in lighting policy referenced by TLightSceneDPGInfo. Retail BM2 carries a
// TLightSceneDPGInfo<FAmbientPlus3DirectionalLightPolicy> DPGInfos[SDPG_MAX_SceneRender]
// member on the proxy. We don't currently dispatch a standalone AP3D light pass, so the
// draw lists stay empty - but FLightSceneInfo::Detach iterates them, so the array still
// has to exist.
class FAmbientPlus3DirectionalLightPolicy
{
public:
	typedef FAmbientPlus3DirectionalLightSceneInfo SceneInfoType;

	class VertexParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap) {}
		template<typename ShaderRHIParamRef>
		void SetLight(ShaderRHIParamRef Shader, const SceneInfoType* Light, const FSceneView* View) const {}
		void Serialize(FArchive& Ar) {}
	};

	class PixelParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap) {}
		void SetLight(FShader* PixelShader, const SceneInfoType* Light, const FSceneView* View) const {}
		void SetLightMesh(FShader* PixelShader, const FPrimitiveSceneInfo* PrimitiveSceneInfo, const SceneInfoType* Light, UBOOL bApplyLightFunctionDisabledBrightness) const {}
		void Serialize(FArchive& Ar) {}
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
		) const { return FALSE; }
	virtual class FShadowProjectionPixelShaderInterface* GetModShadowProjPixelShader(UBOOL bRenderingBeforeLight) const { return NULL; }
	virtual class FBranchingPCFProjectionPixelShaderInterface* GetBranchingPCFModProjPixelShader(UBOOL bRenderingBeforeLight) const { return NULL; }
	virtual FGlobalBoundShaderState* GetModShadowProjBoundShaderState(UBOOL bRenderingBeforeLight) const { return NULL; }
	virtual FGlobalBoundShaderState* GetBranchingPCFModProjBoundShaderState(UBOOL bRenderingBeforeLight) const { return NULL; }

private:
	/** If TRUE, AP3D is merged into the base pass; otherwise rendered after modulated shadows. */
	const BITFIELD bRenderBeforeModShadows : 1;

	TLightSceneDPGInfo<FAmbientPlus3DirectionalLightPolicy> DPGInfos[SDPG_MAX_SceneRender];
};

#endif // BATMAN

#endif // __AMBIENTPLUS3DIRECTIONALLIGHTSCENEINFO_H__
