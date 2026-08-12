// BM
class RInterpTrackDialogue extends InterpTrackVectorBase
	native(Interpolation);

cpptext
{
	virtual void PostLoad();

	// InterpTrack interface
	virtual INT GetNumKeyframes() const;
	virtual void GetTimeRange(FLOAT& StartTime, FLOAT& EndTime) const;
	virtual FLOAT GetKeyframeTime(INT KeyIndex) const;
	virtual INT AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode);
	virtual INT SetKeyframeTime(INT KeyIndex, FLOAT NewKeyTime, UBOOL bUpdateOrder=true);
	virtual void RemoveKeyframe(INT KeyIndex);
	virtual INT DuplicateKeyframe(INT KeyIndex, FLOAT NewKeyTime);
	virtual UBOOL GetClosestSnapPosition(FLOAT InPosition, TArray<INT> &IgnoreKeys, FLOAT& OutPosition);

	virtual void PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst);
	virtual void UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump);
	virtual void PreviewStopPlayback(class UInterpTrackInst* TrInst);

	/** Get the name of the class used to help out when adding tracks, keys, etc. in UnrealEd. */
	virtual const FString GetEdHelperClassName() const;

	virtual class UMaterial* GetTrackIcon() const;
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params );

	/** Whether or not this track is allowed to be used on static actors. */
	virtual UBOOL AllowStaticActors() { return TRUE; }

	// RInterpTrackDialogue interface
	/** Returns the key at the specified position in the track. */
	struct FDialogueTrackKey& GetDialogueTrackKeyAtPosition(FLOAT InPosition);

	/** Get the FaceFX animation and dialogue event active at the given time. */
	void GetSeqInfoForTime( FLOAT InTime, FString& OutGroupName, FString& OutSeqName, FLOAT& OutPosition, FLOAT& OutSeqStart, class URDialogueEvent*& OutLine );

	virtual void SetTrackToSensibleDefault();
}

/** Information for one line of dialogue in the track. */
struct native DialogueTrackKey
{
	var		float			Time;
	var()	RDialogueEvent	Line;
	var		float			WwiseDuration;
	var		float			SubtitleDuration;

	structdefaultproperties
	{
		WwiseDuration=-1.f
	}
};

/** Array of dialogue lines to play at specific times. */
var array<DialogueTrackKey> Dialogues;

/** If true, dialogue on this track will not be forced to finish when the matinee sequence finishes. */
var() bool bContinueDialogueOnMatineeEnd;
/** If true, no subtitles are shown for this track. */
var() bool bSuppressSubtitles;
/** If true, only subtitles are played - no audio is started and no banks are loaded. */
var() bool bSubTitlesOnly;
var() bool bTVSubtitles;
var() bool bStopDialogueOnMatineeSkip;
var() bool bHighPriorityStream;
var() float SubtitleTimeOffset;

defaultproperties
{
	TrackInstClass=class'Engine.RInterpTrackInstDialogue'
	TrackTitle="Dialogue"
}
