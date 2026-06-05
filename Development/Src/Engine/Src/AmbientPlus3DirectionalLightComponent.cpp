/*=============================================================================
	AmbientPlus3DirectionalLightComponent.cpp: BM2's AP3D light - replaces the
	standard secondary SH/Sky light produced by FDynamicLightEnvironmentState
	with a 3-directional + ambient representation that the AP3D base-pass
	light-map policy can consume.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "AmbientPlus3DirectionalLightSceneInfo.h"

#if BATMAN

IMPLEMENT_CLASS(UAmbientPlus3DirectionalLightComponent);

// Registers the shader types referenced by TStaticMeshDrawList<TMeshLightingDrawingPolicy<...,FAmbientPlus3DirectionalLightPolicy>>
// inside our DPGInfos array. ShouldCache returns FALSE for everything so the corrupt placeholder .usf names are never read.
IMPLEMENT_SHADOWLESS_LIGHT_SHADER_TYPE(FAmbientPlus3DirectionalLightPolicy,TEXT("AmbientPlus3DirectionalLightVertexShader"),TEXT("AmbientPlus3DirectionalLightPixelShader"),VER_TRANSLUCENT_PRESHADOWS,0)

FAmbientPlus3DirectionalLightSceneInfo::FAmbientPlus3DirectionalLightSceneInfo(
	const UAmbientPlus3DirectionalLightComponent* Component
	)
:	FLightSceneInfo(Component)
,	bRenderBeforeModShadows(Component->bRenderBeforeModShadows)
,	Ambient(Component->Ambient)
{
	for (INT i = 0; i < 3; i++)
	{
		LightDirections[i] = Component->LightDirections[i];
		LightColours[i] = Component->LightColours[i];
	}
}

void FAmbientPlus3DirectionalLightSceneInfo::AttachPrimitive(const FLightPrimitiveInteraction& Interaction)
{
	if (LightEnvironment && LightEnvironment == Interaction.GetPrimitiveSceneInfo()->LightEnvironment)
	{
		Interaction.GetPrimitiveSceneInfo()->AmbientPlus3DLight = this;
		Interaction.GetPrimitiveSceneInfo()->bRenderAPlus3DLightInBasePass = bRenderBeforeModShadows;
		// Static meshes need their cached drawing-policy bound state regenerated to use the AP3D policy.
		Interaction.GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
	}
}

void FAmbientPlus3DirectionalLightSceneInfo::DetachPrimitive(const FLightPrimitiveInteraction& Interaction)
{
	if (Interaction.GetPrimitiveSceneInfo()->AmbientPlus3DLight == this)
	{
		Interaction.GetPrimitiveSceneInfo()->AmbientPlus3DLight = NULL;
		Interaction.GetPrimitiveSceneInfo()->bRenderAPlus3DLightInBasePass = FALSE;
		Interaction.GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
	}
}

FLightSceneInfo* UAmbientPlus3DirectionalLightComponent::CreateSceneInfo() const
{
	return new FAmbientPlus3DirectionalLightSceneInfo(this);
}

FVector4 UAmbientPlus3DirectionalLightComponent::GetPosition() const
{
	// Non-spatial: report a w=0 directional-light style position so light culling treats us as infinite.
	return FVector4(0, 0, 1, 0);
}

ELightComponentType UAmbientPlus3DirectionalLightComponent::GetLightType() const
{
	return LightType_AmbientPlus3Directional;
}

UBOOL FAmbientPlus3DirectionalLightSceneInfo::DrawTranslucentMesh(
	const FSceneView& View,
	const FMeshElement& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	UBOOL bUseTranslucencyLightAttenuation,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FProjectedShadowInfo* TranslucentPreShadowInfo,
	FHitProxyId HitProxyId
	) const
{
	const FLOAT MaxLight =
		Max(
			Max(LightColours[0].GetMax(), LightColours[1].GetMax()),
			Max(LightColours[2].GetMax(), Ambient.GetMax()));

	if (MaxLight > 0.0f)
	{
		return DrawLitDynamicMesh<FAmbientPlus3DirectionalLightPolicy>(
			View,
			this,
			Mesh,
			bBackFace,
			bPreFog,
			TRUE,
			bUseTranslucencyLightAttenuation,
			PrimitiveSceneInfo,
			TranslucentPreShadowInfo,
			HitProxyId);
	}
	return FALSE;
}

#endif // BATMAN
