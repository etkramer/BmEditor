// BM
class AkComponent extends ActorComponent
	native
	dependson(AkWwise);

enum EAkComponentUpdate
{
	AK_COMPONENT_UPDATE_SINGLE_AUTO,
	AK_COMPONENT_UPDATE_SINGLE_NO_AUTO,
	AK_COMPONENT_UPDATE_MULTIPOINT_AUTO,
	AK_COMPONENT_UPDATE_MULTIPOINT_NO_AUTO,
	AK_COMPONENT_UPDATE_NEVER,
	AK_COMPONENT_UPDATE_MAX
};

enum EAkComponentSourceCreateFailReason
{
	AK_COMPONENT_SOURCE_CREATE_FAIL_REASON_NONE,
	AK_COMPONENT_SOURCE_CREATE_FAIL_REASON_DISTANT,
	AK_COMPONENT_SOURCE_CREATE_FAIL_REASON_HIDDEN,
	AK_COMPONENT_SOURCE_CREATE_FAIL_REASON_DEAD,
	AK_COMPONENT_SOURCE_CREATE_FAIL_REASON_ERROR,
	AK_COMPONENT_SOURCE_CREATE_FAIL_REASON_MAX
};

var(LifetimeEvent)	AkEvent	LifetimeEvent;
var(LifetimeEvent)	bool	AutoPlayLifetimeEvent;
var(LOD)			bool	NeverAutoDestroySource;
var(LOD)			bool	AlwaysAutoCreateSource;
var(LOD)			bool	AutoDestroySourceWhenHidden;
var(LOD)			bool	AutoDestroySourceWhenDead;
var(LOD)			bool	KillSoundsOnDestroy;
var private const transient bool WasInsideLODRadius;
var(Parameters)		bool	EnableCameraDistanceParameters;
var(Parameters)		bool	EnableCameraOffsetParameters;
var(Parameters)		bool	EnableCameraAngleParameters;
var(Parameters)		bool	EnablePlayerDistanceParameters;
var(Parameters)		bool	EnablePlayerOffsetParameters;
var(Parameters)		bool	EnablePlayerAngleParameters;
var(Parameters)		bool	EnableObjectVelocityParameters;
var(Modes)			bool	Enable2DMode;
var(Metering)		bool	EnableDialogueMeter;
var transient		bool	DialogueMeterEnabled;
var(Occlusion)		bool	EnableOcclusionParameters;
var(Occlusion)		bool	ParameterOnlyOcclusion;
var(Environments)	bool	EnableEnvironments;
var(Environments)	bool	EnableNonListenerEnvironments;
var private editoronly const transient bool DebugLastOcclusionPrimaryHit;
var private editoronly const transient bool DebugLastOcclusionSecondaryAHit;
var private editoronly const transient bool DebugLastOcclusionSecondaryBHit;
var private const transient bool SourceActive;
var private const transient bool AuxSourceActive;
var					bool	UseAuxSource;
var					bool	DefaultAkComponent;

var private transient AkWwise.AkSoundHandle LifetimeEventHandle;
var private const transient float PeakAudiblityRadius;
var private const transient float LODDestroyTimer;
var(Parameters)		float	ObjectVelocityFilterRCI;
var(Parameters)		float	ObjectVelocityFilterRCD;
var(Metering)		AkEnvironmentName DialogueMeterEffect;
var(Occlusion)		float	OcclusionUpdateTimeFast;
var(Occlusion)		float	OcclusionUpdateTimeSlow;
var(Occlusion)		float	OcclusionInterpolationTime;
var(Occlusion)		float	OcclusionScalingDistance;
var(Occlusion)		float	OcclusionMultiplier;
var(Occlusion)		float	OcclusionMultiplierAux;
var(Modifiers)		float	WetDryMixVolume;
var(Modifiers)		float	WetDryMixVolumeAux;
var(Modifiers)		float	FalloffRadiusMultiplier;
var(Modifiers)		float	FalloffEnhancementRadius;
var transient		double	FalloffEnhancementTimestamp;
var private const transient double LastOcclusionUpdateTime;
var private const transient float CurrentOcclusion;
var private const transient float TargetOcclusion;
var private const transient float OcclusionVelocity;
var private editoronly const transient Vector DebugLastOcclusionPrimaryFrom;
var private editoronly const transient Vector DebugLastOcclusionPrimaryTo;
var private editoronly const transient Vector DebugLastOcclusionSecondaryAFrom;
var private editoronly const transient Vector DebugLastOcclusionSecondaryATo;
var private editoronly const transient Vector DebugLastOcclusionSecondaryBFrom;
var private editoronly const transient Vector DebugLastOcclusionSecondaryBTo;
var private const transient Vector SourcePosition;
var private const transient Rotator SourceOrientation;
var const transient float LastSourceVelocity;
var private const transient double LastSourceVelocityUpdateTime;
var EAkComponentUpdate SpatialUpdateType;
var transient EAkComponentSourceCreateFailReason DebugLastFailedSourceCreateReason;
var private const transient int SourceID;
var private const transient int AuxSourceID;
var private transient array<AkWwise.AkEnvironmentInfo> ActiveEnvironments;
var transient AkEvent DebugLastEvent;
var transient AkEvent DebugLastAuxEvent;
var transient double DebugLastFailedSourceCreateTime;

native final function CreateAudioSource();
native final function DestroyAudioSource();
native final function bool HasAudioSource();
native final function int GetAudioSourceID(optional bool AllowCreate = true);
native final function int GetAuxAudioSourceID(optional bool AllowCreate = true);
native final function bool CheckListenerProximity(optional bool UpdateSpatial = false);
native final function float GetDistanceToListener(AkWwise.EListenerID listenerID);
native final function AkWwise.AkSoundHandle StartAudioEvent(AkEvent AudioEvent, optional delegate<AkWwise.AkSoundCallback> SoundCallbackDelegate, optional int SoundCallbackFlags = 1);
native final function AkWwise.AkSoundHandle StartAuxAudioEvent(AkEvent AudioEvent, Vector Position, optional delegate<AkWwise.AkSoundCallback> SoundCallbackDelegate, optional int SoundCallbackFlags = 1);
native final function StopAudioEvent(out AkWwise.AkSoundHandle SoundHandle, optional bool QuickStop = false);
native final function bool IsSoundHandleValid(out AkWwise.AkSoundHandle SoundHandleToTest);
native final function KillSounds(optional bool DestroySources = false);
native final function SetSourceParameter(AkParameterName ParamName, float ParamValue);
native final function float GetSourceParameter(AkParameterName ParamName);
native final function SetSourceStickyAudioEvent(AkEvent AudioEvent);
native final function SetSourceStickyAudioEventEx(AkEvent AudioEvent, float NewAttackTime, float NewSustainTime, float NewReleaseTime, AkParameterName ParamName, optional float newSustainValue = 1.0, optional float NewReleaseValue = 0.0);
native final function SetSourceStickyParameter(AkParameterName ParamName, float ParamValue);
native final function SetSourceStickyParameterEx(AkParameterName ParamName, float ParamValue, float NewReleaseValue, float NewReleaseTime, float NewSustainTime, float NewAttackTime, bool AllowPause);
native final function SetSourceSwitch(AkSwitchName SwitchName);
native final function SetSurfaceSwitch(AkSwitchName SwitchName, AkSwitchName FallbackSwitchName, string CharacterName);
native final function SetSourceSpatial(Vector Position, optional Rotator Orientation, optional EAkComponentUpdate NewUpdateType = AK_COMPONENT_UPDATE_SINGLE_NO_AUTO);
native final function SetSourceSpatialMulti(array<AkWwise.AkSourceSpatial> MultiPositions, bool Additive, Vector PrimaryPosition, optional Rotator PrimaryOrientation, optional EAkComponentUpdate NewUpdateType = AK_COMPONENT_UPDATE_MULTIPOINT_NO_AUTO);
native final function SetSourceSpatialRay(Vector RayOrigin, Rotator RayOrientation, optional Rotator Orientation, optional EAkComponentUpdate NewUpdateType = AK_COMPONENT_UPDATE_SINGLE_NO_AUTO);
native final function SetSourceSpatialBeam(Vector BeamStartPoint, Vector BeamEndPoint, optional Rotator Orientation, optional EAkComponentUpdate NewUpdateType = AK_COMPONENT_UPDATE_SINGLE_NO_AUTO);
native final function RegisterEnvironments(array<AkWwise.AkEnvironmentSettings> Envs, optional bool SetListenerEnvironment = false);
native final function UnregisterEnvironments(array<AkWwise.AkEnvironmentSettings> Envs, optional bool ClearListenerEnvironment = false);
native final function RegisterOcclusionMultipliers(float Multiplier, float MultiplierAux);
native final function UnregisterOcclusionMultipliers(float Multiplier, float MultiplierAux);
native final function SetLifetimeEvent(AkEvent NewLifetimeEvent, bool SetAutoplayLifetimeTo);
native final function StartLifetimeEvent(optional bool SetAutoplayLifetimeTo = true);
native final function StopLifetimeEvent(optional bool SetAutoplayLifetimeTo = false);
native final function bool ShouldAutoPlayLifetimeEvent();
native final function bool IsPlayingLifetimeEvent();
native final function bool IsSourceActive();
native final function bool IsAutoUpdateSpatial();
native final function EnableEnvironmentalEffects(bool EnableEnvs);
native final function SetDialogueMeterEffect(bool EnableMeter, AkEnvironmentName MeterEffect);
native final function EnableOcclusion();
native final function DisableOcclusion();
native final function bool IsOcclusionEnabled();
native final function ApplyFalloffRadiusMultiplier(float FalloffMultiplier, bool MixInOut);
native final function EnableFalloffEnhancement(float EnhancementRadius, optional bool EaseIn = true);
native final function DisableFalloffEnhancement();
native final function bool IsFalloffEnhancementEnabled();
native final function float GetFalloffEnhancementMultiplier();
native final function float GetPeakAudibilityRadius();
native final function ResetPeakAudibilityRadius(optional float ResetValue);
native final function float GetSourceLODRadius();
native final function bool IsOwnerHidden();
native final function bool IsOwnerDead();
native final function bool IsOwnerSpeaking();
native final function bool IsOwnerSurveillance();
native final function bool DebugIsFailedSource();

defaultproperties
{
	AutoPlayLifetimeEvent=true
	AutoDestroySourceWhenHidden=true
	AutoDestroySourceWhenDead=true
	KillSoundsOnDestroy=true
	EnableEnvironments=true
	DefaultAkComponent=true
	LODDestroyTimer=-1.0
	// BM: retail defaults to AkEnvironmentName'ENV_Special.MTR_DialogueDefault' - package isn't shipped here.
	OcclusionUpdateTimeFast=0.2
	OcclusionUpdateTimeSlow=2.0
	OcclusionInterpolationTime=0.1
	OcclusionMultiplier=1.0
	OcclusionMultiplierAux=1.0
	WetDryMixVolume=1.0
	WetDryMixVolumeAux=-1.0
	FalloffRadiusMultiplier=1.0
	bTickInEditor=true
}
