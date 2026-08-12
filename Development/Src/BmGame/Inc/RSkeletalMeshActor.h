/*=============================================================================
	RSkeletalMeshActor.h
	BM: Native implementation for BM2's RSkeletalMeshActor.
=============================================================================*/

#ifndef RSKELETALMESHACTOR_H
#define RSKELETALMESHACTOR_H

// Declares no data members - see UnClassExtension.h.
class ARSkeletalMeshActor : public ASkeletalMeshActor
{
public:
	// DECLARE_CLASS would normally provide this; we have no UClass of our own.
	typedef ASkeletalMeshActor Super;

	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime);
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos);
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewUpdateFaceFX(UBOOL bForceAnim, const FString& GroupName, const FString& SeqName, FLOAT InPosition);
	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos );

	void CacheSlotNodes();
	void InternalInitAnimTree();
	void MAT_SetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bFireNotifies, UBOOL bLooping, UBOOL bEnableRootMotion);
	void MAT_SetAnimWeights(const TArray<struct FAnimSlotInfo>& SlotInfos);

	/** Resolve the script properties we touch. FALSE if the class isn't what we expect. */
	UBOOL BindProperties();

	UAnimNodeSequence*& SequenceNode()	{ return SequenceNodeProp(this); }
	TArray<UAnimNodeSlot*>& SlotNodes()	{ return SlotNodesProp(this); }

	DECLARE_FUNCTION(execInternalInitAnimTree)
	{
		P_FINISH;
		InternalInitAnimTree();
	}

	DECLARE_FUNCTION(execTeleport)
	{
		P_FINISH;
		// BM: root-motion teleport bookkeeping not yet ported
	}

	DECLARE_FUNCTION(execInternalSetMorphWeight)
	{
		P_GET_NAME(MorphNodeName);
		P_GET_FLOAT(MorphWeight);
		P_FINISH;
		// BM: morph weight forwarding not yet ported
	}

	DECLARE_FUNCTION(execMAT_BeginAnimControl)
	{
		P_GET_OBJECT(UInterpGroup, InInterpGroup);
		P_FINISH;
		MAT_BeginAnimControl(InInterpGroup);
	}

	DECLARE_FUNCTION(execMAT_FinishAnimControl)
	{
		P_GET_OBJECT(UInterpGroup, InInterpGroup);
		P_FINISH;
		MAT_FinishAnimControl(InInterpGroup);
	}

	DECLARE_FUNCTION(execMAT_SetAnimPosition)
	{
		P_GET_NAME(SlotName);
		P_GET_INT(ChannelIndex);
		P_GET_NAME(InAnimSeqName);
		P_GET_FLOAT(InPosition);
		P_GET_UBOOL(bFireNotifies);
		P_GET_UBOOL(bLooping);
		P_GET_UBOOL(bEnableRootMotion);
		P_FINISH;
		MAT_SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
	}

private:
	static TExtensionProperty<UAnimNodeSequence*> SequenceNodeProp;
	static TExtensionProperty<TArray<UAnimNodeSlot*> > SlotNodesProp;
};

#endif // RSKELETALMESHACTOR_H
