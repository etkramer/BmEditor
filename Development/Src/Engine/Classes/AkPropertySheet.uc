// BM
class AkPropertySheet extends ActorComponent
	native;

// BM: retail declares this noexport (real AkStackable vtable); exported here as we have no C++ base.
var private native const pointer VfTable_AkStackable;

var(ActionSetup) editoronly array<AkAudioAction> Actions;
var(PropertySheetSetup) private bool AutoActivation;
var const transient bool IsActive;
var const transient bool Suspended;
var(StackSetup) AkStackName ActionStackName;
var(StackSetup) int ActionStackPriority;
var private transient float CurrentActivationValue;
var private transient float LastActivationValue;

defaultproperties
{
	AutoActivation=true
	Suspended=true
}
