// BM
class AkAudioActionAltitude extends AkAudioAction
	native
	dependson(AkWwise)
	editinlinenew
	collapsecategories;

var(AltitudeMapping)	const AkWwise.EListenerID	AltitudeFollowListener;
var(AltitudeMapping)	const AkParameterName		AltitudeParameter;
var(AltitudeSettings)	const float					Highest;
var(AltitudeSettings)	const float					Lowest;
var transient float		AltitudeParameterCurrent;
var transient double	AltitudeParameterLastUpdate;

defaultproperties
{
	Stacked=true
}
