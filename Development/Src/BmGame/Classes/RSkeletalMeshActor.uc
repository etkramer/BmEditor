// BM
class RSkeletalMeshActor extends SkeletalMeshActor
	native;

cpptext
{
	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup);
	virtual void PreviewSetAnimPosition(FName SlotName, INT ChannelIndex, FName InAnimSeqName, FLOAT InPosition, UBOOL bLooping, UBOOL bFireNotifies, UBOOL bEnableRootMotion, FLOAT DeltaTime);
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos);
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup);

	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos );

	void CacheSlotNodes();
	void MAT_SetAnimWeights(const TArray<struct FAnimSlotInfo>& SlotInfos);
}

var transient AnimNodeSequence SequenceNode;
var transient array<AnimNodeSlot> SlotNodes;
var transient int AnimControlReferenceCount;
var transient int MatineeControlReferenceCount;
var transient InterpTrackMove LastMove_MoveTrack;
var transient float LastMove_CurTime;

native final function InternalInitAnimTree();
native function Teleport();
native function MAT_SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion);
native final function InternalSetMorphWeight(name MorphNodeName, float MorphWeight);

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();
	InternalInitAnimTree();
}

simulated event BeginAnimControl(InterpGroup InInterpGroup)
{
	AnimControlReferenceCount++;
	MAT_BeginAnimControl(InInterpGroup);
}

simulated event SetAnimPosition(name SlotName, int ChannelIndex, name InAnimSeqName, float InPosition, bool bFireNotifies, bool bLooping, bool bEnableRootMotion)
{
	if(AnimControlReferenceCount == 0)
	{
		return;
	}
	MAT_SetAnimPosition(SlotName, ChannelIndex, InAnimSeqName, InPosition, bFireNotifies, bLooping, bEnableRootMotion);
}

simulated event FinishAnimControl(InterpGroup InInterpGroup)
{
	if(AnimControlReferenceCount == 0)
	{
		return;
	}
	MAT_FinishAnimControl(InInterpGroup);
	AnimControlReferenceCount--;
}

simulated event SetMorphWeight(name MorphNodeName, float MorphWeight)
{
	InternalSetMorphWeight(MorphNodeName, MorphWeight);
}

defaultproperties
{
	Begin Object Name=SkeletalMeshComponent0
		Animations=None
	End Object

	bStasis=true
}
