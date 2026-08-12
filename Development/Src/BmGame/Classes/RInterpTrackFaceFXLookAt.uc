class RInterpTrackFaceFXLookAt extends InterpTrackFloatBase
	native;

cpptext
{
	// InterpTrack interface
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);

	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
}

/** Actor the group's FaceFX head is driven to look at. */
var() Actor Target;
var() string YawRegisterName;
var() string PitchRegisterName;

defaultproperties
{
	YawRegisterName="A_Look_Yaw"
	PitchRegisterName="A_Look_Pitch"

	TrackInstClass=class'BmGame.RInterpTrackInstFaceFXLookAt'
	TrackTitle="FaceFX LookAt"
}
