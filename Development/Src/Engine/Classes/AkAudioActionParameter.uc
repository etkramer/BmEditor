// BM
class AkAudioActionParameter extends AkAudioAction
	native
	editinlinenew
	collapsecategories;

var(ActionParameterSetup)	const Actor				ActionTarget;
var(ActionParameterSetup)	const AkParameterName	ActionParameter;
var(OnActivation)			const float				ActivationValue;
var(OnActivation)			const float				ActivationInterpolationTime;
var(OnActivation)			const bool				ActivationIsAbsolute;
var(OnDeactivation)			const bool				DeactivationIsAbsolute;
var(OnDeactivation)			const float				DeactivationValue;
var(OnDeactivation)			const float				DeactivationInterpolationTime;
var private transient float		TargetActivationValue;
var private transient float		TargetDeactivationValue;
var private transient float		CurrentParameterValue;
var private transient float		CurrentParameterVelocity;
var private transient double	LastUpdateTime;

defaultproperties
{
	ActivationIsAbsolute=true
	DeactivationIsAbsolute=true
}
