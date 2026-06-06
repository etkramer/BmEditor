/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * Rocksteady uber post process effect (RockOn). Performs DOF, bloom,
 * motion blur, scene color grading and tone mapping in a single pass, and
 * composites the RockAtmos atmospherics.
 */
class RockOn extends DOFBloomMotionBlurEffect
	native
	dependson(PostProcessVolume);

var(Scene) vector SceneShadows<DisplayName=Shadows>;
var(Scene) vector SceneHighLights<DisplayName=HighLights>;
var(Scene) vector SceneMidTones<DisplayName=MidTones>;
var(Scene) float  SceneDesaturation<DisplayName=Desaturation>;
var(Scene) vector SceneColorize<DisplayName=Colorize>;

/** The radius of the soft edge for motion blur. A value bigger than 0 enables soft edge motion blur. */
var(MotionBlur) float MotionBlurSoftEdgeKernelSize<DisplayName=SoftEdgeKernelSize>;

/** Whether the image grain (noise) is enabled, to fight 8 bit quantization artifacts and to simulate film grain. */
var(Scene) bool bEnableImageGrain;

/** Image grain scale, only affects the darks, >=0, 0:none, 1(strong) should be less than 1 */
var(Scene) float SceneImageGrainScale;

/** LUTBlender parameters used last frame. */
var const native transient LUTBlender PreviousLUTBlender;

cpptext
{
	// UPostProcessEffect interface
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);

	// UObject interface
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual UBOOL IncludesUberpostprocess() const
	{
		return TRUE;
	}

	virtual void OnPostProcessWarning(FString& OutWarning) const
	{
		// RockOn is the intended uber post process; no warning.
	}
}

defaultproperties
{
	SceneShadows=(X=0.0,Y=0.0,Z=-0.003)
	SceneHighLights=(X=0.8,Y=0.8,Z=0.8)
	SceneMidTones=(X=1.3,Y=1.3,Z=1.3)
	SceneDesaturation=0.4
	SceneColorize=(X=1,Y=1,Z=1)
	bEnableImageGrain=FALSE
	SceneImageGrainScale=0.02
	bShowInEditor=TRUE
	bShowInGame=TRUE
}
