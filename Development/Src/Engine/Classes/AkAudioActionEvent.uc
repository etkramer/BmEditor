// BM
class AkAudioActionEvent extends AkAudioAction
	native
	dependson(AkWwise)
	editinlinenew
	collapsecategories;

var() const Actor		ActionTarget;
var() const AkEvent		ActionEvent;
var const transient AkWwise.AkSoundHandle ActionSoundHandle;

defaultproperties
{
}
