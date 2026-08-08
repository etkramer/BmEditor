// BM
class AkAssetPack extends AkHash
	native;

var(Banks)		array<AkBank>	AssetPackBanks;
var(Importing)	bool			ImportEvents;
var(Importing)	bool			ImportParameters;
var(Importing)	bool			ImportSwitches;
var(Importing)	bool			ImportStates;
var(Importing)	bool			ImportTriggers;
var(Importing)	bool			ImportEnvironments;
var(Prep)		bool			AllEventsPrep;
var const transient bool		IsLoaded;
var const transient bool		ShouldBeLoaded;
var(Prep) editconst array<editconst int> PrepareEventIDs;

native final function LoadAssetPack();
native final function UnloadAssetPack();

defaultproperties
{
	ImportEvents=true
	ImportParameters=true
	ImportSwitches=true
	ImportStates=true
	ImportTriggers=true
	ImportEnvironments=true
}
