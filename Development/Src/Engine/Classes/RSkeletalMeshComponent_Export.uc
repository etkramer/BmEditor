// BM: Shared SkeletalMesh-related types
class RSkeletalMeshComponent_Export extends Object;

enum EFaceFXRegisterOwner
{
	FXREGISTEROWNER_Code,
	FXREGISTEROWNER_CodeBlink,
	FXREGISTEROWNER_CodeLookAt,
	FXREGISTEROWNER_CodeCheat,
	FXREGISTEROWNER_MatineeRegisterTrack,
	FXREGISTEROWNER_MatineeLookAtTrack,
	FXREGISTEROWNER_Kismet,
	FXREGISTEROWNER_Tweak,
};

enum EFaceFXBaseExpression
{
	FXBE_Default,
	FXBE_None,
	FXBE_Angry,
	FXBE_Frown,
	FXBE_Injured,
	FXBE_Intense,
	FXBE_KO,
	FXBE_Smile,
	FXBE_Smirk,
	FXBE_Surprised,
	FXBE_Scowl,
	FXBE_Tired,
	FXBE_BM_Angry_01,
	FXBE_BM_Angry_02,
};

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

enum EParentAnimComponentMode
{
	PACM_Original,
	PACM_Add,
	PACM_Replace,
	PACM_CapeReplace,
	PACM_CapeReplaceAtomsTranslationOnly,
};

enum EStretchPhase
{
	STRETCHPHASE_PostAnimBlend,
	STRETCHPHASE_PreRender,
};

struct native TwistBoneFixer
{
	var int BaseBoneIndex;
	var int DriverBoneIndex;
	var int TwistBoneIndex;
	var int SubTwistBone1Index;
	var int SubTwistBone2Index;
	var int AwkwardBoneIndex;
};

struct native TwistBoneFixers
{
	var notforconsole array<TwistBoneFixer> Fixers;
};

struct native ClavicleFixer
{
	var int NeckBoneIndex;
	var int LeftClavicleBoneIndex;
	var int RightClavicleBoneIndex;
	var bool Enabled;
};

struct native BreathingFixer
{
	var bool Enabled;
	var bool BonesPresent;
	var int Spine1Index;
	var int Spine2Index;
	var int Spine3Index;
	var float Amount;
};

struct native FaceFXRegisterTransition
{
	var int Index;
	var float FromValue;
	var float ToValue;
	var float OneOverDuration;
	var float NormalizedTime;
	var RSkeletalMeshComponent_Export.EFaceFXRegisterOwner Owner;
};

struct native FaceFXRegisterState
{
	var int Index;
	var float Value;
	var RSkeletalMeshComponent_Export.EFaceFXRegisterOwner Owner;
};

struct native FaceFXEmbeddedAnimSample
{
	var FaceFXAnimSet FaceFXAnimSet;
	var float Time;
	var float Weight;
	var bool AllowAutomaticBlinks;
	var bool Mirror;
};

struct native StretchDescription
{
	var() name				Bone;
	var() vector			Translation;
	var() float				Scale;
	var EStretchPhase		Phase;
};

struct native StretchInstance
{
	var Vector4 TranslationAndScale;
	var int BoneIndex;
};

struct native StretchPhaseInstances
{
	var array<StretchInstance> Instances;
};

struct native StretchInstances
{
	var StretchPhaseInstances Phases[EStretchPhase];
};

enum SkeletalMeshOptimizationImportance
{
	SMOI_Normal,
	SMOI_High,
	SMOI_Highest,
};

enum SkeletalMeshOptimizationNormalMode
{
	SMONM_PreserveSmoothingGroups,
	SMONM_RecalculateNormals,
	SMONM_RecalculateNormalsSoft,
	SMONM_RecalculateNormalsHard,
};

struct native CharacterMirrorBoneAction
{
	var byte ShuffleRotation;
	var byte FlipRotation;
	var byte FlipTranslation;
	var byte Padding;
};

struct native CharacterMirrorBone
{
	var int SourceBoneIndex;
	var CharacterMirrorBoneAction Action;
};

struct native BoneMass
{
	var int BoneIndex;
	var float Mass;
};

struct native RotationBoneMass
{
	var quat RotationOffset;
	var int BoneIndex;
	var float Mass;
};

struct native NormalizedBoneMasses
{
	var array<BoneMass> Translation;
	var array<RotationBoneMass> Rotation;
};
