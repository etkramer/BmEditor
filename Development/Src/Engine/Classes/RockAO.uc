/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * Batman: Arkham City ambient occlusion post process effect (RockAO).
 */
class RockAO extends PostProcessEffect
	native;

var(AmbientOcclusion) float RSContrast;
var(AmbientOcclusion) float RSRadiusMax;
var(AmbientOcclusion) float RSRadiusMin;
var(AmbientOcclusion) float RSRadiusScale;
var(AmbientOcclusion) float RSDepthTestRadius;
var(AmbientOcclusion) float RSEdgeThreshold;

var(AmbientOcclusion) LinearColor OcclusionColor;
var(AmbientOcclusion) float OcclusionPower;
var(AmbientOcclusion) float OcclusionScale;
var(AmbientOcclusion) float OcclusionBias;
var(AmbientOcclusion) float MinOcclusion;
var(AmbientOcclusion) float OcclusionRadius;
var(AmbientOcclusion) float OcclusionAttenuation;
var(AmbientOcclusion) float OcclusionFadeoutMinDistance;
var(AmbientOcclusion) float OcclusionFadeoutMaxDistance;

var(AmbientOcclusion) float HaloDistanceThreshold;
var(AmbientOcclusion) float HaloDistanceScale;
var(AmbientOcclusion) float HaloOcclusion;

var(AmbientOcclusion) float EdgeDistanceThreshold;
var(AmbientOcclusion) float EdgeDistanceScale;

var(AmbientOcclusion) float FilterDistanceScale;
var(AmbientOcclusion) int FilterSize;

cpptext
{
	// UPostProcessEffect interface
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);
	virtual UBOOL IsShown(const FSceneView* View) const;
}

defaultproperties
{
	bShowInEditor=TRUE
	bShowInGame=TRUE
}
