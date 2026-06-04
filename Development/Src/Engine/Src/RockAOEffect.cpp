/*=============================================================================
	RockAOEffect.cpp: Batman: Arkham City ambient occlusion post process.

	NOTE: This is currently scaffolding so the native class links and BM2
	packages referencing URockAO load correctly. The screen-space AO render
	passes (depth downsample -> occlusion -> edge-preserving filter -> apply)
	are still to be ported; CreateSceneProxy returns NULL until then.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(URockAO);

FPostProcessSceneProxy* URockAO::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	// TODO: port FRockAOSceneProxy (RockAO occlusion + edge-preserving filter passes).
	return NULL;
}

UBOOL URockAO::IsShown(const FSceneView* View) const
{
	return Super::IsShown(View);
}
