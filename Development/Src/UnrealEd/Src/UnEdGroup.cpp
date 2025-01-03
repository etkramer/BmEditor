/*=============================================================================
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ScopedTransaction.h"

/**
 * Creates a new group from the current selection maintaining existing groups.
 */
void UUnrealEdEngine::edactGroupFromSelected()
{
	TArray<AActor*> ActorsToAdd;
	ULevel* ActorLevel = NULL;

	UBOOL bActorsInSameLevel = TRUE;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = CastChecked<AActor>( *It );

		if( !ActorLevel )
		{
			ActorLevel = Actor->GetLevel();
		}
		else if( ActorLevel != Actor->GetLevel() )
		{
			bActorsInSameLevel = FALSE;
			break;
		}

		// See if a group selected to be added into the new group
		AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
		if(GroupActor == NULL) // Aren't directly selecting a group, see if the actor has a locked parent
		{
			GroupActor = AGroupActor::GetParentForActor(Actor);
			// If the actor has a locked parent, add it. Otherwise, ignore it.
			if(GroupActor && GroupActor->IsLocked())
			{
				Actor = GroupActor;
			}
		}
		ActorsToAdd.AddUniqueItem(Actor);
	}

	// Must be creating a group with at least two actors (actor + group, two groups, etc)
	if( ActorsToAdd.Num() > 1 && bActorsInSameLevel )
	{
		check(ActorLevel);

		// Store off the current level and make the level that contain the actors to group as the current level
		ULevel* PrevLevel = GWorld->CurrentLevel;
		GWorld->CurrentLevel = ActorLevel;

		{
			const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Create") );

			AGroupActor* SpawnedGroupActor = Cast<AGroupActor>( GWorld->SpawnActor(AGroupActor::StaticClass()) );

			for ( INT ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
			{
				AActor* Actor = ActorsToAdd(ActorIndex);
				Actor->Modify();

				// Add each selected actor to our new group
				SpawnedGroupActor->Add(*Actor);
			}
			SpawnedGroupActor->CenterGroupLocation();
			SpawnedGroupActor->Lock();
		}

		// Restore the previous level that was current
		GWorld->CurrentLevel = PrevLevel;
	}
	else if( !bActorsInSameLevel )
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Group_CantCreateGroupMultipleLevels") );
	}
}

/**
 * Creates a new group from the current selection removing any attachments to existing groups.
 */
void UUnrealEdEngine::edactRegroupFromSelected()
{
	TArray<AActor*> ActorsToAdd;

	ULevel* ActorLevel = NULL;

	UBOOL bActorsInSameLevel = TRUE;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
	{
		AActor* Actor = CastChecked<AActor>(*It);
		if( !ActorLevel )
		{
			ActorLevel = Actor->GetLevel();
		}
		else if( ActorLevel != Actor->GetLevel() )
		{
			bActorsInSameLevel = FALSE;
			break;
		}

		if ( Actor->IsA(AActor::StaticClass()) && !Actor->IsA(AGroupActor::StaticClass()) )
		{
			// Add each selected actor to our new group
			// Adding an actor will remove it from any existing groups.
			ActorsToAdd.AddItem( Actor );

		}
	}

	if( bActorsInSameLevel )
	{
		if( ActorsToAdd.Num() > 1 )
		{
			// Store off the current level and make the level that contain the actors to group as the current level
			ULevel* PrevLevel = GWorld->CurrentLevel;
			GWorld->CurrentLevel = ActorLevel;

			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Regroup") );

				AGroupActor* SpawnedGroupActor = Cast<AGroupActor>( GWorld->SpawnActor(AGroupActor::StaticClass()) );

				for( INT ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
				{
					SpawnedGroupActor->Add( *ActorsToAdd(ActorIndex) );
				}

				SpawnedGroupActor->CenterGroupLocation();
				SpawnedGroupActor->Lock();
			}

			// Restore the previous level that was current
			GWorld->CurrentLevel = PrevLevel;
		}
	}
	else
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Group_CantCreateGroupMultipleLevels") );
	}
}

/**
 * Disbands any groups in the current selection, does not attempt to maintain any hierarchy
 */
void UUnrealEdEngine::edactUngroupFromSelected()
{
	TArray<AGroupActor*> OutermostGroupActors;
	
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = CastChecked<AActor>( *It );

		// Get the outermost locked group
		AGroupActor* OutermostGroup = AGroupActor::GetRootForActor( Actor, TRUE );

		if( OutermostGroup )
		{
			OutermostGroupActors.AddUniqueItem( OutermostGroup );
		}
		else
		{
			OutermostGroupActors.AddUniqueItem( AGroupActor::GetParentForActor( Actor ) );
		}
	}

	if( OutermostGroupActors.Num() )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Disband") );
		for( INT GroupIndex = 0; GroupIndex < OutermostGroupActors.Num(); ++GroupIndex )
		{
			AGroupActor* GroupActor = OutermostGroupActors(GroupIndex);
			GroupActor->ClearAndRemove();
		}
	}

}

/**
 * Locks any groups in the current selection
 */
void UUnrealEdEngine::edactLockSelectedGroups()
{
	AGroupActor::LockSelectedGroups();
}

/**
 * Unlocks any groups in the current selection
 */
void UUnrealEdEngine::edactUnlockSelectedGroups()
{
	AGroupActor::UnlockSelectedGroups();
}

/**
 * Activates "Add to Group" mode which allows the user to select a group to append current selection
 */
void UUnrealEdEngine::edactAddToGroup()
{
	AGroupActor::AddSelectedActorsToSelectedGroup();
}

/** 
 * Removes any groups or actors in the current selection from their immediate parent.
 * If all actors/subgroups are removed, the parent group will be destroyed.
 */
void UUnrealEdEngine::edactRemoveFromGroup()
{
	TArray<AActor*> ActorsToRemove;
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// See if an entire group is being removed
		AGroupActor* GroupActor = Cast<AGroupActor>(Actor);
		if(GroupActor == NULL)
		{
			// See if the actor selected belongs to a locked group, if so remove the group in lieu of the actor
			GroupActor = AGroupActor::GetParentForActor(Actor);
			if(GroupActor && !GroupActor->IsLocked())
			{
				GroupActor = NULL;
			}
		}

		if(GroupActor)
		{
			// If the GroupActor has no parent, do nothing, otherwise just add the group for removal
			if(AGroupActor::GetParentForActor(GroupActor))
			{
				ActorsToRemove.AddUniqueItem(GroupActor);
			}
		}
		else
		{
			ActorsToRemove.AddUniqueItem(Actor);
		}
	}

	const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Remove") );
	for ( INT ActorIndex = 0; ActorIndex < ActorsToRemove.Num(); ++ActorIndex )
	{
		AActor* Actor = ActorsToRemove(ActorIndex);
		AGroupActor* ActorGroup = AGroupActor::GetParentForActor(Actor);

		if(ActorGroup)
		{
			AGroupActor* ActorGroupParent = AGroupActor::GetParentForActor(ActorGroup);
			if(ActorGroupParent)
			{
				ActorGroupParent->Add(*Actor);
			}
			else
			{
				ActorGroup->Remove(*Actor);
			}
		}
	}

	// Do a re-selection of each actor, to maintain group selection rules
	GEditor->SelectNone(TRUE, TRUE);
	for ( INT ActorIndex = 0; ActorIndex < ActorsToRemove.Num(); ++ActorIndex )
	{
		GEditor->SelectActor( ActorsToRemove(ActorIndex), TRUE, NULL, FALSE);
	}
}
