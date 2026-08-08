// BM
class AkEnvironmentName extends AkAsset
	native;

var(Rolloff)			bool	EnableRolloff;
var(Occlusion)			bool	EnableOcclusionSends;
var(RolloffDistances)	float	EnvironmentRolloffDistanceMin;
var(RolloffDistances)	float	EnvironmentRolloffDistanceMax;
var(SendLevels)			float	EnvironmentDryMin;
var(SendLevels)			float	EnvironmentDryMax;
var(SendLevels)			float	EnvironmentRolloffSendMin;
var(SendLevels)			float	EnvironmentRolloffSendMax;
var(SpecialSetup)		float	EnvironmentRolloffSendPlayer;
var(SpecialSetup)		float	EnvironmentRolloffSendNoListener;
var(SpecialSetup)		float	EnvironmentRolloffSendOnlyListener;
var(SpecialSetup)		float	EnvironmentRolloffSend2DMode;
var(Dialogue)			float	EnhancedWetSendMultiplier;
var(Dialogue)			float	EnhancedWetSendFalloff;
var(Dialogue)			float	DialogueMeterWetBoost;
var(Dialogue)			float	DialogueMeterWetBoostPlayer;
var(Occlusion)			float	EnvironmentWetSendOccludedMultiplier;
var(Occlusion)			float	EnvironmentWetSendNotOccludedMultiplier;
var(Occlusion)			float	EnvironmentDrySendOccludedMultiplier;
var(Occlusion)			float	EnvironmentDrySendNotOccludedMultiplier;

defaultproperties
{
	EnableRolloff=true
	EnableOcclusionSends=true
	EnvironmentRolloffDistanceMin=2.0
	EnvironmentRolloffDistanceMax=40.0
	EnvironmentDryMin=-1.0
	EnvironmentDryMax=-1.0
	EnvironmentRolloffSendMin=0.05
	EnvironmentRolloffSendMax=1.0
	EnvironmentRolloffSendPlayer=-1.0
	EnvironmentRolloffSendNoListener=-1.0
	EnvironmentRolloffSendOnlyListener=-1.0
	EnvironmentRolloffSend2DMode=0.05
	EnhancedWetSendFalloff=25.0
	DialogueMeterWetBoost=0.33
	DialogueMeterWetBoostPlayer=0.075
	EnvironmentWetSendOccludedMultiplier=1.0
	EnvironmentWetSendNotOccludedMultiplier=1.0
	EnvironmentDrySendOccludedMultiplier=1.0
	EnvironmentDrySendNotOccludedMultiplier=1.0
}
