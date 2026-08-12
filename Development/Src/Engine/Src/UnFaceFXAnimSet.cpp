/*=============================================================================
	UnFaceFXAnimSet.cpp: Code for managing FaceFX AnimSets
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnFaceFXSupport.h"

#if WITH_FACEFX
using namespace OC3Ent;
using namespace Face;
#endif

IMPLEMENT_CLASS(UFaceFXAnimSet);

#if WITH_FACEFX
/** Map of all the FaceFXAssets to which a FaceFXAnimSet is mounted. */
TMultiMap<UFaceFXAnimSet*, UFaceFXAsset*> GFaceFXAnimSetToAssetMap;

/** Map of all the FaceFXAnimSets which are mounted to a particular FaceFXAsset. */
TMultiMap<UFaceFXAsset*, UFaceFXAnimSet*> GFaceFXAssetToAnimSetMap;
#endif

void UFaceFXAnimSet::CreateFxAnimSet( class UFaceFXAsset* FaceFXAsset )
{
	DefaultFaceFXAsset = FaceFXAsset;

#if WITH_FACEFX
	if( DefaultFaceFXAsset && !InternalFaceFXAnimSet )
	{
		FxName AnimSetName(TCHAR_TO_ANSI(*GetName()));
		FxAnimSet* AnimSet = new FxAnimSet();
		AnimSet->SetName(AnimSetName);
		FxAnimGroup ContainedAnimGroup;
		ContainedAnimGroup.SetName(AnimSetName);
		AnimSet->SetAnimGroup(ContainedAnimGroup);
		FxActor* OwningActor = DefaultFaceFXAsset->GetFxActor();
		if( OwningActor )
		{
			AnimSet->SetOwningActorName(OwningActor->GetName());
		}
		InternalFaceFXAnimSet = AnimSet;
	}
#endif
}

#if WITH_FACEFX
class FxAnimSet* UFaceFXAnimSet::GetFxAnimSet( void )
{
	if( !InternalFaceFXAnimSet )
		return NULL;

	FxAnimSet* AnimSet = (FxAnimSet*)InternalFaceFXAnimSet;
	return AnimSet;
}
#endif

void UFaceFXAnimSet::FixupReferencedSoundCues()
{
#if WITH_FACEFX
	NumLoadErrors = 0;
	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		const FxAnimGroup& AnimGroup = AnimSet->GetAnimGroup();
		FxSize NumAnims = AnimGroup.GetNumAnims();

		// If ReferencedDialogueCues is empty and there are Anims to add we need to build it first.
		if( 0 == ReferencedDialogueCues.Num() && NumAnims > 0)
		{
#if CONSOLE
			debugf(NAME_Warning,TEXT("%s has %i anims but no referenced dialogue cues."), *GetFullName(), NumAnims);
#else
			// Build the ReferencedDialogueCues array and set the indices in each
			// animation.
			ReferencedDialogueCues.Reserve(NumAnims);
			for( FxSize i = 0; i < NumAnims; ++i )
			{
				FxAnim* Anim = AnimSet->GetAnimPtr(i);
				if( Anim )
				{
					Anim->SetSoundCueIndex(FxInvalidIndex);
					FxString SoundCuePath = Anim->GetSoundCuePath();
					if( SoundCuePath.Length() > 0 )
					{
						URDialogueEvent* DialogueCue = LoadObject<URDialogueEvent>(NULL, ANSI_TO_TCHAR(SoundCuePath.GetData()), NULL, LOAD_NoWarn, NULL);
						if( DialogueCue )
						{
							if( DialogueCue->IsA(URDialogueEvent::StaticClass()) )
							{
								ReferencedDialogueCues.AddItem(DialogueCue);
								Anim->SetSoundCueIndex(ReferencedDialogueCues.Num()-1);
								Anim->SetSoundCuePointer(DialogueCue);
							}
						}
						else
						{
							NumLoadErrors++;
						}
					}
				}
			}

			if( ReferencedDialogueCues.Num() > 0 )
			{
				// Update the RawFaceFXAnimSetBytes (checking for cooking and byte ordering here is
				// probably a little overkill).
				FxArchive::FxArchiveByteOrder ByteOrder = FxArchive::ABO_LittleEndian;
				// If cooking, save the data in Big Endian format.
				if( GCookingTarget & UE3::PLATFORM_Console )
				{
#if __INTEL_BYTE_ORDER__
					ByteOrder = FxArchive::ABO_BigEndian;
#else
					ByteOrder = FxArchive::ABO_LittleEndian;
#endif
				}
				FxByte* AnimSetMemory = NULL;
				FxSize  NumAnimSetMemoryBytes = 0;
				if( !FxSaveAnimSetToMemory(*AnimSet, AnimSetMemory, NumAnimSetMemoryBytes, ByteOrder) )
				{
					warnf(TEXT("FaceFX: Failed to save animset for %s"), *GetPathName());
				}
				RawFaceFXAnimSetBytes.Empty();
				RawFaceFXAnimSetBytes.Add(NumAnimSetMemoryBytes);
				appMemcpy(RawFaceFXAnimSetBytes.GetData(), AnimSetMemory, NumAnimSetMemoryBytes);
				FxFree(AnimSetMemory, NumAnimSetMemoryBytes);

				// Mark the package as dirty.
				MarkPackageDirty();
			}
#endif
		}
		else
		{
			// ReferencedDialogueCues was valid so link up each animation.
			UBOOL bMadeCorrections = FALSE;
			for( FxSize i = 0; i < NumAnims; ++i )
			{
				FxAnim* Anim = AnimSet->GetAnimPtr(i);
				if( Anim )
				{
					FxSize SoundCueIndex = Anim->GetSoundCueIndex();
					const FxString& SoundCuePath = Anim->GetSoundCuePath();
					if( FxInvalidIndex != SoundCueIndex)
					{
						if( SoundCueIndex < static_cast<FxSize>(ReferencedDialogueCues.Num()) )
						{
							URDialogueEvent* DialogueCue = ReferencedDialogueCues(SoundCueIndex);
							if( DialogueCue && DialogueCue->IsA(URDialogueEvent::StaticClass()) )
							{
								Anim->SetSoundCuePointer(DialogueCue);
#if !CONSOLE
								if( SoundCuePath.Length() > 0 )
								{
									FString ExpectedPath(ANSI_TO_TCHAR(SoundCuePath.GetData()));
									FString ActualPath = DialogueCue->GetPathName();
									if( ExpectedPath != ActualPath )
									{
										bMadeCorrections = TRUE;
										Anim->SetSoundCuePath(FxString(TCHAR_TO_ANSI(*ActualPath)));
										debugf(NAME_Warning, TEXT("FaceFX: Corrected inconsistent sound cue linkage in FaceFXAnimSet %s for animation %s.%s.%s (expected path %s -> actual path %s)"),
											*GetFullName(),
											ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()),
											ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()),
											ANSI_TO_TCHAR(Anim->GetNameAsCstr()),
											*ExpectedPath,
											*ActualPath);
									}
								}
#endif
							}
						}
						else
						{
							Anim->SetSoundCueIndex(FxInvalidIndex);
							Anim->SetSoundCuePointer(NULL);
							GWarn->Logf( TEXT("Error finding SoundCue for FaceFX Anim: %s.%s.%s. Open and resave FaceFXAnimSet (%s)."),
								ANSI_TO_TCHAR(AnimSet->GetNameAsCstr()),
								ANSI_TO_TCHAR(AnimGroup.GetNameAsCstr()),
								ANSI_TO_TCHAR(Anim->GetNameAsCstr()),
								*GetFullName());
						}
					}
					else if(SoundCuePath.Length() > 0)
					{
						NumLoadErrors++;
					}
				}
			}
#if !CONSOLE
			if( bMadeCorrections )
			{
				// Update the RawFaceFXAnimSetBytes (checking for cooking and byte ordering here is
				// probably a little overkill).
				FxArchive::FxArchiveByteOrder ByteOrder = FxArchive::ABO_LittleEndian;
				// If cooking, save the data in Big Endian format.
				if( GCookingTarget & UE3::PLATFORM_Console )
				{
#if __INTEL_BYTE_ORDER__
					ByteOrder = FxArchive::ABO_BigEndian;
#else
					ByteOrder = FxArchive::ABO_LittleEndian;
#endif
				}
				FxByte* AnimSetMemory = NULL;
				FxSize  NumAnimSetMemoryBytes = 0;
				if( !FxSaveAnimSetToMemory(*AnimSet, AnimSetMemory, NumAnimSetMemoryBytes, ByteOrder) )
				{
					warnf(TEXT("FaceFX: Failed to save animset for %s"), *GetPathName());
				}
				RawFaceFXAnimSetBytes.Empty();
				RawFaceFXAnimSetBytes.Add(NumAnimSetMemoryBytes);
				appMemcpy(RawFaceFXAnimSetBytes.GetData(), AnimSetMemory, NumAnimSetMemoryBytes);
				FxFree(AnimSetMemory, NumAnimSetMemoryBytes);

				// Mark the package as dirty.
				MarkPackageDirty();
			}
#endif
		}
	}
#endif
}

FString UFaceFXAnimSet::GetDesc()
{
	INT NumAnims = 0;
	FString OriginalActorName;
#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		NumAnims = AnimSet->GetAnimGroup().GetNumAnims();
		OriginalActorName = ANSI_TO_TCHAR(AnimSet->GetOwningActorName().GetAsCstr());
	}
#endif
	return FString::Printf(TEXT("%d Anims, Created From: %s"), NumAnims, *OriginalActorName);
}

void UFaceFXAnimSet::PostLoad()
{
	Super::PostLoad();

#if BATMAN
	// BM: the FaceFX data is loaded here rather than in Serialize.
#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		delete AnimSet;
	}
	AnimSet = new FxAnimSet();
	InternalFaceFXAnimSet = AnimSet;

	if( !FxLoadAnimSetFromMemory(*AnimSet, static_cast<FxByte*>(RawFaceFXAnimSetBytes.GetData()), RawFaceFXAnimSetBytes.Num()) )
	{
		warnf(TEXT("FaceFX: Failed to load animset for %s"), *GetPathName());
	}
#endif

	// Flush raw bytes that are only needed in the editor and ucc.
	if( !GIsEditor && !GIsUCC )
	{
		RawFaceFXAnimSetBytes.Empty();
		RawFaceFXMiniSessionBytes.Empty();
	}
#endif

	if( ( GIsEditor == TRUE ) && ( GIsCooking == FALSE ) && ( GIsUCC == FALSE ) )
	{
		FixupReferencedSoundCues();
	}
}

void UFaceFXAnimSet::FinishDestroy()
{
	// Clean up FaceFX stuff when the AnimSet gets garbage collected.
#if WITH_FACEFX
	TArray<UFaceFXAsset*> OutAssets;
	GFaceFXAnimSetToAssetMap.MultiFind(this, OutAssets);

	// Unmount this FaceFXAnimSet from all FaceFXAssets it is actively mounted in.
	for(INT i=0; i<OutAssets.Num(); i++)
	{
		UFaceFXAsset* Asset = OutAssets(i);
		check(Asset);
		// This will update the maps to remove the pairs between this set and this asset
		Asset->UnmountFaceFXAnimSet(this);
	}

	FxAnimSet* AnimSet = GetFxAnimSet();
	if( AnimSet )
	{
		delete AnimSet;
		AnimSet = NULL;
		InternalFaceFXAnimSet = NULL;
	}
#endif

	Super::FinishDestroy();
}

void UFaceFXAnimSet::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
#endif

	// FaceFX data is embedded as raw bytes straight into packages.
	if( Ar.IsSaving() )
	{
		RawFaceFXAnimSetBytes.Empty();
		
#if WITH_FACEFX
		if( AnimSet )
		{
			FxArchive::FxArchiveByteOrder ByteOrder = FxArchive::ABO_LittleEndian;
			// If cooking, save the data in Big Endian format.
			if( GCookingTarget & UE3::PLATFORM_Console )
			{
#if __INTEL_BYTE_ORDER__
				ByteOrder = FxArchive::ABO_BigEndian;
#else
				ByteOrder = FxArchive::ABO_LittleEndian;
#endif
			}
			FxByte* AnimSetMemory = NULL;
			FxSize  NumAnimSetMemoryBytes = 0;
			if( !FxSaveAnimSetToMemory(*AnimSet, AnimSetMemory, NumAnimSetMemoryBytes, ByteOrder) )
			{
				warnf(TEXT("FaceFX: Failed to save animset for %s"), *GetPathName());
			}
			RawFaceFXAnimSetBytes.Add(NumAnimSetMemoryBytes);
			appMemcpy(RawFaceFXAnimSetBytes.GetData(), AnimSetMemory, NumAnimSetMemoryBytes);
			FxFree(AnimSetMemory, NumAnimSetMemoryBytes);
		}
#endif

		Ar << RawFaceFXAnimSetBytes;
		Ar << RawFaceFXMiniSessionBytes;
	}
	else if( Ar.IsLoading() )
	{
		RawFaceFXAnimSetBytes.Empty();
		RawFaceFXMiniSessionBytes.Empty();		

		Ar << RawFaceFXAnimSetBytes;
		Ar << RawFaceFXMiniSessionBytes;

		// BM: the FaceFX data is loaded in PostLoad instead.
#if !BATMAN
#if WITH_FACEFX
		if( AnimSet )
		{
			delete AnimSet;
		}
		AnimSet = new FxAnimSet();
		InternalFaceFXAnimSet = AnimSet;

#if LOG_FACEFX_PERF
		DWORD StartSerialization = appCycles();
#endif
		if( !FxLoadAnimSetFromMemory(*AnimSet, static_cast<FxByte*>(RawFaceFXAnimSetBytes.GetData()), RawFaceFXAnimSetBytes.Num()) )
		{
			warnf(TEXT("FaceFX: Failed to load animset for %s"), *GetPathName());
		}
#if LOG_FACEFX_PERF
		DWORD EndSerialization = appCycles();
#endif
#endif
#if LOG_FACEFX_PERF
		FxSize NumBytes = RawFaceFXAnimSetBytes.Num();
#endif

		// Flush raw bytes that are only needed in the editor and ucc.
		if( !GIsEditor && !GIsUCC )
		{
			RawFaceFXAnimSetBytes.Empty();
			RawFaceFXMiniSessionBytes.Empty();
		}

#if LOG_FACEFX_PERF
		debugf(TEXT("DevFaceFX_Perf: Loading animset %s : (Total Bytes: %d) Serialization: %f ms"),
			*GetPathName(),
			NumBytes,
			(EndSerialization-StartSerialization)*GSecondsPerCycle*1000.0f);
#endif
#endif
	}
	else if( Ar.IsCountingMemory() )
	{
		//TODO: Implement these without going through the save process
		//Ar << InternalFaceFXAnimSet;
		//Ar << RawFaceFXAnimSetBytes;
		//Ar << RawFaceFXMiniSessionBytes;
	}
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UFaceFXAnimSet::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

	// if we aren't keeping any non-stripped platforms, we can toss the data
	if (!(PlatformsToKeep & ~UE3::PLATFORM_Stripped))
	{
//		RawFaceFXAnimSetBytes.Empty();
		RawFaceFXMiniSessionBytes.Empty();
	}
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UFaceFXAnimSet::GetResourceSize()
{
#if WITH_FACEFX
	INT Size = 0;
	if (!GExclusiveResourceSizeMode)
	{	
		FArchiveCountMem CountBytesSize( this );
		CountBytesSize << RawFaceFXAnimSetBytes;
		CountBytesSize << ReferencedDialogueCues;	
		Size += CountBytesSize.GetNum();
	}

	if (!GIsEditor)
	{	
		// Add FxAnimSet size - recommended by OC3ENT
		// Originally used serialize, and that could cause memory fragmentation if we do this a lot
		// (i.e. during memory/perf captures )
		// This is an approximation of RawFaceFXAnimSetBytes, but works outside of the editor
		FxAnimSet* AnimSet = GetFxAnimSet();
		if(AnimSet)
		{
			const FxAnimGroup& Group = AnimSet->GetAnimGroup();
			for( UINT I=0; I<Group.GetNumAnims(); I++ )
			{
				const FxAnim Anim = Group.GetAnim(I);
				Size += sizeof(Anim);
				for ( UINT J=0; J<Anim.GetNumAnimCurves(); ++J )
				{
					const FxAnimCurve AnimCurve = Anim.GetAnimCurve(J);
					Size += sizeof(AnimCurve);
					if ( AnimCurve.GetNumKeys() > 0 )
					{
						Size += AnimCurve.GetNumKeys()*sizeof(FxAnimKey);
					}
				}
			}
		}
	}

	return Size;
#else
	return 0;
#endif
}

/** Get list of FaceFX animations in this AnimSet. Names are in the form GroupName.AnimName.*/
void UFaceFXAnimSet::GetSequenceNames(TArray<FString>& OutNames)
{
#if WITH_FACEFX
	FxAnimSet* AnimSet = GetFxAnimSet();
	if(AnimSet)
	{
		const FxAnimGroup& fGroup = AnimSet->GetAnimGroup();
		const FxChar* fGroupName = fGroup.GetNameAsCstr();

		INT NumAnims = static_cast<INT>(fGroup.GetNumAnims());
		for(INT j=0; j<NumAnims; j++)
		{
			const FxAnim& fAnim = fGroup.GetAnim(j);
			const FxChar* fAnimName = fAnim.GetNameAsCstr();

			FString SeqFullName = FString::Printf( TEXT("%s.%s"), ANSI_TO_TCHAR(fGroupName), ANSI_TO_TCHAR(fAnimName) );
			OutNames.AddItem(SeqFullName);
		}
	}
#endif
}
