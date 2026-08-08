/*=============================================================================
	UnLevel.cpp: Level-related functions
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "EngineSequenceClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineDecalClasses.h"
#include "EngineProcBuildingClasses.h"
#include "EngineAnimClasses.h"
#if BATMAN
#include "EngineLightClasses.h"
#endif
#include "EngineMeshClasses.h"
#include "UnOctree.h"
#include "LevelUtils.h"
#include "UnTerrain.h"
#include "ScenePrivate.h"
#include "PrecomputedLightVolume.h"
#include "UnNovodexSupport.h"
#include "NvApexManager.h"
#include "NvApexCommands.h"

#if WITH_APEX
#include "UnNovodexSupport.h"
#include <NxApexSDK.h>
#include <NxApexSDKCachedData.h>
#endif

IMPLEMENT_CLASS(ULineBatchComponent);

/*-----------------------------------------------------------------------------
	ULevelBase implementation.
-----------------------------------------------------------------------------*/

ULevelBase::ULevelBase( const FURL& InURL )
: Actors( this )
, URL( InURL )

{}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void ULevelBase::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevelBase, Actors ) );
}

void ULevelBase::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Actors;
	Ar << URL;
}
IMPLEMENT_CLASS(ULevelBase);


/*-----------------------------------------------------------------------------
	ULevel implementation.
-----------------------------------------------------------------------------*/

/** Whether we have a pending call to BuildStreamingData(). */
UBOOL ULevel::bStreamingDataDirty = FALSE;

/** Timestamp (in appSeconds) when the next call to BuildStreamingData() should be made, if bDirtyStreamingData is TRUE. */
DOUBLE ULevel::BuildStreamingDataTimer = 0.0;

//@deprecated with VER_SPLIT_SOUND_FROM_TEXTURE_STREAMING
struct FStreamableResourceInstanceDeprecated
{
	FSphere BoundingSphere;
	FLOAT TexelFactor;
	friend FArchive& operator<<( FArchive& Ar, FStreamableResourceInstanceDeprecated& ResourceInstance )
	{
		Ar << ResourceInstance.BoundingSphere;
		Ar << ResourceInstance.TexelFactor;
		return Ar;
	}
};
//@deprecated with VER_SPLIT_SOUND_FROM_TEXTURE_STREAMING
struct FStreamableResourceInfoDeprecated
{
	UObject* Resource;
	TArray<FStreamableResourceInstanceDeprecated> ResourceInstances;
	friend FArchive& operator<<( FArchive& Ar, FStreamableResourceInfoDeprecated& ResourceInfo )
	{
		Ar << ResourceInfo.Resource;
		Ar << ResourceInfo.ResourceInstances;
		return Ar;
	}
};
//@deprecated with VER_RENDERING_REFACTOR
struct FStreamableSoundInstanceDeprecated
{
	FSphere BoundingSphere;
	friend FArchive& operator<<( FArchive& Ar, FStreamableSoundInstanceDeprecated& SoundInstance )
	{
		Ar << SoundInstance.BoundingSphere;
		return Ar;
	}
};
//@deprecated with VER_RENDERING_REFACTOR
struct FStreamableSoundInfoDeprecated
{
	USoundNodeWave*	SoundNodeWave;
	TArray<FStreamableSoundInstanceDeprecated> SoundInstances;
	friend FArchive& operator<<( FArchive& Ar, FStreamableSoundInfoDeprecated& SoundInfo )
	{
		Ar << SoundInfo.SoundNodeWave;
		Ar << SoundInfo.SoundInstances;
		return Ar;
	}
};
//@deprecated with VER_RENDERING_REFACTOR
struct FStreamableTextureInfoDeprecated
{
	UTexture*							Texture;
	TArray<FStreamableTextureInstance>	TextureInstances;
	friend FArchive& operator<<( FArchive& Ar, FStreamableTextureInfoDeprecated& TextureInfo )
	{
		Ar << TextureInfo.Texture;
		Ar << TextureInfo.TextureInstances;
		return Ar;
	}
};

INT FPrecomputedVisibilityHandler::NextId = 0;

/** Updates visibility stats. */
void FPrecomputedVisibilityHandler::UpdateVisibilityStats(UBOOL bAllocating) const
{
	if (bAllocating)
	{
		INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (INT BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).Cells.GetAllocatedSize());
			INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.GetAllocatedSize());
			for (INT ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.Num(); ChunkIndex++)
			{
				INC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks(ChunkIndex).Data.GetAllocatedSize());
			}
		}
	}
	else
	{
		DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets.GetAllocatedSize());
		for (INT BucketIndex = 0; BucketIndex < PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
		{
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).Cells.GetAllocatedSize());
			DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.GetAllocatedSize());
			for (INT ChunkIndex = 0; ChunkIndex < PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks.Num(); ChunkIndex++)
			{
				DEC_DWORD_STAT_BY(STAT_PrecomputedVisibilityMemory, PrecomputedVisibilityCellBuckets(BucketIndex).CellDataChunks(ChunkIndex).Data.GetAllocatedSize());
			}
		}
	}
}

/** Sets this visibility handler to be actively used by the rendering scene. */
void FPrecomputedVisibilityHandler::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && PrecomputedVisibilityCellBuckets.Num() > 0)
	{
		Scene->SetPrecomputedVisibility(this);
	}
}

/** Invalidates the level's precomputed visibility and frees any memory used by the handler. */
void FPrecomputedVisibilityHandler::Invalidate(FSceneInterface* Scene)
{
	Scene->SetPrecomputedVisibility(NULL);
	// Block until the renderer no longer references this FPrecomputedVisibilityHandler so we can delete its data
	FlushRenderingCommands();
	UpdateVisibilityStats(FALSE);
	PrecomputedVisibilityCellBucketOriginXY = FVector2D(0,0);
	PrecomputedVisibilityCellSizeXY = 0;
	PrecomputedVisibilityCellSizeZ = 0;
	PrecomputedVisibilityCellBucketSizeXY = 0;
	PrecomputedVisibilityNumCellBuckets = 0;
	PrecomputedVisibilityCellBuckets.Empty();
	// Bump the Id so FSceneViewState will know to discard its cached visibility data
	Id = NextId;
	NextId++;
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVisibilityHandler& D )
{
	Ar << D.PrecomputedVisibilityCellBucketOriginXY;
	Ar << D.PrecomputedVisibilityCellSizeXY;
	Ar << D.PrecomputedVisibilityCellSizeZ;
	Ar << D.PrecomputedVisibilityCellBucketSizeXY;
	Ar << D.PrecomputedVisibilityNumCellBuckets;
	Ar << D.PrecomputedVisibilityCellBuckets;
	if (Ar.IsLoading())
	{
		D.UpdateVisibilityStats(TRUE);
	}
	return Ar;
}


/** Sets this volume distance field to be actively used by the rendering scene. */
void FPrecomputedVolumeDistanceField::UpdateScene(FSceneInterface* Scene) const
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(this);
	}
}

/** Invalidates the level's volume distance field and frees any memory used by it. */
void FPrecomputedVolumeDistanceField::Invalidate(FSceneInterface* Scene)
{
	if (Scene && Data.Num() > 0)
	{
		Scene->SetPrecomputedVolumeDistanceField(NULL);
		// Block until the renderer no longer references this FPrecomputedVolumeDistanceField so we can delete its data
		FlushRenderingCommands();
		Data.Empty();
	}
}

FArchive& operator<<( FArchive& Ar, FPrecomputedVolumeDistanceField& D )
{
	Ar << D.VolumeMaxDistance;
	Ar << D.VolumeBox;
	Ar << D.VolumeSizeX;
	Ar << D.VolumeSizeY;
	Ar << D.VolumeSizeZ;
	Ar << D.Data;

	return Ar;
}

IMPLEMENT_CLASS(ULevel);

ULevel::ULevel( const FURL& InURL )
:	ULevelBase( InURL )
,	PrecomputedLightVolume(NULL)
{
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void ULevel::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, Model ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, ModelComponents ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, GameSequences ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, NavListStart ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, NavListEnd ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, CoverListStart ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, CoverListEnd ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, PylonListStart ) );
	TheClass->EmitObjectReference( STRUCT_OFFSET( ULevel, PylonListEnd ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, CrossLevelActors ) );
	TheClass->EmitObjectArrayReference( STRUCT_OFFSET( ULevel, CoverLinkRefs ) );

	new(TheClass,TEXT("LightmapTotalSize"),RF_Public) UFloatProperty(CPP_PROPERTY(LightmapTotalSize),TEXT(""),CPF_EditConst|CPF_Const);
	new(TheClass,TEXT("ShadowmapTotalSize"),RF_Public) UFloatProperty(CPP_PROPERTY(ShadowmapTotalSize),TEXT(""),CPF_EditConst|CPF_Const);
}

/**
 * Callback used to allow object register its direct object references that are not already covered by
 * the token stream.
 *
 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
 */
void ULevel::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects( ObjectArray );
	for( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(TextureToInstancesMap); It; ++It )
	{
		UTexture2D* Texture2D = It.Key();
		AddReferencedObject( ObjectArray, Texture2D );
	}
	for( TMap<UPrimitiveComponent*,TArray<FDynamicTextureInstance> >::TIterator It(DynamicTextureInstances); It; ++It )
	{
		UPrimitiveComponent* Primitive = It.Key();
		TArray<FDynamicTextureInstance>& TextureInstances = It.Value();

		AddReferencedObject( ObjectArray, Primitive );
		for ( INT InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
		{
			FDynamicTextureInstance& Instance = TextureInstances( InstanceIndex );
			AddReferencedObject( ObjectArray, Instance.Texture );
		}
	}
 	for( INT CovIdx = 0; CovIdx < CoverLinkRefs.Num(); CovIdx++ )
 	{
		ACoverLink* Link = CoverLinkRefs(CovIdx);
		if( Link != NULL )
		{
			AddReferencedObject( ObjectArray, Link );
		} 
 	}
}

void ULevel::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	Ar << Model;

	Ar << ModelComponents;

	Ar << GameSequences;

#if BATMAN
	if (Ar.IsBmCooked(TRUE))
	{
		Ar << BoundingSpheres;
		FStreamableTextureInstance::SerializationBoundingSpheres = &BoundingSpheres;
	}
#endif

	if( !Ar.IsTransacting() )
	{
		Ar << TextureToInstancesMap;

#if BATMAN
		FStreamableTextureInstance::SerializationBoundingSpheres = NULL;
#endif

		if ( Ar.Ver() >= VER_DYNAMICTEXTUREINSTANCES )
		{
			Ar << DynamicTextureInstances;
		}

		if(Ar.Ver() >= VER_APEX_DESTRUCTION)
		{
#if WITH_APEX
			if(Ar.IsLoading())
			{
				DWORD Size;
				Ar << Size;
				if( Size > 16 )
				{
					InitializeApex();
					TArray<BYTE> Buffer;
					Buffer.Add( Size );
					Ar.Serialize( Buffer.GetData(), Size );
					physx::apex::NxApexSDKCachedData& nCachedData = GApexManager->GetApexSDK()->getCachedData();
					physx::PxFileBuf* nStream = GApexManager->GetApexSDK()->createMemoryReadStream( Buffer.GetData(), Size );
					if( nStream != NULL )
					{
						nCachedData.deserialize( *nStream );
						GApexManager->GetApexSDK()->releaseMemoryReadStream( *nStream );
					}
				}
				else
				{
					for (DWORD i=0; i<Size; i++)
					{
						char c;
						Ar << c;
					}
				}
			}
			else if ( Ar.IsSaving() )
			{
				physx::PxU32 Len = 0;
				void* Data = NULL;
				physx::PxFileBuf* nStream = NULL;
				if( GApexManager )
				{
					physx::apex::NxApexSDKCachedData& nCachedData = GApexManager->GetApexSDK()->getCachedData();
					nStream = GApexManager->GetApexSDK()->createMemoryWriteStream();
					if( nStream != NULL )
					{
						nCachedData.serialize( *nStream );
						Data = (void*)GApexManager->GetApexSDK()->getMemoryWriteBuffer( *nStream, Len );
					}
				}
				Ar << Len;
				if( Len != 0 )
				{
					Ar.Serialize( Data, Len );
				}
				if( nStream != NULL )
				{
					GApexManager->GetApexSDK()->releaseMemoryWriteStream( *nStream );
				}
			}
#else
			if (Ar.IsLoading())
			{
				DWORD Size;
				Ar << Size;
				Ar.Seek(Ar.Tell() + Size);
			}
			else if (Ar.IsSaving())
			{
				DWORD Len = 0;
				Ar << Len;
			}
#endif // if WITH_APEX
		}

		CachedPhysBSPData.BulkSerialize(Ar);
    
	    Ar << CachedPhysSMDataMap;
	    Ar << CachedPhysSMDataStore;
	    Ar << CachedPhysPerTriSMDataMap;
	    Ar << CachedPhysPerTriSMDataStore;
        Ar << CachedPhysBSPDataVersion;
	    Ar << CachedPhysSMDataVersion;
		Ar << ForceStreamTextures;

		if(Ar.Ver() >= VER_CONVEX_BSP)
		{
			Ar << CachedPhysConvexBSPData;
			Ar << CachedPhysConvexBSPVersion;
		}
	}

	// Mark archive and package as containing a map if we're serializing to disk.
	if( !HasAnyFlags( RF_ClassDefaultObject ) && Ar.IsPersistent() )
	{
		Ar.ThisContainsMap();
		GetOutermost()->ThisContainsMap();
	}

	// serialize the nav list
	Ar << NavListStart;
	Ar << NavListEnd;
	// and cover
	Ar << CoverListStart;
	Ar << CoverListEnd;
	// and pylons
	if(Ar.Ver() >= VER_PYLONLIST_IN_ULEVEL)
	{
		Ar << PylonListStart;
		Ar << PylonListEnd;
	}

	if( Ar.Ver() >= VER_COVERGUIDREFS_IN_ULEVEL )
	{
		Ar << CrossLevelCoverGuidRefs;
		Ar << CoverLinkRefs;
		Ar << CoverIndexPairs;
	}

	// serialize the list of cross level actors
	Ar << CrossLevelActors;
	if (Ar.Ver() >= VER_GI_CHARACTER_LIGHTING)
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			FPrecomputedLightVolume DummyVolume;
			Ar << DummyVolume;
		}
		else
		{
			if (!PrecomputedLightVolume)
			{
				PrecomputedLightVolume = new FPrecomputedLightVolume();
			}
			Ar << *PrecomputedLightVolume;
		}
	}

#if BATMAN
	if (Ar.IsBmCooked(TRUE))
	{
		Ar << NodeEdgeCollection;
		Ar << HorizontalEdges;
		Ar << ActorHorizontalEdges;
		Ar << bEdgesValid;
	}
#endif

	if (Ar.Ver() >= VER_NONUNIFORM_PRECOMPUTED_VISIBILITY)
	{
		Ar << PrecomputedVisibilityHandler;
	}
	else if (Ar.Ver() >= VER_PRECOMPUTED_VISIBILITY)
	{
		FBox LegacyPrecomputedVisibilityVolume(0);
		FLOAT LegacyPrecomputedVisibilityCellSize = 0;
		TArray<TArray<BYTE> > LegacyPrecomputedVisibilityData;
		Ar << LegacyPrecomputedVisibilityVolume;
		Ar << LegacyPrecomputedVisibilityCellSize;
		Ar << LegacyPrecomputedVisibilityData;
	}

	if (Ar.Ver() >= VER_IMAGE_REFLECTION_SHADOWING)
	{
		Ar << PrecomputedVolumeDistanceField;
	}
}


/**
 * Sorts the actor list by net relevancy and static behaviour. First all not net relevant static
 * actors, then all net relevant static actors and then the rest. This is done to allow the dynamic
 * and net relevant actor iterators to skip large amounts of actors.
 */
void ULevel::SortActorList()
{
	TickableActors.Reset();
	PendingUntickableActors.Reset();

	INT StartIndex = 0;
	TArray<AActor*> NewActors;
	NewActors.Reserve(Actors.Num());

	// The world info and default brush have fixed actor indices.
	NewActors.AddItem(Actors(StartIndex++));
	NewActors.AddItem(Actors(StartIndex++));

	// Static not net relevant actors.
	for (INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);
		if (Actor != NULL && !Actor->bDeleteMe && Actor->IsStatic() && Actor->RemoteRole == ROLE_None)
		{
			NewActors.AddItem(Actor);
		}
	}
	iFirstNetRelevantActor = NewActors.Num();

	// Static net relevant actors.
	for (INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);		
		if (Actor != NULL && !Actor->bDeleteMe && Actor->IsStatic() && Actor->RemoteRole > ROLE_None)
		{
			NewActors.AddItem(Actor);
		}
	}
	iFirstDynamicActor = NewActors.Num();

	// Remaining (dynamic, potentially net relevant actors)
	for (INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++)
	{
		AActor* Actor = Actors(ActorIndex);			
		if (Actor != NULL && !Actor->bDeleteMe && !Actor->IsStatic())
		{
			NewActors.AddItem(Actor);
			if (Actor->WantsTick())
			{
				TickableActors.AddItem(Actor);
			}
		}
	}

	// Replace with sorted list.
	Actors = NewActors;

	// Don't use sorted optimization outside of gameplay so we can safely shuffle around actors e.g. in the Editor
	// without there being a chance to break code using dynamic/ net relevant actor iterators.
	if (!GIsGame)
	{
		iFirstNetRelevantActor = 0;
		iFirstDynamicActor = 0;
	}
}

/**
 * Recreates the array of tickable actors starting from the given actor index. 
 *
 * @param	StartIndex	The index to start in the level's master Actor list. This 
 *						MUST be zero or greater. Otherwise, the function will crash. 
 *						Default StartIndex is zero.
 */
void ULevel::RebuildTickableActors( INT StartIndex )
{
	// The index must be at least zero or greater. If the index is equal to the 
	// number of elements or greater, the tickable actors array will be empty. 
	// This is a valid case when building the world for the first time.
	check( StartIndex >= 0 );

	TickableActors.Reset();

	for( INT ActorIndex = StartIndex; ActorIndex < Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);			

		// Tickable actors should not be marked to delete nor should they be static
		if( Actor != NULL && !Actor->bDeleteMe && !Actor->IsStatic() )
		{
			if( Actor->WantsTick() )
			{
				TickableActors.AddItem(Actor);
			}
		}
	}
}

/**
 * Makes sure that all light components have valid GUIDs associated.
 */
void ULevel::ValidateLightGUIDs()
{
	for( TObjectIterator<ULightComponent> It; It; ++It )
	{
		ULightComponent*	LightComponent	= *It;
		UBOOL				IsInLevel		= LightComponent->IsIn( this );

		if( IsInLevel )
		{
			LightComponent->ValidateLightGUIDs();
		}
	}
}

/** 
 * Associate teleporters with the portal volume they are in
 */
void ULevel::AssociatePortals( void )
{
	check( GWorld );

	for( TObjectIterator<APortalTeleporter> It; It; ++It )
	{
		APortalTeleporter* Teleporter = static_cast<APortalTeleporter*>( *It );
		APortalVolume* Volume = GWorld->GetWorldInfo()->GetPortalVolume( Teleporter->Location );

		if( Volume )
		{
			Volume->Portals.AddUniqueItem( Teleporter );
		}
	}
}

/**
 * Presave function, gets called once before the level gets serialized (multiple times) for saving.
 * Used to rebuild streaming data on save.
 */
void ULevel::PreSave()
{
	Super::PreSave();

	if( !IsTemplate() )
	{
		UPackage* Package = CastChecked<UPackage>(GetOutermost());

		ValidateLightGUIDs();

		// Build bsp-trimesh data for physics engine
		BuildPhysBSPData();

		// clean up the nav list
		GWorld->RemoveLevelNavList(this);
        // if one of the pointers are NULL then eliminate both to prevent possible crashes during gameplay before paths are rebuilt
        if (NavListStart == NULL || NavListEnd == NULL)
        {
            if (HasPathNodes())
            {
                //@todo - add a message box (but make sure it doesn't pop up for autosaves?)
                debugf(NAME_Warning,TEXT("PATHING NEEDS TO BE REBUILT FOR %s"),*GetPathName());
            }
            NavListStart = NULL;
            NavListEnd = NULL;
        }

		// Associate portal teleporters with portal volumes
		AssociatePortals();

		// Clear out any crosslevel references
		for( INT ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
		{
			AActor *Actor = Actors(ActorIdx);
			if( Actor != NULL )
			{
				Actor->ClearCrossLevelReferences();
			}
		}

		// Build the list of cross level actors
		CrossLevelActors.Empty();
		for( INT ActorIdx = 0; ActorIdx < Actors.Num(); ActorIdx++ )
		{
			AActor *Actor = Actors(ActorIdx);
			if( Actor != NULL && !Actor->IsPendingKill() )
			{
				TArray<FActorReference*> ActorRefs;
				Actor->GetActorReferences(ActorRefs,TRUE);
				Actor->GetActorReferences(ActorRefs,FALSE);
				if( ActorRefs.Num() > 0 )
				{
					// and null the cross level references
					UBOOL bHasCrossLevelRef = FALSE;
					for( INT Idx = 0; Idx < ActorRefs.Num(); Idx++ )
					{
						if( ActorRefs(Idx)->Actor == NULL || Cast<ULevel>(ActorRefs(Idx)->Actor->GetOuter()) != this )
						{
							bHasCrossLevelRef = TRUE;
							ActorRefs(Idx)->Actor = NULL;
						}
						else
						{
							ActorRefs(Idx)->Guid = FGuid(EC_EventParm);
						}
					}
					if( bHasCrossLevelRef )
					{
						CrossLevelActors.AddItem(Actor);
					}
				}
			}
		}

		// Fixup crosslevel cover refs/guids
		ClearCrossLevelCoverReferences();

		// Don't rebuild streaming data if we're saving out a cooked package as the raw data required has already been stripped.
		if( !(Package->PackageFlags & PKG_Cooked) )
		{
			BuildStreamingData(NULL, this);
		}
	}
}

/**
 * Removes existing line batch components from actors and associates streaming data with level.
 */
void ULevel::PostLoad()
{
	Super::PostLoad();

	//@todo: investigate removal of code cleaning up existing LineBatchComponents.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if(Actor)
		{
			for( INT ComponentIndex=0; ComponentIndex<Actor->Components.Num(); ComponentIndex++ )
			{
				UActorComponent* Component = Actor->Components(ComponentIndex);
				if( Component && Component->IsA(ULineBatchComponent::StaticClass()) )
				{
					check(!Component->IsAttached());
					Actor->Components.Remove(ComponentIndex--);
				}
			}
		}
	}

#if BATMAN && 0
	// Expand AStaticLightCollectionActors on load
	if (GIsEditor)
	{
		for (INT ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++)
		{
			AActor* Actor = Actors(ActorIndex);
			if (Actor && Actor->IsA(AStaticLightCollectionActor::StaticClass()))
			{
				AStaticLightCollectionActor* LightCollection = (AStaticLightCollectionActor*)Actor;

				for (INT CompIndex = 0; CompIndex < LightCollection->Components.Num(); CompIndex++)
				{
					ULightComponent* LightComp = (ULightComponent*)LightCollection->Components(CompIndex);
					ALight* LightActor = NULL;

					// Choose actor class
					if (LightComp->IsA(UPointLightComponent::StaticClass()))
					{
						LightActor = ConstructObject<APointLight>(APointLight::StaticClass(), this, LightComp->GetFName());
					}
					else if (LightComp->IsA(UDirectionalLightComponent::StaticClass()))
					{
						LightActor = ConstructObject<ADirectionalLight>(ADirectionalLight::StaticClass(), this, LightComp->GetFName());
					}
					else if (LightComp->IsA(USpotLightComponent::StaticClass()))
					{
						LightActor = ConstructObject<ASpotLight>(ASpotLight::StaticClass(), this, LightComp->GetFName());
					}
					else if (LightComp->IsA(USkyLightComponent::StaticClass()))
					{
						LightActor = ConstructObject<ASkyLight>(ASkyLight::StaticClass(), this, LightComp->GetFName());
					}
					else
					{
						warnf(NAME_Warning, TEXT("Unknown ULightComponent type '%s'"), *LightComp->GetClass()->GetFullName());
					}

					// Get transform from component
					FMatrix LightToWorld = FMatrix::Identity;
					if (LightComp->IsA(UPointLightComponent::StaticClass()))
					{
						LightToWorld = ((UPointLightComponent*)LightComp)->CachedParentToWorld;
					}
					else
					{
						// ULightComponent::SetParentToWorld multiplies the ParentToWorld by a matrix which flips the X and Z
						// axis values, so in order for the component's final LightToWorld to remain the same, we'll need to
						// flip the current value here so that when it's flipped in SetParentToWorld it ends up the correct value.
						static FMatrix ReverseZAxisMat =
							FMatrix(
								FPlane(+0, +0, +1, +0),
								FPlane(+0, +1, +0, +0),
								FPlane(+1, +0, +0, +0),
								FPlane(+0, +0, +0, +1)
							);

						LightToWorld = ReverseZAxisMat * LightComp->LightToWorld;
					}

					// Add actor to level
					if (LightActor)
					{
						// Destroy default component for newly-created actor
						LightActor->Components.Remove(0);

						Actors.AddItem(LightActor);
						LightActor->WorldInfo = GetWorldInfo();
						LightActor->Location = LightToWorld.GetOrigin();
						LightActor->Rotation = LightToWorld.Rotator();

						// Copy light properties from original
						LightActor->LightComponent->LightGuid = LightComp->LightGuid;
						LightActor->LightComponent->LightmapGuid = LightComp->LightmapGuid;
						LightActor->LightComponent->CastShadows = LightComp->CastShadows;
						LightActor->LightComponent->CastStaticShadows = LightComp->CastStaticShadows;
						LightActor->LightComponent->CastDynamicShadows = LightComp->CastDynamicShadows;

						// TODO: 10 isn't based on anything, what's the real reason lights are so bright otherwise?
						LightActor->LightComponent->SetLightProperties(LightComp->Brightness / 10, LightComp->LightColor, LightComp->Function);
						LightActor->LightComponent->SetEnabled(LightComp->bEnabled);

						LightCollection->Components.Remove(CompIndex--);
					}
				}

				if (LightCollection->Components.Num())
				{
					warnf(NAME_Warning, TEXT("AStaticLightCollectionActor destroyed with %d components still attached"), LightCollection->Components.Num());
				}

				LightCollection->Components.Empty();
				Actors.Remove(ActorIndex--);
			}
		}
	}

	// Expand AStaticMeshCollectionActors on load
	if (GIsEditor)
	{
		for (INT ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++)
		{
			AActor* Actor = Actors(ActorIndex);
			if (Actor && Actor->IsA(AStaticMeshCollectionActor::StaticClass()))
			{
				AStaticMeshCollectionActor* MeshCollection = (AStaticMeshCollectionActor*)Actor;

				for (INT CompIndex = 0; CompIndex < MeshCollection->Components.Num(); CompIndex++)
				{
					UStaticMeshComponent* MeshComp = (UStaticMeshComponent*)MeshCollection->Components(CompIndex);
					if (!MeshComp)
					{
						continue;
					}

					AStaticMeshActor* MeshActor = ConstructObject<AStaticMeshActor>(AStaticMeshActor::StaticClass(), this, MeshComp->GetFName());

					Actors.AddItem(MeshActor);
					MeshActor->WorldInfo = GetWorldInfo();
					MeshActor->Location = MeshComp->CachedParentToWorld.GetOrigin();
					MeshActor->Rotation = MeshComp->CachedParentToWorld.Rotator();

					MeshComp->Rename(NULL, MeshActor, REN_ForceNoResetLoaders);
					MeshActor->Components.AddItem(MeshComp);
					MeshCollection->Components.Remove(CompIndex--);
				}

				if (!MeshCollection->Components.Num())
				{
					Actors.RemoveItem(MeshCollection);
				}
			}
		}
	}
#endif

	// reattach decals to receivers after level has been fully loaded
	GEngine->IssueDecalUpdateRequest();

	// in the Editor, sort Actor list immediately (at runtime we wait for the level to be added to the world so that it can be delayed in the level streaming case)
	if (GIsEditor)
	{
		SortActorList();
	}

	// Remove UTexture2D references that are NULL (missing texture).
	ForceStreamTextures.RemoveKey( NULL );
}

/**
 * Clears all components of actors associated with this level (aka in Actors array) and 
 * also the BSP model components.
 */
void ULevel::ClearComponents()
{
	bAreComponentsCurrentlyAttached = FALSE;

	// Remove the model components from the scene.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->ConditionalDetach();
		}
	}

	// Remove the actors' components from the scene.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor )
		{
			Actor->ClearComponents();
		}
	}

	// Iterate over all textures with distance based streaming referenced by the level and reset their last render
	// time. This avoids the case where a level is removed but its textures are being streamed in as they are no 
	// longer handled by the distance based streaming code and therefore fall back to using streaming based on
	// last render time.
	for( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(TextureToInstancesMap); It; ++It )
	{
		UTexture2D* Texture = It.Key();
		if( Texture && Texture->Resource )
		{
			// Reset last render time. This is not thread safe but sufficient for our purposes.
			Texture->Resource->LastRenderTime = -FLT_MAX;
		}
	}
	for( TMap<UPrimitiveComponent*,TArray<FDynamicTextureInstance> >::TIterator It(DynamicTextureInstances); It; ++It )
	{
		TArray<FDynamicTextureInstance>& TextureInstances = It.Value();
		for ( INT InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
		{
			UTexture2D* Texture = TextureInstances(InstanceIndex).Texture;
			if( Texture && Texture->Resource )
			{
				// Reset last render time. This is not thread safe but sufficient for our purposes.
				Texture->Resource->LastRenderTime = -FLT_MAX;
			}
		}
	}

	// clear global motion blur state info
	if (GEngine != NULL && 
		GEngine->GameViewport != NULL &&
		GEngine->GameViewport->Viewport != NULL)
	{
		GEngine->GameViewport->Viewport->SetClearMotionBlurInfoGameThread(TRUE);
	}
}

/**
 * A TMap key type used to sort BSP nodes by locality and zone.
 */
struct FModelComponentKey
{
	UINT	ZoneIndex;
	UINT	X;
	UINT	Y;
	UINT	Z;
	DWORD	MaskedPolyFlags;
	DWORD	LightingChannels;

	friend UBOOL operator==(const FModelComponentKey& A,const FModelComponentKey& B)
	{
		return	A.ZoneIndex == B.ZoneIndex 
			&&	A.X == B.X 
			&&	A.Y == B.Y 
			&&	A.Z == B.Z 
			&&	A.MaskedPolyFlags == B.MaskedPolyFlags
			&&	A.LightingChannels == B.LightingChannels;
	}

	friend DWORD GetTypeHash(const FModelComponentKey& Key)
	{
		return appMemCrc(&Key,sizeof(Key),0);
	}
};

/**
 * Updates all components of actors associated with this level (aka in Actors array) and 
 * creates the BSP model components.
 */
void ULevel::UpdateComponents()
{
	// Update all components in one swoop.
	IncrementalUpdateComponents( 0 );
}


/**
 * Incrementally updates all components of actors associated with this level.
 *
 * @param NumComponentsToUpdate	Number of components to update in this run, 0 for all
 */
void ULevel::IncrementalUpdateComponents( INT NumComponentsToUpdate )
{
	// A value of 0 means that we want to update all components.
	UBOOL bForceUpdateAllActors = FALSE;
	if( NumComponentsToUpdate == 0 )
	{
		NumComponentsToUpdate = Actors.Num();
		bForceUpdateAllActors = TRUE;
	}
	// Only the game can use incremental update functionality.
	else
	{
		checkMsg(!GIsEditor && GIsGame,TEXT("Cannot call IncrementalUpdateComponents with non 0 argument in the Editor/ commandlets."));
	}

	// Do BSP on the first pass.
	if( CurrentActorIndexForUpdateComponents == 0 )
	{
		UpdateModelComponents();
	}

	// Do as many Actor's as we were told, with the exception of 'collection' actors. They contain a variable number of 
	// components that can take more time than we are anticipating at a higher level. Unless we do a force full update we
	// only do up to the first collection and only one collection at a time.
	UBOOL bShouldBailOutEarly = FALSE;
	NumComponentsToUpdate = Min( NumComponentsToUpdate, Actors.Num() - CurrentActorIndexForUpdateComponents );
	for( INT i=0; i<NumComponentsToUpdate && !bShouldBailOutEarly; i++ )
	{
		AActor* Actor = Actors(CurrentActorIndexForUpdateComponents++);
		if( Actor )
		{
			// Request an early bail out if we encounter a SMCA... unless we force update all actors.
			UBOOL bIsCollectionActor = Actor->IsA(AStaticMeshCollectionActor::StaticClass()) || Actor->IsA(AProcBuilding::StaticClass());
			bShouldBailOutEarly = bIsCollectionActor ? !bForceUpdateAllActors : FALSE; 

			// Always do at least one and keep going as long as its not a SMCA
			if( !bShouldBailOutEarly || i == 0 )  
			{
#if PERF_TRACK_DETAILED_ASYNC_STATS
				DOUBLE Start = appSeconds();
#endif

				Actor->ClearComponents();
				Actor->ConditionalUpdateComponents();
				// Shrink various components arrays for static actors to avoid waste due to array slack.
				if( Actor->IsStatic() )
				{
					Actor->Components.Shrink();
					Actor->AllComponents.Shrink();
				}

#if PERF_TRACK_DETAILED_ASYNC_STATS
				// Add how long this took to class->time map
				DOUBLE Time = appSeconds() - Start;
				UClass* ActorClass = Actor->GetClass();
				FMapTimeEntry* CurrentEntry = UpdateComponentsTimePerActorClass.Find(ActorClass);
				// Is an existing entry - add to it
				if(CurrentEntry)
				{
					CurrentEntry->Time += Time;
					CurrentEntry->ObjCount += 1;
				}
				// Make a new entry for this class
				else
				{
					UpdateComponentsTimePerActorClass.Set(ActorClass, FMapTimeEntry(ActorClass, 1, Time));
				}
#endif
			}
			else
			{
				// Rollback since we didn't actually process the actor
				CurrentActorIndexForUpdateComponents--;
				break;
			}		
		}
	}

	// See whether we are done.
	if( CurrentActorIndexForUpdateComponents == Actors.Num() )
	{
		CurrentActorIndexForUpdateComponents	= 0;
		bAreComponentsCurrentlyAttached			= TRUE;
	}
	// Only the game can use incremental update functionality.
	else
	{
		check(!GIsEditor && GIsGame);
	}
}



/**
 * Updates the model components associated with this level
 */
void ULevel::UpdateModelComponents()
{
	// Create/update the level's BSP model components.
	if(!ModelComponents.Num())
	{
		// Update the model vertices and edges.
		Model->UpdateVertices();

		Model->InvalidSurfaces = 0;

	    // Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();

		TMap< FModelComponentKey, TArray<WORD> > ModelComponentMap;

		// Sort the nodes by zone, grid cell and masked poly flags.
		for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
		{
			FBspNode& Node = Model->Nodes(NodeIndex);
			FBspSurf& Surf = Model->Surfs(Node.iSurf);

			if(Node.NumVertices > 0)
			{
				for(INT BackFace = 0;BackFace < ((Surf.PolyFlags & PF_TwoSided) ? 2 : 1);BackFace++)
				{
					// Calculate the bounding box of this node.
					FBox NodeBounds(0);
					for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
					{
						NodeBounds += Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
					}

					// Create a sort key for this node using the grid cell containing the center of the node's bounding box.
#define MODEL_GRID_SIZE_XY	2048.0f
#define MODEL_GRID_SIZE_Z	4096.0f
					FModelComponentKey Key;
					Key.ZoneIndex		= Model->NumZones ? Node.iZone[1 - BackFace] : INDEX_NONE;
					Key.X				= appFloor(NodeBounds.GetCenter().X / MODEL_GRID_SIZE_XY);
					Key.Y				= appFloor(NodeBounds.GetCenter().Y / MODEL_GRID_SIZE_XY);
					Key.Z				= appFloor(NodeBounds.GetCenter().Z / MODEL_GRID_SIZE_Z);
					Key.MaskedPolyFlags = Surf.PolyFlags & PF_ModelComponentMask;
					Key.LightingChannels = Surf.LightingChannels.Bitfield;
					// Don't accept lights if material is unlit.
					if( Surf.Material && Surf.Material->GetMaterial() && Surf.Material->GetMaterial()->LightingModel == MLM_Unlit )
					{
						Key.MaskedPolyFlags	= Key.MaskedPolyFlags & (~PF_AcceptsLights);
					}
			
					// Find an existing node list for the grid cell.
					TArray<WORD>* ComponentNodes = ModelComponentMap.Find(Key);
					if(!ComponentNodes)
					{
						// This is the first node we found in this grid cell, create a new node list for the grid cell.
						ComponentNodes = &ModelComponentMap.Set(Key,TArray<WORD>());
					}

					// Add the node to the grid cell's node list.
					ComponentNodes->AddUniqueItem(NodeIndex);
				}
			}
			else
			{
				// Put it in component 0 until a rebuild occurs.
 				Node.ComponentIndex = 0;
			}
		}

		// Create a UModelComponent for each grid cell's node list.
		for(TMap< FModelComponentKey, TArray<WORD> >::TConstIterator It(ModelComponentMap);It;++It)
		{
			const FModelComponentKey&	Key		= It.Key();
			const TArray<WORD>&			Nodes	= It.Value();	

			for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
			{
				Model->Nodes(Nodes(NodeIndex)).ComponentIndex = ModelComponents.Num();							
				Model->Nodes(Nodes(NodeIndex)).ComponentNodeIndex = NodeIndex;
			}
			
			UModelComponent* ModelComponent = new(this) UModelComponent(Model,Key.ZoneIndex,ModelComponents.Num(),Key.MaskedPolyFlags,Key.LightingChannels,Nodes);
			ModelComponents.AddItem(ModelComponent);

			for(INT NodeIndex = 0;NodeIndex < Nodes.Num();NodeIndex++)
			{
				Model->Nodes(Nodes(NodeIndex)).ComponentElementIndex = INDEX_NONE;
				
				const WORD								Node	 = Nodes(NodeIndex);
				const TIndirectArray<FModelElement>&	Elements = ModelComponent->GetElements();
				for( INT ElementIndex=0; ElementIndex<Elements.Num(); ElementIndex++ )
				{
					if( Elements(ElementIndex).Nodes.FindItemIndex( Node ) != INDEX_NONE )
					{
						Model->Nodes(Nodes(NodeIndex)).ComponentElementIndex = ElementIndex;
						break;
					}
				}
			}
		}

		// Clear old cached data in case we don't regenerate it below, e.g. after removing all BSP from a level.
		Model->NumIncompleteNodeGroups = 0;
		Model->CachedMappings.Empty();

		// Work only needed if we actually have BSP in the level.
		if( ModelComponents.Num() )
		{
			// Build the static lighting vertices!
			/** The lights in the world which the system is building. */
			TArray<ULightComponent*> Lights;
			// Prepare lights for rebuild.
			for(TObjectIterator<ULightComponent> LightIt;LightIt;++LightIt)
			{
				ULightComponent* const Light = *LightIt;
				const UBOOL bLightIsInWorld = Light->GetOwner() && GWorld->ContainsActor(Light->GetOwner());
				if (bLightIsInWorld && (Light->HasStaticShadowing() || Light->HasStaticLighting()))
				{
					// Make sure the light GUIDs and volumes are up-to-date.
					Light->ValidateLightGUIDs();
					Light->UpdateVolumes();

					// Add the light to the system's list of lights in the world.
					Lights.AddItem(Light);
				}
			}

			// For BSP, we aren't Component-centric, so we can't use the GetStaticLightingInfo 
			// function effectively. Instead, we look across all nodes in the Level's model and
			// generate NodeGroups - which are groups of nodes that are coplanar, adjacent, and 
			// have the same lightmap resolution (henceforth known as being "conodes"). Each 
			// NodeGroup will get a mapping created for it

			// create all NodeGroups
			Model->GroupAllNodes(this, Lights);

			// now we need to make the mappings/meshes
			for (TMap<INT, FNodeGroup*>::TIterator It(Model->NodeGroups); It; ++It)
			{
				FNodeGroup* NodeGroup = It.Value();

				if (NodeGroup->Nodes.Num())
				{
					// get one of the surfaces/components from the NodeGroup
					// @lmtodo: Remove need for GetSurfaceLightMapResolution to take a surfaceindex, or a ModelComponent :)
					UModelComponent* SomeModelComponent = ModelComponents(Model->Nodes(NodeGroup->Nodes(0)).ComponentIndex);
					INT SurfaceIndex = Model->Nodes(NodeGroup->Nodes(0)).iSurf;

					// fill out the NodeGroup/mapping, as UModelComponent::GetStaticLightingInfo did
					SomeModelComponent->GetSurfaceLightMapResolution(SurfaceIndex, TRUE, NodeGroup->SizeX, NodeGroup->SizeY, NodeGroup->WorldToMap, &NodeGroup->Nodes);
					NodeGroup->MapToWorld = NodeGroup->WorldToMap.Inverse();

					// Cache the surface's vertices and triangles.
					NodeGroup->BoundingBox.Init();

					UBOOL bForceLightMap = FALSE;

					for(INT NodeIndex = 0;NodeIndex < NodeGroup->Nodes.Num();NodeIndex++)
					{
						const FBspNode& Node = Model->Nodes(NodeGroup->Nodes(NodeIndex));
						const FBspSurf& NodeSurf = Model->Surfs(Node.iSurf);
						// If ANY surfaces in this group has ForceLightMap set, they all get it...
						if ((NodeSurf.PolyFlags & PF_ForceLightMap) > 0)
						{
							bForceLightMap = TRUE;
						}
						const FVector& TextureBase = Model->Points(NodeSurf.pBase);
						const FVector& TextureX = Model->Vectors(NodeSurf.vTextureU);
						const FVector& TextureY = Model->Vectors(NodeSurf.vTextureV);
						const INT BaseVertexIndex = NodeGroup->Vertices.Num();
						// Compute the surface's tangent basis.
						FVector NodeTangentX = Model->Vectors(NodeSurf.vTextureU).SafeNormal();
						FVector NodeTangentY = Model->Vectors(NodeSurf.vTextureV).SafeNormal();
						FVector NodeTangentZ = Model->Vectors(NodeSurf.vNormal).SafeNormal();

						// Generate the node's vertices.
						for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							/*const*/ FVert& Vert = Model->Verts(Node.iVertPool + VertexIndex);
							const FVector& VertexWorldPosition = Model->Points(Vert.pVertex);

							FStaticLightingVertex* DestVertex = new(NodeGroup->Vertices) FStaticLightingVertex;
							DestVertex->WorldPosition = VertexWorldPosition;
							DestVertex->TextureCoordinates[0].X = ((VertexWorldPosition - TextureBase) | TextureX) / 128.0f;
							DestVertex->TextureCoordinates[0].Y = ((VertexWorldPosition - TextureBase) | TextureY) / 128.0f;
							DestVertex->TextureCoordinates[1].X = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).X;
							DestVertex->TextureCoordinates[1].Y = NodeGroup->WorldToMap.TransformFVector(VertexWorldPosition).Y;
							DestVertex->WorldTangentX = NodeTangentX;
							DestVertex->WorldTangentY = NodeTangentY;
							DestVertex->WorldTangentZ = NodeTangentZ;

							// TEMP - Will be overridden when lighting is build!
							Vert.ShadowTexCoord = DestVertex->TextureCoordinates[1];

							// Include the vertex in the surface's bounding box.
							NodeGroup->BoundingBox += VertexWorldPosition;
						}

						// Generate the node's vertex indices.
						for(UINT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + 0);
							NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex);
							NodeGroup->TriangleVertexIndices.AddItem(BaseVertexIndex + VertexIndex - 1);

							// track the source surface for each triangle
							NodeGroup->TriangleSurfaceMap.AddItem(Node.iSurf);
						}
					}
				}
			}
		}
		Model->UpdateVertices();

		for (INT UpdateCompIdx = 0; UpdateCompIdx < ModelComponents.Num(); UpdateCompIdx++)
		{
			UModelComponent* ModelComp = ModelComponents(UpdateCompIdx);
			ModelComp->GenerateElements(TRUE);
		}
	}
	else
	{
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents(ComponentIndex))
			{
				ModelComponents(ComponentIndex)->ConditionalDetach();
			}
		}
	}

	// Initialize the model's index buffers.
	for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
		IndexBufferIt;
		++IndexBufferIt)
	{
		BeginInitResource(IndexBufferIt.Value());
	}

	// Update model components.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->ConditionalAttach(GWorld->Scene,NULL,FMatrix::Identity);
		}
	}
}


/** Called before an Undo action occurs */
void ULevel::PreEditUndo()
{
	Super::PreEditUndo();

	// Release the model's resources.
	Model->BeginReleaseResources();
	Model->ReleaseResourcesFence.Wait();

	// Detach existing model components.  These are left in the array, so they are saved for undoing the undo.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->ConditionalDetach();
		}
	}

	// Wait for the components to be detached.
	FlushRenderingCommands();
}


/** Called after an Undo action occurs */
void ULevel::PostEditUndo()
{
	Super::PostEditUndo();
	
	// Rebuild the list of tickable actors because a tickable actor may have 
	// been removed during undo or redo. In that case, the tickable actors list 
	// will be holding a pointer to garbage after the next garbage collection, 
	// which will crash the engine.
	RebuildTickableActors();
	Model->UpdateVertices();
	// Update model components that were detached earlier
	UpdateModelComponents();
}



/**
 * Invalidates the cached data used to render the level's UModel.
 */
void ULevel::InvalidateModelGeometry()
{
	// Save the level/model state for transactions.
	Model->Modify();
	Modify();

	// Begin releasing the model's resources.
	Model->BeginReleaseResources();

	// Remove existing model components.
	for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
	{
		if(ModelComponents(ComponentIndex))
		{
			ModelComponents(ComponentIndex)->Modify();
			ModelComponents(ComponentIndex)->ConditionalDetach();
		}
	}
	ModelComponents.Empty();
}

/**
 * Discards the cached data used to render the level's UModel.  Assumes that the
 * faces and vertex positions haven't changed, only the applied materials.
 */
void ULevel::InvalidateModelSurface()
{
	Model->InvalidSurfaces = TRUE;
}

void ULevel::CommitModelSurfaces()
{
	if(Model->InvalidSurfaces)
	{
		// Detach the model components
		TIndirectArray<FPrimitiveSceneAttachmentContext> ComponentContexts;
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			UPrimitiveComponent* Component = ModelComponents(ComponentIndex);
			if (Component)
			{
				new(ComponentContexts) FPrimitiveSceneAttachmentContext(Component);
			}
		}

		// Begin releasing the model's resources.
		Model->BeginReleaseResources();

		// Wait for the model's resources to be released.
		FlushRenderingCommands();

		// Clear the model index buffers.
		Model->MaterialIndexBuffers.Empty();

		// Update the model vertices.
		Model->UpdateVertices();

		// Update the model components.
		for(INT ComponentIndex = 0;ComponentIndex < ModelComponents.Num();ComponentIndex++)
		{
			if(ModelComponents(ComponentIndex))
			{
				ModelComponents(ComponentIndex)->CommitSurfaces();
			}
		}
		Model->InvalidSurfaces = 0;
		
		// Initialize the model's index buffers.
		for(TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator IndexBufferIt(Model->MaterialIndexBuffers);
			IndexBufferIt;
			++IndexBufferIt)
		{
			BeginInitResource(IndexBufferIt.Value());
		}

		// After this line, the elements in the ComponentContexts array will be destructed, causing components to reattach.
	}
}

IMPLEMENT_COMPARE_CONSTREF( FStreamableTextureInstance, BuildStreamingData, { return (A.TexelFactor - B.TexelFactor) >= 0.0f ? 1 : -1 ; } )

/**
 * Rebuilds static streaming data for all levels in the specified UWorld.
 *
 * @param World				Which world to rebuild streaming data for. If NULL, all worlds will be processed.
 * @param TargetLevel		[opt] Specifies a single level to process. If NULL, all levels will be processed.
 * @param TargetTexture		[opt] Specifies a single texture to process. If NULL, all textures will be processed.
 */
void ULevel::BuildStreamingData(UWorld* World, ULevel* TargetLevel/*=NULL*/, UTexture2D* TargetTexture/*=NULL*/)
{
#if !CONSOLE
	DOUBLE StartTime = appSeconds();

	UBOOL bUseDynamicStreaming = FALSE;
	GConfig->GetBool(TEXT("TextureStreaming"), TEXT("UseDynamicStreaming"), bUseDynamicStreaming, GEngineIni);

	// Clear the streaming data.
	if ( TargetLevel )
	{
		// Update the streaming manager.
		GStreamingManager->RemoveLevel( TargetLevel );
		TargetLevel->TextureToInstancesMap.Empty();
		TargetLevel->DynamicTextureInstances.Empty();
		TargetLevel->ForceStreamTextures.Empty();
	}
	else if ( World )
	{
		for ( INT LevelIndex=0; LevelIndex < World->Levels.Num(); LevelIndex++ )
		{
			ULevel* Level = World->Levels(LevelIndex);
			// Update the streaming manager.
			GStreamingManager->RemoveLevel( Level );
			Level->TextureToInstancesMap.Empty();
			Level->DynamicTextureInstances.Empty();
			Level->ForceStreamTextures.Empty();
		}
	}
	else
	{
		for (TObjectIterator<ULevel> It; It; ++It)
		{
			ULevel* Level = *It;
			// Update the streaming manager.
			GStreamingManager->RemoveLevel( Level );
			Level->TextureToInstancesMap.Empty();
			Level->DynamicTextureInstances.Empty();
			Level->ForceStreamTextures.Empty();
		}
	}

	for ( TObjectIterator<UPrimitiveComponent> It; It; ++It )
	{
		UPrimitiveComponent* Primitive = *It;

		UBOOL bIsClassDefaultOjbect = Primitive->IsTemplate(RF_ClassDefaultObject);

		if ( !bIsClassDefaultOjbect && (Primitive->IsAttached() || GIsCooking) )
		{
			// Find which level the primitive resides in.
			ULevel* Level = NULL;
			if ( TargetLevel )
			{
				Level = Primitive->IsIn(TargetLevel) ? TargetLevel : NULL;
			}
			else
			{
				for (UObject* It=Primitive; It && !Level; It = It->GetOuter())
				{
					Level = Cast<ULevel>(It);
				}
			}
			UBOOL bProcessLevel = (Level && !World) ? TRUE : FALSE;

			// Check that this level is part of the specified UWorld.
			if ( Level && World )
			{
				for ( INT LevelIndex=0; LevelIndex < World->Levels.Num(); LevelIndex++ )
				{
					if ( Level == World->Levels(LevelIndex) )
					{
						bProcessLevel = TRUE;
						break;
					}
				}
			}

			if ( bProcessLevel )
			{
				const AActor* const Owner				= Primitive->GetOwner();
				const UBOOL bIsStaticMeshComponent		= Primitive->IsA(UStaticMeshComponent::StaticClass());
				const UBOOL bIsSkeletalMeshComponent	= Primitive->IsA(USkeletalMeshComponent::StaticClass());
				const UBOOL bIsStatic					= !Owner || Owner->IsStatic();
				const UBOOL bIsLevelPlacedKActor		= Owner && Owner->bNoDelete && Owner->IsA(AKActor::StaticClass());
				const UBOOL bUseAllStaticMeshComponents	= bIsStaticMeshComponent && (!Owner || Owner->bConsiderAllStaticMeshComponentsForStreaming);
				const UBOOL bStreamNonWorldTextures		= bIsStatic || bIsLevelPlacedKActor || bUseAllStaticMeshComponents;

				// Ask the primitive to enumerate the streaming textures it uses.
				TArray<FStreamingTexturePrimitiveInfo> PrimitiveStreamingTextures;
				Primitive->GetStreamingTextureInfo(PrimitiveStreamingTextures);

				for(INT TextureIndex = 0;TextureIndex < PrimitiveStreamingTextures.Num();TextureIndex++)
				{
					const FStreamingTexturePrimitiveInfo& PrimitiveStreamingTexture = PrimitiveStreamingTextures(TextureIndex);
					UTexture2D* Texture2D = Cast<UTexture2D>(PrimitiveStreamingTexture.Texture);
					UBOOL bCanBeStreamedByDistance = !appIsNearlyZero(PrimitiveStreamingTexture.TexelFactor) && !appIsNearlyZero(PrimitiveStreamingTexture.Bounds.W);

					// Only handle 2D textures that match the target texture.
					const UBOOL bIsTargetTexture = (!TargetTexture || TargetTexture == Texture2D);
					UBOOL bShouldHandleTexture = (Texture2D && bIsTargetTexture);

					// Check if this is a lightmap/shadowmap that shouldn't be streamed.
					if ( bShouldHandleTexture )
					{
						UShadowMapTexture2D* ShadowMap2D	= Cast<UShadowMapTexture2D>(Texture2D);
						ULightMapTexture2D* Lightmap2D		= Cast<ULightMapTexture2D>(Texture2D);
						if ( (Lightmap2D && (Lightmap2D->LightmapFlags & LMF_Streamed) == 0) ||
							(ShadowMap2D && (ShadowMap2D->ShadowmapFlags & SMF_Streamed) == 0) )
						{
							bShouldHandleTexture			= FALSE;
						}
					}

					if(bShouldHandleTexture)
					{
						// Check if this is a world texture.
						const UBOOL bIsWorldTexture			= 
							Texture2D->LODGroup == TEXTUREGROUP_World ||
							Texture2D->LODGroup == TEXTUREGROUP_WorldNormalMap ||
							Texture2D->LODGroup == TEXTUREGROUP_WorldSpecular;

						// Check if we should consider this a static mesh texture instance.
						UBOOL bIsStaticMeshTextureInstance = (bStreamNonWorldTextures || bIsWorldTexture) && !bIsSkeletalMeshComponent;

						// Treat textures bIsLevelPlacedKActor dynamically instead.
						if ( bIsLevelPlacedKActor && bUseDynamicStreaming )
						{
							bIsStaticMeshTextureInstance = FALSE;
						}

						// Is the primitive set to force its textures to be resident?
						if ( Primitive->bForceMipStreaming )
						{
							// Add them to the ForceStreamTextures set.
							Level->ForceStreamTextures.Set(Texture2D,TRUE);
						}
						// Is this a static mesh texture instance?
						else if ( bIsStaticMeshTextureInstance && bCanBeStreamedByDistance )
						{
							// Texture instance information.
							FStreamableTextureInstance TextureInstance;
							TextureInstance.BoundingSphere	= PrimitiveStreamingTexture.Bounds;
							TextureInstance.TexelFactor		= PrimitiveStreamingTexture.TexelFactor;

							// See whether there already is an instance in the level.
							TArray<FStreamableTextureInstance>* TextureInstances = Level->TextureToInstancesMap.Find( Texture2D );
							// We have existing instances.
							if( TextureInstances )
							{
								// Add to the array.
								TextureInstances->AddItem( TextureInstance );
							}
							// This is the first instance.
							else
							{
								// Create array with current instance as the only entry.
								TArray<FStreamableTextureInstance> NewTextureInstances;
								NewTextureInstances.AddItem( TextureInstance );
								// And set it .
								Level->TextureToInstancesMap.Set( Texture2D, NewTextureInstances );
							}
						}
						// Is the texture used by a dynamic object that we can track at run-time.
						else if ( bUseDynamicStreaming && Owner && bCanBeStreamedByDistance )
						{
							// Texture instance information.
							FDynamicTextureInstance TextureInstance;
							TextureInstance.Texture = Texture2D;
							TextureInstance.BoundingSphere = PrimitiveStreamingTexture.Bounds;
							TextureInstance.TexelFactor	= PrimitiveStreamingTexture.TexelFactor;
							TextureInstance.OriginalRadius = PrimitiveStreamingTexture.Bounds.W;

							// See whether there already is an instance in the level.
							TArray<FDynamicTextureInstance>* TextureInstances = Level->DynamicTextureInstances.Find( Primitive );
							// We have existing instances.
							if( TextureInstances )
							{
								// Add to the array.
								TextureInstances->AddItem( TextureInstance );
							}
							// This is the first instance.
							else
							{
								// Create array with current instance as the only entry.
								TArray<FDynamicTextureInstance> NewTextureInstances;
								NewTextureInstances.AddItem( TextureInstance );
								// And set it .
								Level->DynamicTextureInstances.Set( Primitive, NewTextureInstances );
							}
						}
					}
				}
			}
		}
	}

	TObjectIterator<ULevel> It;
	INT LevelIndex = 0;
	while ( true )
	{
		ULevel* Level;
		if ( TargetLevel )
		{
			Level = TargetLevel;
		}
		else if ( World )
		{
			if ( LevelIndex >= World->Levels.Num() )
			{
				break;
			}
			Level = World->Levels(LevelIndex++);
		}
		else
		{
			if ( !It )
			{
				break;
			}
			Level = *It;
			++It;
		}

		for ( TMap<UTexture2D*,TArray<FStreamableTextureInstance> >::TIterator It(Level->TextureToInstancesMap); It; ++It )
		{
			UTexture2D* Texture2D = TargetTexture ? TargetTexture : It.Key();
			if ( Texture2D->LODGroup == TEXTUREGROUP_Lightmap || Texture2D->LODGroup == TEXTUREGROUP_Shadowmap )
			{
				TArray<FStreamableTextureInstance>& TextureInstances = It.Value();

				// Clamp texelfactors to 20-80% range.
				// This is to prevent very low-res or high-res charts to dominate otherwise decent streaming.
				Sort<USE_COMPARE_CONSTREF(FStreamableTextureInstance,BuildStreamingData)>( &(TextureInstances(0)), TextureInstances.Num() );

				FLOAT MinTexelFactor = TextureInstances( TextureInstances.Num() * 0.2f ).TexelFactor;
				FLOAT MaxTexelFactor = TextureInstances( TextureInstances.Num() * 0.8f ).TexelFactor;
				for ( INT InstanceIndex=0; InstanceIndex < TextureInstances.Num(); ++InstanceIndex )
				{
					FStreamableTextureInstance& Instance = TextureInstances(InstanceIndex);
					Instance.TexelFactor = Clamp( Instance.TexelFactor, MinTexelFactor, MaxTexelFactor );
				}
			}
			if ( TargetTexture )
			{
				break;
			}
		}

		// Update the streaming manager.
		GStreamingManager->AddLevel( Level );

		if ( TargetLevel )
		{
			break;
		}
	}

	//debugf(TEXT("ULevel::BuildStreamingData took %.3f seconds."), appSeconds() - StartTime);
#else
	appErrorf(TEXT("ULevel::BuildStreamingData should not be called on a console"));
#endif
}

/**
 * Triggers a call to BuildStreamingData(GWorld,NULL,NULL) within a few seconds.
 */
void ULevel::TriggerStreamingDataRebuild()
{
	bStreamingDataDirty = TRUE;
	BuildStreamingDataTimer = appSeconds() + 5.0;
}

/**
 * Calls BuildStreamingData(GWorld,NULL,NULL) if it has been triggered within the last few seconds.
 */
void ULevel::ConditionallyBuildStreamingData()
{
	if ( bStreamingDataDirty && appSeconds() > BuildStreamingDataTimer )
	{
		bStreamingDataDirty = FALSE;
		BuildStreamingData( GWorld );
	}
}

/**
 *	Retrieves the array of streamable texture isntances.
 *
 */
TArray<FStreamableTextureInstance>* ULevel::GetStreamableTextureInstances(UTexture2D*& TargetTexture)
{
	typedef TArray<FStreamableTextureInstance>	STIA_Type;
	for (TMap<UTexture2D*,STIA_Type>::TIterator It(TextureToInstancesMap); It; ++It)
	{
		TArray<FStreamableTextureInstance>& TSIA = It.Value();
		TargetTexture = It.Key();
		return &TSIA;
	}		

	return NULL;
}

/**
 * Returns the default brush for this level.
 *
 * @return		The default brush for this level.
 */
ABrush* ULevel::GetBrush() const
{
	checkMsg( Actors.Num() >= 2, *GetName() );
	ABrush* DefaultBrush = Cast<ABrush>( Actors(1) );
#if BATMAN
#else
	checkMsg( DefaultBrush != NULL, *GetName() );
	checkMsg( DefaultBrush->BrushComponent, *GetName() );
	checkMsg( DefaultBrush->Brush != NULL, *GetName() );
#endif
	return DefaultBrush;
}

/**
 * Returns the world info for this level.
 *
 * @return		The AWorldInfo for this level.
 */
AWorldInfo* ULevel::GetWorldInfo() const
{
	check( Actors.Num() >= 2 );
	AWorldInfo* WorldInfo = Cast<AWorldInfo>( Actors(0) );
	check( WorldInfo != NULL );
	return WorldInfo;
}

/**
 * Returns the sequence located at the index specified.
 *
 * @return	a pointer to the USequence object located at the specified element of the GameSequences array.  Returns
 *			NULL if the index is not a valid index for the GameSequences array.
 */
USequence* ULevel::GetGameSequence() const
{
	USequence* Result = NULL;

	if( GameSequences.Num() )
	{
		Result = GameSequences(0);
	}

	return Result;
}

/**
 * Initializes all actors after loading completed.
 *
 * @param bForDynamicActorsOnly If TRUE, this function will only act on non static actors
 */
void ULevel::InitializeActors(UBOOL bForDynamicActorsOnly)
{
	UBOOL			bIsServer				= GWorld->IsServer();
	APhysicsVolume*	DefaultPhysicsVolume	= GWorld->GetDefaultPhysicsVolume();

	// Kill non relevant client actors, initialize render time, set initial physic volume, initialize script execution and rigid body physics.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor && ( !bForDynamicActorsOnly || !Actor->IsStatic() ) )
		{
			// Kill off actors that aren't interesting to the client.
			if( !bIsServer && !Actor->bScriptInitialized )
			{
				if (Actor->IsStatic() || Actor->bNoDelete)
				{
					if (!Actor->bExchangedRoles)
					{
						Exchange( Actor->Role, Actor->RemoteRole );
						Actor->bExchangedRoles = TRUE;
					}
				}
				else
				{
					GWorld->DestroyActor( Actor );
				}
			}

			if( !Actor->ActorIsPendingKill() )
			{
				Actor->LastRenderTime	= -FLT_MAX;
				Actor->PhysicsVolume	= DefaultPhysicsVolume;
				Actor->Touching.Empty();
				// don't reinitialize actors that have already been initialized (happens for actors that persist through a seamless level change)
				if (!Actor->bScriptInitialized || Actor->GetStateFrame() == NULL)
				{
					Actor->InitExecution();
				}
			}
		}
	}
}

/**
 * Routes pre and post begin play to actors and also sets volumes.
 *
 * @param bForDynamicActorsOnly If TRUE, this function will only act on non static actors
 *
 * @todo seamless worlds: this doesn't correctly handle volumes in the multi- level case
 */
void ULevel::RouteBeginPlay(UBOOL bForDynamicActorsOnly)
{
	// this needs to only be done once, so when we do this again for reseting
	// dynamic actors, we can't do it again
	if (!bForDynamicActorsOnly)
	{
		GWorld->AddLevelNavList( this, TRUE );
	}

	// Send PreBeginPlay, set zones and collect volumes.
	TArray<AVolume*> LevelVolumes;		
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor && ( !bForDynamicActorsOnly || !Actor->IsStatic() ) )
		{
			if( !Actor->bScriptInitialized && (!Actor->IsStatic() || Actor->bRouteBeginPlayEvenIfStatic) )
			{
				Actor->PreBeginPlay();
			}

			// Only collect non-blocking volumes
			AVolume* Volume = Actor->GetAVolume();
			if( Volume && !Volume->bBlockActors )
			{
				LevelVolumes.AddItem(Volume);
			}
		}
	}

	// Send set volumes, beginplay on components, and postbeginplay.
	for( INT ActorIndex=0; ActorIndex<Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( Actor && ( !bForDynamicActorsOnly || !Actor->IsStatic() ) )
		{
			if( !Actor->bScriptInitialized )
			{
				Actor->SetVolumes( LevelVolumes );
			}

			if( !Actor->IsStatic() || Actor->bRouteBeginPlayEvenIfStatic )
			{
#ifdef PERF_DEBUG_CHECKCOLLISIONCOMPONENTS
				INT NumCollisionComponents = 0;
#endif
				// Call BeginPlay on Components.
				for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
				{
					UActorComponent* ActorComponent = Actor->Components(ComponentIndex);
					if( ActorComponent && ActorComponent->IsAttached() )
					{
						ActorComponent->ConditionalBeginPlay();
#ifdef PERF_DEBUG_CHECKCOLLISIONCOMPONENTS
						UPrimitiveComponent *C = Cast<UPrimitiveComponent>(ActorComponent);
						if ( C && C->ShouldCollide() )
						{
							NumCollisionComponents++;
							if( NumCollisionComponents > 1 )
							{
								debugf(TEXT("additional collision component %s owned by %s"), *C->GetName(), *GetName());
							}
						}
#endif
					}
				}
			}
			if( !Actor->bScriptInitialized )
			{
				if( !Actor->IsStatic() || Actor->bRouteBeginPlayEvenIfStatic )
				{
					Actor->PostBeginPlay();
#if WITH_FACEFX
					APawn* ActorPawn = Cast<APawn>(Actor);
					ASkeletalMeshActor *ActorSkelMesh = Cast<ASkeletalMeshActor>(Actor);
					if(ActorPawn || ActorSkelMesh)
					{
						GWorld->MountPersistentFaceFXAnimSetOnActor(Actor);
					}				
#endif	//#if WITH_FACEFX
				}
				// Set script initialized if we skip routing begin play as some code relies on it.
				else
				{
					Actor->bScriptInitialized = TRUE;
				}
			}
		}
	}
}

UBOOL ULevel::HasAnyActorsOfType(UClass *SearchType)
{
	// just search the actors array
	for (INT Idx = 0; Idx < Actors.Num(); Idx++)
	{
		AActor *Actor = Actors(Idx);
		// if valid, not pending kill, and
		// of the correct type
		if (Actor != NULL &&
			!Actor->IsPendingKill() &&
			Actor->IsA(SearchType))
		{
			return TRUE;
		}
	}
	return FALSE;
}

UBOOL ULevel::HasPathNodes()
{
	// if this is the editor
	if (GIsEditor)
	{
		// check the actor list, as paths may not be rebuilt
		return HasAnyActorsOfType(ANavigationPoint::StaticClass());
	}
	else
	{
		// otherwise check the nav list pointers
		return (NavListStart != NULL && NavListEnd != NULL);
	}
}

//debug
//pathdebug
#if 0 && !PS3 && !FINAL_RELEASE
#define CHECKNAVLIST(b, x, n) \
		if( !GIsEditor && ##b ) \
		{ \
			debugf(*##x); \
			for (ANavigationPoint *T = GWorld->GetFirstNavigationPoint(); T != NULL; T = T->nextNavigationPoint) \
			{ \
				T->ClearForPathFinding(); \
			} \
			UWorld::VerifyNavList(*##x, ##n); \
		}
#else
#define CHECKNAVLIST(b, x, n)
#endif

void ULevel::AddToNavList( ANavigationPoint *Nav, UBOOL bDebugNavList )
{
	if (Nav != NULL)
	{
		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT("ADD %s to nav list %s"), *Nav->GetFullName(), *GetFullName()), Nav );

		UBOOL bNewList = FALSE;

		// if the list is currently invalid,
		if (NavListStart == NULL || NavListEnd == NULL)
		{
			// set the new nav as the start/end of the list
			NavListStart = Nav;
			NavListEnd = Nav;
			Nav->nextNavigationPoint = NULL;
			bNewList = TRUE;
		}
		else
		{
			// otherwise insert the nav at the end
			ANavigationPoint* Next = NavListEnd->nextNavigationPoint;
			NavListEnd->nextNavigationPoint = Nav;
			NavListEnd = Nav;
			Nav->nextNavigationPoint = Next;
		}
		// add to the cover list as well
		ACoverLink *Link = Cast<ACoverLink>(Nav);
		if (Link != NULL)
		{
			if (CoverListStart == NULL || CoverListEnd == NULL)
			{
				CoverListStart = Link;
				CoverListEnd = Link;
				Link->NextCoverLink = NULL;
			}
			else
			{
				ACoverLink* Next = CoverListEnd->NextCoverLink;
				CoverListEnd->NextCoverLink = Link;
				CoverListEnd = Link;
				Link->NextCoverLink = Next;
			}
		}
		APylon* Pylon = Cast<APylon>(Nav);
		if( Pylon != NULL )
		{
			if( PylonListStart == NULL || PylonListEnd == NULL )
			{
				PylonListStart = Pylon;
				PylonListEnd = Pylon;
				Pylon->NextPylon = NULL;
			}
			else
			{
				APylon* Next = PylonListEnd->NextPylon;
				PylonListEnd->NextPylon = Pylon;
				PylonListEnd = Pylon;
				Pylon->NextPylon = Next;
			}
		}

		if (bNewList && GIsGame)
		{
			GWorld->AddLevelNavList(this,bDebugNavList);
			debugfSuppressed(NAME_DevPath, TEXT(">>>  ADDED %s to world nav list because of %s"), *GetFullName(), *Nav->GetFullName());
		}

		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT(">>> ADDED %s to nav list"), *Nav->GetFullName()), Nav );
	}
}

void ULevel::RemoveFromNavList( ANavigationPoint *Nav, UBOOL bDebugNavList )
{
	if( GIsEditor && !GIsGame )
	{
		// skip if in the editor since this shouldn't be reliably used (build paths only)
		return;
	}

	if (Nav != NULL)
	{
		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT("REMOVE %s from nav list"), *Nav->GetFullName()), Nav );

		AWorldInfo *Info = GWorld->GetWorldInfo();

		// navigation point
		{
			// this is the nav that was pointing to this nav in the linked list
			ANavigationPoint *PrevNav = NULL;

			// remove from the world list
			// first check to see if this is the head of the world nav list
			if (Info->NavigationPointList == Nav)
			{
				// adjust to the next
				Info->NavigationPointList = Nav->nextNavigationPoint;
			}
			else
			{
				// otherwise hunt through the list for it
				for (ANavigationPoint *ChkNav = Info->NavigationPointList; ChkNav != NULL; ChkNav = ChkNav->nextNavigationPoint)
				{
					if (ChkNav->nextNavigationPoint == Nav)
					{
						// remove from the list
						PrevNav = ChkNav;
						ChkNav->nextNavigationPoint = Nav->nextNavigationPoint;
						break;
					}
				}
			}

			// check to see if it was the head of the level list
			if (Nav == NavListStart)
			{
				NavListStart = Nav->nextNavigationPoint;
			}

			// check to see if it was the end of the level list
			if (Nav == NavListEnd)
			{
				// if the previous nav is in this level
				if (PrevNav != NULL &&
					PrevNav->GetLevel() == this)
				{
					// then set the end to that
					NavListEnd = PrevNav;
				}
				// otherwise null the end
				else
				{
					NavListEnd = NULL;
				}
			}
		}

		// update the cover list as well (MIRROR NavList* update!)
		ACoverLink *Link = Cast<ACoverLink>(Nav);
		if (Link != NULL)
		{
			// this is the nav that was pointing to this nav in the linked list
			ACoverLink *PrevLink = NULL;

			// remove from the world list
			// first check to see if this is the head of the world nav list
			if (Info->CoverList == Link)
			{
				// adjust to the next
				Info->CoverList = Link->NextCoverLink;
			}
			else
			{
				// otherwise hunt through the list for it
				for (ACoverLink *ChkLink = Info->CoverList; ChkLink != NULL; ChkLink = ChkLink->NextCoverLink)
				{
					if (ChkLink->NextCoverLink == Link)
					{
						// remove from the list
						PrevLink = ChkLink;
						ChkLink->NextCoverLink = Link->NextCoverLink;
						break;
					}
				}
			}

			// check to see if it was the head of the level list
			if (Link == CoverListStart)
			{
				CoverListStart = Link->NextCoverLink;
			}

			// check to see if it was the end of the level list
			if (Link == CoverListEnd)
			{
				// if the previous nav is in this level
				if (PrevLink != NULL &&
					PrevLink->GetLevel() == this)
				{
					// then set the end to that
					CoverListEnd = PrevLink;
				}
				// otherwise null the end
				else
				{
					CoverListEnd = NULL;
				}
			}
		}

		// update the Pylon list as well (MIRROR NavList* update!)
		APylon *Pylon = Cast<APylon>(Nav);
		if (Pylon != NULL)
		{
			// this is the nav that was pointing to this nav in the linked list
			APylon *PrevPylon = NULL;

			// remove from the world list
			// first check to see if this is the head of the world nav list
			if (Info->PylonList == Pylon)
			{
				// adjust to the next
				Info->PylonList = Pylon->NextPylon;
			}
			else
			{
				// otherwise hunt through the list for it
				for( APylon *Chk = Info->PylonList; Chk != NULL; Chk = Chk->NextPylon )
				{
					if( Chk->NextPylon == Pylon )
					{
						// remove from the list
						PrevPylon = Chk;
						Chk->NextPylon = Pylon->NextPylon;
						break;
					}
				}
			}

			// check to see if it was the head of the level list
			if( Pylon == PylonListStart )
			{
				PylonListStart = Pylon->NextPylon;
			}

			// check to see if it was the end of the level list
			if( Pylon == PylonListEnd )
			{
				// if the previous nav is in this level
				if( PrevPylon != NULL &&
					PrevPylon->GetLevel() == this )
				{
					// then set the end to that
					PylonListEnd = PrevPylon;
				}
				// otherwise null the end
				else
				{
					PylonListEnd = NULL;
				}
			}
		}

		CHECKNAVLIST(bDebugNavList, FString::Printf(TEXT(">>> REMOVED %s from nav list"), *Nav->GetFullName()), Nav );
	}
}

#undef CHECKNAVLIST

void ULevel::ResetNavList()
{
	NavListStart = NULL;
	NavListEnd = NULL;
	CoverListStart = NULL;
	CoverListEnd = NULL;
	PylonListStart = NULL;
	PylonListEnd = NULL;
}

/** finds all Material references pointing to the specified material relevant to material parameter modifiers (Matinee tracks, etc)
 * in this level and adds entries to the arrays in the structure
 */
void ULevel::GetMaterialRefs(FMaterialReferenceList& ReferenceInfo, UBOOL bFindPostProcessRefsOnly/*= FALSE*/)
{
	if (!bFindPostProcessRefsOnly)
	{
		for (INT i = 0; i < Actors.Num(); i++)
		{
			AActor* Actor = Actors(i);
			if (Actor != NULL && !Actor->ActorIsPendingKill())
			{
				for (INT j = 0; j < Actor->AllComponents.Num(); j++)
				{
					UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Actor->AllComponents(j));
					if (Primitive != NULL)
					{
						INT Num = Primitive->GetNumElements();
						for (INT k = 0; k < Num; k++)
						{
							UMaterialInterface* Material = Primitive->GetElementMaterial(k);
							// check the material directly, but also check if a MIC has already been generated at runtime to replace the desired material
							if ( Material == ReferenceInfo.TargetMaterial ||
								( Material != NULL && Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()) &&
									((UMaterialInstanceConstant*)Material)->Parent == ReferenceInfo.TargetMaterial ) )
							{
								new(ReferenceInfo.AffectedMaterialRefs) FPrimitiveMaterialRef(Primitive, k);
							}
						}
					}
				}
			}
		}
	}

	if (GIsGame)
	{
		for (INT PlayerIndex = 0; PlayerIndex < GEngine->GamePlayers.Num(); ++PlayerIndex)
		{
			ULocalPlayer* Player = GEngine->GamePlayers(PlayerIndex);
			if (Player && Player->PlayerPostProcess)
			{
				for (INT EffectIdx = 0; EffectIdx < Player->PlayerPostProcess->Effects.Num(); ++EffectIdx)
				{
					UMaterialEffect* MaterialEffect = Cast<UMaterialEffect>(Player->PlayerPostProcess->Effects(EffectIdx));

					if (MaterialEffect && MaterialEffect->Material)
					{
						UMaterialInterface* Material = MaterialEffect->Material;
						if ( Material == ReferenceInfo.TargetMaterial ||
							( Material != NULL && Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()) &&
							((UMaterialInstanceConstant*)Material)->Parent == ReferenceInfo.TargetMaterial ) )
						{
							new(ReferenceInfo.AffectedPPChainMaterialRefs) FPostProcessMaterialRef(MaterialEffect);
						}
					}
				}
			}
		}
	}
	else if (GIsEditor && !GIsGame)
	{
		UPostProcessChain* WorldPostProcessChain = GEngine->GetWorldPostProcessChain();
		if (WorldPostProcessChain)
		{
			for (INT EffectIdx = 0; EffectIdx < WorldPostProcessChain->Effects.Num(); ++EffectIdx)
			{
				UMaterialEffect* MaterialEffect = Cast<UMaterialEffect>(WorldPostProcessChain->Effects(EffectIdx));

				if (MaterialEffect && MaterialEffect->Material)
				{
					UMaterialInterface* Material = MaterialEffect->Material;
					if ( Material == ReferenceInfo.TargetMaterial ||
						( Material != NULL && Material->GetNetIndex() == INDEX_NONE && !Material->HasAnyFlags(RF_Standalone) && Material->IsA(UMaterialInstanceConstant::StaticClass()) &&
						((UMaterialInstanceConstant*)Material)->Parent == ReferenceInfo.TargetMaterial ) )
					{
						new(ReferenceInfo.AffectedPPChainMaterialRefs) FPostProcessMaterialRef(MaterialEffect);
					}
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	ULineBatchComponent implementation.
-----------------------------------------------------------------------------*/

/** Represents a LineBatchComponent to the scene manager. */
class FLineBatcherSceneProxy : public FPrimitiveSceneProxy
{
 public:
	FLineBatcherSceneProxy(const ULineBatchComponent* InComponent):
		FPrimitiveSceneProxy(InComponent), Lines(InComponent->BatchedLines), Points(InComponent->BatchedPoints)
	{
		ViewRelevance.bDynamicRelevance = TRUE;
		for(INT LineIndex = 0;LineIndex < Lines.Num();LineIndex++)
		{
			const ULineBatchComponent::FLine& Line = Lines(LineIndex);
			ViewRelevance.SetDPG(Line.DepthPriority,TRUE);
		}

		for(INT PointIndex = 0;PointIndex < Points.Num();PointIndex++)
		{
			const ULineBatchComponent::FPoint& Point = Points(PointIndex);
			ViewRelevance.SetDPG(Point.DepthPriority,TRUE);
		}
	}

	/** 
	 * Draw the scene proxy as a dynamic element
	 *
	 * @param	PDI - draw interface to render to
	 * @param	View - current view
	 * @param	DPGIndex - current depth priority 
	 * @param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		for (INT i = 0; i < Lines.Num(); i++)
		{
			PDI->DrawLine(Lines(i).Start, Lines(i).End, Lines(i).Color, Lines(i).DepthPriority, Lines(i).Thickness);
		}

		for (INT i = 0; i < Points.Num(); i++)
		{
			PDI->DrawPoint(Points(i).Position, Points(i).Color, Points(i).PointSize, Points(i).DepthPriority);
		}
	}

	/**
	 *  Returns a struct that describes to the renderer when to draw this proxy.
	 *	@param		Scene view to use to determine our relevence.
	 *  @return		View relevance struct
	 */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		return ViewRelevance;
	}
	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize() ); }

 private:
	 TArray<ULineBatchComponent::FLine> Lines;
	 TArray<ULineBatchComponent::FPoint> Points;
	 FPrimitiveViewRelevance ViewRelevance;
};

void ULineBatchComponent::DrawLine(const FVector& Start,const FVector& End,const FLinearColor& Color,BYTE DepthPriority,const FLOAT Thickness)
{
	new(BatchedLines) FLine(Start,End,Color,DefaultLifeTime,Thickness,DepthPriority);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

/** Provide many lines to draw - faster than calling DrawLine many times. */
void ULineBatchComponent::DrawLines(const TArray<FLine>& InLines)
{
	BatchedLines.Append(InLines);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

void ULineBatchComponent::DrawPoint(
	const FVector& Position,
	const FLinearColor& Color,
	FLOAT PointSize,
	BYTE DepthPriority
	)
{
	new(BatchedPoints) FPoint(Position,Color,PointSize,DepthPriority);
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

/** Draw a box. */
void ULineBatchComponent::DrawBox(const FBox& Box, const FMatrix& TM, const FColor& Color, BYTE DepthPriorityGroup)
{
	FVector	B[2],P,Q;
	INT ai,aj;
	const FMatrix& L2W = TM;
	B[0]=Box.Min;
	B[1]=Box.Max;

	for( ai=0; ai<2; ai++ ) for( aj=0; aj<2; aj++ )
	{
		P.X=B[ai].X; Q.X=B[ai].X;
		P.Y=B[aj].Y; Q.Y=B[aj].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		new(BatchedLines) FLine(TM.TransformFVector(P), TM.TransformFVector(Q), Color, DefaultLifeTime, 0.0f, DepthPriorityGroup);

		P.Y=B[ai].Y; Q.Y=B[ai].Y;
		P.Z=B[aj].Z; Q.Z=B[aj].Z;
		P.X=B[0].X; Q.X=B[1].X;
		new(BatchedLines) FLine(TM.TransformFVector(P), TM.TransformFVector(Q), Color, DefaultLifeTime, 0.0f, DepthPriorityGroup);

		P.Z=B[ai].Z; Q.Z=B[ai].Z;
		P.X=B[aj].X; Q.X=B[aj].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		new(BatchedLines) FLine(TM.TransformFVector(P), TM.TransformFVector(Q), Color, DefaultLifeTime, 0.0f, DepthPriorityGroup);
	}
	// LineBatcher and PersistentLineBatcher components will be updated at the end of UWorld::Tick
	bNeedsReattach = TRUE;
}

void ULineBatchComponent::Tick(FLOAT DeltaTime)
{
	// Update the life time of batched lines, removing the lines which have expired.
	for(INT LineIndex = 0;LineIndex < BatchedLines.Num();LineIndex++)
	{
		FLine& Line = BatchedLines(LineIndex);
		if(Line.RemainingLifeTime > 0.0f)
		{
			Line.RemainingLifeTime -= DeltaTime;
			if(Line.RemainingLifeTime <= 0.0f)
			{
				// The line has expired, remove it.
				BatchedLines.Remove(LineIndex--);
			}
		}
	}
}

/**
 * Creates a new scene proxy for the line batcher component.
 * @return	Pointer to the FLineBatcherSceneProxy
 */
FPrimitiveSceneProxy* ULineBatchComponent::CreateSceneProxy()
{
	return new FLineBatcherSceneProxy(this);
}


#if BATMAN

/*-----------------------------------------------------------------------------
	BM2: Climbable edge collection building.
-----------------------------------------------------------------------------*/

INT FEdgeCollection::GetNumEdges() const
{
	return Edges.Num();
}

UBOOL FEdgeCollection::GetEdge( INT Index, const UModel* Model, FVector& OutPointA, FVector& OutPointB ) const
{
	if( Index >= Edges.Num() )
	{
		return FALSE;
	}

	const FHorizontalEdge& Edge = Edges(Index);
	if( Edge.VertexA >= Model->Points.Num() || Edge.VertexB >= Model->Points.Num() )
	{
		return FALSE;
	}

	OutPointA = Model->Points(Edge.VertexA);
	OutPointB = Model->Points(Edge.VertexB);
	return TRUE;
}

INT FActorEdgeCollection::GetNumEdges() const
{
	return Edges.Num();
}

UBOOL FActorEdgeCollection::GetEdge( INT Index, const UModel* Model, FVector& OutPointA, FVector& OutPointB ) const
{
	if( Index >= Edges.Num() )
	{
		return FALSE;
	}

	const FActorHorizontalEdge& Edge = Edges(Index);
	OutPointA = FVector( Edge.PointAX, Edge.PointAY, Edge.PointAZ );
	OutPointB = OutPointA + FVector( Edge.PointBX, Edge.PointBY, Edge.PointBZ );
	return TRUE;
}

void FActorEdgeCollection::AddEdge( const FActorHorizontalEdge& Edge )
{
	Edges.AddItem( Edge );

	const FVector PointA( Edge.PointAX, Edge.PointAY, Edge.PointAZ );
	BoundingBox += PointA;
	BoundingBox += PointA + FVector( Edge.PointBX, Edge.PointBY, Edge.PointBZ );
}

/** BM2: Trace flags every climbable-edge probe uses. */
#define EDGE_TRACE_FLAGS		(TRACE_Level | TRACE_Others | TRACE_Blocking | TRACE_LevelGeometry)

// BM: Retail passes 0xA002CC6 when testing actor collision here. Bits 0x2000000 and
// 0x8000000 are BM2-only trace flags we have not identified yet.
#define EDGE_ACTOR_TRACE_FLAGS	( TRACE_Movers | TRACE_Level | TRACE_Blocking | TRACE_LevelGeometry \
								| TRACE_SingleResult | TRACE_Material | TRACE_Terrain | 0x2000000 | 0x8000000 )

/** BM2: A stretch of an edge still waiting to be probed. */
struct FRemainingEdgeSection
{
	FVector PointA;
	FVector PointB;
};

/** BM2: Levels match outright, or are streaming levels differing only by an _LOD suffix. */
static UBOOL AreEdgeLevelsRelated( ULevel* Level, ULevel* HitLevel )
{
	if( Level == HitLevel )
	{
		return TRUE;
	}

	ULevelStreaming* StreamingA = FLevelUtils::FindStreamingLevel( Level );
	ULevelStreaming* StreamingB = FLevelUtils::FindStreamingLevel( HitLevel );
	if( !StreamingA || !StreamingB )
	{
		return FALSE;
	}

	FString NameA = StreamingA->PackageName.ToString();
	FString NameB = StreamingB->PackageName.ToString();

	const INT LodA = NameA.InStr( TEXT("_LOD") );
	const INT LodB = NameB.InStr( TEXT("_LOD") );
	if( LodA != INDEX_NONE )
	{
		NameA = NameA.Left( LodA );
	}
	if( LodB != INDEX_NONE )
	{
		NameB = NameB.Left( LodB );
	}

	return NameA == NameB;
}

/** BM2: Whether a probe hit counts as geometry obstructing the edge. */
static UBOOL DoesHitBlockEdge( const FCheckResult* Hit, AActor* Actor, ULevel* Level, UBOOL bRailingTop )
{
	AActor* HitActor = Hit->Actor;

	UClass* NoClimbVolumeClass = FindObject<UClass>( ANY_PACKAGE, TEXT("RNoClimbVolume") );
	if( !NoClimbVolumeClass )
	{
		return TRUE;
	}

	if( HitActor->IsA( NoClimbVolumeClass ) )
	{
		return TRUE;
	}

	AStaticMeshActorBase* MeshActor = Cast<AStaticMeshActorBase>( HitActor );
	if( MeshActor
	&&	(	( !bRailingTop && HitActor != Actor && (MeshActor->bRailing || MeshActor->bForceAllowKismetModification) )
		||	( MeshActor->bGrappleToSlopedRoof && Actor == HitActor ) ) )
	{
		return FALSE;
	}

	if( HitActor == Actor )
	{
		return TRUE;
	}

	if( HitActor->bMovable )
	{
		return FALSE;
	}

	if( HitActor->IsA( AFracturedStaticMeshActor::StaticClass() ) )
	{
		return FALSE;
	}

	ULevel* HitLevel = Hit->Level ? Hit->Level : Cast<ULevel>( HitActor->GetOuter() );
	return AreEdgeLevelsRelated( Level, HitLevel );
}

/** BM2: First hit at this point that obstructs the edge, or NULL if the space is clear. */
static FCheckResult* FirstBlockingPointHit( const FVector& Point, const FVector& Extent, AActor* Actor, ULevel* Level, UBOOL bRailingTop )
{
	FCheckResult* Hit = GWorld->MultiPointCheck( GMainThreadMemStack, Point, Extent, EDGE_TRACE_FLAGS, APawn::StaticClass()->GetDefaultActor() );
	for( ; Hit; Hit = Hit->GetNext() )
	{
		if( DoesHitBlockEdge( Hit, Actor, Level, bRailingTop ) )
		{
			return Hit;
		}
	}
	return NULL;
}

/** BM2: First hit along this line that obstructs the edge, or NULL if the sweep is clear. */
static FCheckResult* FirstBlockingLineHit( const FVector& Start, const FVector& End, const FVector& Extent, AActor* Actor, ULevel* Level, UBOOL bRailingTop )
{
	FCheckResult* Hit = GWorld->MultiLineCheck( GMainThreadMemStack, End, Start, Extent, EDGE_TRACE_FLAGS, APawn::StaticClass()->GetDefaultActor() );
	for( ; Hit; Hit = Hit->GetNext() )
	{
		if( DoesHitBlockEdge( Hit, Actor, Level, bRailingTop ) )
		{
			return Hit;
		}
	}
	return NULL;
}

/**
 * BM2: Probes the space around a candidate ledge and appends whatever parts of it a pawn
 * could actually hang from. Blocked stretches are re-queued and retried at finer resolution.
 */
static UBOOL AddToEdgeCollection( FActorEdgeCollection& Collection, FVector PointA, FVector PointB,
								  const FVector& InwardRef, ULevel* Level, AActor* Actor,
								  UBOOL bSlopedEdge, UBOOL bSpikeyRailing, UBOOL bAllowCrevice,
								  UBOOL bWideRailing, UBOOL bRailingTop )
{
	FVector Dir = PointB - PointA;
	Dir.Normalize();

	FVector Outward( -Dir.Y, Dir.X, 0.f );
	Outward.Normalize();

	// Keep the outward direction pointing away from the solid side of the edge.
	if( ((InwardRef - PointA) | Outward) > 0.f )
	{
		Outward = -Outward;
		Dir = -Dir;
		Exchange( PointA, PointB );
	}

	const FLOAT AbsDirZ = Abs( Dir.Z );
	AStaticMeshActorBase* MeshActor = Cast<AStaticMeshActorBase>( Actor );

	UBOOL bNoSplit = bRailingTop;
	if( AbsDirZ > appSin( 0.3490658402442932f )
	||	( Actor && Actor->bDisallowShimmy )
	||	( MeshActor && (MeshActor->bRailing || MeshActor->bUseBoundingBoxForClimbing) ) )
	{
		bNoSplit = TRUE;
	}

	FMemMark Mark( GMainThreadMemStack );

	// Diagonal edges need their probes padded out; this is the 2D L1 length of the outward dir.
	const FLOAT OutwardSpread = Abs(Outward.X) + Abs(Outward.Y);

	const FLOAT NearDist = OutwardSpread * 16.f + 4.f;
	const FLOAT FarDist  = OutwardSpread * 16.f + 24.f;
	const FVector NearOffset( NearDist * Outward.X, NearDist * Outward.Y, NearDist * Outward.Z - 30.f );
	const FVector FarOffset ( FarDist  * Outward.X, FarDist  * Outward.Y, FarDist  * Outward.Z - 50.f );

	TArray<FRemainingEdgeSection,TMemStackAllocator<GMainThreadMemStack> > Sections;
	{
		FRemainingEdgeSection& First = Sections( Sections.Add(1) );
		First.PointA = PointA;
		First.PointB = PointB;
	}

	for( INT i = 0; i < (bNoSplit ? 1 : Sections.Num()); i++ )
	{
		const FVector SecA = Sections(i).PointA;
		const FVector SecB = Sections(i).PointB;
		const FLOAT SectionLen = (SecB - SecA).Size();

		// The first pass sweeps coarsely; re-queued stretches get a finer one.
		const FLOAT Step = ( i <= 0 ) ? 40.f : 10.f;
		const FLOAT ZOffset = AbsDirZ * Step * 2.f + 28.f;

		const FVector StartPoint( SecA.X, SecA.Y, SecA.Z + ZOffset );
		const FVector EndPoint  ( SecB.X, SecB.Y, SecB.Z + ZOffset );
		const FVector InwardOffset( -Step * Outward.X, -Step * Outward.Y, -Step * Outward.Z + Step );

		const FVector ExtentBody( 16.f, 16.f, 16.f );
		const FVector ExtentStep( Step, Step, 16.f );
		const FVector ExtentFar ( 16.f, 16.f, 30.f );
		const FVector ExtentNear( 16.f, 16.f, 8.f );

		if( i > 0 && SectionLen < 64.f )
		{
			continue;
		}

		const FLOAT Pad = Step * OutwardSpread;

		if( Pad * 2.f >= SectionLen )
		{
			// Too short to walk - one probe at the midpoint decides the whole section.
			const FVector Mid = (StartPoint + EndPoint) * 0.5f;

			FMemMark ProbeMark( GMainThreadMemStack );
			const UBOOL bClear =
				!FirstBlockingPointHit( Mid,                ExtentBody, Actor, Level, bRailingTop )
			&&	!FirstBlockingPointHit( Mid + InwardOffset, ExtentStep, Actor, Level, bRailingTop )
			&&	!FirstBlockingPointHit( Mid + FarOffset,    ExtentFar,  Actor, Level, bRailingTop )
			&&	!FirstBlockingPointHit( Mid + NearOffset,   ExtentNear, Actor, Level, bRailingTop );
			ProbeMark.Pop();

			if( bClear )
			{
				FActorHorizontalEdge Edge;
				Edge.PointAX = SecA.X;
				Edge.PointAY = SecA.Y;
				Edge.PointAZ = SecA.Z;
				Edge.PointBX = appTrunc( SecB.X - SecA.X );
				Edge.PointBY = appTrunc( SecB.Y - SecA.Y );
				Edge.PointBZ = appTrunc( SecB.Z - SecA.Z );

				BYTE EdgeType = 0;
				if( bSlopedEdge )								EdgeType |= EDGETYPE_SlopedEdge;
				if( bNoSplit )									EdgeType |= EDGETYPE_ExtraSloped;
				if( bSpikeyRailing )							EdgeType |= EDGETYPE_SpikeyRailing;
				if( bWideRailing )								EdgeType |= EDGETYPE_WideRailing;
				if( bRailingTop || (i > 0 && !bAllowCrevice) )	EdgeType |= EDGETYPE_ShimmyOnly;
				if( Actor && Actor->bGrappleToSlopedRoof )		EdgeType |= EDGETYPE_SpecialRoofEdge;
				Edge.EdgeType = EdgeType;

				Collection.AddEdge( Edge );
			}
			else if( i == 0 )
			{
				// Re-queue so the finer pass gets a chance at it.
				const FRemainingEdgeSection Whole = Sections(0);
				Sections.AddItem( Whole );
			}
			continue;
		}

		const FLOAT EndT = SectionLen - Pad;
		FLOAT T = Pad;
		FLOAT SegStart = Pad;
		FLOAT LastExtent = Step;
		UBOOL bEmittedHere = FALSE;

		while( T < EndT )
		{
			FMemMark ProbeMark( GMainThreadMemStack );

			const FVector Probe = StartPoint + Dir * T;
			UBOOL bClear = TRUE;
			bEmittedHere = FALSE;

			if( FirstBlockingPointHit( Probe, ExtentBody, Actor, Level, bRailingTop ) )
			{
				bClear = FALSE;
				LastExtent = 16.f;
			}
			else if( FirstBlockingPointHit( Probe + InwardOffset, ExtentStep, Actor, Level, bRailingTop ) )
			{
				bClear = FALSE;
				LastExtent = Step;
			}
			else if( FirstBlockingPointHit( Probe + FarOffset, ExtentFar, Actor, Level, bRailingTop ) )
			{
				bClear = FALSE;
				LastExtent = 16.f;
			}
			else if( FirstBlockingPointHit( Probe + NearOffset, ExtentNear, Actor, Level, bRailingTop ) )
			{
				bClear = FALSE;
				LastExtent = 16.f;
			}
			ProbeMark.Pop();

			if( bClear )
			{
				// Anything walked over since the last emission was blocked - retry it finer.
				if( T > SegStart && i == 0 )
				{
					const FVector Origin = Sections(0).PointA;
					FRemainingEdgeSection& Gap = Sections( Sections.Add(1) );
					Gap.PointA = Origin + Dir * (SegStart - LastExtent * OutwardSpread);
					Gap.PointB = Origin + Dir * (T        - LastExtent * OutwardSpread);
				}

				FMemMark SweepMark( GMainThreadMemStack );

				const FVector LineEnd = StartPoint + Dir * EndT;
				const FLOAT Scale = OutwardSpread / (LineEnd - Probe).Size();

				// Sweep ahead along the edge to find how far this clear run reaches.
				FLOAT ClearTime = 1.f;
				FCheckResult* Hit = FirstBlockingLineHit( Probe, LineEnd, ExtentBody, Actor, Level, bRailingTop );
				if( Hit )
				{
					ClearTime = (16.f - Step) * Scale + Hit->Time;
				}
				if( ClearTime > 0.f )
				{
					Hit = FirstBlockingLineHit( Probe + InwardOffset, LineEnd + InwardOffset, ExtentStep, Actor, Level, bRailingTop );
					if( Hit )
					{
						ClearTime = Min( ClearTime, Hit->Time );
					}
				}
				if( ClearTime > 0.f )
				{
					Hit = FirstBlockingLineHit( Probe + FarOffset, LineEnd + FarOffset, ExtentFar, Actor, Level, bRailingTop );
					if( Hit )
					{
						ClearTime = Min( ClearTime, (16.f - Step) * Scale + Hit->Time );
					}
				}
				if( ClearTime > 0.f )
				{
					Hit = FirstBlockingLineHit( Probe + NearOffset, LineEnd + NearOffset, ExtentNear, Actor, Level, bRailingTop );
					if( Hit )
					{
						ClearTime = Min( ClearTime, (16.f - Step) * Scale + Hit->Time );
					}
				}
				SweepMark.Pop();

				const FLOAT RunLength = Scale * Step + ClearTime;
				if( RunLength > 0.f )
				{
					const FLOAT RunStart = T - LastExtent * OutwardSpread;
					const FLOAT RunEnd   = T + (EndT - T) * RunLength;

					const FVector EdgeA = SecA + Dir * RunStart;
					const FVector EdgeB = SecA + Dir * RunEnd;

					FActorHorizontalEdge Edge;
					Edge.PointAX = EdgeA.X;
					Edge.PointAY = EdgeA.Y;
					Edge.PointAZ = EdgeA.Z;
					Edge.PointBX = appTrunc( EdgeB.X - EdgeA.X );
					Edge.PointBY = appTrunc( EdgeB.Y - EdgeA.Y );
					Edge.PointBZ = appTrunc( EdgeB.Z - EdgeA.Z );

					BYTE EdgeType = 0;
					if( bSlopedEdge )							EdgeType |= EDGETYPE_SlopedEdge;
					if( bNoSplit )								EdgeType |= EDGETYPE_ExtraSloped;
					if( bSpikeyRailing )						EdgeType |= EDGETYPE_SpikeyRailing;
					if( bWideRailing )							EdgeType |= EDGETYPE_WideRailing;
					if( i > 0 && !bAllowCrevice )				EdgeType |= EDGETYPE_ShimmyOnly;
					if( Actor && Actor->bGrappleToSlopedRoof )	EdgeType |= EDGETYPE_SpecialRoofEdge;
					Edge.EdgeType = EdgeType;

					Collection.AddEdge( Edge );

					SegStart = Pad + RunEnd;
					T = RunEnd;
					bEmittedHere = TRUE;
				}
			}

			T += 8.f;
		}

		// The tail past the last emission never got probed - hand it to the finer pass.
		if( i == 0 && !bEmittedHere )
		{
			const FRemainingEdgeSection First = Sections(0);
			FRemainingEdgeSection& Tail = Sections( Sections.Add(1) );
			Tail.PointA = First.PointA + Dir * (SegStart - Pad);
			Tail.PointB = First.PointB;
		}
	}

	Mark.Pop();
	return TRUE;
}

/**
 * BM2: Collapses pairs of collinear edges that share a vertex and face the same way
 * into a single edge spanning both.
 */
void FEdgeCollection::OptimizeEdgeCollection( UModel* Model )
{
	for( INT i = GetNumEdges() - 1; i > 0; i-- )
	{
		FHorizontalEdge& EdgeI = Edges(i);
		const INT VertA = EdgeI.VertexA;
		const INT VertB = EdgeI.VertexB;

		FVector DirI = Model->Points(VertB) - Model->Points(VertA);
		DirI.Normalize();

		for( INT j = i - 1; j >= 0; j-- )
		{
			FHorizontalEdge& EdgeJ = Edges(j);
			const INT OtherA = EdgeJ.VertexA;
			const INT OtherB = EdgeJ.VertexB;

			// Only edges sharing an endpoint with this one can be merged.
			if( OtherA != VertA && OtherA != VertB && OtherB != VertA && OtherB != VertB )
			{
				continue;
			}

			FVector DirJ = Model->Points(OtherB) - Model->Points(OtherA);
			DirJ.Normalize();

			if( Abs( DirI | DirJ ) <= 0.99f )
			{
				continue;
			}

			// Both outward directions must fall on the same side of the shared line.
			const FLOAT SideJ = EdgeJ.OutwardDir.Y * DirJ.X - EdgeJ.OutwardDir.X * DirJ.Y;
			const FLOAT SideI = EdgeI.OutwardDir.Y * DirJ.X - EdgeI.OutwardDir.X * DirJ.Y;
			if( SideJ * SideI <= 0.f )
			{
				continue;
			}

			// Span the two endpoints that aren't shared.
			INT NewB = VertA;
			INT NewA = OtherA;
			if( OtherA == VertA )
			{
				NewB = VertB;
				NewA = OtherB;
			}
			else if( OtherA == VertB )
			{
				NewA = OtherB;
			}
			else if( OtherB == VertA )
			{
				NewB = VertB;
			}

			EdgeI.VertexA = NewA;
			EdgeI.VertexB = NewB;
			Edges.Remove( j, 1 );
			break;
		}
	}
}

/**
 * BM2: Feeds an edge to the collection if it is long enough and flat enough to hang from.
 * Lenient actors accept anything up to their slope limit; everything else must be near level.
 */
static void AddEdgeIfClimbable( FActorEdgeCollection& Collection, const FVector& A, const FVector& B,
								const FVector& InwardRef, ULevel* Level, AActor* Actor,
								FLOAT SlopeLimit, UBOOL bLenient,
								UBOOL bSpikeyRailing, UBOOL bWideRailing )
{
	const FLOAT Len2DSq = Square(B.X - A.X) + Square(B.Y - A.Y);
	const FLOAT Rise = Abs( B.Z - A.Z );
	const FLOAT Slope = Rise / appSqrt( Len2DSq );

	if( Len2DSq > 1024.f
	&&	( (Rise < 10.f && Slope < 0.005f) || (bLenient && SlopeLimit > Slope) ) )
	{
		AddToEdgeCollection( Collection, A, B, InwardRef, Level, Actor,
			Slope > 0.05f, bSpikeyRailing, Actor->bAllowCrevice, bWideRailing, FALSE );
	}
}

/**
 * BM2: Scans every eligible actor's collision geometry for ledges - hull faces that point
 * up and border a wall - and stores one edge collection per primitive component.
 */
void ULevel::BuildActorEdgeCollections( TArray<FEdgeCollectionMember>& EdgeMembers )
{
	UClass* NoClimbVolumeClass = FindObject<UClass>( ANY_PACKAGE, TEXT("RNoClimbVolume") );

	for( INT ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( !Actor )
		{
			continue;
		}

		// Actors that supply their own edges bypass the geometry scan entirely.
		FActorEdgeCollection CustomCollection;
		if( Actor->CustomEdgeCollection( CustomCollection ) )
		{
			CustomCollection.BoundingBox.Min -= FVector(16.f,16.f,16.f);
			CustomCollection.BoundingBox.Max += FVector(16.f,16.f,16.f);
			const INT CollectionIndex = ActorHorizontalEdges.AddItem( CustomCollection );

			FEdgeCollectionMember Member;
			for( INT i = 0; i < Actor->Components.Num(); i++ )
			{
				UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>( Actor->Components(i) );
				if( Prim )
				{
					Prim->LevelEdgeCollectionIndex = CollectionIndex;
					Member.AssociatedMembers.AddItem( FEdgeCollectionMember::MemberContainer(Prim) );
				}
			}
			EdgeMembers.AddItem( Member );
			continue;
		}

		AActor* PawnDefault = APawn::StaticClass()->GetDefaultActor();

		// Plain BSP brushes are covered by the BSP pass; volumes still need scanning.
		const UBOOL bEligible =
			Actor->bCollideActors
		&&	Actor->bBlockActors
		&&	( !Actor->IsABrush() || Actor->IsAVolume() )
		&&	( !NoClimbVolumeClass || !Actor->IsA(NoClimbVolumeClass) )
		&&	(	!Actor->CollisionComponent
			||	(	Actor->ShouldTrace( Actor->CollisionComponent, PawnDefault, EDGE_ACTOR_TRACE_FLAGS )
				&&	PawnDefault->IsBlockedBy( Actor, Actor->CollisionComponent ) ) );

		if( !bEligible )
		{
			for( INT i = 0; i < Actor->Components.Num(); i++ )
			{
				UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>( Actor->Components(i) );
				if( Prim )
				{
					Prim->LevelEdgeCollectionIndex = 0xFFFF;
				}
			}
			continue;
		}

		UBOOL bLenient = Actor->bAllowSlopedEdges;
		FLOAT SlopeLimit = appTan( 0.3490658402442932f );
		UBOOL bSpikeyRailing = FALSE;
		UBOOL bWideRailing = FALSE;

		AStaticMeshActorBase* MeshActor = Cast<AStaticMeshActorBase>( Actor );
		if( MeshActor )
		{
			if( MeshActor->bClimbableSlopedRailing )
			{
				bLenient = TRUE;
				SlopeLimit = appTan( 0.8203047513961792f );
			}
			bSpikeyRailing = MeshActor->bSpikeyRailing;
			bWideRailing = MeshActor->bAllowWideRailings;
		}

		FKAggregateGeom BoundsGeom;
		appMemzero( &BoundsGeom, sizeof(BoundsGeom) );

		for( INT ComponentIndex = 0; ComponentIndex < Actor->Components.Num(); ComponentIndex++ )
		{
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>( Actor->Components(ComponentIndex) );
			if( !Prim )
			{
				continue;
			}

			if( !Actor->CanPreBuildClimbableEdges() || !Prim->BlockNonZeroExtent || !Prim->CollideActors )
			{
				Prim->LevelEdgeCollectionIndex = 0xFFFF;
				continue;
			}

			UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>( Prim );
			UBrushComponent* BrushComp = Cast<UBrushComponent>( Prim );

			FKAggregateGeom* Geom = NULL;
			if( MeshComp && MeshComp->StaticMesh && MeshComp->StaticMesh->BodySetup )
			{
				Geom = &MeshComp->StaticMesh->BodySetup->AggGeom;

				// Some meshes are too fiddly to hull-scan; use their bounding box instead.
				if( MeshActor && MeshActor->bUseBoundingBoxForClimbing )
				{
					const FBox Box = MeshComp->StaticMesh->Bounds.GetBox();

					BoundsGeom.EmptyElements();

					FKBoxElem BoxElem;
					appMemzero( &BoxElem, sizeof(BoxElem) );
					BoxElem.X = Box.Max.X - Box.Min.X;
					BoxElem.Y = Box.Max.Y - Box.Min.Y;
					BoxElem.Z = Box.Max.Z - Box.Min.Z;
					BoxElem.TM = FMatrix::Identity;
					BoxElem.TM.SetOrigin( (Box.Min + Box.Max) * 0.5f );
					BoundsGeom.BoxElems.AddItem( BoxElem );

					Geom = &BoundsGeom;
				}
			}
			else if( BrushComp )
			{
				Geom = &BrushComp->BrushAggGeom;
			}

			if( !Geom )
			{
				Prim->LevelEdgeCollectionIndex = 0xFFFF;
				continue;
			}

			FMatrix LocalToWorld;
			FVector Scale;
			Prim->GetTransformAndScale( LocalToWorld, Scale );

			// Normals need the inverse-transpose of the scale, not the scale itself.
			const FMatrix NormalMatrix = FScaleMatrix(Scale).Inverse().Transpose() * LocalToWorld;

			FActorEdgeCollection Collection;

			for( INT HullIndex = 0; HullIndex < Geom->ConvexElems.Num(); HullIndex++ )
			{
				const FKConvexElem& Hull = Geom->ConvexElems(HullIndex);
				const INT NumTris = Hull.FaceTriData.Num() / 3;

				for( INT TriIndex = 0; TriIndex < NumTris; TriIndex++ )
				{
					FVector Verts[3];
					for( INT Corner = 0; Corner < 3; Corner++ )
					{
						Verts[Corner] = Hull.VertexData( Hull.FaceTriData(TriIndex * 3 + Corner) );
					}

					const FVector LocalNormal = (Verts[1] - Verts[0]) ^ (Verts[2] - Verts[0]);
					for( INT Corner = 0; Corner < 3; Corner++ )
					{
						Verts[Corner] = LocalToWorld.TransformFVector( Verts[Corner] * Scale );
					}

					FVector WorldNormal = NormalMatrix.TransformNormal( LocalNormal );
					WorldNormal.Normalize();

					// Only faces you could stand on can contribute a ledge.
					if( WorldNormal.Z < 0.7f || (!bLenient && WorldNormal.Z < 0.9f) )
					{
						continue;
					}

					for( INT Side = 0; Side < 3; Side++ )
					{
						const INT NextSide = (Side + 1) % 3;
						const INT IndexA = Hull.FaceTriData(TriIndex * 3 + Side);
						const INT IndexB = Hull.FaceTriData(TriIndex * 3 + NextSide);

						// The edge is only a ledge if the triangle sharing it is a wall.
						UBOOL bBordersWall = FALSE;
						for( INT OtherTri = 0; OtherTri < NumTris && !bBordersWall; OtherTri++ )
						{
							if( OtherTri == TriIndex )
							{
								continue;
							}

							UBOOL bShares = FALSE;
							for( INT OtherSide = 0; OtherSide < 3 && !bShares; OtherSide++ )
							{
								const INT OtherA = Hull.FaceTriData(OtherTri * 3 + OtherSide);
								const INT OtherB = Hull.FaceTriData(OtherTri * 3 + (OtherSide + 1) % 3);
								bShares = (OtherA == IndexA && OtherB == IndexB)
									   || (OtherA == IndexB && OtherB == IndexA);
							}
							if( !bShares )
							{
								continue;
							}

							FVector OtherVerts[3];
							for( INT Corner = 0; Corner < 3; Corner++ )
							{
								OtherVerts[Corner] = Hull.VertexData( Hull.FaceTriData(OtherTri * 3 + Corner) );
							}

							FVector OtherNormal = NormalMatrix.TransformNormal(
								(OtherVerts[1] - OtherVerts[0]) ^ (OtherVerts[2] - OtherVerts[0]) );
							if( OtherNormal.SizeSquared() <= SMALL_NUMBER )
							{
								bBordersWall = TRUE;
								break;
							}

							OtherNormal.Normalize();
							if( OtherNormal.Z < 0.35f )
							{
								bBordersWall = TRUE;
							}
						}

						if( bBordersWall )
						{
							// The triangle's remaining corner marks the solid side of the edge.
							AddEdgeIfClimbable( Collection, Verts[Side], Verts[NextSide], Verts[(Side + 2) % 3],
								this, Actor, SlopeLimit, bLenient, bSpikeyRailing, bWideRailing );
						}
					}
				}
			}

			for( INT BoxIndex = 0; BoxIndex < Geom->BoxElems.Num(); BoxIndex++ )
			{
				const FKBoxElem& BoxElem = Geom->BoxElems(BoxIndex);

				FVector EffScale = Scale;
				if( Abs(Scale.X - Scale.Y) >= KINDA_SMALL_NUMBER || Abs(Scale.Y - Scale.Z) >= KINDA_SMALL_NUMBER )
				{
					// Non-uniform scale only works out for boxes that stayed axis aligned.
					const UBOOL bAxisAligned = BoxElem.bHasCookedAlignmentData
						? BoxElem.bIsAxisAligned
						: BoxElem.TM.IsAxisAligned( KINDA_SMALL_NUMBER );
					if( !bAxisAligned )
					{
						continue;
					}

					const FMatrix Inverse = BoxElem.TM.Inverse();
					for( INT Axis = 0; Axis < 3; Axis++ )
					{
						EffScale[Axis] = Abs( Inverse.M[0][Axis] * Scale.X
											+ Inverse.M[1][Axis] * Scale.Y
											+ Inverse.M[2][Axis] * Scale.Z );
					}
				}

				const FVector Extent( EffScale.X * 0.5f * BoxElem.X,
									  EffScale.Y * 0.5f * BoxElem.Y,
									  EffScale.Z * 0.5f * BoxElem.Z );

				FMatrix BoxToWorld = BoxElem.TM;
				BoxToWorld.SetOrigin( BoxToWorld.GetOrigin() * Scale );
				BoxToWorld = BoxToWorld * LocalToWorld;

				static const FLOAT CornerU[4] = { -1.f, -1.f,  1.f,  1.f };
				static const FLOAT CornerV[4] = { -1.f,  1.f,  1.f, -1.f };

				for( INT Face = 0; Face < 6; Face++ )
				{
					const INT AxisIndex = Face / 2;
					const INT AxisU = (AxisIndex + 1) % 3;
					const INT AxisV = (AxisIndex + 2) % 3;
					const FLOAT Sign = (Face % 2) ? -1.f : 1.f;

					FVector FaceNormal = BoxToWorld.GetAxis(AxisIndex) * Sign;
					FaceNormal.Normalize();
					if( FaceNormal.Z <= 0.7f )
					{
						continue;
					}

					const FVector UAxis = BoxToWorld.GetAxis(AxisU);
					const FVector VAxis = BoxToWorld.GetAxis(AxisV);
					const FVector FaceCenter = BoxToWorld.GetOrigin() + FaceNormal * Extent[AxisIndex];

					FVector Previous(0.f,0.f,0.f);
					for( INT Corner = 0; Corner <= 4; Corner++ )
					{
						const INT Sample = Corner & 3;
						const FVector Point = FaceCenter
							+ UAxis * Extent[AxisU] * CornerU[Sample]
							+ VAxis * Extent[AxisV] * CornerV[Sample];

						if( Corner > 0 )
						{
							AddEdgeIfClimbable( Collection, Previous, Point, FaceCenter,
								this, Actor, SlopeLimit, bLenient, bSpikeyRailing, bWideRailing );
						}
						Previous = Point;
					}
				}
			}

			if( Collection.Edges.Num() <= 0 )
			{
				Prim->LevelEdgeCollectionIndex = 0xFFFF;
			}
			else
			{
				Collection.BoundingBox.Min -= FVector(16.f,16.f,16.f);
				Collection.BoundingBox.Max += FVector(16.f,16.f,16.f);
				Prim->LevelEdgeCollectionIndex = ActorHorizontalEdges.AddItem( Collection );

				FEdgeCollectionMember Member;
				Member.AssociatedMembers.AddItem( FEdgeCollectionMember::MemberContainer(Prim) );
				EdgeMembers.AddItem( Member );
			}
		}
	}
}

/**
 * BM2: Produces one actor edge collection per BSP edge collection, so BSP-derived
 * ledges participate in the same queries as actor-derived ones.
 */
void ULevel::BuildActorEdgeFromBSPEdges( UModel* Model )
{
	const INT NumCollections = HorizontalEdges.Num();
	for( INT CollectionIndex = 0; CollectionIndex < NumCollections; CollectionIndex++ )
	{
		FActorEdgeCollection NewCollection;
		FEdgeCollection& Source = HorizontalEdges(CollectionIndex);

		FVector PointA, PointB;
		for( INT EdgeIndex = 0; Source.GetEdge( EdgeIndex, Model, PointA, PointB ); EdgeIndex++ )
		{
			const FHorizontalEdge& Edge = Source.Edges(EdgeIndex);

			// Edges with no outward direction never resolved to a climbable face.
			if( Edge.OutwardDir.X == 0.f && Edge.OutwardDir.Y == 0.f && Edge.OutwardDir.Z == 0.f )
			{
				continue;
			}

			const FLOAT Run = appSqrt( Square(PointB.X - PointA.X) + Square(PointB.Y - PointA.Y) );
			const UBOOL bSloped = ( Abs(PointB.Z - PointA.Z) / Run ) > 0.05f;

			AddToEdgeCollection( NewCollection, PointA, PointB, PointA - Edge.OutwardDir, this, NULL, bSloped, FALSE, TRUE, FALSE, FALSE );
		}

		NewCollection.BoundingBox.Min -= FVector(16.f,16.f,16.f);
		NewCollection.BoundingBox.Max += FVector(16.f,16.f,16.f);
		ActorHorizontalEdges.AddItem( NewCollection );
	}
}

/** BM2: A collection is only mergeable if nothing feeding it can move at runtime. */
static UBOOL CanMergeEdgeMembers( const TArray<FEdgeCollectionMember>& EdgeMembers, INT Index )
{
	const TArray<FEdgeCollectionMember::MemberContainer>& Members = EdgeMembers(Index).AssociatedMembers;
	for( INT i = 0; i < Members.Num(); i++ )
	{
		if( Members(i).Type != FEdgeCollectionMember::ESMTYPE_PrimComp || !Members(i).PrimComp )
		{
			continue;
		}

		AActor* Owner = Members(i).PrimComp->GetOwner();
		if( Owner && !Owner->CanAlwaysLinkEdges && (!Owner->IsStatic() || Owner->bMovable) )
		{
			return FALSE;
		}
	}
	return TRUE;
}

/** BM2: Whether two edges lie along the same run, optionally requiring the gap between them to be clear. */
static UBOOL AreEdgesCoincident( const FVector& OtherA, const FVector& A, const FVector& B,
								 const FVector& OtherB, ULevel* Level, UBOOL bCheckGap )
{
	const FVector Delta = B - A;

	FVector Dir = Delta;
	Dir.Normalize( 1e-8f );

	FVector OtherDir = OtherB - OtherA;
	OtherDir.Normalize( 1e-8f );

	if( (Dir | OtherDir) < 0.9f )
	{
		return FALSE;
	}

	const FLOAT Length = Delta.Size();
	const FLOAT TimeA = ((OtherA - A) | Delta) / Length;
	const FLOAT TimeB = ((OtherB - A) | Delta) / Length;

	if( (TimeA <= -32.f || Length + 32.f <= TimeA)
	&&	(TimeB <= -32.f || Length + 32.f <= TimeB) )
	{
		return FALSE;
	}

	FVector Outward( -Dir.Y, Dir.X, 0.f );
	Outward.Normalize( 1e-8f );
	const FVector Up = Dir ^ Outward;

	const FVector DeltaA = OtherA - A;
	if( DeltaA.SizeSquared() >= 4.1f
	&&	( Abs(DeltaA | Outward) >= 4.1f || Abs(DeltaA | Up) >= 4.1f ) )
	{
		return FALSE;
	}

	const FVector DeltaB = OtherB - A;
	if( DeltaB.SizeSquared() >= 4.1f
	&&	( Abs(DeltaB | Outward) >= 4.1f || Abs(DeltaB | Up) >= 4.1f ) )
	{
		return FALSE;
	}

	if( !bCheckGap
	||	( (TimeA <= Length || TimeB <= Length) && (TimeB >= 0.f || TimeA >= 0.f) ) )
	{
		return TRUE;
	}

	const FLOAT MidTime = (TimeA <= 0.f)
		? Max(TimeA, TimeB) * 0.5f
		: Length + (Min(TimeA, TimeB) - Length) * 0.5f;

	const FVector Mid = A + Dir * MidTime;

	FMemMark Mark( GMainThreadMemStack );

	const FVector Above( Mid.X, Mid.Y, Mid.Z + Abs(Dir.Z) * 32.f + 24.f );
	UBOOL bClear = !FirstBlockingPointHit( Above, FVector(16.f,16.f,16.f), NULL, Level, FALSE );
	if( bClear )
	{
		bClear = !FirstBlockingPointHit( Mid + Outward * 32.f, FVector(16.f,16.f,64.f), NULL, Level, FALSE );
	}

	Mark.Pop();
	return bClear;
}

/**
 * BM2: Folds collections into one another wherever they describe the same continuous ledge,
 * so a run of geometry ends up as a single collection. Collections that stay apart but are
 * within reach record each other in ConnectedCollections instead.
 */
void ULevel::MergeActorEdgeCollections( UModel* Model, TArray<FEdgeCollectionMember>& EdgeMembers )
{
	for( INT i = ActorHorizontalEdges.Num() - 1; i > 0; i-- )
	{
		if( !ActorHorizontalEdges(i).BoundingBox.IsValid
		||	!CanMergeEdgeMembers( EdgeMembers, i )
		||	ActorHorizontalEdges(i).Edges.Num() == 0 )
		{
			continue;
		}

		for( INT j = i - 1; j >= 0; j-- )
		{
			FActorEdgeCollection& CollA = ActorHorizontalEdges(i);
			FActorEdgeCollection& CollB = ActorHorizontalEdges(j);

			if( !CollB.BoundingBox.IsValid || !CanMergeEdgeMembers( EdgeMembers, j ) )
			{
				continue;
			}

			const TArray<FEdgeCollectionMember::MemberContainer>& MembersA = EdgeMembers(i).AssociatedMembers;
			const TArray<FEdgeCollectionMember::MemberContainer>& MembersB = EdgeMembers(j).AssociatedMembers;

			// Collections sharing a coplanar BSP node are always the same surface.
			UBOOL bMerge = FALSE;
			for( INT b = 0; b < MembersB.Num() && !bMerge; b++ )
			{
				if( MembersB(b).Type != FEdgeCollectionMember::ESMTYPE_BSPNode )
				{
					continue;
				}

				const INT NodeIndex = MembersB(b).NodeIndex;
				for( INT Coplanar = Model->Nodes(NodeIndex).iPlane;
					 Coplanar != INDEX_NONE && Coplanar != NodeIndex && !bMerge;
					 Coplanar = Model->Nodes(Coplanar).iPlane )
				{
					for( INT a = 0; a < MembersA.Num(); a++ )
					{
						if( MembersA(a).Type == FEdgeCollectionMember::ESMTYPE_BSPNode
						&&	MembersA(a).NodeIndex == Coplanar )
						{
							bMerge = TRUE;
							break;
						}
					}
				}
			}

			if( !bMerge && !CollA.BoundingBox.Intersect( CollB.BoundingBox ) )
			{
				continue;
			}

			FBox ExpandedA = CollA.BoundingBox;
			ExpandedA.Min -= FVector(128.f,128.f,64.f);
			ExpandedA.Max += FVector(128.f,128.f,64.f);

			FBox ExpandedB = CollB.BoundingBox;
			ExpandedB.Min -= FVector(128.f,128.f,64.f);
			ExpandedB.Max += FVector(128.f,128.f,64.f);

			UBOOL bClose = ExpandedB.IsInside( CollA.BoundingBox.Min ) && ExpandedB.IsInside( CollA.BoundingBox.Max );
			if( !bClose )
			{
				bClose = ExpandedA.IsInside( CollB.BoundingBox.Min ) && ExpandedA.IsInside( CollB.BoundingBox.Max );
			}

			if( !bClose )
			{
				FVector A0, A1, B0, B1;
				for( INT ea = 0; !bClose && CollA.GetEdge( ea, Model, A0, A1 ); ea++ )
				{
					for( INT eb = 0; CollB.GetEdge( eb, Model, B0, B1 ); eb++ )
					{
						if( AreEdgesCoincident( B0, A0, A1, B1, this, FALSE ) )
						{
							bClose = TRUE;
							break;
						}
					}
				}

				// Near enough to reach between, but not the same surface.
				if( !bClose )
				{
					CollB.ConnectedCollections.AddUniqueItem( (WORD)i );
					CollA.ConnectedCollections.AddUniqueItem( (WORD)j );
					continue;
				}
			}

			for( INT e = 0; e < CollA.Edges.Num(); e++ )
			{
				CollB.Edges.AddItem( CollA.Edges(e) );
			}
			for( INT e = 0; e < CollA.RailingTops.Num(); e++ )
			{
				CollB.RailingTops.AddItem( CollA.RailingTops(e) );
			}
			for( INT e = 0; e < CollA.ConnectedCollections.Num(); e++ )
			{
				CollB.ConnectedCollections.AddUniqueItem( CollA.ConnectedCollections(e) );
			}
			CollB.BoundingBox += CollA.BoundingBox;

			ActorHorizontalEdges.Remove( i );
			if( i < HorizontalEdges.Num() )
			{
				HorizontalEdges.Remove( i );
			}

			// Everything that fed the removed collection now feeds the surviving one.
			TArray<FEdgeCollectionMember::MemberContainer>& MoveFrom = EdgeMembers(i).AssociatedMembers;
			TArray<FEdgeCollectionMember::MemberContainer>& MoveTo = EdgeMembers(j).AssociatedMembers;
			for( INT m = 0; m < MoveFrom.Num(); m++ )
			{
				const FEdgeCollectionMember::MemberContainer& Member = MoveFrom(m);
				if( Member.Type == FEdgeCollectionMember::ESMTYPE_BSPNode )
				{
					NodeEdgeCollection( Member.NodeIndex ) = j;
				}
				else if( Member.Type == FEdgeCollectionMember::ESMTYPE_PrimComp )
				{
					Member.PrimComp->LevelEdgeCollectionIndex = j;
				}
				MoveTo.AddItem( Member );
			}
			EdgeMembers.Remove( i );

			// Removing index i shifted every later collection down by one.
			for( INT n = i; n < EdgeMembers.Num(); n++ )
			{
				TArray<FEdgeCollectionMember::MemberContainer>& Members = EdgeMembers(n).AssociatedMembers;
				for( INT m = 0; m < Members.Num(); m++ )
				{
					if( Members(m).Type == FEdgeCollectionMember::ESMTYPE_BSPNode )
					{
						NodeEdgeCollection( Members(m).NodeIndex )--;
					}
					else if( Members(m).Type == FEdgeCollectionMember::ESMTYPE_PrimComp )
					{
						Members(m).PrimComp->LevelEdgeCollectionIndex--;
					}
				}
			}

			for( INT k = 0; k < ActorHorizontalEdges.Num(); k++ )
			{
				TArray<WORD>& Connected = ActorHorizontalEdges(k).ConnectedCollections;
				for( INT c = 0; c < Connected.Num(); c++ )
				{
					if( Connected(c) == i )
					{
						Connected.Remove( c );
						Connected.AddUniqueItem( (WORD)j );
						k--;
						break;
					}
				}
			}

			for( INT k = 0; k < ActorHorizontalEdges.Num(); k++ )
			{
				TArray<WORD>& Connected = ActorHorizontalEdges(k).ConnectedCollections;
				for( INT c = 0; c < Connected.Num(); c++ )
				{
					if( Connected(c) > i )
					{
						Connected(c)--;
					}
				}
			}

			break;
		}
	}
}

/** BM2: Joins edges within a collection that continue one another into single longer edges. */
void ULevel::OptimiseActorEdgeCollections()
{
	for( INT c = ActorHorizontalEdges.Num() - 1; c >= 0; c-- )
	{
		TArray<FActorHorizontalEdge>& Edges = ActorHorizontalEdges(c).Edges;
		for( INT i = Edges.Num() - 1; i > 0; i-- )
		{
			const FVector A0( Edges(i).PointAX, Edges(i).PointAY, Edges(i).PointAZ );
			const FVector A1 = A0 + FVector( Edges(i).PointBX, Edges(i).PointBY, Edges(i).PointBZ );

			for( INT k = i - 1; k >= 0; k-- )
			{
				const FVector B0( Edges(k).PointAX, Edges(k).PointAY, Edges(k).PointAZ );
				const FVector B1 = B0 + FVector( Edges(k).PointBX, Edges(k).PointBY, Edges(k).PointBZ );

				if( Edges(i).EdgeType != Edges(k).EdgeType
				||	!AreEdgesCoincident( B0, A0, A1, B1, this, TRUE ) )
				{
					continue;
				}

				// Span from the furthest point back to the furthest point forward.
				const FVector Dir = A1 - A0;
				const FVector Points[4] = { A0, A1, B0, B1 };

				FVector MinPoint = A0, MaxPoint = A0;
				FLOAT MinProj = BIG_NUMBER, MaxProj = -BIG_NUMBER;
				for( INT p = 0; p < 4; p++ )
				{
					const FLOAT Proj = (Points[p] - A0) | Dir;
					if( Proj > MaxProj ) { MaxProj = Proj; MaxPoint = Points[p]; }
					if( MinProj > Proj ) { MinProj = Proj; MinPoint = Points[p]; }
				}

				FActorHorizontalEdge& Merged = Edges(k);
				Merged.PointAX = MinPoint.X;
				Merged.PointAY = MinPoint.Y;
				Merged.PointAZ = MinPoint.Z;
				Merged.PointBX = (SWORD)( MaxPoint.X - Merged.PointAX );
				Merged.PointBY = (SWORD)( MaxPoint.Y - Merged.PointAY );
				Merged.PointBZ = (SWORD)( MaxPoint.Z - Merged.PointAZ );

				Edges.Remove( i );
				break;
			}
		}
	}
}

/**
 * BM2: Attaches primitives that produced no edges of their own to the nearest collection
 * they overlap, and records the rest of the overlapping collections as neighbours.
 */
void ULevel::ResolveUnlinkedPrimitives( TArray<FEdgeCollectionMember>& EdgeMembers )
{
	for( INT ActorIndex = 0; ActorIndex < Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Actors(ActorIndex);
		if( !Actor
		||	!Actor->bBlockActors
		||	!Actor->bCollideActors
		||	( Actor->IsABrush() && !Actor->IsAVolume() ) )
		{
			continue;
		}

		for( INT ComponentIndex = 0; ComponentIndex < Actor->Components.Num(); ComponentIndex++ )
		{
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>( Actor->Components(ComponentIndex) );
			if( !Prim
			||	!Actor->CanPreBuildClimbableEdges()
			||	!Prim->BlockNonZeroExtent
			||	!Prim->CollideActors
			||	!Actor->IsStatic()
			||	Actor->bMovable )
			{
				continue;
			}

			UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>( Prim );
			UBrushComponent* BrushComp = Cast<UBrushComponent>( Prim );

			const UBOOL bHasGeom = ( MeshComp && MeshComp->StaticMesh && MeshComp->StaticMesh->BodySetup )
								|| ( !MeshComp && BrushComp );
			if( !bHasGeom )
			{
				continue;
			}

			FBox Bounds = Prim->Bounds.GetBox();
			Bounds.Min -= FVector(16.f,16.f,16.f);
			Bounds.Max += FVector(16.f,16.f,64.f);

			TArray<INT> Nearby;
			INT NearestCollection = INDEX_NONE;
			FLOAT NearestDistSq = BIG_NUMBER;

			for( INT n = ActorHorizontalEdges.Num() - 1; n >= 0; n-- )
			{
				if( n == Prim->LevelEdgeCollectionIndex || !CanMergeEdgeMembers( EdgeMembers, n ) )
				{
					continue;
				}

				const FBox& CollBox = ActorHorizontalEdges(n).BoundingBox;
				if( !CollBox.IsValid || !Bounds.Intersect( CollBox ) )
				{
					continue;
				}

				// Ignore collections sitting well below this primitive.
				if( CollBox.Max.Z - Bounds.Max.Z <= -128.f )
				{
					continue;
				}

				Nearby.AddUniqueItem( n );

				const FVector CollCenter = CollBox.GetCenter();
				const FVector MyCenter = Bounds.GetCenter();
				const FLOAT DistSq = Square(MyCenter.X - CollCenter.X) + Square(MyCenter.Y - CollCenter.Y);
				if( NearestDistSq > DistSq )
				{
					NearestDistSq = DistSq;
					NearestCollection = n;
				}
			}

			if( NearestCollection == INDEX_NONE )
			{
				continue;
			}

			if( Prim->LevelEdgeCollectionIndex == 0xFFFF )
			{
				Prim->LevelEdgeCollectionIndex = NearestCollection;
				EdgeMembers(NearestCollection).AssociatedMembers.AddItem( FEdgeCollectionMember::MemberContainer(Prim) );
			}

			for( INT p = 0; p < Nearby.Num(); p++ )
			{
				if( Nearby(p) == Prim->LevelEdgeCollectionIndex )
				{
					continue;
				}

				ActorHorizontalEdges( Nearby(p) ).ConnectedCollections.AddUniqueItem( (WORD)Prim->LevelEdgeCollectionIndex );
				ActorHorizontalEdges( Prim->LevelEdgeCollectionIndex ).ConnectedCollections.AddUniqueItem( (WORD)Nearby(p) );
			}
		}
	}
}

/**
 * BM2: Collections left with no edges of their own only ever held BSP nodes. Hand those nodes
 * to the nearest overlapping collection, then drop the empty collection.
 */
void ULevel::ResolveUnlinkedBSPNodes( TArray<FEdgeCollectionMember>& EdgeMembers, UModel* Model )
{
	for( INT i = ActorHorizontalEdges.Num() - 1; i >= 0; i-- )
	{
		if( ActorHorizontalEdges(i).Edges.Num() > 0 || ActorHorizontalEdges(i).RailingTops.Num() > 0 )
		{
			continue;
		}

		FBox Box(0);
		FVector P0, P1;
		for( INT e = 0; HorizontalEdges(i).GetEdge( e, Model, P0, P1 ); e++ )
		{
			Box += P0;
			Box += P1;
		}

		INT Nearest = INDEX_NONE;
		FLOAT NearestDistSq = BIG_NUMBER;
		TArray<INT> Nearby;

		if( Box.IsValid )
		{
			Box.Min -= FVector(32.f,32.f,32.f);
			Box.Max += FVector(32.f,32.f,32.f);

			for( INT j = ActorHorizontalEdges.Num() - 1; j >= 0; j-- )
			{
				if( j == i || !CanMergeEdgeMembers( EdgeMembers, j ) )
				{
					continue;
				}

				const TArray<FEdgeCollectionMember::MemberContainer>& MembersI = EdgeMembers(i).AssociatedMembers;
				const TArray<FEdgeCollectionMember::MemberContainer>& MembersJ = EdgeMembers(j).AssociatedMembers;

				// Coplanar with something already in the other collection means the same surface.
				UBOOL bAccept = FALSE;
				for( INT m = 0; m < MembersI.Num() && !bAccept; m++ )
				{
					check( MembersI(m).Type == FEdgeCollectionMember::ESMTYPE_BSPNode );

					const INT NodeIndex = MembersI(m).NodeIndex;
					for( INT Coplanar = Model->Nodes(NodeIndex).iPlane;
						 Coplanar != INDEX_NONE && Coplanar != NodeIndex && !bAccept;
						 Coplanar = Model->Nodes(Coplanar).iPlane )
					{
						for( INT n = 0; n < MembersJ.Num(); n++ )
						{
							if( MembersJ(n).Type == FEdgeCollectionMember::ESMTYPE_BSPNode
							&&	MembersJ(n).NodeIndex == Coplanar )
							{
								bAccept = TRUE;
								break;
							}
						}
					}
				}

				const FBox& OtherBox = ActorHorizontalEdges(j).BoundingBox;
				if( !bAccept )
				{
					bAccept = OtherBox.IsValid
						&& Box.Intersect( OtherBox )
						&& (OtherBox.Max.Z - Box.Max.Z) > -32.f;
				}

				if( !bAccept )
				{
					continue;
				}

				Nearby.AddUniqueItem( j );

				const FLOAT DX = (Box.Min.X + Box.Max.X) * 0.5f - (OtherBox.Min.X + OtherBox.Max.X) * 0.5f;
				const FLOAT DY = (Box.Max.Y + Box.Min.Y) * 0.5f - (OtherBox.Max.Y + OtherBox.Min.Y) * 0.5f;
				if( NearestDistSq > DX*DX + DY*DY )
				{
					NearestDistSq = DX*DX + DY*DY;
					Nearest = j;
				}
			}
		}

		TArray<FEdgeCollectionMember::MemberContainer>& Members = EdgeMembers(i).AssociatedMembers;
		if( Nearest != INDEX_NONE )
		{
			for( INT a = 0; a < Nearby.Num() - 1; a++ )
			{
				for( INT b = a + 1; b < Nearby.Num(); b++ )
				{
					ActorHorizontalEdges( Nearby(a) ).ConnectedCollections.AddUniqueItem( (WORD)Nearby(b) );
					ActorHorizontalEdges( Nearby(b) ).ConnectedCollections.AddUniqueItem( (WORD)Nearby(a) );
				}
			}

			for( INT m = 0; m < Members.Num(); m++ )
			{
				check( Members(m).Type == FEdgeCollectionMember::ESMTYPE_BSPNode );
				NodeEdgeCollection( Members(m).NodeIndex ) = Nearest;
				EdgeMembers(Nearest).AssociatedMembers.AddItem( Members(m) );
			}
		}
		else
		{
			for( INT m = 0; m < Members.Num(); m++ )
			{
				check( Members(m).Type == FEdgeCollectionMember::ESMTYPE_BSPNode );
				NodeEdgeCollection( Members(m).NodeIndex ) = 0xFFFF;
			}
		}

		// Removing index i shifts every later collection down by one.
		for( INT m = 0; m < ActorHorizontalEdges.Num(); m++ )
		{
			TArray<WORD>& Connected = ActorHorizontalEdges(m).ConnectedCollections;
			for( INT n = 0; n < Connected.Num(); n++ )
			{
				if( Connected(n) >= i )
				{
					Connected(n)--;
				}
			}

			if( m > i )
			{
				TArray<FEdgeCollectionMember::MemberContainer>& Later = EdgeMembers(m).AssociatedMembers;
				for( INT n = 0; n < Later.Num(); n++ )
				{
					if( Later(n).Type == FEdgeCollectionMember::ESMTYPE_BSPNode )
					{
						NodeEdgeCollection( Later(n).NodeIndex )--;
					}
					else if( Later(n).Type == FEdgeCollectionMember::ESMTYPE_PrimComp )
					{
						Later(n).PrimComp->LevelEdgeCollectionIndex--;
					}
				}
			}
		}

		ActorHorizontalEdges.Remove( i );
		EdgeMembers.Remove( i );
	}
}

/** BM2: A collection of nothing but shimmy edges is useless if there is no way to reach it. */
void ULevel::RemoveUnconnectedShimmyEdges( TArray<FEdgeCollectionMember>& EdgeMembers )
{
	for( INT i = ActorHorizontalEdges.Num() - 1; i > 0; i-- )
	{
		FActorEdgeCollection& Coll = ActorHorizontalEdges(i);
		if( Coll.RailingTops.Num() > 0 || Coll.ConnectedCollections.Num() > 0 )
		{
			continue;
		}

		UBOOL bAllShimmy = TRUE;
		for( INT e = 0; e < Coll.Edges.Num(); e++ )
		{
			if( !(Coll.Edges(e).EdgeType & EDGETYPE_ShimmyOnly) )
			{
				bAllShimmy = FALSE;
				break;
			}
		}

		if( !bAllShimmy )
		{
			continue;
		}

		ActorHorizontalEdges.Remove( i );
		if( i < HorizontalEdges.Num() )
		{
			HorizontalEdges.Remove( i );
		}

		TArray<FEdgeCollectionMember::MemberContainer>& Members = EdgeMembers(i).AssociatedMembers;
		for( INT m = 0; m < Members.Num(); m++ )
		{
			if( Members(m).Type == FEdgeCollectionMember::ESMTYPE_BSPNode )
			{
				NodeEdgeCollection( Members(m).NodeIndex ) = 0xFFFF;
			}
			else if( Members(m).Type == FEdgeCollectionMember::ESMTYPE_PrimComp )
			{
				Members(m).PrimComp->LevelEdgeCollectionIndex = 0xFFFF;
			}
		}
		EdgeMembers.Remove( i );

		// Removing index i shifts every later collection down by one.
		for( INT n = i; n < EdgeMembers.Num(); n++ )
		{
			TArray<FEdgeCollectionMember::MemberContainer>& Later = EdgeMembers(n).AssociatedMembers;
			for( INT m = 0; m < Later.Num(); m++ )
			{
				if( Later(m).Type == FEdgeCollectionMember::ESMTYPE_BSPNode )
				{
					NodeEdgeCollection( Later(m).NodeIndex )--;
				}
				else if( Later(m).Type == FEdgeCollectionMember::ESMTYPE_PrimComp )
				{
					Later(m).PrimComp->LevelEdgeCollectionIndex--;
				}
			}
		}

		for( INT k = 0; k < ActorHorizontalEdges.Num(); k++ )
		{
			TArray<WORD>& Connected = ActorHorizontalEdges(k).ConnectedCollections;
			for( INT c = 0; c < Connected.Num(); c++ )
			{
				if( Connected(c) > i )
				{
					Connected(c)--;
				}
			}
		}
	}
}

/**
 * BM2: Pairs of parallel edges facing each other are the two sides of a railing. Records the
 * line down the middle as a railing top, caps each end, then extends tops to meet at corners.
 */
void ULevel::CreateRailingData()
{
	for( INT c = ActorHorizontalEdges.Num() - 1; c >= 0; c-- )
	{
		FActorEdgeCollection& Coll = ActorHorizontalEdges(c);

		for( INT i = Coll.Edges.Num() - 1; i > 0; i-- )
		{
			if( Coll.Edges(i).EdgeType & EDGETYPE_ShimmyOnly )
			{
				continue;
			}

			const FVector A0( Coll.Edges(i).PointAX, Coll.Edges(i).PointAY, Coll.Edges(i).PointAZ );
			const FVector A1 = A0 + FVector( Coll.Edges(i).PointBX, Coll.Edges(i).PointBY, Coll.Edges(i).PointBZ );

			for( INT k = i - 1; k >= 0; k-- )
			{
				if( Coll.Edges(k).EdgeType & EDGETYPE_ShimmyOnly )
				{
					continue;
				}

				const FVector B0( Coll.Edges(k).PointAX, Coll.Edges(k).PointAY, Coll.Edges(k).PointAZ );
				const FVector B1 = B0 + FVector( Coll.Edges(k).PointBX, Coll.Edges(k).PointBY, Coll.Edges(k).PointBZ );

				FVector DirA = A1 - A0;
				DirA.Normalize( 1e-8f );

				// The far side of a railing runs the opposite way round.
				FVector DirB = B0 - B1;
				DirB.Normalize( 1e-8f );

				if( (DirA | DirB) <= 0.99f )
				{
					continue;
				}

				FVector Mid = (DirA + DirB) * 0.5f;
				Mid.Normalize( 1e-8f );

				FVector Perp( -Mid.Y, Mid.X, 0.f );
				Perp.Normalize( 1e-8f );

				const FVector OffsetStart = A0 - B1;
				const FVector OffsetEnd = A1 - B0;

				const FLOAT SepStart = OffsetStart | Perp;
				const FLOAT SepEnd = OffsetEnd | Perp;
				const FLOAT HeightDiff = Abs( OffsetEnd.Z - (OffsetEnd | DirB) * DirB.Z )
									   + Abs( OffsetStart.Z - DirB.Z * (OffsetStart | DirB) );

				FLOAT MaxSep = 34.f;
				FLOAT MaxHeight = 12.f;
				if( (Coll.Edges(i).EdgeType & EDGETYPE_WideRailing) && (Coll.Edges(k).EdgeType & EDGETYPE_WideRailing) )
				{
					MaxSep = 100.f;
					MaxHeight = 36.f;
				}

				if( MaxHeight <= HeightDiff
				||	SepEnd <= 0.f || MaxSep <= SepEnd
				||	SepStart <= 0.f || MaxSep <= SepStart )
				{
					continue;
				}

				const FLOAT HalfSep = (SepStart + SepEnd) * 0.25f;
				const FLOAT LengthA = (A1 - A0).Size();

				FLOAT TimeStart = (B1 - A0) | Mid;
				FLOAT TimeEnd = (B0 - A0) | Mid;
				if( LengthA <= TimeStart || TimeEnd <= 0.f )
				{
					continue;
				}

				TimeStart = Max( TimeStart, 0.f );
				TimeEnd = Min( TimeEnd, LengthA );

				const FVector Offset = Perp * HalfSep;
				const FVector Start = A0 + Mid * TimeStart - Offset;
				const FVector End = A0 + Mid * TimeEnd - Offset;

				FActorHorizontalEdge Top;
				Top.PointAX = Start.X;
				Top.PointAY = Start.Y;
				Top.PointAZ = Start.Z;
				Top.PointBX = (SWORD)( End.X - Start.X );
				Top.PointBY = (SWORD)( End.Y - Start.Y );
				Top.PointBZ = (SWORD)( End.Z - Start.Z );
				Top.EdgeType = Coll.Edges(k).EdgeType & Coll.Edges(i).EdgeType;
				Coll.RailingTops.AddItem( Top );

				Coll.Edges(i).EdgeType |= EDGETYPE_Railing;
				Coll.Edges(k).EdgeType |= EDGETYPE_Railing;

				// Cap both ends with a short edge running across the railing.
				const FVector StoredStart( Top.PointAX, Top.PointAY, Top.PointAZ );
				const FVector StoredEnd = StoredStart + FVector( Top.PointBX, Top.PointBY, Top.PointBZ );

				FVector Cap( -(StoredEnd.Y - StoredStart.Y), StoredEnd.X - StoredStart.X, 0.f );
				Cap.Normalize( 1e-8f );
				Cap *= 16.f;

				const FVector Middle = (StoredStart + StoredEnd) * 0.5f;
				AddToEdgeCollection( Coll, StoredStart - Cap, StoredStart + Cap, Middle, this, NULL, FALSE, FALSE, TRUE, FALSE, TRUE );
				AddToEdgeCollection( Coll, StoredEnd + Cap, StoredEnd - Cap, Middle, this, NULL, FALSE, FALSE, TRUE, FALSE, TRUE );
			}
		}
	}

	// Railing tops that stop short of a crossing railing get extended to meet it.
	for( INT c = ActorHorizontalEdges.Num() - 1; c >= 0; c-- )
	{
		FActorEdgeCollection& Coll = ActorHorizontalEdges(c);

		for( INT r = Coll.RailingTops.Num() - 1; r >= 0; r-- )
		{
			FVector R0( Coll.RailingTops(r).PointAX, Coll.RailingTops(r).PointAY, Coll.RailingTops(r).PointAZ );
			const FVector Delta( Coll.RailingTops(r).PointBX, Coll.RailingTops(r).PointBY, Coll.RailingTops(r).PointBZ );
			FVector R1 = R0 + Delta;

			const FLOAT Len2DSq = Square(Delta.X) + Square(Delta.Y);
			const FLOAT Len2D = appSqrt( Len2DSq );
			const FVector Dir( Delta.X / Len2D, Delta.Y / Len2D, 0.f );

			FBox Box(0);
			Box += R0;
			Box += R1;
			Box.Min -= FVector(128.f,128.f,34.f);
			Box.Max += FVector(128.f,128.f,34.f);

			FLOAT BestStart = -BIG_NUMBER, BestEnd = BIG_NUMBER;
			FLOAT DotStart = 1.f, DotEnd = 1.f;

			for( INT p = -1; p < Coll.ConnectedCollections.Num(); p++ )
			{
				FActorEdgeCollection& Other = (p < 0) ? Coll : ActorHorizontalEdges( Coll.ConnectedCollections(p) );

				for( INT k = Other.RailingTops.Num() - 1; k >= 0; k-- )
				{
					const FVector O0( Other.RailingTops(k).PointAX, Other.RailingTops(k).PointAY, Other.RailingTops(k).PointAZ );
					const FVector ODelta( Other.RailingTops(k).PointBX, Other.RailingTops(k).PointBY, Other.RailingTops(k).PointBZ );
					const FVector O1 = O0 + ODelta;

					const FLOAT OLen2D = appSqrt( Square(ODelta.X) + Square(ODelta.Y) );
					const FVector ODir( ODelta.X / OLen2D, ODelta.Y / OLen2D, 0.f );

					FBox OBox(0);
					OBox += O0;
					OBox += O1;
					OBox.Min -= FVector(128.f,128.f,34.f);
					OBox.Max += FVector(128.f,128.f,34.f);

					const FLOAT Dot = Abs( ODir | Dir );
					if( !Box.Intersect( OBox ) || Dot >= 0.995f )
					{
						continue;
					}

					const FLOAT Pad = Dot * 75.f + 75.f;
					const FLOAT Reach = Min( Dot * 75.f, Len2D );

					FLOAT TimeThis, TimeOther;
					LineIntersect2D( R0, Dir, O0, ODir, TimeThis, TimeOther );

					// The crossing has to land on the other railing.
					if( TimeOther <= -Pad || (Pad + OLen2D) <= TimeOther )
					{
						continue;
					}

					if( Reach > TimeThis && TimeThis > -Pad && TimeThis > BestStart )
					{
						BestStart = TimeThis;
						DotStart = Dot;
					}
					if( TimeThis > (Len2D - Reach) && (Pad + Len2D) > TimeThis && BestEnd > TimeThis )
					{
						BestEnd = TimeThis;
						DotEnd = Dot;
					}
				}
			}

			const FLOAT InvLength = appInvSqrt( Square(Delta.Z) + Len2DSq );
			if( BestEnd < BIG_NUMBER && BestEnd > Len2D && DotEnd < 0.99f )
			{
				R1 = R0 + Delta * InvLength * BestEnd;
			}
			if( BestStart > -BIG_NUMBER && BestStart < 0.f && DotStart < 0.99f )
			{
				R0 = R0 + Delta * InvLength * BestStart;
			}

			FActorHorizontalEdge& Top = Coll.RailingTops(r);
			Top.PointAX = R0.X;
			Top.PointAY = R0.Y;
			Top.PointAZ = R0.Z;
			Top.PointBX = (SWORD)( R1.X - Top.PointAX );
			Top.PointBY = (SWORD)( R1.Y - Top.PointAY );
			Top.PointBZ = (SWORD)( R1.Z - Top.PointAZ );
		}
	}
}

/** BM2: Rebuilds every piece of climbable edge data for this level from scratch. */
void ULevel::BuildEdgeCollections( UModel* Model )
{
	ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( this );
	if( StreamingLevel && !FLevelUtils::IsLevelVisible( StreamingLevel ) )
	{
		return;
	}

	const DOUBLE StartTime = appSeconds();
	const INT NumNodes = Model->Nodes.Num();

	HorizontalEdges.Empty();
	NodeEdgeCollection.Empty();
	NodeEdgeCollection.AddZeroed( NumNodes );

	TArray<BYTE> Visited;
	Visited.AddZeroed( NumNodes );

	TArray<FEdgeCollectionMember> EdgeMembers;

	const FLOAT SlopeLimit = appTan( 0.3490658402442932f );

	// One collection per BSP node, holding every near-level edge of its polygon.
	for( INT NodeIndex = 0; NodeIndex < NumNodes; NodeIndex++ )
	{
		if( Visited(NodeIndex) )
		{
			continue;
		}

		FEdgeCollection NewCollection;
		FEdgeCollectionMember NewMember;

		const FBspNode& Node = Model->Nodes(NodeIndex);
		NodeEdgeCollection(NodeIndex) = HorizontalEdges.Num();
		NewMember.AssociatedMembers.AddItem( FEdgeCollectionMember::MemberContainer(NodeIndex) );
		Visited(NodeIndex) = 1;

		if( Node.NumVertices > 0 )
		{
			FVector PrevPoint( 0.f, 0.f, 0.f );
			INT PrevVertex = 0;

			for( INT v = 0; v <= Node.NumVertices; v++ )
			{
				const FVert& Vert = Model->Verts( Node.iVertPool + (v % Node.NumVertices) );
				const FVector Point = Model->Points( Vert.pVertex );

				if( v > 0 && Vert.pVertex != PrevVertex )
				{
					const FLOAT Run = appSqrt( Square(Point.X - PrevPoint.X) + Square(Point.Y - PrevPoint.Y) );
					if( SlopeLimit > Abs(Point.Z - PrevPoint.Z) / Run )
					{
						FHorizontalEdge Edge;
						Edge.VertexA = PrevVertex;
						Edge.VertexB = Vert.pVertex;
						Edge.OutwardDir = FVector(0.f,0.f,0.f);

						// A near-vertical node sharing both vertices is the wall below this ledge.
						if( Abs(Node.Plane.Z) >= 0.5f )
						{
							for( INT Other = 0; Other < NumNodes; Other++ )
							{
								if( Other == NodeIndex )
								{
									continue;
								}

								const FBspNode& OtherNode = Model->Nodes(Other);
								if( Abs(OtherNode.Plane.Z) > 0.5f )
								{
									continue;
								}

								UBOOL bHasPrev = FALSE, bHasCurrent = FALSE;
								for( INT ov = 0; ov < OtherNode.NumVertices && !(bHasPrev && bHasCurrent); ov++ )
								{
									const INT OtherVertex = Model->Verts( OtherNode.iVertPool + ov ).pVertex;
									if( PrevVertex == OtherVertex )			{ bHasPrev = TRUE; }
									else if( Vert.pVertex == OtherVertex )	{ bHasCurrent = TRUE; }
								}

								if( bHasPrev && bHasCurrent )
								{
									Edge.OutwardDir = FVector( OtherNode.Plane.X, OtherNode.Plane.Y, OtherNode.Plane.Z );
									break;
								}
							}
						}

						NewCollection.Edges.AddItem( Edge );
					}
				}

				PrevPoint = Point;
				PrevVertex = Vert.pVertex;
			}
		}

		HorizontalEdges.AddItem( NewCollection );
		EdgeMembers.AddItem( NewMember );
	}

	for( INT i = 0; i < HorizontalEdges.Num(); i++ )
	{
		HorizontalEdges(i).OptimizeEdgeCollection( Model );
	}

	ActorHorizontalEdges.Empty();

	UClass* NoClimbVolumeClass = FindObject<UClass>( ANY_PACKAGE, TEXT("RNoClimbVolume") );

	// No-climb volumes only collide while the edges are being built.
	for( INT i = 0; NoClimbVolumeClass && i < Actors.Num(); i++ )
	{
		ABrush* Volume = Cast<ABrush>( Actors(i) );
		if( Volume && Volume->IsA(NoClimbVolumeClass) && Volume->BrushComponent )
		{
			Volume->SetCollision( TRUE, TRUE, TRUE );
		}
	}

	BuildActorEdgeFromBSPEdges( Model );
	BuildActorEdgeCollections( EdgeMembers );
	MergeActorEdgeCollections( Model, EdgeMembers );
	OptimiseActorEdgeCollections();
	ResolveUnlinkedPrimitives( EdgeMembers );
	ResolveUnlinkedBSPNodes( EdgeMembers, Model );
	RemoveUnconnectedShimmyEdges( EdgeMembers );
	CreateRailingData();

	HorizontalEdges.Empty();

	for( INT i = 0; NoClimbVolumeClass && i < Actors.Num(); i++ )
	{
		ABrush* Volume = Cast<ABrush>( Actors(i) );
		if( Volume && Volume->IsA(NoClimbVolumeClass) && Volume->BrushComponent )
		{
			Volume->SetCollision( FALSE, FALSE, TRUE );
		}
	}

	FArchiveCountMem CountMem( NULL );
	CountMem << NodeEdgeCollection;
	CountMem << ActorHorizontalEdges;

	debugf( NAME_Log, TEXT("************************************************") );
	debugf( NAME_Log, TEXT("Edges built for %s - memory: %dkb - time: %.1f secs"),
		*GetFullName(), (INT)(CountMem.GetNum() >> 10), (FLOAT)(appSeconds() - StartTime) );
	debugf( NAME_Log, TEXT("************************************************") );

	bEdgesValid = TRUE;
}

#endif // BATMAN
