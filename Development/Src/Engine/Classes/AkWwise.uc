// BM
class AkWwise extends Object
	abstract
	native;

enum EListenerID
{
	AK_LISTENER_PLAYER_1,
	AK_LISTENER_CAMERA_1,
	AK_LISTENER_COMPOSITE_1,
	AK_LISTENER_MAX
};

enum EGlobalAudioSourceID
{
	AK_INVALID_SOURCE_ID,
	AK_RESERVED_SOURCE_ID,
	AK_EDITOR_SOURCE_ID,
	AK_MUSIC_SOURCE_ID,
	AK_AMBIENCE_SOURCE_ID,
	AK_UI_SOURCE_ID,
	AK_HUD_SOURCE_ID,
	AK_KISMET_SOURCE_ID,
	AK_MOVIE_SOURCE_ID,
	AK_MATINEE_SOURCE_ID,
	AK_MAX
};

enum EAkGameSyncType
{
	AK_GS_GENERAL,
	AK_GS_SFX,
	AK_GS_MUSIC,
	AK_GS_DIALOGUE,
	AK_GS_FADER,
	AK_GS_LISTENER,
	AK_GS_LFO,
	AK_GS_MIX,
	AK_GS_MAX
};

enum EDialogueHelperType
{
	DialogueHelper_None,
	DialogueHelper_Player,
	DialogueHelper_BatmansRadio,
	DialogueHelper_BroadcastAnalyzer,
	DialogueHelper_Intercept,
	DialogueHelper_Surveillance,
	DialogueHelper_Tape,
	DialogueHelper_TV,
	DialogueHelper_Normal,
	DialogueHelper_TannoyLow,
	DialogueHelper_TannoyHigh,
	DialogueHelper_Helicopter,
	DialogueHelper_Emote,
	DialogueHelper_MAX
};

enum EAkIOStreamPriorities
{
	AKIO_PRIORITY_HIGH,
	AKIO_PRIORITY_ABOVE_NORMAL,
	AKIO_PRIORITY_NORMAL,
	AKIO_PRIORITY_BELOW_NORMAL,
	AKIO_PRIORITY_LOW,
	AKIO_PRIORITY_MAX
};

enum EAkPhysicsNotifyType
{
	AK_PHYS_NOTIFY_IMPACT,
	AK_PHYS_NOTIFY_COLLAPSE,
	AK_PHYS_NOTIFY_SLIDE,
	AK_PHYS_NOTIFY_ROLL,
	AK_PHYS_NOTIFY_FLAP,
	AK_PHYS_NOTIFY_PARTICLE,
	AK_PHYS_NOTIFY_MAX
};

struct native AkSoundHandle
{
	var int EventInstanceID;
	var int OriginalEventID;
	var int SourceID;
};

struct native AkSoundLoop
{
	var AkEvent			SoundEvent;
	var AkSoundHandle	SoundHandle;
};

struct native AkEnvironmentSettings
{
	var() AkEnvironmentName	EnvironmentName;
	var() float				WetMixAdjust;
	var() float				DryMixAdjust;

	structdefaultproperties
	{
		WetMixAdjust=1.0
		DryMixAdjust=1.0
	}
};

struct native AkEnvironmentInfo
{
	var AkEnvironmentSettings	EnvSettings;
	var float					WetMixLevel;
	var float					DryMixLevel;
	var float					WetMixLevelAux;
	var float					DryMixLevelAux;
	var int						ListenerEnvironmentCount;
	var int						RefCount;

	structdefaultproperties
	{
		WetMixLevel=1.0
		DryMixLevel=1.0
		WetMixLevelAux=1.0
		DryMixLevelAux=1.0
	}
};

struct native AkSourceSpatial
{
	var Vector TransformedPosition;
	var Vector TransformedOrientation;

	structdefaultproperties
	{
		TransformedOrientation=(X=0.0,Y=1.0,Z=0.0)
	}
};

struct native AkEnvelopeSettings
{
	var() float SustainValue;
	var() float ReleaseValue;
	var() float AttackDuration;
	var() float SustainDuration;
	var() float ReleaseDuration;

	structdefaultproperties
	{
		SustainValue=1.0
		ReleaseValue=0.0
		AttackDuration=1.0
		SustainDuration=1.0
		ReleaseDuration=1.0
	}
};

struct native AkPhysicsInfo
{
	var AkEvent	ImpactSound;
	var AkEvent	CollapseSound;
	var AkEvent	SlideSound;
	var AkEvent	RollSound;
	var AkEvent	FlapSound;
	var AkEvent	ParticleSound;
	var float	InstantaneousTimeout;
	var float	ContinuousTimeout;
};

delegate AkSoundCallback(int CallbackFlags, AkSoundHandle SoundHandle, int MarkerID);

delegate AkMusicCallback(int CallbackFlags, int MarkerID);


native static final function AkSoundHandle StartGlobalAudioEvent(AkEvent AudioEvent, EGlobalAudioSourceID GlobalSource, optional delegate<AkSoundCallback> SoundCallbackDelegate, optional int SoundCallbackFlags = 1);
native static final function StopGlobalAudioEvent(out AkSoundHandle SoundHandle, optional bool QuickStop = false);
native static final function SetGlobalAudioParameter(AkParameterName ParamName, float ParamValue);
native static final function SetGlobalStickyParameter(AkParameterName ParamName, float ParamValue);
native static final function SetGlobalStickyParameterEx(AkParameterName ParamName, float ParamValue, float NewReleaseValue, float NewReleaseTime, float NewSustainTime, float NewAttackTime, bool AllowPause);
native static final function SetGlobalAudioSwitch(AkSwitchName SwitchName, EGlobalAudioSourceID GlobalSource);
native static final function SetGlobalAudioState(AkStateName StateName);
native static final function StartCollisionAudioEvent(AkEvent CollisionEvent, Actor CollidingActor, Vector CollisionPosition, float CollisionVelocity, float CollisionStrength);
native static final function StartContinuousCollisionAudioEvent(out AkSoundLoop CollisionLoop, Actor CollidingActor, Vector CollisionPosition, float CollisionVelocity, float CollisionStrength);
native static final function bool IsSoundHandleValid(out AkSoundHandle SoundHandleToTest);
native static final function CancelAudioCallbacks(Object OwnerObject);

native static final function StartMusic(optional AkEvent CustomMusicEvent);
native static final function StopMusic();
native static final function SetMusicState(AkStateName StateName);
native static final function SetMixChapterState(string ChapterName);
native static final function SetMusicChapterState(string ChapterName);
native static final function SetMusicLevelState(string LevelName);
native static final function SetMusicGameplayState(string GameplayName);
native static final function SetMusicTrigger(AkTriggerName TriggerName);
native static final function SetMusicParameter(AkParameterName ParamName, float ParamValue, optional float InterpolationTime);
native static final function ResetMusicParameters(optional float InterpolationTime);
native static final function bool RegisterMusicCallback(int CallbackFlags, delegate<AkMusicCallback> MusicCallbackDelegate, optional bool UnregisterOnStop = true);
native static final function UnregisterMusicCallback(Object CallbackOwner);

native static final function AkSoundHandle StartCustomAudioEvent(Actor Parent, string EventName);
native static final function StopCustomAudioEvent(out AkSoundHandle SoundHandle);
native static final function SetCustomGlobalParameter(string ParamName, float ParamValue);
native static final function SetCustomSourceParameter(Actor Parent, string ParamName, float ParamValue);
native static final function SetCustomSourceSwitch(Actor Parent, string SwitchGroup, string SwitchName);
native static final function SetCustomGlobalState(string StateGroup, string StateName);

native static final function NotifySurveillanceDialogue(Actor Speaker, float SurvRange, optional int SurvConversation);
native static final function ResetSurveillanceDialogue();
native static final function bool IsSurveillanceDialogueActive();
native static final function EnableSurveillanceDialogue(bool AllowSurv);
native static final function bool IsSurveillanceDialogueEnabled();
native static final function bool IsSurveillanceUIVisible();
native static final function Actor GetSurveillanceFocus();

native static final function FullReset();

defaultproperties
{
}
