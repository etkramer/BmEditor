// BM
class AkAudioAction extends Object
	abstract
	native
	collapsecategories;

var(Stacking)	bool	Stacked;
var(Muting)		const bool EnableAction;
var private transient int ActivationCount;

defaultproperties
{
	EnableAction=true
}
