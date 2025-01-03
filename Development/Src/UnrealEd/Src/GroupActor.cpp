#include "UnrealEd.h"
#include "ScopedTransaction.h"

const FLinearColor BOXCOLOR_LOCKEDGROUPS( 0.0f, 1.0f, 0.0f );
const FLinearColor BOXCOLOR_UNLOCKEDGROUPS( 1.0f, 0.0f, 0.0f );

void AGroupActor::Spawned()
{
	// Cache our newly created group
	if( !GIsPlayInEditorWorld && !GIsUCC && GIsEditor )
	{
		GEditor->ActiveGroupActors.AddUniqueItem(this);
	}
	Super::Spawned();
}

void AGroupActor::PostLoad()
{
	if( !GIsPlayInEditorWorld && !GIsUCC && GIsEditor )
	{
		// Cache group on de-serialization
		GEditor->ActiveGroupActors.AddUniqueItem(this);
	}

	Super::PostLoad();
}

void AGroupActor::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	// Re-instate group as active if it had children after undo/redo
	if(GroupActors.Num() || SubGroups.Num())
	{
		GEditor->ActiveGroupActors.AddUniqueItem(this);
	}
	else // Otherwise, attempt to remove them
	{
		GEditor->ActiveGroupActors.RemoveItem(this);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AGroupActor::BeginDestroy()
{
	Super::BeginDestroy();
}

void AGroupActor::EditorApplyTranslation(const FVector& DeltaTranslation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
	}
	Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
}

void AGroupActor::EditorApplyRotation(const FRotator& DeltaRotation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		AActor* Actor = GroupActors(ActorIndex);

		AGroupActor* ActorRootGroup = AGroupActor::GetRootForActor( Actor, FALSE, TRUE);
		if(ActorRootGroup)
		{
			Actor->EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);

			// Update all actors based on location of editor pivot
			FVector NewActorLocation = Actor->Location;
			NewActorLocation -= GEditorModeTools().PivotLocation;
			NewActorLocation = FRotationMatrix( DeltaRotation ).TransformFVector( NewActorLocation );
			NewActorLocation += GEditorModeTools().PivotLocation;
			Actor->SetLocation(NewActorLocation);
		}
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
	}
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
}

void AGroupActor::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->EditorApplyScale( DeltaScale, ScaleMatrix, PivotLocation, bAltDown, bShiftDown, bCtrlDown );
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->EditorApplyScale(DeltaScale, ScaleMatrix, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
	}
	Super::EditorApplyScale(DeltaScale, ScaleMatrix, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
}

UBOOL AGroupActor::IsSelected() const
{
	// Group actors can only count as 'selected' if they are locked 
	return IsLocked() && HasSelectedActors() || Super::IsSelected();
}

void AGroupActor::Modify(UBOOL bAlwaysMarkDirty/*=TRUE*/)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->Modify(bAlwaysMarkDirty);
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->Modify(bAlwaysMarkDirty);
	}
	Super::Modify(bAlwaysMarkDirty);
}

void AGroupActor::MarkPackageDirty( UBOOL InDirty/* = 1 */ ) const
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->MarkPackageDirty(InDirty);
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->MarkPackageDirty(InDirty);
	}
	Super::MarkPackageDirty(InDirty);
}
void AGroupActor::InvalidateLightingCache()
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->InvalidateLightingCache();
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->InvalidateLightingCache();
	}
	Super::InvalidateLightingCache();
}
void AGroupActor::PostEditMove(UBOOL bFinished)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->PostEditMove(bFinished);
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->PostEditMove(bFinished);
	}
	Super::PostEditMove(bFinished);
}
void AGroupActor::ForceUpdateComponents(UBOOL bCollisionUpdate/*=FALSE*/,UBOOL bTransformOnly/*=TRUE*/)
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		GroupActors(ActorIndex)->ForceUpdateComponents(bCollisionUpdate, bTransformOnly);
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		SubGroups(SubGroupIndex)->ForceUpdateComponents(bCollisionUpdate, bTransformOnly);
	}
	Super::ForceUpdateComponents(bCollisionUpdate, bTransformOnly);
}

void GetBoundingVectorsForGroup(AGroupActor* GroupActor, FViewport* Viewport, FVector& OutVectorMin, FVector& OutVectorMax)
{
	// Draw a bounding box for grouped actors using the vector range we can gather from any child actors (including subgroups)
	OutVectorMin = FVector(BIG_NUMBER);
	OutVectorMax = FVector(-BIG_NUMBER);

	// Grab all actors for this group, including those within subgroups
	TArray<AActor*> ActorsInGroup;
	GroupActor->GetGroupActors(ActorsInGroup, TRUE);

	// Loop through and collect each actor, using their bounding box to create the bounds for this group
	for(INT ActorIndex = 0; ActorIndex < ActorsInGroup.Num(); ++ActorIndex)
	{
		AActor* Actor = ActorsInGroup(ActorIndex);
		QWORD HiddenClients = Actor->HiddenEditorViews;
		UBOOL bActorHiddenForViewport = FALSE;
		if(!Actor->IsHiddenEd())
		{
			if(Viewport)
			{
				for(INT ViewIndex=0; ViewIndex<GEditor->ViewportClients.Num(); ++ViewIndex)
				{
					// If the current viewport is hiding this actor, don't draw brackets around it
					if(Viewport->GetClient() == GEditor->ViewportClients(ViewIndex) && HiddenClients & ((QWORD)1 << ViewIndex))
					{
						bActorHiddenForViewport = TRUE;
						break;
					}
				}
			}

			if(!bActorHiddenForViewport)
			{
				FBox ActorBox = Actor->GetComponentsBoundingBox( TRUE );

				// MinVector
				OutVectorMin.X = Min<FLOAT>( ActorBox.Min.X, OutVectorMin.X );
				OutVectorMin.Y = Min<FLOAT>( ActorBox.Min.Y, OutVectorMin.Y );
				OutVectorMin.Z = Min<FLOAT>( ActorBox.Min.Z, OutVectorMin.Z );
				// MaxVector
				OutVectorMax.X = Max<FLOAT>( ActorBox.Max.X, OutVectorMax.X );
				OutVectorMax.Y = Max<FLOAT>( ActorBox.Max.Y, OutVectorMax.Y );
				OutVectorMax.Z = Max<FLOAT>( ActorBox.Max.Z, OutVectorMax.Z );
			}
		}
	}	
}

/**
 * Draw brackets around all given groups
 * @param	PDI			FPrimitiveDrawInterface used to draw lines in active viewports
 * @param	Viewport	Current viewport being rendered
 * @param	InGroupList	Array of groups to draw brackets for
 */
void PrivateDrawBracketsForGroups( FPrimitiveDrawInterface* PDI, FViewport* Viewport, const TArray<AGroupActor*>& InGroupList )
{
	// Loop through each given group and draw all subgroups and actors
	for(INT GroupIndex=0; GroupIndex<InGroupList.Num(); ++GroupIndex)
	{
		AGroupActor* GroupActor = InGroupList(GroupIndex);
		const FLinearColor GROUP_COLOR = GroupActor->IsLocked() ? BOXCOLOR_LOCKEDGROUPS : BOXCOLOR_UNLOCKEDGROUPS;
		
		FVector MinVector;
		FVector MaxVector;
		GetBoundingVectorsForGroup( GroupActor, Viewport, MinVector, MaxVector );

		// Create a bracket offset to pad the space between brackets and actor(s) and determine the length of our corner axises
		FLOAT BracketOffset = FDist(MinVector, MaxVector) * 0.1f;
		MinVector = MinVector - BracketOffset;
		MaxVector = MaxVector + BracketOffset;

		// Calculate bracket corners based on min/max vectors
		TArray<FVector> BracketCorners;

		// Bottom Corners
		BracketCorners.AddItem(FVector(MinVector.X, MinVector.Y, MinVector.Z));
		BracketCorners.AddItem(FVector(MinVector.X, MaxVector.Y, MinVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MaxVector.Y, MinVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MinVector.Y, MinVector.Z));

		// Top Corners
		BracketCorners.AddItem(FVector(MinVector.X, MinVector.Y, MaxVector.Z));
		BracketCorners.AddItem(FVector(MinVector.X, MaxVector.Y, MaxVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MaxVector.Y, MaxVector.Z));
		BracketCorners.AddItem(FVector(MaxVector.X, MinVector.Y, MaxVector.Z));
		
		for(INT BracketCornerIndex=0; BracketCornerIndex<BracketCorners.Num(); ++BracketCornerIndex)
		{
			// Direction corner axis should be pointing based on min/max
			const FVector CORNER = BracketCorners(BracketCornerIndex);
			const INT DIR_X = CORNER.X == MaxVector.X ? -1 : 1;
			const INT DIR_Y = CORNER.Y == MaxVector.Y ? -1 : 1;
			const INT DIR_Z = CORNER.Z == MaxVector.Z ? -1 : 1;

			PDI->DrawLine( CORNER, FVector(CORNER.X + (BracketOffset * DIR_X), CORNER.Y, CORNER.Z), GROUP_COLOR, SDPG_Foreground );
			PDI->DrawLine( CORNER, FVector(CORNER.X, CORNER.Y + (BracketOffset * DIR_Y), CORNER.Z), GROUP_COLOR, SDPG_Foreground );
			PDI->DrawLine( CORNER, FVector(CORNER.X, CORNER.Y, CORNER.Z + (BracketOffset * DIR_Z)), GROUP_COLOR, SDPG_Foreground );
		}

		// Recurse through to any subgroups
		TArray<AGroupActor*> SubGroupsInGroup;
		GroupActor->GetSubGroups(SubGroupsInGroup);
		PrivateDrawBracketsForGroups(PDI, Viewport, SubGroupsInGroup);
	}
}

/**
 * Draw brackets around all selected groups
 * @param	PDI				FPrimitiveDrawInterface used to draw lines in active viewports
 * @param	Viewport		Current viewport being rendered
 * @param	bMustBeSelected	Flag to only draw currently selected groups. Defaults to TRUE.
 */
void AGroupActor::DrawBracketsForGroups( FPrimitiveDrawInterface* PDI, FViewport* Viewport, UBOOL bMustBeSelected/*=TRUE*/ )
{
	if( GUnrealEd->bGroupingActive )
	{
		check(PDI);
	
		if(bMustBeSelected)
		{
			// If we're only drawing for selected group, grab only those that have currently selected actors
			TArray<AGroupActor*> SelectedGroups;
			for(INT GroupIndex=0; GroupIndex < GEditor->ActiveGroupActors.Num(); ++GroupIndex)
			{
				AGroupActor *SelectedGroupActor = GEditor->ActiveGroupActors(GroupIndex);		
				if(SelectedGroupActor->HasSelectedActors())
				{
					// We want to start drawing groups from the highest root level.
					// Subgroups will be propagated through during the draw code.
					SelectedGroupActor = AGroupActor::GetRootForActor(SelectedGroupActor);
					SelectedGroups.AddItem(SelectedGroupActor);
				}
			}
			PrivateDrawBracketsForGroups(PDI, Viewport, SelectedGroups);
		}
		else
		{
			PrivateDrawBracketsForGroups(PDI, Viewport, GEditor->ActiveGroupActors );
		}
	}
}

/**
 * Checks to see if the given GroupActor has any parents in the given Array.
 * @param	InGroupActor	Group to check lineage
 * @param	InGroupArray	Array to search for the given group's parent
 * @return	True if a parent was found.
 */
UBOOL GroupHasParentInArray(AGroupActor* InGroupActor, TArray<AGroupActor*>& InGroupArray)
{
	check(InGroupActor);
	AGroupActor* CurrentParentNode = AGroupActor::GetParentForActor(InGroupActor);

	// Use a cursor pointer to continually move up from our starting pointer (InGroupActor) through the hierarchy until
	// we find a valid parent in the given array, or run out of nodes.
	while( CurrentParentNode )
	{
		if(InGroupArray.ContainsItem(CurrentParentNode))
		{
			return TRUE;
		}
		CurrentParentNode = AGroupActor::GetParentForActor(CurrentParentNode);
	}
	return FALSE;
}

/**
 * Changes the given array to remove any existing subgroups
 * @param	GroupArray	Array to remove subgroups from
 */
void AGroupActor::RemoveSubGroupsFromArray(TArray<AGroupActor*>& GroupArray)
{
	for(INT GroupIndex=0; GroupIndex<GroupArray.Num(); ++GroupIndex)
	{
		AGroupActor* GroupToCheck = GroupArray(GroupIndex);
		if(GroupHasParentInArray(GroupToCheck, GroupArray))
		{
			GroupArray.RemoveItem(GroupToCheck);
			--GroupIndex;
		}
	}
}

/**
 * Returns the highest found root for the given actor or null if one is not found. Qualifications of root can be specified via optional parameters.
 * @param	InActor			Actor to find a group root for.
 * @param	bMustBeLocked	Flag designating to only return the topmost locked group.
 * @param	bMustBeSelected	Flag designating to only return the topmost selected group.
 * @return	The topmost group actor for this actor. Returns null if none exists using the given conditions.
 */
AGroupActor* AGroupActor::GetRootForActor(AActor* InActor, UBOOL bMustBeLocked/*=FALSE*/, UBOOL bMustBeSelected/*=FALSE*/)
{
	AGroupActor* RootNode = NULL;
	// If InActor is a group, use that as the beginning iteration node, else try to find the parent
	AGroupActor* InGroupActor = Cast<AGroupActor>(InActor);
	AGroupActor* IteratingNode = InGroupActor == NULL ? AGroupActor::GetParentForActor(InActor) : InGroupActor;
	while( IteratingNode )
	{
		if ( (!bMustBeLocked || IteratingNode->IsLocked()) && (!bMustBeSelected || IteratingNode->HasSelectedActors()) )
		{
			RootNode = IteratingNode;
		}
		IteratingNode = AGroupActor::GetParentForActor(IteratingNode);
	}
	return RootNode;
}

/**
 * Returns the direct parent for the actor or null if one is not found.
 * @param	InActor	Actor to find a group parent for.
 * @return	The direct parent for the given actor. Returns null if no group has this actor as a child.
 */
AGroupActor* AGroupActor::GetParentForActor(AActor* InActor)
{
	for(INT GroupActorIndex = 0; GroupActorIndex < GEditor->ActiveGroupActors.Num(); ++GroupActorIndex)
	{
		AGroupActor* GroupActor = GEditor->ActiveGroupActors(GroupActorIndex);
		if(GroupActor->Contains(*InActor))
		{
			return GroupActor;
		}
	}
	return NULL;
}

/**
 * Query to find how many active groups are currently in the editor.
 * @param	bSelected	Flag to only return currently selected groups (defaults to FALSE).
 * @param	bDeepSearch	Flag to do a deep search when checking group selections (defaults to TRUE).
 * @return	Number of active groups currently in the editor.
 */
const INT AGroupActor::NumActiveGroups( UBOOL bSelected/*=FALSE*/, UBOOL bDeepSearch/*=TRUE*/ )
{
	if(!bSelected)
	{
		return GEditor->ActiveGroupActors.Num();
	}

	INT ActiveSelectedGroups = 0;
	for(INT GroupIdx=0; GroupIdx < GEditor->ActiveGroupActors.Num(); ++GroupIdx )
	{
		if(GEditor->ActiveGroupActors(GroupIdx)->HasSelectedActors(bDeepSearch))
		{
			++ActiveSelectedGroups;
		}
	}
	return ActiveSelectedGroups;
}

/**
* Adds selected ungrouped actors to a selected group. Does nothing if more than one group is selected.
*/
void AGroupActor::AddSelectedActorsToSelectedGroup()
{

	INT SelectedGroupIndex = -1;
	for(INT GroupIdx=0; GroupIdx < GEditor->ActiveGroupActors.Num(); ++GroupIdx )
	{
		if(GEditor->ActiveGroupActors(GroupIdx)->HasSelectedActors(FALSE))
		{
			// Assign the index of the selected group.
			// If this is the second group we find, too many groups are selected, return.
			if( SelectedGroupIndex == -1 )
			{
				SelectedGroupIndex = GroupIdx;
			}
			else { return; }
		}
	}

	if( SelectedGroupIndex != -1 )
	{
		AGroupActor* SelectedGroup = GEditor->ActiveGroupActors(SelectedGroupIndex);
		
		ULevel* GroupLevel = SelectedGroup->GetLevel();

		// We've established that only one group is selected, so we can just call Add on all these actors.
		// Any actors already in the group will be ignored.
		
		TArray<AActor*> ActorsToAdd;

		UBOOL bActorsInSameLevel = TRUE;
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = CastChecked<AActor>( *It );
		
			if( Actor->GetLevel() == GroupLevel )
			{
				ActorsToAdd.AddItem( Actor );
			}
			else
			{
				bActorsInSameLevel = FALSE;
				break;
			}
		}

		if( bActorsInSameLevel )
		{
			if( ActorsToAdd.Num() > 0 )
			{
				const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Add") );
				for( INT ActorIndex = 0; ActorIndex < ActorsToAdd.Num(); ++ActorIndex )
				{
					SelectedGroup->Add( *ActorsToAdd(ActorIndex) );
				}
			}
		}
		else
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd( "Group_CantCreateGroupMultipleLevels") );
		}
	}
}

/**
 * Locks the lowest selected groups in the current selection.
 */
void AGroupActor::LockSelectedGroups()
{
	TArray<AGroupActor*> GroupsToLock;
	for ( INT GroupIndex=0; GroupIndex<GEditor->ActiveGroupActors.Num(); ++GroupIndex )
	{
		AGroupActor* GroupToLock = GEditor->ActiveGroupActors(GroupIndex);

		if( GroupToLock->HasSelectedActors(FALSE) )
		{
			// If our selected group is already locked, move up a level to add it's potential parent for locking
			if( GroupToLock->IsLocked() )
			{
				AGroupActor* GroupParent = AGroupActor::GetParentForActor(GroupToLock);
				if(GroupParent && !GroupParent->IsLocked())
				{
					GroupsToLock.AddUniqueItem(GroupParent);
				}
			}
			else // if it's not locked, add it instead!
			{
				GroupsToLock.AddUniqueItem(GroupToLock);
			}
		}
	}

	if( GroupsToLock.Num() > 0 )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Lock") );
		for ( INT GroupIndex=0; GroupIndex<GroupsToLock.Num(); ++GroupIndex )
		{
			AGroupActor* GroupToLock = GroupsToLock(GroupIndex);
			GroupToLock->Modify();
			GroupToLock->Lock();
			GEditor->SelectGroup(GroupToLock, FALSE );
		}
		GEditor->NoteSelectionChange();
	}
}

/**
 * Unlocks the highest locked parent group for actors in the current selection.
 */
void AGroupActor::UnlockSelectedGroups()
{
	TArray<AGroupActor*> GroupsToUnlock;
	for ( INT GroupIndex=0; GroupIndex<GEditor->ActiveGroupActors.Num(); ++GroupIndex )
	{
		AGroupActor* GroupToUnlock = GEditor->ActiveGroupActors(GroupIndex);
		if( GroupToUnlock->IsSelected() )
		{
			GroupsToUnlock.AddItem(GroupToUnlock);
		}
	}

	// Only unlock topmost selected group(s)
	AGroupActor::RemoveSubGroupsFromArray(GroupsToUnlock);
	if( GroupsToUnlock.Num() > 0 )
	{
		const FScopedTransaction Transaction( *LocalizeUnrealEd("Group_Unlock") );
		for ( INT GroupIndex=0; GroupIndex<GroupsToUnlock.Num(); ++GroupIndex)
		{
			AGroupActor* GroupToUnlock = GroupsToUnlock(GroupIndex);
			GroupToUnlock->Modify();
			GroupToUnlock->Unlock();
		}
		GEditor->NoteSelectionChange();
	}
}

/**
 * Lock this group and all subgroups.
 */
void AGroupActor::Lock()
{
	bLocked = TRUE;
	for(INT SubGroupIdx=0; SubGroupIdx < SubGroups.Num(); ++SubGroupIdx )
	{
		SubGroups(SubGroupIdx)->Lock();
	}
}

/**
 * @param	InActor	Actor to add to this group
 */
void AGroupActor::Add(AActor& InActor)
{	
	// See if the incoming actor already belongs to a group
	AGroupActor* InActorParent = AGroupActor::GetParentForActor(&InActor);
	if(InActorParent) // If so, detach it first
	{
		if(InActorParent == this)
		{
			return;
		}
		InActorParent->Modify();
		InActorParent->Remove(InActor);
	}
	
	Modify();
	AGroupActor* InGroupPtr = Cast<AGroupActor>(&InActor);
	if(InGroupPtr)
	{
		check(InGroupPtr != this);
		SubGroups.AddUniqueItem(InGroupPtr);
	}
	else
	{
		GroupActors.AddUniqueItem(&InActor);
	}
}

/**
 * Removes the given actor from this group. If the group has no actors after this transaction, the group itself is removed.
 * @param	InActor	Actor to remove from this group
 */
void AGroupActor::Remove(AActor& InActor)
{
	AGroupActor* InGroupPtr = Cast<AGroupActor>(&InActor);
	if(InGroupPtr && SubGroups.ContainsItem(InGroupPtr))
	{
		Modify();
		SubGroups.RemoveItem(InGroupPtr);
	}
	else if(GroupActors.ContainsItem(&InActor))
	{
		Modify();
		GroupActors.RemoveItem(&InActor);
	}
	
	// If all children have been removed (or only one subgroup remains), this group is no longer active.
	if( GroupActors.Num() == 0 && SubGroups.Num() <= 1 )
	{
		// Destroy the actor and remove it from active groups
		AGroupActor* ParentGroup = AGroupActor::GetParentForActor(this);
		if(ParentGroup)
		{
			ParentGroup->Modify();
			ParentGroup->Remove(*this);
		}

		// Group is no longer active
		GEditor->ActiveGroupActors.RemoveItem(this);

		if( GWorld )
		{
			GWorld->ModifyLevel(GetLevel());
			
			// Mark the group actor for removal
			MarkPackageDirty();

			// If not currently garbage collecting (changing maps, saving, etc), remove the group immediately
			if(!GIsGarbageCollecting)
			{
				// Refresh all editor browsers after removal
				FScopedRefreshEditor_AllBrowsers LevelRefreshAllBrowsers;

				// Let the object propagator report the deletion
				GObjectPropagator->OnActorDelete(this);

				// Destroy group and clear references.
				GWorld->EditorDestroyActor( this, FALSE );			
				
				LevelRefreshAllBrowsers.Request();
			}
		}
	}
}

/**
 * @param InActor	Actor to search for
 * @return True if the group contains the given actor.
 */
UBOOL AGroupActor::Contains(AActor& InActor) const
{
	AActor* InActorPtr = &InActor;
	AGroupActor* InGroupPtr = Cast<AGroupActor>(InActorPtr);
	if(InGroupPtr)
	{
		return SubGroups.ContainsItem(InGroupPtr);
	}
	return GroupActors.ContainsItem(InActorPtr);
}

/**
 * @param bDeepSearch	Flag to check all subgroups as well. Defaults to TRUE.
 * @return True if the group contains any selected actors.
 */
UBOOL AGroupActor::HasSelectedActors(UBOOL bDeepSearch/*=TRUE*/) const
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		if( GroupActors(ActorIndex)->IsSelected() ) { return TRUE; }
	}
	if(bDeepSearch)
	{
		for(INT GroupIndex=0; GroupIndex<SubGroups.Num(); ++GroupIndex)
		{
			if( SubGroups(GroupIndex)->HasSelectedActors(bDeepSearch) ) { return TRUE; }
		}
	}
	return false;
}

/**
 * Detaches all children (actors and subgroups) from this group and then removes it.
 */
void AGroupActor::ClearAndRemove()
{
	for(INT ActorIndex=0; ActorIndex<GroupActors.Num(); ++ActorIndex)
	{
		Remove(*GroupActors(ActorIndex));
		--ActorIndex;
	}
	for(INT SubGroupIndex=0; SubGroupIndex<SubGroups.Num(); ++SubGroupIndex)
	{
		Remove(*SubGroups(SubGroupIndex));
		--SubGroupIndex;
	}
}

/**
 * Sets this group's location to the center point based on current location of its children.
 */
void AGroupActor::CenterGroupLocation()
{
	FVector MinVector;
	FVector MaxVector;
	GetBoundingVectorsForGroup( this, NULL, MinVector, MaxVector );

	SetLocation((MinVector + MaxVector) * 0.5f);
	GEditor->NoteSelectionChange();
}

/**
 * @param	OutGroupActors	Array to fill with all actors for this group.
 * @param	bRecurse		Flag to recurse and gather any actors in this group's subgroups.
 */
void AGroupActor::GetGroupActors(TArray<AActor*>& OutGroupActors, UBOOL bRecurse/*=FALSE*/) const
{
	if( bRecurse )
	{
		for(INT i=0; i<SubGroups.Num(); ++i)
		{
			SubGroups(i)->GetGroupActors(OutGroupActors, bRecurse);
		}
	}
	else
	{
		OutGroupActors.Empty();
	}
	for(INT i=0; i<GroupActors.Num(); ++i)
	{
		OutGroupActors.AddItem(GroupActors(i));
	}
}

/**
 * @param	OutSubGroups	Array to fill with all subgroups for this group.
 * @param	bRecurse		Flag to recurse and gather any subgroups in this group's subgroups.
 */
void AGroupActor::GetSubGroups(TArray<AGroupActor*>& OutSubGroups, UBOOL bRecurse/*=FALSE*/) const
{
	if( bRecurse )
	{
		for(INT i=0; i<SubGroups.Num(); ++i)
		{
			SubGroups(i)->GetSubGroups(OutSubGroups, bRecurse);
		}
	}
	else
	{
		OutSubGroups.Empty();
	}
	for(INT i=0; i<SubGroups.Num(); ++i)
	{
		OutSubGroups.AddItem(SubGroups(i));
	}
}

/**
 * @param	OutChildren		Array to fill with all children for this group.
 * @param	bRecurse		Flag to recurse and gather any children in this group's subgroups.
 */
void AGroupActor::GetAllChildren(TArray<AActor*>& OutChildren, UBOOL bRecurse/*=FALSE*/) const
{
	GetGroupActors(OutChildren, bRecurse);
	TArray<AGroupActor*> OutSubGroups;
	GetSubGroups(OutSubGroups, bRecurse);
	for(INT SubGroupIdx=0; SubGroupIdx<OutSubGroups.Num(); ++SubGroupIdx)
	{
		OutChildren.AddItem(OutSubGroups(SubGroupIdx));
	}
}

IMPLEMENT_CLASS(AGroupActor);


