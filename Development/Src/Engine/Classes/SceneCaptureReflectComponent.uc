/**
 * SceneCaptureReflectComponent
 *
 * Captures the reflection of the current view to a
 * 2D texture render target.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class SceneCaptureReflectComponent extends SceneCaptureComponent
	native;

/** render target resource to set as target for capture */
var(Capture) TextureRenderTarget2D TextureTarget;
/** scale field of view so that there can be some overdraw */
var(Capture) float ScaleFOV;
/** far plane clip distance: <= 0 means no far plane */
var(Capture) float FarClip;
/** only render primitives flagged as reflection-only */
var(Capture) bool bOnlyShowReflectionObjects;
/** capture is rendering from the reflected view */
var(Capture) bool bIsViewReflect;
/** enables BM2 cape-specific reflection depth bias */
var(Capture) bool bEnableCapeDepthBiasHack;

cpptext
{
public:

	// UActorComponent interface

	/**
	* Attach a new reflect capture component
	*/
	virtual void Attach();

	// SceneCaptureComponent interface

	/**
	* Create a new probe with info needed to render the scene
	*/
	virtual class FSceneCaptureProbe* CreateSceneCaptureProbe();
}

defaultproperties
{
	ScaleFOV=1.f
	FrameRate=1000
}
