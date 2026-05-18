/**
 * One animation sequence of keyframes. Contains a number of tracks of data.
 * The Outer of AnimSequence is expected to be its AnimSet.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

class AnimSequence extends Object
	native(Anim)
	config(Engine)
	dependson(RAnimZip_Settings)
	hidecategories(Object)
	autocollapsecategories(Info);

enum AnimationCompressionFormat
{
	ACF_None,
	ACF_Float96NoW,
	ACF_Fixed48NoW,
	ACF_IntervalFixed32NoW,
	ACF_Fixed32NoW,
	ACF_Float32NoW,
	ACF_Identity,
};

enum EForwardYawDirection
{
	FYD_Clockwise,
	FYD_AntiClockwise,
};

enum EAnimPhysics
{
	APHYS_Walking,
	APHYS_Flying,
	APHYS_Floating,
	APHYS_Falling,
	APHYS_Ceiling,
};

enum ERootMotionRotationOption
{
	RMRO_On,
	RMRO_NoExtraction,
	RMRO_Off,
};

enum ERootMotionTranslationOption
{
	RMTO_On,
	RMTO_NoExtraction,
	RMTO_Off,
};

enum AnimationKeyFormat
{
	AKF_ConstantKeyLerp,
	AKF_VariableKeyLerp,
	AKF_PerTrackCompression,
};

struct native AnimNotifyEvent
{
	var()	float							Time;
	var()	editoronly export AnimNotify	Notify;
};

/**
 * Raw keyframe data for one track.  Each array will contain either NumFrames elements or 1 element.
 * One element is used as a simple compression scheme where if all keys are the same, they'll be
 * reduced to 1 key that is constant over the entire sequence.
 */
struct RawAnimSequenceTrack
{
	var array<vector>	PosKeys;
	var array<quat>		RotKeys;
	var array<float>	ScaleKeys;
};

struct native TimeModifier
{
	var()   float           Time;
	var()   float           TargetStrength;
};

struct native SkelControlModifier
{
	var()   name            SkelControlName;
	var()   editinline array<TimeModifier>  Modifiers;
};

struct native TranslationTrack
{
	var array<vector>	PosKeys;
	var array<float>	Times;
};

struct native RotationTrack
{
	var array<quat>		RotKeys;
	var array<float>	Times;
};

struct native CurveTrack
{
	var name			CurveName;
	var array<float>	CurveWeights;

	structcpptext
	{
	UBOOL IsValidCurveTrack();
	UBOOL CompressCurveWeights();
	}
};

struct native CompressedTrack
{
	var array<byte>		ByteStream;
	var array<float>	Times;
	var float			Mins[3];
	var float			Ranges[3];
};

struct native AnimReferenceOptions
{
	var() bool AutomaticFloorHeight;
	var() EForwardYawDirection ForwardYawDirection;
	var() float ForwardYaw;
	var() float FloorHeight;
};

struct native AnimReferencePeriods
{
	var() AnimReferenceOptions Start;
	var() AnimReferenceOptions End;
	var() bool EnforceMinimumFloorHeight;
	var() float MinimumFloorHeight;
};

struct native AnimCollisionOptions
{
	var() bool BlockActors;
	var() bool CollideWorld;
	var() bool DisableLegIK;
	var() bool AllowIKWhenNotPHYSWalking;
	var() bool PreviousVelocityOverridesAnimRootMotion;
	var() EAnimPhysics Physics;
	var() ERootMotionRotationOption RootMotionRotationOption;
	var() ERootMotionTranslationOption RootMotionTranslationOption;

	structdefaultproperties
	{
		BlockActors=true
		CollideWorld=true
		Physics=APHYS_Walking
		RootMotionRotationOption=RMRO_On
		RootMotionTranslationOption=RMTO_On
	}
};

struct native AnimCollisionPeriods
{
	var() AnimCollisionOptions Middle;
	var() AnimCollisionOptions End;
};

struct native AnimTag
{
	var string			Tag;
	var array<string>	Contains;
};

var		name									SequenceName;
var()	editoronly array<editoronly AnimNotifyEvent>		Notifies;
var(Audio) editoronly bool						AudioComplete;
var()	bool									bUseSimpleForwardYaw;
var()	bool									bUseSimpleFloorHeight;
var()	bool									bUseSimpleRootMotionXY;
var()	bool									bInheritRootMotionFromVelocity;
var()	bool									DisableProportionalMotionDuringBlendOut;
var()	bool									AllowCheekyBlendIn;
var()	bool									AllowCheekyBlendOut;
var(FaceFX) bool								EmbeddedFaceFXAnim_AllowAutomaticBlinks;
var(Info) editconst bool						WeaponSwitchPointEnabled;
var(Compression) bool							Compression_UseLinearInterpolation;
var(Compression) bool							Compression_RelativeToReferencePose;
var(Compression) editconst bool					Compression_UsingTemporaryCompression;
var() editoronly const bool						bDoNotOverrideCompression;
var const transient bool						bHasBeenUsed;
var const transient bool						MetricWasRecorded;
var(Info) editconst float						SequenceLength;
var(Info) editconst int							NumFrames;
var()	float									RateScale;
var	deprecated private const array<RawAnimSequenceTrack>	RawAnimData;
var native private const array<RawAnimSequenceTrack>		RawAnimationData;
var const array<CurveTrack>						CurveData;
var(Info) editoronly editconst AnimationCompressionAlgorithm	CompressionScheme;
var const AnimationCompressionFormat			TranslationCompressionFormat;
var const AnimationCompressionFormat			RotationCompressionFormat;
var(Compression) RAnimZip_Settings.EAnimZipPreset	Compression_Preset;
var const AnimationKeyFormat					KeyEncodingFormat;
var			array<int>							CompressedTrackOffsets;
var native	array<byte>							CompressedByteStream;
var(Info) editoronly string						MaxFilePath;
var(Info) editoronly string						MaxAuthor;
var()	vector									ReferencePoint;
var()	float									ReferencePointYaw;
var()	AnimReferencePeriods					ReferenceOptions;
var()	float									ProportionalMotionDistanceCap;
var()	AnimCollisionPeriods					CollisionOptions;
var()	float									BlendInDuration;
var()	float									BlendOutDuration;
var(FaceFX) editconst FaceFXAnimSet				EmbeddedFaceFXAnim;
var(Info) editconst float						BlendInPoint;
var(Info) editconst float						BlendOutPoint;
var(Info) editconst float						ClippedStart;
var(Info) editconst float						ClippedLength;
var(Info) editconst float						CanCancelBeforeHerePoint;
var(Info) editconst float						CanCancelAfterHerePoint;
var(Info) editconst float						CanCorrectAfterHerePoint;
var(Info) editconst float						ClipRootMotionInPoint;
var(Info) editconst float						ClipRootMotionOutPoint;
var(Info) editconst float						CollisionOptionsOutPoint;
var(Info) editconst float						WeaponSwitchPoint;
var(Compression) editoronly export RAnimZip_Settings	Compression_CustomSettings;
var native	array<byte>							AnimZip_Data;
var(Info) editconst vector						AnimZip_LinearOrigin;
var(Info) editconst vector						AnimZip_LinearSpan;
var private transient native pointer			TranslationCodec;
var private transient native pointer			RotationCodec;
var const int									EncodingPkgVersion;
var editoronly const int						CompressCommandletVersion;
var const transient float						UseScore;
var config editoronly array<AnimTag>			AnimTags;

cpptext
{
	// UObject interface

	virtual void Serialize(FArchive& Ar);
	virtual void PreSave();
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void BeginDestroy();

	/**
	 * Used by various commandlets to purge editor only and platform-specific data from various objects
	 *
	 * @param PlatformsToKeep Platforms for which to keep platform-specific data
	 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
	 */
	virtual void StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData);

	// AnimSequence interface

	/**
	 * Reconstructs a bone atom from key-reduced tracks.
	 */
	static void ReconstructBoneAtom(FBoneAtom& OutAtom,
									const FTranslationTrack& TranslationTrack,
									const FRotationTrack& RotationTrack,
									FLOAT SequenceLength,
									FLOAT Time,
									UBOOL bLooping);

	/**
	 * Reconstructs a bone atom from compressed tracks.
	 */
	static void ReconstructBoneAtom(FBoneAtom& OutAtom,
									const FCompressedTrack& TranslationTrack,
									const FCompressedTrack& RotationTrack,
									AnimationCompressionFormat TranslationCompressionFormat,
									AnimationCompressionFormat RotationCompressionFormat,
									FLOAT SequenceLength,
									FLOAT Time,
									UBOOL bLooping);

	/**
	 * Reconstructs a bone atom from compressed tracks.
	 */
	static void ReconstructBoneAtom(FBoneAtom& OutAtom,
									const BYTE* TransStream,
									INT NumTransKeys,
									const BYTE* RotStream,
									INT NumRotKeys,
									AnimationCompressionFormat TranslationCompressionFormat,
									AnimationCompressionFormat RotationCompressionFormat,
									FLOAT SequenceLength,
									FLOAT Time,
									UBOOL bLooping);

	/**
	 * Decompresses a translation key from the specified compressed translation track.
	 */
	static void ReconstructTranslation(class FVector& Out, const BYTE* Stream, INT KeyIndex, AnimationCompressionFormat TranslationCompressionFormat);

	/**
	 * Decompresses a rotation key from the specified compressed rotation track.
	 */
	static void ReconstructRotation(class FQuat& Out, const BYTE* Stream, INT KeyIndex, AnimationCompressionFormat RotationCompressionFormat, const FLOAT *Mins, const FLOAT *Ranges);

	/**
	 * Decompresses a translation key from the specified compressed translation track.
	 */
	static void ReconstructTranslation(class FVector& Out, const BYTE* Stream, INT KeyIndex);

	/**
	 * Decompresses a rotation key from the specified compressed rotation track.
	 */
	static void ReconstructRotation(class FQuat& Out, const BYTE* Stream, INT KeyIndex, UBOOL bTrackHasCompressionInfo, AnimationCompressionFormat RotationCompressionFormat);

	/**
	 * Populates the key reduced arrays from raw animation data.
	 */
	static void SeparateRawDataToTracks(const TArray<FRawAnimSequenceTrack>& RawAnimData,
										FLOAT SequenceLength,
										TArray<FTranslationTrack>& OutTranslationData,
										TArray<FRotationTrack>& OutRotationData);

	/**
	 * Interpolate keyframes in this sequence to find the bone transform (relative to parent).
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	Time			Time on track to interpolate to.
	 * @param	bLooping		TRUE if the animation is looping.
	 * @param	bUseRawData		If TRUE, use raw animation data instead of compressed data.
	 * @param	CurveKeys		List of Curve Keys if exists
	 */
	void GetBoneAtom(FBoneAtom& OutAtom, INT TrackIndex, FLOAT Time, UBOOL bLooping, UBOOL bUseRawData, FCurveKeyArray* CurveKeys = NULL) const;

	/**
	 * Interpolate curve weights of the Time in this sequence if curve data exists
	 *
	 * @param	Time			Time on track to interpolate to.
	 * @param	bLooping		TRUE if the animation is looping.
	 * @param	CurveKeys		Add the curve keys if exists
	 */
	void GetCurveData(FLOAT Time, UBOOL bLooping, FCurveKeyArray& CurveKeys) const;

	/** Sort the Notifies array by time, earliest first. */
	void SortNotifies();

	/**
	 * @return		A reference to the AnimSet this sequence belongs to.
	 */
	UAnimSet* GetAnimSet() const;

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	/**
	 * @return		The approximate size of raw animation data.
	 */
	INT GetApproxRawSize() const;

	/**
	 * @return		The approximate size of key-reduced animation data.
	 */
	INT GetApproxReducedSize() const;

	/**
	 * @return		The approximate size of compressed animation data.
	 */
	INT GetApproxCompressedSize() const;

	/**
	 * Crops the raw anim data either from Start to CurrentTime or CurrentTime to End depending on
	 * value of bFromStart.  Can't be called against cooked data.
	 *
	 * @param	CurrentTime		marker for cropping (either beginning or end)
	 * @param	bFromStart		whether marker is begin or end marker
	 * @return					TRUE if the operation was successful.
	 */
	UBOOL CropRawAnimData( FLOAT CurrentTime, UBOOL bFromStart );
	/**
	 *  Utility function for lossless compression of a FRawAnimSequenceTrack
	 *  @return TRUE if keys were removed.
	 **/
	UBOOL CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, float MaxPosDiff, float MaxAngleDiff);
	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return TRUE if keys were removed.
	 */
	UBOOL CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff);
	/**
	 * Removes trivial frames -- frames of tracks when position or orientation is constant
	 * over the entire animation -- from the raw animation data.  If both position and rotation
	 * go down to a single frame, the time is stripped out as well.
	 * @return TRUE if keys were removed.
	 */
	UBOOL CompressRawAnimData();

	/** Clears any data in the AnimSequence, so it can be recycled when importing a new animation with same name over it. */
	void RecycleAnimSequence();

	static UBOOL CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, UBOOL bSkipCopyingNotifies=FALSE);
	static UBOOL CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq);
}

/**
 *	Get the time (in seconds) from the start of the animation that the first notify of the given class would fire
 *
 *	@param	NotifyClass		Class of AnimNotify we are looking for (ie AnimNotify_Sound)
 *	@param	PlayRate		Rate that animation would be played at
 *	@param	StartPosition	Initial position in the animation to start checking from
 *	@return					Time in seconds that notify would fire if anim was played at given rate
 *							Returns -1.f if no notify is found
 */
native function float GetNotifyTimeByClass( class<AnimNotify> NotifyClass, optional float PlayRate = 1.f, optional float StartPosition = -1.f, optional out AnimNotify out_Notify, optional out float out_Duration );

defaultproperties
{
	RateScale=1.0
	AllowCheekyBlendIn=true
	AllowCheekyBlendOut=true
}
