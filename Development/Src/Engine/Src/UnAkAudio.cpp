/*=============================================================================
	UnAkAudio.cpp: Audiokinetic/Wwise object stubs.

	BM: These classes exist so that the game's Ak assets round-trip through the editor
	with the correct layout and serialization. There is no Wwise runtime here -
	every script native is a no-op; only URExternalHook::Serialize does real work.
=============================================================================*/

#include "EnginePrivate.h"

#if BATMAN

IMPLEMENT_CLASS(UAkHash);
IMPLEMENT_CLASS(UAkAssetBase);
IMPLEMENT_CLASS(UAkAsset);
IMPLEMENT_CLASS(UAkAssetPrep);
IMPLEMENT_CLASS(UAkEvent);
IMPLEMENT_CLASS(UAkAssetPack);
IMPLEMENT_CLASS(URExternalHook);
IMPLEMENT_CLASS(UAkBankExternalHook);
IMPLEMENT_CLASS(UAkBank);
IMPLEMENT_CLASS(UAkWwise);
IMPLEMENT_CLASS(UAkComponent);
IMPLEMENT_CLASS(UAkParameterName);
IMPLEMENT_CLASS(UAkStateName);
IMPLEMENT_CLASS(UAkStateGroupName);
IMPLEMENT_CLASS(UAkSwitchName);
IMPLEMENT_CLASS(UAkSwitchGroupName);
IMPLEMENT_CLASS(UAkTriggerName);
IMPLEMENT_CLASS(UAkStackName);
IMPLEMENT_CLASS(UAkEnvironmentName);
IMPLEMENT_CLASS(UAkPropertySheet);
IMPLEMENT_CLASS(AAkSoundActor);
IMPLEMENT_CLASS(UAkDrawBoundsComponent);
IMPLEMENT_CLASS(UAkDrawSoundBoxComponent);
IMPLEMENT_CLASS(UAkDrawSoundRadiusComponent);
IMPLEMENT_CLASS(UAkAudioAction);
IMPLEMENT_CLASS(UAkAudioActionAltitude);
IMPLEMENT_CLASS(UAkAudioActionAmbience);
IMPLEMENT_CLASS(UAkAudioActionEvent);
IMPLEMENT_CLASS(UAkAudioActionParameter);
IMPLEMENT_CLASS(UAkAudioActionState);
IMPLEMENT_CLASS(UAkAudioActionSwitch);
IMPLEMENT_CLASS(URDialogueEvent);

/*-----------------------------------------------------------------------------
	URDialogueEvent
-----------------------------------------------------------------------------*/

FLOAT URDialogueEvent::GetCueDuration()
{
	return Duration;
}

/*-----------------------------------------------------------------------------
	URExternalHook
-----------------------------------------------------------------------------*/

// AK's layouts, from GetPrivateStaticClassBody in the retail binary.
checkAtCompileTime( sizeof(FExternalHookDataEntry) == 116, FExternalHookDataEntry_layout_changed );
checkAtCompileTime( sizeof(URExternalHook) == 124, URExternalHook_layout_changed );
checkAtCompileTime( sizeof(UAkBankExternalHook) == 124, UAkBankExternalHook_layout_changed );
checkAtCompileTime( sizeof(UAkBank) == 164, UAkBank_layout_changed );

/**
 * BM: after the tagged properties AK writes one bulk-data payload per ExternalEntries
 * element - for an AkBank, one Wwise SoundBank ("BKHD") per cooked language.
 *
 * Retail keeps only the payload whose Tag matches the running language and discards
 * the rest as it reads them; the editor keeps them all so a re-save writes back what
 * it read. Nothing here interprets the payload - there is no Wwise runtime in this
 * tree - it is parsed for its size and offset only.
 */
/**
 * BM: script frees the array's memory without running our C++ destructors, so the payloads have
 * to let go of the linker here or it is left holding dangling FByteBulkData pointers. Retail's
 * destructor (sub_DBBFB0 -> sub_DB9640) frees ExternalEntries for the same reason.
 */
void URExternalHook::FinishDestroy()
{
	for( INT EntryIndex = 0; EntryIndex < ExternalEntries.Num(); EntryIndex++ )
	{
		ExternalEntries(EntryIndex).BulkStoredData.RemoveBulkData();
	}

	Super::FinishDestroy();
}

void URExternalHook::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if( Ar.IsLoading() )
	{
		for( INT EntryIndex = 0; EntryIndex < ExternalEntries.Num(); EntryIndex++ )
		{
			// Script hands us zeroed memory and FByteBulkData has a vtable, so construct the
			// payload before touching it - which is what retail's serializer does too.
			FExternalHookDataEntry& Entry = ExternalEntries(EntryIndex);
			::new((BYTE*)&Entry.BulkStoredData) FByteBulkData();
			Entry.BulkStoredData.Serialize( Ar, this );
		}
	}
	else if( Ar.IsSaving() )
	{
		for( INT EntryIndex = 0; EntryIndex < ExternalEntries.Num(); EntryIndex++ )
		{
			if( bCooked )
			{
				ExternalEntries(EntryIndex).BulkStoredData.Serialize( Ar, this );
			}
			else
			{
				// Uncooked, the payload lives outside the package and only the header is written.
				FByteBulkData EmptyData;
				EmptyData.Serialize( Ar, this );
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	UAkBank
-----------------------------------------------------------------------------*/

UBOOL UAkBank::LoadBank( UBOOL performDeferredLoad )
{
	return FALSE;
}

void UAkBank::UnloadBank( UBOOL performDeferredUnload )
{
}

void UAkBank::ReloadBank()
{
}

UBOOL UAkBank::IsBankLoadComplete()
{
	return FALSE;
}

UBOOL UAkBank::PreloadBankStreams( UBOOL preload, BYTE Priority )
{
	return FALSE;
}

UBOOL UAkBank::IsBankStreamPreloaded()
{
	return FALSE;
}

/*-----------------------------------------------------------------------------
	UAkHash
-----------------------------------------------------------------------------*/

INT UAkHash::GetHashValue()
{
	return HashValue;
}

FString UAkHash::GetHashString( const FString& NotFoundResult )
{
	return NotFoundResult;
}

FString UAkHash::GetOriginalNameString()
{
	return GetName();
}

void UAkHash::DetermineHashValue()
{
}

FString UAkHash::AkHashToString( INT HashValueToLookup, const FString& ResultIfNotFound )
{
	return ResultIfNotFound;
}

INT UAkHash::StringToAkHash( const FString& StringToHash )
{
	return 0;
}

/*-----------------------------------------------------------------------------
	UAkAssetPack
-----------------------------------------------------------------------------*/

void UAkAssetPack::LoadAssetPack()
{
}

void UAkAssetPack::UnloadAssetPack()
{
}

/*-----------------------------------------------------------------------------
	AAkSoundActor
-----------------------------------------------------------------------------*/

UAkComponent* AAkSoundActor::GetAkComponent( UBOOL AllowCreate )
{
	return ActorAudioComponent;
}

/*-----------------------------------------------------------------------------
	UAkComponent
-----------------------------------------------------------------------------*/

void UAkComponent::CreateAudioSource()
{
}

void UAkComponent::DestroyAudioSource()
{
}

UBOOL UAkComponent::HasAudioSource()
{
	return FALSE;
}

INT UAkComponent::GetAudioSourceID( UBOOL AllowCreate )
{
	return 0;
}

INT UAkComponent::GetAuxAudioSourceID( UBOOL AllowCreate )
{
	return 0;
}

UBOOL UAkComponent::CheckListenerProximity( UBOOL UpdateSpatial )
{
	return FALSE;
}

FLOAT UAkComponent::GetDistanceToListener( BYTE listenerID )
{
	return 0.f;
}

FAkSoundHandle UAkComponent::StartAudioEvent( class UAkEvent* AudioEvent, FScriptDelegate SoundCallbackDelegate, INT SoundCallbackFlags )
{
	return FAkSoundHandle(EC_EventParm);
}

FAkSoundHandle UAkComponent::StartAuxAudioEvent( class UAkEvent* AudioEvent, FVector Position, FScriptDelegate SoundCallbackDelegate, INT SoundCallbackFlags )
{
	return FAkSoundHandle(EC_EventParm);
}

void UAkComponent::StopAudioEvent( FAkSoundHandle& SoundHandle, UBOOL QuickStop )
{
}

UBOOL UAkComponent::IsSoundHandleValid( FAkSoundHandle& SoundHandleToTest )
{
	return FALSE;
}

void UAkComponent::KillSounds( UBOOL DestroySources )
{
}

void UAkComponent::SetSourceParameter( class UAkParameterName* ParamName, FLOAT ParamValue )
{
}

FLOAT UAkComponent::GetSourceParameter( class UAkParameterName* ParamName )
{
	return 0.f;
}

void UAkComponent::SetSourceStickyAudioEvent( class UAkEvent* AudioEvent )
{
}

void UAkComponent::SetSourceStickyAudioEventEx( class UAkEvent* AudioEvent, FLOAT NewAttackTime, FLOAT NewSustainTime, FLOAT NewReleaseTime, class UAkParameterName* ParamName, FLOAT newSustainValue, FLOAT NewReleaseValue )
{
}

void UAkComponent::SetSourceStickyParameter( class UAkParameterName* ParamName, FLOAT ParamValue )
{
}

void UAkComponent::SetSourceStickyParameterEx( class UAkParameterName* ParamName, FLOAT ParamValue, FLOAT NewReleaseValue, FLOAT NewReleaseTime, FLOAT NewSustainTime, FLOAT NewAttackTime, UBOOL AllowPause )
{
}

void UAkComponent::SetSourceSwitch( class UAkSwitchName* SwitchName )
{
}

void UAkComponent::SetSurfaceSwitch( class UAkSwitchName* SwitchName, class UAkSwitchName* FallbackSwitchName, const FString& CharacterName )
{
}

void UAkComponent::SetSourceSpatial( FVector Position, FRotator Orientation, BYTE NewUpdateType )
{
}

void UAkComponent::SetSourceSpatialMulti( const TArray<FAkSourceSpatial>& MultiPositions, UBOOL Additive, FVector PrimaryPosition, FRotator PrimaryOrientation, BYTE NewUpdateType )
{
}

void UAkComponent::SetSourceSpatialRay( FVector RayOrigin, FRotator RayOrientation, FRotator Orientation, BYTE NewUpdateType )
{
}

void UAkComponent::SetSourceSpatialBeam( FVector BeamStartPoint, FVector BeamEndPoint, FRotator Orientation, BYTE NewUpdateType )
{
}

void UAkComponent::RegisterEnvironments( const TArray<FAkEnvironmentSettings>& Envs, UBOOL SetListenerEnvironment )
{
}

void UAkComponent::UnregisterEnvironments( const TArray<FAkEnvironmentSettings>& Envs, UBOOL ClearListenerEnvironment )
{
}

void UAkComponent::RegisterOcclusionMultipliers( FLOAT Multiplier, FLOAT MultiplierAux )
{
}

void UAkComponent::UnregisterOcclusionMultipliers( FLOAT Multiplier, FLOAT MultiplierAux )
{
}

void UAkComponent::SetLifetimeEvent( class UAkEvent* NewLifetimeEvent, UBOOL SetAutoplayLifetimeTo )
{
	LifetimeEvent = NewLifetimeEvent;
	AutoPlayLifetimeEvent = SetAutoplayLifetimeTo;
}

void UAkComponent::StartLifetimeEvent( UBOOL SetAutoplayLifetimeTo )
{
}

void UAkComponent::StopLifetimeEvent( UBOOL SetAutoplayLifetimeTo )
{
}

UBOOL UAkComponent::ShouldAutoPlayLifetimeEvent()
{
	return FALSE;
}

UBOOL UAkComponent::IsPlayingLifetimeEvent()
{
	return FALSE;
}

UBOOL UAkComponent::IsSourceActive()
{
	return FALSE;
}

UBOOL UAkComponent::IsAutoUpdateSpatial()
{
	return FALSE;
}

void UAkComponent::EnableEnvironmentalEffects( UBOOL EnableEnvs )
{
}

void UAkComponent::SetDialogueMeterEffect( UBOOL EnableMeter, class UAkEnvironmentName* MeterEffect )
{
}

void UAkComponent::EnableOcclusion()
{
}

void UAkComponent::DisableOcclusion()
{
}

UBOOL UAkComponent::IsOcclusionEnabled()
{
	return FALSE;
}

void UAkComponent::ApplyFalloffRadiusMultiplier( FLOAT FalloffMultiplier, UBOOL MixInOut )
{
}

void UAkComponent::EnableFalloffEnhancement( FLOAT EnhancementRadius, UBOOL EaseIn )
{
}

void UAkComponent::DisableFalloffEnhancement()
{
}

UBOOL UAkComponent::IsFalloffEnhancementEnabled()
{
	return FALSE;
}

FLOAT UAkComponent::GetFalloffEnhancementMultiplier()
{
	return 1.f;
}

FLOAT UAkComponent::GetPeakAudibilityRadius()
{
	return 0.f;
}

void UAkComponent::ResetPeakAudibilityRadius( FLOAT ResetValue )
{
}

FLOAT UAkComponent::GetSourceLODRadius()
{
	return 0.f;
}

UBOOL UAkComponent::IsOwnerHidden()
{
	return FALSE;
}

UBOOL UAkComponent::IsOwnerDead()
{
	return FALSE;
}

UBOOL UAkComponent::IsOwnerSpeaking()
{
	return FALSE;
}

UBOOL UAkComponent::IsOwnerSurveillance()
{
	return FALSE;
}

UBOOL UAkComponent::DebugIsFailedSource()
{
	return FALSE;
}

/*-----------------------------------------------------------------------------
	UAkWwise
-----------------------------------------------------------------------------*/

FAkSoundHandle UAkWwise::StartGlobalAudioEvent( class UAkEvent* AudioEvent, BYTE GlobalSource, FScriptDelegate SoundCallbackDelegate, INT SoundCallbackFlags )
{
	return FAkSoundHandle(EC_EventParm);
}

void UAkWwise::StopGlobalAudioEvent( FAkSoundHandle& SoundHandle, UBOOL QuickStop )
{
}

void UAkWwise::SetGlobalAudioParameter( class UAkParameterName* ParamName, FLOAT ParamValue )
{
}

void UAkWwise::SetGlobalStickyParameter( class UAkParameterName* ParamName, FLOAT ParamValue )
{
}

void UAkWwise::SetGlobalStickyParameterEx( class UAkParameterName* ParamName, FLOAT ParamValue, FLOAT NewReleaseValue, FLOAT NewReleaseTime, FLOAT NewSustainTime, FLOAT NewAttackTime, UBOOL AllowPause )
{
}

void UAkWwise::SetGlobalAudioSwitch( class UAkSwitchName* SwitchName, BYTE GlobalSource )
{
}

void UAkWwise::SetGlobalAudioState( class UAkStateName* StateName )
{
}

void UAkWwise::StartCollisionAudioEvent( class UAkEvent* CollisionEvent, class AActor* CollidingActor, FVector CollisionPosition, FLOAT CollisionVelocity, FLOAT CollisionStrength )
{
}

void UAkWwise::StartContinuousCollisionAudioEvent( FAkSoundLoop& CollisionLoop, class AActor* CollidingActor, FVector CollisionPosition, FLOAT CollisionVelocity, FLOAT CollisionStrength )
{
}

UBOOL UAkWwise::IsSoundHandleValid( FAkSoundHandle& SoundHandleToTest )
{
	return FALSE;
}

void UAkWwise::CancelAudioCallbacks( class UObject* OwnerObject )
{
}

void UAkWwise::StartMusic( class UAkEvent* CustomMusicEvent )
{
}

void UAkWwise::StopMusic()
{
}

void UAkWwise::SetMusicState( class UAkStateName* StateName )
{
}

void UAkWwise::SetMixChapterState( const FString& ChapterName )
{
}

void UAkWwise::SetMusicChapterState( const FString& ChapterName )
{
}

void UAkWwise::SetMusicLevelState( const FString& LevelName )
{
}

void UAkWwise::SetMusicGameplayState( const FString& GameplayName )
{
}

void UAkWwise::SetMusicTrigger( class UAkTriggerName* TriggerName )
{
}

void UAkWwise::SetMusicParameter( class UAkParameterName* ParamName, FLOAT ParamValue, FLOAT InterpolationTime )
{
}

void UAkWwise::ResetMusicParameters( FLOAT InterpolationTime )
{
}

UBOOL UAkWwise::RegisterMusicCallback( INT CallbackFlags, FScriptDelegate MusicCallbackDelegate, UBOOL UnregisterOnStop )
{
	return FALSE;
}

void UAkWwise::UnregisterMusicCallback( class UObject* CallbackOwner )
{
}

FAkSoundHandle UAkWwise::StartCustomAudioEvent( class AActor* Parent, const FString& EventName )
{
	return FAkSoundHandle(EC_EventParm);
}

void UAkWwise::StopCustomAudioEvent( FAkSoundHandle& SoundHandle )
{
}

void UAkWwise::SetCustomGlobalParameter( const FString& ParamName, FLOAT ParamValue )
{
}

void UAkWwise::SetCustomSourceParameter( class AActor* Parent, const FString& ParamName, FLOAT ParamValue )
{
}

void UAkWwise::SetCustomSourceSwitch( class AActor* Parent, const FString& SwitchGroup, const FString& SwitchName )
{
}

void UAkWwise::SetCustomGlobalState( const FString& StateGroup, const FString& StateName )
{
}

void UAkWwise::NotifySurveillanceDialogue( class AActor* Speaker, FLOAT SurvRange, INT SurvConversation )
{
}

void UAkWwise::ResetSurveillanceDialogue()
{
}

UBOOL UAkWwise::IsSurveillanceDialogueActive()
{
	return FALSE;
}

void UAkWwise::EnableSurveillanceDialogue( UBOOL AllowSurv )
{
}

UBOOL UAkWwise::IsSurveillanceDialogueEnabled()
{
	return FALSE;
}

UBOOL UAkWwise::IsSurveillanceUIVisible()
{
	return FALSE;
}

AActor* UAkWwise::GetSurveillanceFocus()
{
	return NULL;
}

void UAkWwise::FullReset()
{
}

#endif // BATMAN
