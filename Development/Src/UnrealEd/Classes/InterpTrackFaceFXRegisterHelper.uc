// BM
class InterpTrackFaceFXRegisterHelper extends InterpTrackHelper
	native;

cpptext
{
	/** Prompts for the FaceFX register the new track should drive. */
	virtual	UBOOL PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, UBOOL bDuplicatingTrack, UBOOL bAllowPrompts ) const;

	/** Assigns the chosen register to the newly added track. */
	virtual void  PostCreateTrack( UInterpTrack *Track, UBOOL bDuplicatingTrack, INT TrackIndex ) const;
}
