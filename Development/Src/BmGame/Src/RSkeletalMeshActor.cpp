/*=============================================================================
	RSkeletalMeshActor.cpp
	BM: Native implementation for BM2's RSkeletalMeshActor.

	Gives BM2's cinematic actors the SkeletalMeshActorMAT-style AnimTree/slot behaviour on the
	SkeletalMeshActor base. Bodies are ported 1:1 from ASkeletalMeshActorMAT (UnSkeletalMesh.cpp);
	VerifyAnimationMatchSkeletalMesh is file-static in Engine, so its short body is replicated here.
=============================================================================*/

#include "BmGame.h"

IMPLEMENT_CLASS_EXTENSION(ARSkeletalMeshActor, "BmGame.RSkeletalMeshActor");

TExtensionProperty<UAnimNodeSequence*> ARSkeletalMeshActor::SequenceNodeProp;
TExtensionProperty<TArray<UAnimNodeSlot*> > ARSkeletalMeshActor::SlotNodesProp;

UBOOL ARSkeletalMeshActor::BindProperties()
{
	SequenceNodeProp.Bind(GetClass(), TEXT("SequenceNode"));
	SlotNodesProp.Bind(GetClass(), TEXT("SlotNodes"));

	return SequenceNodeProp.IsBound() && SlotNodesProp.IsBound();
}

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

// Resolve (or create) the single AnimNodeSequence used by cinematic actors that drive one
// animation directly rather than through an AnimTree/slots.
static UAnimNodeSequence* GetOrCreateSequenceNode(USkeletalMeshComponent* SkelComp)
{
	if( !SkelComp->Animations && SkelComp->AnimTreeTemplate )
	{
		SkelComp->SetAnimTreeTemplate(SkelComp->AnimTreeTemplate);
	}

	if( SkelComp->Animations )
	{
		UAnimTree* Tree = Cast<UAnimTree>(SkelComp->Animations);
		if( !Tree || Tree->Children.Num() <= 0 )
		{
			return Cast<UAnimNodeSequence>(SkelComp->Animations);
		}

		UAnimNodeSequence* SeqNode = Cast<UAnimNodeSequence>(Tree->Children(0).Anim);
		if( !SeqNode )
		{
			return Cast<UAnimNodeSequence>(SkelComp->Animations);
		}

		return SeqNode;
	}

	UAnimNodeSequence* NewSeqNode = ConstructObject<UAnimNodeSequence>(UAnimNodeSequence::StaticClass(), UObject::GetTransientPackage());
	SkelComp->Animations = NewSeqNode;
	SkelComp->InitAnimTree(TRUE);
	return NewSeqNode;
}

/** Rebuild the SlotNodes cache from the current AnimTree. */
void ARSkeletalMeshActor::CacheSlotNodes()
{
	if( !BindProperties() )
	{
		return;
	}

	SlotNodes().Empty();

	if( SkeletalMeshComponent && SkeletalMeshComponent->Animations )
	{
		TArray<UAnimNode*> Nodes;
		SkeletalMeshComponent->Animations->GetNodesByClass(Nodes, UAnimNodeSlot::StaticClass());

		for(INT i=0; i<Nodes.Num(); i++)
		{
			UAnimNodeSlot* SlotNode = Cast<UAnimNodeSlot>(Nodes(i));
			if( SlotNode )
			{
				SlotNodes().AddItem(SlotNode);
			}
		}
	}
}

/** Instance the AnimTree (if needed) and rebuild the SlotNodes cache. In-game setup path. */
void ARSkeletalMeshActor::InternalInitAnimTree()
{
	if( !BindProperties() )
	{
		return;
	}

	if( SkeletalMeshComponent )
	{
		SkeletalMeshComponent->InitAnimTree();
		SequenceNode() = GetOrCreateSequenceNode(SkeletalMeshComponent);
		CacheSlotNodes();
	}
}

/** Update an AnimTree slot from Matinee track info. Mirrors ASkeletalMeshActorMAT::MAT_SetAnimPosition. */
void ARSkeletalMeshActor::MAT_SetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bFireNotifies, UBOOL bLooping, UBOOL bEnableRootMotion)
{
	if( !BindProperties() )
	{
		return;
	}

	UAnimNodeSequence* SeqNode = SequenceNode();

	// Drive a single AnimNodeSequence directly, for actors without an AnimTree/slots.
	if( SeqNode )
	{
		if( SeqNode->AnimSeqName != InAnimSeqName || SeqNode->AnimSeq == NULL )
		{
			SeqNode->SetAnim(InAnimSeqName);
			SeqNode->SetPosition(InPosition, FALSE);
		}

		if( bEnableRootMotion )
		{
			SkeletalMeshComponent->RootMotionMode = RMM_Translate;
			SeqNode->SetRootBoneAxisOption(RBA_Translate, RBA_Translate, RBA_Translate);
			SkeletalMeshComponent->RootMotionRotationMode = RMRM_RotateActor;
			SeqNode->SetRootBoneRotationOption(RRO_Extract, RRO_Extract, RRO_Extract);
		}
		else
		{
			SkeletalMeshComponent->RootMotionMode = RMM_Ignore;
			SeqNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);
			SkeletalMeshComponent->RootMotionRotationMode = RMRM_Ignore;
			SeqNode->SetRootBoneRotationOption(RRO_Default, RRO_Default, RRO_Default);
		}

		SeqNode->bLooping = bLooping;
		SeqNode->PreviousTime = SeqNode->CurrentTime;
		SeqNode->SetPosition(InPosition, bFireNotifies);
	}

	// Ensure anims are updated correctly in cinematics even when the mesh isn't rendered.
	SkeletalMeshComponent->LastRenderTime = GWorld->GetTimeSeconds();

	// Forward animation positions to slots. They will forward to relevant channels.
	TArray<UAnimNodeSlot*>& CachedSlotNodes = SlotNodes();
	for(INT i=0; i<CachedSlotNodes.Num(); i++)
	{
		UAnimNodeSlot* SlotNode = CachedSlotNodes(i);
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
	if( !BindProperties() )
	{
		return;
	}

	TArray<UAnimNodeSlot*>& CachedSlotNodes = SlotNodes();

	for(INT SlotInfoIdx=0; SlotInfoIdx<SlotInfos.Num(); SlotInfoIdx++)
	{
		const FAnimSlotInfo& SlotInfo = SlotInfos(SlotInfoIdx);

		for(INT SlotIdx=0; SlotIdx<CachedSlotNodes.Num(); SlotIdx++)
		{
			UAnimNodeSlot* SlotNode = CachedSlotNodes(SlotIdx);
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
	if( BindProperties() )
	{
		// We need an AnimTree in Matinee in the editor to preview the animations, so instance one now if
		// we don't have one. Safe to call multiple times - only instances the first time.
		if( !SkeletalMeshComponent->Animations && SkeletalMeshComponent->AnimTreeTemplate )
		{
			SkeletalMeshComponent->Animations = SkeletalMeshComponent->AnimTreeTemplate->CopyAnimTree(SkeletalMeshComponent);
		}

		// Resolve the single-sequence handle (creates one if there's no AnimTree at all).
		SequenceNode() = GetOrCreateSequenceNode(SkeletalMeshComponent);

		// In the editor we don't have access to Script, so cache slot nodes here.
		CacheSlotNodes();
	}

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

	if( !BindProperties() )
	{
		return;
	}

	// When done in Matinee in the editor, drop the AnimTree instance.
	SkeletalMeshComponent->Animations = NULL;
	SequenceNode() = NULL;

	// Clear the weight on all slots before freeing them.
	FAnimSlotInfo SlotNodeInfo;
	SlotNodeInfo.ChannelWeights.AddItem(0.0f);

	TArray<UAnimNodeSlot*>& CachedSlotNodes = SlotNodes();
	for(INT SlotIdx=0; SlotIdx<CachedSlotNodes.Num(); SlotIdx++)
	{
		UAnimNodeSlot* SlotNode = CachedSlotNodes(SlotIdx);
		if( SlotNode )
		{
			SlotNode->MAT_SetAnimWeights(SlotNodeInfo);
			SlotNode->SetRootBoneAxisOption(RBA_Default, RBA_Default, RBA_Default);
			SlotNode->bIsBeingUsedByInterpGroup = FALSE;
		}
	}

	// In the editor, free up the slot nodes.
	CachedSlotNodes.Empty();

	// Update space bases to reset back to ref pose.
	SkeletalMeshComponent->UpdateSkelPose(0.f, FALSE);
	SkeletalMeshComponent->ConditionalUpdateTransform();
}

/*-----------------------------------------------------------------------------
	Natives.

	Retail declares six natives on RSkeletalMeshActor. MAT_BeginAnimControl and
	MAT_FinishAnimControl are natives on SkeletalMeshActor in stock UE3, but the
	redeclaration means they now resolve against RSkeletalMeshActor and must be
	provided here too or they'd bind to NULL.
-----------------------------------------------------------------------------*/

static FNativeFunctionLookup GRSkeletalMeshActorNatives[] =
{
	MAP_NATIVE(ARSkeletalMeshActor, execInternalInitAnimTree)
	MAP_NATIVE(ARSkeletalMeshActor, execTeleport)
	MAP_NATIVE(ARSkeletalMeshActor, execInternalSetMorphWeight)
	MAP_NATIVE(ARSkeletalMeshActor, execMAT_BeginAnimControl)
	MAP_NATIVE(ARSkeletalMeshActor, execMAT_FinishAnimControl)
	MAP_NATIVE(ARSkeletalMeshActor, execMAT_SetAnimPosition)
	{NULL, NULL}
};

void RegisterRSkeletalMeshActorNatives()
{
	RegisterExtensionNatives(TEXT("RSkeletalMeshActor"), GRSkeletalMeshActorNatives);
}
