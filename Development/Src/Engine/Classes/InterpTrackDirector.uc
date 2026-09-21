class InterpTrackDirector extends InterpTrack
	native(Interpolation);

/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * A track type used for binding the view of a Player (attached to this tracks group) to the actor of a different group.
 *
 */

cpptext
{
	// InterpTrack interface
	virtual INT GetNumKeyframes() const;
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const;
	virtual FLOAT GetTrackEndTime() const;
	virtual FLOAT GetKeyframeTime(INT KeyIndex) const;
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);
	virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition);

	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd.
	* @return	String name of the helper class.*/
	virtual const FString	GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );

	// InterpTrackDirector interface
	FName GetViewedGroupName(FLOAT CurrentTime, FLOAT& CutTime, FLOAT& CutTransitionTime);
}

// BM
enum ECameraBlendType
{
	CBT_PlayerWalkCamera,
	CBT_PlayerCombatCamera
};

/** Information for one cut in this track. */
struct immutablewhencooked native DirectorTrackCut
{
	/** Time to perform the cut. */
	var		float	Time;

	/** Time taken to move view to new camera. */
	var		float	TransitionTime;

	/** GroupName of InterpGroup to cut viewpoint to. */
	var()	name	TargetCamGroup;

	// BM
	var		int		ShotNumber;
	// BM
	var		editoronly array<InterpTrack>	BoundTracks;
};

var() bool bResetCameraBehindBatman;

var() bool bKeepBatmanOnScreen;

var() bool bDisableCamerCollisionDuringBlend;

var(Skip) bool bResetCameraBehindBatmanOnSkip;

/** True to allow clients to simulate their own camera cuts.  Can help with latency-induced timing issues. */
var() bool bSimulateCameraCutsOnClients;

var() bool bDetachMic;

// BM
var bool bLockedFromEdits;

// BM
var() bool bCinematicLightingMode;

// BM
var() bool bDontBlendBackToPlayerOnFinish;

var(Skip) float SkipBlendTime;

/** Array of cuts between cameras. */
var	array<DirectorTrackCut>	CutTrack;

// BM
var() ECameraBlendType PreviewBlendFrom;
// BM
var() ECameraBlendType PreviewBlendTo;
// BM
var() float PreviewBlendFromFOV;
// BM
var() float PreviewBlendToFOV;
// BM
var() float PreviewAspectRatio;

defaultproperties
{
	bOnePerGroup=true
	bDirGroupOnly=true
	TrackInstClass=class'Engine.InterpTrackInstDirector'
	TrackTitle="Director"
	bSimulateCameraCutsOnClients=TRUE
	PreviewAspectRatio=1.777777
}
