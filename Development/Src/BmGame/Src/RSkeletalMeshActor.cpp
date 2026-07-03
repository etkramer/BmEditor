/*=============================================================================
	RSkeletalMeshActor.cpp
	BM: Native reconstruction of BM2's RSkeletalMeshActor.

	Gives BM2's cinematic actors the SkeletalMeshActorMAT-style AnimTree/slot behaviour on the
	SkeletalMeshActor base. Bodies are ported 1:1 from ASkeletalMeshActorMAT (UnSkeletalMesh.cpp);
	VerifyAnimationMatchSkeletalMesh is file-static in Engine, so its short body is replicated here.
=============================================================================*/

#include "BmGame.h"

IMPLEMENT_CLASS(ARSkeletalMeshActor);

// Replicated from VerifyAnimationMatchSkeletalMesh (UnSkeletalMesh.cpp) - not exposed in a header.
static UBOOL RVerifyAnimSet(UAnimSet* AnimSet, USkeletalMesh* SkeletalMesh)
{
	if( AnimSet && SkeletalMesh )
	{
		const INT LinkupIndex = AnimSet->GetMeshLinkupIndex(SkeletalMesh);
		if( LinkupIndex == INDEX_NONE || LinkupIndex >= AnimSet->LinkupCache.Num() || SkeletalMesh->SkelMeshRUID == 0 )
		{
			return FALSE;
		}

		const FAnimSetMeshLinkup& Linkup = AnimSet->LinkupCache(LinkupIndex);
		if( Linkup.BoneToTrackTable.Num() != SkeletalMesh->RefSkeleton.Num() )
		{
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

static UBOOL RVerifyComp(USkeletalMeshComponent* Comp)
{
	if( Comp )
	{
		for( INT I=0; I<Comp->AnimSets.Num(); I++ )
		{
			if( RVerifyAnimSet(Comp->AnimSets(I), Comp->SkeletalMesh) == FALSE )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

/** Rebuild the SlotNodes cache from the current AnimTree. */
void ARSkeletalMeshActor::CacheSlotNodes()
{
	SlotNodes.Empty();

	if( SkeletalMeshComponent && SkeletalMeshComponent->Animations )
	{
		TArray<UAnimNode*> Nodes;
		SkeletalMeshComponent->Animations->GetNodesByClass(Nodes, UAnimNodeSlot::StaticClass());

		for(INT i=0; i<Nodes.Num(); i++)
		{
			UAnimNodeSlot* SlotNode = Cast<UAnimNodeSlot>(Nodes(i));
			if( SlotNode )
			{
				SlotNodes.AddItem(SlotNode);
			}
		}
	}
}

// BM: root-motion teleport bookkeeping not yet ported
void ARSkeletalMeshActor::Teleport()
{
}

// BM: morph weight forwarding not yet ported
void ARSkeletalMeshActor::InternalSetMorphWeight(FName MorphNodeName, FLOAT MorphWeight)
{
}

/** Instance the AnimTree (if needed) and rebuild the SlotNodes cache. In-game setup path. */
void ARSkeletalMeshActor::InternalInitAnimTree()
{
	if( SkeletalMeshComponent )
	{
		SkeletalMeshComponent->InitAnimTree();
		CacheSlotNodes();
	}
}

/** Update an AnimTree slot from Matinee track info. Mirrors ASkeletalMeshActorMAT::MAT_SetAnimPosition. */
void ARSkeletalMeshActor::MAT_SetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bFireNotifies, UBOOL bLooping, UBOOL bEnableRootMotion)
{
	// Ensure anims are updated correctly in cinematics even when the mesh isn't rendered.
	SkeletalMeshComponent->LastRenderTime = GWorld->GetTimeSeconds();

	// Forward animation positions to slots. They will forward to relevant channels.
	for(INT i=0; i<SlotNodes.Num(); i++)
	{
		UAnimNodeSlot* SlotNode = SlotNodes(i);
		if( SlotNode && SlotNode->NodeName == SlotName )
		{
			// Verify if skeletalmesh can work with given animation
			if ( RVerifyComp(SkeletalMeshComponent) == FALSE )
			{
				return;
			}

			SlotNode->MAT_SetAnimPosition(ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
		}
	}
}

/** Forward channel weights to the relevant slot(s). Mirrors ASkeletalMeshActorMAT::MAT_SetAnimWeights. */
void ARSkeletalMeshActor::MAT_SetAnimWeights(const TArray<FAnimSlotInfo>& SlotInfos)
{
	for(INT SlotInfoIdx=0; SlotInfoIdx<SlotInfos.Num(); SlotInfoIdx++)
	{
		const FAnimSlotInfo& SlotInfo = SlotInfos(SlotInfoIdx);

		for(INT SlotIdx=0; SlotIdx<SlotNodes.Num(); SlotIdx++)
		{
			UAnimNodeSlot* SlotNode = SlotNodes(SlotIdx);
			if( SlotNode && SlotNode->NodeName == SlotInfo.SlotName )
			{
				SlotNode->MAT_SetAnimWeights(SlotInfo);
				SlotNode->bIsBeingUsedByInterpGroup = TRUE;
			}
			else if( SlotNode )
			{
				SlotNode->bIsBeingUsedByInterpGroup = FALSE;
			}
		}
	}
}

/** PreviewBeginAnimControl - instance the AnimTree for preview and cache its slots. */
void ARSkeletalMeshActor::PreviewBeginAnimControl(UInterpGroup* InInterpGroup)
{
	// We need an AnimTree in Matinee in the editor to preview the animations, so instance one now if
	// we don't have one. Safe to call multiple times - only instances the first time.
	if( !SkeletalMeshComponent->Animations && SkeletalMeshComponent->AnimTreeTemplate )
	{
		SkeletalMeshComponent->Animations = SkeletalMeshComponent->AnimTreeTemplate->CopyAnimTree(SkeletalMeshComponent);
	}

	// In the editor we don't have access to Script, so cache slot nodes here.
	CacheSlotNodes();

	// Base: register the InterpGroup, build the AnimSet list, init the tree.
	Super::PreviewBeginAnimControl(InInterpGroup);
}

/** PreviewSetAnimPosition - drive the slot then update the pose. Mirrors ASkeletalMeshActorMAT. */
void ARSkeletalMeshActor::PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime)
{
	MAT_SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);

	// Update space bases so the new animation position has an effect.
	SkeletalMeshComponent->UpdateSkelPose(DeltaTime, FALSE);
	SkeletalMeshComponent->ConditionalUpdateTransform();
}

/** PreviewSetAnimWeights */
void ARSkeletalMeshActor::PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos)
{
	MAT_SetAnimWeights(SlotInfos);
}

/** SetAnimWeights (in-game) */
void ARSkeletalMeshActor::SetAnimWeights( const TArray<FAnimSlotInfo>& SlotInfos )
{
	MAT_SetAnimWeights(SlotInfos);
}

/** PreviewFinishAnimControl - drop the preview AnimTree and reset to ref pose. Mirrors ASkeletalMeshActorMAT. */
void ARSkeletalMeshActor::PreviewFinishAnimControl(UInterpGroup* InInterpGroup)
{
	MAT_FinishAnimControl(InInterpGroup);

	// Take out our group from the list.
	InterpGroupList.RemoveItem(InInterpGroup);

	// Update AnimSet list.
	UpdateAnimSetList();

	// When done in Matinee in the editor, drop the AnimTree instance.
	SkeletalMeshComponent->Animations = NULL;

	// Clear the weight on all slots before freeing them.
	FAnimSlotInfo SlotNodeInfo;
	SlotNodeInfo.ChannelWeights.AddItem(0.0f);

	for(INT SlotIdx=0; SlotIdx<SlotNodes.Num(); SlotIdx++)
	{
		UAnimNodeSlot* SlotNode = SlotNodes(SlotIdx);
		if( SlotNode )
		{
			SlotNode->MAT_SetAnimWeights(SlotNodeInfo);
			SlotNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);
			SlotNode->bIsBeingUsedByInterpGroup = FALSE;
		}
	}

	// In the editor, free up the slot nodes.
	SlotNodes.Empty();

	// Update space bases to reset back to ref pose.
	SkeletalMeshComponent->UpdateSkelPose(0.f, FALSE);
	SkeletalMeshComponent->ConditionalUpdateTransform();
}
