/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class AmbientPlus3DirectionalLightComponent extends LightComponent
	native(Light)
	hidecategories(Object)
	editinlinenew;

/** Three representative light directions, in world space. */
var() Vector LightDirections[3];

/** Three representative light colors. */
var() Vector LightColours[3];

/** Ambient color added unconditionally to lit surfaces. */
var() Vector Ambient;

/**
 * If TRUE, the AP3D light can be combined into the base pass as an optimization.
 * If FALSE, it will be rendered after modulated shadows (matches SHLight semantics).
 */
var bool bRenderBeforeModShadows;

cpptext
{
	// ULightComponent interface.
	virtual FLightSceneInfo* CreateSceneInfo() const;
	virtual FVector4 GetPosition() const;
	virtual ELightComponentType GetLightType() const;
}

defaultproperties
{
	CastShadows=False
	bRenderBeforeModShadows=True
}
