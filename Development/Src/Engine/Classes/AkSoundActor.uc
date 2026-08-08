// BM
class AkSoundActor extends Actor
	abstract
	native
	notplaceable;

var() editoronly export AkComponent ActorAudioComponent;

native function AkComponent GetAkComponent(optional bool AllowCreate = true);

defaultproperties
{
	Begin Object Class=AkComponent Name=AkSoundActorAkComponent
	End Object
	ActorAudioComponent=AkSoundActorAkComponent
	Components.Add(AkSoundActorAkComponent)

	bHidden=true
}
