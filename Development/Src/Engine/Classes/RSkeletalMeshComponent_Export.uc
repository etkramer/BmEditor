// BM: Shared SkeletalMesh-related types
class RSkeletalMeshComponent_Export extends Object;

enum ESkeletalMeshComponentBoundsType
{
	SMCBT_Automatic,
	SMCBT_Conservative,
	SMCBT_PerBone,
	SMCBT_PhysicsAsset,
	SMCBT_ReferencePose,
	SMCBT_ApproximatePerBone,
	SMCBT_Editor,
	SMCBT_Fixed,
};

enum EStretchPhase
{
	STRETCHPHASE_PostAnimBlend,
	STRETCHPHASE_PreRender,
};

struct native StretchDescription
{
	var() name				Bone;
	var() vector			Translation;
	var() float				Scale;
	var EStretchPhase		Phase;
};
