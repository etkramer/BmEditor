// BM
class InterpTrackFaceFXRegister extends InterpTrackFloatBase
	native(Interpolation);

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);

	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd. */
	virtual const FString GetEdHelperClassName() const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
}

/** Name of the FaceFX register this track drives. */
var() string Register;

defaultproperties
{
	TrackInstClass=class'Engine.InterpTrackInstFaceFXRegister'
	TrackTitle="FaceFX Register"
}
