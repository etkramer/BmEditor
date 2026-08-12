// BM
class RInterpTrackInstDialogue extends InterpTrackInst
	native(Interpolation);

cpptext
{
	virtual void InitTrackInst(UInterpTrack* Track);
	virtual void TermTrackInst(UInterpTrack* Track);
}

var	float LastUpdatePosition;
var export editinline transient AkComponent WwiseMatineeDialogueAudioComp;
var transient AkWwise.AkSoundHandle WwiseMatineeDialogueSoundHandle;
var transient float fSeekPos;
var transient RDialogueEvent LastSubtitle;
var transient bool LoadedBanks;
var transient bool bFirstUpdate;

defaultproperties
{
	fSeekPos=-1.f
}
