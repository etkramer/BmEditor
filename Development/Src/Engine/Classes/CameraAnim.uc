
/**
 *	CameraAnim: defines a pre-packaged animation to be played on a camera.
 * 	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class CameraAnim extends Object
	notplaceable
	native(Camera);

/** The InterpGroup that holds our actual interpolation data. */
var InterpGroup		CameraInterpGroup;

/** Length, in seconds. */
var const float		AnimLength;

/** AABB in local space. */
var const box		BoundingBox;

/** The "base" postprocess settings to use, to support non-animating settings. */
var const PostProcessSettings	BasePPSettings;
var const float					BasePPSettingsAlpha;

/** The */
var const float		BaseFOV;


cpptext
{
protected:
	void CalcLocalAABB();

public:
	/** Overridden to calculate the bbox at save time. */
	virtual void PreSave();
	virtual void PostLoad();

	UBOOL CreateFromInterpGroup(class UInterpGroup* SrcGroup, class USeqAct_Interp* Interp);
	FBox GetAABB(FVector const& BaseLoc, FRotator const& BaseRot, FLOAT Scale) const;
};

defaultproperties
{
	AnimLength=3.f
	BaseFOV=90

	BasePPSettingsAlpha=1.f

	// override nothing unless explicitly chosen
	BasePPSettings={(
		bOverride_InterpolateOverDistance=FALSE,
		bOverride_InterpolateOverDistanceFade=FALSE,
		bOverride_bEnableHighQualityDOF=FALSE,
		bOverride_EnableBloom=FALSE,
		bOverride_EnableDOF=FALSE,
		bOverride_EnableMotionBlur=FALSE,
		bOverride_EnableSceneEffect=FALSE,
		bOverride_AllowAmbientOcclusion=FALSE,
		bOverride_BloomOverload=FALSE,
		bOverride_BloomLowerCut=FALSE,
		bOverride_DOF_ApertureStop=FALSE,
		bOverride_DOF_FocusDistance=FALSE,
		bOverride_DOF_InterpolationDuration=FALSE,
		bOverride_MotionBlur_MaxVelocity=FALSE,
		bOverride_MotionBlur_Amount=FALSE,
		bOverride_MotionBlur_FullMotionBlur=FALSE,
		bOverride_MotionBlur_CameraRotationThreshold=FALSE,
		bOverride_MotionBlur_CameraTranslationThreshold=FALSE,
		bOverride_MotionBlur_InterpolationDuration=FALSE,
		bOverride_Scene_Desaturation=FALSE,
		bOverride_Scene_ImageGrainScale=FALSE,
		bOverride_Scene_HighLights=FALSE,
		bOverride_Scene_MidTones=FALSE,
		bOverride_Scene_Shadows=FALSE,
		bOverride_Scene_InterpolationDuration=FALSE,
	)}
}
